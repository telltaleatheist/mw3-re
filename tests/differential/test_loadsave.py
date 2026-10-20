"""The controls' load/save popup, its edit bar and file list against the original x86 (docs/menus.md).

    cmake --build build && .venv/bin/python -P tests/differential/test_loadsave.py [runs] [--verbose]

The original builds the popup's state 0x0042f230 (and with it the screen
0x0042e890 from dlgOptions.zrd's LOAD_SAVE_POPUP: zTwoD's edit bar
0x005a4ec0 as EDITBAR, the game's file list 0x0048c0e0 as FILELIST) for a
fake state, then runs a plan: the state's enter and leave halves (0x0042f2b0
/ 0x0042f340, the shell's parts stubbed) with a fake directory listing behind
FindFirstFileA / FindNextFileA (files and folders, some names without or
with two extensions), frames of mouse input over the rows, arrows, the bar
and the buttons through Window::update (hover, rollover, the arrows' repeat
while held), presses of OK (0x0042f160: its load or save, 0x0042f380, is a
stub), CANCEL, the bar and the list, characters typed through zInput's text
callback (0x0051a9a0 is a stub that keeps it: letters, the refused
characters, the editing keys, Enter and Escape, numeric bars), the bar's
text, the list's select, scroll, layout, show and hide; then a dump and the
state's destructor. Even runs use the shipped dialog, odd runs a synthetic
one (SIZE, SPACING, OFFSET, BITMAP, BAR, ITEMROLLOVER and FONT, the sounds,
the fonts, the arrows missing or moved; EDITBAR or FILELIST missing). The
port runs the same plan (tests/probe_loadsave.cpp) with the listing's files.
Compared: the call traces (the caret's lines, 0x00562940, included) and a
dump of the dialog, its objects, the bar, its line and caret, the list, its
items, rows, arrows and images. The x87 runs at 24 bits.

FIX (bug) in the port, applied to the original here so the rest compares: a
list with no row shown leaves its rectangle's bottom in a stack slot never
written (0x0048cda0); a code hook at 0x0048d06d writes the top there, as the
port does.
"""
import ctypes
import os
import random
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import Replay, compare, load_probe, Trace  # noqa: E402
from original import ROOT  # noqa: E402
from twod_world import IAT, READER, TwodWorld, entries, find, parse_node, write_node  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn.x86_const import UC_X86_REG_ESP  # noqa: E402
import test_twod  # noqa: E402
from test_twod import canonical, fbits  # noqa: E402
from test_twod_msgbox import archive  # noqa: E402
from test_menus import MECH3, SIZES, mutate_buttons  # noqa: E402

probe = load_probe()
probe.probe_app_set_callback.argtypes = [ctypes.c_void_p]
probe.probe_loadsave_run.argtypes = [ctypes.c_char_p, ctypes.c_uint32, ctypes.c_char_p, ctypes.c_char_p,
                                     ctypes.c_int]

ZRDR_LOAD = 0x556740
PARTS = [0x111ec, 0x113d8, 0x115c4, 0x11954]
NAMES = ['MB_OK', 'MB_CANCEL', 'EDITBAR', 'FILELIST']
BAR, FILES = 0x115c4, 0x11954
ACTIVE = ['keys\\Profile1.zrd', 'DefaultCtlConfig', 'C:\\games\\mw3\\keys\\pad.v2.zrd', '', 'keys/a_much_longer_name']
CHARS = [ord(c) for c in 'abcXYZ019_-.- '] + [ord(c) for c in ':|\\<>/?"*'] + \
    [0x08, 0x08, 0x7f, 0x01, 0x02, 0x04, 0x06, 0x02, 0x0d, 0x1b, 0x0a, 0xe9, 0x00]

test_twod.VT_KIND.update({v: 'C' for v in (0x5be8d8, 0x5be838, 0x5be7a0, 0x5be6f8)})


def i32(v):
    return struct.unpack('<i', struct.pack('<I', v & 0xffffffff))[0]


def hx(s):
    return s.encode('latin1').hex() or '00'[:0] or '-'


def unhx(h):
    return '' if h == '-' else bytes.fromhex(h).decode('latin1')


def splitpath(p):
    drive = p[:2] if len(p) >= 2 and p[1] == ':' else ''
    rest = p[len(drive):]
    i = max(rest.rfind('\\'), rest.rfind('/'))
    d, name = rest[:i + 1], rest[i + 1:]
    j = name.rfind('.')
    return drive, d, (name[:j] if j >= 0 else name), (name[j:] if j >= 0 else '')


