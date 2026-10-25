"""The mission start's vehicles against the original (docs/mission-start.md):
player.cpp's 0x49e810 with the builders it reaches (0x49fce0, 0x49fa20,
0x49f800, 0x49f8a0, 0x49fe90, 0x49ff10), the player's tick 0x4a1160 and the
launch's lancemates 0x49e5e0 with 0x49e540, and Instant Action's line-up
0x49dc80 at the launch and for a new wave.

Orchestration: every call out of player.cpp is stubbed in the emulator and a
hook in the port, both answered from one script, and the call traces (strings
as text, zerr's reports formatted) must be equal; then the memory they write
(every vehicle's team masks, the lancemate slots, the held controller flag,
the globals) is compared. The aiv.zrd records are the shipped ones, read by
the ported reader (test_ai_readers.py checks it against the exe), from every
mission's archive, then changed at random: mechs, teams, flags, skins,
configurations, nets, names (long ones, suffixes, no '_'). The engine's
answers (nets, controllers, mechs, vehicles, placements, reader nodes,
definitions) succeed or fail at random. The launch's campaign has 0 to 3
lancemate slots (their count off by one at times, a slot missing, a slot
without a mech, a record that fails to read), the player's slot a paint (in
range or not) and the player a vehicle net or none, controls or none, and
settings or none. Instant Action: a game of 0 to 10 slots (allies first, a
mech each, a loadout or none, paint -2..9 for the player), endless, wave or
plain mode, waves 0..25, mobile HQs or not, the player's allies and the
records' teams at random, the mechs made with controls or not; the game's
methods (0x436a90, 0x436ac0, 0x436840, its vt+0x34, the slot setters
0x433ee0 / 0x5a2b90, 0x491120) are stubs over a model both sides share
(test_pilot.py runs the real ones). Load-time code: the x87 at 53 bits;
Instant Action's at 24 (its distances).

    cmake --build build && .venv/bin/python tests/differential/test_mission_start.py [cases]
"""
import ctypes
import glob
import random
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import ROOT, Emu, Trace, compare, load_probe  # noqa: E402
from test_mission_load import c_format  # noqa: E402
from unicorn.x86_const import UC_X86_REG_ESP  # noqa: E402

SETUP, TICK, LAUNCH, ACTION = 0x49e810, 0x4a1160, 0x49e5e0, 0x49dc80
CLASS = [3, 2, 1, 2, 3, 2, 3, 0, 0, 2, 1, 0, 1, 1, 0, 1, 3, 2, 1]  # a weight class per roster mech (any will do)
PILOT, VIEW, STEALTH, FRAME_DT = 0x60861c, 0x5f4c8c, 0x609a2c, 0x72c52c
GLOBALS = [(0x5ea1d0, 4), (0x609a00, 4), (0x609a14, 4), (0x609a0c, 4), (0x609a10, 4), (0x60b56c, 1),
           (0x5e8110, 4), (0x5e8114, 4)]
SCRATCH = 0xfeedf00d
MECHS = ['annihilator', 'avatar', 'blackhawk', 'bushwacker', 'cauldron', 'champion', 'daishi', 'elemental',
         'firefly', 'madcat', 'orion', 'owens', 'puma', 'shadowcat', 'strider', 'sunder', 'supernova', 'thor',
         'vulture']
TYPES = ['stealth', 'ttruck', 'bulldog', 'harasser', 'striker', 'apc', 'mobilehq']  # vehicle.zrd's

CALLS = {
    0x402170: '', 0x4d52b0: '', 0x4d5300: '', 0x4fb740: '', 0x4eb5b0: 'c,d', 0x4eb450: 'c,d',
    0x556740: 'zc,d,s0', 0x5569e0: 'c,zd', 0x556820: 'c', 0x479440: '', 0x4794a0: '',
    0x4decf0: 'c', 0x4846a0: 'c', 0x433df0: 'c,s0,s1,s2', 0x433cc0: 'c,s0,s1,s2', 0x4351c0: 'c,s0,s1',
    0x435230: 'c,s0', 0x4845f0: 'zc', 0x58b160: 'c', 0x4d8750: 'c,s0,s1,s2,s3', 0x4ac860: 'c',
    0x4e71c0: 'c,s0,s1,s2,s3', 0x414150: 'c,d,s0,zs1', 0x41e9a0: 'c', 0x414390: 'c,s0', 0x4161c0: 'c,s0,s1',
    0x41cc30: 'c,s0', 0x4589d0: 'c,zd,zs0', 0x4ac320: 'c,zd,zs0', 0x5a84c0: 'c,zs0', 0x49f660: 'c',
    0x5833b0: 'c,d,s0,s1', 0x5a85c0: 'c', 0x4a1140: 's0', 0x41e3f0: 's0', 0x537f20: 'c', 0x41aba0: 'c,s0',
    0x414430: 'c,s0', 0x49e4e0: 'c,s0',
    0x479460: '', 0x436a90: 'c,s0', 0x436ac0: 'c', 0x436840: 'c,s0,s1,s2,s3', 0x433ee0: 'c,s0', 0x5a2b90: 'c,s0',
    0x484700: 'c', 0x484650: 'c', 0x491120: 'c,s0', 0x5a85e0: 'c',
}
LAUNCH_SEEN = {'0x00414150': 'mechs made', '0x00414430': 'configured', '0x00414390': 'painted',
               '0x0049e4e0': 'triggers taken'}
