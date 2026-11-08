"""PM's objectives' reading against its exe: test_objectives moved. The block 0x45d620 (MW3's 0x45c340, now given the
block's node by its callers, which load the file and find it: 0x43518c), a group 0x45d280, the mobile field base
0x45cde0, the condition parser 0x45dc00, the salvage 0x473690 and cond_salvage 0x460570, the node lookup 0x45daf0 and
the free 0x45d530 / 0x45d5a0, on PM's own addresses and conventions.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_objectives.py

Every block of every objectives.zrd PM ships (each mission's, Instant Action's and the standalone campaign's archives
in c1..c4), as a campaign and as multiplayer, then a few hundred made-up blocks. Both sides read the same bytes
through the same fake engine (test_objectives.py's, with PM's slots: an entity's label at vt+0xb0, the game's type at
vt+0x28 and training's free at vt+0x40); the objects they make, PM's new fields with them (+0x09 failed, +0x24 the
fail sound, a group's fails +0x54), and their logs of calls must match (tests/probe_objectives.cpp's
probe_pm_objectives_run). Load-time code: the x87 at 53 bits.
"""
import ctypes
import os
import random
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools' / 'analysis'))
os.environ.setdefault('MW3_X87', '53')
import pm_floors  # noqa: E402
import test_pm_zar  # noqa: E402
import test_pm_zreader as zr  # noqa: E402
import zrd  # noqa: E402
from ai_world import IAT, fnv, ser  # noqa: E402
from original import ROOT  # noqa: E402

probe = test_pm_zar.load_pm_probe()
probe.probe_pm_objectives_run.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int,
                                          ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p, ctypes.c_int,
                                          ctypes.c_void_p, ctypes.c_int]
PM = ROOT / 'game' / 'pm'
WORK = ROOT / 'build' / 'pm_objectives'
CAP = 1 << 20
BLOCK, FREE, FREE_TREE, FILE = 0x45d620, 0x45d530, 0x5787a0, 0x605588
PILOT, GAME, NAVS, POINT = 0x62c40c, 0x6c, 0x62daf0, 0x66f5e0
VT_KIND = {0x5e4ccc: 0, 0x5e4cc0: 1, 0x5e4d6c: 2, 0x5e4d64: 3, 0x5e4d38: 4,
           0x5e4d5c: 5, 0x5e4d54: 6, 0x5e4d48: 7, 0x5e4d40: 8, 0x5e4d2c: 9, 0x5e4d20: 10, 0x5e4d18: 11,
           0x5e4d10: 12, 0x5e4d08: 13, 0x5e4d00: 14, 0x5e4cf8: 15, 0x5e4cf0: 16, 0x5e4ce8: 17, 0x5e4ce0: 18}
ITEM_VT = {0x5dee2c: 0, 0x5e36b4: 1, 0x5dfc40: 2, 0x5dfc70: 3}  # PM's stores have no engine item
# MW3's test_objectives branches that pair with PM's instructions (the rest, with no twin, by what's made: SEEN).
COVER = {0x45ce51: 'mfb outer group', 0x45d55f: 'free', 0x45db43: 'node missing', 0x45db91: 'subnode missing',
         0x45e1b8: 'node_change active', 0x45e341: 'reach nav', 0x45e923: 'mp valve reset', 0x45edf4: 'KEY_CTRL',
         0x45ee32: 'KEY_ALT', 0x45f269: 'obj_list child', 0x460670: 'node_state'}


def h32(v):
    return f'{v & 0xffffffff:x}'


