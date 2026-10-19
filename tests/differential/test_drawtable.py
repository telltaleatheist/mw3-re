"""The ported Direct3D draw table against the original x86 code.

    cmake --build build && .venv/bin/python tests/differential/test_drawtable.py

The original's functions run in Unicorn with the FPU at 24 bits (as in play)
and the IDirect3DDevice3 vtable pointed at Python stubs that record every call.
The recorded SetRenderState / SetTextureStageState / SetTexture calls are
turned into the platform::DrawState they leave, and each DrawPrimitive into
(primitive, DrawState, vertex bytes): what the port must hand Render3D. The
port draws into a recording Render3D (tests/probe_drawtable.cpp). Compared
exactly after every call: the draws, the TL buffer the draw functions write in
place, the state caches, the transparent queue and the overlay list (records
and vertex bytes, stale bytes included), error reports.

Draws: 0x57bf80, 0x57c700, 0x57ce20, 0x57caa0, 0x57d160, 0x57d690 on random work
records, TL vertices, materials, opacities, priorities, overlay flags, texture
types, point lights, display formats and cache states, in long sequences with
the transparent flush 0x57e110 and the overlay flush 0x57e4c0 between them,
next to the queue and overlay limits. Also the frame calls (0x57a740,
0x57a7e0, 0x5777f0, 0x57a850), the point-light colour 0x5798a0, FOGCOLOR
0x57bcf0, and texture records 0x57b130 (with DirectDraw stubs: the surface it
fills, decoded to RGBA8, against what the port uploads).
"""
import array
import ctypes
import math
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import FTOL, IAT_FTOL, ROOT, Original  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))

TL, WORK, PRIM = 0x72f41c, 0x7481a8, 0x72f418
LIGHT, BRIGHT, FULLSCREEN = 0x72f40c, 0x802704, 0x8026e0
MASKS, SHIFTS, GBITS = 0x80272c, 0x802738, 0x802724
CACHE = [PRIM, 0x809388, 0x80938c, 0x809390, 0x809394, 0x809398, 0x8093a8]  # = DeviceState
QCOUNT, POLYS, POOL = 0x803550, 0x80cd9c, 0x82449c
OVCOUNT, OVER, FLAT = 0x809318, 0x8dfc9c, 0x80c59c
THIS, VTABLE = 0x809370, 0x809374
ZERR, D3DERR = 0x49bfd0, 0x580550
FOG_SENT = 0x72f400
DIRTY, FILL, DITHER = 0x8026f8, 0x8026fc, 0x802700
CLEAR_ON, CLEAR_COLOR, BACK_RECORD, ZBUF, PRIMARY = 0x8026f0, 0x802794, 0x8027a4, 0x809378, 0x80280c
K255 = struct.unpack('<f', struct.pack('<I', 0x3b808081))[0]  # 0x5ceb78, 1/255
FIELDS = ('texture', 'address_u', 'address_v', 'blend', 'multiply', 'depth_write', 'depth_always',
          'alpha_modulate', 'fog', 'flat', 'wireframe')
FORMATS = {565: (0xf800, 0x7e0, 0x1f, 8, 3, 3, 6), 555: (0x7c00, 0x3e0, 0x1f, 7, 2, 3, 5)}


def s32(x):
    return x - (1 << 32) if x >= 1 << 31 else x


