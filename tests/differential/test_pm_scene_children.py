"""Pirate's Moon's scene render 0x50a550 against the port's zcls::render_world_children with PM's table
(tests/probe_pm_scene_children.cpp): the world's direct children (nodes that span cells, or flagged 0x80) are
drawn (0x508f10, EDX 2) only when flagged 0x80 or when their cell range (shorts +0x4c x0, +0x4e z0, +0x50 x1,
+0x52 z1) meets the cells the picker kept ([0x639020] x0, [0x639024] z0, [0x639028] x1, [0x63902c] z1).
MW3's 0x4f6c00 draws them all. Shadows (0x5403f0) are off in both.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_scene_children.py

Compared: which children are drawn, in order, over crafted edges and random ranges.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_pm_zar  # noqa: E402
from original import ROOT  # noqa: E402

RENDER, SHADOWS, DRAW, MASK_TOP, PICKED = 0x50a550, 0x5403f0, 0x508f10, 0xa4e104, 0x639020
NODE = 0xd4

probe = ctypes.CDLL(str(next((ROOT / 'build').glob('libpmprobe.*'))))
probe.probe_pm_scene_children.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]
MAX = 64
OUT = (ctypes.c_uint32 * MAX)()


def cases():
    # one picked range, children on every side of it, touching and one past
    r = (3, 4, 6, 8)
    kids = []
    for x0, x1 in ((0, 2), (0, 3), (6, 9), (7, 9), (4, 5), (0, 9)):
        for z0, z1 in ((0, 3), (0, 4), (8, 9), (9, 12), (5, 6), (0, 12)):
            kids.append((0, x0, z0, x1, z1))
    kids += [(0x80, -1, -1, 20, 20), (0x80, 50, 50, 60, 60), (0x84, 50, 50, 60, 60), (0x7f, 0, 0, 9, 9)]
    for i in range(0, len(kids), MAX):
        yield kids[i:i + MAX], r
    rng = random.Random(5)
    for _ in range(300):
        a, b = sorted(rng.randint(-3, 20) for _ in range(2))
        c, d = sorted(rng.randint(-3, 20) for _ in range(2))
        ks = []
        for _ in range(rng.randint(0, 20)):
            x0, x1 = sorted(rng.randint(-1, 22) for _ in range(2))
            z0, z1 = sorted(rng.randint(-1, 22) for _ in range(2))
            ks.append((rng.choice((0, 0, 0, 0x80, 0x100, 0x180)), x0, z0, x1, z1))
        yield ks, (a, c, b, d)


def main():
    z = test_pm_zar.Emulated()
    u = z.cpu._uc
    world = z.alloc(NODE)
    arr = z.alloc(4 * MAX)
    nodes = [z.alloc(NODE) for _ in range(MAX)]
    mask = z.alloc(4)
    u.mem_write(MASK_TOP, struct.pack('<I', mask))
    u.mem_write(world + 0x5c, struct.pack('<I', arr))
    u.mem_write(arr, struct.pack(f'<{MAX}I', *nodes))
    drawn = []
    z.native_stub(SHADOWS, lambda c: 0, 0)
    z.native_stub(DRAW, lambda c: drawn.append((nodes.index(c.arg(0)), c.arg(1))) or 0, 0)
    bad = runs = shown = 0
    for kids, r in cases():
        for i, (f, x0, z0, x1, z1) in enumerate(kids):
            u.mem_write(nodes[i] + 0x24, struct.pack('<I', f))
            u.mem_write(nodes[i] + 0x4c, struct.pack('<4h', x0, z0, x1, z1))
        u.mem_write(world + 0x56, struct.pack('<h', len(kids)))
        u.mem_write(PICKED, struct.pack('<4i', *r))
        u.mem_write(mask, struct.pack('<I', 0))
        drawn.clear()
        z.call(RENDER, args=(world,))
        e = [i for i, _ in drawn]
        mode_ok = all(m == 2 for _, m in drawn) and struct.unpack('<I', bytes(u.mem_read(mask, 4)))[0] == 0x3f
        flat = (ctypes.c_int32 * (5 * max(1, len(kids))))(*[v for k in kids for v in k])
        rr = (ctypes.c_int32 * 4)(*r)
        k = probe.probe_pm_scene_children(len(kids), flat, rr, OUT)
        p = list(OUT[:k])
        runs += 1
        if e != p or not mode_ok:
            bad += 1
            if bad <= 5:
                print(f'FAIL range {r} kids {kids}\n  exe  {e}\n  port {p} (mode/mask ok {mode_ok})')
        elif shown < 2:
            shown += 1
            print(f'range {r}: {len(e)} of {len(kids)} drawn')
    print(f'{runs} scenes, {bad} differ')
    sys.stdout.flush()
    os._exit(1 if bad else 0)


if __name__ == '__main__':
    main()
