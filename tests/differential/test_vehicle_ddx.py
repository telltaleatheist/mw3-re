"""The vehicle editor's pages p1 and p2 (docs/editor.md, The vehicle editor's pages) against the original x86:
their DoDataExchange, 0x4b8bc0 and 0x4b8eb0, run in Unicorn with MFC's DDX_* and DDV_* calls (mfc42.dll, not in the
exe) stood in for by a model of MFC 4.2 here, on a table of controls; the port runs the same pass on its own MFC
stand-in (src/upgrades/editor/ddx.cpp). Random settings and controls, both directions: what the fields become, the
controls' texts, checks and limits, the controls exchanged in order, the list fills called, a failing field's
control and message. Then the conversions alone on hand-picked texts (MSVC's "%.6g" exponents, strtod's 'd'
exponent, "+0" and "-0" refused, the 31-character buffer, %u's '-').

    cmake --build build --target mw3probe && .venv/bin/python -P tests/differential/test_vehicle_ddx.py [runs]

Breaks, by hand, each must fail: p2's x and z swapped (1200 on +0x10); the Lancemate's bit 1 kept when unchecked;
ddx_text(float) without MFC's 31-character cut; DDV_MinMaxInt's top 101.
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import load_probe  # noqa: E402
from original import Original  # noqa: E402

P1, P2 = 0x4b8bc0, 0x4b8eb0
EDITOR_SLOT = 0x60b310
SEND_MESSAGE = 0x5bb734  # SendMessageA's import slot
TAILS = [0x4bc790, 0x4bc570, 0x4bc930, 0x4bc8f0, 0x4bc750, 0x4bc4e0]  # the list fills and gathers
SET_PILOT = 0x49d250
# MFC's imports, by their thunks in the exe.
DDX_CONTROL, DDX_RADIO, DDX_TEXT_UINT, DDX_TEXT_FLOAT, DDX_TEXT_INT = 0x5abd86, 0x5abff0, 0x5abff6, 0x5abd80, 0x5abfea
DDX_CHECK, DDX_TEXT_STR, DDV_MAXCHARS, DDV_MINMAX, CSTR_ASSIGN = 0x5abdce, 0x5abdc2, 0x5ac04a, 0x5ac044, 0x5abdbc

P1_EDITS = [1209, 1210, 1211, 1212, 1214]
P1_CHECKS = {1231: 1, 1217: 1, 1251: 1, 1257: 2}
P2_EDITS = [1111, 1200, 1201, 1202, 1203, 1117, 1243, 1284]
P2_CHECKS = {1249: 2, 1275: 1, 1276: 1, 1279: 1, 1282: 1, 1283: 2}
RADIO = 1197


class Ctl(ctypes.Structure):
    _fields_ = [('id', ctypes.c_int32), ('check', ctypes.c_int32), ('limit', ctypes.c_int32), ('text', ctypes.c_char * 64)]


class State(ctypes.Structure):
    _fields_ = [('params', ctypes.c_uint8 * 0xd0), ('show', ctypes.c_int32), ('show_inout', ctypes.c_int32),
                ('team_side', ctypes.c_int32), ('keep4', ctypes.c_int32), ('objective', ctypes.c_int32),
                ('shutdown', ctypes.c_int32), ('pilot_null', ctypes.c_int32), ('pilot', ctypes.c_char * 64),
                ('p2_pilot', ctypes.c_char * 64), ('range_to', ctypes.c_int32), ('ctl', Ctl * 40),
                ('n_ctl', ctypes.c_int32), ('ids', ctypes.c_int32 * 64), ('n_ids', ctypes.c_int32),
                ('calls', ctypes.c_uint32 * 16), ('n_calls', ctypes.c_int32), ('ok', ctypes.c_int32),
                ('fail_id', ctypes.c_int32), ('message', ctypes.c_char * 96)]


probe = load_probe()
probe.probe_vehicle_ddx.argtypes = [ctypes.c_int32, ctypes.c_int32, ctypes.POINTER(State)]
probe.probe_ddx_float_text.argtypes = [ctypes.c_float, ctypes.c_char_p]
probe.probe_ddx_parse_float.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_double)]
probe.probe_ddx_parse_int.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.POINTER(ctypes.c_int32)]


# --- MFC 4.2, as dlgdata.cpp has it (and MSVCRT's printf and strto*) -------------------------------------------
def float_text(f):
    if f != f:
        b = struct.unpack('<I', struct.pack('<f', f))[0]
        if not b & 0x00400000:
            return '-1.#SNAN' if b >> 31 else '1.#SNAN'
        return '-1.#IND' if b == 0xffc00000 else ('-1.#QNAN' if b >> 31 else '1.#QNAN')
    if f in (float('inf'), float('-inf')):
        return '1.#INF' if f > 0 else '-1.#INF'
    s = '%.6g' % f
    if 'e' in s:
        m, e = s.split('e')
        s = m + 'e' + e[0] + e[1:].rjust(3, '0')
    return s


C_SPACE = ' \t\n\v\f\r'


def msvc_strtod(s, at):
    """(value, end) as MSVCRT's strtod: whitespace, sign, digits, '.', an exponent marked e, E, d or D."""
    i = at
    while i < len(s) and s[i] in C_SPACE:
        i += 1
    t = ''
    if i < len(s) and s[i] in '+-':
        t += s[i]
        i += 1
    digits = 0
    while i < len(s) and s[i].isdigit():
        t += s[i]
        i += 1
        digits += 1
    if i < len(s) and s[i] == '.':
        t += '.'
        i += 1
        while i < len(s) and s[i].isdigit():
            t += s[i]
            i += 1
            digits += 1
    if not digits:
        return 0.0, at
    if i < len(s) and s[i] in 'eEdD':
        j = i + 1
        x = 'e'
        if j < len(s) and s[j] in '+-':
            x += s[j]
            j += 1
        if j < len(s) and s[j].isdigit():
            while j < len(s) and s[j].isdigit():
                x += s[j]
                j += 1
            t += x
            i = j
    if t in ('+.', '-.', '.'):
        return 0.0, at
    try:
        return float(t), i
    except OverflowError:
        return float('inf'), i