_aiv_cache = {}


def archives():
    return sorted(glob.glob(str(ROOT / 'game' / 'mw3-1.2' / 'zbd' / '*' / 'reader*.zbd')))


def shipped_records(probe, path):
    if path not in _aiv_cache:
        n = probe.probe_player_load_aiv(path.encode())
        recs = []
        raw = (ctypes.c_uint8 * 0xd0)()
        name = ctypes.create_string_buffer(256)
        for i in range(max(n, 0)):
            probe.probe_player_get_record(i, raw, name, 256)
            recs.append((name.value.decode('latin1'), bytes(raw)))
        _aiv_cache[path] = recs
    return [(n, bytearray(r)) for n, r in _aiv_cache[path]]


def put(raw, off, v, fmt='<i'):
    struct.pack_into(fmt, raw, off, v)


def get(raw, off, fmt='<i'):
    return struct.unpack_from(fmt, raw, off)[0]


# --- the world both sides see -----------------------------------------------
class World:
    """Fake objects in the emulator's memory; every dword written here also
    goes to the probe's memory model."""

    def __init__(self, e, r, case):
        self.e, self.r, self.mem = e, r, {}
        w = self.w
        alloc = e.world_alloc
        # The pilot, its game, the player's vehicle, the view, the stealth entity.
        game = e.object('game', slots={0x2c: ('', 0), 0x34: ('s0', 4)})
        pveh = e.object('pveh', 0x400, slots={0x34: ('c,s0', 4), 0xd8: ('c', 0)})
        self.pilot = alloc(0x100)
        w(self.pilot + 0x6c, game)
        w(self.pilot + 0x14, pveh)
        w(PILOT, self.pilot)
        view = e.object('view', slots={0x8: ('c,s0', 4)})
        w(VIEW, view)
        stealth = e.object('stealth', slots={0x34: ('c,s0', 4)}) if case['stealth'] else 0
        w(STEALTH, stealth)
        w(view + 0x64, stealth if case['view_on_stealth'] else pveh)
        w(FRAME_DT, case['dt'])
        self.campaign = e.object('campaign', slots={0x34: ('c,s0', 4), 0x38: ('c', 0)}) if case['campaign'] else 0
        # The launch's: the player's slot (its paint +0x10), the lancemate
        # slots (+0x04 mech, +0x08 loadout, +0x14 record), the player's net,
        # his controls (+0x37c, their IControls base) and settings.
        self.pslot = alloc(0x18)
        w(self.pslot + 0x10, case['paint'])
        self.mates = []
        for mech, index in case['mates']:
            sl = alloc(0x18)
            w(sl + 0x04, mech), w(sl + 0x08, alloc(16) if r.random() < 0.8 else 0), w(sl + 0x14, index)
            self.mates.append(sl)
        self.pnet = alloc(16) if case['pnet'] else 0
        if case['pctl']:
            ctl, vbt, vt = alloc(0x200), alloc(16), alloc(0x100)
            w(ctl + 4, vbt), w(vbt + 4, 0xfc)
            e.w32(vt + 0x90, e.new_stub('ictl.vt+0x90', 'c', 0))
            w(ctl + 0x100, vt)
            w(pveh + 0x37c, ctl)
        self.training = e.object('training', slots={0x34: ('c,s0', 4)}) if case['training'] else 0
        self.slot = alloc(0x40)
        # Reader trees: player.zrd's three nodes (item 1 a float), vehicle.zrd's defs.
        self.player_tree, self.vehicle_tree = alloc(16), alloc(16)
        self.gravity = {}
        for key, v in case['gravity'].items():
            node, lst = alloc(16), alloc(32)
            w(node + 4, lst)
            w(lst + 0xc, struct.unpack('<I', struct.pack('<f', v))[0])
            self.gravity[key] = node
        self.defs = {}
        for t in case['types']:
            node, lst = alloc(16), alloc(48)
            w(node + 4, lst)
            self.defs[t] = (node, alloc(16) if r.random() < 0.95 else 0)
        # Mech definitions: configurations (+0x20 -> +0xc..+0x10) and skins (+0x1c -> +4..+8).
        self.dfns = []
        for _ in range(4):
            dfn, cfgs, skins = alloc(0x40), alloc(0x20), alloc(0x20)
            w(dfn + 0x20, cfgs)
            w(dfn + 0x1c, skins)
            for obj, off in ((cfgs, 0xc), (skins, 4)):
                n = r.randrange(0, 10)
                if n and r.random() < 0.9:
                    vec = alloc(4 * n + 4)
                    for k in range(n):
                        w(vec + 4 * k, 0 if r.random() < 0.1 else alloc(16))
                    w(vec + 4 * n, 0x5a5a5a5a)  # past the end: read only by a bound off by one
                    w(obj + off, vec)
                    w(obj + off + 4, vec + 4 * n)
            self.dfns.append(dfn)
        # Nets by record kind: znet's object -> vt+0x28 -> vt+0x34.
        self.netobj, self.netref = {}, {}
        for kind in range(0, 40):
            if r.random() < case['net_rate']:
                o = e.object('netobj', slots={0x28: ('c', 0)})
                x = e.object('netref', slots={0x34: ('c', 0)}) if r.random() < 0.95 else 0
                self.netobj[kind] = o
                if x:
                    self.netref[o] = x
                    self.netref[x] = alloc(16) if r.random() < 0.95 else 0
        # Pools: memory for new, controllers (with their IControls base),
        # what the base's vt+0x90 returns, mechs, vehicles.
        self.blocks = [alloc(0x40) for _ in range(64)]
        self.ctls = []
        for _ in range(64):
            ctl, vbt = alloc(0x400), alloc(16)
            w(ctl + 4, vbt)
            w(vbt + 4, 0xfc)
            vt = alloc(0x100)
            e.w32(vt + 0, e.new_stub('ictl.vt+0x0', 'c,s0', 4))
            e.w32(vt + 0x90, e.new_stub('ictl.vt+0x90', 'c', 0))
            w(ctl + 0x100, vt)
            self.ctls.append(ctl)
        self.held = []
        for _ in range(64):
            o = alloc(0x100)
            w(o + 0x88, r.randrange(3))
            self.held.append(o)
        self.mechs = [e.object('mech', 0x700, slots={0: ('c,s0', 4), 0x34: ('c,s0', 4)}) for _ in range(64)]
        for m in self.mechs:  # their data (+0x684) and its roster number (+0x0c); controls or none
            d = alloc(0x20)
            w(d + 0xc, r.randrange(19))
            w(m + 0x684, d)
            w(m + 0x37c, alloc(16) if r.random() < 0.8 else 0)
        # Instant Action: the game (+0x20 the player's slot, +0x44 mode, +0x48
        # bases, +0x6c wave, +0x7c count), ten slot blocks, the player's place
        # and allies, the mech names.
        ia = case['ia']
        self.action = e.object('action', 0x180, slots={0x34: ('c,s0', 4)})
        w(self.action + 0x44, ia['mode']), w(self.action + 0x48, ia['bases']), w(self.action + 0x6c, ia['wave'])
        self.pslot0 = self.action + 0x20
        w(self.pslot0 + 0x10, ia['paint'])
        self.blocks10 = [self.action + 0x80 + 0x18 * k for k in range(10)]
        self.lineup0 = [dict(s, config=0) for s in ia['slots']]  # mech, config, paint, side
        self.cfgs = []
        for k, sl in enumerate(self.lineup0):
            if r.random() < 0.7:
                sl['config'] = alloc(16)
            if r.random() < 0.15:  # a mech already there (never in play: the end clears them)
                sl['m'] = self.mechs[(k * 7 + 3) % 64]
        self.lineup = [dict(x) for x in self.lineup0]
        self.ppos = alloc(12)
        for k, v in enumerate(ia['pos']):
            w(self.ppos + 4 * k, struct.unpack('<I', struct.pack('<f', v))[0])
        w(pveh + 0xc4, ia['allies'])
        self.names = {m: e.world_string(n) for m, n in enumerate(MECHS)}
        self.notmech = e.world_string('notmech')
        self.sync(w)

        self.vehicles = []
        for _ in range(64):
            v = e.object('veh', 0x400, slots={0xdc: ('c,s0,s1', 8), 0x158: ('c,s0,zs1,s2', 12)})
            w(v + 4, alloc(16))
            w(v + 0xe4, self.ctls[r.randrange(64)] if r.random() < 0.5 else 0)
            self.vehicles.append(v)

    def sync(self, w):
        """The line-up model into memory: the count and each slot block."""
        w(self.action + 0x7c, len(self.lineup))
        for k, b in enumerate(self.blocks10):
            sl = self.lineup[k] if k < len(self.lineup) else None
            w(b + 0x04, sl['mech'] if sl else 0), w(b + 0x08, sl['config'] if sl else 0)
            w(b + 0x0c, sl.get('m', 0) if sl else 0), w(b + 0x10, sl['paint'] if sl else 0)
    def w(self, a, v):
        self.e.w32(a, v)
        self.mem[a] = v & 0xffffffff


