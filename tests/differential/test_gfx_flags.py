"""zModel's and zRender's GfxFlags against the original x86 (docs/render-lighting.md,
the GfxFlags): the starts' part (0x5266f0..0x52672f in zModel's 0x526510,
0x557a95..0x557ac2 in zRender's 0x5578f0, run as instruction ranges with the
flags that pick the hardware or software entry), 0x526750 and 0x557ae0,
over the settings list ([0x731910], 0x597d10 run as it is).

    cmake --build build && .venv/bin/python tests/differential/test_gfx_flags.py [cases]

Each case makes a settings list of 0 to 5 entries (GfxFlags_HW, GfxFlags_SW
and others, any order, random values) on both sides, then 12 calls at random:
the starts for hardware or not, the uses with a listed name, a missing one or
none and random flags. Compared after each: where each pointer points (an
entry by its place, or its own base), the base and the value it reads.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import Emu, Trace, load_probe  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EBX, UC_X86_REG_EFLAGS, UC_X86_REG_ESI, UC_X86_REG_ESP  # noqa: E402

I32 = ctypes.c_int32
LIST, MBASE, MPTR, RBASE, RPTR = 0x731910, 0x74a2c8, 0x74a2cc, 0x8008d4, 0x8008d8
NAMES = ['GfxFlags_HW', 'GfxFlags_SW', 'VMode', 'SoundLOD', 'Gamma']
COUNT = {'calls': 0, 'entry': 0, 'base': 0}


def die():
    sys.stdout.flush()
    os._exit(1)


def run_range(e, begin, end, zf, zero_reg):
    uc = e.uc
    uc.reg_write(zero_reg, 0)
    flags = uc.reg_read(UC_X86_REG_EFLAGS)
    uc.reg_write(UC_X86_REG_EFLAGS, (flags | 0x40) if zf else (flags & ~0x40))
    # The range calls 0x597d10, which returns to the next instruction: run to `end`.
    sp = e.cpu.alloc(0x1000) + 0xf00
    uc.reg_write(UC_X86_REG_ESP, sp)
    uc.emu_start(begin, end, count=2_000_000)


def run_case(probe, seed):
    r = random.Random(seed)
    t = Trace()
    e = Emu(t)
    names = r.sample(NAMES, r.randrange(0, 6))
    values = [r.getrandbits(32) & 0x7fffffff for _ in names]
    probe.probe_gf_reset()
    # The original's list, newest first (each new entry at the head): made in
    # the order given, so the last made is the head.
    head = 0
    entries = []
    for nm, v in zip(names, values):
        ent = e.world_alloc(0x20)
        e.w32(ent, v)
        e.w32(ent + 0x10, e.world_string(nm))
        e.w32(ent + 0x18, head)
        head = ent
        entries.append(ent)
        probe.probe_gf_entry(nm.encode(), v)
    e.w32(LIST, head)
    e.w32(MBASE, 0)
    e.w32(MPTR, MBASE)
    e.w32(RBASE, 0)
    e.w32(RPTR, RBASE)

    def where(p, base):
        return -1 if p == base else entries.index(p) if p in entries else -2

    for k in range(12):
        op = r.randrange(4)
        hw = r.randrange(2)
        name = r.choice(names + ['GfxFlags_HW', 'Missing', None]) if names else r.choice(['GfxFlags_HW', None])
        flags = r.getrandbits(32) & 0x7fffffff
        if op == 0:
            run_range(e, 0x526702, 0x526730, zf=not hw, zero_reg=UC_X86_REG_ESI)
        elif op == 1:
            run_range(e, 0x557a95, 0x557ac3, zf=not hw, zero_reg=UC_X86_REG_EBX)
        else:
            p = e.world_string(name) if name else 0
            e.call(0x526750 if op == 2 else 0x557ae0, ecx=p, edx=flags)
        mp, rp = e.r32(MPTR), e.r32(RPTR)
        a = [where(mp, MBASE), e.r32(MBASE), e.r32(mp), where(rp, RBASE), e.r32(RBASE), e.r32(rp)]
        a = [x - (1 << 32) if x >= 1 << 31 else x for x in a]
        out = (I32 * 6)()
        probe.probe_gf_op(op, hw, name.encode() if name else None, flags, out)
        b = list(out)
        if a != b:
            print(f'seed {seed} call {k} (op {op}, hw {hw}, name {name}, entries {names}): original {a}, rebuild {b}')
            die()
        COUNT['calls'] += 1
        COUNT['entry'] += a[0] >= 0
        COUNT['base'] += a[0] == -1


def main():
    probe = load_probe()
    probe.probe_gf_entry.argtypes = [ctypes.c_char_p, I32]
    probe.probe_gf_op.argtypes = [I32, I32, ctypes.c_char_p, ctypes.c_uint32, ctypes.POINTER(I32)]
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 500
    for seed in range(count):
        run_case(probe, seed)
    print(f'ok: {count} cases ({COUNT["calls"]} calls; zModel on an entry {COUNT["entry"]}, on its base {COUNT["base"]}), '
          'as the original')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()
