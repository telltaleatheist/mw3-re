"""The basic vehicle as an object against the original x86 (docs/vehicles.md):
the entry 0x4ac320 (its node the world's by name, else a named copy of the
type's put in the world), the constructor 0x4ac530 over the vehicle base
0x4aeb00, the destructor 0x4ac650 over 0x4aeb50 / 0x4d58a0, and the HUD
markers 0x4ac3d0; step 1b: the engine sounds' list 0x4ad930 / 0x4add10 and
the setup 0x49ec00 (sounds/engine, motion_anim 0x4ade40, the controls' own).

    cmake --build build && .venv/bin/python tests/differential/test_veh_basic.py [runs]

Every call out is stubbed (calltrace.py): zClass's lookups, copy and naming,
the world's add and remove, the controls' block 0x4ac860 and constructor
0x4ae270 (or the given controls' SetOwner), the camera modes 0x4ace00, the
entity's node 0x4d6c90 (writing +0x04 as the real one does) and zEntity's
list. Each run: the name found or not, the type found or not, the copy
failing, given controls or made ones (their block failing); then the
destructor with the weapons, the network's record, the motion animation and
the engine sound each set or not. Compared: the traces, whether a vehicle was
made, its 0xfc bytes after the constructor and the markers. The engine
sounds: lookups by name (repeats, unknown sounds, null names, more than the
list's 64), then the clear with voices playing. The setup: the definition's
sounds a list or not, an engine or none, a motion_anim or none (a string, 0
or not one), an animation running.
"""
import ctypes
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from unicorn.x86_const import UC_X86_REG_ECX  # noqa: E402
from calltrace import CALLBACK, Emu, Replay, Trace, compare, load_probe, rnd  # noqa: E402

U, P = ctypes.c_uint32, ctypes.c_void_p
WORLD = 0x608c84
# +0x88..+0x93: the tracked list's block (each side's own heap); +0x00 the vtable.
SKIP = set(range(0x88, 0x94)) | set(range(0, 4))


def fake_controls(e, name):
    """A controls block whose IControls base sits at the block itself
    (vbtable[1] = -4): its vt+0x00 (delete) and vt+0x08 (SetOwner)."""
    c = e.world_alloc(0x40)
    vb = e.world_alloc(0x10)
    vt = e.world_alloc(0x40)
    e.w32(c, vt)
    e.w32(c + 4, vb)
    e.w32(vb + 4, (-4) & 0xffffffff)
    e.w32(vt + 0, e.new_stub(f'{name}.ictl.vt+0x0', 'c,s0', 4))
    e.w32(vt + 8, e.new_stub(f'{name}.ictl.vt+0x8', 'c,s0', 4))
    return c