def make_script(case, world, rec_addr, w=None):
    r = case['rng']
    recs = case['records']
    w = w or world.w

    def slot_of(addr):
        return world.lineup[world.blocks10.index(addr)] if addr in world.blocks10[:len(world.lineup)] else None

    def ia_slot(n, a):
        i = a[1]
        if i == 0:
            return world.pslot0
        return world.blocks10[i - 1] if 0 < i <= len(world.lineup) else 0

    def ia_add(n, a):
        if len(world.lineup) > 9:
            return 0
        world.lineup.append({'mech': a[1], 'config': a[3], 'paint': a[4], 'side': a[2] & 0xff})
        world.sync(w)
        return world.blocks10[len(world.lineup) - 1]

    def ia_drop(n, a):
        world.lineup[:] = [x for x in world.lineup if x['side']]
        world.sync(w)
        return 0

    def ia_ally(n, a):
        i = a[1]
        return 1 if i == 0 else int(0 < i <= len(world.lineup) and world.lineup[i - 1]['side'] != 0)

    def set_mech(n, a):
        sl = slot_of(a[0])
        if sl is not None:
            sl['m'] = a[1]
            w(a[0] + 0xc, a[1])
        return 0

    def set_paint(n, a):
        w(a[0] + 0x10, a[1])
        return 0

    def set_controls(n, a):
        w(a[0] + 0x37c, a[1])
        return 0

    def pick(pool, rate):
        return lambda n, a: 0 if r(f'{n}') < rate else pool[n % len(pool)]

    def find(n, a):
        node, key = a
        if node == world.player_tree:
            return world.gravity.get(key, 0)
        if node == world.vehicle_tree:
            return world.defs[key][0] if key in world.defs else 0
        for d, common in world.defs.values():
            if node == d and key == 'common_mode':
                return common
        return 0

    def vehicle(n, a):
        if a[0] == rec_addr[0]:  # the record as controller (no net): a failure would crash the exe
            return world.vehicles[n % 64]
        return 0 if r(f'v{n}') < case['fail'] else world.vehicles[n % 64]

    def mech_index(n, a):
        name = a[0].lower()
        return MECHS.index(name) if name in MECHS else 0xffffffff

    return {
        'game.vt+0x2c': case['ia_type'] if case.get('entry', '').startswith('ia') else case['type'],
        '0x00479460': world.action if case['ia']['game'] else 0,
        'action.vt+0x34': ia_slot, '0x00436a90': ia_ally, '0x00436ac0': ia_drop, '0x00436840': ia_add,
        '0x00433ee0': set_mech, '0x005a2b90': set_paint, '0x00491120': set_controls,
        '0x00484700': lambda n, a: CLASS[a[0]] if 0 <= a[0] < 19 else 0,
        '0x00484650': lambda n, a: world.names.get(a[0], world.notmech),
        '0x005a85e0': world.ppos,
        'rand': lambda n, a: int(r(f'rand{n}') * 0x8000),
        '0x00556740': lambda n, a: world.player_tree if a[0] == 'player.zrd' else
        (world.vehicle_tree if case['vehicle_zrd'] else 0),
        '0x005569e0': find,
        '0x004fb740': 0x7c00,
        'game.vt+0x34': world.pslot,
        'campaign.vt+0x38': case['mates_count'],
        'campaign.vt+0x34': lambda n, a: world.mates[a[1] - 1] if 1 <= a[1] <= len(world.mates) and
        a[1] - 1 not in case['mates_missing'] else 0,
        'pveh.vt+0xd8': world.pnet,
        '0x00479440': world.campaign, '0x004794a0': world.training,
        'training.vt+0x34': world.slot,
        '0x004e0680': lambda n, a: int(any(nm == a[1] for nm, _ in recs)),
        '0x004e0600': lambda n, a: int(0 <= a[1] < len(recs) and a[1] not in case['read_fail']),
        '0x004decf0': case['count'],
        # A made mech always has its definition: the launch reads through a missing one.
        '0x004846a0': lambda n, a: 0 if r(f'd{n}') < case['dfn_fail'] else world.dfns[n % 4],
        '0x0041e9a0': lambda n, a: world.dfns[(n + 1) % 4],
        '0x004845f0': mech_index,
        '0x0058b160': lambda n, a: world.netobj.get(a[0], 0),
        'netobj.vt+0x28': lambda n, a: world.netref.get(a[0], 0),
        'netref.vt+0x34': lambda n, a: world.netref.get(a[0], 0),
        # A lancemate slot's allocation never fails: the exe writes through it.
        'new': lambda n, a: 0 if a[0] != 0x18 and r(f'{n}') < case['fail'] else world.blocks[n % 64],
        '0x004ac860': pick(world.blocks, case['fail']),
        '0x00433cc0': lambda n, a: a[0],
        '0x004d8750': pick(world.ctls, case['fail']),
        '0x004e71c0': pick(world.ctls, case['fail']),
        'ictl.vt+0x90': pick(world.held, 0.3),
        '0x00414150': pick(world.mechs, case['fail']),
        '0x004161c0': lambda n, a: 0 if r(f'p{n}') < case['fail'] else 1,
        '0x004589d0': vehicle, '0x004ac320': vehicle,
    }


