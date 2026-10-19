"""The ported baked static light 0x544ed0 against the original x86
(docs/drafts/bake-textures.md).

    cmake --build build && .venv/bin/python tests/differential/test_bake.py

Each world's gamez.zbd is read with the original 0x4ff520 in Unicorn and with
the port's reader; its lights are updated by the original 0x4f2f60 under a
random view (the sun's camera-space direction). Then, for a sample of the
world's models placed at random in front of the camera, both bake the model
with the real lights plus random ones (static or not, directed, point, spot;
random full/reach entries) and the polygon +0x18 arrays are compared within
1e-6 (relative above 1), at 24-bit x87 as in play. The stack slot the bake
reads before writing ([ebp-0x1c]) is seeded the same on both sides.
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
from original import ROOT, STACK_TOP  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
P = ctypes.c_void_p
probe.probe_bake_world.argtypes = [ctypes.c_char_p]
probe.probe_bake.argtypes = [ctypes.c_int, ctypes.c_uint32, P, P, P, P, ctypes.c_int, P, P, ctypes.c_int]

BAKE, WORLD_UPDATE = 0x544ed0, 0x4f2f60
LIST, COUNT, D3D = 0x749824, 0x74981c, 0x72f348
VERTS_PTR, NORMALS_PTR = 0x74639c, 0x7463a0
POOL, POOL_SIZE = 0x7402e4, 0x7402e0
VIEW, SLOTS, FLAGS, PSLOT, PFLAG = 0x73fbc0, 0x63d760, 0x63d7e8, 0x5eda90, 0x5eda8c
STALE = STACK_TOP - 0x1000 - 4 - 4 - 0x1c   # [ebp-0x1c], no stack arguments
ZBD = ROOT / 'game' / 'mw3-1.2' / 'zbd'
WORLDS = ['c1', 'c2', 'c3', 'c4', 'c4b', 't1']
MODELS_PER_WORLD = 120
TOL = 1e-6


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


def floats(*xs):
    return struct.pack(f'<{len(xs)}f', *xs)


def close(a, b):
    if math.isnan(a) or math.isnan(b):
        return math.isnan(a) and math.isnan(b)
    return abs(a - b) <= TOL * max(1.0, abs(a), abs(b))


def rotation(rng):
    a, b, c = (rng.uniform(-math.pi, math.pi) for _ in range(3))
    sa, ca, sb, cb, sc, cc = math.sin(a), math.cos(a), math.sin(b), math.cos(b), math.sin(c), math.cos(c)
    return [[cc * cb + sa * sb * sc, ca * sc, sa * cb * sc - cc * sb],
            [sa * sb * cc - sc * cb, ca * cc, sb * sc + sa * cb * cc],
            [ca * sb, -sa, ca * cb]]


def unit(rng):
    v = [rng.gauss(0, 1) for _ in range(3)]
    n = math.sqrt(sum(x * x for x in v)) or 1
    return [x / n for x in v]


def rand_light(rng, centre, size):
    d = bytearray(0xd0)
    k = rng.random()
    if k < 0.25:
        flags = 0x8 | 0x100 | rng.choice([0, 0x2, 0x200, 0x283])
    else:
        flags = 0x2 | 0x10 | 0x200 | 0x100
        if rng.random() < 0.4:
            flags |= 0x4
        if rng.random() < 0.2:
            flags |= 0x20
    if rng.random() < 0.15:
        flags &= ~0x100   # not static: the bake skips it
    near = rng.choice([rng.uniform(0.5, 2) * size, 32.0, 4.0, rng.uniform(0.1, 1) * size])
    far = near + rng.choice([rng.uniform(0.1, 3) * size, 32.0, 1.0])
    inv = 1 / f32(far - near)
    pos = [centre[i] + rng.uniform(-1.5, 1.5) * far for i in range(3)]
    direction = unit(rng) if rng.random() < 0.9 else [rng.gauss(0, 2) for _ in range(3)]
    struct.pack_into('<3f', d, 0x78, *pos)
    struct.pack_into('<3f', d, 0x90, *direction)
    diffuse = rng.choice([rng.uniform(0, 1), 1.0, 0.7, rng.uniform(-0.2, 1.5)])
    ambient = rng.choice([rng.uniform(0, 1), 0.0, 0.3, rng.uniform(-0.2, 1.2)])
    struct.pack_into('<5f', d, 0x9c, diffuse, ambient, 1.0, 1.0, 1.0)
    struct.pack_into('<I', d, 0xb0, flags)
    struct.pack_into('<5f', d, 0xb4, near, far, f32(near * near), f32(far * far), inv)
    return bytes(d)


def boundary_light(rng, cam, nvert):
    """A static point light whose near range is exactly one vertex's squared
    distance: inside near^2 the bake compares the squared distance with near,
    so this hits the near test's boundary."""
    i = rng.randrange(nvert)
    p = cam[3 * i:3 * i + 3]
    off = [rng.choice([1.5, 2.0, -2.5, 3.0]) * rng.choice([0, 1]) for _ in range(3)]
    off[0] = off[0] or 2.0
    pos = [f32(p[k] + off[k]) for k in range(3)]
    v = [f32(pos[k] - p[k]) for k in range(3)]
    d2 = f32(f32(f32(v[0] * v[0]) + f32(v[1] * v[1])) + f32(v[2] * v[2]))
    near, far = d2, f32(d2 + rng.uniform(1, 20))
    d = bytearray(rand_light(rng, p, 4.0))
    flags = 0x2 | 0x10 | 0x200 | 0x100 | (0x4 if rng.random() < 0.3 else 0)
    struct.pack_into('<3f', d, 0x78, *pos)
    struct.pack_into('<I', d, 0xb0, flags)
    struct.pack_into('<5f', d, 0xb4, near, far, f32(near * near), f32(far * far), 1 / f32(far - near))
    return bytes(d)


