"""pmconst2.py AUDIT_PM.json OUT.tsv [extra PM addrs]: group A's constants by
instruction. Each PM instruction of the function mapped to its MW3 one
(addrmap 'instructions'); a memory operand's value (4 or 8 bytes) and an
immediate compared between the exes. Kept: a differing pair where either
side reads as a float (or a qword double), or an immediate under 0x400000."""
import json, re, sys, struct, bisect
from pathlib import Path
A = Path('analysis-output')
am = json.load(open(A / 'pm-diff' / 'addrmap.json'))
inv_i = {int(v, 16): int(k, 16) for k, v in am['instructions'].items()}
class PE:
    def __init__(s, path):
        s.d = open(path, 'rb').read(); pe = struct.unpack_from('<I', s.d, 0x3c)[0]
        n = struct.unpack_from('<H', s.d, pe + 6)[0]; opt = struct.unpack_from('<H', s.d, pe + 20)[0]
        s.base = struct.unpack_from('<I', s.d, pe + 52)[0]
        s.secs = [struct.unpack_from('<IIII', s.d, pe + 24 + opt + i * 40 + 8) for i in range(n)]
    def raw(s, va, n):
        r = va - s.base
        for vs, v, rs, raw in s.secs:
            if v <= r < v + min(rs, vs or rs) - n + 1: return s.d[raw + r - v: raw + r - v + n]
        return None
mw3x, pmx = PE('game/mw3-1.2/Mech3.exe'), PE('game/pm/Mech3.exe')
MEM = re.compile(r'(qword|dword) ptr \[0x([0-9a-f]+)\]')
IMM = re.compile(r',\s*0x([0-9a-f]+)\s*$|push\s+0x([0-9a-f]+)\s*$')
MEMa = re.compile(r'(?:fld[sl]?|fmul[sl]?|fadd[sl]?|fsub[sl]?|fdiv[sl]?|fcomp?[sl]?|fsubr[sl]?|fdivr[sl]?|mov[lw]?)\s.*?0x([0-9a-f]{6})\b')
def load(asm, intel):
    ops = {}
    pat = re.compile(r'^\s*([0-9a-f]+):\s+(?:[0-9a-f]{2} )*\s*(.*)$')
    for line in open(asm, errors='replace'):
        m = pat.match(line)
        if not m: continue
        a, ins = int(m.group(1), 16), m.group(2).strip()
        ops[a] = ins
    return ops
pmops, mwops = load(A / 'pm.asm', True), load(A / 'mech3.asm', False)
pm_sorted = sorted(pmops)
def is_float(b):
    f = struct.unpack('<f', b)[0]
    return f == 0 or 1e-6 < abs(f) < 1e7
def memref(ins, intel):
    if intel:
        m = MEM.search(ins)
        return (int(m.group(2), 16), 8 if m.group(1) == 'qword' else 4) if m else None
    m = re.search(r'\b0x([5-9a-f][0-9a-f]{5})\b(?!\()', ins)
    if m and ('%' in ins or 'fld' in ins or 'fmul' in ins or 'fadd' in ins or 'fsub' in ins or 'fdiv' in ins or 'fcom' in ins):
        w = 8 if re.match(r'f\w*l\b', ins) else 4
        return (int(m.group(1), 16), w)
    return None
def imm(ins, intel):
    if intel:
        m = re.search(r'(?:,\s*|push\s+)0x([0-9a-f]+)$', ins)
    else:
        m = re.search(r'\$0x([0-9a-f]+)', ins)
    return int(m.group(1), 16) if m else None
rows = json.load(open(sys.argv[1])); by = {r['addr']: r for r in rows}
starts = sorted(int(r['addr'], 16) for r in rows)
extra = set(sys.argv[3:])
out = open(sys.argv[2], 'w'); nf = nv = 0
for r in rows:
    if r['status'] != 'changed' and r['addr'] not in extra: continue
    a = int(r['addr'], 16)
    i = bisect.bisect_right(starts, a); end = starts[i] if i < len(starts) else a + 0x1000
    j = bisect.bisect_left(pm_sorted, a)
    diffs = []
    while j < len(pm_sorted) and pm_sorted[j] < end:
        pa = pm_sorted[j]; j += 1
        ma = inv_i.get(pa)
        if ma is None or ma not in mwops: continue
        pi, mi = pmops[pa], mwops[ma]
        pr, mr = memref(pi, True), memref(mi, False)
        if pr and mr:
            w = pr[1]
            pv, mv = pmx.raw(pr[0], w), mw3x.raw(mr[0], w)
            if pv and mv and pv != mv:
                if w == 8:
                    x, y = struct.unpack('<d', mv)[0], struct.unpack('<d', pv)[0]
                    if abs(x) < 1e9 and abs(y) < 1e9: diffs.append(f'{x:.6g} -> {y:.6g} (double [{mr[0]:#x}] / [{pr[0]:#x}])')
                elif is_float(pv) and is_float(mv):
                    diffs.append(f"{struct.unpack('<f', mv)[0]:.6g} -> {struct.unpack('<f', pv)[0]:.6g} ([{mr[0]:#x}] / [{pr[0]:#x}])")
            continue
        pv, mv = imm(pi, True), imm(mi, False)
        if pv is not None and mv is not None and pv != mv:
            fb = lambda v: struct.pack('<I', v & 0xffffffff)
            if (0x3a000000 <= pv < 0x4f000000 or 0xba000000 <= pv < 0xcf000000) and (0x3a000000 <= mv < 0x4f000000 or 0xba000000 <= mv < 0xcf000000):
                diffs.append(f"{struct.unpack('<f', fb(mv))[0]:.6g} -> {struct.unpack('<f', fb(pv))[0]:.6g} (immediate at {ma:#x} / {pa:#x})")
            elif pv < 0x10000 and mv < 0x10000 and not any(k in pi for k in ('ptr [e', '+ 0x', '- 0x')):
                diffs.append(f'{mv:#x} -> {pv:#x} (int at {ma:#x} / {pa:#x}: {mi[:30]} | {pi[:30]})')
    nf += 1
    if diffs:
        nv += 1
        out.write(f"{r['addr']}\t{r['mw3']}\t{r['lines']}\t{r['ported_in'][:60]}\t{' ; '.join(diffs[:10])}\n")
print(nf, 'functions;', nv, 'with a value that differs')
