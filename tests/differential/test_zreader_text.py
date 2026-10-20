"""The zReader's text files (0x5563d0: the balance check 0x5562a0, the lists
0x5563f0, the tokens 0x556510, their values 0x556690 and the #ifdef/#else/
#endif directives 0x557550) against the original x86 (docs/zreader.md, Text
files).

    cmake --build build && .venv/bin/python tests/differential/test_zreader_text.py

No text file ships with the game; the controls files the game writes are text
(docs/controls.md). Cases: controls files as the game writes them, then
random files of names, ints (some past 32 bits), floats (exponents e, E and
d), strings that only look like numbers, quoted strings (empty, with spaces,
parentheses and ';', in a token's middle), ';' comments (in a token's
middle too), CR LF and LF, nested and empty lists, unbalanced parentheses
(hidden in strings and comments, or not), tokens and quoted strings up to
255 characters (from 256 the original overwrites its return address; the
port's fix is checked in mw3tests), and
directives: nested #ifdef blocks with and without #else (nothing is defined),
unknown words, stray #endif lines (the depth carries from file to file, on
both sides). The MSVCRT pieces are stand-ins (text_crt.py). Compared: the
result and the whole tree, node by node, and the #ifdef depth after each file.
"""
import ctypes
import random
import struct
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import ROOT, Original  # noqa: E402
import text_crt  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
probe.probe_zrdr_text.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_int32)]
probe.probe_zrdr_dump.argtypes = [ctypes.c_char_p, ctypes.c_int]

PARSE_TEXT, DEPTH, POOL, DEFINES = 0x5563d0, 0x72c1ac, 0x555cd0, 0x557490
HANDLE = 0x77


class Emulated:
    def __init__(self):
        self.cpu = Original()
        self.files = {}
        self.crt = text_crt.install(self.cpu, self.files)
        self.cpu.call(POOL, ecx=64)  # the list elements' pool, made at start (0x480020)
        self.cpu.stub_import(0x5bb4bc, lambda c: c.alloc(max(c.arg(0), 4)))  # operator new
        self.cpu.call(DEFINES)  # the static constructor of the (empty) defines list
        self.base = self.cpu.heap_next

    def u32(self, a):
        return struct.unpack('<I', self.cpu.read(a, 4))[0]

    def parse(self, data, depth):
        cpu = self.cpu
        pass  # the heap is not reset: the list pool keeps elements across files
        self.files[HANDLE] = data
        self.crt['pos'][HANDLE] = 0
        cpu.u32(DEPTH, depth)
        cpu.call(PARSE_TEXT, ecx=HANDLE, count=20_000_000)
        root = cpu.uc.reg_read(UC_X86_REG_EAX)
        depth = struct.unpack('<i', cpu.read(DEPTH, 4))[0]
        if not root:
            return None, depth
        out = []
        self.walk(root, out)
        return ''.join(out), depth

    def walk(self, a, out):
        t, v = self.u32(a), self.u32(a + 4)
        if t == 1:
            out.append(f'I{struct.unpack("<i", struct.pack("<I", v))[0]} ')
        elif t == 2:
            out.append(f'F{v:08x} ')
        elif t == 3:
            s = self.crt['cstr'](v)
            out.append(f'S{len(s)}:' + s.decode('latin-1') + ' ')
        elif t == 4:
            count = struct.unpack('<i', struct.pack('<I', self.u32(v + 4)))[0]
            out.append(f'L{count}[')
            for i in range(1, count):
                self.walk(v + 8 * i, out)
            out.append('] ')
        else:
            out.append('? ')


def port(path, depth):
    d = ctypes.c_int32(depth)
    r = probe.probe_zrdr_text(str(path).encode(), ctypes.byref(d))
    if r < 0:
        raise SystemExit(f'{path}: not readable')
    if r == 0:
        return None, d.value
    buf = ctypes.create_string_buffer(1 << 20)
    if probe.probe_zrdr_dump(buf, len(buf)) < 0:
        raise SystemExit('dump too large')
    return buf.value.decode('latin-1'), d.value


NAMES = ['KEYS', 'AXES', 'CMD_TRIGGER1', 'keya', 'slope', 'Throttle', 'joystick', 'mouse', 'X', 'Rz',
         'a_b', 'detail', 'TRUE', 'name7']


def number(r):
    k = r.randrange(12)
    if k == 0:
        return str(r.randrange(-100, 100))
    if k == 1:
        return '+' + str(r.randrange(1000))
    if k == 2:
        return str(r.choice([2147483647, 2147483648, -2147483648, -2147483649, 99999999999]))
    if k == 3:
        return f'{r.uniform(-10, 10):.{r.randrange(0, 7)}f}'
    if k == 4:
        return r.choice(['.5', '-.25', '5.', '-0.0', '0.050000', '-0.500000'])
    if k == 5:
        return f'{r.randrange(1, 99)}{r.choice("eEdD")}{r.choice(["", "+", "-"])}{r.randrange(0, 12)}'
    if k == 6:
        return r.choice(['1e999', '-1e-999', '3.4e38'])
    if k == 7:
        return f'0x{r.randrange(0, 0x800):x}'
    if k == 8:
        return r.choice(['-', '+', '.', '12abc', '1.2.3', '0x', '-x', '1e', '1e+'])
    return str(r.randrange(0, 0x7de))


