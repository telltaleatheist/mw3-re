"""The shot queries (zclass\\cls_di.c, docs/movement-collision.md, The shot
queries) on the six mission worlds, against the original x86: the segment
list 0x4eed90 with its cell walk 0x4eefe0, node walk 0x4ef570 and class
helpers (0x4ef980, 0x4efaa0, 0x4efba0), the box test 0x4f1470 (0x4ec6b0,
0x4ebdb0, the box faces 0x54d2f0), the model's segment test 0x54cad0 with
its polygon tests 0x54d760 and 0x54dd30; the nearest hit 0x4eece0; the
sphere query 0x4f0ac0 with 0x4f0f90 and 0x4f0f00.

    cmake --build build && .venv/bin/python tests/differential/test_shotq.py [queries per world]

Each world is read by the original's reader into emulator memory and by the
port's loaders (test_scenewalk); the camera and the sun are added as world
children on both sides (classes 1 and 9). Shots are drawn over each world
(along the ground, from above, long and short, off the grid with and
without the world's wrapping), with the first-hit mode, excluded flags, the
static-only mode against a frame and the models' `since` stamp drawn at
random, nodes' +0xc4 stamps set on both sides; spheres at the same spots
with a name prefix, the range test and the line of sight. Half way through
each world a quarter of the polygons become strips, some nodes get
INTERSECT_BBOX and some materials flag 2 (the textured test). The results,
the records (all 32 slots, pointers as indices), the options left, the
captured texture coordinates, and every few queries each model's cached
polygon and each node's flags must match bit for bit at 24-bit precision.
"""
import ctypes
import math
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_di import REC, World, f32, port_state, probe  # noqa: E402
from test_gamez import WORLDS, ZBD  # noqa: E402
from test_place import Place, port_point  # noqa: E402
from test_scenewalk import f2u  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402

probe.sq_set_node_word.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_uint32]
probe.sq_set_material_flags.argtypes = [ctypes.c_int, ctypes.c_uint8]
probe.sq_query.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_float, ctypes.c_char_p,
                           ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_uint32, ctypes.c_int, ctypes.c_int,
                           ctypes.c_uint32, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]

LIST, NEAREST, SPHERE = 0x4eed90, 0x4eece0, 0x4f0ac0
FIRST_HIT, STATIC, SINCE, EXCLUDE, FRAME, UV = 0x60b968, 0x60b96c, 0x60b970, 0x60b974, 0x72f338, 0x74a2c0
COVER = {'animated': 0x4ef980, 'camera': 0x4efaa0, 'light': 0x4efba0, 'box test': 0x4f1470,
         'box faces': 0x54d2f0, 'model test': 0x4ef722, 'textured polygon': 0x54dd30, 'wrapped cell': 0x4ef15f,
         'sphere leaf': 0x4f0d01, 'sphere range': 0x4f0d75, 'sphere sight': 0x4f0e1a, 'sphere child leaf': 0x4f1182}
COVERED = dict.fromkeys(COVER, 0)
WORDS = 2 + 32 * 10


