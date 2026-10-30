"""Pirate's Moon's data against MW3 1.2's, file by file (docs/pirates-moon.md).

  pmdata.py [mw3_dir] [pm_dir] [-v]   defaults: game/mw3-1.2 game/pm; -v lists the entries new and gone

Every file under zbd/ (any case) is new, removed, the same or changed; a
zArchive (reader*.zbd, sounds*.zbd, motion.zbd, mechlib.zbd, ...) is also
compared entry by entry, with its archive version (MW3 1, PM 2).
"""
import hashlib
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(__file__))
import zrd  # noqa: E402


def tree(root):
    out = {}
    for d, _, files in os.walk(os.path.join(root, 'zbd')):
        for f in files:
            p = os.path.join(d, f)
            out[os.path.relpath(p, root).replace(os.sep, '/').lower()] = p
    return out


def archive(data):
    """(version, {name: sha1}) for a zArchive, else None."""
    if len(data) < 12:
        return None
    for tail in (8, 12):
        version, count = struct.unpack_from('<II', data, len(data) - tail)
        if version in (1, 2) and (tail == 8) == (version == 1) and 0 < count < 100000 and count * 0x94 + tail <= len(data):
            try:
                names = {}
                for name, start, length in zrd.entries(data):
                    if start + length > len(data):
                        return None
                    names[name.lower()] = hashlib.sha1(data[start:start + length]).hexdigest()
                return version, names
            except (AssertionError, struct.error, UnicodeDecodeError):
                return None
    return None


def main():
    here = os.path.join(os.path.dirname(__file__), '..', '..')
    verbose = '-v' in sys.argv
    args = [x for x in sys.argv[1:] if x != '-v']
    mw3 = args[0] if len(args) > 0 else os.path.join(here, 'game', 'mw3-1.2')
    pm = args[1] if len(args) > 1 else os.path.join(here, 'game', 'pm')
    a, b = tree(mw3), tree(pm)
    same, changed, new, gone = [], [], sorted(set(b) - set(a)), sorted(set(a) - set(b))
    for k in sorted(set(a) & set(b)):
        da, db = open(a[k], 'rb').read(), open(b[k], 'rb').read()
        (same if da == db else changed).append(k)
    print(f'zbd/: {len(a)} files in MW3 1.2, {len(b)} in PM; {len(same)} the same, {len(changed)} changed, '
          f'{len(new)} new in PM, {len(gone)} only in MW3')
    for k in changed:
        da, db = open(a[k], 'rb').read(), open(b[k], 'rb').read()
        za, zb = archive(da), archive(db)
        line = f'  changed {k}: {len(da)} -> {len(db)} bytes'
        if za and zb:
            ea, eb = za[1], zb[1]
            both = set(ea) & set(eb)
            diff = sum(ea[n] != eb[n] for n in both)
            line += (f'; archive v{za[0]} -> v{zb[0]}, {len(ea)} -> {len(eb)} entries: '
                     f'{len(both) - diff} same, {diff} changed, {len(set(eb) - set(ea))} new, {len(set(ea) - set(eb))} gone')
        elif zb or za:
            line += f'; archive only in {"PM" if zb else "MW3"}'
        print(line)
        if za and zb and verbose:
            for n in sorted(set(zb[1]) - set(za[1])):
                print(f'      new entry {n}')
            for n in sorted(set(za[1]) - set(zb[1])):
                print(f'      gone entry {n}')
    for k in new:
        db = open(b[k], 'rb').read()
        zb = archive(db)
        print(f'  new {k}: {len(db)} bytes' + (f'; archive v{zb[0]}, {len(zb[1])} entries' if zb else ''))
    for k in gone:
        print(f'  only in MW3 {k}: {os.path.getsize(a[k])} bytes')
    for k in same:
        print(f'  same {k}')


if __name__ == '__main__':
    main()
