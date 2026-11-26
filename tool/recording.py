#!/usr/bin/env python3
"""Reads recordings made with UA_source --record (the format is described in
src/world/recorder.h; the state's fields in src/world/state.h).

  recording.py FILE                  what it holds: steps, keyframes, bytes
                                     per part and field
  recording.py FILE OTHER            the first step and the first keyframe
                                     where two recordings differ, and the
                                     fields that do
  recording.py FILE --at STEP        the view after that step, object by
                                     object; --internal adds the internal part
                                     (only at keyframes)
"""
import collections
import math
import os
import struct
import sys
import zlib

ELEM_SIZE = {1: 1, 2: 1, 3: 1, 4: 2, 5: 2, 6: 4, 7: 4, 8: 4, 9: 8, 10: 4, 11: 4}
ELEM_FORMAT = {1: "B", 2: "b", 3: "B", 4: "h", 5: "H", 6: "i", 7: "I", 8: "f", 9: "d", 10: "I", 11: "i"}
T_F32, T_F64, T_UNIT = 8, 9, 10
KIND_NAMES = {1: "world", 2: "sector", 3: "bomb", 4: "vehicle", 5: "weapon", 6: "building", 7: "campaign"}
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


def read_image(raw, p):
    """An image's objects, as State::Serialize writes them, from p on; and
    where it ends."""
    level, count = struct.unpack_from("<iI", raw, p)
    p += 8
    objects = {}
    for _ in range(count):
        kind, oid, size = struct.unpack_from("<BII", raw, p)
        p += 9
        objects[(kind, oid)] = raw[p:p + size]
        p += size
    return objects, p


MAX_GUESSED = 256  # Changed fields bigger than this are stored as they are


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
                if f.list or size > MAX_GUESSED:
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
    """A recording's chunks, found as it's opened; their bytes are read when
    they're needed."""

    def __init__(self, path):
        self.path = path
        self.file = open(path, "rb")
        head = self.file.read(8)
        if head[:6] != b"UAREC\0":
            sys.exit("%s: not a recording" % path)
        version, = struct.unpack("<H", head[6:8])
        if version != 3:
            sys.exit("%s: a recording of format %d, not 3" % (path, version))
        self.keyframes, self.blocks, self.packed = [], [], collections.Counter()
        total, p = os.path.getsize(path), 8
        while p + 12 <= total:
            self.file.seek(p)
            chunk = self.file.read(12)
            tag = chunk[:4].decode("latin-1")
            size, = struct.unpack("<Q", chunk[4:])
            if p + 12 + size > total:
                print("%s is cut short after step %d" % (path, self.last_step()), file=sys.stderr)
                break
            at = p + 12
            p += 12 + size
            self.packed[tag] += size
            if tag in ("INFO", "SCHV", "SCHI"):
                body = self.file.read(size)
                if tag == "INFO":
                    self.level, = struct.unpack("<H", body[:2])
                    texts = [t.decode("latin-1") for t in body[2:].split(b"\0")[:4]]
                    self.started, self.platform, self.compiler, self.built = texts
                elif tag == "SCHV":
                    self.view_schema, self.view_schema_bytes = read_schema(body), body
                else:
                    self.internal_schema, self.internal_schema_bytes = read_schema(body), body
            elif tag == "KEYF":
                step, = struct.unpack("<I", self.file.read(4))
                self.keyframes.append((step, at, size))
            elif tag == "STEP":
                first, steps = struct.unpack("<II", self.file.read(8))
                self.blocks.append((first, steps, at, size))
        if not self.keyframes:
            sys.exit("%s: no keyframe" % path)

    def last_step(self):
        last = self.keyframes[-1][0] if self.keyframes else 0
        if self.blocks:
            last = max(last, self.blocks[-1][0] + self.blocks[-1][1] - 1)
        return last

    def chunk(self, at, size):
        self.file.seek(at)
        return self.file.read(size)

    def keyframe(self, index):
        """A keyframe's view and internal part."""
        _, at, size = self.keyframes[index]
        raw = zlib.decompress(self.chunk(at, size)[8:])
        view, p = read_image(raw, 0)
        internal, _ = read_image(raw, p)
        return view, internal

    def keyframe_raw(self, index):
        _, at, size = self.keyframes[index]
        return zlib.decompress(self.chunk(at, size)[8:])

    def block_steps(self, block):
        """A block's steps: each one's number and bytes."""
        first, _, at, size = block
        raw = zlib.decompress(self.chunk(at, size)[12:])
        p, n = 0, first
        while p + 4 <= len(raw):
            size, = struct.unpack_from("<I", raw, p)
            yield n, raw[p + 4:p + 4 + size]
            p += 4 + size
            n += 1

    def steps(self):
        """Every step's number and bytes, in order."""
        for block in self.blocks:
            yield from self.block_steps(block)

    def read(self, upto=None, internal=False, tally=None, each=None):
        """The view after step upto (the last when None), and the internal
        part of the last keyframe up to there. each(step, keyframe, view,
        internal) is called at the first keyframe and after every step."""
        keyed = {step: i for i, (step, _, _) in enumerate(self.keyframes)}
        v, inner = self.keyframe(0)
        view = {k: [d, d] for k, d in v.items()}
        if not internal:
            inner = {}
        n = self.keyframes[0][0]
        if each:
            each(n, True, view, inner)
        for block in self.blocks:
            if upto is not None and n >= upto:
                break
            # After a keyframe the guesses start over
            for obj in view.values():
                obj[1] = obj[0]
            for step, raw in self.block_steps(block):
                if upto is not None and step > upto:
                    break
                n = step
                p = apply_changes(view, self.view_schema, raw, 0, tally, "view")
                if tally is not None:
                    tally[("messages", "")] += len(raw) - p
                keyframe = n in keyed
                if keyframe and internal:
                    inner = self.keyframe(keyed[n])[1]
                if each:
                    each(n, keyframe, view, inner)
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
    last_time = [0]

    def each(n, keyframe, view, inner):
        world = view.get((1, 0))
        if world:
            v = field_values(rec.view_schema, 1, world[0]).get("time")
            if v:
                last_time[0] = struct.unpack("<i", v[1])[0]

    n, view, _ = rec.read(tally=tally, each=each)
    first = rec.keyframes[0][0]
    steps = n - first
    seconds = last_time[0] // 1024
    total = os.path.getsize(rec.path)
    print("steps %d to %d, game time %d:%02d; %d bytes, %.0f a step" % (first, n, seconds // 60, seconds % 60, total,
                                                                         total / max(steps, 1)))
    keys = [k[0] for k in rec.keyframes]
    shown = keys if len(keys) <= 8 else keys[:4] + ["..."] + keys[-3:]
    print("%d keyframes: %s" % (len(keys), " ".join(map(str, shown))))
    print("packed: steps %d bytes, keyframes %d bytes (%.1f%%)" % (rec.packed["STEP"], rec.packed["KEYF"],
                                                                   100.0 * rec.packed["KEYF"] / max(total, 1)))
    units = collections.Counter(kind_name(k) for k, _ in view if k >= 16)
    print("at the end: " + ", ".join("%d %s" % (c, u) for u, c in sorted(units.items())))
    print("unpacked bytes a step, most first:")
    for (part, what), size in tally.most_common(20):
        print("%9.1f  %s %s" % (size / max(steps, 1), part, what))


