"""Pirate's Moon's respawn in a network game, 0x46fe80 (MW3's 0x46dde0), against the port's net_pilot_respawn
through probe_mech_net_respawn, as test_mech_net_respawn does MW3's. PM's offsets: the game's respawn, unlimited ammo
and the byte after at +0xa4..+0xa6, the mech's data at +0x6a8. What changed: the starts tried are "remote_N" for N
from the first (0x437a40, by the game's kind: stood in for here, piece 4 ports it) on, 16 of them, 8 when the
game's kind (+0x98) is over 1; MW3's were remote_0..15. The rest as MW3's: respawn off (the view's camera 4, +0xa6
cleared); otherwise the mech change, the team, the ammo, the options, the explosion's restart, the lobby slot, the
start farthest from the nearest remote mech (0x470110 and 0x537aa0, run as the exe's), the spread (0x470170, run),
the place, the 0x12 (the exe's sender vt+0x17c stood in for: the port's message reduced to its type), the sensor
range, the objectives, the force feedback; then the answer, the team masks and +0xa6.

    cmake --build build --target pmprobe && MW3_EXE=pm .venv/bin/python tests/differential/test_pm_net_respawn.py
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
RESPAWN, FF, MECHS, SPRINTF = 0x46fe80, 0x62c410, 0x629a5c, 0x5de658
STARTS = 40
N_IN = 22 + STARTS * 5 + 2
KIND, BASE = 22 + STARTS * 5, 22 + STARTS * 5 + 1


def s32(v):
    return struct.unpack('<i', struct.pack('<I', v & 0xffffffff))[0]


def bits(f):
    return struct.unpack('<i', struct.pack('<f', f))[0]


class Exe:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        f = z.cpu.code(b'\xc3')
        self.uc.mem_write(SPRINTF, struct.pack('<I', f))
        z.native_stub(f, self.sprintf)
        stubs = {0x47b710: (lambda c: self.game, 4), 0x408a70: (self.logged(10), 4),
                 0x47a6e0: (self.logged(11), 4), 0x5a8a30: (lambda c: self.v[3], 0),
                 0x436600: (self.team, 4), 0x4b40c0: (self.ammo, 4), 0x40a870: (self.code_(13), 0),
                 0x521050: (self.restart, 0), 0x498930: (self.slot_of, 4), 0x5cb260: (self.position, 0),
                 0x4f1810: (lambda c: 0, 0), 0x4f1880: (lambda c: 0, 0), 0x437a40: (self.base, 4),
                 0x4f19e0: (self.find, 4), 0x4e7170: (self.code_(17), 4), 0x4943e0: (self.code_(18), 0),
                 0x4343a0: (self.logged(19), 4), 0x434370: (self.logged(20), 4)}
        for a, (fn, pop) in stubs.items():
            z.native_stub(a, fn, pop)
        self.vt_place, self.vt_send = z.cpu.code(b'\xc3'), z.cpu.code(b'\xc3')
        z.native_stub(self.vt_place, self.place, 8)
        z.native_stub(self.vt_send, lambda c: self.log.extend([5, s32(c.arg(0)), s32(c.arg(1))]) or 0, 8)

    def w(self, a, v):
        self.uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def r(self, a):
        return struct.unpack('<I', bytes(self.uc.mem_read(a, 4)))[0]

    def put(self, data):
        a = self.z.alloc(len(data))
        self.uc.mem_write(a, data)
        return a

    def code_(self, code):
        return lambda c: self.log.append(code) or 0

    def logged(self, code):
        return lambda c: self.log.extend([code, s32(c.arg(0))]) or 0

    def sprintf(self, c):
        fmt = bytes(self.uc.mem_read(c.arg(1), 16)).split(b'\0')[0]
        assert fmt == b'remote_%d', fmt
        s = b'remote_%d' % s32(c.arg(2))
        self.uc.mem_write(c.arg(0), s + b'\0')
        return len(s)

    def team(self, c):
        self.log += [2, s32(c.arg(0))]
        return self.v[4]

    def ammo(self, c):
        self.log += [3, c.arg(0) & 0xff]
        return 0

    def restart(self, c):
        self.log += [14, s32(c.arg(0)), s32(c.arg(1))]
        return 0

    def slot_of(self, c):
        self.log += [7, s32(c.arg(0))]
        return self.slot_rec

    def base(self, c):
        self.log += [6, s32(c.arg(0))]
        return self.v[BASE]

    def position(self, c):
        return self.pos_of.get(c.ecx, self.pos_of[self.mech])

    def find(self, c):
        name = bytes(self.uc.mem_read(c.arg(0), 16)).split(b'\0')[0].decode()
        i = int(name[7:])
        self.log += [8, i]
        return self.starts[i] if 0 <= i < STARTS else 0

    def place(self, c):
        p = c.arg(0)
        self.log += [9] + list(struct.unpack('<3i', bytes(self.uc.mem_read(p, 12)))) + [s32(c.arg(1))]
        return 0

    def run(self, v):
        z, w = self.z, self.w
        self.v, self.log = v, []
        self.game = z.alloc(0x100)
        self.uc.mem_write(self.game + 0xa4, bytes([v[0], v[1], v[2], 0]))
        w(self.game + 0x98, v[KIND])
        self.slot_rec = z.alloc(0x40)
        w(self.slot_rec + 0x20, v[5])
        mvt = z.alloc(0x200)
        w(mvt + 0xe8, self.vt_place), w(mvt + 0x17c, self.vt_send)
        m = self.mech = z.alloc(0x800)
        w(m, mvt)
        data = z.alloc(0x240)
        w(m + 0x6a8, data), w(data + 0xc, v[6])
        if v[7]:
            w(data + 0x200, 0x5150)
            ck = z.alloc(0x10)
            w(ck + 4, 0x6160)
            w(data + 0x228, ck)
        pilot = z.alloc(0x100)
        w(pilot + 0x14, m)
        w(FF, z.alloc(16) if v[8] else 0)
        self.pos_of = {m: self.put(struct.pack('<3i', *v[19:22]))}
        head = z.alloc(0x10)
        nodes = []
        for k in range(v[9]):
            o = z.alloc(0x800)
            self.pos_of[o] = self.put(struct.pack('<3i', *v[10 + 3 * k:13 + 3 * k]))
            n = z.alloc(0x10)
            w(n + 8, o)
            nodes.append(n)
        ring = [head] + nodes
        for i, n in enumerate(ring):
            w(n, ring[(i + 1) % len(ring)]), w(n + 4, ring[i - 1])
        w(MECHS, head)
        self.starts = []
        for i in range(STARTS):
            s = v[22 + 5 * i:27 + 5 * i]
            if s[0]:
                r = z.alloc(0x40)
                self.uc.mem_write(r + 8, struct.pack('<4i', *s[1:5]))
                self.starts.append(r)
            else:
                self.starts.append(0)
        ok = z.call(RESPAWN, args=(pilot,)) & 0xff
        self.log += [21, ok, 12, s32(self.r(m + 0xc0)), s32(self.r(m + 0xc4)), s32(self.r(m + 0xc8)),
                     22, self.uc.mem_read(self.game + 0xa6, 1)[0]]
        return self.log


def port_log(lg):
    """The probe's log with each send reduced to the exe's stand-in sender's: [5, type, 0]."""
    out, i = [], 0
    while i < len(lg):
        if lg[i] == 1 and i + 4 < len(lg) and lg[i + 4] == (0x12 | lg[i + 3] << 16):  # type 0x12, its size
            n = lg[i + 3]
            out += [5, lg[i + 4] & 0xffff, 0]
            i += 4 + (n + 3) // 4
        else:
            out.append(lg[i])
            i += 1
    return out


