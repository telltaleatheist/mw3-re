"""The sensors and the entity sort against the original x86 (docs/ai.md section 15).

Part 1, the sensors 0x4a8cd0 (with 0x4a8710, 0x4a83c0, 0x4a91b0, 0x4a8990,
0x4a8530, 0x4a7fd0, 0x4a9680, 0x4a9a60, 0x4a9cf0, 0x4a9980, 0x4a92c0,
0x4a9530, 0x4a7f30, 0x4a9fd0, 0x4aa270, 0x4a9e70, 0x4aa400 and the STL
pieces): crafted worlds of 1 to 26 vehicles and 0 to 5 targets, each
entity its own vehicle or another object, alive or not, with or without a
mech (BAP, C3, C3 slave, ECM), shut down, passive radar, lancemates under
orders, hidden (vt+0xb4), teams of one or two bits, ranges from 150 to
50000 (or every one in range: [0x60b570]), stale nodes, old contacts
behind the picks and the contact slots, the player, the view's vehicle and
the stealth entity anywhere or nowhere in the list. Every call out (the
vehicles' virtual calls by object, the node refreshes, the ping) and, after
the frame, every vehicle's and target's picks, kept contacts, flags, masks
and contact list, the entities' +0x09 and the slots: all compared.

Part 2, the entity sort 0x4d5ca0 (0x4d5bc0, 0x4d5be0, MSVC's sort 0x4d7360
with the comparator 0x4d7650 and its inline copy 0x4d7bc0) on lists of 0 to
40 entities (players, stealth, no vehicles, allies with lancemates of
every lancemate setting, brains named as the lance or not), then the
allies turning 0x4d5dc0: the calls, the order, the allies' range, the
flags and masks.

The x87 at 24 bits.

    cmake --build build && .venv/bin/python -P tests/differential/test_sensors.py [cases]
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import Emu, Trace, compare, load_probe  # noqa: E402
from calltrace import call_abs  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn.x86_const import UC_X86_REG_ESP  # noqa: E402

P = ctypes.c_void_p
U32 = ctypes.c_uint32
PILOT, VIEW, STEALTH = 0x60861c, 0x5f4c8c, 0x609a2c
LIST, TARGETS, SEE_ALL, SORTED = 0x736558, 0x60b580, 0x60b570, 0x60b56c
LANCE = ['Dominic Paine', 'Epona Rhi', 'Alan Mattila']
MSG = {0xe14: LANCE[0], 0xe15: LANCE[1], 0xe01: LANCE[2]}
VSLOTS = (0x1c, 0x24, 0x28, 0x2c, 0x40, 0x50, 0x54, 0xa0, 0xb4, 0xc8, 0xcc, 0xd0, 0xec, 0x140)


def die():
    sys.stdout.flush()
    os._exit(1)


def f2u(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


def u2f(u):
    return struct.unpack('<f', struct.pack('<I', u & 0xffffffff))[0]


def setup_probe(probe):
    probe.probe_sensors_poke.argtypes = [U32, U32]
    probe.probe_sensors_poke8.argtypes = [U32, U32]
    probe.probe_sensors_name.argtypes = [U32, ctypes.c_char_p]
    probe.probe_sensors_string.argtypes = [U32, ctypes.c_char_p]
    probe.probe_sensors_vehicle.argtypes = [U32]
    probe.probe_sensors_entities.argtypes = [P, ctypes.c_int32]
    probe.probe_sensors_targets.argtypes = [P, ctypes.c_int32]
    probe.probe_sensors_run.argtypes = [ctypes.c_int32, ctypes.c_int32, U32]
    probe.probe_sensors_state.argtypes = [U32, P, ctypes.c_int32]
    probe.probe_sensors_state.restype = ctypes.c_int32
    probe.probe_sensors_sensed.argtypes = [U32]
    probe.probe_sensors_sensed.restype = ctypes.c_int32
    probe.probe_sensors_list.argtypes = [P]
    probe.probe_sensors_list.restype = ctypes.c_int32
    probe.probe_sensors_forget_kind.argtypes = [U32, U32]
    probe.probe_sensors_forget.argtypes = [U32]


# The branches the cases must reach (address: what).
COVER = {
    0x4a97ae: 'player out of the other\'s range: its own contact', 0x4a9893: 'the other senses the player',
    0x4a9b17: 'lancemate: a new kept contact', 0x4a9c01: 'lancemate: the kept contact reused',
    0x4a9cca: 'lancemate: the nearer one', 0x4a9444: 'pair: a senses b', 0x4a95fa: 'seen by the player',
    0x4a962e: 'seen by an enemy', 0x4a9641: 'the target found again', 0x4a964d: 'the last ally found again',
    0x4a9655: 'the last enemy found again', 0x4a965d: 'the saved pick found again', 0x4a7cec: 'a tracked contact re-pointed',
    0x4a940f: 'the BAP test', 0x4a942a: 'pair: in sight', 0x4a9430: 'pair: not in sight',
    0x4a8178: 'C3 of another', 0x4a8a2b: 'a C3 master', 0x4a8a99: 'a C3 slave', 0x4aa04e: 'a C3 merge',
    0x4aa1b6: 'the master\'s list replaced', 0x4aa2a7: 'C3: on the master', 0x4aa2b6: 'C3: the master\'s own',
    0x4aa37d: 'C3: a shared contact', 0x4a9ec5: 'more than 16 contacts', 0x4a9f35: 'one within 8 m',
    0x4a9f65: 'the target kept', 0x4a9f7e: 'the nearest ally', 0x4a9f8f: 'the nearest of the rest',
    0x4a9fa6: 'vt+0xd0 false', 0x4aa44a: 'the ping', 0x4a8b4e: 'an entity with no vehicle',
    0x4a921e: 'a dead entity', 0x4a8895: 'the stealth entity', 0x4a8939: 'the view on another',
    0x4a8614: 'a target on C3',
}
COVER_SORT = {0x4d7887: 'the lance ranks', 0x4d786b: 'no brain first', 0x4d74ca: 'the median', 0x4d77a5: 'not lance',
              0x4d5c5a: 'lance by default', 0x4d5c27: 'lance setting', 0x4d5d6b: 'the allies end',
              0x4d7bc0: 'the partition'}
REACHED = set()


class World:
    """One case's objects, written to the emulator and mirrored for the probe."""

    def __init__(self, e):
        self.e = e
        self.pokes = []    # (addr, value, size)
        self.names = {}    # handle -> name
        self.script = {}   # trace key -> answer
        self.vehicles = []  # handles whose AiTarget the probe models
        self.strings = {}
        self.slots = []
        self.objects = []  # every entity, vehicle and target (their +0x09)

    def put(self, a, v):
        self.e.w32(a, v & 0xffffffff)
        self.pokes.append((a, v & 0xffffffff, 4))

    def put8(self, a, v):
        self.e.w8(a, v)
        self.pokes.append((a, v & 0xff, 1))

    def putf(self, a, f):
        self.put(a, f2u(f))

    def string(self, s):
        a = self.e.world_string(s)
        self.strings[a] = s
        return a

    def obj(self, name, size=0x400, real_c0=False):
        e = self.e
        o = e.world_alloc(size)
        vt = e.world_alloc(0x180)
        e.w32(o, vt)
        for off in VSLOTS:
            e.w32(vt + off, e.new_stub(f'{name}.vt+{off:#x}', 'c', 0))
        if real_c0:
            # AiTarget's set_target 0x4d5e40 behind a recorded call, except
            # from AiTarget's own pickers (the port calls those directly).
            def rec(emu, key, a, pop):
                caller = emu.r32(emu.uc.reg_read(UC_X86_REG_ESP) + 4)
                if not 0x4d5e40 <= caller < 0x4d6d40:
                    emu.t.handle(key, (a[0], 1 if a[1] else 0, a[2] & 0xff))
                emu._ret(0, 0)
            stub = e.new_stub(f'{name}.vt+0xc0', 'c,s1,s2', 0, rec)
            thunk = e.world_alloc(16)
            e.cpu.write(thunk, call_abs(thunk, stub) + b'\xe9' + struct.pack('<i', 0x4d5e40 - (thunk + 10)))
            e.w32(vt + 0xc0, thunk)
        self.names[o] = name
        return o

    def answer(self, o, slot, v):
        self.script[f'{self.names[o]}.vt+{slot:#x}'] = v & 0xffffffff


