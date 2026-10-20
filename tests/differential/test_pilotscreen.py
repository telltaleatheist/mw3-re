"""The pilot screen and its state (docs/menus.md, The pilot screen) against the original x86.

    cmake --build build && .venv/bin/python -P tests/differential/test_pilotscreen.py [runs] [first] [--verbose]

The original builds pilots first (0x004788b0, saved campaigns 0x004345a0,
their records' fields), deletes them all (0x004785f0, so saved), then the
Pilot State 0x00488910 and with it the screen 0x00486f40 from the main
menu's dialog.zrd tree (PILOTDIALOG; a fake main menu at [0x00609854] holds
the tree, its BACKGROUND image and whether it moves), which lists pilots\\
(0x00478560) over test_pilot.py's fake file system. Then a plan: the game
set (0x00488ad0), the state's enter before the shell's (0x004889a0, the
lobby's path too), frames of mouse input over the lists, buttons and bars
(Window::update, the keys 0x0051a9b0 Delete and Ctrl+R answered by a
script), presses of CANCEL, ACCEPT, DELETE, RENAME, both bars and the lists,
characters typed into the bars through zInput's text callback, Enter and
Escape on them, the lists' and the training cycle's choices, renames
started and ended, the state's Escape, dirty-all and restore; then a dump and
the state's destructor (the screen's 0x004878b0 deletes the other pilots,
so saves them). The port (tests/probe_pilotscreen.cpp) runs the same plan on
the same tree, its pilot files in a real folder. Compared: every call out
(the message box 0x004ac150, pops 0x005905a0, messages, formatted lines
0x00575fd0, the pilot module's store fill and starting mechs), a dump of the
dialog, its objects, both lists, the bars, the cycles, the screen's mode and
pilot, [0x0060861c] and every pilot's game, and every file's bytes. Even
runs use the shipped PILOTDIALOG, odd runs a synthetic one (lists and bars
moved, resized, missing; labels missing). The x87 runs at 24 bits.

FIX (bug) in the port, applied to the original here: DELETE with no pilot
chosen formats the name at address 0x18 (a crash); the message box stub
answers 0 there, unrecorded, as the port asks nothing.
"""
import ctypes
import os
import random
import re
import shutil
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import Replay, compare, load_probe, Trace  # noqa: E402
from original import ROOT  # noqa: E402
from twod_world import IAT, READER, TwodWorld, entries, find, parse_node, write_node  # noqa: E402
import test_twod  # noqa: E402
from test_twod import canonical, fbits  # noqa: E402
from test_twod_msgbox import archive  # noqa: E402
from test_menus import MECH3, SIZES, mutate_buttons  # noqa: E402
import test_pilot  # noqa: E402
from test_pilot import FS, Files, hx, unhx, i32, folder_files  # noqa: E402

probe = load_probe()
probe.probe_app_set_callback.argtypes = [ctypes.c_void_p]
probe.probe_pilotscreen_run.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint32, ctypes.c_char_p,
                                        ctypes.c_char_p, ctypes.c_int]

CANCEL, ACCEPT, DELETE_BTN, RENAME_BTN = 0x111ec, 0x113d8, 0x115c4, 0x1171c
RENAMEBAR, EDITBAR, PILOTLIST, LINES, LINES2, GAMELIST = 0x11874, 0x11c04, 0x11f94, 0x121a0, 0x1225c, 0x12318
SCENARIO, DESC, COMPLETE = 0x12518, 0x1293c, 0x12d60
MODE, CHOSEN = 0x12198, 0x1219c
PARTS = [CANCEL, ACCEPT, DELETE_BTN, RENAME_BTN, EDITBAR, RENAMEBAR, PILOTLIST, GAMELIST, SCENARIO]
NAMES = ['CANCEL', 'ACCEPT', 'DELETE', 'RENAME', 'EDITBAR', 'RENAMEBAR', 'PILOTLIST', 'GAMELIST', 'SCENARIO_LIST']
BARS = [EDITBAR, RENAMEBAR]
PILOT_NAMES = ['Bob', 'Alice', 'Zed', 'Major Tom', 'x', 'Ann_2', 'Kerensky', 'Q', 'Natasha', 'Wolf']
CHARS = [ord(c) for c in 'abcXYZ019_-. '] + [ord(c) for c in ':|\\<>/?"*\t'] + \
    [0x08, 0x08, 0x7f, 0x01, 0x02, 0x04, 0x06, 0x0d, 0x1b, 0xe9, 0x00]
STATE_NEW, STATE_ENTER, STATE_ESCAPE, SET_MODE, STATE_DTOR = 0x488910, 0x4889a0, 0x488990, 0x488ad0, 0x48eb80
RENAME_FN, RESET_FN, REFRESH_FN = 0x487bd0, 0x487b30, 0x488770
CREATE, NEW_CAMPAIGN, OTHERS = 0x4788b0, 0x4345a0, 0x4785f0
# The four game states (kind: ctor, enter, resume, update, leave, escape).
GAME_STATES = {0: (0x48a6c0, 0x48a750, 0x48a7b0, None, None, 0x438c80),
               1: (0x48aef0, 0x48afd0, 0x48b000, 0x48af20, 0x48afa0, None),
               2: (0x48b120, 0x48b140, 0x48b170, None, None, 0x438c80),
               3: (0x49c900, 0x49cab0, 0x49cbb0, 0x49cb90, None, None)}
