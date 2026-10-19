"""The options' getters and setters against the original (docs/options.md).

Random option states and random sequences of the setters and getters at
0x40cbf0..0x40dae0 plus apply-all 0x40cb30 run on both sides, with the calls
into other modules stubbed and recorded. Compared: the call traces (getter
results included), every option word, and what the setters wrote into the
pilot's mech.

    cmake --build build && .venv/bin/python tests/differential/test_app_options.py
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import Emu, Trace, compare, load_probe  # noqa: E402

PILOT, VIEW, VIDEO_MODE, LOCKED = 0x60861c, 0x5f4c8c, 0x72f348, 0x608e45
# set_camera's x87 division can round one ulp apart from the port's double.
ULPS = {'0x004f3d90': [1]}
FLOAT_GETTERS = {0x40daa0, 0x40dae0}
FLOAT_SETTERS = {0x40da80, 0x40dab0}
TWO_ARGS = {0x40d600, 0x40d630, 0x40d6e0, 0x40d720, 0x40d7d0, 0x40d7f0}
SETTERS = [0x40d4c0, 0x40cbf0, 0x40cc20, 0x40ce00, 0x40ce10, 0x40ce90, 0x40ced0, 0x40cf30,
           0x40cf90, 0x40cfd0, 0x40d020, 0x40d070, 0x40d1c0, 0x40d1f0, 0x40d240, 0x40d2a0,
           0x40d310, 0x40d3d0, 0x40d3f0, 0x40d430, 0x40d4a0, 0x40d590, 0x40d600, 0x40d630,
           0x40d670, 0x40d6b0, 0x40d6e0, 0x40d720, 0x40d780, 0x40d7d0, 0x40d7f0, 0x40d810,
           0x40da80, 0x40dab0, 0x40cb30]
GETTERS = [0x40d4d0, 0x40cc10, 0x40cdf0, 0x40ce70, 0x40cec0, 0x40cf20, 0x40cf80, 0x40cfc0,
           0x40d010, 0x40d060, 0x40d1b0, 0x40d1e0, 0x40d220, 0x40d280, 0x40d2f0, 0x40d3b0,
           0x40d3e0, 0x40d410, 0x40d480, 0x40d4b0, 0x40d5e0, 0x40d760, 0x40d770, 0x40d790,
           0x40d7a0, 0x40d7c0, 0x40d9d0, 0x40daa0, 0x40dae0]
STUBS = {0x40ae70: 'c,s0', 0x4b0220: 'c,s0', 0x41cf30: 'c,s0', 0x4e6920: 'cl', 0x508fb0: 'c',
         0x579ea0: '', 0x579b70: 'c', 0x526a70: 'c', 0x4f3ff0: 'c,s0', 0x430440: 's0',
         0x4f7fe0: 'c,zd', 0x4eb170: 'c,d', 0x56f260: 's0', 0x4ac230: 'c,d', 0x4f3d90: 'c,s0,s1',
         0x4f9fa0: 'c,d,s0', 0x4fa0b0: 'c,d,s0', 0x4fa580: 'c,d,s0', 0x4fa600: 'c,d,s0',
         0x51b590: 'c,d', 0x556740: 'zc'}


def fbits(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


class Layout:
    """Where each probe_options_io word lives in the original's memory."""

    def __init__(self, emu):
        self.emu = emu
        e = emu
        entries = {}
        for g in range(0x5f4ccc, 0x5f4d4c, 4):
            if g in (0x5f4d18, 0x5f4d1c, 0x5f4d20, 0x5f4d24, 0x5f4d40):
                size = 0xc if g == 0x5f4d18 else 0x104 if g == 0x5f4d40 else 0x28
                buf = e.alloc(size)
                ent = e.alloc(0x20)
                e.w32(ent, buf)
                entries[g] = buf
            else:
                ent = e.alloc(0x20)
                entries[g] = ent
            e.w32(g, ent)
        self.words = [0x5f4cc8, entries[0x5f4ccc], entries[0x5f4cd0]]
        for g in (0x5f4cd4, 0x5f4cdc, 0x5f4ce4, 0x5f4cec, 0x5f4cf4):
            self.words += [entries[g], entries[g + 4]]
        self.words += [entries[g] for g in (0x5f4cfc, 0x5f4d00, 0x5f4d04, 0x5f4d08, 0x5f4d0c,
                                            0x5f4d10, 0x5f4d14)]
        self.words += [entries[0x5f4d18] + 4 * i for i in range(3)]
        for g in (0x5f4d1c, 0x5f4d20, 0x5f4d24):
            self.words += [entries[g] + 4 * i for i in range(10)]
        self.words += [entries[g] for g in (0x5f4d2c, 0x5f4d30, 0x5f4d34, 0x5f4d38, 0x5f4d3c,
                                            0x5f4d44, 0x5f4d48)]
        self.words += [0x5f4d4c, 0x608e54, 0x608e58, 0x608e5c]
        self.bytes_ = {len(self.words): 0x608e4c, len(self.words) + 1: 0x608e4d}
        self.words += [0x608e4c, 0x608e4d]
        self.words += [0x6040d0 + 4 * i for i in range(16)]
        for a in (0x604110, 0x609994, LOCKED):
            self.bytes_[len(self.words)] = a
            self.words.append(a)

    def write(self, values):
        for i, (a, v) in enumerate(zip(self.words, values)):
            if i in self.bytes_:
                self.emu.w8(a, v)
            else:
                self.emu.w32(a, v)

    def read(self):
        return [self.emu.r8(a) if i in self.bytes_ else self.emu.r32(a)
                for i, a in enumerate(self.words)]


