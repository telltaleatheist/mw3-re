"""The main menu screen against the original x86 (docs/mainmenu.md, Testing).

    cmake --build build && .venv/bin/python tests/differential/test_mainmenu.py [runs]

The original builds the main menu (0x00486260: dialog.zrd's MAINMENU, its
BACKGROUND, seven rippling buttons, MECH3TEXT and the version line, or the
FastMenu still screen), then runs a plan: frames of mouse input over the
buttons through the screen's update (0x005a2bb0, every button's ripple and
line, the scrolling background and title), presses (each button's +0x30:
the menus' App::Push, QUIT's question), the busy flag (+0x18), bake and
unbake (+0x90), bounds, enable, dirty-all, restore and unload; then the
deleting destructor (0x00485a60). Even runs use the shipped dialog, odd runs
a synthetic one (RADIUS and MAXRADIUS set, buttons and primitives missing,
moved, no line; one in eight without MAINMENU at all). FastMenu, the no-voice
flag and the display size (640x480 up to 1600x1200) vary by run. Every call
out is stubbed (twod_world.py plus the options, zVideo's buffers, the
ripple 0x00558dd0, the version and message, App::Push and the quit
question); the port (tests/probe_mainmenu.cpp) runs the same plan with the
original's answers. Compared: the call traces and a dump of the screen and
every object, the ripple and scroll state included. The x87 runs at 24 bits.
"""
import ctypes
import random
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import Replay, Trace, compare, load_probe, rnd  # noqa: E402
from original import ROOT  # noqa: E402
from twod_world import READER, TwodWorld, entries, find, parse_node, write_node  # noqa: E402
import test_twod  # noqa: E402
from test_twod import canonical, fbits  # noqa: E402
from test_twod_msgbox import archive  # noqa: E402

probe = load_probe()
probe.probe_app_set_callback.argtypes = [ctypes.c_void_p]
probe.probe_mainmenu_run.argtypes = [ctypes.c_char_p, ctypes.c_uint32, ctypes.c_int, ctypes.c_char_p,
                                     ctypes.c_char_p, ctypes.c_int]

CTOR, ZRDR_LOAD = 0x486260, 0x556740
BUTTON_VTS = [0x5c4ef0, 0x5c4e50, 0x5c4db0, 0x5c4d10, 0x5c4c70, 0x5c4bd0, 0x5c4b30]
BUTTONS = ['INSTANTACTION', 'NEWCAMPAIGN', 'MULTIPLAYER', 'TRAINING', 'OPTIONS', 'CREDITS', 'QUIT']
GLOBALS = [0x60985c, 0x609864, 0x609860, 0x609874, 0x609868, 0x609878]
test_twod.VT_KIND.update({v: 'C' for v in BUTTON_VTS})
test_twod.VT_KIND.update({0x5bc1b0: 'I', 0x5bd988: 'I'})
SIZES = [(640, 480), (640, 480), (800, 600), (1024, 768), (1600, 1200), (320, 240)]
MECH3 = {0x40cec0: '', 0x40d760: '', 0x40d770: '', 0x56f520: 'zc', 0x56f530: 'zc', 0x4ac290: 'c,d',
         0x576db0: '', 0x576da0: '', 0x576d90: '', 0x576d80: '', 0x558290: 'c,d,s0,s1', 0x4af290: '',
         0x576040: 'c', 0x590400: 's0,s1', 0x482a20: '', 0x558dd0: 'c,d,s0,s1,s2,s3,s4,s5'}


def synthetic(seed, body):
    """The shipped MAINMENU changed: ripple keys, moved and missing pieces."""
    r = random.Random(f'menu:{seed}')
    body = [list(x) if isinstance(x, list) else x for x in body]
    out = []
    for i in range(0, len(body) - 1, 2):
        k, v = body[i], body[i + 1]
        if k == 'BUTTONS':
            nb = []
            for j in range(0, len(v) - 1, 2):
                name, entry = v[j], list(v[j + 1])
                if r.random() < 0.12:
                    continue  # missing: its bind finds nothing
                if r.random() < 0.4:
                    entry += ['RADIUS', [r.choice([5.0, 12.5, 20.0, 33.3])]]
                if r.random() < 0.4:
                    entry += ['MAXRADIUS', [r.choice([0, 25, 60, 200, 400])]]
                if r.random() < 0.3:
                    p = entry.index('POSITION')
                    entry[p + 1] = [r.randrange(-30, 640), r.randrange(-30, 480)]
                if r.random() < 0.15 and 'AUXBITMAP' in entry:
                    p = entry.index('AUXBITMAP')
                    del entry[p:p + 2]
                nb += [name, entry]
            v = nb
        elif k == 'PRIMITIVES':
            nb = []
            for j in range(0, len(v) - 1, 2):
                if r.random() < 0.2 and v[j] != 'MECH3TEXT':
                    continue
                entry = v[j + 1]
                if v[j] == 'MECH3TEXT' and r.random() < 0.85:  # near the left edge: it wraps within the run
                    entry = ['BITMAP', ['main_mech3'], 'POSITION', [r.randrange(0, 60), 240]]
                nb += [v[j], entry]
            v = nb
        out += [k, v]
    name = 'MAINMENU' if seed % 8 != 7 else 'NOTMAINMENU'
    return [['SHARED_IMAGE_PATH', ['common'], name, out]]


