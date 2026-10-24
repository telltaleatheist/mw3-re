"""The waynets at run time against the original x86 (docs/vehicles.md, The
nav waynet; mech3/mech_waynet): the nav waynet's frame 0x479c50 (the watched
points and the selected one within 25 m reached, autonext), the watched list
0x47a270 on it, the HUD's edge arrow 0x479ef0 / 0x479dd0 and its draw
0x479ed0, the icons counted in and out and freed (0x4798f0, 0x479a30,
0x479850, with the leak report), and the nav waynet's make 0x4797c0.

    cmake --build build && .venv/bin/python tests/differential/test_nav_hud.py [runs]

The world is test_veh_waynet.py's (the original's waynets laid out as the
port's), the nav waynet's points given random flags (MFB, autonext, reached,
a nav line). Stubbed: zTwoD's image calls on the arrow at 0x608ae0 (its
flags at 0x608aec and image at 0x608b1c kept in memory as the original
would; the probe keeps its own), the screen's size, the projection 0x53cf10 (x = in.x / 2 + 100,
y = in.z / 2 + 50 and the answer's off-screen bits), the icons' zTwoD and
zImage calls, the leak report, the builder 0x4bcba0 (its +0x34 the nav
waynet or none) and the id. Compared: the traces, the answers, the nav's
reached flags, selection, watched list and the count. The original's path
(no `bugs`).
"""
import ctypes
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import CALLBACK, Replay, compare, load_probe, rnd  # noqa: E402
import test_veh_waynet as tw  # noqa: E402

U, F = ctypes.c_uint32, ctypes.c_float
COVER = {}
ARROWS = ['hud_navarrow_left', 'hud_navarrow_right', 'hud_navarrow_up', 'hud_navarrow_down', 'hud_mfbarrow_left',
          'hud_mfbarrow_right', 'hud_mfbarrow_up', 'hud_mfbarrow_down']


def cover(label, cond=True):
    COVER.setdefault(label, 0)
    if cond:
        COVER[label] += 1


class World(tw.World):
    def __init__(self, seed):
        super().__init__(seed)
        e, t, r = self.e, self.t, self.r
        self.images = {}
        for name in ARROWS:
            img = e.world_alloc(0x40)
            e.cpu.write(img + 4, struct.pack('<hh', r.choice([16, 24, 32]), r.choice([16, 20])))
            self.images[name] = img
        t.script['0x005179d0'] = lambda n, a: self.images.get(a[0], 0)
        t.script['0x0040d760'] = r.choice([640, 800])
        t.script['0x0040d770'] = r.choice([480, 600])
        t.script['0x0053cf10'] = lambda n, a: rnd(seed, f'proj{n}').choice([0, 0, 1, 2, 4, 8, 3, 12])
        t.script['0x004bcba0'] = lambda n, a: rnd(seed, f'built{n}').choice([0, 1, 1])
        t.script['0x0058b1c0'] = 10077

        def hide(em, key, a, pop):
            em.t.handle(key, a)
            em.world_w32(0x608aec, em.r32(0x608aec) | 0x10)
            em._ret(0, pop)

        def show(em, key, a, pop):
            em.t.handle(key, a)
            em.world_w32(0x608aec, em.r32(0x608aec) & ~0x10)
            em._ret(0, pop)

        def set_image(em, key, a, pop):
            em.t.handle(key, a)
            em.world_w32(0x608b1c, a[1])
            em._ret(a[1], pop)
        e.stub(0x59ff50, spec='c', fn=hide)
        e.stub(0x59ff10, spec='c,s0', fn=show)
        e.stub(0x59bb50, spec='c,s0', fn=set_image)
        e.stub(0x59bab0, spec='c,s0,s1')
        e.stub(0x59bb70, spec='c')
        e.stub(0x40d760)
        e.stub(0x40d770)

        def project(em, key, a, pop):
            from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_EDX
            src, dst = em.uc.reg_read(UC_X86_REG_ECX), em.uc.reg_read(UC_X86_REG_EDX)
            x, y, z = struct.unpack('<3f', bytes(em.cpu.read(src, 12)))
            code = em.t.handle(key, [tw.fb(x), tw.fb(y), tw.fb(z)])
            em.cpu.write(dst, struct.pack('<3f', tw.f32(tw.f32(x * 0.5) + 100.0), tw.f32(tw.f32(z * 0.5) + 50.0), 0.0))
            em._ret(code, pop)
        e.stub(0x53cf10, fn=project)
        e.stub(0x516f50, spec='zc')

        def icon(em, key, a, pop):
            em.t.handle(key, [em.cstr(a[1])])
            obj = a[0]
            em.w32(obj, self.icon_vt)
            em.t.override(key, obj)
            em._ret(obj, pop)
        self.icon_vt = e.world_alloc(0x10)
        e.w32(self.icon_vt, e.new_stub('icon.vt+0x0', 'c,s0', 4))
        e.stub(0x59b830, spec='c,s0', fn=icon)
        e.stub(0x5179d0, spec='zc')
        e.stub(0x517b10, spec='c')
        e.stub(0x49bfd0, spec='s0,s4', pop=0)

        def build(em, key, a, pop):
            k = em.t.handle(key, [a[1], em.cstr(a[2])])
            em.w32(a[0] + 0x34, self.nav if k and self.nav else 0)
            em._ret(a[0], pop)
        e.stub(0x4bcba0, spec='c,s0,s1', fn=build)
        e.stub(0x4bd020, key='@builder~', spec='c')
        e.stub(0x58b1c0)


