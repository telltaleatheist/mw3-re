"""The track vehicles' suspension against the original x86 (docs/vehicles.md,
The suspension): 0x4a1760 with the supports' ground 0x4a23f0 (one query
0x4ee2f0 for the four supports, the pick 0x4abd90, the zone 0x53cd20 /
0x5a80d0), the support step 0x4a2820, the plane normal 0x524020, the height
on it 0x4abf70, the forward 0x4a3e40, the matrix 0x4a3ed0, the y turn
0x523e70 and asin, all the original's own; the engine's calls stubbed.

    cmake --build build && .venv/bin/python tests/differential/test_veh_suspension.py [runs]

Each run: a vehicle with four supports at a random place and attitude,
ground under each support at random heights about it (one to three hits,
water now and then, a support with none now and then), random support
speeds and in-air flags, rates, gravity, damping and frame time. The x87
at 24 bits. Compared: the traces (the query's probes and the position set
within an ulp), the answer, the matrix, forward, up, the rates, pitch and
roll, the top speeds, the world supports, the support words (floats within
4 ulps or 1e-5), the in-air flag and the zone record.
"""
import ctypes
import math
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import CALLBACK, Emu, Replay, Trace, compare, load_probe, rnd  # noqa: E402

VEH_VT = 0x5c88e8
DT, INV_DT, ZONES_ON = 0x5e8110, 0x5e8114, 0x5ea1d0


