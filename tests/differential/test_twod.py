"""zTwoD's dialogs against the original x86 (docs/twod.md, Testing).

    cmake --build build && .venv/bin/python tests/differential/test_twod.py [filter] [--seeds N]

For every dialog of the shipped menus (dialog.zrd, briefing.zrd and the
dlg*.zrd files in reader.zbd), the original constructs a dialog (0x0059ec40),
loads it from the original zReader's tree (0x0059f050 -> 0x0059f080, drawing
its static parts once), binds every BUTTONS entry as a push button (0x0059bbe0,
or the check button 0x0059d870 when it has CHECKED) and every PRIMITIVES entry
as an image (0x0059b970) or a text (0x005a15a0) with 0x0059f8b0 / 0x0059f8d0,
then runs a random plan: activation, frames of mouse input over the buttons
(0x005a2bb0), enabling and disabling, hiding and showing, checking, bounds.
Every call out of zTwoD is stubbed (twod_world.py) and recorded; the port
(tests/probe_twod.cpp) runs the same plan with the original's answers
replayed. Compared: the call traces and a dump of the dialog and every object
in it (fonts, sounds, positions, flags, images, labels, text and its render
state, the cursor), images the code made itself matched by first appearance.
The x87 runs at 24 bits, as in play.
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import Replay, Trace, compare, fmt, load_probe  # noqa: E402
from twod_world import READER, TwodWorld, entries, find, parse_node, synthetic_dialog, write_node  # noqa: E402
from original import ROOT  # noqa: E402

probe = load_probe()
probe.probe_app_set_callback.argtypes = [ctypes.c_void_p]
probe.probe_twod_run.argtypes = [ctypes.c_char_p, ctypes.c_long, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p,
                                 ctypes.c_char_p, ctypes.c_int]

FILES = ['dialog.zrd', 'briefing.zrd', 'dlgPause.zrd', 'dlgWeapon.zrd', 'dlgMechLab.zrd', 'dlgSalvage.zrd',
         'dlgWin.zrd', 'dlgMessage.zrd', 'dlgMechLabII.zrd', 'dlgOptions.zrd', 'dlgCredits.zrd']
DIALOG_CTOR, LOAD_FROM, BIND_BUTTON, BIND_PRIMITIVE = 0x59ec40, 0x59f050, 0x59f8b0, 0x59f8d0
CLASSES = {'C': (0x59bbe0, 0x14c, ()), 'K': (0x59d870, 0x164, ()), 'I': (0x59b970, 0xbc, (0,)),
           'T': (0x5a15a0, 0x4cc, ())}
VT_KIND = {0x5cf8c8: 'I', 0x5cf948: 'C', 0x5cfaa8: 'K', 0x5cf9d8: 'T', 0x5cfc88: 'U', 0x5d0da0: 'U',
           0x5cfb38: 'L', 0x5d06e0: 'N', 0x5be308: 'X', 0x5cfbc8: 'Y', 0x5cfe00: 'Z', 0x5cfd28: 'F'}
TEXTURE_ARGS = {'0x0055b1f0': (0,), '0x00517b10': (0,), '0x00517cc0': (0,), '[0x0072f394]': (0,),
                '[0x0072f398]': (0,), '0x00579660': (5,)}


def fbits(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


# ---- the plan ----

def make_plan(seed, dialog, buttons, prims):
    r = random.Random(f'{seed}:{dialog}')
    lines, bound = [], []
    for n, entry in buttons:
        if find(entry, 'LISTSIZE') is not None:
            k = 'X'
        elif find(entry, 'FILLBITMAP') is not None:
            k = 'L'
        else:
            k = 'K' if find(entry, 'CHECKED') is not None else 'C'
        lines.append(f'B {n} {k}')
        bound.append((k, entry))
    for n, entry in prims:
        k = 'T' if find(entry, 'TEXT') is not None else 'I'
        if k == 'T' and r.random() < 0.4:
            lines.append(f'P {n} Z {r.randrange(1, 7)}')  # a text box of that many lines
            bound.append(('Z', entry))
            continue
        lines.append(f'P {n} {k}')
        bound.append((k, entry))
    lines.append('A 1')
    lines += [f'R {i}' for i, b in enumerate(bound) if b[0] in 'CK']  # every button's bounds (get_rect)
    boxes = {}
    for line in lines:
        if line.startswith('P ') and line.split()[2] == 'Z':
            boxes[len(boxes)] = int(line.split()[3])
    zi = [i for i, b in enumerate(bound) if b[0] == 'Z']
    box_lines = {i: boxes[k] for k, i in enumerate(zi)}
    for i, b in enumerate(bound):  # each text box filled past its lines, grown, turned bottom-up
        if b[0] != 'Z':
            continue
        for _ in range(r.randrange(4, 16)):
            op = r.choice('NNNNNOYYDUa')
            if op in 'NO':
                lines.append(f'{op} {i} {r.randrange(-5, 99999)}')
            elif op == 'Y':
                # grown only: a shrink leaves the current line past the end (FIX (bug))
                box_lines[i] = r.randrange(box_lines[i], box_lines[i] + 3)
                lines.append(f'Y {i} {box_lines[i]}')
            elif op == 'D':
                lines.append(f'D {i} {r.randrange(2)}')
            elif op == 'a':
                lines.append(f'a {i} more{r.randrange(100)}')
            else:
                lines.append(f'U {i}')
        lines.append(f'T {i}')
    spots = []
    for k, entry in bound:
        p = find(entry, 'POSITION')
        if isinstance(p, list) and len(p) >= 2 and all(isinstance(v, int) for v in p[:2]):
            spots.append((p[0], p[1]))
    spots = spots or [(100, 100)]
    x, y = 320, 240
    buttons_down = 0
    for _ in range(r.randrange(20, 60)):
        roll = r.random()
        if roll < 0.08 and bound:
            i = r.randrange(len(bound))
            if bound[i][0] in 'CK':
                lines.append(f'E {i} {r.randrange(2)}')
        elif roll < 0.12 and bound:
            lines.append(f'H {r.randrange(len(bound))}')
        elif roll < 0.17 and bound:
            lines.append(f'S {r.randrange(len(bound))} {fbits(r.choice([-1.0, 0.05, 0.4])):x}')
        elif roll < 0.21 and bound:
            ks = [i for i, b in enumerate(bound) if b[0] == 'K']
            if ks:
                lines.append(f'X {r.choice(ks)} {r.randrange(2)}')
        elif roll < 0.26 and bound:
            ks = [i for i, b in enumerate(bound) if b[0] in 'CK']
            if ks:
                lines.append(f'R {r.choice(ks)}')
        elif roll < 0.28:
            lines.append(f'A {r.randrange(2)}')
        elif 0.34 <= roll < 0.42 and any(b[0] in 'LX' for b in bound):
            i = r.choice([i for i, b in enumerate(bound) if b[0] in 'LX'])
            if bound[i][0] == 'L':
                if r.random() < 0.5:
                    lines.append(f'V {i} {fbits(r.choice([-0.5, 0.0, 0.25, 0.5, 0.99, 1.0, 1.7, r.random()])):x}')
                else:
                    lines.append(f'W {i}')
            elif r.random() < 0.6:
                lines.append(f'G {i} item{r.randrange(1000)}')
            else:
                lines.append(f'J {i} {r.randrange(-3, 12)}')
        elif 0.28 <= roll < 0.34 and any(b[0] == 'Z' for b in bound):
            i = r.choice([i for i, b in enumerate(bound) if b[0] == 'Z'])
            op = r.choice('NNNNOUCKYDaT')
            if op in 'NO':
                lines.append(f'{op} {i} {r.randrange(-5, 99999)}')
            elif op == 'C':
                lines.append(f'C {i} {r.randrange(1 << 24):x}')
            elif op == 'Y':
                box_lines[i] = r.randrange(box_lines[i], box_lines[i] + 3)
                lines.append(f'Y {i} {box_lines[i]}')
            elif op == 'D':
                lines.append(f'D {i} {r.randrange(2)}')
            elif op == 'a':
                lines.append(f'a {i} more{r.randrange(100)}')
            else:
                lines.append(f'{op} {i}')
        elif 0.42 <= roll < 0.48:
            ts = [i for i, b in enumerate(bound) if b[0] == 'T']
            if ts:
                i = r.choice(ts)
                op = r.choice('MQLZ')
                if op == 'Q':
                    lines.append(f'Q {i} {r.randrange(-50, 5000)}')
                elif op == 'L':
                    lines.append(f'L {i} {r.randrange(1 << 24):x} {fbits(r.choice([0.0, 0.3, 1.0])):x}')
                else:
                    lines.append(f'{op} {i}')
        # a frame: the mouse near a button, its buttons stepping 0 -> 1 -> 2.. -> 4 -> 0
        sx, sy = r.choice(spots)
        x = sx + r.randrange(-8, 60) if r.random() < 0.8 else r.randrange(-20, 660)
        y = sy + r.randrange(-8, 40) if r.random() < 0.8 else r.randrange(-20, 500)
        if buttons_down == 0:
            left = 1 if r.random() < 0.3 else 0
        elif buttons_down in (1, 2):
            left = 2 if r.random() < 0.5 else 4
        else:
            left = 0
        buttons_down = left
        right = r.choice([0, 0, 0, 1, 4])
        dt = r.choice([0.0, 1 / 30, 0.05, 0.2])
        m = [x & 0xffffffff, y & 0xffffffff, 0, fbits(0.0), fbits(0.0), fbits(0.0), 0, 0, 0,
             fbits(0.0), fbits(0.0), fbits(0.0), left, right, 0]
        lines.append('F ' + ' '.join(f'{v:x}' for v in [fbits(dt)] + m))
    return lines


# ---- the original ----

class Original:
    def __init__(self, seed, data, offset, dialog, plan, no_draw):
        self.t = Trace(seed=seed)
        self.w = TwodWorld(self.t, seed)
        self.e = self.w.emu
        self.data, self.offset, self.dialog, self.plan, self.no_draw = data, offset, dialog, plan, no_draw

    def vcall(self, obj, slot, stack=()):
        vt = self.e.r32(obj)
        return self.e.call(self.e.r32(vt + slot), ecx=obj, stack=stack)

    def run(self):
        e, w, t = self.e, self.w, self.t
        root = w.read_tree(self.data, self.offset)
        self.snapshot = e.snapshot()
        d = self.d = e.alloc(0x10fec)
        e.call(DIALOG_CTOR, ecx=d)
        name = e.world_string(self.dialog)
        e.call(LOAD_FROM, ecx=d, stack=(root, name, self.no_draw))
        self.bound = []
        for line in self.plan:
            p = line.split()
            op = p[0]
            if op in 'BP':
                ctor, size, args = {'C': (0x59bbe0, 0x14c, ()), 'K': (0x59d870, 0x164, ()),
                                    'I': (0x59b970, 0xbc, (0,)), 'T': (0x5a15a0, 0x4cc, ()),
                                    'L': (0x59e0f0, 0x188, ()), 'X': (0x59e430, 0x650, ()),
                                    'Z': (0x5a18f0, 0x4c8, None)}[p[2]]
                if args is None:
                    args = (int(p[3]),)
                o = e.alloc(size)
                e.call(ctor, ecx=o, stack=args)
                self.bound.append(o)
                t.handle('bind', (len(self.bound),))
                e.call(BIND_BUTTON if op == 'B' else BIND_PRIMITIVE, ecx=d,
                       stack=(o, e.world_string(p[1])))
            elif op == 'A':
                self.vcall(d, 0x04, (int(p[1]),))
            elif op == 'F':
                vals = [int(v, 16) for v in p[1:]]
                w.mouse_frames.append(vals[1:])
                self.vcall(d, 0x00, (vals[0],))
                w.frame += 1
            elif op == 'E':
                o = self.bound[int(p[1])]
                e.w32(o + 0xc4, int(p[2]))
                self.vcall(o, 0x80)
            elif op == 'H':
                self.vcall(self.bound[int(p[1])], 0x64)
            elif op == 'S':
                self.vcall(self.bound[int(p[1])], 0x60, (int(p[2], 16),))
            elif op == 'X':
                self.vcall(self.bound[int(p[1])], 0x8c, (int(p[2]),))
            elif op == 'M':
                self.vcall(self.bound[int(p[1])], 0x80)
            elif op == 'Q':
                o = self.bound[int(p[1])]
                e.call(e.r32(e.r32(o) + 0x7c), stack=(o, e.world_string('V%d'), int(p[2])))
            elif op == 'L':
                e.call(0x5a16e0, ecx=self.bound[int(p[1])], stack=(int(p[2], 16), int(p[3], 16)))
            elif op == 'Z':
                e.call(0x5a1720, ecx=self.bound[int(p[1])])
            elif op == 'V':
                e.call(0x59e350, ecx=self.bound[int(p[1])], stack=(int(p[2], 16),))
            elif op == 'W':
                e.call(0x59e2f0, ecx=self.bound[int(p[1])])
            elif op == 'G':
                self.push_string(self.bound[int(p[1])], p[2])
            elif op == 'J':
                self.vcall(self.bound[int(p[1])], 0x90, (int(p[2]) & 0xffffffff,))
            elif op == 'N':
                o = self.bound[int(p[1])]
                e.call(e.r32(e.r32(o) + 0x7c), stack=(o, e.world_string('L%d'), int(p[2]) & 0xffffffff))
            elif op == 'O':
                o = self.bound[int(p[1])]
                e.call(e.r32(e.r32(o) + 0xb0), stack=(o, e.world_string('T%d'), int(p[2]) & 0xffffffff))
            elif op == 'U':
                e.call(0x5a22a0, ecx=self.bound[int(p[1])])
            elif op == 'C':
                e.call(0x5a21e0, ecx=self.bound[int(p[1])], stack=(int(p[2], 16),))
            elif op == 'K':
                e.call(0x5a24e0, ecx=self.bound[int(p[1])])
            elif op == 'Y':
                e.call(0x5a2320, ecx=self.bound[int(p[1])], stack=(int(p[2]),))
            elif op == 'D':
                e.w8(self.bound[int(p[1])] + 0x4b4, int(p[2]))
            elif op == 'a':
                e.call(0x5a22f0, ecx=self.bound[int(p[1])], stack=(e.world_string(p[2]),))
            elif op == 'T':
                o = self.bound[int(p[1])]
                th = e.call(0x5a25f0, ecx=o)
                mw = e.call(0x5a2640, ecx=o)
                t.handle('tb', (th, mw))
            elif op == 'R':
                r = self.vcall(self.bound[int(p[1])], 0x2c)
                if r:
                    t.handle('rect', tuple(e.r32(r + 4 * i) for i in range(4)))
                else:
                    t.handle('rect', ())
        return self.dump()

    def push_string(self, box, text):
        """Appends a CString (a pointer to the characters) to the box's vector at +0x628."""
        e = self.e
        cs = e.alloc(4)
        e.w32(cs, e.world_string(text))
        first, last = e.r32(box + 0x62c), e.r32(box + 0x630)
        old = [e.r32(first + 4 * i) for i in range((last - first) // 4)] if first else []
        new = e.alloc(4 * (len(old) + 1))
        for i, v in enumerate(old + [cs]):
            e.w32(new + 4 * i, v)
        e.w32(box + 0x62c, new)
        e.w32(box + 0x630, new + 4 * (len(old) + 1))
        e.w32(box + 0x634, new + 4 * (len(old) + 1))

    # The dump probe_twod.cpp writes, from the emulator's memory.
    def dump(self):
        e, d = self.e, self.d
        objs = []
        p = e.r32(d + 8)
        while p:
            objs.append(p)
            p = e.r32(p + 4)
        out = []

        def i32(v):
            return v & 0xffffffff
        line = []

        def kv(k, v):
            line.append(f'{k}={i32(v):x}')

        def tx(k, v):
            line.append(f'{k}=T{v:x}')

        def s(k, a):
            line.append(f'{k}="' + e.cstr(a).replace(' ', '_') + '"')

        def rc(k, a):
            vals = [struct.unpack('<i', struct.pack('<I', e.r32(a + 4 * i)))[0] for i in range(4)]
            line.append(f'{k}=' + ','.join(str(v) for v in vals))

        def ref(k, a):
            line.append(f'{k}=o{objs.index(a) if a in objs else -1}')

        def labels(k, a):
            first, last = e.r32(a + 4), e.r32(a + 8)
            items = [e.r32(first + 4 * i) for i in range((last - first) // 4)] if first else []
            line.append(f'{k}=' + ''.join(f'o{objs.index(x) if x in objs else -1},' for x in items))

        def flush(head):
            out.append(head + ' ' + ' '.join(line) + ' ')
            line.clear()

        def base(o):
            ref('next', e.r32(o + 4))
            kv('flags', e.r32(o + 0xc))
            kv('timer', e.r32(o + 0x10))
            kv('x', e.r32(o + 0x14))
            kv('y', e.r32(o + 0x18))
            tx('bg', e.r32(o + 0x1c))
            rc('bgr', o + 0x20)
            kv('mouse', e.r32(o + 0x30) & 0xffff)

        def line2d(o):
            base(o)
            kv('end', e.r32(o + 0x34))
            kv('end_y', e.r32(o + 0x38))
            kv('color', e.r32(o + 0x3c))
            kv('clipped', e.r32(o + 0x40))
            kv('dash', e.r32(o + 0x48))

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
                line.append('src=0')
            kv('center', e.r32(o + 0x48))

        def text(o):
            base(o)
            s('text', o + 0x34)
            kv('font', e.r32(o + 0x234))
            kv('centered', e.r32(o + 0x238))
            kv('align', e.r32(o + 0x244))
            tx('timg', e.r32(o + 0x248))
            ti = e.r32(o + 0x248)
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
                if k != 'bkc' or e.r32(o + 0x46c) == 2:  # unset unless opaque
                    kv(k, e.r32(o + off))
            rc('fr', o + 0x480)
            rc('tr', o + 0x490)
            for k, off in (('sdx', 0x4a0), ('sdy', 0x4a4), ('maxw', 0x4a8), ('named', 0x4ac), ('ft', 0x4b0),
                           ('frate', 0x4b4), ('atop', 0x4b8)):
                kv(k, e.r32(o + off))
            kv('fon', e.r32(o + 0x4c0) & 0xff)
            kv('fmode', e.r32(o + 0x4c4))
            kv('phase', e.r32(o + 0x4c8))

        def control(o):
            image(o)
            for k, off in (('bx', 0xbc), ('by', 0xc0), ('en', 0xc4)):
                kv(k, e.r32(o + off))
            rc('bounds', o + 0xcc)
            for k, off, tex in (('inorm', 0xdc, 1), ('idis', 0xe0, 1), ('iroll', 0xe4, 1), ('sroll', 0xe8, 0),
                                ('proll', 0xec, 0), ('vroll', 0xf0, 0), ('iact', 0xf4, 1), ('sact', 0xf8, 0),
                                ('pact', 0xfc, 0), ('vact', 0x100, 0), ('sdis', 0x104, 0)):
                (tx if tex else kv)(k, e.r32(o + off))
            labels('L', o + 0x10c)
            labels('LR', o + 0x11c)
            labels('LA', o + 0x12c)
            labels('LD', o + 0x13c)

        def slider(o):
            control(o)
            kv('value', e.r32(o + 0x14c))
            tx('fill', e.r32(o + 0x150))
            rc('fsrc', o + 0x154)
            kv('fdx', e.r32(o + 0x164))
            kv('fdy', e.r32(o + 0x168))
            tx('empty', e.r32(o + 0x16c))
            rc('esrc', o + 0x170)
            kv('edx', e.r32(o + 0x180))
            kv('edy', e.r32(o + 0x184))

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

        def item(o):
            fonttext(o)
            kv('index', e.r32(o + 0x4b0))
            line.append(f'box={1 if e.r32(o + 0x4b4) else 0}')

        def textbox(o):
            fonttext(o)
            kv('current', e.r32(o + 0x4b0))
            kv('bup', e.r32(o + 0x4b4) & 0xff)
            kv('repos', e.r32(o + 0x4b5) & 0xff)
            first, last = e.r32(o + 0x4bc), e.r32(o + 0x4c0)
            n = (last - first) // 0x4cc if first else 0
            kv('nlines', n)
            for i in range(n):
                line.append(f'\n  line{i}')
                text(first + 0x4cc * i)

        def listbox(o):
            check(o)
            for k, off in (('count', 0x61c), ('before', 0x620), ('spacing', 0x638), ('first', 0x63c)):
                kv(k, e.r32(o + off))
            first, last = e.r32(o + 0x62c), e.r32(o + 0x630)
            kv('nstr', (last - first) // 4 if first else 0)
            for k, off in (('bx0', 0x640), ('by0', 0x644), ('bx1', 0x648), ('by1', 0x64c)):
                kv(k, e.r32(o + off))
            ref('shown', o + 0x164)

        def check(o):
            control(o)
            ref('ctext', e.r32(o + 0x14c))
            kv('checked', e.r32(o + 0x150))
            for k, off in (('idc', 0x154), ('idu', 0x158), ('iun', 0x15c), ('ich', 0x160)):
                tx(k, e.r32(o + off))

        kv('active', e.r32(d + 4))
        kv('cursor', 1 if e.r32(d + 0x10) else 0)
        tx('behind', e.r32(d + 0x130))
        tx('static', e.r32(d + 0x134))
        for k, off in (('ox', 0x10fe0), ('oy', 0x10fe4), ('mute', 0x10fe8), ('nsnd', 0x1c90), ('snext', 0x1c94),
                       ('mx', 0x14), ('my', 0x18), ('ml', 0x44), ('mr', 0x48)):
            kv(k, e.r32(d + off))
        flush('dialog')
        for i in range(10):
            a = d + 0x1c98 + 0x10 * i
            for k, off in (('cd', 0), ('id', 4), ('playing', 8), ('vol', 0xc)):
                kv(k, e.r32(a + off))
            flush(f'sound{i}')
        for i in range(20):
            a = d + 0x1d38 + 0x24 * i
            kv('used', e.r32(a))
            if e.r32(a):
                s('face', e.r32(a + 4))
                for k, off in (('height', 8), ('color', 0xc), ('color2', 0x10), ('mode', 0x14), ('shadow', 0x18),
                               ('align', 0x20)):
                    kv(k, e.r32(a + off))
            kv('weight', e.r32(a + 0x1c))
            flush(f'font{i}')
        c = d + 0x58
        image(c)
        tx('under', e.r32(c + 0xbc))
        kv('capture', e.r32(c + 0xc0))
        flush('cursorobj')
        for i, o in enumerate(objs):
            k = 'U' if o == c else VT_KIND.get(e.r32(o), '?')
            {'T': text, 'C': control, 'K': check, 'L': slider, 'X': listbox, 'Y': item,
             'Z': textbox, 'F': fonttext, 'B': base, 'N': line2d}.get(k, image)(o)
            flush(f'o{i} {k}')
        return '\n'.join(out) + '\n'


# ---- comparison ----

def canonical(trace, dump, stubbed):
    """Texture ids the stubs did not hand out, numbered by first appearance."""
    names = {}

    def name(v):
        if v == 0 or v in stubbed:
            return v
        if v not in names:
            names[v] = 0xc0de0000 + len(names)
        return names[v]
    out = []
    for key, args in trace:
        pos = TEXTURE_ARGS.get(key, ())
        out.append((key, tuple(name(a) if i in pos and not isinstance(a, str) else a for i, a in enumerate(args))))
    import re

    def sub(m):
        return f'T{name(int(m.group(1), 16)):x}'
    return out, re.sub(r'(?<==)T([0-9a-f]+)', sub, dump)


def run_case(seed, data, offset, dialog, plan, no_draw, verbose, path=READER):
    orig = Original(seed, data, offset, dialog, plan, no_draw)
    dump_o = orig.run()
    rep = Replay(orig.t, orig.e, orig.snapshot)
    probe.probe_app_set_callback(ctypes.cast(rep.callback(), ctypes.c_void_p))
    buf = ctypes.create_string_buffer(1 << 22)
    n = probe.probe_twod_run(str(path).encode(), offset, dialog.encode(), no_draw, '\n'.join(plan).encode(),
                             buf, len(buf))
    assert n >= 0, 'probe failed'
    dump_r = buf.value.decode('latin1')
    stubbed = orig.w.stub_textures
    to, do = canonical(orig.t.log, dump_o, stubbed)
    tr, dr = canonical(rep.log, dump_r, stubbed)
    ok = compare(f'{dialog} seed {seed}', to, tr)
    if do != dr:
        ok = False
        lo, lr = do.split('\n'), dr.split('\n')
        for a, b in zip(lo, lr):
            if a != b:
                print(f'{dialog} seed {seed}: dumps differ')
                ta, tb = a.split(' '), b.split(' ')
                print('  original:', ' '.join(x for x in ta if x not in tb)[:400])
                print('  rebuild: ', ' '.join(x for x in tb if x not in ta)[:400])
                print('  in:', a[:120])
                break
        else:
            print(f'{dialog} seed {seed}: dumps differ in length {len(lo)} {len(lr)}')
    if verbose and ok:
        print(f'{dialog} seed {seed}: {len(to)} calls, {dump_o.count(chr(10))} lines')
    return ok, len(to)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    seeds = 3
    for a in sys.argv[1:]:
        if a.startswith('--seeds='):
            seeds = int(a.split('=')[1])
    verbose = '--verbose' in sys.argv
    data = READER.read_bytes()
    cases = []
    for fname, start, length in entries(data):
        if fname not in FILES:
            continue
        tree, _ = parse_node(data, start)
        for top in tree:
            if not isinstance(top, list):
                continue
            for i in range(0, len(top) - 1, 2):
                name, body = top[i], top[i + 1]
                if not isinstance(name, str) or not isinstance(body, list) or \
                        not name.endswith(('DIALOG', 'MENU', 'POPUP', 'BOX')):
                    continue
                if args and not any(a in name or a in fname for a in args):
                    continue
                b = find(body, 'BUTTONS')
                p = find(body, 'PRIMITIVES')
                buttons = [(b[j], b[j + 1]) for j in range(0, len(b) - 1, 2)] if isinstance(b, list) else []
                prims = [(p[j], p[j + 1]) for j in range(0, len(p) - 1, 2)] if isinstance(p, list) else []
                cases.append((fname, start, name, buttons, prims))
    passed = total = calls = synth_runs = 0
    synth = ROOT / 'build' / 'twod_synth.zrd'
    if not args or any(a in 'SYNTH' for a in args):
        for seed in range(seeds * 10):
            tree = synthetic_dialog(seed)
            blob = write_node(tree)
            synth.write_bytes(blob)
            body = tree[0][3]
            b, p = find(body, 'BUTTONS'), find(body, 'PRIMITIVES')
            buttons = [(b[j], b[j + 1]) for j in range(0, len(b) - 1, 2)]
            prims = [(p[j], p[j + 1]) for j in range(0, len(p) - 1, 2)]
            plan = make_plan(seed, 'SYNTH', buttons, prims)
            ok, n = run_case(seed, blob, 0, 'SYNTH', plan, seed % 3 == 2, verbose, synth)
            total += 1
            passed += ok
            calls += n
            synth_runs += 1
    for fname, start, name, buttons, prims in cases:
        for seed in range(seeds):
            plan = make_plan(seed, name, buttons, prims)
            ok, n = run_case(seed, data, start, name, plan, 1 if seed == 2 else 0, verbose)
            total += 1
            passed += ok
            calls += n
    print(f'zTwoD dialogs: {passed}/{total} runs identical ({len(cases)} shipped dialogs x {seeds} seeds, '
          f'{synth_runs} synthetic; {calls} calls)')
    return 0 if passed == total else 1


if __name__ == '__main__':
    sys.exit(main())
