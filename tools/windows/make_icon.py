#!/usr/bin/env python3
"""Writes tools/windows/mw3.ico: the rebuild's placeholder icon, ours (a green
targeting reticle on a dark rounded square), nothing from the game or its
publisher. PNG images inside the .ico (Windows Vista and later), at 16, 32, 48
and 256 pixels; standard library only.

    python3 tools/windows/make_icon.py
"""
import math
import pathlib
import struct
import zlib

BG = (28, 32, 30)
EDGE = (60, 70, 64)
GREEN = (70, 220, 90)


def coverage(d, half):
    """How much of a pixel lies within `half` of a line at distance `d`."""
    return max(0.0, min(1.0, half + 0.5 - abs(d)))


def pixel(x, y, n):
    s = n / 256.0
    cx = cy = n / 2.0
    px, py = x + 0.5 - cx, y + 0.5 - cy
    # The rounded square: inside, its edge, outside transparent.
    r_corner, half = 40 * s, n / 2.0 - 0.5
    qx, qy = max(abs(px) - (half - r_corner), 0.0), max(abs(py) - (half - r_corner), 0.0)
    out = math.hypot(qx, qy) - r_corner
    a = max(0.0, min(1.0, 0.5 - out))
    if a == 0.0:
        return (0, 0, 0, 0)
    col = list(BG)
    edge = coverage(out + 6 * s, 3 * s)
    col = [c + (e - c) * edge for c, e in zip(col, EDGE)]
    # The reticle: a ring, four ticks with a gap at the centre, a dot.
    r = math.hypot(px, py)
    g = coverage(r - 78 * s, max(7 * s, 0.6))
    w = max(6 * s, 0.6)
    if abs(px) <= w + 0.5 and 30 * s <= abs(py) <= 112 * s:
        g = max(g, coverage(px, w))
    if abs(py) <= w + 0.5 and 30 * s <= abs(px) <= 112 * s:
        g = max(g, coverage(py, w))
    g = max(g, coverage(r, max(10 * s, 0.8)))
    col = [c + (gr - c) * g for c, gr in zip(col, GREEN)]
    return (int(round(col[0])), int(round(col[1])), int(round(col[2])), int(round(a * 255)))


def png(n):
    rows = b''.join(b'\0' + b''.join(bytes(pixel(x, y, n)) for x in range(n)) for y in range(n))

    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)

    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', n, n, 8, 6, 0, 0, 0)) +
            chunk(b'IDAT', zlib.compress(rows, 9)) + chunk(b'IEND', b''))


def main():
    sizes = [16, 32, 48, 256]
    images = [png(n) for n in sizes]
    head = struct.pack('<HHH', 0, 1, len(images))
    offset = 6 + 16 * len(images)
    entries = b''
    for n, data in zip(sizes, images):
        entries += struct.pack('<BBBBHHII', n % 256, n % 256, 0, 0, 1, 32, len(data), offset)
        offset += len(data)
    out = pathlib.Path(__file__).with_name('mw3.ico')
    out.write_bytes(head + entries + b''.join(images))
    print(f'{out}: {", ".join(str(n) for n in sizes)} px')


if __name__ == '__main__':
    main()
