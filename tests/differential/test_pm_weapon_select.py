"""Pirate's Moon's weapon selection against the port's (tests/probe_pm_weapon_select.cpp), on PM's exe: the next and
previous group 0x4b43a0 / 0x4b4480 (a live weapon: a beam, one the weapons table says uses no ammo (0x486ab0), one with
an infinite bin (+0x5c's +0x0c), or ammo > 0; then the select), the select 0x4b4560 (a group whose current weapon isn't
ready (0x4b7bd0) moves on to the next ready one) and a group's next / previous ready weapon 0x4b5e10 / 0x4b5ea0 (chain
fire's step; MW3 took the next with ammo, not destroyed).

Random managers of five groups over up to 16 scripted weapons (beam, ammo -1..5, a table index, a bin limited or
infinite or none, enabled, reload, damage) and the manager's online flag: the exe's weapon calls (vt+8 is_beam, vt+0x20
ammo) answer from the script; the release 0x4b4710 is stubbed (the scripted weapons' release does nothing). Compared:
the result, the manager's group, each group's current index and weapon. First the port's ammo table against PM's
(0x60882c, 0x18 a row) for every weapon.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_weapon_select.py [cases]
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_pm_zar  # noqa: E402
from original import ROOT  # noqa: E402
from unicorn.x86_const import UC_X86_REG_ECX  # noqa: E402

NEXT_GROUP, PREV_GROUP, SELECT, GROUP_NEXT, GROUP_PREV, RELEASE = 0x4b43a0, 0x4b4480, 0x4b4560, 0x4b5e10, 0x4b5ea0, 0x4b4710
AMMO_TABLE = 0x60882c

probe = ctypes.CDLL(str(next((ROOT / 'build').glob('libpmprobe.*'))))
probe.probe_pm_weapon_select.argtypes = [ctypes.c_int, ctypes.c_int * 112, ctypes.c_int * 95, ctypes.c_int, ctypes.c_int,
                                         ctypes.c_int, ctypes.c_int, ctypes.c_void_p]
OUT = (ctypes.c_int32 * 16)()


class Exe:
    def __init__(self):
        e = self.e = test_pm_zar.Emulated()
        self.uc = e.cpu._uc
        is_beam, ammo = e.cpu.code(b'\xc3'), e.cpu.code(b'\xc3')
        e.native_stub(is_beam, lambda c: self.script[self.ecx()][0], 0)
        e.native_stub(ammo, lambda c: self.script[self.ecx()][1] & 0xffffffff, 0)
        e.native_stub(RELEASE, lambda c: 0, 0)
        self.vt = e.alloc(0x40)
        self.uc.mem_write(self.vt + 8, struct.pack('<I', is_beam))
        self.uc.mem_write(self.vt + 0x20, struct.pack('<I', ammo))
        self.def_ = e.alloc(0x100)
        self.weapons = [e.alloc(0xa0) for _ in range(16)]
        self.bins = [e.alloc(0x18) for _ in range(16)]
        self.groups = [e.alloc(0x60) for _ in range(5)]
        self.manager = e.alloc(0x90)

    def ecx(self):
        return self.uc.reg_read(UC_X86_REG_ECX)

    def r32(self, a):
        return struct.unpack('<i', bytes(self.uc.mem_read(a, 4)))[0]

    def w(self, a, fmt, *v):
        self.uc.mem_write(a, struct.pack(fmt, *v))

    def run(self, ws, gs, group, online, op, arg):
        self.script = {}
        for i, (beam, ammo, index, bin_, enabled, reload, damage) in enumerate(ws):
            a = self.weapons[i]
            self.uc.mem_write(a, bytes(0xa0))
            self.script[a] = (beam, ammo)
            self.w(a, '<I', self.vt)
            self.w(a + 0x2c, '<i', index)
            self.w(a + 0x30, '<I', self.def_)
            self.w(a + 0x34, '<f', reload / 1000.0)
            self.w(a + 0x5c, '<I', self.bins[i] if bin_ else 0)
            self.w(self.bins[i], '<iiiBB', 1, max(ammo, 0), 9, 1 if bin_ == 2 else 0, 0)
            self.w(a + 0x74, '<f', damage / 1000.0)
            self.w(a + 0x78, '<B', enabled)
            self.w(a + 0x9c, '<I', self.manager)
        self.uc.mem_write(self.manager, bytes(0x90))
        self.w(self.manager + 0x2c, '<i', group)
        self.w(self.manager + 0x8d, '<B', online)
        for g, (count, current, cw, ids) in enumerate(gs):
            a = self.groups[g]
            self.uc.mem_write(a, bytes(0x60))
            self.w(a, '<IIIiI', self.manager, 0, count, current, self.weapons[cw] if cw >= 0 else 0)
            for k, i in enumerate(ids):
                self.w(a + 0x14 + 4 * k, '<I', self.weapons[i])
            self.w(self.manager + 0x30 + 4 * g, '<I', a)
        if op == 0:
            r = self.e.call(NEXT_GROUP, ecx=self.manager) & 0xff
        elif op == 1:
            r = self.e.call(PREV_GROUP, ecx=self.manager) & 0xff
        elif op == 2:
            r = self.e.call(SELECT, ecx=self.manager, args=[arg]) & 0xff
        else:
            self.e.call(GROUP_NEXT if op == 3 else GROUP_PREV, ecx=self.groups[arg])
            r = 0
        out = [r, self.r32(self.manager + 0x2c)]
        index = {self.weapons[i]: i for i in range(16)}
        for g in range(5):
            cw = self.r32(self.groups[g] + 0x10) & 0xffffffff
            out += [self.r32(self.groups[g] + 0xc), index.get(cw, -1) if cw else -1]
        return out


def port(ws, gs, group, online, op, arg):
    wv = (ctypes.c_int * 112)()
    for i, row in enumerate(ws):
        for k, v in enumerate(row):
            wv[7 * i + k] = v
    gv = (ctypes.c_int * 95)()
    for g, (count, current, cw, ids) in enumerate(gs):
        gv[19 * g], gv[19 * g + 1], gv[19 * g + 2] = count, current, cw
        for k, i in enumerate(ids):
            gv[19 * g + 3 + k] = i
    n = probe.probe_pm_weapon_select(len(ws), wv, gv, group, online, op, arg, OUT)
    return [int(OUT[i]) for i in range(n)]


def case(r):
    nw = r.randint(1, 16)
    ws = [(int(r.random() < 0.3), r.choice([-1, 0, 0, 1, 3, 5]), r.choice([r.randrange(0x38), 0x0e, 0x17, 0x2a]),
           r.choice([0, 1, 1, 2]), int(r.random() < 0.9), r.choice([-1000, 0, 0, 0, 500]), r.choice([0, 0, 500, 1000]))
          for _ in range(nw)]
    gs = []
    for _ in range(5):
        count = r.choice([0, 1, 2, 3, 4, min(nw, 6)]) if r.random() < 0.8 else 0
        ids = [r.randrange(nw) for _ in range(count)]
        # With more than one weapon the current index is a weapon: PM's step reads a null weapon past the list and
        # calls the ready test on it (ECX 0 at 0x4b5e51), a crash in the exe; the port's guard skips it.
        current = r.randrange(count) if count > 1 or (count and r.random() < 0.9) else r.choice([-1, count])
        cw = ids[current] if 0 <= current < count else -1
        gs.append((count, current, cw, ids))
    op = r.choice([0, 1, 2, 3, 4])
    arg = r.randrange(5)
    return ws, gs, r.randrange(5), int(r.random() < 0.9), op, arg


def main():
    cases = int(sys.argv[1]) if len(sys.argv) > 1 else 6000
    exe = Exe()
    table = [exe.uc.mem_read(AMMO_TABLE + 0x18 * i, 1)[0] != 0 for i in range(0x38)]
    wrong = [i for i in range(0x38) if table[i] != bool(probe.probe_pm_weapon_uses_ammo(i))]
    if wrong:
        print(f'the ammo table differs at {wrong}')
        sys.stdout.flush()
        os._exit(1)
    r = random.Random(0x4b43)
    bad = moved = 0
    ops = [0] * 5
    for n in range(cases):
        ws, gs, group, online, op, arg = case(r)
        e = exe.run(ws, gs, group, online, op, arg)
        p = port(ws, gs, group, online, op, arg)
        ops[op] += 1
        moved += e[2:] != [v for g in gs for v in (g[1], g[2])]
        if e != p:
            bad += 1
            if bad <= 5:
                print(f'FAIL case {n} op {op} arg {arg} group {group} online {online}\n  ws {ws}\n  gs {gs}\n  exe  {e}\n  port {p}')
    print(f'{cases} cases (next group {ops[0]}, previous {ops[1]}, select {ops[2]}, a group\'s next {ops[3]}, previous {ops[4]}; '
          f'{moved} moved a group\'s weapon), the ammo table agrees on all 56, {bad} differ')
    sys.stdout.flush()
    os._exit(1 if bad else 0)


if __name__ == '__main__':
    main()
