"""Builds analysis-output/types.full.json: docs/types.json plus placeholder
fields (f_XXX float, u_XXX 4-byte) wherever fields.py sees an unnamed offset
used as a float or a dword. The placeholders only help Ghidra's output; the
names live in docs/types.json.

  typesfull.py
"""
import json
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import fields as F  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
SCANS = {  # object vtable -> the classes it is made of, base first
    0x5bcef8: ['ZEntity', 'ZEntity3d', 'AiTarget', 'Vehicle', 'Mech'],
    0x5c2910: ['MechPart', 'Foot'],
    0x5c2978: ['MechPart', 'Leg'],
}
SIZES = {'byte': 1, 'bool': 1, 'char': 1, 'short': 2}


def width(spec, structs):
    if spec.endswith('*'):
        return 4
    if '[' in spec:
        base, n = spec[:spec.index('[')], int(spec[spec.index('[') + 1:-1], 0)
        return width(base, structs) * n
    if spec in structs:
        return int(structs[spec]['size'], 16)
    return SIZES.get(spec, 4)


def main():
    spec = json.load(open(ROOT / 'docs' / 'types.json'))
    structs = spec['structs']
    asm, addrs, starts = F.load()
    added = 0
    for vt, chain in SCANS.items():
        todo = set(F.vtable(vt)) | {int(a, 16) for c in chain for a in spec['methods'].get(c, [])}
        done = set()
        seen = defaultdict(lambda: {'widths': set(), 'funcs': set(), 'float': 0, 'int': 0, 'writes': 0, 'addr': 0})
        for _ in range(3):
            callees = set()
            for f in sorted(todo - done):
                F.scan(asm, addrs, starts, f, seen, callees)
                done.add(f)
            todo = callees
        lo = 0
        ranges = []
        for c in chain:
            hi = int(structs[c]['size'], 16)
            ranges.append((c, lo, hi))
            lo = hi
        for c, lo, hi in ranges:
            taken = set()
            for off, t, *_ in structs[c]['fields']:
                o = int(off, 16)
                taken.update(range(o, o + width(t, structs)))
            for off, info in sorted(seen.items()):
                if not lo <= off < hi or off in taken or info['widths'] != {4} or off + 4 > hi:
                    continue
                if any(o in taken for o in range(off, off + 4)):
                    continue
                kind = 'float' if info['float'] > info['int'] else 'undefined4'
                name = ('f_' if kind == 'float' else 'u_') + f'{off:x}'
                structs[c]['fields'].append([hex(off), kind, name])
                taken.update(range(off, off + 4))
                added += 1
    out = ROOT / 'analysis-output' / 'types.full.json'
    json.dump(spec, open(out, 'w'), indent=1)
    print(f'{out}: {added} placeholder fields added')


if __name__ == '__main__':
    main()