def fb(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


def bf(b):
    return struct.unpack('<f', struct.pack('<I', b & 0xffffffff))[0]


def f32(x):
    return bf(fb(x))


def same(a, b):
    if a == b or (math.isnan(a) and math.isnan(b)):
        return True
    if math.isnan(a) or math.isnan(b) or math.isinf(a) or math.isinf(b):
        return False
    return abs(a - b) <= max(1e-5, 4 * abs(a) * 2 ** -23)


def attitude(r, airborne):
    tilt = 0.8 if r.random() < 0.25 else 0.3
    yaw, pitch, roll = r.uniform(-math.pi, math.pi), r.uniform(-tilt, tilt), r.uniform(-tilt, tilt)
    if airborne:  # about the 30-degree hold
        pitch, roll = r.choice([-1, 1]) * r.uniform(0.45, 0.65), r.choice([-1, 1]) * r.uniform(0.45, 0.65)
    cy, sy, cp, sp, cr, sr = math.cos(yaw), math.sin(yaw), math.cos(pitch), math.sin(pitch), math.cos(roll), math.sin(roll)
    # Rows: right, up, back (row-vector convention: world = local x M + T).
    fwd = [-sy * cp, sp, -cy * cp]
    up = [sy * sp * cr + cy * sr, cp * cr, cy * sp * cr - sy * sr]
    right = [fwd[1] * up[2] - fwd[2] * up[1], fwd[2] * up[0] - fwd[0] * up[2], fwd[0] * up[1] - fwd[1] * up[0]]
    return right + up + [-x for x in fwd], yaw, pitch, roll, [-sy, -cy]


def run(probe, seed):
    r = rnd(seed, 'suspension')
    t = Trace()
    script = t.script
    e = Emu(t)
    e.heap_stubs()
    e.cpu.fpu_control(0x007f)
    dt = r.choice([0.033, 0.016, 0.1, r.uniform(0.001, 0.2)])
    e.w32(DT, fb(dt))
    e.w32(INV_DT, fb(1.0 / dt))
    e.w32(ZONES_ON, 1)
    probe.probe_zones_mode(1)
    v, ctl, node, mat = e.world_alloc(0x100), e.world_alloc(0xa70), e.world_alloc(0x40), e.world_alloc(0x30)
    e.cpu.write(v, b'\0' * 0x100)
    e.cpu.write(ctl, b'\0' * 0xa70)
    e.w32(v, VEH_VT)
    e.w32(v + 4, node)
    e.w32(v + 0xc, mat)
    e.w32(v + 0xe4, ctl)
    airborne = r.random() < 0.15
    rot, yaw, pitch, roll, flat = attitude(r, airborne)
    pos = [r.uniform(-3000, 3000), r.uniform(-20, 60), r.uniform(-3000, 3000)]
    m = [f32(x) for x in rot + pos]
    sup = [[-2.25, 0.0, -2.4], [2.25, 0.0, -2.4], [2.25, 0.0, 2.34], [-2.25, 0.0, 2.34]]
    if r.random() < 0.4:
        sup = [[x * r.uniform(0.6, 1.6) for x in s] for s in sup]
    f = m + [f32(flat[0]), f32(flat[1])]
    f += [f32(r.uniform(-20, 20)), f32(r.uniform(-3, 3)), f32(r.uniform(-20, 20))]  # velocity
    f += [f32(r.uniform(-2, 2)), f32(r.uniform(-0.5, 0.5)), f32(r.uniform(-0.5, 0.5))]  # x70.y, x50 x, z
    f += [f32(pitch + r.uniform(-0.05, 0.05)), f32(yaw), f32(roll + r.uniform(-0.05, 0.05)), r.choice([60.0, 40.0, 0.5])]
    f += [r.choice([10.0, 40.0]), r.choice([2.0, 3.5]), 0.9, 0.8, r.choice([8.0, 2.0, 20.0])]
    f += [f32(x) for s in sup for x in s]
    f += [f32(r.uniform(-4, 4)) if r.random() < 0.8 else 0.0 for _ in range(4)] + [0.0] * 4
    u = [int(r.random() < 0.3) for _ in range(4)] + [0] * 4 + [int(r.random() < 0.2)]
    if airborne or r.random() < 0.15:
        u[:4] = [1, 1, 1, 1]
    put = lambda a, x: e.w32(a, fb(x))  # noqa: E731
    for i in range(12):
        put(mat + 4 * i, f[i])
    put(v + 0x24, f[12])
    put(v + 0x2c, f[13])
    k = 14
    for off in (0x64, 0x68, 0x6c, 0x74, 0x50, 0x58, 0x32c, 0x330, 0x334, 0x1c):
        put(ctl + off, f[k])
        k += 1
    mb = ctl + 0x764
    for off in (8, 0x10, 0x18c, 0x190, 0x250):
        put(mb + off, f[k])
        k += 1
    e.w32(mb + 0x84, 4)
    for i in range(12):
        put(mb + 0x8c + 4 * i, f[k + i])
    k += 12
    for i in range(8):
        put(mb + 0x14c + 4 * i, f[k + i])
        e.w32(mb + 0x16c + 4 * i, u[i])
    e.w32(ctl + 0x20, u[8])
    entity = e.world_alloc(0x10)
    for i in range(3):
        put(entity + 4 * i, pos[i])
    e.stub(0x5a85e0, spec='c')
    script['0x005a85e0'] = entity
    flag = r.choice([0, 1])

    def flag_get(em, key, a, pop):
        em.w32(a[1], flag)
        em.t.handle(key, (a[0],))
        em.t.override(key, flag)
        em._ret(0, pop)
    e.stub(0x4eb660, spec='c,d', fn=flag_get)
    e.stub(0x4eb620, spec='c,d')
    e.stub(0x4eb810, spec='c,d', fn=lambda em, k_, a, pop: (em.t.handle(k_, (a[0], a[1] & 0xff)), em._ret(0, pop)))
    view, cam, data = e.world_alloc(0x10), e.world_alloc(0x40), e.world_alloc(0x200)
    e.w32(0x5f4c8c, view)
    e.w32(view + 4, cam)
    e.w32(cam + 0x38, data)
    e.cpu.write(data + 0x1e4, bytes([r.randrange(3)] + [r.choice([3, 5, 7]) for _ in range(3)]))
    e.w32(0x608c84, e.world_alloc(0x10))
    # The supports' ground: four records, kept at `keep` for the probe too.
    keep = e.world_alloc(0x504 * 4)
    water = e.world_alloc(0x40)
    e.w32(water + 0x20, 1)
    recs = bytearray(0x504 * 4)
    world = []
    for s in sup:
        world.append([sum(s[j] * m[3 * j + c] for j in range(3)) + m[9 + c] for c in range(3)])
    missing = r.random() < 0.05
    for i in range(4):
        n = 0 if (missing and i == 2) else r.choice([1, 1, 2, 3])
        base = 0x504 * i
        recs[base] = n
        for h in range(n):
            o = base + 4 + 0x28 * h
            nx, nz = r.uniform(-0.3, 0.3), r.uniform(-0.3, 0.3)
            ln = math.sqrt(nx * nx + 1 + nz * nz)
            struct.pack_into('<3f', recs, o, nx / ln, 1 / ln, nz / ln)
            lift = r.uniform(-8, -4) if airborne else r.uniform(2, 6) if r.random() < 0.25 else r.uniform(-1.5, 1.5)  # past the pick's step now and then
            struct.pack_into('<3f', recs, o + 0xc, world[i][0], world[i][1] + lift, world[i][2])
            recs[o + 0x18:o + 0x1c] = bytes([r.randrange(4)] + [r.choice([3, 5, 7, 9]) for _ in range(3)])
            struct.pack_into('<I', recs, o + 0x20, water if r.random() < 0.15 else 0)
    e.cpu.write(keep, bytes(recs))

    def many(em, key, a, pop):
        count, top, out = a[2], a[3], a[4]
        em.t.handle(key, (count, top))
        for i in range(count):
            em.t.handle('0x004ee2f0.probe', tuple(em.r32(a[1] + 12 * i + 4 * j) for j in range(3)))
        em.cpu.write(out, bytes(recs[:0x504 * count]))
        em._ret(0, pop)
    e.stub(0x4ee2f0, spec='c,d,s0,s1,s2', fn=many)

    def set_position(em, key, a, pop):
        em.t.handle('vt+0x18', (a[0],) + tuple(em.r32(a[1] + 4 * j) for j in range(3)))
        em._ret(0, pop)
    e.stub(0x5a8550, spec='c,s0', fn=set_position)
    # The CRT's _CIasin (an import): atan(x / sqrt(1 - x^2)) on the x87.
    asin = e.alloc(32)
    e.cpu.write(asin, b'\xd9\xc0\xd8\xc8\xd9\xe8\xde\xe1\xd9\xfa\xd9\xf3\xc3')
    e.w32(0x5bb5f4, asin)
    probe.probe_vs_setup(v, ctl, node, keep, (ctypes.c_float * len(f))(*f), (ctypes.c_uint32 * 9)(*u))
    rep = Replay(t, e, e.snapshot())
    probe.probe_app_set_callback(rep.callback())
    want = e.call(0x4a1760, ecx=v)
    mine = probe.probe_vs_run()
    name = f'suspension seed {seed} (dt {dt:.3f}{", a support without ground" if missing else ""})'
    if not compare(name, t.log, rep.log, ulps={'vt+0x18': (1, 2, 3), '0x004ee2f0.probe': (0, 1, 2)}):
        return False
    if want != mine:
        print(f'{name}: answer original {want} rebuild {mine}')
        return False
    w = [bf(e.r32(mat + 4 * i)) for i in range(12)]
    w += [bf(e.r32(v + 0x18 + 4 * i)) for i in range(3)]
    w += [bf(e.r32(ctl + o)) for o in (0x68, 0x74, 0x50, 0x58, 0x32c, 0x334, 0x308, 0x30c, 0x310, 0x88, 0x8c)]
    w += [bf(e.r32(mb + 0xec + 4 * i)) for i in range(12)]
    w += [bf(e.r32(mb + 0x14c + 4 * i)) for i in range(16)]
    got, gu = (ctypes.c_float * 55)(), (ctypes.c_uint32 * 2)()
    probe.probe_vs_image(got, gu)
    names = [f'matrix[{i}]' for i in range(12)] + ['x18.x', 'x18.y', 'x18.z', 'velocity.y', 'x70.y', 'rate x', 'rate z',
             'pitch', 'roll', 'up.x', 'up.y', 'up.z', 'top speed', 'top turn'] + \
            [f'support {i // 3}.{"xyz"[i % 3]}' for i in range(12)] + [f'word {i}' for i in range(16)]
    for i, (a, b) in enumerate(zip(w, got)):
        if i >= 39 + 8:  # the in-air flags: words, exact
            if fb(a) != fb(b):
                print(f'{name}: {names[i]} original {fb(a):#x} rebuild {fb(b):#x}')
                return False
        elif not same(a, b):
            print(f'{name}: {names[i]} original {a} rebuild {b}')
            return False
    if e.r32(ctl + 0x20) != gu[0] or e.r32(v + 0x64) != gu[1]:
        print(f'{name}: in the air / zones original {e.r32(ctl + 0x20)} {e.r32(v + 0x64):#x} '
              f'rebuild {gu[0]} {gu[1]:#x}')
        return False
    return True


def main():
    probe = load_probe()
    probe.probe_app_set_callback.argtypes = [CALLBACK]
    probe.probe_vs_setup.argtypes = [ctypes.c_uint32] * 4 + [ctypes.c_void_p, ctypes.c_void_p]
    probe.probe_vs_image.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    probe.probe_zones_mode.argtypes = [ctypes.c_int32]
    probe.probe_vb_hooks(1)
    args = [a for a in sys.argv[1:] if not a.startswith('-')]
    runs = int(args[0]) if args else 600
    for seed in range(runs):
        if not run(probe, seed):
            sys.stdout.flush(); os._exit(1)
    print(f'ok: {runs} suspension steps as the original')
    sys.stdout.flush(); os._exit(0)


if __name__ == '__main__':
    main()
