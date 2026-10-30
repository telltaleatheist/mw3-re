"""Pairs every Pirate's Moon function with its MW3 1.2 twin (docs/pirates-moon.md).

  pmdiff.py [--out DIR]       run from the repo root; writes DIR (default
                              analysis-output/pm-diff) pairs.json and report.txt

Inputs (analysis-output, git-ignored): the objdump listings with bytes,
pm-diff/mw3/mech3-bytes.asm and pm.asm, the Ghidra dumps pm-diff/mw3/functions.jsonl and
pm-decomp/functions.jsonl (tools/ghidra/DumpFunctions.java), MW3's
func2file.json; the two exes from game/.

Stages:
 1. bytes: each function's bytes with the absolute addresses (the
    relocations, which both exes were linked without: the operands' image
    addresses) and the displacements of calls and jumps that leave it masked; a hash found
    once on each side pairs them.
 2. anchors: a string held by one function on each side (assert paths
    compared by file name).
 3. neighbours, repeated until nothing changes: each pair's calls in
    order, its unpaired callees aligned; the unpaired runs between two
    pairs in link order aligned by score; within a source file (or the
    link-order range between two), mutual best matches.
 4. what is left in each group: mutual best at a lower threshold.
Each pair is then identical (the same instructions once addresses are
named by what they point at: a string's text, an .rdata constant's value,
a call's partner) or changed, measured by the instructions inserted,
deleted or replaced (difflib).
"""
import bisect
import difflib
import hashlib
import json
import os
import re
import struct
import sys
from collections import Counter, defaultdict

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
AO = os.path.join(ROOT, 'analysis-output')