def run(probe, seed):
    w = World(seed)
    r, e, t = w.r, w.e, w.t
    n, nn, labs, flat, cur, nav_cur, state, mission, marker, labels = w.randomize()
    if not w.nav:
        return True
    # The nav's points: random flags; the arrow's state; the count.
    npts = w.points[n:]
    flags = []
    for i, q in enumerate(npts):
        f = [int(r.random() < 0.3), int(r.random() < 0.5), int(r.random() < 0.3),
             r.choice([0, e.world_alloc(0x10)])]
        e.w8(q + 0xc, f[0])
        e.w8(q + 0xe, f[1])
        e.w8(q + 0x34, f[2])
        e.w32(q + 0x14, f[3])
        flags.append(f)
    arrow = [r.choice([0, w.images[ARROWS[1]], w.images[ARROWS[3]]]), r.choice([0, 0x10])]
    e.w32(0x608aec, arrow[1])
    e.w32(0x608b1c, arrow[0])
    count = r.choice([0, 0, 1, 2])
    e.w32(0x608ba0, count)
    for a in range(0x608ba8, 0x608bd0, 4):
        e.w32(a, 0)
    rep = Replay(t, e, e.snapshot())
    probe.probe_app_set_callback(rep.callback())
    probe.probe_wn_reset_icons()  # before the set-up: the free forgets the marker
    probe.probe_wn_setup(n, nn, labs, (F * max(1, len(flat)))(*flat), cur, nav_cur, (ctypes.c_int32 * 6)(*state),
                         mission, marker, 0)
    probe.probe_wn_count(count)
    probe.probe_wn_arrow(arrow[0], arrow[1])
    for i, f in enumerate(flags):
        probe.probe_wn_flags(n + i, *f)
    seq = [r.choice([0, 0, 0, 1, 1, 2, 2, 2, 3, 4, 5, 6, 7]) for _ in range(r.choice([2, 4, 8]))]
    name = f'nav seed {seed} (points {n}/{nn}, count {count}) {seq}'
    def selected_point(wn):
        cnt = (e.r32(wn + 0x10) - e.r32(wn + 0xc)) // 4
        i = e.r32(wn + 0x20)
        return i < cnt and e.r32(e.r32(wn + 0xc) + 4 * i) in w.points

    for k, op in enumerate(seq):
        want = got = 0
        a = b = 0
        nm = 0
        # A selected element with no place is read through null by the
        # original (FIX (bug)); the port's guard is checked on its own.
        if op == 0 and not selected_point(w.nav):
            continue
        if op == 2:
            a = r.choice([0, 1])
            if not selected_point(w.nav if a else w.mcb):
                continue
        if op == 0:  # the mech somewhere, maybe on a point
            if npts and r.random() < 0.6:
                q = r.choice(npts)
                p = [e.f32(q + 0x1c + 4 * j) + r.uniform(-30, 30) for j in range(3)]
            else:
                p = [r.uniform(-300, 300) for _ in range(3)]
            for j in range(3):
                e.world_w32(w.mech_pos + 4 * j, tw.fb(tw.f32(p[j])))
            want = e.call(0x479c50, ecx=w.nav, stack=(tw.fb(1 / 30), w.mech)) & 0xff
        elif op == 1:
            a, b = r.randrange(-1, len(w.points) + 1), r.choice([0, 1, 1])
            pt = w.points[a] if 0 <= a < len(w.points) else 0
            e.call(0x47a270, ecx=w.nav, stack=(pt, b))
        elif op == 2:
            b = r.choice([0, 1, 1])
            e.call(0x479ef0, ecx=w.nav if a else w.mcb, stack=(b, tw.fb(1 / 30)))
        elif op == 3:
            e.call(0x479ed0)
        elif op == 4:
            e.call(0x4798f0, ecx=w.nav)
        elif op == 5:
            e.call(0x479a30, ecx=w.nav)
        elif op == 6:
            e.call(0x479850)
        elif op == 7:
            nm = 'waynet_' + str(r.randrange(1, 4))
            want = int(e.call(0x4797c0, ecx=e.world_string(nm)) == w.nav)
        got = probe.probe_wn_nav(op, a, b, w.mech, nm.encode() if nm else None) & 0xff
        if want != got:
            print(f'{name}: op {k} ({op}) answered original {want} rebuild {got}')
            return False
        cover(f'op {op}')
    if not compare(name, t.log, rep.log):
        return False
    ids = {x: i for i, x in enumerate(w.points)}
    want = []
    cnt = (e.r32(w.nav + 0x10) - e.r32(w.nav + 0xc)) // 4
    for j in range(8):
        q = e.r32(e.r32(w.nav + 0xc) + 4 * j) if j < cnt else 0
        want.append(e.r8(q + 0x34) if q in ids else 0xffffffff)
    want.append(e.r32(w.nav + 0x20))
    sent = e.r32(w.nav + 0x28)
    vals, nd = [], e.r32(sent)
    while nd != sent:
        v = e.r32(nd + 8)
        vals.append(ids.get(v, 999 if v == tw.DEF else 998))
        nd = e.r32(nd)
    want += (vals + [0xffffffff] * 4)[:4]
    want.append(e.r32(0x608ba0))
    out = (U * 32)()
    k = probe.probe_wn_nav_image(out)
    got = list(out)[:k]
    if want != got:
        print(f'{name}: image original {want}\n  rebuild {got}')
        return False
    keys = [kk for kk, _ in t.log]
    cover('a line said', '0x0056e3d0' in keys)
    cover('autonext', any(kk == '0x004fc0b0' for kk in keys) and 0 in seq)
    codes = [t.answers.get(('0x0053cf10', j), 0) for j in range(keys.count('0x0053cf10'))]
    first = [next((b for b in (1, 2, 4, 8) if c & b), 0) for c in codes]  # the edge the arrow goes to
    for bit, label in ((1, 'left'), (2, 'right'), (4, 'up'), (8, 'down'), (0, 'on the screen')):
        cover(f'arrow {label}', bit in first)
    cover('arrow drawn', '0x0059bb70' in keys)
    cover('icons made', '0x0059b830' in keys)
    cover('icons freed', 'icon.vt+0x0' in keys)
    cover('leak reported', '0x0049bfd0' in keys)
    cover('built', 7 in seq and any(kk == '0x004bcba0' for kk in keys))
    return True


