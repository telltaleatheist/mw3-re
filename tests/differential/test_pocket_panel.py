"""The pocket commander's panel (docs/ai.md 17.6b) against the original x86:
its constructor 0x468250 (the target line 0x467620, the lancemates' lines
0x467fa0 over zEntity's list, the orders' lines 0x4680c0 / 0x4679a0 from the
table at 0x605cd8, the colours 0x4686e0), its lines' virtuals (the mouse
0x4675e0 / 0x467610, the presses 0x4676c0 / 0x467710 / 0x467c90 / 0x467d00,
the refreshes 0x467580 / 0x4676a0 / 0x46a5a0 / 0x46a670 / 0x46a740 /
0x46a7b0 / 0x46a810, the orders' draw 0x467d40), the pick 0x469060, the
count 0x4687e0 and the destructor 0x468720; then the table's loader
0x467d80.

    cmake --build build && .venv/bin/python tests/differential/test_pocket_panel.py [runs]

The world is made up (twod_world.py's zTwoD outside, plus): the mode's
controls (+0x160, +0x164 the cursor's node, +0x168 its place, +0x184, the
player through the virtual base), the player (vt+0x50, vt+0x28 itself:
+0xc0 his team, +0xc4 his allies), zEntity's list of entities (vt+0x2c
their vehicle: +0x08 up, +0xbc, +0xc4 its allies, vt+0xa4 a name or none),
the cursor's entity (0x40eee0: none, the marker [0x605f58] or a unit of a
team), the repair bays (0x45a000), the sounds [0x605f44] / [0x605f48], the
colours made or not. Each run builds the panel and runs a random plan; the
calls out and a dump of the panel and its lines must match the port's
(mech3/pocket_panel, tests/probe_pocket_panel.cpp). The x87 runs at 24 bits.
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import Replay, Trace, compare, load_probe  # noqa: E402
from original import Original as Cpu  # noqa: E402
from twod_world import TwodWorld  # noqa: E402
from test_twod import canonical  # noqa: E402

probe = load_probe()
probe.probe_app_set_callback.argtypes = [ctypes.c_void_p]
probe.probe_pocket_panel_run.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
probe.probe_pocket_load_commands.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p]

CTOR, DTOR, PICK, PICKED = 0x468250, 0x468720, 0x469060, 0x4687e0
VT_KIND = {0x5c2ca0: 'M', 0x5c3200: 'A', 0x5c2fd8: 'E', 0x5c3090: 'L', 0x5c3148: 'S', 0x5c32b8: 'O'}
ORDERS = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13]
SLOT = {'e': 0x3c, 'l': 0x40, 's': 0x44, 'r': 0x48, 'f': 0xb0, 'd': 0x04}
COVER = {0x46a607: 'enemy order: lit', 0x46a6f0: 'ally order: dark', 0x46a82d: 'repair: lit',
         0x467cab: 'order pressed lit', 0x467cd5: 'order pressed dark', 0x467d00: 'order released',
         0x46909d: 'pick: released', 0x4690b5: 'pick: pressed', 0x46804f: 'a lancemate line'}
REACHED = set()


def config(seed):
    r = random.Random(f'cfg:{seed}')
    ents = []
    team = r.choice([2, 4])
    for _ in range(r.choice([0, 1, 2, 3, 3, 4, 5, 6])):
        ents.append({'active': int(r.random() < 0.9), 'xbc': r.choice([0x18, 0x18, 0x18, 0x18, 0x10, 0x08, 0x19, 0]),
                     'allies': r.choice([team, team, team, 6, 6 - team, 0]),
                     'name': r.choice(['Dominic', 'Fox', 'Pacer', 'Lt. Ravel', None])})
    rows = []
    for _ in range(r.choice([0, 3, 5, 6, 6])):
        rows.append((r.choice(['K', 'Ctrl+K', '-', 'F1']), r.choice(['Attack_my_target', 'Form_on_me', 'Hold']),
                     r.choice(ORDERS)))
    return {'ents': ents, 'rows': rows, 'team': team, 'pallies': r.choice([2, 4, 6]),
            'made': int(r.random() < 0.3), 'sel': r.choice([0, 0x51000]), 'roll': r.choice([0, 0x52000]),
            'x': r.choice([500, 660, 884]), 'y': 0x5f, 'lh': r.choice([12, 12, 14])}


def make_plan(seed, cfg):
    r = random.Random(f'plan:{seed}')
    mates = sum(1 for e in cfg['ents'] if e['active'] and e['xbc'] & 0x10)
    places = max(3, mates)
    n = places + sum(1 for k, nm, o in cfg['rows'] if o in (0, 1, 3, 4, 5, 7, 8, 9, 10, 11, 12))
    lines = []
    for k, nm, o in cfg['rows']:
        lines.append(f'c {k} {nm} {o}')
    lines += [f'V {0:x} {e["active"]:x} {e["xbc"]:x} {e["allies"]:x} {(e["name"] or "-").replace(" ", "_")}'
              for e in cfg['ents']]  # handles patched in
    lines.append(f'P {cfg["team"]:x} {cfg["pallies"]:x}')
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
        elif roll < 0.5 and n > places:
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
        self.player = e.object('@player', 0x200, {0x50: ret_self, 0x28: ret_self},
                               fields={0xc0: cfg['team'], 0xc4: cfg['pallies']})
        self.ctl = c = e.world_alloc(0x200)
        vbt = e.world_alloc(0x10)
        e.w32(c + 4, vbt)
        e.w32(vbt + 4, 0x20)
        e.w32(c + 0x34, self.player)
        self.cursor_place = e.world_alloc(0x10)
        self.ents, self.vehicles = [], []
        for i, d in enumerate(cfg['ents']):
            v = e.object(f'@m{i}', 0x200, {0xa4: ('', 0)}, fields={0xc4: d['allies']})
            e.w8(v + 8, d['active'])
            e.w8(v + 0xbc, d['xbc'])
            w.answers[f'@m{i}.vt+0xa4'] = e.world_string(d['name']) if d['name'] else 0
            ent = e.object(f'@e{i}', 0x40, {0x2c: ('', 0, lambda emu, k, a, pop, v=v: emu._ret(v, pop))})
            self.ents.append(ent)
            self.vehicles.append(v)
        lst = e.world_alloc(4 * len(self.ents) + 4)
        for i, ent in enumerate(self.ents):
            e.w32(lst + 4 * i, ent)
        e.w32(0x73655c, lst)
        e.w32(0x736560, lst + 4 * len(self.ents))
        # The cursor's possible entities: the marker, an ally and an enemy of the player.
        self.marker = e.object('@marker', 0x100, {0x28: ret_self}, fields={0xc0: 1})
        ally = e.object('@ally', 0x100, {0x28: ret_self}, fields={0xc0: cfg['pallies'] & -cfg['pallies']})
        foe = e.object('@foe', 0x100, {0x28: ret_self}, fields={0xc0: 0x10})
        e.w32(0x605f58, self.marker)
        e.stub(0x40eee0, '0x0040e920', '*c', 0)
        w.answers['0x0040e920'] = lambda rr, k, a: rr.choice([0, self.marker, ally, foe, foe])
        e.stub(0x45a000, '0x0045a000', '', 0)
        w.answers['0x0045a000'] = lambda rr, k, a: rr.choice([0, 1, 2])
        e.w32(0x605f54, len(cfg['rows']))
        for i, (k, nm, o) in enumerate(cfg['rows']):
            row = 0x605cd8 + 0x54 * i
            e.cpu.write(row, bytes(0x54))
            e.cpu.write(row, ('' if k == '-' else k).encode() + b'\0')
            e.cpu.write(row + 0x10, nm.replace('_', ' ').encode() + b'\0')
            e.w32(row + 0x50, o)
        e.w32(0x605f4c, cfg['made'])
        if cfg['made']:
            for i in range(8):
                e.w32(0x605ee8 + 8 * i, 0x10101 * (i + 1))
                e.w32(0x605eec + 8 * i, i)
            e.w32(0x605cc8, 0x111)
            e.w32(0x605ccc, 0x11)
            e.w32(0x605cd0, 0x222)
            e.w32(0x605cd4, 0x22)
        e.w32(0x605f48, cfg['sel'])
        e.w32(0x605f44, cfg['roll'])
        for addr in COVER:
            e.uc.hook_add(1 << 2, lambda uc, ad, sz, u: REACHED.add(ad), begin=addr, end=addr)

    def vcall(self, obj, slot, stack=()):
        return self.e.call(self.e.r32(self.e.r32(obj) + slot), ecx=obj, stack=stack)

    def items(self):
        e, p = self.e, self.panel
        first, last = e.r32(p + 0x53c), e.r32(p + 0x540)
        return [e.r32(first + 4 * i) for i in range((last - first) // 4)] if first else []

    def run(self):
        e = self.e
        self.snapshot = e.snapshot()
        p = self.panel = e.alloc(0x548)
        c = self.ctl
        dump = ''
        for line in self.plan:
            a = line.split()
            op = a[0]
            if op == 'K':
                e.w32(c + 0x164, int(a[1], 16))
                e.w32(c + 0x168, self.cursor_place if int(a[2], 16) else 0)
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
                dump = self.dump()
                e.call(DTOR, ecx=p)
                break
            elif op in SLOT:
                i = int(a[1])
                items = self.items()
                o = p + 0x7c if i < 0 else (items[i] if i < len(items) else 0)
                if o:
                    self.vcall(o, SLOT[op])
        return dump

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
            kv('named', e.r32(o + 0x4ac))

        def item(o):
            if not o:
                out.append('Z ')
                return
            k = VT_KIND.get(e.r32(o), '?')
            out.append(f'{k} ')
            kv('state', e.r32(o + 0x4b0))
            if k == 'M':
                u = e.r32(o + 0x4b8)
                kv('unit', self.vehicles.index(u) if u in self.vehicles else -1)
            else:
                kv('order', e.r32(o + 0x4b8))
            fonttext(o)
        items = self.items()
        out.append('panel ')
        kv('active', e.r32(p + 4))
        for k, off in (('mates', 0x70), ('places', 0x74), ('picked', 0x78), ('lh', 0x6c)):
            kv(k, e.r32(p + off))
        rc('rect', p + 0x5c)
        kv('n', len(items))
        kv('x184', e.r32(self.ctl + 0x184))
        kv('order', e.r32(self.ctl + 0x160))
        kv('cursor', e.r32(0x605f5c))
        out.append('\ncolours ')
        for i in range(8):
            kv('c', e.r32(0x605ee8 + 8 * i))
            kv('p', e.r32(0x605eec + 8 * i) & 0xffff)
        kv('y', e.r32(0x605cc8))
        kv('yp', e.r32(0x605ccc) & 0xffff)
        kv('w', e.r32(0x605cd0))
        kv('wp', e.r32(0x605cd4) & 0xffff)
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
    n = probe.probe_pocket_panel_run('\n'.join(plan).encode(), buf, len(buf))
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
    return ok, len(to), do.count(' M state') - 1


# ---- the table's loader 0x467d80 ----

def load_commands(seed):
    """0x467d80 with random bindings (0x51d1f0), key names (0x51d330) and
    messages (0x576040), against the port's pocket_load_commands."""
    r = random.Random(f'cmds:{seed}')
    cpu = Cpu()
    bound = [(0x30 + i if r.random() < 0.7 else 0) for i in range(6)]
    keys = [f'K{b:x}' * r.choice([1, 2, 5]) for b in bound]
    texts = [r.choice(['Attack my target', 'Form on me', 'x' * 80]) for _ in range(6)]
    ids = [2195, 2196, 2142, 2143, 2197, 2204]
    calls = []
    cpu.stub_function(0x51d1f0, lambda c: (calls.append(c.ecx), bound[len(calls) - 1])[1])

    def key_name(c):
        cpu.write(c.edx, keys[bound.index(c.ecx)].encode()[:c.arg(0)].ljust(c.arg(0), b'\0'))
        return 0
    cpu.stub_function(0x51d330, key_name, 4)
    strs = {}
    for i, t in zip(ids, texts):
        a = strs[i] = cpu.alloc(len(t) + 1)
        cpu.write(a, t.encode() + b'\0')
    cpu.stub_function(0x576040, lambda c: strs[c.ecx])

    def strncpy(c):
        dst, src, n = c.arg(0), c.arg(1), c.arg(2)
        data = bytes(cpu.read(src, n)).split(b'\0')[0]
        cpu.write(dst, data.ljust(n, b'\0')[:n])
        return dst
    cpu.stub_import(0x5bb648, strncpy)
    cpu.write(0x605cd8, bytes(0x54 * 6))
    cpu.call(0x467d80)
    count = struct.unpack('<I', bytes(cpu.read(0x605f54, 4)))[0]
    a = bytes(cpu.read(0x605cd8, 0x54 * count))
    out = ctypes.create_string_buffer(0x54 * 6)
    n = probe.probe_pocket_load_commands((ctypes.c_uint32 * 6)(*bound), (ctypes.c_char_p * 6)(*[k.encode() for k in keys]),
                                         '\n'.join(texts).encode(), out)
    return count == n and a == out.raw[:0x54 * n] and calls == [84, 85, 82, 81, 86, 79]


def main():
    args = [x for x in sys.argv[1:] if not x.startswith('--')]
    runs = int(args[0]) if args else 200
    verbose = '--verbose' in sys.argv
    passed = calls = lines = 0
    for seed in range(runs):
        ok, n, m = run_case(seed, verbose)
        passed += ok
        calls += n
        lines += m
    never = [COVER[a] for a in COVER if a not in REACHED]
    tables = sum(load_commands(seed) for seed in range(runs))
    print(f'pocket panel: {passed}/{runs} runs identical ({calls} calls, {lines} lancemate lines); command tables {tables}/{runs} identical'
          + (f'; branches never reached: {never}' if never else ''))
    return 0 if passed == runs and tables == runs and not never else 1


if __name__ == '__main__':
    sys.exit(main())
