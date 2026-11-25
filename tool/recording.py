#!/usr/bin/env python3
"""Reads recordings made with UA_source --record (the format is described in
src/world/recorder.h; the state's fields in src/world/state.h).

  recording.py FILE                  what it holds: steps, the steps the level
                                     can go on from, bytes per part and field
  recording.py FILE OTHER            the first step where two recordings
                                     differ, and the fields that do
  recording.py FILE --at STEP        the view after that step, object by
                                     object; --internal adds the internal part
                                     (only at steps the level can go on from)
"""
import collections
import math
import struct
import sys
import zlib

ELEM_SIZE = {1: 1, 2: 1, 3: 1, 4: 2, 5: 2, 6: 4, 7: 4, 8: 4, 9: 8, 10: 4, 11: 4}
ELEM_FORMAT = {1: "B", 2: "b", 3: "B", 4: "h", 5: "H", 6: "i", 7: "I", 8: "f", 9: "d", 10: "I", 11: "i"}
T_F32, T_F64, T_UNIT = 8, 9, 10
KIND_NAMES = {1: "world", 2: "sector", 3: "bomb", 4: "vehicle", 5: "weapon", 6: "building"}
UNIT_TYPES = {1: "unit", 2: "tank", 3: "host station", 4: "missile", 6: "flyer", 7: "ufo", 8: "car", 9: "gun"}


def kind_name(kind):
    if kind >= 16:
        return UNIT_TYPES.get(kind - 16, "unit type %d" % (kind - 16))
    return KIND_NAMES.get(kind, "kind %d" % kind)


class Field:
    def __init__(self, name, etype, count, is_list):
        self.name, self.type, self.count, self.list = name, etype, count, is_list

    def size(self, data, at):
        """Its size in bytes at data[at:]."""
        if not self.list:
            return ELEM_SIZE[self.type] * self.count
        n, = struct.unpack_from("<H", data, at)
        return 2 + n * self.count * ELEM_SIZE[self.type]

    def show(self, raw):
        start = 2 if self.list else 0
        n = (len(raw) - start) // ELEM_SIZE[self.type]
        values = struct.unpack_from("<%d%s" % (n, ELEM_FORMAT[self.type]), raw, start)
        text = ["#%d" % v if self.type == T_UNIT else ("%.9g" % v if self.type in (T_F32, T_F64) else str(v))
                for v in values[:12]]
        return " ".join(text) + (" ..." if n > 12 else "")


def read_schema(data):
    schema, p = {}, 2
    kinds, = struct.unpack_from("<H", data, 0)
    for _ in range(kinds):
        kind, n = struct.unpack_from("<BH", data, p)
        p += 3
        fields = []
        for _ in range(n):
            etype, count, is_list, length = struct.unpack_from("<BHBB", data, p)
            p += 5
            fields.append(Field(data[p:p + length].decode(), etype, count, bool(is_list)))
            p += length
        schema[kind] = fields
    return schema


def offsets(fields, data):
    """Where each field starts, then the end; None when the bytes don't fit."""
    out, at = [], 0
    for f in fields:
        out.append(at)
        at += f.size(data, at)
        if at > len(data):
            return None
    out.append(at)
    return out if at == len(data) else None


def unpack_image(packed):
    raw = zlib.decompress(packed[4:])
    level, count = struct.unpack_from("<iI", raw, 0)
    p, objects = 8, {}
    for _ in range(count):
        kind, oid, size = struct.unpack_from("<BII", raw, p)
        p += 9
        objects[(kind, oid)] = raw[p:p + size]
        p += size
    return level, objects


def to_f32(x):
    try:
        return struct.unpack("<f", struct.pack("<f", x))[0]
    except OverflowError:
        return math.inf


def guess(field, last, before):
    """What a changed field's bytes are XORed with, as the game guesses it:
    a float moved on by as much as it last moved, else the last bytes.
    Computing a float's sum or difference in double and rounding it gives the
    float result exactly."""
    if before is None or field.type not in (T_F32, T_F64):
        return last
    fmt = "f" if field.type == T_F32 else "d"
    n = len(last) // ELEM_SIZE[field.type]
    v1s = struct.unpack("<%d%s" % (n, fmt), last)
    v0s = struct.unpack("<%d%s" % (n, fmt), before)
    out = []
    for v1, v0 in zip(v1s, v0s):
        g = v1
        if math.isfinite(v1) and math.isfinite(v0):
            moved = v1 - v0
            if fmt == "f":
                moved = to_f32(moved)
            nxt = v1 + moved
            if fmt == "f":
                nxt = to_f32(nxt)
            if math.isfinite(moved) and math.isfinite(nxt):
                g = nxt
        out.append(g)
    return struct.pack("<%d%s" % (n, fmt), *out)


