"""Pirate's Moon's mission fade from black: the step 0x483470 (inline the same in the frames 0x484530 and 0x4838b0)
against the port's ztwod::weather_fade_step (tests/probe_pm_fade.cpp), on PM's exe, the x87 at 53 bits (the reference runtime's). Unless done (+0x1f0), the value
(+0x200, a double) steps by dt x rate (+0x1f8) x 0.3 ([0x608100]); above 1 it is 1, below 0 it is 0, and either is
done; the second tint's alpha (+0x248, the tint at +0x208's +0x40) takes it and the tint draws (its vt+0x7c); then
on the hardware path (+0x1f1) a scene shows it (0x598bb0, [0x8c4c14], 0x598bd0), else the software's (0x57a5d0,
0x57a770).

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_fade.py [cases]
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

STEP, DRAW3D = 0x483470, 0x8c4c14

probe = ctypes.CDLL(str(next((ROOT / 'build').glob('libpmprobe.*'))))
probe.probe_pm_fade_step.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
OUT = (ctypes.c_uint32 * 32)()


class Exe:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        self.layer, self.vt = z.alloc(0x300), z.alloc(0x100)
        draw = z.cpu.code(b'\xc3')
        z.native_stub(draw, lambda u: self.log.extend([1, *self.words(self.layer + 0x248)]) or 0, 0)
        self.uc.mem_write(self.vt + 0x7c, struct.pack('<I', draw))
        z.native_stub(0x598bb0, lambda u: self.log.append(2) or 0, 0)
        z.native_stub(0x598bd0, lambda u: 0, 0)
        d3d = z.cpu.code(b'\xc3')
        z.native_stub(d3d, lambda u: 0, 0)
        self.uc.mem_write(DRAW3D, struct.pack('<I', d3d))
        z.native_stub(0x57a5d0, lambda u: self.log.append(3) or 0, 0)
        z.native_stub(0x57a770, lambda u: 0, 0)
        z.cpu.fpu_control(0x27f)  # 53-bit, the reference runtime's precision (CLAUDE.md)

    def words(self, a):
        return list(struct.unpack('<2I', bytes(self.uc.mem_read(a, 8))))

    def run(self, i):
        L = self.layer
        self.log = []
        self.uc.mem_write(L, bytes(0x300))
        self.uc.mem_write(L + 0x1f0, bytes([i[0], i[1]]))
        self.uc.mem_write(L + 0x1f8, struct.pack('<4I', *i[2:6]))
        self.uc.mem_write(L + 0x208, struct.pack('<I', self.vt))
        self.z.call(STEP, ecx=L, args=(i[6],))
        return [self.uc.mem_read(L + 0x1f0, 1)[0], *self.words(L + 0x200), *self.words(L + 0x248)] + self.log


def dw(x):
    return list(struct.unpack('<2I', struct.pack('<d', x)))


def case(r):
    rate = r.choice([-1.0, -1.0, 1.0, 0.0, -2.5, float('nan')])
    value = r.choice([1.0, 0.5, 0.0, 0.999, 0.001, -0.2, 1.3, float('nan')]) if r.random() < 0.5 else r.uniform(0, 1)
    dt = r.choice([0.033, 0.0, 0.5, 4.0, 1e-5, -0.1]) if r.random() < 0.5 else r.uniform(0, 0.2)
    return [int(r.random() < 0.15), int(r.random() < 0.8), *dw(rate), *dw(value), struct.unpack('<I', struct.pack('<f', dt))[0]]


def main():
    cases = int(sys.argv[1]) if len(sys.argv) > 1 else 5000
    exe = Exe()
    rng = random.Random(0x483470)
    bad = 0
    seen = {}
    for n in range(cases):
        i = case(rng)
        e = exe.run(i)
        k = probe.probe_pm_fade_step((ctypes.c_uint32 * 7)(*i), OUT)
        p = list(OUT[:k])
        key = ('done before' if i[0] else 'stepped', 'now done' if e[0] else 'going', 'hw' if i[1] else 'sw')
        seen[key] = seen.get(key, 0) + 1
        if e != p:
            bad += 1
            if bad <= 5:
                print(f'FAIL case {n}: {i}\n  exe  {e}\n  port {p}')
    kinds = ', '.join(f"{'/'.join(k)} {v}" for k, v in sorted(seen.items()))
    print(f'{cases} steps ({kinds}), {bad} differ')
    sys.stdout.flush()
    os._exit(1 if bad else 0)


if __name__ == '__main__':
    main()
