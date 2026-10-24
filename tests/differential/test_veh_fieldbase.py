"""The mobile field base against the original x86 (docs/vehicles.md, The
mobile field base; mech3/mech_fieldbase): the create 0x4589d0 and
constructor 0x458a70 with the registry's add 0x45a030 (three at most, in a
campaign no more than it has left; a base past them put out of play), the
setup 0x458d40 (the arms' nodes, the scripts' path, the arm script
0x458890 with SetLoopTime 0x5aa3b0, its parts, the AI record's fields), the
frame 0x459640 (the queued steps begun, stepped, ended and dropped; the
throttle held unless the bases may drive; the move, the motion animation,
the arms), the arms 0x4597d0 with 0x458900 and the quaternion 0x525740, the
damage 0x458f50 (the difficulty's share for the player's side, the radio
lines, the alive count 0x45a000, the last base lost 0x47a720, the controls'
hit), the death 0x4590b0 outside a campaign and the destructor 0x458bc0.

    cmake --build build && .venv/bin/python tests/differential/test_veh_fieldbase.py [runs]

The basic vehicle's own pieces are stubbed on both sides (its constructor
0x4ac530, setup 0x49ec00, damage 0x4ad3f0, death 0x4ad2d0, destructor
0x4ac650, move 0x4a1240, motion step 0x4addc0; test_veh_*.py check them),
as are zClass, zEffect, the valves, the sounds and the script's loader and
parts (their poses a fixed function of the call). Compared: the traces, the
base's fields, the registry, the steps' list and running flags, the arm
script's fields, the AI record's words, the matrices written, the throttle.

The registry's one departure (FIX (bug)): a fourth base is written past the
three, over the first valve; the port only counts it (checked apart).
"""
import ctypes
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from unicorn.x86_const import UC_X86_REG_ECX  # noqa: E402
from calltrace import CALLBACK, Emu, Replay, Trace, compare, load_probe, rnd  # noqa: E402

U = ctypes.c_uint32
REG = 0x605ae0
VT = 0x5c23d8
# The branches the runs must reach (each run marks those its trace shows).
COVER = {k: 0 for k in (
    'create: no model', 'create: no copy', 'create: room', 'create: campaign full', 'create: three',
    'create: past three', 'create: valve', 'create: no valve',
    'setup: no controls', 'setup: no brain', 'setup: brain', 'setup: path first', 'setup: path again',
    'setup: script', 'setup: no script',
    'frame: begin next', 'frame: done, next begun', 'frame: done, last', 'frame: stepped, running',
    'frame: stepped', 'frame: destroyed', 'frame: held', 'frame: drives', 'frame: not run', 'frame: after',
    'arms: move', 'arms: still', 'arms: open', 'arms: shut', 'arms: nan',
    'damage: inactive', 'damage: scaled', 'damage: lost, more left', 'damage: lost, last', 'damage: no weapon',
    'damage: no entity', 'damage: ally', 'damage: enemy', 'damage: controls told',
    'destroy: anim', 'destroy: in world', 'destroy: not in world', 'destroy: no node', 'destroy: arms',
    'destroy: steps')}


def cover(label, cond=True):
    if cond:
        COVER[label] += 1


def keys(t):
    return [k for k, _ in t.log]


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