def team_bits(r):
    t = 1 << r.randrange(1, 4)
    if r.random() < 0.15:
        t |= 1 << r.randrange(1, 5)
    if r.random() < 0.05:
        t = 1
    return t


def make_node(w, r, stale_p=0.25, pos=None):
    node = w.e.world_alloc(0x80)
    data = w.e.world_alloc(0x100)
    w.put(node + 0x38, data)
    w.put(node + 0x54, 1 if r.random() < 0.7 else 0)
    w.put(data, 0x20 if r.random() < stale_p else 0)
    p = pos or [r.uniform(-1500, 1500), r.uniform(-50, 120), r.uniform(-1500, 1500)]
    if r.random() < 0.3:
        p = [float(int(x)) for x in p]
    for i in range(3):
        w.putf(data + 0x84 + 4 * i, p[i])
    return node


def build_sensor_world(seed, e):
    r = random.Random(seed)
    w = World(e)
    nveh = r.choice([1, 2, 3, 4, 6, 8, 12, 18, 26])
    ntg = r.choice([0, 0, 1, 2, 5])
    see_all = r.random() < 0.08
    cluster = r.random() < 0.3  # everything close: lists over 16, touching
    spread = 60.0 if cluster else 1500.0
    vehs, ents, mechs = [], [], []
    for i in range(nveh + ntg):
        target = i >= nveh
        x = w.obj(f't{i}' if target else f'v{i}', real_c0=True)
        w.vehicles.append(x)
        w.objects.append(x)
        w.answer(x, 0x2c, x), w.answer(x, 0x24, x), w.answer(x, 0x28, x)
        w.put8(x + 8, 0 if (target and r.random() < 0.15) else 1)
        w.put8(x + 9, r.randrange(2))
        w.put(x + 4, make_node(w, r) if r.random() < 0.8 else 0)
        pos = [r.uniform(-spread, spread), r.uniform(-20, 60), r.uniform(-spread, spread)]
        w.put(x + 0x14, make_node(w, r, pos=pos))
        rng = r.choice([150.0, 500.0, 1000.0, 1000.0, 2000.0, 50000.0])
        w.putf(x + 0x68, 10.0)
        w.putf(x + 0x9c, rng), w.putf(x + 0xa0, rng), w.putf(x + 0xa4, rng * rng)
        team = team_bits(r)
        allies = team & ~1
        if r.random() < 0.1:
            allies = r.randrange(32)
        w.put(x + 0xc0, team), w.put(x + 0xc4, allies), w.put(x + 0xc8, (~allies & 0xfffffffe) & 0xffffffff)
        w.put8(x + 0xbc, r.choice([0, 0, 0, 4, 0x10, 0x10, 0x14, 0x8, 0x3]) if not target else r.choice([0, 0, 4]))
        # A mech (vt+0x50) with its equipment, controls and brain.
        m = 0
        if not target and r.random() < 0.85 or target and r.random() < 0.1:
            m = w.obj(f'm{i}')
            for k, p in ((4, 0.2), (6, 0.12), (7, 0.25), (8, 0.15)):
                w.put(m + 0x3ac + 4 * k, r.choice([1, 2]) if r.random() < p else r.choice([0, 0, -1]))
            w.answer(m, 0x28, x)
        w.answer(x, 0x50, m)
        w.answer(x, 0x40, r.choice([m, x, 0]) if m else 0)
        w.answer(x, 0xa0, r.choice([0, 0, 0, 0, 1, 2]))
        w.answer(x, 0xb4, 1 if r.random() < 0.08 else 0)
        w.answer(x, 0xcc, 0 if r.random() < 0.08 else 1)
        w.answer(x, 0xd0, 0 if r.random() < 0.2 else 1)
        if r.random() < 0.15:  # a ping, on a vehicle with a node (every one in play)
            w.answer(x, 0xec, w.e.world_alloc(0x100))
            if not e.r32(x + 4):
                w.put(x + 4, make_node(w, r))
        mechs.append(m)
        if target:
            continue
        vehs.append(x)
        if r.random() < 0.25:  # an entity of its own whose vehicle this is
            ent = w.obj(f'e{i}')
            w.objects.append(ent)
            w.put8(ent + 9, r.randrange(2))
            w.answer(ent, 0x2c, x)
            ents.append(ent)
        else:
            ents.append(x)
    targets = w.vehicles[nveh:]
    if r.random() < 0.3:  # pairs exactly at one's range: the edge
        for _ in range(3):
            a, b = r.sample(w.vehicles, 2) if len(w.vehicles) > 1 else (w.vehicles[0], w.vehicles[0])
            if a == b:
                break
            k = round(u2f(e.r32(r.choice([a, b]) + 0xa0)))
            if k > 5000:
                continue
            pa = [float(r.randrange(-500, 500)), float(r.randrange(0, 50)), float(r.randrange(-500, 500))]
            pb = [pa[0] + k, pa[1], pa[2]]
            for x, pp in ((a, pa), (b, pb)):
                data = e.r32(e.r32(x + 0x14) + 0x38)
                for i in range(3):
                    w.putf(data + 0x84 + 4 * i, pp[i])
    # The player is the first vehicle, its own entity.
    ents[0] = vehs[0]
    player = vehs[0]
    # Entities with no vehicle.
    extra = []
    for k in range(r.choice([0, 0, 0, 1, 2])):
        o = w.obj(f'n{k}')
        w.objects.append(o)
        w.answer(o, 0x2c, 0)
        extra.append(o)
    lst = ents + extra
    r.shuffle(lst)
    if r.random() < 0.1 and len(lst) > 1:  # the player not in the list
        lst.remove(player)
    view = r.choice([player] * 6 + [r.choice(ents), 0])
    stealth = r.choice([0, 0, 0, r.choice(lst) if lst else 0, w.obj('stealth')])
    if stealth and stealth not in w.names:
        pass
    if stealth and stealth not in lst and stealth not in ents:
        w.objects.append(stealth)
        w.answer(stealth, 0x2c, r.choice([0, w.vehicles[-1]]))
    # Old contacts: some of each vehicle's view of the others, behind its picks.
    for x in w.vehicles:
        olds = []
        for y in w.vehicles:
            if y != x and r.random() < 0.3:
                c = e.world_alloc(0x30)
                w.put(c, x), w.put(c + 4, y if r.random() < 0.95 else 0), w.put(c + 8, y)
                w.putf(c + 0xc, r.uniform(0, 1e6)), w.put8(c + 0x2c, r.randrange(2))
                olds.append(c)
        arr = e.world_alloc(4 * max(1, len(olds)))
        for i, c in enumerate(olds):
            w.put(arr + 4 * i, c)
        w.put(x + 0xac, arr), w.put(x + 0xb0, arr + 4 * len(olds)), w.put(x + 0xb4, arr + 4 * len(olds))
        pick = lambda: r.choice(olds + [0]) if olds and r.random() < 0.7 else 0  # noqa: E731
        t = pick()
        w.put(x + 0x6c, t)
        w.put(x + 0x70, r.choice([t, pick()]) if r.random() < 0.5 else pick())
        w.put(x + 0x74, pick()), w.put(x + 0x78, pick())
        for off in (0x7c, 0x80, 0x94, 0x98, 0xb8):
            w.put(x + off, r.choice(olds) if olds and r.random() < 0.2 else 0)
        refs = e.world_alloc(8)  # tracked contacts, mostly none as in play
        for k in range(2):
            w.put(refs + 4 * k, r.choice(olds) if olds and r.random() < 0.15 else 0)
        w.put(x + 0x88, refs), w.put(x + 0x8c, refs + 8), w.put(x + 0x90, refs + 8)
    # The globals.
    arr = e.world_alloc(4 * max(1, len(lst)))
    for i, o in enumerate(lst):
        w.put(arr + 4 * i, o)
    w.put(LIST + 4, arr), w.put(LIST + 8, arr + 4 * len(lst)), w.put(LIST + 0xc, arr + 4 * len(lst))
    tarr = e.world_alloc(4 * max(1, len(targets)))
    for i, o in enumerate(targets):
        w.put(tarr + 4 * i, o)
    w.put(TARGETS + 4, tarr), w.put(TARGETS + 8, tarr + 4 * len(targets))
    pilot = e.world_alloc(0x100)
    w.put(pilot + 0x14, player), w.put(PILOT, pilot)
    vb = e.world_alloc(0x100)
    w.put(vb + 0x64, view), w.put(VIEW, vb)
    w.put(STEALTH, stealth)
    w.put8(SEE_ALL, 1 if see_all else 0)
    w.lst, w.targets, w.see_all = lst, targets, see_all
    return w


