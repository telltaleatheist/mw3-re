import json, re, sys, collections
from pathlib import Path
A = Path('analysis-output')
am = json.load(open(A / 'pm-diff' / 'addrmap.json'))
inv = {int(v, 16): int(k, 16) for k, v in am['functions'].items()}
rows = {r['addr']: r for r in json.load(open(sys.argv[1]))}
mwrows = {r['addr']: r for r in json.load(open(sys.argv[2]))}
def strings(p):
    t = p.read_text(errors='replace') if p.exists() else ''
    return collections.Counter(re.findall(r'\bs_(\w+?)_[0-9a-f]{8}\b', t))
for l in open(sys.argv[3]):
    port, a, m, n, kinds, k, tests = (l.rstrip('\n').split('\t') + [''])[:7]
    if k != 'read': continue
    r = rows[a]; mr = mwrows.get(m)
    pc = collections.Counter('0x%08x' % inv.get(int(c, 16), int(c, 16)) + ('' if int(c, 16) in inv else '(PM)') for c in r['callees'])
    mc = collections.Counter(mr['callees']) if mr else collections.Counter()
    add, rem = pc - mc, mc - pc
    sp, sm = strings(A / 'pm-decomp' / f'0x{int(a,16):x}.c'), strings(A / 'decomp' / f'0x{int(m,16):x}.c')
    print(f'== {a} (MW3 {m}) {n}L {port.split("/")[-1]} [{kinds}]')
    if add: print('   calls added:', ' '.join(add)[:160])
    if rem: print('   calls gone :', ' '.join(rem)[:160])
    if sp - sm: print('   strings +  :', ' '.join(sp - sm)[:160])
    if sm - sp: print('   strings -  :', ' '.join(sm - sp)[:160])
