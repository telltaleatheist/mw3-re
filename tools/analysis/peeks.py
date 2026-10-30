"""Lists the port's reads of exe globals by address: peek8/16/32/f(addr),
field8/16/32/f(addr, off) and the hooks' unported("peek8", {addr}) stand-ins,
each with its writes (poke*, set_field*) and the other mentions of the
address in the source. A read with no write in the port reads the engine's
memory map, which holds 0 for anything never poked: when a ported struct
owns that global now (its field comment names the address), the read is
wrong (the Win State's [0x00609926] read 0 forever, campaign.md section 14).

    python3 tools/analysis/peeks.py [--all]

Without --all only the reads with no write are listed. Addresses are taken
as written, base + offset folded when both are constants; reads through a
computed address (an object handle) are not globals and are left out.
"""
import re
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DIRS = ['src/mech3', 'src/gamez', 'src/frontend', 'src/platform']
NUM = r'0x[0-9a-fA-F]+'
# peek8(0x609926), peek32(0x6098d0 + 0x78), field32(0x6098d0, 0x78)
READ = re.compile(rf'\b(peek(?:8|16|32|f)?|field(?:8|16|32|f))\(\s*({NUM})\s*(?:([+,])\s*({NUM}))?\s*\)')
STAND_IN = re.compile(rf'unported\(\s*"peek(?:8|16|32|f)?"\s*,\s*\{{\s*({NUM})')
# a stand-in keyed by the global itself: unported("[0x006098d4]+4+0x38", {})
KEYED = re.compile(rf'unported\(\s*"\[({NUM})\]')
WRITE = re.compile(rf'\b(poke(?:8|16|32|f)?|set_field(?:8|16|32|f))\(\s*({NUM})\s*(?:([+,])\s*({NUM}))?')


def sources():
    for d in DIRS:
        for p in sorted((ROOT / d).rglob('*')):
            if p.suffix in ('.cpp', '.h', '.inc'):
                yield p


def address(base, op, off):
    a = int(base, 16)
    if op and off:
        a += int(off, 16)
    return a


def main():
    show_all = '--all' in sys.argv
    reads, writes, texts = defaultdict(list), defaultdict(list), {}
    for p in sources():
        rel = p.relative_to(ROOT).as_posix()
        lines = p.read_text(encoding='utf-8', errors='replace').splitlines()
        texts[rel] = lines
        for n, line in enumerate(lines, 1):
            code = line.split('//', 1)[0]
            for m in READ.finditer(code):
                a = address(m.group(2), m.group(3), m.group(4))
                if a >= 0x400000:  # an exe global, not a small handle
                    reads[a].append((rel, n, m.group(1)))
            for m in STAND_IN.finditer(code):
                reads[int(m.group(1), 16)].append((rel, n, 'stand-in'))
            for m in KEYED.finditer(code):
                reads[int(m.group(1), 16)].append((rel, n, 'keyed'))
            for m in WRITE.finditer(code):
                a = address(m.group(2), m.group(3), m.group(4))
                if a >= 0x400000:
                    writes[a].append((rel, n))
    # A stand-in is a hook's default: what matters is whether the program
    # overrides the method it sits in. The method is the one the line (or the
    # nearest line above it) declares.
    method = re.compile(r'^\s*(?:virtual\s+[\w:<>\*& ]+?\s(\w+)\(|[\w:<>\*& ]+?\s\w+::(\w+)\()')
    alltext = '\n'.join('\n'.join(t) for t in texts.values())

    def overrides(rel, n):
        lines = texts[rel]
        for k in range(n - 1, max(-1, n - 25), -1):
            m = method.search(lines[k].split('//', 1)[0])
            if m:
                name = m.group(1) or m.group(2)
                found = re.findall(rf'\b{name}\([^;{{]*\)\s*(?:const\s*)?override', alltext)
                return name, len(found)
        return '?', 0

    shown = 0
    for a in sorted(reads):
        if writes[a] and not show_all:
            continue
        shown += 1
        forms = {f'0x{a:08x}', f'0x{a:x}'}
        mentions = []
        for rel, lines in texts.items():
            for n, line in enumerate(lines, 1):
                if any(re.search(rf'{f}(?![0-9a-fA-F])', line) for f in forms) and (rel, n) not in {(r, k) for r, k, _ in reads[a]}:
                    mentions.append(f'{rel}:{n}')
        print(f'0x{a:08x}: read {len(reads[a])}, written {len(writes[a])}')
        for rel, n, how in reads[a]:
            note = ''
            if how in ('stand-in', 'keyed'):
                name, k = overrides(rel, n)
                note = f' [{name}: {"overridden" if k else "NO OVERRIDE"}]'
            print(f'  read  {how:9} {rel}:{n}:{note} {texts[rel][n - 1].strip()[:100]}')
        for rel, n in writes[a]:
            print(f'  write           {rel}:{n}')
        for m in mentions[:6]:
            rel, n = m.rsplit(':', 1)
            print(f'  named           {m}: {texts[rel][int(n) - 1].strip()[:100]}')
    print(f'{shown} addresses listed ({len(reads)} read, {sum(1 for a in reads if not writes[a])} never written)')


if __name__ == '__main__':
    main()
