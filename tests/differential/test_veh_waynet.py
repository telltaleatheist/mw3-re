"""The waynets against the original x86 (docs/vehicles.md, The mcbwaynet;
mech3/mech_waynet): a point 0x47a320 (its fields, the trailing links, the
nav line for one that is no dock point) and its line 0x47a500; a waynet's
selection (next 0x479b10, prev 0x479b30, nearest 0x479b50 over the AI's
0x4d51d0, select 0x479b90, find 0x479be0, the place 0x479aa0, the marker
0x479ac0, begin 0x479a50, remove_last 0x47a1e0, the list 0x47a270); the
mcbwaynet's next / prev / nearest (0x47a5c0, 0x47a620, 0x47a680) and clear
0x47a6b0; the MFB command 0x47a7a0 (its three presses, with the mission's
waynet 0x491d00 and the nav waynet a multiplayer game would have) and
0x47a730.

    cmake --build build && .venv/bin/python tests/differential/test_veh_waynet.py [runs]

The original's waynets are laid out in memory as the port's (the default
behaviour at 0x604068 as element 0); the dock's calls (0x45a000, 0x45a500,
0x45a540, 0x45b740), the radio 0x4e6d50, positions 0x5a85e0, the mech's
vt+0x50, the "waypoint" node and the world, zSound are stubbed. Compared:
the traces, the answers, each waynet's state and elements, the mission's
waynet, the marker, the point's fields.
"""
import ctypes
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import CALLBACK, Emu, Replay, Trace, compare, load_probe, rnd  # noqa: E402

U, F = ctypes.c_uint32, ctypes.c_float
MCB_VT, NAV_VT, PT_VT, DEF = 0x5c38e0, 0x5c37b0, 0x5c3828, 0x604068
COVER = {}


def cover(label, cond=True):
    COVER.setdefault(label, 0)
    if cond:
        COVER[label] += 1


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


