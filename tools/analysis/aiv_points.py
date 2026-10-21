import struct, sys
sys.path.insert(0, 'tools/analysis'); sys.path.insert(0, 'tests/differential')
import zrd
from pathlib import Path
def node(d, p):
    t, v = struct.unpack_from('<iI', d, p)
    if t == 1: return struct.unpack_from('<i', d, p+4)[0], p+8
    if t == 2: return struct.unpack_from('<f', d, p+4)[0], p+8
    if t == 3: return d[p+8:p+8+v].decode('latin1'), p+8+v
    out = []; p += 8
    for _ in range(v-1):
        x, p = node(d, p); out.append(x)
    return out, p
out = []
for a in sorted(Path('game/mw3-1.2/zbd').glob('*/reader*.zbd')):
    data = a.read_bytes()
    for n, s, k in zrd.entries(data):
        if n != 'aiv.zrd': continue
        tree, _ = node(data[s:s+k], 0)
        top = tree[0] if len(tree) == 1 and isinstance(tree[0], list) else tree
        for i in range(0, len(top) - 1, 2):
            name, v = top[i], top[i+1]
            if isinstance(v, list) and len(v) > 1 and isinstance(v[1], list) and len(v[1]) == 3:
                x, y, z = v[1]
                out.append(f'{a.parent.name} {a.stem} {name} {x} {y} {z}')
open(sys.argv[1], 'w').write('\n'.join(out) + '\n')
print(len(out))