def object_diffs(part, oa, ob, schema, name_a, name_b):
    lines = []
    for key in sorted(set(oa) | set(ob)):
        name = "%s %d" % (kind_name(key[0]), key[1])
        if key not in ob or key not in oa:
            lines.append("%s %s: only in %s" % (part, name, name_a if key in oa else name_b))
            continue
        fa, fb = field_values(schema, key[0], oa[key]), field_values(schema, key[0], ob[key])
        for fname, (f, raw) in fa.items():
            if fb.get(fname, (None, None))[1] != raw:
                lines.append("%s %s %s: %s / %s" % (part, name, fname, f.show(raw),
                                                    f.show(fb[fname][1]) if fname in fb else "-"))
    return lines


def show_lines(lines):
    for line in lines[:40]:
        print(line)
    if len(lines) > 40:
        print("... and %d more" % (len(lines) - 40))


def compare(a, b):
    print("%s: %s; %s: %s" % (a.path, a.started, b.path, b.started))
    same_fields = a.view_schema_bytes == b.view_schema_bytes and a.internal_schema_bytes == b.internal_schema_bytes
    if not same_fields:
        print("they were recorded with different fields, so their values aren't compared")

    # The keyframes they both have
    kb = {step: i for i, (step, _, _) in enumerate(b.keyframes)}
    shared = 0
    for i, (step, _, _) in enumerate(a.keyframes):
        if step not in kb:
            continue
        shared += 1
        if a.keyframe_raw(i) != b.keyframe_raw(kb[step]):
            print("their keyframes at step %d differ" % step)
            if same_fields:
                (va, ia), (vb, ib) = a.keyframe(i), b.keyframe(kb[step])
                lines = object_diffs("view", va, vb, a.view_schema, a.path, b.path)
                lines += object_diffs("internal", ia, ib, a.internal_schema, a.path, b.path)
                show_lines(lines)
            break
    else:
        print("the same at all %d keyframes they share" % shared)

    # The steps they both have
    sb = dict(b.steps())
    n = shared = 0
    for n, raw in a.steps():
        if n not in sb:
            continue
        shared += 1
        if sb[n] != raw:
            break
    else:
        print("the same for all %d steps they share" % shared)
        return
    print("step %d differs" % n)
    if same_fields:
        _, va, _ = a.read(n)
        _, vb, _ = b.read(n)
        lines = object_diffs("view", {k: v[0] for k, v in va.items()}, {k: v[0] for k, v in vb.items()},
                             a.view_schema, a.path, b.path)
        if not lines:
            print("its views are the same")
        show_lines(lines)


def show_at(rec, step, internal):
    n, view, inner = rec.read(step, internal=internal)
    print("after step %d:" % n)
    if internal and n not in [k[0] for k in rec.keyframes]:
        print("no keyframe there: the internal part is the last keyframe's")
    for part, objects, schema in (("view", {k: v[0] for k, v in view.items()}, rec.view_schema),
                                  ("internal", inner, rec.internal_schema)):
        if part == "internal" and not internal:
            continue
        for key in sorted(objects):
            values = field_values(schema, key[0], objects[key])
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
