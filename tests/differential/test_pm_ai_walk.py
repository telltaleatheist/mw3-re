"""PM's walking context against the exe: the constructor 0x4d9870, then its virtual slots on crafted
steerings and vehicles: walk 0x4d9680, walk on 0x4d9740, walk straight 0x4d9710, hold 0x4d97a0, the measure
0x4d92a0, the aim 0x4d92c0, the turn 0x4d9390, the throttle 0x4d9550. Then a fighting state's part: its
constructor 0x4d9b10, enter 0x4d9be0, reset 0x4d9ce0, release 0x4d9cc0, and its context 0x4d99a0 with the
measures 0x4d9a90, 0x4d9920, 0x4d99e0, the decision 0x4d9b90, the plan 0x4da1c0, the order's target 0x4daa70, and the walk's step 0x4d9d00.
Then the guarding context 0x4dfb50 with its measure 0x4dfc30. On any of them the drive 0x4dcd20 with the turns 0x4d7060,
0x4d71e0, and the away 0x4dccf0. Each step's fields compared.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_ai_walk.py [runs]
"""
import ctypes
import math
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
os.environ['MW3_X87'] = '53'  # the reference's precision, only: docs/pm-tests.md
from unicorn.x86_const import UC_X86_REG_ESP  # noqa: E402

import pm_floors  # noqa: E402
import test_pm_zar  # noqa: E402

CTOR, STEER, PLACE, ORDER, BLOCKED, MEASURE, COPYSIGN, FTOL = 0x4d9870, 0x4dc0f0, 0x5cb260, 0x4fa470, 0x4ccaf0, 0x4d92a0, 0x5de5bc, 0x5cef9a
VT = 0x5eec2c
FIGHT_STATE, FIGHT_CTX, FIGHT_ENTER, FIGHT_RESET, FIGHT_RELEASE = 0x4d9b10, 0x4d99a0, 0x4d9be0, 0x4d9ce0, 0x4d9cc0
FIGHT_MEASURE, FIGHT_TARGET, FIGHT_DECIDE, NOW = 0x4d9a90, 0x4d99e0, 0x4d9b90, 0x62da90
SLOT = {'0': 0x00, '1': 0x04, '2': 0x08, '3': 0x0c, '5': 0x14, '6': 0x1c, '7': 0x20}


def f32(v):
    return struct.unpack('<f', struct.pack('<f', v))[0]


def bits(v):
    return struct.unpack('<I', struct.pack('<f', v))[0]


