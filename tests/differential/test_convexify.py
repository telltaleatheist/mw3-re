"""Convexify (craters part 4) against the original x86 (docs/craters.md,
section 5): convexify 0x5162d0 (quads 0x5162d0 itself, larger polygons the
recursive triangulator 0x5166c0 / 0x516a50) on lists of made-up simple
polygons (star shapes and grid shapes, either way round, with ties for the
leftmost point), and the ring triangles 0x515be0 (the plane 0x515f10, the
region's z 0x5160e0, the bridges 0x5158c0 / 0x515990 / 0x515ae0, the
triangles 0x515aa0 / 0x515b30) between ground polygons and crater rings
inside them, each triangle turned up by 0x516190 / 0x516120 as zgeo_model's
clip does. The x87 at 24 bits. Compared: the results by value.

    cmake --build build && .venv/bin/python tests/differential/test_convexify.py

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
from test_weiler import BUF, CAP, Emulated, P, farr, fbuf, got, probe, ring  # noqa: E402

probe.probe_convexify.argtypes = [P, ctypes.c_int, ctypes.c_int, P, P, ctypes.c_int]
probe.probe_ring.argtypes = [ctypes.c_int, P, ctypes.c_int, P, P, ctypes.c_int]


class Original(Emulated):
    def convexify(self, polys):
        self.begin()
        pts = [v for p in polys for v in p]
        total = len(pts) // 3
        pp = self.put(farr(pts) or bytes(12))
        entries, start = [], 0
        for p in polys:
            entries += [len(p) // 3, start]
            start += len(p)
        ep = self.put(struct.pack(f'<{len(entries)}i', *entries) or bytes(4))
        hdr = self.put(struct.pack('<II', len(polys), ep))
        c = self.call(0x5162d0, ecx=hdr, edx=total, stack=(pp,))
        self.log(1 if c else 0)
        if not c:
            self.log(0, 0)
            return bytes(self.out)
        n = self.u32(c)
        self.log(n)
        self.raw(self.u32(c + 4), 8 * n)
        npts = self.u32(c + 8)
        self.log(npts)
        self.raw(self.u32(c + 12), 12 * npts)
        return bytes(self.out)

    def ring(self, poly, region):
        self.begin()
        np_, nr = len(poly) // 3, len(region) // 3
        pp, rp = self.put(farr(poly)), self.put(farr(region))
        t = self.call(0x515be0, ecx=np_, edx=pp, stack=(nr, rp))
        n = self.u32(t)
        self.log(n)
        self.raw(t + 4, 36 * n)
        self.raw(rp, 12 * nr)
        for i in range(n):
            self.log(self.call(0x516190, ecx=3, edx=t + 4 + 36 * i, stack=(1,)))
            self.raw(t + 4 + 36 * i, 36)
        return bytes(self.out)


def star(rng, n, cx=0.0, cy=0.0, grid=False, ccw=None):
    """A simple polygon: n points at sorted angles round a centre."""
    if grid:
        pts = set()
        while len(pts) < n:
            a = rng.uniform(0, 2 * math.pi)
            r = rng.randint(1, 8)
            pts.add((round(cx + r * math.cos(a)), round(cy + r * math.sin(a))))
        pts = sorted(pts, key=lambda q: math.atan2(q[1] - cy, q[0] - cx))
        # drop points that share an angle (not simple)
        out = []
        for q in pts:
            if not out or abs(math.atan2(q[1] - cy, q[0] - cx) - math.atan2(out[-1][1] - cy, out[-1][0] - cx)) > 1e-6:
                out.append(q)
        pts = out
    else:
        angles = sorted(rng.uniform(0, 2 * math.pi) for _ in range(n))
        pts = [(cx + r * math.cos(a), cy + r * math.sin(a)) for a, r in
               zip(angles, (rng.uniform(1, 10) for _ in range(n)))]
    if ccw is None:
        ccw = rng.random() < 0.7
    if not ccw:
        pts.reverse()
    k = rng.randrange(len(pts))
    pts = pts[k:] + pts[:k]
    return [v for x, y in pts for v in (float(x), float(y), rng.uniform(-2, 2))]


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

    # Convexify: lists of 1..4 polygons of 2..14 points.
    for _ in range(1500):
        polys = []
        for _ in range(rng.choice((1, 1, 2, 4))):
            n = rng.choice((2, 3, 4, 4, 4, 5, 6, 8, 10, 14))
            p = star(rng, n, rng.uniform(-50, 50), rng.uniform(-50, 50), grid=rng.random() < 0.4)
            if len(p) // 3 >= 2:
                polys.append(p)
        if not polys:
            continue
        mine_polys = [list(p) for p in polys]
        if brk == 1:
            mine_polys[0][0] += 0.25
        if brk == 2 and len(mine_polys[0]) >= 15:  # a big polygon the other way round
            q = [mine_polys[0][3 * j:3 * j + 3] for j in range(len(mine_polys[0]) // 3)]
            mine_polys[0] = [v for c in q[::-1] for v in c]
        pts = [v for p in mine_polys for v in p]
        entries, start = [], 0
        for p in mine_polys:
            entries += [len(p) // 3, start]
            start += len(p)
        lb = (ctypes.c_int32 * len(entries))(*entries)
        mine = got(probe.probe_convexify(ctypes.cast(lb, P), len(polys), len(pts) // 3, ctypes.cast(fbuf(pts), P),
                                         BUF, CAP))
        sizes = sorted({len(p) // 3 for p in polys})
        check('convexify_' + ('big' if sizes[-1] > 4 else str(sizes[-1])), mine, emu.convexify(polys))

    # Ring triangles: a ground triangle or quad (sloped) and a crater ring
    # inside it.
    for _ in range(800):
        cx, cy = rng.uniform(-300, 300), rng.uniform(-300, 300)
        size = rng.uniform(20, 60)
        k = rng.choice((3, 4, 4, 5))
        if rng.random() < 0.5:
            poly = star(rng, k, cx, cy)
            poly = [v * (size / 10) if i % 3 != 2 else v for i, v in enumerate(poly)]
            poly = [v + (cx - cx * size / 10 if i % 3 == 0 else cy - cy * size / 10 if i % 3 == 1 else 0)
                    for i, v in enumerate(poly)]
        else:
            a0 = rng.uniform(0, 2 * math.pi)
            poly = [v for j in range(k) for v in (cx + size * math.cos(a0 + 2 * math.pi * j / k),
                                                  cy + size * math.sin(a0 + 2 * math.pi * j / k), 0.0)]
        gx, gy, gz = rng.uniform(-0.3, 0.3), rng.uniform(-0.3, 0.3), rng.uniform(-20, 20)
        for j in range(k):
            poly[3 * j + 2] = gz + gx * (poly[3 * j] - cx) + gy * (poly[3 * j + 1] - cy)
        if rng.random() < 0.5:
            pts = [poly[3 * j:3 * j + 3] for j in range(k)][::-1]
            poly = [v for q in pts for v in q]
        xs, ys = poly[0::3], poly[1::3]
        mx, my = sum(xs) / k, sum(ys) / k
        rmax = min(math.hypot(x - mx, y - my) for x, y in zip(xs, ys)) * 0.35
        region = ring(rng, mx + rng.uniform(-0.2, 0.2) * rmax, my + rng.uniform(-0.2, 0.2) * rmax,
                      rmax * rng.uniform(0.3, 1.0), rng.choice((4, 6, 8, 12, 16, 24, 40)), 5.0, rng.choice((0, 0.1)))
        mine_region = list(region)
        if brk == 3:
            mine_region[0] += 0.5
        rb = fbuf(mine_region)
        mine = got(probe.probe_ring(k, ctypes.cast(fbuf(poly), P), len(region) // 3, ctypes.cast(rb, P), BUF, CAP))
        check('ring', mine, emu.ring(poly, region))

    if fails:
        print(f'{fails} of {runs} runs differ')
        sys.stdout.flush(); os._exit(1)
    print(f'ok: {runs} runs identical ({", ".join(f"{k} {v}" for k, v in sorted(kinds.items()))})')
    sys.stdout.flush(); os._exit(0)


if __name__ == '__main__':
    main()