def apply_changes(objects, schema, raw, p, tally=None, part=""):
    """Applies what changed (objects: key -> [data, before]); returns where it
    ended. tally, when given, counts the bytes of each field."""
    count, = struct.unpack_from("<I", raw, p)
    p += 4
    for _ in range(count):
        kind, oid, what = struct.unpack_from("<BIB", raw, p)
        p += 6
        key = (kind, oid)
        if what == 0:
            objects.pop(key, None)
            if tally is not None:
                tally[(part, "objects gone")] += 6
        elif what == 1:
            size, = struct.unpack_from("<I", raw, p)
            data = raw[p + 4:p + 4 + size]
            p += 4 + size
            objects[key] = [data, data]
            if tally is not None:
                tally[(part, "new " + kind_name(kind))] += size + 10
        else:
            n, = struct.unpack_from("<H", raw, p)
            p += 2
            fields = schema[kind]
            old, before = objects[key]
            old_at, before_at = offsets(fields, old), offsets(fields, before)
            out, field = bytearray(), 0
            for _ in range(n):
                index, = struct.unpack_from("<H", raw, p)
                p += 2
                f = fields[index]
                size = f.size(raw, p)
                out += old[old_at[field]:old_at[index]]
                if f.list:
                    out += raw[p:p + size]
                else:
                    last = old[old_at[index]:old_at[index + 1]]
                    prev = before[before_at[index]:before_at[index + 1]] if before_at else None
                    if prev is not None and len(prev) != len(last):
                        prev = None
                    g = guess(f, last, prev)
                    out += bytes(a ^ b for a, b in zip(g, raw[p:p + size]))
                if tally is not None:
                    tally[(part, "%s %s" % (kind_name(kind), f.name))] += size + 2
                p += size
                field = index + 1
            out += old[old_at[field]:]
            objects[key] = [bytes(out), old]
    return p


class Recording:
    def __init__(self, path):
        data = open(path, "rb").read()
        if data[:4] != b"FORM" or data[8:12] != b"RPLY":
            sys.exit("%s: not a recording" % path)
        self.path, self.blocks, self.packed = path, [], collections.Counter()
        p = 12
        while p + 8 <= len(data):
            tag = data[p:p + 4].decode("latin-1")
            size, = struct.unpack(">I", data[p + 4:p + 8])
            if size == 0xFFFFFFFF or p + 8 + size > len(data):
                print("%s is cut short after %d blocks" % (path, len(self.blocks)), file=sys.stderr)
                break
            body = data[p + 8:p + 8 + size]
            p += 8 + size + (size & 1)
            self.packed[tag] += size
            if tag == "INFO":
                version, self.level = struct.unpack("<HH", body[:4])
                if version != 2:
                    sys.exit("%s: a recording of format %d, not 2" % (path, version))
                texts = [t.decode("latin-1") for t in body[4:].split(b"\0")[:4]]
                self.started, self.platform, self.compiler, self.built = texts
            elif tag == "SCHV":
                self.view_schema = read_schema(body)
            elif tag == "SCHI":
                self.internal_schema = read_schema(body)
            elif tag == "STRV":
                self.start_view = body
            elif tag == "STRI":
                self.start_internal = body
            elif tag == "STEP":
                self.blocks.append(body)

    def steps(self):
        """Every step's bytes, in order."""
        for block in self.blocks:
            raw = zlib.decompress(block[4:])
            p = 0
            while p + 4 <= len(raw):
                size, = struct.unpack_from("<I", raw, p)
                yield raw[p + 4:p + 4 + size]
                p += 4 + size

    def read(self, upto=None, internal=False, tally=None, each=None):
        """The view and internal objects after upto steps (all when None).
        each(step, resumable, view, internal) is called after every step."""
        view = {k: [v, v] for k, v in unpack_image(self.start_view)[1].items()}
        inner = {k: [v, v] for k, v in unpack_image(self.start_internal)[1].items()}
        n = 0
        for raw in self.steps():
            if upto is not None and n >= upto:
                break
            n += 1
            resumable = raw[0] != 0
            p = apply_changes(view, self.view_schema, raw, 1, tally, "view")
            count, = struct.unpack_from("<H", raw, p)
            start = p
            p += 2
            for _ in range(count):
                length, = struct.unpack_from("<H", raw, p + 10)
                p += 12 + length
            if tally is not None:
                tally[("messages", "")] += p - start
            if resumable and (internal or tally is not None):
                apply_changes(inner, self.internal_schema, raw, p, tally, "internal")
            if each:
                each(n, resumable, view, inner)
        return n, view, inner


