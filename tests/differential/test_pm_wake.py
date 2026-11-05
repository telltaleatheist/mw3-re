"""PM's foot wake against the port (mech3/mech_wake): the start 0x460e50 with its
level 0x460d90, the stop 0x460eb0 and playing 0x460ef0, over runs of throttles and
stops, moved from test_sound_extra's wake. zEffect's start 0x521a20 and stop 0x51f830
and the wrapper's set-active (PM: vt+0xc, MW3 a direct 0x4eb170) are stubbed alike
and logged. Every call and the level and playing after each op must match. Some
runs start from a forced level and playing that do not agree, on both sides alike.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

os.environ.setdefault('MW3_X87', '24')
sys.path.insert(0, str(Path(__file__).parent))
import pm_floors  # noqa: E402
import test_pm_zar  # noqa: E402
from original import FTOL, IAT_FTOL  # noqa: E402

probe = test_pm_zar.load_pm_probe()
P = ctypes.c_void_p
probe.probe_pm_wake.argtypes = [P, ctypes.c_int, ctypes.c_int32, ctypes.c_uint32, P, ctypes.c_int]
STOP = 0x7fffffff
BUF = ctypes.create_string_buffer(1 << 16)


def fb(v):
    return struct.unpack('<I', struct.pack('<f', v))[0]


class Emu:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        z.cpu.import_code(IAT_FTOL, FTOL)
        self.vt = z.alloc(0x20)
        addr = z.cpu.code(b'\xc2\x04\x00')
        z.cpu._uc.mem_write(self.vt + 0xc, struct.pack('<I', addr))
        z.native_stub(addr, lambda c: self.log(0x63, c.arg(0)), 4)
        z.native_stub(0x521a20, self.start)
        z.native_stub(0x51f830, lambda c: self.log(0x62, c.arg(0)))
        self.w = z.alloc(0x1c)

    def log(self, *v):
        for x in v:
            self.out += struct.pack('<I', x & 0xffffffff)
        return 0

    def start(self, c):
        self.n += 1
        if c.arg(1) or c.arg(3) or c.arg(4):
            raise SystemExit('start args')
        self.log(0x61, c.arg(0), 1 if c.arg(2) else 0)
        return 0x100 + self.n

    def run(self, ops, level, playing):
        self.out, self.n = bytearray(), 0
        z = self.z
        z.cpu._uc.mem_write(self.w, struct.pack('<7I', self.vt, 0x4444, 0x10, 0x11, 0x12, playing, level & 0xffffffff))
        for op in ops:
            if op == STOP:
                z.call(0x460eb0, ecx=self.w)
            else:
                z.call(0x460e50, ecx=self.w, args=(op,))
            level, playing = struct.unpack('<iI', bytes(z.cpu._uc.mem_read(self.w + 0x18, 4)) +
                                           bytes(z.cpu._uc.mem_read(self.w + 0x14, 4)))
            self.log(level, 1 if playing else 0)
        return bytes(self.out)


def main():
    rng = random.Random(0x460eb0)
    emu = Emu()
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    starts = stops = 0
    for r in range(runs):
        ops = []
        for _ in range(rng.randint(1, 12)):
            if rng.random() < 0.25:
                ops.append(STOP)
            else:
                ops.append(fb(rng.choice((0.0, 0.1, 0.2, 0.19999, 0.35, 0.5, 0.49999, 0.8, 1.0, -0.6,
                                          float('nan'), rng.uniform(-1, 1)))))
        ob = (ctypes.c_int32 * len(ops))(*[struct.unpack('<i', struct.pack('<I', o))[0] for o in ops])
        # Mostly the state a wake starts in; forced ones tell "playing" (+0x14) from the level (+0x18).
        level, playing = rng.choice(((-1, 0), (-1, 0), (-1, 0), (1, 0), (-1, 0x77), (2, 0x77)))
        n = probe.probe_pm_wake(ctypes.cast(ob, P), len(ops), level, playing, BUF, len(BUF))
        want = emu.run(ops, level, playing)
        if BUF.raw[:n] != want:
            raise SystemExit(f'run {r} {[hex(o) for o in ops]}:\n  exe  {want.hex()}\n  port {BUF.raw[:n].hex()}')
        starts += want.count(struct.pack('<I', 0x61))
        stops += want.count(struct.pack('<I', 0x62))
    pm_floors.floors(starts=(starts, 3000), stops=(stops, 2000))
    print(f'ok: {runs} runs identical ({starts} starts, {stops} stops)')


if __name__ == '__main__':
    main()
