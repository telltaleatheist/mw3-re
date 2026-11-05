"""PM's AMS frame 0x41a020 (row #311, docs/pm-mech-step.md) with its find 0x4b7c90 and fire 0x4b7d40 against the port's
update_ams, on random shots: who is hostile to whom, range, rounds, the period, a stale target.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_ams.py
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import pm_floors  # noqa: E402
import test_pm_zar  # noqa: E402

UPDATE, BLIPS, CLOCK, MISSION = 0x41a020, 0x62f82c, 0x8c1b50, 0x62c40c
SHOT_HEALTH = 0x63c
probe = test_pm_zar.load_pm_probe()
probe.probe_pm_ams.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int32, ctypes.c_void_p, ctypes.c_void_p]


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


class Emulated:
    def __init__(self):
        e = self.e = test_pm_zar.Emulated()
        self.uc = e.cpu._uc
        a = e.alloc
        self.mech, self.vt, self.player, self.mission, self.ams = a(0x800), a(0x200), a(0x400), a(0x80), a(0x20)
        self.obj, self.ovt, self.team_obj, self.pos, self.head = a(8), a(0x40), a(0x100), a(0x10), a(12)
        self.shots = [a(0x700) for _ in range(8)]
        self.nodes = [a(12) for _ in range(8)]
        self.recs = [a(8) for _ in range(8)]
        self.w(self.mech, self.vt), self.w(self.obj, self.ovt)
        self.w(BLIPS, self.head)
        self.w(MISSION, self.mission)
        self.w(self.mech + 0x400, self.ams)
        self.vstub(self.vt + 0x24, lambda c: self.obj)
        self.vstub(self.ovt + 0x28, lambda c: self.team_obj)
        e.native_stub(0x5cb260, lambda c: self.pos)

    def w(self, a, v):
        self.uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def wf(self, a, *v):
        self.uc.mem_write(a, struct.pack(f'<{len(v)}f', *v))

    def rf(self, a):
        return struct.unpack('<f', bytes(self.uc.mem_read(a, 4)))[0]

    def r(self, a):
        return struct.unpack('<I', bytes(self.uc.mem_read(a, 4)))[0]

    def vstub(self, slot, fn):
        addr = self.e.cpu.code(b'\xc3')
        self.w(slot, addr)
        self.e.native_stub(addr, fn, 0)

    def run(self, s):
        f, i, blips = s['f'], s['i'], s['blips']
        self.w(self.mission + 0x14, self.player if i[0] else 0)
        self.w(self.player + 0xc8, i[5])
        self.w(self.team_obj + 0xc0, i[4])
        self.w(self.mech + 0x3bc, i[1])
        self.wf(self.mech + 0x68, f[6])
        self.wf(self.pos, f[3], f[4], f[5])
        self.uc.mem_write(self.ams, bytes([i[2] != 0]) + bytes(3))
        self.wf(self.ams + 4, f[0], f[1], f[2])
        self.w(self.ams + 0x14, self.shots[i[6]] if i[6] >= 0 else 0)
        self.w(self.ams + 0x18, i[3])
        self.wf(CLOCK, f[7])
        prev = self.head
        for k, b in enumerate(blips):
            self.wf(self.shots[k] + 0x48, b[0], 0.0, b[1])
            self.wf(self.shots[k] + SHOT_HEALTH, b[2])
            self.w(self.recs[k], self.shots[k]), self.uc.mem_write(self.recs[k] + 4, bytes([b[3] != 0]))
            self.w(self.nodes[k] + 8, self.recs[k])
            self.w(prev, self.nodes[k])
            prev = self.nodes[k]
        self.w(prev, self.head)
        r = self.e.call(UPDATE, ecx=self.mech) & 0xff
        target = self.r(self.ams + 0x14)
        at = self.shots.index(target) if target in self.shots[:len(blips)] else -1
        return [r, self.rf(self.ams + 8), float(struct.unpack('<i', bytes(self.uc.mem_read(self.ams + 0x18, 4)))[0]), float(at)] + [
            self.rf(self.shots[k] + SHOT_HEALTH) for k in range(len(blips))]


def scenario(r):
    n = r.randrange(0, 7)
    x, z = r.uniform(-50, 50), r.uniform(-50, 50)
    blips = [(f32(x + r.uniform(-90, 90)), f32(z + r.uniform(-90, 90)), f32(r.uniform(1, 9)), r.randrange(2)) for _ in range(n)]
    clock, nxt = f32(r.uniform(0, 100)), f32(r.uniform(0, 100))
    f = [f32(r.choice([40.0, 80.0, r.uniform(1, 200)])), nxt if r.random() < 0.3 else f32(clock - 1), f32(r.uniform(0.1, 3)),
         f32(x), f32(r.uniform(0, 40)), f32(z), f32(r.uniform(2, 15)), clock]
    i = [int(r.random() < 0.9), r.choice([0, 1, 1, 1, 2, 3, -1]), int(r.random() < 0.85), r.choice([0, 1, 5, 20, 20, -1]),
         r.choice([1, 2, 4]), r.choice([0, 1, 2, 3, 4, 6]), r.randrange(-1, n) if n and r.random() < 0.2 else -1]
    return dict(f=f, i=i, blips=blips)


def main():
    emu = Emulated()
    rng = random.Random(0x41a020)
    fired = found_none = enemy = own = trials = 0
    for _ in range(20000):
        s = scenario(rng)
        want = emu.run(s)
        n = len(s['blips'])
        fl = (ctypes.c_float * 8)(*s['f'])
        il = (ctypes.c_int32 * 7)(*s['i'])
        bl = (ctypes.c_float * max(1, 4 * n))(*[float(v) for b in s['blips'] for v in b])
        out = (ctypes.c_float * (3 + max(n, 1)))()
        r = probe.probe_pm_ams(fl, il, n, bl, out)
        got = [r & 0xff] + list(out[:3 + n])
        if want != got:
            raise SystemExit(f'{trials}: {s}\n  exe  {want}\n  port {got}')
        trials += 1
        fired += want[0]
        found_none += s['i'][0] and s['i'][1] > 0 and s['i'][2] and not want[0]
        enemy += want[0] and (s['i'][5] & s['i'][4]) != 0
        own += want[0] and (s['i'][5] & s['i'][4]) == 0
    pm_floors.floors(trials=(trials, 20000), fired=(fired, 2500), found_none=(found_none, 1000), enemy=(enemy, 500), own=(own, 500))
    print(f'ok: {trials} frames identical; {fired} fired ({enemy} an enemy AMS, {own} ours), {found_none} found nothing')


if __name__ == '__main__':
    try:
        main()
    except SystemExit as ex:
        if isinstance(ex.code, str):
            print(ex.code)
            sys.stdout.flush()
        import os
        os._exit(0 if ex.code in (None, 0) else 1)
    sys.stdout.flush()
    import os
    os._exit(0)
