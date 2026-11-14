"""Pirate's Moon's ally hit 0x4bb6a0 and stand-down 0x4ba8c0 against the port's pm_hit_an_ally
(tests/probe_pm_ally_hit.cpp), on PM's exe: the ally kept as the obstacle (+0x28), and, with the byte behind +0x94
set, the firing busy (+0x34), its wait (+0x30) the rest (+0x10) and the pick's hold (+0x4c) over, the time (+0x04)
and phase (+0x00) left. The byte is made from [0x60b70c] (0x4be1a0), checked here to be 1 in the exe's data, and
nothing writes either, so this is the only branch the exe takes; the other (the rest) is ported for preservation.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_ally_hit.py [cases]
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_pm_zar  # noqa: E402
from original import ROOT  # noqa: E402

HIT, DEFAULT = 0x4bb6a0, 0x60b70c

probe = ctypes.CDLL(str(next((ROOT / 'build').glob('libpmprobe.*'))))
probe.probe_pm_ally_hit.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
IN, OUT = (ctypes.c_uint32 * 8)(), (ctypes.c_uint32 * 8)()


def bits(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def main():
    cases = int(sys.argv[1]) if len(sys.argv) > 1 else 2000
    e = test_pm_zar.Emulated()
    uc = e.cpu._uc
    default = uc.mem_read(DEFAULT, 1)[0]
    print(f'[0x60b70c] = {default}')
    flag = e.alloc(4)
    uc.mem_write(flag, bytes([default]))
    fire = e.alloc(0xa0)
    r = random.Random(0x4ba8c0)
    bad = 0
    for n in range(cases):
        victim = r.choice([0, r.randrange(1, 1 << 31)])
        rest, wait, until, time = (r.choice([0.0, -1.0, float('nan')]) if r.random() < 0.1 else r.uniform(-5, 50)
                                   for _ in range(4))
        busy, phase = r.randrange(2), r.randrange(9)
        words = [victim, bits(rest), bits(wait), busy, bits(until), bits(time), phase]
        uc.mem_write(fire, bytes(0xa0))
        uc.mem_write(fire + 0x00, struct.pack('<I', phase))
        uc.mem_write(fire + 0x04, struct.pack('<I', words[5]))
        uc.mem_write(fire + 0x10, struct.pack('<I', words[1]))
        uc.mem_write(fire + 0x30, struct.pack('<I', words[2]))
        uc.mem_write(fire + 0x34, bytes([busy]))
        uc.mem_write(fire + 0x4c, struct.pack('<I', words[4]))
        uc.mem_write(fire + 0x94, struct.pack('<I', flag))
        e.call(HIT, ecx=fire, args=(victim,))
        g = lambda o: struct.unpack('<I', bytes(uc.mem_read(fire + o, 4)))[0]
        ex = [g(0x28), g(0x30), uc.mem_read(fire + 0x34, 1)[0], g(0x4c), g(0x04), g(0x00), g(0x10)]
        for i, v in enumerate(words):
            IN[i] = v
        k = probe.probe_pm_ally_hit(IN, OUT)
        pt = list(OUT[:k])
        if ex != pt:
            bad += 1
            if bad <= 5:
                print(f'FAIL case {n}: {words}\n  exe  {ex}\n  port {pt}')
    print(f'{cases} ally hits, {bad} differ')
    sys.stdout.flush()
    os._exit(1 if bad or default != 1 else 0)


if __name__ == '__main__':
    main()
