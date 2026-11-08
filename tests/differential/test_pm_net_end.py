"""Pirate's Moon's network end, 0x46f150 (MW3's 0x46d100), against the port's net_end through probe_pm_net_end. Only
while a game runs ([0x629a88]): the handlers off (0x5a8c20: the in-game types', the player gone's, 1's, 0xe's and,
new, 0xd's, "I am the host"), the crater veto ([0x639c48]) and the flag cleared, the pilot's mech's end (0x41f0a0, when there is
one), each remote mech forgotten (0x4adc40), taken off the list (0x4c63f0) and deleted (vt+0), the player data
cleared (0x5aa4a0), the anim log's net hook off (0x526950(0, 0xc)), the force feedback stopped (0x434020, when
there is one). New in PM: one more round played ([0x62cc38]), the round's tally (0x436980(1) on the network game,
piece 3's), then each remote loadout's mech and loadout given back to the round store (0x475310, Store::give_config on its +0xd4)
and the local slot's (the game's vt+0x30(0): +4 the mech, +8 the loadout). Then the loadouts freed (0x425d50 and the
delete) and the missing list emptied. The calls out are stood in for and logged on both sides; the handlers taken off
are compared as a set with those the port's start and launch (net_register, 0x46f030) put on.

    cmake --build build --target pmprobe && MW3_EXE=pm .venv/bin/python tests/differential/test_pm_net_end.py
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
END = 0x46f150
RUNNING, VETO, PILOT, FF, ROUNDS = 0x629a88, 0x639c48, 0x62c40c, 0x62c410, 0x62cc38
MECHS, MECHS_N, CONFIGS, CONFIGS_END, CONFIGS_CAP, MISSING, MISSING_N = (0x629a5c, 0x629a60, 0x629a4c, 0x629a50,
                                                                        0x629a54, 0x629a6c, 0x629a70)
N_IN = 25


def s32(v):
    return struct.unpack('<i', struct.pack('<I', v & 0xffffffff))[0]


class Exe:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        stubs = {0x5a8c20: (lambda c: self.offs.append(c.arg(0)) or 0, 0),
                 0x41f0a0: (lambda c: self.log.append(70) or 0, 0),
                 0x4adc40: (lambda c: self.log.extend([51, self.owners[c.arg(0)]]) or 0, 0),
                 0x4c63f0: (self.erase, 8), 0x5a8a30: (lambda c: self.v[0], 0),
                 0x5aa4a0: (lambda c: self.log.extend([71, c.arg(0), 1 if c.arg(1) else 0, c.arg(2)]) or 0, 0),
                 0x526950: (self.anim, 0), 0x434020: (lambda c: self.log.append(72) or 0, 0),
                 0x47b710: (lambda c: self.game, 4), 0x436980: (self.tally, 4), 0x475310: (self.give, 8),
                 0x425d50: (lambda c: self.log.extend([73, self.tags[c.ecx]]) or 0, 0),
                 0x5ce8c0: (lambda c: 0, 0)}
        for a, (fn, pop) in stubs.items():
            z.native_stub(a, fn, pop)
        self.vt_del, self.vt_slot = z.cpu.code(b'\xc3'), z.cpu.code(b'\xc3')
        z.native_stub(self.vt_del, lambda c: self.log.extend([40, self.owners[c.ecx]]) or 0, 4)
        z.native_stub(self.vt_slot, lambda c: self.slot, 4)

    def w(self, a, v):
        self.uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def r(self, a):
        return struct.unpack('<I', bytes(self.uc.mem_read(a, 4)))[0]

    def erase(self, c):  # std::list erase(it): unlinks the node, the count down, the next into *out
        node = c.arg(1)
        nxt, prv = self.r(node), self.r(node + 4)
        self.w(prv, nxt), self.w(nxt + 4, prv)
        self.w(MECHS_N, self.r(MECHS_N) - 1)
        self.w(c.arg(0), nxt)
        return c.arg(0)

    def anim(self, c):
        self.anim_args = [1 if c.arg(0) else 0, c.arg(1) << 24 & 0xffffffff]  # as 0x526950 stores it
        return 0

    def tally(self, c):
        assert c.ecx == self.game and c.arg(0) & 0xff == 1, (hex(c.ecx), c.arg(0))
        self.log.append(25)
        return 1

    def give(self, c):
        assert c.ecx == self.game + 0xd4, hex(c.ecx)
        self.log += [26, s32(c.arg(0)), self.tags[c.arg(1)]]
        return 1

    def chain(self, head, items):
        prev = head
        for item in items:
            node = self.z.alloc(0xc)
            self.w(node + 8, item), self.w(prev, node), self.w(node + 4, prev)
            prev = node
        self.w(prev, head), self.w(head + 4, prev)

    def run(self, v):
        z, w = self.z, self.w
        self.v, self.log, self.offs, self.anim_args = v, [], [], [0, 0]
        self.uc.mem_write(RUNNING, bytes([v[1]]))
        w(VETO, v[1])  # the start's (the port's: set by its start)
        pilot = z.alloc(0x40)
        w(pilot + 0x14, z.alloc(0x10) if v[2] else 0)
        w(PILOT, pilot)
        w(FF, z.alloc(0x10) if v[3] else 0)
        vt = z.alloc(4)
        w(vt, self.vt_del)
        self.owners = {}
        mechs = []
        for k in range(v[4]):
            m = z.alloc(0x40)
            w(m, vt)
            self.owners[m] = v[5 + k]
            mechs.append(m)
        head = z.alloc(0xc)
        self.chain(head, mechs)
        w(MECHS, head), w(MECHS_N, len(mechs))
        self.tags = {}
        arr = z.alloc(4 * 8 + 4)
        for k in range(v[13]):
            cfg = z.alloc(0x40)
            w(cfg + 0x14, v[14 + k])
            self.tags[cfg] = k + 1
            w(arr + 4 * k, cfg)
        w(CONFIGS, arr), w(CONFIGS_END, arr + 4 * v[13]), w(CONFIGS_CAP, arr + 4 * 8)
        miss = z.alloc(0xc)
        self.chain(miss, [0] * v[22])
        w(MISSING, miss), w(MISSING_N, v[22])
        w(ROUNDS, v[23])
        self.game = z.alloc(0x100)
        gvt = z.alloc(0x40)
        w(self.game, gvt), w(gvt + 0x30, self.vt_slot)
        self.slot = z.alloc(0x20)
        slot_cfg = z.alloc(0x40)
        self.tags[slot_cfg] = 99
        w(self.slot + 4, v[24]), w(self.slot + 8, slot_cfg)
        z.call(END)
        offs = sorted(set(self.offs))
        assert not v[1] or len(self.offs) == 19, [hex(t) for t in self.offs]
        self.log += [61, len(offs)] + offs
        self.log += [90, self.uc.mem_read(RUNNING, 1)[0], s32(self.r(ROUNDS)),
                     (self.r(CONFIGS_END) - self.r(CONFIGS)) // 4, self.r(MISSING_N), self.r(MECHS_N),
                     1 if self.r(VETO) else 0] + self.anim_args
        return self.log


def case(rng):
    v = [0] * N_IN
    v[0] = rng.randrange(1, 9)
    v[1] = int(rng.random() < 0.85)
    v[2], v[3] = rng.randrange(2), rng.randrange(2)
    v[4] = rng.randrange(0, 9)
    v[5:5 + v[4]] = rng.sample(range(1, 12), v[4])
    v[13] = rng.randrange(0, 9)
    v[14:14 + v[13]] = [rng.choice([-1, 0, 5, 12, 0x19, 30, 47]) for _ in range(v[13])]
    v[22] = rng.randrange(0, 4)
    v[23] = rng.randrange(0, 6)
    v[24] = rng.choice([-1, 0, 7, 0x19, 40])
    return v


def main():
    probe = test_pm_zar.load_pm_probe()
    probe.probe_pm_net_end.argtypes = [ctypes.POINTER(I32), ctypes.POINTER(I32), I32]
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 800
    exe = Exe()
    rng = random.Random(41)
    seen = {}
    for i in range(count):
        v = case(rng)
        theirs = [s32(x) for x in exe.run(v)]
        out = (I32 * 4096)()
        n = probe.probe_pm_net_end((I32 * N_IN)(*v), out, 4096)
        mine = list(out[:n])
        if mine != theirs:
            j = next((k for k, (a, b) in enumerate(zip(mine, theirs)) if a != b), min(len(mine), len(theirs)))
            print(f'case {i} (running {v[1]}, mechs {v[4]}, loadouts {v[13]}) at {j}:\n'
                  f'  exe  {theirs[max(0, j - 8):j + 8]}\n  port {mine[max(0, j - 8):j + 8]}', flush=True)
            os._exit(1)
        key = (v[1], theirs.count(26) > 0)
        seen[key] = seen.get(key, 0) + 1
    print(f'{count} ends identical; (running, given back to the store): {dict(sorted(seen.items()))}', flush=True)
    os._exit(0)


if __name__ == '__main__':
    main()
