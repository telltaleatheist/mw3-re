# imports_in.py <lo> <hi> [...]: the imported functions called from code in the given ranges, by caller.
# Run from the repo root.
import bisect, json, re, struct, sys

b = open('game/mw3-1.2/Mech3.exe', 'rb').read()
pe = struct.unpack_from('<I', b, 0x3c)[0]
nsec = struct.unpack_from('<H', b, pe + 6)[0]
optsz = struct.unpack_from('<H', b, pe + 20)[0]
opt = pe + 24
imp_rva = struct.unpack_from('<I', b, opt + 104)[0]
secs = []
for i in range(nsec):
    o = opt + optsz + i * 40
    vsize, va, rsize, raw = struct.unpack_from('<IIII', b, o + 8)
    secs.append((va, max(vsize, rsize), raw))


def off(rva):
    for va, sz, raw in secs:
        if va <= rva < va + sz:
            return rva - va + raw
    raise ValueError(hex(rva))


def cstr(o):
    return b[o:b.index(b'\0', o)].decode()


iat = {}
d = off(imp_rva)
while True:
    oft, _, _, name, ft = struct.unpack_from('<IIIII', b, d)
    if not name:
        break
    dll = cstr(off(name))
    t, slot = off(oft or ft), ft
    while True:
        e = struct.unpack_from('<I', b, t)[0]
        if not e:
            break
        iat[0x400000 + slot] = dll + '!' + (f'#{e & 0xffff}' if e & 0x80000000 else cstr(off(e) + 2))
        t += 4
        slot += 4
    d += 20
# thunks: jmp dword ptr [iat]
thunks = {}
lines = open('analysis-output/mech3.asm').read().split('\n')
for line in lines:
    m = re.match(r'\s*([0-9a-f]+):\s+jmp\s+dword ptr \[0x([0-9a-f]+)\]', line)
    if m and int(m.group(2), 16) in iat:
        thunks[int(m.group(1), 16)] = iat[int(m.group(2), 16)]
fm = json.load(open('analysis-output/func2file.json'))
starts = sorted(int(k, 16) for k in fm)
ranges = [(int(sys.argv[i], 16), int(sys.argv[i + 1], 16)) for i in range(1, len(sys.argv), 2)]
out = {}
for line in lines:
    m = re.match(r'\s*([0-9a-f]+):\s+call\s+(?:dword ptr \[0x([0-9a-f]+)\]|0x([0-9a-f]+))', line)
    if not m:
        continue
    a = int(m.group(1), 16)
    if not any(lo <= a < hi for lo, hi in ranges):
        continue
    name = iat.get(int(m.group(2), 16)) if m.group(2) else thunks.get(int(m.group(3), 16))
    if name:
        f = starts[bisect.bisect_right(starts, a) - 1]
        out.setdefault(f, set()).add(name)
for f in sorted(out):
    print(f'{f:#x} ' + ', '.join(sorted(out[f])))
