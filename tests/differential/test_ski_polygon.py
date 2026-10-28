"""The skis' debug polygon against the original x86 (docs/movement-ground.md,
The skis' debug quads): 0x4776d0 (fewer than 3 points refused; the zone list 0x53cc40, the
add 0x53c6c0) with its material 0x477730 (three rand()s until they sum to
150, packed unscaled, added through 0x536ad0), and the model priority
0x53bb20. The x87 at 24 bits; rand() is MSVCRT's, seeded the same on both
sides.

A run of adds builds one model from empty: the skis' own quads (flat and
upright along a ski), other polygons of 1..8 points, colinear ones. After
each add: the answer, the caller's points, the model's vertices and
polygons with their materials' colours, and the next rand(). Then the
priority on the model (and on none).

    cmake --build build && .venv/bin/python tests/differential/test_ski_polygon.py

BREAK=<n> breaks the port's side in a known way (see main); each must fail.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_weiler import BUF, CAP, Emulated, P, got, probe  # noqa: E402

probe.probe_ski_begin.argtypes = [ctypes.c_uint32]
probe.probe_ski_add.argtypes = [ctypes.c_int, P, P, ctypes.c_int]
probe.probe_ski_priority.argtypes = [ctypes.c_int, ctypes.c_int, P, ctypes.c_int]
IAT_RAND = 0x5bb668
# zModel's material table (gmod_matl.h): count, capacity, blocks, used, last; the heads.
TABLE = (0x63fbc0, 0x63fbc4, 0x63fbc8, 0x63fbcc, 0x63fbd0)
HEADS = (0x5edd54, 0x5edd58)


class Original(Emulated):
    def __init__(self):
        super().__init__()
        self.seed = 1
        self.cpu.stub_import(IAT_RAND, self.rand)

    def rand(self, c=None):
        self.seed = (self.seed * 214013 + 2531011) & 0xffffffff
        return (self.seed >> 16) & 0x7fff

    def begin_model(self, seed):
        self.begin()
        self.seed = seed
        for a in TABLE:
            self.cpu.write(a, b'\0\0\0\0')
        for a in HEADS:
            self.cpu.write(a, struct.pack('<i', -1))
        self.model = self.alloc(0x60)

    def dump_model(self):
        m = self.model
        cp, cv = self.u32(m + 0x10), self.u32(m + 0x14)
        self.log(cp, cv)
        self.raw(self.u32(m + 0x38), 12 * cv)
        polys = self.u32(m + 0x34)
        for i in range(cp):
            q = polys + 0x24 * i
            flags = self.u32(q)
            self.log(flags, self.u32(q + 4), self.u32(q + 0x20))
            self.raw(self.u32(q + 8), 4 * (flags & 0xff))
            mat = self.u32(q + 0x1c)
            self.log(1 if mat else 0)
            if mat:
                b = self.cpu.read(mat, 0x10)
                self.log(b[0], b[1] & 7, struct.unpack_from('<H', b, 2)[0])  # bits 3..7: stack garbage
                self.out += b[4:16]

    def add(self, n, points):
        self.out = bytearray()
        pp = self.put(struct.pack(f'<{len(points)}f', *points))
        self.log(self.call(0x4776d0, ecx=self.model, edx=n, stack=(pp,)) & 0xff)
        self.raw(pp, 12 * n)
        self.dump_model()
        self.log(self.rand())
        return bytes(self.out)

    def priority(self, p, null_model):
        self.out = bytearray()
        self.call(0x53bb20, ecx=0 if null_model else self.model, edx=p)
        m = self.model
        polys = self.u32(m + 0x34)
        for i in range(self.u32(m + 0x10)):
            self.log(self.u32(polys + 0x24 * i + 4))
        return bytes(self.out)


def ski_quads(rng):
    """0x476090's two quads: a 1 m wide flat one and an upright one along a ski."""
    x, h = rng.choice((-1, 1)) * rng.uniform(0.5, 4.0), rng.uniform(1.0, 6.0)
    flat = [x + 0.5, 0, h, x + 0.5, 0, -h, x - 0.5, 0, -h, x - 0.5, 0, h]
    upright = [x, 0.5, h, x, 0.5, -h, x, -0.5, -h, x, -0.5, h]
    return [flat, upright]


def other(rng):
    n = rng.choice((1, 2, 3, 3, 4, 5, 8))
    pts = [rng.uniform(-20, 20) for _ in range(3 * n)]
    if n >= 3 and rng.random() < 0.3:  # all on a line: refused once the colinear go
        a, b = pts[0:3], pts[3:6]
        pts = [a[i % 3] + (b[i % 3] - a[i % 3]) * (i // 3) for i in range(3 * n)]
    return pts


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

    for _ in range(200):
        seed = rng.randrange(1 << 32)
        probe.probe_ski_begin(seed ^ (1 if brk == 1 else 0))
        emu.begin_model(seed)
        for _ in range(rng.randint(1, 6)):
            batch = ski_quads(rng) if rng.random() < 0.6 else [other(rng)]
            for pts in batch:
                kind = 'ski' if len(batch) == 2 else f'n{len(pts) // 3}'
                n = len(pts) // 3
                mine_pts = list(pts)
                if brk == 2 and n >= 3:
                    mine_pts[0] += 0.25
                buf = (ctypes.c_float * max(len(mine_pts), 1))(*mine_pts)
                mine = got(probe.probe_ski_add(n, ctypes.cast(buf, P), BUF, CAP))
                theirs = emu.add(n, pts)
                check(kind, mine, theirs)
        p = rng.choice((0, 1, 10, -3, 0x7fffffff))
        null_model = rng.random() < 0.2
        mine = got(probe.probe_ski_priority(p + (1 if brk == 3 else 0), 1 if null_model else 0, BUF, CAP))
        check('priority_null' if null_model else 'priority', mine, emu.priority(p, null_model))

    if fails:
        print(f'{fails} of {runs} runs differ')
        sys.stdout.flush(); os._exit(1)
    print(f'ok: {runs} runs identical ({", ".join(f"{k} {v}" for k, v in sorted(kinds.items()))})')
    sys.stdout.flush(); os._exit(0)


if __name__ == '__main__':
    main()
