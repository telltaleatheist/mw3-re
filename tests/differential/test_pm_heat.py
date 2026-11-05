"""PM's heat update 0x41a100 against the exe, frame by frame, with the drowning under 25 m (0x415e90, 0x415f10, 0x41de00), the torso's end 0x41de30 (docs/heat.md).

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_heat.py

A mech in emulator memory uses a copy of the real vtable, legs and feet; the calls heat makes into other code are
stubbed and logged on both sides. Random scenarios drive heat up and down through every threshold. After each frame the
mech's fields and the call log must match (floats within 1e-4). The probe library has play fixes off (zfix).
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import pm_floors  # noqa: E402
import test_pm_zar  # noqa: E402
from original import FTOL, IAT_FTOL  # noqa: E402

probe = test_pm_zar.load_pm_probe()

HEAT, MECH_VTABLE, MECH_SIZE = 0x41a100, 0x5dfaac, 0x800
IAT_RAND = 0x5bb668

# (name, kind, place): kind f float, b byte, i int; place is a mech offset, a
# foot (0/1, offset), a global address, or a scripted value. Order = probe.cpp.
FIELDS = [
    ('height', 'f', 0x68), ('heat', 'f', 0x578), ('heat_added', 'f', 0x57c),
    ('heat_dissipation', 'f', 0x580), ('masc_time', 'f', 0x598), ('shutdown_time', 'f', 0x6c4),
    ('beep_time', 'f', 0x6c8), ('voice_time', 'f', 0x6cc), ('surface1', 'f', 's160'), ('surface4', 'f', 's16c'),
    ('pos_y', 'f', 'pos'), ('time', 'f', 'g62da90'), ('dt', 'f', 'g8c1b4c'),
    ('controls', 'f', 'script'), ('health', 'f', 'script'),
    ('power_down_pending', 'b', 0xe0), ('heat_shutdown', 'b', 0xe1), ('powered_down', 'b', 0xe2),
    ('invincible', 'b', 0x654), ('doubles', 'b', 0x684), ('active', 'b', 8), ('remote', 'b', 'script'),
    ('salvage_mech', 'b', 0x5ae), ('full_all', 'b', 'g604124'), ('no_haze', 'b', 'g62cc5a'),
    ('jump_jets', 'i', 0x5a4), ('sinks', 'i', 0x680), ('heat_state', 'i', 0x6c0),
    ('soil0', 'i', (0, 0xe8)), ('soil1', 'i', (1, 0xe8)), ('beep', 'i', 0x5f0),
    ('alarm', 'i', 0x5f4), ('warn', 'i', 0x62c), ('voice', 'i', 0x630), ('player', 'i', 'player'),
    ('mission', 'i', 'script'), ('option', 'i', 'script'), ('screen', 'i', 'script'),
    ('ammo', 'i', 'script'), ('state', 'i', 0x59c), ('attacker', 'i', 0x688),
]
NAMES = [f[0] for f in FIELDS]
INPUTS = {'controls', 'mission', 'option', 'screen', 'ammo', 'player', 'full_all', 'no_haze',
          'time', 'dt', 'height', 'surface1', 'surface4', 'pos_y', 'health', 'remote', 'active', 'state', 'soil0', 'soil1',
          'attacker', 'salvage_mech',
          'jump_jets', 'masc_time', 'sinks', 'doubles', 'invincible', 'powered_down',
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


class Emulated(test_pm_zar.Emulated):
    def __init__(self):
        super().__init__()
        cpu = self.cpu
        a = self.alloc
        cpu.import_code(IAT_FTOL, FTOL)
        self.mech, self.skis, self.posb = a(MECH_SIZE), a(0x340), a(12)
        self.feet = [a(0xf0), a(0xf0)]
        self.parts = {}
        self.k = a(0x40)  # scripted values the little routines read
        def code(fn, pop=0):
            f = cpu.code(b'\xc3')
            self.native_stub(f, fn, pop)
            return f
        for off, name, n in ((0x3ac, 1, 1), (0x3b4, 2, 2), (0x39c, 3, 3), (0x3a0, 4, 4), (0x3a4, 5, 5), (0x3a8, 6, 6)):
            part, vt = a(0x100), a(0x60)
            self.w32(part, vt)
            self.parts[off] = part
            self.w32(vt + 0x3c, code(lambda c, n=n: self.log(9, c.arg(0), 16 * n + c.arg(1)), 8))
            self.w32(vt + 0x40, code(lambda c, n=n: self.log(10, c.arg(0), 16 * n + c.arg(1)), 8))
            if n == 1:
                self.w32(vt + 0x20, code(lambda c: self.log(6, c.arg(0), c.arg(1)), 8))
        for i in (0, 1):
            self.w32(self.parts[0x39c + 4 * i] + 0x78, self.feet[i])
        # the controls: a virtual-base object whose vtable +0x1c returns k+0 as float
        ctl, vbt, vt = a(0x40), a(8), a(0x40)
        self.w32(ctl + 4, vbt)
        self.w32(vbt + 4, 0x10)
        self.w32(ctl + 0x14, vt)
        self.w32(vt + 0x1c, cpu.code(b'\xd9\x05' + struct.pack('<I', self.k) + b'\xc3'))
        self.ctl = ctl
        # the mission: [0x62c40c] -> +0x6c -> vtable +0x28 returns k+4; 0x47b710 reads the same
        mis, sub_, vt2 = a(0x80), a(8), a(0x40)
        self.w32(0x62c40c, mis)
        self.w32(mis + 0x6c, sub_)
        self.w32(sub_, vt2)
        self.w32(vt2 + 0x28, cpu.code(b'\xa1' + struct.pack('<I', self.k + 4) + b'\xc3'))
        self.w32(0x6187c4, self.k + 8)   # the heat option: flags word, bit 3
        self.pl = a(0x80)
        self.w32(0x61874c, self.pl)
        self.mvt = a(0x200)
        self.wr(self.mvt, self.raw(MECH_VTABLE, 0x200))
        self.w32(self.mvt + 100, code(lambda c: int(self.script['remote'])))
        self.w32(self.mvt + 0xa4, cpu.code(b'\xd9\x05' + struct.pack('<I', self.k + 0xc) + b'\xc3'))
        self.pilot, self.wmgr = a(0x40), a(0x40)
        self.screen_obj = a(0x10)
        self.native_stub(0x5cb260, lambda c: self.posb, 0)
        self.native_stub(0x430e30, lambda c: self.screen_obj if self.script['screen'] else 0)
        self.native_stub(0x43d2b0, self.haze, 0xc)
        self.native_stub(0x432d10, self.play, 0x14)
        self.native_stub(0x58fe70, lambda c: self.log(3, c.arg(0), 0))
        self.native_stub(0x41f310, lambda c: self.log(4, c.arg(0), 0))
        self.native_stub(0x41dab0, lambda c: self.log(5, c.arg(0), 0), 4)
        self.native_stub(0x4b4cd0, self.ammo, 8)
        self.native_stub(0x4107a0, lambda c: self.log(8, c.arg(0), c.arg(1)))
        cpu.stub_import(IAT_RAND, lambda c: self.rand())
        self.cpu.stubs[0x406cb0] = (lambda c: 0, 0)

    def raw(self, a, n):
        return bytes(self.cpu._uc.mem_read(a, n))

    def wr(self, a, b):
        self.cpu._uc.mem_write(a, bytes(b))

    def w32(self, a, v):
        self.wr(a, struct.pack('<I', v & 0xffffffff))

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
        self.wr(self.mech, bytes(MECH_SIZE))
        self.w32(self.mech, self.mvt)
        self.w32(self.mech + 0x38c, self.ctl)
        self.w32(self.mech + 0x42c, self.skis)
        self.w32(self.mech + 0x6a8, self.pilot)
        self.w32(self.mech + 0xd4, self.wmgr)
        for off, part in self.parts.items():
            self.w32(self.mech + off, part)
        for f in self.feet:
            self.wr(f, bytes(0xf0))
        self.rand = MsvcRand(seed)
        self.script = {'controls': 0.0, 'health': 1.0, 'remote': 0, 'mission': 0, 'option': 1, 'screen': 0, 'ammo': 0}
        self.next_sound = 1

    def where(self, place):
        if isinstance(place, int):
            return self.mech + place
        if isinstance(place, tuple):
            return self.feet[place[0]] + place[1]
        if place == 'pos':
            return self.posb + 4
        if place[0] == 'g':
            return int(place[1:], 16)
        if place[0] == 's':
            return self.skis + int(place[1:], 16)
        return None

    def get(self):
        out = []
        for name, kind, place in FIELDS:
            if place == 'script':
                v = self.script[name]
                out.append(f2i(v) if kind == 'f' else int(v))
            elif place == 'player':
                out.append(int(self.r32(self.pl + 0x64) == self.mech))
            else:
                raw = self.raw(self.where(place), 1 if kind == 'b' else 4)
                v = raw[0] if kind == 'b' else struct.unpack('<i', raw)[0]
                out.append(int(v != 0) if name == 'attacker' else v)
        return out

    def set(self, values):
        for (name, kind, place), v in zip(FIELDS, values):
            if place == 'script':
                self.script[name] = i2f(v) if kind == 'f' else v
            elif place == 'player':
                self.w32(self.pl + 0x64, self.mech if v else 0)
            elif name == 'attacker':
                self.w32(self.mech + place, self.mech if v else 0)
            elif kind == 'b':
                self.wr(self.where(place), bytes([v & 0xff]))
            else:
                self.w32(self.where(place), v)
        self.wr(self.k, struct.pack('<f', self.script['controls']))
        self.w32(self.k + 4, self.script['mission'])
        self.w32(self.k + 8, 8 if self.script['option'] else 0)
        self.wr(self.k + 0xc, struct.pack('<f', self.script['health']))

    def step(self):
        self.events = []
        dt = self.r32(0x8c1b4c)
        self.call(HEAT, ecx=self.mech, args=(dt,))
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
        if not close(a[i + 1], b[i + 1], 'f' if a[i] in (1, 8, 9, 10) else 'i'):
            return False
    return True


def scenario(rng):
    """A random mech situation: inputs that change now and then over the run."""
    s = {n: 0 for n in NAMES}
    s.update(surface1=-300.0, surface4=-300.0, pos_y=rng.uniform(0, 40), health=1.0, active=1, salvage_mech=1,
             height=rng.choice([8.0, 12.0, 15.5]), sinks=rng.choice([0, 1, 3, 4, 7, 8, 10, 14, 22]),
             doubles=rng.random() < 0.4, player=rng.random() < 0.6, full_all=rng.random() < 0.85,
             option=rng.random() < 0.9, mission=rng.choice([0, 0, 0, 3]),
             invincible=rng.random() < 0.15, dt=rng.choice([1 / 30, 1 / 60, 0.1]))
    return s


def tweak(s, rng, frame):
    s['time'] = s.get('_t', 0.0)
    s['_t'] = s['time'] + s['dt']
    if rng.random() < 0.08:
        s['heat_added'] = rng.choice([0.0, 0.05, 0.2, 0.5, 1.0])
    else:
        s['heat_added'] = 0.0
    if rng.random() < 0.03:
        d = rng.choice([None, None, -5.0, 0.5, 3.0, 9.0, 20.0, 24.9, 26.0, 40.0])
        s['surface1'] = -300.0 if d is None else s['pos_y'] + d
        s['surface4'] = -300.0 if d is None else s['surface1'] + rng.choice([0.0, 0.0, 2.0])
    s['attacker'] = rng.random() < 0.5
    if rng.random() < 0.05:
        s['salvage_mech'] = 1
    if rng.random() < 0.05:
        s['health'] = rng.choice([1.0, 0.8, 0.5, 0.3, 0.26, 0.2, 0.1])
    if rng.random() < 0.01:
        s['remote'] = rng.random() < 0.1
    if rng.random() < 0.01:
        s['state'] = rng.choice([0, 0, 10])
        s['active'] = int(rng.random() > 0.1)
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
            s['salvage_mech'] = s['salvage_mech'] and want[NAMES.index('salvage_mech')] or frame == 0
            for i, (name, kind, _) in enumerate(FIELDS):
                if name in INPUTS:
                    v = s[name]
                    want[i] = f2i(float(v)) if kind == 'f' else int(v)
            if rng.random() < 0.01:
                # The shutdown override, set by the player's command (0x419190).
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
    want_kinds = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}
    if not want_keys <= keys or not want_kinds <= kinds:
        raise SystemExit(f'coverage: shutdown keys {sorted(map(hex, keys))}, calls {sorted(kinds)}')
    pm_floors.floors(scenarios=(scenarios, 300), calls=(events, 140000))
    print(f'ok: {scenarios} scenarios x {frames} frames agree; {events} calls out matched; '
          f'{len(keys)} shutdown states and all {len(kinds)} kinds of call reached')


if __name__ == '__main__':
    main()