def common_stubs(e, w):
    e.heap_stubs()
    t = e.t
    e.stub(0x51e9d0, '@push', 'c')
    e.stub(0x51fde0, '@ident', '')
    e.stub(0x51ea00, '@pop', '')
    e.stub(0x4ed280, 'refresh', 'c')

    def ping(emu, key, a, pop):
        s = a[0] - 0xe8
        p = a[1]
        v = emu.t.handle(key, (s, emu.r32(p), emu.r32(p + 4), emu.r32(p + 8)))
        emu._ret(v, pop)
    e.stub(0x4adce0, '0x004adce0', 'c,s0', None, ping)
    msgs = {i: w.string(s) for i, s in MSG.items()}
    w.script['0x00576040'] = lambda n, a: msgs.get(a[0], 0)
    e.stub(0x576040, '0x00576040', 'c')
    t.script.update(w.script)


def read_contact(e, c, owner_list):
    if not c:
        return [0]
    out = [1, e.r32(c), e.r32(c + 4), e.r32(c + 8), e.r32(c + 0xc), e.r32(c + 0x10), e.r32(c + 0x14),
           e.r32(c + 0x18), e.r32(c + 0x1c), e.r8(c + 0x2c) & 1]
    idx = 0xffffffff
    if owner_list is not None and c in owner_list:
        idx = owner_list.index(c)
    return out + [idx]


