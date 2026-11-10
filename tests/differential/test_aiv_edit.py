"""The vehicle editor's record compares (docs/editor.md, The vehicle editor) against the original x86:
AiParams::same_numbers (0x4df1b0, the store's 0x4bbd70..0x4bc110 alike), same_vlist (0x4d9090 with its entry
compare 0x4d9140), same_name (0x49d2f0), and the team mask's numbers (0x4d71a0, the editor's team round trip). Random settings blocks in the exe's layout, each pair one word, one
flag bit or nothing apart (and the float words' -0 and NaN); vlists of 0..3 entries one field apart; names
equal, unequal or missing.

    cmake --build build --target mw3probe && .venv/bin/python -P tests/differential/test_aiv_edit.py [runs]

Breaks, by hand in zai_params.cpp, each must fail: a float word compared as an int (word 1); flag bit 0x40 left
out; same_name true for two nulls; the entries' valve left out.
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

SAME_NUMBERS, SAME_VLIST, SAME_NAME = 0x4df1b0, 0x4d9090, 0x49d2f0
INT_WORDS = {0, 5, 14, 0x0f, 0x14, 0x16, 0x17, 0x1f, 0x21, 0x22, 0x25, 0x26, 0x27, 0x28, 0x2e, 0x30, 0x31}
SKIPPED = {0x32, 0x33}  # the flag bytes' word (compared by bits) and past it

probe = load_probe()
probe.probe_aiv_same_numbers.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
probe.probe_aiv_same_vlist.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_int]
probe.probe_aiv_same_name.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
probe.probe_team_number_of.argtypes = [ctypes.c_uint32]
probe.probe_team_number_of.restype = ctypes.c_uint32
TEAM_NUMBER = 0x4d71a0


def block(rng):
    b = bytearray(0xcc)
    for w in range(0x32):
        if w in INT_WORDS:
            struct.pack_into('<i', b, 4 * w, rng.choice([0, 1, -1, rng.randrange(-100, 100)]))
        else:
            struct.pack_into('<f', b, 4 * w, rng.choice([0.0, 1.0, rng.uniform(-500, 500)]))
    b[0xc8] = rng.randrange(256)
    b[0xc9] = rng.randrange(256)
    return b


def mutate(rng, b):
    o = bytearray(b)
    k = rng.randrange(8)
    if k == 0:
        return o, 'none'
    if k == 1:  # the flag bytes, one bit
        at = rng.choice([0xc8, 0xc9])
        bit = rng.randrange(8)
        o[at] ^= 1 << bit
        return o, 'flag %#x bit %d' % (at, bit)
    if k == 2:  # -0 for 0, or NaN for itself
        w = rng.choice([w for w in range(0x32) if w not in INT_WORDS])
        if struct.unpack_from('<f', o, 4 * w)[0] == 0.0 and rng.random() < 0.5:
            struct.pack_into('<I', o, 4 * w, 0x80000000)
            return o, 'minus zero'
        struct.pack_into('<I', o, 4 * w, 0x7fc00000)
        if rng.random() < 0.5:
            return o, 'nan one'
        struct.pack_into('<I', b, 4 * w, 0x7fc00000)  # both NaN
        return o, 'nan both'
    if k == 3:  # bytes past the compared words
        o[0xca] ^= 0xff
        return o, 'unread bytes'
    w = rng.randrange(0x32)
    if w in INT_WORDS:
        struct.pack_into('<i', o, 4 * w, struct.unpack_from('<i', o, 4 * w)[0] + 1)
    else:
        struct.pack_into('<f', o, 4 * w, struct.unpack_from('<f', o, 4 * w)[0] + 0.5)
    return o, 'word %#x' % w


def entry(rng):
    e = bytearray(0x20)
    for off in (0, 4, 8, 0xc, 0x10):
        struct.pack_into('<f', e, off, rng.choice([0.25, 0.1, rng.uniform(0, 100)]))
    struct.pack_into('<I', e, 0x14, rng.choice([0, 0x3000, 0x3100]))
    e[0x18] = rng.randrange(256)
    e[0x19] = rng.randrange(256)
    struct.pack_into('<I', e, 0x1c, rng.choice([0, 0x4000, 0x4100]))
    return e


class Exe:
    def __init__(self):
        self.o = Original()

    def ret(self):
        from unicorn.x86_const import UC_X86_REG_EAX
        return self.o.uc.reg_read(UC_X86_REG_EAX) & 0xff

    def numbers(self, a, b):
        pa, pb = self.o.alloc(0xcc), self.o.alloc(0xcc)
        self.o.write(pa, bytes(a))
        self.o.write(pb, bytes(b))
        self.o.call(SAME_NUMBERS, ecx=pa, stack_args=(pb,))
        return self.ret()

    def owner(self, entries):
        """The 0x0c-byte list owner: +4 the head node, +8 the count; nodes next, prev, the entry at +8."""
        if entries is None:
            return 0
        o = self.o
        own, head = o.alloc(0x0c), o.alloc(0x28)
        nodes = [o.alloc(0x28) for _ in entries]
        ring = [head] + nodes
        for i, n in enumerate(ring):
            o.u32(n, ring[(i + 1) % len(ring)])
            o.u32(n + 4, ring[i - 1])
        for n, e in zip(nodes, entries):
            o.write(n + 8, bytes(e))
        o.u32(own + 4, head)
        o.u32(own + 8, len(entries))
        return own

    def vlist(self, a, b):
        field = self.o.alloc(4)
        self.o.u32(field, self.owner(a))
        self.o.call(SAME_VLIST, ecx=field, stack_args=(self.owner(b),))
        return self.ret()

    def team_number(self, mask):
        from unicorn.x86_const import UC_X86_REG_EAX
        p = self.o.alloc(4)
        self.o.u32(p, mask)
        self.o.call(TEAM_NUMBER, ecx=p)
        return self.o.uc.reg_read(UC_X86_REG_EAX)

    def name(self, a, b):
        def text(s):
            if s is None:
                return 0
            p = self.o.alloc(len(s) + 1)
            self.o.write(p, s.encode() + b'\0')
            return p
        pa, pb = self.o.alloc(4), self.o.alloc(4)
        self.o.u32(pa, text(a))
        self.o.u32(pb, text(b))
        self.o.call(SAME_NAME, ecx=pa, stack_args=(pb,))
        return self.ret()


def main():
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    rng = random.Random(int(os.environ.get('SEED', '7')))
    exe = None
    bad = 0
    kinds = {}

    def check(what, e, p, detail):
        nonlocal bad
        kinds.setdefault(what, [0, 0])[e] += 1
        if e != p:
            bad += 1
            if bad <= 5:
                print('%s: exe %d, port %d (%s)' % (what, e, p, detail))

    for r in range(runs):
        if r % 500 == 0:
            exe = Exe()
        a = block(rng)
        b, what = mutate(rng, a)
        check('numbers ' + what.split()[0], exe.numbers(a, b), probe.probe_aiv_same_numbers(bytes(a), bytes(b)), what)
        n = rng.choice([None, 0, 1, 2, 3])
        ea = None if n is None else [entry(rng) for _ in range(n)]
        eb = None if ea is None else [bytearray(e) for e in ea]
        k = rng.randrange(5)
        if eb is not None and eb and k == 1:
            e = rng.choice(eb)
            at = rng.choice([0, 4, 8, 0xc, 0x10, 0x14, 0x18, 0x19, 0x1c])
            if at <= 0x10 and rng.random() < 0.3:
                struct.pack_into('<I', e, at, 0x7fc00000)  # a NaN in one entry's float
            else:
                e[at] ^= rng.choice([1, 0x40, 0x80])
        elif eb is not None and k == 2:
            eb.append(entry(rng))
        elif k == 3:
            eb = None if eb is not None else []
        na, nb = (-1 if ea is None else len(ea)), (-1 if eb is None else len(eb))
        p = probe.probe_aiv_same_vlist(b''.join(bytes(x) for x in ea or []), na, b''.join(bytes(x) for x in eb or []), nb)
        check('vlist', exe.vlist(ea, eb), p, 'k %d' % k)
        mask = 0
        for _ in range(rng.randrange(1, 8)):
            mask |= 1 << rng.randrange(32)
        e, p = exe.team_number(mask), probe.probe_team_number_of(mask)
        kinds.setdefault('team number', [0, 0])[1] += 1
        if e != p:
            bad += 1
            if bad <= 5:
                print('team number %#x: exe %#x, port %#x' % (mask, e, p))
        x, y = rng.choice([None, 'dom', 'alan']), rng.choice([None, 'dom', 'alan'])
        check('name', exe.name(x, y), probe.probe_aiv_same_name(x.encode() if x else None, y.encode() if y else None),
              '%r %r' % (x, y))
    seen = ', '.join('%s %d/%d' % (k, v[1], v[0]) for k, v in sorted(kinds.items()))
    print('%d runs, %d differ; the exe\'s answers, same/differs: %s' % (runs, bad, seen))
    return 1 if bad else 0


if __name__ == '__main__':
    rc = main()
    sys.stdout.flush()
    os._exit(rc)