APP_FMV, APP_IA_ARG = 0x608e49, 0x608e47
PILOTS, CURRENT = 0x60860c, 0x60861c
MAIN_MENU = 0x609854
VT_GAME = {0x5bee08: 0, 0x5bec40: 1, 0x5bed50: 2, 0x5becf8: 3, 0x5beda8: 4}
FORMAT_ARGS = {0x27e: 2, 0x21d: 2, 0x280: 0, 0x281: 0}

test_twod.VT_KIND.update({v: 'C' for v in (0x5c5448, 0x5c53a8, 0x5bc230, 0x5c5278, 0x5c5310, 0x5c51d0, 0x5c5128,
                                           0x5c5080, 0x5c5ba0)})


# ---- dialogs ----

def mutate_list(r, entry):
    e = [list(x) if isinstance(x, list) else x for x in entry]

    def put(k, v):
        if k in e:
            e[e.index(k) + 1] = v
        else:
            e.extend([k, v])
    if r.random() < 0.4:
        put('SIZE', [r.choice([0, 1, 2, 3, 6, 9])])
    if r.random() < 0.4:
        put('SPACING', [r.choice([0, 3]), r.choice([10, 30, 56])])
    if r.random() < 0.3:
        put('OFFSET', [r.randrange(-10, 90), r.randrange(-10, 130)])
    if r.random() < 0.3:
        put('POSITION', [r.randrange(0, 400), r.randrange(0, 300)])
    if r.random() < 0.3 and 'LABEL' in e:
        del e[e.index('LABEL'):e.index('LABEL') + 2]
    if r.random() < 0.3:
        put('BAR', ['BITMAP', ['selbar'], 'OFFSET', [r.randrange(-5, 5), r.randrange(-3, 3)]])
    return e


def synthetic(seed, tree):
    r = random.Random(f'pilotscreen:{seed}')
    body = [list(x) if isinstance(x, list) else x for x in find(tree, 'PILOTDIALOG')]
    nb = []
    for i in range(0, len(body) - 1, 2):
        k, v = body[i], body[i + 1]
        if k == 'BUTTONS':
            out = []
            for j in range(0, len(v) - 1, 2):
                name, entry = v[j], v[j + 1]
                # The lists, the bars and SCENARIO_LIST reach the screen through their
                # owner unchecked: without them the exe crashes, so they stay.
                if name in ('PILOTLIST', 'GAMELIST'):
                    out += [name, mutate_list(r, entry)]
                elif name in ('EDITBAR', 'RENAMEBAR', 'SCENARIO_LIST', 'SCENARIO_DESC', 'SCENARIO_COMPLETE'):
                    if r.random() < 0.06 and name in ('SCENARIO_DESC', 'SCENARIO_COMPLETE'):
                        continue
                    e = [list(x) if isinstance(x, list) else x for x in entry]
                    if 'POSITION' in e and r.random() < 0.3:
                        e[e.index('POSITION') + 1] = [r.randrange(50, 450), r.randrange(20, 400)]
                    out += [name, e]
                else:
                    out += mutate_buttons(r, [name, entry], None)
            v = out
        elif k == 'PRIMITIVES' and r.random() < 0.2:
            v = []
        nb += [k, v]
    return [['PILOTDIALOG', nb]]


def spots_of(body):
    out = []
    bl = find(body, 'BUTTONS') or []
    for i in range(0, len(bl) - 1, 2):
        name, e = bl[i], bl[i + 1]
        p = find(e, 'POSITION')
        if not p:
            continue
        out.append(tuple(p))
        if name in ('PILOTLIST', 'GAMELIST'):
            off = find(e, 'OFFSET') or [0, 0]
            sp = find(e, 'SPACING') or [0, 12]
            n = (find(e, 'SIZE') or [8])[0]
            for k in range(max(min(n, 8), 1)):
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
    dt = r.choice([0.0, 1 / 60, 1 / 30, 0.05, 0.2])
    m = [x & 0xffffffff, y & 0xffffffff, 0, fbits(0.0), fbits(0.0), fbits(0.0), 0, 0, 0,
         fbits(0.0), fbits(0.0), fbits(0.0), left, r.choice([0, 0, 0, 1, 4]), 0]
    return 'F ' + ' '.join(f'{v:x}' for v in [fbits(dt)] + m)


