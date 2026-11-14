"""The basic vehicle's own controls against the original x86 (docs/vehicles.md, The basic vehicle's own
controls; mech3/veh_basic_controls): their init 0x4ae3c0 (the base's init 0x4ac8a0 stubbed; the net's record
by its number through the real 0x4022c0 over [0x5f4c4c]; the mode from its kind; the record's net's node
nearest the vehicle, 0x4d51d0 and the element's vt+0x80, stubbed; the vehicle's "healthy" node 0x4f9d60 and its
flags 0x4eb750 / 0x4eb620; the ranges from the record, each 0 its default; the vehicle's +0xe0), their setup
0x4ae6b0 (the base's 0x4acbc0 stubbed; the definition's activation, pursuit_range, not_pursuit_dwell and
return_range for the record's zeros, the pursuit pair put in order) and the answers 0x4ae340, 0x4ae350, 0x4ae6a0.
Ported for preservation, not wired in.

    cmake --build build --target mw3probe && .venv/bin/python -P tests/differential/test_basic_controls.py [cases]

Random cases: no vehicle node, no positive number, no record of that number, records with zeros, NaNs and
values, each kind, no nearest element, no "healthy" node, with and without the setup and each of its keys.
Compared: the call traces and the fields.
"""
import ctypes
import math
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import Emu, Replay, Trace, compare, load_probe  # noqa: E402

U, I = ctypes.c_uint32, ctypes.c_int32
INIT, SETUP, MODE, GOAL, DEAD = 0x4ae3c0, 0x4ae6b0, 0x4ae340, 0x4ae350, 0x4ae6a0
HEAD = 0x5f4c4c
COVER = {k: 0 for k in ('no node', 'no number', 'no record', 'record', 'no element', 'no healthy', 'setup',
                        )}


def fv(r):
    return r.choice([0.0, 0.0, r.uniform(1, 400), r.uniform(-50, 50), math.nan])


def make(seed, t):
    r = random.Random(seed)
    e = Emu(t)
    ctl = e.world_alloc(0xa70)
    vbt = e.world_alloc(0x10)
    e.w32(vbt + 4, 0xa58)
    e.w32(ctl + 4, vbt)
    owner = e.world_alloc(0x100)
    node = e.world_alloc(0x40) if r.random() < 0.9 else 0
    e.w32(owner + 4, node)
    e.w32(ctl + 0xa6c, owner)  # the IControls' +0x10 (its part at +0xa5c)
    recs = []
    for i in range(r.randrange(0, 4)):
        a = e.world_alloc(0x5c)
        e.w32(a + 0x18, r.randrange(1, 5))
        e.w32(a + 0x30, r.randrange(0, 4))
        for off in range(0x38, 0x54, 4):
            e.wf(a + off, fv(r))
        recs.append(a)
    for a, b in zip(recs, recs[1:] + [0]):
        e.w32(a + 0x58, b)
    e.w32(HEAD, recs[0] if recs else 0)
    net = r.choice([0, -1, 1, 2, 3, 4, 5])
    state0 = r.choice([0, 2, 4])
    rec0 = r.choice(recs) if recs and r.random() < 0.3 else 0
    e.w32(ctl + 0x38, state0)
    e.w32(ctl + 0xa04, net)
    e.w32(ctl + 0xa08, rec0)
    pos = e.world_alloc(16)
    netnode = e.world_alloc(0x40)
    e.w32(netnode + 0x2c, r.randrange(-1, 30))
    element = e.object('element', slots={0x80: ('c', 0)})
    t.script['element.vt+0x80'] = netnode
    t.script['0x005a85e0'] = pos
    t.script['0x004d51d0'] = element if r.random() < 0.85 else 0
    healthy = e.world_alloc(0x40) if r.random() < 0.7 else 0
    t.script['0x004f9d60'] = healthy
    e.stub(0x4ac8a0, spec='c')
    e.stub(0x5a85e0, spec='c')
    e.stub(0x4d51d0, spec='c,s0')
    e.stub(0x4f9d60, spec='c,zd')
    e.stub(0x4eb750, spec='c,d')
    e.stub(0x4eb620, spec='c,d')
    # The setup's: the base's, the definition's keys (a list node: +0x04 the items, a value at +4 + 8 i).
    setup = r.random() < 0.7
    defn = e.world_alloc(0x10)
    keys = {}
    for k in ('activation', 'pursuit_range', 'not_pursuit_dwell', 'return_range'):
        if r.random() < 0.75:
            n, items = e.world_alloc(0x10), e.world_alloc(0x20)
            e.w32(n + 4, items)
            for i in (1, 2):
                e.wf(items + 4 + 8 * i, r.choice([r.uniform(1, 300), r.uniform(1, 300), 0.0]))
            keys[k] = n
    e.stub(0x4acbc0, spec='c,s0,s1,s2')
    def find(em, k, a, p):
        em.t.handle(k, a)
        v = keys.get(a[1], 0)
        em.t.override(k, v)  # the rebuild's replay gets the same node
        em._ret(v, p)
    e.stub(0x5569e0, spec='c,zd', fn=find)
    case = dict(node=node != 0, net=net, records=len(recs), setup=setup, element=t.script['0x004d51d0'] != 0,
                healthy=healthy != 0)
    return e, case, ctl, owner, node, net, state0, rec0, defn if setup else 0, r.randrange(0, 9), r.choice([net, 1, 2, 3])


