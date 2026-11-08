"""Pirate's Moon's network tick on the pilot's mech, 0x471040 (MW3's 0x46ef70), against the port's net_mech_tick through
probe_pm_net_tick, and its clock send (0x471210 kind 0x11) against net_send_event through probe_pm_net_clock_send
(piece 3b of the multiplayer group, pm-tests.md).

The tick: the state (0xf) on its period (0x472d10 on the game's players +0x50 and the rate [0x62cc30], run as the exe's:
no fast rate in PM), every 26th with every part's damage; the ping (0x10) every 5.1 s; with the clock on ([0x62da98])
the minute's warning (0xf0b) once; on the host, the clock under 3 s with no end reason (+0x90) yet: 3, or 1 (overtime)
without a winner (0x437970), and the clock sent now (+0x84); the clock (0x11) when +0x84 is due, again 5 s on; the game
over (0x472940) once the round's ending (+0x94) and its time (+0x88) passed. The sends (vt+0x17c), the HUD, the winner
and the game over are stood in for and logged on both sides.

The clock send: [0x607744]'s low 3 bits the game's +0x90 and the clock left, sent by the host to all (guaranteed once
there is a reason), then applied here with the local id (0x472850, stood in for).

    cmake --build build --target pmprobe && MW3_EXE=pm .venv/bin/python tests/differential/test_pm_net_tick.py
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
TICK, SEND_EVENT, MP_GAME, HUD_MSG, IS_HOST, WINNER, GAME_OVER = (0x471040, 0x471210, 0x47b710, 0x4107a0, 0x5a8a40,
                                                                  0x437970, 0x472940)
SEND, LOCAL, CLOCK_IN = 0x5a97d0, 0x5a8a30, 0x472850
NOW, CLOCK_LEFT, CLOCK_ON, NEXT_STATE, WARNED, RESENDS, NEXT_PING, RATE, RUNNING = (
    0x62da90, 0x62da94, 0x62da98, 0x629a74, 0x629a78, 0x629aa0, 0x629a9c, 0x62cc30, 0x629a88)
N_TICK = 16


def s32(v):
    return struct.unpack('<i', struct.pack('<I', v & 0xffffffff))[0]


def bits(f):
    return struct.unpack('<i', struct.pack('<f', f))[0]


class Exe:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        stubs = {MP_GAME: (lambda c: self.game, 4), HUD_MSG: (lambda c: self.log.extend([2, s32(c.arg(0)), c.arg(1)]) or 0, 0),
                 IS_HOST: (lambda c: self.v[7], 0), WINNER: (lambda c: self.log.append(5) or self.v[14], 0),
                 GAME_OVER: (lambda c: self.log.append(4) or 0, 0)}
        for a, (fn, pop) in stubs.items():
            z.native_stub(a, fn, pop)
        self.vt_send = z.cpu.code(b'\xc3')
        z.native_stub(self.vt_send, self.send, 8)
        self.game = z.alloc(0x100)
        self.mvt = z.alloc(0x200)
        self.uc.mem_write(self.mvt + 0x17c, struct.pack('<I', self.vt_send))
        self.mech = z.alloc(0x800)

    def w(self, a, v):
        self.uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def r(self, a):
        return struct.unpack('<I', bytes(self.uc.mem_read(a, 4)))[0]

    def send(self, c):
        kind = c.arg(0)
        self.log += [3, kind, 1 if c.arg(1) == self.game else 0]
        return self.v[15] if kind == 0xf else 0

    def run(self, v):
        uc, w, g, m = self.uc, self.w, self.game, self.mech
        self.v, self.log = v, []
        uc.mem_write(g, bytes(0x100))
        w(g + 0x50, v[8]), w(g + 0x90, v[10]), w(g + 0x84, v[11]), uc.mem_write(g + 0x94, bytes([v[12]]))
        w(g + 0x88, v[13])
        uc.mem_write(m, bytes(0x800))
        w(m, self.mvt), w(m + 0x690, 77), uc.mem_write(m + 0x68c, b'\x01'), w(m + 0x6a4, 5)
        w(NOW, v[0]), w(NEXT_STATE, v[1]), w(RESENDS, v[2]), w(NEXT_PING, v[3]), uc.mem_write(CLOCK_ON, bytes([v[4]]))
        w(CLOCK_LEFT, v[5]), uc.mem_write(WARNED, bytes([v[6]])), w(RATE, v[9])
        self.z.call(TICK, ecx=m)
        self.log += [90, s32(self.r(NEXT_STATE)), s32(self.r(RESENDS)), s32(self.r(NEXT_PING)), uc.mem_read(WARNED, 1)[0],
                     s32(self.r(g + 0x90)), s32(self.r(g + 0x84)), s32(self.r(m + 0x690)), uc.mem_read(m + 0x68c, 1)[0],
                     s32(self.r(m + 0x6a4))]
        return self.log


class ExeSend:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        stubs = {SEND: (self.send, 0), IS_HOST: (lambda c: self.v[0], 0), LOCAL: (lambda c: self.v[1], 0),
                 CLOCK_IN: (self.clock_in, 0)}
        for a, (fn, pop) in stubs.items():
            z.native_stub(a, fn, pop)
        self.game = z.alloc(0x100)
        self.mech = z.alloc(0x800)

    def words(self, a, n):
        return list(struct.unpack(f'<{n // 4}i', bytes(self.uc.mem_read(a, n // 4 * 4))))

    def send(self, c):
        n = c.arg(1) & 0xffff
        self.log += [1, s32(c.arg(3)), 1 if c.arg(2) & 0xff else 0, n] + self.words(c.arg(0), n)
        return 1

    def clock_in(self, c):
        self.log += [7, s32(c.arg(0))] + self.words(c.arg(1), 12)
        return 1

    def run(self, v):
        self.v, self.log = v, []
        self.uc.mem_write(RUNNING, b'\x01')
        self.uc.mem_write(self.game + 0x90, struct.pack('<i', v[2]))
        self.uc.mem_write(CLOCK_LEFT, struct.pack('<i', v[3]))
        self.z.call(SEND_EVENT, ecx=self.mech, args=(0x11, self.game))
        return self.log


def tick_case(rng):
    now = rng.uniform(10, 900)
    near = lambda: bits(now + rng.choice([-5, -0.5, 0, 0.5, 5]))  # noqa: E731
    return [bits(now), near(), rng.choice([0, 3, 0x18, 0x19, 0x1a, 40]), near(), int(rng.random() < 0.7),
            bits(rng.choice([0.0, 0.5, 2.9, 3.0, 3.1, 30.0, 59.9, 60.0, 61.0, 300.0])), rng.randrange(2),
            rng.randrange(2), rng.randrange(0, 10), rng.choice([0, 1, 2, 3, 7, -1]), rng.choice([0, 0, 0, 1, 2, 3, 5]),
            near(), rng.randrange(2), near(), rng.randrange(2), rng.randrange(1, 1000)]


def main():
    probe = test_pm_zar.load_pm_probe()
    probe.probe_pm_net_tick.argtypes = [ctypes.POINTER(I32), ctypes.POINTER(I32), I32]
    probe.probe_pm_net_clock_send.argtypes = [ctypes.POINTER(I32), ctypes.POINTER(I32), I32]
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    rng = random.Random(71)
    exe = Exe()
    seen = {}
    for i in range(count):
        v = tick_case(rng)
        theirs = exe.run(v)
        out = (I32 * 256)()
        n = probe.probe_pm_net_tick((I32 * N_TICK)(*v), out, 256)
        mine = list(out[:n])
        if mine != theirs:
            print(f'tick case {i} (host {v[7]}, clock on {v[4]}, reason {v[10]}):\n  exe  {theirs}\n  port {mine}',
                  flush=True)
            os._exit(1)
        ended = theirs[theirs.index(90) + 5]
        key = (v[10], ended, 4 in theirs[:theirs.index(90)])
        seen[key] = seen.get(key, 0) + 1
    print(f'{count} ticks identical; (reason before, after, game over): {dict(sorted(seen.items()))}', flush=True)
    exe2 = ExeSend()
    for i in range(500):
        v = [rng.randrange(2), rng.randrange(1, 9), rng.choice([0, 1, 2, 3, 5, 6, 9, 0x13]), bits(rng.uniform(-3, 900))]
        theirs = exe2.run(v)
        out = (I32 * 64)()
        n = probe.probe_pm_net_clock_send((I32 * 4)(*v), out, 64)
        mine = list(out[:n])
        if mine != theirs:
            print(f'send case {i} (host {v[0]}, reason {v[2]}):\n  exe  {theirs}\n  port {mine}', flush=True)
            os._exit(1)
    print('clock send: 500 identical (the reason in 3 bits, guaranteed once there is one, then applied here)', flush=True)
    os._exit(0)


if __name__ == '__main__':
    main()
