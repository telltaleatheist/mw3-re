"""The pocket commander's orders to the lancemates picked against PM's exe: test_pocket_orders moved. 0x46aa70
(on each one's contact on the player) and 0x46ab20 (on a contact made from each on the target, 0x4f8c70) over the
panel's units (0x46a8e0 / 0x46a9c0), each through the order's object 0x46a750 with the unit's own marker, at 53
bits. The orders' wrappers stand in, answering from a list. Each case logs every order given, marker move and
show, the answer, the target's teams and the markers'; the port's (mech3/lance_orders, tests/probe_pocket.cpp)
must match.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_pocket_orders.py
"""
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

FOLLOW, ON_TARGET_FN, PLACE, SHOW, MARKER = 0x46aa70, 0x46ab20, 0x5cb260, 0x4fca50, 0x629a2c
WRAP = {0: 0x4fb420, 1: 0x4fb4a0, 3: 0x4fb4f0, 4: 0x4fb570, 5: 0x4fb5d0, 6: 0x4fb630, 7: 0x4fb690, 8: 0x4fb7f0,
        9: 0x4fb6e0, 10: 0x4fb730, 11: 0x4fb790, 12: 0x4fb690}
NUM = {a: o for o, a in WRAP.items() if o != 12}
ON_TARGET = (3, 4, 5, 6, 10, 11)
U = ctypes.c_uint32


class Exe:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.w = lambda a, v: z.cpu._uc.mem_write(a, struct.pack('<I', v & 0xffffffff))
        a = z.alloc(0x10000)
        z.cpu._uc.mem_write(a, bytes(0x10000))
        slots = iter(range(a, a + 0x10000, 0x200))
        self.panel, self.list = next(slots), next(slots)
        self.units = [next(slots) for _ in range(4)]
        self.mechs = [next(slots) for _ in range(4)]
        self.target = next(slots)
        self.own = [next(slots) for _ in range(4)]
        self.nodes = [next(slots) for _ in range(4)]
        self.pcs = [next(slots) for _ in range(4)]
        self.brains = [next(slots) for _ in range(4)]
        self.w(self.panel + 0x544, self.list)
        for i, (u, m) in enumerate(zip(self.units, self.mechs)):
            uvt, x, xvt, mvt, ic, icv = (next(slots) for _ in range(6))
            self.w(u, uvt)
            self.w(uvt + 0xb4, self.ret(lambda c, x=x: x))
            self.w(x, xvt)
            self.w(xvt + 0x48, self.ret(lambda c, m=m: m))
            self.w(m, mvt)
            self.obj_slots(mvt)
            self.w(mvt + 0x14c, self.ret(lambda c, ic=ic: ic))
            self.w(ic, icv)
            self.w(icv + 0x58, self.ret(lambda c, i=i: self.brains[i] if self.has_brain[i] else 0))
        tvt = next(slots)
        self.w(self.target, tvt)
        self.obj_slots(tvt)
        for j, o in enumerate(self.own):
            vt = next(slots)
            self.w(o, vt)
            self.w(o + 4, self.nodes[j])
            self.w(vt + 0x18, self.ret(self.placed, 4))
            self.obj_slots(vt)
        bvb = next(slots)
        self.w(bvb, -0x100)  # the member pointer's this: the brain itself
        for b in self.brains:
            self.w(b + 0x100, bvb)
        z.native_stub(PLACE, lambda c: c.ecx + 0x100)
        z.native_stub(SHOW, self.shown)
        for addr, order in NUM.items():
            z.native_stub(addr, lambda c, order=order: self.order(c, order), 4)

    def obj_slots(self, vt):
        for slot in (0x24, 0x28, 0x2c, 0x30, 0x48, 0x50, 0x58):
            self.w(vt + slot, self.ret(lambda c: c.ecx))

    def ret(self, fn, pop=0):
        code = self.z.alloc(4)
        self.z.cpu._uc.mem_write(code, b'\xc3')
        self.z.native_stub(code, fn, pop)
        return code

    def handles(self):
        return self.mechs + [self.target] + self.own

    def index(self, p):
        h = self.handles()
        return h.index(p) + 1 if p in h else 0

    def placed(self, c):
        self.log += [0x51, self.index(c.ecx), self.index(c.arg(0) - 0x100)]
        return 0

    def shown(self, c):
        self.log += [0x52, self.nodes.index(c.arg(0)) + 6, c.arg(1)]
        return 0

    def order(self, c, order):
        arg = c.arg(0)
        self.log += [0x1a, order, self.brains.index(c.ecx) if c.ecx in self.brains else 99]
        if arg in self.pcs:
            self.log.append(0x11 + self.pcs.index(arg))
        else:
            w = struct.unpack('<8I', bytes(self.z.cpu._uc.mem_read(arg, 32)))
            self.log += [0, self.index(w[0]), self.index(w[1]), self.index(w[2]), w[3], w[4], w[5], w[6]]
        r = self.answers[self.nanswer & 7]
        self.nanswer += 1
        return r

    def run(self, inp, pos):
        z, w = self.z, self.w
        self.log, self.nanswer = [], 0
        order, on, count = inp[0], inp[1], inp[2]
        h = self.handles()
        for i, o in enumerate(h):
            z.cpu._uc.mem_write(o + 0x100, struct.pack('<3f', *pos[3 * i:3 * i + 3]))
        w(self.panel + 0x70, count)
        self.has_brain = [inp[5 + 4 * i] for i in range(4)]
        for i in range(4):
            u, m = self.units[i], self.mechs[i]
            w(self.list + 4 * i, u if inp[3 + 4 * i] else 0)
            w(u + 0x4b8, (2 if inp[4 + 4 * i] else 0) | 5)
            w(m + 0x98, self.pcs[i] if inp[6 + 4 * i] else 0)
            w(m + 0xc4, inp[20 + 2 * i]), w(m + 0xc8, inp[21 + 2 * i])
            w(self.brains[i] + 0x2cc, self.own[i] if inp[39 + i] else 0)
            w(self.pcs[i] + 4, h[inp[43 + i] - 1] if inp[43 + i] else 0)
            w(self.own[i] + 0xc0, 1), w(self.own[i] + 0xc4, 0), w(self.own[i] + 0xc8, 0xfffffffe)
        w(MARKER, self.target if inp[19] else 0x1234)
        for k, off in enumerate((0xc0, 0xc4, 0xc8)):
            w(self.target + off, inp[28 + k])
        self.answers = inp[31:39]
        if on:
            r = z.call(ON_TARGET_FN, self.panel, [WRAP[order], 0, 0, self.target])
        else:
            r = z.call(FOLLOW, self.panel, [WRAP[order], 0, 0])
        self.log += [0x2b, r]
        self.log += [z.r32(self.target + off) for off in (0xc0, 0xc4, 0xc8)]
        for i in range(4):
            if inp[39 + i]:
                self.log += [z.r32(self.own[i] + off) for off in (0xc0, 0xc4, 0xc8)]
        return self.log


