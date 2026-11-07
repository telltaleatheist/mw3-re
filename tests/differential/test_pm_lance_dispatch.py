"""The player's lance orders against PM's exe: test_lance_dispatch moved. The dispatcher 0x46b4d0 outside the
command mode, 0x46af20 (on the lancemate's contact on the player), 0x46b1b0 (a contact from the lancemate on
the player's target, 0x4f8c70), the saved target's 0x4e76e0, the order's object 0x46a750 with the lancemate's
own marker, at 53 bits. The orders' wrappers stand in. Each case logs every order, marker move and show, the
controls returned and the markers' team words; the port's (mech3/lance_orders, tests/probe_lance.cpp) must match.
Order 2 (the command mode) isn't here.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_lance_dispatch.py
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

DISPATCH, PLACE, SHOW, MARKER = 0x46b4d0, 0x5cb260, 0x4fca50, 0x629a2c
WRAP = {0x4fb420: 0, 0x4fb4a0: 1, 0x4fb4f0: 3, 0x4fb570: 4, 0x4fb5d0: 5, 0x4fb630: 6, 0x4fb690: 7, 0x4fb7f0: 8,
        0x4fb6e0: 9, 0x4fb730: 10, 0x4fb790: 11}
U = ctypes.c_uint32


class Exe:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.w = lambda a, v: z.cpu._uc.mem_write(a, struct.pack('<I', v & 0xffffffff))
        a = z.alloc(0x6000)
        z.cpu._uc.mem_write(a, bytes(0x6000))
        slots = iter(range(a, a + 0x6000, 0x200))
        self.ctl, vbt, cvt = next(slots), next(slots), next(slots)
        self.obj = [next(slots) for _ in range(6)]  # the player, mate 1, mate 2, the target, the mates' own markers
        self.contacts = [next(slots) for _ in range(3)]
        self.pcs = [next(slots) for _ in range(2)]
        self.brains = [next(slots) for _ in range(2)]
        self.nodes = [next(slots) for _ in range(2)]
        self.w(self.ctl, cvt)
        self.w(self.ctl + 4, vbt)
        self.w(vbt + 4, 0x14)  # the owner at +0x20 + 0x14
        self.w(self.ctl + 0x34, self.obj[0])
        self.w(cvt + 8, self.ret(lambda c: 0))  # not in the command mode
        for i, o in enumerate(self.obj[:4]):
            vt = next(slots)
            self.w(o, vt)
            for slot in (0x24, 0x28, 0x2c, 0x30, 0x48, 0x50, 0x58):
                if i == 3 and slot == 0x58:
                    self.w(vt + slot, self.ret(lambda c: c.ecx if self.target_mech else 0))
                else:
                    self.w(vt + slot, self.ret(lambda c: c.ecx))
            ic, icv = next(slots), next(slots)
            self.w(ic, icv)
            self.w(icv + 0x58, self.ret(lambda c, i=i: self.brain_of(i)))
            self.w(vt + 0x14c, self.ret(lambda c, ic=ic: ic))
        for j, o in enumerate(self.obj[4:]):
            vt = next(slots)
            self.w(o, vt)
            self.w(o + 4, self.nodes[j])
            self.w(vt + 0x18, self.ret(self.placed, 4))
            for slot in (0x24, 0x28, 0x2c, 0x30, 0x48, 0x50, 0x58):
                self.w(vt + slot, self.ret(lambda c: c.ecx))
        bvb = next(slots)
        self.w(bvb, -0x100)  # the member pointer's this: the brain itself
        for b in self.brains:
            self.w(b + 0x100, bvb)
        z.native_stub(PLACE, lambda c: c.ecx + 0x100)
        z.native_stub(SHOW, self.shown)
        for addr, order in WRAP.items():
            z.native_stub(addr, lambda c, order=order: self.order(c, order), 4)

    def ret(self, fn, pop=0):
        code = self.z.alloc(4)
        self.z.cpu._uc.mem_write(code, b'\xc3')
        self.z.native_stub(code, fn, pop)
        return code

    def brain_of(self, i):
        return self.brains[i - 1] if i in (1, 2) and self.has_brain[i - 1] else 0

    def index(self, p):
        return self.obj.index(p) + 1 if p in self.obj else 0

    def placed(self, c):
        self.log += [0x51, self.index(c.ecx), self.index(c.arg(0) - 0x100)]
        return 0

    def shown(self, c):
        self.log += [0x52, self.nodes.index(c.arg(0)) + 5, c.arg(1)]
        return 0

    def order(self, c, order):
        arg = c.arg(0)
        self.log += [0x1a, order, self.brains.index(c.ecx) + 1 if c.ecx in self.brains else 0]
        if arg in self.contacts:
            self.log.append(self.contacts.index(arg) + 1)
        elif arg in self.pcs:
            self.log.append(0x11 + self.pcs.index(arg))
        else:
            words = struct.unpack('<8I', bytes(self.z.cpu._uc.mem_read(arg, 32)))
            self.log += [0, self.index(words[0]), self.index(words[1]), self.index(words[2]), words[3], words[4], words[5], words[6]]
        return 1

    def run(self, inp, pos):
        z, w = self.z, self.w
        self.log = []
        for i, o in enumerate(self.obj):
            z.cpu._uc.mem_write(o + 0x100, struct.pack('<3f', *pos[3 * i:3 * i + 3]))
        for k in range(3):
            w(self.contacts[k] + 4, self.obj[inp[4 + k] - 1] if inp[4 + k] else 0)
        con = lambda k: self.contacts[k - 1] if k else 0  # noqa: E731
        p = self.obj[0]
        w(p + 0x78, con(inp[1])), w(p + 0x70, con(inp[2])), w(p + 0x6c, con(inp[3]))
        self.has_brain = [inp[8], inp[11]]
        self.target_mech = inp[13]
        for j in range(2):
            m = self.obj[1 + j]
            z.cpu._uc.mem_write(m + 0xbc, bytes([inp[7 + 3 * j]]))
            w(m + 0x98, self.pcs[j] if inp[9 + 3 * j] else 0)
            w(self.brains[j] + 0x2cc, self.obj[4 + j] if inp[14 + j] else 0)
            w(self.pcs[j] + 4, self.obj[3] if inp[17] else self.obj[0])
            w(self.obj[4 + j] + 0xc0, 1), w(self.obj[4 + j] + 0xc4, 0), w(self.obj[4 + j] + 0xc8, 0xfffffffe)
        w(MARKER, self.obj[3] if inp[16] else 0xdead0000)
        r = z.call(DISPATCH, args=[self.ctl, inp[0], 1])
        self.log += [0x2b, 1 if r == self.ctl else 0]
        for j in range(2):
            if inp[14 + j]:
                self.log += [z.r32(self.obj[4 + j] + o) for o in (0xc0, 0xc4, 0xc8)]
        return self.log


def case(rng):
    inp = [rng.choice([0, 1, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13]), rng.choice([0, 0, 1, 2, 3]), rng.choice([0, 1, 2, 3]), rng.choice([0, 1, 2, 3])]
    inp += [rng.choice([0, 1, 2, 3, 4, 2]) for _ in range(3)]
    for _ in range(2):
        inp += [rng.choice([0x10, 0x10, 0, 0x14, 0xef]), int(rng.random() < 0.8), int(rng.random() < 0.85)]
    inp += [int(rng.random() < 0.7), int(rng.random() < 0.8), int(rng.random() < 0.8), int(rng.random() < 0.5), int(rng.random() < 0.3)]
    if inp[16] and rng.random() < 0.7:  # the player's target the marker, a lancemate selected
        inp[3], inp[1] = 1, 2
        inp[4], inp[5] = 4, rng.choice([2, 3])
    pos = [rng.uniform(-500, 500) for _ in range(18)]
    return inp, [struct.unpack('<f', struct.pack('<f', x))[0] for x in pos]


def main():
    p = test_pm_zar.load_pm_probe()
    P = ctypes.c_void_p
    p.probe_lance_dispatch.argtypes = [P, P, P, P, ctypes.c_int]
    rng = random.Random(1)
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    emu = Exe()
    handles = (U * 6)(*emu.obj)
    out = (U * 96)()
    given = moved = shown = changed = 0
    for k in range(n):
        inp, pos = case(rng)
        a = emu.run(inp, pos)
        m = p.probe_lance_dispatch((ctypes.c_int32 * len(inp))(*inp), (ctypes.c_float * 18)(*pos), handles, out, 96)
        b = list(out[:m])
        if a != b:
            print(f'case {k}: exe {[hex(x) for x in a]}\n        port {[hex(x) for x in b]}\n  in {inp}')
            return 1
        given += a.count(0x1a)
        moved += a.count(0x51)
        shown += a.count(0x52)
        changed += inp[0] == 10 and inp[16] and 0x1a in a
    print(f'ok: {n} cases, {given} orders, {moved} markers moved, {shown} shown or hidden, {changed} orders 10 on the marker: identical')
    pm_floors.floors(cases=(n, 1000), orders=(given, 500), moved=(moved, 50), shown=(shown, 200), changed=(changed, 10))
    return 0


if __name__ == '__main__':
    sys.exit(main())
