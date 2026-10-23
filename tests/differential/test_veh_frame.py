"""The track vehicles' frame step against the original x86 (docs/vehicles.md,
The frame): 0x4a12c0 (the heading, the turn and its rate, the yaw, the
matrix, the speed or the slide, the world velocity and the move, the points,
the suspension 0x4a1760 with all its own pieces, the shadow, the node, the
tracks' scroll) and 0x4a1240 over it (the orientation 0x525a50 and the
entity's velocities), on test_veh_suspension.py's vehicles with the base
controls' vtables (0x5c8b58, IControls 0x5c8aa0: the real getters, the
turn step 0x4a2a00, vt+0x20 0) and the collisions 0x49ff80 stubbed.

    cmake --build build && .venv/bin/python tests/differential/test_veh_frame.py [runs]

Compared: the traces, the suspension's fields (as its test), and the yaw,
turn rate, velocities, the last world velocity and forward, the collision
points (both copies), the last matrix, the entity's orientation and
velocities, the shadow and slide flags.
"""
import ctypes
import math
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_veh_suspension as S  # noqa: E402
from calltrace import compare, load_probe, CALLBACK  # noqa: E402

BASE_VT, BASE_VBT = 0x5c8b58, 0x5c8b88
# The supports' and the position's floats come out of sums near 0 of the
# matrix's products: an ulp of its sin or cos is several of theirs.
NEAR = {'0x004ee2f0.probe': (0, 1, 2), 'vt+0x18': (1, 2, 3), '0x00526ae0': (3,), '0x004fbd80': (1, 2, 3)}


def traces_match(name, a, b):
    def close(x, y):
        if x == y:
            return True
        if x[0] != y[0] or x[0] not in NEAR or len(x[1]) != len(y[1]):
            return False
        for i, (p, q) in enumerate(zip(x[1], y[1])):
            if p == q:
                continue
            if i not in NEAR[x[0]]:
                return False
            fp, fq = S.bf(p), S.bf(q)
            if not abs(fp - fq) <= max(1e-4, 4e-6 * abs(fp)):
                return False
        return True
    if len(a) == len(b) and all(close(x, y) for x, y in zip(a, b)):
        return True
    return compare(name, a, b)  # reports the first split