class Exe(test_pm_zar.Emulated):
    def __init__(self):
        super().__init__()
        self.brain = self.alloc(0x500)
        self.ctx = self.alloc(0x90)
        self.state = None
        self.guard = False
        self.drove = self.alloc(12)
        self.vehicle = self.alloc(0x100)
        self.order = self.alloc(0x20)
        self.w32(self.order + 8, self.brain)
        self.target = self.alloc(0x40)
        self.goals = self.alloc(8)
        self.w32(COPYSIGN, self.cpu.code(bytes.fromhex('dd442404 d9e1 f644241380 7402 d9e0 c3')))
        # _ftol, truncating
        self.w32(0x5de644, self.cpu.code(bytes.fromhex('83ec0c d93c24 668b0424 80cc0c 6689442402 d96c2402 df7c2404 d92c24 8b442404 83c40c c3')))
        # The controls at +0x100 + 0: their first word is both the offset table (+4 is 0) and the vtable.
        ctl = self.ctl = self.alloc(0x80)
        self.w32(self.brain + 0x100, ctl)
        self.w32(self.brain + 0x11c, self.vehicle)
        self.w32(ctl + 0x1c, self.cpu.code(bytes.fromhex('d905') + struct.pack('<I', self.goals + 4) + b'\xc3'))
        self.w32(ctl + 0x30, self.cpu.code(bytes.fromhex('d905') + struct.pack('<I', self.goals) + b'\xc3'))
        vt = self.alloc(0x200)
        self.w32(self.vehicle, vt)
        # Speed and stopping distance: the argument times a constant, so a wrong argument shows.
        self.k = self.alloc(8)
        self.w32(vt + 0x124, self.cpu.code(bytes.fromhex('d9442404 d80d') + struct.pack('<I', self.k) + bytes.fromhex('c20400')))
        self.w32(vt + 0x12c, self.cpu.code(bytes.fromhex('d9442404 d80d') + struct.pack('<I', self.k + 4) + bytes.fromhex('c20400')))
        self.rec = self.alloc(0x54)
        self.w32(self.vehicle + 0xd8, self.rec)  # the boundary record, as made
        # The via node's two words, the brain's pending list (+0x15c: +4 a node to itself, +8 its count), new.
        self.via = self.alloc(8)
        self.w32(self.via, 0x1111)
        self.w32(self.via + 4, 0x2222)
        self.w32(self.rec + 0x4c, self.via)
        head = self.alloc(12)
        self.w32(head, head)
        self.w32(head + 4, head)
        self.w32(self.brain + 0x15c + 4, head)
        self.head = head
        self.native_stub(0x5ce8c6, lambda c: self.alloc(self.g(c._uc.reg_read(UC_X86_REG_ESP) + 4)))
        self.sent, self.sends = [0, 0], 0
        self.w32(ctl + 0x20, self.ctl_stub(1))
        self.w32(ctl + 0x28, self.ctl_stub(0))
        # The owned object: slot 0 counts its calls and keeps the argument.
        self.deleted = self.alloc(8)
        self.owned, ovt = self.alloc(8), self.alloc(8)
        self.w32(self.owned, ovt)
        self.w32(ovt, self.cpu.code(bytes.fromhex('8b442404 a3') + struct.pack('<I', self.deleted + 4)
                                    + bytes.fromhex('ff05') + struct.pack('<I', self.deleted) + bytes.fromhex('c20400')))
        self.pilot = self.alloc(0x600)
        # The order's target: +0x0c, vt+0x1c, vt+0x18, vt+0x50; vt+0x04 its place, +0x1c's +0x08 live.
        chain = [self.alloc(0x20) for _ in range(4)]
        self.record = self.alloc(0x10)
        for i, slot in ((0, 0x1c), (1, 0x18), (2, 0x50), (3, 0x04)):
            tvt = self.alloc(0x60)
            self.w32(chain[i], tvt)
            answer = chain[i + 1] if i < 3 else None
            self.w32(tvt + slot, self.ret_stub((lambda a: lambda c: a)(answer) if answer else (lambda c: self.target)))
        self.w32(self.order + 0xc, chain[0])
        self.w32(chain[3] + 0x1c, self.record)
        self.leave = False
        self.native_stub(0x4fa210, lambda c: 1 if self.leave else 0, pop=4)
        self.want_pilot, self.blocked, self.steers, self.on_from = False, False, True, False
        self.w32(vt + 0x58, self.ret_stub(lambda c: self.pilot if self.want_pilot else 0))
        self.native_stub(PLACE, lambda c: self.vehicle + 0x80 if c.ecx == self.vehicle else self.target)
        self.native_stub(ORDER, lambda c: 1 if self.steers else 0)
        self.native_stub(BLOCKED, self.pull)
        for init in (0x4dbf80, 0x4dbfa0, 0x4dbfc0, 0x4dc020, 0x4dc0c0, 0x4dc0e0, 0x4d2d40):  # the statics, as at start-up
            self.call(init)
        self.call(STEER, ecx=self.brain + 0x210)
        # The vehicle's contact: [+0x6c], whose +4 answers the target by vt+0x2c.
        self.contact, holder, hvt = self.alloc(0x20), self.alloc(8), self.alloc(0x40)
        self.w32(self.contact + 4, holder)
        self.w32(holder, hvt)
        self.w32(hvt + 0x2c, self.ret_stub(lambda c: self.target))

    def pull(self, c):
        """The boundary, stubbed alike on both sides: a pulled place, or the way's own start."""
        if not self.blocked:
            return 0
        sp = c._uc.reg_read(UC_X86_REG_ESP)
        a, b, o = (self.g(sp + i) for i in (8, 12, 16))
        fa = struct.unpack('<3f', bytes(c._uc.mem_read(a, 12)))
        fb = struct.unpack('<3f', bytes(c._uc.mem_read(b, 12)))
        if self.on_from:
            self.fs(o, [fa[0], fb[1], fa[2]])
        else:
            self.fs(o, [f32(f32((fa[0] + fb[0])) * 0.5) + 3.0, fb[1], f32(f32((fa[2] + fb[2])) * 0.5) - 2.0])  # as the probe's, in floats
        return 1

    def ctl_stub(self, which):
        a = self.cpu.code(b'\xc2\x04\x00')

        def fn(c):
            self.sent[which] = self.g(c._uc.reg_read(UC_X86_REG_ESP) + 4)
            self.sends += 1
            return 0
        self.native_stub(a, fn, pop=4)
        return a

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
        vals += [self.g(self.drove + o) for o in (0, 4, 8)]
        st0 = self.brain + 0x210
        vals += [self.g(st0 + o) for o in range(0x2c, 0x48, 4)] + [self.g(st0 + o) for o in range(0x4c, 0x58, 4)]
        vals += [self.g(st0 + o) for o in range(0x5c, 0x9c, 4)]  # every constant the steering's constructor sets
        exact = [self.g(c + 0x3c) & 0xff, self.g(st0 + 0x48) & 0xff, self.g(st0 + 0x58) & 0xff]
        if self.guard:
            vals += [self.g(c + o) for o in (0x5c, 0x60, 0x64, 0x68, 0x6c)]
            exact += [self.g(c + o) & 0xff for o in (0x58, 0x70, 0x71, 0x72)]
        if self.state:
            s = self.state
            vals += [self.g(c + o) for o in (0x58, 0x5c, 0x60, 0x64, 0x6c, 0x70, 0x74, 0x7c, 0x80, 0x84, 0x88)]
            vals += [self.g(s + o) for o in (0xc, 0x10, 0x14, 0x18, 0x1c, 0x20, 0x24, 0x28)]
            st = self.brain + 0x210
            vals += [self.g(st + o) for o in (4, 8, 0xc, 0x10, 0x14, 0x18, 0x1c, 0x20, 0x2c, 0x34, 0x38)]
            vals += [self.g(self.brain + 0x2b8 + o) for o in (0, 4, 8)]
            vals += self.sent
            byte = lambda a: self.g(a) & 0xff  # noqa: E731
            wp = self.g(c + 0x78)
            way = 0 if not wp else 1 if wp == c + 0x6c else 2 if wp == self.brain + 0x210 + 4 else 3
            exact += [byte(c + 0x68), byte(c + 0x69), byte(c + 0x6a), way, byte(c + 0x8c), byte(c + 0x8d), byte(s + 8),
                      byte(s + 0x2c), byte(s + 0x2d), int(self.g(s + 0x30) != 0), byte(s + 0x34),
                      self.g(self.deleted), self.g(self.deleted + 4), self.sends, self.g(self.brain + 0x15c + 8)]
            last = self.g(self.head + 4)  # the last node's item, its two words
            item = self.g(last + 8) if last != self.head else 0
            exact += [self.g(item + 4), self.g(item + 8)] if item else [0, 0]
        return ' '.join(f'{v:x}' for v in vals) + ' | ' + ' '.join(str(v) for v in exact) + '\n'


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
            lines.append(('O', [r.uniform(0.9, 1.0), r.uniform(0, 3), r.uniform(0, 40)]))
            lines.append(('7',))
        elif k < 0.3:
            lines.append(world())
        elif k < 0.5:
            lines.append(steer())
            lines.append(('4',))
        elif k < 0.75:
            lines.append(drive_line(r))
        elif k < 0.8:
            lines.append(('X',))
        else:
            lines.append((r.choice('0123567'),))
    return lines


