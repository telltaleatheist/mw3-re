"""The player's equipment callouts (docs/port-audit.md, BROKEN): 0x485390 plays the voice of an equipment
kind, its index from the table at 0x5e6870 (0x485360) into [0x608fc0 + 4 x index], through 0x4e6d80 with no
place, volume 1, no velocity, fade -1; 0x485370 plays [0x608fc0 + 4 x index] itself (the engine hit, index 6).
The port's equip_voice_index against the exe for every kind of the table, and both calls' arguments as the
hooks pass them (app_weapons.cpp).

    cmake --build build --target mw3probe && .venv/bin/python -P tests/differential/test_equip_voice.py

BREAK=1 moves one kind's voice (the C3 computer's, 7, to 8): it must fail.
"""
import ctypes
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_weiler import Emulated, probe  # noqa: E402

probe.probe_equip_voice_index.argtypes = [ctypes.c_int32]
probe.probe_equip_voice_index.restype = ctypes.c_int32
EQUIP, VOICE, PLAY, TABLE = 0x485390, 0x485370, 0x4e6d80, 0x608fc0
KINDS = 15  # the exe's table, 0x5e6870 .. 0x5e69e4


def main():
    brk = int(os.environ.get('BREAK', '0'))
    emu = Emulated()
    cpu = emu.cpu
    played = []
    cpu.stub_function(PLAY, lambda c: played.append((c.ecx, c.edx, c.arg(0), c.arg(1), c.arg(2))) or 0, 0xc)
    for i in range(16):
        cpu.write(TABLE + 4 * i, struct.pack('<I', 0x1000 + i))
    bad = 0
    args = set()
    for k in range(KINDS):
        played.clear()
        emu.call(EQUIP, ecx=k)
        sound, pos, volume, vel, fade = played[0]
        args.add((pos, volume, vel, fade))
        mine = probe.probe_equip_voice_index(k)
        if brk == 1 and k == 3:
            mine = 8
        if 0x1000 + mine != sound:
            bad += 1
            print(f'FAIL kind {k}: voice port {mine}, exe {sound - 0x1000}')
    for i in range(16):
        played.clear()
        emu.call(VOICE, ecx=i)
        sound, pos, volume, vel, fade = played[0]
        args.add((pos, volume, vel, fade))
        if sound != 0x1000 + i:
            bad += 1
            print(f'FAIL voice {i}: the exe played {sound - 0x1000}')
    one, minus_one = struct.unpack('<I', struct.pack('<f', 1.0))[0], struct.unpack('<I', struct.pack('<f', -1.0))[0]
    if args != {(0, one, 0, minus_one)}:
        bad += 1
        print(f'FAIL the play\'s arguments: {args}')
    if bad:
        print(f'{bad} differ')
        sys.stdout.flush(); os._exit(1)
    print(f'ok: {KINDS} equipment kinds\' voices identical; both play with no place, volume 1, no velocity, fade -1')
    sys.stdout.flush(); os._exit(0)


if __name__ == '__main__':
    main()