def random_state(rng, n):
    w = [rng.randrange(1 << 16) for _ in range(n)]
    w[0] = rng.randrange(2)                       # hw
    for i in range(3, 13):
        w[i] = rng.randrange(-1, 4) & 0xffffffff  # per-renderer values
    w[1] = rng.randrange(2)
    w[2] = rng.randrange(-2, 3) & 0xffffffff
    w[15] = rng.randrange(1 << 10)                # GameOptions
    w[16] = rng.randrange(-1, 4) & 0xffffffff
    w[17] = rng.randrange(2, 12)
    w[22] = rng.choice([0, 0x5000])               # camera handle
    for r in (23, 33, 43):
        for i in range(9):
            w[r + i] = rng.randrange(-50, 2000) & 0xffffffff
        w[r + 9] = rng.choice([0, 0x6000 + r])    # target handle
    for i in range(53, 58):
        w[i] = fbits(rng.uniform(0, 2))
    w[59] = fbits(rng.uniform(0, 1))
    w[60] = rng.randrange(2)
    for i in (61, 62, 63):
        w[i] = fbits(rng.uniform(0, 2))
    w[64], w[65] = rng.randrange(2), rng.randrange(2)
    w[82], w[83], w[84] = rng.randrange(2), rng.randrange(2), rng.randrange(2)
    return w


def random_ops(rng, count):
    ops = []
    for _ in range(count):
        if rng.random() < 0.35:
            a = rng.choice(GETTERS)
            ops.append((a, 0, 0))
            continue
        a = rng.choice(SETTERS)
        if a in FLOAT_SETTERS:
            v = fbits(rng.uniform(-0.2, 1.2))
        elif a == 0x40ce00:
            v = rng.randrange(1 << 10)
        elif a in (0x40d590, 0x40d670, 0x40d6b0):
            v = rng.choice([0, 0x5000, 0x7000])
        elif a == 0x40d810:
            v = rng.randrange(1, 12)
        elif a in TWO_ARGS or a == 0x40d780:
            v = rng.randrange(0, 1700)
        else:
            v = rng.randrange(-1, 4) & 0xffffffff
        ops.append((a, v, rng.randrange(0, 1300)))
    return ops


def make_trace(seed, world):
    rng = random.Random(seed * 7 + 1)
    fov = (fbits(rng.uniform(0.5, 1.6)), fbits(rng.uniform(0.5, 1.6)))
    script = {
        '0x00579ea0': lambda n, a: rng_for('3d', seed, n),
        '0x00579b70': lambda n, a: rng_for('cap', seed, n),
        '0x004f7fe0': lambda n, a: world['sun'],
        '@fovx': fov[0], '@fovy': fov[1],
        '[0x005f4c8c]': world['view'],
        '[[0x0060861c]+0x14]': world['mech'],
        '[0x0072f348]': world['video'],
        '[mech+0xd4]': world['weapons'],
    }
    t = Trace(script)
    t.silent |= {'@fovx', '@fovy', '[0x005f4c8c]', '[[0x0060861c]+0x14]', '[0x0072f348]',
                 '[mech+0xd4]', 'mech+0x630=', '[mech+0x39c]+0xa0=', '[mech+0x37c]+0x3f='}
    return t, fov


def rng_for(tag, seed, n):
    return random.Random(f'{tag}{seed}:{n}').randrange(2)


