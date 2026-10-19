"""The weapon definitions (docs/weapons.md, "Definitions") against the original x86:
zWeapon's reader 0x583880 with its effect blocks 0x584bb0 and the def lookup
0x581ed0, and the MW3 half 0x4b0020 / 0x4af5a0 (WeaponType) it calls back.

    cmake --build build && .venv/bin/python tests/differential/test_weapons_zrd.py

The real weapons.zrd first, then a few hundred variants of it: keys dropped,
added and changed (gravity, beams with PULSE, craters, quicksand, LOCK_ON_LEAD,
target heat, damage animations, salvo, burst, airstrike, ZAPS_HUD forms,
effect blocks of every kind), a wrong or missing VERSION, no BALLISTICS.
Both sides read the same bytes: the emulator through the original's own
zReader (0x557180), the port from a one-entry archive. Every call into
unported systems (sounds, messages, animations, effects, nodes, the crater
defaults, the pool and tick objects, errors) is logged on both sides and
answered with a value made from its argument. Every def (0x1f4 bytes), its six
impact blocks and its WeaponType (0xc8) must match byte for byte at the
game's 24-bit x87 precision, and so must the globals and the mech lab's view.

The port fixes two bugs (docs/fixes.md), so the original runs patched the
same way: the pool size reads the def's range, not a stale local
(0x58468e), and LOCK_ON_LEAD checks its count, not its first value (0x583fb9).
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

os.environ.setdefault('MW3_X87', '24')
sys.path.insert(0, str(Path(__file__).parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools' / 'analysis'))
from original import FTOL, IAT_FTOL, ROOT, Original  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402
import zrd  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
probe.probe_zwep_run.argtypes = [ctypes.c_char_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_void_p,
                                 ctypes.c_int, ctypes.c_void_p]
probe.probe_zwep_def.argtypes = [ctypes.c_int] + [ctypes.c_void_p] * 4 + [ctypes.c_int]
READER = ROOT / 'game' / 'mw3-1.2' / 'zbd' / 'reader.zbd'
WORK = ROOT / 'build' / 'weapons_zrd'

INIT, STARTUP, READ_NODE, CALLBACK = 0x583880, 0x583750, 0x557180, 0x4b0020
IAT = {'readfile': 0x5bb0c4, 'malloc': 0x5bb664, 'stricmp': 0x5bb580, 'calloc': 0x5bb60c, 'strdup': 0x5bb670,
       'new': 0x5bb4bc}
HANDLE, WORLD, ARG, TICK = 0x1234, 0x6600, 0x1234, 0x7700
PATCHES = {0x58468e: (b'\xd9\x45\xf0', b'\xd9\x43\x1c'), 0x583fb9: (b'\x83\x78\x0c\x03', b'\x83\x78\x04\x03')}
# Places in the original the variants must reach.
COVER = {0x5838fc: 'wrong version', 0x584789: 'no version', 0x583c0d: 'beam time <= 0', 0x583c5d: 'pulse',
         0x583c6b: 'pulse rate <= 0', 0x583cc2: 'crater', 0x584073: 'quicksand crater', 0x583fd2: 'lead min > max',
         0x583fe5: 'lead span', 0x584414: 'freeze', 0x584483: 'impact heat', 0x5844eb: 'designate',
         0x5845dd: 'damage anims on health', 0x58439f: 'soil inherits min-range anim', 0x5843a8: 'soil copies default',
         0x5842dc: 'flyout health with model', 0x584658: 'flyout model', 0x58461a: 'gravity', 0x584729: 'no tick',
         0x4af7fc: 'salvo callback', 0x4af930: 'burst found', 0x4af9de: 'airstrike', 0x4afac1: 'lrm flight',
         0x4afa8c: 'velocity under 100', 0x4afaa3: 'velocity over 5000', 0x4af6ff: 'zaps int', 0x4af70c: 'zaps float',
         0x4af722: 'zaps second int', 0x4af72f: 'zaps second float', 0x4af6c7: 'show on radar', 0x4af85d: 'mass clan',
         0x4af8b7: 'ammo x truecount', 0x584bf1: 'effect index', 0x584c17: 'model', 0x584ce6: 'animation flag',
         0x584d07: 'random rotate', 0x584d40: 'sounds', 0x584d87: 'bounce sounds', 0x583fb0: 'lead min',
         0x583fbf: 'lead max', 0x584159: 'range minimum'}
NAMES = {1: 'lights', 2: 'sound', 3: 'message', 4: 'anim', 5: 'effect', 6: 'find_node', 7: 'crater_defaults',
         8: 'set_intersect_surface', 9: 'set_active', 10: 'tree_unlit', 11: 'pool_objects', 12: 'object3d_create',
         13: 'set_action_priority', 14: 'set_tick_action', 15: 'error'}


def fnv(b):
    h = 2166136261
    for c in b:
        h = ((h ^ c) * 16777619) & 0xffffffff
    return h


def handle(kind, b):
    return (kind << 24) | (fnv(b) & 0xffffff) if b else 0


def s32(v):
    v &= 0xffffffff
    return v - (1 << 32) if v & 0x80000000 else v


class Emulated:
    def __init__(self):
        cpu = self.cpu = Original()
        for va, (old, new) in PATCHES.items():
            assert cpu.read(va, len(old)) == old, hex(va)
            cpu.write(va, new)
        cpu.import_code(IAT_FTOL, FTOL)
        cpu.stub_import(IAT['readfile'], self.read_file, pop_bytes=20)
        cpu.stub_import(IAT['malloc'], lambda c: c.alloc(max(c.arg(0), 1)))
        cpu.stub_import(IAT['new'], lambda c: c.alloc(max(c.arg(0), 1)))
        cpu.stub_import(IAT['calloc'], self.calloc)
        cpu.stub_import(IAT['strdup'], self.strdup)
        cpu.stub_import(IAT['stricmp'], self.stricmp)
        stubs = {
            0x586970: (lambda c: self.log(1), 0),
            0x556740: (self.load, 4),
            0x56f6f0: (lambda c: self.named(2, 1, c.ecx), 0),
            0x575fb0: (self.message, 0),
            0x50b450: (lambda c: self.named(4, 2, c.ecx), 0),
            0x50d9e0: (lambda c: self.named(5, 3, c.ecx), 0),
            0x4f7fe0: (self.find_node, 0),
            0x4ff920: (self.crater, 0),
            0x4eb6d0: (lambda c: self.log(8, c.ecx, c.edx), 0),
            0x4eb170: (lambda c: self.log(9, c.ecx, c.edx), 0),
            0x4f9e00: (self.tree_unlit, 0),
            0x5847c0: (lambda c: self.log(11, c.ecx), 0),
            0x4fb740: (lambda c: self.log(12) or (TICK if self.tick_ok else 0), 0),
            0x4eb5b0: (lambda c: self.log(13, c.ecx, c.edx), 0),
            0x4eb450: (lambda c: self.check(c.edx == 0x5823c0) or self.log(14, c.ecx), 0),
            0x49bfd0: (lambda c: self.log(15, c.arg(0), c.arg(2)), 0),
            0x5a8a20: (lambda c: 0, 0xc),
        }
        for addr, (fn, pop) in stubs.items():
            cpu.stub_function(addr, fn, pop)
        self.reached = set()

        def hit(uc, address, size, user):
            self.reached.add(address)
        for addr in COVER:
            cpu.uc.hook_add(UC_HOOK_CODE, hit, begin=addr, end=addr)
        self.fslot = cpu.alloc(4)
        self.fstp = cpu.code(b'\xd9\x1d' + struct.pack('<I', self.fslot) + b'\xc3')  # the x87 result to memory
        self.path = cpu.alloc(0x20)
        cpu.write(self.path, b'weapons.zrd\0')
        self.base_heap = cpu.heap_next

    # --- C runtime -----------------------------------------------------------
    def read_file(self, cpu):
        h, buf, n, out = cpu.arg(0), cpu.arg(1), cpu.arg(2), cpu.arg(3)
        assert h == HANDLE
        chunk = self.data[self.pos:self.pos + n]
        self.pos += len(chunk)
        cpu.write(buf, chunk)
        cpu.u32(out, len(chunk))
        return 1

    def calloc(self, cpu):
        n = cpu.arg(0) * cpu.arg(1)
        p = cpu.alloc(max(n, 1))
        cpu.write(p, bytes(n))
        return p

    def strdup(self, cpu):
        s = self.cstr(cpu.arg(0))
        p = cpu.alloc(len(s) + 1)
        cpu.write(p, s + b'\0')
        return p

    def stricmp(self, cpu):
        a, b = self.cstr(cpu.arg(0)).lower(), self.cstr(cpu.arg(1)).lower()
        return (a > b) - (a < b)

    # --- stubs ---------------------------------------------------------------
    def log(self, *e):
        self.events.append(tuple(s32(v) for v in e))
        return 0

    def check(self, ok):
        if not ok:
            raise SystemExit('a stub was called with unexpected registers')
        return 0

    def named(self, code, kind, p):
        s = self.cstr(p)
        self.log(code, fnv(s))
        return handle(kind, s)

    def message(self, cpu):
        s = self.cstr(cpu.ecx)
        self.log(3, fnv(s))
        p = cpu.alloc(len(s) + 3)
        cpu.write(p, b'M:' + s + b'\0')
        return p

    def find_node(self, cpu):
        s = self.cstr(cpu.edx)
        self.log(6, cpu.ecx, fnv(s))
        return handle(4, s)

    def crater(self, cpu):
        self.log(7)
        cpu.write(cpu.ecx, struct.pack('<10I', *self.crater_defaults))
        return 0

    def tree_unlit(self, cpu):
        self.check(cpu.edx == 0x4f9de0)
        self.log(10, cpu.ecx)
        return (cpu.ecx >> 3) & 1

    def load(self, cpu):
        self.check(self.cstr(cpu.ecx) == b'weapons.zrd' and cpu.edx == 0 and cpu.arg(0) == 0)
        return self.root

    # --- memory --------------------------------------------------------------
    def u32(self, p):
        return struct.unpack('<I', self.cpu.read(p, 4))[0]

    def cstr(self, p):
        if p == 0:
            return b''
        out = bytearray()
        while True:
            b = self.cpu.read(p + len(out), 64)
            i = b.find(0)
            if i >= 0:
                return bytes(out + b[:i])
            out += b

    def run(self, data, crater, hardware, tick_ok):
        cpu = self.cpu
        cpu.heap_next = self.base_heap
        self.events, self.crater_defaults, self.tick_ok = [], crater, tick_ok
        self.data, self.pos = data, 0
        self.root = cpu.alloc(8)
        cpu.call(READ_NODE, ecx=HANDLE, edx=self.root)
        cpu.call(STARTUP)
        for a in (0x9a177c, 0x9a1794, 0x9a1798, 0x9a179c):  # kept over runs; the port's state starts fresh
            cpu.u32(a, 0)
        cpu.u32(0x72f348, hardware)
        cpu.call(INIT, ecx=WORLD, edx=self.path, stack_args=(ARG, CALLBACK))
        return s32(cpu.uc.reg_read(UC_X86_REG_EAX))

    def globals(self):
        u = self.u32
        n, pool, head = u(0x9a0ea8), u(0x9a0eb0), u(0x9a0eb8)
        base = u(0x9a0eb4)
        ok, k, p = 1, pool, head
        while p:
            if k == 0 or p != base + 0x5d8 * (k - 1):
                ok = 0
            k -= 1
            p = u(p)
        return [n, pool, u(0x9a177c), u(0x9a1794), u(0x9a1798), u(0x9a179c), u(0x9a1784), u(0x9a0ea0),
                u(0x9a0ec0), u(0x9a0ea4), int(ok and k == 0)]

    def defs(self):
        u, cpu = self.u32, self.cpu
        base, n = u(0x9a0eac), u(0x9a0ea8)
        out = []

        def token(p):
            return 0 if p == 0 else (p - base) // 0x1f4 + 1

        for i in range(n):
            d = base + 0x1f4 * i
            img = bytearray(cpu.read(d, 0x1f4))
            strings = [self.cstr(u(d + o)) if u(d + o) else b'<null>' for o in (0, 4, 8, 0xc)]
            imp, tp = u(d + 0x13c), u(d + 0x1f0)
            for o in (0, 4, 8, 0xc, 0x13c, 0x1f0):
                img[o:o + 4] = bytes(4)
            impacts = cpu.read(imp, 6 * 0x5c) if imp else bytes(6 * 0x5c)
            t = bytearray(0xc8)
            if tp:
                t = bytearray(cpu.read(tp, 0xc8))
                for o in (4, 0x58, 0xa0):
                    struct.pack_into('<I', t, o, token(u(tp + o)))
                t[0x80] = 0
                head = u(tp + 0x84)
                struct.pack_into('<I', t, 0x84, int(u(head) == head))
                struct.pack_into('<I', t, 0xb8, int(u(tp + 0xb8) == tp + 0xa4))
            out.append((bytes(img), bytes(impacts), bytes(t), strings))
        return out

    def lab(self, w):
        """0x484b70 (both techs), 0x484bc0, 0x484c00 (both techs) on weapon index w."""
        cpu, f = self.cpu, []
        for fn, edx in ((0x484b70, 1), (0x484b70, 2), (0x484bc0, 0)):
            cpu.call(fn, ecx=w, edx=edx)
            cpu.call(self.fstp)
            f.append(cpu.read(self.fslot, 4))
        i = []
        for tech in (1, 2):
            cpu.call(0x484c00, ecx=w, edx=0, stack_args=(tech,))
            i.append(s32(cpu.uc.reg_read(UC_X86_REG_EAX)))
        return f, i


def port_events(buf, n):
    out, i = [], 0
    while i < n:
        k = buf[i]
        out.append(tuple(s32(v) for v in buf[i + 1:i + 1 + k]))
        i += 1 + k
    return out


def write_archive(path, data):
    name = b'weapons.zrd'
    entry = struct.pack('<II', 0, len(data)) + name.ljust(64, b'\0') + bytes(0x94 - 8 - 64)
    path.write_bytes(data + entry + struct.pack('<II', 1, 1))


def ser(v):
    if isinstance(v, list):
        return struct.pack('<iI', 4, len(v) + 1) + b''.join(ser(x) for x in v)
    if isinstance(v, str):
        e = v.encode('latin-1')
        return struct.pack('<iI', 3, len(e)) + e
    if isinstance(v, float):
        return struct.pack('<if', 2, v)
    return struct.pack('<ii', 1, v)


def real_bytes():
    data = READER.read_bytes()
    s, n = next((s, n) for name, s, n in zrd.entries(data) if name == 'weapons.zrd')
    return data[s:s + n]


# --- variants -----------------------------------------------------------------
def kv(lst):
    return {lst[i]: i for i in range(0, len(lst) - 1, 2) if isinstance(lst[i], str)}


def put(lst, key, value):
    k = kv(lst)
    if key in k:
        lst[k[key] + 1] = value
    else:
        lst += [key, value]


def drop(lst, key):
    k = kv(lst)
    if key in k:
        del lst[k[key]:k[key] + 2]


def f(rng, lo, hi):
    return struct.unpack('<f', struct.pack('<f', rng.uniform(lo, hi)))[0]


def effect(rng):
    e = []
    r = rng.random()
    if r < 0.3:
        e += ['EFFECT', [rng.choice(['fx_a', 'fx_b', 'puff'])] if rng.random() < 0.8 else []]
    elif r < 0.6:
        e += ['MODEL', [rng.choice(['m1.flt', 'm2.flt', 'laser_red.flt'])] if rng.random() < 0.8 else []]
    for k in ('ANIMATION_ATTACHED', 'ANIMATION_MIN_RANGE', 'MODEL_ANIMATION', 'SURFACE_ANIMATION'):
        if rng.random() < 0.25:
            e += [k, [rng.choice(['an1', 'an2', 'an3'])] if rng.random() < 0.85 else []]
    if rng.random() < 0.6:
        a = rng.choice([[], ['anim_x'], ['anim_y', rng.choice([0, 1, 2, 3])], ['anim_z', 1, 'extra']])
        e += ['ANIMATION', a]
    if rng.random() < 0.3:
        e += ['RANDOM_ROTATE', [rng.choice([0, 1, 2, 3])]]
    if rng.random() < 0.5:
        e += ['SOUND', [rng.choice(['s1', 's2', 's3', 's4', 's5']) for _ in range(rng.randrange(0, 5))]]
    if rng.random() < 0.3:
        e += ['BOUNCE_SOUND', [rng.choice(['b1', 'b2', 'b3']) for _ in range(rng.randrange(0, 5))]]
    return e


BOOLS = ['CATCHES_FIRE', 'DETONATE_AT_RANGE', 'DETONATE_ON_WATER', 'RANDOM_DEVIATION', 'EXPIRES', 'FIXED_ROTATE',
         'HIT_OWNER', 'INSTANT', 'MINE', 'MULTI_TARGET', 'RELATIVE_SPEED', 'REMOTE_DETONATE', 'TETHER_GUIDED',
         'RELOAD', 'SHOW_ON_RADAR']


def mutate_def(rng, d, names):
    """Changes one definition's list in place."""
    for _ in range(rng.randrange(1, 8)):
        r = rng.randrange(30)
        if r == 0 and len(d) > 2:
            i = rng.randrange(0, len(d) - 1, 2)
            if d[i] != 'RANGE':
                del d[i:i + 2]
        elif r == 1:
            put(d, rng.choice(BOOLS), [rng.choice([0, 1, 2, 3, -1])])
        elif r == 2:
            put(d, 'GRAVITY', [rng.choice([0.0, f(rng, -30, -1), f(rng, 1, 40)])])
        elif r == 3:
            beam = ['TIME_TO_MAX_RANGE', [rng.choice([0.0, -1.0, 0.0001, f(rng, 0.01, 2)])]]
            if rng.random() < 0.5:
                beam += ['PULSE', rng.choice([[f(rng, -1, 4), 0.5, 0.25], [0.0, 0.1, 0.2], [2.0, 0.3], [1.5]])]
            for k in ('BOUNCES', 'TIMED_DAMAGE'):
                if rng.random() < 0.4:
                    beam += [k, [rng.choice([0, 1, 3])]]
            if rng.random() < 0.4:
                beam += ['ENERGY', [f(rng, 0, 5)]]
            if rng.random() < 0.5:
                beam += ['DAMAGE_AT_MAX_RANGE', [f(rng, 0, 1)]]
            put(d, 'BEAM', beam)
        elif r == 4:
            c = []
            if rng.random() < 0.8:
                c += ['points', [rng.randrange(1, 9), rng.randrange(4, 20)] if rng.random() < 0.8 else [3]]
            if rng.random() < 0.8:
                c += ['radius', [f(rng, 0.5, 3), f(rng, 3, 9)]]
            if rng.random() < 0.8:
                c += ['depth', [f(rng, 0.1, 1), f(rng, 1, 4)]]
            put(d, rng.choice(['CRATER', 'QUICKSAND', 'QUICKSAND']), c)
        elif r == 5:
            put(d, 'LOCK_ON_LEAD', rng.choice([[], [f(rng, 0, 2)], [f(rng, 0, 2), f(rng, 0, 3)], [5, 9], [0.5, f(rng, 0, 3), 1.0], [2.0, 1.0, 0.0], [1.0, 1.0, 3],
                                                [2, 1.5], [3.0, 1.0]]))
        elif r == 6:
            put(d, 'RANGE_MINIMUM', [f(rng, 0, 100), f(rng, 0, 1)])
        elif r == 7:
            put(d, rng.choice(['DETONATION_DISTANCE', 'IMPACT_PROXIMITY', 'LOCK_ON', 'TURN_RATE', 'PITCH_RATE',
                               'TURN_SUSPEND_TIME', 'ACCELERATION', 'AMMO_LIMIT', 'DAMAGE']), [f(rng, 0, 50)])
        elif r == 8:
            put(d, 'IMPACT_TYPE', [rng.randrange(-2, 6)])
        elif r == 9:
            put(d, 'FLYOUT_HEALTH', [rng.randrange(0, 200)])
        elif r == 10:
            put(d, 'NAME', [rng.choice(names + ['Unknown'])])
        elif r == 11:
            put(d, 'MILITARY_NAME', ['MSG_' + rng.choice(['A', 'B', 'C'])])
        elif r == 12:
            put(d, rng.choice(['FIRE', 'FLYOUT']), effect(rng))
        elif r == 13:
            imp = d[kv(d)['IMPACT'] + 1] if 'IMPACT' in kv(d) else []
            imp = list(imp)
            for _ in range(rng.randrange(1, 4)):
                q = rng.randrange(9)
                if q < 3:
                    put(imp, rng.choice(['default', 'water', 'seafloor', 'quicksand', 'lava', 'fire']), effect(rng))
                elif q == 3:
                    put(imp, rng.choice(['FREEZE', 'HEAT', 'DESIGNATE']),
                        [f(rng, 0, 2) for _ in range(rng.choice([6, 6, 5]))])
                elif q == 4:
                    put(imp, 'ANIMATION_ALWAYS', [1])
                elif q == 5:
                    put(imp, rng.choice(['KILL_ANIMATION', 'DAMAGE_ANIMATION']), [rng.choice(['ka', 'kb'])])
                elif q == 6:
                    put(imp, 'DAMAGE_ANIM_ON_HEALTH',
                        [[f(rng, 0, 1), rng.choice(['h1', 'h2'])] for _ in range(rng.randrange(0, 5))])
                elif q == 7:
                    drop(imp, rng.choice(['default', 'water']))
            put(d, 'IMPACT', imp)
        elif r == 14:
            drop(d, 'IMPACT')
        elif r == 15:
            s = []
            for k in ('count', 'truecount'):
                if rng.random() < 0.7:
                    s += [k, [rng.randrange(0, 8)]]
            for k in ('delta', 'radius', 'delay'):
                if rng.random() < 0.6:
                    s += [k, [rng.choice([0.0, f(rng, 0.05, 20)])]]
            put(d, 'SALVO', s)
        elif r == 16:
            m = []
            for k in ('clan', 'sphere'):
                if rng.random() < 0.7:
                    m += [k, [f(rng, 0.5, 15), rng.choice([1.0, 2.0, 2.7, 3.0])]]
            if rng.random() < 0.7:
                m += ['ammo', [rng.randrange(0, 60)]]
            put(d, 'MASS', m)
        elif r == 17:
            b = []
            if rng.random() < 0.9:
                b += ['wep', [rng.choice(names + ['nosuch'])]]
            b += ['count', [rng.randrange(0, 6)], 'radius', [f(rng, 1, 9)]]
            put(d, 'BURST', b)
        elif r == 18:
            put(d, 'ZAPS_HUD', rng.choice([[1], [f(rng, 0, 3)], [2, f(rng, 0, 3)], [f(rng, 0, 2), 3], ['x'], []]))
        elif r == 19:
            put(d, 'SHOW_ON_RADAR', rng.choice([[1], [0], [1.0], [], 3]))
        elif r == 20:
            put(d, 'ISNARC', [rng.choice([0, 1])])
        elif r == 21:
            put(d, rng.choice(['U_RECHARGE_TIME', 'U_FIRE_TIME', 'IMPACT_FORCE', 'RECOIL']), [f(rng, 0.1, 6)])
        elif r == 22:
            put(d, 'U_DESIGNATE_AIRSTRIKE', [rng.choice(names + ['nosuch'])])
        elif r == 23:
            put(d, rng.choice(['U_LOOPED', 'U_RECHARGE_SND']), [rng.choice(['loop1', 'rch'])])
        elif r == 24:
            put(d, 'NAMESOUND', [rng.choice(['ns1', 'ns2'])])
        elif r == 25:
            drop(d, rng.choice(['DESC', 'HEAT', 'FIRE_RATE', 'VELOCITY', 'MASS', 'SALVO', 'BEAM', 'FLYOUT', 'FIRE']))
        elif r == 26:
            put(d, 'VELOCITY', [rng.choice([50.0, 99.0, 100.0, 6000.0, f(rng, 100, 5000)])])
        elif r == 27:
            put(d, 'FIRE_RATE', [rng.choice([0.25, 0.5, 4.0, f(rng, 0.1, 10)])])
        elif r == 28:
            put(d, 'RANGE', [f(rng, 10, 3000)])
        else:
            put(d, 'HEAT', [f(rng, 0, 0.5), f(rng, 0, 0.1)])


