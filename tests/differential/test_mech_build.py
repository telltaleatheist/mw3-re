"""Building a mech against the original (docs/mech-build.md): the launch
0x491150 (the player's controls 0x427280 with 0x427320 and 0x426ce0, the mech
0x4140c0, its skin, teams, control mode 0x41cf30, the view, the cockpit's
cycles, the game type's line-up) and an AI mech's 0x414150, both down the
whole constructor chain: the bases 0x4aeb00 / 0x4d56e0 / 0x5a7e10 /
0x5a8310, the collider 0x4222e0, the weapon manager 0x4b00d0, the model
0x415b00 (0x430c60, 0x430c20, 0x41de80, 0x5a7f50), the parts 0x415e40 and
their constructors (0x45fbe0, 0x460190, 0x460050, hip, torso, head, arms and
hands, legs, calves, feet), the AMS, the firepoints 0x46cb80, the motions
and skis 0x415ca0 / 0x476320 / 0x476090, the configuration 0x414430 with the
parts' armor 0x45fe00 / 0x4644b0.

The original runs first with every call out of that chain stubbed and
answered from one script; the port then runs on a calltrace.Replay of it
(the same answers, `new` the same blocks), its fake objects read and written
through peek and poke. Compared: the call traces; every block the original
allocated, as the port's object of it laid out byte by byte (every byte the
original wrote must be a field the port sets to the same value, every other
byte left as the allocator's fill); every byte written to the fake objects
(the nodes' part ids and owners, the pilot, the interpreter, the missions'
list, the definitions' firepoints); the globals. The definitions are the
shipped dfn_*.zrd read by the ported reader (test_mechdef.py checks it), their
configurations the shipped loadouts and crafted ones; the node trees are the
shipped mechlib.zbd models. Launch-time code: the x87 at 24 bits.

    cmake --build build && .venv/bin/python tests/differential/test_mech_build.py [cases]
"""
import collections
import ctypes
import random
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import CALLBACK, ROOT, Emu, Replay, Trace, compare, load_probe  # noqa: E402
from test_mission_load import c_format  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EBP, UC_X86_REG_EIP, UC_X86_REG_ESI  # noqa: E402

LAUNCH, CREATE = 0x491150, 0x414150
MECHS = ['annihilator', 'avatar', 'blackhawk', 'bushwacker', 'cauldron', 'champion', 'daishi', 'elemental',
         'firefly', 'madcat', 'orion', 'owens', 'puma', 'shadowcat', 'strider', 'sunder', 'supernova', 'thor',
         'vulture']
GAME = ROOT / 'game' / 'mw3-1.2' / 'zbd'
FILL = 0xcd
SETUPS = (0x464290, 0x462130, 0x4663e0, 0x4637b0, 0x45fdd0)  # the parts' vt+0x04 (torso, hip, arm, leg, head)
P = ctypes.c_void_p
U = ctypes.c_uint32

CALLS = {
    0x491d20: 'c', 0x40d1b0: '', 0x40ce70: '', 0x40ce10: 'c', 0x51d2f0: 'c,d', 0x4d9840: 'c', 0x51b2c0: '',
    0x430d10: 'c,d', 0x4846a0: 'c', 0x4757f0: 'c,s0,s1', 0x51dc10: '', 0x40d010: '', 0x40ae70: 'c,s0',
    0x4f3430: 'c,d', 0x4108a0: 'c,s0', 0x430aa0: 'c', 0x595990: 'c,zs0', 0x5190d0: '', 0x49e5e0: '',
    0x478f10: 'c,s0', 0x49dc80: 'cl', 0x45c4a0: 'c,d,s0,s1', 0x46da80: 'c', 0x478ce0: 'c', 0x4794f0: 'c',
    0x4d6d20: 'c,s0', 0x43d920: 'c', 0x40f0a0: '', 0x537520: '',
    0x4f9af0: 'c,d,s0', 0x475a80: 'c,s0,s1', 0x5a7f50: 'c', 0x4eb450: 'c,d', 0x4f9d60: 'c,zd',
    0x4e6250: 'c', 0x4f7db0: 'c', 0x4fc0b0: 'c,s0,s1,s2', 0x53bb20: 'c,d', 0x4eb170: 'c,d', 0x4f7e50: '',
    0x4eb620: 'c,d', 0x5a8500: 'c', 0x4eb9b0: 'c,d', 0x5833b0: 'c,d,s0,s1', 0x4ebb40: 'c,d',
    0x431fb0: 'c,s0', 0x432030: 'c,s0', 0x41ff30: 'c,d', 0x4fb740: '', 0x4eb300: 'c,zd', 0x538fe0: '',
    0x4eb370: 'c,d', 0x4764a0: 'c,s0,s1,s2', 0x422360: 'c,s0', 0x4254d0: 'c,s0', 0x41ef90: 'c,s0',
    0x4329c0: 'c,s0,s1', 0x425960: 'c', 0x4f9050: 'c', 0x4b0730: 'c,s0', 0x4e61e0: 'c,s0,s1', 0x41ce30: 'c',
}


