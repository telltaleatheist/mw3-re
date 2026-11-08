"""Pirate's Moon's round tally, 0x436980 on the network game (piece 3c of the multiplayer group, pm-tests.md), against
MultiplayerGame::tally through probe_pm_tally; the round winner 0x437970 is +0xaa or the tally not awarding.

Run as the exe's: the rescore 0x4374f0, the lines' sort 0x438820 (std::sort by key, largest first), the ranks 0x4373e0
(0 first; keys equal but for their low 4 bits share one; then each player's place +0x30, each team's +0x24), the side's
round 0x436d50 (the teams whose +0x18 bit 0 is the side, and their players' +0x2c), the game's vt+0x48 0x436010 (+0xb0).
A base destroyed (+0x8c 1 or 2, kinds +0x98 over 1) wins for the side vt+0x48 names for it; kind 3 timed out (+0x90 3)
with none lost, side 0; otherwise the lines are rescored without the rounds, sorted and ranked, and the first place
(players not gone, or teams in team play [0x62cc34]) wins when alone, all on it getting the round. Awarding ends with
the rescore with the rounds, sorted and ranked. Compared: the answer, every record's rounds and place, every line's
score, key and rank, the teams' kills.

    cmake --build build --target pmprobe && MW3_EXE=pm .venv/bin/python tests/differential/test_pm_tally.py
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
TALLY, WINNER, TEAM_PLAY, VT48 = 0x436980, 0x437970, 0x62cc34, 0x436010
TBASE = 7 + 8 * 8
N_IN = TBASE + 1 + 4 * 4 + 12


def s32(v):
    return struct.unpack('<i', struct.pack('<I', v & 0xffffffff))[0]


class Exe:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        for a in (0x5ce8e4, 0x5ce8d8):  # the rescore's CStrings: unused
            z.native_stub(a, lambda c: c.ecx, 0)
        self.vt = z.alloc(0x100)
        self.uc.mem_write(self.vt + 0x48, struct.pack('<I', VT48))

    def w(self, a, v):
        self.uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def r(self, a):
        return s32(struct.unpack('<I', bytes(self.uc.mem_read(a, 4)))[0])

    def chain(self, head, items):
        prev = head
        for item in items:
            node = self.z.alloc(0xc)
            self.w(node + 8, item), self.w(prev, node), self.w(node + 4, prev)
            prev = node
        self.w(prev, head), self.w(head + 4, prev)

    def line(self, kind):
        a = self.z.alloc(0x18)
        self.w(a + 4, -7), self.w(a + 0xc, -9), self.w(a + 0x10, 0xdead), self.w(a + 0x14, kind)
        return a

    def run(self, v, winner=False):
        z, w = self.z, self.w
        self.uc.mem_write(TEAM_PLAY, bytes([v[0]]))
        g = z.alloc(0x100)
        w(g, self.vt), w(g + 0x98, v[2]), w(g + 0x90, v[3]), w(g + 0x8c, v[4]), w(g + 0xb0, v[5])
        lines = [0] * 12
        players, teams = [], []
        for k in range(v[6]):
            pid, team, order, kills, deaths, rounds, left, place = v[7 + 8 * k:15 + 8 * k]
            r = z.alloc(0x38)
            w(r + 4, pid), w(r + 8, team), self.uc.mem_write(r + 0x18, bytes([left])), w(r + 0x20, order)
            w(r + 0x24, kills), w(r + 0x28, deaths), w(r + 0x2c, rounds), w(r + 0x30, place)
            lines[k] = self.line(1)
            w(r + 0x34, lines[k])
            players.append(r)
        for k in range(v[TBASE]):
            tid, word2, rounds, place = v[TBASE + 1 + 4 * k:TBASE + 5 + 4 * k]
            t = z.alloc(0x30)
            w(t + 4, tid), w(t + 0x14, -5), w(t + 0x18, word2), w(t + 0x20, rounds), w(t + 0x24, place)
            lines[8 + k] = self.line(2)
            w(t + 0x2c, lines[8 + k])
            teams.append(t)
        order = [lines[i] for i in v[TBASE + 17:TBASE + 29] if i >= 0]
        vec = z.alloc(4 * 12 + 4)
        for i, a in enumerate(order):
            w(vec + 4 * i, a)
        w(g + 0x3c, vec), w(g + 0x40, vec + 4 * len(order)), w(g + 0x44, vec + 4 * 12)
        ph, th = z.alloc(0xc), z.alloc(0xc)
        self.chain(ph, players), self.chain(th, teams)
        w(g + 0x4c, ph), w(g + 0x58, th)
        self.uc.mem_write(g + 0xaa, bytes([v[1] if winner else 0]))
        if winner:
            out = [z.call(WINNER, ecx=g) & 0xff]
        else:
            out = [z.call(TALLY, ecx=g, args=(v[1],)) & 0xff]
        for r in players:
            ln = self.r(r + 0x34)
            out += [self.r(r + 0x2c), self.r(r + 0x30), self.r(ln + 4), self.r(ln + 0x10), self.r(ln + 0xc)]
        for t in teams:
            ln = self.r(t + 0x2c)
            out += [self.r(t + 0x20), self.r(t + 0x24), self.r(ln + 4), self.r(ln + 0x10), self.r(ln + 0xc), self.r(t + 0x14)]
        return out


def case(rng):
    v = [0] * N_IN
    v[0], v[1] = rng.randrange(2), rng.randrange(2)
    v[2] = rng.choice([0, 1, 2, 3, 4])
    v[3] = rng.choice([0, 1, 3, 4, 5, 6])
    v[4] = rng.choice([0, 0, 1, 2, 3])
    v[5] = rng.choice([0, 1, 2])
    v[6] = rng.randrange(0, 9)
    tids = rng.sample(range(1, 40), 4)
    for k in range(v[6]):
        v[7 + 8 * k:15 + 8 * k] = [rng.randrange(1, 99), rng.choice(tids + [0]), rng.randrange(0, 9),
                                   rng.choice([0, 1, 2, 3, 5]), rng.choice([0, 1, 2]), rng.choice([0, 1, 2, 15]),
                                   int(rng.random() < 0.2), rng.randrange(-1, 4)]
    v[TBASE] = rng.randrange(0, 5)
    for k in range(v[TBASE]):
        v[TBASE + 1 + 4 * k:TBASE + 5 + 4 * k] = [tids[k], rng.randrange(0, 4), rng.choice([0, 1, 3, 15]), rng.randrange(-1, 3)]
    present = list(range(v[6])) + [8 + k for k in range(v[TBASE])]
    rng.shuffle(present)
    v[TBASE + 17:TBASE + 29] = present + [-1] * (12 - len(present))
    return v


def main():
    probe = test_pm_zar.load_pm_probe()
    probe.probe_pm_tally.argtypes = [ctypes.POINTER(I32), ctypes.POINTER(I32)]
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    exe = Exe()
    rng = random.Random(73)
    seen = {}
    for i in range(count):
        v = case(rng)
        theirs = exe.run(v)
        out = (I32 * 128)()
        n = probe.probe_pm_tally((I32 * N_IN)(*v), out)
        mine = list(out[:n])
        if mine != theirs:
            print(f'case {i} (team play {v[0]}, award {v[1]}, kind {v[2]}, reason {v[3]}, base lost {v[4]}, +0xb0 {v[5]}, '
                  f'players {v[6]}, teams {v[TBASE]}):\n  exe  {theirs}\n  port {mine}', flush=True)
            os._exit(1)
        key = (v[0], v[1], theirs[0])
        seen[key] = seen.get(key, 0) + 1
    print(f'{count} tallies identical; (team play, award, a winner): {dict(sorted(seen.items()))}', flush=True)
    probe.probe_pm_round_winner.argtypes = [ctypes.POINTER(I32), ctypes.POINTER(I32)]
    seen = {}
    for i in range(1000):
        v = case(rng)  # v[1] is the game's +0xaa here
        theirs = exe.run(v, winner=True)
        out = (I32 * 128)()
        n = probe.probe_pm_round_winner((I32 * N_IN)(*v), out)
        mine = list(out[:n])
        if mine != theirs:
            print(f'winner case {i} (+0xaa {v[1]}, kind {v[2]}):\n  exe  {theirs}\n  port {mine}', flush=True)
            os._exit(1)
        seen[(v[1], theirs[0])] = seen.get((v[1], theirs[0]), 0) + 1
    print(f'round winner: 1000 identical; (+0xaa, a winner): {dict(sorted(seen.items()))}', flush=True)
    os._exit(0)


if __name__ == '__main__':
    main()
