"""The engine's world queries (zclass\\cls_di.c) on the six mission worlds,
against the original x86 (docs/movement-ground.md, The terrain query):
the ground query 0x4ee2f0 with its node walk (0x4ee790, 0x4eeb50,
0x4eec10), box test (0x4f1350 with 0x4ec150, 0x522d60) and the model's
polygon test (0x54c5f0, 0x54c330, 0x524020); the segment query 0x4efcd0
with its cell walk (0x4efef0), node walk (0x4f0480, 0x4f08c0, 0x4f09b0), box
test (0x4f1640 with the box faces 0x54fa10) and the model's crossings
(0x54f450, 0x54e2b0, the textured 0x54ea30 with 0x5240d7).

    cmake --build build && .venv/bin/python tests/differential/test_di.py [queries per world]

Each world is read by the original's reader into emulator memory and by the
port's loaders (test_scenewalk). Probe sets as the mechs make them (seven
points around a spot, single points, spread sets, off-grid points with and
without the world's wrapping, the 24-probe limit and past it) are queried on
both sides, then segments around the same spot (a mech's eleven
collision probes moving at up to 30 m/s, and long shots across cells), with
the first-hit mode, excluded node flags and texture-coordinate capture
drawn at random. The records (all 32 slots of each, pointers as node and
material indices), the probes and segments as left, the captured
coordinates and the error reports must match bit for bit at 24-bit
precision, and every few queries each model's cached polygon and each
node's flags too. Half way through each world a quarter of the polygons
become strips (the shipped worlds have none), and some nodes get
INTERSECT_BBOX.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import FTOL, IAT_FTOL, ROOT  # noqa: E402
from test_gamez import BLOCKS, POOL, POOL_SIZE, ZBD, WORLDS  # noqa: E402
from test_scenewalk import Walk, f2u  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
probe.sw_load.argtypes = [ctypes.c_char_p]
probe.di_ground.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_float, ctypes.c_void_p]
probe.di_state.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int]
probe.di_wrap.argtypes = [ctypes.c_int]
probe.di_set_poly.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_uint32]
probe.di_segments.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_uint32, ctypes.c_int,
                              ctypes.c_void_p]
probe.di_set_flags.argtypes = [ctypes.c_int, ctypes.c_uint32]

GROUND, SEGMENTS, PENDING = 0x4ee2f0, 0x4efcd0, 0x60bea0
FIRST_HIT, EXCLUDE, UV_ON, UV = 0x60b968, 0x60b974, 0x74a2ac, 0x74a2c0
REC = 0x504


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


class World(Walk):
    def __init__(self, name):
        super().__init__()
        cpu = self.cpu
        cpu.import_code(IAT_FTOL, FTOL)
        self.load(ZBD / name / 'gamez.zbd')
        cpu.u32(0x74a28c, self.alloc(0x200000))
        cpu.u32(0x74a288, self.alloc(0x200000))
        cpu.u32(PENDING, 0)
        self.world = self.camera_world()
        self.wd = self.u32(self.world + 0x38)
        wd = self.wd
        self.area = (self.f32(wd + 0x34), self.f32(wd + 0x38), self.f32(wd + 0x64) * self.u32(wd + 0x88),
                     self.f32(wd + 0x68) * self.u32(wd + 0x8c))
        self.out = self.alloc(26 * REC)
        cpu.write(self.out, bytes(26 * REC))
        self.sout = self.alloc(13 * REC)
        cpu.write(self.sout, bytes(13 * REC))
        cpu.write(UV, bytes(8))
        self.probes = self.alloc(26 * 12)
        self.mat_blocks = [self.u32(self.u32(BLOCKS) + 4 * b) for b in range(4)]

    def material_index(self, p):
        if not p:
            return -1
        for b, base in enumerate(self.mat_blocks):
            if base and base <= p < base + 1000 * 0x2c:
                return b * 1000 + (p - base) // 0x2c
        return -2

    def ground(self, probes, top):
        cpu = self.cpu
        n = len(probes) // 3
        cpu.write(self.probes, struct.pack(f'<{len(probes)}f', *probes))
        self.reports = []
        cpu.call(GROUND, ecx=self.world, edx=self.probes, stack_args=(n, f2u(top), self.out), count=500_000_000)
        r = cpu.uc.reg_read(19)
        out = []
        for i in range(min(n, 24)):
            rec = cpu.read(self.out + REC * i, REC)
            out.append(rec[0])
            for j in range(32):
                h = struct.unpack_from('<10I', rec, 4 + 0x28 * j)
                out.extend(h[:8])
                out.append(self.material_index(h[8]) & 0xffffffff)
                out.append(self.node_index(h[9]) & 0xffffffff if h[9] else 0xffffffff)
        after = list(struct.unpack(f'<{len(probes)}I', cpu.read(self.probes, 4 * len(probes))))
        return r, out, after, [rep[1] for rep in self.reports]

    def segments(self, segs, first_hit, exclude, uv_on):
        cpu = self.cpu
        n = len(segs) // 3
        cpu.write(self.probes, struct.pack(f'<{len(segs)}f', *segs))
        cpu.u32(FIRST_HIT, first_hit)
        cpu.u32(EXCLUDE, exclude)
        cpu.u32(UV_ON, uv_on)
        self.reports = []
        cpu.call(SEGMENTS, ecx=self.world, edx=self.probes, stack_args=(n, self.sout), count=500_000_000)
        out = []
        for i in range(min(n, 24) // 2):
            rec = cpu.read(self.sout + REC * i, REC)
            out.append(rec[0])
            for j in range(32):
                h = struct.unpack_from('<10I', rec, 4 + 0x28 * j)
                out.extend(h[:8])
                out.append(self.material_index(h[8]) & 0xffffffff)
                out.append(self.node_index(h[9]) & 0xffffffff if h[9] else 0xffffffff)
        out.extend(struct.unpack('<2I', cpu.read(UV, 8)))
        out.append(self.u32(EXCLUDE))
        after = list(struct.unpack(f'<{len(segs)}I', cpu.read(self.probes, 4 * len(segs))))
        return out, after, [rep[1] for rep in self.reports]

    def set_flags(self, rng):
        """Gives some boxed nodes INTERSECT_BBOX (0x20) on both sides."""
        bs, n = self.u32(0x5ea830), 0
        for b, base in enumerate(self.blocks):
            for i in range(bs):
                a = base + 0xd4 * i + 0x24
                f = self.u32(a)
                if (f & 0x1000000) and (f & 0x100) and (f & 0x10) and rng.random() < 0.15:
                    f |= 0x20
                    self.cpu.u32(a, f)
                    probe.di_set_flags(b * bs + i, f)
                    n += 1
        return n

    def make_strips(self, rng):
        """Marks a share of the polygons as strips (flag 0x400) on both sides:
        the shipped worlds have none."""
        pool, size = self.u32(POOL), self.u32(POOL_SIZE)
        n = 0
        for m in range(size):
            slot = pool + 0x60 * m
            count, polys = self.u32(slot + 0x10), self.u32(slot + 0x34)
            if not polys:
                continue
            for p in range(count):
                f = self.u32(polys + 0x24 * p)
                if (f & 0xff) >= 2 and rng.random() < 0.25:
                    f |= 0x400
                    self.cpu.u32(polys + 0x24 * p, f)
                    probe.di_set_poly(m, p, f)
                    n += 1
        return n

    def state(self):
        pool, size = self.u32(POOL), self.u32(POOL_SIZE)
        models = [self.u32(pool + 0x60 * i + 0x58) for i in range(size)]
        flags = []
        bs = self.u32(0x5ea830)
        for base in self.blocks:
            flags.extend(struct.unpack(f'<{bs}I', b''.join(self.cpu.read(base + 0xd4 * i + 0x24, 4)
                                                          for i in range(bs))))
        return models, flags


def port_ground(probes, top):
    n = len(probes) // 3
    buf = (ctypes.c_float * len(probes))(*probes)
    out = (ctypes.c_int32 * (24 * (1 + 32 * 10)))()
    r = probe.di_ground(buf, n, top, out)
    k = min(n, 24) * (1 + 32 * 10)
    after = [struct.unpack('<I', struct.pack('<f', v))[0] for v in buf]
    return r, [v & 0xffffffff for v in out[:k]], after


def port_segments(segs, first_hit, exclude, uv_on):
    n = len(segs) // 3
    buf = (ctypes.c_float * len(segs))(*segs)
    k = (min(n, 24) // 2) * (1 + 32 * 10) + 3
    out = (ctypes.c_int32 * max(k, 3))()
    probe.di_segments(buf, n, first_hit, exclude, uv_on, out)
    after = [struct.unpack('<I', struct.pack('<f', v))[0] for v in buf]
    return [v & 0xffffffff for v in out[:k]], after


def segment_set(rng, cx, y, cz, x0=None):
    kind = rng.choice(['mech', 'mech', 'mech', 'shot', 'shots', 'odd', 'over', 'down', 'down', 'first out'])
    segs = []
    if kind == 'first out':  # segment 0 off the world, the rest on it (the bounds test reads segment 0's box)
        segs += [x0 - 30, y, cz, x0 - 27, y, cz]
        for _ in range(rng.randrange(1, 12)):
            x, z = cx + rng.uniform(-15, 15), cz + rng.uniform(-15, 15)
            segs += [x, y + rng.uniform(0.1, 8), z, x, y - rng.uniform(0.1, 8), z]
    elif kind == 'down':  # through the ground, as a falling or landing probe would
        for _ in range(rng.randrange(1, 13)):
            x, z = cx + rng.uniform(-15, 15), cz + rng.uniform(-15, 15)
            top, bottom = y + rng.uniform(0.1, 8), y - rng.uniform(0.1, 8)
            segs += [x + rng.uniform(-1, 1), top, z + rng.uniform(-1, 1), x, bottom, z]
    elif kind == 'mech':
        vx, vy, vz = rng.uniform(-30, 30), rng.choice([0.0, 0.0, rng.uniform(-20, 20)]), rng.uniform(-30, 30)
        dt = rng.choice([1 / 30, 1 / 15, 0.25])
        pts = [(cx + dx, y + h, cz + dz) for dx in (-2.5, 0.0, 2.5) for dz, h in ((-6.0, 4.0), (6.0, 4.0))]
        pts += [(cx + dx, y + 0.75, cz + dz) for dx in (-2.0, 2.0) for dz in (-5.0, 5.0)] + [(cx, y + 12.0, cz)]
        pts = rng.sample(pts, rng.randrange(1, 12))
        for (x, py, z) in pts:
            segs += [x - vx * dt, py - vy * dt, z - vz * dt, x, py, z]
    else:
        count = {'shot': 1, 'shots': rng.randrange(2, 13), 'odd': rng.randrange(1, 12), 'over': 13}[kind]
        for _ in range(count):
            a = (cx + rng.uniform(-60, 60), y + rng.uniform(-5, 40), cz + rng.uniform(-60, 60))
            d = rng.uniform(5, 400)
            dx, dy, dz = rng.uniform(-1, 1), rng.uniform(-0.3, 0.3), rng.uniform(-1, 1)
            segs += [a[0], a[1], a[2], a[0] + dx * d, a[1] + dy * d, a[2] + dz * d]
        if kind == 'odd':
            segs += [cx, y, cz]
    return [f32(v) for v in segs], kind


def port_state(nmodels, nflags):
    m, f = (ctypes.c_int32 * max(nmodels, 1))(), (ctypes.c_int32 * max(nflags, 1))()
    probe.di_state(m, f, max(nmodels, nflags))
    return [v & 0xffffffff for v in m[:nmodels]], [v & 0xffffffff for v in f[:nflags]]


def probe_set(rng, area):
    x0, z0, w, d = area
    kind = rng.choice(['mech', 'mech', 'mech', 'one', 'spread', 'off', 'many', 'over', 'grid', 'grid'])
    if kind == 'off':
        cx = x0 + rng.choice([-0.3, 1.3, rng.uniform(-1, 2)]) * w
        cz = z0 + rng.choice([-0.3, 1.3, rng.uniform(-1, 2)]) * d
    else:
        cx, cz = x0 + rng.uniform(0.02, 0.98) * w, z0 + rng.uniform(0.02, 0.98) * d
    y = rng.uniform(-20, 120)
    if kind in ('mech', 'off'):
        pts = [(cx + dx, y, cz + dz) for dx in (-2.5, 2.5) for dz in (-6.0, 0.0, 6.0)] + [(cx, y, cz)]
    elif kind == 'one':
        pts = [(cx, y, cz)]
    elif kind == 'grid':  # on round coordinates, where polygons' edges and corners lie
        step = rng.choice([1.0, 4.0, 8.0, 16.0])
        pts = [(round((cx + rng.uniform(-20, 20)) / step) * step + rng.choice([0.0, 0.0, 1e-4, -1e-4, 5e-4]), y,
                round((cz + rng.uniform(-20, 20)) / step) * step + rng.choice([0.0, 0.0, 1e-4, -5e-4]))
               for _ in range(rng.randrange(1, 8))]
    elif kind == 'spread':
        pts = [(x0 + rng.uniform(0, 1) * w, y, z0 + rng.uniform(0, 1) * d) for _ in range(rng.randrange(2, 9))]
    elif kind == 'many':
        pts = [(cx + rng.uniform(-30, 30), y, cz + rng.uniform(-30, 30)) for _ in range(24)]
    else:
        pts = [(cx + rng.uniform(-30, 30), y, cz + rng.uniform(-30, 30)) for _ in range(26)]
    return [f32(v) for p in pts for v in p], kind


def main():
    queries = int(sys.argv[1]) if len(sys.argv) > 1 else 1500
    rng = random.Random(4950)
    total = hits = wrapped = shits = 0
    kinds, skinds = set(), set()
    for name in WORLDS:
        emu = World(name)
        path = str(ZBD / name / 'gamez.zbd').encode()
        if probe.sw_load(path) != 0:
            raise SystemExit(f'{name}: the port could not read the world')
        probe.di_reset()
        strips = boxed = 0
        for q in range(queries):
            if q == queries // 2:
                strips = emu.make_strips(rng)
                boxed = emu.set_flags(rng)
            wrap = int(rng.random() < 0.5)
            emu.cpu.u32(emu.wd + 0x50, wrap)
            probe.di_wrap(wrap)
            probes, kind = probe_set(rng, emu.area)
            top = f32(rng.choice([2000.0, 2000.0, 300.0, 60.0, 20.0, 0.0, -30.0, probes[1] + 10.0]))
            r, want, after, reports = emu.ground(probes, top)
            pr, got, pafter = port_ground(probes, top)
            where = f'{name} query {q} ({kind}, {len(probes) // 3} probes, wrap {wrap})'
            if (r, after) != (pr, pafter):
                raise SystemExit(f'{where}: result or probes differ: {r} {pr}\n  {after}\n  {pafter}')
            if want != got:
                i = next(i for i, (a, b) in enumerate(zip(want, got)) if a != b)
                rec, slot = divmod(i, 1 + 320)
                raise SystemExit(f'{where}: records differ at probe {rec} word {slot}: {want[i]:#x} vs {got[i]:#x}')
            if len(probes) // 3 > 24 and reports != [0x50d]:
                raise SystemExit(f'{where}: the original reported {reports}')
            total += 1
            kinds.add(kind)
            hits += sum(want[k * 321] for k in range(min(len(probes) // 3, 24)))
            wrapped += wrap and kind == 'off'
            # Segments around the same spot, at the height the ground query found.
            hs = [struct.unpack('<f', struct.pack('<I', want[1 + 4]))[0]] if want and want[0] else []
            y = hs[0] if hs and abs(hs[0]) < 1e4 else probes[1]
            segs, skind = segment_set(rng, probes[0], y, probes[2], emu.area[0])
            first = int(rng.random() < 0.2)
            exclude = rng.choice([0, 0, 0, 0x20000, 0x1000000, 0x80000, 0x40])
            uv_on = int(rng.random() < 0.3)
            swant, safter, sreports = emu.segments(segs, first, exclude, uv_on)
            sgot, spafter = port_segments(segs, first, exclude, uv_on)
            where = f'{name} segments {q} ({skind}, {len(segs) // 6} segments, first {first}, exclude {exclude:#x})'
            if safter != spafter:
                raise SystemExit(f'{where}: segments as left differ\n  {safter}\n  {spafter}')
            if swant != sgot:
                i = next(i for i, (a, b) in enumerate(zip(swant, sgot)) if a != b)
                rec, slot = divmod(i, 1 + 320)
                if os.environ.get('DI_DEBUG'):
                    def show(r):
                        base = rec * 321
                        out = [f'count {r[base]}']
                        for j in range(r[base]):
                            w = r[base + 1 + 10 * j: base + 11 + 10 * j]
                            out.append(' '.join(f'{struct.unpack("<f", struct.pack("<I", v))[0]:.6g}' for v in w[:6]) +
                                       f' mat {w[8]} node {w[9]}')
                        return '\n    '.join(out)
                    print('segment', segs[6 * rec:6 * rec + 6])
                    print('  original', show(swant))
                    print('  port    ', show(sgot))
                raise SystemExit(f'{where}: records differ at segment {rec} word {slot}: {swant[i]:#x} vs {sgot[i]:#x}')
            if len(segs) // 3 > 24 and sreports != [0xc83]:
                raise SystemExit(f'{where}: the original reported {sreports}')
            nseg = min(len(segs) // 3, 24) // 2
            shits += sum(swant[k * 321] for k in range(nseg))
            skinds.add(skind)
            if q % 50 == 49 or q == queries - 1:
                models, flags = emu.state()
                pm, pf = port_state(len(models), len(flags))
                if models != pm:
                    i = next(i for i, (a, b) in enumerate(zip(models, pm)) if a != b)
                    raise SystemExit(f'{where}: model {i} cached polygon {models[i]} vs {pm[i]}')
                if flags != pf:
                    i = next(i for i, (a, b) in enumerate(zip(flags, pf)) if a != b)
                    raise SystemExit(f'{where}: node {i} flags {flags[i]:#x} vs {pf[i]:#x}')
        print(f'{name}: {queries} ground and segment queries identical, the second half with {strips} polygons '
              f'made strips and {boxed} nodes boxes')
    if len(kinds) < 7 or len(skinds) < 7 or not wrapped or not hits or not shits:
        raise SystemExit(f'coverage: {sorted(kinds)} {sorted(skinds)}, {wrapped} wrapped off-grid sets')
    print(f'ok: {total} ground and {total} segment queries on the {len(WORLDS)} worlds identical; {hits} ground hits, '
          f'{shits} crossings, {wrapped} wrapped off-grid sets')


if __name__ == '__main__':
    main()