def original(seed, state, ops, world):
    t, fov = make_trace(seed, world)
    emu = Emu(t)
    lay = Layout(emu)
    lay.write(state)
    if world['pilot']:
        pilot = emu.alloc(0x100)
        emu.w32(PILOT, pilot)
        emu.w32(pilot + 0x14, world['mech'])
    if world['mech']:
        m = world['mech']
        emu.w32(m + 0xd4, world['weapons'])
        emu.w32(m + 0x39c, world['torso'])
        emu.w32(m + 0x37c, world['throttle'])
    emu.w32(VIEW, world['view'])
    emu.w32(VIDEO_MODE, world['video'])
    for addr, spec in STUBS.items():
        emu.stub(addr, spec=spec)

    def get_fov(e, key, a, pop):
        e.t.handle(key, a)
        esp = e.uc.reg_read(ctypes_esp())
        e.w32(e.args('d')[0], fov[0])
        e.w32(e.r32(esp + 4), fov[1])
        e._ret(0, pop)
    emu.stub(0x4f3ee0, spec='c', fn=get_fov)
    slot = emu.alloc(16)
    for a, v, v2 in ops:
        if a in FLOAT_GETTERS:
            here = emu.alloc(32)
            code = b'\xe8' + struct.pack('<i', a - (here + 5)) + b'\xd9\x1d' + struct.pack('<I', slot) + b'\xc3'
            emu.cpu.write(here, code)
            emu.call(here)
            t.handle('ret', [emu.r32(slot)])
        elif a in GETTERS:
            t.handle('ret', [emu.call(a)])
        elif a in FLOAT_SETTERS:
            emu.call(a, stack=(v,))
        else:
            emu.call(a, ecx=v, edx=v2)
    writes = {}
    if world['mech']:
        writes = {'invincible': emu.r8(world['mech'] + 0x630),
                  'torso': emu.r8(world['torso'] + 0xa0),
                  'throttle': emu.r8(world['throttle'] + 0x3f)}
    return t.log, lay.read(), writes


def ctypes_esp():
    from unicorn.x86_const import UC_X86_REG_ESP
    return UC_X86_REG_ESP


def rebuilt(probe, seed, state, ops, world):
    t, fov = make_trace(seed, world)
    writes = dict(world['mech_init'])
    names = {'mech+0x630=': 'invincible', '[mech+0x39c]+0xa0=': 'torso',
             '[mech+0x37c]+0x3f=': 'throttle'}
    for key, name in names.items():
        t.script[key] = (lambda nm: lambda n, a: writes.__setitem__(nm, a[1]))(name)
    probe.probe_app_set_callback(t.callback())
    n = probe.probe_options_words()
    buf = (ctypes.c_uint32 * n)(*state)
    probe.probe_options_io(buf, 1)
    for a, v, v2 in ops:
        r = probe.probe_options_op(a, v, v2)
        if a in GETTERS:
            t.handle('ret', [r])
    probe.probe_options_io(buf, 0)
    return t.log, list(buf), writes if world['mech'] else {}


def main():
    probe = load_probe()
    probe.probe_options_op.restype = ctypes.c_uint32
    n = probe.probe_options_words()
    calls = 0
    for seed in range(80):
        rng = random.Random(seed)
        state = random_state(rng, n)
        ops = random_ops(rng, rng.randrange(5, 40))
        world = {'pilot': rng.random() < 0.7, 'view': rng.choice([0, 0x3000]),
                 'video': rng.randrange(2), 'sun': rng.choice([0, 0x4000])}
        # Without a mech, then (when there is a pilot) with one; the fake mech
        # and its parts sit high in the emulator's heap, away from allocations.
        worlds = [dict(world, mech=0, weapons=0, torso=0, throttle=0, mech_init={})]
        if world['pilot']:
            worlds.append(dict(world, mech=0x10800000, weapons=0x10900000, torso=0x10a00000,
                               throttle=0x10b00000,
                               mech_init={'invincible': 0, 'torso': 0, 'throttle': 0}))
        for w in worlds:
            a = original(seed, state, ops, w)
            b = rebuilt(probe, seed, state, ops, w)
            if not compare(f'seed {seed} mech={w["mech"]:#x}', a[0], b[0], ulps=ULPS):
                raise SystemExit(1)
            if a[1] != b[1]:
                diff = [(i, hex(x), hex(y)) for i, (x, y) in enumerate(zip(a[1], b[1])) if x != y]
                print(f'seed {seed}: option words differ (index, original, rebuild): {diff}')
                raise SystemExit(1)
            if a[2] != b[2]:
                print(f'seed {seed}: mech writes differ: {a[2]} vs {b[2]}')
                raise SystemExit(1)
            calls += len(a[0])
    print(f'{calls} calls over 80 option states: the rebuilt options match the original')


if __name__ == '__main__':
    main()
