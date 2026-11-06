"""Pirate's Moon's world queries (zclass/cls_di, the model tests in zmodel/gmod_hit on PM's models) against PM's exe on
its worlds: the ground query 0x5003a0, the single point 0x4ff8d0, the segment query 0x502360, the shot queries 0x501030
and 0x500f80, the sphere query 0x5033b0, with PM's model tests 0x5675a0, 0x566d60, 0x56a600 (0x569290, 0x569b00), 0x567b20, and a cell's clutter
records (0x4ffb40, 0x4ffb90, 0x4ffc80).

    cmake --build build --target pmprobe
    .venv/bin/python -P tests/differential/test_pm_di.py [queries per world]

Every hit is compared as bits: normal, height or point, zones, material and node slots, whether it has an owner.
"""
import ctypes
import math
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import pm_floors  # noqa: E402
import test_pm_gamez as g  # noqa: E402
import test_pm_zar  # noqa: E402
from ai_world import FLOOR, IAT_FLOOR  # noqa: E402
from original import FTOL, IAT_FTOL  # noqa: E402

GROUND, POINT, SEGMENTS, LINES, NEAREST, SPHERE = 0x5003a0, 0x4ff8d0, 0x502360, 0x501030, 0x500f80, 0x5033b0
FIND_ZONE = 0x55d270
REC = 0x584
SLOT = 0x2c


