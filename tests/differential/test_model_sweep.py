"""zModel's two sweeps against the original x86 in Unicorn, on the missions'
real worlds: the unused models freed, 0x5392c0 (zmod::models_free_unused,
Instant Action's next wave), and the unused materials freed in steps,
0x537f20 (zmod::material_sweep, each Instant Action and multiplayer frame
with 10).

    cmake --build build && .venv/bin/python -P tests/differential/test_model_sweep.py [missions...]

For each mission both sides read the world (test_anim_run's prepare), then
some models' use counts (+0x0c) are set to 0 on both, as when a wave's
mechs are gone: 0x5392c0 releases them. Then 0x537f20 is called with steps
of 1 to 60 models until a pass past the last model frees the materials no
model marks, twice over (the second pass frees nothing new). After each
call the return and both pools are compared: every model slot's use count,
polygon count and link, the pool's count and free head; every material's
first word (alpha, flags), its links, the table's heads. Until 2026-10-30
both calls went to the census unported (docs/campaign.md section 15).
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_anim_run import Compare, prepare, probe  # noqa: E402
from test_gamez import WORLDS  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

probe.ar_material_sweep.argtypes = [ctypes.c_int]
probe.ar_model_set_users.argtypes = [ctypes.c_int, ctypes.c_int]
probe.ar_pools.argtypes = [ctypes.POINTER(ctypes.c_uint32), ctypes.c_int]
MODELS_FREE, MATERIAL_SWEEP = 0x5392c0, 0x537f20
POOL, POOL_SIZE, POOL_COUNT, POOL_FREE = 0x7402e4, 0x7402e0, 0x7402e8, 0x7402ec
MATL_BLOCKS, MATL_CAPACITY, MATL_FREE, MATL_USED, CURSOR = 0x63fbc8, 0x63fbc4, 0x5edd54, 0x5edd58, 0x5edd5c


def i32(v):
    return v - (1 << 32) if v & 0x80000000 else v


def emu_pools(emu):
    u = emu.u
    size, base = i32(u(POOL_SIZE)), u(POOL)
    out = [size & 0xffffffff, u(POOL_COUNT), u(POOL_FREE)]
    for i in range(size):
        s = base + 0x60 * i
        out += [u(s + 0xc), u(s + 0x10), u(s + 0x5c)]
    cap, blocks = i32(u(MATL_CAPACITY)), u(MATL_BLOCKS)
    out += [cap & 0xffffffff, u(MATL_FREE), u(MATL_USED)]
    for i in range(cap):
        m = u(blocks + (i // 1000) * 4) + (i % 1000) * 0x2c
        out += [struct.unpack('<H', emu.cpu.read(m, 2))[0], u(m + 0x28)]
    return out


def port_pools():
    n = probe.ar_pools(None, 0)
    buf = (ctypes.c_uint32 * n)()
    probe.ar_pools(buf, n)
    return list(buf)


def call(emu, addr, ecx=0):
    emu.guard(lambda: emu.cpu.call(addr, ecx=ecx, count=2_000_000_000))
    return i32(emu.cpu.uc.reg_read(UC_X86_REG_EAX))


def run_mission(mission, seed, cmp, stats):
    rng = random.Random(seed)
    emu, port, _, _ = prepare(mission, seed, cmp, stats)
    emu.cpu.u32(CURSOR, 0xffffffff)
    probe.ar_material_sweep_reset()
    cmp(f'{mission} loaded', emu_pools(emu), port_pools())
    # Some held models let go of, on both sides.
    base, size = emu.u(POOL), i32(emu.u(POOL_SIZE))
    held = [i for i in range(size) if emu.u(base + 0x60 * i + 0xc)]
    for i in rng.sample(held, len(held) // 4):
        emu.cpu.u32(base + 0x60 * i + 0xc, 0)
        probe.ar_model_set_users(i, 0)
    want, got = call(emu, MODELS_FREE), probe.ar_models_free_unused()
    assert want == got, f'{mission}: 0x5392c0 freed {want} models, the port {got}'
    cmp(f'{mission} models freed', emu_pools(emu), port_pools())
    stats['models'] += want
    passes = 0
    for k in range(10_000):
        step = rng.randint(1, 60)
        want, got = call(emu, MATERIAL_SWEEP, step), probe.ar_material_sweep(step)
        assert want == got, f'{mission}: 0x537f20({step}) call {k} returned {want}, the port {got}'
        cmp(f'{mission} sweep {k} ({step})', emu_pools(emu), port_pools())
        stats['calls'] += 1
        stats['materials'] += want
        if emu.u(CURSOR) == 0 and k > 0:
            passes += 1
            if passes == 2:
                break
    assert passes == 2, f'{mission}: the sweep never finished'
    del emu


def main():
    missions = sys.argv[1:] or WORLDS
    cmp = Compare()
    stats = {k: 0 for k in ('frames', 'start', 'start_at', 'start_node', 'reset', 'reset_node', 'reset_cockpit',
                            'stop', 'invalidate', 'ram', 'hit', 'player', 'zone', 'running_end',
                            'models', 'materials', 'calls')}
    for i, m in enumerate(missions):
        run_mission(m, 3000 + i, cmp, stats)
        print(f'{m}: ok ({stats["models"]} models, {stats["materials"]} materials freed so far)', flush=True)
    print(f'ok: {len(missions)} worlds, {stats["models"]} models and {stats["materials"]} materials freed in '
          f'{stats["calls"]} sweep calls, identical; {cmp.words} words compared')


if __name__ == '__main__':
    main()
