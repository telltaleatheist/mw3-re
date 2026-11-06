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
import os
import re
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
           'test_mechsave', 'test_zreader', 'test_pilot'}
BY_NAME = set()
missing = {}


# zTwoD's vtables the map lacks, read from each pair of constructors.
VTABLES = {0x5cf948: 0x5f1d9c, 0x5cf9d8: 0x5f1e28, 0x5d0e28: 0x5f30f4, 0x5be308: 0x5e0c34, 0x5cfd28: 0x5f215c,
           0x5d06e0: 0x5f29e8}


# zVideo's pixel format: its neighbours, which the map's block delta gets wrong.
FORMAT = {0x802738: 0xb0619c, 0x80273c: 0xb061a0, 0x802740: 0xb061a4, 0x802744: 0xb061a8, 0x802748: 0xb061ac,
          0x80274c: 0xb061b0}


# zTwoD's classes in PM: a text object has two more words before +0x4ac (0x5a0790's measure), and
# so everything built on it; the dialog is longer. Found by aligning the paired functions.
FIELDS = {'test_twod', 'test_twod_msgbox', 'test_lab_list', 'test_lab_screen'}


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


def number(s, name):
    """A literal in the image's range as its twin in PM, a moved field as where it moved."""
    v = int(s, 16)
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
