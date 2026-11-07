"""PM's firing, the rest against the exe at 53 bits, exact: the plan's virtuals 0x4baae0 (turn), 0x4ba9e0 (cone),
0x4ba0d0 (place), 0x4baac0 (sight), the weapon checks 0x4b9de0 with 0x4b9d80, 0x4b8f80, 0x4bd1c0, and the line 0x4b9f00
with the start-up's table 0x4b9e40, the table compared bit for bit. The vehicle, weapons and manager are made up behind their fields and virtuals;
0x4b7800, 0x4b76d0, 0x41df20, 0x4e5d00, 0x4ff250, 0x5cb260, 0x5cb2b0, 0x4ff3c0, 0x4a7000 and rand stand in, logged. Case by
case the calls and the answer must match the port's (mech3/zai_firing_pm, tests/probe_pm_ai_aim.cpp).

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_ai_plan.py
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

NI, NW, NF, WF, MAX, RAND = 11, 10, 40, 12, 6, 0x5de668
ZERO = 0x66f5e0


def fb(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


class Exe:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        self.w = lambda a, v: self.uc.mem_write(a, struct.pack('<I', v & 0xffffffff))
        a = iter(range(z.alloc(0x20000), 1 << 31, 0x400))
        w = self.w
        self.plan, pvt, self.slot, self.c, self.c2, self.seen, svt, self.ent, evt, self.me, mvt = (next(a) for _ in range(11))
        self.veh, vvt, self.wm, self.grp, self.gw, self.part, self.ctx, self.firing, self.mat, self.wmx, wvt = (next(a) for _ in range(11))
        self.mount, self.mountrec, hvt, self.hold, self.pos, self.vpos, self.epos = (next(a) for _ in range(7))
        w(self.plan, pvt), w(self.plan + 4, self.slot), w(self.plan + 0x14, self.veh), w(self.slot, self.c)
        w(self.c + 4, self.seen), w(self.c2 + 4, self.seen), w(self.seen, svt), w(svt + 0x2c, self.ret(lambda u: self.ent if self.inp[1] else 0))
        w(self.ent, evt), w(evt + 0x58, self.ret(lambda u: self.me if self.inp[2] else 0))
        w(self.veh, vvt), w(self.veh + 0xd4, self.wm), w(self.veh + 0xc, self.mat)
        w(vvt + 0x90, self.ret(self.aim, 0xc)), w(vvt + 0x9c, self.ret(self.turn, 8)), w(vvt + 0x160, self.ret(self.face, 4))
        w(self.ctx + 4, self.wm), w(self.me, mvt), w(mvt + 0x94, self.ret(self.reaches, 4)), w(self.me + 0xc, self.mat)
        w(self.wm + 0x2c, 0), w(self.wm + 0x30, self.grp), w(self.grp + 0x10, self.gw)
        w(self.hold, hvt), w(hvt + 0x2c, self.ret(lambda u: u.ecx)), w(hvt + 0x48, self.ret(lambda u: self.me)), w(self.wmx + 0x88, self.hold)
        self.ws, self.recs, self.wmxs = [], [], []
        w(wvt + 0x20, self.ret(lambda u: self.wi(self.ws.index(u.ecx))[1] & 0xffffffff))
        for i in range(MAX):
            x, rec, wmx = next(a), next(a), next(a)
            self.ws.append(x), self.recs.append(rec), self.wmxs.append(wmx)
            w(x, wvt), w(x + 0x9c, wmx), w(self.wm + 0x44 + 4 * i, x), w(self.grp + 0x14 + 4 * i, x)
        w(RAND, self.ret(self.rand))
        self.uc.mem_write(ZERO, bytes(12))
        self.uc.mem_write(0x4b7b90, b'\xd9\x81\x04\x01\x00\x00\xc3')  # fld [ecx + 0x104]
        for addr, fn, pop in ((0x41df20, lambda u: self.log.append(0x91) or 0, 0), (0x4b7800, self.cos, 0x18), (0x4b76d0, self.wplace, 4), (0x5cb260, lambda u: self.vpos, 0),
                              (0x5cb2b0, lambda u: self.epos, 0), (0x4ff250, self.pplace, 0), (0x4e5d00, self.sight, 8), (0x4ff3c0, self.buried, 0),
                              (0x4a7000, self.mreach, 8), (0x4b4a20, lambda u: self.vec(0x95, u.arg(0)) or 0, 4)):
            z.native_stub(addr, fn, pop)

    def ret(self, fn, pop=0):
        t = self.z.alloc(4)
        self.uc.mem_write(t, b'\xc3')
        self.z.native_stub(t, fn, pop)
        return t

    def wi(self, i):
        return self.inp[NI + NW * i:NI + NW * (i + 1)]

    def vec(self, k, a):
        self.log += [k] + [self.z.r32(a + 4 * i) for i in range(3)]

    def rand(self, u):
        r = self.rands[self.nr]
        self.nr += 1
        return r

    def aim(self, u):
        self.log += [0x92, 1 if u.arg(1) else 0]
        self.uc.mem_write(u.arg(2), struct.pack('<3f', *self.f[1:4]))
        return self.inp[5]

    def turn(self, u):
        self.vec(0x93, u.arg(0))
        self.uc.mem_write(u.arg(1), struct.pack('<3f', *self.f[19:22]))
        return self.inp[6]

    def face(self, u):
        self.vec(0x94, u.arg(0))
        return 0

    def cos(self, u):
        i = self.ws.index(u.ecx)
        self.log += [0x96, i + 1]
        self.uc.mem_write(u.arg(4), struct.pack('<f', self.f[NF + WF * i]))
        return self.wi(i)[2]

    def wplace(self, u):
        self.uc.mem_write(u.arg(0), struct.pack('<3f', *self.f[4:7]))
        return u.arg(0)

    def pplace(self, u):
        self.uc.mem_write(u.arg(1), struct.pack('<3f', *self.f[22:25]))
        return 0

    def sight(self, u):
        self.log.append(0x97)
        return self.inp[7]

    def buried(self, u):
        self.log += [0x98, 31 if u.arg(0) == 0x31 else 32]
        self.vec(0x98, u.arg(1))
        return self.wi(self.cur)[5]

    def mreach(self, u):
        self.vec(0x99, u.arg(0))
        return self.wi(self.cur)[7]

    def reaches(self, u):
        self.vec(0x9a, u.arg(0))
        return self.wi(self.cur)[8]

    def run(self, inp, rands, f):
        z, w, uc = self.z, self.w, self.uc
        self.inp, self.rands, self.nr, self.f, self.log, self.cur = inp, rands, 0, f, [], 0
        put = lambda a, *v: uc.mem_write(a, struct.pack(f'<{len(v)}f', *v))  # noqa: E731
        op = inp[0]
        uc.mem_write(self.plan + 0x18, bytes(12))
        w(self.plan + 0x10, self.grp if inp[3] else 0), w(self.plan + 0x24, self.part if inp[8] else 0), w(self.part + 8, 0x77)
        w(self.grp + 0x10, self.gw if inp[4] else 0)
        put(self.vpos, *f[7:10]), put(self.mat, *f[10:19]), put(self.epos, *f[25:28])
        put(self.c + 0x10, *f[28:31], f[31]), put(self.c2 + 0x10, *f[32:35], f[35])
        put(self.pos, *f[1:4])
        w(self.wm + 0x28, inp[9]), w(self.grp + 8, inp[9])
        for i in range(inp[9]):
            ii, x, ff = self.wi(i), self.ws[i], f[NF + WF * i:NF + WF * (i + 1)]
            w(x + 0x30, 1 if ii[0] else 0), put(x + 0x74, ff[1]), put(x + 0x104, ff[2])
            w(x + 0x28, self.mount if ii[3] else 0), w(x + 0x84, self.recs[i] if ii[3] else 0), w(x + 0x24, 0x32), w(self.mount + 8, 0x31)
            w(x + 0x6c, 1 if ii[4] else 0), put(self.recs[i] + 0x70, *ff[3:6]), put(self.recs[i] + 0x60, ff[6]), put(x + 0x88, *ff[7:10])
            w(self.wmxs[i] + 0x88, self.hold if ii[9] else 0)
        r = 0
        if op == 0:
            r = z.call(0x4baae0, self.plan, [fb(f[0])]) & 0xff
        elif op == 1:
            r = z.call(0x4ba9e0, self.plan, [self.firing, self.ctx, fb(f[0])]) & 0xff
        elif op == 2:
            self.vec(0x9b, z.call(0x4ba0d0, self.plan))
        elif op == 3:
            z.call(0x4baac0, self.plan)
            r = bytes(uc.mem_read(self.plan + 0xd, 1))[0]
        elif op == 4:
            b = z.call(0x4b9de0, self.wm)
            r = self.ws.index(b) + 1 if b in self.ws else 0
        elif op == 5:
            r = z.call(0x4b8f80, self.ws[0], [self.pos]) & 0xff
        elif op == 6:
            for i in range(inp[9]):
                self.cur = i
                if z.call(0x4b8f80, self.ws[i], [self.pos]) & 0xff:
                    r = 1
                    break
        else:
            cone = self.z.alloc(16)
            w(cone + 4, self.c)
            r = z.call(0x4b9f00, cone, [self.c2]) & 0xff
        self.log += [0x2b, r]
        if op == 0:
            self.log += [z.r32(self.plan + 0x18 + 4 * i) for i in range(3)]
        return self.log + [self.nr]


def rf(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


def unit(rng):
    a, b = rng.uniform(0, 6.28), rng.uniform(-1, 1)
    return [math.cos(a) * math.cos(b), math.sin(b), math.sin(a) * math.cos(b)]


def case(rng):
    op = rng.randrange(8)
    n = rng.choice([0, 1, 2, 3, 6])
    inp = [op, int(rng.random() < 0.95), int(rng.random() < 0.9), rng.getrandbits(1), int(rng.random() < 0.8), int(rng.random() < 0.9), int(rng.random() < 0.3),
           rng.getrandbits(1), rng.getrandbits(1), n, 0]
    if op == 0:
        inp[4] = 1 if inp[3] else inp[4]  # no group with a shot: the exe faults, fixes.md
    if op in (5,):
        inp[9] = max(1, n)
    f = [rng.uniform(0.5, 1.0)] + [rng.uniform(-500, 500) for _ in range(9)]
    u, v = unit(rng), unit(rng)
    w2 = [u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]]
    n2 = math.sqrt(sum(x * x for x in w2)) or 1
    f += u + v + [x / n2 for x in w2] + unit(rng) + [rng.uniform(-500, 500) for _ in range(6)]
    a = unit(rng)
    b = [x + rng.uniform(-0.05, 0.05) for x in a]
    d1, d2 = rng.uniform(1, 300), rng.choice([10.0, 30.0, rng.uniform(1, 300), 200.0, 250.0])
    f += [x * d1 for x in a] + [-1.0] + [x * d2 for x in b] + [-1.0]
    f += [0.0] * (NF - len(f))
    rnds = [rng.choice([rng.randrange(32768), 0, 32767]) for _ in range(16)]
    for i in range(MAX):
        inp += [int(rng.random() < 0.9), rng.choice([-1, 0, 5, 5]), rng.getrandbits(1), rng.getrandbits(1), rng.getrandbits(1), int(rng.random() < 0.3), 1,
                rng.getrandbits(1), rng.getrandbits(1), int(rng.random() < 0.9)]
        f += [rng.uniform(0.5, 1), rng.choice([0.0, 0.5, 1.0]), rng.choice([100.0, 200.0, rng.uniform(50, 900)])] + [rng.uniform(-500, 500) for _ in range(3)]
        f += [rng.uniform(0, 3)] + [rng.uniform(-3, 3) for _ in range(3)] + [0.0, 0.0]
    return inp, rnds, [rf(x) for x in f]


def main():
    p = test_pm_zar.load_pm_probe()
    P = ctypes.c_void_p
    p.probe_pm_ai_plan.argtypes = [P, P, P, P, ctypes.c_int]
    p.probe_pm_cone_table.argtypes = [P]
    rng = random.Random(1)
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 16000
    emu = Exe()
    # The start-up's own 0x4b9ed0 builds the table; _hypot lives in the C library, here its sqrt(x*x + y*y) on the x87.
    hypot = emu.z.cpu.code(bytes.fromhex('dd442404 d8c8 dd44240c d8c8 dec1 d9fa c3'.replace(' ', '')))
    emu.w(0x5de5b8, hypot)
    emu.z.call(0x4b9ed0)
    table = (ctypes.c_float * 2)()
    p.probe_pm_cone_table(table)
    exe_table = [emu.z.r32(0x62f8e0), emu.z.r32(0x62f8e8)]
    if exe_table != [fb(table[0]), fb(table[1])]:
        print(f'the cone table: exe {[hex(x) for x in exe_table]}, port {[hex(fb(x)) for x in table]}')
        return 1
    out = (ctypes.c_uint32 * 200)()
    st = collections.Counter()
    for k in range(n):
        inp, rnds, f = case(rng)
        a = emu.run(inp, rnds, f)
        m = p.probe_pm_ai_plan((ctypes.c_int32 * len(inp))(*inp), (ctypes.c_int32 * 16)(*rnds), (ctypes.c_float * len(f))(*f), out, 200)
        b = list(out[:m])
        if a != b:
            i = next((j for j, (x, y) in enumerate(zip(a, b)) if x != y), min(len(a), len(b)))
            print(f'case {k} op {inp[0]}: differ at {i}\n exe  {[hex(x) for x in a[max(0, i - 4):i + 12]]}\n port {[hex(x) for x in b[max(0, i - 4):i + 12]]}\n  in {inp[:NI]}')
            return 1
        st[f'op{inp[0]}'] += 1
        st[f'yes{inp[0]}'] += a[a.index(0x2b, 0) + 1] != 0 if 0x2b in a else 0
    print(f'ok: {n} cases, {dict(st)}: identical')
    pm_floors.floors(cases=(n, 5000), turned=(st['yes0'], 100), coned=(st['yes1'], 100), best=(st['yes4'], 200), hit=(st['yes5'], 100), any=(st['yes6'], 100), line=(st['yes7'], 100))
    return 0


if __name__ == '__main__':
    sys.exit(main())
