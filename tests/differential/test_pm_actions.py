"""PM's action queue and its state machine against the exe: Queue 0x417290, ActionUpdate 0x417350 (with the action
objects' Reset 0x422040, IsStarted 0x422140), StateForAction 0x417170, SoundForAction 0x417220, DeathFall 0x41c4c0,
Stagger 0x41c520, GetUp 0x41c570 and KnockedDown 0x41c3f0 on the real queue (docs/movement-jump.md).

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_actions.py [runs]

A mech in emulator memory (a copy of the real vtable, the remote call switched for remote mechs, the network event
logged), its controls with the real interface vtable, legs that log their damage, the vital parts, the feet's shadow
nodes, and nine action objects with the real vtable except the root sample (+0x30), which both sides compute from the
action's time. The queue is a real MSVC list. The ray and dust of landf/landb (0x417804..) and the pose on the parts
(0x417a2b.., in test_pm_pose) are skipped in the exe and logged, as are sounds, cameras, weapons and the skis. Runs are
sequences of calls on one mech (falls, landings, lying down, getting up, deaths, remote mechs); after each call every
field both sides keep, the queue, the actions and the call log must match bit for bit at 24-bit precision.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

os.environ.setdefault('MW3_X87', '24')
sys.path.insert(0, str(Path(__file__).parent))
import test_pm_zar  # noqa: E402
from original import FTOL, IAT_FTOL  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBP, UC_X86_REG_EIP, UC_X86_REG_ESP  # noqa: E402

probe = test_pm_zar.load_pm_probe()
probe.probe_actions.argtypes = ([ctypes.c_int, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p] +
                                [ctypes.c_uint32] * 5 + [ctypes.c_int, ctypes.c_int, ctypes.c_float, ctypes.c_int,
                                                         ctypes.c_int] + [ctypes.c_void_p] * 5)

OPS = {0: 0x417350, 1: 0x417290, 2: 0x41c4c0, 3: 0x41c520, 4: 0x41c570, 5: 0x41c3f0}
MECH_VT, MECH_SIZE, CTL_VT, CTL, ACT_VT, ACT = 0x5dfaac, 0x800, 0x5bd428, 0x74, 0x5dfeec, 0xf0
LAST_ACTION = 0x629298
SKIPS = {0x417804: (21, 0x417904, 0), 0x417a2b: (17, 0x417d18, 12)}


def v3(off):
    return [(off + 4 * k, 'f') for k in range(3)]


# probe_actions.cpp's mech_io, controls_io, action_io
MECH = ([(0x8, 'b')] + v3(0x24) + v3(0x30) + v3(0x3c) + v3(0x58) + [(0xe0, 'b'), (0xe2, 'b')] + v3(0x498) +
        v3(0x4a4) + [(0x550, 'f')] + v3(0x554) + [(0x59c, 'i'), (0x5a0, 'i'), (0x5a8, 'b'), (0x5b4, 'f')] +
        v3(0x5b8) + v3(0x5c4) + [(0x624, 'i'), (0x628, 'i'), (0x65c, 'i'), (0x6ac, 'i')])
CTL_FIELDS = [(o, 'f') for o in (0x10, 0x14, 0x18, 0x1c, 0x20, 0x24, 0x28, 0x2c, 0x30, 0x44, 0x48, 0x4c)] + [(0x50, 'b')]
ACT_FIELDS = [(0x54, 'f'), (0x88, 'f'), (0xec, 'b'), (0xed, 'b')]


def f2u(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def u2f(x):
    return struct.unpack('<f', struct.pack('<I', x & 0xffffffff))[0]


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


def put(buf, off, kind, v):
    if kind == 'f':
        struct.pack_into('<f', buf, off, v)
    elif kind == 'b':
        buf[off] = int(v) & 0xff
    else:
        struct.pack_into('<I', buf, off, int(v) & 0xffffffff)


def pick(img, fields):
    out = bytearray(len(img))
    for off, kind in fields:
        n = 1 if kind == 'b' else 4
        out[off:off + n] = img[off:off + n]
    return bytes(out)


class Emulated(test_pm_zar.Emulated):
    def __init__(self):
        super().__init__()
        cpu = self.cpu
        cpu.import_code(IAT_FTOL, FTOL)
        a = self.alloc
        self.mech, self.mvt, self.ctl, self.vbt, self.pilot = a(0x900), a(0x200), a(CTL), a(8), a(0x40)
        self.legs, self.lvt, self.feet = [a(0xd4), a(0xd4)], a(0x60), [a(0xec), a(0xec)]
        self.head, self.torso, self.hip, self.weapons = a(0x80), a(0x1c0), a(0xb0), a(0x10)
        self.acts, self.avt, self.head_node, self.pos = [a(ACT) for _ in range(9)], a(0x50), a(12), a(12)
        self.player_obj, self.ff = a(0x40), a(0x40)
        self.wr(self.mvt, self.raw(MECH_VT, 0x200))
        net = cpu.code(b'\xc2\x08\x00')
        self.w32(self.mvt + 0x17c, net)
        self.native_stub(net, lambda c: self.log(22, c.arg(0), c.arg(1)), 8)
        dmg = cpu.code(b'\xc2\x08\x00')
        self.w32(self.lvt + 0x38, dmg)
        self.native_stub(dmg, lambda c: self.log(11, self.legs.index(c.ecx), c.arg(0), c.arg(1)), 8)
        self.wr(self.avt, self.raw(ACT_VT, 0x50))
        root = cpu.code(b'\xc2\x08\x00')
        self.w32(self.avt + 0x30, root)
        self.native_stub(root, self.root, 8)
        for addr, fn, pop in (
            (0x432d10, lambda c: self.handle(3, c.arg(0), c.arg(1), c.arg(4)), 0x14),
            (0x477f90, lambda c: self.log(15, c.arg(0)), 4),
            (0x4b4a50, lambda c: self.log(18, c.arg(0)), 4),
            (0x414370, lambda c: self.log(19), 0),
            (0x4143f0, lambda c: self.log(20), 0),
            (0x4fca50, lambda c: self.log(23, c.arg(0), c.arg(1) & 0xff), 0),
            (0x463840, lambda c: self.log(13), 4),
            (0x434310, lambda c: self.log(14, 0x433840), 0),
            (0x5cb260, lambda c: self.pos, 0),
            (0x5ce8c6, lambda c: self.alloc(c.arg(0)), 0),
            (0x5ce8c0, lambda c: 0, 0),
        ):
            self.native_stub(addr, fn, pop)
        for addr in SKIPS:
            cpu._uc.hook_add(UC_HOOK_CODE, self.skip, begin=addr, end=addr)
        self.w32(0x62c40c, self.player_obj)
        self.view, self.viewp = a(0x80), a(8)
        self.w32(0x61874c, self.view), self.w32(self.view + 0x58, self.viewp)
        self.iface = cpu.tr(CTL_VT, 4)
        self.dfn = a(0x1b0)   # the lean's two numbers, as probe_actions.cpp's
        self.wr(self.dfn + 0x154, struct.pack('<2f', 0.436, 0.5))
        self.native_stub(0x41f740, lambda c: self.dfn, 0)
        self.cpu.stubs[0x406cb0] = (lambda c: 0, 0)

    def raw(self, a, n):
        return bytes(self.cpu._uc.mem_read(a, n))

    def wr(self, a, b):
        self.cpu._uc.mem_write(a, bytes(b))

    def w32(self, a, v):
        self.wr(a, struct.pack('<I', v & 0xffffffff))

    def root(self, c):
        k = self.acts.index(c.ecx)
        t = u2f(self.r32(c.ecx + 0x88))
        self.wr(c.arg(0), struct.pack('<3f', f32(t * 1.5), f32(t * 0.25 + k), f32(t * -2.0)))
        self.wr(c.arg(1), struct.pack('<4f', 1.0, 0.0, f32(t * 0.5), 0.0))
        return 0

    def skip(self, uc, address, size, data):
        code, to, pop = SKIPS[address]
        if code == 17:
            ebp = uc.reg_read(UC_X86_REG_EBP)
            hip = struct.unpack('<3I', self.raw(ebp - 0x30, 12))
            q = struct.unpack('<4I', self.raw(ebp - 0x90, 16))
            self.log(17, *hip, *q)
        else:
            self.log(code)
        uc.reg_write(UC_X86_REG_ESP, uc.reg_read(UC_X86_REG_ESP) + pop)
        uc.reg_write(UC_X86_REG_EIP, to)

    def log(self, *e):
        self.events.append(len(e))
        self.events.extend(v & 0xffffffff for v in e)
        return 0

    def handle(self, *e):
        self.log(*e)
        h = self.next
        self.next += 1
        return h

    def set_queue(self, kinds):
        head = self.alloc(12)
        nodes = [self.alloc(12) for _ in kinds]
        ring = [head] + nodes
        for i, n in enumerate(ring):
            self.w32(n, ring[(i + 1) % len(ring)])
            self.w32(n + 4, ring[i - 1])
        for n, k in zip(nodes, kinds):
            self.w32(n + 8, k)
        self.w32(self.mech + 0x490, head)
        self.w32(self.mech + 0x494, len(kinds))

    def queue(self):
        u = self.r32
        head = u(self.mech + 0x490)
        out, n = [], u(head)
        while n != head:
            out.append(u(n + 8))
            n = u(n)
        if len(out) != u(self.mech + 0x494):
            raise SystemExit('the original queue count is off')
        return out

    def run(self, op, s):
        w, m = self.w32, self.mech
        self.events, self.next = [], 0x9000
        self.wr(m, s['mech'])
        w(m, self.mvt)
        w(self.mvt + 0x64, 0x402250 if s['flags'] & 4 else 0x406ce0)
        w(m + 0x38c, self.ctl)
        w(m + 0x6a8, self.pilot)
        w(self.pilot + 0xc, s['roster'])
        w(m + 0xd4, self.weapons)
        for i in range(2):
            w(m + 0x39c + 4 * i, self.legs[i])
            self.wr(self.legs[i], bytes(0xd4))
            w(self.legs[i], self.lvt)
            self.wr(self.legs[i] + 0x48, bytes([s['destroyed'] >> (1 + i) & 1]))
            w(self.legs[i] + 0x78, self.feet[i])
            w(self.feet[i] + 0x78, s['shadow'][i])
        w(m + 0x3b4, self.head)
        self.wr(self.head + 0x48, bytes([s['destroyed'] & 1]))
        w(m + 0x3ac, self.torso)
        self.wr(self.torso + 0x48, bytes([s['destroyed'] >> 3 & 1]))
        w(m + 0x3b0, self.hip)
        for k in range(9):
            self.wr(self.acts[k], s['acts'][ACT * k:ACT * (k + 1)])
            w(self.acts[k], self.avt)
            w(m + 0x468 + 4 * k, self.acts[k] if s['present'] >> k & 1 else 0)
        self.set_queue(s['queue'])
        self.wr(self.ctl, s['ctl'])
        w(self.ctl, 0x5e009c)
        w(self.ctl + 4, self.vbt)
        w(self.vbt + 4, 0x5c)
        w(self.ctl + 0x5c, 0)
        w(self.ctl + 0x60, self.iface)
        w(self.ctl + 0x70, m)
        w(self.player_obj + 0x14, m if s['flags'] & 1 else 0)
        w(0x62c410, self.ff if s['flags'] & 2 else 0)
        w(LAST_ACTION, s['last'])
        args = {0: (f2u(s['dt']),), 1: (s['arg'], s['arg2']), 2: (s['arg'],), 3: (m + 0x554,), 4: (),
                5: (s['arg'],)}[op]
        r = self.call(OPS[op], ecx=m, args=args)
        r = int(r & 0xff != 0) if op in (1, 2, 5) else 0  # KnockedDown(5) returns 5
        acts = b''.join(pick(self.raw(a, ACT), ACT_FIELDS) for a in self.acts)
        return (pick(self.raw(m, MECH_SIZE), MECH), pick(self.raw(self.ctl, CTL), CTL_FIELDS), acts,
                self.queue(), struct.unpack('<i', self.raw(LAST_ACTION, 4))[0], r, self.events)

    def state(self):
        return self.raw(self.mech, MECH_SIZE), self.raw(self.ctl, CTL), b''.join(self.raw(a, ACT) for a in self.acts)


def port(op, s):
    mech, ctl = ctypes.create_string_buffer(s['mech'], MECH_SIZE), ctypes.create_string_buffer(s['ctl'], CTL)
    acts = ctypes.create_string_buffer(s['acts'], 9 * ACT)
    q = (ctypes.c_int32 * 64)(*s['queue'])
    nq, out, n, last = ctypes.c_int32(len(s['queue'])), (ctypes.c_int32 * 4096)(), ctypes.c_int32(4096), ctypes.c_int32()
    r = probe.probe_actions(op, mech, ctl, acts, s['present'], s['shadow'][0], s['shadow'][1], s['destroyed'],
                            s['flags'], s['roster'], s['last'], s['dt'], s['arg'], s['arg2'], q, ctypes.byref(nq),
                            out, ctypes.byref(n), ctypes.byref(last))
    return (pick(mech.raw, MECH), pick(ctl.raw, CTL_FIELDS),
            b''.join(pick(acts.raw[ACT * k:ACT * (k + 1)], ACT_FIELDS) for k in range(9)),
            list(q[:nq.value]), last.value, r & 0xff, [v & 0xffffffff for v in out[:n.value]])


def fresh(rng):
    mech = bytearray(MECH_SIZE)
    mech[0x8] = rng.random() > 0.05
    for off, kind in MECH:
        if kind == 'f':
            put(mech, off, 'f', f32(rng.uniform(-10, 10)))
    put(mech, 0x24, 'f', 0.0), put(mech, 0x2c, 'f', -1.0)
    mech[0xe0], mech[0xe2] = rng.random() < 0.05, rng.random() < 0.05
    put(mech, 0x59c, 'i', rng.choice([1, 1, 3, 5, 7, 9, 0, 11, 12, 13, 10]))
    mech[0x5a8] = 0
    put(mech, 0x5b4, 'f', f32(rng.choice([5.0, 0.0, -0.1])))
    put(mech, 0x6ac, 'i', rng.choice([0xffffffff, 0xffffffff, 0, 0x1c]))
    ctl = bytearray(CTL)
    for off, kind in CTL_FIELDS:
        put(ctl, off, kind, f32(rng.uniform(-1, 1)) if kind == 'f' else rng.randrange(2))
    acts = bytearray(9 * ACT)
    for k in range(9):
        put(acts, ACT * k + 0x54, 'f', f32(rng.choice([0.5, 1.0, 1.4, 2.0])))
        put(acts, ACT * k + 0x88, 'f', 0.0)
    return {'mech': bytes(mech), 'ctl': bytes(ctl), 'acts': bytes(acts),
            'present': rng.choice([0x1fe, 0x1fe, 0x1fe, 0x07e, 0x1e6, 0x01e]), 'shadow': (rng.choice([0, 0x7070]), 0),
            'destroyed': rng.choice([0, 0, 0, 0, 1, 2, 8]), 'flags': rng.randrange(8) & (7 if rng.random() < 0.15 else 3),
            'roster': rng.randrange(25), 'last': rng.choice([0, 1, 2, 3, 4, 5, 6]), 'queue': []}


def main():
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 1500
    emu = Emulated()
    rng = random.Random(1999)
    calls = 0
    states, codes = set(), set()
    for run in range(runs):
        s = fresh(rng)
        for step in range(rng.choice([20, 60, 150])):
            r = rng.random()
            if not s['queue'] or r < 0.15:
                op = rng.choice([1, 2, 5, 5, 4, 3])
            else:
                op = 0
            s['dt'] = f32(rng.choice([1 / 30, 1 / 60, 0.1, 0.25]))
            s['arg'], s['arg2'] = rng.choice([1, 2, 3, 4, 5, 6, 7, 8, 0, 3, 4]), rng.randrange(2)
            if op == 2:
                s['arg'] = rng.choice([1, 2, 7, 8])
            if op == 5:
                s['arg'] = rng.choice([3, 4, 3, 4, 5])
            want, got = emu.run(op, s), port(op, s)
            names = ('mech', 'controls', 'actions', 'queue', 'last action', 'result', 'calls')
            for k, name in enumerate(names):
                if want[k] != got[k]:
                    if k < 3:
                        diff = [hex(o) for o in range(0, len(want[k]), 4) if want[k][o:o + 4] != got[k][o:o + 4]]
                        raise SystemExit(f'run {run} step {step} op {op}: {name} differ at {diff}')
                    raise SystemExit(f'run {run} step {step} op {op}: {name} differ\n  original {want[k]}\n'
                                     f'  port     {got[k]}')
            calls += 1
            mech, ctl, acts = emu.state()
            s.update(mech=mech, ctl=ctl, acts=acts, queue=want[3], last=want[4])
            states.add(struct.unpack_from('<i', mech, 0x59c)[0])
            log = want[6]
            i = 0
            while i < len(log):
                codes.add(log[i + 1])
                i += log[i] + 1
            if rng.random() < 0.02:   # lying time runs out (Mech::Update's countdown)
                s['mech'] = s['mech'][:0x5b4] + struct.pack('<f', -0.1) + s['mech'][0x5b8:]
    need_states = {1, 10, 11, 12, 13}
    need_codes = {3, 11, 13, 15, 17, 18, 19, 20, 21, 22}
    if not need_states <= states or not need_codes <= codes:
        raise SystemExit(f'coverage: states {sorted(states)}, calls {sorted(codes)}')
    print(f'ok: {runs} runs, {calls} calls identical bit for bit; states {sorted(states)}, '
          f'{len(codes)} kinds of call out')


if __name__ == '__main__':
    main()
