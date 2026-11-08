"""Pirate's Moon's GfxFlags setter 0x40b270 (MW3's 0x40d430, pm-tests item 20): besides the
flags and the sunlight node, PM's LIGHT MAPS level [0x8ccc64] from bits 17 and 18 (0, 1, 2,
both -1: no light maps on models), left as it was for flags of 0. The port against the exe
for random flags and starting levels; the sunlight node is not found on either side.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_gfx_flags.py

BREAK=1 swaps the port's bits 17 and 18: it must fail.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_pm_zar  # noqa: E402

SET, FIND, LEVEL, WHICH, HW_SLOT, SW_SLOT = 0x40b270, 0x50b920, 0x8ccc64, 0x618788, 0x6187a0, 0x61879c


def main():
    brk = int(os.environ.get('BREAK', '0'))
    lib = test_pm_zar.load_pm_probe()
    lib.probe_pm_gfx_flags.argtypes = [ctypes.c_int32, ctypes.c_int32]
    lib.probe_pm_gfx_flags.restype = ctypes.c_int32
    z = test_pm_zar.Emulated()
    w = lambda a, v: z.cpu._uc.mem_write(a, struct.pack('<i', v))  # noqa: E731
    z.native_stub(FIND, lambda c: 0)  # no "sunlight" node
    w(HW_SLOT, z.alloc(4)), w(SW_SLOT, z.alloc(4)), w(WHICH, 1)
    rng = random.Random(20)
    cases = [0, 0x1f, 0x20000, 0x40000, 0x60000, 0x6001f] + [rng.getrandbits(32) & 0x7ffff for _ in range(200)]
    bad = 0
    for flags in cases:
        before = rng.choice([-1, 0, 1, 2])
        w(LEVEL, before)
        z.call(SET, args=(flags,))
        theirs = struct.unpack('<i', struct.pack('<I', z.r32(LEVEL)))[0]
        f = flags
        if brk == 1:
            f = (f & ~0x60000) | ((f >> 1) & 0x20000) | ((f << 1) & 0x40000)
        mine = lib.probe_pm_gfx_flags(f, before)
        if mine != theirs:
            bad += 1
            print(f'FAIL flags {flags:#x} from {before}: port {mine}, exe {theirs}')
    if bad:
        print(f'{bad} of {len(cases)} differ')
        sys.stdout.flush(); os._exit(1)
    print(f'ok: {len(cases)} flags, the LIGHT MAPS level identical')
    sys.stdout.flush(); os._exit(0)


if __name__ == '__main__':
    main()
