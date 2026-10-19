"""The ported fog maths and transparent queue against the original x86 code.

    cmake --build build && .venv/bin/python tests/differential/test_fogsort.py

Fog: 0x555640 (a bounding sphere's fog) and 0x555a70 (fog per vertex into
0x7463a4[], then the device fog colour push) on random fog settings, camera
matrices, spheres and vertices around the ranges and the altitude band, plus
the shipped settings, the world-file dummy and exact boundaries. Factors may
differ by x87 extended vs float rounding: compared within 1e-6 (they are 0..1);
everything else exactly.
Sort: 0x57dec0 on random key arrays of many sizes and shapes; the resulting
order must be identical (the sort is not stable, so duplicates test it).
Queue: the inlined reserve block through 0x57dba0 and 0x57bf80, overflow
reports included. Flush: 0x57e110 with the Direct3D device calls logged.
"""
import ctypes
import math
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import ROOT, Original  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
probe.probe_fog_sphere.restype = ctypes.c_float
probe.probe_fog_sphere.argtypes = [ctypes.c_char_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_float]

FOG, MATRIX, VERTS_PTR, FACTOR = 0x74a22c, 0x73fc00, 0x74639c, 0x7463a4
DEV_COLOR, DEV_SENT, DEV_SEND = 0x72f3f4, 0x72f400, 0x72f3f0
COUNT, VCOUNT, ORDER, POLYS, POOL = 0x803550, 0x803554, 0x803558, 0x80cd9c, 0x82449c
PRIM, SRC_VERTS = 0x72f418, 0x72f41c
THIS, VTABLE = 0x809370, 0x809374
STATE = [PRIM, 0x809388, 0x80938c, 0x809390, 0x809394, 0x809398, 0x8093a8]  # = DeviceState
ZERR, D3DERR = 0x49bfd0, 0x580550
TOL = 1e-6
VISIBLE = 0.003921569


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


def fbits(x):
    return struct.unpack('<i', struct.pack('<f', x))[0]


def floats(*xs):
    return struct.pack(f'<{len(xs)}f', *xs)


def close(a, b):
    return (math.isnan(a) and math.isnan(b)) or abs(a - b) <= TOL


# ---------------------------------------------------------------- fog

def fog_blob(near, far, high, low, on=1, linear=1, color=(0.5, 0.5, 0.5), density=0.0, inv=None):
    """The 0x38 bytes at 0x74a22c as the setters leave them."""
    inv_r = inv[0] if inv else (1 / f32(far - near) if far != near else 0.0)
    inv_a = inv[1] if inv else (1 / f32(high - low) if high != low else 0.0)
    return struct.pack('<ii3f9f', on, linear, *color, near, f32(near * near), far, f32(far * far),
                       inv_r, high, low, inv_a, density)


def classify(fog, m, v):
    """Which branch of 0x555a70 a vertex takes (for coverage only)."""
    near_sq, far_sq, high, low = (struct.unpack_from('<f', fog, o)[0] for o in (0x18, 0x20, 0x28, 0x2c))
    d2 = f32(f32(v[0] * v[0]) + f32(v[2] * v[2]))
    y = m[7] * v[2] + v[1] * m[4] + v[0] * m[1] + m[10]
    if not d2 > near_sq:
        return 'near'
    if y >= high:
        return 'above'
    if not d2 >= far_sq:
        return 'range-band' if y > low else 'range-low'
    return 'far-low' if not y > low else 'far-band'


