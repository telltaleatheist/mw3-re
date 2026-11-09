"""Checks a Pirate's Moon zArchive's CRC (version 2), and with --fix rewrites it.

  zcrc.py [--fix] <archive.zbd> ...

The CRC is the archive's last dword: zarc.crc of every byte before the entry
table (docs/pirates-moon.md, Archive version 2). The exe quits silently on a
stale one. --fix only rewrites the dword; an archive whose data is out of
table order needs zarc.py's rebuild instead.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from zarc import crc  # noqa: E402


def main():
    fix = '--fix' in sys.argv
    paths = [a for a in sys.argv[1:] if a != '--fix']
    if not paths:
        sys.exit(__doc__)
    bad = 0
    for path in paths:
        data = bytearray(open(path, 'rb').read())
        version, count, stored = struct.unpack_from('<III', data, len(data) - 12)
        if version != 2:
            print(f'{path}: archive version {version}, no CRC')
            continue
        actual = crc(bytes(data[:len(data) - 12 - count * 0x94]))
        print(f'{path}: stored {stored:#010x}, computed {actual:#010x}, {"OK" if actual == stored else "MISMATCH"}')
        if actual != stored:
            bad += 1
            if fix:
                struct.pack_into('<I', data, len(data) - 4, actual)
                open(path, 'wb').write(data)
                print('  fixed')
    sys.exit(1 if bad and not fix else 0)


if __name__ == '__main__':
    main()
