"""A won mission's salvage (docs/campaign.md section 9) against the original x86.

    cmake --build build && .venv/bin/python -P tests/differential/test_salvage.py [runs] [first] [--verbose]

The original's store (test_pilot.py's emulator: its operator new, the store
at [0x00609990]) is filled by a plan through its own adds (0x00471c10 a mech, 0x00472630 a weapon's copy,
0x00471840 a weapon, 0x00471940 ammo, 0x00471a20 equipment); then 0x0047ae10
(CL 0: won, DL the whole team or not) runs over a fake zEntity list: entities
without a mech, the player's mech, allies and others by team, each with its
parts (torso, head, legs, arms, hip, hands, calves, feet; destroyed or not,
some missing), its weapon manager (weapons of every type and tech, beams,
shared and empty bins, infinite ones, missing slots and items), its loadout
(equipment on standing or destroyed parts, heat sinks, weapons and ammo) and
its sinks; 0x0041df10 (the definition's tech) is stubbed, rand answers from
the plan. The port (tests/probe_salvage.cpp: the ported store adds,
`lance_salvage`) runs the same. Compared: the store's items, the bins left,
the rand calls used. Since 8b2 also the briefing's reallocation: 0x00408b40
and 0x00408bd0 on a fake briefing over a campaign (or another game) whose
store and salvage the plan fills, with the mech lab's edits between
(weapons' tons from weapons.zrd's MASS, test_mechsave.py's); compared: the
four stores. The player's mech goes into the campaign's +0x64
store (a real store at the fake game's +0x64), its allies' into [0x00609990].
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import load_probe  # noqa: E402
from unicorn.x86_const import UC_X86_REG_ESP  # noqa: E402
from twod_world import IAT  # noqa: E402
from test_pilot import FS, KIND_SIZE, KIND_VT, SALVAGE, Emulated, i32  # noqa: E402

from test_mechsave import WEAPON_NAMES, WEAPONS, weapon_labs  # noqa: E402
from test_zreader import READER  # noqa: E402

probe = load_probe()
probe.probe_salvage_run.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
probe.probe_realloc_run.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
probe.probe_set_weapon_labs.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
probe.probe_lance_run.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
LANCE, CONFIG_CTOR, CONFIG_ADD, DFN_CACHE = 0x408c30, 0x423d00, 0x4723c0, 0x5e60b0
LAB_BUILD, LAB_REFRESH, REMOVE_ITEM, ADD_COPY_ITEM, FIND_WEAPON = 0x408b40, 0x408bd0, 0x471d20, 0x472630, 0x581ed0

RESULTS, MECH_TECH = 0x47ae10, 0x41df10
ADD_MECH, ADD_WEAPON, ADD_AMMO, ADD_EQUIP = 0x471c10, 0x471840, 0x471940, 0x471a20
CURRENT, LIST = 0x60861c, 0x73655c
RAND_IAT = 0x5bb668


class Salvage(Emulated):
    def __init__(self, seed):
        Emulated.__init__(self, FS())
        self.rands = []
        self.used = 0

        def rand(c):
            self.used += 1
            return self.rands.pop(0) if self.rands else 0
        self.cpu.stub_import(RAND_IAT, rand, 0)
        self.cpu.stub_function(MECH_TECH, lambda c: self.r32(c.ecx + 0x6f0))
        self.code = {}

    def fn(self, hexcode):
        if hexcode not in self.code:
            self.code[hexcode] = self.cpu.code(bytes.fromhex(hexcode))
        return self.code[hexcode]

    def part(self, gone, extra=0):
        p = self.alloc(0x80)
        self.cpu.write(p + 0x48, bytes([gone]))
        return p

    def mech(self, e):
        """A fake mech from the entity's description (the same goes to the probe)."""
        cpu = self.cpu
        m = self.alloc(0x700)
        vt = self.alloc(0x100)
        cpu.u32(m, vt)
        me = self.fn('89c8c3')  # mov eax, ecx; ret
        for off in (0x28, 0x2c):
            cpu.u32(vt + off, me)
        cpu.u32(vt + 0x50, me if e['mech'] else self.fn('31c0c3'))
        cpu.u32(m + 0xc0, e['team'])
        if e['has']:
            d = self.alloc(0x20)
            cpu.u32(d + 0xc, e['type'] & 0xffffffff)
            cpu.u32(m + 0x684, d)
        cpu.u32(m + 0x6f0, e['tech'])
        # parts by id: 1 torso, 2/3 arms, 4/5 legs, 6 head, 15 hip; 9..14 hands, calves, feet
        st = e['parts']
        torso, a0, a1, l0, l1, head, hip = (self.part(st[i] == 2) for i in (1, 2, 3, 4, 5, 6, 15))
        for off, p in ((0x39c, torso), (0x394, a0), (0x398, a1), (0x38c, l0), (0x390, l1), (0x3a4, head),
                       (0x3a0, hip)):
            cpu.u32(m + off, p)
        for pid, owner, off in ((9, a0, 0x74), (10, a1, 0x74), (11, l0, 0x74), (12, l1, 0x74), (13, l0, 0x78),
                                (14, l1, 0x78)):
            if st[pid]:
                cpu.u32(owner + off, self.part(st[pid] == 2))
        if e['manager']:
            wm = self.alloc(0x94)
            cpu.u32(wm + 0x24, len(e['weapons']))
            bins = []
            for b in e['bins']:
                a = self.alloc(0x18)
                cpu.u32(a, 1)
                if b == -1:
                    cpu.write(a + 0xc, b'\1')
                else:
                    cpu.u32(a + 4, b)
                    cpu.u32(a + 8, b)
                bins.append(a)
            e['bin_addrs'] = bins
            for i, w in enumerate(e['weapons']):
                if not w['present']:
                    continue
                o = self.alloc(0x98)
                wvt = self.alloc(0x40)
                cpu.u32(o, wvt)
                cpu.u32(wvt + 8, self.fn('b001c3' if w['beam'] else '30c0c3'))
                d = self.alloc(0x1f4)
                t = self.alloc(0xc8)
                cpu.u32(t, w['type'] & 0xffffffff)
                cpu.u32(d + 0x1f0, t)
                cpu.u32(o + 0x2c, d)
                if w['item']:
                    it = self.alloc(0x1c)
                    cpu.u32(it + 0x18, w['tech'])
                    cpu.u32(o + 0x90, it)
                if w['bin'] >= 0:
                    cpu.u32(o + 0x58, bins[w['bin']])
                cpu.u32(wm + 0x40 + 4 * i, o)
            cpu.u32(m + 0xd4, wm)
        if e['config'] is not None:
            c = self.alloc(0xd8)
            head_n = self.alloc(0xc)
            cpu.u32(head_n, head_n)
            cpu.u32(head_n + 4, head_n)
            cpu.u32(c + 8, head_n)
            for k, n, part, ident, tech in e['config']:
                it = self.item(k, ident & 0xffffffff, tech)
                cpu.u32(it + 4, n)
                cpu.u32(it + 0xc, part & 0xffffffff)
                node, last = self.alloc(0xc), self.r32(head_n + 4)
                cpu.u32(node, head_n)
                cpu.u32(node + 4, last)
                cpu.u32(node + 8, it)
                cpu.u32(last, node)
                cpu.u32(head_n + 4, node)
            cpu.u32(c + 0xc, len(e['config']))
            cpu.u32(m + 0x3ec, c)
        cpu.u32(m + 0x658, e['sink_tech'])
        cpu.u32(m + 0x65c, e['sinks'] & 0xffffffff)
        cpu.write(m + 0x660, bytes([e['doubles']]))
        return m

    def run(self, cfg):
        cpu, s = self.cpu, self.r32(SALVAGE)
        for op in cfg['fill']:
            if op[0] == 'mech':
                self.call(ADD_MECH, ecx=s, stack=(op[1] & 0xffffffff,))
            elif op[0] == 'weapon':
                self.call(ADD_WEAPON, ecx=s, stack=(op[1], op[2]))
            elif op[0] == 'copyw':
                it = self.item(1, op[1], op[2])
                self.call(0x472630, ecx=s, stack=(it, op[3]))
            elif op[0] == 'ammo':
                self.call(ADD_AMMO, ecx=s, stack=(op[1], op[2] & 0xffffffff))
            else:
                self.call(ADD_EQUIP, ecx=s, stack=(op[1], op[2], op[3] & 0xffffffff))
        ents = []
        player = None
        for e in cfg['ents']:
            m = self.mech(e)
            ents.append(m)
            if e['player']:
                player = m
        if player is None:
            player = self.alloc(0x700)
        cpu.u32(player + 0xc4, cfg['allies'])
        pilot, game, gvt = self.alloc(0x84), self.alloc(0xd0), self.alloc(0x40)
        cpu.u32(game, gvt)
        self.call(0x470ce0, ecx=game + 0x64, stack=(0,))  # the campaign's +0x64 store
        cpu.u32(gvt + 0x2c, self.fn('b8%02x000000c3' % cfg['kind']))
        cpu.u32(pilot + 0x6c, game)
        cpu.u32(pilot + 0x14, player)
        cpu.u32(CURRENT, pilot)
        arr = self.alloc(4 * max(1, len(ents)))
        for i, m in enumerate(ents):
            cpu.u32(arr + 4 * i, m)
        cpu.u32(LIST, arr)
        cpu.u32(LIST + 4, arr + 4 * len(ents))
        self.rands = list(cfg['rands'])
        self.used = 0
        self.call(RESULTS, ecx=0, edx=cfg['team'])
        out = self.store('store', s)
        own = self.store('own', game + 0x64)
        bins = []
        for e in cfg['ents']:
            for a in e.get('bin_addrs', []):
                bins.append(-1 if self.r8(a + 0xc) else i32(self.r32(a + 4)))
        return out, own, bins, self.used


