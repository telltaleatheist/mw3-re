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
import math
import os
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import Replay, Trace, compare, load_probe, rnd  # noqa: E402
from twod_world import IAT, READER, ROOT, TwodWorld, c_format, entries, parse_node, write_node  # noqa: E402
from test_twod_msgbox import archive  # noqa: E402
import random  # noqa: E402
from ai_world import FLOOR, FTOL, IAT_FLOOR, IAT_FTOL  # noqa: E402
import test_twod  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_EIP, UC_X86_REG_ESP  # noqa: E402

probe = load_probe()
probe.probe_hud_entity_class.argtypes = [ctypes.c_uint32, ctypes.c_int32]
probe.probe_hud_free_after.argtypes = [ctypes.c_int32]
probe.probe_hud_build.argtypes = [ctypes.c_uint32, ctypes.c_uint32, ctypes.c_char_p, ctypes.POINTER(ctypes.c_float),
                                  ctypes.c_int, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p, ctypes.c_int,
                                  ctypes.c_char_p, ctypes.c_int]
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


# The test's changes between frames (probe_hud.cpp applies the same): 0 a
# frame (dt), 1 the HUD's set_active, 2 the zoom window shown or hidden, 3 the
# reticle's state, 4 the zoom light's interval, 5 an MFD damage part's flash
# (part | on << 8 | mode << 16), 6 the MFD's mode, 7 a piece shown or hidden
# (its offset), 8 the zoom window round, 9 TARGET's slot (the mech's own
# +0x6c, a contact's, an empty one; never null), 10 TARGET's mode, 11 a
# child's flags XORed (CHILDREN index | bits << 16),
# 12 a weapon line's count (line | count << 8) and changed, 13 every child
# dirty (kDirty), 14 every child and piece shown, 15 a text set (FTEXTS
# index), 16 an image set (IMAGES index | shared light << 8: the weapon
# list's three, then the equipment's), 17 the Direct3D pass 0x0043c6b0 (dt),
# 18 TARGET's tracking 0x00440b00 (dt), 19 the lead point 0x0041d940 on the
# lead mech (its address), 20 the HUD's data update 0x0043c390 (dt), 21 the
# radar's polylines' counts (first | second << 8) and lists, 22 the radar's
# map and changed flags (map | changed << 1), 23 and 24 its lists (addresses),
# 33 the zoom window's show_for (0x004438d0, seconds), the HUD keys
# (Mech::command's): 34 keypad 0's lock (0x00442e00), 35 R's radar size
# (0x0043da40), 36 D's MFD mode (0x0043d240), 37 Ctrl+D's TARGET mode
# (0x0043d250), 38 Shift+R's radar map (0x0043dc10).
OPS = list(range(1, 20))
# The message panel's messages and times (ops 27, 28; probe_hud.cpp the same).
MESSAGES = ['Objective complete', 'Nav Alpha reached', 'Lancemate destroyed: the rest of the lance falls back']
MESSAGE_TIMES = [0.0, 0.5, 2.0, 6.0]
# The static's add (0x0043bfe0, op 40): steps, holds and tops by index.
STATIC_NULL = [0, 0, 0, 0]  # TARGET's null reads skipped (FIX (bug)), static fills, zoom renders, zoom mask fans
STATIC_STEP, STATIC_HOLD, STATIC_MOST = [0.1, 0.05, 0.5, 1.0], [0.0, 0.016, 0.3, 2.0], [0.1, 0.5, 1.0, 1.25]
FTEXTS = ([0x1da8 + o for o in (0x38, 0x4e8, 0x998)]
          + [0x188c0 + 0x38 + 0xa40 * i + o for i in range(4) for o in (0, 0x4cc)]
          + [0x6520 + 0x130 + 0x1120 * i + 0xae8 for i in range(4)]
          + [0x3434 + 0x828] + [0x410c + 0x1144 + 0x4b0 * k for k in range(4)] + [0x1508 + 0x180])
IMAGES = ([0x188c0 + 0x38 + 0xa40 * i + 0x97c for i in range(4)]
          + [0x6520 + 0x130 + 0x1120 * i + o for i in range(4) for o in (0xf98, 0x1054)] + [0x410c + 0x1088])
# The pieces' children by offset in the HUD ('view': the MFD's 3D view, new'd);
# probe_hud.cpp's child() lists the same, in the same order.
CHILDREN = ([0x1da8 + o for o in (0x38, 0x4e8, 0x998, 0xe48, 0xf88, 0x10c8, 0x1208, 0x12c4, 0x1380, 0x1450, 0x1510,
                                  0x15cc)]
            + [0xdd4 + 0x50, 0xdd4 + 0x10c, 0x17964 + 0x4c8, 0x17964 + 0x5d4, 0x188c0 + 0xa438]
            + [0x188c0 + 0x38 + 0xa40 * i + o for i in range(16) for o in (0, 0x4cc, 0x97c)]
            + [0x6520 + 0x11330, 0x6520 + 0x3c]
            + [0x6520 + 0x130 + 0x1120 * i + o for i in range(16) for o in (0, 0xae8, 0xf98, 0x1054)]
            + [0x3434 + 0x828, 0x3434 + 0x3c, 0x3434 + 0x3c + 0x34]
            + [0x3434 + 0x3c + 0xf0 + 0xdc * i for i in range(8)]
            + [0x410c + o for o in (0xdfc, 0xe48, 0xe94, 0x1144, 0x15f4, 0x1aa4, 0x1f54, 0x1088, 0x1004)]
            + [0x1508 + o for o in (0x180, 0x630, 0x700, 0x754)] + ['view'])
FOCUS = [CHILDREN.index('view'), CHILDREN.index(0x17964 + 0x5d4)] + [CHILDREN.index(0x188c0 + 0x38 + 0xa40 * i)
                                                                    for i in range(4)]
TOGGLED = [0xdd4, 0x1da8, 0x3434, 0x410c, 0x6520, 0x17964, 0x188c0, 0x144c, 0x14]
CLIP = (0.0, 0.0, 0.5, 640.0, 480.0, 1000.0)  # zRender's clip volume, both sides


def f32(v):
    return struct.unpack('<I', struct.pack('<f', v))[0]


HUD_SIZE = 0x22e40
# The entity classes' vtables (mech3/entity_classes: EntityClass's values),
# for contacts whose objects carry a real one (the radar's and TARGET's).
CLASSED = [(0x5bcef8, 1), (0x5c3508, 2), (0x5c23d8, 3), (0x5c8460, 3), (0x5c88e8, 3), (0x5c8d48, 4),
           (0x5c8770, 5), (0x5ccb90, 6), (0x5ccc80, 7), (0x5cced8, 8), (0x5ccfe0, 9)]
CLASSED_TARGET = [c for c in CLASSED if c[1] in (4, 6, 7, 8, 9)]  # their vt+0x68 is empty (0x4a5bc0)
BLIP_TABLES = (0x605618, 0x60b120, 0x609a38, 0x60b4f0)
# The classed objects' slot functions the cases must run: the blips of each
# table and none, the target's hidden test, the centre, the names.
CLASS_SLOTS = {0x41ec60: 'a mech blip', 0x45b970: 'a vehicle blip', 0x4a5fc0: 'a turret blip',
               0x4d2d70: 'a target blip', 0x410510: 'no blip (field base, markers)', 0x4d3350: 'a target hidden or not',
               0x4d84f0: 'a target centre', 0x4d3330: 'a target name', 0x4aee70: 'a field base name',
               0x5a82d0: 'zEntity velocity'}
CLASS_REACHED = set()
PIECES = {0x14: 'radar', 0xdd4: 'compass', 0x144c: 'reticle', 0x1da8: 'gauges', 0x3434: 'mfd', 0x410c: 'target',
          0x6520: 'weapons', 0x17964: 'objectives', 0x18410: 'timer', 0x188c0: 'equipment'}
DAMAGE = 0x605888


class MsvcRand:
    """MSVCRT's rand from seed 1 (the port's crt::msvc_rand after msvc_srand(1))."""

    def __init__(self, seed):
        self.state = seed

    def __call__(self):
        self.state = (self.state * 214013 + 2531011) & 0xffffffff
        return (self.state >> 16) & 0x7fff


