#!/usr/bin/env python3
"""Makes a bigger network level out of one of the game's: N x N copies of its
inside (its border dropped) in one border around them all.

  tilemap.py SOURCE.ldf N OUT.ldf

Each host station keeps its corner: it stands in the copy at its own corner
of the big map, where it stood in the source. In each copy, sectors of other
host stations' owners become neutral. Gems are repeated in every copy; items
(superbombs) are left out. Titles get "BIG_<W>X<H>_" in front.

The game finds network levels as LEVELS/MULTI/l<n><n>.ldf in its folder, n
from 1 to 255, e.g. l200200.ldf for level 200:

  tool/tilemap.py ~/Games/urban-assault/LEVELS/MULTI/l8181.ldf 2 l200200.ldf

makes a 60 x 60 level of level 81 (31 x 31) for four players.
"""
import re, sys

if len(sys.argv) != 4:
    sys.exit(__doc__)

src, n, out = sys.argv[1], int(sys.argv[2]), sys.argv[3]
text = open(src, 'rb').read().decode('latin-1').replace('\r', '')
lines = text.split('\n')

def blocks(name):
    """(start, end) line indexes of every 'begin_<name>' .. 'end' block"""
    res = []
    i = 0
    while i < len(lines):
        if re.match(r'\s*begin_' + name + r'\b', lines[i]):
            j = i
            while not re.match(r'\s*end\b', lines[j]):
                j += 1
            res.append((i, j))
            i = j
        i += 1
    return res

def read_map(key):
    for i, l in enumerate(lines):
        if re.match(r'\s*' + key + r'\s*=', l):
            w, h = map(int, lines[i + 1].split()[:2])
            rows = [lines[i + 2 + r].split()[:w] for r in range(h)]
            return w, h, rows
    raise SystemExit('no ' + key)

w, h, typ = read_map('typ_map')
_, _, own = read_map('own_map')
_, _, hgt = read_map('hgt_map')
_, _, blg = read_map('blg_map')
iw, ih = w - 2, h - 2
W, H = iw * n + 2, ih * n + 2
SECTOR = 1200

def field(block, key):
    for l in lines[block[0]:block[1]]:
        m = re.match(r'\s*' + key + r'\s*=\s*(-?\w+)', l)
        if m:
            return m.group(1)
    return None

# Each host station's copy: the corner of the big map on its side of the source
robos = []
for b in blocks('robo'):
    owner = int(field(b, 'owner'))
    sx = int(float(field(b, 'pos_x')) // SECTOR)
    sy = int(-float(field(b, 'pos_z')) // SECTOR)
    tx = 0 if sx <= w // 2 else n - 1
    ty = 0 if sy <= h // 2 else n - 1
    robos.append((b, owner, tx, ty))
robo_tile = {owner: (tx, ty) for _, owner, tx, ty in robos}

def big(rows, cell):
    res = []
    for Y in range(H):
        row = []
        for X in range(W):
            if Y in (0, H - 1) or X in (0, W - 1):
                # The source's border: corners and edges
                sy = 0 if Y == 0 else (h - 1 if Y == H - 1 else 1)
                sx = 0 if X == 0 else (w - 1 if X == W - 1 else 1)
                row.append(rows[sy][sx])
            else:
                tx, ty = (X - 1) // iw, (Y - 1) // ih
                row.append(cell(rows, 1 + (X - 1) % iw, 1 + (Y - 1) % ih, tx, ty))
        res.append(row)
    return res

plain = lambda rows, x, y, tx, ty: rows[y][x]

def owner_cell(rows, x, y, tx, ty):
    o = int(rows[y][x], 16)
    if o in robo_tile and robo_tile[o] != (tx, ty):
        return '00'
    return rows[y][x]

maps = {'typ_map': big(typ, plain), 'own_map': big(own, owner_cell),
        'hgt_map': big(hgt, plain), 'blg_map': big(blg, plain)}

res = []
skip = set()
for b in blocks('item'):
    skip.update(range(b[0], b[1] + 1))
for b, owner, tx, ty in robos:
    skip.update(range(b[0], b[1] + 1))
gem_blocks = blocks('gem')
for b in gem_blocks:
    skip.update(range(b[0], b[1] + 1))
maps_block = blocks('maps')[0]
skip.update(range(maps_block[0], maps_block[1] + 1))

def shifted(b, dx, dy, robo=False):
    out_lines = []
    for l in lines[b[0]:b[1] + 1]:
        m = re.match(r'(\s*)(pos_x|pos_z|sec_x|sec_y)(\s*=\s*)(-?\d+)(.*)', l)
        if m:
            v = int(m.group(4))
            key = m.group(2)
            if key == 'pos_x': v += dx * iw * SECTOR
            if key == 'pos_z': v -= dy * ih * SECTOR
            if key == 'sec_x': v += dx * iw
            if key == 'sec_y': v += dy * ih
            l = m.group(1) + key + m.group(3) + str(v) + m.group(5)
        out_lines.append(l)
    return out_lines

robos_emitted = False
for i, l in enumerate(lines):
    if i in skip:
        if not robos_emitted and any(i == b[0] for b, *_ in robos):
            for b, owner, tx, ty in robos:
                res += shifted(b, tx, ty)
            robos_emitted = True
        if gem_blocks and i == gem_blocks[0][0]:
            for ty in range(n):
                for tx in range(n):
                    for b in gem_blocks:
                        res += shifted(b, tx, ty)
                        res.append('')
        if i == maps_block[0]:
            res.append('begin_maps')
            for key in ('typ_map', 'own_map', 'hgt_map', 'blg_map'):
                res.append('    %s =' % key)
                res.append('        %d %d' % (W, H))
                for row in maps[key]:
                    res.append('        ' + ' '.join(row) + ' ')
            res.append('end')
        continue
    m = re.match(r'(\s*title_\w+\s*=\s*)(\S+)', l)
    if m:
        l = m.group(1) + 'BIG_%dX%d_' % (W, H) + m.group(2)
    res.append(l)

open(out, 'wb').write('\r\n'.join(res).encode('latin-1'))
print(f'{out}: {W}x{H} sectors, {len(robos)} host stations at', [(o, robo_tile[o]) for _, o, *_ in robos], f'{len(gem_blocks) * n * n} gems')