def state_original(e, x):
    b, en = e.r32(x + 0xac), e.r32(x + 0xb0)
    lst = [e.r32(a) for a in range(b, en, 4)] if b else []
    out = []
    refs = e.r32(x + 0x88)
    for a in [x + 0x6c, x + 0x70, x + 0x74, x + 0x78, x + 0x7c, x + 0x80, refs, refs and refs + 4, x + 0x94,
              x + 0x98, x + 0xb8]:
        out += read_contact(e, e.r32(a) if a else 0, lst)
    out += [e.r8(x + 0xbc), e.r32(x + 0xc0), e.r32(x + 0xc4), e.r32(x + 0xc8), len(lst)]
    for c in lst:
        out += read_contact(e, c, None)
    return out


def hook_cover(e, cover):
    def hit(uc, address, size, user):
        REACHED.add(address)
    for a in cover:
        e.uc.hook_add(UC_HOOK_CODE, hit, begin=a, end=a)


def sensors_original(seed):
    t = Trace()
    e = Emu(t)
    e.cpu.fpu_control(0x007f)
    w = build_sensor_world(seed, e)
    common_stubs(e, w)
    hook_cover(e, COVER)
    t.silent.add('0x00576040')
    e.call(0x4a8c30)  # the C3 scratch's static set-up (32 nulls)
    t.reset()
    e.call(0x4a8cd0)
    states = {x: state_original(e, x) for x in w.vehicles}
    sensed = {o: e.r8(o + 9) for o in w.objects}
    return t, w, states, sensed


