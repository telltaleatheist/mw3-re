"""zModel's polygon add (craters part 5a) against the original x86
(docs/craters.md, section 5): 0x53c6c0 with the colinear check 0x53a530,
the planarity test and the fan 0x53a930, the pieces past 48 points
0x53ac10, the record 0x53c370 (vertices 0x539d80 / 0x539ec0, normals
0x53a070) and its fill 0x53c500 (UVs flattened 0x53bdc0 / 0x53c270 and
snapped 0x53af50). The x87 at 24 bits.

A run of adds builds one model from empty: ground-like polygons (flat,
tilted, bent out of plane), with points repeated within the merge distance,
on a line, 3..60 of them, textured or not, with or without normals,
colours and morphs, at the default merge distance and the crater's 0.005.
After each add: the code, the reports, the caller's points, the model's
counts, vertices, normals, morphs and every polygon.

    cmake --build build && .venv/bin/python tests/differential/test_gmod_add.py

BREAK=<n> breaks the port's copy of the inputs in a known way (see main);
each must fail.
"""
import ctypes
import math
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from ai_world import FLOOR, IAT_FLOOR  # noqa: E402
from test_weiler import BUF, CAP, Emulated, P, farr, got, probe  # noqa: E402

probe.probe_gadd_begin.argtypes = [ctypes.c_float]
probe.probe_gadd.argtypes = [ctypes.c_int, P, P, P, ctypes.c_int, P, P, ctypes.c_int, ctypes.c_int, ctypes.c_uint32,
                             P, P, P, ctypes.c_int]
PRECISION = 0x5ee3e0


class Original(Emulated):
    def __init__(self):
        super().__init__()
        self.cpu.import_code(IAT_FLOOR, FLOOR)

    def begin_model(self, precision):
        self.begin()
        self.model = self.alloc(0x60)
        self.mats = [self.alloc(0x2c), self.alloc(0x2c)]
        self.cpu.write(self.mats[1] + 1, b'\x01')
        self.cpu.write(PRECISION, struct.pack('<f', precision))
        self.log_start = 0

    def add(self, n, points, normals, uvs, morph, mpoints, muvs, material, priority, flag, zone, colors):
        self.out = bytearray()
        put = lambda v, fmt='f': self.put(struct.pack(f'<{len(v)}{fmt}', *v)) if v is not None else 0
        pp = put(points)
        args = (pp, put(normals), put(uvs), morph, put(mpoints), put(muvs), self.mats[material], priority, flag,
                self.put(bytes(zone)), put(colors))
        code = self.call(0x53c6c0, ecx=self.model, edx=n, stack=args)
        self.log(code)
        self.raw(pp, 12 * n)
        m = self.model
        cp, cv, cn, cm = (self.u32(m + o) for o in (0x10, 0x14, 0x18, 0x1c))
        self.log(cp, cv, cn, cm)
        self.raw(self.u32(m + 0x38), 12 * cv)
        self.raw(self.u32(m + 0x3c), 12 * cn)
        self.raw(self.u32(m + 0x44), 12 * cm)
        polys = self.u32(m + 0x34)
        for i in range(cp):
            q = polys + 0x24 * i
            flags = self.u32(q)
            k = flags & 0xff
            self.log(flags, self.u32(q + 4))
            self.raw(self.u32(q + 8), 4 * k)
            if flags & 0x200:
                self.raw(self.u32(q + 0xc), 4 * k)
            uv = self.u32(q + 0x10)
            self.log(1 if uv else 0)
            if uv:
                self.raw(uv, 8 * k)
            self.raw(self.u32(q + 0x14), 12 * k)
            mat = self.u32(q + 0x1c)
            self.log(1 if mat == self.mats[1] else 0 if mat == self.mats[0] else 9)
            self.log(self.u32(q + 0x20))
        return bytes(self.out)