def fbits(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def setup_probe():
    probe = load_probe()
    probe.probe_app_set_callback.argtypes = [CALLBACK]
    probe.probe_mb_reset.argtypes = [U, U, U, U]
    probe.probe_mb_open.argtypes = [ctypes.c_char_p]
    probe.probe_mb_config_set.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, U]
    probe.probe_mb_config_set.restype = U
    probe.probe_mb_dfn_image.argtypes = [ctypes.c_int, U, P, ctypes.c_int, ctypes.c_int]
    probe.probe_mb_dfn_skins.argtypes = [ctypes.c_int, P]
    probe.probe_mb_dfn_skins.restype = P
    probe.probe_mb_pilot.argtypes = [U, ctypes.c_int, U, U, U, U, U, U]
    probe.probe_mb_ai_controls.argtypes = [U, U]
    probe.probe_mb_record.argtypes = [P, ctypes.c_int, U]
    probe.probe_mb_controls.argtypes = [U]
    probe.probe_mb_globals.argtypes = [P, ctypes.c_int]
    probe.probe_mb_create.argtypes = [ctypes.c_int, U, ctypes.c_int, ctypes.c_char_p]
    probe.probe_mb_create.restype = U
    probe.probe_mb_image.argtypes = [U, U, P, P]
    probe.probe_mb_tree.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
    probe.probe_mb_tree_node.argtypes = [ctypes.c_int, P, P, P, ctypes.c_int, P, P]
    assert probe.probe_mb_open(str(GAME / 'reader.zbd').encode()) == 1
    return probe


_trees = {}


def tree(probe, entry):
    """mechlib's tree: [(name, cls, parent, model, children, box bytes, data bytes)]."""
    if entry not in _trees:
        n = probe.probe_mb_tree(str(GAME / 'mechlib.zbd').encode(), entry.encode())
        out = []
        for i in range(max(n, 0)):
            name = ctypes.create_string_buffer(36)
            info = (ctypes.c_int32 * 3)()
            kids = (ctypes.c_int32 * 64)()
            box = (ctypes.c_float * 6)()
            data = (ctypes.c_uint8 * 0x30)()
            c = probe.probe_mb_tree_node(i, name, info, kids, 64, box, data)
            out.append((bytes(name.raw), info[0], info[1], info[2], list(kids[:c]), bytes(box), bytes(data)))
        _trees[entry] = out
    return _trees[entry]


