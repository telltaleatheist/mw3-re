"""zWeapon's damage path (docs/weapons.md, "Hits") against the original x86:
the beam damage with its falloff 0x583020, the hand-off to the hit node's
handler (for a mech, Mech::TakeDamage) 0x583480, the impact 0x5826b0 with
the def callbacks it runs (salvo marks, LB-X burst, airstrike), the splash
query and damage 0x582920 / 0x5829b0, the projectile hit with RANGE_MINIMUM
0x586350, and the air burst 0x582320.

    cmake --build build && .venv/bin/python tests/differential/test_weapons_hit.py

The definitions are the real weapons.zrd (test_weapons_zrd.py). The scene is
fake on both sides and the same: nodes with their damage handlers (none, a
shootable projectile, or a handler that kills, reports health, returns a
value), materials with soil kinds, the splash query's answer, craters,
sounds, effects. Every call out is logged on both sides; after each operation
the hit globals, the projectile, the records and the salvo marks must match
exactly at the game's 24-bit x87 precision.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

os.environ.setdefault('MW3_X87', '24')
sys.path.insert(0, str(Path(__file__).parent))
import test_weapons_zrd as zr  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

probe = zr.probe
for name, args in (('probe_hit_world', [ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p,
                                        ctypes.c_int, ctypes.c_int32, ctypes.c_int32, ctypes.c_void_p]),
                   ('probe_hit_state', [ctypes.c_void_p]),
                   ('probe_hit_op', [ctypes.c_int, ctypes.c_int32, ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p,
                                     ctypes.c_void_p, ctypes.c_void_p, ctypes.c_uint32]),
                   ('probe_hit_clock', [ctypes.c_float]), ('probe_hit_poke', [ctypes.c_int, ctypes.c_int, ctypes.c_int32])):
    getattr(probe, name).argtypes = args
probe.probe_hit_op.restype = ctypes.c_int32

IAT_RAND = 0x5bb668
OPS = {0: 0x583020, 1: 0x583480, 2: 0x5826b0, 3: 0x586350, 4: 0x582320, 5: 0x5829b0}
NAMES = {1: 'surface_mark', 2: 'handler', 3: 'effect', 4: 'crater', 5: 'quicksand', 6: 'sound', 7: 'impact_effect',
         8: 'surface_angles', 9: 'set_intersect', 10: 'query', 11: 'spawn', 12: 'pre_hit', 20: 'beam_flags'}
s32, f2i, i2f = zr.s32, None, None


def f2i(x):  # noqa: F811
    return struct.unpack('<I', struct.pack('<f', x))[0]


def i2f(i):  # noqa: F811
    return struct.unpack('<f', struct.pack('<I', i & 0xffffffff))[0]


def f32(x):
    return i2f(f2i(x))


class MsvcRand:
    def __init__(self, seed):
        self.state = seed

    def __call__(self, cpu=None):
        self.state = (self.state * 214013 + 2531011) & 0xffffffff
        return (self.state >> 16) & 0x7fff


class Emulated(zr.Emulated):
    def __init__(self):
        super().__init__()
        cpu = self.cpu
        real = zr.real_bytes()
        assert self.run(real, [0] * 10, 1, 1) == 0
        self.defs_base, self.ndefs = self.u32(0x9a0eac), self.u32(0x9a0ea8)
        arch = zr.WORK / 'hit.zbd'
        zr.WORK.mkdir(parents=True, exist_ok=True)
        zr.write_archive(arch, real)
        buf, n = (ctypes.c_int32 * 400000)(), ctypes.c_int()
        assert probe.probe_zwep_run(str(arch).encode(), (ctypes.c_uint32 * 10)(*[0] * 10), 1, 1, buf, 400000,
                                    ctypes.byref(n)) == 0
        self.names = {self.cstr(self.u32(self.defs_base + 0x1f4 * k)).decode(): k for k in range(self.ndefs)}
        a = self.zalloc
        self.P = a(0x600)
        self.recs = [a(0x40) for _ in range(4)]
        self.list = a(0x500)
        self.nodes = [a(0x100) for _ in range(8)]
        self.handlers = [a(0x10) for _ in range(8)]
        self.hprojs = [a(0x600) for _ in range(8)]
        self.mats = [a(0x40) for _ in range(6)]
        self.events = []
        self.rand = MsvcRand(1)
        cpu.stub_import(IAT_RAND, lambda c: self.rand())
        cpu.stub_import(0x5bb4c8, lambda c: 0)  # operator delete
        cpu.stub_import(0x5bb4bc, lambda c: self.zalloc(max(c.arg(0), 1)))  # operator new
        fn = cpu.code(b'\xc2\x08\x00')
        cpu.stub_function(fn, self.handler, 8)
        self.handler_fn = fn
        pre = cpu.code(b'\xc2\x08\x00')
        cpu.stub_function(pre, lambda c: self.log(12, c.ecx, c.edx, c.arg(0), self.def_index(c.arg(1))), 8)
        self.pre_fn = pre
        stubs = {
            0x542060: (lambda c: self.log(1, self.token(c.ecx)), 0),
            0x508aa0: (lambda c: self.check(c.edx == 0) or self.log(3, c.ecx, *[c.arg(i) for i in range(9)]), 0x24),
            0x5825b0: (lambda c: self.log(4, self.def_index(c.ecx), self.token(c.edx), s32(c.arg(0))) or
                       self.crater, 8),
            0x5824b0: (lambda c: self.log(5, self.def_index(c.ecx), self.token(c.edx), s32(c.arg(0))), 8),
            0x582f60: (lambda c: self.log(6, self.def_index(c.ecx), self.token(c.edx), s32(c.arg(0)), c.arg(1)), 8),
            0x50d650: (lambda c: self.log(7, s32(c.ecx), *self.vec(c.edx)), 0),
            0x525a50: (lambda c: self.log(8, *self.vec(c.edx)), 4),
            0x525f80: (lambda c: self.cpu.write(c.edx, struct.pack('<3f', *self.angles)), 0),
            0x4eb6d0: (lambda c: self.log(9, c.ecx, c.edx), 0),
            0x4f0ac0: (self.query, 0x14),
            0x585010: (self.spawn, 0x18),
        }
        for addr, (f, pop) in stubs.items():
            if addr in cpu.stubs:  # the reader's stub there: replace it, a second hook would run twice
                cpu.stubs[addr] = (f, pop)
            else:
                cpu.stub_function(addr, f, pop)
        # data no shipped weapon has, the same on both sides
        def poke(name, which, v):
            k = self.names[name]
            dp = self.defs_base + 0x1f4 * k
            if which == 0:
                cpu.u32(dp + 0x6c, v)
            elif which in (1, 2):
                cpu.u32(dp + 0x20 + 4 * which, v)
            else:
                imp = self.u32(dp + 0x13c)
                for s_ in range(6):
                    cpu.u32(imp + 0x5c * s_ + 0x1c, v)
            probe.probe_hit_poke(k, which, v)
        flags = lambda n: self.u32(self.defs_base + 0x1f4 * self.names[n] + 0x6c)  # noqa: E731
        poke('NUKE', 0, (flags('NUKE') & ~0x10) | 0x40000)       # quicksand
        poke('GASTANK', 0, (flags('GASTANK') & ~0x10) | 0x40000)
        poke('LRM_BG', 0, flags('LRM_BG') | 0x4000)              # a mine's full splash
        poke('SRM2', 0, flags('SRM2') | 0x80000000)              # RANGE_MINIMUM (90 0): a dud inside
        poke('SRM2', 1, f2i(90.0))
        poke('SRM2', 2, 0)
        poke('AC20', 3, 0x77)                                    # an impact EFFECT
        self.reached = set()
        self.cover = {
            0x58304a: 'beam falloff', 0x58307f: 'falloff floor', 0x583090: 'timed beam', 0x583032: 'beam dealt',
            0x5834c4: 'shot down a projectile', 0x583517: 'shot line', 0x583570: 'pre-hit hook',
            0x58358d: 'kill animation', 0x5835c4: 'override animation', 0x583614: 'damage animation',
            0x583646: 'beam ground animation', 0x58368c: 'hit counted', 0x5834a7: 'surface mark',
            0x582718: 'crater', 0x582746: 'quicksand', 0x582796: 'crater at the kill', 0x5827c7: 'quicksand at kill',
            0x582800: 'impact sound', 0x58284e: 'impact animation', 0x58288e: 'surface animation',
            0x582904: 'impact effect', 0x5826e1: 'def callback', 0x582937: 'intersect off',
            0x5829f8: 'mine full splash', 0x5829fd: 'splash falloff', 0x5863d3: 'min-range dud',  # noqa: E501
            0x586402: 'min-range scale', 0x586426: 'destroy anim at min range', 0x58647b: 'hit splash',
            0x582388: 'air burst splash', 0x582add: 'splash exclude',
        }

        def hit(uc, address, size, user):
            self.reached.add(address)
        for addr in self.cover:
            cpu.uc.hook_add(UC_HOOK_CODE, hit, begin=addr, end=addr)

    def zalloc(self, n):
        p = self.cpu.alloc(n)
        self.cpu.write(p, bytes(n))
        return p

    def def_index(self, d):
        return s32(self.u32(d + 0x10)) if d else -1

    def vec(self, p):
        return list(struct.unpack('<3I', self.cpu.read(p, 12)))

    def log(self, *e):
        self.events.append(tuple(s32(v) for v in e))
        return 0

    def token(self, p):
        if p == 0:
            return -1
        if p in self.recs:
            return 0x100 + self.recs.index(p)
        for base, tag in ((self.out, 0x200), (self.list, 0x300)):
            if base and base + 4 <= p < base + 4 + 0x28 * 31 and (p - base - 4) % 0x28 == 0:
                return tag + (p - base - 4) // 0x28
        return -2

    def handler(self, c):
        hd, d, rec, dmg = c.ecx, c.edx, c.arg(0), c.arg(1)
        self.log(2, hd, self.def_index(d), self.token(rec), dmg)
        row = next(r for r in self.node_rows if r[1] == hd)
        if row[6]:
            if self.u32(rec + 0x24):
                self.cpu.u32(0x9a1790, rec)
            self.cpu.u32(0x9a1788, row[6])
        self.cpu.u32(0x9a178c, row[7])
        return row[5]

    def query(self, c):
        pos, zero, radius, one, one2, out = c.edx, c.arg(0), c.arg(1), c.arg(2), c.arg(3), c.arg(4)
        self.check(zero == 0 and one == 1 and one2 == 1)
        self.log(10, *self.vec(pos), radius)
        self.out = out
        self.cpu.write(out, bytes(4))
        self.cpu.write(out, bytes([len(self.qrecs)]))
        for i, r in enumerate(self.qrecs):
            self.cpu.write(out + 4 + 0x28 * i, struct.pack('<10I', *[x & 0xffffffff for x in r]))
        return self.qresult

    def spawn(self, c):
        d, node = c.ecx, c.edx
        x64, mz, dr, vel, p5, p6 = (c.arg(i) for i in range(6))
        self.log(11, self.def_index(d), node, x64, *self.vec(mz), *self.vec(dr), *self.vec(vel), s32(p5), s32(p6))
        return 0


def rec_values(rng, emu, nodes, mats, mine=False):
    """A hit record: normal, point, x18, distance squared, material, node."""
    n = [f32(rng.uniform(-1, 1)) for _ in range(3)]
    p = [f32(rng.uniform(-300, 300)) for _ in range(3)]
    d2 = f32(rng.choice([0.0, rng.uniform(0, 100), rng.uniform(0, 1000)]))
    mat = rng.choice(mats + [0])
    node = rng.choice(nodes)  # a record without a node crashes both (0x5834ae)
    return [f2i(x) for x in n] + [f2i(x) for x in p] + [0, f2i(d2), mat, node]


def main():
    emu = Emulated()
    cpu = emu.cpu
    rng = random.Random(406)
    runs = int(os.environ.get('WEAPON_HITS', '6000'))
    calls = 0
    defs = list(range(emu.ndefs))
    clock = 10.0
    for k in range(runs):
        # the scene
        node_rows = []
        for i, nd in enumerate(emu.nodes):
            kind = rng.choice(['handler', 'handler', 'none', 'proj'])
            hd = {'none': 0, 'proj': 1, 'handler': emu.handlers[i]}[kind]
            node_rows.append([nd, hd, f2i(f32(rng.choice([-1.0, 0.5, 2.0, 10.0]))), int(rng.random() < 0.4),
                              int(rng.random() < 0.9), rng.choice([0, 0, 1, 2]),
                              rng.choice([0, 0, 0, 0x6600 + i]), f2i(f32(rng.choice([1.0, 0.6, 0.3, 0.1, 0.0])))])
        emu.node_rows = node_rows
        for i, (nd, hd, ph, inter, live, res, kill, health) in enumerate(node_rows):
            cpu.write(nd, bytes(0x100))
            cpu.u32(nd + 0x24, 0x10 if inter else 0)
            cpu.u32(nd + 0xbc, hd)
            cpu.u32(nd + 0x40, emu.hprojs[i])
            cpu.u32(emu.hprojs[i] + 0x5b8, ph)
            hm = emu.handlers[i]
            cpu.u32(hm, hm)
            cpu.u32(hm + 4, emu.handler_fn if live else 0)
        mats = []
        for i, m in enumerate(emu.mats):
            soil = rng.choice([0, 0, 1, 2, 3, 4, 5])
            cpu.u32(m + 0x20, soil)
            mats.append((m, soil))
        mat_addrs = [m for m, _ in mats]
        nodes = [r[0] for r in node_rows]
        emu.qrecs = [rec_values(rng, emu, nodes, mat_addrs) for _ in range(rng.randrange(0, 6))]
        emu.qresult = rng.choice([0, 0, 1])
        emu.crater = rng.choice([0, 1])
        emu.angles = [f32(rng.uniform(-3, 3)) for _ in range(3)]
        emu.out = 0
        flat_nodes = [x for r in node_rows for x in r]
        flat_mats = [x for m in mats for x in m]
        flat_q = [x for r in emu.qrecs for x in r]
        probe.probe_hit_world((ctypes.c_uint32 * len(flat_nodes))(*flat_nodes), len(node_rows),
                              (ctypes.c_int32 * len(flat_mats))(*[s32(x) for x in flat_mats]), len(mats),
                              (ctypes.c_int32 * max(1, len(flat_q)))(*[s32(x) for x in flat_q]), len(emu.qrecs),
                              emu.qresult, emu.crater, (ctypes.c_float * 3)(*emu.angles))
        # the globals
        st = [int(rng.random() < 0.9), int(rng.random() < 0.8), int(rng.random() < 0.7), int(rng.random() < 0.2),
              rng.choice([0, 0x6300, 0x6301]), f2i(f32(rng.choice([1 / 30, 0.1]))), rng.randrange(5),
              f2i(f32(rng.choice([1.0, 0.5])))]
        probe.probe_hit_state((ctypes.c_int32 * 8)(*[s32(x) for x in st]))
        cpu.u32(0x9a0eec, st[0])
        cpu.u32(0x9a0ee8, st[1])
        cpu.u32(0x9a0ef0, st[2])
        cpu.u32(0x9a17a4, emu.pre_fn if st[3] else 0)
        cpu.u32(0x9a17c4, st[4])
        cpu.u32(0x72c52c, st[5])
        cpu.u32(0x9a17c0, st[6])
        cpu.u32(0x9a178c, st[7])
        for addr in (0x9a1788, 0x9a1790):
            cpu.u32(addr, 0)
        clock += rng.choice([0.0, 0.1, 0.3, 0.6])
        cpu.write(0x72c530, struct.pack('<f', clock))
        probe.probe_hit_clock(clock)
        # the operation
        op = rng.choice([0, 1, 2, 2, 3, 3, 3, 4, 5])
        d = rng.choice(defs)
        if op == 5:  # splash runs only with IMPACT_PROXIMITY (0/0 is a NaN whose sign differs by CPU)
            d = rng.choice([k for k in defs if emu.u32(emu.defs_base + 0x1f4 * k + 0x40)])
        if op == 4 and emu.cstr(emu.u32(emu.defs_base + 0x1f4 * d)) == b'LANDMINE':
            op = 3  # its surface animation reads the air burst's unset normal (FIX (bug), docs/fixes.md)
        recs = [rec_values(rng, emu, nodes, mat_addrs) for _ in range(rng.randrange(1, 5))]
        for i, r in enumerate(recs):
            cpu.write(emu.recs[i], struct.pack('<10I', *[x & 0xffffffff for x in r]))
        dv = [rng.uniform(-1, 1) for _ in range(3)]
        n = sum(x * x for x in dv) ** 0.5 or 1
        proj = [rng.choice([0x6300, 0x6301]), rng.choice(nodes), 1, *[f2i(f32(x / n)) for x in dv],
                *[f2i(f32(rng.uniform(-300, 300))) for _ in range(3)], f2i(f32(rng.choice([400.0, 900.0, 0.0]))),
                f2i(f32(rng.choice([0.0, 30.0, 89.0, 95.0, 500.0]))), f2i(f32(rng.choice([-1.0, 5.0]))),
                f2i(f32(rng.choice([1.0, 0.25, 0.5])))]
        P = emu.P
        cpu.write(P, bytes(0x600))
        cpu.u32(P + 4, proj[0])
        cpu.u32(P + 0xc, proj[1])
        cpu.u32(P + 0x20, proj[2])
        cpu.write(P + 0x3c, struct.pack('<6I', *proj[3:9]))
        cpu.u32(P + 0x58c, proj[9])
        cpu.u32(P + 0x5b0, proj[10])
        cpu.u32(P + 0x5b8, proj[11])
        cpu.u32(P + 0x5bc, proj[12])
        extra = [rng.choice([0, 0, 0x6700]), rng.choice([0] + [r[1] for r in node_rows]), rng.choice([0x6300, 0x6301]),
                 f2i(f32(rng.uniform(0, 30))), *[f2i(f32(rng.uniform(-300, 300))) for _ in range(3)]]
        beam = [f2i(f32(rng.uniform(0, 1200))), f2i(f32(rng.choice([1.0, 0.5]))), f2i(f32(rng.choice([1.0, 0.5]))),
                rng.choice([0, 1, 2, 3])]
        seed = rng.randrange(1 << 31)
        emu.rand = MsvcRand(seed)
        emu.events = []
        defp = emu.defs_base + 0x1f4 * d
        if op == 0:
            bm = emu.zalloc(0x150)
            cpu.write(bm + 0x28, struct.pack('<I', beam[0]))
            cpu.write(bm + 0x34, struct.pack('<II', beam[1], beam[2]))
            cpu.u32(bm + 0x14c, beam[3])
            cpu.call(OPS[0], ecx=defp, edx=bm)
            cpu.call(emu.fstp)
            res = emu.u32(emu.fslot)
            emu.log(20, emu.u32(bm + 0x14c))
        elif op == 1:
            frm = emu.zalloc(12)
            cpu.write(frm, struct.pack('<3I', *extra[4:7]))
            cpu.call(OPS[1], ecx=defp, edx=extra[2], stack_args=(frm, emu.recs[0], extra[3], extra[0]))
            res = cpu.uc.reg_read(UC_X86_REG_EAX)
        elif op == 2:
            cpu.call(OPS[2], ecx=defp, edx=emu.recs[0], stack_args=(P, extra[0]))
            res = cpu.uc.reg_read(UC_X86_REG_EAX)
        elif op == 3:
            cpu.call(OPS[3], ecx=defp, edx=P, stack_args=(emu.recs[0],))
            res = 0
        elif op == 4:
            cpu.call(OPS[4], ecx=defp, edx=P)
            res = 0
        else:
            cpu.write(emu.list, bytes(4))
            cpu.write(emu.list, bytes([len(recs)]))
            for i, r in enumerate(recs):
                cpu.write(emu.list + 4 + 0x28 * i, struct.pack('<10I', *[x & 0xffffffff for x in r]))
            cpu.call(OPS[5], ecx=defp, edx=P, stack_args=(emu.list, extra[1]))
            res = cpu.uc.reg_read(UC_X86_REG_EAX)
        a_ev = emu.events
        flat_r = [x for r in recs for x in r]
        b_res = probe.probe_hit_op(op, d, (ctypes.c_int32 * len(flat_r))(*[s32(x) for x in flat_r]), len(recs),
                                   (ctypes.c_int32 * 13)(*[s32(x) for x in proj]),
                                   (ctypes.c_int32 * 7)(*[s32(x) for x in extra]),
                                   (ctypes.c_int32 * 4)(*[s32(x) for x in beam]), seed)
        buf = (ctypes.c_int32 * 50000)()
        nlog = probe.probe_hit_log(buf, 50000)
        b_ev = zr.port_events(buf, nlog)
        if op == 2:
            res, b_res = 0, 0  # impact returns whatever EAX holds (no return value)
        # state
        u = emu.u32
        sa = [u(0x9a0ec4 + 4 * i) for i in range(6)] + [u(0x9a0edc), u(0x9a0eec), u(0x9a1788),
                                                       emu.token(u(0x9a1790)), u(0x9a178c), u(0x9a17c0)]
        sa += [u(P + 0x20), *[u(P + 0x48 + 4 * i) for i in range(3)], u(P + 0x5b8), u(P + 0x5bc)]
        for i in range(len(recs)):
            sa += [u(emu.recs[i] + 4 * j) for j in range(6)]
        sa += [u(p + 0x5b8) for p in emu.hprojs]
        for kd in range(emu.ndefs):
            t = u(emu.defs_base + 0x1f4 * kd + 0x1f0)
            if not t:
                continue
            head = u(t + 0x84)
            marks, nd = [], u(head)
            while nd != head:
                r = u(nd + 8)
                marks += [u(r), u(r + 4), u(r + 8), u(r + 0xc)]
                nd = u(nd)
            sa += [u(t + 0x88)] + marks
        sa.append(int(u(0x9a0ee0) != 0))
        sa = [s32(x) for x in sa]
        dbuf = (ctypes.c_int32 * 20000)()
        nd = probe.probe_hit_dump(dbuf, 20000)
        sb = list(dbuf[:nd])
        if a_ev != b_ev or s32(res) != s32(b_res) or sa != sb:
            first = next((i for i, (x, y) in enumerate(zip(a_ev, b_ev)) if x != y), min(len(a_ev), len(b_ev)))
            diff = next((i for i, (x, y) in enumerate(zip(sa, sb)) if x != y), None)
            def show(ev):
                return '\n    '.join(f'{NAMES.get(e[0], e[0])}{tuple(hex(x & 0xffffffff) for x in e[1:])}' for e in ev)
            raise SystemExit(f'run {k} op {op} def {d}: result {s32(res):#x} vs {s32(b_res):#x}; state item {diff}'
                             f' {sa[diff] if diff is not None else ""} vs {sb[diff] if diff is not None else ""}'
                             f'\n  calls differ at {first} ({len(a_ev)} vs {len(b_ev)}):\n  original:\n    '
                             f'{show(a_ev[first:first + 5])}\n  port:\n    {show(b_ev[first:first + 5])}')
        calls += len(a_ev)
    missing = sorted(emu.cover[a] for a in set(emu.cover) - emu.reached)
    if missing:
        raise SystemExit(f'coverage: never reached {missing}')
    print(f'ok: {runs} hits agree; {calls} calls out matched; all {len(emu.cover)} paths reached')


if __name__ == '__main__':
    main()
