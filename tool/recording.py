#!/usr/bin/env python3
"""Reads recordings made with UA_source --record (the format is described in
src/world/recorder.h).

  recording.py FILE                   what it holds, after checking every step
  recording.py FILE OTHER             the first block, step or save line where
                                      two recordings differ
  recording.py FILE --save N OUT      writes block N's save game to OUT, which
                                      UA_source --load can start from
"""
import struct
import sys
import zlib

OBJECT_FIELDS = ["kind", "status", "energy", "position", "rotation", "look", "targets", "aggression"]
OBJECT_FIELD_SIZES = [7, 5, 4, 12, 18, 1, 10, 1]
SECTOR_SIZE = 13


def chunks(data, start, end):
    """The IFF chunks between start and end: (tag or FORM type, start, end)."""
    pos = start
    while pos + 8 <= end:
        tag = data[pos:pos + 4]
        size = struct.unpack(">I", data[pos + 4:pos + 8])[0]
        if size == 0xFFFFFFFF:  # Never written, as when the recording was cut short
            size = end - pos - 8
        body = pos + 8
        if tag == b"FORM":
            yield data[body:body + 4], body + 4, body + size
        else:
            yield tag, body, body + size
        pos = body + size + (size & 1)


def read(path):
    """The recording's INFO, and its blocks as {'KEYF': save, 'STEP': steps}."""
    data = open(path, "rb").read()
    forms = list(chunks(data, 0, len(data)))
    if len(forms) != 1 or forms[0][0] != b"RPLY":
        sys.exit("%s: not a recording" % path)
    _, start, end = forms[0]
    info, blocks = None, []
    for tag, s, e in chunks(data, start, end):
        if tag == b"INFO":
            version, level = struct.unpack("<HH", data[s:s + 4])
            texts = [t.decode("latin-1") for t in data[s + 4:e].split(b"\0")[:4]]
            info = dict(version=version, level=level, started=texts[0], platform=texts[1],
                        compiler=texts[2], built=texts[3])
        elif tag == b"BLCK":
            block = {}
            try:
                for t, cs, ce in chunks(data, s, min(e, len(data))):
                    size = struct.unpack("<I", data[cs:cs + 4])[0]
                    unpacked = zlib.decompress(data[cs + 4:ce])
                    if len(unpacked) != size:
                        raise ValueError("%s unpacks to %d bytes, not %d" % (t.decode(), len(unpacked), size))
                    block[t.decode()] = unpacked
                if set(block) != {"KEYF", "STEP"}:
                    raise ValueError("it has %s" % sorted(block))
            except (zlib.error, struct.error, ValueError) as error:
                print("%s is cut short: block %d is incomplete (%s); reading the %d before it"
                      % (path, len(blocks), error, len(blocks)), file=sys.stderr)
                break
            blocks.append(block)
    return info, blocks


def steps(raw):
    """The steps of a block's STEP chunk, one dict each."""
    p = 0
    while p < len(raw):
        step, time, units_hash, sectors_hash, controlled = struct.unpack_from("<IiQQI", raw, p)
        p += 28
        count, = struct.unpack_from("<H", raw, p)
        p += 2
        objects = []
        for _ in range(count):
            oid, fields = struct.unpack_from("<IH", raw, p)
            p += 6
            values = {}
            for i, size in enumerate(OBJECT_FIELD_SIZES):
                if fields & (1 << i):
                    values[OBJECT_FIELDS[i]] = raw[p:p + size]
                    p += size
            objects.append((oid, fields, values))
        count, = struct.unpack_from("<H", raw, p)
        p += 2
        gone = list(struct.unpack_from("<%dI" % count, raw, p))
        p += 4 * count
        count, = struct.unpack_from("<H", raw, p)
        p += 2
        sectors = []
        for _ in range(count):
            index, = struct.unpack_from("<H", raw, p)
            sectors.append((index, raw[p + 2:p + 2 + SECTOR_SIZE]))
            p += 2 + SECTOR_SIZE
        count, = struct.unpack_from("<H", raw, p)
        p += 2
        messages = []
        for _ in range(count):
            msg_id, priority, unit, length = struct.unpack_from("<ihIH", raw, p)
            p += 12
            messages.append((msg_id, priority, unit, raw[p:p + length].decode("latin-1")))
            p += length
        yield dict(step=step, time=time, units_hash=units_hash, sectors_hash=sectors_hash,
                   controlled=controlled, objects=objects, gone=gone, sectors=sectors, messages=messages)


def summary(path):
    info, blocks = read(path)
    print("Level %(level)d, started from %(started)s; %(platform)s, %(compiler)s, built %(built)s" % info)
    count = records = sectors = messages = previous = 0
    last = None
    for b, block in enumerate(blocks):
        alive = set()
        for s in steps(block["STEP"]):
            if s["step"] != previous + 1:
                sys.exit("block %d: step %d follows step %d" % (b, s["step"], previous))
            previous = s["step"]
            for oid, fields, _ in s["objects"]:
                if oid not in alive and fields != 0xFF:
                    sys.exit("step %d: object %d appears without every field" % (s["step"], oid))
                alive.add(oid)
            for oid in s["gone"]:
                if oid not in alive:
                    sys.exit("step %d: object %d goes without having been there" % (s["step"], oid))
                alive.discard(oid)
            count += 1
            records += len(s["objects"])
            sectors += len(s["sectors"])
            messages += len(s["messages"])
            last = s
    if not count:
        print("%d blocks, no steps" % len(blocks))
        return
    print("%d blocks, %d steps to game time %d:%02d; %.0f object records a step, %d sector records, %d messages"
          % (len(blocks), count, last["time"] // 1024 // 60, last["time"] // 1024 % 60, records / count, sectors, messages))


def compare(path, other):
    (info_a, blocks_a), (info_b, blocks_b) = read(path), read(other)
    for key in ("level", "started"):
        if info_a[key] != info_b[key]:
            print("They differ in %s: %r and %r" % (key, info_a[key], info_b[key]))
    for n, (a, b) in enumerate(zip(blocks_a, blocks_b)):
        if a["STEP"] != b["STEP"]:
            for s, t in zip(steps(a["STEP"]), steps(b["STEP"])):
                if s != t:
                    what = [k for k in ("units_hash", "sectors_hash", "objects", "gone", "sectors", "messages") if s[k] != t[k]]
                    print("Block %d: step %d differs first, in %s" % (n, s["step"], ", ".join(what)))
                    return
        if a["KEYF"] != b["KEYF"]:
            lines_a, lines_b = a["KEYF"].split(b"\n"), b["KEYF"].split(b"\n")
            i = next((i for i, (x, y) in enumerate(zip(lines_a, lines_b)) if x != y), min(len(lines_a), len(lines_b)))
            section = next((l for l in reversed(lines_a[:i]) if l.startswith(b"begin_")), b"the start")
            print("Block %d: the save differs at line %d, in %s" % (n, i + 1, section.decode("latin-1")))
            return
    if len(blocks_a) != len(blocks_b):
        print("The same as far as both go, but one has %d blocks and the other %d" % (len(blocks_a), len(blocks_b)))
    else:
        print("The same")


if __name__ == "__main__":
    args = sys.argv[1:]
    if len(args) == 1:
        summary(args[0])
    elif len(args) == 2:
        compare(args[0], args[1])
    elif len(args) == 4 and args[1] == "--save":
        _, blocks = read(args[0])
        open(args[3], "wb").write(blocks[int(args[2])]["KEYF"])
    else:
        sys.exit(__doc__)