# ---- dialogs ----

def mutate_list(r, entry):
    e = [list(x) if isinstance(x, list) else x for x in entry]

    def put(k, v):
        if k in e:
            e[e.index(k) + 1] = v
        else:
            e.extend([k, v])

    def drop(k):
        if k in e:
            i = e.index(k)
            del e[i:i + 2]
    if r.random() < 0.5:
        put('SIZE', [r.choice([0, 1, 2, 3, 5, 7, 10])])
    if r.random() < 0.6:
        put('SPACING', [r.choice([0, 3, -2, 6]), r.choice([10, 15, 22, 0])])
    if r.random() < 0.4:
        put('OFFSET', [r.randrange(-10, 20), r.randrange(-10, 30)])
    if r.random() < 0.3:
        put('POSITION', [r.randrange(150, 400), r.randrange(100, 300)])
    if r.random() < 0.25:
        put('BITMAP', ['listbg'])
    if r.random() < 0.5:
        put('BAR', ['BITMAP', ['selbar'], 'OFFSET', [r.randrange(-5, 5), r.randrange(-3, 3)]])
    if r.random() < 0.5:
        put('ITEMROLLOVER', ['BITMAP', [r.choice(['rollbar', 'cdbad'])], 'OFFSET', [r.randrange(-5, 5), 0]])
        if r.random() < 0.7:
            put('FONT', [r.choice(['ia_small_green', 'main_red_small', 'nofont'])])
    for k in ('ROLLOVER_SOUND', 'SELECT_SOUND'):
        if r.random() < 0.4:
            put(k, [r.choice(['menu_rollover', 'box_rollover', 'quiet'])])
    for k in ('NORMAL_FONT', 'SELECTED_FONT'):
        if r.random() < 0.15:
            drop(k)
    for k in ('UP', 'DOWN'):
        if r.random() < 0.15:
            drop(k)
        elif k in e and r.random() < 0.3:
            sub = list(e[e.index(k) + 1])
            if 'POSITION' in sub:
                sub[sub.index('POSITION') + 1] = [r.randrange(-30, 200), r.randrange(-20, 160)]
            e[e.index(k) + 1] = sub
    return e


def mutate_bar(r, entry):
    e = [list(x) if isinstance(x, list) else x for x in entry]
    if r.random() < 0.3 and 'POSITION' in e:
        e[e.index('POSITION') + 1] = [r.randrange(100, 400), r.randrange(50, 300)]
    if r.random() < 0.3 and 'LABEL' in e:  # one label, moved (the bar reads its first label unchecked)
        e[e.index('LABEL') + 1] = ['typed', 'offset', [r.randrange(-5, 30), r.randrange(-5, 15)], 'font',
                                   [r.choice(['ia_small_green', 'nofont'])]]
    return e


def synthetic(seed, tree):
    r = random.Random(f'loadsave:{seed}')
    body = [list(x) if isinstance(x, list) else x for x in find(tree, 'LOAD_SAVE_POPUP')]
    nb = []
    for i in range(0, len(body) - 1, 2):
        k, v = body[i], body[i + 1]
        if k == 'BUTTONS':
            out = []
            for j in range(0, len(v) - 1, 2):
                name, entry = v[j], v[j + 1]
                if name == 'FILELIST':
                    if r.random() < 0.08:
                        continue
                    out += [name, mutate_list(r, entry)]
                elif name == 'EDITBAR':
                    if r.random() < 0.08:
                        continue
                    out += [name, mutate_bar(r, entry)]
                else:
                    out += mutate_buttons(r, [name, entry], None)
            v = out
        elif k == 'PRIMITIVES' and r.random() < 0.2:
            v = []
        nb += [k, v]
    return [['SHARED_IMAGE_PATH', ['common'], 'LOAD_SAVE_POPUP', nb]]


def spots_of(body):
    out = []
    bl = find(body, 'BUTTONS') or []
    for i in range(0, len(bl) - 1, 2):
        name, e = bl[i], bl[i + 1]
        p = find(e, 'POSITION')
        if not p:
            continue
        out.append(tuple(p))
        if name == 'FILELIST':
            off = find(e, 'OFFSET') or [0, 0]
            sp = find(e, 'SPACING') or [0, 12]
            n = (find(e, 'SIZE') or [8])[0]
            for k in range(max(n, 1)):
                out.append((p[0] + off[0] + sp[0] + 3, p[1] + off[1] + sp[1] * k + 4))
            for key in ('UP', 'DOWN'):
                a = find(e, key)
                ap = find(a, 'POSITION') if a else None
                if ap:
                    out.append((p[0] + ap[0], p[1] + ap[1]))
    return out


