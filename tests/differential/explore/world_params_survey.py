"""Prints every mission's world parameters (world.zrd, and mpinfo.zrd for the
multiplayer maps 3 and 4) one line per block, for docs/drafts/world-params.md.

  .venv/bin/python tests/differential/explore/world_params_survey.py
"""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools' / 'analysis'))
import zrd  # noqa: E402

GAME = ROOT / 'game' / 'mw3-1.2' / 'zbd'


def read(path, name):
    data = path.read_bytes()
    for n, start, length in zrd.entries(data):
        if n.lower() == name:
            return zrd.node(data[start:start + length], 0)[0]
    return None


def pairs(lst):
    return {lst[i]: lst[i + 1] for i in range(0, len(lst) - 1, 2) if isinstance(lst[i], str)}


def show(v):
    if isinstance(v, list):
        if len(v) == 1 and not isinstance(v[0], list):
            return show(v[0])
        if all(isinstance(x, str) for x in v[::2]) and len(v) % 2 == 0 and v:
            return '{' + ' '.join(f'{k}={show(x)}' for k, x in pairs(v).items()) + '}'
        return '(' + ' '.join(show(x) for x in v) + ')'
    if isinstance(v, float):
        return f'{v:g}'
    return str(v)


def blocks():
    for m in sorted(p.name for p in GAME.iterdir() if p.is_dir()):
        top = read(GAME / m / 'reader.zbd', 'world.zrd')
        if top is not None:
            for key, params in pairs(top[0]).items():
                yield f'{m}/{key}', params
        for f in sorted((GAME / m).glob('readermp*.zbd')):
            mp = read(f, 'mpinfo.zrd')
            if mp is not None:
                yield f'{m}/{f.stem[6:]}(mpinfo)', pairs(mp[0]).get('world_params')


def main():
    for name, params in blocks():
        print(name)
        for k, v in pairs(params).items():
            print(f'  {k:20s} {show(v)}')


if __name__ == '__main__':
    main()
