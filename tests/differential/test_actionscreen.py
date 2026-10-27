"""The Action screen and its state (docs/menus.md, The Action screen) against the original x86.

    cmake --build build && .venv/bin/python -P tests/differential/test_actionscreen.py [runs] [first] [--verbose]

The original builds the Action State 0x0048a6c0 and with it the screen
0x00488bc0 from the main menu's dialog.zrd tree (ACTIONDIALOG; a fake main
menu at [0x00609854] holds the tree, its BACKGROUND image and whether it
moves), before or after a pilot is made (0x004788b0 over test_pilot.py's fake
file system) with Instant Action its game (0x00478250), its best wave
(+0x68) and its mech (+0x24) set; the kept choices [0x00605820..0x0060587f]
set at random. Each mech's definition (0x004846a0, stubbed) has none, or
loadouts made by the exe's own 0x00423d00 (some for training), and _access
(0x00576080) answers the scenario's files at random. Then a plan: the state's
enter (back from a game or to the pilot screen) and resume, the screen's
reset (0x0048aac0), frames of mouse input over the cycles and their arrows
(Window::update), presses (+0x30), choices (+0xa0) and steps (+0xa4) of
every cycle, the wave and custom fills (0x0048a810, 0x0048a930), CANCEL and
ACCEPT (+0x14: the ActionGame filled, the pilot saved, pop and replace).
The port (tests/probe_actionscreen.cpp) runs the same plan on the same tree,
its pilot file in a real folder. Compared: every call out, a dump of the
dialog and its objects, every cycle (control, index, arrows, fonts, offsets,
items), the screen's fill state, the kept choices, the pilot's ActionGame
(mode, field base, waves, world, mission, texts, the line-up with each
slot's loadout by name) and the files' bytes. Even runs use the shipped
ACTIONDIALOG, odd runs a synthetic one. The x87 runs at 24 bits.

FIX (bug) in the port, applied to the original here: a wave past the table's
20 (the reset with the best wave 20) reads the strings after it as mechs;
the port and the hooked original read none.

Without a pilot the exe's reset in Progression and its ACCEPT read the
pilot's game through null (crashes); the plan does neither then. The port's
ACCEPT returns at once without one.
"""
import ctypes
import os
import random
import re
import shutil
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
import test_pilotscreen  # noqa: E402

probe = load_probe()
probe.probe_app_set_callback.argtypes = [ctypes.c_void_p]
probe.probe_actionscreen_run.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint32, ctypes.c_char_p,
                                         ctypes.c_char_p, ctypes.c_int]

WORLDS, SCENARIOS, DESC, WAVES = 0x111ec, 0x11618, 0x11a44, 0x16b68
ALLIES = [0x11fc8 + 0x434 * i for i in range(5)]
ENEMIES = [0x138f8 + 0x868 * i for i in range(6)]
CONFIGS = [0x13d2c + 0x868 * i for i in range(6)]
ETITLE, CANCEL, ACCEPT = 0x134cc, 0x16fa8, 0x17194
PARTS = [WORLDS, SCENARIOS, DESC, WAVES] + ALLIES + ENEMIES + CONFIGS + [CANCEL, ACCEPT]
FIXED, WAVE, FILLING = 0x16f9c, 0x16fa0, 0x16fa4
STATE_NEW, STATE_ENTER, STATE_RESUME = 0x48a6c0, 0x48a750, 0x48a7b0
RESET, SHOW_WAVE, SHOW_CUSTOM = 0x48aac0, 0x48a810, 0x48a930
CREATE, SELECT_GAME, CONFIG_NEW = 0x4788b0, 0x478250, 0x423d00
APP_IA_ARG, CURRENT, MAIN_MENU, PILOT_STATE = 0x608e47, 0x60861c, 0x609854, 0x609858
SETUP = 0x605820
VT_GAME = test_pilotscreen.VT_GAME
# 0x00575fd0's arguments by message: a mech's name, none, a number.
STRING_FORMATS, NO_ARGS = {0x18b, 0x28d, 0x28f}, {0x28c, 0x28e}
PILOT_NAMES = ['Bob', 'Alice', 'Zed', 'Major Tom', 'Kerensky']

test_twod.VT_KIND.update({v: 'C' for v in (0x5c5870, 0x5c57c8, 0x5c5720, 0x5c5918, 0x5c59c0, 0x5c5a68, 0x5c5678,
                                           0x5c55d8, 0x5c5538, 0x5bc230, 0x5c5ba0)})


# ---- dialogs ----

