"""MESSAGE_AREA's flash and timing against PM's exe: the flash's start 0x5c2430, the message's
end 0x4509f0 (the sound, shown for 3 seconds), and the tick 0x5c24e0 (timed hide, the two colours
swapped each half RATE, the draw). MW3's stays up and white. The real hide and show run on both
sides; the draw is counted. Not here: reading FLASH from the dialog 0x5c2320 (read; seen in the
program's shots), the text itself.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_lab_message.py
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import pm_floors  # noqa: E402
import test_pm_zar  # noqa: E402

probe = test_pm_zar.load_pm_probe()
probe.probe_pm_message_new.argtypes = [ctypes.c_uint32, ctypes.c_uint32]
probe.probe_pm_message_op.argtypes = [ctypes.c_int32, ctypes.c_uint32, ctypes.c_float, ctypes.POINTER(ctypes.c_uint32)]
FLASH, AFTER, TICK, DRAW, SOUND, VT = 0x5c2430, 0x4509f0, 0x5c24e0, 0x5c1d90, 0x58f7d0, 0x5e40e4


def bits(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


class Exe:
    def __init__(self):
        e = self.e = test_pm_zar.Emulated()
        self.draws = 0
        e.native_stub(DRAW, self.draw)
        e.native_stub(SOUND, lambda c: 0)
        self.vt = e.alloc(0x200)
        e.cpu._uc.mem_write(self.vt, bytes(e.cpu._uc.mem_read(VT, 0x200)))
        dirty = e.cpu.code(b'\xc3')
        e.native_stub(dirty, lambda c: 0)
        self.w(self.vt + 0x20, dirty)

    def draw(self, c):
        self.draws += 1
        return 0

    def w(self, a, v):
        self.e.cpu._uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def new(self, top, bottom):
        o = self.o = self.e.alloc(0x500)
        self.draws = 0
        self.w(o, self.vt), self.w(o + 0x24c, top), self.w(o + 0x250, bottom)
        self.w(o + 0x4bc, bits(0.35)), self.w(o + 0x4d0, 1), self.w(o + 0x4d4, 1), self.w(o + 0x4d8, bits(1.0))

    def op(self, op, a, b):
        if op == 0:
            self.e.call(FLASH, ecx=self.o, args=[a, bits(b)])
        elif op == 1:
            self.e.call(AFTER, ecx=self.o)
        else:
            self.e.call(TICK, ecx=self.o, args=[bits(b)])
        r = self.e.r32
        o = self.o
        return [r(o + 0xc) & 0x11, r(o + 0x10), r(o + 0x24c), r(o + 0x250), r(o + 0x4b8), r(o + 0x4bc), r(o + 0x4c0), r(o + 0x4c4),
                self.e.cpu._uc.mem_read(o + 0x4c8, 1)[0], r(o + 0x4cc), r(o + 0x4d0), r(o + 0x474), self.draws]


def main():
    exe = Exe()
    rng = random.Random(0x5c24e0)
    out = (ctypes.c_uint32 * 13)()
    n = bad = swaps = hides = 0
    for run in range(60):
        top, bottom = rng.randrange(1 << 24), rng.randrange(1 << 24)
        exe.new(top, bottom)
        probe.probe_pm_message_new(top, bottom)
        for step in range(120):
            k = rng.random()
            if k < 0.04:
                op, a, b = 0, rng.randrange(1 << 24), rng.choice((0.35, 0.1, 1.0, 0.0))
            elif k < 0.12:
                op, a, b = 1, 0, 0.0
            else:
                op, a, b = 2, 0, rng.choice((1 / 30, 0.033, 0.1, 0.5, 1.0, 3.0))
            before_top = exe.e.r32(exe.o + 0x24c)
            before = exe.e.r32(exe.o + 0xc) & 0x10
            theirs = exe.op(op, a, b)
            probe.probe_pm_message_op(op, a, b, out)
            ours = list(out)
            n += 1
            swaps += theirs[2] != before_top
            hides += (theirs[0] & 0x10) != before
            if theirs != ours:
                bad += 1
                if bad <= 5:
                    print(f'run {run} step {step} op {op} {a:#x} {b}: exe {[hex(x) for x in theirs]}\n  port {[hex(x) for x in ours]}')
    pm_floors.floors(steps=(n, 7000), swaps=(swaps, 100), hides=(hides, 50))
    print(f'{n} steps, {swaps} colour swaps, {hides} hides or shows: {bad} differ')
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    code = 0
    try:
        main()
    except SystemExit as ex:
        if isinstance(ex.code, str):
            print(ex.code)
        code = 0 if ex.code in (None, 0) else 1
    except BaseException:
        import traceback
        traceback.print_exc()
        code = 1
    sys.stdout.flush()
    import os
    os._exit(code)
