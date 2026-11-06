"""PM's mech health 0x41c740 with what it now does when it works the health out: how bad the limp is 0x418540 and the
top speed from it 0x418640; the speed setter 0x414c00; the limp's start 0x41e3b0 and MASC 0x41eec0, which call them;
the destroy valve 0x4f5210 with the kill 0x41de00 (vt+0x78), and the torso's end 0x41de30 (docs/damage.md,
docs/pm-mech-step.md).

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_health.py [trials]

A mech in emulator memory with a copy of the real vtable and controls; the parts answer their structure from a table.
The answer, the fields and every call out must match at 24-bit precision, floats within 1e-6.
"""
import math
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
from original import FTOL, IAT_FTOL  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402

probe = test_pm_zar.load_pm_probe()
P = ctypes.c_void_p
probe.probe_pm_health.argtypes = [ctypes.c_int, ctypes.c_int, P, P, ctypes.c_uint32, P, P, P, P, ctypes.c_int]
HEALTH, START_LIMP, MASC, DESTROY, TORSO_END = 0x41c740, 0x41e3b0, 0x41eec0, 0x4f5210, 0x41de30
MECH_VT, CTL_VT = 0x5dfaac, 0x5bd428
SLOTS = {0: 0x3b4, 1: 0x39c, 2: 0x3a0, 3: 0x3a4, 4: 0x3a8, 5: 0x3ac}


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


class Emulated(test_pm_zar.Emulated):
    def __init__(self):
        super().__init__()
        cpu = self.cpu
        cpu.import_code(IAT_FTOL, FTOL)
        a = self.alloc
        self.mech, self.mvt, self.pilot, self.cfg = a(0x800), a(0x300), a(0x300), a(0x40)
        self.ctl, self.vbt, self.ret = a(0x74), a(8), a(4)
        self.anims = [a(0xdc) for _ in range(13)]
        self.parts, self.table = [], None
        self.wr(self.mvt, self.raw(MECH_VT, 0x300))
        for k in range(6):
            part, vt = a(0x20), a(0x60)
            self.parts.append(part)
            self.w32(part, vt)
            f = cpu.code(bytes.fromhex('d905') + struct.pack('<I', self.ret) + bytes.fromhex('c20400'))
            self.w32(vt + 0x48, f)
            cpu._uc.hook_add(UC_HOOK_CODE, lambda uc, ad, sz, d, k=k: self.answer(k), begin=f, end=f)
            for off, code in ((0x3c, 5), (0x40, 6)):
                g = cpu.code(b'\xc3')
                self.w32(vt + off, g)
                self.native_stub(g, lambda c, k=k, code=code: self.log(code, k, c.arg(0), c.arg(1)), 8)
        self.native_stub(0x41fc00, lambda c: self.log(1, self.anims.index(c.ecx), c.arg(0)), 4)
        self.native_stub(0x432d40, lambda c: self.log(2, c.arg(1)), 8)
        self.native_stub(0x420f40, lambda c: self.log(3, self.anims.index(c.arg(0)), self.anims.index(c.arg(1))) or self.anims[12], 0)
        self.native_stub(0x41f310, lambda c: self.log(4, c.arg(0)), 0)
        self.iface = cpu.tr(CTL_VT, 4)
        # The destroy valve 0x4f5210: a valve whose value (vt+4) matches, a name that finds the mech.
        self.action, self.valve, self.value, self.vvt = a(0x10), a(0x10), a(8), a(0x10)
        self.w32(self.action + 8, 7)
        self.w32(self.valve + 4, self.value), self.w32(self.value, self.vvt)
        self.w32(self.vvt + 4, cpu.code(b'\xb8\x07\x00\x00\x00\xc3'))
        self.native_stub(0x4e77d0, lambda c: self.mech, 0)
        self.player = a(0x40)
        self.w32(0x62c40c, self.player)
        self.cpu.stubs[0x406cb0] = (lambda c: 0, 0)

    def answer(self, k):
        section = self.cpu.arg(0)
        v = self.table[0] if k == 0 else self.table[k] if k < 5 else self.table[5 + section]
        self.wr(self.ret, struct.pack('<f', v))

    def raw(self, a, n):
        return bytes(self.cpu._uc.mem_read(a, n))

    def wr(self, a, b):
        self.cpu._uc.mem_write(a, bytes(b))

    def w32(self, a, v):
        self.wr(a, struct.pack('<I', v & 0xffffffff))

    def log(self, *e):
        self.events.append(len(e))
        self.events.extend(v & 0xffffffff for v in e)
        return 0

    def run(self, s):
        f, i, m = s['f'], s['i'], self.mech
        self.table, self.events = f[:8], []
        self.wr(m, bytes(0x800))
        self.w32(m, self.mvt)
        for k, off in SLOTS.items():
            self.w32(m + off, self.parts[k])
        self.wr(self.pilot + 0xe4, struct.pack('<ff', f[9], f[10]))
        self.wr(self.pilot + 0x22c, struct.pack('<f', f[11]))
        self.w32(self.pilot + 0xc, i[6])
        self.w32(m + 0x6a8, self.pilot)
        self.w32(self.cfg + 0x14, i[8]), self.w32(self.cfg + 0x24, i[7])
        self.w32(m + 0x3fc, self.cfg)
        self.wr(self.ctl, bytes(0x74))
        self.wr(self.ctl + 0x10, struct.pack('<f', f[16]))
        self.w32(self.ctl + 4, self.vbt), self.w32(self.vbt + 4, 0x5c), self.w32(self.ctl + 0x60, self.iface), self.w32(self.ctl + 0x70, m)
        self.w32(m + 0x38c, self.ctl)
        self.wr(m + 0x41c, struct.pack('<f', f[8]))
        self.wr(m + 0x420, bytes([i[0]]))
        self.w32(m + 0x59c, i[1])
        self.wr(m + 0x654, bytes([i[2]]))
        self.wr(m + 0x5ae, bytes([1]))
        self.w32(m + 0x5d0, i[3])
        self.w32(m + 0x3ec, i[4])
        self.wr(m + 0x540, struct.pack('<f', f[12]))
        self.wr(m + 0x528, struct.pack('<f', f[13]))
        self.wr(m + 0x520, struct.pack('<f', f[14]))
        self.wr(m + 0x598, struct.pack('<f', f[15]))
        self.w32(self.player + 0x14, m if i[5] else 0)
        for k in range(13):
            self.w32(m + 0x434 + 4 * k, self.anims[k] if s['present'] >> k & 1 else 0)
        op, arg = s['op'], s['arg']
        if op == 0:
            self.call(HEALTH, ecx=m)
            r = struct.unpack('<f', self.raw(m + 0x41c, 4))[0]  # what it returns on the x87
        elif op == 1:
            r = 1.0 if self.call(START_LIMP, ecx=m, args=(arg, 0)) & 0xff else 0.0
        elif op == 2:
            self.call(MASC, ecx=m, args=(arg,))
            r = 0.0
        elif op == 3:
            self.call(DESTROY, ecx=self.action, args=(self.valve,))
            r = 0.0
        else:
            self.call(TORSO_END, ecx=m, args=(1,))
            r = 0.0
        out = [struct.unpack('<f', self.raw(m + o, 4))[0] for o in (0x41c, 0x540, 0x528, 0x520, 0x598)]
        out += [struct.unpack('<f', self.raw(self.ctl + 0x10, 4))[0], float(self.raw(m + 0x420, 1)[0]),
                float(self.raw(m + 0x654, 1)[0]), float(self.raw(m + 0x5ae, 1)[0])]
        return r, out, struct.unpack('<i', self.raw(m + 0x5d0, 4))[0], self.events


