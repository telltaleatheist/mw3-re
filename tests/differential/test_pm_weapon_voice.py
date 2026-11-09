"""PM's destroyed voice 0x4875b0 (MW3's 0x4853d0, test_weapon_voice.py): the weapon's family from
its row in the table at 0x608818 (0x4875a0, the word at 0x608820), its sound [0x62d438 + 4 x family]
played through 0x4f8b10 (cdecl: the sound, volume 1, no place, no velocity, fade -1). The port's
weapon_family against the exe for all 56 weapons and out of range.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_weapon_voice.py

BREAK=1 moves one family (the LB-X 2's, 0, to 1): it must fail.
"""
import ctypes
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import pm_floors  # noqa: E402,F401
import test_pm_zar  # noqa: E402

VOICE, PLAY, TABLE = 0x4875b0, 0x4f8b10, 0x62d438
WEAPONS = 56
LBX2 = 19


def main():
    brk = int(os.environ.get('BREAK', '0'))
    p = test_pm_zar.load_pm_probe()
    p.probe_pm_weapon_family.argtypes = [ctypes.c_int]
    p.probe_pm_weapon_family.restype = ctypes.c_int
    e = test_pm_zar.Emulated()
    played = []
    e.native_stub(PLAY, lambda c: played.append((c.arg(0), c.arg(1), c.arg(2), c.arg(3), c.arg(4))) or 0, 0)
    for f in range(15):
        e.cpu._uc.mem_write(TABLE + 4 * f, struct.pack('<I', 0x1000 + f if f else 0))
    bad = 0
    args = set()
    for i in range(WEAPONS):
        played.clear()
        e.call(VOICE, args=[i])
        sound, volume, a, b, fade = played[0]
        args.add((volume, a, b, fade))
        theirs = sound - 0x1000 if sound else 0
        mine = p.probe_pm_weapon_family(i)
        if brk == 1 and i == LBX2:
            mine = 1
        if mine != theirs:
            bad += 1
            print(f'FAIL weapon {i}: family port {mine}, exe {theirs}')
    for i in (-1, WEAPONS, 1000):  # the exe reads past its table; the port says none
        if p.probe_pm_weapon_family(i) != 0:
            bad += 1
            print(f'FAIL weapon {i}: out of range, family {p.probe_pm_weapon_family(i)}')
    one, minus_one = struct.unpack('<I', struct.pack('<f', 1.0))[0], struct.unpack('<I', struct.pack('<f', -1.0))[0]
    if args != {(one, 0, 0, minus_one)}:
        bad += 1
        print(f'FAIL the play\'s arguments: {args}')
    if bad:
        print(f'{bad} differ')
        sys.stdout.flush(); os._exit(1)
    print(f'ok: {WEAPONS} weapons\' families identical; played with volume 1, no place, no velocity, fade -1')
    sys.stdout.flush(); os._exit(0)


if __name__ == '__main__':
    main()
