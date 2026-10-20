"""The player's view and its cameras against the original (docs/view.md).

Each run builds a view, a mech (its centroid and torso nodes, controls,
pilot) and up to four camera modes of the four classes with random fields in
the emulator and in the rebuild (probe_view.cpp), then plays random calls on
them: the view's update 0x40ada0 over frames (each mode's update, the ground
and water check 0x40af30, the weather check 0x40b9a0), attach 0x40adf0, the
mode switch 0x40ae70, the free look (0x421c00, 0x4220c0, the base's), reset
0x40ad90 and the camera key 0x40aed0. Everything outside the family is
stubbed and answered from one script; the camera set-up 0x430e70 runs on
stubbed reader calls. Compared: the call traces (the camera's position and
angles each frame among them), the view's fields and every mode's, at the
24-bit x87 precision of play.

    cmake --build build && .venv/bin/python tests/differential/test_view.py [runs]
"""
import ctypes
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import Emu, Replay, Trace, compare, load_probe, ret_pop, rnd  # noqa: E402
from original import FTOL, IAT_FTOL  # noqa: E402,F401

VIEW_VT = 0x5bc2e8
MODE_VT = {0: 0x5bc2c0, 1: 0x5bd2f8, 2: 0x5be9f0, 3: 0x5be9c8}
LOOK_FN = {0: 0x40bac0, 1: 0x421c00, 2: 0x421c00, 3: 0x4220c0}


