"""PM's firing's step 0x4bb700 against the exe, every state, at 53 bits, exact. Everything it calls stands in, logged,
with answers from one script of bits: the pick, the shot, the aim, the plan's virtuals, the group's soonest, the cone
0x4b9f00. Each case runs 40 frames of made-up clock and heat from a start phase; the calls in order and the state's
fields each frame must match the port's (mech3/zai_firing_pm, tests/probe_pm_ai_aim.cpp). What the stand-ins do is
tested in test_pm_ai_aim, _pick and _shot; the plan's virtuals aren't yet.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_ai_step.py
"""
import collections
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
os.environ['MW3_X87'] = '53'  # the reference's precision: PM's AI, docs/pm-tests.md
import pm_floors  # noqa: E402
import test_pm_zar  # noqa: E402

STEP, NOW, FRAMES, NF = 0x4bb700, 0x62da90, 40, 15


def fb(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


class Exe:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        self.w = lambda a, v: self.uc.mem_write(a, struct.pack('<I', v & 0xffffffff))
        a = iter(range(z.alloc(0x8000), 1 << 31, 0x400))
        w = self.w
        self.fi, self.brain, self.bvt, bvt2, self.veh, vvt, self.mech, mvt, self.wm, self.plan, pvt, self.rec = (next(a) for _ in range(12))
        self.slot, self.con, self.ot, self.grp, self.cells, self.lst, self.lk, lvt, self.aim, self.at = (next(a) for _ in range(10))
        w(self.fi + 0x6c, self.brain), w(self.brain, bvt2), w(self.brain + 0x100, self.bvt), w(self.bvt + 4, 0)
        w(self.brain + 0x11c, self.veh), w(self.brain + 0x124, self.rec), w(self.veh, vvt), w(self.mech, mvt)
        w(self.brain + 0xd0, 0x77), w(self.fi + 0x50, 0x55), w(self.fi + 0x98, self.aim), w(self.fi + 0x70, lvt)
        w(vvt + 0x58, self.ret(lambda u: self.mech if self.inp[1] else 0))
        w(mvt + 0xa8, self.fld(self.cells)), w(mvt + 0x58, self.ret(lambda u: u.ecx))
        w(vvt + 0x164, self.ret(self.stand, 0xc)), w(self.bvt + 0x10, self.ret(self.trigger, 4)), w(self.bvt + 0x78, self.ret(lambda u: self.put(13)))
        w(bvt2 + 0x20, self.ret(lambda u: self.put(19))), w(lvt, self.ret(lambda u: self.put(5), 4))
        w(self.plan, pvt), w(self.plan + 4, self.slot)
        w(pvt + 0x14, self.ret(lambda u: self.ans(8, u.arg(0)), 4)), w(pvt + 4, self.ret(lambda u: self.ans(9, u.arg(2)), 0xc))
        w(pvt + 0x1c, self.ret(self.sight)), w(pvt + 0x20, self.ret(self.place))
        w(self.veh + 0xac, self.lst), w(self.veh + 0xb0, self.lst + 4), w(self.lst, 0x999)
        self.uc.mem_write(0x4b7b50, b'\xd9\x05' + struct.pack('<I', self.cells + 8) + b'\xc3')
        self.uc.mem_write(0x4b5bf0, b'\xd9\x05' + struct.pack('<I', self.cells + 4) + b'\xc3')
        for addr, fn, pop in ((0x41ad50, lambda u: self.put(1), 0), (0x5bfdc0, lambda u: self.put(2, u.arg(0)), 4), (0x434980, lambda u: self.put(3, u.arg(0)), 4),
                              (0x4e7150, lambda u: self.put(4, u.arg(0)), 4), (0x4b9de0, lambda u: self.best(), 0), (0x4bc2e0, self.pick, 4), (0x4bc8c0, self.free, 0),
                              (0x4bcd40, self.shot, 0x10), (0x4bd1c0, lambda u: self.ans(12), 4), (0x4bb040, lambda u: self.put(16), 0xc), (0x4bb580, lambda u: self.put(17, u.arg(0)), 0x14),
                              (0x4b4370, lambda u: 0, 0), (0x4b9f00, self.cone, 4), (0x4677e0, lambda u: 0, 8)):
            z.native_stub(addr, fn, pop)

    def fld(self, cell):
        return self.z.cpu.code(b'\xd9\x05' + struct.pack('<I', cell) + b'\xc3')

    def ret(self, fn, pop=0):
        t = self.z.alloc(4)
        self.uc.mem_write(t, b'\xc3')
        self.z.native_stub(t, fn, pop)
        return t

    def put(self, k, v=0):
        self.log += [k, v]
        return 0

    def bit(self):
        b = self.bits[self.n]
        self.n += 1
        return b

    def ans(self, k, v=None):
        self.put(k, v or 0)
        return self.bit()

    def best(self):
        self.put(0x15)
        return 0x8000 if self.bit() else 0

    def pick(self, u):
        self.put(6)
        return self.plan if self.bit() else 0

    def free(self, u):
        self.put(7)
        self.w(u.ecx + 0x14, 0)
        return 0

    def sight(self, u):
        self.put(10)
        s = self.bit()
        self.uc.mem_write(self.plan + 0xd, bytes([s]))
        return s

    def place(self, u):
        self.put(0x16)
        return self.at

    def shot(self, u):
        self.put(11, u.arg(2)), self.put(11, u.arg(3))
        return self.bit()

    def cone(self, u):
        self.put(0x14)
        return self.bit()

    def trigger(self, u):
        self.put(14, u.arg(0))
        return 0

    def stand(self, u):
        self.put(18)
        self.w(u.arg(1), self.veh)
        return 0

    def run(self, inp, frames, bits, f):
        z, w, uc, r32 = self.z, self.w, self.uc, self.z.r32
        self.inp, self.bits, self.n, self.log = inp, bits, 0, []
        put = lambda a, *v: uc.mem_write(a, struct.pack(f'<{len(v)}f', *v))  # noqa: E731
        fi, b = self.fi, self.brain
        w(fi, inp[0]), put(fi + 4, 1e6 if inp[3] & 8 else 0.0), put(fi + 8, f[4], f[5], f[6]), w(fi + 0x14, 0 if inp[0] in (0, 8) else self.plan)
        put(fi + 0x18, f[7], f[8], f[9]), put(fi + 0x2c, 0.0, 0.25), uc.mem_write(fi + 0x34, bytes([inp[3] & 1])), put(fi + 0x40, 0.0)
        put(fi + 0x38, f[10]), put(fi + 0x58, f[11], 0.0, f[12], f[13], 0.0)
        put(b + 0x34, f[0], f[1]), put(b + 0x30, f[2]), put(self.rec + 0x54, f[3]), put(self.cells + 8, f[14])
        w(self.veh + 0xd4, self.wm if inp[2] else 0), w(self.plan + 0x10, 1 if inp[3] & 2 else 0), uc.mem_write(self.plan + 0xc, bytes([0, 1 if inp[3] & 4 else 0]))
        for k in range(frames):
            fr, ff = inp[4 + 4 * k:8 + 4 * k], f[NF + 4 * k:NF + 4 + 4 * k]
            w(self.veh + 0x6c, self.con if fr[0] else 0), w(self.wm + 0x2c, 0 if fr[1] else -1), w(self.wm + 0x30, self.grp)
            w(self.slot, self.con if fr[2] else 0), uc.mem_write(self.plan + 0xc, bytes([fr[3]]))
            put(NOW, ff[0]), put(self.cells, ff[1]), put(self.cells + 4, ff[3]), put(self.mech + 0x590, ff[2])
            z.call(STEP, fi, [fb(1 / 30)])
            self.log += [0x2b, r32(fi), r32(fi + 4), r32(fi + 0x30), r32(fi + 0x2c), bytes(uc.mem_read(fi + 0x34, 1))[0], r32(fi + 0x5c), r32(fi + 0x68), 1 if r32(self.plan + 0x10) else 0, r32(fi + 0x40)]
        return self.log + [self.n]


def rf(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


def case(rng):
    frames = FRAMES
    inp = [rng.choice([8, 0, 100, 1, 2, 3, 4, 5, 6, 7, 7, 3]), int(rng.random() < 0.8), int(rng.random() < 0.97), rng.choice([0, 0, 0, 2, 6, 8, 10, 14, 1, 9, 15, 11])]
    f = [rng.choice([0.0, rng.uniform(0.1, 3)]), rng.uniform(1, 6), rng.uniform(100, 500), rng.uniform(0.2, 2), rng.choice([0.0, rng.uniform(0, 3)]), rng.choice([0.0, rng.uniform(1, 6)]), rng.choice([0.0, rng.uniform(0.5, 3)]), 0.9, 0.87, 0.7,
         rng.uniform(0.5, 2), rng.uniform(1, 6), rng.uniform(0.2, 1.2), rng.uniform(0.2, 3), rng.uniform(50, 900)]
    t = rng.uniform(1, 500)
    for k in range(frames):
        t += rng.choice([1 / 30, 0.5, 1.5, 3.0, 0.0])
        inp += [int(rng.random() < 0.92), int(rng.random() < 0.9), int(rng.random() < 0.95), int(rng.random() < 0.4)]
        f += [t, rng.choice([f[12], rng.uniform(0, 2), rng.uniform(0, 2)]), rng.choice([0.0, 0.5]), rng.uniform(-1, 3)]
    p = rng.choice([0.5, 0.8, 0.9])
    bits = bytes(int(rng.random() < p) for _ in range(400))
    return inp, bits, [rf(x) for x in f]


def main():
    p = test_pm_zar.load_pm_probe()
    P = ctypes.c_void_p
    p.probe_pm_ai_step.argtypes = [P, ctypes.c_int, P, P, P, ctypes.c_int]
    rng = random.Random(1)
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    emu = Exe()
    out = (ctypes.c_uint32 * 8000)()
    st = collections.Counter()
    for k in range(n):
        inp, bits, f = case(rng)
        a = emu.run(inp, FRAMES, bits, f)
        m = p.probe_pm_ai_step((ctypes.c_int32 * len(inp))(*inp), FRAMES, bits, (ctypes.c_float * len(f))(*f), out, 8000)
        b = list(out[:m])
        if a != b:
            i = next((j for j, (x, y) in enumerate(zip(a, b)) if x != y), min(len(a), len(b)))
            print(f'case {k}: differ at {i} of {len(a)}, {len(b)}\n exe  {[hex(x) for x in a[max(0, i - 12):i + 10]]}\n port {[hex(x) for x in b[max(0, i - 12):i + 10]]}\n  in {inp[:4]}')
            return 1
        for j, x in enumerate(a):
            if x == 0x2b and j + 1 < len(a):
                st[a[j + 1]] += 1
        st['lead'] += a.count(17)
        st['flush'] += a.count(1) > 0
    print(f'ok: {n} cases, {dict(st)}: identical')
    pm_floors.floors(cases=(n, 1000), lead=(st['lead'], 100), flush=(st['flush'], 50), fire=(st[4], 100), track=(st[3], 100), hold=(st[6], 100), wait=(st[2], 100))
    return 0


if __name__ == '__main__':
    sys.exit(main())