class Run(zr.Emulated):
    def __init__(self):
        super().__init__()
        cpu = self.cpu
        for name, fn in (('calloc', lambda c: self.calloc(c.arg(0) * c.arg(1))), ('strtol', self.strtol),
                         ('strrchr', self.strrchr), ('strncmp', self.strncmp), ('sprintf', self.sprintf),
                         ('snprintf', self.snprintf)):
            cpu.stub_import(IAT[name], fn, 0)
        stubs = {
            0x596e50: self.message, 0x596de0: self.message_with, 0x596db0: self.localize, 0x590a70: self.sound,
            0x4e77d0: self.entity_find, 0x50b920: self.node_find, 0x50da50: self.subnode_find,
            0x4e4100: self.valve_find, FREE_TREE: self.free_tree, 0x406cb0: self.zerr,
            0x411940: self.command_find, 0x536a60: self.command_key, 0x536ac0: self.command_joy,
            0x536ae0: self.command_mouse, 0x534700: self.key_by_name,
        }
        for addr, fn in stubs.items():
            self.native_stub(addr, fn)
        self.native_stub(0x47bf10, self.nav_find, 4)
        self.native_stub(0x47c560, self.nav_show, 8)
        self.native_stub(0x4e40d0, self.valve_set, 4)
        self.entity_vt = self.calloc(0x100)
        self.w(self.entity_vt + 0xb0, self.ret(self.label))
        game_vt = self.calloc(0x100)
        self.w(game_vt + 0x28, self.ret(lambda c: (self.ev('type'), self.type & 0xffffffff)[1]))
        self.w(game_vt + 0x40, self.ret(lambda c: self.ev('trainfree')))
        self.game = self.calloc(0x10)
        self.w(self.game, game_vt)
        self.pilot = self.calloc(0x100)
        self.w(self.pilot + GAME, self.game)
        self.reached = set()
        for a in COVER:
            self.cpu._uc.hook_add(1 << 2, lambda uc, ad, sz, u: self.reached.add(ad), begin=a, end=a)

    # --- memory ----------------------------------------------------------------
    def w(self, a, v):
        self.cpu._uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def u32(self, a):
        return self.r32(a)

    def u8(self, a):
        return bytes(self.cpu._uc.mem_read(a, 1))[0]

    def calloc(self, n):
        return self.alloc(max(n, 1))

    def ret(self, fn, pop=0):
        t = self.cpu.code(b'\xc3')
        self.native_stub(t, fn, pop)
        return t

    def arg(self, c, i):
        return c.arg(i)

    def text(self, p):
        s = self.cstr(p) if p else None
        return '(null)' if s is None else s.decode('latin-1')

    def keep(self, s):
        return self.string(s)

    # --- the C runtime (ai_world's) ----------------------------------------------
    def strtol(self, c):
        p, end, base = c.arg(0), c.arg(1), c.arg(2)
        assert base == 10
        s = self.cstr(p)
        i = 0
        while i < len(s) and s[i] in b' \t\n\r\f\v':
            i += 1
        j = i + (1 if i < len(s) and s[i] in b'+-' else 0)
        k = j
        while k < len(s) and 0x30 <= s[k] <= 0x39:
            k += 1
        if k == j:
            v, k = 0, 0
        else:
            v = max(-0x80000000, min(0x7fffffff, int(s[i:k])))
        if end:
            self.w(end, p + k)
        return v & 0xffffffff

    def strrchr(self, c):
        p, ch = c.arg(0), c.arg(1) & 0xff
        s = self.cstr(p)
        i = s.rfind(bytes([ch])) if ch else len(s)
        return 0 if i < 0 else p + i

    def strncmp(self, c):
        a, b = self.cstr(c.arg(0))[:c.arg(2)], self.cstr(c.arg(1))[:c.arg(2)]
        return ((a > b) - (a < b)) & 0xffffffff

    def format(self, fmt, raw):
        args, k, i = [], 0, 0
        while i < len(fmt):
            if fmt[i] == 0x25:
                j = i + 1
                while fmt[j] in b'0123456789.-':
                    j += 1
                if fmt[j] != 0x25:
                    args.append(self.cstr(raw[k]) if fmt[j] == 0x73 else raw[k])
                    k += 1
                i = j + 1
            else:
                i += 1
        from ai_world import printf
        return printf(fmt, args)

    def sprintf(self, c):
        d, fmt = c.arg(0), self.cstr(c.arg(1))
        out = self.format(fmt, [c.arg(k) for k in range(2, 10)])
        self.cpu._uc.mem_write(d, out + b'\0')
        return len(out)

    def snprintf(self, c):
        d, n, fmt = c.arg(0), c.arg(1), self.cstr(c.arg(2))
        out = self.format(fmt, [c.arg(k) for k in range(3, 11)])
        self.cpu._uc.mem_write(d, (out + b'\0')[:n])
        return len(out) if len(out) < n else -1

    # --- the fakes (probe_objectives.cpp's) ----------------------------------
    def ev(self, s):
        self.lines.append(s)
        return 0

    def localize(self, c):
        p = c.arg(0)
        self.ev(f'loc {self.text(p)}')
        return self.keep('L:' + self.text(p)) if p else 0

    def message(self, c):
        self.ev(f'msg {h32(c.arg(0))}')
        return self.keep(f'M{h32(c.arg(0))}')

    def message_with(self, c):
        buf, size, mid, s = c.arg(0), c.arg(1), c.arg(2), self.text(c.arg(3))
        assert size == 0x40
        self.ev(f'msgw {h32(mid)} {s}')
        self.cpu._uc.mem_write(buf, (f'F{h32(mid)}:{s}'[:63]).encode('latin-1') + b'\0')
        return 1

    def sound(self, c):
        p = c.arg(0)
        self.ev(f'snd {self.text(p)}')
        return 0x5000000 | (fnv(self.cstr(p)) & 0xffffff) if p else 0

    def entity_find(self, c):
        p = c.arg(0)
        self.ev(f'ent {self.text(p)}')
        if not p:
            return 0
        h = fnv(self.cstr(p))
        if h % 5 == 0:
            return 0
        e = 0x6000000 | (h & 0xffffff)
        if e not in self.entities:
            o = self.calloc(0x10)
            self.w(o, self.entity_vt)
            self.cpu._uc.mem_write(o + 8, bytes([int(e & 0xf0 != 0)]))
            self.entities[e] = o
            self.addr_of[o] = e
        return self.entities[e]

    def label(self, c):
        e = self.addr_of[c.ecx]
        self.ev(f'label {h32(e)}')
        return self.keep(f'E{h32(e)}') if (e >> 4) & 3 else 0

    def nav_find(self, c):
        assert c.ecx == self.r32(NAVS)
        name = self.cstr(c.arg(0))
        self.ev(f'nav {name.decode("latin-1")}')
        h = fnv(name)
        return 0 if h % 4 == 0 else 0x7000000 | (h & 0xffffff)

    def nav_show(self, c):
        return self.ev(f'navshow {h32(self.handle(c.arg(0)))} {h32(c.arg(1) & 0xff)}')

    def node_find(self, c):
        assert c.arg(0) == 6
        name = self.cstr(c.arg(1))
        self.ev(f'node {name.decode("latin-1")}')
        h = fnv(name)
        return 0 if h % 6 == 0 else 0x8000000 | (h & 0xffffff)

    def subnode_find(self, c):
        node, name = c.arg(0), self.cstr(c.arg(1))
        self.ev(f'sub {h32(node)} {name.decode("latin-1")}')
        h = fnv(name) ^ node
        return 0 if h % 6 == 0 else 0x9000000 | (h & 0xffffff)

    def valve_find(self, c):
        name = self.cstr(c.arg(0))
        self.ev(f'valve {name.decode("latin-1")}')
        h = fnv(name)
        return 0 if h % 7 == 0 else 0xa000000 | (h & 0xffffff)

    def valve_set(self, c):
        return self.ev(f'vset {h32(self.handle(c.ecx))} {h32(c.arg(0))}')

    def command_find(self, c):
        self.ev(f'cmd {self.text(c.arg(0))}')
        return fnv(self.cstr(c.arg(0))) % 40

    def command_key(self, c):
        v = c.arg(0) - (1 << 32) if c.arg(0) >> 31 else c.arg(0)
        self.ev(f'ckey {h32(v)}')
        return ((v * 3) % 5 - 1) & 0xffffffff

    def command_joy(self, c):
        self.ev(f'cjoy {h32(c.arg(0))}')
        return 0 if c.arg(0) % 3 == 0 else c.arg(0)

    def command_mouse(self, c):
        self.ev(f'cmouse {h32(c.arg(0))}')
        return c.arg(0) % 3 if c.arg(0) % 4 == 0 else 0

    def key_by_name(self, c):
        self.ev(f'key {self.text(c.arg(0))}')
        h = fnv(self.cstr(c.arg(0)))
        return 0xffffffff if h % 5 == 0 else h % 200

    def zerr(self, c):
        return self.ev(f'zerr {h32(c.arg(0))} {h32(c.arg(2))}')

    def free_tree(self, c):
        return self.ev('free')

    def handle(self, v):
        return self.addr_of.get(v, v)

    # --- a run ----------------------------------------------------------------
    def run(self, rel, data, block, gtype, point, free):
        self.files[test_pm_zar.key(rel)] = bytearray(data)
        if not self.call(zr.OPEN_ARCHIVE, args=[self.string(rel), 1]):
            raise SystemExit(f'{rel}: PM does not open it')
        self.lines, self.entities, self.addr_of, self.type = [], {}, {}, gtype
        self.w(PILOT, self.pilot)
        self.w(NAVS, 0x1000)
        self.cpu._uc.mem_write(POINT, struct.pack('<3f', *point))
        out = self.calloc(0x14)
        # The callers (0x435150): the file loaded, the block found, read, the tree freed.
        self.ev('load objectives.zrd')
        root = self.call(zr.LOAD, args=[FILE, 0, 0])
        node = self.call(zr.FIND, args=[root, self.string(block)]) if root else 0
        ok = self.call(BLOCK, args=[node] + [out + 4 * i for i in range(5)]) & 0xff
        if root:
            self.call(FREE_TREE, args=[root])
        text = 'desc ' + self.opt(self.u32(out)) + '\n'
        for i, name in enumerate(('primary', 'secondary', 'final', 'fail')):
            text += name + '\n'
            p = self.u32(out + 4 + 4 * i)
            if p:
                text += self.obj_text(p, 1)
        if free:
            self.call(FREE, args=[out + 4 * i for i in range(5)])
            left = any(self.u32(out + 4 * i) for i in range(5))
            text += f'freed {"no" if left else "yes"}\n'
        return ok, text, '\n'.join(self.lines) + ('\n' if self.lines else '')

    def opt(self, p):
        s = self.cstr(p) if p else None
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
        return o + ']'

    def obj_text(self, p, depth):
        k = VT_KIND[self.u32(p)]
        u, b = self.u32, self.u8
        o = (' ' * (2 * depth) + f'K{h32(k)} nav{h32(self.handle(u(p + 4)))} d{h32(b(p + 8))} {self.opt(u(p + 0xc))} '
             f'v{h32(self.handle(u(p + 0x28)))}={h32(u(p + 0x2c))} snd{h32(u(p + 0x20))} vis{h32(b(p + 0x34))} '
             f'sil{h32(b(p + 0x35))} per{self.opt(u(p + 0x30))} f{h32(b(p + 9))} fsnd{h32(u(p + 0x24))}')
        o += self.store(u(p + 0x10))
        for e in self.items(u(p + 0x18)):
            o += f' N{h32(self.handle(u(e)))}/{h32(b(e + 4))}' + self.store(u(e + 8))
        q = p + 4  # the types' own fields: each 4 further than MW3's
        if k == 0:
            o += f' | {h32(b(p + 0x38))} {h32(b(p + 0x39))} {h32(u(p + 0x3c))} {h32(u(p + 0x40))}\n'
            for c in self.items(u(p + 0x48)):
                o += self.obj_text(c, depth + 1)
            for c in self.items(u(p + 0x54)):
                o += ' ' * (2 * depth + 2) + 'fails\n' + self.obj_text(c, depth + 1)
            return o
        if k == 1:
            o += f' | {self.opt(u(q + 0x34))} {h32(self.handle(u(q + 0x38)))} {h32(b(q + 0x3c))}'
        elif k == 2:
            o += f' | {h32(self.handle(u(q + 0x34)))} {h32(b(q + 0x38))}'
        elif k == 3:
            o += f' | {h32(self.handle(u(q + 0x34)))} ' + ' '.join(h32(u(q + off)) for off in (0x38, 0x3c, 0x40, 0x44, 0x48))
        elif k == 4:
            o += f' | {h32(self.handle(u(q + 0x34)))} {h32(u(q + 0x38))}'
        elif k in (5, 12):
            o += ' | ' + ' '.join(h32(u(q + off)) for off in ((0x34, 0x38) if k == 5 else (0x34, 0x38, 0x3c)))
        elif k == 6:
            o += f' | {h32(self.handle(u(q + 0x34)))}'
        elif k == 7:
            o += f' | {self.opt(u(q + 0x34))} {h32(self.handle(u(q + 0x38)))} {h32(u(q + 0x3c))}'
        elif k in (9, 10):
            o += f' | {self.opt(u(q + 0x34))} {h32(self.handle(u(q + 0x38)))}'
        elif k == 11:
            o += ' |'
        else:
            o += f' | {h32(u(q + 0x34))}'
        return o + '\n'


