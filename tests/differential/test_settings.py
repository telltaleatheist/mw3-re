"""The settings loader 0x40c420 and the settings store against the original
(docs/settings.md).

Each case gives both sides the same machine (the 12 dwords 0x597fc0 probes),
InstallOptions, detail.zrd tree (the shipped one or a random one: conditions on
every machine value with every operator, unknown names and operators, value
names, ints and floats), registry contents (values of the right size, the
wrong size, missing) and failures (either key not opening). The registry is a
fake on both sides; its calls, the loader's calls into other modules and the
options' setters' calls are recorded and compared, then the whole store (every
entry's name, type, size, scope and bytes, newest first) and the globals the
loader and setters write. Then the store is saved (0x597a60) with some values
changed, with and without failures, and the registry writes are compared.

Load-time code: the x87 runs at 53 bits (the CRT start-up's _PC_53).

    cmake --build build && .venv/bin/python tests/differential/test_settings.py
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import Emu, Trace, compare, load_probe  # noqa: E402
from test_app_options import STUBS  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools' / 'analysis'))
import zrd  # noqa: E402

LOADER, READ_ALL, WRITE_ALL, CREATE = 0x40c420, 0x5977d0, 0x597a60, 0x597d90
READ_NODE, LOAD, FREE_NODE, MESSAGE, CTL_DEFAULTS, CTL_LOAD = (
    0x557180, 0x556740, 0x556820, 0x576040, 0x411150, 0x4120a0)
LIST, NAMES, INITED, MACHINE, INSTALL = 0x731910, 0x731918, 0x731914, 0x731924, 0x731954
IAT = {'malloc': 0x5bb664, 'calloc': 0x5bb60c, 'free': 0x5bb65c, 'strdup': 0x5bb670,
       'strncpy': 0x5bb648, 'readfile': 0x5bb0c4, 'RegCreateKeyExA': 0x5bb000,
       'RegSetValueExA': 0x5bb004, 'RegCloseKey': 0x5bb008, 'RegOpenKeyExA': 0x5bb00c,
       'RegQueryValueExA': 0x5bb010}
HKCU, HKLM = 0x80000001, 0x80000002
FILE = 0x1234
SILENT = {'[0x005f4c8c]', '[[0x0060861c]+0x14]', '[0x0072f348]', '[mech+0xd4]'}
# Probe words (probe_options_io) of the globals outside the store: hw, then
# pixel_double, the intensity preset, the axis table, its ready byte, cheated.
GLOBAL_WORDS = [(0, 0x5f4cc8, 4), (60, 0x5f4d4c, 4), (61, 0x608e54, 4), (62, 0x608e58, 4),
                (63, 0x608e5c, 4), (64, 0x608e4c, 1), (65, 0x608e4d, 1)] + \
               [(66 + i, 0x6040d0 + 4 * i, 4) for i in range(16)] + [(82, 0x604110, 1), (83, 0x609994, 1)]

OPTION_NAMES = ['HWCardFlag', 'HWCardDev', 'TextureMemory_SW', 'TextureMemory_HW', 'GfxFlags_SW',
                'GfxFlags_HW', 'SoundLOD', 'ObjectLOD_SW', 'ObjectLOD_HW', 'EffectsLevel_SW',
                'EffectsLevel_HW', 'ShadowParts_SW', 'ShadowParts_HW', 'Shadow', 'GameOptions',
                'MouseAccelX', 'MouseAccelY', 'JoyAccelX', 'JoyAccelY', 'GameIntensity', 'MouseMode',
                'SoundVolume', 'CDVolume', 'VStride', 'Camera', 'Render', 'Display', 'Window', 'VMode',
                'InGameVMode', 'ActiveCtlCfg', 'DefaultCmdMap']
SIZES = {'Camera': 12, 'Render': 0x28, 'Display': 0x28, 'Window': 0x28, 'ActiveCtlCfg': 0x104,
         'DefaultCmdMap': 0x29c}
DETAIL_KEYS = ['VMode', 'TextureMemory_SW', 'TextureMemory_HW', 'Transparency', 'Lighting',
               'Perspective', 'GlobalLight_HW', 'GlobalLight_SW', 'AllVideoBuffer', 'SoundLOD',
               'ObjectLOD_HW', 'ObjectLOD_SW', 'EffectsLevel_HW', 'EffectsLevel_SW', 'ShadowParts_SW',
               'ShadowParts_HW', 'Shadow', 'CameraMode', 'Invincible', 'InfiniteAmmo', 'HeatMgmt',
               'TorsoAutoRtn', 'ThrottleConst', 'UseJoystick', 'MW2JJets', 'FastMenu', 'InGameVMode']
VALUE_NAMES = ['TRUE', 'FALSE', 'HIGH', 'MEDIUM', 'LOW', 'CPU_CLASS_PENTIUM_PRO', 'CPU_CLASS_80486',
               'TEXMEM_MAX', 'TEXMEM_4MB', 'ZVID_320x240x16', 'ZVID_640x480x16', 'ZVID_1024x768x16',
               'camAttach', 'camTether', 'camTetherZ', 'SOUND_API_A3D', 'NOT_A_NAME']
COND_NAMES = ['CPU_CLASS', 'CPU_MHZ', 'VIDEO_KB', 'RAM_KB', 'HW_ACCEL', 'GPU_MHZ']
OPS = ['==', '<', '>', '<=', '>=', '!=', '~=', '=>']


# --- reader trees ------------------------------------------------------------
def encode(n):
    """A node in the binary zrd form (docs/zreader.md)."""
    if isinstance(n, bool):
        n = int(n)
    if isinstance(n, int):
        return struct.pack('<ii', 1, n)
    if isinstance(n, float):
        return struct.pack('<if', 2, n)
    if isinstance(n, str):
        b = n.encode('latin1')
        return struct.pack('<iI', 3, len(b)) + b
    return struct.pack('<iI', 4, len(n) + 1) + b''.join(encode(x) for x in n)


def shipped_detail():
    data = (ROOT / 'game' / 'mw3-1.2' / 'zbd' / 'reader.zbd').read_bytes()
    for name, start, length in zrd.entries(data):
        if name == 'detail.zrd':
            return data[start:start + length]
    raise SystemExit('detail.zrd not in reader.zbd')


def random_value(r):
    k = r.random()
    if k < 0.45:
        return r.choice(VALUE_NAMES)
    if k < 0.8:
        return r.randrange(-2, 12)
    return r.uniform(-3, 12)


def random_condition(r):
    if r.random() < 0.25:
        return r.choice(['DEFAULT', 'DEFAULT', 'default', 'OTHER'])
    if r.random() < 0.05:
        return [r.choice(COND_NAMES), '>=']  # not three items: never holds
    v = r.choice([r.randrange(0, 8), r.randrange(100, 400), r.randrange(10000, 70000),
                  r.choice(VALUE_NAMES), r.uniform(0, 300)])
    return [r.choice(COND_NAMES), r.choice(OPS), v]


def random_detail(r):
    body = []
    for key in r.sample(DETAIL_KEYS, r.randrange(4, len(DETAIL_KEYS))):
        entries = [[random_condition(r), random_value(r)] for _ in range(r.randrange(0, 4))]
        if r.random() < 0.7:
            entries.append(['DEFAULT', random_value(r)])
        body += [key, entries]
    return encode([body])


def random_machine(r):
    m = [0] * 12
    vendor = b'GenuineIntel'
    m[0:3] = struct.unpack('<3I', vendor)
    m[4] = r.choice([r.randrange(0, 8), r.randrange(0, 8) | 0x8000])
    m[5] = r.choice([r.randrange(60, 400), r.randrange(100, 300), 200, 233, 166, 3000])
    m[6] = r.randrange(0, 0x80)
    m[7] = r.choice([r.randrange(8000, 70000), 33555, 63000, 31000, 15000, 0x3fffff])
    m[8] = r.choice([0, 0, r.randrange(1 << 16)])
    m[9] = r.randrange(1 << 20)
    m[10] = r.choice([0, r.randrange(1 << 8)])
    m[11] = r.choice([0, r.randrange(1 << 8)])
    return m


def fbits(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


def random_registry(r):
    """{(user, name): bytes}: right sizes, wrong sizes, and some missing."""
    reg = {}
    for name in OPTION_NAMES + ['Unknown', 'InstallOptions']:
        if r.random() < 0.35:
            continue
        size = SIZES.get(name, 4)
        if r.random() < 0.15:
            size = r.choice([0, 1, 2, 8, size + 4, max(size - 4, 0)])
        if name in ('MouseAccelX', 'MouseAccelY', 'JoyAccelX', 'JoyAccelY', 'SoundVolume', 'CDVolume') \
                and size == 4:
            data = struct.pack('<f', r.uniform(0, 3))
        elif size == 4:
            data = struct.pack('<I', r.choice([0, 1, 2, 3, 5, 8, 9, 10, 999, 0x1ff, r.randrange(1 << 32)]))
        else:
            data = bytes(r.randrange(256) for _ in range(size))
        user = 1 if r.random() < 0.85 else 0
        reg[(user, name)] = data
    return reg


# --- the original's side -----------------------------------------------------
class Original:
    def __init__(self, t, case):
        self.t = t
        self.case = case
        e = self.emu = Emu(t)
        e.cpu.fpu_control(0x027f)
        e.heap_stubs()
        self.reg = dict(case['reg'])
        self.handles = {}
        self.file = b''
        self.pos = 0

        def imp(name, fn, pop=0):
            e.import_stub(IAT[name], name, '', pop, lambda em, k, a, p: em._ret(fn() or 0, p))
        a = e.cpu.arg
        imp('malloc', lambda: e.alloc(max(a(0), 4)))
        imp('calloc', lambda: e.alloc(max(a(0) * a(1), 4)))
        imp('free', lambda: 0)
        imp('strdup', lambda: e.world_string(e.cstr(a(0))))
        imp('strncpy', self.strncpy)
        imp('readfile', self.read_file, 20)
        imp('RegOpenKeyExA', lambda: self.open('RegOpenKeyExA', a(0), a(1), a(4)), 20)
        imp('RegCreateKeyExA', lambda: self.open('RegCreateKeyExA', a(0), a(1), a(7)), 36)
        imp('RegQueryValueExA', self.query, 24)
        imp('RegSetValueExA', self.set_value, 24)
        imp('RegCloseKey', lambda: self.t.handle('RegCloseKey', [self.h(a(0))]) and 0, 4)
        for addr, spec in STUBS.items():
            e.stub(addr, spec=spec)
        e.stub(LOAD, spec='zc', fn=self.load)
        e.stub(FREE_NODE, spec='')
        e.stub(MESSAGE, spec='c', fn=self.message)
        e.stub(CTL_DEFAULTS, spec='')
        # The controls file's name is ActiveCtlCfg's 0x104 bytes; a saved one
        # without its 0 runs on in the original (FIX (bug) in the port).
        e.stub(CTL_LOAD, spec='c', fn=lambda em, k, a, p: em._ret(
            self.t.handle(k, [em.cstr(a[0])[:0x104]]), p))
        # The store as 0x598210 leaves it, with the machine 0x597fc0 found.
        e.w32(NAMES, e.world_string('MicroProse'))
        e.w32(NAMES + 4, e.world_string('MechWarrior 3'))
        e.w32(NAMES + 8, e.world_string('1.0'))
        e.w32(INITED, 1)
        e.w32(LIST, 0)
        for i, v in enumerate(case['machine']):
            e.w32(MACHINE + 4 * i, v)
        e.w32(INSTALL, case['install'])
        self.tree = 0
        if case['zrd'] is not None:
            self.tree = self.build(case['zrd'])
        if case['cmdmap']:
            name = e.world_string('DefaultCmdMap')
            ent = e.call(CREATE, ecx=name, edx=7, stack=(0x29c, 1))
            buf = e.r32(ent)
            e.cpu.write(buf, bytes((i * 7 + 3) & 0xff for i in range(0x29c)))

    def h(self, v):
        return v if v in (0x100, 0x101) else 0xbad

    def strncpy(self):
        e, a = self.emu, self.emu.cpu.arg
        dst, src, n = a(0), a(1), a(2)
        s = e.cstr(src).encode('latin1')[:n]
        e.cpu.write(dst, s + b'\0' * (n - len(s)))
        return dst

    def read_file(self):
        a = self.emu.cpu.arg
        buf, n, out = a(1), a(2), a(3)
        chunk = self.file[self.pos:self.pos + n]
        self.pos += len(chunk)
        self.emu.cpu.write(buf, chunk)
        self.emu.w32(out, len(chunk))
        return 1

    def build(self, data):
        self.file, self.pos = data, 0
        root = self.emu.alloc(8)
        self.emu.call(READ_NODE, ecx=FILE, edx=root)
        return root

    def load(self, emu, key, args, pop):
        self.t.handle(key, args)
        emu._ret(self.tree if args[0] == 'detail.zrd' else 0, pop)

    def message(self, emu, key, args, pop):
        self.t.handle(key, args)
        emu._ret(emu.world_string('DefaultCtlConfig'), pop)

    def open(self, key, hkey, sub, phk):
        r = self.t.handle(key, [hkey, self.emu.cstr(sub)])
        self.emu.w32(phk, (0x101 if hkey == HKCU else 0x100) if r == 0 else 0)
        return r

    def query(self):
        e, a = self.emu, self.emu.cpu.arg
        hk, name, typ, data, cb = a(0), e.cstr(a(1)), a(3), a(4), a(5)
        self.t.handle('RegQueryValueExA', [self.h(hk), name, 1 if data else 0, e.r32(cb) if data else 0])
        v = self.reg.get((1 if hk == 0x101 else 0, name))
        if v is None:
            return 2
        if data:
            if len(v) > e.r32(cb):
                return 234
            e.cpu.write(data, v)
        e.w32(cb, len(v))
        if typ:
            e.w32(typ, 3)
        return 0

    def set_value(self):
        e, a = self.emu, self.emu.cpu.arg
        hk, name, typ, data, cb = a(0), e.cstr(a(1)), a(3), a(4), a(5)
        return self.t.handle('RegSetValueExA', [self.h(hk), name, typ, cb, e.cpu.read(data, cb).hex()])

    def store(self):
        e = self.emu
        out = []
        p = e.r32(LIST)
        while p:
            typ, size, name, scope = e.r32(p + 8), e.r32(p + 0xc), e.cstr(e.r32(p + 0x10)), e.r32(p + 0x14)
            data = e.cpu.read(p, 4 if typ <= 1 else 8) if typ <= 2 else e.cpu.read(e.r32(p), size)
            out.append(f'{name} {s32(typ)} {size} {s32(scope)} {data.hex()}')
            p = e.r32(p + 0x18)
        out.append('cmdmap set' if e.r32(0x5f4d28) else 'cmdmap none')
        return out

    def globals_(self):
        e = self.emu
        g = {i: (e.r32(a) if n == 4 else e.r8(a)) for i, a, n in GLOBAL_WORDS}
        g['machine'] = [e.r32(0x5f4c98 + 4 * i) for i in range(12)]
        return g


def s32(v):
    return v - (1 << 32) if v & 0x80000000 else v


def make_trace(case):
    t = Trace(dict(case['script']))
    t.silent |= SILENT
    for k in SILENT:
        t.script.setdefault(k, 0)
    return t


def run_original(case):
    t = make_trace(case)
    o = Original(t, case)
    r = o.emu.call(LOADER, ecx=o.emu.world_string('detail.zrd') if case['path'] else 0)
    load_log = list(t.log)
    store = o.store() if r else None
    glob = o.globals_() if r else None
    save = None
    if r and case['save'] is not None:
        t.log = []
        for name, data in case['save']['pokes']:
            p = o.emu.r32(LIST)
            while p:
                if o.emu.cstr(o.emu.r32(p + 0x10)) == name:
                    typ = o.emu.r32(p + 8)
                    o.emu.cpu.write(p if typ <= 2 else o.emu.r32(p), data)
                p = o.emu.r32(p + 0x18)
        sr = o.emu.call(WRITE_ALL)
        save = (sr, list(t.log))
    return r, load_log, store, glob, save


def run_rebuild(probe, case):
    t = make_trace(case)
    probe.probe_app_set_callback(t.callback())
    m = (ctypes.c_uint32 * 12)(*case['machine'])
    probe.probe_settings_reset(m, case['install'])
    if case['cmdmap']:
        probe.probe_settings_cmdmap(0x29c)
    for (user, name), data in case['reg'].items():
        probe.probe_settings_reg(user, name.encode(), data, len(data))
    z = case['zrd']
    r = probe.probe_settings_load(1 if case['path'] else 0, z if z is not None else b'',
                                  len(z) if z is not None else -1)
    load_log = list(t.log)
    store = glob = save = None
    if r:
        buf = ctypes.create_string_buffer(1 << 16)
        probe.probe_settings_dump(buf, len(buf))
        store = buf.value.decode('latin1').strip('\n').split('\n')
        n = probe.probe_options_words()
        w = (ctypes.c_uint32 * n)()
        probe.probe_options_io(w, 0)
        glob = {i: w[i] for i, _, _ in GLOBAL_WORDS}
        mc = (ctypes.c_uint32 * 12)()
        probe.probe_settings_machine_copy(mc)
        glob['machine'] = list(mc)
    if r and case['save'] is not None:
        t.log = []
        for name, data in case['save']['pokes']:
            probe.probe_settings_poke(name.encode(), data, len(data))
        sr = probe.probe_settings_save()
        save = (sr, list(t.log))
    return r, load_log, store, glob, save


def make_case(seed):
    r = random.Random(seed)
    shipped = seed % 3 != 2
    case = {
        'machine': random_machine(r),
        'install': r.choice([0, 0x101, 0xff00, 0x1, r.randrange(1 << 16)]),
        'zrd': shipped_detail() if shipped else random_detail(r),
        'path': True,
        'cmdmap': r.random() < 0.7,
        'reg': random_registry(r),
        'script': {},
        'save': None,
    }
    if seed % 17 == 5:
        case['zrd'] = None          # detail.zrd cannot be read: 0, nothing made
    if seed % 13 == 7:
        case['path'], case['zrd'] = False, None  # no file named: the defaults
    fail = r.random()
    if fail < 0.06:
        case['script']['RegOpenKeyExA'] = [r.choice([2, 5])]          # HKCU missing
    elif fail < 0.12:
        case['script']['RegOpenKeyExA'] = [0, 2]                      # HKLM missing
    if r.random() < 0.8:
        pokes = []
        for name in r.sample(OPTION_NAMES, r.randrange(0, 6)):
            size = SIZES.get(name, 4)
            pokes.append((name, bytes(r.randrange(256) for _ in range(size))))
        case['save'] = {'pokes': pokes}
        f = r.random()
        if f < 0.08:
            case['script']['RegCreateKeyExA'] = [2]
        elif f < 0.16:
            case['script']['RegCreateKeyExA'] = [0, 5]
        elif f < 0.26:
            k = r.randrange(1, 20)
            case['script']['RegSetValueExA'] = [0] * k + [5]
    return case


def main():
    probe = load_probe()
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 300
    calls = writes = 0
    for seed in range(count):
        case = make_case(seed)
        a = run_original(case)
        b = run_rebuild(probe, case)
        name = f'seed {seed}'
        if a[0] != b[0]:
            print(f'{name}: the loader returned {a[0]} (original) and {b[0]} (rebuild)')
            raise SystemExit(1)
        if not compare(name + ' load', a[1], b[1]):
            raise SystemExit(1)
        if a[2] != b[2]:
            for x, y in zip(a[2] or [], b[2] or []):
                if x != y:
                    print(f'{name}: store differs:\n  original {x[:160]}\n  rebuild  {y[:160]}')
                    break
            else:
                print(f'{name}: store lengths differ {len(a[2] or [])} {len(b[2] or [])}')
            raise SystemExit(1)
        if a[3] != b[3]:
            diff = {k: (a[3][k], b[3][k]) for k in a[3] if a[3][k] != b[3][k]}
            print(f'{name}: globals differ (original, rebuild): {diff}')
            raise SystemExit(1)
        if a[4] is not None or b[4] is not None:
            if a[4][0] != b[4][0]:
                print(f'{name}: the save returned {a[4][0]} and {b[4][0]}')
                raise SystemExit(1)
            if not compare(name + ' save', a[4][1], b[4][1]):
                raise SystemExit(1)
            writes += len(a[4][1])
        calls += len(a[1])
    print(f'{count} cases: {calls} load calls and {writes} save calls; the rebuilt settings loader '
          'and store match the original')


if __name__ == '__main__':
    main()
