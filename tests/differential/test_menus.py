"""The credits and option screens against the original x86 (docs/menus.md, Testing).

    cmake --build build && .venv/bin/python tests/differential/test_menus.py [runs] [credits|options|gfx]

The original builds a screen (the credits 0x004289a0, the game options
0x004379f0, the graphics and audio options 0x00438f60) from its dialog
(dlgCredits.zrd's CREDITSDIALOG, dlgOptions.zrd's GAME_OPTION_DIALOG and
GFX_AUDIO_OPTIONDIALOG), then runs a plan: frames of mouse input over the
buttons and the cycles' arrows through the screen's update (the credits'
scroll 0x00429030, the window 0x005a2bb0), presses (+0x30 of every button,
cycle and slider: the options read and written, pushes, pops, the restart
question), the option screens' +0x10 (0 reads the options, 1 writes them),
the cycles' +0x90 / +0x94 / +0xa0 / +0xa4, the graphics screen's device
change 0x00439a90, busy, bake, dirty-all and restore; then the deleting
destructor. Even runs use the shipped dialog, odd runs a synthetic one
(buttons, arrows and labels missing or moved, RADIUS / MAXRADIUS, other
SCROLLING_TEXT, RECT and SCROLL_RATE; one in eight without the dialog).
FastMenu, the main menu's background (there or not, moving or still), a
running mission, the display size and every option's value vary by run.
Every call out is stubbed (twod_world.py plus the options, the app's queue,
zVideo's devices and memory, the settings write); the port
(tests/probe_menus.cpp) runs the same plan with the original's answers.
Compared: the call traces and a dump of the dialog, every object in its list
and the screen's own state (each cycle's choices and arrows, the credits'
lines, the entries the screens write). The x87 runs at 24 bits.
"""
import ctypes
import random
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import Replay, compare, load_probe, rnd, Trace  # noqa: E402
from original import ROOT  # noqa: E402
from twod_world import IAT, READER, TwodWorld, c_format, entries, find, parse_node, write_node  # noqa: E402
from unicorn.x86_const import UC_X86_REG_ESP  # noqa: E402
import test_twod  # noqa: E402
from test_twod import canonical, fbits  # noqa: E402
from test_twod_msgbox import archive  # noqa: E402

probe = load_probe()
probe.probe_app_set_callback.argtypes = [ctypes.c_void_p]
probe.probe_menus_run.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_uint32, ctypes.c_char_p,
                                  ctypes.c_char_p, ctypes.c_int]

ZRDR_LOAD = 0x556740
SLIDE_VTS = [0x5bf648, 0x5bf5a8, 0x5bef20, 0x5bee80, 0x5bf898, 0x5bf7f8, 0x5bd7e8]
CYCLE_VTS = [0x5bf500, 0x5bf458, 0x5bf3b0, 0x5bf308, 0x5bf260, 0x5bf1b8, 0x5bf110, 0x5bf068, 0x5befc0,
             0x5bfe78, 0x5bfdd0, 0x5bfd28, 0x5bfc80, 0x5bfbd8, 0x5bfb30, 0x5bfa88, 0x5bf9e0, 0x5bf938]
ALERT_VTS = {0x5bf3b0, 0x5bf308, 0x5bf260}
test_twod.VT_KIND.update({v: 'C' for v in SLIDE_VTS + CYCLE_VTS + [0x5bd758]})
test_twod.VT_KIND.update({0x5bffb0: 'L', 0x5bff20: 'L', 0x5bc1b0: 'I'})

