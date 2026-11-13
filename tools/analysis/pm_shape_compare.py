"""An order-aware compare of a PM function with its MW3 twin (docs/port-audit-pm.md, method): each instruction is
reduced to what carries logic: calls by their mapped target, conditional jumps, float constants by value, the
float compares and divides, compares, tests, logic and shifts with their small immediates, immediates stored (moves,
pushes, stack adjustments and register juggling dropped as the two compilers differ there), the two sequences aligned with difflib, and what does not
align printed. The feature compare counts; this keeps the order, so a test moved or a branch changed shows here.

    pm_shape_compare.py PMADDR [MW3ADDR]   the ratio and the unaligned stretches
"""
import re
import sys
import bisect
import struct
import difflib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import pm_feature_compare as F  # noqa: E402

REG = re.compile(r'\b(e?[abcd]x|e?[sd]i|e?[sb]p|[abcd][lh]|st\(\d\)|st)\b')


def shape(game, addr):
    ins, idx = F.asm(game)
    out = []
    for s, e in F.RANGES[game][addr]:
        for k in range(bisect.bisect_left(idx, s), bisect.bisect_right(idx, e)):
            ad, mn, ops = ins[k]
            if mn in ('nop', 'int3'):
                continue
            if mn == 'call' and ops.startswith('0x'):
                v = int(ops.split()[0], 16)
                t = (F.FM.get(hex(v)) if game == 'mw3' else (hex(v) if hex(v) in F.PMF else None)) or 'unmapped'
                out.append((ad, 'call ' + t))
                continue
            if mn.startswith('j'):
                out.append((ad, mn))
                continue
            o = ops.split('<')[0]
            m = F.FLD.search(o)
            if mn.startswith('f') and m:
                b = F.EXE[game](int(m.group(2), 16), 8 if m.group(1) == 'qword' else 4)
                if b:
                    v = struct.unpack('<d' if m.group(1) == 'qword' else '<f', b)[0]
                    out.append((ad, '%s %.6g' % (mn, v)))
                    continue
            if mn.startswith('f'):
                if mn in ('fcomp', 'fcom', 'fcompp', 'fucompp', 'fnstsw', 'fchs', 'fabs', 'fsqrt', 'fdivr', 'fdiv', 'fsubr'):
                    out.append((ad, mn))  # the float operations that carry logic, operands aside
                continue
            if mn not in ('cmp', 'test', 'and', 'or', 'xor', 'shl', 'shr', 'sar', 'imul', 'idiv', 'div', 'neg', 'not', 'setne', 'sete', 'sbb', 'adc') and not (mn == 'mov' and re.search(r'\], 0x', o)):
                continue  # moves, pushes, stack adjustments, lea: register and convention noise
            imm = re.findall(r'(?<![\[+-] )(?<!\[)\b(0x[0-9a-f]+)\b(?!\])', re.sub(r'[+-] 0x[0-9a-f]+\]', ']', o))
            if mn in ('xor',) and not imm:
                continue  # a register cleared
            out.append((ad, mn + (' ' + ','.join(x if int(x, 16) < 0x40 else 'I' for x in imm) if imm else '')))
    return out


def main(argv):
    pm = int(argv[0], 16)
    mw = int(argv[1], 16) if len(argv) > 1 else F.TWIN[pm]
    a, b = shape('mw3', mw), shape('pm', pm)
    sm = difflib.SequenceMatcher(None, [x for _, x in a], [x for _, x in b], autojunk=False)
    print('PM 0x%08x MW3 0x%08x: %d / %d instructions, ratio %.3f' % (pm, mw, len(b), len(a), sm.ratio()))
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == 'equal':
            continue
        w = ' ; '.join(x for _, x in a[i1:i2])[:160]
        p = ' ; '.join(x for _, x in b[j1:j2])[:160]
        print('  %s mw3 %s [%s]  pm %s [%s]' % (tag, hex(a[i1][0]) if i1 < len(a) else '-', w, hex(b[j1][0]) if j1 < len(b) else '-', p))


if __name__ == '__main__':
    main(sys.argv[1:])
