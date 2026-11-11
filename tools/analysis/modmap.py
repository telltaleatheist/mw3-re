# modmap.py: assign every function to an original source file by link-order address ranges
#   modmap.py [func2file.json]
# Reads filemap.json and decomp/ from analysis-output/ and writes there (git-ignored) unless a path is given:
# never into the folder it happens to be run from.
import json, os, sys
AO = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'analysis-output')
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(AO, 'func2file.json')
fm = json.load(open(os.path.join(AO, 'filemap.json')))
files = sorted(((int(v[0], 16), int(v[-1], 16), k) for k, v in fm.items() if v and not k.endswith('.h')), key=lambda t: t[0])
funcs = sorted(int(f[:-2], 16) for f in os.listdir(os.path.join(AO, 'decomp')) if f.startswith('0x'))
size = {a: os.path.getsize(os.path.join(AO, 'decomp', f'{hex(a)}.c')) for a in funcs}
assign = {}
for a in funcs:
    owner = None
    for i, (lo, hi, k) in enumerate(files):
        if lo <= a <= hi: owner = k; break
        if a < lo:
            owner = f'(between {files[i-1][2] if i else "start"} and {k})'; break
    assign[a] = owner or f'(after {files[-1][2]})'
json.dump({hex(a): k for a, k in assign.items()}, open(OUT, 'w'), indent=0)
rows = {}
for a, k in assign.items():
    r = rows.setdefault(k, [a, a, 0, 0]); r[0] = min(r[0], a); r[1] = max(r[1], a); r[2] += 1; r[3] += size[a]
for k, (lo, hi, n, sz) in sorted(rows.items(), key=lambda kv: kv[1][0]):
    print(f'{hex(lo)}-{hex(hi)}  {n:5d} fns {sz//1024:6d} KB  {k}')
