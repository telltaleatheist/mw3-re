"""Pirate's Moon's kill message (0x13) against the port (docs/pm-map.md, the in-game types): the handler 0x470b80
(MW3's 0x46eba0) through probe_mech_net_events' kind 2, as test_mech_net_events does MW3's. PM names both players
from the lobby's records (0x47b710(0)'s +0x48, 0x498930: the record's +0x10), "Unknown" ([0x6077c0]) for one not
there, where MW3 named them from their mechs; the line is the same (0xf0a a suicide, 0xf09 a kill, 5 s), and a host
scores the kill (0x4721e0, tested in test_pm_net_score: here its call, logged on both sides).

    cmake --build build --target pmprobe && MW3_EXE=pm .venv/bin/python tests/differential/test_pm_net_kill.py
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
KILL_IN, MP_PLAYER, IS_HOST, SCORE_KILL, HUD = 0x470b80, 0x47b710, 0x5a8a40, 0x4721e0, 0x4107a0
CS_NEW, CS_FROM, CS_ASSIGN, CS_FREE = 0x5ce8e4, 0x5ceabe, 0x5ceaac, 0x5ce8d8

# probe_mech_net_events' input (test_mech_net_events.py's, copied: that file loads MW3's probe).
NAMES = ['kind', 'len', ('msg', 16), 'local', 'host', 'remotes', ('owners', 4), 'who', 'active', 'hud_scores',
         'has_game', 'limit', 'end_state', 'lobby', ('slots', 32), ('shove', 3)]
K, _i = {}, 0
for n in NAMES:
    name, size = (n, 1) if isinstance(n, str) else n
    K[name], _i = _i, _i + size
COUNT = _i


def num(s):  # the probe's: "Unknown" -1, "P<n>" n
    return -1 if s == b'Unknown' else int(s[1:])


class Exe:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        self.empty = z.alloc(4)
        r32 = lambda a: struct.unpack('<I', bytes(self.uc.mem_read(a, 4)))[0]  # noqa: E731
        self.r32 = r32
        stubs = {CS_NEW: (lambda c: (self.w(c.ecx, self.empty), c.ecx)[1], 0),
                 CS_FROM: (lambda c: (self.w(c.ecx, c.arg(0)), c.ecx)[1], 4),
                 CS_ASSIGN: (lambda c: (self.w(c.ecx, r32(c.arg(0))), c.ecx)[1], 4),
                 CS_FREE: (lambda c: 0, 0), MP_PLAYER: (lambda c: self.lobby, 4),
                 IS_HOST: (lambda c: self.v[K['host']], 0), SCORE_KILL: (self.scored, 8), HUD: (self.hud, 0)}
        for a, (fn, pop) in stubs.items():
            z.native_stub(a, fn, pop)
        game = z.alloc(0x20)  # the scoring's this: [0x62c40c]'s +0x14
        self.w(0x62c40c, game), self.w(game + 0x14, z.alloc(0x100))

    def w(self, a, v):
        self.uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def cstr(self, a):
        return bytes(self.uc.mem_read(a, 0x20)).split(b'\0')[0]

    def hud(self, c):
        t, line = c.arg(0), c.arg(1)
        self.log += [2, struct.unpack('<i', struct.pack('<I', t))[0], line, num(self.cstr(c.arg(2)))]
        if line != 0xf0a:
            self.log.append(num(self.cstr(c.arg(3))))
        return 0

    def scored(self, c):
        self.log += [99, c.arg(0), c.arg(1)]
        return 0

    def run(self, v):
        z, w = self.z, self.w
        self.v, self.log = v, []
        self.lobby = z.alloc(0x60)
        sentinel = z.alloc(0xc)
        w(self.lobby + 0x4c, sentinel)  # the lobby at +0x48, its records' list at +4
        prev = sentinel
        for k in range(v[K['lobby']]):  # the records' list, in the lobby's order
            pid = v[K['slots'] + 4 * k]
            node, rec, name = z.alloc(0xc), z.alloc(0x30), z.alloc(0x10)
            self.uc.mem_write(name, b'P%d\0' % pid)
            w(rec + 4, pid), w(rec + 0x10, name), w(node + 8, rec)
            w(prev, node), w(node + 4, prev)
            prev = node
        w(prev, sentinel), w(sentinel + 4, prev)
        msg = z.alloc(0x40)
        self.uc.mem_write(msg, struct.pack('<16i', *v[K['msg']:K['msg'] + 16])[:v[K['len']]])
        z.call(KILL_IN, args=(msg,))
        return self.log


def case(rng):
    v = [0] * COUNT
    v[K['kind']] = 2
    v[K['local']] = rng.randrange(1, 9)
    v[K['host']] = rng.randrange(2)
    v[K['remotes']] = rng.randrange(0, 5)
    owners = rng.sample([i for i in range(1, 9) if i != v[K['local']]], 4)
    for k in range(4):
        v[K['owners'] + k] = owners[k]
    v[K['has_game']] = 1
    v[K['limit']] = rng.choice([1, 3, 10])
    ids = [v[K['local']]] + owners[:v[K['remotes']]]
    lobby_ids = rng.sample(range(1, 12), rng.randrange(0, 9))
    v[K['lobby']] = len(lobby_ids)
    for k, pid in enumerate(lobby_ids):
        v[K['slots'] + 4 * k:K['slots'] + 4 * k + 4] = [pid, rng.randrange(1, 4), rng.randrange(0, 12), rng.randrange(0, 12)]
    some = lambda: rng.choice(ids + lobby_ids + [rng.randrange(1, 12)])  # noqa: E731
    a = some()
    msg = struct.pack('<HHII', 0x13, 0xc, a, a if rng.random() < 0.2 else some())
    v[K['len']] = len(msg)
    v[K['msg']:K['msg'] + 3] = struct.unpack('<3i', msg)
    return v


def main():
    probe = test_pm_zar.load_pm_probe()
    probe.probe_mech_net_events.argtypes = [ctypes.POINTER(I32), ctypes.POINTER(I32), I32]
    assert COUNT == probe.probe_mech_net_ev_count(), (COUNT, probe.probe_mech_net_ev_count())
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 2000
    exe = Exe()
    rng = random.Random(13)
    seen = {}
    for i in range(count):
        v = case(rng)
        theirs = exe.run(v)
        out = (I32 * 4096)()
        n = probe.probe_mech_net_events((I32 * COUNT)(*v), out, 4096)
        lg = list(out[:n])[:-8]  # less the probe's closing line (90 and seven fields)
        line = lg[:4 if lg[2] == 0xf0a else 5]
        mine = lg
        if mine != theirs:
            print(f'case {i}:\n  exe  {theirs}\n  port {mine} (whole log {lg})', flush=True)
            os._exit(1)
        key = (line[2] == 0xf0a, -1 in line[3:], bool(v[K['host']]))
        seen[key] = seen.get(key, 0) + 1
    print(f'{count} kill lines identical; (a suicide, an Unknown, host): {dict(sorted(seen.items()))}', flush=True)
    os._exit(0)


if __name__ == '__main__':
    main()
