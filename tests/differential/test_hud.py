"""The HUD's construction against the original (docs/hud.md).

The original's constructor 0x43b190 runs in the emulator on a fresh
0x22e40-byte block (zTwoD's outside stubbed by twod_world.py, the pieces not
ported yet stubbed and recorded), then the rebuild's (probe_hud.cpp) with the
original's answers replayed. Compared: the call traces and a dump of the HUD,
its pieces and their zTwoD objects (the same fields as test_twod.py's dumps),
and the damage silhouettes every HUD shares.

    cmake --build build && .venv/bin/python -P -u tests/differential/test_hud.py [seeds]
"""
import ctypes
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import Replay, Trace, compare, load_probe, rnd  # noqa: E402
from twod_world import TwodWorld  # noqa: E402
from ai_world import FLOOR, IAT_FLOOR  # noqa: E402
import test_twod  # noqa: E402

probe = load_probe()
probe.probe_hud_build.argtypes = [ctypes.c_uint32, ctypes.c_uint32, ctypes.c_char_p, ctypes.c_int]

HUD_SIZE = 0x22e40
PIECES = {0x14: 'radar', 0xdd4: 'compass', 0x144c: 'reticle', 0x1da8: 'gauges', 0x3434: 'mfd', 0x410c: 'target',
          0x6520: 'weapons', 0x17964: 'objectives', 0x18410: 'timer', 0x188c0: 'equipment'}
# The pieces not ported yet: constructor -> (spec of its arguments).
STUBBED = {0x43d260: 's0', 0x443b60: 'zs0,zs1', 0x444760: '', 0x442060: 's0,s1,s2,s3,s4',
           0x43fcb0: 's0,s1,s2,s3'}
DAMAGE = 0x605888


