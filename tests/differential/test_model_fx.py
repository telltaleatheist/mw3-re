"""The models' texture scroll and the object3d colour blend against the
original x86 (docs/render.md section 3): 0x526ae0 (the scroll's set), 0x541ad0
(its step, the UVs moved), 0x4fb8c0 (the blend's colour and weight) and
0x4fb840 (its switch). The x87 at 24 bits, as in play.

    cmake --build build && .venv/bin/python tests/differential/test_model_fx.py [cases]

The step: models of 0 to 6 polygons, each textured or not, of 0 to 8
vertices whose UVs lie in random ranges (some across whole numbers, some
negative, large, or NaN), speeds 0, NaN or random on either axis, dt random,
the frame the model was last stepped the same or not; compared: the result,
the header and every UV bit for bit. The set: on or off, any speeds, a
header's other bits kept; none. The blend: no node, no data, another class,
an object3d with random data; the switch on or off, the colour and weight
(in range, beyond, negative, NaN) or no colour; compared: the result and the
data's 0x90 bytes.
"""
import ctypes
import math
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import Emu, Trace, load_probe  # noqa: E402

U8P = ctypes.POINTER(ctypes.c_uint8)
I32 = ctypes.c_int32
FRAME, DT = 0x72f338, 0x72c52c
COUNT = {'steps': 0, 'polygons moved': 0, 'wraps': 0, 'sets': 0, 'blends': 0}


def die():
    sys.stdout.flush()
    os._exit(1)