def drive_line(r):
    return ('Y', [r.uniform(0, 30), r.uniform(0, 30), r.uniform(0, 60), r.choice([0.0, 5.0, 40.0]), r.choice([0.02, 0.1, 1.0]),
                  r.choice([0.0, 0.5, 5.0]), r.choice([0.0, 0.5, 5.0])])


def fight_plan(seed):
    """A fighting state and its context: each measure and decision on worlds set to reach both answers."""
    r = random.Random(f'fight:{seed}')
    lines = plan(seed)[:2]

    def fight():
        far = r.choice([20.0, 60.0, 200.0, 1000.0])
        return ('F', [r.choice([1.0, 10.0]), r.choice([1.0, 10.0, 100.0]), far, far * r.choice([0.0, 0.1, 0.5]), r.uniform(0, 100)])

    def state():
        return ('f', r.choice([0x3, 0xf5, 0x1a]), [r.choice([0.0, 50.0, 200.0]), r.uniform(0, 100)])

    lines += [fight(), ('B',), ('N', 1), ('E',)]
    base = plan(seed + 100000)
    worlds = [ln for ln in base if ln[0] == 'W'] or [base[0]]
    # Some far apart, for the way cut short past the middle range + 200.
    worlds += [(w[0], [u * 6 for u in w[1][:6]] + w[1][6:]) + w[2:] for w in worlds]
    steers = [ln for ln in base if ln[0] == 'T'] or [base[1]]
    for _ in range(r.randrange(6, 16)):
        k = r.random()
        if k < 0.1:
            lines.append(fight())
        elif k < 0.2:
            lines.append(r.choice(worlds))
        elif k < 0.3:
            t = r.choice(steers)
            if r.random() < 0.3:  # the carrot past the way's length
                t = ('T', t[1][:7] + [t[1][6] + r.uniform(1, 9)] + t[1][8:])
            lines.append(t)
        elif k < 0.35:
            lines.append(('Z', r.choice([0, 0, 1])))
        elif k < 0.4:
            lines.append(state())
        elif k < 0.45:
            lines.append(('N', r.choice([0, 1])))
            lines.append(('D',))
            lines.append(('N', 1))
        elif k < 0.5:
            lines.append(('E',))
        elif k < 0.55:
            lines.append(('R',))
        elif k < 0.6:
            lines.append(('L',))
        elif k < 0.65:
            lines.append(('K',))
        elif k < 0.75:
            lines.append(('D',))
        elif k < 0.8:
            lines.append(('M',))
            lines.append(('G',))
        elif k < 0.85:
            lines += [('D',), ('M',), ('P',)]
            if r.random() < 0.3:
                lines += [('V', r.choice([0, 1]), 0), ('A',)]
        elif k < 0.95:  # the walk's step, from each of its steps
            if r.random() < 0.6:
                lines.append(('f', r.randrange(11) | r.choice([0, 0x50]), [r.choice([0.0, 50.0, 200.0]), r.uniform(0, 100)]))
            lines.append(('U', r.choice([0, 0, 1])))
            # The target dead (live off through the order): its place still good, so the measures don't fault.
            if r.random() < 0.3:
                lines += [('V', 0, 0), ('A',), ('S',), ('S',), ('U', 0), ('S',), ('V', 1, 0)]
            else:
                lines += [('D',), ('S',), ('S',), ('U', 0), ('S',)]
        else:
            lines.append((r.choice('0123567'),))
    return lines