def mask(rng):
    return rng.choice([0, 1, 2, 4, 6, 0x10, 0x8000_0000, 0x7, rng.getrandbits(32)])


def case(rng):
    order = rng.choice([0, 1, 3, 4, 5, 6, 7, 8, 9, 10, 10, 11, 11, 12])
    inp = [order, int(order in ON_TARGET), rng.choice([0, 1, 2, 3, 4, 4, 4])]
    for _ in range(4):
        inp += [int(rng.random() < 0.9), int(rng.random() < 0.7), int(rng.random() < 0.85), int(rng.random() < 0.8)]
    inp.append(int(rng.random() < 0.6))
    for _ in range(4):
        inp += [mask(rng), mask(rng)]
    inp += [mask(rng), mask(rng), mask(rng)]
    inp += [rng.choice([0, 1, 1]) for _ in range(8)]
    inp += [int(rng.random() < 0.8) for _ in range(4)]
    inp += [rng.choice([0, 1, 2, 5, 5]) for _ in range(4)]
    pos = [rng.uniform(-500, 500) for _ in range(27)]
    return inp, [struct.unpack('<f', struct.pack('<f', x))[0] for x in pos]


def main():
    p = test_pm_zar.load_pm_probe()
    P = ctypes.c_void_p
    p.probe_pocket_orders.argtypes = [P, P, P, P, ctypes.c_int]
    rng = random.Random(1)
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    emu = Exe()
    handles = (U * 9)(*emu.handles())
    out = (U * 160)()
    given = moved = shown = taken = refused = none = changed = 0
    for k in range(n):
        inp, pos = case(rng)
        a = emu.run(inp, pos)
        sin = [x - (1 << 32) if x >= 1 << 31 else x for x in inp]
        m = p.probe_pocket_orders((ctypes.c_int32 * len(sin))(*sin), handles, (ctypes.c_float * 27)(*pos), out, 160)
        b = list(out[:m])
        if a != b:
            print(f'case {k}: exe {[hex(x) for x in a]}\n        port {[hex(x) for x in b]}\n  in {inp}')
            return 1
        given += a.count(0x1a)
        moved += a.count(0x51)
        shown += a.count(0x52)
        r = a[a.index(0x2b) + 1]
        taken += r == 1
        refused += r == 0
        none += r == 2
        changed += inp[0] == 10 and inp[19] and 0x1a in a
    print(f'ok: {n} cases, {given} orders, {moved} markers moved, {shown} shown or hidden, answers {taken}/{refused}/{none}, '
          f'{changed} orders 10 on the marker: identical')
    pm_floors.floors(cases=(n, 1000), orders=(given, 1500), moved=(moved, 50), shown=(shown, 200), taken=(taken, 300),
                     refused=(refused, 100), none=(none, 100), changed=(changed, 10))
    return 0


if __name__ == '__main__':
    sys.exit(main())
