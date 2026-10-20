"""zTwoD's software blit against the original x86 (docs/twod.md, Drawing).

    cmake --build build && .venv/bin/python tests/differential/test_twod_blit.py [runs]

The original 0x0055b3c0 and the port's zrndr::blit16 draw the same random
image into the same random 16-bit target: direct and palettised pixels, with
an alpha plane (565 and 555 blends), with the colour key, or opaque (the
whole-width copy too), placed anywhere from off screen to past the corners,
whole or by a source rectangle, with targets of odd widths and padded pitches.
The targets must be identical. Two FIX (bug)s are outside the comparison:
the original's extra row (a bottom of exactly the height), whose writes are
counted, and a zero-width palettised blit, which runs away in the original
(its loops count the width down from 0) and draws nothing in the port.
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import load_probe  # noqa: E402
from original import Original  # noqa: E402

BLIT = 0x55b3c0
TARGET = (0x8007c8, 0x8007cc, 0x8007d0, 0x8007d4)
FORMAT = 0x74a738
probe = load_probe()
probe.probe_blit16.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_int32,
                               ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int32, ctypes.c_int32,
                               ctypes.c_int32, ctypes.c_uint8, ctypes.c_int32, ctypes.c_int32, ctypes.c_uint16,
                               ctypes.c_void_p]


def case(r):
    tw = r.choice([640, 320, 512, 63, 101, 800])
    th = r.choice([480, 240, 384, 47, 77])
    pitch = 2 * tw + r.choice([0, 0, 0, 2, 64])
    fmt = r.choice([6, 6, 5])
    w = r.choice([1, 2, 3, 7, 16, 33, 64, 100, 255, tw, tw + 5])
    h = r.choice([1, 2, 5, 16, 40, 99, th, th + 3])
    stride = w + r.choice([0, 0, 0, 3])
    pal = r.random() < 0.3
    flags = r.choice([0x01, 0x03, 0x01 | 0x08 | 0x02, 0x00])
    alpha = r.random() < 0.35
    if r.random() < 0.15 and w <= tw and h <= th and pitch == 2 * tw:  # the whole-width copy
        stride, x, y, w = tw, 0, r.randrange(-5, th), tw
    else:
        x = r.randrange(-w - 10, tw + 10)
        y = r.randrange(-h - 10, th + 10)
        if r.random() < 0.1:
            y = th - h + 1  # a bottom of exactly the height
    rect = None
    if r.random() < 0.4:
        a = r.randrange(0, w)
        b = r.randrange(0, h)
        rect = [a, b, r.randrange(a, w + 1), r.randrange(b, h + 1)]
        if r.random() < 0.05:
            rect = [5, 5, 3, 9]  # negative width: nothing drawn
    key = r.choice([0, 0, 0xf81f, 5, r.randrange(1 << 16)])
    target = bytes(r.randrange(256) for _ in range(pitch * (th + 2)))
    if pal:
        pixels = bytes(r.randrange(256) if r.random() < 0.8 else key & 0xff for _ in range(stride * h))
        palette = struct.pack('<256H', *[r.randrange(1 << 16) for _ in range(256)])
    else:
        pixels = struct.pack(f'<{stride * h}H', *[r.randrange(1 << 16) if r.random() < 0.8 else key
                                                   for _ in range(stride * h)])
        palette = None
    alphas = bytes(r.choice([0, 2, 3, 4, 7, 8, 100, 200, 251, 252, 255, r.randrange(256)])
                   for _ in range(stride * h)) if alpha else None
    return dict(tw=tw, th=th, pitch=pitch, fmt=fmt, w=w, h=h, stride=stride, flags=flags, x=x, y=y, rect=rect,
                key=key, target=target, pixels=pixels, palette=palette, alpha=alphas)


def original(cpu, c):
    cpu.heap_next = 0x10000000
    tgt = cpu.alloc(len(c['target']))
    cpu.write(tgt, c['target'])
    for a, v in zip(TARGET, (tgt, c['tw'], c['th'], c['pitch'])):
        cpu.u32(a, v)
    cpu.u32(FORMAT, c['fmt'])
    img = cpu.alloc(0x38)
    cpu.write(img, bytes(0x38))
    px = cpu.alloc(len(c['pixels']) + 16)
    cpu.write(px, c['pixels'])
    cpu.write(img + 4, struct.pack('<hh', c['w'], c['h']))
    cpu.write(img + 9, bytes([c['flags']]))
    cpu.u32(img + 0x10, px)
    if c['alpha']:
        al = cpu.alloc(len(c['alpha']) + 16)
        cpu.write(al, c['alpha'])
        cpu.u32(img + 0x14, al)
    if c['palette']:
        pl = cpu.alloc(512)
        cpu.write(pl, c['palette'])
        cpu.u32(img + 0x18, pl)
    cpu.u32(img + 0x34, c['stride'])
    rp = 0
    if c['rect']:
        rp = cpu.alloc(16)
        cpu.write(rp, struct.pack('<4i', *c['rect']))
    cpu.call(BLIT, ecx=img, edx=c['x'] & 0xffffffff, stack_args=(c['y'] & 0xffffffff, c['key'], rp))
    return cpu.read(tgt, len(c['target']))


def port(c):
    tgt = ctypes.create_string_buffer(c['target'], len(c['target']))
    px = ctypes.create_string_buffer(c['pixels'] + bytes(16))
    al = ctypes.create_string_buffer(c['alpha'] + bytes(16)) if c['alpha'] else None
    pl = ctypes.create_string_buffer(c['palette']) if c['palette'] else None
    rc = (ctypes.c_int32 * 4)(*c['rect']) if c['rect'] else None
    probe.probe_blit16(tgt, c['tw'], c['th'], c['pitch'], c['fmt'], px, al, pl, c['w'], c['h'], c['stride'],
                       c['flags'], c['x'], c['y'], c['key'], rc)
    return tgt.raw


def main():
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    cpu = Original()
    bad = extra = 0
    kinds = {}
    runaway = 0
    for i in range(runs):
        r = random.Random(i)
        c = case(r)
        if c['palette'] and c['rect'] and c['rect'][2] == c['rect'][0] and c['rect'][3] > c['rect'][1]:
            # FIX (bug): the original's palette loops count a zero width down
            # from 0 and write until they fault; the port draws nothing.
            runaway += 1
            if port(c) != c['target']:
                bad += 1
                print(f'run {i}: the port drew a zero-width palettised blit')
            continue
        a, b = original(cpu, c), port(c)
        inside = c['pitch'] * c['th']
        kind = ('pal' if c['palette'] else 'rgb') + ('+alpha' if c['alpha'] else '+key' if c['flags'] & 2 else '')
        kinds[kind] = kinds.get(kind, 0) + 1
        if a[inside:] != c['target'][inside:]:
            extra += 1
        if a[:inside] != b[:inside]:
            bad += 1
            if bad <= 5:
                n = next(j for j in range(inside) if a[j] != b[j])
                print(f'run {i}: differ at byte {n} (row {n // c["pitch"]}, x {(n % c["pitch"]) // 2}); '
                      f'{ {k: v for k, v in c.items() if k not in ("target", "pixels", "palette", "alpha")} }')
    print(f'zTwoD blit: {runs - bad}/{runs} targets identical ({", ".join(f"{k} {v}" for k, v in sorted(kinds.items()))}; '
          f'the original wrote past the last row in {extra}; {runaway} zero-width palettised blits, which '
          f'run away in the original, drew nothing)')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