def rng(seed):
    def f(key):
        return random.Random(f'{seed}:{key}').random()
    return f


# --- the original -------------------------------------------------------------
def run_original(case, entry):
    rec_addr = [0]
    t = Trace()
    t.silent |= {'sprintf', 'strncmp', 'strchr', '0x004e0490', '0x004e04f0', 'calloc', 'free'}
    e = Emu(t)
    e.cpu.fpu_control(0x007f if entry.startswith('ia') else 0x027f)
    world = World(e, random.Random(case['seed']), case)
    t.script = make_script(case, world, rec_addr, e.w32)
    for addr, spec in CALLS.items():
        e.stub(addr, spec=spec)
    names = {}

    def record_ctor(em, key, a, pop):
        rec_addr[0] = em.args('c')[0]
        em._ret(rec_addr[0], 0)
    e.stub(0x4e0490, '0x004e0490', '', 0, record_ctor)
    e.stub(0x4e04f0, '0x004e04f0', '', 0, lambda em, k, a, p: em._ret(0, 0))

    def fill(em, i):
        name, raw = case['records'][i]
        em.cpu.write(rec_addr[0] + 4, bytes(raw[4:0xd0]))
        if i not in names:
            names[i] = em.world_string(name)
        return names[i]

    def read_named(em, key, a, pop):
        this, name = em.args('c,zs0')
        ok = t.handle(key, [this, name])
        if ok:
            fill(em, next(i for i, (nm, _) in enumerate(case['records']) if nm == name))
        em._ret(this if ok else 0, 4)

    def read(em, key, a, pop):
        this, i, out = em.args('c,s0,s1')
        ok = t.handle(key, [this, i])
        if ok:
            em.w32(out, fill(em, i))
        em._ret(this if ok else 0, 8)
    e.stub(0x4e0680, '0x004e0680', '', 4, read_named)
    e.stub(0x4e0600, '0x004e0600', '', 8, read)

    def anims_fn(em, key, a, pop):
        esp = em.uc.reg_read(UC_X86_REG_ESP)
        area, node, mode, name, typ, slot = em.args('c,s0,s1,zs2,zs3,s4')
        if slot == esp + 0x44:  # 0x0049f8a0's zeroed local [esp+0x24]
            slot = SCRATCH
        em._ret(t.handle(key, [area, node, mode, name, typ, slot]), 0x14)
    e.stub(0x49ef40, '0x0049ef40', '', 0x14, anims_fn)

    def zerr(em, key, a, pop):
        level, file, line, f = em.args('s0,zs1,s2,zs3')
        vals, k = [], 4
        for m in re.finditer(r'%([sd])', f):
            v = em.args(f's{k}')[0]
            vals.append(em.cstr(v) if m.group(1) == 's' else v)
            k += 1
        em._ret(t.handle('0x0049bfd0', [level, file, line, c_format(f, vals)]), 0)
    e.stub(0x49bfd0, '0x0049bfd0', '', 0, zerr)

    def op_new(em, key, a, pop):
        em._ret(t.handle('new', em.args('s0')), 0)
    e.stub(0x5abd26, 'new', '', 0, op_new)

    def sprintf(em, key, a, pop):
        buf, fmt = em.args('s0,s1')
        s = c_format(em.cstr(fmt), [])
        em.cpu.write(buf, s.encode('latin1') + b'\0')
        em._ret(len(s), 0)

    def strncmp(em, key, a, pop):
        p, q, n = em.args('s0,s1,s2')
        x, y = em.cpu.read(p, n), em.cpu.read(q, n)
        res = 0
        for i in range(n):
            if x[i] != y[i]:
                res = -1 if x[i] < y[i] else 1
                break
            if x[i] == 0:
                break
        em._ret(res, 0)

    def strchr(em, key, a, pop):
        p, ch = em.args('s0,s1')
        s = em.cstr(p)
        i = s.find(chr(ch & 0xff))
        em._ret(p + i if i >= 0 else 0, 0)
    e.import_stub(0x5bb658, 'sprintf', '', 0, sprintf)
    e.import_stub(0x5bb654, 'strncmp', '', 0, strncmp)
    e.import_stub(0x5bb63c, 'strchr', '', 0, strchr)

    heap = [e.world_alloc(0x8000), 0]  # 0x49dc80's two calloc'd arrays, out of the compared memory

    def calloc(em, key, a, pop):
        n, size = em.args('s0,s1')
        p = heap[0] + heap[1]
        heap[1] += (max(n * size, 4) + 15) & ~15
        em.cpu.write(p, bytes(max(n * size, 4)))
        em._ret(p, 0)
    e.import_stub(0x5bb60c, 'calloc', '', 0, calloc)
    e.import_stub(0x5bb65c, 'free', '', 0, lambda em, k, a, p: em._ret(0, 0))
    e.import_stub(0x5bb668, 'rand', '', 0, lambda em, k, a, p: em._ret(t.handle('rand', []), 0))
    e.watch_writes([(e.world_base, e.world_base + e.world_size), (0x6099c0, 0x6099c4)],
                   ignore=[(heap[0], heap[0] + 0x8000)])
    initial = read_globals(e)
    try:
        if entry == 'setup':
            e.call(SETUP, ecx=case['full'])
        elif entry == 'launch':
            t.handle('ret', [e.call(LAUNCH) & 0xff])
        elif entry.startswith('ia'):
            t.handle('ret', [e.call(ACTION, ecx=int(entry == 'ia1')) & 0xff])
        else:
            e.call(TICK)
    except Exception:
        from unicorn.x86_const import UC_X86_REG_EIP
        print(f'  at {e.uc.reg_read(UC_X86_REG_EIP):#x}; the last calls: {t.log[-3:]}')
        raise
    writes = {}
    for a in sorted(e.writes):
        d = a & ~3
        writes[d] = e.r32(d)
    return t.log, read_globals(e), writes, world, initial, rec_addr[0]


