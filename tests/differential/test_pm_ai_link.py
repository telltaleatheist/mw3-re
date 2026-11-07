"""PM's link pick 0x4d60d0 on the exe against pick_link_pm, at 53 bits, to the bit: a node's links, real or missing or
the null step, their weights and flat directions, the way finder's answer, the roll."""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
os.environ['MW3_X87'] = '53'  # the reference's precision, only: docs/pm-tests.md
from unicorn.x86_const import UC_X86_REG_ESP  # noqa: E402

import pm_floors  # noqa: E402
import test_pm_zar  # noqa: E402

PICK, PATH, SCRATCH, RAND = 0x4d60d0, 0x4d3120, 0x62fadc, 0x5de668


class Exe(test_pm_zar.Emulated):
    def __init__(self):
        super().__init__()
        self.brain = self.alloc(0x500)
        self.w32(self.brain + 0x148, self.alloc(0x40))  # the order list 0x4d7750 reads, unused after
        self.at, self.atvt = self.alloc(8), self.alloc(0x40)
        self.w32(self.at, self.atvt)
        self.point, self.heading = self.alloc(12), self.alloc(12)
        self.miss, self.seen = self.alloc(8), self.alloc(12)
        self.n, self.path, self.roll, self.rolled = 0, 0, 0, 0
        self.out = []
        self.native_stub(PATH, lambda c: self.path, pop=4)
        roll = self.cpu.code(b'\xc3')
        self.w32(RAND, roll)

        def do_roll(c):
            self.rolled += 1
            return self.roll
        self.native_stub(roll, do_roll, 0)
        self.w32(self.atvt + 4, self.ret(lambda c: self.n))
        self.w32(self.atvt + 0x10, self.ret(lambda c: self.miss))
        self.w32(self.atvt + 0x1c, self.ret(lambda c: self.point))
        link = self.cpu.code(b'\xc2\x04\x00')
        self.w32(self.atvt + 8, link)
        self.native_stub(link, self.link, 4)
        self.block = self.alloc(0x100)
        self.native_stub(0x5ce8c6, lambda c: self.block)  # one at a time: each freed before the next
        self.native_stub(0x5ce8c0, lambda c: 0)
        self.scratch = self.alloc(0x10)
        self.w32(SCRATCH, self.scratch)
        self.holders, self.kinds = [], []
        self.slots = [self.make(i) for i in range(9)]

    def ret(self, fn):
        a = self.cpu.code(b'\xc3')
        self.native_stub(a, fn, 0)
        return a

    def w32(self, a, v):
        self.cpu._uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def g(self, a):
        return struct.unpack('<I', bytes(self.cpu._uc.mem_read(a, 4)))[0]

    def fs(self, a, v):
        self.cpu._uc.mem_write(a, struct.pack(f'<{len(v)}f', *v))

    def link(self, c):
        i = self.g(c._uc.reg_read(UC_X86_REG_ESP) + 4)
        return self.holders[i] if i < self.n else 0

    def make(self, i):
        """Link i: the first object, its vt+0x60, that one's vt+0x74, whose vt+0xac writes the way."""
        last, vals = self.alloc(8), self.alloc(16)
        code = (bytes.fromhex('8b44240c')                     # mov eax, [esp + 0xc]: the way
                + b'\x8b\x0d' + struct.pack('<I', vals + 4) + bytes.fromhex('894814')
                + b'\x8b\x0d' + struct.pack('<I', vals + 8) + bytes.fromhex('894818')
                + b'\x8b\x0d' + struct.pack('<I', vals + 12) + bytes.fromhex('89481c')
                + bytes.fromhex('8b4808') + b'\x89\x0d' + struct.pack('<I', self.seen)
                + bytes.fromhex('8b480c') + b'\x89\x0d' + struct.pack('<I', self.seen + 4)
                + bytes.fromhex('8b4810') + b'\x89\x0d' + struct.pack('<I', self.seen + 8)
                + b'\xd9\x05' + struct.pack('<I', vals) + bytes.fromhex('c20c00'))  # fld the weight; ret 0xc
        vt = self.alloc(0x100)
        self.w32(last, vt)
        self.w32(vt + 0xac, self.cpu.code(code))
        self.w32(self.miss, vt)
        mid, mvt = self.alloc(8), self.alloc(0x100)
        self.w32(mid, mvt)
        self.w32(mvt + 0x74, self.ret(lambda c: {0: last, 1: 0, 2: self.miss}[self.kinds[i]]))
        h, hvt = self.alloc(8), self.alloc(0x100)
        self.w32(h, hvt)
        self.w32(hvt + 0x60, self.ret(lambda c: mid))
        return h, last, vals

    def run(self, n, kinds, weights, dirs, heading, point, path, roll):
        self.n, self.path, self.roll, self.rolled, self.kinds = n, path, roll, 0, kinds
        self.fs(self.heading, heading)
        self.fs(self.point, point)
        self.holders, finals = [], []
        for i in range(n):
            h, last, vals = self.slots[i]
            self.fs(vals, [weights[i]] + dirs[3 * i:3 * i + 3])
            self.holders.append(h)
            finals.append(last)
        r = self.call(PICK, ecx=self.at, args=(self.brain, self.heading))
        if r == 0 or r == path:
            pick = r
        elif r == self.miss:  # a roll can land on the null step
            pick = 100
        else:
            pick = 1 + finals.index(r)
        sums = bytes(self.cpu._uc.mem_read(self.g(self.scratch + 4), 4 * n)) if n and self.g(self.scratch + 4) else b''
        return pick, sums, bytes(self.cpu._uc.mem_read(self.seen, 12)), self.rolled


