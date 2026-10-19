"""The ported world parameters reader (mech3/mission_world, 0x491de0) against
the original x86 on every mission's world.zrd and both mpinfo.zrd files.

    cmake --build build && .venv/bin/python tests/differential/test_world_params.py

The original runs in Unicorn with zReader's load (0x556740) served from the
same archive bytes and read by the original zRdrReadNode. A small scene is
built in both: list 6 with a decoy, three skies under a horizon, the
`sunlight` light, a world and a camera; the original's list find (0x4f7fe0),
node SetActive (0x4eb170), world fog setters and getters (0x4f23e0..0x4f24e0)
and colour packing (0x579370) run for real, as the port's do. Everything not
ported (wind, light, puffer, cloud pattern, weather objects, the software
colour, the game type and mission number, the HWCardFlag option) is stubbed
in both and recorded. The trace of those calls and the state after (the
mission's +0x08..+0x4c, the three globals, each weather object, the world's
fog fields and every node's flags) must be identical.

Each mission block runs on hardware with the sun option on and in software
with it off; the multiplayer maps also as multiplayer games, maps 3 and 4
reading mpinfo.zrd. Crafted files cover missing keys and blocks, odd values
(temperature 0, -5, NaN; unknown weather types), a missing file and key, a
second read, and scenes without the sun, the world, the view or the horizon.
"""
import ctypes
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import ROOT  # noqa: E402
from test_zreader import Emulated as ReaderEmu, entries, probe  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDX  # noqa: E402

sys.path.insert(0, str(ROOT / 'tools' / 'analysis'))
import zrd  # noqa: E402

probe.probe_world.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_void_p, ctypes.c_int,
                              ctypes.c_void_p, ctypes.c_int]
GAME = ROOT / 'game' / 'mw3-1.2' / 'zbd'
CRAFTED = ROOT / 'build' / 'world_params_test.zbd'

READ, CTOR, SET_FORMAT = 0x491de0, 0x491d70, 0x5792c0
LOAD, FREE_NODE, NEW, ZERR = 0x556740, 0x556820, 0x5abd26, 0x49bfd0
PILOT, HW, WORLD_OBJ, VIEW, HWCARD = 0x60861c, 0x72f348, 0x608c84, 0x5f4c8c, 0x40d4d0
LIST_SLOTS, ENV, MANAGER = 0x5ea6d0, 0x5e5d3c, 0x736328
NAMES = ['decoy', 'horscroll', 'hormain', 'horblack', 'horizon', 'sunlight', 'world1', 'camera1']
DECOY, SCROLL, MAIN, BLACK, HORIZON, SUN, WORLD, CAMERA = range(8)
(HARDWARE, GAME_TYPE, MISSION, HW_CARD, HAS_WORLD, HAS_VIEW, HAS_SUN, HAS_HORIZON, NO_MEMORY,
 ROT_X, ROT_Y, ROT_Z, DIFFUSE, AMBIENT) = range(14)


