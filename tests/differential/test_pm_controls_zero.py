"""Pirate's Moon's controls vt+0x1c against the port's zero_held (tests/probe_pm_controls_zero.cpp), on PM's exe: the
base's 0x428940 zeroes every input but the throttle (+0x10) and its target (+0x44), the joystick throttle (+0x40) and
the fire byte (+0x34) and wrap (+0x38) left; the player's 0x4289d0 also the look (+0x158..+0x164). The Core state's
resume from the pause (0x483de0, case 8) calls it on the player's controls.

Random values in the 16 words at +0x10..+0x4c and the look; the words with no float (+0x34, +0x38, +0x3c) checked
untouched by the exe and carried as they are by the port.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_controls_zero.py [cases]
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

BASE, PLAYER = 0x428940, 0x4289d0

probe = ctypes.CDLL(str(next((ROOT / 'build').glob('libpmprobe.*'))))
F20 = ctypes.c_float * 20
probe.probe_pm_controls_zero.argtypes = [ctypes.c_int, F20, F20]


def main():
    cases = int(sys.argv[1]) if len(sys.argv) > 1 else 2000
    e = test_pm_zar.Emulated()
    uc = e.cpu._uc
    obj = e.alloc(0x180)
    r = random.Random(0x4289)
    bad = 0
    for n in range(cases):
        player = n % 2
        vals = [r.choice([0.0, 1.0, -1.0, r.uniform(-2, 2)]) for _ in range(20)]
        for k in (9, 10, 11):  # +0x34, +0x38, +0x3c: not floats the port keeps
            vals[k] = float(r.randrange(1, 100))
        uc.mem_write(obj, bytes(0x180))
        uc.mem_write(obj + 0x10, struct.pack('<16f', *vals[:16]))
        uc.mem_write(obj + 0x158, struct.pack('<4f', *vals[16:]))
        e.call(PLAYER if player else BASE, ecx=obj)
        got = list(struct.unpack('<16f', bytes(uc.mem_read(obj + 0x10, 64)))) + list(struct.unpack('<4f', bytes(uc.mem_read(obj + 0x158, 16))))
        if not player:
            got[16:] = vals[16:]  # the base leaves the look; the port's object holds it apart
        out = F20()
        probe.probe_pm_controls_zero(player, F20(*vals), out)
        p = list(out)
        if not player:
            p[16:] = vals[16:]
        if [struct.pack('<f', x) for x in got] != [struct.pack('<f', x) for x in p]:
            bad += 1
            if bad <= 5:
                print(f'FAIL case {n} player {player}\n  in   {vals}\n  exe  {got}\n  port {p}')
    print(f'{cases} controls (half the player\'s), {bad} differ')
    sys.stdout.flush()
    os._exit(1 if bad else 0)


if __name__ == '__main__':
    main()
