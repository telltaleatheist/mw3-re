"""Compares a Pirate's Moon function with its MW3 twin by instruction features
(docs/port-audit-pm.md, method: the feature compare).

    pm_feature_compare.py PMADDR...          one report per function
    pm_feature_compare.py --flag PMADDR...   one line each: same, or what differs

Where the two compilers allocate registers and schedule otherwise, the
decompiled text differs while the logic does not. This compares what neither
can move, as multisets over the function's instructions:

- calls and data references, MW3's mapped to PM through pm-diff/addrmap.json;
- immediates (stack arithmetic and `ret n` left out: calling conventions);
- float and double constants loaded from data, by their raw values in each exe;
- field offsets off a register (not esp, not ebp's locals), reported but not
  flagged: PM's layouts shift them.

--flag marks a function when a call, an immediate above 1 or a float constant
differs. Ranges run from each function's start to the next decompiled function
(MW3's capped at 1.3 x PM's size + 64, past which MW3's next decompiled
function can lie). Reads analysis-output/{pm.asm,mech3.asm,decomp,pm-decomp,
pm-diff} and the two exes under game/ (git-ignored).
"""
import bisect
import collections
import json
import os
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
AO = ROOT / 'analysis-output'
AM = json.load(open(AO / 'pm-diff/addrmap.json'))
FM = AM['functions']                       # MW3 -> PM, '0x...' without padding
PMF = set(FM.values())
TWIN = {}
for q in json.load(open(AO / 'pm-diff/pairs.json')):
    if q.get('pm') and q.get('mw3'):
        TWIN[int(q['pm'], 16)] = int(q['mw3'], 16)


def starts(d):
    return sorted(int(f[2:-2], 16) for f in os.listdir(d) if f.startswith('0x') and f.endswith('.c'))


PM_STARTS, MW_STARTS = starts(AO / 'pm-decomp'), starts(AO / 'decomp')


def load(path, bytes_col):
    # pm.asm has a bytes column (a 10-byte instruction has no space before
    # the tab); mech3.asm has none, and its mnemonics can start with hex
    # letters ("fadd", "dec"), so the two take separate patterns.
    pat = (re.compile(r'\s+([0-9a-f]+):\s+(?:[0-9a-f]{2} ?)+\s*\t(\S+)\s*(.*)') if bytes_col
           else re.compile(r'\s+([0-9a-f]+):\s*\t(\S+)\s*(.*)'))
    ins = []
    for line in open(path):
        m = pat.match(line)
        if m:
            ins.append((int(m.group(1), 16), m.group(2), m.group(3).split('<')[0].strip()))
    return ins, [i[0] for i in ins]


_ASM = {}


def asm(game):
    if game not in _ASM:
        _ASM[game] = load(AO / ('pm.asm' if game == 'pm' else 'mech3.asm'), game == 'pm')
    return _ASM[game]


def image(path):
    d = open(path, 'rb').read()
    e = struct.unpack_from('<I', d, 0x3c)[0]
    ns, oh = struct.unpack_from('<H', d, e + 6)[0], struct.unpack_from('<H', d, e + 20)[0]
    secs = [struct.unpack_from('<8sIIII', d, e + 24 + oh + i * 40) for i in range(ns)]

    def read(va, n):
        for _, _, vaa, rs, ro in secs:
            if 0x400000 + vaa <= va < 0x400000 + vaa + rs:
                o = ro + va - 0x400000 - vaa
                return d[o:o + n]
    return read


EXE = {'pm': image(ROOT / 'game/pm/Mech3.exe'), 'mw3': image(ROOT / 'game/mw3-1.2/Mech3.exe')}
OFF = re.compile(r'\[(\w+)(?: \+ \w+\*\d)? ([+-]) (0x[0-9a-f]+)\]')
IMM = re.compile(r'(?<![+\-\[] )(?<!\[)\b(0x[0-9a-f]+)\b(?!\])')
FLD = re.compile(r'(dword|qword) ptr \[0x([0-9a-f]+)\]')


def features(game, s, e):
    ins, idx = asm(game)
    c = collections.Counter()
    for k in range(bisect.bisect_left(idx, s), len(ins)):
        ad, mn, ops = ins[k]
        if ad >= e:
            break
        if mn.startswith('j') or mn == 'ret' or ops.startswith('esp,'):
            continue
        for m in OFF.finditer(ops):
            if m.group(1) != 'esp' and not (m.group(1) == 'ebp' and m.group(2) == '-'):
                c['off' + m.group(2) + m.group(3)] += 1
        for m in IMM.finditer(re.sub(r'[+-] 0x[0-9a-f]+\]', ']', ops)):
            v = int(m.group(1), 16)
            kind = 'call ' if mn == 'call' else 'ref '
            if v >= 0x400000:
                if game == 'mw3':
                    c[kind + FM.get(hex(v), 'unmapped')] += 1
                else:
                    c[kind + (hex(v) if hex(v) in PMF else 'unmapped')] += 1
            else:
                c['imm ' + m.group(1)] += 1
        if mn.startswith('f'):
            m = FLD.search(ops)
            if m:
                b = EXE[game](int(m.group(2), 16), 8 if m.group(1) == 'qword' else 4)
                if b:
                    v = struct.unpack('<d' if m.group(1) == 'qword' else '<f', b)[0]
                    if v == 0 or 1e-30 < abs(v) < 1e30:
                        c['const %s %.9g' % (m.group(1)[0], v)] += 1
    return c


def compare(pm):
    mw = TWIN[pm]
    pe = PM_STARTS[bisect.bisect_right(PM_STARTS, pm)]
    me = min(MW_STARTS[bisect.bisect_right(MW_STARTS, mw)], mw + int((pe - pm) * 1.3) + 64)
    fp, fw = features('pm', pm, pe), features('mw3', mw, me)
    diff = [(k, fw[k], fp[k]) for k in sorted(set(fp) | set(fw)) if fw[k] != fp[k]]
    flagged = [k for k, _, _ in diff
               if k.startswith('call') or k.startswith('const')
               or (k.startswith('imm') and int(k.split()[1], 16) > 1)]
    return mw, diff, flagged


def main(argv):
    flag = '--flag' in argv
    for a in (x for x in argv if x != '--flag'):
        pm = int(a, 16)
        mw, diff, flagged = compare(pm)
        if flag:
            print('0x%08x' % pm, 'same' if not flagged else ' '.join(flagged))
            continue
        print('=== PM 0x%08x MW3 0x%08x, %d differ, %d flagged' % (pm, mw, len(diff), len(flagged)))
        for k, w, p in diff:
            print('  %-36s mw3 %3d  pm %3d%s' % (k, w, p, '  *' if k in flagged else ''))


if __name__ == '__main__':
    main(sys.argv[1:])