def read_globals(e):
    return [e.r8(a) if n == 1 else e.r32(a) for a, n in GLOBALS]


def run_rebuild(probe, case, entry, world, initial, rec_addr):
    world.lineup = [dict(x) for x in world.lineup0]
    t = Trace(make_script(case, world, [rec_addr], lambda a, v: probe.probe_player_poke(a, v & 0xffffffff)))
    probe.probe_app_set_callback(t.callback())
    probe.probe_player_reset()
    for a, v in world.mem.items():
        probe.probe_player_poke(a, v)
    for name, raw in case['records']:
        probe.probe_player_add_record(name.encode('latin1'), (ctypes.c_uint8 * 0xd0).from_buffer_copy(raw))
    probe.probe_player_set_record_addr(rec_addr)
    probe.probe_player_set_globals((ctypes.c_uint32 * 8)(*initial))
    if entry == 'setup':
        probe.probe_player_setup(case['full'])
    elif entry == 'launch':
        t.handle('ret', [probe.probe_player_launch()])
    elif entry.startswith('ia'):
        for m, n in enumerate(MECHS):
            probe.probe_player_mech_name(m, n.encode())
        t.handle('ret', [probe.probe_player_action(int(entry == 'ia1'))])
    else:
        probe.probe_player_tick()
    g = (ctypes.c_uint32 * 8)()
    probe.probe_player_globals(g)
    out = (ctypes.c_uint32 * 4096)()
    n = probe.probe_player_writes(out, 2048)
    writes = {out[2 * i]: out[2 * i + 1] for i in range(n)}
    return t.log, list(g), writes


