"""The gait choice (0x4181c0) and turning (0x418fc0, with the pivot leg
0x418e00 and the foot's 0x462dc0) against the original x86 (docs/movement.md).

    cmake --build build && .venv/bin/python tests/differential/test_gait.py

A whole mech in emulator memory with the real vtable, controls laid out as
MechControls with its real interface vtable, legs, feet, the torso and
thirteen animation objects whose SetBlend and Seek are logged; the gait's
CopyPhase (0x41ef00) runs for real. Move (0x417a80), the crouch and the
wading effect are stubbed and logged. Each trial puts a random mech of one of
the 19 shipped definitions (its gait table and turn rate) in a random state
near the gait thresholds, runs one call on both sides, and compares every
field both sides keep and the call log, bit for bit at 24-bit precision.

This checks the exe's path: the probes turn the fixes off. With them on the
port departs in the wading slowdown (zfix deep_water: a straight line from 1
at a tenth of the height to 0.7 at full depth instead of 0.7 (H - d) / H) and
in the blend-in's frame count (per 1/30 s); tests/test_fixes.cpp checks those.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

os.environ.setdefault('MW3_X87', '24')
sys.path.insert(0, str(Path(__file__).parent))
from mechdata import mechs  # noqa: E402
from original import FTOL, IAT_FTOL, ROOT, Original  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
probe.probe_gait.argtypes = [ctypes.c_int] + [ctypes.c_char_p] * 5 + [ctypes.c_uint32, ctypes.c_int, ctypes.c_int,
                                                                       ctypes.c_float, ctypes.c_void_p, ctypes.c_int]

GAIT, TURN = 0x4181c0, 0x418fc0
MECH_VT, MECH_SIZE, CTL_IFACE_VT = 0x5bcef8, 0x6d8, 0x5bd428
FLOOR = bytes.fromhex('83ec04 d93c24 668b0424 6625fff3 660d0004 6689442402 d96c2402 dd442408 d9fc d92c24 83c404 c3')
IAT_FLOOR = 0x5bb66c
ANIM, NANIM, FOOT, TORSO, CTL = 0xdc, 13, 0x100, 0x1b4, 0x74

# Mech fields the two functions read or write (offset, kind); probe_move.cpp's mech_io.
MECH = ([(0x3c + 4 * k, 'f') for k in range(3)] + [(0x58 + 4 * k, 'f') for k in range(3)] +
        [(0x68, 'f'), (0xde, 'b'), (0x418, 'f'), (0x420, 'i')] + [(0x4a0 + 4 * k, 'f') for k in range(26)] +
        [(0x520, 'f'), (0x528, 'f'), (0x52c, 'f'), (0x530, 'f'), (0x534, 'f'), (0x538, 'f'), (0x554, 'f'),
         (0x578, 'i'), (0x57c, 'i'), (0x584, 'b'), (0x586, 'b'), (0x588, 'b'), (0x58c, 'i'), (0x5ac, 'i'),
         (0x688, 'i'), (0x6b0, 'f')])
CTL_FIELDS = [(0x08, 'i')] + [(o, 'f') for o in (0x10, 0x14, 0x18, 0x1c, 0x20, 0x24, 0x28, 0x2c, 0x30, 0x44, 0x48,
                                                  0x4c)] + [(0x50, 'b')]
FOOT_FIELDS = [(0x8c, 'f'), (0x94, 'i'), (0xc0, 'b'), (0xd4, 'f'), (0xdc, 'f'), (0xf0, 'f'), (0xf4, 'f'), (0xf8, 'f')]
ANIM_FIELDS = [(o, 'f') for o in (0x04, 0x08, 0x0c, 0x54, 0x88, 0x8c, 0xd0, 0xd4, 0xd8)]


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
    """Only the listed fields, so both sides compare what they keep."""
    out = bytearray(len(img))
    for off, kind in fields:
        n = 1 if kind == 'b' else 4
        out[off:off + n] = img[off:off + n]
    return bytes(out)


class Emulated:
    def __init__(self):
        cpu = self.cpu = Original()
        cpu.import_code(IAT_FTOL, FTOL)
        cpu.import_code(IAT_FLOOR, FLOOR)
        a = cpu.alloc
        self.mech, self.ctl, self.vbt = a(0x700), a(CTL), a(8)
        self.legs, self.feet, self.torso = [a(0xd4), a(0xd4)], [a(FOOT), a(FOOT)], a(TORSO)
        self.anims, self.avt = [a(ANIM) for _ in range(NANIM)], a(0x50)
        self.player_obj, self.ff_obj = a(0x40), a(0x40)
        blend, seek = cpu.code(b'\xc2\x04\x00'), cpu.code(b'\xc2\x04\x00')
        cpu.write(self.avt, struct.pack('<20I', *([0] * 20)))
        cpu.u32(self.avt + 0x14, blend)
        cpu.u32(self.avt + 0x44, seek)
        cpu.stub_function(blend, lambda c: self.log(4, self.anims.index(c.ecx), c.arg(0)), 4)
        cpu.stub_function(seek, lambda c: self.log(5, self.anims.index(c.ecx), c.arg(0)), 4)
        cpu.stub_function(0x417a80, lambda c: self.log(1, c.arg(0), self.index(c.arg(1)), c.arg(2)), 0xc)
        cpu.stub_function(0x41bda0, lambda c: self.log(2, c.arg(0)), 4)
        cpu.stub_function(0x433930, lambda c: self.check(c.ecx == self.ff_obj) or self.log(3, c.arg(0)), 4)
        cpu.u32(0x60861c, self.player_obj)

    def index(self, p):
        return self.anims.index(p) if p in self.anims else -1

    def check(self, ok):
        if not ok:
            raise SystemExit('a stub was called on the wrong object')

    def log(self, *e):
        self.events.append(len(e))
        self.events.extend(v & 0xffffffff for v in e)

    def load(self, s):
        cpu = self.cpu
        cpu.write(self.mech, s['mech'])
        cpu.u32(self.mech, MECH_VT)
        cpu.u32(self.mech + 0x37c, self.ctl)
        cpu.u32(self.mech + 0x38c, self.legs[0])
        cpu.u32(self.mech + 0x390, self.legs[1])
        cpu.u32(self.mech + 0x39c, self.torso)
        for k in range(NANIM):
            cpu.u32(self.mech + 0x424 + 4 * k, self.anims[k] if s['present'] >> k & 1 else 0)
            cpu.write(self.anims[k], s['anims'][k * ANIM:(k + 1) * ANIM])
            cpu.u32(self.anims[k], self.avt)
        cpu.write(self.ctl, s['ctl'])
        cpu.u32(self.ctl + 4, self.vbt)
        cpu.u32(self.vbt + 4, 0x5c)
        cpu.u32(self.ctl + 0x5c, 0)
        cpu.u32(self.ctl + 0x60, CTL_IFACE_VT)
        cpu.u32(self.ctl + 0x70, self.mech)
        for i in range(2):
            cpu.u32(self.legs[i] + 0x78, self.feet[i])
            cpu.write(self.feet[i], s['feet'][i * FOOT:(i + 1) * FOOT])
            cpu.u32(self.feet[i] + 0x18, self.feet[i] + 0xf0)
        cpu.write(self.torso, s['torso'])
        cpu.u32(self.player_obj + 0x14, self.mech if s['player'] else 0)
        cpu.u32(0x608620, self.ff_obj if s['ff'] else 0)

    def run(self, op, s, dt):
        self.load(s)
        self.events = []
        self.cpu.call(GAIT if op == 0 else TURN, ecx=self.mech, stack_args=(f2u(dt),))
        cpu = self.cpu
        return {'mech': pick(cpu.read(self.mech, MECH_SIZE), MECH), 'ctl': pick(cpu.read(self.ctl, CTL), CTL_FIELDS),
                'feet': b''.join(pick(cpu.read(f, FOOT), FOOT_FIELDS) for f in self.feet),
                'anims': b''.join(pick(cpu.read(a, ANIM), ANIM_FIELDS) for a in self.anims)}, self.events


def port(op, s, dt):
    mech, ctl = ctypes.create_string_buffer(s['mech'], MECH_SIZE), ctypes.create_string_buffer(s['ctl'], CTL)
    feet, torso = ctypes.create_string_buffer(s['feet'], 2 * FOOT), ctypes.create_string_buffer(s['torso'], TORSO)
    anims = ctypes.create_string_buffer(s['anims'], NANIM * ANIM)
    out = (ctypes.c_int32 * 4096)()
    n = probe.probe_gait(op, mech, ctl, feet, torso, anims, s['present'], s['player'], s['ff'], dt, out, 4096)
    log = []
    for v in out[:n]:
        log.append(v & 0xffffffff if v < 0 else v)
    return {'mech': pick(mech.raw, MECH), 'ctl': pick(ctl.raw, CTL_FIELDS),
            'feet': b''.join(pick(feet.raw[i * FOOT:(i + 1) * FOOT], FOOT_FIELDS) for i in range(2)),
            'anims': b''.join(pick(anims.raw[k * ANIM:(k + 1) * ANIM], ANIM_FIELDS) for k in range(NANIM))}, log


def near(rng, table, scale):
    """A throttle around one of the gait bounds (in throttle units after the speed factor)."""
    bounds = [v for pair in table[3:7] for v in pair]
    b = rng.choice(bounds) / scale
    r = rng.random()
    if r < 0.3:
        v = b
    elif r < 0.7:
        v = b * (1 + rng.uniform(-0.02, 0.02)) + rng.uniform(-1e-3, 1e-3)
    else:
        v = rng.uniform(0, 1.2)
    return f32(v if rng.random() < 0.75 else -v)


def scenario(rng, defs):
    name = rng.choice(sorted(defs))
    d = defs[name]
    scale_max = rng.choice([1.0, 1.0, 1.0, 0.8])  # MASC lowers every bound by 0.8
    table = [(f32(lo * scale_max), f32(hi * scale_max)) for lo, hi in d['gait']]
    mech = bytearray(MECH_SIZE)
    sf = f32(rng.choice([1.0, 1.0, 0.6, 0.36, rng.uniform(0.3, 1.0)]))
    for k, (lo, hi) in enumerate(table):
        put(mech, 0x4a0 + 8 * k, 'f', lo)
        put(mech, 0x4a4 + 8 * k, 'f', hi)
    height = f32(rng.choice([8.0, 12.0, 15.5]))
    vals = {0x68: height, 0xde: rng.random() < 0.04, 0x418: f32(rng.random()), 0x420: rng.choice([2, 3, 3, 4, 5, 6, 8, 9, 10]),
            0x520: d['turn_rate'], 0x528: sf, 0x52c: f32(rng.random()), 0x530: d['accel'], 0x534: d['decel'],
            0x538: d['turn_accel'],
            0x554: f32(rng.choice([0.0, 0.3, 0.6, 0.61, 0.9, 1.2, 2.0, rng.uniform(0, 2)])),
            0x578: rng.choice([0, 1, 1, 2, 3, 3, 4, 5, 5, 6, 7, 8, 9, 11, 12, 13]),
            0x57c: rng.randrange(14), 0x584: rng.random() < 0.08, 0x586: rng.random() < 0.15,
            0x588: rng.random() < 0.3, 0x58c: rng.randrange(30), 0x5ac: rng.choice([-1, -1, -1, 0, 1]),
            0x688: rng.choice([0xffffffff, 0xffffffff, 0x1d, 0x1c, 0x10, 0, 0x14, 0xffffffef]),
            0x6b0: f32(rng.choice([1 / 30, 1 / 60, 0.1, rng.uniform(0.005, 0.2)]))}
    for k in range(3):
        put(mech, 0x3c + 4 * k, 'f', f32(rng.uniform(-10, 10)))
        put(mech, 0x58 + 4 * k, 'f', f32(rng.uniform(-1, 1)))
    for off, kind in MECH:
        if off in vals:
            put(mech, off, kind, vals[off])
    ctl = bytearray(CTL)
    thr = near(rng, table, sf)
    put(ctl, 0x10, 'f', thr)
    put(ctl, 0x44, 'f', rng.choice([thr, 0.0, near(rng, table, 1.0)]))
    put(ctl, 0x14, 'f', rng.choice([0.0, 0.0, 1.0, -1.0, f32(rng.uniform(-1, 1))]))
    put(ctl, 0x50, 'b', rng.random() < 0.5)
    feet = bytearray(2 * FOOT)
    for i in range(2):
        b = FOOT * i
        put(feet, b + 0x8c, 'f', f32(rng.choice([0.0, 0.1, 0.3, rng.uniform(-0.5, 2)])))
        put(feet, b + 0x94, 'i', rng.choice([0, 1, 2, 3, 3]))
        put(feet, b + 0xdc, 'f', f32(rng.choice([0.0, 0.0, height * 0.05, height * 0.3, height * 0.99, height,
                                                height * 1.4, rng.uniform(0, height * 1.2)])))
        for k in range(3):
            put(feet, b + 0xf0 + 4 * k, 'f', f32(rng.uniform(-3, 3)))
    torso = bytearray(TORSO)
    put(torso, 0x1b0, 'i', rng.choice([0, 0, 0, 1, 2]))
    anims = bytearray(NANIM * ANIM)
    for k in range(NANIM):
        b = ANIM * k
        loop = f32(rng.choice([1.0, 1.2, 1.5, 2.0, rng.uniform(0.5, 3)]))
        put(anims, b + 0x54, 'f', loop)
        put(anims, b + 0x04, 'f', f32(rng.choice([1.0, rng.uniform(0.2, 2.5)])))
        put(anims, b + 0x88, 'f', f32(rng.uniform(0, 3 * loop)))
        put(anims, b + 0xd0, 'f', f32(rng.uniform(0, loop * 0.5)))
        put(anims, b + 0xd4, 'f', f32(rng.uniform(loop * 0.5, loop)))
        put(anims, b + 0xd8, 'f', f32(loop * 0.25))
    present = 0x17fc | (rng.random() < 0.7) << 11 | (rng.random() < 0.5) * 3
    return {'mech': bytes(mech), 'ctl': bytes(ctl), 'feet': bytes(feet), 'torso': bytes(torso), 'anims': bytes(anims),
            'present': present, 'player': rng.random() < 0.5, 'ff': rng.random() < 0.6}


def main():
    defs = mechs()
    emu = Emulated()
    rng = random.Random(1999)
    trials = int(sys.argv[1]) if len(sys.argv) > 1 else 30000
    states, slots, calls = set(), set(), set()
    for n in range(trials):
        op = 0 if rng.random() < 0.75 else 1
        s = scenario(rng, defs)
        dt = struct.unpack('<f', s['mech'][0x6b0:0x6b4])[0]
        want, wlog = emu.run(op, s, dt)
        got, glog = port(op, s, dt)
        for part in want:
            if want[part] != got[part]:
                w, g = want[part], got[part]
                diff = [hex(o) for o in range(0, len(w), 4) if w[o:o + 4] != g[o:o + 4]]
                raise SystemExit(f'trial {n} op {op}: {part} differs at {diff[:8]}')
        if wlog != glog:
            raise SystemExit(f'trial {n} op {op}: calls differ\n  original {wlog}\n  port     {glog}')
        if op == 0:
            states.add(struct.unpack('<i', want['mech'][0x578:0x57c])[0])
            slots.add(struct.unpack('<i', want['mech'][0x420:0x424])[0])
        i = 0
        while i < len(wlog):
            calls.add(wlog[i + 1])
            if wlog[i + 1] == 5:
                calls.add(('seek', wlog[i + 2], wlog[i + 3]))
            i += wlog[i] + 1
    need = set(range(10)) - {2}
    if not need <= states or not {2, 3, 4, 5, 6, 8, 9, 10} <= slots or not {1, 2, 3, 4, 5, ('seek', 3, 2), ('seek', 8, 0), ('seek', 8, 1)} <= calls:
        raise SystemExit(f'coverage: states {sorted(states)}, slots {sorted(slots)}, calls {calls}')
    print(f'ok: {trials} gait and turn calls identical bit for bit; states {sorted(states)}, '
          f'slots {sorted(slots)}')


if __name__ == '__main__':
    main()
