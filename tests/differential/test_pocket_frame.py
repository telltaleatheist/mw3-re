"""The pocket commander's frame (docs/ai.md 17.6c) against the original x86:
0x469d00 on the panel (test_pocket_panel.py's, built by 0x468250) with its
mouse 0x468820, the cursor's three ways (the trigger let go: the reticle's
pick 0x444330 or the map's 0x444670 / 0x4587b0, a bare spot putting the
marker [0x605f58] there; else the player's target contact (+0x6c,
0x4e6fe0); else a network game's element 0x58b060), the lines re-lit, the
aim copied and the way out (+0x148 / +0x14c, 0x4b5290, 0x40ae70, +0x38).

    cmake --build build && .venv/bin/python tests/differential/test_pocket_frame.py [runs]

The world is test_pocket_panel.py's plus: the view [0x5f4c8c] (+0x64 the
player or not), the HUD (0x430aa0: +0x70 the map up, the reticle's spot
+0x144c +0x8fc), the map (0x491c80, +0xd8's size), the pick records (+0x24
a node or none), four entities and the marker (+0x04 a node, vt+0xa4 a name
or none, 0x5a85e0 a place), the node's entity (0x40eee0), the trigger
(0x467480), the target contact, a network game's elements (+0x18's
vt+0x2c an entity or none, vt+0x18 a place). Each run builds the panel and
runs frames with the mouse over its lines among the other ops; the calls
out and the dump (the panel's and the mode's fields) must match the port's
(mech3/pocket_panel's pocket_frame, tests/probe_pocket_panel.cpp).
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import Replay, compare  # noqa: E402
import test_pocket_panel as tp  # noqa: E402
from test_twod import canonical  # noqa: E402

probe = tp.probe
NODES = 0x2c3f0000
COVER = {0x4678c5: 'a line: from the left edge (the unit inside the text)', 0x4678bf: 'a line: the right edge',
         0x468a6d: 'the mark sized', 0x468ab2: 'the mark on the map', 0x468b66: 'the spot mark yellow',
         0x468b79: 'the spot mark white', 0x46783b: 'a line drawn',
         0x469e30: 'a map pick', 0x469e43: 'the reticle (map up, outside it)', 0x469eae: 'a pick with a node',
         0x469f37: 'a bare spot: the marker', 0x46a008: 'the target moved', 0x46a10a: 'the net: an entity',
         0x46a18f: 'the net: the marker', 0x46a2a9: 'the way out starts', 0x46a2e7: 'the camera back',
         0x46a336: 'done (+0x38)', 0x4688a9: 'a line clicked'}
REACHED = set()


def fbits(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


def frame_config(seed):
    r = random.Random(f'frame:{seed}')
    return {'viewed': 1,  # not viewed: the exe reads the HUD through null (FIX)
            'map': r.choice([0, 0, 1]), 'mapw': r.choice([100, 400]),
            'maph': r.choice([100, 300]), 'net': int(r.random() < 0.3), 'cam': r.choice([0, 1, 3]),
            'active': r.randrange(2)}


class Original(tp.Original):
    def __init__(self, seed, cfg, fcfg, plan):
        super().__init__(seed, cfg, plan)
        e, w = self.e, self.w
        r = random.Random(f'world:{seed}')
        self.fcfg = fcfg
        ww = e.world_w32
        names = {}
        name_stub = e.new_stub('name', 'c', 0)
        self_stub = e.new_stub('@self', 'c', 0, lambda emu, k, a, pop: emu._ret(a[0], pop))
        w.answers['name'] = lambda rr, k, a: names.get(a[0], 0)

        def entity(name):
            o, vt = e.world_alloc(0x100), e.world_alloc(0x100)
            e.w32(o, vt)
            e.w32(vt + 0xa4, name_stub)
            e.w32(vt + 0x28, self_stub)  # its team (the orders' lit tests)
            ww(o + 0xc0, r.choice([cfg['pallies'], 0x10, 1]))
            ww(o + 4, 0x7700 + len(names))  # its node
            names[o] = e.world_string(name) if name else 0
            return o
        self.entities = [entity(r.choice(['Dominic', 'Fox', 'Heavy Turret', None])) for _ in range(4)]
        # The marker (the panel's world made one): a node, a name, vt+0x18.
        mk = self.marker
        ww(mk + 4, 0x7799)
        names[mk] = e.world_string('Nav Alpha') if r.random() < 0.5 else 0
        mvt = e.r32(mk)
        e.w32(mvt + 0xa4, name_stub)
        ctl = self.ctl

        def marker_move(emu, key, a, pop):
            emu.t.handle(key, (1 if a[0] == ctl + 0x16c else 2 if a[0] == self.cursor_place else a[0],))
            emu._ret(0, pop)
        e.w32(mvt + 0x18, e.new_stub('marker.vt+0x18', 's0', 4, marker_move))
        e.w8(mk + 8, fcfg['active'])
        ents = self.entities + [mk]
        w.answers['0x0040e920'] = lambda rr, k, a: rr.choice([0, 0] + ents)
        e.stub(0x5a85e0, '0x005a85e0', 'c', 0)
        w.answers['0x005a85e0'] = lambda rr, k, a: a[0] + 0x40
        e.stub(0x4e6fe0, '0x004e6fe0', '', 0)
        w.answers['0x004e6fe0'] = lambda rr, k, a: rr.choice(ents)
        e.stub(0x4eb170, '0x004eb170', 'c,d', 0)
        e.stub(0x4a7b40, '0x004a7b40', 's0', 4)
        e.stub(0x576040, '0x00576040', 'c', 0)
        w.answers['0x00576040'] = e.world_string('Unknown')
        e.stub(0x467480, '0x0051d300', '', 0)
        w.answers['0x0051d300'] = lambda rr, k, a: rr.choice([0, 0, 4, 4, 5, 1])
        # The view and the HUD.
        view = e.world_alloc(0x100)
        e.w32(0x5f4c8c, view)
        e.w32(view + 0x64, self.player if fcfg['viewed'] else 0x1234)
        e.stub(0x4b5290, '0x004b5290', 's0', 4)
        e.stub(0x40ae70, '0x0040ae70', 's0', 4)
        hud = e.world_alloc(0x3000)
        ww(hud + 0x70, fcfg['map'])
        for i in range(3):
            ww(hud + 0x144c + 0x8fc + 4 * i, fbits(r.uniform(-500, 500)))
        e.stub(0x430aa0, '0x00430aa0', '', 0)
        w.answers['0x00430aa0'] = hud
        mp, size = e.world_alloc(0x100), e.world_alloc(0x20)
        ww(mp + 0xd8, size)
        ww(size + 8, fcfg['mapw'])
        ww(size + 0xc, fcfg['maph'])
        e.stub(0x491c80, '0x00491c80', '', 0)
        w.answers['0x00491c80'] = mp
        nodes = [0x7700, 0x7701, 0x7702, 0, 0x7799, 0x7703, 0, 0x7710]
        for i, n in enumerate(nodes):
            ww(NODES + 4 * i, n)
        recs = []
        for n in (0x7700, 0x7702, 0, 0x7799, 0x7710):
            q = e.world_alloc(0x40)
            ww(q + 0x24, n)
            recs.append(q)
        e.stub(0x444330, '0x00444330', 's0', 4)
        w.answers['0x00444330'] = lambda rr, k, a: rr.choice([0] + recs)

        def map_mouse(emu, key, a, pop):
            v = emu.t.handle(key, ())
            emu.w32(a[0], v & 0xfff)
            emu.w32(a[1], (v >> 12) & 0xfff)
            emu._ret(0, pop)
        e.stub(0x444670, '0x00444670', 's0,s1', 8, map_mouse)
        mw, mh = fcfg['mapw'], fcfg['maph']  # the edges themselves too: outside the map
        w.answers['0x00444670'] = lambda rr, k, a: (rr.choice([rr.randrange(0, 500), mw - 1, mw])
                                                   | rr.choice([rr.randrange(0, 400), mh - 1, mh]) << 12)

        def map_pick(emu, key, a, pop):
            v = emu.t.handle(key, (a[0], a[1]))
            if v & 1:
                for i, f in enumerate((float(v >> 8), 2.0, -float(v >> 8))):
                    emu.w32(a[2] + 4 * i, fbits(f))
                emu.w32(a[3] + 0x24, emu.r32(NODES + 4 * ((v >> 1) & 0x7f)))
            emu._ret(v & 1, pop)
        e.stub(0x4587b0, '0x004587b0', 's0,s1,s2,s3', 0x10, map_pick)
        w.answers['0x004587b0'] = lambda rr, k, a: rr.choice([0, 1]) | rr.randrange(8) << 1 | rr.randrange(1, 900) << 8
        # A network game's elements.
        if fcfg['net']:
            game = e.world_alloc(0x40)
            e.w32(0x609980, game)
            nets = []
            for i in range(3):
                n, vt = e.world_alloc(0x80), e.world_alloc(0x40)
                e.w32(n, vt)
                e.w32(vt + 0x18, e.new_stub('net.vt+0x18', 'c', 0))
                holder = 0
                if i:
                    holder, hvt = e.world_alloc(0x40), e.world_alloc(0x40)
                    e.w32(holder, hvt)
                    e.w32(hvt + 0x2c, e.new_stub('holder.vt+0x2c', 'c', 0))
                ww(n + 0x18, holder)
                nets.append((n, holder))
            hmap = {h: (self.entities[1] if i == 1 else mk) for i, (n, h) in enumerate(nets) if h}
            w.answers['holder.vt+0x2c'] = lambda rr, k, a: hmap.get(a[0], 0)
            w.answers['net.vt+0x18'] = lambda rr, k, a: a[0] + 0x40
            e.stub(0x58b060, '0x0058b060', '', 4)
            w.answers['0x0058b060'] = lambda rr, k, a: rr.choice([0] + [n for n, h in nets])
        else:
            e.w32(0x609980, 0)

        def mouse(emu, key, a, pop):
            emu.t.handle('0x0051b7b0', ())
            vals = w.mouse_frames[w.frame] if w.frame < len(w.mouse_frames) else [0] * 15
            for i, v in enumerate(vals):
                emu.w32(a[0] + 4 * i, v)
            emu._ret(0, pop)
        e.stub(0x51ba60, '0x0051b7b0', 'c', 0, mouse)
        # The draws' world: every entity's and vehicle's node (+0x14: its data
        # +0x38, flagged or not, its place +0x84) and size (+0x68), the
        # projection's fy, the screen, the projections, the map's pixels.
        def node_for(o):
            n, dat = e.world_alloc(0x40), e.world_alloc(0x100)
            ww(o + 0x14, n)
            ww(n + 0x38, dat)
            ww(dat, r.choice([0, 0x20]))
            for i in range(3):
                ww(dat + 0x84 + 4 * i, fbits(r.uniform(-900, 900)))
            ww(o + 0x68, fbits(r.choice([2.5, 8.0, 15.0])))
        for o in self.entities + [mk] + self.vehicles:
            node_for(o)
        ww(0x73fba0, fbits(r.choice([320.0, 412.5])))
        screen = e.world_alloc(0x10)
        for i, v in enumerate((0, 0, r.choice([640, 800]), r.choice([480, 600]))):
            ww(screen + 4 * i, v)
        e.stub(0x40d7b0, '0x0040d7b0', '', 0)
        w.answers['0x0040d7b0'] = screen
        e.stub(0x4ed280, '0x004ed280', 'c,d', 0)
        for a in (0x51e9d0, 0x51fde0, 0x51ea00):
            e.stub(a, f'@{a:x}', '', 0)
        e.stub(0x40d4b0, '@pixel_double', '', 0)
        w.answers['@pixel_double'] = 0  # not doubled (the port's options: 0)
        e.stub(0x40da30, '@render_to_screen', '', 0)  # pixels not doubled: nothing (the port's the same)
        e.stub(0x4b51f0, '0x004b51f0', '', 0)
        e.stub(0x40ef40, '0x0040ee50', '*c', 0)
        w.answers['0x0040ee50'] = lambda rr, k, a: rr.randrange(2)
        depth = [0.5, 0.0, 1.5, 1.0]

        def project(emu, key, a, pop):
            p3 = [emu.r32(a[0] + 4 * i) for i in range(3)]
            v = emu.t.handle(key, tuple(p3))
            emu.cpu.write(a[1], struct.pack('<3f', float(v & 0x3ff), float((v >> 10) & 0x3ff), depth[(v >> 20) & 3]))
            emu._ret((v >> 24) & 0xff, pop)
        e.stub(0x53cf10, '0x0053cf10', 'c,d', 0, project)
        w.answers['0x0053cf10'] = lambda rr, k, a: (rr.randrange(-40, 900) & 0x3ff | rr.randrange(0, 700) << 10
                                                    | rr.randrange(4) << 20 | rr.choice([0, 0, 8]) << 24)

        def map_point(emu, key, a, pop):
            p3 = [emu.r32(a[1] + 4 * i) for i in range(3)]
            v = emu.t.handle(key, (a[0], *p3))
            emu.w32(a[2], v & 0xfff)
            emu.w32(a[2] + 4, (v >> 12) & 0xfff)
            emu._ret(1, pop)
        e.stub(0x4586e0, '0x004586e0', 'c,s0,s1', 8, map_point)
        w.answers['0x004586e0'] = lambda rr, k, a: rr.randrange(0, 600) | rr.randrange(0, 500) << 12
        e.stub(0x562940, '0x00562940', 'c,d,s0,s1,s2', 0xc)

        def line_list(emu, key, a, pop):
            n, clip = a[1], a[2]
            emu.t.handle(key, (n, *[emu.r32(clip + 4 * i) for i in range(4)], a[3]))
            for i in range(n + 1):
                emu.t.handle('pt', (emu.r32(a[0] + 8 * i), emu.r32(a[0] + 8 * i + 4)))
            emu._ret(0, pop)
        e.stub(0x562cb0, '0x00562cb0', 'c,d,s0,s1', 8, line_list)
        self.contacts = []
        for _ in range(3):
            q = e.world_alloc(0x40)
            e.w32(q, r.choice(self.entities))
            e.w32(q + 8, r.choice(ents))
            self.contacts.append(q)
        e.w32(ctl + 0x150, fcfg['cam'])
        for addr in COVER:
            e.uc.hook_add(1 << 2, lambda uc, ad, sz, u: REACHED.add(ad), begin=addr, end=addr)

    def op(self, a):
        e, c, w = self.e, self.ctl, self.w
        if a[0] == 'T':
            k = int(a[1])
            e.w32(self.player + 0x6c, self.contacts[k - 1] if k else 0)
        elif a[0] == 'Y':
            e.w32(c + 0x184, int(a[1]))
        elif a[0] == 'F':
            vals = [int(v, 16) for v in a[1:]]
            w.mouse_frames.append(vals[1:])
            e.w32(c + 0x180, self.panel)
            e.call(0x469d00, ecx=c, stack=(vals[0],))
            w.frame += 1
        elif a[0] == 'D':
            e.w32(c + 0x180, self.panel)
            e.w32(0x605f64, c)
            e.call(0x46a410, stack=(int(a[1], 16),))
            e.w32(0x605f64, 0)
        elif a[0] == 'X':
            self.out = self.dump() + self.dump_frame()
            e.call(tp.DTOR, ecx=self.panel)
            return True
        else:
            return super().op(a)
        return False

    def dump_frame(self):
        e, c = self.e, self.ctl
        out = ['frame ']

        def kv(k, v):
            out.append(f'{k}={v & 0xffffffff:x} ')
        for k, off in (('x148', 0x148), ('x14c', 0x14c), ('x154', 0x154), ('x164', 0x164)):
            kv(k, e.r32(c + off))
        x168 = e.r32(c + 0x168)
        kv('x168', 1 if x168 == c + 0x16c else 2 if x168 == self.cursor_place else x168)
        for i in range(3):
            kv('pl', e.r32(c + 0x16c + 4 * i))
        for k, off in (('x184', 0x184), ('x1b8', 0x1b8), ('net', 0x1bc)):
            kv(k, e.r32(c + off))
        kv('x1c0', e.r8(c + 0x1c0))
        kv('x1c1', e.r8(c + 0x1c1))
        kv('done', e.r32(c + 0x38))
        kv('aim_o', e.r32(c + 0x188))
        kv('aim_e', e.r32(c + 0x190))
        kv('cursor', e.r32(0x605f5c))
        kv('x50', e.r32(0x605f50))
        kv('x60', e.r32(0x605f60))
        kv('tgt', e.r32(self.panel + 0x534))
        kv('mk_on', e.r8(self.marker + 8))
        vals = [struct.unpack('<i', struct.pack('<I', e.r32(0x605ed8 + 4 * i)))[0] for i in range(4)]
        out.append('clip=' + ','.join(str(v) for v in vals) + ' ')
        out.append('\n')
        return ''.join(out)


def make_plan(seed, cfg, fcfg):
    r = random.Random(f'fplan:{seed}')
    base = tp.make_plan(seed, cfg)
    head = [ln for ln in base if ln[0] in 'cVPGKN']
    lines = head[:-1] + [f'W {fcfg["viewed"]:x} TARGET {fcfg["cam"]:x} {fcfg["active"]:x} {fcfg["net"]:x}', head[-1]]
    mates = sum(1 for e in cfg['ents'] if e['active'] and e['xbc'] & 0x10)
    n = max(3, mates) + sum(1 for k, nm, o in cfg['rows'] if o in (0, 1, 3, 4, 5, 7, 8, 9, 10, 11, 12))
    for _ in range(r.randrange(20, 70)):
        roll = r.random()
        if roll < 0.55:
            if r.random() < 0.7:  # over a line
                x, y = cfg['x'] + r.randrange(0, 60), cfg['y'] + cfg['lh'] * r.randrange(0, n + 1) + 3
            else:
                x, y = r.randrange(0, 640), r.randrange(0, 480)
            left = r.choice([0, 0, 0, 4, 1])
            right = r.choice([0, 0, 4])
            m = [x, y, 0, fbits(0.0), fbits(0.0), fbits(0.0), 0, 0, 0, fbits(0.0), fbits(0.0), fbits(0.0), left, right, 0]
            lines.append('F ' + ' '.join(f'{v:x}' for v in [fbits(1 / 30)] + m))
            if r.random() < 0.6:
                lines.append(f'D {fbits(1 / 30):x}')
        elif roll < 0.65:
            lines.append(f'T {r.choice([0, 1, 2, 3])}')
        elif roll < 0.7:
            lines.append(f'Y {r.choice([0, 0, 1, 2])}')
        elif roll < 0.75:
            lines.append(f'K {r.randrange(1, 9):x} {r.randrange(2):x}')
        else:
            lines.append(f'{r.choice("pfefl")} {r.randrange(-1, n)}')
    lines.append('X')
    return lines


def run_case(seed, verbose):
    cfg = tp.config(seed)
    fcfg = frame_config(seed)
    plan = make_plan(seed, cfg, fcfg)
    orig = Original(seed, cfg, fcfg, plan)
    plan = [ln.replace('MARKER', f'{orig.marker:x}') for ln in plan]
    vi = iter(orig.ents)
    plan = [('V ' + f'{next(vi):x} ' + ' '.join(ln.split()[2:])) if ln.startswith('V ') else ln for ln in plan]
    # The probe takes the target contact by its fields.
    def contact(k):
        if not k:
            return '0 0'
        q = orig.contacts[k - 1]
        return f'{orig.e.r32(q):x} {orig.e.r32(q + 8):x}'
    plan = [ln.replace('TARGET', '0 0') for ln in plan]
    pplan = [f'T {contact(int(ln.split()[1]))}' if ln.startswith('T ') else ln for ln in plan]
    vi2 = iter(orig.vehicles)
    pplan = [ln + f' {next(vi2):x}' if ln.startswith('V ') else ln for ln in pplan]
    pplan.insert(0, f'Q {orig.e.r32(0x73fba0):x}')
    orig.plan = plan
    dump_o = orig.run()
    rep = Replay(orig.t, orig.e, orig.snapshot)
    probe.probe_app_set_callback(ctypes.cast(rep.callback(), ctypes.c_void_p))
    buf = ctypes.create_string_buffer(1 << 20)
    n = probe.probe_pocket_panel_run('\n'.join(pplan).encode(), buf, len(buf))
    assert n >= 0, 'probe failed'
    dump_r = buf.value.decode('latin1')
    stubbed = orig.w.stub_textures
    to, do = canonical(orig.t.log, dump_o, stubbed)
    tr, dr = canonical(rep.log, dump_r, stubbed)
    ok = compare(f'frame seed {seed}', to, tr)
    if do != dr:
        ok = False
        for a, b in zip(do.split('\n'), dr.split('\n')):
            if a != b:
                print(f'frame seed {seed}: dumps differ')
                ta, tb = a.split(' '), b.split(' ')
                print('  original:', ' '.join(x for x in ta if x not in tb)[:400])
                print('  rebuild: ', ' '.join(x for x in tb if x not in ta)[:400])
                print('  in:', a[:100])
                break
        else:
            print(f'frame seed {seed}: dumps differ in length')
    if verbose and ok:
        print(f'seed {seed}: {len(to)} calls')
    return ok, len(to)


def main():
    args = [x for x in sys.argv[1:] if not x.startswith('--')]
    runs = int(args[0]) if args else 200
    verbose = '--verbose' in sys.argv
    passed = calls = 0
    for seed in range(runs):
        ok, n = run_case(seed, verbose)
        passed += ok
        calls += n
    never = [COVER[a] for a in COVER if a not in REACHED]
    print(f'pocket frame: {passed}/{runs} runs identical ({calls} calls)'
          + (f'; branches never reached: {never}' if never else f'; {len(COVER)} branches reached'))
    return 0 if passed == runs and not never else 1


if __name__ == '__main__':
    sys.exit(main())
