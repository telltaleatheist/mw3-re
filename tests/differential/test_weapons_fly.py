"""zWeapon's projectiles (docs/weapons.md, "Projectiles") against the original x86:
the pool 0x584df0 / 0x584e60 / 0x584f40 / 0x584fb0 / 0x584fe0 and its scene
nodes 0x5847c0, the FLYOUT model pool 0x5830b0 / 0x5830e0, the spawn 0x585010
with the static-hit cast, the tick 0x5823c0 with update 0x585710, homing
0x585900 (the intercept 0x524247, the slerp 0x51e460), collision 0x5861f0 with
0x586160 / 0x5861c0, the hit pick 0x583130, the classify 0x582b10, the commit
0x5858b0 / 0x5864c0 / 0x586600, mines 0x586670 / 0x586740, the bursts
0x5821c0 / 0x5820c0 / 0x5822c0, the impact sounds 0x582f60 / 0x582fc0, the
aim slope 0x581ff0, the pool count 0x581e50 and the light pool 0x586970 /
0x5869f0 / 0x586b20 / 0x586b70.

    cmake --build build && .venv/bin/python tests/differential/test_weapons_fly.py

Sessions on the real weapons.zrd (test_weapons_zrd.py), with unshipped
features poked into some defs the same way on both sides (mines, tethered
and lead-homing missiles, gravity, INSTANT, HIT_OWNER, pooled FLYOUT models).
Each session runs a few dozen random operations (spawns, frames, detonations,
light takes) on one fake scene. The scene and the engine's answers live here
in Python for both sides: node parents and children, the queries (segments,
ground, splash), damage handlers, new nodes and animation handles, answered
in call order from the operation's seed. Every call out is logged on both
sides; after each operation every pool slot, the weapon lists, the fire / hit
/ run globals and the scene must match. Floats match bit for bit except for
the CRT's trig against the x87's (asin, acos, atan2, sin, cos), compared
within 3e-5 of the size of the value, or of 1 for smaller values (MW3_FLY_TOL): the
difference grows over a homing missile's frames.
"""
import ctypes
import math
import os
import random
import struct
import sys
from pathlib import Path

os.environ.setdefault('MW3_X87', '24')
sys.path.insert(0, str(Path(__file__).parent))
import original  # noqa: E402
import test_weapons_zrd as zr  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

TOL = float(os.environ.get('MW3_FLY_TOL', '3e-5'))
probe = zr.probe
PYFN = ctypes.CFUNCTYPE(ctypes.c_int32, ctypes.c_int32, ctypes.POINTER(ctypes.c_int32), ctypes.c_int32,
                        ctypes.POINTER(ctypes.c_int32))
probe.probe_fly_setup.argtypes = [PYFN]
probe.probe_fly_globals.argtypes = [ctypes.c_void_p]
probe.probe_fly_op.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_uint32, ctypes.c_void_p]
probe.probe_fly_op.restype = ctypes.c_int32
probe.probe_fly_poke.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int32]
probe.probe_fly_slot.argtypes = [ctypes.c_int, ctypes.c_void_p]
probe.probe_fly_world.argtypes = [ctypes.c_void_p]

s32 = zr.s32
WORLD = zr.WORLD
IAT_RAND, IAT_ASIN, IAT_ACOS = 0x5bb668, 0x5bb5f4, 0x5bb554
ASIN = bytes.fromhex('d9c0 d8c8 d9e8 dee1 d9fa d9f3 c3')       # atan2(x, sqrt(1 - x x))
ACOS = bytes.fromhex('d9c0 d8c8 d9e8 dee1 d9fa d9c9 d9f3 c3')  # atan2(sqrt(1 - x x), x)
NAMES = {1: 'add_child', 2: 'remove_child', 3: 'obj_add_child', 4: 'obj_remove_child', 5: 'clone',
         6: 'set_intersect', 9: 'scale', 10: 'rotation', 11: 'translation', 12: 'rotate', 13: 'object3d_create',
         14: 'set_name', 15: 'flag620', 16: 'flag750', 17: 'node_free', 18: 'node_destroy', 19: 'pool_live',
         20: 'light_create', 21: 'light_color', 22: 'light_range', 23: 'light_position', 24: 'add_light',
         25: 'remove_light', 26: 'anim_attached', 27: 'anim_follow', 28: 'anim_model', 29: 'anim_on_done',
         30: 'anim_stop', 31: 'effect', 32: 'impact_effect', 33: 'sound_at', 34: 'impact_type',
         35: 'query_static', 36: 'query_since', 37: 'query_mask', 38: 'segment_list', 39: 'segment',
         40: 'ground', 41: 'net_gate', 42: 'net_detonated', 43: 'splash_query', 44: 'handler',
         45: 'surface_mark', 46: 'crater', 47: 'quicksand', 49: 'surface_angles', 51: 'tick_cb', 52: 'set_active',
         53: 'flag4fbb90', 54: 'uniform_scale', 55: 'sound_stop', 56: 'sound_frequency', 57: 'anim_free',
         79: 'flag4eb790', 83: 'touch_fn', 75: 'crater_defaults', 76: 'crater_make', 77: 'quicksand_defaults', 78: 'quicksand_make'}
FLOATS = {9: (1, 2, 3), 10: (1, 2, 3), 11: (1, 2, 3), 12: (1, 2, 3), 21: (1, 2, 3), 22: (1, 2), 23: (1, 2, 3),
          31: tuple(range(1, 10)), 32: (1, 2, 3), 33: (1, 2, 3, 4), 38: tuple(range(6)), 39: tuple(range(6)),
          40: (0, 1, 2), 42: (2, 3, 4), 43: (1, 2, 3, 4), 44: (2, 3, 4, 6), 45: (0, 1, 2), 46: (1, 2, 3),
          47: (1, 2, 3), 49: (0, 1, 2), 54: (1,), 56: (1,), 76: (4, 5, 6, 7, 8), 78: (5, 6, 7, 8, 9), 83: (2,)}
COVER = {
    0x584e1a: 'pooled flyout model', 0x5830cb: 'model cloned', 0x5830ba: 'model from the pool',
    0x584ebd: 'model returned', 0x584eb6: 'model detached', 0x585063: 'pool empty', 0x58503c: 'network gate refuses',
    0x585212: 'static cast', 0x58529b: 'static cast empty', 0x585302: 'static sort swap',
    0x585385: 'accelerating start', 0x58544c: 'fire sound', 0x58545e: 'fire effect', 0x5854d5: 'fire attached',
    0x58551f: 'fire animation', 0x58548f: 'fire random roll', 0x58556f: 'flyout rotation', 0x5855b1: 'flyout anim',
    0x5855d1: 'flyout attached', 0x585600: 'model animation', 0x585662: 'lock copied', 0x585687: 'shootable',
    0x58573b: 'tick callback', 0x58574d: 'no zone', 0x58578a: 'destroy animation', 0x585798: 'shot down: burst',
    0x58580a: 'mine waits', 0x5857c6: 'homing', 0x5857e2: 'instant', 0x585871: 'hit', 0x58587e: 'mine lands',
    0x5858e2: 'retired', 0x5858cf: 'committed', 0x586533: 'gravity faces velocity', 0x586568: 'mine spins',
    0x5865a0: 'mine grows', 0x58665c: 'tether leak', 0x586182: 'owner kept', 0x5861a8: 'own node hidden',
    0x586239: 'static walk', 0x586272: 'static moved node', 0x5862a1: 'static reached', 0x586313: 'static vs dynamic',
    0x586337: 'dynamic nearer', 0x583185: 'water splash', 0x5831f0: 'water: next record', 0x583203: 'water only',
    0x582b62: 'shot line recorded', 0x582ba6: 'bounce', 0x582bf7: 'passes', 0x5866a2: 'mine timed out',
    0x5866d7: 'mine goes off', 0x586713: 'mine idles', 0x5867b3: 'mine bounces', 0x58684e: 'bounce sound',
    0x5868d9: 'mine settles', 0x5868ec: 'mine timer', 0x58688e: 'mine bounce hits again', 0x5868b7: 'mine hits a handler',
    0x58223b: 'ground burst impact', 0x582289: 'ground burst splash', 0x582111: 'remote detonate',
    0x58213e: 'detonate at a point', 0x58219b: 'network told', 0x585947: 'lead intercept', 0x585971: 'full lead',
    0x585985: 'lead slerp', 0x5859e6: 'suspend ramp', 0x5859de: 'no suspend', 0x585a1f: 'ground probe',
    0x585a89: 'tether ceiling', 0x585ab5: 'fcs climb probe', 0x585b05: 'fcs climb', 0x585b26: 'no ground',
    0x585bc6: 'turn limited', 0x585bb1: 'turn reached', 0x585c32: 'base velocity fades', 0x585c16: 'past lock-on',
    0x524331: 'intercept a 0', 0x52437c: 'intercept a < 0', 0x5243f3: 'intercept other root',
    0x51e514: 'slerp opposite x 0', 0x51e524: 'slerp opposite', 0x51e620: 'slerp linear', 0x51e690: 'slerp arc',
    0x51e483: 'slerp t 0', 0x51e4af: 'slerp t 1', 0x582f85: 'impact sound', 0x582fe5: 'bounce sound pick',
    0x582064: 'aim slope gravity', 0x582095: 'aim slope mine', 0x5869f0: 'lights freed', 0x586b2f: 'light taken',
    0x586b70: 'light returned', 0x5847de: 'pool objects freed',
    0x58330e: 'handler: new table', 0x583359: 'handler: own slot', 0x58339f: 'handler: slot taken',
    0x583381: 'handler: split from the parent', 0x5833ce: 'handler: flag', 0x583420: 'handler: removed',
    0x5832d7: 'handler: handed down', 0x5836c5: 'touch: line recorded',
    0x4b3b5c: 'AMS: nearer shot', 0x4b3b21: 'AMS: foes only', 0x4b3b2d: 'AMS: friends only',
    0x4b3bd0: 'AMS: one system', 0x4b3bd8: 'AMS: several share the period', 0x4b3ae8: 'AMS: out of rounds',
    0x5825cb: 'crater', 0x582602: 'crater points', 0x58263a: 'crater radius', 0x58266d: 'crater depth',
    0x5824d5: 'quicksand', 0x582505: 'quicksand points',
    0x584ab3: 'beam: 8 segments at most', 0x584ac2: 'beam: designate one', 0x584b11: 'beam: segment flag',
    0x58126f: 'beam: designate silent', 0x581281: 'beam: fire effect', 0x5812aa: 'beam: random roll',
    0x581312: 'beam: fire animation', 0x581390: 'beam: lock', 0x5813b5: 'beam: designate light',
    0x58140f: 'beam: loop stopped', 0x581437: 'beam: animation freed', 0x58147b: 'beam: light back',
    0x58199f: 'beam: grows', 0x5819c4: 'beam: snaps', 0x5819bb: 'beam: clamped', 0x581ab9: 'beam: owner hidden',
    0x581b11: 'beam: hit', 0x581ba4: 'beam: designating', 0x581bdc: 'beam: designate impact',
    0x581c67: 'beam: burns', 0x581c8c: 'beam: damage', 0x581cbe: 'beam: designate off', 0x581ce1: 'beam: volume',
    0x581cea: 'beam: timer out', 0x581d0e: 'beam: ground animation', 0x581d3a: 'beam: bounce',
    0x581d90: 'beam: designate lost', 0x581566: 'multi: chain', 0x581583: 'multi: four at most',
    0x58165c: 'multi: kinks', 0x58171e: 'multi: chain goes on', 0x5817d1: 'multi: last target', 0x58181f: 'multi: one lock',
    0x5818e9: 'multi: one lock reached', 0x58192c: 'multi: no lock', 0x582c50: 'multi: farthest',
    0x582d44: 'multi: sorted', 0x582e61: 'multi: cast handler', 0x582e7b: 'multi: cast damage', 0x582ed1: 'multi: cast miss',
}


