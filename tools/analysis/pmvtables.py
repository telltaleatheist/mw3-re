#!/usr/bin/env python3
"""Pairs MW3's vtables with PM's from what each pair of twin functions writes (constructors and
destructors, in order), for tests/differential/pm_moved.py. pm-diff/addrmap.json's data section matches
by contents, and vtables look alike: it is wrong for about half of them.

    tools/analysis/pmvtables.py   writes analysis-output/pm-diff/vtables.json
"""
import bisect
import collections
import json
import re
from pathlib import Path

OUT = Path(__file__).resolve().parents[2] / 'analysis-output'
WRITE = re.compile(r'mov\s+dword ptr \[e\w\w(?: \+ 0x[0-9a-f]+)?\],\s*(0x5[b-f][0-9a-f]{4})$')
LINE = re.compile(r'^\s*([0-9a-f]+):\s+(?:[0-9a-f]{2} )*\s*(\S.*)$')


def writes(name):
    """Each vtable-like write in an asm listing, by address."""
    found = []
    for line in (OUT / name).read_text(errors='replace').split('\n'):
        m = LINE.match(line)
        if m:
            w = WRITE.match(re.sub(r'\s*<.*', '', m.group(2)).strip())
            if w:
                found.append((int(m.group(1), 16), int(w.group(1), 16)))
    return found


def starts(folder):
    return sorted(int(p.stem, 16) for p in (OUT / folder).glob('0x*.c'))


def between(found, lo, hi):
    keys = [a for a, _ in found]
    return [v for _, v in found[bisect.bisect_left(keys, lo):bisect.bisect_left(keys, hi)]]


def main():
    functions = json.loads((OUT / 'pm-diff' / 'addrmap.json').read_text())['functions']
    mw, pm = writes('mech3.asm'), writes('pm.asm')
    mw_starts, pm_starts = starts('decomp'), starts('pm-decomp')
    seen = collections.defaultdict(collections.Counter)
    for a, b in functions.items():
        a, b = int(a, 16), int(b, 16)
        i, k = bisect.bisect_right(mw_starts, a), bisect.bisect_right(pm_starts, b)
        if i == len(mw_starts) or k == len(pm_starts):
            continue
        x, y = between(mw, a, mw_starts[i]), between(pm, b, pm_starts[k])
        if x and len(x) == len(y):
            for p, q in zip(x, y):
                seen[p][q] += 1
    pairs = {hex(p): hex(c.most_common(1)[0][0]) for p, c in sorted(seen.items())}
    (OUT / 'pm-diff' / 'vtables.json').write_text(json.dumps(pairs, indent=0))
    print(f'{len(pairs)} vtables paired; {sum(len(c) > 1 for c in seen.values())} with more than one answer')


if __name__ == '__main__':
    main()
