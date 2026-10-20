"""Mech::Update (0x419fc0), the mech's frame, against the original x86
(docs/movement.md, The frame): the entity budget (0x5a8910, 0x5a8960), the
death stop, the view key, the yaw decay, the lying countdown, the order of the
gait, actions, turn, torso inputs (0x466200, 0x466240), part updates and
collision, the lean (0x525210 with the definition's lean_angle and
lean_factor), the orientation step (0x525900, 0x5255d0, 0x525500), the world
velocity and push, the display matrix (0x525740), the push decay or the
throttle stop, the hit timeout (vtable +0x14c, 0x41d190), the movement
sounds, the controls' unwrap and the knock decay.

    cmake --build build && .venv/bin/python tests/differential/test_update.py [trials]

A mech in emulator memory with a copy of the real vtable, controls with the
real IControls vtable (its Update stubbed), a torso for the real input
setters. Every other call (the gait, actions, turn, parts, collision, node
move, weapons, heat, sounds, HUD) is a logging stub on both sides. Each trial
is one call; the fields Update reads and writes, the display matrix, the
budget and the call log must match bit for bit at 24-bit precision.
"""
import ctypes
import math
import os
import random
import struct
import sys
from pathlib import Path

os.environ.setdefault('MW3_X87', '24')
sys.path.insert(0, str(Path(__file__).parent))
from original import FTOL, IAT_FTOL, ROOT, Original  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
P, F, I, U = ctypes.c_void_p, ctypes.c_float, ctypes.c_int32, ctypes.c_uint32
probe.probe_update.argtypes = [P, P, P, U, P, U, I, F, F, F, I, F, F, F, F, U, I, I, U, P, F, P, P, P]

UPDATE, MECH_VT, CTL_VT, CTL, MECH_SIZE, TORSO = 0x419fc0, 0x5bcef8, 0x5bd428, 0x74, 0x6d8, 0x1b4
BUDGET, UPDATING = 0x736558, 0x73657c
TIME, FLOOR, DEAD = 0x60991c, 0x605630, 0x6099c0

MECH = ([(0x4, 'i'), (0x8, 'b')] + [(o, 'f') for o in range(0x24, 0x64, 4)] + [(0x68, 'f')] +
        [(0xdc, 'b'), (0xdd, 'b'), (0xde, 'b'), (0xe0, 'f'), (0xe4, 'i')] +
        [(0x374 + k, 'b') for k in range(4)] +
        [(0x414, 'f'), (0x528, 'f'), (0x53c, 'f'), (0x540, 'f'), (0x544, 'f'), (0x548, 'f'), (0x54c, 'f'),
         (0x570, 'f'), (0x578, 'i'), (0x57c, 'i'), (0x580, 'i'), (0x584, 'b'), (0x590, 'f')] +
        [(o, 'f') for o in range(0x594, 0x5ac, 4)] + [(0x5ac, 'i'), (0x5b0, 'b')] +
        [(0x5b8 + 4 * k, 'i') for k in range(25)] + [(0x688, 'i'), (0x68c, 'i'), (0x6b0, 'f')])
CTL_FIELDS = ([(0x8, 'i'), (0xc, 'i')] + [(o, 'f') for o in (0x10, 0x14, 0x18, 0x1c, 0x2c, 0x30)] +
              [(0x34, 'b'), (0x38, 'i'), (0x44, 'f'), (0x48, 'f'), (0x50, 'b'), (0x58, 'b'), (0x59, 'b')])
TORSO_FIELDS = [(0x9c, 'i'), (0xbc, 'f'), (0xc0, 'f'), (0xc4, 'f'), (0xc8, 'f')]


