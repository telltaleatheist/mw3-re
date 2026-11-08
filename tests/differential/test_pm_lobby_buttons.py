"""Pirate's Moon's lobby buttons (piece 6d of the multiplayer group, pm-tests.md) against lobby_buttons,
lobby_team_chosen and lobby_player_chosen through probe_pm_lobby_buttons, on a lobby screen laid out as the exe's: each
button an object whose vt+0x80 applies its +0xc4 (logged), TP_LIST and PLAYERS_LIST with their items and choice
(0x48bdf0, the choice set, logged), GAME_TYPE's items and pick read by the exe's own 0x4395e0 and 0x438800, the
screen's flags (+0x16799 locked, +0x1679d between rounds, +0x166c0 READY, +0x1679a everyone ready), the game's teams
(+0x5c) and team play ([0x62cc34]); the host (0x5a8a40), our id (0x5a8a30), our team (0x498800), our skin (0x498830)
and whether we are in a team (0x498860) stood in for.

1. The refresh's buttons 0x49ca00: READY, MECHLAB, SALVAGE, OPTIONS, START, NEW_TEAM, and with READY or between
   rounds the lists' choices and their buttons.
2. A TP_LIST choice 0x49dc50 (a team, a player, none): JOIN, REMOVE, KICK, PICK_SKIN, the choice undone when ready,
   the team kept (+0x167b0).
3. A PLAYERS_LIST choice 0x49dfd0: KICK2, PICK_SKIN, the choice undone.

    cmake --build build --target pmprobe && MW3_EXE=pm .venv/bin/python tests/differential/test_pm_lobby_buttons.py
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
REFRESH, TEAM_CHOSEN, PLAYER_CHOSEN = 0x49ca00, 0x49dc50, 0x49dfd0
TEAM_PLAY = 0x62cc34
BUTTONS = [0x16570, 0x12188, 0x12304, 0x12480, 0x1200c, 0x15104, 0x1525c, 0x14cfc, 0x14e54, 0x14fac, 0x153b4]
TP, PLAYERS, GAME_TYPE = 0x15eb8, 0x160bc, 0x12778
N_IN = 15


class Exe:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        v = lambda i: (lambda c: self.v[i])  # noqa: E731
        stubs = {0x5a8a40: (v(0), 0), 0x5a8a30: (v(6), 0), 0x498800: (v(7), 4), 0x498830: (v(8), 4),
                 0x498860: (v(14), 8), 0x48bdf0: (self.choose, 4)}
        for a, (fn, pop) in stubs.items():
            z.native_stub(a, fn, pop)
        self.vt = z.alloc(0x100)
        f = z.cpu.code(b'\xc3')
        z.native_stub(f, self.applied)
        self.uc.mem_write(self.vt + 0x80, struct.pack('<I', f))
        self.kind_vt = {}
        for kind in (1, 2, 3):
            vt, f = z.alloc(0x10), z.cpu.code(b'\xc3')
            z.native_stub(f, (lambda k: lambda c: k)(kind))
            self.uc.mem_write(vt, struct.pack('<I', f))
            self.kind_vt[kind] = vt
        self.sc, self.g = z.alloc(0x16800), z.alloc(0x100)
        self.entry, self.item, self.arr = z.alloc(0x10), z.alloc(8), z.alloc(4 * 4)
        self.kinds, self.kind_items = z.alloc(4 * 8), [z.alloc(8) for _ in range(5)]

    def w(self, a, v):
        self.uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def r(self, a):
        return struct.unpack('<I', bytes(self.uc.mem_read(a, 4)))[0]

    def applied(self, c):
        self.set[c.ecx - self.sc] = struct.unpack('<i', bytes(self.uc.mem_read(c.ecx + 0xc4, 4)))[0]
        return 0

    def choose(self, c):
        self.chose[c.ecx - self.sc] = struct.unpack('<i', struct.pack('<I', c.arg(0)))[0]
        return 0

    def run(self, op, v):
        self.v, self.set, self.chose = v, {}, {}
        w, sc, g = self.w, self.sc, self.g
        self.uc.mem_write(sc, bytes(0x16800)), self.uc.mem_write(g, bytes(0x100))
        w(sc + 0x11cf0, g), w(g + 0x5c, v[9])
        for off in BUTTONS:
            w(sc + off, self.vt)
        self.uc.mem_write(TEAM_PLAY, bytes([v[1]]))
        self.uc.mem_write(sc + 0x16799, bytes([v[2]])), self.uc.mem_write(sc + 0x1679d, bytes([v[3]]))
        w(sc + 0x166c0, v[4]), self.uc.mem_write(sc + 0x1679a, bytes([v[5]]))
        # GAME_TYPE: the kinds 0, 1, 3, 4, picked or not
        for i, k in enumerate((0, 1, 3, 4)):
            w(self.kind_items[i] + 4, k), w(self.kinds + 4 * i, self.kind_items[i])
        w(sc + GAME_TYPE + 0x15c, self.kinds), w(sc + GAME_TYPE + 0x160, self.kinds + 16)
        w(sc + GAME_TYPE + 0x16c, [0, 1, 3, 4].index(v[11]) if v[10] else -1)
        # the chosen list's one entry
        lst = TP if op == 1 else PLAYERS
        if v[12]:
            w(self.entry, self.kind_vt[v[12]]), w(self.entry + 4, v[13]), w(self.item + 4, self.entry)
            w(self.arr, self.item)
            w(sc + lst + 0x15c, self.arr), w(sc + lst + 0x160, self.arr + 4), w(sc + lst + 0x17c, 0)
        else:
            w(sc + lst + 0x17c, -1)
        self.z.call((REFRESH, TEAM_CHOSEN, PLAYER_CHOSEN)[op], ecx=sc)
        out = [self.set.get(off, -1) for off in BUTTONS]
        out += [1 if self.chose.get(TP) == -1 else 0, 1 if self.chose.get(PLAYERS) == -1 else 0,
                1 if self.r(sc + 0x167b0) == self.entry and v[12] else 0]
        return out


def case(rng):
    v = [0] * N_IN
    v[0:6] = [rng.randrange(2) for _ in range(6)]
    v[6] = rng.randrange(1, 9)
    v[7] = rng.choice([0, 0, 100 + rng.randrange(4)])
    v[8] = rng.choice([0, rng.randrange(1, 9)])
    v[9] = rng.randrange(0, 10)
    v[10], v[11] = rng.randrange(2), rng.choice([0, 1, 3, 4])
    v[12] = rng.choice([0, 1, 1, 2, 2, 3])
    v[13] = rng.choice([v[6], rng.randrange(1, 9), 100 + rng.randrange(4)])
    v[14] = rng.randrange(2)
    return v


def main():
    probe = test_pm_zar.load_pm_probe()
    probe.probe_pm_lobby_buttons.argtypes = [I32, ctypes.POINTER(I32), ctypes.POINTER(I32)]
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    exe = Exe()
    rng = random.Random(0x49ca00)
    for op, name in ((0, 'refresh'), (1, 'TP_LIST choice'), (2, 'PLAYERS_LIST choice')):
        seen = set()
        for i in range(count):
            v = case(rng)
            theirs = exe.run(op, v)
            out = (I32 * 14)()
            probe.probe_pm_lobby_buttons(op, (I32 * N_IN)(*v), out)
            mine = list(out)
            if mine != theirs:
                print(f'{name} case {i} {v}:\n  exe  {theirs}\n  port {mine}', flush=True)
                os._exit(1)
            seen.add(tuple(theirs))
        print(f'{name}: {count} identical, {len(seen)} distinct outcomes', flush=True)
    os._exit(0)


if __name__ == '__main__':
    main()
