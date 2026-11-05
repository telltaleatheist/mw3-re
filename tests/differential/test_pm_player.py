"""PM's player controls 0x4299f0 (with the axes 0x429560 and the axis readers
0x430210 and on) and the AI's 0x4fab70 against PM's x86 (docs/controls.md).

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_player.py [trials]

PlayerControls in emulator memory with its virtual base (the real IControls
vtable 0x5e00d8 and its this-adjusting thunks), owned by a mech on the real
vtable. zInput is stubbed: each command's state (0x536c50), the joystick's
state and axis setup (0x537690, 0x537420), the mouse (0x534fe0), the hat's
commands (0x535c70), the mouse placement (0x534c50); the axis table 0x627d28,
the camera's view, the MW2 jump-jet option behind the game's vt+0x44 (0x40afd0),
the torso's absolute flag, the HUD (its zoom and flags) and the pilot's tick are
scenario data or logged stubs. Each trial runs one update at a random frame time;
the AI's runs its brain (0x4fab90) stubbed. PM's: states 12 and 13 skip the inputs,
the absolute torso's twist keys and rate, the jets' first thrust. A multiplayer
game's jet gate (its own +0xa7, 0x4365f0) is not here: multiplayer's item.
Every controls field, the zoom sound's slot, the return value and every call
out (each command read, in order) must match bit for bit at 24-bit x87.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

os.environ.setdefault('MW3_X87', '24')
sys.path.insert(0, str(Path(__file__).parent))
import pm_floors  # noqa: E402
from original import FTOL, IAT_FTOL  # noqa: E402
import test_pm_zar  # noqa: E402

probe = test_pm_zar.load_pm_probe()
P = ctypes.c_void_p
probe.probe_controls_load.argtypes = [P, P, P]
probe.probe_controls_op.argtypes = [ctypes.c_int, ctypes.c_float]
probe.probe_controls_store.argtypes = [ctypes.c_int, P, P, P, ctypes.c_int]

MECH_VT, ICTL_VT, CTL_SIZE, MECH_SIZE = 0x5dfaac, 0x5e00d8, 0x344, 0x748
AXES, CAMERA, MW2 = 0x627d28, 0x61874c, 0x40afd0
FLAGS, CAM_VIEW, SCREEN_FLAGS, SCREEN_BUSY, BRAIN, AXIS, COMMANDS = 0, 1, 2, 3, 4, 5, 21
JOY = COMMANDS + 0x40
JOY_CONFIG = JOY + 0x44
MOUSE = JOY_CONFIG + 12
ENV = MOUSE + 15
CMDS = [2, 3, 4, 5, 8, 9, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11, 0x12, 0x14, 0x18, 0x19, 0x3e]


def F(o, n=1):
    return [(o + 4 * k, 'f') for k in range(n)]


CTL_FIELDS = [(0x8, 'i'), (0xc, 'i')] + F(0x10, 9) + [(0x34, 'b'), (0x3c, 'b'), (0x3d, 'b'), (0x3e, 'b'),
                                                      (0x3f, 'b')] + F(0x40, 4) + [(0x50, 'b'), (0x54, 'i')]
PLAYER_FIELDS = CTL_FIELDS + F(0x158, 4)
MECH_FIELDS = [(0xe0, 'b'), (0xe2, 'b'), (0x3e8, 'i')] + F(0x544, 3) + [(0x59c, 'i'), (0x5a8, 'b'), (0x638, 'i')]


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


def u(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


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


class World:
    """The original's side."""

    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        z.cpu.import_code(IAT_FTOL, FTOL)
        a = z.alloc
        self.mech, self.mvt, self.ctl, self.vbt, self.ivt = a(MECH_SIZE), a(0x200), a(CTL_SIZE), a(8), a(0xc0)
        self.joy, self.joyc, self.mouse = a(0x110), a(0x118 * 6), a(0x40)
        self.camera, self.view, self.screen, self.zvt = a(0x80), a(0x50), a(0x1700), a(0x80)
        self.torso, self.missions, self.game, self.gvt = a(0x1bc), a(0x80), a(8), a(0x80)
        self.events = []
        self.wr(self.mvt, bytes(z.cpu._uc.mem_read(MECH_VT, 0x200)))
        self.wr(self.ivt, bytes(z.cpu._uc.mem_read(ICTL_VT, 0xc0)))
        self.vstub(self.ivt + 0x9c, lambda c: self.log(62), 0)
        self.vstub(self.zvt + 0x60, lambda c: self.log(65, 1) if c.arg(0) == u(-1.0) else
                   self.fail(f'zoom arg {c.arg(0):#x}'), 4)
        self.vstub(self.zvt + 0x64, lambda c: self.log(65, 0), 0)
        s = z.native_stub
        s(0x536c50, lambda c: self.log(61, c.arg(0)) or self.env[COMMANDS + c.arg(0)])
        s(0x537690, lambda c: self.joy)
        s(0x534fe0, lambda c: self.mouse)
        s(0x537420, lambda c: self.joyc)
        s(0x5375c0, lambda c: self.env[FLAGS] >> 1 & 1)
        s(0x535c70, lambda c: self.log(66, c.arg(0)))
        s(0x534c50, lambda c: self.log(67, c.arg(0), c.arg(1)))
        s(MW2, lambda c: self.env[FLAGS] >> 2 & 1)
        s(0x430e30, lambda c: self.log(63) or (self.screen if self.env[FLAGS] >> 3 & 1 else 0))
        s(0x432d10, self.sound, 20)
        # The AI's brain returns a byte; 0x4fab70 makes it 0 or 1.
        s(0x4fab90, lambda c: (self.log(68, c.arg(0)) or self.env[BRAIN]) if c.ecx == self.ctl + 0x5c else
          self.fail('brain this'), 4)
        self.w32(self.gvt + 0x44, 0x435070)
        self.w32(self.game, self.gvt)
        self.w32(self.missions + 0x6c, self.game)
        self.w32(0x62c40c, self.missions)
        self.wr(0x629314, struct.pack('<f', 38.0))  # 0x417e80's, at start-up

    def wr(self, a, b):
        self.z.cpu._uc.mem_write(a, bytes(b))

    def w32(self, a, v):
        self.wr(a, struct.pack('<I', v & 0xffffffff))

    def fail(self, why):
        raise SystemExit(why)

    def vstub(self, slot, fn, pop):
        addr = self.z.cpu.code(b'\xc3')
        self.w32(slot, addr)
        self.z.native_stub(addr, fn, pop)

    def log(self, *e):
        self.events.append(len(e))
        self.events.extend(v & 0xffffffff for v in e)

    def sound(self, c):
        args = [c.arg(k) for k in range(5)]
        if args[2:] != [0, 0, u(-1.0)]:
            raise SystemExit(f'sound args {args}')
        self.log(64, args[0], args[1], 0)
        return 0x77

    def load(self, s, op):
        self.env = s['env']
        self.wr(self.mech, s['mech'])
        self.w32(self.mech, self.mvt)
        self.w32(self.mech + 0x3ac, self.torso)
        self.wr(self.torso + 0xa8, bytes([self.env[FLAGS] >> 4 & 1]))
        img = bytearray(CTL_SIZE)
        img[:len(s['ctl'])] = s['ctl']
        self.wr(self.ctl, img)
        self.w32(self.ctl, 0x5e0194)  # the smoothing's zero goes through it (vt+0x18)
        self.w32(self.ctl + 4, self.vbt)
        if op == 0:
            self.wr(self.vbt, struct.pack('<II', 0, 0x168))
            self.w32(self.ctl + 0x168, 0)
            self.w32(self.ctl + 0x16c, self.ivt)
            self.w32(self.ctl + 0x188, self.mech)
        self.wr(AXES, struct.pack('<16I', *self.env[AXIS:AXIS + 16]))
        self.wr(self.joy, struct.pack('<68I', *self.env[JOY:JOY + 0x44]))
        for k in range(6):
            self.wr(self.joyc + 0x118 * k + 8, struct.pack('<2I', *self.env[JOY_CONFIG + 2 * k:JOY_CONFIG + 2 * k + 2]))
        self.wr(self.mouse, struct.pack('<15I', *self.env[MOUSE:MOUSE + 15]))
        self.w32(CAMERA, self.camera)
        self.w32(self.camera + 0x58, self.view)
        self.w32(self.view + 0x40, self.env[CAM_VIEW])
        self.w32(self.camera + 0x64, self.mech if self.env[FLAGS] & 1 else 0)
        self.w32(self.screen + 0x152c, self.env[SCREEN_FLAGS])
        self.wr(self.screen + 0x1694, bytes([self.env[SCREEN_BUSY]]))
        self.w32(self.screen + 0x1520, self.zvt)
        self.events = []

    def run(self, op, dt):
        if op == 0:
            return self.z.call(0x4299f0, ecx=self.ctl + 0x16c, args=(u(dt),))
        return self.z.call(0x4fab70, ecx=self.ctl + 0x330, args=(u(dt),))

    def store(self, op):
        r = lambda a, n: bytes(self.z.cpu._uc.mem_read(a, n))
        return {'mech': pick(r(self.mech, MECH_SIZE), MECH_FIELDS),
                'ctl': pick(r(self.ctl, 0x180), PLAYER_FIELDS if op == 0 else CTL_FIELDS)}, self.events