# kind, constructor, size, file, dialog, slide buttons, cycles, other controls
SCREENS = {
    'credits': (0, 0x4289a0, 0x11ae0, 'dlgCredits.zrd', 'CREDITSDIALOG', [(0x111ec, 'CANCEL')], [],
                [(0x113d8, 'CREDITS_SCREEN')]),
    'options': (1, 0x4379f0, 0x1407c, 'dlgOptions.zrd', 'GAME_OPTION_DIALOG',
                [(0x111ec, 'ACCEPT'), (0x113d8, 'BACK'), (0x13b2c, 'GFX_AUDIO_OPTIONS'), (0x13d18, 'CONTROLS_CFG')],
                [(0x115c4, 'DIFFICULTY'), (0x119e8, 'CAMERA'), (0x11e0c, 'INVINCIBLE'), (0x1223c, 'INFINITE_AMMO'),
                 (0x1266c, 'HEAT_MGMT'), (0x12a9c, 'TORSO_AUTO_RTN'), (0x12ec0, 'THROTTLE_CONST'),
                 (0x132e4, 'MW2JJETS'), (0x13708, 'CMD_INVERT_MOUSE_Y')], []),
    'gfx': (2, 0x438f60, 0x13f9c, 'dlgOptions.zrd', 'GFX_AUDIO_OPTIONDIALOG',
            [(0x13a48, 'ACCEPT'), (0x13c34, 'BACK')],
            [(0x11504, 'SHADOWS'), (0x11928, 'SELF_SHADOWS'), (0x11d4c, 'LIGHTING'), (0x12170, 'RESOLUTION'),
             (0x12594, 'VIDEO_DEVICE'), (0x129b8, 'TEXTURES'), (0x12ddc, 'SOUND_QUALITY'),
             (0x13200, 'OBJECT_DETAIL'), (0x13624, 'SPECIAL_EFFECTS')],
            [(0x111ec, 'MUSICSLIDER'), (0x11378, 'SFXSLIDER')]),
}
SIZES = [(640, 480), (640, 480), (800, 600), (1024, 768), (1280, 1024), (1600, 1200)]

MECH3 = {0x40cec0: '', 0x40d760: '', 0x40d770: '', 0x576db0: '', 0x576da0: '', 0x576d90: '', 0x576d80: '',
         0x576040: 'c', 0x558290: 'c,d,s0,s1', 0x558dd0: 'c,d,s0,s1,s2,s3,s4,s5', 0x597a60: '', 0x590400: 's0,s1',
         0x5905a0: 's0', 0x482970: 's0', 0x590250: 's0,s1', 0x4ac150: 's0,s1,s2', 0x597d80: 'c',
         0x579a10: '', 0x579a40: '', 0x579b70: 'c', 0x579aa0: '', 0x579a70: 'c', 0x526750: 'zc,d', 0x557ae0: 'zc,d',
         # getters
         0x40cdf0: '', 0x40ce70: '', 0x40cf20: '', 0x40cf80: '', 0x40cfc0: '', 0x40d010: '', 0x40d060: '',
         0x40d1e0: '', 0x40cc10: '', 0x40d410: '', 0x40d480: '', 0x40d3e0: '', 0x40d3b0: '', 0x40d2f0: '',
         0x40d220: '', 0x40d280: '', 0x40d4d0: '',
         # setters (ECX, or a float pushed)
         0x40cc20: 'c', 0x40ce10: 'c', 0x40ced0: 'c', 0x40cf30: 'c', 0x40cf90: 'c', 0x40cfd0: 'c', 0x40d020: 'c',
         0x40d1c0: 'c', 0x40cbf0: 'c', 0x40d3f0: 'c', 0x40d3d0: 'c', 0x40d310: 'c', 0x40d2a0: 'c', 0x40d1f0: 'c',
         0x40d240: 'c', 0x40d430: 'c', 0x40dab0: 's0', 0x40da80: 's0'}
FLOAT_GETTERS = (0x40dae0, 0x40daa0)


def i32(v):
    return struct.unpack('<i', struct.pack('<I', v & 0xffffffff))[0]


# ---- dialogs ----

def mutate_buttons(r, v, cycles):
    nb = []
    for j in range(0, len(v) - 1, 2):
        name, entry = v[j], list(v[j + 1])
        if r.random() < 0.1:
            continue  # missing: its bind finds nothing
        if r.random() < 0.3:
            entry += ['RADIUS', [r.choice([5.0, 12.5, 33.3])]]
        if r.random() < 0.3:
            entry += ['MAXRADIUS', [r.choice([0, 25, 60, 400])]]
        if r.random() < 0.25 and 'POSITION' in entry:
            p = entry.index('POSITION')
            entry[p + 1] = [r.randrange(-30, 400), r.randrange(-30, 300)]
        for key in ('TOGGLE_LEFT', 'TOGGLE_RIGHT'):
            if key in entry and r.random() < 0.15:
                p = entry.index(key)
                del entry[p:p + 2]
        if 'LABEL' in entry and r.random() < 0.08:
            p = entry.index('LABEL')
            del entry[p:p + 2]
        nb += [name, entry]
    return nb


