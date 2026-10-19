"""The engine's world queries (zclass\\cls_di.c) on the six mission worlds,
against the original x86 (docs/movement-ground.md, The terrain query):
the ground query 0x4ee2f0 with its node walk (0x4ee790, 0x4eeb50,
0x4eec10), box test (0x4f1350 with 0x4ec150, 0x522d60) and the model's
polygon test (0x54c5f0, 0x54c330, 0x524020).

    cmake --build build && .venv/bin/python tests/differential/test_di.py [queries per world]

Each world is read by the original's reader into emulator memory and by the
port's loaders (test_scenewalk). Probe sets as the mechs make them (seven
points around a spot, single points, spread sets, off-grid points with and
without the world's wrapping, the 24-probe limit and past it) are queried on
both sides; the records (all 32 slots of each, pointers as node and
material indices), the probes as left and the error reports must match bit
for bit at 24-bit precision, and every few queries each model's cached
polygon and each node's flags too.
"""
import ctypes
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

GROUND, PENDING = 0x4ee2f0, 0x60bea0
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


def port_state(nmodels, nflags):
    m, f = (ctypes.c_int32 * max(nmodels, 1))(), (ctypes.c_int32 * max(nflags, 1))()
    probe.di_state(m, f, max(nmodels, nflags))
    return [v & 0xffffffff for v in m[:nmodels]], [v & 0xffffffff for v in f[:nflags]]


def probe_set(rng, area):
    x0, z0, w, d = area
    kind = rng.choice(['mech', 'mech', 'mech', 'one', 'spread', 'off', 'many', 'over'])
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
    total = hits = wrapped = 0
    kinds = set()
    for name in WORLDS:
        emu = World(name)
        path = str(ZBD / name / 'gamez.zbd').encode()
        if probe.sw_load(path) != 0:
            raise SystemExit(f'{name}: the port could not read the world')
        probe.di_reset()
        strips = 0
        for q in range(queries):
            if q == queries // 2:
                strips = emu.make_strips(rng)
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
            if q % 50 == 49 or q == queries - 1:
                models, flags = emu.state()
                pm, pf = port_state(len(models), len(flags))
                if models != pm:
                    i = next(i for i, (a, b) in enumerate(zip(models, pm)) if a != b)
                    raise SystemExit(f'{where}: model {i} cached polygon {models[i]} vs {pm[i]}')
                if flags != pf:
                    i = next(i for i, (a, b) in enumerate(zip(flags, pf)) if a != b)
                    raise SystemExit(f'{where}: node {i} flags {flags[i]:#x} vs {pf[i]:#x}')
        print(f'{name}: {queries} ground queries identical, the second half with {strips} polygons made strips')
    if len(kinds) < 6 or not wrapped or not hits:
        raise SystemExit(f'coverage: {sorted(kinds)}, {wrapped} wrapped off-grid sets')
    print(f'ok: {total} ground queries on the {len(WORLDS)} worlds identical; {hits} hits, {wrapped} wrapped off-grid sets')


if __name__ == '__main__':
    main()
