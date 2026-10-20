"""The missions' frame and cleanup against the original (docs/mission-frame.md):
the start 0x48f400 (with the cheats' check 0x491d20), the frame 0x48f470
(the world's cloud drift 0x4926d0, the valves' timers, the objectives, the
voice lines 0x48f630 / 0x48f6c0), the win 0x48f740, the loss 0x48f9e0, the
end screen 0x491bf0, Exit Mission 0x48f440 and the cleanup 0x491780 (the
world's 0x491d80 with it).

Orchestration: every call out of mission.cpp is stubbed in the emulator and
a hook in the port, both answered from one script; the call traces must be
equal, then the missions object's fields and the words of other objects the
code writes (Instant Action's waves). Each case runs a random sequence of
those entries on a random missions object: every game type, Instant
Action's modes and waves round 20, training's lessons, objectives present
or not and answering either way, the loss's delay running out, voices of
every priority, multiplayer, the cockpit's screen there or not, the cloud
pattern on and off, the cleanup with and without the unload and with every
handle set or not. In play: the x87 at 24 bits.

    cmake --build build && .venv/bin/python tests/differential/test_mission_frame.py [cases]
"""
import os
import random
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import Emu, Trace, compare, load_probe  # noqa: E402

THIS = 0x6098d0
BEGIN, UPDATE, VOICE_ASK, COMPLETE, LOSE, COMMAND, CLEANUP = \
    0x48f400, 0x48f470, 0x48f6c0, 0x48f740, 0x48f9e0, 0x48f440, 0x491780
PILOT, VIEW, STEALTH, THREAD, X60B544, NETWORK, X1C2, DT = \
    0x60861c, 0x5f4c8c, 0x609a2c, 0x608cb4, 0x60b544, 0x608e45, 0x608e4a, 0x72c52c

# The missions object's fields: (offset, size, float).
FIELDS = [(0x00, 4, 0), (0x04, 4, 0), (0x08, 1, 0), (0x09, 1, 0), (0x4c, 4, 1), (0x50, 4, 1), (0x54, 1, 0),
          (0x55, 1, 0), (0x56, 1, 0), (0x58, 4, 1), (0x5c, 1, 0), (0x60, 4, 0), (0x64, 4, 0), (0x68, 4, 0),
          (0x6c, 4, 0), (0x70, 1, 0)] + [(o, 4, 0) for o in range(0x78, 0x9c, 4)] + \
         [(0x9c, 4, 0), (0xa0, 4, 0), (0xa4, 4, 1), (0xa8, 4, 0), (0xac, 4, 1), (0xb0, 4, 0), (0xb4, 4, 0),
          (0xb8, 4, 0), (0xbc, 4, 0), (0xc4, 1, 0)]

CALLS = {
    # the start
    0x51d2f0: 'c,d', 0x40cf20: '', 0x40cf80: '', 0x40cfc0: '',
    # the frame
    0x4d3d30: 's0', 0x45a270: 'c,s0', 0x45e540: 'c,s0', 0x430aa0: 'c', 0x45c180: 'c,d,s0,s1',
    0x446ad0: 'c', 0x56e8e0: 'c', 0x56e9f0: 'c', 0x56e3d0: 'c,s0', 0x479460: '', 0x5392c0: '',
    0x49dc80: 'cl', 0x45c1d0: 'c,d,s0,s1,s2', 0x45c2d0: 'zc,zd,s0,s1,s2,s3,s4', 0x45c4a0: 'c,d,s0,s1',
    0x4794a0: 's0,s1,s2,s3,s4', 0x4373b0: 'c', 0x479480: '', 0x41d510: 'c,s0', 0x40f0b0: 'cl',
    0x48ef00: 'c,d,s0,s1,s2,s3,s4,s5', 0x59fc80: 'c,s0', 0x543910: 'c',
    # the cleanup
    0x4eb040: '', 0x4eb030: 'c', 0x4e7bc0: '', 0x4582c0: 'c', 0x50ace0: 'c', 0x570410: 'c', 0x56eb10: '',
    0x56f0a0: 'c', 0x56f190: 'c', 0x5716a0: '', 0x40ae70: 'c,s0', 0x40adf0: 'c,s0,s1', 0x4782c0: 's0',
    0x5a89c0: '*s0', 0x5a8820: '*s0', 0x4e6910: '', 0x4b4ae0: 'c', 0x413770: 'c', 0x49dc40: '', 0x4a7970: '',
    0x4d8270: '', 0x584fe0: '', 0x47a720: 'c', 0x4d54f0: '', 0x479850: '', 0x4ea750: '', 0x405120: 'zc',
    0x58b290: 'c', 0x4c5fa0: '', 0x4c3240: '', 0x4d3fa0: '', 0x4d3030: '', 0x4d71f0: '', 0x427300: '',
    0x4f7bd0: '', 0x414000: '', 0x56f530: 'zc', 0x431800: '', 0x50b2f0: '', 0x490480: '', 0x5394c0: 'c,d',
    0x5573d0: 'c', 0x576270: 'c',
}
ULPS = {'0x00543a50': {0, 1}}