def fb(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def bf(u):
    return struct.unpack('<f', struct.pack('<I', u & 0xffffffff))[0]


def new_emu(seed):
    t = Trace(seed=seed)
    e = Emu(t)
    e.heap_stubs()
    e.cpu.fpu_control(0x007f)
    return t, e


def controls(e, extra=()):
    """A controls block whose IControls sits at +0xa08 (vbtable[1] 0xa04),
    with the base's GetThrottle and SetThrottle (0x4a5b50, 0x4a5b70)."""
    ctl = e.world_alloc(0xa70)
    e.cpu.write(ctl, b'\0' * 0xa70)
    pvt, ivt, vb = e.world_alloc(0x40), e.world_alloc(0x100), e.world_alloc(0x10)
    e.w32(ctl, pvt)
    e.w32(pvt + 0x1c, e.new_stub('ctl.vt+0x1c', '', 0))
    e.w32(ctl + 4, vb)
    e.w32(vb + 4, 0xa04)
    e.w32(ctl + 0xa08, ivt)
    e.w32(ivt + 0xc, e.new_stub('ctl.ictl.vt+0xc', 's0', 4))
    e.w32(ivt + 0x1c, 0x4a5b50)
    e.w32(ivt + 0x20, 0x4a5b70)
    e.w32(ivt + 0x54, e.new_stub('ctl.ictl.vt+0x54', 'c', 0))
    e.w32(ivt + 0x94, e.new_stub('ctl.ictl.vt+0x94', 'c,s0,s1,s2,s3', 0x10))
    return ctl, ctl + 0xa08


def pilot(e, r, gtype, bases_left):
    p, pv = e.world_alloc(0x100), e.world_alloc(0x100)
    game = e.object('game', slots={0x2c: ('', 0)})
    e.w32(p + 0x6c, game)
    e.w32(p + 0x14, pv)
    e.w32(0x60861c, p)
    e.t.script['game.vt+0x2c'] = gtype
    camp = e.world_alloc(0x100)
    e.w32(camp + 0xa8, bases_left)
    e.stub(0x479440, spec='')
    e.t.script['0x00479440'] = camp
    return p, pv


def registry(e, r, count, active=None):
    others = [e.world_alloc(0x160) for _ in range(3)]
    act = active or [1, 1, 1]
    for b, a in zip(others, act):
        e.cpu.write(b, b'\0' * 0x10)
        e.cpu.write(b + 8, bytes([a]))
    valves = [e.world_alloc(0x10) if r.random() < 0.7 else 0 for _ in range(3)]
    e.w32(REG + 0x10, count)
    bases = [others[i] if i < count else 0 for i in range(3)]
    vals = [valves[i] if i < count else 0 for i in range(3)]
    for i in range(3):
        e.w32(REG + 0x14 + 4 * i, bases[i])
        e.w32(REG + 0x20 + 4 * i, vals[i])
    return others, bases, act, vals


def registry_original(e, others, v):
    out = [e.r32(REG + 0x10)]
    for i in range(3):
        b = e.r32(REG + 0x14 + 4 * i)
        out.append(4 if b == v and v else (others.index(b) + 1 if b in others else 0))
    out += [e.r32(REG + 0x20 + 4 * i) for i in range(3)]
    return out


def ret_this(em, key, a, pop):
    em._ret(em.t.handle(key, a) or a[0], pop)


# ---------------------------------------------------------------------------
# The create and the registry


def run_create(probe, seed):
    r = rnd(seed, 'create')
    t, e = new_emu(seed)
    over = r.random() < 0.1  # the fourth base (the FIX)
    count = 3 if over else r.choice([0, 0, 1, 2, 2])
    gtype = 0 if over else r.choice([0, 1, 1, 2, 3])
    left = r.choice([0, 1, 2, 3, 3])
    pilot(e, r, gtype, left)
    others, bases, act, vals = registry(e, r, count)
    ctl = controls(e)[0] if r.random() < 0.5 else 0
    found, cloned, valve = r.random() < 0.9, r.random() < 0.92, r.random() < 0.8
    tname, vname = r.choice(['mobilehq', 'mobilehqs']), r.choice(['mobilehq_01', 'mobilehq_02', 'base'])
    model, clone = e.world_alloc(0x40), e.world_alloc(0x80)
    e.cpu.write(clone, vname.encode() + b'\0')
    t.script['0x004f7fe0'] = model if found else 0
    t.script['0x004f9af0'] = clone if cloned else 0
    t.script['0x004d3c40'] = e.world_alloc(0x10) if valve else 0
    e.stub(0x4f7fe0, spec='c,zd')
    e.stub(0x4f9af0, spec='c,d,s0')
    e.stub(0x4eb300, spec='c,zd')
    e.w32(0x608c84, e.object('world', slots={0: ('s0', 4)}))

    def construct(em, key, a, pop):
        v = a[0]
        em.w8(v + 8, 1)
        em.w32(v + 4, 0)
        em.w32(v + 0xec, 0x5151)
        em.t.handle(key, a)
        em._ret(v, pop)
    e.stub(0x4ac530, spec='c,s0,s1', fn=construct)

    def set_node(em, key, a, pop):
        em.w32(a[0] + 4, a[1])
        em._ret(em.t.handle(key, a), pop)
    e.stub(0x4d6c90, spec='c,s0,s1,s2,s3', fn=set_node)
    e.stub(0x5a7f50, spec='c')
    e.stub(0x4d3c40, spec='zc')
    e.stub(0x4d3c00, spec='c,s0')
    e.stub(0x4eb170, spec='c,d')
    e.stub(0x4eb620, spec='c,d')
    ts, vs = e.world_string(tname), e.world_string(vname)
    rep = Replay(t, e, e.snapshot())
    probe.probe_app_set_callback(rep.callback())
    probe.probe_fb_registry(count, (U * 3)(*bases), (U * 3)(*act), (U * 3)(*vals))
    name = f'create seed {seed} (count {count}, type {gtype}, left {left}, found {found}/{cloned})'
    v = e.call(0x4589d0, ecx=ctl, edx=ts, stack=(vs,))
    made = probe.probe_fb_create(v, ctl, tname.encode(), vname.encode())
    if bool(v) != bool(made):
        print(f'{name}: made original {bool(v)} rebuild {bool(made)}')
        return False
    if not compare(name, t.log, rep.log):
        return False
    k = keys(t)
    cover('create: no model', not found)
    cover('create: no copy', found and not cloned)
    if not v:
        return True
    cover('create: valve', '0x004d3c00' in k)
    cover('create: no valve', '0x004d3c40' in k and '0x004d3c00' not in k)
    cover('create: room', '0x004d3c40' in k)
    cover('create: campaign full', gtype == 1 and count < 3 and '0x004eb170' in k)
    cover('create: three', count == 3)
    cover('create: past three', over)
    want = [e.r8(v + 8), e.r32(v + 0x68), e.r32(v + 0xec), e.r32(v + 0x104), e.r8(v + 0x108), e.r32(v + 0x15c)]
    got = (U * 6)()
    probe.probe_fb_image(got)
    if want != list(got):
        print(f'{name}: base original {[hex(x) for x in want]} rebuild {[hex(x) for x in got]}')
        return False
    want = registry_original(e, others, v)
    got = (U * 7)()
    probe.probe_fb_registry_image(got)
    got = list(got)
    if over:  # the exe's write past the three lands on the first valve
        if want[4] != 4 and e.r32(REG + 0x20) != v:
            print(f'{name}: the original did not write past the three')
            return False
        want[4], got[4] = 0, 0
    if want != got:
        print(f'{name}: registry original {want} rebuild {got}')
        return False
    return True


# ---------------------------------------------------------------------------
# The setup


def run_setup(probe, seed):
    r = rnd(seed, 'setup')
    t, e = new_emu(seed)
    v, node = e.world_alloc(0x160), e.world_alloc(0x80)
    e.cpu.write(v, b'\0' * 0x160)
    e.w32(v, VT)
    e.w32(v + 4, node)
    has_ctl = r.random() < 0.7
    ctl, ictl = controls(e) if has_ctl else (0, 0)
    e.w32(v + 0xe4, ctl)
    brain = e.world_alloc(0x100) if r.random() < 0.8 else 0
    if brain:
        for off in range(0x3c, 0xa4, 4):
            e.w32(brain + off, 0xdeadbeef)
    t.script['ctl.ictl.vt+0x54'] = brain
    made = r.random() < 0.7
    e.w32(0x736584, e.world_alloc(0x10) if made else 0)
    t.script['0x00576100'] = e.world_alloc(0x10)
    frames = r.choice([0, 20, 31, 31])
    t.script['0x004f9d60'] = [e.world_alloc(0x10) if r.random() < 0.85 else 0 for _ in range(7)]
    t.script['0x005aa270'] = [e.world_alloc(0x10) if r.random() < 0.85 else 0 for _ in range(7)]
    e.stub(0x49ec00, spec='c,s0,zs1,s2')
    e.stub(0x4f9d60, spec='c,zd')
    e.stub(0x576100, spec='zc')
    e.stub(0x576140, spec='c,zd')

    def script_ctor(em, key, a, pop):
        this = em.uc.reg_read(UC_X86_REG_ECX)
        em.w8(this + 0x10, 1 if frames else 0)
        em.w32(this + 0x64, frames)
        em._ret(em.t.handle(key, a) or this, pop)
    e.stub(0x5a9990, spec='zs0', fn=script_ctor)
    e.stub(0x5aa270, spec='zs0,s1')
    defn = e.world_alloc(0x10)
    nm = r.choice(['mobilehq_01', 'mobilehq_03'])
    kind = r.choice([2, 2, 5])
    ns = e.world_string(nm)
    rep = Replay(t, e, e.snapshot())
    probe.probe_app_set_callback(rep.callback())
    name = f'setup seed {seed} (controls {has_ctl}, brain {bool(brain)}, frames {frames}, path {made})'
    e.call(0x458d40, ecx=v, stack=(defn, ns, kind))
    probe.probe_fb_setup(v, node, ctl, ictl, frames, int(made), defn, nm.encode(), kind)
    if not compare(name, t.log, rep.log):
        return False
    k = keys(t)
    cover('setup: no controls', not has_ctl)
    cover('setup: no brain', has_ctl and not brain)
    cover('setup: brain', has_ctl and bool(brain))
    cover('setup: path first', '0x00576100' in k)
    cover('setup: path again', '0x00576140' in k)
    cover('setup: script', frames != 0)
    cover('setup: no script', frames == 0)
    a = e.r32(v + 0x13c)
    want = [e.r32(v + 0x120 + 4 * i) for i in range(7)] + [e.r32(v + 0x140 + 4 * i) for i in range(7)]
    want += [e.r8(v + 9), e.r8(a + 0x10), e.r32(a + 0x54), e.r32(a + 0x58), e.r32(a + 0x5c), e.r32(a + 0x60),
             e.r32(a + 0x64), e.r8(a + 0x70), e.r32(a + 0x88), e.r32(a + 0x8c)]
    if brain:
        want += [e.r32(brain + off) for off in (0x60, 0x3c, 0x80, 0x5c, 0x88, 0x98, 0x9c, 0xa0)]
    else:
        want += [0xdeadbeef] * 8
    want.append(int(e.r32(0x736584) != 0))
    got = (U * 33)()
    probe.probe_fb_setup_image(got)
    if want != list(got):
        bad = next(i for i in range(33) if want[i] != got[i])
        print(f'{name}: word {bad} original {want[bad]:#x} rebuild {got[bad]:#x}')
        return False
    return True


# ---------------------------------------------------------------------------
# The frame and the arms


def pose(n, t, k):
    out = []
    for i in range(k):
        a = f32(float((n * 7 + i * 3) % 11) * 0.25)
        out.append(f32(f32(a - f32(1.2)) + f32(t * 0.5)))
    return out


def pose_stubs(e):
    def make(addr, k, base):
        def fn(em, key, a, pop):
            p, tb, out = em.args('c,s0,s1')
            n = em.t.counts.get(key, 0) + base
            em.t.handle(key, (p, tb))
            for i, x in enumerate(pose(n, bf(tb), k)):
                em.wf(out + 4 * i, x)
            em._ret(0, pop)
        e.stub(addr, spec='c,s0', fn=fn)
    make(0x5aafe0, 3, 0)
    make(0x5ab1e0, 4, 100)


def run_frame(probe, seed, arms_only=False):
    r = rnd(seed, 'frame')
    t, e = new_emu(seed)
    v, node = e.world_alloc(0x160), e.world_alloc(0x80)
    e.cpu.write(v, b'\0' * 0x160)
    e.w32(v, VT)
    e.w32(v + 4, node)
    e.w8(v + 8, 1)
    ctl, _ = controls(e)
    e.w32(v + 0xe4, ctl)
    throttle = f32(r.choice([0.0, 0.5, 1.0, -1.0, r.uniform(-1, 1)]))
    e.wf(ctl + 8, throttle)
    entry = e.world_alloc(0xc) if r.random() < 0.7 else 0
    e.w32(v + 0xec, entry)
    anim = e.world_alloc(0x10) if r.random() < 0.5 else 0
    e.w32(v + 0xf4, anim)
    t.script['ctl.ictl.vt+0xc'] = r.choice([0, 1, 1, 1, 2])
    t.script['ctl.vt+0x1c'] = r.choice([0, 0, 0, 1])
    drive = int(r.random() < 0.4)
    e.w8(0x60b4d0, drive)
    # The steps: a std::list (sentinel, then next, prev, the step).
    n = 0 if arms_only else r.choice([0, 1, 1, 2, 3])
    acts = []
    for i in range(n):
        acts.append(e.object(f'act{i}', size=0x20, slots={0: ('', 0), 4: ('s0', 4), 8: ('s0', 4), 0xc: ('', 0),
                                                         0x18: ('', 0)}))
        e.cpu.write(acts[-1] + 4, b'\0' * 4)
        t.script[f'act{i}.vt+0x18'] = r.choice([0, 0, 1])
        t.script[f'act{i}.vt+0x4'] = r.choice([0, 1])
    sent = e.world_alloc(0xc)
    nodes_l = [e.world_alloc(0xc) for _ in range(n)]
    chain = [sent] + nodes_l
    for i, nd in enumerate(chain):
        e.w32(nd, chain[(i + 1) % len(chain)])
        e.w32(nd + 4, chain[i - 1])
    for nd, a in zip(nodes_l, acts):
        e.w32(nd + 8, a)
    e.w32(v + 0x100, sent)
    e.w32(v + 0x104, n)
    begin_next = int(n > 0 and r.random() < 0.3)
    e.w8(v + 0x108, begin_next)
    # The arms.
    has_arms = arms_only or r.random() < 0.8
    target = r.choice([0.0, 1.0])
    now = f32(r.choice([0.0, 1.0, target, r.uniform(0, 1), r.uniform(-0.2, 1.2)]))
    if r.random() < 0.05:
        now = float('nan')
    arms = 0
    if has_arms:
        arms = e.world_alloc(0x90)
        e.cpu.write(arms, b'\0' * 0x90)
        e.w32(arms, 0x5c2388)
        e.wf(arms + 4, 0.5)
        e.wf(arms + 0x88, target)
        e.wf(arms + 0x8c, now)
    e.w32(v + 0x13c, arms)
    nodes = [e.world_alloc(0x10) for _ in range(7)]
    parts = [e.world_alloc(0x10) if r.random() < 0.8 else 0 for _ in range(7)]
    mats = {}
    for i in range(7):
        e.w32(v + 0x120 + 4 * i, nodes[i])
        e.w32(v + 0x140 + 4 * i, parts[i])
        m = e.world_alloc(0x30)
        for k in range(12):
            e.wf(m + 4 * k, f32(r.uniform(-5, 5)))
        mats[nodes[i]] = m

    def node_matrix(em, key, a, pop):
        em.t.handle(key, a)
        em.t.override(key, mats[a[0]])
        em._ret(mats[a[0]], pop)
    e.stub(0x4fc3f0, spec='c', fn=node_matrix)
    e.stub(0x4fc300, spec='c,d')
    pose_stubs(e)
    e.stub(0x4a1240, spec='c')
    e.stub(0x4addc0, spec='c,s0,s1')
    e.stub(0x5a85c0, spec='c')
    e.stub(0x506ca0, spec='c')
    dt = f32(r.choice([0.033, 0.1, 0.5, r.uniform(0, 3)]))
    if arms_only and r.random() < 0.04:
        dt = float('nan')  # the arms' time NaN: 0x458900 puts it at 0
    rep = Replay(t, e, e.snapshot())
    probe.probe_app_set_callback(rep.callback())
    probe.probe_fb_frame_setup(v, ctl, node, n, (U * 4)(*(acts + [0] * (4 - n))), begin_next, throttle, entry, anim,
                               drive, int(has_arms), target, now, (U * 7)(*nodes), (U * 7)(*parts))
    what = 'arms' if arms_only else 'frame'
    name = f'{what} seed {seed} (steps {n}, begin {begin_next}, drive {drive}, arms {has_arms} {now:.2f}->{target})'
    if arms_only:
        e.call(0x4597d0, ecx=v, stack=(fb(dt),))
        probe.probe_fb_arms(dt)
    else:
        e.call(0x459640, ecx=v, stack=(fb(dt),))
        probe.probe_fb_frame(dt)
    if not compare(name, t.log, rep.log):
        return False
    k = keys(t)
    if not arms_only:
        cover('frame: begin next', 'act0.vt+0x0' in k and begin_next)
        ended = 'act0.vt+0xc' in k
        cover('frame: done, next begun', ended and 'act1.vt+0x0' in k)
        cover('frame: done, last', ended and n == 1)
        cover('frame: stepped, running', 'act0.vt+0x4' in k and e.r8(acts[0] + 4) == 1)
        cover('frame: stepped', 'act0.vt+0x4' in k and e.r8(acts[0] + 4) == 0)
        cover('frame: destroyed', t.script['ctl.vt+0x1c'] == 1)
        ran = t.script['ctl.ictl.vt+0xc'] == 1 and t.script['ctl.vt+0x1c'] == 0
        cover('frame: held', ran and not drive)
        cover('frame: drives', ran and drive)
        cover('frame: not run', t.script['ctl.ictl.vt+0xc'] != 1)
        cover('frame: after', any(x.endswith('.vt+0x8') for x in k))
    if arms:
        moved = '0x004fc300' in k or (e.r32(arms + 4) != fb(0.5))
        cover('arms: move', moved)
        cover('arms: still', not moved)
        cover('arms: open', moved and bf(e.r32(arms + 0x8c)) == 1.0)
        cover('arms: shut', moved and bf(e.r32(arms + 0x8c)) == 0.0)
        cover('arms: nan', now != now or dt != dt)
    want = [e.r8(v + 8), e.r32(v + 0xec), e.r32(v + 0xf4), e.r32(ctl + 8),
            e.r32(arms + 0x8c) if arms else 0, e.r32(arms + 4) if arms else 0, e.r8(v + 0x108), e.r32(v + 0x104)]
    nd = e.r32(sent)
    while nd != sent:
        want.append(acts.index(e.r32(nd + 8)))
        nd = e.r32(nd)
    want += [e.r8(a + 4) for a in acts]
    got = (U * 32)()
    k = probe.probe_fb_frame_image(got)
    if want != list(got)[:k]:
        print(f'{name}: original {[hex(x) for x in want]} rebuild {[hex(x) for x in list(got)[:k]]}')
        return False
    out = (U * (13 * 8))()
    k = probe.probe_fb_mats(out)
    got = list(out)[:k]
    for j in range(0, k, 13):
        m = got[j]
        w = [e.r32(m + 4 * i) for i in range(12)]
        if w != got[j + 1:j + 13]:
            print(f'{name}: matrix {m:#x} original {[bf(x) for x in w]} rebuild {[bf(x) for x in got[j + 1:j + 13]]}')
            return False
    return True


# ---------------------------------------------------------------------------
# The damage, the death, the destructor


def run_damage(probe, seed):
    r = rnd(seed, 'damage')
    t, e = new_emu(seed)
    v = e.world_alloc(0x160)
    e.cpu.write(v, b'\0' * 0x160)
    e.w32(v, VT)
    active = int(r.random() < 0.9)
    e.w8(v + 8, active)
    team, allies = r.choice([1, 2, 4]), r.choice([0, 1, 3, 6])
    e.w32(v + 0xc0, team)
    e.w32(v + 0xc4, allies)
    has_ctl = r.random() < 0.8
    ctl, ictl = controls(e) if has_ctl else (0, 0)
    e.w32(v + 0xe4, ctl)
    gtype = r.choice([0, 1, 1, 3])
    _, pv = pilot(e, r, gtype, 3)
    e.w32(pv + 0xc4, r.choice([0, 1, 2, 7]))
    diff = r.choice([0.7, 1.0, 1.3])
    e.wf(0x608e5c, f32(diff))
    count = r.choice([1, 2, 3])
    act = [int(r.random() < 0.6) for _ in range(3)]
    others, bases, act, vals = registry(e, r, count, act)
    sounds = [e.world_alloc(0x10) if r.random() < 0.8 else 0 for _ in range(3)]
    for i, s in enumerate(sounds):
        e.w32(0x605b6c + 4 * i, s)
    e.w32(0x60b544, e.world_alloc(0x10))
    kill = int(r.random() < 0.4)

    def damage(em, key, a, pop):
        if kill:
            em.w8(a[0] + 8, 0)
        em._ret(em.t.handle(key, a), pop)
    e.stub(0x4ad3f0, spec='c,s0,s1,s2', fn=damage)
    t.script['0x004ad3f0'] = r.choice([0, 0, 1])
    e.stub(0x4e6d50, spec='c,s0')
    e.stub(0x47a720, spec='c')
    attacker = e.world_alloc(0x10)
    ent = e.world_alloc(0x100)
    evt = e.world_alloc(0x40)
    e.w32(ent, evt)
    e.w32(evt + 0x28, 0x4d2a70)
    e.w32(ent + 0xc0, r.choice([1, 2, 4]))
    e.stub(0x581e40, spec='')
    t.script['0x00581e40'] = attacker
    e.stub(0x40eee0, spec='*c')
    t.script['0x0040eee0'] = ent if r.random() < 0.8 else 0
    weapon = e.world_alloc(0x10) if r.random() < 0.8 else 0
    x = r.randrange(5)
    amount = f32(r.choice([1.0, 7.5, 40.0, r.uniform(0, 100)]))
    rep = Replay(t, e, e.snapshot())
    probe.probe_app_set_callback(rep.callback())
    probe.probe_fb_registry(count, (U * 3)(*bases), (U * 3)(*act), (U * 3)(*vals))
    probe.probe_fb_sounds((U * 3)(*sounds))
    name = f'damage seed {seed} (type {gtype}, team {team}, active {active}, kill {kill}, alive {act[:count]})'
    want = e.call(0x458f50, ecx=v, stack=(weapon, x, fb(amount)))
    got = probe.probe_fb_damage(v, active, team, allies, ctl, ictl, kill, weapon, x, amount)
    if not compare(name, t.log, rep.log):
        return False
    k = keys(t)
    cover('damage: inactive', not active)
    cover('damage: scaled', active and gtype != 3 and (e.r32(pv + 0xc4) & team) != 0)
    cover('damage: lost, more left', '0x004e6d50' in k and kill)
    cover('damage: lost, last', '0x0047a720' in k)
    cover('damage: no weapon', active and not kill and not weapon)
    cover('damage: no entity', '0x0040eee0' in k and '0x004e6d50' not in k)
    hit = active and not kill and weapon and '0x004e6d50' in k
    cover('damage: ally', hit and (e.r32(ent + 0xc0) & allies) != 0)
    cover('damage: enemy', hit and (e.r32(ent + 0xc0) & allies) == 0)
    cover('damage: controls told', 'ctl.ictl.vt+0x94' in k)
    if (want, e.r8(v + 8)) != (got & 0xffffffff, probe.probe_fb_active()):
        print(f'{name}: original {want} / {e.r8(v + 8)} rebuild {got} / {probe.probe_fb_active()}')
        return False
    return True


def run_die(probe, seed):
    t, e = new_emu(seed)
    v = e.world_alloc(0x160)
    e.cpu.write(v, b'\0' * 0x160)
    e.w32(v, VT)
    e.w8(v + 8, 1)

    def die(em, key, a, pop):
        em.w8(a[0] + 8, 0)
        em._ret(em.t.handle(key, a), pop)
    e.stub(0x4ad2d0, spec='c', fn=die)
    e.stub(0x479440, spec='')
    rep = Replay(t, e, e.snapshot())
    probe.probe_app_set_callback(rep.callback())
    e.call(0x4590b0, ecx=v)
    probe.probe_fb_die(v)
    return compare(f'die seed {seed}', t.log, rep.log)


def run_destroy(probe, seed):
    r = rnd(seed, 'destroy')
    t, e = new_emu(seed)
    v = e.world_alloc(0x160)
    e.cpu.write(v, b'\0' * 0x160)
    e.w32(v, VT)
    anim = e.world_alloc(0x10) if r.random() < 0.5 else 0
    e.w32(v + 0x15c, anim)
    node = e.world_alloc(0x80) if r.random() < 0.85 else 0
    if node:
        e.cpu.write(node, b'\0' * 0x80)
        if r.random() < 0.8:
            parents = e.world_alloc(0x10)
            e.w32(parents, e.world_alloc(0x40))
            e.w32(node + 0x54, 1)
            e.w32(node + 0x58, parents)
    e.w32(v + 4, node)
    arms = e.world_alloc(0x90) if r.random() < 0.8 else 0
    e.w32(v + 0x13c, arms)
    k = r.choice([0, 1, 3])
    sent = e.alloc(0xc)
    chain = [sent] + [e.alloc(0xc) for _ in range(k)]
    for i, nd in enumerate(chain):
        e.w32(nd, chain[(i + 1) % len(chain)])
        e.w32(nd + 4, chain[i - 1])
    e.w32(v + 0x100, sent)
    e.w32(v + 0x104, k)
    for addr, spec in ((0x506ca0, 'c'), (0x583400, 'c'), (0x4ebb40, 'c,d'), (0x506bc0, 'c,d,s0'), (0x4f9050, 'c'),
                       (0x5a7f70, 'c'), (0x5a9dc0, 'c'), (0x4ac650, 'c')):
        e.stub(addr, spec=spec)

    def set_node(em, key, a, pop):
        em.w32(a[0] + 4, a[1])
        em._ret(em.t.handle(key, a), pop)
    e.stub(0x4d6c90, spec='c,s0,s1,s2,s3', fn=set_node)
    rep = Replay(t, e, e.snapshot())
    probe.probe_app_set_callback(rep.callback())
    name = f'destroy seed {seed} (anim {bool(anim)}, node {bool(node)}, arms {bool(arms)}, steps {k})'
    e.call(0x458b30, ecx=v, stack=(0,))
    probe.probe_fb_destroy(v, anim, node, arms, k)
    if not compare(name, t.log, rep.log):
        return False
    ks = keys(t)
    cover('destroy: anim', bool(anim))
    cover('destroy: in world', '0x004ebb40' in ks)
    cover('destroy: not in world', node and '0x004ebb40' not in ks)
    cover('destroy: no node', not node)
    cover('destroy: arms', bool(arms))
    cover('destroy: steps', k > 0)
    want = [e.r32(v + 4), e.r32(v + 0x15c), e.r32(v + 0x104)]
    got = (U * 3)()
    probe.probe_fb_destroy_image(got)
    if want != list(got):
        print(f'{name}: original {want} rebuild {list(got)}')
        return False
    return True


def main():
    probe = load_probe()
    probe.probe_app_set_callback.argtypes = [CALLBACK]
    P = ctypes.c_void_p
    probe.probe_fb_registry.argtypes = [ctypes.c_int32, P, P, P]
    probe.probe_fb_registry_image.argtypes = [P]
    probe.probe_fb_sounds.argtypes = [P]
    probe.probe_fb_create.argtypes = [U, U, ctypes.c_char_p, ctypes.c_char_p]
    probe.probe_fb_image.argtypes = [P]
    probe.probe_fb_setup.argtypes = [U, U, U, U, U, ctypes.c_int32, U, ctypes.c_char_p, ctypes.c_int32]
    probe.probe_fb_setup_image.argtypes = [P]
    probe.probe_fb_frame_setup.argtypes = [U, U, U, ctypes.c_int32, P, ctypes.c_int32, ctypes.c_float, U, U,
                                           ctypes.c_int32, ctypes.c_int32, ctypes.c_float, ctypes.c_float, P, P]
    probe.probe_fb_frame.argtypes = [ctypes.c_float]
    probe.probe_fb_arms.argtypes = [ctypes.c_float]
    probe.probe_fb_frame_image.argtypes = [P]
    probe.probe_fb_mats.argtypes = [P]
    probe.probe_fb_damage.argtypes = [U, ctypes.c_int32, U, U, U, U, ctypes.c_int32, U, U, ctypes.c_float]
    probe.probe_fb_damage.restype = ctypes.c_int32
    probe.probe_fb_die.argtypes = [U]
    probe.probe_fb_destroy.argtypes = [U, U, U, U, ctypes.c_int32]
    probe.probe_fb_destroy_image.argtypes = [P]
    probe.probe_fb_hooks(1)
    args = [a for a in sys.argv[1:] if not a.startswith('-')]
    runs = int(args[0]) if args else 300
    for name, fn in (('create', run_create), ('setup', run_setup), ('frame', run_frame),
                     ('arms', lambda p, s: run_frame(p, s, True)), ('damage', run_damage), ('die', run_die),
                     ('destroy', run_destroy)):
        for seed in range(runs if name != 'die' else 20):
            if not fn(probe, seed):
                sys.stdout.flush()
                os._exit(1)
    missed = [k for k, n in COVER.items() if not n]
    if missed:
        print(f'not reached: {missed}')
        sys.stdout.flush()
        os._exit(1)
    print(f'ok: {runs} creates, setups, frames, arm steps, damages and destructors, 20 deaths, as the original '
          f'({len(COVER)} branches reached)')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()
