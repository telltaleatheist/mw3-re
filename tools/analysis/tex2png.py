"""Exports textures from a texture library (rimage.zbd, rmechtex*.zbd) as PNG.

  tex2png.py <library.zbd> <outdir> [name-substring]

Reads the format in docs/textures.md directly (no game code), writes one PNG
per texture, with alpha when the texture has any.
"""
import struct
import sys
import zlib
from pathlib import Path


def png(path, w, h, rows, alpha):
    def chunk(kind, data):
        c = kind + data
        return struct.pack('>I', len(data)) + c + struct.pack('>I', zlib.crc32(c) & 0xffffffff)
    raw = b''.join(b'\0' + r for r in rows)
    ihdr = struct.pack('>IIBBBBB', w, h, 8, 6 if alpha else 2, 0, 0, 0)
    Path(path).write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', ihdr) +
                           chunk(b'IDAT', zlib.compress(raw, 6)) + chunk(b'IEND', b''))


def rgb(c):
    r, g, b = (c >> 11) & 0x1f, (c >> 5) & 0x3f, c & 0x1f
    return (r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)


def main():
    data = Path(sys.argv[1]).read_bytes()
    out = Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    want = sys.argv[3].lower() if len(sys.argv) > 3 else ''
    _, has, npal, count, _, _ = struct.unpack_from('<IIiiII', data, 0)
    assert has == 1
    entries = [struct.unpack_from('<32sIi', data, 24 + 40 * i) for i in range(count)]
    pal_at = 24 + 40 * count
    palettes = [struct.unpack_from('<256H', data, pal_at + 0x200 * i) for i in range(npal)]
    written = 0
    for raw_name, off, gpal in entries:
        name = raw_name.split(b'\0')[0].decode('latin-1')
        if want not in name.lower():
            continue
        flags, w, h, _, pcount, _ = struct.unpack_from('<IHHIHH', data, off)
        p = off + 16
        n = w * h
        if pcount:
            idx = data[p:p + n]
            p += n
        else:
            px = struct.unpack_from(f'<{n}H', data, p)
            p += 2 * n
        alpha = None
        if flags & 0x08:
            alpha = data[p:p + n]
            p += n
        if pcount:
            pal = palettes[gpal] if gpal != -1 else struct.unpack_from(f'<{pcount}H', data, p)
            px = [pal[i] if i < len(pal) else 0 for i in idx]
        elif flags & 0x02 and alpha is None:
            alpha = bytes(0 if c == 0 else 255 for c in px)
        rows = []
        for y in range(h):
            row = bytearray()
            for x in range(w):
                row += bytes(rgb(px[y * w + x]))
                if alpha is not None:
                    row.append(alpha[y * w + x])
            rows.append(bytes(row))
        png(out / f'{name}.png', w, h, rows, alpha is not None)
        written += 1
    print(f'{written} textures written to {out}')


if __name__ == '__main__':
    main()
