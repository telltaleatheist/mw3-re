# pe.py: read 32-bit values from Mech3.exe by virtual address. Usage: pe.py vt <addr> <count> [cmp_addr]
import struct, sys
from pathlib import Path
b = (Path(__file__).resolve().parents[2] / 'game' / 'mw3-1.2' / 'Mech3.exe').read_bytes()
pe = struct.unpack_from('<I', b, 0x3c)[0]; n = struct.unpack_from('<H', b, pe + 6)[0]; osz = struct.unpack_from('<H', b, pe + 20)[0]
secs = []
for i in range(n):
    o = pe + 24 + osz + i * 40
    vs, va, rs, raw = struct.unpack_from('<IIII', b, o + 8); secs.append((va, raw, rs))
def u32(va):
    rva = va - 0x400000
    for sva, raw, rs in secs:
        if sva <= rva < sva + rs: return struct.unpack_from('<I', b, rva - sva + raw)[0]
    return None
def cstr(va):
    rva = va - 0x400000
    for sva, raw, rs in secs:
        if sva <= rva < sva + rs:
            o = rva - sva + raw; return b[o:b.index(b'\0', o)].decode('latin1')
if __name__ == '__main__' and sys.argv[1] == 'vt':
    a = int(sys.argv[2], 16); cnt = int(sys.argv[3]); c = int(sys.argv[4], 16) if len(sys.argv) > 4 else None
    for i in range(cnt):
        v = u32(a + i * 4); w = u32(c + i * 4) if c else None
        print(f'{i*4:#05x} {v:#010x}' + ('' if w is None else ('   same' if v == w else f'   OVERRIDES {w:#010x}')))
