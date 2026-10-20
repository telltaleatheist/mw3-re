# Exploration script (subagent, 2026-10-19) for interp.zbd (docs/interp.md):
# dump = decode the archive, cmds/asmcmds = list the dispatcher's commands.
import re,sys
import os; ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
sys.path.insert(0,ROOT + '/tools/analysis'); import pe
t=open(ROOT + '/analysis-output/decomp/0x591300.c').read()
pat=re.compile(r"(?:strncmp\(\w+,|FUN_00595240\()\(?(?:char \*)?\)?&?(?:s_\w+_|PTR_DAT_|DAT_|PTR_s_\w+_)([0-9a-f]{8}),(0x[0-9a-f]+|\d+)\)")
ms=list(pat.finditer(t))
ARG={'FUN_00595e70':'s','FUN_00595ef0':'f','FUN_00595f10':'i','FUN_00595ea0':'b'}
for i,m in enumerate(ms):
    a=int(m.group(1),16); n=int(m.group(2),0)
    end=ms[i+1].start() if i+1<len(ms) else len(t)
    body=t[m.end():end]
    # only up to 'break;' / next case at outer level heuristics
    calls=re.findall(r'FUN_[0-9a-f]{8}',body)
    args=''.join(ARG[c] for c in calls if c in ARG)
    other=[c[4:].lstrip('0') for c in calls if c not in ARG and c not in('FUN_005952d0','FUN_004108f0','FUN_00596090')]
    err=calls.count('FUN_005952d0')
    name=pe.cstr(a)
    flag='' if n==len(name) else ' PREFIX(%d of %d: "%s")'%(n,len(name),name[:n])
    print(f'{name:40s} {args:10s} 0x{" 0x".join(dict.fromkeys(other))}{flag}')
