"""PM's scripted steps' states against the exe at 53 bits, exact (mech3/zai_brain_pm's OrderStepPmHooks,
tests/probe_pm_ai_brain.cpp): speed 0x4d48b0, wait_interval 0x4d4a20, wait_time 0x4d4b80, the valve, shunt and
valve union steps' 0x4d6ec0 and valve_assign's 0x4d7010. The way finder (0x4d3120), the valve's set (0x4e40d0) and
the next step (0x4f9f00) stand in; the brain's +0x10c and the again flag (+0x158, set by 0x4fbe60) are compared. The
waits' ctors (0x4d4970, 0x4d4ad0, MW3's) are checked for what the port's keeps: the start the game time, one stop.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_ai_steps.py

BREAK=1 makes the port's wait_interval go at the sum itself (>=): it must fail.
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

NOW = 0x62da90
RUNS = {0: 0x4d48b0, 1: 0x4d4a20, 2: 0x4d4b80, 3: 0x4d6ec0, 4: 0x4d7010}
NAMES = {0: 'speed', 1: 'wait_interval', 2: 'wait_time', 3: 'pass', 4: 'assign'}
STEP, NEXT = 0xc5, 0xb7


def fb(x):
    """A double as a float's bits, rounded; past the float's range infinite (as the x87's store)."""
    try:
        return struct.unpack('<I', struct.pack('<f', x))[0]
    except OverflowError:
        return 0x7f800000 if x > 0 else 0xff800000


def bf(u):
    return struct.unpack('<f', struct.pack('<I', u))[0]


class Exe:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        self.st, self.order, self.brain, self.ctl = z.alloc(0x20), z.alloc(0x20), z.alloc(0x400), z.alloc(0x40)
        self.w(self.order + 8, self.brain)
        # The controls' first word is both the offset table (+4 is 0) and the vtable: vt+0x14 the stop.
        self.w(self.brain + 0x100, self.ctl), self.w(self.ctl + 4, 0)
        self.w(self.ctl + 0x14, self.ret(lambda u: self.put(73), 0))
        z.native_stub(0x4f9f00, lambda u: self.put(64, u.arg(0)) if u.ecx == self.order else 1 / 0, 4)
        z.native_stub(0x4d3120, lambda u: (self.put(71, u.arg(0)), self.next_at)[1] if u.ecx == self.brain + 0x1f8 else 1 / 0, 4)
        z.native_stub(0x4e40d0, lambda u: (self.put(72, u.ecx), self.put(72, u.arg(0)))[0], 4)

    def w(self, a, v):
        self.uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def r(self, a):
        return struct.unpack('<I', bytes(self.uc.mem_read(a, 4)))[0]

    def ret(self, fn, pop=0):
        t = self.z.alloc(4)
        self.uc.mem_write(t, b'\xc3')
        self.z.native_stub(t, fn, pop)
        return t

    def put(self, k, v=0):
        self.log += [k, v]
        return 0

    def run(self, kind, inp):
        """inp: value (or the valve), its value, start, now, the way finder's answer."""
        self.log, self.next_at = [], inp[4]
        step = self.z.alloc(0x20)
        self.w(step + 0xc, NEXT), self.w(step + 0x10, inp[0]), self.w(step + 0x14, inp[1])
        self.w(self.st + 4, self.order), self.w(self.st + 8, step), self.w(self.st + 0xc, inp[2])
        self.w(NOW, inp[3])
        self.w(self.brain + 0x10c, 0), self.uc.mem_write(self.brain + 0x158, b'\0')
        self.z.call(RUNS[kind], ecx=self.st, args=(fb(0.05),))
        log = [STEP if k == 71 and n % 2 and v == step else v for n, (k, v) in enumerate(zip([0] + self.log, self.log))]
        return log + [self.uc.mem_read(self.brain + 0x158, 1)[0], self.r(self.brain + 0x10c)]

    def ctor(self, addr, now):
        self.log = []
        self.w(NOW, fb(now))
        st, step = self.z.alloc(0x20), self.z.alloc(0x20)
        self.z.call(addr, ecx=st, args=(step, self.order))  # the behaviour, then the order
        return self.log, bf(self.r(st + 0xc)), self.r(st + 4) == self.order, self.r(st + 8) == step


def floats(r):
    return r.choice([0.0, -0.0, 0.1, 0.2, 0.3, 1.0, 2.5, 5.0, 30.0, 100.0, 1234.5, 1e-30, 1e30, 3.4028235e38,
                     float('inf'), float('nan'), r.uniform(-50, 50), r.uniform(0, 2000)])


def case(r, kind):
    start, value = bf(fb(floats(r))), bf(fb(floats(r)))
    pick = r.random()
    if kind == 1 and pick < 0.5:  # around the sum itself, at 53 bits and as floats
        s = value + start
        now = r.choice([s, s * (1 + 1e-7), s * (1 - 1e-7), s + 1e-6])
    elif kind == 2 and pick < 0.5:
        now = r.choice([value, value * (1 + 1e-7), value * (1 - 1e-7)])
    else:
        now = floats(r)
    nb, vb = fb(now), fb(value)
    if kind == 4:
        vb = r.choice([0x7700, 0x7704])  # the valve
    return [vb, r.choice([0, 1, 7, -1]) & 0xffffffff, fb(start), nb, r.choice([0, 0, 0x3a3a])]


def main():
    brk = int(os.environ.get('BREAK', '0'))
    lib = test_pm_zar.load_pm_probe()
    fn = lib.probe_pm_ai_order_step
    fn.argtypes = [ctypes.c_int32, ctypes.POINTER(ctypes.c_uint32), ctypes.POINTER(ctypes.c_uint32)]
    x = Exe()
    r = random.Random(53)
    out = (ctypes.c_uint32 * 64)()
    bad = 0
    went = {k: [0, 0] for k in RUNS}
    for i in range(20000):
        kind = i % 5
        inp = case(r, kind)
        a = x.run(kind, inp)
        n = fn(kind, (ctypes.c_uint32 * 5)(*inp), out)
        b = list(out[:n])
        if brk == 1 and kind == 1 and bf(inp[3]) == bf(inp[0]) + bf(inp[2]) and 64 not in b[:-2:2]:
            b = b[:-2] + [64, NEXT] + b[-2:]  # the port going at the sum itself
        went[kind][64 in a[:-2:2]] += 1
        if a != b:
            print(f'{NAMES[kind]} {[hex(u) for u in inp]} ({[bf(u) for u in inp[:4]]})\n  exe  {a}\n  port {b}')
            bad += 1
            if bad >= 6:
                break
    print(f'steps: 20000 cases, {bad} differ; went on / stayed: ' + ', '.join(f'{NAMES[k]} {went[k][1]}/{went[k][0]}' for k in RUNS))
    for addr in (0x4d4970, 0x4d4ad0):
        for now in (0.0, 12.5, 1234.5):
            log, start, o, s = x.ctor(addr, now)
            if log != [73, 0] or start != now or not o or not s:
                print(f'ctor {addr:#x} at {now}: calls {log}, start {start}, order {o}, step {s}')
                bad += 1
    print(f'wait ctors: {bad and "see above" or "the start the game time, the pilot stopped once"}')
    pm_floors.floors(**{f'{NAMES[k]}_on': (went[k][1], 50) for k in RUNS}, waits_stay=(went[1][0] + went[2][0], 50))
    return bad


if __name__ == '__main__':
    sys.exit(1 if main() else 0)