class FogEmu:
    def __init__(self):
        cpu = self.cpu = Original()
        self.centre = cpu.alloc(16)
        self.result = cpu.alloc(16)
        self.verts = cpu.alloc(12 * 1216)
        call = 0x555640
        stub = cpu.code_next
        body = bytes.fromhex('ff742404') + b'\xe8' + struct.pack('<i', call - (stub + 9))
        body += b'\xd9\x1d' + struct.pack('<I', self.result) + b'\xc3'
        self.sphere_stub = cpu.code(body)
        self.sends = 0

        def send(c):
            self.sends += 1
        send_addr = cpu.code(b'\xc3')
        cpu.stub_function(send_addr, send)
        self.send_addr = send_addr

    def load(self, fog, matrix):
        self.cpu.write(FOG, fog)
        self.cpu.write(MATRIX, floats(*matrix))

    def sphere(self, fog, matrix, centre, radius):
        self.load(fog, matrix)
        self.cpu.write(self.centre, floats(*centre))
        self.cpu.call(self.sphere_stub, ecx=self.centre, stack_args=(fbits(radius) & 0xffffffff,))
        return struct.unpack('<f', self.cpu.read(self.result, 4))[0]

    def vertices(self, fog, matrix, verts, factor, device):
        cpu = self.cpu
        self.load(fog, matrix)
        cpu.write(self.verts, floats(*[c for v in verts for c in v]))
        cpu.u32(VERTS_PTR, self.verts)
        cpu.write(FACTOR, floats(*factor))
        cpu.write(DEV_COLOR, floats(*device))
        cpu.u32(DEV_SEND, self.send_addr)
        self.sends = 0
        cpu.call(0x555a70, ecx=len(verts))
        eax = cpu.uc.reg_read(UC_X86_REG_EAX)
        out = list(struct.unpack(f'<{len(verts)}f', cpu.read(FACTOR, 4 * len(verts)))) if verts else []
        dev = list(struct.unpack('<6f', cpu.read(DEV_COLOR, 24)))
        return eax, out, dev, self.sends


def rebuilt_sphere(fog, matrix, centre, radius):
    return probe.probe_fog_sphere(fog, (ctypes.c_float * 12)(*matrix), (ctypes.c_float * 3)(*centre), radius)


def rebuilt_vertices(fog, matrix, verts, factor, device):
    n = len(verts)
    vb = (ctypes.c_float * max(1, 3 * n))(*[c for v in verts for c in v])
    fb = (ctypes.c_float * max(1, n))(*factor)
    db = (ctypes.c_float * 6)(*device)
    sends = ctypes.c_int32()
    r = probe.probe_fog_vertices(fog, (ctypes.c_float * 12)(*matrix), vb, n, fb, db, ctypes.byref(sends))
    return r, list(fb)[:n], list(db), sends.value


SHIPPED = [  # (near, far, high, low): hwrange and altitude of the worlds, lava, defaults, dummy
    (32, 750, 100, 80), (64, 750, 85, 70), (64, 350, -3, -65), (500, 750, 100, 80),
    (500, 700, 300, 200), (0, 0, 0, 0),
]


def random_fog(rng):
    pick = rng.random()
    if pick < 0.4:
        near, far, high, low = rng.choice(SHIPPED)
    else:
        near = rng.uniform(0, 600)
        far = near + rng.choice([rng.uniform(1, 800), rng.uniform(-200, 0)])  # also far < near
        low = rng.uniform(-100, 200)
        high = low + rng.choice([rng.uniform(1, 100), rng.uniform(-50, 0)])  # also an inverted band
    near, far, high, low = map(f32, (near, far, high, low))
    inv = None
    if near == far or high == low:
        inv = (rng.choice([0.0, 0.004, 2.0]), rng.choice([0.0, 0.05]))  # a stale inverse
    return fog_blob(near, far, high, low, on=rng.randint(0, 1), linear=rng.randint(0, 2),
                    color=tuple(rng.random() for _ in range(3)), density=rng.random(), inv=inv), \
        (near, far, high, low)


def random_matrix(rng, band):
    if rng.random() < 0.3:  # level camera: world y = camera y + height
        m = [1, 0, 0, 0, 1, 0, 0, 0, 1, rng.uniform(-500, 500), 0, rng.uniform(-500, 500)]
    else:
        m = [rng.uniform(-1, 1) for _ in range(9)] + [rng.uniform(-500, 500), 0, rng.uniform(-500, 500)]
    m[10] = rng.uniform(band[1] - 40, band[0] + 40)
    return [f32(x) for x in m]


def random_point(rng, near, far, spread=60):
    d = rng.choice([rng.uniform(0, max(near, 1)), rng.uniform(near, far) if far > near else near,
                    rng.uniform(far, far * 1.5 + 10), rng.uniform(0, 2 * max(far, 50))])
    a = rng.uniform(0, 2 * math.pi)
    return [f32(d * math.cos(a)), f32(rng.uniform(-spread, spread)), f32(d * math.sin(a))]