def load_probe_world(probe, w):
    probe.probe_sensors_reset()
    for a, v, size in w.pokes:
        (probe.probe_sensors_poke if size == 4 else probe.probe_sensors_poke8)(a, v)
    for h, n in w.names.items():
        probe.probe_sensors_name(h, n.encode())
    for a, s in w.strings.items():
        probe.probe_sensors_string(a, s.encode('latin1'))
    for x in w.vehicles:
        probe.probe_sensors_vehicle(x)
    arr = (U32 * max(1, len(w.lst)))(*w.lst)
    probe.probe_sensors_entities(arr, len(w.lst))


def state_rebuild(probe, x):
    buf = (U32 * 20000)()
    n = probe.probe_sensors_state(x, buf, 20000)
    return list(buf[:n])


def sensors_rebuild(probe, w):
    t = Trace(dict(w.script))
    t.silent.add('0x00576040')
    probe.probe_app_set_callback(t.callback())
    load_probe_world(probe, w)
    tarr = (U32 * max(1, len(w.targets)))(*w.targets)
    probe.probe_sensors_targets(tarr, len(w.targets))
    probe.probe_sensors_run(0, 1 if w.see_all else 0, 0)
    states = {x: state_rebuild(probe, x) for x in w.vehicles}
    sensed = {}
    initial = {a: v for a, v, size in w.pokes if size == 1}
    for o in w.objects:
        s = probe.probe_sensors_sensed(o)
        sensed[o] = initial.get(o + 9, 0) if s < 0 else s
    return t, states, sensed


