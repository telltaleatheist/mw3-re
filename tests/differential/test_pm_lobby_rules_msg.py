"""Pirate's Moon's lobby rules message 0x20 (piece 6c-2 of the multiplayer group, pm-tests.md) against
lobby_rules_message and MultiplayerGame::read_lobby_rules through probe_pm_lobby_rules_msg, on a lobby screen laid out
as the exe's: each list an object whose items are at +0x15c..+0x160 and whose pick is at +0x16c (its value an item's
+4), read by the exe's own 0x4395e0 and 0x438800 and picked by its 0x48a2e0 (the list's vt+0xa0 sets the pick); each
box an object whose vt+0x8c sets its +0x150.

1. The host's send 0x49d220: the flags (the seven boxes, 0x20 the lock +0x16799), the world, kills, minutes, tons,
   kind and rounds (shorts) and REALLOC (a word), 0x18 bytes, guaranteed to all; a list without a pick sends 0.
2. Its handler 0x49d450 (the sender, the message): the lists picked by the message's values, the boxes set (and
   disabled), the game's block (+0x60 the flags with the lock as it was, +0x64..+0x6a the four lists' values, +0x6c
   rand() (the import stood in for with the case's word), +0x9c its CRC by the exe's 0x597bc0), the lock as sent, and
   on its first lock the team play ([0x62cc34], the message's kind) with the screen's calls after (chat box, 0x5a9520,
   0x49eb20) stood in for and logged.

    cmake --build build --target pmprobe && MW3_EXE=pm .venv/bin/python tests/differential/test_pm_lobby_rules_msg.py
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
SEND, READ = 0x49d220, 0x49d450
SCREEN, DIRTY, TEAM_PLAY, RAND = 0x62db54, 0x62db51, 0x62cc34, 0x5de668
LISTS = [0x13808, 0x13c2c, 0x14050, 0x12fc0, 0x133e4, 0x12778, 0x12b9c]  # kills, minutes, tons, realloc, rounds, kind, world
BOXES = [0x14474, 0x1474c, 0x148b8, 0x14a24, 0x14b90, 0x11b84, 0x145e0]  # respawn, ammo, outside, jets, radar, stock, mobile
LOCK = 0x16799
SHIPPED = [[1, 2, 5, 10, 20, 40, 50, 60, 80, 1000], [5, 10, 20, 30, 1000], list(range(10, 105, 5)),
           [0, 1, 2, 3, 6, -6, -3, -2, -1], list(range(1, 11)), [0, 1, 3, 4],
           [0x100, 0x200, 0x101, 0x201, 0x102, 0x202, 0x103, 0x203]]
N_IN = 114


def s32(v):
    return struct.unpack('<i', struct.pack('<I', v & 0xffffffff))[0]


class Exe:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        stubs = {0x5a8a40: (lambda c: 1, 0), 0x5a97d0: (self.send, 0),
                 0x5a9520: (lambda c: self.log.append(3) or 0, 0), 0x49eb20: (lambda c: self.log.append(4) or 0, 0)}
        for a, (fn, pop) in stubs.items():
            z.native_stub(a, fn, pop)
        f = z.cpu.code(b'\xc3')
        z.native_stub(f, lambda c: self.seed)
        self.uc.mem_write(RAND, struct.pack('<I', f))
        self.vt = z.alloc(0x100)  # the screen's, the lists', the boxes', the chat box's
        for off, fn, pop in ((0x14, lambda c: 0, 4), (0xa0, lambda c: self.w(c.ecx + 0x16c, c.arg(0)) or 0, 4),
                             (0x8c, lambda c: self.w(c.ecx + 0x150, c.arg(0) & 0xff) or 0, 4), (0x80, lambda c: 0, 0),
                             (0x60, lambda c: self.log.append(1) or 0, 4), (0x64, lambda c: self.log.append(2) or 0, 0)):
            f = z.cpu.code(b'\xc3')
            z.native_stub(f, fn, pop)
            self.uc.mem_write(self.vt + off, struct.pack('<I', f))
        self.sc, self.g = z.alloc(0x16800), z.alloc(0x100)
        self.arrs = [z.alloc(4 * 12) for _ in LISTS]
        self.items = [[z.alloc(8) for _ in range(12)] for _ in LISTS]
        self.msg = z.alloc(0x18)

    def w(self, a, v):
        self.uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def r(self, a):
        return struct.unpack('<I', bytes(self.uc.mem_read(a, 4)))[0]

    def send(self, c):
        n = c.arg(1)
        assert n == 0x18 and c.arg(2) & 0xff == 1 and c.arg(3) == 0, (n, c.arg(2), c.arg(3))
        self.sent = list(struct.unpack('<6i', bytes(self.uc.mem_read(c.arg(0), n))))
        return 1

    def screen(self, v):
        w, sc, g = self.w, self.sc, self.g
        self.uc.mem_write(sc, bytes(0x16800)), self.uc.mem_write(g, bytes(0x100))
        w(SCREEN, sc), w(sc + 0x11cf0, g), w(sc, self.vt), w(sc + 0x11388, self.vt)
        for k, base in enumerate(LISTS):
            n, pick = v[14 * k], v[14 * k + 1]
            for i in range(n):
                w(self.arrs[k] + 4 * i, self.items[k][i]), w(self.items[k][i] + 4, v[14 * k + 2 + i])
            w(sc + base, self.vt)
            w(sc + base + 0x15c, self.arrs[k]), w(sc + base + 0x160, self.arrs[k] + 4 * n), w(sc + base + 0x16c, pick)
        for k, base in enumerate(BOXES):
            w(sc + base, self.vt), w(sc + base + 0x150, v[98 + k])
        self.uc.mem_write(sc + LOCK, bytes([v[105]]))

    def run(self, v):
        self.log, self.sent, self.seed = [], None, v[112]
        self.screen(v)
        self.z.call(SEND, ecx=self.sc)
        sent = self.sent
        self.screen(v)
        self.uc.mem_write(self.msg, struct.pack('<6i', *v[106:112]))
        self.uc.mem_write(TEAM_PLAY, bytes([v[113]])), self.uc.mem_write(DIRTY, b'\0')
        self.z.call(READ, args=(7, self.msg))
        sc, g, rd = self.sc, self.g, lambda a: self.uc.mem_read(a, 1)[0]  # noqa: E731
        sh = lambda a: struct.unpack('<h', bytes(self.uc.mem_read(a, 2)))[0]  # noqa: E731
        assert rd(DIRTY) == 1
        first = 4 in self.log
        assert self.log == ([1 if rd(TEAM_PLAY) else 2, 3, 4] if first else []), self.log
        return (sent + [s32(self.r(sc + b + 0x16c)) for b in LISTS] + [s32(self.r(sc + b + 0x150)) for b in BOXES] +
                [rd(sc + LOCK), 1 if first else 0, rd(TEAM_PLAY), s32(self.r(g + 0x60)), sh(g + 0x64), sh(g + 0x66),
                 sh(g + 0x68), sh(g + 0x6a), s32(self.r(g + 0x6c)), s32(self.r(g + 0x9c))])


def case(rng):
    v = [0] * N_IN
    for k, shipped in enumerate(SHIPPED):
        # made-up lists: wide, narrow (repeated values: the first is picked) or past a short (REALLOC's a word)
        lo, hi = rng.choice([(-8, 1100), (0, 4), (-100000, 100000)])
        vals = list(shipped) if rng.random() < 0.6 else [rng.randrange(lo, hi) for _ in range(rng.randrange(0, 13))]
        vals = vals[:12]
        v[14 * k] = len(vals)
        v[14 * k + 1] = rng.choice([-1, len(vals), rng.randrange(len(vals))] if vals else [-1, 0])
        v[14 * k + 2:14 * k + 2 + len(vals)] = vals
    for k in range(7):
        v[98 + k] = rng.randrange(2)
    v[105] = rng.randrange(2)
    flags = rng.randrange(0x400) if rng.random() < 0.8 else rng.randrange(1 << 32)
    shorts = []
    for k in (6, 0, 1, 2, 5, 4):  # world, kills, minutes, tons, kind, rounds
        vals = v[14 * k + 2:14 * k + 2 + v[14 * k]]
        x = rng.choice(vals) if vals and rng.random() < 0.8 else rng.randrange(-5, 1100)
        shorts.append(x & 0xffff)
    vals = v[14 * 3 + 2:14 * 3 + 2 + v[14 * 3]]
    realloc = rng.choice(vals) if vals and rng.random() < 0.8 else rng.randrange(-10, 10)
    b = struct.pack('<IIHHHHHHi', 0x180020, flags, *shorts, realloc)
    v[106:112] = struct.unpack('<6i', b)
    v[112] = rng.randrange(0x8000)
    v[113] = rng.randrange(2)
    return v


def main():
    probe = test_pm_zar.load_pm_probe()
    probe.probe_pm_lobby_rules_msg.argtypes = [ctypes.POINTER(I32), ctypes.POINTER(I32)]
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 2000
    exe = Exe()
    rng = random.Random(113)
    seen = {}
    for i in range(count):
        v = case(rng)
        theirs = exe.run(v)
        out = (I32 * 30)()
        probe.probe_pm_lobby_rules_msg((I32 * N_IN)(*v), out)
        mine = list(out)
        if mine != theirs:
            print(f'case {i}:\n  exe  {theirs}\n  port {mine}', flush=True)
            os._exit(1)
        key = ('first lock' if theirs[21] else 'locked' if theirs[20] else 'open')
        seen[key] = seen.get(key, 0) + 1
    print(f'{count} rules messages identical, sent and read; {dict(sorted(seen.items()))}', flush=True)
    os._exit(0)


if __name__ == '__main__':
    main()
