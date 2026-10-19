"""Jump jets (0x4173b0), AddJumpJet (0x417300), the landing (0x4162c0, with
the knockdown it can start, 0x41b660) and Move (0x417a80: the knockdown
check, the animation's root motion and its blend-in, then jets and the
landing for real) against the original x86 (docs/movement-jump.md,
docs/movement.md).

    cmake --build build && .venv/bin/python tests/differential/test_jets.py [trials]

A mech in emulator memory with a copy of the real vtable (its +0x5c switched
for remote mechs), MechControls with the real interface vtable, the pilot,
the configuration, the skis and logging legs and jump pose. The skis' update,
sounds, effects, force feedback, the fall's action queue and hip reset are
stubbed and logged on both sides; the jump pose (0x4177e7..0x4179fd) and the
landing's ray, dust and pose (0x4164f3.., 0x41660b..) and Move's samples
and pose (0x417bbc.., 0x417e9c.., 0x417eb8..) are skipped by redirecting the
original past them, and logged; the feet's ground update is a logged stand-in. Each trial is one call on a
random mech of the shipped tonnages; the fields both sides keep, the result
and the call log must match bit for bit at 24-bit precision.
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
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBP, UC_X86_REG_EIP  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
probe.probe_jets.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint32, ctypes.c_int,
                             ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_int,
                             ctypes.c_float, ctypes.c_float, ctypes.c_void_p, ctypes.c_void_p]
probe.probe_jets_seed.argtypes = [ctypes.c_uint32]

JETS, ADD, LAND, MOVE = 0x4173b0, 0x417300, 0x4162c0, 0x417a80
MECH_VT, MECH_SIZE, CTL_VT, CTL = 0x5bcef8, 0x6d8, 0x5bd428, 0x74
IAT_RAND, GRAVITY = 0x5bb668, 0x5e5d34
SKIPS = {0x4177e7: (8, 0x4179fd), 0x4164f3: (9, 0x41660b), 0x41660b: (10, 0x41662d), 0x417bbc: (None, 0x417c4f),
         0x417e9c: (30, 0x418187, 0x8), 0x417eb8: (30, 0x418187, 0x10)}
# probe_jets.cpp's mech_io and controls_io
MECH = ([(0x24 + 4 * k, 'f') for k in range(3)] + [(0x3c + 4 * k, 'f') for k in range(3)] +
        [(0x68, 'f'), (0x3a8, 'i'), (0x3d8, 'i'), (0x58c, 'i'), (0x6c4, 'f'), (0x6c8, 'f')] +
        [(0x488 + 4 * k, 'f') for k in range(3)] + [(0x518, 'f'), (0x53c, 'f')] +
        [(0x540 + 4 * k, 'f') for k in range(3)] + [(0x560, 'f'), (0x564, 'f'), (0x568, 'b'), (0x578, 'i'),
                                                     (0x57c, 'i'), (0x580, 'i'), (0x584, 'b')] +
        [(0x594 + 4 * k, 'f') for k in range(3)] + [(0x5c4, 'i'), (0x620, 'i'), (0x688, 'i')])
CTL_FIELDS = [(0x10, 'f'), (0x20, 'f'), (0x24, 'f'), (0x28, 'f'), (0x44, 'f'), (0x50, 'b')]


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


class MsvcRand:
    def __init__(self, seed):
        self.state = seed

    def __call__(self, cpu=None):
        self.state = (self.state * 214013 + 2531011) & 0xffffffff
        return (self.state >> 16) & 0x7fff


class Emulated:
    def __init__(self):
        cpu = self.cpu = Original()
        cpu.import_code(IAT_FTOL, FTOL)
        a = cpu.alloc
        self.mech, self.mvt, self.ctl, self.vbt = a(0x700), a(0x200), a(CTL), a(8)
        self.pilot, self.config, self.skis, self.pos = a(0x300), a(0x40), a(0x300), a(12)
        self.legs, self.lvt, self.hip, self.pose, self.avt = [a(0xd4), a(0xd4)], a(0x60), a(0x100), a(0xdc), a(0x50)
        self.player_obj, self.ff, self.optword = a(0x40), a(0x40), a(4)
        self.view, self.gait, self.gvt, self.feet, self.torso = a(0x80), a(0xdc), a(0x30), [a(0xec), a(0xec)], a(0x1b4)
        self.vecs = a(0x40)
        for off, code in ((0x8, 33), (0x10, 34)):
            f = cpu.code(b'\xc2\x04\x00')
            cpu.u32(self.gvt + off, f)
            cpu.stub_function(f, lambda c, code=code: self.log(code, c.arg(0)), 4)
        f = cpu.code(b'\xc2\x04\x00')
        cpu.u32(self.gvt + 0x20, f)
        cpu.stub_function(f, self.velocity, 4)
        cpu.write(0x462a10, bytes.fromhex('d9eec20400'))   # fldz; ret 4: the feet's ground update
        cpu.uc.hook_add(UC_HOOK_CODE, lambda uc, a_, s_, d_: self.log(31, self.feet.index(self.cpu.ecx)),
                        begin=0x462a10, end=0x462a10)
        cpu.stub_function(0x41e770, lambda c: self.log(32, c.ecx), 8)
        cpu.u32(0x5f4c8c, self.view)
        cpu.write(self.mvt, cpu.read(MECH_VT, 0x200))
        dmg, seek = cpu.code(b'\xc2\x08\x00'), cpu.code(b'\xc2\x04\x00')
        cpu.u32(self.lvt + 0x38, dmg)
        cpu.u32(self.avt + 0x44, seek)
        cpu.stub_function(dmg, lambda c: self.log(11, self.legs.index(c.ecx), c.arg(0), c.arg(1)), 8)
        cpu.stub_function(seek, lambda c: self.log(7, c.arg(0)), 4)
        stubs = {
            0x56e9f0: (lambda c: self.log(1, c.ecx), 0),
            0x506ca0: (lambda c: self.log(2, c.ecx), 0),
            0x432990: (lambda c: self.handle(3, c.arg(0), c.arg(1), c.arg(4)), 0x14),
            0x571d20: (lambda c: self.log(4, c.ecx) and 0, 4),
            0x432830: (lambda c: self.handle(5, c.arg(0), c.arg(1)), 0x10),
            0x433870: (lambda c: self.log(6, c.arg(0)), 4),
            0x416780: (lambda c: self.log(12, c.arg(0), c.arg(1)) or self.queue_ok, 8),
            0x4621f0: (lambda c: self.log(13), 4),
            0x433840: (lambda c: self.log(14, 0x433840), 0),
            0x4765b0: (lambda c: self.log(15, c.arg(0)) or self.sky, 4),
            0x433800: (lambda c: self.log(16, c.arg(0)), 4),
            0x5a85e0: (lambda c: self.pos, 0),
        }
        for addr, (fn, pop) in stubs.items():
            cpu.stub_function(addr, fn, pop)
        for addr in SKIPS:
            cpu.uc.hook_add(UC_HOOK_CODE, self.skip, begin=addr, end=addr)
        cpu.stub_import(IAT_RAND, lambda c: self.rand())
        cpu.u32(0x60861c, self.player_obj)
        cpu.u32(0x5f4d04, self.optword)

    def skip(self, uc, address, size, data):
        code, to, *arg = SKIPS[address]
        if code is not None:
            if arg:
                ebp = uc.reg_read(UC_X86_REG_EBP)
                self.log(code, struct.unpack('<I', self.cpu.read(ebp + arg[0], 4))[0])
            else:
                self.log(code)
        uc.reg_write(UC_X86_REG_EIP, to)

    def velocity(self, c):
        self.log(35)
        c.write(c.arg(0) + 8, struct.pack('<f', self.vz))
        return 0

    def log(self, *e):
        self.events.append(len(e))
        self.events.extend(v & 0xffffffff for v in e)
        return 0

    def handle(self, *e):
        self.log(*e)
        h = self.next
        self.next += 1
        return h

    def run(self, op, s):
        cpu = self.cpu
        self.events, self.next = [], 0x9000
        self.rand = MsvcRand(s['seed'])
        self.sky, self.queue_ok = s['flags'] >> 5 & 1, s['flags'] >> 6 & 1
        self.vz = s['vz']
        cpu.u32(self.view + 0x64, self.mech if s['flags'] & 128 else 0)
        cpu.u32(self.gait, self.gvt)
        for i in range(2):
            cpu.u32(self.legs[i] + 0x78, self.feet[i])
            cpu.u32(self.feet[i] + 0x18, self.vecs + 12 * i)
        cpu.u32(self.hip + 0x18, self.vecs + 24)
        cpu.u32(self.mech + 0x39c, self.torso)
        cpu.write(self.mech, s['mech'])
        cpu.u32(self.mech, self.mvt)
        cpu.u32(self.mvt + 0x5c, 0x4d2a70 if s['flags'] & 8 else 0x40bae0)
        cpu.u32(self.mech + 0x37c, self.ctl)
        cpu.u32(self.mech + 0x684, self.pilot)
        cpu.u32(self.pilot + 0xc, s['roster'])
        cpu.u32(self.mech + 0x3ec, self.config)
        cpu.u32(self.config + 0x14, s['roster'])
        cpu.u32(self.mech + 0x41c, self.skis)
        cpu.write(self.skis + 0x13c, struct.pack('<f', s['ground6']))
        cpu.write(self.pos, struct.pack('<3f', 0.0, s['pos_y'], 0.0))
        for i in range(2):
            cpu.u32(self.mech + 0x38c + 4 * i, self.legs[i])
            cpu.u32(self.legs[i], self.lvt)
        cpu.u32(self.mech + 0x3a0, self.hip)
        cpu.u32(self.mech + 0x454, self.pose if s['flags'] & 16 else 0)
        cpu.u32(self.pose, self.avt)
        cpu.write(self.ctl, s['ctl'])
        cpu.u32(self.ctl + 4, self.vbt)
        cpu.u32(self.vbt + 4, 0x5c)
        cpu.u32(self.ctl + 0x5c, 0)
        cpu.u32(self.ctl + 0x60, CTL_VT)
        cpu.u32(self.ctl + 0x70, self.mech)
        cpu.u32(self.player_obj + 0x14, self.mech if s['flags'] & 1 else 0)
        cpu.u32(0x608620, self.ff if s['flags'] & 2 else 0)
        cpu.u32(self.optword, 0x80 if s['flags'] & 4 else 0)
        cpu.write(GRAVITY, struct.pack('<f', s['gravity']))
        args = {0: (f2u(s['dt']),), 1: (f2u(s['dt']),), 2: (s['arg'],),
                3: (f2u(s['dt']), self.gait if s['arg'] else 0, f2u(s['speed']))}[op]
        cpu.call((JETS, LAND, ADD, MOVE)[op], ecx=self.mech, stack_args=args)
        r = cpu.uc.reg_read(UC_X86_REG_EAX) if op == 0 else 0
        return pick(cpu.read(self.mech, MECH_SIZE), MECH), pick(cpu.read(self.ctl, CTL), CTL_FIELDS), r, self.events


def port(op, s):
    mech, ctl = ctypes.create_string_buffer(s['mech'], MECH_SIZE), ctypes.create_string_buffer(s['ctl'], CTL)
    out, n = (ctypes.c_int32 * 4096)(), ctypes.c_int32(4096)
    probe.probe_jets_seed(s['seed'])
    r = probe.probe_jets(op, mech, ctl, s['flags'], s['roster'], s['pos_y'], s['ground6'], s['gravity'], s['dt'],
                         s['arg'], s['speed'], s['vz'], out, ctypes.byref(n))
    return pick(mech.raw, MECH), pick(ctl.raw, CTL_FIELDS), r, [v & 0xffffffff for v in out[:n.value]]


def scenario(rng):
    mech = bytearray(MECH_SIZE)
    jets = rng.choice([0, 0, 1, 2, 3, 4, 5])
    vals = {0x3c: rng.choice([0.0, 5.0, -25.0, 30.0, rng.uniform(-45, 45)]),
            0x40: rng.choice([0.0, -10.0, -57.0, -85.0, 3.0, rng.uniform(-120, 20)]),
            0x44: rng.choice([0.0, -10.0, 8.0, -60.0, rng.uniform(-90, 90)]),
            0x68: rng.choice([8.0, 12.0, 15.5]), 0x3a8: rng.choice([0, 0x7700]), 0x3d8: jets,
            0x518: rng.choice([60.0, 86.4, 108.0]), 0x53c: rng.choice([0.0, 3.0]),
            0x560: rng.choice([0.0, 0.125, 0.25, rng.uniform(0, 0.5)]),
            0x564: rng.choice([1.0, 0.0, 0.24, 0.26, 0.5, 0.999, rng.random()]), 0x568: rng.random() < 0.05,
            0x578: rng.choice([1, 3, 5, 7, 9, 11, 12]), 0x57c: rng.randrange(13),
            0x580: rng.random() < 0.4, 0x584: rng.random() < 0.5,
            0x598: rng.choice([0.0, 0.0, 1.0, 7.0, -1.0]), 0x5c4: rng.choice([0, 0, 0x5555]),
            0x620: rng.choice([0, 0, 0x6666]), 0x688: rng.choice([0xfffffffd, 0xfffffffd, 0x1d, 0x1c, 0x10, 0, 8]),
            0x24: rng.choice([0.0, 1.0, -0.6]), 0x2c: rng.choice([-1.0, 0.0, 0.8]),
            0x58c: rng.choice([0, 1, 3, 10, 19, 20, 25]), 0x6c4: 3.76, 0x6c8: 11.28}
    vals[0x53c] = rng.choice([0.0, 3.0, 3.76, 9.0, 15.04, 20.0])
    if rng.random() < 0.3:
        vals[0x584] = 0
    for off, kind in MECH:
        put(mech, off, kind, f32(vals[off]) if kind == 'f' and off in vals else vals.get(off, 0))
    for off in (0x488, 0x48c, 0x490, 0x540, 0x544, 0x548):
        put(mech, off, 'f', f32(rng.uniform(-5, 5)))
    ctl = bytearray(CTL)
    for off, v in ((0x10, rng.choice([0.0, 0.5, -0.4, 1.0])), (0x20, rng.choice([0.0, 0.0, 1.0, 0.6, 1e-6])),
                   (0x24, rng.choice([0.0, 0.0, 1.0, -1.0])), (0x28, rng.choice([0.0, 0.0, 1.0, -1.0])),
                   (0x44, rng.uniform(-1, 1))):
        put(ctl, off, 'f', f32(v))
    ctl[0x50] = rng.random() < 0.5
    flags = sum(b << i for i, b in enumerate([rng.random() < 0.5, rng.random() < 0.5, rng.random() < 0.3,
                                               rng.random() < 0.1, rng.random() < 0.6, rng.random() < 0.3,
                                               rng.random() < 0.6, rng.random() < 0.5]))
    ground = f32(rng.uniform(-20, 50))
    return {'mech': bytes(mech), 'ctl': bytes(ctl), 'flags': flags, 'roster': rng.choice(list(range(19)) + [7, 7]),
            'pos_y': f32(ground + rng.choice([0.0, 2.0, 5.0, 7.0, 30.0])), 'ground6': ground,
            'gravity': f32(rng.choice([30.0, 30.0, 15.0])), 'dt': f32(rng.choice([1 / 30, 1 / 60, 0.1, 0.25])),
            'arg': rng.randrange(2), 'seed': rng.randrange(1 << 31),
            'speed': f32(rng.uniform(-1.5, 1.5)), 'vz': f32(rng.uniform(-30, 10))}


def main():
    trials = int(sys.argv[1]) if len(sys.argv) > 1 else 40000
    emu = Emulated()
    rng = random.Random(3025)
    seen = set()
    for n in range(trials):
        op = rng.choice([0, 0, 1, 1, 2, 3, 3, 3])
        s = scenario(rng)
        want, got = emu.run(op, s), port(op, s)
        for k, name in enumerate(('mech', 'controls', 'result', 'calls')):
            if want[k] != got[k]:
                if k < 2:
                    diff = [hex(o) for o in range(0, len(want[k]), 4) if want[k][o:o + 4] != got[k][o:o + 4]]
                    raise SystemExit(f'trial {n} op {op}: {name} differ at {diff}')
                raise SystemExit(f'trial {n} op {op}: {name} differ\n  original {want[k]}\n  port     {got[k]}')
        log = want[3]
        i = 0
        while i < len(log):
            seen.add((op, log[i + 1]))
            i += log[i] + 1
    need = ({(0, c) for c in (1, 2, 3, 4, 5, 6, 7, 8)} | {(1, c) for c in (9, 10, 11, 12, 13, 14, 15, 16)} |
            {(3, c) for c in (12, 15, 30, 31, 32, 33, 34, 35)})
    if not need <= seen:
        raise SystemExit(f'coverage: missing {sorted(need - seen)}')
    print(f'ok: {trials} jump-jet, landing, AddJumpJet and Move calls identical bit for bit; '
          f'{len(seen)} kinds of call out reached')


if __name__ == '__main__':
    main()
