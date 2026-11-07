"""PM's firing's shot against the exe: 0x4bcd40 with 0x4bc930, 0x4bcad0, 0x4bca70, 0x4bd170, the removes, std::sort
0x4bd920 and the bin's 0x4b8d30 real, at 53 bits, exact. The contact's owner and the weapons stand in behind their
virtuals; 0x4b7bd0, 0x4b8f80, 0x4b5dc0, 0x4b5d80 and 0x4b7a90 are stubbed and logged. Each case logs what's asked to
hit, the group cleared, each weapon added with its rate, the mode and the answer; the port's must match.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_ai_shot.py
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

SHOT, MAX = 0x4bcd40, 16


class Exe:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        self.w = lambda a, v: self.uc.mem_write(a, struct.pack('<I', v & 0xffffffff))
        a = iter(range(z.alloc(0x20000), 1 << 31, 0x200))
        w = self.w
        self.wm, self.grp, self.c, self.owner, ovt, self.mech, mvt, self.ic, ivt, self.brain, self.pilot, self.at = (next(a) for _ in range(12))
        w(self.wm + 0x30, self.grp), w(self.c, self.owner), w(self.owner, ovt), w(self.mech, mvt), w(self.ic, ivt)
        w(ovt + 0x2c, self.ret(lambda u: self.owner))
        w(ovt + 0x48, self.ret(lambda u: self.mech))
        w(ovt + 0x58, self.ret(lambda u: self.owner if self.inp[2] else 0))
        w(ovt + 0xa8, self.fld(self.owner + 0x100)), w(ovt + 0xa4, self.fld(self.owner + 0x104))
        w(mvt + 0x14c, self.ret(lambda u: self.ic)), w(ivt + 0x58, self.ret(lambda u: self.brain if self.inp[3] else 0))
        w(self.brain + 0x124, self.pilot)
        self.ws, self.bins = [], []
        wvt = next(a)
        w(wvt + 0x1c, self.z.cpu.code(b'\xd9\x81\x20\x01\x00\x00\xc3'))  # fld [ecx + 0x120]
        w(wvt + 0x20, self.ret(lambda u: self.wi(self.ws.index(u.ecx))[2]))
        for i in range(MAX):
            x, b = next(a), next(a)
            self.ws.append(x), self.bins.append(b)
            w(x, wvt)
            w(self.wm + 0x44 + 4 * i, x)
        self.uc.mem_write(0x4b7b40, b'\xd9\x81\x00\x01\x00\x00\xc3')  # fld [ecx + 0x100]
        self.uc.mem_write(0x4b7b90, b'\xd9\x81\x04\x01\x00\x00\xc3')  # fld [ecx + 0x104]
        for addr, fn, pop in ((0x4b7bd0, lambda u: self.wi(self.ws.index(u.ecx))[0], 0), (0x4b8f80, self.can_hit, 4), (0x4b5dc0, self.removed, 4),
                              (0x4b5d80, self.added, 4), (0x4b7a90, self.rate, 4), (0x5ce8c6, self.new, 0), (0x5ce8c0, lambda u: 0, 0)):
            z.native_stub(addr, fn, pop)

    def new(self, u):
        p = self.heap
        self.heap += (u.arg(0) + 15) & ~15
        return p

    def wi(self, i):
        return self.inp[5 + 6 * i:11 + 6 * i]

    def fld(self, cell):
        return self.z.cpu.code(b'\xd9\x05' + struct.pack('<I', cell) + b'\xc3')

    def ret(self, fn, pop=0):
        t = self.z.alloc(4)
        self.uc.mem_write(t, b'\xc3')
        self.z.native_stub(t, fn, pop)
        return t

    def can_hit(self, u):
        i = self.ws.index(u.ecx)
        self.log += [0x81, i + 1]
        return self.wi(i)[1]

    def removed(self, u):
        n = self.z.r32(self.grp + 8)
        if not self.cleared:
            self.log += [0x82, n]
            self.cleared = True
        self.w(self.grp + 8, n - 1)
        return 1

    def added(self, u):
        self.log += [0x83, self.ws.index(u.arg(0)) + 1]
        return 1

    def rate(self, u):
        self.log.append(u.arg(0))
        return 0

    def run(self, inp, f):
        z, w, uc = self.z, self.w, self.uc
        self.inp, self.log, self.cleared = inp, [], False
        self.heap = self.heap0
        put = lambda a, *v: uc.mem_write(a, struct.pack(f'<{len(v)}f', *v))  # noqa: E731
        w(self.wm + 0x28, inp[0]), w(self.wm + 0x1c, 0), w(self.grp + 8, inp[4])
        put(self.c + 0xc, f[0]), put(self.owner + 0x100, f[3], f[4])
        put(self.pilot + 0x38, f[5], f[6], f[7], f[8], f[9])
        for i in range(inp[0]):
            ii, x = self.wi(i), self.ws[i]
            ff = f[10 + 6 * i:16 + 6 * i]
            put(x + 0x100, ff[0], ff[1]), put(x + 0x1c, ff[2]), put(x + 0x18, ff[3]), put(x + 0x120, ff[4])
            w(x + 0x5c, self.bins[i] if ii[3] else 0)
            w(self.bins[i] + 8, ii[4]), uc.mem_write(self.bins[i] + 0xc, b'\0')
        r = z.call(SHOT, self.wm, [self.c, self.at if inp[1] else 0, struct.unpack('<I', struct.pack('<f', f[1]))[0], struct.unpack('<I', struct.pack('<f', f[2]))[0]])
        mode = z.r32(self.wm + 0x1c)
        if mode:
            self.log += [0x84, mode]
        return self.log + [0x2b, r & 0xff]


def rf(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


def edge(rng, inp, f):
    """Two weapons that fit, the room between their summed heat as a float and as the double the exe compares."""
    while True:
        a, b = rf(rng.uniform(0.1, 1)), rf(rng.uniform(0.001, 0.1))
        if rf(a + b) < a + b:
            break
    inp[:5] = [2, 0, 1, 1, 0]
    inp[5:11], inp[11:17] = [1, 1, -1, 0, 10, 0], [1, 1, -1, 0, 10, 0]
    f[:10] = [100.0, 1.0, 1.0, 0.0, 0.0, 1000.0, rf(a + b), 0.0, 2.0, 0.0]
    f[10:16], f[16:22] = [0.0, 1000.0, 1.0, 2.0, a, 0.0], [0.0, 1000.0, 1.0, 1.0, b, 0.0]
    return inp, [rf(x) for x in f]


def case(rng):
    n = rng.choice([1, 2, 3, 4, 6, 8, 12, 16])
    dist2 = rng.uniform(100, 40000)
    tight = rng.random() < 0.5
    inp = [n, rng.getrandbits(1), int(rng.random() < 0.9), int(rng.random() < 0.9), rng.choice([0, 2, 5])]
    inp[3] = inp[3] or inp[2]  # a vehicle with no brain faults in the exe; the shooter here always has one
    f = [dist2, rng.choice([1.0, 2.0, 0.0, rng.uniform(0, 4)]), rng.uniform(0, 2), rng.uniform(0, 1), rng.uniform(0, 1), rng.uniform(1, 4 if tight else 40),
         rng.uniform(1, 4 if tight else 40), rng.uniform(0, 3), rng.choice([0.0, 0.3, rng.uniform(0, 1), 2.0]), rng.choice([0.0, 0.3, 0.7, 0.5])]
    for i in range(MAX):
        ammo = rng.choice([-1, 0, 5, 10, 20, 5])
        inp += [int(rng.random() < 0.85), int(rng.random() < 0.85), ammo, int(rng.random() < 0.8), rng.choice([10, 20, 5]), 0]
        v = rng.choice([1.0, 2.0, 3.0, rng.uniform(0, 4)])  # ties in the sort
        f += [rng.choice([0.0, dist2 * 0.5, dist2 * 2]) if rng.random() < 0.3 else 0.0, rng.choice([dist2 * 2, dist2 * 2, dist2 * 0.5]),
              rng.choice([1.0, 2.0, rng.uniform(0, 3)]), v, rng.choice([1.0, 2.0, rng.uniform(0, 4)]), 0.0]
    if rng.random() < 0.1:  # the heat's room on a weapon's heat exactly
        f[3], f[6] = 0.0, rng.choice([1.0, 2.0])
    if rng.random() < 0.05:
        return edge(rng, inp, f)
    return inp, [rf(x) for x in f]


def main():
    p = test_pm_zar.load_pm_probe()
    P = ctypes.c_void_p
    p.probe_pm_ai_shot.argtypes = [P, P, P, ctypes.c_int]
    rng = random.Random(1)
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 20000
    emu = Exe()
    emu.heap0 = emu.z.alloc(0x4000)
    out = (ctypes.c_uint32 * 200)()
    st = collections.Counter()
    for k in range(n):
        inp, f = case(rng)
        a = emu.run(inp, f)
        m = p.probe_pm_ai_shot((ctypes.c_int32 * len(inp))(*inp), (ctypes.c_float * len(f))(*f), out, 200)
        b = list(out[:m])
        if a != b:
            i = next((j for j, (x, y) in enumerate(zip(a, b)) if x != y), min(len(a), len(b)))
            print(f'case {k}: differ at {i}\n exe  {[hex(x) for x in a[max(0, i - 4):i + 14]]}\n port {[hex(x) for x in b[max(0, i - 4):i + 14]]}\n  in {inp[:5]} {f[:10]}')
            return 1
        st['fired'] += a[-1]
        st[f'n{min(a.count(0x83), 3)}'] += a[-1]
        st['many'] += a.count(0x83) > 3
    print(f'ok: {n} cases, {dict(st)}: identical')
    pm_floors.floors(cases=(n, 5000), fired=(st['fired'], 3000), one=(st['n1'], 500), two=(st['n2'], 200), three=(st['n3'], 200), many=(st['many'], 100))
    return 0


if __name__ == '__main__':
    sys.exit(main())
