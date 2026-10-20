"""The HUD's construction against the original (docs/hud.md).

The original's constructor 0x43b190 runs in the emulator on a fresh
0x22e40-byte block (zTwoD's outside stubbed by twod_world.py; the set-up
0x43bb80, zRender's nodes, the nav list and the messages stubbed and
recorded), then the rebuild's (probe_hud.cpp) with the original's answers
replayed. Random per run: the render path, the options' display and window
rects, the saved MFD modes, whether messages exist. Compared: the call traces
and a dump of the HUD, all ten pieces and their zTwoD objects (the fields
each constructor sets), and the lights, fonts and damage silhouettes the HUDs
share.

    cmake --build build && .venv/bin/python -P -u tests/differential/test_hud.py [seeds]
"""
import ctypes
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import Replay, Trace, compare, load_probe, rnd  # noqa: E402
from twod_world import IAT, READER, ROOT, TwodWorld, entries, parse_node, write_node  # noqa: E402
from test_twod_msgbox import archive  # noqa: E402
import random  # noqa: E402
from ai_world import FLOOR, IAT_FLOOR  # noqa: E402
import test_twod  # noqa: E402
from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESP  # noqa: E402

probe = load_probe()
probe.probe_hud_build.argtypes = [ctypes.c_uint32, ctypes.c_uint32, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
ZRDR_LOAD = 0x556740
SHIPPED = READER.read_bytes()
HUD_START = next(s for n, s, _ in entries(SHIPPED) if n.lower() == 'hud.zrd')
HUD_TREE, _ = parse_node(SHIPPED, HUD_START)
SYNTH = ROOT / 'build' / 'test_hud.zbd'


def synthetic(seed):
    """The shipped hud.zrd changed: blocks and keys dropped or renamed, odd values."""
    import copy
    r = random.Random(f'hudzrd:{seed}')
    tree = copy.deepcopy(HUD_TREE)
    top = tree[0]
    for _ in range(r.randrange(1, 6)):
        bi = r.randrange(0, len(top), 2)
        what = r.randrange(5)
        if what == 0 and len(top) > 2:
            del top[bi:bi + 2]  # a block gone
            continue
        body = top[bi + 1]
        ki = r.randrange(0, len(body), 2)
        if what == 1:
            del body[ki:ki + 2]  # a piece gone
        elif what == 2:
            body[ki] = body[ki].lower()  # renamed
        else:
            fields = body[ki + 1]
            fi = r.randrange(0, len(fields), 2)
            if what == 3:
                fields[fi + 1] = [r.randrange(-50, 1700) for _ in fields[fi + 1]]
            else:
                del fields[fi:fi + 2]
    for key, n in (('heat_text', 2), ('width', 1)):  # moved everywhere
        if r.random() < 0.4:
            v = [r.randrange(0, 600) for _ in range(n)]
            for i in range(0, len(top), 2):
                b = top[i + 1]
                for j in range(1, len(b), 2):
                    f = b[j]
                    for q in range(0, len(f) - 1, 2):
                        if f[q] == key:
                            f[q + 1] = list(v)
    if r.random() < 0.6:  # MFD and TARGET resized everywhere: the panel's image grows or shrinks
        size = [r.randrange(20, 160), r.randrange(20, 160)]
        if r.random() < 0.3:
            size = [100, 75]  # as many pixels, transposed
        for i in range(0, len(top), 2):
            for j in range(0, len(top[i + 1]), 2):
                if top[i + 1][j] in ('MFD', 'TARGET'):
                    f = top[i + 1][j + 1]
                    for q in range(0, len(f), 2):
                        if f[q] == 'size':
                            f[q + 1] = list(size)
    return tree


HUD_SIZE = 0x22e40
PIECES = {0x14: 'radar', 0xdd4: 'compass', 0x144c: 'reticle', 0x1da8: 'gauges', 0x3434: 'mfd', 0x410c: 'target',
          0x6520: 'weapons', 0x17964: 'objectives', 0x18410: 'timer', 0x188c0: 'equipment'}
DAMAGE = 0x605888


class Original:
    def __init__(self, seed):
        self.t = Trace(seed=seed)
        self.w = TwodWorld(self.t, seed)
        self.e = self.w.emu
        self.seed = seed

    def run(self):
        e, w = self.e, self.w
        r0 = rnd(self.seed, 'setup')
        # hud.zrd: the shipped one, a crafted one, or none.
        kind = r0.choice(['shipped', 'shipped', 'synthetic', 'none'])
        self.path = b''
        tree = 0
        if kind == 'shipped':
            tree = w.read_tree(SHIPPED, HUD_START)
            self.path = str(READER).encode()
        elif kind == 'synthetic':
            blob = write_node(synthetic(self.seed))
            SYNTH.write_bytes(archive([('hud.zrd', blob)]))
            tree = w.read_tree(blob, 0)
            self.path = str(SYNTH).encode()
        e.stub(ZRDR_LOAD, '@zrdr_load', 'c,d', None, lambda emu, key, a, pop: emu._ret(tree, pop))

        def realloc(emu, key, a, pop):
            p, n = a
            q = emu.alloc(max(n, 4))
            if p:
                emu.cpu.write(q, bytes(emu.cpu.read(p, n)))
            emu._ret(q, 0)
        e.import_stub(IAT['realloc'], '@realloc', 's0,s1', 0, realloc)
        box = e.world_alloc(4)
        e.w32(box, r0.choice([3, 5, 6, 7, 8, 9, 10, 3, 5, 0, 4, 11]))
        e.w32(0x5f4d0c, box)
        # The mech's side: dead (vt+0xa0), its kind (0x0041df10), the HUD sound
        # channels, the nav list, the weapon list's refresh.
        e.stub(0x41df10, spec='c')
        w.answers['0x0041df10'] = lambda rr, n, a: rr.choice([0, 1, 2, 2])
        e.stub(0x41e7a0, spec='c,d')
        e.stub(0x47eba0, spec='c,s0')
        e.stub(0x47eb60, spec='c,s0,s1,s2,s3,s4')
        w.answers['mech.vt+0xa0'] = lambda rr, n, a: rr.choice([0, 0, 1])
        r = rnd(self.seed, 'world')
        entry = e.world_alloc(0x100)
        e.cpu.write(entry + 0xd0, struct.pack('<f', r.choice([0.0, 1.0, -1.0])))
        w.answers['0x00491c80'] = lambda rr, n, a: entry if self.flip else 0
        self.flip = r.random() < 0.5
        w.answers['@entry+0xd0'] = struct.unpack('<I', bytes(e.cpu.read(entry + 0xd0, 4)))[0]
        self.e.stub(0x491c80, spec='')
        e.stub(0x446ad0, spec='c')
        e.stub(0x446520, spec='c')
        # zRender's nodes for the zoom views: new ones are fresh handles.
        for a, spec in ((0x4f9e90, ''), (0x4f32e0, ''), (0x4eb300, 'c,zd'), (0x4eb170, 'c,d'), (0x4f9fa0, 'c,d,s0'),
                        (0x4fa0b0, 'c,d,s0'), (0x4f3620, 'c,d'), (0x4f3560, 'c,d'), (0x4f6c80, 'c,*d'),
                        (0x4f3ff0, 'c,s0'), (0x4f3d90, 'c,s0,s1'), (0x4f3a70, 'c,s0,s1')):
            e.stub(a, spec=spec)
        w.answers['0x004f9e90'] = lambda rr, n, a: e.world_alloc(0x40)
        w.answers['0x004f32e0'] = lambda rr, n, a: e.world_alloc(0x40)
        world = e.world_alloc(0x10)
        e.w32(world + 4, e.world_alloc(0x40))
        e.w32(0x608c84, world)
        # The player camera's clip (0x004f3b00): written to *edx and *s0; the
        # answer points the probe at the two.
        e.stub(0x4f3b00, spec='')
        clip = e.world_alloc(8)
        e.cpu.write(clip, struct.pack('<ff', r.choice([0.5, 1.0, 2.0]), r.choice([300.0, 500.0, 1000.0])))

        def player_clip(rr, n, a):
            esp = e.uc.reg_read(UC_X86_REG_ESP)
            e.w32(e.uc.reg_read(UC_X86_REG_EDX), e.r32(clip))
            e.w32(e.r32(esp + 4), e.r32(clip + 4))
            return clip
        w.answers['0x004f3b00'] = player_clip
        # The MFDs: the nav list (0x0047e840, not ported) answers its object;
        # the title by the pilot's game; the last modes.
        e.stub(0x47e840, spec='s0,s1,s2,s3,s4')
        w.answers['0x0047e840'] = lambda rr, n, a: e.uc.reg_read(UC_X86_REG_ECX)
        e.stub(0x479480, spec='')
        w.answers['0x00479480'] = lambda rr, n, a: rr.choice([0, 0x51000000])
        e.stub(0x576040, spec='c')
        titles = {i: e.world_string(f'title {i:x}') for i in (0xdbe, 0xfbb)}
        w.answers['0x00576040'] = lambda rr, n, a: titles.get(a[0], 0)
        e.stub(0x4f34c0, spec='c,d')
        e.w32(0x605988, r.choice([0, 0, 1, 2, 3]))
        e.w32(0x60598c, r.choice([0, 0, 1, 2, 3]))
        # The player view: its camera node, the node's data, the field of view.
        p_cam, node, data = e.world_alloc(8), e.world_alloc(0x40), e.world_alloc(0x100)
        e.w32(0x5f4c8c, p_cam)
        e.w32(p_cam + 4, node)
        e.w32(node + 0x38, data)
        e.wf(data + 0xe8, r.choice([1.0471976, 0.87266463, 1.5707964]))
        # The options' display and window rects (**[0x005f4d20], **[0x005f4d24]).
        for g in (0x5f4d20, 0x5f4d24):
            rect = e.world_alloc(0x18)
            if r.random() < 0.7:
                vals = (0, 0, r.choice([640, 800, 1024, 1600]), r.choice([480, 600, 768, 1200]))
            else:
                vals = tuple(r.randrange(-20, 500) for _ in range(4))
            e.cpu.write(rect, struct.pack('<6i', *vals, vals[2] - vals[0], vals[3] - vals[1]))
            box = e.world_alloc(4)
            e.w32(box, rect)
            e.w32(g, box)
        # Mech3Msg.dll's messages: "msg<id>" into the buffer (s0), or none.
        e.stub(0x575ff0, spec='s1,s2')
        has_msgs = r.random() < 0.8

        def message(rr, n, a):
            if not has_msgs:
                return 0
            t = f'msg{a[1]}'.encode()
            e.cpu.write(e.r32(e.uc.reg_read(UC_X86_REG_ESP) + 4), t + b'\0')
            return len(t)
        w.answers['0x00575ff0'] = message
        e.cpu.import_code(IAT_FLOOR, FLOOR)
        e.cpu.write(0x72f348, struct.pack('<I', r.choice([0, 1])))  # the render path: software, Direct3D
        self.hud = e.world_alloc(HUD_SIZE)
        self.mech = self.fake_mech(r) if r.random() < 0.8 else 0
        self.snapshot = e.snapshot()
        e.call(0x43b190, ecx=self.hud, stack=(self.mech,))
        return self.dump()

    def fake_mech(self, r):
        """The fields the set-up reads: vt+0xa0, +0x6c (empty), +0xbc, the
        weapon manager's count, the parts' equipment counts."""
        e = self.e
        m = e.world_alloc(0x400)
        vt = e.world_alloc(0x100)
        e.w32(m, vt)
        e.w32(vt + 0xa0, e.new_stub('mech.vt+0xa0', 'c', 0))
        # +0x6c: no target (a target's path is TARGET's item, hud.md section 9)
        e.cpu.write(m + 0xbc, bytes([r.randrange(256)]))
        wm = e.world_alloc(0x40)
        e.w32(m + 0xd4, wm)
        e.w32(wm + 0x24, r.randrange(0, 12))
        for i in range(16):
            e.w32(m + 0x3ac + 4 * i, r.choice([0, 1, 2, 3, -1]) & 0xffffffff)
        return m

    def dump(self):
        e, h = self.e, self.hud
        out, line = [], []

        def kv(k, v):
            line.append(f'{k}={v & 0xffffffff:x}')

        def tx(k, v):
            line.append(f'{k}=T{v:x}')

        def s(k, a):
            line.append(f'{k}="' + e.cstr(a).replace(' ', '_') + '"')

        def rc(k, a):
            vals = [struct.unpack('<i', struct.pack('<I', e.r32(a + 4 * i)))[0] for i in range(4)]
            line.append(f'{k}=' + ','.join(str(v) for v in vals))

        def piece(a):
            return PIECES.get(a - h, '?') if a else '0'

        def flush(head):
            out.append(head + ' ' + ' '.join(line) + ' ')
            line.clear()

        def base(o):
            line.append(f'next={"o-1" if e.r32(o + 4) else "o-1"}')
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
                line.append('src=0')
            kv('center', e.r32(o + 0x48))

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

        def ftext(o):
            fonttext(o)
            for k, off in (('font', 0x234), ('centered', 0x238), ('shcol', 0x254), ('lead', 0x478), ('fixed', 0x47c),
                           ('sdx', 0x4a0), ('sdy', 0x4a4)):
                kv(k, e.r32(o + off))
            if e.r32(o + 0x46c) == 2:
                kv('bkc', e.r32(o + 0x470))
            rc('fr', o + 0x480)

        def text(o):
            ftext(o)
            for k, off in (('ft', 0x4b0), ('frate', 0x4b4), ('atop', 0x4b8)):
                kv(k, e.r32(o + off))
            kv('fon', e.r8(o + 0x4c0))
            kv('fmode', e.r32(o + 0x4c4))
            kv('phase', e.r32(o + 0x4c8))

        def textbox(o, head):
            fonttext(o)
            kv('current', e.r32(o + 0x4b0))
            kv('bup', e.r8(o + 0x4b4))
            kv('repos', e.r8(o + 0x4b5))
            first, last = e.r32(o + 0x4bc), e.r32(o + 0x4c0)
            n = (last - first) // 0x4cc if first else 0
            kv('nlines', n)
            flush(head)
            for i in range(n):
                text(first + 0x4cc * i)
                flush(f'{head}.line{i}')

        def panel(o):
            image(o)
            t = e.r32(o + 0x3c)
            if t and t not in self.w.stub_textures:
                n = e.r32(t)
                kv('tn', n)
                kv('twh', e.r32(t + 4))
                kv('tf', e.r8(t + 9))
                kv('tw2', e.r32(t + 0x34))
                kv('tpx', int(e.r32(t + 0x10) == e.r32(o + 0x100)))
                kv('tal', int(e.r32(t + 0x14) == e.r32(o + 0x104)))
                kv('hc', test_twod_fnv(bytes(e.cpu.read(e.r32(o + 0x100), 2 * n))))
                if e.r8(o + 0xfe) < 0xfe:  # else the alpha is never filled
                    kv('ha', test_twod_fnv(bytes(e.cpu.read(e.r32(o + 0x104), n))))
            rc('rect', o + 0xbc)
            for i in range(12):
                kv('c', e.r32(o + 0xcc + 4 * i))
            kv('color', e.r32(o + 0xfc) & 0xffff)
            kv('alpha', e.r8(o + 0xfe))
            kv('colors', int(e.r32(o + 0x100) != 0))
            kv('alphas', int(e.r32(o + 0x104) != 0))

        kv('active', e.r32(h + 4))
        line.append(f'head={piece(e.r32(h + 8))} tail={piece(e.r32(h + 0xc))}')
        kv('mech', e.r32(h + 0x10))
        kv('reader', e.r32(h + 0x22e38))
        kv('block', int(e.r32(h + 0x22e3c) != 0))
        for i in range(12):
            kv('z', e.r32(h + 0x22e08 + 4 * i))
        out.append(' '.join(line) + ' ')
        line.clear()
        for off in (0xdd4, 0x1da8, 0x410c, 0x3434, 0x6520, 0x188c0):
            o = h + off
            out.append(f'link {PIECES[off]} next={piece(e.r32(o + 4))} owner={1 if e.r32(o + 8) == h else 0}')
        c = h + 0xdd4
        base(c)
        kv('scale', e.r32(c + 0x34))
        kv('half', e.r32(c + 0x38))
        kv('centre', e.r32(c + 0x3c))
        rc('srcr', c + 0x40)
        flush('compass')
        image(c + 0x50)
        flush('tape')
        image(c + 0x10c)
        flush('frame')
        fonttext(c + 0x1c8)
        flush('ctext')
        fonttext(h + 0x18410)
        flush('timer')
        o = h + 0x17964
        textbox(o, 'objectives')
        panel(o + 0x4c8)
        flush('opanel')
        textbox(o + 0x5d4, 'scores')
        for off in (0xa9c, 0xaa0, 0xaa4, 0xaa8):
            kv('font', e.r32(o + off))
        flush('ofonts')
        o = h + 0x188c0
        base(o)
        kv('hud', e.r32(o + 0x34))
        kv('below', e.r8(o + 0xa540))
        kv('column', e.r32(o + 0xa544))
        flush('equipment')
        panel(o + 0xa438)
        flush('epanel')
        for i in range(16):
            q = o + 0x38 + 0xa40 * i
            text(q)
            flush(f'eline{i}')
            fonttext(q + 0x4cc)
            flush('value')
            image(q + 0x97c)
            kv('state', e.r32(q + 0xa38))
            kv('item', e.r32(q + 0xa3c))
            flush('light')
        def poly(o):
            base(o)
            for i in range(42):
                kv('p', e.r32(o + 0x34 + 4 * i))
            for k, off in (('count', 0xdc), ('color', 0xe0), ('dl', 0xe4)):
                kv(k, e.r32(o + off))
            kv('tf', e.r8(o + 0xe8))

        o = h + 0x14
        base(o)
        for k, off in (('radius', 0x34), ('rsq', 0x38), ('color', 0x3c)):
            kv(k, e.r32(o + off))
        kv('clipped', e.r8(o + 0x40))
        rc('clip', o + 0x44)
        kv('x54', e.r32(o + 0x54))
        kv('x58', e.r32(o + 0x58) & 0xffff)
        kv('zoomed', e.r32(o + 0x5a) & 0xffff)
        kv('active', e.r8(o + 0x5c))
        kv('changed', e.r8(o + 0x5d))
        for i in range(12):
            kv('v', e.r32(o + 0x60 + 4 * i))
        for i in range(3):
            kv('dir', e.r32(o + 0x90 + 4 * i))
        for i in range(3):
            kv('fov', e.r32(o + 0xa8 + 4 * i))
        for k, off in (('xbd4', 0xbd4), ('xbd8', 0xbd8), ('xdb4', 0xdb4), ('xdb8', 0xdb8), ('src', 0xdbc)):
            kv(k, e.r32(o + off))
        flush('radar')
        for i in range(3):
            q = o + 0xb4 + 0x50 * i
            base(q)
            for j in range(7):
                kv('l', e.r32(q + 0x34 + 4 * j))
            flush(f'rline{i}')
        for nm, off in (('rtext1', 0x1a4), ('rtext2', 0x654)):
            ftext(o + off)
            flush(nm)
        q = o + 0xb04
        image(q)
        for i in range(3):
            tx('l', e.r32(e.r32(q + 0xbc) + 4 * i))
        for k, off in (('count', 0xc0), ('cur', 0xc4), ('xc8', 0xc8), ('xcc', 0xcc)):
            kv(k, e.r32(q + off))
        flush('rlight')
        for i in range(2):
            poly(o + 0xbdc + 0xec * i)
            flush(f'rpoly{i}')
        def zoomview(o, head):
            base(o)
            kv('cam', e.r32(o + 0x38))
            kv('world', e.r32(o + 0x40))
            kv('x10', e.r32(o + 0x44))
            kv('hw', e.r32(o + 0x48))
            kv('x4c', e.r32(o + 0x4c))
            rc('rect', o + 0x50)
            kv('x168', e.r32(o + 0x168))
            flush(head)
            wn = e.r32(o + 0x3c)
            kv('node', e.r32(wn + 4))
            rc('area', wn + 8)
            rc('target', wn + 0x18)
            kv('scaled', e.r8(wn + 0x28))
            if e.r8(wn + 0x28):
                for i in range(4):
                    kv('s', e.r32(wn + 0x2c + 4 * i))
            flush(head + '.window')
            panel(o + 0x60)
            flush(head + '.panel')

        def damageview(o, head):
            base(o)
            rc('rect', o + 0x7d4)
            flush(head)
            image(o + 0x34)
            flush(head + '.bg')
            for i in range(8):
                q = o + 0xf0 + 0xdc * i
                image(q)
                for j in range(6):
                    kv('f', e.r32(q + 0xbc + 4 * j))
                kv('part', (e.r32(q + 0xd8) - DAMAGE) // 0x20)
                flush(f'{head}.part{i}')

        def mfd(o, head):
            base(o)
            kv('x34', e.r32(o + 0x34))
            kv('mode', e.r32(o + 0x38))
            kv('nav', e.r32(o + 0x820))
            kv('dmech', e.r32(o + 0x3c + 0x7d0))
            flush(head)
            damageview(o + 0x3c, head + '.damage')
            v = e.r32(o + 0x824)
            zoomview(v, head + '.view')
            poly(v + 0x16c)
            flush(head + '.box')
            ftext(o + 0x828)
            flush(head + '.title')

        mfd(h + 0x3434, 'mfd')
        o = h + 0x410c
        mfd(o, 'target')

        def shape(q):
            poly(q)
            kv('xf', e.r32(q + 0xec))
            for i in range(13):
                kv('s', e.r32(q + 0xf0 + 4 * i))

        shape(o + 0xcd8)
        flush('tshape1')
        for i in range(3):
            q = o + 0xdfc + 0x4c * i
            base(q)
            for j in range(6):
                kv('l', e.r32(q + 0x34 + 4 * j))
            flush(f'tline{i}')
        shape(o + 0xee0)
        flush('tshape2')
        q = o + 0x1004
        base(q)
        for k, off in (('radius', 0x34), ('rsq', 0x38), ('color', 0x3c)):
            kv(k, e.r32(q + off))
        kv('clipped', e.r8(q + 0x40))
        rc('clip', q + 0x44)
        flush('tframe')
        for i in range(12):
            tx('a', e.r32(o + 0x1058 + 4 * i))
        kv('target', e.r32(o + 0x2404))
        kv('shown', e.r8(o + 0x2408))
        kv('x240c', e.r32(o + 0x240c))
        kv('lh', e.r32(o + 0x2410))
        kv('dmech', e.r32(o + 0x3c + 0x7d0))
        flush('tfields')
        image(o + 0x1088)
        flush('tmarker')
        for i in range(4):
            ftext(o + 0x1144 + 0x4b0 * i)
            flush(f'ttext{i}')
        o = h + 0x144c
        image(o)
        for k, off in (('state', 0x908), ('x950', 0x950), ('x954', 0x954)):
            kv(k, e.r32(o + off))
        kv('locked', e.r8(o + 0x90c))
        kv('src', e.r32(o + 0x958))
        for i in range(5):
            tx('i', e.r32(o + 0x910 + 4 * i))
        for i in range(10):
            kv('z', e.r32(o + 0x928 + 4 * i))
        flush('reticle')
        q = o + 0xbc
        zoomview(q, 'zoom')
        kv('round', e.r8(q + 0x16c))
        kv('zoom', e.r32(q + 0x170))
        kv('x174', e.r8(q + 0x174))
        kv('bw', e.r32(q + 0x178))
        kv('bh', e.r32(q + 0x17c))
        flush('zoomw')
        ftext(q + 0x180)
        flush('ztext')
        image(q + 0x630)
        for i in range(4):
            tx('l', e.r32(e.r32(q + 0x630 + 0xbc) + 4 * i))
        for k, off in (('count', 0xc0), ('cur', 0xc4), ('xc8', 0xc8), ('xcc', 0xcc)):
            kv(k, e.r32(q + 0x630 + off))
        flush('zlight')
        base(q + 0x700)
        for k, off in (('radius', 0x34), ('rsq', 0x38), ('color', 0x3c)):
            kv(k, e.r32(q + 0x700 + off))
        kv('clipped', e.r8(q + 0x740))
        rc('clip', q + 0x744)
        flush('zframe')
        poly(q + 0x754)
        flush('zbox')
        o = h + 0x1da8
        base(o)
        for k, off in (('x143c', 0x143c), ('x1440', 0x1440), ('x1444', 0x1444), ('hw', 0x1448), ('hh', 0x144c),
                       ('hb', 0x150c), ('ty', 0x1688)):
            kv(k, e.r32(o + off))
        flush('gauges')
        for nm, off in (('coolant', 0x38), ('fuel', 0x4e8), ('speed', 0x998)):
            ftext(o + off)
            flush(nm)
        for nm, off in (('sbar', 0xe48), ('fbar', 0xf88), ('cbar', 0x10c8)):
            base(o + off)
            for i in range(63):
                kv('p', e.r32(o + off + 0x34 + 4 * i))
            for k, d in (('count', 0x130), ('height', 0x138), ('width', 0x13c)):
                kv(k, e.r32(o + off + d))
            if nm != 'sbar':
                kv('color', e.r32(o + off + 0x134))
            flush(nm)
        for nm, off in (('sframe', 0x1208), ('cframe', 0x12c4), ('fframe', 0x1380), ('hbar', 0x1450),
                        ('hmeter', 0x1510), ('toggler', 0x15cc)):
            image(o + off)
            flush(nm)
        o = h + 0x6520
        base(o)
        kv('hud', e.r32(o + 0x34))
        kv('x38', e.r32(o + 0x38))
        tx('select', e.r32(o + 0x128))
        tx('chain', e.r32(o + 0x12c))
        kv('shown', e.r8(o + 0x11438))
        kv('x1143c', e.r32(o + 0x1143c))
        kv('column', e.r32(o + 0x11440))
        flush('weapons')
        base(o + 0x3c)
        for i in range(42):
            kv('p', e.r32(o + 0x3c + 0x34 + 4 * i))
        for k, off in (('count', 0xdc), ('color', 0xe0), ('dl', 0xe4)):
            kv(k, e.r32(o + 0x3c + off))
        kv('tf', e.r8(o + 0x3c + 0xe8))
        flush('wpoly')
        panel(o + 0x11330)
        flush('wpanel')
        for i in range(16):
            q = o + 0x130 + 0x1120 * i
            image(q)
            for k, off in (('bx', 0xbc), ('by', 0xc0), ('en', 0xc4), ('value', 0x14c)):
                kv(k, e.r32(q + off))
            rc('bounds', q + 0xcc)
            tx('fill', e.r32(q + 0x150))
            tx('empty', e.r32(q + 0x16c))
            kv('count', e.r32(q + 0x1110))
            kv('red', e.r8(q + 0x1118))
            kv('changed', e.r8(q + 0x111a))
            kv('x111b', e.r8(q + 0x111b))
            kv('weapon', e.r32(q + 0x111c))
            flush(f'wline{i}')
            for nm, off in (('name', 0x188), ('text2', 0x638), ('ctext', 0xae8)):
                fonttext(q + off)
                flush(nm)
            for nm, off in (('light', 0xf98), ('mark', 0x1054)):
                image(q + off)
                flush(nm)
        for i in range(3):
            tx('light', e.r32(0x6059b0 + 4 * i))
        for i in range(4):
            kv('font', e.r32(0x6059c0 + 4 * i))
        flush('wset')
        for i in range(3):
            tx('light', e.r32(0x6059d0 + 4 * i))
        for i in range(4):
            kv('font', e.r32(0x6059e0 + 4 * i))
        flush('eset')
        kv('loaded', e.r8(0x605990))
        kv('bgw', e.r32(0x605998))
        kv('bgh', e.r32(0x60599c))
        for i in range(3):
            tx('bg', e.r32(0x6059a0 + 4 * i))
        for p in range(8):
            for i in range(5):
                tx('i', e.r32(DAMAGE + 0x20 * p + 4 * i))
            kv('px', e.r32(DAMAGE + 0x20 * p + 0x14))
            kv('py', e.r32(DAMAGE + 0x20 * p + 0x18))
        out.append(' '.join(line) + ' ')
        return '\n'.join(out) + '\n'


def test_twod_fnv(b):
    v = 2166136261
    for c in b:
        v = ((v ^ c) * 16777619) & 0xffffffff
    return v


def strip_next(dump):
    """The objects' own next fields are links inside groups (test_twod refs); the
    pieces' links are compared by the 'link' lines."""
    import re
    return re.sub(r'next=o-?\d+ ', '', dump)


def run_case(seed, verbose=False):
    orig = Original(seed)
    dump_o = orig.run()
    rep = Replay(orig.t, orig.e, orig.snapshot)
    probe.probe_app_set_callback(ctypes.cast(rep.callback(), ctypes.c_void_p))
    buf = ctypes.create_string_buffer(1 << 20)
    n = probe.probe_hud_build(orig.hud, orig.mech, orig.path, buf, len(buf))
    assert n >= 0, 'probe failed'
    dump_r = buf.value.decode('latin1')
    stubbed = orig.w.stub_textures
    to, do = test_twod.canonical(orig.t.log, strip_next(dump_o), stubbed)
    tr, dr = test_twod.canonical(rep.log, strip_next(dump_r), stubbed)
    ok = compare(f'seed {seed}', to, tr)
    if ok and do != dr:
        ok = False
        for a, b in zip(do.split('\n'), dr.split('\n')):
            if a != b:
                ta, tb = a.split(' '), b.split(' ')
                print(f'seed {seed}: dumps differ in "{a[:40]}"')
                print('  original:', ' '.join(x for x in ta if x not in tb)[:400])
                print('  rebuild: ', ' '.join(x for x in tb if x not in ta)[:400])
                break
    return ok, len(to)


def main():
    seeds = int(sys.argv[1]) if len(sys.argv) > 1 else 20
    calls = 0
    for seed in range(seeds):
        ok, n = run_case(seed)
        if not ok:
            sys.stdout.flush()
            os._exit(1)
        calls += n
    print(f'{seeds} HUDs built, {calls} calls: the rebuilt HUD matches the original')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    try:
        main()
    except BaseException as ex:  # an emulator fault: print it, then leave without Unicorn's teardown
        import traceback
        traceback.print_exc()
        sys.stdout.flush()
        os._exit(1)
