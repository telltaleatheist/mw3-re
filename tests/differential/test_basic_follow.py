"""The basic vehicle's own controls' path follower against the original x86 (docs/vehicles.md, The basic vehicle's
own controls, part 2; mech3/veh_basic_controls): 0x401050 entered as the controls' frame calls it (ECX the
controls, the target pushed), over a scene in the emulator's memory: the vehicle and the ring on its +0xe0, their
controls (the real vtables 0x5c8c50 and IControls 0x5c8b98, so SetThrottle and SetTurn run), the record's nodes
(0x4ae8e0's class, made with the real constructor) with their link records, the first weapon slot and its def,
the target, the clock [0x60991c]. Stubbed and logged on both sides: the places 0x5a85e0 and lock points 0x5a8630,
the line of sight 0x4d5980, the sweep 0x4a0910 and its clear 0x4a0100, the beam 0x581230, rand, the node free
0x4024a0; operator new and malloc answered alike (a breadcrumb node and its record).

    cmake --build build --target mw3probe && .venv/bin/python -P tests/differential/test_basic_follow.py [cases]

Random scenes: each mode (0..4), nodes made at run time and loaded (their links packed from slot 0, as the reader
fills them: with a gap 0x401620's pick by the count can name an empty slot, which the exe then reads through), links
ahead and behind, blocked or not, within
10 or not, the submarine's pitch, armed or not, the target in and out of each range, active or not, a clear line
or not, beam and locking weapons, a ring of up to three vehicles in mode 2. Compared: the call traces, each
vehicle's controls' fields after (the input curve's two outputs within an ulp, as test_veh_stealth), and the nodes
the frame made.
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
FOLLOW, NOW = 0x401050, 0x60991c
FIELDS = ['mode', 'link', 'node', 'ahead', 'throttle', 'turn', 'heading.x', 'heading.z', 'x760', 'pitch_in',
          'pitch_out', 'in40', 'out48', 'xa18', 'crumb_at', 'chase.x', 'chase.z', 'x350', 'fire_on', 'beam_on',
          'x65c', 'lock set', 'x668', 'slot+0x24', 'player_at.x']
COVER = {f'mode {m}': 0 for m in range(5)}
COVER.update({k: 0 for k in ('made a node', 'freed a node', 'blocked', 'engaged', 'fired', 'ring woke')})


def f2b(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def make(seed, t):
    r = random.Random(seed)
    e = Emu(t)
    e.heap_stubs()

    def alloc(em, key, a, pop):  # answered through the trace, so the rebuild's replay gets the same address
        em.t.handle(key, a)
        got = em.alloc(max(a[0], 4))
        em.t.override(key, got)
        em._ret(got, 0)
    e.stub(0x5abd26, 'new', 's0', 0, alloc)
    e.import_stub(0x5bb664, '@malloc', 's0', 0, alloc)
    e.import_stub(0x5bb668, 'rand', '', 0)
    t.script['rand'] = lambda n, a: random.Random(f'{seed}:rand:{n}').randrange(32768)
    # The CRT's _CIpow (0x5ac386, the input curve's) reaches tables the emulator lacks; its exponent is always 2:
    # the base squared on the x87 (fstp st0, fmul st0, st0, ret), as test_veh_stealth does.
    e.cpu.write(0x5ac386, bytes([0xdd, 0xd8, 0xd8, 0xc8, 0xc3]))
    now = r.uniform(5, 100)
    e.wf(NOW, now)

    def vec(x, y, z):
        a = e.world_alloc(12)
        e.wf(a, x), e.wf(a + 4, y), e.wf(a + 8, z)
        return a

    # Nodes: a few placed nodes linked in a ring or a line, some made at run time (number -1).
    nodes = []
    for i in range(r.randrange(2, 6)):
        n = e.world_alloc(0x34)
        e.w32(n, 0x5c8cb8)
        for k, v in enumerate((r.uniform(-200, 200), r.uniform(-5, 5), r.uniform(-200, 200))):
            e.wf(n + 4 + 4 * k, v)
        e.w32(n + 0x2c, -1 if r.random() < 0.25 else i)
        e.w32(n + 0x30, 0)
        nodes.append(n)
    for i, n in enumerate(nodes):
        count = r.choice([1, 1, 2, 3])  # packed from slot 0, as the reader fills them (0x401620 picks by the count)
        for k in range(count):
            if True:
                e.w32(n + 0x10 + 4 * k, nodes[(i + 1 + k) % len(nodes)])
                rec = e.world_alloc(0x3c)
                for w in range(15):
                    e.wf(rec + 4 * w, r.uniform(-1, 1))
                e.w32(n + 0x1c + 4 * k, rec)
    # The target: active or not, its centroid's data (the distance reads its +0x84 world place), its velocity.
    target = e.world_alloc(0x100)
    e.w32(target + 8, 1 if r.random() < 0.85 else 0)
    tc, tdata = e.world_alloc(0x40), e.world_alloc(0xa0)
    e.w32(tc + 0x38, tdata)
    e.w32(tdata, 0x20 if r.random() < 0.2 else 0)  # its matrix stale: refreshed first
    for k in range(3):
        e.wf(tdata + 0x84 + 4 * k, r.uniform(-150, 150))
    e.w32(target + 0x14, tc)

    def vehicle(mode):
        ctl = e.world_alloc(0xa70)
        e.w32(ctl, 0x5c8c50)
        vbt = e.world_alloc(0x10)
        e.w32(vbt + 4, 0xa58)
        e.w32(ctl + 4, vbt)
        e.w32(ctl + 0xa5c, 0x5c8b98)
        veh = e.world_alloc(0x100)
        e.w32(veh + 0xe4, ctl)
        e.w32(ctl + 0xa6c, veh)
        ang = r.uniform(0, 2 * math.pi)
        e.wf(veh + 0x24, math.sin(ang)), e.wf(veh + 0x2c, math.cos(ang))
        vc, vdata = e.world_alloc(0x40), e.world_alloc(0xa0)
        e.w32(vc + 0x38, vdata)
        e.w32(vdata, 0x20 if r.random() < 0.2 else 0)
        for k in range(3):
            e.wf(vdata + 0x84 + 4 * k, r.uniform(-150, 150))
        e.w32(veh + 0x14, vc)
        e.w32(ctl + 0x38, 2)
        for k, o in enumerate((0x64, 0x68, 0x6c)):
            e.wf(ctl + o, r.uniform(-20, 20))
        e.wf(ctl + 0x32c, r.uniform(-0.3, 0.3))
        e.w32(ctl + 0x374, 1 if r.random() < 0.1 else 0)
        e.w32(ctl + 0x37c, 1 if r.random() < 0.1 else 0)
        e.w32(ctl + 0x354, 1 if r.random() < 0.15 else 0)
        e.w32(ctl + 0x360, 1 if r.random() < 0.15 else 0)
        e.wf(ctl + 0x740, 100.0), e.wf(ctl + 0x744, r.choice([100.0, 90.0, 50.0]))
        e.w32(ctl + 0x760, r.choice([0, 1]))
        e.w32(ctl + 0x764, r.choice([0, 2, 2, 3, 4]))
        e.wf(ctl + 0x7ec, r.uniform(-3, 3))
        node = r.choice(nodes)
        e.w32(ctl + 0xa0c, node)
        links = [k for k in range(3) if e.r32(node + 0x10 + 4 * k)]
        e.w32(ctl + 0xa10, r.choice(links))
        e.w32(ctl + 0xa14, mode)
        e.wf(ctl + 0xa18, now + r.choice([-5.0, 5.0]))
        e.wf(ctl + 0xa1c, r.uniform(1, 20)), e.wf(ctl + 0xa20, r.uniform(1, 10))
        e.wf(ctl + 0xa24, r.choice([1e9, 100.0, r.uniform(1e3, 1e5)]))
        e.wf(ctl + 0xa28, r.uniform(2000, 9000)), e.wf(ctl + 0xa2c, r.uniform(100, 2000))
        e.wf(ctl + 0xa30, r.choice([1e9, 10.0, r.uniform(1e3, 1e5)]))
        for k in range(3):
            e.wf(ctl + 0xa34 + 4 * k, r.uniform(-150, 150))
        e.wf(ctl + 0xa40, now + r.choice([-1.0, 1.0]))
        e.w32(ctl + 0xa54, r.choice([0, 1]))
        if mode in (1, 3) or r.random() < 0.8:  # the first weapon slot and its def (the fire check reads it)
            w, d = e.world_alloc(0x38), e.world_alloc(0x80)
            e.w32(w, d)
            e.w32(d + 0x6c, r.choice([0, 0, 2, 0x8000]))
            e.wf(w + 0x24, now + r.choice([-1.0, 1.0])), e.wf(w + 0x28, r.uniform(0.5, 3))
            e.wf(w + 0x2c, r.uniform(0, 500)), e.wf(w + 0x30, r.choice([1e9, r.uniform(1e3, 5e4)]))
            e.w32(w + 0x34, e.world_alloc(0x10))
            e.w32(ctl + 0x3b4, w)
            e.w32(ctl + 0x390, 0)
            e.w32(ctl + 0x394, r.choice([0, 0, 1]))
        return veh, ctl

    mode = r.randrange(0, 5)
    veh, ctl = vehicle(mode)
    ring = [veh]
    if mode == 2 or r.random() < 0.2:
        for _ in range(r.randrange(1, 3)):
            ring.append(vehicle(r.choice([2, 2, 0]))[0])
    for a, b in zip(ring, ring[1:] + [ring[0]]):
        e.w32(a + 0xe0, b if len(ring) > 1 else a)
    # Places: each entity's 0x5a85e0 a stored vector (the vehicle's own near its node, sometimes within 10).
    places = {}
    for v in ring:
        n = e.r32(e.r32(v + 0xe4) + 0xa0c)
        to = e.r32(n + 0x10 + 4 * e.r32(e.r32(v + 0xe4) + 0xa10))
        near = r.random() < 0.3
        base = to if near else n
        places[v] = vec(e.f32(base + 4) + r.uniform(-8, 8), e.f32(base + 8) + r.uniform(-2, 2),
                        e.f32(base + 0xc) + r.uniform(-8, 8) if near else e.f32(base + 0xc) + r.uniform(-60, 60))
    places[target] = vec(r.uniform(-150, 150), r.uniform(-5, 5), r.uniform(-150, 150))
    lock = vec(1.0, 2.0, 3.0)
    e.stub(0x5a85e0, spec='c', fn=lambda em, k, a, p: em._ret(over(em, k, a, places.get(a[0], 0)), p))
    e.stub(0x5a8630, spec='c', fn=lambda em, k, a, p: em._ret(over(em, k, a, lock), p))
    clear = r.random() < 0.6
    e.stub(0x4d5980, spec='c,s0,s1', fn=lambda em, k, a, p: em._ret(over(em, k, a, 1 if clear else 0), p))

    def sweep(em, k, a, p):
        pts = a[1]
        em.t.handle(k, (a[0],) + tuple(em.r32(pts + 4 * i) for i in range(6)) + (a[2],))
        em._ret(0, p)
    e.stub(0x4a0910, spec='c,s0,s1', fn=sweep)
    e.stub(0x4a0100, spec='c')
    e.stub(0x581230, spec='c,d')
    e.stub(0x4024a0, spec='c')
    for k in (0x51e9d0, 0x51fde0, 0x51ea00):
        e.stub(k, spec='')
    e.stub(0x4ed280, spec='c,d')
    return e, r, veh, ctl, target, ring, mode


def over(em, key, args, value):
    em.t.handle(key, args)
    em.t.override(key, value)
    return value


def snapshot(e, ring, target):
    rows = []
    for v in ring:
        c = e.r32(v + 0xe4)
        w = e.r32(c + 0x3b4)
        rows += [e.r32(c + o) for o in (0xa14, 0xa10, 0xa0c, 0xa54, 0x8, 0xc, 0x748, 0x750, 0x760, 0x44, 0x4c, 0x40,
                                        0x48, 0xa18, 0xa40, 0xa34, 0xa3c, 0x350, 0x390, 0x394, 0x65c)]
        rows += [1 if e.r32(c + 0x664) else 0, e.r32(c + 0x668), e.r32(w + 0x24) if w else 0, e.r32(c + 0x6f0)]
    return rows


def run(probe, seed):
    t = Trace()
    e, r, veh, ctl, target, ring, mode = make(seed, t)
    kind = 1 if r.random() < 0.5 else 0
    snap = e.snapshot()
    rep = Replay(t, e, snap)
    probe.probe_app_set_callback(rep.callback())
    e.cpu.fpu_control(0x007f)
    # The record's kind (next_node's, a run-time node's mode 2) through the controls' +0xa08.
    rec = e.world_alloc(0x5c)
    e.w32(rec + 0x30, kind)
    for v in ring:
        e.w32(e.r32(v + 0xe4) + 0xa08, rec)
    e.call(FOLLOW, ecx=ctl, stack=(target,))
    a = snapshot(e, ring, target)
    out = (U * 4096)()
    k = probe.probe_basic_follow(veh, ctl, target, kind, out)
    b = list(out[:k])
    name = f'seed {seed} (mode {mode}, ring {len(ring)})'
    if not compare(name, t.log, rep.log):
        return False
    per = len(FIELDS)
    for i in range(len(ring)):
        x, y = a[i * per:(i + 1) * per], b[i * per:(i + 1) * per]
        # The input curve's outputs (pitch_out, out48) within an ulp: its powers are the x87's 80 bits (as
        # test_veh_stealth compares them).
        curve = {FIELDS.index('pitch_out'), FIELDS.index('out48')}
        bad = [(n, p, q) for k2, (n, p, q) in enumerate(zip(FIELDS, x, y))
               if p != q and not (k2 in curve and abs(p - q) <= 1)]
        if bad:
            print(f'{name}, vehicle {i}: ' + ', '.join(f'{n} {p:#x}/{q:#x}' for n, p, q in bad))
            return False
    made = b[len(ring) * per:]
    for j in range(0, len(made), 21):
        node = made[j]
        exe = [e.r32(node + 4), e.r32(node + 8), e.r32(node + 0xc), e.r32(node + 0x2c), e.r32(node + 0x10)]
        recw = e.r32(node + 0x1c)
        exe += [e.r32(recw + 4 * w) for w in range(15)] if recw else [0] * 15
        if exe != made[j + 1:j + 21]:
            print(f'{name}: made node {node:#x}: exe {[hex(v) for v in exe]} port {[hex(v) for v in made[j + 1:j + 21]]}')
            return False
        COVER['made a node'] += 1
    COVER[f'mode {mode}'] += 1
    keys = [k for k, _ in t.log]
    COVER['freed a node'] += '0x004024a0' in keys
    COVER['blocked'] += e.r32(ctl + 0xa14) == 4 and mode == 0
    COVER['engaged'] += mode != 1 and e.r32(ctl + 0xa14) == 1
    COVER['fired'] += e.r32(ctl + 0x390) == 1
    COVER['ring woke'] += sum(1 for v in ring[1:] if e.r32(e.r32(v + 0xe4) + 0xa14) == 1) > 0
    return True


def fills(probe, n):
    # 0x4023d0 on its own (ECX the record, EDX `from`, then `to` and the width), over close places, far ones and NaNs:
    # its half-width floor is reached only by places within one and a half widths, which the frame's breadcrumbs never
    # are (they are made 20 or more from the node).
    probe.probe_link_fill.argtypes = [ctypes.POINTER(ctypes.c_float), ctypes.POINTER(ctypes.c_float), U,
                                      ctypes.POINTER(U)]
    bad = 0
    t = Trace()
    e = Emu(t)
    e.cpu.fpu_control(0x007f)
    rec, a, b = e.world_alloc(0x3c), e.world_alloc(12), e.world_alloc(12)
    out = (U * 15)()
    for s in range(n):
        r = random.Random(f'fill {s}')
        p = [r.uniform(-300, 300) for _ in range(3)]
        q = [x + r.choice([r.uniform(-8, 8), r.uniform(-300, 300)]) for x in p]
        if r.random() < 0.03:
            q[r.randrange(3)] = math.nan
        width = r.choice([0.0, 10.0, r.uniform(0, 40)])
        for k in range(3):
            e.wf(a + 4 * k, p[k]), e.wf(b + 4 * k, q[k])
        e.cpu.write(rec, bytes(0x3c))
        e.call(0x4023d0, ecx=rec, edx=a, stack=(b, f2b(width)))
        exe = [e.r32(rec + 4 * k) for k in range(15)]
        probe.probe_link_fill((ctypes.c_float * 3)(*p), (ctypes.c_float * 3)(*q), f2b(width), out)
        port = list(out)

        def nan(w):
            return (w & 0x7f800000) == 0x7f800000 and (w & 0x7fffff) != 0
        if any(x != y and not (nan(x) and nan(y)) for x, y in zip(exe, port)):  # a NaN's sign is not compared
            bad += 1
            if bad <= 3:
                print(f'fill {s}: exe {[hex(v) for v in exe]} port {[hex(v) for v in port]}')
    print(f'{n} link fills (0x4023d0), {bad} differ')
    return bad


def main():
    probe = load_probe()
    probe.probe_basic_follow.argtypes = [U, U, U, I, ctypes.POINTER(U)]
    probe.probe_basic_follow.restype = I
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 800
    bad = 0
    for s in range(n):
        if not run(probe, s):
            bad += 1
            if bad >= 8:
                break
    print(f'{n} cases, {bad} differ; {COVER}')
    bad += fills(probe, 600)
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
