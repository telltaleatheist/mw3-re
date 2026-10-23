"""The vehicles' gun aim against the original x86 (docs/vehicles.md, The gun's
aim): the pitch limits' within and clamp (0x4a4180, 0x4a41b0, through the
vehicles' limiter vtable 0x5c8340) by the vehicle's slots vt+0x88..+0x94
(0x4ad740, 0x4ad780, 0x4ad7c0, 0x4ad7f0: the direction in its frame or a
world one turned into it), the health share vt+0x98 0x4ad820, and the AI
controls' turret: vt+0x150 / vt+0x154 (0x4ad6e0, 0x4ad700) through the AI
controls' vtable 0x5cd880 to 0x4e7490 and 0x4e7570, and the turret's turn
0x4e72f0 (on the controls' +0xa04, with the owner's real vtable 0x5c88e8).

    cmake --build build && .venv/bin/python tests/differential/test_veh_aim.py [runs]

Each run: a random rotation for the vehicle and the turret, the shipped
limits (-0.2588, 0.5) or random ones, directions inside and past them (and
straight up, zero, NaN now and then), an aim set or not, no turret node
now and then. The x87 at 24 bits. Compared: answers and outputs (floats
within 4 ulps or 1e-6), the turret's turn call (its angle within 1e-5).
The clamp is the original's (the probe library has zfix bugs off); the
fix's own check is that the clamped direction keeps the heading and sits
on the limit.
"""
import ctypes
import math
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import CALLBACK, Emu, Replay, Trace, call_abs, compare, load_probe, mov_ecx, push_imm, rnd  # noqa: E402

F3 = ctypes.c_float * 3
VEH_VT, AI_VT, LIMIT_VT = 0x5c88e8, 0x5cd880, 0x5c8340


def fb(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


def bf(b):
    return struct.unpack('<f', struct.pack('<I', b & 0xffffffff))[0]


def near(a, b):
    if a == b or (math.isnan(a) and math.isnan(b)):
        return True
    if math.isnan(a) or math.isnan(b) or math.isinf(a) or math.isinf(b):
        return False
    return abs(a - b) <= max(1e-6, 4 * abs(a) * 2 ** -23)


def rotation(r):
    q = [r.uniform(-1, 1) for _ in range(4)]
    n = math.sqrt(sum(x * x for x in q)) or 1
    w, x, y, z = (v / n for v in q)
    if r.random() < 0.3:  # mostly upright, as vehicles are
        a = r.uniform(-math.pi, math.pi)
        w, x, y, z = math.cos(a / 2), 0.0, math.sin(a / 2), 0.0
    return [1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w),
            2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w),
            2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]


def direction(r):
    c = r.random()
    if c < 0.03:
        return [0.0, 0.0, 0.0]
    if c < 0.05:
        return [float('nan'), 0.5, -0.5]
    if c < 0.065:
        return [0.3, float('nan'), -0.5]
    if c < 0.075:
        return [0.0, 0.0, r.choice([-1.0, 1.0])]
    if c < 0.1:
        return [0.0, r.choice([1.0, -1.0]), 0.0]
    e = r.uniform(-1.4, 1.4) if r.random() < 0.6 else r.uniform(-0.3, 0.55)
    h = r.uniform(-math.pi, math.pi)
    s = r.uniform(0.2, 3) if r.random() < 0.3 else 1.0
    return [s * math.cos(e) * math.sin(h), s * math.sin(e), -s * math.cos(e) * math.cos(h)]


def put3(e, a, v):
    for i, f in enumerate(v):
        e.w32(a + 4 * i, fb(f))


def get3(e, a):
    return [bf(e.r32(a + 4 * i)) for i in range(3)]


def float_call(e, addr, ecx, args):
    """A float answer through a fresh thunk (Unicorn keeps translated code)."""
    slot, cw, here = e.alloc(16), e.alloc(4), e.alloc(64)
    e.cpu.write(cw, struct.pack('<H', 0x007f))
    code = b'\xdb\xe3\xd9\x2d' + struct.pack('<I', cw)
    code += b''.join(push_imm(x) for x in reversed(args)) + mov_ecx(ecx)
    code += call_abs(here + len(code), addr) + b'\xd9\x1d' + struct.pack('<I', slot) + b'\xc3'
    e.cpu.write(here, code)
    e.call(here)
    return bf(e.r32(slot))


