"""Pirate's Moon's group trigger 0x4b5f80 against the port's WeaponGroup::trigger (tests/probe_pm_group_fire.cpp), on
PM's exe: in group fire (the manager's mode 2) the best rated beam fires first, then the rest in order; a weapon whose
kind has already fired in the volley gets the FIRE choice 1 (the flash alone, 0x5a3680 running), else 0 (both); once
a beam has fired, the other beams get the ground effect switch [0xb0e44c] cleared when the group has more than one
beam. In the other modes the current weapon alone.

Random groups of up to 16 scripted weapons (beam or not, a rating, one of four kinds, firing or not): the exe's weapon
calls (vt+8 is_beam, vt+0x34 trigger) answer from the script and record, as the port's, the weapon, its kind's choice
and the switch at each trigger.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_group_fire.py [cases]
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

TRIGGER, GROUND = 0x4b5f80, 0xb0e44c

probe = ctypes.CDLL(str(next((ROOT / 'build').glob('libpmprobe.*'))))
I16, F16 = ctypes.c_int * 16, ctypes.c_float * 16
probe.probe_pm_group_fire.argtypes = [ctypes.c_int, I16, F16, I16, I16, ctypes.c_int, ctypes.c_void_p]
OUT = (ctypes.c_int32 * 128)()


class Exe:
    def __init__(self):
        e = self.e = test_pm_zar.Emulated()
        self.uc = e.cpu._uc
        self.log = []
        is_beam, trigger = e.cpu.code(b'\xc3'), e.cpu.code(b'\xc3')
        e.native_stub(is_beam, lambda c: self.script[self.ecx()][0], 0)
        e.native_stub(trigger, self.trigger, 0)
        self.vt = e.alloc(0x40)
        self.uc.mem_write(self.vt + 8, struct.pack('<I', is_beam))
        self.uc.mem_write(self.vt + 0x34, struct.pack('<I', trigger))
        self.defs = [e.alloc(0x200) for _ in range(4)]
        self.weapons = [e.alloc(0x40) for _ in range(16)]
        self.group, self.manager = e.alloc(0x54), e.alloc(0x40)

    def ecx(self):
        return self.uc.reg_read(UC_X86_REG_ECX)

    def r32(self, a):
        return struct.unpack('<i', bytes(self.uc.mem_read(a, 4)))[0]

    def trigger(self, c):
        w = self.ecx()
        _, kind, fires, i = self.script[w]
        self.log += [i, self.r32(self.defs[kind] + 0xdc), self.r32(GROUND)]
        return fires

    def run(self, ws, mode):
        self.script = {}
        for d in self.defs:
            self.uc.mem_write(d + 0xdc, struct.pack('<i', 5))
        self.uc.mem_write(GROUND, struct.pack('<i', 1))
        self.uc.mem_write(self.manager + 0x1c, struct.pack('<i', mode))
        g = struct.pack('<IIIiI', self.manager, 0, len(ws), 0, self.weapons[0] if ws else 0)
        g += b''.join(struct.pack('<I', self.weapons[i]) for i in range(len(ws)))
        self.uc.mem_write(self.group, g)
        for i, (beam, rating, kind, fires) in enumerate(ws):
            w = self.weapons[i]
            self.script[w] = (beam, kind, fires, i)
            self.uc.mem_write(w, struct.pack('<I', self.vt))
            self.uc.mem_write(w + 0x18, struct.pack('<f', rating))
            self.uc.mem_write(w + 0x30, struct.pack('<I', self.defs[kind]))
        self.log = []
        r = self.e.call(TRIGGER, ecx=self.group) & 0xff
        return self.log + [r, self.r32(GROUND)] + [self.r32(d + 0xdc) for d in self.defs]


def port(ws, mode):
    n = len(ws)
    beam, rating, kind, fires = I16(), F16(), I16(), I16()
    for i, w in enumerate(ws):
        beam[i], rating[i], kind[i], fires[i] = w
    k = probe.probe_pm_group_fire(n, beam, rating, kind, fires, mode, OUT)
    return [int(OUT[i]) for i in range(k)]


def case(r):
    n = r.choice([0, 1, 2, 3, 4, 5, 6, 8, 10, 16])
    ratings = [0.0, 0.0, 1.5, 2.5, 2.5, 4.0, -1.0, float('nan')]
    ws = [(int(r.random() < 0.5), r.choice(ratings) if r.random() < 0.7 else r.uniform(0, 6), r.randrange(4) if r.random() < 0.8 else 0,
           int(r.random() < 0.75)) for _ in range(n)]
    return ws, 2 if r.random() < 0.85 else r.choice([0, 1])


def main():
    cases = int(sys.argv[1]) if len(sys.argv) > 1 else 4000
    exe = Exe()
    r = random.Random(0x5f80)
    bad = repeats = first_beam = cleared = 0
    for n in range(cases):
        ws, mode = case(r)
        e = exe.run(ws, mode)
        p = port(ws, mode)
        trig = e[:-6]
        repeats += any(trig[i + 1] == 1 for i in range(0, len(trig), 3))
        cleared += any(trig[i + 2] == 0 for i in range(0, len(trig), 3))
        first_beam += bool(trig) and ws[trig[0]][0] == 1 and trig[0] != 0
        if e != p:
            bad += 1
            if bad <= 5:
                print(f'FAIL case {n} mode {mode}: {ws}\n  exe  {e}\n  port {p}')
    print(f'{cases} volleys ({repeats} with a repeated kind, {cleared} with a beam\'s ground effect cleared, {first_beam} '
          f'with a beam fired ahead of its place), {bad} differ')
    sys.stdout.flush()
    os._exit(1 if bad else 0)


if __name__ == '__main__':
    main()
