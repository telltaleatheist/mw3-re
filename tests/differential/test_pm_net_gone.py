"""Pirate's Moon's player gone (0x46f440, a DirectPlay player-left; MW3's 0x46d3b0) and its one-side check 0x472da0
against the port's net_player_left and net_two_sides through probe_mech_net_gone (docs/pm-map.md, the in-game
types). PM, whether or not the player's mech is here: the lobby's kills 0 for it (0x436750, stood in for and
logged), its record left (+0x18 1) without a score line (+0x2c 0), on the host the leaving scored as a suicide
(0x4721e0, piece 2's: logged, against the port's net_score_kill crediting the suicide); with the mech, MW3's line
(0x26a, 4 s), the sensors' forget (0x4adc40) and the delete (vt+0). Then if the round has no two sides left
(0x472da0: the network mechs whose players' records aren't out (+0xe, none counts as out) and the pilot's mech
unless the local player is out, by team mask +0xc0) and its end reason (+0x90) is 0, the reason is 5 ("No more
enemies") and the clock's next send (+0x84) is now. The check is also run alone before, on the same state.

    cmake --build build --target pmprobe && MW3_EXE=pm .venv/bin/python tests/differential/test_pm_net_gone.py
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
GONE, SIDES, MP_GAME, SET_KILLS, IS_HOST, SCORE_KILL = 0x46f440, 0x472da0, 0x47b710, 0x436750, 0x5a8a40, 0x4721e0
PLAYER_NAME, HUD, FORGET = 0x5a9960, 0x4107a0, 0x4adc40
GAME_OBJ, MECHS, SESSION, NOW = 0x62c40c, 0x629a5c, 0x8cdcc4, 0x62da90

NAMES = ['from', 'local', 'host', 'pilot', 'pilot_team', 'game', 'round_end', 'clock_next', 'time', 'mechs',
         ('mech', 16), 'lobby', ('rec', 16)]
K, _i = {}, 0
for n in NAMES:
    name, size = (n, 1) if isinstance(n, str) else n
    K[name], _i = _i, _i + size
COUNT = _i


def bits(f):
    return struct.unpack('<i', struct.pack('<f', f))[0]


class Exe:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        self.free_fn = z.cpu.code(b'\xc3')
        stubs = {MP_GAME: (lambda c: self.game, 4), SET_KILLS: (self.set_kills, 8),
                 IS_HOST: (lambda c: self.v[K['host']], 0), SCORE_KILL: (self.scored, 8),
                 PLAYER_NAME: (lambda c: self.names[c.arg(0)], 0), HUD: (self.hud, 0),
                 FORGET: (lambda c: self.log.extend([51, self.owner(c.arg(0))]) or 0, 0),
                 self.free_fn: (self.free, 4)}
        for a, (fn, pop) in stubs.items():
            z.native_stub(a, fn, pop)
        self.names = {}
        for i in range(1, 13):
            a = z.alloc(8)
            self.uc.mem_write(a, b'P%d\0' % i)
            self.names[i] = a

    def w(self, a, v):
        self.uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def r(self, a):
        return struct.unpack('<I', bytes(self.uc.mem_read(a, 4)))[0]

    def owner(self, mech):
        return self.r(mech + 0x76c)

    def set_kills(self, c):
        self.log += [22, c.arg(0), c.arg(1), 52, c.arg(0)]  # and the record's left, which follows unconditionally
        return 0

    def scored(self, c):
        self.log += [99, c.arg(0), c.arg(1)]
        return 0

    def hud(self, c):
        name = bytes(self.uc.mem_read(c.arg(2), 8)).split(b'\0')[0]
        self.log += [2, struct.unpack('<i', struct.pack('<I', c.arg(0)))[0], c.arg(1), int(name[1:])]
        return 0

    def free(self, c):  # vt+0 (1): the destructor takes the mech off the list
        mech = c.ecx
        self.log += [40, self.owner(mech)]
        node = self.r(self.sentinel)
        while node != self.sentinel:
            if self.r(node + 8) == mech:
                nxt, prv = self.r(node), self.r(node + 4)
                self.w(prv, nxt), self.w(nxt + 4, prv)
                break
            node = self.r(node)
        return 0

    def chain(self, head, items):
        prev = head
        for item in items:
            node = self.z.alloc(0xc)
            self.w(node + 8, item), self.w(prev, node), self.w(node + 4, prev)
            prev = node
        self.w(prev, head), self.w(head + 4, prev)

    def run(self, v):
        z, w = self.z, self.w
        self.v, self.log = v, []
        self.game = g = z.alloc(0x100) if v[K['game']] else 0
        go = z.alloc(0x20)
        w(GAME_OBJ, go)
        pilot = z.alloc(0x800)
        w(pilot + 0xc0, v[K['pilot_team']])
        w(go + 0x14, pilot)
        sess = z.alloc(8)
        w(SESSION, sess), w(sess + 4, v[K['local']])
        self.uc.mem_write(NOW, struct.pack('<i', v[K['time']]))
        recs = []
        self.recs = {}
        for k in range(v[K['lobby']]):
            rec = z.alloc(0x30)
            pid, out = v[K['rec'] + 2 * k], v[K['rec'] + 2 * k + 1]
            w(rec + 4, pid), w(rec + 0x2c, 0x1234)
            self.uc.mem_write(rec + 0xe, bytes([out]))
            recs.append(rec)
            self.recs[pid] = rec
        if g:
            w(g + 0x90, v[K['round_end']]), w(g + 0x84, v[K['clock_next']])
            lobby = z.alloc(0x10)
            w(g + 0x4c, lobby)
            self.chain(lobby, recs)
        vt = z.alloc(4)
        w(vt, self.free_fn)
        mechs = []
        for k in range(v[K['mechs']]):
            m = z.alloc(0x800)
            w(m, vt), w(m + 0x76c, v[K['mech'] + 2 * k]), w(m + 0xc0, v[K['mech'] + 2 * k + 1])
            mechs.append(m)
        self.sentinel = z.alloc(0xc)
        w(MECHS, self.sentinel)
        self.chain(self.sentinel, mechs)
        self.log += [30, z.call(SIDES) & 1]
        z.call(GONE, args=(v[K['from']],))
        rec = self.recs.get(v[K['from']])
        if rec and (self.uc.mem_read(rec + 0x18, 1)[0] != 1 or self.r(rec + 0x2c) != 0):
            self.log.append('record not left')
        self.log += [90, self.r(g + 0x90), self.r(g + 0x84)] if g else [90, v[K['round_end']], v[K['clock_next']]]
        return self.log


def port_log(lg):
    """The probe's log with the port's scoring (its suicide credit, 20) as the exe's call (99), its sends dropped."""
    out, i = [], 0
    sizes = {30: 2, 22: 3, 52: 2, 2: 4, 51: 2, 40: 2, 90: 3}
    while i < len(lg):
        t = lg[i]
        if t == 20:
            out += [99, lg[i + 1], lg[i + 1]]
            i += 2
        elif t == 1:  # a send: to, guaranteed, size, then its words
            i += 4 + (lg[i + 3] + 3) // 4
        elif t in sizes:
            out += lg[i:i + sizes[t]]
            i += sizes[t]
        else:
            out.append(('unknown', t))
            i += 1
    return out


