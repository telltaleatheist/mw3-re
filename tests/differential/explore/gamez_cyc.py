# Exploration script (subagent, 2026-10-19): walks gamez.zbd in the exe's read
# order (docs/gamez.md). A starting point for the gamez port's tests.
import struct,sys
import os; ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
G=ROOT + '/game/mw3-1.2/zbd/'
b=open(G+sys.argv[1]+'/gamez.zbd','rb').read()
h=struct.unpack_from('<9i',b,0); mo=h[4]
asz,cnt,_,ilast=struct.unpack_from('<4i',b,mo); o=mo+16+asz*0x2c
cyc=[i for i in range(cnt) if b[mo+16+i*0x2c+1]&4]
recs=[]
while len(recs)<len(cyc):
    c=struct.unpack_from('<iiffiiI',b,o); o+=0x1c; recs.append(c); o+=4*c[4]
for name,order in (('desc',cyc[::-1]),('asc',cyc)):
    print(name)
    for k,c in list(zip(order,recs))[:6]:
        p=struct.unpack_from('<I',b,mo+16+k*0x2c+0x24)[0]
        tex=struct.unpack_from('<I',b,mo+16+k*0x2c+0x10)[0]
        print('  mat',k,'cycle_ptr',hex(p),'tex',tex,'cyc',c[:6],'texmap_ptr',hex(c[6]),'diff',hex(c[6]-p))
