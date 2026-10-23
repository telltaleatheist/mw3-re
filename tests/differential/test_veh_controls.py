"""The vehicles' controls base against the original x86 (docs/vehicles.md):
its init 0x4ac8a0 (a serial number, the node's rotation into the owner's
matrix 0x4ab990, the body, turret, shadow and gun nodes, the gun's
firepoints, the health 100), its setup 0x4acbc0 (the init, the definition's
start_anims and health, the owner's 0x4af2a0 and weapons 0x4b4660),
0x4accd0, and the owner's weapon slots 0x4af2a0 (step 2a1b).

    cmake --build build && .venv/bin/python tests/differential/test_veh_controls.py [runs]

The original's base is made by its constructor 0x4ac750 (owner given, most
derived), at the 24-bit x87 precision of play. Every call out is stubbed
(calltrace.py); values handed back through pointers (the rotation, a
translation, a model) come to the port as "@" answers, and the definition's
reader nodes live in the emulator's memory (the probe mirrors them). Each
run: an owner or none, its node or none, each node found or not, the gun's
firepoints each there or not, start_anims of 0..3 names (a def found or
not), a health or none; the init alone, the setup, or 0x4accd0. Compared:
the traces, the base's fields, the owner's matrix and the row the base
copies from it (within 4 ulps or 1e-6: x87 against float rounding) and
the serial counter. The slots: their words preset at random, a serial of 1 or
not, a gun or none, a `weapons` list of 0..10 items (each weapon known, a
beam or not, paired or not, counted or not), firepoints found or not.
Compared: the traces (the beams' muzzle and direction pointers among them)
and every slot's words. The placement 0x4a3630 (step 1c), on a stand-in
vehicle (vt+0x18 stubbed) over the original's base: a position, a yaw, the
ground hit or not (none, one, several), the controls' fields preset (a
serial of 0 or not, 0..15 points, 0..16 supports); compared: the traces (the
place given to vt+0x18 among them) and the base's fields. The motion
block 0x49ef40 (step 1c2a): every key of vehicle.zrd's mode there or not
(the mode's strings, unknown ones too), the model's support and collide
nodes all there, some or none (the lists then), the destroy animation by
name, type or neither; compared: the traces (the reports' texts), the
whole base and the collision count.
"""
import ctypes
import math
import os
import struct
import sys
from pathlib import Path

os.environ.setdefault('MW3_X87', '24')
sys.path.insert(0, str(Path(__file__).parent))
from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESP  # noqa: E402
from calltrace import CALLBACK, Emu, Replay, Trace, compare, load_probe, rnd  # noqa: E402

U, P = ctypes.c_uint32, ctypes.c_void_p
SERIAL, GRAVITY = 0x60b148, 0x609a14
SIZE = 0xa04  # the base, to its IControls
COMPUTED = {0x308, 0x30c, 0x310}  # the matrix's row, compared as the matrix
# friction[1] = 0.9 friction[0] and 1 / mass: the x87 rounds once at 24 bits,
# the port through a double (one ulp at most).
ONE_ULP = {0x764 + 0x5c, 0x764 + 0x24c}
NODES = ('body', 'turret', 'shadow', 'gun')
FPNTS = ('fpnt_c', 'fpnt_l', 'fpnt_r')


