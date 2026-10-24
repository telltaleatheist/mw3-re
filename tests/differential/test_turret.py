"""The turrets as objects against the original x86 (docs/vehicles.md, The
turrets): the loader 0x4a76b0 (the HUD markers, aiTurrets.zrd's TURRET
entries over list 6 by name prefix, a turret on each node whose `healthy`
child is active, placed through vt+0x38 and its AUX_ANIMs reset 0x4a7af0),
the constructor 0x4a5c60 over AiTarget 0x4d56e0 and the setup 0x4a6270 (every
key of the definition, the turn limits 0x4a4b30 in their four kinds), then
the slots: hits 0x4a7530, the outright kill 0x4a7600, deactivate 0x4a7620,
messages 0x4a74f0, the tint 0x4a75b0, 0x4a7a70, 0x4a7ab0, 0x46b780, the
markers 0x4a5fc0; the destructor 0x4a5fd0 / 0x4a5ff0 and the end 0x4a7970.

    cmake --build build && .venv/bin/python tests/differential/test_turret.py [cases]

The shipped aiTurrets.zrd, then made-up files (keys present or not, PARTS of
one to five names, yaw limits of every kind, a weapon or none, a beam weapon,
AMS, odd values). The original's zReader runs in the emulator on the same
tree the port reads; every other call out is stubbed and answered from one
script, the world's objects (nodes, matrices, weapon definitions, the
attacker) shared memory both sides read. Compared: the call traces, each
turret's 0x284 bytes, its animation lists and AMS after the load and after
every slot. The x87 at 24 bits.
"""
import ctypes
import hashlib
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools' / 'analysis'))
from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EIP  # noqa: E402
from calltrace import Emu, Trace, call_abs, compare, load_probe, mov_ecx  # noqa: E402
from ai_world import ZBD, ser  # noqa: E402
import zrd  # noqa: E402

P, U32, I32 = ctypes.c_void_p, ctypes.c_uint32, ctypes.c_int32
PILOT, WORLD = 0x60861c, 0x608c84
MARKERS, COUNT = 0x609a38, 0x609a48
STRICMP = 0x5bb580
SIZE = 0x284
# Heap pointers: AiTarget's vector, the AMS, the lists' heads; the player's
# contact and the AMS compared as set or not.
SKIP = set(range(0x88, 0x94)) | set(range(0x250, 0x254)) | set(range(0x25c, 0x260)) | \
    set(range(0x268, 0x26c)) | set(range(0x274, 0x278))
BOOLS = (0x98, 0x13c)
COVER = {
    0x4a62db: 'a healthy node', 0x4a62e8: 'PARTS of four', 0x4a6349: 'PARTS of three', 0x4a6384: 'PARTS of two',
    0x4a63e6: 'a DEACTIVATE path', 0x4a643e: 'an EFFECT node', 0x4a64db: 'an ONFIRE_ANIM', 0x4a657d: 'a WAKEUP_ANIM',
    0x4a6612: 'a SLEEP_ANIM', 0x4a66ab: 'an AUX_ANIM claimed', 0x4a6739: 'a fire sound', 0x4a6787: 'a YAW',
    0x4a681a: 'a weapon by name', 0x4a6852: "the weapon's range", 0x4a686d: 'FIRE_LIMITS', 0x4a68b5: 'an AMS',
    0x4a6a1c: 'a beam', 0x4a6aa7: 'the callback refused', 0x4a6ab7: 'no destroy animation', 0x4a6b11: 'a box',
    0x4a6b82: 'TURRET_HEIGHT', 0x4a4b5a: 'fixed limits', 0x4a4b93: 'narrow limits', 0x4a4bcb: 'wide limits',
    0x4a77c5: 'no file', 0x4a78a4: 'a turret made', 0x4a762d: 'a kill credited', 0x4a764f: 'the destroy animation',
    0x4a766e: 'the effect put out', 0x4a7578: 'a hit that kills', 0x4a7593: 'a hit that does not',
    0x4a75c3: 'a tint', 0x4a7a9d: 'awake', 0x4a7ad9: 'in range', 0x4a602a: 'the AMS freed', 0x4a6044: 'the beam freed',
    0x4a750f: 'message 0x34', 0x4a7519: 'message 0x33', 0x4a7523: 'message 0x32', 0x4a7501: 'another message',
}
REACHED = set()


