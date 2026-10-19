"""Recovers an object's field layout from how its methods use `this`.

  fields.py <vtable> [--extra 0xADDR,...] [--max 0x6d8] [--json out.json]

Starts from the methods in a vtable (plus any extra functions), follows ECX
(`this`) through register copies, and collects every [this + offset] access:
its width, whether the FPU reads or writes it as a float, and which functions
touch it. Functions called with ECX still equal to `this` join the set (they
are methods of the class or of a base), so the walk covers the hierarchy.

It is a heuristic: a register is `this` from a copy until it is written again,
and branches are ignored. Good for a first layout, which the decompiled code
then confirms.
"""
import argparse
import bisect
import json
import re
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import pe  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
LINE = re.compile(r'^\s*([0-9a-f]+):\s*\t(\S+)\s*(.*)$')
REGS = ('eax', 'ebx', 'ecx', 'edx', 'esi', 'edi', 'ebp')
SUB = {'al': 'eax', 'ah': 'eax', 'ax': 'eax', 'bl': 'ebx', 'bh': 'ebx', 'bx': 'ebx',
       'cl': 'ecx', 'ch': 'ecx', 'cx': 'ecx', 'dl': 'edx', 'dh': 'edx', 'dx': 'edx',
       'si': 'esi', 'di': 'edi', 'bp': 'ebp'}
WIDTH = {'byte': 1, 'word': 2, 'dword': 4, 'qword': 8, 'tbyte': 10}
TEXT_END = 0x5b0000


def load():
    asm = []
    for line in open(ROOT / 'analysis-output' / 'mech3.asm', encoding='latin-1'):
        m = LINE.match(line)
        if m:
            asm.append((int(m.group(1), 16), m.group(2), m.group(3)))
    addrs = [a for a, _, _ in asm]
    starts = sorted(int(k, 16) for k in json.load(open(ROOT / 'analysis-output' / 'func2file.json')))
    return asm, addrs, starts


def body(asm, addrs, starts, func):
    k = bisect.bisect_right(starts, func)
    end = starts[k] if k < len(starts) else func + 0x10000
    i = bisect.bisect_left(addrs, func)
    while i < len(asm) and asm[i][0] < end:
        yield asm[i]
        i += 1


def written(mnem, ops):
    """The full register a 2-operand instruction writes, if any."""
    if mnem in ('cmp', 'test', 'push', 'call') or mnem.startswith('j') or mnem.startswith('f'):
        return None
    first = ops.split(',')[0].strip()
    first = SUB.get(first, first)
    return first if first in REGS else None


def pinned(code):
    """A callee-saved register that gets ECX in the prologue and is never
    written again holds `this` for the whole function, whatever the branches."""
    for addr, mnem, ops in code[:16]:
        m = re.match(r'(esi|edi|ebx|ebp), ecx$', ops)
        if mnem == 'mov' and m:
            reg = m.group(1)
            writes = sum(1 for _, mn, op in code if written(mn, op) == reg or (mn == 'pop' and op == reg))
            pops = sum(1 for _, mn, op in code if mn == 'pop' and op == reg)
            if writes - pops == 1:
                return reg
            return None
        if mnem == 'call' or written(mnem, ops) == 'ecx':
            return None
    return None


def scan(asm, addrs, starts, func, fields, callees):
    code = list(body(asm, addrs, starts, func))
    keep = pinned(code)
    this = {'ecx'}
    for addr, mnem, ops in code:
        if keep:
            this.add(keep)
        for m in re.finditer(r'(?:(byte|word|dword|qword|tbyte) ptr )?\[(\w+) \+ 0x([0-9a-f]+)\]', ops):
            if m.group(2) in this:
                off = int(m.group(3), 16)
                width = WIDTH.get(m.group(1), 0)
                f = fields[off]
                f['widths'].add(width)
                f['funcs'].add(func)
                if mnem.startswith('f') and mnem not in ('fnstsw',) and not mnem.startswith('fi'):
                    f['float'] += 1
                elif mnem.startswith('fi'):
                    f['int'] += 1
                if ops.startswith(m.group(0)) and mnem.startswith(('mov', 'fst', 'and', 'or', 'add', 'sub', 'inc', 'dec', 'xor')):
                    f['writes'] += 1
                if mnem == 'lea':
                    f['addr'] += 1
        for m in re.finditer(r'(?:byte|word|dword|qword) ptr \[(\w+)\]', ops):
            if m.group(1) in this and mnem != 'lea':
                fields[0]['funcs'].add(func)
        if mnem == 'call':
            m = re.match(r'0x([0-9a-f]+)', ops)
            if m and 'ecx' in this:
                target = int(m.group(1), 16)
                if 0x401000 <= target < TEXT_END:
                    callees.add(target)
            this.discard('eax'); this.discard('ecx'); this.discard('edx')
            continue
        if mnem.startswith('ret') or mnem == 'jmp' and 'ptr' in ops:
            continue
        m = re.match(r'(\w+), (\w+)$', ops)
        if mnem == 'mov' and m and m.group(2) in this and m.group(1) in REGS:
            this.add(m.group(1))
            continue
        w = written(mnem, ops)
        if w:
            this.discard(w)


def vtable(addr, limit=200):
    out = []
    for i in range(limit):
        v = pe.u32(addr + 4 * i)
        if v is None or not (0x401000 <= v < TEXT_END):
            break
        out.append(v)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('vtable', type=lambda s: int(s, 16))
    ap.add_argument('--extra', default='')
    ap.add_argument('--max', type=lambda s: int(s, 16), default=0x10000)
    ap.add_argument('--json')
    ap.add_argument('--depth', type=int, default=3)
    a = ap.parse_args()
    asm, addrs, starts = load()
    todo = set(vtable(a.vtable)) | {int(x, 16) for x in a.extra.split(',') if x}
    done = set()
    fields = defaultdict(lambda: {'widths': set(), 'funcs': set(), 'float': 0, 'int': 0, 'writes': 0, 'addr': 0})
    for _ in range(a.depth):
        callees = set()
        for f in sorted(todo - done):
            scan(asm, addrs, starts, f, fields, callees)
            done.add(f)
        todo = callees
    rows = []
    for off in sorted(fields):
        if off >= a.max:
            continue
        f = fields[off]
        kind = 'float' if f['float'] > 0 and f['float'] >= f['int'] else ''
        rows.append({'off': off, 'widths': sorted(f['widths']), 'kind': kind, 'writes': f['writes'],
                     'addr_taken': f['addr'], 'funcs': sorted(f['funcs'])})
    if a.json:
        json.dump(rows, open(a.json, 'w'), indent=1, default=lambda x: hex(x))
    print(f'{len(done)} functions scanned')
    for r in rows:
        fs = ' '.join(f'{x:#x}' for x in r['funcs'][:6]) + (' …' if len(r['funcs']) > 6 else '')
        print(f"{r['off']:#06x}  w={','.join(map(str, r['widths'])):6} {r['kind']:5} n={len(r['funcs']):3}  {fs}")


if __name__ == '__main__':
    main()
