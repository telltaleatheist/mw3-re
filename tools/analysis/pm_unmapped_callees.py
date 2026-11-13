"""The feature compare's unmapped calls (docs/port-audit-pm.md, method): for each listed PM function, its
unmapped callees paired with its MW3 twin's by their place in the call order, each pair run through the same
compare. pm_unmapped_callees.py FILE (one PM address a line)."""
import sys, bisect, collections
sys.path.insert(0, 'tools/analysis')
import pm_feature_compare as F
def calls(game, addr):
    ins, idx = F.asm(game); out = []
    for s, e in F.RANGES[game].get(addr, []):
        for k in range(bisect.bisect_left(idx, s), bisect.bisect_right(idx, e)):
            ad, mn, ops = ins[k]
            if mn == 'call' and ops.startswith('0x'):
                out.append(int(ops.split()[0], 16))
    return out
def mapped(game, v):
    return (F.FM.get(hex(v)) if game == 'mw3' else (hex(v) if hex(v) in F.PMF else None))
def feat_cmp(pm, mw):
    if pm not in F.RANGES['pm'] or mw not in F.RANGES['mw3']: return ['no range']
    fp, fw = F.features('pm', pm), F.features('mw3', mw)
    return [k for k in sorted(set(fp) | set(fw)) if fp[k] != fw[k] and (k.startswith('call') or k.startswith('const') or k.startswith('field') or (k.startswith('imm') and int(k.split()[1], 16) > 1))]
fns = [l.strip() for l in open(sys.argv[1]) if l.strip()]
pairs = collections.OrderedDict(); bad_align = []
for a in fns:
    pm = int(a, 16); mw = F.TWIN[pm]
    cp, cw = calls('pm', pm), calls('mw3', mw)
    up = [c for c in cp if not mapped('pm', c)]; uw = [c for c in cw if not mapped('mw3', c)]
    if len(up) != len(uw): bad_align.append(a); continue
    for x, y in zip(up, uw): pairs.setdefault((x, y), []).append(a)
same = diff = 0
res = []
for (p, m), users in pairs.items():
    fl = feat_cmp(p, m)
    if fl: diff += 1; res.append((hex(p), hex(m), fl, users[:3], len(users)))
    else: same += 1
print('functions', len(fns), 'misaligned', len(bad_align), bad_align[:10])
print('unmapped callee pairs', len(pairs), 'same', same, 'differ', diff)
for r in res: print(*r)