def fbits(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def pattern(r, k):
    return ((r << 28) ^ (k * 2654435761)) & 0xffffffff


POOL_FILL = array.array('I', (pattern(1, k) for k in range(24000 * 8))).tobytes()
OVER_FILL = [array.array('I', (pattern(2, e * 512 + k) for k in range(512))).tobytes() for e in range(0x180)]
FLAT_FILL = array.array('I', (pattern(3, k) for k in range(512))).tobytes()


def c_format(cpu, fmt, args):
    """printf for the %d / %s the zvid_ddd3d messages use."""
    out, i, n = '', 0, 0
    while i < len(fmt):
        if fmt[i] == '%' and i + 1 < len(fmt):
            a = args[n]
            n += 1
            if fmt[i + 1] == 's':
                out += cpu.read(a, 256).split(b'\0')[0].decode('latin-1')
            else:
                out += str(s32(a))
            i += 2
        else:
            out += fmt[i]
            i += 1
    return out


# ---------------------------------------------------------------- the original

class Emu:
    def __init__(self):
        cpu = self.cpu = Original()
        cpu.import_code(IAT_FTOL, FTOL)
        cpu.fpu_control(0x7f)  # 24 bits, as Direct3D leaves it in play
        self.log = []
        self.state = {}
        vt = cpu.alloc(0x100)
        cpu.u32(THIS, 0xdeadbeef)
        cpu.u32(VTABLE, vt)

        def method(table, off, fn, pop):
            addr = cpu.code(b'\xc3')
            cpu.u32(table + off, addr)
            cpu.stub_function(addr, fn, pop)
        method(vt, 0x58, lambda c: self.render_state(c.arg(1), c.arg(2)), 12)
        method(vt, 0xa0, lambda c: self.stage_state(c.arg(1), c.arg(2), c.arg(3)), 16)
        method(vt, 0x98, lambda c: self.set_texture(c.arg(1), c.arg(2)), 12)
        method(vt, 0x70, self.draw, 24)
        method(vt, 0x24, lambda c: self.log.append(('begin',)), 4)
        method(vt, 0x28, lambda c: self.log.append(('end',)), 4)
        cpu.stub_function(ZERR, self.zerr)
        cpu.stub_function(D3DERR, lambda c: self.log.append(('d3derr', c.arg(0))), 4)
        self.textures = cpu.alloc(16 * 0x18)
        self.material = cpu.alloc(16)
        self.rect = cpu.alloc(16)
        # Surfaces for the frame calls: Blt (+0x14) and Flip (+0x2c).
        self.blts = []
        for name, addr in (('back', BACK_RECORD + 0x20), ('z', ZBUF)):
            obj, tab = cpu.alloc(16), cpu.alloc(0x100)
            cpu.u32(obj, tab)
            cpu.u32(addr, obj)
            method(tab, 0x14, lambda c, n=name: self.blts.append(
                (n, c.arg(4), struct.unpack('<I', c.read(c.arg(5) + 0x50, 4))[0])), 24)
        self.primary = cpu.alloc(16)
        tab = cpu.alloc(0x100)
        cpu.u32(self.primary, tab)
        method(tab, 0x2c, lambda c: self.log.append(('present',)), 12)

    def zerr(self, c):
        fmt = c.read(c.arg(3), 256).split(b'\0')[0].decode('latin-1')
        self.log.append(('zerr', c.arg(0), c.arg(2), c_format(c, fmt, [c.arg(4), c.arg(5), c.arg(6)])))

    # The D3D calls, as the DrawState they leave (the port's FIX (A) mapping).
    def render_state(self, state, value):
        st = self.state
        if state == 8:
            st['wireframe'] = int(value == 2)
        elif state == 9:
            st['flat'] = int(value == 1)
        elif state == 14:
            st['depth_write'] = int(value != 0)
        elif state == 23:
            st['depth_always'] = int(value == 8)
        elif state == 27:
            st['blend'] = int(value != 0)
        elif state == 28:
            st['fog'] = int(value != 0)
        elif state == 34:
            self.log.append(('fog', value >> 16 & 0xff, value >> 8 & 0xff, value & 0xff))
        elif state != 26:
            raise SystemExit(f'unexpected SetRenderState({state}, {value})')

    def stage_state(self, stage, kind, value):
        if stage != 0:
            raise SystemExit(f'stage {stage}')
        if kind == 4:
            if value not in (2, 4):
                raise SystemExit(f'ALPHAOP {value}')
            self.state['alpha_modulate'] = int(value == 4)
        elif kind in (13, 14):
            self.state['address_u' if kind == 13 else 'address_v'] = value
        else:
            raise SystemExit(f'unexpected SetTextureStageState({kind}, {value})')

    def set_texture(self, stage, tex):
        if stage != 0:
            raise SystemExit(f'SetTexture stage {stage}')
        self.state['texture'] = s32(tex) if tex else -1

    def draw(self, c):
        prim, fvf, verts, count, flags = (c.arg(i) for i in range(1, 6))
        if fvf != 0x1c4 or flags != 0x18:
            raise SystemExit(f'DrawPrimitive fvf {fvf:#x} flags {flags:#x}')
        self.log.append(('draw', prim, count, tuple(self.state[f] for f in FIELDS), c.read(verts, 32 * count)))
        return 0

    # Setup and reads.
    def setup(self, textures):
        for i, (h, t, u, v) in enumerate(textures):
            self.cpu.write(self.textures + 0x18 * i, struct.pack('<6I', 0, 0, h, t, u, v))

    def reset(self, count, vcount, ov):
        cpu = self.cpu
        cpu.write(QCOUNT, struct.pack('<II', count, vcount))
        cpu.u32(OVCOUNT, ov)
        # Polygons already queued: one vertex each, untextured, so a flush can draw them.
        cpu.write(POLYS, b''.join(struct.pack('<4I', 6, 1, 0, POOL + 32 * min(i, max(vcount - 1, 0)))
                                  for i in range(count)) + bytes(16 * (6000 - count)))
        cpu.write(POOL, POOL_FILL)
        for e in range(0x180):
            cpu.write(OVER + 0x80c * e + 0xc, OVER_FILL[e])
        cpu.write(FLAT, FLAT_FILL)

    def set(self, cache, draw, light, brightest, fmt):
        cpu = self.cpu
        for a, v in zip(CACHE, cache):
            cpu.u32(a, v)
        self.state = dict(zip(FIELDS, draw))
        cpu.write(LIGHT, struct.pack('<3f', *light))
        cpu.u32(BRIGHT, brightest)
        cpu.write(MASKS, struct.pack('<3I', *fmt[:3]))
        cpu.write(SHIFTS, struct.pack('<3I', *fmt[3:6]))
        cpu.u32(GBITS, fmt[6])

    def get(self):
        cpu = self.cpu
        cache = [s32(struct.unpack('<I', cpu.read(a, 4))[0]) for a in CACHE]
        light = list(struct.unpack('<3f', cpu.read(LIGHT, 12)))
        return cache, [self.state[f] for f in FIELDS], light, struct.unpack('<i', cpu.read(BRIGHT, 4))[0]

    def tex(self, i):
        return self.textures + 0x18 * i

    def call(self, fn, a, tl, work, rgb):
        cpu = self.cpu
        self.log = []
        cpu.write(TL, tl)
        cpu.write(WORK, work)
        cpu.write(self.material, struct.pack('<3f', *rgb))
        if fn == 0:
            cpu.call(0x57bf80, ecx=a[0], edx=a[1], stack_args=(a[2], a[3], a[4]))
        elif fn == 1:
            cpu.call(0x57c700, ecx=a[0], edx=self.material, stack_args=(a[1], a[2], a[3], a[4], a[5]))
        elif fn == 2:
            cpu.call(0x57ce20, ecx=a[0], edx=self.tex(a[1]), stack_args=(a[2], a[3]))
        elif fn == 3:
            cpu.call(0x57d160, ecx=a[0], edx=a[1], stack_args=(self.tex(a[2]), a[3], a[4], a[5]))
        elif fn == 4:
            cpu.call(0x57d690, ecx=a[0], edx=a[1], stack_args=(self.tex(a[2]), a[3], a[4]))
        elif fn == 5:
            cpu.call(0x57e110)
        elif fn == 6:
            cpu.call(0x57e4c0)
        elif fn == 7:
            cpu.write(DIRTY, struct.pack('<3I', a[0], a[1], a[2]))
            cpu.call(0x57a740)
            self.log.append(('dirty', struct.unpack('<I', cpu.read(DIRTY, 4))[0]))
        elif fn == 8:
            cpu.call(0x57a7e0)
        elif fn == 9:
            cpu.u32(FULLSCREEN, a[0])
            cpu.call(0x5798a0, ecx=self.material)
        elif fn == 10:
            cpu.write(FOG_SENT, struct.pack('<3f', *rgb))
            cpu.call(0x57bcf0)
        elif fn == 11:
            cpu.u32(CLEAR_ON, a[0])
            cpu.u32(CLEAR_COLOR, a[1])
            self.blts = []
            cpu.call(0x5777f0, ecx=self.rect, edx=BACK_RECORD)
            color = [f for n, fl, f in self.blts if n == 'back' and fl == 0x1000400]
            depth = [f for n, fl, f in self.blts if n == 'z' and fl == 0x2000000]
            if depth != [0]:
                raise SystemExit(f'frame clear: depth fills {self.blts}')
            if color:
                v = unpack_color(color[0], self.fmt)
                self.log.append(('clear', 1, v >> 16 & 0xff, v >> 8 & 0xff, v & 0xff))
            else:
                self.log.append(('clear', 0, 0, 0, 0))
        elif fn == 13:
            cpu.call(0x57caa0, ecx=a[0], edx=self.tex(a[1]), stack_args=(a[2], a[3], a[4]))
        elif fn == 12:
            cpu.u32(PRIMARY, self.primary if a[0] else 0)
            cpu.call(0x57a850, ecx=self.rect, edx=self.rect, stack_args=(1, 0))
            self.log.append(('ret', s32(cpu.uc.reg_read(UC_X86_REG_EAX))))
        return self.log, cpu.read(TL, 64 * 32)

    def counts(self):
        n, v = struct.unpack('<II', self.cpu.read(QCOUNT, 8))
        return n, v, s32(struct.unpack('<I', self.cpu.read(OVCOUNT, 4))[0])

    def queue(self):
        cpu = self.cpu
        n, v = struct.unpack('<II', cpu.read(QCOUNT, 8))
        raw = array.array('I', cpu.read(POLYS, 16 * min(n, 6000)))
        polys = []
        for i in range(min(n, 6000)):
            p, c, t, vp = raw[4 * i:4 * i + 4]
            polys.append((p, c, (t - self.textures) // 0x18 + 1 if t else 0, (vp - POOL) // 32 if vp else -1))
        return (n, v), polys, cpu.read(POOL, 32 * min(v, 24000))

    def overlay(self):
        cpu = self.cpu
        n = s32(struct.unpack('<I', cpu.read(OVCOUNT, 4))[0])
        raw = cpu.read(OVER, 0x80c * n)
        out = []
        for i in range(n):
            t, c, tex = struct.unpack_from('<3I', raw, 0x80c * i)
            out.append((t, c, (tex - self.textures) // 0x18 + 1 if tex else 0, raw[0x80c * i + 0xc:0x80c * (i + 1)]))
        return out

    def flat(self):
        return self.cpu.read(FLAT, 64 * 32)


def unpack_color(c, fmt):
    """0x57bf80's unpack of a display-packed colour, alpha 0."""
    rm, gm, bm, rs, gs, bs = fmt[:6]
    v = (c & rm) >> (rs & 31)
    v = (v << 8) | ((c & gm) >> (gs & 31))
    return ((v << 8) | ((c & bm) << (bs & 31))) & 0xffffffff


# ---------------------------------------------------------------- the port

LOG_CAP = 1 << 20
log_buf = (ctypes.c_int32 * LOG_CAP)()


def port_log():
    n = probe.probe_dt_log(log_buf, LOG_CAP)
    if n > LOG_CAP:
        raise SystemExit('probe log overflow')
    w = log_buf[:n]
    out, i = [], 0
    while i < n:
        k = w[i]
        if k == 1:
            prim, count = w[i + 1], w[i + 2]
            st = tuple(w[i + 3:i + 14])
            verts = struct.pack(f'<{8 * count}i', *w[i + 14:i + 14 + 8 * count])
            out.append(('draw', prim, count, st, verts))
            i += 14 + 8 * count
        elif k == 2:
            out.append(('tex', w[i + 1], w[i + 2], w[i + 3]))
            i += 4
        elif k == 3:
            color = w[i + 1]
            out.append(('clear', color) + (tuple(w[i + 2:i + 5]) if color else (0, 0, 0)))
            i += 5
        elif k in (4, 5, 7):
            out.append(({4: 'begin', 5: 'end', 7: 'present'}[k],))
            i += 1
        elif k == 6:
            out.append(('fog',) + tuple(w[i + 1:i + 4]))
            i += 4
        elif k == 8:
            m = w[i + 3]
            out.append(('zerr', w[i + 1], w[i + 2], ''.join(chr(x) for x in w[i + 4:i + 4 + m])))
            i += 4 + m
        elif k == 9:
            out.append(('dirty', w[i + 1] & 0xffffffff))
            i += 2
        elif k == 10:
            out.append(('ret', w[i + 1]))
            i += 2
        else:
            raise SystemExit(f'bad probe log word {k} at {i}')
    return out


class Port:
    def setup(self, textures):
        probe.probe_dt_init((ctypes.c_int32 * 64)(*[x for t in textures for x in t]))

    def reset(self, count, vcount, ov):
        probe.probe_dt_reset(count, vcount, ov)

    def set(self, cache, draw, light, brightest, fmt):
        probe.probe_dt_set((ctypes.c_int32 * 7)(*cache), (ctypes.c_int32 * 11)(*draw),
                           (ctypes.c_float * 3)(*light), brightest, (ctypes.c_int32 * 7)(*fmt))

    def get(self):
        c, d, l, b = (ctypes.c_int32 * 7)(), (ctypes.c_int32 * 11)(), (ctypes.c_float * 3)(), ctypes.c_int32()
        probe.probe_dt_get(c, d, l, ctypes.byref(b))
        return list(c), list(d), list(l), b.value

    def call(self, fn, a, tl, work, rgb):
        tb = ctypes.create_string_buffer(bytes(tl), 64 * 32)
        args = (ctypes.c_int32 * 8)(*[s32(x & 0xffffffff) for x in a])
        probe.probe_dt_call(fn, args, tb, ctypes.create_string_buffer(bytes(work), 64 * 32),
                            (ctypes.c_float * 3)(*rgb))
        return port_log(), tb.raw[:64 * 32]

    def counts(self):
        out = (ctypes.c_int32 * 3)()
        probe.probe_dt_counts(out)
        return tuple(out)

    def queue(self):
        counts = (ctypes.c_int32 * 2)()
        polys = (ctypes.c_int32 * (4 * 6000))()
        pool = ctypes.create_string_buffer(24000 * 32)
        n = probe.probe_dt_queue(counts, polys, pool)
        recs = [tuple(polys[4 * i:4 * i + 4]) for i in range(min(n, 6000))]
        return (counts[0], counts[1]), recs, pool.raw[:32 * min(counts[1], 24000)]

    def overlay(self):
        e = (ctypes.c_int32 * (3 * 0x180))()
        v = ctypes.create_string_buffer(0x180 * 64 * 32)
        n = probe.probe_dt_overlay(e, v)
        return [(e[3 * i], e[3 * i + 1], e[3 * i + 2], v.raw[2048 * i:2048 * (i + 1)]) for i in range(n)]

    def flat(self):
        b = ctypes.create_string_buffer(64 * 32)
        probe.probe_dt_flat(b)
        return b.raw


# ---------------------------------------------------------------- inputs

def rfloat(rng, choices):
    c = rng.choice(choices)
    return c(rng) if callable(c) else c


def random_tl(rng):
    out = bytearray()
    for _ in range(64):
        sz = rfloat(rng, [lambda r: r.uniform(0.001, 1), 0.5, 1.0, lambda r: r.uniform(0, 4)])
        out += struct.pack('<4f2I2f', rng.uniform(-10, 650), rng.uniform(-10, 490), sz, rng.random(),
                           rng.getrandbits(32), rng.getrandbits(32), rng.uniform(-2, 2), rng.uniform(-2, 2))
    return bytes(out)


COLOR = [0.0, 255.0, 127.5, 0.5, 254.5, lambda r: r.uniform(0, 255), lambda r: float(r.randint(0, 255)),
         lambda r: r.uniform(250, 600), lambda r: r.uniform(-3, 3)]
POINT = [0.0, K255, lambda r: r.uniform(0, 0.01), lambda r: r.uniform(0, 1), lambda r: r.uniform(0, 3),
         struct.unpack('<f', struct.pack('<I', 0x3b808082))[0], 1.0]


def random_work(rng, mode):
    out = bytearray()
    for _ in range(64):
        dark = rfloat(rng, [0.0, 1.0, 0.5, lambda r: r.random(), lambda r: r.uniform(-0.3, 1.3)])
        point = rfloat(rng, POINT) if mode != 'plain' else 0.0
        if rng.random() < 0.01:
            point = float('nan')
        fog = rfloat(rng, [0.0, 1.0, lambda r: r.random(), lambda r: r.uniform(-0.2, 1.2), 0.5])
        col = [rfloat(rng, COLOR) for _ in range(3)]
        out += struct.pack('<8f', dark, point, fog, *col, rng.random(), rng.random())
    return bytes(out)


def opacity(rng):
    return fbits(rfloat(rng, [1.0, 1.0, 1.0, 0.999, 0.5, 0.0, 1.5, lambda r: r.random(),
                              lambda r: r.uniform(-0.1, 0.05)]))


def random_call(rng):
    fn = rng.choices([0, 1, 2, 3, 4, 5, 6, 13], [5, 5, 5, 5, 5, 1, 1, 5])[0]
    count = rng.choice([1, 2, 3, 3, 4, 4, 5, 6, 8, rng.randint(1, 64), 64])
    ov = 1 if rng.random() < 0.25 else 0
    alpha = rng.choice([255, 255, 254, 0, 128, rng.randint(0, 255), 256, 300, -1])
    flags = rng.randint(0, 7)
    tex = rng.randint(0, 15)
    pri = rng.randint(0, 15)
    if fn == 0:
        return fn, [rng.getrandbits(16), alpha, count, pri, ov]
    if fn == 1:
        return fn, [rng.getrandbits(16), flags, alpha, count, pri, ov]
    if fn == 2:
        return fn, [count, tex, opacity(rng), ov]
    if fn == 3:
        return fn, [flags, count, tex, pri, opacity(rng), ov]
    if fn == 4:
        return fn, [flags, count, tex, opacity(rng), ov]
    if fn == 13:
        return fn, [count, tex, pri, opacity(rng), ov]
    return fn, []


def random_device(rng, textures):
    cache = [rng.choice([5, 6, 6, 1]), rng.randint(0, 1), rng.choice([0, 1, 2]), rng.choice([0, 1, 2, 4, 5]),
             rng.choice([0, 1, 3]), rng.choice([0, 1, 3]), rng.randint(0, 1)]
    draw = [rng.choice([-1, textures[rng.randint(0, 15)][0]]), rng.choice([1, 3]), rng.choice([1, 3])] + \
        [rng.randint(0, 1) for _ in range(8)]
    light = [rfloat(rng, [0.0, 255.0, lambda r: r.uniform(0, 255), lambda r: r.uniform(0, 400)]) for _ in range(3)]
    return cache, draw, light, rng.randint(0, 2), FORMATS[rng.choice([565, 565, 555])]


# ---------------------------------------------------------------- tests

def describe_diff(want, got):
    for i, (a, b) in enumerate(zip(want, got)):
        if a != b:
            if a[0] == 'draw' and b[0] == 'draw':
                if a[:4] != b[:4]:
                    return f'entry {i}: {a[:4]} vs {b[:4]}'
                va, vb = array.array('I', a[4]), array.array('I', b[4])
                k = next(j for j in range(len(va)) if va[j] != vb[j])
                return f'entry {i} draw vertex {k // 8} field {k % 8}: {va[k]:#x} vs {vb[k]:#x}'
            return f'entry {i}: {a} vs {b}'
    return f'lengths {len(want)} vs {len(got)}: {want[len(got):][:2]} / {got[len(want):][:2]}'


def check(what, want, got):
    if want != got:
        if isinstance(want, tuple) and isinstance(got, tuple) and len(want) == len(got):
            for i, (a, b) in enumerate(zip(want, got)):
                if a != b:
                    if isinstance(a, (list, bytes)) and len(a) == len(b):
                        k = next(j for j in range(len(a)) if a[j] != b[j])
                        raise SystemExit(f'{what}: part {i} item {k}: original {a[k]!r:.300} rebuilt {b[k]!r:.300}')
                    raise SystemExit(f'{what}: part {i}: original {a!r:.300} rebuilt {b!r:.300}')
        raise SystemExit(f'{what}: original {want!r:.300} rebuilt {got!r:.300}')


def test_draws(rng, emu, port, sessions=300, calls=40):
    stats = {'calls': 0, 'draws': 0, 'queued': 0, 'overlay': 0, 'errors': 0, 'flushed': 0}
    names = {0: 'flat', 1: 'lit', 2: 'textured', 3: 'textured lit', 4: 'vertex lit', 5: 'flush',
             6: 'overlay flush', 13: 'textured white'}
    for s in range(sessions):
        textures = [(0x1000 + i, rng.choice([1, 4, 5]), rng.choice([1, 3]), rng.choice([1, 3])) for i in range(16)]
        emu.setup(textures)
        port.setup(textures)
        start = rng.choice([(0, 0, 0)] * 6 + [(5998, 0, 0), (0, 23990, 0), (5990, 23950, 0x17c), (0, 0, 0x17e),
                                              (6000, 24000, 0x180)])
        emu.reset(*start)
        port.reset(*start)
        dev = random_device(rng, textures)
        emu.set(*dev)
        port.set(*dev)
        emu.fmt = dev[4]
        for c in range(calls):
            fn, a = random_call(rng)
            if rng.random() < 0.1:  # a new primitive or point light between polygons
                dev = random_device(rng, textures)
                cache, draw, light, bright, fmt = dev
                cache = emu.get()[0]
                cache[0] = dev[0][0]
                emu.set(cache, emu.get()[1], light, bright, fmt)
                port.set(cache, port.get()[1], light, bright, fmt)
                emu.fmt = fmt
            mode = 'plain' if rng.random() < 0.2 else 'lit'
            tl, work = random_tl(rng), random_work(rng, mode)
            rgb = [rfloat(rng, COLOR) for _ in range(3)]
            c0 = emu.counts()
            want, want_tl = emu.call(fn, a, tl, work, rgb)
            got, got_tl = port.call(fn, a, tl, work, rgb)
            where = f'session {s} call {c} {names[fn]} {a}'
            if want != got:
                raise SystemExit(f'{where}: device calls differ, {describe_diff(want, got)}')
            if want_tl != got_tl:
                k = next(j for j in range(0, 2048, 4) if want_tl[j:j + 4] != got_tl[j:j + 4])
                raise SystemExit(f'{where}: TL buffer vertex {k // 32} field {k % 32 // 4}: '
                                 f'{want_tl[k:k + 4].hex()} vs {got_tl[k:k + 4].hex()}')
            check(f'{where}: caches, state, light', emu.get(), port.get())
            c1 = emu.counts()
            check(f'{where}: queue and overlay counts', c1, port.counts())
            if c1[0] > c0[0]:  # the new polygon and its vertices
                eq, pq = emu.queue(), port.queue()
                check(f'{where}: queued record', eq[1][-1], pq[1][-1])
                o = eq[1][-1][3]
                check(f'{where}: queued vertices', eq[2][32 * o:], pq[2][32 * o:])
                stats['queued'] += 1
            if c1[2] > c0[2]:
                check(f'{where}: overlay entry', emu.overlay()[-1], port.overlay()[-1])
                stats['overlay'] += 1
            stats['calls'] += 1
            stats['draws'] += sum(e[0] == 'draw' for e in want)
            stats['errors'] += sum(e[0] == 'zerr' for e in want)
            stats['flushed'] += fn in (5, 6)
        check(f'session {s}: queue', emu.queue(), port.queue())
        check(f'session {s}: overlay', emu.overlay(), port.overlay())
        check(f'session {s}: flat buffer', emu.flat(), port.flat())
    return (f"{stats['calls']} calls in {sessions} sequences: {stats['draws']} draws, {stats['queued']} queued, "
            f"{stats['overlay']} overlay entries, {stats['errors']} overflow reports, {stats['flushed']} flushes; "
            'device calls, TL buffer, caches, queue and overlay identical')


def test_flush(rng, emu, port):
    """0x57e110 on big random queues (as test_fogsort did before Render3D)."""
    runs = draws = 0
    for case in range(150):
        textures = [(0x1000 + i, rng.choice([1, 4, 5, 2]), rng.choice([1, 3]), rng.choice([1, 3])) for i in range(16)]
        emu.setup(textures)
        port.setup(textures)
        emu.reset(0, 0, 0)
        port.reset(0, 0, 0)
        dev = random_device(rng, textures)
        emu.set(*dev)
        port.set(*dev)
        n = rng.choice([1, 2, 5, 20, 100, rng.randint(1, 600)])
        for _ in range(n):  # fill the queue through 0x57ce20 / 0x57bf80 with transparent polygons
            cnt = rng.randint(1, 6)
            tl = random_tl(rng)
            work = random_work(rng, 'plain')
            if rng.random() < 0.3:
                a = [rng.getrandbits(16), rng.randint(0, 254), cnt, 0, 0]
                fn = 0
            else:
                a = [cnt, rng.randint(0, 15), fbits(rng.choice([0.5, 0.25, 0.999])), 0]
                fn = 2
            emu.call(fn, a, tl, work, [0, 0, 0])
            port.call(fn, a, tl, work, [0, 0, 0])
        want = emu.call(5, [], tl, work, [0, 0, 0])[0]
        got = port.call(5, [], tl, work, [0, 0, 0])[0]
        if want != got:
            raise SystemExit(f'flush case {case} n={n}: {describe_diff(want, got)}')
        check(f'flush case {case}: state', emu.get(), port.get())
        check(f'flush case {case}: counts', emu.queue()[0], port.queue()[0])
        runs += 1
        draws += n
    return f'{runs} big flushes ({draws} polygons) identical'


def test_frame(rng, emu, port):
    runs = 0
    textures = [(0x1000 + i, 1, 1, 1) for i in range(16)]
    emu.setup(textures)
    port.setup(textures)
    tl, work = random_tl(rng), random_work(rng, 'plain')
    for case in range(400):
        dev = random_device(rng, textures)
        emu.set(*dev)
        port.set(*dev)
        emu.fmt = dev[4]
        fn = rng.choice([7, 8, 9, 10, 11, 12])
        a = [rng.randint(0, 3), rng.randint(0, 2), rng.randint(0, 1)] if fn == 7 else \
            [rng.randint(0, 1)] if fn in (9, 12) else [rng.randint(0, 1), rng.getrandbits(16)] if fn == 11 else []
        rgb = [rfloat(rng, [0.0, 1.0, 0.5, lambda r: r.random(), lambda r: r.uniform(0, 300)]) for _ in range(3)]
        if fn == 9 and rng.random() < 0.3:
            rgb[rng.randint(0, 2)] = rgb[rng.randint(0, 2)]  # ties
        want = emu.call(fn, a, tl, work, rgb)[0]
        got = port.call(fn, a, tl, work, rgb)[0]
        if want != got:
            raise SystemExit(f'frame call {fn} {a} {rgb}: {describe_diff(want, got)}')
        check(f'frame call {fn} {a} {rgb}: state', emu.get(), port.get())
        runs += 1
    return f'{runs} frame, fog colour and point-light calls identical'


# ---------------------------------------------------------------- textures

class TexEmu:
    """0x57b130 with DirectDraw stubs: CreateSurface, QueryInterface, Lock, Load."""

    def __init__(self):
        cpu = self.cpu = Original()
        cpu.import_code(IAT_FTOL, FTOL)
        cpu.fpu_control(0x7f)
        self.log = []
        self.surface_mem = cpu.alloc(512 * 512 * 2 + 4096)
        dd, ddt = cpu.alloc(16), cpu.alloc(0x100)
        cpu.u32(dd, ddt)
        cpu.u32(0x809358, dd)
        self.obj = {}

        def method(table, off, fn, pop):
            addr = cpu.code(b'\xc3')
            cpu.u32(table + off, addr)
            cpu.stub_function(addr, fn, pop)
        self.surf_tab, self.tex_tab = cpu.alloc(0x100), cpu.alloc(0x100)
        method(ddt, 0x18, self.create_surface, 16)
        method(ddt, 0x14, lambda c: self.fail('CreatePalette'), 20)
        method(self.surf_tab, 0x00, self.query, 12)
        method(self.surf_tab, 0x08, lambda c: 0, 4)
        method(self.surf_tab, 0x64, self.lock, 20)
        method(self.surf_tab, 0x80, lambda c: 0, 8)
        method(self.surf_tab, 0x7c, lambda c: self.fail('SetPalette'), 8)
        method(self.tex_tab, 0x08, lambda c: 0, 4)
        method(self.tex_tab, 0x14, lambda c: 0, 8)
        self.heap = cpu.alloc(0x10000)
        self.heap_next = self.heap

        def calloc(c):
            a = self.heap_next
            self.heap_next += 0x20
            cpu.write(a, b'\0' * 0x18)
            return a
        cpu.stub_import(0x5bb60c, calloc)
        cpu.stub_function(ZERR, self.zerr)
        cpu.stub_function(D3DERR, lambda c: self.fail('d3d error'), 4)
        self.caps, self.dev = cpu.alloc(0x200), cpu.alloc(0xc00)
        cpu.u32(0x80c278, self.caps)
        cpu.u32(0x809644, self.dev)
        self.default = cpu.alloc(0x18)
        cpu.u32(0x80931c, self.default)
        cpu.u32(0x802718, 16)
        self.name = cpu.alloc(64)
        self.image = cpu.alloc(0x38)
        self.pixels = cpu.alloc(512 * 512 * 2)
        self.alpha = cpu.alloc(512 * 512)
        self.next_obj = cpu.alloc(0x400)

    def fail(self, what):
        raise SystemExit(f'texture: unexpected {what}')

    def zerr(self, c):
        fmt = c.read(c.arg(3), 256).split(b'\0')[0].decode('latin-1')
        self.log.append(('zerr', c.arg(0), c.arg(2), c_format(c, fmt, [c.arg(4), c.arg(5), c.arg(6)])))

    def new_obj(self, tab):
        a = self.next_obj
        self.next_obj += 16
        self.cpu.u32(a, tab)
        return a

    def create_surface(self, c):
        desc = c.arg(1)
        if not self.formats:  # the first: the system-memory surface the pixels go to
            self.formats.append(struct.unpack('<5I', c.read(desc + 0x48 + 0xc, 20)))
        s = self.new_obj(self.surf_tab)
        self.cpu.u32(c.arg(2), s)
        return 0

    def query(self, c):
        self.cpu.u32(c.arg(2), self.new_obj(self.tex_tab))
        return 0

    def lock(self, c):
        desc = c.arg(2)
        self.cpu.u32(desc + 0x10, self.pitch)
        self.cpu.u32(desc + 0x24, self.surface_mem)
        return 0

    def run(self, name, w, h, pixels, alpha, palette, alpha_arg, cu, cv, disp):
        cpu = self.cpu
        self.log, self.formats = [], []
        self.pitch = 2 * w + 2 * (w & 1) + 8 * (w % 3 == 0)
        cpu.write(self.surface_mem, b'\xee' * (self.pitch * max(h, 1)))
        cpu.write(self.name, name.encode() + b'\0')
        cpu.write(self.pixels, struct.pack(f'<{w * h}H', *pixels))
        if alpha:
            cpu.write(self.alpha, bytes(alpha))
        img = struct.pack('<ihhBBhhh', w * h, w, h, 0, 0, 0, 0, 0) + \
            struct.pack('<III', self.pixels, self.alpha if alpha else 0, 0x1234 if palette else 0)
        cpu.write(self.image, img + b'\0' * (0x38 - len(img)))
        cpu.write(self.caps + 0x148, struct.pack('<II', disp[0], disp[1]))
        cpu.u32(0x809448, disp[2])
        cpu.u32(self.dev + 0xb04, disp[3])
        cpu.u32(GBITS, disp[4])
        cpu.write(MASKS, struct.pack('<3I', *disp[5:8]))
        cpu.call(0x57b130, ecx=self.name, edx=self.image, stack_args=(alpha_arg, cu, cv))
        rec = cpu.uc.reg_read(UC_X86_REG_EAX)
        if rec == self.default:
            return self.log, None, None
        r = struct.unpack('<6I', cpu.read(rec, 0x18))
        # The surface as filled, decoded with the format CreateSurface was given.
        bits, rm, gm, bm, am = self.formats[0]
        rows = [cpu.read(self.surface_mem + self.pitch * y, 2 * w) for y in range(h)]
        px = array.array('H', b''.join(rows))
        rgba = bytearray()
        for p in px:
            rgba += bytes((expand(p, rm), expand(p, gm), expand(p, bm), expand(p, am) if am else 255))
        return self.log, (r[3], r[4], r[5]), bytes(rgba)


def expand(p, mask):
    shift = (mask & -mask).bit_length() - 1
    bits = bin(mask).count('1')
    v = (p & mask) >> shift
    r, filled = 0, 0
    while filled < 8:
        r = (r << bits) | v
        filled += bits
    return r >> (filled - 8)


def port_texture(name, w, h, pixels, alpha, palette, alpha_arg, cu, cv, disp):
    rec = (ctypes.c_int32 * 5)()
    rgba = ctypes.create_string_buffer(max(1, w * h * 4))
    probe.probe_dt_texture(name.encode(), w, h, (ctypes.c_uint16 * max(1, w * h))(*pixels),
                           (ctypes.c_uint8 * max(1, w * h))(*alpha) if alpha else None, palette, alpha_arg,
                           cu, cv, (ctypes.c_int32 * 8)(*disp), rec, rgba)
    log = port_log()
    if rec[0]:
        return log, None, None
    if not log or log[-1][0] != 'tex' or log[-1][1:] != (w, h, rec[1]):
        raise SystemExit(f'texture {name}: upload {log}')
    return log[:-1], (rec[1], rec[2], rec[3]), rgba.raw[:w * h * 4]


def test_textures(rng):
    emu = TexEmu()
    port = Port()
    port.setup([(0, 1, 1, 1)] * 16)
    runs, kinds = 0, {}
    for case in range(500):
        w = rng.choice([1, 2, 4, 8, 16, 32, 64, 128, 3, 5, 12, 24, 256])
        h = rng.choice([w, w, 1, 2, 4, 8, 16, 32, 64, 3, 6, 128])
        if rng.random() < 0.05:
            w = rng.choice([2048, 4096, 8192])
            h = 1
        n = w * h
        pixels = [rng.choice([0, 0xffff, rng.getrandbits(16), rng.getrandbits(16)]) for _ in range(min(n, 4096))]
        pixels = (pixels * (n // max(len(pixels), 1) + 1))[:n]
        alpha = [rng.choice([0, 255, 0x80, 0x7f, rng.getrandbits(8)]) for _ in range(n)] if rng.random() < 0.5 else None
        fmt = FORMATS[rng.choice([565, 565, 555])]
        disp = [rng.choice([256, 1024, 4096]), rng.choice([256, 1024, 4096]), rng.choice([0, 0, 2]),
                rng.randint(0, 1), fmt[6], fmt[0], fmt[1], fmt[2]]
        palette = int(rng.random() < 0.05)
        args = (f'tex{case}', w, h, pixels, alpha, palette, rng.choice([0, 1]), rng.randint(0, 1),
                rng.randint(0, 1), disp)
        if w * h > 70000:
            continue
        want = emu.run(*args)
        got = port_texture(*args)
        if want[:2] != got[:2]:
            raise SystemExit(f'texture case {case} {args[1:3]} alpha {args[6]} plane {bool(alpha)} disp {disp}: '
                             f'{want[:2]} vs {got[:2]}')
        if want[2] != got[2]:
            k = next(i for i in range(0, len(want[2]), 4) if want[2][i:i + 4] != got[2][i:i + 4])
            raise SystemExit(f'texture case {case}: pixel {k // 4} {want[2][k:k + 4].hex()} vs {got[2][k:k + 4].hex()}'
                             f' (source {pixels[k // 4]:#06x}, alpha {alpha[k // 4] if alpha else None})')
        kind = 'default' if want[1] is None else f'type {want[1][0]}'
        kinds[kind] = kinds.get(kind, 0) + 1
        runs += 1
    return f'{runs} texture records identical ({kinds}), surfaces decoded to the same RGBA'


def main():
    rng = random.Random(5)
    emu, port = Emu(), Port()
    print(test_draws(rng, emu, port))
    print(test_flush(rng, emu, port))
    print(test_frame(rng, emu, port))
    print(test_textures(rng))
    print('ok: draw table, flushes, frame calls and texture records identical to the original (24-bit FPU)')


if __name__ == '__main__':
    main()