# --- made-up blocks (test_objectives.py's, with PM's fail_sound and fails) -------------------
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
    if r.random() < 0.2:  # PM's
        tail += [['fail_sound', f'fs{r.randint(0, 9)}']]
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
        return ((['test', [r.choice(['GT', 'GE', 'EQ', 'NE', 'LE', 'LE', 'LT', 'XX'])]] if r.random() < 0.8 else []) +
                (['velocity', [r.choice([0, 5, 12.5, -3.0])]] if r.random() < 0.85 else []))
    if t == 'aim_at_object':
        return ['node', node_list(r)] if r.random() < 0.9 else []
    if t in ('target_object', 'view_object', 'face_object'):
        a = ['entity', [f'ent{r.randint(0, 40)}']] if r.random() < 0.9 else []
        if t == 'target_object' and r.random() < 0.6:
            a += ['part', [r.choice(['head', 'larm', 'rtorso', 'nopart'])]]
        return a
    if t == 'time_limit':
        return ([r.choice([30, 12.5, 7.5, 'x', [4]])] + ([r.choice([2, 40.5])] if r.random() < 0.3 else [])
                if r.random() < 0.85 else [])
    if t == 'User_Command':
        return ['cmd', [r.choice(['CMD_FIRE', 'CMD_JUMP', 'CMD_X', 'CMD_ZOOM'])]] if r.random() < 0.9 else ['cmd', [3]]
    if t == 'joystick_press':
        return ['button', [r.choice([0, 1, 4, 'one'])]] if r.random() < 0.9 else []
    if t == 'key_press':
        return ['key', [r.choice(['KEY_CTRL', 'KEY_CTRL', 'KEY_ALT', 'KEY_SHIFT', 'KEY_FOO', 'a', 'F12', 'space', 7])
                        for _ in range(r.randint(0, 3))]] if r.random() < 0.9 else []
    if t == 'mouse_press':
        return ['button', [r.choice(['left', 'LEFT', 'RIGHT', 'Middle', 'side', 2])]] if r.random() < 0.9 else []
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
        if r.random() < 0.4:  # PM's: any met fails the group
            a += ['fails', sum(([*condition(r, depth + 1)] for _ in range(r.randint(0, 2))), [])]
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


