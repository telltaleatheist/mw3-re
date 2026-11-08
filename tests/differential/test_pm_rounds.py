"""Pirate's Moon's multiplayer rounds file, mprounds.zrd, and its reader 0x438380 / 0x438460 against read_round_rules
through probe_pm_round_rules (piece 3d of the multiplayer group, pm-tests.md). The block "round<N>", else the fallback
block named: its "table" (a list of 8, numbers as floats, ints converted, 0x578fc0) over the defaults [0x60554c]
(70 50 40 35 30 25 20 15), its "salvage" handed to the store's reader (0x473690, stood in for on both sides: what
reaches it is compared, flattened). PM's own find (0x5788b0, nested lists too) and float read run as the exe's; the
tree is the file's own nodes, laid out in the emulator as the reader holds them.

Cases: the shipped file (reader.zbd) for rounds 0..12 with "default" and a missing fallback, then random trees: blocks
present or not, tables of 7 to 10 entries, ints and floats, salvage lists, keys nested deeper.

    cmake --build build --target pmprobe && MW3_EXE=pm .venv/bin/python tests/differential/test_pm_rounds.py
"""
import ctypes
import os
import random
import struct
import sys
import tempfile
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
sys.path.insert(0, str(Path(__file__).parents[2] / 'tools' / 'analysis'))
os.environ['MW3_EXE'] = 'pm'
import test_pm_zar  # noqa: E402
import zrd  # noqa: E402
from original import FTOL  # noqa: E402

I32 = ctypes.c_int32
READ, STORE_NEW, SALVAGE, OPEN, FREE = 0x438380, 0x473170, 0x473690, 0x578630, 0x5787a0
CS_NEW, CS_FORMAT, CS_FREE = 0x5ce8e4, 0x5ce8de, 0x5ce8d8
ROOT = Path(__file__).parents[2]


def s32(v):
    return struct.unpack('<i', struct.pack('<I', v & 0xffffffff))[0]


def encode(v):
    """A tree (lists, str, int, float) as the archive's binary nodes."""
    if isinstance(v, list):
        return struct.pack('<iI', 4, len(v) + 1) + b''.join(encode(x) for x in v)
    if isinstance(v, str):
        b = v.encode('latin-1')
        return struct.pack('<iI', 3, len(b)) + b
    if isinstance(v, float):
        return struct.pack('<if', 2, v)
    return struct.pack('<ii', 1, v)


class Exe:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        stubs = {STORE_NEW: (lambda c: c.ecx, 4), SALVAGE: (self.salvage, 4), OPEN: (lambda c: self.root, 0),
                 FREE: (lambda c: 0, 0), CS_NEW: (self.cs_new, 0), CS_FORMAT: (self.cs_format, 0),
                 CS_FREE: (lambda c: 0, 0)}
        for a, (fn, pop) in stubs.items():
            z.native_stub(a, fn, pop)

    def w(self, a, *v):
        self.uc.mem_write(a, struct.pack(f'<{len(v)}I', *[x & 0xffffffff for x in v]))

    def r(self, a):
        return struct.unpack('<I', bytes(self.uc.mem_read(a, 4)))[0]

    def place(self, v, at):
        """Writes node `v` at `at` (8 bytes), its list's or string's storage allocated."""
        if isinstance(v, list):
            arr = self.z.alloc(8 * (len(v) + 1))
            self.w(arr, 1, len(v) + 1)
            for k, x in enumerate(v):
                self.place(x, arr + 8 * (k + 1))
            self.w(at, 4, arr)
        elif isinstance(v, str):
            self.w(at, 3, self.z.string(v))
        elif isinstance(v, float):
            self.uc.mem_write(at, struct.pack('<if', 2, v))
        else:
            self.w(at, 1, v)

    def cs_new(self, c):
        self.w(c.ecx, self.z.string(''))
        return c.ecx

    def cs_format(self, c):
        fmt = self.z.cstr(c.arg(1))
        assert fmt == b'round%d', fmt
        self.w(c.arg(0), self.z.string('round%d' % s32(c.arg(2))))
        return 0

    def flatten(self, at):
        t, val = struct.unpack('<iI', bytes(self.uc.mem_read(at, 8)))
        if t == 4:
            n = self.r(val + 4)
            for k in range(1, n):
                self.flatten(val + 8 * k)
        elif t == 3:
            self.out += list(self.z.cstr(val)) + [0]
        else:
            self.out.append(s32(val))

    def salvage(self, c):
        assert c.ecx == self.obj + 0x20, hex(c.ecx)
        self.flatten(c.arg(0))
        self.out.append(-1)
        return 1

    def run(self, tree, fallback, rnd):
        self.root = self.z.alloc(8)
        self.place(tree, self.root)
        self.obj = self.z.alloc(0x40)
        self.out = []
        self.z.call(READ, ecx=self.obj, args=(self.z.string(fallback), rnd))
        return [s32(x) for x in struct.unpack('<8I', bytes(self.uc.mem_read(self.obj, 32)))] + self.out


