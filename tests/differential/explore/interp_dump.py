# Exploration script (subagent, 2026-10-19) for interp.zbd (docs/interp.md):
# dump = decode the archive, cmds/asmcmds = list the dispatcher's commands.
#   interp_dump.py <interp.zbd> [out.txt]
# The decoded scripts (the game's own text: not for the repo) go to out.txt,
# by default a file in the system's temp folder, never the current directory.
import struct,sys,datetime,collections,os,tempfile
OUT=sys.argv[2] if len(sys.argv)>2 else os.path.join(tempfile.gettempdir(),'mw3-interp-dump','scripts.txt')
os.makedirs(os.path.dirname(os.path.abspath(OUT)),exist_ok=True)
d=open(sys.argv[1],'rb').read()
sig,ver,n=struct.unpack_from('<III',d,0)
print(hex(sig),ver,n)
ents=[]
for i in range(n):
    o=12+i*128
    name=d[o:o+120].split(b'\0')[0].decode()
    ts,st=struct.unpack_from('<II',d,o+120)
    ents.append((name,ts,st))
cmds=collections.Counter()
out=open(OUT,'w')
for name,ts,st in ents:
    print(name,datetime.datetime.utcfromtimestamp(ts),st)
    o=st; out.write('== %s\n'%name)
    while True:
        sz,=struct.unpack_from('<I',d,o); o+=4
        if sz==0: break
        ac,=struct.unpack_from('<I',d,o); o+=4
        b=d[o:o+sz]; o+=sz
        parts=b[:-1].split(b'\0')
        out.write(' | '.join(p.decode('latin1') for p in parts)+'\n')
        cmds[parts[0].decode()]+=1
print('end',o,len(d))
for k,v in sorted(cmds.items()): print(k,v)
out.close()
print('scripts written to',OUT)