def synthetic(seed, tree):
    r = random.Random(f'actionscreen:{seed}')
    body = [list(x) if isinstance(x, list) else x for x in find(tree, 'ACTIONDIALOG')]
    nb = []
    for i in range(0, len(body) - 1, 2):
        k, v = body[i], body[i + 1]
        if k == 'BUTTONS':
            out = []
            for j in range(0, len(v) - 1, 2):
                name, entry = v[j], v[j + 1]
                e = [list(x) if isinstance(x, list) else x for x in entry]

                def put(key, val):
                    if key in e:
                        e[e.index(key) + 1] = val
                    else:
                        e.extend([key, val])
                if name in ('ALLY_LIST', 'ENEMY_LIST', 'ENEMY_CONFIG_LIST'):
                    if r.random() < 0.4:
                        put('OFFSET', [r.randrange(-10, 40), r.randrange(-5, 30)])
                    if r.random() < 0.3:
                        put('POSITION', [r.randrange(20, 300), r.randrange(20, 300)])
                    if r.random() < 0.15 and 'OFFSET' in e:
                        del e[e.index('OFFSET'):e.index('OFFSET') + 2]
                if 'LABEL' in e and r.random() < 0.2:
                    lab = e[e.index('LABEL') + 1]
                    if isinstance(lab, list) and 'inactive_font' in lab:
                        lab = list(lab)
                        del lab[lab.index('inactive_font'):lab.index('inactive_font') + 2]
                        e[e.index('LABEL') + 1] = lab
                if name in ('CANCEL', 'ACCEPT', 'ALLY_TITLE'):
                    out += mutate_buttons(r, [name, e], None)
                else:
                    if r.random() < 0.2 and 'POSITION' in e:
                        e[e.index('POSITION') + 1] = [r.randrange(50, 450), r.randrange(20, 400)]
                    out += [name, e]
            v = out
        elif k == 'PRIMITIVES' and r.random() < 0.2:
            v = []
        nb += [k, v]
    return [['ACTIONDIALOG', nb]]


def spots_of(body):
    out = []
    bl = find(body, 'BUTTONS') or []
    for i in range(0, len(bl) - 1, 2):
        name, e = bl[i], bl[i + 1]
        p = find(e, 'POSITION')
        if not p:
            continue
        off = find(e, 'OFFSET') or [0, 0]
        n = 5 if name == 'ALLY_LIST' else 6 if name in ('ENEMY_LIST', 'ENEMY_CONFIG_LIST') else 1
        for k in range(n):
            x, y = p[0] + off[0] * k, p[1] + off[1] * k
            out.append((x + 20, y + 3))
            for key in ('TOGGLE_LEFT', 'TOGGLE_RIGHT'):
                a = find(e, key)
                ap = find(a, 'POSITION') if a else None
                if ap:
                    out.append((x + ap[0] + 3, y + ap[1] + 3))
    return out


# ---- the plan ----

def mouse_line(r, spots, state):
    sx, sy = r.choice(spots)
    if r.random() < 0.85:
        x, y = sx + r.randrange(-3, 8), sy + r.randrange(-3, 8)
    else:
        x, y = r.randrange(-20, 660), r.randrange(-20, 500)
    if state['down'] == 0:
        left = 1 if r.random() < 0.35 else 0
    elif state['down'] in (1, 2):
        left = 2 if r.random() < 0.6 else 4
    else:
        left = 0
    state['down'] = left
    dt = r.choice([0.0, 1 / 60, 1 / 30, 0.05])
    m = [x & 0xffffffff, y & 0xffffffff, 0, fbits(0.0), fbits(0.0), fbits(0.0), 0, 0, 0,
         fbits(0.0), fbits(0.0), fbits(0.0), left, r.choice([0, 0, 0, 1, 4]), 0]
    return 'F ' + ' '.join(f'{v:x}' for v in [fbits(dt)] + m)


