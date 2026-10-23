"""The vehicles' controls base against the original x86 (docs/vehicles.md):
its init 0x4ac8a0 (a serial number, the node's rotation into the owner's
matrix 0x4ab990, the body, turret, shadow and gun nodes, the gun's
firepoints, the health 100), its setup 0x4acbc0 (the init, the definition's
start_anims and health, the owner's 0x4af2a0 and weapons 0x4b4660) and
0x4accd0.

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
the serial counter.
"""
import ctypes
import math
import os
import struct
import sys
from pathlib import Path

os.environ.setdefault('MW3_X87', '24')
sys.path.insert(0, str(Path(__file__).parent))
from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_EDX  # noqa: E402
from calltrace import CALLBACK, Emu, Replay, Trace, compare, load_probe, rnd  # noqa: E402

U, P = ctypes.c_uint32, ctypes.c_void_p
SERIAL, GRAVITY = 0x60b148, 0x609a14
SIZE = 0x7a4
COMPUTED = {0x308, 0x30c, 0x310}  # the matrix's row, compared as the matrix
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


def main():
    probe = load_probe()
    probe.probe_app_set_callback.argtypes = [CALLBACK]
    probe.probe_vc_new.argtypes = [U, U, ctypes.c_int32]
    probe.probe_vc_setup.argtypes = [U]
    probe.probe_vc_image.argtypes = [P]
    probe.probe_vc_matrix.argtypes = [P]
    probe.probe_vc_set398.argtypes = [P]
    probe.probe_vc_gravity.restype = ctypes.c_float
    args = [a for a in sys.argv[1:] if not a.startswith('-')]
    runs = int(args[0]) if args else 400
    calls = 0
    for seed in range(runs):
        ok, n = run(probe, seed)
        if not ok:
            sys.stdout.flush(); os._exit(1)
        calls += n
    print(f'ok: {runs} controls inits, setups and resets as the original ({calls} calls)')
    sys.stdout.flush(); os._exit(0)


if __name__ == '__main__':
    main()
