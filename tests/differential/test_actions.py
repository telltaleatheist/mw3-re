"""The action queue and its state machine against the original x86: Queue
(0x416780), ActionUpdate (0x416860, with the action objects' Reset 0x420f40,
Advance 0x420f80, IsStarted 0x421000), StateForAction (0x416660),
SoundForAction (0x416720), DeathFall (0x41b760), Stagger (0x41b7f0), GetUp
(0x41b840) and KnockedDown (0x41b660) now on the real queue
(docs/movement-jump.md).

    cmake --build build && .venv/bin/python tests/differential/test_actions.py [runs]

A mech in emulator memory (a copy of the real vtable, +0x5c switched for
remote mechs, +0x168 the logged network event), its controls with the real
interface vtable, legs that log their damage, the vital parts, the feet's
shadow nodes, and nine action objects with the real vtable except the root
sample (+0x30), which both sides compute from the action's time. The queue
is a real MSVC list. The ray and dust of landf/landb (0x416d3a..) and the
pose on the parts (0x416fb8..) are skipped in the original and logged, as
are sounds, cameras, weapons and the skis. Runs are sequences of calls on one
mech (falls, landings, lying down, getting up, deaths, remote mechs); after
each call every field both sides keep, the queue, the actions and the call
log must match bit for bit at 24-bit precision.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

os.environ.setdefault('MW3_X87', '24')
sys.path.insert(0, str(Path(__file__).parent))
from original import FTOL, IAT_FTOL, ROOT, Original  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBP, UC_X86_REG_EIP, UC_X86_REG_ESP  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
probe.probe_actions.argtypes = ([ctypes.c_int, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p] +
                                [ctypes.c_uint32] * 5 + [ctypes.c_int, ctypes.c_int, ctypes.c_float, ctypes.c_int,
                                                         ctypes.c_int] + [ctypes.c_void_p] * 5)

OPS = {0: 0x416860, 1: 0x416780, 2: 0x41b760, 3: 0x41b7f0, 4: 0x41b840, 5: 0x41b660}
MECH_VT, MECH_SIZE, CTL_VT, CTL, ACT_VT, ACT = 0x5bcef8, 0x6d8, 0x5bd428, 0x74, 0x5bd290, 0xf0
IAT_NEW, IAT_DELETE, LAST_ACTION = 0x5bb4bc, 0x5bb4c8, 0x60550c
SKIPS = {0x416d3a: (21, 0x416e91, 0), 0x416fb8: (17, 0x417293, 12)}


def v3(off):
    return [(off + 4 * k, 'f') for k in range(3)]


# probe_actions.cpp's mech_io, controls_io, action_io
MECH = ([(0x8, 'b')] + v3(0x24) + v3(0x30) + v3(0x3c) + v3(0x58) + [(0xdc, 'b'), (0xde, 'b')] + v3(0x488) +
        v3(0x494) + [(0x53c, 'f')] + v3(0x540) + [(0x578, 'i'), (0x57c, 'i'), (0x584, 'b'), (0x590, 'f')] +
        v3(0x594) + v3(0x5a0) + [(0x600, 'i'), (0x604, 'i'), (0x638, 'i'), (0x688, 'i')])
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


class Emulated:
    def __init__(self):
        cpu = self.cpu = Original()
        cpu.import_code(IAT_FTOL, FTOL)
        a = cpu.alloc
        self.mech, self.mvt, self.ctl, self.vbt, self.pilot = a(0x700), a(0x200), a(CTL), a(8), a(0x40)
        self.legs, self.lvt, self.feet = [a(0xd4), a(0xd4)], a(0x60), [a(0xec), a(0xec)]
        self.head, self.torso, self.hip, self.weapons = a(0x80), a(0x1b4), a(0xb0), a(0x10)
        self.acts, self.avt, self.head_node, self.pos = [a(ACT) for _ in range(9)], a(0x50), a(12), a(12)
        self.player_obj, self.ff = a(0x40), a(0x40)
        cpu.write(self.mvt, cpu.read(MECH_VT, 0x200))
        net = cpu.code(b'\xc2\x08\x00')
        cpu.u32(self.mvt + 0x168, net)
        cpu.stub_function(net, lambda c: self.log(22, c.arg(0), c.arg(1)), 8)
        dmg = cpu.code(b'\xc2\x08\x00')
        cpu.u32(self.lvt + 0x38, dmg)
        cpu.stub_function(dmg, lambda c: self.log(11, self.legs.index(c.ecx), c.arg(0), c.arg(1)), 8)
        cpu.write(self.avt, cpu.read(ACT_VT, 0x50))
        root = cpu.code(b'\xc2\x08\x00')
        cpu.u32(self.avt + 0x30, root)
        cpu.stub_function(root, self.root, 8)
        stubs = {
            0x432990: (lambda c: self.handle(3, c.arg(0), c.arg(1), c.arg(4)), 0x14),
            0x4765b0: (lambda c: self.log(15, c.arg(0)), 4),
            0x4b0bf0: (lambda c: self.log(18, c.arg(0)), 4),
            0x413af0: (lambda c: self.log(19), 0),
            0x413b70: (lambda c: self.log(20), 0),
            0x4eb170: (lambda c: self.log(23, c.ecx, c.edx & 0xff), 0),
            0x4621f0: (lambda c: self.log(13), 4),
            0x433840: (lambda c: self.log(14, 0x433840), 0),
            0x5a85e0: (lambda c: self.pos, 0),
        }
        for addr, (fn, pop) in stubs.items():
            cpu.stub_function(addr, fn, pop)
        cpu.stub_import(IAT_NEW, lambda c: c.alloc(c.arg(0)))
        cpu.stub_import(IAT_DELETE, lambda c: 0)
        for addr in SKIPS:
            cpu.uc.hook_add(UC_HOOK_CODE, self.skip, begin=addr, end=addr)
        cpu.u32(0x60861c, self.player_obj)

    def root(self, c):
        k = self.acts.index(c.ecx)
        t = u2f(struct.unpack('<I', c.read(c.ecx + 0x88, 4))[0])
        c.write(c.arg(0), struct.pack('<3f', f32(t * 1.5), f32(t * 0.25 + k), f32(t * -2.0)))
        c.write(c.arg(1), struct.pack('<4f', 1.0, 0.0, f32(t * 0.5), 0.0))
        return 0

    def skip(self, uc, address, size, data):
        code, to, pop = SKIPS[address]
        if code == 17:
            ebp = uc.reg_read(UC_X86_REG_EBP)
            hip = struct.unpack('<3I', self.cpu.read(ebp - 0x30, 12))
            q = struct.unpack('<4I', self.cpu.read(ebp - 0x90, 16))
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
        cpu = self.cpu
        head = cpu.alloc(12)
        nodes = [cpu.alloc(12) for _ in kinds]
        ring = [head] + nodes
        for i, n in enumerate(ring):
            cpu.u32(n, ring[(i + 1) % len(ring)])
            cpu.u32(n + 4, ring[i - 1])
        for n, k in zip(nodes, kinds):
            cpu.u32(n + 8, k)
        cpu.u32(self.mech + 0x480, head)
        cpu.u32(self.mech + 0x484, len(kinds))

    def queue(self):
        u = lambda a: struct.unpack('<I', self.cpu.read(a, 4))[0]
        head = u(self.mech + 0x480)
        out, n = [], u(head)
        while n != head:
            out.append(u(n + 8))
            n = u(n)
        if len(out) != u(self.mech + 0x484):
            raise SystemExit('the original queue count is off')
        return out

    def run(self, op, s):
        cpu = self.cpu
        self.events, self.next = [], 0x9000
        cpu.write(self.mech, s['mech'])
        cpu.u32(self.mech, self.mvt)
        cpu.u32(self.mvt + 0x5c, 0x4d2a70 if s['flags'] & 4 else 0x40bae0)
        cpu.u32(self.mech + 0x37c, self.ctl)
        cpu.u32(self.mech + 0x684, self.pilot)
        cpu.u32(self.pilot + 0xc, s['roster'])
        cpu.u32(self.mech + 0xd4, self.weapons)
        for i in range(2):
            cpu.u32(self.mech + 0x38c + 4 * i, self.legs[i])
            cpu.write(self.legs[i], bytes(0xd4))
            cpu.u32(self.legs[i], self.lvt)
            cpu.write(self.legs[i] + 0x48, bytes([s['destroyed'] >> (1 + i) & 1]))
            cpu.u32(self.legs[i] + 0x78, self.feet[i])
            cpu.u32(self.feet[i] + 0x78, s['shadow'][i])
        cpu.u32(self.mech + 0x3a4, self.head)
        cpu.write(self.head + 0x48, bytes([s['destroyed'] & 1]))
        cpu.u32(self.mech + 0x39c, self.torso)
        cpu.write(self.torso + 0x48, bytes([s['destroyed'] >> 3 & 1]))
        cpu.u32(self.mech + 0x3a0, self.hip)
        for k in range(9):
            cpu.write(self.acts[k], s['acts'][ACT * k:ACT * (k + 1)])
            cpu.u32(self.acts[k], self.avt)
            cpu.u32(self.mech + 0x458 + 4 * k, self.acts[k] if s['present'] >> k & 1 else 0)
        self.set_queue(s['queue'])
        cpu.write(self.ctl, s['ctl'])
        cpu.u32(self.ctl + 4, self.vbt)
        cpu.u32(self.vbt + 4, 0x5c)
        cpu.u32(self.ctl + 0x5c, 0)
        cpu.u32(self.ctl + 0x60, CTL_VT)
        cpu.u32(self.ctl + 0x70, self.mech)
        cpu.u32(self.player_obj + 0x14, self.mech if s['flags'] & 1 else 0)
        cpu.u32(0x608620, self.ff if s['flags'] & 2 else 0)
        cpu.u32(LAST_ACTION, s['last'])
        args = {0: (f2u(s['dt']),), 1: (s['arg'], s['arg2']), 2: (s['arg'],), 3: (self.mech + 0x540,), 4: (),
                5: (s['arg'],)}[op]
        cpu.call(OPS[op], ecx=self.mech, stack_args=args)
        r = int(cpu.uc.reg_read(UC_X86_REG_EAX) & 0xff != 0) if op in (1, 2, 5) else 0  # KnockedDown(5) returns 5
        acts = b''.join(pick(cpu.read(a, ACT), ACT_FIELDS) for a in self.acts)
        return (pick(cpu.read(self.mech, MECH_SIZE), MECH), pick(cpu.read(self.ctl, CTL), CTL_FIELDS), acts,
                self.queue(), struct.unpack('<i', cpu.read(LAST_ACTION, 4))[0], r, self.events)

    def state(self):
        cpu = self.cpu
        return cpu.read(self.mech, MECH_SIZE), cpu.read(self.ctl, CTL), b''.join(cpu.read(a, ACT) for a in self.acts)


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
    mech[0xdc], mech[0xde] = rng.random() < 0.05, rng.random() < 0.05
    put(mech, 0x578, 'i', rng.choice([1, 1, 3, 5, 7, 9, 0, 11, 12, 13, 10]))
    mech[0x584] = 0
    put(mech, 0x590, 'f', f32(rng.choice([5.0, 0.0, -0.1])))
    put(mech, 0x688, 'i', rng.choice([0xffffffff, 0xffffffff, 0, 0x1c]))
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
            'roster': rng.randrange(19), 'last': rng.choice([0, 1, 2, 3, 4, 5, 6]), 'queue': []}


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
            states.add(struct.unpack_from('<i', mech, 0x578)[0])
            log = want[6]
            i = 0
            while i < len(log):
                codes.add(log[i + 1])
                i += log[i] + 1
            if rng.random() < 0.02:   # lying time runs out (Mech::Update's countdown)
                s['mech'] = s['mech'][:0x590] + struct.pack('<f', -0.1) + s['mech'][0x594:]
    need_states = {1, 10, 11, 12, 13}
    need_codes = {3, 11, 13, 15, 17, 18, 19, 20, 21, 22}
    if not need_states <= states or not need_codes <= codes:
        raise SystemExit(f'coverage: states {sorted(states)}, calls {sorted(codes)}')
    print(f'ok: {runs} runs, {calls} calls identical bit for bit; states {sorted(states)}, '
          f'{len(codes)} kinds of call out')


if __name__ == '__main__':
    main()