def run(probe, seed, entry):
    extra = {}

    def prep(e, r, t, v, ctl):
        mb = ctl + 0x764
        e.w32(ctl, BASE_VT)
        e.w32(ctl + 4, BASE_VBT)  # IControls (0x5c8aa0) at +4 + vbtable[1]
        ictl = ctl + 4 + e.r32(BASE_VBT + 4)
        e.w32(ictl, 0x5c8aa0)
        f = [S.f32(r.uniform(-1, 1)) if r.random() < 0.7 else 0.0, S.f32(r.uniform(-1, 1)) if r.random() < 0.7 else 0.0]
        h = r.uniform(-math.pi, math.pi)
        f += [S.f32(-math.sin(h)), S.f32(-math.cos(h)), S.f32(r.uniform(-2, 2))]
        f += [S.f32(r.uniform(-3, 3)) if r.random() < 0.5 else 0.0, S.f32(r.uniform(-30, 30))]
        f += [S.f32(r.uniform(-1, 1)) for _ in range(3)]
        f += [r.choice([10.0, 40.0]), r.choice([0.6, 2.5]), r.choice([30.0, 20.0]), r.choice([30.0, 20.0]),
              r.choice([30.0, 20.0]), r.choice([10000.0, 40.0]), r.choice([10.0, 2.0])]
        f += [S.f32(r.uniform(-4, 4)) for _ in range(36)]
        shadow = e.world_alloc(0x40) if r.random() < 0.8 else 0
        tracks = [e.world_alloc(0x40), e.world_alloc(0x40)] if r.random() < 0.7 else [0, 0]
        u = [3, int(r.random() < 0.2), int(r.random() < 0.3), r.choice([0, 1]), r.randrange(13), shadow] + tracks
        k = 0
        for off in (8, 0xc, 0x748, 0x750, 0x54, 0x70, 0x78, 0x314, 0x318, 0x31c):
            e.w32(ctl + off, S.fb(f[k]))
            k += 1
        for off in (4, 0xc, 0x14, 0x18, 0x1c, 0x58, 0x5c):
            e.w32(mb + off, S.fb(f[k]))
            k += 1
        for i in range(36):
            e.w32(mb + 0x194 + 4 * i, S.fb(f[k + i]))
        e.w32(mb, u[0])
        e.w32(ctl + 0x34, u[1])
        e.w32(ctl + 0x760, u[2])
        e.w32(ctl + 0x24, u[3])
        e.w32(ctl + 0x214, u[4])
        e.w32(ctl + 0x70c, u[5])
        frozen = int(r.random() < 0.08)
        e.w32(ctl + 0x28, frozen)
        yaw = S.f32(r.uniform(-7, 7)) if r.random() < 0.2 else S.f32(r.uniform(-3, 3))
        e.w32(ctl + 0x330, S.fb(yaw))
        probe.probe_vf_more(frozen, yaw)
        e.w32(mb + 0x294, u[6])
        e.w32(mb + 0x298, u[7])
        e.stub(0x49ff80, spec='c')
        e.stub(0x4eb170, spec='c,d')
        e.stub(0x4fbd80, spec='c,s0,s1,s2')
        e.stub(0x5a85c0, spec='c')
        e.stub(0x4eca00, spec='c')
        models = {u[6]: e.world_alloc(0x20), u[7]: e.world_alloc(0x20)}

        def node_model(em, key, a, pop):
            m = models.get(a[0], 0)
            em.w32(a[1], m)
            em.t.handle(key, (a[0],))
            em.t.override(key, m)
            em._ret(0, pop)
        e.stub(0x4eb420, spec='c,d', fn=node_model)
        e.stub(0x526ae0, spec='c,d,s0,s1')
        e.stub(0x541ad0, spec='c')
        probe.probe_vf_extra((ctypes.c_float * len(f))(*f), (ctypes.c_uint32 * 8)(*u))
        extra.update(ctl=ctl, v=v)

    x = S.build(probe, seed, prep)
    e, ctl, v = x['e'], x['ctl'], x['v']
    e.call(entry, ecx=v)
    probe.probe_vf_run(0 if entry == 0x4a12c0 else 1)
    name = f'frame seed {seed} ({entry:#x}, dt {x["dt"]:.3f})'
    if not traces_match(name, x['t'].log, x['rep'].log):
        return False
    # A support's speed is (ground - y) / dt: an ulp of the matrix (sin, cos)
    # becomes 1e-6 of it.
    if not S.check_image(probe, name, x, rel=4e-6):
        return False
    w = [S.bf(e.r32(ctl + o)) for o in (0x320, 0x324, 0x328, 0x330, 0x54, 0x64, 0x6c, 0x70, 0x78, 0x7c, 0x80, 0x84, 0x314, 0x318, 0x31c)]
    w += [S.bf(e.r32(ctl + 0xac + 4 * i)) for i in range(45)]
    w += [S.bf(e.r32(ctl + 0x160 + 4 * i)) for i in range(45)]
    w += [S.bf(e.r32(ctl + 0x278 + 4 * i)) for i in range(12)]
    w += [S.bf(e.r32(v + 0x48 + 4 * i)) for i in range(4)]
    w += [S.bf(e.r32(v + 0x30 + 4 * i)) for i in range(3)] + [S.bf(e.r32(v + 0x3c + 4 * i)) for i in range(3)]
    got, gu = (ctypes.c_float * len(w))(), (ctypes.c_uint32 * 3)()
    probe.probe_vf_image(got, gu)
    names = ['angles0.x', 'angles0.y', 'angles0.z', 'yaw', 'turn rate', 'velocity.x', 'velocity.z', 'x70.x', 'x70.z', 'x7c.x', 'x7c.y', 'x7c.z', 'x314.x',
             'x314.y', 'x314.z'] + [f'point {i // 3}.{"xyz"[i % 3]}' for i in range(45)] + \
            [f'copy {i // 3}.{"xyz"[i % 3]}' for i in range(45)] + [f'last matrix[{i}]' for i in range(12)] + \
            [f'orientation[{i}]' for i in range(4)] + ['entity v.x', 'entity v.y', 'entity v.z', '+0x3c.x', '+0x3c.y', '+0x3c.z']
    if entry != 0x4a1240:
        w, got = w[:-10], list(got)[:-10]
    for i, (a, b) in enumerate(zip(w, got)):
        if not S.same(a, b, 4e-6):
            print(f'{name}: {names[i]} original {a} rebuild {b}')
            return False
    wu = [e.r32(ctl + 0x24), e.r32(ctl + 0x34), e.r32(ctl + 0x760)]
    if wu != list(gu):
        print(f'{name}: x24 / sliding / turning original {wu} rebuild {list(gu)}')
        return False
    return True


