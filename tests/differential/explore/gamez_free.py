# Exploration script (subagent, 2026-10-19): walks gamez.zbd in the exe's read
# order (docs/gamez.md). A starting point for the gamez port's tests.
import struct,sys
G='game/mw3-1.2/zbd/'
U=lambda b,o,f='<i': struct.unpack_from(f,b,o)[0]
for m in sys.argv[1:]:
    b=open(G+m+'/gamez.zbd','rb').read()
    h=struct.unpack_from('<9i',b,0); nas,nlf,no=h[6],h[7],h[8]
    N=[b[no+i*0xd4:no+i*0xd4+0xd4] for i in range(nas)]
    typed=set(i for i,n in enumerate(N) if U(n,0x34)!=0)
    empty=set(i for i,n in enumerate(N) if U(n,0x34)==0 and n[0]!=0)
    zero=set(i for i,n in enumerate(N) if n[0]==0)
    seen=[];k=nlf;vis=set()
    while k!=0xffffff and k not in vis and 0<=k<nas:
        vis.add(k);seen.append(k);k=U(N[k],0xd0,'<I')&0xffffff
    print(m,'typed',len(typed),'empty(named,type0)',len(empty),'zero',len(zero),'freelist len',len(seen),'term',hex(k),
          'covers empty',empty<=vis,'covers zero',zero<=vis,'typed in list',len(typed&vis))
    # does any empty node appear as a parent/child of a typed node?
    refs=set()
    o=None
    # empty nodes with parents/children
    print('  empty with parents/children',sum(1 for i in empty if U(N[i],0x54)>0 or U(N[i],0x5c)>0))
    print('  sample empty', [(i,N[i][:36].split(b'\0')[0], hex(U(N[i],0x24,'<I')), U(N[i],0x3c)) for i in sorted(empty)[:3]])