def f2u(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


class Emulated(g.Emulated):
    def __init__(self, world):
        super().__init__()
        self.cpu.import_code(IAT_FTOL, FTOL)
        self.cpu.import_code(IAT_FLOOR, FLOOR)
        self.cpu.stub_import(0x5bb654, self.strncmp)  # the sphere query's name test
        self.cpu.fpu_control(0x007f)
        self.stack([world], 5, 6, 5, models=True, nodes=True)
        self.probes, self.out = self.alloc(4 * 3 * 64), self.alloc(REC * 24)
        self.a, self.b = self.alloc(12), self.alloc(12)
        # The model tests' scratch, made at the game's start: the morphed and the placed vertices.
        self.w32(0xa4e094, self.alloc(12 * 0x10000))
        self.w32(0xa4e098, self.alloc(12 * 0x10000))

    def strncmp(self, c):
        a, b = self.cstr(c.arg(0))[:c.arg(2)], self.cstr(c.arg(1))[:c.arg(2)]
        return ((a > b) - (a < b)) & 0xffffffff

    def node_at(self, i):
        blocks = self.i32(g.N_BLOCKS) & 0xffffffff
        return (self.i32(blocks + 4 * (i // 2500)) & 0xffffffff) + 0xd4 * (i % 2500)

    def node_slot(self, p):
        return self.pool_index(p, g.N_BLOCKS, g.N_NBLOCKS, 2500, 0xd4) if p else -1

    def world_node(self):
        for i in range(2500):
            n = self.node_at(i)
            if self.i32(n + 0x34) == 2:
                return i, n
        raise SystemExit('no world')

    def hit(self, raw):
        w = struct.unpack('<11I', raw)
        return [struct.unpack('<i', struct.pack('<I', v))[0] for v in w[:8]] + [
            self.mat_index(w[8]) if w[8] else -1, self.node_slot(w[9]), 1 if w[10] else 0]

    def ground(self, world, probes, top):
        n = len(probes) // 3
        self.cpu._uc.mem_write(self.probes, struct.pack(f'<{n * 3}f', *probes))
        self.cpu._uc.mem_write(self.out, bytes(REC * 24))
        r = self.call(GROUND, args=[world, self.probes, n, f2u(top), self.out]) & 0xffffffff
        o = [struct.unpack('<i', struct.pack('<I', r))[0]]
        for i in range(min(n, 24)):
            rec = self.read(self.out + REC * i, REC)
            o.append(rec[0])
            for k in range(rec[0]):
                o += self.hit(rec[4 + SLOT * k:4 + SLOT * (k + 1)])
        return o

    def point(self, world, x, y, z):
        self.cpu._uc.mem_write(self.out, bytes(REC))
        r = self.call(POINT, args=[world, f2u(x), f2u(y), f2u(z), self.out]) & 0xff
        rec = self.read(self.out, REC)
        o = [r, rec[0]]
        for k in range(rec[0]):
            o += self.hit(rec[4 + SLOT * k:4 + SLOT * (k + 1)])
        return o

    def clutter(self, world, cx, cz, node, recs, flags, boxes, leaf_of):
        """recs: 13 floats each (the matrix, the zone). boxes: the root's and four leaves', or None."""
        data = self.i32(world + 0x38) & 0xffffffff
        row = self.i32((self.i32(data + 0xa0) & 0xffffffff) + 4 * cz) & 0xffffffff
        cell = row + 0x58 * cx
        arr = self.alloc(0x5c * len(recs))
        for i, r in enumerate(recs):
            a = arr + 0x5c * i
            self.w32(a, node)
            self.cpu._uc.mem_write(a + 4, struct.pack('<12f', *r[:12]))
            self.w32(a + 0x40, int(r[12]))
            self.w32(a + 0x48, flags[i])
        self.w32(cell + 0x40, len(recs)), self.w32(cell + 0x44, arr)
        if not boxes:
            self.w32(cell + 0x50, 0), self.w32(cell + 0x54, 0)
            return
        t = self.alloc(0x40 * 5)
        for k in range(5):
            e = t + 0x40 * k
            self.cpu._uc.mem_write(e + 0xc, struct.pack('<6f', *boxes[k]))
            if k == 0:
                self.w32(e + 0x3c, 1)
                continue
            self.w32(e + 0x3c, 0xffffffff)
            ids = [i for i in range(len(recs)) if leaf_of[i] == k - 1]
            lst = self.alloc(4 * max(len(ids), 1))
            for j, i in enumerate(ids):
                self.w32(lst + 4 * j, i)
            self.w32(e + 0x34, len(ids)), self.w32(e + 0x38, lst)
        self.w32(cell + 0x50, t), self.w32(cell + 0x54, 5)

    def line(self, world, a, b, nearest):
        self.cpu._uc.mem_write(self.out, bytes(REC))
        if nearest:
            self.cpu._uc.mem_write(self.a, struct.pack('<3f', *a))
            self.cpu._uc.mem_write(self.b, struct.pack('<3f', *b))
            r = self.call(NEAREST, args=[world, self.a, self.b, self.out])
        else:
            r = self.call(LINES, args=[world, *map(f2u, a), *map(f2u, b), self.out])
        r = struct.unpack('<i', struct.pack('<I', r & 0xffffffff))[0]
        rec = self.read(self.out, REC)
        o = [r, rec[0], rec[1]]
        shown = rec[1] if nearest else rec[0]
        for k in range(min(shown, 32)):
            w = struct.unpack_from('<11I', rec, 4 + SLOT * k)
            o += [struct.unpack('<i', struct.pack('<I', v))[0] for v in w[:6]]
            o += [struct.unpack('<i', struct.pack('<I', w[7]))[0], self.mat_index(w[8]) if w[8] else -1, self.node_slot(w[9]), 1 if w[10] else 0]
        return o

    def sphere(self, world, centre, radius, prefix, ranged, sighted):
        self.cpu._uc.mem_write(self.out, bytes(REC))
        self.cpu._uc.mem_write(self.a, struct.pack('<3f', *centre))
        pre = self.string(prefix) if prefix else 0
        r = self.call(SPHERE, args=[world, self.a, pre, f2u(radius), ranged, sighted, self.out]) & 0xffffffff
        rec = self.read(self.out, REC)
        o = [struct.unpack('<i', struct.pack('<I', r))[0], rec[0], 0]
        for k in range(rec[0]):
            w = struct.unpack_from('<11i', rec, 4 + SLOT * k)
            o += [w[3], w[4], w[5], w[7], self.node_slot(w[9] & 0xffffffff)]
        return o

    def segments(self, world, segs):
        points = len(segs) // 3
        self.cpu._uc.mem_write(self.probes, struct.pack(f'<{points * 3}f', *segs))
        self.cpu._uc.mem_write(self.out, bytes(REC * 24))
        self.call(SEGMENTS, args=[world, self.probes, points, self.out])
        o = []
        for i in range(min(points // 2, 12)):
            rec = self.read(self.out + REC * i, REC)
            o.append(rec[0])
            for k in range(rec[0]):
                o += self.hit(rec[4 + SLOT * k:4 + SLOT * (k + 1)])
        return o


def u2f(w):
    return struct.unpack('<f', struct.pack('<i', w))[0]


def near(a, b):
    """Within 2e-4 of the exe's; a non-finite value only as itself (a scaled allowance would let it through)."""
    if a == b:
        return True
    return math.isfinite(a) and math.isfinite(b) and abs(a - b) <= 2e-4 * max(1.0, abs(a))


def compare(want, got, kind, stats):
    """Counts, slots and flags exactly; a hit's floats within the x87's last bits (CLAUDE.md: play-accurate)."""
    if len(want) != len(got):
        return f'{len(want)}/{len(got)} words'
    head, size, nfloats = {'ground': (1, 11, 6), 'point': (2, 11, 6), 'segs': (0, 11, 6)}.get(kind, (3, 10, 7))
    i, per_record = 0, kind in ('ground', 'segs')
    while i < len(want):
        if i < head:
            if want[i] != got[i]:
                return f'word {i}: {want[i]} / {got[i]}'
            i += 1
            continue
        n = want[i] if per_record else (want[1] if kind == 'point' else want[2] if kind == 'nearest' else want[1])
        if per_record:
            if want[i] != got[i]:
                return f'count at {i}: {want[i]} / {got[i]}'
            i += 1
        for _ in range(n):
            for k in range(size):
                x, y = want[i + k], got[i + k]
                if k < nfloats:
                    if x != y:
                        a, b = u2f(x), u2f(y)
                        if not near(a, b):
                            return f'float {k} at {i}: {a} / {b}'
                        stats['near'] += 1
                    stats['floats'] += 1
                elif x != y:
                    return f'word {k} at {i}: {x} / {y}'
                if k == size - 1 and x:
                    stats['clutter hits'] = stats.get('clutter hits', 0) + 1
            i += size
            stats[kind] += 1
        if not per_record:
            break
    return None


def compare_sphere(want, got, stats):
    if want[:3] != got[:3] or len(want) != len(got):
        return f'{want[:3]} / {got[:3]}, {len(want)}/{len(got)} words'
    for i in range(3, len(want), 5):
        for k in range(4):
            x, y = u2f(want[i + k]), u2f(got[i + k])
            if not near(x, y):
                return f'float {k} at {i}: {x} / {y}'
        if want[i + 4] != got[i + 4]:
            return f'node at {i}: {want[i + 4]} / {got[i + 4]}'
        stats['sphere'] = stats.get('sphere', 0) + 1
    return None


def main():
    queries = int(sys.argv[1]) if len(sys.argv) > 1 else 300
    p = test_pm_zar.load_pm_probe()
    p.probe_pm_draw_load.argtypes = [ctypes.c_char_p]
    FP = ctypes.POINTER(ctypes.c_float)
    IP = ctypes.POINTER(ctypes.c_int32)
    p.probe_pm_di_ground.argtypes = [ctypes.c_int, FP, ctypes.c_int, ctypes.c_float, IP, ctypes.c_int]
    p.probe_pm_di_point.argtypes = [ctypes.c_int, ctypes.c_float, ctypes.c_float, ctypes.c_float, IP, ctypes.c_int]
    p.probe_pm_di_segments.argtypes = [ctypes.c_int, FP, ctypes.c_int, IP, ctypes.c_int]
    p.probe_pm_di_clutter.argtypes = [ctypes.c_int] * 4 + [FP, ctypes.POINTER(ctypes.c_uint32), ctypes.c_int, FP, IP]
    p.probe_pm_di_centre.argtypes = [ctypes.c_int, FP]
    p.probe_pm_di_sphere.argtypes = [ctypes.c_int, FP, ctypes.c_float, ctypes.c_char_p, ctypes.c_int, ctypes.c_int, IP, ctypes.c_int]
    p.probe_pm_di_line.argtypes = [ctypes.c_int, FP, FP, ctypes.c_int, IP, ctypes.c_int]
    buf = (ctypes.c_int32 * (1 << 20))()
    totals = {'ground': 0, 'point': 0, 'segs': 0, 'list': 0, 'nearest': 0, 'floats': 0, 'near': 0}
    for name in sys.argv[2:] or ('c1', 'c2', 'c3', 'c4'):
        path = g.ZBD / name / 'gamez.zbd'
        emu = Emulated(path)
        if p.probe_pm_draw_load(str(path).encode()) != 0:
            raise SystemExit('probe load failed')
        slot, world = emu.world_node()
        rng = random.Random(name)
        # Around the map's middle and the ground's height, where the world has polygons.
        for q in range(queries):
            cx, cz = rng.uniform(500, 4500), rng.uniform(-4500, -500)
            y = rng.uniform(-10, 120)
            kind = rng.choice(('ground', 'ground', 'point', 'segs', 'segs', 'list', 'nearest'))
            if kind == 'ground':
                pts = [f32(v) for _ in range(rng.choice((1, 7, 24))) for v in (cx + rng.uniform(-8, 8), y, cz + rng.uniform(-8, 8))]
                top = f32(y + rng.uniform(0, 50))
                want = emu.ground(world, pts, top)
                arr = (ctypes.c_float * len(pts))(*pts)
                n = p.probe_pm_di_ground(slot, arr, len(pts) // 3, top, buf, len(buf))
            elif kind == 'point':
                pts = [f32(cx), f32(y), f32(cz)]
                want = emu.point(world, *pts)
                n = p.probe_pm_di_point(slot, *pts, buf, len(buf))
            elif kind in ('list', 'nearest'):
                a = [f32(cx), f32(y + rng.uniform(0, 40)), f32(cz)]
                d = rng.uniform(5, 500)
                b = [f32(a[0] + rng.uniform(-1, 1) * d), f32(a[1] + rng.uniform(-0.5, 0.2) * d), f32(a[2] + rng.uniform(-1, 1) * d)]
                want = emu.line(world, a, b, kind == 'nearest')
                n = p.probe_pm_di_line(slot, (ctypes.c_float * 3)(*a), (ctypes.c_float * 3)(*b), kind == 'nearest', buf, len(buf))
            else:
                segs = []
                for _ in range(rng.randrange(1, 12)):
                    a = (cx + rng.uniform(-30, 30), y + rng.uniform(0, 60), cz + rng.uniform(-30, 30))
                    segs += [*a, a[0] + rng.uniform(-30, 30), a[1] - rng.uniform(1, 120), a[2] + rng.uniform(-30, 30)]
                segs = [f32(v) for v in segs]
                want = emu.segments(world, segs)
                arr = (ctypes.c_float * len(segs))(*segs)
                n = p.probe_pm_di_segments(slot, arr, len(segs) // 3, buf, len(buf))
            got = list(buf[:n])
            bad = compare(want, got, kind, totals)
            if bad:
                raise SystemExit(f'{name} query {q} {kind}: {bad}\n exe {want[:30]}\n port {got[:30]}')
        # The sphere query records a node with flag 0x40: some of the world's get it, on both sides.
        for i in range(2500):
            n = emu.node_at(i)
            f = emu.i32(n + 0x24) & 0xffffffff
            if (f & 0x104) == 0x104 and emu.i32(n + 0x34) in (5, 6) and rng.random() < 0.5:
                emu.w32(n + 0x24, f | 0x40)
                p.probe_pm_di_flag(i, 0x40)
        for q in range(queries // 2):
            centre = [f32(rng.uniform(500, 4500)), f32(rng.uniform(0, 60)), f32(rng.uniform(-4500, -500))]
            radius = f32(rng.choice((40.0, 150.0, 400.0)))
            prefix = rng.choice((None, None, 'g', 'nosuch'))
            ranged, sighted = rng.randrange(2), rng.random() < 0.3
            want = emu.sphere(world, centre, radius, prefix, ranged, int(sighted))
            n = p.probe_pm_di_sphere(slot, (ctypes.c_float * 3)(*centre), radius, prefix.encode() if prefix else None, ranged, int(sighted), buf, len(buf))
            bad = compare_sphere(want, list(buf[:n]), totals)
            if bad:
                raise SystemExit(f'{name} sphere {q}: {bad}\n exe {want[:20]}\n port {list(buf[:n])[:20]}')
        # A cell's clutter, crafted on both sides from a world's model node: as an array, then through a tree.
        data = emu.i32(world + 0x38) & 0xffffffff
        ox, oz = struct.unpack('<2f', emu.read(data + 0x34, 8))
        ix, iz = struct.unpack('<2f', emu.read(data + 0x84, 8))
        w, d = emu.i32(data + 0x98), emu.i32(data + 0x9c)
        # The model with the most polygons of those the queries test (active, 8, 0x10, a box).
        def polygons(i):
            n = emu.node_at(i)
            if emu.i32(n + 0x34) != 5 or not emu.i32(n + 0x3c) or (emu.i32(n + 0x24) & 0x11c) != 0x11c or not emu.i32(n + 0x38):
                return -1
            return emu.i32((emu.i32(n + 0x3c) & 0xffffffff) + 0x10)
        prop = max(range(2500), key=polygons)
        # A parent whose tested child has a model, for the 0x200000 owner walk: the child's hits name the parent's record.
        def child_of(i):
            n = emu.node_at(i)
            if polygons(i) < 0:
                return -1
            kids = emu.i32(n + 0x5c) & 0xffffffff
            for k in range(emu.read(n + 0x56, 2)[0]):
                c = emu.i32(kids + 4 * k) & 0xffffffff
                if emu.i32(c + 0x3c) and emu.i32(c + 0x34) == 5 and (emu.i32(c + 0x24) & 0x11c) == 0x11c:
                    return emu.node_slot(c)
            return -1
        walk = next((i for i in range(2500) if child_of(i) >= 0), None)
        child = -1
        if walk is not None:
            child = child_of(walk)
            cn = emu.node_at(child)
            emu.w32(cn + 0x24, (emu.i32(cn + 0x24) & 0xffffffff) | 0x200000)
            p.probe_pm_di_flag(child, 0x200000)
            totals['walk props'] = totals.get('walk props', 0) + 1
        for boxed, the_prop in ((False, prop), (True, prop)) + (((False, walk),) if walk is not None else ()):
            prop = the_prop
            node = emu.node_at(prop)
            # With the walk's parent the record sits at the child's vertex, so queries reach the child.
            aim = node if prop != walk else emu.node_at(child)
            model = emu.i32(aim + 0x3c) & 0xffffffff
            v0 = struct.unpack('<3f', emu.read(emu.i32(model + 0x38) & 0xffffffff, 12))
            cx, cz = rng.randrange(w // 3, 2 * w // 3), rng.randrange(d // 3, 2 * d // 3)
            sx, sz = (cx + 0.5) / ix + ox, (cz + 0.5) / iz + oz
            recs, flags, leaf_of = [], [], []
            for k in range(12):
                a = rng.uniform(0, 6.28)
                c_, s_ = math.cos(a), math.sin(a)
                x, z = sx + rng.uniform(-30, 30), sz + rng.uniform(-30, 30)
                y = rng.uniform(0, 30)
                # The record's matrix takes the model's first vertex to (x, y, z).
                tx, tz = c_ * v0[0] + s_ * v0[2], -s_ * v0[0] + c_ * v0[2]
                recs.append([f32(v) for v in (c_, 0, -s_, 0, 1, 0, s_, 0, c_, x - tx, y - v0[1], z - tz, rng.choice((0xff, 3)))])
                flags.append(rng.choice((0, 0, 2, 1)))
                leaf_of.append((x > sx) + 2 * (z > sz))
            boxes = None
            if boxed:
                s9 = 500.0
                boxes = [(sx - s9, -500, sz - s9, sx + s9, 500, sz + s9), (sx - s9, -500, sz - s9, sx, 500, sz), (sx, -500, sz - s9, sx + s9, 500, sz),
                         (sx - s9, -500, sz, sx, 500, sz + s9), (sx, -500, sz, sx + s9, 500, sz + s9)]
                boxes = [tuple(f32(v) for v in b) for b in boxes]
            emu.clutter(world, cx, cz, node, recs, flags, boxes, leaf_of)
            flat = (ctypes.c_float * (13 * len(recs)))(*[v for r in recs for v in r])
            fl = (ctypes.c_uint32 * len(flags))(*flags)
            bx = (ctypes.c_float * 30)(*[v for b in boxes for v in b]) if boxes else None
            lo = (ctypes.c_int32 * len(leaf_of))(*leaf_of)
            p.probe_pm_di_clutter(slot, cx, cz, prop, flat, fl, len(recs), bx, lo)
            hits = {'owner': 0}
            for q in range(queries // 2):
                px, pz = sx + rng.uniform(-35, 35), sz + rng.uniform(-35, 35)
                py = rng.uniform(-5, 60)
                kind = rng.choice(('ground', 'point', 'segs', 'list', 'nearest'))
                if kind == 'ground':
                    pts = [f32(v) for _ in range(rng.choice((1, 7))) for v in (px + rng.uniform(-4, 4), py, pz + rng.uniform(-4, 4))]
                    top = f32(py + rng.uniform(0, 40))
                    want = emu.ground(world, pts, top)
                    n = p.probe_pm_di_ground(slot, (ctypes.c_float * len(pts))(*pts), len(pts) // 3, top, buf, len(buf))
                elif kind == 'point':
                    pts = [f32(px), f32(py), f32(pz)]
                    want = emu.point(world, *pts)
                    n = p.probe_pm_di_point(slot, *pts, buf, len(buf))
                elif kind == 'segs':
                    segs = []
                    for _ in range(rng.randrange(1, 12)):
                        v = (px + rng.uniform(-10, 10), py + rng.uniform(0, 30), pz + rng.uniform(-10, 10))
                        segs += [*v, v[0] + rng.uniform(-10, 10), v[1] - rng.uniform(1, 60), v[2] + rng.uniform(-10, 10)]
                    segs = [f32(v) for v in segs]
                    want = emu.segments(world, segs)
                    n = p.probe_pm_di_segments(slot, (ctypes.c_float * len(segs))(*segs), len(segs) // 3, buf, len(buf))
                else:
                    a = [f32(px), f32(py + rng.uniform(0, 30)), f32(pz)]
                    b = [f32(sx + rng.uniform(-30, 30)), f32(rng.uniform(-5, 30)), f32(sz + rng.uniform(-30, 30))]
                    want = emu.line(world, a, b, kind == 'nearest')
                    n = p.probe_pm_di_line(slot, (ctypes.c_float * 3)(*a), (ctypes.c_float * 3)(*b), kind == 'nearest', buf, len(buf))
                got = list(buf[:n])
                bad = compare(want, got, kind, totals)
                if prop == walk and kind in ('ground', 'point', 'segs'):
                    totals['walk hits'] = totals.get('walk hits', 0) + sum(
                        1 for i in range(10, len(want)) if want[i] == 1 and want[i - 1] == child)
                # The record left its place on the node: the centre and radius (0x50d930), PM's +0x60.
                centre = struct.unpack('<4f', emu.read(node + 0x60, 16))
                mine = (ctypes.c_float * 4)()
                p.probe_pm_di_centre(prop, mine)
                if not bad and any(not near(x, y) for x, y in zip(centre, mine)):
                    bad = f'the node\'s centre {centre} / {list(mine)}'
                if bad:
                    raise SystemExit(f'{name} clutter ({"tree" if boxed else "array"}) query {q} {kind}: {bad}\n exe {want[:30]}\n port {got[:30]}')
        print(f'{name}: {queries} queries match; hits so far {totals}', flush=True)
    pm_floors.floors(ground=(totals['ground'], 3000), point=(totals['point'], 500), segs=(totals['segs'], 1400), list=(totals['list'], 240), nearest=(totals['nearest'], 200), sphere=(totals['sphere'], 1300), clutter=(totals['clutter hits'], 2200))
    print(f'ok: {totals}')


if __name__ == '__main__':
    main()