def test_fog(rng):
    emu = FogEmu()
    spheres = vcalls = verts_total = 0
    branches = {}
    sphere_kinds = {'zero': 0, 'one': 0, 'between': 0}
    for case in range(1500):
        fog, (near, far, high, low) = random_fog(rng)
        m = random_matrix(rng, (high, low))
        for _ in range(8):
            c = random_point(rng, near, far)
            r = f32(rng.choice([0.0, rng.uniform(0, 5), rng.uniform(0, 100), rng.uniform(100, 600)]))
            want = emu.sphere(fog, m, c, r)
            got = rebuilt_sphere(fog, m, c, r)
            if not close(want, got):
                raise SystemExit(f'sphere {near, far, high, low} m {m} c {c} r {r}: original {want} rebuilt {got}')
            sphere_kinds['zero' if want == 0 else 'one' if want == 1 else 'between'] += 1
            spheres += 1
        n = rng.choice([0, 1, 2, 3, 10, 50, 200, 1216])
        verts = [random_point(rng, near, far) for _ in range(n)]
        for v in verts:
            k = classify(fog, m, v)
            branches[k] = branches.get(k, 0) + 1
        factor = [rng.uniform(-2, 2) for _ in range(n)]
        dev = [rng.choice([0.0, 127.5, 255.0]) for _ in range(3)]
        dev += rng.choice([dev[:3], [rng.uniform(0, 255) for _ in range(3)]])
        want = emu.vertices(fog, m, verts, factor, dev)
        got = rebuilt_vertices(fog, m, verts, factor, dev)
        ok = want[0] == got[0] and want[2] == got[2] and want[3] == got[3] and \
            all(close(a, b) for a, b in zip(want[1], got[1]))
        if not ok:
            at = next((i for i, (a, b) in enumerate(zip(want[1], got[1])) if not close(a, b)), None)
            raise SystemExit(f'vertices case {case} {near, far, high, low}: eax {want[0]}/{got[0]} '
                             f'device {want[2]}/{got[2]} sends {want[3]}/{got[3]} first factor diff {at}'
                             + (f' {verts[at]} {want[1][at]} vs {got[1][at]}' if at is not None else ''))
        vcalls += 1
        verts_total += n
    # Exact boundaries on a level camera at height 0: d2 == near^2, y == high, y == low,
    # d2 == far^2, and NaN.
    for near, far, high, low in SHIPPED[:5]:
        fog = fog_blob(*map(f32, (near, far, high, low)))
        m = [1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0]
        pts = []
        for x in (near, far, (near + far) / 2, far * 2):
            for y in (high, low, (high + low) / 2, low - 10, high + 1):
                pts.append([f32(x), f32(y), 0.0])
        pts.append([float('nan'), 0.0, 0.0])
        pts.append([1.0, float('nan'), far])
        want = emu.vertices(fog, m, pts, [0.5] * len(pts), [0.0] * 6)
        got = rebuilt_vertices(fog, m, pts, [0.5] * len(pts), [0.0] * 6)
        if want[0] != got[0] or not all(close(a, b) for a, b in zip(want[1], got[1])):
            raise SystemExit(f'boundaries {near, far, high, low}: {want} vs {got}')
        for p in pts:
            for r in (0.0, 1.0, float(far - near)):
                a, b = emu.sphere(fog, m, p, r), rebuilt_sphere(fog, m, p, r)
                if not close(a, b):
                    raise SystemExit(f'sphere boundary {near, far, high, low} {p} {r}: {a} vs {b}')
                spheres += 1
        vcalls += 1
    # Heights that land on alt_high/alt_low only after rounding to float (the x87
    # compares the unrounded sum), and faint fog just past near (0 < fog <= 1/255).
    edges = 0
    for _ in range(300):
        near, far, high, low = map(f32, rng.choice(SHIPPED[:5]))
        fog = fog_blob(near, far, high, low)
        ty = f32(rng.uniform(0.01, 3))
        m = [1, 0, 0, 0, 1, 0, 0, 0, 1, 0, ty, 0]
        pts = []
        for edge in (high, low):
            base = f32(edge - ty)
            for k in range(-3, 4):
                y = struct.unpack('<f', struct.pack('<i', fbits(base) + k))[0]
                pts.append([f32(rng.uniform(near, far)), y, f32(rng.uniform(-5, 5))])
        faint = [[f32(near * (1 + rng.uniform(0, 0.01))), f32(low - ty - 5), 0.0]]
        for vs in (pts, faint):
            want = emu.vertices(fog, m, vs, [0.5] * len(vs), [0.0] * 6)
            got = rebuilt_vertices(fog, m, vs, [0.5] * len(vs), [0.0] * 6)
            if want[0] != got[0] or want[3] != got[3] or not all(close(a, b) for a, b in zip(want[1], got[1])):
                raise SystemExit(f'edge heights {near, far, high, low} ty {ty}: {want[:2]} vs {got[:2]}')
            edges += 1
        for p in pts:
            r = f32(rng.choice([0.0, 0.5, 2.0]))
            p2 = [p[0], f32(p[1] + r), p[2]]
            a, b = emu.sphere(fog, m, p2, r), rebuilt_sphere(fog, m, p2, r)
            if not close(a, b):
                raise SystemExit(f'sphere edge {near, far, high, low} {p2} {r}: {a} vs {b}')
            spheres += 1
    vcalls += edges
    missing = {'near', 'above', 'range-band', 'range-low', 'far-low', 'far-band'} - set(branches)
    if missing or min(sphere_kinds.values()) == 0:
        raise SystemExit(f'fog branches not reached: {missing} {sphere_kinds}')
    return f'{spheres} spheres ({sphere_kinds}), {vcalls} vertex calls ({verts_total} vertices, {branches})'