def strto32(s, at, unsigned):
    i = at
    while i < len(s) and s[i] in C_SPACE:
        i += 1
    sign = ''
    if i < len(s) and s[i] in '+-':
        sign = s[i]
        i += 1
    if i >= len(s) or not s[i].isdigit():
        return 0, at
    j = i
    while j < len(s) and s[j].isdigit():
        j += 1
    v = int(s[i:j])
    if unsigned:
        if v > 0xffffffff:
            return -1, j
        v = (-v if sign == '-' else v) & 0xffffffff
        return v - (1 << 32) if v >> 31 else v, j
    v = -v if sign == '-' else v
    return max(-0x80000000, min(0x7fffffff, v)), j


def skip_st(s, i):
    while i < len(s) and s[i] in ' \t':
        i += 1
    return i


def parse_float(s):
    i = skip_st(s, 0)
    first = s[i] if i < len(s) else ''
    d, end = msvc_strtod(s, i)
    if d == 0.0 and first != '0':
        return None
    return d if skip_st(s, end) == len(s) else None


def parse_int(s, unsigned):
    i = skip_st(s, 0)
    first = s[i] if i < len(s) else ''
    if unsigned and first == '-':
        return None
    v, end = strto32(s, i, unsigned)
    if v == 0 and first != '0':
        return None
    return v if skip_st(s, end) == len(s) else None


class Failed(Exception):
    pass


class Mfc:
    """MFC's DDX on a dict of controls, for the exe: {id: [text, check, limit]}."""

    def __init__(self, o, ctl, max_check, radios):
        self.o, self.ctl, self.max_check, self.radios = o, ctl, max_check, radios
        self.ids, self.last, self.message, self.calls, self.pilot_set = [], 0, '', [], []

    def prep(self, i):
        self.last = i
        self.ids.append(i)
        return self.ctl.setdefault(i, ['', 0, 0])

    def fail(self, text):
        self.message = text
        raise Failed()