def field_values(schema, kind, data):
    fields = schema.get(kind, [])
    at = offsets(fields, data)
    if at is None:
        return {}
    return {f.name: (f, data[at[i]:at[i + 1]]) for i, f in enumerate(fields)}


def summary(rec):
    print("%s: level %d, from %s; recorded on %s with %s, built %s"
          % (rec.path, rec.level, rec.started, rec.platform, rec.compiler, rec.built))
    tally = collections.Counter()
    resumable = []
    last_time = [0]

    def each(n, res, view, inner):
        if res:
            resumable.append(n)
        world = view.get((1, 0))
        if world:
            v = field_values(rec.view_schema, 1, world[0]).get("time")
            if v:
                last_time[0] = struct.unpack("<i", v[1])[0]

    n, view, _ = rec.read(tally=tally, each=each)
    seconds = last_time[0] // 1024
    total = sum(rec.packed.values())
    print("%d steps, game time %d:%02d; %d bytes, %.0f a step" % (n, seconds // 60, seconds % 60, total, total / max(n, 1)))
    shown = resumable if len(resumable) <= 8 else resumable[:4] + ["..."] + resumable[-3:]
    print("the level can go on from step 0 and %d more: %s" % (len(resumable), " ".join(map(str, shown))))
    units = collections.Counter(kind_name(k) for k, _ in view if k >= 16)
    print("at the end: " + ", ".join("%d %s" % (c, u) for u, c in sorted(units.items())))
    print("unpacked bytes a step, most first:")
    for (part, what), size in tally.most_common(20):
        print("%9.1f  %s %s" % (size / max(n, 1), part, what))


def compare(a, b):
    print("%s: %s; %s: %s" % (a.path, a.started, b.path, b.started))
    n = 0
    for n, (sa, sb) in enumerate(zip(a.steps(), b.steps()), 1):
        if sa != sb:
            break
    else:
        print("the same for all %d steps they share" % n)
        return
    print("step %d differs" % n)
    _, va, ia = a.read(n, internal=True)
    _, vb, ib = b.read(n, internal=True)
    parts = [("view", va, vb, a.view_schema), ("internal", ia, ib, a.internal_schema)]
    if (sa[0] != 0) != (sb[0] != 0):
        # As when one ends there: its last step is one to go on from
        print("only %s can go on from it, so only its views are compared" % (a.path if sa[0] else b.path))
        parts = parts[:1]
    lines = []
    for part, oa, ob, schema in parts:
        for key in sorted(set(oa) | set(ob)):
            name = "%s %d" % (kind_name(key[0]), key[1])
            if key not in ob or key not in oa:
                lines.append("%s %s: only in %s" % (part, name, a.path if key in oa else b.path))
                continue
            fa, fb = field_values(schema, key[0], oa[key][0]), field_values(schema, key[0], ob[key][0])
            for fname, (f, raw) in fa.items():
                if fb.get(fname, (None, None))[1] != raw:
                    lines.append("%s %s %s: %s / %s" % (part, name, fname, f.show(raw),
                                                        f.show(fb[fname][1]) if fname in fb else "-"))
    if not lines:
        print("its views are the same")
    for line in lines[:40]:
        print(line)
    if len(lines) > 40:
        print("... and %d more" % (len(lines) - 40))


def show_at(rec, step, internal):
    n, view, inner = rec.read(step, internal=internal)
    print("after step %d:" % n)
    for part, objects, schema in (("view", view, rec.view_schema), ("internal", inner, rec.internal_schema)):
        if part == "internal" and not internal:
            continue
        for key in sorted(objects):
            values = field_values(schema, key[0], objects[key][0])
            print("%s %s %d: %s" % (part, kind_name(key[0]), key[1],
                                    ", ".join("%s %s" % (k, f.show(v)) for k, (f, v) in values.items())))


def main(args):
    if len(args) == 1:
        summary(Recording(args[0]))
    elif len(args) == 2:
        compare(Recording(args[0]), Recording(args[1]))
    elif len(args) >= 3 and args[1] == "--at":
        show_at(Recording(args[0]), int(args[2]), "--internal" in args[3:])
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main(sys.argv[1:])
