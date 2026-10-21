"""zRender's ripple, 0x00558dd0 (docs/render-ripple.md), against the port.

Random 16-bit screens and scratch buffers, centres inside, near and past the
edges, radii, amplitudes, wavelengths, phases and clip rectangles; the x87 at
24 bits (Direct3D 6's setting) and at 53 (the CRT's, as Windows 11 runs it).
Compared: the screen and the scratch ([0x0072c1e0]) after each call, pixel for
pixel.
"""
import ctypes
import math
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import load_probe  # noqa: E402
from original import FTOL, IAT_FTOL, Original  # noqa: E402

RIPPLE = 0x558dd0
SCRATCH, SCREEN, WIDTH, HEIGHT, PITCH_PX = 0x72c1e0, 0x72c1e4, 0x72c1e8, 0x72c1ec, 0x72c1f4

probe = load_probe()
probe.probe_ripple.argtypes = [ctypes.c_void_p, ctypes.c_int32, ctypes.c_int32, ctypes.c_int32, ctypes.c_void_p,
                               ctypes.c_int32, ctypes.c_int32, ctypes.c_int32, ctypes.c_int32, ctypes.c_int32,
                               ctypes.c_float, ctypes.c_float, ctypes.c_void_p, ctypes.c_int32]
probe.probe_ripple.restype = None


def bits(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


def f32(f):
    return struct.unpack('<f', struct.pack('<f', f))[0]


def case(rnd):
    if rnd.random() < 0.35:  # as the main menu's buttons ripple (mainmenu.md): 12-pixel waves, 3 of them
        w, h = rnd.choice(((640, 480), (320, 240), (400, 300)))
        pitch = w
        outer = rnd.randint(20, 200)
        inner = outer - 36
        amp = rnd.randint(2, 7)
        wl = 12.0
        phase = f32(math.pi * (outer - 20) / 12.0)
        x, y = rnd.randint(-20, w + 20), rnd.randint(-20, h + 20)
        if rnd.random() < 0.5:
            x, y = rnd.randint(outer + amp + 1, max(outer + amp + 1, w - outer - amp - 2)), \
                rnd.randint(outer + amp + 1, max(outer + amp + 1, h - outer - amp - 2))
    else:
        w = rnd.randint(40, 160)
        h = rnd.randint(40, 120)
        pitch = w + rnd.choice((0, 0, 3, 16))
        outer = rnd.choice((0, 1, 2) + (rnd.randint(3, 40),) * 7)
        inner = rnd.choice((rnd.randint(-3, outer + 2), rnd.randint(0, max(0, outer)), rnd.randint(0, max(0, outer)), 0))
        amp = rnd.choice((0, 1) + (rnd.randint(2, 10),) * 4)
        wl = f32(rnd.choice((1.0, 2.5, rnd.uniform(0.3, 12.0))))
        phase = f32(rnd.uniform(-7.0, 20.0))
        if rnd.random() < 0.5:  # well inside
            x = rnd.randint(outer + amp + 1, max(outer + amp + 1, w - outer - amp - 2))
            y = rnd.randint(outer + amp + 1, max(outer + amp + 1, h - outer - amp - 2))
        else:
            x = rnd.randint(-outer - amp - 4, w + outer + amp + 4)
            y = rnd.randint(-outer - amp - 4, h + outer + amp + 4)
    clip = None
    if rnd.random() < 0.4:
        l = rnd.randint(0, w // 2)
        t = rnd.randint(0, h // 2)
        clip = (l, t, rnd.randint(l, w - 1), rnd.randint(t, h - 1))
    screen = [rnd.randrange(0x10000) for _ in range(pitch * h)]
    scratch = [rnd.randrange(0x10000) for _ in range(w * h)]
    return w, h, pitch, x, y, inner, outer, amp, wl, phase, clip, screen, scratch


def run_original(o, c):
    w, h, pitch, x, y, inner, outer, amp, wl, phase, clip, screen, scratch = c
    scr = o.alloc(2 * pitch * h)
    scx = o.alloc(2 * w * h)
    o.write(scr, struct.pack(f'<{pitch * h}H', *screen))
    o.write(scx, struct.pack(f'<{w * h}H', *scratch))
    for addr, v in ((SCRATCH, scx), (SCREEN, scr), (WIDTH, w), (HEIGHT, h), (PITCH_PX, pitch)):
        o.u32(addr, v)
    cp = 0
    if clip:
        cp = o.alloc(16)
        o.write(cp, struct.pack('<4i', *clip))
    o.call(RIPPLE, ecx=x, edx=y, stack_args=(inner & 0xffffffff, outer & 0xffffffff, amp, bits(wl), bits(phase), cp))
    out = (o.read(scr, 2 * pitch * h), o.read(scx, 2 * w * h))
    o.heap_next = scr  # the buffers are freed
    return out


def run_port(c, precision):
    w, h, pitch, x, y, inner, outer, amp, wl, phase, clip, screen, scratch = c
    scr = (ctypes.c_uint16 * (pitch * h))(*screen)
    scx = (ctypes.c_uint16 * (w * h))(*scratch)
    cp = (ctypes.c_int32 * 4)(*clip) if clip else None
    probe.probe_ripple(scr, w, h, pitch, scx, x, y, inner, outer, amp, wl, phase, cp, precision)
    return bytes(scr), bytes(scx)


def main():
    n = int(os.environ.get('RIPPLE_CASES', '300'))
    failed = 0
    reached = {'inside': 0, 'clipped': 0, 'nothing': 0, 'moved': 0}
    for precision, cw in ((24, 0x007f), (53, 0x027f)):
        o = Original()
        o.import_code(IAT_FTOL, FTOL)
        o.fpu_control(cw)
        rnd = random.Random(0x558dd0 + precision)
        for k in range(n):
            c = case(rnd)
            want = run_original(o, c)
            got = run_port(c, precision)
            if want != got:
                failed += 1
                if failed <= 5:
                    w, h, pitch, x, y, inner, outer, amp, wl, phase, clip = c[:11]
                    which = 'screen' if want[0] != got[0] else 'scratch'
                    print(f'FAIL x87 {precision} case {k}: {which} differs: {w}x{h} pitch {pitch} at ({x}, {y}) '
                          f'radii {inner}..{outer} amp {amp} wl {wl} phase {phase} clip {clip}')
            if precision == 24:
                screen0 = struct.pack(f'<{len(c[11])}H', *c[11])
                w, h, pitch, x, y, inner, outer, amp = c[:8]
                cl = c[10] or (0, 0, w - 1, h - 1)
                inn = min(max(inner, 0), max(outer, 0))
                if max(outer, 0) == inn or x - outer - amp > cl[2] or x + outer + amp < cl[0] or \
                        y - outer - amp > cl[3] or y + outer + amp < cl[1]:
                    reached['nothing'] += 1
                elif x - outer - amp < cl[0] or x + outer + amp >= cl[2] or y - outer - amp < cl[1] or \
                        y + outer + amp >= cl[3]:
                    reached['clipped'] += 1
                else:
                    reached['inside'] += 1
                reached['moved'] += want[0] != screen0
    print(f'ok: {2 * n} ripples identical at 24 and 53 bits ({reached["inside"]} inside, {reached["clipped"]} '
          f'clipped, {reached["nothing"]} drawing nothing; {reached["moved"]} changed the screen)' if not failed
          else f'{failed} of {2 * n} differ')
    sys.stdout.flush()
    os._exit(1 if failed else 0)


main()