def make_plan(seed, positions, n_buttons=7):
    r = random.Random(f'plan:{seed}')
    lines = ['N']
    if r.random() < 0.8:
        lines.append('U')  # the Main state unloads the tree at once (0x0059fa10)
    spots = list(positions) or [(100, 100)]
    down = 0
    for _ in range(r.randrange(30, 90)):
        roll = r.random()
        if roll < 0.05:
            lines.append(f'P {r.randrange(n_buttons)}')
        elif roll < 0.08:
            lines.append(f'B {r.randrange(2)}')
        elif roll < 0.11:
            lines.append(f'K {r.randrange(n_buttons)} {r.randrange(2)}')
        elif roll < 0.15:
            lines.append(f'R {r.randrange(n_buttons)}')
        elif roll < 0.17:
            lines.append(f'E {r.randrange(n_buttons)} {r.randrange(2)}')
        elif roll < 0.19:
            lines.append(f'A {r.randrange(2)}')
        elif roll < 0.21:
            lines.append(r.choice('DH'))
        sx, sy = r.choice(spots)
        if r.random() < 0.85:
            x, y = sx + r.randrange(-10, 70), sy + r.randrange(-10, 70)
        else:
            x, y = r.randrange(-20, 660), r.randrange(-20, 500)
        if down == 0:
            left = 1 if r.random() < 0.25 else 0
        elif down in (1, 2):
            left = 2 if r.random() < 0.5 else 4
        else:
            left = 0
        down = left
        right = r.choice([0, 0, 0, 1, 4])
        dt = r.choice([0.0, 1 / 60, 1 / 30, 0.05, 0.2])
        m = [x & 0xffffffff, y & 0xffffffff, 0, fbits(0.0), fbits(0.0), fbits(0.0), 0, 0, 0,
             fbits(0.0), fbits(0.0), fbits(0.0), left, right, 0]
        lines.append('F ' + ' '.join(f'{v:x}' for v in [fbits(dt)] + m))
    lines.append('X')
    return lines