def pm_archive(files, last):
    """A version-2 archive of {name: bytes} (PM's: test_pm_zreader.entries)."""
    body, table = bytearray(), bytearray()
    for name, data in files.items():
        table += struct.pack('<II', len(body), len(data)) + name.encode().ljust(0x8c, b'\0')
        body += data
    return bytes(body + table + struct.pack('<III', 2, len(files), last))


def port(path, block, gtype, point, free):
    out, log = ctypes.create_string_buffer(CAP), ctypes.create_string_buffer(CAP)
    pt = (ctypes.c_float * 3)(*point)
    r = probe.probe_pm_objectives_run(str(path).encode(), b'objectives.zrd', block.encode(), gtype, pt, int(free),
                                      out, CAP, log, CAP)
    assert r >= 0, r
    return r, out.value.decode('latin-1'), log.value.decode('latin-1')


def shipped():
    """(archive, objectives.zrd bytes, its blocks) of every PM archive that holds one."""
    out = []
    for p in sorted((PM / 'zbd').glob('c*/reader*.zbd')):
        data = p.read_bytes()
        version, count = struct.unpack_from('<II', data, len(data) - 12)
        table = len(data) - 12 - count * 0x94
        for i in range(count):
            start, length = struct.unpack_from('<II', data, table + i * 0x94)
            name = data[table + i * 0x94 + 8:table + i * 0x94 + 72].split(b'\0')[0].decode('latin1')
            if name.lower() == 'objectives.zrd':
                body = data[start:start + length]
                root, _ = zrd.node(body, 0)
                top = root[0]
                blocks = [top[i] for i in range(0, len(top), 2) if isinstance(top[i], str)]
                out.append((p.relative_to(PM / 'zbd').as_posix(), body, blocks))
    return out


