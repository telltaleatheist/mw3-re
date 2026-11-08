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
import gc
import math
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
os.environ['MW3_X87'] = '53'  # the reference's precision, only: docs/pm-tests.md
from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn.x86_const import UC_X86_REG_ESP  # noqa: E402

import original  # noqa: E402
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
        self.w32(self.record + 8, 1)  # live, as the probe's world starts
        rvt, self.face = self.alloc(0x40), self.alloc(0x40)
        self.face_on = False
        self.w32(self.record, rvt), self.w32(rvt + 0x24, self.ret_stub(lambda c: self.face if self.face_on else 0))
        self.bh = None
        self.cp = None
        self.asked = [0, 0, 0]
        self.gs = None
        self.at = None
        self.native_stub(0x4c7950, lambda c: 0)  # the circling context's +0x18, which its state never reads
        self.k_ahead = self.alloc(4)
        self.w32(vt + 0x134, self.cpu.code(bytes.fromhex('d9442404 d80d') + struct.pack('<I', self.k_ahead) + bytes.fromhex('c20400')))
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
        # The move state 0x4dcfe0: its +8's vt+0x1c the place; what comes next and the behaviour taken.
        self.move = self.alloc(0x30)
        self.w32(self.move + 4, self.order)
        mh, mvt = self.alloc(8), self.alloc(0x40)
        self.w32(mh, mvt)
        self.w32(self.move + 8, mh)
        self.w32(mvt + 0x1c, self.ret_stub(lambda c: self.vehicle + 0x80))
        self.next, self.taken, self.along = 0, [0, 0], self.alloc(12)
        nxt, nvt = self.alloc(8), self.alloc(0x80)
        self.w32(nxt, nvt)
        self.w32(nvt + 0x74, self.ret_stub(lambda c: 0x7700 if self.next == 2 else 0))

        def along(c):
            sp = c._uc.reg_read(UC_X86_REG_ESP)
            d = self.g(sp + 8)
            for o in (0, 4, 8):
                self.w32(self.along + o, self.g(d + o))
            return nxt if self.next else 0

        def take(c):
            self.taken = [self.taken[0] + 1, self.g(c._uc.reg_read(UC_X86_REG_ESP) + 4)]
            return 0
        self.native_stub(0x4d60d0, along, pop=8)
        # The terminus's end: its new 0x5ef250 (operator new, 0x4ea070) to 0x4ea180 on the brain's +0x15c, its list.
        self.backs = 0
        self.native_stub(0x5ce8c6, lambda c: self.alloc(8))
        self.native_stub(0x4ea070, lambda c: c.ecx)

        def back(c):
            item = self.g(c._uc.reg_read(UC_X86_REG_ESP) + 4)
            assert c.ecx == self.brain + 0x15c and self.g(item) == 0x5ef250, (hex(c.ecx), hex(self.g(item)))
            self.backs += 1
            return 0
        self.native_stub(0x4ea180, back, pop=4)
        self.native_stub(0x4f9f00, take, pop=4)
        # The manoeuvre 0x4d5040: the vehicle's answers, the order's pop, the picks.
        self.man = self.alloc(0x40)
        self.w32(self.man + 4, self.order)
        self.m = self.alloc(0x20)  # fuel, speed
        self.w32(vt + 0x154, self.cpu.code(b'\xd9\x05' + struct.pack('<I', self.m) + b'\xc3'))
        self.w32(vt + 0x120, self.cpu.code(b'\xd9\x05' + struct.pack('<I', self.m + 4) + b'\xc3'))
        self.m_air, self.m_pops, self.m_picks, self.m_hit, self.m_lead = False, 0, {}, False, None
        self.w32(vt + 0x150, self.ret_stub(lambda c: 1 if self.m_air else 0))
        ovt = self.alloc(0x40)
        self.w32(self.order, ovt)

        def popped(c):
            self.m_pops += 1
            return 0
        self.w32(ovt + 0xc, self.ret_stub(popped))
        self.m_kinds = {}
        for kind in (1, 3):
            self.m_kinds[kind] = self.alloc(8)

        def picked(c):
            k = self.g(c._uc.reg_read(UC_X86_REG_ESP) + 4)
            if not self.m_picks.get(k):
                return 0
            self.w32(self.m_kinds[k] + 4, self.m_picks[k])
            return self.m_kinds[k]
        self.native_stub(0x4e7010, picked, pop=4)
        self.native_stub(0x4b27d0, lambda c: 1 if self.m_hit else 0, pop=4)
        self.native_stub(0x4b2820, lambda c: 0x6601)
        self.m_place = self.alloc(12)
        self.native_stub(0x5cb2b0, lambda c: self.m_place)

        def lead(c):
            if self.m_lead is None:
                return 0
            self.fs(self.g(c._uc.reg_read(UC_X86_REG_ESP) + 4), self.m_lead)
            return 1
        self.native_stub(0x41e8b0, lead, pop=4)
        self.m_sent, self.m_stops = [0, 0], 0

        def stopped(c):
            self.m_stops += 1
            return 0
        self.w32(ctl + 0x14, self.ret_stub(stopped))
        self.w32(ctl + 0x34, self.m_ctl(0))
        self.w32(ctl + 0x3c, self.m_ctl(1))
        self.rolls = []
        roll = self.cpu.code(b'\xc3')
        self.w32(0x5de668, roll)
        self.native_stub(roll, lambda c: self.rolls.pop(0), 0)
        self.pick = self.alloc(0x30)
        self.contact, holder, hvt = self.alloc(0x20), self.alloc(8), self.alloc(0x40)
        self.w32(self.contact + 4, holder)
        self.w32(holder, hvt)
        self.w32(hvt + 0x2c, self.ret_stub(lambda c: self.target))

    def m_ctl(self, i):
        a = self.cpu.code(b'\xc2\x04\x00')

        def fn(c):
            self.m_sent[i] = self.g(c._uc.reg_read(UC_X86_REG_ESP) + 4)
            return 0
        self.native_stub(a, fn, pop=4)
        return a

    def pull(self, c):
        """The boundary, stubbed alike on both sides: a pulled place, or the way's own start."""
        sp = c._uc.reg_read(UC_X86_REG_ESP)
        a, b, o = (self.g(sp + i) for i in (8, 12, 16))
        self.asked = [self.g(b + i) for i in (0, 4, 8)]
        if not self.blocked:
            return 0
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
        m = self.move
        vals += [self.g(m + o) for o in (0x10, 0x18, 0x1c, 0x20, 0x24)] + [self.g(self.along + o) for o in (0, 4, 8)]
        vals += [self.g(self.man + o) for o in (0xc, 0x14, 0x18)] + self.m_sent
        vals += [self.g(self.pick + o) for o in (0x10, 0x14, 0x18)] + [self.g(self.brain + 0x2b8 + o) for o in (0, 4, 8)]
        exact = [self.g(c + 0x3c) & 0xff, self.g(st0 + 0x48) & 0xff, self.g(st0 + 0x58) & 0xff]
        if self.bh:
            st = self.brain + 0x210
            vals += [self.g(self.bh + o) for o in (0x10, 0x14, 0x18, 0x1c, 0x20, 0x24, 0x28, 0x2c, 0x34, 0x38)]
            vals += [self.g(st + o) for o in (4, 8, 0xc, 0x10, 0x14, 0x18, 0x1c, 0x20, 0x2c, 0x34, 0x38)]
            vals += [self.g(self.brain + 0x2b8 + o) for o in (0, 4, 8)] + self.sent
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
        if self.bh:
            exact += [self.g(self.bh + 0xc), self.sends]
        if self.gs:
            st = self.brain + 0x210
            vals += [self.g(self.gs + o) for o in (0x10, 0x14, 0x18, 0x1c, 0x20, 0x24, 0x28)]
            vals += [self.g(st + o) for o in (4, 8, 0xc, 0x10, 0x14, 0x18, 0x1c, 0x20, 0x2c, 0x34, 0x38)]
            vals += [self.g(self.brain + 0x2b8 + o) for o in (0, 4, 8)] + self.sent
            exact += [self.g(self.gs + 8), self.g(self.gs + 0xc), self.g(self.gs + 0x30), self.sends]
        if self.at:
            vals += [self.g(self.at + o) for o in range(0xc, 0x2c, 4)]
            exact += [self.g(self.at + 8), self.g(self.at + 0x2c) & 0xffff, self.g(self.at + 0x30), self.g(self.at + 0x38),
                      self.g(self.at + 0x3c), self.g(self.at + 0x40)]
        if self.cp:
            vals += [self.g(self.cp + o) for o in (0xc, 0x1c, 0x20)] + self.asked + self.sent
            exact += [self.g(self.cp + 8), self.g(self.cp + 0x18), int(self.g(self.brain + 0x3c) == 0), self.sends]
        exact += [self.m_stops, self.g(self.brain + 0x110)]
        exact += [self.g(self.man + 0x10), self.g(self.man + 0x2c), self.m_pops, self.g(self.brain + 0x118) & 0xff]
        exact += [self.g(self.brain + 0x158) & 0xff, self.g(self.order + 4) & 0xff, self.backs]
        exact += [self.g(m + 0xc), self.taken[0], self.taken[1]]
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

    clock = [0.0]

    def fight():
        far = r.choice([20.0, 60.0, 200.0, 1000.0])
        clock[0] = r.uniform(0, 100)
        return ('F', [r.choice([1.0, 10.0]), r.choice([1.0, 10.0, 100.0]), far, far * r.choice([0.0, 0.1, 0.5]), clock[0]])

    def state():
        return ('f', r.choice([0x3, 0xf5, 0x1a]), [r.choice([0.0, 50.0, 200.0]), r.uniform(0, 100)])

    lines += [fight(), ('V', r.choice([0, 1, 1]), r.choice([0, 0, 1])), ('B',) if r.random() < 0.7 else ('@', [r.choice([0.0, 1.0]), r.choice([0.0, 1.0, 2.0])]), ('V', 1, 0), ('N', 1), ('E',)]
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
        elif k < 0.8525:
            lines.append(('b',))
            lines += [('r',)] * r.randrange(1, 4)
            lines += [('V', 0, 0), ('r',), ('V', 1, 0)]  # the target gone: the order ends
        elif k < 0.855:
            lines.append(('n',))
        elif k < 0.86:
            lines.append(('l',))
        elif k < 0.88:
            if 'F' not in [ln[0] for ln in lines[:4]]:
                lines.append(fight())
            for _ in range(r.randrange(1, 4)):
                a = r.uniform(0, 6.283)
                tp = [r.uniform(-300, 300), r.uniform(-10, 40), r.uniform(-300, 300)]
                if r.random() < 0.5:  # facing it within a few degrees: the dead zone's edges
                    w = r.choice(worlds)
                    v = list(w[1])
                    v[6], v[7] = f32(math.sin(a)), f32(math.cos(a))
                    lines.append((w[0], v) + w[2:])
                    t = a + math.radians(r.choice([1, -1]) * r.choice([0.5, 4.9, 4.99, 5.01, 5.1, 7, 20, 60, 120, 174.9, 175.1]))
                    tp = [v[0] + 200 * math.sin(t), r.uniform(-10, 40), v[2] + 200 * math.cos(t)]
                ld = [r.uniform(-300, 300), r.uniform(-10, 40), r.uniform(-300, 300)]
                lines.append(('u', [r.choice([0, 0, 1]), r.choice([0, 0, 1, 1, 2, 2, 2]), r.choice([0, 1]), r.choice([0, 1]), r.choice([0, 0, 1]),
                                    r.choice([0, 0, 1]), r.choice([0, 0, 0x6602]), r.choice([0, 0x40, 0x40]), r.choice([0, 1]), r.choice([0, 1]),
                                    r.choice([0, 1]), r.choice([0, 1])],
                              [r.choice([0.0, 1.0, 0.5]), r.choice([0.0, 0.0, 1.0, -0.3]), r.choice([0.0, clock[0], clock[0] + 5, clock[0] - 5]),
                               r.choice([0.05, 0.1, 0.3, 0.6, 0.7, 0.9]), r.choice([0.0, 0.5, -0.5, 1.0, 1.5, -2.0, 4.0, -9.0, 30.0, 1.002, -1.002, r.uniform(-6, 6)]),
                               r.choice([0.0, 0.5, -0.5, 1.5, -2.0, 4.0, -9.0, 30.0, 1.002, -1.002, r.uniform(-6, 6)]), r.choice([-1000.0, 1000.0, 0.0])] + tp + ld))
        elif k < 0.9:  # the move state or the backup node's, from each of their steps
            backup = r.random() < 0.5
            terminus = not backup and r.random() < 0.4
            ws, ts = [ln for ln in lines if ln[0] == 'W'], [ln for ln in lines if ln[0] == 'T']
            if backup and ws and ts and r.random() < 0.5:  # facing back along the way, within a few degrees: the turns done
                d = ts[-1][1][3:5]
                # The front to the way back (steps 2, 3) or the back to it (step 0 done, the drive on its front).
                t = math.atan2(-d[0], -d[1]) + math.radians(r.choice([0.0, 2.0, -10.0, 25.0, -25.4, 26.0, 180.0, 178.0, 160.0]))
                v = list(ws[-1][1])
                v[6], v[7] = f32(math.sin(t)), f32(math.cos(t))
                lines.append((ws[-1][0], v) + ws[-1][2:])
            for _ in range(r.randrange(2, 7) if backup or terminus else r.randrange(1, 4)):
                if (backup or terminus) and ts and r.random() < (0.2 if backup else 0.5):  # at the way's end, its sum the distance
                    tv = list(ts[-1][1])
                    tv[7] = tv[5]
                    lines.append(('T', tv))
                    ts.append(('T', tv))
                    on = f32(f32(abs(f32(tv[10]))) + f32(abs(f32(tv[9]))))
                    lines.append(('t' if terminus else 'k', r.choice([0, 1]), r.choice([0, 1, 2, 2]),
                                  [f32(on * r.choice([1.0, 1.0, 2.0, 0.5])) if terminus else on, r.choice([0.01, 0.05]), r.choice([0.0, 1e4]), r.uniform(-1, 1), r.uniform(-1, 1)]))
                    continue
                if terminus:
                    lines.append(('t', r.choice([0, 1, 1, 2]), 0, [r.choice([0.0, 5.0, 50.0, 500.0, r.uniform(0, 8), r.uniform(0, 80)]), r.choice([0.002, 0.01, 0.05, r.uniform(0, 5)]), 0.0, r.uniform(-1, 1), r.uniform(-1, 1)]))
                    continue
                lines.append(('k' if backup else 'm', r.choice([0, 0, 1, 2, 2, 3] if backup else [0, 1, 2, 2]), r.choice([0, 1, 2, 2] if backup else [0, 1, 2]),
                              [r.choice([0.0, 5.0, 50.0, 500.0, r.uniform(0, 8), r.uniform(0, 80)]), r.choice([0.002, 0.01, 0.05, r.uniform(0, 5)]), r.choice([0.0, clock[0], 200.0, 1e4]), r.uniform(-1, 1), r.uniform(-1, 1)]))
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
        elif k < 0.85:
            lines.append(('q', r.randrange(32768), r.randrange(32768), [r.uniform(0.1, 3), r.uniform(0, 80), r.uniform(0, 300)]))
        else:
            lines.append(('4',))
    return lines


