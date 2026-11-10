"""Symbolic x87 trace of an address range of analysis-output/mech3.asm:
prints each float store (and compare) as a C expression in the exe's own
evaluation order, which Ghidra's printed sums do not keep, with its rounding
points: a store to a float shows as f(...), and a slot stored earlier in the
trace reads back as that f(...). What is not inside an f() stays on the x87,
so at the reference runtime's 53 bits it is a double (docs/ai.md 17.4b).

usage: x87.py <start> <end> [<start> <end> ...] [reg=name ...] [--asm=FILE]
  e.g. x87.py 4ec7a0 4ec9a0 eax=l esi=t
       x87.py 54d760 54d83b 54d85c 54d886   (a path through a branch)

[reg+off] operands of the named registers print as name[off / 4]; others
stay as written. A stack slot read before it was pushed (a range that starts
mid-function) shows as STn?. Branches are not followed: give the path taken
as ranges one after another (the stack carries over), each without a taken
jump inside, and pick register names per range (registers are reused).
--asm reads another listing in the same form, e.g. objdump -d
--x86-asm-syntax=intel --start-address=... where mech3.asm lost its
alignment after a jump table."""
import re
import sys

asm = next((a[6:] for a in sys.argv[1:] if a.startswith('--asm=')), 'analysis-output/mech3.asm')
args = [a for a in sys.argv[1:] if not a.startswith('--asm=')]
ranges = [int(a, 16) for a in args if '=' not in a]
names = dict(a.split('=') for a in args if '=' in a)
if not ranges or len(ranges) % 2:
    sys.exit(__doc__)


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
slots = {}  # memory stored in the trace, read back as stored


def mem(op):
    m = re.search(r'\[(.*)\]', op)
    if not m: return op
    e = m.group(1).replace(' ', '')
    m2 = re.fullmatch(r'([a-z]+)(?:\+(0x[0-9a-f]+|\d+))?', e)
    if m2 and m2.group(1) in names:
        off = int(m2.group(2) or '0', 0)
        return f'{names[m2.group(1)]}[{off // 4}]' if off % 4 == 0 else f'{names[m2.group(1)]}+{off}'
    return '[' + e + ']'


def load(op):
    where = mem(op)
    return slots.get(where, where)


def sti(op):
    m = re.fullmatch(r'st\((\d)\)', op.strip())
    return int(m.group(1)) if m else (0 if op.strip() == 'st' else None)


def lines():
    pairs = list(zip(ranges[::2], ranges[1::2]))
    text = open(asm).readlines()
    for start, end in pairs:
        for line in text:
            m = re.match(r'\s+([0-9a-f]+):\s+(\S+)\s*(.*)', line)
            if not m: continue
            a = int(m.group(1), 16)
            if a < start: continue
            if a >= end: break
            yield a, m.group(2), re.sub(r'<.*?>', '', m.group(3)).strip()


for a, ins, ops in lines():
    args = [x.strip() for x in ops.split(',')] if ops else []
    def pop(): return st.pop(0)
    if ins == 'fld':
        i = sti(args[0]); st.insert(0, st[i] if i is not None else load(args[0]))
    elif ins in ('fild',): st.insert(0, '(float)' + load(args[0]))
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
            size = re.match(r'(\w+) ptr', args[0])
            v = st[0] if ins == 'fistp' or (size and size.group(1) == 'tbyte') else (
                f'f({st[0]})' if not size or size.group(1) == 'dword' else f'd({st[0]})')
            slots[mem(args[0])] = v
            print(f'{a:x}: {mem(args[0])} = {v}')
        if ins != 'fst': pop()
    elif ins[:4] in ('fadd', 'fmul', 'fsub', 'fdiv'):
        base = ins.rstrip('p')
        opc = {'fadd': '+', 'fmul': '*', 'fsub': '-', 'fdiv': '/', 'fsubr': 'r-', 'fdivr': 'r/'}[base]
        def comb(x, y):  # dest = dest op src
            if opc == 'r-': return f'({y} - {x})'
            if opc == 'r/': return f'({y} / {x})'
            return f'({x} {opc} {y})'
        if len(args) == 1 and sti(args[0]) is None:
            st[0] = comb(st[0], load(args[0]))
        elif len(args) == 2:
            d, s = sti(args[0]), sti(args[1])
            st[d] = comb(st[d], st[s])
            if ins.endswith('p'): pop()
        elif not args:  # faddp etc with no operands: st1 = st1 op st0, pop
            st[1] = comb(st[1], st[0]); pop()
    elif ins in ('fcomp', 'fcompp', 'fcom', 'fucomp', 'fucom', 'fucompp'):
        other = st[sti(args[0])] if args and sti(args[0]) is not None else (load(args[0]) if args else st[1])
        print(f'{a:x}: compare {st[0]} with {other}')
        if ins in ('fcomp', 'fucomp'): pop()
        if ins in ('fcompp', 'fucompp'): pop(); pop()
    elif ins.startswith('f') and ins not in ('fnstsw', 'fwait'):
        print(f'{a:x}: ?? {ins} {ops}  stack={st}')
    elif ins == 'call':
        print(f'{a:x}: call {ops}  stack depth {len(st)}')
print('left on stack:', st)