def variant(rng, tree):
    import copy
    t = copy.deepcopy(tree)
    top = t[0]
    bal = top[kv(top)['BALLISTICS'] + 1]
    names = [bal[i] for i in range(0, len(bal), 2)]
    r = rng.random()
    if r < 0.02:
        top[kv(top)['VERSION'] + 1] = [rng.choice([1, 3])]
    elif r < 0.03:
        drop(top, 'VERSION')
    elif r < 0.04:
        drop(top, 'BALLISTICS')
    if rng.random() < 0.3:
        for k in ('LOCK_ON_WARNING', 'NO_AMMO_WARNING', 'TRIGGER_INACTIVE', 'WEAPON_INACTIVE'):
            if rng.random() < 0.5:
                top += [k, [k.lower()]]
    if rng.random() < 0.3:
        top += ['MAX_CRATER_RADIUS', [f(rng, 5, 60)]]
    if 'BALLISTICS' in kv(top):
        for _ in range(rng.randrange(1, 12)):
            i = rng.randrange(0, len(bal), 2)
            mutate_def(rng, bal[i + 1], names)
        if rng.random() < 0.2:
            i = rng.randrange(0, len(bal), 2)
            del bal[i:i + 2]
        if rng.random() < 0.15:  # a new def, first
            d = list(bal[rng.randrange(0, len(bal), 2) + 1])
            bal[0:0] = ['Extra', d]
        if rng.random() < 0.3:
            keep = sorted(rng.sample(range(0, len(bal), 2), rng.randrange(1, min(10, len(bal) // 2) + 1)))
            nb = []
            for i in keep:
                nb += bal[i:i + 2]
            bal[:] = nb
    return t


def compare(emu, data, crater, hardware, tick_ok, label, lab=True):
    WORK.mkdir(parents=True, exist_ok=True)
    arch = WORK / 'w.zbd'
    write_archive(arch, data)
    a_rc = emu.run(data, crater, hardware, tick_ok)
    a_ev = emu.events
    buf = (ctypes.c_int32 * 400000)()
    n = ctypes.c_int()
    b_rc = probe.probe_zwep_run(str(arch).encode(), (ctypes.c_uint32 * 10)(*crater), hardware, tick_ok, buf,
                                400000, ctypes.byref(n))
    b_ev = port_events(buf, n.value)
    if a_rc != b_rc or a_ev != b_ev:
        first = next((i for i, (x, y) in enumerate(zip(a_ev, b_ev)) if x != y), min(len(a_ev), len(b_ev)))
        raise SystemExit(f'{label}: result {a_rc} vs {b_rc}; {len(a_ev)} vs {len(b_ev)} calls, first difference '
                         f'at {first}:\n  original {a_ev[first:first + 4]}\n  port     {b_ev[first:first + 4]}')
    if a_rc != 0:
        return a_rc, 0, a_ev
    ga = emu.globals()
    gb = (ctypes.c_uint32 * 11)()
    probe.probe_zwep_globals(gb)
    if ga != list(gb):
        raise SystemExit(f'{label}: globals {ga} vs {list(gb)}')
    for i, (img, imp, t, strings) in enumerate(emu.defs()):
        d, im, ty = (ctypes.c_uint8 * 0x1f4)(), (ctypes.c_uint8 * (6 * 0x5c))(), (ctypes.c_uint8 * 0xc8)()
        text = ctypes.create_string_buffer(4096)
        k = probe.probe_zwep_def(i, d, im, ty, text, 4096)
        bs = text.raw[:k].split(b'\0')[:4]
        for what, x, y in (('def', img, bytes(d)), ('impacts', imp, bytes(im)), ('type', t, bytes(ty))):
            if x != y:
                offs = [o for o in range(0, len(x), 4) if x[o:o + 4] != y[o:o + 4]]
                raise SystemExit(f'{label}: def {i} ({strings[0]!r}) {what} differs at ' + ', '.join(
                    f'+{o:#x} {x[o:o + 4].hex()} vs {y[o:o + 4].hex()}' for o in offs[:8]))
        if strings != bs:
            raise SystemExit(f'{label}: def {i} strings {strings} vs {bs}')
    for w in range(37 if lab else 0):
        fa, ia = emu.lab(w)
        fb, ib = (ctypes.c_float * 3)(), (ctypes.c_int32 * 2)()
        probe.probe_zwep_lab(w, fb, ib)
        if [bytes(x) for x in fa] != [struct.pack('<f', v) for v in fb] or ia != list(ib):
            raise SystemExit(f'{label}: mech lab for weapon {w}: {fa} {ia} vs {list(fb)} {list(ib)}')
    probe.probe_zwep_shutdown()
    return 0, ga[0], a_ev


def main():
    emu = Emulated()
    rng = random.Random(1999)
    real = real_bytes()
    tree, _ = zrd.node(real, 0)
    assert ser(tree) == real
    crater = [rng.randrange(1 << 32) for _ in range(10)]
    rc, ndefs, ev = compare(emu, real, crater, 1, 1, 'weapons.zrd')
    assert rc == 0 and ndefs == 49, (rc, ndefs)
    real_calls = len(ev)
    compare(emu, real, crater, 0, 0, 'weapons.zrd, software, no tick')
    variants = int(os.environ.get('WEAPONS_VARIANTS', '300'))
    defs = calls = fails = 0
    for v in range(variants):
        t = variant(rng, tree)
        crater = [rng.randrange(1 << 32) for _ in range(10)]
        rc, nd, ev = compare(emu, ser(t), crater, int(rng.random() < 0.7), int(rng.random() < 0.95), f'variant {v}', lab=v % 25 == 0)
        defs += nd
        calls += len(ev)
        fails += rc != 0
    missing = sorted(COVER[a] for a in set(COVER) - emu.reached)
    if missing:
        raise SystemExit(f'coverage: never reached {missing}')
    print(f'ok: weapons.zrd (49 defs, {real_calls} calls) and {variants} variants ({defs} defs, {calls} calls, '
          f'{fails} rejected) agree byte for byte, with the mech lab view of all 37 weapons; '
          f'all {len(COVER)} paths reached')


if __name__ == '__main__':
    main()
