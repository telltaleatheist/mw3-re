"""The pocket commander's controls against PM's exe: test_pocket moved. The dispatcher 0x46b4d0 makes the mode
(0x46ac20, its owner 0x46bfc0 with the keys pushed), the keys 0x46c3e0 with the player's real 0x4291a0 under them,
the update 0x46bee0, the way out, at 53 bits. The view, HUD, panel, key maps and marker pieces stand in, logged.
Each case: random inputs on the player's controls, the instance them or another, the mech viewed or not, a HUD or
not, powered down or not, the animation state, the mode's +0x174, a few keys, another map over the mode's or not;
the log (the calls, the mode's fields after the start, its axes after the update, the controls given back and
their inputs) must match the port's (mech3/mech_pocket, tests/probe_pocket.cpp).

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_pocket.py
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
from unicorn import UC_HOOK_MEM_WRITE  # noqa: E402

NEW, DELETE = 0x5ce8c6, 0x5ce8c0
DISPATCH, PLAYER, KEYS, UPDATE = 0x46b4d0, 0x428fd0, 0x46c3e0, 0x46bee0
INSTANCE, ACTIVE, VIEW, NET = 0x6293cc, 0x629a38, 0x61874c, 0x62daf0
INPUTS = [(0x3f, 'B'), (0x3e, 'B'), (0x44, 'f'), (0x5a, 'B'), (0x40, 'f'), (0x50, 'B'), (0x10, 'f'), (0x20, 'f'),
          (0x28, 'f'), (0x24, 'f'), (0x14, 'f'), (0x18, 'f'), (0x1c, 'f'), (0x34, 'B'), (0x3c, 'B')]


def fb(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


class Exe:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        self.w = lambda a, v: self.uc.mem_write(a, struct.pack('<I', v & 0xffffffff))
        self.arena = z.alloc(0x400000)
        self.at = self.arena
        self.deleted = []
        z.native_stub(NEW, lambda c: self.zalloc(c.arg(0)))
        z.native_stub(DELETE, lambda c: self.deleted.append(c.arg(0)) or 0)
        self.mech, mvt = self.zalloc(0x800), self.zalloc(0x200)
        self.w(self.mech, mvt)
        self.w(mvt + 0x58, self.ret(lambda c: c.ecx))
        self.w(mvt + 0xac, self.ret(lambda c: self.inp[4]))
        self.w(self.mech + 0x6a8, self.zalloc(0x40))
        self.view, self.camera = self.zalloc(0x100), self.zalloc(0x20)
        self.w(VIEW, self.view)
        self.hud = self.zalloc(0x3000)
        self.w(NET, 0)
        for addr, fn, pop in ((0x4b96e0, lambda c: self.log(1, c.arg(0)), 4), (0x408a70, lambda c: self.log(2, c.arg(0)), 4),
                              (0x4283f0, lambda c: self.log(3, c.arg(0)), 4),
                              (0x430e30, lambda c: self.hud if self.inp[3] else 0, 0),
                              (0x5bba50, lambda c: 0, 4), (0x469dc0, lambda c: self.log(5) or c.ecx, 0x10),
                              (0x46a2b0, lambda c: self.log(6), 0), (0x449cb0, lambda c: self.log(7, c.arg(0) & 0xff), 4),
                              (0x46abd0, lambda c: self.log(8, c.arg(0)) or 1, 4), (0x46b980, lambda c: self.log(10), 4),
                              (0x46cc50, lambda c: self.log(11), 4), (0x40b670, lambda c: self.inp[0], 0),
                              (0x536d00, self.push, 0), (0x536cf0, lambda c: self.maps[-1], 0),
                              (0x536df0, self.pop, 0)):
            z.native_stub(addr, fn, pop)
        self.uc.hook_add(UC_HOOK_MEM_WRITE, self.flag, begin=self.hud + 0x1d78, end=self.hud + 0x1d78)

    def flag(self, uc, access, addr, size, value, user):
        self.out += [4, value & 0xff]

    def push(self, c):
        self.log(12)
        self.maps.append(0x7000 + 16 * len(self.maps))
        return 0

    def pop(self, c):
        self.log(13)
        self.maps.pop()
        return 0

    def zalloc(self, n):
        n = (n + 15) & ~15
        p = self.at
        self.at += n
        self.uc.mem_write(p, bytes(n))
        return p

    def ret(self, fn, pop=0):
        t = self.zalloc(4)
        self.uc.mem_write(t, b'\xc3')
        self.z.native_stub(t, fn, pop)
        return t

    def log(self, *v):
        self.out += [x & 0xffffffff for x in v]
        return 0

    def inputs_of(self, c):
        out = []
        for off, k in INPUTS:
            out.append(bytes(self.uc.mem_read(c + off, 1))[0] if k == 'B' else self.z.r32(c + off))
        return out

    def run(self, inp, vals):
        z, w, r32 = self.z, self.w, self.z.r32
        self.at = self.arena + 0x20000 if self.at > self.arena + 0x300000 else self.at
        self.inp, self.out, self.deleted, self.maps = inp, [], [], [0x7000]
        w(self.view + 0x58, self.camera if inp[1] >= 0 else 0)
        w(self.camera + 4, inp[1])
        w(self.view + 0x64, self.mech if inp[2] else 0x1234)
        w(self.mech + 0x59c, inp[8])
        self.uc.mem_write(self.hud + 0x1d78, b'\0')
        self.out = []
        prev = self.zalloc(0x1c0)
        z.call(PLAYER, prev, [1, 1])  # most derived, as the exe makes them
        for (off, k), v in zip(INPUTS, vals):
            self.uc.mem_write(prev + off, bytes([int(v)]) if k == 'B' else struct.pack('<f', v))
        w(prev + 0x188, self.mech)  # the IControls' owner
        other = self.zalloc(0x1c0)
        w(INSTANCE, other if inp[5] else prev)
        idf = lambda p: 1 if p == prev else 2 if p == other else 3 if p else 0  # noqa: E731
        p = z.call(DISPATCH, args=[prev, 2, 1])
        ok = p not in (0, prev)
        self.out += [0x20, 1 if ok else 0]
        if ok:
            self.out += [r32(p + 0x170), idf(r32(p + 0x178)), idf(r32(p + 0x17c)), r32(p + 0x180), r32(p + 0x198),
                         r32(p + 0x19c), 1 if r32(p + 0x1a0) else 0, int(r32(INSTANCE) == p), int(r32(ACTIVE) == p)]
            self.out += self.inputs_of(p)
            w(p + 0x174, inp[6])
            for cmd in inp[10:10 + inp[7]]:
                z.call(KEYS, p, [cmd])
            self.out.append(0x21)
            z.call(UPDATE, p + 0x1ec, [fb(1 / 30)])
            self.out.append(0x22)
            self.out += [r32(p + 0x4c), bytes(self.uc.mem_read(p + 0x3d, 1))[0], r32(p + 0x24), r32(p + 0x28),
                         bytes(self.uc.mem_read(p + 0x34, 1))[0]]
            if inp[9]:
                self.maps.append(0x6ff0)
            back = z.call(DISPATCH, args=[p, 2, 1])
            self.out += [0x23, idf(back), idf(r32(INSTANCE)), 1 if r32(ACTIVE) else 0, int(r32(prev + 0x188) == self.mech)]
            self.out += self.inputs_of(prev)
        return self.out


def case(rng):
    inp = [rng.choice([640, 800, 1024]), rng.choice([-1, 0, 1, 2, 4]), int(rng.random() < 0.7), int(rng.random() < 0.8),
           int(rng.random() < 0.15), int(rng.random() < 0.3), rng.choice([0, 0, 1, 3])]
    keys = [rng.choice([0x1a, 0x1b, 0x1c, 0x1e, 0x1f, 0x21, 0x23, 0x24, 0x2a, 0x10, 0x5d, 0x5e, 0x5f, 0x60, 0x61])
            for _ in range(rng.randrange(0, 5))]
    inp += [len(keys), rng.choice([0, 1, 5, 10, 11, 12, 13, 13, 14]), int(rng.random() < 0.2)] + keys
    vals = []
    for off, k in INPUTS:
        vals.append(float(rng.choice([0, 1])) if k == 'B' else struct.unpack('<f', struct.pack('<f', rng.uniform(-1, 1)))[0])
    return inp, vals


def main():
    p = test_pm_zar.load_pm_probe()
    P = ctypes.c_void_p
    p.probe_pocket_run.argtypes = [P, P, P, ctypes.c_int]
    rng = random.Random(1)
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 400
    emu = Exe()
    out = (ctypes.c_uint32 * 512)()
    picks = popped = kept = blocked = 0
    for k in range(n):
        inp, vals = case(rng)
        a = emu.run(inp, vals)
        m = p.probe_pocket_run((ctypes.c_int32 * len(inp))(*inp), (ctypes.c_float * 15)(*vals), out, 512)
        b = list(out[:m])
        if a != b:
            i = next((j for j, (x, y) in enumerate(zip(a, b)) if x != y), min(len(a), len(b)))
            print(f'case {k}: differ at {i}\n  exe  {a[max(0, i - 6):i + 8]}\n  port {b[max(0, i - 6):i + 8]}\n  in {inp}\n  {vals}')
            return 1
        picks += a.count(8)
        popped += 13 in a
        kept += inp[9]
        blocked += inp[8] in (12, 13) and any(0x1a <= c <= 0x24 for c in inp[10:])
    print(f'ok: {n} cases, {picks} picks, {popped} keys popped, {kept} left under another map, {blocked} throttle keys in states 12, 13: identical')
    pm_floors.floors(cases=(n, 200), picks=(picks, 20), popped=(popped, 100), kept=(kept, 30), blocked=(blocked, 20))
    return 0


if __name__ == '__main__':
    sys.exit(main())
