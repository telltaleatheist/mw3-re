"""The mission load against the original (docs/mission-load.md): the world
0x48faf0 (with its texture budget 0x4902a0) and the mission setup 0x490ab0.

Both are orchestration: every call out of mission.cpp is stubbed in the
emulator and a hook in the port, both answered from one script, and the call
traces (arguments decoded, strings as text) must be equal; then the memory
they write (the missions object's fields, the globals, the view window's
rectangles) is compared. Cases: every shipped mission (c1 c2 c3 c4 c4b t1),
every game type, software and hardware, card memory from none to 64 MB,
every TextureMemory setting, InstallOptions with and without the 16-bit
sets, the size and mission scripts failing, the load aborted after any
call, allocations failing, a mission already up (the same and another),
the start animation found with and without its number. Load-time code:
the x87 at 53 bits.

    cmake --build build && .venv/bin/python tests/differential/test_mission_load.py
"""
import ctypes
import random
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import Emu, Trace, compare, load_probe  # noqa: E402

THIS = 0x6098d0
LOAD_WORLD, SETUP, BUDGET = 0x48faf0, 0x490ab0, 0x4902a0
PILOT, RGB, TURRETS = 0x60861c, 0x608e41, 0x608e3a
GLOBALS = [(0x6061cc, 1), (0x5f4244, 1), (0x9a17c0, 4), (0x5f4c8c, 4), (0x608c84, 4), (0x609a2c, 4)]
FIELDS = [(0x55, 1), (0x56, 1), (0x58, 4), (0x5c, 1), (0x60, 4), (0x70, 1)] + \
         [(0x78 + 4 * i, 4) for i in range(5)] + [(0x8c + 4 * i, 4) for i in range(4)]
HUD, HUD_STEP = 0x5fc848, 0x4b0
NEW_BASE = 0x2c200000
MISSIONS = ['c1', 'c2', 'c3', 'c4', 'c4b', 't1']

CALLS = {
    0x4050d0: 's0', 0x405120: 'zc', 0x518f50: 'c', 0x490480: '', 0x595bb0: 'zs0', 0x484270: 'cl',
    0x509150: 'c', 0x40d4d0: '', 0x40d410: '', 0x597d80: 'c', 0x5269e0: 'c', 0x576270: 'c',
    0x516f20: 'c', 0x5182a0: 'zc,d,s0', 0x595990: 'zs0', 0x4f8f70: '', 0x526800: '', 0x535ed0: 's0',
    0x49bfd0: 's0,zs1,s2,zs3,zs4', 0x491780: 's0', 0x516f50: 'zc', 0x5190d0: '', 0x4f7fe0: 'c,zd',
    0x47f2b0: 'c,s0', 0x4b52e0: 'c,s0', 0x40aca0: 'c,s0,s1,s2', 0x4f7f70: '', 0x4f7bd0: '',
    0x40d670: 'c', 0x40d6b0: 'c', 0x40d590: 'c', 0x4f9fa0: 'c,d,s0', 0x4fa0b0: 'c,d,s0', 0x4e7b00: '',
    0x50d2c0: 'c,d,zs0', 0x5090e0: 'c', 0x50ae60: 'zc', 0x501ab0: 'c,d', 0x583880: 'c,zd,s0,s1',
    0x40d7b0: '', 0x40d750: '', 0x40f7f0: 'c,d', 0x40db60: '', 0x4ac860: 'c', 0x4a51b0: 'c,s0',
    0x4a4de0: 'c,s0',
    0x48f400: '', 0x413e60: '', 0x4319a0: '', 0x459e80: 'c', 0x491de0: 'zs0,zs1', 0x4e7b40: '',
    0x5a84c0: 'c,zs0', 0x4c3b80: '', 0x4135c0: 'c,zs0,zs1', 0x4b4ab0: 'c', 0x49e810: 'cl',
    0x4a76b0: 'zc,d', 0x45c2d0: 'zc,zd,s0,s1,s2,s3,s4', 0x50b450: 'zc', 0x490950: 'c',
    0x56f6f0: 'zc', 0x4d2e20: '', 0x4d31d0: '', 0x4e2120: '', 0x4d8610: 'c', 0x51a300: '',
    0x40f040: '', 0x40f890: '', 0x4d4b30: '', 0x508c10: 'c,d,s0,s1,s2', 0x5964c0: 's0',
    0x557320: 'zc,d', 0x5561a0: 'zc', 0x5561e0: 'zc',
}
STRINGS = {0x4af290: 'ver', 0x5799f0: 'drv', 0x579040: 'mode', 0x576040: 'msg'}
SILENT = {'new'}


