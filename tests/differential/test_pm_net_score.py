"""Pirate's Moon's kill scoring (0x4721e0, MW3's 0x470170) against the port's net_score_kill through
probe_mech_net_score, and its suicide rule (0x436820, MW3's 0x435ac0) against MultiplayerGame::add_deaths through
probe_pm_add_deaths (docs/pm-map.md, piece 2 of the multiplayer group in pm-tests.md).

The scoring, on the host with the network game there: nothing once the round's end reason (+0x90) is over 1; the
victim out of the round (0x4379b0) unless players respawn (+0xa4); a suicide (0x436820); a kill between teams
(0x436600) counted (0x436710), and at the score limit (+0x78, 0x4367b0) a round with no reason yet ends, 4 with a
winner (0x437970) else 1 (overtime); in overtime a winner ends it (2); without respawning, no two sides left
(0x472da0) and no reason yet ends it (5) with the round's scores (0x437a10); each end sends the clock now (+0x84).
Then the rescore (0x4374f0), the scores (0x14: each record's id, kills, deaths) and the HUD's scores (0x4486e0)
unless hidden (+0x17b5c bit 4). The lobby's calls, the winner and the two sides are stood in for on both sides from
one table (the port's two sides from network mechs that give the same answer).

    cmake --build build --target pmprobe && MW3_EXE=pm .venv/bin/python tests/differential/test_pm_net_score.py
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
SCORE, SUICIDE = 0x4721e0, 0x436820
NAMES = ['victim', 'killer', 'host', 'game', 'reason', 'clock', 'time', 'limit', 'respawn', 'sides', 'winners',
         'hud_hidden', 'lobby', ('rec', 40)]
K, _i = {}, 0
for n in NAMES:
    name, size = (n, 1) if isinstance(n, str) else n
    K[name], _i = _i, _i + size
COUNT = _i
NOW, VIEW, MALLOC, FREE = 0x62da90, 0x61874c, 0x5de664, 0x5de65c


def s32(v):
    return struct.unpack('<i', struct.pack('<I', v & 0xffffffff))[0]


def bits(f):
    return struct.unpack('<i', struct.pack('<f', f))[0]


class Exe:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        for slot, fn in ((MALLOC, lambda c: z.alloc(c.arg(0))), (FREE, lambda c: 0)):
            f = z.cpu.code(b'\xc3')
            self.uc.mem_write(slot, struct.pack('<I', f))
            z.native_stub(f, fn)
        rec = self.field
        stubs = {0x5a8a40: (lambda c: self.v[K['host']], 0), 0x47b710: (lambda c: self.game, 4),
                 0x4379b0: (self.set_out, 8), 0x436820: (lambda c: self.log.extend([20, c.arg(0)]) or 0, 8),
                 0x436600: (lambda c: rec(c.arg(0), 1, 0), 4),
                 0x436710: (lambda c: self.log.extend([21, c.arg(0)]) or 0, 8),
                 0x4367b0: (lambda c: rec(c.arg(0), 4, -1), 4), 0x437970: (self.winner, 0),
                 0x472da0: (lambda c: self.v[K['sides']], 0), 0x437a10: (lambda c: self.log.append(61) or 0, 0),
                 0x4374f0: (lambda c: self.log.append(24) or 0, 4), 0x5a97d0: (self.send, 0),
                 0x430e30: (lambda c: self.hud, 0), 0x4486e0: (lambda c: self.log.append(25) or 0, 0)}
        for a, (fn, pop) in stubs.items():
            z.native_stub(a, fn, pop)

    def w(self, a, v):
        self.uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def r(self, a):
        return struct.unpack('<I', bytes(self.uc.mem_read(a, 4)))[0]

    def field(self, p, f, none):
        for k in range(self.v[K['lobby']]):
            if self.v[K['rec'] + 5 * k] == p:
                return self.v[K['rec'] + 5 * k + f]
        return none

    def set_out(self, c):
        if not self.uc.mem_read(self.game + 0xa4, 1)[0]:  # the function's own check
            self.log += [60, c.arg(0), c.arg(1) & 0xff]
        return 0

    def winner(self, c):
        a = self.v[K['winners']] >> self.asked & 1
        self.asked += 1
        return a

    def send(self, c):
        buf, n, guaranteed, to = (c.arg(i) for i in range(4))
        n &= 0xffff
        data = bytes(self.uc.mem_read(buf, n)) + bytes(4)
        self.log += [1, s32(to), 1 if guaranteed else 0, n]
        self.log += [struct.unpack('<i', data[k:k + 4])[0] for k in range(0, n, 4)]
        return 1

    def run(self, v):
        z, w = self.z, self.w
        self.v, self.log, self.asked = v, [], 0
        g = z.alloc(0x100)
        self.game = g if v[K['game']] else 0
        w(g + 0x90, v[K['reason']]), w(g + 0x84, v[K['clock']]), w(g + 0x78, v[K['limit']])
        self.uc.mem_write(g + 0xa4, bytes([v[K['respawn']]]))
        w(g + 0x50, v[K['lobby']])
        head = z.alloc(0xc)
        w(g + 0x4c, head)
        prev = head
        for k in range(v[K['lobby']]):
            rec, node = z.alloc(0x30), z.alloc(0xc)
            pid, _team, kills, deaths, _score = v[K['rec'] + 5 * k:K['rec'] + 5 * k + 5]
            w(rec + 4, pid), w(rec + 0x24, kills), w(rec + 0x28, deaths)
            w(node + 8, rec), w(prev, node), w(node + 4, prev)
            prev = node
        w(prev, head), w(head + 4, prev)
        self.uc.mem_write(NOW, struct.pack('<i', v[K['time']]))
        pilot = z.alloc(0x800)
        view = z.alloc(0x80)
        w(VIEW, view), w(view + 0x64, pilot)
        self.hud = z.alloc(0x10) - 0x17b5c  # only its flags (+0x17b5c) are read: the rest is stood in for
        w(self.hud + 0x17b5c, 0x10 if v[K['hud_hidden']] else 0)
        z.call(SCORE, ecx=pilot, args=(v[K['victim']], v[K['killer']]))
        self.log += [90, s32(self.r(g + 0x90)), s32(self.r(g + 0x84))]
        return self.log


def case(rng):
    v = [0] * COUNT
    v[K['host']] = int(rng.random() < 0.9)
    v[K['game']] = int(rng.random() < 0.95)
    v[K['reason']] = rng.choice([0, 0, 0, 0, 1, 1, 2, 5])
    v[K['clock']] = bits(rng.uniform(0, 100))
    v[K['time']] = bits(rng.uniform(0, 600))
    v[K['limit']] = rng.choice([1, 3, 5, 10])
    v[K['respawn']] = rng.randrange(2)
    v[K['sides']] = rng.randrange(2)
    v[K['winners']] = rng.getrandbits(3)
    v[K['hud_hidden']] = rng.randrange(2)
    ids = rng.sample(range(1, 12), rng.randrange(1, 9))
    v[K['lobby']] = len(ids)
    for k, p in enumerate(ids):
        v[K['rec'] + 5 * k:K['rec'] + 5 * k + 5] = [p, rng.choice([1, 2, 4]), rng.randrange(0, 12), rng.randrange(0, 6),
                                                    rng.randrange(0, 12)]
    who = ids + [rng.randrange(1, 12)]
    v[K['victim']] = rng.choice(who)
    v[K['killer']] = v[K['victim']] if rng.random() < 0.25 else rng.choice(who)
    return v


def deaths_rule(probe, z, rng):
    """The suicide's own rule: deaths up only while kills exceed deaths."""
    uc = z.cpu._uc
    rescored = []
    z.native_stub(0x4374f0, lambda c: rescored.append(1) or 0, 4)
    probe.probe_pm_add_deaths.argtypes = [I32, I32, I32, I32, ctypes.POINTER(I32)]
    for i in range(500):
        listed, kills, deaths, n = rng.randrange(4) > 0, rng.randrange(0, 6), rng.randrange(0, 6), rng.choice([1, 2])
        lobby = z.alloc(0x60)
        head, node, rec = z.alloc(0xc), z.alloc(0xc), z.alloc(0x30)
        w = lambda a, x: uc.mem_write(a, struct.pack('<I', x & 0xffffffff))  # noqa: E731
        w(lobby + 0x4c, head)
        if listed:
            w(rec + 4, 7), w(rec + 0x24, kills), w(rec + 0x28, deaths)
            w(node + 8, rec), w(head, node), w(head + 4, node), w(node, head), w(node + 4, head)
        else:
            w(head, head), w(head + 4, head)
        rescored.clear()
        r = s32(z.call(SUICIDE, ecx=lobby, args=(7, n)))
        exe = [r, s32(struct.unpack('<I', bytes(uc.mem_read(rec + 0x28, 4)))[0]) if listed else -9]
        out = (I32 * 2)()
        probe.probe_pm_add_deaths(int(listed), kills, deaths, n, out)
        if exe != list(out):
            print(f'suicide case {i} (listed {listed}, kills {kills}, deaths {deaths}, n {n}): exe {exe} port '
                  f'{list(out)}', flush=True)
            os._exit(1)
    print('suicide rule: 500 identical (counted only while kills exceed deaths)', flush=True)


def main():
    probe = test_pm_zar.load_pm_probe()
    probe.probe_mech_net_score.argtypes = [ctypes.POINTER(I32), ctypes.POINTER(I32), I32]
    assert COUNT == probe.probe_mech_net_score_count(), (COUNT, probe.probe_mech_net_score_count())
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    exe = Exe()
    rng = random.Random(23)
    seen = {}
    for i in range(count):
        v = case(rng)
        theirs = exe.run(v)
        out = (I32 * 4096)()
        n = probe.probe_mech_net_score((I32 * COUNT)(*v), out, 4096)
        mine = list(out[:n])
        if mine != theirs:
            print(f'case {i} ({dict((k, v[K[k]]) for k in NAMES[:12])}):\n  exe  {theirs}\n  port {mine}', flush=True)
            os._exit(1)
        key = (v[K['reason']], theirs[-2])
        seen[key] = seen.get(key, 0) + 1
    print(f'{count} scorings identical; (reason before, after): {dict(sorted(seen.items()))}', flush=True)
    deaths_rule(probe, test_pm_zar.Emulated(), rng)
    os._exit(0)


if __name__ == '__main__':
    main()
