"""Pirate's Moon's MipBias 0x598230 (common.gw's script command) against the port's zvid::set_mip_bias
(tests/probe_pm_mip_bias.cpp): the bias kept in [0xb06164], clamped to -1..1 ([0x5deda0] 1.0, [0x5dfa00] -1.0),
with the state's dirty bit 4 set ([0xb06158]); the port holds it in device_state().mip_bias (not applied: the GPU
path's textures have one mip level, fixes.md).

    cmake --build build --target pmprobe && MW3_EXE=pm .venv/bin/python -P tests/differential/test_pm_mip_bias.py

Compared bit for bit over edges, random values, infinities and a NaN.
"""
import ctypes
import math
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_pm_zar  # noqa: E402
from original import ROOT  # noqa: E402

SET, KEPT, DIRTY = 0x598230, 0xb06164, 0xb06158

probe = ctypes.CDLL(str(next((ROOT / 'build').glob('libpmprobe.*'))))
probe.probe_pm_mip_bias.argtypes = [ctypes.c_uint32]
probe.probe_pm_mip_bias.restype = ctypes.c_uint32


def bits(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


def main():
    z = test_pm_zar.Emulated()
    u = z.cpu._uc
    rng = random.Random(4)
    values = [-1.0, 1.0, 0.0, -0.0, -1.0000001, 1.0000001, -2.0, 2.0, 0.5, -0.5, math.inf, -math.inf, math.nan]
    values += [rng.uniform(-3, 3) for _ in range(200)]
    bad = 0
    for v in values:
        b = bits(v)
        u.mem_write(KEPT, struct.pack('<I', 0x12345678))
        u.mem_write(DIRTY, struct.pack('<I', 0))
        z.call(SET, args=(b,))
        e = struct.unpack('<I', bytes(u.mem_read(KEPT, 4)))[0]
        dirty = struct.unpack('<I', bytes(u.mem_read(DIRTY, 4)))[0]
        p = probe.probe_pm_mip_bias(b)
        if e != p or dirty != 4:
            bad += 1
            if bad <= 5:
                print(f'FAIL {v!r}: exe {e:#010x} (dirty {dirty}), port {p:#010x}')
    print(f'{len(values)} values, {bad} differ (-1 kept as {struct.unpack("<f", struct.pack("<I", probe.probe_pm_mip_bias(bits(-1.0))))[0]})')
    sys.stdout.flush()
    os._exit(1 if bad else 0)


if __name__ == '__main__':
    main()