class Original:
    def __init__(self, seed):
        self.t = Trace(seed=seed)
        self.w = TwodWorld(self.t, seed)
        self.e = self.w.emu
        self.seed = seed

    def run(self):
        e, w = self.e, self.w
        for addr, spec in STUBBED.items():
            e.stub(addr, spec=spec)
        e.stub(0x43bb80, spec='c,s0')
        r = rnd(self.seed, 'world')
        entry = e.world_alloc(0x100)
        e.cpu.write(entry + 0xd0, struct.pack('<f', r.choice([0.0, 1.0, -1.0])))
        w.answers['0x00491c80'] = lambda rr, n, a: entry if self.flip else 0
        self.flip = r.random() < 0.5
        w.answers['@entry+0xd0'] = struct.unpack('<I', bytes(e.cpu.read(entry + 0xd0, 4)))[0]
        self.e.stub(0x491c80, spec='')
        e.stub(0x446ad0, spec='c')
        e.stub(0x446520, spec='c')
        e.cpu.import_code(IAT_FLOOR, FLOOR)
        e.cpu.write(0x72f348, struct.pack('<I', r.choice([0, 1])))  # the render path: software, Direct3D
        self.hud = e.world_alloc(HUD_SIZE)
        self.mech = e.world_alloc(0x10) if r.random() < 0.8 else 0
        self.snapshot = e.snapshot()
        e.call(0x43b190, ecx=self.hud, stack=(self.mech,))
        return self.dump()

    def dump(self):
        e, h = self.e, self.hud
        out, line = [], []

        def kv(k, v):
            line.append(f'{k}={v & 0xffffffff:x}')

        def tx(k, v):
            line.append(f'{k}=T{v:x}')

        def s(k, a):
            line.append(f'{k}="' + e.cstr(a).replace(' ', '_') + '"')

        def rc(k, a):
            vals = [struct.unpack('<i', struct.pack('<I', e.r32(a + 4 * i)))[0] for i in range(4)]
            line.append(f'{k}=' + ','.join(str(v) for v in vals))

        def piece(a):
            return PIECES.get(a - h, '?') if a else '0'

        def flush(head):
            out.append(head + ' ' + ' '.join(line) + ' ')
            line.clear()

        def base(o):
            line.append(f'next={"o-1" if e.r32(o + 4) else "o-1"}')
            kv('flags', e.r32(o + 0xc))
            kv('timer', e.r32(o + 0x10))
            kv('x', e.r32(o + 0x14))
            kv('y', e.r32(o + 0x18))
            tx('bg', e.r32(o + 0x1c))
            rc('bgr', o + 0x20)
            kv('mouse', e.r32(o + 0x30) & 0xffff)

        def image(o):
            base(o)
            kv('owned', e.r32(o + 0x34))
            kv('queued', e.r32(o + 0x38))
            tx('img', e.r32(o + 0x3c))
            kv('u40', e.r32(o + 0x40) & 0xffff)
            src = e.r32(o + 0x44)
            if src:
                rc('src', src)
            else:
                line.append('src=0')
            kv('center', e.r32(o + 0x48))

        def fonttext(o):
            base(o)
            s('text', o + 0x34)
            kv('align', e.r32(o + 0x244))
            tx('timg', e.r32(o + 0x248))
            for k, off in (('top', 0x24c), ('bottom', 0x250), ('hfont', 0x258)):
                kv(k, e.r32(o + off))
            s('rendered', o + 0x260)
            for k, off in (('w', 0x460), ('h', 0x464), ('shadow', 0x468), ('bk', 0x46c), ('nr', 0x474)):
                kv(k, e.r32(o + off))
            rc('tr', o + 0x490)
            kv('maxw', e.r32(o + 0x4a8))
            kv('named', e.r32(o + 0x4ac))

        def text(o):
            fonttext(o)
            for k, off in (('font', 0x234), ('centered', 0x238), ('shcol', 0x254), ('lead', 0x478), ('fixed', 0x47c),
                           ('sdx', 0x4a0), ('sdy', 0x4a4), ('ft', 0x4b0), ('frate', 0x4b4), ('atop', 0x4b8)):
                kv(k, e.r32(o + off))
            if e.r32(o + 0x46c) == 2:
                kv('bkc', e.r32(o + 0x470))
            rc('fr', o + 0x480)
            kv('fon', e.r8(o + 0x4c0))
            kv('fmode', e.r32(o + 0x4c4))
            kv('phase', e.r32(o + 0x4c8))

        def textbox(o, head):
            fonttext(o)
            kv('current', e.r32(o + 0x4b0))
            kv('bup', e.r8(o + 0x4b4))
            kv('repos', e.r8(o + 0x4b5))
            first, last = e.r32(o + 0x4bc), e.r32(o + 0x4c0)
            n = (last - first) // 0x4cc if first else 0
            kv('nlines', n)
            flush(head)
            for i in range(n):
                text(first + 0x4cc * i)
                flush(f'{head}.line{i}')

        def panel(o):
            image(o)
            rc('rect', o + 0xbc)
            for i in range(12):
                kv('c', e.r32(o + 0xcc + 4 * i))
            kv('color', e.r32(o + 0xfc) & 0xffff)
            kv('alpha', e.r8(o + 0xfe))
            kv('colors', e.r32(o + 0x100))
            kv('alphas', e.r32(o + 0x104))

        kv('active', e.r32(h + 4))
        line.append(f'head={piece(e.r32(h + 8))} tail={piece(e.r32(h + 0xc))}')
        kv('mech', e.r32(h + 0x10))
        for i in range(12):
            kv('z', e.r32(h + 0x22e08 + 4 * i))
        out.append(' '.join(line) + ' ')
        line.clear()
        for off in (0xdd4, 0x1da8, 0x410c, 0x3434, 0x6520, 0x188c0):
            o = h + off
            out.append(f'link {PIECES[off]} next={piece(e.r32(o + 4))} owner={1 if e.r32(o + 8) == h else 0}')
        c = h + 0xdd4
        base(c)
        kv('scale', e.r32(c + 0x34))
        kv('half', e.r32(c + 0x38))
        kv('centre', e.r32(c + 0x3c))
        rc('srcr', c + 0x40)
        flush('compass')
        image(c + 0x50)
        flush('tape')
        image(c + 0x10c)
        flush('frame')
        fonttext(c + 0x1c8)
        flush('ctext')
        fonttext(h + 0x18410)
        flush('timer')
        o = h + 0x17964
        textbox(o, 'objectives')
        panel(o + 0x4c8)
        flush('opanel')
        textbox(o + 0x5d4, 'scores')
        for off in (0xa9c, 0xaa0, 0xaa4, 0xaa8):
            kv('font', e.r32(o + off))
        flush('ofonts')
        o = h + 0x188c0
        base(o)
        kv('hud', e.r32(o + 0x34))
        kv('below', e.r8(o + 0xa540))
        kv('column', e.r32(o + 0xa544))
        flush('equipment')
        panel(o + 0xa438)
        flush('epanel')
        for i in range(16):
            q = o + 0x38 + 0xa40 * i
            text(q)
            flush(f'eline{i}')
            fonttext(q + 0x4cc)
            flush('value')
            image(q + 0x97c)
            kv('state', e.r32(q + 0xa38))
            kv('item', e.r32(q + 0xa3c))
            flush('light')
        o = h + 0x6520
        base(o)
        kv('hud', e.r32(o + 0x34))
        kv('x38', e.r32(o + 0x38))
        tx('select', e.r32(o + 0x128))
        tx('chain', e.r32(o + 0x12c))
        kv('shown', e.r8(o + 0x11438))
        kv('x1143c', e.r32(o + 0x1143c))
        kv('column', e.r32(o + 0x11440))
        flush('weapons')
        base(o + 0x3c)
        for i in range(42):
            kv('p', e.r32(o + 0x3c + 0x34 + 4 * i))
        for k, off in (('count', 0xdc), ('color', 0xe0), ('dl', 0xe4)):
            kv(k, e.r32(o + 0x3c + off))
        kv('tf', e.r8(o + 0x3c + 0xe8))
        flush('wpoly')
        panel(o + 0x11330)
        flush('wpanel')
        for i in range(16):
            q = o + 0x130 + 0x1120 * i
            image(q)
            for k, off in (('bx', 0xbc), ('by', 0xc0), ('en', 0xc4), ('value', 0x14c)):
                kv(k, e.r32(q + off))
            rc('bounds', q + 0xcc)
            tx('fill', e.r32(q + 0x150))
            tx('empty', e.r32(q + 0x16c))
            kv('count', e.r32(q + 0x1110))
            kv('red', e.r8(q + 0x1118))
            kv('changed', e.r8(q + 0x111a))
            kv('x111b', e.r8(q + 0x111b))
            kv('weapon', e.r32(q + 0x111c))
            flush(f'wline{i}')
            for nm, off in (('name', 0x188), ('text2', 0x638), ('ctext', 0xae8)):
                fonttext(q + off)
                flush(nm)
            for nm, off in (('light', 0xf98), ('mark', 0x1054)):
                image(q + off)
                flush(nm)
        for i in range(3):
            tx('light', e.r32(0x6059b0 + 4 * i))
        for i in range(4):
            kv('font', e.r32(0x6059c0 + 4 * i))
        flush('wset')
        for i in range(3):
            tx('light', e.r32(0x6059d0 + 4 * i))
        for i in range(4):
            kv('font', e.r32(0x6059e0 + 4 * i))
        flush('eset')
        kv('loaded', e.r8(0x605990))
        kv('bgw', e.r32(0x605998))
        kv('bgh', e.r32(0x60599c))
        for i in range(3):
            tx('bg', e.r32(0x6059a0 + 4 * i))
        for p in range(8):
            for i in range(5):
                tx('i', e.r32(DAMAGE + 0x20 * p + 4 * i))
            kv('px', e.r32(DAMAGE + 0x20 * p + 0x14))
            kv('py', e.r32(DAMAGE + 0x20 * p + 0x18))
        out.append(' '.join(line) + ' ')
        return '\n'.join(out) + '\n'


