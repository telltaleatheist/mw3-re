"""zTwoD's HUD primitives' draws against the original x86 (docs/twod.md, Drawing).

    cmake --build build && .venv/bin/python -P tests/differential/test_twod_prims.py [runs]

Each run builds one object with the original's own constructor in the
emulator (Line2D 0x005a67e0, ClipLine2D 0x005a6ae0, Frame 0x005a39f0,
FilledBox 0x005a3b60, Polygon 0x005a6bd0, PolyLine 0x005a3830), sets its
fields at random (place, end, colour, clip rects and flags, dash, noise,
points, the round clip), and calls its draw (vtable +4: 0x005a68b0,
0x005a6b30, 0x005a3b00, 0x005a3bc0, 0x005a6ca0, 0x005a38f0) with zRender's
2D calls stubbed and logged; the port draws the same fields through a
logging host (tests/probe_twod.cpp). Compared: the calls with their
arguments (a round PolyLine's clipped ends, the HUD line's float clip, the
polygon's winding) and the line clip box after (0x005f4800, 0x007319a0).
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import load_probe  # noqa: E402
from original import Original  # noqa: E402
from ai_world import FTOL, IAT_FTOL  # noqa: E402

KINDS = {0: ('Line2D', 0x5a67e0, 5), 1: ('ClipLine2D', 0x5a6ae0, 5), 2: ('Frame', 0x5a39f0, 0),
         3: ('FilledBox', 0x5a3b60, 5), 4: ('Polygon', 0x5a6bd0, 0), 5: ('PolyLine', 0x5a3830, 0)}
probe = load_probe()
probe.probe_twod_prim.argtypes = [ctypes.c_int32, ctypes.c_char_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_char_p,
                                  ctypes.c_int]


def r32(cpu, a):
    return struct.unpack('<I', cpu.read(a, 4))[0]


def f32(v):
    return struct.unpack('<I', struct.pack('<f', v))[0]


def i32(v):
    return v & 0xffffffff


class Log:
    def __init__(self):
        self.lines = []

    def add(self, k, vals):
        self.lines.append(k + ''.join(f' {v & 0xffffffff:x}' for v in vals))


def stubs(cpu, log, saved):
    def rect(p):
        return struct.unpack('<4i', bytes(cpu.read(p, 16)))

    def line(c):
        log.add('line', [c.ecx, c.edx, c.arg(0), c.arg(1), c.arg(2)])
        return 0

    def dashed(c):
        log.add('dashed', [c.ecx, c.edx, c.arg(0), c.arg(1), c.arg(2), c.arg(3)])
        return 0

    def lst(c):
        n = c.edx
        log.add('list', [n, *rect(c.arg(0)), c.arg(1)])
        pts = struct.unpack(f'<{2 * (n + 1)}i', bytes(cpu.read(c.ecx, 8 * (n + 1))))
        for i in range(n + 1):
            log.add(' pt', [pts[2 * i], pts[2 * i + 1]])
        return 0

    def circle(c):
        log.add('circle', [c.ecx, c.edx, c.arg(0), c.arg(1)])
        if c.arg(2):
            log.add(' clip', rect(c.arg(2)))
        return 0

    def noisy_circle(c):
        log.add('noisy circle', [c.ecx, c.edx, c.arg(0), c.arg(1), c.arg(2)])
        if c.arg(3):
            log.add(' clip', rect(c.arg(3)))
        return 0

    def noisy_line(c):
        log.add('noisy line', [c.ecx, c.edx, c.arg(0), c.arg(1), c.arg(2), c.arg(3)])
        if c.arg(4):
            log.add(' clip', rect(c.arg(4)))
        return 0

    def polygon(c):
        n = c.edx
        log.add('polygon', [n, c.arg(0), r32(cpu, 0x74a3c8)])
        for i in range(n):
            log.add(' v', [r32(cpu, c.ecx + 12 * i), r32(cpu, c.ecx + 12 * i + 4)])
        return 0

    def save_clip(c):  # 0x0053ce20: the render's clip volume
        cpu.write(c.ecx, struct.pack('<3f', *saved[:3]))
        cpu.write(c.edx, struct.pack('<3f', *saved[3:]))
        return 0
    for addr, fn, pop in ((0x562940, line, 0xc), (0x562c80, dashed, 0x10), (0x562cb0, lst, 8),
                          (0x5630c0, circle, 0xc), (0x563440, noisy_circle, 0x10), (0x562970, noisy_line, 0x14),
                          (0x55d010, polygon, 4), (0x53ce20, save_clip, 0)):
        cpu.stub_function(addr, fn, pop)


def case(r):
    kind = r.choice(list(KINDS))
    raw = bytearray(0x210)
    w = lambda off, v: struct.pack_into('<I', raw, off, v & 0xffffffff)  # noqa: E731
    span = lambda: r.randrange(-50, 700)  # noqa: E731
    w(0x14, span())
    w(0x18, span())
    rect = [r.randrange(0, 300), r.randrange(0, 300)]
    rect += [rect[0] + r.randrange(0, 300), rect[1] + r.randrange(0, 300)]
    struct.pack_into('<4i', raw, 0x200, *rect)
    c = {'kind': kind, 'raw': raw, 'rect': rect,
         'saved': [r.uniform(-1, 50), r.uniform(-1, 50), r.uniform(0.1, 2), r.uniform(100, 700), r.uniform(100, 500),
                   r.uniform(100, 900)],
         'clip': [r.uniform(0, 100), r.uniform(0, 100), 0.0, r.uniform(100, 640), r.uniform(100, 480), 0.0]}
    if kind <= 1:
        w(0x34, span())
        w(0x38, span())
        w(0x3c, r.randrange(1 << 32))
        clipped = r.random() < 0.6
        w(0x40, clipped)
        w(0x44, 1 if clipped else r.choice([0, 1]))  # a clipped line always has its rect here
        w(0x48, r.choice([0, 1, 2, 3, 5]))
        struct.pack_into('<f', raw, 0x4c, r.choice([0.0, 0.0, 0.1, 0.5]))
    elif kind <= 3:
        w(0x34, r.choice([0, 1, 5, 40, 200]))
        w(0x3c, r.randrange(1 << 32))
        raw[0x40] = r.choice([0, 1])
        struct.pack_into('<4i', raw, 0x44, *rect)
        struct.pack_into('<f', raw, 0x54, r.choice([0.0, 0.0, 0.1, 1.0]))
    elif kind == 4:
        n = r.randrange(0, 22)
        for i in range(21):
            struct.pack_into('<3f', raw, 0x34 + 12 * i, float(span()), float(span()), r.uniform(-1, 1))
        if r.random() < 0.2 and n >= 3:  # a degenerate first three
            struct.pack_into('<3f', raw, 0x40, *struct.unpack_from('<3f', raw, 0x34))
        w(0x130, n)
        w(0x134, r.randrange(1 << 32))
    else:
        n = r.randrange(0, 22)
        for i in range(21):
            struct.pack_into('<2i', raw, 0x34 + 8 * i, span(), span())
        w(0xdc, n)
        w(0xe0, r.randrange(1 << 32))
        has = r.random() < 0.7
        w(0xe4, 1 if has else 0)
        raw[0xe8] = 1 if has and r.random() < 0.6 else 0
    return c


LOG = Log()
SAVED = [0.0] * 6


def original(cpu, c):
    log = LOG
    log.lines = []
    SAVED[:] = c['saved']
    cpu.heap_next = 0x10000000
    name, ctor, nargs = KINDS[c['kind']]
    obj = cpu.alloc(0x200)
    cpu.write(obj, bytes(0x200))
    cpu.call(ctor, ecx=obj, stack_args=(0,) * nargs)
    rp = cpu.alloc(16)
    cpu.write(rp, struct.pack('<4i', *c['rect']))
    raw = c['raw']
    keep = {0: 0x4c, 1: 0x50, 2: 0x54, 3: 0x58, 4: 0x138, 5: 0xec}[c['kind']]
    body = bytearray(raw[0x14:keep])
    cpu.write(obj + 0x14, bytes(body[:4 * 2]))
    cpu.write(obj + 0x34, bytes(raw[0x34:keep]))
    if c['kind'] <= 1 and struct.unpack_from('<I', raw, 0x44)[0]:
        cpu.u32(obj + 0x44, rp)
    if c['kind'] == 5 and struct.unpack_from('<I', raw, 0xe4)[0]:
        cpu.u32(obj + 0xe4, rp)
    cpu.write(0x5f4800, struct.pack('<3f', *c['clip'][:3]))
    cpu.write(0x5f4810, struct.pack('<3f', *c['clip'][3:]))
    draw = r32(cpu, r32(cpu, obj) + 4)
    cpu.call(draw, ecx=obj)
    lc = [r32(cpu, 0x5f4800 + 4 * i) for i in range(3)] + [r32(cpu, 0x5f4810 + 4 * i) for i in range(3)]
    lc += [r32(cpu, 0x7319a0 + 4 * i) for i in range(4)]
    log.add('lineclip', lc)
    return '\n'.join(log.lines) + '\n'


def port(c):
    out = ctypes.create_string_buffer(1 << 16)
    saved = (ctypes.c_float * 6)(*c['saved'])
    clip = (ctypes.c_float * 6)(*c['clip'])
    n = probe.probe_twod_prim(c['kind'], bytes(c['raw']), saved, clip, out, len(out))
    assert n >= 0
    return out.value.decode()


def main():
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 2000
    cpu = Original()
    cpu.fpu_control(0x007f)  # 24-bit, as in play
    cpu.import_code(IAT_FTOL, FTOL)
    stubs(cpu, LOG, SAVED)
    bad, kinds, calls = 0, {}, 0
    for i in range(runs):
        c = case(random.Random(i))
        a, b = original(cpu, c), port(c)
        kinds[KINDS[c['kind']][0]] = kinds.get(KINDS[c['kind']][0], 0) + 1
        calls += a.count('\n')
        if a != b:
            bad += 1
            if bad <= 4:
                la, lb = a.split('\n'), b.split('\n')
                k = next(j for j in range(max(len(la), len(lb))) if j >= len(la) or j >= len(lb) or la[j] != lb[j])
                print(f'run {i} ({KINDS[c["kind"]][0]}): line {k}: original {la[k] if k < len(la) else "-"!r}, '
                      f'port {lb[k] if k < len(lb) else "-"!r}')
    print(f'zTwoD HUD primitives: {runs - bad}/{runs} draws identical ({calls} lines; '
          f'{", ".join(f"{k} {v}" for k, v in sorted(kinds.items()))})')
    sys.stdout.flush()
    return 1 if bad else 0


if __name__ == '__main__':
    import os
    os._exit(main())