def fbits(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


def config(**kw):
    c = [0] * 16
    c[HARDWARE], c[GAME_TYPE], c[MISSION], c[HW_CARD] = 1, 1, 1, 1
    c[HAS_WORLD] = c[HAS_VIEW] = c[HAS_SUN] = c[HAS_HORIZON] = 1
    c[ROT_X], c[ROT_Y], c[ROT_Z] = fbits(0.25), fbits(-0.5), fbits(0.75)
    c[DIFFUSE], c[AMBIENT] = fbits(0.9), fbits(0.15)
    for k, v in kw.items():
        c[globals()[k.upper()]] = v
    return c


class Emulated(ReaderEmu):
    def __init__(self, archive, cfg):
        super().__init__(archive)
        self.cfg, self.out, self.weathers = cfg, [], []
        self.files = {n.lower(): s for n, s, _ in entries(archive)}
        cpu = self.cpu
        cpu.call(SET_FORMAT, ecx=5, edx=6, stack_args=(5, 0, 0, 0))
        f = self.f32

        def code(fn, pop=0):
            a = cpu.code(b'\xc3')
            cpu.stub_function(a, fn, pop)
            return a

        stubs = {
            LOAD: (self.load_stub, 4), FREE_NODE: (lambda c: 0, 0), NEW: (self.new, 0), ZERR: (self.zerr, 0),
            HWCARD: (lambda c: self.log('hw_card_flag') or cfg[HW_CARD], 0),
            0x549410: (lambda c: self.log('puffer'), 0),
            0x54b630: (lambda c: self.log('wind_vel ' + ' '.join(f'{self.u32(c.ecx + 4 * i):08x}' for i in range(3))), 0),
            0x54b650: (lambda c: self.log(f'wind_max {c.arg(0):08x}'), 4),
            0x54b660: (lambda c: self.log(f'wind_accel {c.arg(0):08x}'), 4),
            0x54b670: (lambda c: self.log(f'wind_angvel {c.arg(0):08x}'), 4),
            0x4fb060: (self.light_get, 8),
            0x4faff0: (lambda c: self.log(f'light_rot {self.name(c.ecx)} {c.arg(0):08x} {c.arg(1):08x} {c.arg(2):08x}'), 0xc),
            0x4fa940: (lambda c: self.log(f'light_amb {self.name(c.ecx)} {c.arg(0):08x}'), 4),
            0x4fa9b0: (lambda c: self.log(f'light_dif {self.name(c.ecx)} {c.arg(0):08x}'), 4),
            0x5a5ad0: (lambda c: self.ctor('snow', c), 4), 0x5a60a0: (lambda c: self.ctor('rain', c), 4),
            0x59fbe0: (lambda c: self.manager(c, 'weather_add'), 4),
            0x543910: (lambda c: self.log(f'fl_on {c.ecx}'), 0),
            0x543920: (lambda c: self.log(f'fl_period {c.arg(0):08x}'), 4),
            0x543940: (lambda c: self.log(f'fl_wave {c.arg(0):08x}'), 4),
            0x543960: (lambda c: self.log(f'fl_baseamp {c.arg(0):08x} {c.arg(1):08x}'), 8),
            0x5439f0: (lambda c: self.log(f'fl_heading {c.arg(0):08x}'), 4),
            0x566250: (lambda c: self.log('swcolor ' + ' '.join(f'{self.u32(c.ecx + 4 * i):08x}' for i in range(3))), 0),
        }
        for addr, (fn, pop) in stubs.items():
            cpu.stub_function(addr, fn, pop)
        self.vtable = cpu.alloc(0x68)
        cpu.u32(self.vtable + 0x60, code(lambda c: self.log(f'weather_start {self.weather(c.ecx)} {c.arg(0):08x}'), 4))
        cpu.u32(self.vtable + 0x64, code(lambda c: self.log(f'weather_stop {self.weather(c.ecx)}')))
        game_vt = cpu.alloc(0x50)
        cpu.u32(game_vt + 0x2c, code(lambda c: self.log('game_type') or cfg[GAME_TYPE]))
        cpu.u32(game_vt + 0x4c, code(lambda c: self.log('mission_number') or cfg[MISSION]))
        game, pilot = cpu.alloc(0x10), cpu.alloc(0x80)
        cpu.u32(game, game_vt)
        cpu.u32(pilot + 0x6c, game)
        cpu.u32(PILOT, pilot)
        cpu.u32(HW, cfg[HARDWARE])
        flag = cpu.alloc(4)
        cpu.u32(flag, cfg[HW_CARD])
        cpu.u32(0x5f4ccc, flag)
        self.build_scene()
        for i, v in enumerate((1.5, 2.5, 3.5)):
            cpu.u32(ENV + 4 * i, fbits(v))

    def f32(self, u):
        return struct.unpack('<f', struct.pack('<I', u))[0]

    def log(self, s):
        self.out.append(s)
        return 0

    def build_scene(self):
        cpu, cfg = self.cpu, self.cfg
        self.nodes = [cpu.alloc(0xd4) for _ in NAMES]
        for i, n in enumerate(self.nodes):
            cpu.write(n, bytes(0xd4))
            cpu.write(n, NAMES[i].encode() + b'\0')
            cpu.u32(n + 0x34, 9 if i == SUN else 13 if i == WORLD else 8 if i == CAMERA else 5)
            cpu.u32(n + 0x24, 4 if i in (SCROLL, SUN) else 0x10)
        light, world, camera = cpu.alloc(0xd0), cpu.alloc(0xbc), cpu.alloc(0x1e8)
        cpu.write(light, bytes(0xd0))
        cpu.u32(light + 0x9c, cfg[DIFFUSE])
        cpu.u32(light + 0xa0, cfg[AMBIENT])
        cpu.u32(self.nodes[SUN] + 0x38, light)
        cpu.write(world, bytes(0xbc))
        for o in range(4, 0x34, 4):
            cpu.u32(world + o, 0x3f800000 + o * 0x1000)
        self.world = world
        cpu.u32(self.nodes[WORLD] + 0x38, world)
        cpu.write(camera, bytes(0x1e8))
        cpu.u32(camera + 8, self.nodes[HORIZON] if cfg[HAS_HORIZON] else 0)
        cpu.u32(self.nodes[CAMERA] + 0x38, camera)
        kids = cpu.alloc(12)
        cpu.write(kids, struct.pack('<III', self.nodes[SCROLL], self.nodes[MAIN], self.nodes[BLACK]))
        cpu.u32(self.nodes[HORIZON] + 0x5c, 3)
        cpu.u32(self.nodes[HORIZON] + 0x60, kids)
        # List 6, items {node, prev, next, removable}.
        listed = [n for i, n in enumerate(self.nodes) if i != SUN or cfg[HAS_SUN]]
        items = [cpu.alloc(0x10) for _ in listed]
        for k, (item, n) in enumerate(zip(items, listed)):
            nxt = items[k + 1] if k + 1 < len(items) else 0
            prv = items[k - 1] if k else 0
            cpu.write(item, struct.pack('<IIII', n, prv, nxt, 1))
        head = cpu.alloc(12)
        cpu.write(head, struct.pack('<III', items[0], items[-1], 0))
        cpu.u32(LIST_SLOTS + 4 * 6, head)
        for addr, on, node in ((WORLD_OBJ, cfg[HAS_WORLD], WORLD), (VIEW, cfg[HAS_VIEW], CAMERA)):
            obj = cpu.alloc(8)
            cpu.u32(obj + 4, self.nodes[node])
            cpu.u32(addr, obj if on else 0)

    def name(self, addr):
        return NAMES[self.nodes.index(addr)] if addr in self.nodes else ('0' if addr == 0 else '?')

    def weather(self, addr):
        return f'W{self.weathers.index(addr) + 1}' if addr in self.weathers else ('0' if addr == 0 else 'W?')

    def load_stub(self, c):
        name = self.cstr(c.ecx).decode('latin-1').split('\\')[-1].lower()
        return self.trees.get(name, 0)

    def read_trees(self):
        """Each file read up front by zRdrReadNode: a stub cannot run the emulator."""
        self.trees = {}
        for name, start in self.files.items():
            if name not in ('world.zrd', 'mpinfo.zrd'):
                continue
            self.pos = start
            root = self.cpu.alloc(8)
            self.cpu.call(0x557180, ecx=0x1234, edx=root)
            self.trees[name] = root

    def new(self, c):
        size = c.arg(0)
        if size == 0x98 and self.cfg[NO_MEMORY]:
            return 0
        p = c.alloc(size)
        c.write(p, bytes(size))
        return p

    def ctor(self, kind, c):
        self.log(f'{kind} {struct.unpack("<i", struct.pack("<I", c.arg(0)))[0]}')
        self.weathers.append(c.ecx)
        c.u32(c.ecx, self.vtable)
        return c.ecx

    def manager(self, c, what):
        assert c.ecx == MANAGER
        self.log(f'{what} {self.weather(c.arg(0))}')

    def light_get(self, c):
        self.log(f'light_get {self.name(c.ecx)}')
        c.u32(c.edx, self.cfg[ROT_X])
        c.u32(c.arg(0), self.cfg[ROT_Y])
        c.u32(c.arg(1), self.cfg[ROT_Z])

    def zerr(self, c):
        level, line, fmt = c.arg(0), c.arg(2), self.cstr(c.arg(3)).decode('latin-1')
        if '%s' in fmt:
            fmt = fmt.replace('%s', self.cstr(c.arg(4)).decode('latin-1'), 1)
        self.log(f'zerr {level:x} {line} {fmt}')

    def run(self, file, key, reads):
        cpu, u = self.cpu, self.u32
        self.read_trees()
        this = cpu.alloc(0x4c)
        for o in range(0, 0x4c, 4):
            cpu.u32(this + o, 0x40000000 + o * 0x1000)
        cpu.u32(this + 0x28, 0)
        cpu.call(CTOR, ecx=this)
        fp, kp = cpu.alloc(len(file) + 1), cpu.alloc(len(key) + 1)
        cpu.write(fp, file + b'\0')
        cpu.write(kp, key + b'\0')
        for r in range(reads):
            cpu.call(READ, ecx=this, stack_args=(fp, kp))
            self.log(f'read {r} -> {cpu.uc.reg_read(UC_X86_REG_EAX) & 0xff}')
            raw = cpu.read(this, 0x4c)
            words = ' '.join(f'{u(this + o):08x}' for o in range(0x0c, 0x4c, 4) if o != 0x28)
            sun = u(this + 4)
            self.log(f'm {words} fl={raw[8]} fog={raw[9]} weather={self.weather(u(this))} '
                     f'sun={self.name(u(sun + 4)) if sun else "0"} sky={self.name(u(this + 0x28))}')
            self.log('env ' + ' '.join(f'{u(ENV + 4 * i):08x}' for i in range(3)))
            for w in self.weathers:
                words = ' '.join(f'{u(w + o):08x}' for o in range(4, 0x98, 4) if o != 0x50)
                self.log(f'weather {words} cam={self.name(u(w + 0x50))}')
            self.log('world ' + ' '.join(f'{u(self.world + o):08x}' for o in range(0, 0x34, 4)))
            for i, n in enumerate(self.nodes):
                self.log(f'node {NAMES[i]} {u(n + 0x24):08x}')
        return '\n'.join(self.out) + '\n'


def port(archive_path, file, key, cfg, reads):
    buf = ctypes.create_string_buffer(1 << 16)
    c = (ctypes.c_int32 * 16)(*[v - (1 << 32) if v >= 1 << 31 else v for v in cfg])
    n = probe.probe_world(str(archive_path).encode(), file, key, c, reads, buf, len(buf))
    if n < 0:
        raise SystemExit(f'{archive_path}: the port cannot open it')
    return buf.raw[:n].decode('latin-1')


def compare(label, archive_path, file, key, cfg, reads=1):
    data = archive_path.read_bytes()
    want = Emulated(data, cfg).run(file, key, reads)
    got = port(archive_path, file, key, cfg, reads)
    if want != got:
        w, g = want.splitlines(), got.splitlines()
        i = next((k for k in range(max(len(w), len(g))) if k >= len(w) or k >= len(g) or w[k] != g[k]), 0)
        raise SystemExit(f'{label}: traces differ at line {i}\n  original: {w[i] if i < len(w) else "<end>"}\n'
                         f'  port:     {g[i] if i < len(g) else "<end>"}')
    return want


# A crafted archive: zReader's binary nodes, then the 0x94-byte table.
def node(v):
    if isinstance(v, bool):
        raise TypeError
    if isinstance(v, int):
        return struct.pack('<ii', 1, v)
    if isinstance(v, float):
        return struct.pack('<if', 2, v)
    if isinstance(v, bytes):  # raw float bits
        return struct.pack('<i', 2) + v
    if isinstance(v, str):
        b = v.encode('latin-1')
        return struct.pack('<iI', 3, len(b)) + b
    return struct.pack('<iI', 4, len(v) + 1) + b''.join(node(x) for x in v)


def archive(files):
    data, table = b'', b''
    for name, tree in files.items():
        body = node(tree)
        table += struct.pack('<II', len(data), len(body)) + name.encode().ljust(64, b'\0') + \
            struct.pack('<I', 0) + bytes(64) + bytes(8)
        data += body
    return data + table + struct.pack('<II', 1, len(files))


NAN = struct.pack('<I', 0x7fc00000)
FULL = ['fog', ['node', ['hormain'], 'color', [0.5, 0.25, 0.125], 'altitude', [10.0, 20.0],
                'range', [100.0, 200.0], 'hwrange', [30, 400], 'state', [7]],
        'underwater', ['color', [0.1, 0.2, 0.3], 'range', [1, 2], 'hwrange', [3, 4], 'altitude', [5, 6]],
        'jet_ceiling', [-12.5], 'ambient_temperature', [8],
        'global_wind', ['velocity', [1, 2.5, -3], 'max_speed', [13], 'random_accel', [2.5], 'random_angvel', [45]],
        'sun', ['orientation', [-20.0, 22, 0.5], 'diffuse', [0.7], 'ambient', [0.3]],
        'weather', ['type', ['SNOW'], 'particles', [77], 'color', [300, 128.9, -1],
                    'wind', ['dir', [90], 'vel', [0.5]], 'gravity', [0.2], 'alpha_gradient', [1, 0.25]],
        'fluctuate', ['period', [8.0], 'wavelength', [600], 'base', [0.6], 'amplitude', [0.4], 'heading', [30]]]
CRAFTED_KEYS = {
    'full': FULL,
    'empty': [],
    'fog_bare': ['fog', ['node', ['nosuch']]],
    'fog_black': ['fog', ['node', ['horblack'], 'range', [1.0]], 'underwater', []],
    'fog_int_node': ['fog', ['node', ['horizon'], 'hwrange', [5.0, 6.0]]],
    'temp_zero': ['ambient_temperature', [0]],
    'temp_negative': ['ambient_temperature', [-5.0]],
    'temp_nan': ['ambient_temperature', [NAN], 'jet_ceiling', [NAN]],
    'temp_string': ['ambient_temperature', ['hot'], 'jet_ceiling', ['high']],
    'wind_bare': ['global_wind', ['velocity', [1, 2]]],
    'wind_empty': ['global_wind', []],
    'sun_bare': ['sun', []],
    'sun_diffuse': ['sun', ['diffuse', [0.5], 'orientation', [1, 2]]],
    'weather_hail': ['weather', ['type', ['HAIL'], 'gravity', [9.0]]],
    'weather_lower': ['weather', ['type', ['rain']]],
    'weather_untyped': ['weather', ['particles', [5]]],
    'weather_bare': ['weather', ['type', ['RAIN']]],
    'weather_wind_bare': ['weather', ['type', ['SNOW'], 'wind', [], 'alpha_gradient', [0.5]]],
    'fluct_bare': ['fluctuate', []],
    'fluct_partial': ['fluctuate', ['base', [0.9], 'heading', [-90.0]]],
}


def main():
    blocks, mp = [], []
    for m in sorted(p.name for p in GAME.iterdir() if p.is_dir()):
        reader = GAME / m / 'reader.zbd'
        data = reader.read_bytes()
        top = next(zrd.node(data[s:s + n], 0)[0] for name, s, n in zrd.entries(data) if name == 'world.zrd')
        for key in top[0][::2]:
            blocks.append((m, reader, key))
        for f in sorted((GAME / m).glob('readermp*.zbd')):
            if any(name == 'mpinfo.zrd' for name, _, _ in zrd.entries(f.read_bytes())):
                mp.append((m, f, int(f.stem[8:])))
    runs = 0
    for m, reader, key in blocks:
        k = key.encode()
        compare(f'{m}/{key} hw', reader, b'world.zrd', k, config())
        compare(f'{m}/{key} sw', reader, b'world.zrd', k, config(hardware=0, hw_card=0))
        runs += 2
        if key.startswith('mp'):
            compare(f'{m}/{key} mp', reader, b'world.zrd', k, config(game_type=3, mission=int(key[2:])))
            runs += 1
    for m, f, n in mp:
        for cfg in (config(game_type=3, mission=n), config(game_type=3, mission=n, hardware=0)):
            compare(f'{m}/mp{n} mpinfo', f, b'world.zrd', f'mp{n}'.encode(), cfg)
            runs += 1
    # The multiplayer branch only for game type 3: a campaign "mission 3" reads world.zrd.
    compare('c3 campaign 3', GAME / 'c3' / 'reader.zbd', b'world.zrd', b'm3', config(mission=3))
    # Rain, read twice: the second read makes a second weather object.
    compare('c3/mp2 twice', GAME / 'c3' / 'reader.zbd', b'world.zrd', b'mp2', config(), reads=2)
    compare('c1/m2 twice sw', GAME / 'c1' / 'reader.zbd', b'world.zrd', b'm2', config(hardware=0), reads=2)
    runs += 3

    CRAFTED.write_bytes(archive({'world.zrd': [[x for k, v in CRAFTED_KEYS.items() for x in (k, v)]]}))
    scenes = [config(), config(hardware=0, hw_card=0), config(has_sun=0), config(has_world=0),
              config(has_horizon=0), config(no_memory=1), config(hw_card=0)]
    for key in CRAFTED_KEYS:
        for i, cfg in enumerate(scenes):
            compare(f'crafted {key} scene {i}', CRAFTED, b'world.zrd', key.encode(), cfg)
            runs += 1
        compare(f'crafted {key} twice', CRAFTED, b'world.zrd', key.encode(), config(), reads=2)
        runs += 1
    # No view: only safe without weather (the original stores [[0x5f4c8c]+4] unchecked).
    for key in ('fog_black', 'sun_bare', 'empty'):
        compare(f'crafted {key} no view', CRAFTED, b'world.zrd', key.encode(), config(has_view=0))
        runs += 1
    compare('missing key', CRAFTED, b'world.zrd', b'nosuch', config())
    compare('missing file', CRAFTED, b'..\\data\\other.zrd', b'full', config())
    compare('missing mpinfo', CRAFTED, b'world.zrd', b'full', config(game_type=3, mission=4))
    runs += 3
    print(f'ok: {runs} runs identical ({len(blocks)} mission blocks, {len(mp)} mpinfo files, '
          f'{len(CRAFTED_KEYS)} crafted blocks)')


if __name__ == '__main__':
    main()