def port(s):
    fa = (ctypes.c_float * 17)(*s['f'])
    ia = (ctypes.c_int32 * 9)(*s['i'])
    r, limp, out, log = ctypes.c_float(), ctypes.c_int32(), (ctypes.c_float * 9)(), (ctypes.c_int32 * 512)()
    n = probe.probe_pm_health(s['op'], s['arg'], fa, ia, s['present'], ctypes.byref(r), out, ctypes.byref(limp), log, 512)
    return r.value, list(out), limp.value, [v & 0xffffffff for v in log[:n]]


def scenario(rng):
    leg = rng.choice([0.0, 0.1, 0.125, 0.2, 0.3, 0.5, 0.6, 1.0, rng.random()])
    table = [f32(rng.choice([1.0, rng.random()])) for _ in range(8)]
    table[1] = f32(leg)
    w = rng.choice([rng.uniform(0.2, 0.6), 3.0])   # 3: over the top speed's cap of 60
    f = table + [f32(rng.random()), f32(w), f32(w), f32(rng.uniform(5, 20)),
                 f32(rng.random()), f32(rng.choice([20.0, 40.0, 70.0])), f32(rng.uniform(10, 40)),
                 f32(rng.choice([0.0, 0.0, 0.001])), f32(rng.uniform(-1, 1))]
    i = [int(rng.random() < 0.8), rng.choice([0, 5, 10]), int(rng.random() < 0.15), rng.choice([-1, -1, 0, 1]),
         rng.choice([0, 1, 2]), int(rng.random() < 0.5), rng.randrange(25), rng.randrange(12), rng.randrange(25)]
    return {'op': rng.choice([0, 0, 0, 1, 2, 3, 4]), 'arg': rng.randrange(2), 'f': f, 'i': i,
            'present': rng.choice([0x1fff, 0x1ffc, 0x17f3, 0x1ff1, 0x1ff2])}


def close(a, b):
    if a == b:
        return True
    return math.isfinite(a) and math.isfinite(b) and abs(a - b) <= 1e-6 * max(1.0, abs(a), abs(b))


def main():
    trials = int(sys.argv[1]) if len(sys.argv) > 1 else 20000
    emu, rng = Emulated(), random.Random(0x41c740)
    seen, capped = set(), 0
    for t in range(trials):
        s = scenario(rng)
        w, g = emu.run(s), port(s)
        ok = close(w[0], g[0]) and all(close(a, b) for a, b in zip(w[1], g[1])) and w[2] == g[2] and w[3] == g[3]
        if not ok:
            raise SystemExit(f'trial {t}: op {s["op"]} arg {s["arg"]} i {s["i"]}\n  exe  {w}\n  port {g}')
        seen.add((s['op'], w[1][1] != s['f'][12], w[2]))
        capped += w[1][2] == 60.0
    if not capped:
        raise SystemExit('coverage: the cap never reached')
    pm_floors.floors(calls=(trials, 20000), kinds=(len(seen), 23))
    print(f'ok: {trials} calls agree; {len(seen)} kinds; {capped} at the top speed cap of 60')


if __name__ == '__main__':
    main()