def f2u(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


def u2f(u):
    return struct.unpack('<f', struct.pack('<I', u))[0]


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


def h32(*parts):
    return int.from_bytes(hashlib.blake2b(repr(parts).encode(), digest_size=4).digest(), 'little')


def nan_free(img):
    """Every NaN as one: the x87 makes the negative one (0 / 0, inf * 0),
    the port's hardware the positive (play-accurate, not bit-identical)."""
    b = bytearray(img)
    for o in range(0, len(b), 4):
        v = struct.unpack_from('<I', b, o)[0]
        if (v & 0x7f800000) == 0x7f800000 and v & 0x7fffff:
            struct.pack_into('<I', b, o, 0x7fc00000)
    return bytes(b)


# --- the world both sides see -------------------------------------------------------------
class World:
    """The objects of modules not ported, made on first ask and kept for both
    sides: their memory is the emulator's and the probe's model of it."""

    def __init__(self, e, seed):
        self.e, self.seed = e, seed
        self.objs = {}
        self.markers = []
        self.mem = {}
        self.names = {}
        self.r = random.Random(seed)
        self.player = e.object('obj', slots={0x80: ('c,s0', 4)})  # credited with outright kills
        self.put(PILOT, self.block())
        self.put(self.mem[PILOT] + 0x14, self.player)
        world = self.block()
        self.put(WORLD, world)
        self.put(world + 4, self.handle('worldnode'))

    def put(self, a, v):
        self.e.w32(a, v & 0xffffffff)
        self.mem[a] = v & 0xffffffff

    def putf(self, a, f):
        self.put(a, f2u(f))

    def block(self, n=0x100):
        a = self.e.world_alloc(n)
        self.e.cpu.write(a, b'\0' * n)
        return a

    def handle(self, *key):
        """An opaque handle (nothing reads through it)."""
        return 0x2a000000 + (h32(self.seed, key) & 0xfffff0)

    def chance(self, p, *key):
        return (h32(self.seed, 'p', key) % 1000) < p * 1000

    def node(self, *key, name=None):
        """A zClass node: its name at +0, flags +0x24 (4 active), model +0x3c."""
        if key in self.objs:
            return self.objs[key]
        a = self.block()
        nm = name or f'n{h32(key) & 0xffff:04x}'
        self.e.cpu.write(a, nm.encode()[:0x20] + b'\0')
        self.names[a] = nm
        self.put(a + 0x24, 4 if self.chance(0.7, 'active', key) else 0)
        self.put(a + 0x3c, self.handle('model', key))
        self.putf(a + 0x28, self.r.uniform(-3, 6))  # read as a matrix's y
        self.objs[key] = a
        return a

    def weapon(self, name):
        key = ('weapon', name)
        if key not in self.objs:
            a = self.block()
            self.putf(a + 0x1c, self.r.choice([150.0, 300.0, 600.0, 1000.0]))
            self.put(a + 0x6c, self.r.choice([0, 0, 1, 2, 3]))
            self.objs[key] = a
        return self.objs[key]

    def attacker(self):
        key = ('attacker',)
        if key not in self.objs:
            self.objs[key] = self.e.object('obj', slots={0x80: ('c,s0', 4)})
        return self.objs[key]


def script_for(w, names_by_prefix, root):
    """One side's answers (the walk of list 6 keeps its own place)."""
    walk = {'list': [], 'i': 0}

    def nodes(n, a):
        name = a[0]
        if name:
            walk['list'] = names_by_prefix.get(name, [])
            walk['i'] = 0
            return 0
        if walk['i'] >= len(walk['list']):
            return 0
        walk['i'] += 1
        return w.node('world', walk['list'][walk['i'] - 1], name=walk['list'][walk['i'] - 1])

    def find(n, a):
        root, name = a
        if name == 'healthy':
            return w.node('healthy', root) if w.chance(0.85, 'h', root) else 0
        if not name or not w.chance(0.85, 'f', root, name):
            return 0
        return w.node('child', root, name)

    def anim(n, a):
        return w.handle('anim', a[0]) if a[0] and w.chance(0.8, 'anim', a[0]) else 0

    def fval(kind):
        return lambda n, a: f2u(round(random.Random(h32(w.seed, kind, a)).uniform(-20, 20), 3))

    def box(n, a):
        return f2u(random.Random(h32(w.seed, 'box', a)).uniform(-6, 6) + (3 if a[1] >= 3 else 0))

    return {
        '0x0059b830': lambda n, a: w.markers[n] if n < len(w.markers) else 0,
        '0x00556740': root,
        '0x0050b450': anim,
        '0x004f8b80': nodes,
        '0x004f9d60': find,
        '0x004eb0d0': find,
        '0x004f7fe0': lambda n, a: w.node('named', a[1]) if a[1] and w.chance(0.8, 'named', a[1]) else 0,
        '0x004fc3f0': lambda n, a: w.node('matrix', a[0]),
        '@pos': fval('pos'), '@rot': fval('rot'), '@tr': fval('tr'), '@box': box,
        '@tnode': lambda n, a: w.handle('tnode', a[0]),
        '@cnode': lambda n, a: w.handle('cnode', a[0]),
        '0x004ebd50': lambda n, a: 0 if w.chance(0.7, 'box', a[0]) else 1,
        '0x00581ed0': lambda n, a: w.weapon(a[0]) if a[0] and a[0] != 'nope' else 0,
        '0x0056f6f0': lambda n, a: w.handle('sound', a[0]) if a[0] and w.chance(0.8, 's', a[0]) else 0,
        '0x004d4f20': lambda n, a: h32(a) & 1,
        '0x005833b0': lambda n, a: 0 if w.chance(0.15, 'cb', a[0]) else 1,
        '0x00584a70': lambda n, a: w.handle('beam', a[0]),
        '0x00584b40': 0,
        '0x00581e40': lambda n, a: w.handle('hitnode', n),
        '0x0040eee0': lambda n, a: w.attacker() if w.chance(0.7, 'att', a[0]) else 0,
        '0x0053cc90': lambda n, a: h32(w.seed, 'z', a) % 3,
    }


# --- the original's side ---------------------------------------------------------------------
def emu_tree(e, v):
    """A reader tree in the emulator's memory (zreader.h: 8-byte nodes; a
    list's first slot its count)."""
    a = e.world_alloc(8)
    put_node(e, a, v)
    return a


def put_node(e, a, v):
    if isinstance(v, list):
        items = e.world_alloc(8 * (len(v) + 1))
        e.w32(items, 1)
        e.w32(items + 4, len(v) + 1)
        for i, x in enumerate(v):
            put_node(e, items + 8 * (i + 1), x)
        e.w32(a, 4)
        e.w32(a + 4, items)
    elif isinstance(v, str):
        e.w32(a, 3)
        e.w32(a + 4, e.world_string(v))
    elif isinstance(v, float):
        e.w32(a, 2)
        e.w32(a + 4, f2u(v))
    else:
        e.w32(a, 1)
        e.w32(a + 4, v & 0xffffffff)


STUBS = {
    0x59b830: 'zs0,s1,s2,s3', 0x556740: 'zc,d', 0x556820: 'c', 0x50b450: 'zc', 0x4f8b80: 'zc,d',
    0x4f9d60: 'c,zd', 0x4eb0d0: 'c,zd', 0x4f7fe0: 'c,zd', 0x4eb170: 'c,d', 0x4eb750: 'c,d',
    0x4fc3f0: 'c', 0x4fc0b0: 'c,s0,s1,s2', 0x4f7e50: '', 0x4fb8c0: 'c,d,s0', 0x4fb840: 'c,d',
    0x526ae0: 'c,d,s0,s1', 0x508460: 'c,d', 0x508c10: 'c,d,s0,s1,s2', 0x4d4f20: 'c,d', 0x56f6f0: 'zc',
    0x581ed0: 'zc', 0x584a70: 'c,d,s0,s1,s2,s3,s4', 0x584b40: 'c', 0x581400: 'c', 0x5833b0: 'c,d,s0,s1',
    0x583400: 'c', 0x581e40: '', 0x40eee0: '*c', 0x583710: 'c,d', 0x583730: 's0', 0x53cc90: 'c,d',
    0x5a7e10: 'c,s0', 0x5a7f90: 'c', 0x5a7f50: 'c', 0x5a7f70: 'c', 0x5a80f0: 'c,s0,s1', 0x5a8500: 'c',
    0x5a85c0: 'c', 0x4d7270: 'c,s0,s1', 0x542e40: 'c', 0x542ca0: 'c',
}


class Original:
    def __init__(self, tree, world_seed, prefixes, read_ok):
        t = Trace()
        e = Emu(t)
        self.e, self.t = e, t
        e.cpu.fpu_control(0x007f)
        self.w = World(e, world_seed)
        t.script = script_for(self.w, prefixes, 0)
        self.turrets = []

        def new(em, key, a, pop):
            p = em.alloc(max(a[0], 4))
            if a[0] == SIZE:
                self.turrets.append(p)
            em._ret(p, 0)
        e.stub(0x5abd26, 'new', 's0', 0, new)
        e.stub(0x5abd1a, 'delete', 's0', 0, lambda em, k, a, p: em._ret(0, 0))
        t.silent |= {'new', 'delete'}
        def stricmp(em, k, a, p):
            x, y = em.cstr(a[0]).lower(), em.cstr(a[1]).lower()
            em._ret((x > y) - (x < y), 0)
        e.import_stub(STRICMP, 'stricmp', 's0,s1', 0, stricmp)
        t.silent.add('stricmp')
        for addr, spec in STUBS.items():
            e.stub(addr, spec=spec)
        e.stub(0x49bfd0, spec='s0,zs1,s2,zs3,zs4', pop=0)
        e.stub(0x41ebb0, 'marker.vt+0x0', 'c,s0')  # the zTwoD image's scalar delete (vtable 0x5bce78)
        self.outs()
        self.root = emu_tree(e, tree) if tree is not None else 0
        t.script['0x00556740'] = self.root if read_ok else 0
        e.stub(0x4d6c90, spec='c,s0,s1,s2,s3', fn=self.set_node)

        def marker(em, key, a, pop):  # the image's constructor on the loader's block
            self.w.markers.append(em.args('c')[0])
            em._ret(t.handle(key, a), pop)
        e.stub(0x59b830, spec='zs0,s1,s2,s3', fn=marker)

        def hit(uc, address, size, user):
            REACHED.add(address)
        for a in COVER:
            e.uc.hook_add(UC_HOOK_CODE, hit, begin=a, end=a)

    def outs(self):
        e, t = self.e, self.t

        def writer(key, regs):
            def fn(em, k, a, pop):
                v = t.handle(k, a)
                node = a[0]
                for i, where in enumerate(regs):
                    em.w32(where(em), t.handle(key, [node, i]))
                em._ret(v, pop)
            return fn

        def edx(off):
            return lambda em: em.args('d')[0] + off

        def stack(n):
            return lambda em: em.args(f's{n}')[0]
        e.stub(0x4ed650, spec='c', fn=writer('@pos', [edx(0), edx(4), edx(8)]))
        e.stub(0x4fbe90, spec='c', fn=writer('@rot', [edx(0), stack(0), stack(1)]))
        e.stub(0x4fc020, spec='c', fn=writer('@tr', [edx(0), stack(0), stack(1)]))

        def box(em, k, a, pop):
            v = t.handle(k, a)
            if v == 0:
                out = em.args('d')[0]
                for i in range(6):
                    em.w32(out + 4 * i, t.handle('@box', [a[0], i]))
            em._ret(v, pop)
        e.stub(0x4ebd50, spec='c', fn=box)

    def set_node(self, em, key, a, pop):
        self.t.handle(key, a)
        this, node = a[0], a[1]
        em.w32(this + 4, node)
        em.w32(this + 0x10, self.t.handle('@tnode', [node]) if node else 0)
        em.w32(this + 0x14, self.t.handle('@cnode', [node]) if node else 0)
        em._ret(0, pop)

    def load(self, at):
        return self.e.call(0x4a76b0, ecx=self.e.world_string('aiTurrets.zrd'), edx=at, count=400_000_000) & 0xff

    def state(self, p):
        e = self.e
        img = bytearray(e.cpu.read(p, SIZE))
        for o in SKIP:
            img[o] = 0
        for o in BOOLS:
            v = int(struct.unpack_from('<I', img, o)[0] != 0)
            struct.pack_into('<I', img, o, v)
        extra = []
        for head in (0x250, 0x25c, 0x268, 0x274):
            h = e.r32(p + head)
            items, q = [], e.r32(h)
            while q != h:
                items.append(e.r32(q + 8))
                q = e.r32(q)
            extra += [len(items)] + items
        ams = e.r32(p + 0x13c)
        if ams:
            extra += [e.r8(ams), e.r32(ams + 4), e.r32(ams + 8), e.r32(ams + 0xc), e.r32(ams + 0x18)]
        return bytes(img), extra

    def op(self, p, op, a, b, c):
        e = self.e
        if op == 0:
            return e.call(0x4a7530, ecx=p, stack=(a, b, c))
        if op == 1:
            e.call(0x4a7600, ecx=p)
            return 0
        if op == 2:
            e.call(0x4a7620, ecx=p, stack=(a,))
            return 0
        if op == 3:
            e.call(0x4a74f0, ecx=p, stack=(a, b))
            return 0
        if op == 4:
            e.call(0x4a75b0, ecx=p, stack=(a,))
            return 0
        if op == 5:
            return e.call(0x4a7a70, ecx=p) & 0xff
        if op == 6:
            return e.call(0x4a7ab0, ecx=p) & 0xff
        if op == 7:
            out = e.alloc(4)
            code = mov_ecx(p)
            th = e.alloc(64)
            code += call_abs(th + len(code), 0x46b780)
            code += b'\xd9\x1d' + struct.pack('<I', out) + b'\xc3'
            e.cpu.write(th, code)
            e.call(th)
            return e.r32(out)
        if op == 8:
            c_ = 0
            if b:
                c_ = e.alloc(0x30)
                e.w32(c_ + 0xc, a)
            e.w32(p + 0x98, c_)
            return 0
        if op == 9:
            return e.r32(MARKERS + 4 * a)
        if op == 10:
            e.call(0x4a7af0, ecx=p)
            return 0

    def delete_all(self):
        for p in self.turrets:
            self.t.handle('turret.vt+0x0', [p, 1])
            self.e.call(0x4a5fd0, ecx=p, stack=(1,))

    def end(self):
        self.e.call(0x4a7970)


# --- the port's side ------------------------------------------------------------------------------
def setup_probe(probe):
    probe.probe_turret_poke.argtypes = [U32, U32]
    probe.probe_turret_name.argtypes = [U32, ctypes.c_char_p]
    probe.probe_turret_handles.argtypes = [P, I32]
    probe.probe_turret_node_args.argtypes = [P, I32]
    probe.probe_turret_load.argtypes = [ctypes.c_char_p, I32, U32]
    probe.probe_turret_load.restype = I32
    probe.probe_turret_state.argtypes = [I32, P, P]
    probe.probe_turret_state.restype = I32
    probe.probe_turret_op.argtypes = [I32, I32, U32, U32, U32]
    probe.probe_turret_op.restype = U32


def port_state(probe, i):
    img = (ctypes.c_uint8 * SIZE)()
    extra = (U32 * 4096)()
    n = probe.probe_turret_state(i, img, extra)
    return bytes(img), list(extra[:n])


# --- the cases -------------------------------------------------------------------------------------
def shipped():
    data = (ZBD / 'reader.zbd').read_bytes()
    for n, s, k in zrd.entries(data):
        if n == 'aiTurrets.zrd':
            return zrd.node(data[s:s + k], 0)[0]
    raise SystemExit('aiTurrets.zrd not found')


NAMES = ['turret', 'gun', 'firepoint', 'firepoint1', 'firepoint2', 'barrel', 'base']


def made_def(r, crash_ok):
    d = []
    if r.random() < 0.9:
        d += ['PARTS', r.sample(NAMES, r.choice([1, 2, 2, 3, 3, 3, 4, 4, 5]))]
    if r.random() < 0.15:
        d += ['DEACTIVATE', [f'off{k}' for k in range(r.randrange(1, 4))]]
    if r.random() < 0.15:
        d += ['EFFECT', [r.choice(['lightning', 'spark']), r.choice([0.5, 2.0, 1])]]
    if r.random() < 0.9:
        d += ['HEALTH', [r.choice([15.0, 20.0, 30.0, 100.0, 0.0, 7.5, 40])]]
    if r.random() < 0.8:
        d += ['DESTROY_ANIM', [r.choice(['destroy_tur_generic', 'destroy_tur_gauss1', 'nothing_here'])]]
    for key in ('ONFIRE_ANIM', 'WAKEUP_ANIM', 'SLEEP_ANIM', 'AUX_ANIM'):
        if r.random() < 0.3:
            d += [key, [f'{key.lower()}{k}' for k in range(r.randrange(0, 4))]]
    if r.random() < 0.4:
        d += ['IS_IDLE', [r.choice(['TRUE', 'FALSE', 'true', 1, 0, 2])]]
    if r.random() < 0.2:
        d += ['SOUNDS', ['FIRE', [r.choice(['wep_ams_fire', 'wep_x'])]]]
    if r.random() < 0.35:
        lo = r.choice([-30.0, -90.0, -179.0, -180.0, 0.0, -360.0, 10.0, -0.0001])
        span = r.choice([0.0, 0.00001, 0.05, 60.0, 179.9, 180.0, 181.0, 359.0, 360.0, 400.0, -20.0])
        d += ['LIMITS', ['YAW', [lo, lo + span]] if r.random() < 0.9 else ['PITCH', [1.0, 2.0]]]
    if r.random() < 0.2:
        d += ['BBOX', [r.choice([0, 1, 2])]]
    if r.random() < 0.9 or not crash_ok:
        w = []
        name_kind = r.random()
        if name_kind < 0.75:
            w += ['NAME', [r.choice(['SRM2', 'ERPPC', 'GaussRifle', 'LRM5', 'ERLaser_Large', 'nope'])]]
        elif name_kind < 0.85:
            w += ['NAME', []]
        elif name_kind < 0.9:
            w += ['NAME', ['A', 'B']]
        real = len(w) == 2 and len(w[1]) == 1 and w[1][0] != 'nope'
        if r.random() < 0.2:
            w += ['AMMO', [r.choice([10, 0, 500])]]
        if r.random() < 0.3 or (not crash_ok and not real):
            w += ['DETECTION_RANGE', [r.choice([800.0, 250.0, 0.0])]]
        if r.random() < 0.6:
            w += ['FIRE_LIMITS', [r.choice([0.4, 0.125, 2.5]), r.choice([10.0, 4.0, 0.125])]]
        if r.random() < 0.8:
            w += ['FIRE_RATE', [r.choice([0.2, 10.0, 0.1, 7.0])]]
        d += ['WEAPON', w]
    if r.random() < 0.1:
        d += ['AMS_RANGE', [r.choice([250.0, 100.0])]]
    if r.random() < 0.15:
        d += ['TURRET_HEIGHT', [r.choice([12.0, 3.5])]]
    return d


def made_tree(r, crash_ok=False):
    names = [f'tur_{k}{r.choice(["", "x"])}' for k in range(r.randrange(1, 6))]
    top = []
    if r.random() < 0.8:
        top += ['GENERIC_DESTROY_ANIM', ['destroy_tur_generic']]
    lst = []
    for nm in names:
        lst += [nm, made_def(r, crash_ok)]
    top += ['TURRET', lst]
    return [top]


def turret_names(tree):
    lst = tree[0][tree[0].index('TURRET') + 1]
    return lst[0::2]


def prefixes_for(r, names):
    out = {}
    for nm in names:
        k = r.choice([0, 1, 1, 2, 3])
        out[nm] = [f'{nm}{j:02d}' for j in range(1, k + 1)]
    return out


def ops_for(r, n):
    ops = []
    for _ in range(r.randrange(0, 14)):
        op = r.choice([0, 0, 0, 1, 2, 3, 3, 4, 5, 6, 7, 8, 9, 10])
        i = r.randrange(n) if n else 0
        if op == 0:
            ops.append((i, 0, 0x55, 0x66, f2u(r.choice([0.5, 5.0, 10.0, 25.0, 100.0, 0.0]))))
        elif op == 2:
            ops.append((i, 2, r.choice([0, 1]), 0, 0))
        elif op == 3:
            ops.append((i, 3, 0x77, r.choice([0x32, 0x33, 0x34, 0x10, 7]), 0))
        elif op == 4:
            ops.append((i, 4, r.choice([0, 1]), 0, 0))
        elif op == 8:
            ops.append((i, 8, f2u(r.choice([100.0, 39999.0, 40001.0, 640000.0, 1e7])), r.choice([0, 1]), 0))
        elif op == 9:
            ops.append((i, 9, r.randrange(3), 0, 0))
        elif op == 6:
            if r.random() < 0.6:  # at the turret's detection range, times this
                ops.append((i, 8, f2u(r.choice([0.5, 0.9999, 1.0, 1.0001, 2.0])), 2, 0))
            ops.append((i, 6, 0, 0, 0))
        else:
            ops.append((i, op, 0, 0, 0))
    return ops


class Run:
    def __init__(self, probe):
        self.probe = probe
        self.fails = 0
        self.cases = 0
        self.made = 0
        self.kinds = set()

    def fail(self, label, msg):
        self.fails += 1
        if self.fails <= 8:
            print(f'MISMATCH {label}: {msg}')

    def run(self, label, tree, seed, read_ok=True, port_only=False):
        r = random.Random(seed)
        names = turret_names(tree) if tree else []
        prefixes = prefixes_for(r, names)
        at = 0x2b000000 + (seed & 0xff0)
        ops = None
        self.cases += 1
        o = Original(tree, seed, prefixes, read_ok)
        try:
            ok = o.load(at)
        except Exception as ex:  # the original crashed; the port must not
            if not port_only:
                eip = o.e.uc.reg_read(UC_X86_REG_EIP)
                self.fail(label, f'the original crashed at {eip:#x}: {ex}')
                return
            o = None
        probe = self.probe
        t = Trace()
        probe.probe_app_set_callback(t.callback())
        probe.probe_turret_reset()  # the last run's turrets, if it stopped early
        t.reset()
        w = o.w if o else World(Emu(Trace()), seed)
        t.script = script_for(w, prefixes, o.root if o and read_ok else int(read_ok))
        for a, v in w.mem.items():
            probe.probe_turret_poke(a, v)
        for a, nm in w.names.items():
            probe.probe_turret_name(a, nm.encode())
        handles = o.turrets if o else [0x2d000000 + 0x300 * k for k in range(64)]
        arr = (U32 * max(1, len(handles)))(*handles)
        probe.probe_turret_handles(arr, len(handles))
        data = ser(tree) if tree is not None else ser([])
        args = []
        if o:
            for p in o.turrets:
                args.append(o.e.r32(p + 4))
        probe.probe_turret_node_args((U32 * max(1, len(args)))(*args), len(args))
        n = probe.probe_turret_load(data, len(data), at)
        if not o:
            probe.probe_turret_delete_all()
            probe.probe_turret_end()
            return
        if (n >= 0) != bool(ok) or (ok and n != len(o.turrets)):
            self.fail(label, f'load {ok} with {len(o.turrets)} turrets, port {n}')
            return
        self.made += len(o.turrets)
        if not compare(f'{label} load trace', o.t.log, t.log):
            self.fail(label, 'load trace')
            return
        if not self.states(label + ' load', o, probe):
            return
        ops = ops_for(r, len(o.turrets))
        o.t.log.clear()
        t.log.clear()
        for k, (i, op, a, b, c) in enumerate(ops):
            if i >= len(o.turrets) and op != 9:
                continue
            if op == 8 and b == 2:
                rng = o.e.f32(o.turrets[i] + 0x148)
                a, b = f2u(f32(f32(rng * rng) * u2f(a))), 1
            mine = o.op(o.turrets[i] if o.turrets else 0, op, a, b, c)
            port = probe.probe_turret_op(i, op, a, b, c)
            if op in (0, 5, 6, 7, 9) and (mine & (0xff if op in (5, 6) else 0xffffffff)) != port:
                self.fail(label, f'op {k} {op}: {mine:#x} vs {port:#x}')
                return
            if not self.states(f'{label} op {k} ({op})', o, probe):
                return
        if not compare(f'{label} ops trace', o.t.log, t.log):
            self.fail(label, 'ops trace')
            return
        o.t.log.clear()
        t.log.clear()
        o.delete_all()
        o.end()
        probe.probe_turret_delete_all()
        probe.probe_turret_end()
        if not compare(f'{label} end trace', o.t.log, t.log):
            self.fail(label, 'end trace')

    def states(self, label, o, probe):
        for i, p in enumerate(o.turrets):
            mine, mx = o.state(p)
            port, px = port_state(probe, i)
            mine, port = nan_free(mine), nan_free(port)
            self.kinds.add(struct.unpack_from('<I', mine, 0x210)[0])
            if mine != port:
                diffs = [k for k in range(SIZE) if mine[k] != port[k]]
                self.fail(label, f'turret {i} bytes differ at {[hex(d) for d in diffs[:12]]}: '
                          f'{mine[diffs[0] & ~3:(diffs[0] & ~3) + 8].hex()} vs {port[diffs[0] & ~3:(diffs[0] & ~3) + 8].hex()}')
                return False
            if mx != px:
                self.fail(label, f'turret {i} lists / AMS {mx} vs {px}')
                return False
        return True


def main():
    probe = load_probe()
    setup_probe(probe)
    run = Run(probe)
    cases = int(sys.argv[1]) if len(sys.argv) > 1 else 300
    tree = shipped()
    for seed in range(12):
        run.run(f'shipped {seed}', tree, 1000 + seed)
    run.run('no file', tree, 77, read_ok=False)
    rng = random.Random(5)
    for k in range(cases):
        run.run(f'made-up {k}', made_tree(rng), 2000 + k)
    # A WEAPON without its weapon or a DETECTION_RANGE: the exe reads the
    # null definition (FIX (bug)); the port alone, which must not crash.
    for k in range(cases // 10):
        run.run(f'crash {k}', made_tree(rng, crash_ok=True), 5000 + k, port_only=True)
    kinds = {0x5c8310: 'fixed', 0x5c8320: 'narrow', 0x5c8330: 'wide', 0x5c8340: 'full'}
    seen = sorted(kinds.get(k, hex(k)) for k in run.kinds)
    if run.fails:
        raise SystemExit(f'{run.fails} of {run.cases} differ')
    missed = [v for a, v in COVER.items() if a not in REACHED]
    if missed:
        raise SystemExit(f'not reached: {missed}')
    if len(run.kinds) < 4:
        raise SystemExit(f'only these limit kinds reached: {seen}')
    print(f'ok: {run.cases} files, {run.made} turrets, every slot after the load, limits {seen}, {len(COVER)} branches, as the original')


if __name__ == '__main__':
    main()
