# peimports.py <pe>: print each imported DLL with its functions (PE32)
import struct, sys
b = open(sys.argv[1], 'rb').read()
pe = struct.unpack_from('<I', b, 0x3c)[0]
nsec = struct.unpack_from('<H', b, pe + 6)[0]
optsz = struct.unpack_from('<H', b, pe + 20)[0]
opt = pe + 24
imp_rva = struct.unpack_from('<I', b, opt + 104)[0]
secs = []
for i in range(nsec):
    o = opt + optsz + i * 40
    name = b[o:o+8].rstrip(b'\0').decode()
    vsize, va, rsize, raw = struct.unpack_from('<IIII', b, o + 8)
    secs.append((name, va, max(vsize, rsize), raw))
    print('section', name, hex(va), vsize, rsize)
def off(rva):
    for _, va, sz, raw in secs:
        if va <= rva < va + sz: return rva - va + raw
    raise ValueError(hex(rva))
def cstr(o): return b[o:b.index(b'\0', o)].decode()
d = off(imp_rva)
while True:
    oft, _, _, name, ft = struct.unpack_from('<IIIII', b, d)
    if not name: break
    funcs = []
    t = off(oft or ft)
    while True:
        e = struct.unpack_from('<I', b, t)[0]
        if not e: break
        funcs.append(f'#{e & 0xffff}' if e & 0x80000000 else cstr(off(e) + 2))
        t += 4
    print(f'\n{cstr(off(name))} ({len(funcs)}): ' + ', '.join(funcs))
    d += 20
