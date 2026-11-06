"""Where the port still has MW3's name, key or format and PM's exe has another (reads only).

  pm_sweep.py [outdir]

Lists, from game/pm/Mech3.exe, game/mw3-1.2/Mech3.exe, analysis-output/pm.asm, the shipped .zrd archives and src/:
  keys.txt     keys passed to the .zrd readers in PM's exe that no string literal in src/ has
  datakeys.txt keys in PM's shipped .zrd that src/ has no literal for, that MW3's data lacks, with the file they're in
  formats.txt  printf-style strings of PM's exe that src/ lacks and MW3's exe lacks
  mw3only.txt  literals in src/ that MW3's exe has and PM's lacks: MW3's form, still in the port
A literal built at run time shows as missing; each line is a lead to read, not a bug.
"""
import json
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools' / 'analysis'))
import zrd  # noqa: E402

# zreader.cpp's calls that take a key (their MW3 addresses).
READERS = ('0x556930', '0x5569e0', '0x5569f0', '0x556a20', '0x556a60', '0x556ae0', '0x556b30', '0x556b50', '0x5570b0',
           '0x5570d0', '0x5570f0', '0x557110', '0x557130', '0x557150')


def sections(data):
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    n, opt = struct.unpack_from('<H', data, pe + 6)[0], struct.unpack_from('<H', data, pe + 20)[0]
    out = []
    for i in range(n):
        vsize, va, rsize, raw = struct.unpack_from('<IIII', data, pe + 24 + opt + 40 * i + 8)
        out.append((0x400000 + va, rsize, raw))
    return out


def exe_strings(path):
    """Every NUL-ended printable run of 3 or more, by address."""
    data = path.read_bytes()
    found = {}
    for va, size, raw in sections(data):
        for m in re.finditer(rb'[\x20-\x7e\t\r\n]{3,}\x00', data[raw:raw + size]):
            found[va + m.start()] = m.group()[:-1].decode('latin-1')
    return found, data


def literal_set():
    lits = set()
    pat = re.compile(r'"((?:[^"\\\n]|\\.)*)"')
    for p in list((ROOT / 'src').rglob('*.cpp')) + list((ROOT / 'src').rglob('*.h')) + list((ROOT / 'src').rglob('*.inc')):
        text = p.read_text(errors='replace')
        text = re.sub(r'^\s*//.*$', '', text, flags=re.M)
        text = re.sub(r'\s//.*$', '', text, flags=re.M)
        text = re.sub(r'"\s*\n\s*"', '', text)
        for m in pat.finditer(text):
            lits.add(m.group(1).encode().decode('unicode_escape', 'replace'))
    return lits


def keys_in_exe(strings, data):
    m = json.load(open(ROOT / 'analysis-output' / 'pm-diff' / 'addrmap.json'))['functions']
    calls = {m[a] for a in READERS if a in m}
    want = {f'call\t{a}' for a in calls}
    lines = (ROOT / 'analysis-output' / 'pm.asm').read_text(errors='replace').splitlines()
    keys = {}
    for i, line in enumerate(lines):
        if 'call\t0x' not in line or not any(w + ' ' in line or line.endswith(w) for w in want):
            continue
        for back in lines[max(0, i - 8):i]:
            for a in re.findall(r'0x[0-9a-f]{6}\b', back):
                s = strings.get(int(a, 16))
                if s:
                    keys.setdefault(s, line.split(':')[0].strip())
    return keys


def walk(node, out):
    if isinstance(node, (list, tuple)):
        for i, n in enumerate(node):
            nxt = node[i + 1] if i + 1 < len(node) else None
            if isinstance(n, str) and isinstance(nxt, (list, tuple)):
                out.add(n)
            walk(n, out)


def data_keys(game):
    keys = {}
    for z in sorted((ROOT / 'game' / game / 'zbd').rglob('*.zbd')):
        if not z.name.lower().startswith('reader'):
            continue
        blob = z.read_bytes()
        try:
            names = list(zrd.entries(blob))
        except Exception:
            continue
        for name, start, length in names:
            found = set()
            try:
                tree = zrd.node(blob, start)
                walk(tree[0] if isinstance(tree, tuple) and len(tree) == 2 and isinstance(tree[1], int) else tree, found)
            except Exception:
                continue
            for k in found:
                keys.setdefault(k, name)
    return keys


def main():
    out = Path(sys.argv[1] if len(sys.argv) > 1 else 'pm-sweep')
    out.mkdir(parents=True, exist_ok=True)
    pm, pm_data = exe_strings(ROOT / 'game' / 'pm' / 'Mech3.exe')
    mw3, _ = exe_strings(ROOT / 'game' / 'mw3-1.2' / 'Mech3.exe')
    pm_set, mw3_set = set(pm.values()), set(mw3.values())
    lits = literal_set()
    lower = {s.lower() for s in lits}
    keys = keys_in_exe(pm, pm_data)
    a = sorted(k for k in keys if k not in lits and k.lower() not in lower)
    (out / 'keys.txt').write_text(''.join(f'{keys[k]}\t{k}\n' for k in a))
    pm_keys, mw3_keys = data_keys('pm'), data_keys('mw3-1.2')
    b = sorted(k for k in pm_keys if k not in mw3_keys and k.lower() not in lower and (k in pm_set or re.fullmatch(r'[A-Z_0-9]{3,}', k)))
    (out / 'datakeys.txt').write_text(''.join(f'{pm_keys[k]}\t{k}\t{"in exe" if k in pm_set else ""}\n' for k in b))
    c = sorted(s for s in pm_set if '%' in s and s not in mw3_set and s not in lits and len(s) < 120)
    (out / 'formats.txt').write_text(''.join(repr(s)[1:-1] + '\n' for s in c))
    d = sorted(s for s in lits if s in mw3_set and s not in pm_set and len(s) > 3 and 'zipper' not in s.lower())
    (out / 'mw3only.txt').write_text(''.join(repr(s)[1:-1] + '\n' for s in d))
    print(f'keys {len(keys)} read, {len(a)} not in src; data keys {len(b)}; formats {len(c)}; mw3-only literals {len(d)}')


if __name__ == '__main__':
    main()
