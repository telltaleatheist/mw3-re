# Exploration script (subagent, 2026-10-19) for interp.zbd (docs/interp.md):
# dump = decode the archive, cmds/asmcmds = list the dispatcher's commands.
import re,sys
sys.path.insert(0,'tools/analysis'); import pe
L=open(sys.argv[1]).read().splitlines()
H={'595e70':'s','595ef0':'f','595f10':'i','595ea0':'b'}
SKIP={'5952d0','4108f0','596090','595240'}
ev=[]
for l in L:
    m=re.match(r'\s*([0-9a-f]+):\s+(.*)',l)
    if not m: continue
    a,ins=m.groups()
    for imm in re.findall(r'0x(5f[2-4][0-9a-f]{3}|5e9[0-9a-f]{3}|5e16d8)\b',ins):
        s=pe.cstr(int(imm,16))
        if s and re.fullmatch(r'[A-Za-z][A-Za-z0-9]*',s): ev.append((a,'N',s))
    c=re.search(r'call\s+0x([0-9a-f]+)',ins)
    if c: ev.append((a,'C',c.group(1)))
cur=None
out=[]
for a,k,v in ev:
    if k=='N':
        if cur and cur[0]==v: continue
        cur=[v,a,'',[]]; out.append(cur)
    elif cur:
        if v in H: cur[2]+=H[v]
        elif v not in SKIP and v not in cur[3]: cur[3].append(v)
for n,a,args,calls in out:
    print(f'{a} {n:40s} {args:8s} {" ".join(calls)}')