def plan(r):
    n = r.choice([0, 1, 2, 2, 3, 4, 6, 9])
    kinds = [r.choice([0, 0, 0, 0, 1, 2]) for _ in range(n)]
    weights = [r.choice([0.0, -1.0, 1.0, 0.5, r.uniform(0.001, 3), r.uniform(0, 100)]) for _ in range(n)]
    dirs = []
    for _ in range(n):
        a = r.uniform(0, 6.283)
        dirs += [struct.unpack('<f', struct.pack('<f', __import__('math').sin(a)))[0], 0.0, struct.unpack('<f', struct.pack('<f', __import__('math').cos(a)))[0]]
    a = r.uniform(0, 6.283)
    heading = [__import__('math').sin(a), 0.0, __import__('math').cos(a)]
    return n, kinds, weights, dirs, heading, [r.uniform(-500, 500) for _ in range(3)], r.choice([0, 0, 0, 0, 0x5000]), r.choice([0, 32767, r.randrange(32768), r.randrange(32768)])


def main():
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 4000
    lib = test_pm_zar.load_pm_probe()
    pi, pf = ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_float)
    lib.probe_pm_ai_link.argtypes = [ctypes.c_int, pi, pf, pf, pf, pf, ctypes.c_int, ctypes.c_int, pf, pf, pi]
    x = Exe()
    r = random.Random(4960)
    bad = rolled = picked = paths = nones = 0
    fa = lambda v: (ctypes.c_float * max(len(v), 1))(*v)  # noqa: E731
    for i in range(runs):
        p = plan(r)
        n, kinds, weights, dirs, heading, point, path, roll = p
        want, wsums, wseen, wrolled = x.run(*p)
        sums, seen, gr = (ctypes.c_float * max(n, 1))(), (ctypes.c_float * 3)(), ctypes.c_int()
        got = lib.probe_pm_ai_link(n, (ctypes.c_int * max(n, 1))(*kinds), fa(weights), fa(dirs), fa(heading), fa(point), path, roll, sums, seen, ctypes.byref(gr))
        gsums = bytes(sums)[:4 * n] if n else b''
        if path and not want:
            continue
        ok = want == got and gr.value == wrolled and (path != 0 or n < 2 or wsums[:4 * n] == gsums)
        any_weighed = any(k != 1 and k != 2 and w > 0 for k, w in zip(kinds, weights))
        if ok and any_weighed and not path and wseen != bytes(seen):
            ok = False
        if not ok:
            bad += 1
            print(f'{i}: {p}\n  exe  {want} {wsums.hex()} {wrolled}\n  port {got} {gsums.hex()} {gr.value}')
            if bad > 5:
                break
        rolled += wrolled
        picked += bool(want) and not path
        paths += bool(path)
        nones += not want
    print(f'{runs} picks, {picked} by weight, {rolled} rolled, {paths} by the way finder, {nones} none: {bad} differ')
    pm_floors.floors(picks=(picked, 500), rolled=(rolled, 300), paths=(paths, 100), nones=(nones, 100))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