def pilots_phase(r):
    lines, names = [], []
    for _ in range(r.choice([0, 1, 2, 3, 4, 6, 9])):
        n = r.choice(PILOT_NAMES)
        lines.append(f'C {hx(n)}')
        names.append(n)
        for _ in range(r.choice([0, 0, 1, 2, 3])):
            c, m = r.choice([(1, 2), (2, 3), (3, 1), (4, 6), (1, 1)])
            lines.append(f'N {hx(n)} {c} {m}')
            if r.random() < 0.8:
                lines.append(f'B {hx(f"pilots//games//{n}{c:02d}{m:02d}.mw3")} 00')
        for _ in range(r.randrange(0, 9)):
            g, off = r.choice([(2, 0x4c + 4 * k) for k in range(8)] + [(3, 0x98), (3, 0x9c), (4, 0x3c), (4, 0x3d),
                                                                     (4, 0x3e), (4, 0x3f), (4, 0x40)])
            lines.append(f'V {hx(n)} {g} 0 {off:x} {r.choice([0, 1, 2, 7, 300, 0xffffffff]):x}')
        lines.append(f'W {hx(n)}')  # saved (a new pilot is clean, so the changes would be lost)
    if r.random() < 0.15:
        lines.append(f'B {hx("pilots/" + r.choice(PILOT_NAMES) + ".mw3")} -')  # an empty file: read as a new pilot
    lines.append('Z')
    if r.random() < 0.3 and names:
        lines.append(f'X {hx(r.choice(names))}')  # the pilot chosen before
    return lines, names


def make_plan(seed, bound, spots):
    r = random.Random(f'plan:{seed}')
    lines, names = pilots_phase(r)
    lines.append('S')
    # Back from an Instant Action game (ia) there is a pilot: only with one chosen before.
    chosen = any(x.startswith('X ') for x in lines)
    lines.append(f'flags {r.randrange(2):x} {r.choice([0, 0, 1]) if chosen else 0:x}')
    kind = r.randrange(4)
    lines.append(f'g {kind} new')
    state = {'down': 0}
    mode = r.choice([1, 2, 3, 4])
    lines += [f'g {kind} enter' if r.random() < 0.7 else f'M {mode}', 'Y', 'A 1']
    for _ in range(r.randrange(40, 120)):
        roll = r.random()
        if roll < 0.03:
            lines += [f'M {r.choice([1, 2, 3, 4, 0])}']
        elif roll < 0.05:
            lines += ['Y', 'A 1']
        elif roll < 0.13 and bound:
            lines.append(f'P {r.choice(bound):x}')
        elif roll < 0.24:
            for _ in range(r.randrange(1, 7)):
                lines.append(f'K {r.choice(CHARS):x} {r.randrange(0x100):x}')
        elif roll < 0.28:
            lines.append(f'X {r.randrange(2)} {r.randrange(2)}')
        elif roll < 0.33:
            lines.append(f'L {r.randrange(-1, 6) & 0xffffffff:x}')
        elif roll < 0.36:
            lines.append(f'G {r.randrange(-1, 5) & 0xffffffff:x}')
        elif roll < 0.39:
            lines.append(f'Q {r.randrange(-1, 5) & 0xffffffff:x}')
        elif roll < 0.42:
            lines.append(f'R {r.randrange(2)} {r.randrange(2)}')
        elif roll < 0.45:
            lines.append(f'T {r.randrange(2)} {hx(r.choice(["", "Ace", "Bob", "New Name", "a_name_too_long_for_it"]))}')
        elif roll < 0.46:
            lines.append('O')
        elif roll < 0.47:
            lines.append(r.choice(['D', 'H', 'I', 'W']))
        elif roll < 0.49:
            lines.append('E')
        elif roll < 0.53:  # the game state hears how the screen ended, and goes on
            lines.append(f'g {kind} resume {r.choice([2, 2, 3, 4]):x}')
            for _ in range(r.randrange(0, 4)):
                lines.append(f'g {kind} update')
            if r.random() < 0.3:
                lines.append(f'g {kind} {r.choice(["leave", "escape"])}')
            if r.random() < 0.3:
                lines.append(f'g {kind} enter')
        lines.append(mouse_line(r, spots, state))
    lines += ['E', 'U', 'E']
    return lines


# ---- the original ----

