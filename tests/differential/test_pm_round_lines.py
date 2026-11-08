"""Pirate's Moon's end of a round on the lobby screen, 0x49b730, and its score lines 0x436db0 (MW3's 0x435b60), against
MultiplayerGame::round_lines and score_texts through probe_pm_round_lines (piece 3e of the multiplayer group,
pm-tests.md). The results list's lines: before any round ([0x62cc38] 0) message 0x2bd; else "End of Round <played> of
<total>" (the game's +0x74, or the screen's saved +0x167a4 once it says the game is over, +0x167a0), " Rounds extended
due to tie." (+0x1679f), " Game over: No one else can win.", then every score line in the lines' order after their sort.
A score line: the name cut to 21 (in team play indented), a space, then the rounds won (the team's in team play), the
score and the kills, each cut to 7 (MW3: score, kills, suicides); a team's "<name> (Rounds Won: <n>)"; the most rounds
won and how many have it (+0xbc, +0xc0). 0x436db0 and its sort 0x438ac0 run as the exe's, MFC's CString modelled
(Format, Left, +, +=, = both ways); the list's calls (0x438800, 0x49bb50, 0x459110, the screen's vt+0xac) and the message stood in
for. The Gaming Zone's report ([0x62cc2d] off) isn't ported (FIX (A)).

    cmake --build build --target pmprobe && MW3_EXE=pm .venv/bin/python tests/differential/test_pm_round_lines.py
"""
import ctypes
import os
import random
import re
import struct
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
os.environ['MW3_EXE'] = 'pm'
import test_pm_zar  # noqa: E402

I32 = ctypes.c_int32
END_ROUND, TEAM_PLAY, PLAYED, ZONE = 0x49b730, 0x62cc34, 0x62cc38, 0x62cc2d
TBASE = 7 + 8 * 8
N_IN = TBASE + 1 + 4 * 4 + 12


def s32(v):
    return struct.unpack('<i', struct.pack('<I', v & 0xffffffff))[0]


class Exe:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        stubs = {0x5ce8e4: (self.cs_new, 0), 0x5ce8d8: (lambda c: 0, 0), 0x5ce8de: (self.cs_format, 0),
                 0x5ceb2a: (self.cs_left, 8), 0x5ceb1e: (self.cs_addeq, 4), 0x5ceb24: (self.cs_plus, 12),
                 0x5ceaac: (self.cs_assign, 4), 0x5cea10: (self.cs_assign_str, 4), 0x450a60: (lambda c: 0, 0), 0x450a20: (lambda c: 0, 0),
                 0x438800: (lambda c: 0, 0), 0x5ce8c6: (lambda c: z.alloc(c.arg(0)), 0),
                 0x49bb50: (self.item, 8), 0x459110: (self.append, 8), 0x596e50: (self.message, 0)}
        for a, (fn, pop) in stubs.items():
            z.native_stub(a, fn, pop)
        self.refresh = z.cpu.code(b'\xc3')
        z.native_stub(self.refresh, lambda c: 0, 0)
        self.svt = z.alloc(0x100)
        self.uc.mem_write(self.svt + 0xac, struct.pack('<I', self.refresh))
        self.screen = z.alloc(0x16800)

    def r(self, a):
        return struct.unpack('<I', bytes(self.uc.mem_read(a, 4)))[0]

    def w(self, a, v):
        self.uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    # MFC's CString: a pointer to its text
    def get(self, cs):
        p = self.r(cs)
        return self.z.cstr(p) if p else b''

    def put(self, cs, b):
        self.w(cs, self.z.string(b.decode('latin1')))

    def cs_new(self, c):
        self.put(c.ecx, b'')
        return c.ecx

    def cs_format(self, c):
        out, k = b'', 2
        for part in re.split(rb'(%[ds])', self.z.cstr(c.arg(1))):
            if part == b'%d':
                out += str(s32(c.arg(k))).encode()
                k += 1
            elif part == b'%s':
                out += self.z.cstr(c.arg(k))
                k += 1
            else:
                out += part
        self.put(c.arg(0), out)
        return 0

    def cs_left(self, c):
        self.put(c.arg(0), self.get(c.ecx)[:max(0, s32(c.arg(1)))])
        return c.arg(0)

    def cs_addeq(self, c):
        self.put(c.ecx, self.get(c.ecx) + self.get(c.arg(0)))
        return c.ecx

    def cs_plus(self, c):
        self.put(c.arg(0), self.get(c.arg(1)) + self.z.cstr(c.arg(2)))
        return c.arg(0)

    def cs_assign(self, c):
        self.put(c.ecx, self.get(c.arg(0)))
        return c.ecx

    def cs_assign_str(self, c):
        self.put(c.ecx, self.z.cstr(c.arg(0)))
        return c.ecx

    # the results list
    def item(self, c):
        self.w(c.ecx, c.arg(0))
        return c.ecx

    def append(self, c):
        it = self.r(c.arg(1))
        self.out += list(self.z.cstr(self.r(it))) + [0]
        return 0

    def message(self, c):
        assert c.arg(0) == 0x2bd
        return self.z.string('M2BD')

    def chain(self, head, items):
        prev = head
        for item in items:
            node = self.z.alloc(0xc)
            self.w(node + 8, item), self.w(prev, node), self.w(node + 4, prev)
            prev = node
        self.w(prev, head), self.w(head + 4, prev)

    def run(self, v):
        z, w, sc = self.z, self.w, self.screen
        self.out = []
        self.uc.mem_write(TEAM_PLAY, bytes([v[0]]))
        self.uc.mem_write(ZONE, b'\0')
        w(PLAYED, v[1])
        w(sc + 0x15cb4, self.svt)
        self.uc.mem_write(sc + 0x1679f, bytes([v[2], v[3]]))
        w(sc + 0x167a4, v[4])
        g = z.alloc(0x100)
        w(sc + 0x11cf0, g), w(g + 0x74, v[5])
        lines = [0] * 12
        players, teams = [], []
        for k in range(v[6]):
            pid, team, order, kills, deaths, rounds, score, key = v[7 + 8 * k:15 + 8 * k]
            r = z.alloc(0x38)
            w(r + 4, pid), w(r + 8, team), w(r + 0x10, z.string(f'P{pid}')), w(r + 0x20, order), w(r + 0x24, kills)
            w(r + 0x28, deaths), w(r + 0x2c, rounds)
            ln = lines[k] = z.alloc(0x18)
            w(ln, z.string('')), w(ln + 4, score), w(ln + 0x10, key), w(ln + 0x14, 1)
            w(r + 0x34, ln)
            players.append(r)
        for k in range(v[TBASE]):
            tid, rounds, score, key = v[TBASE + 1 + 4 * k:TBASE + 5 + 4 * k]
            t = z.alloc(0x30)
            w(t + 4, tid), w(t + 8, z.string(f'T{tid}')), w(t + 0x20, rounds)
            ln = lines[8 + k] = z.alloc(0x18)
            w(ln, z.string('')), w(ln + 4, score), w(ln + 0x10, key), w(ln + 0x14, 2)
            w(t + 0x2c, ln)
            teams.append(t)
        order = [lines[i] for i in v[TBASE + 17:TBASE + 29] if i >= 0]
        vec = z.alloc(4 * 12 + 4)
        for i, a in enumerate(order):
            w(vec + 4 * i, a)
        w(g + 0x3c, vec), w(g + 0x40, vec + 4 * len(order)), w(g + 0x44, vec + 4 * 12)
        ph, th = z.alloc(0xc), z.alloc(0xc)
        self.chain(ph, players), self.chain(th, teams)
        w(g + 0x4c, ph), w(g + 0x58, th)
        z.call(END_ROUND, ecx=sc, args=(0, 0))
        return self.out + [-1, s32(self.r(g + 0xbc)), s32(self.r(g + 0xc0))]


