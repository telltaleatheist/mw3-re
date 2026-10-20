"""Symbolic x87 trace of an address range of analysis-output/mech3.asm:
prints each float store (and compare) as a C expression in the exe's own
evaluation order, which Ghidra's printed sums do not keep.

usage: x87.py <start> <end> [reg=name ...]   e.g. x87.py 4ec7a0 4ec9a0 eax=l esi=t

[reg+off] operands of the named registers print as name[off / 4]; others
stay as written. A stack slot read before it was pushed (a range that starts
mid-function) shows as STn?. Branches are not followed: run it on a
straight-line range, and pick register names per range (registers are
reused)."""
import re, sys
start, end = int(sys.argv[1], 16), int(sys.argv[2], 16)
names = dict(a.split('=') for a in sys.argv[3:])
class St(list):
    def __getitem__(self, i):
        while i >= len(self): self.append(f'ST{len(self)}?')
        return list.__getitem__(self, i)
    def __setitem__(self, i, v):
        while i >= len(self): self.append(f'ST{len(self)}?')
        list.__setitem__(self, i, v)
    def pop(self, i=-1):
        if not self: return 'EMPTY?'
        return list.pop(self, i)
st = St()
def mem(op):
    m = re.search(r'\[(.*)\]', op)
    if not m: return op
    e = m.group(1).replace(' ', '')
    m2 = re.fullmatch(r'([a-z]+)(?:\+(0x[0-9a-f]+|\d+))?', e)
    if m2 and m2.group(1) in names:
        off = int(m2.group(2) or '0', 0)
        return f'{names[m2.group(1)]}[{off // 4}]' if off % 4 == 0 else f'{names[m2.group(1)]}+{off}'
    return '[' + e + ']'
def sti(op):
    m = re.fullmatch(r'st\((\d)\)', op.strip())
    return int(m.group(1)) if m else (0 if op.strip() == 'st' else None)
for line in open('analysis-output/mech3.asm'):
    m = re.match(r'\s+([0-9a-f]+):\s+(\S+)\s*(.*)', line)
    if not m: continue
    a = int(m.group(1), 16)
    if a < start: continue
    if a >= end: break
    ins, ops = m.group(2), re.sub(r'<.*?>', '', m.group(3)).strip()
    args = [x.strip() for x in ops.split(',')] if ops else []
    def pop(): return st.pop(0)
    if ins == 'fld':
        i = sti(args[0]); st.insert(0, st[i] if i is not None else mem(args[0]))
    elif ins in ('fild',): st.insert(0, '(float)' + mem(args[0]))
    elif ins == 'fldz': st.insert(0, '0')
    elif ins == 'fld1': st.insert(0, '1')
    elif ins == 'fchs': st[0] = f'-({st[0]})'
    elif ins == 'fabs': st[0] = f'fabs({st[0]})'
    elif ins == 'fsqrt': st[0] = f'sqrt({st[0]})'
    elif ins == 'fxch':
        i = sti(args[0]) if args else 1; st[0], st[i] = st[i], st[0]
    elif ins in ('fstp', 'fst', 'fistp'):
        i = sti(args[0])
        if i is not None:
            st[i] = st[0]
        else:
            print(f'{a:x}: {mem(args[0])} = {st[0]}')
        if ins != 'fst': pop()
    elif ins[:4] in ('fadd', 'fmul', 'fsub', 'fdiv') or ins in ('fsubr', 'fsubrp', 'fdivr', 'fdivrp'):
        base = ins.rstrip('p'); popit = ins.endswith('p') and ins not in ('fsubrp',) or ins == 'fsubrp' or ins == 'fdivrp'
        opc = {'fadd': '+', 'fmul': '*', 'fsub': '-', 'fdiv': '/', 'fsubr': 'r-', 'fdivr': 'r/'}[base]
        def comb(x, y):  # dest = dest op src
            if opc == 'r-': return f'({y} - {x})'
            if opc == 'r/': return f'({y} / {x})'
            return f'({x} {opc} {y})'
        if len(args) == 1 and sti(args[0]) is None:
            st[0] = comb(st[0], mem(args[0]))
        elif len(args) == 2:
            d, s = sti(args[0]), sti(args[1])
            st[d] = comb(st[d], st[s])
            if ins.endswith('p'): pop()
        elif not args:  # faddp etc with no operands: st1 = st1 op st0, pop
            st[1] = comb(st[1], st[0]); pop()
    elif ins in ('fcomp', 'fcompp', 'fcom', 'fucomp'):
        print(f'{a:x}: compare {st[0]} with {mem(args[0]) if args else st[1]}')
        if ins == 'fcomp' or ins == 'fucomp': pop()
        if ins == 'fcompp': pop(); pop()
    elif ins.startswith('f') and ins not in ('fnstsw', 'fwait'):
        print(f'{a:x}: ?? {ins} {ops}  stack={st}')
    elif ins == 'call':
        print(f'{a:x}: call {ops}  stack depth {len(st)}')
print('left on stack:', st)
