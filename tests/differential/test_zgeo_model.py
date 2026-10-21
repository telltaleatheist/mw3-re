"""zGeometry's model side against the original x86 (docs/craters.md, section
5; craters part 2): the colour cache 0x50da90 / 0x50dad0 / 0x50dbc0, the
plane turns 0x50dc90 / 0x50dc70, the boxes 0x50dcb0 / 0x50e3d0, the coloured
materials 0x50dd20 / 0x50de00, the gradient 0x50e020 and the UVs, colours
and normals carried to new points 0x50df30 / 0x50e100 / 0x50e240, a
polygon's points 0x50f0a0, the clip polygon 0x50e450 / 0x50e4c0 / 0x50e520
with its point and edge searches and inserts 0x50e550 / 0x50e690 / 0x50e5a0,
the polygon adds 0x50dea0 / 0x50f4e0 / 0x50f640, the clip set 0x50e910 /
0x50e930 and its commit 0x50e850.

    cmake --build build && .venv/bin/python tests/differential/test_zgeo_model.py

Made-up inputs: random points, triangles (with ones flat in x and z),
polygons with and without UVs, normals and a material, clip outlines (round
and axis-aligned) with points on their vertices, on their edges, near the
ends and off them, commits over cells and nodes. The x87 at 24 bits, as in
play. zModel's add-a-polygon 0x53c6c0, the material table's add 0x536ad0,
rand, the node calls 0x4eb420 / 0x4eb370, the model release 0x539150 and
Weiler's free 0x50fab0 are stubbed on both sides; 0x536a70, 0x53cc40,
0x515130, 0x4020d0 and _ftol run. Compared: everything the stubs are called
with, in order, the results, and what the functions leave in memory.

BREAK=<n> breaks the port's copy of the inputs in a known way (see main);
each must fail.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import FTOL, IAT_FTOL, ROOT, Original  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
P = ctypes.c_void_p
for name, args in (('colors', [P, ctypes.c_int, P, ctypes.c_int]),
                   ('geom', [ctypes.c_int, P, ctypes.c_int, P, ctypes.c_int]),
                   ('poly', [ctypes.c_int, P, ctypes.c_int, P, P, ctypes.c_uint32, ctypes.c_int32, P, P, P, P,
                             ctypes.c_int32, ctypes.c_uint32, P, P, P, ctypes.c_int]),
                   ('add', [P, ctypes.c_int, ctypes.c_int32, P, P, P, P, P, ctypes.c_int]),
                   ('clip', [P, ctypes.c_int, P, ctypes.c_int, P, ctypes.c_int]),
                   ('commit', [ctypes.c_int, P, P, P, P, P, ctypes.c_int]),
                   ('materials', [P, P, P, ctypes.c_int])):
    getattr(probe, 'probe_zgeo_' + name).argtypes = args

IAT_MALLOC, IAT_FREE, IAT_REALLOC, IAT_MEMMOVE, IAT_RAND = 0x5bb664, 0x5bb65c, 0x5bb610, 0x5bb584, 0x5bb668
ZERR, MODEL_ADD, MAT_ADD, MAT_INIT = 0x49bfd0, 0x53c6c0, 0x536ad0, 0x536a70
NODE_MODEL, NODE_SET, MODEL_RELEASE, WEILER_FREE = 0x4eb420, 0x4eb370, 0x539150, 0x50fab0
COLORS, COLORS_END = 0x60cb38, 0x60cb50 + 63 * 0x24
MATS, MADE, MODELS, NODES, CELLS = 0x6e000000, 0x6e100000, 0x6e200000, 0x6e300000, 0x6e400000
CAP = 1 << 20


def fb(v):
    return struct.unpack('<I', struct.pack('<f', v))[0]


def farr(vals):
    return struct.pack(f'<{len(vals)}f', *vals)


class Emulated:
    def __init__(self):
        cpu = self.cpu = Original()
        cpu.import_code(IAT_FTOL, FTOL)
        cpu.fpu_control(0x007f)
        for base in (MATS, MADE, MODELS, NODES, CELLS):
            cpu.uc.mem_map(base, 0x10000)
        self.sizes = {}
        cpu.stub_import(IAT_MALLOC, lambda c: self.alloc(c.arg(0)))
        cpu.stub_import(IAT_REALLOC, self.realloc)
        cpu.stub_import(IAT_FREE, lambda c: 0)
        cpu.stub_import(IAT_MEMMOVE, self.memmove)
        cpu.stub_import(IAT_RAND, lambda c: self.rands.pop(0))
        cpu.stub_function(ZERR, self.zerr, 0)
        cpu.stub_function(MODEL_ADD, self.model_add, 0x2c)
        cpu.stub_function(MAT_ADD, self.mat_add, 0)
        cpu.stub_function(NODE_MODEL, self.node_model, 0)
        cpu.stub_function(NODE_SET, lambda c: self.log(0x64, self.node_id(c.ecx), self.model_id(c.edx)), 0)
        cpu.stub_function(MODEL_RELEASE, lambda c: self.log(0x65, self.model_id(c.ecx)), 0)
        cpu.stub_function(WEILER_FREE, lambda c: self.log(0x66, c.ecx), 0)
        # 0x536a70 keeps flag bits 3..7 of the uninitialised stack material:
        # zero it first, as the port's value-initialised one.
        cpu.uc.hook_add(UC_HOOK_CODE, lambda uc, a, s, u: cpu.write(cpu.ecx, bytes(0x28)), begin=MAT_INIT,
                        end=MAT_INIT)
        self.mark = cpu.heap_next
        self.rands = []
        self.current = {}

    # --- plumbing --------------------------------------------------------
    def begin(self, rands=()):
        self.cpu.heap_next = self.mark
        self.sizes.clear()
        self.out = bytearray()
        self.rands = list(rands)
        self.made = 0

    def alloc(self, n):
        p = self.cpu.alloc(max(n, 1))
        self.cpu.write(p, bytes(max(n, 1)))
        self.sizes[p] = n
        return p

    def put(self, data):
        p = self.alloc(len(data))
        self.cpu.write(p, data)
        return p

    def realloc(self, c):
        old, n = c.arg(0), c.arg(1)
        p = self.alloc(n)
        if old:
            self.cpu.write(p, self.cpu.read(old, min(n, self.sizes.get(old, 0))))
        return p

    def memmove(self, c):
        dst, src, n = c.arg(0), c.arg(1), c.arg(2)
        self.cpu.write(dst, self.cpu.read(src, n))
        return dst

    def call(self, addr, ecx=0, edx=0, stack=()):
        self.cpu.call(addr, ecx, edx, stack)
        return self.cpu.uc.reg_read(UC_X86_REG_EAX)

    def u32(self, a):
        return struct.unpack('<I', self.cpu.read(a, 4))[0]

    def log(self, *v):
        for x in v:
            self.out += struct.pack('<I', x & 0xffffffff)
        return 0

    def floats(self, p, n):
        if not p:
            return self.log(0)
        self.log(1)
        self.out += self.cpu.read(p, 4 * n)
        return 0

    def cstr(self, p):
        b = self.cpu.read(p, 256)
        return b[:b.index(b'\0')]

    # --- ids -------------------------------------------------------------
    @staticmethod
    def mat_id(p):
        if not p:
            return 0
        if MATS <= p < MATS + 0x10000:
            return 0x100 + (p - MATS) // 0x40
        return 0x400 + (p - MADE) // 0x40

    @staticmethod
    def model_id(p):
        return 0x200 + (p - MODELS) // 0x60 if p else 0

    @staticmethod
    def node_id(p):
        return 0x300 + (p - NODES) // 0xd4 if p else 0

    # --- stubs -----------------------------------------------------------
    def zerr(self, c):
        msg = self.cstr(c.arg(3)) % c.arg(4)
        self.log(0x67, c.arg(0), c.arg(2), len(msg))
        self.out += msg
        return 0

    def mat_add(self, c):
        m = c.read(c.ecx, 0x24)
        alpha, flags, rgb = m[0], m[1], struct.unpack_from('<H', m, 2)[0]
        self.log(0x61, alpha, flags, rgb)
        self.out += m[4:16]
        self.log(struct.unpack_from('<I', m, 0x20)[0])
        p = MADE + 0x40 * (self.made % 64)
        self.made += 1
        return p

    def model_add(self, c):
        n = c.edx
        self.log(0x62, self.model_id(c.ecx), n)
        self.floats(c.arg(0), 3 * n), self.floats(c.arg(1), 3 * n), self.floats(c.arg(2), 2 * n)
        self.log(c.arg(3), c.arg(4), c.arg(5), self.mat_id(c.arg(6)), c.arg(7), c.arg(8), self.u32(c.arg(9)))
        self.floats(c.arg(10), 3 * n)
        return n + 7

    def node_model(self, c):
        self.log(0x63, self.node_id(c.ecx))
        c.u32(c.edx, self.current.get(c.ecx, 0))
        return 0

    # --- the runs ----------------------------------------------------------
    def dump_colors(self):
        count = self.u32(0x60cb3c)
        self.log(count, self.u32(0x60cb38))
        self.floats(0x60cb40, 3)
        for i in range(count):
            e = 0x60cb50 + 0x24 * i
            self.floats(e, 3), self.floats(e + 0x18, 3)

    def colors(self, ops):
        self.begin()
        self.cpu.write(COLORS, bytes(COLORS_END - COLORS))
        for op, x, y, z, a, b, cc in ops:
            pos = self.put(farr([x, y, z]))
            if op == 0:
                self.call(0x50da90, stack=(fb(x), fb(y), fb(z), fb(a)))
            elif op == 1:
                self.call(0x50dad0, ecx=pos, stack=(self.put(farr([a, b, cc])),))
            else:
                out = self.put(farr([-1, -1, -1]))
                self.log(self.call(0x50dbc0, ecx=pos, stack=(out,)))
                self.floats(out, 3)
        self.dump_colors()
        return bytes(self.out)

    def geom(self, op, vals, n):
        self.begin()
        p = self.put(farr(vals))
        if op in (0, 1):
            self.call(0x50dc90 if op == 0 else 0x50dc70, ecx=n, edx=p)
            self.floats(p, 3 * n)
        elif op == 2:
            self.log(self.call(0x50dcb0, ecx=p, edx=p + 16))
        elif op == 3:
            box = self.alloc(16)
            self.call(0x50e3d0, ecx=box, edx=p, stack=(n,))
            self.floats(box, 4)
        else:
            out = self.alloc(8)
            self.call(0x50e020, ecx=p, edx=p + 12, stack=(p + 24, fb(vals[9]), fb(vals[10]), fb(vals[11]), out))
            self.floats(out, 2)
        return bytes(self.out)

    def model(self, case):
        verts, normals = self.put(farr(case['verts'])), self.put(farr(case['normals']))
        m = MODELS
        self.cpu.write(m, bytes(0x60))
        self.cpu.write(m + 0x38, struct.pack('<II', verts, normals))
        n = case['flags'] & 0xff
        poly = self.alloc(0x24)
        uvs = self.put(farr(case['uvs'])) if case['uvs'] is not None else 0
        mat = MATS + 0x40 * (case['material'] - 0x100) if case['material'] else 0
        self.cpu.write(poly, struct.pack('<IiIIIIIII', case['flags'], case['priority'],
                                         self.put(struct.pack(f'<{n}I', *case['vidx'])),
                                         self.put(struct.pack(f'<{n}I', *case['nidx'])), uvs,
                                         self.put(farr(case['colors'])), 0, mat, case['zone']))
        return m, poly

    def poly(self, op, case, points, rands=(), cache=(0, 0, 0, 0)):
        self.begin(rands)
        m, poly = self.model(case)
        pts = self.put(farr(points)) if points else self.alloc(12)
        n = len(points) // 3
        if op in (0, 1, 2):
            r = self.call((0x50df30, 0x50e100, 0x50e240)[op], ecx=n, edx=pts, stack=(m, poly))
            self.floats(r, (2, 3, 3)[op] * n)
        elif op == 3:
            r = self.call(0x50f0a0, ecx=m, edx=poly, stack=(0,))
            self.floats(r, 3 * (case['flags'] & 0xff))
        else:
            self.cpu.write(COLORS, bytes(COLORS_END - COLORS))
            self.call(0x50da90, stack=tuple(fb(v) for v in cache))
            if op == 4:
                ret = self.call(0x50f4e0, ecx=MODELS + 0x60, edx=n, stack=(pts, m, poly))
            else:
                ret = self.call(0x50f640, ecx=MODELS + 0x60, edx=m, stack=(poly,))
            self.log(ret)
            self.dump_colors()
        return bytes(self.out)

    def add(self, points, material, uvs, normals, colors, rands):
        self.begin(rands)
        n = len(points) // 3
        args = [self.put(farr(points)) if points else self.alloc(12),
                MATS + 0x40 * (material - 0x100) if material else 0]
        args += [self.put(farr(v)) if v is not None else 0 for v in (uvs, normals, colors)]
        self.log(self.call(0x50dea0, ecx=MODELS + 0x60, edx=n, stack=tuple(args)))
        return bytes(self.out)

    def clip(self, points, ins):
        self.begin()
        n, m = len(points) // 3, len(ins) // 3
        pts, ip = self.put(farr(points)), self.put(farr(ins) or bytes(12))
        c = self.call(0x50e450, ecx=n, edx=pts)
        self.cpu.write(c, struct.pack('<I', 0x5150))
        self.floats(c + 0xc, 4), self.floats(self.u32(c + 4), 3 * n)
        for i in range(m):
            self.log(self.call(0x50e550, ecx=c, edx=ip + 12 * i))
            self.log(self.call(0x50e690, ecx=c, edx=ip + 12 * i))
            self.log(self.call(0x50e5a0, ecx=c, edx=1, stack=(ip + 12 * i,)))
        count, buf = self.put(struct.pack('<i', -1)), self.put(struct.pack('<I', self.put(farr([7, 7, 7]))))
        self.log(self.call(0x50e4c0, ecx=c, edx=count, stack=(buf,)))
        k = self.u32(count)
        self.log(k), self.floats(self.u32(buf), 3 * k)
        self.call(0x50e520, ecx=c)
        c = self.call(0x50e450, ecx=n, edx=pts)
        self.log(self.call(0x50e5a0, ecx=c, edx=m, stack=(ip,)))
        k = self.u32(c + 8)
        self.log(k), self.floats(self.u32(c + 4), 3 * k)
        self.call(0x50e520, ecx=c)
        return bytes(self.out)

    def commit(self, counts, pairs, current, craters):
        self.begin()
        self.current = {NODES + 0xd4 * i: (MODELS + 0x60 * v if v >= 0 else 0) for i, v in enumerate(current)}
        s = self.call(0x50e910)
        cells = self.alloc(0xc * len(counts))
        k = 0
        for i, cnt in enumerate(counts):
            self.cpu.write(CELLS + 0x48 * i + 0x39, bytes([craters[i]]))
            pr = self.alloc(8 * cnt)
            for j in range(cnt):
                node, model = pairs[2 * (k + j)], pairs[2 * (k + j) + 1]
                self.cpu.write(pr + 8 * j, struct.pack('<II', NODES + 0xd4 * node,
                                                       MODELS + 0x60 * model if model >= 0 else 0))
            k += cnt
            self.cpu.write(cells + 0xc * i, struct.pack('<III', cnt, pr, CELLS + 0x48 * i))
        self.cpu.write(s + 8, struct.pack('<II', len(counts), cells))
        self.log(self.call(0x50e850, ecx=s))
        for i in range(len(counts)):
            self.log(self.cpu.read(CELLS + 0x48 * i + 0x39, 1)[0], self.u32(cells + 0xc * i))
        self.call(0x50e930, ecx=s)
        return bytes(self.out)

    def materials(self, rgb, rands):
        self.begin(rands)
        self.log(self.mat_id(self.call(0x50dd20)))
        self.log(self.mat_id(self.call(0x50de00, stack=tuple(fb(v) for v in rgb))))
        self.log(self.mat_id(self.u32(0x60d450)), self.mat_id(self.u32(0x60d454)))
        return bytes(self.out)


# --- the port ----------------------------------------------------------------
BUF = ctypes.create_string_buffer(CAP)


def fptr(vals):
    if vals is None:
        return None
    return ctypes.cast(ctypes.create_string_buffer(farr(vals) or bytes(12), max(4 * len(vals), 12)), P)


def iptr(vals, fmt='i'):
    return ctypes.cast(ctypes.create_string_buffer(struct.pack(f'<{len(vals)}{fmt}', *vals) or bytes(4),
                                                   max(4 * len(vals), 4)), P)


def got(n):
    assert n <= CAP
    return BUF.raw[:n]


def port_poly(op, case, points, rands=(), cache=(0, 0, 0, 0)):
    return got(probe.probe_zgeo_poly(op, fptr(points), len(points) // 3, fptr(case['verts']), fptr(case['normals']),
                                     case['flags'], case['priority'], iptr(case['vidx'], 'I'),
                                     iptr(case['nidx'], 'I'), fptr(case['uvs']), fptr(case['colors']),
                                     case['material'], case['zone'], fptr(list(cache)), iptr(list(rands) + [0]),
                                     BUF, CAP))


# --- inputs ------------------------------------------------------------------
def rnd_case(rng, n=None, flat=False):
    n = n or rng.randint(3, 8)
    nv = n + rng.randint(0, 4)
    verts = []
    for i in range(nv):
        x, z = rng.uniform(-200, 200), rng.uniform(-200, 200)
        if flat and i < 3:
            x, z = (10.0, 10.0) if i != 1 else (10.0 + rng.choice((0, 5)), 10.0 + rng.choice((0, 5)))
            if i == 2:
                x, z = 2 * verts[3] - 10.0, 2 * verts[5] - 10.0
        verts += [x, rng.uniform(-20, 40), z]
    normals = []
    for _ in range(nv):
        normals += [rng.uniform(-1, 1), rng.uniform(0, 1), rng.uniform(-1, 1)]
    flags = n | (rng.random() < 0.5) << 8 | (rng.random() < 0.6) << 9 | rng.choice((0, 0x400, 0x1000))
    return dict(verts=verts, normals=normals, flags=flags, priority=rng.randint(-3, 9),
                vidx=([0, 1, 2] + rng.sample(range(3, nv), n - 3)) if flat else rng.sample(range(nv), n), nidx=rng.sample(range(nv), n),
                uvs=None if rng.random() < 0.25 else [rng.uniform(-4, 4) for _ in range(2 * n)],
                colors=[rng.uniform(0, 255) for _ in range(3 * n)],
                material=rng.choice((0, 0x100, 0x101, 0x105)), zone=rng.getrandbits(32))


def rnd_points(rng, case, k):
    v = case['verts']
    out = []
    for _ in range(k):
        i = rng.choice(case['vidx'])
        out += [v[3 * i] + rng.uniform(-30, 30), rng.uniform(-10, 30), v[3 * i + 2] + rng.uniform(-30, 30)]
    return out


def outline(rng):
    """An outline (world points) and its points in the plane, (x, -z, y)."""
    n = rng.randint(3, 8)
    cx, cz, y = rng.uniform(-100, 100), rng.uniform(-100, 100), rng.uniform(-5, 20)
    if rng.random() < 0.35:
        w, h = rng.choice((2.0, 8.0, 15.5)), rng.choice((3.0, 9.0, 12.25))
        pts = [(cx - w, cz - h), (cx + w, cz - h), (cx + w, cz + h), (cx - w, cz + h)]
    else:
        import math
        r = rng.uniform(3, 40)
        pts = [(cx + r * math.cos(2 * math.pi * i / n), cz + r * math.sin(2 * math.pi * i / n)) for i in range(n)]
    world = []
    for x, z in pts:
        world += [x, y + rng.uniform(-1, 1), z]
    plane = [(world[3 * i], -world[3 * i + 2], world[3 * i + 1]) for i in range(len(pts))]
    return world, plane


def inserts(rng, plane, k):
    out = []
    n = len(plane)
    for _ in range(k):
        i = rng.randrange(n)
        a, b = plane[i], plane[(i + 1) % n]
        kind = rng.random()
        if kind < 0.25:
            p = (a[0] + rng.choice((0, 0.004, -0.009, 0.02)), a[1] + rng.choice((0, 0.006, 0.011)), 3.0)
        elif kind < 0.75:
            t = rng.choice((rng.uniform(0.02, 0.98), 0.001, 0.999, 0.5))
            e = rng.choice((0, 0, 0.003, 0.008, 0.02))
            p = (a[0] + t * (b[0] - a[0]) + e, a[1] + t * (b[1] - a[1]) - e, rng.uniform(-2, 2))
        else:
            p = (rng.uniform(-150, 150), rng.uniform(-150, 150), 0.0)
        out += list(p)
    return out


def main():
    rng = random.Random(int(os.environ.get('SEED', '1')))
    brk = int(os.environ.get('BREAK', '0'))
    emu = Emulated()
    runs = fails = 0
    kinds = {}

    def check(kind, mine, theirs):
        nonlocal runs, fails
        runs += 1
        kinds[kind] = kinds.get(kind, 0) + 1
        if mine != theirs:
            fails += 1
            if fails <= 3:
                i = next((k for k in range(min(len(mine), len(theirs))) if mine[k] != theirs[k]),
                         min(len(mine), len(theirs)))
                print(f'FAIL {kind}: first difference at byte {i} (port {len(mine)}, original {len(theirs)})')
                print('  port    ', mine[max(0, i - 16):i + 32].hex())
                print('  original', theirs[max(0, i - 16):i + 32].hex())

    # The colour cache.
    for _ in range(300):
        ops = []
        cx, cy, cz, r = rng.uniform(-300, 300), rng.uniform(0, 50), rng.uniform(-300, 300), rng.uniform(5, 40)
        if rng.random() < 0.9:
            ops.append((0, cx, cy, cz, r, 0, 0))
        added = []
        for _ in range(rng.randint(0, 90)):
            k = rng.random()
            if k < 0.6:
                if added and rng.random() < 0.2:
                    x, z = rng.choice(added)
                else:
                    x = cx + rng.choice((rng.uniform(-r - 6, r + 6), r + 4, -(r + 4)))
                    z = cz + rng.uniform(-r - 6, r + 6)
                added.append((x, z))
                ops.append((1, x, rng.uniform(0, 30), z, rng.uniform(0, 255), rng.uniform(0, 255),
                            rng.uniform(0, 255)))
            elif k < 0.95:
                x, z = rng.choice(added) if added else (cx, cz)
                ops.append((2, x + rng.uniform(-0.8, 0.8), 0, z + rng.uniform(-0.8, 0.8), 0, 0, 0))
            else:
                cx, cz, r = cx + rng.uniform(-5, 5), cz + rng.uniform(-5, 5), rng.uniform(5, 40)
                ops.append((0, cx, cy, cz, r, 0, 0))
        flat = [v for op in ops for v in op]
        if brk == 1 and ops and ops[0][0] == 0:
            flat[4] += 1
        mine = got(probe.probe_zgeo_colors(fptr(flat), len(ops), BUF, CAP))
        check('colors', mine, emu.colors(ops))

    # The plane, boxes and gradients.
    for _ in range(400):
        op = rng.randrange(5)
        n = rng.randint(0, 8) if op in (0, 1) else rng.randint(1, 8)
        if op == 2:
            b = sorted(rng.uniform(-20, 20) for _ in range(2)), sorted(rng.uniform(-20, 20) for _ in range(2))
            box_b = [b[0][0], b[1][0], b[0][1], b[1][1]]
            edge = rng.choice((0, 1.0, -1.0, 1.0001, 0.9999, rng.uniform(-3, 3)))
            box_a = [box_b[2] + edge, box_b[3] + rng.uniform(-3, 3), box_b[2] + edge + 4, box_b[3] + 6]
            if rng.random() < 0.5:
                box_a = [box_b[0] - 5, box_b[1] - 5 - rng.choice((0, 1.0, 2.0)), box_b[0] - edge,
                         box_b[1] - rng.choice((1.0, 0.5, 1.5))]
            vals = box_a + box_b
        elif op == 4:
            c = rnd_case(rng, 3, flat=rng.random() < 0.3)
            vals = c['verts'][:9] + [rng.uniform(-10, 10) for _ in range(3)]
        else:
            vals = [rng.uniform(-100, 100) for _ in range(3 * n)]
        mine_vals = list(vals)
        if brk == 2 and op == 4:
            mine_vals[9] += 1
        mine = got(probe.probe_zgeo_geom(op, fptr(mine_vals), n, BUF, CAP))
        check(('to_plane', 'from_plane', 'boxes_touch', 'box_of', 'gradient')[op], mine, emu.geom(op, vals, n))

    # A polygon's UVs, colours, normals and points; its children and copies.
    for _ in range(500):
        case = rnd_case(rng, flat=rng.random() < 0.15)
        op = rng.randrange(6)
        if op in (0, 2) and case['uvs'] is None:
            case['uvs'] = [rng.uniform(-4, 4) for _ in range(2 * (case['flags'] & 0xff))]
        if op == 2:
            case['flags'] |= 0x200
        points = rnd_points(rng, case, rng.randint(0, 9) if op == 4 else rng.randint(1, 9))
        rands = [rng.randrange(32768) for _ in range(3)]
        v0 = case['verts'][3 * case['vidx'][0]:3 * case['vidx'][0] + 3]
        cache = (v0[0] + rng.uniform(-10, 10), 0.0, v0[2] + rng.uniform(-10, 10), rng.uniform(5, 40))
        mine_case = dict(case)
        if brk == 3 and op == 0:
            mine_case['uvs'] = case['uvs'][:2] + case['uvs'][4:6] + case['uvs'][2:4] + case['uvs'][6:]
        if brk == 4 and op == 2:
            mine_case['nidx'] = case['nidx'][1:2] + case['nidx'][:1] + case['nidx'][2:]
        if brk == 5 and op == 4:
            mine_case['flags'] ^= 0x100
        check(('uvs_at', 'colors_at', 'normals_at', 'polygon_points', 'add_child', 'copy_polygon')[op],
              port_poly(op, mine_case, points, rands, cache), emu.poly(op, case, points, rands, cache))

    # add_polygon.
    for _ in range(150):
        n = rng.randint(0, 8)
        pts = [rng.uniform(-50, 50) for _ in range(3 * n)]
        material = rng.choice((0, 0, 0x100, 0x103))
        uvs = None if rng.random() < 0.5 else [rng.uniform(-2, 2) for _ in range(2 * n)]
        normals = None if rng.random() < 0.5 else [rng.uniform(-1, 1) for _ in range(3 * n)]
        colors = None if rng.random() < 0.3 else [rng.uniform(0, 255) for _ in range(3 * n)]
        rands = [rng.randrange(32768) for _ in range(3)]
        mine_rands = [(r + 1) % 32768 for r in rands] if brk == 6 else rands
        mine = got(probe.probe_zgeo_add(fptr(pts), n, material, fptr(uvs), fptr(normals), fptr(colors),
                                        iptr(mine_rands), BUF, CAP))
        check('add_polygon', mine, emu.add(pts, material, uvs, normals, colors, rands))

    # The clip polygon: points found, edges found, inserts.
    for _ in range(400):
        world, plane = outline(rng)
        ins = inserts(rng, plane, rng.randint(0, 6))
        mine_ins = list(ins)
        if brk == 7 and ins:
            mine_ins[0] += 0.02
        mine = got(probe.probe_zgeo_clip(fptr(world), len(world) // 3, fptr(mine_ins), len(ins) // 3, BUF, CAP))
        check('clip', mine, emu.clip(world, ins))

    # Commits.
    for _ in range(150):
        counts = [rng.randint(0, 4) for _ in range(rng.randint(0, 4))]
        pairs = []
        for _ in range(sum(counts)):
            pairs += [rng.randrange(16), rng.choice((-1, 0, 1, 2, 3))]
        current = [rng.choice((-1, 0, 1, 2, 3)) for _ in range(16)]
        craters = [rng.choice((0, 1, 7, 255)) for _ in range(len(counts))]
        mine_current = [-1] * 16 if brk == 8 else current
        mine = got(probe.probe_zgeo_commit(len(counts), iptr(counts or [0]), iptr(pairs or [0]), iptr(mine_current),
                                           ctypes.cast(ctypes.create_string_buffer(bytes(craters) or b'\0'), P),
                                           BUF, CAP))
        check('commit', mine, emu.commit(counts, pairs, current, craters))

    # The coloured materials.
    for _ in range(100):
        rgb = [rng.choice((rng.uniform(0, 256), float(rng.randrange(256)))) for _ in range(3)]
        rands = [rng.randrange(32768) for _ in range(3)]
        mine_rgb = [rgb[1], rgb[0], rgb[2]] if brk == 9 else rgb
        mine = got(probe.probe_zgeo_materials(fptr(mine_rgb), iptr(rands), BUF, CAP))
        check('materials', mine, emu.materials(rgb, rands))

    if fails:
        print(f'{fails} of {runs} runs differ')
        sys.stdout.flush(); os._exit(1)
    print(f'ok: {runs} runs identical ({", ".join(f"{k} {v}" for k, v in kinds.items())})')
    sys.stdout.flush(); os._exit(0)


if __name__ == '__main__':
    main()
