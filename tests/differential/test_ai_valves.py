"""valves.zrd against the original x86 (docs/ai.md section 13): the mission
start's load 0x4e2120 (the record object 0x4e1d00, the loop 0x4e20d0, the file
0x4e5290 / 0x4e5e30, the record 0x4e2190 / 0x4e2330, the build 0x4e2fb0 /
0x4e44b0 with every action's constructor and the conditions 0x4e40e0,
0x4e4d20..0x4e4f90, 0x4e5080, 0x4e4080), then a script of valve sets
(0x4d3c00, firing 0x4d3bf0 and the actions' 0x4e3040..0x4e3fb0, the lists
0x4d4320 / 0x4d41a0) and timer ticks (0x4d3d30, 0x4d41e0, 0x4e3e60,
0x4e3f10), then the mission's end (0x4d3fa0, 0x4d3ef0).

Every mission's valves.zrd, set valve by valve, then made-up files that use
every tag (the shipped ones use eight). The world the actions reach is fake:
names to objects whose vtable slots answer from a table, brains and targets
whose words are compared at the end. Both sides write one stream (the
registry, each valve's value or condition and its reactions, the timers,
after the load and after every step) and log every call out.

    cmake --build build && .venv/bin/python tests/differential/test_ai_valves.py [made-up]
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from ai_world import ROOT, Emulated, archive, fnv, mission_archives, ser  # noqa: E402
from original import STUBS  # noqa: E402
import zrd  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
P = ctypes.c_void_p
probe.probe_valves_reentries.restype = ctypes.c_int
probe.probe_valves_world.argtypes = [P, ctypes.c_int, P, ctypes.c_int, P, ctypes.c_int]
probe.probe_valves_run.argtypes = [ctypes.c_char_p, P, ctypes.c_int, P, ctypes.c_int, P, ctypes.c_int, P,
                                   ctypes.c_int, P]
WORK = ROOT / 'build' / 'ai_valves'
CAP, LOGCAP = 1 << 22, 1 << 20
FILL = 0xcd
VALVE_FILE, TIMERS, GAME_TIME, X60B56C = 0x60b668, 0x60b514, 0x60991c, 0x60b56c
GLOBALS = [(TIMERS, 4), (VALVE_FILE, 0x108), (0x60b770, 0x68)]
WORDS = (0x3c, 0x40, 0x98, 0x9c, 0xa0, 0xdc, 0xc0, 0xc4, 0xc8)
COMPOSITE, VALUE = 0x5ccd70, 0x5cd558
CONDITIONS = (0x5cd568, 0x5cd578, 0x5cd588, 0x5cd598)
SLOTS = (0x2c, 0x40, 0x50, 0x70, 0x140, 0x54)


def u32(b):
    return struct.unpack('<I', b)[0]


class Out:
    def __init__(self):
        self.b = bytearray()

    def u(self, v):
        self.b += struct.pack('<I', v & 0xffffffff)

    def s(self, b):
        if b is None:
            self.u(0xffffffff)
        else:
            self.u(len(b))
            self.b += b


# --- the fake world ---------------------------------------------------------------
class World:
    """Objects named in the files, each with answers by vtable slot; brains
    with one order whose net part answers the order's net."""

    def __init__(self, rng, names):
        self.rng = rng
        self.names = {}     # name -> object or 0 (the targets' lookup)
        self.entities = {}  # name -> object or 0 (zEntity's)
        self.answers = {}   # (object, slot) -> answer
        self.objects = []
        self.brains = []
        self.names_list = names

    def lay(self, e):
        """Allocates the objects in the emulator's memory (filled with 0xcd)."""
        r = self.rng
        pool = []
        for _ in range(8):
            o = e.alloc(0x200)
            e.cpu.write(o, bytes([FILL]) * 0x200)
            pool.append(o)
        self.objects = pool
        self.brains = pool[5:]
        for o in pool:
            for slot in SLOTS:
                pick = r.random()
                if slot == 0x54:
                    self.answers[(o, slot)] = r.choice(self.brains) if pick < 0.8 else 0
                elif slot == 0x50:
                    self.answers[(o, slot)] = (0x7000000 | r.getrandbits(16)) if pick < 0.5 else 0
                elif slot == 0x70:
                    self.answers[(o, slot)] = 0
                else:
                    self.answers[(o, slot)] = r.choice(pool[:5]) if pick < 0.85 else 0
        for b in self.brains:
            self.answers[(b, 0x1000)] = 0x8000000 | r.getrandbits(16)
        for n in self.names_list:
            self.names[n] = r.choice(pool[:5]) if r.random() < 0.8 else 0
            self.entities[n] = r.choice(pool[:5]) if r.random() < 0.7 else 0
        # A scan's vehicle must have a brain: the exe writes through none
        # when it fires (FIX (bug)). 'scanner' leads to one.
        self.names['scanner'] = pool[0]
        self.answers[(pool[0], 0x40)] = pool[1]
        self.answers[(pool[1], 0x140)] = pool[2]
        self.answers[(pool[2], 0x54)] = self.brains[0]


