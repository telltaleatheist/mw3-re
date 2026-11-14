"""Pirate's Moon's HUD weapon name 0x486a50 (the weapon line 0x447840 names the weapon by it) against the port's
table (mech_names.cpp kWeapons, PM's 0x608818: weapon_name_message), on PM's exe: for an index 0..0x37 the text of
the row's message (0x596e50, stubbed to answer its id), for any other "noweapon".

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_weapon_name.py
"""
import ctypes
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_pm_zar  # noqa: E402
from original import ROOT  # noqa: E402

NAME, TEXT, NOWEAPON = 0x486a50, 0x596e50, 0x6094d4

probe = ctypes.CDLL(str(next((ROOT / 'build').glob('libpmprobe.*'))))
probe.probe_pm_weapon_name.argtypes = [ctypes.c_int32]
probe.probe_pm_weapon_name.restype = ctypes.c_int32


def main():
    z = test_pm_zar.Emulated()
    cell = z.alloc(0x100)
    ids = {}

    def text(c):
        a = cell + 4 * (len(ids) % 60)
        ids[a] = c.arg(0)
        return a
    z.native_stub(TEXT, text, 0)
    bad = 0
    for i in range(-3, 0x42):
        ids.clear()
        r = z.call(NAME, args=(i & 0xffffffff,))
        e = -1 if r == NOWEAPON else ids.get(r, 'other')
        p = probe.probe_pm_weapon_name(i)
        if e != p:
            bad += 1
            print(f'FAIL index {i}: exe {e} port {p}')
    print(f'{0x42 + 3} indexes (rows 0..0x37 by message, the rest noweapon), {bad} differ')
    sys.stdout.flush()
    os._exit(1 if bad else 0)


if __name__ == '__main__':
    main()
