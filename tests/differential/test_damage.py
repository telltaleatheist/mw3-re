"""Damage at run time (docs/damage.md) against the original x86.

    cmake --build build && .venv/bin/python tests/differential/test_damage.py

A whole mech in emulator memory: the real Mech code with a copy of its vtable,
the thirteen parts with their real vtables and nodes, the pilot, the loadout's
item list and the part list. Random mechs take random hits (every part node,
area damage, knockback, NARC, nukes, criticals by the roll), plus direct
criticals of every kind, part destruction, limps, falls and the torso's
front/rear choice. Every call into unported code is stubbed and logged on both
sides; after each operation the mech, its parts, the items and the lists must
match exactly (floats bit for bit at the game's 24-bit x87 precision).
"""
import ctypes
import math
import os
import random
import struct
import sys
from pathlib import Path

os.environ.setdefault('MW3_X87', '24')  # the precision the game runs at; exact compares need it
sys.path.insert(0, str(Path(__file__).parent))
from original import FTOL, IAT_FTOL, ROOT, Original  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDI  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))

IAT_RAND, IAT_DELETE = 0x5bb668, 0x5bb4c8
MECH_VT, MECH_SIZE, PART_IMG = 0x5bcef8, 0x6d8, 0x200
TAKE_DAMAGE, DESTROY_PART, START_LIMP, KNOCKED_DOWN, HIT_SECTION = 0x41ac60, 0x41cfa0, 0x41d5f0, 0x41b660, 0x460350
# name, id, vtable: the order of probe_damage.cpp
PARTS = [('torso', 1, 0x5c29e0), ('leg0', 4, 0x5c2978), ('leg1', 5, 0x5c2978), ('arm0', 2, 0x5c2a48),
         ('arm1', 3, 0x5c2a48), ('head', 6, 0x5c2b80), ('hip', 15, 0x5c28a8), ('foot0', 13, 0x5c2910),
         ('foot1', 14, 0x5c2910), ('calf0', 11, 0x5c2b18), ('calf1', 12, 0x5c2b18), ('hand0', 9, 0x5c2ab0),
         ('hand1', 10, 0x5c2ab0)]
TORSO, LEG0, LEG1, ARM0, ARM1, HEAD, HIP, FOOT0, FOOT1, CALF0, CALF1, HAND0, HAND1 = range(13)


def vec(off):
    return [(off, 'f'), (off + 4, 'f'), (off + 8, 'f')]


MECH_FIELDS = ([(0x04, 'i'), (0x08, 'b')] + vec(0x30) + vec(0x3c) + vec(0x58) +
               [(0xc0, 'i'), (0xc4, 'i'), (0xe0, 'f'), (0xe4, 'i')] +
               [(0x3ac + 4 * k, 'i') for k in range(15)] + [(0x3e8, 'i'), (0x410, 'b'), (0x411, 'b')] +
               [(0x424 + 4 * k, 'i') for k in range(13)] + [(0x458 + 4 * k, 'i') for k in range(10)] +
               [(0x510, 'f'), (0x518, 'f'), (0x53c, 'f')] + vec(0x540) +
               [(0x558, 'f'), (0x570, 'f'), (0x578, 'i'), (0x57c, 'i'), (0x584, 'b'), (0x5ac, 'i')] +
               [(0x5b8 + 4 * k, 'i') for k in range(25)] + [(0x630, 'b'), (0x65c, 'i'), (0x664, 'i')] +
               [(0x6b8 + 4 * k, 'f') for k in range(5)])
PART_FIELDS = [(0x08, 'i'), (0x38, 'f'), (0x3c, 'f'), (0x40, 'f'), (0x44, 'f'), (0x48, 'b'), (0x68, 'i'),
               (0x6c, 'i'), (0x70, 'i')]
EXTRA = {TORSO: [(0x74, 'i'), (0x78, 'i'), (0x7c, 'f'), (0x80, 'f')] + vec(0xd8) + vec(0x100) + vec(0x14c) +
         [(0x174 + 4 * s, 'f') for s in range(1, 6)] + [(0x188 + 4 * s, 'f') for s in range(1, 6)] +
         [(0x1a0, 'f'), (0x1a4, 'f'), (0x1a8, 'f'), (0x1ac, 'f')],
         FOOT0: [(0xac, 'b'), (0xdc, 'f'), (0xe0, 'f'), (0xe4, 'i'), (0xe8, 'i')],
         ARM0: [(0x74, 'i')]}
EXTRA[FOOT1], EXTRA[ARM1] = EXTRA[FOOT0], EXTRA[ARM0]

# The log codes of probe_damage.cpp; 'f' arguments print as floats.
NAMES = {1: 'controls_hit', 2: 'ff_hit', 3: 'damage_decal', 4: 'last_hit', 5: 'zaps_hud', 6: 'head_hit_hud',
         7: 'net_event', 8: 'weapons_lost', 9: 'weapons_critical', 10: 'weapons_stop', 11: 'hud_weapons',
         12: 'mech_sound', 13: 'equip_voice', 14: 'voice', 15: 'fly_off', 16: 'spawn_effect', 17: 'spawn_effect_at',
         18: 'on_effect_end', 19: 'node_unlink', 20: 'node_set_active', 21: 'node_set_name', 22: 'stop_foot_effect',
         23: 'free_part', 24: 'node_update', 25: 'anim_ok', 26: 'hip_reset', 27: 'death_prep', 28: 'death_anim',
         29: 'stop_action', 30: 'blend_anim', 31: 'anim_speed', 32: 'gait_speed', 33: 'stats', 34: 'view_camera',
         35: 'mission_player_died', 36: 'stop_sound', 37: 'controls_stop', 38: 'killer_notify', 391: 'hud_view_off',
         392: 'hud_442e00', 393: 'screen_vt4'}