def strip_next(dump):
    """The objects' own next fields are links inside groups (test_twod refs); the
    pieces' links are compared by the 'link' lines."""
    import re
    return re.sub(r'next=o-?\d+ ', '', dump)


def run_case(seed, verbose=False):
    orig = Original(seed)
    dump_o = orig.run()
    rep = Replay(orig.t, orig.e, orig.snapshot)
    probe.probe_app_set_callback(ctypes.cast(rep.callback(), ctypes.c_void_p))
    buf = ctypes.create_string_buffer(1 << 20)
    n = probe.probe_hud_build(orig.hud, orig.mech, buf, len(buf))
    assert n >= 0, 'probe failed'
    dump_r = buf.value.decode('latin1')
    stubbed = orig.w.stub_textures
    to, do = test_twod.canonical(orig.t.log, strip_next(dump_o), stubbed)
    tr, dr = test_twod.canonical(rep.log, strip_next(dump_r), stubbed)
    ok = compare(f'seed {seed}', to, tr)
    if ok and do != dr:
        ok = False
        for a, b in zip(do.split('\n'), dr.split('\n')):
            if a != b:
                ta, tb = a.split(' '), b.split(' ')
                print(f'seed {seed}: dumps differ in "{a[:40]}"')
                print('  original:', ' '.join(x for x in ta if x not in tb)[:400])
                print('  rebuild: ', ' '.join(x for x in tb if x not in ta)[:400])
                break
    return ok, len(to)


def main():
    seeds = int(sys.argv[1]) if len(sys.argv) > 1 else 20
    calls = 0
    for seed in range(seeds):
        ok, n = run_case(seed)
        if not ok:
            sys.stdout.flush()
            os._exit(1)
        calls += n
    print(f'{seeds} HUDs built, {calls} calls: the rebuilt HUD matches the original')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    try:
        main()
    except BaseException as ex:  # an emulator fault: print it, then leave without Unicorn's teardown
        import traceback
        traceback.print_exc()
        sys.stdout.flush()
        os._exit(1)
