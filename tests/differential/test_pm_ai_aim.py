"""PM's firing's aim error against the exe: 0x4bb040 (track) and 0x4bb580 (lead), with the frame 0x4badc0, the drift
0x4baf40, the re-pick 0x4bb290, the contact's distance 0x4f8dc0 and the unit 0x4013d0 real, at 53 bits, exact. The
plan (vt+0x20, its contact), the entity's virtuals, rand and the weapon's line 0x4b7800 stand in. Each case: a world
and an aim at random, one track or lead; the manager's aim, its flag, the weapon's arguments, the nine words and the
rand count must match the port's (mech3/zai_firing_pm, tests/probe_pm_ai_aim.cpp).

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_ai_aim.py
"""
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

TRACK, LEAD, PLACE, LINE, NOW, ERR, SCALE, RAND = 0x4bb040, 0x4bb580, 0x5cb260, 0x4b7800, 0x62da90, 0x62cc64, 0x62f8ec, 0x5de668
ZERO = 0x66f5e0
IN_FRONT, CARRIED, ON_TARGET = 0x4bb48f, 0x4bb34d, 0x4bb3f8  # the re-pick: offset dropped, carried, carried onto the target


def fb(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


class Exe:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        self.w = lambda a, v: self.uc.mem_write(a, struct.pack('<I', v & 0xffffffff))
        a = iter(range(z.alloc(0x4000), 1 << 31, 0x200))
        self.aim, self.plan, pvt, self.slot, self.c, self.ent, evt, self.mech = (next(a) for _ in range(8))
        mvt, self.pilot, self.brain, bvt, self.veh, vvt, self.pos, self.wm, self.weapon = (next(a) for _ in range(9))
        w = self.w
        w(self.plan, pvt), w(self.plan + 4, self.slot), w(self.slot, self.c), w(pvt + 0x20, self.ret(lambda c: self.pos))
        w(self.ent, evt), w(self.c + 4, self.ent)
        w(evt + 0x2c, self.ret(lambda c: self.ent if self.inp[0] else 0))
        w(evt + 0x48, self.ret(lambda c: self.mech if self.inp[1] else 0))
        w(evt + 0x58, self.ret(lambda c: self.mech if self.inp[2] else 0))
        w(self.mech, mvt), w(self.mech + 0x6a8, self.pilot)
        w(self.brain + 0x100, bvt), w(bvt + 4, 0)
        w(self.brain + 0x11c, self.veh), w(self.veh, vvt), w(vvt + 0x58, self.ret(lambda c: self.veh))
        z.native_stub(PLACE, lambda c: self.mem_vec)
        z.native_stub(LINE, self.line, 0x18)
        self.mem_vec = z.alloc(16)
        rand = self.ret(self.rand)
        w(RAND, rand)
        z.call(0x4bad40)  # the start-up's [0x62f8ec]
        self.uc.mem_write(ZERO, bytes(12))
        self.reached = set()
        for addr in (IN_FRONT, CARRIED, ON_TARGET):
            self.uc.hook_add(4, lambda uc, ad, sz, u: self.reached.add(ad), begin=addr, end=addr)

    def ret(self, fn, pop=0):
        t = self.z.alloc(4)
        self.uc.mem_write(t, b'\xc3')
        self.z.native_stub(t, fn, pop)
        return t

    def rand(self, c):
        r = self.rands[self.nrand]
        self.nrand += 1
        return r

    def line(self, c):
        self.log += [self.z.r32(c.arg(0) + 4 * i) for i in range(3)]
        self.uc.mem_write(c.arg(1), struct.pack('<3f', *self.f[6:9]))
        self.uc.mem_write(c.arg(2), struct.pack('<3f', *self.f[9:12]))
        return 1

    def run(self, inp, rands, f):
        z, w, uc = self.z, self.w, self.uc
        self.inp, self.rands, self.nrand, self.f, self.log = inp, rands, 0, f, []
        uc.mem_write(ERR, struct.pack('<f', f[0])), uc.mem_write(self.brain + 0xb8, struct.pack('<f', f[1]))
        uc.mem_write(self.brain + 0xc0, struct.pack('<f', f[2]))
        w(self.veh + 0x3f4, 1 if inp[3] == 1 else 0), w(self.veh + 0x3c8, 1 if inp[3] == 2 else 0)
        uc.mem_write(self.mem_vec, struct.pack('<3f', *f[3:6]))
        uc.mem_write(NOW, struct.pack('<f', f[12]))
        uc.mem_write(self.pos, struct.pack('<3f', *f[14:17]))
        uc.mem_write(self.c + 0x10, struct.pack('<3f', *f[17:20]))
        uc.mem_write(self.c + 0x1c, struct.pack('<f', f[20]))
        w(self.pilot + 0xc, inp[2])
        uc.mem_write(self.aim, struct.pack('<6f', *f[21:27]) + bytes([inp[6]]) + bytes(3) + struct.pack('<2f', *f[27:29]))
        uc.mem_write(self.wm + 0x8c, b'\x77')
        uc.mem_write(self.wm + 0x18, b'\x77')
        weapon = self.weapon if inp[4] else 0
        if inp[5] == 0:
            z.call(TRACK, self.aim, [self.plan, weapon, self.brain])
        else:
            z.call(LEAD, self.aim, [fb(f[13]), self.plan, weapon, self.wm, self.brain])
            self.log = [0x42, bytes(uc.mem_read(self.wm + 0x8c, 1))[0]] + self.log
            self.log += [0x41] + [z.r32(self.wm + 0xc + 4 * i) for i in range(3)]
        r = self.log + [0x2b] + [z.r32(self.aim + 4 * i) for i in range(6)]
        return r + [bytes(uc.mem_read(self.aim + 0x18, 1))[0], z.r32(self.aim + 0x1c), z.r32(self.aim + 0x20), self.nrand]


def rf(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


def on_target(rng):
    """The carried point lands exactly on the target: the line turned round, 64 ahead and 64 past it."""
    x = float(rng.randrange(-512, 512))
    f = [1.0, 1.0, 1.0, x, 0.0, 0.0, x, 0.0, 0.0, -1.0, 0.0, 0.0, 10.0, 0.0, x + 64, 0.0, 0.0, 64.0, 0.0, 0.0, 64.0]
    f += [64.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
    return [1, 1, 3, 0, 1, 1, 1], [rng.randrange(32768) for _ in range(8)], f


def edges(rng):
    """The dot's order decides a branch: an offset whose square lands on a float's rounding edge across a power of 2
    (found by search) against a limit on that edge, and a line that cancels, the last term alone deciding the side."""
    r = [rng.randrange(32768) for _ in range(8)]
    k = rng.randrange(3)
    if k < 2:
        o = [(2047.9998779296875, 0.8660245537757874, 0.0012074464466422796),
             (2047.999755859375, 1.118033528327942, 0.0009849858470261097)][k]
        f = [1.0, 1.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 10.0, 0.0, 9000.0, 0.0, 0.0, 50.0, 0.0, 0.0, 50.0]
        return [1, 1, 3, 0, 1, 1, 1], r, f + list(o) + [0.0, 0.0, 0.0, 0.0, 4194304.0]
    f = [1.0, 1.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, -1.0, rf(-1e-20), 10.0, 0.0, 1024.0, 1024.0, 1024.0, 50.0, 0.0, 0.0, 50.0]
    return [1, 1, 3, 0, 1, 1, 1], r, f + [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, -1.0]


def case(rng):
    if rng.random() < 0.02:
        return on_target(rng)
    if rng.random() < 0.02:
        return edges(rng)
    inp = [int(rng.random() < 0.95), int(rng.random() < 0.95), rng.choice([0, 0xb, 0xb, 3]), rng.choice([0, 0, 1, 2]),
           int(rng.random() < 0.5), rng.choice([0, 1, 1]), rng.choice([0, 1, 1, 1])]
    place = [rng.uniform(-2000, 2000) for _ in range(3)]
    muzzle = [x + rng.uniform(-5, 5) for x in place]
    target = [x + rng.uniform(-400, 400) for x in place]
    d = [t - m for t, m in zip(target, muzzle)]
    n = math.sqrt(sum(x * x for x in d)) or 1
    line = [x / n for x in d]
    if rng.random() < 0.15:
        line = [rng.choice([1.0, 0.0, -1.0]) for _ in range(3)]
    delta = [t - p for t, p in zip(target, place)]
    f = [rng.choice([1.2, 1.0, 0.6, 0.0]), rng.choice([0.0, 1.0, 0.5, rng.uniform(0, 3)]), rng.choice([0.5, 1.0, rng.uniform(0, 2)])]
    f += place + muzzle + line + [rng.uniform(0, 900), rng.choice([1 / 30, 0.1, 0.5, 2.0, 60.0])] + target + delta
    f += [rng.choice([-1.0, -1.0, n, rng.uniform(0, 300)])]
    big = rng.random() < 0.3
    f += [rng.uniform(-500, 500) if big else rng.uniform(-20, 20) for _ in range(3)]
    f += [rng.uniform(-10, 10) for _ in range(3)] + [rng.uniform(0, 900)]
    f += [rng.choice([-1.0, 0.0, rng.uniform(0, 400), 1e9])]
    rands = [rng.choice([rng.randrange(32768), 0, 32767, 16383]) for _ in range(8)]
    return inp, rands, [rf(x) for x in f]


def main():
    p = test_pm_zar.load_pm_probe()
    P = ctypes.c_void_p
    p.probe_pm_ai_aim.argtypes = [P, P, P, P, ctypes.c_int]
    rng = random.Random(1)
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 20000
    emu = Exe()
    out = (ctypes.c_uint32 * 48)()
    tracks = leads = active = repick = 0
    for k in range(n):
        inp, rands, f = case(rng)
        a = emu.run(inp, rands, f)
        probe_in = list(inp)
        m = p.probe_pm_ai_aim((ctypes.c_int32 * 7)(*probe_in), (ctypes.c_int32 * 8)(*rands), (ctypes.c_float * 29)(*f), out, 48)
        b = list(out[:m])
        if a != b:
            print(f'case {k}: exe {[hex(x) for x in a]}\n        port {[hex(x) for x in b]}\n  in {inp} {rands}\n  {f}')
            return 1
        tracks += inp[5] == 0
        leads += inp[5] == 1
        active += a[-4]
        repick += a[-1] > 0 and inp[5] == 1
    print(f'ok: {n} cases, {tracks} tracks, {leads} leads, {active} active, {repick} re-picks; re-pick branches {len(emu.reached)} of 3: identical')
    pm_floors.floors(cases=(n, 5000), tracks=(tracks, 1000), leads=(leads, 1000), active=(active, 2000), repick=(repick, 200))
    missed = {IN_FRONT, CARRIED, ON_TARGET} - emu.reached
    if missed:
        raise SystemExit(f'branches never reached: {[hex(x) for x in missed]}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
