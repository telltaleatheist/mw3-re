"""PM's load limit, 0x4e2e30, against its exe: a mech's weight on a target (c1m5's two bridges, 50 tons). Each call sets
the stress from the last call's weight against the limit; over 10 the target's vt+0x78 runs. Same stress, same weight
left and the same call giving way, at both precisions. Then the port's per_second: the same time to fall at any frame
rate, which the exe doesn't have (per call). BREAK=1 uses 0.6 for the 0.5.

    cmake --build build --target pmprobe && .venv/bin/python tests/differential/test_pm_bridge.py [cases]
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import ROOT, Original  # noqa: E402

probe = ctypes.CDLL(str(ROOT / 'build' / 'libpmprobe.dylib'))
probe.probe_pm_load_step.argtypes = [ctypes.POINTER(ctypes.c_float), ctypes.POINTER(ctypes.c_float), ctypes.c_float,
                                     ctypes.c_float, ctypes.c_float]
LOAD_FN, FRAME = 0x4e2e30, 0x8c4b60


class Exe:
    def __init__(self, cw):
        cpu = self.cpu = Original('pm')
        cpu.tr = lambda a, size=1: a  # PM's addresses
        cpu.fpu_control(cw)
        self.obj, self.mech, vt_o, vt_m = cpu.alloc(0x200), cpu.alloc(0x40), cpu.alloc(0x100), cpu.alloc(0x100)
        self.fell, self.tons = cpu.alloc(4), cpu.alloc(4)
        count = cpu.code(b'\xff\x05' + struct.pack('<I', self.fell) + b'\xc3')  # inc dword [fell]; ret
        weigh = cpu.code(b'\xd9\x05' + struct.pack('<I', self.tons) + b'\xc3')  # fld dword [tons]; ret
        cpu.u32(vt_o + 0x78, count)
        cpu.u32(vt_m + 0x8c, weigh)
        cpu.u32(self.obj, vt_o)
        cpu.u32(self.mech, vt_m)
        cpu.u32(FRAME, 1)

    def start(self, limit):
        c = self.cpu
        c.write(self.obj + 8, b'\x01')
        c.write(self.obj + 0xf0, struct.pack('<f', limit))
        for at in (0xf4, 0x100, 0x104, 0x108):
            c.u32(self.obj + at, 0)
        c.u32(self.fell, 0)

    def step(self, tons):
        c = self.cpu
        c.write(self.tons, struct.pack('<f', tons))
        c.call(LOAD_FN, ecx=self.obj, stack_args=(self.mech,))
        load, stress = (struct.unpack('<f', bytes(c.read(self.obj + a, 4)))[0] for a in (0x100, 0x104))
        return load, stress, struct.unpack('<I', bytes(c.read(self.fell, 4)))[0]


def main():
    cases = int(sys.argv[1]) if len(sys.argv) > 1 else 400
    rng = random.Random(1)
    for cw in (0x027f, 0x007f):
        exe = Exe(cw)
        fell = worst = 0
        for _ in range(cases):
            limit = rng.choice((50.0, 50.0, 30.0, rng.uniform(5, 120)))
            exe.start(limit)
            stress, load = ctypes.c_float(0), ctypes.c_float(0)
            down = 0
            for _ in range(40):
                tons = rng.choice((20.0, 40.0, 50.0, 55.0, 75.0, 100.0, rng.uniform(10, 100)))
                d, s, n = exe.step(tons)
                gave = probe.probe_pm_load_step(ctypes.byref(stress), ctypes.byref(load), limit, tons, 1.0)
                down += gave
                worst = max(worst, abs(stress.value - s) / max(1.0, s))
                assert d == load.value, (d, load.value)
                assert n == down, f'gave way on another call: exe {n}, port {down}, stress {s} {stress.value}'
                assert abs(stress.value - s) <= 2e-5 * max(1.0, s), (s, stress.value)
            fell += down > 0
        print(f'{cw:#x}: {cases} cases, {fell} gave way, worst stress difference {worst:.2e}')
        assert fell > cases // 8

    # The same time to fall at any frame rate: 75 tons on a 50-ton bridge.
    times = {}
    for fps in (30, 60, 144, 20):
        stress, load = ctypes.c_float(0), ctypes.c_float(0)
        n = 0
        while not probe.probe_pm_load_step(ctypes.byref(stress), ctypes.byref(load), 50.0, 75.0, 30.0 / fps):
            n += 1
        times[fps] = n / fps
    print('seconds to fall by frame rate:', {k: round(v, 3) for k, v in times.items()})
    assert max(times.values()) - min(times.values()) < 0.1
    # Under the limit it never goes, at any rate.
    stress, load = ctypes.c_float(0), ctypes.c_float(0)
    assert not any(probe.probe_pm_load_step(ctypes.byref(stress), ctypes.byref(load), 50.0, 40.0, 0.5) for _ in range(5000))
    if os.environ.get('BREAK'):
        sys.exit('BREAK: this run should have failed above')


if __name__ == '__main__':
    main()