def store_text(text):
    """test_pilot's store dump, as the probe's: the count, then the items."""
    body = text[text.index('{') + 1:text.rindex('}')]
    parts = body.split(' ', 3)  # b.. f.. n.. items
    return f'store{{{parts[2]} {parts[3] if len(parts) > 3 else ""}}}'


class Realloc(Salvage):
    """The briefing's reallocation: 0x00408b40 builds the mech lab's store,
    the plan edits it as the lab would, 0x00408bd0 splits it back."""

    def __init__(self, seed, labs):
        Salvage.__init__(self, seed)
        cpu = self.cpu
        self.weapons = {}
        for name, lab in zip(WEAPON_NAMES, labs):
            if lab is None:
                continue
            obj, blk = self.alloc(0x200), self.alloc(0x40)
            cpu.write(blk + 0x1c, struct.pack('<fi', lab[0], lab[2]))
            cpu.write(blk + 0x28, struct.pack('<fi', lab[1], lab[3]))
            cpu.write(blk + 0x34, struct.pack('<f', lab[4]))
            cpu.u32(obj + 0x1f0, blk)
            self.weapons[name] = obj
        cpu.stub_function(FIND_WEAPON, lambda c: self.weapons.get(self.cstr(c.ecx).encode(), 0))
        cpu.stub_import(IAT['_strdup'], lambda c: self.string(self.cstr(c.arg(0))) if c.arg(0) else 0, 0)

    def items_of(self, st):
        head = self.r32(st + 8)
        out, n = [], self.r32(head)
        while n != head:
            out.append(self.r32(n + 8))
            n = self.r32(n)
        return out

    def fill(self, st, ops):
        for op in ops:
            if op[0] == 'mech':
                self.call(ADD_MECH, ecx=st, stack=(op[1] & 0xffffffff,))
            elif op[0] == 'weapon':
                self.call(ADD_WEAPON, ecx=st, stack=(op[1], op[2]))
            elif op[0] == 'ammo':
                self.call(ADD_AMMO, ecx=st, stack=(op[1], op[2] & 0xffffffff))
            elif op[0] == 'equip':
                self.call(ADD_EQUIP, ecx=st, stack=(op[1], op[2], op[3] & 0xffffffff))
            elif op[0] == 'remove':  # the i-th item (mod the count), n of it
                its = self.items_of(st)
                if its:
                    self.call(REMOVE_ITEM, ecx=st, stack=(its[op[1] % len(its)], op[2]))

    def run_realloc(self, cfg):
        cpu = self.cpu
        b = self.alloc(0x12e6c)
        for off in (0x111ec, 0x11200):
            self.call(0x470ce0, ecx=b + off, stack=(0,))
        game, gvt = self.alloc(0xd0), self.alloc(0x40)
        cpu.u32(game, gvt)
        cpu.u32(gvt + 0x2c, self.fn('b8%02x000000c3' % cfg['kind']))
        for off in (0x8, 0x3c):
            self.call(0x470ce0, ecx=game + off, stack=(0,))
        cpu.u32(game + 0xa8, cfg['units'] & 0xffffffff)
        pilot = self.alloc(0x84)
        cpu.u32(pilot + 0x6c, game)
        cpu.u32(CURRENT, pilot)
        self.fill(game + 8, cfg['game'])
        self.fill(game + 0x3c, cfg['salvage'])
        lab = self.call(LAB_BUILD, ecx=b)
        self.fill(lab, cfg['edits'])
        self.call(LAB_REFRESH, ecx=b)
        return '\n'.join(store_text(self.store(n, a)).replace('store', n, 1) for n, a in (
            ('game', game + 8), ('salvage', game + 0x3c), ('realloc', b + 0x111ec), ('lab', b + 0x11200))) + \
            f"\nlab_is {'lab' if lab == b + 0x11200 else 'game' if lab == game + 8 else hex(lab)}\n"


