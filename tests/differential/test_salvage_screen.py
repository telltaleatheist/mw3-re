"""The salvage screen's making, refill and presses (docs/campaign.md section 10) against the original x86.

    cmake --build build && .venv/bin/python -P tests/differential/test_salvage_screen.py [runs] [first] [--verbose]

The original builds the screen 0x0047d3b0 from dlgSalvage.zrd's
SALVAGEDIALOG for a game whose kind (a campaign or not), field bases
(+0xa8), slots (vt+0x34, the count vt+0x38) and stores (the game's +0x08,
the campaign's +0x3c, [0x00609990]) come from the run's setup: real stores
of items with the exe's own vtables, weights from weapons.zrd's MASS
(test_salvage.py's blocks). Then a plan: frames (the mouse over the lists'
rows, clicks), the lists' choices (+0x98, 0x0047da70), presses 0x0047e490 of
every id and row, dumps; the destructor. The port
(tests/probe_salvage_screen.cpp) runs the same plan. Compared: every call
out (the fast menu, the state's pop, the messages, the mechs' names) and a
dump of the dialog, the cards, the lists (rows by store and position) and
the stores. 0x004b4900 is stood in as test_salvage.py's moves part has it.
The x87 runs at 24 bits.
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
from original import FTOL, IAT_FTOL  # noqa: E402
from twod_world import IAT, READER, TwodWorld, c_format, entries, find, parse_node  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn.x86_const import UC_X86_REG_ESP  # noqa: E402
import test_twod  # noqa: E402
from test_twod import canonical, fbits  # noqa: E402
from test_menus import MECH3, SIZES  # noqa: E402
from test_debriefing import FLOOR, IAT_FLOOR, mouse_line  # noqa: E402
from test_mechsave import WEAPON_NAMES  # noqa: E402
from test_salvage import set_labs  # noqa: E402

probe = load_probe()
probe.probe_app_set_callback.argtypes = [ctypes.c_void_p]
probe.probe_salvage_screen_run.argtypes = [ctypes.c_char_p] * 4 + [ctypes.c_int]

SIZE = 0x166a8
CTOR = 0x47d3b0
BAYS = [0x114c0 + 0x6a0 * i for i in range(6)]
LANCE = [0x13c80 + 0x6a0 * i for i in range(4)]
PLAYER, MISSION = 0x15700, 0x15dd0
CURRENT, SALVAGE_STORE, FIND_WEAPON = 0x60861c, 0x609990, 0x581ed0
STATE = 0x608bd8

test_twod.VT_KIND.update({0x5c0e00: 'C', 0x5c3d58: 'C', 0x5c3ca8: 'C', 0x5c11c0: 'C', 0x5bc230: 'C'})


def i32(v):
    return struct.unpack('<i', struct.pack('<I', v & 0xffffffff))[0]


def spots_of(tree):
    out = [(320, 240)]
    bl = find(find(tree, 'SALVAGEDIALOG') or [], 'BUTTONS') or []
    for i in range(0, len(bl) - 1, 2):
        p = find(bl[i + 1], 'POSITION')
        if p:
            out.append(tuple(p))
            if bl[i] in ('PLAYER_SALVAGE', 'MISSION_SALVAGE'):  # the rows, 23 apart
                out += [(p[0] + 10, p[1] + 5 + 23 * k) for k in range(14)]
    return out


def make_plan(seed, spots):
    r = random.Random(f'plan:{seed}')
    state = {'down': 0}
    lines = ['S', 'E']
    for _ in range(r.randrange(10, 50)):
        roll = r.random()
        if roll < 0.25:
            lines.append(f'L {r.choice("pm")} {r.choice([-1, 0, 1, 2, 3, 4, 5, 6, 8, 11, 15, 30]) & 0xffffffff:x}')
        elif roll < 0.32:
            lines.append(f'B {r.choice([10, 11, 12, 12, 11, 4, 0]):x} {r.choice([-1, 0, 1, 2, 5, 9]) & 0xffffffff:x}')
        elif roll < 0.38:
            lines.append('E')
        lines.append(mouse_line(r, spots, state))
    lines += ['E', 'U']
    return lines


def make_cfg(seed):
    r = random.Random(f'cfg:{seed}')

    def items():
        out = []
        for _ in range(r.choice([0, 1, 3, 6, 10, 14])):
            k = r.choice([0, 0, 1, 1, 2, 2, 3, 3, 3])
            if k == 0:
                ident, extra, n = r.choice([0, 3, 5, 13, 18]), 0, r.choice([1, 1, 2, 3, 7])
            elif k == 1:
                ident, extra, n = r.choice([0, 3, 6, 9, 12, 19, 25, 35]), r.choice([1, 2, 3]), r.choice([1, 1, 2])
            elif k == 2:
                ident, extra, n = r.choice([0, 3, 9, 19, 35]), 0, r.choice([1, 20, 60, 200, 2000])
            else:
                ident, extra = r.choice([0, 1, 2, 1, 2, 3, 5, 9, 10]), r.choice([1, 2])
                n = r.choice([1, 2, 19, 20, 40, 99, 100, 400, 1000])
            out.append({'kind': k, 'id': ident, 'extra': extra, 'n': n, 'own': 1 if r.random() < 0.05 else 0})
        return out
    return {'kind': r.choice([1, 1, 1, 2]), 'units': r.choice([0, 1, 2, 3, 3, 3, 4]),
            'fast': r.choice([0, 0, 1]), 'size': r.choice(SIZES),
            'slots': [r.choice([3, 8, 13, 0, 18, -1]) for _ in range(r.choice([0, 1, 1, 2, 3, 4]))],
            'count': r.choice([0, 1, 1, 2, 4]),
            'player': items(), 'campaign': items(), 'mission': items()}


class Original:
    def __init__(self, seed, data, start, cfg, labs):
        self.t = Trace(seed=seed)
        w = self.w = TwodWorld(self.t, seed)
        e = self.e = w.emu
        self.cfg = cfg
        tree = w.read_tree(data, start)
        e.stub(0x556740, '@zrdr_load', 'c,d', None, lambda emu, key, a, pop: emu._ret(tree, pop))
        e.stub(0x556820, '@zrdr_free', 'c')
        for addr, spec in MECH3.items():
            if addr not in (0x4ac150, 0x482970, 0x590250):
                e.stub(addr, f'{addr:#010x}', spec)
        e.cpu.import_code(IAT_FLOOR, FLOOR)
        e.cpu.import_code(IAT_FTOL, FTOL)
        e.cpu.stub_function(0x4b4900, lambda c: 100 if i32(c.edx) > 150 else 20 if i32(c.edx) > 20 else 1)

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

        # Weapons' tons (test_salvage.py's Realloc).
        weapons = {}
        for name, lab in zip(WEAPON_NAMES, labs):
            if lab is None:
                continue
            obj, blk = e.world_alloc(0x200), e.world_alloc(0x40)
            e.cpu.write(blk + 0x1c, struct.pack('<fi', lab[0], lab[2]))
            e.cpu.write(blk + 0x28, struct.pack('<fi', lab[1], lab[3]))
            e.cpu.write(blk + 0x34, struct.pack('<f', lab[4]))
            e.w32(obj + 0x1f0, blk)
            weapons[name] = obj
        e.cpu.stub_function(FIND_WEAPON, lambda c: weapons.get(e.cstr(c.ecx).encode(), 0))

        # The game: kind, slots, field bases, stores.
        pilot, game, vt = e.world_alloc(0x84), e.world_alloc(0xd0), e.world_alloc(0x60)
        self.game = game
        e.w32(game, vt)
        e.w32(pilot + 0x6c, game)
        e.w32(CURRENT, pilot)
        e.w32(vt + 0x2c, e.cpu.code(b'\xb8' + struct.pack('<I', cfg['kind']) + b'\xc3'))
        slots = cfg['slots']
        table = e.world_alloc(4 * max(1, len(slots)))
        for i, m in enumerate(slots):
            s = e.world_alloc(0x18)
            e.w32(s + 4, m & 0xffffffff)
            e.w32(table + 4 * i, s)
        e.w32(vt + 0x34, e.cpu.code(bytes.fromhex('8b442404 3d') + struct.pack('<I', len(slots)) +
                                    bytes.fromhex('730a 8b0485') + struct.pack('<I', table) +
                                    bytes.fromhex('c20400 31c0 c20400')))
        e.w32(vt + 0x38, e.cpu.code(b'\xb8' + struct.pack('<I', cfg['count']) + b'\xc3'))
        e.w32(game + 0xa8, cfg['units'])
        self.stores = {'p': game + 8, 'c': game + 0x3c, 'm': e.world_alloc(0x14)}
        for k, where in (('p', 'player'), ('c', 'campaign'), ('m', 'mission')):
            self.store_setup(self.stores[k], cfg[where])
        e.w32(SALVAGE_STORE, self.stores['m'])

        def fmt(emu, key, a, pop):
            buf, size, mid, n, s = a
            if mid in (0x1c3, 0x1c4):  # the store rows' ammo: a name, then a number
                s, n = n, (s if mid == 0x1c4 else 0)
            text = emu.cstr(s) if s else ''
            v = self.t.handle(key, (mid, i32(n), text))
            out = f'F{mid:x}:{i32(n)},{text}'[:size - 1] if v else ''
            emu.cpu.write(buf, out.encode('latin1') + b'\0')
            emu._ret(len(out), 0)
        e.stub(0x575fd0, '0x00575fd0', 's0,s1,s2,s3,s4', 0, fmt)

        def msg(emu, key, a, pop):  # Mech3Msg.dll's messages: "msg<id>:<first two numbers>", or none
            buf, size, mid, va = a
            got = self.t.handle(key, (size, mid))
            text = f'msg{mid}:{i32(emu.r32(va))}'  # (0x163, 0x164: the bases, one number)
            if got:
                emu.cpu.write(buf, text.encode()[:size - 1] + b'\0')
            emu._ret(len(text) if got else 0, pop)
        e.stub(0x575ff0, '0x00575ff0', 's0,s1,s2,s3', None, msg)

        def mech_name(emu, key, a, pop):
            v = self.t.handle(key, (i32(a[0]),))
            emu._ret(e.world_string(f'mech{i32(a[0])}') if v else 0, pop)
        e.stub(0x484650, '0x00484650', 'c', None, mech_name)

        def sprintf(emu, key, a, pop):
            esp = emu.uc.reg_read(UC_X86_REG_ESP)
            text = c_format(emu, emu.cstr(a[1]), esp + 12)
            emu.cpu.write(a[0], text.encode('latin1') + b'\0')
            emu._ret(len(text), 0)
        e.import_stub(0x5bb658, '@sprintf', 's0,s1', 0, sprintf)
        e.stub(0x40ce90, '0x0040ce90', 'c')
        e.stub(0x562940, '0x00562940', 'c,d,s0,s1,s2')  # a card's line (probe_twod's host)
        w.answers.update({
            '0x0040cec0': cfg['fast'],
            '0x0040ce90': cfg['fast'],
            '0x0040d760': cfg['size'][0], '0x0040d770': cfg['size'][1],
            '0x00576040': lambda rr, k, a: e.world_string(f'M{a[0]:x}'),
            '0x00484650': lambda rr, k, a: rr.choice([1, 1, 1, 0]),
            '0x00575fd0': lambda rr, k, a: rr.choice([0, 1, 1]),
            '0x00575ff0': lambda rr, k, a: rr.choice([1, 1, 0]),
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

    # A store (0x14 bytes, vtable 0x005c36e0) whose list holds items with the
    # exe's own vtables (test_debriefing.py's layout).
    def store_setup(self, st, items):
        e = self.e
        head = e.world_alloc(0xc)
        e.w32(head, head)
        e.w32(head + 4, head)
        e.w32(st, 0x5c36e0)
        e.w32(st + 8, head)
        e.w32(st + 0xc, len(items))
        vt = {0: 0x5bc160, 1: 0x5c0fc0, 2: 0x5bd150, 3: 0x5bd180}
        size = {0: 0x18, 1: 0x1c, 2: 0x18, 3: 0x20}
        prev = head
        for it in items:
            k = it['kind']
            o = e.world_alloc(size[k])
            e.w32(o, vt[k])
            e.w32(o + 4, it['n'] & 0xffffffff)
            e.w32(o + 0xc, 0xffffffff)
            e.cpu.write(o + 0x10, bytes([it['own']]))
            if k == 3:
                e.w32(o + 0x18, it['id'])
                e.w32(o + 0x1c, it['extra'])
            else:
                e.w32(o + 0x14, it['id'] & 0xffffffff)
                if k == 1:
                    e.w32(o + 0x18, it['extra'])
            it['addr'] = o
            node = e.world_alloc(0xc)
            e.w32(node, head)
            e.w32(node + 4, prev)
            e.w32(node + 8, o)
            e.w32(prev, node)
            e.w32(head + 4, node)
            prev = node

    def items_of(self, st):
        e = self.e
        head = e.r32(st + 8)
        out, n = [], e.r32(head)
        while n != head:
            out.append(e.r32(n + 8))
            n = e.r32(n)
        return out

    def where(self, item):
        """An item by its store and place: p3 (or the raw value when in none)."""
        for k, st in self.stores.items():
            its = self.items_of(st)
            if item in its:
                return f'{k}{its.index(item)}'
        return f'{item:x}'

    def vcall(self, obj, slot, stack=()):
        return self.e.call(self.e.r32(self.e.r32(obj) + slot), ecx=obj, stack=stack)

    def run(self, plan):
        e, w = self.e, self.w
        dump = ''
        for line in plan:
            p = line.split()
            op = p[0]
            m = self.m
            if op == 'S':
                self.snapshot = e.snapshot()
                self.m = self.d = m = e.alloc(SIZE)
                e.call(CTOR, ecx=m, stack=(STATE,))
            elif not m:
                continue
            elif op == 'F':
                pv = [int(x, 16) for x in p[1:]]
                w.mouse_frames.append(pv[1:])
                self.vcall(m, 0x00, (pv[0],))
                w.frame += 1
            elif op == 'L':
                self.vcall(m + (PLAYER if p[1] == 'p' else MISSION), 0x98, (int(p[2], 16),))
            elif op == 'B':
                self.vcall(m, 0x14, (int(p[1], 16), 0, int(p[2], 16)))
            elif op == 'E':
                dump += test_twod.Original.dump(self) + self.dump_screen()
            elif op == 'U':
                self.vcall(m, 0x08, (1,))
                self.m = 0
        return dump

    def item_text(self, o):
        e = self.e
        k = {0x5bc160: 0, 0x5c0fc0: 1, 0x5bd150: 2, 0x5bd180: 3}.get(e.r32(o), -1)
        ident = e.r32(o + 0x18) if k == 3 else e.r32(o + 0x14)
        extra = e.r32(o + 0x1c) if k == 3 else e.r32(o + 0x18) if k == 1 else 0
        return f'[k{k} n{i32(e.r32(o + 4))} i{i32(ident)} x{extra} b{e.r8(o + 0x10)}]'

    # The dump probe_salvage_screen.cpp writes, from the emulator's memory.
    def dump_screen(self):
        e, m = self.e, self.m
        objs = []
        p = e.r32(m + 8)
        while p:
            objs.append(p)
            p = e.r32(p + 4)

        def ref(a):
            return f'o{objs.index(a) if a in objs else -1}'
        out = []
        for name, off in (('player', PLAYER), ('mission', MISSION)):
            lst = m + off
            first, last = e.r32(lst + 0x15c), e.r32(lst + 0x160)
            its = [e.r32(first + 4 * i) for i in range((last - first) // 4)] if first else []
            st = e.r32(lst + 0x214)
            sname = next((k for k, v in self.stores.items() if v == st), f'{st:x}')
            out.append(f'{name} id={i32(e.r32(lst + 0x150))} store={sname} x218={e.r32(lst + 0x218):x} '
                       f'x20d={e.r8(lst + 0x20d)} sel={i32(e.r32(lst + 0x17c))} items=' +
                       '|'.join(f'{e.cstr(e.r32(x))}={self.where(e.r32(x + 4)) if e.r32(x + 4) else 0}'
                                for x in its) + '\n')
        for name, cards in (('bay', BAYS), ('lance', LANCE)):
            for i, c in enumerate(cards):
                b = m + c
                out.append(f'{name}{i} id={i32(e.r32(b + 0x150))} mech={i32(e.r32(b + 0x17c))} '
                           f'name={ref(b + 0x180)}\n')
        for k in ('p', 'c', 'm'):
            out.append(f'store {k}: ' + ''.join(self.item_text(o) for o in self.items_of(self.stores[k])) + '\n')
        return ''.join(out)


def run_case(seed, path, data, start, cfg, plan, labs, verbose):
    orig = Original(seed, data, start, cfg, labs)
    dump_o = orig.run(plan)
    rep = Replay(orig.t, orig.e, orig.snapshot)
    probe.probe_app_set_callback(ctypes.cast(rep.callback(), ctypes.c_void_p))
    c = ' '.join(f'{v & 0xffffffff:x}' for v in (cfg['kind'], cfg['units'], cfg['count'], *orig.stores.values()))
    c += f" {len(cfg['slots']):x} " + ' '.join(f'{v & 0xffffffff:x}' for v in cfg['slots'])
    for where in ('player', 'campaign', 'mission'):
        items = cfg[where]
        c += f' {len(items):x} ' + ' '.join(
            f"{x['addr']:x} {x['kind']:x} {x['id'] & 0xffffffff:x} {x['extra']:x} {x['n'] & 0xffffffff:x} {x['own']:x}"
            for x in items)
    buf = ctypes.create_string_buffer(1 << 23)
    n = probe.probe_salvage_screen_run(str(path).encode(), c.encode(), '\n'.join(plan).encode(), buf, len(buf))
    assert n >= 0, 'probe failed'
    dump_r = buf.value.decode('latin1')
    stubbed = orig.w.stub_textures
    to, do = canonical(orig.t.log, dump_o, stubbed)
    tr, dr = canonical(rep.log, dump_r, stubbed)
    do, dr = (re.sub(r'face=".*?" height=', 'height=', x, flags=re.S) for x in (do, dr))
    ok = compare(f'salvage screen seed {seed}', to, tr)
    if do != dr:
        ok = False
        for a, b in zip(do.split('\n'), dr.split('\n')):
            if a != b:
                print(f'salvage screen seed {seed}: dumps differ')
                ta, tb = a.split(' '), b.split(' ')
                print('  original:', ' '.join(x for x in ta if x not in tb)[:400])
                print('  rebuild: ', ' '.join(x for x in tb if x not in ta)[:400])
                print('  in:', a[:200])
                break
        else:
            print(f'salvage screen seed {seed}: dumps differ in length')
    if verbose and ok:
        print(f'salvage screen seed {seed}: {len(to)} calls')
    return ok, to, dump_o


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    runs = int(args[0]) if args else 24
    first = int(args[1]) if len(args) > 1 else 0
    verbose = '--verbose' in sys.argv
    labs = set_labs()
    data = READER.read_bytes()
    start = next(s for n, s, _ in entries(data) if n.lower() == 'dlgsalvage.zrd')
    tree, _ = parse_node(data, start)
    spots = spots_of(tree)
    passed = calls = moves = 0
    stats = {}
    for seed in range(first, first + runs):
        if verbose:
            print('seed', seed, flush=True)
        cfg = make_cfg(seed)
        plan = make_plan(seed, spots)
        ok, trace, dump = run_case(seed, READER, data, start, cfg, plan, labs, verbose)
        passed += ok
        calls += len(trace)
        stores = re.findall(r'^store [pcm]: .*$', dump, flags=re.M)
        moves += stores[:3] != stores[-3:]  # the first dump's stores against the last's
        for k, _ in trace:
            if k in ('0x005905a0', '0x00484650', '0x0040ce90', '0x00576040', '0x00575fd0', '0x00575ff0'):
                stats[k] = stats.get(k, 0) + 1
    print(f'salvage screen: {passed}/{runs} runs identical ({calls} calls, {moves} runs whose stores changed; ' +
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