def token(r):
    k = r.randrange(10)
    if k < 4:
        return r.choice(NAMES)
    if k < 7:
        return number(r)
    if k == 7:
        body = ''.join(r.choice('ab ()\t;x1.') for _ in range(r.randrange(0, 8)))
        return '"' + body + '"'
    if k == 8:
        return r.choice(NAMES) + '"q ' + r.choice(['', ')', ';']) + '"' + r.choice(['', 'z'])
    return r.choice(NAMES) + ';inline comment\n' + r.choice(['tail', '5', ''])


def gap(r, nl):
    return r.choice([' ', '\t', nl, '  ', nl + '\t', ' ;note (\n'.replace('\n', nl)])


def block(r, depth, nl):
    """A list's body: tokens, nested lists and directive blocks."""
    out = []
    for _ in range(r.randrange(0, 6)):
        k = r.random()
        if k < 0.6:
            out.append(token(r))
        elif k < 0.85 and depth < 4:
            out.append('(' + block(r, depth + 1, nl) + ')')
        elif k < 0.95:
            inner = block(r, depth + 1, nl) if depth < 4 else token(r)
            d = nl + '#ifdef ' + r.choice(['DEBUG', 'X', 'name7']) + nl + inner
            if r.random() < 0.5:
                d += nl + '#else' + nl + (block(r, depth + 1, nl) if depth < 4 else token(r))
            d += nl + '#endif' + nl
            out.append(d)
        else:
            out.append(nl + r.choice(['#define X 1', '#pragma once', '#include foo']) + nl)
        out.append(gap(r, nl))
    return ''.join(out)


def random_file(r, n):
    nl = r.choice(['\n', '\r\n'])
    k = n % 10
    if k == 0:
        return b''
    if k == 1:
        body = '(' + block(r, 0, nl) + ')' + r.choice([')', '(', '"()"', ';)'])  # unbalanced or not
    elif k == 2:
        body = '(' + 'x' * r.randrange(240, 256) + ' "' + 'q' * r.randrange(240, 256) + '" ' + block(r, 1, nl) + ')'
    elif k == 3:
        body = nl + '#endif' + nl + '(' + block(r, 0, nl) + ')' + nl
    else:
        body = '(' + block(r, 0, nl) + ')' + nl
        if r.random() < 0.3:
            body += token(r) + ' ' + '(' + block(r, 1, nl) + ')'
    return body.encode('latin-1')


def controls_file(r):
    """A controls file as 0x411e60 writes it."""
    lines = ['(', '\tKEYS (']
    for name in sorted(r.sample(['CMD_TRIGGER1', 'CMD_JJ_UP', 'CMD_TURN_LEFT', 'CMD_MASC', 'CMD_UNKNOWN'], 3)):
        s = f'\t\t({name}'
        for k in ('keya', 'keyb', 'mousebtn', 'joybtn'):
            if r.random() < 0.5:
                s += f'\t{k}(0x{r.randrange(1, 0x7de):x})'
        lines.append(s + ')')
    lines += ['\t)', '\tAXES (']
    for ax in ('Throttle', 'Twist', 'Pitch', 'LR'):
        if r.random() < 0.8:
            lines.append(f'\t\t({ax}\t{r.choice(["joystick", "mouse"])}({r.choice(["X", "Y", "Z", "Rz", "Slider0"])})'
                         f'\tslope({r.uniform(-2, 2):f})\tintercept({r.uniform(-1, 1):f})\tdeadzone({r.uniform(0, 0.3):f}))')
    lines += ['\t)', ')', '']
    return '\n'.join(lines).encode()


def main():
    emu = Emulated()
    r = random.Random(20261020)
    depth = 0
    cases = trees = unbalanced = 0
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / 'case.zrd'
        datas = [controls_file(r) for _ in range(40)] + [random_file(r, n) for n in range(500)]
        for i, data in enumerate(datas):
            path.write_bytes(data)
            try:
                want, want_d = emu.parse(data, depth)
            except Exception as e:
                eip = emu.cpu.uc.reg_read(UC_X86_REG_EIP)
                raise SystemExit(f'case {i}: the original stopped at {eip:#x} ({e}) on {data[:400]!r}')
            got, got_d = port(path, depth)
            if want != got or want_d != got_d:
                print(f'case {i}: depth original {want_d} port {got_d}')
                print('file:', data[:600])
                print('original:', (want or 'None')[:800])
                print('port:    ', (got or 'None')[:800])
                raise SystemExit(1)
            depth = want_d
            cases += 1
            trees += want is not None
            unbalanced += want is None
    print(f'ok: {cases} text files ({trees} trees, {unbalanced} unbalanced) identical, '
          f'{emu.crt["printf"]} too-long tokens; #ifdef depth {depth} at the end on both sides')


if __name__ == '__main__':
    main()