# ---------------------------------------------------------------- sort

class SortEmu:
    def __init__(self):
        self.cpu = Original()

    def sort(self, keys, order):
        cpu, n = self.cpu, len(keys)
        cpu.u32(COUNT, n)
        cpu.write(POLYS, b''.join(struct.pack('<4I', 0, 0, 0, POOL + 32 * i) for i in range(n)))
        pool = bytearray(32 * n)
        for i, k in enumerate(keys):
            struct.pack_into('<i', pool, 32 * i + 8, k)
        cpu.write(POOL, pool)
        cpu.write(ORDER, struct.pack(f'<{n}i', *order))
        cpu.call(0x57dec0)
        return list(struct.unpack(f'<{n}i', cpu.read(ORDER, 4 * n)))


def rebuilt_sort(keys, order):
    n = len(keys)
    o = (ctypes.c_int32 * max(1, n))(*order)
    probe.probe_sort((ctypes.c_int32 * max(1, n))(*keys), n, o)
    return list(o)[:n]


def key_arrays(rng):
    sizes = [0, 1, 2, 3, 4, 8, 9, 10, 11, 16, 17, 31, 64, 100, 257, 1000, 3000, 6000]
    for n in sizes:
        rnd = [fbits(rng.random()) for _ in range(n)]
        yield n, 'random', rnd
        yield n, 'sorted', sorted(rnd)
        yield n, 'reversed', sorted(rnd, reverse=True)
        yield n, 'few values', [fbits(rng.choice([0.25, 0.5, 0.75])) for _ in range(n)]
        yield n, 'all equal', [fbits(0.5)] * n
        yield n, 'negative floats', [fbits(rng.uniform(-1, 1)) for _ in range(n)]
        yield n, 'organ pipe', [fbits(min(i, n - i) / max(n, 1)) for i in range(n)]
        yield n, 'int extremes', [rng.choice([-2**31, 2**31 - 1, 0, -1, 1]) for _ in range(n)]
    for _ in range(300):
        n = rng.randint(0, 600)
        yield n, 'random small', [fbits(rng.choice([rng.random(), 0.5])) for _ in range(n)]


def test_sort(rng):
    emu = SortEmu()
    runs = 0
    for n, shape, keys in key_arrays(rng):
        orders = [list(range(n - 1, -1, -1))]  # what the flush hands it
        perm = list(range(n))
        rng.shuffle(perm)
        orders.append(perm)
        for order in orders:
            want = emu.sort(keys, order)
            got = rebuilt_sort(keys, order)
            if want != got:
                at = next(i for i in range(n) if want[i] != got[i])
                raise SystemExit(f'sort {shape} n={n}: first difference at {at}: {want[at]} vs {got[at]}')
            if [keys[i] for i in want] != sorted(keys):
                raise SystemExit(f'sort {shape} n={n}: the original does not sort?')
            runs += 1
    return f'{runs} sorts identical (sizes 0..6000)'


