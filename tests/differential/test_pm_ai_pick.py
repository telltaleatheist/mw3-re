"""PM's firing's plan's pick against the exe: 0x4bc2e0 with 0x4bbfa0, 0x4bbe30, 0x4bbf20, 0x4ba190, 0x4ba340, 0x4ba030,
0x4bc230, 0x4bc260, 0x4bc2a0 and the sort 0x4bd760 real, at 53 bits, exact. The vehicle, contacts, entities and parts
are made up behind their virtuals; 0x4e5d00, 0x5cb260, 0x4b2820, 0x4b2890 and rand stand in. Each case logs every
target asked for, what faces and sight are asked of, the plan made (its contact, score, flags, part) and the firing's
fields; the port's (mech3/zai_firing_pm, tests/probe_pm_ai_aim.cpp) must match.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_ai_pick.py
"""
import collections
import ctypes
import math
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
os.environ['MW3_X87'] = '53'  # the reference's precision: PM's AI, docs/pm-tests.md
import pm_floors  # noqa: E402
import test_pm_zar  # noqa: E402

PICK, NOW, RAND, THREAT0, THREAT1, SIGHT, PLACE, PLAYER = 0x4bc2e0, 0x62da90, 0x5de668, 0x4b2820, 0x4b2890, 0x4e5d00, 0x5cb260, 0x62c40c
NI, NF, CI, CF, MAX = 16, 18, 16, 18, 10
SLOTS = (0x39c, 0x3a0, 0x3a4, 0x3a8, 0x3ac, 0x3b0, 0x3b4)


