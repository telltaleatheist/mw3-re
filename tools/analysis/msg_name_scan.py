"""Every MSG_ name in every .zrd of every .zbd under GAMEDIR against that game's Mech3Msg.dll name table (ZLocGetID's
sorted table, as zutl_msgdll.cpp reads it); the unknown ones print with where they are used. Both exes and the
port show an unknown name as itself (0x00575fb0 / zutl::localize). Run from the repo root.

    msg_name_scan.py GAMEDIR    e.g. game/pm
"""
import sys,struct,re,glob,subprocess,os,collections
G=sys.argv[1]
dll=open(os.path.join(G,'Mech3Msg.dll'),'rb').read()
u32=lambda o:struct.unpack_from('<I',dll,o)[0]; u16=lambda o:struct.unpack_from('<H',dll,o)[0]
pe=u32(0x3c); n=u16(pe+6); osz=u16(pe+20); opt=pe+24; base=u32(opt+28)
secs=[(u32(opt+osz+40*i+12),max(u32(opt+osz+40*i+8),u32(opt+osz+40*i+16)),u32(opt+osz+40*i+20)) for i in range(n)]
def off(rva):
    for va,sz,raw in secs:
        if va<=rva<va+sz: return raw+rva-va
def cstr(o): return dll[o:dll.index(b'\0',o)].decode('latin-1')
ex=off(u32(opt+96)); nn=u32(ex+0x18); funcs=off(u32(ex+0x1c)); names=off(u32(ex+0x20)); ords=off(u32(ex+0x24))
fn=[u32(funcs+4*u16(ords+2*i)) for i in range(nn) if cstr(off(u32(names+4*i)))=='ZLocGetID'][0]
code=off(fn)
for i in range(code,code+16):
    if dll[i]==0xe8: code=off(fn+i-code+5+struct.unpack_from('<i',dll,i+1)[0]); break
table=end=0
for i in range(code,code+0xc0):
    if not table and dll[i]==0xbb: table=u32(i+1)
    if dll[i]==0x81 and dll[i+1]==0xfb: end=u32(i+2)
ids=set()
for va in range(table,end,8):
    o=off(va-base); ids.add(cstr(off(u32(o)-base)))
uses=collections.defaultdict(set)
for z in sorted(glob.glob(os.path.join(G,'**','*.zbd'),recursive=True)):
    lst=subprocess.run([sys.executable,'tools/analysis/zrd.py',z],capture_output=True,text=True).stdout
    for name in re.findall(r'\S+\.zrd',lst):
        t=subprocess.run([sys.executable,'tools/analysis/zrd.py',z,name],capture_output=True,text=True).stdout
        for m in set(re.findall(r'"(MSG_\w+)"',t)):
            uses[m].add(os.path.relpath(z,G)+':'+name)
bad=sorted(m for m in uses if m not in ids)
print(G,': names in the DLL',len(ids),'| MSG_ names used',len(uses),'| unknown',len(bad))
for m in bad: print('  ',m,' <- ',', '.join(sorted(uses[m]))[:200])