def compare(files, block, gtype, point, free, tag):
    WORK.mkdir(parents=True, exist_ok=True)
    data = pm_archive(files, LAST)
    path = WORK / 'objectives.zbd'
    path.write_bytes(data)
    emu = Run()
    eok, etext, elog = emu.run('objectives.zbd', data, block, gtype, point, free)
    COVERED.update(emu.reached)
    SEEN.update(f'kind {k}' for k in re.findall(r'^ *K([0-9a-f]+) ', etext, re.M))
    SEEN.update(f'item {k}' for k in re.findall(r' ([0-3]):[0-9a-f]+:[0-9a-f]+:[0-9a-f]+:[0-9a-f]+:[0-9a-f]+', etext))
    SEEN.update(w for w, hit in (('fails', '  fails\n' in etext), ('fail sound', re.search(r' fsnd[1-9a-f]', etext)),
                                 ('training free', 'trainfree' in elog), ('report', 'zerr' in elog),
                                 ('mfb part', 'mobilehq' in elog)) if hit)
    pok, ptext, plog = port(path, block, gtype, point, free)
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


LAST = struct.unpack_from('<I', (PM / 'zbd' / 'c1' / 'readerm1.zbd').read_bytes(), -4)[0]
COVERED = set()
SEEN = set()
WANT = {f'kind {k:x}' for k in range(19)} | {f'item {k}' for k in range(4)} | {'fails', 'fail sound', 'training free',
                                                                                 'report', 'mfb part'}