def fake_parts(mech):
    """The stand-in definitions' firepoints' parts (test_mechsave.py's)."""
    return [((i * 5 + mech) % 10) - 1 for i in range(8)]


class Lance(Realloc):
    """0x00408c30 over a campaign's slots, with stand-in definitions in
    0x004846a0's cache (firepoints and a first loadout)."""

    # test_pilot.py's emulator stands in for the loadouts' copy, recount and
    # destructor; here they run as they are.
    NATIVE = (0x424010, 0x425bd0, 0x424350)

    def __init__(self, seed, labs):
        import original
        keep = original.Original.stub_function

        def stub_function(cpu, addr, fn, pop_bytes=0):
            if addr not in Lance.NATIVE:
                keep(cpu, addr, fn, pop_bytes)
        original.Original.stub_function = stub_function
        try:
            Realloc.__init__(self, seed, labs)
        finally:
            original.Original.stub_function = keep
        from twod_world import c_format

        def sprintf(c):
            esp = c.uc.reg_read(UC_X86_REG_ESP)
            text = c_format(self, self.cstr(c.arg(1)), esp + 12)
            c.write(c.arg(0), text.encode('latin1') + b'\0')
            return len(text)
        self.cpu.stub_import(0x5bb658, sprintf, 0)

    def config(self, mech, name, items):
        cpu = self.cpu
        c = self.alloc(0xd8)
        nm = self.string(name)
        self.call(CONFIG_CTOR, ecx=c, stack=(mech & 0xffffffff, nm))
        for k, ident, extra, part, n in items:
            it = self.item(k, ident, extra)
            cpu.u32(it + 0xc, part & 0xffffffff)
            self.call(CONFIG_ADD, ecx=c, stack=(it, n))
        return c

    def config_text(self, c):
        if not c:
            return 'none'
        head = self.r32(c + 8)
        its, n = [], self.r32(head)
        while n != head:
            its.append(self.r32(n + 8))
            n = self.r32(n)
        return (f'cfg{{m{i32(self.r32(c + 0x14))} "{self.cstr(self.r32(c + 0x18))}" mod{self.r8(c + 0x1f)} '
                f's{i32(self.r32(c + 0x64))},{i32(self.r32(c + 0x68))},{self.r8(c + 0x6c)} '
                f'free{i32(self.r32(c + 0x8c))} ' + ''.join(self.item_text(x) for x in its) + '}')

    def run_lance(self, cfg):
        cpu = self.cpu
        for m in range(19):
            fps = []
            for part in fake_parts(m):
                f = self.alloc(0x20)
                cpu.u32(f + 0xc, part & 0xffffffff)
                fps.append(f)
            arr, vec, dfn = self.alloc(4 * len(fps)), self.alloc(0x10), self.alloc(0x1a8)
            for i, f in enumerate(fps):
                cpu.u32(arr + 4 * i, f)
            cpu.u32(vec + 4, arr)
            cpu.u32(vec + 8, arr + 4 * len(fps))
            cpu.u32(dfn + 0x24, vec)
            cl = self.alloc(0x18)
            d = cfg['dfns'][m]
            if d is not None:
                c = self.config(m, f'dfn{m}', d)
                a = self.alloc(4)
                cpu.u32(a, c)
                cpu.u32(cl + 0xc, a)
                cpu.u32(cl + 0x10, a + 4)
            cpu.u32(dfn + 0x20, cl)
            cpu.u32(DFN_CACHE + 0x3c * m, dfn)
        b = self.alloc(0x12e6c)
        for off in (0x111ec, 0x11200):
            self.call(0x470ce0, ecx=b + off, stack=(0,))
        game, gvt = self.alloc(0xd0), self.alloc(0x60)
        cpu.u32(game, gvt)
        slots = []
        for sl in cfg['slots']:
            o = self.alloc(0x18)
            cpu.u32(o, 0x5bec38)
            cpu.u32(o + 4, sl['mech'] & 0xffffffff)
            if sl['config'] is not None:
                cm, items = sl['config']
                cpu.u32(o + 8, self.config(cm, f'slot{len(slots)}', items))
            slots.append(o)
        arr = self.alloc(4 * max(1, len(slots)))
        for i, o in enumerate(slots):
            cpu.u32(arr + 4 * i, o)
        for off, v in ((0x2c, 1), (0x48, cfg['op']), (0x4c, cfg['mission']), (0x3c, len(slots))):
            cpu.u32(gvt + off, self.fn('b8' + struct.pack('<I', v).hex() + 'c3'))
        # vt+0x34(i): the i-th slot (8b 44 24 04 = mov eax,[esp+4]; 8b 04 85 arr = mov eax,[eax*4+arr]; c2 04 00)
        cpu.u32(gvt + 0x34, self.fn('8b442404' + '8b0485' + struct.pack('<I', arr).hex() + 'c20400'))
        for off in (0x8, 0x3c):
            self.call(0x470ce0, ecx=game + off, stack=(0,))
        cpu.u32(game + 0xa8, cfg['units'] & 0xffffffff)
        pilot = self.alloc(0x84)
        cpu.u32(pilot + 0x6c, game)
        cpu.u32(CURRENT, pilot)
        self.fill(game + 8, cfg['game'])
        self.fill(game + 0x3c, cfg['salvage'])
        self.call(LANCE, ecx=b)
        out = [f"slot m{i32(self.r32(o + 4))} {self.config_text(self.r32(o + 8))}" for o in slots]
        out += [store_text(self.store(n, a)).replace('store', n, 1) for n, a in (
            ('game', game + 8), ('salvage', game + 0x3c), ('realloc', b + 0x111ec), ('lab', b + 0x11200))]
        return '\n'.join(out) + '\n'


