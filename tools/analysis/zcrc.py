"""Checks a Pirate's Moon zArchive's CRC (version 2), and with --fix rewrites it.

  zcrc.py [--fix] <archive.zbd> ...

The CRC is the archive's last dword, computed as PM's check does (zarc.archive_crc,
0x00597a30). PM checks only zbd\\reader.zbd and closes without a message on a
mismatch (docs/protection.md section 6); the other archives ship with 0 there,
reported as unchecked and left alone by --fix.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from zarc import archive_crc  # noqa: E402


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
        if stored == 0:
            print(f'{path}: no CRC stored (0, as the shipped mechlib, motion and sounds; not checked)')
            continue
        actual = archive_crc(bytes(data))
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
