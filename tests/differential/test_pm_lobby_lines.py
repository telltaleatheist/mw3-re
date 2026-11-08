"""Pirate's Moon's lobby lists' lines, 0x49bb70 (MW3's 0x498710, piece 6a of the multiplayer group, pm-tests.md),
against MultiplayerGame::lobby_lines through probe_pm_lobby_lines. On a lobby screen laid out as the exe's: TP_LIST at
+0x15eb8 and PLAYERS_LIST at +0x160bc (each item {its text, strdup'd; its record}, the items at the list's +0x15c),
the kinds list at +0x12778 (its value 3 the one-base kind), the skins' names at +0x11cf0 + 4 skin. The records stand
in the game's lobby (the enumerations that make them again, 0x5aa610 and 0x5aa660, and the lists' clears stood in
for); the lines are read from the two lists after. MFC's CString modelled (Format, MakeLower, +, +=, =, the copy).

A team's line: its name, "  {skin} " lowered, in kind 3 " Attack" for side 1 else "Defend"; a player's: its name
(indented under a team), "++" the host's, " ", the state's text (1 0x2bf, 2 0x1061, 3 0x1062, 5 0x2c0), outside team
play " {skin} " lowered, ours our slot's mech's name (0x4867c0, run). Everyone ready (+0x1679a): every player at 5.

Then the player's record from its network data, 0x49c650 (LobbyPlayer::from_data through probe_pm_player_data), run as
the exe's with the data stood in for (0x5aa470, 0x5aa4e0): kind 1's state, else 4; the skin; between rounds (the
screen's +0x1679d) kind 1's words 4 and 5 as the rounds won and place. A kind 1 record too short for its state or
for those words: the exe reads past it, the port takes the defaults (FIX (bug)), so those words aren't compared.

    cmake --build build --target pmprobe && MW3_EXE=pm .venv/bin/python tests/differential/test_pm_lobby_lines.py
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
REFRESH, TEAM_PLAY = 0x49bb70, 0x62cc34
STRDUP, FREE, MALLOC = 0x5de678, 0x5de65c, 0x5de664
TP, PL, KINDS = 0x15eb8, 0x160bc, 0x12778
TB = 5 + 8 * 5
N_IN = TB + 1 + 4 * 3


def s32(v):
    return struct.unpack('<i', struct.pack('<I', v & 0xffffffff))[0]


class Exe:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        for slot, fn in ((STRDUP, lambda c: z.string(z.cstr(c.arg(0)).decode('latin1'))), (FREE, lambda c: 0),
                         (MALLOC, lambda c: z.alloc(c.arg(0)))):
            f = z.cpu.code(b'\xc3')
            self.uc.mem_write(slot, struct.pack('<I', f))
            z.native_stub(f, fn)
        cs = {0x5ce8e4: (self.cs_new, 0), 0x5ce8d8: (lambda c: 0, 0), 0x5ce8de: (self.cs_format, 0),
              0x5ceaac: (self.cs_assign, 4), 0x5cea10: (self.cs_assign_str, 4), 0x5ceabe: (self.cs_from, 4),
              0x5cea70: (self.cs_copy, 4), 0x5cecb0: (self.cs_lower, 0), 0x5ceac4: (self.cs_plus_sc, 12),
              0x5ceb24: (self.cs_plus_cs, 12), 0x5cecaa: (self.cs_plus_cc, 12), 0x5ceb1e: (self.cs_addeq, 4)}
        other = {0x498770: 0, 0x4988a0: 0, 0x498960: 0, 0x5aa610: 0, 0x5aa660: 0, 0x5aad50: 0, 0x5aaca0: 0,
                 0x5a9520: 0}
        for a, (fn, pop) in cs.items():
            z.native_stub(a, fn, pop)
        for a, pop in other.items():
            z.native_stub(a, lambda c: 0, pop)
        z.native_stub(0x5ce8c6, lambda c: z.alloc(c.arg(0)), 0)
        z.native_stub(0x5ce8c0, lambda c: 0, 0)
        z.native_stub(0x596e50, lambda c: z.string('M%x' % c.arg(0)), 0)
        z.native_stub(0x5a8a30, lambda c: self.v[2], 0)
        self.vt = z.alloc(0x200)
        for off, pop in ((0xac, 0), (0x80, 0)):
            f = z.cpu.code(b'\xc3')
            z.native_stub(f, lambda c: 0, pop)
            self.uc.mem_write(self.vt + off, struct.pack('<I', f))
        self.gvt = z.alloc(0x100)
        f = z.cpu.code(b'\xc3')
        z.native_stub(f, lambda c: self.slot, 4)
        self.uc.mem_write(self.gvt + 0x30, struct.pack('<I', f))
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

    def cs_copy(self, c):
        self.put(c.ecx, self.get(c.arg(0)))
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
        self.put(c.ecx, self.z.cstr(c.arg(0)))
        return c.ecx

    def cs_lower(self, c):
        self.put(c.ecx, self.get(c.ecx).lower())
        return 0

    def cs_plus_sc(self, c):  # operator+(LPCTSTR, const CString&)
        self.put(c.arg(0), self.z.cstr(c.arg(1)) + self.get(c.arg(2)))
        return c.arg(0)

    def cs_plus_cs(self, c):  # operator+(const CString&, LPCTSTR)
        self.put(c.arg(0), self.get(c.arg(1)) + self.z.cstr(c.arg(2)))
        return c.arg(0)

    def cs_plus_cc(self, c):  # operator+(const CString&, const CString&)
        self.put(c.arg(0), self.get(c.arg(1)) + self.get(c.arg(2)))
        return c.arg(0)

    def cs_addeq(self, c):
        self.put(c.ecx, self.get(c.ecx) + self.get(c.arg(0)))
        return c.ecx

    def chain(self, head, items):
        prev = head
        for item in items:
            node = self.z.alloc(0xc)
            self.w(node + 8, item), self.w(prev, node), self.w(node + 4, prev)
            prev = node
        self.w(prev, head), self.w(head + 4, prev)

    def items(self, base):
        out = []
        a, b = self.r(self.sc + base + 0x15c), self.r(self.sc + base + 0x160)
        for p in range(a, b, 4):
            it = self.r(p)
            text, rec = self.r(it), self.r(it + 4)
            ident = s32(self.r(rec + 4)) + (1000 if rec in self.teams else 0)
            out += list(self.z.cstr(text)) + [0, ident]
        return out + [-1]

    def run(self, v):
        z, w, sc, g = self.z, self.w, self.sc, self.g
        self.v = v
        self.uc.mem_write(sc, bytes(0x16800)), self.uc.mem_write(g, bytes(0x100))
        self.uc.mem_write(TEAM_PLAY, bytes([v[0]]))
        w(sc + 0x11cf0, g), w(g, self.gvt)
        for skin in range(1, 9):
            w(sc + 0x11cf0 + 4 * skin, z.string(f'S{skin}'))
        for base in (TP, PL):
            w(sc + base, self.vt)
            arr = z.alloc(4 * 32)
            w(sc + base + 0x15c, arr), w(sc + base + 0x160, arr), w(sc + base + 0x164, arr + 4 * 32)
            w(sc + base + 0x17c, -1)
        for obj in (0x14fac, 0x153b4):  # the buttons the lists' choices enable (vt+0x80)
            w(sc + obj, self.vt)
        lst = sc + KINDS
        item = z.alloc(8)
        w(item + 4, v[1])
        arr = z.alloc(8)
        w(arr, item)
        w(lst + 0x15c, arr), w(lst + 0x160, arr + 4), w(lst + 0x16c, 0)
        self.slot = z.alloc(0x20)
        w(self.slot + 4, v[3])
        players, self.teams = [], set()
        for k in range(v[4]):
            pid, team, host, state, skin = v[5 + 5 * k:10 + 5 * k]
            r = z.alloc(0x38)
            w(r + 4, pid), w(r + 8, team), self.uc.mem_write(r + 0xc, bytes([host])), w(r + 0x10, z.string(f'P{pid}'))
            w(r + 0x14, state), w(r + 0x1c, skin)
            players.append(r)
        teams = []
        for k in range(v[TB]):
            tid, skin, number = v[TB + 1 + 3 * k:TB + 4 + 3 * k]
            t = z.alloc(0x30)
            w(t + 4, tid), w(t + 8, z.string(f'T{tid}')), w(t + 0x10, skin), w(t + 0x18, number)
            teams.append(t)
            self.teams.add(t)
        ph, th = z.alloc(0xc), z.alloc(0xc)
        self.chain(ph, players), self.chain(th, teams)
        w(g + 0x4c, ph), w(g + 0x58, th), w(g + 0x50, len(players))
        z.call(REFRESH, ecx=sc)
        return self.items(TP) + self.items(PL) + [self.uc.mem_read(sc + 0x1679a, 1)[0]]


def case(rng):
    v = [0] * N_IN
    v[0], v[1] = rng.randrange(2), rng.choice([0, 1, 3, 4])
    v[4] = rng.randrange(0, 9)
    ids = rng.sample(range(1, 99), 8)
    tids = rng.sample(range(100, 140), 4)
    v[2] = rng.choice(ids[:max(1, v[4])] + [500])
    v[3] = rng.choice([-1, 0, 5, 13, 24, 25])
    for k in range(v[4]):
        v[5 + 5 * k:10 + 5 * k] = [ids[k], rng.choice(tids + [0, 0]), rng.randrange(2), rng.randrange(0, 7),
                                   rng.randrange(0, 9)]
    v[TB] = rng.randrange(0, 5)
    for k in range(v[TB]):
        v[TB + 1 + 3 * k:TB + 4 + 3 * k] = [tids[k], rng.randrange(1, 9), rng.randrange(0, 8)]  # 0 reads the game as text
    return v


def main():
    probe = test_pm_zar.load_pm_probe()
    probe.probe_pm_lobby_lines.argtypes = [ctypes.POINTER(I32), ctypes.POINTER(I32), I32]
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 1500
    exe = Exe()
    rng = random.Random(103)
    seen = {}
    sample = None
    for i in range(count):
        v = case(rng)
        theirs = exe.run(v)
        out = (I32 * 8192)()
        n = probe.probe_pm_lobby_lines((I32 * N_IN)(*v), out, 8192)
        mine = list(out[:n])
        if mine != theirs:
            j = next((k for k, (a, b) in enumerate(zip(mine, theirs)) if a != b), min(len(mine), len(theirs)))
            text = lambda x: ''.join(chr(c) if 31 < c < 127 else f'<{c}>' for c in x)  # noqa: E731
            print(f'case {i} (team play {v[0]}, kinds {v[1]}) at {j}:\n  exe  {text(theirs[max(0, j - 30):j + 30])}\n'
                  f'  port {text(mine[max(0, j - 30):j + 30])}', flush=True)
            os._exit(1)
        if v[0] and v[1] == 3 and v[TB] and v[4] > 2 and sample is None:
            sample = ''.join(chr(c) if 31 < c < 127 else ('|' if c == 0 else '') for c in theirs[:theirs.index(-1)])
        key = (v[0], theirs[-1])
        seen[key] = seen.get(key, 0) + 1
    print(f'{count} lobby lists identical; (team play, all ready): {dict(sorted(seen.items()))}\ne.g. {sample}',
          flush=True)
    player_data(probe, rng)
    os._exit(0)


def player_data(probe, rng):
    z = test_pm_zar.Emulated()
    uc = z.cpu._uc
    w = lambda a, x: uc.mem_write(a, struct.pack('<I', x & 0xffffffff))  # noqa: E731
    r = lambda a: struct.unpack('<I', bytes(uc.mem_read(a, 4)))[0]  # noqa: E731
    for slot, fn in ((MALLOC, lambda c: z.alloc(c.arg(0))), (FREE, lambda c: 0),
                     (STRDUP, lambda c: z.string(z.cstr(c.arg(0)).decode('latin1')))):
        f = z.cpu.code(b'\xc3')
        uc.mem_write(slot, struct.pack('<I', f))
        z.native_stub(f, fn)
    state = {}

    def get(c):
        words = state['words']
        if not state['ok']:
            return 0
        uc.mem_write(c.arg(1), struct.pack(f'<{len(words)}I', *words))
        return 1
    stubs = {0x5aa470: (lambda c: 4 * len(state['words']), 0), 0x5aa4e0: (get, 0),
             0x5ce8c6: (lambda c: z.alloc(c.arg(0)), 0), 0x5ce8c0: (lambda c: 0, 0),
             0x5ce8e4: (lambda c: w(c.ecx, z.string('')) or c.ecx, 0), 0x5cea10: (lambda c: c.ecx, 4)}
    for a, (fn, pop) in stubs.items():
        z.native_stub(a, fn, pop)
    probe.probe_pm_player_data.argtypes = [ctypes.POINTER(I32), ctypes.POINTER(I32)]
    sc, g = z.alloc(0x16800), z.alloc(0x100)
    w(0x62db54, sc), w(sc + 0x11cf0, g)
    seen = {}
    for i in range(1000):
        between, n = rng.randrange(2), rng.choice([0, 1, 2, 3, 4, 5, 6, 8])
        words = [rng.choice([0, 1, 2]) if not k else rng.randrange(0, 9) for k in range(n)]
        state['words'], state['ok'] = words, int(rng.random() < 0.9)
        uc.mem_write(sc + 0x1679c, bytes([1, between]))
        for k in (0x3c, 0x40, 0x44):  # the score lines vector
            w(g + k, 0)
        h = z.alloc(0xc)
        w(h, h), w(h + 4, h), w(g + 0x4c, h), w(g + 0x50, 0)
        z.call(0x49c650, args=(7, 3, 0, z.string('P7')))
        rec = r(r(h) + 8)
        theirs = [s32(r(rec + 0x14)), s32(r(rec + 0x1c)), s32(r(rec + 0x2c)), s32(r(rec + 0x30))]
        accepted = state['ok'] and 4 * n > 7
        if accepted and words[0] == 1:  # the exe reads past a short record; the port doesn't (FIX (bug))
            if n < 3:
                theirs[0] = 4
            if between and n < 6:
                theirs[2] = words[4] if n > 4 else 0
                theirs[3] = 0
        given = words if (state['ok'] and 4 * n > 7) else []
        out = (I32 * 4)()
        probe.probe_pm_player_data((I32 * 10)(between, len(given), *(given + [0] * (8 - len(given)))), out)
        mine = list(out)
        if mine != theirs:
            print(f'player data case {i} (between {between}, ok {state["ok"]}, words {words}): exe {theirs} port {mine}',
                  flush=True)
            os._exit(1)
        seen[(theirs[0], theirs[2] > 0)] = seen.get((theirs[0], theirs[2] > 0), 0) + 1
    print(f'player data: 1000 identical; (state, rounds kept) {dict(sorted(seen.items()))}', flush=True)


if __name__ == '__main__':
    main()
