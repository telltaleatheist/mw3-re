"""Lists the float and double constants the code loads from .rdata, for
ApplyTypes.java to define, so Ghidra prints their values instead of DAT_ names.

  fpconsts.py      writes analysis-output/fpconsts.txt
"""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RDATA = (0x5bb000, 0x5df9f0)
PAT = re.compile(r'\b(f\w+)\s+(dword|qword) ptr \[0x([0-9a-f]+)\]')


def main():
    kinds = {}
    for line in open(ROOT / 'analysis-output' / 'mech3.asm', encoding='latin-1'):
        for m in PAT.finditer(line):
            a = int(m.group(3), 16)
            if RDATA[0] <= a < RDATA[1]:
                kinds.setdefault(a, set()).add('float' if m.group(2) == 'dword' else 'double')
    rows = [f'{a:#x} {k.pop()}\n' for a, k in sorted(kinds.items()) if len(k) == 1]
    (ROOT / 'analysis-output' / 'fpconsts.txt').write_text(''.join(rows))
    print(f'{len(rows)} constants')


if __name__ == '__main__':
    main()
