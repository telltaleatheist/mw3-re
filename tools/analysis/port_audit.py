"""Every game function of the original against the port (docs/port-audit.md).

    python3 tools/analysis/port_audit.py [--game mw3|pm] [--src DIR] [--out FILE.json] [--module NAME]

For each function of func2file.json (pm-func2file.json for --game pm) below
the C runtime (MW3 0x5aa450, the first "(after ...si_script.cpp)"; PM's
own start of it), it says:
  status    ported (a comment starting "Mech3.exe 1.2 @ 0x..." or, for PM,
            "Pirate's Moon 1.0 @ 0x..." names it: its own port), named (the
            address appears elsewhere in src: file:line kept), or none
  lines     the decomp's line count (analysis-output/decomp, pm-decomp)
  file      the original source file (func2file)
  module    that file's folder (mech3, zmodel, ...)
  callers   functions with a direct call to it (mech3.asm / pm.asm)
  callees   functions it calls directly
  tables    places in the exe's data that hold its address (vtables and
            callback tables: how a function with no callers is reached)
  from_ported  a ported function calls it directly (reachable from our code)
Without --out it prints counts by module and status.
"""
import argparse
import bisect
import json
import re
import struct
import sys
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
A = ROOT / 'analysis-output'

GAMES = {
    'mw3': dict(f2f=A / 'func2file.json', decomp=A / 'decomp', asm=A / 'mech3.asm',
                exe=ROOT / 'game' / 'mw3-1.2' / 'Mech3.exe', tag=r'Mech3\.exe 1\.2', crt='si_script.cpp)'),
    'pm': dict(f2f=A / 'pm-func2file.json', decomp=A / 'pm-decomp', asm=A / 'pm.asm',
               exe=ROOT / 'game' / 'pm' / 'Mech3.exe', tag=r"Pirate's Moon 1\.0", crt='si_script.cpp)'),
}


def module_of(path):
    """mech3\\briefing.cpp -> mech3; '(between a\\x.cpp and b\\y.cpp)' -> a's folder."""
    m = re.search(r'([A-Za-z0-9_]+)\\[A-Za-z0-9_]+\.(?:cpp|c)', path)
    return m.group(1).lower() if m else '?'


def crt_start(f2f, marker):
    """The first function of the C runtime: the first one past the game's last source file."""
    after = sorted(int(a, 16) for a, p in f2f.items() if p.startswith('(after') and marker in p)
    return after[0] if after else 1 << 32


def decomp_lines(folder, addr):
    for name in (f'0x{addr:x}.c', f'0x{addr:08x}.c'):
        p = folder / name
        if p.exists():
            return sum(1 for _ in p.open(errors='replace'))
    return 0


DECL = re.compile(r'^\s*(undefined\d?|int|uint|char|byte|short|ushort|float|double|void|bool|long|code|float10|[A-Z][A-Za-z_]*)\s*\**\s*[a-zA-Z_][\w\[\]]*;\s*$')


def body_of(folder, addr):
    """The decomp's statements: the signature, braces, declarations and comments dropped."""
    for name in (f'0x{addr:x}.c', f'0x{addr:08x}.c'):
        p = folder / name
        if p.exists():
            lines = p.read_text(errors='replace').splitlines()
            out = []
            for l in lines[3:]:
                t = l.strip()
                if not t or t in ('{', '}') or t.startswith(('/*', '//')) or DECL.match(l):
                    continue
                out.append(t)
            return out
    return []


