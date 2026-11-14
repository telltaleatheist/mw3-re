"""Pirate's Moon's top-down cameras against the port (tests/probe_pm_top_cameras.cpp): the mech's god camera
(0x4311e0, the port's mech3::make_cameras) is 120 m away and pitched by -1.413716 rad (-81 degrees), MW3's
150 m and -pi/2; a vehicle's above camera (0x4b0790, vehicle_cameras_make) keeps 150 m and takes the same pitch.
The distances and angles are read from PM's own instructions (0x4314ee / 0x4314f3, 0x4b08bf / 0x4b08c4) and the
rotations made by PM's quat_pitch 0x53ea70 in the emulator; MW3's table must still give MW3's.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_top_cameras.py

Compared: each camera's distance and rotation (w, x, y, z), bit for bit.
"""
import ctypes
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_pm_zar  # noqa: E402
from original import ROOT  # noqa: E402

QUAT_PITCH = 0x53ea70
GOD_PITCH, GOD_DIST, ABOVE_PITCH, ABOVE_DIST = 0x4314ee + 1, 0x4314f3 + 3, 0x4b08bf + 1, 0x4b08c4 + 3
MW3_PITCH, MW3_DIST = 0xbfc90fd8, 0x43160000

probe = ctypes.CDLL(str(next((ROOT / 'build').glob('libpmprobe.*'))))
probe.probe_pm_top_cameras.argtypes = [ctypes.c_int, ctypes.c_void_p]
OUT = (ctypes.c_uint32 * 10)()


def main():
    z = test_pm_zar.Emulated()
    u = z.cpu._uc
    word = lambda a: struct.unpack('<I', bytes(u.mem_read(a, 4)))[0]  # noqa: E731
    q = z.alloc(16)

    def pitch(bits):
        u.mem_write(q, bytes(16))
        z.call(QUAT_PITCH, args=(bits, q))
        return list(struct.unpack('<4I', bytes(u.mem_read(q, 16))))

    want = {1: [word(GOD_DIST)] + pitch(word(GOD_PITCH)) + [word(ABOVE_DIST)] + pitch(word(ABOVE_PITCH)),
            0: [MW3_DIST] + pitch(MW3_PITCH) + [MW3_DIST] + pitch(MW3_PITCH)}
    bad = 0
    for pm in (1, 0):
        r = probe.probe_pm_top_cameras(pm, OUT)
        got = list(OUT)
        name = 'PM' if pm else "MW3's table"
        f = lambda v: struct.unpack('<f', struct.pack('<I', v))[0]  # noqa: E731
        print(f'{name}: god {f(got[0]):g} m, above {f(got[5]):g} m; rotation w {f(got[1]):.6f} x {f(got[2]):.6f}')
        if r or got != want[pm]:
            bad += 1
            print(f'FAIL {name}\n  want {[hex(v) for v in want[pm]]}\n  port {[hex(v) for v in got]} ({r})')
    print(f'2 tables, {bad} differ')
    sys.stdout.flush()
    os._exit(1 if bad else 0)


if __name__ == '__main__':
    main()
