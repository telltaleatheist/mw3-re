"""PM's aim ray (0x00445c60) boxes a contact through its object's vt+0x48 (pm-tests 37): the slot read from
Pirate's Moon's Mech3.exe for each entity class our port tells apart (src/mech3/entity_classes.h, MW3's vtables
mapped to PM's by pm-diff/vtables.json). 0x00402250 returns the object itself (it is boxed), 0x00406ce0 none (its
node keeps its mesh test): app_sensors.cpp's hud_object_aim_entity is this table.

    .venv/bin/python -P tests/differential/test_pm_aim_entities.py
"""
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
EXE = ROOT / 'game' / 'pm' / 'Mech3.exe'
VTABLES = ROOT / 'analysis-output' / 'pm-diff' / 'vtables.json'
SELF, NONE = 0x402250, 0x406ce0
# (class, MW3's vtable, what PM's vt+0x48 is)
CLASSES = [
    ('mech', 0x5bcef8, SELF), ('net_mech', 0x5c3508, SELF), ('vehicle', 0x5c88e8, SELF), ('stealth object', 0x5c8460, SELF),
    ('field_base', 0x5c23d8, SELF), ('turret', 0x5c8770, NONE), ('target', 0x5ccb90, NONE), ('trap', 0x5ccc80, NONE),
]


def reader(path):
    d = path.read_bytes()
    pe = struct.unpack_from('<I', d, 0x3c)[0]
    count, opt = struct.unpack_from('<H', d, pe + 6)[0], struct.unpack_from('<H', d, pe + 20)[0]
    base = struct.unpack_from('<I', d, pe + 24 + 28)[0]
    secs = [struct.unpack_from('<8sIIII', d, pe + 24 + opt + 40 * i) for i in range(count)]

    def u32(va):
        for _, vsize, vaddr, rsize, raw in secs:
            if vaddr <= va - base < vaddr + max(vsize, rsize):
                return struct.unpack_from('<I', d, raw + va - base - vaddr)[0]
        raise ValueError(hex(va))
    return u32


def main():
    if not EXE.exists() or not VTABLES.exists():
        print('skipped: no game/pm or pm-diff')
        return 0
    u32, vt = reader(EXE), json.loads(VTABLES.read_text())
    bad = 0
    for name, mw3, want in CLASSES:
        pm = int(vt[hex(mw3)], 16)
        got = u32(pm + 0x48)
        ok = got == want
        bad += not ok
        print(f'{"ok  " if ok else "FAIL"} {name}: vtable {hex(pm)}, vt+0x48 {hex(got)} ({"itself" if got == SELF else "none" if got == NONE else "?"})')
    print(f'{len(CLASSES)} classes, {bad} wrong')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
