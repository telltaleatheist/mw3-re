"""Pirate's Moon's lobby on entering, 0x49a3c0 (MW3's 0x497720; piece 6b of the multiplayer group, pm-tests.md), its
rounds against MultiplayerGame::enter_rounds through probe_pm_enter_rounds. The exe runs on a lobby screen and its game
(players and teams with their rounds and places, the score lines) up to the local player's data (0x5aa4a0), where the
test stops it: the scores 0x436db0 and the runner-up 0x437380 run as the exe's (MFC's CString modelled), the results
list 0x49b730 (test_pm_round_lines), the next round's rules 0x438580 (test_pm_rounds) and the lists' refresh 0x49bb70
(test_pm_lobby_lines) stood in for, the labels and messages too.

Compared: the match extended on a tie (+0x1679f, the rounds +0x74 one more), over before its end (+0x167a0, the total
kept in +0x167a4, the rounds cut to those played), the next round or not (the screen's +0x1679d), the rounds played
after ([0x62cc38], cleared at the end), team play after ([0x62cc34], off at the end), the local player's data words 4
and 5 (its rounds won and place, 0 at the end), the most rounds won and how many (+0xbc, +0xc0), and for the host in
team play each team's end: gone (0x5a9480 its players, 0x5a94f0 it) or its rounds into its data's fourth word (0x5aa550),
and between rounds the next round's rules asked (0x438580 with our place and the rounds played less one).

    cmake --build build --target pmprobe && MW3_EXE=pm .venv/bin/python tests/differential/test_pm_lobby_enter.py
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
ENTER, TEAM_PLAY, PLAYED = 0x49a3c0, 0x62cc34, 0x62cc38
PB = 7 + 8 * 6
N_IN = PB + 1 + 4 * 3


def s32(v):
    return struct.unpack('<i', struct.pack('<I', v & 0xffffffff))[0]


class Stop(Exception):
    pass


class Exe:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        cs = {0x5ce8e4: (self.cs_new, 0), 0x5ce8d8: (lambda c: 0, 0), 0x5ce8de: (self.cs_format, 0),
              0x5ceaac: (self.cs_assign, 4), 0x5cea10: (self.cs_assign_str, 4), 0x5ceabe: (self.cs_from, 4),
              0x5ceb2a: (self.cs_left, 8), 0x5ceb1e: (self.cs_addeq, 4), 0x5ceb24: (self.cs_plus_cs, 12)}
        for a, (fn, pop) in cs.items():
            z.native_stub(a, fn, pop)
        stubs = {0x5a8a30: (lambda c: self.v[2], 0), 0x5a8a40: (lambda c: self.v[3], 0),
                 0x5a9960: (lambda c: z.string('me'), 0), 0x5aac90: (lambda c: z.string('game'), 0),
                 0x497f70: (lambda c: 0, 4), 0x596e50: (lambda c: z.string('M%x' % c.arg(0)), 0),
                 0x49b730: (self.results, 4), 0x438580: (self.next_round, 8), 0x49bb70: (lambda c: 0, 0),
                 0x49b3c0: (lambda c: 0, 0), 0x49f570: (lambda c: 0, 8), 0x596520: (lambda c: z.alloc(c.arg(1)), 0),
                 0x5a9480: (self.leave, 0), 0x5a94f0: (self.gone, 0), 0x5aa590: (lambda c: 1, 0),
                 0x5aa550: (self.team_data, 0), 0x5aa4a0: (self.data, 0)}
        for a, (fn, pop) in stubs.items():
            z.native_stub(a, fn, pop)
        self.vt = z.alloc(0x200)
        f = z.cpu.code(b'\xc3')
        z.native_stub(f, lambda c: 0, 0)
        self.uc.mem_write(self.vt + 0xac, struct.pack('<I', f))
        self.sc, self.g = z.alloc(0x16800), z.alloc(0x100)

    def w(self, a, v):
        self.uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def r(self, a):
        return struct.unpack('<I', bytes(self.uc.mem_read(a, 4)))[0]

    # MFC's CString: a pointer to its text
    def get(self, cs):
        p = self.r(cs)
        return self.z.cstr(p) if p else b''

    def put(self, cs, b):
        self.w(cs, self.z.string(b.decode('latin1')))

    def cs_new(self, c):
        self.put(c.ecx, b'')
        return c.ecx

    def cs_from(self, c):
        self.put(c.ecx, self.z.cstr(c.arg(0)) if c.arg(0) else b'')
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

    def cs_assign(self, c):
        self.put(c.ecx, self.get(c.arg(0)))
        return c.ecx

    def cs_assign_str(self, c):
        self.put(c.ecx, self.z.cstr(c.arg(0)) if c.arg(0) else b'')
        return c.ecx

    def cs_left(self, c):
        self.put(c.arg(0), self.get(c.ecx)[:max(0, s32(c.arg(1)))])
        return c.arg(0)

    def cs_addeq(self, c):
        self.put(c.ecx, self.get(c.ecx) + self.get(c.arg(0)))
        return c.ecx

    def cs_plus_cs(self, c):
        self.put(c.arg(0), self.get(c.arg(1)) + self.z.cstr(c.arg(2)))
        return c.arg(0)

    # the stand-ins
    def results(self, c):
        return 0

    def next_round(self, c):
        self.ends += [7, s32(c.arg(0)), s32(c.arg(1))]
        return 0

    def leave(self, c):
        return 0

    def gone(self, c):
        self.ends += [self.team_ids[c.arg(0)], 0]
        return 0

    def team_data(self, c):
        self.ends += [self.team_ids[c.arg(0)], 1, s32(self.r(c.arg(1) + 12))]
        return 1

    def data(self, c):
        self.words = [s32(self.r(c.arg(1) + 4 * k)) for k in range(7)]
        self.uc.emu_stop()
        return 0

    def chain(self, head, items):
        prev = head
        for item in items:
            node = self.z.alloc(0xc)
            self.w(node + 8, item), self.w(prev, node), self.w(node + 4, prev)
            prev = node
        self.w(prev, head), self.w(head + 4, prev)

    def run(self, v):
        z, w, sc, g = self.z, self.w, self.sc, self.g
        self.v, self.ends, self.words = v, [], None
        self.uc.mem_write(sc, bytes(0x16800)), self.uc.mem_write(g, bytes(0x100))
        self.uc.mem_write(TEAM_PLAY, bytes([v[4]]))
        w(PLAYED, v[1])
        w(sc + 0x11cf0, g), w(g + 0x74, v[5])
        w(sc + 0x15eb8, self.vt)
        arr = z.alloc(8)
        w(sc + 0x16014, arr), w(sc + 0x16018, arr), w(sc + 0x1601c, arr + 8)
        lines, players, teams = [], [], []
        self.team_ids = {}
        for k in range(v[6]):
            pid, team, kills, deaths, rounds, place = v[7 + 6 * k:13 + 6 * k]
            r = z.alloc(0x38)
            w(r + 4, pid), w(r + 8, team), w(r + 0x10, z.string(f'P{pid}')), w(r + 0x24, kills), w(r + 0x28, deaths)
            w(r + 0x2c, rounds), w(r + 0x30, place)
            ln = z.alloc(0x18)
            w(ln, z.string('')), w(ln + 0x14, 1), w(r + 0x34, ln)
            players.append(r), lines.append(ln)
        for k in range(v[PB]):
            tid, rounds, number = v[PB + 1 + 3 * k:PB + 4 + 3 * k]
            t = z.alloc(0x30)
            w(t + 4, tid), w(t + 8, z.string(f'T{tid}')), w(t + 0x18, number), w(t + 0x20, rounds)
            ln = z.alloc(0x18)
            w(ln, z.string('')), w(ln + 0x14, 2), w(t + 0x2c, ln)
            teams.append(t), lines.append(ln)
            self.team_ids[tid] = tid
        vec = z.alloc(4 * 12 + 4)
        for i, a in enumerate(lines):
            w(vec + 4 * i, a)
        w(g + 0x3c, vec), w(g + 0x40, vec + 4 * len(lines)), w(g + 0x44, vec + 4 * 12)
        ph, th = z.alloc(0xc), z.alloc(0xc)
        self.chain(ph, players), self.chain(th, teams)
        w(g + 0x4c, ph), w(g + 0x58, th), w(g + 0x50, len(players))
        try:
            z.call(ENTER, ecx=sc, args=(v[0],))
        except RuntimeError:  # stopped at the data (the rest is the screen's)
            pass
        assert self.words is not None, 'the data was never sent'
        rd = lambda a: self.uc.mem_read(a, 1)[0]  # noqa: E731
        return [rd(sc + 0x1679f), rd(sc + 0x167a0), s32(self.r(sc + 0x167a4)), rd(sc + 0x1679d), s32(self.r(g + 0x74)),
                s32(self.r(PLAYED)), rd(TEAM_PLAY), self.words[5], self.words[4], s32(self.r(g + 0xbc)),
                s32(self.r(g + 0xc0))] + self.ends


def case(rng):
    v = [0] * N_IN
    v[0], v[1] = rng.randrange(2), rng.choice([0, 1, 2, 3, 4, 5])
    v[3], v[4], v[5] = rng.randrange(2), rng.randrange(2), rng.choice([1, 2, 3, 5, 8])
    v[6] = rng.randrange(1, 9)
    ids = rng.sample(range(1, 99), 8)
    tids = rng.sample(range(100, 140), 4)
    v[2] = rng.choice(ids[:v[6]] + [500])
    for k in range(v[6]):
        v[7 + 6 * k:13 + 6 * k] = [ids[k], rng.choice(tids + [0]), rng.randrange(0, 6), rng.randrange(0, 3),
                                   rng.choice([0, 0, 1, 2, 3, 4]), rng.randrange(0, 4)]
    v[PB] = rng.randrange(0, 5)
    for k in range(v[PB]):
        v[PB + 1 + 3 * k:PB + 4 + 3 * k] = [tids[k], rng.choice([0, 1, 2, 3, 4]), rng.randrange(0, 8)]
    return v


def main():
    probe = test_pm_zar.load_pm_probe()
    probe.probe_pm_enter_rounds.argtypes = [ctypes.POINTER(I32), ctypes.POINTER(I32), I32]
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 2000
    exe = Exe()
    rng = random.Random(107)
    seen = {}
    for i in range(count):
        v = case(rng)
        theirs = exe.run(v)
        out = (I32 * 64)()
        n = probe.probe_pm_enter_rounds((I32 * N_IN)(*v), out, 64)
        mine = list(out[:n])
        if mine != theirs:
            print(f'case {i} (from a game {v[0]}, played {v[1]}, host {v[3]}, team play {v[4]}, rounds {v[5]}):\n'
                  f'  exe  {theirs}\n  port {mine}', flush=True)
            os._exit(1)
        key = ('extended' if theirs[0] else 'over' if theirs[1] else 'next' if theirs[3] else 'end')
        seen[key] = seen.get(key, 0) + 1
    print(f'{count} lobby entries identical; {dict(sorted(seen.items()))}', flush=True)
    os._exit(0)


if __name__ == '__main__':
    main()
