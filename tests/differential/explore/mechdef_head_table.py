"""One row per dfn_*.zrd with the head values (docs/drafts/mechdef-head.md), and
stray top-level strings (comment words the compiler kept)."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
from mechdef_head_survey import ROOT, TOP, SHAKE, find, zrd  # noqa: E402

KNOWN = set(TOP) | {'omnimech', 'clan', 'internal_mass', 'max_mass', 'weight_class', 'hud', 'camera', 'animations',
                    'skins', 'firepoints', 'parts', 'configurations'}
data = (ROOT / 'game/mw3-1.2/zbd/reader.zbd').read_bytes()
cols = ['max_turn_rate', 'accel_rate', 'decel_rate', 'crouch_height', 'engine_minmax', 'torso_limits', 'lean_angle',
        'lean_factor', 'recoil_bend', 'recoil_slide', 'recoil_twist', 'aim_slerp_freq', 'aim_min_pitch',
        'aim_max_pitch', 'aim_min_yaw', 'aim_max_yaw', 'aim_arm_pitch_factor', 'aim_ignore_arms', 'chicken']
def v(r):
    if r is None: return '-'
    x = r[0]
    if isinstance(x, list): return ' '.join(str(round(y, 3)) if isinstance(y, float) else str(y) for y in x)
    return str(x)
print('|mech|' + '|'.join(cols) + '|shake p y r f d|')
for name, s, l in zrd.entries(data):
    if not name.lower().startswith('dfn_'): continue
    tree, _ = zrd.node(data[s:s + l], 0)
    top = tree[0]
    row = [v(find(tree, c)) for c in cols]
    sh = find(tree, 'torso_shake')[0]
    row.append(' '.join(v(find(sh, k)) for k in SHAKE))
    print('|' + name[4:-4] + '|' + '|'.join(row) + '|')
    stray, i = [], 0
    while i < len(top):
        it = top[i]
        if isinstance(it, str) and it in KNOWN: i += 2; continue
        stray.append(zrd.text(it).strip().replace('\n', ' ')[:40]); i += 1
    if stray: print('   stray:', stray)