def fbits(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def ulps(a, b):
    ia = struct.unpack('<i', struct.pack('<f', a))[0]
    ib = struct.unpack('<i', struct.pack('<f', b))[0]
    if ia < 0:
        ia = -0x80000000 - ia
    if ib < 0:
        ib = -0x80000000 - ib
    return abs(ia - ib)


def close(a, b):
    """A rotation's entry (in [-1, 1]): within 4 ulps or 1e-6, the sines'
    and cosines' x87 against float rounding, larger where terms cancel."""
    return ulps(a, b) <= 4 or abs(a - b) <= 1e-6


def reader_list(e, items):
    """A zReader list node: [(type, value)], the count first."""
    lst = e.world_alloc(8 * (len(items) + 1))
    e.w32(lst, 1)
    e.w32(lst + 4, len(items) + 1)
    for i, (ty, v) in enumerate(items, 1):
        e.w32(lst + 8 * i, ty)
        e.w32(lst + 8 * i + 4, v)
    node = e.world_alloc(8)
    e.w32(node, 4)
    e.w32(node + 4, lst)
    return node


def make(seed, t, gravity):
    e = Emu(t)
    e.heap_stubs()
    r = rnd(seed, 'case')
    case = {
        'owner': r.random() < 0.93,
        'node': r.random() < 0.93,
        'found': {n: r.random() < 0.8 for n in NODES},
        'fpnts': {n: r.random() < 0.6 for n in FPNTS},
        'anims': r.choice([0, 0, 1, 2, 3]),
        'health': r.choice([None, 10.0, 75.0, 10000.0]),
        'entry': r.choice(['init', 'setup', 'setup', 'reset']),
    }
    owner = e.world_alloc(0x100) if case['owner'] else 0
    node = e.world_alloc(0x40) if case['node'] else 0
    matrix = e.world_alloc(0x40)
    if owner:
        e.w32(owner + 4, node)
        e.w32(owner + 0xc, matrix)
    nodes = {n: e.world_alloc(0x40) if case['found'][n] else 0 for n in NODES}
    fpnts = {n: e.world_alloc(0x40) if case['fpnts'][n] else 0 for n in FPNTS}
    gun_matrix = e.world_alloc(0x40)
    for k in range(12):
        e.wf(gun_matrix + 4 * k, r.uniform(-50, 50))
    e.w32(SERIAL, r.randrange(0, 1000))
    e.w32(GRAVITY, fbits(gravity))

    def angle():
        return r.choice([0.0, r.uniform(-3.2, 3.2), r.uniform(-20, 20), math.pi / 2])

    def out_floats(key, ptrs, vals):
        def fn(em, k, a, pop):
            em.t.handle(k, a[:1])
            for p, axis, v in zip(ptrs(em), 'xyz', vals()):
                em.wf(p, v)
                em.t.handle(f'@{key}.{axis}', ())
                em.t.override(f'@{key}.{axis}', fbits(v))
            em._ret(0, pop)
        return fn

    esp_args = lambda em: [em.uc.reg_read(UC_X86_REG_EDX)] + em.args('s0,s1')  # noqa: E731
    e.stub(0x4fbe90, spec='c', fn=out_floats('rot', esp_args, lambda: [angle() for _ in range(3)]))
    e.stub(0x4fc020, spec='c', fn=out_floats('tr', esp_args, lambda: [r.uniform(-5, 5) for _ in range(3)]))

    def answer_by_name(table):
        def fn(em, key, a, pop):
            v = table.get(a[1], 0)
            em.t.handle(key, a)
            em.t.override(key, v)
            em._ret(v, pop)
        return fn
    e.stub(0x4f9d60, spec='c,zd', fn=answer_by_name(nodes))
    e.stub(0x4eb0d0, spec='c,zd', fn=answer_by_name(fpnts))

    def answer(value):
        def fn(em, key, a, pop):
            em.t.handle(key, a)
            em.t.override(key, value)
            em._ret(value, pop)
        return fn
    e.stub(0x4fc3f0, spec='c', fn=answer(gun_matrix))
    model = r.choice([0, e.world_alloc(0x10)])

    def get_model(em, key, a, pop):
        em.w32(em.uc.reg_read(UC_X86_REG_EDX), model)
        em.t.handle(key, a)
        em.t.handle('@model', ())
        em.t.override('@model', model)
        em._ret(0, pop)
    e.stub(0x4eb420, spec='c', fn=get_model)
    for addr, spec in ((0x5a85c0, 'c'), (0x4eb370, 'c,d'), (0x539150, 'c'), (0x4ebb40, 'c,d'),
                       (0x4f9050, 'c'), (0x508c10, 'c,d,s0,s1,s2'), (0x4af2a0, 'c,s0'), (0x4b4660, 'c,s0')):
        e.stub(addr, spec=spec)
    names = ['apc_spin', 'radar_dish', 'flag_wave'][:case['anims']]
    anim_defs = {nm: (e.world_alloc(0x10) if r.random() < 0.7 else 0) for nm in names}
    e.stub(0x50b450, spec='zc', fn=lambda em, k, a, p: answer(anim_defs.get(a[0], 0))(em, k, a, p))
    keys = {}
    if names or r.random() < 0.3:
        keys['start_anims'] = reader_list(e, [(3, e.world_string(nm)) for nm in names])
    if case['health'] is not None:
        keys['health'] = reader_list(e, [(2, fbits(case['health']))])
    e.stub(0x5569e0, spec='c,zd', fn=answer_by_name(keys))
    def sprintf(em, key, a, pop):  # the CRT's, for "%s_L", "%s_R", "support_%02d", "collide_%02d"
        fmt = em.cstr(a[1])
        out = fmt.replace('%s', em.cstr(a[2])) if '%s' in fmt else fmt.replace('%02d', f'{a[2]:02d}')
        em.cpu.write(a[0], out.encode('latin1') + b'\0')
        em._ret(len(out), pop)
    e.import_stub(0x5bb658, '@sprintf', 's0,s1,s2', 0, sprintf)

    def strncmp(em, key, a, pop):
        x, y = em.cstr(a[0])[:a[2]], em.cstr(a[1])[:a[2]]
        em._ret(0 if x == y else (-1 if x < y else 1), pop)
    e.import_stub(0x5bb654, '@strncmp', 's0,s1,s2', 0, strncmp)
    ctl = e.alloc(0xa70)
    e.call(0x4ac750, ecx=ctl, stack=(owner, 1))
    return e, case, owner, node, matrix, ctl


def run(probe, seed):
    t = Trace()
    e, case, owner, node, matrix, ctl = make(seed, t, probe.probe_vc_gravity())
    defn = e.world_alloc(0x10)
    rep = Replay(t, e, e.snapshot())
    probe.probe_app_set_callback(rep.callback())
    probe.probe_vc_new(owner, node, e.r32(SERIAL))
    name = f'seed {seed} {case}'
    r = rnd(seed, 'block')
    block = [r.randrange(1 << 32) for _ in range(6)]
    for k, v in enumerate(block):
        e.w32(ctl + 0x398 + 4 * k, v)
    probe.probe_vc_set398((U * 6)(*block))
    e.cpu.fpu_control(0x007f)
    if case['entry'] == 'init':
        e.call(0x4ac8a0, ecx=ctl)
        probe.probe_vc_init()
    elif case['entry'] == 'setup':
        e.call(0x4acbc0, ecx=ctl, stack=(defn,))
        probe.probe_vc_setup(defn)
    else:
        e.call(0x4accd0, ecx=ctl)
        probe.probe_vc_reset_1c()
    if not compare(name, t.log, rep.log):
        return False, 0
    a = bytes(e.cpu.read(ctl, SIZE))
    b = (ctypes.c_uint8 * SIZE)(*a)
    probe.probe_vc_image(b)
    b = bytes(b)
    for o in range(0, SIZE, 4):
        if o in COMPUTED and close(*struct.unpack('<2f', a[o:o + 4] + b[o:o + 4])):
            continue
        if a[o:o + 4] != b[o:o + 4]:
            print(f'{name}: +{o:#x} original {a[o:o + 4].hex()} rebuild {b[o:o + 4].hex()}')
            return False, 0
    if owner:
        got = (ctypes.c_float * 9)()
        probe.probe_vc_matrix(got)
        for k in range(9):
            want = e.f32(matrix + 4 * k)
            if not close(want, got[k]):
                print(f'{name}: matrix [{k}] original {want!r} rebuild {got[k]!r}')
                return False, 0
    if e.r32(SERIAL) != probe.probe_vc_serial() & 0xffffffff:
        print(f'{name}: serial original {e.r32(SERIAL)} rebuild {probe.probe_vc_serial()}')
        return False, 0
    return True, len(t.log)


def item(e, r, name):
    """A `weapons` item: (name, counted, paired, ammo, three values, a flag)."""
    vals = [(3, e.world_string(name)), (1, r.choice([0, 1, 5])), (1, r.choice([0, 1, 2, 3])),
            (1, r.choice([0, 20, 150])), (1, r.randrange(1 << 32)), (1, r.randrange(1 << 32)),
            (1, r.randrange(1 << 32)), (1, r.choice([0, 1, 2, 3]))]
    lst = e.world_alloc(8 * (len(vals) + 1))
    e.w32(lst, 1)
    e.w32(lst + 4, len(vals) + 1)
    for i, (ty, v) in enumerate(vals, 1):
        e.w32(lst + 8 * i, ty)
        e.w32(lst + 8 * i + 4, v)
    return (4, lst)


def run_slots(probe, seed):
    t = Trace()
    e, case, owner, node, matrix, ctl = make(seed, t, probe.probe_vc_gravity())
    if not owner:
        return True, 0
    r = rnd(seed, 'slots')
    names = ['AC2', 'MGUN', 'SRM2', 'Laser_Large', 'ERPPC', 'Flamer']
    defs = {}
    for nm in names:
        d = e.world_alloc(0x80)
        e.w32(d + 4, e.world_string(nm))
        e.w32(d + 0x6c, r.choice([0, 2, 3, 0x40]))
        defs[nm] = d
    count = r.choice([0, 1, 2, 3, 10])
    items = [item(e, r, r.choice(names)) for _ in range(count)]
    weapons = reader_list(e, items) if count or r.random() < 0.5 else 0
    gun = e.world_alloc(0x40) if r.random() < 0.8 else 0
    fp = {}
    for nm in names:
        for n2 in (nm, nm + '_L', nm + '_R'):
            fp[n2] = e.world_alloc(0x40) if r.random() < 0.6 else 0
    serial = r.choice([0, 1, 1, 2, 7])
    del e.slots[0x4af2a0]  # here it runs

    e.w32(owner + 0xe4, ctl)
    e.w32(ctl + 0x14, serial)
    e.w32(ctl + 0x708, gun)

    def answer_by(table, idx):
        def fn(em, key, a, pop):
            v = table.get(a[idx], 0)
            em.t.handle(key, a)
            em.t.override(key, v)
            em._ret(v, pop)
        return fn
    e.stub(0x5569e0, spec='c,zd', fn=lambda em, k, a, p: answer_by({'weapons': weapons}, 1)(em, k, a, p))
    e.stub(0x581ed0, spec='zc', fn=answer_by(defs, 0))
    e.stub(0x4eb0d0, spec='c,zd', fn=answer_by(fp, 1))
    e.stub(0x4eb170, spec='c,d')
    beams = iter(e.world_alloc(0x10) for _ in range(12))

    def beam(em, key, a, pop):
        v = next(beams)
        em.t.handle(key, a)
        em.t.override(key, v)
        em._ret(v, pop)
    e.stub(0x584a70, spec='c,d,s0,s1,s2,s3,s4', fn=beam)
    slots = [[r.randrange(1 << 32) for _ in range(14)] for _ in range(10)]
    for i, w in enumerate(slots):
        for k, v in enumerate(w):
            e.w32(ctl + 0x3b8 + 0x38 * i + 4 * k, v)
    defn = e.world_alloc(0x10)
    rep = Replay(t, e, e.snapshot())
    probe.probe_app_set_callback(rep.callback())
    probe.probe_vc_new(owner, node, e.r32(SERIAL))
    probe.probe_vc_slots_state(ctl, serial, gun)
    for i, w in enumerate(slots):
        probe.probe_vc_set_slot(i, (U * 14)(*w))
    name = f'slots seed {seed} ({count} items, serial {serial}, gun {bool(gun)})'
    e.cpu.fpu_control(0x007f)
    e.call(0x4af2a0, ecx=owner, stack=(defn,))
    probe.probe_vc_weapon_slots(defn)
    if not compare(name, t.log, rep.log):
        return False, 0
    a = bytes(e.cpu.read(ctl, SIZE))
    b = (ctypes.c_uint8 * SIZE)(*a)
    probe.probe_vc_image(b)
    b = bytes(b)
    for o in list(range(0x3b4, 0x3b8 + 0x38 * 10, 4)) + [0x5ec]:
        if a[o:o + 4] != b[o:o + 4]:
            print(f'{name}: +{o:#x} (slot {(o - 0x3b8) // 0x38}, +{(o - 0x3b8) % 0x38:#x}) original {a[o:o + 4].hex()} rebuild {b[o:o + 4].hex()}')
            return False, 0
    return True, len(t.log)


def run_place(probe, seed):
    t = Trace()
    e, case, owner, node, matrix, ctl = make(seed, t, probe.probe_vc_gravity())
    if not owner:
        return True, 0
    r = rnd(seed, 'place')
    vt = e.world_alloc(0x200)
    e.w32(owner, vt)

    def set_position(em, key, a, pop):
        p = a[0]
        em.t.handle(key, (owner, em.r32(p), em.r32(p + 4), em.r32(p + 8)))
        em._ret(0, pop)
    e.w32(vt + 0x18, e.new_stub('veh.vt+0x18', 's0', 4, set_position))
    e.w32(owner + 0xe4, ctl)
    world = e.world_alloc(0x10)
    e.w32(0x608c84, world)
    e.w32(world + 4, e.world_alloc(0x10))
    count = r.choice([0, 0, 1, 3])
    found = r.random() < 0.8
    height = r.choice([0.0, r.uniform(-50, 300)])

    def ground(em, key, a, pop):
        out = em.uc.reg_read(UC_X86_REG_EDX)
        em.cpu.write(out, bytes([count]))
        em.wf(out + 0x14, height)
        em.t.handle(key, a)
        em.t.override(key, 0 if found else 1)
        em.t.handle('@hits.count', ())
        em.t.override('@hits.count', count)
        em.t.handle('@hits.height', ())
        em.t.override('@hits.height', fbits(height))
        em._ret(0 if found else 1, pop)
    e.stub(0x4edab0, spec='s0,s1,s2', fn=ground)
    w = [fbits(r.uniform(-1, 1)), fbits(r.uniform(-1, 1)), r.choice([0, 0, 1, 5]), r.randrange(1 << 32),
         r.randrange(1 << 32), fbits(r.uniform(-9, 9)), fbits(r.uniform(-9, 9)), fbits(r.uniform(-9, 9)),
         fbits(r.uniform(-3, 3)), r.randrange(0, 16), r.randrange(0, 17)]
    for off, v in zip((0x08, 0x0c, 0x14, 0x20, 0x24, 0x70, 0x74, 0x78, 0x330, 0x214, 0x7e8), w):
        e.w32(ctl + off, v)
    points = [r.uniform(-500, 500) for _ in range(45)]
    for k, v in enumerate(points):
        e.wf(ctl + 0x160 + 4 * k, v)
    words = [r.randrange(1 << 32) for _ in range(16)]
    for k, v in enumerate(words):
        e.w32(ctl + 0x8b0 + 4 * k, v)
    pos = [r.uniform(-3000, 3000), r.choice([0.0, r.uniform(-20, 200)]), r.uniform(-3000, 3000)]
    yaw = r.uniform(-3.2, 3.2)
    pos_addr = e.world_alloc(0x10)
    for k, v in enumerate(pos):
        e.wf(pos_addr + 4 * k, v)
    rep = Replay(t, e, e.snapshot())
    probe.probe_app_set_callback(rep.callback())
    probe.probe_vc_new(owner, node, e.r32(SERIAL))
    probe.probe_vc_place_state((U * 11)(*w), (ctypes.c_float * 45)(*points), (U * 16)(*words))
    name = f'place seed {seed} (hits {count}, found {found}, serial {w[2]}, points {w[9]}, words {w[10]})'
    e.cpu.fpu_control(0x007f)
    e.call(0x4a3630, ecx=owner, stack=(pos_addr, fbits(yaw)))
    probe.probe_vc_place((ctypes.c_float * 3)(*pos), yaw)
    if not compare(name, t.log, rep.log):
        return False, 0
    a = bytes(e.cpu.read(ctl, SIZE))
    b = (ctypes.c_uint8 * SIZE)(*a)
    probe.probe_vc_image(b)
    b = bytes(b)
    for o in range(0, SIZE, 4):
        if a[o:o + 4] != b[o:o + 4]:
            print(f'{name}: +{o:#x} original {a[o:o + 4].hex()} rebuild {b[o:o + 4].hex()}')
            return False, 0
    return True, len(t.log)


MODE_KEYS = {
    'rates': 2, 'friction': 3, 'stopping': 1, 'quicksand_slowdown': 1, 'lava_slowdown': 1, 'turns': 2,
    'turn_damping': 1, 'rate_damping': 2, 'a_damping': 1, 'mass': 1, 'gun_pitch': 2, 'amphib_wave': 7,
    'hover_wave': 7, 'sub_wave': 7, 'chas_smooth': 1, 'chas_pitch': 3, 'chas_roll': 3, 'collision_d': 2,
}


def points_list(e, r, n):
    return reader_list(e, [(4, reader_list(e, [(2, fbits(r.uniform(-9, 9))) for _ in range(3)]) and
                            e.r32(reader_list(e, [(2, fbits(r.uniform(-9, 9))) for _ in range(3)]) + 4))
                           for _ in range(n)])


def run_motion(probe, seed):
    t = Trace()
    e, case, owner, node, matrix, ctl = make(seed, t, probe.probe_vc_gravity())
    r = rnd(seed, 'motion')
    vnode = e.world_alloc(0x40)
    keys = {}
    for k, n in MODE_KEYS.items():
        if r.random() < 0.5:
            vals = [r.choice([r.uniform(-50, 50), 0.0, 1.0, r.uniform(0, 20000)]) for _ in range(n)]
            keys[k] = reader_list(e, [(2, fbits(v)) for v in vals])
    if r.random() < 0.8:
        keys['mode'] = reader_list(e, [(3, e.world_string(r.choice(['basic', 'track', 'hover', 'amphib', 'sub', 'fly',
                                                                     'tracked', 'boat', 'fx'])))])
    keys['platform'] = points_list(e, r, r.choice([0, 1, 4, 6]))
    keys['collision'] = points_list(e, r, r.choice([0, 3, 12]))
    supports = r.choice([4, 4, 2, 0])
    collides = r.choice([12, 12, 7, 0])
    names = {f'support{i:02d}': (e.world_alloc(0x10) if i < supports else 0) for i in range(4)}
    names.update({f'collide{i:02d}': (e.world_alloc(0x10) if i < collides else 0) for i in range(12)})
    for nm in ('right_morphs', 'left_morphs', 'chassis', 'rtracks', 'ltracks'):
        names[nm] = e.world_alloc(0x10) if r.random() < 0.5 else 0
    vname, vtype = r.choice(['apc_01', 'truck02']), r.choice(['apc', 'ttruck'])
    anims = {vname: e.world_alloc(0x10) if r.random() < 0.4 else 0, vtype: e.world_alloc(0x10) if r.random() < 0.5 else 0}
    mode_alt = r.choice([None, 1.5, 3.0])

    def answer_by(table, idx):
        def fn(em, key, a, pop):
            v = table.get(a[idx], 0)
            em.t.handle(key, a)
            em.t.override(key, v)
            em._ret(v, pop)
        return fn
    e.stub(0x5569e0, spec='c,zd', fn=answer_by(keys, 1))
    e.stub(0x4f9d60, spec='c,zd', fn=answer_by(names, 1))
    e.stub(0x50b450, spec='zc', fn=answer_by(anims, 0))
    e.stub(0x4eb170, spec='c,d')

    def get_float(em, key, a, pop):
        ok = mode_alt is not None
        if ok:
            em.wf(a[2], mode_alt)
        em.t.handle(key, a[:2])
        em.t.override(key, int(ok))
        em.t.handle('@float', ())
        em.t.override('@float', fbits(mode_alt) if ok else 0)
        em._ret(int(ok), pop)
    e.stub(0x556a60, spec='c,zd,s0', fn=get_float)

    def report(em, key, a, pop):
        msg = em.cstr(a[3])
        for k in (4, 5):
            if '%s' in msg:
                msg = msg.replace('%s', em.cstr(em.r32(em.uc.reg_read(UC_X86_REG_ESP) + 4 + 4 * k)), 1)
        em.t.handle(key, (a[0], a[1], a[2], msg))
        em._ret(0, pop)
    e.stub(0x49bfd0, spec='s0,zs1,s2,s3', pop=0, fn=report)
    count_addr = e.world_alloc(0x10)
    preset = r.randrange(0, 13)
    e.w32(count_addr, preset)
    old_mode = r.choice([0, 1, 4])
    e.w32(ctl + 0x764, old_mode)
    ns, ts = e.world_string(vname), e.world_string(vtype)
    mode_node = e.world_alloc(0x10)
    rep = Replay(t, e, e.snapshot())
    probe.probe_app_set_callback(rep.callback())
    probe.probe_vc_new(owner, node, e.r32(SERIAL))
    probe.probe_vc_set_mode(old_mode)
    cnt = ctypes.c_int32(preset)
    name = f'motion seed {seed} (keys {sorted(keys)}, supports {supports}, collides {collides})'
    e.cpu.fpu_control(0x007f)
    e.call(0x49ef40, ecx=ctl + 0x764, stack=(vnode, mode_node, ns, ts, count_addr))
    probe.probe_vc_motion(vnode, mode_node, vname.encode(), vtype.encode(), ctypes.byref(cnt))
    if not compare(name, t.log, rep.log):
        return False, 0
    a = bytes(e.cpu.read(ctl, SIZE))
    b = (ctypes.c_uint8 * SIZE)(*a)
    probe.probe_vc_image(b)
    b = bytes(b)
    for o in range(0, SIZE, 4):
        if o in ONE_ULP and ulps(*struct.unpack('<2f', a[o:o + 4] + b[o:o + 4])) <= 1:
            continue
        if a[o:o + 4] != b[o:o + 4]:
            print(f'{name}: +{o:#x} (block +{o - 0x764:#x}) original {a[o:o + 4].hex()} rebuild {b[o:o + 4].hex()}')
            return False, 0
    if e.r32(count_addr) != cnt.value & 0xffffffff:
        print(f'{name}: collision count original {e.r32(count_addr)} rebuild {cnt.value}')
        return False, 0
    return True, len(t.log)


def main():
    probe = load_probe()
    probe.probe_app_set_callback.argtypes = [CALLBACK]
    probe.probe_vc_new.argtypes = [U, U, ctypes.c_int32]
    probe.probe_vc_setup.argtypes = [U]
    probe.probe_vc_image.argtypes = [P]
    probe.probe_vc_matrix.argtypes = [P]
    probe.probe_vc_set398.argtypes = [P]
    probe.probe_vc_slots_state.argtypes = [U, ctypes.c_int32, U]
    probe.probe_vc_set_slot.argtypes = [ctypes.c_int, P]
    probe.probe_vc_weapon_slots.argtypes = [U]
    probe.probe_vc_place_state.argtypes = [P, P, P]
    probe.probe_vc_place.argtypes = [P, ctypes.c_float]
    probe.probe_vc_motion.argtypes = [U, U, ctypes.c_char_p, ctypes.c_char_p, P]
    probe.probe_vc_set_mode.argtypes = [ctypes.c_int32]
    probe.probe_vc_gravity.restype = ctypes.c_float
    args = [a for a in sys.argv[1:] if not a.startswith('-')]
    runs = int(args[0]) if args else 400
    calls = 0
    for seed in range(runs):
        for part in (run, run_slots, run_place, run_motion):
            ok, n = part(probe, seed)
            if not ok:
                sys.stdout.flush(); os._exit(1)
            calls += n
    print(f'ok: {runs} controls inits, setups and resets {runs} weapon-slot fills, {runs} placements and {runs} motion blocks as the original ({calls} calls)')
    sys.stdout.flush(); os._exit(0)


if __name__ == '__main__':
    main()
