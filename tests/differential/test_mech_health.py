"""A mech's health, vt+0x98 0x41ba40 (docs/ai.md 17.3), against the original
x86: the parts' internal health weighed and multiplied, cached at +0x40c
while +0x410 is clear; 0 when dead (+0x578 10), 1 when invincible.

    cmake --build build && .venv/bin/python tests/differential/test_mech_health.py

It rides on test_damage.py's mechs (the real Mech and part code in the
emulator, the port's from the same images): after each of their damage
operations the health is asked on both sides, with +0x40c random and +0x410
set or not, and the answer, +0x40c and +0x410 must match bit for bit at the
game's 24-bit x87.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

os.environ.setdefault('MW3_X87', '24')
sys.path.insert(0, str(Path(__file__).parent))
import test_damage as td  # noqa: E402

HEALTH = 0x41ba40


def check(run, rng, stats):
    emu, cpu = run.emu, run.emu.cpu
    cached = td.f2i(td.f32(rng.uniform(0, 1)))
    stale = int(rng.random() < 0.8)
    cpu.u32(emu.mech + 0x40c, cached)
    cpu.write(emu.mech + 0x410, bytes([stale]))
    cpu.call(HEALTH, ecx=emu.mech)
    cpu.call(emu.fstp)
    a = (emu.u32(emu.fslot), emu.u32(emu.mech + 0x40c), cpu.read(emu.mech + 0x410, 1)[0])
    result = ctypes.c_int32()
    out = (ctypes.c_int32 * 8)()
    td.probe.probe_damage_op(6, (ctypes.c_int32 * 16)(td.s32(cached), stale), ctypes.byref(result), out, 8)
    b = (result.value & 0xffffffff, result.value & 0xffffffff, out[0])
    if a != b:
        raise SystemExit(f'health: exe {td.i2f(a[0])} (+0x40c {td.i2f(a[1])}, +0x410 {a[2]}), '
                         f'port {td.i2f(b[0])} (+0x410 {b[2]})')
    state = emu.u32(emu.mech + 0x578)
    kind = 'cached' if not stale else 'dead' if state == 10 else 'invincible' if cpu.read(emu.mech + 0x630, 1)[0] else \
        'whole' if a[0] == td.f2i(1.0) else 'hurt'
    stats[kind] = stats.get(kind, 0) + 1


def main():
    emu = td.Emulated()
    if not hasattr(emu, 'fstp'):
        emu.fslot = emu.cpu.alloc(16)
        emu.fstp = emu.cpu.code(b'\xd9\x1d' + struct.pack('<I', emu.fslot) + b'\xc3')
    run = td.Run(emu)
    rng = random.Random(9810)
    scenarios = int(os.environ.get('HEALTH_SCENARIOS', '400'))
    stats = {}
    for n in range(scenarios):
        run.load(rng, rng.randrange(1 << 31))
        check(run, rng, stats)
        for step in range(rng.randrange(4, 30)):
            run.world['shot'] = run.shot(rng)
            run.world['time'] = td.f32(run.world['time'] + 0.25)
            r = rng.random()
            if r < 0.8:
                op, args = 0, td.random_hit(rng)
            elif r < 0.92:
                part = rng.randrange(13)
                op, args = 1, (part, rng.randrange(5), rng.randrange(6) if part == td.TORSO else 0)
            else:
                op, args = 2, (rng.choice([1, 2, 3, 4, 5, 6, 15, 7, 0]),)
            td.run_op(run, op, args, rng)
            try:
                check(run, rng, stats)
            except SystemExit as e:
                raise SystemExit(f'scenario {n} step {step} after op {op}: {e}')
    missing = [k for k in ('cached', 'dead', 'invincible', 'hurt') if k not in stats]
    if missing:
        raise SystemExit(f'cases never reached: {missing} {stats}')
    print(f'ok: {scenarios} mechs, {sum(stats.values())} health checks identical ({stats})')


if __name__ == '__main__':
    code = 0
    try:
        main()
    except SystemExit as e:
        print(e)
        code = 1
    sys.stdout.flush()
    os._exit(code)
