"""Pirate's Moon's camera ground and water check 0x408b20, with its set_underwater 0x408d00 and the fog mode
0x5404f0 running, against the port's PlayerView::follow_ground (tests/probe_pm_view_ground.cpp), on PM's exe, the x87 at 53 bits (the reference runtime's).

PM probes the ground from 1024 m above the camera ([0x6026e0]; MW3's 50 m); a hit ends it. Else the water under it
(0x4af770: the zone index, kind, level and the ground) decides under water (kind 1, the camera below the level, the
level above the ground), and set_underwater sets the world's fog from the mission's air or water block (0x505670
range, 0x505620 colour, 0x505650 altitude) and, new in PM, the renderer's fog mode: 2 under water, 1 above, where
0x5404f0 keeps a 2 against anything but 0. An outside camera below the ground is lifted to half a metre above it
(0x506d90); the camera's zone follows (0x408c90).

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_view_ground.py [cases]
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_pm_zar  # noqa: E402
from original import ROOT  # noqa: E402

CHECK, POS, GROUND, ENTITY, WATER, RANGE, COLOUR, ALT, LIFT, ZONE = (
    0x408b20, 0x4ff250, 0x4ff8d0, 0x5cb260, 0x4af770, 0x505670, 0x505620, 0x505650, 0x506d90, 0x408c90)
FOGMODE, FLAG, AIRFOG, WATERFOG, WORLD = 0xa4e038, 0x6026e4, 0x62da14, 0x62da3c, 0x62ca90

probe = ctypes.CDLL(str(next((ROOT / 'build').glob('libpmprobe.*'))))
probe.probe_pm_view_ground.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
OUT = (ctypes.c_uint32 * 64)()


def fb(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


class Exe:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        u = self.uc = z.cpu._uc
        a = iter(range(z.alloc(0x2000), 1 << 31, 0x100))
        self.view, self.holder, self.target, self.mode, self.place, self.cell = (next(a) for _ in range(6))
        z.native_stub(POS, self.pos, 0)
        z.native_stub(GROUND, lambda c: self.log.extend([1, c.arg(2)]) or self.i[3], 0)
        z.native_stub(ENTITY, lambda c: self.place, 0)
        water = z.cpu.code(b'\xc3')
        z.native_stub(water, self.water, 0)
        # the water query answers in ST0: its entry calls the stub, then loads the answer
        u.mem_write(WATER, b'\xe8' + struct.pack('<i', water - (WATER + 5)) + b'\xd9\x05' + struct.pack('<I', self.cell) + b'\xc3')
        z.native_stub(RANGE, lambda c: self.log.extend([3, c.arg(1), c.arg(2)]) or 0, 0)
        z.native_stub(COLOUR, lambda c: self.log.extend([4, c.arg(1), c.arg(2), c.arg(3)]) or 0, 0)
        z.native_stub(ALT, lambda c: self.log.extend([5, c.arg(1), c.arg(2)]) or 0, 0)
        z.native_stub(LIFT, lambda c: self.log.extend([7, c.arg(1), c.arg(2), c.arg(3)]) or 0, 0)
        z.native_stub(ZONE, lambda c: self.log.extend([6, c.arg(1)]) or 1, 8)
        z.cpu.fpu_control(0x27f)  # 53-bit, the reference runtime's precision (CLAUDE.md)

    def wr(self, a, v):
        self.uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def rd(self, a):
        return struct.unpack('<I', bytes(self.uc.mem_read(a, 4)))[0]

    def pos(self, c):
        self.uc.mem_write(c.arg(1), struct.pack('<3I', *self.i[0:3]))
        return 0

    def water(self, c):
        # args shifted by the patched call's own return address: [esp+4] the exe's return, then its 8 arguments
        sp = self.uc.reg_read(__import__('unicorn').x86_const.UC_X86_REG_ESP)
        args = struct.unpack('<9I', bytes(self.uc.mem_read(sp + 4, 36)))[1:]
        _, _, y, height, index, flag, kind, level = args
        self.log += [2, y, height, flag & 0xff]
        self.wr(index, self.i[6])
        self.wr(kind, self.i[7])
        self.wr(level, self.i[8])
        self.wr(self.cell, self.i[9])
        return 0

    def run(self, i):
        self.i, self.log = i, []
        v = self.view
        self.uc.mem_write(v, bytes(0x80))
        self.wr(v + 4, 0x1000)
        self.wr(v + 0xc, self.holder)
        self.wr(self.holder + 4, 0x2000)
        if i[4]:
            self.wr(v + 0x64, self.target)
            self.wr(self.target + 0x68, i[4])
            self.uc.mem_write(self.place, struct.pack('<3I', 0, i[5], 0))
        if i[13] != 0xffffffff:
            self.wr(v + 0x58, self.mode)
            self.wr(self.mode + 4, i[13])
        self.uc.mem_write(v + 0x6a, bytes([i[11]]))
        self.wr(FOGMODE, i[12])
        self.wr(WORLD, self.holder)  # the world holder 0x408d00's fog setters take +4 of
        self.wr(FLAG, i[10])
        self.uc.mem_write(AIRFOG, struct.pack('<7I', *i[14:21]))
        self.uc.mem_write(WATERFOG, struct.pack('<7I', *i[21:28]))
        r = self.z.call(CHECK, ecx=v) & 0xff
        return [1 if r else 0, self.uc.mem_read(v + 0x6a, 1)[0], self.rd(FOGMODE)] + self.log


def case(r):
    y = r.uniform(-50, 300)
    level = y + r.choice([-5.0, 5.0, 0.0]) if r.random() < 0.7 else r.uniform(-50, 300)
    ground = r.choice([y - 10, y + 2, level - 20, level + 3]) if r.random() < 0.8 else r.uniform(-60, 300)
    return [fb(r.uniform(-1000, 1000)), fb(y), fb(r.uniform(-1000, 1000)), int(r.random() < 0.15),
            fb(r.uniform(5, 20)) if r.random() < 0.7 else 0, fb(y + r.uniform(-15, 5)),
            r.randrange(4), r.choice([1, 1, 0, 2]), fb(level), fb(ground), r.choice([0, 1]),
            r.randrange(2), r.choice([0, 1, 2]), r.choice([0xffffffff, 1, 0, 3]),
            *[fb(r.uniform(0, 600)) for _ in range(14)]]


def main():
    cases = int(sys.argv[1]) if len(sys.argv) > 1 else 4000
    exe = Exe()
    rng = random.Random(0x408b20)
    bad, kinds = 0, {}
    for n in range(cases):
        i = case(rng)
        e = exe.run(i)
        k = probe.probe_pm_view_ground((ctypes.c_uint32 * 28)(*i), OUT)
        p = list(OUT[:k])
        key = ('hit' if i[3] else ('under' if e[1] else 'above'), f'mode {e[2]}', 'lifted' if 7 in e[3::] else '')
        kinds[key] = kinds.get(key, 0) + 1
        if e != p:
            bad += 1
            if bad <= 5:
                print(f'FAIL case {n}: {i[:14]}\n  exe  {e}\n  port {p}')
    seen = ', '.join(f"{' '.join(x for x in k if x)} {v}" for k, v in sorted(kinds.items()))
    print(f'{cases} checks ({seen}), {bad} differ')
    sys.stdout.flush()
    os._exit(1 if bad else 0)


if __name__ == '__main__':
    main()