# --- cases ----------------------------------------------------------------------
def mutate(r, recs):
    for i, (name, raw) in enumerate(recs):
        if r.random() < 0.15:
            put(raw, 0xc4, r.choice([-3, -1, -1, 0, 7, 13, 18, 19, 25]))
        if r.random() < 0.15:
            raw[0xcc] ^= r.choice([0x80, 0x20, 0xa0])
        if r.random() < 0.1:
            put(raw, 0x18, r.choice([0, 1, 2, 3, 0x41, 0x22, 0x3ff]))
        if r.random() < 0.1:
            put(raw, 0xc8, r.choice([0, 1, 3, 7, 8, 9, -1]))
        if r.random() < 0.1:
            put(raw, 0xbc, r.choice([-1, 0, 1, 2, 5, 9]))
        if r.random() < 0.1:
            put(raw, 0x04, r.randrange(0, 45))
        if r.random() < 0.06:
            name = r.choice(['madcat_mech01', 'apc_mech', 'ttruck_mechanic', 'BULLDOG_01', 'thor',
                             'mobilehq', 'mobilehqs_1', 'mobilehqs', 'averyveryverylong_vehicle_name_01',
                             'averyveryverylongmechname_01', 'vulture_a_b', 'Lancemate_01', 'player_2'])
        recs[i] = (name, raw)
    if r.random() < 0.1 and recs:
        recs.pop(r.randrange(len(recs)))
    if r.random() < 0.05:
        recs[:] = [(n, raw) for n, raw in recs if n != 'player']
    if r.random() < 0.1 and len(recs) > 1:
        r.shuffle(recs)