def cstring(o, s):
    b = s.encode('latin-1')
    a = o.alloc(12 + len(b) + 1)
    o.write(a, struct.pack('<iii', 1, len(b), len(b)) + b + b'\0')
    return a + 12


def cstr_read(o, p):
    n = struct.unpack('<i', o.read(p - 8, 4))[0]
    return o.read(p, n).decode('latin-1')


def c_str(o, p):
    out = b''
    while True:
        c = o.read(p, 1)
        if c == b'\0':
            return out.decode('latin-1')
        out += c
        p += 1


def setup(o, m):
    """MFC's calls for the run in hand (m, swapped per run by m_box[0])."""
    def save():
        return struct.unpack('<i', o.read(o.arg(0), 4))[0] != 0

    def num(kind):
        def fn(o):
            mm = m[0]
            dx, i, p = o.arg(0), o.arg(1), o.arg(2)
            c = mm.prep(i)
            fmt = {'f': '<f', 'i': '<i', 'u': '<I'}[kind]
            if not save():
                v = struct.unpack(fmt, o.read(p, 4))[0]
                c[0] = float_text(v) if kind == 'f' else str(v)
                return 0
            text = c[0][:31]
            if kind == 'f':
                d = parse_float(text)
                if d is None:
                    mm.fail('Please enter a number.')
                try:
                    o.write(p, struct.pack('<f', d))
                except OverflowError:
                    o.write(p, struct.pack('<f', float('inf') if d > 0 else float('-inf')))
            else:
                v = parse_int(text, kind == 'u')
                if v is None:
                    mm.fail('Please enter a positive integer.' if kind == 'u' else 'Please enter an integer.')
                o.write(p, struct.pack('<i', v))
            return 0
        return fn

    def check(o):
        mm = m[0]
        i, p = o.arg(1), o.arg(2)
        c = mm.prep(i)
        if save():
            o.write(p, struct.pack('<i', c[1]))
            return 0
        v = struct.unpack('<i', o.read(p, 4))[0]
        if v < 0 or v > 2:
            v = 0
            o.write(p, struct.pack('<i', 0))
        c[1] = min(v, mm.max_check.get(i, 1))
        return 0

    def radio(o):
        mm = m[0]
        i, p = o.arg(1), o.arg(2)
        c = mm.prep(i)
        n = mm.radios.get(i, 0)
        if save():
            o.write(p, struct.pack('<i', c[1] if 0 <= c[1] < n else -1))
        else:
            v = struct.unpack('<i', o.read(p, 4))[0]
            c[1] = v if 0 <= v < n else -1
        return 0

    def text_str(o):
        mm = m[0]
        i, p = o.arg(1), o.arg(2)
        c = mm.prep(i)
        if save():
            o.u32(p, cstring(o, c[0]))
        else:
            c[0] = cstr_read(o, struct.unpack('<I', o.read(p, 4))[0])
        return 0

    def max_chars(o):
        mm = m[0]
        p, n = o.arg(1), o.arg(2)
        s = cstr_read(o, struct.unpack('<I', o.read(p, 4))[0])
        if save() and len(s) > n:
            mm.fail('Please enter no more than %d characters.' % n)
        mm.ctl[mm.last][2] = n
        return 0

    def min_max(o):
        mm = m[0]
        v, lo, hi = [struct.unpack('<i', struct.pack('<I', o.arg(k)))[0] for k in (1, 2, 3)]
        if save() and (v < lo or v > hi):
            mm.fail('Please enter an integer between %d and %d.' % (lo, hi))
        return 0

    def assign(o):
        p = o.arg(0)
        o.u32(o.ecx, cstring(o, c_str(o, p) if p else ''))
        return 0

    def tail(addr):
        def fn(o):
            m[0].calls.append(addr)
            return 0
        return fn

    def set_pilot(o):
        p = o.arg(0)
        m[0].pilot_set.append((o.ecx, c_str(o, p) if p else None))
        return 0

    def send(o):
        mm = m[0]
        msg, wp = o.arg(1), o.arg(2)
        if msg == 0x14e:  # CB_SETCURSEL
            mm.range_to = wp if wp < 2 else -1
            return mm.range_to
        if msg == 0x147:  # CB_GETCURSEL
            return mm.range_to & 0xffffffff
        raise RuntimeError('SendMessage %#x' % msg)

    o.stub_function(DDX_CONTROL, lambda o: 0, 12)
    o.stub_function(DDX_TEXT_FLOAT, num('f'), 12)
    o.stub_function(DDX_TEXT_INT, num('i'), 12)
    o.stub_function(DDX_TEXT_UINT, num('u'), 12)
    o.stub_function(DDX_CHECK, check, 12)
    o.stub_function(DDX_RADIO, radio, 12)
    o.stub_function(DDX_TEXT_STR, text_str, 12)
    o.stub_function(DDV_MAXCHARS, max_chars, 12)
    o.stub_function(DDV_MINMAX, min_max, 16)
    o.stub_function(CSTR_ASSIGN, assign, 4)
    for a in TAILS:
        o.stub_function(a, tail(a))
    o.stub_function(SET_PILOT, set_pilot, 4)
    o.stub_import(SEND_MESSAGE, send, 16)


