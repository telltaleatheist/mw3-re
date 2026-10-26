"""The mech lab's screen (docs/mechlab.md section 2, step 2) against the original x86.

    cmake --build build && .venv/bin/python -P tests/differential/test_lab_screen.py [runs] [first] [--verbose]

The original builds the screen 0x0044b920 from dlgMechLabII.zrd's LABMENU
(or a synthetic one, odd runs: entries moved or missing) for a lance of 1..5
slots, with the Lab State's globals ([0x00605a7c] the pilot, [0x00605a28]
the slot shown, [0x00605a84] the flag) and the game's slots (vt+0x34) and
kind (vt+0x2c) from the run's setup. Then a plan: page switches 0x0044ca50
(each page shown or hidden), presses 0x0044d4d0 (pages, ACCEPT, the
slots, the pages' own ids), a chassis set for the shown slot 0x00449010,
CONFIG_EDIT moved, a rename marked under way, dumps, the destructor. The port
(tests/probe_lab.cpp) runs the same plan. Compared: every call out and a
dump of the dialog, the tabs, the mech buttons, the parts, the panels and
MESSAGE_AREA. The x87 runs at 24 bits.

Since 5b2 the exe's item rule 0x00453840 runs (UNASSIGNED_LIST's +0xa8):
the loadout's helpers it asks are stubbed with answers (0x00425300 its
weight on the x87 stack, 0x00425ea0, 0x00473570, 0x00425e20, and the slot
counts), ASSIGNED_LIST's part 0x00455eb0 too; the loadouts carry armour
and the definitions firepoints (+0x24).

Left to the steps that port them (mechlab.md section 5), stubbed here: the
pages' controls' reading, after-bind and showing (CHASSIS_LIST,
CONFIG_LIST, UNASSIGNED_LIST, ASSIGNED_LIST, the weapon lists, ENGINE_TABLE
and LOADOUT_LIST: their own vtables' slots, "lab.vt+<slot>" with the
control's offset), the panels made with new (CHASSIS_DATA 0x00457600,
MECH_DATA 0x00456cd0, ARMOR_TITLES made inline at 0x0044bd7e: a code hook
jumps over it), the part pick 0x004504d0 and the weapon groups' refresh
0x0044dcf0.
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
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP  # noqa: E402
import test_twod  # noqa: E402
from test_twod import canonical, fbits  # noqa: E402
from test_twod_msgbox import archive  # noqa: E402
from test_menus import MECH3, mutate_buttons  # noqa: E402
from test_mechsave import WEAPON_NAMES, weapon_labs  # noqa: E402

probe = load_probe()
probe.probe_app_set_callback.argtypes = [ctypes.c_void_p]
probe.probe_lab_run.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
probe.probe_set_weapon_labs.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
LABS = weapon_labs(READER.read_bytes())  # the weapons' tons and slots, for both sides (test_mechsave.py's)

SIZE = 0x1b468
CTOR = 0x44b920
FLOOR = bytes.fromhex('83ec04 d93c24 668b0424 6625fff3 660d0004 6689442402 d96c2402 dd442408 d9fc d92c24 83c404 c3')
IAT_FLOOR = 0x5bb66c
PILOT, SLOT, SLOTS, MECH, STORE, SCREEN, FLAG = 0x605a7c, 0x605a28, 0x605a2c, 0x605a30, 0x605a80, 0x605a58, 0x605a84
TABS = [0x117c4 + 0x17c * i for i in range(7)]
MECHS = [0x122e4 + 0x64c * i for i in range(12)]
PARTS = [0x18880, 0x189f0, 0x18b60, 0x18cd0, 0x18e40, 0x18fb0, 0x19120]
PANELS = [0x112ac, 0x11374, 0x1143c]
# The pages' controls' vtables and the slots left to their steps.
LATER = {v: (0x84, 0x88, 0x60, 0x64)
         for v in (0x5c0f18, 0x5c1130)}
LATER[0x5c1d68] = (0xac,)  # UNASSIGNED_LIST's put-on (step 5b2's second half)
UNASSIGNED, ASSIGNED, PART_HEAD = 0x19290, 0x1a13c, 0x18710
UNGROUPED, GROUPED = 0x1a36c, 0x1a570
COUNTED = (0xf5b, 0x24c, 0x24d)  # ALIST_TITLES's messages: their texts show the count (probe_lab.cpp's too)
CONFIGS, EDIT = 0x17548, 0x17ca8
NAMES = ['Stock', 'Mine', 'Raider', 'Hunter', '']
POPS = {0x84: 8, 0x88: 0, 0x60: 4, 0x64: 0, 0xb8: 0, 0xa8: 4, 0xac: 4}
CHASSIS, SKINS, CURRENT = 0x16e74, 0x1803c, 0x1a774
MODEL = 0x605a24
ITEM_VT = {0: 0x5bc160, 1: 0x5c0fc0, 2: 0x5bd150, 3: 0x5bd180}

test_twod.VT_KIND.update({0x5c0e98: 'I', 0x5c0e00: 'C', 0x5c0d70: 'C', 0x5c14f0: 'C', 0x5c1448: 'C',
                          0x5c1630: 'C', 0x5c13b0: 'K', 0x5c11c0: 'C', 0x5c1740: 'F', 0x5c0cc0: 'C', 0x5c5e30: 'C',
                          0x5c5da0: 'C', 0x5bc230: 'C', 0x5c5cf8: 'C', 0x5c1588: 'C',
                          0x5c1d68: 'C', 0x5c1fe0: 'C', 0x5c1300: 'C', 0x5c1250: 'C', 0x5c1f48: 'K', 0x5c1eb8: 'C', 0x5c1e28: 'C', 0x5c1cc0: 'C', 0x5c1c30: 'C'})


# 0x004846e0's table: each mech's most tons (10 out of range; mech_names.cpp's kMaxMass).
MAX_MASS = [100, 70, 50, 55, 65, 60, 100, 10, 30, 75, 75, 35, 35, 45, 40, 90, 90, 70, 60]


def max_mass(mech):
    return float(MAX_MASS[mech]) if 0 <= mech < 19 else 10.0


def i32(v):
    return struct.unpack('<i', struct.pack('<I', v & 0xffffffff))[0]


# ---- the files ----

def synthetic(seed, tree):
    r = random.Random(f'lab:{seed}')
    body = [list(x) if isinstance(x, list) else x for x in find(tree, 'LABMENU')]
    out = []
    for i in range(0, len(body) - 1, 2):
        k, v = body[i], body[i + 1]
        if k == 'BUTTONS':
            nb = []
            for j in range(0, len(v) - 1, 2):
                name, entry = v[j], v[j + 1]
                e = [list(x) if isinstance(x, list) else x for x in entry]
                if name in ('CURRENT_CHASSIS', 'UNASSIGNED_LIST', 'ALIST_TITLES') or name.startswith('ICON_'):
                    # CURRENT_CHASSIS's first label, the tabs, UNASSIGNED_LIST's DOWN
                    # and ALIST_TITLES's three labels are used unchecked (the exe
                    # crashes without them): kept, maybe moved.
                    if 'POSITION' in e and r.random() < 0.3:
                        e[e.index('POSITION') + 1] = [r.randrange(20, 500), r.randrange(20, 400)]
                    nb += [name, e]
                else:
                    nb += mutate_buttons(r, [name, entry], None)
                if name == 'CONFIG_SAVE':  # CONFIG_ADD, which the shipped dialog leaves out
                    e2 = [list(x) if isinstance(x, list) else x for x in entry]
                    if 'POSITION' in e2:
                        e2[e2.index('POSITION') + 1] = [r.randrange(20, 500), r.randrange(20, 400)]
                    nb += ['CONFIG_ADD', e2]
                if name == 'ICON_CHASSIS':  # the tabs the shipped dialog leaves out, as the code expects them
                    for extra in ('ICON_WEAPONS', 'ICON_EQUIPMENT', 'ICON_MISSIONDATA'):
                        e2 = [list(x) if isinstance(x, list) else x for x in entry]
                        if 'POSITION' in e2:
                            e2[e2.index('POSITION') + 1] = [r.randrange(20, 500), r.randrange(20, 400)]
                        nb += [extra, e2]
            v = nb
        out += [k, v]
    return [['LABMENU', out]]


def loadout_items(r):
    """A loadout's items: weapons on a firepoint (+0x0c its index), ammo and
    equipment on a part (sinks sometimes of no group, -1)."""
    out = []
    for _ in range(r.choice([0, 1, 3, 6, 10])):
        k = r.choice([1, 1, 2, 3, 3])
        if k == 1:
            out.append({'kind': 1, 'id': r.choice([0, 3, 9, 0x11, 19]), 'n': 1, 'tech': r.choice([1, 2]),
                        'part': r.randrange(-1, 7), 'group': r.choice([-1, -1, 0, 1, 2, 4])})
        elif k == 2:
            out.append({'kind': 2, 'id': r.choice([0, 9, 19]), 'n': r.choice([20, 200]), 'part': r.randrange(-1, 9)})
        else:
            out.append({'kind': 3, 'id': r.choice([0, 3, 5, 9, 10, 11, 12]), 'n': r.choice([1, 2]),
                        'tech': r.choice([1, 2]), 'part': r.randrange(-1, 9), 'group': r.choice([0, 0, -1])})
    return out


def make_plan(seed, accept=True, crit=True):
    r = random.Random(f'plan:{seed}')
    # The item rule (0x00453840) reads the shown loadout ASSIGNED_LIST keeps
    # unchecked: as in play (the lab's entry refreshes first; CRITICALS is
    # grey without a loadout), the criticals page and its tabs only after a
    # refresh with one (`crit`: the shown slot has a loadout).
    # (A rename under way is started for real: RENAME, 14.)
    lines = ['S'] + (['R'] if crit else []) + ['E'] + (['B e', 'R', 'E'] if accept and r.random() < 0.3 else [])
    for _ in range(r.randrange(2, 12)):
        if r.random() < 0.15:
            lines.append(f'M {r.randrange(0, 600):x} {r.randrange(0, 400):x}')
        if accept and r.random() < 0.1:
            lines += ['B e', 'R']  # a refresh during a rename: no skin arrows
        if accept and r.random() < 0.15:  # a chassis chosen for the shown slot (0x00449010; step 3 calls it)
            lines.append(f'C {r.choice([0, 3, 7, 18]):x}')
        if r.random() < 0.5:
            # ACCEPT reads the shown slot unchecked: only with one shown (as
            # in play: entering the lab shows slot 0 first).
            # The pages refresh from the shown slot, read unchecked too.
            ids = [8, 9, 10, 11, 8, 9, -1, 7, 12, 13, 19, 31, 32, 31, 32] + (list(range(23, 31)) * 2 if crit else []) + \
                ([0, 1, 2, 3, 4, 5, 6, 14, 15, 16, 14, 15, 16, 0] if accept else [])
            lines.append(f'B {r.choice(ids) & 0xffffffff:x}')
            for _ in range(2):
                if accept and r.random() < 0.5:  # a chassis row pressed (with a slot shown, as in play)
                    lines.append(f'L {r.randrange(-1, 12) & 0xffffffff:x}')
            if accept and r.random() < 0.3:
                lines.append('Q')  # the row of the slot's own loadout's chassis
            if accept and r.random() < 0.4:
                lines.append(f'Y {r.randrange(-1, 6) & 0xffffffff:x}')  # a loadout row pressed
            if accept and r.random() < 0.4:  # a name typed into CONFIG_EDIT, then Enter or Escape
                lines.append('T ' + r.choice(NAMES + ['Stock1', 'Mine2', 'New']).encode().hex())
                lines.append(r.choice(['N', 'N', 'Z']))
            if r.random() < 0.2:
                lines.append(f'K {r.randrange(-1, 5) & 0xffffffff:x}')   # a skin chosen
            if accept and r.random() < 0.2:
                lines.append('R')  # the page refreshed (0x0044cf50)
            if r.random() < 0.4:
                lines.append('E')
            continue
        pages = [-1, 0, 1, 2, 3, 3, 3, 4, 5, 6, 7] if crit else [-1, 0, 1, 2, 4, 5, 6, 7]
        lines.append(f'G {r.choice(pages) & 0xffffffff:x} {r.choice([0, 1, 1]):x}')
        if r.random() < 0.4:
            lines.append('E')
    if accept and crit:  # a criticals session: the store listed, its tabs and sorts (UNASSIGNED_LIST)
        lines += ['R', 'G 3 1', 'E']
        for _ in range(r.randrange(4, 14)):
            lines.append(f'B {r.choice([23, 24, 25, 26, 24, 26, 27, 28, 29, 30]):x}')
            if r.random() < 0.15:
                lines.append('R')
            if r.random() < 0.4:
                lines.append(f'W {r.choice([1, 2, 5, -1, 30]) & 0xffffffff:x}')
            if r.random() < 0.3:  # the weapon groups: a row of either list pressed
                lines.append(f'H {r.choice([0, 1]):x} {r.randrange(-1, 12) & 0xffffffff:x}')
            if r.random() < 0.4:  # ASSIGNED_LIST: a part button, or its arrows stepping the part
                if r.random() < 0.5:
                    lines.append(f'P {r.randrange(8):x}')
                else:  # the arrows pressed in a run (to the end of the list and past it)
                    lines += [f'V {r.choice([1, -1]) & 0xffffffff:x}'] * r.randrange(1, 10)
            if r.random() < 0.5:
                lines.append('E')
        lines += ['G 3 0', 'E']
    if accept and crit:  # a weapon groups session (its tab, 4): rows of both lists pressed
        lines += ['B 4', 'E']
        for _ in range(r.randrange(3, 12)):
            lines.append(f'H {r.choice([0, 1, 1]):x} {r.choice([0, 0, 1, 1, 2, 2, 3, 4, 6, 9, -1]) & 0xffffffff:x}')
            if r.random() < 0.4:
                lines.append('E')
        lines += ['E']
    if accept:  # a loadout session: the chassis page, a row, then SAVE / RENAME / DELETE with names
        lines += ['B 0', 'E']
        for _ in range(r.randrange(3, 10)):
            lines.append(f'Y {r.randrange(0, 3):x}')
            lines.append(f'B {r.choice([14, 14, 14, 15, 16, 16]):x}')
            lines.append('T ' + r.choice(NAMES[:4] * 3 + ['', 'Stock1', 'Mine2', 'New', 'Raider3']).encode().hex())
            lines.append(r.choice(['N', 'N', 'N', 'Z']))
            if r.random() < 0.5:
                lines.append('E')
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
            e.stub(addr, f'{addr:#010x}', spec)
        e.cpu.import_code(IAT_FLOOR, FLOOR)

        # The Lab State's globals and the game: its slots and kind.
        pilot, game, vt = e.world_alloc(0x84), e.world_alloc(0xd0), e.world_alloc(0x60)
        e.w32(game, vt)
        e.w32(pilot + 0x6c, game)
        e.w32(PILOT, pilot)
        e.w32(SLOT, cfg['shown'] & 0xffffffff)
        e.w32(SLOTS, cfg['slots'])
        e.w32(MECH, 0x13)
        e.cpu.write(FLAG, bytes([cfg['flag']]))
        self.citems = []  # the loadouts' items, for the probe
        slots = self.slots = []
        for m, c, sk in zip(cfg['mechs'], cfg['configs'], cfg['skins']):
            s = e.world_alloc(0x18)
            e.w32(s + 4, m & 0xffffffff)
            e.w32(s + 0xc, 7)
            e.w32(s + 0x10, sk & 0xffffffff)
            if c:
                co = e.world_alloc(0xd8)
                e.w32(co + 0x14, c['mech'] & 0xffffffff)
                e.w32(co + 0x18, e.world_string(c['name']))
                e.cpu.write(co + 0x1c, bytes([1]))
                e.cpu.write(co + 0x1f, bytes([c['modified']]))
                self.armour(co, c)
                e.w32(s + 8, co)
            slots.append(s)
        e.w32(0x60861c, pilot)
        e.w32(MODEL, e.world_alloc(0x20) if cfg['model'] else 0)
        self.store_setup()
        e.w32(STORE, self.store)
        # The weapons' mech-lab blocks (0x00581ed0 finds a weapon by name, +0x1f0 its block).
        blocks = {}
        for name, lab in zip(WEAPON_NAMES, LABS):
            if lab is None:
                continue
            obj, blk = e.world_alloc(0x200), e.world_alloc(0x40)
            e.cpu.write(blk + 0x1c, struct.pack('<fi', lab[0], lab[2]))
            e.cpu.write(blk + 0x28, struct.pack('<fi', lab[1], lab[3]))
            e.cpu.write(blk + 0x34, struct.pack('<f', lab[4]))
            e.w32(obj + 0x1f0, blk)
            blocks[name.decode()] = obj
        e.stub(0x581ed0, '@find_weapon', 'zc', None,
               lambda emu, k, a, p: emu._ret(blocks.get(a[0] if isinstance(a[0], str) else '', 0), p))
        # The shown loadout's slots and sinks (Config's, test_mechsave.py checks them).
        for addr in (0x425d80, 0x425e40, 0x425e80, 0x426070, 0x425f30, 0x425ea0):
            e.stub(addr, f'{addr:#010x}', 'c', 0)
        # The item rule's (0x00453840): a part's free slots, the equipment of a
        # kind, the loadout's weight (a float, on the x87 stack) and
        # ASSIGNED_LIST's part (step 6's, by its offset).
        e.stub(0x425e20, '0x00425e20', 'c,s0', 4)
        e.stub(0x473570, '0x00473570', 'c,s0,s1', 8)
        wbuf = e.world_alloc(4)
        wthunk = e.thunk(b'\xd9\x05' + struct.pack('<I', wbuf) + b'\xc2\x04\x00')  # fld, ret 4

        def weight(emu, key, a, pop):
            emu.w32(wbuf, t.handle(key, tuple(a)))
            emu._ret(0, 0, then=wthunk)
        e.stub(0x425300, '0x00425300', 'c,s0', 4, weight)


        def fmt(emu, key, a, pop):  # the ammo's message 0xf59 with its weapon's name; 0x24b a group's number
            buf, size, mid, arg = a
            if mid == 0x24b:
                s_ = f'F{mid}:{i32(arg)}'
                emu.cpu.write(buf, s_.encode('latin1')[:size - 1] + b'\0')
                emu._ret(t.handle(key, (mid, arg)), pop)
                return
            s_ = f'F{mid:x}:{emu.cstr(arg)}'
            emu.cpu.write(buf, s_.encode('latin1')[:size - 1] + b'\0')
            emu._ret(t.handle(key, (mid, emu.cstr(arg))), pop)
        e.stub(0x575fd0, '0x00575fd0', 's0,s1,s2,s3', None, fmt)
        # The mechs' definitions: their skin sets (+0x1c, a vector of skins,
        # each its name first), or none.
        self.dfns = {}
        rr = random.Random(f'dfn:{seed}')
        for mech in range(-1, 19):
            # (No definition only for mechs no slot, loadout or store item can
            # be: the exe reads those mechs' definitions unchecked.)
            # (-1 too: a row of none chosen leaves it on the slot, and the item rule reads it.)
            if rr.random() < 0.15 and mech not in (-1, 0, 3, 7, 18) and mech not in cfg['mechs'] and \
                    mech not in [x['id'] for x in cfg['store'] if x['kind'] == 0]:
                self.dfns[mech] = 0
                continue
            d, st = e.world_alloc(0x40), e.world_alloc(0x14)
            names = [f'skin{mech}_{k}' for k in range(rr.choice([0, 1, 2, 3, 4]))]
            arr = e.world_alloc(4 * max(1, len(names)))
            for k, nm in enumerate(names):
                sk = e.world_alloc(0x18)
                e.w32(sk, e.world_string(nm) if rr.random() < 0.9 else 0)
                e.w32(arr + 4 * k, sk if rr.random() < 0.9 else 0)
            e.w32(st + 4, arr)
            e.w32(st + 8, arr + 4 * len(names))
            e.w32(d + 0x1c, st)  # (every definition has its set: none crashes the exe)
            # Its firepoints (+0x24, a vector at +0x04 of 0x20-byte records,
            # each its part at +0x0c): where weapons may go (0x00453840).
            fps = [rr.randrange(0, 8) for _ in range(rr.choice([0, 1, 3, 6]))]
            fv, fa = e.world_alloc(0x10), e.world_alloc(4 * max(1, len(fps)))
            for k, part in enumerate(fps):
                fp = e.world_alloc(0x20)
                e.wf(fp + 4, rr.choice([0.0, 0.0, 0.5]))
                e.w32(fp + 0xc, part)
                e.w32(fa + 4 * k, fp)
            e.w32(fv + 4, fa)
            e.w32(fv + 8, fa + 4 * len(fps))
            e.w32(d + 0x24, fv)
            # Its loadouts (+0x20, a vector at +0x0c): the game's and the player's.
            lst = e.world_alloc(0x18)
            cfgs = []
            for k in range(rr.choice([0, 1, 2, 3, 4])):
                co = e.world_alloc(0xd8)
                e.w32(co + 0x14, mech & 0xffffffff)
                e.w32(co + 0x18, e.world_string(rr.choice(NAMES[:4]) + ('' if rr.random() < 0.6 else str(k))))
                e.cpu.write(co + 0x1c, bytes([rr.choice([0, 1, 1])]))
                e.cpu.write(co + 0x1e, bytes([1 if rr.random() < 0.15 else 0]))
                e.cpu.write(co + 0x1f, bytes([1 if rr.random() < 0.2 else 0]))
                self.armour(co, {'tons': rr.choice([0.0, 4.5, 10.0, 17.5]), 'tech': rr.choice([1, 2]),
                                 'ferro': rr.choice([0, 0, 1]), 'endo': rr.choice([0, 0, 1]),
                                 'engine': rr.randrange(0, 79), 'xl': rr.choice([0, 0, 1]), 'items': loadout_items(rr)})
                cfgs.append(co)
            arr = e.world_alloc(4 * max(1, len(cfgs)))
            for k, co in enumerate(cfgs):
                e.w32(arr + 4 * k, co)
            e.w32(lst + 0xc, arr if cfgs else 0)
            e.w32(lst + 0x10, arr + 4 * len(cfgs) if cfgs else 0)
            e.w32(lst + 0x14, arr + 4 * len(cfgs) if cfgs else 0)
            e.w32(d + 0x20, lst)
            self.dfns[mech] = d

        def dfn(emu, key, a, pop):
            v = self.dfns.get(i32(a[0]), 0)
            t.handle(key, (a[0],))
            t.override(key, v)
            emu._ret(v, pop)
        e.stub(0x4846a0, '0x004846a0', 'c', 0, dfn)

        def slot(emu, key, a, pop):
            i = i32(a[1])
            v = slots[i] if 0 <= i < len(slots) else 0
            t.handle(key, a)
            t.override(key, v)
            emu._ret(v, pop)
        e.w32(vt + 0x34, e.new_stub('vt+0x34', 'c,s0', 4, slot))
        def kind(emu, key, a, pop):
            t.handle(key, a)
            t.override(key, cfg['kind'])
            emu._ret(cfg['kind'], pop)
        e.w32(vt + 0x2c, e.new_stub('vt+0x2c', 'c', 0, kind))

        # The panels made with new: fakes whose slots are recorded.
        def fake(emu, key, a, pop):
            t.handle(key, a)
            o = self.fake_panel()
            t.override(key, o)
            emu._ret(o, pop)
        e.stub(0x457600, '0x00457600', '', 0, fake)
        e.stub(0x456cd0, '0x00456cd0', '', 0, fake)

        def armor(uc, address, size, user):  # new(0x31e0) and its inline making
            t.handle('0x0044fad0', ())
            o = self.fake_panel()
            t.override('0x0044fad0', o)
            uc.reg_write(UC_X86_REG_ESP, uc.reg_read(UC_X86_REG_ESP) + 4)  # the size pushed
            uc.reg_write(UC_X86_REG_EAX, o)
            uc.reg_write(UC_X86_REG_EIP, 0x44be16)
        e.uc.hook_add(UC_HOOK_CODE, armor, begin=0x44bd8e, end=0x44bd8e)

        # The pages' controls' slots of later steps.
        for vtab, slots_ in LATER.items():
            for sl in slots_:
                spec = 'c,s0' if sl in (0x60, 0xa8, 0xac) else 'c'
                fn = self.later_fields if sl == 0xb8 else self.later
                e.w32(vtab + sl, e.new_stub(f'lab.vt+{sl:#x}', spec, POPS[sl], fn))
        e.stub(0x4504d0, '0x004504d0', 's0', 4)
        for addr, spec, pop in ((0x479440, 'c', 0), (0x425bd0, 'c', 0), (0x4254d0, 'c,s0', 4), (0x457940, 'c,s0', 4),
                                (0x457330, 'c,s0', 4), (0x450e00, 'c,s0', 4), (0x44fd90, 'c,s0', 4),
                                ):
            e.stub(addr, f'{addr:#010x}', spec, pop)

        def box(emu, key, a, pop):  # (cdecl) the 0x1ed question has the mech's and loadout's names
            esp = emu.uc.reg_read(UC_X86_REG_ESP)
            args = tuple(a)
            if a[2] == 0x1ed:
                args += tuple(emu.cstr(emu.r32(esp + 0x10 + 4 * i)) for i in range(2))
            v = t.handle(key, args)
            emu._ret(v, pop)
        e.stub(0x4ac150, '0x004ac150', 's0,s1,s2', 0, box)
        e.stub(0x44b230, '0x0044b230', 'c,s0', 4, self.later)
        e.stub(0x424010, '0x00424010', 'zc,d,s0,s1', 8, self.copy_config)
        e.stub(0x424350, '0x00424350', 'c', 0)
        e.stub(0x424f60, '0x00424f60', 'c', 0)
        e.stub(0x4735f0, '0x004735f0', 'c,s0,s1,s2', 12)
        e.stub(0x44b4b0, '0x0044b4b0', 'c,s0', 4, self.later)
        e.stub(0x472a30, '0x00472a30', 'c,s0', 4)
        e.stub(0x471c10, '0x00471c10', 'c,s0', 4)
        # The slot takes the loadout (the exe takes a copy; the loadout itself
        # here, its name the same): the emulator's slot, and the probe's poke.
        e.stub(0x433df0, '0x00433df0', 'c,s0,s1,s2', 12,
               lambda emu, k, a, p: (t.handle(k, a), emu.w32(a[0] + 8, a[1]), emu._ret(1, p)))

        def sprintf(emu, key, a, pop):
            esp = emu.uc.reg_read(UC_X86_REG_ESP)
            text = c_format(emu, emu.cstr(a[1]), esp + 12)
            emu.cpu.write(a[0], text.encode('latin1') + b'\0')
            emu._ret(len(text), 0)
        e.import_stub(0x5bb658, '@sprintf', 's0,s1', 0, sprintf)

        def msg(emu, key, a, pop):  # Mech3Msg.dll's formatted message: "msg<id>" (probe_twod.h's), or none
            buf, size, mid = a[:3]
            got = t.handle(key, (size, mid))
            text = f'msg{mid}' + (f' {i32(emu.r32(a[3]))}' if mid in COUNTED else '')
            if got:
                emu.cpu.write(buf, text.encode()[:size - 1] + b'\0')
            emu._ret(len(text) if got else 0, pop)
        e.stub(0x575ff0, '0x00575ff0', 's0,s1,s2,s3', None, msg)

        def press(uc, address, size, user):  # the pages' own presses 18..22: their steps
            esp = uc.reg_read(UC_X86_REG_ESP)
            pid = i32(e.r32(esp + 0x24))
            if not 18 <= pid <= 22:
                return  # 14..16 the loadouts', 17 nothing, 23..30 the unassigned list's, 31, 32 the sorts
            t.handle('lab.press', (pid,))
            ret = e.r32(esp + 0x20)
            uc.reg_write(UC_X86_REG_ESP, esp + 0x24 + 0xc)
            uc.reg_write(UC_X86_REG_EIP, ret)
        e.uc.hook_add(UC_HOOK_CODE, press, begin=0x44d54a, end=0x44d54a)

        def mech_name(emu, key, a, pop):
            v = t.handle(key, (i32(a[0]),))
            emu._ret(e.world_string(f'mech{i32(a[0])}') if v else 0, pop)
        e.stub(0x484650, '0x00484650', 'c', None, mech_name)
        empty = e.world_string('')
        e.stub(0x5abdb0, '@cs', 'c', 0, lambda emu, k, a, p: (emu.w32(a[0], empty), emu._ret(a[0], 0)))
        e.stub(0x5abda4, '@cs~', '', 0, lambda emu, k, a, p: emu._ret(0, 0))
        e.stub(0x5abe04, '@cs(s)', 'c,s0', 4, lambda emu, k, a, p: (
            emu.w32(a[0], emu.world_string(emu.cstr(a[1]) if a[1] else '')), emu._ret(a[0], 4)))
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

        def strdup(emu, key, a, pop):
            s = emu.cpu.read(a[0], 256).split(b'\0')[0] if a[0] else b''
            p = emu.alloc(len(s) + 1)
            emu.cpu.write(p, s + b'\0')
            emu._ret(p if a[0] else 0, 0)
        e.import_stub(IAT['_strdup'], '@_strdup', 's0', 0, strdup)

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
        find_font = w.answers['0x00517680']
        w.answers.update({
            '0x00517680': lambda rr, n, a: 0 if a[0] in ('', None) else find_font(rr, n, a),  # no name
            '0x00575fd0': 1,
            '0x00425d80': lambda rr, k, a: rr.choice([0, 1, 2, 3, 5, 8, 14, 20, 39]),
            '0x00425e40': lambda rr, k, a: rr.choice([0, 0, 7, 14]),
            '0x00425e80': lambda rr, k, a: rr.choice([0, 0, 7, 14]),
            '0x00426070': lambda rr, k, a: rr.choice([10, 10, 10, 12, 20]),
            '0x00425f30': lambda rr, k, a: rr.choice([0, 1, 2, 5, 12]),
            '0x00425ea0': lambda rr, k, a: rr.choice([0, 3, 9, 15, 15, 16]),
            '0x00473570': lambda rr, k, a: rr.choice([0, 0, 0, 1, 4, 5]),
            '0x00425e20': lambda rr, k, a: rr.choice([0, 1, 2, 3, 6, 12]),
            # (often just under the loadout's mech's most, 0x004846e0: the item's own ton or half ton decides)
            '0x00425300': lambda rr, k, a: fbits(rr.choice([0.0, 5.0, 12.5, 20.0, 34.5, 49.75]) if rr.random() < 0.4
                                                 else max_mass(i32(e.r32(a[0] + 0x14))) - rr.choice([0.5, 0.25, 1.5])),
            '0x0040cec0': cfg['fast'],
            '0x0040d760': 640, '0x0040d770': 480,
            '0x00576040': lambda rr, k, a: e.world_string(f'M{a[0]:x}' + (' %d' if a[0] in COUNTED else '')),
            '0x00484650': lambda rr, k, a: rr.choice([1, 1, 1, 0]),
            '0x00479440': lambda rr, k, a: rr.choice([0, 1]),
            '0x004254d0': lambda rr, k, a: rr.choice([0, 1, 1, 1]),
            '0x004ac150': lambda rr, k, a: rr.choice([0, 1, 1, 2]),
            '0x004735f0': lambda rr, k, a: rr.choice([0, 1, 1]),
        })
        self.m = 0
        self.snapshot = None

    # The lab's store (test_debriefing.py's layout): mechs and some weapons.
    def store_setup(self):
        e = self.e
        st = e.world_alloc(0x14)
        head = e.world_alloc(0xc)
        e.w32(head, head)
        e.w32(head + 4, head)
        e.w32(st, 0x5c36e0)
        e.w32(st + 8, head)
        e.w32(st + 0xc, len(self.cfg['store']))
        e.cpu.write(st + 0x10, bytes([self.cfg['full']]))  # (Instant Action's: everything)
        prev = head
        for it in self.cfg['store']:
            o = e.world_alloc(0x1c)
            e.w32(o, ITEM_VT[it['kind']])
            e.w32(o + 4, it['n'])
            e.w32(o + 0xc, 0xffffffff)
            e.w32(o + 0x8, it.get('group', 0))
            e.cpu.write(o + 0x10, bytes([it.get('unl', 0)]))
            if it['kind'] == 3:
                e.w32(o + 0x18, it['id'] & 0xffffffff)
                e.w32(o + 0x1c, it.get('tech', 2))
            else:
                e.w32(o + 0x14, it['id'] & 0xffffffff)
            if it['kind'] == 1:
                e.w32(o + 0x18, it.get('tech', 1))
            it['addr'] = o
            node = e.world_alloc(0xc)
            e.w32(node, head)
            e.w32(node + 4, prev)
            e.w32(node + 8, o)
            e.w32(prev, node)
            e.w32(head + 4, node)
            prev = node
        self.store = st

    def wstring(self, s):  # a string a stub makes: the rebuild reads it through peek
        e = self.e
        b = s.encode('latin1') + b'\0'
        b += b'\0' * (-len(b) % 4)
        a = e.world_alloc(len(b))
        for i in range(0, len(b), 4):
            e.world_w32(a + i, int.from_bytes(b[i:i + 4], 'little'))
        return a

    def armour(self, co, c):  # a loadout's armour: tons (+0x2c), tech (+0x30), ferro-fibrous (+0x34)
        e = self.e
        e.wf(co + 0x2c, c.get('tons', 0.0))
        e.w32(co + 0x30, c.get('tech', 2))
        e.cpu.write(co + 0x34, bytes([c.get('ferro', 0)]))
        # its endo steel (+0x20), engine (+0x24 its index, +0x28 XL) and items (+0x08's list)
        e.cpu.write(co + 0x20, bytes([c.get('endo', 0)]))
        e.w32(co + 0x24, c.get('engine', 20))
        e.cpu.write(co + 0x28, bytes([c.get('xl', 0)]))
        self.item_list(co, c.get('items', []))

    def item_list(self, co, items):
        """A loadout's item list (+0x08, its count +0x0c): items with the exe's vtables."""
        e = self.e
        head = e.world_alloc(0xc)
        e.w32(head, head)
        e.w32(head + 4, head)
        e.w32(co + 8, head)
        e.w32(co + 0xc, len(items))
        prev = head
        for it in items:
            o = e.world_alloc(0x20)
            e.w32(o, ITEM_VT[it['kind']])
            e.w32(o + 4, it['n'])
            e.w32(o + 8, it.get('group', 0) & 0xffffffff)
            e.w32(o + 0xc, it['part'] & 0xffffffff)
            if it['kind'] == 3:
                e.w32(o + 0x18, it['id'])
                e.w32(o + 0x1c, it.get('tech', 2))
            else:
                e.w32(o + 0x14, it['id'] & 0xffffffff)
                if it['kind'] == 1:
                    e.w32(o + 0x18, it.get('tech', 1))
            it['addr'] = o
            self.citems.append(it)
            node = e.world_alloc(0xc)
            e.w32(node, head)
            e.w32(node + 4, prev)
            e.w32(node + 8, o)
            e.w32(prev, node)
            e.w32(head + 4, node)
            prev = node

    # 0x00424010 (ECX the name, EDX the loadout; no store): a player's copy.
    def copy_config(self, emu, key, a, pop):
        e = self.e
        self.t.handle(key, a)
        co = e.world_alloc(0xd8)
        for off in range(0, 0x20, 4):
            e.world_w32(co + off, 0)
        e.world_w32(co + 0x14, e.r32(a[1] + 0x14))
        e.world_w32(co + 0x18, self.wstring(a[0] if isinstance(a[0], str) else ''))  # ('zc': the name)
        e.world_w32(co + 0x1c, 1)
        for off in (0x2c, 0x30, 0x34, 0x20, 0x24, 0x28):  # the armour and engine, as the copy's source has them
            e.world_w32(co + off, e.r32(a[1] + off))
        head = e.world_alloc(0xc)  # (an empty list: the copy's items are its own, unread here)
        e.world_w32(head, head)
        e.world_w32(head + 4, head)
        e.world_w32(co + 8, head)
        self.t.override(key, co)
        emu._ret(co, pop)

    def fake_panel(self):
        e, w = self.e, self.w
        o = w.emu.world_alloc(0x200)
        vt = e.world_alloc(0x100)
        for sl, spec, pop in ((0x0, 'c,s0', 4), (0x60, 'c,s0', 4), (0x64, 'c', 0), (0x84, 'c', 8), (0x88, 'c', 0)):
            e.w32(vt + sl, e.new_stub(f'vt+{sl:#x}', spec, pop))
        e.w32(o, vt)
        sub = e.world_alloc(0x100)
        e.w32(sub + 0x98, e.new_stub('vt+0x98', 'c,s0,s1', 8))
        e.w32(o + 0x188, sub)
        return o

    # The chassis row of the shown slot's loadout's mech, -1 none (probe_lab.cpp's too).
    def config_row(self):
        e, m = self.e, self.m
        s = self.slots[i32(e.r32(SLOT))] if 0 <= i32(e.r32(SLOT)) < len(self.slots) else 0
        c = e.r32(s + 8) if s else 0
        if not c:
            return -1
        want = i32(e.r32(c + 0x14))
        ch = m + CHASSIS
        first, last = e.r32(ch + 0x15c), e.r32(ch + 0x160)
        for k in range((last - first) // 4 if first else 0):
            v = e.r32(e.r32(first + 4 * k) + 4)
            if v and e.r32(v) == ITEM_VT[0] and i32(e.r32(v + 0x14)) == want:
                return k
        return -1

    def later_fields(self, emu, key, a, pop):  # the item lists' refill, with what they list
        self.t.handle(key, (a[0] - self.m, emu.r32(a[0] + 0x220), emu.r32(a[0] + 0x224)))
        emu._ret(0, pop)

    def later(self, emu, key, a, pop):
        args = (a[0] - self.m,) + tuple(a[1:])
        v = self.t.handle(key, args)
        if args[0] == 0x1a13c and key in ('lab.vt+0x60', 'lab.vt+0x64'):  # ASSIGNED_LIST's hidden flag, as its own
            f = emu.r32(a[0] + 0xc)
            emu.w32(a[0] + 0xc, f & ~0x10 if key == 'lab.vt+0x60' else f | 0x10)
        emu._ret(v if key == 'lab.vt+0xa8' else 0, pop)  # (may the item go on: step 5b2's)

    def vcall(self, obj, slot, stack=()):
        return self.e.call(self.e.r32(self.e.r32(obj) + slot), ecx=obj, stack=stack)

    def run(self, plan):
        e = self.e
        dump = ''
        for line in plan:
            p = line.split()
            op = p[0]
            pv = [int(x, 16) for x in p[1:]]
            m = self.m
            if op == 'S':
                self.snapshot = e.snapshot()
                self.m = self.d = m = e.alloc(SIZE)
                e.call(CTOR, ecx=m, stack=(self.cfg['slots'],))
                e.w32(SCREEN, m)  # (the Lab State keeps it once made)
            elif not m:
                continue
            elif op == 'G':
                e.call(0x44ca50, ecx=m, stack=(pv[0], pv[1]))
            elif op == 'B':
                self.vcall(m, 0x14, (pv[0], 0, 0))
            elif op == 'L':
                self.vcall(m + CHASSIS, 0x98, (pv[0],))
            elif op == 'Q':
                k = self.config_row()
                if k >= 0:
                    self.vcall(m + CHASSIS, 0x98, (k,))
            elif op == 'Y':  # (with a loadout on the shown slot, as whenever the list has rows in play)
                sh = i32(e.r32(SLOT))
                if 0 <= sh < len(self.slots) and e.r32(self.slots[sh] + 8):
                    self.vcall(m + CONFIGS, 0x98, (pv[0],))
            elif op == 'T':
                e.call(0x5a52a0, ecx=m + EDIT, stack=(e.world_string(bytes.fromhex(p[1]).decode() if len(p) > 1 else ''),))
            elif op == 'N':
                self.vcall(m + EDIT, 0x90)
            elif op == 'Z':
                self.vcall(m + EDIT, 0x94)
            elif op == 'K':
                self.vcall(m + SKINS, 0xa0, (pv[0],))
            elif op == 'R':
                e.call(0x44cf50, ecx=m)
            elif op == 'C':
                e.call(0x449010, ecx=0x6059f8, stack=(pv[0],))
            elif op == 'W':  # UNASSIGNED_LIST scrolled (its +0x9c, as its arrows)
                self.vcall(m + UNASSIGNED, 0x9c, (pv[0],))
            elif op == 'H':  # a weapon list's row pressed (+0x98): 0 ungrouped, 1 grouped
                self.vcall(m + (GROUPED if pv[0] else UNGROUPED), 0x98, (pv[1],))
            elif op == 'V':  # ASSIGNED_LIST's +0x9c: the chosen part stepped
                self.vcall(m + ASSIGNED, 0x9c, (pv[0],))
            elif op == 'P':  # a part button pressed (+0x30), head first
                self.vcall(m + ([PART_HEAD] + PARTS)[pv[0]], 0x30)
            elif op == 'X':  # a rename under way (CONFIG_EDIT's +0x38c)
                e.w32(m + 0x18034, pv[0])
            elif op == 'M':  # CONFIG_EDIT moved (its set_pos, 0x0044a710)
                self.vcall(m + 0x17ca8, 0x0c, (pv[0], pv[1]))
            elif op == 'E':
                dump += (test_twod.Original.dump(self) + self.dump_screen() + self.dump_unassigned() +
                         self.dump_assigned() + self.dump_weapons())
            elif op == 'U':
                self.vcall(m, 0x08, (1,))
                self.m = 0
                dump += 'gone\n'
        return dump

    def dump_weapons(self):
        """probe_lab.cpp's dump_weapons: both weapon lists, their rows and the weapons' tags."""
        e = self.e
        out = []
        for name, off in (('ungrouped', UNGROUPED), ('grouped', GROUPED)):
            w = self.m + off
            r = lambda o: i32(e.r32(w + o))  # noqa: E731

            def vec(o):
                first, last = e.r32(w + o), e.r32(w + o + 4)
                return [e.r32(first + 4 * i) for i in range((last - first) // 4)] if first else []
            out.append(f'{name} sel={r(0x17c)} top={r(0x180)} flags={e.r32(w + 0xc):x} font={e.r32(w + 0x200):x} items=' +
                       '|'.join(f'{e.cstr(e.r32(x)) if e.r32(x) else ""}={e.r32(x + 4):x}' for x in vec(0x15c)) + '\n')
            for i, row in enumerate(vec(0x16c)):
                tag = e.r32(row + 0x4cc)
                t = (f' tag={i32(e.r32(tag + 0x14))},{i32(e.r32(tag + 0x18))}/{e.r32(tag + 0xc):x}'
                     f' "{e.cstr(tag + 0x108 + 0x34)}"/{i32(e.r32(tag + 0x108 + 0x14))},{i32(e.r32(tag + 0x108 + 0x18))}'
                     f'/{e.r32(tag + 0x108 + 0x234):x}/{i32(e.r32(tag + 0x108 + 0x244))}' if tag else '')
                out.append(f'{name}{i} idx={i32(e.r32(row + 0x4d0))} at={i32(e.r32(row + 0x14))},{i32(e.r32(row + 0x18))}'
                           f' "{e.cstr(row + 0x34)}"/{e.r32(row + 0xc):x}/{e.r32(row + 0x234):x}{t}\n')
        return ''.join(out)

    def dump_assigned(self):
        """probe_lab.cpp's dump_assigned."""
        e, a = self.e, self.m + ASSIGNED
        r = lambda o: i32(e.r32(a + o))  # noqa: E731

        def vec(o):
            first, last = e.r32(a + o), e.r32(a + o + 4)
            return [e.r32(first + 4 * i) for i in range((last - first) // 4)] if first else []
        out = [f'assigned top={r(0x158)} end={r(0x15c)} sel={r(0x160)} listing={r(0x22c)} flags={e.r32(a + 0xc):x} '
               'parts=' + ','.join(str(i32(e.r32(self.m + o + 0x150))) for o in [PART_HEAD] + PARTS) + '\n']
        out.append('aitems ' + '|'.join(f'{e.cstr(e.r32(x)) if e.r32(x) else ""}={e.r32(x + 4):x}/'
                                         f'{e.r32(x + 8):x}/{e.r32(x + 0xc):x}' for x in vec(0x168)) + '\n')

        def text(k, t):
            return f' {k}="{e.cstr(t + 0x34)}"/{e.r32(t + 0xc):x}/{e.r32(t + 0x234):x}'
        for i, row in enumerate(vec(0x178)):
            ln = row + 0x1440
            out.append(f'arow{i} idx={i32(e.r32(row + 0x4d0))}' + text('t', row) + text('c', row + 0x5dc) +
                       text('m', row + 0xaa8) + text('q', row + 0xf74) +
                       f' line={i32(e.r32(ln + 0x14))},{i32(e.r32(ln + 0x18))},{i32(e.r32(ln + 0x34))},'
                       f'{i32(e.r32(ln + 0x38))}/{e.r32(ln + 0x3c):x}/{e.r32(ln + 0xc):x}\n')
        return ''.join(out)

    def dump_unassigned(self):
        """probe_lab.cpp's dump_unassigned."""
        e, u = self.e, self.m + UNASSIGNED
        r = lambda o: i32(e.r32(u + o))

        def vec(o):
            first, last = e.r32(u + o), e.r32(u + o + 4)
            return [e.r32(first + 4 * i) for i in range((last - first) // 4)] if first else []
        out = [f'unassigned kind={r(0xd58)} armor={e.r8(u + 0xd54)} sort={r(0xd5c)} top={r(0x158)} end={r(0x15c)} '
               f'sel={r(0x160)} size={r(0x18c)},{r(0x190)} short={r(0x22c)},{r(0x230)} long={r(0x234)},{r(0x238)} '
               f'dy={r(0x23c)} flags={e.r32(u + 0xc):x} arrows=' +
               ','.join(f'{i32(e.r32(e.r32(u + o) + 0x14))}/{i32(e.r32(e.r32(u + o) + 0x18))}' if e.r32(u + o) else '-'
                        for o in (0x1ec, 0x1f0)) + ' checks=' +
               ','.join(str(r(o + 0x150)) for o in (0x244, 0x3b0, 0x51c, 0x688)) + '\n']
        out.append('uitems ' + '|'.join(f'{e.cstr(e.r32(x)) if e.r32(x) else ""}={e.r32(x + 4):x}/'
                                         f'{e.r32(x + 8):x}/{e.r32(x + 0xc):x}' for x in vec(0x168)) + '\n')

        def text(k, t):
            return f' {k}="{e.cstr(t + 0x34)}"/{e.r32(t + 0xc):x}/{e.r32(t + 0x234):x}'
        for i, row in enumerate(vec(0x178)):
            out.append(f'urow{i} idx={i32(e.r32(row + 0x4d0))}' + text('t', row) + text('c', row + 0x5dc) +
                       text('m', row + 0xaa8) + text('q', row + 0xf74) + '\n')
        out.append('titles' + ''.join(text('l', t) for t in vec(0xd60 + 0x110)) + '\n')
        return ''.join(out)

    # The dump probe_lab.cpp writes, from the emulator's memory.
    def dump_screen(self):
        e, m = self.e, self.m
        objs = []
        p = e.r32(m + 8)
        while p:
            objs.append(p)
            p = e.r32(p + 4)

        def ref(a):
            return f'o{objs.index(a) if a in objs else -1}'
        out = [f'screen page={i32(e.r32(m + 0x111ec))}\n']
        for i, off in enumerate(TABS):
            b = m + off
            first, last = e.r32(b + 0x170), e.r32(b + 0x174)
            hl = [e.r32(first + 4 * k) for k in range((last - first) // 4)] if first else []
            out.append(f'tab{i} id={i32(e.r32(b + 0x150))} en={i32(e.r32(b + 0xc4))} sel={e.r8(b + 0x158)} '
                       f'blink={e.r8(b + 0x159)} help=' + ','.join(ref(x) for x in hl) + '\n')
        for i, off in enumerate(MECHS):
            b = m + off
            out.append(f'mech{i} id={i32(e.r32(b + 0x150))} mech={i32(e.r32(b + 0x17c))} name={ref(b + 0x180)}\n')
        head = m + 0x18710
        out.append(f'parts head={i32(e.r32(head + 0x16c))},{i32(e.r32(head + 0x164))}' +
                   ''.join(f' {i32(e.r32(m + o + 0x16c))},{i32(e.r32(m + o + 0x164))}' for o in PARTS) + '\n')
        for off in PANELS:
            b = m + off
            out.append(f'panel {e.r32(b + 0xbc):x} {e.r32(b + 0xc0):x} {e.r32(b + 0xc4):x}\n')
        msg = m + 0x1afb0
        out.append(f'message sound={e.r32(msg + 0x4b0):x} vol={e.r32(msg + 0x4b4):x}\n')
        ch = m + CHASSIS
        out.append(f'lab slot={i32(e.r32(SLOT))} mech={i32(e.r32(MECH))} chassis sel={i32(e.r32(ch + 0x17c))} '
                   f'x218={e.r32(ch + 0x218):x} store={e.r32(ch + 0x214):x} slots=' +
                   ','.join(f'{i32(e.r32(s + 4))}/{e.r32(s + 0xc):x}/{i32(e.r32(s + 0x10))}' for s in self.slots) +
                   '\n')
        first, last = e.r32(ch + 0x15c), e.r32(ch + 0x160)
        its = [e.r32(first + 4 * i) for i in range((last - first) // 4)] if first else []
        out.append(f'chassis sort={i32(e.r32(ch + 0x6d0))} top={i32(e.r32(ch + 0x180))} items=' +
                   '|'.join(f'{e.cstr(e.r32(x))}={e.r32(x + 4):x}' for x in its) + '\n')
        rf, rl = e.r32(ch + 0x16c), e.r32(ch + 0x170)
        rows = [e.r32(rf + 4 * i) for i in range((rl - rf) // 4)] if rf else []
        sel_att = e.r32(e.r32(ch + 0x178) + 0x4cc) if e.r32(ch + 0x178) else 0
        for i, row in enumerate(rows):
            t = e.r32(row + 0x4cc)
            if t:
                out.append(f'row{i} tons {i32(e.r32(t + 0x14))},{i32(e.r32(t + 0x18))} flags={e.r32(t + 0xc):x} '
                           f'"{e.cstr(t + 0x34)}" named={e.r32(t + 0x4ac):x} sel={1 if t == sel_att else 0}\n')
        cf = m + CONFIGS
        first, last = e.r32(cf + 0x15c), e.r32(cf + 0x160)
        its = [e.r32(first + 4 * i) for i in range((last - first) // 4)] if first else []
        out.append(f'configs sel={i32(e.r32(cf + 0x17c))} items=' +
                   '|'.join(f'{e.cstr(e.r32(x))}={e.r32(x + 4):x}' for x in its) + '\n')
        ed = m + EDIT
        out.append(f'edit cfg={e.r32(ed + 0x38c):x} rename={e.r8(ed + 0x390)} focus={e.r8(ed + 0x374)} '
                   f'flags={e.r32(ed + 0xc):x} text="{e.cstr(e.r32(ed + 0x14c + 8))}"\n')
        for s_ in self.slots:
            c = e.r32(s_ + 8)
            if c:
                out.append(f'slotcfg {c:x} "{e.cstr(e.r32(c + 0x18)) if e.r32(c + 0x18) else ""}" mod={e.r8(c + 0x1f)}\n')
        for mech in sorted(self.dfns):
            d = self.dfns[mech]
            if not d:
                continue
            lst = e.r32(d + 0x20)
            first, last = e.r32(lst + 0xc), e.r32(lst + 0x10)
            cs = [e.r32(first + 4 * i) for i in range((last - first) // 4)] if first else []
            out.append(f'list{mech} ' + ','.join(f'{c:x}:{e.cstr(e.r32(c + 0x18)) if e.r32(c + 0x18) else ""}'
                                                  f':{e.r8(c + 0x1f)}' for c in cs) + '\n')
        sk = m + SKINS
        first, last = e.r32(sk + 0x15c), e.r32(sk + 0x160)
        its = [e.r32(first + 4 * i) for i in range((last - first) // 4)] if first else []
        out.append(f'skins arrows={i32(e.r32(sk + 0x188 + 0xc4))},{i32(e.r32(sk + 0x2d4 + 0xc4))} '
                   f'index={i32(e.r32(sk + 0x16c))} items=' +
                   '|'.join(f'{e.cstr(e.r32(x)) if e.r32(x) else ""}={e.r32(x + 4):x}' for x in its) + '\n')
        return ''.join(out)


def run_case(seed, path, data, start, cfg, plan, verbose):
    orig = Original(seed, data, start, cfg)
    dump_o = orig.run(plan)
    rep = Replay(orig.t, orig.e, orig.snapshot)
    probe.probe_app_set_callback(ctypes.cast(rep.callback(), ctypes.c_void_p))
    buf = ctypes.create_string_buffer(1 << 23)
    setup = ' '.join(f'{v & 0xffffffff:x}' for v in [cfg['slots'], orig.e.r32(PILOT), orig.store, cfg['shown'],
                                                       cfg['flag'], len(orig.slots)] + orig.slots +
                     [len(orig.dfns)] + [v for mech in sorted(orig.dfns) for v in (mech, orig.dfns[mech])] +
                     [cfg['full'], orig.e.r32(MODEL), len(cfg['store'])] +
                     [v for x in cfg['store'] for v in (x['addr'], x['kind'], x['id'], x['n'], x.get('unl', 0),
                                                        x.get('tech', 1 if x['kind'] == 1 else 2), x.get('group', 0))] +
                     [len(orig.citems)] +
                     [v for x in orig.citems for v in (x['addr'], x['kind'], x['id'], x['n'], x.get('unl', 0),
                                                       x.get('tech', 1 if x['kind'] == 1 else 2), x.get('group', 0),
                                                       x['part'])])
    n = probe.probe_lab_run(str(path).encode(), setup.encode(), '\n'.join(plan).encode(), buf, len(buf))
    assert n >= 0, 'probe failed'
    dump_r = buf.value.decode('latin1')
    stubbed = orig.w.stub_textures
    to, do = canonical(orig.t.log, dump_o, stubbed)
    tr, dr = canonical(rep.log, dump_r, stubbed)
    do, dr = (re.sub(r'face=".*?" height=', 'height=', x, flags=re.S) for x in (do, dr))
    ok = compare(f'lab seed {seed}', to, tr)
    if do != dr:
        ok = False
        for a, b in zip(do.split('\n'), dr.split('\n')):
            if a != b:
                print(f'lab seed {seed}: dumps differ')
                ta, tb = a.split(' '), b.split(' ')
                print('  original:', ' '.join(x for x in ta if x not in tb)[:400])
                print('  rebuild: ', ' '.join(x for x in tb if x not in ta)[:400])
                print('  in:', a[:200])
                break
        else:
            print(f'lab seed {seed}: dumps differ in length')
    if verbose and ok:
        print(f'lab seed {seed}: {len(to)} calls')
    return ok, to, do


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    runs = int(args[0]) if args else 40
    first = int(args[1]) if len(args) > 1 else 0
    verbose = '--verbose' in sys.argv
    data = READER.read_bytes()
    start = next(s for n, s, _ in entries(data) if n.lower() == 'dlgmechlabii.zrd')
    tree, _ = parse_node(data, start)
    synth_path = ROOT / 'build' / 'lab_synth.zbd'
    table = (ctypes.c_float * (len(LABS) * 5))()
    present = (ctypes.c_uint8 * len(LABS))()
    for i, lab in enumerate(LABS):
        if lab:
            present[i] = 1
            table[5 * i:5 * i + 5] = [lab[0], lab[1], 0, 0, lab[4]]
            ctypes.memmove(ctypes.addressof(table) + 20 * i + 8, struct.pack('<ii', lab[2], lab[3]), 8)
    probe.probe_set_weapon_labs(table, present)
    passed = calls = 0
    stats = {}
    for seed in range(first, first + runs):
        if verbose:
            print('seed', seed, flush=True)
        r = random.Random(f'cfg:{seed}')
        n = r.choice([1, 2, 3, 4, 4, 5])
        mechs = [r.randrange(19) for _ in range(r.choice([0, 1, 3, 6, 12, 15, 16, 17, 18, 25]))]
        store = [{'kind': 0, 'id': m, 'n': r.choice([1, 1, 2])} for m in mechs]
        for _ in range(r.choice([0, 3, 8, 12, 20, 30])):  # weapons, ammo and equipment (armour, sinks)
            k = r.choice([1, 1, 2, 3, 3])
            it = {'kind': k, 'id': r.randrange(30) if k < 3 else r.choice([0, 1, 1, 2, 2, 3, 4, 6, 6, 7, 8, 9, 10, 10, 9, 11, 12, 13, 14]),
                  'n': r.choice([1, 1, 2, 4, 10]), 'unl': 1 if r.random() < 0.15 else 0,
                  'tech': r.choice([1, 2, 3]), 'group': r.choice([0, 7, 14])}
            store.insert(r.randrange(len(store) + 1), it)
        cfg = {'store': store, 'model': r.choice([0, 1]), 'full': r.choice([0, 0, 0, 1]),
               'configs': [None if r.random() < 0.3 else
                           {'mech': r.choice([0, 3, 7, 18]), 'name': r.choice(['Stock', 'Mine']),
                            'modified': r.choice([0, 0, 1]), 'tons': r.choice([0.0, 2.5, 4.5, 6.0, 7.5, 9.0, 10.0, 12.5, 17.5, 30.0]),
                            'tech': r.choice([1, 2]), 'ferro': r.choice([0, 0, 1]), 'endo': r.choice([0, 0, 1]),
                            'engine': r.randrange(0, 79), 'xl': r.choice([0, 0, 1]),
                            'items': loadout_items(r)} for _ in range(n)],
               'skins': [r.choice([0, 0, 1, 2, 5, -1]) for _ in range(n)],
               'slots': n, 'mechs': [r.choice([0, 3, 7, 18, -1]) for _ in range(n)],
               'shown': r.choice([-1, 0, 0, 0, 0, 1, 1, 3, 7]), 'flag': r.choice([0, 0, 1]),
               'kind': r.choice([1, 1, 2, 3, 0]), 'fast': r.choice([0, 0, 1])}
        if seed % 2:
            t = synthetic(seed, tree)
            blob = write_node(t)
            synth_path.write_bytes(archive([('dlgMechLabII.zrd', blob)]))
            path, fdata, fstart = synth_path, blob, 0
        else:
            path, fdata, fstart = READER, data, start
        shown = 0 <= cfg['shown'] < n and cfg['mechs'][cfg['shown']] is not None
        plan = make_plan(seed, shown, shown and cfg['configs'][cfg['shown']] is not None)
        ok, trace, dump = run_case(seed, path, fdata, fstart, cfg, plan, verbose)
        passed += ok
        stats['seeds with unassigned rows'] = stats.get('seeds with unassigned rows', 0) + (
            1 if re.search(r'^uitems [^\n]*=', dump, re.M) else 0)
        stats['seeds with weapon groups'] = stats.get('seeds with weapon groups', 0) + (
            1 if '=0|' in dump and 'F587:1' in dump else 0)
        stats['seeds with weapon tags'] = stats.get('seeds with weapon tags', 0) + (1 if ' tag=' in dump else 0)
        stats['seeds with part slot bars'] = stats.get('seeds with part slot bars', 0) + (
            1 if re.search(r'c="[ \d]\d/[ \d]\d"', dump) else 0)
        calls += len(trace)
        for k, a_ in trace:
            if k == '0x004ac150':
                stats[f'box {a_[2]:x}'] = stats.get(f'box {a_[2]:x}', 0) + 1
            if k.startswith('lab.') or k.startswith('vt+') or k in ('0x004504d0', '0x0044dcf0', '0x00472a30',
                                                                      '0x00471c10', '0x00433df0', '0x004846a0', '0x0044fd90',
                                                                      '0x004ac150', '0x00425bd0', '0x004254d0',
                                                                      '0x00479440', '0x00575ff0', '0x00424010',
                                                                      '0x00424350', '0x00424f60', '0x004735f0',
                                                                      '0x005905a0', '0x0044a3a0', '0x00449b00',
                                                                      '0x00425d80', '0x00426070', '0x00575fd0',
                                                                      '0x00425300', '0x00425ea0', '0x00473570',
                                                                      '0x00425e20', '0x00425e40', '0x00425f30'):
                stats[k] = stats.get(k, 0) + 1
    print(f'lab screen: {passed}/{runs} runs identical ({calls} calls; ' +
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