def fb(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def heap_strs(e):
    def strdup(em, key, args, pop):
        s = em.cstr(args[0]).encode('latin1') + b'\0'
        a = em.alloc(len(s))
        em.cpu.write(a, s)
        em._ret(a, 0)
    e.import_stub(0x5bb670, 'strdup', 's0', 0, strdup)
    e.import_stub(0x5bb65c, 'free', 's0', 0)

    def sprintf(em, key, args, pop):
        buf, fmt, x = em.args('s0,s1,s2')
        s = em.cstr(fmt).replace('%s', em.cstr(x)).encode('latin1') + b'\0'
        em.cpu.write(buf, s)
        em._ret(len(s) - 1, 0)
    e.import_stub(0x5bb658, 'sprintf', '', 0, sprintf)

    def stricmp(em, key, args, pop):
        a, b = em.cstr(args[0]).lower(), em.cstr(args[1]).lower()
        em._ret(0 if a == b else (-1 if a < b else 1), 0)
    e.import_stub(0x5bb580, '_stricmp', 's0,s1', 0, stricmp)
    e.t.silent |= {'strdup', 'free', 'sprintf', '_stricmp'}


class World:
    def __init__(self, seed):
        r = self.r = rnd(seed, 'waynet')
        t = self.t = Trace(seed=seed)
        e = self.e = Emu(t)
        e.heap_stubs()
        heap_strs(e)
        e.cpu.fpu_control(0x007f)
        e.cpu.write(DEF, b'\0' * 0x10)
        e.cpu.write(0x603fa0, b'\0' * 0x10)
        e.call(0x4105d0, ecx=0x603fa0)  # the null step (an out-of-range element's answer)
        e.call(0x4c8750, ecx=DEF)       # the default behaviour (element 0)
        for addr, spec in ((0x45a000, 'c'), (0x45a500, 'c,s0'), (0x45a540, 'c'), (0x45b740, 'c,s0'),
                           (0x4e6d50, 'c,s0'), (0x4eb170, 'c,d'), (0x4fc0b0, 'c,s0,s1,s2'), (0x4f7fe0, 'c,zd'),
                           (0x56f6f0, 'zc'), (0x56e3d0, 'c,s0')):
            e.stub(addr, spec=spec)
        e.w32(0x608c84, e.object('world', slots={0: ('s0', 4)}))
        self.mech = e.object('mech', slots={0x50: ('c', 0)})
        self.mech_pos = e.world_alloc(0x10)

        def position(em, key, a, pop):
            em.t.handle(key, a)
            em.t.override(key, self.mech_pos)
            em._ret(self.mech_pos, pop)
        e.stub(0x5a85e0, spec='c', fn=position)

    def waynet(self, vt, size, wid, labels, pos, cap_extra):
        e = self.e
        w = e.world_alloc(size)
        e.cpu.write(w, b'\0' * size)
        e.w32(w, vt)
        e.w32(w + 4, wid)
        pts = []
        for lab, p in zip(labels, pos):
            q = e.world_alloc(0x38)
            e.cpu.write(q, b'\0' * 0x38)
            e.w32(q, PT_VT)
            e.w8(q + 0xc, 1)
            e.w8(q + 0xd, 1)
            for k in range(3):
                e.wf(q + 0x1c + 4 * k, p[k])
            e.w32(q + 0x28, e.world_string('p'))
            e.w32(q + 0x2c, e.world_string(lab))
            pts.append(q)
        elems = [DEF] + pts
        vec = e.alloc(4 * (len(elems) + cap_extra))
        for i, x in enumerate(elems):
            e.w32(vec + 4 * i, x)
        e.w32(w + 0xc, vec)
        e.w32(w + 0x10, vec + 4 * len(elems))
        e.w32(w + 0x14, vec + 4 * (len(elems) + cap_extra))
        e.w32(w + 0x1c, e.world_string('mcbwaynet' if vt == MCB_VT else 'nav'))
        sent = e.alloc(0xc)
        e.w32(sent, sent)
        e.w32(sent + 4, sent)
        e.w32(w + 0x28, sent)
        return w, pts

    def randomize(self):
        r, e, t = self.r, self.e, self.t
        n = r.choice([0, 1, 1, 2, 3, 5])
        nn = r.choice([-1, -1, 0, 1, 3])
        names = ['MFB Baker', 'mfb able', 'MFB Charlie', 'Dock', 'mfb baker', 'Nav One', 'Nav Two', 'Nav 3']
        labels = [r.choice(names) for _ in range(n + max(nn, 0))]
        pos = [[f32(r.uniform(-300, 300)) for _ in range(3)] for _ in labels]
        self.mcb, mp = self.waynet(MCB_VT, 0x48, 10050, labels[:n], pos[:n], r.choice([0, 2]))
        self.nav, npts = (self.waynet(NAV_VT, 0x30, 10051, labels[n:], pos[n:], r.choice([0, 0, 3]))
                          if nn >= 0 else (0, []))
        self.points = mp + npts
        cur = r.randrange(n + 1)  # always in range: the exe's second press reads it unchecked
        nav_cur = r.randrange(max(nn, 0) + 2)
        e.w32(self.mcb + 0x20, cur & 0xffffffff)
        if self.nav:
            e.w32(self.nav + 0x20, nav_cur)
        ids = list(range(len(self.points))) + [-1, -1]

        def addr(i):
            return self.points[i] if 0 <= i < len(self.points) else 0
        state = [r.choice([0, 1, 2]), r.choice(ids), r.choice(ids), r.choice(ids), int(r.random() < 0.6),
                 r.choice([0, 0, 1, 1, 2, 3])]
        e.w32(self.mcb + 0x30, state[0])
        e.w32(self.mcb + 0x34, addr(state[1]))
        e.w32(self.mcb + 0x38, addr(state[2]))
        e.w32(self.mcb + 0x3c, addr(state[3]))
        if not self.nav:
            state[4] = 0
        e.w32(self.mcb + 0x40, self.nav if state[4] else 0)
        e.w32(self.mcb + 0x44, state[5])
        # The mcbwaynet is the mission's only while picking (step 1): it was
        # made so by the first press and is put back by the second or clear.
        if state[5] == 1:
            mission = 1
        else:
            mission = r.choice([0, 2]) if self.nav else 0
        e.w32(0x60b544, self.mcb)
        e.w32(0x609980, [0, self.mcb, self.nav][mission])
        marker = 0
        if r.random() < 0.7:
            marker = e.world_alloc(0x80)
            e.cpu.write(marker, b'\0' * 0x80)
            e.w32(marker + 0x54, r.choice([0, 1]))
        e.w32(0x608ba4, marker)
        t.script['0x004f7fe0'] = e.world_alloc(0x80) if r.random() < 0.7 else 0
        if t.script['0x004f7fe0']:
            e.w32(t.script['0x004f7fe0'] + 0x54, r.choice([0, 1]))
        e.w32(0x608ba0, 2)
        # The dock's.
        t.script['0x0045a000'] = r.choice([0, 1, 3])
        e.w8(0x605b98, int(r.random() < 0.6))
        e.w32(0x605b78, e.world_alloc(0x10))
        e.w32(0x605b7c, e.world_alloc(0x10))
        t.script['0x0045b740'] = r.choice([0, 1])
        t.script['mech.vt+0x50'] = r.choice([self.mech, 0])
        for k in range(3):
            e.wf(self.mech_pos + 4 * k, f32(r.uniform(-300, 300)))
        lab_blob = b''.join(x.encode() + b'\0' for x in labels)
        flat = [x for p in pos for x in p]
        return n, nn, lab_blob, flat, cur, nav_cur, state, mission, marker, labels

    def image(self):
        e = self.e
        ids = {a: i for i, a in enumerate(self.points)}

        def idv(a):
            if a == 0:
                return 0xffffffff
            return ids.get(a, 999 if a == DEF else 998)
        m = self.mcb
        out = [e.r32(m + 0x20), e.r32(m + 0x30), idv(e.r32(m + 0x34)), idv(e.r32(m + 0x38)), idv(e.r32(m + 0x3c)),
               1 if e.r32(m + 0x40) else 0, e.r32(m + 0x44)]
        mw = e.r32(0x609980)
        out.append(1 if mw == m else 2 if mw and mw == self.nav else 9 if mw else 0)
        out.append(e.r32(0x608ba4))
        if self.nav:
            nv = self.nav
            cnt = (e.r32(nv + 0x10) - e.r32(nv + 0xc)) // 4
            out += [e.r32(nv + 0x20), cnt]
            out += [idv(e.r32(e.r32(nv + 0xc) + 4 * j)) if j < cnt else 0xffffffff for j in range(8)]
        else:
            out += [0xffffffff, 0xffffffff] + [0xffffffff] * 8
        out.append((e.r32(m + 0x10) - e.r32(m + 0xc)) // 4)
        sent = e.r32(m + 0x28)
        vals = []
        nd = e.r32(sent)
        while nd != sent:
            vals.append(idv(e.r32(nd + 8)))
            nd = e.r32(nd)
        out.append(e.r32(m + 0x2c))
        out += (vals + [0xffffffff] * 4)[:4]
        return out


OPS = {0: 'next', 1: 'prev', 2: 'nearest', 3: 'select', 4: 'find', 5: 'position', 6: 'clear', 7: 'command',
       8: 'begin', 9: 'requester', 10: 'remove_last', 11: 'note', 12: 'nav next'}


def run_ops(probe, seed):
    w = World(seed)
    r, e, t = w.r, w.e, w.t
    n, nn, labs, flat, cur, nav_cur, state, mission, marker, labels = w.randomize()
    rep = Replay(t, e, e.snapshot())
    probe.probe_app_set_callback(rep.callback())
    probe.probe_wn_setup(n, nn, labs, (F * max(1, len(flat)))(*flat), cur, nav_cur, (ctypes.c_int32 * 6)(*state),
                         mission, marker, 0)
    seq = [r.choice(list(OPS)) for _ in range(r.choice([1, 2, 4, 8]))]
    if r.random() < 0.3:
        seq = [7, 7, 7, 7][:r.choice([1, 2, 3])]
    name = f'waynet seed {seed} (points {n}/{nn}, mission {mission}, step {state[5]}) {[OPS[o] for o in seq]}'
    m = w.mcb
    for k, op in enumerate(seq):
        if op == 12 and not w.nav:
            op = 0
        pos = [f32(r.uniform(-300, 300)) for _ in range(3)]
        pb = e.world_alloc(0x10)
        for j in range(3):
            e.wf(pb + 4 * j, pos[j])
        i = r.randrange(-1, n + 3)
        lab = r.choice(labels + ['Nobody', 'mfb BAKER']) if labels else 'Nobody'
        ls = e.world_string(lab)
        mech = w.mech
        add = r.choice([0, 1])
        want = got = 0
        if op in (0, 1):
            e.call(e.r32(MCB_VT + (0x60 if op == 0 else 0x64)), ecx=m)
        elif op == 2:
            e.call(e.r32(MCB_VT + 0x68), ecx=m, stack=(pb,))
        elif op == 3:
            want = e.call(0x479b90, ecx=m, stack=(i & 0xffffffff,)) & 0xff
        elif op == 4:
            a = e.call(0x479be0, ecx=m, stack=(ls,))
            want = {x: j for j, x in enumerate(w.points)}.get(a, 999 if a == DEF else 998) if a else 0xffffffff
        elif op == 5:
            a = e.call(0x479aa0, ecx=m, stack=(i & 0xffffffff,))
            want = 0 if a == 0 else 1 if a == 0x63d870 else e.r32(a)
        elif op == 6:
            e.call(0x47a6b0, ecx=m)
        elif op == 7:
            e.call(0x47a7a0, ecx=m, stack=(mech,))
        elif op == 8:
            e.call(e.r32(MCB_VT + 0x2c), ecx=m)
        elif op == 9:
            want = e.call(0x47a730, stack=(mech,)) & 0xff
        elif op == 10:
            if not w.nav:
                continue
            j = r.randrange(-1, len(w.points) + 1)
            a = w.points[j] if 0 <= j < len(w.points) else 0
            want = e.call(0x47a1e0, ecx=w.nav, stack=(a,)) & 0xff
            i = j
        elif op == 11:  # a point (or none) onto the watched list or off it
            i = r.randrange(-1, len(w.points) + 1)
            a = w.points[i] if 0 <= i < len(w.points) else 0
            e.call(0x47a270, ecx=m, stack=(a, add))
        elif op == 12:
            e.call(0x479b10, ecx=w.nav)
        got = probe.probe_wn_call(op, (F * 3)(*pos), i, lab.encode() if op != 11 else (b'x' if add else None), mech)
        if op in (3, 9, 10):
            got &= 0xff
        if want != got:
            print(f'{name}: op {k} {OPS[op]} answered original {want:#x} rebuild {got:#x}')
            return False
        cover(f'op {OPS[op]}')
    if not compare(name, t.log, rep.log):
        return False
    want = w.image()
    out = (U * 64)()
    k = probe.probe_wn_image(out)
    got = list(out)[:k]
    if want != got:
        bad = next((j for j in range(min(len(want), len(got))) if want[j] != got[j]), None)
        print(f'{name}: image word {bad} original {[hex(x) for x in want]}\n  rebuild {[hex(x) for x in got]}')
        return False
    keys = [kk for kk, _ in t.log]
    cover('command: occupied', any(kk == '0x004e6d50' and a[0] == e.r32(0x605b78) for kk, a in t.log))
    cover('command: picked', any(kk == '0x004e6d50' and a[0] == e.r32(0x605b7c) for kk, a in t.log))
    cover('command: asked', '0x0045a500' in keys)
    cover('command: aborted', '0x0045a540' in keys)
    cover('command: onto the nav', w.nav and want[11] != 0xffffffff and want[10] > 1 + max(nn, 0))
    cover('marker placed', '0x004fc0b0' in keys)
    cover('marker hidden', any(kk == '0x004eb170' and a[1] == 0 for kk, a in t.log))
    cover('marker found', '0x004f7fe0' in keys)
    cover('marker into the world', 'world.vt+0x0' in keys)
    return True


def run_point(probe, seed):
    r = rnd(seed, 'point')
    t = Trace(seed=seed)
    e = Emu(t)
    e.heap_stubs()
    heap_strs(e)
    e.stub(0x56f6f0, spec='zc')
    e.stub(0x56e3d0, spec='c,s0')
    t.script['0x0056f6f0'] = r.choice([0, e.world_alloc(0x10)])
    name = r.choice(['p_1', None, 'Alpha'])
    label = r.choice(['MFB Baker', None, 'One'])
    pos = [f32(r.uniform(-500, 500)) for _ in range(3)]
    n = r.choice([0, 1, 3, 5])
    links = [r.choice([2, 7, -1, -1]) for _ in range(n)]
    b = [r.choice([0, 1]) for _ in range(5)]
    q = e.world_alloc(0x40)
    e.cpu.write(q, b'\0' * 0x40)
    pb, lb = e.world_alloc(0x10), e.world_alloc(4 * max(1, n))
    for k in range(3):
        e.wf(pb + 4 * k, pos[k])
    for k, x in enumerate(links):
        e.w32(lb + 4 * k, x & 0xffffffff)
    ns = e.world_string(name) if name else 0
    ls_ = e.world_string(label) if label else 0
    rep = Replay(t, e, e.snapshot())
    probe.probe_app_set_callback(rep.callback())
    e.call(0x47a320, ecx=q, stack=(ns, ls_, pb, lb if n else 0, n) + tuple(b))
    out = (U * 16)()
    probe.probe_wn_point(name.encode() if name else None, label.encode() if label else None, (F * 3)(*pos),
                         (ctypes.c_int32 * max(1, n))(*(links or [0])) if n else None, n, (ctypes.c_uint8 * 5)(*b),
                         out)
    reached = r.choice([0, 1, 2])
    for _ in range(reached):
        on = r.choice([0, 1])
        e.call(0x47a500, ecx=q, stack=(on,))
        probe.probe_wn_reached(on)
    nm = f'point seed {seed} ({name}, {label}, links {links}, bytes {b})'
    if not compare(nm, t.log, rep.log):
        return False
    want = [e.r8(q + 0xc), e.r8(q + 0xd), e.r8(q + 0xe), e.r8(q + 0xf), e.r8(q + 0x10), e.r32(q + 0x14),
            e.r32(q + 0x30), e.r8(q + 0x34)] + [e.r32(q + 0x1c + 4 * k) for k in range(3)] + \
           [int(e.r32(q + 0x28) != 0), int(e.r32(q + 0x2c) != 0)]
    gw = list(out)[:13]
    gw[7] = probe.probe_wn_reached_get()
    if want != gw:
        print(f'{nm}: original {want} rebuild {gw}')
        return False
    cover('point: dock', b[0] == 1)
    cover('point: nav line', b[0] == 0 and label is not None)
    cover('point: trailing links', n > 0 and links[-1] == -1)
    return True


def main():
    probe = load_probe()
    probe.probe_app_set_callback.argtypes = [CALLBACK]
    P = ctypes.c_void_p
    probe.probe_wn_setup.argtypes = [ctypes.c_int32, ctypes.c_int32, ctypes.c_char_p, P, ctypes.c_int32,
                                     ctypes.c_int32, P, ctypes.c_int32, U, ctypes.c_int32]
    probe.probe_wn_call.argtypes = [ctypes.c_int32, P, ctypes.c_int32, ctypes.c_char_p, U]
    probe.probe_wn_call.restype = U
    probe.probe_wn_image.argtypes = [P]
    probe.probe_wn_point.argtypes = [ctypes.c_char_p, ctypes.c_char_p, P, P, ctypes.c_int32, P, P]
    probe.probe_wn_reached.argtypes = [ctypes.c_uint8]
    probe.probe_wn_reached_get.restype = U
    probe.probe_wn_hooks(1)
    args = [a for a in sys.argv[1:] if not a.startswith('-')]
    runs = int(args[0]) if args else 600
    for seed in range(runs):
        if not run_ops(probe, seed):
            sys.stdout.flush()
            os._exit(1)
    for seed in range(runs // 2):
        if not run_point(probe, seed):
            sys.stdout.flush()
            os._exit(1)
    missed = sorted(k for k, n in COVER.items() if not n) + [f'op {o}' for o in OPS.values() if f'op {o}' not in COVER]
    if missed:
        print(f'not reached: {missed}')
        sys.stdout.flush()
        os._exit(1)
    print(f'ok: {runs} waynet runs and {runs // 2} points as the original ({len(COVER)} branches reached)')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()