class Exe:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        self.w = lambda a, v: self.uc.mem_write(a, struct.pack('<I', v & 0xffffffff))
        self.base = z.alloc(0x40000)
        a = iter(range(self.base, self.base + 0x40000, 0x800))
        w = self.w
        self.firing, self.ctx, self.veh, vvt, self.brain, bvt, self.lst, self.held, self.aimed, self.mine, self.player = (next(a) for _ in range(11))
        self.plan, self.me = next(a), next(a)
        w(self.ctx, self.veh), w(self.veh, vvt), w(self.firing + 0x6c, self.brain), w(self.brain + 0x100, bvt), w(bvt + 4, 0)
        w(vvt + 0x98, self.ret(self.faces, 4)), w(vvt + 0xcc, self.ret(self.select, 8))
        pl = next(a)
        w(PLAYER, pl), w(pl + 0x14, self.player)
        w(RAND, self.ret(self.rand))
        self.c, self.seen, self.e, self.m, self.g, self.pm = [], [], [], [], [], []
        self.cell = next(a)
        for i in range(MAX):
            c, sn, snv, e, ev, m, mv, g, gv, pm, pmv = (next(a) for _ in range(11))
            self.c.append(c), self.seen.append(sn), self.e.append(e), self.m.append(m), self.g.append(g), self.pm.append(pm)
            w(c + 4, sn), w(sn, snv), w(snv + 0x2c, self.ret(lambda u, i=i: self.e[i] if self.ci(i)[0] else 0))
            w(e, ev), w(m, mv), w(g, gv), w(pm, pmv)
            w(ev + 0x28, self.ret(lambda u: u.ecx))
            w(ev + 0x48, self.ret(lambda u, i=i: self.m[i] if self.ci(i)[7] else 0))
            w(ev + 0x50, self.ret(lambda u, i=i: self.ci(i)[11]))
            w(ev + 0x58, self.ret(lambda u, i=i: self.pm[i] if self.ci(i)[13] else 0))
            w(ev + 0x68, self.ret(lambda u, i=i: self.g[i] if self.ci(i)[4] else 0))
            w(ev + 0xa4, self.fld(self.cell + 16 * i, 0))
            w(mv + 0x58, self.ret(lambda u, i=i: {0: 0, 1: 0x77, 2: self.player}[self.ci(i)[8]]))
            w(mv + 0x100, self.ret(lambda u, i=i: self.ci(i)[10]))
            w(gv + 0xf8, self.ret(lambda u, i=i: self.ci(i)[5]))
            w(gv + 0xe8, self.ret(lambda u, i=i: self.ci(i)[6]))
            for k, off in enumerate(SLOTS):
                part, pv = z.alloc(0x80), z.alloc(0x60)
                w(pm + off, part), w(part, pv), w(pv + 0x48, self.fld(self.cell + 0x200 + 32 * i + 4 * k, 4))
                w(part + 0x74, 0x1000 + 0x80 + i * 0x100 + (k if k < 2 else 0))
                self.partmap[part] = 0x1000 + i * 0x100 + off - 0x39c
        for addr, fn, pop in ((SIGHT, self.sight, 8), (PLACE, lambda u: self.place(u), 0), (THREAT0, lambda u: self.threat(13), 0),
                              (THREAT1, lambda u: self.threat(14), 0), (0x5ce8c6, lambda u: self.plan2, 0),
                              (0x5ce8c0, lambda u: self.freed.append(u.arg(0)) or 0, 0)):
            z.native_stub(addr, fn, pop)
        self.plan2 = next(a)
        z.call(0x4ba110), z.call(0x4ba140)  # the start-up's: 0.25 and the no-contact record

    partmap = {}

    def fld(self, cell, pop):
        return self.z.cpu.code(b'\xd9\x05' + struct.pack('<I', cell) + (b'\xc2' + struct.pack('<H', pop) if pop else b'\xc3'))

    def ret(self, fn, pop=0):
        t = self.z.alloc(4)
        self.uc.mem_write(t, b'\xc3')
        self.z.native_stub(t, fn, pop)
        return t

    def ci(self, i):
        return self.inp[NI + CI * i:NI + CI * (i + 1)]

    def rand(self, u):
        r = self.rands[self.nrand]
        self.nrand += 1
        return r

    def faces(self, u):
        self.log += [0x73] + [self.z.r32(u.arg(0) + 4 * i) for i in range(3)]
        return self.inp[11] & 1

    def sight(self, u):
        i = self.e.index(u.arg(0))
        self.log += [0x74, i]
        return self.ci(i)[1]

    def select(self, u):
        c = u.arg(0)
        if not c:
            self.log.append(0x72)
            self.w(self.veh + 0x6c, 0)
            return 0
        self.log += [0x71, self.c.index(c)]
        ok = (self.inp[12] >> (self.nsel & 15)) & 1
        self.nsel += 1
        if ok:
            self.w(self.veh + 0x6c, c)
        return ok

    def place(self, u):
        return u.ecx + 0x100

    def threat(self, k):
        v = self.inp[k]
        return self.e[v - 1] if v else 0

    def run(self, inp, rands, f):
        z, w, uc, r32 = self.z, self.w, self.uc, self.z.r32
        self.inp, self.rands, self.nrand, self.nsel, self.log, self.freed = inp, rands, 0, 0, [], []
        n = inp[0]
        put = lambda a, *v: uc.mem_write(a, struct.pack(f'<{len(v)}f', *v))  # noqa: E731
        b, fi = self.brain, self.firing
        w(b + 0xd4, inp[1]), w(b + 0xd8, inp[2])
        uc.mem_write(b + 0xed, bytes([inp[3], inp[4]]))
        w(b + 0x2cc, inp[5])
        put(b + 0xdc, *f[:4]), put(b + 0xcc, f[4]), put(b + 0x30, f[5]), put(fi + 0x2c, f[6]), put(NOW, f[7]), put(fi + 0x4c, f[8])
        put(self.veh + 0x24, *f[9:12]), put(self.aimed + 0x100, *f[12:15]), put(self.veh + 0x100, *f[15:18])
        w(self.veh + 0xc4, inp[10])
        w(fi + 0x8c, self.e[inp[7] - 1] if inp[7] else 0), w(fi + 0x90, self.e[inp[8] - 1] if inp[8] else 0)
        w(fi + 0x28, self.aimed if inp[9] else 0)
        uc.mem_write(self.aimed + 8, bytes([1 if inp[9] == 2 else 0]))
        w(self.veh + 0x6c, self.c[inp[15] - 1] if inp[15] else 0)
        for i in range(n):
            w(self.lst + 4 * i, self.c[i])
        w(self.veh + 0xac, self.lst if n else 0), w(self.veh + 0xb0, self.lst + 4 * n if n else 0)
        for i in range(n):
            ii, ff = self.ci(i), f[NF + CF * i:NF + CF * (i + 1)]
            c = self.c[i]
            put(c + 0xc, *ff[:5])
            uc.mem_write(self.e[i] + 8, bytes([ii[2]]))
            uc.mem_write(self.e[i] + 0xbc, bytes([ii[9]]))
            w(self.e[i] + 0xc0, ii[3])
            put(self.cell + 16 * i, ff[8])
            put(self.pm[i] + 0x578, ff[9])
            for k in range(7):
                put(self.cell + 0x200 + 32 * i + 4 * k, ff[10 + k])
        self.w(self.plan + 4, self.plan + 0x40), w(self.plan + 0x40, 0x1234 if inp[6] == 1 else 0)
        w(fi + 0x14, self.plan if inp[6] else 0)
        r = z.call(PICK, fi, [self.ctx])
        tail = [0x2b]
        if not r:
            tail.append(0)
        elif r == self.plan:
            tail.append(2)
        else:
            slot = r32(r + 4)
            tail += [1, 99 if slot == self.veh + 0x6c else (slot - self.lst) // 4, r32(r + 8), bytes(uc.mem_read(r + 0xc, 1))[0], bytes(uc.mem_read(r + 0xd, 1))[0]]
            p = r32(r + 0x24)
            tail.append(self.partmap.get(p, p))
        t = r32(self.veh + 0x6c)
        s = r32(self.ctx + 0xc)
        tail += [r32(fi + 0x4c), 1 if r32(fi + 0x90) else 0, 1 if r32(fi + 0x28) else 0, self.c.index(t) + 1 if t in self.c else 0,
                 self.c.index(s) + 1 if s in self.c else 0, self.nrand]
        return self.log + tail


def rf(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


def case(rng):
    n = rng.choice([0, 1, 2, 3, 5, 8, 10, 10])
    pick = rng.choice([0, 0, 1, 2, 3, 4, 5, 6])
    inp = [n, pick, rng.choice([0, 0, 1, 2, 3, 4]), rng.choice([0xe4, 0xe4, 0, 4, 8, 0x10, 0x20, 0x40, 0x80, rng.getrandbits(8)]),
           rng.choice([1, 1, 0]), rng.choice([0, 0x99]), rng.choice([0, 0, 1, 2]), 0, 0, rng.choice([0, 0, 1, 2]), rng.choice([2, 2, 4, 1]),
           rng.getrandbits(1), rng.choice([0xffff, 0xffff, rng.getrandbits(16)]), 0, 0, 0]
    if n:
        inp[7] = rng.choice([0, 0, rng.randint(1, n)])
        inp[8] = rng.choice([0, 0, rng.randint(1, n)])
        inp[13], inp[14] = rng.choice([0, rng.randint(1, n)]), rng.choice([0, rng.randint(1, n)])
        inp[15] = rng.choice([0, rng.randint(1, n)])
    else:  # no contacts: the next wish is kept
        inp[8] = rng.choice([0, 1])
    f = [rng.choice([0.0, rng.uniform(0, 4)]) for _ in range(4)] + [rng.uniform(1, 9), rng.uniform(100, 900), rng.uniform(100, 900)]
    now = rng.uniform(1, 900)
    a = rng.uniform(0, 6.28)
    f += [now, now + rng.uniform(-5, 5), math.sin(a), 0.0, math.cos(a)]
    f += [rng.uniform(-300, 300) for _ in range(3)] + [rng.uniform(-300, 300) for _ in range(3)]
    if rng.random() < 0.3:
        f[12:15] = [x + rng.uniform(-8, 8) for x in f[15:18]]
    d = sorted(rng.uniform(1, 700) for _ in range(n))
    for i in range(MAX):
        inp += [int(rng.random() < 0.95), rng.getrandbits(1), int(rng.random() < 0.95), rng.choice([8, 8, 8, 2]), int(rng.random() < 0.7),
                rng.choice([0, 0, 0, inp[5]]), rng.choice([0, 1]), int(rng.random() < 0.8), rng.choice([0, 1, 2]), rng.choice([0, 0x10, 0x18]),
                rng.choice([0, 1]), rng.choice([0, 1]), 0, int(rng.random() < 0.8), 0, 0]
        r = d[i] if i < n else 10.0
        b = rng.uniform(0, 6.28)
        y = rng.choice([0.0, rng.uniform(-0.3, 0.3), rng.uniform(-0.6, 0.6)])
        v = [r * math.cos(b), r * y, r * math.sin(b)]
        f += [rng.choice([r * r, r * r, 0.5, 2.0]), v[0], v[1], v[2], -1.0, 0.0, 0.0, 0.0, rng.uniform(0, 2), rng.uniform(0, 3)]
        f += [rng.choice([0.0, rng.uniform(0, 1), 1.0]) for _ in range(7)] + [0.0]
    rands = [rng.choice([rng.randrange(32768), 0, 32767]) for _ in range(4)]
    return inp, rands, [rf(x) for x in f]


def main():
    p = test_pm_zar.load_pm_probe()
    P = ctypes.c_void_p
    p.probe_pm_ai_pick.argtypes = [P, P, P, P, ctypes.c_int]
    rng = random.Random(1)
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 6000
    emu = Exe()
    out = (ctypes.c_uint32 * 400)()
    st = collections.Counter()
    for k in range(n):
        inp, rands, f = case(rng)
        a = emu.run(inp, rands, f)
        m = p.probe_pm_ai_pick((ctypes.c_int32 * len(inp))(*inp), (ctypes.c_int32 * 4)(*rands), (ctypes.c_float * len(f))(*f), out, 400)
        b = list(out[:m])
        if a != b:
            i = next((j for j, (x, y) in enumerate(zip(a, b)) if x != y), min(len(a), len(b)))
            print(f'case {k}: differ at {i}\n exe  {[hex(x) for x in a[max(0, i - 4):i + 12]]}\n port {[hex(x) for x in b[max(0, i - 4):i + 12]]}\n  in {inp[:NI]}')
            return 1
        x = a.index(0x2b, max(0, len(a) - 16)) if 0x2b in a[-16:] else a.index(0x2b)
        st[{0: 'none', 1: 'made', 2: 'held'}[a[x + 1]]] += 1
        if a[x + 1] == 1:
            st[f'mode{inp[2]}'] += 1
            st['in part'] += a[x + 6] != 0
            st['rand'] += a[-1] > 0
        st['asked'] += a.count(0x71)
    print(f'ok: {n} cases, {dict(st)}: identical')
    pm_floors.floors(cases=(n, 2000), none=(st['none'], 200), made=(st['made'], 1000), held=(st['held'], 100), asked=(st['asked'], 1000), rand=(st['rand'], 200),
                     lowest=(st['mode1'], 100), highest=(st['mode2'], 100))
    return 0


if __name__ == '__main__':
    sys.exit(main())
