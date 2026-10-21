"""Weiler–Atherton (parts 3a..3c) against the original x86 (docs/craters.md,
section 5). Whole clips 0x50faf0 (two polygons in turn through one Weiler,
every result list wanted or not: the merge 0x5129f0 / 0x5154d0, the count
0x513390 / 0x514710, the output 0x513480 / 0x5135a0); the walk 0x510da0 and
the shared edges 0x510250 on their own; and part 3a: the state 0x50f960
builds (repeats dropped 0x515360, axes 0x513680, far regions moved 0x514c40,
the rings and contours 0x50fe70 / 0x5146a0 / 0x5129a0 / 0x514680 / 0x5136f0)
and its reset 0x514d10; the driver's first steps as 0x50faf0 takes them (the
case 0x50ff70 / 0x510180 / 0x513cf0, the whole-polygon results 0x5139e0 /
0x513a90 / 0x515040, the results' z 0x514e40, the offset back 0x514dd0) and
the side tables 0x513750; the leaves 0x514f80, 0x515170, 0x515410.

    cmake --build build && .venv/bin/python tests/differential/test_weiler.py

Made-up regions (crater rings, with repeated points, some past 65,536 m)
and polygons apart from them, around them, inside them, equal to them and
across them; random leaves. The x87 at 24 bits. Compared: the state by
value (pointers as array and index), the results, the tables.

BREAK=<n> breaks the port's copy of the inputs in a known way (see main);
each must fail.
"""
import ctypes
import math
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import FTOL, IAT_FTOL, ROOT, Original  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
P = ctypes.c_void_p
probe.probe_weiler_new.argtypes = [P, ctypes.c_int, P, ctypes.c_int]
probe.probe_weiler_case.argtypes = [P, ctypes.c_int, P, ctypes.c_int, ctypes.c_uint32, P, ctypes.c_int]
probe.probe_weiler_weed.argtypes = [P, ctypes.c_int, P, ctypes.c_int, P, ctypes.c_int]
probe.probe_weiler_walk.argtypes = [P, ctypes.c_int, P, ctypes.c_int, P, ctypes.c_int]
probe.probe_weiler_clip.argtypes = [P, ctypes.c_int, P, ctypes.c_int, P, ctypes.c_int, ctypes.c_uint32, P,
                                    ctypes.c_int]
probe.probe_weiler_leaf.argtypes = [ctypes.c_int, P, ctypes.c_int, P, ctypes.c_int, ctypes.c_float, ctypes.c_float,
                                    ctypes.c_int, P, ctypes.c_int]

IAT_MALLOC, IAT_CALLOC, IAT_FREE, IAT_REALLOC, IAT_MEMMOVE = 0x5bb664, 0x5bb60c, 0x5bb65c, 0x5bb610, 0x5bb584
IAT_FPRINTF = 0x5bb62c
ZERR = 0x49bfd0
CAP = 1 << 20
GAVE_UP = struct.pack('<III', 0x70, 0x100, 0)  # the port's walk giving up (zerr line 0)


def fb(v):
    return struct.unpack('<I', struct.pack('<f', v))[0]


def farr(vals):
    return struct.pack(f'<{len(vals)}f', *vals)