def case(rng):
    v = [0] * COUNT
    v[K['local']] = rng.randrange(1, 9)
    v[K['host']] = rng.randrange(2)
    v[K['pilot']] = 1
    teams = rng.sample([1 << b for b in range(1, 9)], rng.randrange(1, 4))
    v[K['pilot_team']] = rng.choice(teams)
    v[K['game']] = 1  # the exe reads the network game unchecked
    v[K['round_end']] = rng.choice([0, 0, 0, 1, 5])
    v[K['clock_next']] = bits(rng.uniform(0, 100))
    v[K['time']] = bits(rng.uniform(0, 600))
    others = rng.sample([i for i in range(1, 12) if i != v[K['local']]], 8)
    v[K['mechs']] = rng.randrange(0, 6)
    for k in range(8):
        v[K['mech'] + 2 * k:K['mech'] + 2 * k + 2] = [others[k], rng.choice(teams)]
    players = [v[K['local']]] + others[:v[K['mechs']]] + [rng.randrange(1, 12)]
    listed = [p for p in dict.fromkeys(players) if rng.random() < 0.85][:8]
    v[K['lobby']] = len(listed)
    for k, p in enumerate(listed):
        v[K['rec'] + 2 * k:K['rec'] + 2 * k + 2] = [p, int(rng.random() < 0.25)]
    v[K['from']] = rng.choice(others[:v[K['mechs']]] + [rng.randrange(1, 12)]) if v[K['mechs']] else rng.randrange(1, 12)
    return v


def main():
    probe = test_pm_zar.load_pm_probe()
    probe.probe_mech_net_gone.argtypes = [ctypes.POINTER(I32), ctypes.POINTER(I32), I32]
    assert COUNT == probe.probe_mech_net_gone_count(), (COUNT, probe.probe_mech_net_gone_count())
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 2000
    exe = Exe()
    rng = random.Random(17)
    seen = {}
    for i in range(count):
        v = case(rng)
        theirs = exe.run(v)
        out = (I32 * 4096)()
        n = probe.probe_mech_net_gone((I32 * COUNT)(*v), out, 4096)
        mine = port_log(list(out[:n]))
        theirs = [x if isinstance(x, str) else struct.unpack('<i', struct.pack('<I', x & 0xffffffff))[0] for x in theirs]
        if mine != theirs:
            print(f'case {i} ({dict((k, v[K[k]]) for k in ("from", "local", "host", "game", "round_end", "mechs"))}):'
                  f'\n  exe  {theirs}\n  port {mine}', flush=True)
            os._exit(1)
        ended = theirs[-2] == 5 and v[K['round_end']] == 0
        key = (theirs[1], 40 in theirs, 99 in theirs, ended)
        seen[key] = seen.get(key, 0) + 1
    print(f'{count} identical; (two sides before, a mech deleted, scored, the round ended): '
          f'{dict(sorted(seen.items()))}', flush=True)
    os._exit(0)


if __name__ == '__main__':
    main()