class Original:
    def __init__(self, seed, data, start, cfg, fs):
        self.t = Trace(seed=seed)
        w = self.w = TwodWorld(self.t, seed)
        e = self.e = w.emu
        t = self.t
        self.tree = w.read_tree(data, start)
        for addr, spec in MECH3.items():
            if addr not in (0x4ac150, 0x576040):
                e.stub(addr, f'{addr:#010x}', spec)
        e.stub(0x576040, '0x00576040', 'c')
        self.files = Files(e.cpu, fs)

        def strdup(emu, key, a, pop):
            s = emu.cpu.read(a[0], 256).split(b'\0')[0] if a[0] else b''
            p = emu.alloc(len(s) + 1)
            emu.cpu.write(p, s + b'\0')
            emu._ret(p if a[0] else 0, 0)
        e.import_stub(IAT['_strdup'], '@_strdup', 's0', 0, strdup)

        def strchr(emu, key, a, pop):
            s, c = a
            d = emu.cpu.read(s, 512).split(b'\0')[0]
            c &= 0xff
            i = len(d) if c == 0 else d.find(bytes([c]))
            emu._ret(s + i if i >= 0 else 0, 0)
        e.import_stub(IAT['strchr'], '@strchr', 's0,s1', 0, strchr)

        def copy_rect(emu, key, a, pop):
            emu.cpu.write(a[0], emu.cpu.read(a[1], 16))
            emu._ret(1, 8)
        e.import_stub(IAT['CopyRect'], '@CopyRect', 's0,s1', 8, copy_rect)

        def sprintf(emu, key, a, pop):
            emu.cpu.write(a[0], emu.cpu.read(a[1], 256).split(b'\0')[0] + b'\0')  # the formats here have no %
            emu._ret(0, 0)
        e.import_stub(IAT['sprintf'], '@sprintf', 's0,s1', 0, sprintf)

        # MFC's CString: a pointer to a copy of the text (test_pilot.py).
        empty = e.world_string('')
        e.stub(0x5abdb0, '@cs', 'c', 0, lambda emu, k, a, p: (emu.w32(a[0], empty), emu._ret(a[0], 0)))
        e.stub(0x5abe04, '@cs(s)', 'c,s0', 4, lambda emu, k, a, p: (
            emu.w32(a[0], emu.world_string(emu.cstr(a[1]))), emu._ret(a[0], 4)))
        e.stub(0x5abda4, '@cs~', '', 0, lambda emu, k, a, p: emu._ret(0, 0))
        e.stub(0x5abdc8, '@cs.buf', 'c', 4, lambda emu, k, a, p: emu._ret(emu.r32(a[0]), 4))
        e.stub(0x5ac140, '@atexit', '', 0, lambda emu, k, a, p: emu._ret(0, 0))

        def cs_format(emu, key, a, pop):
            fmt = emu.cstr(a[1])
            vals = [emu.r32(emu.uc.reg_read(test_pilot.UC_X86_REG_ESP) + 12 + 4 * i) for i in range(4)]
            out, n, i = '', 0, 0
            while i < len(fmt):
                if fmt[i] != '%':
                    out += fmt[i]
                    i += 1
                    continue
                j = i + 1
                while fmt[j] in '0123456789':
                    j += 1
                out += emu.cstr(vals[n]) if fmt[j] == 's' else fmt[i:j + 1] % i32(vals[n])
                n += 1
                i = j + 1
            emu.w32(a[0], emu.world_string(out))
            emu._ret(a[0], 0)
        e.stub(0x5abdaa, '@cs.format', 's0,s1', 0, cs_format)

        def store_fill(emu, key, a, pop):
            emu._ret(t.handle(key, (VT_GAME[emu.r32(a[0] - 8)],)), 0)
        e.stub(0x470e30, '0x00470e30', 'c', 0, store_fill)
        e.stub(0x4733b0, '0x004733b0', 's0,zs1', 8)

        def fmt(emu, key, a, pop):
            buf, size, mid = a[:3]
            vals = [i32(x) for x in a[3:3 + FORMAT_ARGS.get(mid, 1)]]
            t.handle(key, (mid, *vals))
            text = (f'F{mid:x}:' + ','.join(str(v) for v in vals))[:size - 1]
            emu.cpu.write(buf, text.encode('latin1') + b'\0')
            emu._ret(len(text), 0)
        e.stub(0x575fd0, '0x00575fd0', 's0,s1,s2,s3,s4', 0, fmt)

        def msgbox(emu, key, a, pop):
            if a[3] < 0x10000:  # FIX (bug), as the port: no question without a pilot
                emu._ret(0, 0)
                return
            emu._ret(t.handle(key, (a[0], a[1], a[2], emu.cstr(a[3]))), 0)
        e.stub(0x4ac150, '0x004ac150', 's0,s1,s2,s3', 0, msgbox)
        e.stub(0x51a9b0, '0x0051a9b0', 'c')
        e.stub(0x586ce0, '0x00586ce0', '')
        e.stub(0x586d10, '0x00586d10', '')
        e.stub(0x588a60, '0x00588a60', 'c', None,
               lambda emu, k, a, p: emu._ret(emu.world_string(f'Lobby{t.handle(k, a) & 0xff:x}'), p))

        def char_cb(emu, key, a, pop):
            cb, ctx = a
            e.w32(0x63c49c, cb)
            e.w32(0x63c4a0, ctx)
            t.handle(key, (1 if cb else 0, ctx - self.m if ctx else 0))
            emu._ret(0, 0)
        e.stub(0x51a9a0, '0x0051a9a0', 'c,d', 0, char_cb)
        e.stub(0x562940, '0x00562940', 'c,d,s0,s1,s2', 0xc)
        e.stub(0x48ebf0, '@shell_enter', 'c')
        e.stub(0x48ec40, '@shell_leave', 'c')
        e.stub(0x48ecc0, '@shell_resume', 'c,s0')
        e.stub(0x48ed30, '@shell_update', 'c')
        for addr in (0x488bc0, 0x493260, 0x495f90, 0x49bd80):  # the Action and multiplayer screens
            e.stub(addr, f'{addr:#010x}', 's0')
        e.stub(0x48aac0, '0x0048aac0', '')
        e.stub(0x494920, '0x00494920', 'c')
        e.stub(0x47fc40, '0x0047fc40', 's0')
        e.stub(0x49cb20, '0x0049cb20', 'c')
        e.stub(0x49b660, '0x0049b660', 'c')
        for addr, spec in ((0x598cc0, 's0,s1,s2'), (0x598d80, 'c,zs0,zs1'), (0x5996d0, 'c'), (0x599710, 'c'),
                           (0x599750, 'c,s0'), (0x598d00, 'c')):
            e.stub(addr, f'{addr:#010x}', spec)
        self.queue = []
        e.stub(0x482970, '@state', 's0', None, lambda emu, k, a, p: emu._ret(0x608f18 if a[0] == 6 else 0, p))

        def replace(emu, key, a, pop):
            self.queue.append((3, a[0], a[1]))
            emu._ret(0, pop)
        e.stub(0x590250, '@replace', 's0,s1', None, replace)
        w.answers.update({
            '0x0040cec0': (lambda rr, k, a: rr.randrange(2)) if cfg['fast'] == 'random' else cfg['fast'],
            '0x0040d760': cfg['size'][0], '0x0040d770': cfg['size'][1],
            '0x00576040': lambda rr, k, a: e.world_string(f'M{a[0]:x}'),
            '0x004ac150': lambda rr, k, a: rr.choice([1, 1, 2, 6]),
            '0x0051a9b0': lambda rr, k, a: 1 if rr.random() < 0.04 else 0,
            '0x00586ce0': lambda rr, k, a: 1 if rr.random() < cfg['lobby'] else 0,
            '0x00586d10': lambda rr, k, a: 0x5e55000 + rr.randrange(0x100),
            '0x00588a60': lambda rr, k, a: a[0],
            '0x004733b0': 1,
            '0x00488bc0': 0,
            '0x00493260': lambda rr, k, a: e.world_alloc(0x16000),
            '0x00495f90': lambda rr, k, a: e.world_alloc(0x16000),
            '0x0049bd80': lambda rr, k, a: e.world_alloc(0x16000),
            '0x00598cc0': lambda rr, k, a: 0x6f000000 + k,
            '0x00598d80': lambda rr, k, a: rr.choice([-1 & 0xffffffff, 0, 1]),
            '0x00599710': lambda rr, k, a: rr.choice([0, 1, 1]),
        })

        # FIX (bug), as the port: an empty list's rectangle ends at its top (test_loadsave.py).
        from unicorn import UC_HOOK_CODE
        from unicorn.x86_const import UC_X86_REG_ESP

        def empty_rect(uc, address, size, user):
            esp = uc.reg_read(UC_X86_REG_ESP)
            if i32(e.r32(esp + 0x10)) <= 0:
                e.w32(esp + 0x30, e.r32(esp + 0x1c))
        e.uc.hook_add(UC_HOOK_CODE, empty_rect, begin=0x48d06d, end=0x48d06d)

        # FIX (bug), as the port: deleting the current pilot clears [0x0060861c] (pilot.md).
        from unicorn.x86_const import UC_X86_REG_EAX

        def deleted(uc, address, size, user):
            p = uc.reg_read(UC_X86_REG_EAX)
            if p and e.r32(CURRENT) == p:
                e.w32(CURRENT, 0)
        e.uc.hook_add(UC_HOOK_CODE, deleted, begin=0x477f56, end=0x477f56)

        menu = e.world_alloc(0x12700)
        e.world_w32(menu + 0x12600, self.tree)
        e.world_w32(menu + 0x110ec, w.fake_texture(1280, 960))
        e.cpu.write(menu + 0x111e8, bytes([cfg['moving']]))
        e.world_bytes[menu + 0x111e8] = cfg['moving']
        e.world_w32(MAIN_MENU, menu)
        e.call(test_pilot.LIST_INIT)
        e.w32(CURRENT, 0)
        self.state = e.world_alloc(0x18)
        e.world_w32(0x609858, self.state)
        self.games = {k: e.world_alloc(0x30) for k in GAME_STATES}
        self.m = 0

    def vcall(self, obj, slot, stack=()):
        return self.e.call(self.e.r32(self.e.r32(obj) + slot), ecx=obj, stack=stack)

    def find(self, name):
        return self.e.call(test_pilot.FIND, ecx=self.e.world_string(name))

    def run(self, plan):
        e, w = self.e, self.w
        dump = ''
        S = self.state
        for line in plan:
            p = line.split()
            op = p[0]
            pv = [int(x, 16) if re.fullmatch(r'[0-9a-f]+', x) else x for x in p[1:]]
            m = self.m
            if op == 'C':
                e.call(CREATE, ecx=e.world_string(unhx(p[1])))
            elif op == 'N':
                pl = self.find(unhx(p[1]))
                if pl:
                    o = e.alloc(200)
                    e.call(NEW_CAMPAIGN, ecx=o, stack=(pl, int(p[2]), int(p[3])))
                    first, last = e.r32(pl + 0x60), e.r32(pl + 0x64)
                    items = [e.r32(first + 4 * i) for i in range((last - first) // 4)] + [o]
                    arr = e.alloc(4 * len(items))
                    for i, x in enumerate(items):
                        e.w32(arr + 4 * i, x)
                    e.w32(pl + 0x60, arr)
                    e.w32(pl + 0x64, arr + 4 * len(items))
                    e.w32(pl + 0x68, arr + 4 * len(items))
            elif op == 'V':
                pl = self.find(unhx(p[1]))
                if pl:
                    g = e.r32(pl + {2: 0x70, 3: 0x74, 4: 0x78}[int(p[2])])
                    off, v = int(p[4], 16), int(p[5], 16)
                    size = 1 if int(p[2]) == 4 and 0x3c <= off <= 0x3f else 4
                    e.cpu.write(g + off, struct.pack('<I', v)[:size])
            elif op == 'B':
                self.files.fs.put(unhx(p[1]), bytes.fromhex(p[2]) if p[2] != '-' else b'')
            elif op == 'Z':
                e.w32(CURRENT, 0)
                e.call(OTHERS)
            elif op == 'W' and not m:
                pl = self.find(unhx(p[1]))
                if pl:
                    e.call(test_pilot.OPEN, ecx=pl)
                    if e.call(test_pilot.HEADER, ecx=pl) & 0xff:
                        e.call(e.r32(e.r32(pl) + 4), ecx=pl)
                    h = e.r32(pl + 0x10)
                    self.files.handles.pop(h, None)
                    e.w32(pl + 0x10, 0xffffffff)
            elif op == 'X' and len(p) == 2:
                e.w32(CURRENT, self.find(unhx(p[1])))
            elif op == 'S':
                self.snapshot = e.snapshot()
                e.call(STATE_NEW, ecx=S)
                self.m = self.d = e.r32(S + 0xc)
            elif op == 'M':
                e.call(SET_MODE, ecx=S, stack=(pv[0],))
            elif op == 'Y':
                e.call(STATE_ENTER, ecx=S)
            elif op == 'U':
                e.call(STATE_DTOR, ecx=S)
                self.m = 0
            elif op == 'flags':
                e.cpu.write(APP_FMV, bytes([pv[0]]))
                e.cpu.write(APP_IA_ARG, bytes([pv[1]]))
            elif op == 'g':  # a game state's step: kind, what, argument
                k, what = pv[0], p[2]
                ctor, enter, resume, update, leave, escape = GAME_STATES[k]
                G = self.games[k]
                if what == 'new':
                    e.call(ctor, ecx=G)
                elif what == 'enter':
                    e.call(enter, ecx=G)
                elif what == 'resume':  # (a pilot accepted with none: unreachable for these two)
                    if not (pv[2] == 2 and k in (0, 2) and not e.r32(CURRENT)):
                        e.call(resume, ecx=G, stack=(pv[2],))
                elif what == 'update' and update:
                    e.call(update, ecx=G)
                elif what == 'leave' and leave:
                    e.call(leave, ecx=G)
                elif what == 'escape' and escape:
                    e.call(escape, ecx=G)
            elif op == 'E' and not m:
                dump += 'gone\n' + self.dump_pilots() + self.dump_games()
            elif not m:
                continue
            elif op == 'A':
                self.vcall(m, 0x04, (pv[0],))
            elif op == 'F':
                w.mouse_frames.append(pv[1:])
                self.vcall(m, 0x00, (pv[0],))
                w.frame += 1
            elif op == 'P':
                self.vcall(m + PARTS[pv[0]], 0x30)
            elif op == 'K':
                cb = e.r32(0x63c49c)
                if cb:
                    e.call(cb, ecx=pv[0], edx=pv[1], stack=(e.r32(0x63c4a0),))
            elif op == 'X':
                self.vcall(m + BARS[pv[0]], 0x90 + 4 * pv[1])
            elif op == 'L':
                self.vcall(m + PILOTLIST, 0x98, (pv[0],))
            elif op == 'G':  # (a save chosen with no pilot: unreachable, the list is empty)
                if e.r32(m + CHOSEN) or e.r32(m + MODE) != 1:
                    self.vcall(m + GAMELIST, 0x98, (pv[0],))
            elif op == 'Q':
                self.vcall(m + SCENARIO, 0xa0, (pv[0],))
            elif op == 'R':
                e.call(RENAME_FN, ecx=m, stack=(pv[0], pv[1]))
            elif op == 'T':
                e.call(0x5a52a0, ecx=m + BARS[pv[0]], stack=(e.world_string(unhx(p[2])),))
            elif op == 'O':
                e.call(STATE_ESCAPE, ecx=S)
            elif op == 'I':
                e.call(RESET_FN, ecx=m)
            elif op == 'W':
                e.call(REFRESH_FN, ecx=m, stack=(1, 0))
            elif op == 'D':
                e.call(0x59fa60, ecx=m)
            elif op == 'H':
                e.call(0x59fa40, ecx=m)
            elif op == 'E':
                dump += test_twod.Original.dump(self) + self.dump_screen() + self.dump_games()
        return dump

    # The dump probe_pilotscreen.cpp writes, from the emulator's memory.
    def dump_screen(self):
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
            for k, off, t_ in (('inorm', 0xdc, 1), ('idis', 0xe0, 1), ('iroll', 0xe4, 1), ('sroll', 0xe8, 0),
                               ('proll', 0xec, 0), ('vroll', 0xf0, 0), ('iact', 0xf4, 1), ('sact', 0xf8, 0),
                               ('pact', 0xfc, 0), ('vact', 0x100, 0), ('sdis', 0x104, 0)):
                (tx if t_ else kv)(k, e.r32(o + off))
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

        def name_of(pl):
            return f'"{e.cstr(pl + 0x18)}"' if pl else '-'

        def file_list(f, values):
            control(f)
            for k, off in (('selected', 0x17c), ('top', 0x180), ('size', 0x184)):
                kv(k, e.r32(f + off))
            rc('rect', f + 0x188)
            kv('arrow', e.r32(f + 0x1dc))
            kv('inside', e.r8(f + 0x1e8))
            first, last = e.r32(f + 0x16c), e.r32(f + 0x170)
            rows = [e.r32(first + 4 * i) for i in range((last - first) // 4)] if first else []
            lit, sel = e.r32(f + 0x1ec), e.r32(f + 0x178)
            kv('lit', 100 if lit and lit == sel else rows.index(lit) if lit in rows else -1)
            kv('rep', e.r8(f + 0x1fc))
            first, last = e.r32(f + 0x15c), e.r32(f + 0x160)
            items = [e.r32(first + 4 * i) for i in range((last - first) // 4)] if first else []
            kv('n', len(items))
            for it in items:
                s('item', e.r32(it))
                if values:
                    kv('v', e.r32(it + 4))
            for i, rw in enumerate(rows):
                out.append(f'\n row{i} ')
                text(rw)
                kv('index', e.r32(rw + 0x4d0))
            if sel:
                out.append('\n selrow ')
                text(sel)
                kv('index', e.r32(sel + 0x4d0))

        def bar(b):
            control(b)
            kv('focus', e.r8(b + 0x374))
            kv('active', e.r8(b + 0x38c))
            kv('reading', e.r8(b + 0x388))
            kv('lx', e.r32(b + 0x380))
            kv('ly', e.r32(b + 0x384))
            ed = b + 0x14c
            kv('size', e.r32(ed + 0xc))
            kv('cursor', e.r32(ed + 0x10))
            s('buf', e.r32(ed + 8))
            s('reject', e.r32(ed + 4))

        def cycle(c):
            control(c)
            kv('index', e.r32(c + 0x16c))
            first, last = e.r32(c + 0x15c), e.r32(c + 0x160)
            items = [e.r32(first + 4 * i) for i in range((last - first) // 4)] if first else []
            for it in items:
                s('item', e.r32(it))
                kv('v', e.r32(it + 4))

        cur = e.r32(CURRENT)
        out.append(f'screen mode={i32(e.r32(m + MODE))} chosen={name_of(e.r32(m + CHOSEN))} '
                   f'current={name_of(cur)} x200={e.r8(m + 0x12194)} pstate={e.r32(m + 0x10fec):x}')
        for name, off, fn in (('pilots', PILOTLIST, lambda o: file_list(o, False)),
                              ('games', GAMELIST, lambda o: file_list(o, True)),
                              ('newbar', EDITBAR, bar), ('renamebar', RENAMEBAR, bar),
                              ('scenarios', SCENARIO, cycle), ('desc', DESC, cycle), ('complete', COMPLETE, cycle)):
            out.append(f'\n{name} ')
            fn(m + off)
        out.append('\n' + self.dump_pilots())
        return ''.join(out)

    def dump_games(self):
        e = self.e
        out = [f'queue {self.queue} fmv={e.r8(APP_FMV)} ia={e.r8(APP_IA_ARG)}\n']
        for k, G in self.games.items():
            out.append(f'game{k} movie={e.r32(G + 0x10):x} want={e.r32(G + 0x14):x} x19={e.r8(G + 0x19)}\n')
        return ''.join(out)

    def pilot_list(self):
        e = self.e
        head = e.r32(PILOTS)
        ps, n = [], e.r32(head)
        while n != head:
            ps.append(e.r32(n + 8))
            n = e.r32(n)
        return ps

    def dump_pilots(self):
        e = self.e
        ps = self.pilot_list()
        out = [f'list {len(ps)}\n']
        for pl in ps:
            cur = e.r32(pl + 0x6c)
            first, last = e.r32(pl + 0x60), e.r32(pl + 0x64)
            camps = [e.r32(first + 4 * i) for i in range((last - first) // 4)] if first else []
            kind = VT_GAME.get(e.r32(cur), -9) if cur else -99
            idx = camps.index(cur) if cur in camps else -1
            training = e.r32(pl + 0x78)
            out.append(f'pilot "{e.cstr(pl + 0x18)}" dirty={e.r8(pl + 0x58)} game={kind} campaign={idx} '
                       f'ncamp={len(camps)} training={i32(e.r32(training + 0x40))}\n')
        return ''.join(out)


def run_case(seed, path, data, start, cfg, plan_args, verbose):
    fs = FS()
    orig = Original(seed, data, start, cfg, fs)
    plan = [re.sub(r'^g (\d) new$', lambda mm: f'g {mm.group(1)} new {orig.games[int(mm.group(1))]:x}', x)
            for x in make_plan(seed, *plan_args)]
    dump_o = orig.run(plan)
    rep = Replay(orig.t, orig.e, orig.snapshot)
    probe.probe_app_set_callback(ctypes.cast(rep.callback(), ctypes.c_void_p))
    d = test_pilot.DIR.parent / 'pilotscreen' / str(seed)
    shutil.rmtree(d, ignore_errors=True)
    d.mkdir(parents=True)
    buf = ctypes.create_string_buffer(1 << 23)
    n = probe.probe_pilotscreen_run(str(path).encode(), str(d).encode(), orig.state, '\n'.join(plan).encode(), buf,
                                    len(buf))
    assert n >= 0, 'probe failed'
    dump_r = buf.value.decode('latin1')
    stubbed = orig.w.stub_textures
    to, do = canonical(orig.t.log, dump_o, stubbed)
    tr, dr = canonical(rep.log, dump_r, stubbed)
    do, dr = (re.sub(r'face=".*?" height=', 'height=', x, flags=re.S) for x in (do, dr))
    ok = compare(f'pilotscreen seed {seed}', to, tr)
    if do != dr:
        ok = False
        for a, b in zip(do.split('\n'), dr.split('\n')):
            if a != b:
                print(f'pilotscreen seed {seed}: dumps differ')
                ta, tb = a.split(' '), b.split(' ')
                print('  original:', ' '.join(x for x in ta if x not in tb)[:400])
                print('  rebuild: ', ' '.join(x for x in tb if x not in ta)[:400])
                print('  in:', a[:200])
                break
        else:
            print(f'pilotscreen seed {seed}: dumps differ in length')
    fs_o, fs_p = fs.snapshot(), folder_files(d)
    if [(k.lower(), v) for k, v in fs_o] != [(k.lower(), v) for k, v in fs_p]:
        ok = False
        print(f'pilotscreen seed {seed}: files differ: {[k for k, _ in fs_o]} / {[k for k, _ in fs_p]}')
    if ok:
        shutil.rmtree(d, ignore_errors=True)
    if verbose and ok:
        print(f'pilotscreen seed {seed}: {len(to)} calls, {len(fs_o)} files')
    return ok, to


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    runs = int(args[0]) if args else 24
    first = int(args[1]) if len(args) > 1 else 0
    verbose = '--verbose' in sys.argv
    data = READER.read_bytes()
    fname = 'dialog.zrd'
    start = next(s for n, s, _ in entries(data) if n == fname)
    tree, _ = parse_node(data, start)
    synth_path = ROOT / 'build' / 'pilotscreen_synth.zbd'
    passed = calls = 0
    stats = {}
    for seed in range(first, first + runs):
        if verbose:
            print('seed', seed, flush=True)
        r = random.Random(f'cfg:{seed}')
        cfg = {'fast': r.choice([0, 0, 1, 'random']), 'size': r.choice(SIZES), 'moving': r.choice([0, 1, 1]),
               'lobby': r.choice([0, 0, 0, 0.3])}
        if seed % 2:
            t = synthetic(seed, tree)
            blob = write_node(t)
            synth_path.write_bytes(archive([(fname, blob)]))
            path, file_data, file_start, here = synth_path, blob, 0, t
        else:
            path, file_data, file_start, here = READER, data, start, tree
        body = find(here, 'PILOTDIALOG') or []
        names = [x for x in (find(body, 'BUTTONS') or [])[::2] if isinstance(x, str)]
        bound = [i for i, nm in enumerate(NAMES) if nm in names]
        spots = spots_of(body) or [(250, 150)]
        ok, trace = run_case(seed, path, file_data, file_start, cfg, (bound, spots), verbose)
        passed += ok
        calls += len(trace)
        for k, _ in trace:
            if k in ('0x004ac150', '0x005905a0', '0x00575fd0', '0x0051a9a0', '0x00470e30', '0x004733b0',
                     '0x00562940', 'DrawTextA', '0x00586d10', '0x00590400', '0x00598cc0', '0x0048aac0', '0x0047fc40',
                     '0x0049cb20', '0x00494920'):
                stats[k] = stats.get(k, 0) + 1
    print(f'pilotscreen: {passed}/{runs} runs identical ({calls} calls; ' +
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
