# Exploration script (subagent, 2026-10-19): world light nodes (class 9), model
# flags and model lights in each gamez.zbd, for the lighting map.
import struct, sys, collections
G='game/mw3-1.2/zbd/'
U=lambda b,o,f='<i': struct.unpack_from(f,b,o)[0]
def walk(m):
    b=open(G+m+'/gamez.zbd','rb').read()
    sig,ver,tc,to,mo,mdo,nas,nlf,no=struct.unpack_from('<9i',b,0)
    asz,cnt,_,_=struct.unpack_from('<4i',b,mo); mat0=mo+16
    textured=[b[mat0+i*0x2c+1]&1 for i in range(asz)]
    o=mdo; masz,mcnt,mfree=struct.unpack_from('<3i',b,o); o+=12
    hdrs=[b[o+i*0x60:o+i*0x60+0x60] for i in range(masz)]; o+=masz*0x60
    mflags=collections.Counter(); ml=collections.Counter(); pflags=collections.Counter()
    for i,h in enumerate(hdrs[:mcnt]):
        mflags[hex(U(h,8,'<I'))]+=1
        pc,poly,vert,nrm,morph,light=struct.unpack_from('<6i',h,0xc)
        o+=12*vert+12*nrm+12*morph
        if light>0:
            L=[b[o+j*0x4c:o+j*0x4c+0x4c] for j in range(light)]; o+=0x4c*light
            for l in L:
                ml[(U(l,0),U(l,4),round(U(l,8,'<f'),2),U(l,0xc),tuple(round(x,2) for x in struct.unpack_from('<3f',l,0x1c)),hex(U(l,0x2a,'<H')),tuple(round(x,2) for x in struct.unpack_from('<3f',l,0x30)),U(l,0x3c),tuple(round(x,2) for x in struct.unpack_from('<3f',l,0x40)))]+=1
                o+=12*U(l,0xc)
        if poly>0:
            P=[b[o+j*0x24:o+j*0x24+0x24] for j in range(poly)]; o+=0x24*poly
            for p in P:
                fl=U(p,0,'<I'); k=fl&0xff; mi=U(p,0x1c)
                pflags[hex(fl&~0xff)]+=1
                if k:
                    o+=4*k
                    if fl&0x200: o+=4*k
                if textured[mi]: o+=8*k
                o+=12*k
    print(m,'model flags',dict(mflags)); print('  poly flags',dict(pflags))
    for k,v in ml.items(): print('  model light',v,k)
    o=no
    N=[b[o+i*0xd4:o+i*0xd4+0xd4] for i in range(nas)]; o+=nas*0xd4
    sizes={1:0x1e8,2:0xbc,3:0xf8,4:0x1c,5:0x90,6:0x50,9:0xd0,10:0x94}
    for i,n in enumerate(N):
        t=U(n,0x34); pc=U(n,0x54); cc=U(n,0x60-4)
        if t:
            d=b[o:o+sizes[t]]; o+=sizes[t]
            if t==2:
                lc=U(d,0xa0); sc=U(d,0xac); xc=U(d,0x88); zc=U(d,0x8c)
                print('  world fog type',U(d,0x10),'col',struct.unpack_from('<3f',d,0x14),'range',struct.unpack_from('<4f',d,0x20),'dens',U(d,0x30,'<f'))
                o+=4*max(lc,0)+4*max(sc,0)
                for _ in range(zc*xc):
                    o+=0x48; nc=struct.unpack_from('<h',b,o-0x48+0x3a)[0]
                    if nc>0: o+=4*nc
            if t==9:
                f=struct.unpack_from('<52f',d,0)
                print('  light node',i,n[:36].split(b'\0')[0],'nodeflags',hex(U(n,0x24,'<I')),'flags',hex(U(d,0xb0,'<I')))
                print('    orient',f[0:3],'trans',f[3:6],'diffuse',f[39],'ambient',f[40],'color',f[41:44],'range',f[45:50],'view',f[33:36])
                if U(d,200)>0: o+=4*U(d,200)
            if t==10 and U(d,0x8c)>0: o+=4*U(d,0x8c)
        if pc>0: o+=4*pc
        if cc>0: o+=4*cc
for m in sys.argv[1:]: walk(m)
