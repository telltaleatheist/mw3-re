"""PM's hit path on its edges, which test_pm_damage's random hits seldom land on:
Torso::hit_section 0x4617a0 with `crosses` 0x461bb0, and MechPart::take_hit 0x461d70.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_hit.py

Shots aimed across each of the torso's eight edges from both sides, so an edge's
one-sided test is seen, with the side and centre structure forced to 0 or not. A
critical's five thresholds with the roll forced on each side of each: the seed
is worked back so the first rand() of the hit is the wanted value on both sides.
The armor's limit 0.15 on both sides of it, a beam (flags 2) and invincible.

Forced, on both sides alike: the armor, the rolls, the structure +0x38, +0x1a8
and +0x1ac (those two the torso's side internals, MW3's +0x1a0 and +0x1a4; the
build and the damage set them in play, not the AI). PM's logic here is MW3's: the
section table, the fallback and the rates are read the same from both exes.
"""
import ctypes
import math
import os
import random
import struct
import sys
from pathlib import Path

os.environ.setdefault('MW3_X87', '24')
sys.path.insert(0, str(Path(__file__).parent))
import pm_floors  # noqa: E402
import test_pm_damage as td  # noqa: E402
from test_pm_damage import f2i, f32, s32  # noqa: E402

probe = td.probe
probe.probe_damage_srand.argtypes = [ctypes.c_uint32]
probe.probe_damage_part_armor.argtypes = [ctypes.c_int, ctypes.c_float]
probe.probe_damage_torso_sides.argtypes = [ctypes.c_int, ctypes.c_float, ctypes.c_float, ctypes.c_float]

TAKE_HIT, TORSO = 0x461d70, td.TORSO
THRESHOLDS = (0.008, 0.013, 0.016, 0.019, 0.02)
LSB = f32(3.0518509447574615e-05)
INV = pow(214013, -1, 1 << 32)
LOCAL_EDGES = [((.2, -1), (-.2, -1)), ((-.2, 1), (.2, 1)), ((-.2, -1), (-1, -1)), ((-1, 1), (-.2, 1)),
               ((1, -1), (.2, -1)), ((.2, 1), (1, 1)), ((-1, -1), (-1, 1)), ((1, 1), (1, -1))]


def seed_for(value, rng):
    """A seed whose first rand() is `value`."""
    after = (value << 16) | rng.randrange(1 << 16)
    return ((after - 2531011) * INV) & 0xffffffff


def both_seed(run, seed):
    run.emu.rand = td.MsvcRand(seed)
    probe.probe_damage_srand(seed)


def compare(what, run, a_res, b_res, a_ev, b_ev):
    bad = td.diff(td.read_state(run.emu, run), td.read_port(run), run.emu)
    if bad or a_ev != b_ev or a_res != b_res:
        raise SystemExit(f'{what}:\n  ' + '\n  '.join(bad[:30]) + f'\n  result {a_res:#x} vs {b_res:#x}'
                         f'\n  original {td.show(a_ev)}\n  port {td.show(b_ev)}')


def force_sides(run, side0, side1, centre):
    t = run.emu.parts[TORSO]
    run.emu.cpu.write(t + 0x1a8, struct.pack('<2f', side0, side1))
    run.emu.cpu.write(t + 0x38, struct.pack('<f', centre))
    probe.probe_damage_torso_sides(TORSO, side0, side1, centre)


def world_of(m, x, z):
    return tuple(x * m[k] + z * m[6 + k] + m[9 + k] for k in range(3))


def edge_shot(run, edge, sign, rng):
    t = run.emu.parts[TORSO]
    w, d = struct.unpack('<2f', run.emu.cpu.read(t + 0x84, 8))
    (ax, az), (bx, bz) = LOCAL_EDGES[edge]
    k = rng.uniform(0.1, 0.9)
    x, z = (ax + (bx - ax) * k) * w, (az + (bz - az) * k) * d
    nx, nz = (bz - az) * d, -(bx - ax) * w
    n = math.hypot(nx, nz) or 1.0
    r = rng.uniform(0.01, 0.5) * sign
    m = run.matrix
    p0 = world_of(m, x + nx / n * r, z + nz / n * r)
    p1 = world_of(m, x - nx / n * r, z - nz / n * r)
    return [f32(v) for v in (*p0, *p1)]


