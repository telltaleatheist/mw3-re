"""Rebuilds a Pirate's Moon zArchive (version 2) with some entries' data replaced.

  zarc.py <archive.zbd> <out.zbd> <name.zrd>=<file> ...

As a module: rebuild(data, {name: bytes}) -> bytes.

The archive ends with the 0x94-byte entry table, version 2, the count and
a CRC: zutl::crc32 run over each entry's data in table order (PM's
0x00597a30). PM checks it on zbd\\reader.zbd only, after the intro
(0x004876e0, docs/protection.md section 6), and a mismatch closes the game
without a message; mechlib, motion and the sounds hold 0 and nothing checks
them. This writes the data back to back in table order, repoints each
record's start and length, keeps the rest of each record (names, time
stamp) and recomputes the CRC. It takes only an archive already laid out
that way (reader*.zbd, mechlib, the sounds); motion.zbd's records hold 1 as
the length, so it is refused.
"""
import struct
import sys

_TABLE = []
for _i in range(256):
    _c = _i << 24
    for _ in range(8):
        _c = ((_c << 1) ^ 0x04C11DB7) & 0xffffffff if _c & 0x80000000 else (_c << 1) & 0xffffffff
    _TABLE.append(_c)


def crc(data, c=0):
    """MW3's zutl::crc32 (0x576c10, PM's 0x00597bc0): MSB first, polynomial 0x04c11db7, from 0."""
    for x in data:
        c = ((c << 8) & 0xffffffff) ^ _TABLE[((c >> 24) ^ x) & 0xff]
    return c


def archive_crc(data):
    """The CRC PM's check computes (0x00597a30): each entry's data, by its record's start and length, in table order."""
    _, count, _ = struct.unpack_from('<III', data, len(data) - 12)
    table, c = len(data) - 12 - count * 0x94, 0
    for i in range(count):
        start, length = struct.unpack_from('<II', data, table + i * 0x94)
        c = crc(data[start:start + length], c)
    return c


def rebuild(data, new):
    version, count, stored = struct.unpack_from('<III', data, len(data) - 12)
    if version != 2:
        raise ValueError(f'archive version {version}, not 2 (Pirate\'s Moon)')
    table = len(data) - 12 - count * 0x94
    pos = 0
    for i in range(count):
        start, length = struct.unpack_from('<II', data, table + i * 0x94)
        if start != pos:
            raise ValueError(f'entry {i} starts at {start}, not {pos}: the data is not back to back in table order')
        pos = start + length
    body, records = bytearray(), []
    for i in range(count):
        rec = bytearray(data[table + i * 0x94:table + (i + 1) * 0x94])
        start, length = struct.unpack_from('<II', rec, 0)
        name = rec[8:72].split(b'\0')[0].decode('latin-1')
        entry = new.get(name, data[start:start + length])
        struct.pack_into('<II', rec, 0, len(body), len(entry))
        body += entry
        records.append(bytes(rec))
    return bytes(body) + b''.join(records) + struct.pack('<III', 2, count, crc(bytes(body)) if stored else 0)  # an archive shipped with 0 keeps it


def main():
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    new = {}
    for arg in sys.argv[3:]:
        name, path = arg.split('=', 1)
        new[name] = open(path, 'rb').read()
    out = rebuild(open(sys.argv[1], 'rb').read(), new)
    open(sys.argv[2], 'wb').write(out)


if __name__ == '__main__':
    main()