def f2u(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


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
        self.mech, self.mvt, self.ctl, self.vbt, self.pvt, self.ivt = a(0x700), a(0x200), a(CTL), a(8), a(0x10), a(0x60)
        self.pilot, self.skis, self.torso, self.transform, self.pos = a(0x40), a(0x300), a(TORSO), a(48), a(12)
        self.parts = [a(0x100) for _ in range(5)]   # hip, legs, arms
        self.pvts = [a(0x20) for _ in range(6)]     # torso, hip, legs, arms
        self.dfn, self.view, self.viewp, self.player_obj = a(0x1a8), a(0x80), a(8), a(0x20)
        self.log_ = []

        def vstub(slot_addr, fn, pop):
            f = cpu.code(b'\xc3')
            cpu.u32(slot_addr, f)
            cpu.stub_function(f, fn, pop)
        cpu.write(self.mvt, cpu.read(MECH_VT, 0x200))
        vstub(self.mvt + 0x158, lambda c: self.log(38, c.arg(0)), 4)
        cpu.write(self.ivt, cpu.read(CTL_VT, 0x60))
        vstub(self.ivt + 0xc, lambda c: self.log(11, c.arg(0)) or self.go, 4)
        vstub(self.pvt + 4, lambda c: self.log(36) or self.ctl, 0)
        for k, code in enumerate((23, 24, 25, 26, 27, 28)):
            vstub(self.pvts[k] + 0x10, lambda c, code=code: self.log(code, c.arg(0)), 4)
        stubs = {
            0x41cac0: (lambda c: self.log(6), 0),
            0x56e9f0: (lambda c: self.log(1, c.ecx), 0),
            0x41e7a0: (lambda c: self.log(7, c.ecx), 0),
            0x41e350: (lambda c: self.log(8), 4),
            0x51d300: (lambda c: self.log(9, c.ecx) or self.key, 0),
            0x426780: (lambda c: self.log(10, c.arg(0)), 4),
            0x41ce60: (lambda c: self.log(12, c.arg(0)), 4),
            0x416860: (lambda c: self.log(13, c.arg(0)), 4),
            0x4181c0: (lambda c: self.log(14, c.arg(0)), 4),
            0x41b840: (lambda c: self.log(15), 0),
            0x418fc0: (lambda c: self.log(16, c.arg(0)), 4),
            0x4225c0: (self.collide, 0),
            0x41e9a0: (lambda c: self.dfn, 0),
            0x4fc1b0: (lambda c: self.log(18, c.ecx, c.arg(0), c.arg(1), c.arg(2)), 0xc),
            0x5a85c0: (lambda c: self.log(19), 0),
            0x413560: (lambda c: self.log(20, struct.unpack('<I', c.read(c.arg(0), 4))[0]), 4),
            0x4b2da0: (lambda c: self.log(21) or self.weapons, 0),
            0x430aa0: (lambda c: self.log(40) or self.screen, 0),
            0x446240: (lambda c: self.log(22, c.ecx - 0x6520), 0),
            0x4b0c20: (lambda c: self.log(30, c.arg(0) & 0xff), 4),
            0x4b0c30: (lambda c: self.log(31, c.arg(0)), 4),
            0x419200: (lambda c: self.log(32, c.arg(0)), 4),
            0x4192f0: (lambda c: self.log(33, c.arg(0)), 4),
            0x56f370: (lambda c: self.log(34, c.ecx, c.arg(0)), 4),
            0x571f30: (lambda c: self.log(35, c.ecx), 8),
            0x432990: (lambda c: self.handle(3, c.arg(0), c.arg(1), c.arg(4)), 0x14),
            0x571d20: (lambda c: self.log(4, c.ecx), 4),
            0x4e6480: (lambda c: self.log(37, c.ecx, c.arg(0)), 4),
            0x413cb0: (lambda c: self.log(39, c.arg(0)), 4),
            0x41d130: (lambda c: self.log(5), 0),
            0x5a85e0: (lambda c: self.pos, 0),
        }
        for addr, (fn, pop) in stubs.items():
            cpu.stub_function(addr, fn, pop)
        cpu.u32(0x5f4c8c, self.view)
        cpu.u32(0x60861c, self.player_obj)
        cpu.write(self.pos, struct.pack('<3f', 1.5, 2.5, 3.5))

    def collide(self, c):
        self.log(17)
        c.write(self.mech + 0x374, bytes(self.blocked))
        return self.held

    def log(self, *e):
        self.events.append(len(e))
        self.events.extend(v & 0xffffffff for v in e)
        return 0

    def handle(self, *e):
        self.log(*e)
        h = self.next
        self.next += 1
        return h

    def run(self, s):
        cpu = self.cpu
        self.events, self.next = [], 0x9000
        self.key, self.go, self.held, self.blocked = s['key'], s['go'], s['held'], s['blocked']
        self.weapons, self.screen = s['flags'] >> 4 & 1, 0x7000 if s['flags'] & 32 else 0
        cpu.write(self.mech, s['mech'])
        cpu.u32(self.mech, self.mvt)
        cpu.u32(self.mvt + 0x5c, 0x4d2a70 if s['flags'] & 4 else 0x40bae0)
        cpu.u32(self.mech + 0xc, self.transform)
        cpu.write(self.transform, s['transform'])
        cpu.u32(self.mech + 0x37c, self.ctl)
        cpu.u32(self.mech + 0x684, self.pilot)
        cpu.u32(self.pilot + 0xc, s['roster'])
        cpu.u32(self.mech + 0x41c, self.skis)
        cpu.write(self.skis + 5, bytes([s['slid']]))
        cpu.write(self.skis + 0x160, struct.pack('<f', s['surf1']))
        cpu.write(self.skis + 0x16c, struct.pack('<f', s['surf4']))
        cpu.u32(self.mech + 0x484, s['actions'])
        cpu.u32(self.mech + 0x39c, self.torso)
        cpu.write(self.torso, s['torso'])
        cpu.u32(self.torso, self.pvts[0])
        cpu.u32(self.mech + 0x3a0, self.parts[0])
        cpu.u32(self.parts[0], self.pvts[1])
        for k in range(4):
            cpu.u32(self.mech + 0x38c + 4 * k, self.parts[1 + k])
            cpu.u32(self.parts[1 + k], self.pvts[2 + k])
        cpu.write(self.ctl, s['ctl'])
        cpu.u32(self.ctl, self.pvt)
        cpu.u32(self.ctl + 4, self.vbt)
        cpu.u32(self.vbt + 4, 0x5c)
        cpu.u32(self.ctl + 0x5c, 0)
        cpu.u32(self.ctl + 0x60, self.ivt)
        cpu.u32(self.ctl + 0x70, self.mech)
        cpu.write(self.dfn + 0x154, struct.pack('<2f', s['lean_angle'], s['lean_factor']))
        cpu.u32(self.view + 0x64, self.mech if s['flags'] & 1 else 0)
        cpu.u32(self.view + 0x58, self.viewp)
        cpu.u32(self.viewp + 4, 1 if s['flags'] & 8 else 0)
        cpu.u32(self.player_obj + 0x14, self.mech if s['flags'] & 2 else 0)
        cpu.write(TIME, struct.pack('<f', s['time']))
        cpu.write(FLOOR, struct.pack('<f', s['floor']))
        cpu.u32(DEAD, s['dead'])
        b = s['budget']
        cpu.u32(UPDATING, b[0])
        for k in range(4):
            cpu.u32(BUDGET + 0x10 + 4 * k, b[1 + k])
        cpu.call(UPDATE, ecx=self.mech, stack_args=(f2u(s['dt']),))
        budget = [struct.unpack('<I', cpu.read(UPDATING, 4))[0]] + \
            list(struct.unpack('<4I', cpu.read(BUDGET + 0x10, 16)))
        return (pick(cpu.read(self.mech, MECH_SIZE), MECH), pick(cpu.read(self.ctl, CTL), CTL_FIELDS),
                pick(cpu.read(self.torso, TORSO), TORSO_FIELDS), cpu.read(self.transform, 48),
                budget + [struct.unpack('<I', cpu.read(DEAD, 4))[0]], self.events)


def port(s, torso_addr):
    mech = ctypes.create_string_buffer(s['mech'], MECH_SIZE)
    ctl = ctypes.create_string_buffer(s['ctl'], CTL)
    torso = ctypes.create_string_buffer(s['torso'], TORSO)
    tr = ctypes.create_string_buffer(s['transform'], 48)
    b = (ctypes.c_int32 * 5)(*[struct.unpack('<i', struct.pack('<I', v))[0] for v in s['budget']])
    dead = ctypes.c_int32(s['dead'])
    out, n = (ctypes.c_int32 * 4096)(), ctypes.c_int32(4096)
    probe.probe_update(mech, ctl, torso, torso_addr, b, s['flags'], s['actions'], float(s['slid']), s['surf1'],
                       s['surf4'], s['roster'], s['lean_angle'], s['lean_factor'], s['time'], s['floor'], s['key'],
                       s['go'], s['held'], struct.unpack('<I', bytes(s['blocked']))[0], tr, s['dt'],
                       ctypes.byref(dead), out, ctypes.byref(n))
    budget = [v & 0xffffffff for v in b] + [dead.value & 0xffffffff]
    return (pick(mech.raw, MECH), pick(ctl.raw, CTL_FIELDS), pick(torso.raw, TORSO_FIELDS), tr.raw[:48], budget,
            [v & 0xffffffff for v in out[:n.value]])


def unit_quat(rng):
    a = rng.uniform(-math.pi, math.pi)
    q = [math.cos(a / 2), 0.0, math.sin(a / 2), 0.0]
    if rng.random() < 0.2:   # drifted off unit and off pure yaw, as a long game leaves it
        q = [v + rng.uniform(-0.02, 0.02) for v in q]
    return [f32(v) for v in q]


def scenario(rng):
    mech = bytearray(MECH_SIZE)
    v = {0x4: 0x7700, 0x8: 1, 0x68: rng.choice([8.0, 12.0, 15.5]),
         0xdc: rng.random() < 0.05, 0xdd: 0, 0xde: rng.random() < 0.08,
         0xe0: rng.choice([0.0, 99.0, 100.0, 100.5, 101.5]), 0xe4: rng.choice([0, 0x1234]),
         0x414: rng.uniform(0, 100), 0x528: rng.choice([1.0, 0.6, rng.uniform(0.3, 1)]),
         0x53c: rng.choice([0.0, 0.0, 0.1, 0.25, 3.0, 12.0]),
         0x54c: rng.choice([0.0, 0.0, 0.005, -0.02, rng.uniform(-0.3, 0.3)]),
         0x570: rng.choice([-1.0, 0.0, 5.0, 29.9]),
         0x578: rng.choice([1, 3, 5, 7, 9, 10, 10, 11, 12, 13, 14]), 0x57c: rng.randrange(13),
         0x580: rng.random() < 0.3, 0x584: rng.random() < 0.3, 0x590: rng.choice([5.0, 0.01, -0.5, 2.0]),
         0x5ac: rng.choice([-1, -1, 0, 1, 2]), 0x5b0: rng.random() < 0.8,
         0x688: rng.choice([0xffffffff, 0xffffffff, 0x1d, 0x1c, 0x10, 0, 0x21, 0x20]),
         0x6b0: rng.choice([0.0, 0.0, 0.0333, 0.1])}
    for off in range(0x24, 0x48, 4):
        v[off] = rng.uniform(-30, 30) if rng.random() < 0.8 else 0.0
    for k, q in enumerate(unit_quat(rng)):
        v[0x48 + 4 * k] = q
    v[0x58], v[0x60] = rng.uniform(-1, 1), rng.uniform(-1, 1)
    v[0x5c] = rng.choice([0.0, 0.01, -0.03, 0.06, rng.uniform(-1.5, 1.5)])
    for off in (0x540, 0x544, 0x548):
        v[off] = rng.uniform(-5, 5)
    for off in range(0x594, 0x5ac, 4):
        v[off] = rng.choice([0.0, 0.0, 0.005, rng.uniform(-8, 8)])
    for k in range(25):
        v[0x5b8 + 4 * k] = rng.choice([0, 0, 0, 0x5000 + k])
    for off, kind in MECH:
        val = v.get(off, 0)
        put(mech, off, kind, f32(val) if kind == 'f' else val)
    ctl = bytearray(CTL)
    cv = {0x8: rng.choice([0, 4]), 0xc: rng.choice([0, 1, 2, 3]),
          0x10: rng.choice([0.0, 0.5, -0.4, 1.0, rng.uniform(-1, 1)]),
          0x14: rng.choice([0.0, 0.0, 1.0, -1.0, rng.uniform(-1, 1)]),
          0x18: rng.choice([0.0, 0.0, 1.0, -0.5]), 0x1c: rng.choice([0.0, 0.0, 1.5, -1.5]),
          0x2c: rng.choice([0.0, 0.3]), 0x30: rng.choice([0.0, -0.3]), 0x34: rng.randrange(3),
          0x38: rng.random() < 0.1, 0x44: rng.uniform(-1, 1), 0x48: rng.uniform(-1, 1), 0x50: rng.random() < 0.5,
          0x58: rng.random() < 0.85, 0x59: rng.random() < 0.3}
    for off, kind in CTL_FIELDS:
        val = cv.get(off, 0)
        put(ctl, off, kind, f32(val) if kind == 'f' else val)
    torso = bytearray(TORSO)
    put(torso, 0x9c, 'i', rng.choice([0, 1]))
    for off in (0xbc, 0xc0, 0xc4, 0xc8):
        put(torso, off, 'f', f32(rng.uniform(-1, 1)))
    per_frame = rng.choice([0, 0, 2, -1])
    left = rng.choice([0, 1, 2, -1])
    budget = [int(rng.random() < 0.3), 0x8100, 0x8200, left & 0xffffffff, per_frame & 0xffffffff]
    flags = sum(b << i for i, b in enumerate([rng.random() < 0.5, rng.random() < 0.4, rng.random() < 0.1,
                                               rng.random() < 0.3, rng.random() < 0.3, rng.random() < 0.6]))
    h = struct.unpack_from('<f', mech, 0x68)[0]
    tr = bytes(struct.pack('<12f', *[f32(rng.uniform(-1, 1)) for _ in range(12)]))
    return {'mech': bytes(mech), 'ctl': bytes(ctl), 'torso': bytes(torso), 'transform': tr, 'budget': budget,
            'flags': flags, 'actions': rng.choice([0, 0, 0, 1, 2]), 'slid': int(rng.random() < 0.2),
            'surf1': f32(rng.choice([-300.0, -300.0, 0.0, h * 0.3, h * 0.4])),
            'surf4': f32(rng.choice([-300.0, -300.0, h * 0.5])), 'roster': rng.randrange(19),
            'lean_angle': f32(rng.choice([0.436, 0.2, 0.0])), 'lean_factor': f32(rng.choice([1.0, 2.0, 0.5])),
            'time': f32(rng.choice([100.0, 101.0, 102.0])), 'floor': f32(rng.choice([0.0, 0.0, 0.2])),
            'key': rng.choice([0, 0, 1, 2, 4, 5]), 'go': rng.choice([1, 1, 1, 0]), 'held': rng.choice([0, 0, 1, 2]),
            'blocked': [int(rng.random() < 0.15) for _ in range(4)],
            'dt': f32(rng.choice([1 / 30, 1 / 60, 0.1, 0.25])), 'dead': rng.randrange(5)}


def main():
    trials = int(sys.argv[1]) if len(sys.argv) > 1 else 20000
    emu = Emulated()
    rng = random.Random(4190)
    seen = set()
    names = ('mech', 'controls', 'torso', 'matrix', 'budget', 'calls')
    for n in range(trials):
        s = scenario(rng)
        want, got = emu.run(s), port(s, emu.torso)
        for k, name in enumerate(names):
            if want[k] != got[k]:
                if k < 4:
                    diff = [hex(o) for o in range(0, len(want[k]), 4) if want[k][o:o + 4] != got[k][o:o + 4]]
                    raise SystemExit(f'trial {n}: {name} differ at {diff}')
                raise SystemExit(f'trial {n}: {name} differ\n  original {want[k]}\n  port     {got[k]}')
        log = want[5]
        i = 0
        while i < len(log):
            seen.add(log[i + 1])
            i += log[i] + 1
    need = set(range(1, 41)) - {2, 29}
    if not need <= seen:
        raise SystemExit(f'coverage: missing {sorted(need - seen)}')
    print(f'ok: {trials} Mech::Update calls identical bit for bit; {len(seen)} kinds of call out reached')


if __name__ == '__main__':
    main()