def run(probe, seed):
    t = Trace()
    e, case, ctl, owner, node, net, state0, rec0, defn, arg2, net2 = make(seed, t)
    snap = e.snapshot()
    rep = Replay(t, e, snap)
    probe.probe_app_set_callback(rep.callback())
    e.cpu.fpu_control(0x007f)
    e.call(INIT, ecx=ctl)
    if defn:
        e.call(SETUP, ecx=ctl, stack=(defn, arg2, net2))
    mode = e.call(MODE, ecx=ctl)
    nn = e.r32(ctl + 0xa0c)
    goal = e.call(GOAL, ecx=ctl) if nn else 0
    dead = e.call(DEAD, ecx=ctl) & 0xff
    out = (U * 24)()
    k = probe.probe_basic_controls(ctl, owner, node, net, e.r32(0x5f4c4c), state0, rec0, defn, arg2, net2, out)
    name = f'seed {seed} {case}'
    if not compare(name, t.log, rep.log):
        return False
    a = [e.r32(ctl + 0x38), e.r32(ctl + 0xa04), e.r32(ctl + 0xa08), nn, e.r32(ctl + 0xa14), e.r32(ctl + 0xa18)]
    a += [e.r32(ctl + o) for o in (0xa1c, 0xa20, 0xa24, 0xa28, 0xa2c, 0xa30, 0xa44)]
    a += [e.r32(owner + 0xe0), mode & 0xffffffff, goal & 0xffffffff, 1 if dead else 0]
    b = list(out[:k])
    if a != b:
        names = ['state', 'net', 'record', 'node', 'mode', 'xa18', 'xa1c', 'dwell', 'xa24', 'far', 'near', 'return',
                 'activation', 'owner+0xe0', 'path_mode', 'path_goal', 'destroyed']
        diff = [f'{n}: {x:#x} / {y:#x}' for n, x, y in zip(names, a, b) if x != y]
        print(f'{name}: {diff}')
        return False
    COVER['no node' if not case['node'] else 'no number' if net <= 0 else
          'record' if e.r32(ctl + 0x38) == 2 else 'no record'] += 1
    if case['node'] and e.r32(ctl + 0x38) == 2:
        COVER['no element'] += not case['element']
        COVER['no healthy'] += not case['healthy']
        COVER['setup'] += bool(defn)
    return True


def main():
    probe = load_probe()
    probe.probe_basic_controls.argtypes = [U, U, U, I, U, I, U, U, U, I, ctypes.POINTER(U)]
    probe.probe_basic_controls.restype = I
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 600
    bad = sum(0 if run(probe, s) else 1 for s in range(n))
    print(f'{n} cases, {bad} differ; {COVER}')
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
