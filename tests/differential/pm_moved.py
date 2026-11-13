"""Runs one of MW3's differential tests on PM's exe by rewriting its addresses, not translating them:
each module named below is read with every integer literal in the image's range replaced by its twin
in PM (tools/analysis/pmaddr.py's map), and the emulator gets no address map. So pointers the exe holds
at run time (its vtables, its strings) are read as they are, which a translating map can't tell from a
test's constants. Stub keys stay MW3's, which is what the port's probes record.

    MW3_EXE=pm .venv/bin/python -P tests/differential/pm_moved.py test_lab_list [args]
"""
import importlib.abc
import importlib.machinery
import importlib.util
import io
import json
import os
import re
import struct
import sys
import tokenize
from pathlib import Path

os.environ['MW3_EXE'] = 'pm'
os.environ['MW3_PM_MOVED'] = '1'
HERE = Path(__file__).parent
sys.path.insert(0, str(HERE))
import original  # noqa: E402

MAP = original.AddressMap()
# Modules whose addresses are MW3's and rewritten. `original` itself isn't.
MODULES = {'calltrace', 'twod_world', 'test_twod', 'test_twod_msgbox', 'test_lab_list', 'test_lab_screen', 'test_menus',
           'lab_weapons', 'test_pilot', 'test_salvage'}
BY_NAME = set()
missing = {}


# zTwoD's vtables the map lacks, read from each pair of constructors.
# (pm-diff/addrmap.json's data section matches vtables by contents and is often wrong; tools/analysis/pmvtables.py.)
VTABLES = {int(a, 16): int(b, 16) for a, b in json.loads(
    (HERE.parents[1] / 'analysis-output' / 'pm-diff' / 'vtables.json').read_text()).items()}
# The lab screen's constructor makes one control fewer in line (its writes don't pair by count):
VTABLES.update({0x5c1630: 0x5e3d58, 0x5c1588: 0x5e3ca8, 0x5c14f0: 0x5e3c10, 0x5c1448: 0x5e3b68, 0x5c13b0: 0x5e3ad4,
                0x5c1300: 0x5e3a20, 0x5c1250: 0x5e396c, 0x5c11c0: 0x5e38e0, 0x5c1130: 0x5e3854, 0x5c1110: 0x5e3830,
                0x5c1080: 0x5e37a0, 0x5c0ff0: 0x5e3714})


# zVideo's pixel format: its neighbours, which the map's block delta gets wrong.
FORMAT = {0x802738: 0xb0619c, 0x80273c: 0xb061a0, 0x802740: 0xb061a4, 0x802744: 0xb061a8, 0x802748: 0xb061ac,
          0x80274c: 0xb061b0}


# zTwoD's classes in PM: a text object has two more words before +0x4ac (0x5a0790's measure), and
# so everything built on it; the dialog is longer. Found by aligning the paired functions.
FIELDS = {'test_twod', 'test_twod_msgbox', 'test_lab_list', 'test_lab_screen'}


# A lab row holds three text objects after its own, each 8 longer: what follows moves by each.
LAB_ROW = {0xaa8: 0xab8, 0xf74: 0xf8c, 0x1440: 0x1460, 0x148c: 0x14ac}


def field(v):
    if v < 0x1000:
        return v + 8
    if v < 0x10000:
        return v + 0x70
    if v < 0x11100:
        return v + 0x190
    if v < 0x11900:
        return v + 0x198
    return v + 0x1a0


# The lab screen's own members (0x44cec0 against 0x44b920, by twin instructions), and in its controls
# only what moved: rows and their texts 8 further, CONFIG_EDIT's text 0x300 longer, a file list's font 4 further.
LAB_MEMBERS = [(0x10fe0, 0x190), (0x111ec, 0x198), (0x11708, 0x19c), (0x16e74, 0x1fc), (0x17548, 0x208),
               (0x17ca8, 0x20c), (0x18028, 0x50c), (0x1a570, 0x510), (0x1a774, 0x514)]
LAB_SMALL = {0x4ac, 0x4b0, 0x4b4, 0x4cc, 0x4d0, 0x5dc, 0x64c}
LAB_TEXT = [('ed + 0x374', 'ed + 0x674'), ('ed + 0x38c', 'ed + 0x68c'), ('ed + 0x390', 'ed + 0x690'),
            ('msg + 0x4b0', 'msg + 0x4d4'), ('msg + 0x4b4', 'msg + 0x4d8'), ('e.w32(MECH, 0x13)', 'e.w32(MECH, 0x19)'), ('e.world_alloc(0x200), e.world_alloc(0x40)', 'e.world_alloc(0x210), e.world_alloc(0x40)'),
            ('obj + 0x1f0', 'obj + 0x200'),  # the weapon's lab block (0x00486d90)
            ('fp = e.world_alloc(0x20)', 'fp = e.world_alloc(0x80)'), ('e.w32(fp + 0xc, part)', 'e.w32(fp + 0xc, part); e.w32(fp + 0x68, part)'),  # (0x00475260)
            # PM's: the second campaign's game (5), and its mechs past MW3's 19.
            ("'kind': r.choice([1, 1, 2, 3, 0])", "'kind': r.choice([1, 1, 2, 3, 0, 5, 5])"),
            ("for mech in range(-1, 19):", "for mech in range(-1, 25):"), ('r.randrange(19)', 'r.randrange(25)'),
            ("r.choice([0, 3, 7, 18, -1])", "r.choice([0, 3, 7, 18, 19, 22, 24, -1])"),
            ('w + 0x200', 'w + 0x204'),
            ('self.cfg_vt + 0x14', 'self.cfg_vt + 0x10'),
            ('ch + 0x218', 'ch + 0x21c'), ('ch + 0x214', 'ch + 0x218'), ('ch + 0x6d0', 'ch + 0x6dc')]


