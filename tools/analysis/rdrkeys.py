"""Finds every reader lookup in Mech3.exe: which key it asks for, which getter,
where the value goes, and which function and source file it sits in.

  rdrkeys.py [filter]     filter: a substring of the function, file or key

Reads analysis-output/mech3.asm and func2file.json. The getters take ECX = node
and EDX = key (docs/zreader.md); the out pointer, when there is one, is pushed.
"""
import bisect
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import pe  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
GETTERS = {
    0x5569e0: 'find', 0x556930: 'find@', 0x5568a0: 'find_flat',
    0x5569f0: 'get_string', 0x556a20: 'get_string_flat', 0x556a60: 'get_float',
    0x556ae0: 'get_int', 0x556b30: 'get_int_if', 0x556b50: 'get_byte_if',
    0x5570b0: 'get_vec3', 0x5570d0: 'get_vec2', 0x5570f0: 'get_ivec3',
    0x557110: 'get_ivec2', 0x557130: 'get_bool_int', 0x557150: 'get_bool',
}
LINE = re.compile(r'^\s*([0-9a-f]+):\s*\t(.*)$')


def load_asm():
    out = []
    for line in open(ROOT / 'analysis-output' / 'mech3.asm', encoding='latin-1'):
        m = LINE.match(line)
        if m:
            out.append((int(m.group(1), 16), m.group(2).strip()))
    return out


def lookups():
    asm = load_asm()
    f2f = json.load(open(ROOT / 'analysis-output' / 'func2file.json'))
    starts = sorted(int(k, 16) for k in f2f)
    for i, (addr, ins) in enumerate(asm):
        m = re.match(r'call\s+0x([0-9a-f]+)', ins)
        if not m or int(m.group(1), 16) not in GETTERS:
            continue
        key = out = node = None
        regs = {}
        for j in range(i - 1, max(i - 14, 0), -1):
            a, prev = asm[j]
            if prev.startswith('call') or prev.startswith('ret') or prev.startswith('jmp'):
                break
            mm = re.match(r'mov\s+edx, 0x([0-9a-f]+)$', prev)
            if mm and key is None:
                key = pe.cstr(int(mm.group(1), 16))
            mm = re.match(r'mov\s+ecx, (.*)$', prev)
            if mm and node is None:
                node = mm.group(1)
            mm = re.match(r'push\s+(\w+)$', prev)
            if mm and out is None:
                out = mm.group(1)
            mm = re.match(r'lea\s+(\w+), \[(.*)\]$', prev)
            if mm:
                regs.setdefault(mm.group(1), mm.group(2))
        if out in regs:
            out = '&[' + regs[out] + ']'
        k = bisect.bisect_right(starts, addr) - 1
        func = starts[k]
        yield addr, func, f2f[f'{func:#x}'], GETTERS[int(m.group(1), 16)], key, node, out


def main():
    flt = sys.argv[1].lower() if len(sys.argv) > 1 else ''
    for addr, func, src, getter, key, node, out in lookups():
        row = f'{addr:#x}\t{func:#x}\t{src}\t{getter}\t{key!r}\tecx={node}\tout={out}'
        if flt in row.lower():
            print(row)


if __name__ == '__main__':
    main()
