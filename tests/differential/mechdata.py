"""The movement values of the 19 shipped mechs, read from reader.zbd's
dfn_<mech>.zrd as the game reads them (docs/movement.md, mechdef-configs.md):
the definition's turn, accel and decel rates and the gait table (each gait's
"trans" pair, with stand, step and walk.lo forced as 0x431a70 does)."""
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools' / 'analysis'))
import zrd  # noqa: E402

READER = ROOT / 'game' / 'mw3-1.2' / 'zbd' / 'reader.zbd'
GAITS = ['stand', 'step', 'walk', 'trot', 'run']


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


def _pairs(items):
    """Each string key followed by its list (the reader's key/value layout)."""
    return {k: v for k, v in zip(items, items[1:]) if isinstance(k, str) and isinstance(v, list)}


def _find(items, key):
    """The list after the first `key` string, depth first."""
    for i, v in enumerate(items):
        if v == key and i + 1 < len(items) and isinstance(items[i + 1], list):
            return items[i + 1]
        if isinstance(v, list):
            r = _find(v, key)
            if r is not None:
                return r
    return None


def mechs():
    """{name: {'turn_rate', 'accel', 'decel', 'turn_accel', 'gait': [(lo, hi)] * 13}}"""
    data = READER.read_bytes()
    out = {}
    for name, start, length in zrd.entries(data):
        if not (name.startswith('dfn_') and name.endswith('.zrd')):
            continue
        root, _ = zrd.node(data, start)
        top = _pairs(root[0])
        def first(key, default):
            v = top.get(key)
            return f32(v[0]) if v else default
        motion = {}
        for key, block in _pairs(_find(root, 'motion') or []).items():
            trans = _pairs(block).get('trans')
            if trans:
                motion[key] = (f32(trans[0]), f32(trans[1]))
        gait = [(0.0, 0.0)] * 13
        for i, g in enumerate(GAITS):
            gait[2 + i] = motion.get(g, (0.0, 0.0))
        gait[2] = (gait[2][0], f32(0.01))          # 0x431a70: stand.hi, step, walk.lo forced
        gait[3] = (0.0, f32(0.05))
        gait[4] = (f32(0.02), gait[4][1])
        out[name[4:-4]] = {
            'turn_rate': f32(first('max_turn_rate', 2.5132) * 0.5),
            'accel': first('accel_rate', 0.5), 'decel': first('decel_rate', 1.3),
            'turn_accel': first('turn_accel_factor', 1.5), 'gait': gait,
        }
    return out


if __name__ == '__main__':
    for k, v in mechs().items():
        print(k, v)