def config_items(r):
    out = []
    for _ in range(r.choice([0, 1, 2, 4, 6])):
        k = r.choice([1, 1, 2, 3, 3])
        if k == 1:
            out.append((1, r.choice([0, 3, 9, 12, 19, 25]), r.choice([1, 2]), r.randrange(8), 1))
        elif k == 2:
            out.append((2, r.choice([0, 3, 9, 19]), 0, -1, r.choice([10, 20, 60])))
        else:
            out.append((3, r.choice([0, 1, 9, 10, 11, 3]), r.choice([1, 2, 3]), -1, r.choice([1, 2, 8])))
    return out


def make_lance(seed):
    r = random.Random(f'lance:{seed}')
    dfns = [config_items(r) if r.random() < 0.85 else None for _ in range(19)]
    slots = []
    for _ in range(r.choice([1, 2, 3, 4])):
        mech = r.choice([-1, 0, 3, 5, 9, 13, 18])
        config = None
        if r.random() < 0.6:
            config = (mech if r.random() < 0.8 else r.choice([0, 3, 9]), config_items(r))
        slots.append({'mech': mech, 'config': config})
    op, mission = r.choice([(1, 1), (2, 3), (4, 6), (4, 6)])
    return {'dfns': dfns, 'slots': slots, 'op': op, 'mission': mission, 'units': r.choice([1, 2, 3, 3]),
            'game': store_ops(r, r.randrange(0, 20)), 'salvage': store_ops(r, r.randrange(0, 20))}


