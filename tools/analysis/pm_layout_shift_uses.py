"""The layout-shift rule's use check (docs/port-audit-pm.md, method): each matched immediate of the rule's pairs found
in the PM function, kept when it is an offset (add / sub / lea on a register) or a size before operator new; any other
use printed to be read. pm_layout_shift_uses.py FILE (pm_layout_shift.py's output)."""
import sys,re,bisect
sys.path.insert(0,'tools/analysis'); import pm_feature_compare as F
ins,idx=F.asm('pm')
ALLOC={'0x5ce8c6','0x5cf380'}  # operator new, malloc (PM)
def uses(addr,val):
    out=[]
    rng=[k for s,e in F.RANGES['pm'][addr] for k in range(bisect.bisect_left(idx,s),bisect.bisect_right(idx,e))]
    for j,k in enumerate(rng):
        ad,mn,ops=ins[k]
        bare=re.sub(r'\[[^\]]*\]','[]',ops)
        if not re.search(r'\b%s\b'%hex(val),bare): continue
        nxt=' '.join(ins[rng[x]][1]+' '+ins[rng[x]][2] for x in range(j+1,min(j+6,len(rng))))
        if mn in ('add','sub','lea') and re.match(r'e[a-z]{2}, ',ops) : kind='offset'
        elif mn=='push' and re.search(r'call\s+0x(5ce8c6|5cf380|5ce8c6)',nxt): kind='size(new)'
        elif mn=='mov' and ops.startswith('ecx') and 'rep' in nxt: kind='size(rep)'
        elif mn=='mov' and re.match(r'e[a-z]{2}, 0x',ops) and re.search(r'add\s+e[a-z]{2}, e|lea|\[e[a-z]{2} \+ e',nxt): kind='offset?(reg)'
        else: kind='OTHER'
        out.append((kind,hex(ad),mn,ops))
    return out
for line in open(sys.argv[1]):
    p=line.split()
    if len(p)<4: continue
    a=int(p[0],16); flagged=[]
    for pr in set(p[3:]):
        w,pm=[int(x,16) for x in pr.split('->')]
        for u in uses(a,pm):
            if u[0]!='offset' and not u[0].startswith('size'): flagged.append((pr,)+u)
    if flagged:
        print(p[0]); [print('   ',*f) for f in flagged]
