"""Pirate's Moon's game kinds' helpers on the network game (piece 4b of the multiplayer group, pm-tests.md) against
MultiplayerGame's through probe_pm_kind_helpers, each run as the exe's on the same lobby (players at +0x4c, teams at
+0x58, team play [0x62cc34]; the lobby's lookups 0x498800 and 0x498930 run too):

1. vt+0x58 0x436580: the objectives block's name, "mp<+0xb0>" (CString's Format modelled, its static's set-up stood
   in for);
2. vt+0x08 0x437850: the scenario's (+0xac) text id by whether +0xb0 is 1 (the message 0x596e50 stood in for);
3. 0x4368d0: a player's enemies (the others whose 0x436600 differs);
4. 0x436930: a round to each on the player's team (0x436600 the same), how many;
5. 0x437380: the most rounds won short of the most (+0xbc), teams' in team play;
6, 7. 0x4366d0, 0x4366f0: a player's rounds (+0x2c), place (+0x30);
8. 0x436600 (net_team): the player's order, in team play its team's number (+0x18; MW3 the skin, +0x10);
9. 0x436650: the team with a number.

    cmake --build build --target pmprobe && MW3_EXE=pm .venv/bin/python tests/differential/test_pm_kind_helpers.py
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
TEAM_PLAY = 0x62cc34
TBASE = 7 + 8 * 5
N_IN = TBASE + 1 + 4 * 3
FUNCS = {1: 0x436580, 2: 0x437850, 3: 0x4368d0, 4: 0x436930, 5: 0x437380, 6: 0x4366d0, 7: 0x4366f0, 8: 0x436600,
         9: 0x436650}
NAMES = {1: 'objectives key', 2: 'scenario text', 3: 'enemies', 4: "the team's round", 5: 'runner-up rounds',
         6: 'rounds of', 7: 'place of', 8: 'team number', 9: 'team by number'}


def s32(v):
    return struct.unpack('<i', struct.pack('<I', v & 0xffffffff))[0]


class Exe:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        stubs = {0x5ce8e4: (lambda c: self.w(c.ecx, z.string('')) or c.ecx, 0), 0x5cee12: (lambda c: 0, 0),
                 0x5ce8de: (self.format, 0), 0x596e50: (lambda c: c.arg(0), 0)}
        for a, (fn, pop) in stubs.items():
            z.native_stub(a, fn, pop)
        self.vt = z.alloc(0x100)
        self.uc.mem_write(self.vt + 0x48, struct.pack('<I', 0x436010))

    def w(self, a, v):
        self.uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def r(self, a):
        return struct.unpack('<I', bytes(self.uc.mem_read(a, 4)))[0]

    def format(self, c):
        fmt = self.z.cstr(c.arg(1))
        assert fmt == b'mp%d', fmt
        self.w(c.arg(0), self.z.string('mp%d' % s32(c.arg(2))))
        return 0

    def push(self, head, item):
        node, last = self.z.alloc(0xc), self.r(head + 4)
        self.w(node + 8, item), self.w(node, head), self.w(node + 4, last), self.w(last, node), self.w(head + 4, node)

    def run(self, v):
        z, w = self.z, self.w
        self.uc.mem_write(TEAM_PLAY, bytes([v[1]]))
        self.uc.mem_write(0x62951c, b'\0')  # the key's static CString made again
        g = z.alloc(0x100)
        w(g, self.vt), w(g + 0xb0, v[3]), w(g + 0xac, v[4]), w(g + 0xbc, v[5])
        for head in (0x4c, 0x58):
            h = z.alloc(0xc)
            w(h, h), w(h + 4, h), w(g + head, h)
        players = []
        for k in range(v[6]):
            pid, team, order, rounds, place = v[7 + 5 * k:12 + 5 * k]
            r = z.alloc(0x38)
            w(r + 4, pid), w(r + 8, team), w(r + 0x20, order), w(r + 0x2c, rounds), w(r + 0x30, place)
            self.push(self.r(g + 0x4c), r)
            players.append(r)
        for k in range(v[TBASE]):
            tid, number, rounds = v[TBASE + 1 + 3 * k:TBASE + 4 + 3 * k]
            t = z.alloc(0x30)
            w(t + 4, tid), w(t + 0x18, number), w(t + 0x20, rounds)
            self.push(self.r(g + 0x58), t)
        op = v[0]
        if op in (1, 2, 5):
            ret = z.call(FUNCS[op], ecx=g)
        else:
            ret = z.call(FUNCS[op], ecx=g, args=(v[2],))
        if op == 1:
            return list(z.cstr(ret))
        if op == 4:
            return [s32(ret)] + [s32(self.r(r + 0x2c)) for r in players]
        if op == 9:
            return [s32(self.r(ret + 4)) if ret else 0]
        return [s32(ret)]


def case(rng, op):
    v = [0] * N_IN
    v[0], v[1] = op, rng.randrange(2)
    v[3], v[4], v[5] = rng.randrange(-1, 4), rng.randrange(-1, 6), rng.choice([0, 1, 2, 3])
    v[6] = rng.randrange(0, 9)
    tids = rng.sample(range(1, 40), 4)
    ids = rng.sample(range(1, 30), 8)
    for k in range(v[6]):
        v[7 + 5 * k:12 + 5 * k] = [ids[k], rng.choice(tids + [0, 99]), rng.randrange(0, 4), rng.randrange(0, 4),
                                   rng.randrange(-1, 4)]
    v[TBASE] = rng.randrange(0, 5)
    for k in range(v[TBASE]):
        v[TBASE + 1 + 3 * k:TBASE + 4 + 3 * k] = [tids[k], rng.randrange(0, 8), rng.randrange(0, 4)]
    v[2] = rng.randrange(0, 8) if op == 9 else rng.choice(ids[:max(1, v[6])] + [77])
    return v


def main():
    probe = test_pm_zar.load_pm_probe()
    probe.probe_pm_kind_helpers.argtypes = [ctypes.POINTER(I32), ctypes.POINTER(I32)]
    exe = Exe()
    rng = random.Random(97)
    for op in FUNCS:
        seen = {}
        for i in range(800):
            v = case(rng, op)
            theirs = exe.run(v)
            out = (I32 * 64)()
            n = probe.probe_pm_kind_helpers((I32 * N_IN)(*v), out)
            mine = list(out[:n])
            if mine != theirs:
                print(f'{NAMES[op]} case {i} (team play {v[1]}, asked {v[2]}): exe {theirs} port {mine}', flush=True)
                os._exit(1)
            seen[theirs[0]] = seen.get(theirs[0], 0) + 1
        print(f'{NAMES[op]}: 800 identical; first words {dict(sorted(seen.items())[:8])}', flush=True)
    os._exit(0)


if __name__ == '__main__':
    main()