def make(seed, t):
    e = Emu(t)
    e.heap_stubs()
    r = rnd(seed, 'case')
    case = {
        'by_name': r.random() < 0.4,
        'by_type': r.random() < 0.8,
        'clone_ok': r.random() < 0.85,
        'given_ctl': r.random() < 0.5,
        'alloc_ok': r.random() < 0.85,
    }
    node = e.world_alloc(0x40)
    model = e.world_alloc(0x40)
    clone = e.world_alloc(0x40)
    made = fake_controls(e, 'ctl')
    given = fake_controls(e, 'ctl') if case['given_ctl'] else 0
    mem = e.world_alloc(0x40)
    e.cpu.write(mem, b'\0' * 0x40)

    def named(em, key, a, pop):
        n = em.t.counts.get(key, 0)
        hit = case['by_name'] if n == 0 else case['by_type']
        value = (node if n == 0 else model) if hit else 0
        em.t.handle(key, a)
        em.t.override(key, value)
        em._ret(value, pop)

    def answer(value):
        def fn(em, key, a, pop):
            em.t.handle(key, a)
            em.t.override(key, value)
            em._ret(value, pop)
        return fn

    e.stub(0x4f7fe0, spec='c,zd', fn=lambda em, k, a, p: named(em, k, a, p))
    e.stub(0x4f9af0, spec='c,d,s0', fn=answer(clone if case['clone_ok'] else 0))
    e.stub(0x4eb300, spec='c,zd')
    world = e.object('world', slots={0: ('s0', 4)})
    e.w32(WORLD, world)
    e.stub(0x4ebb40, spec='d')
    e.stub(0x4ac860, spec='c', fn=answer(mem if case['alloc_ok'] else 0))
    e.stub(0x4ae270, spec='c,s0,s1,s2', fn=answer(made))
    e.stub(0x4ace00, spec='')

    def set_node(em, key, a, pop):
        em.w32(a[0] + 4, a[1])
        em._ret(em.t.handle(key, a), pop)
    e.stub(0x4d6c90, spec='c,s0,s1,s2,s3', fn=set_node)
    e.stub(0x5a8390, spec='c,s0,s1,s2,s3', fn=set_node)
    for addr in (0x5a7f50, 0x5a7f70, 0x583400, 0x4f9050, 0x506ca0, 0x4b01d0):
        e.stub(addr, spec='c')

    def marker(em, key, a, pop):
        c = em.uc.reg_read(UC_X86_REG_ECX)
        em.t.handle(key, a)
        em.t.override(key, c)
        em._ret(c, pop)
    e.stub(0x59b830, spec='zs0,s1,s2,s3', fn=marker)
    # The engine sounds: 64 known, a few unknown to zSound.
    sounds = {}
    for k in range(80):
        nm = f'engine_{k:02d}'
        sounds[nm] = (e.world_string(nm), e.world_alloc(0x10) if k < 72 else 0)
    by_handle = {h: a for a, h in sounds.values() if h}
    e.sounds = sounds

    def sound_find(em, key, a, pop):
        h = sounds.get(a[0], (0, 0))[1] if a[0] else 0
        em.t.handle(key, a)
        em.t.override(key, h)
        em._ret(h, pop)
    e.stub(0x56f6f0, spec='zc', fn=sound_find)

    def sound_name(em, key, a, pop):
        v = by_handle.get(a[0], 0)
        em.t.handle(key, a)
        em.t.override(key, v)
        em._ret(v, pop)
    e.stub(0x56f740, spec='c', fn=sound_name)
    for addr in (0x56f410, 0x56e9f0):
        e.stub(addr, spec='c')
    e.stub(0x49bfd0, spec='s0,zs1,s2,zs3', pop=0)
    return e, case, given


def image_original(e, v):
    return bytes(e.cpu.read(v, 0xfc))


def image_probe(probe, orig):
    b = (ctypes.c_uint8 * 0xfc)(*orig)
    probe.probe_vb_image(b)
    return bytes(b)


def run(probe, seed, verbose=False):
    t = Trace()
    e, case, given = make(seed, t)
    r = rnd(seed, 'run')
    tname = r.choice(['truck1', 'apc', 'm4tank', 'hovercraft'])
    vname = r.choice(['truck01', 'apc_03', 'veh_a', tname])
    ts, vs = e.world_string(tname), e.world_string(vname)
    snap = e.snapshot()
    rep = Replay(t, e, snap)
    probe.probe_app_set_callback(rep.callback())
    name = f'seed {seed} {case}'

    e.cpu.fpu_control(0x007f)
    v = e.call(0x4ac320, ecx=given, edx=ts, stack=(vs,))
    made = probe.probe_vb_create(v, given, tname.encode(), vname.encode())
    if bool(v) != bool(made):
        print(f'{name}: made original {bool(v)} rebuild {bool(made)}')
        return False, 0
    if v:
        a = image_original(e, v)
        b = image_probe(probe, a)
        diff = [o for o in range(0xfc) if o not in SKIP and a[o] != b[o]]
        if diff:
            o = diff[0] & ~3
            print(f'{name}: the vehicle differs at +{o:#x}: original {a[o:o + 4].hex()} rebuild {b[o:o + 4].hex()}')
            return False, 0
        # The destructor, with its optional fields set alike.
        fields = [r.choice([0, e.world_alloc(0x10)]) for _ in range(4)]
        for off, val in zip((0xd4, 0xd8, 0xf4, 0xec), fields):
            e.w32(v + off, val)
        probe.probe_vb_set(*fields)
        e.cpu.fpu_control(0x007f)
        e.call(0x4ac650, ecx=v)
        probe.probe_vb_destroy()
    if not compare(name, t.log, rep.log):
        return False, 0
    if verbose:
        print(name, len(t.log), 'calls')
    return True, len(t.log)


