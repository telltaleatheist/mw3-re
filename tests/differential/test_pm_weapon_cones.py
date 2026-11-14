"""Pirate's Moon's per-weapon enable 0x4b6e80 and aim 0x4b7ac0 against the port's Weapon::set_enabled and
Weapon::aim (PM's on this branch, tests/probe_pm_weapon_cones.cpp), on PM's exe and layout. The weapon's line
0x4b7800 (ported and tested with the AI's firing, zai_firing_pm.cpp weapon_line) stands in on both sides, answering
from the case: whether it ran, its cosine and its kept cosine.

The enable: for the pilot's own mech (the manager's +0x84 against [0x62c40c]+0x14) the line to the aim point (the
manager's +0x0c) is run, and a weapon whose line was clamped (cosine not the kept one) is off; then MW3's: under
water only beams fire, `side` keeps one arm, `force` turns it on whatever; the answer is whether it changed. The aim:
the line, then the weapon's floor (+0x98) at most the kept cosine (x87: below, equal or unordered).

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_weapon_cones.py [cases]
"""
import ctypes
import math
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_pm_zar  # noqa: E402
from original import ROOT  # noqa: E402

ENABLE, AIM, LINE, APP = 0x4b6e80, 0x4b7ac0, 0x4b7800, 0x62c40c

probe = ctypes.CDLL(str(next((ROOT / 'build').glob('libpmprobe.*'))))
probe.probe_pm_weapon_cone.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p]
OUT = (ctypes.c_uint32 * 64)()


def fb(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


class Exe:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        a = iter(range(z.alloc(0x4000), 1 << 31, 0x200))
        self.w, self.mgr, self.defn, self.part, self.fp, self.app, self.me = (next(a) for _ in range(7))
        z.native_stub(LINE, self.line, 0x18)

    def wr(self, addr, v):
        self.uc.mem_write(addr, struct.pack('<I', v & 0xffffffff))

    def line(self, u):
        self.log += [1, 1 if u.arg(0) else 0]
        if u.arg(0):
            self.log += list(struct.unpack('<3I', bytes(self.uc.mem_read(u.arg(0), 12))))
        self.wr(u.arg(4), self.ans[1])
        self.wr(u.arg(5), self.ans[2])
        return self.ans[0]

    def run(self, mode, i):
        w, wr = self.w, self.wr
        self.log, self.ans = [], i[0:3]
        self.uc.mem_write(w, bytes(0xb0))
        wr(w + 0x9c, self.mgr)
        wr(self.mgr + 0x84, self.me if i[3] else self.me + 4)
        wr(self.app + 0x14, self.me)
        wr(APP, self.app)
        for k in range(3):
            wr(self.mgr + 0xc + 4 * k, i[11 + k])
        wr(w + 0x30, self.defn)
        self.uc.mem_write(self.defn + 0x6c, bytes([2 if i[4] else 0]))
        wr(w + 0x28, self.part)
        self.uc.mem_write(self.part + 0x49, bytes([1 if i[5] else 0]))
        wr(w + 0x84, self.fp)
        wr(self.fp + 0x68, i[6])
        self.uc.mem_write(w + 0x78, bytes([i[9]]))
        wr(w + 0x98, i[10])
        if mode == 0:
            r = self.z.call(ENABLE, ecx=w, args=(i[7] & 0xffffffff, i[8])) & 0xff
            out = [r, self.uc.mem_read(w + 0x78, 1)[0]]
        else:
            buf = self.z.alloc(0x40)
            r = self.z.call(AIM, ecx=w, args=(self.mgr + 0xc, buf, buf + 0xc, 0, buf + 0x18, buf + 0x1c)) & 0xff
            out = [1 if r else 0]
        return out + self.log


def case(r):
    cosv = r.choice([1.0, 0.9, 0.4735, 0.2, -0.5, float('nan')])
    kept = r.choice([cosv, cosv, 1.0, 0.4735, 0.9])
    floor = r.choice([-1.0, -1.0, 0.0, 0.3, 0.9, 1.0, float('nan')])
    at = [r.uniform(-500, 500) for _ in range(3)]
    return [r.random() < 0.8, fb(cosv), fb(kept), r.random() < 0.6, r.random() < 0.3, r.random() < 0.3,
            r.choice([2, 3, 4, 7]), r.choice([-1, -1, 2, 3, 4]), r.random() < 0.15, r.random() < 0.5, fb(floor),
            *[fb(x) for x in at]]


def main():
    cases = int(sys.argv[1]) if len(sys.argv) > 1 else 4000
    exe = Exe()
    rng = random.Random(6280)
    seen, bad = {}, 0
    for n in range(cases):
        i = [int(x) & 0xffffffff for x in case(rng)]
        for mode in (0, 1):
            e = exe.run(mode, i)
            inp = (ctypes.c_uint32 * 14)(*i)
            k = probe.probe_pm_weapon_cone(mode, inp, OUT)
            p = list(OUT[:k])
            key = (mode, tuple(e))[:1] + (e[0], e[1] if mode == 0 else None, len(e) > (2 if mode == 0 else 1))
            seen[key] = seen.get(key, 0) + 1
            if e != p:
                bad += 1
                if bad <= 5:
                    print(f'FAIL case {n} mode {mode}: in {i}\n  exe  {e}\n  port {p}')
    print(f'{cases} cases (enable and aim), {len(seen)} outcomes, {bad} differ')
    sys.stdout.flush()
    os._exit(1 if bad else 0)


if __name__ == '__main__':
    main()
