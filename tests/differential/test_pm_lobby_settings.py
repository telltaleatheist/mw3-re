"""Pirate's Moon's lobby settings (piece 4d of the multiplayer group, pm-tests.md) against the port through
probe_pm_lobby_settings, on a lobby screen laid out as the exe's: each list an object whose items are at +0x15c..+0x160
and whose pick is at +0x16c (its value an item's +4), read by the exe's own 0x4395e0 and 0x438800; the boxes as ints.

1. The host's start 0x49ef20, its game half (MultiplayerGame::apply_lobby): the score limit (+0x78, 0x7fffffff at
   1000 or over), the boxes into +0xa4..+0xa9 and +0xc4, the tons (+0x80), the minutes (+0x7c), REALLOC (+0x70), the
   rounds (+0x74), the kind (+0x98), the rules' record (+0x64..+0x6a) and flags (+0x60, or'd on), the clock ([0x62da98]
   under 1000 minutes, [0x62da94] the seconds), the world's folder (+0xac) and number (+0xb0). Not the host here; the
   calls after (0x49d170, 0x4377c0, 0x49ea90, 0x4a02b0) stood in for.
2. Its message as the host (start_message): 0x1e, the count, each player's id, order (+0x20) and team number
   (0x436600, run as the exe's), guaranteed to all.
3. The option rules 0x49e400 (check_lobby_rules): which error, which confirmations (0x4afb30 with 0x1e3 and 0xfbd or
   0x2c4, answered by the case), and the team play it sets ([0x62cc34]); the screen's own calls stood in for.
4. A target killed 0x4e2f50 (CZAiTarget::kill, MW3's 0x4d2b60): a base's (its targets.zrd mpoption, +0xe8, 1 or 2;
   -1 none) then tells the network game 0x472b40 (TargetHooks::base_destroyed; stood in for and logged).
5. The 0x1e read 0x49f510 (MultiplayerGame::read_start_message; a handler: the sender, the message): each listed player takes its order
   (the lobby's lookup 0x498930 run), the team number unread; the messages made by start_message's rule, with counts
   past the players listed.

    cmake --build build --target pmprobe && MW3_EXE=pm .venv/bin/python tests/differential/test_pm_lobby_settings.py
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
START, RULES = 0x49ef20, 0x49e400
TEAM_PLAY, CLOCK_ON, CLOCK_LEFT, MALLOC, FREE = 0x62cc34, 0x62da98, 0x62da94, 0x5de664, 0x5de65c
PICKS = [0x13974, 0x13d98, 0x141bc, 0x1312c, 0x13550, 0x128e4, 0x12d08]  # kills, minutes, tons, realloc, rounds, kind, world
BOXES = [0x145c4, 0x1489c, 0x14a08, 0x14b74, 0x14ce0, 0x11cd4, 0x14730]  # respawn, ammo, outside, jets, radar, stock, mobile
ERRORS = {b'Attack/Defend with one base requires a time limit or a score limit': 1,
          b'Attack/Defend with two bases requires unlimited time and unlimited score.': 3,
          b"Attack/Defend games and 'No RESPAWN' are incompatible.": 4}
N_IN = 25 + 8 * 3 + 1 + 4 * 2


def s32(v):
    return struct.unpack('<i', struct.pack('<I', v & 0xffffffff))[0]


class Exe:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        for slot, fn in ((MALLOC, lambda c: z.alloc(c.arg(0))), (FREE, lambda c: 0)):
            f = z.cpu.code(b'\xc3')
            self.uc.mem_write(slot, struct.pack('<I', f))
            z.native_stub(f, fn)
        stubs = {0x5a8a40: (lambda c: self.host, 0), 0x5a97d0: (self.send, 0), 0x49d170: (lambda c: 0, 0),
                 0x4377c0: (lambda c: 0, 4), 0x49ea90: (lambda c: 0, 0), 0x4a02b0: (lambda c: 0, 0),
                 0x4afa20: (self.error, 0), 0x4afb30: (self.confirm, 0), 0x48a2e0: (lambda c: self.log.extend([8, s32(c.arg(0))]) or 0, 4),
                 0x5a9520: (lambda c: 0, 0), 0x5a9220: (lambda c: 0, 0), 0x5a92e0: (lambda c: 0, 0),
                 0x5aad50: (lambda c: 0, 0), 0x5aadb0: (lambda c: 0, 0), 0x49b3f0: (lambda c: 0, 0),
                 0x49d220: (lambda c: 0, 0), 0x49eb20: (lambda c: self.log.append(9) or 0, 0)}
        for a, (fn, pop) in stubs.items():
            z.native_stub(a, fn, pop)
        self.vt = z.alloc(0x100)
        for off, pop in ((0x60, 4), (0x64, 0), (0x8c, 4)):
            f = z.cpu.code(b'\xc3')
            z.native_stub(f, (lambda o: lambda c: self.log.append(0x100 + o) if o == 0x8c else 0)(off), pop)
            self.uc.mem_write(self.vt + off, struct.pack('<I', f))
        self.host = 0

    def w(self, a, v):
        self.uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def r(self, a):
        return struct.unpack('<I', bytes(self.uc.mem_read(a, 4)))[0]

    def send(self, c):
        n = c.arg(1) & 0xffff
        self.sent = [1 if c.arg(2) & 0xff else 0, s32(c.arg(3))] + \
            list(struct.unpack(f'<{n // 4}i', bytes(self.uc.mem_read(c.arg(0), n))))
        return 1

    def error(self, c):
        self.err = ERRORS[self.z.cstr(c.arg(2))]
        return 0

    def confirm(self, c):
        assert c.arg(0) == 1 and c.arg(1) == 0x1e3, (c.arg(0), hex(c.arg(1)))
        self.log += [7, c.arg(2)]
        return 1 if self.v[23 if c.arg(2) == 0xfbd else 24] else 0

    def screen(self, v):
        z, w = self.z, self.w
        if not hasattr(self, 'sc'):
            self.sc, self.g = z.alloc(0x16800), z.alloc(0x100)
        sc, g = self.sc, self.g
        self.uc.mem_write(sc, bytes(0x16800)), self.uc.mem_write(g, bytes(0x100))
        w(sc + 0x11cf0, g)
        for obj in (0x58, 0x14474, 0x11388):  # objects the screen holds: their vtables
            w(sc + obj, self.vt)
        for k, idx in enumerate(PICKS):
            has, value = v[1 + 2 * k], v[2 + 2 * k]
            lst = sc + idx - 0x16c
            item = z.alloc(8)
            w(item + 4, value)
            arr = z.alloc(8)
            w(arr, item)
            w(lst + 0x15c, arr), w(lst + 0x160, arr + 4), w(idx + sc, 0 if has else -1)
        for k, off in enumerate(BOXES):
            w(sc + off, v[15 + k])
        w(g + 0x60, v[22])
        for head in (0x4c, 0x58):
            h = z.alloc(0xc)
            w(h, h), w(h + 4, h), w(g + head, h)
        return sc, g

    def push(self, head, item):
        node, last = self.z.alloc(0xc), self.r(head + 4)
        self.w(node + 8, item), self.w(node, head), self.w(node + 4, last), self.w(last, node), self.w(head + 4, node)

    def settings(self, v):
        self.v, self.log, self.host = v, [], 0
        sc, g = self.screen(v)
        self.z.call(START, ecx=sc)
        rd8 = lambda a: self.uc.mem_read(a, 1)[0]  # noqa: E731
        sh = lambda a: struct.unpack('<h', bytes(self.uc.mem_read(a, 2)))[0]  # noqa: E731
        return [s32(self.r(g + 0x78)), rd8(g + 0xa4), rd8(g + 0xa5), rd8(g + 0xa6), rd8(g + 0xa7), rd8(g + 0xa8),
                rd8(g + 0xa9), rd8(g + 0xc4), s32(self.r(g + 0x80)), s32(self.r(g + 0x7c)), s32(self.r(g + 0x70)),
                s32(self.r(g + 0x74)), s32(self.r(g + 0x98)), sh(g + 0x64), sh(g + 0x66), sh(g + 0x68), sh(g + 0x6a),
                s32(self.r(g + 0x60)), rd8(CLOCK_ON), s32(self.r(CLOCK_LEFT)), s32(self.r(g + 0xac)), s32(self.r(g + 0xb0))]

    def message(self, v):
        self.v, self.log, self.host, self.sent = v, [], 1, None
        sc, g = self.screen(v)
        self.uc.mem_write(TEAM_PLAY, bytes([v[23]]))
        w = self.w
        w(g + 0x50, v[24])
        for k in range(v[24]):
            pid, team, order = v[25 + 3 * k:28 + 3 * k]
            r = self.z.alloc(0x38)
            w(r + 4, pid), w(r + 8, team), w(r + 0x20, order)
            self.push(self.r(g + 0x4c), r)
        tb = 25 + 8 * 3
        for k in range(v[tb]):
            tid, number = v[tb + 1 + 2 * k:tb + 3 + 2 * k]
            t = self.z.alloc(0x30)
            w(t + 4, tid), w(t + 0x18, number)
            self.push(self.r(g + 0x58), t)
        self.z.call(START, ecx=sc)
        assert self.sent[:2] == [1, 0], self.sent[:2]
        return self.sent[2:]

    def rules(self, v):
        self.v, self.log, self.host, self.err = v, [], 0, None
        sc, g = self.screen(v)
        self.uc.mem_write(TEAM_PLAY, b'\x07')
        self.z.call(RULES, ecx=sc)
        confirms = [x for i, x in enumerate(self.log) if i and self.log[i - 1] == 7]
        if self.err is not None:
            rule = self.err
        elif 9 in self.log:
            rule = 0
        else:
            rule = 2
        team = self.uc.mem_read(TEAM_PLAY, 1)[0]
        out = []
        for t in confirms:
            out += [7, t]
        return out + [90, rule, 1 if rule == 0 and team == 1 else 0], self.log


def case(rng):
    v = [0] * N_IN
    for k in range(7):
        v[1 + 2 * k] = int(rng.random() < 0.9)
    v[2] = rng.choice([1, 5, 20, 999, 1000, 1001])
    v[4] = rng.choice([1, 10, 30, 999, 1000])
    v[6] = rng.choice([10, 35, 90, 101])
    v[8] = rng.choice([0, 2, 10, -4])
    v[10] = rng.choice([1, 3, 5, 12])
    v[12] = rng.choice([0, 1, 2, 3, 4])
    v[14] = rng.choice([0x100, 0x201, 0x302, 0x103, 0x2ff, 0x10203])
    for k in range(7):
        v[15 + k] = rng.choice([0, 0, 1, 7])
    v[22] = rng.choice([0, 0x20, 0x3])
    v[23], v[24] = rng.randrange(2), rng.randrange(2)
    return v


def message_case(rng):
    v = case(rng)
    v[23] = rng.randrange(2)
    v[24] = rng.randrange(0, 9)
    tids = rng.sample(range(1, 40), 4)
    ids = rng.sample(range(1, 99), 8)
    for k in range(v[24]):
        v[25 + 3 * k:28 + 3 * k] = [ids[k], rng.choice(tids + [0]), rng.randrange(0, 9)]
    tb = 25 + 8 * 3
    v[tb] = rng.randrange(0, 5)
    for k in range(v[tb]):
        v[tb + 1 + 2 * k:tb + 3 + 2 * k] = [tids[k], rng.randrange(0, 8)]
    return v


def main():
    probe = test_pm_zar.load_pm_probe()
    probe.probe_pm_lobby_settings.argtypes = [ctypes.POINTER(I32), ctypes.POINTER(I32)]
    exe = Exe()
    rng = random.Random(101)
    for op, name, make, run in ((1, 'settings', case, exe.settings), (2, 'start message', message_case, exe.message),
                                (3, 'option rules', case, lambda v: exe.rules(v)[0])):
        seen = {}
        for i in range(1000):
            v = make(rng)
            v[0] = op
            theirs = run(v)
            out = (I32 * 128)()
            n = probe.probe_pm_lobby_settings((I32 * N_IN)(*v), out)
            mine = list(out[:n])
            if mine != theirs:
                print(f'{name} case {i} {v[:25]}:\n  exe  {theirs}\n  port {mine}', flush=True)
                os._exit(1)
            key = theirs[-2] if op == 3 else theirs[18] if op == 1 else len(theirs)
            seen[key] = seen.get(key, 0) + 1
        print(f'{name}: 1000 identical; {dict(sorted(seen.items()))}', flush=True)
    target_kill(probe, rng)
    read_start(probe, rng)
    os._exit(0)


def read_start(probe, rng):
    z = test_pm_zar.Emulated()
    uc = z.cpu._uc
    w = lambda a, x: uc.mem_write(a, struct.pack('<I', x & 0xffffffff))  # noqa: E731
    r = lambda a: struct.unpack('<I', bytes(uc.mem_read(a, 4)))[0]  # noqa: E731
    probe.probe_pm_read_start.argtypes = [ctypes.POINTER(I32), ctypes.POINTER(I32)]
    sc, g = z.alloc(0x16800), z.alloc(0x100)
    w(0x62db54, sc), w(sc + 0x11cf0, g)
    for i in range(500):
        ids = rng.sample(range(1, 40), rng.randrange(0, 9))
        h = z.alloc(0xc)
        prev, recs = h, []
        for pid in ids:
            rec, node = z.alloc(0x38), z.alloc(0xc)
            w(rec + 4, pid), w(rec + 0x20, -5)
            w(node + 8, rec), w(prev, node), w(node + 4, prev)
            prev = node
            recs.append(rec)
        w(prev, h), w(h + 4, prev), w(g + 0x4c, h)
        listed = rng.sample(ids + rng.sample(range(40, 60), 3), rng.randrange(0, len(ids) + 3)) if ids else []
        words = [0x1e | (8 + 12 * len(listed)) << 16, len(listed)]
        for pid in listed:
            words += [pid, rng.randrange(0, 9), rng.randrange(0, 8)]
        msg = z.alloc(4 * len(words) + 4)
        uc.mem_write(msg, struct.pack(f'<{len(words)}I', *words))
        z.call(0x49f510, args=(1, msg))  # (sender, message), a zNetwork handler
        theirs = [s32(r(rec + 0x20)) for rec in recs]
        out = (I32 * 8)()
        inp = [len(ids)] + ids + [0] * (8 - len(ids)) + [len(words)] + words
        probe.probe_pm_read_start((I32 * len(inp))(*inp), out)
        mine = list(out[:len(ids)])
        if mine != theirs:
            print(f'0x1e read case {i} {listed}: exe {theirs} port {mine}', flush=True)
            os._exit(1)
    print('0x1e read: 500 identical (each listed player its order)', flush=True)


def target_kill(probe, rng):
    z = test_pm_zar.Emulated()
    uc = z.cpu._uc
    told = []
    z.native_stub(0x472b40, lambda c: told.append(s32(c.arg(0))) or 0, 0)
    probe.probe_pm_target_kill.argtypes = [ctypes.POINTER(I32), ctypes.POINTER(I32)]
    t = z.alloc(0x110)
    seen = {}
    for i in range(500):
        health = struct.unpack('<i', struct.pack('<f', rng.choice([0.0, -5.0, 0.5, 100.0])))[0]
        v = [health, rng.choice([-1, -1, 1, 2, 0, 7]), rng.randrange(2)]
        uc.mem_write(t, bytes(0x110))
        uc.mem_write(t + 0xd8, struct.pack('<i', health)), uc.mem_write(t + 0xe8, struct.pack('<i', v[1]))
        uc.mem_write(t + 8, bytes([v[2]]))
        told.clear()
        z.call(0x4e2f50, ecx=t)
        theirs = [told[0] if told else -9, uc.mem_read(t + 0xfc, 1)[0], uc.mem_read(t + 8, 1)[0],
                  s32(struct.unpack('<I', bytes(uc.mem_read(t + 0xd8, 4)))[0])]
        out = (I32 * 4)()
        probe.probe_pm_target_kill((I32 * 3)(*v), out)
        mine = list(out)
        if mine != theirs:
            print(f'target kill case {i} {v}: exe {theirs} port {mine}', flush=True)
            os._exit(1)
        seen[theirs[0]] = seen.get(theirs[0], 0) + 1
    print(f'target kill: 500 identical; the base told {dict(sorted(seen.items()))}', flush=True)


if __name__ == '__main__':
    main()
