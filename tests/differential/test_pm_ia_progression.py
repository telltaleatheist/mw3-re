"""Pirate's Moon's Instant Action waves (pm-tests item 24): ia_progression.zrd's block read by 0x40c070 into
the game's +0x8c (a vector of waves, each a vector of (mech, loadout)), and the accessors 0x40c260 (the last
wave), 0x40c280 (a wave's slots), 0x40c2c0 / 0x40c300 (a slot's mech and loadout), against PM's exe on the
shipped file: each world's block, "default" and none. The accessors are asked of every wave and slot and of
the ones just past them; past a wave's slots the exe reads on into the heap (FIX (bug): -1 and 0 here), so
only the wave's own are compared there.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_ia_progression.py

BREAK=1 reads each pair's loadout from the wrong value (the port's side): it must fail.
"""
import ctypes
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_pm_zar  # noqa: E402
import test_pm_zreader as zr  # noqa: E402

READ, LAST, SLOTS, MECH, CONFIG = 0x40c070, 0x40c260, 0x40c280, 0x40c2c0, 0x40c300
BLOCKS = ['default', 'c1', 'c2', 'c3', 'c4', 'c5', 'nowhere', None]


def s32(v):
    return struct.unpack('<i', struct.pack('<I', v & 0xffffffff))[0]


def main():
    brk = int(os.environ.get('BREAK', '0'))
    rel = 'zbd/reader.zbd'
    data = (zr.PM / rel).read_bytes()
    port = zr.Port()
    lib = port.p
    lib.probe_pm_progression.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_int32), ctypes.c_int]
    lib.probe_pm_wave_query.argtypes = [ctypes.c_int32, ctypes.c_int32, ctypes.POINTER(ctypes.c_int32)]
    if port.load(rel, 'ia_progression.zrd') is None:
        raise SystemExit('the port finds no ia_progression.zrd')
    emu = zr.Emulated()
    emu.files[test_pm_zar.key(rel)] = bytearray(data)
    if not emu.call(zr.OPEN_ARCHIVE, args=[emu.string(rel.replace('/', '\\')), 1]):
        raise SystemExit('PM does not open reader.zbd')
    root = emu.call(zr.LOAD, args=[emu.string('ia_progression.zrd'), 0, 0])
    if not root:
        raise SystemExit('PM finds no ia_progression.zrd')
    bad = waves = slots = 0
    for name in BLOCKS:
        block = emu.call(zr.FIND, args=[root, emu.string(name)]) if name else 0
        game = emu.alloc(0x200)
        emu.call(READ, ecx=game, args=[block])
        first, end = emu.r32(game + 0x8c), emu.r32(game + 0x90)
        exe = [(end - first) // 16 if first else 0]
        for w in range(exe[0]):
            b, e = emu.r32(first + w * 16 + 4), emu.r32(first + w * 16 + 8)
            n = (e - b) // 8 if b else 0
            exe.append(n)
            for i in range(n):
                exe += [s32(emu.r32(b + 8 * i)), s32(emu.r32(b + 8 * i + 4))]
        out = (ctypes.c_int32 * 4096)()
        n = lib.probe_pm_progression(name.encode() if name else None, out, 4096)
        mine = list(out[:n])
        if brk == 1 and len(mine) > 3:
            mine[3] += 1
        if mine != exe:
            bad += 1
            print(f'FAIL block {name}: port {mine[:12]}.., exe {exe[:12]}..')
            continue
        last = s32(emu.call(LAST, ecx=game))
        q = (ctypes.c_int32 * 4)()
        for w in range(-1, exe[0] + 1):
            n_exe = s32(emu.call(SLOTS, ecx=game, args=[w & 0xffffffff]))
            for i in range(0, max(n_exe, 0) + 1):
                lib.probe_pm_wave_query(w, i, q)
                want = [last, n_exe]
                if 0 <= w < exe[0] and i < n_exe:
                    want += [s32(emu.call(MECH, ecx=game, args=[w, i])), s32(emu.call(CONFIG, ecx=game, args=[w, i]))]
                    got = list(q)
                    slots += 1
                elif not 0 <= w < exe[0]:  # a wave out of range: the exe answers -1 and 0 itself
                    want += [s32(emu.call(MECH, ecx=game, args=[w & 0xffffffff, i])), s32(emu.call(CONFIG, ecx=game, args=[w & 0xffffffff, i]))]
                    got = list(q)
                else:
                    got = list(q[:2])
                if got != want:
                    bad += 1
                    print(f'FAIL block {name} wave {w} slot {i}: port {got}, exe {want}')
            waves += 1
    if bad:
        print(f'{bad} differ')
        sys.stdout.flush(); os._exit(1)
    print(f'ok: {len(BLOCKS)} blocks read identically; {waves} waves\' accessors and {slots} slots agree')
    sys.stdout.flush(); os._exit(0)


if __name__ == '__main__':
    main()
