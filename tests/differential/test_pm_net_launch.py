"""Pirate's Moon's launch of the pilot into a network game, 0x46fb20 (MW3's 0x46da80), against the port's
net_launch_player through probe_mech_net_launch, as test_mech_net_launch does MW3's. PM's offsets: the game's
unlimited ammo and outside view at +0xa5, +0xa6, the mech's data at +0x6a8, its sent and acked at +0x690, +0x68c.
What changed: the player's record (kind 2, the skin, the roster, the loadout; made by 0x46fe30, run as the exe's) is
sent twice (0x5aa4a0, DirectPlay's SetPlayerData: once made, again after the ammo); the clock's last second is left
as it was (only [0x629a74] and [0x629a78] cleared); the start is "remote_<first - 1 + slot>" (the first from
0x437a40, by the game's kind: stood in for here, piece 4 ports it). The rest as MW3's: the team, the ammo, the
rescore (0x4374f0), the outside view, the slot, the spread (0x470170, run), the place, the force feedback. PM then
registers the network's handlers (0x46f030): logged on the exe's side and left out of the comparison, the next
piece's (the registrations move from the start to here).

    cmake --build build --target pmprobe && MW3_EXE=pm .venv/bin/python tests/differential/test_pm_net_launch.py
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
LAUNCH, FF, SPRINTF, FREE = 0x46fb20, 0x62c410, 0x5de658, 0x5de65c
NEXT_STATE, WARNED = 0x629a74, 0x629a78


def s32(v):
    return struct.unpack('<i', struct.pack('<I', v & 0xffffffff))[0]


def bits(f):
    return struct.unpack('<i', struct.pack('<f', f))[0]


class Exe:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        for slot, fn in ((SPRINTF, self.sprintf), (FREE, lambda c: 0)):
            f = z.cpu.code(b'\xc3')
            self.uc.mem_write(slot, struct.pack('<I', f))
            z.native_stub(f, fn)
        stubs = {0x47b710: (lambda c: self.game, 4), 0x4867f0: (self.dfn, 0),
                 0x5a8a30: (lambda c: self.local, 0), 0x4274d0: (lambda c: self.blob, 0),
                 0x436600: (self.team, 4), 0x4b40c0: (self.ammo, 4), 0x5aa4a0: (self.set_data, 0),
                 0x4374f0: (lambda c: self.log.append(5) or 0, 4), 0x408a70: (self.view, 4),
                 0x498930: (self.slot_of, 4), 0x437a40: (self.base, 4), 0x4f1810: (lambda c: 0, 0),
                 0x4f19e0: (self.find, 4), 0x4f1880: (lambda c: 0, 0), 0x434370: (self.ff, 4),
                 0x46f030: (lambda c: self.log.append(15) or 0, 0)}
        for a, (fn, pop) in stubs.items():
            z.native_stub(a, fn, pop)
        self.vt_slot, self.vt_place = z.cpu.code(b'\xc3'), z.cpu.code(b'\xc3')
        z.native_stub(self.vt_slot, lambda c: self.slot0, 4)
        z.native_stub(self.vt_place, self.place, 8)

    def w(self, a, v):
        self.uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def r(self, a):
        return struct.unpack('<I', bytes(self.uc.mem_read(a, 4)))[0]

    def sprintf(self, c):
        fmt = bytes(self.uc.mem_read(c.arg(1), 16)).split(b'\0')[0]
        assert fmt == b'remote_%d', fmt
        s = b'remote_%d' % s32(c.arg(2))
        self.uc.mem_write(c.arg(0), s + b'\0')
        return len(s)

    def dfn(self, c):
        self.log += [1, s32(c.arg(0))]
        return 0

    def team(self, c):
        self.log += [2, c.arg(0)]
        return self.team_v

    def ammo(self, c):
        self.log += [3, c.arg(0) & 0xff]
        return 0

    def set_data(self, c):
        pid, data, size = c.arg(0), c.arg(1), c.arg(2)
        self.log += [4, pid, size] + list(struct.unpack(f'<{size // 4}i', bytes(self.uc.mem_read(data, size // 4 * 4))))
        return 1

    def view(self, c):
        self.log += [6, c.arg(0)]
        return 0

    def slot_of(self, c):
        self.log += [7, c.arg(0)]
        return self.slot2

    def base(self, c):
        self.log += [16, c.arg(0)]
        return self.base_v

    def find(self, c):
        name = bytes(self.uc.mem_read(c.arg(0), 16)).split(b'\0')[0].decode()
        self.log += [8, int(name[7:])]
        return self.spawn if self.found else 0

    def place(self, c):
        p = c.arg(0)
        self.log += [9] + list(struct.unpack('<3i', bytes(self.uc.mem_read(p, 12)))) + [s32(c.arg(1))]
        return 0

    def ff(self, c):
        self.log += [10, c.arg(0)]
        return 0

    def run(self, v, words):
        z, w = self.z, self.w
        self.log = []
        local, roster, skin, team, start, unlimited, outside, found = v[:8]
        spawn, ff, self.base_v = v[8:12], v[12], v[13]
        self.local, self.team_v, self.found = local, team, found
        self.blob = z.alloc(4 * len(words))
        self.uc.mem_write(self.blob, struct.pack(f'<{len(words)}I', *words))
        self.game = z.alloc(0x100)
        vt = z.alloc(0x40)
        w(vt + 0x30, self.vt_slot), w(self.game, vt)
        self.uc.mem_write(self.game + 0xa5, bytes([unlimited, outside]))
        self.slot0 = z.alloc(0x20)
        w(self.slot0 + 0x10, skin)
        self.slot2 = z.alloc(0x30)
        w(self.slot2 + 0x20, start)
        self.spawn = z.alloc(0x20)
        self.uc.mem_write(self.spawn + 8, struct.pack('<4i', *spawn))
        mvt = z.alloc(0x100)
        w(mvt + 0xe8, self.vt_place)
        m, data = z.alloc(0x800), z.alloc(0x20)
        w(m, mvt), w(m + 0x6a8, data), w(data + 0xc, roster), w(m + 0xd4, z.alloc(16))
        self.uc.mem_write(m + 0x68c, b'\0')
        w(m + 0x690, 5)
        pilot = z.alloc(0x100)
        w(pilot + 0x14, m)
        w(FF, z.alloc(16) if ff else 0)
        w(NEXT_STATE, bits(7.0))
        self.uc.mem_write(WARNED, b'\x01')
        ok = z.call(LAUNCH, args=(pilot,)) & 0xff
        self.log += [11, ok, 12, s32(self.r(m + 0xc0)), s32(self.r(m + 0xc4)), s32(self.r(m + 0xc8)),
                     13, self.uc.mem_read(m + 0x68c, 1)[0], s32(self.r(m + 0x690)), 14, 1, s32(self.r(NEXT_STATE)),
                     self.uc.mem_read(WARNED, 1)[0]]
        return self.log


def main():
    probe = test_pm_zar.load_pm_probe()
    probe.probe_mech_net_launch.argtypes = [ctypes.POINTER(I32), ctypes.POINTER(ctypes.c_uint32), I32,
                                            ctypes.POINTER(I32), I32]
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 600
    exe = Exe()
    rng = random.Random(3)
    seen = {}
    for case in range(count):
        n = rng.randrange(0, 12)
        words = [0x34 + 0xc * n | rng.randrange(19) << 16] + [rng.getrandbits(32) for _ in range(12 + 3 * n)]
        v = [rng.randrange(1, 9), rng.randrange(19), rng.randrange(5), rng.choice([0, 1, 2, 5, 33, 70, 1000]),
             rng.randrange(10), rng.randrange(2), rng.randrange(2), int(rng.random() < 0.8)]
        v += [bits(rng.uniform(-900, 900)) for _ in range(3)] + [bits(rng.uniform(-3.2, 3.2))] + [rng.randrange(2)]
        v += [rng.choice([0, 8, 16, 24])]
        theirs = exe.run(v, words)
        tail = len(theirs) - 13  # the answer and the fields read after: 13 words
        registered = theirs[tail + 1] == 1  # only on success, just before the answer
        assert not registered or theirs[tail - 1] == 15, theirs[tail - 3:]
        if registered:  # the next piece's: the port registers at the start
            del theirs[tail - 1]
        out = (I32 * 4096)()
        k = probe.probe_mech_net_launch((I32 * len(v))(*v), (ctypes.c_uint32 * len(words))(*words), len(words), out,
                                        4096)
        mine = list(out[:k])
        if mine != theirs:
            i = next((j for j, (a, b) in enumerate(zip(mine, theirs)) if a != b), min(len(mine), len(theirs)))
            print(f'case {case} (found {v[7]}, slot {v[4]}, first {v[13]}) at {i}:\n'
                  f'  exe  {theirs[max(0, i - 8):i + 8]}\n  port {mine[max(0, i - 8):i + 8]}', flush=True)
            os._exit(1)
        size = (words[0] & 0xffff) + 0xc
        sends = sum(theirs[j:j + 3] == [4, v[0], size] for j in range(len(theirs) - 2))
        key = (v[7], sends, registered)
        seen[key] = seen.get(key, 0) + 1
    print(f'{count} launches identical; (start found, records sent, handlers registered): {dict(sorted(seen.items()))}',
          flush=True)
    os._exit(0)


if __name__ == '__main__':
    main()
