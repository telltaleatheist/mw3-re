"""The version-105 AI nets (src/mech3/zainet105, docs/ai.md 17.9) against the original: the reader 0x4025b0 (with
0x4021c0, 0x402300, 0x4022e0) on random blocks, then the nodes' link methods (0x403cd0, 0x403d10, 0x403d60, 0x403db0,
0x403df0, 0x403e20, 0x403e70) on them. The blocks: a name or none (the default then), a type in any case or none,
each setting present or not, pursuit_range either way round, up to a dozen node_NN entries with gaps, each with a
word, a place and up to three links (to nodes there, or -1). The measure 0x4023d0 (the basic controls') is a stub
on the exe's side and a hook on the port's: each call's from, to and width compared in order. Compared after the
read and after the operations: the net's fields, each node (number, word, place, links by number, records made) and
the elements (the nodes by number, the null node where a number is missing). The exe crashes on a link to a number
no node has and on remeasuring a slot without a record; neither is generated (the port's FIX (A)s keep them safe).

    cmake --build build --target mw3probe && .venv/bin/python tests/differential/test_net105.py [cases]

On Pirate's Moon's exe (0x402610 / 0x4025f0 cdecl, the link methods 0x403d30..0x403ef0): MW3_EXE=pm with pmprobe.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import Emu, Trace, load_probe  # noqa: E402
from ai_world import ser  # noqa: E402
from original import to_exe  # noqa: E402

NONE_PTR = 0x5e8654
HEAD, TAIL = 0x5f4c4c, 0x5f4c50
I32, U32 = ctypes.c_int32, ctypes.c_uint32


def raw_cstr(em, p):
    """A string at a run-time pointer, read as it is (on PM's exe the exe's own strings are PM's addresses)."""
    out = bytearray()
    while True:
        b = bytes(em.cpu._uc.mem_read(p + len(out), 64))
        i = b.find(b'\0')
        if i >= 0:
            return (out + b[:i]).decode('latin-1')
        out += b


def u(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


def put_node(e, a, v):
    if isinstance(v, list):
        items = e.world_alloc(8 * (len(v) + 1))
        e.w32(items, 1)
        e.w32(items + 4, len(v) + 1)
        for i, x in enumerate(v):
            put_node(e, items + 8 * (i + 1), x)
        e.w32(a, 4)
        e.w32(a + 4, items)
    elif isinstance(v, str):
        e.w32(a, 3)
        e.w32(a + 4, e.world_string(v))
    elif isinstance(v, float):
        e.w32(a, 2)
        e.w32(a + 4, u(v))
    else:
        e.w32(a, 1)
        e.w32(a + 4, v & 0xffffffff)


def block(r):
    """A random version-105 block (as a reader list) and its node numbers."""
    items = []

    def kv(k, v):
        items.extend([k, v])
    if r.random() < 0.7:
        kv('name', [r.choice(['north', 'Gate_02', 'x' * 19, 'patrol west', 'a'])])
    if r.random() < 0.8:
        kv('type', [r.choice(['ST', 'st', 'Hi', 'hill', 'FIre', 'fi', 'XX', 'S', 'stand'])])
    for key in ('path_width', 'activate_rad', 'attack_rad', 'attack_dwell', 'not_pursuit_dwell', 'return_range'):
        if r.random() < 0.75:
            kv(key, [r.choice([0.0, 2.5, 10.0, 75.0, r.uniform(0, 500)])])
    if r.random() < 0.75:
        a, b = r.uniform(0, 400), r.uniform(0, 400)
        kv('pursuit_range', [a, b])
    numbers = sorted(r.sample(range(0, 24), r.randint(0, 12)))
    for k in numbers:
        place = [r.uniform(-900, 900), r.uniform(-20, 80), r.uniform(-900, 900)]
        links = [r.choice(numbers + [-1, -1]) for _ in range(r.randint(0, 3))]
        kv(f'node_{k:02d}', [r.randint(0, 5), place, links])
    return items, numbers


class Exe:
    def __init__(self):
        self.t = Trace()
        e = self.e = Emu(self.t)
        e.heap_stubs()
        self.log = []

        def malloc(em, key, a, pop):
            em._ret(em.alloc(max(a[0], 4)), 0)
        e.import_stub(0x5bb664, 'malloc', 's0', 0, malloc)

        def strupr(em, key, a, pop):
            p = a[0]
            t = raw_cstr(em, p)
            em.cpu._uc.mem_write(p, t.upper().encode('latin-1'))
            em._ret(p, 0)

        def strncmp(em, key, a, pop):
            x, y = raw_cstr(em, a[0]).encode('latin-1')[:a[2]], raw_cstr(em, a[1]).encode('latin-1')[:a[2]]
            em._ret(0 if x == y else (-1 if x < y else 1), 0)

        def sprintf(em, key, a, pop):  # "node_%02d" only
            fmt = raw_cstr(em, a[1])
            text = fmt.replace('%02d', f'{a[2]:02d}').encode('latin-1') + b'\0'
            em.cpu._uc.mem_write(a[0], text)
            em._ret(len(text) - 1, 0)
        e.import_stub(0x5bb650, '_strupr', 's0', 0, strupr)
        e.import_stub(0x5bb654, 'strncmp', 's0,s1,s2', 0, strncmp)
        e.import_stub(0x5bb658, 'sprintf', 's0,s1,s2', 0, sprintf)
        self.t.silent |= {'_strupr', 'strncmp', 'sprintf'}
        self.t.silent |= {'malloc'}

        def fill(em, key, a, pop):
            rec, frm, to, width = a
            self.log.append(0x4023d0)
            self.log += [em.r32(frm + 4 * i) for i in range(3)]
            self.log += [em.r32(to + 4 * i) for i in range(3)]
            self.log.append(width)
            em._ret(0, pop)
        e.stub(0x4023d0, 'fill', 'c,d,s0,s1', None, fill)
        self.none = e.r32(NONE_PTR)
        e.call(0x4c7b80, ecx=to_exe(0x603fd8))  # znet's registry, as the AI tests make it
        e.call(0x4ae8b0)  # the null node's static constructor ([0x005e8654] -> 0x0060b160)

    def who(self, p, by_addr):
        if p == 0:
            return 0xffffffff
        if p == self.none:
            return 0xfffffffe
        return by_addr[p]

    def dump(self, net):
        e = self.e
        out = [0xd0d0, e.r32(net + 0x18)]
        out += [e.r32(net + 0x1c + i) for i in range(0, 20, 4)]
        out += [e.r32(net + o) for o in (0x30, 0x34, 0x38, 0x3c, 0x40, 0x44, 0x48, 0x4c, 0x50)]
        nodes, k = [], e.r32(net + 0x54)
        while k:
            nodes.append(k)
            k = e.r32(k + 0x30)
        by_addr = {a: e.r32(a + 0x2c) for a in nodes}
        for a in nodes:
            out += [0xaaaa, e.r32(a + 0x2c), e.r32(a + 0x28)] + [e.r32(a + 4 + 4 * i) for i in range(3)]
            for i in range(3):
                out += [self.who(e.r32(a + 0x10 + 4 * i), by_addr), 1 if e.r32(a + 0x1c + 4 * i) else 0]
        beg, end = e.r32(net + 0xc), e.r32(net + 0x10)
        n = (end - beg) // 4 if beg else 0
        out += [0xeeee, n] + [self.who(e.r32(beg + 4 * i), by_addr) for i in range(n)]
        return out, {e.r32(a + 0x2c): a for a in nodes}

    def run(self, items, x18, default, ops):
        e = self.e
        self.log = []
        e.w32(HEAD, 0), e.w32(TAIL, 0)
        tree = e.world_alloc(8)
        put_node(e, tree, items)
        name = e.world_string(default)
        net = e.call(0x4025b0, ecx=x18 & 0xffffffff, edx=tree, stack=(name,))
        first, by = self.dump(net)
        out = list(first) + self.log
        self.log = []
        kept = []
        for op, a, b, i in ops:
            if a not in by or b not in by:
                continue
            pa, pb = by[a], by[b]
            if op == 4 and not (e.r32(pb + 0x10 + 4 * i) == pa and e.r32(pb + 0x1c + 4 * i)):
                op = 2  # remeasuring needs `b`'s slot i to hold `a` with a record (else the exe writes through 0)
            kept.append((op, a, b, i))
            out += [0x0b0b, op]
            addr = {1: 0x403cd0, 2: 0x403db0, 3: 0x403df0, 4: 0x403e20, 5: 0x403d10, 6: 0x403d60}[op]
            stack = {1: (pb,), 2: (), 3: (pb,), 4: (pb, i), 5: (pb, i), 6: (pb, i)}[op]
            r = e.call(addr, ecx=pa, stack=stack)
            out += self.log
            self.log = []
            if op in (1, 4, 5):
                out.append(1 if r & 0xff else 0)
        last, _ = self.dump(net)
        return out + last, kept


def port(probe, items, x18, default, ops):
    data = ser(items)
    flat = [w for o in ops for w in o]
    buf = (U32 * 200000)()
    n = probe.probe_net105(data, len(data), x18, default.encode(), (I32 * max(1, len(flat)))(*flat), len(ops), buf,
                           len(buf))
    return list(buf[:n])


def main():
    cases = int(sys.argv[1]) if len(sys.argv) > 1 else 400
    probe = load_probe()
    probe.probe_net105.argtypes = [ctypes.c_char_p, I32, I32, ctypes.c_char_p, ctypes.POINTER(I32), I32,
                                   ctypes.POINTER(U32), I32]
    probe.probe_net105.restype = I32
    exe = Exe()
    r = random.Random(0x4025b0)
    bad, counts = 0, {'nodes': 0, 'fills': 0, 'ops': 0}
    for c in range(cases):
        items, numbers = block(r)
        x18 = r.choice([0, 7, -1, 0x1234])
        default = r.choice(['net_05', 'def'])
        ops = [(r.choice([1, 1, 2, 3, 4, 5, 6]), r.choice(numbers or [0]), r.choice(numbers or [0]), r.randint(0, 2))
               for _ in range(r.randint(0, 8))]
        want, kept = exe.run(items, x18, default, ops)
        got = port(probe, items, x18, default, kept)
        counts['nodes'] += len(numbers)
        counts['fills'] += want.count(0x4023d0)
        counts['ops'] += len(kept)
        if got != want:
            bad += 1
            if bad <= 3:
                k = next(i for i in range(min(len(got), len(want)) + 1) if i >= min(len(got), len(want)) or got[i] != want[i])
                print(f'case {c}: differ at word {k} of {len(want)} / {len(got)}')
                print('  exe ', [hex(x) for x in want[max(0, k - 4):k + 8]])
                print('  port', [hex(x) for x in got[max(0, k - 4):k + 8]])
    print(f'{cases} blocks, {bad} differ; {counts}')
    sys.stdout.flush()
    short = cases >= 400 and (counts['nodes'] < cases * 3 or counts['fills'] < cases * 5 or counts['ops'] < cases * 2)
    if short:
        print('too little reached')
    os._exit(1 if bad or short else 0)


if __name__ == '__main__':
    main()
