"""PM's firepoint picker 0x455930 against the exe: where a weapon goes in the mech lab. It
replaces MW3's last-and-first turn: a missile weapon the widest scatter (the first of
equals), any other a random firepoint of the part, none on the part no firepoint.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_lab_pick.py
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import pm_floors  # noqa: E402,F401
import test_pm_zar  # noqa: E402

probe = test_pm_zar.load_pm_probe()
probe.probe_pm_lab_pick.argtypes = [ctypes.POINTER(ctypes.c_int32), ctypes.POINTER(ctypes.c_float), ctypes.c_int32,
                                    ctypes.c_int32, ctypes.c_int32, ctypes.c_uint32]
probe.probe_pm_lab_pick.restype = ctypes.c_int32
PICK, DFN_OF, NEW, DELETE, IAT_RAND = 0x455930, 0x4867f0, 0x5ce8c6, 0x5ce8c0, 0x5de668
WEAPONS = 56


class Exe:
    def __init__(self):
        e = self.e = test_pm_zar.Emulated()
        self.uc = e.cpu._uc
        self.state = 0
        self.dfn, self.vec, self.cfg = e.alloc(0x40), e.alloc(0x20), e.alloc(0x40)
        self.w(self.dfn + 0x24, self.vec)
        e.native_stub(DFN_OF, lambda c: self.dfn)
        e.native_stub(NEW, lambda c: e.alloc(c.arg(0)))
        e.native_stub(DELETE, lambda c: 0)
        addr = e.cpu.code(b'\xc3')
        e.native_stub(addr, lambda c: self.rand())
        self.w(IAT_RAND, addr)
        self.rows = e.alloc(0x400)
        self.fps = [e.alloc(0x80) for _ in range(32)]

    def w(self, addr, v):
        self.uc.mem_write(addr, struct.pack('<I', v & 0xffffffff))

    def rand(self):
        self.state = (self.state * 214013 + 2531011) & 0xffffffff
        return (self.state >> 16) & 0x7fff

    def pick(self, parts, radii, weapon, part, seed):
        self.state = seed
        for i, (p, r) in enumerate(zip(parts, radii)):
            f = self.fps[i]
            self.uc.mem_write(f + 0x60, struct.pack('<f', r))
            self.w(f + 0x68, p)
            self.w(self.rows + 4 * i, f)
        self.w(self.vec + 4, self.rows), self.w(self.vec + 8, self.rows + 4 * len(parts))
        got = struct.unpack('<I', struct.pack('<i', self.e.call(PICK, args=(self.cfg, weapon, part))))[0]
        return self.fps.index(got) if got else -1


def main():
    exe = Exe()
    rng = random.Random(1)
    bad = none = missile = other = 0
    for _ in range(6000):
        n = rng.randrange(0, 12)
        parts = [rng.randrange(6) for _ in range(n)]
        radii = [rng.choice((0.0, 0.0, 0.5, 1.0, 1.0, 2.5, rng.uniform(0, 4))) for _ in range(n)]
        weapon, part, seed = rng.randrange(WEAPONS + 4), rng.randrange(6), rng.randrange(1 << 31)
        theirs = exe.pick(parts, radii, weapon, part, seed)
        mine = probe.probe_pm_lab_pick((ctypes.c_int32 * max(n, 1))(*parts), (ctypes.c_float * max(n, 1))(*radii), n,
                                       weapon, part, seed)
        if theirs < 0:
            none += 1
        elif 0x17 <= weapon <= 0x1e or 0x26 <= weapon <= 0x2b or 0x2d <= weapon <= 0x30:
            missile += 1
        else:
            other += 1
        if mine != theirs:
            bad += 1
            if bad < 8:
                print('differs', parts, radii, hex(weapon), part, 'mine', mine, 'exe', theirs)
    if bad:
        print(bad, 'differ')
        sys.exit(1)
    print(f'6000 picks alike: {missile} missile weapons, {other} others by rand, {none} with none on the part')


if __name__ == '__main__':
    main()
