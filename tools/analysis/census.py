"""Classifies the unported census (tests/test_program.cpp,
census_c1m1_unported_headless; docs/playtest.md, "Unported in c1m1").

    MW3_TEST=census_c1m1 MW3_CENSUS=1 MW3_CENSUS_OUT=census.tsv build/mw3tests
    python3 tools/analysis/census.py census.tsv

Each address the program logged gets its original source file
(analysis-output/func2file.json), its size in pseudocode lines
(analysis-output/decomp/<addr>.c) and an AI mark (the AI's files and ranges,
docs/ai.md). Keys that are not an address (a virtual slot, "new", a peek)
are listed as they are.
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
F2F = json.loads((ROOT / 'analysis-output' / 'func2file.json').read_text())
DECOMP = ROOT / 'analysis-output' / 'decomp'
AI_FILES = re.compile(r'zai|zrdrai|zrdrvalve|waynet|aiv|zainet', re.I)
AI_RANGES = [(0x4b5580, 0x4b7d80), (0x4bcba0, 0x4e44b0), (0x4e7000, 0x4ea400)]


def info(key):
    m = re.fullmatch(r'0x0*([0-9a-f]+)', key.split()[0])
    if not m:
        return None, None, False
    a = int(m.group(1), 16)
    name = f'0x{a:x}'
    f = F2F.get(name, '?')
    d = DECOMP / f'{name}.c'
    lines = sum(1 for _ in d.open()) if d.exists() else None
    # A file only counts when it is the function's own (not "between X and Y").
    ai = (not f.startswith('(') and bool(AI_FILES.search(f))) or any(lo <= a < hi for lo, hi in AI_RANGES)
    return f, lines, ai


HOOKS = {}


def hook_of(key):
    """The src line that logs `key` (a hook's default): `file:line text`."""
    if not HOOKS:
        for f in (ROOT / 'src').rglob('*'):
            if f.suffix not in ('.h', '.cpp'):
                continue
            for i, l in enumerate(f.read_text(errors='replace').splitlines(), 1):
                for m in re.finditer(r'unported\("([^"]+)"', l):
                    HOOKS.setdefault(m.group(1), f'{f.relative_to(ROOT)}:{i} {l.strip()[:90]}')
    return HOOKS.get(key, '')


def main():
    rows = []
    for line in Path(sys.argv[1]).read_text().splitlines():
        if not line or line.startswith('#'):
            continue
        n, key = line.split('\t', 1)
        f, lines, ai = info(key)
        rows.append((int(n), key, f or '-', lines if lines is not None else '-', 'AI' if ai else '', hook_of(key)))
    w = max(len(r[1]) for r in rows)
    for n, key, f, lines, ai, hook in rows:
        print(f'{n:8d}  {key:<{w}}  {str(lines):>5}  {ai:2}  {f}')
        if '--hooks' in sys.argv and hook:
            print(f'{"":10}{hook}')


main()
