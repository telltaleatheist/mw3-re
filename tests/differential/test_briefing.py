"""The briefing screen's shell (docs/campaign.md section 2) against the original x86.

    cmake --build build && .venv/bin/python -P tests/differential/test_briefing.py [runs] [first] [--verbose]

The original builds the screen 0x00405230 for a briefing's section of
briefing.zrd (or a synthetic file, odd runs: buttons moved or missing,
sections missing, an mpinfo.zrd) with the current game a fake pilot's
(+0x6c) whose kind, operation and mission, multiplayer +0x94 and campaign
salvage (+0x3c) come from the run's setup. Then a plan: frames 0x00406440
(the mouse over the buttons, keys L, M, Space and S answered by a script,
the load's end answered by a script), presses 0x00404c30 of every id, the
buttons' left-up, enter and leave, set_loaded 0x00408ef0, ABORT 0x00405050
and LAUNCH 0x00404f70 directly, the load's fraction 0x004050d0 and lines
0x00405120, the voice and step globals set; dumps; the destructor. The port
(tests/probe_briefing.cpp) runs the same plan on the same tree. Compared:
every call out (the mission's load, launch and cleanup, the states, the
message box, the voice stops, keys, the hooks of later items) and a dump of
the dialog, the buttons' blinking and help, the bar and the screen's fields.
The x87 runs at 24 bits.

Left to later items, skipped in the original here (a code hook jumps over
them): the SOUND key's script and map (0x0040583d..0x00405bc8) and the
REALLOC_LIST bind (0x00405c1e..0x00405c3d). Stubbed: 0x00405160 (the map's
image), 0x00404ae0 (the reallocation list), 0x00408c30 (the lancemates'
loadouts), 0x00408b40 and 0x00448830 (the mech lab).
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
from unicorn.x86_const import UC_X86_REG_EIP, UC_X86_REG_ESP  # noqa: E402
import test_twod  # noqa: E402
from test_twod import canonical, fbits  # noqa: E402
from test_twod_msgbox import archive  # noqa: E402
from test_menus import MECH3, SIZES, mutate_buttons  # noqa: E402

probe = load_probe()
probe.probe_app_set_callback.argtypes = [ctypes.c_void_p]
probe.probe_briefing_run.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p,
                                     ctypes.c_int]

SIZE = 0x12e6c
CTOR, FRAME, SET_LOADED, ABORT, LAUNCH, PROGRESS, LOG = 0x405230, 0x406440, 0x408ef0, 0x405050, 0x404f70, \
    0x4050d0, 0x405120
BUTTONS = [0x11884, 0x11a00, 0x11b7c, 0x11cf8, 0x11e74, 0x11ff0]  # ABORT LAUNCH MECHLAB REPLAY SALVAGE LOADING
NAMES = ['ABORT', 'LAUNCH', 'MECHLAB', 'REPLAY', 'SALVAGE', 'LOADING']
REALLOC_OK = 0x12638
PARTS = BUTTONS + [REALLOC_OK]
BAR, LINES, VERSION = 0x11234, 0x113bc, 0x1216c
SEEN, TARGET, LOADED, SCRIPT = 0x11214, 0x11218, 0x1121c, 0x11220
CUE = 0x405ed0
STEP_KINDS = {0x5bbc50: 'V', 0x5bbc40: 'M', 0x5bbc30: 'S'}
REACHED, VOICE, SCREEN, CURRENT, FRAME_DT = 0x5e043c, 0x5f4c84, 0x5f4c80, 0x60861c, 0x72c52c
SKIPS = {}
KEYS = (0x26, 0x32, 0x39, 0x1f)
SECTIONS = ['brief_zz', 'brief_zz', 'brief_c11', 'brief_c12', 'brief_c14', 'brief_c24', 'brief_c36', 'brief_c4b6', 'brief_t13', 'brief_iac21',
            'brief_mpc11', 'default', 'brief_none']
LINES_TEXT = ['Loading world', 'Ready To Launch', '', 'Textures 12/40', 'a much longer line of the load ' * 3]

test_twod.VT_KIND.update({0x5bbd20: 'C', 0x5bc230: 'C', 0x5bbc90: 'L', 0x5bc1b0: 'I', 0x5bc030: 'F', 0x5bbf30: 'I',
                          0x5bbfb0: 'B', 0x5bbeb0: 'B', 0x5bbe30: 'B', 0x5bbdb0: 'B',
                          0x5c5ec0: 'C', 0x5c5e30: 'C', 0x5c5da0: 'C'})  # REALLOC_LIST, its arrows
MAP_KINDS = {0x5bbfb0: 'circle', 0x5bbeb0: 'waypoint', 0x5bc030: 'text', 0x5bbf30: 'bitmap', 0x5bbe30: 'triangle',
             0x5bbdb0: 'line'}
NAVS, WORLD_UP = 0x609980, 0x60bf48


def i32(v):
    return struct.unpack('<i', struct.pack('<I', v & 0xffffffff))[0]


def hx(s):
    return s.encode('latin1').hex() or '-'


# ---- the files ----

def random_object(r):
    kind = r.choice(['circle', 'text', 'waypoint', 'waypoint', 'bitmap', 'line', 'line', 'triangle', 'triangle', 'triangle',
                     'game_type', 'db_name',
                     'scenario_name', 'scenario_desc', 'label', 'nonsense'])
    at = ['at', [r.randrange(-20, 660), r.randrange(-20, 500)]]
    opt = []
    if r.random() < 0.5:
        opt += ['color', [r.randrange(256), r.randrange(256), r.randrange(256)]]
    if r.random() < 0.3:
        opt += ['font', [r.choice(['brief_grey', 'lab_small_white', 'no_font'])]]
    if r.random() < 0.4:
        opt += ['sound', [r.choice(['sndCircle', 'no_sound'])]]
    if kind in ('circle', 'waypoint'):
        if r.random() < 0.7:
            opt += ['radius', [r.randrange(0, 40), r.randrange(-2, 12)]]
    if kind == 'waypoint':
        opt += ['label', [r.choice(['Able', 'Baker', 'Charlie', 'Dog', 'Easy', 'Easy', 'Fox', 'Nowhere', 'Nav7'])]]
    elif kind == 'text':
        opt += ['string', [r.choice(['MSG_NAV', 'Hello'])]]
    elif kind == 'bitmap':
        opt += ['image', [r.choice(['map_pic', 'nav_icon'])]]
    elif kind == 'triangle':
        opt += ['height', [r.randrange(1, 30), r.randrange(0, 9)]]
    elif kind == 'line':
        pts = [[r.randrange(0, 640), r.randrange(0, 480)] for _ in range(r.randrange(0, 6))]
        if r.random() < 0.3:
            pts.insert(r.randrange(len(pts) + 1), [r.randrange(9)])  # one number: not a point, skipped
        opt += ['points', pts]
    body = (at if r.random() < 0.9 else []) + opt
    return [kind, body]


def synthetic(seed, tree):
    """briefing.zrd with BRIEFINGDIALOG's buttons moved or missing, sections
    missing, and an mpinfo.zrd with a briefing section (or none)."""
    r = random.Random(f'briefing:{seed}')
    root = tree[0]
    out = []
    for i in range(0, len(root) - 1, 2):
        k, v = root[i], root[i + 1]
        if k == 'BRIEFINGDIALOG':
            if r.random() < 0.08:
                continue
            body = []
            for j in range(0, len(v) - 1, 2):
                if v[j] == 'BUTTONS':
                    body += ['BUTTONS', mutate_buttons(r, v[j + 1], None)]
                else:
                    body += [v[j], v[j + 1]]
            v = body
        elif k.startswith('brief_') and r.random() < 0.3:
            continue
        elif isinstance(v, list) and 'SOUND' in v and r.random() < 0.6:
            v = [x for x in v]
            si = v.index('SOUND') + 1
            snd = [x for x in v[si]]
            roll = r.random()
            if k in SECTIONS and roll < 0.6:
                roll = 0.6 + roll / 2  # the sections the plans open: mostly new objects
            if roll < 0.2:
                snd[0] = r.choice(['no_such_voice', 'briefing_c2m3'])
            elif roll < 0.5 and 'MARKERS' in snd:
                mi = snd.index('MARKERS') + 1
                marks = list(snd[mi])
                snd[mi] = marks[:r.randrange(0, len(marks) + 1)] + marks[:r.randrange(0, 3)]
            elif 'MARKERS' in snd:  # objects of every kind
                mi = snd.index('MARKERS') + 1
                marks = [list(x) for x in snd[mi]]
                for _ in range(r.randrange(2, 9)):
                    if not marks:
                        marks.append([])
                    marks[r.randrange(len(marks))] += random_object(r)
                snd[mi] = marks
            elif roll < 0.6:
                del v[si - 1:si + 1]  # no SOUND
                snd = None
            if snd is not None:
                v[si] = snd
        elif k == 'default' and r.random() < 0.15:
            continue
        elif isinstance(v, list) and 'BUTTONS' in v and r.random() < 0.2:
            v = [x for x in v]
            b = v.index('BUTTONS')
            del v[b:b + 2]  # no TRANSPORT_PROGRESS
        out += [k, v]
    # A briefing of map objects of every kind (the plans open it often).
    dflt = find(tree, 'default') or []
    marks = []
    for _ in range(r.randrange(3, 6)):
        step = []
        for _ in range(r.randrange(3, 9)):
            step += random_object(r)
        marks.append(step)
    marks[0] += ['line', ['points', [[10, 20], [3], [30, 40], [50, 60]]],  # a point skipped in the middle
                 'waypoint', ['label', ['Easy'], 'radius', [12, 4]],  # a label with no message
                 'circle', ['at', [330, 250], 'radius', [60, 50]],  # under the mouse's spot (320, 240)
                 'text', ['string', ['MSG_NAV']]]  # no AT: no text
    out += ['brief_zz', [x for x in dflt] + ['SOUND', [r.choice(['briefing_c1m1', 'no_voice']), 'MARKERS', marks]]]
    files = [('briefing.zrd', write_node([out]))]
    if r.random() < 0.7:
        dflt = find(tree, 'default') or []
        name = 'briefing' if r.random() < 0.8 else 'other'
        files.append(('mpinfo.zrd', write_node([[name, dflt]])))
    return files


def spots_of(tree):
    out = [(320, 240)]
    dlg = find(tree, 'BRIEFINGDIALOG') or []
    bl = find(dlg, 'BUTTONS') or []
    for i in range(0, len(bl) - 1, 2):
        p = find(bl[i + 1], 'POSITION')
        if p:
            out.append(tuple(p))
    return out


# ---- the plan ----

def mouse_line(r, spots, state):
    sx, sy = r.choice(spots)
    if r.random() < 0.85:
        x, y = sx + r.randrange(-4, 50), sy + r.randrange(-4, 40)
    else:
        x, y = r.randrange(-20, 660), r.randrange(-20, 500)
    if state['down'] == 0:
        left = 1 if r.random() < 0.15 else 0
    elif state['down'] in (1, 2):
        left = 2 if r.random() < 0.6 else 4
    else:
        left = 0
    state['down'] = left
    dt = r.choice([0.0, 1 / 60, 1 / 30, 0.03, 0.05, 0.2])
    m = [x & 0xffffffff, y & 0xffffffff, 0, fbits(0.0), fbits(0.0), fbits(0.0), 0, 0, 0,
         fbits(0.0), fbits(0.0), fbits(0.0), left, r.choice([0, 0, 0, 1, 4]), 0]
    return 'F ' + ' '.join(f'{v:x}' for v in [fbits(dt)] + m)


def make_plan(seed, spots):
    r = random.Random(f'plan:{seed}')
    lines = []
    if r.random() < 0.3:
        lines.append(f'V {r.choice([0x77700001, 0x77700002]):x}')
    state = {'down': 0}
    lines.append(f'S {hx("brief_zz" if seed % 2 and r.random() < 0.5 else r.choice(SECTIONS))}')
    if r.random() < 0.7:
        lines.append('Y')
        if r.random() < 0.6:  # the voice, then its markers one cue at a time
            for c in range(r.randrange(1, 5)):
                lines += [mouse_line(r, spots, state), f'C {c:x}', mouse_line(r, spots, state), 'E']
            state['cue'] = c
            if r.random() < 0.6:  # every step at once
                lines += [f'C {999:x}', mouse_line(r, spots, state), mouse_line(r, spots, state), 'E']
    for _ in range(r.randrange(30, 110)):
        roll = r.random()
        if roll < 0.08:
            lines.append(f'B {r.choice([0, 1, 2, 3, 4, 5, 6, 7, 8]):x}')
        elif roll < 0.14:
            lines.append(f'P {r.randrange(len(PARTS)):x}')
        elif roll < 0.22:
            lines.append(f'{r.choice("NO")} {r.randrange(len(BUTTONS)):x}')
        elif roll < 0.26:
            lines.append(f'L {r.choice([0, 1, 1]):x}')
        elif roll < 0.28:
            lines.append(r.choice(['A', 'Q', 'X']))
        elif roll < 0.33:
            lines.append(f'G {fbits(r.choice([0.0, 0.1, 0.33, 0.5, 0.9, 1.0, 1.5])):x}')
        elif roll < 0.36:
            lines.append(f'T {hx(r.choice(LINES_TEXT))}')
        elif roll < 0.38:
            lines.append(f'V {r.choice([0, 0x77700003]):x}')
        elif roll < 0.40:
            lines.append(f'R {r.choice([-1, 0, 2, 5]) & 0xffffffff:x}')
        elif roll < 0.42:
            lines.append('E')
        elif roll < 0.48:
            state['cue'] = min(state.get('cue', -1) + r.choice([0, 1, 1, 2]), 12) if r.random() < 0.8 else \
                r.choice([-1, 999])
            lines.append(f'C {state["cue"] & 0xffffffff:x}')
            if r.random() < 0.5:  # the step it reaches, a frame on
                lines += [mouse_line(r, spots, state), 'E']
        elif roll < 0.50:
            lines.append('Y')
        lines.append(mouse_line(r, spots, state))
    lines += ['E', 'U']
    return lines


# ---- the original ----

class Original:
    def __init__(self, seed, files, cfg):
        self.t = Trace(seed=seed)
        w = self.w = TwodWorld(self.t, seed)
        e = self.e = w.emu
        t = self.t
        self.trees = {}
        for name, blob in files:
            self.trees[name] = w.read_tree(blob, 0)
        e.stub(0x556740, '@zrdr_load', 'c,d', None,
               lambda emu, key, a, pop: emu._ret(self.trees.pop(emu.cstr(a[0]).lower(), 0), pop))
        e.stub(0x556820, '@zrdr_free', 'c')
        for addr, spec in MECH3.items():
            if addr not in (0x4ac150, 0x482970, 0x590250):
                e.stub(addr, f'{addr:#010x}', spec)
        e.stub(0x4ac150, '0x004ac150', 's0,s1,s2', 0)

        # The current game (memory the screen reads; silent on both sides).
        pilot, game, vt = e.world_alloc(0x84), e.world_alloc(0xd0), e.world_alloc(0x60)
        e.w32(game, vt)
        e.w32(pilot + 0x6c, game)
        e.w32(CURRENT, pilot)
        self.game = game
        for off in (0x08, 0x3c):  # the stores (empty; the salvage's count alone set)
            e.call(0x470ce0, ecx=game + off, stack=(0,))
        for i in range(cfg['salvage']):  # that many mechs of different kinds
            e.call(0x471c10, ecx=game + 0x3c, stack=(i,))
        e.w32(game + 0xa8, 3)
        k4c = cfg['mission'] if cfg['kind'] == 1 else cfg['mp']
        # vt+0x3c: no slots (0x00408c30's slots are test_salvage.py's)
        for off, v in ((0x2c, cfg['kind']), (0x48, cfg['op']), (0x4c, k4c), (0x3c, 0)):
            e.w32(vt + off, e.new_stub(f'@vt{off:x}', '', 0, lambda emu, k, a, p, v=v: emu._ret(v, 0)))
        # The game's texts (multiplayer's vt+0x04 .. +0x10): a string or none.
        for off in (4, 8, 0xc, 0x10):
            def gtext(emu, k, a, p, off=off):
                emu._ret(e.world_string(f'G{off:x}') if t.handle('game.vt', (off,)) else 0, 0)
            e.w32(vt + off, e.new_stub(f'@game{off:x}', '', 0, gtext))
        # The mission's nav points, its map and the projection onto it.
        self.map = e.world_alloc(0x40)
        e.world_w32(self.map + 0x3c, w.fake_texture(400, 300))
        self.pos = e.world_alloc(16)
        self.navs = []
        for i in range(3):
            nv, nvt = e.world_alloc(8), e.world_alloc(0x20)
            e.w32(nv, nvt)
            e.w32(nvt + 0x18, e.new_stub('nav.vt+0x18', 'c', 0))
            self.navs.append(nv)
        e.w32(NAVS, cfg['navs'])
        e.stub(0x479be0, '0x00479be0', 'c,zs0')

        def project(emu, k, a, pop):
            v = t.handle(k, (a[0], a[1]))
            emu.w32(a[2], v & 0x3ff)
            emu.w32(a[2] + 4, (v >> 10) & 0x3ff)
            emu._ret((v >> 20) & 1, pop)
        e.stub(0x4586e0, '0x004586e0', 'c,s0,s1', None, project)
        # Mech3Msg.dll's messages: "msg<id>" into the buffer, or none; a
        # waypoint's (0x200) argument, its name, logged.
        def msg(emu, key, a, pop):
            buf, size, mid, va = a
            got = t.handle(key, (size, mid))
            if mid == 0x200:
                t.handle(key + '.s', (emu.cstr(emu.r32(va)),))
            if got:
                emu.cpu.write(buf, f'msg{mid}'.encode()[:size - 1] + b'\0')
            emu._ret(len(f'msg{mid}') if got else 0, pop)
        e.stub(0x575ff0, '0x00575ff0', 's0,s1,s2,s3', None, msg)
        # zRender's 2D circle and line (the map's circles and lines).
        def circle(emu, key, a, pop):
            c = a[-1]
            t.handle(key, tuple(a[:-1]) + (tuple(i32(emu.r32(c + 4 * i)) & 0xffffffff for i in range(4)) if c else ()))
            emu._ret(0, pop)
        e.stub(0x5630c0, '0x005630c0', 'c,d,s0,s1,s2', None, circle)
        e.stub(0x562940, '0x00562940', 'c,d,s0,s1,s2')
        screen = e.world_alloc(16)
        for i, v in enumerate((0, 0, 640, 480)):
            e.world_w32(screen + 4 * i, v)
        w.answers['0x00579880'] = screen
        e.stub(0x579880, '0x00579880', '')
        e.stub(0x4af290, '@version', '', None, lambda emu, k, a, p: emu._ret(e.world_string('1.2'), p))
        e.wf(FRAME_DT, cfg['dt'])

        def replace(emu, key, a, pop):
            t.handle(key, (a[0] & 0xff,))
            emu._ret(0, pop)
        e.stub(0x482970, '@state', 's0', None, lambda emu, k, a, p: emu._ret(0x77000000 | a[0], p))
        e.stub(0x590250, '0x00590250', 's0,s1', None, replace)
        e.stub(0x405160, '0x00405160', 's0', None,
               lambda emu, k, a, p: emu._ret(t.handle(k, (1 if a[0] else 0,)), p))
        plain = {0x40a810: '', 0x40ce90: 'c', 0x448830: 'd,s0',
                 0x47d2f0: '', 0x46ceb0: '', 0x490930: '', 0x491150: '', 0x491780: 's0', 0x434cb0: '',
                 0x478f10: 's0', 0x56dfb0: 'c,d', 0x576ea0: 'c,d,s0,s1', 0x51a9b0: 'c', 0x51a300: '',
                 0x56e8e0: 'c', 0x56f3e0: 'c', 0x491c80: '', 0x4f8f50: ''}
        for addr, spec in plain.items():
            e.stub(addr, f'{addr:#010x}', spec)
        w.answers.update({
            '0x0040cec0': cfg['fast'],
            '0x0040d760': cfg['size'][0], '0x0040d770': cfg['size'][1],
            '0x00576040': lambda rr, k, a: e.world_string(f'M{a[0]:x}'),
            '0x0040a810': cfg['lines'],
            '0x004ac150': lambda rr, k, a: rr.choice([1, 1, 2, 6]),
            '0x0051a9b0': lambda rr, k, a: rr.choice([1, 2, 3]) if rr.random() < cfg['keys'] else 0,
            '0x00490930': lambda rr, k, a: 1 if rr.random() < cfg['load'] else 0,
            '0x00491150': lambda rr, k, a: 1 if rr.random() < 0.7 else 0,
            '0x00491c80': lambda rr, k, a: self.map,  # (made when missing: never none)
            '0x004f8f50': lambda rr, k, a: rr.choice([0, 1, 1]),
            '0x00479be0': lambda rr, k, a: rr.choice(self.navs + [0]),
            'nav.vt+0x18': lambda rr, k, a: self.pos,
            '0x004586e0': lambda rr, k, a: rr.randrange(1 << 21),
            'game.vt': lambda rr, k, a: rr.choice([1, 1, 0]),
            '0x00575ff0': lambda rr, k, a: rr.choice([1, 1, 0]),
            '0x0056e8e0': lambda rr, k, a: rr.choice([0, 1, 1, 0xffffffff]),
        })

        def skip(uc, address, size, user):
            uc.reg_write(UC_X86_REG_EIP, SKIPS[address])
        for a in SKIPS:
            e.uc.hook_add(UC_HOOK_CODE, skip, begin=a, end=a)

        # FIX (bug), as the port's FileList: an empty list's rectangle ends at its
        # top (test_loadsave.py); the unbound REALLOC_LIST lays out so as it goes.
        def empty_rect(uc, address, size, user):
            esp = uc.reg_read(UC_X86_REG_ESP)
            if i32(e.r32(esp + 0x10)) <= 0:
                e.w32(esp + 0x30, e.r32(esp + 0x1c))
        e.uc.hook_add(UC_HOOK_CODE, empty_rect, begin=0x48d06d, end=0x48d06d)

        def copy_rect(emu, key, a, pop):
            emu.cpu.write(a[0], emu.cpu.read(a[1], 16))
            emu._ret(1, 8)
        e.import_stub(IAT['CopyRect'], '@CopyRect', 's0,s1', 8, copy_rect)
        self.m = 0
        self.screen_addr = 0
        self.snapshot = None

    def vcall(self, obj, slot, stack=()):
        return self.e.call(self.e.r32(self.e.r32(obj) + slot), ecx=obj, stack=stack)

    def run(self, plan):
        e, w = self.e, self.w
        dump = ''
        for line in plan:
            p = line.split()
            op = p[0]
            pv = [int(x, 16) for x in p[1:]] if op not in ('S', 'T') else []
            m = self.m
            if op == 'V':
                e.w32(VOICE, pv[0])
            elif op == 'R':
                e.w32(REACHED, pv[0])
            elif op == 'C':
                e.call(CUE, ecx=pv[0])
            elif op == 'S':
                self.snapshot = e.snapshot()
                self.m = self.d = m = e.alloc(SIZE)
                self.screen_addr = self.screen_addr or m
                name = bytes.fromhex(p[1]).decode('latin1') if p[1] != '-' else ''
                e.call(CTOR, ecx=m, stack=(e.world_string(name),))
                e.w32(SCREEN, m)
            elif not m:
                continue
            elif op == 'F':
                w.mouse_frames.append(pv[1:])
                self.vcall(m, 0x00, (pv[0],))
                w.frame += 1
            elif op == 'B':
                self.vcall(m, 0x14, (pv[0], 0, 0))
            elif op == 'P':
                self.vcall(m + PARTS[pv[0]], 0x30)
            elif op == 'N':
                self.vcall(m + BUTTONS[pv[0]], 0x3c)
            elif op == 'O':
                self.vcall(m + BUTTONS[pv[0]], 0x40)
            elif op == 'L':
                e.call(SET_LOADED, ecx=m, stack=(pv[0],))
            elif op == 'X':  # back from the mech lab (the Load state): reallocated, the list shown
                e.call(0x408bd0, ecx=m)
                e.call(0x404ae0, ecx=m, stack=(1,))
            elif op == 'Y':  # the Load state's start of the script
                e.w8(m + SCRIPT + 0x10, 1)
                e.w32(m + SCRIPT + 0xc, e.r32(e.r32(m + SCRIPT + 4)))
            elif op == 'A':
                e.call(ABORT, ecx=m)
            elif op == 'Q':
                e.call(LAUNCH, ecx=m)
            elif op == 'G':
                e.call(PROGRESS, stack=(pv[0],))
            elif op == 'T':
                text = bytes.fromhex(p[1]).decode('latin1') if p[1] != '-' else ''
                e.call(LOG, ecx=e.world_string(text))
            elif op == 'E':
                dump += test_twod.Original.dump(self) + self.dump_screen()
            elif op == 'U':
                self.vcall(m, 0x08, (1,))
                e.w32(SCREEN, 0)
                self.m = 0
                dump += f'gone reached={i32(e.r32(REACHED))} voice={e.r32(VOICE):x}\n'
        return dump

    # The dump probe_briefing.cpp writes, from the emulator's memory.
    def dump_screen(self):
        e, m = self.e, self.m
        objs = []
        p = e.r32(m + 8)
        while p:
            objs.append(p)
            p = e.r32(p + 4)
        head = e.r32(m + SCRIPT + 4)
        at = e.r32(m + SCRIPT + 0xc)
        out = [f'screen seen={e.r8(m + SEEN)} target={e.r32(m + TARGET):x} loaded={e.r8(m + LOADED)} '
               f'playing={e.r8(m + SCRIPT + 0x10)} at_end={1 if at in (head, 0) else 0} steps={e.r32(m + SCRIPT + 8)} '
               f'reached={i32(e.r32(REACHED))} voice={e.r32(VOICE):x} bar={e.r32(m + BAR + 0x14c):x}\n']
        for name, off in zip(NAMES, BUTTONS):
            b = m + off
            first, last = e.r32(b + 0x170), e.r32(b + 0x174)
            help_ = [e.r32(first + 4 * i) for i in range((last - first) // 4)] if first else []
            out.append(f'{name} id={i32(e.r32(b + 0x150))} en={e.r32(b + 0xc4)} sel={e.r8(b + 0x158)} '
                       f'blink={e.r8(b + 0x159)} period={e.r32(b + 0x15c):x} timer={e.r32(b + 0x160):x} '
                       f'glow={i32(e.r32(b + 0x164))},{i32(e.r32(b + 0x168))} help=' +
                       ''.join(f'o{objs.index(x) if x in objs else -1},' for x in help_) + '\n')
        nodes, n = [], e.r32(head)
        while n != head:
            nodes.append(n)
            n = e.r32(n)
        out.append(f'script at={nodes.index(at) if at in nodes else -1}')
        for nd in nodes:
            v = e.r32(nd + 8)
            k = STEP_KINDS.get(e.r32(v), '?')
            if k == 'V':
                out.append(f' V"{e.cpu.read(v + 4, 0x50).split(bytes(1))[0].decode("latin1")}",{e.r32(v + 0x54):x},'
                           f'{e.r8(v + 0x58)},{e.r32(v + 0x5c)}')
            elif k == 'M':
                out.append(f' M{i32(e.r32(v + 4))}')
            else:
                o = e.r32(v + 4)
                out.append(f' {k}o{objs.index(o) if o in objs else -1}')
        out.append('\n')
        lst = m + 0x12e60
        hd, nds, n = e.r32(lst + 4), [], None
        n = e.r32(hd)
        while n != hd:
            nds.append(e.r32(n + 8))
            n = e.r32(n)
        for o in nds:
            k = MAP_KINDS.get(e.r32(o), '?')
            line = [f'map o{objs.index(o) if o in objs else -1} {k}']
            if k in ('circle', 'waypoint'):
                lb = o + 0x54
                line.append(f'r={i32(e.r32(o + 0x34))} r2={i32(e.r32(o + 0x38))} col={e.r32(o + 0x3c):x} '
                            f'clip={e.r8(o + 0x40)},' + ','.join(str(i32(e.r32(o + 0x44 + 4 * i))) for i in range(4)) +
                            f' snd={e.r32(o + 0x504):x} end={i32(e.r32(o + 0x508))} '
                            f'label="{e.cstr(lb + 0x34).replace(" ", "_")}",{e.r32(lb + 0xc):x},'
                            f'{i32(e.r32(lb + 0x14))},{i32(e.r32(lb + 0x18))},'
                            + ','.join(str(i32(e.r32(lb + 0x20 + 4 * i))) for i in range(4)))
                if k == 'waypoint':
                    line.append(f'map={e.r32(o + 0x50c):x} nav={e.r32(o + 0x510):x}')
            elif k in ('triangle', 'line'):
                cnt = i32(e.r32(o + 0xdc))
                line.append(f'n={cnt} col={e.r32(o + 0xe0):x} pts=' +
                            ','.join(str(i32(e.r32(o + 0x34 + 4 * i))) for i in range(2 * max(0, min(cnt, 21)))))
                if k == 'triangle':
                    line.append(f'xf={e.r32(o + 0xec)} ' + ','.join(f'{e.r32(o + 0xf0 + 4 * i):x}' for i in range(13)) +
                                f' snd={e.r32(o + 0x124):x} end={e.r32(o + 0x128):x}')
                else:
                    line.append(f'xec={e.r32(o + 0xec):x}')
            out.append(' '.join(line) + '\n')
        ok = m + REALLOC_OK
        out.append(f'REALLOC_OK id={i32(e.r32(ok + 0x150))} en={e.r32(ok + 0xc4)}\n')
        return ''.join(out)


def run_case(seed, path, files, cfg, plan, verbose):
    orig = Original(seed, files, cfg)
    dump_o = orig.run(plan)
    rep = Replay(orig.t, orig.e, orig.snapshot)
    probe.probe_app_set_callback(ctypes.cast(rep.callback(), ctypes.c_void_p))
    c = (f'{cfg["kind"]} {cfg["op"]} {cfg["mission"]} {cfg["mp"]} {cfg["salvage"]} {fbits(cfg["dt"]):x} '
         f'{cfg["navs"]:x} {orig.screen_addr:x} {orig.game:x}')
    buf = ctypes.create_string_buffer(1 << 23)
    n = probe.probe_briefing_run(str(path).encode(), c.encode(), '\n'.join(plan).encode(), buf, len(buf))
    assert n >= 0, 'probe failed'
    dump_r = buf.value.decode('latin1')
    stubbed = orig.w.stub_textures
    to, do = canonical(orig.t.log, dump_o, stubbed)
    tr, dr = canonical(rep.log, dump_r, stubbed)
    do, dr = (re.sub(r'face=".*?" height=', 'height=', x, flags=re.S) for x in (do, dr))
    ok = compare(f'briefing seed {seed}', to, tr)
    if do != dr:
        ok = False
        for a, b in zip(do.split('\n'), dr.split('\n')):
            if a != b:
                print(f'briefing seed {seed}: dumps differ')
                ta, tb = a.split(' '), b.split(' ')
                print('  original:', ' '.join(x for x in ta if x not in tb)[:400])
                print('  rebuild: ', ' '.join(x for x in tb if x not in ta)[:400])
                print('  in:', a[:200])
                break
        else:
            print(f'briefing seed {seed}: dumps differ in length')
    if verbose and ok:
        print(f'briefing seed {seed}: {len(to)} calls')
    return ok, to


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    runs = int(args[0]) if args else 24
    first = int(args[1]) if len(args) > 1 else 0
    verbose = '--verbose' in sys.argv
    data = READER.read_bytes()
    start = next(s for n, s, _ in entries(data) if n == 'briefing.zrd')
    tree, end = parse_node(data, start)
    shipped = data[start:end]
    synth_path = ROOT / 'build' / 'briefing_synth.zbd'
    passed = calls = 0
    stats = {}
    for seed in range(first, first + runs):
        if verbose:
            print('seed', seed, flush=True)
        r = random.Random(f'cfg:{seed}')
        kind = r.choice([0, 1, 1, 1, 2, 3, 4])
        op, mission = r.choice([(1, 1), (1, 1), (1, 2), (2, 4), (4, 6)])
        cfg = {'navs': r.choice([0, 0x5e700000, 0x5e700000]), 'kind': kind, 'op': op, 'mission': mission, 'mp': r.choice([1, 1, 2, 3, 4]),
               'salvage': r.choice([0, 1, 3]), 'dt': r.choice([1 / 30, 1 / 60, 0.1]),
               'fast': r.choice([0, 0, 1]), 'size': r.choice(SIZES), 'lines': r.choice([0, 0, 1]),
               'keys': r.choice([0.0, 0.03, 0.1]), 'load': r.choice([0.02, 0.1, 0.5, 1.0])}
        if seed % 2:
            files = synthetic(seed, tree)
            synth_path.write_bytes(archive(files))
            path, here = synth_path, parse_node(files[0][1], 0)[0]
            if not any(n == 'mpinfo.zrd' for n, _ in files):
                cfg['mp'] = min(cfg['mp'], 2)  # (no mpinfo.zrd: the exe reads it unchecked)
        else:
            files = [('briefing.zrd', shipped)]
            path, here = READER, tree
            cfg['mp'] = min(cfg['mp'], 2)
        plan = make_plan(seed, spots_of(here))
        ok, trace = run_case(seed, path, files, cfg, plan, verbose)
        passed += ok
        calls += len(trace)
        for k, _ in trace:
            if k in ('0x00590250', '0x004ac150', '0x00491150', '0x00491780', '0x00478f10', '0x00434cb0',
                     '0x00448830', '0x0047d2f0', '0x0046ceb0', '0x00408c30', '0x0056e9f0', '0x0051a300',
                     '0x0056e3d0', '0x0056f3e0', '0x0056e8e0', '0x005630c0', '0x00562940', '0x004586e0',
                     '0x00479be0', 'game.vt', '0x00575ff0.s', 'nav.vt+0x18', '0x004f8f50'):
                stats[k] = stats.get(k, 0) + 1
    print(f'briefing: {passed}/{runs} runs identical ({calls} calls; ' +
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
