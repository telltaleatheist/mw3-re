"""The skins on a mech's model tree against the port (docs/mechdef-skins.md,
The skins in play): the first skin on the type's model 0x00475780, the
mech's own copy 0x00475a80, a later skin swapped in 0x004757f0, with the
damage skin's linking 0x00475520 / 0x00474740 under HOLES, the copy
0x00475870 / 0x00474210 / 0x00473bb0, the walks 0x004748b0 / 0x004749e0 and
the entries' load 0x00473d40, x87 untouched (no floats).

Random scenarios: textures in the table or not, with images or not, cached
images of given sizes (the damage check), skins of entries with and without
alternate names (one sometimes `vult_d03_c`), a damage skin, materials
textured or not, a small node tree. The exe's skins and entries are made by
its own constructors (0x00473e50, 0x00474000); the set and the tree by hand.
The texture table's find 0x00518220, its load 0x00518d10, zImage's cache
0x005179d0, the soil lookup 0x00535f20 and the releases are stubbed alike.
Compared: the materials (flags, texture, soil), every entry (texture, cached,
damage link, +0x14, references), the skins' loaded flags, the made skin
(shared entries or copies) and the stubs' calls in order.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import load_probe  # noqa: E402
from original import Original  # noqa: E402

probe = load_probe()
probe.probe_skin_apply.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int32]
probe.probe_skin_apply.restype = ctypes.c_int32

NEW, DELETE, STRDUP, ZERR = 0x5abd26, 0x5abd1a, 0x5bb670, 0x49bfd0
LIST, LIST_COUNT, HOLES = 0x6061bc, 0x6061c0, 0x6061cc


def s32(v):
    return struct.unpack('<i', struct.pack('<I', v & 0xffffffff))[0]


class Emu:
    def __init__(self):
        o = self.o = Original()
        self.heap = 0x31000000
        o.uc.mem_map(self.heap, 0x4000000)

        def new(c):
            return self.alloc(c.arg(0))
        o.stub_function(NEW, new)
        o.stub_function(DELETE, lambda c: 0)

        def strdup(c):
            s = self.cstr(c.arg(0))
            return self.string(s)
        o.stub_import(STRDUP, strdup)
        self.zerr = []
        o.stub_function(ZERR, lambda c: self.zerr.append(c.arg(2)) or 0)
        o.stub_import(0x5bb65c, lambda c: 0)  # free (the skin's name)
        o.stub_function(0x518220, self.find)
        o.stub_function(0x518d10, self.load, 4)
        o.stub_function(0x5179d0, self.fetch)
        o.stub_function(0x535f20, lambda c: 7)
        o.stub_function(0x517b10, lambda c: self.log.extend([6, self.tag(c.ecx)]) or 0)
        o.stub_function(0x518b30, lambda c: self.log.extend([6, self.tag(c.ecx)]) or 0)

    def alloc(self, n):
        a = self.heap
        self.heap += (max(n, 4) + 15) & ~15
        self.o.write(a, bytes(max(n, 4)))
        return a

    def string(self, s):
        a = self.alloc(len(s) + 1)
        self.o.write(a, s.encode() + b'\0')
        return a

    def cstr(self, a):
        b = self.o.read(a, 64)
        return b[:b.index(b'\0')].decode()

    def u(self, a):
        return struct.unpack('<I', self.o.read(a, 4))[0]

    def image(self, w, h):
        a = self.alloc(0x10)
        self.o.write(a + 4, struct.pack('<hh', w, h))
        self.images.append((a, w, h))
        return a

    def name_index(self, n):
        return self.names.index(n) if n in self.names else -1

    def find(self, c):
        i = self.name_index(self.cstr(c.ecx))
        self.log += [1, i]
        return self.table[i] if i >= 0 and self.tex[i][0] else 0

    def load(self, c):
        i = self.name_index(self.cstr(c.ecx))
        self.log += [2, i, s32(c.edx), s32(c.arg(0))]
        if i < 0 or not self.tex[i][1]:
            return self.deflt
        e = self.alloc(0x28)
        self.o.write(e + 8, self.names[i].encode() + b'\0')
        self.o.u32(e, self.image(self.tex[i][1], self.tex[i][2]))
        self.loaded[e] = i
        return e

    def fetch(self, c):
        i = self.name_index(self.cstr(c.ecx))
        self.log += [3, i]
        if i < 0 or not self.tex[i][3]:
            return 0
        a = self.image(self.tex[i][3], self.tex[i][4])
        self.cached[a] = i
        return a

    def tag(self, p):
        if p == 0:
            return 0
        if p == self.deflt:
            return 1
        if p in self.table:
            return 100 + self.table.index(p)
        if p in self.loaded:
            return 200 + self.loaded[p]
        if p in self.cached:
            return 300 + self.cached[p]
        return -1

    def run(self, sc):
        o = self.o
        self.log, self.images, self.loaded, self.cached, self.zerr = [], [], {}, {}, []
        it = iter(sc)
        holes, op, arg, nt, ns, ne, has_damage, nm, nn, vult = [next(it) for _ in range(10)]
        self.names = ['vult_d03_c' if (i == 0 and vult) else f'tex{i:02d}' for i in range(nt)]
        self.tex = [[next(it) for _ in range(5)] for _ in range(nt)]
        self.deflt = self.alloc(0x28)
        self.table = []
        for i in range(nt):
            e = self.alloc(0x28)
            o.write(e + 8, self.names[i].encode() + b'\0')
            o.u32(e, self.image(self.tex[i][1], self.tex[i][2]) if self.tex[i][1] else 0)
            self.table.append(e)
        # zImage's entries' global list: an empty sentinel
        sentinel = self.alloc(0xc)
        o.u32(sentinel, sentinel), o.u32(sentinel + 4, sentinel)
        o.u32(LIST, sentinel), o.u32(LIST_COUNT, 0)
        o.write(HOLES, bytes([0]))
        skins = []
        for s in range(ns + (1 if has_damage else 0)):
            sk = self.alloc(0x18)
            o.call(0x473e50, ecx=sk, stack_args=(self.string(f'skin{s}' if s < ns else 'damage'), ne))
            for _ in range(ne):
                ni, ai = next(it), next(it)
                o.call(0x474000, ecx=sk, stack_args=(self.string(self.names[ni]),
                                                     self.string(self.names[ai]) if ai >= 0 else 0))
            skins.append(sk)
        originals = []
        for sk in skins:
            b, e = self.u(sk + 8), self.u(sk + 0xc)
            originals += [self.u(b + 4 * j) for j in range((e - b) // 4)]
        st = self.alloc(0x14)
        arr = self.alloc(4 * max(ns, 1))
        for k in range(ns):
            o.u32(arr + 4 * k, skins[k])
        o.u32(st + 4, arr), o.u32(st + 8, arr + 4 * ns), o.u32(st + 0xc, arr + 4 * ns)
        o.u32(st + 0x10, skins[ns] if has_damage else 0)
        mats = []
        for _ in range(nm):
            m = self.alloc(0x2c)
            fl, t = next(it), next(it)
            o.write(m + 1, bytes([fl]))
            o.u32(m + 0x10, self.table[t] if t >= 0 else 0)
            mats.append(m)
        nodes, parents = [], []
        for _ in range(nn):
            parents.append(next(it))
            np = next(it)
            pl = [next(it) for _ in range(np)]
            node = self.alloc(0x100)
            if np:
                polys = self.alloc(0x24 * np)
                for j, mi in enumerate(pl):
                    o.u32(polys + 0x24 * j + 0x1c, mats[mi] if mi >= 0 else 0)
                model = self.alloc(0x60)
                o.u32(model + 0x10, np), o.u32(model + 0x34, polys)
                o.u32(node + 0x3c, model)
            nodes.append(node)
        for n in range(nn):
            kids = [nodes[k] for k in range(nn) if parents[k] == n]
            karr = self.alloc(4 * max(len(kids), 1))
            for j, kd in enumerate(kids):
                o.u32(karr + 4 * j, kd)
            o.u32(nodes[n] + 0x5c, len(kids)), o.u32(nodes[n] + 0x60, karr)
        o.write(HOLES, bytes([holes]))
        self.log = []
        root, lod = nodes[0], nodes[1] if nn > 1 else 0
        ret, made = 0, 0
        if op == 0:
            o.call(0x475780, ecx=st, stack_args=(root,))
        elif op == 1:
            o.call(0x475a80, ecx=st, stack_args=(arg, root))
            made = s32(o.uc.reg_read(__import__('unicorn').x86_const.UC_X86_REG_EAX)) & 0xffffffff
        else:
            o.call(0x475a80, ecx=st, stack_args=(0, root))
            slot = o.uc.reg_read(__import__('unicorn').x86_const.UC_X86_REG_EAX)
            mech = self.alloc(0x700)
            o.u32(mech + 0x3e8, slot), o.u32(mech + 0x694, root), o.u32(mech + 0x698, lod)
            to = skins[arg] if 0 <= arg < ns else 0
            o.call(0x4757f0, ecx=st, stack_args=(mech, to))
            ret = o.uc.reg_read(__import__('unicorn').x86_const.UC_X86_REG_EAX) & 0xff
            made = self.u(mech + 0x3e8)
        self.copies = self.links = 0
        out = [ret]
        for m in mats:
            out += [o.read(m + 1, 1)[0], self.tag(self.u(m + 0x10)), s32(self.u(m + 0x20))]

        def entry(e):
            dmg = self.u(e + 0x10)
            return [self.tag(self.u(e)), o.read(e + 4, 1)[0], originals.index(dmg) if dmg in originals else -1,
                    o.read(e + 0x14, 1)[0], s32(self.u(e + 0x18))]
        for e in originals:
            out += entry(e)
            self.links += self.u(e + 0x10) != 0
        for sk in skins:
            out.append(o.read(sk + 0x14, 1)[0])
        if made:
            b, e = self.u(made + 8), self.u(made + 0xc)
            ents = [self.u(b + 4 * j) for j in range((e - b) // 4)]
            out.append(len(ents))
            for en in ents:
                if en in originals:
                    out.append(-1 - originals.index(en))
                else:
                    self.copies += 1
                    nm_p, al_p = self.u(en + 8), self.u(en + 0xc)
                    out += [0, self.name_index(self.cstr(nm_p)), -1 if al_p == nm_p else self.name_index(self.cstr(al_p))]
                    out += entry(en)
        else:
            out.append(-1)
        return out + [-999] + self.log


def scenario(rng):
    nt = rng.randrange(3, 12)
    ns, ne = rng.randrange(1, 4), rng.randrange(1, 6)
    has_damage = int(rng.random() < 0.6)
    nm = rng.randrange(1, 10)
    nn = rng.randrange(1, 5)
    op = rng.choice((0, 0, 1, 1, 2, 2))
    # 0x00475a80 with an index past the skins crashes (a null skin into 0x004749e0): the callers pass 0.
    arg = rng.randrange(ns) if op == 1 else rng.randrange(-1, ns + 1) if op == 2 else 0
    sizes = [(0, 0), (16, 16), (32, 32), (32, 16), (64, 64)]
    sc = [int(rng.random() < 0.5), op, arg, nt, ns, ne, has_damage, nm, nn, int(rng.random() < 0.2)]
    for _ in range(nt):
        w, h = rng.choice(sizes)
        cw, ch = rng.choice(sizes) if rng.random() < 0.7 else (w, h)
        sc += [int(rng.random() < 0.6), w, h, cw, ch]
    for _ in range(ns + has_damage):
        for _ in range(ne):
            sc += [rng.randrange(nt), rng.choice((-1, -1, rng.randrange(nt)))]
    for _ in range(nm):  # a textured material (flag 1) always has its texture, as in the data
        t = rng.choice((-1, rng.randrange(nt), rng.randrange(nt)))
        fl = rng.choice((0x11, 0x11, 0x10, 0x01, 0x13))
        sc += [fl & ~1 if t < 0 else fl, t]
    for n in range(nn):
        np = rng.randrange(0, 5)
        sc += [rng.randrange(n) if n else -1, np] + [rng.choice((-1, rng.randrange(nm), rng.randrange(nm)))
                                                    for _ in range(np)]
    return sc


def main():
    n = int(os.environ.get('SKINS_CASES', '400'))
    rng = random.Random(0x475780)
    emu = Emu()
    out = (ctypes.c_int32 * 65536)()
    failed = 0
    seen = {'swapped': 0, 'copies': 0, 'linked': 0, 'loads': 0, 'size errors': 0, 'not loaded': 0}
    for case in range(n):
        sc = scenario(rng)
        want = emu.run(sc)
        k = probe.probe_skin_apply((ctypes.c_int32 * len(sc))(*sc), out, 65536)
        got = list(out[:k])
        if want != got:
            failed += 1
            if failed <= 3:
                i = next(i for i in range(min(len(want), len(got)) + 1) if i >= len(want) or i >= len(got) or
                         want[i] != got[i])
                print(f'FAIL case {case} (op {sc[1]} holes {sc[0]}) at {i}:\n  want {want[max(0, i - 6):i + 14]}\n'
                      f'  got  {got[max(0, i - 6):i + 14]}')
        log = want[want.index(-999) + 1:]
        seen['loads'] += log.count(2)
        seen['swapped'] += sc[1] == 2 and want[0] == 1
        seen['copies'] += emu.copies
        seen['linked'] += emu.links
        seen['size errors'] += emu.zerr.count(0xed)
        seen['not loaded'] += emu.zerr.count(0xde)
    print(f'ok: {n} scenarios identical ({seen["swapped"]} skins swapped in, {seen["loads"]} texture loads, '
          f'{seen["copies"]} entries copied for a mech, {seen["linked"]} damage links, {seen["size errors"]} size '
          f'mismatches, {seen["not loaded"]} damage textures missing)' if not failed else f'{failed} of {n} differ')
    sys.stdout.flush()
    os._exit(1 if failed else 0)


main()
