"""The hooks the ported code calls through a function pointer or a
std::function field (a struct of them, e.g. zeff::AnimRunHooks), and which
of them the program (src/, outside tests/) ever sets. A field nothing sets
is a call the program skips (or a default it returns) without the census
seeing it.

  python3 tools/analysis/hookaudit.py            # the table, unset ones marked
  python3 tools/analysis/hookaudit.py --unset    # only the unset fields
  python3 tools/analysis/hookaudit.py --inc F    # also write the C++ list for the test

A field counts as set when some source outside tests/ assigns it
(`.f =`, `->f =`, `f = [` in an aggregate) in a file that names its struct,
its accessor or a variable of it. Field names shared by several structs make
this an over-count of "set", never an under-count of the fields.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / 'src'

FIELD = re.compile(r'^\s*(?:[\w:<>,\*&\s]+?)\(\s*\*\s*(\w+)\s*\)\s*\(.*?\)\s*(?:=\s*nullptr\s*)?;', re.S)
STDFN = re.compile(r'^\s*std::function<.*>\s+(\w+)\s*(?:=\s*nullptr\s*)?;')
OPEN = re.compile(r'^\s*(?:struct|class)\s+(\w+)\b[^;]*\{')


def structs():
    """(namespace-less struct name, file, [fields]) for each struct with
    function-pointer or std::function fields."""
    out = []
    for h in sorted(SRC.rglob('*.h')):
        text = h.read_text(errors='replace')
        lines = text.split('\n')
        stack = []  # (name, depth at open, fields)
        depth = 0
        buf = ''
        for line in lines:
            code = line.split('//')[0]
            m = OPEN.match(code)
            if m:
                stack.append([m.group(1), depth, []])
            if stack:
                buf = (buf + ' ' + code) if buf else code
                if ';' in code or '{' in code or '}' in code:
                    for rx in (FIELD, STDFN):
                        f = rx.match(buf)
                        if f and depth == stack[-1][1] + 1:
                            stack[-1][2].append(f.group(1))
                    buf = ''
            depth += code.count('{') - code.count('}')
            while stack and depth <= stack[-1][1]:
                name, _, fields = stack.pop()
                if fields:
                    out.append((name, h.relative_to(ROOT).as_posix(), fields))
    return out


def sources():
    files = {}
    for p in SRC.rglob('*'):
        if p.suffix in ('.cpp', '.h') and 'tests' not in p.parts:
            files[p.relative_to(ROOT).as_posix()] = p.read_text(errors='replace')
    return files


def accessor(name, files):
    rx = re.compile(r'\b' + name + r'\s*&\s*(\w+)\s*\(\s*\)')
    for f, t in files.items():
        m = rx.search(t)
        if m:
            return m.group(1)
    return None


NS_OPEN = re.compile(r'^namespace\s+([\w:]+)\s*\{')
NS_CLOSE = re.compile(r'^\}\s*//\s*namespace\s+([\w:]+)')


def declared(name, files, header=None):
    """(header, qualified accessor) of `Name& acc();` declared at namespace
    scope in a header, or None."""
    rx = re.compile(r'^(?:const\s+)?' + name + r'\s*&\s*(\w+)\s*\(\s*\)\s*;')
    order = sorted(files.items(), key=lambda ft: (ft[0] != header, ft[0]))  # its own header first
    for f, t in order:
        if not f.endswith('.h'):
            continue
        ns, depth = [], 0  # (name, the depth inside it)
        for line in t.split('\n'):
            code = line.split('//')[0]
            m = NS_OPEN.match(code)
            m2 = rx.match(code)
            before = depth
            depth += code.count('{') - code.count('}')
            if m and depth > before:  # not a one-line `namespace x { struct y; }`
                ns.append((m.group(1), depth))
            while ns and depth < ns[-1][1]:
                ns.pop()
            m = m2
            if m and (not ns or depth == ns[-1][1]):
                return f, '::'.join([n for n, _ in ns] + [m.group(1)])
    return None


def audit():
    files = sources()
    rows = []
    for name, header, fields in structs():
        acc = accessor(name, files)
        names = [name] + ([acc] if acc else [])
        related = [f for f, t in files.items() if any(re.search(r'\b' + n + r'\b', t) for n in names)]
        for fld in fields:
            rx = re.compile(r'(?:\.|->)\s*' + fld + r'\s*=(?!=)|\b' + fld + r'\s*=\s*\[|\.' + fld + r'\s*=\s*&')
            setters = [f for f in related if f != header and rx.search(files[f])]
            if not setters:  # set through a local reference of another name (h.x = in a wiring file)
                setters = [f for f in related if rx.search(files[f]) and f != header]
            rows.append((name, header, acc, fld, setters))
    return rows


def main():
    rows = audit()
    only_unset = '--unset' in sys.argv
    unset = [r for r in rows if not r[4]]
    for name, header, acc, fld, setters in rows:
        if only_unset and setters:
            continue
        mark = 'UNSET' if not setters else 'set  '
        print(f'{mark} {name}.{fld}  ({header}{", " + acc + "()" if acc else ""})'
              + (f'  <- {setters[0]}' if setters else ''))
    structs_n = len({(r[0], r[1]) for r in rows})
    print(f'{structs_n} structs, {len(rows)} fields, {len(unset)} unset', file=sys.stderr)
    if '--inc' in sys.argv:
        # The test's list: HOOK(accessor, field) for each field of a struct
        # with a namespace-scope accessor, and the headers that declare them.
        files = sources()
        path = Path(sys.argv[sys.argv.index('--inc') + 1])
        heads, lines, none = set(), [], set()
        for name, header, acc, fld, _ in rows:
            d = declared(name, files, header)
            if not d:
                none.add(name)
                continue
            heads.add(d[0]), heads.add(header)
            lines.append(f'HOOK({d[1]}, {fld}, "{name}.{fld}")\n')
        out = ['// Generated by tools/analysis/hookaudit.py --inc; do not edit.\n',
               '// Hooks structs without a namespace-scope accessor (not checked): '
               + ', '.join(sorted(none)) + '\n',
               '#ifdef HOOK_INCLUDES\n']
        out += [f'#include "{h[len("src/"):]}"\n' for h in sorted(heads)]
        out += ['#else\n'] + lines + ['#endif\n']
        path.write_text(''.join(out))
        print(f'{len(lines)} fields of {len(heads)} headers to {path}; no accessor: {len(none)} structs',
              file=sys.stderr)


if __name__ == '__main__':
    main()
