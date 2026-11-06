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


def rewrite(text, name):
    out = []
    for tok in tokenize.generate_tokens(io.StringIO(text).readline):
        s = tok.string
        if tok.type == tokenize.NUMBER and s.lower().startswith('0x'):
            v = int(s, 16)
            if 0x400000 <= v < 0x900000:
                try:
                    s = hex(MAP(v))
                except original.Unmapped:
                    missing.setdefault(name, set()).add(v)  # left: it raises if the test really uses it
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