def hit_direct(run, part, flags, dmg, rng):
    emu, cpu = run.emu, run.emu.cpu
    v, f = run.script(rng)
    force = 0.0
    cpu.write(emu.def_, bytes(0x210))
    cpu.u32(emu.def_ + 0x6c, flags)
    cpu.u32(emu.def_ + 0x200, emu.type_)
    cpu.write(emu.type_, bytes(0xd0))
    run.push_script(v, f)
    run.set_world()
    emu.events = []
    cpu.call(TAKE_HIT, ecx=emu.parts[part], stack_args=(emu.def_, 0, f2i(dmg) & 0xffffffff, 0))
    args = [flags, 0, 0, 0, f2i(force), 0, 0, 0, 0, 0, f2i(dmg), part, 0, 0, 0, 0]
    result = ctypes.c_int32()
    buf = (ctypes.c_int32 * 8192)()
    n = probe.probe_damage_op(8, (ctypes.c_int32 * 16)(*[s32(x & 0xffffffff) for x in args]), ctypes.byref(result), buf,
                              8192)
    if n < 0:
        raise SystemExit('port log overflow')
    return 0, 0, emu.events, td.port_events(buf, n)


def main():
    emu = td.Emulated()
    run = td.Run(emu)
    rng = random.Random(1999)
    sections, fallbacks, one_sided, rolls, limits = {}, set(), 0, set(), set()
    scenarios = int(sys.argv[1]) if len(sys.argv) > 1 else 200
    for n in range(scenarios):
        run.load(rng, rng.randrange(1 << 31))
        for step in range(40):
            edge, sign = rng.randrange(8), rng.choice([-1, 1])
            run.world['shot'] = edge_shot(run, edge, sign, rng)
            zero = rng.choice([(1, 1, 1), (0, 1, 1), (1, 0, 1), (0, 0, 1), (0, 0, 0), (1, 1, 0)])
            force_sides(run, *[float(k) * 0.5 for k in zero])
            a_res, b_res, a_ev, b_ev = td.run_op(run, 5, (), rng)
            compare(f'scenario {n} shot {step} edge {edge} sign {sign}', run, a_res, b_res, a_ev, b_ev)
            sections[a_res] = sections.get(a_res, 0) + 1
            one_sided += a_res in (0, 3) and edge in (2, 3, 4, 5, 6, 7)
            if not zero[0] and edge in (2, 3, 6):
                fallbacks.add((a_res, edge))
        for t in THRESHOLDS:
            for v in (int(t / LSB), int(t / LSB) + 1):
                for armor in (0.0, 0.15, 0.1500001):
                    run.load(rng, rng.randrange(1 << 31))
                    part = rng.randrange(13)
                    a32 = f32(armor)
                    emu.cpu.write(emu.parts[part] + 0x40, struct.pack('<f', a32))
                    probe.probe_damage_part_armor(part, a32)
                    run.world['shot'] = run.shot(rng)
                    both_seed(run, seed_for(v, rng))
                    flags = rng.choice([0, 0, 0, 2, 4])
                    res = hit_direct(run, part, flags, f32(rng.uniform(1, 40)), rng)
                    compare(f'scenario {n} roll {v} armor {armor} part {part} flags {flags}', run, *res)
                    rolls.add((t, v * LSB < t))
                    limits.add(armor)
            if n >= 8:
                break
    if set(sections) != set(range(6)):
        raise SystemExit(f'sections reached {sorted(sections)}')
    pm_floors.floors(shots=(sum(sections.values()), 7000), fallbacks=(len(fallbacks), 2), one_sided=(one_sided, 100),
                     rolls=(len(rolls), 10), limits=(len(limits), 3))
    print(f'ok: {sum(sections.values())} aimed shots by section {[sections[k] for k in range(6)]}, '
          f'{one_sided} through a side edge into the centre, {len(rolls)} roll sides, armor limits {sorted(limits)}')


if __name__ == '__main__':
    main()
