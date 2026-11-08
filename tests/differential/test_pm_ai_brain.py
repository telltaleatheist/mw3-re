"""PM's brain against the exe at 53 bits, exact. The frame 0x4fab90: everything it calls stands in, with answers from
one script of bits, and the calls in order and the brain's fields must match the port's (mech3/zai_brain_pm,
tests/probe_pm_ai_brain.cpp). What the stand-ins do is tested piece by piece.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_ai_brain.py [cases]
"""
import ctypes
import math
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
os.environ['MW3_X87'] = '53'  # the reference's precision: PM's AI, docs/pm-tests.md
import pm_floors  # noqa: E402
import test_pm_zar  # noqa: E402

TICK, NOW, ON, AWAKE, RAND = 0x4fab90, 0x62da90, 0x630055, 0x62db48, 0x5de668
FLT_MAX = 0x7f7fffff
PICK, PLAYER, TARGET, NET, BEH, CUR_NET, CUR_STATE, WORST = 0x51, 0x52, 0x53, 0x71, 0x72, 0x73, 0x74, 0x75


def fb(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


class Exe:
    real = ()

    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        self.w = lambda a, v: self.uc.mem_write(a, struct.pack('<I', v & 0xffffffff))
        a = iter(range(z.alloc(0x8000), 1 << 31, 0x400))
        w = self.w
        # The controls' first word is both the offset table (+4 is 0) and the vtable.
        self.brain, bvt, ctl, self.veh, vvt, steer, svt = (next(a) for _ in range(7))
        self.pick, self.player, self.target, chain, cvt2, ent, evt, self.pv, pvt, self.v, tvt = (next(a) for _ in range(11))
        self.rec, nvt, cn, self.cells = (next(a) for _ in range(4))
        self.cn = cn
        self.inp = None
        w(self.brain, bvt), w(self.brain + 0x100, ctl), w(ctl + 4, 0), w(self.brain + 0x11c, self.veh)
        w(self.brain + 0x210, svt), w(self.veh, vvt), w(self.brain + 0x124, 0x4444)
        w(self.brain + 0x2c8, 0)
        self.radio = next(a)
        [w(self.radio + o, 0x1000 + o) for o in (0, 0x18, 0x20, 0x24, 0x28)]
        w(self.pick, 0x1), w(self.player + 4, chain), w(chain, cvt2)
        w(cvt2 + 0x2c, self.ret(lambda u: ent)), w(ent, evt), w(evt + 0x48, self.ret(lambda u: self.ans_ptr(6, self.pv)))
        w(self.v, tvt), w(tvt + 0xe4, self.ret(lambda u: self.ans_ptr(20, NET)))
        w(vvt + 0xac, self.ret(lambda u: self.ans(1)))
        w(svt + 4, self.ret(lambda u: self.put(2, u.arg(0)), 8))
        w(ctl + 0x14, self.ret(lambda u: self.put(3))), w(bvt + 0x10, self.ret(lambda u: self.put(4, u.arg(0)), 4))
        w(ctl + 0x98, self.ret(lambda u: self.ans(9))), w(bvt + 0x28, self.ret(lambda u: self.put(13, u.arg(0)), 4))
        self.uc.mem_write(0x4b8f50, b'\xdd\x05' + struct.pack('<I', self.cells) + b'\xc3')
        w(self.rec + 0xc, cn), w(self.rec + 0x10, CUR_STATE), w(cn, nvt), w(nvt + 0x18, self.ret(lambda u: 0))
        w(RAND, self.ret(lambda u: self.rand()))
        for addr, fn, pop in ((0x4e7010, lambda u: self.ans_ptr(5, self.pick), 4), (0x4f8ae0, lambda u: self.put(7, u.arg(0)), 0),
                              (0x4fa9e0, self.vlist, 0xc), (0x4fa6d0, lambda u: self.ans(10), 0), (0x4ba8c0, lambda u: self.put(11), 0),
                              (0x4b2820, self.worst, 0), (0x4fab30, lambda u: self.put(14), 0), (0x4fbf30, self.current, 0),
                              (0x4fa5a0, self.engage, 0xc), (0x4f8d70, self.tv, 0), (0x4c7950, lambda u: self.ans_ptr(21, BEH), 0),
                              (0x4fa150, self.attack, 8), (0x4fb260, lambda u: self.put(24, u.arg(0)), 4),
                              (0x4fb3d0, lambda u: self.put(25, u.arg(0)), 4)):
            if addr not in self.real:
                z.native_stub(addr, fn, pop)
        # 0x4f8d70's answer then vt+0x48: one call in the port, so the second just passes on.
        self.tent = next(a)
        tevt = next(a)
        w(self.tent, tevt), w(tevt + 0x48, self.ret(lambda u: self.v))

    def ret(self, fn, pop=0):
        t = self.z.alloc(4)
        self.uc.mem_write(t, b'\xc3')
        self.z.native_stub(t, fn, pop)
        return t

    def put(self, k, v=0):
        self.log += [k, v]
        return 0

    def bit(self):
        b = self.bits[self.n]
        self.n += 1
        return b

    def ans(self, k):
        self.put(k)
        return self.bit()

    def ans_ptr(self, k, p):
        self.put(k)
        return p if self.bit() else 0

    def rand(self):
        self.put(23)
        return self.inp[4]

    def vlist(self, u):
        self.put(8)
        if self.bit():
            self.uc.mem_write(u.arg(1), b'\x01')
            self.w(u.arg(2), self.pick if self.bit() else 0)
        return 0

    def worst(self, u):
        self.put(12)
        return WORST

    def current(self, u):
        self.put(15)
        return self.rec

    def engage(self, u):
        self.put(17, CUR_NET if u.arg(1) == self.cn else 0), self.put(17, u.arg(2))
        return self.bit()

    def tv(self, u):
        c = {self.pick: PICK, self.player: PLAYER, self.target: TARGET}[u.ecx]
        self.put(18, c)
        return self.tent if self.bit() else 0

    def attack(self, u):
        self.put(22, u.arg(0)), self.put(22, u.arg(1))
        return 0

    def run(self, ints, fl, bits):
        w, b, v = self.w, self.brain, self.veh
        self.log, self.bits, self.n, self.inp = [], bits, 0, ints
        self.uc.mem_write(ON, bytes([1 if ints[0] else 0]))
        w(NOW, fb(fl[1])), w(AWAKE, 0)
        w(v + 0x98, self.player if ints[1] else 0), w(v + 0x6c, self.target if ints[2] else 0), w(v + 0xd4, 0x8000 if ints[3] else 0)
        for off, x in ((0x3c, ints[5]), (0x5c, ints[6]), (0x88, ints[7]), (0x8c, ints[8]), (0x9c, ints[9]), (0xa0, ints[10]), (0x110, ints[11])):
            w(b + off, x)
        self.uc.mem_write(b + 0x2c4, bytes([ints[12]]))
        w(b + 0x2c8, self.radio if ints[13] else 0)
        for off, i in ((0x1c, 2), (0x20, 3), (0x64, 4), (0x68, 5), (0xb0, 6), (0xfc, 7), (0x114, 8), (0x120, 9)):
            w(b + off, fb(fl[i]))
        w(self.pick + 0xc, fb(fl[10])), w(self.player + 0xc, fb(fl[11])), w(self.target + 0xc, fb(fl[12]))
        w(self.pv + 0xa4, fb(fl[13])), self.uc.mem_write(self.cells, struct.pack('<ii', ints[14], ints[15]))
        r = self.z.call(TICK, ecx=b, args=(fb(fl[0]),)) & 0xff
        g = self.z.r32
        return [r, g(b + 0x3c), g(b + 0xfc), g(b + 0x114), g(b + 0x120), g(AWAKE), self.n] + self.log


class Side(Exe):
    """The smaller pieces, with their real neighbours standing in."""
    real = (0x4fa5a0, 0x4fab30)

    def __init__(self):
        super().__init__()
        a = iter(range(self.z.alloc(0x4000), 1 << 31, 0x400))
        w, self.pos, self.home, self.rec2, self.ivt, self.net, nvt2, self.st, svt2 = self.w, *(next(a) for _ in range(8))
        self.at, avt, self.tg, self.pl = next(a), next(a), next(a), next(a)
        self.vv = next(a)
        v = self.veh
        vvt = self.z.r32(v)
        w(self.net, nvt2), w(self.st, svt2), w(nvt2 + 0xc, self.ret(lambda u: self.ans(30))), w(svt2 + 4, self.ret(lambda u: self.ans(31)))
        w(vvt + 0xa4, self.ret_st(lambda u: self.put(32), self.cells + 8, b'\xc3'))
        w(vvt + 0x124, self.ret_st(lambda u: self.put(40, u.arg(1)), self.cells + 12, b'\xc2\x04\x00'))
        w(self.at, avt), w(avt + 0x28, self.ret(lambda u: self.team())), w(avt + 0x58, self.ret(lambda u: self.who()))
        w(0x62c40c, self.pl), w(self.pl + 0x14, self.vv)
        for addr, fn, pop in ((0x5cb260, lambda u: self.pos, 0), (0x4b2900, self.record, 8), (0x4e6240, lambda u: self.put(37), 0),
                              (0x4b2a00, self.base, 0x10), (0x4b2ab0, lambda u: self.put(39), 0),
                              (0x4fa170, lambda u: self.put(33, {0x627cf8: 0, 0x627ce8: 1}[u.arg(0)]), 4)):
            self.z.native_stub(addr, fn, pop)
        w(self.brain + 0x210, svt2 + 0x100), w(svt2 + 0x100, self.ret(self.way, 0x10))

    def ret_st(self, log, cell, end):
        """Logs through a stand-in called first (the argument one word further), then answers in st(0)."""
        return self.z.cpu.code(b'\xb8' + struct.pack('<I', self.ret(log)) + b'\xff\xd0\xd9\x05' + struct.pack('<I', cell) + end)

    def team(self):
        self.put(34)
        self.w(self.tg + 0xc0, self.side_in[1])
        return self.tg

    def who(self):
        self.put(35)
        return self.vv if self.side_in[2] else self.at

    def record(self, u):
        self.put(36, 0x90 if u.arg(0) == self.vv else 0x91)
        return self.rec2 if self.bit() else 0

    def base(self, u):
        for i in range(3):
            self.put(38, u.arg(i))
        self.put(38, 0x95 if u.arg(3) == self.at else 0x80 if u.arg(3) == self.veh else u.arg(3))
        return 0

    def way(self, u):
        g = self.z.r32
        for p in (u.arg(0), u.arg(1)):
            for i in range(3):
                self.put(41, g(p + i * 4))
        self.put(41, u.arg(2)), self.put(41, 0x80 if u.arg(3) == self.veh else u.arg(3))
        return 0

    def side(self, what, ints, fl, bits):
        w, b, v = self.w, self.brain, self.veh
        self.log, self.bits, self.n, self.side_in = [], bits, 0, ints
        w(NOW, fb(fl[0])), w(0x8c1b50, fb(fl[1]))
        for i in range(3):
            w(self.pos + i * 4, fb(fl[2 + i])), w(self.rec2 + i * 4, fb(fl[7 + i])), w(self.home + i * 4, fb(fl[11 + i]))
            w(b + 8 + i * 4, fb(fl[18 + i]))
            w(b + 0x2b8 + i * 4, 0)
        w(self.cells + 8, fb(fl[5])), w(self.cells + 12, fb(fl[10]))
        self.logged_health = True
        w(v + 0xc4, ints[0]), w(v + 0x98, self.player if ints[3] else 0), w(self.player + 0xc, fb(fl[6])), w(v + 4, ints[4])
        w(b + 0x3c, ints[5]), self.uc.mem_write(b + 0xec, bytes([ints[6]])), w(b + 0x104, self.home if ints[7] else 0)
        w(b + 0xa4, ints[8]), w(b + 0x2c8, self.radio if ints[9] else 0)
        w(b + 0x24, fb(fl[14])), w(b + 0x50, fb(fl[15])), w(b + 0x108, fb(fl[16])), w(b + 0x1f0, fb(fl[17])), w(b + 0x114, 0)
        w(b + 0x10c, fb(fl[21]))
        r = 0
        if what == 0:
            r = self.z.call(0x4fa5a0, ecx=b, args=(v, self.net if ints[13] else 0, self.st if ints[14] else 0)) & 0xff
        elif what == 1:
            self.z.call(0x4fab30, ecx=b)
        elif what == 2:
            self.w(self.brain + 0x11c, v)
            self.z.call(0x4fbc20, ecx=b + 0x2d4, args=(11, 22, 33, v if ints[10] else 0 if ints[11] else self.at))
        elif what == 3:
            self.z.call(0x4fbdd0, ecx=b + 0x2d4)
        elif what == 5:
            r = self.z.call(0x4fa210, ecx=b, args=(v,)) & 0xff
        elif what == 6:  # the controls' vt+0x78, after its thunk: +0x11c read here direct, not through the base
            w(b + 0x110, ints[15]), w(b + 0x98, ints[16]), w(b + 0x11c, fb(fl[22])), w(b + 0xa8, fb(fl[23]))
            r = self.z.call(0x4e8d40, ecx=b + 0x2d4) & 0xff
            hold = self.z.r32(b + 0x11c)
            self.w(self.brain + 0x11c, v)
            return [r, hold, 0, 0, 0, self.n] + self.log
        else:
            self.w(self.brain + 0x11c, v if ints[12] else 0)
            self.z.call(0x4fbe90, ecx=b)
            self.w(self.brain + 0x11c, v)
        g = self.z.r32
        hold = g(b + 0x114)
        return [r, hold, g(b + 0x2b8), g(b + 0x2bc), g(b + 0x2c0), self.n] + self.log


class Orders(Exe):
    """The orders on the exe's own deque and list, what they run standing in."""
    real = (0x4fa6d0, 0x4fb260, 0x4fbf30)

    def __init__(self):
        super().__init__()
        z, w = self.z, self.w
        self.ov, iv = z.alloc(0x100), z.alloc(16)
        self.o = self.brain + 0x128
        self.objs = {}
        self.next_part = 0x100
        z.native_stub(0x5ce8c6, lambda u: z.alloc(u.arg(0)))
        z.native_stub(0x5ce8c0, lambda u: 0)
        z.native_stub(0x4ea0b0, self.item_gone)
        z.native_stub(0x4f9f30, self.run_order, 4)
        for slot, fn, pop in ((0, lambda u: self.put(56, self.id(u)), 4), (8, lambda u: self.ids(50, u), 0), (0x10, lambda u: self.put(62, self.id(u)), 0), (0x14, lambda u: self.ids(51, u), 0),
                              (0x20, lambda u: self.ids(52, u), 0), (0xc, lambda u: self.put(65, self.id(u)), 0), (0x18, self.net_of, 0), (0x34, self.as_net, 0), (0x54, lambda u: self.make(54, u), 8), (0x8c, lambda u: self.make(55, u), 8)):
            w(self.ov + slot, self.ret(fn, pop))
        w(iv, self.ret(self.item_run, 4))
        self.iv = iv
        for addr, i in ((0x627c68, 0xd1), (0x627cc8, 0xd2), (0x62faf8, 0xd3)):
            self.obj(i, addr)
        head = z.alloc(12)
        w(head, head), w(head + 4, head)
        w(self.o + 0x38, head)
        self.head = head

    def obj(self, i, at=None):
        a = at or self.z.alloc(16)
        self.w(a, self.ov), self.w(a + 4, i)
        self.objs[i] = a
        return a

    def id(self, u):
        return self.z.r32(u.ecx + 4)

    def ids(self, k, u):
        self.put(k, self.id(u))
        return self.bit()

    def net_of(self, u):
        self.put(66, self.id(u))
        return u.ecx

    def as_net(self, u):
        self.put(53, self.id(u))
        return u.ecx if self.bit() else 0

    def make(self, k, u):
        self.put(k, self.id(u))
        a = self.obj(self.next_part)
        self.next_part += 1
        return a

    def run_order(self, u):
        self.put(57, self.z.r32(self.z.r32(u.ecx + 0xc) + 4)), self.put(57, u.arg(0))
        if self.bit():
            self.uc.mem_write(self.o + 0x30, b'\x01')
        return 0

    def item_run(self, u):
        self.put(58, self.z.r32(u.ecx + 4))
        return 0

    def item_gone(self, u):
        p = self.z.r32(u.ecx)
        if p:
            self.put(59, self.z.r32(p + 4))
        self.w(u.ecx, 0)
        return 0

    def top(self):
        return self.z.call(0x4fbf30, ecx=self.o)

    def scene(self, ops, bits):
        z, w, o = self.z, self.w, self.o
        self.log, self.bits, self.n = [], bits, 0
        for op, a, b in ops:
            if op == 0:
                z.call(0x4fafe0, ecx=o)
                z.call(0x4f9e70, ecx=self.top(), args=(self.objs[a] if a else 0, self.objs[b] if b else 0))
            elif op == 1:
                self.uc.mem_write(self.top() + 4, bytes([a]))
            elif op == 2:
                self.put(60, z.call(0x4fa6d0, ecx=self.brain) & 0xff)
            elif op == 3:
                self.put(60, z.call(0x4fa470, ecx=self.brain) & 0xff)
            elif op == 4:
                z.call(0x4fb260, ecx=o, args=(0x3d000000,))
            elif op == 5:
                self.put(60, z.call(0x4fa860, ecx=self.brain + 0x2d4, args=(self.objs[a] if a else 0,)) & 0xff)
            elif op == 9:
                z.call(0x4ea740, ecx=o, args=(self.objs[a] if a else 0, self.objs[b] if b else 0))
            elif op == 10:
                z.call(0x4ea7f0, ecx=o, args=(self.objs[b] if b else 0,))
            elif op == 8:  # the new order has no parts, and everything here reads them: given the default ones
                z.call(0x4ea5f0, ecx=o)
                z.call(0x4f9e70, ecx=self.top(), args=(0, 0))
            else:
                item = z.alloc(8)
                w(item, self.iv), w(item + 4, a)
                z.call(0x4ea180 if op == 7 else 0x4ea0f0, ecx=o + 0x34, args=(item,))
            self.put(61, z.r32(o + 0x2c))
        return self.log


def orders_case(r, x):
    ops = [(0, 0xa1, 0xa2)]
    depth = 1
    for _ in range(r.randrange(10, 120)):
        k = r.random()
        if k < 0.45:
            ops.append((0, r.choice([0, 0xa1, 0xa3]), r.choice([0, 0xa2, 0xa4])))
            depth += 1
        elif k < 0.55:
            ops.append((1, r.randrange(2), 0))
        elif k < 0.65:
            ops.append((2, 0, 0))
        elif k < 0.75:
            ops.append((3, 0, 0))
        elif k < 0.85:
            ops.append((4, 0, 0))
        elif k < 0.89:
            ops.append((6, r.randrange(1, 99), 0))
        elif k < 0.92:
            ops.append((7, r.randrange(1, 99), 0))
        elif k < 0.935:
            ops.append((8, 0, 0))
        elif k < 0.94:
            ops.append((9, r.choice([0, 0xa1]), r.choice([0, 0xa2])))
        elif k < 0.945:
            ops.append((10, 0, r.choice([0, 0xa2, 0xa4])))
        elif k < 0.95:
            ops.append((5, r.choice([0, 0xa1]), 0))
    return ops, bytes(r.randrange(2) for _ in range(1024))


class Net(Exe):
    """A vehicle net's table as 0x4c6f90 makes it, and the behaviours asked of it."""
    real = (0x4c7950,)

    def __init__(self):
        super().__init__()
        z, w = self.z, self.w
        z.native_stub(0x5ce8c6, lambda u: z.alloc(u.arg(0)))
        z.native_stub(0x5ce8c0, lambda u: 0)
        z.native_stub(0x5acbe0, lambda u: 0x600)
        z.native_stub(0x4e54c0, lambda u: 0, 4)
        self.net = z.alloc(0x40)
        z.call(0x4c6f90, ecx=self.net, args=(0x1234,))
        self.slots = [z.r32(z.r32(self.net + 0xc) + i * 4) for i in range(10)]
        self.table = z.alloc(0x28)
        self.objs = []
        self.w(self.net + 0xc, self.table)
        for i in range(10):
            o, v, o2, v2 = z.alloc(4), z.alloc(0x70), z.alloc(4), z.alloc(0xa0)
            w(o, v), w(o2, v2), w(v + 0x60, self.ret(lambda u, a=o2: a)), w(v2 + 0x74, self.ret(lambda u: u.ecx))
            for off in (0x90, 0x94, 0x98):
                w(v2 + off, self.ret(lambda u, i=i, off=off: (0x200 + i) * 0x1000 + off))
            self.objs.append(o)

    def ask(self, which, live):
        for i in range(10):
            self.w(self.table + i * 4, self.objs[i] if live[i] else 0)
        if which < 0:
            return self.z.call({-1: 0x4c7920, -2: 0x4c7950, -3: 0x4c7980}[which], ecx=self.net)
        return self.z.call(0x4c7800, ecx=self.net, args=(which - 16,))


class Lead(Exe):
    """0x4e7210 and 0x4e6330, real."""
    real = ()

    def __init__(self):
        super().__init__()
        z, w = self.z, self.w
        self.me, mv, self.con, self.ent, ev, self.wp, self.df, self.pil = (z.alloc(0x200) for _ in range(8))
        self.cells = z.alloc(0x100)
        self.tv, self.ov, self.pv2, self.at, self.mz = (self.cells + o for o in (0, 12, 24, 36, 48))
        self.flags_cell = 0
        w(self.me, mv), w(self.con + 8, self.ent), w(self.ent, ev), w(ev + 0xc, self.ret(lambda u: self.tv)), w(mv + 0xc, self.ret(lambda u: self.ov))
        w(mv + 0x58, self.ret(lambda u: self.pil if self.pilot else 0))
        w(self.wp + 0x30, self.df)
        z.native_stub(0x5cb2b0, lambda u: self.at)
        z.native_stub(0x4b76d0, lambda u: self.mz if self.has_muzzle else 0, 4)
        self.target = z.alloc(0x200)
        tv2 = z.alloc(0x40)
        w(self.target, tv2), w(tv2 + 0x28, self.ret(lambda u: self.target))
        z.native_stub(0x4f8d70, lambda u: self.target)
        self.owner = z.alloc(0x200)

    def fs(self, a, v):
        for i, x in enumerate(v):
            self.w(a + i * 4, fb(x))

    def lead(self, ints, fl):
        self.pilot, self.has_muzzle = ints[0], ints[2]
        g = self.z.r32
        self.fs(self.mz, fl[0:3]), self.fs(self.ov, fl[3:6]), self.fs(self.tv, fl[6:9]), self.fs(self.pil + 0x100, fl[9:12]), self.fs(self.at, fl[12:15])
        self.w(self.df + 0x6c, ints[1]), self.fs(self.df + 0x2c, fl[15:16])
        o = self.z.alloc(12)
        r = self.z.call(0x4e7210, ecx=self.me, args=(self.con, self.wp if ints[3] else 0, o)) & 0xff
        return [r, g(o), g(o + 4), g(o + 8)]

    def set_target(self, ints, old):
        w, o, g = self.w, self.owner, self.z.r32
        for i, off in enumerate((0x6c, 0x70, 0x74, 0x78)):
            w(o + off, old[i])
        w(o + 0xbc, ints[0]), w(o + 0xc4, ints[1]), w(o + 0xc8, ints[2])
        w(self.target + 0xc0, ints[5]), w(self.target + 0xbc, ints[6])
        self.z.call(0x4e6330, ecx=o, args=(0x77 if ints[4] else 0, ints[3]))
        return [g(o + off) for off in (0x6c, 0x70, 0x74, 0x78)]


def lead_case(r):
    s = r.choice([0.0, 50.0, 300.0, 1000.0])
    v = lambda n=30: [r.uniform(-n, n) for _ in range(3)]  # noqa: E731
    pil = v()
    k = r.random()
    own = pil[:] if k < 0.1 else [pil[0] - s, pil[1], pil[2]] if k < 0.2 else [x - r.uniform(-0.001, 0.001) for x in pil] if k < 0.3 else v()
    at = v(400)
    mz = v(40) if r.random() < 0.9 else at[:]
    if 0.1 <= k < 0.2 and r.random() < 0.5:  # square term 0, and the way across the motion: no time at all
        mz[0] = at[0]
    return ([r.choice([0, 1, 1, 1]), r.choice([0, 0, 0, 2, 0x800, 1, 0x803]), r.choice([0, 1, 1, 1, 1]), r.choice([0, 1, 1, 1, 1])],
            mz + own + v() + pil + at + [s])


def reach_cases(x, r, lib):
    """0x4a7000 on made-up cones and 0x467490 on made-up limits, real on the exe."""
    z, cone, dir_, out, torso = x.z, x.z.alloc(0x60), x.z.alloc(12), x.z.alloc(0x20), x.z.alloc(0x200)
    x.uc.mem_write(0x5cefd6, bytes.fromhex('d9c0 d8c8 d9e8 dee1 d9fa d9f3 c3'))  # _CIasin: atan2(x, sqrt(1 - x * x))
    bad = yes = no = 0
    pack = lambda v: struct.pack(f'<{len(v)}f', *v)  # noqa: E731
    for i in range(4000):
        a = r.uniform(-3.2, 3.2)
        y = r.choice([0.0, r.uniform(-1, 1), 0.99899, 0.9991, -0.9995, 1.0])
        k = (1 - y * y) ** 0.5
        d = [k * math.sin(a), y, -k * math.cos(a)]
        band = lambda: [math.cos(u := r.uniform(0, 6.3)), 0.0, math.sin(u), r.uniform(-0.3, 0.3)]  # noqa: E731
        c = band() + band() + [0.0] * 6 + [r.uniform(-1, 0.2), r.uniform(-0.2, 1), 0.0, 0.0]
        x.uc.mem_write(cone, pack(c)), x.uc.mem_write(dir_, pack(d))
        p = z.call(0x4a7000, ecx=cone, args=(dir_, out)) & 0xff
        q = lib.probe_pm_firepoint_reaches((ctypes.c_float * 18)(*c), (ctypes.c_float * 3)(*d)) & 0xff
        lim = [r.uniform(-1.5, 0), r.uniform(0, 1.5), r.uniform(0, 3.2)]
        x.uc.mem_write(torso + 0x148, pack(lim))
        p2 = z.call(0x467490, ecx=torso, args=(dir_, out, out + 4)) & 1
        q2 = lib.probe_pm_torso_reaches((ctypes.c_float * 3)(*lim), (ctypes.c_float * 3)(*d)) & 1
        yes += p + p2
        no += 2 - p - p2
        if p != q or p2 != q2:
            bad += 1
            if bad < 4:
                print(f'reach {c} {d} {lim}: exe {p} {p2} port {q} {q2}')
    return bad, yes, no


def line_cases(x, r, lib):
    """0x4b7800 real: a weapon with or without a part, node and firepoint, a salvo or not, a point or none."""
    z, w = x.z, x.w
    wp, fp, part, n41, n42, ax41, ax42, at, mz, dir_, c1, c2 = (z.alloc(0xa0) for _ in range(12))
    rolls = []
    z.native_stub(0x4ff3c0, lambda u: line_place(x, u), 0)
    w(0x5de668, x.ret(lambda u: rolls.pop(0)))
    pack = lambda v: struct.pack(f'<{len(v)}f', *v)  # noqa: E731
    bad = sorts = 0
    seen = set()
    for i in range(3000):
        ints = [r.randrange(2), r.choice([0, 1, 1]), r.randrange(2), r.choice([0, 0, 1]), r.choice([0, 1, 1]), int(r.random() < 0.1),
                r.randrange(32768), r.randrange(32768)]
        unit = lambda: (lambda v: [c / math.sqrt(sum(d * d for d in v)) for c in v])([r.uniform(-1, 1) for _ in range(3)])  # noqa: E731
        f = [r.choice([0.0, 0.5, 1.0]), *[r.uniform(-2, 2) for _ in range(3)], r.uniform(0, 3), r.choice([0.2, 0.47, 0.9, -1.0]),
             *[r.uniform(-1, 1) for _ in range(3)], *[r.uniform(-50, 50) for _ in range(3)], *unit(), *unit()]
        if i % 10 == 0:  # the tolerance met exactly: the way straight down the node's axis, the tolerance 1
            ints[0:6] = [0, 1, 1, 0, 1, 0]
            f[0], f[5], f[6:9], f[9:12], f[15:18] = 0.0, 1.0, [0.0] * 3, [1.0, 2.0, -50.0], [0.0, 0.0, 1.0]
        rolls[:] = ints[6:8]
        w(wp + 0x74, fb(f[0])), w(wp + 0x28, part if ints[0] else 0), w(part + 8, n41), w(wp + 0x24, n42 if ints[1] else 0)
        w(wp + 0x84, fp if ints[2] else 0), x.uc.mem_write(fp, pack([f[5]])), x.uc.mem_write(fp + 0x60, pack([f[4]]))
        x.uc.mem_write(fp + 0x70, pack(f[1:4])), w(wp + 0x6c, ints[3]), x.uc.mem_write(wp + 0x88, pack(f[6:9]))
        x.uc.mem_write(at, pack(f[9:12])), w(n41 + 0x38, ax41 - 0x78), x.uc.mem_write(ax41, pack(f[12:15]))
        w(n42 + 0x38, ax42 - 0x78), x.uc.mem_write(ax42, pack(f[15:18])), x.uc.mem_write(0x62f86c, bytes([ints[5]]))
        for a in (mz, dir_, c1, c2):
            x.uc.mem_write(a, pack([-7.0] * 3))
        ans = z.call(0x4b7800, ecx=wp, args=(at if ints[4] else 0, mz, dir_, 0, c1, c2)) & 0xff
        g = z.r32
        a = [ans] + [g(mz + 4 * k) for k in range(3)] + [g(dir_ + 4 * k) for k in range(3)] + [g(c1), g(c2)]
        o = (ctypes.c_float * 9)()
        h_ints = [ints[0], ints[1], ints[2], ints[3], ints[4], ints[5], ints[6], ints[7]]
        rolls[:] = []
        lib.probe_pm_weapon_line((ctypes.c_int32 * 8)(*h_ints), (ctypes.c_float * 18)(*f), o)
        b = [int(o[0])] + [fb(v) for v in o[1:]]
        if ans == 0 and not ints[1] and not (ints[0] and ints[2]) and f[0] != 1.0:
            a[4:9] = b[4:9]  # the exe leaves these unwritten: the FIX's answer stands
        seen.add((ans, ints[2], ints[3], a[8] == fb(1.0)))
        if a != b:
            bad += 1
            if bad < 4:
                print(f'line {ints} {f}\n  exe  {[hex(v) for v in a]}\n  port {[hex(v) for v in b]}')
    return bad, len(seen)


def line_place(x, u):
    node, v = u.arg(0), u.arg(1)
    if not node:
        return 1
    p = list(struct.unpack('<3f', bytes(x.uc.mem_read(v, 12))))
    x.uc.mem_write(v, struct.pack('<3f', p[0] + 1.0, p[1] + 2.0, p[2] + 3.0))
    return 0


def scan_exe(tests):
    """0x4d5a10: tests through an adjustment on the brain, the again flag, then the next step."""
    x = Exe()
    z, w = x.z, x.w
    st, order, step, table = z.alloc(16), z.alloc(16), z.alloc(0x20), z.alloc(0x40)
    w(st + 4, order), w(st + 8, step), w(order + 8, x.brain), w(step + 0xc, 0xb7)
    adj = 0x10
    for i in range(tests):
        w(table + i * 8, x.ret((lambda k: lambda u: x.put(63, k if u.ecx == x.brain + adj else 999))(i))), w(table + i * 8 + 4, adj)
    w(step + 0x14, table), w(step + 0x18, table + tests * 8)
    x.log = []
    z.native_stub(0x4f9f00, lambda u: x.put(64, u.arg(0)) if u.ecx == order else 1 / 0, 4)
    z.call(0x4d5a10, ecx=st, args=(0,))
    return x.log + [x.uc.mem_read(x.brain + 0x158, 1)[0]]


def side_case(r, what):
    now = r.choice([0.5, 100.0])
    ints = [r.choice([0, 1, 6]), r.choice([0, 2, 6]), r.randrange(2), r.randrange(2), r.randrange(2), r.choice([0, 1, 1, 1, 2]),
            r.randrange(16), r.randrange(2), r.randrange(3), r.randrange(2), int(r.random() < 0.15), int(r.random() < 0.15), r.randrange(2),
            r.randrange(2), r.randrange(2), int(r.random() < 0.2) if what == 6 else 0, int(r.random() < 0.8) if what == 6 else 0]
    if what == 2 and r.random() < 0.5:  # the player hitting an ally
        ints[0], ints[1], ints[2], ints[10], ints[11] = 6, 6, 1, 0, 0
    d = r.choice([0.0, 2.9, 3.0, 3.1, 6.9, 7.0, 7.5])
    home = [r.uniform(-30, 30) for _ in range(3)]
    p = [h + r.uniform(-20, 20) for h in home]
    fl = [now, 500.0 + r.choice([0.0, 0.5]), *p, r.choice([0.2, 0.5, 0.8]), r.choice([100.0, 500.0, 900.0]),
          r.choice([5.0, 10.0, 15.0]), 500.0 - d, 500.0 - r.choice([0.0, 2.9, 3.0, 3.5, 3.1, 8.0]),
          r.choice([0.0, 1e-30, 4.25, 30.0]), *home, r.choice([100.0, 400.0, 900.0]), r.choice([0.2, 0.5, 0.8]),
          now + r.choice([-1, 0, 1]), r.choice([0.2, 0.5, 0.8]), *[r.uniform(-9, 9) for _ in range(3)], r.uniform(0, 2),
          *([now + r.choice([-1.0, 0.0, 1.0, float('nan')]), r.choice([0.0, 2.5, 10.0])] if what == 6 else [0.0, 0.0])]
    return ints, fl, bytes(r.randrange(2) for _ in range(16))


def case(r):
    now = r.choice([0.5, 100.0, 1234.5])
    t = lambda: r.choice([now - 5, now, now + 5, 3.4028235e38])  # noqa: E731
    d = lambda: r.choice([10.0, 300.0, 400.0, 400.0, 90000.0])  # noqa: E731
    ints = [int(r.random() < 0.95), r.randrange(2), r.randrange(2), r.randrange(2), r.randrange(32768), r.randrange(4),
            r.randrange(2), int(r.random() < 0.3), r.randrange(2), r.randrange(2), r.randrange(2), int(r.random() < 0.2),
            r.randrange(2), r.randrange(2)]
    ints += struct.unpack('<ii', struct.pack('<d', r.choice([0.05, 0.1, 0.10000000075, 0.100000001490116, 0.1000000015, 0.2, 0.25, 0.5])))
    fl = [r.uniform(0.01, 0.1), now, 400.0, 400.0, r.uniform(0, 30), r.uniform(0, 60), r.uniform(0, 20), t(), t(), t(),
          d(), d(), d(), d(), r.choice([0.05, 0.1, 0.100000001, 0.2, 0.25, 0.5])]
    return ints, fl, bytes(r.randrange(2) for _ in range(64))


def main():
    cases = int(sys.argv[1]) if len(sys.argv) > 1 else 6000
    lib = test_pm_zar.load_pm_probe()
    fn = lib.probe_pm_ai_tick
    x = Exe()
    r = random.Random(77)
    out = (ctypes.c_uint32 * 512)()
    big = (ctypes.c_uint32 * 8192)()
    bad = 0
    seen = {}
    for i in range(cases):
        ints, fl, bits = case(r)
        a = x.run(ints, fl, bits)
        n = fn((ctypes.c_int32 * len(ints))(*ints), bits, (ctypes.c_float * len(fl))(*fl), out, 512)
        b = list(out[:n])
        for k in a[7::2]:
            seen[k] = seen.get(k, 0) + 1
        seen['asleep', a[0]] = seen.get(('asleep', a[0]), 0) + 1
        seen['mode', a[1] == 1 and ints[5] != 1] = 1
        if a != b:
            print(f'case {i} {ints} {fl}\n  exe  {a}\n  port {b}')
            bad += 1
            if bad >= 6:
                break
    print(f'{cases} frames: {bad} differ')
    sx = Side()
    sf = lib.probe_pm_ai_side
    for i in range(cases):
        what = i % 6
        ints, fl, bits = side_case(r, what)
        a = sx.side(what, ints, fl, bits)
        n = sf(what, (ctypes.c_int32 * len(ints))(*ints), bits, (ctypes.c_float * len(fl))(*fl), out, 512)
        b = list(out[:n])
        for k in a[6::2]:
            seen[k] = seen.get(k, 0) + 1
        seen['ret', a[0]] = seen.get(('ret', a[0]), 0) + 1
        if a != b:
            print(f'side {what} case {i} {ints} {fl}\n  exe  {a}\n  port {b}')
            bad += 1
            if bad >= 6:
                break
    print(f'{cases} side: {bad} differ')
    # 0x4e8d40 (the step's "can't hit from here"), on its own numbers so the cases above and below draw as before.
    r6, aimed = random.Random(66), {}
    for i in range(cases // 3):
        ints, fl, bits = side_case(r6, 6)
        a = sx.side(6, ints, fl, bits)
        n = sf(6, (ctypes.c_int32 * len(ints))(*ints), bits, (ctypes.c_float * len(fl))(*fl), out, 512)
        b = list(out[:n])
        moved = a[1] != struct.unpack('<I', struct.pack('<f', fl[22]))[0]
        aimed[a[0], moved] = aimed.get((a[0], moved), 0) + 1
        if a != b:
            print(f'scan_aim case {i} {ints} {fl}\n  exe  {a}\n  port {b}')
            bad += 1
            if bad >= 6:
                break
    print(f'{cases // 3} scan_aim: {bad} differ; (answer, +0x11c moved on): {aimed}')
    for i in range(max(cases // 200, 30)):
        ox = Orders()
        for k in (0xa1, 0xa2, 0xa3, 0xa4):
            ox.obj(k)
        ops, bits = orders_case(r, ox)
        a = ox.scene(ops, bits)
        flat = [v for op in ops for v in op]
        n = lib.probe_pm_ai_orders((ctypes.c_int32 * len(flat))(*flat), len(ops), bits, big, len(big))
        b = list(big[:n])
        for k in a[::2]:
            seen[k] = seen.get(k, 0) + 1
        seen['depth', 'past a block'] = max(seen.get(('depth', 'past a block'), 0), max(a[1::2][j] for j in range(len(a) // 2) if a[j * 2] == 61))
        if a != b:
            j = next((j for j, (u, v) in enumerate(zip(a, b)) if u != v), min(len(a), len(b)))
            print(f'orders {i}: at {j} exe {a[max(j - 6, 0):j + 6]} port {b[max(j - 6, 0):j + 6]}')
            bad += 1
            if bad >= 6:
                break
    lx = Lead()
    fo = (ctypes.c_float * 3)()
    wins = [0, 0, 0]
    for i in range(cases):
        ints, fl = lead_case(r)
        a = lx.lead(ints, fl)
        rr = lib.probe_pm_ai_lead((ctypes.c_int32 * 4)(*ints), (ctypes.c_float * len(fl))(*fl), fo) & 0xff
        b = [rr] + [fb(v) for v in fo]
        wins[0 if a[0] == 0 else 1 if a[1] == fb(fl[12]) else 2] += 1
        if a != b:
            print(f'lead {i} {ints} {fl}\n  exe  {[hex(v) for v in a]}\n  port {[hex(v) for v in b]}')
            bad += 1
            if bad >= 6:
                break
    print(f'lead: {cases} cases, none {wins[0]}, unmoved {wins[1]}, moved {wins[2]}: {bad} differ')
    u4 = (ctypes.c_uint32 * 4)()
    for i in range(cases):
        ints = [r.choice([0, 0x20]), r.choice([0, 1, 2]), r.choice([0, 1, 2]), r.randrange(2), int(r.random() < 0.85), r.randrange(4), r.choice([0, 0x10])]
        old = [r.choice([0, 5, 6]), r.choice([0, 5, 6]), r.choice([0, 5, 6]), r.choice([0, 5, 6])]
        a = lx.set_target(ints, old)
        lib.probe_pm_ai_settarget((ctypes.c_int32 * 7)(*ints), (ctypes.c_uint32 * 4)(*old), u4)
        if a != list(u4):
            print(f'target {i} {ints} {old}: exe {a} port {list(u4)}')
            bad += 1
            if bad >= 6:
                break
    nx = Net()
    for i in range(10):
        if (nx.z.r32(nx.slots[i]) if nx.slots[i] else 0) != lib.probe_pm_ai_slot_vt(i):
            print(f'slot {i}: exe {nx.z.r32(nx.slots[i]):x} port {lib.probe_pm_ai_slot_vt(i):x}')
            bad += 1
    lib.probe_pm_ai_kind.restype = ctypes.c_uint32
    for i in range(1000):
        which = r.randrange(-3, 28)
        live = bytes(r.randrange(2) for _ in range(10))
        a, b = nx.ask(which, live), lib.probe_pm_ai_kind(which, live)
        if a != b:
            print(f'kind {which - 16} {list(live)}: exe {a:x} port {b:x}')
            bad += 1
            if bad >= 6:
                break
    fires = reach_cases(nx, r, lib)
    if fires[0]:
        print(f'reaches: {fires[0]} differ')
        bad += fires[0]
    print(f'reaches: {fires[1]} yes, {fires[2]} no')
    lb, kinds = line_cases(nx, r, lib)
    print(f'weapon line: 3000 cases, {kinds} kinds: {lb} differ')
    bad += lb
    nx.z.call(0x4ba660)
    a = [nx.z.r32(0x60b688 + i * 4) for i in range(27)]
    b = (ctypes.c_uint32 * 27)()
    lib.probe_pm_firing_seed(b)
    if a != list(b):
        print('firing defaults:', [(hex(i * 4), hex(u), hex(v)) for i, (u, v) in enumerate(zip(a, b)) if u != v])
        bad += 1
    for tests in range(5):
        a = scan_exe(tests)
        n = lib.probe_pm_ai_scan(tests, big)
        if a != list(big[:n]):
            print(f'scan {tests}: exe {a} port {list(big[:n])}')
            bad += 1
    print(f'orders: {bad} differ, deepest {seen.get(("depth", "past a block"))}')
    print('calls:', ' '.join(f'{k}:{seen[k]}' for k in seen if isinstance(k, int)))
    pm_floors.floors(cases=(cases, 500), asleep=(seen.get(('asleep', 0), 0), 20),
                     mode=(seen.get(('mode', True), 0), 1), engaged=(seen.get(('ret', 1), 0), 20),
                     lead_none=(wins[0], 20), lead_unmoved=(wins[1], 20), lead_moved=(wins[2], 20), **{f'call_{k}': (seen.get(k, 0), 5) for k in [*range(1, 16), 17, 18, *range(20, 26), *range(30, 42), *range(50, 60), 62, 65, 66]}, depth=(seen.get(('depth', 'past a block'), 0), 32))
    return bad


if __name__ == '__main__':
    sys.exit(1 if main() else 0)
