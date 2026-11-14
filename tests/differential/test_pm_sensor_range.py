"""PM's sensor range (0x4e7170, mech3/zai_target set_sensor_range) against the port. MW3's 0x4d6d20 gives the
player's vehicle (or one whose vt+0x60 answers) 50000 in Instant Action and every multiplayer game; PM gives it
in multiplayer only with the game's RADAR byte (+0xa8 of the pilot's +0x6c, 0x47b710) on; every other range is
capped at 1000. Random pilots, games, answers and ranges (NaN and past the cap among them); the stored range,
the range and its square must match bit for bit.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

os.environ.setdefault('MW3_X87', '24')
sys.path.insert(0, str(Path(__file__).parent))
import pm_floors  # noqa: E402
import test_pm_zar  # noqa: E402

PILOT = 0x62c40c
probe = test_pm_zar.load_pm_probe()
probe.probe_pm_sensor_range.argtypes = [ctypes.POINTER(ctypes.c_uint32), ctypes.POINTER(ctypes.c_uint32)]


def fb(v):
    return struct.unpack('<I', struct.pack('<f', v))[0]


class Emu:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.case = None

        def stub(fn):
            a = z.cpu.code(b'\xc3')
            z.native_stub(a, fn)
            return a
        self.obj = z.alloc(0xb0)
        vt = z.alloc(0x100)
        z.cpu._uc.mem_write(vt + 0x58, struct.pack('<I', stub(lambda c: self.case[2])))
        z.cpu._uc.mem_write(vt + 0x60, struct.pack('<I', stub(lambda c: self.case[3])))
        z.cpu._uc.mem_write(self.obj, struct.pack('<I', vt))
        self.pilot = z.alloc(0x100)
        self.game = z.alloc(0x100)
        gvt = z.alloc(0x40)
        z.cpu._uc.mem_write(gvt + 0x28, struct.pack('<I', stub(lambda c: self.case[4])))
        z.cpu._uc.mem_write(self.game, struct.pack('<I', gvt))
        z.cpu._uc.mem_write(self.pilot + 0x6c, struct.pack('<I', self.game))

    def run(self, case):
        self.case = case
        z = self.z
        z.cpu._uc.mem_write(PILOT, struct.pack('<I', self.pilot if case[0] else 0))
        z.cpu._uc.mem_write(self.pilot + 0x14, struct.pack('<I', case[1]))
        z.cpu._uc.mem_write(self.game + 0xa8, bytes([case[5]]))
        z.call(0x4e7170, ecx=self.obj, args=(case[6],))
        return struct.unpack('<3I', bytes(z.cpu._uc.mem_read(self.obj + 0x9c, 12)))


def main():
    rng = random.Random(0x4e7170)
    emu = Emu()
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    wide = capped = 0
    out = (ctypes.c_uint32 * 3)()
    for r in range(runs):
        player = rng.choice((0x5000, 0x6000))
        case = [rng.random() < 0.85, player, rng.choice((0x5000, 0x5000, 0x7000, 0)), rng.choice((0, 0, 1, 0x99)),
                rng.choice((0, 1, 2, 3, 3, 3, 4)), rng.choice((0, 1, 0x80)),
                fb(rng.choice((10.0, 999.0, 1000.0, 1000.5, 2500.0, 60000.0, -5.0, float('nan'), rng.uniform(0, 4000))))]
        want = emu.run(case)
        probe.probe_pm_sensor_range((ctypes.c_uint32 * 7)(*[int(x) & 0xffffffff for x in case]), out)
        got = tuple(out)
        if got != want:
            raise SystemExit(f'case {r} {case}: exe {[hex(x) for x in want]} port {[hex(x) for x in got]}')
        if want[1] == fb(50000.0):
            wide += 1
        elif want[1] == fb(1000.0):
            capped += 1
    pm_floors.floors(wide=(wide, 300), capped=(capped, 600))
    print(f'ok: {runs} cases identical ({wide} wide, {capped} capped)')


if __name__ == '__main__':
    main()
