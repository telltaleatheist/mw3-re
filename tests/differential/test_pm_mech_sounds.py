"""Pirate's Moon's mech data sounds 0x430a70 against the port's mech3::load_sounds with PM's table
(tests/probe_pm_mech_sounds.cpp): the 25 sound lookups (0x590a70, zsnd's find, stubbed and logged) in order and
the slot (+0x28 + 4 * slot) each lands in. PM's slot 24 is "mech_targeting" for every mech; MW3's table, run
through the same probe, still builds "mech_<name>".

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_mech_sounds.py
"""
import ctypes
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_pm_zar  # noqa: E402
from original import ROOT  # noqa: E402

LOAD, FIND = 0x430a70, 0x590a70

probe = ctypes.CDLL(str(next((ROOT / 'build').glob('libpmprobe.*'))))
probe.probe_pm_mech_sounds.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_void_p]


def main():
    z = test_pm_zar.Emulated()
    u = z.cpu._uc
    names = []

    def find(c):
        names.append(bytes(u.mem_read(c.arg(0), 64)).split(b'\0')[0].decode())
        return len(names)

    z.native_stub(FIND, find, 0)
    data = z.alloc(0x100)
    bad = 0
    buf = ctypes.create_string_buffer(4096)
    slots = (ctypes.c_uint32 * 25)()
    for mech in ('madcat', 'owens', 'elemental'):
        names.clear()
        u.mem_write(data, bytes(0x100))
        z.call(LOAD, ecx=data)
        e_slots = list(struct.unpack('<25I', bytes(u.mem_read(data + 0x28, 100))))
        e = (list(names), e_slots)
        n = probe.probe_pm_mech_sounds(mech.encode(), 1, buf, len(buf), slots)
        p = ([s.decode() for s in buf.raw[:n].split(b'\0') if s], list(slots))
        ok = e == p
        bad += not ok
        print(f'{mech}: {len(e[0])} lookups, slot 24 {e[0][e_slots[24] - 1] if e_slots[24] else None!r}'
              + ('' if ok else f'\n  FAIL\n  exe  {e}\n  port {p}'))
    n = probe.probe_pm_mech_sounds(b'madcat', 0, buf, len(buf), slots)
    mw3 = [s.decode() for s in buf.raw[:n].split(b'\0') if s]
    mw3_24 = mw3[slots[24] - 1] if slots[24] else None
    print(f"MW3's table: slot 24 {mw3_24!r}")
    bad += mw3_24 != 'mech_madcat'
    print(f'4 runs, {bad} differ')
    sys.stdout.flush()
    os._exit(1 if bad else 0)


if __name__ == '__main__':
    main()
