"""PM's jump state against the exe: the plan 0x4dac40 with 0x4dad70, the turn 0x4d7060, the check 0x4dade0, the
constructor 0x4daf40, then the run 0x4db0e0 through its phases on crafted worlds. The state's fields, the steering's
and each call to the controls, in order, compared after every step.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_ai_jump.py [runs]
"""
import ctypes
import math
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from unicorn.x86_const import UC_X86_REG_ESP  # noqa: E402

import pm_floors  # noqa: E402
import test_pm_ai_walk as walk  # noqa: E402
import test_pm_zar  # noqa: E402

CTOR, PLAN, TURN, CAN, NEXT, NOW, CLIMB = 0x4daf40, 0x4dac40, 0x4d7060, 0x4dade0, 0x4f9f00, 0x62da90, 0x629314
bits = walk.bits


class Exe(walk.Exe):
    def __init__(self):
        super().__init__()
        self.log = []
        self.state = self.alloc(0x40)
        self.fs(CLIMB, [38.0])
        self.call(0x4daec0)  # the statics, as at start-up
        self.call(0x4daf00)
        # The CRT's _hypot, two doubles, cdecl: fld x; fmul st; fld y; fmul st; faddp; fsqrt
        self.w32(0x5de5b8, self.cpu.code(bytes.fromhex('dd442404 d8c8 dd44240c d8c8 dec1 d9fa c3')))
        self.mech = self.pilot
        mvt = self.alloc(0x200)
        self.w32(self.mech, mvt)
        self.air = True
        self.w32(mvt + 0x150, self.ret_stub(lambda c: 1 if self.air else 0))
        # The step: its next step's way point (vt+0x1c), its margin at +0x10.
        self.step, nxt, nvt = self.alloc(0x20), self.alloc(8), self.alloc(0x40)
        self.way = self.alloc(12)
        self.w32(self.step + 0xc, nxt)
        self.w32(nxt, nvt)
        self.w32(nvt + 0x1c, self.ret_stub(lambda c: self.way))
        self.native_stub(NEXT, self.go_next, pop=4)
        self.order2 = self.alloc(0x10)
        self.w32(self.order2 + 8, self.brain)
        for name, slot in (('throttle', 0x20), ('turn', 0x28), ('forward', 0x3c), ('jump', 0x4c)):
            self.w32(self.ctl + slot, self.arg_stub(name))

    def arg_stub(self, name):
        a = self.cpu.code(b'\xc2\x04\x00')

        def fn(c):
            sp = c._uc.reg_read(UC_X86_REG_ESP)
            self.log.append(f'{name}={self.g(sp + 4):x};')
            return 0
        self.native_stub(a, fn, pop=4)
        return a

    def go_next(self, c):
        self.log.append('next;')
        return 1

    def fields(self):
        s, st = self.state, self.brain + 0x210
        vals = [self.g(s + o) for o in (0x20, 0x24, 0x28, 0x30, 0xc, 0x10, 0x14, 0x18, 0x1c)]
        vals += [self.g(self.brain + 0x2b8 + o) for o in (0, 4, 8)]
        vals += [self.g(st + o) for o in (0x10, 0x14, 0x18, 0x1c, 0x20, 0x2c)]
        return vals


