"""The debriefing screen's shell and press (docs/campaign.md section 3) against the original x86.

    cmake --build build && .venv/bin/python -P tests/differential/test_debriefing.py [runs] [first] [--verbose]

The original builds the screen 0x0047b430 from dlgWin.zrd's WINMENU (or a
synthetic one, odd runs: buttons moved or missing) for a game whose kind,
operation and mission, Instant Action name and record come from the run's
setup, the mission's flags (failed [0x00609926], a cheat [0x00609994]), time
[0x0060991c] and salvage store [0x00609990] too, and the Win State's "won"
byte. Then a plan: frames (the mouse over the buttons and cards), presses
0x0047d030 of every id, the buttons' left-up, enter and leave, dumps; the
destructor. The port (tests/probe_debriefing.cpp) runs the same plan.
Compared: every call out (the states, the cleanup, the pilot's save, the
lancemates' reset, the fade, the voice, the hooks of later items) and a dump
of the dialog, the buttons' help, the cards, the salvage list and the game's
fields the press writes. The x87 runs at 24 bits.

The results (0x0047baf0..0x0047c84b, the rank 0x0047b2f0) run on a fake
zEntity list of mechs (the player's, allies, others, entities without a
mech; their kills and points), fake objective lists ([0x0060994c],
[0x00609950]: done, shown, a line or none from 0x0045e080), the campaign's
score and records and Instant Action's mode, rounds and waves; the probe
gets the same through its setup. Stubbed: a card's set_vehicle 0x0047b210
as far as the port has it (the display's show, item 6c), a card's mech
display 0x0047e840 (a fake object). Since 8b the salvage list refills
(0x0048dbe0) from a real store of items with the exe's own vtables (their
names through Mech3Msg.dll's stub and sprintf, a stub on c_format);
the pilot's file opened 0x00478ce0 (closed: nothing written).
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
from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn.x86_const import UC_X86_REG_ESP  # noqa: E402
import test_twod  # noqa: E402
from test_twod import canonical, fbits  # noqa: E402
from test_twod_msgbox import archive  # noqa: E402
from test_menus import MECH3, SIZES, mutate_buttons  # noqa: E402

probe = load_probe()
probe.probe_app_set_callback.argtypes = [ctypes.c_void_p]
probe.probe_debriefing_run.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p,
                                       ctypes.c_int]

SIZE = 0x13bf4
CTOR, PRESS = 0x47b430, 0x47d030
ICONS = [0x112a8, 0x11424, 0x115a0]  # CONTINUE REPLAY ABORT
CARDS = [0x1171c + 0x6a0 * i for i in range(4)]
RANK, TITLE, SALVAGE = 0x1319c, 0x133b0, 0x134fc
PARTS = ICONS + CARDS + [RANK]
CURRENT, FAILED, CHEAT, TIME, SALVAGE_STORE, IA_ROUND = 0x60861c, 0x609926, 0x609994, 0x60991c, 0x609990, 0x60587c
ENDED, LIST, PRIMARY, SECONDARY, LANCE_COUNT = 0x609925, 0x73655c, 0x60994c, 0x609950, 0x608bf8
NAMES = ['', 'Me14', 'Me15', 'Me01', 'Kobra']
FLOOR = bytes.fromhex('83ec04 d93c24 668b0424 6625fff3 660d0004 6689442402 d96c2402 dd442408 d9fc d92c24 83c404 c3')
IAT_FLOOR = 0x5bb66c
WIN_STATE = 0x608e70

test_twod.VT_KIND.update({0x5c0e00: 'C', 0x5c39b0: 'C', 0x5c3bd0: 'C', 0x5c11c0: 'C', 0x5c3b20: 'C',
                          0x5bc230: 'C', 0x5c5cf8: 'C', 0x5c5e30: 'C', 0x5c5da0: 'C'})


def i32(v):
    return struct.unpack('<i', struct.pack('<I', v & 0xffffffff))[0]


# ---- the files ----

def synthetic(seed, tree):
    r = random.Random(f'debriefing:{seed}')
    body = [list(x) if isinstance(x, list) else x for x in find(tree, 'WINMENU')]
    out = []
    for i in range(0, len(body) - 1, 2):
        k, v = body[i], body[i + 1]
        if k == 'BUTTONS':
            nb = []
            for j in range(0, len(v) - 1, 2):
                name, entry = v[j], v[j + 1]
                # The icons' HELP, WIN_TITLE's and ICON_RANK's labels are read
                # unchecked (the exe crashes without them; the port does not): kept.
                if name in ('ICON_CONTINUE', 'ICON_ABORT', 'ICON_REPLAY', 'WIN_TITLE', 'ICON_RANK'):
                    e = [list(x) if isinstance(x, list) else x for x in entry]
                    if 'POSITION' in e and r.random() < 0.3:
                        e[e.index('POSITION') + 1] = [r.randrange(20, 500), r.randrange(20, 400)]
                    nb += [name, e]
                elif name == 'SALVAGE_LIST' or name.startswith('ICON_PLAYER'):
                    e = [list(x) if isinstance(x, list) else x for x in entry]
                    if 'POSITION' in e and r.random() < 0.3:
                        e[e.index('POSITION') + 1] = [r.randrange(20, 500), r.randrange(20, 400)]
                    nb += [name, e]
                else:
                    nb += mutate_buttons(r, [name, entry], None)
            v = nb
        out += [k, v]
    return [['WINMENU', out]]


def spots_of(tree):
    out = [(320, 240)]
    body = find(tree, 'WINMENU') or []
    bl = find(body, 'BUTTONS') or []
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
    state = {'down': 0}
    lines = ['S']
    for _ in range(r.randrange(20, 70)):
        roll = r.random()
        if roll < 0.06:
            lines.append(f'B {r.choice([0, 1, 2, 3]):x}')
        elif roll < 0.12:
            lines.append(f'P {r.randrange(len(PARTS)):x}')
        elif roll < 0.2:
            lines.append(f'{r.choice("NO")} {r.randrange(len(PARTS)):x}')
        elif roll < 0.23:
            lines.append('E')
        elif roll < 0.31:
            lines.append(f'R {r.randrange(16):x} {r.choice([0, 1]):x} {r.choice([0, 1]):x} '
                         f'{r.choice([0, 1, 2, 3]):x} {r.choice([-1, 0, 1, 2, 4]) & 0xffffffff:x}')
            lines.append('E')
        lines.append(mouse_line(r, spots, state))
    if r.random() < 0.5:
        lines += [f'B {r.choice([0, 0, 2]):x}', mouse_line(r, spots, state)]
    lines += ['E', 'U']
    return lines


# ---- the original ----

class Original:
    def __init__(self, seed, data, start, cfg):
        self.t = Trace(seed=seed)
        w = self.w = TwodWorld(self.t, seed)
        e = self.e = w.emu
        t = self.t
        self.cfg = cfg
        tree = w.read_tree(data, start)
        e.stub(0x556740, '@zrdr_load', 'c,d', None, lambda emu, key, a, pop: emu._ret(tree, pop))
        e.stub(0x556820, '@zrdr_free', 'c')
        for addr, spec in MECH3.items():
            if addr not in (0x4ac150, 0x482970, 0x590250):
                e.stub(addr, f'{addr:#010x}', spec)
        e.cpu.import_code(IAT_FLOOR, FLOOR)

        # MFC's CString: a pointer to the text (test_pilot.py).
        empty = e.world_string('')
        e.stub(0x5abdb0, '@cs', 'c', 0, lambda emu, k, a, p: (emu.w32(a[0], empty), emu._ret(a[0], 0)))
        e.stub(0x5abda4, '@cs~', '', 0, lambda emu, k, a, p: emu._ret(0, 0))
        e.stub(0x5abe5e, '@cs+=', 'c,s0', 4, lambda emu, k, a, p: (
            emu.w32(a[0], emu.world_string(emu.cstr(emu.r32(a[0])) + (emu.cstr(a[1]) if a[1] else ''))),
            emu._ret(a[0], 4)))
        e.stub(0x5abdbc, '@cs=', 'c,s0', 4, lambda emu, k, a, p: (
            emu.w32(a[0], emu.world_string(emu.cstr(a[1]) if a[1] else '')), emu._ret(a[0], 4)))

        def cs_format(emu, key, a, pop):
            va = emu.uc.reg_read(UC_X86_REG_ESP) + 12
            emu.w32(a[0], emu.world_string(c_format(emu, emu.cstr(a[1]), va)))
            emu._ret(a[0], 0)
        e.stub(0x5abdaa, '@cs.format', 's0,s1', 0, cs_format)

        # The current game and the mission (memory the screen reads; silent).
        pilot, game, vt = e.world_alloc(0x84), e.world_alloc(0xd0), e.world_alloc(0x60)
        self.game = game
        e.w32(game, vt)
        e.w32(pilot + 0x6c, game)
        e.w32(pilot + 0x10, 0xffffffff)  # the pilot's file, closed
        e.w32(CURRENT, pilot)
        for off, v in ((0x2c, cfg['kind']), (0x48, cfg['op']), (0x4c, cfg['mission'])):
            e.w32(vt + off, e.new_stub(f'@vt{off:x}', '', 0, lambda emu, k, a, p, v=v: emu._ret(v, 0)))

        def ia_name(emu, k, a, p):
            emu._ret(e.world_string('Arena') if t.handle('game.vt', (0xc,)) else 0, 0)
        e.w32(vt + 0xc, e.new_stub('@game.c', '', 0, ia_name))
        e.w32(game + 0x44, cfg['ia44'])
        e.w32(game + 0x6c, cfg['x6c'])
        e.w32(game + 0x70, cfg['x70'])
        e.w32(game + 0xc4, cfg['c4'])
        for i, v in enumerate(cfg['b4']):
            e.w32(game + 0xb4 + 4 * i, v)
        e.cpu.write(ENDED, bytes([cfg['ended']]))
        self.lance_setup(pilot)
        self.objectives_setup()
        e.w32(game + 0xb0, cfg['b0'])
        e.w32(IA_ROUND, cfg['round'])
        e.cpu.write(FAILED, bytes([cfg['failed']]))
        e.cpu.write(CHEAT, bytes([cfg['cheat']]))
        e.wf(TIME, cfg['time'])
        self.store_setup()
        e.w32(SALVAGE_STORE, self.store)
        self.won = e.world_alloc(4)
        e.cpu.write(self.won, bytes([cfg['won']]))

        def replace(emu, key, a, pop):
            s = a[0]
            t.handle(key, (s & 0xff if s >> 24 == 0x77 else s,))
            emu._ret(0, pop)
        e.stub(0x482970, '@state', 's0', None, lambda emu, k, a, p: emu._ret(0x77000000 | a[0], p))
        e.stub(0x590250, '0x00590250', 's0,s1', None, replace)

        def fmt(emu, key, a, pop):
            buf, size, mid, n, s = a
            if mid in (0x1c3, 0x1c4):  # the store rows' ammo: a name, then a number
                s, n = n, (s if mid == 0x1c4 else 0)
            text = emu.cstr(s) if s else ''
            v = t.handle(key, (mid, i32(n), text))
            out = f'F{mid:x}:{i32(n)},{text}'[:size - 1] if v else ''
            emu.cpu.write(buf, out.encode('latin1') + b'\0')
            emu._ret(len(out), 0)
        e.stub(0x575fd0, '0x00575fd0', 's0,s1,s2,s3,s4', 0, fmt)
        e.stub(0x48ef00, '0x0048ef00', 'c,d,s0,s1,s2,s3,s4,s5')

        def msg(emu, key, a, pop):  # Mech3Msg.dll's messages: "msg<id>:<first two numbers>", or none
            buf, size, mid, va = a
            got = t.handle(key, (size, mid))
            text = f'msg{mid}:{i32(emu.r32(va))},{i32(emu.r32(va + 4))}' if mid != 0x299 else \
                f'msg{mid}:{emu.cstr(emu.r32(va))}'  # the tonnage, a string
            if got:
                emu.cpu.write(buf, text.encode()[:size - 1] + b'\0')
            emu._ret(len(text) if got else 0, pop)
        e.stub(0x575ff0, '0x00575ff0', 's0,s1,s2,s3', None, msg)

        # The bars' fills (0x0055d010, the polygon filler): recorded, as test_hud.py.
        def polygon(emu, key, a, pop):
            v, n, color = a
            t.handle(key, (n, color, emu.r32(0x74a3c8)))
            for i in range(n):
                t.handle(key + '.v', (emu.r32(v + 12 * i), emu.r32(v + 12 * i + 4)))
            emu._ret(0, pop)
        e.stub(0x55d010, '0x0055d010', 'c,d,s0', None, polygon)

        # A mech's name (0x00484650): "mech<n>", or none.
        def mech_name(emu, key, a, pop):
            v = t.handle(key, (i32(a[0]),))
            emu._ret(e.world_string(f'mech{i32(a[0])}') if v else 0, pop)
        e.stub(0x484650, '0x00484650', 'c', None, mech_name)

        def objective_text(emu, key, a, pop):
            v = t.handle(key, (a[0],))
            emu._ret(e.world_string(f'obj{a[0]:x}') if v else 0, pop)
        e.stub(0x45e080, '0x0045e080', 'c', None, objective_text)

        def sprintf(emu, key, a, pop):
            esp = emu.uc.reg_read(UC_X86_REG_ESP)
            text = c_format(emu, emu.cstr(a[1]), esp + 12)
            emu.cpu.write(a[0], text.encode('latin1') + b'\0')
            emu._ret(len(text), 0)
        e.import_stub(0x5bb658, '@sprintf', 's0,s1', 0, sprintf)
        plain = {0x40ce90: 'c', 0x47abe0: 'c', 0x435240: 's0', 0x478ce0: '', 0x491780: 's0',
                 0x50ad60: '', 0x562940: 'c,d,s0,s1,s2'}
        for addr, spec in plain.items():
            e.stub(addr, f'{addr:#010x}', spec)
        w.answers.update({
            '0x0040cec0': cfg['fast'],
            '0x0040ce90': cfg['fast'],
            '0x0040d760': cfg['size'][0], '0x0040d770': cfg['size'][1],
            '0x00576040': lambda rr, k, a: e.world_string(f'M{a[0]:x}'),
            '0x00484650': lambda rr, k, a: rr.choice([1, 1, 1, 0]),
            'game.vt': lambda rr, k, a: rr.choice([0, 1, 1]),
            '0x00575fd0': lambda rr, k, a: rr.choice([0, 1, 1]),
            '0x00575ff0': lambda rr, k, a: rr.choice([1, 1, 0]),
            '0x0045e080': lambda rr, k, a: self.has_text.get(a[0], 0),
        })

        # FIX (bug), as the port's FileList: an empty list's rectangle ends at its top.
        def empty_rect(uc, address, size, user):
            esp = uc.reg_read(UC_X86_REG_ESP)
            if i32(e.r32(esp + 0x10)) <= 0:
                e.w32(esp + 0x30, e.r32(esp + 0x1c))
        e.uc.hook_add(UC_HOOK_CODE, empty_rect, begin=0x48d06d, end=0x48d06d)

        def copy_rect(emu, key, a, pop):
            emu.cpu.write(a[0], emu.cpu.read(a[1], 16))
            emu._ret(1, 8)
        e.import_stub(IAT['CopyRect'], '@CopyRect', 's0,s1', 8, copy_rect)

        def strdup(emu, key, a, pop):
            s = emu.cpu.read(a[0], 256).split(b'\0')[0] if a[0] else b''
            p = emu.alloc(len(s) + 1)
            emu.cpu.write(p, s + b'\0')
            emu._ret(p if a[0] else 0, 0)
        e.import_stub(IAT['_strdup'], '@_strdup', 's0', 0, strdup)
        self.m = 0
        self.snapshot = None

    # zEntity's list: fake mechs whose vt+0x2c and vt+0x28 are themselves,
    # vt+0x50 themselves or none, vt+0x98 a health, vt+0xa4 a name (silent).
    def lance_setup(self, pilot):
        e, cfg = self.e, self.cfg
        player = None
        ents = []
        for ent in cfg['lance']:
            m = e.world_alloc(0x700)
            vt = e.world_alloc(0x100)
            e.w32(m, vt)
            me = e.cpu.code(bytes.fromhex('89c8c3'))
            for off in (0x28, 0x2c):
                e.w32(vt + off, me)
            e.w32(vt + 0x50, me if ent['mech'] else e.cpu.code(bytes.fromhex('31c0c3')))
            hp = e.world_alloc(4)
            e.wf(hp, 0.37)
            e.w32(vt + 0x98, e.cpu.code(bytes.fromhex('d905') + struct.pack('<I', hp) + b'\xc3'))
            name = e.world_string(NAMES[ent['name']])
            e.w32(vt + 0xa4, e.cpu.code(b'\xb8' + struct.pack('<I', name) + b'\xc3'))
            # The display's reads: the loadout's armor, the parts' armor left
            # (vt+0x44 by section, from a table).
            cf = e.world_alloc(0xd8)
            for i, v in enumerate(ent['armor']):
                e.w32(cf + 0x38 + 4 * i, v)
            e.w32(m + 0x3ec, cf)
            for off, vals in ((0x39c, ent['torso']), (0x3a4, [ent['left'][6]]), (0x394, [ent['left'][7]]),
                              (0x398, [ent['left'][8]]), (0x38c, [ent['left'][9]]), (0x390, [ent['left'][10]])):
                part, pvt, tbl = e.world_alloc(0x80), e.world_alloc(0x50), e.world_alloc(0x20)
                for i, v in enumerate(vals):
                    e.wf(tbl + 4 * i, v)
                e.w32(part, pvt)
                e.w32(pvt + 0x44, e.cpu.code(bytes.fromhex('8b442404d90485') + struct.pack('<I', tbl) +
                                             bytes.fromhex('c20400')))
                e.w32(m + off, part)
            e.w32(m + 0xc0, ent['team'])
            e.w32(m + 0xc4, cfg['allies'])
            for i, v in enumerate(ent['stats']):
                e.w32(m + 0x3f4 + 4 * i, v)
            pl = e.world_alloc(0x20)
            e.w32(pl + 0xc, ent['type'] & 0xffffffff)
            e.w32(m + 0x684, pl)
            ent['addr'] = m
            if ent['player']:
                player = m
            if not ent.get('hidden'):
                ents.append(m)
        if player is None:  # the player's mech, off the list
            player = e.world_alloc(0x700)
            e.w32(player + 0xc4, cfg['allies'])
        e.w32(pilot + 0x14, player)
        arr = e.world_alloc(4 * max(1, len(ents)))
        for i, m in enumerate(ents):
            e.w32(arr + 4 * i, m)
        e.w32(LIST, arr)
        e.w32(LIST + 4, arr + 4 * len(ents))

    # The objective lists: a count (+0x48) and a ring of nodes (+0x44), each
    # node's +8 an objective (+0x08 done, +0x30 shown).
    def objectives_setup(self):
        e = self.e
        self.has_text = {}
        for where, objs in ((PRIMARY, self.cfg['primary']), (SECONDARY, self.cfg['secondary'])):
            if objs is None:
                e.w32(where, 0)
                continue
            lst = e.world_alloc(0x50)
            head = e.world_alloc(0x10)
            e.w32(head, head)
            e.w32(lst + 0x44, head)
            e.w32(lst + 0x48, len(objs))
            prev = head
            for o in objs:
                ob = e.world_alloc(0x40)
                e.cpu.write(ob + 8, bytes([o['done']]))
                e.cpu.write(ob + 0x30, bytes([o['shown']]))
                self.has_text[ob] = o['text']
                o['addr'] = ob
                node = e.world_alloc(0x10)
                e.w32(node, head)
                e.w32(node + 8, ob)
                e.w32(prev, node)
                prev = node
            e.w32(where, lst)

    # The salvage store: a store (0x14 bytes) whose list holds items with the
    # exe's own vtables (test_pilot.py's layout), or none.
    def store_setup(self):
        e = self.e
        self.store = 0
        if self.cfg['store'] is None:
            return
        st = e.world_alloc(0x14)
        head = e.world_alloc(0xc)
        e.w32(head, head)
        e.w32(head + 4, head)
        e.w32(st, 0x5c36e0)
        e.w32(st + 8, head)
        e.w32(st + 0xc, len(self.cfg['store']))
        vt = {0: 0x5bc160, 1: 0x5c0fc0, 2: 0x5bd150, 3: 0x5bd180}
        size = {0: 0x18, 1: 0x1c, 2: 0x18, 3: 0x20}
        prev = head
        for it in self.cfg['store']:
            k, ident, extra, n, own = it['kind'], it['id'], it['extra'], it['n'], it['own']
            o = e.world_alloc(size[k])
            e.w32(o, vt[k])
            e.w32(o + 4, n & 0xffffffff)
            e.w32(o + 0xc, 0xffffffff)
            e.cpu.write(o + 0x10, bytes([own]))
            if k == 3:
                e.w32(o + 0x18, ident)
                e.w32(o + 0x1c, extra)
            else:
                e.w32(o + 0x14, ident & 0xffffffff)
                if k == 1:
                    e.w32(o + 0x18, extra)
            it['addr'] = o
            node = e.world_alloc(0xc)
            e.w32(node, head)
            e.w32(node + 4, prev)
            e.w32(node + 8, o)
            e.w32(prev, node)
            e.w32(head + 4, node)
            prev = node
        self.store = st

    def vcall(self, obj, slot, stack=()):
        return self.e.call(self.e.r32(self.e.r32(obj) + slot), ecx=obj, stack=stack)

    def run(self, plan):
        e, w, cfg = self.e, self.w, self.cfg
        dump = ''
        for line in plan:
            p = line.split()
            op = p[0]
            pv = [int(x, 16) for x in p[1:]]
            m = self.m
            if op == 'S':
                self.snapshot = e.snapshot()
                self.m = self.d = m = e.alloc(SIZE)
                e.call(CTOR, ecx=m, stack=(WIN_STATE, cfg['op'], cfg['mission'], self.won))
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
                self.vcall(m + PARTS[pv[0]], 0x3c)
            elif op == 'O':
                self.vcall(m + PARTS[pv[0]], 0x40)
            elif op == 'R':  # the salvage list's settings, its store, a selection, a refill
                kinds, x209, x20d, x210, sel = pv
                sl = m + SALVAGE
                e.cpu.write(sl + 0x204, bytes([(kinds >> i) & 1 for i in range(4)]))
                e.cpu.write(sl + 0x209, bytes([x209]))
                e.cpu.write(sl + 0x20d, bytes([x20d]))
                e.w32(sl + 0x210, x210)
                e.w32(sl + 0x214, self.store)
                self.vcall(sl, 0x98, (sel & 0xffffffff,))
                self.vcall(sl, 0xa8)
            elif op == 'E':
                dump += test_twod.Original.dump(self) + self.dump_screen()
            elif op == 'U':
                self.vcall(m, 0x08, (1,))
                self.m = 0
                dump += self.dump_game()
        return dump

    def dump_display(self, i, d):
        e = self.e

        def poly(p):
            n = e.r32(p + 0x130)
            pts = ','.join(f'{e.r32(p + 0x34 + 12 * k):x}/{e.r32(p + 0x38 + 12 * k):x}' for k in range(min(n, 4)))
            return f'{i32(e.r32(p + 0x14))},{i32(e.r32(p + 0x18))} n{n} c{e.r32(p + 0x134):x} [{pts}]'
        out = [f'display{i} most={i32(e.r32(d + 0x1ca4))} scale={e.r32(d + 0x1ca8):x} '
               f'box={",".join(str(i32(e.r32(d + 0x1c84 + 4 * k))) for k in range(4))}']
        for k in range(11):
            b = d + 0x4c + 0x290 * k
            out.append(f' bar{k} {poly(b)} {i32(e.r32(b + 0x138))}/{i32(e.r32(b + 0x13c))} | {poly(b + 0x140)} '
                       f'{i32(e.r32(b + 0x278))}/{i32(e.r32(b + 0x27c))} '
                       + ' '.join(str(i32(e.r32(b + o))) for o in (0x280, 0x284, 0x288, 0x28c)))
        for k in range(4):
            lb = d + 0x1cac + 0x4b0 * k
            out.append(f' label{k} "{e.cstr(lb + 0x34)}" {i32(e.r32(lb + 0x14))},{i32(e.r32(lb + 0x18))}')
        ln = d + 0x2f6c
        out.append(f' line {i32(e.r32(ln + 0x14))},{i32(e.r32(ln + 0x18))} {i32(e.r32(ln + 0x34))},'
                   f'{i32(e.r32(ln + 0x38))} c{e.r32(ln + 0x3c):x}')
        return '\n'.join(out) + '\n'

    def dump_game(self):
        e, g = self.e, self.game
        return f'game b0={e.r8(g + 0xb0)} x6c={i32(e.r32(g + 0x6c))} x70={i32(e.r32(g + 0x70))}\n'

    # The dump probe_debriefing.cpp writes, from the emulator's memory.
    def dump_screen(self):
        e, m = self.e, self.m
        objs = []
        p = e.r32(m + 8)
        while p:
            objs.append(p)
            p = e.r32(p + 4)

        def ref(a):
            return f'o{objs.index(a) if a in objs else -1}'
        out = [f'screen won={e.r8(self.won)} store={e.r32(m + SALVAGE + 0x214):x} '
               f'x218={e.r32(m + SALVAGE + 0x218):x} font={e.r32(m + SALVAGE + 0x6cc):x}\n']
        first, last = e.r32(m + SALVAGE + 0x15c), e.r32(m + SALVAGE + 0x160)
        its = [e.r32(first + 4 * i) for i in range((last - first) // 4)] if first else []
        out.append(f'salvage sel={i32(e.r32(m + SALVAGE + 0x17c))} title={e.cstr(m + SALVAGE + 0x21c + 0x34)} items=' +
                   '|'.join(f'{e.cstr(e.r32(x))}={e.r32(x + 4):x}' for x in its) + '\n')
        for name, off in zip(['CONTINUE', 'REPLAY', 'ABORT', 'RANK'], ICONS + [RANK]):
            b = m + off
            first, last = e.r32(b + 0x170), e.r32(b + 0x174)
            hl = [e.r32(first + 4 * i) for i in range((last - first) // 4)] if first else []
            out.append(f'{name} id={i32(e.r32(b + 0x150))} en={e.r32(b + 0xc4)} blink={e.r8(b + 0x159)} '
                       f'help=' + ','.join(ref(x) for x in hl) + '\n')
        for i, c in enumerate(CARDS):
            d = e.r32(m + c + 0x650)
            if d:
                out.append(self.dump_display(i, d))
        for i, c in enumerate(CARDS):
            b = m + c
            d = e.r32(b + 0x650)
            out.append(f'card{i} mech={i32(e.r32(b + 0x17c))} veh={e.r32(b + 0x64c):x} display={1 if d else 0} '
                       f'name={ref(b + 0x180)}\n')
        out.append(f'results score={i32(e.r32(m + 0x13be0))} lance=' +
                   ','.join(str(i32(e.r32(m + 0x13be4 + 4 * i))) for i in range(4)) +
                   f' count={i32(e.r32(LANCE_COUNT))} texts={(e.r32(m + 0x13bd4) - e.r32(m + 0x13bd0)) // 4}\n')
        out.append(self.dump_game())
        return ''.join(out)


def run_case(seed, path, data, start, cfg, plan, verbose):
    orig = Original(seed, data, start, cfg)
    dump_o = orig.run(plan)
    rep = Replay(orig.t, orig.e, orig.snapshot)
    probe.probe_app_set_callback(ctypes.cast(rep.callback(), ctypes.c_void_p))
    c = ' '.join(f'{v:x}' for v in (cfg['kind'], cfg['op'], cfg['mission'], cfg['failed'], cfg['cheat'],
                                     fbits(cfg['time']), orig.store, cfg['won'], cfg['ia44'], cfg['b0'],
                                     cfg['round'], cfg['x6c'], cfg['x70'], cfg['c4'], *cfg['b4'], cfg['ended'],
                                     cfg['allies']))
    ents = [x for x in cfg['lance'] if x['mech'] and not x.get('hidden')]
    c += f' {len(ents):x} ' + ' '.join(
        ' '.join(f'{v & 0xffffffff:x}' for v in [x['addr'], x['player'], x['team'], x['type'], *x['stats'], x['name'],
                                                   *x['armor'], *[fbits(f) for f in x['left']]])
        for x in ents)
    items = cfg['store'] or []
    c += f' {len(items):x} ' + ' '.join(
        f"{x['addr']:x} {x['kind']:x} {x['id'] & 0xffffffff:x} {x['extra']:x} {x['n'] & 0xffffffff:x} {x['own']:x}"
        for x in items)
    for objs in (cfg['primary'], cfg['secondary']):
        objs = objs or []
        c += f' {len(objs):x} ' + ' '.join(f"{o['addr']:x} {o['done']:x} {o['shown']:x}" for o in objs)
    buf = ctypes.create_string_buffer(1 << 23)
    n = probe.probe_debriefing_run(str(path).encode(), c.encode(), '\n'.join(plan).encode(), buf, len(buf))
    assert n >= 0, 'probe failed'
    dump_r = buf.value.decode('latin1')
    stubbed = orig.w.stub_textures
    to, do = canonical(orig.t.log, dump_o, stubbed)
    tr, dr = canonical(rep.log, dump_r, stubbed)
    do, dr = (re.sub(r'face=".*?" height=', 'height=', x, flags=re.S) for x in (do, dr))
    ok = compare(f'debriefing seed {seed}', to, tr)
    if do != dr:
        ok = False
        for a, b in zip(do.split('\n'), dr.split('\n')):
            if a != b:
                print(f'debriefing seed {seed}: dumps differ')
                ta, tb = a.split(' '), b.split(' ')
                print('  original:', ' '.join(x for x in ta if x not in tb)[:400])
                print('  rebuild: ', ' '.join(x for x in tb if x not in ta)[:400])
                print('  in:', a[:200])
                break
        else:
            print(f'debriefing seed {seed}: dumps differ in length')
    if verbose and ok:
        print(f'debriefing seed {seed}: {len(to)} calls')
    return ok, to


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    runs = int(args[0]) if args else 24
    first = int(args[1]) if len(args) > 1 else 0
    verbose = '--verbose' in sys.argv
    data = READER.read_bytes()
    start = next(s for n, s, _ in entries(data) if n.lower() == 'dlgwin.zrd')
    tree, _ = parse_node(data, start)
    synth_path = ROOT / 'build' / 'debriefing_synth.zbd'
    passed = calls = 0
    stats = {}
    for seed in range(first, first + runs):
        if verbose:
            print('seed', seed, flush=True)
        r = random.Random(f'cfg:{seed}')
        op, mission = r.choice([(1, 1), (1, 2), (2, 4), (4, 4), (4, 4), (4, 6)])
        cfg = {'kind': r.choice([0, 1, 1, 1, 2, 4]), 'op': op, 'mission': mission,
               'failed': r.choice([0, 0, 1]), 'cheat': r.choice([0, 0, 0, 1]),
               'time': r.choice([0.0, 59.5, 754.25, 3599.9, 3700.0, 9000.75]),
               'won': r.choice([0, 0, 1]),
               'ia44': r.choice([1, 2, 3]), 'b0': r.choice([0, 1]), 'round': r.randrange(5),
               'fast': r.choice([0, 0, 1]), 'size': r.choice(SIZES),
               'x6c': r.randrange(-1, 6), 'x70': r.choice([0, 0, 1, 2, 3, 7]),
               'c4': r.choice([0, 1234, 99999, 100000, 250000, 399999, 400000, 900000, -5]),
               'b4': [r.choice([0, 0, 3, 17, 250]) for _ in range(4)], 'ended': r.choice([0, 1]),
               'allies': r.choice([0, 1, 3, 3, 0x13])}
        cfg['store'] = None if r.random() < 0.25 else [
            {'kind': (k := r.choice([0, 1, 1, 2, 2, 3, 3])),
             'id': r.choice([0, 3, 18, 19, -1]) if k == 0 else (r.choice([0, 1, 2, 9, 14, 15]) if k == 3
                                                                 else r.choice([0, 15, 21, 36, 37, -1])),
             'extra': r.choice([1, 2, 3]), 'n': r.choice([1, 1, 2, 5, 0, 40]), 'own': r.choice([0, 0, 1])}
            for _ in range(r.choice([0, 1, 3, 6, 10]))]
        cfg['lance'] = []
        player_at = r.choice([0, 0, 1, 2, 3, None])
        fracs = [0.0, 1.0, 1.0, 0.5, 0.25, 0.75, 0.1, 0.9, 0.33, 1.5, -0.25]  # (out of 0..1: the clamps)
        for i in range(r.choice([0, 1, 2, 3, 4, 5, 6, 7, 7])):
            cfg['lance'].append({
                'mech': r.random() < 0.9, 'player': i == player_at, 'team': r.choice([0, 1, 1, 2, 4, 0x10, 3]),
                'type': r.choice([3, 8, 13, 0, -1, 5]), 'name': r.randrange(len(NAMES)),
                'stats': [r.choice([0, 0, 1, 2, 5]), r.choice([0, 0, 1, 3]), r.choice([0, 7]),
                          r.choice([0, 0, 150, 1000]), r.choice([0, 0, 250]), r.choice([0, 0, 500])],
                'armor': [r.choice([1, 8, 12, 20, 33, 40, 0]) for _ in range(11)],
                'torso': [r.choice(fracs) for _ in range(6)],
                'left': [r.choice(fracs) for _ in range(11)]})
            cfg['lance'][-1]['armor'][r.randrange(11)] = r.choice([20, 40, 45])  # (some armor: no 0 / 0 scale)
            cfg['lance'][-1]['left'][:6] = cfg['lance'][-1]['torso']

        def objectives():
            if r.random() < 0.15:
                return None
            return [{'done': r.choice([0, 1]), 'shown': r.choice([0, 1, 1, 1]), 'text': r.choice([0, 1, 1, 1])}
                    for _ in range(r.choice([0, 1, 2, 3, 4, 6]))]
        cfg['primary'], cfg['secondary'] = objectives(), objectives()
        if seed % 2:
            t = synthetic(seed, tree)
            blob = write_node(t)
            synth_path.write_bytes(archive([('dlgWin.zrd', blob)]))
            path, fdata, fstart, here = synth_path, blob, 0, t
        else:
            path, fdata, fstart, here = READER, data, start, tree
        plan = make_plan(seed, spots_of(here))
        ok, trace = run_case(seed, path, fdata, fstart, cfg, plan, verbose)
        passed += ok
        calls += len(trace)
        for k, _ in trace:
            if k in ('0x00590250', '0x00491780', '0x00478ce0', '0x00435240', '0x0050ad60', '0x0047abe0',
                     '0x0048ef00', '0x00484650', '0x0048dbe0', 'game.vt',
                     '0x00575fd0', '0x0045e080', '0x005179d0'):
                stats[k] = stats.get(k, 0) + 1
    print(f'debriefing: {passed}/{runs} runs identical ({calls} calls; ' +
          ', '.join(f'{k} x{v}' for k, v in sorted(stats.items())) + ')')
    return 0 if passed == runs else 1


if __name__ == '__main__':
    code = 1
    try:
        code = main()
    except BaseException:
        import traceback
        traceback.print_exc()
    sys.stdout.flush()
    sys.stderr.flush()
    os._exit(code)