class Fail(Exception):
    pass


def world_lights(emu, rng):
    """The world's lights after 0x4f2f60 under a random view: their data bytes."""
    cpu, i32 = emu.cpu, emu.i32
    bs, nblocks, cap = i32(0x5ea830), i32(0x60bf30), i32(0x60bf34)
    blocks = [i32(i32(0x60bf38) + 4 * b) & 0xffffffff for b in range(nblocks)]
    world = None
    for i in range(cap):
        a = blocks[i // bs] + (i % bs) * 0xd4
        h = cpu.read(a, 0xd4)
        if struct.unpack_from('<I', h, 0xd0)[0] & 0x1000000 and struct.unpack_from('<i', h, 0x34)[0] == 2:
            world = a
            break
    wd = i32(world + 0x38) & 0xffffffff
    n = i32(wd + 0xa0)
    dp = i32(wd + 0xa8) & 0xffffffff
    datas = [i32(dp + 4 * k) & 0xffffffff for k in range(n)]
    m = [x for row in rotation(rng) for x in row] + [rng.uniform(-500, 500) for _ in range(3)]
    base = cpu.alloc(48)
    cpu.write(VIEW, floats(*m))
    cpu.u32(SLOTS + 4, base), cpu.u32(FLAGS + 4, 0), cpu.u32(PSLOT, SLOTS + 4), cpu.u32(PFLAG, FLAGS + 4)
    cpu.u32(D3D, 1)
    cpu.call(WORLD_UPDATE, ecx=world)
    return [cpu.read(d, 0xd0) for d in datas]


def check_model(emu, buf, index, real, rng, cover):
    cpu, i32 = emu.cpu, emu.i32
    m = i32(POOL) + 0x60 * index
    h = cpu.read(m, 0x60)
    flags, = struct.unpack_from('<I', h, 8)
    npoly, nvert, nnorm = struct.unpack_from('<3i', h, 0x10)
    vp, np_, pp = (struct.unpack_from('<I', h, o)[0] for o in (0x38, 0x3c, 0x34))
    verts = struct.unpack(f'<{3 * nvert}f', cpu.read(vp, 12 * nvert)) if nvert > 0 else ()
    norms = struct.unpack(f'<{3 * nnorm}f', cpu.read(np_, 12 * nnorm)) if nnorm > 0 and np_ else ()
    c = [sum(verts[k::3]) / max(nvert, 1) for k in range(3)]
    size = max([abs(verts[i] - c[i % 3]) for i in range(len(verts))] + [1.0])
    r = rotation(rng)
    t = [rng.uniform(-60, 60), rng.uniform(-60, 60), rng.uniform(5, 300)]
    cam = []
    for i in range(nvert):
        v = [verts[3 * i + k] - c[k] for k in range(3)]
        cam += [f32(v[0] * r[0][k] + v[1] * r[1][k] + v[2] * r[2][k] + t[k]) for k in range(3)]
    nrm = []
    for i in range(nnorm):
        v = norms[3 * i:3 * i + 3]
        nrm += [f32(v[0] * r[0][k] + v[1] * r[1][k] + v[2] * r[2][k]) for k in range(3)]
    lights = list(real) if rng.random() < 0.8 else []
    lights += [rand_light(rng, t, size) for _ in range(rng.choice([0, 1, 2, 3, 5]))]
    if nvert and rng.random() < 0.3:
        lights.append(boundary_light(rng, cam, nvert))
    rng.shuffle(lights)
    fr = []
    for _ in lights:
        fr += [int(rng.random() < 0.3), int(rng.random() < 0.85)]
    run_flags = rng.choice([flags, flags, flags | 0x20, flags & ~0x20])
    stale = [f32(rng.gauss(0, 1)) for _ in range(3)]

    # The original.
    cb = cpu.alloc(max(12 * nvert, 16))
    nb = cpu.alloc(max(12 * nnorm, 16))
    cpu.write(cb, floats(*cam))
    cpu.write(nb, floats(*nrm))
    cpu.u32(VERTS_PTR, cb), cpu.u32(NORMALS_PTR, nb)
    ld = cpu.alloc(0xd0 * max(len(lights), 1))
    for k, l in enumerate(lights):
        cpu.write(ld + 0xd0 * k, l)
        cpu.write(LIST + 0x14 * k, struct.pack('<IIiii', ld + 0xd0 * k, 0, fr[2 * k], fr[2 * k + 1], 0))
    cpu.u32(COUNT, len(lights))
    cpu.write(STALE, floats(*stale))
    cpu.u32(m + 8, run_flags)
    cpu.call(BAKE, ecx=m)
    cpu.u32(m + 8, flags)
    want_stale = list(struct.unpack('<3f', cpu.read(STALE, 12)))
    want = []
    polys = cpu.read(pp, 0x24 * npoly)
    for p in range(npoly):
        pf, = struct.unpack_from('<I', polys, 0x24 * p)
        scratch, = struct.unpack_from('<I', polys, 0x24 * p + 0x18)
        if pf & 0x100:
            continue
        cnt = pf & 0xff
        want += struct.unpack(f'<{cnt}f', cpu.read(scratch, 4 * cnt))
        cover['normals'] += bool(pf & 0x200) and not (run_flags & 0x20)
        cover['polygons'] += 1

    # The port.
    fa = lambda xs: (ctypes.c_float * max(len(xs), 1))(*xs)
    sb = fa(stale)
    n = probe.probe_bake(index, run_flags, fa(cam), fa(nrm), b''.join(lights) or b'\0', (ctypes.c_int32 * max(len(fr), 1))(*fr),
                         len(lights), sb, buf, len(buf))
    if n < 0:
        raise Fail(f'model {index}: probe_bake {n}')
    got = list(buf[:n])
    if len(got) != len(want):
        raise Fail(f'model {index}: {len(want)} vs {len(got)} values')
    bad = [i for i in range(n) if not close(want[i], got[i])]
    if bad:
        i = bad[0]
        raise Fail(f'model {index} (flags {run_flags:#x}, {len(lights)} lights {[hex(struct.unpack_from("<I", l, 0xb0)[0]) for l in lights]}, '
                   f'full/reach {fr}): {len(bad)} values differ, first [{i}] {want[i]} vs {got[i]}')
    if not all(close(a, b) for a, b in zip(want_stale, sb)):
        raise Fail(f'model {index}: stale vector {want_stale} vs {list(sb)}')
    for l in lights:
        fl, = struct.unpack_from('<I', l, 0xb0)
        if fl & 0x100:
            cover['directed' if fl & 8 else 'spot' if fl & 4 else 'point'] += 1
    cover['values'] += n
    cover['lit'] += sum(1 for x in want if abs(x - 1.0) > 1e-3)


def main():
    rng = random.Random(0xba4e)
    from test_gamez import Emulated  # noqa: E402
    emu = Emulated()
    buf = (ctypes.c_float * (1 << 20))()
    cover = {k: 0 for k in ('polygons', 'values', 'lit', 'normals', 'directed', 'point', 'spot')}
    models = 0
    try:
        for world in WORLDS:
            path = ZBD / world / 'gamez.zbd'
            emu.read_zbd(path.read_bytes(), str(path).encode())
            size = probe.probe_bake_world(str(path).encode())
            if size != emu.i32(POOL_SIZE):
                raise Fail(f'{world}: pool size {emu.i32(POOL_SIZE)} vs {size}')
            pool = emu.i32(POOL)
            usable = []
            for i in range(size):
                npoly, = struct.unpack('<i', emu.cpu.read(pool + 0x60 * i + 0x10, 4))
                pp, = struct.unpack('<I', emu.cpu.read(pool + 0x60 * i + 0x34, 4))
                if npoly > 0 and pp:
                    usable.append(i)
            pick = rng.sample(usable, min(MODELS_PER_WORLD, len(usable)))
            real = world_lights(emu, rng)
            for i in pick:
                check_model(emu, buf, i, real, rng, cover)
                models += 1
            print(f'{world}: {len(pick)} of {len(usable)} models identical ({len(real)} world lights)')
    except Fail as e:
        print('FAIL', e)
        sys.exit(1)
    print('coverage: ' + ', '.join(f'{k} {v}' for k, v in cover.items()))
    print(f'bake: {models} models identical at 24-bit x87')


if __name__ == '__main__':
    main()
