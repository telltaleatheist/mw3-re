"""The pocket commander's controls (docs/ai.md 17.6a) against the original
x86: the lance order 2 through the dispatcher 0x469940 into the mode (the
controls 0x4690d0 over the player's, built by the exe's 0x427320, their
SetOwner 0x46a440) and out again, its keys 0x46a860 and its update 0x46a360.

    cmake --build build && .venv/bin/python tests/differential/test_pocket.py

The world is made up: the player's mech (vt+0x50 itself, vt+0xa0 powered
down or not, +0x684 its pilot data), the view [0x5f4c8c] (+0x58 a camera,
+0x64 the viewed mech), the HUD (0x430aa0's, its +0x144c part); the panel
(0x468250, 0x468720, 0x469d00, 0x46b0a0, 0x469060), the view's pocket
0x4b5290, the camera 0x40ae70, the view state 0x426780, 0x4484b0,
0x59bb50, the player's keys 0x4274d0 and 0x40d760 stubbed and logged. Each
case: random inputs on the player's controls, the instance [0x6056ec] them
or another, the mech viewed or not, a HUD or not, powered down or not, the
mode's +0x154, a few keys; the log (the calls, the mode's fields after the
start, its axes after the update, the controls given back and their inputs)
must match the port's (mech3/mech_pocket, tests/probe_pocket.cpp).
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import IAT_FTOL, FTOL, ROOT, Original  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
P = ctypes.c_void_p
probe.probe_pocket_run.argtypes = [P, P, P, ctypes.c_int]
IAT_NEW, IAT_DELETE = 0x5bb4bc, 0x5bb4c8
INPUTS = [(0x3f, 'B'), (0x3e, 'B'), (0x44, 'f'), (0x5a, 'B'), (0x40, 'f'), (0x50, 'B'), (0x10, 'f'), (0x20, 'f'),
          (0x28, 'f'), (0x24, 'f'), (0x14, 'f'), (0x18, 'f'), (0x1c, 'f'), (0x34, 'B'), (0x3c, 'B')]
COVER = {0x46a3d9: 'update: the axes off', 0x469c80: 'the HUD flag on', 0x469ab2: 'the HUD flag off (leaving)'}
HUD_FLAG = {0x469c80: 1, 0x469ab2: 0, 0x4694ff: 0, 0x46977f: 0, 0x46ac8a: 0, 0x46ae29: 0}  # the +0x90c writes
REACHED = set()


def fb(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


class Emulated:
    def __init__(self):
        cpu = self.cpu = Original()
        cpu.import_code(IAT_FTOL, FTOL)
        cpu.fpu_control(0x007f)
        cpu.stub_import(IAT_NEW, lambda c: self.zalloc(c.arg(0)))
        cpu.stub_import(IAT_DELETE, lambda c: self.deleted.append(c.arg(0)) or 0)
        self.mech, mvt = self.zalloc(0x800), self.zalloc(0x200)
        cpu.u32(self.mech, mvt)
        cpu.u32(mvt + 0x50, self.stub(lambda c: c.ecx))
        cpu.u32(mvt + 0xa0, self.stub(lambda c: self.inp[4]))
        self.pilot = self.zalloc(0x40)
        cpu.u32(self.mech + 0x684, self.pilot)
        self.view, self.camera = self.zalloc(0x100), self.zalloc(0x20)
        cpu.u32(0x5f4c8c, self.view)
        self.hud = self.zalloc(0x3000)
        stub = cpu.stub_function
        stub(0x4b5290, lambda c: self.log(1, c.arg(0)), 4)
        stub(0x40ae70, lambda c: self.log(2, c.arg(0)), 4)
        stub(0x426780, lambda c: self.log(3, c.arg(0)), 4)
        stub(0x430aa0, lambda c: self.hud if self.inp[3] else 0)
        stub(0x59bb50, lambda c: self.count59() or 0, 4)
        stub(0x468250, lambda c: self.log(5) or c.ecx, 0x10)
        stub(0x468720, lambda c: self.log(6))
        stub(0x4484b0, lambda c: self.log(7, c.arg(0) & 0xff), 4)
        stub(0x469060, lambda c: self.log(8, c.arg(0)) or 1, 4)
        stub(0x4274d0, lambda c: self.log(9, c.arg(0)), 4)
        stub(0x469d00, lambda c: self.log(10), 4)
        stub(0x46b0a0, lambda c: self.log(11), 4)
        stub(0x40d760, lambda c: self.inp[0])
        cpu.u32(0x609980, 0)  # no network game
        for addr in COVER:
            cpu.uc.hook_add(UC_HOOK_CODE, lambda uc, ad, sz, u: REACHED.add(ad), begin=addr, end=addr)
        for addr, v in HUD_FLAG.items():  # the port's hud_mode
            cpu.uc.hook_add(UC_HOOK_CODE, lambda uc, ad, sz, u, v=v: self.log(4, v), begin=addr, end=addr)

    def zalloc(self, n):
        p = self.cpu.alloc(max(n, 4))
        self.cpu.write(p, bytes(max(n, 4)))
        return p

    def stub(self, fn, pop=0):
        t = self.zalloc(4)
        self.cpu.write(t, b'\xc3')
        self.cpu.stub_function(t, fn, pop)
        return t

    def log(self, *v):
        self.out += [x & 0xffffffff for x in v]
        return 0

    def count59(self):
        self.n59 += 1

    def inputs_of(self, c):
        out = []
        for off, k in INPUTS:
            if k == 'B':
                out.append(self.cpu.read(c + off, 1)[0])
            else:
                out.append(self.cpu.u32_read(c + off) if hasattr(self.cpu, 'u32_read') else
                           struct.unpack('<I', bytes(self.cpu.read(c + off, 4)))[0])
        return out

    def r32(self, a):
        return struct.unpack('<I', bytes(self.cpu.read(a, 4)))[0]

    def run(self, inp, vals):
        cpu = self.cpu
        self.inp, self.out, self.deleted, self.n59 = inp, [], [], 0
        cpu.u32(self.view + 0x58, self.camera if inp[1] >= 0 else 0)
        cpu.u32(self.camera + 4, inp[1] & 0xffffffff)
        cpu.u32(self.view + 0x64, self.mech if inp[2] else 0x1234)
        cpu.write(self.hud + 0x144c + 0x90c, b'\0')
        prev = self.zalloc(0x160)
        cpu.call(0x427320, ecx=prev, stack_args=(1, 1))  # most derived, as 0x427280 makes them
        for (off, k), v in zip(INPUTS, vals):
            cpu.write(prev + off, bytes([int(v)]) if k == 'B' else struct.pack('<f', v))
        cpu.u32(prev + 0x15c, self.mech)  # the IControls' owner
        other = self.zalloc(0x160)
        cpu.u32(0x6056ec, other if inp[5] else prev)
        idf = lambda p: 1 if p == prev else 2 if p == other else 3 if p else 0  # noqa: E731
        cpu.call(0x469940, ecx=prev, edx=2, stack_args=(1,))
        p = cpu.uc.reg_read(UC_X86_REG_EAX)
        ok = p not in (0, prev)
        self.out += [0x20, 1 if ok else 0]
        if ok:
            self.out += [self.r32(p + 0x150), idf(self.r32(p + 0x158)), idf(self.r32(p + 0x15c)), self.r32(p + 0x160),
                         self.r32(p + 0x178), self.r32(p + 0x17c), 1 if self.r32(p + 0x180) else 0,
                         int(self.r32(0x6056ec) == p), int(self.r32(0x605f64) == p)]
            self.out += self.inputs_of(p)
            cpu.u32(p + 0x154, inp[6])
            for cmd in inp[8:8 + inp[7]]:
                cpu.call(0x46a860, ecx=p, stack_args=(cmd,))
            self.out.append(0x21)
            cpu.call(0x46a360, ecx=p + 0x1c8, stack_args=(fb(1 / 30),))
            self.out.append(0x22)
            self.out += [self.r32(p + 0x4c), cpu.read(p + 0x3d, 1)[0], self.r32(p + 0x24), self.r32(p + 0x28),
                         cpu.read(p + 0x34, 1)[0]]
            cpu.call(0x469940, ecx=p, edx=2, stack_args=(1,))
            back = cpu.uc.reg_read(UC_X86_REG_EAX)
            self.out += [0x23, idf(back), idf(self.r32(0x6056ec)), 1 if self.r32(0x605f64) else 0,
                         int(self.r32(prev + 0x15c) == self.mech)]
            self.out += self.inputs_of(prev)
        return self.out


def case(rng):
    inp = [rng.choice([480, 600, 768]), rng.choice([-1, 0, 1, 2, 4]), int(rng.random() < 0.7), int(rng.random() < 0.8),
           int(rng.random() < 0.15), int(rng.random() < 0.3), rng.choice([0, 0, 1, 3])]
    keys = [rng.choice([0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x21, 0x23, 0x2a, 0x2b, 0x10]) for _ in range(rng.randrange(0, 5))]
    inp += [len(keys)] + keys
    vals = []
    for off, k in INPUTS:
        vals.append(float(rng.choice([0, 1])) if k == 'B' else struct.unpack('<f', struct.pack('<f', rng.uniform(-1, 1)))[0])
    return inp, vals


def main():
    rng = random.Random(int(os.environ.get('SEED', '1')))
    n = int(os.environ.get('POCKET_CASES', '400'))
    emu = Emulated()
    out = (ctypes.c_uint32 * 512)()
    for k in range(n):
        inp, vals = case(rng)
        a = emu.run(inp, vals)
        m = probe.probe_pocket_run((ctypes.c_int32 * len(inp))(*inp), (ctypes.c_float * 15)(*vals), out, 512)
        b = list(out[:m])
        if a != b:
            i = next((j for j, (x, y) in enumerate(zip(a, b)) if x != y), min(len(a), len(b)))
            print(f'case {k}: differ at {i}\n  exe  {a[max(0, i - 6):i + 8]}\n  port {b[max(0, i - 6):i + 8]}\n  in {inp}')
            sys.stdout.flush(); os._exit(1)
    never = [COVER[x] for x in COVER if x not in REACHED]
    if never:
        print(f'branches never reached: {never}')
        sys.stdout.flush(); os._exit(1)
    print(f'ok: {n} cases identical; {len(COVER)} branches reached')
    sys.stdout.flush(); os._exit(0)


if __name__ == '__main__':
    main()
