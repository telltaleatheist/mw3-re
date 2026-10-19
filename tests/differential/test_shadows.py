"""Projected shadows and projected lights against the original x86 code
(docs/render-shadows.md), FPU at 24 bits as in play.

    cmake --build build && .venv/bin/python tests/differential/test_shadows.py [draws per world] [worlds...]

1. The draw-table entries with recorded Direct3D calls (the device vtable
   stubbed as in test_drawtable.py, extended to SRCBLEND / DESTBLEND,
   COLOROP, TEXCOORDINDEX, the filters and the multi-texture FVF): the
   shadowed draws 0x57eeb0, 0x57f2a0, 0x57f860 and the projected-light draws
   0x57ea20, 0x57fe20 on random polygons, shadow sets (0..7), light
   multipliers, flags, opacities, overlays and device caches; compared after
   every call: each DrawPrimitive (FVF, primitive, the DrawState it sees, the
   interleaved vertex bytes), the TL buffer, caches, state, the queue and the
   overlay list.
2. The caster lists (0x543720, 0x543760, 0x543810) on random lists: ties
   on the partition's edges, NaN and infinite boxes, disabled casters, more
   than the 200 slots. test_scenewalk.py runs them inside the walk on every
   world, with records over drawn nodes.
3. The model draw 0x53f2e0 (test_modeldraw.py's harness) with shadows on,
   casters over the model and projected lights (flags 0x10 | 0x40) near it:
   the receiver test, the shadowed clips, 0x547540 / 0x547740 with their
   subdivision, every table call with its shadow sets and light multipliers.

    --part N   run one part only
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
import test_drawtable as dt  # noqa: E402
from test_drawtable import Emu, probe, random_tl, random_work, rfloat, fbits, s32  # noqa: E402

FIELDS = dt.FIELDS + ('color_select', 'texcoord')
SHADOW_COUNT, SHADOW_TEX, SHADOW_UV, MULTI = 0x7489a8, 0x7489ac, 0x7489c8, 0x8008e0

probe.probe_sh_init.argtypes = [ctypes.c_void_p]
probe.probe_sh_set.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int32]
probe.probe_sh_get.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
probe.probe_sh_call.argtypes = [ctypes.c_int32, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p,
                                ctypes.c_void_p, ctypes.c_void_p]
probe.probe_sh_log.argtypes = [ctypes.c_void_p, ctypes.c_int32]


class ShEmu(Emu):
    """The draw-table emulator with the states and vertex formats of the shadow passes."""

    def __init__(self):
        super().__init__()
        self.blend = [5, 6]  # SRCBLEND, DESTBLEND as 0x57a900 leaves them
        self.light = self.cpu.alloc(64 * 12)

    def render_state(self, state, value):
        if state in (19, 20):
            self.blend[state - 19] = value
            self.state['multiply'] = int(self.blend == [1, 3])
        else:
            super().render_state(state, value)

    def stage_state(self, stage, kind, value):
        if stage != 0:
            raise SystemExit(f'stage {stage}')
        if kind == 1:
            if value not in (2, 4):
                raise SystemExit(f'COLOROP {value}')
            self.state['color_select'] = int(value == 2)
        elif kind == 11:
            self.state['texcoord'] = value
        elif kind in (16, 17):
            if value != 2:
                raise SystemExit(f'filter {kind} = {value}')
        else:
            super().stage_state(stage, kind, value)

    def draw(self, c):
        prim, fvf, verts, count, flags = (c.arg(i) for i in range(1, 6))
        if flags != 0x18:
            raise SystemExit(f'DrawPrimitive flags {flags:#x}')
        st = tuple(self.state[f] for f in FIELDS)
        if fvf == 0x1c4 and verts != MULTI:
            self.log.append(('draw', prim, count, st, c.read(verts, 32 * count)))
        elif fvf & 0xff == 0xc4 and verts == MULTI:
            sets = fvf >> 8
            self.log.append(('multi', prim, sets, count, st, c.read(verts, 4 * (6 + 2 * sets) * count)))
        else:
            raise SystemExit(f'DrawPrimitive fvf {fvf:#x} at {verts:#x}')
        return 0

    def set(self, cache, draw, light, brightest):
        cpu = self.cpu
        for a, v in zip(dt.CACHE, cache):
            cpu.u32(a, v)
        self.state = dict(zip(FIELDS, draw))
        cpu.write(dt.LIGHT, struct.pack('<3f', *light))
        cpu.u32(dt.BRIGHT, brightest)

    def get(self):
        cache = [s32(struct.unpack('<I', self.cpu.read(a, 4))[0]) for a in dt.CACHE]
        return cache, [self.state[f] for f in FIELDS]

    def call(self, fn, a, tl, work, shadow, light):
        cpu = self.cpu
        self.log = []
        cpu.write(dt.TL, tl)
        cpu.write(dt.WORK, work)
        cpu.u32(SHADOW_COUNT, shadow[0])
        for k in range(7):
            cpu.u32(SHADOW_TEX + 4 * k, self.tex(shadow[1 + k]))
        cpu.write(SHADOW_UV, struct.pack('<896f', *shadow[8]))
        cpu.write(self.light, struct.pack('<192f', *light))
        count, tex, flags, op, ov, pri, has_light = a
        lp = self.light if has_light else 0
        if fn == 0:
            cpu.call(0x57eeb0, ecx=count, edx=self.tex(tex))
        elif fn == 1:
            cpu.call(0x57f2a0, ecx=flags, edx=count, stack_args=(self.tex(tex),))
        elif fn == 2:
            cpu.call(0x57f860, ecx=flags, edx=count, stack_args=(self.tex(tex),))
        elif fn == 3:
            cpu.call(0x57ea20, ecx=lp, edx=flags, stack_args=(count, self.tex(tex), pri, op, ov))
        elif fn == 4:
            cpu.call(0x57fe20, ecx=lp, edx=flags, stack_args=(count, self.tex(tex)))
        return self.log, cpu.read(dt.TL, 64 * 32)


LOG_CAP = 1 << 20
log_buf = (ctypes.c_int32 * LOG_CAP)()


def port_log():
    n = probe.probe_sh_log(log_buf, LOG_CAP)
    w = log_buf[:n]
    out, i = [], 0
    while i < n:
        k = w[i]
        if k == 1:
            prim, count = w[i + 1], w[i + 2]
            st = tuple(w[i + 3:i + 16])
            out.append(('draw', prim, count, st, struct.pack(f'<{8 * count}i', *w[i + 16:i + 16 + 8 * count])))
            i += 16 + 8 * count
        elif k == 11:
            prim, sets, count = w[i + 1], w[i + 2], w[i + 3]
            st = tuple(w[i + 4:i + 17])
            m = (6 + 2 * sets) * count
            out.append(('multi', prim, sets, count, st, struct.pack(f'<{m}i', *w[i + 17:i + 17 + m])))
            i += 17 + m
        elif k == 8:
            m = w[i + 3]
            out.append(('zerr', w[i + 1], w[i + 2], ''.join(chr(x) for x in w[i + 4:i + 4 + m])))
            i += 4 + m
        else:
            raise SystemExit(f'bad probe log word {k} at {i}')
    return out


class Port:
    def setup(self, textures):
        flat = (ctypes.c_int32 * 64)(*[x for t in textures for x in t])
        probe.probe_dt_init(flat)
        probe.probe_sh_init(flat)

    def reset(self, count, vcount, ov):
        probe.probe_dt_reset(count, vcount, ov)

    def set(self, cache, draw, light, brightest):
        probe.probe_sh_set((ctypes.c_int32 * 7)(*cache), (ctypes.c_int32 * 13)(*draw), (ctypes.c_float * 3)(*light),
                           brightest)

    def get(self):
        c, d = (ctypes.c_int32 * 7)(), (ctypes.c_int32 * 13)()
        probe.probe_sh_get(c, d)
        return list(c), list(d)

    def call(self, fn, a, tl, work, shadow, light):
        tb = ctypes.create_string_buffer(bytes(tl), 64 * 32)
        sh = (ctypes.c_int32 * (8 + 896))(*shadow[:8], *[struct.unpack('<i', struct.pack('<f', x))[0]
                                                         for x in shadow[8]])
        probe.probe_sh_call(fn, (ctypes.c_int32 * 7)(*[s32(x & 0xffffffff) for x in a]), tb,
                            ctypes.create_string_buffer(bytes(work), 64 * 32), sh, (ctypes.c_float * 192)(*light))
        return port_log(), tb.raw[:64 * 32]


def describe(want, got):
    for i, (a, b) in enumerate(zip(want, got)):
        if a != b:
            if a[0] == b[0] and a[0] in ('draw', 'multi'):
                if a[:-1] != b[:-1]:
                    return f'entry {i}: {a[:-1]} vs {b[:-1]}'
                k = next(j for j in range(0, len(a[-1]), 4) if a[-1][j:j + 4] != b[-1][j:j + 4])
                return f'entry {i} {a[0]} dword {k // 4}: {a[-1][k:k + 4].hex()} vs {b[-1][k:k + 4].hex()}'
            return f'entry {i}: {a} vs {b}'
    return f'lengths {len(want)} vs {len(got)}: {want[len(got):][:1]} / {got[len(want):][:1]}'


def random_shadow(rng):
    n = rng.choice([0, 1, 1, 2, 3, 7, rng.randint(0, 7)])
    tex = [rng.randint(0, 15) for _ in range(7)]
    uv = [rfloat(rng, [0.0, 1.0, lambda r: r.uniform(-1, 2), lambda r: r.random()]) for _ in range(896)]
    return [n] + tex + [uv]


def random_light(rng):
    return [rfloat(rng, [1.0, 0.0, lambda r: r.uniform(0, 1), lambda r: r.uniform(0, 3), lambda r: r.uniform(-1, 1)])
            for _ in range(192)]


def random_device(rng, textures):
    cache = [rng.choice([5, 6, 6, 1]), rng.randint(0, 1), rng.choice([0, 1, 2]), rng.choice([0, 1, 2, 4, 5]),
             rng.choice([0, 1, 3]), rng.choice([0, 1, 3]), rng.randint(0, 1)]
    draw = [rng.choice([-1, textures[rng.randint(0, 15)][0]]), rng.choice([1, 3]), rng.choice([1, 3])] + \
        [rng.randint(0, 1) for _ in range(9)] + [rng.randint(0, 3)]
    light = [rfloat(rng, [0.0, 255.0, lambda r: r.uniform(0, 255), lambda r: r.uniform(0, 400)]) for _ in range(3)]
    return cache, draw, light, rng.randint(0, 2)


NAMES = ['0x57eeb0 shadowed', '0x57f2a0 shadowed lit', '0x57f860 shadowed per-vertex', '0x57ea20 projected',
         '0x57fe20 projected shadowed']


def part_draws(rng, sessions=250, calls=30):
    emu, port = ShEmu(), Port()
    stats = {'calls': 0, 'draws': 0, 'multi': 0, 'queued': 0, 'overlay': 0, 'errors': 0}
    per = [0] * 5
    for s in range(sessions):
        textures = [(0x1000 + i, rng.choice([1, 4, 5]), rng.choice([1, 3]), rng.choice([1, 3])) for i in range(16)]
        emu.setup(textures)
        port.setup(textures)
        start = rng.choice([(0, 0, 0)] * 6 + [(5999, 0, 0), (0, 23998, 0), (0, 0, 0x17f), (6000, 24000, 0x180)])
        emu.reset(*start)
        port.reset(*start)
        dev = random_device(rng, textures)
        emu.set(*dev)
        port.set(*dev)
        for c in range(calls):
            fn = rng.choice([0, 1, 2, 3, 3, 4])
            count = rng.choice([3, 3, 3, 4, 5, 8, rng.randint(1, 64), 64])
            op = fbits(rfloat(rng, [1.0, 1.0, 0.999, 0.5, 0.0, 1.5, lambda r: r.random(), lambda r: r.uniform(-0.1, 0.05)]))
            a = [count, rng.randint(0, 15), rng.randint(0, 7), op,
                 int(rng.random() < 0.25), rng.randint(0, 15), int(rng.random() < 0.8)]
            if rng.random() < 0.1:
                cache, draw, light, bright = random_device(rng, textures)
                cache[1:] = emu.get()[0][1:]
                draw = emu.get()[1]
                emu.set(cache, draw, light, bright)
                port.set(cache, draw, light, bright)
            tl, work = random_tl(rng), random_work(rng, 'lit')
            shadow, light = random_shadow(rng), random_light(rng)
            c0 = emu.counts()
            want, want_tl = emu.call(fn, a, tl, work, shadow, light)
            got, got_tl = port.call(fn, a, tl, work, shadow, light)
            where = f'session {s} call {c} {NAMES[fn]} {a} sets {shadow[0]}'
            if want != got:
                raise SystemExit(f'{where}: device calls differ, {describe(want, got)}')
            if want_tl != got_tl:
                k = next(j for j in range(0, 2048, 4) if want_tl[j:j + 4] != got_tl[j:j + 4])
                raise SystemExit(f'{where}: TL vertex {k // 32} field {k % 32 // 4}: '
                                 f'{want_tl[k:k + 4].hex()} vs {got_tl[k:k + 4].hex()}')
            dt.check(f'{where}: caches and state', tuple(emu.get()), tuple(port.get()))
            c1 = emu.counts()
            pc = tuple(dt.Port().counts())
            dt.check(f'{where}: queue and overlay counts', c1, pc)
            if c1[0] > c0[0]:
                eq, pq = emu.queue(), dt.Port().queue()
                last = (ctypes.c_int32 * 4)()
                probe.probe_sh_queue_last(last)
                dt.check(f'{where}: queued record', eq[1][-1], tuple(last))
                o = eq[1][-1][3]
                dt.check(f'{where}: queued vertices', eq[2][32 * o:], pq[2][32 * o:])
                stats['queued'] += 1
            if c1[2] > c0[2]:
                e, v = (ctypes.c_int32 * 3)(), ctypes.create_string_buffer(2048)
                probe.probe_sh_overlay_last(e, v)
                dt.check(f'{where}: overlay entry', emu.overlay()[-1], (e[0], e[1], e[2], v.raw))
                stats['overlay'] += 1
            stats['calls'] += 1
            per[fn] += 1
            stats['draws'] += sum(e[0] == 'draw' for e in want)
            stats['multi'] += sum(e[0] == 'multi' for e in want)
            stats['errors'] += sum(e[0] == 'zerr' for e in want)
    return (f"draws: {stats['calls']} calls ({', '.join(f'{n} {k}' for n, k in zip(NAMES, per))}): "
            f"{stats['multi']} multi-texture and {stats['draws']} plain DrawPrimitives, {stats['queued']} queued, "
            f"{stats['overlay']} overlay entries, {stats['errors']} reports; identical")


# ---------------------------------------------------------------- part 2: the caster lists

def part_casters(rng, runs=1500):
    """0x543720, 0x543760 and 0x543810 straight on random caster lists: ties
    on the partition box's edges, NaN and infinite boxes, disabled casters,
    lists over the 200 slots; the real worlds' run is in test_scenewalk.py."""
    from original import Original
    cpu = Original()
    cpu.fpu_control(0x7f)
    probe.probe_sh_cast.argtypes = [ctypes.c_void_p, ctypes.c_int32] + [ctypes.c_void_p] * 4
    recs = [cpu.alloc(0x90) for _ in range(260)]
    box_at = cpu.alloc(24)
    taken = cells = full = 0
    for run in range(runs):
        n = rng.choice([0, 1, 2, 5, 20, 60, 199, 200, 201, 230, 260])
        box = [rng.uniform(-500, 500), 0.0, rng.uniform(-500, 500)]
        box += [box[0] + rng.uniform(1, 300), 50.0, box[2] + rng.uniform(1, 300)]
        off = [0.0, 0.0, 0.0] if rng.random() < 0.5 else [rng.choice([-320.0, 640.0, rng.uniform(-900, 900)]), 0.0,
                                                          rng.choice([0.0, -640.0, rng.uniform(-900, 900)])]
        box, off = [f32(x) for x in box], [f32(x) for x in off]
        lo = [f32(box[0] - off[0]), f32(box[2] - off[2])]
        hi = [f32(box[3] - off[0]), f32(box[5] - off[2])]

        def coord(axis, which):
            return rfloat(rng, [lambda r: r.uniform(lo[axis] - 200, hi[axis] + 200), lo[axis], hi[axis],
                                float('nan'), float('inf'), -float('inf'), lambda r: r.uniform(-1e4, 1e4)])
        words, body = [], b''
        for i in range(n):
            on = int(rng.random() < 0.9)
            sx, sz = rfloat(rng, [0.0, lambda r: r.uniform(-3, 3)]), rfloat(rng, [0.0, lambda r: r.uniform(-3, 3)])
            mnx, mnz, mxx, mxz = coord(0, 0), coord(1, 0), coord(0, 1), coord(1, 1)
            w = [on] + [u32f(x) for x in (sx, sz, mnx, mnz, mxx, mxz)] + [0, 0]
            words += w
            rec = bytearray(0x8c)
            struct.pack_into('<2I', rec, 0x20, w[1], w[2])
            struct.pack_into('<I', rec, 0x2c, w[3])
            struct.pack_into('<I', rec, 0x34, w[4])
            struct.pack_into('<I', rec, 0x38, w[5])
            struct.pack_into('<I', rec, 0x40, w[6])
            struct.pack_into('<I', rec, 0x58, on)
            struct.pack_into('<I', rec, 0x88, recs[i + 1] if i + 1 < n else 0)
            cpu.write(recs[i], bytes(rec))
        view = [rng.uniform(-1, 1) for _ in range(9)] + [rng.uniform(-100, 100) for _ in range(3)]
        cpu.u32(0x64fbe0, recs[0] if n else 0)
        cpu.write(0x73fbc0, struct.pack('<12f', *view))
        cpu.write(0x73fbb0, struct.pack('<3f', *off))
        cpu.write(box_at, struct.pack('<6f', *box))
        cpu.call(0x543720)
        cpu.call(0x543760)
        cpu.call(0x543810, ecx=box_at)
        rd = lambda a: struct.unpack('<I', cpu.read(a, 4))[0]  # noqa: E731
        na = rd(0x7402c0)
        all_ids = [recs.index(rd(0x73fc60 + 4 * k)) for k in range(na)]
        slopes = [cpu.read(rd(0x73fc60 + 4 * k) + 0x4c, 12) for k in range(na)]
        nc = rd(0x73ff80)
        want = (all_ids, b''.join(slopes), [recs.index(rd(0x73ffa0 + 4 * k)) for k in range(nc)])
        out = (ctypes.c_uint32 * 1200)()
        probe.probe_sh_cast((ctypes.c_uint32 * max(1, len(words)))(*words), n, (ctypes.c_float * 12)(*view),
                            (ctypes.c_float * 3)(*off), (ctypes.c_float * 6)(*box), out)
        pa = out[0]
        p_all = list(out[1:1 + pa])
        p_slopes = bytes(out)[4 * (1 + pa):4 * (1 + 4 * pa)]
        pc = out[1 + 4 * pa]
        got = (p_all, p_slopes, list(out[2 + 4 * pa:2 + 4 * pa + pc]))
        if want != got:
            part = next(j for j in range(3) if want[j] != got[j])
            raise SystemExit(f'casters run {run} ({n} casters, offset {off}): '
                             f'{["the list", "the slopes", "the cell list"][part]} differs: {want[part]!r:.200} vs '
                             f'{got[part]!r:.200}')
        taken += na
        cells += nc
        full += int(na == 200) + int(nc == 200)
    return (f'casters: {runs} lists, {taken} casters taken, {cells} over their cell, {full} full lists; '
            'lists, slopes and cell lists bit for bit')