def make_case(probe, seed, paths):
    r = random.Random(seed)
    path = paths[seed % len(paths)]
    recs = shipped_records(probe, path)
    if seed >= len(paths):
        mutate(r, recs)
    training = r.random() < 0.3
    for name, raw in recs:
        if name == 'player' and training and r.random() < 0.5:
            put(raw, 0xbc, r.choice([-1, 0, 1, 3, 9, 0x7fffffff, -0x80000000]))
    count = len(recs) + (r.choice([-1, 1, 2]) if r.random() < 0.05 else 0)
    gravity = {'nom_gravity': 40.0, 'wat_gravity': 9.0, 'qsd_gravity': 3.0}
    if r.random() < 0.2:
        gravity = {k: v for k, v in gravity.items() if r.random() < 0.5}
    types = list(TYPES) if r.random() < 0.9 else [t for t in TYPES if r.random() < 0.5]
    if r.random() < 0.3 and len(recs) > 2:
        types.append('mobilehqs')  # a made-up type: "mobilehq" is matched whole
        i = r.randrange(2, len(recs))
        put(recs[i][1], 0xc4, -1)
        recs[i] = ('mobilehqs_01', recs[i][1])
    block = Path(path).name[6:-4]
    default_type = 3 if block.startswith('mp') else 2 if block.startswith('ia') else 4 if 't1' in path else 1
    dt = r.choice([0.0, 0.033333335, 0.05, 1e-30, 0.2, float('nan')])
    # The launch's lancemate slots: 0..3, a mech or none (-1), a record by index.
    mates = [(r.choice([-1, r.randrange(19), r.randrange(19)]), r.randrange(-1, len(recs) + 2))
             for _ in range(r.choice([0, 1, 2, 3, 3]))]
    pctl = r.random() < 0.9
    # The exe reads through a missing record when the player's settings take an
    # empty slot's triggers (0x0049e4e0, FIX (bug)): not given to it.
    read_fail = {i for i in range(len(recs)) if r.random() < 0.03}
    if pctl:
        read_fail -= {i for m, i in mates if m == -1}
        mates = [(m, i) if m != -1 or 0 <= i < len(recs) else (m, r.randrange(len(recs))) for m, i in mates] \
            if recs else [(m, i) for m, i in mates if m != -1]
    if r.random() < 0.4 and len(recs) > 4:  # start points shared: ties for the farthest
        src = recs[r.randrange(2, len(recs))][1]
        for _ in range(r.randrange(1, 4)):
            recs[r.randrange(2, len(recs))][1][0x08:0x14] = src[0x08:0x14]
    # Instant Action: allies first, then enemies, up to 10.
    allies, enemies = r.randrange(0, 4), r.randrange(0, 7)
    slots = [{'mech': r.randrange(19), 'paint': r.randrange(8), 'side': 1} for _ in range(allies)] + \
            [{'mech': r.choice([r.randrange(19), -1, 25]) if r.random() < 0.1 else r.randrange(19), 'paint': 0, 'side': 0}
             for _ in range(enemies)]
    ia = {'game': r.random() < 0.95, 'mode': r.choice([0, 1, 2, 2, 7]), 'bases': r.choice([0, 0, 1, 0x100]),
          'wave': r.choice([1, 1, 2, 5, 13, 20, 21, 25, r.randrange(1, 21)]), 'slots': slots[:10],
          'paint': r.choice([0, 3, 7, 8, 9, -1, -2, r.randrange(8)]),
          'pos': [r.uniform(-3000, 3000), r.uniform(-50, 200), r.uniform(-3000, 3000)],
          'allies': r.choice([0x4, 0x6, 0x2, 0, 0xfffffffe, r.getrandbits(32)])}
    return {
        'ia': ia, 'ia_type': 2 if r.random() < 0.9 else r.choice([1, 3, 4]),
        'seed': seed, 'path': path, 'records': recs, 'count': max(count, 0),
        'full': 1 if r.random() < 0.92 else 0,
        'type': default_type if r.random() < 0.8 else r.choice([-1, 0, 1, 2, 3, 4, 5]),
        'campaign': r.random() < 0.6, 'training': training,
        'gravity': gravity, 'types': types, 'vehicle_zrd': r.random() < 0.97,
        'read_fail': read_fail,
        'mates': mates, 'mates_count': len(mates) + 1 + (r.choice([-1, 1]) if r.random() < 0.15 else 0),
        'mates_missing': {i for i in range(len(mates)) if r.random() < 0.05},
        'paint': r.choice([0, 0, 1, 2, 5, 9, -1, r.randrange(10)]), 'pnet': r.random() < 0.9, 'pctl': pctl,
        'net_rate': r.choice([0.0, 0.5, 0.9, 1.0]), 'fail': r.choice([0.0, 0.0, 0.1, 0.3]),
        'stealth': r.random() < 0.8, 'view_on_stealth': r.random() < 0.3,
        'dt': struct.unpack('<I', struct.pack('<f', dt))[0],
        'rng': rng(seed),
    }