def lab_field(v):
    if v == 0x1b468:  # the screen's size
        return 0x1b9a0
    if v >= 0x10fe0:
        return v + [d for lo, d in LAB_MEMBERS if v >= lo][-1]
    return v + 8 if v in LAB_SMALL else v


def slot_of(vt, slot):
    """A virtual call's slot in MW3's class as the same function's slot in PM's vtable `vt` (PM's
    classes gained and lost some); a function the map doesn't pair moves as the next one that it does."""
    if (vt, slot) not in _slots:
        back = next((a for a, b in VTABLES.items() if b == vt), None)
        found = slot
        for later in range(slot, slot + 0x40, 4):
            want = MAP_JSON['functions'].get(hex(_mw_read(back + later))) if back else None
            at = next((i * 4 for i in range(80) if want and _pm_read(vt + i * 4) == int(want, 16)), None)
            if at is not None:
                found = at - later + slot
                break
        _slots[vt, slot] = found
    return _slots[vt, slot]


def _reader(path):
    data = path.read_bytes()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    n, opt = struct.unpack_from('<H', data, pe + 6)[0], struct.unpack_from('<H', data, pe + 20)[0]
    base = struct.unpack_from('<I', data, pe + 52)[0]
    secs = [struct.unpack_from('<IIII', data, pe + 24 + opt + 40 * i + 8) for i in range(n)]

    def read(a):
        for _, va, size, raw in secs:
            if base + va <= a < base + va + size:
                return struct.unpack_from('<I', data, raw + a - base - va)[0]
        return 0
    return read


_slots = {}
MAP_JSON = json.loads((HERE.parents[1] / 'analysis-output' / 'pm-diff' / 'addrmap.json').read_text())
_mw_read, _pm_read = (_reader(HERE.parents[1] / 'game' / x / 'Mech3.exe') for x in ('mw3-1.2', 'pm'))


def number(s, name):
    """A literal in the image's range as its twin in PM, a moved field as where it moved."""
    v = int(s, 16)
    if name in ('test_lab_list', 'test_lab_screen') and v in LAB_ROW:
        return hex(LAB_ROW[v])
    if name == 'test_lab_screen' and 0x4ac <= v < 0x1c000:
        return hex(lab_field(v))
    if name in FIELDS and 0x4ac <= v < 0x13000 and not (0x1000 <= v < 0x4000) and v != 0xffff:
        return hex(field(v))
    if v in VTABLES:
        return hex(VTABLES[v])
    if v in FORMAT:
        return hex(FORMAT[v])
    if 0x400000 <= v < 0x900000:
        try:
            return hex(MAP(v))
        except original.Unmapped:
            missing.setdefault(name, set()).add(v)  # left: it raises if the test really uses it
    return s


def rewrite(text, name):
    if name == 'test_lab_screen':
        for old, new in LAB_TEXT:
            text = text.replace(old, new)
    out = []
    for tok in tokenize.generate_tokens(io.StringIO(text).readline):
        s = tok.string
        if tok.type == tokenize.NUMBER and s.lower().startswith('0x'):
            s = number(s, name)
        if tok.type == tokenize.STRING and s[:2].lower() in ('f"', "f'", 'rf', 'fr'):
            s = re.sub(r'0x[0-9a-fA-F]+', lambda m: number(m.group(0), name), s)
        out.append((tok.type, s, tok.start, tok.end, tok.line))
    return tokenize.untokenize([(a, b) for a, b, *_ in out])


class Loader(importlib.machinery.SourceFileLoader):
    def source_to_code(self, data, path, **k):
        return compile(rewrite(data.decode('utf-8'), Path(path).stem), path, 'exec', dont_inherit=True)

    def get_code(self, fullname):  # no .pyc: its source is rewritten
        return self.source_to_code(self.get_data(self.get_filename(fullname)), self.get_filename(fullname))


class Finder(importlib.abc.MetaPathFinder):
    def find_spec(self, name, path=None, target=None):
        if name in MODULES:
            p = HERE / f'{name}.py'
            return importlib.util.spec_from_file_location(name, str(p), loader=Loader(name, str(p)))
        return None


def main():
    sys.meta_path.insert(0, Finder())
    name = sys.argv[1]
    sys.argv = [str(HERE / f'{name}.py')] + sys.argv[2:]
    m = importlib.import_module(name)
    code = m.main() if hasattr(m, 'main') else 0
    sys.stdout.flush()
    os._exit(code or 0)


if __name__ == '__main__':
    main()
