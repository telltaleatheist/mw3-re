# twod_map.py <lo> <hi>: list a range's functions with size, callers and vtable stores (docs/twod.md).
# Run from the repo root.
import bisect, collections, json, re, sys

A = 'analysis-output/'
fm = json.load(open(A + 'func2file.json'))
starts = sorted(int(k, 16) for k in fm)
lo, hi = int(sys.argv[1], 16), int(sys.argv[2], 16)
callers = collections.defaultdict(set)
vtstores = collections.defaultdict(set)


def owner(a):
    i = bisect.bisect_right(starts, a) - 1
    return starts[i] if i >= 0 else None


for line in open(A + 'mech3.asm'):
    m = re.match(r'\s*([0-9a-f]+):\s+(\w+)\s+(.*)', line)
    if not m:
        continue
    a = int(m.group(1), 16)
    op, rest = m.group(2), m.group(3)
    if op in ('call', 'jmp'):
        t = re.match(r'0x([0-9a-f]+)', rest)
        if t:
            callers[int(t.group(1), 16)].add(owner(a))
    if lo <= a < hi and op == 'mov':
        t = re.search(r'dword ptr \[(\w+)( \+ 0x[0-9a-f]+)?\], 0x(5[c-f][0-9a-f]{4})$', rest)
        if t:
            vtstores[owner(a)].add(t.group(3))

for i, s in enumerate(starts):
    if not (lo <= s < hi):
        continue
    size = starts[i + 1] - s
    cs = callers.get(s, set())
    outside = sorted(c for c in cs if c is not None and not (lo <= c < hi))
    inside = len([c for c in cs if c is not None and lo <= c < hi])
    try:
        lines = open(A + f'decomp/0x{s:x}.c').read().count('\n')
    except OSError:
        lines = 0
    print(f'0x{s:x} {size:6d} {lines:4d}L in={inside:2d} out={len(outside):2d} '
          f'{" ".join(hex(c) for c in outside[:6])} vt={",".join(sorted(vtstores.get(s, [])))}')