def f2i(x):
    return struct.unpack('<i', struct.pack('<f', x))[0]


def i2f(i):
    return struct.unpack('<f', struct.pack('<I', i & 0xffffffff))[0]


def f32(x):
    return i2f(f2i(x))


def s32(v):
    return v - (1 << 32) if v & 0x80000000 else v


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
        a = cpu.alloc
        self.mech, self.mvt, self.other = a(0x700), a(0x200), a(0x700)
        cpu.write(self.mvt, cpu.read(MECH_VT, 0x200))
        net = cpu.code(b'\xc2\x08\x00')
        cpu.u32(self.mvt + 0x168, net)
        cpu.stub_function(net, lambda c: self.log(7, c.arg(0), s32(c.arg(1))), 8)
        self.parts = [a(PART_IMG) for _ in PARTS]
        self.nodes = [a(0x100) for _ in PARTS]
        self.data = [a(0x100) for _ in PARTS]
        self.area_node, self.pilot, self.config = a(0x100), a(0x300), a(0x40)
        self.x228 = a(0x10)
        self.k = a(0x40)  # scripted values read by little routines
        self.items_mem = [a(0x20) for _ in range(40)]
        self.list_nodes = [a(0x10) for _ in range(64)]
        self.def_, self.type_, self.coll = a(0x200), a(0xd0), a(0x40)
        self.nuke = a(0x10)
        cpu.write(self.nuke, b'NUKE\0')
        self.lrm = a(0x10)
        cpu.write(self.lrm, b'LRM20\0')
        self.pos = a(0x10)
        # controls: a virtual-base object, vtable +0x1c throttle, +0x94 hit, +0xa8 stop
        ctl, vbt, vt = a(0x40), a(8), a(0x100)
        cpu.u32(ctl + 4, vbt)
        cpu.u32(vbt + 4, 0x10)
        cpu.u32(ctl + 0x14, vt)
        cpu.u32(vt + 0x1c, cpu.code(b'\xd9\x05' + struct.pack('<I', self.k) + b'\xc3'))
        hit, stop = cpu.code(b'\xc2\x10\x00'), cpu.code(b'\xc3')
        cpu.u32(vt + 0x94, hit)
        cpu.u32(vt + 0xa8, stop)
        cpu.stub_function(hit, lambda c: self.log(1, c.arg(2), c.arg(3)), 0x10)
        cpu.stub_function(stop, lambda c: self.log(37))
        self.controls = ctl
        # the attacker: vtable +0x80 is told of a kill
        self.attacker, avt = a(0x10), a(0x100)
        cpu.u32(self.attacker, avt)
        kill = cpu.code(b'\xc2\x04\x00')
        cpu.u32(avt + 0x80, kill)
        cpu.u32(self.mvt + 0x80, kill)  # a mech can be its own killer
        cpu.stub_function(kill, lambda c: self.log(38, c.ecx, c.arg(0)), 4)
        # [0x60861c]: +0x14 the mission's player, +0x6c -> vtable +0x2c the game type
        self.mission, sub, gvt = a(0x80), a(8), a(0x40)
        cpu.u32(0x60861c, self.mission)
        cpu.u32(self.mission + 0x6c, sub)
        cpu.u32(sub, gvt)
        cpu.u32(gvt + 0x2c, cpu.code(b'\xa1' + struct.pack('<I', self.k + 4) + b'\xc3'))
        # [0x5f4c8c]: +0x64 the player, +0x58 the view
        self.pl, self.view = a(0x80), a(0x10)
        cpu.u32(0x5f4c8c, self.pl)
        self.stats_obj = a(0x40)
        # the screen: vtable +4, an object at +0x1508 with vtable +0x64, flags +0x167c, +0x1514
        self.screen, svt, hvt = a(0x2000), a(0x40), a(0x100)
        cpu.u32(self.screen, svt)
        s4, h64 = cpu.code(b'\xc2\x04\x00'), cpu.code(b'\xc3')
        cpu.u32(svt + 4, s4)
        cpu.u32(self.screen + 0x1508, hvt)
        cpu.u32(hvt + 0x64, h64)
        cpu.stub_function(s4, lambda c: self.log(393, c.arg(0)), 4)
        cpu.stub_function(h64, lambda c: self.log(391))
        cpu.stub_function(0x442e00, lambda c: self.check(c.ecx == self.screen + 0x1508) or self.log(392))
        stubs = {
            0x40e920: (lambda c: self.sc['attacker'], 0),
            0x576040: (lambda c: self.nuke, 0),
            0x430aa0: (lambda c: self.sc['screen'], 0),
            0x414370: (lambda c: self.sc['x1c'], 0),
            0x586ce0: (lambda c: self.sc['force0'], 0),
            0x479480: (lambda c: self.sc['networked'], 0),
            0x5a85e0: (lambda c: self.pos, 0),
            0x45fbc0: (self.lose, 4),
            0x433720: (lambda c: self.log(2, c.arg(0), *self.vec(c.arg(1)), c.arg(2)), 0xc),
            0x474d90: (lambda c: self.log(3, c.ecx, c.arg(0), c.arg(2)), 0xc),
            0x41d130: (lambda c: self.log(4), 0),
            0x43be70: (lambda c: self.log(5, c.ecx, c.arg(0), c.arg(1), c.arg(2)), 0xc),
            0x508da0: (lambda c: self.log(6, c.ecx, c.edx), 0xc),
            0x4b1040: (lambda c: self.log(8, s32(c.arg(0))), 4),
            0x4b0de0: (lambda c: self.log(9, s32(c.arg(0)), c.arg(1)) or self.sc['wcrit'], 8),
            0x4b1110: (lambda c: self.log(10), 0),
            0x446520: (lambda c: self.log(11, (c.ecx - 0x6520) & 0xffffffff), 0),
            0x41e770: (self.mech_sound, 8),
            0x485390: (lambda c: self.log(13, c.ecx), 0),
            0x485370: (lambda c: self.log(14, c.ecx), 0),
            0x467100: (self.fly_off, 0x10),
            0x466fe0: (self.fly_off, 0x10),
            0x508aa0: (lambda c: self.effect(16, c.ecx, c.edx, *[c.arg(i) for i in range(9)]), 0x24),
            0x508c10: (lambda c: self.effect(17, c.ecx, c.edx, c.arg(0), c.arg(1), c.arg(2)), 0xc),
            0x508f00: (lambda c: self.check(c.edx == 0x461c90) or self.log(18, c.ecx, c.arg(0)), 4),
            0x4e66a0: (lambda c: self.log(19, c.ecx, c.arg(0)), 4),
            0x4eb170: (lambda c: self.log(20, c.ecx, c.edx), 0),
            0x4eb300: (lambda c: self.log(21, c.ecx, int(self.cstr(c.edx) == b'deadmech')), 0),
            0x45fb50: (lambda c: self.log(22, c.ecx), 0),
            0x466df0: (lambda c: self.log(23, c.ecx), 4),
            0x4ed530: (lambda c: self.log(24, c.ecx), 0),
            0x416780: (lambda c: self.log(25, c.arg(0), c.arg(1)) or self.sc['anim_ok'], 8),
            0x4621f0: (lambda c: self.log(26, c.ecx, *self.vec(c.arg(0))), 4),
            0x41d070: (lambda c: self.log(27), 0),
            0x41b760: (lambda c: self.log(28, c.arg(0)), 4),
            0x421060: (lambda c: self.log(29, c.ecx), 0),
            0x41ff30: (lambda c: self.log(30, c.ecx, c.edx) or self.sc['blend'], 0),
            0x41ef90: (lambda c: self.log(31, c.ecx, c.arg(0)), 4),
            0x4329c0: (lambda c: self.check(c.ecx == self.pilot and c.arg(0) == self.mech + 0x4a0)
                       or self.log(32, c.arg(1)), 8),
            0x433530: (lambda c: self.log(33, 0x433530, 0), 0),
            0x433900: (lambda c: self.log(33, 0x433900, c.arg(0)), 4),
            0x4338d0: (lambda c: self.log(33, 0x4338d0, c.arg(0)), 4),
            0x433840: (lambda c: self.log(33, 0x433840, 0), 0),
            0x40ae70: (lambda c: self.log(34, c.arg(0)), 4),
            0x48f9e0: (lambda c: self.check(c.ecx == 0x6098d0) or self.log(35), 0),
            0x56e9f0: (lambda c: self.log(36, c.ecx), 0),
        }
        for addr, (fn, pop) in stubs.items():
            cpu.stub_function(addr, fn, pop)
        cpu.stub_import(IAT_DELETE, lambda c: 0)
        cpu.stub_import(IAT_RAND, lambda c: self.rand())
        self.sc = {}
        self.events = []
        # Coverage: places in the original each scenario set should reach.
        self.reached = set()
        reg = cpu.uc.reg_read
        cover = {
            0x460b8b: lambda: f'critical roll {reg(UC_X86_REG_EDI)}',
            0x460e39: lambda: 'CASE stops an ammo critical' if reg(UC_X86_REG_EAX) & 0xff else None,
            0x460992: lambda: 'multiplayer: no difficulty' if reg(UC_X86_REG_EAX) == 3 else None,
            0x460a4e: lambda: 'difficulty scaling',
            0x46138c: lambda: 'invincible' if reg(UC_X86_REG_ECX) & 0xff else None,
            0x4614a3: lambda: 'torso structure floored at armor',
            0x4614d3: lambda: 'structure floored at armor',
            0x4615b2: lambda: 'part structure at 0',
            0x4648c3: lambda: 'torso section structure at 0',
            0x461215: lambda: 'equipment critical',
            0x463c5b: lambda: 'limp roll',
            0x41d666: lambda: 'limping, left',
            0x41d685: lambda: 'limping, right',
            0x41afc9: lambda: 'area damage',
            0x41adfb: lambda: 'knockback',
            0x41ad20: lambda: 'NARC',
            0x465f59: lambda: 'nuke kill',
            0x466048: lambda: 'death explosion',
            0x4660b1: lambda: 'death in water',
            0x41b629: lambda: 'Elemental part loss',
        }

        def hit(uc, address, size, user):
            r = cover[address]()
            if r:
                self.reached.add(r)
        for addr in cover:
            cpu.uc.hook_add(UC_HOOK_CODE, hit, begin=addr, end=addr)
        self.cover_names = {'critical roll %d' % k for k in range(5)} | {
            'CASE stops an ammo critical', 'multiplayer: no difficulty', 'difficulty scaling', 'invincible',
            'torso structure floored at armor', 'structure floored at armor', 'part structure at 0',
            'torso section structure at 0', 'equipment critical', 'limp roll', 'limping, left', 'limping, right',
            'area damage', 'knockback', 'NARC', 'nuke kill', 'death explosion', 'death in water',
            'Elemental part loss'}

    # --- stubs -------------------------------------------------------------
    def log(self, *e):
        self.events.append(tuple(v & 0xffffffff for v in e))
        return 0

    def check(self, ok):
        if not ok:
            raise SystemExit('a stub was called with unexpected registers')
        return 0

    def vec(self, p):
        return struct.unpack('<3I', self.cpu.read(p, 12))

    def cstr(self, p):
        b = self.cpu.read(p, 32)
        return b[:b.index(0)]

    def lose(self, c):
        # 0x0045fbc0 as the exe: the sinks lost, failure always (the port's
        # FIX (bug) counting a lost sink as success is behind zfix bugs, off
        # here; test_fixes.cpp checks it).
        p = c.ecx + 4
        n = struct.unpack('<i', self.cpu.read(p, 4))[0]
        m = max(n - s32(c.arg(0)), 0)
        self.cpu.u32(p, m)
        return 0

    def mech_sound(self, c):
        self.check(c.arg(0) == 0x3f800000 and c.arg(1) == 0xbf800000)
        return self.log(12, c.ecx)

    def fly_off(self, c):
        self.log(15, c.ecx, c.arg(3))
        for k in range(3):
            self.cpu.write(c.arg(k), struct.pack('<3f', *self.sc['fly'][k]))
        return self.sc['fly_node']

    def effect(self, code, *args):
        self.log(code, *args)
        h = self.sc['next_effect']
        self.sc['next_effect'] += 1
        return h

    # --- memory ------------------------------------------------------------
    def u32(self, p):
        return struct.unpack('<I', self.cpu.read(p, 4))[0]

    def build_list(self, head_at, values, pool):
        """An MSVC list (sentinel [next, prev]) of values at head_at, count at +4."""
        cpu = self.cpu
        head = pool.pop()
        nodes = [pool.pop() for _ in values]
        ring = [head] + nodes
        for i, n in enumerate(ring):
            cpu.u32(n, ring[(i + 1) % len(ring)])
            cpu.u32(n + 4, ring[i - 1])
        for n, v in zip(nodes, values):
            cpu.u32(n + 8, v)
        cpu.u32(head_at, head)
        cpu.u32(head_at + 4, len(values))

    def read_list(self, head_at):
        head = self.u32(head_at)
        out, n = [], self.u32(head)
        while n != head:
            out.append(self.u32(n + 8))
            n = self.u32(n)
        return out