def make_plan(seed, spots):
    r = random.Random(f'plan:{seed}')
    lines = []
    for mech in range(19):
        if r.random() < 0.08:
            lines.append(f'D {mech:x} -')
        else:
            n = r.choice([0, 1, 1, 2, 3, 5])
            lines.append(f'D {mech:x} ' + ' '.join('1' if r.random() < 0.2 else '0' for _ in range(n)))
    for i in [k for k in range(24) if k not in (5, 12, 13, 20)]:
        if r.random() < 0.6:
            # (the kept world's row stays real: ACCEPT reads its name unchecked)
            v = r.choice([-1, 0, 0, 1, 2, 3, 5, 19, 25]) if i < 21 else r.randrange(4) if i == 21 \
                else r.randrange(-1, 7) if i == 22 else r.choice([0, 1, 2, 3, 4, 7, 19, 20, -1])
            lines.append(f'K {i:x} {v & 0xffffffff:x}')
    name = r.choice(PILOT_NAMES)
    pilot = [f'C {hx(name)}', f'V 68 {r.choice([0, 0, 1, 3, 10, 19, 20, 25, -1]) & 0xffffffff:x}']
    if r.random() < 0.8:
        pilot.append(f'V 24 {r.choice(list(range(-1, 20)) + [3, 3]) & 0xffffffff:x}')
    early = r.random() < 0.5  # the screen made before the pilot (as at the main menu)
    if not early:
        lines += pilot
    lines += ['S', 'A 1']
    if early:
        lines += pilot
    state = {'down': 0}
    lines.append(f'flags {r.randrange(2):x}')
    lines += ['Y'] if r.random() < 0.7 else ['I']
    for _ in range(r.randrange(30, 90)):
        roll = r.random()
        if roll < 0.12:
            lines.append(f'P {r.randrange(21):x} 0')
        elif roll < 0.24:
            k = r.randrange(21)
            v = r.randrange(4) if k == 0 else r.randrange(-2, 22)  # (the world's row, as above)
            lines.append(f'Q {k:x} {v & 0xffffffff:x}')
        elif roll < 0.36:
            lines.append(f'T {r.randrange(21):x} {r.choice([-1, 1, 1, 0, 2, -3]) & 0xffffffff:x}')
        elif roll < 0.39:
            lines.append(f'W {r.choice([0, 1, 5, 12, 19]):x}')
        elif roll < 0.42:
            lines.append('M')
        elif roll < 0.45:
            lines.append('I')
        elif roll < 0.47:
            lines.append(f'R {r.choice([2, 2, 3, 4]):x}')
        elif roll < 0.49:
            lines.append('Y')
        elif roll < 0.51:
            lines.append(f'B {r.choice([3, 3, 4, 4, 0, 7]):x}')
        elif roll < 0.53:
            lines.append(f'P {r.choice([21, 22]):x} 0')
        elif roll < 0.56:
            lines.append('E')
        lines.append(mouse_line(r, spots, state))
    lines += ['B 4', 'E']
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
            if addr not in (0x576040, 0x482970):
                e.stub(addr, f'{addr:#010x}', spec)
        e.stub(0x576040, '0x00576040', 'c')
        e.stub(0x482970, '@state', 's0', None, lambda emu, k, a, p: emu._ret(0x608f18 if a[0] == 6 else 0, p))
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

        def realloc(emu, key, a, pop):  # ActionGame's texts (0x004368e0): a fresh block, the old text copied
            old, size = a
            p = emu.alloc(max(size, 1))
            if old:
                emu.cpu.write(p, emu.cpu.read(old, size))
            emu._ret(p, 0)
        e.import_stub(IAT['realloc'], '@realloc', 's0,s1', 0, realloc)

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
            if mid in STRING_FORMATS:
                s = emu.cstr(a[3]) if a[3] else ''
                t.handle(key, (mid, s))
                text = f'F{mid:x}:{s}'
            elif mid in NO_ARGS:
                t.handle(key, (mid,))
                text = f'F{mid:x}:'
            else:
                n = test_pilotscreen.FORMAT_ARGS.get(mid, 1)
                vals = [i32(x) for x in a[3:3 + n]]
                t.handle(key, (mid, *vals))
                text = f'F{mid:x}:' + ','.join(str(v) for v in vals)
            text = text[:size - 1]
            emu.cpu.write(buf, text.encode('latin1') + b'\0')
            emu._ret(len(text), 0)
        e.stub(0x575fd0, '0x00575fd0', 's0,s1,s2,s3,s4', 0, fmt)

        self.dfns = {}
        def dfn(emu, key, a, pop):
            t.handle(key, a)
            emu._ret(self.dfns.get(i32(a[0]), 0) if 0 <= i32(a[0]) < 19 else 0, 0)
        e.stub(0x4846a0, '0x004846a0', 'c', 0, dfn)
        e.stub(0x576080, '0x00576080', 'zc')
        e.stub(0x488ad0, '0x00488ad0', 'c,s0')
        e.stub(0x48ebf0, '@shell_enter', 'c')
        e.stub(0x48ec40, '@shell_leave', 'c')
        e.stub(0x48ecc0, '@shell_resume', 'c,s0')
        e.stub(0x48ed30, '@shell_update', 'c')
        w.answers.update({
            '0x0040cec0': (lambda rr, k, a: rr.randrange(2)) if cfg['fast'] == 'random' else cfg['fast'],
            '0x0040d760': cfg['size'][0], '0x0040d770': cfg['size'][1],
            '0x00576040': lambda rr, k, a: e.world_string(f'M{a[0]:x}'),
            '0x004733b0': 1,
            '0x00576080': lambda rr, k, a: 1 if rr.random() < cfg['exists'] else 0,
        })

        # FIX (bug), as the port: a wave outside the table (the best 20, from the
        # reset) reads its mechs as none (the exe reads the strings after it).
        from unicorn import UC_HOOK_CODE
        from unicorn.x86_const import UC_X86_REG_EBP, UC_X86_REG_EDI

        def past_table(uc, address, size, user):
            if not 0x5e33a8 <= uc.reg_read(UC_X86_REG_EBP) < 0x5e33a8 + 20 * 0x30:
                uc.reg_write(UC_X86_REG_EDI, 0xffffffff)
        e.uc.hook_add(UC_HOOK_CODE, past_table, begin=0x48a88c, end=0x48a88c)

        menu = e.world_alloc(0x12700)
        e.world_w32(menu + 0x12600, self.tree)
        e.world_w32(menu + 0x110ec, w.fake_texture(1280, 960))
        e.cpu.write(menu + 0x111e8, bytes([cfg['moving']]))
        e.world_bytes[menu + 0x111e8] = cfg['moving']
        e.world_w32(MAIN_MENU, menu)
        e.world_w32(PILOT_STATE, 0x5e5e000)
        e.call(test_pilot.LIST_INIT)
        e.w32(CURRENT, 0)
        for i in range(24):
            e.w32(SETUP + 4 * i, 0)
        self.state = e.world_alloc(0x18)
        self.m = 0

    def make_dfn(self, mech, flags):
        e = self.e
        if flags == ['-']:
            return
        cfgs = []
        for i, f in enumerate(flags):
            c = e.alloc(0xd8)
            e.call(CONFIG_NEW, ecx=c, stack=(mech, e.world_string(f'L{mech}_{i}')))
            e.cpu.write(c + 0x1e, bytes([int(f)]))
            cfgs.append(c)
        arr = e.alloc(4 * max(len(cfgs), 1))
        for i, c in enumerate(cfgs):
            e.w32(arr + 4 * i, c)
        lst = e.alloc(0x18)
        e.w32(lst, len(cfgs))
        e.w32(lst + 4, mech)
        e.w32(lst + 0xc, arr)
        e.w32(lst + 0x10, arr + 4 * len(cfgs))
        e.w32(lst + 0x14, arr + 4 * len(cfgs))
        d = e.alloc(0x1a8)
        e.w32(d + 0x20, lst)
        self.dfns[mech] = d

    def vcall(self, obj, slot, stack=()):
        return self.e.call(self.e.r32(self.e.r32(obj) + slot), ecx=obj, stack=stack)

    def pilot(self):
        return self.e.r32(CURRENT)

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
                pl = e.call(test_pilot.FIND, ecx=e.world_string(unhx(p[1])))
                e.w32(CURRENT, pl)
                if pl:
                    e.call(SELECT_GAME, ecx=pl, stack=(2,))
            elif op == 'V':
                pl = self.pilot()
                if pl:
                    e.w32(e.r32(pl + 0x70) + pv[0], pv[1])
            elif op == 'K':
                e.w32(SETUP + 4 * pv[0], pv[1])
            elif op == 'D':
                self.make_dfn(pv[0], p[2:])
            elif op == 'S':
                self.snapshot = e.snapshot()
                e.call(STATE_NEW, ecx=S)
                self.m = self.d = e.r32(S + 0xc)
            elif op == 'Y':
                e.call(STATE_ENTER, ecx=S)
            elif op == 'R':
                if not (pv[0] == 2 and not self.pilot()):
                    e.call(STATE_RESUME, ecx=S, stack=(pv[0],))
            elif op == 'flags':
                e.cpu.write(APP_IA_ARG, bytes([pv[0]]))
            elif op == 'E' and not m:
                dump += self.dump_setup() + self.dump_game()
            elif not m:
                continue
            elif op == 'A':
                self.vcall(m, 0x04, (pv[0],))
            elif op == 'F':
                w.mouse_frames.append(pv[1:])
                self.vcall(m, 0x00, (pv[0],))
                w.frame += 1
            elif op == 'I':
                e.call(RESET, ecx=m)
            elif op == 'B':
                self.vcall(m, 0x14, (pv[0], 1, 0))
            elif op in ('P', 'Q', 'T'):
                part = m + PARTS[pv[0]]
                if op == 'P':
                    self.vcall(part, 0x30)
                elif pv[0] < 21:
                    self.vcall(part, 0xa0 if op == 'Q' else 0xa4, (pv[1],))
            elif op == 'W':
                e.call(SHOW_WAVE, ecx=m, stack=(pv[0],))
            elif op == 'M':
                e.call(SHOW_CUSTOM, ecx=m)
            elif op == 'E':
                dump += test_twod.Original.dump(self) + self.dump_screen() + self.dump_setup() + self.dump_game()
        return dump

    # The dumps probe_actionscreen.cpp writes, from the emulator's memory.
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

        def control(o):
            ref('next', e.r32(o + 4))
            kv('flags', e.r32(o + 0xc))
            kv('timer', e.r32(o + 0x10))
            kv('x', e.r32(o + 0x14))
            kv('y', e.r32(o + 0x18))
            tx('bg', e.r32(o + 0x1c))
            rc('bgr', o + 0x20)
            kv('mouse', e.r32(o + 0x30) & 0xffff)
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

        def cycle(c, lineup):
            control(c)
            kv('index', e.r32(c + 0x16c))
            kv('click', e.r8(c + 0x168))
            kv('pen', e.r32(c + 0x188 + 0xc4))
            kv('nen', e.r32(c + 0x2d4 + 0xc4))
            kv('f', e.r32(c + 0x424))
            kv('fi', e.r32(c + 0x428))
            if lineup:
                kv('ox', e.r32(c + 0x42c))
                kv('oy', e.r32(c + 0x430))
            first, last = e.r32(c + 0x15c), e.r32(c + 0x160)
            for it in [e.r32(first + 4 * i) for i in range((last - first) // 4)] if first else []:
                s('item', e.r32(it))

        out.append(f'screen fixed={e.r8(m + FIXED)} wave={i32(e.r32(m + WAVE))} filling={i32(e.r32(m + FILLING))}')
        for name, off, lu in (('worlds', WORLDS, 0), ('scenarios', SCENARIOS, 0), ('desc', DESC, 0),
                              ('etitle', ETITLE, 0)):
            out.append(f'\n{name} ')
            cycle(m + off, lu)
        for i in range(5):
            out.append(f'\nally{i} ')
            cycle(m + ALLIES[i], 1)
        for i in range(6):
            out.append(f'\nenemy{i} ')
            cycle(m + ENEMIES[i], 1)
            out.append(f'\nconfig{i} ')
            cycle(m + CONFIGS[i], 1)
        out.append('\nwaves ')
        cycle(m + WAVES, 1)
        out.append('\n')
        return ''.join(out)

    def dump_setup(self):
        e = self.e
        vals = [e.r32(SETUP + 4 * i) for i in range(24) if i not in (5, 12, 13, 20)]
        return 'setup ' + ' '.join(f'{v:x}' for v in vals) + '\n'

    def dump_game(self):
        e = self.e
        pl = self.pilot()
        if not pl:
            return 'game -\n'
        g = e.r32(pl + 0x70)
        cur = e.r32(pl + 0x6c)

        def text(a):
            return e.cstr(a) if a else '-'
        out = (f'game mode={e.r32(g + 0x44):x} fb={e.r8(g + 0x48)} x6c={e.r32(g + 0x6c):x} x70={e.r32(g + 0x70):x} '
               f'x74={e.r32(g + 0x74):x} x78={e.r32(g + 0x78):x} count={e.r32(g + 0x7c):x} '
               f'slot={e.r32(g + 0x24):x} cur={(VT_GAME.get(e.r32(cur), -9) if cur else -1) & 0xffffffff:x} '
               f't38={text(e.r32(g + 0x38))} t3c={text(e.r32(g + 0x3c))} t40={text(e.r32(g + 0x40))}')
        for i in range(10):
            sl = g + 0x80 + 0x18 * i
            c = e.r32(sl + 8)
            name = e.cstr(e.r32(c + 0x18)) if c and e.r32(c + 0x18) else '-'
            out += f' [{i32(e.r32(sl + 4))},{e.r8(g + 0x170 + i)},{name},{i32(e.r32(sl + 0x10))}]'
        return out + '\n'


def run_case(seed, path, data, start, cfg, spots, verbose):
    fs = FS()
    orig = Original(seed, data, start, cfg, fs)
    plan = make_plan(seed, spots)
    dump_o = orig.run(plan)
    rep = Replay(orig.t, orig.e, orig.snapshot)
    probe.probe_app_set_callback(ctypes.cast(rep.callback(), ctypes.c_void_p))
    d = test_pilot.DIR.parent / 'actionscreen' / str(seed)
    shutil.rmtree(d, ignore_errors=True)
    d.mkdir(parents=True)
    buf = ctypes.create_string_buffer(1 << 24)
    n = probe.probe_actionscreen_run(str(path).encode(), str(d).encode(), orig.state, '\n'.join(plan).encode(), buf,
                                     len(buf))
    assert n >= 0, 'probe failed'
    dump_r = buf.value.decode('latin1')
    stubbed = orig.w.stub_textures
    to, do = canonical(orig.t.log, dump_o, stubbed)
    tr, dr = canonical(rep.log, dump_r, stubbed)
    do, dr = (re.sub(r'face=".*?" height=', 'height=', x, flags=re.S) for x in (do, dr))
    ok = compare(f'actionscreen seed {seed}', to, tr)
    if do != dr:
        ok = False
        for a, b in zip(do.split('\n'), dr.split('\n')):
            if a != b:
                print(f'actionscreen seed {seed}: dumps differ')
                ta, tb = a.split(' '), b.split(' ')
                print('  original:', ' '.join(x for x in ta if x not in tb)[:400])
                print('  rebuild: ', ' '.join(x for x in tb if x not in ta)[:400])
                print('  in:', a[:200])
                break
        else:
            print(f'actionscreen seed {seed}: dumps differ in length')
    fs_o, fs_p = fs.snapshot(), folder_files(d)
    if [(k.lower(), v) for k, v in fs_o] != [(k.lower(), v) for k, v in fs_p]:
        ok = False
        print(f'actionscreen seed {seed}: files differ: {[k for k, _ in fs_o]} / {[k for k, _ in fs_p]}')
    if ok:
        shutil.rmtree(d, ignore_errors=True)
    if verbose and ok:
        print(f'actionscreen seed {seed}: {len(to)} calls, {len(fs_o)} files')
    return ok, to, fs_o, dump_o


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    runs = int(args[0]) if args else 24
    first = int(args[1]) if len(args) > 1 else 0
    verbose = '--verbose' in sys.argv
    data = READER.read_bytes()
    fname = 'dialog.zrd'
    start = next(s for n, s, _ in entries(data) if n == fname)
    tree, _ = parse_node(data, start)
    synth_path = ROOT / 'build' / 'actionscreen_synth.zbd'
    passed = calls = saves = 0
    stats, modes, fixed = {}, {}, 0
    for seed in range(first, first + runs):
        if verbose:
            print('seed', seed, flush=True)
        r = random.Random(f'cfg:{seed}')
        cfg = {'fast': r.choice([0, 0, 1, 'random']), 'size': r.choice(SIZES), 'moving': r.choice([0, 1, 1]),
               'exists': r.choice([1.0, 1.0, 0.7, 0.3, 0.0])}
        if seed % 2:
            t = synthetic(seed, tree)
            blob = write_node(t)
            synth_path.write_bytes(archive([(fname, blob)]))
            path, file_data, file_start, here = synth_path, blob, 0, t
        else:
            path, file_data, file_start, here = READER, data, start, tree
        body = find(here, 'ACTIONDIALOG') or []
        spots = spots_of(body) or [(250, 150)]
        ok, trace, files, dump = run_case(seed, path, file_data, file_start, cfg, spots, verbose)
        for mode in re.findall(r'^game mode=(\w+) ', dump, flags=re.M)[-1:]:
            modes[mode] = modes.get(mode, 0) + 1
        fixed += 'screen fixed=0' in dump
        passed += ok
        calls += len(trace)
        saves += bool(files)
        for k, _ in trace:
            if k in ('0x005905a0', '0x00590250', '0x00575fd0', '0x004846a0', '0x00576080', '0x00488ad0',
                     '0x00590400', '0x00470e30'):
                stats[k] = stats.get(k, 0) + 1
    print(f'actionscreen: {passed}/{runs} runs identical ({calls} calls, {saves} runs saved a pilot, the last '
          f'ACCEPT by mode {dict(sorted(modes.items()))}, {fixed} runs showed a wave; ' +
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