def behind_plan(seed):
    """The state behind the order's target: made, then run on worlds moving under it."""
    r = random.Random(f'behind:{seed}')
    base = plan(seed + 300000)
    worlds = [ln for ln in base if ln[0] == 'W']
    steers = [ln for ln in base if ln[0] == 'T']
    a = r.uniform(0, 6.283)
    lines = base[:2] + [('C',), ('F', [r.uniform(1, 40), r.uniform(1, 40), 60.0, r.uniform(0, 60), 10.0]), ('a', [r.choice([r.uniform(5, 120), 40.0])]),
                        ('V', 1, 0), ('j', r.choice([0, 1, 1]), math.cos(a), math.sin(a)), ('w',)]
    for _ in range(r.randrange(4, 14)):
        k = r.random()
        if k < 0.2 and worlds:
            lines.append(r.choice(worlds))
        elif k < 0.3 and steers:
            lines.append(r.choice(steers))
        elif k < 0.4:
            lines.append(('V', r.choice([0, 1, 1, 1]), r.choice([0, 0, 0, 1])))
        lines.append(('x',))
    return lines


def circle_plan(seed):
    """The circling state: made, then run with the clock going and the boundary coming and going."""
    r = random.Random(f'circle:{seed}')
    base = plan(seed + 400000)
    worlds = [ln for ln in base if ln[0] == 'W']
    now = r.uniform(0, 30)
    lines = base[:2] + [('C',), ('F', [1.0, 1.0, 60.0, 30.0, now]), ('V', 1, 0),
                        ('y', [r.uniform(0, 2), r.uniform(0, 60), r.choice([r.uniform(5, 120), 40.0])])]
    for _ in range(r.randrange(4, 16)):
        k = r.random()
        if k < 0.35 and worlds:
            w = r.choice(worlds)
            if r.random() < 0.08:  # standing on the target: nothing to normalise
                w = (w[0], w[1][:3] + w[1][:3] + w[1][6:]) + w[2:]
            lines.append(w)
        elif k < 0.5:
            lines.append(('V', r.choice([0, 1, 1, 1]), r.choice([0, 0, 0, 1])))
        elif k < 0.75:
            now += r.choice([0.2, 0.5, 0.99, 1.5, 4.0])
            lines.append(('F', [1.0, 1.0, 60.0, 30.0, now]))
        elif k < 0.8:
            lines.append(('!',))
        lines.append(('z',))
    return lines


