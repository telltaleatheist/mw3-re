"""PM's commander panel against its exe: test_pocket_panel moved. The constructor 0x469dc0 (the target line 0x469190,
the colours 0x46a270, the lancemates' lines 0x469a70 over zEntity's list, the orders' lines 0x469bf0 over the table
0x606f30 and 0x469510, each line put by 0x469980), the lines' virtuals (the mouse 0x469150 / 0x469180, the presses
0x469230 / 0x469280 / 0x469800 / 0x469870, the refreshes 0x4690f0 / 0x469210 / 0x46c140 / 0x46c200 / 0x46c2c0 /
0x46c330 / 0x46c390, the orders' draw 0x4698b0), the pick 0x46abd0, the count 0x46a370 and the destructor 0x46a2b0.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_pocket_panel.py [runs]

PM's differences from MW3's: a line reads "%3s %s", its key (blank without one) and its text, where MW3's numbered a
lancemate's; the orders are read from the key bindings at each panel, not once a mission; the enemy order is lit on
one of the player's enemies (his +0xc8), where MW3's took any not his ally. The world is made up as test_pocket_panel's
(twod_world.py's zTwoD outside, run on PM's exe through pm_moved.py), with PM's offsets: the mode's order +0x180, the
cursor's node +0x184 and place +0x188, +0x1a4, the player at the IControls' +0x20 (vt+0x58, vt+0x28: +0xc0 his team,
+0xc4 allies, +0xc8 enemies), a vehicle's name at vt+0xb0; and PM's own calls stubbed: the keys (0x536a60, 0x536c90),
the actions' names (0x536c70) and the picks' bindings (0x596e50, 0x536c00, 0x536c30). Each run builds the panel and runs
a random plan; the calls out and a dump of the panel and its lines must match the port's (mech3/pocket_panel,
tests/probe_pm_pocket_panel.cpp). The x87 runs at 24 bits.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
os.environ['MW3_EXE'] = 'pm'
import pm_moved  # noqa: E402  (sets MW3_PM_MOVED: this file's addresses are PM's)

sys.meta_path.insert(0, pm_moved.Finder())  # calltrace, twod_world and test_twod with PM's addresses
import pm_floors  # noqa: E402
from calltrace import Replay, Trace, compare, load_probe  # noqa: E402
from twod_world import TwodWorld  # noqa: E402
from test_twod import canonical  # noqa: E402
from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_ESP  # noqa: E402

probe = load_probe()
probe.probe_app_set_callback.argtypes = [ctypes.c_void_p]
probe.probe_pm_pocket_panel_run.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]

CTOR, DTOR, PICK, PICKED = 0x469dc0, 0x46a2b0, 0x46abd0, 0x46a370
VT_KIND = {0x5e5228: 'M', 0x5e5740: 'A', 0x5e5518: 'E', 0x5e55d0: 'L', 0x5e5688: 'S', 0x5e57f8: 'O'}
ROWS = [(0x58, 10), (0x59, 11), (0x56, 0), (0x55, 1), (0x5a, 12), (0x53, 2)]  # 0x606f30: the action, the order
ACTIONS = ['Attack my target', 'Form on me', 'Hold']  # probe_pm_pocket_panel.cpp's
LIST, MARKER, CURSOR, MADE = 0x8d3ac4, 0x629a2c, 0x629a30, 0x629a20
COLOURS, YELLOW, WHITE, ROLLOVER, SELECT = 0x6299c0, 0x629998, 0x6299a0, 0x629a18, 0x629a1c
BLANK = 0x618754  # the exe's empty string
NAMED = 0x4b4  # a text's +0x4ac in MW3's: PM's text has two more words before it
STATE, PANEL, UNIT = 0x4b8, 0x4bc, 0x4c0
SLOT = {'e': 0x3c, 'l': 0x40, 's': 0x44, 'r': 0x48, 'f': 0xb0, 'd': 0x04}
COVER = {0x46c19e: 'enemy order: lit', 0x46c277: 'ally order: dark', 0x46c3ad: 'repair: lit',
         0x46981b: 'order pressed lit', 0x469845: 'order pressed dark', 0x469870: 'order released',
         0x46abf7: 'pick: released', 0x46ac0d: 'pick: pressed', 0x469b26: 'a lancemate line'}
REACHED = set()


def config(seed):
    r = random.Random(f'cfg:{seed}')
    ents = []
    team = r.choice([2, 4])
    for _ in range(r.choice([0, 1, 2, 3, 3, 4, 5, 6])):
        ents.append({'active': int(r.random() < 0.9), 'xbc': r.choice([0x18, 0x18, 0x18, 0x18, 0x10, 0x08, 0x19, 0]),
                     'allies': r.choice([team, team, team, 6, 6 - team, 0]),
                     'name': r.choice(['Dominic', 'Fox', 'Pacer', 'Lt. Ravel', None])})
    pallies = r.choice([2, 4, 6])
    # The enemies: every team but his and his allies' as the game sets them, or not quite (bit 0, a team left out).
    enemies = r.choice([~(pallies | 1), ~(pallies | 1), ~pallies, ~(pallies | 1) & ~0x10]) & 0xffffffff
    return {'ents': ents, 'team': team, 'pallies': pallies, 'enemies': enemies,
            'made': int(r.random() < 0.3), 'sel': r.choice([0, 0x51000]), 'roll': r.choice([0, 0x52000]),
            'x': r.choice([500, 660, 884]), 'y': 0x5f, 'lh': r.choice([12, 12, 14])}


def make_plan(seed, cfg):
    r = random.Random(f'plan:{seed}')
    mates = sum(1 for e in cfg['ents'] if e['active'] and e['xbc'] & 0x10)
    places = max(3, mates)
    n = places + 5  # the rows with an order item (not the last, order 2)
    lines = [f'V {0:x} {e["active"]:x} {e["xbc"]:x} {e["allies"]:x} {(e["name"] or "-").replace(" ", "_")}'
             for e in cfg['ents']]  # handles patched in
    lines.append(f'P {cfg["team"]:x} {cfg["pallies"]:x} {cfg["enemies"]:x}')
    lines.append(f'G {cfg["made"]:x} {cfg["sel"]:x} {cfg["roll"]:x} MARKER')
    lines.append(f'K {r.randrange(1, 9):x} {r.randrange(2):x}')
    lines.append(f'N {cfg["x"]} {cfg["y"]} {cfg["lh"]}')
    for _ in range(r.randrange(10, 50)):
        roll = r.random()
        i = r.randrange(-1, n + 1)
        if roll < 0.2:
            lines.append(f'p {r.randrange(0, 6)}')
        elif roll < 0.3:
            lines.append(f'M {cfg["x"] + r.randrange(-10, 120)} {cfg["y"] + r.randrange(-5, 12 * n + 10)}')
        elif roll < 0.35:
            lines.append(f'K {r.randrange(1, 9):x} {r.randrange(2):x}')
        elif roll < 0.4:
            lines.append(f'u {r.choice([-1, 1])}')  # from the panel's places (+0x74), as its callers
        elif roll < 0.5:
            lines.append(f'd {r.randrange(places, n)}')
        else:
            lines.append(f'{r.choice("elsrff")} {i}')
    lines.append('X')
    return lines


class Original:
    def __init__(self, seed, cfg, plan):
        self.t = Trace(seed=seed)
        w = self.w = TwodWorld(self.t, seed)
        e = self.e = w.emu
        self.cfg, self.plan = cfg, plan
        ret_self = ('c', 0, lambda emu, k, a, pop: emu._ret(a[0], pop))
        self.player = e.object('@player', 0x200, {0x58: ret_self, 0x28: ret_self},
                               fields={0xc0: cfg['team'], 0xc4: cfg['pallies'], 0xc8: cfg['enemies']})
        self.ctl = c = e.world_alloc(0x200)
        vbt = e.world_alloc(0x10)
        e.w32(c + 4, vbt)
        e.w32(vbt + 4, 0x20)
        e.w32(c + 0x40, self.player)
        self.cursor_place = e.world_alloc(0x10)
        self.ents, self.vehicles = [], []
        for i, d in enumerate(cfg['ents']):
            v = e.object(f'@m{i}', 0x200, {0xb0: ('', 0)}, fields={0xc4: d['allies']})
            e.w8(v + 8, d['active'])
            e.w8(v + 0xbc, d['xbc'])
            w.answers[f'@m{i}.vt+0xb0'] = e.world_string(d['name']) if d['name'] else 0
            ent = e.object(f'@e{i}', 0x40, {0x2c: ('', 0, lambda emu, k, a, pop, v=v: emu._ret(v, pop))})
            self.ents.append(ent)
            self.vehicles.append(v)
        lst = e.world_alloc(4 * len(self.ents) + 4)
        for i, ent in enumerate(self.ents):
            e.w32(lst + 4 * i, ent)
        e.w32(LIST, lst)
        e.w32(LIST + 4, lst + 4 * len(self.ents))
        # The cursor's possible entities: the marker, an ally, an enemy and one of neither (team 0's bit 0).
        self.marker = e.object('@marker', 0x100, {0x28: ret_self}, fields={0xc0: 1})
        ally = e.object('@ally', 0x100, {0x28: ret_self}, fields={0xc0: cfg['pallies'] & -cfg['pallies']})
        foe = e.object('@foe', 0x100, {0x28: ret_self}, fields={0xc0: 0x10})
        neutral = e.object('@neutral', 0x100, {0x28: ret_self}, fields={0xc0: 1})
        e.w32(MARKER, self.marker)
        e.stub(0x40f160, '0x0040e920', '', 0, lambda emu, k, a, pop: emu._ret(
            self.t.handle(k, (emu.r32(emu.uc.reg_read(UC_X86_REG_ECX)),)), 0))
        w.answers['0x0040e920'] = lambda rr, k, a: rr.choice([0, self.marker, ally, foe, foe, neutral])
        e.stub(0x45b0d0, '0x0045a000', '', 0)
        w.answers['0x0045a000'] = lambda rr, k, a: rr.choice([0, 1, 2])
        self.keys()
        e.w32(MADE, cfg['made'])
        if cfg['made']:
            for i in range(8):
                e.w32(COLOURS + 8 * i, 0x10101 * (i + 1))
                e.w32(COLOURS + 4 + 8 * i, i)
            e.w32(YELLOW, 0x111)
            e.w32(YELLOW + 4, 0x11)
            e.w32(WHITE, 0x222)
            e.w32(WHITE + 4, 0x22)
        e.w32(SELECT, cfg['sel'])
        e.w32(ROLLOVER, cfg['roll'])
        for addr in COVER:
            e.uc.hook_add(1 << 2, lambda uc, ad, sz, u: REACHED.add(ad), begin=addr, end=addr)

    def keys(self):
        """PM's key calls (cdecl): their answers say what they wrote, as the probe reads them."""
        e, w, t = self.e, self.w, self.t
        texts = {}

        def key_name(emu, k, a, pop):
            key, buf, n = (emu.r32(emu.uc.reg_read(UC_X86_REG_ESP) + 4 * i) for i in (1, 2, 3))
            if t.handle('key_name', (key, n)):
                e.cpu.write(buf, f'K{key:x}'.encode()[:n - 1] + b'\0')
                emu._ret(buf, 0)
            else:
                emu._ret(BLANK, 0)

        def action_name(emu, k, a, pop):
            action, buf, n = (emu.r32(emu.uc.reg_read(UC_X86_REG_ESP) + 4 * i) for i in (1, 2, 3))
            v = t.handle('action_name', (action, n))
            if v:
                e.cpu.write(buf, ACTIONS[(v - 1) % 3].encode() + b'\0')
            emu._ret(buf if v else 0, 0)

        def message(emu, k, a, pop):
            i = emu.r32(emu.uc.reg_read(UC_X86_REG_ESP) + 4)
            s = texts.setdefault(i, e.world_string(f'm{i}'))
            emu._ret(s, 0)

        def bind(emu, k, a, pop):
            action, text, key = (emu.r32(emu.uc.reg_read(UC_X86_REG_ESP) + 4 * i) for i in (1, 2, 3))
            ids = [i for i, s in texts.items() if s == text]
            emu._ret(t.handle('bind', (action, ids[0] if ids else 0, key)) and action, 0)

        def callback(emu, k, a, pop):
            emu._ret(t.handle('callback', (emu.r32(emu.uc.reg_read(UC_X86_REG_ESP) + 4),)), 0)

        def key_of(emu, k, a, pop):
            emu._ret(t.handle('key_of', (emu.r32(emu.uc.reg_read(UC_X86_REG_ESP) + 4),)), 0)
        for addr, key, fn in ((0x536a60, 'key_of', key_of), (0x536c90, 'key_name', key_name),
                              (0x536c70, 'action_name', action_name), (0x596e50, '@message', message),
                              (0x536c00, 'bind', bind), (0x536c30, 'callback', callback)):
            e.stub(addr, key, '', 0, fn)
        w.answers['key_of'] = lambda rr, k, a: rr.choice([0, 0x30 + rr.randrange(16), 0x130 + rr.randrange(16)])
        w.answers['key_name'] = lambda rr, k, a: int(rr.random() < 0.8)
        w.answers['action_name'] = lambda rr, k, a: rr.choice([0, 1, 1, 2, 2, 3])
        w.answers['bind'] = 1
        w.answers['callback'] = 0

    def vcall(self, obj, slot, stack=()):
        return self.e.call(self.e.r32(self.e.r32(obj) + slot), ecx=obj, stack=stack)

    def items(self):
        e, p = self.e, self.panel
        first, last = e.r32(p + 0x544), e.r32(p + 0x548)
        return [e.r32(first + 4 * i) for i in range((last - first) // 4)] if first else []

    def run(self):
        e = self.e
        self.snapshot = e.snapshot()
        self.panel = e.alloc(0x550)
        self.out = ''
        for line in self.plan:
            if self.op(line.split()):
                break
        return self.out

    def op(self, a):
        """One plan line; True at its end ('X')."""
        e, p, c = self.e, self.panel, self.ctl
        op = a[0]
        if op == 'K':
            e.w32(c + 0x184, int(a[1], 16))
            e.w32(c + 0x188, self.cursor_place if int(a[2], 16) else 0)
        elif op == 'N':
            e.call(CTOR, ecx=p, stack=(c, int(a[1]), int(a[2]), int(a[3])))
        elif op == 'M':
            e.w32(p + 0x14, int(a[1]) & 0xffffffff)
            e.w32(p + 0x18, int(a[2]) & 0xffffffff)
        elif op == 'p':
            self.t.handle('pick', (e.call(PICK, ecx=p, stack=(int(a[1]),)) & 0xff and 1,))
        elif op == 'u':
            e.call(PICKED, ecx=p, stack=(int(a[1]) & 0xffffffff, e.r32(p + 0x74)))
        elif op == 'X':
            self.out = self.dump()
            e.call(DTOR, ecx=p)
            return True
        elif op in SLOT:
            i = int(a[1])
            items = self.items()
            o = p + 0x7c if i < 0 else (items[i] if i < len(items) else 0)
            if o:
                self.vcall(o, SLOT[op])
        return False

    def dump(self):
        e, p = self.e, self.panel
        objs = []
        q = e.r32(p + 8)
        while q:
            objs.append(q)
            q = e.r32(q + 4)
        out = []

        def kv(k, v):
            out.append(f'{k}={v & 0xffffffff:x} ')

        def rc(k, a):
            vals = [struct.unpack('<i', struct.pack('<I', e.r32(a + 4 * i)))[0] for i in range(4)]
            out.append(f'{k}=' + ','.join(str(v) for v in vals) + ' ')

        def s(k, a):
            out.append(f'{k}="' + e.cstr(a).replace(' ', '_') + '" ')

        def fonttext(o):
            nxt = e.r32(o + 4)
            out.append(f'next=o{objs.index(nxt) if nxt in objs else -1} ')
            kv('flags', e.r32(o + 0xc))
            kv('timer', e.r32(o + 0x10))
            kv('x', e.r32(o + 0x14))
            kv('y', e.r32(o + 0x18))
            out.append(f'bg=T{e.r32(o + 0x1c):x} ')
            rc('bgr', o + 0x20)
            kv('mouse', e.r32(o + 0x30) & 0xffff)
            s('text', o + 0x34)
            kv('align', e.r32(o + 0x244))
            out.append(f'timg=T{e.r32(o + 0x248):x} ')
            for k, off in (('top', 0x24c), ('bottom', 0x250), ('hfont', 0x258)):
                kv(k, e.r32(o + off))
            s('rendered', o + 0x260)
            for k, off in (('w', 0x460), ('h', 0x464), ('shadow', 0x468), ('bk', 0x46c), ('nr', 0x474)):
                kv(k, e.r32(o + off))
            rc('tr', o + 0x490)
            kv('maxw', e.r32(o + 0x4a8))
            kv('named', e.r32(o + NAMED))

        def item(o):
            if not o:
                out.append('Z ')
                return
            k = VT_KIND.get(e.r32(o), '?')
            out.append(f'{k} ')
            kv('state', e.r32(o + STATE))
            if k == 'M':
                u = e.r32(o + UNIT)
                kv('unit', self.vehicles.index(u) if u in self.vehicles else -1)
            else:
                kv('order', e.r32(o + UNIT))
            fonttext(o)
        items = self.items()
        out.append('panel ')
        kv('active', e.r32(p + 4))
        for k, off in (('mates', 0x70), ('places', 0x74), ('picked', 0x78), ('lh', 0x6c)):
            kv(k, e.r32(p + off))
        rc('rect', p + 0x5c)
        kv('n', len(items))
        kv('x184', e.r32(self.ctl + 0x1a4))
        kv('order', e.r32(self.ctl + 0x180))
        kv('cursor', e.r32(CURSOR))
        out.append('\ncolours ')
        for i in range(8):
            kv('c', e.r32(COLOURS + 8 * i))
            kv('p', e.r32(COLOURS + 4 + 8 * i) & 0xffff)
        kv('y', e.r32(YELLOW))
        kv('yp', e.r32(YELLOW + 4) & 0xffff)
        kv('w', e.r32(WHITE))
        kv('wp', e.r32(WHITE + 4) & 0xffff)
        out.append('\ntarget ')
        item(p + 0x7c)
        for i, o in enumerate(items):
            out.append(f'\nitem{i} ')
            item(o)
        out.append('\n')
        return ''.join(out)


def run_case(seed, verbose):
    cfg = config(seed)
    plan = make_plan(seed, cfg)
    orig = Original(seed, cfg, plan)
    plan = [ln.replace('MARKER', f'{orig.marker:x}') for ln in plan]
    vi = iter(orig.ents)
    plan = [('V ' + f'{next(vi):x} ' + ' '.join(ln.split()[2:])) if ln.startswith('V ') else ln for ln in plan]
    orig.plan = plan
    dump_o = orig.run()
    rep = Replay(orig.t, orig.e, orig.snapshot)
    probe.probe_app_set_callback(ctypes.cast(rep.callback(), ctypes.c_void_p))
    buf = ctypes.create_string_buffer(1 << 20)
    n = probe.probe_pm_pocket_panel_run('\n'.join(plan).encode(), buf, len(buf))
    assert n >= 0, 'probe failed'
    dump_r = buf.value.decode('latin1')
    stubbed = orig.w.stub_textures
    to, do = canonical(orig.t.log, dump_o, stubbed)
    tr, dr = canonical(rep.log, dump_r, stubbed)
    ok = compare(f'panel seed {seed}', to, tr)
    if do != dr:
        ok = False
        for a, b in zip(do.split('\n'), dr.split('\n')):
            if a != b:
                print(f'panel seed {seed}: dumps differ')
                ta, tb = a.split(' '), b.split(' ')
                print('  original:', ' '.join(x for x in ta if x not in tb)[:400])
                print('  rebuild: ', ' '.join(x for x in tb if x not in ta)[:400])
                print('  in:', a[:100])
                break
        else:
            print(f'panel seed {seed}: dumps differ in length')
    if verbose and ok:
        print(f'seed {seed}: {len(to)} calls')
    keyed = sum(1 for ln in do.split('\n') if ln.startswith('item') and 'text="K' in ln)
    return ok, len(to), do.count(' M state') - 1, keyed


def main():
    args = [x for x in sys.argv[1:] if not x.startswith('--')]
    runs = int(args[0]) if args else 200
    verbose = '--verbose' in sys.argv
    passed = calls = lines = keyed = 0
    for seed in range(runs):
        ok, n, m, k = run_case(seed, verbose)
        passed += ok
        calls += n
        lines += m
        keyed += k
    never = [COVER[a] for a in COVER if a not in REACHED]
    print(f'pm pocket panel: {passed}/{runs} runs identical ({calls} calls, {lines} lancemate lines, {keyed} lines with a key)'
          + (f'; branches never reached: {never}' if never else ''))
    if passed != runs or never:
        return 1
    pm_floors.floors(runs=(runs, 100), calls=(calls, 5000), lines=(lines, 100), keyed=(keyed, 200))
    return 0


if __name__ == '__main__':
    code = main()
    sys.stdout.flush()
    os._exit(code)
