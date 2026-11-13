"""setsweep.py GAME AUDIT.json LABELS.tsv [MW3LABELS.tsv] SRC: folded setters whose written global or field the port never writes."""
import json, re, sys
from pathlib import Path
game, audit, labels = sys.argv[1], sys.argv[2], sys.argv[3]
mw3l = sys.argv[4] if game == 'pm' else None
src = Path(sys.argv[-1])
rows = json.load(open(audit)); by = {r['addr']: r for r in rows}
lab = {}
for f in ([mw3l] if mw3l else []) + [labels]:
    for l in open(f):
        p = l.rstrip('\n').split('\t', 2)
        if len(p) == 3: lab[p[0]] = (p[1], p[2])
dec = Path('analysis-output') / ('pm-decomp' if game == 'pm' else 'decomp')
txt = {}
for p in src.rglob('*'):
    if p.suffix in ('.cpp', '.h', '.inc'): txt[str(p)] = p.read_text(errors='replace')
alltxt = '\n'.join(txt.values())
# globals written in the shipped data the port mirrors: named anywhere as 0x00XXXXXX or 0xXXXXXX
def named(a): return re.search(r'0x(00)?%s\b' % a, alltxt)
readers = {}
gl = re.compile(r'\b_?DAT_00([0-9a-f]{6})\s*=(?!=)')
fl = re.compile(r'\*\([^)]*\)\s*\((?:in_ECX|param_1)\s*\+\s*(0x[0-9a-f]+)\)\s*=(?!=)|(?:in_ECX|param_1)\[(0x[0-9a-f]+|\d+)\]\s*=(?!=)')
# readers of each global, for the report
for p in dec.glob('0x*.c'):
    t = p.read_text(errors='replace')
    for m in re.finditer(r'\b_?DAT_00([0-9a-f]{6})\b(?!\s*=[^=])', t): readers.setdefault(m.group(1), set()).add(p.stem)
out = []
for r in rows:
    a = r['addr']
    L = lab.get(a) or (lab.get(r.get('mw3', '')) if r.get('same') else None)
    shp = r.get('shape') or []
    folded = (L and L[0] == 'folded') or (not L and shp and shp[0] == 'folded')
    if not folded or r['status'] == 'ported' or r['lines'] > 16: continue
    reason = (L[1] if L else shp[1])
    if not re.search(r'sets members|setter|small helper|one-line|helper', reason): continue
    f = dec / f'0x{int(a,16):x}.c'
    if not f.exists(): continue
    t = f.read_text(errors='replace')
    gs = sorted(set(gl.findall(t))); fs = sorted({x or y for x, y in fl.findall(t)})
    miss_g = [g for g in gs if not named(g)]
    callers = [c for c in r['callers'] if c in by and by[c]['status'] == 'ported']
    files = {by[c]['ported_in'].split(' ')[0] for c in callers}
    miss_f = []
    for off in fs:
        o = int(off, 0) if off.startswith('0x') else int(off) * 4
        if not any(re.search(r'\+\s*0x0*%x\b|0x0*%x\b' % (o, o), txt.get(fn, '')) for fn in files): miss_f.append(hex(o))
    if miss_g or (miss_f and not gs):
        rd = {g: sorted(readers.get(g, set()) - {f.stem})[:5] for g in miss_g}
        out.append((a, r['lines'], miss_g, miss_f, rd, callers[:3]))
for o in out: print(*o, sep='\t')
print(len(out), 'flagged', file=sys.stderr)
