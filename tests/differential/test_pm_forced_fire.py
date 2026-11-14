"""Pirate's Moon's projectile and beam starts 0x4b7600 and 0x4b8030 against the port's Weapon::aim_and_fire and
BeamWeapon::aim_and_fire (tests/probe_pm_forced_fire.cpp), on PM's exe and layout, forced (the network's fire) and
not. Unforced: the readiness test (0x4b7bd0), then PM's aim (0x4b7ac0, real: the line and the weapon's floor +0x98),
either failing ends it. Forced: neither; the line (0x4b7800) runs with [0x62f86c] set (no facing test) and the
weapon fires (the projectile's vt+4 with the vehicle's velocity, +0x70 cleared) or makes its beam if it has none
(0x5a6560: the definition, the vehicle's node, 0, the weapon's node, the muzzle, the direction, 1) whatever it says.

The line, the readiness test, the fire slot and the beam's maker answer from the case on both sides and are logged
(the line with the switch's value as it runs).

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_forced_fire.py [cases]
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

SHOT, BEAM, READY, LINE, MAKE, QUIET = 0x4b7600, 0x4b8030, 0x4b7bd0, 0x4b7800, 0x5a6560, 0x62f86c

probe = ctypes.CDLL(str(next((ROOT / 'build').glob('libpmprobe.*'))))
probe.probe_pm_forced_fire.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p]
OUT = (ctypes.c_uint32 * 64)()


def fb(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


class Exe:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        a = iter(range(z.alloc(0x4000), 1 << 31, 0x200))
        self.w, self.mgr, self.veh, self.defn, self.vt, self.at = (next(a) for _ in range(6))
        z.native_stub(READY, lambda u: self.i[3], 0)
        z.native_stub(LINE, self.line, 0x18)
        z.native_stub(MAKE, self.make, 0)
        fire = z.cpu.code(b'\xc3')
        z.native_stub(fire, self.fire, 4)
        self.wr(self.vt + 4, fire)

    def wr(self, addr, v):
        self.uc.mem_write(addr, struct.pack('<I', v & 0xffffffff))

    def rd(self, addr):
        return struct.unpack('<I', bytes(self.uc.mem_read(addr, 4)))[0]

    def line(self, u):
        self.log += [1, self.uc.mem_read(QUIET, 1)[0]]
        self.log += list(struct.unpack('<3I', bytes(self.uc.mem_read(u.arg(0), 12))))
        self.uc.mem_write(u.arg(1), struct.pack('<3f', 1, 2, 3))
        self.uc.mem_write(u.arg(2), struct.pack('<3f', 4, 5, 6))
        self.wr(u.arg(4), self.i[1])
        self.wr(u.arg(5), self.i[2])
        return self.i[0]

    def fire(self, u):
        self.log += [2, 1 if u.arg(0) == self.veh + 0x30 else 0]
        return self.i[9]

    def make(self, u):
        self.log += [3, u.arg(1), u.arg(3), self.rd(u.arg(4)), self.rd(u.arg(5) + 8)]
        return 0x77

    def run(self, mode, i):
        w, wr = self.w, self.wr
        self.i, self.log = i, []
        self.uc.mem_write(w, bytes(0xb0))
        wr(w, self.vt)
        wr(w + 0x24, 0x2424)
        wr(w + 0x30, self.defn)
        wr(w + 0x9c, self.mgr)
        wr(self.mgr + 0x88, self.veh)
        wr(self.veh + 4, 0x5150)
        wr(w + 0x98, i[5])
        wr(w + 0x70, 0x1234)
        if mode == 1 and i[10]:
            wr(w + 0xa8, 0x66)
        self.uc.mem_write(self.at, struct.pack('<3I', *i[6:9]))
        self.uc.mem_write(QUIET, b'\0')
        r = self.z.call(SHOT if mode == 0 else BEAM, ecx=w, args=(self.at, i[4])) & 0xff
        r = 1 if r else 0
        if mode == 0:
            out = [r, self.rd(w + 0x70)] + [self.rd(w + 0x38 + 4 * k) for k in range(3)]
        else:
            out = [r, self.rd(w + 0xa8), 1 if self.rd(w + 0xa4) == self.at else 0]
        return out + self.log


def case(r):
    cosv = r.choice([1.0, 0.9, 0.4735, 0.2, -0.5, float('nan')])
    kept = r.choice([cosv, cosv, 1.0, 0.4735, 0.9])
    floor = r.choice([-1.0, -1.0, 0.0, 0.3, 0.9, 1.0, float('nan')])
    at = [r.uniform(-500, 500) for _ in range(3)]
    return [r.random() < 0.7, fb(cosv), fb(kept), r.random() < 0.7, r.random() < 0.5, fb(floor),
            *[fb(x) for x in at], r.random() < 0.7, r.random() < 0.3]


def main():
    cases = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    exe = Exe()
    rng = random.Random(0x4b7600)
    bad, kinds = 0, {}
    for n in range(cases):
        i = [int(x) & 0xffffffff for x in case(rng)]
        for mode in (0, 1):
            e = exe.run(mode, i)
            k = probe.probe_pm_forced_fire(mode, (ctypes.c_uint32 * 11)(*i), OUT)
            p = list(OUT[:k])
            key = (('shot', 'beam')[mode], 'forced' if i[4] else 'unforced', 'fired' if e[0] else 'not')
            kinds[key] = kinds.get(key, 0) + 1
            if e != p:
                bad += 1
                if bad <= 5:
                    print(f'FAIL case {n} mode {mode}: in {i}\n  exe  {e}\n  port {p}')
    seen = ', '.join(f"{'/'.join(k)} {v}" for k, v in sorted(kinds.items()))
    print(f'{cases} cases ({seen}), {bad} differ')
    sys.stdout.flush()
    os._exit(1 if bad else 0)


if __name__ == '__main__':
    main()