# --- random states ---------------------------------------------------------------------------------------------
FLOAT_WORDS = {0x08, 0x0c, 0x10, 0x14, 0x1c, 0x20, 0x24, 0x28, 0x2c}


def rand_float(rng):
    k = rng.randrange(8)
    if k == 0:
        return 0.0
    if k == 1:
        return -0.0
    if k == 2:
        return rng.choice([1e-7, 123456789.0, 1e12, 3.4e38, -2.5e-12, 0.1])
    if k == 3:
        return round(rng.uniform(-500, 500), rng.randrange(4))
    return rng.uniform(-5000, 5000)


def rand_params(rng):
    b = bytearray(0xd0)
    for off in range(0x04, 0xcc, 4):
        if off in FLOAT_WORDS:
            struct.pack_into('<f', b, off, rand_float(rng))
        else:
            struct.pack_into('<i', b, off, rng.choice([0, 1, 2, -1, 3, 5, rng.randrange(-100, 200)]))
    b[0xcc] = rng.randrange(256)
    b[0xcd] = rng.randrange(256)
    return b


TEXTS = ['0', '1', '-1', '+5', '+0', '-0', '0.5', '.5', '-.5', '1e5', '1d5', '1E-3', '1e', '1e+', ' 12 ', '\t7\t',
         '\n9', '12abc', 'abc', '', ' ', '00', '0x10', '4294967295', '4294967296', '2147483648', '-2147483649',
         '1.#INF', '3.4e39', '-5', ' -5', '\n-5', '1' * 40, '0' * 35 + '7', '1234567.875', '99', '100', '101']


def rand_text(rng, odd):
    """A field's text: one in `odd` is odd (a hand-picked or random string), the rest numbers."""
    if rng.randrange(odd) == 0:
        if rng.randrange(2):
            return rng.choice(TEXTS)
        return ''.join(rng.choice('0123456789 .-+eda') for _ in range(rng.randrange(1, 8)))
    if rng.randrange(2):
        return str(rng.randrange(0, 120))
    return float_text(struct.unpack('<f', struct.pack('<f', rand_float(rng)))[0])


def rand_state(rng, page):
    edits, checks = (P1_EDITS, P1_CHECKS) if page == 0 else (P2_EDITS, P2_CHECKS)
    ctl = {}
    odd = rng.choice([3, 30])  # many fields odd, or one now and then
    for i in edits:
        ctl[i] = [rand_text(rng, odd), 0, 0]
    if page == 1:
        ctl[1243] = [''.join(rng.choice('abcXYZ ') for _ in range(rng.choice([0, 3, 12, 31, 32, 32, 33, 40]))), 0, 0]
    for i, mx in checks.items():
        ctl[i] = ['', rng.randrange(mx + 1), 0]
    if page == 1:
        ctl[RADIO] = ['', rng.randrange(-1, 3), 0]
    return dict(params=rand_params(rng), show=rng.choice([0, 1, 2, 3, -1]), show_inout=rng.randrange(2),
                team_side=rng.choice([-1, 0, 1, 2, 5]), keep4=rng.choice([0, 1, 2, 3, -2]),
                objective=rng.choice([0, 1, 2]), shutdown=rng.choice([0, 1, 7]),
                pilot=rng.choice([None, '', 'Dom', 'x' * 40]), p2_pilot=rng.choice(['', 'Alan', 'y' * 33]),
                range_to=rng.choice([-1, 0, 1]), ctl=ctl)