def attack_made_plan(seed):
    """The attack state as its constructor leaves it."""
    r = random.Random(f'attackmade:{seed}')
    base = plan(seed + 600000)
    return base[:2] + [('C',), ('F', [r.uniform(1, 40), r.uniform(1, 40), r.uniform(0, 300), r.uniform(0, 300), 10.0]), ('I',)]


def guard_run_plan(seed):
    """The guarding state: made, then run while the target, the vehicle and the way move."""
    r = random.Random(f'guardrun:{seed}')
    base = plan(seed + 500000)
    worlds = [ln for ln in base if ln[0] == 'W']
    steers = [ln for ln in base if ln[0] == 'T']
    lines = base[:2] + [('C',), ('F', [1.0, 1.0, 60.0, r.choice([0.0, 5.0, 20.0]), 10.0]), ('V', 1, 0)]
    lines.append(('Q', r.randrange(32768), r.randrange(32768), [r.choice([20.0, 60.0, 150.0, 400.0])]))
    for _ in range(r.randrange(4, 14)):
        k = r.random()
        if k < 0.3 and worlds:
            lines.append(r.choice(worlds))
        elif k < 0.4 and steers:
            lines.append(r.choice(steers))
        elif k < 0.55:
            lines.append(('V', r.choice([0, 1, 1, 1]), r.choice([0, 0, 0, 1])))
        elif k < 0.7:
            lines.append(('e', r.randrange(32768), r.randrange(32768)))
        lines.append(('g',))
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
        elif op == '@':
            x.state = x.alloc(0x50)
            x.call(FIGHT_STATE, ecx=x.state, args=(x.order,))
            hold = x.alloc(12)
            x.fs(hold, [100.0, 0.0, 50.0])
            x.w32(x.state + 0x3c, hold if ln[1][0] else 0)
            x.w32(x.brain + 0x3c, int(ln[1][1]))
            x.call(0x4debe0, ecx=x.ctx, args=(x.order, x.state))
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
        elif op == 'm':
            x.w32(x.move + 0xc, ln[1])
            x.next = ln[2]
            x.fs(x.move + 0x1c, ln[3][:3])
            x.fs(x.move + 0x10, ln[3][3:4])
            x.fs(x.move + 0x18, ln[3][4:5])
            x.call(0x4dcfe0, ecx=x.move, args=(0,))
        elif op == 'k':
            if not getattr(x, 'aim_hook', False):
                # The backup run's aim, a stack vector it never sets in steps 0 and 2 ([ebp-0x28]): the port's FIX,
                # back along the way, put there on entry.
                def aim(uc, ad, sz, dir_at):
                    sp = uc.reg_read(UC_X86_REG_ESP)
                    d = struct.unpack('<2I', bytes(uc.mem_read(dir_at, 8)))
                    uc.mem_write(sp - 0x2c, struct.pack('<3I', d[0] ^ 0x80000000, 0, d[1] ^ 0x80000000))
                x.cpu._uc.hook_add(UC_HOOK_CODE, aim, x.brain + 0x210 + 0x10, begin=0x4ddab0, end=0x4ddab0)
                x.aim_hook = True
            x.w32(x.move + 0xc, ln[1])
            x.next = ln[2]
            x.fs(x.move + 0x1c, ln[3][:3])
            x.fs(x.move + 0x10, ln[3][3:4])
            x.fs(x.move + 0x18, ln[3][4:5])
            x.call(0x4ddab0, ecx=x.move, args=(0,))
        elif op == 't':
            x.w32(x.move + 0xc, ln[1])
            x.fs(x.move + 0x1c, ln[3][:3])
            x.fs(x.move + 0x10, ln[3][3:4])
            x.fs(x.move + 0x18, ln[3][4:5])
            x.call(0x4e1fd0, ecx=x.move, args=(0,))
        elif op == 'u':
            v, f = ln[1], ln[2]
            x.w32(x.man + 8, v[0])
            x.w32(x.man + 0x10, v[1])
            x.m_air, x.m_hit = bool(v[2]), bool(v[5])
            x.m_picks = {1: 0x6603 if v[3] else 0, 3: 0x6604 if v[4] else 0}
            x.w32(x.man + 0x2c, v[6])
            x.w32(x.brain + 0xec, v[7])
            x.cpu._uc.mem_write(x.brain + 0x118, bytes([v[8]]))
            x.want_pilot = bool(v[9])
            mine = x.alloc(0x10)  # its own contact: walk's is a chain
            x.w32(mine + 4, v[6] if v[10] else 0x7777)
            x.m_lead = f[10:13] if v[11] else None
            held = x.g(x.vehicle + 0x6c)
            x.w32(x.vehicle + 0x6c, mine)
            x.fs(x.man + 0xc, f[0:1])
            x.fs(x.man + 0x14, f[1:2])
            x.fs(x.man + 0x18, f[2:3])
            x.fs(x.m, f[3:5])
            x.fs(x.vehicle + 0x44, f[5:6])
            x.fs(x.brain + 0x7c, f[6:7])
            x.fs(x.m_place, f[7:10])
            x.call(0x4d5040, ecx=x.man)
            x.w32(x.vehicle + 0x6c, held)
        elif op == 'l':
            x.call(0x4d4fa0, ecx=x.man)
        elif op == 'n':
            x.call(0x4dcbe0, ecx=x.move, args=(x.g(x.move + 8), x.order))
        elif op == 'q':
            x.fs(x.pick + 0x1c, ln[3][:1])
            x.fs(x.pick + 0x28, ln[3][1:2])
            x.fs(x.brain + 0x44, ln[3][2:3])
            x.rolls = [ln[1], ln[2]]
            x.call(0x4dfec0, ecx=x.pick, args=(x.ctx,))
        elif op == 'V':
            x.w32(x.record + 8, ln[1])
            x.leave = bool(ln[2])
        elif op == 'a':
            x.fs(x.brain + 0x44, ln[1])
        elif op == 'j':
            x.face_on = bool(ln[1])
            x.fs(x.face + 0x24, [ln[2], 0.0, ln[3]])
        elif op == 'w':
            x.bh = x.alloc(0x50)
            x.call(0x4d8690, ecx=x.bh, args=(0x1234, x.order))
        elif op == 'x':
            x.call(0x4d89d0, ecx=x.bh, args=(0,))
        elif op == 'Q':
            x.rolls = [ln[1], ln[2]] * 50
            x.fs(x.brain + 0x44, ln[3])
            x.gs = x.alloc(0x50)
            x.call(0x4dfe40, ecx=x.gs, args=(0x1234, x.order))
        elif op == 'e':
            x.rolls = [ln[1], ln[2]] * 50
        elif op == 'g':
            x.call(0x4dffb0, ecx=x.gs, args=(0,))
        elif op == 'I':
            x.at = x.alloc(0x50)
            x.call(0x4decb0, ecx=x.at, args=(0x1234, x.order))
        elif op == 'y':
            x.fs(x.brain + 0x94, ln[1][0:1])
            x.fs(x.k_ahead, ln[1][1:2])
            x.fs(x.brain + 0x44, ln[1][2:3])
            x.w32(x.brain + 0x3c, 7)
            x.cp = x.alloc(0x50)
            x.call(0x4d8ca0, ecx=x.cp, args=(0x1234, x.order))
        elif op == 'z':
            x.call(0x4d8dc0, ecx=x.cp, args=(0,))
        elif op == '!':
            x.call(0x4d8d30, ecx=x.cp)
        elif op == 'b':
            x.state = x.alloc(0x50)
            x.call(0x4daaf0, ecx=x.state, args=(0x1234, x.order))
        elif op == 'r':
            x.call(0x4dabf0, ecx=x.state, args=(0,))
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
        elif ln[0] == 'u':
            o.append('u ' + ' '.join(str(u) for u in ln[1]) + ' ' + ' '.join(f'{bits(u):x}' for u in ln[2]))
        elif ln[0] == 'e':
            o.append(f'e {ln[1]} {ln[2]}')
        elif ln[0] in 'mktqQ':
            o.append(f'{ln[0]} {ln[1]} {ln[2]} ' + ' '.join(f'{bits(u):x}' for u in ln[3]))
        elif ln[0] == 'j':
            o.append(f'j {ln[1]} {bits(ln[2]):x} {bits(ln[3]):x}')
        elif ln[0] in 'Y@ya':
            o.append(ln[0] + ' ' + ' '.join(f'{bits(u):x}' for u in ln[1]))
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
    for seed in range(runs * 7):
        lines = plan(seed) if seed < runs else fight_plan(seed) if seed < runs * 2 else guard_plan(seed) if seed < runs * 3 else behind_plan(seed) if seed < runs * 4 else circle_plan(seed) if seed < runs * 5 else guard_run_plan(seed) if seed < runs * 6 else attack_made_plan(seed)
        a = run_exe(Exe(), lines).split('\n')
        # original.py keeps every emulator for the precision check at exit, 40 MB a plan: the newest is enough here.
        del original._ORIGINALS[:-1]
        gc.collect()
        if runs * 5 <= seed < runs * 6:
            for p in a:
                ex = p.partition('|')[2].split()
                if len(ex) > 8:
                    key = ('guard', ex[4])
                    seen[key] = seen.get(key, 0) + 1
        elif runs * 4 <= seed < runs * 5:
            for p, q in zip(a, a[1:]):
                was, ex = p.partition('|')[2].split(), q.partition('|')[2].split()
                if len(ex) > 8 and ex != was:
                    seen['circle', 'cleared' if ex[5] == '1' and was[5] != '1' else 'other'] = 1
            for p in a:
                vals = p.partition('|')[0].split()
                if len(vals) > 3:
                    seen['circle', 'sent0' if vals[-1] == '0' else 'sent'] = seen.get(('circle', 'sent0' if vals[-1] == '0' else 'sent'), 0) + 1
                    seen['circle', 'left' if int(vals[-3], 16) >> 31 else 'right'] = seen.get(('circle', 'left' if int(vals[-3], 16) >> 31 else 'right'), 0) + 1
        if runs <= seed < runs * 2:
            for p in a:
                ex = p.partition('|')[2].split()
                if len(ex) > 8:
                    seen['step', str(int(ex[9]) & 15)] = seen.get(('step', str(int(ex[9]) & 15)), 0) + 1
                    seen['queued', '1' if int(ex[17]) else '0'] = 1
                    seen['move', ex[-3]] = seen.get(('move', ex[-3]), 0) + 1
                    seen['taken', '1' if int(ex[-2]) else '0'] = seen.get(('taken', '1' if int(ex[-2]) else '0'), 0) + 1
                    for name, i in (('in_range', 3), ('at_stop', 4), ('no_target', 5), ('way', 6), ('may_go', 7), ('off_way', 8)):
                        seen[name, ex[i]] = seen.get((name, ex[i]), 0) + 1
        for ln, was, p in zip(lines[1:], a, a[1:]):
            ex, before = p.partition('|')[2].split(), was.partition('|')[2].split()
            if ln[0] == 't' and len(ex) > 8:
                seen['terminus', ex[-3]] = seen.get(('terminus', ex[-3]), 0) + 1
                if len(before) > 8 and ex[-4] != before[-4]:
                    seen['terminus', 'back'] = seen.get(('terminus', 'back'), 0) + 1
            if ln[0] == 'k' and len(ex) > 8:
                seen['backup', ex[-3]] = seen.get(('backup', ex[-3]), 0) + 1
                if len(before) > 8 and ex[-2] != before[-2]:  # the next behaviour taken by this line
                    seen['backup', 'taken'] = seen.get(('backup', 'taken'), 0) + 1
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
    print(f'{runs * 7} plans, {steps} lines: {bad} differ')
    print('fight answers:', ' '.join(f'{n}={v}:{c}' for (n, v), c in sorted(seen.items())))
    pm_floors.floors(plans=(runs, 100), lines=(steps, 1000),
                     **{f'{n}_{v}': (seen.get((n, v), 0), 5) for n in ('in_range', 'at_stop', 'no_target', 'may_go', 'off_way') for v in '01'},
                     way_own=(seen.get(('way', '1'), 0), 5), way_start=(seen.get(('way', '2'), 0), 5),
                     queued=(seen.get(('queued', '1'), 0), 1),
                     moved=(seen.get(('move', '1'), 0), 5), waited=(seen.get(('move', '2'), 0), 5),
                     taken=(seen.get(('taken', '1'), 0), 5),
                     **{f'backup_{k}': (seen.get(('backup', k), 0), 5) for k in '0123'},
                     backup_taken=(seen.get(('backup', 'taken'), 0), 1),
                     **{f'terminus_{k}': (seen.get(('terminus', k), 0), 5) for k in ('1', 'back')},
                     **{f'guard_{k}': (seen.get(('guard', k), 0), 1) for k in '023'},
                     **{f'circle_{k}': (seen.get(('circle', k), 0), 1) for k in ('cleared', 'sent0', 'sent', 'left', 'right')},
                     **{f'step_{k}': (seen.get(('step', str(k)), 0), 5) for k in range(1, 11)})
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
