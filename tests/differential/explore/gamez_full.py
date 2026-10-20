# Exploration script (subagent, 2026-10-19): walks gamez.zbd in the exe's read
# order (docs/gamez.md). A starting point for the gamez port's tests.
import struct, sys, collections
import os; ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
G=ROOT + '/game/mw3-1.2/zbd/'
U=lambda b,o,f='<i': struct.unpack_from(f,b,o)[0]
def walk(m):
    b=open(G+m+'/gamez.zbd','rb').read()
    sig,ver,tc,to,mo,mdo,nas,nlf,no=struct.unpack_from('<9i',b,0)
    asz,cnt,_,_=struct.unpack_from('<4i',b,mo); mat0=mo+16
    textured=[b[mat0+i*0x2c+1]&1 for i in range(asz)]
    # models
    o=mdo; masz,mcnt,mfree=struct.unpack_from('<3i',b,o); o+=12
    hdrs=[b[o+i*0x60:o+i*0x60+0x60] for i in range(masz)]; o+=masz*0x60
    st=collections.Counter()
    nz_noparent=0
    for i,h in enumerate(hdrs):
        pc,poly,vert,nrm,morph,light=struct.unpack_from('<6i',h,0xc)
        if pc<1 and any(h[:0x5c]): nz_noparent+=1
        if i<mcnt:
            off=U(h,0x5c,'<I')
            if off!=o: st['offset_mismatch']+=1
        elif U(h,0x5c)!=(i+1 if i+1<masz else -1): st['free_link_odd']+=1
        o+=12*vert+12*nrm+12*morph
        if light>0:
            L=[b[o+j*0x4c:o+j*0x4c+0x4c] for j in range(light)]; o+=0x4c*light
            st['lights']+=light
            for l in L: o+=12*U(l,0xc)
        if poly>0:
            P=[b[o+j*0x24:o+j*0x24+0x24] for j in range(poly)]; o+=0x24*poly
            for p in P:
                fl=U(p,0,'<I'); k=fl&0xff; mi=U(p,0x1c)
                if k:
                    o+=4*k
                    if fl&0x200: o+=4*k
                    if (U(p,0xc)!=0)!=bool(fl&0x200): st['nrm_ptr_vs_flag']+=1
                tex=textured[mi]
                if tex: o+=8*k
                if (U(p,0x10)!=0)!=bool(tex): st['uvptr_vs_textured']+=1
                if k==0: st['k0']+=1
                o+=12*k
                if U(p,0x14)==0: st['nocolptr']+=1
                if not(0<=mi<cnt): st['bad_mat']+=1
                st['polys']+=1
    print(m,'models',masz,mcnt,mfree,'end',hex(o),'nodes_off',hex(no),'OK' if o==no else 'MISMATCH','nonzero_noparent',nz_noparent,dict(st))
    # nodes
    o=no
    N=[b[o+i*0xd4:o+i*0xd4+0xd4] for i in range(nas)]; o+=nas*0xd4
    types=collections.Counter(); live=0; empt=[]
    sizes={1:0x1e8,2:0xbc,3:0xf8,4:0x1c,5:0x90,6:0x50,9:0xd0,10:0x94}
    firstzero=None; model_idx_zero=0
    for i,n in enumerate(N):
        t=U(n,0x34); pc=U(n,0x54); cc=U(n,0x60-4)
        if firstzero is None and not any(n[:36]): firstzero=i
        ret=0
        if t not in sizes and t!=0: print('unknown type',t,i); return
        if t:
            types[t]+=1; ret=1
            d=b[o:o+sizes[t]]; o+=sizes[t]
            if t==2:
                lc=U(d,0xa0); sc=U(d,0xac); xc=U(d,0x88); zc=U(d,0x8c)
                o+=4*max(lc,0)+4*max(sc,0)
                for _ in range(zc*xc):
                    o+=0x48; nc=struct.unpack_from('<h',b,o-0x48+0x3a)[0]
                    if nc>0: o+=4*nc
                print('  world',i,'lights',lc,'sounds',sc,'partitions',xc,'x',zc)
            if t==9 and U(d,200)>0: o+=4*U(d,200)
            if t==10 and U(d,0x8c)>0: o+=4*U(d,0x8c)
            if t==1: print('  camera',i,[U(d,j) for j in (0,4,8,0xc)])
        elif any(n[:0xd0]): empt.append((i,n[:36].split(b'\0')[0],pc,cc,hex(U(n,0xd0,'<I'))))
        if pc>0: o+=4*pc; ret=1
        if cc>0: o+=4*cc; ret=1
        if ret: live+=1
    print(m,'nodes',nas,'last_free',nlf,'firstzero',firstzero,'types',dict(types),'live',live,'end',hex(o),'filelen',hex(len(b)),'OK' if o==len(b) else 'MISMATCH')
    print('  empty nodes',len(empt),empt[:4])
    tails=collections.Counter((U(n,0xd0,'<I')>>24) for n in N[:firstzero])
    print('  +0xd0 top bytes of used nodes',dict(tails))
for m in sys.argv[1:]: walk(m)
