# Exploration script (subagent, 2026-10-19), kept as a starting point for a real
# test: runs the original zClass code / read_tree in Unicorn. See docs/zclass-world.md.
import struct,sys
sys.path.insert(0,'tests/differential')
from original import Original
import test_mechlib as tm
from unicorn.x86_const import UC_X86_REG_EAX
data=tm.MECHLIB.read_bytes()
ents=[e for e in tm.entries(data) if e[0].endswith('.flt')]
total=0; nodes_total=0; diffs={}
for name,start,length in ents:
    c=Original(); c.uc.mem_map(tm.BIG,0x4000000)
    st={'next':tm.BIG,'pos':start}
    sizes={}
    def alloc(n):
        a=st['next']; st['next']+=(max(n,1)+0xf)&~0xf; sizes[a]=n; return a
    def rf(cpu):
        h,buf,n,out=cpu.arg(0),cpu.arg(1),cpu.arg(2),cpu.arg(3)
        ch=data[st['pos']:st['pos']+n]; st['pos']+=len(ch); cpu.write(buf,ch)
        if out: cpu.u32(out,len(ch))
        return 1
    def calloc(cpu):
        n=cpu.arg(0)*cpu.arg(1); a=alloc(n); cpu.write(a,bytes(n)); return a
    def realloc(cpu):
        p,n=cpu.arg(0),cpu.arg(1); a=alloc(n)
        if p: cpu.write(a,cpu.read(p,min(sizes[p],n)))
        return a
    def sprintf(cpu):
        raw=bytes(cpu.uc.mem_read(cpu.arg(2),64)).split(b'\0')[0]; cpu.write(cpu.arg(0),raw+b'\0'); return 0
    def pool(cpu):
        a=alloc(0x60); cpu.write(a,bytes(0x60)); return a
    c.stub_import(tm.IAT_READFILE,rf,pop_bytes=20); c.stub_import(tm.IAT_MALLOC,lambda cpu: alloc(cpu.arg(0)))
    c.stub_import(0x5bb60c,calloc); c.stub_import(0x5bb610,realloc); c.stub_import(0x5bb658,sprintf)
    c.stub_function(tm.MODEL_POOL,pool); c.stub_function(0x50da70,lambda cpu: 1/0)
    c.u32(0x72f338,100)
    lib=alloc(0x100); c.write(lib,bytes(0x100)); c.call(0x4fcdc0,ecx=lib,stack_args=(tm.HANDLE,0)); root=c.uc.reg_read(UC_X86_REG_EAX)
    assert st['pos']==start+length,(name,st['pos']-start,length)
    u=lambda a: struct.unpack('<I',c.read(a,4))[0]
    # walk tree and compare node bytes with file bytes
    pos=[start]
    def walk(nd, parent):
        global nodes_total
        f=data[pos[0]:pos[0]+0xd0]; pos[0]+=0xd0+0x90
        m=c.read(nd,0xd0)
        nodes_total+=1
        for off in range(0,0xd0,4):
            a,b=f[off:off+4],m[off:off+4]
            if a!=b:
                key=off
                if off==0x24: key=('flags',hex(struct.unpack('<I',a)[0]^struct.unpack('<I',b)[0]))
                diffs[key]=diffs.get(key,0)+1
        hasm=struct.unpack_from('<I',f,0x3c)[0]
        if hasm:
            pp=pos[0]; polys,verts,norms,morphs=struct.unpack_from('<4I',data,pp+0x10)
            p=pp+0x5c+12*(verts+norms+morphs); t=p; p+=0x24*polys
            for i in range(polys):
                fl,_,_,np_,up,cp=struct.unpack_from('<IiIIII',data,t+0x24*i); k=fl&0xff; p+=4*k
                if fl&0x200 and np_: p+=4*k
                if up: p+=8*k
                if cp: p+=12*k
            pos[0]=p
        n=u(nd+0x5c); assert n==struct.unpack_from('<I',f,0x5c)[0]
        assert u(nd+0x54)==(1 if parent else 0)
        if parent: assert u(u(nd+0x58))==parent
        for k in range(n): walk(u(u(nd+0x60)+4*k), nd)
    walk(root,0)
    total+=1
print(total,'trees',nodes_total,'nodes; differing dwords (offset: count):')
for k,v in sorted(diffs.items(),key=str): print(' ',k if isinstance(k,tuple) else hex(k),v)
