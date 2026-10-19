"""The ported heat update (0x4192f0) against the original x86, frame by frame.

    cmake --build build && .venv/bin/python tests/differential/test_heat.py

A fake mech in emulator memory uses the real vtable (0x5bcef8), legs and feet;
the calls heat makes into unported code are stubbed and logged on both sides.
Random scenarios drive heat up and down through every threshold. After each
frame the mech's fields and the call log must match (floats within 1e-4).
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import FTOL, IAT_FTOL, ROOT, Original  # noqa: E402
from unicorn.x86_const import UC_X86_REG_ECX  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))

HEAT, MECH_VTABLE, MECH_SIZE = 0x4192f0, 0x5bcef8, 0x6d8
IAT_RAND = 0x5bb668

# (name, kind, place): kind f float, b byte, i int; place is a mech offset, a
# foot (0/1, offset), a global address, or a scripted value. Order = probe.cpp.
FIELDS = [
    ('height', 'f', 0x68), ('heat', 'f', 0x554), ('heat_added', 'f', 0x558),
    ('heat_dissipation', 'f', 0x55c), ('masc_time', 'f', 0x574), ('shutdown_time', 'f', 0x6a0),
    ('beep_time', 'f', 0x6a4), ('voice_time', 'f', 0x6a8), ('depth0', 'f', (0, 0xdc)),
    ('depth1', 'f', (1, 0xdc)), ('time', 'f', 'g60991c'), ('dt', 'f', 'g72c52c'),
    ('controls', 'f', 'script'),
    ('power_down_pending', 'b', 0xdc), ('heat_shutdown', 'b', 0xdd), ('powered_down', 'b', 0xde),
    ('heat_death_disabled', 'b', 0x630), ('doubles', 'b', 0x660), ('in_water0', 'b', (0, 0xac)),
    ('in_water1', 'b', (1, 0xac)), ('full_all', 'b', 'g5e1fd0'), ('no_haze', 'b', 'g608e4d'),
    ('jump_jets', 'i', 0x580), ('sinks', 'i', 0x65c), ('heat_state', 'i', 0x69c),
    ('soil0', 'i', (0, 0xe8)), ('soil1', 'i', (1, 0xe8)), ('beep', 'i', 0x5cc),
    ('alarm', 'i', 0x5d0), ('warn', 'i', 0x608), ('voice', 'i', 0x60c), ('player', 'i', 'player'),
    ('mission', 'i', 'script'), ('option', 'i', 'script'), ('screen', 'i', 'script'),
    ('ammo', 'i', 'script'),
]
NAMES = [f[0] for f in FIELDS]
INPUTS = {'controls', 'mission', 'option', 'screen', 'ammo', 'player', 'full_all', 'no_haze',
          'time', 'dt', 'height', 'depth0', 'depth1', 'in_water0', 'in_water1', 'soil0', 'soil1',
          'jump_jets', 'masc_time', 'sinks', 'doubles', 'heat_death_disabled', 'powered_down',
          'power_down_pending', 'heat_added'}


def f2i(x):
    return struct.unpack('<i', struct.pack('<f', x))[0]


def i2f(i):
    return struct.unpack('<f', struct.pack('<i', i))[0]


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
        self.mech = cpu.alloc(MECH_SIZE)
        self.feet = [cpu.alloc(0xec), cpu.alloc(0xec)]
        legs = self.legs = [cpu.alloc(0xd4), cpu.alloc(0xd4)]
        for i in range(2):
            cpu.u32(legs[i] + 0x78, self.feet[i])
            cpu.u32(self.mech + 0x38c + 4 * i, legs[i])
        self.k = cpu.alloc(0x40)  # scripted values the little routines read
        # controls: a virtual-base object whose vtable +0x1c returns k+0 as float
        ctl, vbt, vt = cpu.alloc(0x40), cpu.alloc(8), cpu.alloc(0x40)
        cpu.u32(ctl + 4, vbt)
        cpu.u32(vbt + 4, 0x10)
        cpu.u32(ctl + 0x14, vt)
        cpu.u32(vt + 0x1c, cpu.code(b'\xd9\x05' + struct.pack('<I', self.k) + b'\xc3'))
        cpu.u32(self.mech + 0x37c, ctl)
        # mission: [0x60861c] -> +0x6c -> vtable +0x2c returns k+4
        mis, sub, vt2 = cpu.alloc(0x80), cpu.alloc(8), cpu.alloc(0x40)
        cpu.u32(0x60861c, mis)
        cpu.u32(mis + 0x6c, sub)
        cpu.u32(sub, vt2)
        cpu.u32(vt2 + 0x2c, cpu.code(b'\xa1' + struct.pack('<I', self.k + 4) + b'\xc3'))
        # heat option: [0x5f4d04] -> flags word, bit 3
        cpu.u32(0x5f4d04, self.k + 8)
        # player: [0x5f4c8c] + 0x64
        self.pl = cpu.alloc(0x80)
        cpu.u32(0x5f4c8c, self.pl)
        # torso: [mech + 0x39c] with vtable +0x20 logged
        torso, vt3 = cpu.alloc(0x40), cpu.alloc(0x40)
        cpu.u32(torso, vt3)
        cpu.u32(self.mech + 0x39c, torso)
        hit = cpu.code(b'\xc2\x08\x00')
        cpu.u32(vt3 + 0x20, hit)
        cpu.stub_function(hit, lambda c: self.log(6, c.arg(0), c.arg(1)), 8)
        cpu.u32(self.mech + 0x684, cpu.alloc(0x40))
        cpu.u32(self.mech + 0xd4, cpu.alloc(0x40))
        cpu.stub_function(0x430aa0, lambda c: self.screen_obj if self.script['screen'] else 0)
        self.screen_obj = cpu.alloc(0x10)
        cpu.stub_function(0x43c1f0, self.haze, 0xc)
        cpu.stub_function(0x432990, self.play, 0x14)
        cpu.stub_function(0x56e9f0, lambda c: self.log(3, c.uc.reg_read(UC_X86_REG_ECX), 0))
        cpu.stub_function(0x41e770, lambda c: self.log(4, c.uc.reg_read(UC_X86_REG_ECX), 0), 8)
        cpu.stub_function(0x41cc30, lambda c: self.log(5, c.arg(0), 0), 4)
        cpu.stub_function(0x4b0de0, self.ammo, 8)
        cpu.stub_function(0x40fe70, lambda c: self.log(8, c.arg(0), c.arg(1)))
        cpu.stub_import(IAT_RAND, lambda c: self.rand())

    def log(self, *e):
        self.events.extend(e)
        return 0

    def haze(self, c):
        self.log(1, c.arg(0), c.arg(1))
        self.log(1, c.arg(2), 0)

    def play(self, c):
        h = self.next_sound
        self.next_sound += 1
        self.log(2, c.arg(0), h)
        return h

    def ammo(self, c):
        self.log(7, self.script['ammo'], 0)
        return self.script['ammo']

    def reset(self, seed):
        cpu = self.cpu
        keep = {o: cpu.read(self.mech + o, 4) for o in (0x37c, 0x38c, 0x390, 0x39c, 0x684, 0xd4)}
        cpu.write(self.mech, bytes(MECH_SIZE))
        cpu.u32(self.mech, MECH_VTABLE)
        for o, v in keep.items():
            cpu.write(self.mech + o, v)
        for f in self.feet:
            cpu.write(f, bytes(0xec))
        self.rand = MsvcRand(seed)
        self.script = {'controls': 0.0, 'mission': 0, 'option': 1, 'screen': 0, 'ammo': 0}
        self.next_sound = 1

    def where(self, place):
        if isinstance(place, int):
            return self.mech + place
        if isinstance(place, tuple):
            return self.feet[place[0]] + place[1]
        if isinstance(place, str) and place.startswith('g'):
            return int(place[1:], 16)
        return None

    def get(self):
        out = []
        for name, kind, place in FIELDS:
            if place == 'script':
                v = self.script[name]
                out.append(f2i(v) if kind == 'f' else int(v))
            elif place == 'player':
                out.append(int(struct.unpack('<I', self.cpu.read(self.pl + 0x64, 4))[0] == self.mech))
            else:
                a = self.where(place)
                raw = self.cpu.read(a, 1 if kind == 'b' else 4)
                out.append(raw[0] if kind == 'b' else struct.unpack('<i', raw)[0])
        return out

    def set(self, values):
        for (name, kind, place), v in zip(FIELDS, values):
            if place == 'script':
                self.script[name] = i2f(v) if kind == 'f' else v
            elif place == 'player':
                self.cpu.u32(self.pl + 0x64, self.mech if v else 0)
            else:
                a = self.where(place)
                if kind == 'b':
                    self.cpu.write(a, bytes([v & 0xff]))
                else:
                    self.cpu.u32(a, v)
        cpu = self.cpu
        cpu.write(self.k, struct.pack('<f', self.script['controls']))
        cpu.u32(self.k + 4, self.script['mission'])
        cpu.u32(self.k + 8, 8 if self.script['option'] else 0)

    def step(self):
        self.events = []
        dt = struct.unpack('<I', self.cpu.read(0x72c52c, 4))[0]
        self.cpu.call(HEAT, ecx=self.mech, stack_args=(dt,))
        return self.events


def port_get():
    v = (ctypes.c_int32 * len(FIELDS))()
    probe.probe_heat_io(v, 0)
    return list(v)


def port_set(values):
    probe.probe_heat_io((ctypes.c_int32 * len(FIELDS))(*values), 1)


def port_step():
    buf = (ctypes.c_int32 * 4096)()
    n = probe.probe_heat_step(buf, 4096)
    return list(buf[:n])


def close(a, b, kind):
    if kind != 'f':
        return a == b
    x, y = i2f(a), i2f(b)
    return x == y or abs(x - y) <= 1e-4 * max(1.0, abs(x), abs(y))


def same_calls(a, b):
    """Call logs are triples (kind, x, y); x is a float for kinds 1 and 8."""
    if len(a) != len(b):
        return False
    for i in range(0, len(a), 3):
        if a[i] != b[i] or a[i + 2] != b[i + 2]:
            return False
        if not close(a[i + 1], b[i + 1], 'f' if a[i] in (1, 8) else 'i'):
            return False
    return True


def scenario(rng):
    """A random mech situation: inputs that change now and then over the run."""
    s = {n: 0 for n in NAMES}
    s.update(height=rng.choice([8.0, 12.0, 15.5]), sinks=rng.choice([10, 14, 22]),
             doubles=rng.random() < 0.4, player=rng.random() < 0.6, full_all=rng.random() < 0.85,
             option=rng.random() < 0.9, mission=rng.choice([0, 0, 0, 3]),
             heat_death_disabled=rng.random() < 0.15, dt=rng.choice([1 / 30, 1 / 60, 0.1]))
    return s


def tweak(s, rng, frame):
    s['time'] = s.get('_t', 0.0)
    s['_t'] = s['time'] + s['dt']
    if rng.random() < 0.08:
        s['heat_added'] = rng.choice([0.0, 0.05, 0.2, 0.5, 1.0])
    else:
        s['heat_added'] = 0.0
    if rng.random() < 0.03:
        s['in_water0'] = rng.random() < 0.5
        s['depth0'] = rng.choice([0.5, 3.0, 9.0, 20.0])
    if rng.random() < 0.03:
        s['soil0'], s['soil1'] = rng.choice([0, 8]), rng.choice([0, 3, 8])
    if rng.random() < 0.05:
        s['jump_jets'] = rng.random() < 0.3
        s['masc_time'] = rng.choice([0.0, 0.0, 0.001, 2.5])
    if rng.random() < 0.05:
        s['controls'] = rng.uniform(-1.0, 1.0)
    if rng.random() < 0.03:
        s['powered_down'] = rng.random() < 0.3
        s['power_down_pending'] = rng.random() < 0.1
    s['screen'] = rng.random() < 0.9
    s['ammo'] = rng.random() < 0.5


def main():
    emu = Emulated()
    rng = random.Random(1999)
    scenarios, frames, events = 300, 400, 0
    keys, kinds = set(), set()
    for n in range(scenarios):
        seed = rng.randrange(1 << 31)
        emu.reset(seed)
        probe.probe_heat_reset(seed)
        s = scenario(rng)
        for frame in range(frames):
            tweak(s, rng, frame)
            want = emu.get()
            for i, (name, kind, _) in enumerate(FIELDS):
                if name in INPUTS:
                    v = s[name]
                    want[i] = f2i(float(v)) if kind == 'f' else int(v)
            if rng.random() < 0.01:
                # The overheated flag is set elsewhere in the game (0x419190).
                want[NAMES.index('heat_state')] |= 0x1000000
            emu.set(want)
            port_set(want)
            a, b = emu.step(), port_step()
            ga, gb = emu.get(), port_get()
            bad = [FIELDS[i][0] for i in range(len(FIELDS)) if not close(ga[i], gb[i], FIELDS[i][1])]
            if bad or not same_calls(a, b):
                detail = {k: (i2f(ga[i]) if FIELDS[i][1] == 'f' else ga[i],
                              i2f(gb[i]) if FIELDS[i][1] == 'f' else gb[i])
                          for i, k in enumerate(NAMES) if k in bad}
                raise SystemExit(f'scenario {n} frame {frame}: fields {detail}\n'
                                 f'  original calls {a}\n  port calls     {b}')
            events += len(a) // 3
            st = ga[NAMES.index('heat_state')] & 0xffffffff
            keys.add((st & 0x7f000000) | ((st >> 3) & 7))
            kinds.update(a[0::3])
    want_keys = {0, 1, 3, 4, 0x1000000, 0x1000001, 0x1000003, 0x2000000, 0x2000003}
    want_kinds = {1, 2, 3, 4, 5, 6, 7, 8}
    if not want_keys <= keys or not want_kinds <= kinds:
        raise SystemExit(f'coverage: shutdown keys {sorted(map(hex, keys))}, calls {sorted(kinds)}')
    print(f'ok: {scenarios} scenarios x {frames} frames agree; {events} calls out matched; '
          f'{len(keys)} shutdown states and all {len(kinds)} kinds of call reached')


if __name__ == '__main__':
    main()