def gen(rng):
    """A random mech: images for the mech and parts, pilot, items, lists."""
    mech = bytearray(MECH_SIZE)
    parts = [bytearray(PART_IMG) for _ in PARTS]

    def put(b, off, kind, v):
        struct.pack_into({'f': '<f', 'i': '<i', 'b': '<B', 'I': '<I'}[kind], b, off, v)

    def frac():
        r = rng.random()
        if r < 0.25:
            return 1.0
        if r < 0.35:
            return 0.0
        if r < 0.5:
            return rng.uniform(0.0, 0.2)
        if r < 0.55:
            return 0.15
        return rng.random()

    def scale():
        return f32(100.0 / rng.choice([1, 2, 3, 5, 8, 12, 17, 21, 25, 31, 36, 48]))

    stripped = rng.random() < 0.3  # armor gone everywhere: criticals roll on every non-beam hit

    for i, (name, pid, _) in enumerate(PARTS):
        b = parts[i]
        a, h = frac(), frac()
        if stripped:
            a = f32(rng.uniform(0.0, 0.15))
        put(b, 0x38, 'f', h)
        put(b, 0x3c, 'f', scale())
        put(b, 0x40, 'f', a)
        put(b, 0x44, 'f', scale())
        put(b, 0x48, 'b', rng.random() < 0.05)
        put(b, 0x68, 'I', rng.choice([0, 0, 0, 0x6100 + i]))
    t = parts[TORSO]
    put(t, 0x7c, 'f', f32(rng.uniform(1.0, 5.0)))
    put(t, 0x80, 'f', f32(rng.uniform(0.8, 4.0)))
    for k in range(3):
        put(t, 0x14c + 4 * k, 'f', f32(rng.uniform(0.0, 3.0)))
    for s in range(1, 6):
        put(t, 0x174 + 4 * s, 'f', frac())
        put(t, 0x188 + 4 * s, 'f', scale())
    for k in range(2):
        put(t, 0x1a0 + 4 * k, 'f', frac())
        put(t, 0x1a8 + 4 * k, 'f', scale())
    for f in (FOOT0, FOOT1):
        put(parts[f], 0xac, 'b', rng.random() < 0.3)
        put(parts[f], 0xdc, 'f', f32(rng.uniform(0, 5)))
        put(parts[f], 0xe0, 'f', f32(rng.uniform(-10, 10)))
        put(parts[f], 0xe4, 'I', rng.choice([0, 0x6200 + f]))
        put(parts[f], 0xe8, 'i', rng.randrange(11))

    put(mech, 0x04, 'I', 0x6000)
    put(mech, 0x08, 'b', rng.random() < 0.97)
    for off in (0x30, 0x34, 0x38, 0x3c, 0x40, 0x58, 0x5c, 0x60):
        put(mech, off, 'f', f32(rng.uniform(-30, 30)))
    put(mech, 0x44, 'f', f32(rng.choice([rng.uniform(-30, 30), 0.0, -13.888889, 13.888889, 5.0])))
    put(mech, 0xc0, 'I', rng.choice([1, 2, 4, 3]))
    put(mech, 0xc4, 'I', rng.choice([1, 2, 4, 6]))
    put(mech, 0xe0, 'f', 1.5)
    for k in range(15):
        put(mech, 0x3ac + 4 * k, 'i', rng.choice([0, 0, 1, 2, 3]))
    put(mech, 0x3e8, 'I', rng.choice([0, 0x6300]))
    for k in range(13):
        put(mech, 0x424 + 4 * k, 'I', rng.choice([0, 0x6400 + k, 0x6400 + k]))
    for k in range(10):
        put(mech, 0x458 + 4 * k, 'I', 0x6500 + k)
    put(mech, 0x518, 'f', f32(rng.uniform(5, 30)))
    put(mech, 0x510, 'f', f32(rng.uniform(3, 18)))
    for off in (0x540, 0x544, 0x548):
        put(mech, off, 'f', f32(rng.uniform(-3, 3)))
    put(mech, 0x53c, 'f', f32(rng.uniform(0, 5)))
    put(mech, 0x558, 'f', 0.0)
    put(mech, 0x578, 'i', rng.choice([1, 1, 2, 3, 5, 9, 0, 10, 11, 12, 13]))
    put(mech, 0x584, 'b', rng.random() < 0.1)
    put(mech, 0x5ac, 'i', rng.choice([-1, -1, -1, 0, 1, 2]))
    for k in range(25):
        put(mech, 0x5b8 + 4 * k, 'I', rng.choice([0, 0x6600 + k]))
    put(mech, 0x630, 'b', rng.random() < 0.1)
    put(mech, 0x65c, 'i', rng.choice([0, 1, 10, 14, 22]))
    put(mech, 0x664, 'I', 0)
    kd = f32(rng.uniform(2, 20))
    for k in range(5):
        put(mech, 0x6b8 + 4 * k, 'f', f32(kd * (k + 1)))

    roster = rng.choice([0, 3, 7, 7, 11, 18, rng.randrange(19)])
    effects = [rng.choice([0, 0x6800 + i]) for i in range(18)]
    death = [rng.choice([0, 0x6900 + i]) for i in range(4)]
    # the cockpit explosion (+0x200), +0x20c, the cockpit block present, its node
    pilot = [roster] + effects + death + [rng.choice([0, 0x6b00]), rng.choice([0, 0x6a00]), rng.choice([0, 1]),
                                         rng.choice([0, 0x6c00])]
    # items: (kind, count, group, part, id, tech)
    items = []
    for _ in range(rng.randrange(0, 18)):
        kind = rng.choice([3, 3, 3, 3, 1, 2])
        part = rng.choice([1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 13, 15, -1])
        if kind == 3:
            items.append((3, rng.choice([1, 1, 2]), int(rng.random() < 0.1), part, rng.randrange(15),
                          rng.choice([1, 2])))
        else:
            items.append((kind, rng.choice([1, 20]), 0, part, rng.randrange(36), rng.choice([1, 2])))
    plist = list(range(13))
    rng.shuffle(plist)
    plist = plist[:rng.randrange(8, 14)] + ([HAND0] if rng.random() < 0.2 else [])
    actions = rng.choice([[], [], [5], [6], [3], [6, 5]])
    return mech, parts, pilot, items, plist, actions