def case(rng):
    v = [0] * N_IN
    v[0] = rng.randrange(2)
    v[1] = rng.choice([0, 1, 2, 3, 5])
    v[2], v[3] = rng.randrange(2), rng.randrange(2)
    v[4], v[5] = rng.randrange(0, 9), rng.randrange(0, 9)
    v[6] = rng.randrange(0, 9)
    tids = rng.sample(range(1, 40), 4)
    keys = rng.sample(range(0, 5000), 12)
    for k in range(v[6]):
        v[7 + 8 * k:15 + 8 * k] = [rng.choice([1, 7, 12, 123456789]) * 0 + rng.randrange(1, 99999), rng.choice(tids + [0]),
                                   rng.randrange(0, 9), rng.choice([0, 3, 12, 1234567, -2]), rng.randrange(0, 5),
                                   rng.choice([0, 1, 2, 15]), rng.choice([0, 4, 99, 12345678]), keys[k]]
    v[TBASE] = rng.randrange(0, 5)
    for k in range(v[TBASE]):
        v[TBASE + 1 + 4 * k:TBASE + 5 + 4 * k] = [tids[k], rng.choice([0, 1, 3, 15]), rng.randrange(0, 50), keys[8 + k]]
    present = list(range(v[6])) + [8 + k for k in range(v[TBASE])]
    rng.shuffle(present)
    v[TBASE + 17:TBASE + 29] = present + [-1] * (12 - len(present))
    return v


def main():
    probe = test_pm_zar.load_pm_probe()
    probe.probe_pm_round_lines.argtypes = [ctypes.POINTER(I32), ctypes.POINTER(I32), I32]
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 1500
    exe = Exe()
    rng = random.Random(83)
    seen = {}
    sample = None
    for i in range(count):
        v = case(rng)
        theirs = exe.run(v)
        out = (I32 * 8192)()
        n = probe.probe_pm_round_lines((I32 * N_IN)(*v), out, 8192)
        mine = list(out[:n])
        if mine != theirs:
            j = next((k for k, (a, b) in enumerate(zip(mine, theirs)) if a != b), min(len(mine), len(theirs)))
            text = lambda x: bytes(c for c in x if 0 < c < 256).decode('latin1')  # noqa: E731
            print(f'case {i} (team play {v[0]}, played {v[1]}, extended {v[2]}, over {v[3]}) at {j}:\n'
                  f'  exe  {text(theirs[max(0, j - 40):j + 40])!r}\n  port {text(mine[max(0, j - 40):j + 40])!r}\n'
                  f'  tails exe {theirs[-3:]} port {mine[-3:]}', flush=True)
            os._exit(1)
        if v[1] and v[6] > 1 and sample is None:
            sample = bytes(c if c else 10 for c in theirs[:theirs.index(-1)]).decode('latin1')
        key = (v[0], v[1] > 0)
        seen[key] = seen.get(key, 0) + 1
    print(f'{count} end-of-round lists identical; (team play, a round played): {dict(sorted(seen.items()))}\n'
          f'e.g.\n{sample}', flush=True)
    os._exit(0)


if __name__ == '__main__':
    main()
