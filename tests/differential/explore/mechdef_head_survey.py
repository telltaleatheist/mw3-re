"""Survey of the head keys of every dfn_*.zrd, found the way the reader finds
them (depth first, values match as keys). docs/drafts/mechdef-head.md.

    .venv/bin/python tests/differential/explore/mechdef_head_survey.py
"""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools' / 'analysis'))
import zrd  # noqa: E402

TOP = ['version', 'chicken', 'max_turn_rate', 'accel_rate', 'decel_rate', 'turn_accel_factor', 'crouch_height',
       'engine_minmax', 'torso_limits', 'torso_shake', 'lod_range', 'lean_angle', 'lean_factor', 'lean_speed',
       'recoil_bend', 'recoil_slide', 'recoil_twist', 'aim_slerp_freq', 'aim_min_pitch', 'aim_max_pitch',
       'aim_min_yaw', 'aim_max_yaw', 'aim_arm_pitch_factor', 'aim_ignore_arms']
SHAKE = ['pitch_amp', 'yaw_amp', 'roll_amp', 'freq', 'damp_freq']


def find(lst, key):
    """0x556930: returns (found, value-after) where value-after may be None (past the end)."""
    for i, item in enumerate(lst):
        if isinstance(item, list):
            r = find(item, key)
            if r is not None:
                return r
        elif isinstance(item, str) and item == key:
            return (lst[i + 1] if i + 1 < len(lst) else '<past end>',)
    return None


def show(v):
    return zrd.text(v).strip().replace('\n', ' ')


def main():
    data = (ROOT / 'game/mw3-1.2/zbd/reader.zbd').read_bytes()
    files = [(n, s, l) for n, s, l in zrd.entries(data) if n.lower().startswith('dfn_')]
    print(len(files), 'files')
    seen = {}
    for name, start, length in files:
        tree, _ = zrd.node(data[start:start + length], 0)
        print(f'== {name}')
        for k in TOP:
            r = find(tree, k)
            print(f'  {k:22} {"MISSING" if r is None else show(r[0])}')
            seen.setdefault(k, 0)
            seen[k] += r is not None
        sh = find(tree, 'torso_shake')
        if sh and isinstance(sh[0], list):
            for k in SHAKE:
                r = find(sh[0], k)
                print(f'    {k:20} {"MISSING" if r is None else show(r[0])}')
        top = tree[0] if len(tree) == 1 and isinstance(tree[0], list) else tree
        keys = [top[i] for i in range(0, len(top), 2) if isinstance(top[i], str)]
        print('  top keys:', ' '.join(keys))
    print('present counts:', seen)


if __name__ == '__main__':
    main()
