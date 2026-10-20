"""Resolves the merge conflicts parallel agent branches always make in the
shared lists, by keeping both sides (CLAUDE.md, working with agents):

- CMakeLists.txt: both sides' lines, each once; a one-line mw3probe list is
  turned into one entry per line.
- tests/differential/run_all.sh: the union of both test lists, HEAD's order
  first.
- docs/symbols.csv (and any other file given): both sides' blocks.

    python3 tools/dev/merge_lists.py [files...]   (default: the three above)

Leaves a file alone if it has no conflict markers. Check the result before
committing.
"""
import re
import sys
from pathlib import Path

MARK = re.compile(r"<<<<<<< [^\n]*\n(.*?)=======\n(.*?)>>>>>>> [^\n]*\n", re.S)


def lines_union(a, b):
    out = []
    for line in a.splitlines(True) + b.splitlines(True):
        if line not in out:
            out.append(line)
    return ''.join(out)


def cmake(s):
    s = MARK.sub(lambda m: lines_union(m.group(1), m.group(2)), s)
    # Any one-line mw3probe list becomes entries of the multi-line one.
    one = re.compile(r"add_library\(mw3probe SHARED ([^\n)]*)\)\n")
    extra = []
    for m in one.finditer(s):
        extra += m.group(1).split()
    s = one.sub('', s)
    if extra:
        block = re.search(r"add_library\(mw3probe SHARED\n(.*?)\)\n", s, re.S)
        have = block.group(1).split()
        new = have + [x for x in extra if x not in have]
        s = s[:block.start()] + "add_library(mw3probe SHARED\n" + ''.join(f"  {x}\n" for x in new) + ")\n" + \
            s[block.end():]
    return s


def run_all(s):
    # The whole list, from the first `for t in` to the last `; do`, rebuilt as
    # the union of every name in it (conflict markers and repeated loop heads
    # from earlier merges included), in order of first appearance.
    a = s.index('for t in')
    b = s.rindex('; do\n') + len('; do\n')
    names = []
    for line in s[a:b].splitlines():
        if line.startswith(('<<<<<<<', '=======', '>>>>>>>')):
            continue
        for w in re.findall(r"[a-z0-9_]+", line.replace('for t in', ' ').replace('; do', ' ')):
            if w not in names:
                names.append(w)
    block = "for t in \\\n" + ''.join(f"    {t} \\\n" for t in names) + "    ; do\n"
    return s[:a] + block + s[b:]


def concat(s):
    return MARK.sub(lambda m: m.group(1) + m.group(2), s)


def main():
    files = sys.argv[1:] or ['CMakeLists.txt', 'tests/differential/run_all.sh', 'docs/symbols.csv']
    for f in files:
        p = Path(f)
        s = p.read_text()
        if '<<<<<<<' not in s and not (p.name == 'run_all.sh' and s.count('for t in') > 1):
            continue
        s = cmake(s) if p.name == 'CMakeLists.txt' else run_all(s) if p.name == 'run_all.sh' else concat(s)
        if '<<<<<<<' in s or '>>>>>>>' in s:
            raise SystemExit(f'{f}: conflicts left')
        p.write_text(s)
        print(f'resolved {f}')


if __name__ == '__main__':
    main()
