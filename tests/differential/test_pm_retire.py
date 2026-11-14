"""Pirate's Moon's shot retire 0x5a8390 against the port's zwep::retire (tests/probe_pm_retire.cpp), on PM's exe: a
shot whose stamp (+0x638) is not this frame's count ([0x8c4b60]) is stamped with the next frame's and kept (0: its
update 0x5a7570 puts it back in the list, and the update 0x5a73c0 skips it as dead), so it is drawn once more where
it ended; on that frame it goes as MW3's (speed and state, its flyout animations stopped (0x521050), out of the world
(0x4fd7a0), back to the pool (0x5a6a80) unless TETHER_GUIDED (0x200000), whose owner is cleared): 1.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_retire.py
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

RETIRE, ANIM_STOP, REMOVE, POOL, FRAME, WORLD = 0x5a8390, 0x521050, 0x4fd7a0, 0x5a6a80, 0x8c4b60, 0xb0daa0

probe = ctypes.CDLL(str(next((ROOT / 'build').glob('libpmprobe.*'))))
probe.probe_pm_retire.argtypes = [ctypes.c_uint32, ctypes.c_uint32, ctypes.c_int, ctypes.c_uint32, ctypes.c_uint32,
                                  ctypes.c_void_p]
OUT = (ctypes.c_uint32 * 32)()


def main():
    z = test_pm_zar.Emulated()
    u = z.cpu._uc
    st = {}
    z.native_stub(ANIM_STOP, lambda c: st['log'].extend([1, c.arg(0)]) or 0, 0)
    z.native_stub(REMOVE, lambda c: st['log'].extend([2, c.arg(0), c.arg(1)]) or 0, 0)
    z.native_stub(POOL, lambda c: st.update(released=1) or 0, 0)
    d, p = z.alloc(0x200), z.alloc(0x700)
    r = random.Random(0x5a8390)
    bad = 0
    kinds = {}
    for n in range(2000):
        frame = r.choice([0, 1, 5, 1000, 0xffffffff, r.randrange(1 << 32)])
        stamp = r.choice([frame, frame, (frame + 1) & 0xffffffff, (frame - 1) & 0xffffffff, 0, r.randrange(1 << 32)])
        tether = r.random() < 0.2
        a1, a2 = r.choice([0, 0x1111]), r.choice([0, 0x2222])
        st.update(log=[], released=0)
        u.mem_write(d, bytes(0x200))
        u.mem_write(d + 0x6c, struct.pack('<I', 0x200000 if tether else 0))
        u.mem_write(p, bytes(0x700))
        u.mem_write(p + 4, struct.pack('<I', 0x4040))
        u.mem_write(p + 0xc, struct.pack('<III', 0x5050, a1, a2))
        u.mem_write(p + 0x60c, struct.pack('<f', 5.0))
        u.mem_write(p + 0x638, struct.pack('<I', stamp))
        u.mem_write(FRAME, struct.pack('<I', frame))
        u.mem_write(WORLD, struct.pack('<I', 0x7070))
        ret = z.call(RETIRE, args=(d, p)) & 0xff
        g = lambda o: struct.unpack('<I', bytes(u.mem_read(p + o, 4)))[0]
        e = [ret, g(0x638), g(0x60c), g(0x20), g(0x10), g(0x14), g(4), st['released']] + st['log']
        k = probe.probe_pm_retire(frame, stamp, int(tether), a1, a2, OUT)
        q = list(OUT[:k])
        key = ('freed' if ret else 'held', 'tether' if tether else '')
        kinds[key] = kinds.get(key, 0) + 1
        if e != q:
            bad += 1
            if bad <= 5:
                print(f'FAIL {n}: frame {frame} stamp {stamp} tether {tether}\n  exe  {e}\n  port {q}')
    seen = ', '.join(f"{' '.join(x for x in k if x)} {v}" for k, v in sorted(kinds.items()))
    print(f'2000 retires ({seen}), {bad} differ')
    sys.stdout.flush()
    os._exit(1 if bad else 0)


if __name__ == '__main__':
    main()