class Original:
    def __init__(self, seed, data, start, plan, cfg):
        self.t = Trace(seed=seed)
        w = self.w = TwodWorld(self.t, seed)
        e = self.e = w.emu
        self.plan, self.cfg = plan, cfg
        tree = w.read_tree(data, start)
        e.stub(ZRDR_LOAD, '@zrdr_load', 'c,d', None, lambda emu, key, a, pop: emu._ret(tree, pop))
        for addr, spec in MECH3.items():
            e.stub(addr, f'{addr:#010x}', spec)
        if cfg['small']:  # the background and the title small enough to wrap within the run
            base_image = w.answers['0x005179d0']

            def image(rr, k, a):
                if a[0] in ('common_background', 'main_mech3'):
                    hv = rnd(seed, a[0]).randrange(1 << 16)
                    return w.fake_texture(20 + hv % 40, 12 + (hv >> 6) % 30)
                return base_image(rr, k, a)
            w.answers['0x005179d0'] = image
        fast = cfg['fast']
        w.answers.update({
            '0x0040cec0': (lambda rr, k, a: rr.randrange(2)) if fast == 'random' else fast,
            '0x0040d760': cfg['size'][0], '0x0040d770': cfg['size'][1],
            '0x004af290': e.world_string(cfg['version']), '0x00576040': e.world_string(cfg['title']),
            '0x00590400': 0, '0x00482a20': 0, '0x00558dd0': 0, '0x00558290': 0,
        })
        for i, a in enumerate((0x576db0, 0x576da0, 0x576d90, 0x576d80)):
            w.answers[f'{a:#010x}'] = 0x5e000000 + i
        for i, g in enumerate(GLOBALS):
            e.w32(g, 0x51000000 + i)
        e.w8(0x608e4b, cfg['no_voices'])

    def vcall(self, obj, slot, stack=()):
        return self.e.call(self.e.r32(self.e.r32(obj) + slot), ecx=obj, stack=stack)

    def run(self):
        e, w, t = self.e, self.w, self.t
        self.snapshot = e.snapshot()
        m = self.d = e.alloc(0x12c1c)
        dump = ''
        for line in self.plan:
            p = line.split()
            op = p[0]
            b = (lambda i: m + 0x111ec + 0x2a8 * int(i))
            if op == 'N':
                e.call(CTOR, ecx=m, stack=(self.cfg['state'],))
            elif op == 'F':
                vals = [int(v, 16) for v in p[1:]]
                w.mouse_frames.append(vals[1:])
                self.vcall(m, 0x00, (vals[0],))
                w.frame += 1
            elif op == 'A':
                self.vcall(m, 0x04, (int(p[1]),))
            elif op == 'P':
                self.vcall(b(p[1]), 0x30)
            elif op == 'B':
                old = self.vcall(m, 0x18, (int(p[1]),)) & 0xff
                t.handle('busy', (old, self.vcall(m, 0x1c) & 0xff))
            elif op == 'D':
                e.call(0x59fa60, ecx=m)
            elif op == 'H':
                e.call(0x59fa40, ecx=m)
            elif op == 'U':
                e.call(0x59fa10, ecx=m)
            elif op == 'K':
                self.vcall(b(p[1]), 0x90, (int(p[2]),))
            elif op == 'R':
                r = self.vcall(b(p[1]), 0x2c)
                t.handle('rect', tuple(e.r32(r + 4 * i) for i in range(4)) if r else ())
            elif op == 'E':
                e.w32(b(p[1]) + 0xc4, int(p[2]))
                self.vcall(b(p[1]), 0x80)
            elif op == 'X':
                dump = test_twod.Original.dump(self) + self.dump_menu()
                self.vcall(m, 0x08, (1,))
        return dump

    # The section probe_mainmenu.cpp's dump_menu writes.
    def dump_menu(self):
        e, m = self.e, self.d
        objs = []
        p = e.r32(m + 8)
        while p:
            objs.append(p)
            p = e.r32(p + 4)
        out = []

        def kv(k, v):
            out.append(f'{k}={v & 0xffffffff:x} ')

        def tx(k, v):
            out.append(f'{k}=T{v:x} ')

        def rc(k, a):
            vals = [struct.unpack('<i', struct.pack('<I', e.r32(a + 4 * i)))[0] for i in range(4)]
            out.append(f'{k}=' + ','.join(str(v) for v in vals) + ' ')

        def image(o):
            nxt = e.r32(o + 4)
            out.append(f'next=o{objs.index(nxt) if nxt in objs else -1} ')
            kv('flags', e.r32(o + 0xc))
            kv('timer', e.r32(o + 0x10))
            kv('x', e.r32(o + 0x14))
            kv('y', e.r32(o + 0x18))
            tx('bg', e.r32(o + 0x1c))
            rc('bgr', o + 0x20)
            kv('mouse', e.r32(o + 0x30) & 0xffff)
            kv('owned', e.r32(o + 0x34))
            kv('queued', e.r32(o + 0x38))
            tx('img', e.r32(o + 0x3c))
            kv('u40', e.r32(o + 0x40) & 0xffff)
            src = e.r32(o + 0x44)
            if src:
                rc('src', src)
            else:
                out.append('src=0 ')
            kv('center', e.r32(o + 0x48))
        out.append('menu ')
        kv('state', e.r32(m + 0x10fec))
        kv('busy', e.r8(m + 0x10ff0))
        kv('def', 1 if e.r32(m + 0x12600) else 0)
        out.append('\nstill ')
        image(m + 0x10ff4)
        bg = m + 0x110b0
        out.append('\nbackground ')
        image(bg)
        kv('step', e.r32(bg + 0xbc))
        kv('stepy', e.r32(bg + 0xc0))
        for i in range(4):
            kv('pc', e.r8(bg + 0xc4 + i))
            kv('px', e.r32(bg + 0xc8 + 8 * i))
            kv('py', e.r32(bg + 0xcc + 8 * i))
            rc('psrc', bg + 0xf8 + 0x10 * i)
        rc('view', bg + 0xe8)
        kv('moving', e.r8(bg + 0x138))
        out.append('\ntitle ')
        image(m + 0x12484)
        kv('speed', e.r32(m + 0x12484 + 0xbc))
        for i in range(7):
            k = m + 0x111ec + 0x2a8 * i
            out.append(f'\nbutton{i} ')
            kv('reading', e.r8(k + 0x14c))
            kv('id', e.r32(k + 0x150))
            kv('borrowed', e.r8(k + 0x154))
            tx('stashed', e.r32(k + 0x158))
            rc('reach', k + 0x15c)
            rp = k + 0x16c
            kv('rx', e.r32(rp + 0x14))
            kv('ry', e.r32(rp + 0x18))
            kv('rflags', e.r32(rp + 0xc))
            for name, off in (('inner', 0x38), ('outer', 0x3c), ('ramp', 0x40), ('rwl', 0x44), ('rph', 0x48)):
                kv(name, e.r32(rp + off))
            kv('rippling', e.r8(k + 0x1b8))
            for name, off in (('speed', 0x1bc), ('amp', 0x1c0), ('wl', 0x1c4), ('phase', 0x1c8), ('radius', 0x1cc),
                              ('maxr', 0x1d0), ('rr', 0x1d4), ('rsq', 0x1d8)):
                kv(name, e.r32(k + off))
            first, last = e.r32(k + 0x1e0), e.r32(k + 0x1e4)
            kv('nowned', (last - first) // 4 if first else 0)
            out.append('\n  line ')
            image(k + 0x1ec)
        out.append('\n')
        return ''.join(out)


def run_case(seed, path, data, start, plan, cfg, verbose):
    orig = Original(seed, data, start, plan, cfg)
    dump_o = orig.run()
    rep = Replay(orig.t, orig.e, orig.snapshot)
    probe.probe_app_set_callback(ctypes.cast(rep.callback(), ctypes.c_void_p))
    buf = ctypes.create_string_buffer(1 << 22)
    n = probe.probe_mainmenu_run(str(path).encode(), cfg['state'], cfg['no_voices'], '\n'.join(plan).encode(),
                                 buf, len(buf))
    assert n >= 0, 'probe failed'
    dump_r = buf.value.decode('latin1')
    stubbed = orig.w.stub_textures
    to, do = canonical(orig.t.log, dump_o, stubbed)
    tr, dr = canonical(rep.log, dump_r, stubbed)
    if 'U' in plan:  # the fonts' faces point into the freed tree (read only while loading)
        do, dr = (re.sub(r'face=".*?" height=', 'height=', x, flags=re.S) for x in (do, dr))
    ok = compare(f'main menu seed {seed}', to, tr)
    if do != dr:
        ok = False
        for a, b in zip(do.split('\n'), dr.split('\n')):
            if a != b:
                print(f'main menu seed {seed}: dumps differ')
                ta, tb = a.split(' '), b.split(' ')
                print('  original:', ' '.join(x for x in ta if x not in tb)[:400])
                print('  rebuild: ', ' '.join(x for x in tb if x not in ta)[:400])
                print('  in:', a[:100])
                break
        else:
            print(f'main menu seed {seed}: dumps differ in length')
    if verbose and ok:
        print(f'seed {seed}: {len(to)} calls')
    return ok, to


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    runs = int(args[0]) if args else 60
    verbose = '--verbose' in sys.argv
    data = READER.read_bytes()
    start = next(s for n, s, _ in entries(data) if n == 'dialog.zrd')
    tree, _ = parse_node(data, start)
    body = find(tree, 'MAINMENU')
    synth_path = ROOT / 'build' / 'mainmenu_synth.zbd'
    passed = calls = 0
    stats = {'fast': 0, 'presses': 0, 'ripples': 0, 'quits': 0, 'pushes': 0}
    for seed in range(runs):
        r = random.Random(f'cfg:{seed}')
        cfg = {'fast': r.choice([0, 0, 0, 1, 'random']), 'size': r.choice(SIZES), 'no_voices': r.randrange(2),
               'state': 0x608f00, 'small': seed % 2 == 1 or seed % 6 == 0, 'version': r.choice(['1.2', '1.2b']),
               'title': r.choice(['MechWarrior 3', 'MW3'])}
        if seed % 2:
            t = synthetic(seed, body)
            blob = write_node(t)
            synth_path.write_bytes(archive([('dialog.zrd', blob)]))
            here = find(t, 'MAINMENU') or []
            path, file_data, file_start = synth_path, blob, 0
        else:
            here = body
            path, file_data, file_start = READER, data, start
        bl = find(here, 'BUTTONS') or []
        positions = [tuple(find(bl[i + 1], 'POSITION')) for i in range(0, len(bl) - 1, 2)]
        plan = make_plan(seed, positions)
        ok, trace = run_case(seed, path, file_data, file_start, plan, cfg, verbose)
        passed += ok
        calls += len(trace)
        stats['fast'] += cfg['fast'] == 1
        stats['presses'] += sum(1 for line in plan if line.startswith('P'))
        stats['ripples'] += sum(1 for k, _ in trace if k == '0x00558dd0')
        stats['quits'] += sum(1 for k, _ in trace if k == '0x00482a20')
        stats['pushes'] += sum(1 for k, _ in trace if k == '0x00590400')
    print(f'main menu: {passed}/{runs} runs identical ({calls} calls; {stats["ripples"]} ripples drawn, '
          f'{stats["pushes"]} menu pushes, {stats["quits"]} quit questions, {stats["fast"]} FastMenu runs)')
    return 0 if passed == runs else 1


if __name__ == '__main__':
    sys.exit(main())