# --- the original ---------------------------------------------------------------
class ValvesRun:
    def __init__(self, emu):
        self.e = emu
        self.stub_fns = {}

    def stub(self, addr, fn, pop=0):
        self.e.cpu.stub_function(addr, fn, pop)

    def vslot(self, fn, pop=0):
        """A stub to put in a fake vtable."""
        cpu = self.e.cpu
        a = STUBS + 16 * len(cpu.stubs)
        cpu.stubs[a] = (fn, pop)
        return a

    def install(self, world):
        e, cpu = self.e, self.e.cpu
        w = world
        ev = e.log
        self.stub(0x50b450, lambda c: (ev(5, fnv(e.cstr(c.ecx))), 0x6000000 | (fnv(e.cstr(c.ecx)) & 0xffffff))[1])
        self.stub(0x508c10, lambda c: ev(9, c.ecx), 0xc)
        self.stub(0x56e7e0, lambda c: ev(8, c.ecx), 8)

        def target(c):
            n = e.cstr(c.ecx)
            ev(6, fnv(n) if n is not None else 0)
            return w.names.get(n.decode('latin-1') if n is not None else None, 0)
        self.stub(0x4d7330, target)

        def entity(c):
            n = e.cstr(c.arg(0))
            ev(7, fnv(n) if n is not None else 0)
            return w.entities.get(n.decode('latin-1') if n is not None else None, 0)
        self.stub(0x5a8210, entity, 4)
        self.stub(0x41cfa0, lambda c: ev(10, c.ecx, c.arg(0)), 4)
        self.stub(0x41cc30, lambda c: ev(11, c.ecx, c.arg(0) & 0xff), 4)
        self.stub(0x48f730, lambda c: ev(12))
        self.stub(0x48f710, lambda c: ev(13))
        # The fake objects' vtables: every slot logs (16, slot, object).
        vt = e.alloc(0x200)
        for slot in SLOTS:
            pop = 0
            cpu.u32(vt + slot, self.vslot(lambda c, s=slot: (ev(16, s, c.ecx), w.answers.get((c.ecx, s), 0))[1], pop))

        def place(c):
            p, yaw = c.arg(0), c.arg(1)
            ev(17, c.ecx, e.u32(p), e.u32(p + 4), e.u32(p + 8), yaw)
            return 0
        cpu.u32(vt + 0xdc, self.vslot(place, 8))
        cpu.u32(vt + 0x4, self.vslot(lambda c: ev(18, c.ecx, c.arg(0)), 4))
        for o in w.objects:
            cpu.u32(o, vt)
        # Each brain's deque holds one order whose net part answers its net.
        nvt = e.alloc(0x10)
        for b in w.brains:
            blk = e.alloc(0x98)
            part = e.alloc(0x10)
            cpu.u32(part, nvt)
            cpu.u32(part + 4, b)
            cpu.u32(blk + 0xc, part)
            cpu.u32(b + 0x174, blk), cpu.u32(b + 0x178, blk + 0xf70), cpu.u32(b + 0x17c, blk + 0x98)
            cpu.u32(b + 0x180, e.alloc(8))

        def order_net(c):
            b = e.u32(c.ecx + 4)
            ev(15, b)
            return w.answers.get((b, 0x1000), 0)
        cpu.u32(nvt + 8, self.vslot(order_net))

    def valve_list(self):
        return self.e.valves()

    def token(self, v):
        vl = self.valve_list()
        return 0 if v == 0 else vl.index(v) + 1 if v in vl else 0xdead

    def get(self, holder):
        cpu = self.e.cpu
        cpu.call(self.e.u32(self.e.u32(holder) + 4), ecx=holder)
        return self.e.eax()

    def put_reaction(self, o, r):
        e, u = self.e, self.e.u32
        if r == 0:
            return o.u(0)
        k = u(r)
        o.u(k)
        if k == COMPOSITE:
            head = u(r + 8)
            items, p = [], u(head)
            while p != head:
                items.append(u(p + 8))
                p = u(p)
            o.u(len(items))
            for x in items:
                self.put_reaction(o, x)
        elif k in (0x5cd458, 0x5cd468, 0x5cd478, 0x5cd488):
            o.u(u(r + 4)), o.u(u(r + 8))
        elif k == 0x5cd498:
            o.u(u(r + 4)), o.u(u(r + 8)), o.u(u(r + 0xc)), o.u(u(r + 0x10))
        elif k == 0x5cd4a8:
            o.u(u(r + 4)), o.u(u(r + 8))
        elif k in (0x5cd4b8, 0x5cd4c8, 0x5cd4d8, 0x5cd4e8, 0x5cd4f8):
            o.s(e.cstr(u(r + 4))), o.u(u(r + 8))
            if k == 0x5cd4c8:
                o.u(e.u8(r + 0xc))
            elif k in (0x5cd4d8, 0x5cd4e8):
                o.u(u(r + 0xc))
            elif k == 0x5cd4f8:
                for off in (0xc, 0x10, 0x14, 0x18):
                    o.u(u(r + off))
        elif k == 0x5cd508:
            o.u(self.token(u(r + 4)))
        elif k == 0x5cd528:
            o.u(self.token(u(r + 4))), o.u(u(r + 8))
        elif k == 0x5cd518:
            o.u(self.token(u(r + 4))), o.u(u(r + 8)), o.u(u(r + 0xc))
        else:
            raise SystemExit(f'reaction vtable {k:#x}')

    def put_valves(self, o):
        e, u = self.e, self.e.u32
        vl = self.valve_list()
        o.u(len(vl))
        for v in vl:
            o.s(e.cstr(u(v)))
            h = u(v + 4)
            k = u(h)
            if k in CONDITIONS:
                b, end = u(h + 8), u(h + 0xc)
                o.u(k), o.u((end - b) // 4)
                for p in range(b, end, 4):
                    o.u(self.token(u(p)))
            else:
                assert k == VALUE, hex(k)
                o.u(VALUE)
            o.u(self.get(h))
            self.put_reaction(o, u(v + 8))
        self.put_reaction(o, u(TIMERS))

    def run(self, files, world, ops):
        e, cpu = self.e, self.e.cpu
        e.reset(files)
        for a, n in GLOBALS:
            cpu.write(a, bytes(n))
        cpu.write(X60B56C, b'\x01')
        cpu.write(GAME_TIME, bytes(4))
        cpu.call(0x4e1cb0)  # valves.zrd's file object (0x0060b668)
        world.lay(e)
        words = [(obj, off) for obj in world.objects for off in WORDS]
        self.install(world)
        o = Out()
        cpu.call(0x4e2120)
        self.put_valves(o)
        for op, a, b in ops:
            if op == 0:
                for v in self.valve_list():
                    if fnv(e.cstr(e.u32(v))) == a:
                        cpu.call(0x4d3c00, ecx=v, stack_args=(b,))
                        o.u(e.eax())
                        break
            elif op == 1:
                cpu.u32(GAME_TIME, b)
                cpu.call(0x4d3d30, stack_args=(0,))
            elif op == 2:
                cpu.u32(GAME_TIME, b)
            elif op == 3:
                cpu.call(0x4d3fa0)
            o.u(0x55555555)
            self.put_valves(o)
        for obj, off in words:
            o.u(e.u32(obj + off))
        o.u(e.u8(X60B56C))
        return bytes(o.b), e.events, words


# --- the port ----------------------------------------------------------------------
def port_events(buf, n):
    out, i = [], 0
    while i < n:
        k = buf[i]
        out.append(tuple(buf[i + 1:i + 1 + k]))
        i += 1 + k
    return out


def run_port(files, world, ops, words):
    path = WORK / 'valves.zbd'
    path.write_bytes(archive(files))
    names = [(fnv(n.encode('latin-1')), o) for n, o in world.names.items()]
    ents = [(fnv(n.encode('latin-1')), o) for n, o in world.entities.items()]
    ans = [(o, s, v) for (o, s), v in world.answers.items()]
    nb = (ctypes.c_uint32 * max(2 * len(names), 1))(*[x for p in names for x in p])
    eb = (ctypes.c_uint32 * max(2 * len(ents), 1))(*[x for p in ents for x in p])
    ab = (ctypes.c_uint32 * max(3 * len(ans), 1))(*[x for p in ans for x in p])
    probe.probe_valves_world(nb, len(names), eb, len(ents), ab, len(ans))
    ob = (ctypes.c_uint32 * max(3 * len(ops), 1))(*[x & 0xffffffff for p in ops for x in p])
    wb = (ctypes.c_uint32 * max(2 * len(words), 1))(*[x for p in words for x in p])
    out = (ctypes.c_uint8 * CAP)()
    log = (ctypes.c_uint32 * LOGCAP)()
    nlog = ctypes.c_int()
    r = probe.probe_valves_run(str(path).encode(), ob, len(ops), wb, len(words), out, CAP, log, LOGCAP,
                               ctypes.byref(nlog))
    if r < 0:
        raise SystemExit(f'probe_valves_run: {r}')
    return bytes(out[:r]), port_events(log, nlog.value)


# --- cases ---------------------------------------------------------------------------
def records(data):
    root, _ = zrd.node(data, 0)
    top = root[0] if isinstance(root, list) and root and isinstance(root[0], list) else []
    return [(top[i], top[i + 1]) for i in range(0, len(top) - 1, 2)]


def names_in(v, out):
    if isinstance(v, list):
        for x in v:
            names_in(x, out)
    elif isinstance(v, str):
        out.add(v)


def fbits(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def script(rng, valve_names, steps=40):
    """Sets of every valve to values the records name, ticks forward in time."""
    ops, t = [], 0.0
    for _ in range(steps):
        p = rng.random()
        if p < 0.6 and valve_names:
            ops.append((0, fnv(rng.choice(valve_names).encode('latin-1')), rng.choice([0, 1, 1, 2, 3, -1, 5])))
        elif p < 0.9:
            t += rng.choice([0.0, 0.5, 1.0, 4.0, 10.0, 25.0])
            ops.append((1, 0, fbits(t)))
        else:
            ops.append((2, 0, fbits(t + rng.choice([0.0, 2.0]))))
    if rng.random() < 0.5:
        ops.append((3, 0, 0))
    return ops


TAGS = ['value', 'sound', 'sound_once', 'animation', 'animation_once', 'scan_aim', 'scan_hit', 'scan_time',
        'canleave', 'shutdown', 'missionreset', 'missionabort', 'missioncomplete', 'destroy', 'setteam',
        'attack_strategy', 'update', 'delayupdate', 'any_nonzero', 'any_zero', 'all_nonzero', 'all_zero', 'teleport']
ENTITIES = ['owens_01', 'veh_a', 'mech_b', 'nobody', 'mobilehq_01', 'scanner']


def made_up(rng, k):
    vnames = [f'v{i}' for i in range(rng.randrange(2, 9))]
    recs = []
    for _ in range(rng.randrange(1, 12)):
        # Updates go to later valves and conditions read earlier ones: a valve
        # that updates itself, directly or round a loop, recurses until the
        # stack runs out, in the exe as in the port (fixes.md, candidates).
        vi = rng.randrange(len(vnames))
        name = vnames[vi]
        later, earlier = vnames[vi + 1:], vnames[:vi]
        body = []
        for tag in rng.sample(TAGS, rng.randrange(1, 4)):
            ent = rng.choice(ENTITIES + [0])
            n = rng.choice([0, 1, 1, 2, -1])
            if tag == 'value' or tag.startswith('mission'):
                body += [tag, [n]]
            elif tag in ('sound', 'sound_once', 'animation', 'animation_once'):
                body += [tag, [rng.choice(['snd_a', 'snd_b', 'anim_c', 0]), n]]
            elif tag == 'destroy':
                body += [tag, [ent, n]]
            elif tag in ('scan_aim', 'scan_hit', 'scan_time', 'canleave'):
                body += [tag, ['scanner', rng.choice([0, 1, 7]), n]]
            elif tag == 'shutdown':
                body += [tag, [ent, rng.choice([0, 1, 7]), n]]
            elif tag in ('setteam', 'attack_strategy'):
                body += [tag, [ent, rng.choice([0, 1, 2, 3, 0x22, 33]), n]]
            elif tag == 'update':
                body += [tag, [rng.choice(later + [0])]]
            elif tag == 'delayupdate':
                body += [tag, [rng.choice(vnames + [0]), rng.choice([0.0, 1.0, 2.5, 7, 30.0])]]
            elif tag == 'teleport':
                body += [tag, [ent, [rng.uniform(-900, 900), rng.uniform(-10, 60), rng.uniform(-900, 900)],
                               rng.choice([0.0, 1.5, -2.0, 3]), n]]
            else:  # a condition: its namelist
                pool = earlier + ['w_new']
                members = rng.sample(pool, min(len(pool), rng.randrange(0, 4))) + ([0] if rng.random() < 0.1 else [])
                body += [tag, 'namelist', members] if rng.random() < 0.8 else [tag, 'x']
        recs.append((name, body))
    flat = []
    for n, b in recs:
        flat += [n, b]
    return f'made-up {k}', ser([flat]), vnames


# Files the original crashes on (it reads a value as a list or a string),
# which the port reads (FIX (bug), fixes.md).
BROKEN = [
    [['v0', ['sound', 5]]], [['v0', ['value', 7]]], [['v0', ['any_nonzero', 'namelist', 'namelist', ['a']]]],
    [['v0', ['shutdown', [5, 1, 1]]]], [['v0', ['namelist', [1, 2]]]], [['v0', 5]],
]


def main():
    WORK.mkdir(parents=True, exist_ok=True)
    rng = random.Random(3)
    n_made = int(sys.argv[1]) if len(sys.argv) > 1 else 300
    emu = Emulated()
    cases = fails = 0
    seen = {}
    kinds = {1: 'reports', 4: 'sound lookups', 5: 'animation lookups', 6: 'target lookups', 7: 'entity lookups',
             8: 'sounds played', 9: 'animations run', 10: 'mechs destroyed', 11: 'powers', 12: 'aborts',
             13: 'completes', 15: 'order nets', 16: 'object calls', 17: 'teleports', 18: 'set nets'}
    work = []
    for path, files in mission_archives():
        if 'valves.zrd' in files:
            recs = records(files['valves.zrd'])
            names = set()
            for k, v in recs:
                names_in(v, names)
            vnames = sorted({k for k, _ in recs if isinstance(k, str)} | names)
            work.append((f'{path.parent.name}/{path.name}', files['valves.zrd'], vnames, sorted(names)))
    shipped = len(work)
    crashes = reentries = 0
    for k, recs in enumerate(BROKEN):
        flat = []
        for n, b in recs:
            flat += [n, b]
        work.append((f'broken {k}', ser([flat]), ['v0'], ENTITIES))
    for k in range(n_made):
        label, data, vnames = made_up(rng, k)
        work.append((label, data, vnames, ENTITIES))
    for label, data, vnames, enames in work:
        world = World(random.Random(label), enames)
        ops = script(random.Random(label + 'ops'), vnames)
        cases += 1
        files = {'valves.zrd': data}
        try:
            mine, mlog, words = ValvesRun(emu).run(files, world, ops)
        except Exception as ex:
            if not label.startswith('broken'):
                raise
            run_port(files, world, ops, [])  # the port must read it
            crashes += 1
            emu = Emulated()
            continue
        port, plog = run_port(files, world, ops, words)
        reentries += probe.probe_valves_reentries()
        for ev in mlog:
            if ev[0] in kinds:
                seen[kinds[ev[0]]] = seen.get(kinds[ev[0]], 0) + 1
        seen['timer states'] = seen.get('timer states', 0) + mine.count(struct.pack('<I', 0x5cd518))
        if mine != port or mlog != plog:
            fails += 1
            if fails <= 6:
                i = next((j for j in range(min(len(mine), len(port))) if mine[j] != port[j]), min(len(mine), len(port)))
                print(f'MISMATCH {label}: stream {len(mine)} vs {len(port)} bytes, first difference at {i}; '
                      f'logs {len(mlog)} vs {len(plog)}')
                print('  original', mine[max(0, i - 16):i + 32].hex())
                print('  port    ', port[max(0, i - 16):i + 32].hex())
                if mlog != plog:
                    j = next((q for q in range(min(len(mlog), len(plog))) if mlog[q] != plog[q]), min(len(mlog), len(plog)))
                    print('  log original', mlog[max(0, j - 2):j + 4], '\n      port    ', plog[max(0, j - 2):j + 4])
        emu = Emulated()
    if fails:
        raise SystemExit(f'ai valves: {fails} of {cases} cases differ')
    print('  ' + ', '.join(f'{k} {v}' for k, v in sorted(seen.items())))
    # The re-entry guard (FIX (bug), Valve::fire) skips a valve firing again
    # inside its own reactions, which the original survives nowhere: none here.
    if reentries:
        raise SystemExit(f'ai valves: the port re-entered a valve {reentries} times')
    if crashes != len(BROKEN):
        raise SystemExit(f'ai valves: the original read {len(BROKEN) - crashes} of the broken files')
    print(f'ai valves: {cases} cases identical ({shipped} shipped files, {n_made} made up; '
          f'{crashes} the original crashes on, read by the port)')


if __name__ == '__main__':
    code = 0
    try:
        main()
    except SystemExit as ex:
        if ex.code not in (None, 0):
            print(ex.code)
            code = 1
    except BaseException:
        import traceback
        traceback.print_exc()
        code = 1
    sys.stdout.flush()
    sys.stderr.flush()
    os._exit(code)