def describe(st):
    return ' '.join(f'{v:#x}' for v in st[:60])


def run_sensors(probe, count):
    calls = 0
    for seed in range(count):
        a, w, sa, ea = sensors_original(seed)
        b, sb, eb = sensors_rebuild(probe, w)
        name = f'sensors seed {seed} ({len(w.vehicles) - len(w.targets)} vehicles, {len(w.targets)} targets)'
        if not compare(name, a.log, b.log, ulps=None):
            die()
        for x in w.vehicles:
            if sa[x] != sb[x]:
                print(f'{name}: {w.names[x]} differs\n  original {describe(sa[x])}\n  rebuild  {describe(sb[x])}')
                die()
        if ea != eb:
            d = [w.names.get(o, hex(o)) for o in ea if ea[o] != eb[o]]
            print(f'{name}: +0x09 differs on {d}')
            die()
        calls += len(a.log)
    missing = [v for k, v in COVER.items() if k not in REACHED]
    if missing:
        print(f'  part 1 never reached: {", ".join(missing)}')
    print(f'  part 1: {count} frames, {calls} calls, every list, pick and flag identical')


# --- part 3: the forget ------------------------------------------------------------------------
COVER_FORGET = {0x4aa651: 'itself', 0x4aa4f8: 'the player contact on it', 0x4aa526: 'the lance contact on it',
                0x4aa54b: 'the target on it', 0x4aa56a: 'the last ally on it', 0x4aa57d: 'the last enemy on it',
                0x4aa590: 'the saved pick on it', 0x4aa5a6: 'the touching one on it', 0x4aa60e: 'a contact kept',
                0x4aa5f1: 'a contact dropped', 0x4aeed2: 'a vehicle with weapons', 0x4aeee6: 'a vehicle with controls'}


def forget_world(seed, e, w):
    """Each vehicle's vt+0xd4: the base 0x4aa4d0, or a vehicle's 0x4aeec0
    with weapons (0x4b0370 stubbed) and controls (vt+0x60 stubbed) or not."""
    r = random.Random(seed * 7 + 1)
    kinds = {}
    for x in w.vehicles:
        vt = e.r32(x)
        if r.random() < 0.5:
            e.w32(vt + 0xd4, 0x4aa4d0)
            continue
        e.w32(vt + 0xd4, 0x4aeec0)
        mgr = e.world_alloc(0x80) if r.random() < 0.7 else 0
        w.put(x + 0xd4, mgr)
        ctl = 0
        if r.random() < 0.7:
            ctl = w.obj(f'k{len(kinds)}')
            cvt = e.r32(ctl)
            e.w32(cvt + 0x60, e.new_stub(f'k{len(kinds)}.vt+0x60', 'c,s0', 4))
        w.answer(x, 0x140, ctl)
        kinds[x] = mgr
    # Who goes: a vehicle, an entity, a target, the player, a stranger.
    pool = list(w.vehicles) + list(w.lst) + [e.world_alloc(0x10)]
    return kinds, r.choice(pool)