def c_format(fmt, args):
    """MSVC's sprintf for %s, %d, %.2f (the formats mission.cpp uses)."""
    out, i = [], 0
    for m in re.finditer(r'%(\.\d+)?([sdf%])', fmt):
        out.append(fmt[i:m.start()])
        i = m.end()
        if m.group(2) == '%':
            out.append('%')
            continue
        v = args.pop(0)
        if m.group(2) == 's':
            out.append(v)
        elif m.group(2) == 'd':
            out.append(str(v - (1 << 32) if v & 0x80000000 else v))
        else:
            out.append(('%' + (m.group(1) or '') + 'f') % v)
    out.append(fmt[i:])
    return ''.join(out)


class Side(Trace):
    """A trace that sets the abort flag after a given number of calls."""

    def __init__(self, script, abort_at, on_abort):
        super().__init__(script)
        self.calls = 0
        self.abort_at = abort_at
        self.on_abort = on_abort

    def handle(self, key, args):
        r = Trace.handle(self, key, args)
        self.calls += 1
        if self.calls == self.abort_at:
            self.on_abort()
        return r


def make_script(case):
    s = case['script']
    return {
        '0x0040d4d0': case['hw'], '[0x0072f390]': case['fn390'], '[0x0072f38c]': case['texmem_ok'],
        '0x0040d410': case['texopt'],
        '0x00597d80': lambda n, a: case['install'] & a[0],
        '0x00595990': lambda n, a: (case['gs_ok'] if a[0].endswith('.gs') else s.get('size', 1)),
        '0x004f7fe0': lambda n, a: case['nodes'].get(a[1], 0),
        'new': lambda n, a: case['new'][n] if n < len(case['new']) else NEW_BASE + 0x1000 * (n + 1),
        '0x0047f2b0': lambda n, a: a[0], '0x004b52e0': lambda n, a: a[0],
        '0x0040aca0': lambda n, a: a[0] if case['view_ok'] else 0,
        '0x004ac860': case['stealth_obj'], '0x004a51b0': 0x7700, '0x004a4de0': 0x7800,
        'game.vt+0x54': case['index'], 'game.vt+0x2c': case['type'], 'game.vt+0x4c': case['number'],
        'game.vt+0x28': case['saved'],
        '0x0050b450': case['anims'],
        '0x0056f6f0': lambda n, a: 0x9000 + n,
        '0x0040d7b0': 3, '0x0040d750': 2,
        '0x004af290': 7, '0x005799f0': 8, '0x00579040': 9, '0x00576040': 10,
    }