def shipped():
    data = (ROOT / 'game' / 'pm' / 'zbd' / 'reader.zbd').read_bytes()
    for name, start, length in zrd.entries(data):
        if name == 'mprounds.zrd':
            return data[start:start + length]
    raise SystemExit('mprounds.zrd not in reader.zbd')


def random_tree(rng):
    def table():
        n = rng.choice([7, 8, 8, 8, 9, 10])
        return ['table', [rng.choice([float(rng.randrange(0, 90)), rng.randrange(0, 90), -3.5]) for _ in range(n)]]

    def salvage():
        return ['salvage', ['mechs', [[rng.choice(['clint', 'puma', 'owens']), 'MSG_CONFIG_PRIMARY']],
                            'equipment', [['AMS', rng.choice(['clan', 'sphere'])]] * rng.randrange(1, 3)]]

    def block():
        b = []
        if rng.random() < 0.8:
            b += table()
        if rng.random() < 0.5:
            b += salvage()
        if rng.random() < 0.2:
            b = ['wrap', b]  # the keys a level down: the find goes into lists
        return b

    tree = []
    for name in ['default'] + [f'round{k}' for k in range(1, 6)] + ['other']:
        if rng.random() < 0.6:
            tree += [name, block()]
    return tree


def main():
    probe = test_pm_zar.load_pm_probe()
    probe.probe_pm_round_rules.argtypes = [ctypes.c_char_p, ctypes.c_char_p, I32, ctypes.POINTER(I32), I32]
    exe = Exe()
    tmp = tempfile.mkdtemp(prefix='pm_rounds_')
    path = os.path.join(tmp, 'mprounds.bin')

    def check(raw, fallback, rnd, label):
        tree, _ = zrd.node(raw, 0)
        theirs = exe.run(tree, fallback, rnd)
        with open(path, 'wb') as f:
            f.write(raw)
        out = (I32 * 65536)()
        n = probe.probe_pm_round_rules(path.encode(), fallback.encode(), rnd, out, 65536)
        mine = list(out[:n])
        if mine != theirs:
            j = next((k for k, (a, b) in enumerate(zip(mine, theirs)) if a != b), min(len(mine), len(theirs)))
            print(f'{label} (fallback {fallback!r}, round {rnd}) at {j} of exe {len(theirs)}, port {len(mine)}:\n'
                  f'  exe  {theirs[max(0, j - 6):j + 12]}\n  port {mine[max(0, j - 6):j + 12]}', flush=True)
            os._exit(1)
        return theirs

    raw = shipped()
    tables = set()
    for rnd in range(0, 13):
        for fallback in ('default', 'nothing'):
            t = check(raw, fallback, rnd, 'shipped')
            tables.add(tuple(t[:8]))
    print(f'shipped mprounds.zrd: rounds 0..12 identical, {len(tables)} distinct tables', flush=True)
    rng = random.Random(79)
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 1500
    for i in range(count):
        tree = random_tree(rng)
        check(encode(tree), rng.choice(['default', 'other', 'none']), rng.randrange(0, 7), f'random case {i}')
    print(f'{count} random files identical', flush=True)
    next_round(probe, rng)
    os._exit(0)


