"""The unmapped-call hole's second half (docs/port-audit-pm.md, method): a fold that rests on the feature compare, the
slot pairing or the layout-shift rule is right only if the functions it calls that the address map cannot place are
the same too. For each such fold, its PM side's unmapped callees (direct calls, and their thunks followed one jump)
are looked up in the doc: a callee labelled MISSING or unsure sends the fold back to read; one still unlabelled
makes it wait for that callee. Run again after the last labels land.

    pm_fold_propagation.py [--list]    the counts, then (with --list) each fold and the callees that hold it
"""
import re
import sys
import bisect
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import pm_feature_compare as F  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
COMPARE = re.compile(r'feature compare|slot pairing|layout-shift rule|deleting destructor')


def calls(addr):
    ins, idx = F.asm('pm')
    out = []
    for s, e in F.RANGES['pm'].get(addr, []):
        for k in range(bisect.bisect_left(idx, s), bisect.bisect_right(idx, e)):
            _, mn, ops = ins[k]
            if mn == 'call' and ops.startswith('0x'):
                out.append(int(ops.split()[0], 16))
    return out


def thunk_target(addr):
    """A one-instruction jmp thunk's target, else the address."""
    ins, idx = F.asm('pm')
    k = bisect.bisect_left(idx, addr)
    if k < len(ins) and ins[k][0] == addr and ins[k][1] == 'jmp' and ins[k][2].startswith('0x'):
        return int(ins[k][2].split()[0], 16)
    return addr


def main(argv):
    doc = (ROOT / 'docs' / 'port-audit-pm.md').read_text()
    state = {int(a, 16): c.strip() for a, c in re.findall(r'\| (0x[0-9a-f]{8}) \| \d+ \| ([^|]+?) \|', doc)}
    labels = [l.rstrip('\n').split('\t') for l in open(ROOT / 'tools/analysis/port_audit_labels_pm.tsv') if l.count('\t') >= 2]
    folds = [(int(a, 16), r) for a, c, r in labels if c == 'folded' and COMPARE.search(r)]
    back, wait = {}, {}
    for a, _ in folds:
        for c in calls(a):
            if hex(c) in F.PMF:
                continue  # mapped: compared by identity
            for t in {c, thunk_target(c)}:
                s = state.get(t)
                if s is None:
                    continue  # no row: ported or outside the audit
                if s.startswith('MISSING') or 'unsure' in s.lower():
                    back.setdefault(a, set()).add((t, s[:30]))
                elif s == 'unlabelled':
                    wait.setdefault(a, set()).add((t, s))
    print('compare-based folds', len(folds), '| back to read (a callee missing or unsure)', len(back),
          '| waiting on an unlabelled callee', len(wait))
    if '--list' in argv:
        for name, d in (('back', back), ('wait', wait)):
            for a in sorted(d):
                print(name, '0x%08x' % a, ' '.join('0x%08x(%s)' % x for x in sorted(d[a])))


if __name__ == '__main__':
    main(sys.argv[1:])