def case(rng):
    v = [0] * N_IN
    v[0] = int(rng.random() < 0.8)
    v[1], v[2] = rng.randrange(2), rng.randrange(2)
    v[3] = rng.randrange(1, 9)
    v[4] = rng.choice([0, 1, 2, 5, 33, 70, 1000])
    v[5] = rng.randrange(10)
    v[6] = rng.randrange(19)
    v[7], v[8] = rng.randrange(2), rng.randrange(2)
    v[9] = rng.randrange(4)
    for k in range(10, 22):
        v[k] = bits(rng.uniform(-900, 900))
    for i in range(STARTS):
        s = 22 + 5 * i
        v[s] = int(rng.random() < 0.6)
        v[s + 1:s + 4] = [bits(rng.uniform(-900, 900)) for _ in range(3)]
        v[s + 4] = bits(rng.uniform(-3.2, 3.2))
    v[KIND] = rng.choice([0, 1, 2, 3, 4])
    v[BASE] = rng.choice([0, 8, 16, 24])
    return v


def main():
    probe = test_pm_zar.load_pm_probe()
    probe.probe_mech_net_respawn.argtypes = [ctypes.POINTER(I32), ctypes.POINTER(I32), I32]
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 1500
    exe = Exe()
    rng = random.Random(8)
    seen = {}
    for n in range(count):
        v = case(rng)
        theirs = [x if x is None else s32(x) for x in exe.run(v)]
        out = (I32 * 8192)()
        k = probe.probe_mech_net_respawn((I32 * N_IN)(*v), out, 8192)
        mine = port_log(list(out[:k]))
        if mine != theirs:
            i = next((j for j, (a, b) in enumerate(zip(mine, theirs)) if a != b), min(len(mine), len(theirs)))
            print(f'case {n} (kind {v[KIND]}, first {v[BASE]}, respawn {v[0]}) at {i}:\n'
                  f'  exe  {theirs[max(0, i - 8):i + 8]}\n  port {mine[max(0, i - 8):i + 8]}', flush=True)
            os._exit(1)
        tried = [theirs[j + 1] for j in range(2, len(theirs) - 1) if theirs[j] == 8 and theirs[j - 2] == 6 and theirs[j - 1] == v[3]]
        key = (v[0], len(tried), min(tried) if tried else None)
        seen[key] = seen.get(key, 0) + 1
    print(f'{count} respawns identical; (respawn on, starts tried, the first): {dict(sorted(seen.items(), key=str))}',
          flush=True)
    os._exit(0)


if __name__ == '__main__':
    main()
