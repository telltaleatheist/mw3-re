#!/usr/bin/env python3
"""Find pixels that blink between consecutive frames (playtest #22).

Usage: blinks.py OUT.png FRAME1.bmp FRAME2.bmp ... (in frame order, three at least)

A pixel blinks in frame i when it differs strongly from both frame i-1 and
frame i+1 while those two agree: something drawn for one frame only, or
missing for one frame. Prints the count per frame and writes OUT.png, the
middle frame darkened with every blinking pixel in red. The frames come from
`build/mw3 --shot F.bmp --shot-frame N --shot-series K` (24- or 32-bit BMPs).
Moving edges (a walking camera's bob) show too; a still view isolates flicker.
"""
import struct
import sys
import zlib


def load(path):
    d = open(path, 'rb').read()
    off = struct.unpack_from('<I', d, 10)[0]
    w, h = struct.unpack_from('<ii', d, 18)
    bp = struct.unpack_from('<H', d, 28)[0] // 8
    row = (w * bp + 3) & ~3
    ah = abs(h)
    px = [None] * ah
    for y in range(ah):
        r = off + y * row
        line = [(d[r + x * bp + 2], d[r + x * bp + 1], d[r + x * bp]) for x in range(w)]
        px[ah - 1 - y if h > 0 else y] = line
    return w, ah, px


def write_png(path, w, h, rows):
    raw = b''.join(b'\0' + bytes(c for p in r for c in p) for r in rows)

    def chunk(t, b):
        return struct.pack('>I', len(b)) + t + b + struct.pack('>I', zlib.crc32(t + b) & 0xffffffff)

    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) +
                           chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))


def main():
    if len(sys.argv) < 5:
        sys.exit(__doc__)
    out, files = sys.argv[1], sys.argv[2:]
    imgs = [load(f)[2] for f in files]
    h, w = len(imgs[0]), len(imgs[0][0])

    def diff(a, b):
        return abs(a[0] - b[0]) + abs(a[1] - b[1]) + abs(a[2] - b[2])

    mark = [[False] * w for _ in range(h)]
    for i in range(1, len(imgs) - 1):
        a, b, c = imgs[i - 1], imgs[i], imgs[i + 1]
        n = 0
        for y in range(h):
            ra, rb, rc = a[y], b[y], c[y]
            for x in range(w):
                if diff(ra[x], rb[x]) > 90 and diff(rb[x], rc[x]) > 90 and diff(ra[x], rc[x]) < 30:
                    n += 1
                    mark[y][x] = True
        print(f'{files[i]}: {n} blinking pixels')
    base = imgs[len(imgs) // 2]
    write_png(out, w, h, [[(255, 0, 0) if mark[y][x] else tuple(v // 3 for v in base[y][x]) for x in range(w)]
                          for y in range(h)])


if __name__ == '__main__':
    main()
