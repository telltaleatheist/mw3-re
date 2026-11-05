"""A shot in a network game (PM's sender 0x471210, kind 0x16): the record on the
mech's list, and PM's addition, the game's Total Shots +0xcc (0x471ad4) against the
exe. The pilot screen reads that count; MW3's sender has no such line.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_net_shot.py
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import pm_floors  # noqa: E402,F401
import test_pm_zar  # noqa: E402

probe = test_pm_zar.load_pm_probe()
probe.probe_mech_net_shot.argtypes = [ctypes.c_int32] * 4 + [ctypes.POINTER(ctypes.c_int32)]
probe.probe_mech_net_shot.restype = None
SENDER, RUNNING, MISSION = 0x471210, 0x629a88, 0x62c40c


class Exe:
    def __init__(self):
        e = self.e = test_pm_zar.Emulated()
        self.uc = e.cpu._uc
        a = e.alloc
        e.native_stub(0x5ce8c6, lambda c: e.alloc(c.arg(0)))
        self.game, gvt, mission = a(0x100), a(0x40), a(0x80)
        self.w(gvt + 0x28, e.cpu.code(b'\xb8\x03\0\0\0\xc3'))  # the type: 3, a network game
        self.w(self.game, gvt), self.w(mission + 0x6c, self.game), self.w(MISSION, mission)
        self.w(RUNNING, 1)
        self.mech, self.head = a(0x800), a(0x10)
        self.w(self.head, self.head), self.w(self.head + 4, self.head)
        self.w(self.mech + 0x69c, self.head)
        self.ammo = a(0x20)
        self.w(self.mech + 0xd4, self.ammo)

    def w(self, addr, v):
        self.uc.mem_write(addr, struct.pack('<I', v & 0xffffffff))

    def r(self, addr):
        return struct.unpack('<i', bytes(self.uc.mem_read(addr, 4)))[0]

    def shot(self, slot, locked, owner, state):
        a = self.e.alloc
        w = a(0x80)
        self.w(w + 0x20, slot)
        self.uc.mem_write(w + 0x60, bytes([locked]))
        if owner is not None:
            t = a(0x800)
            self.w(t + 0x76c, owner), self.w(w + 0x14, t)
        self.w(self.ammo + 4, state)
        ret = self.e.call(SENDER, ecx=self.mech, args=(0x16, w))
        node = self.r(self.head + 4)  # the newest, pushed at the back
        rec = self.r(node + 8)  # the node holds a 0x10-byte record
        return ret, self.r(self.mech + 0x6a0), self.r(self.game + 0xcc), self.r(rec), self.r(rec + 8), self.r(rec + 12)


def port(slot, locked, owner, state):
    out = (ctypes.c_int32 * 6)()
    probe.probe_mech_net_shot(slot, locked, -1 if owner is None else owner, state, out)
    return list(out)


def main():
    exe = Exe()
    rng = random.Random(1)
    bad = shots = 0
    for n in range(1, 41):
        slot, locked = rng.randrange(16), rng.randrange(2)
        owner = rng.choice((None, rng.randrange(1, 0x7fff)))
        state = rng.randrange(1000)
        ret, count, fired, r_slot, r_state, r_target = exe.shot(slot, locked, owner, state)
        # Each port call starts a new mech and counter: the exe's run up by one each.
        theirs = [ret, 1, fired - shots, r_slot, r_state, r_target]
        assert count == n and fired == n, (n, count, fired)
        shots = fired
        mine = port(slot, locked, owner, state)
        if mine != theirs:
            bad += 1
            print('differs', slot, locked, owner, state, 'mine', mine, 'exe', theirs)
    if bad:
        sys.exit(1)
    print(f'{shots} shots: the record and the count of each match the exe')


if __name__ == '__main__':
    main()
