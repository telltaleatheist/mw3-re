"""Pirate's Moon's rescore, 0x4374f0 on the network game (MW3's 0x4361e0), against MultiplayerGame::rescore through
probe_pm_rescore (piece 2 of the multiplayer group, pm-tests.md). Each score is kills less suicides, not under 0; in
team play ([0x62cc34]) a team's is its players' sum and its +0x14 their kills. What PM changed is the order key: its low
bits are a player's lobby order (+0x20, MW3's skin) or a team's third data word (+0x18), and with the flag (every
caller but the tally's first pass) the rounds won, 4 bits (a player's +0x2c, a team's +0x20), sort above the score.
PM's records: a player 0x38 bytes (+0x34 its line), a team 0x30 (+0x2c its line); a line's +4 score, +0x10 key. The
exe reads every line unchecked, so every record has one here.

    cmake --build build --target pmprobe && MW3_EXE=pm .venv/bin/python tests/differential/test_pm_rescore.py
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
RESCORE, TEAM_PLAY = 0x4374f0, 0x62cc34
N_IN = 3 + 8 * 7 + 1 + 4 * 4


def s32(v):
    return struct.unpack('<i', struct.pack('<I', v & 0xffffffff))[0]


class Exe:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        for a in (0x5ce8e4, 0x5ce8d8):  # CString's ctor and dtor: unused here
            z.native_stub(a, lambda c: c.ecx, 0)

    def w(self, a, v):
        self.uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def r(self, a):
        return struct.unpack('<I', bytes(self.uc.mem_read(a, 4)))[0]

    def chain(self, head, items):
        prev = head
        for item in items:
            node = self.z.alloc(0xc)
            self.w(node + 8, item), self.w(prev, node), self.w(node + 4, prev)
            prev = node
        self.w(prev, head), self.w(head + 4, prev)

    def line(self):
        a = self.z.alloc(0x18)
        self.w(a + 4, -7), self.w(a + 0x10, 0xdead)
        return a

    def run(self, v):
        z, w = self.z, self.w
        self.uc.mem_write(TEAM_PLAY, bytes([v[0]]))
        g = z.alloc(0x100)
        players, teams = [], []
        for k in range(v[2]):
            pid, team, order, kills, deaths, rounds, _ = v[3 + 7 * k:10 + 7 * k]
            r = z.alloc(0x38)
            w(r + 4, pid), w(r + 8, team), w(r + 0x20, order), w(r + 0x24, kills), w(r + 0x28, deaths)
            w(r + 0x2c, rounds), w(r + 0x34, self.line())
            players.append(r)
        base = 3 + 8 * 7
        for k in range(v[base]):
            tid, word2, rounds, _ = v[base + 1 + 4 * k:base + 5 + 4 * k]
            t = z.alloc(0x30)
            w(t + 4, tid), w(t + 0x14, -5), w(t + 0x18, word2), w(t + 0x20, rounds), w(t + 0x2c, self.line())
            teams.append(t)
        ph, th = z.alloc(0xc), z.alloc(0xc)
        self.chain(ph, players), self.chain(th, teams)
        w(g + 0x4c, ph), w(g + 0x58, th)
        z.call(RESCORE, ecx=g, args=(v[1],))
        out = []
        for r in players:
            ln = self.r(r + 0x34)
            out += [s32(self.r(ln + 4)), s32(self.r(ln + 0x10))]
        for t in teams:
            ln = self.r(t + 0x2c)
            out += [s32(self.r(ln + 4)), s32(self.r(ln + 0x10)), s32(self.r(t + 0x14))]
        return out


def case(rng):
    v = [0] * N_IN
    v[0], v[1] = rng.randrange(2), int(rng.random() < 0.7)
    v[2] = rng.randrange(0, 9)
    tids = rng.sample(range(1, 40), 4)
    for k in range(v[2]):
        v[3 + 7 * k:10 + 7 * k] = [rng.randrange(1, 99), rng.choice(tids + [0, 77]), rng.randrange(0, 20),
                                   rng.choice([0, 1, 3, 7, 0xfff, 0x1005, 40]), rng.choice([0, 1, 2, 9, 50]),
                                   rng.choice([0, 1, 2, 5, 15, 16, 33]), 1]
    base = 3 + 8 * 7
    v[base] = rng.randrange(0, 5)
    for k in range(v[base]):
        v[base + 1 + 4 * k:base + 5 + 4 * k] = [tids[k], rng.randrange(-3, 20), rng.choice([0, 1, 4, 15, 17]), 1]
    return v


def main():
    probe = test_pm_zar.load_pm_probe()
    probe.probe_pm_rescore.argtypes = [ctypes.POINTER(I32), ctypes.POINTER(I32)]
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    exe = Exe()
    rng = random.Random(59)
    seen = {}
    for i in range(count):
        v = case(rng)
        theirs = exe.run(v)
        out = (I32 * 64)()
        probe.probe_pm_rescore((I32 * N_IN)(*v), out)
        mine = list(out[:len(theirs)])
        if mine != theirs:
            print(f'case {i} (team play {v[0]}, rounds {v[1]}, players {v[2]}, teams {v[3 + 56]}):\n'
                  f'  exe  {theirs}\n  port {mine}', flush=True)
            os._exit(1)
        key = (v[0], v[1])
        seen[key] = seen.get(key, 0) + 1
    print(f'{count} rescores identical; (team play, rounds): {dict(sorted(seen.items()))}', flush=True)
    os._exit(0)


if __name__ == '__main__':
    main()