def run_port(page, save, st):
    s = State()
    for i, v in enumerate(st['params']):
        s.params[i] = v
    s.show, s.show_inout, s.team_side, s.keep4 = st['show'], st['show_inout'], st['team_side'], st['keep4']
    s.objective, s.shutdown = st['objective'], st['shutdown']
    s.pilot_null = 1 if st['pilot'] is None else 0
    s.pilot = (st['pilot'] or '').encode('latin-1')
    s.p2_pilot = st['p2_pilot'].encode('latin-1')
    s.range_to = st['range_to']
    ids = sorted(st['ctl'])
    for k, i in enumerate(ids):
        s.ctl[k].id, s.ctl[k].text, s.ctl[k].check, s.ctl[k].limit = i, st['ctl'][i][0].encode('latin-1'), \
            st['ctl'][i][1], st['ctl'][i][2]
    s.n_ctl = len(ids)
    probe.probe_vehicle_ddx(page, save, ctypes.byref(s))
    return dict(params=bytes(s.params[4:0xd0]), show=s.show, show_inout=s.show_inout, team_side=s.team_side,
                keep4=s.keep4, objective=s.objective, shutdown=s.shutdown,
                pilot=None if s.pilot_null else s.pilot.decode('latin-1'), p2_pilot=s.p2_pilot.decode('latin-1'),
                range_to=s.range_to, ctl={s.ctl[k].id: [s.ctl[k].text.decode('latin-1'), s.ctl[k].check,
                                                        s.ctl[k].limit] for k in range(s.n_ctl)},
                ids=list(s.ids[:s.n_ids]), calls=list(s.calls[:s.n_calls]), ok=s.ok,
                fail=(s.fail_id, s.message.decode('latin-1')) if not s.ok else None)


def run_exe(o, m_box, page, save, st):
    mx = dict(P1_CHECKS) if page == 0 else dict(P2_CHECKS)
    mm = Mfc(o, {i: list(v) for i, v in st['ctl'].items()}, mx, {RADIO: 3})
    mm.range_to = st['range_to']
    m_box[0] = mm
    e = o.alloc(0x988)
    o.write(e, bytes(0x988))
    o.u32(EDITOR_SLOT, e)
    o.write(e + 0x64, st['params'])
    o.u32(e + 0x64 + 0xd4, 0 if st['pilot'] is None else cstring(o, st['pilot']) + 0)  # a plain char* will do
    o.u32(e + 0x1cc, st['show'])
    o.write(e + 0x1d1, bytes([st['show_inout']]))
    o.u32(e + 0x4dc, st['team_side'])
    o.u32(e + 0x984, st['keep4'])
    pg = o.alloc(0x80)
    o.write(pg, bytes(0x80))
    o.u32(pg + 0x60, cstring(o, st['p2_pilot']))
    o.u32(pg + 0x64, st['objective'])
    o.u32(pg + 0x68, st['shutdown'])
    dx = o.alloc(16)
    o.write(dx, struct.pack('<iiii', 1 if save else 0, pg, 0, 0))
    ok = 1
    try:
        o.call(P1 if page == 0 else P2, ecx=pg, stack_args=(dx,))
    except Exception:  # Fail(): MFC throws past the rest of the exchange
        if not isinstance(mm.message, str) or not mm.message:
            raise
        ok = 0
    params = bytearray(o.read(e + 0x64, 0xd0))
    pilot_p = struct.unpack('<I', o.read(e + 0x64 + 0xd4, 4))[0]
    pilot = None if not pilot_p else c_str(o, pilot_p)
    for ecx, s in mm.pilot_set:
        if ecx != e + 0x138:
            raise RuntimeError('0x49d250 on %#x' % ecx)
        pilot = s if s else None
    return dict(params=bytes(params[4:0xd0]), show=struct.unpack('<i', o.read(e + 0x1cc, 4))[0],
                show_inout=o.read(e + 0x1d1, 1)[0], team_side=struct.unpack('<i', o.read(e + 0x4dc, 4))[0],
                keep4=struct.unpack('<i', o.read(e + 0x984, 4))[0],
                objective=struct.unpack('<i', o.read(pg + 0x64, 4))[0],
                shutdown=struct.unpack('<i', o.read(pg + 0x68, 4))[0], pilot=pilot,
                p2_pilot=cstr_read(o, struct.unpack('<I', o.read(pg + 0x60, 4))[0]),
                range_to=mm.range_to, ctl=mm.ctl, ids=mm.ids, calls=mm.calls, ok=ok,
                fail=(mm.last, mm.message) if not ok else None)