class Emulated:
    def __init__(self):
        cpu = self.cpu = Original()
        cpu.import_code(IAT_FTOL, FTOL)
        cpu.fpu_control(0x007f)
        self.sizes = {}
        cpu.stub_import(IAT_MALLOC, lambda c: self.alloc(c.arg(0)))
        cpu.stub_import(IAT_CALLOC, lambda c: self.alloc(c.arg(0) * c.arg(1)))
        cpu.stub_import(IAT_REALLOC, self.realloc)
        cpu.stub_import(IAT_FREE, lambda c: 0)
        cpu.stub_import(IAT_MEMMOVE, self.memmove)
        cpu.stub_import(IAT_FPRINTF, lambda c: 0)
        cpu.stub_function(ZERR, lambda c: self.log(0x70, c.arg(0), c.arg(2)), 0)
        self.mark = cpu.heap_next

    def begin(self):
        self.cpu.heap_next = self.mark
        self.sizes.clear()
        self.out = bytearray()

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

    def raw(self, a, nbytes):
        self.out += self.cpu.read(a, nbytes)

    # --- the state ---------------------------------------------------------
    def point_ref(self, w, p):
        for kind, base, n in ((1, self.caller, self.caller_n), (2, self.u32(w + 0x18), self.u32(w + 0x14)),
                              (3, self.u32(w + 0x2c), self.u32(w + 0x28))):
            if base and base <= p < base + 12 * n:
                return self.log(kind, (p - base) // 12)
        xb = self.u32(w + 0x68)
        if xb and xb <= p < xb + 0x30 * self.u32(w + 0x60) and (p - xb) % 0x30 == 0:
            return self.log(4, (p - xb) // 0x30)
        self.log(9 if p else 0, 0)

    def dump(self, w):
        nr = self.u32(w + 0x14)
        self.log(self.u32(w), self.u32(w + 4), nr)
        self.raw(self.u32(w + 0x18), 12 * nr)
        self.raw(w + 0x28c0, 8)
        self.log(self.cpu.read(w + 0x28c8, 1)[0])
        ebase, ecount = self.u32(w + 0x40), self.u32(w + 0x3c)
        cbase, ccount = self.u32(w + 0x54), self.u32(w + 0x50)

        def eref(p):
            if not p:
                return 0xffff
            return (p - ebase) // 0x3c if ebase <= p < ebase + 0x3c * self.u32(w + 0x38) else 0xfffe

        def cref(p):
            if not p:
                return 0xffff
            return (p - cbase) // 0xc if cbase <= p < cbase + 0xc * self.u32(w + 0x4c) else 0xfffe
        self.log(ecount)
        for i in range(ecount):
            e = ebase + 0x3c * i
            self.log(eref(self.u32(e)), eref(self.u32(e + 4)), self.u32(e + 8))
            self.point_ref(w, self.u32(e + 0xc)), self.point_ref(w, self.u32(e + 0x10))
            self.log(self.u32(e + 0x14))
            self.raw(e + 0x18, 8), self.raw(e + 0x24, 8)
            self.log(cref(self.u32(e + 0x38)))
        self.log(ccount)
        for i in range(ccount):
            c = cbase + 0xc * i
            self.log(self.u32(c), eref(self.u32(c + 4)))
        for i in range(ccount):
            first = e = self.u32(cbase + 0xc * i + 4)
            for _ in range(200):
                if not e:
                    break
                self.log(eref(e), self.u32(e + 8))
                self.point_ref(w, self.u32(e + 0xc)), self.point_ref(w, self.u32(e + 0x10))
                self.raw(e + 0x18, 8), self.raw(e + 0x24, 8)
                e = self.u32(e + 4)
                if e == first:
                    break
            self.log(0xeeee)

    def weiler_new(self, points):
        self.begin()
        n = len(points) // 3
        self.caller, self.caller_n = self.put(farr(points)), n
        w = self.call(0x50f960, ecx=self.caller, edx=n, stack=(0,))
        self.log(self.u32(w + 0x14))
        self.raw(self.caller, 12 * n)
        self.dump(w)
        slot = self.alloc(4)
        self.log(self.call(0x50f950, ecx=w, edx=slot), self.u32(slot) == self.u32(w + 0x18))
        self.call(0x514d10, ecx=w)
        self.dump(w)
        return bytes(self.out)

    def case(self, region, poly, want):
        self.begin()
        nr, np_ = len(region) // 3, len(poly) // 3
        self.caller, self.caller_n = self.put(farr(region)), nr
        w = self.call(0x50f960, ecx=self.caller, edx=nr, stack=(0,))
        pp = self.put(farr(poly))
        self.cpu.write(w, struct.pack('<I', want))
        self.cpu.write(w + 0x28, struct.pack('<II', np_, pp))
        offset_on = self.cpu.read(w + 0x28c8, 1)[0]
        if offset_on:
            self.call(0x514c40, ecx=w)
        for off, size in ((0x70, 8), (0x84, 8), (0x98, 8), (0xac, 0xc)):
            self.call(0x5128e0, ecx=w + off, edx=0x80, stack=(size,))
        out = self.alloc(0x20)
        self.cpu.write(w + 8, struct.pack('<I', out))
        self.cpu.write(out, struct.pack('<8I', 0, self.u32(w + 0x7c), 0, self.u32(w + 0x90), 0, self.u32(w + 0xa4), 0,
                                        self.u32(w + 0xb8)))
        code = self.call(0x50ff70, ecx=w)
        self.log(code)
        if code in (3, 4):
            self.log(self.call(0x5139e0, ecx=w, edx=3) & 0xff)
        if code == 2:
            self.log(self.call(0x513a90, ecx=w) & 0xff)
            self.log(self.call(0x5139e0, ecx=w, edx=2) & 0xff)
        if code >= 2:
            self.call(0x514e40, ecx=w)
        if offset_on:
            self.call(0x514dd0, ecx=w)
        for k in range(3):
            cnt, base = self.u32(out + 8 * k), self.u32(out + 8 * k + 4)
            self.log(cnt)
            self.raw(base, 8 * cnt)
        npts = self.u32(out + 0x18)
        self.log(npts)
        self.raw(self.u32(out + 0x1c), 12 * npts)
        self.raw(pp, 12 * np_)
        self.call(0x514d10, ecx=w)
        self.call(0x513750, ecx=w)
        nr = self.u32(w + 0x14)
        self.raw(w + 0xc0, 4 * np_ * (nr + 1))
        self.raw(w + 0x14c0, 4 * nr * (np_ + 1))
        return bytes(self.out)

    def weed(self, region, poly):
        self.begin()
        nr, np_ = len(region) // 3, len(poly) // 3
        self.caller, self.caller_n = self.put(farr(region)), nr
        w = self.call(0x50f960, ecx=self.caller, edx=nr, stack=(0,))
        pp = self.put(farr(poly))
        self.cpu.write(w + 0x28, struct.pack('<II', np_, pp))
        self.call(0x514d10, ecx=w)
        self.call(0x50fe70, ecx=w, edx=pp, stack=(np_, 2))
        self.call(0x513750, ecx=w)
        self.log(self.call(0x510250, ecx=w) & 0xff)
        self.dump(w)
        return bytes(self.out)

    def walk(self, region, poly, budget=50_000_000):
        self.begin()
        nr, np_ = len(region) // 3, len(poly) // 3
        self.caller, self.caller_n = self.put(farr(region)), nr
        w = self.call(0x50f960, ecx=self.caller, edx=nr, stack=(0,))
        pp = self.put(farr(poly))
        self.cpu.write(w + 0x28, struct.pack('<II', np_, pp))
        self.call(0x514d10, ecx=w)
        self.call(0x50fe70, ecx=w, edx=pp, stack=(np_, 2))
        self.call(0x513750, ecx=w)
        self.log(self.call(0x510250, ecx=w) & 0xff)
        self.cpu.call(0x510da0, w, 0, (), count=budget)
        self.log(self.cpu.uc.reg_read(UC_X86_REG_EAX))
        self.dump(w)
        ebase, ecap = self.u32(w + 0x40), self.u32(w + 0x38)
        xb, xcap, xcount = self.u32(w + 0x68), self.u32(w + 0x60), self.u32(w + 0x64)

        def eref(p):
            if not p:
                return 0xffff
            return (p - ebase) // 0x3c if ebase <= p < ebase + 0x3c * ecap else 0xfffe

        def xref(p):
            if not p:
                return 0xffff
            return (p - xb) // 0x30 if xb <= p < xb + 0x30 * xcap else 0xfffe
        self.log(xcount)
        for i in range(xcount):
            c = xb + 0x30 * i
            self.raw(c, 12)
            self.log(self.u32(c + 0x2c))
            for k in range(8):
                self.log(eref(self.u32(c + 0xc + 4 * k)))
        for i in range(self.u32(w + 0x3c)):
            e = ebase + 0x3c * i
            self.log(xref(self.u32(e + 0x30)), xref(self.u32(e + 0x34)), self.u32(e + 0x14))
        return bytes(self.out)

    def clip(self, region, polys, want, budget=50_000_000):
        self.begin()
        nr = len(region) // 3
        self.caller, self.caller_n = self.put(farr(region)), nr
        w = self.call(0x50f960, ecx=self.caller, edx=nr, stack=(0,))
        for poly in polys:
            np_ = len(poly) // 3
            pp = self.put(farr(poly))
            out = self.alloc(0x20)
            self.cpu.call(0x50faf0, w, want, (pp, np_, out), count=budget)
            code = self.cpu.uc.reg_read(UC_X86_REG_EAX)
            self.log(code)
            if code:
                for k in range(3):
                    cnt = self.u32(out + 8 * k)
                    self.log(cnt)
                    self.raw(self.u32(out + 8 * k + 4), 8 * cnt)
                npts = self.u32(out + 0x18)
                self.log(npts)
                self.raw(self.u32(out + 0x1c), 12 * npts)
            self.raw(pp, 12 * np_)
            cb, cn = self.u32(w + 0x54), self.u32(w + 0x50)
            self.log(cn)
            for i in range(cn):
                c = cb + 0xc * i
                self.log(self.u32(c), self.u32(c + 8), self.u32(c + 4) != 0)
            self.log(self.cpu.read(w + 0x28c9, 1)[0])
        return bytes(self.out)

    def leaf(self, op, a, b, eps1=0.0, eps2=0.0, code=0):
        self.begin()
        pa = self.put(farr(a) or bytes(12))
        pb = self.put(farr(b) or bytes(12))
        na, nb = len(a) // 3, len(b) // 3
        if op == 0:
            self.log(self.call(0x513cf0, ecx=pa, edx=nb, stack=(pb,)) & 0xff)
        elif op == 1:
            self.log(self.call(0x510180, ecx=na, edx=pa, stack=(nb, pb, code)))
        elif op == 2:
            self.log(self.call(0x515410, ecx=pa, edx=na, stack=(pb, nb, fb(eps1), fb(eps2))))
            self.raw(pb, 12 * nb)
        elif op == 3:
            self.log(self.call(0x515170, ecx=pa, edx=pa + 12, stack=(pb, fb(eps1))))
            self.raw(pb, 12)
        elif op == 4:
            self.log(self.call(0x514f80, ecx=pb, edx=pa, stack=(pa + 12,)))
        else:
            self.log(self.call(0x515360, ecx=pb, edx=nb))
            self.raw(pb, 12 * nb)
        return bytes(self.out)


# --- the port ----------------------------------------------------------------
BUF = ctypes.create_string_buffer(CAP)


def fbuf(vals):
    return ctypes.create_string_buffer(farr(vals) or bytes(12), max(4 * len(vals), 12))


def got(n):
    assert n <= CAP
    return BUF.raw[:n]


# --- inputs ------------------------------------------------------------------
def ring(rng, cx, cy, r, n, z=0.0, jitter=0.0, repeats=0):
    pts = []
    for i in range(n):
        a = 2 * math.pi * i / n + rng.uniform(-jitter, jitter)
        rr = r * rng.uniform(1 - jitter, 1 + jitter) if jitter else r
        pts.append([cx + rr * math.cos(a), cy + rr * math.sin(a), z + rng.uniform(-1, 1)])
    for _ in range(repeats):
        i = rng.randrange(n)
        q = list(pts[i])
        q[0] += rng.choice((0.0, 0.004, -0.009))
        pts.insert(i + 1, q)
    if repeats and rng.random() < 0.3:
        pts.append(list(pts[0]))
    return [v for p in pts for v in p]


def polygon(rng, region, kind):
    xs, ys = region[0::3], region[1::3]
    cx, cy = sum(xs) / len(xs), sum(ys) / len(ys)
    r = max(math.hypot(x - cx, y - cy) for x, y in zip(xs, ys))
    n = rng.randint(3, 6)
    if kind == 'apart':
        d = r * rng.uniform(2.5, 5)
        a = rng.choice((0.0, math.pi / 2, rng.uniform(0, 2 * math.pi)))
        return ring(rng, cx + d * math.cos(a), cy + d * math.sin(a), r * rng.uniform(0.3, 1), n, 5, 0.1)
    if kind == 'around':
        return ring(rng, cx + rng.uniform(-r, r) * 0.2, cy + rng.uniform(-r, r) * 0.2, r * rng.uniform(2.2, 4), n, 5)
    if kind == 'inside':
        return ring(rng, cx + rng.uniform(-r, r) * 0.1, cy + rng.uniform(-r, r) * 0.1, r * rng.uniform(0.1, 0.5), n, 5)
    if kind == 'same':
        pts = [region[3 * i:3 * i + 3] for i in range(len(region) // 3)]
        k = rng.randrange(len(pts))
        pts = pts[k:] + pts[:k]
        return [v for p in pts for v in p]
    if kind == 'boxin':  # a box inside the region's box that crosses the region
        return [cx - r * 0.95, cy - r * 0.95, 3, cx + r * 0.95, cy - r * 0.95, 3, cx + r * 0.95, cy + r * 0.95, 4,
                cx - r * 0.95, cy + r * 0.95, 5]
    return ring(rng, cx + r * rng.uniform(0.6, 1.2), cy + rng.uniform(-r, r) * 0.3, r * rng.uniform(0.5, 1.2), n, 5)


def weed_case(rng):
    """A region and a polygon on a grid: rectangles (or diamonds) sharing,
    overlapping or reversing each other's sides."""
    g = rng.choice((1.0, 2.0, 0.5))
    ox, oy = rng.randint(-50, 50) * g, rng.randint(-50, 50) * g

    def rect(x0, y0, x1, y1, z):
        pts = [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]
        if rng.random() < 0.5:
            pts.reverse()
        k = rng.randrange(4)
        pts = pts[k:] + pts[:k]
        if rng.random() < 0.3:  # an extra point along a side
            i = rng.randrange(4)
            a, b = pts[i], pts[(i + 1) % 4]
            pts.insert(i + 1, ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2))
        return [v for x, y in pts for v in (ox + x * g, oy + y * g, z)]
    if rng.random() < 0.2:
        d = rng.randint(1, 4)
        region = [v for x, y in ((0, -d), (d, 0), (0, d), (-d, 0)) for v in (ox + x * g, oy + y * g, 1.0)]
        e = rng.randint(1, 4)
        sx, sy = rng.randint(-2, 2), rng.randint(-2, 2)
        poly = [v for x, y in ((sx, sy - e), (sx + e, sy), (sx, sy + e), (sx - e, sy)) for v in
                (ox + x * g, oy + y * g, 2.0)]
    else:
        w0, h0 = rng.randint(1, 6), rng.randint(1, 6)
        region = rect(0, 0, w0, h0, 1.0)
        x0, y0 = rng.randint(-3, w0 + 2), rng.randint(-3, h0 + 2)
        x0 = rng.choice((x0, 0, w0, -rng.randint(1, 3)))
        poly = rect(x0, y0, x0 + rng.randint(1, 6), y0 + rng.randint(1, 6), 3.0)
    return region, poly


def walk_case(rng):
    """A region and a polygon that cross: grid pairs, or a crater ring and a
    triangle or quad across it, some with an edge through a region point or
    a corner on a region point or edge."""
    if rng.random() < 0.5:
        return weed_case(rng)
    region = ring(rng, rng.uniform(-50, 50), rng.uniform(-50, 50), rng.uniform(3, 20), rng.randint(3, 12), 1.0,
                  rng.choice((0, 0.1)))
    poly = polygon(rng, region, rng.choice(('cross', 'cross', 'boxin', 'around')))
    if rng.random() < 0.25:  # a polygon edge through a region point
        i = rng.randrange(len(region) // 3)
        a = rng.choice((0.0, math.pi / 2, rng.uniform(0, 2 * math.pi)))
        dx, dy, k = round(math.cos(a), 6), round(math.sin(a), 6), rng.choice((1.0, 2.0, 0.5))
        poly[0], poly[1] = region[3 * i] + k * dx, region[3 * i + 1] + k * dy
        poly[3], poly[4] = region[3 * i] - k * dx, region[3 * i + 1] - k * dy
    elif rng.random() < 0.3:  # a polygon corner on a region point or edge
        i = rng.randrange(len(region) // 3)
        j = (i + 1) % (len(region) // 3)
        t = rng.choice((0.0, 0.5, rng.random()))
        poly[0] = region[3 * i] + t * (region[3 * j] - region[3 * i])
        poly[1] = region[3 * i + 1] + t * (region[3 * j + 1] - region[3 * i + 1])
    return region, poly


def trivial_case(rng, region):
    return region, polygon(rng, region, rng.choice(('apart', 'around', 'inside', 'same', 'boxin', 'cross')))


def main():
    rng = random.Random(int(os.environ.get('SEED', '1')))
    brk = int(os.environ.get('BREAK', '0'))
    only = os.environ.get('ONLY')  # one section: new, case, weed, walk, clip, leaf
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

    def region_of(rng, big=False):
        far = rng.random() < 0.2
        cx = rng.choice((70000.0, -90000.5)) if far else rng.uniform(-500, 500)
        cy = rng.uniform(-500, 500) if not far or rng.random() < 0.5 else 80000.25
        n = rng.randint(100, 150) if big and rng.random() < 0.1 else rng.randint(3, 16)
        return ring(rng, cx, cy, rng.uniform(3, 30), n, 2.0, rng.choice((0, 0.05, 0.2)), rng.choice((0, 0, 1, 3)))

    # weiler_new and reset.
    for _ in range(300 if only in (None, 'new') else 0):
        region = region_of(rng, big=True)
        mine_region = list(region)
        if brk == 1:
            mine_region[0] += 0.001
        n = len(region) // 3
        b = fbuf(mine_region)
        mine = got(probe.probe_weiler_new(ctypes.cast(b, P), n, BUF, CAP))
        check('new', mine, emu.weiler_new(region))

    # The driver's first steps.
    for _ in range(600 if only in (None, 'case') else 0):
        region = region_of(rng)
        kind = rng.choice(('apart', 'around', 'inside', 'same', 'boxin', 'cross'))
        poly = polygon(rng, region, kind)
        want = rng.choice((1, 2, 3, 7))
        mine_poly = list(poly)
        if brk == 2 and kind == 'around':
            mine_poly[2] += 1.0
        if brk == 3 and kind == 'cross':
            mine_poly[0] += 0.5
        nr, np_ = len(region) // 3, len(poly) // 3
        mine = got(probe.probe_weiler_case(ctypes.cast(fbuf(region), P), nr, ctypes.cast(fbuf(mine_poly), P), np_,
                                           want, BUF, CAP))
        check('case_' + kind, mine, emu.case(region, poly, want))

    # Shared edges: rectangles and diamonds on a grid, the polygon sharing,
    # overlapping or reversing the region's sides.
    for _ in range(800 if only in (None, 'weed') else 0):
        region, poly = weed_case(rng)
        mine_poly = list(poly)
        if brk == 6:
            mine_poly[3] += 1e-3
        nr, np_ = len(region) // 3, len(poly) // 3
        mine = got(probe.probe_weiler_weed(ctypes.cast(fbuf(region), P), nr, ctypes.cast(fbuf(mine_poly), P), np_,
                                           BUF, CAP))
        check('weed', mine, emu.weed(region, poly))

    # The walk: crater rings against triangles and quads crossing them, and
    # the grid pairs (touching ends, shared corners, collinear runs).
    for _ in range(1200 if only in (None, 'walk') else 0):
        region, poly = walk_case(rng)
        mine_poly = list(poly)
        if brk == 7:
            mine_poly[1] += 0.01
        nr, np_ = len(region) // 3, len(poly) // 3
        mine = got(probe.probe_weiler_walk(ctypes.cast(fbuf(region), P), nr, ctypes.cast(fbuf(mine_poly), P), np_,
                                           BUF, CAP))
        if GAVE_UP in mine:
            # The port's FIX: the original loops here; confirm it never returns.
            try:
                emu.walk(region, poly, budget=20_000_000)
                check('walk_hang', mine, b'original returned')
            except Exception:
                kinds['walk_hang'] = kinds.get('walk_hang', 0) + 1
                emu = Emulated()
            continue
        try:
            theirs = emu.walk(region, poly, budget=20_000_000)
        except Exception:  # the original looped where the port did not give up
            theirs = b'original never returned'
            emu = Emulated()
        check('walk', mine, theirs)

    # Whole clips, two polygons in turn through one Weiler: every case above,
    # crater rings far out too, and every result list wanted or not.
    for _ in range(1500 if only in (None, 'clip') else 0):
        region, p1 = walk_case(rng) if rng.random() < 0.6 else trivial_case(rng, region_of(rng))
        polys = [p1]
        if rng.random() < 0.5:
            polys.append(polygon(rng, region, rng.choice(('cross', 'apart', 'inside', 'boxin'))))
        want = rng.choice((1, 2, 3, 4, 5, 7))
        mine_polys = [list(p) for p in polys]
        if brk == 8:
            mine_polys[0][2] += 0.5
        mine_want = want ^ 4 if brk == 9 else want
        bufs = [fbuf(p) for p in mine_polys] + [fbuf([])]
        n2 = len(polys[1]) // 3 if len(polys) > 1 else 0
        mine = got(probe.probe_weiler_clip(ctypes.cast(fbuf(region), P), len(region) // 3, ctypes.cast(bufs[0], P),
                                           len(p1) // 3, ctypes.cast(bufs[1], P), n2, mine_want, BUF, CAP))
        if GAVE_UP in mine:
            try:
                emu.clip(region, polys, want, budget=5_000_000)
                check('clip_hang', mine, b'original returned')
                if os.environ.get('SHOW'):
                    print('case', region, polys, want)
            except Exception:
                kinds['clip_hang'] = kinds.get('clip_hang', 0) + 1
                emu = Emulated()
            continue
        try:
            theirs = emu.clip(region, polys, want, budget=5_000_000)
        except Exception:
            theirs = b'original never returned'
            emu = Emulated()
        if os.environ.get('SHOW') and mine != theirs:
            print('case', region, polys, want)
        at = 0
        while mine[at:at + 4] == b'\x70\0\0\0':  # error reports come first
            at += 12
        code = struct.unpack_from('<I', mine, at)[0] if len(mine) >= at + 4 else 9
        check(f'clip_{code}', mine, theirs)

    # Leaves.
    for _ in range(1500 if only in (None, 'leaf') else 0):
        op = rng.randrange(6)
        if op in (0, 1):
            poly = ring(rng, 0, 0, rng.uniform(2, 10), rng.randint(3, 9), 0, 0.3)
            if op == 0:
                v = rng.randrange(len(poly) // 3)
                q = rng.choice(([rng.uniform(-12, 12), rng.uniform(-12, 12), 0], poly[3 * v:3 * v + 3],
                                [poly[3 * v], rng.uniform(-12, 12), 0], [rng.uniform(-12, 12), poly[3 * v + 1], 0]))
                a, b, code = q, poly, 0
            else:
                a = ring(rng, rng.uniform(-3, 3), rng.uniform(-3, 3), rng.uniform(0.5, 6), rng.randint(3, 9), 0, 0.2)
                if rng.random() < 0.2:
                    a = list(poly)
                a, b, code = a, poly, rng.choice((2, 3))
            eps1 = eps2 = 0.0
        elif op == 2:
            a = ring(rng, 0, 0, 5, rng.randint(3, 6), 1, 0.2)
            b = []
            for _ in range(rng.randint(1, 8)):
                i = rng.randrange(len(a) // 3)
                j = (i + 1) % (len(a) // 3)
                t = rng.choice((0.0, rng.uniform(0, 1)))
                b += [a[3 * i] + t * (a[3 * j] - a[3 * i]) + rng.uniform(-0.15, 0.15),
                      a[3 * i + 1] + t * (a[3 * j + 1] - a[3 * i + 1]) + rng.uniform(-0.15, 0.15), 2.0]
            eps1, eps2, code = 0.1, 0.1, 0
        elif op == 3:
            a = [rng.uniform(-5, 5), rng.uniform(-5, 5), 0]
            a += rng.choice(([a[0] + rng.uniform(-5, 5), a[1] + rng.uniform(-5, 5), 0],
                             [a[0] + rng.uniform(-0.05, 0.05), a[1] + rng.uniform(-5, 5), 0],
                             [a[0] + rng.uniform(-5, 5), a[1] + rng.uniform(-0.05, 0.05), 0]))
            t = rng.uniform(-0.2, 1.2)
            b = [a[0] + t * (a[3] - a[0]) + rng.uniform(-0.12, 0.12), a[1] + t * (a[4] - a[1]) + rng.uniform(-0.12, 0.12),
                 7.0]
            eps1, eps2, code = rng.choice((0.1, 0.01)), 0.0, 0
        elif op == 4:
            a = [rng.uniform(-5, 5), rng.uniform(-5, 5), 0]
            a += [a[0] + rng.choice((0.0, 0.000005, rng.uniform(-5, 5))), a[1] + rng.uniform(-5, 5), 0]
            b = [rng.uniform(-6, 6), rng.uniform(-6, 6), 0]
            eps1 = eps2 = 0.0
            code = 0
        else:
            a = [0, 0, 0]
            b = ring(rng, 0, 0, 5, rng.randint(1, 8), 0, 0, rng.choice((0, 1, 2, 4)))
            eps1 = eps2 = 0.0
            code = 0
        mine_b = list(b)
        if brk == 4 and op == 0 and rng.random() < 0.5:
            mine_b = mine_b[3:] + mine_b[:3]
            mine_b[0] += 0.3
        mine_eps1 = eps1 * 2 if brk == 5 and op == 3 else eps1
        bb = fbuf(mine_b)
        mine = got(probe.probe_weiler_leaf(op, ctypes.cast(fbuf(a), P), len(a) // 3, ctypes.cast(bb, P), len(b) // 3,
                                           mine_eps1, eps2, code, BUF, CAP))
        check(('in_polygon', 'all_inside', 'snap', 'snap_segment', 'within', 'repeats')[op], mine,
              emu.leaf(op, a, b, eps1, eps2, code))

    if fails:
        print(f'{fails} of {runs} runs differ')
        sys.stdout.flush(); os._exit(1)
    print(f'ok: {runs} runs identical ({", ".join(f"{k} {v}" for k, v in sorted(kinds.items()))})')
    sys.stdout.flush(); os._exit(0)


if __name__ == '__main__':
    main()
