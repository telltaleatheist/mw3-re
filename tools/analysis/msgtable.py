"""Prints the message table of Mech3Msg.dll (what FormatMessageA reads).

  msgtable.py [id ...]     all messages, or just these ids (hex or decimal)
"""
import struct
import sys
from pathlib import Path

DLL = Path(__file__).resolve().parents[2] / 'game' / 'mw3-1.2' / 'Mech3Msg.dll'


def messages(b):
    pe = struct.unpack_from('<I', b, 0x3c)[0]
    n = struct.unpack_from('<H', b, pe + 6)[0]
    osz = struct.unpack_from('<H', b, pe + 20)[0]
    opt = pe + 24
    rsrc = struct.unpack_from('<I', b, opt + 112)[0]
    secs = []
    for i in range(n):
        vs, va, rs, raw = struct.unpack_from('<IIII', b, opt + osz + i * 40 + 8)
        secs.append((va, max(vs, rs), raw))

    def off(r):
        for va, sz, raw in secs:
            if va <= r < va + sz:
                return r - va + raw

    base = off(rsrc)

    def entries(o):
        nn, ni = struct.unpack_from('<HH', b, o + 12)
        for k in range(nn + ni):
            yield struct.unpack_from('<II', b, o + 16 + 8 * k)

    out = {}
    for t, d in entries(base):
        if t != 11:
            continue
        for _, d2 in entries(base + (d & 0x7fffffff)):
            for _, d3 in entries(base + (d2 & 0x7fffffff)):
                rva, _ = struct.unpack_from('<II', b, base + d3)
                o = off(rva)
                blocks, = struct.unpack_from('<I', b, o)
                for k in range(blocks):
                    lo, hi, at = struct.unpack_from('<III', b, o + 4 + 12 * k)
                    p = o + at
                    for mid in range(lo, hi + 1):
                        ln, flags = struct.unpack_from('<HH', b, p)
                        raw = b[p + 4:p + ln]
                        text = raw.decode('utf-16le' if flags & 1 else 'latin-1').rstrip('\0\r\n')
                        out[mid] = text
                        p += ln
    return out


def main():
    msgs = messages(DLL.read_bytes())
    ids = [int(x, 0) for x in sys.argv[1:]] or sorted(msgs)
    for i in ids:
        print(f'{i:#06x}\t{msgs.get(i)!r}')


if __name__ == '__main__':
    main()