# ---------------------------------------------------------------- queue

class QueueEmu:
    def __init__(self):
        cpu = self.cpu = Original()
        self.errors = []
        cpu.stub_function(ZERR, lambda c: self.errors.append((c.arg(0), c.arg(2), c.arg(4))))
        self.point = cpu.alloc(16)

    def add(self, kind, count):
        """kind 'point' runs 0x57dba0, 'fan' 0x57bf80 with `count` vertices; alpha 0x80."""
        cpu = self.cpu
        self.errors = []
        if kind == 'point':
            cpu.write(self.point, floats(1, 2, 3))
            cpu.call(0x57dba0, ecx=self.point, edx=0x1234, stack_args=(0x80,))
        else:
            cpu.call(0x57bf80, ecx=0x1234, edx=0x80, stack_args=(count, 0, 0))
        return self.errors

    def state(self):
        n, v = struct.unpack('<II', self.cpu.read(COUNT, 8))
        return n, v


def test_queue(rng):
    emu = QueueEmu()
    cpu = emu.cpu
    runs = 0
    for start_n, start_v in ((0, 0), (5990, 0), (0, 23990), (5999, 23999), (6000, 0), (100, 24000)):
        cpu.write(COUNT, struct.pack('<II', start_n, start_v))
        probe.probe_queue_reset(start_n, start_v)
        for _ in range(40):
            kind = rng.choice(['point', 'fan'])
            count = 1 if kind == 'point' else rng.randint(1, 12)
            prim = 1 if kind == 'point' else rng.choice([5, 6])
            cpu.u32(PRIM, prim)
            n0, v0 = emu.state()
            errors = emu.add(kind, count)
            line = ctypes.c_int32()
            off = probe.probe_queue(prim, count, 0, ctypes.byref(line))
            n1, v1 = emu.state()
            if errors:
                want = (-1, errors[0][1])
            else:
                rec = struct.unpack('<4I', cpu.read(POLYS + 16 * n0, 16))
                if rec[:3] != (prim, count, 0) or n1 != n0 + 1 or v1 != v0 + count:
                    raise SystemExit(f'queue {kind}: record {rec} state {n0, v0} -> {n1, v1}')
                want = ((rec[3] - POOL) // 32, 0)
            if (off, line.value) != want:
                raise SystemExit(f'queue {kind} x{count} at {n0, v0}: original {want} rebuilt {off, line.value}')
            runs += 1
    return f'{runs} queue reservations identical (with both overflow reports)'


# ---------------------------------------------------------------- flush

class FlushEmu:
    def __init__(self):
        cpu = self.cpu = Original()
        self.log = []
        self.draw_result = 0
        vt = cpu.alloc(0x100)
        cpu.u32(THIS, 0xdeadbeef)
        cpu.u32(VTABLE, vt)

        def method(off, fn, pop):
            addr = cpu.code(b'\xc3')
            cpu.u32(vt + off, addr)
            cpu.stub_function(addr, fn, pop)
        method(0x58, lambda c: self.log.append((1, c.arg(1), c.arg(2))), 12)
        method(0xa0, lambda c: self.log.append((2, c.arg(1), c.arg(2), c.arg(3))), 16)
        method(0x98, lambda c: self.log.append((3, c.arg(1), c.arg(2))), 12)

        def draw(c):
            self.log.append((4, c.arg(1), c.arg(2), (c.arg(3) - POOL) // 32, c.arg(4), c.arg(5)))
            return self.draw_result
        method(0x70, draw, 24)
        cpu.stub_function(D3DERR, lambda c: self.log.append((5, c.ecx, c.arg(0))), 4)
        self.textures = cpu.alloc(16 * 0x18)

    def flush(self, polys, textures, pool, state, draw_result):
        cpu = self.cpu
        self.log = []
        self.draw_result = draw_result
        for i, (d3d, typ, au, av) in enumerate(textures):
            cpu.write(self.textures + 0x18 * i, struct.pack('<6I', 0, 0, d3d, typ, au, av))
        cpu.write(POOL, pool)
        cpu.write(POLYS, b''.join(struct.pack('<4I', p, c, self.textures + 0x18 * t if t else 0, POOL + 32 * o)
                                  for p, c, t, o in polys))
        cpu.write(COUNT, struct.pack('<II', len(polys), len(pool) // 32))
        for addr, v in zip(STATE, state):
            cpu.u32(addr, v)
        cpu.call(0x57e110)
        st = [struct.unpack('<i', cpu.read(a, 4))[0] for a in STATE]
        order = list(struct.unpack(f'<{len(polys)}i', cpu.read(ORDER, 4 * len(polys))))
        return [tuple(e) + (0,) * (6 - len(e)) for e in self.log], st, order + list(struct.unpack('<II', cpu.read(COUNT, 8)))


def rebuilt_flush(polys, textures, pool, state, draw_result):
    n = len(polys)
    pb = (ctypes.c_int32 * max(1, 4 * n))(*[x for p in polys for x in p])
    tb = (ctypes.c_int32 * 64)(*[x for t in textures for x in t])
    sb = (ctypes.c_int32 * 7)(*state)
    cap = 8 * n + 16
    lb = (ctypes.c_int32 * (6 * cap))()
    ob = (ctypes.c_int32 * (n + 2))()
    k = probe.probe_flush(pb, n, tb, pool, len(pool) // 32, sb, draw_result, lb, cap, ob)
    return [tuple(lb[6 * i:6 * i + 6]) for i in range(min(k, cap))], list(sb), list(ob)


def test_flush(rng):
    emu = FlushEmu()
    runs = draws = 0
    for case in range(400):
        textures = [(0, 0, 0, 0)] + [(0x1000 + i, rng.choice([1, 4, 5, 2]), rng.choice([1, 3]), rng.choice([1, 3]))
                                     for i in range(1, 16)]
        n = rng.choice([0, 1, 2, 5, 20, 100, rng.randint(1, 600)])
        polys, verts = [], []
        for _ in range(n):
            c = rng.randint(1, 6)
            tex = rng.choice([0, 0] + list(range(1, rng.choice([2, 4, 16]))))
            polys.append((rng.choice([1, 5, 6]), c, tex, len(verts)))
            sz = rng.choice([rng.random(), 0.5, 0.25])
            for _ in range(c):
                color = rng.choice([0xff000000, 0xffffffff, 0x80ffffff, 0xff000001, rng.getrandbits(32)])
                verts.append((rng.uniform(0, 640), rng.uniform(0, 480), sz, rng.random(), color,
                              rng.getrandbits(32), rng.random(), rng.random()))
        pool = b''.join(struct.pack('<4f2I2f', *v) for v in verts)
        state = [rng.choice([5, 6]), rng.randint(0, 1), rng.choice([1, 2]), rng.choice([1, 2, 4, 5]),
                 rng.choice([1, 3]), rng.choice([1, 3]), rng.randint(0, 1)]
        draw_result = rng.choice([0, 0, 0, -2005532292])
        want = emu.flush(polys, textures, pool, state, draw_result)
        got = rebuilt_flush(polys, textures, pool, state, draw_result)
        want_log = [tuple(x - 2**32 if x >= 2**31 else x for x in e) for e in want[0]]
        got_log = got[0]
        if want_log != got_log or want[1] != got[1] or want[2] != got[2]:
            at = next((i for i, (a, b) in enumerate(zip(want_log, got_log)) if a != b), None)
            raise SystemExit(f'flush case {case} n={n}: log {len(want_log)}/{len(got_log)} first diff {at}: '
                             f'{want_log[at] if at is not None else ""} vs {got_log[at] if at is not None else ""}; '
                             f'state {want[1]} vs {got[1]}; order same {want[2] == got[2]}')
        runs += 1
        draws += n
    return f'{runs} flushes ({draws} polygons) with identical device calls, order and state'


def main():
    rng = random.Random(3)
    print(test_fog(rng))
    print(test_sort(rng))
    print(test_queue(rng))
    print(test_flush(rng))
    print('ok: fog sphere and vertices, transparent sort, queue and flush identical to the original')


if __name__ == '__main__':
    main()
