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
        pr, mr = memref(pi, True), memref(mi, True)
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
        pv, mv = imm(pi, True), imm(mi, True)
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

# The instruction map covers about 40% of these functions' instructions. A
# second check over all of them: the float values each function reads (its
# memory operands and float immediates), as a set per game; a value in one
# and not the other is listed (pm_constants_sets.tsv).
if len(sys.argv) > 2:
    mw_sorted = sorted(mwops)
    starts_mw = sorted(int(r['mw3'], 16) for r in rows if r['mw3'])
    def values(ops, srt, a, end, intel, exe):
        out = set()
        j = bisect.bisect_left(srt, a)
        while j < len(srt) and srt[j] < end:
            ins = ops[srt[j]]; j += 1
            if re.match(r'fld1\b', ins): out.add(1.0); continue
            if re.match(r'fldz\b', ins): out.add(0.0); continue
            r_ = memref(ins, intel)
            if r_:
                b = exe.raw(r_[0], r_[1])
                if b:
                    if r_[1] == 8:
                        v = struct.unpack('<d', b)[0]
                        if abs(v) < 1e9 and v == v: out.add(round(v, 6))
                    elif is_float(b): out.add(round(struct.unpack('<f', b)[0], 6))
                continue
            v = imm(ins, intel)
            nxt_ = ops[srt[j]] if j < len(srt) else ''
            if v is not None and ins.startswith('mov') and re.match(r'i?mul\b', nxt_): continue  # a division's magic number
            if v is not None and (0x3a000000 <= v < 0x4f000000 or 0xba000000 <= v < 0xcf000000):
                out.add(round(struct.unpack('<f', struct.pack('<I', v))[0], 6))
        return out
    so = open(sys.argv[2].replace('.tsv', '_sets.tsv'), 'w'); ns = 0
    mw_starts = sorted(int(a, 16) for a in json.load(open(A / 'func2file.json')))
    for r in rows:
        if (r['status'] != 'changed' and r['addr'] not in extra) or not r['mw3']: continue
        a, m = int(r['addr'], 16), int(r['mw3'], 16)
        i = bisect.bisect_right(starts, a); pe_ = starts[i] if i < len(starts) else a + 0x1000
        k = bisect.bisect_right(mw_starts, m); me_ = mw_starts[k] if k < len(mw_starts) else m + 0x1000
        pv, mv = values(pmops, pm_sorted, a, pe_, True, pmx), values(mwops, mw_sorted, m, me_, True, mw3x)
        only_m, only_p = sorted(mv - pv), sorted(pv - mv)
        if only_m or only_p:
            ns += 1
            so.write(f"{r['addr']}\t{r['mw3']}\t{r['lines']}\t{r['ported_in'][:60]}\tMW3 only {only_m[:8]}\tPM only {only_p[:8]}\n")
    print(ns, 'functions whose float values differ as sets')

# Integers, the same way (pm_constants_ints.tsv): compare, mov and push
# immediates (the operand after the last comma or the push's), and dword
# reads from a fixed address that are not floats or addresses. Dropped:
# addresses, anything inside brackets (stack and field offsets), 0, 1, -1, 2.
if len(sys.argv) > 2:
    def ints(ops, srt, a, end, exe):
        out = set()
        j = bisect.bisect_left(srt, a)
        while j < len(srt) and srt[j] < end:
            ins = ops[srt[j]]; j += 1
            mnem = ins.split()[0] if ins else ''
            if mnem not in ('cmp', 'mov', 'push'): continue
            r_ = memref(ins, True)
            if r_ and r_[1] == 4 and mnem == 'mov' and ins.split(',')[-1].strip().startswith('dword ptr [0x'):
                b = exe.raw(r_[0], 4)
                if b:
                    v = struct.unpack('<i', b)[0]
                    if not is_float(b) or abs(v) < 0x10000:
                        if not (0x400000 <= (v & 0xffffffff) < 0xb00000) and v not in (0, 1, -1, 2): out.add(v)
                continue
            m = re.search(r'(?:,\s*|push\s+)(0x[0-9a-f]+|\d+)$', ins)
            if not m: continue
            v = int(m.group(1), 0)
            if v >= 0x80000000: v -= 1 << 32
            if 0x400000 <= (v & 0xffffffff) < 0xb00000 or v in (0, 1, -1, 2): continue
            b = struct.pack('<I', v & 0xffffffff)
            if abs(v) >= 0x10000 and is_float(b): continue   # a float immediate: the float check's
            out.add(v)
        return out
    si = open(sys.argv[2].replace('.tsv', '_ints.tsv'), 'w'); ni = 0
    for r in rows:
        if (r['status'] != 'changed' and r['addr'] not in extra) or not r['mw3']: continue
        a, m = int(r['addr'], 16), int(r['mw3'], 16)
        i = bisect.bisect_right(starts, a); pe_ = starts[i] if i < len(starts) else a + 0x1000
        k = bisect.bisect_right(mw_starts, m); me_ = mw_starts[k] if k < len(mw_starts) else m + 0x1000
        pv, mv = ints(pmops, pm_sorted, a, pe_, pmx), ints(mwops, mw_sorted, m, me_, mw3x)
        om, op_ = sorted(mv - pv), sorted(pv - mv)
        if om or op_:
            ni += 1
            si.write(f"{r['addr']}\t{r['mw3']}\t{r['lines']}\t{r['ported_in'][:60]}\tMW3 {om[:10]}\tPM {op_[:10]}\n")
    print(ni, 'functions whose integer values differ as sets')