class Shots(Place):
    def __init__(self, name):
        super().__init__(name)
        cpu = self.cpu
        self.rec = self.alloc(REC)
        cpu.write(self.rec, bytes(REC))
        self.pts = self.alloc(32)
        self.prefix_buf = self.alloc(64)
        cpu.stub_import(0x5bb654, self.strncmp)  # msvcrt strncmp (cdecl)
        for key, a in COVER.items():
            cpu.uc.hook_add(UC_HOOK_CODE, lambda uc, ad, n, k: COVERED.__setitem__(k, COVERED[k] + 1), user_data=key,
                            begin=a, end=a)

    def strncmp(self, c):
        a, b, n = c.arg(0), c.arg(1), c.arg(2)
        x, y = self.cpu.read(a, n), self.cpu.read(b, n)
        for i in range(n):
            if x[i] != y[i]:
                return (x[i] - y[i]) & 0xffffffff
            if x[i] == 0:
                return 0
        return 0

    def query(self, kind, a, b, radius, prefix, ranged, sighted, opts):
        cpu = self.cpu
        first, exclude, static, since, frame = opts
        cpu.u32(FIRST_HIT, first)
        cpu.u32(EXCLUDE, exclude)
        cpu.u32(STATIC, static)
        cpu.u32(SINCE, since)
        cpu.u32(FRAME, frame)
        self.reports = []
        if kind == 0:
            cpu.call(LIST, ecx=self.world, edx=self.rec, stack_args=tuple(f2u(v) for v in a + b),
                     count=500_000_000)
        elif kind == 1:
            cpu.write(self.pts, struct.pack('<6f', *a, *b))
            cpu.call(NEAREST, ecx=self.world, edx=self.pts, stack_args=(self.pts + 12, self.rec), count=500_000_000)
        else:
            cpu.write(self.pts, struct.pack('<3f', *a))
            p = 0
            if prefix is not None:
                cpu.write(self.prefix_buf, prefix + b'\0')
                p = self.prefix_buf
            cpu.call(SPHERE, ecx=self.world, edx=self.pts, stack_args=(p, f2u(radius), ranged, sighted, self.rec),
                     count=500_000_000)
        r = cpu.uc.reg_read(19)
        raw = cpu.read(self.rec, REC)
        out = [raw[0], raw[1]]
        for j in range(32):
            h = struct.unpack_from('<10I', raw, 4 + 0x28 * j)
            out.extend(h[:8])
            out.append(self.material_index(h[8]) & 0xffffffff)
            out.append(self.node_index(h[9]) & 0xffffffff if h[9] else 0xffffffff)
        after = [self.u32(FIRST_HIT), self.u32(EXCLUDE)]
        uv = list(struct.unpack('<2I', cpu.read(UV, 8)))
        return r, out, after, uv, [rep[1] for rep in self.reports]

    def stamp_nodes(self, rng):
        """Gives a share of the nodes a +0xc4 stamp (the frame they moved),
        on both sides."""
        bs = self.u32(0x5ea830)
        n = 0
        for b, base in enumerate(self.blocks):
            for i in range(bs):
                a = base + 0xd4 * i
                if self.u32(a + 0x38) and rng.random() < 0.3:
                    v = rng.randrange(0, 6)
                    self.cpu.u32(a + 0xc4, v)
                    probe.sq_set_node_word(b * bs + i, 0xc4, v)
                    n += 1
                f = self.u32(a + 0x24)
                if self.u32(a + 0x38) and (f & 0x10) and rng.random() < 0.05:
                    f |= 0x40000  # the flag the sphere's sight line excludes
                    self.cpu.u32(a + 0x24, f)
                    probe.sq_set_node_word(b * bs + i, 0x24, f)
                if self.u32(a + 0x38) and (f & 0x100) and rng.random() < 0.15:
                    # The sphere query's targets (the game sets 0x40 at run time),
                    # some also taking its line of sight (0x400000).
                    f |= 0x40 | (0x400000 if rng.random() < 0.5 else 0)
                    self.cpu.u32(a + 0x24, f)
                    probe.sq_set_node_word(b * bs + i, 0x24, f)
        return n

    def flag_materials(self, rng):
        """Material flag 2 (the textured test) on a share, on both sides."""
        n = 0
        for b, base in enumerate(self.mat_blocks):
            if not base:
                continue
            for i in range(1000):
                a = base + 0x2c * i
                f = self.cpu.read(a + 1, 1)[0]
                if (f & 1) and rng.random() < 0.4:
                    f |= 2
                    self.cpu.write(a + 1, bytes([f]))
                    probe.sq_set_material_flags(b * 1000 + i, f)
                    n += 1
        return n

    def names(self, rng, k=6):
        """Some prefixes of node names with flag 0x40."""
        bs = self.u32(0x5ea830)
        out = []
        for base in self.blocks:
            for i in range(bs):
                a = base + 0xd4 * i
                if self.u32(a + 0x38) and (self.u32(a + 0x24) & 0x40):
                    name = self.cpu.read(a, 0x20).split(b'\0')[0]
                    if name:
                        out.append(name[:rng.randrange(1, len(name) + 1)])
        rng.shuffle(out)
        return out[:k]


def port_query(kind, a, b, radius, prefix, ranged, sighted, opts):
    first, exclude, static, since, frame = opts
    pa = (ctypes.c_float * 3)(*a)
    pb = (ctypes.c_float * 3)(*(b or (0.0, 0.0, 0.0)))
    out = (ctypes.c_int32 * WORDS)()
    after = (ctypes.c_int32 * 2)()
    uv = (ctypes.c_float * 2)()
    r = probe.sq_query(kind, pa, pb, radius, prefix, ranged, sighted, first, exclude, static,
                       ctypes.c_int32(since).value, frame, out, after, uv)
    return (r & 0xffffffff, [v & 0xffffffff for v in out], [v & 0xffffffff for v in after],
            list(struct.unpack('<2I', bytes(uv))))