def guard_plan(seed):
    """The guarding context: made on worlds with a live target or not, then measured."""
    r = random.Random(f'guard:{seed}')
    base = plan(seed + 200000)
    lines = base[:2] + [('V', r.choice([0, 1, 1]), r.choice([0, 0, 1])), ('H',), ('4',)]
    worlds = [ln for ln in base if ln[0] == 'W']
    steers = [ln for ln in base if ln[0] == 'T']
    for _ in range(r.randrange(3, 10)):
        k = r.random()
        if k < 0.25 and worlds:
            lines.append(r.choice(worlds))
        elif k < 0.5 and steers:
            lines.append(r.choice(steers))
        elif k < 0.7:
            lines.append(('V', r.choice([0, 1, 1]), r.choice([0, 0, 1])))
            lines.append(('H',))
        else:
            lines.append(('4',))
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
        elif op == 'F':
            v = ln[1]
            x.fs(x.k, v[0:2])
            x.fs(x.brain + 0x28, v[2:4])
            x.fs(NOW, v[4:5])
        elif op == 'B':
            x.state = x.alloc(0x50)
            x.call(FIGHT_STATE, ecx=x.state, args=(x.order,))
            x.call(FIGHT_CTX, ecx=x.ctx, args=(x.order, x.state))
            x.w32(x.ctx + 0xc, x.vehicle + 0x80)
            made = True
        elif op == 'Y':
            v = ln[1]
            a, c = x.alloc(8), x.alloc(0x14)
            x.fs(a, v[0:2])
            for i, w in enumerate((x.brain, x.vehicle, x.vehicle + 0x80, x.vehicle + 0x24, x.brain + 0x210)):
                x.w32(c + 4 * i, w)
            x.call(0x4dcd20, args=(a, bits(v[2]), bits(v[3]), c, x.drove, x.drove + 4, bits(v[4]), bits(v[5]), bits(v[6])))
        elif op == 'X':
            c = x.alloc(0x14)
            x.w32(c + 0x10, x.brain + 0x210)
            x.call(0x4dccf0, args=(0, x.drove, x.drove + 8, c))
        elif op == 'V':
            x.w32(x.record + 8, ln[1])
            x.leave = bool(ln[2])
        elif op == 'A':
            x.cpu._uc.mem_write(x.ctx, bytes(0x90))  # a new one, as the port's
            x.call(0x4daa70, ecx=x.ctx, args=(x.order, x.state))
            x.w32(x.ctx + 0xc, x.vehicle + 0x80)
        elif op == 'H':
            x.state = None
            x.guard = True
            x.cpu._uc.mem_write(x.ctx, bytes(0x90))
            x.call(0x4dfb50, ecx=x.ctx, args=(x.order,))
            x.w32(x.ctx + 0xc, x.vehicle + 0x80)
            made = True
        elif op == 'Z':
            x.on_from = bool(ln[1])
        elif op == 'U':
            x.cpu._uc.mem_write(x.rec + 0x50, bytes([ln[1]]))
        elif op == 'S':
            x.call(0x4d9d00, ecx=x.state, args=(0, x.ctx))
        elif op == 'P':
            x.call(x.g(x.g(x.state) + 0x24), ecx=x.state, args=(x.ctx,))
        elif op == 'E':
            x.call(FIGHT_ENTER, ecx=x.state, args=(x.ctx,))
        elif op == 'R':
            x.call(x.g(x.g(x.state) + 0x10), ecx=x.state)
        elif op == 'f':
            x.w32(x.state + 8, ln[1])
            x.fs(x.state + 0x24, ln[2])
            x.cpu._uc.mem_write(x.state + 0x2c, b'\1\1')
        elif op == 'L':
            x.w32(x.state + 0x30, x.owned)
        elif op == 'K':
            x.call(FIGHT_RELEASE, ecx=x.state)
        elif op == 'N':
            x.w32(x.ctx + 0x54, x.target if ln[1] else 0)
        elif op == 'D':
            x.call(FIGHT_TARGET, ecx=x.ctx, args=(0,))
        elif op == 'M':
            x.call(x.g(x.g(x.ctx) + 0x10), ecx=x.ctx)
        elif op == 'G':
            x.call(FIGHT_DECIDE, ecx=x.ctx, args=(x.state,))
        elif op == 'C':
            x.call(CTOR, ecx=x.ctx, args=(x.order,))
            x.w32(x.ctx + 0xc, x.vehicle + 0x80)
            made = True
        elif op == '4':
            x.call(0x4dfc30 if x.guard else MEASURE, ecx=x.ctx)
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
        elif ln[0] == 'F':
            o.append('F ' + ' '.join(f'{bits(u):x}' for u in ln[1]))
        elif ln[0] == 'f':
            o.append(f'f {ln[1]:x} ' + ' '.join(f'{bits(u):x}' for u in ln[2]))
        elif ln[0] in 'NZU':
            o.append(f'{ln[0]} {ln[1]}')
        elif ln[0] == 'V':
            o.append(f'V {ln[1]} {ln[2]}')
        elif ln[0] == 'Y':
            o.append('Y ' + ' '.join(f'{bits(u):x}' for u in ln[1]))
        else:
            o.append(ln[0])
    return '\n'.join(o) + '\n'


