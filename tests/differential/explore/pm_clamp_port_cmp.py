"""EXPLORE (2026-11-14): found that PM's clamp is its own (straight ahead at the limit for a vertical way, where MW3's
scaled the heading); since PM's direction limits are ported (mech3/dir_limits_pm) it passes, 0 of 3,000 differ. The
test is test_pm_dir_limits.py (docs/pm-tests.md, The direction limits).

Pirate's Moon's turret limit clamp 0x4a71f0 (with its scaling 0x4a7170; the vehicles' and turrets' vt+0x90 0x4b10b0
/ 0x4aaf80 call it) against the port's TurretLimits::clamp (tests/probe_pm_clamp.cpp), on PM's exe. A direction past
the pitch limit takes the limit's sine and its heading's own length, hypot(x, z), for the rest (MW3's 0x4a41b0 had
hypot(x, y), a FIX (bug) on main; PM's own here, the bug fixes on or off). The limits made by 0x4a7390 (gun pitch
lo..hi, full yaw), as the explore script pm_clamp_cmp.py did; the x87 at 53 bits.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/explore/pm_clamp_port_cmp.py [cases]
"""
import ctypes
import math
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from original import Original, ROOT  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

probe = ctypes.CDLL(str(next((ROOT / 'build').glob('libpmprobe.*'))))
F3 = ctypes.c_float * 3
probe.probe_pm_clamp.argtypes = [ctypes.c_float, ctypes.c_float, F3, F3]


def fb(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def main():
    cases = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    pm = Original(exe='pm')
    pm.tr = lambda a, size=1: a
    pm._uc.reg_write  # noqa: B018
    po, pi_, pout = pm.alloc(0x100), pm.alloc(16), pm.alloc(16)
    r = random.Random(0x4a71)
    bad = past = 0
    for n in range(cases):
        if r.random() < 0.3:
            lo, hi = -0.259, 0.5
        else:
            lo, hi = sorted(r.uniform(-1.5, 1.5) for _ in range(2))
        th, ph = r.uniform(-math.pi, math.pi), r.uniform(-1.5, 1.5)
        s = r.choice([1.0, r.uniform(0.3, 3)])
        d = (s * math.cos(ph) * math.sin(th), s * math.sin(ph), s * math.cos(ph) * math.cos(th))
        if r.random() < 0.03:
            d = (0.0, d[1], 0.0)  # straight up or down: no heading
        d = struct.unpack('<3f', struct.pack('<3f', *d))
        lo, hi = struct.unpack('<2f', struct.pack('<2f', lo, hi))
        pm.write(po, b'\0' * 0x100)
        pm.call(0x004a7390, ecx=po, stack_args=[fb(lo), fb(hi), fb(-2 * math.pi), fb(2 * math.pi)])
        pm.write(pi_, struct.pack('<3f', *d))
        pm.write(pout, b'\0' * 12)
        pm.call(0x004a71f0, ecx=po, stack_args=[pi_, pout])
        er = pm._uc.reg_read(UC_X86_REG_EAX) & 0xff
        ev = struct.unpack('<3f', pm.read(pout, 12))
        out = F3()
        pr = probe.probe_pm_clamp(lo, hi, F3(*d), out)
        pv = tuple(out)
        past += not er
        same = bool(er) == bool(pr) and all(abs(a - b) <= 2e-7 * max(1.0, abs(a)) for a, b in zip(ev, pv))
        if not same:
            bad += 1
            if bad <= 5:
                print(f'FAIL case {n}: lo {lo} hi {hi} d {d}\n  exe  {er} {ev}\n  port {pr} {pv}')
    print(f'{cases} directions ({past} past a limit), {bad} differ')
    sys.stdout.flush()
    os._exit(1 if bad else 0)


if __name__ == '__main__':
    main()
