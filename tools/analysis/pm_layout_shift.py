"""The PM audit's layout-shift rule (docs/port-audit-pm.md, method): a changed pair whose feature compare flags
nothing but (a) immediates that pair off one MW3 value for one PM value, each a multiple of 4 and at least 0x40
(field offsets and sizes of PM's larger structures), and (b) a float constant against its negative (fsub written
as fadd) is a layout shift of the same logic. Prints each candidate with its pairs, or with --list only the
addresses.

    pm_layout_shift.py [--list] PMADDR...   (or no addresses: every unlabelled changed row of the doc)
"""
import re
import sys
import json
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import pm_feature_compare as F  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]


def shift_only(pm):
    mw, diff, flagged = F.compare(pm)
    if not flagged:
        return None  # nothing flagged: the plain compare already passes it
    imm_w, imm_p, const = [], [], {}
    for k, w, p in diff:
        if k.startswith('imm '):
            v = int(k.split()[1], 16)
            if v <= 1:
                continue
            if w > p:
                imm_w += [v] * (w - p)
            else:
                imm_p += [v] * (p - w)
        elif k.startswith('const '):
            kind, val = k.split()[1], float(k.split()[2])
            const.setdefault((kind, abs(val)), [0, 0])
            const[(kind, abs(val))][0 if val < 0 else 1] += (p - w)
        elif k.startswith('call') or k.startswith('field'):
            return False
    # each float constant's change is its sign only: as many negatives gone as positives come, or the reverse
    for (_, _), (neg, pos) in const.items():
        if neg + pos != 0:
            return False
    if len(imm_w) != len(imm_p):
        return False
    for v in imm_w + imm_p:
        if v % 4 or v < 0x40:
            return False
    return list(zip(sorted(imm_w), sorted(imm_p)))


def main(argv):
    only = '--list' in argv
    addrs = [int(a, 16) for a in argv if a != '--list']
    if not addrs:
        doc = (ROOT / 'docs' / 'port-audit-pm.md').read_text()
        unl = set(re.findall(r'\| (0x[0-9a-f]{8}) \| \d+ \| unlabelled \|', doc))
        addrs = sorted(int(a, 16) for a in unl if int(a, 16) in F.TWIN)
    for a in addrs:
        try:
            r = shift_only(a)
        except KeyError:
            continue
        if r:
            print('0x%08x' % a if only else '0x%08x MW3 0x%08x  %s' % (a, F.TWIN[a], ' '.join('%#x->%#x' % x for x in r)))


if __name__ == '__main__':
    main(sys.argv[1:])