# ---- the plan ----

def mouse_line(r, spots, state):
    sx, sy = r.choice(spots)
    if r.random() < 0.85:
        x, y = sx + r.randrange(-4, 40), sy + r.randrange(-4, 14)
    else:
        x, y = r.randrange(-20, 660), r.randrange(-20, 500)
    if state['down'] == 0:
        left = 1 if r.random() < 0.3 else 0
    elif state['down'] in (1, 2):
        left = 2 if r.random() < 0.75 else 4
    else:
        left = 0
    state['down'] = left
    right = r.choice([0, 0, 0, 0, 1, 4])
    dt = r.choice([0.0, 1 / 60, 1 / 30, 0.05, 0.2, 0.6])
    m = [x & 0xffffffff, y & 0xffffffff, 0, fbits(0.0), fbits(0.0), fbits(0.0), 0, 0, 0,
         fbits(0.0), fbits(0.0), fbits(0.0), left, right, 0]
    return 'F ' + ' '.join(f'{v:x}' for v in [fbits(dt)] + m)


def listing(r):
    words = ['Profile1', 'my set', 'DefaultCtlConfig', 'a.b', 'pad.v2', 'JOY', 'zz', 'MixedCase', 'x', 'long_name_of_a_set',
             'keys', 'Ab', 'ab', '0']
    out = []
    for _ in range(r.choice([0, 1, 2, 3, 5, 8, 12, 20])):
        w = r.choice(words) + (str(r.randrange(10)) if r.random() < 0.3 else '')
        ext = r.choice(['.zrd', '.zrd', '.zrd', '.ZRD', '', '.bak.zrd'])
        attr = 0x10 if r.random() < 0.12 else r.choice([0x20, 0x01, 0x80])
        out.append((w + ext, attr))
    if r.random() < 0.3:
        out.insert(0, ('.', 0x10))
    return out


def make_plan(seed, bound, spots):
    r = random.Random(f'plan:{seed}')
    lines = [f'active {hx(r.choice(ACTIVE))}', 'N', f'y {r.randrange(2):x}']
    state = {'down': 0}
    if r.random() < 0.85:
        ls = listing(r)
        lines.append(f'Y {len(ls):x} ' + ' '.join(f'{hx(n)} {a:x}' for n, a in ls))
        lines.append('A 1')  # the shell's enter (stubbed) activates the screen
    for _ in range(r.randrange(40, 120)):
        roll = r.random()
        if roll < 0.04:
            ls = listing(r)
            lines.append(f'Y {len(ls):x} ' + ' '.join(f'{hx(n)} {a:x}' for n, a in ls))
            lines.append('A 1')
        elif roll < 0.06:
            lines.append('Z')
            if r.random() < 0.5:
                lines.append('A 0')  # the shell's leave
        elif roll < 0.10 and 2 in bound:
            lines.append('P 2')  # the bar: the focus
        elif roll < 0.26:
            for _ in range(r.randrange(1, 6)):
                lines.append(f'K {r.choice(CHARS):x} {r.randrange(0x100):x}')
        elif roll < 0.29:
            lines.append(f'P {r.choice([k for k in (0, 1, 3) if k in bound] or [0]):x}')
        elif roll < 0.33 and 3 in bound:
            lines.append(f'S {r.randrange(0, 12):x}')
        elif roll < 0.36:
            lines.append(f'W {r.choice([-1, 1, 1, 3, -5]) & 0xffffffff:x}')
        elif roll < 0.39:
            lines.append(f'T {hx(r.choice(["", "abc", "Profile1", "a_name_longer_than_twenty", "x.y"]))}')
        elif roll < 0.41:
            lines.append('U')
        elif roll < 0.42:
            lines.append('c')
        elif roll < 0.45:
            lines.append(f'X {r.randrange(2):x}')
        elif roll < 0.48:
            lines.append(f'{r.choice("hv")} {r.randrange(2):x}')
        elif roll < 0.50:
            lines.append('G')
        elif roll < 0.51 and 3 in bound:
            lines.append('R')
        elif roll < 0.52:
            lines.append(r.choice('DH'))
        elif roll < 0.53:
            lines.append(f'A {r.randrange(2):x}')
        if r.random() < 0.12:  # a dwell: the button held down on one spot for several frames
            sx, sy = r.choice(spots)
            x, y = sx + r.randrange(0, 12), sy + r.randrange(0, 8)
            n = r.randrange(3, 25)
            for k in range(n):
                left = 1 if k == 0 else 4 if k == n - 1 else 2
                if r.random() < 0.2:
                    left = 0
                m = [x & 0xffffffff, y & 0xffffffff, 0, fbits(0.0), fbits(0.0), fbits(0.0), 0, 0, 0, fbits(0.0), fbits(0.0), fbits(0.0),
                     left, 0, 0]
                lines.append('F ' + ' '.join(f'{v:x}' for v in [fbits(r.choice([0.05, 0.1, 0.2, 0.3]))] + m))
            state['down'] = 0
            continue
        lines.append(mouse_line(r, spots, state))
    lines.append('E')
    return lines