def polygon(rng, kind):
    """n points of a ground-like polygon: a circle-ish outline in a plane."""
    n = rng.choice((3, 3, 4, 4, 5, 6, 8, 12)) if kind != 'big' else rng.randint(46, 60)
    cx, cz, y0 = rng.uniform(-200, 200), rng.uniform(-200, 200), rng.uniform(-10, 30)
    gx, gz = rng.uniform(-0.5, 0.5), rng.uniform(-0.5, 0.5)
    if kind == 'grid':
        cx, cz, y0 = round(cx), round(cz), round(y0)
        gx = gz = 0.0
    r = rng.uniform(2, 30)
    a0 = rng.uniform(0, 2 * math.pi)
    pts = []
    for j in range(n):
        a = a0 + 2 * math.pi * j / n
        x, z = cx + r * math.cos(a), cz + r * math.sin(a)
        if kind == 'grid':
            x, z = round(x), round(z)
        y = y0 + gx * (x - cx) + gz * (z - cz)
        if kind == 'bent' and j == n // 2:
            y += rng.choice((0.004, 0.01, 0.5, 3.0))
        pts.append([x, y, z])
    if kind == 'colinear' and n >= 4:  # a midpoint on an edge
        j = rng.randrange(n)
        a, b = pts[j], pts[(j + 1) % n]
        t = rng.choice((0.5, 0.25))
        pts.insert(j + 1, [a[i] + t * (b[i] - a[i]) for i in range(3)])
    if kind == 'repeat':  # a point again, within the merge distance
        j = rng.randrange(n)
        pts.insert(j + 1, [v + rng.choice((0.0, 0.0004, 0.003)) for v in pts[j]])
    if kind == 'line':  # three points on a line: dropped to two
        a, b = pts[0], pts[1]
        pts = [a, [(a[i] + b[i]) / 2 for i in range(3)], b]
    if kind == 'wall':  # stood up: x or z the flat axis
        k = rng.choice((0, 2))
        pts = [[p[0], p[2], p[1]] if k == 2 else [p[1], p[2], p[0]] for p in pts]
    if rng.random() < 0.5:
        pts.reverse()
    return [v for p in pts for v in p]


def main():
    rng = random.Random(int(os.environ.get('SEED', '1')))
    brk = int(os.environ.get('BREAK', '0'))
    emu = Original()
    runs = fails = 0
    kinds = {}

    def check(kind, mine, theirs):
        nonlocal runs, fails
        runs += 1
        kinds[kind] = kinds.get(kind, 0) + 1
        if mine != theirs:
            fails += 1
            if fails <= 3:
                i = next((k for k in range(min(len(mine), len(theirs))) if mine[k] != theirs[k]),
                         min(len(mine), len(theirs)))
                print(f'FAIL {kind}: first difference at byte {i} (port {len(mine)}, original {len(theirs)})')
                print('  port    ', mine[max(0, i - 16):i + 32].hex())
                print('  original', theirs[max(0, i - 16):i + 32].hex())

    for model in range(150):
        precision = rng.choice((0.001, 0.005))
        morph = 1 if rng.random() < 0.15 else 0
        probe.probe_gadd_begin(precision)
        emu.begin_model(precision)
        shared = []  # earlier polygons' points, reused so vertices merge
        for _ in range(rng.randint(1, 14)):
            kind = rng.choice(('flat', 'flat', 'grid', 'bent', 'colinear', 'repeat', 'big' if rng.random() < 0.2
                               else 'flat', 'tiny', 'line', 'wall', 'wall'))
            if kind == 'tiny':
                pts = polygon(rng, 'flat')[:3 * rng.choice((1, 2))]
            else:
                pts = polygon(rng, kind)
            if shared and rng.random() < 0.3:  # share an edge with an earlier polygon
                pts[0:6] = shared[-1][3:9] if len(shared[-1]) >= 9 else pts[0:6]
            n = len(pts) // 3
            shared.append(pts)
            textured = rng.random() < 0.7
            normals = [rng.choice((0.0, 1.0, 0.70710677)) for _ in range(3 * n)] if rng.random() < 0.3 else None
            uvs = [rng.uniform(-3, 5) for _ in range(2 * n)] if textured else None
            colors = [float(rng.randint(0, 255)) for _ in range(3 * n)] if rng.random() < 0.6 else None
            mpts = [v + rng.uniform(-1, 1) for v in pts] if morph else None
            muvs = [rng.uniform(0, 1) for _ in range(2 * n)] if morph and textured else None
            priority, flag = rng.randint(0, 3), rng.randint(0, 1)
            zone = [rng.randint(0, 255) for _ in range(4)]
            mine_pts = list(pts)
            if brk == 1:
                mine_pts[1] += 0.01
            mine_uvs = None if uvs is None else list(uvs)
            if brk == 2 and mine_uvs:
                mine_uvs[-1] += 0.25
            c = lambda v, t=ctypes.c_float: ctypes.cast((t * len(v))(*v), P) if v is not None else None
            pbuf = (ctypes.c_float * len(mine_pts))(*mine_pts)
            mine = got(probe.probe_gadd(n, ctypes.cast(pbuf, P), c(normals), c(mine_uvs), morph, c(mpts), c(muvs),
                                        1 if textured else 0, priority, flag, c(zone, ctypes.c_uint8), c(colors),
                                        BUF, CAP))
            theirs = emu.add(n, pts, normals, uvs, morph, mpts, muvs, 1 if textured else 0, priority, flag, zone,
                             colors)
            check(kind, mine, theirs)

    if fails:
        print(f'{fails} of {runs} runs differ')
        sys.stdout.flush(); os._exit(1)
    print(f'ok: {runs} runs identical ({", ".join(f"{k} {v}" for k, v in sorted(kinds.items()))})')
    sys.stdout.flush(); os._exit(0)


if __name__ == '__main__':
    main()
