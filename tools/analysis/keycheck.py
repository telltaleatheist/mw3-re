"""Lists the reader keys the exe looks up that no data file contains.

  keycheck.py

Walks every reader archive under game/mw3-1.2/zbd and every lookup from
rdrkeys.py; a key never present is optional, dead, or a spelling mismatch.
"""
import glob
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import rdrkeys  # noqa: E402
import zrd  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]


def strings():
    out = set()

    def walk(v):
        if isinstance(v, list):
            for x in v:
                walk(x)
        elif isinstance(v, str):
            out.add(v)
    zbd = ROOT / 'game' / 'mw3-1.2' / 'zbd'
    for f in [zbd / 'reader.zbd'] + [Path(p) for p in glob.glob(str(zbd / '*' / 'reader*.zbd'))]:
        data = f.read_bytes()
        for _, start, length in zrd.entries(data):
            try:
                walk(zrd.node(data[start:start + length], 0)[0])
            except (ValueError, IndexError, UnicodeDecodeError, struct_error):
                pass
    return out


struct_error = __import__('struct').error


def main():
    have = strings()
    seen = {}
    for addr, func, src, getter, key, node, out in rdrkeys.lookups():
        if key and key not in have:
            seen.setdefault(key, (func, src))
    for key in sorted(seen):
        func, src = seen[key]
        print(f'{key!r:32} {func:#x} {src}')
    print(f'{len(seen)} keys never present in the data')


if __name__ == '__main__':
    main()
