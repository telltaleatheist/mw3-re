"""Pirate's Moon's game kinds' queries (piece 4a of the multiplayer group, pm-tests.md) against the port through
probe_pm_game_kind. On the network game (kind +0x98, the side whose base is base 1 +0xb0 through vt+0x48 0x436010):

1. the spawn base 0x437a40 (MultiplayerGame::spawn_base): 24, but in kinds 3 and 4 by the player's team's side (its
   third data word's bit 0; the lobby's lookups 0x498800 and 0x498930 run as the exe's): 0, 8 or 16;
2. the base check 0x437920 (base_counts): kinds 0, 1 none; 4 either base; else the base vt+0x48 names;
3. the base destroyed 0x472b40 (base_destroyed): over kind 1 and a base that counts, reason 6, the base kept (+0x8c),
   the clock sent now (+0x84);
4. the jump jets' assist, a game's vt+0x44 (jets_assist_of): the multiplayer game's 0x4365f0 (+0xa7), every other
   game's 0x435070 (the MW2 jump jets option 0x40afd0, bit 7 of [[0x6187c4]], the other way round).

    cmake --build build --target pmprobe && MW3_EXE=pm .venv/bin/python tests/differential/test_pm_game_kinds.py
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
SPAWN_BASE, BASE_COUNTS, BASE_DESTROYED, VT48, MP_JETS, JETS = 0x437a40, 0x437920, 0x472b40, 0x436010, 0x4365f0, 0x435070
NOW, OPTIONS = 0x62da90, 0x6187c4


def s32(v):
    return struct.unpack('<i', struct.pack('<I', v & 0xffffffff))[0]


def bits(f):
    return struct.unpack('<i', struct.pack('<f', f))[0]


class Exe:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        z.native_stub(0x47b710, lambda c: self.game, 4)
        self.vt = z.alloc(0x100)
        self.uc.mem_write(self.vt + 0x48, struct.pack('<I', VT48))
        self.opts = z.alloc(4)
        self.uc.mem_write(OPTIONS, struct.pack('<I', self.opts))

    def w(self, a, v):
        self.uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def r(self, a):
        return struct.unpack('<I', bytes(self.uc.mem_read(a, 4)))[0]

    def new_game(self, kind, xb0):
        g = self.game = self.z.alloc(0x100)
        self.w(g, self.vt), self.w(g + 0x98, kind), self.w(g + 0xb0, xb0)
        for head in (0x4c, 0x58):
            h = self.z.alloc(0xc)
            self.w(h, h), self.w(h + 4, h), self.w(g + head, h)
        return g

    def push(self, head, item):
        node, last = self.z.alloc(0xc), self.r(head + 4)
        self.w(node + 8, item), self.w(node, head), self.w(node + 4, last), self.w(last, node), self.w(head + 4, node)

    def spawn(self, v):
        g = self.new_game(v[0], v[1])
        if v[3]:
            r = self.z.alloc(0x38)
            self.w(r + 4, v[2]), self.w(r + 8, v[4])
            self.push(self.r(g + 0x4c), r)
        if v[5]:
            t = self.z.alloc(0x30)
            self.w(t + 4, v[4]), self.w(t + 0x18, v[6])
            self.push(self.r(g + 0x58), t)
        return [s32(self.z.call(SPAWN_BASE, ecx=g, args=(v[2],)))]

    def counts(self, v):
        return [self.z.call(BASE_COUNTS, ecx=self.new_game(v[0], v[1]), args=(v[2],)) & 0xff]

    def destroyed(self, v):
        g = self.new_game(v[0], v[1])
        self.w(NOW, v[3]), self.w(g + 0x90, v[4]), self.w(g + 0x8c, v[5]), self.w(g + 0x84, v[6])
        self.z.call(BASE_DESTROYED, args=(v[2],))
        return [s32(self.r(g + 0x90)), s32(self.r(g + 0x8c)), s32(self.r(g + 0x84))]

    def jets(self, v):
        self.w(self.opts, 0x49 | (v[2] << 7))
        if v[0]:
            g = self.new_game(0, 0)
            self.uc.mem_write(g + 0xa7, bytes([v[1]]))
            return [self.z.call(MP_JETS, ecx=g) & 0xff]
        return [self.z.call(JETS) & 0xff]


def main():
    probe = test_pm_zar.load_pm_probe()
    probe.probe_pm_game_kind.argtypes = [I32, ctypes.POINTER(I32), ctypes.POINTER(I32)]
    exe = Exe()
    rng = random.Random(89)
    parts = {1: (exe.spawn, lambda: [rng.randrange(0, 6), rng.randrange(0, 4), rng.randrange(1, 9), rng.randrange(2),
                                     rng.randrange(0, 5), int(rng.random() < 0.8), rng.randrange(0, 4)]),
             2: (exe.counts, lambda: [rng.randrange(0, 6), rng.randrange(0, 4), rng.randrange(-1, 4)]),
             3: (exe.destroyed, lambda: [rng.randrange(0, 6), rng.randrange(0, 4), rng.randrange(-1, 4),
                                         bits(rng.uniform(0, 900)), rng.randrange(0, 6), rng.randrange(0, 3),
                                         bits(rng.uniform(0, 900))]),
             4: (exe.jets, lambda: [rng.randrange(2), rng.randrange(2), rng.randrange(2)])}
    names = {1: 'spawn base', 2: 'base check', 3: 'base destroyed', 4: 'jets assist'}
    for op, (run, case) in parts.items():
        seen = {}
        for i in range(1500):
            v = case()
            theirs = run(v)
            out = (I32 * 4)()
            n = probe.probe_pm_game_kind(op, (I32 * 8)(*(v + [0] * (8 - len(v)))), out)
            mine = list(out[:n])
            if mine != theirs:
                print(f'{names[op]} case {i} {v}: exe {theirs} port {mine}', flush=True)
                os._exit(1)
            seen[theirs[0]] = seen.get(theirs[0], 0) + 1
        print(f'{names[op]}: 1500 identical; answers {dict(sorted(seen.items()))}', flush=True)
    os._exit(0)


if __name__ == '__main__':
    main()