def shape(body):
    """A label for the trivial shapes a port folds into its callers, else None."""
    b = [t for t in body if t not in ('return;',)]
    ret = re.compile(r'^return (\(\w+\d*\))?\*?\(?(\w+ \*?\*?\))?\(?in_ECX( \+ 0x[0-9a-f]+|\[\w+\])?\)?;$')
    if len(b) == 1 and ret.match(b[0]):
        return 'folded', 'a getter of a member'
    if len(b) == 1 and re.match(r'^return (&?DAT_|_DAT_|PTR_|&PTR_|0x|-?\d|\(float10\))', b[0]):
        return 'folded', 'returns a constant or a global'
    if b and all(re.match(r'^\*?\(?(\w+ \*\))?\(?in_ECX( \+ 0x[0-9a-f]+|\[\w+\])?\)? = [^;]+;$', t) for t in b) and len(b) <= 4:
        return 'folded', 'sets members (a setter or a small constructor)'
    if any('operator_delete(in_ECX)' in t for t in b) and len(b) <= 5:
        return 'folded', 'the scalar deleting destructor (the destructor, then delete)'
    if len(b) == 1 and re.match(r'^(free|operator_delete)\(', b[0]):
        return 'folded', 'frees the object (a destructor\'s release)'
    if len(b) <= 2 and any(re.match(r'^\w+ = (malloc|operator_new)\(', t) for t in b):
        return 'folded', 'allocates (operator new)'
    if len(b) == 1 and re.match(r'^(return )?\(?\*?\*?\(?(code \*\*?\)?)?FUN_[0-9a-f]+\([^;]*\);$', b[0]):
        return 'folded', 'a thunk: one call to another function'
    if len(b) == 1 and re.match(r'^(return )?\(\*\*\(code \*\*\)\(\*in_ECX \+ 0x[0-9a-f]+\)\)\([^;]*\);$', b[0]):
        return 'folded', 'a thunk: one virtual call'
    if not b:
        return 'folded', 'empty: does nothing'
    if any('_Lockit' in t for t in b):
        return 'folded', 'C++ library container code (a std::map, set or list instance); the port uses its own containers'
    return None


def scan_src(src, tag):
    """Ported addresses (a comment starting with the tag) and every address named anywhere."""
    head = re.compile(r'//\s*' + tag + r' @ 0x([0-9a-fA-F]{6,8})')
    any_addr = re.compile(r'0x(00)?([45][0-9a-fA-F]{5})\b')
    ported, named = {}, defaultdict(list)
    for p in sorted(src.rglob('*')):
        if p.suffix not in ('.cpp', '.h', '.c', '.hpp', '.inc'):
            continue
        rel = p.relative_to(src.parent).as_posix()
        for n, line in enumerate(p.open(errors='replace'), 1):
            for m in head.finditer(line):
                ported.setdefault(int(m.group(1), 16), rel)
            for m in any_addr.finditer(line):
                named[int(m.group(2), 16)].append(f'{rel}:{n}')
    return ported, named


def scan_asm(asm, starts):
    """Direct calls and immediate references between functions, and each
    function's references into data (for the tables it reaches)."""
    call = re.compile(r"^\s*([0-9a-f]+):\s+(?:call|jmp)\s+0x([0-9a-f]+)")
    imm = re.compile(r'^\s*([0-9a-f]+):\s+(?:push|mov)\s.*\b0x([45][0-9a-f]{5})\b')
    callers, callees, refs = defaultdict(set), defaultdict(set), defaultdict(set)
    data_refs = defaultdict(set)
    dimm = re.compile(r'^\s*([0-9a-f]+):.*?\b0x([5-9a-f][0-9a-f]{5,6})\b')
    sset = set(starts)

    def owner(a):
        i = bisect.bisect_right(starts, a) - 1
        return starts[i] if i >= 0 else None

    for line in asm.open(errors='replace'):
        m = call.match(line)
        if m:
            at, to = int(m.group(1), 16), int(m.group(2), 16)
            if to in sset:
                f = owner(at)
                if f is not None and f != to:
                    callers[to].add(f)
                    callees[f].add(to)
            continue
        m = imm.match(line)
        if m:
            at, to = int(m.group(1), 16), int(m.group(2), 16)
            if to in sset:
                f = owner(at)
                if f is not None and f != to:
                    refs[to].add(f)
        for m in dimm.finditer(line):
            at, to = int(m.group(1), 16), int(m.group(2), 16)
            if to >= 0x5b0000 and to not in sset:
                f = owner(at)
                if f is not None:
                    data_refs[f].add(to)
    return callers, callees, refs, data_refs


def reachable(entry, starts, callees, refs_from, data_refs, tables):
    """Functions reached from the entry point by calls, code references and
    the tables live code points at (a slot belongs to the nearest referenced
    data address at or below it, within 0x800 bytes)."""
    slot_fns = defaultdict(list)
    for f, locs in tables.items():
        for l in locs:
            slot_fns[l].append(f)
    slots = sorted(slot_fns)
    # Every function a table holds is a root: a table's owner is not always
    # found by its code's references (objects in .data, tables filled at start).
    seen, todo, opened = set(), [entry] + list(tables), set()
    while todo:
        f = todo.pop()
        if f in seen:
            continue
        seen.add(f)
        todo.extend(callees.get(f, ()))
        todo.extend(refs_from.get(f, ()))
        for d in data_refs.get(f, ()):
            if d in opened:
                continue
            opened.add(d)
            i = bisect.bisect_left(slots, d)
            while i < len(slots) and slots[i] < d + 0x800:
                todo.extend(slot_fns[slots[i]])
                i += 1
    return seen