def lance_setup(cfg):
    def items(its):
        return f'{len(its)} ' + ' '.join(' '.join(str(x) for x in it) for it in its)
    out = [f"op {cfg['op']} {cfg['mission']} {cfg['units']}"]
    for m, d in enumerate(cfg['dfns']):
        if d is not None:
            out.append(f'dfn {m} {items(d)}')
    for sl in cfg['slots']:
        if sl['config'] is None:
            out.append(f"slot {sl['mech']} -2")
        else:
            out.append(f"slot {sl['mech']} {sl['config'][0]} {items(sl['config'][1])}")
    for where in ('game', 'salvage'):
        for op in cfg[where]:
            out.append(where + ' ' + ' '.join(str(x) for x in op))
    return '\n'.join(out)


def run_lance(seed, labs, verbose):
    cfg = make_lance(seed)
    want = Lance(seed, labs).run_lance(cfg)
    buf = ctypes.create_string_buffer(1 << 17)
    n = probe.probe_lance_run(lance_setup(cfg).encode(), buf, len(buf))
    assert n >= 0, 'probe failed'
    got = buf.value.decode()
    if got.replace(' }', '}') != want.replace(' }', '}'):
        print(f'lance seed {seed}: differ\n  original: {want}\n  rebuild:  {got}')
        return False
    if verbose:
        print(f'lance seed {seed}: {want[:300]}')
    return True


