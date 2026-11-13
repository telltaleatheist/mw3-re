"""The order-aware compare run back over the compare, slot and layout folds (docs/port-audit-pm.md, method):
each fold's logic tokens (pm_shape_compare.shape) as multisets, float constants as sets of values; prints the counts
and writes the folds whose constants, immediates or calls differ to OUT.json.

    pm_shape_back.py OUT.json
"""
import sys,re,json,difflib,collections; sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
import pm_feature_compare as F, pm_shape_compare as P
COMPARE=re.compile(r'feature compare|slot pairing|layout-shift rule|deleting destructor')
rows=[l.rstrip('\n').split('\t') for l in open('tools/analysis/port_audit_labels_pm.tsv') if l.count('\t')>=2]
folds=[(int(a,16),r) for a,c,r in rows if c=='folded' and COMPARE.search(r)]
def mwof(a,r):
  if a in F.TWIN: return F.TWIN[a]
  m=re.search(r"MW3(?:'s| vtable 0x[0-9a-f]+'s| 0x[0-9a-f]+ \+0x[0-9a-f]+)? (0x[0-9a-f]{8})",r)
  return int(m.group(1),16) if m else None
def toks(game,a):
  out=collections.Counter(); calls=collections.Counter()
  for _,x in P.shape(game,a):
    if x.startswith('call'): calls[x]+=1; continue
    p=x.split()
    if len(p)==2 and x.startswith('f'):
      try: out['fconst '+str(round(abs(float(p[1])),5))]=1; continue
      except ValueError: pass
    if x.startswith('f'): 
      if x in ('fsqrt',): out[x]+=1
      continue
    if len(p)==2 and p[0] in ('cmp','mov','shl','shr','sar','and','or','imul','xor') and p[1] not in ('0x0','0x1','I'):
      out[x]+=1
  return out,calls
res={'same':0,'calls':[],'other':[],'skip':0}
for a,r in folds:
  mw=mwof(a,r)
  if not mw or a not in F.RANGES['pm'] or mw not in F.RANGES['mw3']: res['skip']+=1; continue
  tw,cw=toks('mw3',mw); tp,cp=toks('pm',a)
  d=[(k,tw[k],tp[k]) for k in set(tw)|set(tp) if tw[k]!=tp[k]]
  dc=sum(cw.values())!=sum(cp.values()) or any(cw[k]!=cp[k] for k in set(cw)|set(cp) if 'unmapped' not in k)
  if d: res['other'].append(('0x%08x'%a,'0x%08x'%mw,sorted(d)[:6]))
  elif dc: res['calls'].append(('0x%08x'%a,'0x%08x'%mw,sorted((k,cw[k],cp[k]) for k in set(cw)|set(cp) if cw[k]!=cp[k])[:4]))
  else: res['same']+=1
print('folds',len(folds),'aligned (tokens equal)',res['same'],'constants/immediates differ',len(res['other']),'only calls differ',len(res['calls']),'skipped',res['skip'])
json.dump(res,open(sys.argv[1],'w'),indent=0)
