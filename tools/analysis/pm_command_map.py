"""pm_command_map.py OUT.json (PM audit, docs/port-audit-pm.md): MW3 and PM command ids by their CMD_ names, read from each exe's command registration
(MW3 0x004110d0: id in ecx; PM 0x00411980: id the last push); prints the map's differences."""
import sys,re,json; sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__))); import pm_feature_compare as F
def regs(g, fn):
  ins,_=F.asm(g); out={}; win=[]
  for at,mn,o in ins:
    if mn=='call' and o.startswith(hex(fn)):
      name=None; cid=None
      for _,m2,o2 in win:
        if m2=='push' and re.fullmatch(r'0x[0-9a-f]+',o2):
          s=F.string_key(g,int(o2,16))
          if s and s.startswith('str CMD_'): name=s[4:]
          elif g=='pm': cid=int(o2,16)
        if g=='mw3' and m2=='mov' and o2.startswith('ecx, 0x'): cid=int(o2[5:],16)
      if name and cid is not None: out[name]=cid
      win=[]
    elif mn=='call': win=[]
    else: win.append((at,mn,o))
  return out
mw,pm=regs('mw3',0x4110d0),regs('pm',0x411980)
m={mw[n]:pm.get(n) for n in mw}
json.dump({'mw3_to_pm':{hex(k):(hex(v) if v is not None else None) for k,v in sorted(m.items())},'mw3':mw,'pm':pm},open(sys.argv[1],'w'),indent=0)
print(len(mw),'mw3',len(pm),'pm')
print('gone in pm:',[n for n in mw if n not in pm]); print('new in pm:',[(hex(pm[n]),n) for n in pm if n not in mw])
sh={}
for k,v in sorted(m.items()):
  if v is not None: sh.setdefault(v-k,[]).append(k)
print('shifts:',{d:(hex(min(v)),hex(max(v)),len(v)) for d,v in sh.items()})