def forget_original(seed):
    t = Trace()
    e = Emu(t)
    e.cpu.fpu_control(0x007f)
    w = build_sensor_world(seed, e)
    kinds, x = forget_world(seed, e, w)
    common_stubs(e, w)
    e.stub(0x4b0370, '0x004b0370', 'c,s0')
    e.stub(0x4d5100, '0x004d5100', 'c')
    hook_cover(e, COVER_FORGET)
    t.silent.add('0x00576040')
    e.call(0x4a8c30)
    e.call(0x4a8cd0)
    t.reset()
    e.call(0x4aa6b0, ecx=x)
    states = {v: state_original(e, v) for v in w.vehicles}
    return t, w, kinds, x, states


def run_forgets(probe, count):
    calls = 0
    for seed in range(count):
        a, w, kinds, x, sa = forget_original(seed)
        t = Trace(dict(w.script))
        t.silent.add('0x00576040')
        probe.probe_app_set_callback(t.callback())
        load_probe_world(probe, w)
        tarr = (U32 * max(1, len(w.targets)))(*w.targets)
        probe.probe_sensors_targets(tarr, len(w.targets))
        for v, mgr in kinds.items():
            probe.probe_sensors_forget_kind(v, mgr)
        probe.probe_sensors_run(0, 1 if w.see_all else 0, 0)
        t.reset()
        probe.probe_sensors_forget(x)
        name = f'forget seed {seed} ({w.names.get(x, "a stranger")})'
        if not compare(name, a.log, t.log):
            die()
        for v in w.vehicles:
            sb = state_rebuild(probe, v)
            if sa[v] != sb:
                print(f'{name}: {w.names[v]} differs\n  original {describe(sa[v])}\n  rebuild  {describe(sb)}')
                die()
        calls += len(a.log)
    missing = [v for k, v in COVER_FORGET.items() if k not in REACHED]
    if missing:
        print(f'  part 3 never reached: {", ".join(missing)}')
        die()
    print(f'  part 3: {count} forgets, {calls} calls, every list, pick and kept contact identical')


# --- part 2: the entity sort ---------------------------------------------------------------
def build_sort_world(seed, e):
    r = random.Random(seed)
    w = World(e)
    n = r.choice([0, 1, 2, 3, 5, 8, 16, 17, 24, 40])
    player_team = 1 << r.randrange(1, 3)
    lst = []
    for i in range(n):
        x = w.obj(f'v{i}')
        w.vehicles.append(x)
        w.objects.append(x)
        w.answer(x, 0x2c, x if r.random() < 0.9 else 0)
        ent = x
        if r.random() < 0.3:  # an entity of its own whose vehicle this is (made below)
            ent = ('e', i, x)
        team = player_team if r.random() < 0.6 else team_bits(r)
        allies = team & ~1
        w.put(x + 0xc0, team), w.put(x + 0xc4, allies), w.put(x + 0xc8, (~allies & 0xfffffffe) & 0xffffffff)
        w.put8(x + 0xbc, r.choice([0, 0x10, 0x14, 0x4, 0xff]))
        w.answer(x, 0x28, x)
        m = w.obj(f'm{i}') if r.random() < 0.85 else 0
        if m and r.random() < 0.3:
            m = x  # a mech is its own (vt+0x50 answers itself)
        w.answer(x, 0x50, m)
        w.answer(x, 0x40, r.choice([m, m, x, 0]) if m else r.choice([0, x]))
        for o in dict.fromkeys((m, x)):
            if not o:
                continue
            w.answer(o, 0x28, x)
            c = w.obj(f'c{i}{"m" if o == m else "v"}') if r.random() < 0.8 else 0
            w.answer(o, 0x140, c)
            if c:
                b = e.world_alloc(0x100) if r.random() < 0.85 else 0
                w.answer(c, 0x54, b)
                if b:
                    w.put8(b + 0xcd, r.choice([0, 1, 2, 3, 2]))
                    nm = r.choice(LANCE + LANCE + ['Captain Cameron', 'Dominic'])
                    w.put(b + 0xd4, w.string(nm))
        lst.append(ent)
    # The entities of their own, made in another order than their vehicles.
    own = [k for k, o in enumerate(lst) if isinstance(o, tuple)]
    r.shuffle(own)
    for k in own:
        _, i, x = lst[k]
        ent = w.obj(f'e{i}')
        w.answer(ent, 0x2c, x if r.random() < 0.95 else 0)
        lst[k] = ent
    selfs = [x for x in lst if x in w.vehicles]
    player = r.choice(selfs + selfs[:1] * 3) if selfs else 0
    if player:
        w.put(player + 0xc0, player_team)
        w.put(player + 0xc4, player_team & ~1 | (2 if r.random() < 0.3 else 0))
    stealth = r.choice([0, 0, r.choice(lst)]) if lst else 0
    if r.random() < 0.1 and player in lst:
        lst.remove(player)
    if not player:
        player = e.world_alloc(0x100)
        w.vehicles.append(player)
    arr = e.world_alloc(4 * max(1, len(lst)))
    for i, o in enumerate(lst):
        w.put(arr + 4 * i, o)
    w.put(LIST + 4, arr), w.put(LIST + 8, arr + 4 * len(lst)), w.put(LIST + 0xc, arr + 4 * len(lst))
    pilot = e.world_alloc(0x100)
    w.put(pilot + 0x14, player), w.put(PILOT, pilot)
    w.put(STEALTH, stealth)
    w.put8(SORTED, 0)
    w.lst, w.targets, w.see_all, w.arr, w.turn = lst, [], False, arr, r.random() < 0.3
    return w


