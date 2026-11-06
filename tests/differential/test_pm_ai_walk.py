"""PM's walking context against the exe: the constructor 0x4d9870, then its virtual slots on crafted
steerings and vehicles: walk 0x4d9680, walk on 0x4d9740, walk straight 0x4d9710, hold 0x4d97a0, the measure
0x4d92a0, the aim 0x4d92c0, the turn 0x4d9390, the throttle 0x4d9550. Each step's fields compared.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_ai_walk.py [runs]
"""
import ctypes
import math
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import pm_floors  # noqa: E402
import test_pm_zar  # noqa: E402

CTOR, STEER, PLACE, ORDER, BLOCKED, MEASURE, COPYSIGN, FTOL = 0x4d9870, 0x4dc0f0, 0x5cb260, 0x4fa470, 0x4ccaf0, 0x4d92a0, 0x5de5bc, 0x5cef9a
VT = 0x5eec2c
SLOT = {'0': 0x00, '1': 0x04, '2': 0x08, '3': 0x0c, '5': 0x14, '6': 0x1c, '7': 0x20}


def bits(v):
    return struct.unpack('<I', struct.pack('<f', v))[0]


class Exe(test_pm_zar.Emulated):
    def __init__(self):
        super().__init__()
        self.brain = self.alloc(0x500)
        self.ctx = self.alloc(0x60)
        self.vehicle = self.alloc(0x100)
        self.order = self.alloc(0x20)
        self.w32(self.order + 8, self.brain)
        self.target = self.alloc(0x40)
        self.goals = self.alloc(8)
        self.w32(COPYSIGN, self.cpu.code(bytes.fromhex('dd442404 d9e1 f644241380 7402 d9e0 c3')))
        # _ftol, truncating
        self.w32(0x5de644, self.cpu.code(bytes.fromhex('83ec0c d93c24 668b0424 80cc0c 6689442402 d96c2402 df7c2404 d92c24 8b442404 83c40c c3')))
        # The controls at +0x100 + 0: their first word is both the offset table (+4 is 0) and the vtable.
        ctl = self.alloc(0x40)
        self.w32(self.brain + 0x100, ctl)
        self.w32(self.brain + 0x11c, self.vehicle)
        self.w32(ctl + 0x1c, self.cpu.code(bytes.fromhex('d905') + struct.pack('<I', self.goals + 4) + b'\xc3'))
        self.w32(ctl + 0x30, self.cpu.code(bytes.fromhex('d905') + struct.pack('<I', self.goals) + b'\xc3'))
        vt = self.alloc(0x100)
        self.w32(self.vehicle, vt)
        self.pilot = self.alloc(0x600)
        self.want_pilot, self.blocked, self.steers = False, False, True
        self.w32(vt + 0x58, self.ret_stub(lambda c: self.pilot if self.want_pilot else 0))
        self.native_stub(PLACE, lambda c: self.vehicle + 0x80 if c.ecx == self.vehicle else self.target)
        self.native_stub(ORDER, lambda c: 1 if self.steers else 0)
        self.native_stub(BLOCKED, lambda c: 1 if self.blocked else 0)
        for init in (0x4dbf80, 0x4dbfa0, 0x4dbfc0, 0x4dc020, 0x4dc0c0, 0x4dc0e0):  # the statics, as at start-up
            self.call(init)
        self.call(STEER, ecx=self.brain + 0x210)
        # The vehicle's contact: [+0x6c], whose +4 answers the target by vt+0x2c.
        self.contact, holder, hvt = self.alloc(0x20), self.alloc(8), self.alloc(0x40)
        self.w32(self.contact + 4, holder)
        self.w32(holder, hvt)
        self.w32(hvt + 0x2c, self.ret_stub(lambda c: self.target))

    def ret_stub(self, fn):
        a = self.cpu.code(b'\xc3')
        self.native_stub(a, fn)
        return a

    def w32(self, a, v):
        self.cpu._uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def fs(self, a, vals):
        self.cpu._uc.mem_write(a, struct.pack(f'<{len(vals)}f', *vals))

    def g(self, a):
        return struct.unpack('<I', bytes(self.cpu._uc.mem_read(a, 4)))[0]

    def words(self):
        c = self.ctx
        vals = [self.g(c + o) for o in (0x18, 0x1c, 0x20, 0x24, 0x28, 0x2c, 0x30, 0x38, 0x40, 0x44, 0x48, 0x4c)]
        vals.append(self.g(self.brain + 0x210 + 0x84))
        return ' '.join(f'{v:x}' for v in vals) + f' {self.g(c + 0x3c) & 0xff}\n'


