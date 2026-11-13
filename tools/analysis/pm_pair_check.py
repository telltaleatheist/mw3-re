"""Checks the position-based pairs of analysis-output/pm-diff/pairs.json ('link order', 'loose', 'structure'), which
slide when PM adds or drops a neighbour (docs/port-audit-pm.md, method). A pair is suspect when the vtables the two
functions write (a constructor's or destructor's class; PM's mapped by pm-diff/vtables.json) share none.
On 2026-11-13: 8 suspects of 3,905 such pairs, 1 of 2,970 byte/call/string pairs. The callers' check is
pm_pair_callers.py. Run from the repo root.

    pm_pair_check.py [HOW...]   the suspects: PM, MW3, how paired, the PM row's label, the vtables
"""
import re
import sys
import json
import bisect
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import pm_feature_compare as F  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]


def writes(game, a):
    ins, idx = F.asm(game)
    out = set()
    for s, e in F.RANGES[game].get(a, []):
        for k in range(bisect.bisect_left(idx, s), bisect.bisect_right(idx, e)):
            _, mn, o = ins[k]
            m = re.match(r'dword ptr \[e[a-z]+\], (0x5[0-9a-f]{5})$', o)
            if mn == 'mov' and m:
                out.add(int(m.group(1), 16))
    return out


def main(argv):
    hows = set(argv) or {'link order', 'loose', 'structure'}
    p2m = {int(p, 16): int(m, 16) for m, p in json.load(open(ROOT / 'analysis-output/pm-diff/vtables.json')).items()}
    lab = {int(l.split('\t')[0], 16): l.split('\t')[1] for l in open(ROOT / 'tools/analysis/port_audit_labels_pm.tsv') if l.count('\t') >= 2}
    n = 0
    for q in json.load(open(ROOT / 'analysis-output/pm-diff/pairs.json')):
        if q.get('how') not in hows or not q.get('pm') or not q.get('mw3'):
            continue
        p, m = int(q['pm'], 16), int(q['mw3'], 16)
        wp = {p2m[v] for v in writes('pm', p) if v in p2m}
        wm = writes('mw3', m)
        if wp and wm and not (wp & wm):
            n += 1
            print('0x%08x mw3 0x%08x %-10s %-8s writes %s vs %s' % (p, m, q['how'], lab.get(p, '-'),
                  ' '.join(map(hex, sorted(wp))), ' '.join(map(hex, sorted(wm)))))
    print(n, 'suspect pairs', file=sys.stderr)


if __name__ == '__main__':
    main(sys.argv[1:])