def run_frame(probe, seed):
    """0x4a1040 with its controls' answers scripted, the movement stubbed;
    the motion animation 0x4addc0 and the weapons 0x4b4880 the original's."""
    from calltrace import Emu, Replay, Trace, rnd
    r = rnd(seed, 'frame')
    t = Trace()
    script = t.script
    e = Emu(t)
    e.heap_stubs()
    e.cpu.fpu_control(0x007f)
    v, ctl, node = e.world_alloc(0x100), e.world_alloc(0xa70), e.world_alloc(0x40)
    e.cpu.write(v, b'\0' * 0x100)
    e.cpu.write(ctl, b'\0' * 0xa70)
    e.w32(v, 0x5c88e8)
    e.w32(v + 4, node)
    e.cpu.write(v + 8, bytes([1]))
    ammo = e.world_alloc(0x40) if r.random() < 0.7 else 0
    e.w32(v + 0xd4, ammo)
    e.w32(v + 0xe4, ctl)
    entry = e.world_alloc(0xc)
    e.w32(v + 0xec, entry)
    defn = e.world_alloc(0x10) if r.random() < 0.8 else 0
    anim = e.world_alloc(0x10) if r.random() < 0.5 else 0
    now = r.uniform(0, 500)
    until = S.f32(now + r.uniform(-1, 1))
    e.w32(v + 0xf0, defn)
    e.w32(v + 0xf4, anim)
    e.w32(v + 0xf8, S.fb(until))
    e.w32(0x60991c, S.fb(now))
    pvt, ivt, vb = e.world_alloc(0x40), e.world_alloc(0x100), e.world_alloc(0x10)
    e.w32(ctl, pvt)
    e.w32(pvt + 0x1c, e.new_stub('ctl.vt+0x1c', '', 0))
    e.w32(ctl + 4, vb)
    e.w32(vb + 4, 0xa04)
    e.w32(ctl + 0xa08, ivt)
    e.w32(ivt + 0xc, e.new_stub('ctl.ictl.vt+0xc', 's0', 4))
    e.w32(ivt + 0x1c, 0x4a5b50)  # GetThrottle, the controls' own
    e.w32(ivt + 0x70, e.new_stub('ctl.ictl.vt+0x70', '', 0))
    script['ctl.ictl.vt+0xc'] = r.choice([0, 1, 1, 1, 2])
    script['ctl.vt+0x1c'] = r.choice([0, 0, 0, 1])
    script['ctl.ictl.vt+0x70'] = r.choice([0, 1, 1])
    throttle = S.f32(r.choice([0.0, 0.5, 0.9, 0.95, 1.0, -1.0, r.uniform(-1, 1)]))
    e.w32(ctl + 8, S.fb(throttle))
    fire = r.choice([0, 1, 2])
    e.w32(ctl + 0x390, fire)
    pilot = e.world_alloc(0x40)
    mech = e.world_alloc(0x40) if r.random() < 0.7 else 0
    e.w32(pilot + 0x14, mech)
    e.w32(0x60861c, pilot)
    lock = e.world_alloc(0x10)
    for i in range(3):
        e.w32(lock + 4 * i, S.fb(S.f32(r.uniform(-900, 900))))
    e.stub(0x5a8630, spec='c')
    script['0x005a8630'] = lock
    e.stub(0x4a1240, spec='c')
    e.stub(0x508c10, spec='c,d,s0,s1,s2')
    script['0x00508c10'] = e.world_alloc(0x10)
    e.stub(0x506ca0, spec='c')
    e.stub(0x4b0c20, spec='c,s0', fn=lambda em, k, a, pop: (em.t.handle(k, (a[0], a[1] & 0xff)), em._ret(0, pop)))
    e.stub(0x4b0c30, spec='c,s0')
    e.stub(0x5a85c0, spec='c')
    dt = S.f32(r.choice([0.033, 0.1]))
    probe.probe_vfr_setup(v, ctl, node, throttle, fire, ammo, defn, anim, until, entry)
    rep = Replay(t, e, e.snapshot())
    probe.probe_app_set_callback(rep.callback())
    e.call(0x4a1040, ecx=v, stack=(S.fb(dt),))
    probe.probe_vfr_run(dt)
    name = f'frame seed {seed} (throttle {throttle:.2f})'
    if not compare(name, t.log, rep.log):
        return False
    want = [e.r32(v + 8) & 0xff, int(e.r32(v + 0xec) != 0), e.r32(v + 0xf4), e.r32(v + 0xf8)] + \
        [e.r32(ctl + 0x6f0 + 4 * i) for i in range(3)]
    got = (ctypes.c_uint32 * 7)()
    probe.probe_vfr_image(got)
    if want != list(got):
        print(f'{name}: original {[hex(x) for x in want]} rebuild {[hex(x) for x in got]}')
        return False
    return True


def main():
    probe = load_probe()
    probe.probe_app_set_callback.argtypes = [CALLBACK]
    probe.probe_vs_setup.argtypes = [ctypes.c_uint32] * 4 + [ctypes.c_void_p, ctypes.c_void_p]
    probe.probe_vs_image.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    probe.probe_vf_extra.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    probe.probe_vf_image.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    probe.probe_vf_more.argtypes = [ctypes.c_uint32, ctypes.c_float]
    probe.probe_vfr_setup.argtypes = [ctypes.c_uint32] * 3 + [ctypes.c_float, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_float, ctypes.c_uint32]
    probe.probe_vfr_run.argtypes = [ctypes.c_float]
    probe.probe_vfr_image.argtypes = [ctypes.c_void_p]
    probe.probe_zones_mode.argtypes = [ctypes.c_int32]
    probe.probe_vb_hooks(1)
    args = [a for a in sys.argv[1:] if not a.startswith('-')]
    runs = int(args[0]) if args else 600
    for seed in range(runs):
        if not run(probe, seed, 0x4a12c0 if seed % 3 else 0x4a1240):
            sys.stdout.flush(); os._exit(1)
    for seed in range(runs // 2):
        if not run_frame(probe, seed):
            sys.stdout.flush(); os._exit(1)
    print(f'ok: {runs} track steps and moves and {runs // 2} frames as the original')
    sys.stdout.flush(); os._exit(0)


if __name__ == '__main__':
    main()