def node_flags(i):
    return PARTS[i][1]


class Run:
    def __init__(self, emu):
        self.emu = emu
        self.seen = {}

    def load(self, rng, seed):
        emu, cpu = self.emu, self.emu.cpu
        mech, parts, pilot, items, plist, actions = gen(rng)
        self.items = items
        cpu.write(emu.mech, bytes(0x700))
        # structure the probe sets up itself
        struct.pack_into('<I', mech, 0, emu.mvt)
        struct.pack_into('<I', mech, 0xd4, 0x6700)
        struct.pack_into('<I', mech, 0x37c, emu.controls)
        for slot, idx in ((0x38c, LEG0), (0x390, LEG1), (0x394, ARM0), (0x398, ARM1), (0x39c, TORSO),
                          (0x3a0, HIP), (0x3a4, HEAD)):
            struct.pack_into('<I', mech, slot, emu.parts[idx])
        struct.pack_into('<I', mech, 0x3ec, emu.config)
        struct.pack_into('<I', mech, 0x684, emu.pilot)
        cpu.write(emu.mech, bytes(mech))
        pool = list(emu.list_nodes)
        emu.build_list(emu.mech + 0x384, [emu.parts[i] for i in plist], pool)
        emu.build_list(emu.mech + 0x480, actions, pool)
        for i, (name, pid, vt) in enumerate(PARTS):
            b = parts[i]
            struct.pack_into('<I', b, 0, vt)
            struct.pack_into('<I', b, 4, emu.mech)
            struct.pack_into('<I', b, 8, emu.nodes[i])
            cpu.write(emu.nodes[i], bytes(0x100))
            cpu.u32(emu.nodes[i] + 0x28, pid)
            cpu.u32(emu.nodes[i] + 0x38, emu.data[i])
            cpu.u32(emu.nodes[i] + 0x40, emu.parts[i])
        for leg, calf, foot in ((LEG0, CALF0, FOOT0), (LEG1, CALF1, FOOT1)):
            struct.pack_into('<II', parts[leg], 0x74, emu.parts[calf], emu.parts[foot])
        for arm, hand in ((ARM0, HAND0), (ARM1, HAND1)):
            struct.pack_into('<I', parts[arm], 0x74, emu.parts[hand])
        for i in range(13):
            cpu.write(emu.parts[i], bytes(parts[i]))
        # pilot
        p = emu.pilot
        cpu.write(p, bytes(0x300))
        cpu.u32(p + 0xc, pilot[0])
        for i in range(18):
            cpu.u32(p + 0x1a4 + 0xc * (i // 3) + 4 * (i % 3), pilot[1 + i])
        for i in range(4):
            cpu.u32(p + 0x1ec + 4 * i, pilot[19 + i])
        cpu.u32(p + 0x200, pilot[23])
        cpu.u32(p + 0x20c, pilot[24])
        cpu.u32(p + 0x228, emu.x228 if pilot[25] else 0)
        cpu.u32(emu.x228 + 4, pilot[26])
        # the loadout's items
        vts = {1: 0x5c0fc0, 2: 0x5bd150, 3: 0x5bd180}
        mem = []
        for n, (kind, count, group, part, ident, tech) in enumerate(items):
            it = emu.items_mem[n]
            cpu.write(it, bytes(0x20))
            cpu.u32(it, vts[kind])
            cpu.u32(it + 4, count)
            cpu.u32(it + 8, group)
            cpu.u32(it + 0xc, part)
            cpu.u32(it + 0x14, ident if kind != 3 else 0)
            cpu.u32(it + 0x18, ident if kind == 3 else tech)
            cpu.u32(it + 0x1c, tech)
            mem.append(it)
        emu.build_list(emu.config + 8, mem, pool)
        self.item_mem = mem
        flat = [v for row in items for v in row]
        imgs = b''.join(bytes(b) for b in parts)
        probe.probe_damage_reset((ctypes.c_uint32 * 13)(*emu.parts), emu.mech)
        probe.probe_damage_load(bytes(mech), imgs, (ctypes.c_int32 * 27)(*pilot),
                                (ctypes.c_int32 * max(1, len(flat)))(*flat), len(items),
                                (ctypes.c_int32 * len(plist))(*plist), len(plist),
                                (ctypes.c_int32 * max(1, len(actions)))(*actions), len(actions), seed)
        emu.rand = MsvcRand(seed)
        # the world for this mech
        self.world = {'player': rng.random() < 0.7, 'mission': rng.random() < 0.7, 'stats': rng.random() < 0.5,
                      'difficulty': rng.choice([0.7, 1.0, 1.3]), 'toggle': rng.random() < 0.5,
                      'time': f32(rng.uniform(0, 500)), 'dt': rng.choice([1 / 30, 1 / 60, 0.1]),
                      'allies': rng.choice([0, 1, 2, 7]), 'shot': [0.0] * 6}
        self.matrix = self.make_matrix(rng)
        cpu.write(emu.data[TORSO] + 0x60, struct.pack('<12f', *self.matrix))

    def make_matrix(self, rng):
        if rng.random() < 0.3:
            return [1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0]
        a = rng.uniform(-math.pi, math.pi)
        c, s = math.cos(a), math.sin(a)
        t = [rng.uniform(-200, 200), rng.uniform(0, 20), rng.uniform(-200, 200)]
        return [f32(v) for v in (c, 0, -s, 0, 1, 0, s, 0, c, *t)]

    def set_world(self):
        emu, cpu, w = self.emu, self.emu.cpu, self.world
        cpu.u32(emu.pl + 0x64, emu.mech if w['player'] else emu.other)
        cpu.u32(emu.mission + 0x14, emu.mech if w['mission'] else emu.other)
        cpu.u32(emu.other + 0xc4, w['allies'])
        cpu.u32(0x608620, emu.stats_obj if w['stats'] else 0)
        cpu.write(0x608e5c, struct.pack('<f', w['difficulty']))
        cpu.write(0x9a0ec4, struct.pack('<6f', *w['shot']))
        cpu.write(0x605cc0, bytes([w['toggle']]))
        cpu.write(0x60991c, struct.pack('<f', w['time']))
        cpu.write(0x72c52c, struct.pack('<f', w['dt']))
        v = [int(w['player']), int(w['mission']), int(w['stats']), f2i(w['difficulty'])] + \
            [f2i(x) for x in w['shot']] + [int(w['toggle']), f2i(w['time']), f2i(w['dt']), w['allies']]
        probe.probe_damage_world((ctypes.c_int32 * 14)(*v))

    def script(self, rng):
        emu = self.emu
        sc = {'attacker': rng.choice([0, emu.attacker, emu.attacker, emu.mech]),
              'screen': rng.choice([0, emu.screen, emu.screen]), 'x1c': rng.choice([0, 0x6b00]),
              'view': rng.choice([-1, 0, 1, 1]), 'owner': 0, 'fly_node': rng.choice([0, 0x6c00, 0x6c00]),
              'blend': 0x6d00 + rng.randrange(16), 'game_type': rng.choice([0, 0, 1, 3]),
              'force0': int(rng.random() < 0.15), 'networked': int(rng.random() < 0.5),
              'anim_ok': int(rng.random() < 0.7), 'hud167c': int(rng.random() < 0.5),
              'hud1514': rng.choice([0, 0x10, 0x30]), 'wcrit': int(rng.random() < 0.6),
              'next_effect': 0x7000 + rng.randrange(0x100) * 0x10,
              'throttle': f32(rng.choice([-1.0, -0.2, 0.0, 0.5, 1.0])),
              'fly': [[f32(rng.uniform(-5, 5)) for _ in range(3)] for _ in range(3)],
              'position': [f32(rng.uniform(-100, 100)) for _ in range(3)]}
        self.sc = emu.sc = sc
        cpu = emu.cpu
        cpu.write(emu.k, struct.pack('<f', sc['throttle']))
        cpu.u32(emu.k + 4, sc['game_type'])
        cpu.u32(emu.pl + 0x58, 0 if sc['view'] < 0 else emu.view)
        cpu.u32(emu.view + 4, max(sc['view'], 0))
        cpu.write(emu.screen + 0x167c, bytes([sc['hud167c']]))
        cpu.u32(emu.screen + 0x1514, sc['hud1514'])
        cpu.write(emu.pos, struct.pack('<3f', *sc['position']))
        v = [sc['attacker'], sc['screen'], sc['x1c'], sc['view'], sc['owner'], sc['fly_node'], sc['blend'],
             sc['game_type'], sc['force0'], sc['networked'], sc['anim_ok'], sc['hud167c'], sc['hud1514'],
             sc['wcrit'], sc['next_effect']]
        f = [sc['throttle']] + [x for row in sc['fly'] for x in row] + sc['position'] + self.matrix
        return v, f

    def push_script(self, v, f):
        v[4] = self.sc['owner']
        probe.probe_damage_script((ctypes.c_int32 * 15)(*[s32(x & 0xffffffff) for x in v]),
                                  (ctypes.c_float * 25)(*f))

    def shot(self, rng):
        m = self.matrix
        centre = (m[9], m[10], m[11])
        if rng.random() < 0.05:
            return [f32(rng.uniform(-300, 300)) for _ in range(6)]
        a, r = rng.uniform(-math.pi, math.pi), rng.uniform(20, 400)
        p0 = (centre[0] + r * math.cos(a), centre[1] + rng.uniform(-5, 15), centre[2] + r * math.sin(a))
        p1 = [centre[k] + rng.uniform(-4, 4) for k in range(3)]
        if rng.random() < 0.03:
            p1 = list(p0)
        return [f32(v) for v in (*p0, *p1)]


def read_state(emu, run):
    cpu = emu.cpu
    out = {}

    def get(base, off, kind):
        raw = cpu.read(base + off, 1 if kind == 'b' else 4)
        return raw[0] if kind == 'b' else struct.unpack('<i', raw)[0]

    out['mech'] = [get(emu.mech, o, k) for o, k in MECH_FIELDS]
    out['parts'] = []
    for i in range(13):
        fields = PART_FIELDS + EXTRA.get(i, [])
        out['parts'].append([get(emu.parts[i], o, k) for o, k in fields])
    out['groups'] = [s32(emu.u32(it + 8)) for it in run.item_mem]
    out['list'] = [emu.parts.index(p) for p in emu.read_list(emu.mech + 0x384)]
    out['toggle'] = cpu.read(0x605cc0, 1)[0]
    return out


def read_port(run):
    emu = run.emu
    mech = (ctypes.c_uint8 * MECH_SIZE)()
    parts = (ctypes.c_uint8 * (PART_IMG * 13))()
    groups = (ctypes.c_int32 * 64)()
    plist = (ctypes.c_int32 * 64)()
    n = ctypes.c_int()
    freed = (ctypes.c_int32 * 13)()
    toggle = ctypes.c_int32()
    probe.probe_damage_store(mech, parts, groups, plist, ctypes.byref(n), freed, ctypes.byref(toggle))
    mb, pb = bytes(mech), bytes(parts)

    def get(b, off, kind):
        return b[off] if kind == 'b' else struct.unpack_from('<i', b, off)[0]

    out = {'mech': [get(mb, o, k) for o, k in MECH_FIELDS], 'parts': []}
    for i in range(13):
        fields = PART_FIELDS + EXTRA.get(i, [])
        img = pb[PART_IMG * i:PART_IMG * (i + 1)]
        out['parts'].append(None if freed[i] else [get(img, o, k) for o, k in fields])
    out['groups'] = list(groups[:len(run.item_mem)])
    out['list'] = list(plist[:n.value])
    out['toggle'] = toggle.value
    return out


def diff(a, b, emu):
    bad = []
    for (o, k), x, y in zip(MECH_FIELDS, a['mech'], b['mech']):
        if x != y:
            bad.append(f'mech+{o:#x} {fmt(x, k)} vs {fmt(y, k)}')
    for i in range(13):
        if b['parts'][i] is None:
            continue  # freed in the port; the emulator's hand memory stays
        for (o, k), x, y in zip(PART_FIELDS + EXTRA.get(i, []), a['parts'][i], b['parts'][i]):
            if x != y:
                bad.append(f'{PARTS[i][0]}+{o:#x} {fmt(x, k)} vs {fmt(y, k)}')
    for key in ('groups', 'list', 'toggle'):
        if a[key] != b[key]:
            bad.append(f'{key} {a[key]} vs {b[key]}')
    return bad


def fmt(v, k):
    return f'{i2f(v)!r}' if k == 'f' else hex(v & 0xffffffff)


def show(events):
    return '\n    '.join(f'{NAMES.get(e[0], e[0])}{tuple(hex(x) for x in e[1:])}' for e in events)


def port_events(buf, n):
    out, i = [], 0
    while i < n:
        k = buf[i]
        out.append(tuple(v & 0xffffffff for v in buf[i + 1:i + 1 + k]))
        i += 1 + k
    return out


def run_op(run, op, args, rng):
    """Runs one operation on both; returns (original result, port result, events)."""
    emu, cpu = run.emu, run.emu.cpu
    v, f = run.script(rng)
    if op == 0:
        (flags, heat2, zaps, zaps2, force, narc, wid, nuke, x20, node, dmg) = args
        if node == 'area':
            node = emu.area_node
            owner = rng.choice([0, emu.mech, emu.other])
            cpu.write(node, bytes(0x100))
            cpu.u32(node + 0x28, 0x1000 if owner else 0)
            cpu.u32(node + 0x40, owner)
            run.sc['owner'] = owner
        else:
            node = emu.nodes[node]
        cpu.write(emu.def_, bytes(0x200))
        cpu.u32(emu.def_ + 8, emu.nuke if nuke else emu.lrm)
        cpu.u32(emu.def_ + 0x6c, flags)
        cpu.u32(emu.def_ + 0x1f0, emu.type_)
        cpu.write(emu.type_, bytes(0xd0))
        cpu.u32(emu.type_, wid)
        for off, x in ((0xc, heat2), (0x14, zaps), (0x18, zaps2), (0x48, force)):
            cpu.write(emu.type_ + off, struct.pack('<f', x))
        cpu.write(emu.type_ + 0x4c, bytes([narc]))
        cpu.write(emu.coll, bytes(0x40))
        cpu.u32(emu.coll + 0x20, x20)
        cpu.u32(emu.coll + 0x24, node)
        pargs = [flags, f2i(heat2), f2i(zaps), f2i(zaps2), f2i(force), narc, wid, nuke, x20, node, f2i(dmg)]
    else:
        pargs = list(args)
    run.push_script(v, f)
    run.set_world()
    emu.events = []
    if op == 0:
        cpu.call(TAKE_DAMAGE, ecx=emu.mech, stack_args=(emu.def_, emu.coll, f2i(dmg) & 0xffffffff))
        res = cpu.uc.reg_read(UC_X86_REG_EAX)
    elif op == 1:
        part = emu.parts[args[0]]
        fn = emu.u32(emu.u32(part) + 0x28)
        cpu.call(fn, ecx=part, stack_args=(args[1], args[2]))
        res = cpu.uc.reg_read(UC_X86_REG_EAX) & 0xff
    elif op == 2:
        cpu.call(DESTROY_PART, ecx=emu.mech, stack_args=(args[0],))
        res = 0
    elif op == 3:
        cpu.call(START_LIMP, ecx=emu.mech, stack_args=(args[0], args[1]))
        res = cpu.uc.reg_read(UC_X86_REG_EAX) & 0xff
    elif op == 4:
        cpu.call(KNOCKED_DOWN, ecx=emu.mech, stack_args=(args[0],))
        res = int((cpu.uc.reg_read(UC_X86_REG_EAX) & 0xff) != 0)  # AL is the kind's low byte for other kinds
    else:
        cpu.call(HIT_SECTION, ecx=emu.parts[TORSO], stack_args=(0x9a0ec4, 0))
        res = cpu.uc.reg_read(UC_X86_REG_EAX)
    a_events = emu.events
    result = ctypes.c_int32()
    buf = (ctypes.c_int32 * 8192)()
    n = probe.probe_damage_op(op, (ctypes.c_int32 * 16)(*[s32(x & 0xffffffff) for x in pargs]),
                              ctypes.byref(result), buf, 8192)
    if n < 0:
        raise SystemExit('port log overflow')
    b_events = port_events(buf, n)
    pres = result.value & (0xff if op in (1, 3, 4) else 0xffffffff)
    return res, pres, a_events, b_events


def random_hit(rng):
    flags = rng.choice([0, 0, 0, 0x6, 0x2, 0x4000])
    heat2 = f32(rng.choice([0.0, 0.0, 0.0, 0.05, 0.3]))
    zaps = f32(rng.choice([0.0, 0.0, 0.5, 2.0, -1.0]))
    zaps2 = f32(rng.choice([0.0, 1.0, 3.0]))
    force = f32(rng.choice([0.0, 0.0, 1.5, 5.0, 12.0]))
    narc = int(rng.random() < 0.04)
    wid = rng.randrange(37)
    nuke = int(rng.random() < 0.05)
    x20 = rng.choice([0, 0x6e00])
    r = rng.random()
    node = 'area' if r < 0.12 else rng.randrange(13)
    dmg = f32(rng.choice([rng.uniform(0.2, 12), rng.uniform(5, 40), rng.uniform(40, 400), 0.0]))
    return (flags, heat2, zaps, zaps2, force, narc, wid, nuke, x20, node, dmg)


def main():
    emu = Emulated()
    run = Run(emu)
    rng = random.Random(3025)
    scenarios = int(os.environ.get('DAMAGE_SCENARIOS', '1500'))
    ops = calls = 0
    codes, sections = set(), set()
    for n in range(scenarios):
        seed = rng.randrange(1 << 31)
        run.load(rng, seed)
        for step in range(rng.randrange(6, 40)):
            run.world['shot'] = run.shot(rng)
            run.world['time'] = f32(run.world['time'] + 0.25)
            r = rng.random()
            if r < 0.72:
                op, args = 0, random_hit(rng)
            elif r < 0.86:
                part = rng.randrange(13)
                kind = rng.randrange(5)
                op, args = 1, (part, kind, rng.randrange(6) if part == TORSO else 0)
            elif r < 0.89:
                op, args = 2, (rng.choice([1, 2, 3, 4, 5, 6, 15, 7, 0]),)
            elif r < 0.93:
                op, args = 3, (rng.choice([0, 1, 2]), int(rng.random() < 0.5))
            elif r < 0.96:
                op, args = 4, (rng.choice([3, 4, 0, 1, 7]),)
            else:
                op, args = 5, ()
            a_res, b_res, a_ev, b_ev = run_op(run, op, args, rng)
            if op == 5:
                sections.add(a_res)
            ga, gb = read_state(emu, run), read_port(run)
            bad = diff(ga, gb, emu)
            if bad or a_ev != b_ev or a_res != b_res:
                raise SystemExit(f'scenario {n} step {step} op {op} {args}:\n  ' + '\n  '.join(bad[:20]) +
                                 f'\n  result {a_res:#x} vs {b_res:#x}' +
                                 f'\n  original calls:\n    {show(a_ev)}\n  port calls:\n    {show(b_ev)}')
            ops += 1
            calls += len(a_ev)
            codes.update(e[0] for e in a_ev)
    missing = set(NAMES) - codes
    unreached = emu.cover_names - emu.reached
    if missing or unreached or len(sections) < 6:
        raise SystemExit(f'coverage: calls never made {sorted(missing)}; paths never reached {sorted(unreached)}; '
                         f'sections {sorted(sections)}')
    print(f'ok: {scenarios} mechs, {ops} operations agree; {calls} calls out matched; '
          f'all {len(NAMES)} kinds of call and {len(emu.reached)} paths reached '
          f'(every critical, CASE, difficulty, multiplayer, structure at 0, deaths)')


if __name__ == '__main__':
    main()
