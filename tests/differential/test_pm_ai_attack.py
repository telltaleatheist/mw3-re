"""PM's attack state 0x4dedf0 against mech3/zai_attack_pm: every step of its state machine on random flags and
worlds. The context 0x4debe0, the fighting state's step 0x4d9d00 and enter 0x4d9be0 are stubbed alike (test_pm_ai_walk
has those); the deque, the order chain and the two list pushes are real memory or logged. Fields compared bit for bit.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_ai_attack.py [runs]
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
os.environ['MW3_X87'] = '53'  # the reference's precision, only: docs/pm-tests.md
import pm_floors  # noqa: E402
import test_pm_zar  # noqa: E402

ATTACK, NOW, SCALE = 0x4dedf0, 0x62da90, 0x62cc60
OFF = dict(mode=0x3c, kind=0x40, delay=0x4c, x58=0x58, x5c=0x5c, x60=0x60, xb4=0xb4, xfc=0xfc, place=0x104, x108=0x108, x1f0=0x1f0)


def u(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


def f(v):
    return struct.unpack('<f', struct.pack('<I', v))[0]


class Exe(test_pm_zar.Emulated):
    def __init__(self):
        super().__init__()
        a = self.alloc
        self.state, self.order, self.brain, self.vehicle = a(0x60), a(0x20), a(0x200), a(0x10)
        self.vvt, self.pos, self.place, self.log = a(0x100), a(12), a(12), []
        self.poke(self.vehicle, self.vvt)
        self.poke(self.state + 4, self.order)
        self.speedv = a(8)

        def obj(slots):
            o, vt = a(0x20), a(0x100)
            self.poke(o, vt)
            for off, fn in slots.items():
                g = self.cpu.code(b'\xc3')
                self.poke(vt + off, g)
                self.native_stub(g, fn, 0)
            return o
        # vt+0xa4: st0 the speed, in the register at full width
        self.poke(self.vvt + 0xa4, self.cpu.code(bytes.fromhex('dd05') + struct.pack('<I', self.speedv) + b'\xc3'))
        self.beh = obj({0x1c: lambda c: self.place})
        self.elem = obj({0x74: lambda c: self.beh})
        self.net_o = obj({})
        self.prior = obj({0x18: lambda c: self.net_o if self.i['net'] else 0})
        tgt = obj({})
        t3 = obj({0x50: lambda c: tgt})
        t2 = obj({0x18: lambda c: t3})
        t1 = obj({0x1c: lambda c: t2})
        self.tgt = tgt
        self.poke(self.order + 0xc, t1)
        # the deque's blocks: one block; two back is the order that holds the prior net
        self.block, self.map = a(0x84 * 31), a(8)
        self.poke(self.map, self.block)
        self.poke(self.block + 0x84 * 0 + 0xc, self.prior)
        self.poke(self.brain + 0x13c, self.block)
        self.poke(self.brain + 0x140, self.block + 0x84 * 31)
        self.poke(self.brain + 0x144, self.block + 0x84 * 2)
        self.poke(self.brain + 0x148, self.map)
        s = self.native_stub
        s(0x4debe0, self.make_context, 8)
        s(0x4d9d00, lambda c: self.note(1), 8)
        s(0x4d9be0, lambda c: self.note(2), 4)
        s(0x4e5580, lambda c: self.nearest(c), 4)
        s(0x4c7800, lambda c: self.note(4, c.arg(0)) or (self.beh if self.i['beh'] else 0), 4)
        s(0x4ea0f0, lambda c: self.pushed(5, c), 4)
        s(0x4ea180, lambda c: self.pushed(6, c), 4)
        s(0x5ce8c6, lambda c: self.alloc(c.arg(0)), 0)

    def poke(self, a, v):
        self.cpu._uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def fw(self, a, v):
        self.cpu._uc.mem_write(a, struct.pack('<f', v))

    def fr(self, a):
        return struct.unpack('<I', bytes(self.cpu._uc.mem_read(a, 4)))[0]

    def note(self, a, b=0, c=0):
        self.log += [a, b & 0xffffffff, c & 0xffffffff]
        return 0

    def make_context(self, c):
        ctx = c.ecx
        self.poke(ctx + 4, self.brain)
        self.poke(ctx + 8, self.vehicle)
        self.poke(ctx + 0xc, self.pos)
        self.cpu._uc.mem_write(ctx + 0x6a, bytes([self.i['no_target']]))
        return ctx

    def nearest(self, c):
        self.note(3, 1 if c.ecx == self.net_o else 0, self.fr(c.arg(0)))
        return self.elem if self.i['near'] else 0

    def pushed(self, code, c):
        o = c.arg(0)
        vt = self.fr(o)
        if code == 5:
            assert vt == 0x5eeca4 and self.fr(o + 4) == self.tgt and self.fr(o + 8) == self.beh
            self.note(5, 1, 1)
        elif vt == 0x5ef254:
            self.note(6, 1 if self.fr(o + 4) == self.beh else 2)
        else:
            assert vt == 0x5ef250
            self.note(6, 0)
        return 0

    def run(self, i, x):
        self.i, self.log = i, []
        st, b = self.state, self.brain
        self.poke(st + 8, i['flags'])
        self.fw(st + 0x10, x['r10']), self.fw(st + 0x14, x['r14']), self.fw(st + 0x18, x['r18'])
        self.poke(st + 0x3c, self.place if i['held'] else 0)
        self.poke(st + 0x40, self.beh if i['hnet'] else 0)
        for k in range(3):
            self.fw(st + 0x44 + 4 * k, x['hold'][k])
            self.fw(self.pos + 4 * k, x['pos'][k])
            self.fw(self.place + 4 * k, x['place'][k])
        for k in ('mode', 'kind', 'x5c', 'x60'):
            self.poke(b + OFF[k], i[k])
        for k in ('delay', 'x58', 'xb4', 'xfc', 'x108', 'x1f0'):
            self.fw(b + OFF[k], x[k])
        self.poke(b + OFF['place'], self.pos)
        self.poke(b + 0x154, i['orders'])
        self.cpu._uc.mem_write(self.speedv, struct.pack('<d', x['speed']))
        self.fw(NOW, x['now'])
        self.fw(SCALE, x['scale'])
        self.call(ATTACK, ecx=st, args=(u(x['dt']),))
        place = self.fr(b + OFF['place'])
        return (self.log, [self.fr(st + 8), self.fr(b + 0x40), self.fr(b + 0x5c), self.fr(b + 0x60), self.fr(b + 0x3c), 1 if self.fr(st + 0x40) else 0, int(self.fr(st + 0x3c) != 0), 0 if place == 0 else 1 if place == st + 0x44 else 2],
                [self.fr(b + OFF[k]) for k in ('xfc', 'x108', 'x1f0')] + [self.fr(st + o) for o in (0x10, 0x14, 0x18, 0x44, 0x48, 0x4c)])


def main():
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    lib = test_pm_zar.load_pm_probe()
    P = ctypes.c_void_p
    lib.probe_pm_ai_attack.argtypes = [P, P, ctypes.c_double, P, P, P]
    x = Exe()
    r = random.Random(7)
    seen = {}
    bad = 0
    for t in range(runs):
        i = dict(no_target=int(r.random() < .3), orders=r.choice([0, 1, 2, 3]), net=int(r.random() < .8), near=int(r.random() < .8), beh=int(r.random() < .7),
                 flags=r.choice([0, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0xf0]) | r.choice([0, 1, 2, 3, 4, 5, 6, 7, 10]), kind=r.randrange(8), x5c=r.choice([0, 1]), x60=r.choice([0, 1]),
                 mode=r.choice([0, 1, 2]), held=r.choice([0, 1]), hnet=r.choice([0, 1]))
        v = dict(speed=r.uniform(0, 60), now=r.uniform(0, 500), scale=r.choice([0.3, 0.5, 1.0]), delay=r.uniform(0, 9), x58=r.uniform(0, 9), xb4=r.uniform(0, 9), xfc=r.uniform(0, 9), x108=r.uniform(0, 9),
                 x1f0=r.uniform(0, 9), pos=[r.uniform(-500, 500) for _ in range(3)], place=[r.uniform(-500, 500) for _ in range(3)], r10=1.5, r14=2.5, r18=3.5, hold=[1.0, 2.0, 3.0], dt=0.03)
        v = {k: ([f(u(e)) for e in w] if isinstance(w, list) else (w if k == 'speed' else f(u(w)))) for k, w in v.items()}
        a = x.run(i, v)
        ins = (ctypes.c_int32 * 13)(i['no_target'], i['orders'], 1 if i['net'] else 0, 1 if i['near'] else 0, 1, 1 if i['beh'] else 0, i['flags'], i['kind'], i['x5c'], i['x60'], i['mode'], 1 if i['hnet'] else 0, i['held'])
        fl = [v['speed'], v['now'], v['scale'], v['delay'], v['x58'], v['xb4'], v['xfc'], v['x108'], v['x1f0']] + v['pos'] + v['place'] + [v['r10'], v['r14'], v['r18']] + v['hold']
        fin = (ctypes.c_float * len(fl))(*fl)
        out, fo, log = (ctypes.c_int32 * 8)(), (ctypes.c_float * 9)(), (ctypes.c_uint32 * 30)()
        n = lib.probe_pm_ai_attack(ins, fin, v['speed'], out, fo, log)
        calls = []
        for k in range(n):
            c = list(log[3 * k:3 * k + 3])
            calls += c
        got = (calls, [int(out[0]) & 0xffffffff] + list(out[1:]), [u(z) for z in fo])
        got[1][5] = 1 if got[1][5] else 0
        if a != got:
            print(f'{t} {i}\n  exe  {a}\n  port {got}')
            bad += 1
            if bad > 4:
                break
        seen[i['flags'] & 0xf0, a[1][0] & 0xf0] = seen.get((i['flags'] & 0xf0, a[1][0] & 0xf0), 0) + 1
    print(f'{runs} steps: {bad} differ; transitions {sorted(seen)}')
    pm_floors.floors(steps=(runs, 1000), transitions=(len(seen), 8))
    return 1 if bad else 0


sys.exit(main())