def f2i(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def i2f(i):
    return struct.unpack('<f', struct.pack('<I', i & 0xffffffff))[0]


def f32(x):
    return i2f(f2i(x))


class MsvcRand:
    def __init__(self, seed):
        self.state = seed

    def __call__(self, cpu=None):
        self.state = (self.state * 214013 + 2531011) & 0xffffffff
        return (self.state >> 16) & 0x7fff


def _stub_function(self, addr, fn, pop_bytes=0):
    """One hook per address, so a stub can be swapped or passed through."""
    first = addr not in self.stubs
    self.stubs[addr] = (fn, pop_bytes)
    if first:
        self.uc.hook_add(UC_HOOK_CODE, self._fly_hit, begin=addr, end=addr)


def _fly_hit(self, uc, address, size, user):
    if address in getattr(self, 'passthrough', ()):
        return
    self._stub_hit(uc, address, size, user)


original.Original.stub_function = _stub_function
original.Original._fly_hit = _fly_hit


class Node:
    def __init__(self):
        self.parents, self.children = [], []
        self.flags = self.handler = self.user = self.frame = 0


class Scene:
    """One side's scene: node -> Node. The emulator's side mirrors parents and
    children into the nodes' memory; flags, +0x40 and +0xbc are written by the
    original itself, so for it those are read from memory."""

    def __init__(self, emu=None):
        self.emu, self.nodes = emu, {}

    def get(self, n):
        if n not in self.nodes:
            self.nodes[n] = Node()
        return self.nodes[n]

    def mirror(self, n):
        e = self.emu
        if e is None or n not in e.mem_nodes:
            return
        nd, cpu = self.get(n), e.cpu
        pa, ca = e.mem_nodes[n]
        cpu.u32(n + 0x54, len(nd.parents))
        cpu.u32(n + 0x5c, len(nd.children))
        cpu.write(pa, struct.pack(f'<{len(nd.parents)}I', *nd.parents))
        cpu.write(ca, struct.pack(f'<{len(nd.children)}I', *nd.children))

    def add(self, parent, child):
        self.get(parent).children.append(child)
        self.get(child).parents.append(parent)
        self.mirror(parent)
        self.mirror(child)

    def remove(self, parent, child):
        p, c = self.get(parent), self.get(child)
        if child in p.children:
            p.children.remove(child)
        if parent in c.parents:
            c.parents.remove(parent)
        self.mirror(parent)
        self.mirror(child)


class Fly(zr.Emulated):
    def __init__(self):
        super().__init__()
        cpu = self.cpu
        self.zr_stubs = dict(cpu.stubs)
        cpu.import_code(IAT_ASIN, ASIN)
        cpu.import_code(IAT_ACOS, ACOS)
        self.rand = MsvcRand(1)
        cpu.stub_import(IAT_RAND, lambda c: self.rand())
        cpu.stub_import(0x5bb4c8, lambda c: 0)  # operator delete
        cpu.stub_import(0x5bb4bc, lambda c: self.zalloc(max(c.arg(0), 1)))  # operator new
        cpu.stub_import(0x5bb658, self.sprintf)
        cpu.stub_import(0x5bb65c, lambda c: 0)  # free
        huge = cpu.code(struct.pack('<d', math.inf))  # _HUGE, the CRT's double infinity (kept over sessions)
        cpu.u32(0x5bb5bc, huge)
        self.handler_fn = cpu.code(b'\xc2\x08\x00')
        self.gate_fn = cpu.code(b'\xc3')
        self.det_fn = cpu.code(b'\xc2\x04\x00')
        self.tick_fn = cpu.code(b'\xc3')
        self.touch_fns = [cpu.code(b'\xc2\x04\x00'), cpu.code(b'\xc2\x04\x00'), cpu.code(b'\xc2\x10\x00')]
        self.reached = set()

        def hit(uc, address, size, user):
            self.reached.add(address)
        for addr in COVER:
            cpu.uc.hook_add(UC_HOOK_CODE, hit, begin=addr, end=addr)
        real = zr.real_bytes()
        self.real = real
        zr.WORK.mkdir(parents=True, exist_ok=True)
        self.arch = zr.WORK / 'fly.zbd'
        zr.write_archive(self.arch, real)
        self.cb = PYFN(self.port_call)
        self.after_init = None  # the memory after the first session's reader run

    def sprintf(self, c):
        out = (self.cstr(c.arg(1)).decode() % c.arg(2)).encode()
        c.write(c.arg(0), out + b'\0')
        return len(out)

    def zalloc(self, n):
        p = self.cpu.alloc(n)
        self.cpu.write(p, bytes(n))
        return p

    def f(self, p):
        return struct.unpack('<f', self.cpu.read(p, 4))[0]

    def fa(self, n):
        """Stack argument n as a float's bits."""
        return self.cpu.arg(n)

    # --- the shared world ------------------------------------------------------
    def new_session(self, rng):
        cpu = self.cpu
        cpu.stubs.update(self.zr_stubs)
        cpu.passthrough = set()
        # The reader 0x583880 on the real weapons.zrd once (4.5 s here under
        # the hooks); each later session starts from a copy of the memory it
        # left, so 200-400 sessions do not spend half an hour reading the file.
        if self.after_init is None:
            assert self.run(self.real, [0] * 10, 1, 1) == 0
            self.after_init = ([(b, bytes(cpu.uc.mem_read(b, e - b + 1))) for b, e, _ in cpu.uc.mem_regions()],
                               cpu.heap_next)
        else:
            regions, cpu.heap_next = self.after_init
            for b, data in regions:
                cpu.uc.mem_write(b, data)
        buf, n = (ctypes.c_int32 * 400000)(), ctypes.c_int()
        assert probe.probe_zwep_run(str(self.arch).encode(), (ctypes.c_uint32 * 10)(*[0] * 10), 1, 1, buf, 400000,
                                    ctypes.byref(n)) == 0
        self.defs_base, self.ndefs = self.u32(0x9a0eac), self.u32(0x9a0ea8)
        self.pool_base, self.npool = self.u32(0x9a0eb4), self.u32(0x9a0eb0)
        self.names = {self.cstr(self.u32(self.defs_base + 0x1f4 * k)).decode(): k for k in range(self.ndefs)}
        self.install()
        probe.probe_fly_setup(self.cb)
        # the scene: fresh nodes handed out in order, world nodes with handlers
        self.mem_nodes = {}
        self.fresh = []
        for _ in range(420):
            nd = self.zalloc(0x100)
            self.mem_nodes[nd] = (self.zalloc(4 * 16), self.zalloc(4 * 48))
            cpu.u32(nd + 0x58, self.mem_nodes[nd][0])
            cpu.u32(nd + 0x60, self.mem_nodes[nd][1])
            self.fresh.append(nd)
        self.next_fresh = {'orig': 0, 'port': 0}
        self.handles = {'orig': 0, 'port': 0}
        self.scene = {'orig': Scene(self), 'port': Scene()}
        self.world_nodes, self.rows = [], {}
        for i in range(8):
            nd = self.fresh.pop()
            kind = rng.choice(['handler', 'handler', 'none'])
            hd = 0
            if kind == 'handler':
                hd = self.zalloc(0x10)
                cpu.u32(hd, hd)
                cpu.u32(hd + 4, self.handler_fn if rng.random() < 0.9 else 0)
                self.rows[hd] = (rng.choice([0, 0, 1, 2]), rng.choice([0, 0, 0, 0x6600 + i]),
                                 f2i(f32(rng.choice([1.0, 0.6, 0.3, 0.1, 0.0]))), self.u32(hd + 4) != 0)
            frame = rng.choice([0, 0, 0, 50, 10 ** 6])
            if rng.random() < 0.6:  # takes craters
                cpu.u32(nd + 0x24, 0x10000)
                self.scene['port'].get(nd).flags = 0x10000
            cpu.u32(nd + 0xbc, hd)
            cpu.u32(nd + 0xc4, frame)
            pn = self.scene['port'].get(nd)
            pn.handler, pn.frame = hd, frame
            self.world_nodes.append(nd)
        # a little tree for the handler slots: 0 > 1, 2; 1 > 3, 4; 2 > 4, 5 (4 has two parents)
        self.tree = [self.fresh.pop() for _ in range(6)]
        for pa, ch in ((0, 1), (0, 2), (1, 3), (1, 4), (2, 4), (2, 5)):
            for side in ('orig', 'port'):
                self.scene[side].add(self.tree[pa], self.tree[ch])
        self.mats = []
        for i in range(6):
            m = self.zalloc(0x40)
            soil = rng.choice([0, 0, 1, 1, 2, 3, 4, 5])
            cpu.u32(m + 0x20, soil)
            self.mats.append((m, soil))
        self.soils = dict(self.mats)
        self.pool_live = int(rng.random() < 0.9)
        self.crater_tpl = [rng.randrange(1 << 16) for _ in range(4)] + [f2i(f32(rng.uniform(0, 3))) for _ in range(5)] + \
            [rng.randrange(1 << 16) for _ in range(2)]
        self.lock = [self.zalloc(4), self.zalloc(12 * 8), [self.zalloc(12) for _ in range(8)],
                     [self.zalloc(12) for _ in range(8)]]  # flag, the LockRef array, positions, velocities
        self.beams = {}     # slot -> the emulator's beam
        self.beam_at = [(self.zalloc(12), self.zalloc(12), self.zalloc(4)) for _ in range(16)]
        self.taken_lights = []
        self.beams, self.beam_state = {}, {}
        self.ams = 0
        self.ahead = [0.0, 0.0, -400.0]
        # the radar list's head (the ctor at 0x4b4970)
        cpu.call(0x4b4970)

    def install(self):
        cpu = self.cpu
        stubs = {
            0x4eb9b0: (lambda c: self.ev('orig', 1, c.ecx, c.edx) or self.scene['orig'].add(c.ecx, c.edx), 0),
            0x4ebb40: (lambda c: self.ev('orig', 2, c.ecx, c.edx) or self.scene['orig'].remove(c.ecx, c.edx), 0),
            0x4fb7a0: (lambda c: self.ev('orig', 3, c.ecx, c.edx) or self.scene['orig'].add(c.ecx, c.edx), 0),
            0x4fb7f0: (lambda c: self.ev('orig', 4, c.ecx, c.edx) or self.scene['orig'].remove(c.ecx, c.edx), 0),
            0x4f9af0: (lambda c: self.check(c.edx == 0 and c.arg(0) == 1) or self.fresh_node('orig', 5, c.ecx), 4),
            0x4eb6d0: (self.orig_intersect, 0),
            0x4fbc10: (lambda c: self.ev('orig', 9, c.ecx, c.arg(0), c.arg(1), c.arg(2)), 12),
            0x4fbd80: (lambda c: self.ev('orig', 10, c.ecx, c.arg(0), c.arg(1), c.arg(2)), 12),
            0x4fc0b0: (lambda c: self.ev('orig', 11, c.ecx, c.arg(0), c.arg(1), c.arg(2)), 12),
            0x4fbef0: (lambda c: self.ev('orig', 12, c.ecx, c.arg(0), c.arg(1), c.arg(2)), 12),
            0x4fb740: (lambda c: self.fresh_node('orig', 13), 0),
            0x4eb300: (lambda c: self.ev('orig', 14, c.ecx, zr.fnv(self.cstr(c.edx))), 0),
            0x4eb620: (lambda c: self.ev('orig', 15, c.ecx, c.edx), 0),
            0x4eb750: (lambda c: self.ev('orig', 16, c.ecx, c.edx), 0),
            0x4eb170: (lambda c: self.ev('orig', 52, c.ecx, c.edx), 0),
            0x4f9050: (lambda c: self.ev('orig', 17, c.ecx), 0),
            0x4eaea0: (lambda c: self.ev('orig', 18, c.ecx), 0),
            0x4f8f50: (lambda c: self.ev('orig', 19) or self.pool_live, 0),
            0x4fa720: (lambda c: self.fresh_node('orig', 20), 0),
            0x4faf80: (lambda c: self.ev('orig', 21, c.ecx, c.arg(0), c.arg(1), c.arg(2)), 12),
            0x4fadf0: (lambda c: self.ev('orig', 22, c.ecx, c.arg(0), c.arg(1)), 8),
            0x4fb510: (lambda c: self.ev('orig', 23, c.ecx, c.arg(0), c.arg(1), c.arg(2)), 12),
            0x4f2d50: (lambda c: self.check(c.ecx == WORLD) or self.ev('orig', 24, c.edx), 0),
            0x4f2e00: (lambda c: self.check(c.ecx == WORLD) or self.ev('orig', 25, c.edx), 0),
            0x508da0: (lambda c: self.check(c.edx == 0 and c.arg(2) == 0) or
                       self.handle('orig', 26, c.ecx, c.arg(0), c.arg(1)), 12),
            0x508ee0: (lambda c: self.check(c.edx == 0 and c.arg(1) == 0 and c.arg(3) == 0 and c.arg(0) == c.arg(2))
                       or self.handle('orig', 27, c.ecx, c.arg(0)), 16),
            0x508c10: (lambda c: self.check(c.arg(0) == 0 and c.arg(1) == 0 and c.arg(2) == 0) or
                       self.handle('orig', 28, c.ecx, c.edx), 12),
            0x508f00: (lambda c: self.check(c.edx == 0x583120) or self.ev('orig', 29, c.ecx, self.ptok(c.arg(0))), 4),
            0x508460: (lambda c: self.check(c.edx == 0) or self.ev('orig', 30, c.ecx), 0),
            0x508aa0: (lambda c: self.check(c.edx == 0) or self.handle('orig', 31, c.ecx, *[c.arg(i) for i in range(9)]),
                       0x24),
            0x50d650: (lambda c: self.ev('orig', 32, c.ecx, *self.vec(c.edx)), 0),
            0x56e7e0: (lambda c: self.check(c.arg(1) == 0) or self.handle('orig', 33, c.ecx, *self.vec(c.edx), c.arg(0)),
                       8),
            0x5426a0: (lambda c: self.ev('orig', 34, c.ecx), 0),
            0x4ed9d0: (lambda c: self.ev('orig', 35, c.ecx), 0),
            0x4ed9e0: (lambda c: self.ev('orig', 36, c.ecx, c.edx), 0),
            0x4eda00: (lambda c: self.ev('orig', 37, c.ecx), 0),
            0x4eed90: (self.orig_segment_list, 0x18),
            0x4eece0: (self.orig_segment, 8),
            0x4eda10: (self.orig_ground, 4),
            0x4f0ac0: (self.orig_splash_query, 0x14),
            0x542060: (lambda c: self.ev('orig', 45, *self.rec_args(c.ecx)), 0),
            0x4ff920: (lambda c: self.ev('orig', 75) or self.cpu.write(c.ecx, struct.pack('<10I', *self.crater_tpl[:10])), 0),
            0x4ffaa0: (lambda c: self.ev('orig', 76, *struct.unpack('<10I', self.cpu.read(c.ecx, 40))) or
                       self.crater_answer('orig'), 0),
            0x5020f0: (lambda c: self.ev('orig', 77) or self.cpu.write(c.ecx, struct.pack('<11I', *self.crater_tpl)), 0),
            0x502110: (lambda c: self.ev('orig', 78, *struct.unpack('<11I', self.cpu.read(c.ecx, 44))), 0),
            0x525a50: (lambda c: self.ev('orig', 49, *self.vec(c.edx)), 4),
            0x525f80: (lambda c: self.cpu.write(c.edx, struct.pack('<3I', *self.angles)), 0),
            0x4fbb90: (lambda c: self.ev('orig', 53, c.ecx, c.edx), 0),
            0x4eb790: (lambda c: self.ev('orig', 79, c.ecx, c.edx), 0),
            0x4fba20: (lambda c: self.ev('orig', 54, c.ecx, c.arg(0)), 4),
            0x56e9f0: (lambda c: self.ev('orig', 55, c.ecx), 0),
            0x56f280: (lambda c: self.ev('orig', 56, c.ecx, c.arg(0)), 4),
            0x506ca0: (lambda c: self.ev('orig', 57, c.ecx), 0),
        }
        for addr, (fn, pop) in stubs.items():
            cpu.stub_function(addr, fn, pop)
        cpu.stub_function(self.handler_fn, self.orig_handler, 8)
        cpu.stub_function(self.gate_fn, self.orig_gate, 0)
        cpu.stub_function(self.det_fn, lambda c: self.det_event('orig', c), 4)
        cpu.stub_function(self.tick_fn, lambda c: self.ev('orig', 51, self.ptok(c.ecx)), 0)
        cpu.stub_function(self.touch_fns[0], lambda c: self.ev('orig', 83, 1, c.ecx, c.arg(0)), 4)
        cpu.stub_function(self.touch_fns[1], lambda c: self.ev('orig', 83, 2, c.ecx, c.arg(0)), 4)
        cpu.passthrough = {0x5847c0, 0x586970}

    def check(self, ok):
        if not ok:
            raise SystemExit('a stub was called with unexpected registers')
        return 0

    def def_index(self, d):
        return s32(self.u32(d + 0x10)) if d else -1

    def ptok(self, p):
        if p == 0:
            return 0
        if self.pool_base <= p < self.pool_base + 0x5d8 * self.npool and (p - self.pool_base) % 0x5d8 == 0:
            return 0x10000 + (p - self.pool_base) // 0x5d8
        return 0x20000

    def ltok(self, p):
        if p == 0:
            return 0
        for i in range(8):
            if p == self.lock[2][i]:
                return 0x30001 + 2 * i
            if p == self.lock[3][i]:
                return 0x30002 + 2 * i
        return -2

    def btok(self, p):
        if p == 0:
            return 0
        for k_, v in self.beams.items():
            if v == p:
                return 0x50000 + k_
        return -2

    def beam_fields(self, slot):
        b, u, o, fl = self.beams[slot], self.u32, [], []

        def put(v, f=False):
            if f:
                fl.append(len(o))
            o.append(s32(v))
        put(self.btok(u(b)))
        put(self.btok(u(b + 4)))
        put(self.def_index(u(b + 8)))
        put(u(b + 0xc))
        put(int(u(b + 0x10) != 0))
        put(u(b + 0x14))
        put(u(b + 0x1c))
        for off in (0x28, 0x2c, 0x30, 0x34, 0x38):
            put(u(b + off), True)
        put(u(b + 0x3c))
        put(u(b + 0x40))
        for i in range(8):
            sg = b + 0x44 + 0x20 * i
            put(u(sg))
            for k_ in range(7):
                put(u(sg + 4 + 4 * k_), True)
        put(1 if u(b + 0x144) == self.lock[0] else -2 if u(b + 0x144) else 0)
        put(1 if u(b + 0x148) == self.lock[1] else -2 if u(b + 0x148) else 0)
        put(u(b + 0x14c))
        return o, fl

    def vec(self, p):
        return list(struct.unpack('<3I', self.cpu.read(p, 12)))

    def rec_args(self, p):
        return self.vec(p + 0xc) + [self.u32(p + 0x24)]

    def ev(self, side, kind, *args):
        self.events[side].append((kind,) + tuple(s32(a) for a in args))
        return 0

    def fresh_node(self, side, kind, *args):
        n = self.fresh[self.next_fresh[side]]
        self.next_fresh[side] += 1
        self.ev(side, kind, *args, n)
        return n

    def handle(self, side, kind, *args):
        self.handles[side] += 1
        h = 0x05000000 + self.handles[side]
        self.ev(side, kind, *args, h)
        return h

    def answer_rng(self, side, kind):
        k = (side, kind)
        self.calls[k] = self.calls.get(k, 0) + 1
        return random.Random(self.op_seed * 1000003 + kind * 7919 + self.calls[k])

    def make_list(self, rng, frm, to, kind):
        """A query's answer: records along from-to; (result, count byte, total byte, records)."""
        if rng.random() < (0.35 if kind == 38 else self.miss_rate):  # moving shots mostly fly on
            return 1, 0, 0, []
        k = rng.choice([1, 1, 2, 3, 4])
        a, b = [i2f(x) for x in frm], [i2f(x) for x in to]
        recs = []
        pool_nodes = [self.u32(self.pool_base + 0x5d8 * i + 0xc) for i in range(self.npool)]
        for _ in range(k):
            t = rng.random()
            pt = [f32(a[i] + (b[i] - a[i]) * t) for i in range(3)]
            nrm = [f32(rng.uniform(-1, 1)) for _ in range(3)]
            mat = rng.choice([m for m, _ in self.mats] + [0])
            node = rng.choice(self.world_nodes + ([rng.choice(pool_nodes)] if pool_nodes[0] else []))  # never 0: the damage hand-off reads it (0x5834ae)
            d2 = f32(sum((pt[i] - a[i]) ** 2 for i in range(3)))
            recs.append([f2i(x) for x in nrm] + [f2i(x) for x in pt] + [0, f2i(d2), mat, node])
        if kind == 38:
            return 0, k, None, recs
        near = min(range(k), key=lambda i: i2f(recs[i][7]))
        return 0, near, k, recs

    # --- emulator stubs ---------------------------------------------------------
    def write_list(self, out, cb, tb, recs):
        cpu = self.cpu
        cpu.write(out, bytes([cb & 0xff]))
        if tb is not None:
            cpu.write(out + 1, bytes([tb & 0xff]))
        for i, r in enumerate(recs):
            cpu.write(out + 4 + 0x28 * i, struct.pack('<10I', *[x & 0xffffffff for x in r]))

    def orig_intersect(self, c):
        self.ev('orig', 6, c.ecx, c.edx)
        if c.ecx in self.mem_nodes:
            fl = self.u32(c.ecx + 0x24)
            self.cpu.u32(c.ecx + 0x24, (fl | 0x10) if c.edx else (fl & ~0x10))
        return 0 if c.ecx else 5

    def orig_segment_list(self, c):
        self.check(c.ecx == WORLD)
        args = [c.arg(i) for i in range(6)]
        zone = self.u32(0x74a328)
        self.ev('orig', 38, *args, zone)
        r, cb, tb, recs = self.make_list(self.answer_rng('orig', 38), args[:3], args[3:], 38)
        self.write_list(c.edx, cb, tb, recs)
        return r

    def orig_segment(self, c):
        self.check(c.ecx == WORLD)
        frm, to = self.vec(c.edx), self.vec(c.arg(0))
        zone = self.u32(0x74a328)
        self.ev('orig', 39, *frm, *to, zone)
        r, cb, tb, recs = self.make_list(self.answer_rng('orig', 39), frm, to, 39)
        self.write_list(c.arg(1), cb, tb, recs)
        return r

    def ground_answer(self, rng, at):
        if rng.random() < 0.15:
            return [0, 0] + [0] * 10
        h = f32(i2f(at[1]) - rng.choice([rng.uniform(-5, 40), 3.0, 12.0, 27.0]))
        rec = [0, f2i(1.0), 0, at[0], f2i(h), at[2], 0, 0, 0, 0]
        return [1, 0] + rec

    def orig_ground(self, c):
        self.check(c.ecx == WORLD)
        at = self.vec(c.edx)
        self.ev('orig', 40, *at)
        a = self.ground_answer(self.answer_rng('orig', 40), at)
        self.write_list(c.arg(0), a[0], a[1], [a[2:12]])
        return 0

    def splash_answer(self, rng, at, radius):
        if rng.random() < 0.3:
            return 1, []
        k = rng.randrange(0, 4)
        recs = []
        for _ in range(k):
            pt = [f32(i2f(at[i]) + rng.uniform(-3, 3)) for i in range(3)]
            d2 = f32(rng.uniform(0, max(i2f(radius), 0.0) ** 2))
            recs.append([0, 0, 0] + [f2i(x) for x in pt] + [0, f2i(d2), rng.choice([m for m, _ in self.mats] + [0]),
                                                            rng.choice(self.world_nodes)])
        return rng.choice([0, 0, 1]), recs

    def orig_splash_query(self, c):
        pos, zero, radius, one, one2, out = c.edx, c.arg(0), c.arg(1), c.arg(2), c.arg(3), c.arg(4)
        self.check(zero == 0 and one == 1 and one2 == 1)
        at = self.vec(pos)
        self.ev('orig', 43, c.ecx, *at, radius)
        r, recs = self.splash_answer(self.answer_rng('orig', 43), at, radius)
        self.write_list(out, len(recs), None, recs)
        return r

    def orig_handler(self, c):
        hd, d, rec, dmg = c.ecx, c.edx, c.arg(0), c.arg(1)
        self.ev('orig', 44, hd, self.def_index(d), *self.rec_args(rec), dmg)
        res, kill, health, _ = self.rows[hd]
        if kill:
            if self.u32(rec + 0x24):
                self.cpu.u32(0x9a1790, rec)
            self.cpu.u32(0x9a1788, kill)
        self.cpu.u32(0x9a178c, health)
        return res

    def orig_gate(self, c):
        d, idp = c.ecx, c.edx
        idv = s32(self.u32(idp))
        self.ev('orig', 41, self.def_index(d), idv)
        r, nid = self.gate_answer(self.answer_rng('orig', 41), idv)
        self.cpu.u32(idp, nid)
        return r

    def gate_answer(self, rng, idv):
        return int(rng.random() < 0.8), idv ^ 0x55

    def det_event(self, side, c):
        at = c.edx
        args = [self.def_index(c.ecx), 1 if at else 0] + (self.vec(at) if at else []) + [c.arg(0)]
        return self.ev(side, 42, *args)

    def crater_answer(self, side):
        return self.answer_rng(side, 46).choice([0, 1])

    # --- the port's calls ------------------------------------------------------------
    def port_call(self, kind, args, n, out):
        a = [args[i] for i in range(n)]
        sc = self.scene['port']
        if 60 <= kind <= 73:
            if kind == 60:
                return len(sc.get(a[0]).parents)
            if kind == 61:
                return s32(sc.get(a[0]).parents[0])
            if kind == 62:
                return len(sc.get(a[0]).children)
            if kind == 63:
                return s32(sc.get(a[0]).children[0])
            if kind == 64:
                return s32(sc.get(a[0] & 0xffffffff).user)
            if kind == 65:
                sc.get(a[0] & 0xffffffff).user = a[1] & 0xffffffff
                return 0
            if kind == 66:
                nd = sc.get(a[0] & 0xffffffff)
                if a[1]:
                    nd.flags |= 0x8000000
                    nd.handler, nd.user = 1, ('p', a[1])
                else:
                    nd.flags &= ~0x8000000
                return 0
            if kind == 67:
                sc.get(a[0] & 0xffffffff).handler = 0
                return 0
            if kind == 68:
                return s32(sc.get(a[0] & 0xffffffff).frame)
            if kind == 69:
                return s32(sc.get(a[0] & 0xffffffff).handler)
            if kind == 70:
                u = sc.get(a[0] & 0xffffffff).user
                return u[1] if isinstance(u, tuple) else 0
            if kind == 71:
                return int(bool(sc.get(a[0] & 0xffffffff).flags & 0x10))
            if kind == 72:
                return int(self.rows[a[0] & 0xffffffff][3])
            if kind == 73:
                return self.soils.get(a[0] & 0xffffffff, 0)
            raise SystemExit(f'port query {kind}')
        u = [x & 0xffffffff for x in a]
        if kind in (1, 3):
            self.ev('port', kind, *a)
            sc.add(u[0], u[1])
            return 0
        if kind in (2, 4):
            self.ev('port', kind, *a)
            sc.remove(u[0], u[1])
            return 0
        if kind in (5, 13, 20):
            return s32(self.fresh_node('port', kind, *a))
        if kind == 6:
            self.ev('port', 6, *a)
            if u[0] in self.mem_nodes:
                nd = sc.get(u[0])
                nd.flags = (nd.flags | 0x10) if a[1] else (nd.flags & ~0x10)
            return 0
        if kind in (26, 27, 28, 31, 33):
            return s32(self.handle('port', kind, *a))
        if kind == 19:
            self.ev('port', 19)
            return self.pool_live
        if kind in (38, 39):
            self.ev('port', kind, *a)
            r, cb, tb, recs = self.make_list(self.answer_rng('port', kind), u[0:3], u[3:6], kind)
            self.fill(out, cb, tb, recs)
            return r
        if kind == 40:
            self.ev('port', 40, *a)
            g = self.ground_answer(self.answer_rng('port', 40), u)
            self.fill(out, g[0], g[1], [g[2:12]])
            return 0
        if kind == 43:
            self.ev('port', 43, *a)
            r, recs = self.splash_answer(self.answer_rng('port', 43), u[1:4], u[4])
            self.fill(out, len(recs), 0, recs)
            return r
        if kind == 44:
            self.ev('port', 44, *a)
            res, kill, health, _ = self.rows[u[0]]
            out[0], out[1] = s32(kill), s32(health)
            return res
        if kind == 41:
            self.ev('port', 41, *a)
            r, nid = self.gate_answer(self.answer_rng('port', 41), a[1])
            out[0] = s32(nid)
            return r
        if kind == 74:
            return s32(sc.get(u[0]).flags)
        if kind == 80:
            return s32(sc.get(u[0]).parents[a[1]])
        if kind == 81:
            return s32(sc.get(u[0]).children[a[1]])
        if kind == 82:
            sc.get(u[0]).handler = u[1]
            return 0
        if kind in (75, 77):
            self.ev('port', kind)
            for i in range(11):
                out[i] = s32(self.crater_tpl[i])
            return 0
        if kind == 76:
            self.ev('port', 76, *a)
            return self.crater_answer('port')
        if kind == 49:
            self.ev('port', 49, *a)
            for i in range(3):
                out[i] = s32(self.angles[i])
            return 0
        self.ev('port', kind, *a)
        return 0

    def fill(self, out, cb, tb, recs):
        out[0] = cb
        out[1] = -1 if tb is None else tb
        for i, r in enumerate(recs):
            for j in range(10):
                out[2 + 10 * i + j] = s32(r[j])
        out[312] = len(recs)

    # --- state ----------------------------------------------------------------------
    def slot_fields(self, i):
        """A pool slot's fields in the order probe_fly_slot writes them; float positions."""
        b = self.pool_base + 0x5d8 * i
        u, o, fl = self.u32, [], []

        def put(v, f=False):
            if f:
                fl.append(len(o))
            o.append(s32(v))

        def putv(at):
            for k in range(3):
                put(u(at + 4 * k), True)
        put(self.ptok(u(b)))
        for off in (4, 8, 0xc, 0x10, 0x14, 0x18, 0x1c, 0x20):
            put(u(b + off))
        for off in (0x24, 0x30, 0x3c, 0x48, 0x54, 0x60):
            putv(b + off)
        put(u(b + 0x6c))
        put(self.ltok(u(b + 0x70)))
        put(self.ltok(u(b + 0x74)))
        put(u(b + 0x78))
        put(u(b + 0x7c))
        cnt = self.cpu.read(b + 0x80, 2)
        put(cnt[0])
        put(cnt[1])
        for k in range(min(cnt[0], 31)):
            r = b + 0x84 + 0x28 * k
            putv(r + 0xc)
            put(u(r + 0x1c), True)
            put(u(r + 0x24))
        put(u(b + 0x584))
        for off in (0x588, 0x58c, 0x590):
            put(u(b + off), True)
        putv(b + 0x598)
        putv(b + 0x5a4)
        for off in (0x5b0, 0x5b4, 0x5b8, 0x5bc, 0x5c0, 0x5c4):
            put(u(b + off), True)
        put(u(b + 0x5c8))
        put(u(b + 0x5cc))
        t = u(b + 0x5d0)
        put({0: 0, self.tick_fn: 1, 0x4b4a30: 2, 0x4b4a20: 3}.get(t, -2))
        put(self.ptok(u(b + 0x5d4)) if u(b + 0x5d4) else 0)
        return o, fl

    def world_fields(self):
        u, o, fl = self.u32, [], []

        def put(v, f=False):
            if f:
                fl.append(len(o))
            o.append(s32(v))
        p = u(0x9a0eb8)
        while p:
            put(self.ptok(p))
            p = u(p)
        put(-1)
        for k in range(self.ndefs):
            d = self.defs_base + 0x1f4 * k
            p = u(d + 0x7c) if not (u(d + 0x6c) & 2) else 0
            while p:
                put(self.ptok(p))
                p = u(p)
            put(-1)
            put(u(d + 0x140))
            put(u(d + 0xd0))
            put(u(d + 0xd4))
            put(u(d + 0x130))
            put(u(d + 0xd8))
            put(u(d + 0x1e8), True)
        put(int(u(0x9a0ee0) != 0))
        for a in (0x9a17d0, 0x9a17d8, 0x9a17dc):
            put(u(a), True)
        put(u(0x9a17e0))
        for i in range(6):
            put(u(0x9a0ec4 + 4 * i), True)
        for a in (0x9a0edc, 0x9a0eec, 0x9a0ee8, 0x9a1788):
            put(u(a))
        put(u(0x9a178c), True)
        put(u(0x9a17c0))
        for a in (0x74a328, 0x9a17a8, 0x9a17ac, 0x9a17b0, 0x9a0ebc):
            put(u(a))
        return o, fl

    def table_token(self, side, v, seen):
        if v in (0, 1) or v in self.rows:
            return v
        if v not in seen:
            seen[v] = 0x60000 + len(seen)
            if side == 'orig':
                cont = [s32(self.u32(v + 4 * i)) for i in range(4)]
                cont[1] = self.fn_tok(cont[1])
                cont[3] = self.fn_tok(cont[3])
            else:
                buf = (ctypes.c_int32 * 4)()
                probe.probe_fly_table(s32(v), buf)
                cont = list(buf)
            seen[('c', v)] = cont
        return (seen[v], tuple(seen[('c', v)]))

    def fn_tok(self, f):
        f &= 0xffffffff
        return 0 if f == 0 else 1 if f == self.touch_fns[0] else 2 if f == self.touch_fns[1] else \
            3 if f == self.touch_fns[2] else -2

    def scene_fields(self, side):
        sc, o = self.scene[side], []
        seen = {}
        for n in sorted(self.mem_nodes):
            nd = sc.get(n)
            if side == 'orig':
                flags, user, handler = self.u32(n + 0x24), self.u32(n + 0x40), self.u32(n + 0xbc)
                if 0x10000 <= self.ptok(user) < 0x20000:
                    user = ('p', self.ptok(user))
            else:
                flags, user, handler = nd.flags, nd.user, nd.handler
            o.append((n, tuple(nd.parents), tuple(nd.children), flags & 0x8000010, self.table_token(side, handler, seen),
                      user))
        return o

    def radar(self):
        head = self.u32(0x60b22c)
        out, nd = [], self.u32(head)
        while nd != head:
            e = self.u32(nd + 8)
            out.append((self.ptok(self.u32(e)), self.cpu.read(e + 4, 1)[0]))
            nd = self.u32(nd)
        return out


def close(a, b, tol, floor=1.0):
    if a == b:
        return True
    x, y = i2f(a), i2f(b)
    if math.isnan(x) and math.isnan(y):
        return True
    return abs(x - y) <= tol * max(abs(x), abs(y), floor)


def compare_lists(what, a, b, floats, tol, stats):
    if len(a) != len(b):
        return f'{what}: {len(a)} vs {len(b)} values'
    for i, (x, y) in enumerate(zip(a, b)):
        if x == y:
            continue
        if i in floats and close(x, y, tol):
            stats['near'] += 1
            continue
        return f'{what}: item {i}: {x:#x} ({i2f(x)!r}) vs {y:#x} ({i2f(y)!r})'
    return None


def compare_events(a, b, tol, stats):
    for i, (x, y) in enumerate(zip(a, b)):
        if x == y:
            continue
        fl = FLOATS.get(x[0], ())
        if x[0] == y[0] and len(x) == len(y) and all(
                p == q or (j - 1 in fl and close(p, q, tol)) for j, (p, q) in enumerate(zip(x, y))):
            stats['near'] += 1
            continue
        return i
    return None if len(a) == len(b) else min(len(a), len(b))


def show(ev):
    return '\n    '.join(f'{NAMES.get(e[0], e[0])}{tuple(hex(v & 0xffffffff) for v in e[1:])}' for e in ev)


def poke(emu, name, which, v):
    k = emu.names[name]
    dp = emu.defs_base + 0x1f4 * k
    off = {0: 0x6c, 1: 0x70, 2: 0x74, 3: 0x78, 4: 0x4c, 5: 0x100, 6: 0xf0, 7: 0xe4, 8: 0xec, 9: 0x168, 10: 0xa0,
           11: 0xd4, 12: 0xd8, 13: 0x80, 14: 0x1c, 16: 0x38, 18: 0x54, 19: 0x84, 20: 0x2c, 21: 0x1a0, 22: 0x178, 23: 0x44}
    if which == 17:
        imp = emu.u32(dp + 0x13c)
        for k_ in range(6):
            emu.cpu.u32(imp + 0x5c * k_ + 0x38, 2)
            emu.cpu.u32(imp + 0x5c * k_ + 0x3c, v)
            emu.cpu.u32(imp + 0x5c * k_ + 0x40, v + 1)
    elif which == 15:
        emu.cpu.u32(dp + 0x3c, v)
        emu.cpu.u32(dp + 0x40, f2i(i2f(v) * i2f(v)))
    else:
        emu.cpu.u32(dp + off[which], v)
    probe.probe_fly_poke(k, which, s32(v))


def pokes(emu, rng):
    fl = lambda n: emu.u32(emu.defs_base + 0x1f4 * emu.names[n] + 0x6c)  # noqa: E731
    MINE, BOUNCES, TETHER, LEAD, INSTANT, HIT_OWNER, FIXED, REMOTE = (0x4000, 1, 0x200000, 0x10000, 0x800,
                                                                      0x4000000, 0x100, 0x100000)
    poke(emu, 'LANDMINE', 0, fl('LANDMINE') | MINE | BOUNCES)
    poke(emu, 'GASTANK', 0, fl('GASTANK') | MINE | BOUNCES | REMOTE)
    poke(emu, 'GASTANK', 15, f2i(25.0))
    poke(emu, 'StreakSRM2', 0, fl('StreakSRM2') | TETHER)
    poke(emu, 'SRM4', 0, fl('SRM4') | LEAD)
    poke(emu, 'SRM4', 1, f2i(0.05))
    poke(emu, 'SRM4', 2, f2i(0.6))
    poke(emu, 'SRM4', 3, f2i(1 / 0.55))
    poke(emu, 'SRM6', 0, fl('SRM6') | LEAD)
    poke(emu, 'SRM6', 1, f2i(0.0))
    poke(emu, 'SRM6', 2, f2i(0.0))
    poke(emu, 'AC2', 0, fl('AC2') | INSTANT)
    poke(emu, 'AC5', 4, f2i(9.8))
    poke(emu, 'AC10', 4, f2i(9.8))
    poke(emu, 'AC10', 0, fl('AC10') | FIXED)
    for n in ('LRM5', 'LRM15', 'LRM20', 'SRM6', 'StreakSRM4', 'MGUN', 'AC2', 'TAG_MISSILE'):
        poke(emu, n, 0, fl(n) | HIT_OWNER)
    poke(emu, 'AC20', 4, f2i(9.8))
    poke(emu, 'MGUN', 6, 0x0b000001)   # an animated FLYOUT model: pooled copies
    poke(emu, 'MGUN', 7, 0x0b000002)
    poke(emu, 'MGUN', 8, 0x0b000003)
    poke(emu, 'GAUSS_RIFLE' if 'GAUSS_RIFLE' in emu.names else 'AC20', 9, 0x0b000004)  # destroy animation
    poke(emu, 'AC20', 10, 0x0b000005)  # a fire EFFECT
    poke(emu, 'LRM10', 12, 0x0b000006)  # a fire attach node
    poke(emu, 'SRM2', 0, fl('SRM2') | 0x8)  # FLYOUT_HEALTH
    poke(emu, 'SRM2', 13, f2i(rng.choice([1.0, 0.0])))
    poke(emu, 'TAG_MISSILE', 14, f2i(40.0))
    poke(emu, 'NUKE', 0, (fl('NUKE') & ~0x10) | 0x40000)  # QUICKSAND
    for n in ('NUKE', 'AC20'):  # a points span for the crater module
        poke(emu, n, 21, 4)
        poke(emu, n, 22, 2)
    poke(emu, 'AC20', 0, fl('AC20') | 0x10)  # CRATER
    poke(emu, 'LANDMINE', 4, f2i(9.8))      # a thrown mine
    poke(emu, 'LANDMINE', 17, 0x0c000010)   # bounce sounds
    poke(emu, 'GASTANK', 17, 0x0c000020)
    poke(emu, 'SRM6', 18, 0)                # no TURN_SUSPEND_TIME: full turn rate
    poke(emu, 'AC5', 0, fl('AC5') | 0x8)    # shootable, health 0, no destroy animation: shot down at once
    poke(emu, 'AC5', 13, 0)
    poke(emu, 'AC5', 9, 0)
    poke(emu, 'Laser_Small', 0, fl('Laser_Small') | 0x20000 | 0x8000)  # MULTI_TARGET with LOCK_ON
    poke(emu, 'ERLaser_Small', 0, fl('ERLaser_Small') | 0x20000)       # MULTI_TARGET, no lock: a straight ray
    poke(emu, 'Laser_Medium', 0, fl('Laser_Medium') | BOUNCES)
    poke(emu, 'PulseLaser_Small', 10, 0x0b000007)       # a fire EFFECT on a beam
    poke(emu, 'PulseLaser_Medium', 19, 1)               # RANDOM_ROTATE on a beam's fire animation
    poke(emu, 'Laser_Large', 20, f2i(4.0))              # a slow beam: it grows, then snaps to its reach
    poke(emu, 'TAG', 23, f2i(0.5))                      # TAG calls its strike after 0.5 s on a target
    poke(emu, 'ERLaser_Medium', 0, fl('ERLaser_Medium') | BOUNCES | 0x8000)


def main():
    emu = Fly()
    rng = random.Random(1205)
    sessions = int(os.environ.get('WEAPON_FLY_SESSIONS', '200'))
    ops_per = int(os.environ.get('WEAPON_FLY_OPS', '40'))
    stats = {'near': 0, 'ops': 0, 'calls': 0, 'spawned': 0}
    cpu = emu.cpu
    # At least `sessions`; more (up to twice as many) until every path is reached.
    sess = -1
    while True:
        sess += 1
        if sess >= sessions and (sess >= 2 * sessions or not set(COVER) - emu.reached):
            break
        emu.new_session(rng)
        pokes(emu, rng)
        init_arg = int(rng.random() < 0.3)
        cpu.u32(0x9a0ec0, init_arg)
        cpu.u32(0x9a178c, f2i(1.0))  # the port's HitState starts at 1
        cpu.write(0x9a0ec4, bytes(0x1c))  # the shot line and attacker, kept over sessions by the original
        for a in (0x9a17a8, 0x9a17ac, 0x9a0ebc):  # likewise the tick's zone, the hidden flag, the lights
            cpu.u32(a, 0)
        if rng.random() < 0.15:  # a nearly empty pool
            n_free = rng.randrange(1, 6)
            p, i = emu.u32(0x9a0eb8), 1
            while p and i < n_free:
                p, i = emu.u32(p), i + 1
            if p:
                cpu.u32(p, 0)
            probe.probe_fly_trim(n_free)
        gate, det = rng.random() < 0.5, rng.random() < 0.5
        cpu.u32(0x72fcac, emu.gate_fn if gate else 0)
        cpu.u32(0x72fcb4, emu.det_fn if det else 0)
        probe.probe_fly_net(int(gate), int(det))
        names = list(emu.names)
        proj_defs = [n for n in names if not (emu.u32(emu.defs_base + 0x1f4 * emu.names[n] + 0x6c) & 2)]
        frame, netclock, clock = 100, 5.0, 10.0
        for k in range(ops_per + 1):
            stats['ops'] += 1
            emu.events = {'orig': [], 'port': []}
            emu.calls = {}
            emu.op_seed = rng.randrange(1 << 30)
            emu.miss_rate = rng.choice([0.7, 0.7, 0.1])  # some frames hit nearly everything (TAG's second on a target)
            emu.angles = [f2i(f32(rng.uniform(-3, 3))) for _ in range(3)]
            frame += 1
            netclock += rng.choice([0.0, 30.0, 0.5])
            clock += rng.choice([0.0, 0.1, 0.3, 0.6])
            dt = f32(rng.choice([1 / 30, 1 / 60, 0.1, 0.05, 0.25]))
            g = [f2i(dt), frame, f2i(f32(netclock)), rng.choice([0, 3, 0x104, 7]), init_arg,
                 f2i(f32(rng.choice([1.0, 1.0, 0.5]))), f2i(f32(rng.choice([1.0, 2.0]))),
                 f2i(f32(rng.choice([1.0, 2.0]))), rng.choice([0, 0, 1]), int(rng.random() < 0.6),
                 rng.choice([1, 1, 0, -1]), rng.choice(emu.world_nodes),
                 *([f2i(f32(rng.uniform(-300, 300))) for _ in range(3)] if rng.random() < 0.7 else
                   [f2i(f32(x)) for x in emu.ahead]),  # ahead of the last shot
                 *[f2i(f32(rng.uniform(-30, 30))) for _ in range(3)],
                 int(rng.random() < 0.9), int(rng.random() < 0.85), rng.choice([0] + emu.world_nodes[:2]),
                 rng.randrange(5), f2i(f32(rng.choice([30.0, 5.0]))), f2i(f32(clock))]
            for _ in range(7):  # more lock targets, for MULTI_TARGET beams
                g += [rng.choice(emu.world_nodes), *[f2i(f32(rng.uniform(-300, 300))) for _ in range(3)],
                      *[f2i(f32(rng.uniform(-30, 30))) for _ in range(3)]]
            g += [f2i(f32(rng.choice([1.0, 1.0, 0.5]))), rng.choice([1, 1, 0])]
            if g[9] and rng.random() < 0.5:
                g[10] = rng.choice([0, 1, 2, 3, 4, 5, 6, 8])  # the lock array holds 8: more and both read past it
            # the beams' muzzles move
            for slot in emu.beams:
                at, dp, _ = emu.beam_at[slot]
                v = [f2i(f32(rng.uniform(-100, 100))) for _ in range(3)]
                d_ = [rng.uniform(-1, 1) for _ in range(3)]
                n2 = math.sqrt(sum(x * x for x in d_)) or 1
                v += [f2i(f32(x / n2)) for x in d_]
                cpu.write(at, struct.pack('<3I', *v[:3]))
                cpu.write(dp, struct.pack('<3I', *v[3:]))
                probe.probe_fly_beam_at(slot, (ctypes.c_int32 * 6)(*[s32(x) for x in v]))
            probe.probe_fly_globals((ctypes.c_int32 * len(g))(*[s32(x) for x in g]))
            for addr, v in ((0x72c52c, g[0]), (0x72f338, g[1]), (0x72c538, g[2]), (0x74a328, g[3]),
                            (0x9a0ec0, g[4]), (0x9a17d0, g[5]), (0x9a17d8, g[6]), (0x9a17dc, g[7]), (0x9a17e0, g[8]),
                            (0x9a0eec, g[18]), (0x9a0ee8, g[19]), (0x9a17c4, g[20]), (0x9a17c0, g[21]),
                            (0x9a17a0, g[22]), (0x72c530, g[23]), (0x9a1788, 0), (0x9a1790, 0),
                            (0x9a17d4, g[73]), (0x9a17cc, g[74])):
                cpu.u32(addr, v)
            flag, st, ats, vels = emu.lock
            cpu.u32(flag, g[10])
            for i in range(8):
                e = g[11:18] if i == 0 else g[24 + 7 * (i - 1):31 + 7 * (i - 1)]
                cpu.write(st + 12 * i, struct.pack('<3I', e[0], ats[i], vels[i]))
                cpu.write(ats[i], struct.pack('<3I', *e[1:4]))
                cpu.write(vels[i], struct.pack('<3I', *e[4:7]))
            if g[9]:
                cpu.u32(0x9a0ee0, flag)
                cpu.u32(0x9a0ee4, st)
            else:
                cpu.u32(0x9a0ee0, 0)
                cpu.u32(0x9a0ee4, 0)
            if k == 0:
                op, args = 5, [1]
            else:
                op = rng.choice([0] * 9 + [1] * 12 + [2, 2, 3, 6, 7, 8, 9, 10, 11, 12, 13, 15, 16] + [17, 17, 18, 18, 18, 19, 20] +
                                [4, 5] * (k % 17 == 0) + [21, 21, 22, 22, 23, 24, 25, 26, 27, 27, 28, 28, 8, 8, 8, 29, 29, 30, 30, 30, 30])
                args = []
            seed = rng.randrange(1 << 31)
            emu.rand = MsvcRand(seed)
            out = (ctypes.c_int32 * 8)()
            res_a = 0
            if op == 0:
                name = rng.choice(proj_defs if rng.random() < 0.6 else
                                  ['LRM15', 'SRM4', 'SRM6', 'TAG_MISSILE', 'StreakSRM2', 'LANDMINE', 'GASTANK'])
                d = emu.names[name]
                dp = emu.defs_base + 0x1f4 * d
                if rng.random() < 0.3:  # a fire sound or animation choice, as the game sets them
                    cpu.u32(dp + 0xd0, rng.choice([0, -1 & 0xffffffff]))
                    cpu.u32(dp + 0xd4, rng.choice([0, 1, -1 & 0xffffffff]))
                    cpu.u32(dp + 0x130, rng.choice([0, -1 & 0xffffffff]))
                    probe.probe_fly_poke(d, 11, s32(emu.u32(dp + 0xd4)))
                    # (the sound and flyout choices through the same pokes)
                    port_set = (s32(emu.u32(dp + 0xd0)), s32(emu.u32(dp + 0x130)))
                else:
                    port_set = None
                owner = rng.choice(emu.world_nodes)
                zone_on = int(rng.random() < 0.7)
                zone = rng.choice([3, 0x104, 0x11223344])
                v = [rng.uniform(-1, 1) for _ in range(3)]
                n_ = math.sqrt(sum(x * x for x in v)) or 1
                dirv = [f2i(f32(x / n_)) for x in v]
                mz = [f2i(f32(rng.uniform(-200, 200))) for _ in range(3)]
                vel = [f2i(f32(rng.choice([0.0, rng.uniform(-20, 20)]))) for _ in range(3)]
                net = rng.randrange(100)
                args = [d, owner, zone_on, zone, *mz, *dirv, *vel, net, 0]
                zp, mp, dpp, vp = emu.zalloc(4), emu.zalloc(12), emu.zalloc(12), emu.zalloc(12)
                cpu.u32(zp, zone)
                cpu.write(mp, struct.pack('<3I', *mz))
                cpu.write(dpp, struct.pack('<3I', *dirv))
                cpu.write(vp, struct.pack('<3I', *vel))
                if port_set:
                    set_choices(d, *port_set)
                emu.ahead = [i2f(mz[i]) + 400 * i2f(dirv[i]) + rng.uniform(-30, 30) for i in range(3)]
                cpu.call(0x585010, ecx=dp, edx=owner, stack_args=(zp if zone_on else 0, mp, dpp, vp, net, 0))
                res_a = emu.ptok(cpu.uc.reg_read(UC_X86_REG_EAX))
                stats['spawned'] += res_a != 0
            elif op in (1, 30):
                if op == 30:  # a frame where nearly every cast hits (TAG's time on a target, mines settling)
                    emu.miss_rate = 0.0
                    op = 1
                cpu.call(0x5823c0)
            elif op == 2:
                d = emu.names[rng.choice(['GASTANK', 'LANDMINE', rng.choice(proj_defs)])]
                at_on = int(rng.random() < 0.4)
                at = [f2i(f32(rng.uniform(-200, 200))) for _ in range(3)]
                owner = rng.choice([0, 0] + emu.world_nodes)
                args = [d, at_on, *at, owner]
                ap = emu.zalloc(12)
                cpu.write(ap, struct.pack('<3I', *at))
                cpu.call(0x5820c0, ecx=emu.defs_base + 0x1f4 * d, edx=ap if at_on else 0, stack_args=(owner,))
                res_a = s32(cpu.uc.reg_read(UC_X86_REG_EAX))
            elif op == 3:
                d = emu.names[rng.choice(proj_defs)]
                at = [f2i(f32(rng.uniform(-200, 200))) for _ in range(3)]
                args = [d, *at]
                ap = emu.zalloc(12)
                cpu.write(ap, struct.pack('<3I', *at))
                cpu.call(0x5822c0, ecx=emu.defs_base + 0x1f4 * d, edx=ap)
                res_a = s32(cpu.uc.reg_read(UC_X86_REG_EAX))
            elif op == 4:
                if 1 in emu.beam_state.values():
                    continue  # a firing beam would be killed as a projectile (FIX (A), zwep_shot.cpp)
                cpu.call(0x584fe0)
            elif op == 5:
                if not args:
                    args = [rng.choice([0, 1])]
                if args[0] == 0 and 1 in emu.beam_state.values():
                    continue
                cpu.call(0x5847c0, ecx=args[0])  # passed through: the original runs
                res_a = s32(cpu.uc.reg_read(UC_X86_REG_EAX))
            elif op == 6:
                d = emu.names[rng.choice(['AC5', 'AC10', 'LANDMINE', 'GASTANK', rng.choice(names)])]
                fr = [f2i(f32(rng.uniform(-100, 100))) for _ in range(3)]
                to = [f2i(f32(rng.uniform(-3000, 3000))) for _ in range(3)]
                args = [d, *fr, *to]
                fp, tp, op_ = emu.zalloc(12), emu.zalloc(12), emu.zalloc(4)
                cpu.write(fp, struct.pack('<3I', *fr))
                cpu.write(tp, struct.pack('<3I', *to))
                cpu.call(0x581ff0, ecx=emu.defs_base + 0x1f4 * d, edx=fp, stack_args=(0x77, tp, op_))
                cpu.call(emu.fstp)
                res_a = s32(emu.u32(emu.fslot))
                out_a = [s32(emu.u32(op_))]
            elif op == 7:
                sp = emu.zalloc(16)
                cpu.call(0x581e50, ecx=sp)
                out_a = [s32(emu.u32(sp + 4 * i)) for i in range(4)]
            elif op == 8:
                flying = [emu.ptok(emu.u32(emu.defs_base + 0x1f4 * dk + 0x7c)) for dk in range(emu.ndefs)]
                flying = [t - 0x10000 for t in flying if 0x10000 <= t < 0x20000]
                slot = rng.choice(flying) if flying and rng.random() < 0.8 else rng.randrange(emu.npool)
                args = [0x10000 + slot, rng.choice([0, 1, 1, 2, 2])]
                b = emu.pool_base + 0x5d8 * slot
                cpu.u32(b + 0x5d0, [emu.tick_fn, 0x4b4a30, 0x4b4a20][args[1]])
                cpu.u32(b + 0x5d4, b)
            elif op == 9:
                at = [f2i(f32(rng.uniform(-200, 200))) for _ in range(3)]
                args = at
                ap = emu.zalloc(12)
                cpu.write(ap, struct.pack('<3I', *at))
                cpu.call(0x586b20, ecx=ap)
                res_a = s32(cpu.uc.reg_read(UC_X86_REG_EAX))
                if res_a:
                    emu.taken_lights.append(res_a)
            elif op == 10:
                if not emu.taken_lights:
                    continue
                lt = emu.taken_lights.pop(rng.randrange(len(emu.taken_lights)))
                args = [lt]
                cpu.call(0x586b70, ecx=lt & 0xffffffff)
            elif op == 11:
                flying = []
                for dk in range(emu.ndefs):
                    p = emu.u32(emu.defs_base + 0x1f4 * dk + 0x7c)
                    if p and not (emu.u32(emu.defs_base + 0x1f4 * dk + 0x6c) & 2):
                        flying.append((dk, p))
                if not flying:
                    continue
                dk, p = rng.choice(flying)
                # off the active list first, as the callers do (0x5820c0)
                dp = emu.defs_base + 0x1f4 * dk
                cpu.u32(dp + 0x7c, emu.u32(p))
                args = [dk, emu.ptok(p)]
                cpu.call(0x584f40, ecx=dp, edx=p)
            elif op in (12, 13):
                d = emu.names[rng.choice(names)]
                soil = rng.randrange(6)
                pt = [f2i(f32(rng.uniform(-100, 100))) for _ in range(3)]
                vol = f2i(f32(rng.choice([1.0, 0.7])))
                args = [d, soil, *pt, vol]
                rp = emu.zalloc(0x28)
                cpu.write(rp + 0xc, struct.pack('<3I', *pt))
                cpu.call(0x582f60 if op == 12 else 0x582fc0, ecx=emu.defs_base + 0x1f4 * d, edx=rp,
                         stack_args=(soil, vol))
            elif op == 29:
                cand = [i for i, st in emu.beam_state.items() if st == 1]
                if not cand:
                    continue
                slot = rng.choice(cand)
                b = emu.beams[slot]
                dl = f2i(f32(rng.choice([0.05, 0.5, -0.05])))
                tm = f2i(f32(rng.choice([0.0, 0.3, 0.45])))
                args = [slot, dl, tm]
                reach = f32(i2f(emu.u32(b + 0x38)) * i2f(emu.u32(emu.u32(b + 8) + 0x1c)))
                cpu.u32(b + 0x28, f2i(f32(reach - i2f(dl))))
                cpu.u32(b + 0x2c, tm)
            elif op in (17, 18, 19, 20):
                beam_defs = [n for n in names if emu.u32(emu.defs_base + 0x1f4 * emu.names[n] + 0x6c) & 2]
                if op == 17:
                    free = [i for i in range(16) if i not in emu.beams]
                    if not free:
                        continue
                    slot = rng.choice(free)
                    d = emu.names[rng.choice(beam_defs + ['Laser_Small', 'Laser_Small', 'TAG', 'TAG', 'Laser_Large'])]
                    zone_on = int(rng.random() < 0.5)
                    zone = rng.choice([3, 0x104])
                    segs = rng.choice([1, 1, 2, 3, 8, 9])
                    args = [d, rng.choice(emu.world_nodes), zone_on, zone, slot, segs]
                    at, dp, zp = emu.beam_at[slot]
                    cpu.u32(zp, zone)
                    cpu.call(0x584a70, ecx=emu.defs_base + 0x1f4 * d, edx=args[1],
                             stack_args=(zp if zone_on else 0, 0x77, at, dp, segs))
                    emu.beams[slot] = cpu.uc.reg_read(UC_X86_REG_EAX)
                    emu.beam_state[slot] = 0
                    res_a = 0x50000 + slot
                else:
                    want = {18: 0, 19: 1, 20: 0}[op]
                    cand = [i for i, st in emu.beam_state.items() if st == want]
                    if not cand:
                        continue
                    slot = rng.choice(cand)
                    args = [slot]
                    cpu.call({18: 0x581230, 19: 0x581400, 20: 0x584b40}[op], ecx=emu.beams[slot])
                    res_a = s32(cpu.uc.reg_read(UC_X86_REG_EAX))
                    if op == 20:
                        del emu.beams[slot]
                        del emu.beam_state[slot]
                    else:
                        emu.beam_state[slot] = 1 if op == 18 else 0
            elif op in (26, 27, 28):
                if op != 26 and not emu.ams:
                    continue
                if op == 26:
                    emu.ams = emu.zalloc(0x1c)
                    args = [f2i(f32(rng.choice([150.0, 400.0]))), f2i(f32(rng.choice([0.6, 0.05]))), rng.choice([100, 2, 0])]
                    cpu.call(0x4b3ab0, ecx=emu.ams, stack_args=tuple(args))
                elif op == 27:
                    near = [emu.pool_base + 0x5d8 * (b_[0] - 0x10000) for b_ in emu.radar()]
                    if near and rng.random() < 0.7:  # beside a shot on the radar
                        at = struct.unpack('<3f', cpu.read(rng.choice(near) + 0x48, 12))
                        args = [rng.choice([0, 1, 2])] + [f2i(f32(x + rng.uniform(-40, 40))) for x in at]
                    else:
                        args = [rng.choice([0, 1, 2])] + [f2i(f32(rng.uniform(-200, 200))) for _ in range(3)]
                    ap = emu.zalloc(12)
                    cpu.write(ap, struct.pack('<3I', *args[1:]))
                    cpu.call(0x4b3ae0, ecx=emu.ams, stack_args=(0x77, args[0], ap))
                    res_a = cpu.uc.reg_read(UC_X86_REG_EAX) & 0xff
                else:
                    if not emu.u32(emu.ams + 0x14):
                        continue  # fire wants a target (it writes its health)
                    args = [rng.choice([1, 1, 2, 3])]
                    cpu.call(0x4b3bb0, ecx=emu.ams, stack_args=(args[0],))
                cpu.call(0x4b3ba0, ecx=emu.ams)
                tp = cpu.uc.reg_read(UC_X86_REG_EAX)
                u = emu.u32
                out_a = [emu.cpu.read(emu.ams, 1)[0], s32(u(emu.ams + 4)), s32(u(emu.ams + 8)), s32(u(emu.ams + 0xc)),
                         emu.ptok(u(emu.ams + 0x14)), s32(u(emu.ams + 0x18)),
                         emu.ptok(tp - 0x48) if tp else 0]
            elif op in (21, 22, 23, 24, 25):
                node = rng.choice(emu.tree)
                obj = rng.randrange(1, 1 << 20)
                if op == 21:
                    fnt = rng.choice([1, 2, 3])
                    args = [obj, node, fnt, rng.choice([0, 1])]
                    cpu.call(0x5833b0, ecx=obj, edx=node, stack_args=(args[3], emu.touch_fns[fnt - 1]))  # flag, fn
                elif op == 22:
                    fnt = rng.choice([1, 2])
                    args = [obj, node, fnt]
                    cpu.call(0x5833f0, ecx=obj, edx=node, stack_args=(emu.touch_fns[fnt - 1],))
                elif op in (23, 24):
                    args = [node]
                    cpu.call(0x583400 if op == 23 else 0x583250, ecx=node)
                else:
                    t = emu.u32(node + 0xbc)
                    if t in (0, 1) or emu.fn_tok(emu.u32(t + 0xc)) not in (1, 2):
                        continue  # no touch handler: both would call a null function
                    fr = [f2i(f32(rng.uniform(-50, 50))) for _ in range(3)]
                    pt = [f2i(f32(rng.uniform(-50, 50))) for _ in range(3)]
                    dmg = f2i(f32(rng.choice([20.0, 0.5])))
                    args = fr + pt + [node, dmg]
                    fp, rp = emu.zalloc(12), emu.zalloc(0x28)
                    cpu.write(fp, struct.pack('<3I', *fr))
                    cpu.write(rp + 0xc, struct.pack('<3I', *pt))
                    cpu.u32(rp + 0x24, node)
                    emu.touch_rec = rp
                    cpu.call(0x5836b0, ecx=fp, edx=rp, stack_args=(dmg,))
                res_a = s32(cpu.uc.reg_read(UC_X86_REG_EAX)) if op != 25 else 0
                if op == 24:
                    res_a &= 0xff
            elif op in (15, 16):
                vs = lambda n_, lo, hi: [f2i(f32(rng.uniform(lo, hi))) for _ in range(n_)]  # noqa: E731
                if op == 15:
                    a_ = vs(3, -100, 100)
                    t_ = rng.choice([vs(3, -100, 100), a_])
                    vel = vs(3, -50, 50)
                    sp = math.sqrt(sum(i2f(x) ** 2 for x in vel))
                    speed = f2i(f32(rng.choice([rng.uniform(0, 80), sp, rng.uniform(0, 10)])))
                    if rng.random() < 0.2:
                        speed = f2i(f32(sp))
                        vel = [f2i(f32(i2f(x))) for x in vel]
                    args = a_ + t_ + [speed] + vel
                else:
                    def unit():
                        v_ = [rng.uniform(-1, 1) for _ in range(3)]
                        n2 = math.sqrt(sum(x * x for x in v_)) or 1
                        return [f32(x / n2) for x in v_]
                    a_ = unit()
                    if rng.random() < 0.1:
                        a_ = [0.0, f32(rng.choice([1.0, -1.0])), 0.0]
                    b_ = rng.choice([unit(), [f32(-x) for x in a_], [f32(x + 0.05) for x in a_]])
                    t_ = rng.choice([0.0, 1.0, 1.5, -0.2, rng.random()])
                    args = [f2i(x) for x in a_ + b_] + [f2i(f32(t_))]
                bp = emu.zalloc(0x40)
                cpu.write(bp, struct.pack(f'<{len(args)}I', *args))
                o_ = emu.zalloc(12)
                if op == 15:
                    cpu.call(0x524247, ecx=bp, edx=bp + 12, stack_args=(args[6], bp + 28, o_))
                    res_a = s32(cpu.uc.reg_read(UC_X86_REG_EAX))
                    if not res_a:
                        cpu.write(o_, bytes(12))
                else:
                    cpu.call(0x51e460, ecx=bp, edx=bp + 12, stack_args=(args[6], o_))
                out_a = [s32(x) for x in struct.unpack('<3I', cpu.read(o_, 12))]
            # the port
            emu.rand = MsvcRand(seed)
            probe.probe_fly_seed(seed)
            if op == 29:
                res_b = probe.probe_fly_beam(4, (ctypes.c_int32 * 3)(*[s32(x) for x in args]))
            elif op >= 26:
                res_b = probe.probe_fly_ams(op - 26, (ctypes.c_int32 * len(args))(*[s32(x) for x in args]), out)
                if out_a != list(out[:7]):
                    raise SystemExit(f'session {sess} op {k} AMS ({op}): {out_a} vs {list(out[:7])}')
            elif op >= 21:
                res_b = probe.probe_fly_handler(op - 21, (ctypes.c_int32 * len(args))(*[s32(x) for x in args]))
            elif op >= 17:
                res_b = probe.probe_fly_beam(op - 17, (ctypes.c_int32 * len(args))(*[s32(x) for x in args]))
            else:
                res_b = probe.probe_fly_op(op, (ctypes.c_int32 * max(1, len(args)))(*[s32(x) for x in args]), seed,
                                           out)
            if op in (6,):
                if not close(res_a & 0xffffffff, res_b & 0xffffffff, TOL) or out_a[0] != out[0]:
                    raise SystemExit(f'session {sess} op {k} aim slope: {res_a:#x} {out_a} vs {res_b:#x} {out[0]:#x}')
                res_a = res_b = 0
            if op in (15, 16):
                if op == 15 and not res_b:
                    out[0] = out[1] = out[2] = 0
                if any(not close(x & 0xffffffff, y & 0xffffffff, TOL, 1.0) for x, y in zip(out_a, out[:3])):  # unit vectors
                    raise SystemExit(f'session {sess} op {k} ({op}) {[hex(x) for x in args]}: '
                                     f'{[hex(x & 0xffffffff) for x in out_a]} vs {[hex(x & 0xffffffff) for x in out[:3]]}')
            if op == 7 and out_a != list(out[:4]):
                raise SystemExit(f'session {sess} op {k} pool stats {out_a} vs {list(out[:4])}')
            a_ev, b_ev = emu.events['orig'], emu.events['port']
            first = compare_events(a_ev, b_ev, TOL, stats)
            where = f'session {sess} op {k} ({op}, args {args[:3]})'
            if first is not None or s32(res_a) != s32(res_b):
                raise SystemExit(f'{where}: result {res_a:#x} vs {res_b:#x}; calls differ at {first} '
                                 f'({len(a_ev)} vs {len(b_ev)}):\n  original:\n    {show(a_ev[max(0, first - 8):first + 6])}'
                                 f'\n  port:\n    {show(b_ev[max(0, first - 8):first + 6])}')
            stats['calls'] += len(a_ev)
            buf = (ctypes.c_int32 * 4000)()
            for i in range(emu.npool):
                a, fl = emu.slot_fields(i)
                nb = probe.probe_fly_slot(i, buf)
                err = compare_lists(f'{where}: slot {i}', a, list(buf[:nb]), fl, TOL, stats)
                if err:
                    if os.environ.get("FLY_DEBUG"):
                        print([hex(x & 0xffffffff) for x in a])
                        print([hex(x & 0xffffffff) for x in buf[:nb]])
                        print(show(a_ev))
                    raise SystemExit(err)
            a, fl = emu.world_fields()
            nb = probe.probe_fly_world(buf)
            err = compare_lists(f'{where}: world', a, list(buf[:nb]), fl, TOL, stats)
            if err:
                if os.environ.get("FLY_DEBUG"):
                    import difflib
                    print("\n".join(list(difflib.unified_diff([str(x) for x in a], [str(x) for x in buf[:nb]], lineterm=""))[:40]))
                    print(a.index(-1), a[160:200], list(buf[160:200]))
                raise SystemExit(err)
            for slot in emu.beams:
                a, fl = emu.beam_fields(slot)
                nb = probe.probe_fly_beam_dump(slot, buf)
                err = compare_lists(f'{where}: beam {slot}', a, list(buf[:nb]), fl, TOL, stats)
                if err:
                    raise SystemExit(err)
            la = []
            for i in range(8):
                la += [s32(emu.u32(emu.lock[1] + 12 * i)), emu.ltok(emu.u32(emu.lock[1] + 12 * i + 4))]
            probe.probe_fly_locks(buf)
            if la != list(buf[:16]):
                raise SystemExit(f'{where}: lock array {la} vs {list(buf[:16])}')
            ra = [x for e in emu.radar() for x in e]
            nb = probe.probe_fly_radar(buf)
            if ra != list(buf[:nb]):
                raise SystemExit(f'{where}: radar {ra} vs {list(buf[:nb])}')
            sa, sb = emu.scene_fields('orig'), emu.scene_fields('port')
            if sa != sb:
                i = next(i for i, (x, y) in enumerate(zip(sa, sb)) if x != y)
                raise SystemExit(f'{where}: scene node {sa[i][0]:#x}: {sa[i]} vs {sb[i]}')
    missing = sorted(COVER[a] for a in set(COVER) - emu.reached)
    if missing:
        raise SystemExit(f'coverage: never reached {missing}')
    print(f'ok: {sess} sessions, {stats["ops"]} operations ({stats["spawned"]} shots) agree; {stats["calls"]} '
          f'calls out matched, {stats["near"]} trig-fed floats within {TOL}; all {len(COVER)} paths reached')


def set_choices(d, sound, flyout):
    """The port's fire sound and flyout animation choices (the emulator's were written)."""
    probe.probe_fly_choice(d, sound, flyout)


probe.probe_fly_beam.argtypes = [ctypes.c_int, ctypes.c_void_p]
probe.probe_fly_beam.restype = ctypes.c_int32
probe.probe_fly_beam_at.argtypes = [ctypes.c_int, ctypes.c_void_p]
probe.probe_fly_beam_dump.argtypes = [ctypes.c_int, ctypes.c_void_p]
probe.probe_fly_locks.argtypes = [ctypes.c_void_p]
probe.probe_fly_radar.argtypes = [ctypes.c_void_p]
probe.probe_fly_handler.argtypes = [ctypes.c_int, ctypes.c_void_p]
probe.probe_fly_handler.restype = ctypes.c_int32
probe.probe_fly_table.argtypes = [ctypes.c_int32, ctypes.c_void_p]
probe.probe_fly_ams.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p]
probe.probe_fly_ams.restype = ctypes.c_int32
probe.probe_fly_seed.argtypes = [ctypes.c_uint32]
probe.probe_fly_trim.argtypes = [ctypes.c_int]
probe.probe_fly_net.argtypes = [ctypes.c_int, ctypes.c_int]
probe.probe_fly_choice.argtypes = [ctypes.c_int, ctypes.c_int32, ctypes.c_int32]

if __name__ == '__main__':
    main()