def rnd(rng, lo, hi, zero=0.0):
    return 0.0 if rng.random() < zero else f32(rng.uniform(lo, hi))


def scenario(rng):
    mech = bytearray(MECH_SIZE)
    put(mech, 0xe0, 'b', rng.random() < 0.08)
    put(mech, 0xe2, 'b', rng.random() < 0.08)
    put(mech, 0x3e8, 'i', rng.choice([0, 0, 2, 4]))
    for o in (0x544, 0x548, 0x54c):
        put(mech, o, 'f', rnd(rng, 0.2, 4))
    put(mech, 0x59c, 'i', rng.choice([0, 0, 0, 0, 1, 5, 10, 11, 12, 13, 14]))
    put(mech, 0x5a8, 'b', rng.random() < 0.3)
    put(mech, 0x638, 'i', rng.choice([0, 5]))
    ctl = bytearray(0x180)
    put(ctl, 0x8, 'i', rng.choice([0, 4]))
    put(ctl, 0xc, 'i', rng.choice([0, 0, 1, 2, 3, 4]))
    for o in range(0x10, 0x34, 4):
        put(ctl, o, 'f', rnd(rng, -1.5, 1.5, 0.3))
    for o in (0x3c, 0x3d, 0x3e, 0x3f):
        put(ctl, o, 'b', rng.random() < 0.4)
    put(ctl, 0x40, 'f', rng.choice([0.0, rnd(rng, -1, 1)]))
    for o in (0x44, 0x48, 0x4c):
        put(ctl, o, 'f', rnd(rng, -1, 1, 0.3))
    put(ctl, 0x50, 'b', rng.random() < 0.3)
    pov = [0xffffffff, 0xffff, 0, 4499, 4500, 9000, 13500, 13501, 18000, 22499, 22500, 27000, 31500, 31501, 35999]
    put(ctl, 0x54, 'i', rng.choice(pov + [1]))
    for o in range(0x158, 0x168, 4):
        put(ctl, o, 'f', rnd(rng, -1.2, 1.2, 0.3))
    env = [0] * ENV
    env[FLAGS] = rng.getrandbits(5)
    env[CAM_VIEW] = rng.choice([0, 1, 2, 3, 4, 5, 6])
    env[SCREEN_FLAGS] = rng.choice([0, 0x10, 0xffffffff, 0xffffffef])
    env[SCREEN_BUSY] = rng.random() < 0.2
    env[BRAIN] = rng.choice([0, 1, 0xff])
    for k in range(4):
        src = rng.choice([0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12])
        if k in (1, 2) and rng.random() < 0.3:
            src = rng.choice([9, 10, 11])
        env[AXIS + 4 * k] = src
        env[AXIS + 4 * k + 1] = u(rnd(rng, -2, 2))
        env[AXIS + 4 * k + 2] = u(rnd(rng, -0.2, 0.2, 0.5))
        env[AXIS + 4 * k + 3] = u(rng.choice([0.0, rnd(rng, 0, 0.3)]))
    for c in CMDS:
        env[COMMANDS + c] = rng.choice([0, 0, 0, 0, 1, 2, 3, 4, 5])
    joy = [rng.randrange(-1000, 1001) for _ in range(6)] + [rng.randrange(-1000, 1001) for _ in range(2)]
    for k in range(4):  # a slider exactly on its dead zone's edge now and then
        if env[AXIS + 4 * k] == 7 and rng.random() < 0.5:
            sc, of = (struct.unpack('<f', struct.pack('<I', env[AXIS + 4 * k + j]))[0] for j in (1, 2))
            env[AXIS + 4 * k + 3] = u(abs(f32(f32(sc * float(joy[6])) + of)))
    joy += [rng.choice(pov) for _ in range(4)] + [0] * 32 + [rng.randrange(-50, 51) for _ in range(8)] + [0] * 16
    env[JOY:JOY + 0x44] = [v & 0xffffffff for v in joy]
    for k in range(6):
        env[JOY_CONFIG + 2 * k] = u(rnd(rng, 0.0005, 0.002))
        env[JOY_CONFIG + 2 * k + 1] = u(rnd(rng, -10, 10))
    mouse = [rng.randrange(0, 640), rng.randrange(0, 480), 500] + [u(rnd(rng, -1, 1)) for _ in range(3)] + \
        [rng.randrange(-20, 21) for _ in range(3)] + [u(rnd(rng, -0.05, 0.05)) for _ in range(3)] + [0, 0, 0]
    env[MOUSE:MOUSE + 15] = [v & 0xffffffff for v in mouse]
    return {'mech': bytes(mech), 'ctl': bytes(ctl), 'env': env}