def conversions():
    bad = 0
    for t in TEXTS + ['1.5d2', '  0', '0 x', '+', '-', '.', '1.e3', '\v3', '3\n']:
        d = ctypes.c_double()
        got = probe.probe_ddx_parse_float(t.encode('latin-1'), ctypes.byref(d))
        want = parse_float(t)
        if (got != 0) != (want is not None) or (got and d.value != want and not (d.value != d.value)):
            print('float %r: port %s %r, MFC %r' % (t, got, d.value, want))
            bad += 1
        for u in (0, 1):
            v = ctypes.c_int32()
            got = probe.probe_ddx_parse_int(t.encode('latin-1'), u, ctypes.byref(v))
            want = parse_int(t, u)
            if (got != 0) != (want is not None) or (got and v.value != want):
                print('int%s %r: port %s %d, MFC %r' % ('u' if u else '', t, got, v.value, want))
                bad += 1
    for f in [0.0, -0.0, 1e-7, 1e12, 1e-40, 3.4e38, 0.1, 123456.5, 1234567.0, float('inf'), float('-inf'),
              struct.unpack('<f', b'\x00\x00\xc0\xff')[0], struct.unpack('<f', b'\x00\x00\xc0\x7f')[0]]:
        buf = ctypes.create_string_buffer(64)
        probe.probe_ddx_float_text(f, buf)
        want = float_text(struct.unpack('<f', struct.pack('<f', f))[0])
        if buf.value.decode() != want:
            print('text %r: port %r, MFC %r' % (f, buf.value, want))
            bad += 1
    return bad


def main():
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 2000
    rng = random.Random(1999)
    o = Original()
    m_box = [None]
    setup(o, m_box)
    bad = conversions()
    fails = {0: 0, 1: 0}
    keys = ['params', 'show', 'show_inout', 'team_side', 'keep4', 'objective', 'shutdown', 'pilot', 'p2_pilot',
            'range_to', 'ctl', 'ids', 'calls', 'ok', 'fail']
    for n in range(runs):
        page, save = rng.randrange(2), rng.randrange(2)
        st = rand_state(rng, page)
        heap = o.heap_next
        want = run_exe(o, m_box, page, save, st)
        o.heap_next = heap
        got = run_port(page, save, st)
        if not want['ok']:
            fails[page] += 1
        for k in keys:
            w, g = want[k], got[k]
            if k == 'ctl':
                w = {i: v for i, v in w.items() if i in st['ctl']}
            if k == 'p2_pilot' and page == 0:
                continue
            if w != g:
                bad += 1
                if bad <= 10:
                    print('run %d page p%d %s: %s differs' % (n, page + 1, 'save' if save else 'show', k))
                    if k == 'params':
                        for off in range(0, len(w), 4):
                            if w[off:off + 4] != g[off:off + 4]:
                                print('  +%#x exe %s port %s' % (off + 4, w[off:off + 4].hex(), g[off:off + 4].hex()))
                    else:
                        print('  exe  %r\n  port %r' % (w, g))
                break
    if bad:
        print('FAIL: %d of %d runs differ' % (bad, runs))
        sys.exit(1)
    print('ok: %d page passes the same (%d stopped at a failed field), and the conversions' % (runs, fails[0] + fails[1]))


if __name__ == '__main__':
    main()