class Original:
    def __init__(self, seed):
        self.t = Trace(seed=seed)
        self.w = TwodWorld(self.t, seed)
        self.e = self.w.emu
        self.seed = seed

    def run(self):
        e, w, t = self.e, self.w, self.t
        for addr in CLASS_SLOTS:  # the classed objects' real slots run (coverage)
            e.uc.hook_add(UC_HOOK_CODE, lambda uc, ad, size, user: CLASS_REACHED.add(ad), begin=addr, end=addr)
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
        entry = e.object('map', size=0x100, slots={0x24: ('c,s0', 4)})  # the mission map (the radar's, part 1)
        e.cpu.write(entry + 0xd0, struct.pack('<f', r.choice([0.0, 1.0, -1.0])))
        e.cpu.write(entry + 0xd4, struct.pack('<f', r.choice([0.05, 0.1, -0.08, 0.0])))  # its scale (0: no radius, hidden)
        mrect = e.world_alloc(16)
        e.cpu.write(mrect, struct.pack('<4i', 40, 30, 600, 450))
        e.w32(entry + 0xd8, mrect)  # (the radar's light test reads it unchecked in map mode)
        w.answers['0x00491c80'] = lambda rr, n, a: entry if self.flip else 0
        self.flip = r.random() < 0.5
        w.answers['@entry+0xd0'] = struct.unpack('<I', bytes(e.cpu.read(entry + 0xd0, 4)))[0]
        self.e.stub(0x491c80, spec='')
        e.stub(0x446ad0, spec='c')
        # zRender's nodes for the zoom views: new ones are fresh handles.
        for a, spec in ((0x4f9e90, ''), (0x4f32e0, ''), (0x4eb300, 'c,zd'), (0x4eb170, 'c,d'), (0x4f9fa0, 'c,d,s0'),
                        (0x4fa0b0, 'c,d,s0'), (0x4f3620, 'c,d'), (0x4f3560, 'c,d'), (0x4f6c80, 'c,*d'),
                        (0x4f3ff0, 'c,s0'), (0x4f3d90, 'c,s0,s1'), (0x4f3a70, 'c,s0,s1')):
            e.stub(a, spec=spec)
        w.answers['0x004f9e90'] = lambda rr, n, a: e.world_alloc(0x40)
        def new_camera(rr, n, a):  # a camera node with its data (+0x38), its place at +0x14
            c = e.world_alloc(0x40)
            e.w32(c + 0x38, e.world_alloc(0x200))
            return c
        w.answers['0x004f32e0'] = new_camera
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
        nav_vt = e.world_alloc(0x40)
        e.w32(nav_vt, e.new_stub('nav.vt+0', 'c,s0', 4))  # its deleting destructor (the MFD's 0x4422b0)

        def nav_new(rr, n, a):
            obj = e.uc.reg_read(UC_X86_REG_ECX)
            e.w32(obj, nav_vt)
            return obj
        w.answers['0x0047e840'] = nav_new
        e.stub(0x479480, spec='')
        w.answers['0x00479480'] = lambda rr, n, a: rr.choice([0, 0x51000000])
        e.stub(0x576040, spec='c')
        titles = {i: e.world_string(f'title {i:x}') for i in (0xdbe, 0xfbb)}
        titles[0x1d7] = e.world_string('km/h')  # the speed's unit (0x004453a0)
        w.answers['0x00576040'] = lambda rr, n, a: titles.get(a[0], 0)
        e.stub(0x4f34c0, spec='c,d')
        e.w32(0x605988, r.choice([0, 0, 1, 2, 3]))
        # R's zoom sound on the source HUD's mech (0x00432990: 0x10 in, 0x11 out),
        # Shift+R's mission maps (one only where the radar has its map).
        e.stub(0x432990, '0x0043da50.sound', 's0', 0x14,
               lambda emu, key, a, pop: emu._ret(emu.t.handle(key, [self.hud, int(a[0] == 0x10)]), pop))

        def map_find(emu, key, a, pop):
            emu._ret(emu.t.handle(key, [emu.cstr(a[0]) if a[0] else '']), pop)
        e.stub(0x458590, '0x00458590', 'c', None, map_find)
        w.answers['0x00458590'] = lambda rr, n, a: rr.choice([0, entry, entry]) if self.flip else 0
        e.w32(0x60598c, r.choice([0, 1, 2, 2, 3]))  # TARGET's (2: its damage view)
        # The player view: its camera node, the node's data, the field of view.
        p_cam, node, data = e.world_alloc(0x80), e.world_alloc(0x40), e.world_alloc(0x200)  # (to its zone, +0x1e4)
        e.w32(0x5f4c8c, p_cam)  # (+0x64 no view target: 0x0040f8d0's HUD part skipped)
        e.w32(p_cam + 4, node)
        e.w32(node + 0x38, data)
        e.wf(data + 0xe8, r.choice([1.0471976, 0.87266463, 1.5707964]))
        for k in range(3):  # its place (the zoom camera's, 0x004436c0) and zone
            e.wf(data + 0x14 + 4 * k, r.uniform(-500, 500))
        e.w32(data + 0x1e4, r.choice([0xffffff01, 0x00000302, 0xff]))
        # The options' display and window rects (**[0x005f4d20], **[0x005f4d24]).
        e.w32(0x5f4d4c, r.choice([0, 0, 1]))  # pixels doubled (VMode 3)
        for g in (0x5f4d1c, 0x5f4d20, 0x5f4d24):
            rect = e.world_alloc(0x20)
            if r.random() < 0.7:
                vals = (0, 0, r.choice([640, 800, 1024, 1600]), r.choice([480, 600, 768, 1200]))
            else:
                vals = tuple(r.randrange(-20, 500) for _ in range(4))
            e.cpu.write(rect, struct.pack('<8i', *vals, vals[2] - vals[0], vals[3] - vals[1], vals[2] - 1, vals[3] - 1))
            box = e.world_alloc(4)
            e.w32(box, rect)
            e.w32(g, box)
        # Mech3Msg.dll's messages: "msg<id>" into the buffer (s0), or none.
        def msg(emu, key, a, pop):  # the radar marker's (0x228, 0x229) arguments logged too
            buf, size, mid, va = a
            got = t.handle(key, (size, mid))
            if mid == 0x228:
                text = emu.cstr(emu.r32(va + 4))
                t.handle(key + '.args', (emu.r32(va), len(text)))
                for ch in text.encode('latin1'):
                    t.handle(key + '.ch', (ch,))
            elif mid == 0x229:
                t.handle(key + '.args', (emu.r32(va), emu.r32(va + 4)))
            elif mid in (0x22d, 0x22e):  # the zoom's text: the name (0x22d), the distance
                for k in range(2 if mid == 0x22d else 1):
                    for ch in emu.cstr(emu.r32(va + 4 * k)).encode('latin1'):
                        t.handle(key + '.ch', (ch,))
            emu._ret(got, pop)
        e.stub(0x575ff0, '0x00575ff0', 's0,s1,s2,s3', None, msg)
        has_msgs = r.random() < 0.8

        def message(rr, n, a):
            if not has_msgs:
                return 0
            t = f'msg{a[1]}'.encode()
            e.cpu.write(e.r32(e.uc.reg_read(UC_X86_REG_ESP) + 4), t + b'\0')
            return len(t)
        w.answers['0x00575ff0'] = message
        e.cpu.import_code(IAT_FLOOR, FLOOR)
        e.cpu.import_code(0x5bb61c, FLOOR.replace(bytes.fromhex('660d0004'), bytes.fromhex('660d0008')))  # ceil: rounding up
        e.cpu.import_code(IAT_FTOL, FTOL)
        e.cpu.write(0x72f348, struct.pack('<I', r.choice([0, 1])))  # the render path: software, Direct3D
        self.hud = e.world_alloc(HUD_SIZE)
        self.mech = self.fake_mech(r) if r.random() < 0.8 else 0
        self.aim_world(r)
        self.frame_world(rnd(self.seed, 'frame world'))
        self.snapshot = e.snapshot()
        e.call(0x43b190, ecx=self.hud, stack=(self.mech,))
        e.call(0x40f410)  # the message panel
        # The reticle placed where the player looks (0x004445f0), a few times.
        self.aim_lines = []
        if self.mech:
            for fx, fy in self.places:
                e.call(0x4445f0, ecx=self.hud + 0x144c, stack=(struct.unpack('<I', struct.pack('<f', fx))[0],
                                                             struct.unpack('<I', struct.pack('<f', fy))[0]))
                self.aim_lines.append(self.aim_dump())
        self.run_frames()
        d = self.dump()
        # The mission's end (item 9): the HUD's destructor 0x43b860, after the dump.
        self.free = rnd(self.seed, 'free').random() < 0.5
        if self.free:
            e.call(0x43b860, ecx=self.hud)
        return d

    def frame_world(self, r):
        """The HUD frame's outside (0x0043c820): the unported pieces stubbed, the
        flags it reads, zRender's 2D draws stubbed and logged."""
        e, w, t = self.e, self.w, self.t
        e.w32(0x5fc384, 0x10)  # the debug text hidden: its overlays not run
        e.w32(0x609980, 0)  # the network's radar marker (part 2) not set up yet
        lhead = e.world_alloc(16)  # the weapons' radar list (0x0060b22c): empty
        e.w32(lhead, lhead)
        e.w32(lhead + 4, lhead)
        e.w32(0x60b22c, lhead)
        e.cpu.write(0x609994, bytes([r.choice([0, 1])]))
        # the message panel (made after the HUD, 0x0040f410; hud.md section 18): its messages
        e.w32(0x603d7c, 0)
        self.msg_fmt = e.world_string('%s')
        self.msgs = [e.world_string(m) for m in MESSAGES]
        # the equipment's item names (0x00484980)
        inames = [e.world_string(x) for x in ('ECM', 'Beagle', 'AMS')]
        e.stub(0x484980, spec='c')
        w.answers['0x00484980'] = lambda rr, n, a: inames[a[0] % 3]
        e.w32(0x605f88, r.choice([0, 1]))
        for a, spec in ((0x479ed0, ''),):
            e.stub(a, spec=spec)

        # The aim state 0x00443d20 runs; its outside stubbed (hud.md section 12):
        # the weapon group (float range on the x87 stack), the contact's mech
        # and distance, the HUD sounds, the aimed node's object.
        fbuf = e.world_alloc(8)
        fthunk = e.thunk(b'\xd9\x05' + struct.pack('<I', fbuf) + b'\xc3')

        def fret(emu, key, a, pop):
            emu.w32(fbuf, t.handle(key, tuple(a)))
            emu._ret(0, 0, then=fthunk)
        e.stub(0x4b1bd0, '0x004b1bd0', 'c', None, fret)
        e.stub(0x4e7030, '0x004e7030', 'c', None, fret)
        w.answers['0x004b1bd0'] = lambda rr, n, a: f32(rr.choice([0.0, 60.0, 250.0, 900.0, 900.0, 900.0]))
        w.answers['0x004e7030'] = lambda rr, n, a: f32(rr.choice([10.0, 200.0, 700.0, 999.6, 1000.0, 1500.0, 2345.6]))
        for a_, spec in ((0x4b1c70, 'c'), (0x4b1cd0, 'c'), (0x4e6ff0, 'c'), (0x41e800, 'c'), (0x41e770, 'c')):
            e.stub(a_, spec=spec)
        mechs = []
        self.target_mech = 0
        # TARGET's text (0x00441850): a mech's network flag and name, its
        # pilot, the names by kind and part, the player's saved or ally contact.
        cvt = e.world_alloc(0x100)
        e.w32(cvt + 0x5c, e.new_stub('cmech.vt+0x5c', 'c', 0))
        e.w32(cvt + 0xa4, e.new_stub('cmech.vt+0xa4', 'c', 0))
        cnames = [e.world_string(x) for x in ('Owens', 'Mad Cat', 'notmech', 'Left Arm', 'Kerensky')]
        w.answers['cmech.vt+0x5c'] = lambda rr, n, a: rr.choice([0, 0, 1])
        w.answers['cmech.vt+0xa4'] = lambda rr, n, a: rr.choice([cnames[0], cnames[4], 0])
        for a_, spec in ((0x484670, 'c'), (0x484d70, 'c'), (0x41d510, 'c,s0'), (0x4d7240, 'c')):
            e.stub(a_, spec=spec)
        w.answers['0x00484670'] = lambda rr, n, a: rr.choice(cnames[:3])
        w.answers['0x00484d70'] = cnames[3]
        w.answers['0x0041d510'] = lambda rr, n, a: rr.choice([0, 1])
        ally_c = e.world_alloc(0x40)  # its mech from 0x004e6ff0, as any contact's
        w.answers['0x004d7240'] = lambda rr, n, a: rr.choice([0, ally_c, ally_c])

        def msg_arg(emu, key, a, pop):  # 0x00575fd0: the message with one name, "msg<id>:<name>"
            buf, size, mid, arg = a
            name = emu.cstr(arg)
            t.handle(key, (size, mid))
            for ch in name.encode('latin1'):
                t.handle(key + '.ch', (ch,))
            emu.cpu.write(buf, f'msg{mid}:{name}'.encode('latin1') + b'\0')
            emu._ret(1, pop)
        e.stub(0x575fd0, '0x00575fd0', 's0,s1,s2,s3', None, msg_arg)
        # MFC's CString (0x00441850's texts), untraced: a pointer to a copy of the text.
        def cs_new(text):
            b = e.world_alloc(0x100)
            e.cpu.write(b, text.encode('latin1')[:0xff] + b'\0')
            return b

        def cs_text(emu, cs):
            return emu.cstr(emu.r32(cs))

        def cs_format(emu, key, a, pop):  # cdecl (this, format, ...): %s (none: "(null)") and %d
            esp = emu.uc.reg_read(UC_X86_REG_ESP)
            cs, fmt = emu.r32(esp + 4), emu.cstr(emu.r32(esp + 8))
            out, i, k = '', 0, 0
            while i < len(fmt):
                if fmt[i] == '%' and i + 1 < len(fmt):
                    v = emu.r32(esp + 12 + 4 * k)
                    k += 1
                    out += (emu.cstr(v) if v else '(null)') if fmt[i + 1] == 's' else str(struct.unpack('<i', struct.pack('<I', v))[0])
                    i += 2
                else:
                    out += fmt[i]
                    i += 1
            emu.w32(cs, cs_new(out))
            emu._ret(0, 0)
        empty = cs_new('')
        e.stub(0x5abdb0, 'cs.ctor', 'c', 0, lambda emu, key, a, pop: (emu.w32(a[0], empty), emu._ret(a[0], 0)))
        e.stub(0x5abda4, 'cs.dtor', 'c', 0, lambda emu, key, a, pop: emu._ret(0, 0))
        e.stub(0x5abdaa, 'cs.format', '', 0, cs_format)
        e.stub(0x5abdbc, 'cs.set', 'c,s0', 4,
               lambda emu, key, a, pop: (emu.w32(a[0], cs_new(emu.cstr(a[1]))), emu._ret(a[0], 4)))
        e.stub(0x5abe4c, 'cs.add', 'c,s0', 4,
               lambda emu, key, a, pop: (emu.w32(a[0], cs_new(cs_text(emu, a[0]) + cs_text(emu, a[1]))), emu._ret(a[0], 4)))
        for _ in range(2):
            m = e.world_alloc(0x600)
            e.w32(m, cvt)
            pilot = e.world_alloc(0x20)
            e.w32(pilot + 0x18, r.choice([cnames[4], cnames[0]]))
            e.w32(m + 0x3ec, pilot)
            self.give_parts(m)
            e.cpu.write(m + 0x570, struct.pack('<f', r.choice([0.0, 0.0, 12.0])))  # the NARC timer (0x00440a30)
            kind = e.world_alloc(0x20)
            e.w32(kind + 0xc, r.choice([7, 2, 1, 1]))
            e.w32(m + 0x684, kind)
            e.w32(m + 0x3cc, r.choice([0, 0, 2]))
            mechs.append(m)
            self.target_mech = self.target_mech or m
        self.cmechs = mechs
        w.answers['0x004b1c70'] = lambda rr, n, a: rr.choice([0, 1, 1, 1, 1, 1])
        w.answers['0x004b1cd0'] = lambda rr, n, a: rr.choice([0, 1, 1, 1, 1, 1])
        contact_mechs = {}  # one answer per contact for the run (the original asks twice in a row)

        def contact_mech(rr, n, a):
            if a[0] not in contact_mechs:
                contact_mechs[a[0]] = rr.choice([0, mechs[0], mechs[1]])
            return contact_mechs[a[0]]
        w.answers['0x004e6ff0'] = contact_mech
        w.answers['0x0041e800'] = lambda rr, n, a: rr.choice([0, 1])
        objs = []
        ovt = e.world_alloc(0x100)  # vt+0xa4 their names (the zoom's text, 0x004436c0): none
        e.w32(ovt + 0xa4, e.new_stub('obj.vt+0xa4', 'c', 0))
        for _ in range(2):
            o = e.world_alloc(0x20)
            e.w32(o, ovt)
            e.cpu.write(o + 8, bytes([r.choice([0, 1])]))
            objs.append(o)

        def node_object(emu, key, a, pop):  # 0x0040eee0: ECX points at the node
            emu._ret(t.handle('0x0040e920', (emu.r32(a[0]),)), pop)
        e.stub(0x40eee0, '0x0040e920', 'c', None, node_object)
        # the same node, the same object (the state asks twice)
        w.answers['0x0040e920'] = lambda rr, n, a: [0, objs[0], objs[1]][(a[0] >> 8) % 3]
        # The Direct3D pass's render side (hud.md section 11).

        def dirty(emu, key, a, pop):
            t.handle(key, tuple(rect(a[0])) if a[0] else (0, 0, 0, 0))
            emu._ret(0, pop)

        def flat(emu, key, a, pop):
            colour, alpha, *rest = a
            v = [emu.r32(0x72f41c + 0x20 * i + 4 * k) for i in range(4) for k in range(3)]
            t.handle('0x0057bf80', (colour, alpha, *v))
            emu._ret(0, pop)
        e.stub(0x576ce0, '0x00576ce0', 'c', None, dirty)
        # The static (0x0043cd20, hud.md section 21): the surface's getters,
        # the finish target and the fill.
        for a in (0x576ca0, 0x576cb0, 0x576cc0, 0x576cd0, 0x576d80, 0x576d90, 0x576da0, 0x576db0):
            e.stub(a, '@surface', '', None, lambda emu, key, a, pop: emu._ret(0, pop))
        e.stub(0x558290, '0x00558290', '', None, lambda emu, key, a, pop: emu._ret(t.handle(key, ()), pop))

        def static_fill(emu, key, a, pop):
            STATIC_NULL[1] += 1
            q = rect(a[0]) if a[0] else (0, 0, 0, 0)
            t.handle(key, (1 if a[0] else 0, *q, a[2], a[3], a[1] & 0xffff, a[4] & 0xffff, a[5] & 0xffff))
            emu._ret(0, pop)
        e.stub(0x558c10, '0x00558c10', 'c,d,s0,s1,s2,s3', None, static_fill)
        # The zoom's 3D pass (0x00442f50, hud.md section 23): its camera's
        # window rect, the clear fans, its zView's render, the sprintf.
        e.stub(0x4f3630, '0x004f3630', 'c')
        w.answers['0x004f3630'] = lambda rr, n, a: 0x7700 + a[0] % 0x100

        def win_pair(k1, k2):
            def fn(emu, key, a, pop):
                p, q = t.handle(k1, (a[0],)), t.handle(k2, (a[0],))
                emu.w32(a[1], p)
                emu.w32(a[2], q)
                emu._ret(0, pop)
            return fn
        e.stub(0x4fa130, '@win.xy', 'c,d,s0', None, win_pair('0x004fa130.x', '0x004fa130.y'))
        e.stub(0x4fa020, '@win.wh', 'c,d,s0', None, win_pair('0x004fa020.w', '0x004fa020.h'))
        w.answers['0x004fa130.x'] = lambda rr, n, a: rr.choice([10, 0x172, rr.randrange(0, 600)])
        w.answers['0x004fa130.y'] = lambda rr, n, a: rr.choice([10, 0x5a, rr.randrange(0, 400)])
        w.answers['0x004fa020.w'] = lambda rr, n, a: rr.choice([0x5a, 0x80, rr.randrange(1, 300)])
        w.answers['0x004fa020.h'] = lambda rr, n, a: rr.choice([0x5a, 0x60, rr.randrange(1, 300)])

        def fan(emu, key, a, pop):
            STATIC_NULL[3] += 1
            for k in range(a[1]):
                t.handle(key, tuple(emu.r32(a[0] + 12 * k + 4 * i) for i in range(3)))
            emu._ret(0, pop)
        e.w32(0x72f3ac, e.new_stub('[0x0072f3ac]', 'c,d', 0, fan))

        def zview(emu, key, a, pop):  # the zoom's render (0x00443320 -> 0x004f7030): the port's zview_render, view 3
            t.handle('0x00442870', (3, self.zoom_dt))
            STATIC_NULL[2] += 1
            emu._ret(0, pop)
        e.stub(0x4f7030, '@zview', 'c,d', None, zview)
        self.zoom_dt = 0

        def fog_range(emu, key, a, pop):  # 0x004f24c0: near to *edx, far to *s0
            emu.w32(a[1], t.handle('0x004f24c0.near', (a[0],)))
            emu.w32(a[2], t.handle('0x004f24c0.far', (a[0],)))
            emu._ret(0, pop)
        e.stub(0x4f24c0, '@fog.range', 'c,d,s0', None, fog_range)
        w.answers['0x004f24c0.near'] = lambda rr, n, a: f32(rr.choice([0.0, 50.0, rr.uniform(0, 300)]))
        w.answers['0x004f24c0.far'] = lambda rr, n, a: f32(rr.choice([750.0, 1500.0, rr.uniform(300, 3000)]))
        e.stub(0x4f2440, '0x004f2440', 'c,s0,s1')
        e.stub(0x4f1df0, '0x004f1df0', 'c')

        def sprintf(emu, key, a, pop):  # "%.1f" with a double
            fmt = emu.cstr(a[1])
            d = struct.unpack('<d', struct.pack('<2I', a[2], a[3]))[0]
            s = (fmt.replace('%.1f', '{:.1f}').format(d)).encode('latin1')
            emu.cpu.write(a[0], s + b'\0')
            emu._ret(len(s), 0)
        e.import_stub(0x5bb658, '@sprintf', 's0,s1,s2,s3', 0, sprintf)

        def static_null(uc, addr, size, user):  # FIX (bug) as the port: TARGET's mode without a rect
            if uc.reg_read(UC_X86_REG_EAX) == 0:
                STATIC_NULL[0] += 1
                uc.reg_write(UC_X86_REG_EIP, 0x43cf3b)
        e.uc.hook_add(UC_HOOK_CODE, static_null, begin=0x43cf08, end=0x43cf08)
        for a, spec in ((0x579af0, ''), (0x579b10, '')):
            e.stub(a, spec=spec)

        def view_call(emu, key, a, pop):  # the views by name: 1 the MFD's, 2 TARGET's (new'd, probe_hud.cpp)
            v, arg = a
            h = self.hud
            name = {emu.r32(h + 0x3434 + 0x824): 1, emu.r32(h + 0x410c + 0x824): 2}.get(v, v)
            t.handle(key, (name, arg))
            emu._ret(0, pop)
        e.stub(0x442870, '0x00442870', 'c,s0', None, view_call)
        e.stub(0x4428b0, '0x004428b0', 'c,s0', None, view_call)
        e.w32(0x72f3b0, e.new_stub('[0x0072f3b0]', 'c,d,s0,s1,s2', 0xc, flat))
        e.w32(0x72f3e4, e.new_stub('[0x0072f3e4]', '', 0))

        def rect(p):
            return [e.r32(p + 4 * i) for i in range(4)]

        def plain(emu, key, a, pop):
            t.handle(key, tuple(a))
            emu._ret(0, pop)

        def lst(emu, key, a, pop):
            pts, n, rp, px = a
            t.handle(key, (n, *rect(rp), px))
            for i in range(n + 1):
                t.handle(key + '.pt', (emu.r32(pts + 8 * i), emu.r32(pts + 8 * i + 4)))
            emu._ret(0, pop)

        def clipped(emu, key, a, pop):
            t.handle(key, tuple(a[:-1]) + tuple(rect(a[-1]) if a[-1] else ()))
            emu._ret(0, pop)

        def polygon(emu, key, a, pop):
            v, n, color = a
            t.handle(key, (n, color, emu.r32(0x74a3c8)))
            for i in range(n):
                t.handle(key + '.v', (emu.r32(v + 12 * i), emu.r32(v + 12 * i + 4)))
            emu._ret(0, pop)

        def render_clip(emu, key, a, pop):
            t.handle(key, ())
            emu.cpu.write(a[0], struct.pack('<3f', *CLIP[:3]))
            emu.cpu.write(a[1], struct.pack('<3f', *CLIP[3:]))
            emu._ret(0, pop)
        for a, spec, fn in ((0x562940, 'c,d,s0,s1,s2', plain), (0x5630a0, 'c,d,s0', plain), (0x562c80, 'c,d,s0,s1,s2,s3', plain),
                            (0x562cb0, 'c,d,s0,s1', lst),
                            (0x5630c0, 'c,d,s0,s1,s2', clipped), (0x563440, 'c,d,s0,s1,s2,s3', clipped),
                            (0x562970, 'c,d,s0,s1,s2,s3,s4', clipped), (0x55d010, 'c,d,s0', polygon),
                            (0x53ce20, 'c,d', render_clip)):
            e.stub(a, f'{a:#010x}', spec, None, fn)
        # TARGET's slots: one with an object (0x004e6fe0 answers it; its
        # vt+0xa8 and vt+0x50 answer at random), one empty.
        self.slots = [e.world_alloc(8), e.world_alloc(8)]
        contact = e.world_alloc(0x40)
        e.w32(self.slots[0], contact)
        obj = e.object('obj', slots={0xa8: ('c', 0), 0x50: ('c', 0), 0x68: ('c,s0', 4), 0xc: ('c', 0),
                                     0x28: ('c', 0), 0xbc: ('c', 0), 0xa4: ('c', 0), 0x3c: ('c,s0', 4)})
        names = [e.world_string('Owens'), 0]
        w.answers['obj.vt+0xa4'] = lambda rr, n, a: rr.choice(names)
        self.classed = {}
        if r.random() < 0.5:  # a target (twice as likely), a trap, the field base, a marker
            self.classify(r, obj, r.choice(CLASSED_TARGET + [c for c in CLASSED_TARGET if c[1] == 6]))
        if self.mech and r.random() < 0.5:  # the player's target (the set-up's TARGET slot, 0x00441800)
            e.w32(self.mech + 0x6c, contact)
        self.track_world(r, contact, obj)
        e.w32(obj + 4, e.world_alloc(0x10))  # its node
        e.stub(0x4e6fe0, spec='c')
        # the live contact's vehicle is there (the triangle reads it unchecked); others at random
        # (and the radar's contacts': it reads theirs unchecked too)
        self.contact, self.obj = contact, obj
        self.radar_contacts = [contact]
        self.contact_objects = {}  # the radar's contacts with their own (classed) object
        w.answers['0x004e6fe0'] = lambda rr, n, a: (self.contact_objects.get(a[0], obj)
                                                    if a[0] in self.radar_contacts or rr.random() < 0.8 else 0)
        w.answers['obj.vt+0xa8'] = lambda rr, n, a: rr.choice([0, 0, 1])
        w.answers['obj.vt+0x50'] = lambda rr, n, a: rr.choice([0, self.target_mech, self.target_mech])  # TARGET's damage view reads its parts
        # The lead point (0x0041d940, op 19): a second mech with a contact in
        # its +0x6c, the first's weapon manager, an aim point or none;
        # positions and velocities as pointers to vectors.
        vecs = []
        for _ in range(6):
            v = e.world_alloc(12)
            e.cpu.write(v, struct.pack('<3f', *(r.uniform(-300, 300) for _ in range(3))))
            vecs.append(v)
        slow = e.world_alloc(12)
        e.cpu.write(slow, struct.pack('<3f', *(r.uniform(-20, 20) for _ in range(3))))
        lm = e.object('mech', size=0x400, slots={0xc: ('c', 0)})
        e.w32(lm + 0x6c, e.r32(self.slots[0]) if r.random() < 0.85 else 0)
        e.w32(lm + 0xd4, e.r32(self.mech + 0xd4) if self.mech else 0)
        e.w32(lm + 0xf8, r.choice([-1, -1, 0]) & 0xffffffff)
        e.cpu.write(lm + 0xec, struct.pack('<3f', *(r.uniform(-300, 300) for _ in range(3))))
        self.lead_mech = lm
        self.lead_out = e.world_alloc(12)
        self.lead_lines = []
        for a_ in (0x5a8630, 0x5a85e0):
            e.stub(a_, spec='c')
        w.answers['0x005a8630'] = lambda rr, n, a: rr.choice(vecs)
        w.answers['0x005a85e0'] = lambda rr, n, a: rr.choice(vecs)
        w.answers['mech.vt+0xc'] = lambda rr, n, a: rr.choice([slow, vecs[0]])
        w.answers['obj.vt+0xc'] = lambda rr, n, a: rr.choice([slow, vecs[1], slow])
        self.text = e.world_string('88 TXT')
        if self.mech:
            self.data_world(r)
            self.radar_world(r)
        self.ops = []
        if not self.mech:
            return
        fr = rnd(self.seed, 'frames')
        if fr.random() < 0.5:  # TARGET shown with a target
            self.ops += [(7, 0x410c), (9, self.slots[0])]
        for _ in range(fr.randrange(1, 20)):
            for _ in range(fr.randrange(0, 6)):
                k = fr.choice(list(OPS))
                if k == 19 and not self.lead_ok():  # no group: the original's null read (FIX (bug))
                    k = 17
                if k == 1:
                    v = fr.choice([0, 1, 1, 1])
                elif k == 3:
                    v = fr.choice([0, 1, 2, 3, 5])
                elif k == 4:
                    v = f32(fr.choice([0.0, 0.0, 0.1, 0.25, 1.0]))
                elif k == 5:
                    v = fr.randrange(8) | fr.choice([0, 1]) << 8 | fr.choice([0, 1, 2]) << 16
                elif k == 6:
                    v = fr.choice([0, 1, 2, 3])
                elif k == 7:
                    v = fr.choice(TOGGLED)
                elif k == 9:
                    v = fr.choice([self.mech + 0x6c, self.slots[0], self.slots[0], self.slots[1]])  # never null (the set-up's slot)
                elif k == 10:
                    v = fr.choice([0, 1, 2, 3])
                elif k == 11:
                    # half the time the MFD's view, an equipment line or the scores
                    i = fr.randrange(len(CHILDREN)) if fr.random() < 0.5 else fr.choice(FOCUS)
                    v = i | fr.choice([0x10, 0x4, 0x4, 0x2, 0x6]) << 16
                elif k == 15:
                    v = fr.randrange(len(FTEXTS))
                elif k == 16:
                    v = fr.randrange(len(IMAGES)) | fr.randrange(6) << 8
                elif k == 17:
                    v = f32(fr.choice([0.016, 0.033, 0.1]))
                elif k == 19:
                    v = self.lead_mech
                elif k == 18:
                    v = f32(fr.choice([0.016, 0.033, 0.1]))
                elif k == 12:
                    v = fr.randrange(16) | (fr.choice([-1, 0, 3, 12]) & 0xff) << 8
                else:
                    v = fr.choice([0, 1])
                self.ops.append((k, v))
            self.ops.append((0, f32(fr.choice([0.0, 0.016, 0.033, 0.1, 0.35, 1.0, 2.5]))))
            if fr.random() < 0.5:  # TARGET's tracking as the data update runs it each frame
                self.ops.append((18, f32(fr.choice([0.016, 0.033, 0.1]))))
            if fr.random() < 0.15:  # the radar's mode
                self.ops.append((22, fr.choice([0, 1, 2, 3] if self.flip else [0, 2])))  # map mode needs the map
            if fr.random() < 0.45:  # the message panel: a slide frame, messages, its switches
                self.ops.append((32, f32(fr.choice([0.016, 0.033, 0.1, 0.5, 1.0]))))
            if fr.random() < 0.15:  # (before the messages: a hidden panel set in, then a message)
                self.ops.append(fr.choice([(29, 0), (30, 0), (30, 1), (31, 0)]))
            if fr.random() < 0.15:
                self.ops.append((27, fr.randrange(len(MESSAGES)) | fr.randrange(4) << 8))
            if fr.random() < 0.15:
                self.ops.append((28, fr.randrange(len(MESSAGES))))
            if fr.random() < 0.12:  # the zoom window shown through its show_for (0x004438d0)
                self.ops.append((33, f32(fr.choice([-1.0, 0.0, 0.5, 3.0]))))
            # the zoom's 3D pass (shown, or still scaled), with a device only: the
            # software path (0x00564460, 0x004f6dc0) is not ported (CLAUDE.md, Rendering)
            if fr.random() < 0.35 and self.e.r32(0x72f348):
                self.ops.append((42, f32(fr.choice([0.016, 0.033, 0.1]))))
            if fr.random() < 0.25:  # the HUD keys
                self.ops.append((fr.choice([34, 35, 36, 36, 37, 37, 38]), 0))
            if fr.random() < 0.1:  # the jump jets' count (+0x3d8): the fuel gauge shown or hidden
                self.ops.append((26, fr.choice([0, 1, 2])))
            if fr.random() < 0.1:  # its static (0x0043dc70, the ZAPS hits')
                self.ops.append((25, f32(fr.choice([0.0, 0.05, 0.3, 1.5]))))
            if fr.random() < 0.3:  # the static (0x0043cd20): GameIntensity's flag, an add, the tick
                if fr.random() < 0.15:
                    self.ops.append((41, fr.choice([0, 0, 1])))
                if fr.random() < 0.4:
                    self.ops.append((40, fr.randrange(6) | fr.randrange(4) << 4 | fr.randrange(4) << 8
                                     | fr.randrange(4) << 12))
                self.ops.append((39, f32(fr.choice([0.016, 0.033, 0.1, 0.5]))))
            if fr.random() < 0.15:  # its polylines: their lists, then their counts
                self.ops += [(23, self.radar_lists[0]), (24, self.radar_lists[1]),
                             (21, fr.choice([0, 2, 3, 3, 4, 5, 8]) | fr.choice([0, 3, 4, 6, 8]) << 8)]  # 3: shown, not drawn
            if fr.random() < 0.4:  # the data update (0x0043c390)
                self.ops.append((20, f32(fr.choice([0.016, 0.033, 0.1]))))
            if fr.random() < 0.2 and self.lead_ok():
                self.ops.append((19, self.lead_mech))

    def classify(self, r, o, cls):
        """An object given a real class vtable and the fields its slots read:
        the team +0xc0, height +0x68, a target's flags +0xe8 and name +0xec, the
        centroid node +0x14 (its data's refresh flag and place), zEntity's
        velocity +0x30; the pilots' names (0x004aee70) answered."""
        e, w = self.e, self.w
        vt, c = cls
        e.w32(o, vt)
        e.w32(o + 0xc0, r.choice([1, 2, 8, 0x10]))
        e.cpu.write(o + 0x68, struct.pack('<f', r.choice([0.0, 4.0, 12.5])))
        e.cpu.write(o + 0xe8, bytes([r.randrange(0, 0x40)]))
        e.w32(o + 0xec, e.world_string(f'tgt{len(self.classed)}') if r.random() < 0.7 else 0)
        node, data = e.world_alloc(0x40), e.world_alloc(0x100)
        e.w32(o + 0x14, node)
        e.w32(node + 0x38, data)
        e.cpu.write(data, bytes([r.choice([0, 0x20])]))
        e.cpu.write(data + 0x84, struct.pack('<3f', *(r.uniform(-400, 400) for _ in range(3))))
        e.cpu.write(o + 0x30, struct.pack('<3f', *(r.uniform(-5, 5) for _ in range(3))))
        self.classed[o] = c
        if not getattr(self, 'pilot_names', None):
            self.pilot_names = [e.world_string('Mr. Burns'), 0]
            e.stub(0x4aee70, '0x004aee70', 'c')
            w.answers['0x004aee70'] = lambda rr, n, a: rr.choice(self.pilot_names)

    def track_world(self, r, contact, obj):
        """TARGET's tracking (0x00440b00, op 18; hud.md section 14): the contact
        (its seen object +4, entity +8, direction +0x20), entities whose
        nodes hold a position (+0x14, data +0x38: flags, +0x84), the player
        mech's node and team masks, the target's team and centre, the
        projections from a table in call order, MSVCRT's rand."""
        e, w, t = self.e, self.w, self.t

        def entity():
            ent, node, data = e.world_alloc(0x40), e.world_alloc(0x40), e.world_alloc(0x100)
            e.w32(ent + 0x14, node)
            e.w32(node + 0x38, data)
            e.cpu.write(data, bytes([r.choice([0, 0x20])]))
            e.cpu.write(data + 0x84, struct.pack('<3f', *(r.uniform(-400, 400) for _ in range(3))))
            return ent
        e.w32(contact + 4, obj)
        e.w32(contact + 8, entity())
        d = [r.uniform(-1, 1) for _ in range(3)]
        n = math.sqrt(sum(x * x for x in d)) or 1.0
        e.cpu.write(contact + 0x20, struct.pack('<3f', *(x / n for x in d)))
        if self.mech:
            ent = entity()
            e.w32(self.mech + 0x14, e.r32(ent + 0x14))
            e.w32(self.mech + 0xc4, r.choice([0, 1, 6]))
            e.w32(self.mech + 0xc8, r.choice([0, 2, 9]))
        team = e.world_alloc(0x100)
        e.w32(team + 0xc0, r.choice([1, 2, 8]))
        centre = e.world_alloc(12)
        e.cpu.write(centre, struct.pack('<3f', *(r.uniform(-400, 400) for _ in range(3))))
        w.answers['obj.vt+0x28'] = team
        w.answers['obj.vt+0xbc'] = centre
        for a_, spec in ((0x4f36b0, 'c,s0,s1,s2'), (0x4ed280, 'c,d'), (0x47ef10, 'c,s0')):
            e.stub(a_, spec=spec)

        def set_place(emu, key, a, pop):  # 0x004f3870: logged, and the place into the camera's data +0x14
            t.handle(key, tuple(a))
            d = emu.r32(a[0] + 0x38) if a[0] else 0
            if d:
                for k in range(3):
                    emu.w32(d + 0x14 + 4 * k, a[1 + k])
            emu._ret(0, pop)
        e.stub(0x4f3870, '0x004f3870', 'c,s0,s1,s2', None, set_place)
        fbuf = e.world_alloc(8)
        fthunk = e.thunk(b'\xd9\x05' + struct.pack('<I', fbuf) + b'\xc3')

        def fret(emu, key, a, pop):
            emu.w32(fbuf, t.handle(key, tuple(a)))
            emu._ret(0, pop, then=fthunk)
        e.stub(0x4e7070, '0x004e7070', 'c', None, fret)
        w.answers['0x004e7070'] = lambda rr, n, a: f32(rr.choice([5.0, 12.0, 30.0]))
        # 0x0053cf10: the outcode answered, the point from the table in call order
        ptab = e.world_alloc(12 * 400)
        for k in range(400):
            e.cpu.write(ptab + 12 * k, struct.pack('<3f', r.uniform(-60, 700), r.uniform(-60, 520), r.uniform(0, 1)))

        def project(emu, key, a, pop):
            src, out = a
            code = t.handle(key, tuple(emu.r32(src + 4 * i) for i in range(3)))
            at = t.handle('0x0053cf10.out', ())
            emu.cpu.write(out, bytes(emu.cpu.read(at, 12)))
            emu._ret(code, pop)
        e.stub(0x53cf10, '0x0053cf10', 'c,d', None, project)
        w.answers['0x0053cf10'] = lambda rr, n, a: rr.choice([0, 0, 0, 0, 1, 2, 4, 8, 5, 9])
        w.answers['0x0053cf10.out'] = lambda rr, n, a: ptab + 12 * (n % 400)
        rng = MsvcRand(1)
        e.import_stub(0x5bb668, '@rand', '', 0, lambda emu, key, a, pop: emu._ret(rng(), pop))

    def data_world(self, r):
        """The HUD's data update (0x0043c390, op 20; hud.md section 14 part 4):
        the mech's controls (IControls through the virtual base at +0x37c),
        speed, heat, coolant and forward fields, the mission time; its
        unported calls stubbed (floats on the x87 by a thunk)."""
        e, w, t = self.e, self.w, self.t
        m = self.mech
        fbuf = e.world_alloc(8)
        fthunk = e.thunk(b'\xd9\x05' + struct.pack('<I', fbuf) + b'\xc3')

        def fret(emu, key, a, pop):
            emu.w32(fbuf, t.handle(key, ()))
            emu._ret(0, pop, then=fthunk)
        c, vb = e.world_alloc(0x100), e.world_alloc(0x10)
        e.w32(m + 0x37c, c)
        e.w32(c + 4, vb)
        e.w32(vb + 4, 0x20)  # the IControls at c + 4 + 0x20
        vt = e.world_alloc(0x40)
        e.w32(c + 0x24, vt)
        e.w32(vt + 0x1c, e.new_stub('controls.vt+0x1c', '', 0, fret))
        e.w32(vt + 0x2c, e.new_stub('controls.vt+0x2c', '', 0, fret))
        w.answers['controls.vt+0x1c'] = lambda rr, n, a: f32(rr.choice([0.0, 0.5, 1.0, -0.4]))
        w.answers['controls.vt+0x2c'] = lambda rr, n, a: f32(rr.choice([0.0, 0.7, -1.0]))
        mvt = e.r32(m)
        e.w32(mvt + 0x148, e.new_stub('mech.vt+0x148', 'c', 0, lambda emu, key, a, pop: (
            emu.w32(fbuf, t.handle(key, tuple(a))), emu._ret(0, pop, then=fthunk))))
        w.answers['mech.vt+0x148'] = lambda rr, n, a: f32(rr.choice([0.0, 0.4, 1.0, 1.3, -0.2]))
        for off, v in ((0x518, r.uniform(5, 25)), (0x528, r.uniform(0.2, 1.0)), (0x554, r.choice([0.0, 0.8, 1.45, 1.7])),
                       (0x558, r.uniform(0, 0.1)), (0x55c, r.uniform(0, 0.6)), (0x56c, r.uniform(0, 1))):
            e.cpu.write(m + off, struct.pack('<f', v))
        a_ = r.uniform(-math.pi, math.pi)
        e.cpu.write(m + 0x24, struct.pack('<3f', math.sin(a_), 0.0, -math.cos(a_)))
        e.cpu.write(0x60991c, struct.pack('<f', r.choice([0.0, 59.9, 125.5, 3700.25, -42.0, -3700.5])))
        e.cpu.write(0x609920, struct.pack('<f', r.choice([0.0, 600.0])))
        vecs = e.world_alloc(36)
        e.cpu.write(vecs, struct.pack('<9f', *(r.uniform(-1, 1) for _ in range(9))))

        def view_dir(emu, key, a, pop):  # 0x00522170: ECX the camera's +0x20, EDX the out vector
            at = t.handle(key, ())
            emu.cpu.write(a[1], bytes(emu.cpu.read(at, 12)))
            emu._ret(0, pop)
        e.stub(0x522170, '0x00522170', 'c,d', None, view_dir)
        w.answers['0x00522170'] = vecs

        def normalize(emu, key, a, pop):  # 0x004020d0 (as stubbed: the vector unchanged, its length answered)
            emu.w32(fbuf, t.handle(key, tuple(emu.r32(a[0] + 4 * i) for i in range(3))))
            emu._ret(0, pop, then=fthunk)
        e.stub(0x4020d0, '0x004020d0', 'c', None, normalize)
        w.answers['0x004020d0'] = lambda rr, n, a: f32(math.sqrt(sum(struct.unpack('<f', struct.pack('<I', x))[0] ** 2
                                                                     for x in a)))
        e.stub(0x41cdf0, spec='c')
        w.answers['0x0041cdf0'] = vecs + 12

        for a2, spec in (
                         (0x479ef0, 's0,s1'), (0x43f2d0, 'c,s0')):
            e.stub(a2, spec=spec)

    def radar_world(self, r):
        """The radar's update (0x0043dd00, part 1; hud.md section 15): the
        mech's sensor range and orientation, the map's point (0x004586e0)
        answered, the polylines' lists (op 21)."""
        e, w, t = self.e, self.w, self.t
        m = self.mech
        e.cpu.write(m + 0x9c, struct.pack('<f', r.choice([400.0, 1000.0, 2500.0, -500.0])))
        a_ = r.uniform(-math.pi, math.pi)
        e.cpu.write(m + 0x48, struct.pack('<4f', math.cos(a_ / 2), 0.0, math.sin(a_ / 2), 0.0))

        def map_point(emu, key, a, pop):  # 0x004586e0: ECX the map, the point, out ints
            mp, pp, out = a
            ok = t.handle(key, (mp,) + tuple(emu.r32(pp + 4 * i) for i in range(3)))
            emu.w32(out, t.handle('0x004586e0.x', ()))
            emu.w32(out + 4, t.handle('0x004586e0.y', ()))
            emu._ret(ok, pop)
        e.stub(0x4586e0, '0x004586e0', 'c,s0,s1', None, map_point)

        def sprintf(emu, key, a, pop):  # the range text (the port's snprintf, untraced)
            buf, fmt = a
            va = emu.uc.reg_read(UC_X86_REG_ESP) + 12
            f = emu.cstr(fmt)
            if re.search(r'%[-0-9.]*f', f):  # doubles among the arguments: %s, %d, %f in order
                out, k, i = '', va, 0
                for m in re.finditer(r'%([-0-9.]*)([sdf])', f):
                    out += f[i:m.start()]
                    i = m.end()
                    if m.group(2) == 'f':
                        out += ('%' + m.group(1) + 'f') % struct.unpack('<d', bytes(emu.cpu.read(k, 8)))[0]
                        k += 8
                    else:
                        v = emu.r32(k)
                        k += 4
                        out += (emu.cstr(v) if v else '(null)') if m.group(2) == 's' else str(struct.unpack('<i', struct.pack('<I', v))[0])
                text = (out + f[i:]).encode('latin1')
            else:
                text = c_format(emu, f, va).encode('latin1')
            emu.cpu.write(buf, text + b'\0')
            emu._ret(len(text), 0)
        e.import_stub(IAT['sprintf'], '@sprintf', 's0,s1', 0, sprintf)
        w.answers['0x004586e0'] = lambda rr, n, a: rr.choice([1, 1, 0])
        w.answers['0x004586e0.x'] = lambda rr, n, a: rr.choice([rr.randrange(-20, 660), 40, 41]) & 0xffffffff
        w.answers['0x004586e0.y'] = lambda rr, n, a: rr.choice([rr.randrange(-20, 500), 30, 31]) & 0xffffffff  # the map rect's edges
        items = []
        for _ in range(3):
            it = e.object('item', size=0x20, slots={0x18: ('c', 0)})
            items.append(it)
        pts = e.world_alloc(12 * 8)
        e.cpu.write(pts, struct.pack('<24f', *(r.uniform(-1500, 1500) for _ in range(24))))
        w.answers['item.vt+0x18'] = lambda rr, n, a: pts + 12 * rr.randrange(8)
        self.radar_lists = [e.object('list', size=0x20, slots={0xc: ('c,s0', 4)}) for _ in range(2)]
        w.answers['list.vt+0xc'] = lambda rr, n, a: rr.choice(items)
        self.contacts_world(r)

    def contacts_world(self, r):
        """The radar's contacts (0x0043dd00 from 0x0043e445, part 2): the mech's
        contacts (TARGET's, one the same by entity and vtable, others), their
        blips (vt+0x3c) or none; the network's marker; the weapons' radar list
        of shots; the landmines; the selected target's triangle clipped to the
        screen (0x00579880)."""
        e, w, t = self.e, self.w, self.t
        m, contact = self.mech, self.contact
        vt = e.world_alloc(0x10)
        e.w32(contact, vt)
        for _ in range(r.choice([0, 1, 3, 5, 7])):
            c = e.world_alloc(0x40)
            same = r.random() < 0.3
            e.w32(c, vt if same or r.random() < 0.5 else e.world_alloc(0x10))
            e.w32(c + 8, e.r32(contact + 8) if same else e.world_alloc(0x20))
            e.cpu.write(c + 0x2c, bytes([r.choice([0, 1, 3])]))
            self.radar_contacts.append(c)
            if r.random() < 0.6:  # a turret, a target, a vehicle, a field base, ...: its own object
                o = e.world_alloc(0x100)
                self.classify(r, o, r.choice(CLASSED))
                e.w32(c + 4, o)
                self.contact_objects[c] = o
        e.cpu.write(contact + 0x2c, bytes([r.choice([0, 1])]))
        r.shuffle(self.radar_contacts)
        cv = e.world_alloc(4 * len(self.radar_contacts) + 4)
        for i, c in enumerate(self.radar_contacts):
            e.w32(cv + 4 * i, c)
        e.w32(m + 0xac, cv)
        e.w32(m + 0xb0, cv + 4 * len(self.radar_contacts))
        blips = [e.object(f'blip', size=0x40, slots={0xc: ('c,s0,s1', 8), 0x4: ('c', 0)}) for _ in range(2)]
        w.answers['obj.vt+0x3c'] = lambda rr, n, a: rr.choice(blips + [blips[0], 0])
        for table in BLIP_TABLES:  # each class's ally, enemy, other (0x0041ec60, 0x0045b970, 0x004a5fc0, 0x004d2d70)
            for k in range(3):
                e.w32(table + 4 * k, r.choice(blips + [0]))
        srect = e.world_alloc(16)
        e.cpu.write(srect, struct.pack('<4i', 0, 0, 640, 480))
        e.stub(0x579880, spec='')
        w.answers['0x00579880'] = srect
        # the network's marker: the player's element, its image by its +0xc
        if r.random() < 0.5:
            net = e.world_alloc(0x40)
            e.w32(net + 0x20, r.randrange(4))
            mk = e.object('marker', size=0x40, slots={0x18: ('c', 0)})
            e.cpu.write(mk + 0xc, bytes([r.choice([0, 1])]))
            e.w32(mk + 0x28, e.world_string('Kerensky'))
            mpos = [e.world_alloc(12) for _ in range(3)]
            for v, d in zip(mpos, (300.0, 900.0, 2500.0)):
                a_ = r.uniform(-math.pi, math.pi)
                e.cpu.write(v, struct.pack('<3f', d * math.cos(a_), r.uniform(-20, 20), d * math.sin(a_)))
            w.answers['marker.vt+0x18'] = lambda rr, n, a: rr.choice(mpos)
            e.stub(0x58b060, spec='c,s0')
            w.answers['0x0058b060'] = lambda rr, n, a: rr.choice([mk, mk, 0])
            e.w32(0x608ba8, r.choice([blips[0], 0]))
            e.w32(0x608bac, r.choice([blips[1], blips[0]]))
            e.w32(0x609980, net)
        # the weapons' radar list: {shot, friendly}, a shot's place its +0x48
        head = e.r32(0x60b22c)
        prev = head
        for _ in range(r.choice([0, 2, 4])):
            nd, b, shot = e.world_alloc(12), e.world_alloc(8), e.world_alloc(0x60)
            e.cpu.write(shot + 0x48, struct.pack('<3f', *(r.uniform(-600, 600) for _ in range(3))))
            e.w32(b, shot)
            e.cpu.write(b + 4, bytes([r.choice([0, 1])]))
            e.w32(nd + 8, b)
            e.w32(nd + 4, prev)
            e.w32(prev, nd)
            e.w32(nd, head)
            e.w32(head + 4, nd)
            prev = nd
        # the landmines: 16 bytes each, armed and the place
        n = r.choice([0, 2, 5])
        mv = e.world_alloc(16 * n + 16)
        for i in range(n):
            e.cpu.write(mv + 16 * i, bytes([r.choice([0, 1, 1])]))
            e.cpu.write(mv + 16 * i + 4, struct.pack('<3f', *(r.uniform(-350, 350) for _ in range(3))))
        e.w32(0x604e4c, mv)
        e.w32(0x604e50, mv + 16 * n)

    def lead_ok(self):
        """A group selected on the lead mech's manager (0x0041d940 reads it unchecked)."""
        e = self.e
        wm = e.r32(self.lead_mech + 0xd4)
        gi = e.r32(wm + 0x28) if wm else 0xffffffff
        return gi != 0xffffffff and e.r32(wm + 0x2c + 4 * gi) != 0

    def run_frames(self):
        """The ops (OPS) and frames, in the original."""
        e, h = self.e, self.hud
        if self.ops:
            vt = e.world_alloc(0x40)
            stub = e.new_stub('nav.vt+0x24', 'c,s0', 4)
            for m in (h + 0x3434, h + 0x410c):
                if e.r32(m + 0x820):
                    old = e.r32(e.r32(m + 0x820))  # its own slots kept (vt+0 its deleting destructor, item 9's free)
                    for i in range(0, 0x40, 4):
                        e.w32(vt + i, e.r32(old + i))
                    e.w32(vt + 0x24, stub)
                    e.w32(e.r32(m + 0x820), vt)
        zoom = h + 0x144c + 0xbc
        for k, v in self.ops:
            if k == 0:
                e.call(0x43c820, ecx=h, stack=(v,))
            elif k == 1:
                e.call(0x43baf0, ecx=h, stack=(v,))
            elif k == 2:
                e.w32(zoom + 0xc, e.r32(zoom + 0xc) ^ 0x10)
            elif k == 3:
                e.w32(h + 0x1d54, v)
            elif k == 4:
                e.w32(zoom + 0x630 + 0xc8, v)
            elif k == 5:
                part = h + 0x3434 + 0x3c + 0xf0 + 0xdc * (v & 0xff)
                e.w32(part + 0xc4, v >> 8 & 0xff)
                e.w32(part + 0xc8, v >> 16)
            elif k == 6:
                e.w32(h + 0x3434 + 0x38, v)
            elif k == 7:
                e.w32(h + v + 0xc, e.r32(h + v + 0xc) ^ 0x10)
            elif k == 8:
                e.cpu.write(zoom + 0x16c, bytes([v]))
            elif k == 9:
                e.w32(h + 0x410c + 0x2404, v)
            elif k == 10:
                e.w32(h + 0x410c + 0x38, v)
            elif k == 11:
                c = CHILDREN[v & 0xffff]
                a = e.r32(h + 0x3434 + 0x824) if c == 'view' else h + c
                e.w32(a + 0xc, e.r32(a + 0xc) ^ v >> 16)
            elif k == 17:
                e.call(0x43c6b0, ecx=h, stack=(v,))
            elif k == 20:
                e.call(0x43c390, ecx=h, stack=(v,))
            elif k == 21:
                rd = h + 0x14
                e.w32(rd + 0xbd4, v & 0xff)
                e.w32(rd + 0xbd8, v >> 8)
                # their clip, the radar's square (in play set as the radar first moves)
                e.w32(rd + 0xcc0, 0x5e39c8)
                e.w32(rd + 0xdac, 0x5e39c8)
            elif k in (23, 24):
                e.w32(h + 0x14 + (0xdb4 if k == 23 else 0xdb8), v)
            elif k == 27:
                e.call(0x40fe20, stack=(f32(MESSAGE_TIMES[v >> 8]), self.msg_fmt, self.msgs[v & 0xff]))
            elif k == 28:
                e.call(0x40fe50, stack=(self.msg_fmt, self.msgs[v]))
            elif k == 29:
                e.call(0x40f070)
            elif k == 30:
                e.call(0x40f0b0, ecx=v)
            elif k == 31:
                e.call(0x40f040)
            elif k == 32:
                e.w32(0x72c52c, v)  # the frame's seconds
                e.call(0x40f8d0)
            elif k == 33:
                e.call(e.r32(e.r32(zoom) + 0x60), ecx=zoom, stack=(v,))
            elif k == 34:
                e.call(0x442e00, ecx=zoom)
            elif k in (35, 38):
                e.call(0x43da40 if k == 35 else 0x43dc10, ecx=h + 0x14)
            elif k in (36, 37):
                e.call(0x43d240 if k == 36 else 0x43d250, ecx=h)
            elif k == 26:
                e.w32(self.mech + 0x3d8, v)
            elif k == 39:  # the mission's player is the HUD's mech for the call
                keep = e.r32(0x60861c)
                world = e.world_alloc(0x20)
                e.w32(world + 0x14, self.mech)
                e.w32(0x60861c, world)
                e.call(0x43cd20, ecx=h, stack=(v,))
                e.w32(0x60861c, keep)
            elif k == 40:
                e.call(0x43bfe0, ecx=h, stack=(f32(STATIC_STEP[v >> 4 & 3]), v & 0xf, f32(STATIC_HOLD[v >> 8 & 3]),
                                               f32(STATIC_MOST[v >> 12 & 3])))
            elif k == 41:
                e.cpu.write(0x608e4c, bytes([v]))
            elif k == 42:  # the reticle's +0x28 (0x00444230): the zoom's 3D pass
                self.zoom_dt = v
                e.call(0x444230, ecx=h + 0x144c, stack=(v,))
            elif k == 25:
                e.call(0x43dc70, ecx=h + 0x14, stack=(v,))
            elif k == 22:
                rd = h + 0x14
                e.cpu.write(rd + 0x5c, bytes([v & 1, v >> 1 & 1]))
            elif k == 18:
                e.call(0x440b00, ecx=h + 0x410c, stack=(v,))
            elif k == 19:
                e.call(0x41d940, ecx=v, stack=(self.lead_out,))
                ok = e.uc.reg_read(UC_X86_REG_EAX) & 0xff
                vals = ' '.join(f'{e.r32(self.lead_out + 4 * i):x}' for i in range(3)) if ok else ''
                self.lead_lines.append(f'lead {ok} {vals}')
            elif k in (13, 14):
                for c in CHILDREN:
                    a = e.r32(h + 0x3434 + 0x824) if c == 'view' else h + c
                    f = e.r32(a + 0xc)
                    e.w32(a + 0xc, f | 4 if k == 13 else f & ~0x10)
                if k == 14:
                    for c in TOGGLED:
                        e.w32(h + c + 0xc, e.r32(h + c + 0xc) & ~0x10)
            elif k == 15:
                e.call(0x5a12f0, ecx=h + FTEXTS[v], stack=(self.text,))
            elif k == 16:
                j = v >> 8
                img = e.r32((0x6059b0 if j < 3 else 0x6059d0 - 12) + 4 * j)
                e.call(0x59bb50, ecx=h + IMAGES[v & 0xff], stack=(img,))
            elif k == 12:
                line = h + 0x6520 + 0x130 + 0x1120 * (v & 0xff)
                e.w32(line + 0x1110, struct.unpack('<b', bytes([v >> 8 & 0xff]))[0] & 0xffffffff)
                e.cpu.write(line + 0x111a, b'\x01')

    def aim_world(self, r):
        """The aim ray's answers, made before the snapshot so the probe can read
        them: the unprojected points, the segment lists and their results, the
        materials' soils and [0x00608c00]."""
        e, w = self.e, self.w
        n = r.randrange(1, 6) if self.mech else 0
        self.places = [(r.uniform(-1.2, 1.2), r.uniform(-1.2, 1.2)) for _ in range(n)]
        self.utab = e.world_alloc(24 * max(n, 1))
        self.stab = e.world_alloc(0x508 * max(n, 1))
        for i in range(16):
            e.w32(0x608c00 + 4 * i, r.choice([0, 0, 1, 2]))
        # the hits' nodes: real blocks (the aim state reads +0xbc, hud.md section 12)
        self.nodes = []
        for _ in range(4):
            nd = e.world_alloc(0x100)
            e.w32(nd + 0xbc, r.choice([0, 1, 2, 3]))
            self.nodes.append(nd)
        mats = [0]
        for _ in range(4):
            m = e.world_alloc(0x40)
            e.w32(m + 0x20, r.randrange(16))
            mats.append(m)
        for k in range(n):
            a = [r.uniform(-50, 50) for _ in range(3)]
            b = [v + r.uniform(-400, 400) for v in a]
            e.cpu.write(self.utab + 24 * k, struct.pack('<6f', *a, *b))
            base = self.stab + 0x508 * k
            code = r.choice([0, 0, 0, 0, 1])
            count = r.choice([0, 1, 1, 2, 3, 5])
            e.w32(base, code)
            buf = bytearray(0x504)
            buf[0] = count
            for j in range(count):
                t = r.random()
                pt = [a[q] + (b[q] - a[q]) * t for q in range(3)]
                struct.pack_into('<6fIfII', buf, 4 + 0x28 * j, r.uniform(-1, 1), r.uniform(-1, 1), r.uniform(-1, 1),
                                 *pt, r.randrange(4), r.uniform(0, 100), r.choice(mats), r.choice(self.nodes))
            e.cpu.write(base + 4, bytes(buf))
        calls = {'u': 0, 's': 0}

        def unproject(emu, key, a, pop):
            sp, out, cnt = a
            vals = struct.unpack('<3I', bytes(emu.cpu.read(sp, 12)))
            self.t.handle(key, vals)
            i = calls['u']
            calls['u'] += 1
            emu.cpu.write(out, bytes(emu.cpu.read(self.utab + 12 * i, 12)))
            emu._ret(0, pop)
        e.stub(0x523930, '0x00523930', 'c,d,s0', None, unproject)

        def segments(emu, key, a, pop):
            world, buf, *pts = a
            k = calls['s']
            calls['s'] += 1
            code = self.t.handle(key, (world, *pts))
            base = self.stab + 0x508 * k
            emu.cpu.write(buf, bytes(emu.cpu.read(base + 4, 0x504)))
            emu._ret(code, pop)
        w.answers['0x004eed90'] = lambda rr, n, a: e.r32(self.stab + 0x508 * n)
        e.stub(0x4eed90, '0x004eed90', 'c,d,s0,s1,s2,s3,s4,s5', None, segments)
        e.stub(0x4b51f0, spec='')
        e.stub(0x4eb6d0, spec='c,d')

    def aim_dump(self):
        e = self.e
        o = self.hud + 0x144c
        vals = [e.r32(o + 0x14), e.r32(o + 0x18)] + [e.r32(o + 0x8fc + 4 * i) for i in range(3)]
        vals += [e.r32(o + 0x924)] + [e.r32(o + 0x928 + 4 * i) for i in range(10)]
        z = o + 0xbc
        vals += [e.r32(z + 0x50 + 4 * i) for i in range(4)] + [e.r32(z + 0x14), e.r32(z + 0x18)]
        return 'aim ' + ' '.join(f'{v:x}' for v in vals) + ' '

    def give_parts(self, m):
        """The parts the damage view reads (0x00447a50): legs +0x38c, arms
        +0x394, torso +0x39c, head +0x3a4 (+0x3a0 the hip), each answering its
        internal by section (vt+0x48, a float on the x87)."""
        e, w, t = self.e, self.w, self.t
        if not hasattr(self, 'part_vt'):
            pbuf = e.world_alloc(8)
            pthunk = e.thunk(b'\xd9\x05' + struct.pack('<I', pbuf) + b'\xc2\x04\x00')  # fld; ret 4

            def pret(emu, key, a, pop):
                emu.w32(pbuf, t.handle(key, tuple(a)))
                emu._ret(0, pop, then=pthunk)
            self.part_vt = e.world_alloc(0x60)
            e.w32(self.part_vt + 0x48, e.new_stub('part.vt+0x48', 'c,s0', 4, pret))
            w.answers['part.vt+0x48'] = lambda rr, n, a: f32(rr.choice(
                [1.0, 1.0, 0.99, 0.75, 0.7, 0.5, 0.4, 0.3, 0.2, 0.1, 0.0, -0.25]))
        for off in range(0x38c, 0x3a8, 4):
            part = e.world_alloc(0x40)
            e.w32(part, self.part_vt)
            e.w32(m + off, part)

    def fake_mech(self, r):
        """The fields the set-up reads: vt+0xa0, +0x6c (empty), +0xbc, the
        weapon manager's count, the parts' equipment counts."""
        e = self.e
        m = e.world_alloc(0x400)
        vt = e.world_alloc(0x200)  # to +0x148 (the jump fuel)
        e.w32(m, vt)
        e.w32(m + 4, e.world_alloc(0x40))  # its node
        e.w32(vt + 0xa0, e.new_stub('mech.vt+0xa0', 'c', 0))
        e.w32(vt + 0xc, e.new_stub('mech.vt+0xc', 'c', 0))  # its velocity (the lead point)
        e.w32(m + 0xf8, r.choice([-1, -1, 0]) & 0xffffffff)
        e.cpu.write(m + 0xec, struct.pack('<3f', r.uniform(-300, 300), r.uniform(0, 30), r.uniform(-300, 300)))
        # +0x6c: no target (a target's path is TARGET's item, hud.md section 9)
        e.cpu.write(m + 0xbc, bytes([r.randrange(256)]))
        wm = e.world_alloc(0x100)  # +0x88: the lock shown (the aim state)
        e.w32(m + 0xd4, wm)
        e.w32(wm + 0x24, r.randrange(0, 12))
        none = r.random() < 0.15  # no equipment: the list hides (0x00448320)
        for i in range(16):
            e.w32(m + 0x3ac + 4 * i, (0 if none and i != 11 else r.choice([0, 1, 2, 3, -1])) & 0xffffffff)
        # the equipment's (0x00447e40): the +0x3f0 record (on, a count) or none, +0x574
        if r.random() < 0.7:
            rec = e.world_alloc(0x20)
            e.cpu.write(rec, bytes([r.choice([0, 1])]))
            e.w32(rec + 0x18, r.randrange(0, 40))
            e.w32(m + 0x3f0, rec)
        e.cpu.write(m + 0x574, struct.pack('<f', r.choice([0.0, 0.0, 0.5, 3.5])))
        self.weapon_owner = m
        self.weapon_groups(wm)
        self.give_parts(m)
        return m

    def weapon_groups(self, wm):
        """The weapon list's side of the manager (hud.md section 13): mode, the
        current group, up to five groups of fake weapons (16 lines at most),
        each weapon's group, name and enabled flag; its vt+8, +0x14 (ammo)
        and +0x34 (recycle, on the x87) and 0x004b3010 (damage) stubbed."""
        e, w, t = self.e, self.w, self.t
        r = rnd(self.seed, 'weapons')
        fbuf = e.world_alloc(8)
        fthunk = e.thunk(b'\xd9\x05' + struct.pack('<I', fbuf) + b'\xc3')

        def fret(emu, key, a, pop):
            emu.w32(fbuf, t.handle(key, tuple(a)))
            emu._ret(0, pop, then=fthunk)
        weapons = []
        for k in range(6):
            o = e.object('weapon', slots={0x8: ('c', 0), 0x14: ('c', 0), 0x34: ('c', 0, fret)})
            d = e.world_alloc(0x80)
            e.w32(d + 8, e.world_string(f'WPN{k}'))
            e.cpu.write(d + 0x2c, struct.pack('<f', r.choice([60.0, 300.0, 1000.0, 4.0])))  # the shot's speed
            e.w32(d + 0x6c, r.choice([0, 0, 2, 0x800]))  # instant / beam
            e.w32(o + 0x2c, d)
            e.cpu.write(o + 0x74, bytes([r.choice([0, 1, 1])]))
            weapons.append(o)
        self.groups_made = []
        e.w32(wm + 0x1c, r.choice([0, 1, 2]))
        e.w32(wm + 0x28, r.choice([-1, 0, 1, 2, 3, 4]) & 0xffffffff)
        left = 16
        for i in range(5):
            if r.random() < 0.3 or left == 0:
                continue
            n = min(r.randrange(0, 5), left)
            left -= n
            g = e.world_alloc(0x40)
            e.w32(g + 8, n)
            self.groups_made.append(g)
            for k in range(n):
                wpn = r.choice(weapons)
                e.w32(g + 0x14 + 4 * k, wpn)
                e.w32(wpn + 0x60, r.choice([i, i, i - 1]) & 0xffffffff)
            e.w32(wm + 0x2c + 4 * i, g)
        # The selected group exists (the original's lead point 0x0041d940
        # reads it unchecked: FIX (bug)); without any, no targeting computer.
        made = [i for i in range(5) if e.r32(wm + 0x2c + 4 * i)]
        e.w32(wm + 0x28, r.choice(made) if made else 0xffffffff)
        if not made and self.weapon_owner:
            e.w32(self.weapon_owner + 0x3e4, 0)
        for g in self.groups_made:  # the group's current weapon (+0x10)
            n = e.r32(g + 8)
            e.w32(g + 0x10, e.r32(g + 0x14 + 4 * r.randrange(n)) if n and r.random() < 0.8 else 0)
        e.stub(0x4b3010, '0x004b3010', 'c', None, fret)
        e.stub(0x4b04c0, spec='c')
        w.answers['weapon.vt+0x34'] = lambda rr, n, a: f32(rr.choice([0.0, 0.3, 1.0, 1.0, 1.5, -0.2]))
        w.answers['weapon.vt+0x14'] = lambda rr, n, a: rr.choice([0, 5, 20])
        w.answers['0x004b3010'] = lambda rr, n, a: f32(rr.choice([0.0, 0.0, 0.6, 0.9, 1.0]))
        w.answers['0x004b04c0'] = lambda rr, n, a: rr.choice([0] + weapons)

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
        mp = e.r32(0x603d7c)  # the message panel (hud.md section 18)
        if mp:
            textbox(mp, 'mpanel')
            for k, off in (('timer', 0x4c8), ('speed', 0x4cc), ('width', 0x4d0), ('onx', 0x4d8), ('ony', 0x4dc),
                           ('offx', 0x4e0), ('offy', 0x4e4), ('x', 0x4e8), ('y', 0x4ec), ('font', 0x5f8),
                           ('fontb', 0x5fc)):
                kv(k, e.r32(mp + off))
            kv('in', e.r8(mp + 0x4d4))
            panel(mp + 0x4f0)
            flush('mpfields')
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
        shown = e.r8(e.r32(self.mech + 0xd4) + 0x88) if self.mech else 0
        saved = ' '.join(f'{e.r32(a):x}' for a in (0x605988, 0x60598c, 0x60998c))
        return '\n'.join(out + self.aim_lines + self.lead_lines) + f'\nlockshown {shown:x}\nsaved {saved}\n'


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
    probe.probe_hud_classes_clear()
    probe.probe_hud_free_after(1 if orig.free else 0)
    for o, c in getattr(orig, 'classed', {}).items():
        probe.probe_hud_entity_class(o, c)
    buf = ctypes.create_string_buffer(1 << 20)
    places = (ctypes.c_float * (2 * len(orig.places) + 1))(*[v for p in orig.places for v in p])
    ops = (ctypes.c_uint32 * (2 * len(orig.ops) + 1))(*[x for op in orig.ops for x in op])
    n = probe.probe_hud_build(orig.hud, orig.mech, orig.path, places, len(orig.places), orig.utab, orig.stab, ops,
                              len(orig.ops), buf, len(buf))
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
    missing = [v for k, v in CLASS_SLOTS.items() if k not in CLASS_REACHED]
    if missing and seeds >= 20:
        print('classed objects never ran: ' + ', '.join(missing))
        sys.stdout.flush()
        os._exit(1)
    print(f'{seeds} HUDs built, {calls} calls: the rebuilt HUD matches the original '
          f'({len(CLASS_REACHED)} of the classed objects\' slots run; {STATIC_NULL[1]} static fills, '
          f'{STATIC_NULL[0]} TARGET reads without a rect skipped as the port; {STATIC_NULL[2]} zoom renders, '
          f'{STATIC_NULL[3]} mask fans)')
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
