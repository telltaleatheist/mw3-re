"""The weapons' names and their MASS blocks, for the tests of the mech lab and the loadouts."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools' / 'analysis'))
from twod_world import entries  # noqa: E402

WEAPON_NAMES = [b'AC2', b'AC5', b'AC10', b'AC20', b'ERLaser_Small', b'ERLaser_Medium', b'ERLaser_Large', b'ERPPC',
                b'Flamer', b'GaussRifle', b'Laser_Small', b'Laser_Medium', b'Laser_Large', b'LB2XAC', b'LB5XAC',
                b'LB10XAC', b'LB20XAC', b'LRM5', b'LRM10', b'LRM15', b'LRM20', b'MGUN', b'NARC', b'PulseLaser_Small',
                b'PulseLaser_Medium', b'PulseLaser_Large', b'SRM2', b'SRM4', b'SRM6', b'StreakSRM2', b'StreakSRM4',
                b'StreakSRM6', b'TAG', b'UltraAC2', b'UltraAC5', b'UltraAC10', b'UltraAC20']


def weapon_labs(data):
    """Each weapon's MASS from weapons.zrd (tons and slots, Clan then Inner
    Sphere; tons per round = 1 / (rounds per ton x salvo)), as zWeapon builds
    it. Flamer and NARC get no block, to try that path."""
    import zrd
    start, length = next((s, n) for name, s, n in entries(data) if name.lower() == 'weapons.zrd')
    tree, _ = zrd.node(data[start:start + length], 0)
    defs = {}

    def walk(lst):
        for i, item in enumerate(lst):
            if isinstance(item, str) and i + 1 < len(lst) and isinstance(lst[i + 1], list) and 'MASS' in lst[i + 1]:
                defs[item.lower()] = lst[i + 1]
            elif isinstance(item, list):
                walk(item)
    walk(tree)

    def get(lst, key):
        return lst[lst.index(key) + 1] if key in lst and lst.index(key) + 1 < len(lst) else None

    labs = []
    for i, name in enumerate(WEAPON_NAMES):
        d = defs.get(name.decode().lower())
        if d is None or i in (8, 22):
            labs.append(None)
            continue
        mass = get(d, 'MASS')
        clan, sphere, ammo = get(mass, 'clan') or [0, 0], get(mass, 'sphere') or [0, 0], get(mass, 'ammo')
        salvo = get(d, 'SALVO')
        true = get(salvo, 'truecount') if isinstance(salvo, list) else None
        per_ton = (ammo[0] if ammo else 0) * (true[0] if true else 1)
        labs.append((float(clan[0]), float(sphere[0]), int(clan[1]), int(sphere[1]),
                     1.0 / per_ton if per_ton else 0.0))
    return labs