def port_run(s, op, dt):
    env = (ctypes.c_uint32 * ENV)(*s['env'])
    probe.probe_controls_load(s['mech'], s['ctl'], env)
    ret = probe.probe_controls_op(op, dt)
    mech = ctypes.create_string_buffer(MECH_SIZE)
    ctl = ctypes.create_string_buffer(0x180)
    out = (ctypes.c_int32 * 4096)()
    n = probe.probe_controls_store(op, mech, ctl, out, 4096)
    return ret, {'mech': pick(mech.raw, MECH_FIELDS),
                 'ctl': pick(ctl.raw, PLAYER_FIELDS if op == 0 else CTL_FIELDS)}, [v & 0xffffffff for v in out[:n]]


def main():
    world = World()
    rng = random.Random(1999)
    trials = int(sys.argv[1]) if len(sys.argv) > 1 else 20000
    counts = [0, 0]
    kinds = set()
    for n in range(trials):
        op = 0 if rng.random() < 0.9 else 1
        s = scenario(rng)
        dt = rng.choice([f32(1 / 30), f32(1 / 60), rnd(rng, 0.001, 0.2), rnd(rng, 0.05, 0.4)])
        world.load(s, op)
        wret = world.run(op, dt)
        want, wlog = world.store(op)
        gret, got, glog = port_run(s, op, dt)
        what = f'trial {n} op {op} dt {dt}'
        if wret != gret:
            raise SystemExit(f'{what}: returns {wret} {gret}')
        for key in want:
            if want[key] != got[key]:
                w, g = want[key], got[key]
                diff = [o for o in range(0, len(w), 4) if w[o:o + 4] != g[o:o + 4]]
                vals = [(struct.unpack('<f', w[o:o + 4])[0], struct.unpack('<f', g[o:o + 4])[0]) for o in diff[:4]]
                raise SystemExit(f'{what}: {key} differs at {[hex(o) for o in diff[:8]]} {vals}')
        if wlog != glog:
            k = next(i for i in range(min(len(wlog), len(glog)) + 1) if i >= len(wlog) or i >= len(glog) or
                     wlog[i] != glog[i])
            raise SystemExit(f'{what}: calls differ at {k}\n  original {wlog[max(0, k - 8):k + 24]}\n'
                             f'  port     {glog[max(0, k - 8):k + 24]}')
        counts[op] += 1
        i = 0
        while i < len(wlog):
            kinds.add(wlog[i + 1])
            i += wlog[i] + 1
    pm_floors.floors(player=(counts[0], 15000), ai=(counts[1], 1000), kinds=(len(kinds), 7))
    print(f'ok: {trials} controls updates identical bit for bit (player {counts[0]}, AI {counts[1]}); '
          f'{len(kinds)} kinds of call out')


if __name__ == '__main__':
    main()