# ---- the original ----

class Original:
    def __init__(self, seed, data, start, cfg):
        self.t = Trace(seed=seed)
        w = self.w = TwodWorld(self.t, seed)
        e = self.e = w.emu
        t = self.t
        trees = [w.read_tree(data, start) for _ in range(2)]
        e.stub(ZRDR_LOAD, '@zrdr_load', 'c,d', None, lambda emu, key, a, pop: emu._ret(trees.pop(0), pop))
        for addr, spec in MECH3.items():
            e.stub(addr, f'{addr:#010x}', spec)

        def strdup(emu, key, a, pop):
            s = emu.cpu.read(a[0], 256).split(b'\0')[0] if a[0] else b''
            p = emu.alloc(len(s) + 1)
            emu.cpu.write(p, s + b'\0')
            emu._ret(p if a[0] else 0, 0)
        e.import_stub(IAT['_strdup'], '@_strdup', 's0', 0, strdup)

        def strchr(emu, key, a, pop):
            s, c = a
            data = emu.cpu.read(s, 512).split(b'\0')[0]
            c &= 0xff
            i = len(data) if c == 0 else data.find(bytes([c]))
            emu._ret(s + i if i >= 0 else 0, 0)
        e.import_stub(IAT['strchr'], '@strchr', 's0,s1', 0, strchr)

        def split(emu, key, a, pop):
            parts = splitpath(emu.cstr(a[0]))
            for buf, s in zip(a[1:], parts):
                if buf:
                    emu.cpu.write(buf, s[:255].encode('latin1') + b'\0')
            emu._ret(0, 0)
        e.import_stub(IAT['_splitpath'], '@_splitpath', 's0,s1,s2,s3,s4', 0, split)

        def copy_rect(emu, key, a, pop):
            emu.cpu.write(a[0], emu.cpu.read(a[1], 16))
            emu._ret(1, 8)
        e.import_stub(IAT['CopyRect'], '@CopyRect', 's0,s1', 8, copy_rect)

        self.listing = []
        self.pos = 0

        def entry(buf):
            name, attr = self.listing[self.pos]
            self.pos += 1
            e.cpu.write(buf, b'\0' * 0x140)
            e.w32(buf, attr)
            e.cpu.write(buf + 0x2c, name.encode('latin1')[:259] + b'\0')

        def find_first(emu, key, a, pop):
            t.handle('FindFirstFileA', (emu.cstr(a[0]),))
            self.pos = 0
            if not self.listing:
                emu._ret(0xffffffff, 8)
                return
            entry(a[1])
            emu._ret(0x4f00 + len(self.listing), 8)

        def find_next(emu, key, a, pop):
            if self.pos >= len(self.listing):
                emu._ret(0, 8)
                return
            entry(a[1])
            emu._ret(1, 8)
        e.import_stub(IAT['FindFirstFileA'], '@FindFirstFileA', 's0,s1', 8, find_first)
        e.import_stub(IAT['FindNextFileA'], '@FindNextFileA', 's0,s1', 8, find_next)

        def char_cb(emu, key, a, pop):
            cb, ctx = a
            e.w32(0x63c49c, cb)
            e.w32(0x63c4a0, ctx)
            t.handle(key, (1 if cb else 0, ctx - self.m if ctx else 0))
            emu._ret(0, 0)
        e.stub(0x51a9a0, '0x0051a9a0', 'c,d', 0, char_cb)
        e.stub(0x562940, '0x00562940', 'c,d,s0,s1,s2', 0xc)
        e.stub(0x42f380, '0x0042f380', 'zs0,sb1', 8)
        e.stub(0x48ebf0, '@shell_enter', 'c')
        e.stub(0x48ec40, '@shell_leave', 'c')
        w.answers.update({
            '0x0040cec0': (lambda rr, k, a: rr.randrange(2)) if cfg['fast'] == 'random' else cfg['fast'],
            '0x0040d760': cfg['size'][0], '0x0040d770': cfg['size'][1],
        })

        # FIX (bug), as the port: an empty list's rectangle ends at its top (module docstring).
        def empty_rect(uc, address, size, user):
            esp = uc.reg_read(UC_X86_REG_ESP)
            if i32(e.r32(esp + 0x10)) <= 0:
                e.w32(esp + 0x30, e.r32(esp + 0x1c))
        e.uc.hook_add(UC_HOOK_CODE, empty_rect, begin=0x48d06d, end=0x48d06d)

        self.state = e.world_alloc(0x18)
        abuf = e.world_alloc(0x110)
        ent = e.world_alloc(16)
        e.w32(ent, abuf)
        e.w32(0x5f4d40, ent)
        self.abuf = abuf

    def vcall(self, obj, slot, stack=()):
        try:
            return self.e.call(self.e.r32(self.e.r32(obj) + slot), ecx=obj, stack=stack)
        except Exception:
            from unicorn.x86_const import UC_X86_REG_EIP
            print('FAIL at', hex(self.e.uc.reg_read(UC_X86_REG_EIP)), hex(obj - self.m), hex(slot), self.last,
                  self.t.log[-12:])
            raise

    def run(self, plan):
        e, w = self.e, self.w
        self.snapshot = e.snapshot()
        self.m = 0
        dump = ''
        S = self.state
        for n_line, line in enumerate(plan):
            self.last = (n_line, line)
            p = line.split()
            op = p[0]
            pv = [int(x, 16) if re.fullmatch(r'[0-9a-f]+', x) else x for x in p[1:]]
            m = self.m
            if op == 'active':
                e.cpu.write(self.abuf, unhx(p[1]).encode('latin1') + b'\0')
            elif op == 'N':
                e.call(0x42f230, ecx=S)
                vt = e.world_alloc(0x40)
                e.cpu.write(vt, e.cpu.read(0x5be978, 0x2c))
                e.w32(vt + 0x28, e.new_stub('vt+0x28', 'c', 0))
                e.w32(S, vt)
                self.m = self.d = e.r32(S + 0xc)
            elif op == 'y':
                e.w8(S + 0x14, pv[0])
            elif op == 'Y':
                self.listing = [(unhx(p[2 + 2 * i]), int(p[3 + 2 * i], 16)) for i in range(pv[0])]
                e.call(0x42f2b0, ecx=S)
            elif op == 'Z':
                e.call(0x42f340, ecx=S)
            elif op == 'F':
                w.mouse_frames.append(pv[1:])
                self.vcall(m, 0x00, (pv[0],))
                w.frame += 1
            elif op == 'P':
                self.vcall(m + PARTS[pv[0]], 0x30)
            elif op == 'R':
                self.vcall(m + FILES, 0x34)
            elif op == 'S':
                f = m + FILES
                first, last = e.r32(f + 0x15c), e.r32(f + 0x160)
                if pv[0] < ((last - first) // 4 if first else 0):
                    self.vcall(f, 0x98, (pv[0],))
            elif op == 'W':
                e.call(0x48d160, ecx=m + FILES, stack=(pv[0],))
            elif op == 'K':
                cb = e.r32(0x63c49c)
                if cb:
                    e.call(cb, ecx=pv[0], edx=pv[1], stack=(e.r32(0x63c4a0),))
            elif op == 'T':
                s = e.world_string(unhx(p[1]))
                e.call(0x5a52a0, ecx=m + BAR, stack=(s,))
            elif op == 'U':
                e.w8(m + BAR + 0x37c, 1)
            elif op == 'c':
                e.call(0x5a50d0, ecx=m + BAR)
            elif op == 'X':
                self.vcall(m + BAR, 0x90 + 4 * pv[0])
            elif op in ('h', 'v'):
                o = m + (BAR if pv[0] == 0 else FILES)
                if op == 'h':
                    self.vcall(o, 0x64)
                else:
                    self.vcall(o, 0x60, (fbits(-1.0),))
            elif op == 'G':
                self.vcall(m + FILES, 0xa4)
            elif op == 'A':
                self.vcall(m, 0x04, (pv[0],))
            elif op == 'D':
                e.call(0x59fa60, ecx=m)
            elif op == 'H':
                e.call(0x59fa40, ecx=m)
            elif op == 'E':
                dump = test_twod.Original.dump(self) + self.dump_popup()
                e.call(0x48eb80, ecx=S)
        return dump

    # The dump probe_loadsave.cpp writes (probe_twod.h's Dump), from the emulator's memory.
    def dump_popup(self):
        e, m = self.e, self.m
        objs = []
        p = e.r32(m + 8)
        while p:
            objs.append(p)
            p = e.r32(p + 4)
        out = []

        def kv(k, v):
            out.append(f'{k}={v & 0xffffffff:x} ')

        def tx(k, v):
            out.append(f'{k}=T{v:x} ')

        def s(k, a):
            out.append(f'{k}="' + (e.cstr(a).replace(' ', '_') if a else '') + '" ')

        def rc(k, a):
            out.append(f'{k}=' + ','.join(str(i32(e.r32(a + 4 * i))) for i in range(4)) + ' ')

        def ref(k, a):
            out.append(f'{k}=o{objs.index(a) if a in objs else -1} ')

        def labels(k, a):
            first, last = e.r32(a + 4), e.r32(a + 8)
            items = [e.r32(first + 4 * i) for i in range((last - first) // 4)] if first else []
            out.append(f'{k}=' + ''.join(f'o{objs.index(x) if x in objs else -1},' for x in items) + ' ')

        def base(o):
            ref('next', e.r32(o + 4))
            kv('flags', e.r32(o + 0xc))
            kv('timer', e.r32(o + 0x10))
            kv('x', e.r32(o + 0x14))
            kv('y', e.r32(o + 0x18))
            tx('bg', e.r32(o + 0x1c))
            rc('bgr', o + 0x20)
            kv('mouse', e.r32(o + 0x30) & 0xffff)

        def image(o):
            base(o)
            kv('owned', e.r32(o + 0x34))
            kv('queued', e.r32(o + 0x38))
            tx('img', e.r32(o + 0x3c))
            kv('u40', e.r32(o + 0x40) & 0xffff)
            src = e.r32(o + 0x44)
            if src:
                rc('src', src)
            else:
                out.append('src=0 ')
            kv('center', e.r32(o + 0x48))

        def control(o):
            image(o)
            for k, off in (('bx', 0xbc), ('by', 0xc0), ('en', 0xc4)):
                kv(k, e.r32(o + off))
            rc('bounds', o + 0xcc)
            for k, off, t in (('inorm', 0xdc, 1), ('idis', 0xe0, 1), ('iroll', 0xe4, 1), ('sroll', 0xe8, 0),
                              ('proll', 0xec, 0), ('vroll', 0xf0, 0), ('iact', 0xf4, 1), ('sact', 0xf8, 0),
                              ('pact', 0xfc, 0), ('vact', 0x100, 0), ('sdis', 0x104, 0)):
                (tx if t else kv)(k, e.r32(o + off))
            labels('L', o + 0x10c)
            labels('LR', o + 0x11c)
            labels('LA', o + 0x12c)
            labels('LD', o + 0x13c)

        def text(o):
            base(o)
            s('text', o + 0x34)
            kv('font', e.r32(o + 0x234))
            kv('centered', e.r32(o + 0x238))
            kv('align', e.r32(o + 0x244))
            ti = e.r32(o + 0x248)
            tx('timg', ti)
            if ti and ti not in self.w.stub_textures:
                w_, h_ = e.r32(ti + 4) & 0xffff, e.r32(ti + 4) >> 16
                px = e.cpu.read(e.r32(ti + 0x10), 2 * w_ * h_)
                hsh = 2166136261
                for i in range(w_ * h_):
                    hsh = ((hsh ^ (px[2 * i] | px[2 * i + 1] << 8)) * 16777619) & 0xffffffff
                kv('tw', w_)
                kv('th', h_)
                kv('tflags', e.r32(ti + 8) >> 8 & 0xff)
                kv('tpix', hsh)
            for k, off in (('top', 0x24c), ('bottom', 0x250), ('shcol', 0x254), ('hfont', 0x258)):
                kv(k, e.r32(o + off))
            s('rendered', o + 0x260)
            for k, off in (('w', 0x460), ('h', 0x464), ('shadow', 0x468), ('bk', 0x46c), ('bkc', 0x470),
                           ('nr', 0x474), ('lead', 0x478), ('fixed', 0x47c)):
                if k != 'bkc' or e.r32(o + 0x46c) == 2:
                    kv(k, e.r32(o + off))
            rc('fr', o + 0x480)
            rc('tr', o + 0x490)
            for k, off in (('sdx', 0x4a0), ('sdy', 0x4a4), ('maxw', 0x4a8), ('named', 0x4ac), ('ft', 0x4b0),
                           ('frate', 0x4b4), ('atop', 0x4b8)):
                kv(k, e.r32(o + off))
            kv('fon', e.r32(o + 0x4c0) & 0xff)
            kv('fmode', e.r32(o + 0x4c4))
            kv('phase', e.r32(o + 0x4c8))

        out.append('popup ')
        tx('grab', e.r32(m + 0x11ccc))
        kv('save', e.r8(m + 0x11cd0))
        kv('pstate', e.r32(m + 0x10fec))
        b = m + BAR
        out.append('\nbar ')
        control(b)
        kv('focus', e.r8(b + 0x374))
        kv('clicked', e.r32(b + 0x378))
        kv('numeric', e.r8(b + 0x37c))
        kv('caretw', e.r32(b + 0x370))
        kv('active', e.r8(b + 0x38c))
        kv('reading', e.r8(b + 0x388))
        kv('lx', e.r32(b + 0x380))
        kv('ly', e.r32(b + 0x384))
        ed = b + 0x14c
        kv('size', e.r32(ed + 0xc))
        kv('cursor', e.r32(ed + 0x10))
        s('buf', e.r32(ed + 8))
        s('reject', e.r32(ed + 4))
        c = b + 0x264
        out.append('\ncaret ')
        base(c)
        n = e.r32(c + 0xdc)
        kv('count', n)
        if n:
            kv('color', e.r32(c + 0xe0))
        for i in range(n):
            out.append(f'{i32(e.r32(c + 0x34 + 8 * i))},{i32(e.r32(c + 0x38 + 8 * i))} ')
        for k, off in (('cx', 0xec), ('cy', 0xf0), ('cw', 0xf4), ('ch', 0xf8), ('blink', 0xfc), ('period', 0x100),
                       ('btimer', 0x104), ('phase', 0x108)):
            kv(k, e.r32(c + off))
        f = m + FILES
        out.append('\nfiles ')
        control(f)
        for k, off in (('selected', 0x17c), ('top', 0x180), ('size', 0x184)):
            kv(k, e.r32(f + off))
        rc('rect', f + 0x188)
        for k, off in (('sx', 0x198), ('sy', 0x19c), ('ox', 0x1a0), ('oy', 0x1a4), ('fn', 0x1a8), ('fs', 0x1ac),
                       ('fr', 0x1b0), ('bdx', 0x1b4), ('bdy', 0x1b8), ('rdx', 0x1bc), ('rdy', 0x1c0), ('s0', 0x1cc),
                       ('s1', 0x1d0), ('p0', 0x1d4), ('p1', 0x1d8), ('arrow', 0x1dc)):
            kv(k, e.r32(f + off))
        kv('inside', e.r8(f + 0x1e8))
        first, last = e.r32(f + 0x16c), e.r32(f + 0x170)
        rows = [e.r32(first + 4 * i) for i in range((last - first) // 4)] if first else []
        lit, sel = e.r32(f + 0x1ec), e.r32(f + 0x178)
        kv('lit', 100 if lit and lit == sel else rows.index(lit) if lit in rows else -1)
        for k, off in (('rt', 0x1f0), ('rd', 0x1f4), ('rr', 0x1f8)):
            kv(k, e.r32(f + off))
        kv('rep', e.r8(f + 0x1fc))
        kv('norep', e.r8(f + 0x1fd))
        first, last = e.r32(f + 0x15c), e.r32(f + 0x160)
        items = [e.r32(first + 4 * i) for i in range((last - first) // 4)] if first else []
        kv('n', len(items))
        for it in items:
            s('item', e.r32(it))
            kv('v', e.r32(it + 4))
        kv('nrows', len(rows))
        for i, rw in enumerate(rows):
            out.append(f'\nrow{i} ')
            text(rw)
            kv('index', e.r32(rw + 0x4d0))
        if sel:
            out.append('\nselrow ')
            text(sel)
            kv('index', e.r32(sel + 0x4d0))
        for name, off, fn in (('up', 0x1e0, control), ('down', 0x1e4, control), ('barimg', 0x1c8, image),
                              ('rollimg', 0x1c4, image)):
            o = e.r32(f + off)
            if o:
                out.append(f'\n{name} ')
                fn(o)
        out.append('\n')
        return ''.join(out)


def run_case(seed, path, data, start, cfg, plan_args, verbose):
    orig = Original(seed, data, start, cfg)
    plan = make_plan(seed, *plan_args)
    dump_o = orig.run(plan)
    rep = Replay(orig.t, orig.e, orig.snapshot)
    probe.probe_app_set_callback(ctypes.cast(rep.callback(), ctypes.c_void_p))
    buf = ctypes.create_string_buffer(1 << 23)
    n = probe.probe_loadsave_run(str(path).encode(), orig.state, '\n'.join(plan).encode(), buf, len(buf))
    assert n >= 0, 'probe failed'
    dump_r = buf.value.decode('latin1')
    stubbed = orig.w.stub_textures
    to, do = canonical(orig.t.log, dump_o, stubbed)
    tr, dr = canonical(rep.log, dump_r, stubbed)
    do, dr = (re.sub(r'face=".*?" height=', 'height=', x, flags=re.S) for x in (do, dr))
    ok = compare(f'loadsave seed {seed}', to, tr)
    if do != dr:
        ok = False
        for a, b in zip(do.split('\n'), dr.split('\n')):
            if a != b:
                print(f'loadsave seed {seed}: dumps differ')
                ta, tb = a.split(' '), b.split(' ')
                print('  original:', ' '.join(x for x in ta if x not in tb)[:400])
                print('  rebuild: ', ' '.join(x for x in tb if x not in ta)[:400])
                print('  in:', a[:160])
                break
        else:
            print(f'loadsave seed {seed}: dumps differ in length')
    if verbose and ok:
        print(f'loadsave seed {seed}: {len(to)} calls')
    return ok, to


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    runs = int(args[0]) if args else 24
    first = int(args[1]) if len(args) > 1 else 0
    verbose = '--verbose' in sys.argv
    data = READER.read_bytes()
    fname = 'dlgOptions.zrd'
    start = next(s for n, s, _ in entries(data) if n == fname)
    tree, _ = parse_node(data, start)
    synth_path = ROOT / 'build' / 'loadsave_synth.zbd'
    passed = calls = 0
    stats = {}
    for seed in range(first, first + runs):
        if verbose:
            print('seed', seed, flush=True)
        r = random.Random(f'cfg:{seed}')
        cfg = {'fast': r.choice([0, 0, 1, 'random']), 'size': r.choice(SIZES)}
        if seed % 2:
            t = synthetic(seed, tree)
            blob = write_node(t)
            synth_path.write_bytes(archive([(fname, blob)]))
            path, file_data, file_start, here = synth_path, blob, 0, t
        else:
            path, file_data, file_start, here = READER, data, start, tree
        body = find(here, 'LOAD_SAVE_POPUP') or []
        names = [x for x in (find(body, 'BUTTONS') or [])[::2] if isinstance(x, str)]
        bound = [i for i, n in enumerate(NAMES) if n in names]
        spots = spots_of(body) or [(250, 150)]
        ok, trace = run_case(seed, path, file_data, file_start, cfg, (bound, spots), verbose)
        passed += ok
        calls += len(trace)
        for k, _ in trace:
            if k in ('0x00562940', '0x0051a9a0', '0x0042f380', 'vt+0x28', 'FindFirstFileA', '0x0056e3d0',
                     '0x0056e9f0', 'DrawTextA'):
                stats[k] = stats.get(k, 0) + 1
    print(f'loadsave: {passed}/{runs} runs identical ({calls} calls; ' +
          ', '.join(f'{k} x{v}' for k, v in sorted(stats.items())) + ')')
    return 0 if passed == runs else 1


if __name__ == '__main__':
    # Out with os._exit: the interpreter's own shutdown can hang with the
    # emulator and the probe's callbacks in one process (macOS).
    code = 1
    try:
        code = main()
    except BaseException:
        import traceback
        traceback.print_exc()
    sys.stdout.flush()
    sys.stderr.flush()
    os._exit(code)
