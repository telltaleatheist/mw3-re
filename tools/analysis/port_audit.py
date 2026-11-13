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
    # Pirate's Moon's header also comes after MW3's on a shared port
    # ("Mech3.exe 1.2 @ ..., Pirate's Moon 1.0 @ ...") or inside a comment,
    # with a list of addresses: each one listed is ported there.
    pm_head = re.compile(r"Pirate's Moon(?:'s)? 1\.0 @ ((?:0x[0-9a-fA-F]{6,8}(?:, | and | / |/)?)+)") if 'Pirate' in tag else None
    any_addr = re.compile(r'0x(00)?([45][0-9a-fA-F]{5})\b')
    ported, named = {}, defaultdict(list)
    for p in sorted(src.rglob('*')):
        if p.suffix not in ('.cpp', '.h', '.c', '.hpp', '.inc'):
            continue
        rel = p.relative_to(src.parent).as_posix()
        for n, line in enumerate(p.open(errors='replace'), 1):
            for m in head.finditer(line):
                ported.setdefault(int(m.group(1), 16), rel)
            if pm_head and '//' in line:
                for m in pm_head.finditer(line[line.index('//'):]):
                    for a in re.findall(r'0x([0-9a-fA-F]{6,8})', m.group(1)):
                        ported.setdefault(int(a, 16), rel)
            for m in any_addr.finditer(line):
                named[int(m.group(2), 16)].append(f'{rel}:{n}')
    return ported, named


def scan_asm(asm, starts):
    """Direct calls and immediate references between functions, and each
    function's references into data (for the tables it reaches)."""
    call = re.compile(r"^\s*([0-9a-f]+):\s+(?:[0-9a-f]{2} )*\s*(?:call|jmp)l?\s+0x([0-9a-f]+)")  # pm.asm keeps the bytes
    imm = re.compile(r'^\s*([0-9a-f]+):\s+(?:[0-9a-f]{2} )*\s*(?:push|mov)l?\s.*\b0x([45][0-9a-f]{5})\b')
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


LABEL_WORDS = {
    'folded': 'folded', 'sdl': 'replaced (SDL or the platform layer)', 'software': 'software renderer',
    'unused': 'unused: PORT (preserve)', 'debug': 'debug or dev tool: PORT (preserve)',
    'devdialog-logic': 'dev dialog (MFC), its logic: PORT (preserve)',
    'devdialog-shell': 'dev dialog (MFC), its window plumbing: replaced',
    'missing': 'MISSING LOGIC', 'missing-mod': 'MISSING LOGIC (only data the shipped game lacks reaches it)',
    'unsure': 'UNSURE',
}


