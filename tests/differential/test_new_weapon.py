"""New Weapon's definition (upgrades/editor/new_weapon, docs/editor.md, New Weapon) through the original's own
weapons.zrd reader 0x583880 in Unicorn: the shipped file with the dialog's definition appended (a projectile and a
beam copied, each with every main number changed, the techs and ammo varied) reads 50 definitions with no error, the
port's reader field for field the same (test_weapons_zrd.compare: every def, impact and WeaponType), the new one's
numbers as set, and the original's mech lab view of its own 37 weapons unchanged (docs/modding.md, Both ways). Then
mech3ax (its unzbd) reads the install's reader.zbd with that weapons.zrd in it: every other member the same, and
weapons.zrd the shipped one with the new weapon last in BALLISTICS.

    cmake --build build --target mw3probe && .venv/bin/python -P tests/differential/test_new_weapon.py

Breaks, by hand in new_weapon.cpp, each must fail: the count of BALLISTICS left as it was; DAMAGE written to the
copy's own definition; MASS's sphere pair written under clan.
Needs unzbd built in ../mech3ax (MECH3AX_UNZBD for another place); without it the mech3ax half is skipped.
"""
import ctypes
import json
import os
import struct
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_weapons_zrd as W  # noqa: E402

UNZBD = Path(os.environ.get('MECH3AX_UNZBD') or
             (W.ROOT / 'game').resolve().parent.parent / 'mech3ax' / 'target' / 'release' / 'unzbd')

W.probe.probe_new_weapon.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_char_p,
                                     ctypes.POINTER(ctypes.c_float), ctypes.POINTER(ctypes.c_int32), ctypes.c_int,
                                     ctypes.c_int, ctypes.c_char_p, ctypes.c_int]

CASES = [  # copy, name, (damage, heat, range, fire rate, velocity), (clan t, s, sphere t, s, rounds), tech, ammo
    ('ERPPC', 'ERPPC_B', (21.5, 0.5, 1000.0, 0.2, 800.0), (8, 3, 9, 4, 0), 3, 0),
    ('AC20', 'AC20_B', (8.0, 0.06, 300.0, 0.4, 450.0), (12, 8, 13, 9, 7), 2, 1),
    ('Laser_Large', 'Laser_Large_B', (9.0, 0.3, 600.0, 0.5, 1.0), (5, 2, 6, 3, 0), 1, 0),
]


def made(real, c):
    copy, name, nums, ints, tech, ammo = c
    out = ctypes.create_string_buffer(len(real) + 65536)
    n = W.probe.probe_new_weapon(real, len(real), copy.encode(), name.encode(), (ctypes.c_float * 5)(*nums),
                                 (ctypes.c_int32 * 5)(*ints), tech, ammo, out, len(out))
    assert n > 0, (copy, n)
    return out.raw[:n]


def with_entry(archive, name, data):
    """An MW3 archive (version 1) with entry `name` holding `data`, the others' bytes and table rows as they were."""
    version, count = struct.unpack_from('<II', archive, len(archive) - 8)
    assert version == 1
    table = len(archive) - 8 - count * 0x94
    out, rows = bytearray(), []
    for i in range(count):
        row = bytearray(archive[table + 0x94 * i:table + 0x94 * (i + 1)])
        start, length = struct.unpack_from('<II', row, 0)
        body = data if row[8:0x48].split(b'\0')[0].decode('latin-1').lower() == name else archive[start:start + length]
        struct.pack_into('<II', row, 0, len(out), len(body))
        out += body
        rows.append(bytes(row))
    return bytes(out) + b''.join(rows) + struct.pack('<II', 1, count)


def extract(zbd, out):
    subprocess.run([str(UNZBD), 'mw', 'reader', str(zbd), str(out)], check=True, capture_output=True)
    with zipfile.ZipFile(out) as z:
        return {n: json.loads(z.read(n)) for n in z.namelist()}


def mech3ax_check(real, data, name):
    archive = W.READER.read_bytes()
    with tempfile.TemporaryDirectory() as t:
        a, b = Path(t) / 'a.zbd', Path(t) / 'b.zbd'
        a.write_bytes(with_entry(archive, 'weapons.zrd', real))
        b.write_bytes(with_entry(archive, 'weapons.zrd', data))
        assert a.read_bytes() == archive  # rewritten as it was: the same file
        x, y = extract(a, Path(t) / 'a.zip'), extract(b, Path(t) / 'b.zip')
    others = all(x[k] == y.get(k) for k in x if k != 'weapons.zrd.json') and set(x) == set(y)
    bx, by = x['weapons.zrd.json'][0][3], y['weapons.zrd.json'][0][3]
    return others and by[:-2] == bx and by[-2] == name


def main():
    emu = W.Emulated()
    real = W.real_bytes()
    crater = [0x1234 * (i + 1) for i in range(10)]
    rc, nd, _ = W.compare(emu, real, crater, 1, 1, 'weapons.zrd as shipped')
    assert rc == 0 and nd == 49
    shipped_lab = [emu.lab(w) for w in range(37)]
    bad = 0
    for c in CASES:
        data = made(real, c)
        rc, nd, _ = W.compare(emu, data, crater, 1, 1, "weapons.zrd with New Weapon's " + c[1])
        defs = emu.defs()
        img = defs[-1][0] if defs else b''
        damage, rng = struct.unpack_from('<f', img, 0x48)[0], struct.unpack_from('<f', img, 0x1c)[0]
        period = struct.unpack_from('<f', img, 0x14)[0]
        lab = [emu.lab(w) for w in range(37)]
        # The WeaponType's MASS (+0x1c clan tons, slots, present; +0x28 the sphere's; +0x34 tons a round).
        t = defs[-1][2] if defs else bytes(0xc8)
        clan = (struct.unpack_from('<f', t, 0x1c)[0], struct.unpack_from('<i', t, 0x20)[0], t[0x24])
        sphere = (struct.unpack_from('<f', t, 0x28)[0], struct.unpack_from('<i', t, 0x2c)[0], t[0x30])
        per_round = struct.unpack_from('<f', t, 0x34)[0]
        tech, ints = c[4], c[3]
        mass_ok = (not tech & 1 or clan == (ints[0], ints[1], 1)) and (not tech & 2 or sphere == (ints[2], ints[3], 1))
        mass_ok = mass_ok and (not c[5] or abs(per_round - 1 / ints[4]) < 1e-6)
        ok = (rc == 0 and nd == 50 and abs(damage - c[2][0]) < 1e-6 and abs(rng - c[2][2]) < 1e-3
              and abs(period - 1 / c[2][3]) < 1e-5 and lab == shipped_lab and mass_ok)
        print('%-14s from %-12s: %d defs, damage %.2f, range %.0f, period %.3f, clan %r, sphere %r, lab of 37 %s -> %s'
              % (c[1], c[0], nd, damage, rng, period, clan, sphere, 'same' if lab == shipped_lab else 'CHANGED',
                 'ok' if ok else 'BAD'))
        if UNZBD.exists():
            mx = mech3ax_check(real, data, c[1])
            print('  mech3ax: %s' % ('the other members the same, %s last in BALLISTICS' % c[1] if mx else 'DIFFERS'))
            bad += not mx
        bad += not ok
    if bad:
        print('FAIL: %d of %d' % (bad, len(CASES)))
        return 1
    print('ok: %d new weapons read by the original as by the port, the 37 unchanged%s' % (
        len(CASES), '; mech3ax reads each save' if UNZBD.exists() else '; mech3ax skipped (no unzbd)'))
    return 0


if __name__ == '__main__':
    rc = main()
    sys.stdout.flush()
    os._exit(rc)