def plan(seed):
    r = random.Random(f'jump:{seed}')
    span = r.choice([20, 100, 400])

    def world(air=None):
        a = r.uniform(0, 6.283) if r.random() < 0.7 else 0.0
        at = [r.uniform(-span, span), r.uniform(0, 40), r.uniform(-span, span)]
        to = [at[0] + r.uniform(-span, span), r.uniform(0, 40), at[2] + r.uniform(-span, span)]
        if r.random() < 0.5:  # facing the way, past the first turn
            n = math.hypot(to[0] - at[0], to[2] - at[2]) or 1.0
            sx, cz = (to[0] - at[0]) / n, (to[2] - at[2]) / n
        else:
            sx, cz = math.sin(a), math.cos(a)
        return ('W', at + to + [sx, cz, r.choice([0.0, 0.0, -0.5, 0.5]), r.choice([0.3, 1.0]), r.choice([5.0, 12.0, 40.0]),
                                r.choice([0.0, -4.0, 3.0]), r.choice([0.0, 5.0, 11.0, -2.0]), r.choice([1.0, 5.0, 20.0])],
                1 if air is None and r.random() < 0.9 else (air or 0), 1, 1)

    lines = [('N', [r.uniform(0, 500)]), world(1), ('C',)]
    now = lines[0][1][0]
    w0 = lines[1]
    for _ in range(r.randrange(10, 40)):
        k = r.random()
        if k < 0.75:
            now += r.choice([0.03, 0.1, 0.5, 2.0])
            lines.append(('N', [now]))
            lines.append(('R', [r.choice([0.033, 0.1])]))
        elif k < 0.85:  # the same world, the mech's place and speeds moved
            v = list(w0[1])
            v[1] += r.uniform(-30, 30)
            v[0] += r.uniform(-20, 20)
            v[11], v[12] = r.choice([0.0, -4.0, 3.0]), r.choice([0.0, 5.0, 11.0, -2.0])
            w0 = ('W', v, r.choice([1, 1, 1, 0]), 1, 1)
            lines.append(w0)
        elif k < 0.9:
            v = [r.uniform(-span, span) for _ in range(6)]
            margin = r.choice([0.0, 1.0, 20.0])
            if r.random() < 0.2:  # no height at all: the plan takes no time
                v[4], margin = v[1], 0.0
            lines.append(('L', v + [r.choice([8.0, 2.0]), r.choice([-29.62, -30.0]), margin, r.choice([0.0, -1.0, 5.0, 40.0, 1000.0])]))
        elif k < 0.97:
            a, b = r.uniform(0, 6.283), r.uniform(0, 6.283) if r.random() < 0.6 else 0.0
            b = a + r.choice([0.0, 0.01, -0.01, 0.1, -0.1, 0.5, -0.5, 2.0, -2.0, b])
            dead = r.choice([(0.99985, 0.01745), (1.0, 0.2)])  # the second: a side within the dead zone, not facing
            lines.append(('T', [math.sin(a), math.cos(a), math.sin(b), math.cos(b), r.choice([0.0, 0.5, -0.5]),
                                dead[0], dead[1], r.choice([1.2, 60.0]), 0.1, r.choice([0.0, 0.0, -0.8])]))
        else:
            lines.append(('J', r.choice([0, 1, 1]), r.choice([0, 1]), r.choice([0, 1, 1])))
    return lines


def run_exe(x, lines):
    out = []
    made = False
    world = None
    for ln in lines:
        op = ln[0]
        x.log = []
        extra = ''
        if op == 'W':
            v = ln[1]
            world = ln
            x.fs(x.vehicle + 0x80, v[0:3])
            x.fs(x.way, v[3:6])
            x.fs(x.vehicle + 0x24, [v[6]])
            x.fs(x.vehicle + 0x2c, [v[7]])
            x.fs(x.goals, [v[8]])
            x.fs(x.brain + 0x10c, [v[9]])
            x.fs(x.k, [v[10]])
            x.fs(x.mech + 0x40, [v[11]])
            x.fs(x.mech + 0x44, [v[12]])
            x.fs(x.step + 0x10, [v[13]])
            x.air = bool(ln[2])
            x.fs(x.mech + 1000, [0.0])
            x.w32(x.mech + 1000, 1 if ln[3] else 0)
            x.want_pilot = bool(ln[4])
        elif op == 'N':
            x.fs(NOW, ln[1])
        elif op == 'C':
            x.call(CTOR, ecx=x.state, args=(x.step, x.order2))
            made = True
        elif op == 'R':
            x.call(x.g(x.g(x.state) + 0x14), ecx=x.state, args=(bits(ln[1][0]),))
        elif op == 'J':
            b = x.alloc(0x200)
            ctl = x.alloc(8)
            x.w32(b + 0x100, ctl)
            x.w32(b + 0x11c, x.vehicle if ln[1] else 0)
            jets, pilot = x.g(x.mech + 1000), x.want_pilot
            x.w32(x.mech + 1000, ln[2])
            x.want_pilot = bool(ln[3])
            extra = f'can {x.call(CAN, args=(b,)) & 0xff}'
            x.w32(x.mech + 1000, jets)
            x.want_pilot = pilot
        elif op == 'L':
            v = ln[1]
            o, a, b = x.alloc(0x3c), x.alloc(12), x.alloc(12)
            x.fs(a, v[0:3])
            x.fs(b, v[3:6])
            x.call(PLAN, ecx=o, args=(a, b, bits(v[6]), bits(v[7]), bits(v[8]), bits(v[9])))
            extra = ' '.join(f'{x.g(o + i):x}' for i in range(0, 0x3c, 4)) + ' '
        elif op == 'T':
            v = ln[1]
            f, t, p, dot, cross = x.alloc(12), x.alloc(12), x.alloc(0x14), x.alloc(4), x.alloc(4)
            x.fs(f, [v[0], 0.0, v[1]])
            x.fs(t, [v[2], 0.0, v[3]])
            x.fs(p, v[5:10])
            x.fs(cross, [7.0])  # what it held
            ret = x.alloc(4)
            x.call(x.cpu.code(b'\xff\x74\x24\x18\xff\x74\x24\x18\xff\x74\x24\x18\xff\x74\x24\x18\xff\x74\x24\x18\xff\x74\x24\x18'
                              + b'\xb8' + struct.pack('<I', TURN) + b'\xff\xd0\x83\xc4\x18\xd9\x1d' + struct.pack('<I', ret) + b'\xc3'),
                   args=(f, t, dot, cross, bits(v[4]), p))
            extra = ret, dot, cross
        vals = ' '.join(f'{v:x}' for v in x.fields()) + ' ' if made else ''
        if isinstance(extra, tuple):
            extra = f'{x.g(extra[0]):x} {x.g(extra[1]):x} {x.g(extra[2]):x} '
        head = f'{x.g(x.state + 8)} {x.g(x.state + 0x2c) & 0xff} ' if made else ''
        out.append(f'{vals}{extra} | {head}x588:{int(x.g(x.mech + 0x588) != 0)} {"".join(x.log)}\n')
    return ''.join(out)