def main():
    fails = checked = 0
    pt = (10.5, -3.0, 250.0)
    for rel, body, blocks in shipped():
        print(f'{rel}: {len(blocks)} blocks', flush=True)
        for b in blocks:
            for gt in (1, 3):
                fails += not compare({'objectives.zrd': body}, b, gt, pt, gt == 3, f'{rel} {b} type {gt}')
                checked += 1
                if fails > 5:
                    break
    print(f'shipped: {checked} block runs')
    r = random.Random(int(os.environ.get('SEED', '1')))
    n = int(os.environ.get('N', '400'))
    made = 0
    for i in range(n):
        if fails > 5:
            break
        data = made_up(r)
        gt = r.choice([1, 1, 3, 0, 4, -1, -2])
        fails += not compare({'objectives.zrd': data}, r.choice(['m1', 'm1', 'm2', 'm9']), gt,
                             (r.uniform(-100, 100), 0.0, r.uniform(-5, 5)), r.random() < 0.5, f'made-up {i} type {gt}')
        made += 1
    missing = sorted(v for k, v in COVER.items() if k not in COVERED) + sorted(WANT - SEEN)
    if missing:
        print('not reached:', ', '.join(missing))
    print('FAIL' if fails or missing else 'PASS', f'({fails} mismatches; {checked} shipped runs, {made} made up)')
    if fails or missing:
        return 1
    pm_floors.floors(shipped=(checked, 60), made_up=(made, 400))
    return 0


if __name__ == '__main__':
    code = main()
    sys.stdout.flush()
    os._exit(code)