# ---------------------------------------------------------------- part 3: the model draw

def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


def u32f(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def norm(v):
    n = math.sqrt(sum(x * x for x in v)) or 1.0
    return [x / n for x in v]


def times3(p, m):
    """Row vector p times the 3x3 of m (12 floats, row-major)."""
    return [p[0] * m[j] + p[1] * m[3 + j] + p[2] * m[6 + j] for j in range(3)]


def random_casters(rng, cw, r, view, off):
    """Caster records over the model: boxes around its world centre cw
    (shifted back by the wrapped-cell offset), lights from above."""
    out = []
    for i in range(rng.choice([0, 1, 1, 2, 3, 5, 9])):
        L = norm([rng.uniform(-1, 1), -rng.uniform(0.25, 1), rng.uniform(-1, 1)])
        sx, sz = L[0] / L[1], L[2] / L[1]
        cx = cw[0] - off[0] + rng.uniform(-1, 1) * r
        cz = cw[2] - off[2] + rng.uniform(-1, 1) * r
        g = r * rng.uniform(0.2, 2.0)
        top = cw[1] + r * rng.uniform(-1.2, 1.5)
        low = cw[1] - r * rng.uniform(0.5, 1.5)
        ground = cw[1] - r * rng.uniform(-0.5, 1.0)
        bmin = [cx - g, low, cz - g]
        bmax = [cx + g * rng.uniform(0.5, 1.5), top, cz + g * rng.uniform(0.5, 1.5)]
        cam = times3([sx, 1.0, sz], view)
        pos = [cx + rng.uniform(-g, g), top, cz + rng.uniform(-g, g)]
        fl = [*pos, cx, cz, ground, sx, sz, math.hypot(sx, sz), *bmin, *bmax,
              1 / (bmax[0] - bmin[0]), 1 / (bmax[2] - bmin[2]), *cam]
        words = [u32f(x) for x in fl] + [0x60000000 + 0x10 * i, rng.getrandbits(16), 1, 0]
        out.append(words)
    return out


def random_lights(rng, centre_cam, r):
    """Projected lights (flags 0x10 | 0x40, ranged and on) near the model,
    now and then a saturated one or a plain point light among them."""
    out = []
    small = rng.random() < 0.3  # small lights on the model's skin: most polygons out of their reach
    for _ in range(rng.choice([0, 0, 1, 1, 1, 2, 3, 4] if not small else [2, 3, 4, 5])):
        d = norm([rng.uniform(-1, 1) for _ in range(3)])
        if small:
            dist, far = r * rng.uniform(0.3, 1.0), r * rng.uniform(0.05, 0.3) + 1
        else:
            dist = r * rng.choice([0.0, 0.3, 1.0, rng.uniform(0, 3)])
            far = r * rng.uniform(0.2, 3.0) + rng.uniform(1, 20)
        pos = [c + dist * k for c, k in zip(centre_cam, d)]
        near = far * rng.choice([0.0, 0.2, rng.uniform(0, 0.9)])
        flags = 0x10 | 0x40 | 0x2 | 0x200
        if rng.random() < 0.15:
            flags |= 0x20
        if rng.random() < 0.15:
            flags &= ~0x40
        b = bytearray(0xd0)
        struct.pack_into('<3f', b, 0x24, *pos)
        struct.pack_into('<3f', b, 0x60, *pos)
        struct.pack_into('<3f', b, 0x78, *pos)
        struct.pack_into('<3f', b, 0x90, *norm([rng.uniform(-1, 1) for _ in range(3)]))
        struct.pack_into('<2f', b, 0x9c, rng.uniform(0.3, 1.0), rng.uniform(0.0, 0.5))
        struct.pack_into('<3f', b, 0xa4, *(rng.uniform(0, 1) for _ in range(3)))
        struct.pack_into('<I', b, 0xb0, flags)
        struct.pack_into('<5f', b, 0xb4, near, far, f32(near) * f32(near), f32(far) * f32(far),
                         1 / (f32(far) - f32(near)))
        out.append(bytes(b))
    return out


def part_modeldraw(rng, draws, worlds):
    import test_modeldraw as md
    probe.probe_sh_md_setup.argtypes = [ctypes.c_void_p] * 3
    probe.probe_sh_md_casters.argtypes = [ctypes.c_void_p, ctypes.c_int32]
    probe.probe_sh_md_lights.argtypes = [ctypes.c_char_p, ctypes.c_int32]
    probe.probe_sh_md_mesh.argtypes = [ctypes.c_void_p]
    stats = md.Stats()
    total, meshes, sets_seen, entered = {}, [], {}, {}
    overflows = 0
    for world in worlds:
        orig = md.Original(world)
        path = md.ZBD / world / 'gamez.zbd'
        if md.probe.probe_md_load(str(path).encode()) != 0:
            raise SystemExit(f'{world}: the port could not read {path}')
        flags = bytes(rng.choice([0, 0, 0, 2]) for _ in range(orig.texture_count))
        orig.textures(flags)
        md.probe.probe_md_textures(flags, len(flags))
        info = (ctypes.c_int * 5)()
        nodes = []
        for i in range(md.probe.probe_md_node_count()):
            md.probe.probe_md_node(i, info)
            if info[0] and info[2] >= 0 and info[3] in (0, 1) and info[4] > 0 and md.probe.probe_md_max_poly(info[2]) <= 64:
                nodes.append((i, info[2], info[3]))
        lit = [n for n in nodes if orig.model_u32(n[1], 8) & 1]
        from unicorn import UC_HOOK_CODE
        from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_EIP
        for a in (0x547540, 0x547740, 0x547160):
            orig.cpu.uc.hook_add(UC_HOOK_CODE, lambda uc, addr, n, u: entered.__setitem__(addr, entered.get(addr, 0) + 1),
                                 begin=a, end=a)
        overflow = []
        ret = orig.cpu.code(b'\xc3')

        def split_fits(uc, addr, n, u):
            # A split past the 2048 triangles / 6144 vertices overruns the
            # original's arrays; the port refuses it (FIX (bug)): the original
            # gets the same fix here, the triangle left whole and inactive.
            t, e = uc.reg_read(UC_X86_REG_ECX), uc.reg_read(UC_X86_REG_EDX)
            across = orig.i32(0x64fdf0 + 0x1c * t + 0xc + 4 * e)
            if orig.i32(0x64fbec) + (1 if across < 0 else 2) > 2048 or orig.i32(0x64fbe8) + 1 > 6144:
                overflow.append(1)
                orig.cpu.u32(0x64fdf0 + 0x1c * t + 0x18, 0)
                uc.reg_write(UC_X86_REG_EIP, ret)
        orig.cpu.uc.hook_add(UC_HOOK_CODE, split_fits, begin=0x547160, end=0x547160)
        buf = ctypes.create_string_buffer(1 << 24)
        crng = random.Random(rng.random())
        calls = 0
        for k in range(draws):
            i, mi, mtype = rng.choice(lit if rng.random() < 0.85 else nodes)
            h = orig.cpu.read(orig.model_addr(mi), 0x60)
            centre = struct.unpack_from('<3f', h, 0x48)
            radius, = struct.unpack_from('<f', h, 0x54)
            world_m, cam, proj, clip, second, mask, st, fog = md.scenario(rng, centre, radius)
            st[0] = 1.0 if rng.random() < 0.9 else st[0]   # receivers need full opacity
            st[9] = rng.random() < 0.9                      # shadows on
            st[5] = k + 2
            uniform = rng.random() < 0.12
            if uniform:  # no light, everything fully fogged: the uniform shadowed draw 0x57f2a0
                st[7] = False
                fog = struct.pack('<ii', 1, 1) + md.fbits(0.8, 0.5, 0.2, 0.5, 0.25, 1.0, 1.0, 2.0, 2e6, 1e6,
                                                          1e-6, 2.0)
            views = orig.camera(cam, proj, clip, second)
            md.probe.probe_md_camera(md.fbits(*cam), md.fbits(*proj), clip, second, views)
            orig.state(st, fog)
            md.probe.probe_md_state(md.fbits(*st), fog)
            undo = md.edit(orig, rng, mi, mtype, i) if rng.random() < 0.3 else []
            if mtype == 1:
                mask |= 0x10
            view = struct.unpack('<12f', views[:48])
            cw = [sum(centre[q] * world_m[3 * q + j] for q in range(3)) + world_m[9 + j] for j in range(3)]
            r = max(radius, 0.5) * (math.sqrt(sum(world_m[q] ** 2 for q in range(3))) or 1.0)
            cc = [sum(cw[q] * view[3 * q + j] for q in range(3)) + view[9 + j] for j in range(3)]
            off = [0.0, 0.0, 0.0] if crng.random() < 0.6 else [crng.uniform(-600, 600), 0.0, crng.uniform(-600, 600)]
            off = [f32(x) for x in off]
            casters = random_casters(crng, cw, r, view, off)
            lights = [] if uniform else random_lights(crng, cc, r)
            point = [crng.uniform(0, 255) for _ in range(3)]
            # The original's side.
            cpu = orig.cpu
            cpu.write(0x73fbb0, struct.pack('<3f', *off))
            cpu.write(0x72f40c, struct.pack('<3f', *point))
            recs = []
            for words in casters:
                rec = orig.emu.alloc(0x90)
                body = bytearray(0x8c)
                struct.pack_into('<5I', body, 4, *words[:5])        # position, ground_at
                struct.pack_into('<I', body, 0x18, words[21])       # colour
                struct.pack_into('<15I', body, 0x1c, *words[5:20])  # ground_y .. slope_cam
                struct.pack_into('<I', body, 0x58, words[22])
                struct.pack_into('<I', body, 0x60, words[20])
                cpu.write(rec, bytes(body))
                recs.append(rec)
            lst = orig.emu.alloc(4 * max(1, len(recs)))
            cpu.write(lst, struct.pack(f'<{len(recs)}I', *recs))
            cpu.u32(0x73fc4c, lst)
            cpu.u32(0x7402c4, len(recs))
            n0 = orig.i32(0x74981c)
            for j, data in enumerate(lights):
                a = orig.emu.alloc(0xd0)
                cpu.write(a, data)
                cpu.write(0x749824 + 0x14 * (n0 + j), struct.pack('<5I', a, 0, 0, 0, 0))
            cpu.u32(0x74981c, n0 + len(lights))
            if n0 + len(lights):
                cpu.u32(0x749814, 1)
            inv_f = struct.unpack('<2f', cpu.read(0x63d758, 8))
            # The port's side.
            probe.probe_sh_md_setup((ctypes.c_float * 3)(*off), (ctypes.c_float * 2)(*inv_f),
                                    (ctypes.c_float * 3)(*point))
            flat = [w for c in casters for w in c]
            probe.probe_sh_md_casters((ctypes.c_uint32 * max(1, len(flat)))(*flat), len(casters))
            probe.probe_sh_md_lights(b''.join(lights), len(lights))
            overflow.clear()
            try:
                want = orig.draw(i, world_m, mask)
            except Exception as e:
                from unicorn.x86_const import UC_X86_REG_EIP
                raise SystemExit(f'{world} draw {k}: the original stopped at {cpu.uc.reg_read(UC_X86_REG_EIP):#x}: {e} '
                                 f'({len(casters)} casters, {len(lights)} lights, entered {entered}, projected '
                                 f'vertices {orig.i32(0x64fbe8)} triangles {orig.i32(0x64fbec)})')
            n = md.probe.probe_md_draw(i, md.fbits(*world_m), mask, buf, len(buf))
            got = buf.raw[:n]
            for fn in reversed(undo):
                fn()
            overflows += bool(overflow)
            if n < 0:
                raise SystemExit('event buffer too small')
            tol = md.FACADE_TOLERANCE if mtype == 1 else md.TOLERANCE
            why = md.compare(want, got, stats, tol, 0.01 if mtype == 1 else 0.0)
            if why:
                raise SystemExit(f'{world} draw {k} (node {i}, model {mi}, mask {mask:#x}, {len(casters)} casters, '
                                 f'{len(lights)} lights, state {st}): {why}')
            events = md.parse(want)
            for e in events:
                total[e[0]] = total.get(e[0], 0) + 1
                if e[0] in md.WITH_SETS:  # the set count follows the fields, lights and vertices
                    fmt = md.FORMATS[e[0]]
                    nv = max(0, min(e[1 + fmt.index('c')], 64))
                    sets = e[1 + len(fmt) + (3 * nv if e[0] in md.WITH_LIGHT else 0) + 14 * nv]
                    sets_seen[sets] = sets_seen.get(sets, 0) + 1
            if any(e[0] in (11, 12) for e in events):
                m = (ctypes.c_int32 * 2)()
                probe.probe_sh_md_mesh(m)
                meshes.append(m[1])
            calls += 1
        print(f'  {world}: {calls} draws identical')
    names = {0: 'flat', 1: 'lit', 2: 'textured', 3: 'textured lit', 4: 'textured per-vertex', 5: 'shadowed',
             6: 'shadowed lit', 7: 'shadowed per-vertex', 8: 'point', 9: 'model light', 10: 'fog colour',
             11: 'projected', 12: 'projected shadowed', 13: 'static bake', 14: 'zerr', 15: 'point colour',
             16: 'send mix', 17: 'mix fog'}
    biggest = max(meshes) if meshes else 0
    return (f"model draw: {draws * len(worlds)} draws with casters and projected lights, calls "
            f"{', '.join(f'{names.get(k, k)} {v}' for k, v in sorted(total.items()))}; shadow sets per call "
            f"{dict(sorted(sets_seen.items()))}; {len(meshes)} projected polygons, up to {biggest} triangles "
            f"(0x547540 {entered.get(0x547540, 0)}, 0x547740 {entered.get(0x547740, 0)}, 0x547160 splits "
            f"{entered.get(0x547160, 0)} in the original; {overflows} polygons that would overrun the original's 2048 "
            f"triangles); "
            f"floats {stats.floats} ({stats.exact} bit-identical, worst {stats.worst:.1e}); identical")


def main():
    args = sys.argv[1:]
    part = None
    if '--part' in args:
        i = args.index('--part')
        part = int(args[i + 1])
        del args[i:i + 2]
    rng = random.Random(7)
    draws = int(args[0]) if args else 500
    worlds = args[1:] or ['c1', 'c4b', 't1']
    if part in (None, 1):
        print(part_draws(rng))
    if part in (None, 2):
        print(part_casters(rng))
    if part in (None, 3):
        print(part_modeldraw(rng, draws, worlds))
    print('ok: projected shadows and lights identical to the original (24-bit FPU)')


if __name__ == '__main__':
    main()
