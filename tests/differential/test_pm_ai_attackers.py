"""PM's list of who hit a pilot against the exe: 0x4b2580, 0x4b25c0, 0x4b2740, 0x4b27d0, 0x4b2820,
0x4b2890 on random calls and clocks at 53 bits, the list compared to the bit after each.

    cmake --build build --target pmprobe && .venv/bin/python tests/differential/test_pm_ai_attackers.py
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
os.environ['MW3_X87'] = '53'  # the reference's precision, only: docs/pm-tests.md

from original import FTOL  # noqa: E402
import test_pm_zar  # noqa: E402

MAKE, AGE, ADD, WITHIN, LATEST, HARDEST, CLOCK = 0x4b2580, 0x4b25c0, 0x4b2740, 0x4b27d0, 0x4b2820, 0x4b2890, 0x8c1b50
NEW, DELETE = 0x5ce8c6, 0x5ce8c0
bits = lambda f: struct.unpack('<I', struct.pack('<f', f))[0]


class Exe(test_pm_zar.Emulated):
    def __init__(self):
        super().__init__()
        self.free = []
        self.native_stub(NEW, self.new)
        self.native_stub(DELETE, self.delete)
        self.obj = self.alloc(0x20)
        self.w32(0x5de644, self.cpu.code(FTOL))  # MSVCRT's _ftol, 0x5cef9a's import

    def new(self, c):
        return self.free.pop() if self.free else self.alloc(0x18)

    def delete(self, c):
        from unicorn.x86_const import UC_X86_REG_ESP
        self.free.append(self.r32(c._uc.reg_read(UC_X86_REG_ESP) + 4))
        return 0

    def w32(self, a, v):
        self.cpu._uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def clock(self, t):
        self.w32(CLOCK, bits(t))

    def dump(self):
        head = self.r32(self.obj + 4)
        out, n = [], self.r32(head)
        while n != head:
            out.extend(self.r32(n + i) for i in (8, 12, 16, 20))
            n = self.r32(n)
        return out


def main():
    lib = ctypes.CDLL(str(next((test_pm_zar.ROOT / 'build').glob('libpmprobe.*'))))
    for n in ('new', 'add'):
        getattr(lib, 'probe_attackers_' + n).restype = None
    lib.probe_attackers_new.argtypes = [ctypes.c_float] * 3
    lib.probe_attackers_age.argtypes = lib.probe_attackers_add.argtypes = [ctypes.c_uint32, ctypes.c_float, ctypes.c_float]
    lib.probe_attackers_within.argtypes = [ctypes.c_float, ctypes.c_float]
    lib.probe_attackers_hardest.argtypes = lib.probe_attackers_latest.argtypes = [ctypes.c_float]
    lib.probe_attackers_hardest.restype = lib.probe_attackers_latest.restype = ctypes.c_uint32
    rng = random.Random(7)
    exe = Exe()
    cases = bad = 0
    buf = (ctypes.c_uint32 * 400)()
    for run in range(300):
        rate, limit = (0.3, 5.0) if run % 2 else (rng.uniform(0.05, 2), rng.uniform(0.5, 20))
        t = rng.uniform(0, 2000)
        exe.clock(t)
        exe.call(MAKE, ecx=exe.obj, args=(bits(rate), bits(limit)))
        lib.probe_attackers_new(rate, limit, t)
        for step in range(60):
            t = struct.unpack('<f', struct.pack('<f', t + rng.choice([0.0, 1 / 30, 0.1, 0.5, 2.0, 6.0, 900.0])))[0]
            exe.clock(t)
            who = rng.choice([0, 0x101, 0x102, 0x103, 0x104])
            amount = rng.uniform(0, 80)
            op = rng.randrange(5)
            if op == 0:
                a, b = exe.call(AGE, ecx=exe.obj, args=(who, bits(amount))) & 0xff, lib.probe_attackers_age(who, amount, t)
            elif op == 1:
                exe.call(ADD, ecx=exe.obj, args=(1, 2, bits(amount), who))
                lib.probe_attackers_add(who, amount, t)
                a = b = 0
            elif op == 2:
                s = rng.choice([0.0, rng.uniform(0, 8)])  # 0: a tie with a hit this instant
                a, b = (exe.call(WITHIN, ecx=exe.obj, args=(bits(s),)) >> 0) & 0xff, lib.probe_attackers_within(s, t)
            elif op == 3:
                a, b = exe.call(HARDEST, ecx=exe.obj), lib.probe_attackers_hardest(t)
            else:
                a, b = exe.call(LATEST, ecx=exe.obj), lib.probe_attackers_latest(t)
            n = lib.probe_attackers_dump(buf, 100)
            cases += 1
            if a != b or exe.dump() != list(buf[:4 * n]):
                bad += 1
                if bad < 4:
                    print('differs run', run, 'step', step, 'op', op, 'ret', a, b, exe.dump()[:8], list(buf[:8]))
    print(cases, 'calls,', bad, 'differ')
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