def sort_original(seed):
    t = Trace()
    e = Emu(t)
    w = build_sort_world(seed, e)
    common_stubs(e, w)
    hook_cover(e, COVER_SORT)
    e.call(0x4d5ca0)
    if w.turn:
        e.call(0x4d5dc0)
    n = len(w.lst)
    out = [e.r32(w.arr + 4 * i) for i in range(n)]
    first, end = e.r32(0x60b550), e.r32(0x60b548)
    out += [(first - w.arr) // 4, (end - w.arr) // 4, e.r8(SORTED)]
    states = {x: state_original(e, x) for x in w.vehicles}
    return t, w, out, states


def sort_rebuild(probe, w):
    t = Trace(dict(w.script))
    probe.probe_app_set_callback(t.callback())
    load_probe_world(probe, w)
    probe.probe_sensors_run(1, 0, 0)
    if w.turn:
        probe.probe_sensors_run(2, 0, 0)
    buf = (U32 * 128)()
    n = probe.probe_sensors_list(buf)
    states = {x: state_rebuild(probe, x) for x in w.vehicles}
    return t, list(buf[:n]), states


def run_sorts(probe, count):
    calls = 0
    for seed in range(count):
        a, w, la, sa = sort_original(seed)
        b, lb, sb = sort_rebuild(probe, w)
        name = f'sort seed {seed} ({len(w.lst)} entities)'
        if not compare(name, a.log, b.log):
            die()
        if la != lb:
            print(f'{name}: list {[w.names.get(x, hex(x)) for x in la[:-3]]} {la[-3:]}\n'
                  f'       rebuild {[w.names.get(x, hex(x)) for x in lb[:-3]]} {lb[-3:]}')
            die()
        for x in w.vehicles:
            if sa[x] != sb[x]:
                print(f'{name}: {w.names.get(x, hex(x))} differs\n  original {describe(sa[x])}\n  rebuild  {describe(sb[x])}')
                die()
        calls += len(a.log)
    missing = [v for k, v in COVER_SORT.items() if k not in REACHED]
    if missing:
        print(f'  part 2 never reached: {", ".join(missing)}')
    print(f'  part 2: {count} sorts, {calls} calls, every order, range and flag identical')


def main():
    probe = load_probe()
    setup_probe(probe)
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 1500
    run_sensors(probe, count)
    run_sorts(probe, max(200, count // 3))
    run_forgets(probe, max(300, count // 3))
    print('the rebuilt sensors, entity sort and forget match the original')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()