# The integer sets' shift filter, kept only where the value is a size or an
# id (lead, 2026-11-13): pushed before operator new / malloc / calloc, a
# switch's case base (cmp, ja, then the jump table), or pushed before the
# message lookup. Every pair the filter caught goes to pm_constants_shifted.tsv
# with its reason; in gameplay modules the unexplained ones are candidates.
if len(sys.argv) > 2:
    NEW_PM, NEW_MW = 0x5ce8c6, 0x5abd26
    ALLOC_IAT = ('0x5de664', '0x5de60c', '0x5bb664', '0x5bb60c')
    MSG_MW = 0x576040
    MSG_PM = int(am['functions'].get(hex(MSG_MW), '0x0'), 16)
    def ctx_ints(ops, srt, a, end, exe, new_thunk, msg):
        vals = {}
        idx = list(range(bisect.bisect_left(srt, a), bisect.bisect_left(srt, end)))
        for n_, j in enumerate(idx):
            ins = ops[srt[j]]
            mnem = ins.split()[0] if ins else ''
            if mnem not in ('cmp', 'mov', 'push'): continue
            m = re.search(r'(?:,\s*|push\s+)(0x[0-9a-f]+|\d+)$', ins)
            if not m: continue
            v = int(m.group(1), 0)
            if v >= 0x80000000: v -= 1 << 32
            if 0x400000 <= (v & 0xffffffff) < 0xb00000 or v in (0, 1, -1, 2): continue
            if abs(v) >= 0x10000 and is_float(struct.pack('<I', v & 0xffffffff)): continue
            nxt = [ops[srt[k]] for k in idx[n_ + 1:n_ + 5]]
            why = ''
            if mnem == 'push' and any(re.search(r'call\s+0x%x\b' % new_thunk, x) or any(i_ in x for i_ in ALLOC_IAT) for x in nxt[:3]): why = 'size'
            elif mnem == 'cmp' and len(nxt) > 1 and nxt[0].startswith('ja') and 'jmp' in nxt[1] and '*4' in nxt[1].replace(' ', ''): why = 'switch'
            elif mnem == 'push' and msg and any(re.search(r'call\s+0x%x\b' % msg, x) for x in nxt[:3]): why = 'message id'
            elif mnem == 'push' and any(re.search(r'call\s+0x%x\b' % rep, x) for rep in (0x49bfd0, 0x406cb0) for x in nxt[:4]): why = 'report line'
            vals.setdefault(v, why or vals.get(v, ''))
        return vals
    GAMEPLAY = ('weapon', 'mech_', 'zai_', 'hud', 'player', 'mission', 'salvage', 'pilot', 'veh_', 'turret', 'zweapon', 'landmine')
    sh = open(sys.argv[2].replace('.tsv', '_shifted.tsv'), 'w')
    cand = open(sys.argv[2].replace('.tsv', '_ints_back.tsv'), 'w'); nb = 0; big = []
    for r in rows:
        if (r['status'] != 'changed' and r['addr'] not in extra) or not r['mw3']: continue
        a, m = int(r['addr'], 16), int(r['mw3'], 16)
        i = bisect.bisect_right(starts, a); pe_ = starts[i] if i < len(starts) else a + 0x1000
        k = bisect.bisect_right(mw_starts, m); me_ = mw_starts[k] if k < len(mw_starts) else m + 0x1000
        pv = ctx_ints(pmops, pm_sorted, a, pe_, pmx, NEW_PM, MSG_PM)
        mv = ctx_ints(mwops, mw_sorted, m, me_, mw3x, NEW_MW, MSG_MW)
        gp = any(g in r['ported_in'] for g in GAMEPLAY)
        om, op_ = sorted(set(mv) - set(pv)), sorted(set(pv) - set(mv))
        for v in om + op_:
            if abs(v) >= 100000 and gp: big.append((r['addr'], v))
        used = set(); back = []
        for v in [x for x in om if abs(x) < 100000]:
            s = [p for p in op_ if p not in used and 0 < p - v <= 256 and v >= 64 and abs(p) < 100000]
            if not s: continue
            p = s[0]; used.add(p)
            why = mv[v] or pv[p]
            sh.write(f"{r['addr']}\t{r['mw3']}\t{r['ported_in'][:50]}\t{v} -> {p}\t{why or 'unexplained'}\n")
            if not why and gp: back.append(f'{v} -> {p}')
        if back:
            nb += 1
            cand.write(f"{r['addr']}\t{r['mw3']}\t{r['lines']}\t{r['ported_in'][:60]}\t{' ; '.join(back)}\tint, shifted\n")
    print(nb, 'gameplay functions with shifted values the filter cannot explain;', len(big), 'gameplay values over 100000:', big[:12])