def text(lines):
    o = []
    for ln in lines:
        if ln[0] == 'W':
            o.append('W ' + ' '.join(f'{bits(u):x}' for u in ln[1]) + f' {ln[2]} {ln[3]} {ln[4]}')
        elif ln[0] in 'NRLT':
            o.append(ln[0] + ' ' + ' '.join(f'{bits(u):x}' for u in ln[1]))
        elif ln[0] == 'J':
            o.append(f'J {ln[1]} {ln[2]} {ln[3]}')
        else:
            o.append(ln[0])
    return '\n'.join(o) + '\n'


def main():
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 300
    lib = test_pm_zar.load_pm_probe()
    lib.probe_pm_ai_jump.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
    buf = ctypes.create_string_buffer(1 << 21)
    bad = steps = 0
    phases, nexts = {}, 0
    for seed in range(runs):
        lines = plan(seed)
        a = run_exe(Exe(), lines).split('\n')
        assert lib.probe_pm_ai_jump(text(lines).encode(), buf, len(buf)) >= 0
        b = buf.value.decode().split('\n')
        steps += len(a)
        for i, (p, q) in enumerate(zip(a, b)):
            pf, _, pe = p.partition('|')
            qf, _, qe = q.partition('|')
            pw, qw = pf.split(), qf.split()
            same = len(pw) == len(qw) and all(close(u, v) for u, v in zip(pw, qw))
            if pf.startswith('can') or qf.startswith('can') or ' can ' in pf:
                same = pf.split() == qf.split()
            if not same or not same_tail(pe, qe):
                print(f'seed {seed} line {i} {lines[i][0] if i < len(lines) else ""}:\n  exe  {p}\n  port {q}')
                bad += 1
                break
            ex = pe.split()
            if ex and ex[0].isdigit():
                phases[ex[0]] = phases.get(ex[0], 0) + 1
            nexts += 'next;' in pe
        else:
            if len(a) != len(b):
                print(f'seed {seed}: {len(a)} lines against {len(b)}')
                bad += 1
        if bad >= 6:
            break
    print(f'{runs} plans, {steps} lines: {bad} differ; lines in each phase {sorted(phases.items())}, {nexts} ended')
    pm_floors.floors(plans=(runs, 100), lines=(steps, 1000), ended=(nexts, 5),
                     **{f'phase_{k}': (phases.get(str(k), 0), 5) for k in range(1, 8)})
    return 1 if bad else 0


def close(a, b):
    if a == b:
        return True
    try:
        x, y = (struct.unpack('<f', struct.pack('<I', int(v, 16)))[0] for v in (a, b))
    except ValueError:
        return False
    if x != x or y != y:
        return x != x and y != y
    if math.isinf(x) or math.isinf(y):  # the allowance would be infinite too
        return False
    return abs(x - y) <= 1e-3 + 1e-4 * max(abs(x), abs(y))


def same_tail(p, q):
    """The phase, the flags and the calls in order, their arguments within the tolerance."""
    pw, qw = p.split(), q.split()
    if len(pw) != len(qw):
        return False
    for u, v in zip(pw, qw):
        if u == v:
            continue
        cu, cv = u.split(';'), v.split(';')
        if len(cu) != len(cv):
            return False
        for a, b in zip(cu, cv):
            na, _, va = a.partition('=')
            nb, _, vb = b.partition('=')
            if na != nb or not close(va or '0', vb or '0'):
                return False
    return True


if __name__ == '__main__':
    sys.exit(main())