def main():
    probe = load_probe()
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 400
    paths = [p for p in archives() if shipped_records(probe, p)]
    calls = 0
    ia_seen = {}
    seen = {'ran': 0, 'mechs made': 0, 'configured': 0, 'painted': 0, 'triggers taken': 0, 'duties refused': 0}
    for seed in range(count):
        case = make_case(probe, seed, paths)
        if not case['stealth'] and case['view_on_stealth']:
            case['view_on_stealth'] = False  # the exe would update a null stealth entity
        for entry in ('setup', 'tick', 'launch', 'ia1', 'ia0'):
            case['entry'] = entry
            case['dfn_fail'] = 0.0 if entry in ('launch', 'ia1', 'ia0') else 0.1
            if entry.startswith('ia'):  # the exe reads on through a record that fails to read (FIX (bug))
                case['read_fail_saved'] = case.get('read_fail_saved', case['read_fail'])
                case['count_saved'] = case.get('count_saved', case['count'])
                case['read_fail'] = set()
                case['count'] = min(case['count'], len(case['records']))
            elif 'read_fail_saved' in case:
                case['read_fail'] = case.pop('read_fail_saved')
                case['count'] = case.pop('count_saved')
            try:
                log_a, glob_a, writes_a, world, initial, rec_addr = run_original(case, entry)
            except Exception:
                print(f'seed {seed} {entry}: the original faulted ({case["path"]}, type {case["type"]})')
                raise
            log_b, glob_b, writes_b = run_rebuild(probe, case, entry, world, initial, rec_addr)
            name = f'seed {seed} {entry} ({Path(case["path"]).parent.name}/{Path(case["path"]).name}, ' \
                   f'type {case["type"]}, full {case["full"]})'
            if not compare(name, log_a, log_b):
                raise SystemExit(1)
            if glob_a != glob_b:
                diff = [(hex(GLOBALS[i][0]), hex(x), hex(y)) for i, (x, y) in enumerate(zip(glob_a, glob_b)) if x != y]
                print(f'{name}: globals differ (address, original, rebuild): {diff}')
                raise SystemExit(1)
            if writes_a != writes_b:
                keys = sorted(set(writes_a) | set(writes_b))
                diff = [(hex(k), hex(writes_a.get(k, -1)), hex(writes_b.get(k, -1))) for k in keys
                        if writes_a.get(k) != writes_b.get(k)]
                print(f'{name}: memory written differs (address, original, rebuild): {diff[:12]}')
                raise SystemExit(1)
            calls += len(log_a)
            if entry.startswith('ia'):
                for k, v in log_a:
                    key = None
                    if k == 'ret':
                        key = 'ia ran' if v[0] == 1 else None
                    elif k == '0x00414150':
                        key = f'ia mechs made ({entry})'
                    elif k == 'rand':
                        key = 'ia random picks'
                    elif k == '0x005a85e0':
                        key = 'ia distances'
                    elif k == '0x00491120' and v[1]:
                        key = 'ia controls given'
                    elif k == '0x00436840':
                        key = 'ia wave mechs added'
                    elif k == '0x004589d0':
                        key = 'ia mobile HQs'
                    elif k == '0x0049bfd0' and v[2] in (0x14f, 0x154):
                        key = 'ia reports'
                    if key:
                        ia_seen[key] = ia_seen.get(key, 0) + 1
            if entry == 'launch':
                for k, v in log_a:
                    if k == 'ret':
                        seen['ran'] += v[0] == 1
                    elif k == '0x0049bfd0' and v[2] in (0x186, 0x18a, 0x18e):
                        seen['duties refused'] += 1
                    elif k in LAUNCH_SEEN:
                        seen[LAUNCH_SEEN[k]] += 1
    print(f'{count} cases x 5 entries, {calls} calls: the rebuilt mission start matches the original')
    print('  the launch: ' + ', '.join(f'{v} {k}' for k, v in seen.items()))
    print('  instant action: ' + ', '.join(f'{v} {k}' for k, v in sorted(ia_seen.items())))


if __name__ == '__main__':
    main()