def store_ops(r, n, removes=False):
    ops = []
    for _ in range(n):
        k = r.choice(['mech', 'mech', 'weapon', 'weapon', 'ammo', 'equip'] + (['remove'] * 3 if removes else []))
        if k == 'mech':
            ops.append(('mech', r.choice([0, 3, 5, 9, 13, 18])))
        elif k == 'weapon':
            ops.append(('weapon', r.choice([0, 3, 6, 7, 9, 12, 19, 25, 35]), r.choice([1, 2, 3])))
        elif k == 'ammo':
            ops.append(('ammo', r.choice([0, 3, 9, 19, 35]), r.choice([1, 20, 60, 200, 2000])))
        elif k == 'equip':
            ops.append(('equip', r.choice([0, 1, 2, 3, 9, 10, 11]), r.choice([1, 2]), r.choice([1, 2, 10, 40, 300])))
        else:
            ops.append(('remove', r.randrange(16), r.choice([1, 2, 5])))
    return ops


def make_realloc(seed):
    r = random.Random(f'realloc:{seed}')
    return {'kind': r.choice([1, 1, 1, 2]), 'units': r.choice([0, 1, 1, 2, 3, 8]),
            'game': store_ops(r, r.randrange(0, 20)), 'salvage': store_ops(r, r.randrange(0, 20)),
            'edits': store_ops(r, r.randrange(0, 8), True)}


