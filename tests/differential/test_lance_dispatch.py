"""The player's lance orders' dispatcher (docs/ai.md 17.6) against the
original x86: 0x469940 (ECX the controls, EDX the order) outside the command
mode, its 0x469400 (the selected lancemate's contact on the player) and
0x469650 (a contact from the lancemate on the player's target, 0x4e6ee0)
over 0x4d7240, 0x4e6fe0 and 0x4e6ff0, the brain's order wrappers stubbed.

    cmake --build build && .venv/bin/python tests/differential/test_lance_dispatch.py

The world is made up: the controls (their vt+0x08: not in the command mode;
their owner the player through the virtual base), the player (+0x6c, +0x70,
+0x78), three contacts on the player, two would-be lancemates or the target
(or none), the two mates (+0xbc, +0x98, vt+0x140 -> vt+0x54 their brain or
none), the four mechs answering vt+0x24 / +0x2c / +0x50 with themselves,
their places (0x5a85e0). Each case logs every order given (the order, the
brain, the contact, a made one's fields) and the controls returned; the
port's (mech3/lance_orders on real Mechs, tests/probe_lance.cpp) must match.
Order 2 (the command mode, item L4c) is left out.
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
probe.probe_lance_dispatch.argtypes = [P, P, P, P, ctypes.c_int]
U = ctypes.c_uint32
WRAP = {0x4ea000: 0, 0x4ea070: 1, 0x4ea0b0: 3, 0x4ea0f0: 4, 0x4ea120: 5, 0x4ea150: 6, 0x4ea180: 7,
        0x4ea040: 8, 0x4ea1c0: 9, 0x4ea200: 10, 0x4ea230: 11}
COVER = {0x4695fd: 'a lancemate', 0x46963f: 'its order (on the player)', 0x469866: 'the target path',
         0x4698cc: 'a contact made'}
REACHED = set()


def f2i(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


class Emulated:
    def __init__(self):
        cpu = self.cpu = Original()
        cpu.fpu_control(0x007f)
        self.mem = cpu.alloc(0x4000)
        cpu.write(self.mem, bytes(0x4000))
        a = iter(range(self.mem, self.mem + 0x4000, 0x200))
        self.ctl, self.vbt, cvt = next(a), next(a), next(a)
        self.obj = [next(a) for _ in range(4)]  # the player, mate 1, mate 2, the target
        self.contacts = [next(a) for _ in range(3)]
        self.pcs = [next(a) for _ in range(2)]  # each mate's contact on the player
        self.brains = [next(a) for _ in range(2)]
        bvb = next(a)
        cpu.u32(self.ctl, cvt)
        cpu.u32(self.ctl + 4, self.vbt)
        cpu.u32(self.vbt + 4, 0x20)  # the IControls at +0x24, its owner at +0x34
        cpu.u32(self.ctl + 0x34, self.obj[0])
        stub = self.stub
        cpu.u32(cvt + 0x8, stub(lambda c: 0))  # not in the command mode
        self.mctl = []
        for i, o in enumerate(self.obj):
            vt = next(a)
            cpu.u32(o, vt)
            for slot in (0x24, 0x2c, 0x50, 0x40):
                cpu.u32(vt + slot, stub(lambda c: c.ecx))
            ic, icv = next(a), next(a)
            cpu.u32(ic, icv)
            cpu.u32(icv + 0x54, stub(lambda c, i=i: self.brain_of(i)))
            cpu.u32(vt + 0x140, stub(lambda c, ic=ic: ic))
        cpu.u32(bvb, (-0xe0) & 0xffffffff)  # the member pointer's this: the brain itself
        for b in self.brains:
            cpu.u32(b + 0xe0, bvb)
        cpu.stub_function(0x5a85e0, lambda c: self.obj_pos(c.ecx))
        for addr, order in WRAP.items():
            cpu.stub_function(addr, lambda c, order=order: self.order(c, order), 4)
        cpu.u32(0x605f58, 0xdead0000)  # 0x468ca0's: the command mode's last unit (L4c), none of these
        for addr in COVER:
            cpu.uc.hook_add(UC_HOOK_CODE, lambda uc, ad, sz, u: REACHED.add(ad), begin=addr, end=addr)

    def stub(self, fn, pop=0):
        t = self.cpu.alloc(4)
        self.cpu.write(t, b'\xc3')
        self.cpu.stub_function(t, fn, pop)
        return t

    def brain_of(self, i):
        return self.brains[i - 1] if i in (1, 2) and self.has_brain[i - 1] else 0

    def obj_pos(self, o):
        return o + 0x100  # each object's place at its +0x100

    def index(self, p):
        return self.obj.index(p) + 1 if p in self.obj else 0

    def order(self, c, order):
        arg = c.arg(0)
        self.log += [0x1a, order, self.brains.index(c.ecx) + 1 if c.ecx in self.brains else 0]
        if arg in self.contacts:
            self.log.append(self.contacts.index(arg) + 1)
        elif arg in self.pcs:
            self.log.append(0x11 + self.pcs.index(arg))
        else:
            w = struct.unpack('<8I', bytes(self.cpu.read(arg, 32)))
            self.log += [0, self.index(w[0]), self.index(w[1]), self.index(w[2]), w[3], w[4], w[5], w[6]]
        return 1

    def run(self, inp, pos):
        cpu = self.cpu
        self.log = []
        for i, o in enumerate(self.obj):
            cpu.write(o + 0x100, struct.pack('<3f', *pos[3 * i:3 * i + 3]))
        for k in range(3):
            cpu.u32(self.contacts[k] + 4, self.obj[inp[4 + k] - 1] if inp[4 + k] else 0)
        con = lambda k: self.contacts[k - 1] if k else 0  # noqa: E731
        p = self.obj[0]
        cpu.u32(p + 0x78, con(inp[1]))
        cpu.u32(p + 0x70, con(inp[2]))
        cpu.u32(p + 0x6c, con(inp[3]))
        self.has_brain = [inp[8], inp[11]]
        for j in range(2):
            m = self.obj[1 + j]
            cpu.write(m + 0xbc, bytes([inp[7 + 3 * j]]))
            cpu.u32(m + 0x98, self.pcs[j] if inp[9 + 3 * j] else 0)
        cpu.call(0x469940, ecx=self.ctl, edx=inp[0], stack_args=(1,))
        r = cpu.uc.reg_read(UC_X86_REG_EAX)
        self.log += [0x2b, 1 if r == self.ctl else 0]
        return self.log


def case(rng):
    order = rng.choice([0, 1, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13])
    inp = [order, rng.choice([0, 0, 1, 2, 3]), rng.choice([0, 1, 2, 3]), rng.choice([0, 1, 2, 3])]
    inp += [rng.choice([0, 1, 2, 3, 4, 2]) for _ in range(3)]
    for _ in range(2):
        inp += [rng.choice([0x10, 0x10, 0, 0x14, 0xef]), int(rng.random() < 0.8), int(rng.random() < 0.85)]
    pos = [rng.uniform(-500, 500) for _ in range(12)]
    return inp, [struct.unpack('<f', struct.pack('<f', x))[0] for x in pos]


def main():
    rng = random.Random(int(os.environ.get('SEED', '1')))
    n = int(os.environ.get('DISPATCH_CASES', '3000'))
    emu = Emulated()
    handles = (U * 4)(*emu.obj)
    out = (U * 64)()
    given = 0
    for k in range(n):
        inp, pos = case(rng)
        a = emu.run(inp, pos)
        m = probe.probe_lance_dispatch((ctypes.c_int32 * len(inp))(*inp), (ctypes.c_float * 12)(*pos), handles, out, 64)
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
