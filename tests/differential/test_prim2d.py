"""zRender's 2D primitives against the original x86 (docs/twod.md, Drawing).

    cmake --build build && .venv/bin/python -P tests/differential/test_prim2d.py [runs]

The HUD's drawing on the locked 16-bit target: the dashed line 0x00562c80
(0x00563c50), the clipped line 0x00563db0 and the line list 0x00562cb0, the
pixel 0x005630a0, the circle 0x005630c0, the jittered circle 0x00563440 and
the noisy line 0x00562970 (both on MSVCRT's rand, the same seed on both
sides; the x87 at 24 bits, as in play). Random targets (odd widths, padded
pitches), shapes, colours, dashes, noise and clip rects. The targets must be
identical. Unclipped shapes stay on the target, and clip rects 4 pixels
inside it: the original writes off the target into the neighbouring rows
(FIX (bug), the port skips those pixels), and its clip can overshoot the
rect by a pixel or two, which the port keeps.
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

TARGET = (0x8007c8, 0x8007cc, 0x8007d0, 0x8007d4)
IAT_RAND = 0x5bb668
OPS = {0: (0x562c80, 'dashed'), 1: (0x563db0, 'clipped'), 2: (0x562cb0, 'list'), 3: (0x5630a0, 'pixel'),
       4: (0x5630c0, 'circle'), 5: (0x563440, 'noisy circle'), 6: (0x562970, 'noisy line')}
probe = load_probe()
probe.probe_prim2d.argtypes = [ctypes.c_int32, ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32,
                               ctypes.c_void_p, ctypes.c_float, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int32,
                               ctypes.c_uint32]


class MsvcRand:
    def __init__(self, seed):
        self.state = seed

    def __call__(self, cpu=None):
        self.state = (self.state * 214013 + 2531011) & 0xffffffff
        return (self.state >> 16) & 0x7fff


RAND = MsvcRand(0)


def f32(v):
    return struct.unpack('<I', struct.pack('<f', v))[0]


def case(r):
    op = r.choice(list(OPS))
    tw, th = r.randrange(24, 160), r.randrange(24, 120)
    pitch = 2 * tw + 2 * r.choice([0, 0, 1, 3])
    c = {'op': op, 'tw': tw, 'th': th, 'pitch': pitch, 'seed': r.randrange(1 << 32), 'noise': 0.0, 'clip': None,
         'points': [], 'a': [0] * 6, 'target': bytes(r.randrange(256) for _ in range(pitch * th))}
    colour = r.randrange(0x10000)
    clip = None
    if op in (1, 2) or (op >= 4 and r.random() < 0.5):
        # Inside the target by 4: the original's clip can overshoot its rect
        # by a pixel or two (the move is truncated), which the port draws
        # too, but past the target's edge it writes into the next row.
        l, t = r.randrange(4, tw - 4), r.randrange(4, th - 4)
        clip = [l, t, r.randrange(l, tw - 4), r.randrange(t, th - 4)]
    c['clip'] = clip
    if op == 0:
        x0, y0, x1, y1 = r.randrange(tw), r.randrange(th), r.randrange(tw), r.randrange(th)
        c['a'] = [x0, y0, x1, y1, colour, r.choice([2, 3, 4, 7, 10])]
    elif op == 1:
        span = lambda n: r.randrange(-n, 2 * n)  # noqa: E731
        c['a'] = [span(tw), span(th), span(tw), span(th), colour, 0]
    elif op == 2:
        n = r.randrange(1, 7)
        c['points'] = [r.randrange(-tw // 2, tw + tw // 2) if i % 2 == 0 else r.randrange(-th // 2, th + th // 2)
                       for i in range(2 * (n + 1))]
        c['a'] = [0, 0, 0, 0, colour, 0]
    elif op == 3:
        c['a'] = [r.randrange(tw), r.randrange(th), 0, 0, colour, 0]
    elif op in (4, 5):
        rad = r.randrange(1, max(2, min(tw, th) // 2 - 4))
        c['noise'] = r.choice([0.0, 0.1, 0.5, 1.0]) if op == 5 else 0.0
        m = rad + int(rad * c['noise'] * 0.1) + 2
        if clip is None and (tw <= 2 * m or th <= 2 * m):
            rad, m = 1, 3
        cx = r.randrange(m, max(m + 1, tw - m)) if clip is None else r.randrange(-10, tw + 10)
        cy = r.randrange(m, max(m + 1, th - m)) if clip is None else r.randrange(-10, th + 10)
        c['a'] = [cx, cy, rad, 0, colour, 0]
    else:
        c['noise'] = r.choice([0.0, 0.05, 0.2])
        x0, y0, x1, y1 = r.randrange(tw), r.randrange(th), r.randrange(tw), r.randrange(th)
        if clip is None:  # the throw stays on the target
            ln = ((x1 - x0) ** 2 + (y1 - y0) ** 2) ** 0.5
            m = int(ln * c['noise'] * 0.1 * 1.1) + 2
            x0, x1 = (min(max(v, m), tw - 1 - m) for v in (x0, x1))
            y0, y1 = (min(max(v, m), th - 1 - m) for v in (y0, y1))
            if tw <= 2 * m + 1 or th <= 2 * m + 1:
                c['noise'] = 0.0
                x0, y0, x1, y1 = 1, 1, tw - 2, th - 2
        c['a'] = [x0, y0, x1, y1, colour, 0]
    return c


def original(cpu, c):
    cpu.heap_next = 0x10000000
    tgt = cpu.alloc(len(c['target']) + 64)
    cpu.write(tgt, c['target'])
    for a, v in zip(TARGET, (tgt, c['tw'], c['th'], c['pitch'])):
        cpu.u32(a, v)
    cpu.u32(0x8008c4, 0x563c50)
    cpu.u32(0x8008c8, 0x563db0)
    RAND.state = c['seed']
    rp = 0
    if c['clip']:
        rp = cpu.alloc(16)
        cpu.write(rp, struct.pack('<4i', *c['clip']))
    a = [v & 0xffffffff for v in c['a']]
    op, colour = c['op'], a[4]
    if op == 0:
        cpu.call(0x562c80, ecx=a[0], edx=a[1], stack_args=(a[2], a[3], colour, a[5]))
    elif op == 1:
        cpu.call(0x563db0, ecx=tgt, edx=rp, stack_args=(a[0], a[1], a[2], a[3], colour))
    elif op == 2:
        pp = cpu.alloc(4 * len(c['points']) + 16)
        cpu.write(pp, struct.pack(f'<{len(c["points"])}i', *c['points']))
        cpu.call(0x562cb0, ecx=pp, edx=len(c['points']) // 2 - 1, stack_args=(rp, colour))
    elif op == 3:
        cpu.call(0x5630a0, ecx=a[0], edx=a[1], stack_args=(colour,))
    elif op == 4:
        cpu.call(0x5630c0, ecx=a[0], edx=a[1], stack_args=(a[2], colour, rp))
    elif op == 5:
        cpu.call(0x563440, ecx=a[0], edx=a[1], stack_args=(a[2], colour, f32(c['noise']), rp))
    else:
        cpu.call(0x562970, ecx=a[0], edx=a[1], stack_args=(a[2], a[3], colour, f32(c['noise']), rp))
    return cpu.read(tgt, len(c['target']))


def port(c):
    tgt = ctypes.create_string_buffer(c['target'], len(c['target']))
    a = (ctypes.c_int32 * 6)(*c['a'])
    clip = (ctypes.c_int32 * 4)(*c['clip']) if c['clip'] else None
    pts = (ctypes.c_int32 * max(1, len(c['points'])))(*c['points']) if c['points'] else None
    probe.probe_prim2d(c['op'], tgt, c['tw'], c['th'], c['pitch'], a, c['noise'], clip, pts,
                       len(c['points']) // 2 - 1 if c['points'] else 0, c['seed'])
    return tgt.raw


def main():
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    cpu = Original()
    cpu.fpu_control(0x007f)  # 24-bit, as in play
    cpu.stub_import(IAT_RAND, RAND)
    cpu.import_code(IAT_FTOL, FTOL)
    bad, kinds, drew = 0, {}, 0
    for i in range(runs):
        c = case(random.Random(i))
        a, b = original(cpu, c), port(c)
        kinds[OPS[c['op']][1]] = kinds.get(OPS[c['op']][1], 0) + 1
        drew += a != c['target']
        if a != b:
            bad += 1
            if bad <= 5:
                n = next(j for j in range(len(a)) if a[j] != b[j])
                print(f'run {i} ({OPS[c["op"]][1]}): differ at row {n // c["pitch"]}, x {(n % c["pitch"]) // 2}; '
                      f'{ {k: v for k, v in c.items() if k != "target"} }')
    print(f'zRender 2D primitives: {runs - bad}/{runs} targets identical ({drew} drew something; '
          f'{", ".join(f"{k} {v}" for k, v in sorted(kinds.items()))})')
    sys.stdout.flush()
    return 1 if bad else 0


if __name__ == '__main__':
    import os
    os._exit(main())