def realloc_setup(cfg):
    out = [f"kind {cfg['kind']} {cfg['units']}"]
    for where in ('game', 'salvage', 'edits'):
        for op in cfg[where]:
            out.append(where + ' ' + ' '.join(str(x) for x in op))
    return '\n'.join(out)


def make_cfg(seed):
    r = random.Random(f'salvage:{seed}')
    fill = []
    for _ in range(r.randrange(0, 12)):
        k = r.choice(['mech', 'weapon', 'ammo', 'equip', 'copyw'])
        if k == 'mech':
            fill.append(('mech', r.choice([0, 3, 7, 13, 18])))
        elif k == 'weapon':
            fill.append(('weapon', r.choice([r.randrange(37), 2, 9, 15, 35]), r.choice([1, 2, 3])))
        elif k == 'ammo':
            fill.append(('ammo', r.randrange(37), r.choice([1, 20, -5])))
        elif k == 'equip':
            fill.append(('equip', r.choice([0, 1, 2, 9, 10, 11]), r.choice([1, 2, 3]), r.choice([1, 2, -1, -3])))
        else:  # a weapon through add_copy: its place 0 (add_weapon clears it)
            fill.append(('copyw', r.choice([2, 9, 15, 35]), r.choice([1, 2, 3]), r.choice([1, 2])))
    ents = []
    player_at = r.choice([0, 0, 1, 2, None])
    for i in range(r.choice([0, 1, 2, 3, 4, 5])):
        nb = r.choice([0, 1, 2, 3])
        bins = [r.choice([0, 5, 20, -1, 1]) for _ in range(nb)]
        weapons = []
        for _ in range(r.choice([0, 1, 2, 4, 6])):
            weapons.append({'present': r.random() < 0.9, 'type': r.choice([r.randrange(37), 0x20, 9, 4, 0, 35, 15, 2]),
                            'item': r.random() < 0.5, 'tech': r.choice([1, 2, 3]), 'beam': r.random() < 0.25,
                            'bin': r.choice([-1] + list(range(nb)))})
        parts = [0] * 16
        for pid in (1, 2, 3, 4, 5, 6, 15):
            parts[pid] = r.choice([1, 1, 1, 2])
        for pid in range(9, 15):
            parts[pid] = r.choice([0, 1, 1, 2])
        config = None
        if r.random() < 0.8:
            config = []
            for _ in range(r.choice([0, 1, 3, 5])):
                k = r.choice([3, 3, 3, 1, 2])
                config.append((k, r.choice([1, 2, 4]), r.choice([-1, 1, 2, 4, 6, 9, 11, 13, 15, 0, 20]),
                               r.choice([0, 1, 2, 5, 9, 10, 11]) if k == 3 else r.randrange(37), r.choice([1, 2, 3])))
        ents.append({'mech': r.random() < 0.9, 'player': i == player_at, 'team': r.choice([0, 1, 2, 3, 0x10]),
                     'has': r.random() < 0.95, 'type': r.choice([0, 3, 7, 8, 13, 18, -1]), 'tech': r.choice([1, 2, 3]),
                     'parts': parts, 'manager': r.random() < 0.9, 'weapons': weapons, 'bins': bins,
                     'config': config, 'sink_tech': r.choice([1, 2, 3]), 'sinks': r.choice([8, 10, 10, 11, 14, 20]),
                     'doubles': r.choice([0, 1])})
    return {'fill': fill, 'ents': ents, 'allies': r.choice([0, 1, 3, 0x13]), 'team': r.choice([0, 1]),
            'kind': r.choice([1, 1, 1, 2]), 'rands': [r.randrange(32768) for _ in range(8)]}