LIST = 0x60b110


def entry_index(e, a):
    return -1 if a == 0 else (a - e.r32(LIST + 4)) // 0xc


def run_sounds(probe, seed):
    t = Trace()
    e = make(seed, t)[0]
    e.call(0x4ad840, ecx=LIST)
    r = rnd(seed, 'sounds')
    names = list(e.sounds)
    n = r.choice([10, 40, 200])
    pool = names[:r.choice([8, 30, 80])]
    seq = [r.choice(pool) if r.random() > 0.05 else None for _ in range(n)]
    held = {nm: e.world_string(nm) for nm in names}
    rep = Replay(t, e, e.snapshot())
    probe.probe_app_set_callback(rep.callback())
    probe.probe_vb_sounds_reset()
    name = f'sounds seed {seed} ({n} lookups)'
    for i, nm in enumerate(seq):
        got = e.call(0x4ad930, ecx=LIST, stack=(held[nm] if nm else 0,))
        want = entry_index(e, got)
        mine = probe.probe_vb_sound_find(nm.encode() if nm else None)
        if want != mine:
            print(f'{name}: lookup {i} ({nm}): original entry {want} rebuild {mine}')
            return False, 0
    count = (e.r32(LIST + 8) - e.r32(LIST + 4)) // 0xc
    if count != probe.probe_vb_sound_count():
        print(f'{name}: entries original {count} rebuild {probe.probe_vb_sound_count()}')
        return False, 0
    out = (U * 3)()
    for i in range(count):
        base = e.r32(LIST + 4) + 0xc * i
        probe.probe_vb_sound_entry(i, out)
        if [e.r32(base), e.r32(base + 4), e.r32(base + 8)] != list(out):
            print(f'{name}: entry {i} differs')
            return False, 0
        if r.random() < 0.3:
            v = e.world_alloc(0x10)
            e.w32(base + 8, v)
            probe.probe_vb_sound_voice(i, v)
    e.call(0x4add10)
    probe.probe_vb_sounds_clear()
    if e.r32(LIST + 8) != e.r32(LIST + 4) or probe.probe_vb_sound_count() != 0:
        print(f'{name}: not emptied')
        return False, 0
    return compare(name, t.log, rep.log), len(t.log)


