"""The ported model lighting against the original x86 code (docs/drafts/lighting.md).

    cmake --build build && .venv/bin/python tests/differential/test_lighting.py

The x87 runs at 24-bit precision, as in play (CLAUDE.md). Checked:
- world lights: every world's gamez.zbd read with the original 0x4ff520, then
  0x4f2f60 (0x4fb310 / 0x4fb0c0 on each light: position, angles, directions)
  and 0x4f2f30 (the light list 0x543b60) under random views, Direct3D and
  software; the port reads the same file and runs its versions.
- the light list on random light sets (inactive nodes, more than 128 lights,
  software colours).
- 0x543d90 (lights reaching a bounding sphere) and 0x53daa0 (the model's
  needs bits and the colour mix) on random lights, spheres and stack tops.
- 0x545720 (per-polygon shading) on random polygons, normals, vertex normals,
  baked vertex light, work records and lights, and on the real sunlight of
  each world with its shipped diffuse/ambient and fluctuate settings.
- the cloud setters and sampler 0x543920..0x543ac0.
Floats are compared within 1e-6 (relative above 1), everything else exactly.
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
from original import ROOT, STACK_TOP, Original  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
P = ctypes.c_void_p
probe.probe_light_setup.argtypes = [ctypes.c_char_p, P, ctypes.c_int, ctypes.c_int, P, P, P, P]
probe.probe_light_reaching.argtypes = [P, ctypes.c_float, ctypes.c_int, ctypes.c_int, P, P, P]
probe.probe_model_needs.restype = ctypes.c_uint32
probe.probe_model_needs.argtypes = [ctypes.c_uint32, P, ctypes.c_float, P, ctypes.c_int, ctypes.c_char_p, P,
                                    ctypes.c_uint32, ctypes.c_int, P, ctypes.c_int, ctypes.c_int, P, P, P,
                                    ctypes.c_int, P]
probe.probe_light_shade.argtypes = [P, P, ctypes.c_int, P, ctypes.c_int, ctypes.c_uint32, P, ctypes.c_int, P,
                                    P, P, P, ctypes.c_int, P]
probe.probe_light_world.argtypes = [ctypes.c_char_p, P, ctypes.c_int, ctypes.c_int, P, ctypes.c_int]
probe.probe_clouds.restype = ctypes.c_float
probe.probe_clouds.argtypes = [ctypes.c_int, ctypes.c_float, ctypes.c_float, P]

# Original addresses.
BUILD, REACH, NEEDS, SHADE = 0x543b60, 0x543d90, 0x53daa0, 0x545720
WORLD_BUILD, WORLD_UPDATE = 0x4f2f30, 0x4f2f60
LIST, HDR, SW = 0x749824, 0x74980c, 0x7497c8
D3D, FOG, FOG_COLOR = 0x72f348, 0x74a22c, 0x74a234
REC, CAM, NRM_FLAG, NRM = 0x7481a8, 0x7476a4, 0x747ca4, 0x747ca8
CLOUDS, CAMERA, OFFSET, VIEW = 0x5ef860, 0x73fc00, 0x73fbb0, 0x73fbc0
TINT_ON, TINT, MASK_PTR, SINGLE = 0x74a270, 0x74a274, 0x74a2cc, 0x74a2d4
DEV_SEND, DEV_COLOR, DEV_SENT = 0x72f3f0, 0x72f3f4, 0x72f400
POINT_COLOR, SEND_MIX, MIX_FOG, ZERR = 0x5798a0, 0x579980, 0x5680c0, 0x49bfd0
SLOTS, FLAGS, PSLOT, PFLAG = 0x63d760, 0x63d7e8, 0x5eda90, 0x5eda8c
ZBD = ROOT / 'game' / 'mw3-1.2' / 'zbd'
WORLDS = ['c1', 'c2', 'c3', 'c4', 'c4b', 't1']
# Shipped "fluctuate" (period, wavelength, base, amplitude) per mission; c1 from
# render.md, the rest use the same block (docs/render.md, Cloud pattern).
FLUCTUATE = (8.0, 600.0, 0.7, 0.3)
TOL = 1e-6
VIS = 0.003921569


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


def fbits(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def floats(*xs):
    return struct.pack(f'<{len(xs)}f', *xs)


def close(a, b):
    if math.isnan(a) or math.isnan(b):
        return math.isnan(a) and math.isnan(b)
    return abs(a - b) <= TOL * max(1.0, abs(a), abs(b))


def same(a, b):
    return len(a) == len(b) and all(close(x, y) for x, y in zip(a, b))


def fa(n, xs=()):
    return (ctypes.c_float * max(n, 1))(*xs)


def ia(n, xs=()):
    return (ctypes.c_int32 * max(n, 1))(*xs)


class Emu:
    def __init__(self, cpu=None):
        cpu = self.cpu = cpu or Original()
        self.log = []
        self.reports = []
        cpu.stub_function(POINT_COLOR, lambda c: self.log.append((1,) + self.vec(c.ecx)))
        cpu.stub_function(SEND_MIX, lambda c: self.log.append((2, 0.0, 0.0, 0.0)))
        cpu.stub_function(MIX_FOG, lambda c: self.log.append((3,) + self.vec(c.ecx)))
        cpu.stub_function(ZERR, lambda c: self.reports.append((c.arg(0), c.arg(2))))
        send = cpu.code(b'\xc3')
        cpu.stub_function(send, lambda c: self.log.append((4,) + self.vec(DEV_SENT)))
        cpu.u32(DEV_SEND, send)
        self.mask = cpu.alloc(16)
        self.lights = cpu.alloc(0xd0 * 200)
        self.nodes = cpu.alloc(0xd4 * 200)
        self.arrays = cpu.alloc(8 * 200)
        self.centre = cpu.alloc(16)
        self.model = cpu.alloc(0x60)
        self.top = cpu.alloc(48)
        self.baked = cpu.alloc(4 * 64)
        self.normal = cpu.alloc(16)
        self.io = cpu.alloc(16)

    def vec(self, a):
        return struct.unpack('<3f', self.cpu.read(a, 12))

    def i32(self, a):
        return struct.unpack('<i', self.cpu.read(a, 4))[0]

    def entries(self):
        count = self.i32(HDR + 0x10)
        out = []
        for i in range(count):
            d, n, full, reach = struct.unpack('<IIii', self.cpu.read(LIST + 0x14 * i, 16))
            out += [(d - self.lights) // 0xd0, (n - self.nodes) // 0xd4, full, reach]
        return out

    def setup(self, datas, node_flags, d3d, fog_color):
        cpu = self.cpu
        n = len(node_flags)
        cpu.write(self.lights, datas)
        for i, fl in enumerate(node_flags):
            cpu.write(self.nodes + 0xd4 * i, bytes(0xd4))
            cpu.u32(self.nodes + 0xd4 * i + 0x24, fl)
            cpu.u32(self.arrays + 4 * i, self.nodes + 0xd4 * i)
            cpu.u32(self.arrays + 0x400 + 4 * i, self.lights + 0xd0 * i)
        cpu.u32(D3D, d3d)
        cpu.write(FOG_COLOR, floats(*fog_color))
        self.reports = []
        cpu.call(BUILD, ecx=self.arrays, edx=self.arrays + 0x400, stack_args=(n,))
        hdr = [self.i32(HDR + 8), self.i32(HDR + 0x10), self.i32(HDR + 0x14), len(self.reports)]
        sw = list(struct.unpack('<16f', cpu.read(SW, 0x40)))
        return hdr, self.entries(), sw

    def reaching(self, centre, radius, single, d3d, rec):
        cpu = self.cpu
        cpu.write(self.centre, floats(*centre))
        cpu.u32(SINGLE, single), cpu.u32(D3D, d3d)
        cpu.write(REC, floats(*rec))
        cpu.call(REACH, ecx=self.centre, stack_args=(fbits(radius),))
        r = self.i32_eax()
        return r, list(struct.unpack('<1024f', cpu.read(REC, 4096))), self.entries(), self.i32(HDR + 0x14)

    def i32_eax(self):
        return struct.unpack('<i', struct.pack('<I', self.cpu.uc.reg_read(UC_X86_REG_EAX)))[0]

    def stack_top(self, matrix, identity):
        cpu = self.cpu
        cpu.write(self.top, floats(*matrix))
        cpu.u32(SLOTS + 4, self.top), cpu.u32(FLAGS + 4, identity)
        cpu.u32(PSLOT, SLOTS + 4), cpu.u32(PFLAG, FLAGS + 4)

    def needs(self, flags, centre, radius, matrix, identity, fog, camera, mask, tint_on, tint, single, d3d, rec):
        cpu = self.cpu
        model = bytearray(0x60)
        struct.pack_into('<I', model, 8, flags)
        struct.pack_into('<4f', model, 0x48, *centre, radius)
        cpu.write(self.model, model)
        self.stack_top(matrix, identity)
        cpu.write(FOG, fog)
        cpu.write(CAMERA, floats(*camera))
        cpu.u32(self.mask, mask), cpu.u32(MASK_PTR, self.mask)
        cpu.u32(TINT_ON, tint_on), cpu.write(TINT, floats(*tint))
        cpu.u32(SINGLE, single), cpu.u32(D3D, d3d)
        cpu.write(REC, floats(*rec))
        self.log = []
        cpu.call(NEEDS, ecx=self.model)
        r = cpu.uc.reg_read(UC_X86_REG_EAX)
        return r, list(struct.unpack('<1024f', cpu.read(REC, 4096))), self.entries(), list(self.log)

    def shade(self, baked, normal, count, io, shadowed, needs, cam, vnorm, normals, rec, state):
        cpu = self.cpu
        if baked is not None:
            cpu.write(self.baked, floats(*baked))
        cpu.write(self.normal, floats(*normal))
        cpu.write(self.io, struct.pack('<II', *io))
        cpu.write(CAM, floats(*cam))
        cpu.u32(NRM_FLAG, vnorm)
        cpu.write(NRM, floats(*normals))
        cpu.write(REC, floats(*rec))
        cpu.u32(CLOUDS, int(state[0]))
        cpu.write(CLOUDS + 4, floats(*state[1:9]))
        cpu.write(CAMERA, floats(*state[9:21]))
        cpu.write(OFFSET, floats(*state[21:24]))
        cpu.write(TINT, floats(*state[24:28]))
        # The uninitialised slot [ebp-0x1c]: ebp = entry esp - 4.
        stale = STACK_TOP - 0x1000 - 4 * 5 - 4 - 4 - 0x1c
        cpu.write(stale, floats(*state[28:31]))
        cpu.write(DEV_COLOR, floats(*state[31:34]))
        cpu.write(DEV_SENT, floats(*state[34:37]))
        self.log = []
        cpu.call(SHADE, ecx=self.baked if baked is not None else 0, edx=self.normal,
                 stack_args=(count, self.io, self.io + 4, shadowed, needs))
        r = self.i32_eax()
        out_state = list(state)
        out_state[28:31] = self.vec(stale)
        out_state[31:34] = self.vec(DEV_COLOR)
        out_state[34:37] = self.vec(DEV_SENT)
        return (r, list(self.vec(self.normal)), list(struct.unpack('<II', cpu.read(self.io, 8))),
                list(struct.unpack(f'<{8 * count}f', cpu.read(REC, 32 * count))), out_state, list(self.log))


# ---------------------------------------------------------------- inputs

FLAG_BITS = [0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200]


def rand_light(rng, polygon_centre=(0, 0, 50)):
    d = bytearray(0xd0)
    kind = rng.random()
    if kind < 0.3:
        flags = 0x38b if rng.random() < 0.5 else 0x8 | rng.choice([0, 0x2, 0x200, 0x202, 0x100, 0x140])
    else:
        flags = 0x200 | 0x2 | 0x10
        for b in (0x4, 0x20, 0x40, 0x100):
            if rng.random() < 0.3:
                flags |= b
        if rng.random() < 0.1:
            flags &= ~rng.choice([0x2, 0x200])
    near = rng.choice([rng.uniform(0.5, 40), 32.0, 4.0, float(rng.randrange(1, 40))])
    far = near + rng.choice([rng.uniform(0.1, 200), 32.0, float(rng.randrange(1, 100))])
    if rng.random() < 0.05:
        far = near
    inv = 1 / f32(far - near) if far != near else 0.0
    vec = [rng.gauss(0, 1) for _ in range(3)]
    n = math.sqrt(sum(x * x for x in vec)) or 1
    direction = [x / n for x in vec] if rng.random() < 0.9 else vec
    pos = [polygon_centre[k] + rng.uniform(-1.5, 1.5) * far for k in range(3)]
    struct.pack_into('<3f', d, 0x78, *pos)
    struct.pack_into('<3f', d, 0x90, *direction)
    diffuse = rng.choice([rng.uniform(0, 1), 1.0, 0.7, 0.2, rng.uniform(-0.2, 1.5)])
    ambient = rng.choice([rng.uniform(0, 1), 0.0, 0.4, 0.35, rng.uniform(-0.2, 1.2)])
    struct.pack_into('<5f', d, 0x9c, diffuse, ambient, *(rng.uniform(0, 1) for _ in range(3)))
    struct.pack_into('<I', d, 0xb0, flags)
    struct.pack_into('<5f', d, 0xb4, near, far, f32(near * near), f32(far * far), inv)
    return bytes(d)


def rand_set(rng, n=None, centre=(0, 0, 50)):
    n = rng.choice([0, 1, 1, 2, 3, 5, 8]) if n is None else n
    datas = b''.join(rand_light(rng, centre) for _ in range(n))
    nodes = [0x108011c if rng.random() < 0.9 else 0x1080118 for _ in range(n)]
    return datas, nodes


def rand_matrix(rng):
    rot = [rng.uniform(-math.pi, math.pi) for _ in range(3)]
    sa, ca, sb, cb, sc, cc = (f(rot[i]) for i in range(3) for f in (math.sin, math.cos))
    m = [sb * sa * sc + cc * cb, sc * ca, sc * cb * sa - cc * sb,
         sb * sa * cc - sc * cb, cc * ca, cc * cb * sa + sc * sb,
         sb * ca, -sa, cb * ca]
    return m + [rng.uniform(-500, 500) for _ in range(3)]


def fog_blob(rng):
    near, far = rng.uniform(0, 100), rng.uniform(100, 800)
    high, low = rng.uniform(50, 150), rng.uniform(-50, 50)
    return struct.pack('<ii3f9f', rng.random() < 0.7, 1, 0.5, 0.5, 0.5, near, f32(near * near), far,
                       f32(far * far), 1 / f32(far - near), high, low, 1 / f32(high - low), 0.0)


def rand_polygon(rng, count):
    cx, cy, cz = rng.uniform(-50, 50), rng.uniform(-50, 50), rng.uniform(5, 300)
    size = rng.choice([1.0, 10.0, 50.0])
    cam = [c + rng.uniform(-size, size) for _ in range(count) for c in (cx, cy, cz)]
    return (cx, cy, cz), cam


def unit(rng):
    v = [rng.gauss(0, 1) for _ in range(3)]
    n = math.sqrt(sum(x * x for x in v)) or 1
    return [x / n for x in v]


def rand_state(rng, clouds=None):
    period, wave, base, amp = FLUCTUATE if rng.random() < 0.5 else (
        rng.uniform(1, 20), rng.uniform(50, 1000), rng.uniform(0, 1), rng.uniform(0, 0.5))
    on = (rng.random() < 0.5) if clouds is None else clouds
    st = [float(on), f32(2 * math.pi / period), f32(2 * math.pi / wave), base, amp, 0.0, 1.0,
          rng.uniform(0, 6.28), rng.uniform(0, 6.28)]
    st += rand_matrix(rng)
    st += [rng.uniform(-5, 5) for _ in range(3)]
    st += [rng.uniform(0, 1) for _ in range(3)] + [rng.uniform(0, 1)]
    st += [rng.gauss(0, 1) for _ in range(3)]
    dev = [rng.choice([0.0, 255.0, rng.uniform(0, 255)]) for _ in range(3)]
    st += dev + (dev if rng.random() < 0.3 else [rng.uniform(0, 255) for _ in range(3)])
    return [f32(x) for x in st]


# ---------------------------------------------------------------- checks

class Fail(Exception):
    pass


def port_setup(datas, nodes, d3d, fog_color):
    hdr, ent, sw = ia(4), ia(4 * 200), fa(16)
    probe.probe_light_setup(datas, ia(len(nodes), nodes), len(nodes), d3d, fa(3, fog_color), hdr, ent, sw)
    return list(hdr), list(ent)[:4 * hdr[1]], list(sw)


def check_setup(emu, rng, datas, nodes, d3d, fog_color):
    want = emu.setup(datas, nodes, d3d, fog_color)
    got = port_setup(datas, nodes, d3d, fog_color)
    if want[0] != got[0] or want[1] != got[1] or not same(want[2], got[2]):
        raise Fail(f'light list: {want} vs {got}')
    return want[0][1]


def check_reaching(emu, rng, d3d, count, datas=b''):
    centre = [rng.uniform(-60, 60), rng.uniform(-60, 60), rng.uniform(-10, 200)]
    radius = rng.choice([0.0, rng.uniform(0, 30), rng.uniform(0, 300)])
    if datas and rng.random() < 0.3:
        # Exact boundaries: a directed light's distance is the centre's z, so
        # z + r = near or z - r = far can be hit exactly with whole numbers.
        k = rng.randrange(len(datas) // 0xd0)
        near, far = struct.unpack_from('<2f', datas, 0xd0 * k + 0xb4)
        centre[2] = float(rng.randrange(-20, 20))
        radius = (near - centre[2]) if rng.random() < 0.5 else (centre[2] - far)
        if radius < 0:
            centre[2], radius = (near - 1.0, 1.0) if rng.random() < 0.5 else (far + 1.0, 1.0)
    single = int(rng.random() < 0.3)
    rec = [f32(rng.uniform(-1, 2)) for _ in range(1024)]
    want = emu.reaching(centre, radius, single, d3d, rec)
    rb, ent, di = fa(1024, rec), ia(800), ia(1)
    r = probe.probe_light_reaching(fa(3, centre), radius, single, d3d, rb, ent, di)
    got = (r, list(rb), list(ent)[:4 * count], di[0])
    if want[0] != got[0] or want[2] != got[2] or want[3] != got[3] or not same(want[1], got[1]):
        bad = [i for i in range(1024) if not close(want[1][i], got[1][i])][:4]
        raise Fail(f'reaching: {want[0]} {want[2]} {want[3]} vs {got[0]} {got[2]} {got[3]}; rec {bad}')
    return want[0]


def check_needs(emu, rng, d3d, count):
    flags = rng.choice([0, 1, 2, 3, 0x23, 0x13])
    centre = [rng.uniform(-20, 20) for _ in range(3)]
    radius = rng.uniform(0, 100)
    matrix = rand_matrix(rng)
    matrix[11] = rng.uniform(-50, 300)
    identity = int(rng.random() < 0.2)
    fog, camera = fog_blob(rng), rand_matrix(rng)
    mask = rng.choice([0, 1, 1, 3])
    tint_on = int(rng.random() < 0.4)
    tint = [rng.uniform(0, 1) for _ in range(4)]
    single = int(rng.random() < 0.2)
    rec = [f32(rng.uniform(0, 1)) for _ in range(1024)]
    want = emu.needs(flags, centre, radius, matrix, identity, fog, camera, mask, tint_on, tint, single, d3d, rec)
    rb, ent, log, n = fa(1024, rec), ia(800), fa(64), ia(1)
    r = probe.probe_model_needs(flags, fa(3, centre), radius, fa(12, matrix), identity, fog, fa(12, camera), mask,
                                tint_on, fa(4, tint), single, d3d, rb, ent, log, 16, n)
    glog = [tuple(log[4 * i:4 * i + 4]) for i in range(n[0])]
    got = (r, list(rb), list(ent)[:4 * count], glog)
    if want[0] != got[0] or want[2] != got[2] or not same(want[1], got[1]) or len(want[3]) != len(got[3]) \
            or not all(same(a, b) for a, b in zip(want[3], got[3])):
        raise Fail(f'needs: {want[0]} {want[2]} {want[3]} vs {got[0]} {got[2]} {got[3]}')
    return want[0]


def check_shade(emu, rng, count=None, state=None, baked_ok=True, label='shade'):
    count = count or rng.choice([1, 3, 3, 4, 4, 5, 6, 8, 12, 20])
    centre, cam = rand_polygon(rng, count)
    normal = unit(rng) if rng.random() < 0.6 else [rng.gauss(0, 3) for _ in range(3)]
    vnorm = int(rng.random() < 0.3)
    normals = [x for _ in range(count) for x in unit(rng)]
    baked = [f32(rng.uniform(0, 1.2)) for _ in range(count)] if baked_ok and rng.random() < 0.3 else None
    io = [rng.choice([0, 0, 1, 1, 2, 3]), rng.choice([0, 0, 4, 1, 2, 4 | 2])]
    shadowed = int(rng.random() < 0.3)
    needs = rng.choice([2, 2, 2, 3, 6, 4, 0, 7])
    rec = [f32(rng.uniform(-0.5, 1.5)) for _ in range(8 * count)]
    state = state or rand_state(rng)
    cam = [f32(x) for x in cam]
    normal = [f32(x) for x in normal]
    want = emu.shade(baked, normal, count, io, shadowed, needs, cam, vnorm, normals, rec, state)
    nb, iob, rb, sb = fa(3, normal), (ctypes.c_uint32 * 2)(*io), fa(8 * count, rec), fa(37, state)
    log, n = fa(64), ia(1)
    r = probe.probe_light_shade(fa(count, baked) if baked is not None else None, nb, count, iob, shadowed, needs,
                                fa(3 * count, cam), vnorm, fa(3 * count, normals), rb, sb, log, 16, n)
    glog = [tuple(log[4 * i:4 * i + 4]) for i in range(n[0])]
    got = (r, list(nb), list(iob), list(rb), list(sb), glog)
    names = ('result', 'normal', 'flags', 'records', 'state', 'calls')
    for k in range(6):
        a, b = want[k], got[k]
        ok = a == b if k in (0, 2) else (len(a) == len(b) and all(same(x, y) for x, y in zip(a, b))) if k == 5 \
            else same(a, b)
        if not ok:
            raise Fail(f'{label} {names[k]}: {a} vs {b} (count {count}, needs {needs}, io {io}, '
                       f'baked {baked is not None}, vnorm {vnorm}, shadowed {shadowed})')
    return want


def check_world(emu_world, cpu, world, rng, runs):
    """One read of the world with the original, then each (d3d, none) run under
    a random view against a fresh read in the port."""
    path = ZBD / world / 'gamez.zbd'
    emu_world.read_zbd(path.read_bytes(), str(path).encode())
    i32 = emu_world.i32
    bs, nblocks, cap = i32(0x5ea830), i32(0x60bf30), i32(0x60bf34)
    blocks = [i32(i32(0x60bf38) + 4 * b) & 0xffffffff for b in range(nblocks)]
    world_node = None
    for i in range(cap):
        a = blocks[i // bs] + (i % bs) * 0xd4
        h = cpu.read(a, 0xd4)
        if struct.unpack_from('<I', h, 0xd0)[0] & 0x1000000 and struct.unpack_from('<i', h, 0x34)[0] == 2:
            world_node = a
            break
    wd = i32(world_node + 0x38) & 0xffffffff
    n = i32(wd + 0xa0)
    lp, dp = i32(wd + 0xa4) & 0xffffffff, i32(wd + 0xa8) & 0xffffffff
    nodes = [i32(lp + 4 * k) & 0xffffffff for k in range(n)]
    datas = [i32(dp + 4 * k) & 0xffffffff for k in range(n)]
    base = cpu.alloc(48)
    lights = None
    for d3d, none in runs:
        view = rand_matrix(rng)
        cpu.write(VIEW, floats(*view))
        cpu.u32(SLOTS + 4, base), cpu.u32(FLAGS + 4, 0), cpu.u32(PSLOT, SLOTS + 4), cpu.u32(PFLAG, FLAGS + 4)
        cpu.u32(D3D, d3d)
        cpu.call(WORLD_UPDATE, ecx=world_node)
        cpu.call(WORLD_BUILD, ecx=world_node, edx=none)
        lights = [cpu.read(d, 0xc8) for d in datas]
        count = i32(HDR + 0x10)
        entries = []
        for i in range(count):
            d, nd = struct.unpack('<II', cpu.read(LIST + 0x14 * i, 8))
            entries.append(next((k for k in range(n) if datas[k] == d and nodes[k] == nd), -1))
        want = ([i32(HDR + 8), count, i32(HDR + 0x14)], entries, list(struct.unpack('<16f', cpu.read(SW, 64))))
        buf = ctypes.create_string_buffer(1 << 16)
        size = probe.probe_light_world(str(path).encode(), fa(12, view), none, d3d, buf, len(buf))
        if size < 0:
            raise Fail(f'{world}: probe_light_world {size}')
        b = buf.raw[:size]
        gn = struct.unpack_from('<i', b, 0)[0]
        glights = [b[4 + 0xc8 * k:4 + 0xc8 * (k + 1)] for k in range(gn)]
        o = 4 + 0xc8 * gn
        ghdr = list(struct.unpack_from('<3i', b, o))
        gent = list(struct.unpack_from(f'<{ghdr[1]}i', b, o + 12))
        gsw = list(struct.unpack_from('<16f', b, o + 12 + 4 * ghdr[1]))
        if gn != n or want[0] != ghdr or want[1] != gent or not same(want[2], gsw):
            raise Fail(f'{world}: list {want} vs {ghdr, gent, gsw}')
        for k in range(n):
            wf = struct.unpack('<50f', lights[k])
            gf = struct.unpack('<50f', glights[k])
            for j in range(50):
                if j == 0xb0 // 4:  # flags: compare the word
                    if lights[k][0xb0:0xb4] != glights[k][0xb0:0xb4]:
                        raise Fail(f'{world} light {k} flags')
                elif not close(wf[j], gf[j]):
                    raise Fail(f'{world} light {k} +{4 * j:#x}: {wf[j]} vs {gf[j]}')
    return lights


def check_clouds(emu, rng):
    cpu = emu.cpu
    n = 0
    for _ in range(400):
        st = [float(rng.random() < 0.8), f32(rng.uniform(0, 2)), f32(rng.uniform(0, 0.1)), rng.uniform(0, 1),
              rng.uniform(0, 0.5), 0.0, 1.0, rng.uniform(0, 6.3), rng.uniform(0, 6.3)]
        st = [f32(x) for x in st]
        op = rng.randrange(5)
        a = rng.choice([rng.uniform(-2, 2), rng.uniform(-1000, 1000), 0.0, 1.0, -0.5, 1.5])
        b = rng.choice([rng.uniform(-2, 2), rng.uniform(-1000, 1000), 0.0, 0.5])
        if op in (2, 3) and a == 0.0:
            a = 3.0
        cpu.u32(CLOUDS, int(st[0]))
        cpu.write(CLOUDS + 4, floats(*st[1:]))
        addr = {0: 0x543960, 1: 0x5439f0, 2: 0x543920, 3: 0x543940, 4: 0x543ac0}[op]
        args = (fbits(a), fbits(b)) if op in (0, 4) else (fbits(a),)
        if op == 4:
            stub = cpu.code(b'\xff\x74\x24\x08\xff\x74\x24\x08\xe8' + struct.pack('<i', addr - (cpu.code_next + 13))
                            + b'\xd9\x1d' + struct.pack('<I', emu.centre) + b'\xc3') if not hasattr(emu, 'sample') \
                else emu.sample
            emu.sample = stub
            cpu.call(stub, stack_args=args)
            want_r = struct.unpack('<f', cpu.read(emu.centre, 4))[0]
        else:
            cpu.call(addr, stack_args=args)
            want_r = 0.0
        want = [float(emu.i32(CLOUDS))] + list(struct.unpack('<8f', cpu.read(CLOUDS + 4, 32)))
        sb = fa(9, st)
        got_r = probe.probe_clouds(op, a, b, sb)
        if not close(want_r, got_r) or not same(want, list(sb)):
            raise Fail(f'clouds op {op} ({a}, {b}): {want_r} {want} vs {got_r} {list(sb)}')
        n += 1
    return n


def main():
    rng = random.Random(0x5ec7)
    sys.path.insert(0, str(Path(__file__).parent))
    from test_gamez import Emulated  # noqa: E402
    world_emu = Emulated()
    world_emu.zerr_stubbed = True  # Emu stubs 0x49bfd0 itself
    world_emu.reports = []
    cpu = world_emu.cpu
    emu = Emu(cpu)
    results = []
    try:
        # World lights through the original reader.
        suns = {}
        wcases = 0
        runs = [(1, 0), (0, 0), (1, 0), (0, 1)] * 3
        for world in WORLDS:
            suns[world] = check_world(world_emu, cpu, world, rng, runs)
            wcases += len(runs)
        results.append(f'world lights: {wcases} frames over the six worlds identical')

        # The light list on random sets, including more than 128 lights.
        list_cases = 0
        for i in range(400):
            n = 140 if i % 50 == 0 else None
            datas, nodes = rand_set(rng, n)
            check_setup(emu, rng, datas, nodes, rng.choice([0, 1]), [rng.uniform(0, 1) for _ in range(3)])
            list_cases += 1
        results.append(f'light list: {list_cases} sets identical')

        reach_cases = needs_cases = shade_cases = 0
        cover = {}
        lit = 0
        for i in range(1500):
            d3d = int(rng.random() < 0.7)
            datas, nodes = rand_set(rng)
            count = check_setup(emu, rng, datas, nodes, d3d, (0.5, 0.5, 0.5))
            check_reaching(emu, rng, d3d, count, datas)
            reach_cases += 1
            check_needs(emu, rng, d3d, count)
            needs_cases += 1
            # The entries now hold the last model's reach: shade against them.
            for _ in range(3):
                w = check_shade(emu, rng)
                lit += w[0]
                shade_cases += 1
                for kind, *_ in w[5]:
                    cover[f'call{kind}'] = cover.get(f'call{kind}', 0) + 1
                for bit in (1, 4, 8):
                    if w[2][0] & bit:
                        cover[f'draw{bit}'] = cover.get(f'draw{bit}', 0) + 1
                for bit in (1, 2):
                    if w[2][1] & bit:
                        cover[f'vary{bit}'] = cover.get(f'vary{bit}', 0) + 1
        results.append('coverage: ' + ', '.join(f'{k} {v}' for k, v in sorted(cover.items())))
        results.append(f'lights reaching: {reach_cases}, model needs: {needs_cases}, '
                       f'shading: {shade_cases} random polygons ({lit} lit) identical')

        # Real suns (after the update above) with the shipped values.
        real = 0
        for world, lights in suns.items():
            datas = b''.join(l + bytes(8) for l in lights)
            nodes = [0x108011c] * len(lights)
            check_setup(emu, rng, datas, nodes, 1, (0.5, 0.5, 0.5))
            for _ in range(200):
                centre = [rng.uniform(-60, 60), rng.uniform(-60, 60), rng.uniform(5, 300)]
                check_reaching(emu, rng, 1, len(lights))
                st = rand_state(rng, clouds=rng.random() < 0.7)
                period, wave, base, amp = FLUCTUATE
                st[1:5] = [f32(2 * math.pi / period), f32(2 * math.pi / wave), base, amp]
                check_shade(emu, rng, state=[f32(x) for x in st], label=f'{world} sun')
                real += 1
        results.append(f'shipped suns: {real} polygons identical')
        results.append(f'clouds: {check_clouds(emu, rng)} setter/sampler calls identical')
    except Fail as e:
        print('FAIL', e)
        sys.exit(1)
    for r in results:
        print(r)
    print('lighting: all identical at 24-bit x87')


if __name__ == '__main__':
    main()
