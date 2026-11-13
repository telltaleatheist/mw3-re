"""zMath's 0x522170 (ECX the angles, EDX the out vector): pitch and yaw to the way they face,
(-cos p sin y, sin p, -cos p cos y), each sine and cosine stored as a float before the products.
The radar's wedge over the mission map takes the view camera's through it (docs/port-audit.md,
BROKEN), as the clouds' drift (mission_world.cpp) and an objective's direction do. The port's
zmth::look_dir against the exe on random angles, whole turns, the axes and large ones, at the
x87's 24 bits and at 53 (the sines and cosines are not affected by the precision; the products are
float by float). Compared bit for bit; a sine or cosine one ulp apart from the host's is counted
and allowed (the x87's fsin against libm), nothing more.

    cmake --build build --target mw3probe && .venv/bin/python -P tests/differential/test_look_dir.py [count]

BREAK=1 swaps the port's x and z: it must fail.
"""
import ctypes
import math
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_weiler import Emulated, probe  # noqa: E402

FN = 0x522170
probe.probe_look_dir.argtypes = [ctypes.POINTER(ctypes.c_float), ctypes.POINTER(ctypes.c_float)]
probe.probe_look_dir.restype = None


def ulps(a, b):
    ia, ib = struct.unpack('<i', struct.pack('<f', a))[0], struct.unpack('<i', struct.pack('<f', b))[0]
    if ia < 0:
        ia = -0x80000000 - ia
    if ib < 0:
        ib = -0x80000000 - ib
    return abs(ia - ib)


def main():
    brk = int(os.environ.get('BREAK', '0'))
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 20000
    rnd = random.Random(522170)
    cases = [(0.0, 0.0), (math.pi / 2, 0.0), (-math.pi / 2, 0.0), (0.0, math.pi), (0.0, -math.pi / 2),
             (1e-30, 1e-30), (100.0, -250.0), (1e6, 3e5), (2 * math.pi, 4 * math.pi)]
    cases += [(rnd.uniform(-math.pi, math.pi), rnd.uniform(-4 * math.pi, 4 * math.pi)) for _ in range(count)]
    emu = Emulated()
    cpu = emu.cpu
    bad = off1 = 0
    for cw in (0x007f, 0x027f):
        cpu.fpu_control(cw)
        for p, y in cases:
            emu.begin()
            a = emu.put(struct.pack('<3f', p, y, 0.0))
            o = emu.alloc(12)
            emu.call(FN, ecx=a, edx=o)
            theirs = struct.unpack('<3f', bytes(cpu.read(o, 12)))
            ang = (ctypes.c_float * 3)(p, y, 0.0)
            out = (ctypes.c_float * 3)()
            probe.probe_look_dir(ang, out)
            mine = list(out)
            if brk == 1:
                mine[0], mine[2] = mine[2], mine[0]
            d = max(ulps(m, t) for m, t in zip(mine, theirs))
            if d > 1:
                bad += 1
                if bad < 6:
                    print(f'FAIL cw {cw:#x} pitch {p!r} yaw {y!r}: port {mine}, exe {list(theirs)}')
            elif d == 1:
                off1 += 1
    n = 2 * len(cases)
    if bad:
        print(f'{bad} of {n} differ')
        sys.stdout.flush(); os._exit(1)
    print(f'ok: {n} directions (24 and 53 bits) identical, {off1} a sine or cosine one ulp off')
    sys.stdout.flush(); os._exit(0)


if __name__ == '__main__':
    main()