def run(probe, seed):
    r = rnd(seed, 'aim')
    t = Trace()
    e = Emu(t)
    e.heap_stubs()
    e.cpu.fpu_control(0x007f)
    v, ctl = e.world_alloc(0x100), e.world_alloc(0xd00)
    e.cpu.write(v, b'\0' * 0x100)
    e.cpu.write(ctl, b'\0' * 0xd00)
    e.w32(v, VEH_VT)
    e.w32(v + 0xe4, ctl)
    m = rotation(r) + [r.uniform(-900, 900) for _ in range(3)]
    mat = e.world_alloc(0x30)
    for i, f in enumerate(m):
        e.w32(mat + 4 * i, fb(f))
    e.w32(v + 0x0c, mat)
    x18 = direction(r)
    put3(e, v + 0x18, x18)
    if r.random() < 0.6:
        lo, hi = -0.2588, 0.5
    else:
        lo, hi = sorted([r.uniform(-1.5, 1.5), r.uniform(-1.5, 1.5)])
    lim = [math.sin(lo), math.cos(lo), math.sin(hi), math.cos(hi)]
    lim = [bf(fb(x)) for x in lim]
    e.w32(ctl, AI_VT)
    e.w32(ctl + 0x78c, ctl + 0x790)
    e.w32(ctl + 0x790, LIMIT_VT)
    for i, f in enumerate(lim):
        e.w32(ctl + 0x794 + 4 * i, fb(f))
    vb = e.world_alloc(0x10)
    e.w32(ctl + 4, vb)
    e.w32(vb + 4, 0xcc0)
    e.w32(ctl + 0xcd4, v)
    turret = 0
    ahead = r.random() < 0.1  # the turret straight ahead on its aim: -z exactly 1, against a limit of 1
    if r.random() < 0.85 or ahead:
        turret, data = e.world_alloc(0x40), e.world_alloc(0x100)
        e.w32(turret + 0x38, data)
        for i, f in enumerate([1, 0, 0, 0, 1, 0, 0, 0, 1] if ahead else rotation(r)):
            e.w32(data + 0x60 + 4 * i, fb(f))
    e.w32(ctl + 0x704, turret)
    aim, aim_set = ([0.0, 0.0, -1.0], 1) if ahead else (direction(r), int(r.random() < 0.7))
    put3(e, ctl + 0xcb0, aim)
    e.cpu.write(ctl + 0xcbc, bytes([aim_set]))
    e.stub(0x4fbef0, spec='c,s0,s1,s2')
    # The CRT's _hypot (an import): sqrt(a^2 + b^2) on the x87.
    hyp = e.alloc(32)
    e.cpu.write(hyp, b'\xdd\x44\x24\x04\xd8\xc8\xdd\x44\x24\x0c\xd8\xc8\xde\xc1\xd9\xfa\xc3')
    e.w32(0x5bb5b0, hyp)
    probe.probe_va_setup((ctypes.c_float * 12)(*[bf(fb(x)) for x in m]), (ctypes.c_float * 4)(*lim), F3(*x18),
                         turret, F3(*aim), aim_set)
    rep = Replay(t, e, e.snapshot())
    probe.probe_app_set_callback(rep.callback())
    name = f'aim seed {seed}'
    buf, out = e.world_alloc(0x10), e.world_alloc(0x10)
    for k in range(6):
        d = direction(r)
        put3(e, buf, d)
        world = k & 1
        want = e.call(0x4ad780 if world else 0x4ad740, ecx=v, stack=(buf,)) & 0xff
        mine = probe.probe_va_within(world, F3(*d))
        if want != mine:
            print(f'{name}: within (world {world}) {d}: original {want} rebuild {mine}')
            return False
        want = e.call(0x4ad7f0 if world else 0x4ad7c0, ecx=v, stack=(buf, out)) & 0xff
        got = F3()
        mine = probe.probe_va_clamp(world, F3(*d), got)
        a, b = get3(e, out), list(got)
        if want != mine or not all(near(x, y) for x, y in zip(a, b)):
            print(f'{name}: clamp (world {world}) {d}: original {want} {a} rebuild {mine} {b}')
            return False
    hm, hh = r.choice([100.0, 40.0, 0.0]), r.uniform(-10, 100)
    e.w32(ctl + 0x740, fb(hm))
    e.w32(ctl + 0x744, fb(hh))
    want, mine = float_call(e, 0x4ad820, v, []), probe.probe_va_health(hm, hh)
    if not near(want, mine):
        print(f'{name}: health share original {want} rebuild {mine}')
        return False
    for k in range(3):
        c = 1.0 if ahead and k == 0 else r.uniform(-1, 1)
        want = e.call(0x4ad6e0, ecx=v, stack=(fb(c),)) & 0xff
        mine = probe.probe_va_can_aim(c)
        if want != mine:
            print(f'{name}: can aim ({c}): original {want} rebuild {mine}')
            return False
    if r.random() < 0.5:  # a new aim, then the turn on it
        d = direction(r)
        put3(e, buf, d)
        e.call(0x4ad700, ecx=v, stack=(buf, r.getrandbits(32)))
        got = F3()
        probe.probe_va_aim_at(F3(*d), got)
        a, b = get3(e, ctl + 0xcb0), list(got)
        if not all(near(x, y) for x, y in zip(a, b)) or e.r32(ctl + 0xcbc) & 0xff != 1:
            print(f'{name}: aim at {d}: original {a} rebuild {b}')
            return False
    dt = r.choice([0.033, 0.016, 0.1, r.uniform(0, 0.5), 0.0, r.uniform(15, 40)])
    e.cpu.fpu_control(0x007f)
    want = e.call(0x4e72f0, ecx=ctl + 0xa04, stack=(fb(dt),))
    mine = probe.probe_va_turn(dt)
    if want != mine:
        print(f'{name}: turn answer original {want} rebuild {mine}')
        return False
    ok = compare(name, t.log, rep.log, ulps={'0x004fbef0': (2,)})
    return ok


