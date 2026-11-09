"""Rebuilds a Pirate's Moon zArchive (version 2) with some entries' data replaced.

  zarc.py <archive.zbd> <out.zbd> <name.zrd>=<file> ...

As a module: rebuild(data, {name: bytes}) -> bytes.

PM's exe takes an archive only as its own writer lays it out: the entries'
data back to back in table order, then the 0x94-byte table, then version 2,
the count and the CRC of every byte before the table (docs/pirates-moon.md,
Archive version 2). A stale CRC, or data out of table order or with gaps
(an edited entry appended before the table, say), makes the exe quit
silently soon after its "dshell(void): STARTING" line. This writes the data
in table order, repoints each record's start and length, keeps the rest of
each record (names, time stamp) and recomputes the CRC.
"""
import struct
import sys

_TABLE = []
for _i in range(256):
    _c = _i << 24
    for _ in range(8):
        _c = ((_c << 1) ^ 0x04C11DB7) & 0xffffffff if _c & 0x80000000 else (_c << 1) & 0xffffffff
    _TABLE.append(_c)


def crc(data):
    """MW3's zutl::crc32 (0x576c10): MSB first, polynomial 0x04c11db7, from 0."""
    c = 0
    for x in data:
        c = ((c << 8) & 0xffffffff) ^ _TABLE[((c >> 24) ^ x) & 0xff]
    return c


def rebuild(data, new):
    version, count, _ = struct.unpack_from('<III', data, len(data) - 12)
    if version != 2:
        raise ValueError(f'archive version {version}, not 2 (Pirate\'s Moon)')
    table = len(data) - 12 - count * 0x94
    body, records = bytearray(), []
    for i in range(count):
        rec = bytearray(data[table + i * 0x94:table + (i + 1) * 0x94])
        start, length = struct.unpack_from('<II', rec, 0)
        name = rec[8:72].split(b'\0')[0].decode('latin-1')
        entry = new.get(name, data[start:start + length])
        struct.pack_into('<II', rec, 0, len(body), len(entry))
        body += entry
        records.append(bytes(rec))
    return bytes(body) + b''.join(records) + struct.pack('<III', 2, count, crc(bytes(body)))


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