def shot(rng, emu, ground_at):
    x0, z0, w, d = emu.area
    kind = rng.choice(['ground', 'ground', 'down', 'long', 'short', 'off', 'up', 'grid', 'grid'])
    if kind == 'off':
        x, z = x0 + rng.choice([-0.3, 1.3, rng.uniform(-1, 2)]) * w, z0 + rng.choice([-0.3, 1.3]) * d
    else:
        x, z = x0 + rng.uniform(0.02, 0.98) * w, z0 + rng.uniform(0.02, 0.98) * d
    if rng.random() < 0.15:
        x, z = round(x / 8) * 8, round(z / 8) * 8
    if kind == 'grid':  # on round coordinates: polygons' edges and corners, the cells' corners
        step = rng.choice([1.0, 4.0, 8.0, 16.0, 64.0])
        x, z = round(x / step) * step, round(z / step) * step
    g = ground_at(x, z)
    y = g + rng.uniform(0.5, 25)
    if kind == 'grid':
        if rng.random() < 0.5:  # straight down
            a, b = (x, g + rng.uniform(1, 50), z), (x, g - rng.uniform(1, 20), z)
        else:  # along a grid line or a diagonal through cell corners
            d = rng.choice([(1, 0), (0, 1), (1, 1), (-1, 1), (1, -1)])
            length = rng.choice([64.0, 128.0, 256.0])
            a, b = (x, y, z), (x + d[0] * length, y - rng.uniform(0, 10), z + d[1] * length)
    elif kind == 'down':
        a, b = (x, y + rng.uniform(0, 100), z), (x + rng.uniform(-3, 3), g - rng.uniform(0.1, 20), z + rng.uniform(-3, 3))
    elif kind == 'up':
        a, b = (x, g - 5, z), (x, g + rng.uniform(5, 200), z)
    else:
        length = {'ground': rng.uniform(20, 400), 'long': rng.uniform(400, 1500), 'short': rng.uniform(0.5, 20),
                  'off': rng.uniform(20, 600)}[kind]
        h = rng.uniform(0, 2 * math.pi)
        dy = rng.uniform(-0.3, 0.1)
        a = (x, y, z)
        b = (x + math.cos(h) * length, y + dy * length, z + math.sin(h) * length)
    return [f32(v) for v in a], [f32(v) for v in b], kind


