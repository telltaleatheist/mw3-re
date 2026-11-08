"""PM's crater cut against its exe: the polygon add 0x55cb70 (colinear points, non-planar fans, pieces past 48 points,
UVs per material, snapped and flattened) and the clip 0x528920 with 0x528f00, 0x528d20, 0x527710. Models are PM's; a
crater dug into the ground on this branch goes through these. The model alloc and release are stubbed in the exe (its
pool needs the game's set-up) and the port's real ones run: the pool's count must come back. At 24 bits the words are
compared exactly, cut and add alike. At 53 the x87's doubles between stores are kept (zgeo_x87.h), floats must agree to
1e-3 (seen: the last bit of a cut point, worst 1.2e-4 relative) and the clip may take another branch on 1 cut in 500 at
most: 1 of the 1,116 cuts here does, a ground piece's two coplanar triangles split the other way (docs/craters.md).

    cmake --build build --target pmprobe && .venv/bin/python tests/differential/test_pm_crater_cut.py [cases]

Compared: results, the outline as the clip left it, every model field. BREAK=1 changes one UV of the port's input.
Strips (0x2000) in half the clip cases' ground, a row each, some with a repeated point: the clip unstrips its source
first (0x5726f0, pm::zmod::model_unstrip), and the source is compared after too (pm-tests item 11).
Untested here: a polygon without UVs (a random material through the material pool); test_pm_mission shows no shipped
ground has one. clip_cell, commit and the crater's own polygons are MW3's logic on these, seen in the mission test.
"""
import ctypes
import math
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import pm_floors  # noqa: E402
import test_pm_zar  # noqa: E402
from original import FTOL, Original, pm_iat  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

I32, F32, U32 = ctypes.c_int32, ctypes.c_float, ctypes.c_uint32
P = ctypes.c_void_p
probe = test_pm_zar.load_pm_probe()
probe.probe_pm_crater.argtypes = [I32, I32, P, P, P, P, P, P, P, I32, P, F32, P, I32]
probe.probe_pm_crater.restype = I32
probe.probe_pm_set_fpu53.argtypes = [I32]
ADD, ALLOC, RELEASE, CLIP, CLIP_NEW, WEILER_NEW, PRECISION = 0x55cb70, 0x558f50, 0x5590f0, 0x528920, 0x527bf0, 0x529280, 0x611c00
MATS = 4
# fld qword [esp+4]; floor by the control word's round-down; ret
FLOOR = bytes.fromhex('dd442404 83ec08 d93c24 0fb70424 25fff30000 0d00040000 6689442402 d96c2402 d9fc d92c24 83c408 c3')


def farr(v):
    return struct.pack(f'<{len(v)}f', *v)


