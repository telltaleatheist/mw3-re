"""The objectives' reading against the original x86 (docs/objectives.md):
objectives_load 0x45c2d0, the block 0x45c340, a group 0x45bf60, the mobile
field base 0x45bc90, the condition parser 0x45c920 for the campaign's types
(destroy_entity, node_change, reach_area, valve_change, obj_list), the
salvage 0x4711a0 and cond_salvage 0x45e9f0, the node lookup 0x45c810, the
free 0x45c1d0 / 0x45c250.

    cmake --build build && .venv/bin/python tests/differential/test_objectives.py

Every block of every mission's objectives.zrd, as a campaign and as
multiplayer, then a few hundred made-up blocks that reach what the shipped
ones do not. Both sides read the same bytes through the same fake engine
(entities, nav points, nodes, valves, sounds, messages; tests/
probe_objectives.cpp); the objects they make and their logs of calls must
match, training's (t1) types included. Load-time code: the x87 at its
power-on precision.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from ai_world import ROOT, ZBD, Emulated, archive, fnv, ser  # noqa: E402
import zrd  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
probe.probe_objectives_run.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int,
                                       ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p, ctypes.c_int,
                                       ctypes.c_void_p, ctypes.c_int]
WORK = ROOT / 'build' / 'objectives'
CAP = 1 << 20
PILOT, GAME, NAVS, POINT = 0x60861c, 0x6c, 0x609980, 0x63d870
VT_KIND = {0x5c2678: 0, 0x5c2668: 1, 0x5c2730: 2, 0x5c2728: 3, 0x5c26f8: 4,
           # training's (piece 5)
           0x5c2720: 5, 0x5c2718: 6, 0x5c2708: 7, 0x5c2700: 8, 0x5c26e8: 9, 0x5c26d8: 10, 0x5c26d0: 11,
           0x5c26c8: 12, 0x5c26c0: 13, 0x5c26b8: 14, 0x5c26b0: 15, 0x5c26a8: 16, 0x5c26a0: 17, 0x5c2698: 18}
ITEM_VT = {0x5bc160: 0, 0x5c0fc0: 1, 0x5bd150: 2, 0x5bd180: 3, 0x5c3700: 4}
COVER = {0x45ca93: 'destroy desc', 0x45cbf5: 'destroy group', 0x45ce47: 'node_change active',
         0x45cf0d: 'reach node', 0x45cf9a: 'reach nav', 0x45d034: 'reach point', 0x45d53d: 'mp valve reset',
         0x45dd40: 'obj_list child', 0x45ca22: 'set_valve', 0x45bcfa: 'mfb outer group',
         0x45be25: 'mfb part', 0x45c8a5: 'subnode missing', 0x45c858: 'node missing',
         0x4713e9: 'engines', 0x4712b0: 'weapons', 0x471254: 'mechs', 0x45eaef: 'node_state',
         0x45c206: 'training free', 0x45c216: 'free',
         0x45d2b3: 'reach_velocity', 0x45d343: 'aim_at_object', 0x45d41c: 'target_object', 0x45d4c7: 'time_limit',
         0x45d67d: 'view_object', 0x45d735: 'face_object', 0x45d79f: 'heat_warning', 0x45d872: 'User_Command',
         0x45d91e: 'joystick_press', 0x45daf4: 'key_press', 0x45dbef: 'mouse_press', 0x45de73: 'weapon_fire',
         0x45df27: 'weapon_select', 0x45dfc9: 'weapon_group_select', 0x45d9fb: 'KEY_CTRL', 0x45da39: 'KEY_ALT',
         0x45d494: 'time_limit int'}


def h32(v):
    return f'{v & 0xffffffff:x}'


class Run(Emulated):
    def __init__(self):
        super().__init__()
        cpu = self.cpu
        stubs = {
            0x576040: (self.message, 0),
            0x575fd0: (self.message_with, 0),
            0x4d7330: (self.entity_find, 0),
            0x479be0: (self.nav_find, 4),
            0x47a270: (self.nav_show, 8),
            0x4f7fe0: (self.node_find, 0),
            0x4f9d60: (self.subnode_find, 0),
            0x4d3c40: (self.valve_find, 0),
            0x4d3c00: (self.valve_set, 4),
            0x556820: (self.free_tree, 0),
            # Training's lookups: commands, keys (probe_objectives.cpp's).
            0x410fe0: (self.command_find, 0), 0x51d1f0: (self.command_key, 0), 0x51d220: (self.command_joy, 0),
            0x51d230: (self.command_mouse, 0), 0x51aee0: (self.key_by_name, 0),
        }
        for addr, (fn, pop) in stubs.items():
            cpu.stub_function(addr, fn, pop)
        ret = cpu.code(b'\xc3')
        cpu.stub_function(ret, self.label, 0)
        self.entity_vt = self.calloc(0x100)
        cpu.u32(self.entity_vt + 0xa4, ret)
        game_vt = self.calloc(0x100)
        t, f = cpu.code(b'\xc3'), cpu.code(b'\xc3')
        cpu.stub_function(t, self.game_type, 0)
        cpu.stub_function(f, lambda c: self.ev('trainfree'), 0)
        cpu.u32(game_vt + 0x2c, t)
        cpu.u32(game_vt + 0x44, f)
        self.game = self.calloc(0x10)
        cpu.u32(self.game, game_vt)
        self.pilot = self.calloc(0x100)
        cpu.u32(self.pilot + GAME, self.game)
        self.heap0 = self.next
        self.cover(COVER)

    # --- the fakes (probe_objectives.cpp's) ----------------------------------
    def ev(self, s):
        self.lines.append(s)
        return 0

    def text(self, p):
        s = self.cstr(p)
        return '(null)' if s is None else s.decode('latin-1')

    def keep(self, s):
        return self.new_string(s.encode('latin-1'))

    def localize(self, c):
        self.ev(f'loc {self.text(c.ecx)}')
        return self.keep('L:' + self.text(c.ecx)) if c.ecx else 0

    def message(self, c):
        self.ev(f'msg {h32(c.ecx)}')
        return self.keep(f'M{h32(c.ecx)}')

    def message_with(self, c):
        buf, size, mid, s = c.arg(0), c.arg(1), c.arg(2), self.text(c.arg(3))
        assert size == 0x40
        self.ev(f'msgw {h32(mid)} {s}')
        self.cpu.write(buf, (f'F{h32(mid)}:{s}'[:63]).encode('latin-1') + b'\0')
        return 1

    def sound(self, c):
        self.ev(f'snd {self.text(c.ecx)}')
        return 0x5000000 | (fnv(self.cstr(c.ecx)) & 0xffffff) if c.ecx else 0

    def entity_find(self, c):
        self.ev(f'ent {self.text(c.ecx)}')
        if not c.ecx:
            return 0
        h = fnv(self.cstr(c.ecx))
        if h % 5 == 0:
            return 0
        e = 0x6000000 | (h & 0xffffff)
        if e not in self.entities:
            o = self.calloc(0x10)
            self.cpu.u32(o, self.entity_vt)
            self.cpu.write(o + 8, bytes([int(e & 0xf0 != 0)]))
            self.entities[e] = o
            self.addr_of[o] = e
        return self.entities[e]

    def label(self, c):
        e = self.addr_of[c.ecx]
        self.ev(f'label {h32(e)}')
        return self.keep(f'E{h32(e)}') if (e >> 4) & 3 else 0

    def nav_find(self, c):
        assert c.ecx == self.u32(NAVS)
        name = self.cstr(c.arg(0))
        self.ev(f'nav {name.decode("latin-1")}')
        h = fnv(name)
        return 0 if h % 4 == 0 else 0x7000000 | (h & 0xffffff)

    def nav_show(self, c):
        return self.ev(f'navshow {h32(self.handle(c.arg(0)))} {h32(c.arg(1) & 0xff)}')

    def node_find(self, c):
        assert c.ecx == 6
        name = self.cstr(c.edx)
        self.ev(f'node {name.decode("latin-1")}')
        h = fnv(name)
        return 0 if h % 6 == 0 else 0x8000000 | (h & 0xffffff)

    def subnode_find(self, c):
        name = self.cstr(c.edx)
        self.ev(f'sub {h32(c.ecx)} {name.decode("latin-1")}')
        h = fnv(name) ^ c.ecx
        return 0 if h % 6 == 0 else 0x9000000 | (h & 0xffffff)

    def valve_find(self, c):
        name = self.cstr(c.ecx)
        self.ev(f'valve {name.decode("latin-1")}')
        h = fnv(name)
        return 0 if h % 7 == 0 else 0xa000000 | (h & 0xffffff)

    def valve_set(self, c):
        return self.ev(f'vset {h32(self.handle(c.ecx))} {h32(c.arg(0))}')

    def command_find(self, c):
        self.ev(f'cmd {self.text(c.ecx)}')
        return fnv(self.cstr(c.ecx)) % 40

    def command_key(self, c):
        v = c.ecx - (1 << 32) if c.ecx >> 31 else c.ecx
        self.ev(f'ckey {h32(v)}')
        return ((v * 3) % 5 - 1) & 0xffffffff  # C's % (v >= 0 here)

    def command_joy(self, c):
        self.ev(f'cjoy {h32(c.ecx)}')
        return 0 if c.ecx % 3 == 0 else c.ecx

    def command_mouse(self, c):
        self.ev(f'cmouse {h32(c.ecx)}')
        return c.ecx % 3 if c.ecx % 4 == 0 else 0

    def key_by_name(self, c):
        self.ev(f'key {self.text(c.ecx)}')
        h = fnv(self.cstr(c.ecx))
        return 0xffffffff if h % 5 == 0 else h % 200

    def game_type(self, c):
        self.ev('type')
        return self.type & 0xffffffff

    def zerr(self, c):
        return self.ev(f'zerr {h32(c.arg(0))} {h32(c.arg(2))}')

    def load(self, c):
        name = self.cstr(c.ecx)
        self.ev(f'load {name.decode("latin-1")}')
        trees = self.trees.get(name.decode('latin-1').lower())
        return trees.pop(0) if trees else 0

    def free_tree(self, c):
        return self.ev('free')

    # --- a run ----------------------------------------------------------------
    def run(self, files, block, gtype, point, free):
        cpu = self.cpu
        self.next = self.heap0
        self.lines, self.entities, self.addr_of, self.type = [], {}, {}, gtype
        self.trees = {k.lower(): [self.build(v)] for k, v in files.items()}
        cpu.u32(PILOT, self.pilot)
        cpu.u32(NAVS, 0x1000)
        cpu.write(POINT, struct.pack('<3f', *point))
        out = self.calloc(0x14)
        fname, bname = self.new_string(b'objectives.zrd'), self.new_string(block.encode())
        cpu.call(0x45c2d0, ecx=fname, edx=bname, stack_args=[out + 4 * i for i in range(5)])
        ok = self.eax() & 0xff
        text = 'desc ' + self.opt(self.u32(out)) + '\n'
        for i, name in enumerate(('primary', 'secondary', 'final', 'fail')):
            text += name + '\n'
            p = self.u32(out + 4 + 4 * i)
            if p:
                text += self.obj(p, 1)
        if free:
            cpu.call(0x45c1d0, ecx=out, edx=out + 4, stack_args=[out + 8, out + 12, out + 16])
            left = any(self.u32(out + 4 * i) for i in range(5))
            text += f'freed {"no" if left else "yes"}\n'
        return ok, text, '\n'.join(self.lines) + ('\n' if self.lines else '')

    def opt(self, p):
        s = self.cstr(p)
        return "-" if s is None else "'" + s.decode('latin-1') + "'"

    def items(self, head):
        p, out = self.u32(head), []
        while p != head:
            out.append(self.u32(p + 8))
            p = self.u32(p)
        return out

    def store(self, s):
        if not s:
            return ' S-'
        o = f' S[b{h32(self.u8(s + 4))} f{h32(self.u8(s + 0x10))}'
        for i in self.items(self.u32(s + 8)):
            k = ITEM_VT[self.u32(i)]
            ident = self.u32(i + (0x18 if k == 3 else 0x14))
            o += ' ' + ':'.join(h32(v) for v in (k, self.u32(i + 4), self.u32(i + 8), self.u32(i + 0xc),
                                                  self.u8(i + 0x10), ident))
            if k == 1:
                o += ':' + h32(self.u32(i + 0x18))
            if k == 3:
                o += ':' + h32(self.u32(i + 0x1c))
            if k == 4:
                o += ':' + h32(self.u8(i + 0x18))
        return o + ']'

    def handle(self, v):
        return self.addr_of.get(v, v)

    def obj(self, p, depth):
        vt = self.u32(p)
        k = VT_KIND[vt]
        u, b = self.u32, self.u8
        o = (' ' * (2 * depth) + f'K{h32(k)} nav{h32(self.handle(u(p + 4)))} d{h32(b(p + 8))} {self.opt(u(p + 0xc))} '
             f'v{h32(self.handle(u(p + 0x24)))}={h32(u(p + 0x28))} snd{h32(u(p + 0x20))} vis{h32(b(p + 0x30))} '
             f'sil{h32(b(p + 0x31))} per{self.opt(u(p + 0x2c))}')
        o += self.store(u(p + 0x10))
        for e in self.items(u(p + 0x18)):
            o += f' N{h32(self.handle(u(e)))}/{h32(b(e + 4))}' + self.store(u(e + 8))
        if k == 0:
            o += f' | {h32(b(p + 0x34))} {h32(b(p + 0x35))} {h32(u(p + 0x38))} {h32(u(p + 0x3c))}\n'
            for c in self.items(u(p + 0x44)):
                o += self.obj(c, depth + 1)
            return o
        if k == 1:
            o += f' | {self.opt(u(p + 0x34))} {h32(self.handle(u(p + 0x38)))} {h32(b(p + 0x3c))}'
        elif k == 2:
            o += f' | {h32(self.handle(u(p + 0x34)))} {h32(b(p + 0x38))}'
        elif k == 3:
            o += f' | {h32(self.handle(u(p + 0x34)))} ' + ' '.join(h32(u(p + off)) for off in (0x38, 0x3c, 0x40, 0x44, 0x48))
        elif k == 4:
            o += f' | {h32(self.handle(u(p + 0x34)))} {h32(u(p + 0x38))}'
        elif k in (5, 12):
            o += ' | ' + ' '.join(h32(u(p + off)) for off in ((0x34, 0x38) if k == 5 else (0x34, 0x38, 0x3c)))
        elif k == 6:
            o += f' | {h32(self.handle(u(p + 0x34)))}'
        elif k == 7:
            o += f' | {self.opt(u(p + 0x34))} {h32(self.handle(u(p + 0x38)))} {h32(u(p + 0x3c))}'
        elif k in (9, 10):
            o += f' | {self.opt(u(p + 0x34))} {h32(self.handle(u(p + 0x38)))}'
        elif k == 11:
            o += ' |'
        else:
            o += f' | {h32(u(p + 0x34))}'
        return o + '\n'


def port(files, block, gtype, point, free):
    WORK.mkdir(parents=True, exist_ok=True)
    path = WORK / 'objectives.zbd'
    path.write_bytes(archive(files))
    out, log = ctypes.create_string_buffer(CAP), ctypes.create_string_buffer(CAP)
    pt = (ctypes.c_float * 3)(*point)
    r = probe.probe_objectives_run(str(path).encode(), b'objectives.zrd', block.encode(), gtype, pt, int(free),
                                   out, CAP, log, CAP)
    assert r >= 0, r
    return r, out.value.decode('latin-1'), log.value.decode('latin-1')


def shipped():
    """(name, objectives.zrd bytes, its blocks) of every mission archive."""
    out = []
    for p in sorted(ZBD.glob('*/reader.zbd')):
        data = p.read_bytes()
        for name, start, length in zrd.entries(data):
            if name.lower() == 'objectives.zrd':
                body = data[start:start + length]
                root, _ = zrd.node(body, 0)
                top = root[0]
                blocks = [top[i] for i in range(0, len(top), 2) if isinstance(top[i], str)]
                out.append((p.parent.name, body, blocks))
    return out


# --- made-up blocks ------------------------------------------------------------
MECHS = ['madcat', 'vulture', 'thor', 'nosuchmech']
WEAPONS = ['ERPPC', 'GaussRifle', 'LRM20', 'AC10', 'lb2xac', 'nosuchgun']
EQUIP = ['tag', 'ams', 'sink', 'jumpjet', 'nothing']
TECH = ['clan', 'sphere', 'x']


def salvage(r):
    s = []
    if r.random() < 0.5:
        s += ['mechs', [r.choice(MECHS) for _ in range(r.randint(0, 3))] + ([7] if r.random() < 0.2 else [])]
    if r.random() < 0.5:
        s += ['equipment', [[r.choice(EQUIP), r.choice(TECH)] if r.random() < 0.8 else [r.choice(EQUIP)]
                            for _ in range(r.randint(0, 3))]]
    if r.random() < 0.5:
        s += ['weapons', [[r.choice(WEAPONS), r.choice(TECH)] if r.random() < 0.7 else r.choice(WEAPONS)
                          for _ in range(r.randint(0, 3))]]
    if r.random() < 0.5:
        s += ['ammo', [[r.choice(WEAPONS), r.choice([0, 5, 20, -3])] for _ in range(r.randint(0, 3))]]
    if r.random() < 0.3:
        s += [r.choice(['engines', 'engine']), [[r.choice([100, 120, 300, 5, 500])] + (['xl'] if r.random() < 0.5
                                                                                    else [])
                                               for _ in range(r.randint(1, 2))]]
    return s


def node_list(r):
    return [f'n{r.randint(0, 30)}' for _ in range(r.randint(1, 3))]


def shared(r, a, names=False):
    """The keys every type reads. For destroy_entity (`names`), nav, sound and
    persist go last with plain-string values: the original reads the slots of
    those it does not count as entity names, and a list there would be read
    as a string (the port's FIX (A) takes none)."""
    if r.random() < 0.6:
        a += ['desc', [r.choice([f'MSG_{r.randint(0, 9)}', 'A\nB', '\n\nC\n\nD\n', '\n'])]]
    if r.random() < 0.3:
        a += ['visible', [r.choice([0, 1, 2])]]
    if r.random() < 0.2:
        a += ['silent', [r.choice([0, 1])]]
    if r.random() < 0.2:
        a += ['salvage', salvage(r)]
    if r.random() < 0.15:
        a += ['cond_salvage', sum((['node_state', ['node', node_list(r), 'state', [r.choice(['active', 'dead'])],
                                                    'salvage', salvage(r)]] for _ in range(r.randint(1, 2))), [])]
    if r.random() < 0.15:
        a += ['set_valve', r.choice([[f'v{r.randint(0, 20)}', r.randint(0, 3)], [f'v{r.randint(0, 9)}'],
                                     [f'v{r.randint(0, 9)}', 1.5]])]
    tail = []
    if r.random() < 0.3:
        tail += [['nav', f'nav{r.randint(0, 12)}']]
    if r.random() < 0.2:
        tail += [['sound', f'snd{r.randint(0, 9)}']]
    if r.random() < 0.1:
        tail += [['persist', f'p{r.randint(0, 9)}']]
    if not names:
        r.shuffle(tail)
        tail = [[k, [v]] for k, v in tail]
    for k, v in tail:
        a += [k, v]
    return a


TRAINING = ['reach_velocity', 'aim_at_object', 'target_object', 'time_limit', 'view_object', 'face_object',
            'heat_warning', 'User_Command', 'joystick_press', 'key_press', 'mouse_press', 'weapon_fire',
            'weapon_select', 'weapon_group_select']


def training_args(r, t):
    if t == 'reach_velocity':
        return ((['test', [r.choice(['GT', 'GE', 'EQ', 'NE', 'LE', 'LT', 'XX'])]] if r.random() < 0.8 else []) +
                (['velocity', [r.choice([0, 5, 12.5, -3.0])]] if r.random() < 0.85 else []))
    if t == 'aim_at_object':
        return ['node', node_list(r)] if r.random() < 0.9 else []
    if t in ('target_object', 'view_object', 'face_object'):
        a = ['entity', [f'ent{r.randint(0, 40)}']] if r.random() < 0.9 else []
        if t == 'target_object' and r.random() < 0.6:
            a += ['part', [r.choice(['head', 'larm', 'rtorso', 'nopart'])]]
        return a
    if t == 'time_limit':
        return [r.choice([30, 12.5, 'x', [4]])] if r.random() < 0.85 else []
    if t == 'User_Command':
        return ['cmd', [r.choice(['CMD_FIRE', 'CMD_JUMP', 'CMD_X', 'CMD_ZOOM'])]] if r.random() < 0.9 else ['cmd', [3]]
    if t == 'joystick_press':
        return ['button', [r.choice([0, 1, 4, 'one'])]] if r.random() < 0.9 else []
    if t == 'key_press':
        return ['key', [r.choice(['KEY_CTRL', 'KEY_ALT', 'KEY_SHIFT', 'KEY_FOO', 'a', 'F12', 'space', 7])
                        for _ in range(r.randint(0, 3))]] if r.random() < 0.9 else []
    if t == 'mouse_press':
        return ['button', [r.choice(['left', 'RIGHT', 'Middle', 'side', 2])]] if r.random() < 0.9 else []
    if t in ('weapon_fire', 'weapon_select'):
        return ['weapon', [r.choice(['ERPPC', 'LRM20', 'PPC', 'TAG', 'UltraAC20', 'nosuch', 3])]] if r.random() < 0.9 else []
    if t == 'weapon_group_select':
        return ['group', [r.choice([0, 1, 3, 5, 6, 'g'])]] if r.random() < 0.9 else []
    return []


def condition(r, depth=0):
    t = r.choice(['destroy_entity'] * 3 + ['node_change', 'reach_area', 'valve_change', 'obj_list', 'bogus'] +
                 TRAINING * (r.random() < 0.5))
    if depth > 2 and t == 'obj_list':
        t = 'destroy_entity'
    a = shared(r, [], t == 'destroy_entity')
    if t == 'destroy_entity':
        a += [f'ent{r.randint(0, 40)}' for _ in range(r.randint(0, 4))]
    elif t == 'node_change':
        if r.random() < 0.9:
            a += ['node', node_list(r)]
        if r.random() < 0.8:
            a += ['state', [r.choice(['active', 'inactive'])]]
    elif t == 'reach_area':
        if r.random() < 0.5:
            a += ['radius', [r.choice([10, 40.5, 0.25])]]
        if r.random() < 0.4:
            a += ['node', node_list(r)]
        if r.random() < 0.5:
            a += ['point', [1.5, -2.0, r.choice([3, 4.25])]]
    elif t == 'valve_change':
        if r.random() < 0.9:
            a += ['valve', [f'v{r.randint(0, 20)}']]
        if r.random() < 0.6:
            a += ['value', [r.randint(-1, 3)]]
    elif t in TRAINING:
        a += training_args(r, t)
    elif t == 'obj_list':
        if r.random() < 0.3:
            a += ['goal_count', [r.randint(0, 3)]]
        if r.random() < 0.3:
            a += ['simultaneous', [r.choice([0, 1])]]
        a += ['list', sum(([*condition(r, depth + 1)] for _ in range(r.randint(0, 3))), [])]
    return [t, a]


def group(r):
    g = []
    if r.random() < 0.4:
        g += ['desc', f'GDESC{r.randint(0, 5)}']
    if r.random() < 0.2:
        g += ['sound', f'gs{r.randint(0, 5)}']
    if r.random() < 0.2:
        g += ['visible', r.choice([0, 1, 5])]
    if r.random() < 0.2:
        g += ['silent', r.choice([0, 1])]
    for _ in range(r.randint(0, 3)):
        g += condition(r)
    return g


def made_up(r):
    block = []
    if r.random() < 0.7:
        block += ['desc', [f'BDESC{r.randint(0, 3)}']]
    for key in ('primary', 'secondary', 'final', 'fail'):
        if r.random() < 0.85:
            block += [key, group(r)]
    return ser([['m1', block, 'm2', ['primary', ['valve_change', ['valve', ['vx']]]]]])


def compare(emu, files, block, gtype, point, free, tag, must_match=True):
    eok, etext, elog = emu.run(files, block, gtype, point, free)
    pok, ptext, plog = port(files, block, gtype, point, free)
    if not must_match:
        return eok, etext, elog, pok, ptext, plog
    if (eok, etext, elog) != (pok, ptext, plog):
        print(f'MISMATCH {tag}: ok {eok} vs {pok}')
        for name, a, b in (('objects', etext, ptext), ('log', elog, plog)):
            al, bl = a.splitlines(), b.splitlines()
            for i in range(max(len(al), len(bl))):
                x, y = (al[i] if i < len(al) else '<end>'), (bl[i] if i < len(bl) else '<end>')
                if x != y:
                    print(f'  {name} line {i}:\n    exe:  {x}\n    port: {y}')
                    break
        return False
    return True


def main():
    emu = Run()
    fails = 0
    checked = 0
    pt = (10.5, -3.0, 250.0)
    for mission, body, blocks in shipped():
        files = {'objectives.zrd': body}
        types = (4, 1) if mission == 't1' else (1, 3)
        print(f'{mission}: {len(blocks)} blocks', flush=True)
        for b in blocks:
            for gt in types:
                ok = compare(emu, files, b, gt, pt, gt == 3, f'{mission} {b} type {gt}')
                fails += not ok
                checked += 1
    print(f'shipped: {checked} block runs')
    r = random.Random(int(os.environ.get('SEED', '1')))
    n = int(os.environ.get('N', '400'))
    for i in range(n):
        data = made_up(r)
        gt = r.choice([1, 1, 3, 0, 4, -1, -2])
        ok = compare(emu, {'objectives.zrd': data}, r.choice(['m1', 'm1', 'm2', 'm9']), gt,
                     (r.uniform(-100, 100), 0.0, r.uniform(-5, 5)), r.random() < 0.5, f'made-up {i} type {gt}')
        fails += not ok
        if fails > 5:
            break
    missing = sorted(v for k, v in COVER.items() if k not in emu.reached)
    if missing:
        print('not reached:', ', '.join(missing))
    print('FAIL' if fails or missing else 'PASS', f'({fails} mismatches)')
    return 1 if fails or missing else 0


if __name__ == '__main__':
    code = main()
    sys.stdout.flush()
    os._exit(code)