def probe_setup(cfg):
    out = []
    for op in cfg['fill']:
        out.append(' '.join(str(x) for x in op))
    for v in cfg['rands']:
        out.append(f'rand {v}')
    for e in cfg['ents']:
        st = e['parts']
        torso, head, l0, l1 = (int(st[i] == 2) for i in (1, 6, 4, 5))
        f = [1 if e['mech'] else 0, 1 if e['player'] else 0, e['team'], 1 if e['has'] else 0,
             e['type'] if e['has'] else -1, e['tech'], torso, head, l0, l1, e['sink_tech'], e['sinks'],
             e['doubles'], 16] + st
        f += [1 if e['manager'] else 0, len(e['weapons']) if e['manager'] else 0]
        if e['manager']:
            for w in e['weapons']:
                f += [1 if w['present'] else 0, w['type'], 1 if w['item'] else 0, w['tech'], 1 if w['beam'] else 0,
                      w['bin']]
        f += [len(e['bins']) if e['manager'] else 0] + (e['bins'] if e['manager'] else [])
        if e['config'] is None:
            f += [-1]
        else:
            f += [len(e['config'])]
            for it in e['config']:
                f += list(it)
        out.append('ent ' + ' '.join(str(x) for x in f))
    out.append(f"run {cfg['allies']} {cfg['team']} {1 if cfg['kind'] == 1 else 0}")
    return '\n'.join(out)


def run_case(seed, verbose):
    cfg = make_cfg(seed)
    em = Salvage(seed)
    store, own, bins, used = em.run(cfg)
    want = f"{store_text(store)}\n{store_text(own).replace('store', 'own', 1)}\nbins{''.join(f' {b}' for b in bins)}\nrand {used}\n"
    buf = ctypes.create_string_buffer(1 << 16)
    n = probe.probe_salvage_run(probe_setup(cfg).encode(), buf, len(buf))
    assert n >= 0, 'probe failed'
    got = buf.value.decode()
    if got.replace(' }', '}') != want.replace(' }', '}'):
        print(f'salvage seed {seed}: differ\n  original: {want}\n  rebuild:  {got}')
        return False, cfg
    if verbose:
        print(f'salvage seed {seed}: {store_text(store)[:200]}')
    return True, cfg


def set_labs():
    labs = weapon_labs(READER.read_bytes())
    table = (ctypes.c_float * (WEAPONS * 5))()
    present = (ctypes.c_uint8 * WEAPONS)()
    for i, lab in enumerate(labs):
        if lab:
            present[i] = 1
            table[5 * i:5 * i + 5] = [lab[0], lab[1], 0, 0, lab[4]]
            ctypes.memmove(ctypes.addressof(table) + 20 * i + 8, struct.pack('<ii', lab[2], lab[3]), 8)
    probe.probe_set_weapon_labs(table, present)
    return labs


def run_realloc(seed, labs, verbose):
    cfg = make_realloc(seed)
    em = Realloc(seed, labs)
    want = em.run_realloc(cfg)
    buf = ctypes.create_string_buffer(1 << 16)
    n = probe.probe_realloc_run(realloc_setup(cfg).encode(), buf, len(buf))
    assert n >= 0, 'probe failed'
    got = buf.value.decode()
    if got.replace(' }', '}') != want.replace(' }', '}'):
        print(f'realloc seed {seed}: differ\n  original: {want}\n  rebuild:  {got}')
        return False
    if verbose:
        print(f'realloc seed {seed}: {want[:300]}')
    return True


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    runs = int(args[0]) if args else 200
    first = int(args[1]) if len(args) > 1 else 0
    verbose = '--verbose' in sys.argv
    passed = mechs = 0
    for seed in range(first, first + runs):
        ok, cfg = run_case(seed, verbose)
        passed += ok
        mechs += len(cfg['ents'])
    print(f'salvage: {passed}/{runs} runs identical ({mechs} entities)')
    labs = set_labs()
    rpassed = sum(run_realloc(seed, labs, verbose) for seed in range(first, first + runs))
    print(f'realloc: {rpassed}/{runs} runs identical')
    lpassed = sum(run_lance(seed, labs, verbose) for seed in range(first, first + runs))
    print(f'lance: {lpassed}/{runs} runs identical')
    return 0 if passed == runs and rpassed == runs and lpassed == runs else 1


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