class Exe:
    def __init__(self, cw):
        cpu = self.cpu = Original('pm')
        cpu.fpu_control(cw)
        self.sizes = {}
        self.room = {}
        cpu.stub_import(pm_iat('malloc'), lambda c: self.alloc(c.arg(0)))
        cpu.stub_import(pm_iat('calloc'), lambda c: self.alloc(c.arg(0) * c.arg(1)))
        cpu.stub_import(pm_iat('free'), lambda c: 0)
        cpu.stub_import(pm_iat('realloc'), self.realloc)
        cpu.import_code(pm_iat('_ftol'), FTOL)
        cpu.import_code(pm_iat('floor'), FLOOR)
        cpu.import_code(pm_iat('_CIsqrt'), bytes.fromhex('d9fac3'))
        cpu.stub_import(pm_iat('memmove'), self.memmove)
        cpu.tr = lambda a, size=1: a  # the slots above went through the map; PM's addresses from here
        self.stub(0x596520, self.realloc)
        self.stub(0x406cb0, lambda c: 0)
        self.stub(ALLOC, self.model_alloc)
        self.stub(RELEASE, lambda c: 0)
        self.thunks = {}
        self.mats = cpu.alloc(0x2c * MATS)
        cpu.write(self.mats, bytes(0x2c * MATS))
        for i in range(3):
            cpu.write(self.mats + 0x2c * i + 1, b'\x01')
        self.mark = cpu.heap_next

    def stub(self, addr, fn, pop=0):
        self.cpu.stubs[addr] = (fn, pop)
        self.cpu._uc.hook_add(UC_HOOK_CODE, self.cpu._stub_hit, begin=addr, end=addr)

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
        if old and n <= self.room.get(old, 0):  # the bump heap: growth in steps, or a big model runs it out
            self.sizes[old] = n
            return old
        p = self.alloc(2 * n)
        self.sizes[p], self.room[p] = n, 2 * n
        if old:
            self.cpu.write(p, bytes(self.cpu.read(old, min(n, self.sizes.get(old, 0)))))
        return p

    def memmove(self, c):
        dst, src, n = c.arg(0), c.arg(1), c.arg(2)
        self.cpu.write(dst, bytes(self.cpu.read(src, n)))
        return dst

    def model_alloc(self, c):
        m = self.alloc(0x68)
        self.cpu.write(m + 8, struct.pack('<I', 3))
        return m

    def call(self, addr, args=()):
        if addr not in self.thunks:
            self.thunks[addr] = self.cpu.code(b'\xb8' + struct.pack('<I', addr) + b'\xff\xe0')
        self.cpu.call(self.thunks[addr], stack_args=args, count=2_000_000_000)
        return self.cpu._uc.reg_read(UC_X86_REG_EAX)

    def u32(self, a, n=1):
        return list(struct.unpack(f'<{n}I', bytes(self.cpu.read(a, 4 * n))))

    def dump(self, m, o):
        if not m:
            o.append(0xffffffff)
            return
        polys, verts, norms = self.u32(m + 0x10, 3)
        o += [polys, verts, norms]
        vp, np_ = self.u32(m + 0x38, 2)
        if verts:
            o += self.u32(vp, 3 * verts)
        if norms:
            o += self.u32(np_, 3 * norms)
        base = self.u32(m + 0x34)[0]
        for i in range(polys):
            f, pr, vi, ni, mc, ml, uv, col, _, zone = self.u32(base + 0x28 * i, 10)
            k = f & 0x3ff
            o += [f, pr, zone, mc]
            o += [(a - self.mats) // 0x2c for a in self.u32(ml, mc)]
            o += self.u32(vi, k)
            if f & 0x1000:
                o += self.u32(ni, k)
            o.append(1 if uv else 0)
            if uv:
                for a in self.u32(uv, mc):
                    o += self.u32(a, 2 * k)
            o += self.u32(col, 3 * k)

    def add(self, c, zone):
        self.cpu.heap_next = self.mark
        self.sizes.clear()
        self.room.clear()
        m = self.alloc(0x68)
        self.cpu.write(m + 8, struct.pack('<I', 3))
        o = []
        z = self.put(zone)
        at = uv_at = 0
        for i, n in enumerate(c['nv']):
            pts = self.put(farr(c['pts'][at * 3:(at + n) * 3]))
            nrm = self.put(farr(c['normals'][at * 3:(at + n) * 3])) if c['normals'] else 0
            col = self.put(farr(c['colors'][at * 3:(at + n) * 3])) if c['colors'] else 0
            uvs, mats = [], []
            for j in range(c['mc'][i]):
                uvs.append(self.put(farr(c['uvs'][uv_at:uv_at + 2 * n])))
                mats.append(self.mats + 0x2c * ((i + j) % MATS))
                uv_at += 2 * n
            u = self.put(struct.pack(f'<{len(uvs)}I', *uvs))
            ml = self.put(struct.pack(f'<{len(mats)}I', *mats))
            f = c['flags'][i]
            o.append(self.call(ADD, (m, n, pts, nrm, u, 0, 0, 0, c['mc'][i], ml, i, f & 1, f >> 1 & 1, z, col)))
            at += n
        self.dump(m, o)
        return o

    def clip(self, c):
        self.cpu.heap_next = self.mark
        self.sizes.clear()
        self.room.clear()
        total = sum(c['nv'])
        vp = self.put(farr(c['pts']))
        cp = self.put(farr(c['colors']))
        np_ = self.put(farr(c['normals'])) if c['normals'] else 0
        polys = self.alloc(0x28 * len(c['nv']))
        at = uv_at = 0
        for i, n in enumerate(c['nv']):
            ix = self.put(struct.pack(f'<{n}I', *range(at, at + n)))
            uvs, mats = [], []
            for j in range(c['mc'][i]):
                uvs.append(self.put(farr(c['uvs'][uv_at:uv_at + 2 * n])))
                mats.append(self.mats + 0x2c * ((i + j) % 3))
                uv_at += 2 * n
            f = n | (c['flags'][i] & 3) << 10 | (0x1000 if c['normals'] else 0) | (0x2000 if c['flags'][i] & 4 else 0)
            self.cpu.write(polys + 0x28 * i, struct.pack('<10I', f, i, ix, ix if c['normals'] else 0, c['mc'][i],
                                                         self.put(struct.pack(f'<{len(mats)}I', *mats)),
                                                         self.put(struct.pack(f'<{len(uvs)}I', *uvs)),
                                                         self.put(farr(c['colors'][at * 3:(at + n) * 3])), self.alloc(4 * n), 0xffffff00))
            at += n
        m = self.alloc(0x68)
        self.cpu.write(m + 0x10, struct.pack('<III', len(c['nv']), total, total if c['normals'] else 0))
        self.cpu.write(m + 0x34, struct.pack('<III', polys, vp, np_))
        no = len(c['outline']) // 3
        op = self.put(farr(c['outline']))
        p = self.call(CLIP_NEW, (no, op))
        self.cpu.write(p, struct.pack('<I', self.call(WEILER_NEW, (self.u32(p + 4)[0], self.u32(p + 8)[0], 0))))
        out = self.alloc(4)
        o = [self.call(CLIP, (p, m, out))]
        k = self.u32(p + 8)[0]
        o.append(k)
        o += self.u32(self.u32(p + 4)[0], 3 * k)
        self.dump(self.u32(out)[0], o)
        self.dump(m, o)  # the source, as the clip's unstrip (0x5726f0) left it
        return o


def port(op, c, precision, brk):
    n = len(c['nv'])
    a = lambda t, v: (t * len(v))(*v) if v else None  # noqa: E731
    uvs = list(c['uvs'])
    if brk and uvs:
        uvs[0] += 0.25
    nv, mc, fl = a(I32, c['nv']), a(I32, c['mc']), a(I32, c['flags'])
    pts, uv, col, nrm, ol = a(F32, c['pts']), a(F32, uvs), a(F32, c['colors']), a(F32, c['normals']), a(F32, c.get('outline', []))
    out = (U32 * 400000)()
    r = probe.probe_pm_crater(op, n, nv, mc, fl, pts, uv, col, nrm, len(c.get('outline', [])) // 3, ol, precision, out, 400000)
    w = list(out[:r])
    return w[:-1], w[-1]


def add_case(r):
    k = r.choice([1, 1, 2, 3])
    c = dict(nv=[], mc=[], flags=[], pts=[], uvs=[], colors=[], normals=[])
    with_col, with_nrm = r.random() < .7, r.random() < .5
    kinds = []
    for _ in range(k):
        kind = r.choice(['flat', 'flat', 'big', 'bent', 'line'])
        kinds.append(kind)
        n = {'flat': r.randrange(3, 9), 'big': r.randrange(49, 58), 'bent': r.randrange(4, 9), 'line': r.randrange(4, 9)}[kind]
        rad = r.uniform(2, 40)
        cx, cy, cz = (r.uniform(-50, 50) for _ in range(3))
        pts = []
        for i in range(n):
            t = 2 * math.pi * i / n
            x, z = cx + rad * math.cos(t), cz + rad * math.sin(t)
            y = cy + (r.uniform(-3, 3) if kind == 'bent' else (x - cx) * .1)
            if kind == 'line' and i % 2 and i + 1 < n:
                x, y, z = (pts[-3] + cx + rad * math.cos(t + 2 * math.pi / n)) / 2, cy + (x - cx) * .1, (pts[-1] + cz + rad * math.sin(t + 2 * math.pi / n)) / 2
            pts += [x, y, z]
        c['nv'].append(n)
        c['pts'] += pts
        c['mc'].append(r.randrange(1, 4))
        c['flags'].append(r.randrange(4))
        c['uvs'] += [r.uniform(-4, 6) for _ in range(2 * n * c['mc'][-1])]
        if with_col:
            c['colors'] += [r.uniform(0, 255) for _ in range(3 * n)]
        if with_nrm:
            c['normals'] += [v for _ in range(n) for v in (0.0, 1.0, 0.0)]
    return c, kinds


def clip_case(r):
    cols, rows = r.randrange(1, 6), r.randrange(1, 6)
    size = r.choice([8.0, 16.0, 32.0])
    a, b, h = r.uniform(-.2, .2), r.uniform(-.2, .2), r.uniform(-20, 20)
    c = dict(nv=[], mc=[], flags=[], pts=[], uvs=[], colors=[], normals=[])
    with_nrm = r.random() < .4
    x0, z0 = r.uniform(-100, 100), r.uniform(-100, 100)
    strips = r.random() < .5  # rows as Direct3D strips (0x2000), as PM's ground has them: unstripped by the clip (0x5726f0)
    for j in range(rows):
        if strips and r.random() < .7:
            row = [[(i, j + 1) if k % 2 == 0 else (i, j) for i in range(cols + 1) for k in (0, 1)]]
            if r.random() < .3 and len(row[0]) > 4:  # a repeated point, as the shipped strips join
                k = r.randrange(1, len(row[0]) - 1)
                row[0].insert(k, row[0][k])
            cells = [(q, 4) for q in row]
        else:
            cells = []
            for i in range(cols):
                corners = [(i, j), (i + 1, j), (i + 1, j + 1), (i, j + 1)]
                if r.random() < .5:  # two triangles
                    cells += [(corners[:3], 0), ([corners[0], corners[2], corners[3]], 0)]
                else:
                    cells += [(corners, 0)]
        for q, strip in cells:
                n = len(q)
                for u, v in q:
                    x, z = x0 + u * size, z0 + v * size
                    c['pts'] += [x, a * (x - x0) + b * (z - z0) + h, z]
                c['nv'].append(n)
                c['mc'].append(r.randrange(1, 3))
                c['flags'].append(r.randrange(4) | strip)
                c['uvs'] += [r.uniform(-3, 3) for _ in range(2 * n * c['mc'][-1])]
                c['colors'] += [r.uniform(0, 255) for _ in range(3 * n)]
                if with_nrm:
                    c['normals'] += [v2 for _ in range(n) for v2 in (0.0, 1.0, 0.0)]
    n = r.randrange(6, 13)
    rad = r.uniform(1.5, size * 1.5)
    cx, cz = x0 + r.uniform(-size, (cols + 1) * size), z0 + r.uniform(-size, (rows + 1) * size)
    c['outline'] = []
    for i in range(n):
        t = 2 * math.pi * i / n
        x, z = cx + rad * math.cos(t), cz + rad * math.sin(t)
        c['outline'] += [x, a * (x - x0) + b * (z - z0) + h, z]
    return c


def same(a, b, cw):
    """(ok, floats near but not equal, the words differ past that)"""
    if a == b:
        return True, 0, False
    if cw == 0x007f or len(a) != len(b):
        return False, 0, True
    far = near = 0
    for x, y in zip(a, b):
        if x == y:
            continue
        fx, fy = struct.unpack('<2f', struct.pack('<2I', x, y))
        if math.isfinite(fx) and math.isfinite(fy) and abs(fx - fy) < 1e-3 * max(1.0, abs(fx)):
            near += 1
        else:
            far += 1
    return far == 0, near, far > 0


def main():
    cases = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    brk = os.environ.get('BREAK') == '1'
    for cw in (0x027f, 0x007f):
        probe.probe_pm_set_fpu53(1 if cw == 0x027f else 0)
        e = Exe(cw)
        kinds = {}
        cut = whole = near_cases = near_words = flips = strips = 0
        for seed in range(cases):
            r = random.Random(seed)
            precision = r.choice([0.001, 0.005])
            e.cpu.write(PRECISION, struct.pack('<f', precision))
            if seed % 2:
                c, ks = add_case(r)
                for k in ks:
                    kinds[k] = kinds.get(k, 0) + 1
                a = e.add(c, bytes((0, 0xff, 0xff, 0xff)))
                b, left = port(0, c, precision, brk)
            else:
                c = clip_case(r)
                strips += any(f & 4 for f in c['flags'])
                a = e.clip(c)
                b, left = port(1, c, precision, brk)
                if a[0] == 1:
                    if a[2 + 3 * a[1]] == 0xffffffff:
                        whole += 1
                    else:
                        cut += 1
            ok, near, past = same(a, b, cw)
            if past and cw == 0x027f and not seed % 2 and not left:
                flips += 1
                ok = True
            if not ok or left:
                i = next((i for i, (x, y) in enumerate(zip(a, b)) if x != y), min(len(a), len(b)))
                print('differs at seed', seed, 'word', i, 'of', len(a), len(b), 'left', left, a[max(0, i - 3):i + 4], b[max(0, i - 3):i + 4])
                sys.exit(1)
            near_cases += 1 if near else 0
            near_words += near
        print(f'fpu {cw:#x}: ok: {cases} cases, adds {kinds}, cut {cut}, untouched {whole}; '
              f'floats within 1e-3 but not equal in {near_cases} cases ({near_words} words), other branch in {flips} cuts')
        if flips > cases // 500:
            sys.exit(f'{flips} cuts took another branch: more than before')
        pm_floors.floors(strips=(strips, cases // 8), cut=(cut, cases // 8), big=(kinds.get('big', 0), cases // 16), bent=(kinds.get('bent', 0), cases // 16))


if __name__ == '__main__':
    main()
