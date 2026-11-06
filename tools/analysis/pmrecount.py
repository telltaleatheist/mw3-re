#!/usr/bin/env python3
"""What's left of the AI's attack path: the trace's functions the attack state reaches, less what the port names.

A function counts as done when the port's PM AI sources name its address outside the hook classes, or it's in --inlined.
Cited isn't landed: a comment can name what isn't ported, and only a test shows a function is. This is a size, nothing more.

    tools/analysis/pmrecount.py --trace DIR [--roots 0x4dedf0,0x4debe0] [--inlined 0x4c7e10,...]

DIR holds the PC trace's SUMMARY.tsv (pm_addr, port_state, instructions, one column per run). Needs analysis-output/pm.asm,
pm-decomp/ and game/pm/Mech3.exe. Run from the repo's root.
"""
import argparse
import bisect
import collections
import csv
import glob
import os
import re
import struct

RUNS = ('c1m1-idle', 'c1sa1-idle', 'c2m3-idle', 'c1m1-walk')
SOURCES = 'src/mech3/zai_*_pm.* src/mech3/zainet_inout.*'
LOW, HIGH = 0x4b8000, 0x500000  # the AI's range


def reader(path):
    d = open(path, 'rb').read()
    o = struct.unpack_from('<I', d, 0x3c)[0]
    n = struct.unpack_from('<H', d, o + 6)[0]
    size = struct.unpack_from('<H', d, o + 20)[0]
    base = struct.unpack_from('<I', d, o + 52)[0]
    sections = [(base + va, rs, rp) for vs, va, rs, rp in
                (struct.unpack_from('<IIII', d, o + 24 + size + 40 * i + 8) for i in range(n))]

    def rd(a):
        for va, rs, rp in sections:
            if va <= a < va + rs:
                return struct.unpack_from('<I', d, rp + a - va)[0]
        return None
    return rd


def instructions(path):
    for line in open(path, errors='replace'):
        m = re.match(r'\s*([0-9a-f]+):\s*(?:[0-9a-f]{2} +)*\s*(\S.*)$', line)
        if m:
            t = re.sub(r'<.*', '', m.group(2)).strip()
            t = re.sub(r'^(?:[0-9a-f]{2} )+(?=[a-z])', '', t)
            if t:
                yield int(m.group(1), 16), re.sub(r'\s+', ' ', t)


def cited():
    text = ''
    for pat in SOURCES.split():
        for f in glob.glob(pat):
            text += open(f, errors='replace').read()
    # A hook names what the port doesn't have.
    text = re.sub(r'(?:class|struct)\s+\w*Hooks?\b[^{;]*\{.*?\n\};', '', text, flags=re.S)
    text = '\n'.join(ln for ln in text.split('\n') if 'virtual ' not in ln)
    return {int(x, 16) for x in re.findall(r'0x00([0-9a-f]{6})\b', text)}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--trace', required=True)
    ap.add_argument('--roots', default='0x4dedf0,0x4debe0')
    ap.add_argument('--inlined', default='')
    a = ap.parse_args()
    rows = {r['pm_addr']: r for r in csv.DictReader(open(os.path.join(a.trace, 'SUMMARY.tsv'), encoding='utf-8-sig'), delimiter='\t')}
    ran = {k for k, r in rows.items() if any('load' in r[x] or 'mission' in r[x] for x in RUNS)}
    starts = sorted(int(os.path.basename(f)[:-2], 16) for f in glob.glob('analysis-output/pm-decomp/0x*.c'))
    owner = lambda x: starts[bisect.bisect_right(starts, x) - 1]  # noqa: E731
    rd = reader('game/pm/Mech3.exe')
    calls, vts = collections.defaultdict(set), collections.defaultdict(set)
    for at, t in instructions('analysis-output/pm.asm'):
        if not LOW <= at < HIGH:
            continue
        o = owner(at)
        m = re.match(r'call (0x[0-9a-f]+)$', t)
        if m:
            calls[o].add(int(m.group(1), 16))
        m = re.match(r'mov dword ptr \[e\w\w(?: \+ 0x[0-9a-f]+)?\], (0x5[def][0-9a-f]{4})$', t)
        if m:
            vts[o].add(int(m.group(1), 16))

    def table(v):
        out = []
        for i in range(70):
            x = rd(v + 4 * i)
            if x is None or not 0x401000 <= x < 0x5d0000:
                break
            out.append(x)
        return out
    seen, todo = set(), [int(x, 16) for x in a.roots.split(',')]
    while todo:
        x = todo.pop()
        if x in seen or not LOW <= x < HIGH:
            continue
        seen.add(x)
        todo += list(calls.get(x, ()))
        for v in vts.get(x, ()):
            todo += table(v)
    done = cited() | {int(x, 16) for x in a.inlined.split(',') if x}
    listed = [(x, rows[hex(x)]) for x in seen if hex(x) in rows]
    left = [(x, r) for x, r in listed if r['pm_addr'] in ran and x not in done]
    print(f'reached {len(seen)}, listed {len(listed)}, ran {sum(r["pm_addr"] in ran for _, r in listed)}, not cited {len(left)}')
    for state in sorted({r['port_state'] for _, r in left}):
        part = sorted(((int(r['instructions']), r['pm_addr']) for _, r in left if r['port_state'] == state), reverse=True)
        print(f'{state}: {len(part)} functions, {sum(n for n, _ in part)} instructions: {part}')


if __name__ == '__main__':
    main()
