"""The pocket commander's orders to the lancemates picked (docs/ai.md 17.6a)
against the original x86: 0x468f00 (on each one's contact on the player) and
0x468fa0 (on a contact made from each on the target, 0x4e6ee0, with the team
change 0x468ca0 first) over the panel's units (0x468c00 / 0x468e20).

    cmake --build build && .venv/bin/python tests/differential/test_pocket_orders.py

The world is made up: the panel (+0x70 the count, +0x53c the list), four
units (+0x4b0 bit 1 picked; vt+0xb4 -> vt+0x2c -> vt+0x40 their mech) or
null entries, the mechs (+0x98 a contact on the player or none, +0xc4 /
+0xc8 their allies and enemies, vt+0x140 -> vt+0x54 a brain or none), the
target (+0xc0..+0xc8 its teams), [0x605f58] the target or not, the places
(0x5a85e0); the brain's order wrappers stubbed, answering from a list. Each
case logs every order given (the order, the unit, the contact, a made one's
fields), the answer and the target's teams; the port's (mech3/lance_orders,
tests/probe_pocket.cpp) must match.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

os.environ.setdefault('MW3_X87', '24')
sys.path.insert(0, str(Path(__file__).parent))
from original import ROOT, Original  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
P = ctypes.c_void_p
probe.probe_pocket_orders.argtypes = [P, P, P, P, ctypes.c_int]
U = ctypes.c_uint32
WRAP = {0: 0x4ea000, 1: 0x4ea070, 3: 0x4ea0b0, 4: 0x4ea0f0, 5: 0x4ea120, 6: 0x4ea150, 7: 0x4ea180, 8: 0x4ea040,
        9: 0x4ea1c0, 10: 0x4ea200, 11: 0x4ea230, 12: 0x4ea180}
NUM = {a: o for o, a in WRAP.items() if o != 12}
ON_TARGET = (3, 4, 5, 6, 10, 11)
COVER = {0x468c8e: 'player contact: taken', 0x468c97: 'player contact: refused', 0x468ee6: 'target: taken',
         0x468ef3: 'target: refused', 0x468dc0: 'a team changed', 0x468f91: 'answer 0', 0x469045: 'answer 2 or 0'}
REACHED = set()


class Emulated:
    def __init__(self):
        cpu = self.cpu = Original()
        cpu.fpu_control(0x007f)
        self.mem = cpu.alloc(0x8000)
        cpu.write(self.mem, bytes(0x8000))
        a = iter(range(self.mem, self.mem + 0x8000, 0x200))
        self.panel, self.list = next(a), next(a)
        self.units = [next(a) for _ in range(4)]
        self.mechs = [next(a) for _ in range(4)]
        self.target = next(a)
        self.pcs = [next(a) for _ in range(4)]
        self.brains = [next(a) for _ in range(4)]
        bvb = next(a)
        cpu.u32(self.panel + 0x53c, self.list)
        stub = self.stub
        for i, (u, m) in enumerate(zip(self.units, self.mechs)):
            uvt, x, xvt, y, yvt = next(a), next(a), next(a), next(a), next(a)
            cpu.u32(u, uvt)
            cpu.u32(uvt + 0xb4, stub(lambda c, x=x: x))
            cpu.u32(x, xvt)
            cpu.u32(xvt + 0x2c, stub(lambda c, y=y: y))
            cpu.u32(y, yvt)
            cpu.u32(yvt + 0x40, stub(lambda c, m=m: m))
            mvt, ic, icv = next(a), next(a), next(a)
            cpu.u32(m, mvt)
            for slot in (0x24, 0x2c, 0x50):
                cpu.u32(mvt + slot, stub(lambda c: c.ecx))
            cpu.u32(mvt + 0x140, stub(lambda c, ic=ic: ic))
            cpu.u32(ic, icv)
            cpu.u32(icv + 0x54, stub(lambda c, i=i: self.brains[i] if self.has_brain[i] else 0))
        tvt = next(a)
        cpu.u32(self.target, tvt)
        for slot in (0x24, 0x2c, 0x50):
            cpu.u32(tvt + slot, stub(lambda c: c.ecx))
        cpu.u32(bvb, (-0xe0) & 0xffffffff)  # the member pointer's this: the brain itself
        for b in self.brains:
            cpu.u32(b + 0xe0, bvb)
        cpu.stub_function(0x5a85e0, lambda c: c.ecx + 0x100)  # each object's place at its +0x100
        for addr, order in NUM.items():
            cpu.stub_function(addr, lambda c, order=order: self.order(c, order), 4)
        for addr in COVER:
            cpu.uc.hook_add(UC_HOOK_CODE, lambda uc, ad, sz, u: REACHED.add(ad), begin=addr, end=addr)

    def stub(self, fn, pop=0):
        t = self.cpu.alloc(4)
        self.cpu.write(t, b'\xc3')
        self.cpu.stub_function(t, fn, pop)
        return t

    def handles(self):
        return self.mechs + [self.target]

    def index(self, p):
        h = self.handles()
        return h.index(p) + 1 if p in h else 0

    def r32(self, a):
        return struct.unpack('<I', bytes(self.cpu.read(a, 4)))[0]

    def order(self, c, order):
        arg = c.arg(0)
        self.log += [0x1a, order, self.brains.index(c.ecx) if c.ecx in self.brains else 99]
        if arg in self.pcs:
            self.log.append(0x11 + self.pcs.index(arg))
        else:
            w = struct.unpack('<8I', bytes(self.cpu.read(arg, 32)))
            self.log += [0, self.index(w[0]), self.index(w[1]), self.index(w[2]), w[3], w[4], w[5], w[6]]
        r = self.answers[self.nanswer & 7]
        self.nanswer += 1
        return r

    def run(self, inp, pos):
        cpu = self.cpu
        self.log, self.nanswer = [], 0
        order, on, count = inp[0], inp[1], inp[2]
        for i, o in enumerate(self.handles()):
            cpu.write(o + 0x100, struct.pack('<3f', *pos[3 * i:3 * i + 3]))
        cpu.u32(self.panel + 0x70, count)
        self.has_brain = [inp[5 + 4 * i] for i in range(4)]
        for i in range(4):
            u, m = self.units[i], self.mechs[i]
            cpu.u32(self.list + 4 * i, u if inp[3 + 4 * i] else 0)
            cpu.u32(u + 0x4b0, (2 if inp[4 + 4 * i] else 0) | 0x5)
            cpu.u32(m + 0x98, self.pcs[i] if inp[6 + 4 * i] else 0)
            cpu.u32(m + 0xc4, inp[20 + 2 * i] & 0xffffffff)
            cpu.u32(m + 0xc8, inp[21 + 2 * i] & 0xffffffff)
        cpu.u32(0x605f58, self.target if inp[19] else 0x1234)
        for k, off in enumerate((0xc0, 0xc4, 0xc8)):
            cpu.u32(self.target + off, inp[28 + k] & 0xffffffff)
        self.answers = inp[31:39]
        if on:
            cpu.call(0x468fa0, ecx=self.panel, stack_args=(WRAP[order], 0, 0, self.target))
        else:
            cpu.call(0x468f00, ecx=self.panel, stack_args=(WRAP[order], 0, 0))
        self.log += [0x2b, cpu.uc.reg_read(UC_X86_REG_EAX)]
        self.log += [self.r32(self.target + off) for off in (0xc0, 0xc4, 0xc8)]
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
    pos = [rng.uniform(-500, 500) for _ in range(15)]
    return inp, [struct.unpack('<f', struct.pack('<f', x))[0] for x in pos]


def main():
    rng = random.Random(int(os.environ.get('SEED', '1')))
    n = int(os.environ.get('POCKET_ORDER_CASES', '3000'))
    emu = Emulated()
    handles = (U * 5)(*emu.handles())
    out = (U * 128)()
    given = 0
    for k in range(n):
        inp, pos = case(rng)
        a = emu.run(inp, pos)
        sin = [x - (1 << 32) if x >= 1 << 31 else x for x in inp]
        m = probe.probe_pocket_orders((ctypes.c_int32 * len(sin))(*sin), handles, (ctypes.c_float * 15)(*pos), out, 128)
        b = list(out[:m])
        if a != b:
            print(f'case {k}: exe {a}\n        port {b}\n  in {inp}')
            sys.stdout.flush(); os._exit(1)
        given += a.count(0x1a)
    never = [COVER[x] for x in COVER if x not in REACHED]
    if never:
        print(f'branches never reached: {never}')
        sys.stdout.flush(); os._exit(1)
    print(f'ok: {n} cases, {given} orders given, identical; {len(COVER)} branches reached')
    sys.stdout.flush(); os._exit(0)


if __name__ == '__main__':
    main()
