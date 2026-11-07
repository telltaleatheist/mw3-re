"""PM's dock state for a static MFB, the starbase refit (0x5e4c80: begin 0x45bc80, step 0x45bfb0, end 0x45c020, abort
0x45c080, the animation's end 0x45bcc0, with 0x45be50 and 0x45bd20) against mech3/mech_fieldbase_dock's DockRefit
(docs/vehicles.md, The starbase refit). Random set-ups, every call out and the state's fields compared.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_refit.py [trials]
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

os.environ.setdefault('MW3_X87', '53')
sys.path.insert(0, str(Path(__file__).parent))
import test_pm_zar  # noqa: E402

probe = test_pm_zar.load_pm_probe()
P = ctypes.c_void_p
probe.probe_pm_refit_reset.argtypes = [P, ctypes.c_float, ctypes.c_float]
probe.probe_pm_refit_op.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_uint32, ctypes.c_float, P, P]
BEGIN, STEP, END, ABORT, ANIM_END = 0x45bc80, 0x45bfb0, 0x45c020, 0x45c080, 0x45bcc0
VT = 0x5e4c80
GAME_VIEW, FRAME, GAME_TIME, TIME, QUIET = 0x61874c, 0x8c4b60, 0x8c1b50, 0x62da90, 0x629930
DRIVE, CONTACT = 0x62fbb2, 0x62fbb1
POINT = (10.0, 2.0, 30.0)


class Emulated(test_pm_zar.Emulated):
    def __init__(self):
        super().__init__()
        a = self.alloc
        self.reg, self.mech, self.point, self.view = a(0x200), a(0x800), a(0x40), a(0x80)
        self.mvt, self.pvt, self.vehicle, self.contact = a(0x200), a(0x40), a(0x40), a(0x20)
        self.list_head, self.name = a(16), a(16)
        self.pos_m, self.pos_p = a(12), a(12)
        self.cpu._uc.mem_write(self.name, b'pointname\0')
        self.poke(self.point + 0x2c, self.name)
        self.poke(self.point, self.pvt)
        self.poke(self.mech, self.mvt)
        self.st = self.reg + 0x150
        self.log = []
        self.anim, self.node, self.defn = 0x7000, 0x7100, 0x7200

        def slot(vt, off, fn):
            f = self.cpu.code(b'\xc3')
            self.poke(vt + off, f)
            self.native_stub(f, fn, 0)
        slot(self.mvt, 0xac, lambda c: self.s['powered'])
        slot(self.mvt, 100, lambda c: self.s['remote'])
        slot(self.pvt, 0x18, lambda c: self.pos_p)
        slot(self.pvt, 0x14, lambda c: 3)
        stub = self.native_stub
        stub(0x5cb260, lambda c: self.pos_m, 0)
        stub(0x5ce8c6, lambda c: self.alloc(c.arg(0)), 0)  # operator new
        stub(0x47b710, lambda c: self.s['mp'], 4)
        stub(0x4e7010, lambda c: self.contact if self.s['enemy'] else 0, 4)
        stub(0x4f8d80, lambda c: self.vehicle, 0)
        slot_v = self.alloc(0x200)
        self.poke(self.vehicle, slot_v)
        slot(slot_v, 0xb0, lambda c: self.name)
        stub(0x410750, lambda c: self.note(1), 0)
        stub(0x50b920, lambda c: self.node if self.s['node'] else 0, 0)
        stub(0x524b50, lambda c: self.find(c), 0)
        stub(0x521870, lambda c: self.effects(c), 0)
        stub(0x521bb0, lambda c: self.note(5) if c.arg(1) else None, 0)
        stub(0x408a70, lambda c: self.note(2, c.arg(0)), 4)
        stub(0x419c40, lambda c: self.note(3), 4)
        stub(0x472010, lambda c: 0, 0)
        stub(0x415fc0, lambda c: self.note(7), 4)
        stub(0x41dab0, lambda c: self.note(8), 4)
        stub(0x47ca00, lambda c: self.note(9), 0)
        stub(0x4f8ae0, lambda c: self.note(10, c.arg(0)), 0)

    def poke(self, a, v):
        self.cpu._uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def pokef(self, a, v):
        self.cpu._uc.mem_write(a, struct.pack('<f', v))

    def peekb(self, a):
        return self.cpu._uc.mem_read(a, 1)[0]

    def note(self, a, b=0):
        self.log.extend([a, b & 0xffffffff])
        return 0

    def find(self, c):
        name = bytes(self.cpu._uc.mem_read(c.arg(0), 16)).split(b'\0')[0]
        self.note(11, 1 if name == b'mfb_station_01' else 0)
        return self.defn if self.s['def'] else 0

    def effects(self, c):
        self.note(4, 1 if c.arg(1) else 0)
        return self.anim

    def reset(self, s):
        self.s = s
        self.cpu._uc.mem_write(self.reg, b'\0' * 0x200)
        self.cpu._uc.mem_write(self.mech + 0xe1, b'\0')
        self.poke(self.reg + 4, self.list_head)
        self.poke(self.list_head, self.list_head)
        self.poke(self.list_head + 4, self.list_head)
        self.poke(self.reg + 0x74, self.mech if s['mech'] else 0)
        self.poke(self.reg + 0x7c, self.point if s['point'] else 0)
        for i in range(11):
            self.poke(self.reg + 0x8c + 4 * i, 0x100 + i)
        self.poke(self.st, VT)
        self.poke(self.st + 8, self.reg)
        self.poke(GAME_VIEW, self.view)
        self.poke(self.view + 0x64, self.mech if s['viewed'] else self.vehicle)
        self.poke(self.view + 0x5c, 7)
        self.poke(self.contact + 0, 0)
        self.pokef(self.contact + 0xc, s['enemy_d2'])
        for k in range(3):
            self.pokef(self.pos_p + 4 * k, POINT[k])
            self.pokef(self.pos_m + 4 * k, (s['x'], 2.0, 30.0)[k])
        self.cpu._uc.mem_write(DRIVE, b'\0')
        self.cpu._uc.mem_write(CONTACT, b'\0')
        self.pokef(QUIET, 0.0)

    def op(self, op, arg, frame, time):
        self.log = []
        self.poke(FRAME, frame)
        self.pokef(GAME_TIME, time)
        self.pokef(TIME, time)
        st = self.st
        res = 0
        if op == 0:
            self.call(BEGIN, ecx=st)
        if op == 1:
            res = self.call(STEP, ecx=st) & 0xff
        if op == 2:
            self.call(END, ecx=st)
        if op == 3:
            self.call(ABORT, ecx=st)
        if op == 4:
            self.call(ANIM_END, ecx=st, args=(self.anim, arg))
        n = struct.unpack('<I', bytes(self.cpu._uc.mem_read(self.reg + 8, 4)))[0]
        return (self.log, [self.peekb(st + 4), int(self.r32(st + 0x18) != 0), self.peekb(DRIVE), self.peekb(CONTACT), self.r32(st + 0xc), n, self.peekb(self.mech + 0xe1)], res)


def port(op, arg, frame, time):
    log = (ctypes.c_uint32 * 64)()
    out = (ctypes.c_int * 7)()
    n = probe.probe_pm_refit_op(op, arg, frame, time, log, out)
    calls = []
    for k in range(n):  # the shutdown is a field in the exe, a hook here
        if log[2 * k] == 6:
            port.shut = log[2 * k + 1]
        else:
            calls += [log[2 * k], log[2 * k + 1]]
    return (calls, [out[1], out[2], out[3], out[4], out[5] & 0xffffffff, out[6], port.shut], out[0] if op == 1 else 0)


def main():
    trials = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    emu = Emulated()
    r = random.Random(1)
    started = ended = refused = 0
    for t in range(trials):
        s = {k: int(r.random() < p) for k, p in dict(powered=.8, mp=.3, enemy=.5, remote=.1, node=.3, viewed=.7, far=0, mech=.95, point=.95).items()}
        s['def'] = int(r.random() < .9)
        s['x'] = 10.0 + r.choice([1.0, 5.0, 6.3, 6.4, 6.43, 6.45, 6.5, 6.51, 8.0])
        s['enemy_d2'] = r.choice([100.0, 4999.0, 5000.0, 5001.0, 9000.0])
        emu.reset(s)
        flat = [s['powered'], s['mp'], s['enemy'], s['remote'], s['node'], s['def'], s['viewed'], s['far'], s['mech'], s['point']]
        probe.probe_pm_refit_reset((ctypes.c_int * 10)(*flat), s['enemy_d2'], s['x'])
        frame, time = 100, 10.0
        port.shut = 0
        for op, arg in [(0, 0), (1, 0), (1, 0)] + [(r.choice([1, 1, 1, 3, 4]), r.randint(0, 1)) for _ in range(6)] + [(2, 0)]:
            if op == 4 and not emu.r32(emu.st + 0x18):  # only a running animation ends
                continue
            frame += r.randint(1, 4)
            time += r.choice([0.1, 1.0, 3.0])
            want, got = emu.op(op, arg, frame, time), port(op, arg, frame, time)
            if want[0] != got[0] or want[1][:5] != got[1][:5] or want[1][6] != got[1][6] or want[1][5] != got[1][5] - 1:  # the port's list starts with its idle state
                print(f'trial {t} op {op}/{arg} {s}\n  exe  {want}\n  port {got}')
                sys.exit(1)
            started += want[0].count(4)
            ended += 7 in want[0]
            refused += 1 in want[0]
    probe.probe_pm_refit_done()
    print(f'ok: {trials} set-ups; {started} animations started, {ended} refits ended, {refused} refused for an enemy')
    if not started or not ended or not refused:
        sys.exit('a path never ran')


main()
