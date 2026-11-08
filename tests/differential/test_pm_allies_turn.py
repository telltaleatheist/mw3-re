"""PM's allies turning 0x4e6240 against the port's allies_turn_pm, on worlds of 0 to 12 entities: the player's masks
and the allies after him in the sorted range each to team 31 (0x80000000 team and allies, 0x7ffffffe enemies), the
sorted flag [0x62fc40] cleared, and the pick's byte (+0xed) bit 8 on each pilot reached through the mech (vt+0x48),
its controls (vt+0x14c), the brain (vt+0x58) and the brain's base at +0x100 (vt+0x88). Entities with no vehicle,
vehicles with no mech, controls, brain or pilot. docs/pm-tests.md, One hit turns a lancemate.
    cmake --build build --target pmprobe && MW3_EXE=pm .venv/bin/python tests/differential/test_pm_allies_turn.py [cases]
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

PILOT, LIST_END, LIST_BEGIN, SORTED = 0x62c40c, 0x62fc20, 0x62fc28, 0x62fc40
U32 = ctypes.c_uint32


class World:
    def __init__(self, z):
        self.z = z
        self.uc = z.cpu._uc
        self.vt = []        # (object, slot, answer)
        self.targets = []   # (object, team, allies, enemies)
        self.pilots = []    # (brain, pilot, byte)

    def w(self, a, v):
        self.uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def r(self, a):
        return struct.unpack('<I', bytes(self.uc.mem_read(a, 4)))[0]

    def ret(self, v):
        t = self.z.alloc(8)
        self.uc.mem_write(t, b'\xb8' + struct.pack('<I', v) + b'\xc3')  # mov eax, v; ret
        return t

    def obj(self, answers):
        o, table = self.z.alloc(0x200), self.z.alloc(0x200)
        self.w(o, table)
        for slot, v in answers.items():
            self.w(table + slot, self.ret(v))
            self.vt.append((o, slot, v))
        return o

    def target(self, o, team, allies, enemies):
        self.w(o + 0xc0, team), self.w(o + 0xc4, allies), self.w(o + 0xc8, enemies)
        self.targets.append((o, team, allies, enemies))


def build(seed, z):
    r = random.Random(seed)
    w = World(z)
    player = w.obj({})
    w.target(player, 2, r.choice([2, 0x60000002, 0x40000002]), r.choice([0x9ffffffc, 0xfffffffc]))
    pilot = z.alloc(0x40)
    w.w(pilot + 0x14, player), w.w(PILOT, pilot)
    n = r.choice([0, 1, 2, 3, 5, 8, 12])
    ents = []
    for i in range(n):
        x = 0
        if r.random() < 0.9:
            b = 0
            if r.random() < 0.85:
                pl = z.alloc(0x100) if r.random() < 0.9 else 0
                if pl:
                    z.cpu._uc.mem_write(pl + 0xed, bytes([r.choice([0, 1, 2, 3, 8, 0xff, 0x40])]))
                base = w.obj({0x88: pl})
                w.w(base + 4, 0)  # its first word is both the offset table (+4: 0) and the vtable
                b = z.alloc(0x200)
                w.w(b + 0x100, w.r(base))
                if pl:
                    w.pilots.append((b, pl, z.cpu._uc.mem_read(pl + 0xed, 1)[0]))
            c = w.obj({0x58: b}) if r.random() < 0.9 else 0
            m = w.obj({0x14c: c}) if r.random() < 0.9 else 0
            x = w.obj({0x48: m})
            team = r.choice([2, 2, 4, 8, 0x80000000])
            w.target(x, team, team | r.choice([0, 0x40000000, 2]), r.choice([0xbffffffc, 0x9ffffffa]))
        e = w.obj({0x2c: x}) if r.random() < 0.7 or not x else x
        if e == x and x:
            w.vt.append((x, 0x2c, x))
            w.w(w.r(x) + 0x2c, w.ret(x))
        ents.append(e)
    first = r.randrange(0, n + 1) if n else 0
    end = r.randrange(first, n + 1) if n else 0
    arr = z.alloc(4 * max(1, n))
    for i, e in enumerate(ents):
        w.w(arr + 4 * i, e)
    w.w(LIST_BEGIN, arr + 4 * first), w.w(LIST_END, arr + 4 * end)
    z.cpu._uc.mem_write(SORTED, b'\x01')
    return w, ents, first, end, player


def original(z, w):
    z.call(0x4e6240)
    out = []
    for o, *_ in w.targets:
        out += [w.r(o + 0xc0), w.r(o + 0xc4), w.r(o + 0xc8)]
    out += [z.cpu._uc.mem_read(pl + 0xed, 1)[0] for _, pl, _ in w.pilots]
    out.append(z.cpu._uc.mem_read(SORTED, 1)[0])
    return out


def rebuild(probe, w, ents, first, end, player):
    def arr(rows):
        flat = [v for row in rows for v in row]
        return (U32 * max(1, len(flat)))(*flat)
    out = (U32 * 256)()
    k = probe.probe_pm_allies_turn((U32 * max(1, len(ents)))(*ents), len(ents), first, end, player,
                                   arr(w.vt), len(w.vt), arr(w.targets), len(w.targets), arr(w.pilots), len(w.pilots), out)
    return list(out[:k])


def main():
    probe = test_pm_zar.load_pm_probe()
    probe.probe_pm_allies_turn.restype = ctypes.c_int32
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 600
    turned = 0
    for seed in range(count):
        z = test_pm_zar.Emulated()
        w, ents, first, end, player = build(seed, z)
        a = original(z, w)
        b = rebuild(probe, w, ents, first, end, player)
        if a != b:
            print(f'seed {seed} ({len(ents)} entities, range {first}..{end}): differs\n  original {[hex(v) for v in a]}\n'
                  f'  rebuild  {[hex(v) for v in b]}')
            sys.stdout.flush()
            os._exit(1)
        turned += end - first
    print(f'{count} worlds, {turned} allies in range: every mask, pilot byte and the sorted flag identical')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()
