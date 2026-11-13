"""The callers' check of the pairs (docs/port-audit-pm.md, method; pm_pair_check.py the vtables'): a pair is suspect
when the PM function has at least 2 direct callers with twins and none of those twins calls the MW3 function directly
(MW3 may have inlined it: read before relabelling). On 2026-11-13: 10 of 700 checkable position-based pairs, 1 of
1,271 byte/call/string ones. Run from the repo root.

    pm_pair_callers.py [HOW...]   default 'link order', 'loose', 'structure'
"""
import sys,json,bisect,re
sys.path.insert(0,'tools/analysis');import pm_feature_compare as F
hows=set(sys.argv[1:]) or {'link order','loose','structure'}
def callmap(game):
    ins,idx=F.asm(game); calls={}
    for a,rs in F.RANGES[game].items():
        for s,e in rs:
            for k in range(bisect.bisect_left(idx,s),bisect.bisect_right(idx,e)):
                _,mn,o=ins[k]
                if mn=='call' and o.startswith('0x'): calls.setdefault(a,set()).add(int(o.split()[0],16))
    return calls
cp,cm=callmap('pm'),callmap('mw3')
callers={}
for a,cs in cp.items():
    for c in cs: callers.setdefault(c,set()).add(a)
lab={int(l.split('\t')[0],16):l.split('\t')[1] for l in open('tools/analysis/port_audit_labels_pm.tsv') if l.count('\t')>=2}
n=bad=0
for q in json.load(open('analysis-output/pm-diff/pairs.json')):
    if q.get('how') not in hows or not q.get('pm') or not q.get('mw3'): continue
    p,m=int(q['pm'],16),int(q['mw3'],16)
    tw=[F.TWIN[c] for c in callers.get(p,()) if c in F.TWIN]
    if len(tw)<2: continue
    n+=1
    hit=sum(1 for t in tw if m in cm.get(t,()))
    if hit==0:
        bad+=1; print('0x%08x mw3 0x%08x %-10s %-8s callers %d, none of their twins calls it'%(p,m,q['how'],lab.get(p,'-'),len(tw)))
print('checked',n,'suspect',bad,file=sys.stderr)
