"""Pirate's Moon's clock in, 0x472850 (0x11 from the host, and the host's own; MW3's 0x470740), against the port's
net_clock_in through probe_pm_net_clock (piece 3a of the multiplayer group, pm-tests.md). The sender kept at the
network game's +0xa0 (0xd's old host), the clock left [0x62da94], the round's end reason (the message's +4 bits 0..2)
always kept at +0x90. 1 overtime: its line (0x410750, 5 s) once a round (+0xab), nothing more. 2 a winner, 5 no more
enemies, 6 a base destroyed: a line; 3 and 4 the time's messages 0x9aa, 0x9ab (0x4107a0); each then ends the round 3 s
on (+0x94 = 1, +0x88 = the game time [0x62da90] + 3). 0 and 7: nothing more. The HUD's two are stood in for and logged
on both sides.

    cmake --build build --target pmprobe && MW3_EXE=pm .venv/bin/python tests/differential/test_pm_net_clock.py
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
os.environ['MW3_EXE'] = 'pm'
import test_pm_zar  # noqa: E402

I32 = ctypes.c_int32
CLOCK_IN, MP_GAME, HUD_LINE, HUD_MSG = 0x472850, 0x47b710, 0x410750, 0x4107a0
CLOCK_LEFT, NOW = 0x62da94, 0x62da90


def s32(v):
    return struct.unpack('<i', struct.pack('<I', v & 0xffffffff))[0]


def bits(f):
    return struct.unpack('<i', struct.pack('<f', f))[0]


class Exe:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        stubs = {MP_GAME: (lambda c: self.game, 4), HUD_LINE: (self.line, 0),
                 HUD_MSG: (lambda c: self.log.extend([2, s32(c.arg(0)), c.arg(1)]) or 0, 0)}
        for a, (fn, pop) in stubs.items():
            z.native_stub(a, fn, pop)
        self.game = z.alloc(0x100)
        self.msg = z.alloc(12)

    def r(self, a):
        return struct.unpack('<I', bytes(self.uc.mem_read(a, 4)))[0]

    def line(self, c):
        t = bytes(self.uc.mem_read(c.arg(1), 0x80)).split(b'\0')[0]
        self.log += [1, s32(c.arg(0)), len(t)] + list(t)
        return 0

    def run(self, v):
        uc, g = self.uc, self.game
        self.log = []
        uc.mem_write(g, bytes(0x100))
        uc.mem_write(g + 0xa0, struct.pack('<I', 0xeeee))
        uc.mem_write(g + 0xab, bytes([v[4]]))
        uc.mem_write(g + 0x90, struct.pack('<i', v[5]))
        uc.mem_write(g + 0x94, bytes([v[6]]))
        uc.mem_write(g + 0x88, struct.pack('<i', v[7]))
        uc.mem_write(NOW, struct.pack('<i', v[3]))
        uc.mem_write(CLOCK_LEFT, struct.pack('<i', bits(-1.0)))
        uc.mem_write(self.msg, struct.pack('<HHii', 0x11, 12, v[1], v[2]))
        self.z.call(CLOCK_IN, args=(v[0], self.msg))
        self.log += [90, s32(self.r(CLOCK_LEFT)), s32(self.r(g + 0xa0)), s32(self.r(g + 0x90)),
                     uc.mem_read(g + 0x94, 1)[0], s32(self.r(g + 0x88)), uc.mem_read(g + 0xab, 1)[0]]
        return self.log


def case(rng):
    flags = rng.choice(range(8)) | rng.getrandbits(29) << 3 if rng.random() < 0.5 else rng.randrange(8)
    return [rng.randrange(0, 12), s32(flags), bits(rng.uniform(-5, 900)), bits(rng.uniform(0, 3000)),
            rng.randrange(2), rng.randrange(0, 7), rng.randrange(2), bits(rng.uniform(0, 100))]


def main():
    probe = test_pm_zar.load_pm_probe()
    probe.probe_pm_net_clock.argtypes = [ctypes.POINTER(I32), ctypes.POINTER(I32), I32]
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 2000
    exe = Exe()
    rng = random.Random(67)
    seen = {}
    for i in range(count):
        v = case(rng)
        theirs = exe.run(v)
        out = (I32 * 512)()
        n = probe.probe_pm_net_clock((I32 * 8)(*v), out, 512)
        mine = list(out[:n])
        if mine != theirs:
            print(f'case {i} (reason {v[1] & 7}, overtime shown {v[4]}):\n  exe  {theirs}\n  port {mine}', flush=True)
            os._exit(1)
        key = (v[1] & 7, theirs[0] if theirs[0] != 90 else None)
        seen[key] = seen.get(key, 0) + 1
    print(f'{count} clocks identical; (reason, what the HUD showed: 1 a line, 2 a message): {dict(sorted(seen.items(), key=str))}',
          flush=True)
    os._exit(0)


if __name__ == '__main__':
    main()
