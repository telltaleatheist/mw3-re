"""PM's lance flag 0x4e5f90 against the exe: MW3's 0x4d5be0 with the bits on its own marker's sides (30 allies, 29
ally or enemy by the flag) and the brain's own marker made or freed. The marker's constructor and the delete stand
in, logged; what the port makes for it (app_pocket's program_own_marker) rests on reading. At 53 bits.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_lance_flag.py
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

FLAG, MAKE, NEW, ENTITY = 0x4e5f90, 0x4eae70, 0x5ce8c6, 0x62c40c


class Exe:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        self.w = lambda a, v: self.uc.mem_write(a, struct.pack('<I', v & 0xffffffff))
        a = iter(range(z.alloc(0x2000), 1 << 31, 0x100))
        self.x, xvt, self.m, mvt, self.ic, ivt, self.brain = (next(a) for _ in range(7))
        self.player, pvt, self.other, ovt, self.marker, kvt, lst = (next(a) for _ in range(7))
        self.w(self.x, xvt), self.w(self.m, mvt), self.w(self.ic, ivt)
        self.w(xvt + 0x48, self.ret(lambda c: self.m if self.inp[0] else 0))
        self.w(mvt + 0x14c, self.ret(lambda c: self.ic if self.inp[1] else 0))
        self.w(ivt + 0x58, self.ret(lambda c: self.brain if self.inp[2] else 0))
        self.w(xvt + 0x58, self.ret(lambda c: {0: 0, 1: self.player, 2: self.other}[self.inp[4]]))
        for o, vt in ((self.player, pvt), (self.other, ovt)):
            self.w(o, vt)
            self.w(vt + 0x28, self.ret(lambda c: c.ecx))
        self.w(ENTITY, lst), self.w(lst + 0x14, self.player)
        self.w(self.marker, kvt), self.w(kvt, self.ret(self.freed, 4))
        z.native_stub(NEW, lambda c: self.m)
        z.native_stub(MAKE, self.made)

    def ret(self, fn, pop=0):
        t = self.z.alloc(4)
        self.uc.mem_write(t, b'\xc3')
        self.z.native_stub(t, fn, pop)
        return t

    def made(self, c):
        self.log.append(0x71)
        return self.marker

    def freed(self, c):
        self.log.append(0x72)
        return 0

    def run(self, inp):
        w, z = self.w, self.z
        self.inp, self.log = inp, []
        self.uc.mem_write(self.brain + 0xed, bytes([inp[3]]))
        w(self.player + 0xc0, inp[5]), w(self.other + 0xc0, inp[5])
        w(self.player + 0xc4, inp[6])
        self.uc.mem_write(self.x + 0xbc, bytes([inp[7]]))
        w(self.x + 0xc4, inp[8]), w(self.x + 0xc8, inp[9])
        w(self.brain + 0x2cc, self.marker if inp[10] else 0)
        z.call(FLAG, self.x)
        return [bytes(self.uc.mem_read(self.x + 0xbc, 1))[0], z.r32(self.x + 0xc4), z.r32(self.x + 0xc8)] + self.log


def main():
    p = test_pm_zar.load_pm_probe()
    p.probe_pm_lance_flag.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int]
    rng = random.Random(1)
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    emu = Exe()
    out = (ctypes.c_uint32 * 8)()
    on = off = freed = 0
    for k in range(n):
        inp = [int(rng.random() < 0.9), int(rng.random() < 0.9), int(rng.random() < 0.85), rng.choice([0, 1, 2, 3, 0xf0]),
               rng.choice([0, 1, 2, 2]), rng.choice([2, 4]), rng.choice([2, 4, 6, 0]), rng.choice([0, 0x10, 0xff, 0xef]),
               rng.getrandbits(31), rng.getrandbits(31), int(rng.random() < 0.5)]
        a = emu.run(inp)
        m = p.probe_pm_lance_flag((ctypes.c_int32 * 11)(*inp), out, 8)
        b = list(out[:m])
        if a != b:
            print(f'case {k}: exe {[hex(x) for x in a]}\n        port {[hex(x) for x in b]}\n  in {inp}')
            return 1
        on += a[0] >> 4 & 1
        off += not a[0] >> 4 & 1
        freed += 0x72 in a
    print(f'ok: {n} cases, {on} on, {off} off, {freed} markers freed: identical')
    pm_floors.floors(cases=(n, 1000), on=(on, 300), off=(off, 300), freed=(freed, 200))
    return 0


if __name__ == '__main__':
    sys.exit(main())