def main():
    queries = int(sys.argv[1]) if len(sys.argv) > 1 else 500
    rng = random.Random(0x4eed90)
    total = hits = spheres = sphere_hits = 0
    kinds = set()
    for name in WORLDS:
        emu = Shots(name)
        if probe.sw_load(str(ZBD / name / 'gamez.zbd').encode()) != 0:
            raise SystemExit(f'{name}: the port could not read the world')
        probe.di_reset()
        probe.sq_reset()
        emu.add_world_children([1, 9])
        stamped = emu.stamp_nodes(rng)
        prefixes = emu.names(rng)

        def ground_at(x, z):  # on both sides: the point query moves models' cached polygons
            r, out, _ = emu.point(f32(x), 2000.0, f32(z), 0)[:3]
            port_point(f32(x), 2000.0, f32(z), 0)
            return struct.unpack('<f', struct.pack('<I', out[5]))[0] if out[0] else 0.0
        strips = boxed = flagged = 0
        for q in range(queries):
            if q == queries // 2:
                strips = emu.make_strips(rng)
                boxed = emu.set_flags(rng)
                flagged = emu.flag_materials(rng)
            wrap = int(rng.random() < 0.3)
            emu.cpu.u32(emu.wd + 0x50, wrap)
            probe.di_wrap(wrap)
            a, b, kind = shot(rng, emu, ground_at)
            opts = (int(rng.random() < 0.2), rng.choice([0, 0, 0, 0x40000, 0x80000, 0x20000]),
                    int(rng.random() < 0.2), rng.choice([-1, -1, 0, 1, 3]), rng.randrange(0, 6))
            qk = rng.choice([0, 0, 1])
            where = f'{name} query {q} ({kind}, {"list" if qk == 0 else "nearest"}, wrap {wrap}, opts {opts})'
            want = emu.query(qk, a, b, 0.0, None, 0, 0, tuple(v & 0xffffffff for v in opts))
            got = port_query(qk, a, b, 0.0, None, 0, 0, opts)
            check(where, want, got)
            total += 1
            kinds.add(kind)
            hits += want[1][1] if qk == 1 else want[1][0]
            if os.environ.get('SQ_EVERY'):
                models, flags = emu.state()
                pm, _ = port_state(len(models), len(flags))
                if models != pm:
                    i = next(i for i, (x, y) in enumerate(zip(models, pm)) if x != y)
                    raise SystemExit(f'{where}: model {i} cached polygon {models[i]} vs {pm[i]}; shot {a} {b}')
            # A sphere at the shot's start.
            prefix = rng.choice([None, None, b''] + prefixes)
            radius = f32(rng.choice([5.0, 30.0, 120.0, rng.uniform(1, 300)]))
            ranged, sighted = rng.choice([0, 1]), rng.choice([0, 0, 1])
            where = f'{name} sphere {q} (radius {radius}, prefix {prefix}, ranged {ranged}, sighted {sighted})'
            want = emu.query(2, a, None, radius, prefix, ranged, sighted, (0, 0, 0, 0xffffffff, 0))
            got = port_query(2, a, None, radius, prefix, ranged, sighted, (0, 0, 0, -1, 0))
            check(where, want, got)
            spheres += 1
            sphere_hits += want[1][0]
            if q % 50 == 49 or q == queries - 1 or os.environ.get("SQ_EVERY"):
                models, flags = emu.state()
                pm, pf = port_state(len(models), len(flags))
                if models != pm:
                    i = next(i for i, (x, y) in enumerate(zip(models, pm)) if x != y)
                    raise SystemExit(f'{where}: model {i} cached polygon {models[i]} vs {pm[i]}')
                if flags != pf:
                    i = next(i for i, (x, y) in enumerate(zip(flags, pf)) if x != y)
                    raise SystemExit(f'{where}: node {i} flags {flags[i]:#x} vs {pf[i]:#x}')
        print(f'{name}: {queries} shots and spheres identical ({stamped} nodes stamped); the second half with '
              f'{strips} strips, {boxed} boxed nodes, {flagged} textured materials', flush=True)
    print('reached:', ', '.join(f'{k} {v}' for k, v in COVERED.items()))
    missing = [k for k, v in COVERED.items() if not v and k != 'animated']
    if missing or not hits or not sphere_hits or len(kinds) < 7:
        raise SystemExit(f'coverage: {missing} not reached, {hits} hits, {sphere_hits} sphere hits, {sorted(kinds)}')
    print(f'ok: {total} shot queries ({hits} crossings) and {spheres} sphere queries ({sphere_hits} found) on the '
          f'{len(WORLDS)} worlds identical (no animated node on them)')


def check(where, want, got):
    r, out, after, uv, reports = want
    pr, pout, pafter, puv = got
    if r != pr:
        raise SystemExit(f'{where}: result {r} vs {pr}')
    if out != pout:
        i = next(i for i, (x, y) in enumerate(zip(out, pout)) if x != y)
        slot, word = divmod(i - 2, 10) if i >= 2 else (-1, i)
        if os.environ.get('SQ_DEBUG'):
            for s in range(min(max(out[0], out[1], pout[0], pout[1]) + 1, 32)):
                w1 = out[2 + 10 * s:12 + 10 * s]
                w2 = pout[2 + 10 * s:12 + 10 * s]
                f = lambda w: ' '.join(f'{struct.unpack("<f", struct.pack("<I", v))[0]:.7g}' for v in w[:8])  # noqa
                print(f'  slot {s}: {f(w1)} mat {w1[8]} node {w1[9]}\n        : {f(w2)} mat {w2[8]} node {w2[9]}')
        raise SystemExit(f'{where}: records differ at slot {slot} word {word}: {out[i]:#x} vs {pout[i]:#x} '
                         f'(count {out[0]}/{pout[0]})')
    if after != pafter:
        raise SystemExit(f'{where}: options after {after} vs {pafter}')
    if uv != puv:
        raise SystemExit(f'{where}: captured coordinates {uv} vs {puv}')
    if reports and not (out[0] >= 32 and set(reports) <= {0x10f5, 0xa1c}):  # "array is full", with a full record
        raise SystemExit(f'{where}: the original reported {reports}')


if __name__ == '__main__':
    main()
    sys.stdout.flush()
    os._exit(0)