def plan(seed):
    r = random.Random(f'walk:{seed}')
    near = r.choice([5, 30, 100])

    def pt():
        return [r.uniform(-near, near), r.uniform(-3, 3), r.uniform(-near, near)]

    def world():
        a = r.uniform(0, 6.283)
        return ('W', pt() + pt() + [math.sin(a), math.cos(a)] + [r.choice([-0.5, 0.0, 0.5]), r.choice([0.0, 0.05, 0.5, -0.5, -1.0]),
                                                                 r.choice([0.3, 1.0])],
                r.choice([0, 0, 1]), r.choice([0, 1]), r.choice([1, 1, 0]), r.choice([0, 1]), r.choice([0.0, 0.5, 1.0, 3.0]))

    def steer():
        a = r.uniform(0, 6.283)
        ln = r.uniform(0, near * 2)
        return ('T', pt() + [math.sin(a), math.cos(a), r.choice([0.0, 0.0, 10.0]), ln, r.uniform(-90, ln), r.uniform(-5, 5),
                             r.uniform(-near, near), r.uniform(-near, near)])
    w0 = world()
    lines = [w0, steer(), ('C',), ('4',)]
    for _ in range(r.randrange(4, 12)):
        k = r.random()
        if k < 0.15:  # the throttle alone, facing the way and near the carrot
            lines.append(('O', [r.uniform(0.9, 1.0), r.uniform(0, 3), r.uniform(0, 3)]))
            lines.append(('7',))
        elif k < 0.3:
            lines.append(world())
        elif k < 0.5:
            lines.append(steer())
            lines.append(('4',))
        else:
            lines.append((r.choice('0123567'),))
    return lines


def run_exe(x, lines):
    out = []
    st = x.brain + 0x210
    made = False
    for ln in lines:
        op = ln[0]
        if op == 'W':
            v = ln[1]
            x.fs(x.vehicle + 0x80, v[0:3])
            x.fs(x.target, v[3:6])
            x.fs(x.vehicle + 0x24, [v[6]])
            x.fs(x.vehicle + 0x2c, [v[7]])
            x.fs(x.goals, [v[8], v[9]])
            x.fs(x.brain + 0x10c, [v[10]])
            x.blocked, x.steers = bool(ln[2]), bool(ln[4])
            x.w32(x.vehicle + 0x6c, x.contact if ln[3] else 0)
            x.want_pilot = bool(ln[5])
            x.fs(x.pilot + 0x544, [ln[6]])
        elif op == 'T':
            v = ln[1]
            x.fs(st + 4, v[0:3])
            x.fs(st + 0x10, v[3:5])
            x.fs(st + 0x18, v[5:9])
            x.fs(st + 0x34, v[9:11])
        elif op == 'O':
            x.fs(x.ctx + 0x40, [ln[1][0]])
            x.fs(x.ctx + 0x18, ln[1][1:])
        elif op == 'C':
            x.call(CTOR, ecx=x.ctx, args=(x.order,))
            x.w32(x.ctx + 0xc, x.vehicle + 0x80)
            made = True
        elif op == '4':
            x.call(MEASURE, ecx=x.ctx)
        else:
            x.call(x.g(VT + SLOT[op]), ecx=x.ctx)
        if made:
            out.append(x.words())
    return ''.join(out)


def text(lines):
    o = []
    for ln in lines:
        if ln[0] == 'O':
            o.append('O ' + ' '.join(f'{bits(u):x}' for u in ln[1]))
        elif ln[0] == 'W':
            v = ln[1]
            o.append('W ' + ' '.join(f'{bits(u):x}' for u in v) + f' {ln[2]} {ln[3]} {ln[4]} {ln[5]} {bits(ln[6]):x}')
        elif ln[0] == 'T':
            o.append('T ' + ' '.join(f'{bits(u):x}' for u in ln[1]))
        else:
            o.append(ln[0])
    return '\n'.join(o) + '\n'


def close(a, b):
    if a == b:
        return True
    x, y = (struct.unpack('<f', struct.pack('<I', int(v, 16)))[0] for v in (a, b))
    return abs(x - y) <= 1e-3 + 1e-4 * max(abs(x), abs(y))


def main():
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 300
    lib = test_pm_zar.load_pm_probe()
    lib.probe_pm_ai_walk.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
    buf = ctypes.create_string_buffer(1 << 20)
    bad = steps = 0
    for seed in range(runs):
        lines = plan(seed)
        a = run_exe(Exe(), lines).split('\n')
        assert lib.probe_pm_ai_walk(text(lines).encode(), buf, len(buf)) >= 0
        b = buf.value.decode().split('\n')
        steps += len(a)
        for i, (p, q) in enumerate(zip(a, b)):
            pw, qw = p.split(), q.split()
            if len(pw) != len(qw) or pw[-1:] != qw[-1:] or not all(close(u, v) for u, v in zip(pw[:-1], qw[:-1])):
                print(f'seed {seed} line {i}:\n  exe  {p}\n  port {q}')
                bad += 1
                break
        else:
            if len(a) != len(b):
                print(f'seed {seed}: {len(a)} lines against {len(b)}')
                bad += 1
        if bad >= 6:
            break
    print(f'{runs} plans, {steps} lines: {bad} differ')
    pm_floors.floors(plans=(runs, 100), lines=(steps, 1000))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
