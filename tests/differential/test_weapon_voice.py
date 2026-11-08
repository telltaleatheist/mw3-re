"""A critical's destroyed voice 0x4853d0 (pm-tests item 22): the weapon's family from the
table at 0x5e64f8 (0x4853c0), its sound [0x609400 + 4 x family] played through 0x4e6d80
with no place, volume 1, no velocity, fade -1. The port's weapon_family against the exe for
every weapon of the table; the call's arguments as the hook passes them (app_weapons).

    cmake --build build --target mw3probe && .venv/bin/python -P tests/differential/test_weapon_voice.py

BREAK=1 moves one family (the LB-X 2's, 0, to 1): it must fail.
"""
import ctypes
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_weiler import Emulated, probe  # noqa: E402

probe.probe_weapon_family.argtypes = [ctypes.c_int32]
probe.probe_weapon_family.restype = ctypes.c_int32
VOICE, PLAY, TABLE = 0x4853d0, 0x4e6d80, 0x609400
WEAPONS = 37  # the exe's table; the rebuild's 38th (the plain PPC) is family 10, snd_wep_ppc


def main():
    brk = int(os.environ.get('BREAK', '0'))
    emu = Emulated()
    cpu = emu.cpu
    played = []
    cpu.stub_function(PLAY, lambda c: played.append((c.ecx, c.edx, c.arg(0), c.arg(1), c.arg(2))) or 0, 0xc)
    for f in range(15):
        cpu.write(TABLE + 4 * f, struct.pack('<I', 0x1000 + f if f else 0))
    bad = 0
    args = set()
    for i in range(WEAPONS):
        played.clear()
        emu.call(VOICE, ecx=i)
        sound, pos, volume, vel, fade = played[0]
        args.add((pos, volume, vel, fade))
        theirs = sound - 0x1000 if sound else 0
        mine = probe.probe_weapon_family(i)
        if brk == 1 and i == 13:
            mine = 1
        if mine != theirs:
            bad += 1
            print(f'FAIL weapon {i}: family port {mine}, exe {theirs}')
    one, minus_one = struct.unpack('<I', struct.pack('<f', 1.0))[0], struct.unpack('<I', struct.pack('<f', -1.0))[0]
    if args != {(0, one, 0, minus_one)}:
        bad += 1
        print(f'FAIL the play\'s arguments: {args}')
    if bad:
        print(f'{bad} differ')
        sys.stdout.flush(); os._exit(1)
    print(f'ok: {WEAPONS} weapons\' families identical; played with no place, volume 1, no velocity, fade -1')
    sys.stdout.flush(); os._exit(0)


if __name__ == '__main__':
    main()