def synthetic(seed, body, dialog, credits):
    r = random.Random(f'menus:{seed}')
    body = [list(x) if isinstance(x, list) else x for x in body]
    out = []
    for i in range(0, len(body) - 1, 2):
        k, v = body[i], body[i + 1]
        if k == 'BUTTONS':
            v = mutate_buttons(r, v, None)
            if credits:
                for j in range(0, len(v) - 1, 2):
                    if v[j] == 'CREDITS_SCREEN':
                        v[j + 1] = credits_screen(r)
        elif k == 'PRIMITIVES':
            nb = []
            for j in range(0, len(v) - 1, 2):
                if r.random() < 0.15:
                    continue
                nb += [v[j], v[j + 1]]
            v = nb
        out += [k, v]
    name = dialog if seed % 8 != 7 else 'NOT' + dialog
    return [['SHARED_IMAGE_PATH', ['common'], name, out]]


def credits_screen(r):
    keys = ['MSG_GAME_NAME', 'MSG_CREDITS_PRODUCER', 'Rob Sears', 'MSG_NONE', 'Zipper', 'A - B']
    fonts = ['main_red', 'lab_large_grey', 'ia_dark_grey', 'no_such_font']
    secs = []
    for _ in range(r.randrange(0, 30)):
        lines = []
        for _ in range(r.choice([1, 1, 1, 2, 3])):
            line = [[r.choice(keys) for _ in range(r.choice([1, 1, 2, 3]))], 'offset',
                    [r.randrange(-10, 40), r.randrange(0, 30)]]
            if r.random() < 0.85:
                line += ['font', [r.choice(fonts)]]
            lines.append(line)
        secs.append(lines)
    x0, y0 = r.randrange(0, 80), r.randrange(0, 60)
    return ['POSITION', [r.randrange(60, 140), r.randrange(60, 140)], 'BITMAP', [],
            'RECT', [[x0, y0], [x0 + r.randrange(50, 300), y0 + r.randrange(40, 260)]],
            'SCROLL_RATE', r.choice([1.0, 0.25, 0.05, 2.0, 0.0]),
            'SCROLLING_TEXT', secs]


# ---- the plan ----

def make_plan(seed, kind, n_slides, n_cycles, n_others, spots, bound):
    """`bound`: the (list, index) pairs the dialog has; only those are pressed
    (an unbound button has no owner, and the presses read through it)."""
    r = random.Random(f'plan:{seed}')
    lines = ['N']
    if kind != 0 and r.random() < 0.9:
        lines.append('R 0')  # the state's enter reads the options
    spots = list(spots) or [(100, 100)]
    down = 0
    for _ in range(r.randrange(30, 90)):
        roll = r.random()
        pb = [i for k, i in bound if k == 'P']
        cb = [i for k, i in bound if k == 'C']
        ob = [i for k, i in bound if k == 'O']
        if roll < 0.06 and pb:
            lines.append(f'P {r.choice(pb)}')
        elif roll < 0.16 and cb:
            lines.append(f'C {r.choice(cb)}')
        elif roll < 0.19 and ob:
            lines.append(f'O {r.choice(ob)}')
        elif roll < 0.22 and n_cycles:
            lines.append(f'S {r.randrange(n_cycles)} {r.choice([-3, -1, 1, 1, 2, 5])}')
        elif roll < 0.24 and n_cycles:
            lines.append(f'Q {r.randrange(n_cycles)} {r.choice([-1, 0, 1, 2, 3, 9])}')
        elif roll < 0.26 and n_cycles:
            lines.append(f'E {r.randrange(n_cycles)} {r.randrange(2)}')
        elif roll < 0.27 and n_cycles:
            lines.append(f'W {r.randrange(n_cycles)} {r.randrange(2)} {r.randrange(2)} {r.randrange(2)}')
        elif roll < 0.30 and kind != 0:
            lines.append(f'R {r.randrange(2)}')
        elif roll < 0.31 and kind == 2:
            lines.append(f'G {r.choice([-1, 0, 1, 2])}')
        elif roll < 0.33:
            lines.append(f'B {r.randrange(2)}')
        elif roll < 0.35 and n_slides:
            lines.append(f'K {r.randrange(n_slides)} {r.randrange(2)}')
        elif roll < 0.36:
            lines.append(f'A {r.randrange(2)}')
        elif roll < 0.38:
            lines.append(r.choice('DH'))
        sx, sy = r.choice(spots)
        if r.random() < 0.85:
            x, y = sx + r.randrange(-8, 40), sy + r.randrange(-8, 30)
        else:
            x, y = r.randrange(-20, 660), r.randrange(-20, 500)
        if down == 0:
            left = 1 if r.random() < 0.25 else 0
        elif down in (1, 2):
            left = 2 if r.random() < 0.5 else 4
        else:
            left = 0
        down = left
        right = r.choice([0, 0, 0, 1, 4])
        dt = r.choice([0.0, 1 / 60, 1 / 30, 0.05, 0.2, 0.7] if kind == 0 else [0.0, 1 / 60, 1 / 30, 0.05, 0.2])
        m = [x & 0xffffffff, y & 0xffffffff, 0, fbits(0.0), fbits(0.0), fbits(0.0), 0, 0, 0,
             fbits(0.0), fbits(0.0), fbits(0.0), left, right, 0]
        lines.append('F ' + ' '.join(f'{v:x}' for v in [fbits(dt)] + m))
    lines.append('X')
    return lines


