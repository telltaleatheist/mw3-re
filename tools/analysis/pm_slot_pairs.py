"""pm_slot_pairs.py GROUP [JSON_DIR] (docs/port-audit-pm.md, method: paired by its slot) : the unlabelled table-only rows of a file group, each paired with the MW3 function in the same
slot of its MW3 twin vtable (pm-diff/vtables.json), the pair run through the feature compare, the MW3 function's
label and port. A row in several vtables shows the first."""
import json, re, sys, os, bisect, struct
sys.path.insert(0, 'tools/analysis')
S = sys.argv[2] if len(sys.argv) > 2 else os.path.dirname(os.path.abspath(__file__))  # where audit-check.json is
import pm_feature_compare as F
rows = {r['addr']: r for r in json.load(open(S + '/audit-check.json'))}
unl = set(re.findall(r'\| (0x[0-9a-f]{8}) \| \d+ \| unlabelled \|', open('docs/port-audit-pm.md').read()))
f2f = json.load(open('analysis-output/pm-func2file.json'))
vt = json.load(open('analysis-output/pm-diff/vtables.json'))  # mw3 -> pm
pmv = sorted(int(p, 16) for p in vt.values()); inv = {int(p, 16): int(m, 16) for m, p in vt.items()}
mlab = {l.split('\t')[0]: (l.split('\t')[1], l.split('\t')[2][:110]) for l in open('tools/analysis/port_audit_labels.tsv') if l.count('\t') >= 2}
mported = {}
for p in __import__('pathlib').Path('src').rglob('*'):
    if p.suffix in ('.cpp', '.h'):
        for n, line in enumerate(p.open(errors='replace'), 1):
            for m in re.finditer(r'Mech3\.exe 1\.2 @ 0x([0-9a-f]{8})', line): mported.setdefault('0x' + m.group(1), f'{p}:{n}')
mdoc = {}
for m in re.finditer(r'\| (0x[0-9a-f]{8}) \| \d+ \| ([^|]+?) \| ([^|]*)\|', open(os.environ.get('MW3_AUDIT_DOC', 'docs/port-audit.md')).read()):
    mdoc[m.group(1)] = (m.group(2).strip(), m.group(3).strip()[:110])
def u32(game, a):
    b = F.EXE[game](a, 4); return struct.unpack('<I', b)[0] if b else None
grp = sys.argv[1]
out = []
for a in sorted(unl):
    r = rows.get(a)
    if not r or r['callers'] or not r['tables']: continue
    if grp not in f2f.get(hex(int(a, 16)), ''): continue
    slot = int(r['tables'][0], 16); i = bisect.bisect_right(pmv, slot) - 1
    base = pmv[i]; off = slot - base; mb = inv[base]; mf = u32('mw3', mb + off)
    s = f'{a} {r["lines"]:4}L vt {base:#x}+{off:#x} <- MW3 {mb:#x}: {mf:#010x}' if mf else f'{a} vt {base:#x}+{off:#x} MW3 slot unreadable'
    if mf:
        k = '0x%08x' % mf
        lab = mlab.get(k); port = mported.get(k)
        fl = '?'
        if int(a, 16) in F.RANGES['pm'] and mf in F.RANGES['mw3']:
            fp, fw = F.features('pm', int(a, 16)), F.features('mw3', mf)
            fl = [x for x in sorted(set(fp) | set(fw)) if fp[x] != fw[x] and (x.startswith('call') or x.startswith('const') or x.startswith('field') or (x.startswith('imm') and int(x.split()[1], 16) > 1))]
            fl = 'same' if not fl else ' '.join(fl)
        s += f'\n    compare: {fl}\n    MW3: ' + (f'ported {port}' if port else (f'{mdoc[k][0]}: {mdoc[k][1]}' if k in mdoc else 'ported (not in the MW3 audit list)'))
    if r['named_in']: s += '\n    named: ' + ' | '.join(r['named_in'][:2])
    out.append(s)
print('\n'.join(out)); print(len(out), 'rows')