def write_doc(o, g, rows):
    """docs/port-audit.md: the labels by class, then a table per module."""
    labels = {}
    for line in open(o.labels, errors='replace'):
        parts = line.rstrip('\n').split('\t')
        if len(parts) >= 3:
            labels[parts[0]] = (parts[1], parts[2])
    byaddr = {r['addr']: r for r in rows}
    mw3_labels = {}
    if o.game == 'pm':
        for line in open(o.mw3_labels, errors='replace'):
            parts = line.rstrip('\n').split('\t')
            if len(parts) >= 3:
                mw3_labels[parts[0]] = (parts[1], parts[2])

    def label_of(r, depth=0):
        if r['addr'] in labels:
            return labels[r['addr']]
        if r.get('same') and r.get('mw3') in mw3_labels:
            lab, why = mw3_labels[r['mw3']]
            return lab, f'as MW3 {r["mw3"]}: {why}'
        sh = r['shape']
        if sh and sh[0] == 'follows' and depth < 8:
            t = byaddr.get(sh[1])
            if t:
                lab, why = label_of(t, depth + 1)
                return lab, f'follows {sh[1]}: {why}'
        if sh and sh[0] == 'folded':
            return 'folded', sh[1] + (f' ({sh[2]})' if len(sh) > 2 else '')
        return None, ''

    un = [r for r in rows if r['status'] != 'ported']
    done = [(r, *label_of(r)) for r in un]
    out = ['# Port audit: the original functions not ported (' + o.game + ')', '',
           'Every game function of the original (below the C runtime) that has no port of its own, labelled from its decomp,',
           'its callers and callees, the port\'s comments and the docs (tools/analysis/port_audit.py; labels with their',
           'reasons in tools/analysis/port_audit_labels.tsv). Addresses and plain words only.', '',
           'Classes: **folded** (into its callers or a ported class: getters, setters, constructors, destructors, thunks,',
           'container code, or covered where the port names it); **replaced** (SDL or the platform layer); **PORT (preserve)**',
           '(debug and dev tools, unused code: to be ported for preservation, not called by the game); **MISSING LOGIC**',
           '(runs in play and the port does not, or does it differently); **UNSURE** (what would settle it is said).', '']
    if o.game == 'pm':
        out += ['## Method (pm)', '',
                'Each PM function is paired with its MW3 twin by analysis-output/pm-diff (pairs.json, addrmap.json). A pair',
                'the diff calls identical takes the MW3 label ("as MW3 ..."). A changed pair under a shared port was read:',
                'those whose control flow differs from the twin\'s one by one, by file; those whose text differs but not their',
                'control flow either by reading or by **the feature compare** (tools/analysis/pm_feature_compare.py, at commit',
                '7f6e9e10): the calls and data references (MW3\'s mapped to PM), the immediates and the float constants by their',
                'raw values, as multisets over the function, and the number of field accesses, field stores and distinct field',
                'offsets (the offsets themselves reported apart: PM\'s layouts shift them). A sample of 40 was read whole; the',
                'functions the compare flags are read one by one, the rest are labelled "folded, by the feature compare". Its',
                'earlier versions counted no field accesses and passed added reads and writes (a new field cleared, a new flag',
                'byte): the 130 they passed and this one flags were taken back to be read. **Residual risk**: the compare cannot',
                'see an access moved from one existing field to another the function already touches, with the counts unchanged;',
                'the offsets are not yet mapped through the pm port\'s PM structures to check each one against the layout\'s shift.',
                'A call the address map cannot place counts as "unmapped" on both sides, so a caller passes with an unmapped',
                'callee that differs from its MW3 counterpart: of the 867 folded by the compare, 161 have such calls (68 callee',
                'pairs, matched by their place in the call order: 47 equal by the same compare, the other 21 mispaired by that',
                'order or differing). Each unmapped PM callee has a row of its own here, so a difference in it is labelled on',
                'that row, not on its caller\'s.',
                'A function reached only through a table is paired **by its slot**: the MW3 function in the same slot of',
                'its vtable\'s MW3 twin (pm-diff/vtables.json). A pair the feature compare finds equal takes the MW3 function\'s',
                'class, the label naming the pairing; a pair that differs is read, since PM\'s longer vtables move the later',
                'slots (the pairing then names another function). A scalar deleting destructor (one call to a destructor,',
                'then delete when bit 0 is set, matched on its whole body) is folded, its destructor labelled on its own row.',
                'A changed pair is a **layout shift** (tools/analysis/pm_layout_shift.py) when the compare flags nothing but',
                'immediates that pair one MW3 value off for one PM value, each a multiple of 4 and at least 0x40 (the offsets',
                'and sizes of PM\'s larger structures), and float constants against their negatives, each only added or subtracted with the operation swapped (fsub -c written fadd c, checked by instruction): no',
                'call, no field count, no other immediate. Such a pair is folded, the label naming its pairs; a small or odd',
                'value (a mask, a count) is read. A multiple of 4 can also be a tuning value, so each matched value\'s uses',
                'were then checked (tools/analysis/pm_layout_shift_uses.py): of the 72, 16 used one other than as an address',
                'offset or a size before operator new; read, 15 were element sizes for the vector constructor and destructor',
                'iterators or a malloc wrapper, error-report line numbers, a stack frame\'s size or a record\'s stride, and',
                'one was a real value (0x0057a170, the static\'s noise table: 40000 bytes for 25600), taken off the rule.',
                'The compare up to 7f6e9e10 counted data references but did not flag them, and an unmapped string counted as',
                '"unmapped" whatever its text, so a function naming another string passed (0x00430a70: "mech_targeting" for',
                'MW3\'s "mech_%s"). It now keys each string of 4+ characters by its text (a source path by its file name),',
                'flags data references, and keeps apart what is no data: code addresses (exception handlers pushed),',
                'negatives (an SEH frame\'s -1) and values outside the image (flags). The 1,627 labels resting on the compare',
                'were rechecked with it: 20 flag a reference; 12 had been read already, of the other 8 seven are register',
                'reuse, a return written twice, a COMDAT-folded callback or the DirectInput data format (in MW3\'s .text), and',
                'one was real (0x00430a70, labelled missing). The layout-shift rule refuses a reference or string difference too.',
                '**Checked by test** means a passing PM differential test ran the',
                'function\'s body (a block trace of the emulator, per entry the test calls) and compares what that body',
                'produces; the label names the compared value. Reaching a function, or running a caller, is not enough.', '']
    from collections import Counter as C
    cnt = C(lab or 'unlabelled' for _, lab, _ in done)
    out += ['| Class | Functions |', '|---|---|'] + [f'| {LABEL_WORDS.get(k, k)} | {n} |' for k, n in cnt.most_common()] + ['']
    out += ['## MISSING LOGIC', '']
    for kind in ('missing', 'missing-mod'):
        groups = {}
        for r, lab, why in done:
            if lab == kind:
                groups.setdefault(re.sub(r'^(follows 0x[0-9a-f]+: )+', '', why), []).append(r)
        for why, rs in groups.items():
            lines = sum(x['lines'] for x in rs)
            out.append(f'- **{", ".join(x["addr"] for x in rs[:6])}{" ..." if len(rs) > 6 else ""}** ({len(rs)} functions, {lines} decomp lines; '
                       f'{LABEL_WORDS[kind]}): {why}')
    broken = ROOT / 'tools' / 'analysis' / ('port_audit_broken_pm.tsv' if o.game == 'pm' else 'port_audit_broken.tsv')
    if broken.exists():
        out += ['', '## BROKEN (null hook)', '',
                'Hooks the ported code calls that the program leaves unset (a null field, a census stand-in, or a',
                'base virtual that only logs), which shipped play reaches. "Shown by": the chain runs (the census',
                'of a campaign, Instant Action or training run) or the static check (the data and the callers).', '',
                '| Hook | Original | Reached by | The original does | In the port | Seen in play | Shown by |',
                '|---|---|---|---|---|---|---|']
        notes = []
        for line in broken.read_text().splitlines():
            if line.startswith('> '):
                notes.append(line[2:])
            elif line.strip() and not line.startswith('#'):
                out.append('| ' + ' | '.join(line.split('\t')) + ' |')
        for n in notes:
            out += ['', n]
    out += ['', '## PORT (preserve)', '']
    for kind in ('debug', 'devdialog-logic', 'unused'):
        groups = {}
        for r, lab, why in done:
            if lab == kind:
                groups.setdefault(re.sub(r'^(follows 0x[0-9a-f]+: )+', '', why), []).append(r)
        if groups:
            out.append(f'### {LABEL_WORDS[kind]}')
            out.append('')
            for why, rs in groups.items():
                out.append(f'- {len(rs)} functions, {sum(x["lines"] for x in rs)} decomp lines ({", ".join(x["addr"] for x in rs[:4])}'
                           f'{" ..." if len(rs) > 4 else ""}): {why}')
            out.append('')
    out += ['## UNSURE', '']
    groups = {}
    for r, lab, why in done:
        if lab == 'unsure':
            groups.setdefault(re.sub(r'^(follows 0x[0-9a-f]+: )+', '', why), []).append(r)
    for why, rs in groups.items():
        out.append(f'- {", ".join(x["addr"] for x in rs)} ({len(rs)} functions, {sum(x["lines"] for x in rs)} decomp lines): {why}')
    out += ['', '## By module', '']
    mods = sorted({r['module'] for r in un}, key=lambda m: -sum(1 for r in un if r['module'] == m))
    for m in mods:
        rs = [(r, lab, why) for r, lab, why in done if r['module'] == m]
        out += [f'### {m} ({len(rs)} not ported)', '', '| Address | Lines | Class | Why |', '|---|---|---|---|']
        for r, lab, why in rs:
            out.append(f'| {r["addr"]} | {r["lines"]} | {LABEL_WORDS.get(lab, lab or "unlabelled")} | {why.replace("|", "/")} |')
        out.append('')
    Path(o.doc).write_text('\n'.join(out) + '\n')
    print(f'wrote {o.doc}: ' + ', '.join(f'{k} {n}' for k, n in cnt.most_common()))


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
    ap.add_argument('--doc', help='write the audit document (docs/port-audit.md) from the labels')
    ap.add_argument('--labels', help='the labels (default port_audit_labels.tsv, PM: port_audit_labels_pm.tsv)')
    ap.add_argument('--mw3-labels', default=str(ROOT / 'tools' / 'analysis' / 'port_audit_labels.tsv'),
                    help="PM: the MW3 labels a same-logic pair takes")
    o = ap.parse_args()
    if not o.labels:
        o.labels = str(ROOT / 'tools' / 'analysis' / ('port_audit_labels_pm.tsv' if o.game == 'pm' else 'port_audit_labels.tsv'))
    g = GAMES[o.game]
    f2f = json.load(open(g['f2f']))
    crt = crt_start(f2f, g['crt'])
    every = sorted(int(a, 16) for a in f2f)
    game = [a for a in every if a < crt]
    ported, named = scan_src(Path(o.src), g['tag'])
    # Pirate's Moon: a PM function paired with an MW3 one (pm-diff/pairs.json)
    # whose logic is the same (identical, or changed in its instructions only)
    # is ported where the shared MW3 port is; named where it is; and takes the
    # MW3 label otherwise. A pair whose calls, strings, constants or imports
    # changed is 'changed': the shared port may not do PM's version.
    pairs, mw3p, mw3n = {}, {}, {}
    if o.game == 'pm':
        mw3p, mw3n = scan_src(Path(o.src), GAMES['mw3']['tag'])
        # A bare PM address in a comment is most often an MW3 address that
        # happens to match: a name counts only in PM's own code (src/pm/) or
        # on a line that says Pirate's Moon / PM.
        src_root = Path(o.src).parent
        def pm_ref(ref):
            fn, n = ref.rsplit(':', 1)
            if '/pm/' in fn or re.search(r'_pm\.\w+$', fn):  # src/pm/, or the pm port's *_pm files
                return True
            lines = src_lines.setdefault(fn, (src_root / fn).read_text(errors='replace').splitlines())
            line = lines[int(n) - 1]
            if re.search(r"Pirate|\bPM\b|\bpm\b", line):
                return True
            # Or inside a function the pm port headed as Pirate's Moon's: the
            # nearest header above (either game's) is a Pirate's Moon one, with
            # no function's end (a '}' in the first column) between, and in a
            # header file no blank line between (declarations run on).
            header = fn.endswith(('.h', '.hpp', '.inc'))
            for k in range(int(n) - 1, -1, -1):
                if re.search(r"//\s*Mech3\.exe 1\.2 @", lines[k]):
                    return False
                if re.search(r"//\s*Pirate's Moon 1\.0 @", lines[k]):
                    return True
                if k < int(n) - 1 and (lines[k].startswith('}') or (header and not lines[k].strip())):
                    return False
            return False
        src_lines = {}
        named = defaultdict(list, {a: [r for r in rs if pm_ref(r)] for a, rs in named.items()})
        named = defaultdict(list, {a: rs for a, rs in named.items() if rs})
        for x in json.load(open(A / 'pm-diff' / 'pairs.json')):
            if 'pm' in x and 'mw3' in x:
                pairs[int(x['pm'], 16)] = x
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
        x = pairs.get(a)
        m = int(x['mw3'], 16) if x else None
        same = bool(x) and (x['kind'] == 'identical' or not x.get('feature_diff'))
        shared = ''
        if status != 'ported' and m is not None:
            if m in mw3p:
                status, shared = ('ported' if same else 'changed'), f'{mw3p[m]} (shared: MW3 0x{m:08x})'
            elif m in mw3n and status == 'none':
                status = 'named'
                named[a] = mw3n[m]
        sa = m if m is not None else a
        rows.append(dict(
            addr=f'0x{a:08x}', status=status, module=module_of(path), file=path,
            lines=decomp_lines(g['decomp'], a),
            named_in=sorted(set(named.get(a, [])))[:6] if status in ('named', 'changed') else [],  # changed: where the pm port mentions it, to read with
            callers=[f'0x{c:08x}' for c in sorted(callers[a])],
            callees=[f'0x{c:08x}' for c in sorted(callees[a])],
            refs=[f'0x{c:08x}' for c in sorted(refs[a])],
            tables=[f'0x{t:08x}' for t in tables[a][:8]],
            from_ported=any(c in ported for c in callers[a]),
            reached=a in reach,
            ported_in=ported.get(a, '') or shared,
            shape=None if status in ('ported', 'changed') else (['folded', 'ported, by docs/symbols.csv: ' + sym[sa][:120]]
                                             if (o.game == 'mw3' or same) and sym.get(sa, '').lower().startswith('ported') else shape(body_of(g['decomp'], a))),
            symbols=sym.get(sa, '') if (o.game == 'mw3' or m is not None) else '',
            near='' if a in ported else near(a),
            mw3=f'0x{m:08x}' if m is not None else '',
            pair=x['kind'] if x else ('new in PM' if o.game == 'pm' else ''),
            same=same,
        ))
    done_ported = {int(r['addr'], 16) for r in rows if r['status'] == 'ported'}
    for r in rows:
        r['from_ported'] = any(int(c, 16) in done_ported for c in r['callers'])
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
    # A function of 40 decomp lines or fewer that the port names only in
    # ordinary comments of its own code is covered there (longer ones are read) (the comment says where); one named in an unported() hook,
    # or with "not ported", "deferred", "TODO", "not yet", is read instead.
    flag = re.compile(r'unported|not ported|deferred|todo|not yet|missing|stub|note\(', re.I)
    # Hook fields the program may leave null (tests/hook_allow.txt): their
    # declarations name the original's function but do not port it.
    allowed = set()
    for line in open(ROOT / 'tests' / 'hook_allow.txt', errors='replace'):
        if line.strip() and not line.startswith('#') and '\t' in line:
            allowed.add(line.split('\t')[0].split('.')[-1])
    # and the census stand-ins (src/mech3/hook_census.inc): null in the
    # program too, only counted.
    for m in re.finditer(r'^SKIP\([\w:]+,\s*(\w+),', (ROOT / 'src' / 'mech3' / 'hook_census.inc').read_text(), re.M):
        allowed.add(m.group(1))
    field = re.compile(r'\(\*(\w+)\)\s*\(')
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
        null_hook = any((m := field.search(t)) and m.group(1) in allowed for t in texts)
        if r['lines'] <= 40 and texts and not null_hook and not any(flag.search(t) for t in texts) and not all(fn.startswith('tests/') for fn in refs_all):
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
    if o.doc:
        write_doc(o, g, rows)
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