# --- the fake world -----------------------------------------------------------
class World:
    def __init__(self, e, probe, case):
        self.e, self.case, self.r = e, case, random.Random(case['seed'])
        a, w = e.world_alloc, e.w32
        self.names = {}  # node address -> name
        self.kids = {}   # node address -> children
        # The mission's objects.
        self.missions = a(0x100)
        lst, head = a(0x10), a(0xc)
        w(self.missions + 0xc0, lst)
        w(lst + 8, head)
        nodes = [a(0xc) for _ in range(case['list_entries'])]
        ring = [head] + nodes
        for i, n in enumerate(ring):
            w(n, ring[(i + 1) % len(ring)])
            w(n + 4, ring[i - 1])
            if n != head:
                w(n + 8, a(0x10))
        w(lst + 0xc, len(nodes))
        self.game = e.object('game', slots={0x34: ('c,s0', 4), 0x2c: ('c', 0)})
        self.pilot = e.object('pilot', 0x100, slots={0x4: ('c', 0)})
        w(self.pilot + 0x6c, self.game)
        w(self.pilot + 0x10, case['pilot_handle'])
        w(0x60861c, self.pilot)
        self.view = a(0x100)
        w(self.view + 4, a(0x40))
        if case['view_camera']:
            cam = a(0x10)
            w(cam + 4, case['view_camera'])
            w(self.view + 0x58, cam)
        w(0x5f4c8c, self.view)
        self.interp = a(0x100)
        w(0x6040b0, self.interp)
        hud = e.object('hud', 0x40, slots={0x60: ('c,s0', 4), 0x64: ('c', 0)})
        self.screen = a(0x18000)
        w(self.screen + 0x17964, e.r32(hud))
        # The definitions (the probe's layout of the port's).
        self.dfn_base = a(0x8000)
        self.dfn_size = probe.probe_mb_dfn_image(case['roster'], self.dfn_base, case['dfn_buf'], 0x8000, 0)
        assert self.dfn_size > 0, self.dfn_size
        e.cpu.write(self.dfn_base, bytes(case['dfn_buf'])[:self.dfn_size])
        self.configs = self.config_addrs()
        # The models: mech_<name>.flt and the low-detail mech_<name>_1.flt as fake nodes.
        name = MECHS[case['roster']]
        self.model = self.lay_tree(tree(probe, f'mech_{name}.flt'))
        self.lod = self.lay_tree(tree(probe, f'mech_{name}_1.flt')) if case['lod'] else 0
        self.cockpit = a(0x80) if case['cockpit'] else 0
        if self.cockpit:
            self.cpit = self.lay_tree([(b'cockpit', 5, -1, 0, [1, 2, 3], bytes(24), bytes(0x30))] +
                                      [(f'cpit_ani{c}'.encode(), 5, 0, 0, [], bytes(24), bytes(0x30))
                                       for c in 'abc'])
            w(self.cockpit + 4, self.cpit)
        self.data = a(0x240)
        w(self.data + 0xc, case['roster'])
        self.data_name = e.world_string(name)
        w(self.data + 0x10, self.data_name)
        w(self.data + 0x14, self.model)
        w(self.data + 0x18, self.model)
        w(self.data + 0x1c, self.lod)
        w(self.data + 0x228, self.cockpit)
        # The pilot's game slot: the mech, its configuration, its skin.
        self.slot = a(0x20)
        w(self.slot + 4, case['slot_mech'])
        cfgs = self.configs
        w(self.slot + 8, cfgs[case['config']] if 0 <= case['config'] < len(cfgs) else case['config_handle'])
        w(self.slot + 0x10, case['skin'] & 0xffffffff)
        # An AI mech's controller with its IControls base at +0x100.
        self.ctl = a(0x400)
        vbt = a(0x10)
        w(self.ctl + 4, vbt)
        w(vbt + 4, 0xfc)
        ivt = a(0x100)
        e.w32(ivt + 8, e.new_stub('ictl.vt+0x8', 'c,s0', 4))
        w(self.ctl + 0x100, ivt)
        self.anims = [a(0x10) for _ in range(40)]
        self.models = [a(0x10) for _ in range(8)]

    def config_addrs(self):
        e, d = self.e, self.dfn_base
        cl = e.r32(d + 0x20)
        if not cl:
            return []
        b, end = e.r32(cl + 0xc), e.r32(cl + 0x10)
        return [e.r32(b + 4 * i) for i in range((end - b) // 4)] if b else []

    def lay_tree(self, nodes):
        e, a = self.e, self.e.world_alloc
        addrs = [a(0xd0) for _ in nodes]
        for i, (name, cls, parent, model, kids, box, data) in enumerate(nodes):
            n = addrs[i]
            e.cpu.write(n, name[:36])
            self.names[n] = name.split(b'\0')[0].decode('latin1')
            e.w32(n + 0x34, cls)
            if cls == 5:
                d = a(0x80)
                e.cpu.write(d + 0x30, data)
                e.w32(n + 0x38, d)
            e.w32(n + 0x3c, a(0x10) if model else 0)
            if parent >= 0:
                arr = a(4)
                e.w32(arr, addrs[parent])
                e.w32(n + 0x54, 1)
                e.w32(n + 0x58, arr)
            if kids:
                arr = a(4 * len(kids))
                for k, c in enumerate(kids):
                    e.w32(arr + 4 * k, addrs[c])
                e.w32(n + 0x5c, len(kids))
                e.w32(n + 0x60, arr)
            self.kids[n] = [addrs[c] for c in kids]
            e.cpu.write(n + 0x74, box)
        return addrs[0]

    def find(self, root, name):
        """0x4f9d60 as the port has it: the node, then its children last to first."""
        if not root:
            return 0
        if self.names.get(root) == name:
            return root
        for c in reversed(self.kids.get(root, [])):
            hit = self.find(c, name)
            if hit:
                return hit
        return 0


def make_script(case, world, e):
    r = case['rng']
    blocks = case['blocks']

    def alloc(n, a):
        size = a[0]
        addr = e.cpu.alloc(max(size, 4))
        e.cpu.write(addr, bytes([FILL]) * max(size, 4))
        blocks.append((addr, size))
        return addr

    def calloc(n, a):
        size = a[0] * a[1]
        addr = e.cpu.alloc(max(size, 4))
        e.cpu.write(addr, bytes(max(size, 4)))
        blocks.append((addr, size))
        return addr

    def find(n, a):
        root, name = a
        # A definition's firepoint must be in the mech (0x46cb80 reads through
        # its part: a crash without, FIX (bug)); the model's markers may go.
        mech = world.kids[world.model][0]
        if root != mech or not name.startswith('firepoint'):
            if name in case['missing'] or (name.startswith('firepoint') and r(f'fp{n}') < case['fp_drop']):
                return 0
        return world.find(root, name)

    def anim(n, a):
        return 0 if r(f'an{n}') < case['anim_fail'] else world.anims[n % 40]

    return {
        'new': alloc, 'calloc': calloc,
        '0x004e0680': int(case['record'] is not None),
        '0x0040d1b0': case['use_joystick'], '0x0040ce70': case['camera_mode'],
        'game.vt+0x34': world.slot, 'game.vt+0x2c': case['type'],
        '0x00430d10': world.data if case['data'] else 0,
        '0x004846a0': world.dfn_base,
        '0x004757f0': lambda n, a: r(f'sk{n}') < 0.5,
        '0x0051dc10': case['joy_present'], '0x0040d010': case['torso_auto_return'],
        '0x00430aa0': world.screen, '0x00595990': 1,
        '0x004794f0': case['pilot_4794f0'],
        '0x004f9af0': lambda n, a: a[0],
        '0x00475a80': lambda n, a: world.models[0] if r(f'dc{n}') < 0.8 else 0,
        '0x004f9d60': find,
        '0x004e6250': lambda n, a: a[0], '0x004e61e0': lambda n, a: a[0],
        '0x004ebd50': case['box_fail'],
        '@box': lambda n, a: fbits(case['box'][a[0]]),
        '@tx': lambda n, a: fbits(r(f'tx{n}') * 8 - 4),
        '@target': lambda n, a: world.models[1], '@centroid': lambda n, a: world.models[2],
        '@transform': lambda n, a: case['transform'],
        '0x00431fb0': anim, '0x00432030': anim,
        '0x0041ff30': lambda n, a: 0 if not a[0] or not a[1] else world.anims[(n + 20) % 40],
        '0x004fb740': lambda n, a: world.models[3 + n % 5],
        '0x00538fe0': lambda n, a: world.models[(n + 1) % 8],
        '0x004254d0': case['valid'],
        '@wcount': case['wcount'], '@wgroup': lambda n, a: case['wgroups'][a[0]],
    }


def rng(seed):
    def f(key):
        return random.Random(f'{seed}:{key}').random()
    return f


# --- the original ---------------------------------------------------------------
def run_original(probe, case):
    t = Trace()
    t.silent |= {'sprintf', 'strncmp', 'cs', 'cs~', 'Format', '0x004e0490', '0x004e04f0', '@rec'}
    e = Emu(t)
    e.cpu.fpu_control(0x007f)
    world = World(e, probe, case)
    t.script = make_script(case, world, e)
    for addr, spec in CALLS.items():
        e.stub(addr, spec=spec)
    for addr in SETUPS:
        e.stub(addr, 'part.vt+0x4', 'c')
    e.stub(0x428700, 'ictl.vt+0x88', 'c,s0', 4)  # a this-adjusting thunk into 0x428440 (ret 4)
    rec_addr = [0]

    def rec_ctor(em, key, a, pop):
        rec_addr[0] = em.args('c')[0]
        em._ret(rec_addr[0], 0)
    e.stub(0x4e0490, '0x004e0490', '', 0, rec_ctor)
    e.stub(0x4e04f0, '0x004e04f0', '', 0, lambda em, k, a, p: em._ret(0, 0))

    def read_named(em, key, a, pop):
        this, name = em.args('c,zs0')
        ok = t.handle(key, [this, name])
        if ok:
            em.cpu.write(this + 4, bytes(case['record'][4:0xd0]))
        em._ret(this if ok else 0, 4)
    e.stub(0x4e0680, '0x004e0680', '', 4, read_named)

    def new(em, key, a, pop):
        em._ret(t.handle('new', em.args('s0')), 0)
    e.stub(0x5abd26, 'new', '', 0, new)
    e.stub(0x5abd1a, 'delete', 's0', 0)
    e.import_stub(0x5bb60c, 'calloc', 's0,s1', 0)
    e.import_stub(0x5bb0d0, 'CloseHandle', 's0', 4)
    empty = e.world_string('')

    def cs_ctor(em, key, a, pop):
        em.w32(em.args('c')[0], empty)
        em._ret(em.args('c')[0], 0)

    def c_printf(em, f, k):
        """MSVC's sprintf for %s, %c and %d with a width (what mech.cpp formats)."""
        vals = []
        for m in re.finditer(r'%(\d*)([csd])', f):
            v = em.args(f's{k}')[0]
            if m.group(2) == 's':
                v = em.cstr(v)
            elif m.group(2) == 'c':
                v = chr(v & 0xff)
            else:
                v = v - (1 << 32) if v & 0x80000000 else v
            vals.append(v)
            k += 1
        return f % tuple(vals)

    def cs_format(em, key, a, pop):
        this, fmt = em.args('s0,s1')
        em.w32(this, em.world_string(c_printf(em, em.cstr(fmt), 2)))
        em._ret(0, 0)

    def sprintf(em, key, a, pop):
        buf, fmt = em.args('s0,s1')
        s = c_printf(em, em.cstr(fmt), 2)
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
    e.stub(0x5abdb0, 'cs', '', 0, cs_ctor)
    e.stub(0x5abda4, 'cs~', '', 0, lambda em, k, a, p: em._ret(0, 0))
    e.stub(0x5abdaa, 'Format', '', 0, cs_format)
    e.import_stub(0x5bb658, 'sprintf', '', 0, sprintf)
    e.import_stub(0x5bb654, 'strncmp', '', 0, strncmp)

    # Stubs that write what the port's hooks set: zEntity's node, the box,
    # a firepoint's place, the pilot's vehicle, the view's target.
    def entity(em, key, a, pop):
        this, node, name, x, y = em.args('c,s0,zs1,s2,s3')
        t.handle(key, [this, node, name, x, y])
        em.w32(this + 4, node)
        em.w32(this + 0xc, t.handle('@transform', []))
        em.w32(this + 0x10, t.handle('@target', []))
        em.w32(this + 0x14, t.handle('@centroid', []))
        em._ret(0, 0x10)
    e.stub(0x4d6c90, '0x004d6c90', '', 0x10, entity)

    def box(em, key, a, pop):
        node, out = em.args('c,d')
        res = t.handle(key, [node])
        if res == 0:
            for i in range(6):
                em.w32(out + 4 * i, t.handle('@box', [i]))
        em._ret(res, 0)
    e.stub(0x4ebd50, '0x004ebd50', '', 0, box)

    def translation(em, key, a, pop):
        node, px, py, pz = em.args('c,d,s0,s1')
        t.handle(key, [node])
        for i, p in enumerate((px, py, pz)):
            em.world_w32(p, t.handle('@tx', [i]))
        em._ret(0, 8)
    e.stub(0x4fc020, '0x004fc020', '', 8, translation)

    def pilot_mech(em, key, a, pop):
        pilot, mech = em.args('c,s0')
        t.handle(key, [pilot, mech])
        em.world_w32(pilot + 0x14, mech)
        em._ret(0, 4)
    e.stub(0x4782c0, '0x004782c0', '', 4, pilot_mech)

    def attach(em, key, a, pop):
        mech = em.args('c')[0]
        t.handle(key, [mech])
        if case['view_on_mech']:
            em.world_w32(world.view + 0x64, mech)
        em._ret(0, 0)
    e.stub(0x41ce30, '0x0041ce30', '', 0, attach)

    def place(em, key, a, pop):
        pilot, pos, yaw = em.args('c,s0,s1')
        em._ret(t.handle(key, [pilot, em.r32(pos), em.r32(pos + 4), em.r32(pos + 8), yaw]), 8)
    e.stub(0x4782f0, '0x004782f0', '', 8, place)

    def polygon(em, key, a, pop):
        model, n, v = em.args('c,d,s0')
        em._ret(t.handle(key, [model, n] + [em.r32(v + 4 * i) for i in range(12)]), 4)
    e.stub(0x4776d0, '0x004776d0', '', 4, polygon)

    def report(em, key, a, pop):
        level, file, line, text = em.args('s0,zs1,s2,zs3')
        em._ret(t.handle(key, [level, file, line, text]), 0)
    e.stub(0x49bfd0, '0x0049bfd0', '', 0, report)

    def msgbox(em, key, a, pop):
        x, y, msg, name = em.args('s0,s1,s2,zs3')
        em._ret(t.handle(key, [x, y, msg, name]), 0)
    e.stub(0x4ac150, '0x004ac150', '', 0, msgbox)

    # The weapons of the configuration (0x414501..0x414732, test_weapons_run.py):
    # one call here, the manager's counts from the script.
    def weapons(uc, address, size, user):
        mech, cfg = uc.reg_read(UC_X86_REG_ESI), uc.reg_read(UC_X86_REG_EBP)
        t.handle('weapons', [mech, cfg])
        wm = e.r32(mech + 0xd4)
        e.w32(wm + 0x24, t.handle('@wcount', []))
        for i in range(5):
            e.w32(e.r32(wm + 0x2c + 4 * i) + 8, t.handle('@wgroup', [i]))
        uc.reg_write(UC_X86_REG_EIP, 0x414732)
    e.uc.hook_add(UC_HOOK_CODE, weapons, begin=0x414501, end=0x414501)

    # The globals before: the made-mechs counter, [0x60b56c] set, the zone mode.
    e.w32(0x605628, case['made'])
    e.cpu.write(0x60b56c, b'\x01')
    e.cpu.write(0x60b570, b'\x01')
    e.w32(0x5ea1d0, case['zone'])
    snap = e.snapshot()
    e.watch_writes([(e.world_base, e.world_base + e.world_size), (0x60b570, 0x60b571)])
    try:
        if case['entry'] == 'launch':
            if case['again']:  # the controls made before (a second launch): only their mode set
                e.call(0x427280, ecx=case['mode0'])
            result = e.call(LAUNCH, ecx=world.missions) & 0xff  # al
        else:
            name = e.world_string(case['name']) if case['name'] else 0
            result = e.call(CREATE, ecx=case['roster_arg'], edx=world.ctl if case['ctl'] else 0,
                            stack=(case['config'], name))
    except Exception:
        print(f'  at {e.uc.reg_read(UC_X86_REG_EIP):#x}; the last calls: {t.log[-4:]}')
        raise
    return t, e, world, snap, result, rec_addr[0]


def run_port(probe, case, t, e, world, snap, rec_addr):
    rep = Replay(t, e, snap)
    probe.probe_app_set_callback(rep.callback())
    probe.probe_mb_reset(world.missions, world.pilot, world.view, world.interp)
    probe.probe_mb_dfn_image(case['roster'], world.dfn_base, case['dfn_buf'], 0x8000, 1)
    lod1 = world.lod
    probe.probe_mb_pilot(world.data, case['roster'], world.model, world.model, lod1, world.cockpit,
                         world.cpit if world.cockpit else 0, world.data_name)
    probe.probe_mb_ai_controls(world.ctl, world.ctl + 0x100)
    raw = case['record'] or bytes(0xd0)
    probe.probe_mb_record(raw, int(case['record'] is not None), rec_addr)
    g = (U * 3)(case['made'], 1, case['zone'])
    probe.probe_mb_globals(g, 1)
    if case['entry'] == 'launch':
        if case['again']:
            probe.probe_mb_controls(case['mode0'])
        result = probe.probe_mb_launch()
    else:
        result = probe.probe_mb_create(case['roster_arg'], world.ctl if case['ctl'] else 0, case['config'],
                                       case['name'].encode() if case['name'] else None)
    probe.probe_mb_globals(g, 0)
    return rep, result, list(g)


# --- the comparison --------------------------------------------------------------
KINDS = ['mech', 'hip', 'torso', 'head', 'arm', 'hand', 'leg', 'calf', 'foot', 'manager', 'group', 'ams', 'skis',
         'list', 'pair', 'wrapper', 'toes', 'toe quats', 'controls']
LIST_ALLOC = {0x380, 0x47c, 0x674, 0x6cc}  # the mech's lists' allocator bytes: whatever the register held


def compare_blocks(probe, name, case, e):
    bad = []
    for addr, size in case['blocks']:
        mem = e.cpu.read(addr, max(size, 4))[:size]
        img = (ctypes.c_uint8 * max(size, 4))(*([FILL] * max(size, 4)))
        mask = (ctypes.c_uint8 * max(size, 4))()
        k = probe.probe_mb_image(addr, size, img, mask)
        if k < 0:
            # An object the port does not model (its constructor is a hook, the
            # stub writes nothing): the original must not write it either.
            if any(b != FILL for b in mem):
                bad.append(f'block {addr:#x} ({size:#x} bytes): written, no port object ({k})')
            continue
        kind = KINDS[k]
        for i in range(size):
            if kind == 'mech' and i in LIST_ALLOC:
                continue
            if kind == 'mech' and i == 0xbc:  # the low nibble cleared, the rest the heap's
                if mem[i] != FILL & 0xf0:
                    bad.append(f'{kind} +{i:#x}: original {mem[i]:#x}')
                continue
            if mask[i]:
                if mem[i] != img[i]:
                    bad.append(f'{kind} +{i:#x}: original {mem[i]:#04x} port {img[i]:#04x}')
            elif mem[i] != FILL and not (kind in ('toes', 'toe quats') and mem[i] == 0):
                bad.append(f'{kind} +{i:#x}: original writes {mem[i]:#04x}, the port has no field')
        if len(bad) > 12:
            break
    if bad:
        print(f'{name}: blocks differ:')
        for b in bad[:16]:
            print('   ', b)
        return False
    return True


def compare_world(probe, name, case, e, world, rep):
    written = {a: e.cpu.read(a, 1)[0] for a in e.writes}
    after = (ctypes.c_uint8 * 0x8000)()
    n = probe.probe_mb_dfn_image(case['roster'], world.dfn_base, after, 0x8000, 0)
    lo, hi = world.dfn_base, world.dfn_base + n
    bad = []
    for a, v in sorted(written.items()):
        if a in rep.shadow:
            got = rep.shadow[a]
        elif lo <= a < hi:
            got = after[a - lo]
        else:
            bad.append(f'{a:#x}: the original writes {v:#04x}, the port nothing')
            continue
        if got != v:
            bad.append(f'{a:#x}: original {v:#04x} port {got:#04x}')
    for a, v in rep.shadow.items():
        if a not in written and (a >= e.world_base or a == 0x60b570) and not (lo <= a < hi):
            bad.append(f'{a:#x}: the port writes {v:#04x}, the original nothing')
    for a in range(lo, hi):  # the definitions' bytes the port changed without the original
        if a not in written and after[a - lo] != e.cpu.read(a, 1)[0]:
            bad.append(f'dfn +{a - lo:#x}: the port changed it to {after[a - lo]:#04x}')
            break
    if bad:
        print(f'{name}: memory written differs:')
        for b in bad[:12]:
            print('   ', b)
        return False
    return True


# --- cases ------------------------------------------------------------------------
def make_case(probe, seed):
    r = random.Random(seed)
    roster = seed % 19
    shipped = seed < 19 * 4
    n_cfg = probe.probe_mb_config_count(roster)
    assert n_cfg > 0, (roster, n_cfg)
    case = {'seed': seed, 'roster': roster, 'rng': rng(seed), 'blocks': [], 'dfn_buf': (ctypes.c_uint8 * 0x8000)()}
    case['entry'] = 'launch' if seed % 4 != 3 else 'create'
    case['config'] = (seed // 19) % n_cfg if shipped else r.randrange(n_cfg)
    crafted = not shipped and r.random() < 0.6
    case['mods'] = []
    if crafted:
        for _ in range(r.randrange(1, 6)):
            field = r.choice([0, 1, 2, 3, 4, 4, 5, 6, 7, 8, 10, 11, 9])
            if field == 0:
                v = r.choice([0, 1, 10, 20, 30, 40, 50, 60, 2, 33])
            elif field == 1:
                v = r.choice([0, 1, 10, 15, 25])
            elif field == 2:
                v = r.choice([0, 1])
            elif field == 3:
                v = r.choice([0, 1, 2, 3])
            elif field == 4:
                v = (r.randrange(11) << 16) | r.choice([0, 0, 1, 7, 25, 64, 100])
            elif field in (5, 6, 7, 8):
                v = fbits(r.choice([0.0, 0.5, 1.25663686, 3.0, 2.5e-5, -1.0]))
            elif field == 9:
                v = r.choice([roster, roster, (roster + 1) % 19])
            else:
                v = r.getrandbits(32)
            case['mods'].append((field, v))
    case['type'] = 1 if r.random() < 0.6 else r.choice([-1, 0, 1, 2, 3, 4, 5])
    case['record'] = None if r.random() < 0.04 else bytearray(0xd0)
    if case['record'] is not None:
        rec = case['record']
        struct.pack_into('<i', rec, 4, r.randrange(40))
        struct.pack_into('<fff', rec, 8, r.uniform(-500, 500), r.uniform(0, 50), r.uniform(-500, 500))
        struct.pack_into('<f', rec, 0x14, r.uniform(-3.2, 3.2))
        struct.pack_into('<I', rec, 0x18, r.choice([1, 1, 1, 2, 0, 0x41, 0x22]))
        rec[0xcc] = r.choice([0, 0x20, 0x80, 0xa0])
        case['record'] = bytes(rec)
    case['use_joystick'] = r.choice([0, 1, 1, 2])
    case['camera_mode'] = r.choice([0, 1, 2, 3])
    case['joy_present'] = r.choice([0, 1])
    case['torso_auto_return'] = r.choice([0, 1, 2])
    case['view_camera'] = r.choice([0, 3, 7])
    case['view_on_mech'] = r.random() < 0.9
    case['pilot_handle'] = r.choice([0xffffffff, 0x1234])
    case['pilot_4794f0'] = r.choice([0, 1])
    case['list_entries'] = r.choice([0, 1, 3])
    case['data'] = r.random() < 0.97
    case['slot_mech'] = roster
    case['config_handle'] = 0
    if r.random() < 0.03:
        case['config'] = -1  # the slot without a configuration: no mech
    case['skin'] = r.choice([-1, 0, 0, 1, 2, 5, 9])
    case['lod'] = r.random() < 0.9
    case['cockpit'] = r.random() < 0.5
    case['missing'] = set()
    for nm in ('butt', 'lshadow', 'rshadow', 'ltoe01', 'rtoe02', 'cpit_anib', 'firepoint03'):
        if r.random() < 0.1:
            case['missing'].add(nm)
    case['fp_drop'] = r.choice([0.0, 0.0, 0.3])
    case['anim_fail'] = r.choice([0.0, 0.0, 0.2])
    case['box_fail'] = 0 if r.random() < 0.9 else 5
    case['box'] = [r.uniform(-8, 0), r.uniform(-1, 0), r.uniform(-8, 0), r.uniform(0, 8), r.uniform(4, 20),
                   r.uniform(0, 8)]
    case['transform'] = r.getrandbits(32) & ~3
    case['valid'] = 1 if r.random() < 0.93 else 0
    case['wcount'] = r.choice([0, 0, 1, 4, 9])
    case['wgroups'] = [r.choice([0, 0, 1, 3]) for _ in range(5)]
    case['made'] = r.randrange(0, 120)
    case['zone'] = r.choice([0, 1, 1])
    # The AI mech's entry: the roster index (the type's), a controller, a name or none.
    case['roster_arg'] = roster
    case['ctl'] = r.random() < 0.97
    case['name'] = r.choice(['owens_01', None, 'madcat_mech01', 'x'])
    case['again'] = case['entry'] == 'launch' and r.random() < 0.2
    case['mode0'] = r.choice([0, 1, 3, 4, 6, 7])
    case['skins'] = True
    if case['entry'] == 'create':
        case['config'] = r.choice([0, 1, case['config'], -1, 7, 100])
        case['skins'] = r.random() < 0.85  # a definition without skins (the launch reads through them)
    return case


def main():
    probe = setup_probe()
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 240
    calls = blocks = 0
    stats = collections.Counter()
    for seed in range(count):
        case = make_case(probe, seed)
        cfg_index = case['config'] if 0 <= case['config'] < probe.probe_mb_config_count(case['roster']) else 0
        # Crafted configurations: the shipped one changed for this case, put back after.
        saved = []
        skins = None if case['skins'] else probe.probe_mb_dfn_skins(case['roster'], None)
        for field, v in case['mods']:
            old = probe.probe_mb_config_set(case['roster'], cfg_index, field, v)
            saved.append((field, (v & 0xffff0000) | (old & 0xffff) if field == 4 else old))
        try:
            t, e, world, snap, result, rec_addr = run_original(probe, case)
        except Exception:
            print(f'seed {seed}: the original faulted ({MECHS[case["roster"]]}, {case["entry"]})')
            raise
        rep, got, glob = run_port(probe, case, t, e, world, snap, rec_addr)
        name = f'seed {seed} ({MECHS[case["roster"]]} config {case["config"]}, {case["entry"]}, type {case["type"]})'
        if not compare(name, t.log, rep.log):
            raise SystemExit(1)
        want = result & 0xff if case['entry'] == 'launch' else result
        if (want != 0) != (got != 0):
            print(f'{name}: result original {want:#x} port {got:#x}')
            raise SystemExit(1)
        if not compare_blocks(probe, name, case, e) or not compare_world(probe, name, case, e, world, rep):
            raise SystemExit(1)
        orig = [e.r32(0x605628), e.cpu.read(0x60b56c, 1)[0], e.r32(0x5ea1d0)]
        if orig != glob:
            print(f'{name}: globals (made, 0x60b56c, zone) original {orig} port {glob}')
            raise SystemExit(1)
        calls += len(t.log)
        blocks += len(case['blocks'])
        for field, old in reversed(saved):
            probe.probe_mb_config_set(case['roster'], cfg_index, field, old)
        if not case['skins']:
            probe.probe_mb_dfn_skins(case['roster'], skins)
        stats[case['entry'] + (' again' if case['again'] else '')] += 1
        if case['entry'] == 'launch':
            stats[f"type {case['type']}"] += 1
        stats['mechs built'] += any(k == 'new' and a == (0x6d8,) for k, a in t.log)
        stats['configured'] += any(k == 'weapons' for k, _ in t.log)
        stats['crafted'] += bool(case['mods'])
    print(f'{count} cases, {calls} calls, {blocks} blocks: the rebuilt mech build matches the original')
    print('  ' + ', '.join(f'{k} {v}' for k, v in sorted(stats.items())))


if __name__ == '__main__':
    main()