def run_setup(probe, seed):
    t = Trace()
    e, case, given = make(seed, t)
    case['by_name'] = True  # a vehicle to set up
    e.call(0x4ad840, ecx=LIST)
    r = rnd(seed, 'setup')
    ctl = fake_controls(e, 'ctl')
    vt = e.r32(ctl)
    e.w32(vt + 0xc, e.new_stub('ctl.vt+0xc', 'c,s0,zs1,s2', 0xc))
    sounds_node = r.choice([0, e.world_alloc(0x10), e.world_alloc(0x10)])
    if sounds_node:
        e.w32(sounds_node, r.choice([4, 4, 3]))
    engine = r.choice([None, 'engine_03', 'engine_75', 'engine_40'])
    motion = r.choice([None, 'apc_move', 'truck_roll'])
    motion_ok = r.random() < 0.8
    strs = {x: e.world_string(x) for x in ('engine_03', 'engine_75', 'engine_40', 'apc_move', 'truck_roll')}
    anims = {'apc_move': e.world_alloc(0x10), 'truck_roll': 0}

    def answer(value):
        def fn(em, key, a, pop):
            em.t.handle(key, a)
            em.t.override(key, value)
            em._ret(value, pop)
        return fn
    e.stub(0x5569e0, spec='c,zd', fn=answer(sounds_node))
    e.stub(0x5569f0, spec='c,zd', fn=answer(strs[engine] if engine else 0))

    def string_value(em, key, a, pop):
        out = strs[motion] if motion else 0
        if motion_ok:  # 0x557030 writes only what it read
            em.w32(a[2], out)
        em.t.handle(key, a[:2])
        em.t.override(key, int(motion_ok))
        em.t.handle('@0x00557090.out', ())
        em.t.override('@0x00557090.out', out)
        em._ret(int(motion_ok), pop)
    e.stub(0x557090, spec='c,zd,s0', fn=string_value)

    def anim_find(em, key, a, pop):
        v = anims.get(a[0], 0)
        em.t.handle(key, a)
        em.t.override(key, v)
        em._ret(v, pop)
    e.stub(0x50b450, spec='zc', fn=anim_find)
    ts, vs = e.world_string('apc'), e.world_string('apc_01')
    defn = e.world_alloc(0x10)
    rep = Replay(t, e, e.snapshot())
    probe.probe_app_set_callback(rep.callback())
    probe.probe_vb_sounds_reset()
    name = f'setup seed {seed} (sounds {bool(sounds_node)}, engine {engine}, motion {motion} {motion_ok})'
    v = e.call(0x4ac320, ecx=ctl, edx=ts, stack=(vs,))
    probe.probe_vb_create(v, ctl, b'apc', b'apc_01')
    running = r.choice([0, e.world_alloc(0x10)])
    e.w32(v + 0xf4, running)
    probe.probe_vb_set(0, 0, running, 0)
    kind = r.choice([1, 3, 5])
    e.call(0x49ec00, ecx=v, stack=(defn, vs, kind))
    probe.probe_vb_setup(defn, b'apc_01', kind)
    a = image_original(e, v)
    b = image_probe(probe, a)
    diff = [o for o in range(0xfc) if o not in SKIP and a[o] != b[o]]
    if diff:
        o = diff[0] & ~3
        print(f'{name}: the vehicle differs at +{o:#x}: original {a[o:o + 4].hex()} rebuild {b[o:o + 4].hex()}')
        return False, 0
    want, mine = entry_index(e, e.r32(v + 0xec)), probe.probe_vb_engine_index()
    if want != mine:
        print(f'{name}: engine sound original {want} rebuild {mine}')
        return False, 0
    return compare(name, t.log, rep.log), len(t.log)


def markers(probe):
    t = Trace()
    e = make(0, t)[0]
    rep = Replay(t, e, e.snapshot())
    probe.probe_app_set_callback(rep.callback())
    e.call(0x4ac3d0)
    got = (U * 3)()
    probe.probe_vb_markers(got)
    want = [e.r32(a) for a in (0x60b120, 0x60b124, 0x60b128)]
    ok = compare('markers', t.log, rep.log) and list(got) == want
    if not ok:
        print(f'markers: original {[hex(x) for x in want]} rebuild {[hex(x) for x in got]}')
    return ok


def main():
    probe = load_probe()
    probe.probe_app_set_callback.argtypes = [CALLBACK]
    probe.probe_vb_create.argtypes = [U, U, ctypes.c_char_p, ctypes.c_char_p]
    probe.probe_vb_set.argtypes = [U, U, U, U]
    probe.probe_vb_image.argtypes = [P]
    probe.probe_vb_markers.argtypes = [P]
    probe.probe_vb_sound_find.argtypes = [ctypes.c_char_p]
    probe.probe_vb_sound_entry.argtypes = [ctypes.c_int, P]
    probe.probe_vb_sound_voice.argtypes = [ctypes.c_int, U]
    probe.probe_vb_setup.argtypes = [U, ctypes.c_char_p, ctypes.c_int32]
    probe.probe_vb_hooks(1)
    args = [a for a in sys.argv[1:] if not a.startswith('-')]
    runs = int(args[0]) if args else 200
    calls = 0
    for seed in range(runs):
        ok, n = run(probe, seed, verbose='-v' in sys.argv)
        if not ok:
            sys.stdout.flush(); os._exit(1)
        calls += n
    for seed in range(runs // 2):
        for part in (run_sounds, run_setup):
            ok, n = part(probe, seed)
            if not ok:
                sys.stdout.flush(); os._exit(1)
            calls += n
    if not markers(probe):
        sys.stdout.flush(); os._exit(1)
    print(f'ok: {runs} vehicles made and destroyed as the original, {runs // 2} engine-sound runs and setups ({calls} calls), the three markers alike')
    sys.stdout.flush(); os._exit(0)


if __name__ == '__main__':
    main()