def f2u(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


def c_format(fmt, args):
    out, i = [], 0
    for m in re.finditer(r'%([sd%])', fmt):
        out.append(fmt[i:m.start()])
        i = m.end()
        v = args.pop(0) if m.group(1) != '%' else None
        out.append('%' if v is None else v if m.group(1) == 's' else str(v - (1 << 32) if v & 0x80000000 else v))
    out.append(fmt[i:])
    return ''.join(out)


# --- the case ----------------------------------------------------------------------
BASE = 0x2c100000  # the world objects' addresses: the original's fake objects sit at the same ones


def make_case(seed):
    r = random.Random(seed)
    mp = r.random() < 0.08
    type_ = r.choice([0, 1, 1, 1, 2, 2, 3, 4, 4, 5])
    if mp:
        type_ = 3
    case = {'seed': seed, 'type': type_, 'number': r.randrange(1, 12), 'mp': mp,
            'network': r.random() < 0.15, 'x1c2': r.random() < 0.15}
    # Which objects exist (their addresses come from the emulator's world).
    case['has'] = {
        'vehicle': mp or r.random() < 0.85, 'own_view': r.random() < 0.7, 'screen': r.random() < 0.7,
        'stations': r.random() < 0.2, 'xb4': r.random() < 0.3, 'stealth': r.random() < 0.6,
        'weather': r.random() < 0.3, 'event': r.random() < 0.3, 'thread': r.random() < 0.3,
        'xbc': r.random() < 0.3, 'ia': type_ == 2 or r.random() < 0.3,
    }
    case['obj'] = [r.random() < 0.15, r.random() < 0.75, r.random() < 0.6, r.random() < 0.6, r.random() < 0.6]
    case['ia_mode'] = r.choice([0, 1, 2, 2, 3])
    waves = [0, 5, 18, 19, 19, 20, 20, 21, 25]
    case['ia_waves'] = [r.choice(waves), r.choice(waves), r.randrange(30)]
    case['angles'] = [r.uniform(-1.5, 1.5), r.uniform(-7, 7), r.uniform(-1, 1)]
    case['dt'] = r.choice([0.0, 1 / 30, 0.05, 0.25, 1.0, r.uniform(0, 3)])
    case['veh578'] = r.choice([0, 10, 3])
    fl = lambda: r.choice([-1.0, 0.0, 0.5, 1.0, 2.0, 8.0, r.uniform(-2, 10)])  # noqa: E731
    case['fields'] = {
        0x08: r.randrange(2), 0x09: r.randrange(2), 0x4c: f2u(r.uniform(0, 600)), 0x50: f2u(r.uniform(-60, 600)),
        0x54: r.randrange(2), 0x55: int(r.random() < 0.25), 0x56: int(r.random() < 0.25),
        0x58: f2u(r.choice([0.0, 0.01, 0.2, 1.0, 8.0, -1.0])), 0x5c: int(r.random() < 0.85),
        0x60: r.choice([-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9]) & 0xffffffff, 0x64: r.randrange(0, 12), 0x70: r.randrange(2),
        0x8c: 0x7a00, 0x90: 0x7a01, 0x94: 0x7a02, 0x98: 0x7a03,
        0x9c: r.choice([0, 0x7b00]), 0xa0: r.choice([0, 0, 0x7b01]), 0xa4: f2u(fl()),
        0xa8: r.choice([0, 0, 0x7b02]), 0xac: f2u(fl()), 0xb8: r.choice([0, 0x7b03]), 0xc4: r.randrange(2),
    }
    n = 40
    rl = lambda p: [int(r.random() < p) for _ in range(n)]  # noqa: E731
    case['answers'] = {
        '0x0045e540': rl(0.4), 'condition.vt+0x4': rl(0.3), '0x0045c180': rl(0.5), '0x0056e8e0': rl(0.5),
        '0x004373b0': rl(0.5), '0x00479480': int(mp), '0x0040cf20': rl(0.2), '0x0040cf80': rl(0.2),
        '0x0040cfc0': [int(r.random() < 0.8) for _ in range(n)],
    }
    ops = []
    if r.random() < 0.3:
        ops.append(('begin',))
    for _ in range(r.randrange(1, 7)):
        k = r.random()
        if k < 0.6:
            ops.append(('update', f2u(case['dt'] if r.random() < 0.7 else r.uniform(0, 2))))
        elif k < 0.7:
            ops.append(('complete',))
        elif k < 0.8:
            ops.append(('lose',))
        elif k < 0.9:
            ops.append(('command', r.choice([0x71, 0x71, 0x72, 0x43])))
        else:
            ops.append(('ask', r.choice([0, 0x7c00, 0x7c01]), f2u(fl())))
    if r.random() < 0.5:
        ops.append(('cleanup', r.randrange(2)))
    case['ops'] = ops
    return case


def script_for(case, addr):
    a = case['answers']
    s = dict(a)
    s.update({
        'game.vt+0x2c': case['type'], 'game.vt+0x4c': case['number'],
        '0x00430aa0': addr['screen'], '0x0056e3d0': lambda n, args: 0x7d00 + n,
        '0x00479460': addr['ia'], '0x004794a0': 0x7e00, '0x0041d510': addr['action'],
        '0x004eb040': 0x7e10, '0x004582c0': 0x7e20, '0x0056eb10': 0x7e30,
    })
    return s


# --- the original ---------------------------------------------------------------------
def build_world(e, case):
    """The objects both sides see: the emulator's fake objects, at addresses the
    rebuild's memory model gets too."""
    h = case['has']
    mem = {}  # address -> dword the rebuild's peek answers (bytes as dwords)
    addr = {}

    def put(a, v, size=4):
        (e.w8 if size == 1 else e.w32)(a, v)
        mem[a] = v
    game = e.object('game', slots={0x2c: ('', 0), 0x4c: ('', 0)})
    pilot = e.world_alloc(0x100)
    put(pilot + 0x6c, game)
    put(PILOT, pilot)
    vehicle = e.object('object', size=0x700, slots={0: ('c,s0', 4)}) if h['vehicle'] else 0
    if vehicle:
        put(vehicle + 0x578, case['veh578'])
        put(vehicle + 0x684, 0x7f00)
    put(pilot + 0x14, vehicle)
    view = e.world_alloc(0x80)
    node, data = e.world_alloc(0x40), e.world_alloc(0x40)
    put(view + 4, node)
    put(node + 0x38, data)
    for i, a in enumerate(case['angles']):
        put(data + 0x20 + 4 * i, f2u(a))
    put(view + 0x64, vehicle if h['own_view'] and vehicle else 0x7f10)  # no mech, no cockpit (the exe reads through it)
    put(VIEW, view)
    screen = 0
    if h['screen']:
        screen = e.world_alloc(0x17970)
        svt = e.world_alloc(0x100)
        e.w32(svt + 0x60, e.new_stub('screen.vt+0x60', 'c,s0', 4))
        put(screen + 0x17964, svt)
    addr['screen'] = screen
    ia = e.world_alloc(0x80) if h['ia'] else 0
    if ia:
        put(ia + 0x44, case['ia_mode'])
        for i, v in enumerate(case['ia_waves']):
            put(ia + 0x68 + 4 * i, v)
    addr['ia'] = ia
    addr['action'] = e.object('action', slots={0x3c: ('c,s0,s1', 8)})
    put(STEALTH, 0x7f20 if h['stealth'] else 0)
    put(THREAD, 0x7f30)
    put(X60B544, 0x7f40)
    put(NETWORK, int(case['network']), 1)
    put(X1C2, int(case['x1c2']), 1)
    put(DT, f2u(case['dt']))
    # The objectives: +0x7c / +0x80 lists with a done byte, +0x84 / +0x88 conditions.
    objs = []
    for i, on in enumerate(case['obj']):
        if not on:
            objs.append(0)
        elif i in (3, 4):
            objs.append(e.object('condition', slots={4: ('c', 0)}))
        else:
            o = e.world_alloc(0x20)
            put(o + 8, random.Random(case['seed'] * 7 + i).randrange(2), 1)
            objs.append(o)
    fields = dict(case['fields'])
    for i, o in enumerate(objs):
        fields[0x78 + 4 * i] = o
    fields[0x00] = e.object('weather', slots={0: ('c,s0', 4)}) if h['weather'] else 0
    fields[0x04] = 0
    fields[0x68] = 0x7f50 if h['event'] else 0
    fields[0x6c] = 0x7f60 if h['thread'] else 0
    fields[0xb0] = e.object('stations', slots={0x6c: ('c,s0,s1', 8)}) if h['stations'] else 0
    fields[0xb4] = e.object('object', slots={0: ('c,s0', 4)}) if h['xb4'] else 0
    fields[0xbc] = 0x7f70 if h['xbc'] else 0
    return mem, addr, fields, ia


def run_original(case):
    t = Trace()
    t.silent |= {'new'}
    e = Emu(t)
    e.cpu.fpu_control(0x007f)  # 24 bits, as in play
    for a, spec in CALLS.items():
        e.stub(a, spec=spec)

    def check(em, key, a, pop):  # an objective list's check sets its done byte
        v = t.handle(key, a)
        em.w8(a[0] + 8, v)
        em._ret(v, pop)
    e.stub(0x45e540, spec=CALLS[0x45e540], fn=check)
    # The CRT and MFC the cleanup's name uses.
    empty = e.world_string('')

    def cs_ctor(em, key, a, pop):
        em.w32(em.args('c')[0], empty)
        em._ret(em.args('c')[0], 0)

    def cs_format(em, key, a, pop):
        this, fmt = em.args('s0,s1')
        f = em.cstr(fmt)
        vals = []
        for k, m in enumerate(re.finditer(r'%([sd])', f)):
            v = em.args(f's{k + 2}')[0]
            vals.append(em.cstr(v) if m.group(1) == 's' else v)
        em.w32(this, em.world_string(c_format(f, vals)))
        em._ret(0, 0)

    def sprintf(em, key, a, pop):
        buf, fmt = em.args('s0,s1')
        f = em.cstr(fmt)
        vals = [em.args(f's{k + 2}')[0] for k, _ in enumerate(re.finditer(r'%d', f))]
        s = c_format(f, vals)
        em.cpu.write(buf, s.encode('latin1') + b'\0')
        em._ret(len(s), 0)
    e.stub(0x5abdb0, 'cs', '', 0, cs_ctor)
    e.stub(0x5abda4, 'cs~', '', 0, lambda em, k, a, p: em._ret(0, 0))
    e.stub(0x5abdaa, 'Format', '', 0, cs_format)
    e.import_stub(0x5bb658, 'sprintf', '', 0, sprintf)
    e.import_stub(0x5bb0d0, 'CloseHandle', 's0', 4)
    e.import_stub(0x5bb0f8, 'SetThreadPriority', 's0,s1', 8)

    def clouds(em, key, a, pop):
        p, dt = em.args('c,s0')
        em._ret(t.handle('0x00543a50', [em.r32(p), em.r32(p + 4), dt]), 4)
    e.stub(0x543a50, spec='', fn=clouds)

    def unload(em, key, a, pop):  # 0x490480 leaves no mission up
        t.handle(key, [])
        em.w32(THIS + 0x60, 0xffffffff)
        em._ret(0, 0)
    e.stub(0x490480, spec='', fn=unload)
    mem, addr, fields, ia = build_world(e, case)
    t.script = script_for(case, addr)
    for off, size, _ in FIELDS:
        if off in fields:
            (e.w8 if size == 1 else e.w32)(THIS + off, fields[off])
    for op in case['ops']:
        k = op[0]
        if k == 'begin':
            e.call(BEGIN, ecx=THIS)
        elif k == 'update':
            e.call(UPDATE, ecx=THIS, stack=(op[1],))
        elif k == 'complete':
            e.call(COMPLETE, ecx=THIS)
        elif k == 'lose':
            e.call(LOSE, ecx=THIS)
        elif k == 'command':
            e.call(COMMAND, ecx=op[1])
        elif k == 'ask':
            e.call(VOICE_ASK, ecx=THIS, stack=(op[1], op[2]))
        elif k == 'cleanup':
            r = e.call(CLEANUP, ecx=THIS, stack=(op[1],)) & 0xff
            t.log.append(('=cleanup', (r,)))
        t.log.append(('=fields', tuple((e.r8(THIS + o) if n == 1 else e.r32(THIS + o)) for o, n, _ in FIELDS)))
    words = tuple(e.r32(ia + o) for o in (0x68, 0x6c, 0x70)) if ia else ()
    return t, mem, addr, fields, words


def run_rebuild(probe, case, mem, addr, fields, ia):
    t = Trace(script_for(case, addr))
    t.silent |= {'new'}
    probe.probe_app_set_callback(t.callback())
    probe.probe_mission_reset()
    for a, v in mem.items():
        probe.probe_mission_poke(a, v)
    for off, size, _ in FIELDS:
        if off in fields:
            probe.probe_mission_set_field(off, fields[off])
    for op in case['ops']:
        k = op[0]
        if k == 'begin':
            probe.probe_mission_begin()
        elif k == 'update':
            probe.probe_mission_update(op[1])
        elif k == 'complete':
            probe.probe_mission_complete()
        elif k == 'lose':
            probe.probe_mission_lose()
        elif k == 'command':
            probe.probe_mission_command(op[1])
        elif k == 'ask':
            probe.probe_mission_ask_voice(op[1], op[2])
        elif k == 'cleanup':
            t.log.append(('=cleanup', (probe.probe_mission_cleanup(op[1]),)))
        t.log.append(('=fields', tuple(probe.probe_mission_field(o) & 0xffffffff for o, _, _ in FIELDS)))
    words = tuple(probe.probe_mission_peek(ia + o) for o in (0x68, 0x6c, 0x70)) if ia else ()
    return t, words


def main():
    import ctypes
    probe = load_probe()
    probe.probe_mission_field.restype = ctypes.c_uint32
    probe.probe_mission_peek.restype = ctypes.c_uint32
    for name in ('probe_mission_field', 'probe_mission_set_field', 'probe_mission_update', 'probe_mission_cleanup',
                 'probe_mission_command', 'probe_mission_ask_voice', 'probe_mission_poke', 'probe_mission_peek'):
        getattr(probe, name).argtypes = {
            'probe_mission_field': [ctypes.c_uint32], 'probe_mission_set_field': [ctypes.c_uint32, ctypes.c_uint32],
            'probe_mission_update': [ctypes.c_uint32], 'probe_mission_cleanup': [ctypes.c_int32],
            'probe_mission_command': [ctypes.c_int32], 'probe_mission_ask_voice': [ctypes.c_uint32, ctypes.c_uint32],
            'probe_mission_poke': [ctypes.c_uint32, ctypes.c_uint32], 'probe_mission_peek': [ctypes.c_uint32],
        }[name]
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 400
    calls = 0
    reached = set()
    for seed in range(count):
        case = make_case(seed)
        try:
            a, mem, addr, fields, words_a = run_original(case)
        except Exception:
            print(f'seed {seed}: the original faulted; ops {case["ops"]}')
            raise
        b, words_b = run_rebuild(probe, case, mem, addr, fields, addr['ia'])
        name = f'seed {seed} (type {case["type"]}, ops {[o[0] for o in case["ops"]]})'
        if not compare(name, a.log, b.log, ulps=ULPS):
            sys.stdout.flush()
            os._exit(1)
        if words_a != words_b:
            print(f'{name}: Instant Action\'s words differ: {words_a} (original) {words_b} (rebuild)')
            sys.stdout.flush()
            os._exit(1)
        calls += len(a.log)
        reached |= {k for k, _ in a.log}
    keys = [f'{k:#010x}' for k in CALLS] + ['CloseHandle', 'SetThreadPriority', '0x00543a50', 'weather.vt+0x0',
                                            'screen.vt+0x60', 'stations.vt+0x6c', 'action.vt+0x3c',
                                            'condition.vt+0x4', 'object.vt+0x0', 'game.vt+0x4c']
    missing = sorted(k for k in keys if k not in reached)
    print(f'{count} cases, {calls} calls and states: the rebuilt missions frame and cleanup match the original')
    if missing:
        print(f'  never reached: {", ".join(missing)}')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()