def fbits(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


def bitsf(u):
    return struct.unpack('<f', struct.pack('<I', u & 0xffffffff))[0]


def qnorm(q):
    n = sum(x * x for x in q) ** 0.5 or 1.0
    return [x / n for x in q]


class Script(dict):
    def __init__(self, f):
        super().__init__()
        self.f = f

    def get(self, key, default=None):
        return self.f(key)

    def items(self):
        return []


class World:
    def __init__(self, seed):
        self.seed = seed
        t = self.t = Trace()
        e = self.e = Emu(t)
        e.cpu.fpu_control(0x007f)
        e.heap_stubs()
        from test_scenewalk import ASIN, COPYSIGN, FLOOR, IAT_ASIN, IAT_COPYSIGN, IAT_FLOOR
        e.cpu.import_code(IAT_FLOOR, FLOOR)
        e.cpu.import_code(IAT_ASIN, ASIN)
        e.cpu.import_code(IAT_COPYSIGN, COPYSIGN)
        r = self.r = rnd(seed, 'world')
        A = e.world_alloc
        self.view = A(0x80)
        self.cam, self.camdata, self.world, self.world_node = A(0x60), A(0x200), A(0x10), A(0x10)
        self.parent = A(0x10)
        self.mech, self.mech_vt = A(0x700), A(0x200)
        self.centroid, self.cdata = A(0x60), A(0x100)
        self.torso, self.torso_node = A(0x180), A(0x10)
        self.controls, self.pilot, self.game, self.pilot_global = A(0x20), A(0x10), A(0x100), A(0x10)
        self.aim, self.entity, self.option = A(0x10), A(0x10), A(0x10)
        self.weather, self.weather_vt = (A(0x20), A(0x80)) if r.random() < 0.8 else (0, 0)
        self.scratch = A(0x10)
        self.modes = []
        self.found = 0
        self.stubs()

    # ------------------------------------------------------------------ stubs
    def fret(self, key, pop, value):
        """A stub returning a float on the x87 stack."""
        e = self.e
        e.cpu.write(self.scratch, struct.pack('<I', value))
        thunk = self.thunks.get(pop)
        if thunk is None:
            thunk = self.thunks[pop] = e.cpu.code(b'\xd9\x05' + struct.pack('<I', self.scratch) + b'\xc2' +
                                                  struct.pack('<H', pop))
        e._ret(0, pop, then=thunk)

    def stubs(self):
        e, t = self.e, self.t
        self.thunks = {}
        for addr, spec in ((0x4f3870, 'c,s0,s1,s2'), (0x4f36b0, 'c,s0,s1,s2'), (0x4f3920, 'c,s0,s1,s2'),
                           (0x4ed280, 'c,d'), (0x51e9d0, ''), (0x51fde0, ''), (0x51ea00, ''),
                           (0x4ebb40, 'c,d'), (0x4eb9b0, 'c,d'), (0x4eb6d0, 'c,d'), (0x4ed9c0, 'c'),
                           (0x41ce10, 'c,s0'), (0x41ca30, 'c,s0'), (0x41ca60, 'c,s0'),
                           (0x464ac0, 'c,s0'), (0x465a00, 'c,s0'), (0x466190, 'c,s0'), (0x426780, 'c,s0'),
                           (0x502da0, 'c'), (0x430aa0, 'c'), (0x446240, 'c'), (0x479480, ''), (0x40ce10, 'c'),
                           (0x4f2440, 'c,s0,s1'), (0x4f23f0, 'c,s0,s1,s2'), (0x4f2420, 'c,s0,s1'),
                           (0x5a80d0, 'c'), (0x502d70, ''), (0x5569e0, 'c,zd')):
            e.stub(addr, spec=spec)

        def ws(k, a, p):  # 0x4ed530: ECX node, EDX out
            node, out = e.uc.reg_read(UC_ECX), e.uc.reg_read(UC_EDX)
            e.t.handle(k, [node])
            src = self.camera_world_pos if node == self.cam else self.torso_node_pos
            e.cpu.write(out, struct.pack('<3f', *src))
            e._ret(1, p)
        e.stub(0x4ed530, fn=lambda em, k, a, p: ws(k, a, p))

        def seg(k, a, p):  # 0x4eed90: ECX world, EDX results; a, b
            out = e.uc.reg_read(UC_EDX)
            args = [e.r32(e.uc.reg_read(UC_ESP) + 4 + 4 * i) for i in range(6)]
            r = e.t.handle(k, args)
            e.w8(out, e.t.handle('@found', []) & 0xff)
            e._ret(r, p)
        e.stub(0x4eed90, fn=lambda em, k, a, p: seg(k, a, p))

        def ground(k, a, p):  # 0x4edab0: x, y, z
            args = [e.r32(e.uc.reg_read(UC_ESP) + 4 + 4 * i) for i in range(3)]
            e._ret(e.t.handle(k, args), p)
        e.stub(0x4edab0, fn=lambda em, k, a, p: ground(k, a, p))

        def water(k, a, p):  # 0x4abd90: y, height, &index, flag, &kind, &level
            sp = e.uc.reg_read(UC_ESP)
            s = [e.r32(sp + 4 + 4 * i) for i in range(6)]
            v = e.t.handle(k, [s[0], s[1], s[3] & 0xff])
            idx = e.t.handle('@index', [])
            self.index = idx
            e.w32(s[2], idx)
            e.w32(s[4], e.t.handle('@kind', []))
            e.w32(s[5], e.t.handle('@level', []))
            self.fret(k, p, v)
        e.stub(0x4abd90, fn=lambda em, k, a, p: water(k, a, p))

        def zone(k, a, p):  # 0x4f6c80: ECX camera, EDX the hit
            e.t.handle(k, [e.uc.reg_read(UC_ECX), self.index])
            e._ret(0, p)
        e.stub(0x4f6c80, fn=lambda em, k, a, p: zone(k, a, p))

        def entity(k, a, p):
            e.t.handle(k, [e.uc.reg_read(UC_ECX)])
            e._ret(self.entity, p)
        e.stub(0x5a85e0, fn=lambda em, k, a, p: entity(k, a, p))

        def aim(k, a, p):
            e.t.handle(k, [e.uc.reg_read(UC_ECX)])
            e._ret(self.aim, p)
        e.stub(0x41ce00, fn=lambda em, k, a, p: aim(k, a, p))

        for addr in (0x465900, 0x4658c0):
            e.stub(addr, fn=lambda em, k, a, p: self.fret(k, p, e.t.handle(k, [e.uc.reg_read(UC_ECX)])))

        def pull(k, a, p):  # 0x40b310: ECX pos, EDX at; back
            c, d = e.uc.reg_read(UC_ECX), e.uc.reg_read(UC_EDX)
            b = e.r32(e.uc.reg_read(UC_ESP) + 4)
            vals = [e.r32(x + 4 * i) for x in (c, d, b) for i in range(3)]
            e.t.handle(k, vals)
            e._ret(0, p)
        e.stub(0x40b310, fn=lambda em, k, a, p: pull(k, a, p))

        def fl(k, a, p):  # 0x00556a60: ECX node, EDX key; out
            node, key = e.uc.reg_read(UC_ECX), e.cstr(e.uc.reg_read(UC_EDX))
            out = e.r32(e.uc.reg_read(UC_ESP) + 4)
            r = e.t.handle(k, [node, key])
            if r:
                e.w32(out, e.t.handle('@float', []))
            e._ret(r, p)
        e.stub(0x556a60, fn=lambda em, k, a, p: fl(k, a, p))

        def v3(k, a, p):  # 0x005570b0
            node, key = e.uc.reg_read(UC_ECX), e.cstr(e.uc.reg_read(UC_EDX))
            out = e.r32(e.uc.reg_read(UC_ESP) + 4)
            r = e.t.handle(k, [node, key])
            if r:
                for i in range(3):
                    e.w32(out + 4 * i, e.t.handle('@float', []))
            e._ret(r, p)
        e.stub(0x5570b0, fn=lambda em, k, a, p: v3(k, a, p))

        def dfn(k, a, p):  # 0x004846a0: ECX roster; the definition, its reader at +8
            e.t.handle(k, [e.uc.reg_read(UC_ECX)])
            d = e.world_alloc(0x20)
            e.w32(d + 8, 0x2b000100)
            e._ret(d, p)
        e.stub(0x4846a0, fn=lambda em, k, a, p: dfn(k, a, p))

        # The mech's vtable: +0x50 returns itself (the exe's 0x004d2a70).
        e.w32(self.mech, self.mech_vt)
        e.w32(self.mech_vt + 0x50, 0x4d2a70)
        for off, spec, pop in ((0xa0, 'c', 0), (0x100, 'c', 0), (0x104, 'c', 0)):
            e.w32(self.mech_vt + off, e.new_stub(f'mech.vt+{off:#x}', spec, pop))
        e.w32(self.mech_vt + 0x10c, e.new_stub('mech.vt+0x10c', 'c,s0', 4))
        if self.weather:
            e.w32(self.weather, self.weather_vt)
            e.w32(self.weather_vt + 0x64, e.new_stub('weather.vt+0x64', '', 0))
            e.w32(self.weather_vt + 0x60, e.new_stub('weather.vt+0x60', 's0', 4))

    # ------------------------------------------------------------------ world
    def build(self):
        e, r = self.e, self.r
        rf = lambda lo, hi: r.uniform(lo, hi)  # noqa: E731
        self.centroid_pos = [rf(-300, 300), rf(0, 40), rf(-300, 300)]
        self.torso_node_pos = [self.centroid_pos[0] + rf(-1, 1), self.centroid_pos[1] + rf(3, 8),
                               self.centroid_pos[2] + rf(-1, 1)]
        self.camera_world_pos = [rf(-300, 300), rf(-5, 40), rf(-300, 300)]
        self.camera_local_pos = [rf(-300, 300), rf(-5, 40), rf(-300, 300)]
        self.entity_pos = [self.centroid_pos[0], rf(-2, 30), self.centroid_pos[2]]
        q = qnorm([rf(-1, 1) for _ in range(4)]) if r.random() < 0.7 else [1.0, 0.0, 0.0, 0.0]
        aimq = qnorm([1.0, rf(-0.3, 0.3), rf(-0.3, 0.3), 0.0]) if r.random() < 0.7 else [1.0, 0.0, 0.0, 0.0]
        fog = [[rf(10, 900) for _ in range(7)] for _ in range(2)]
        cflags = 0x20 if r.random() < 0.3 else 0
        u = [self.mech, self.torso, self.torso_node, self.centroid, self.controls, self.pilot, self.cam,
             self.world_node, self.parent, 0, cflags, r.choice([0, 4, 5]), r.choice([0, 1, 2]),
             r.getrandbits(8), r.choice([1, 1, 0]), r.choice([1, 0]), self.weather,
             r.choice([0, 0x10, 0x11]), r.choice([0, 0, 1]), r.choice([1, 1, 0])]
        (self.mech_, self.torso_, self.tnode_, self.cent_, self.ctl_, self.pil_, self.cam_, self.wnode_, self.par_, _,
         cflags, self.ctl_flags, self.ctl_kind, self.option_v, self.head_bob, self.water_flag, _, self.wflags,
         self.g8e, self.has_view) = u
        f = (self.centroid_pos + self.torso_node_pos + self.camera_world_pos + self.camera_local_pos +
             self.entity_pos + aimq + fog[0] + fog[1] + [r.choice([0.15, 0.3, 0.0]), r.choice([1.0472, 0.9, 1.4])])
        self.u, self.f = u, f
        # The emulator's memory.
        w = e.w32
        w(0x5f4c8c, self.view)
        w(0x608c84, self.world)
        w(self.world + 4, self.world_node)
        w(self.cam + 0x38, self.camdata)
        e.cpu.write(self.camdata + 0x14, struct.pack('<3f', *self.camera_local_pos))
        e.cpu.write(self.camdata + 0xec, struct.pack('<f', f[-1]))
        w(self.cam + 0x54, 1)
        parents = e.world_alloc(8)
        w(self.cam + 0x58, parents)
        w(parents, self.parent)
        w(self.mech + 0x14, self.centroid)
        w(self.centroid + 0x38, self.cdata)
        w(self.cdata, cflags)
        e.cpu.write(self.cdata + 0x84, struct.pack('<3f', *self.centroid_pos))
        w(self.mech + 0x4, self.mech + 0x1000)
        w(self.mech + 0x39c, self.torso)
        w(self.torso + 0x8, self.torso_node)
        w(self.mech + 0x37c, self.controls)
        w(self.controls + 0x8, self.ctl_flags)
        w(self.controls + 0xc, self.ctl_kind)
        w(self.mech + 0x684, self.pilot)
        w(0x5f4d44, self.option)
        w(self.option, self.option_v)
        e.w8(0x5e2448, self.head_bob)
        e.cpu.write(0x5e244c, struct.pack('<f', f[-2]))
        w(0x5e06f0, self.water_flag)
        w(0x6098d0, self.weather)
        if self.weather:
            w(self.weather + 0xc, self.wflags)
        w(0x60861c, self.pilot_global)
        e.w8(self.game + 0x8e, self.g8e)
        e.cpu.write(self.aim, struct.pack('<4f', *aimq))
        e.cpu.write(self.entity, struct.pack('<3f', *self.entity_pos))
        # Missions' fog: air +0x0c range, +0x14 altitude, +0x1c colour; water +0x30.. (0x006098d0 + ...).
        a, wt = fog
        e.cpu.write(0x6098dc, struct.pack('<7f', *a))
        e.cpu.write(0x609900, struct.pack('<7f', *wt))
        if not self.has_view:
            pass
        self.mech_values(q)

    def mech_values(self, q):
        e, r = self.e, self.r
        rf = lambda lo, hi: r.uniform(lo, hi)  # noqa: E731
        vel = [rf(-8, 8), rf(-2, 2), rf(-8, 8)]
        v3c = [rf(-1, 1), rf(-1, 1), rf(-20, 20)]
        v58 = [rf(-0.3, 0.3) for _ in range(3)]
        height = rf(8, 14)
        vq = qnorm([1.0, rf(-0.4, 0.4), rf(-0.4, 0.4), 0.0])
        self.active = int(r.random() < 0.85)
        self.anim_state = r.choice([0, 1, 12, 13, 10, 2])
        self.view_aim = int(r.random() < 0.2)
        e.w8(self.mech + 0x8, self.active)
        e.cpu.write(self.mech + 0x30, struct.pack('<3f', *vel))
        e.cpu.write(self.mech + 0x3c, struct.pack('<3f', *v3c))
        e.cpu.write(self.mech + 0x48, struct.pack('<4f', *q))
        e.cpu.write(self.mech + 0x58, struct.pack('<3f', *v58))
        e.cpu.write(self.mech + 0x68, struct.pack('<f', height))
        e.w32(self.mech + 0x578, self.anim_state)
        e.w8(self.torso + 0xa2, self.view_aim)
        e.cpu.write(self.torso + 0xa4, struct.pack('<4f', *vq))
        self.mech_f = vel + v3c + q + v58 + [height] + vq

    def add_mode(self, kind, idn):
        e, r = self.e, self.r
        a = e.world_alloc(0x78)
        e.w32(a, MODE_VT[kind])
        rf = lambda lo, hi: r.uniform(lo, hi)  # noqa: E731
        dq = qnorm([1.0, rf(-0.2, 0.2), 0.0, 0.0]) if r.random() < 0.5 else [1.0, 0.0, 0.0, 0.0]
        dist = r.choice([0.0, 0.0, 12.0, 14.0, 40.0, 150.0, rf(1, 50)]) if kind != 2 else 0.0
        # The cockpit at distance 0 (its offset is unset otherwise, FIX (bug); the files keep it 0).
        zero = kind == 2
        fields = dq + [rf(-1, 1), rf(-1, 2), rf(-1, 1)] + [0.0 if zero else r.choice([0.0, 1.0, rf(0, 2)]), dist,
                                                           0.0 if zero else r.choice([0.0, 0.1, rf(0, 1)]),
                                                           0.0 if zero else r.choice([0.0, 0.5, 0.2]),
                                                           r.choice([0.4, 1.0, 0.2]), r.choice([2.0, 1.0]),
                                                           r.choice([3.0, 4.0])]
        raw = [idn] + [fbits(x) for x in fields] + [r.choice([2, 0, 1])]
        lq = qnorm([1.0, rf(-0.5, 0.5), rf(-0.5, 0.5), 0.0]) if r.random() < 0.3 else [1.0, 0.0, 0.0, 0.0]
        raw += [fbits(x) for x in lq] + [r.choice([0, 1])] + [fbits(dist), fbits(rf(0, 10))] + [0] + \
            [r.choice([0, 0, 1]) if kind == 2 else 0] + [fbits(x) for x in (1.5533430, -1.5533430, -0.2617994,
                                                                             0.7853982)]
        raw = raw[:29]
        e.cpu.write(a + 4, struct.pack('<29I', *[x & 0xffffffff for x in raw]))
        if kind != 0:
            e.w8(a + 0x54, raw[0x14])
            e.w32(a + 0x60, self.view)
            e.w8(a + 0x64, raw[0x18])
            for i in (0x14, 0x18):
                pass
        self.modes.append((a, kind, idn, raw))
        return a


def mode_raw_emu(w, a, kind):
    e = w.e
    raw = list(struct.unpack('<29I', bytes(e.cpu.read(a + 4, 0x74))))
    if kind == 0:
        raw[0x10:] = [0] * (29 - 0x10)
    else:
        raw[0x14] = e.r8(a + 0x54)
        raw[0x17] = 1 if e.r32(a + 0x60) else 0
        raw[0x18] = e.r8(a + 0x64)
    return raw + [0]


def view_raw_emu(w):
    raw = list(struct.unpack('<31I', bytes(w.e.cpu.read(w.view, 31 * 4))))
    out = [0] * 31
    for i in list(range(6, 26)) + [30]:
        out[i] = raw[i]
    out[26] = raw[26] & 0xffffff
    out[25] = w.mech if raw[25] else 0
    return out


from unicorn.x86_const import UC_X86_REG_ECX as UC_ECX, UC_X86_REG_EDX as UC_EDX, UC_X86_REG_ESP as UC_ESP  # noqa: E402


def answers(seed, w):
    def answer(key):
        def f(n, a):
            r = rnd(seed, f'{key}#{n}')
            if key == 'mech.vt+0x10c':
                ids = {m[2]: m[0] for m in w.modes}
                return ids.get(a[1], 0) if r.random() < 0.9 else 0
            if key in ('0x00465900', '0x004658c0'):
                return fbits(r.choice([0.0, 0.0, r.uniform(-1, 1)]))
            if key == '0x004abd90':
                return fbits(r.choice([w.camera_world_pos[1] + r.uniform(-3, 3), -50.0, r.uniform(-5, 30)]))
            if key == '@level':
                return fbits(r.choice([w.camera_world_pos[1] + r.uniform(-3, 3), r.uniform(-5, 30)]))
            if key == '@kind':
                return r.choice([0, 1, 1, 2])
            if key == '@index':
                return r.randrange(6)
            if key == '@found':
                return r.choice([0, 1, 2])
            if key in ('0x004eed90', '0x004edab0'):
                return r.choice([0, 0, 1])
            if key == '0x00479480':  # the pilot's game: the same one every call
                return w.game if rnd(seed, 'game').random() < 0.6 else 0
            if key == '0x00430aa0':
                return r.choice([0, 0x2a000000 + r.randrange(16) * 0x10])
            if key == 'mech.vt+0xa0':
                return 1 if r.random() < 0.1 else 0
            if key == '0x004846a0':
                return 0x2b000000
            if key == '0x005569e0':
                return 0 if r.random() < 0.1 else 0x2b100000 + r.randrange(0x100) * 0x10
            if key in ('0x00556a60', '0x005570b0'):
                return 1 if r.random() < 0.5 else 0
            if key == '@float':
                return fbits(r.choice([0.0, 1.0, r.uniform(-30, 30), r.uniform(0, 2)]))
            return r.randrange(4)
        return f
    return answer


def ecall(w, addr, **kw):
    from unicorn import UcError
    from unicorn.x86_const import UC_X86_REG_EIP
    if os.environ.get('VIEW_TRACE'):
        from unicorn import UC_HOOK_CODE
        hist = w.hist = getattr(w, 'hist', [])
        if not getattr(w, 'hooked', False):
            w.e.uc.hook_add(UC_HOOK_CODE, lambda uc, a, n, u: (hist.append(a), hist.__delitem__(0) if len(hist) > 40 else None))
            w.hooked = True
    try:
        return w.e.cpu.call(addr, **kw)
    except UcError as err:
        eip = w.e.uc.reg_read(UC_X86_REG_EIP)
        h = [hex(x) for x in getattr(w, 'hist', [])[-25:]]
        raise SystemExit(f'{addr:#x}: {err} at eip {eip:#x}, after {w.t.log[-3:]} {h}')


def run(probe, seed, verbose=False):
    w = World(seed)
    r = rnd(seed, 'run')
    w.build()
    w.t.script = Script(answers(seed, w))
    e = w.e
    probe.probe_view_world((ctypes.c_uint32 * len(w.u))(*w.u), (ctypes.c_float * len(w.f))(*w.f))
    probe.probe_view_mech((ctypes.c_float * len(w.mech_f))(*w.mech_f), w.active, w.anim_state, w.view_aim)
    probe.probe_view_new(0)
    kinds = r.sample([(0, 0), (1, 0), (2, 1), (1, 2), (1, 3), (3, 5), (1, 4)], r.randint(1, 4))
    for kind, idn in kinds:
        a = w.add_mode(kind, idn)
        raw = w.modes[-1][3]
        probe.probe_view_mode(a, kind, idn, (ctypes.c_uint32 * 30)(*([x & 0xffffffff for x in raw] + [0])))
    # The view's fields, written to both.
    rf = lambda lo, hi: r.uniform(lo, hi)  # noqa: E731
    vraw = [0] * 31
    e.w32(w.view, VIEW_VT)
    e.w32(w.view + 4, w.cam)
    e.w32(w.view + 0xc, w.world)  # zView's +0x0c (0x004b4fe0's object): +4 the world node the ground query asks
    vraw[6] = fbits(rf(0, 8))
    for base in (7, 11, 15):
        q = qnorm([1.0, rf(-0.2, 0.2), rf(-0.2, 0.2), 0.0]) if r.random() < 0.5 else [1.0, 0.0, 0.0, 0.0]
        vraw[base:base + 4] = [fbits(x) for x in q]
    vraw[19:22] = [fbits(x) for x in w.camera_local_pos]
    first = r.choice(w.modes)
    vraw[22] = first[0] if r.random() < 0.85 else 0
    vraw[23] = r.choice([1, 0, 2])
    vraw[24] = fbits(rf(0, 20))
    vraw[25] = w.mech if vraw[22] or r.random() < 0.9 else 0  # a mode always has a target (the game's invariant)
    vraw[26] = r.choice([0, 0x10000])
    vraw[30] = fbits(r.choice([0.033, 0.016, 0.1]))
    e.cpu.write(w.view + 0x18, struct.pack('<25I', *vraw[6:31]))
    probe.probe_view_set((ctypes.c_uint32 * 31)(*vraw))
    snap = e.snapshot()
    rep = Replay(w.t, e, snap)
    probe.probe_app_set_callback(rep.callback())
    name = f'seed {seed} (modes {kinds})'
    ok = True
    ops = []
    for step in range(r.randint(3, 25)):
        op = r.choices(['update', 'attach', 'mode', 'look', 'reset', 'cycle', 'cameras'],
                       [12, 1, 2, 2, 1, 1, 1])[0]
        e.cpu.fpu_control(0x007f)
        if op == 'update':
            dt = r.choice([0.033, 0.016, 0.1, 0.0, 0.25])
            ecall(w, 0x40ada0, ecx=w.view, stack_args=(fbits(dt),))
            pa, pb = 0x40ada0, (fbits(dt), 0)
        elif op == 'attach':
            m = r.choice(w.modes + [(0, 0, 0, 0)])[0]
            t = w.mech if r.random() < 0.85 else 0
            ecall(w, 0x40adf0, ecx=w.view, stack_args=(t, m))
            pa, pb = 0x40adf0, (m, int(t != 0))
        elif op == 'mode':
            idn = r.choice([m[2] for m in w.modes] + [7])
            ecall(w, 0x40ae70, ecx=w.view, stack_args=(idn,))
            pa, pb = 0x40ae70, (idn, 0)
        elif op == 'look':
            mode = e.r32(w.view + 0x58)
            if not mode:
                continue
            look = r.randrange(6)
            fn = e.r32(e.r32(mode) + 0x10)
            ecall(w, fn, ecx=mode, stack_args=(look,))
            pa, pb = 0x421c00, (look, 0)
        elif op == 'reset':
            ecall(w, 0x40ad90, ecx=w.view)
            pa, pb = 0x40ad90, (0, 0)
        elif op == 'cycle':
            ecall(w, 0x40aed0, ecx=w.view)
            pa, pb = 0x40aed0, (0, 0)
        else:
            roster = r.randrange(0x14)
            e.w32(0x5f4c8c, w.view if w.has_view else 0)
            pilot = e.world_alloc(0x30)
            e.w32(pilot + 0xc, roster)
            ecall(w, 0x430e70, ecx=pilot)
            e.w32(0x5f4c8c, w.view)
            pa, pb = 0x430e70, (roster, 0)
        probe.probe_view_call(pa, *pb)
        ops.append(op)
        if op == 'cameras':
            lst = e.r32(pilot + 0x24)
            b, en = e.r32(lst + 4), e.r32(lst + 8)
            emu_modes = [e.r32(b + 4 * i) for i in range((en - b) // 4)]
            kinds_ = (ctypes.c_int32 * 8)()
            raws = (ctypes.c_uint32 * (8 * 30))()
            n = probe.probe_view_cameras(kinds_, raws)
            kind_of = {0x5bc2c0: 0, 0x5bd2f8: 1, 0x5be9f0: 2, 0x5be9c8: 3}
            got = [(kinds_[i], list(raws[30 * i:30 * i + 30])) for i in range(n)]
            want = [(kind_of[e.r32(m)], mode_raw_emu(w, m, kind_of[e.r32(m)])) for m in emu_modes]
            for g in got:
                g[1][0x17] = 0
            for wnt in want:
                wnt[1][0x17] = 0
            if want != got:
                print(f'{name}: cameras differ (roster {roster})')
                for i, (x, y) in enumerate(zip(want, got)):
                    if x != y:
                        d = [(j, hex(a_), hex(b_)) for j, (a_, b_) in enumerate(zip(x[1], y[1])) if a_ != b_]
                        print(f'  camera {i}: kind {x[0]} vs {y[0]}: {d[:8]}')
                ok = False
                break
            continue
        vw = view_raw_emu(w)
        vp = (ctypes.c_uint32 * 31)()
        probe.probe_view_get(vp)
        if vw != list(vp):
            d = [(i, hex(a_), hex(b_)) for i, (a_, b_) in enumerate(zip(vw, vp)) if a_ != b_]
            print(f'{name}: after {ops}: view fields differ (word, original, rebuild): {d[:10]}')
            ok = False
            break
        for a, kind, idn, _ in w.modes:
            mw = mode_raw_emu(w, a, kind)
            mp = (ctypes.c_uint32 * 30)()
            probe.probe_view_mode_get(a, mp)
            if mw != list(mp):
                d = [(i, hex(a_), hex(b_)) for i, (a_, b_) in enumerate(zip(mw, mp)) if a_ != b_]
                print(f'{name}: after {ops}: mode {idn} (kind {kind}) fields differ: {d[:10]}')
                ok = False
                break
        if not ok:
            break
    # The camera's angles within one unit in the last place (asin and atan2, 0x00523bc0).
    ok = ok and compare(f'{name} {ops}', w.t.log, rep.log, ulps={'0x004f36b0': {1, 2, 3}})
    return ok, len(w.t.log), ops


def main():
    probe = load_probe()
    probe.probe_view_call.restype = ctypes.c_int32
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 300
    calls = 0
    count = {}
    first = int(os.environ.get('VIEW_FIRST', '0'))
    for seed in range(first, first + runs):
        if os.environ.get('VIEW_SEEDS'):
            print(f'seed {seed}', flush=True)
        ok, n, ops = run(probe, seed)
        if not ok:
            sys.stdout.flush()
            os._exit(1)
        calls += n
        for o in ops:
            count[o] = count.get(o, 0) + 1
    print(f'{runs} runs, {calls} calls, {count}: the rebuilt view and cameras match the original')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    try:
        main()
    except SystemExit as e:  # an emulator fault: print it, then leave without Unicorn's teardown
        print(e)
        sys.stdout.flush()
        os._exit(1)