def main():
    probe = load_probe()
    probe.probe_app_set_callback.argtypes = [CALLBACK]
    P = ctypes.c_void_p
    probe.probe_wn_setup.argtypes = [ctypes.c_int32, ctypes.c_int32, ctypes.c_char_p, P, ctypes.c_int32,
                                     ctypes.c_int32, P, ctypes.c_int32, U, ctypes.c_int32]
    probe.probe_wn_flags.argtypes = [ctypes.c_int32, ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8, U]
    probe.probe_wn_count.argtypes = [ctypes.c_int32]
    probe.probe_wn_arrow.argtypes = [U, ctypes.c_int32]
    probe.probe_wn_nav.argtypes = [ctypes.c_int32, ctypes.c_int32, ctypes.c_int32, U, ctypes.c_char_p]
    probe.probe_wn_nav.restype = U
    probe.probe_wn_nav_image.argtypes = [P]
    probe.probe_wn_hooks(1)
    probe.probe_wn_fix_check.restype = ctypes.c_int
    args = [a for a in sys.argv[1:] if not a.startswith('-')]
    runs = int(args[0]) if args else 600
    if probe.probe_wn_fix_check() != 1:
        print('the port\'s guard: a selected element with no place')
        os._exit(1)
    for seed in range(runs):
        if not run(probe, seed):
            sys.stdout.flush()
            os._exit(1)
    missed = sorted(k for k, v in COVER.items() if not v)
    if missed:
        print(f'not reached: {missed}')
        sys.stdout.flush()
        os._exit(1)
    print(f'ok: {runs} nav runs as the original ({len(COVER)} branches reached)')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()