def scan_tables(exe, sset):
    """Each function address held as a dword in the exe's non-code sections (vtables, callback tables)."""
    data = exe.read_bytes()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    nsec = struct.unpack_from('<H', data, pe + 6)[0]
    optsz = struct.unpack_from('<H', data, pe + 20)[0]
    base = struct.unpack_from('<I', data, pe + 24 + 28)[0]
    tables = defaultdict(list)
    sec = pe + 24 + optsz
    for i in range(nsec):
        name = data[sec:sec + 8].rstrip(b'\0').decode('latin-1')
        vsize, va, rsize, raw = struct.unpack_from('<IIII', data, sec + 8)
        flags = struct.unpack_from('<I', data, sec + 36)[0]
        sec += 40
        if flags & 0x20:  # code
            continue
        for off in range(0, min(rsize, vsize or rsize) - 3, 4):
            v = struct.unpack_from('<I', data, raw + off)[0]
            if v in sset:
                tables[v].append(base + va + off)
    return tables


def pe_entry(exe):
    data = exe.read_bytes()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    ep = struct.unpack_from('<I', data, pe + 24 + 16)[0]
    base = struct.unpack_from('<I', data, pe + 24 + 28)[0]
    return base + ep


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--game', default='mw3', choices=GAMES)
    ap.add_argument('--src', default=str(ROOT / 'src'))
    ap.add_argument('--out')
    ap.add_argument('--module')
    o = ap.parse_args()
    g = GAMES[o.game]
    f2f = json.load(open(g['f2f']))
    crt = crt_start(f2f, g['crt'])
    every = sorted(int(a, 16) for a in f2f)
    game = [a for a in every if a < crt]
    ported, named = scan_src(Path(o.src), g['tag'])
    callers, callees, refs, data_refs = scan_asm(g['asm'], every)
    tables = scan_tables(g['exe'], set(every))
    refs_from = defaultdict(set)
    for to, fs in refs.items():
        for f in fs:
            refs_from[f].add(to)
    entry = pe_entry(g['exe'])
    reach = reachable(entry, every, callees, refs_from, data_refs, tables)
    sym = {}  # docs/symbols.csv's notes: "ported inline", "not ported on purpose: ..."
    import csv
    for rec in csv.reader(open(ROOT / 'docs' / 'symbols.csv', errors='replace')):
        if rec and rec[0].startswith('0x') and len(rec) > 3:
            try:
                sym[int(rec[0], 16)] = rec[3]
            except ValueError:
                pass
    port_sorted = sorted(a for a in ported if a < crt)

    def near(a):
        i = bisect.bisect_left(port_sorted, a)
        prev = ported[port_sorted[i - 1]] if i > 0 else ''
        nxt = ported[port_sorted[i]] if i < len(port_sorted) else ''
        return prev if prev == nxt or not nxt else nxt if not prev else f'{prev} | {nxt}'

    rows = []
    for a in game:
        path = f2f[f'0x{a:x}']
        status = 'ported' if a in ported else 'named' if a in named else 'none'
        rows.append(dict(
            addr=f'0x{a:08x}', status=status, module=module_of(path), file=path,
            lines=decomp_lines(g['decomp'], a),
            named_in=sorted(set(named.get(a, [])))[:6] if status == 'named' else [],
            callers=[f'0x{c:08x}' for c in sorted(callers[a])],
            callees=[f'0x{c:08x}' for c in sorted(callees[a])],
            refs=[f'0x{c:08x}' for c in sorted(refs[a])],
            tables=[f'0x{t:08x}' for t in tables[a][:8]],
            from_ported=any(c in ported for c in callers[a]),
            reached=a in reach,
            ported_in=ported.get(a, ''),
            shape=None if a in ported else (['folded', 'ported, by docs/symbols.csv: ' + sym[a][:120]]
                                             if sym.get(a, '').lower().startswith('ported') else shape(body_of(g['decomp'], a))),
            symbols=sym.get(a, ''),
            near='' if a in ported else near(a),
        ))
    # A thunk is folded only when its target is ported or folded itself; else it
    # follows its target (so missing logic cannot hide behind a thunk). A setter
    # or small constructor is folded only when ported code calls it.
    byaddr = {r['addr']: r for r in rows}
    # An address not on a 16-byte boundary is a piece Ghidra cut out of the
    # function before it (the compiler aligns function starts): it is that
    # function's, ported or not.
    prev = None
    for r in rows:
        a = int(r['addr'], 16)
        if a % 16 and prev is not None and r['status'] != 'ported':
            r['shape'] = ['folded', 'a piece of the ported function before it (Ghidra split it there)', prev['addr']] if prev['status'] == 'ported' \
                else ['follows', prev['addr']]
        if a % 16 == 0:
            prev = r
    # A function the port names only in ordinary comments of its own code is
    # covered there (the comment says where); one named in an unported() hook,
    # or with "not ported", "deferred", "TODO", "not yet", is read instead.
    flag = re.compile(r'unported|not ported|deferred|todo|not yet|missing|stub', re.I)
    cache = {}
    for r in rows:
        if r['status'] != 'named' or r['shape']:
            continue
        refs_all = sorted(set(named.get(int(r['addr'], 16), [])))
        texts = []
        for ref in refs_all:
            fn, n = ref.rsplit(':', 1)
            if fn not in cache:
                cache[fn] = (ROOT / fn).read_text(errors='replace').splitlines()
            texts.append(cache[fn][int(n) - 1])
        if texts and not any(flag.search(t) for t in texts) and not all(fn.startswith('tests/') for fn in refs_all):
            r['shape'] = ['folded', 'covered by the port where it names it', refs_all[0]]
    # A function the C runtime's code calls (its exception unwinds) and that
    # resets a vtable is a destructor.
    for r in rows:
        if r['status'] != 'ported' and not r['shape'] and any(int(c, 16) >= crt for c in r['callers']):
            b = body_of(g['decomp'], int(r['addr'], 16))
            if any(re.match(r'^\*?\(?(undefined\d? \*\*?\*?\))?\(?in_ECX( \+ 0x[0-9a-f]+)?\)? = &PTR_', t) for t in b[:4]):
                r['shape'] = ['folded', 'a destructor (the exception unwinds call it; it resets the vtable)']
    thunk_unported = 0
    for _ in range(4):
        for r in rows:
            sh = r['shape']
            if not sh or r['status'] == 'ported':
                continue
            if sh[1].startswith('a thunk') and len(r['callees']) == 1:
                t = byaddr.get(r['callees'][0])
                ok = t is None or t['status'] == 'ported' or (t['shape'] and t['shape'][0] == 'folded')
                if not ok:
                    r['shape'] = ['follows', r['callees'][0]]
            elif sh[1].startswith('sets members') and not r['from_ported']:
                r['shape'] = None
    for r in rows:
        if r['shape'] and r['shape'][0] == 'follows':
            thunk_unported += 1
    if o.module:
        rows = [r for r in rows if r['module'] == o.module]
    if o.out:
        json.dump(rows, open(o.out, 'w'), indent=1)
    by = defaultdict(Counter)
    for r in rows:
        by[r['module']][r['status']] += 1
    print(f'{o.game}: {len(every)} functions, {len(game)} below the C runtime (0x{crt:08x})')
    tot = Counter()
    for m in sorted(by, key=lambda m: -sum(by[m].values())):
        c = by[m]
        tot += c
        print(f'  {m:12} {sum(c.values()):5}  ported {c["ported"]:5}  named {c["named"]:5}  none {c["none"]:5}')
    print(f'  not reached from the entry point: {sum(1 for r in rows if not r["reached"])} '
          f'(unported {sum(1 for r in rows if not r["reached"] and r["status"] != "ported")})')
    print(f'  {"all":12} {sum(tot.values()):5}  ported {tot["ported"]:5}  named {tot["named"]:5}  none {tot["none"]:5}')
    print(f'  thunks whose target is unported (they follow the target): {thunk_unported}')
    sh = Counter(r['shape'][1] for r in rows if r['status'] != 'ported' and r['shape'] and r['shape'][0] == 'folded')
    print(f'  trivial shapes among the unported: {sum(sh.values())}')
    for w, n in sh.most_common():
        print(f'    {n:5}  {w}')
    small = sum(1 for r in rows if r['status'] == 'none' and r['lines'] <= 15)
    print(f'  none with 15 decomp lines or fewer: {small}')


if __name__ == '__main__':
    sys.exit(main())