def close(a, b):
    return a == b  # at 53 bits, to the bit


def main():
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 300
    lib = test_pm_zar.load_pm_probe()
    lib.probe_pm_ai_walk.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
    buf = ctypes.create_string_buffer(1 << 20)
    bad = steps = 0
    seen = {}
    for seed in range(runs * 3):
        lines = plan(seed) if seed < runs else fight_plan(seed) if seed < runs * 2 else guard_plan(seed)
        a = run_exe(Exe(), lines).split('\n')
        if runs <= seed < runs * 2:
            for p in a:
                ex = p.partition('|')[2].split()
                if len(ex) > 8:
                    seen['step', str(int(ex[9]) & 15)] = seen.get(('step', str(int(ex[9]) & 15)), 0) + 1
                    seen['queued', '1' if int(ex[17]) else '0'] = 1
                    for name, i in (('in_range', 3), ('at_stop', 4), ('no_target', 5), ('way', 6), ('may_go', 7), ('off_way', 8)):
                        seen[name, ex[i]] = seen.get((name, ex[i]), 0) + 1
        assert lib.probe_pm_ai_walk(text(lines).encode(), buf, len(buf)) >= 0
        b = buf.value.decode().split('\n')
        steps += len(a)
        for i, (p, q) in enumerate(zip(a, b)):
            pf, _, pe = p.partition('|')
            qf, _, qe = q.partition('|')
            pw, qw = pf.split(), qf.split()
            if len(pw) != len(qw) or pe.split() != qe.split() or not all(close(u, v) for u, v in zip(pw, qw)):
                print(f'seed {seed} line {i}:\n  exe  {p}\n  port {q}')
                bad += 1
                break
        else:
            if len(a) != len(b):
                print(f'seed {seed}: {len(a)} lines against {len(b)}')
                bad += 1
        if bad >= 6:
            break
    print(f'{runs * 3} plans, {steps} lines: {bad} differ')
    print('fight answers:', ' '.join(f'{n}={v}:{c}' for (n, v), c in sorted(seen.items())))
    pm_floors.floors(plans=(runs, 100), lines=(steps, 1000),
                     **{f'{n}_{v}': (seen.get((n, v), 0), 5) for n in ('in_range', 'at_stop', 'no_target', 'may_go', 'off_way') for v in '01'},
                     way_own=(seen.get(('way', '1'), 0), 5), way_start=(seen.get(('way', '2'), 0), 5),
                     queued=(seen.get(('queued', '1'), 0), 1),
                     **{f'step_{k}': (seen.get(('step', str(k)), 0), 5) for k in range(1, 11)})
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
