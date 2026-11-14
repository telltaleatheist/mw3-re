"""Pirate's Moon's camera setters 0x506ae0 / 0x506cd0 (rotation, position), adders 0x506c10 / 0x506d90 and
add_child 0x506760 against the port's zcls::camera_* with PM's table (tests/probe_pm_camera_moved.cpp): after
MW3's logic, a camera with children (add_child: any) marks itself and everything below it moved (0x4ff6d0: an
object3d's data word | 0x20, +0x2c | 1, flags | 0x2000000; a child already flagged 0x2000000 is not entered).
The camera has no parents here (the changed-list step, 0x506ba0, is MW3's and the walk test's).

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_camera_moved.py

Compared: every node's flags, +0x2c and object3d data word after the call (the added child's too), and the result.
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

SET_ROT, SET_POS, ADD_ROT, ADD_POS, ADD_CHILD = 0x506ae0, 0x506cd0, 0x506c10, 0x506d90, 0x506760
LIST_PUSH = 0x50b9e0  # the link's changed list (7): stubbed, its item not compared
NODE, CAM = 0xd4, 0x1e8
MAX = 12

probe = ctypes.CDLL(str(next((ROOT / 'build').glob('libpmprobe.*'))))
probe.probe_pm_camera_moved.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p]
OUT = (ctypes.c_uint32 * (3 * MAX + 3))()
NAMES = ('set rotation', 'set position', 'add rotation', 'add position', 'add child')


def f32(v):
    return struct.unpack('<I', struct.pack('<f', v))[0]


def trees():
    yield [(1, -1, 0, 0, 0)]  # no children: nothing marked
    yield [(1, -1, 0, 0, 0), (5, 0, 0, 0, 0), (5, 1, 0, 4, 2), (8, 1, 0x4, 0, 0),
           (5, 0, 0x2000000, 0, 0), (5, 4, 0, 0, 0)]  # a marked child's subtree is left alone
    rng = random.Random(9)
    for _ in range(200):
        n = rng.randint(1, MAX)
        t = [(1, -1, rng.choice((0, 4, 0x2000000)), rng.choice((0, 2, 0x20)), 0)]
        for i in range(1, n):
            cls = rng.choice((5, 5, 5, 0, 6, 8, 9))
            t.append((cls, rng.randrange(i), rng.choice((0, 0, 4, 0x2000000, 0x80)), rng.choice((0, 1, 0x40, 0x81)),
                      rng.choice((0, 0x20, 0x8, 0x1)) if cls == 5 else 0))
        yield t


def main():
    z = test_pm_zar.Emulated()
    u = z.cpu._uc
    nodes = [z.alloc(NODE) for _ in range(MAX + 1)]
    arrays = [z.alloc(4 * MAX) for _ in range(MAX)]
    for a in arrays:
        z.sizes[a] = 4 * MAX  # the link's realloc copies what it knows
    datas = [z.alloc(CAM) for _ in range(MAX + 1)]
    z.native_stub(LIST_PUSH, lambda c: 0, 0)
    bad = runs = 0
    for t in trees():
        for which, setter in enumerate((SET_ROT, SET_POS, ADD_ROT, ADD_POS, ADD_CHILD)):
            n = len(t)
            for i, (cls, parent, flags, w2c, d0) in enumerate(t):
                u.mem_write(nodes[i], bytes(NODE))
                u.mem_write(datas[i], bytes(CAM))
                u.mem_write(nodes[i] + 0x24, struct.pack('<I', flags))
                u.mem_write(nodes[i] + 0x2c, struct.pack('<I', w2c))
                u.mem_write(nodes[i] + 0x34, struct.pack('<I', cls))
                u.mem_write(nodes[i] + 0x38, struct.pack('<I', datas[i]))
                u.mem_write(datas[i], struct.pack('<I', d0))
            for i in range(n):
                ks = [nodes[j] for j in range(n) if t[j][1] == i]
                u.mem_write(nodes[i] + 0x56, struct.pack('<h', len(ks)))
                u.mem_write(nodes[i] + 0x5c, struct.pack('<I', arrays[i]))
                if ks:
                    u.mem_write(arrays[i], struct.pack(f'<{len(ks)}I', *ks))
            add = nodes[MAX]
            u.mem_write(add, bytes(NODE))
            u.mem_write(datas[MAX], bytes(CAM))
            u.mem_write(add + 0x34, struct.pack('<2I', 5, datas[MAX]))
            if which == 4:
                args = (add,)
            else:
                args = (f32(1.0), f32(2.0), f32(3.0)) if which & 1 else (f32(0.1), f32(0.2), f32(0.3))
            r = z.call(setter, args=(nodes[0],) + args)
            e = [r]
            for i, (cls, *_rest) in enumerate(t):
                fl, w2c = struct.unpack('<2I', bytes(u.mem_read(nodes[i] + 0x24, 4)) + bytes(u.mem_read(nodes[i] + 0x2c, 4)))
                d0 = struct.unpack('<I', bytes(u.mem_read(datas[i], 4)))[0] if cls == 5 else 0
                e += [fl, w2c, d0]
            e += list(struct.unpack('<2I', bytes(u.mem_read(add + 0x24, 4)) + bytes(u.mem_read(add + 0x2c, 4))))
            e.append(struct.unpack('<I', bytes(u.mem_read(datas[MAX], 4)))[0])
            spec = (ctypes.c_uint32 * (5 * n))(*[v & 0xffffffff for row in t for v in row])
            pr = probe.probe_pm_camera_moved(n, spec, which, OUT)
            p = [pr] + list(OUT[:3 * n + 3])
            runs += 1
            if e != p:
                bad += 1
                if bad <= 5:
                    print(f'FAIL {NAMES[which]} tree {t}\n  exe  {[hex(v) for v in e]}\n'
                          f'  port {[hex(v) for v in p]}')
    print(f'{runs} calls, {bad} differ')
    sys.stdout.flush()
    os._exit(1 if bad else 0)


if __name__ == '__main__':
    main()