class ExeNext:
    """0x438580 on the network game, the rules' read (0x438380) and the salvage's add (0x475270, appending the
    salvage's mechs as the store's add does for new ones) stood in for; the reward 0x4384f0, the filter 0x438790 (the
    items' vt+0xc, the masses 0x486830 from the exe's table) and the list's removal run as the exe's."""

    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        stubs = {READ: (self.read, 8), 0x475270: (self.add, 4), 0x4731b0: (lambda c: c.ecx, 0),
                 0x5ce8c0: (lambda c: 0, 0)}
        for a, (fn, pop) in stubs.items():
            z.native_stub(a, fn, pop)
        self.uc.mem_write(0x5de644, struct.pack('<I', z.cpu.code(FTOL)))  # MSVCRT's _ftol, 0x5cef9a's import
        self.kind_fn = z.cpu.code(b'\xc3')
        z.native_stub(self.kind_fn, lambda c: self.kinds[c.ecx], 0)
        self.vt = z.alloc(0x40)
        self.uc.mem_write(self.vt + 0xc, struct.pack('<I', self.kind_fn))

    def w(self, a, v):
        self.uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def r(self, a):
        return struct.unpack('<I', bytes(self.uc.mem_read(a, 4)))[0]

    def item(self, kind, ident):
        a = self.z.alloc(0x20)
        self.w(a, self.vt), self.w(a + 0x14, ident)
        self.kinds[a] = kind
        return a

    def append(self, item):
        head = self.r(self.game + 0xdc)
        node, last = self.z.alloc(0xc), self.r(head + 4)
        self.w(node + 8, item), self.w(node, head), self.w(node + 4, last), self.w(last, node), self.w(head + 4, node)
        self.w(self.game + 0xe0, self.r(self.game + 0xe0) + 1)

    def read(self, c):
        self.log += [5, s32(c.arg(1))]
        assert self.z.cstr(c.arg(0)) == b'default'
        self.uc.mem_write(c.ecx, struct.pack('<8i', *self.v[7:15]))
        self.w(c.ecx + 0x2c, self.v[15])  # the salvage store's count
        if self.v[15]:
            self.log += [6, self.v[15]]
        return c.ecx

    def add(self, c):
        for k in range(self.v[15]):
            self.append(self.item(0, self.v[16] + k))
        return c.ecx

    def run(self, v):
        z, w = self.z, self.w
        self.v, self.log, self.kinds = v, [], {}
        g = self.game = z.alloc(0x100)
        w(g + 0x50, v[2]), w(g + 0x70, v[3]), w(g + 0x74, v[4]), w(g + 0x80, v[5]), w(g + 0xd0, v[6])
        head = z.alloc(0xc)
        w(head, head), w(head + 4, head), w(g + 0xdc, head), w(g + 0xe0, 0)
        for k in range(v[17]):
            self.append(self.item(v[18 + 2 * k], v[19 + 2 * k]))
        z.call(0x438580, ecx=g, args=(v[0], v[1]))
        out = self.log + [90, s32(self.r(g + 0xd0)), s32(self.r(g + 0x80))]
        node = self.r(head)
        while node != head:
            it = self.r(node + 8)
            out += [self.kinds[it], s32(self.r(it + 0x14))]
            node = self.r(node)
        assert self.r(g + 0xe0) == (len(out) - len(self.log) - 3) // 2
        return out


def next_round(probe, rng):
    probe.probe_pm_next_round.argtypes = [ctypes.POINTER(I32), ctypes.POINTER(I32), I32]
    exe = ExeNext()
    seen = {}
    for i in range(3000):
        v = [0] * 38
        v[0], v[1], v[2] = rng.randrange(0, 9), rng.randrange(0, 8), rng.randrange(0, 9)
        v[3], v[4] = rng.choice([-20, -5, -1, 0, 1, 2, 5, 10, 20]), rng.randrange(0, 8)
        v[5] = rng.choice([10, 13, 35, 50, 90, 97, 100])
        v[6] = struct.unpack('<i', struct.pack('<f', rng.uniform(-50, 300)))[0]
        table = [rng.choice([0.0, 2.5, 15.0, 50.0, rng.uniform(0, 80)]) for _ in range(8)]
        v[7:15] = [struct.unpack('<i', struct.pack('<f', t))[0] for t in table]
        v[15], v[16] = rng.randrange(0, 5), rng.randrange(13, 21)
        v[17] = rng.randrange(0, 11)
        for k in range(v[17]):
            v[18 + 2 * k:20 + 2 * k] = [rng.choice([0, 0, 0, 1, 2]), rng.randrange(0, 13)]
        theirs = exe.run(v)
        out = (I32 * 256)()
        n = probe.probe_pm_next_round((I32 * 38)(*v), out, 256)
        mine = list(out[:n])
        if mine != theirs:
            print(f'next round case {i} (place {v[0]}, round {v[1]}, players {v[2]}, realloc {v[3]}, rounds {v[4]}, '
                  f'tons {v[5]}):\n  exe  {theirs}\n  port {mine}', flush=True)
            os._exit(1)
        key = (v[1] == v[4], theirs.index(90) > 0)
        seen[key] = seen.get(key, 0) + 1
    print(f'between rounds: 3000 identical; (the last round, the rules read): {dict(sorted(seen.items()))}', flush=True)


if __name__ == '__main__':
    main()
