"""Pirate's Moon's object-detail setter 0x40b120 against the port's mech3::set_object_lod
(tests/probe_pm_object_lod.cpp), on PM's exe: the level kept in the software or hardware entry ([0x618788] picks
[0x6187a4] or [0x6187a8]); the camera's LOD scale (0x5073f0, when there is a camera, 0x40b490) and the mechs'
(0x430800): 1 and 1, 0.75 and 0.75, and at level 2 PM's 0.2 (MW3's 0.4) and 0; any other level leaves them 1.
The level-2 flag [0x62cc58] and the software prep's switch (0x541a30) are printed, not compared (the port reads
object_lod() == 2 for the first; the second only the software path reads).

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_object_lod.py
"""
import ctypes
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_pm_zar  # noqa: E402
from original import ROOT  # noqa: E402

SET, CAMERA, CAM_SCALE, MECH_SCALE = 0x40b120, 0x40b490, 0x5073f0, 0x430800
HW, SW_SLOT, HW_SLOT, LOW, SOFT = 0x618788, 0x6187a4, 0x6187a8, 0x62cc58, 0x6112e4

probe = ctypes.CDLL(str(next((ROOT / 'build').glob('libpmprobe.*'))))
probe.probe_pm_object_lod.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_uint32, ctypes.c_void_p]
OUT = (ctypes.c_uint32 * 16)()


def main():
    z = test_pm_zar.Emulated()
    u = z.cpu._uc
    cells = z.alloc(8)
    u.mem_write(SW_SLOT, struct.pack('<I', cells))
    u.mem_write(HW_SLOT, struct.pack('<I', cells + 4))
    st = {}
    z.native_stub(CAMERA, lambda c: st['cam'], 0)
    z.native_stub(CAM_SCALE, lambda c: st['log'].extend([1, c.arg(0), c.arg(1)]) or 0, 0)
    z.native_stub(MECH_SCALE, lambda c: st['log'].extend([2, c.arg(0)]) or 0, 0)
    bad = runs = 0
    for level in (0, 1, 2, 3, -1, 7):
        for hw in (0, 1):
            for cam in (0, 0x5150):
                st.update(cam=cam, log=[])
                u.mem_write(cells, struct.pack('<2i', 7, 7))
                u.mem_write(HW, struct.pack('<I', hw))
                z.call(SET, args=(level & 0xffffffff,))
                e = list(struct.unpack('<2I', bytes(u.mem_read(cells, 8)))) + st['log']
                k = probe.probe_pm_object_lod(level, hw, cam, OUT)
                p = list(OUT[:k])
                runs += 1
                flags = struct.unpack('<2I', bytes(u.mem_read(LOW, 4)) + bytes(u.mem_read(SOFT, 4)))
                if e != p:
                    bad += 1
                    print(f'FAIL level {level} hw {hw} cam {cam:#x}\n  exe  {e}\n  port {p}')
                elif cam and hw == 0:
                    scale = struct.unpack('<f', struct.pack('<I', e[4]))[0] if len(e) > 4 else None
                    print(f'level {level}: camera scale {scale}, [0x62cc58] {flags[0]}, [0x6112e4] {flags[1]}')
    print(f'{runs} settings, {bad} differ')
    sys.stdout.flush()
    os._exit(1 if bad else 0)


if __name__ == '__main__':
    main()