class PE:
    def __init__(self, path):
        self.b = open(path, 'rb').read()
        b = self.b
        pe = struct.unpack_from('<I', b, 0x3c)[0]
        nsec = struct.unpack_from('<H', b, pe + 6)[0]
        opt = pe + 24
        optsize = struct.unpack_from('<H', b, pe + 20)[0]
        self.base = struct.unpack_from('<I', b, opt + 28)[0]
        reloc_rva, reloc_size = struct.unpack_from('<II', b, opt + 96 + 5 * 8)
        self.secs = []
        for i in range(nsec):
            s = pe + 24 + optsize + i * 40
            name = b[s:s + 8].rstrip(b'\0').decode('latin-1')
            vsize, va, rsize, raw = struct.unpack_from('<IIII', b, s + 8)
            flags = struct.unpack_from('<I', b, s + 36)[0]
            self.secs.append((name, self.base + va, max(vsize, rsize), raw, rsize, flags))
        self.imports = {}  # IAT slot -> "dll!name"
        imp_rva = struct.unpack_from('<I', b, opt + 96 + 1 * 8)[0]
        at = self.off(self.base + imp_rva) if imp_rva else None
        while at is not None:
            ilt, _, _, name, iat = struct.unpack_from('<IIIII', b, at)
            if not name:
                break
            dll = self.cstr(self.base + name).lower()
            k = 0
            while True:
                e = self.dword(self.base + (ilt or iat) + 4 * k)
                if not e:
                    break
                fn = f'#{e & 0xffff}' if e & 0x80000000 else self.cstr(self.base + e + 2)
                self.imports[self.base + iat + 4 * k] = f'{dll}!{fn}'
                k += 1
            at += 20
        self.relocs = set()  # MW3's and PM's exes are linked without .reloc: masked from the operands then
        at = self.off(self.base + reloc_rva) if reloc_rva else None
        end = at + reloc_size if at is not None else 0
        while at is not None and at + 8 <= end:
            page, size = struct.unpack_from('<II', b, at)
            if size < 8:
                break
            for k in range((size - 8) // 2):
                e = struct.unpack_from('<H', b, at + 8 + 2 * k)[0]
                if e >> 12 == 3:
                    self.relocs.add(self.base + page + (e & 0xfff))
            at += size

    def sec(self, va):
        for s in self.secs:
            if s[1] <= va < s[1] + s[2]:
                return s
        return None

    def off(self, va):
        s = self.sec(va)
        if not s or va - s[1] >= s[4]:
            return None
        return s[3] + va - s[1]

    def cstr(self, va):
        o = self.off(va)
        return self.b[o:self.b.find(b'\0', o)].decode('latin-1') if o is not None else ''

    def string(self, va):
        o = self.off(va)
        if o is None:
            return None
        e = self.b.find(b'\0', o, o + 300)
        if e < 0 or e - o < 3:
            return None
        s = self.b[o:e]
        if all(32 <= c < 127 or c in (9, 10, 13) for c in s):
            return s.decode('latin-1')
        return None

    def dword(self, va):
        o = self.off(va)
        return struct.unpack_from('<I', self.b, o)[0] if o is not None and o + 4 <= len(self.b) else None


LINE = re.compile(r'^\s*([0-9a-f]+):\s((?:[0-9a-f]{2} )+)\s*\t(\S+)(?:\t(.*))?$')
HEX = re.compile(r'0x([0-9a-f]+)')
BRANCH = {'call', 'jmp'} | {'j' + c for c in 'o no b nb ae nae e z ne nz be nbe a na s ns p np l nl ge nge le nle g ng c nc ecxz'.split()}


def read_asm(path):
    addrs, ins = [], {}
    for line in open(path, encoding='latin-1'):
        m = LINE.match(line)
        if not m:
            continue
        a = int(m.group(1), 16)
        ops = (m.group(4) or '').split(' <')[0].strip()
        ins[a] = (bytes.fromhex(m.group(2).replace(' ', '')), m.group(3), ops)
        addrs.append(a)
    addrs.sort()
    return addrs, ins


class Exe:
    def __init__(self, label, exe, asm, dump, func2file=None):
        self.label = label
        self.pe = PE(exe)
        self.addrs, self.ins = read_asm(asm)
        self.f = {}
        for line in open(dump):
            d = json.loads(line)
            if d['thunk']:
                continue
            if d['name'].startswith('Unwind@'):  # the compiler's exception-unwind funclets: counted, not paired
                self.unwind = getattr(self, 'unwind', 0) + 1
                continue
            d['a'] = int(d['addr'], 16)
            # an assert path by its file name: MW3 built from C:\zipper\GameZMech3\, PM from C:\zipper\branchep1\gamez\
            d['paths'] = [x for x in d['strings'] if re.search(r'^[a-z]:\\zipper\\', x, re.I)]
            d['strings'] = [x.split('\\')[-1].lower() if x in d['paths'] else x for x in d['strings']]
            d['r'] = [(int(x, 16), int(y, 16)) for x, y in d['ranges']]
            self.f[d['a']] = d
        self.order = sorted(self.f)
        self.file = {}
        if func2file:
            self.file = {int(k, 16): v for k, v in func2file.items()}
        for a, d in self.f.items():
            d['body'] = self.body(d)
            d['consts'], d['offsets'] = self.constants(d)

    def constants(self, d):
        """Immediates (not addresses, not in a memory operand) and the .rdata
        constants' values it reads; and the field offsets of its memory
        operands (a class that grew moves these)."""
        consts, offsets = set(), set()
        for a in d['body']:
            raw, mn, ops = self.ins[a]
            if mn in BRANCH:
                continue
            inner = re.findall(r'\[([^\]]*)\]', ops)
            outer = re.sub(r'\[[^\]]*\]', '', ops)
            for v in re.findall(r'(?<![\w])(?:0x[0-9a-f]+|\d+)(?![\w])', outer):
                n = int(v, 16) if v.startswith('0x') else int(v)
                if n >= 0x100 and not self.pe.sec(n) and n < 0xffff0000:
                    consts.add(n)
            for m in inner:
                for v in HEX.findall(m):
                    n = int(v, 16)
                    s = self.pe.sec(n)
                    if s and s[0] == '.rdata' and n not in self.pe.imports and self.pe.string(n) is None:
                        w = self.pe.dword(n)
                        if w is not None:
                            consts.add(('K', w))
                    elif not s and ('esp' not in m and 'ebp' not in m):
                        offsets.add(n)
        return sorted(consts, key=str), sorted(offsets)

    def body(self, d):
        out = []
        for lo, hi in d['r']:
            i = bisect.bisect_left(self.addrs, lo)
            while i < len(self.addrs) and self.addrs[i] < hi:
                out.append(self.addrs[i])
                i += 1
        return out

    def inside(self, d, t):
        return any(lo <= t < hi for lo, hi in d['r'])

    def masked(self, d):
        h = hashlib.sha1()
        for a in d['body']:
            raw, mn, ops = self.ins[a]
            m = bytearray(raw)
            for k in range(len(raw) - 3):
                if a + k in self.pe.relocs:
                    m[k:k + 4] = b'\0\0\0\0'
            for v in HEX.findall(ops):  # absolute addresses in the image, where the bytes hold them
                v = int(v, 16)
                if self.pe.sec(v):
                    k = bytes(m).rfind(struct.pack('<I', v))
                    if k >= 0:
                        m[k:k + 4] = b'\0\0\0\0'
            if mn in BRANCH:
                t = HEX.match(ops)
                if t and not self.inside(d, int(t.group(1), 16)):
                    n = 4 if len(raw) >= 5 else 1
                    m[len(raw) - n:] = b'\0' * n
            h.update(bytes(m))
        return h.hexdigest()

    def callees(self, d):
        out = []
        for a in d['body']:
            raw, mn, ops = self.ins[a]
            if mn in ('call', 'jmp'):
                t = HEX.match(ops)
                if t:
                    v = int(t.group(1), 16)
                    if v in self.f and not self.inside(d, v):
                        out.append(v)
        return out

    def norm(self, d, partner):
        """The instructions with addresses named by what they point at."""
        out = []
        start = d['a']
        for a in d['body']:
            raw, mn, ops = self.ins[a]
            if mn in BRANCH and HEX.match(ops):
                v = int(HEX.match(ops).group(1), 16)
                if self.inside(d, v):
                    out.append(f'{mn} +{v - start:x}')
                else:
                    out.append(f'{mn} {partner(v)}')
                continue

            def name(m):
                v = int(m.group(1), 16)
                s = self.pe.sec(v)
                if not s:
                    return m.group(0)
                if v in self.pe.imports:
                    return 'I' + self.pe.imports[v]
                st = self.pe.string(v)
                if st is not None:
                    return 'S' + json.dumps(st)
                if s[0] == '.rdata':
                    w = self.pe.dword(v)
                    return f'K{w:x}' if w is not None else 'K?'
                if s[5] & 0x20000000:  # code
                    return partner(v)
                return 'G'
            out.append(mn + ' ' + HEX.sub(name, ops))
        return out


def basename(path):
    """A file, or a link-order range between two ("(between a.cpp and b.cpp)"), by file names alone."""
    if not path:
        return None
    return re.sub(r'[^\s()]*\\', '', path.lower())


def file_ranges(exe, path_strings):
    """func2file by link order from the functions holding an assert path."""
    pat = re.compile(r'([a-z]:\\zipper\\.*\.(?:c|cpp|h))$|^(\.\.\\.*\.(?:c|cpp|h))$|^([a-z_0-9]+\.(?:c|cpp))$', re.I)
    owners = defaultdict(list)
    for a, d in exe.f.items():
        for s in d['paths']:
            m = pat.search(s)
            if m:
                p = s.lower().replace('c:\\zipper\\', '')
                if not p.endswith('.h'):
                    owners[p].append(a)
    files = sorted(((min(v), max(v), k) for k, v in owners.items()), key=lambda t: t[0])
    out = {}
    for a in exe.order:
        owner = None
        for i, (lo, hi, k) in enumerate(files):
            if lo <= a <= hi:
                owner = k
                break
            if a < lo:
                owner = f'(between {files[i - 1][2] if i else "start"} and {k})'
                break
        out[a] = owner or f'(after {files[-1][2]})'
    return out


def jacc(x, y):
    x, y = set(x), set(y)
    return len(x & y) / len(x | y) if x or y else 1.0


def score(da, db):
    s = 0.0
    s += 3 * jacc(da['strings'], db['strings']) if (da['strings'] or db['strings']) else 0.5
    s += 2 * jacc(da['imports'], db['imports']) if (da['imports'] or db['imports']) else 0.3
    s += 2 * jacc(da['consts'], db['consts']) if (da['consts'] or db['consts']) else 0.3
    na, nb = len(da['body']), len(db['body'])
    s += 2 * min(na, nb) / max(na, nb, 1)
    s += 1 - abs(len(da['callees']) - len(db['callees'])) / max(len(da['callees']), len(db['callees']), 1)
    s += 1 - abs(len(da['callers']) - len(db['callers'])) / max(len(da['callers']), len(db['callers']), 1)
    ma, mb = da['mnemonics'].split(), db['mnemonics'].split()
    if max(len(ma), len(mb)) < 3000:
        s += 3 * difflib.SequenceMatcher(None, ma, mb, autojunk=False).ratio()
    return s  # out of 14


def prologue(e, d):
    """The code generation's mark: a frame pointer (push ebp; mov ebp, esp) or not."""
    m = [e.ins[x][1] + ' ' + e.ins[x][2] for x in d['body'][:3]]
    return 'push ebp' in m[:1] and 'mov ebp, esp' in m[1:3]


def main():
    out_dir = os.path.join(AO, 'pm-diff')
    if '--out' in sys.argv:
        out_dir = sys.argv[sys.argv.index('--out') + 1]
    os.makedirs(out_dir, exist_ok=True)
    mw3 = Exe('MW3 1.2', os.path.join(ROOT, 'game/mw3-1.2/Mech3.exe'), os.path.join(AO, 'pm-diff/mw3/mech3-bytes.asm'),
              os.path.join(AO, 'pm-diff/mw3/functions.jsonl'), json.load(open(os.path.join(AO, 'func2file.json'))))
    pm = Exe('PM 1.0', os.path.join(ROOT, 'game/pm/Mech3.exe'), os.path.join(AO, 'pm.asm'),
             os.path.join(AO, 'pm-decomp/functions.jsonl'))
    print(f'functions: MW3 {len(mw3.f)}, PM {len(pm.f)} (and {mw3.unwind}, {pm.unwind} unwind funclets)', flush=True)
    pm.file = file_ranges(pm, None)
    json.dump({hex(a): v for a, v in pm.file.items()}, open(os.path.join(AO, 'pm-func2file.json'), 'w'), indent=0)
    mw3_own = file_ranges(mw3, None)
    for e in (mw3, pm):
        for d in e.f.values():
            d['callees'] = e.callees(d)
            d['hash'] = e.masked(d)
            d['imports'] = sorted(set(d['imports']) | {e.pe.imports[int(v, 16)] for x in d['body'] for v in HEX.findall(e.ins[x][2])
                                                        if int(v, 16) in e.pe.imports})

    pair, back, how = {}, {}, {}

    def take(a, b, why):
        if a in pair or b in back:
            return False
        pair[a], back[b], how[a] = b, a, why
        return True

    def report(stage):
        print(f'{stage}: {len(pair)} pairs', flush=True)

    # 1. masked bytes, unique on both sides
    ha, hb = Counter(d['hash'] for d in mw3.f.values()), Counter(d['hash'] for d in pm.f.values())
    byh = {d['hash']: b for b, d in pm.f.items() if hb[d['hash']] == 1}
    for a, d in mw3.f.items():
        if ha[d['hash']] == 1 and d['hash'] in byh:
            take(a, byh[d['hash']], 'bytes')
    report('stage 1 (bytes)')

    # 2. anchors: a string held by one function on each side
    sa, sb = defaultdict(set), defaultdict(set)
    for a, d in mw3.f.items():
        for x in d['strings']:
            sa[x].add(a)
    for b, d in pm.f.items():
        for x in d['strings']:
            sb[x].add(b)
    for x in sa:
        if len(sa[x]) == 1 and len(sb.get(x, ())) == 1 and len(x) >= 6:
            a, b = next(iter(sa[x])), next(iter(sb[x]))
            if jacc(mw3.f[a]['strings'], pm.f[b]['strings']) >= 0.4:
                take(a, b, 'strings')
    report('stage 2 (unique strings)')

    # 3. neighbours, repeated
    def group(own, a):
        return basename(own.get(a))
    ga, gb = defaultdict(list), defaultdict(list)
    for a in mw3.order:
        ga[group(mw3.file if a in mw3.file else mw3_own, a) or group(mw3_own, a)].append(a)
    for b in pm.order:
        gb[group(pm.file, b)].append(b)
    ib = {b: i for i, b in enumerate(pm.order)}
    scores = {}

    def sc(a, b):
        k = (a, b)
        if k not in scores:
            scores[k] = score(mw3.f[a], pm.f[b])
        return scores[k]

    def align(ua, ub, threshold, why):
        """Unpaired runs in order: a dynamic programme over the scores."""
        if not ua or not ub or len(ua) * len(ub) > 20000:
            return 0
        n, m = len(ua), len(ub)
        best = [[0.0] * (m + 1) for _ in range(n + 1)]
        for i in range(n - 1, -1, -1):
            for j in range(m - 1, -1, -1):
                s_ = sc(ua[i], ub[j])
                take_ = (s_ - threshold + best[i + 1][j + 1]) if s_ >= threshold else -1
                best[i][j] = max(best[i + 1][j], best[i][j + 1], take_)
        i = j = added = 0
        while i < n and j < m:
            s_ = sc(ua[i], ub[j])
            if s_ >= threshold and best[i][j] == s_ - threshold + best[i + 1][j + 1]:
                added += take(ua[i], ub[j], why)
                i += 1
                j += 1
            elif best[i][j] == best[i + 1][j]:
                i += 1
            else:
                j += 1
        return added

    for it in range(30):
        added = 0
        # (a) each pair's calls in order: unpaired callees aligned
        for a, b in list(pair.items()):
            la = [x for x in mw3.f[a]['callees'] if x in mw3.f]
            lb = [y for y in pm.f[b]['callees'] if y in pm.f]
            ta = [pair.get(x, 'U') for x in la]
            tb = [y if y in back else 'U' for y in lb]
            sm = difflib.SequenceMatcher(None, ta, tb, autojunk=False)
            for op, i1, i2, j1, j2 in sm.get_opcodes():
                if op == 'equal':
                    for k in range(i2 - i1):
                        x, y = la[i1 + k], lb[j1 + k]
                        if x not in pair and y not in back and sc(x, y) >= 6:
                            added += take(x, y, 'calls')
                elif op == 'replace':
                    added += align([x for x in la[i1:i2] if x not in pair], [y for y in lb[j1:j2] if y not in back], 7, 'calls')
        # (b) link order: the unpaired runs between two pairs
        prev = None
        for a in mw3.order:
            if a not in pair:
                continue
            if prev is not None and ib[pair[a]] > ib[pair[prev]]:
                i0, i1 = mw3.order.index(prev) if False else None, None
            prev = a
        pa = sorted(pair)
        pos_a = {a: i for i, a in enumerate(mw3.order)}
        for k in range(len(pa) - 1):
            x0, x1 = pa[k], pa[k + 1]
            y0, y1 = pair[x0], pair[x1]
            if not ib[y0] < ib[y1]:
                continue
            ua = [x for x in mw3.order[pos_a[x0] + 1:pos_a[x1]] if x not in pair]
            ub = [y for y in pm.order[ib[y0] + 1:ib[y1]] if y not in back]
            added += align(ua, ub, 7, 'link order')
        # (c) structure within a source file, mutual best
        for g in ga:
            if g is None or g not in gb:
                continue
            ua = [a for a in ga[g] if a not in pair]
            ub = [b for b in gb[g] if b not in back]
            if not ua or not ub or len(ua) * len(ub) > 40000:
                continue
            best_a, best_b = {}, {}
            for a in ua:
                for b in ub:
                    s_ = sc(a, b)
                    if s_ > best_a.get(a, (0, None))[0]:
                        best_a[a] = (s_, b)
                    if s_ > best_b.get(b, (0, None))[0]:
                        best_b[b] = (s_, a)
            for a, (s_, b) in best_a.items():
                if s_ >= 9 and best_b.get(b, (0, None))[1] == a:
                    added += take(a, b, 'structure')
        report(f'stage 3 round {it + 1} (+{added})')
        if not added:
            break

    # 4. what is left in each group, mutual best at a lower threshold
    for it in range(5):
        added = 0
        for g in ga:
            if g is None or g not in gb:
                continue
            ua = [a for a in ga[g] if a not in pair]
            ub = [b for b in gb[g] if b not in back]
            if not ua or not ub or len(ua) * len(ub) > 40000:
                continue
            best_a, best_b = {}, {}
            for a in ua:
                for b in ub:
                    s_ = sc(a, b)
                    if s_ > best_a.get(a, (0, None))[0]:
                        best_a[a] = (s_, b)
                    if s_ > best_b.get(b, (0, None))[0]:
                        best_b[b] = (s_, a)
            for a, (s_, b) in best_a.items():
                if s_ >= 7.5 and best_b.get(b, (0, None))[1] == a:
                    added += take(a, b, 'loose')
        report(f'stage 4 round {it + 1} (+{added})')
        if not added:
            break

    # Sorting: identical, or changed by what the compiler cannot make up
    # (calls, strings, constants, imports added or removed) and by the
    # instructions inserted, deleted or replaced.
    def partner_a(v):
        return f'F{v:x}' if v in pair else 'F?'

    def partner_b(v):
        return f'F{back[v]:x}' if v in back else 'F?'

    rows = []
    for a, b in pair.items():
        da, db = mw3.f[a], pm.f[b]
        na, nb = mw3.norm(da, partner_a), pm.norm(db, partner_b)
        r = {'mw3': hex(a), 'pm': hex(b), 'how': how[a], 'size': [len(na), len(nb)]}
        ca = Counter(partner_a(x) for x in da['callees'])
        cb = Counter(partner_b(y) for y in db['callees'])
        feat = {
            'calls': [sum((ca - cb).values()), sum((cb - ca).values())],
            'strings': [len(set(da['strings']) - set(db['strings'])), len(set(db['strings']) - set(da['strings']))],
            'consts': [len(set(da['consts']) - set(db['consts'])), len(set(db['consts']) - set(da['consts']))],
            'imports': [len(set(da['imports']) - set(db['imports'])), len(set(db['imports']) - set(da['imports']))],
        }
        r['features'] = feat
        r['offsets'] = [len(set(da['offsets']) - set(db['offsets'])), len(set(db['offsets']) - set(da['offsets']))]
        r['feature_diff'] = sum(sum(v) for v in feat.values())
        r['codegen'] = prologue(mw3, da) != prologue(pm, db)
        if na == nb:
            r['kind'], r['diff'] = 'identical', 0
        else:
            sm = difflib.SequenceMatcher(None, na, nb, autojunk=False)
            r['kind'] = 'changed'
            r['diff'] = sum(max(i2 - i1, j2 - j1) for op, i1, i2, j1, j2 in sm.get_opcodes() if op != 'equal')
        rows.append(r)
    for b in pm.order:
        if b not in back:
            rows.append({'pm': hex(b), 'kind': 'new in PM', 'size': [0, len(pm.f[b]['body'])]})
    for a in mw3.order:
        if a not in pair:
            rows.append({'mw3': hex(a), 'kind': 'MW3 only', 'size': [len(mw3.f[a]['body']), 0]})
    for r in rows:
        if 'mw3' in r:
            r['mw3_file'] = mw3.file.get(int(r['mw3'], 16)) or mw3_own.get(int(r['mw3'], 16))
        if 'pm' in r:
            r['pm_file'] = pm.file.get(int(r['pm'], 16))
            r['pm_strings'] = [x[:80] for x in pm.f[int(r['pm'], 16)]['strings'][:3]]
    json.dump(rows, open(os.path.join(out_dir, 'pairs.json'), 'w'), indent=0)
    c = Counter(r['kind'] for r in rows)
    print('sorted:', dict(c), 'by stage:', dict(Counter(how.values())))
    write_report(rows, mw3, pm, how, os.path.join(out_dir, 'report.txt'))


def write_report(rows, mw3, pm, how, path):
    names = {}
    sym = os.path.join(ROOT, 'docs', 'symbols.csv')
    if os.path.exists(sym):
        import csv
        for row in csv.reader(open(sym)):
            if row and row[0].startswith('0x'):
                names.setdefault(int(row[0], 16), row[1])
    out = []
    c = Counter(r['kind'] for r in rows)
    out.append(f'functions: MW3 1.2 {len(mw3.f)}, PM 1.0 {len(pm.f)} (unwind funclets left out: {mw3.unwind}, {pm.unwind})')
    out.append(f'identical {c["identical"]}, changed {c["changed"]}, new in PM {c["new in PM"]}, MW3 only {c["MW3 only"]}')
    out.append('pairs by stage: ' + ', '.join(f'{k} {v}' for k, v in Counter(how.values()).most_common()))
    ch = [r for r in rows if r['kind'] == 'changed']
    real = [r for r in ch if r['feature_diff']]
    out.append(f'changed: {len(real)} with calls, strings, constants or imports added or removed; '
               f'{len(ch) - len(real)} in their instructions only; code generation differs in '
               f'{sum(r["codegen"] for r in rows if "codegen" in r)} pairs')
    out.append('')
    out.append('By file (link order; "between" ranges hold the files without assert strings):')
    out.append(f'{"ident":>6} {"chg":>5} {"chg+":>5} {"new":>5} {"gone":>5} {"cg":>4}  file')
    groups = defaultdict(Counter)
    order = {}
    for r in rows:
        g = basename(r.get('mw3_file') or r.get('pm_file')) or '?'
        k = r['kind']
        groups[g][k] += 1
        if k == 'changed' and r['feature_diff']:
            groups[g]['chg+'] += 1
        if r.get('codegen'):
            groups[g]['cg'] += 1
        a = int(r['mw3'], 16) if 'mw3' in r else int(r['pm'], 16) - 0x20000
        order[g] = min(order.get(g, a), a)
    for g in sorted(groups, key=lambda g: order[g]):
        n = groups[g]
        out.append(f'{n["identical"]:6d} {n["changed"]:5d} {n["chg+"]:5d} {n["new in PM"]:5d} {n["MW3 only"]:5d} {n["cg"]:4d}  {g}')
    out.append('')
    out.append('Biggest changes (calls, strings, constants, imports added+removed; then instructions):')
    for r in sorted(real, key=lambda r: (-r['feature_diff'], -r['diff']))[:60]:
        a = int(r['mw3'], 16)
        f = r['features']
        out.append(f'  {r["mw3"]} -> {r["pm"]}  {names.get(a, "")[:34]:34s} feat {r["feature_diff"]:3d} '
                   f'(calls -{f["calls"][0]}+{f["calls"][1]} str -{f["strings"][0]}+{f["strings"][1]} '
                   f'const -{f["consts"][0]}+{f["consts"][1]} imp -{f["imports"][0]}+{f["imports"][1]}) '
                   f'ins {r["diff"]} of {r["size"][0]}->{r["size"][1]}  {basename(r.get("mw3_file"))}')
    out.append('')
    out.append('New in PM, by file, with their strings:')
    newg = defaultdict(list)
    for r in rows:
        if r['kind'] == 'new in PM':
            newg[basename(r.get('pm_file')) or '?'].append(r)
    for g in sorted(newg, key=lambda g: -len(newg[g])):
        big = sorted(newg[g], key=lambda r: -r['size'][1])
        strs = [x for r in big for x in r['pm_strings'] if not x.endswith(('.cpp', '.c'))][:4]
        out.append(f'  {len(big):4d} ({sum(r["size"][1] for r in big)} instructions)  {g}  {json.dumps(strs)[:160]}')
    out.append('')
    out.append('MW3 only, by file:')
    gone = defaultdict(list)
    for r in rows:
        if r['kind'] == 'MW3 only':
            gone[basename(r.get('mw3_file')) or '?'].append(r)
    for g in sorted(gone, key=lambda g: -len(gone[g])):
        big = sorted(gone[g], key=lambda r: -r['size'][0])
        nm = [names.get(int(r['mw3'], 16)) for r in big if names.get(int(r['mw3'], 16))][:4]
        out.append(f'  {len(big):4d} ({sum(r["size"][0] for r in big)} instructions)  {g}  {", ".join(nm)[:150]}')
    open(path, 'w').write('\n'.join(out) + '\n')
    print('report:', path)


if __name__ == '__main__':
    main()
