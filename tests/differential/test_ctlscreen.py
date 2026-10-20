"""The controls screen and its axis popup against the original x86 (docs/menus.md, The controls screen).

    cmake --build build && .venv/bin/python -P tests/differential/test_ctlscreen.py [runs] [--verbose]

The original builds the controls screen 0x0042a590 (dlgOptions.zrd's
CTL_CFG_DIALOG, and inside it the axis popup 0x0042da80 from
ASSIGN_AXIS_POPUP) for a fake state, then runs a plan: frames of mouse input
over the lists, buttons and arrows through the screen's update 0x0042be10
(the captures: zInput's last key, stick and mouse buttons, the stick's
readings), presses of every bound button, list (left and right) and arrow,
row selections (+0x90), the group cycle, the binds 0x0042c670..0x0042cb00,
the axis bind 0x0042cca0 and its clears, load and save 0x0042d0b0, the
popup's enter and leave halves (0x0042e720 / 0x0042e7c0 with the shell's
parts stubbed), its frames 0x0042e470 and presses (OK binds the axis); then
the deleting destructor. Even runs use the shipped dialogs, odd runs
synthetic ones (buttons, lists, arrows and labels missing or moved; one in
eight without the screen's dialog). zInput's map and capture, the controls
setup (groups, commands, defaults, the file's read and write), the stick's
readings, the messages and their formatting, the message box, the app's push
and the popup states' +0x28 are stubs; the command entries (0x00604128), the
axis table, ActiveCtlCfg and the mouse guard [0x006056f4] are random and
handed to the port (tests/probe_ctlscreen.cpp), which runs the same plan with
the original's answers. Compared: the call traces and a dump of both
dialogs, their objects, the lists' items, the cycles, the capture state, the
axis table, ActiveCtlCfg and the popup's fields. The x87 runs at 24 bits.
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
from twod_world import IAT, READER, TwodWorld, c_format, entries, find, parse_node, write_node  # noqa: E402
from unicorn.x86_const import UC_X86_REG_ESP  # noqa: E402
import test_twod  # noqa: E402
from test_twod import canonical, fbits  # noqa: E402
from test_twod_msgbox import archive  # noqa: E402
from test_menus import MECH3, SIZES, mutate_buttons  # noqa: E402

probe = load_probe()
probe.probe_app_set_callback.argtypes = [ctypes.c_void_p]
probe.probe_ctlscreen_run.argtypes = [ctypes.c_char_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_char_p,
                                      ctypes.c_char_p, ctypes.c_int]

ZRDR_LOAD = 0x556740
SIZE = 0x16c94
SLIDES = [0x12eb4, 0x130a0, 0x1328c, 0x13478, 0x15f48]
SLIDE_NAMES = ['ACCEPT', 'CANCEL', 'CMD_RESET_TO_MOUSE_BTN', 'CMD_RESET_TO_JSTICK_BTN', 'CMD_ASSIGN_AXIS_BTN']
LISTS = [0x13664, 0x13cb4, 0x14304, 0x14954, 0x14fa4]
LIST_NAMES = ['CMD_COMMAND_BTN', 'CMD_KEYA_BTN', 'CMD_KEYB_BTN', 'CMD_JOY_BTN', 'CMD_MOUSE_BTN']
ARROWS = [0x15a18, 0x15b64, 0x15cb0, 0x15dfc]
ARROW_NAMES = ['CMD_NEXT_CMD_BTN', 'CMD_PREV_CMD_BTN', 'CMD_EXPORT_BTN', 'CMD_IMPORT_BTN']
CYCLE = 0x155f4
POPUP = 0x16760
LOADSAVE = 0x1677c
POPUP_PARTS = [0x111ec, 0x113d8, 0x115c4, 0x119e8, 0x11b54]
POPUP_NAMES = ['MB_OK', 'MB_CANCEL', 'CMD_AXIS_CYCLE_BTN', 'INVERT_CHECKBOX', 'AXIS_VALUE']
BINDS = [0x42c670, 0x42c830, 0x42c960, 0x42cb00]
NAMED_KEYS = {0x51d310: 'C', 0x51d330: 'K', 0x51d350: 'J', 0x51d370: 'B'}
MSGBOX3 = {0xf27, 0xf29, 0xf2b, 0xfb6}
ACTIVE = ['DefaultCtlConfig', 'keys\\Profile1.zrd', 'keys\\my set.zrd', 'C:\\games\\mw3\\keys\\pad.v2.zrd',
          'keys/stick', '', 'x.y\\name']

test_twod.VT_KIND.update({v: 'C' for v in (0x5be268, 0x5be1c8, 0x5be128, 0x5be088, 0x5bda08, 0x5bdce8, 0x5bdc58,
                                           0x5bdbc8, 0x5bdb38, 0x5bdaa8, 0x5be608, 0x5be568, 0x5be4c0)})
test_twod.VT_KIND.update({v: 'X' for v in (0x5bdff0, 0x5bdf58, 0x5bdec0, 0x5bde28, 0x5bdd90)})
test_twod.VT_KIND.update({0x5be428: 'K', 0x5bbc90: 'L', 0x5bc1b0: 'I', 0x5bd988: 'I'})


def i32(v):
    return struct.unpack('<i', struct.pack('<I', v & 0xffffffff))[0]


def splitpath(p):
    drive = p[:2] if len(p) >= 2 and p[1] == ':' else ''
    rest = p[len(drive):]
    i = max(rest.rfind('\\'), rest.rfind('/'))
    d, name = rest[:i + 1], rest[i + 1:]
    j = name.rfind('.')
    return drive, d, (name[:j] if j >= 0 else name), (name[j:] if j >= 0 else '')


# ---- dialogs ----

def synthetic(seed, tree):
    """The shipped file with CTL_CFG_DIALOG and ASSIGN_AXIS_POPUP mutated."""
    r = random.Random(f'ctl:{seed}')
    out = []
    for name in ('CTL_CFG_DIALOG', 'ASSIGN_AXIS_POPUP', 'LOAD_SAVE_POPUP'):
        body = [list(x) if isinstance(x, list) else x for x in find(tree, name)]
        nb = []
        for i in range(0, len(body) - 1, 2):
            k, v = body[i], body[i + 1]
            if k == 'BUTTONS':
                v = mutate_buttons(r, v, None)
            elif k == 'PRIMITIVES':
                v = [x for j in range(0, len(v) - 1, 2) if r.random() >= 0.15 for x in (v[j], v[j + 1])]
            nb += [k, v]
        if name == 'CTL_CFG_DIALOG' and seed % 8 == 7:
            name = 'NOT' + name
        out += [name, nb]
    return [['SHARED_IMAGE_PATH', ['common']] + out]


def spots_of(body):
    out = []
    bl = find(body, 'BUTTONS') or []
    for i in range(0, len(bl) - 1, 2):
        e = bl[i + 1]
        p = find(e, 'POSITION')
        if not p:
            continue
        out.append(tuple(p))
        if find(e, 'LISTSIZE') is not None:
            out += [(p[0] + 4, p[1] + 15 * k) for k in range(-2, 6)]
        for key in ('TOGGLE_LEFT', 'TOGGLE_RIGHT'):
            t = find(e, key)
            tp = find(t, 'POSITION') if t else None
            if tp:
                out.append((p[0] + tp[0], p[1] + tp[1]))
    return out


# ---- the plan ----

def mouse_line(op, r, spots, state):
    sx, sy = r.choice(spots)
    if r.random() < 0.85:
        x, y = sx + r.randrange(-6, 30), sy + r.randrange(-6, 20)
    else:
        x, y = r.randrange(-20, 660), r.randrange(-20, 500)
    if state['down'] == 0:
        left = 1 if r.random() < 0.25 else 0
    elif state['down'] in (1, 2):
        left = 2 if r.random() < 0.5 else 4
    else:
        left = 0
    state['down'] = left
    right = r.choice([0, 0, 0, 0, 1, 4])
    dt = r.choice([0.0, 1 / 60, 1 / 30, 0.05, 0.2])
    m = [x & 0xffffffff, y & 0xffffffff, 0, fbits(0.0), fbits(0.0), fbits(0.0), 0, 0, 0,
         fbits(0.0), fbits(0.0), fbits(0.0), left, right, 0]
    return f'{op} ' + ' '.join(f'{v:x}' for v in [fbits(dt)] + m)


def make_plan(seed, setup, bound, pbound, spots, pspots):
    r = random.Random(f'plan:{seed}')
    lines = list(setup) + ['N']
    state = {'down': 0}
    owner = False
    names = ['Profile1', 'my_set', 'DefaultCtlConfig', 'a.b']
    for _ in range(r.randrange(40, 110)):
        roll = r.random()
        sl = [i for k, i in bound if k == 'P']
        ls = [i for k, i in bound if k == 'L']
        ar = [i for k, i in bound if k == 'O']
        cy = [i for k, i in bound if k == 'C']
        if roll < 0.07 and sl:
            lines.append(f'P {r.choice(sl):x}')
        elif roll < 0.15 and ls:
            lines.append(f'L {r.choice(ls):x}')
        elif roll < 0.19 and ls:
            lines.append(f'M {r.choice(ls):x}')
        elif roll < 0.25 and ls:
            lines.append(f'S {r.choice(ls):x} {r.randrange(0, 6):x}')
        elif roll < 0.28 and ar:
            lines.append(f'O {r.choice(ar):x}')
        elif roll < 0.30 and cy:
            lines.append('C')
        elif roll < 0.32:
            lines.append(f'W {r.choice([-1, 1, 2]) & 0xffffffff:x}')
            if cy and r.random() < 0.7:
                lines.append('C')  # the next group's lists
        elif roll < 0.35:
            kind = r.randrange(4)
            v = r.choice([0, 1, r.randrange(2, 0x500)]) if kind < 2 else r.choice([0, r.randrange(1, 12)])
            lines.append(f'B {kind:x} {v:x} {r.randrange(0, 6):x}')
        elif roll < 0.37:
            src = r.choice([0, 1, 2, 3, 6, 7, 9, 10, 11])
            lines.append(f'X {src:x} {fbits(r.choice([1.0, -1.0, 0.5])):x} {fbits(r.choice([0.0, 0.25, -0.75])):x}')
        elif roll < 0.38:
            lines.append(r.choice('JK'))
        elif roll < 0.40:
            lines.append(f'V {r.randrange(2):x} {r.choice(names)}')
        elif roll < 0.43:
            lines.append(f'y {r.randrange(4):x} {r.choice([0, 0, 1, 3, 9, 11]):x}')
            owner = True
        elif roll < 0.45:
            lines.append(r.choice('YZ'))
        elif roll < 0.52 and owner and pbound:
            lines.append(f'p {r.choice(pbound):x}')
        elif roll < 0.54:
            lines.append(f'q {r.choice([-1, 1, 3]) & 0xffffffff:x}')
        elif roll < 0.62:
            lines.append(mouse_line('f', r, pspots, state))
            continue
        elif roll < 0.63:
            lines.append(r.choice('DH'))
        elif roll < 0.64:
            lines.append(f'A {r.randrange(2):x}')
        lines.append(mouse_line('F', r, spots, state))
    lines.append('E')
    return lines


# ---- the original ----

class Original:
    def __init__(self, seed, data, start, cfg):
        self.t = Trace(seed=seed)
        w = self.w = TwodWorld(self.t, seed)
        e = self.e = w.emu
        self.cfg = cfg
        # A tree for each load (the popup's, then the screen's): each dialog frees its own.
        trees = [w.read_tree(data, start) for _ in range(4)]
        e.stub(ZRDR_LOAD, '@zrdr_load', 'c,d', None, lambda emu, key, a, pop: emu._ret(trees.pop(0), pop))
        for addr, spec in MECH3.items():
            if addr not in (0x4ac150,):
                e.stub(addr, f'{addr:#010x}', spec)
        t = self.t

        def strdup(emu, key, a, pop):
            s = emu.cpu.read(a[0], 256).split(b'\0')[0] if a[0] else b''
            p = emu.alloc(len(s) + 1)
            emu.cpu.write(p, s + b'\0')
            emu._ret(p if a[0] else 0, 0)
        e.import_stub(IAT['_strdup'], '@_strdup', 's0', 0, strdup)

        def sprintf(emu, key, a, pop):
            buf, fmt = a
            va = emu.uc.reg_read(UC_X86_REG_ESP) + 12
            s = c_format(emu, emu.cstr(fmt), va).encode('latin1')
            emu.cpu.write(buf, s + b'\0')
            emu._ret(len(s), 0)
        e.import_stub(IAT['sprintf'], '@sprintf', 's0,s1', 0, sprintf)
        e.import_stub(IAT['_mkdir'], '@_mkdir', 's0', 0, lambda emu, key, a, pop: emu._ret(0, 0))

        def copy_rect(emu, key, a, pop):  # the load/save popup's file list (built with the screen)
            emu.cpu.write(a[0], emu.cpu.read(a[1], 16))
            emu._ret(1, 8)
        e.import_stub(IAT['CopyRect'], '@CopyRect', 's0,s1', 8, copy_rect)

        def split(emu, key, a, pop):
            parts = splitpath(emu.cstr(a[0]))
            for buf, s in zip(a[1:], parts):
                if buf:
                    emu.cpu.write(buf, s[:255].encode('latin1') + b'\0')
            emu._ret(0, 0)
        e.import_stub(IAT['_splitpath'], '@_splitpath', 's0,s1,s2,s3,s4', 0, split)

        # zInput, the controls setup, the stick
        plain = {0x51aa00: 'c', 0x51dd20: 'c', 0x51baf0: 'c', 0x51dce0: '', 0x51b2c0: '', 0x51b2f0: '',
                 0x51d200: 'c', 0x51d210: 'c', 0x51d220: 'c', 0x51d230: 'c', 0x51d240: 'c', 0x51d250: 'c',
                 0x51d260: 'c', 0x51d270: 'c', 0x51d280: 'c,d', 0x51d290: 'c,d', 0x51d2a0: 'c,d', 0x51d2b0: 'c,d',
                 0x51d1c0: '', 0x51dc10: '', 0x411150: '', 0x410f80: '', 0x410fa0: 'c', 0x410fb0: 'c',
                 0x410fd0: 'c,d', 0x4120a0: 'zc', 0x411e60: 'zc', 0x40d070: 'c'}
        for addr, spec in plain.items():
            e.stub(addr, f'{addr:#010x}', spec)
        e.stub(0x48ebf0, '@shell_enter', 'c')
        e.stub(0x48ec40, '@shell_leave', 'c')

        def named(prefix):
            def fn(emu, key, a, pop):
                v, buf, n = a
                ok = t.handle(key, (v, n))
                s = f'{prefix}{v:x}' if ok else ''
                emu.cpu.write(buf, s[:n - 1].encode('latin1') + b'\0')
                emu._ret(buf, pop)
            return fn
        for addr, prefix in NAMED_KEYS.items():
            e.stub(addr, f'{addr:#010x}', 'c,d,s0', None, named(prefix))

        def fmt(emu, key, a, pop):
            buf, size, mid, arg = a
            s = emu.cstr(arg) if arg else ''
            t.handle(key, (mid, s))
            text = f'F{mid:x}:{s}'[:size - 1]
            emu.cpu.write(buf, text.encode('latin1') + b'\0')
            emu._ret(len(text), 0)
        e.stub(0x575fd0, '0x00575fd0', 's0,s1,s2,s3', 0, fmt)

        def msgbox(emu, key, a, pop):
            if a[1] in MSGBOX3:
                v = t.handle(key, (a[0], a[1], a[2], emu.cstr(a[3]), emu.cstr(a[4]), emu.cstr(a[5])))
            else:
                v = t.handle(key, (a[0], a[1], a[2]))
            emu._ret(v, 0)
        e.stub(0x4ac150, '0x004ac150', 's0,s1,s2,s3,s4,s5', 0, msgbox)

        fbuf = e.world_alloc(4)
        thunk = e.thunk(b'\xd9\x05' + struct.pack('<I', fbuf) + b'\xc2\x0c\x00')

        def reading(emu, key, a, pop):
            emu.w32(fbuf, t.handle(key, tuple(a)))
            emu._ret(0, 0, then=thunk)
        e.stub(0x42fea0, '0x0042fea0', 's0,s1,s2', 12, reading)

        titles = [e.world_string(f'Group {i}') for i in range(10)]
        named_cmds = [c for c in range(1, 0xa7) if c % 13]
        # The groups are fixed for a run, as the setup's: sizes 6..9, rows 0..5 named (the plan's rows).
        gr = random.Random(f'groups:{seed}')
        gsize = [gr.randrange(6, 10) for _ in range(16)]
        gcmd = [[gr.choice(named_cmds) if i < 6 else gr.randrange(1, 0xa7) for i in range(16)] for _ in range(16)]
        w.answers.update({
            '0x0040cec0': (lambda rr, k, a: rr.randrange(2)) if cfg['fast'] == 'random' else cfg['fast'],
            '0x0040d760': cfg['size'][0], '0x0040d770': cfg['size'][1],
            '0x00576040': lambda rr, k, a: 0 if 0x100 <= a[0] < 0x200 and rr.random() < 0.2
            else e.world_string(f'M{a[0]:x}'),
            '0x004ac150': lambda rr, k, a: rr.choice([1, 1, 2, 6]),
            '0x00590400': 0, '0x005905a0': 0, '0x00558290': 0, '0x00558dd0': 0,
            '0x0051aa00': lambda rr, k, a: rr.choice([0] * 7 + [1, 0x1c, 0x2a, 0x11e, 0x210]),
            '0x0051dd20': lambda rr, k, a: rr.choice([0] * 6 + [1, 4, 10]),
            '0x0051baf0': lambda rr, k, a: rr.choice([0] * 6 + [1, 2, 3]),
            '0x0051dce0': lambda rr, k, a: 0 if rr.random() < 0.15 else 0x7000000 + 0x10 * (k & 3),
            '0x0042fea0': lambda rr, k, a: fbits(rr.choice([0.0, 0.0, 0.1, -0.2, 0.9, -0.95, 0.6, -0.5, 1.0])),
            '0x0051dc10': lambda rr, k, a: rr.randrange(2),
            '0x00411150': 1,
            '0x00410f80': cfg['groups'],
            '0x00410fa0': lambda rr, k, a: titles[a[0] % len(titles)],
            '0x00410fb0': lambda rr, k, a: gsize[a[0] & 15],
            '0x00410fd0': lambda rr, k, a: gcmd[a[0] & 15][a[1] & 15],
        })
        for a_ in NAMED_KEYS:
            w.answers[f'{a_:#010x}'] = (lambda rr, k, a: 0 if a[0] % 13 == 0 else 1) \
                if a_ == 0x51d310 else (lambda rr, k, a: 0 if rr.random() < 0.1 else 1)
        for key in ('0x0051d240', '0x0051d250', '0x0051d260', '0x0051d270'):
            w.answers[key] = lambda rr, k, a: rr.choice([0, 0, 0, rr.randrange(1, 0xa7)])
        for key in ('0x0051d200', '0x0051d210', '0x0051d220', '0x0051d230'):
            w.answers[key] = lambda rr, k, a: rr.choice([0, rr.randrange(1, 0x300)])
        for i, a_ in enumerate((0x576db0, 0x576da0, 0x576d90, 0x576d80)):
            w.answers[f'{a_:#010x}'] = 0x5e000000 + i

        r = random.Random(f'world:{seed}')
        vt = e.world_alloc(0x40)
        e.w32(vt + 0x28, e.new_stub('vt+0x28', 'c', 0))
        e.w32(vt + 0x10, e.new_stub('vt+0x10', 'c,s0', 4))
        self.state = e.world_alloc(0x10)
        e.w32(self.state, vt)
        if cfg['main']:
            mm = e.world_alloc(0x12c1c)
            e.w32(mm + 0x110ec, w.fake_texture(1280, 960) if r.random() < 0.9 else 0)
            e.w8(mm + 0x111e8, cfg['main_moving'])
            e.w32(0x609854, mm)
        else:
            e.w32(0x609854, 0)
        e.w8(0x608ed9, cfg['mission'])
        # The setup handed to the port: the command entries, the axis table, ActiveCtlCfg, the guard.
        self.setup = []
        for c in range(0xa7):
            msg = r.randrange(0x10, 0x100)
            ak = 1 if r.random() < 0.2 else 0
            ax = r.randrange(4) if ak else r.choice([0, 0, 2])
            base = 0x604128 + 0x14 * c
            e.w32(base, msg)
            e.w8(base + 8, ak)
            e.w32(base + 0x10, ax)
            self.setup.append(f'entry {c:x} {msg:x} {ak:x} {ax:x}')
        for i in range(4):
            src = r.choice([0, 1, 2, 3, 6, 9, 10, 11])
            vals = [src, fbits(r.choice([1.0, -1.0, -0.5])), fbits(r.choice([0.0, 0.5])), fbits(r.choice([0.0, 0.05]))]
            for j, v in enumerate(vals):
                e.w32(0x6040d0 + 0x10 * i + 4 * j, v)
            self.setup.append(f'axis {i:x} ' + ' '.join(f'{v:x}' for v in vals))
        ready = r.randrange(2)
        e.w8(0x604110, ready)
        self.setup.append(f'ready {ready:x}')
        active = r.choice(ACTIVE)
        abuf = e.world_alloc(0x110)
        e.cpu.write(abuf, active.encode('latin1') + b'\0')
        ent = e.world_alloc(16)
        e.w32(ent, abuf)
        e.w32(0x5f4d40, ent)
        self.setup.append(f'active {active}')
        guard = r.randrange(-5, 12)
        e.w32(0x6056f4, guard & 0xffffffff)
        self.setup.append(f'guard {guard & 0xffffffff:x}')

    def vcall(self, obj, slot, stack=()):
        try:
            return self.e.call(self.e.r32(self.e.r32(obj) + slot), ecx=obj, stack=stack)
        except Exception:
            from unicorn.x86_const import UC_X86_REG_EIP
            print('FAIL at', hex(self.e.uc.reg_read(UC_X86_REG_EIP)), hex(obj - self.m), hex(slot), self.last,
                  self.t.log[-12:])
            raise

    def run(self, plan):
        e, w, t = self.e, self.w, self.t
        self.snapshot = e.snapshot()
        m = self.m = self.d = e.alloc(SIZE)
        dump = ''
        for n_line, line in enumerate(plan):
            self.last = (n_line, line)
            p = line.split()
            op = p[0]
            if op in ('entry', 'axis', 'ready', 'active', 'guard'):
                continue
            pv = [int(x, 16) if re.fullmatch(r'[0-9a-f]+', x) else x for x in p[1:]]
            if op == 'N':
                try:
                    e.call(0x42a590, ecx=m, stack=(self.state,))
                except Exception:
                    from unicorn.x86_const import UC_X86_REG_EIP
                    print("FAIL at", hex(e.uc.reg_read(UC_X86_REG_EIP)), t.log[-8:])
                    raise
                # the popup state's +0x28 as a stub (the port's goes through Engine::vcall)
                vt = e.world_alloc(0x40)
                e.cpu.write(vt, e.cpu.read(0x5be6a8, 0x40))
                e.w32(vt + 0x28, e.new_stub('vt+0x28', 'c', 0))
                e.w32(m + POPUP, vt)
            elif op in ('F', 'f'):
                w.mouse_frames.append(pv[1:])
                obj = m if op == 'F' else e.r32(m + POPUP + 0xc)
                self.vcall(obj, 0x00, (pv[0],))
                w.frame += 1
            elif op == 'A':
                self.vcall(m, 0x04, (pv[0],))
            elif op == 'P':
                self.vcall(m + SLIDES[pv[0]], 0x30)
            elif op in ('L', 'M', 'S'):
                o = m + LISTS[pv[0]]
                self.vcall(o, {'L': 0x30, 'M': 0x34, 'S': 0x90}[op], (pv[1],) if op == 'S' else ())
            elif op == 'C':
                self.vcall(m + CYCLE, 0x30)
            elif op == 'W':
                self.vcall(m + CYCLE, 0xa4, (pv[0],))
            elif op == 'O':
                self.vcall(m + ARROWS[pv[0]], 0x30)
            elif op == 'B':
                e.call(BINDS[pv[0]], ecx=m, stack=(pv[1], pv[2]))
            elif op == 'X':
                e.call(0x42cca0, ecx=m, stack=(pv[0], pv[1], pv[2]))
            elif op == 'J':
                e.call(0x42ce90, ecx=m)
            elif op == 'K':
                e.call(0x42cfa0, ecx=m)
            elif op == 'V':
                name = e.world_string(p[2])
                e.call(0x42d0b0, ecx=m, stack=(name, pv[0]))
            elif op == 'y':
                e.w32(m + POPUP + 0x10, m)
                e.w32(m + POPUP + 0x14, pv[0])
                e.w32(m + POPUP + 0x18, pv[1])
            elif op == 'Y':
                e.call(0x42e720, ecx=m + POPUP)
            elif op == 'Z':
                e.call(0x42e7c0, ecx=m + POPUP)
            elif op == 'p':
                pop = e.r32(m + POPUP + 0xc)
                self.vcall(pop + POPUP_PARTS[pv[0]], 0x30)
            elif op == 'q':
                pop = e.r32(m + POPUP + 0xc)
                self.vcall(pop + 0x115c4, 0xa4, (pv[0],))
            elif op == 'D':
                e.call(0x59fa60, ecx=m)
            elif op == 'H':
                e.call(0x59fa40, ecx=m)
            elif op == 'E':
                dump = test_twod.Original.dump(self)
                pop = e.r32(m + POPUP + 0xc)
                if pop:
                    self.d = pop
                    dump += 'POPUP\n' + test_twod.Original.dump(self)
                    self.d = m
                dump += self.dump_screen()
                self.vcall(m, 0x08, (1,))
        return dump

    def dump_screen(self):
        e, m = self.e, self.m
        out = []

        def kv(k, v):
            out.append(f'{k}={v & 0xffffffff:x} ')

        def s(k, a):
            out.append(f'{k}="' + (e.cstr(a).replace(' ', '_') if a else '') + '" ')

        def tx(k, v):
            out.append(f'{k}=T{v:x} ')

        def rc(k, a):
            out.append(f'{k}=' + ','.join(str(i32(e.r32(a + 4 * i))) for i in range(4)) + ' ')

        def cycle(c):
            kv('index', e.r32(c + 0x16c))
            kv('hover', e.r32(c + 0x174))
            first, last = e.r32(c + 0x15c), e.r32(c + 0x160)
            items = [e.r32(first + 4 * j) for j in range((last - first) // 4)] if first else []
            kv('n', len(items))
            for it in items:
                tp = e.r32(it)
                out.append('item="' + (e.cstr(tp).replace(' ', '_') if tp else '(null)') + '" ')
                kv('v', e.r32(it + 4))
            out.append('\n  prev ')
            self.control(c + 0x188, out)
            out.append('\n  next ')
            self.control(c + 0x2d4, out)

        out.append('ctl ')
        kv('state', e.r32(m + 0x10fec))
        kv('busy', e.r8(m + 0x10ff0))
        kv('mode', e.r32(m + 0x16c44))
        kv('dirty', e.r8(m + 0x16c90))
        kv('guard', e.r32(0x6056f4))
        for i in range(8):
            kv('lo', e.r32(m + 0x16c50 + 8 * i))
            kv('hi', e.r32(m + 0x16c54 + 8 * i))
        bg = m + 0x110b0
        out.append('\nbackground ')
        self.image(bg, out)
        kv('moving', e.r8(bg + 0x138))
        rc('view', bg + 0xe8)
        for i, off in enumerate(LISTS):
            o = m + off
            out.append(f'\nlist{i} ')
            kv('first', e.r32(o + 0x63c))
            first, last = e.r32(o + 0x62c), e.r32(o + 0x630)
            items = [e.r32(first + 4 * j) for j in range((last - first) // 4)] if first else []
            kv('n', len(items))
            for it in items:
                s('item', e.r32(it))
                kv('v', e.r32(it + 4))
        out.append('\ngroups ')
        cycle(m + CYCLE)
        for i, off in enumerate(SLIDES):
            k = m + off
            out.append(f'\nslide{i} ')
            kv('rippling', e.r8(k + 0x1b8))
            kv('radius', e.r32(k + 0x1cc))
            kv('maxr', e.r32(k + 0x1d0))
        out.append('\naxes ')
        for i in range(4):
            a = 0x6040d0 + 0x10 * i
            for k, o in (('src', 0), ('scale', 4), ('off', 8), ('dz', 12)):
                kv(k, e.r32(a + o))
        kv('ready', e.r8(0x604110))
        out.append('\nactive ')
        s('cfg', e.r32(0x5f4d40) and e.r32(e.r32(0x5f4d40)))
        st = m + POPUP
        out.append('\npopup ')
        kv('owner', e.r32(st + 0x10))
        kv('axis', e.r32(st + 0x14))
        kv('source', e.r32(st + 0x18))
        kv('lsowner', e.r32(m + LOADSAVE + 0x10))
        kv('lssave', e.r8(m + LOADSAVE + 0x14))
        pop = e.r32(st + 0xc)
        if pop:
            kv('paxis', e.r32(pop + 0x12190))
            tx('grab', e.r32(pop + 0x1218c))
            kv('pstate', e.r32(pop + 0x10fec))
            kv('inv_id', e.r32(pop + 0x119e8 + 0x164))
            kv('inv_active', e.r8(pop + 0x119e8 + 0x168))
            out.append('\n  sources ')
            cycle(pop + 0x115c4)
        out.append('\n')
        return ''.join(out)

    def image(self, o, out):
        e = self.e

        def kv(k, v):
            out.append(f'{k}={v & 0xffffffff:x} ')
        objs = []
        p = e.r32(self.m + 8)
        while p:
            objs.append(p)
            p = e.r32(p + 4)
        nxt = e.r32(o + 4)
        out.append(f'next=o{objs.index(nxt) if nxt in objs else -1} ')
        for k, off in (('flags', 0xc), ('timer', 0x10), ('x', 0x14), ('y', 0x18)):
            kv(k, e.r32(o + off))
        out.append(f'bg=T{e.r32(o + 0x1c):x} ')
        out.append('bgr=' + ','.join(str(i32(e.r32(o + 0x20 + 4 * i))) for i in range(4)) + ' ')
        kv('mouse', e.r32(o + 0x30) & 0xffff)
        kv('owned', e.r32(o + 0x34))
        kv('queued', e.r32(o + 0x38))
        out.append(f'img=T{e.r32(o + 0x3c):x} ')
        kv('u40', e.r32(o + 0x40) & 0xffff)
        src = e.r32(o + 0x44)
        if src:
            out.append('src=' + ','.join(str(i32(e.r32(src + 4 * i))) for i in range(4)) + ' ')
        else:
            out.append('src=0 ')
        kv('center', e.r32(o + 0x48))

    def control(self, o, out):
        e = self.e
        self.image(o, out)
        for k, off in (('bx', 0xbc), ('by', 0xc0), ('en', 0xc4)):
            out.append(f'{k}={e.r32(o + off):x} ')
        out.append('bounds=' + ','.join(str(i32(e.r32(o + 0xcc + 4 * i))) for i in range(4)) + ' ')
        for k, off in (('inorm', 0xdc), ('idis', 0xe0), ('iroll', 0xe4), ('iact', 0xf4)):
            out.append(f'{k}=T{e.r32(o + off):x} ')


def run_case(seed, path, data, start, cfg, plan_args, verbose):
    orig = Original(seed, data, start, cfg)
    plan = make_plan(seed, orig.setup, *plan_args)
    dump_o = orig.run(plan)
    rep = Replay(orig.t, orig.e, orig.snapshot)
    probe.probe_app_set_callback(ctypes.cast(rep.callback(), ctypes.c_void_p))
    buf = ctypes.create_string_buffer(1 << 23)
    n = probe.probe_ctlscreen_run(str(path).encode(), orig.state, orig.m, '\n'.join(plan).encode(), buf, len(buf))
    assert n >= 0, 'probe failed'
    dump_r = buf.value.decode('latin1')
    stubbed = orig.w.stub_textures
    to, do = canonical(orig.t.log, dump_o, stubbed)
    tr, dr = canonical(rep.log, dump_r, stubbed)
    do, dr = (re.sub(r'face=".*?" height=', 'height=', x, flags=re.S) for x in (do, dr))
    ok = compare(f'ctl seed {seed}', to, tr)
    if do != dr:
        ok = False
        for a, b in zip(do.split('\n'), dr.split('\n')):
            if a != b:
                print(f'ctl seed {seed}: dumps differ')
                ta, tb = a.split(' '), b.split(' ')
                print('  original:', ' '.join(x for x in ta if x not in tb)[:400])
                print('  rebuild: ', ' '.join(x for x in tb if x not in ta)[:400])
                print('  in:', a[:160])
                break
        else:
            print(f'ctl seed {seed}: dumps differ in length')
    if verbose and ok:
        print(f'ctl seed {seed}: {len(to)} calls')
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
    synth_path = ROOT / 'build' / 'ctlscreen_synth.zbd'
    passed = calls = 0
    stats = {}
    for seed in range(first, first + runs):
        if verbose:
            print("seed", seed, flush=True)
        r = random.Random(f'cfg:{seed}')
        cfg = {'fast': r.choice([0, 0, 0, 1, 'random']), 'size': r.choice(SIZES), 'main': r.random() < 0.7,
               'main_moving': r.randrange(2), 'mission': 1 if r.random() < 0.3 else 0,
               'groups': r.choice([1, 3, 7])}
        if seed % 2:
            t = synthetic(seed, tree)
            blob = write_node(t)
            synth_path.write_bytes(archive([(fname, blob)]))
            path, file_data, file_start, here = synth_path, blob, 0, t
        else:
            path, file_data, file_start, here = READER, data, start, tree
        body = find(here, 'CTL_CFG_DIALOG') or []
        pbody = find(here, 'ASSIGN_AXIS_POPUP') or []
        names = [x for x in (find(body, 'BUTTONS') or [])[::2] if isinstance(x, str)]
        pnames = [x for x in (find(pbody, 'BUTTONS') or [])[::2] if isinstance(x, str)]
        bound = [(k, i) for k, lst in (('P', SLIDE_NAMES), ('L', LIST_NAMES), ('O', ARROW_NAMES))
                 for i, n in enumerate(lst) if n in names]
        if 'CMD_SET_BTN' in names:
            bound.append(('C', 0))
        pbound = [i for i, n in enumerate(POPUP_NAMES) if n in pnames]
        spots = spots_of(body) or [(100, 100)]
        pspots = spots_of(pbody) or [(250, 250)]
        ok, trace = run_case(seed, path, file_data, file_start, cfg, (bound, pbound, spots, pspots), verbose)
        passed += ok
        calls += len(trace)
        for k, _ in trace:
            if k in ('0x00590400', 'vt+0x28', '0x004ac150', '0x0051d280', '0x0051d290', '0x0051d2a0',
                     '0x0051d2b0', '0x004120a0', '0x00411e60', '0x0042fea0', '0x0051b2c0', '0x00411150',
                     '0x0040d070', '0x00579500', '0x0051dd20', '0x0051baf0'):
                stats[k] = stats.get(k, 0) + 1
    print(f'ctlscreen: {passed}/{runs} runs identical ({calls} calls; ' +
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