def f2u(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


def u2f(u):
    return struct.unpack('<f', struct.pack('<I', u))[0]


def setup_probe(p):
    p.probe_mfx_tick.argtypes = [U8P, I32, ctypes.POINTER(I32), ctypes.POINTER(I32), ctypes.POINTER(ctypes.c_float),
                                 ctypes.c_uint32, ctypes.c_float, I32]
    p.probe_mfx_tick.restype = I32
    p.probe_mfx_set_scroll.argtypes = [U8P, I32, ctypes.c_float, ctypes.c_float, I32]
    p.probe_mfx_set_scroll.restype = I32
    p.probe_mfx_blend.argtypes = [I32, I32, U8P, I32, I32, ctypes.POINTER(ctypes.c_float), ctypes.c_float]
    p.probe_mfx_blend.restype = I32


def speed(r):
    return r.choice([0.0, 0.0, -0.0, float('nan'), r.uniform(-3, 3), r.uniform(-40, 40), 10.0, 0.015])


def uv_value(r, base):
    k = r.random()
    if k < 0.03:
        return float('nan')
    if k < 0.1:
        return r.uniform(-1e5, 1e5)
    return base + r.uniform(-0.8, 0.8)


def emu_new():
    t = Trace()
    e = Emu(t)
    e.cpu.fpu_control(0x007f)
    e.heap_stubs()
    e.stub(0x49bfd0, '@zerr', '')  # the report (empty in the release)
    return e


def same_bits(a, b):
    return a == b


def run_tick(probe, e, r, seed):
    n = r.randrange(0, 7)
    counts = [r.choice([0, 1, 3, 3, 4, 4, 5, 8]) for _ in range(n)]
    textured = [1 if r.random() < 0.8 else 0 for _ in range(n)]
    uvs = []
    for c in counts:
        bu, bv = r.uniform(-3, 3), r.uniform(-3, 3)
        if r.random() < 0.3:
            bu, bv = float(r.randrange(-3, 4)), float(r.randrange(-3, 4))  # across a whole number
        for _ in range(max(c, 1)):
            uvs += [uv_value(r, bu), uv_value(r, bv)]
    header = bytearray(r.randbytes(0x5c))
    su, sv = speed(r), speed(r)
    header[0x28:0x2c] = struct.pack('<f', su)
    header[0x2c:0x30] = struct.pack('<f', sv)
    header[0x10:0x14] = struct.pack('<I', n)
    frame = r.randrange(1, 1000)
    header[0x30:0x34] = struct.pack('<I', frame if r.random() < 0.15 else frame - 1)
    dt = r.choice([1 / 30, 1 / 60, r.uniform(0, 0.125), 0.0])
    null = 1 if r.random() < 0.02 else 0
    # The original's model.
    m = e.world_alloc(0x60)
    polys = e.world_alloc(0x24 * max(n, 1))
    header[0x34:0x38] = struct.pack('<I', polys)  # the polygons (the port's own field)
    e.cpu.write(m, bytes(header))
    m_uv = e.world_alloc(4 * max(len(uvs), 2))
    e.cpu.write(m_uv, struct.pack(f'<{len(uvs)}f', *uvs))
    at = m_uv
    for i, c in enumerate(counts):
        p = polys + 0x24 * i
        mat = e.world_alloc(0x2c)
        e.cpu.write(mat + 1, bytes([textured[i] | (r.randrange(0, 0x80) & ~1)]))
        e.w32(p, (r.randrange(0, 0x100) << 8) | c)
        e.w32(p + 0x10, at)
        e.w32(p + 0x1c, mat)
        at += 8 * max(c, 1)
    e.w32(FRAME, frame)
    e.w32(DT, f2u(dt))
    ra = e.call(0x541ad0, ecx=0 if null else m) & 0xffffffff
    ha = bytes(e.cpu.read(m, 0x5c))
    ua = list(struct.unpack(f'<{len(uvs)}I', bytes(e.cpu.read(m_uv, 4 * len(uvs))))) if uvs else []
    # The port's.
    hb = (ctypes.c_uint8 * 0x5c)(*header)
    # the polygon flags' upper bits do not matter to the port (count & 0xff)
    cb = (I32 * max(n, 1))(*counts)
    tb = (I32 * max(n, 1))(*textured)
    ub = (ctypes.c_float * max(len(uvs), 2))(*uvs)
    rb = probe.probe_mfx_tick(hb, n, cb, tb, ub, frame, dt, null) & 0xffffffff
    ubits = [f2u(ub[i]) for i in range(len(uvs))]
    name = f'tick seed {seed} (n {n}, speeds {su}, {sv}, dt {dt:.4f}, frame {"same" if header[0x30:0x34] == struct.pack("<I", frame) else "new"})'
    if ra != rb or ha != bytes(hb):
        print(f'{name}: result {ra:#x} / {rb:#x}, header {"same" if ha == bytes(hb) else "differs"}')
        die()
    if ua != ubits:
        k = next(i for i in range(len(ua)) if ua[i] != ubits[i])
        print(f'{name}: uv {k} original {u2f(ua[k])!r} ({ua[k]:#x}) rebuild {u2f(ubits[k])!r} ({ubits[k]:#x})')
        die()
    COUNT['steps'] += 1
    moved = 0
    at = 0
    for c in counts:
        span = 2 * max(c, 1)
        before = [f2u(x) for x in uvs[at:at + span]]
        if before != ua[at:at + span]:
            moved += 1
            if any(abs(u2f(a) - u2f(b)) > 0.5 for a, b in zip(ua[at:at + span], before)
                   if not math.isnan(u2f(a)) and not math.isnan(u2f(b))):
                COUNT['wraps'] += 1
        at += span
    COUNT['polygons moved'] += moved


def run_set(probe, e, r, seed):
    header = bytearray(r.randbytes(0x5c))
    on = r.choice([0, 1, 2, 3, -1])
    u, v = speed(r), speed(r)
    null = 1 if r.random() < 0.05 else 0
    m = e.world_alloc(0x60)
    e.cpu.write(m, bytes(header))
    ra = e.call(0x526ae0, ecx=0 if null else m, edx=on & 0xffffffff, stack=(f2u(u), f2u(v))) & 0xffffffff
    ha = bytes(e.cpu.read(m, 0x5c))
    hb = (ctypes.c_uint8 * 0x5c)(*header)
    rb = probe.probe_mfx_set_scroll(hb, on, u, v, null) & 0xffffffff
    if ra != rb or ha != bytes(hb):
        print(f'set seed {seed} (on {on}, {u}, {v}, null {null}): result {ra} / {rb}, header differs: {ha != bytes(hb)}')
        die()
    COUNT['sets'] += 1


def blend_value(r):
    return r.choice([r.uniform(0, 1), r.uniform(-2, 3), 1.0, 0.0, -0.0, float('nan'), 1e9, -1e9])


def run_blend(probe, e, r, seed):
    what = r.choice([0, 1, 2, 2, 2, 2, 2])
    cls = 5 if r.random() < 0.85 else r.choice([0, 2, 6, 14])
    data = bytearray(r.randbytes(0x90))
    op = r.randrange(2)
    on = r.choice([0, 1, 7, -1])
    rgb = None if r.random() < 0.2 else [blend_value(r) for _ in range(3)]
    w = blend_value(r)
    node = e.world_alloc(0xd4)
    d = e.world_alloc(0x90)
    e.cpu.write(d, bytes(data))
    e.w32(node + 0x34, cls)
    e.w32(node + 0x38, 0 if what == 1 else d)
    ecx = 0 if what == 0 else node
    if op == 0:
        ra = e.call(0x4fb840, ecx=ecx, edx=on & 0xffffffff) & 0xffffffff
    else:
        p = 0
        if rgb is not None:
            p = e.world_alloc(12)
            e.cpu.write(p, struct.pack('<3f', *rgb))
        ra = e.call(0x4fb8c0, ecx=ecx, edx=p, stack=(f2u(w),)) & 0xffffffff
    da = bytes(e.cpu.read(d, 0x90))
    db = (ctypes.c_uint8 * 0x90)(*data)
    rp = (ctypes.c_float * 3)(*rgb) if rgb is not None else None
    rb = probe.probe_mfx_blend(what, cls, db, op, on, rp, w) & 0xffffffff
    if ra != rb or da != bytes(db):
        k = next((i for i in range(0x90) if da[i] != db[i]), -1)
        print(f'blend seed {seed} (what {what}, class {cls}, op {op}, on {on}, rgb {rgb}, w {w}): result {ra} / {rb}, '
              f'data differs at {k:#x}')
        die()
    COUNT['blends'] += 1


def main():
    probe = load_probe()
    setup_probe(probe)
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    e = emu_new()
    for seed in range(count):
        r = random.Random(seed)
        if seed % 200 == 0:
            e = emu_new()  # the world area is a bump allocator
        run_tick(probe, e, r, seed)
        run_set(probe, e, r, seed)
        run_blend(probe, e, r, seed)
    print(f'ok: {count} cases ({", ".join(f"{v} {k}" for k, v in COUNT.items())}), as the original')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()