# --- the original ------------------------------------------------------------
def run_original(case, entry):
    emu_box = {}

    def abort():
        emu_box['e'].w8(THIS + 0x70, 1)
    t = Side(make_script(case), case['abort_at'], abort)
    t.silent |= SILENT
    e = emu_box['e'] = Emu(t)
    e.cpu.fpu_control(0x027f)
    for addr, spec in CALLS.items():
        e.stub(addr, spec=spec)
    strings = []

    def string_stub(tag):
        def fn(em, key, a, pop):
            v = t.handle(key, a)
            p = em.world_string(f'{tag}{v}')
            em._ret(p, pop)
        return fn
    for addr, tag in STRINGS.items():
        e.stub(addr, spec='c' if addr == 0x576040 else '', fn=string_stub(tag))

    def new(em, key, a, pop):
        em._ret(t.handle('new', a), 0)
    e.stub(0x5abd26, 'new', 's0', 0, new)
    # MFC's CString: a pointer to its characters.
    empty = e.world_string('')

    def cs_ctor(em, key, a, pop):
        em.w32(em.args('c')[0], empty)
        em._ret(em.args('c')[0], 0)

    def cs_ctor_str(em, key, a, pop):
        this, src = em.args('c,s0')
        em.w32(this, em.world_string(em.cstr(src)))
        em._ret(this, 4)

    def cs_format(em, key, a, pop):
        this, fmt = em.args('s0,s1')
        f = em.cstr(fmt)
        vals, k = [], 2
        for m in re.finditer(r'%(\.\d+)?([sdf])', f):
            if m.group(2) == 'f':
                lo, hi = em.args(f's{k},s{k + 1}')
                vals.append(struct.unpack('<d', struct.pack('<II', lo, hi))[0])
                k += 2
            else:
                v = em.args(f's{k}')[0]
                vals.append(em.cstr(v) if m.group(2) == 's' else v)
                k += 1
        em.w32(this, em.world_string(c_format(f, vals)))
        em._ret(0, 0)

    def sprintf(em, key, a, pop):
        buf, fmt = em.args('s0,s1')
        f = em.cstr(fmt)
        vals, k = [], 2
        for m in re.finditer(r'%(\.\d+)?([sdf])', f):
            v = em.args(f's{k}')[0]
            vals.append(em.cstr(v) if m.group(2) == 's' else v)
            k += 1
        s = c_format(f, vals)
        em.cpu.write(buf, s.encode('latin1') + b'\0')
        em._ret(len(s), 0)
    e.stub(0x5abdb0, 'cs', '', 0, cs_ctor)
    e.stub(0x5abda4, 'cs~', '', 0, lambda em, k, a, p: em._ret(0, 0))
    e.stub(0x5abe04, 'cs(s)', '', 4, cs_ctor_str)
    e.stub(0x5abdaa, 'Format', '', 0, cs_format)
    e.import_stub(0x5bb658, 'sprintf', '', 0, sprintf)

    def unload(em, key, a, pop):
        t.handle(key, [])
        em.w32(THIS + 0x60, 0xffffffff)
        em._ret(0, 0)
    e.stub(0x490480, spec='', fn=unload)
    # The render rectangle 0x40d6a0 points at.
    rect = e.world_alloc(16)
    for i, v in enumerate(case['rect']):
        e.w32(rect + 4 * i, v & 0xffffffff)
    e.stub(0x40d6a0, spec='', fn=lambda em, k, a, p: (t.handle(k, a), em._ret(rect, 0)))
    # zVideo's function pointers.

    def texmem(em, key, a, pop):
        r = t.handle(key, a)
        if r:
            em.w32(em.args('d')[0], case['texmem'][0])
            em.w32(em.args('s0')[0], case['texmem'][1])
        em._ret(r, 4)
    e.w32(0x72f38c, e.new_stub('[0x0072f38c]', 'c', 4, texmem))
    e.w32(0x72f390, e.new_stub('[0x0072f390]', 'c', 0))
    # The pilot's game, the HUD's text lines, the 0x479480 lookup.
    game = e.object('game', slots={0x54: ('', 0), 0x2c: ('', 0), 0x4c: ('', 0), 0x28: ('', 0),
                                   0x40: ('zs0,zs1,s2,s3,s4,s5,s6', 0x1c)})
    pilot = e.world_alloc(0x100)
    e.w32(pilot + 0x6c, game)
    e.w8(pilot + 0xa8, case['mp_flag'])
    e.w32(PILOT, pilot)
    e.stub(0x479480, 'x', 'c', None, lambda em, k, a, p: em._ret(a[0], 0))
    hud_vt = e.world_alloc(0x100)
    e.w32(hud_vt + 0x7c, e.new_stub('hud', 's0', 0, lambda em, k, a, p: em._ret(
        t.handle('hudline.vt+0x7c', [(a[0] - HUD) // HUD_STEP]), 0)))
    for i in range(25):
        e.w32(HUD + HUD_STEP * i, hud_vt)
    e.w8(RGB, case['rgb'])
    e.w8(TURRETS, case['turrets'])
    for a, v in case['mem'].items():
        e.w32(a, v)
    e.w32(THIS + 0x60, case['current'] & 0xffffffff)
    e.w32(THIS + 0xb0, case['stations'])
    for (off, n), v in zip(FIELDS, case['fields']):
        if off not in (0x60, 0x70):
            (e.w8 if n == 1 else e.w32)(THIS + off, v)
    e.w8(THIS + 0x70, 0)
    if entry == 'world':
        r = e.call(LOAD_WORLD, ecx=THIS, stack=(case['index'],)) & 0xff
    else:
        r = e.call(SETUP, ecx=THIS) & 0xff
    mem = {}
    for a, n in GLOBALS:
        mem[a] = e.r8(a) if n == 1 else e.r32(a)
    vw = case['new'][1] if len(case['new']) > 1 else NEW_BASE + 0x2000
    for off in range(8, 0x28, 4):
        mem[vw + off] = e.r32(vw + off)
    mem[vw + 0x28] = e.r8(vw + 0x28)
    fields = [(e.r8(THIS + o) if n == 1 else e.r32(THIS + o)) for o, n in FIELDS]
    return r, t.log, mem, fields


_initial = None


def initial_globals():
    global _initial
    if _initial is None:
        from original import Original
        o = Original()
        _initial = {a: (o.read(a, 1)[0] if n == 1 else struct.unpack('<I', o.read(a, 4))[0]) for a, n in GLOBALS}
    return _initial


def run_rebuild(probe, case, entry):
    t = Side(make_script(case), case['abort_at'], lambda: None)  # the probe aborts itself
    t.silent |= SILENT
    probe.probe_app_set_callback(t.callback())
    probe.probe_mission_reset()
    pilot = 0x2c0f0000
    probe.probe_mission_poke(PILOT, pilot)
    probe.probe_mission_poke(pilot + 0xa8, case['mp_flag'])
    probe.probe_mission_poke(RGB, case['rgb'])
    probe.probe_mission_poke(TURRETS, case['turrets'])
    for a, v in initial_globals().items():  # the exe's .data as the emulator starts with it
        probe.probe_mission_poke(a, v)
    for a, v in case['mem'].items():
        probe.probe_mission_poke(a, v)
    rect = (ctypes.c_int32 * 4)(*case['rect'])
    probe.probe_mission_set(rect, case['texmem'][0], case['texmem'][1], case['abort_at'], case['current'])
    probe.probe_mission_stations(case['stations'])
    fields = (ctypes.c_uint32 * 15)(*case['fields'])
    probe.probe_mission_init_fields(fields)
    if entry == 'world':
        r = probe.probe_mission_load_world(case['index'])
    else:
        r = probe.probe_mission_setup()
    mem = {}
    for a, n in GLOBALS:
        mem[a] = probe.probe_mission_peek(a) & (0xff if n == 1 else 0xffffffff)
    vw = case['new'][1] if len(case['new']) > 1 else NEW_BASE + 0x2000
    for off in range(8, 0x28, 4):
        mem[vw + off] = probe.probe_mission_peek(vw + off)
    mem[vw + 0x28] = probe.probe_mission_peek(vw + 0x28) & 0xff
    f = (ctypes.c_uint32 * 15)()
    probe.probe_mission_fields(f)
    return r, t.log, mem, list(f)


def make_case(seed):
    r = random.Random(seed)
    index = seed % 6 if r.random() < 0.9 else r.choice([6, 7, 8, 9, -1])
    new = [NEW_BASE + 0x1000 * (i + 1) for i in range(6)]
    if r.random() < 0.08:
        new[r.choice([0, 3])] = 0  # an allocation fails (the view and its window never do: the original writes through them)
    view = new[2]
    turret_obj = 0x2c300000
    case = {
        'index': index, 'type': r.choice([-1, 0, 1, 1, 2, 3, 4, 5]), 'number': r.randrange(0, 9),
        'saved': r.randrange(2), 'hw': r.randrange(2), 'fn390': r.choice([0, 0, 1]),
        'texmem_ok': [r.choice([0, 1, 1]), r.choice([0, 1])],
        'texmem': [r.choice([0, 2 << 20, 3 << 20, 4 << 20, 6 << 20, 8 << 20, 12 << 20, 16 << 20, 64 << 20,
                             r.randrange(1 << 30)]), r.randrange(1 << 28)],
        'texopt': r.randrange(0, 8), 'install': r.choice([0, 1, 2, 4, 6, 0x101, 0x103, 0xff]),
        'gs_ok': 0 if r.random() < 0.08 else 1, 'script': {'size': r.randrange(2)},
        'nodes': {'world1': 0x7101, 'camera1': 0x7102, 'window1': 0x7103, 'display': 0x7104},
        'new': new, 'view_ok': True, 'stealth_obj': r.choice([0, 0x7600]),
        'anims': [r.choice([0, 0x8800]), r.choice([0, 0x8900])],
        'rect': [r.randrange(0, 50), r.randrange(0, 50), r.randrange(300, 1700), r.randrange(200, 1300)],
        'rgb': r.randrange(2), 'turrets': r.randrange(2), 'mp_flag': r.randrange(2),
        'mem': {view + 4: turret_obj, turret_obj + 0x38: 0x2c310000},
        'current': -1, 'stations': r.choice([0, 0x7a00]),
        'abort_at': r.randrange(1, 170) if r.random() < 0.35 else -1,
        'fields': [r.randrange(256) if n == 1 else r.randrange(1 << 32) for _, n in FIELDS],
    }
    if r.random() < 0.08:
        case['current'] = r.choice([index, (index + 1) % 6])
        case['mem'][0x5f4c8c] = view  # the mission up has its view
    if not case['turrets'] and r.random() < 0.1:
        case['view_ok'] = False  # the view's constructor fails (the turrets would read through it)
    return case


def main():
    probe = load_probe()
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 240
    calls = 0
    for seed in range(count):
        case = make_case(seed)
        for entry in ('world', 'setup'):
            try:
                a = run_original(case, entry)
            except Exception:
                print(f'seed {seed} {entry}: the original faulted; case {case}')
                raise
            b = run_rebuild(probe, case, entry)
            name = f'seed {seed} {entry} ({case["index"]}, type {case["type"]})'
            if not compare(name, a[1], b[1]):
                raise SystemExit(1)
            if a[0] != b[0]:
                print(f'{name}: returned {a[0]} (original) and {b[0]} (rebuild)')
                raise SystemExit(1)
            if a[2] != b[2]:
                diff = {hex(k): (hex(a[2][k]), hex(b[2].get(k, 0))) for k in a[2] if a[2][k] != b[2].get(k)}
                print(f'{name}: memory differs (original, rebuild): {diff}')
                raise SystemExit(1)
            if a[3] != b[3]:
                diff = [(hex(FIELDS[i][0]), hex(x), hex(y)) for i, (x, y) in enumerate(zip(a[3], b[3])) if x != y]
                print(f'{name}: the missions object differs (offset, original, rebuild): {diff}')
                raise SystemExit(1)
            calls += len(a[1])
    print(f'{count} cases x 2 entries, {calls} calls: the rebuilt mission load matches the original')


if __name__ == '__main__':
    main()
