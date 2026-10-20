# Exploration script (subagent, 2026-10-19), kept as a starting point for a real
# test: runs the original zClass code / read_tree in Unicorn. See docs/zclass-world.md.
import struct,sys
import os; ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
sys.path.insert(0,ROOT + '/tests/differential')
from original import Original
c=Original()
heap={}
def alloc(n):
    a=c.alloc(max(n,4)+16); heap[a]=n; return a
def calloc(cpu):
    n=cpu.arg(0)*cpu.arg(1); a=alloc(n); c.write(a,b'\0'*n); return a
def realloc(cpu):
    p,n=cpu.arg(0),cpu.arg(1); a=alloc(n)
    if p: c.write(a,c.read(p,min(heap[p],n)))
    return a
def sprintf(cpu):
    dst,fmt,s=cpu.arg(0),cpu.arg(1),cpu.arg(2)
    assert c.read(fmt,3)==b'%s\0'
    raw=bytes(c.uc.mem_read(s,64)).split(b'\0')[0]; c.write(dst,raw+b'\0'); return len(raw)
freed=[]
c.stub_import(0x5bb60c,calloc); c.stub_import(0x5bb610,realloc)
c.stub_import(0x5bb664,lambda cpu: alloc(cpu.arg(0))); c.stub_import(0x5bb65c,lambda cpu: freed.append(cpu.arg(0)))
c.stub_import(0x5bb658,sprintf)
fatal=[]
c.stub_function(0x50da70,lambda cpu: fatal.append(1))
def u(a): return struct.unpack('<I',c.read(a,4))[0]
def i(a): return struct.unpack('<i',c.read(a,4))[0]
c.u32(0x5ea830,3)        # chunk size 3 to exercise growth
c.u32(0x72f338,100)      # frame
from unicorn.x86_const import UC_X86_REG_EAX
def call(a,ecx=0,edx=0,st=()):
    c.call(a,ecx,edx,st); return c.uc.reg_read(UC_X86_REG_EAX)
nodes=[call(0x4fb740) for _ in range(5)]
print('nodes',[hex(n) for n in nodes],'count',u(0x60bf3c),'total',u(0x60bf34),'chunks',u(0x60bf30),'free',i(0x5ea834))
n=nodes[0]
print('name',c.read(n,20),'flags %#x'%u(n+0x24),'2c %#x'%u(n+0x2c),'30 %#x'%u(n+0x30),'34',u(n+0x34),'44',u(n+0x44),'4c',i(n+0x4c),i(n+0x50),'c0',u(n+0xc0),'c4',u(n+0xc4),'cc %#x'%u(n+0xcc),'d0 %#x'%u(n+0xd0))
d=u(n+0x38); print('o3d',c.read(d,0x90).hex())
item=u(n+0xcc); print('item',[hex(u(item+k)) for k in (0,4,8,12)])
# lists 6 and 7
for L,base in ((6,0x60be4c),(7,0x60bea0)):
    h=u(base); seq=[]
    while h: seq.append(nodes.index(u(h)) if u(h) in nodes else hex(u(h))); h=u(h+8)
    print('list',L,seq,'tail',hex(u(base+4)),'dirty',u(base+8))
# add child: 0 parent of 1, 0 and 2 parents of 3
print('add',call(0x4eb9b0,nodes[0],nodes[1]),call(0x4eb9b0,nodes[0],nodes[3]),call(0x4eb9b0,nodes[2],nodes[3]),call(0x4eb9b0,nodes[3],nodes[4]))
for k,nd in enumerate(nodes):
    print(k,'flags %#x 2c %#x par %d ch %d'%(u(nd+0x24),u(nd+0x2c),u(nd+0x54),u(nd+0x5c)))
print('self',call(0x4eb9b0,nodes[0],nodes[0]),'null',call(0x4eb9b0,0,nodes[0]))
print('remove',call(0x4ebb40,nodes[2],nodes[3]))
for k,nd in enumerate(nodes):
    print(k,'flags %#x par %d ch %d'%(u(nd+0x24),u(nd+0x54),u(nd+0x5c)))
# free slot by pointer
call(0x4f8de0,nodes[4]); print('after free free=',i(0x5ea834),'count',u(0x60bf3c),'d0 %#x'%u(nodes[4]+0xd0))
nn=call(0x4fb740); print('realloc same slot', nn==nodes[4], 'free', i(0x5ea834))
print('fatal',fatal)