def spots_of(body):
    """Where the buttons and their arrows are (the plan's mouse targets)."""
    out = []
    bl = find(body, 'BUTTONS') or []
    for i in range(0, len(bl) - 1, 2):
        e = bl[i + 1]
        p = find(e, 'POSITION')
        if not p:
            continue
        out.append(tuple(p))
        for key in ('TOGGLE_LEFT', 'TOGGLE_RIGHT'):
            t = find(e, key)
            tp = find(t, 'POSITION') if t else None
            if tp:
                out.append((p[0] + tp[0], p[1] + tp[1]))
    return out


# ---- the original ----

class Original:
    def __init__(self, seed, kind_name, data, start, plan, cfg):
        self.t = Trace(seed=seed)
        w = self.w = TwodWorld(self.t, seed)
        e = self.e = w.emu
        self.plan, self.cfg = plan, cfg
        self.spec = SCREENS[kind_name]
        tree = w.read_tree(data, start)
        e.stub(ZRDR_LOAD, '@zrdr_load', 'c,d', None, lambda emu, key, a, pop: emu._ret(tree, pop))
        for addr, spec in MECH3.items():
            e.stub(addr, f'{addr:#010x}', spec)
        def strdup(emu, key, a, pop):  # the cycles' choices
            s = emu.cpu.read(a[0], 256).split(b'\0')[0] if a[0] else b''
            p = emu.alloc(len(s) + 1)
            emu.cpu.write(p, s + b'\0')
            emu._ret(p if a[0] else 0, 0)
        e.import_stub(IAT['_strdup'], '@_strdup', 's0', 0, strdup)

        def sprintf(emu, key, a, pop):  # the device names, as their own format
            buf, fmt = a
            va = emu.uc.reg_read(UC_X86_REG_ESP) + 12
            s = c_format(emu, emu.cstr(fmt), va).encode('latin1')
            emu.cpu.write(buf, s + b'\0')
            emu._ret(len(s), 0)
        e.import_stub(IAT['sprintf'], '@sprintf', 's0,s1', 0, sprintf)
        fbuf = e.world_alloc(16)
        for i, addr in enumerate(FLOAT_GETTERS):
            thunk = e.thunk(b'\xd9\x05' + struct.pack('<I', fbuf + 4 * i) + b'\xc3')

            def fret(emu, key, a, pop, slot=fbuf + 4 * i, thunk=thunk):
                emu.w32(slot, self.t.handle(key, ()))
                emu._ret(0, 0, then=thunk)
            e.stub(addr, f'{addr:#010x}', '', None, fret)

        def card_memory(emu, key, a, pop):  # [0x0072f38c]: ECX device, EDX the memory out
            v = self.t.handle(key, (a[0],))
            emu.w32(a[1], self.t.handle('@video_total', ()))
            emu._ret(v, 4)
        e.w32(0x72f38c, e.new_stub('[0x0072f38c]', 'c,d', 4, card_memory))
        r = rnd(seed, 'answers')
        mask = cfg['install']
        names = [e.world_string(n) for n in ('3Dfx Voodoo2', 'Riva TNT', 'Card 3', 'G400')]
        w.answers.update({
            '0x0040cec0': (lambda rr, k, a: rr.randrange(2)) if cfg['fast'] == 'random' else cfg['fast'],
            '0x0040d760': cfg['size'][0], '0x0040d770': cfg['size'][1],
            '0x00576040': lambda rr, k, a: e.world_string(f'M{a[0]:x}'),
            '0x00579a70': lambda rr, k, a: names[a[0] % len(names)],
            '0x00575fb0': lambda rr, k, a: e.world_string('T:' + a[0]) if isinstance(a[0], str) else 0,
            '0x00597d80': lambda rr, k, a: a[0] & mask,
            '0x00579aa0': cfg['devices'], '0x00579a10': cfg['vram'][0], '0x00579a40': cfg['vram'][1],
            '0x00579b70': lambda rr, k, a: rr.randrange(2),
            '[0x0072f38c]': lambda rr, k, a: 1 if rr.random() < 0.8 else 0,
            '@video_total': cfg['card_mb'] << 20,
            '0x004ac150': lambda rr, k, a: rr.choice([1, 1, 2, 6]),
            '0x00482970': 0x608ef8,
            '0x0040dae0': lambda rr, k, a: fbits(rr.choice([0.0, 0.5, 1.0, 0.25, 1.5, -0.5])),
            '0x0040daa0': lambda rr, k, a: fbits(rr.choice([0.0, 0.5, 1.0, 0.75])),
            '0x0040cc10': lambda rr, k, a: rr.choice([-1, -1, 0, 1, 2]) & 0xffffffff,
            '0x0040d410': lambda rr, k, a: rr.choice([0, 0, 3, 5, 6, 7]),
            '0x0040d3b0': lambda rr, k, a: rr.choice([0, 1, 2, 3]),
            '0x0040d2f0': lambda rr, k, a: rr.choice([0, 1, 2, 3]),
            '0x0040d3e0': lambda rr, k, a: rr.choice([0, 2, 1]),
            '0x0040ce70': lambda rr, k, a: rr.choice([0, 1, 2, 3]),
            '0x0040cdf0': lambda rr, k, a: rr.choice([0, 1, 2, 5]),
            '0x0040d480': lambda rr, k, a: rr.randrange(0x40),
            '0x00558dd0': 0, '0x00558290': 0, '0x00590400': 0, '0x005905a0': 0, '0x00590250': 0,
        })
        for g in ('0x0040cf20', '0x0040cf80', '0x0040cfc0', '0x0040d010', '0x0040d060', '0x0040d1e0',
                  '0x0040d220', '0x0040d280', '0x0040d4d0'):
            w.answers[g] = lambda rr, k, a: rr.choice([0, 1, 1, 4])
        for i, a in enumerate((0x576db0, 0x576da0, 0x576d90, 0x576d80)):
            w.answers[f'{a:#010x}'] = 0x5e000000 + i
        # The state, its screen and the game options' (vt+0x28, vt+0x10 stubs).
        vt = e.world_alloc(0x40)
        e.w32(vt + 0x28, e.new_stub('vt+0x28', 'c', 0))
        e.w32(vt + 0x10, e.new_stub('vt+0x10', 'c,s0', 4))
        self.state = e.world_alloc(0x10)
        fake_screen = e.world_alloc(0x10)
        e.w32(fake_screen, vt)
        e.w32(self.state, vt)
        e.w32(self.state + 0xc, fake_screen)
        other = e.world_alloc(0x10)
        e.w32(other, vt)
        other_screen = e.world_alloc(0x10)
        e.w32(other_screen, vt)
        e.w32(other + 0xc, other_screen)
        e.w32(0x609868, other if r.random() < 0.8 else 0)
        e.w32(0x60986c, 0x51000004)
        e.w32(0x609870, 0x51000008)
        # The main menu's background, a running mission, the entries.
        if cfg['main']:
            mm = e.world_alloc(0x12c1c)
            e.w32(mm + 0x110ec, w.fake_texture(1280, 960) if r.random() < 0.9 else 0)
            e.w8(mm + 0x111e8, cfg['main_moving'])
            e.w32(0x609854, mm)
        else:
            e.w32(0x609854, 0)
        e.w8(0x608ed9, cfg['mission'])
        for g, v in ((0x5f4d30, fbits(r.choice([1.3, -1.3, 0.8, -2.0]))), (0x5f4d10, r.randrange(3, 11))):
            ent = e.world_alloc(16)
            e.w32(ent, v)
            e.w32(g, ent)

    def vcall(self, obj, slot, stack=()):
        return self.e.call(self.e.r32(self.e.r32(obj) + slot), ecx=obj, stack=stack)

    def run(self):
        e, w, t = self.e, self.w, self.t
        kind, ctor, size = self.spec[0], self.spec[1], self.spec[2]
        slides = [o for o, _ in self.spec[5]]
        cycles = [o for o, _ in self.spec[6]]
        others = [o for o, _ in self.spec[7]]
        self.snapshot = e.snapshot()
        m = self.d = e.alloc(size)
        dump = ''
        for line in self.plan:
            p = line.split()
            op = p[0]
            if op == 'N':
                try:
                    e.call(ctor, ecx=m, stack=(self.state,))
                except Exception:
                    from unicorn.x86_const import UC_X86_REG_EIP
                    print("FAIL at", hex(e.uc.reg_read(UC_X86_REG_EIP)), t.log[-6:])
                    raise
            elif op == 'F':
                vals = [int(v, 16) for v in p[1:]]
                w.mouse_frames.append(vals[1:])
                self.vcall(m, 0x00, (vals[0],))
                w.frame += 1
            elif op == 'A':
                self.vcall(m, 0x04, (int(p[1]),))
            elif op == 'R':
                self.vcall(m, 0x10, (int(p[1]),))
            elif op == 'P':
                self.vcall(m + slides[int(p[1])], 0x30)
            elif op == 'C':
                self.vcall(m + cycles[int(p[1])], 0x30)
            elif op == 'O':
                self.vcall(m + others[int(p[1])], 0x30)
            elif op == 'S':
                self.vcall(m + cycles[int(p[1])], 0xa4, (int(p[2]) & 0xffffffff,))
            elif op == 'Q':
                self.vcall(m + cycles[int(p[1])], 0xa0, (int(p[2]) & 0xffffffff,))
            elif op == 'E':
                self.vcall(m + cycles[int(p[1])], 0x90, (int(p[2]),))
            elif op == 'W':
                self.vcall(m + cycles[int(p[1])], 0x94, tuple(int(x) for x in p[2:5]))
            elif op == 'B':
                old = self.vcall(m, 0x18, (int(p[1]),)) & 0xff
                t.handle('busy', (old, self.vcall(m, 0x1c) & 0xff))
            elif op == 'D':
                e.call(0x59fa60, ecx=m)
            elif op == 'H':
                e.call(0x59fa40, ecx=m)
            elif op == 'K':
                self.vcall(m + slides[int(p[1])], 0x90, (int(p[2]),))
            elif op == 'G':
                e.call(0x439a90, ecx=m, stack=(int(p[1]) & 0xffffffff,))
            elif op == 'X':
                dump = test_twod.Original.dump(self) + self.dump_screen(slides, cycles, others, kind)
                self.vcall(m, 0x08, (1,))
        return dump

    # The section probe_menus.cpp's dump_screen writes.
    def dump_screen(self, slides, cycles, others, kind):
        e, m = self.e, self.d
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
            vals = [i32(e.r32(a + 4 * i)) for i in range(4)]
            out.append(f'{k}=' + ','.join(str(v) for v in vals) + ' ')

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

        def fonttext(o):
            base(o)
            s('text', o + 0x34)
            kv('align', e.r32(o + 0x244))
            tx('timg', e.r32(o + 0x248))
            for k, off in (('top', 0x24c), ('bottom', 0x250), ('hfont', 0x258)):
                kv(k, e.r32(o + off))
            s('rendered', o + 0x260)
            for k, off in (('w', 0x460), ('h', 0x464), ('shadow', 0x468), ('bk', 0x46c), ('nr', 0x474)):
                kv(k, e.r32(o + off))
            rc('tr', o + 0x490)
            kv('maxw', e.r32(o + 0x4a8))
            kv('named', e.r32(o + 0x4ac))

        out.append('menu ')
        kv('state', e.r32(m + 0x10fec))
        kv('busy', e.r8(m + 0x10ff0))
        out.append('\nstill ')
        image(m + 0x10ff4)
        bg = m + 0x110b0
        out.append('\nbackground ')
        image(bg)
        kv('step', e.r32(bg + 0xbc))
        kv('stepy', e.r32(bg + 0xc0))
        for i in range(4):
            kv('pc', e.r8(bg + 0xc4 + i))
            kv('px', e.r32(bg + 0xc8 + 8 * i))
            kv('py', e.r32(bg + 0xcc + 8 * i))
            rc('psrc', bg + 0xf8 + 0x10 * i)
        rc('view', bg + 0xe8)
        kv('moving', e.r8(bg + 0x138))
        for i, off in enumerate(slides):
            k = m + off
            out.append(f'\nslide{i} ')
            kv('reading', e.r8(k + 0x14c))
            kv('id', e.r32(k + 0x150))
            kv('borrowed', e.r8(k + 0x154))
            tx('stashed', e.r32(k + 0x158))
            rp = k + 0x16c
            kv('rx', e.r32(rp + 0x14))
            kv('ry', e.r32(rp + 0x18))
            for name, o in (('inner', 0x38), ('outer', 0x3c), ('ramp', 0x40)):
                kv(name, e.r32(rp + o))
            kv('rippling', e.r8(k + 0x1b8))
            for name, o in (('amp', 0x1c0), ('phase', 0x1c8), ('radius', 0x1cc), ('maxr', 0x1d0), ('rr', 0x1d4)):
                kv(name, e.r32(k + o))
        for i, off in enumerate(cycles):
            c = m + off
            out.append(f'\ncycle{i} ')
            kv('reading', e.r8(c + 0x14c))
            kv('cid', e.r32(c + 0x420))
            kv('index', e.r32(c + 0x16c))
            kv('click', e.r8(c + 0x168))
            kv('wraps', e.r8(c + 0x170))
            kv('hover', e.r32(c + 0x174))
            for name, o in (('pdx', 0x178), ('pdy', 0x17c), ('ndx', 0x180), ('ndy', 0x184)):
                kv(name, e.r32(c + o))
            first, last = e.r32(c + 0x15c), e.r32(c + 0x160)
            items = [e.r32(first + 4 * j) for j in range((last - first) // 4)] if first else []
            kv('n', len(items))
            for it in items:
                tp = e.r32(it)
                out.append('item="' + (e.cstr(tp).replace(' ', '_') if tp else '(null)') + '" ')
                kv('v', e.r32(it + 4))
            if e.r32(c) in ALERT_VTS:
                kv('fnorm', e.r32(c + 0x424))
                kv('falert', e.r32(c + 0x428))
                kv('alert', e.r32(c + 0x42c))
            out.append('\n  prev ')
            control(c + 0x188)
            out.append('\n  next ')
            control(c + 0x2d4)
        for i, off in enumerate(others):
            o = m + off
            out.append(f'\nother{i} ')
            if kind == 2:
                kv('reading', e.r8(o + 0x188))
                kv('value', e.r32(o + 0x14c))
        if kind == 0:
            n = m + 0x113d8
            out.append('\ncredits ')
            kv('clock', e.r32(m + 0x11adc))
            kv('unused', e.r32(m + 0x11ad8))
            for name, o in (('left', 0x15c), ('top', 0x160), ('right', 0x164), ('bottom', 0x168), ('total', 0x16c),
                            ('rate', 0x170)):
                kv(name, e.r32(n + o))
            first, last = e.r32(n + 0x150), e.r32(n + 0x154)
            secs = [first + 0x10 * j for j in range((last - first) // 0x10)] if first else []
            kv('nsec', len(secs))
            for j, sec in enumerate(secs):
                out.append(f'\n  sec{j} ')
                lf, ll = e.r32(sec + 4), e.r32(sec + 8)
                lines = [lf + 0x4b8 * q for q in range((ll - lf) // 0x4b8)] if lf else []
                kv('nlines', len(lines))
                for ln in lines:
                    out.append('\n    line ')
                    fonttext(ln)
                    kv('dx', e.r32(ln + 0x4b0))
                    kv('dy', e.r32(ln + 0x4b4))
        if kind == 2:
            out.append('\ngfx ')
            kv('dev', e.r32(m + 0x13f98))
        out.append('\nentries ')
        kv('accel_y', e.r32(e.r32(0x5f4d30)))
        kv('vmode', e.r32(e.r32(0x5f4d10)))
        out.append('\n')
        return ''.join(out)


def run_case(seed, kind_name, path, data, start, plan, cfg, verbose):
    orig = Original(seed, kind_name, data, start, plan, cfg)
    dump_o = orig.run()
    rep = Replay(orig.t, orig.e, orig.snapshot)
    probe.probe_app_set_callback(ctypes.cast(rep.callback(), ctypes.c_void_p))
    buf = ctypes.create_string_buffer(1 << 23)
    n = probe.probe_menus_run(str(path).encode(), SCREENS[kind_name][0], orig.state, '\n'.join(plan).encode(),
                              buf, len(buf))
    assert n >= 0, 'probe failed'
    dump_r = buf.value.decode('latin1')
    stubbed = orig.w.stub_textures
    to, do = canonical(orig.t.log, dump_o, stubbed)
    tr, dr = canonical(rep.log, dump_r, stubbed)
    # the fonts' faces point into the freed tree (read only while loading)
    do, dr = (re.sub(r'face=".*?" height=', 'height=', x, flags=re.S) for x in (do, dr))
    ok = compare(f'{kind_name} seed {seed}', to, tr)
    if do != dr:
        ok = False
        for a, b in zip(do.split('\n'), dr.split('\n')):
            if a != b:
                print(f'{kind_name} seed {seed}: dumps differ')
                ta, tb = a.split(' '), b.split(' ')
                print('  original:', ' '.join(x for x in ta if x not in tb)[:400])
                print('  rebuild: ', ' '.join(x for x in tb if x not in ta)[:400])
                print('  in:', a[:120])
                break
        else:
            print(f'{kind_name} seed {seed}: dumps differ in length')
    if verbose and ok:
        print(f'{kind_name} seed {seed}: {len(to)} calls')
    return ok, to


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    runs = int(args[0]) if args else 30
    which = args[1:] or list(SCREENS)
    verbose = '--verbose' in sys.argv
    data = READER.read_bytes()
    total_ok = total = 0
    for kind_name in which:
        kind, ctor, size, fname, dialog, slides, cycles, others = SCREENS[kind_name]
        start = next(s for n, s, _ in entries(data) if n == fname)
        tree, _ = parse_node(data, start)
        body = find(tree, dialog)
        synth_path = ROOT / 'build' / f'menus_{kind_name}_synth.zbd'
        passed = calls = 0
        stats = {}
        for seed in range(runs):
            r = random.Random(f'cfg:{kind_name}:{seed}')
            cfg = {'fast': r.choice([0, 0, 0, 1, 'random']), 'size': r.choice(SIZES),
                   'main': r.random() < 0.7, 'main_moving': r.randrange(2), 'mission': 1 if r.random() < 0.3 else 0,
                   'install': r.choice([0xffffffff, 0xffffffff, 0x101, 0xff00, 0x10700, 0]),
                   'devices': r.choice([0, 1, 2, 3]), 'vram': r.choice([(0, 0), (8 << 20, 1 << 20), (3 << 20, 1 << 20),
                                                                      (2 << 20, 0)]),
                   'card_mb': r.choice([1, 2, 4, 8, 16])}
            if kind_name == 'credits':
                cfg['main'] = r.random() < 0.95 or seed % 2 == 0  # the credits read it unchecked
            if seed % 2:
                t = synthetic(seed, body, dialog, kind_name == 'credits')
                blob = write_node(t)
                synth_path.write_bytes(archive([(fname, blob)]))
                here = find(t, dialog) or []
                path, file_data, file_start = synth_path, blob, 0
            else:
                here = body
                path, file_data, file_start = READER, data, start
            if kind_name == 'credits' and not cfg['main']:
                cfg['main'] = True  # (the original would crash: FIX (bug) in borrow_background)
            names = [x for x in (find(here, 'BUTTONS') or [])[::2] if isinstance(x, str)] if here else []
            bound = [(k, i) for k, lst in (('P', slides), ('C', cycles), ('O', others))
                     for i, (_, n) in enumerate(lst) if n in names]
            plan = make_plan(seed, kind, len(slides), len(cycles), len(others), spots_of(here), bound)
            ok, trace = run_case(seed, kind_name, path, file_data, file_start, plan, cfg, verbose)
            passed += ok
            calls += len(trace)
            for k, _ in trace:
                if k in ('0x00590400', '0x005905a0', '0x00590250', 'vt+0x28', 'vt+0x10', '0x004ac150', '0x00597a60'):
                    stats[k] = stats.get(k, 0) + 1
        print(f'{kind_name}: {passed}/{runs} runs identical ({calls} calls; ' +
              ', '.join(f'{k} x{v}' for k, v in sorted(stats.items())) + ')')
        total_ok += passed
        total += runs
    return 0 if total_ok == total else 1


if __name__ == '__main__':
    sys.exit(main())