def fix_check(probe):
    """The fix: clamped, the heading kept and the pitch on the limit."""
    lim = [math.sin(-0.2588), math.cos(-0.2588), math.sin(0.5), math.cos(0.5)]
    ident = [1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0]
    probe.probe_va_setup((ctypes.c_float * 12)(*ident), (ctypes.c_float * 4)(*lim), F3(0, 0, 0), 0, F3(0, 0, 0), 0)
    probe.probe_zfix_bugs(1)
    try:
        for d in ([0.0, 0.9, -0.43], [0.3, 0.8, -0.5], [0.0, -0.8, 0.6]):
            out = F3()
            probe.probe_va_clamp(0, F3(*d), out)
            o = list(out)
            pitch = math.atan2(o[1], math.hypot(o[0], o[2]))
            target = 0.5 if d[1] > 0 else -0.2588
            heading_ok = abs(math.atan2(o[0], -o[2]) - math.atan2(d[0], -d[2])) < 1e-5
            if abs(pitch - target) > 1e-5 or not heading_ok:
                print(f'fix: clamp {d} gives {o} (pitch {pitch})')
                return False
    finally:
        probe.probe_zfix_bugs(0)
    return True


def main():
    probe = load_probe()
    probe.probe_app_set_callback.argtypes = [CALLBACK]
    probe.probe_va_setup.argtypes = [ctypes.c_void_p] * 3 + [ctypes.c_uint32, ctypes.c_void_p, ctypes.c_int]
    probe.probe_va_within.argtypes = [ctypes.c_int, ctypes.c_void_p]
    probe.probe_va_clamp.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p]
    probe.probe_va_health.argtypes = [ctypes.c_float, ctypes.c_float]
    probe.probe_va_health.restype = ctypes.c_float
    probe.probe_va_can_aim.argtypes = [ctypes.c_float]
    probe.probe_va_aim_at.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    probe.probe_va_turn.argtypes = [ctypes.c_float]
    args = [a for a in sys.argv[1:] if not a.startswith('-')]
    runs = int(args[0]) if args else 600
    for seed in range(runs):
        if not run(probe, seed):
            sys.stdout.flush(); os._exit(1)
    if not fix_check(probe):
        sys.stdout.flush(); os._exit(1)
    print(f'ok: {runs} aims (limits, clamps, health shares, turret checks and turns) as the original; the clamp fix on the limit')
    sys.stdout.flush(); os._exit(0)


if __name__ == '__main__':
    main()
