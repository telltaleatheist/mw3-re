"""PM's power on/off (0x41dab0) and power-down completion (0x41d920)
against the exe, over every combination of the flags they read.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_power.py
"""
import ctypes
import itertools
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import pm_floors  # noqa: E402
import test_pm_zar  # noqa: E402

probe = test_pm_zar.load_pm_probe()
POWER, FINISH, MECH_VT = 0x41dab0, 0x41d920, 0x5dfaac
# (offset, kind) for the int fields in probe order (pending .. sound_flags)
INTS = [(0xe0, 'b'), (0xe1, 'b'), (0xe2, 'b'), (0x59c, 'i'), (0x5a8, 'b'), (0x5ac, 'b'),
        (0x5dc, 'i'), (0x5e0, 'i'), (0x5e4, 'i'), (0x5f8, 'i'), (0x5fc, 'i'), (0x6ac, 'i')]


class Emulated:
    def __init__(self):
        e = self.e = test_pm_zar.Emulated()
        self.uc = e.cpu._uc
        a = e.alloc
        self.mech, self.mvt, self.player, self.pvt = a(0x800), a(0x200), a(0x800), a(0x200)
        self.uc.mem_write(self.mvt, bytes(self.uc.mem_read(MECH_VT, 0x200)))
        self.vstub(self.mvt + 0x64, lambda c: self.remote, 0)
        self.vstub(self.pvt + 0xcc, lambda c: self.log(13, 0, 0), 8)
        ctl, vbt, vt = a(0x40), a(8), a(0x60)
        self.w(ctl + 4, vbt), self.w(vbt + 4, 0x10), self.w(ctl + 0x14, vt)
        self.vstub(vt + 0x50, lambda c: self.log(9, c.arg(0), 0), 4)
        self.ctl = ctl
        self.pl, self.mission = a(0x80), a(0x80)
        self.w(0x61874c, self.pl), self.w(0x62c40c, self.mission)
        self.w(self.mission + 0x14, self.player), self.w(self.player, self.pvt)
        self.screen_obj, self.pos, self.owner = a(0x10), a(0x10), a(0x40)
        self.contact = a(0x10)
        n = e.native_stub
        n(0x430e30, lambda c: self.screen_obj if self.screen else 0)
        n(0x43cbc0, lambda c: self.log(10, self.byte, 0) or self.byte)
        n(0x43cbf0, lambda c: self.log(11, c.arg(0) & 0xff, 0), 4)
        n(0x58fe70, lambda c: self.log(3, c.arg(0), 0))
        n(0x5cb260, lambda c: self.pos)
        n(0x432d10, self.play, 0x14)
        n(0x41f8d0, lambda c: self.log(12, 0, 0), 4)
        n(0x4f8d80, lambda c: self.mech)

    def w(self, a, v):
        self.uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def vstub(self, slot, fn, pop):
        addr = self.e.cpu.code(b'\xc3')
        self.w(slot, addr)
        self.e.native_stub(addr, fn, pop)

    def log(self, *e):
        self.events.extend(e)
        return 0

    def play(self, c):
        h = self.next_sound
        self.next_sound += 1
        self.log(2, c.arg(0), h)
        return h

    def run(self, op, f, i):
        m, u = self.mech, self.uc
        u.mem_write(m, bytes(0x800))
        self.w(m, self.mvt)
        self.w(m + 0x38c, self.ctl)
        self.w(m + 0x6a8, self.owner)
        u.mem_write(m + 0x3c, struct.pack('<3f', *f))
        for (off, kind), v in zip(INTS, i):
            u.mem_write(m + off, bytes([v & 0xff]) if kind == 'b' else struct.pack('<i', v))
        self.w(self.pl + 0x64, m if i[12] else 0)
        self.screen, self.byte, self.remote = i[13], i[14], i[15]
        self.w(self.player + 0x6c, self.contact if i[16] else 0)
        self.events, self.next_sound = [], 100
        if op == 2:
            self.e.call(FINISH, ecx=m)
        else:
            self.e.call(POWER, ecx=m, args=(op,))
        fo = list(struct.unpack('<3f', bytes(u.mem_read(m + 0x3c, 12))))
        io = []
        for off, kind in INTS[:11]:
            raw = bytes(u.mem_read(m + off, 1 if kind == 'b' else 4))
            io.append(raw[0] if kind == 'b' else struct.unpack('<i', raw)[0])
        return fo, io, self.events


def main():
    emu = Emulated()
    cases = 0
    for op in (0, 1, 2):
        for pending, shut, down, anim, nopd, flags, player, screen, remote, target in itertools.product(
                (0, 1), (0, 1), (0, 1), (0, 1, 10, 12, 13, 5), (0, 1), (0, 1), (0, 1), (0, 1), (0, 1), (0, 1)):
            f = [1.5, -2.0, 3.25]
            i = [pending, shut, down, anim, nopd, 7, 11, 0, 13, 0, 0, flags, player, screen, 0x5a, remote, target]
            want = emu.run(op, f, i)
            pf, pi, plog = (ctypes.c_float * 3)(*f), (ctypes.c_int32 * 17)(*i), (ctypes.c_int32 * 64)()
            n = probe.probe_power(op, pf, pi, plog, 64)
            got = (list(pf), list(pi)[:11], list(plog)[:n])
            if want != got:
                raise SystemExit(f'op {op} case {i}:\n  original {want}\n  port     {got}')
            cases += 1
    other = 0
    for player, v in itertools.product((0, 1), (0, 1, 2, -1)):  # 0x41d880
        m = emu.mech
        emu.w(emu.mission + 0x14, m if player else emu.player)
        emu.uc.mem_write(m + 0x65c, bytes(4))
        emu.e.call(0x41d880, ecx=m, args=(v & 0xffffffff,))
        want = struct.unpack('<i', bytes(emu.uc.mem_read(m + 0x65c, 4)))[0]
        got = probe.probe_pm_hud_fall(player, v)
        if want != got:
            raise SystemExit(f'hud fall {player} {v}: original {want} port {got}')
        other += 1
    emu.w(emu.mission + 0x14, emu.player)
    for viewed, on, state in itertools.product((0, 1), (0, 1), (0, 1, 2, 3, 4, 0x1000000, 0x1000001, 0x1000004)):  # 0x419fb0
        m = emu.mech
        emu.w(emu.pl + 0x64, m if viewed else 0)
        emu.uc.mem_write(m + 0x6c0, struct.pack('<i', state))
        emu.uc.mem_write(m + 0x634, bytes(4))
        emu.events, emu.next_sound = [], 100
        emu.e.call(0x419fb0, ecx=m, args=(on,))
        want = (struct.unpack('<i', bytes(emu.uc.mem_read(m + 0x6c0, 4)))[0], struct.unpack('<i', bytes(emu.uc.mem_read(m + 0x634, 4)))[0])
        snd = ctypes.c_int32()
        got = (probe.probe_pm_heat_override(viewed, on, state, ctypes.byref(snd)), snd.value)
        if want != got:
            raise SystemExit(f'override {viewed} {on} {state:#x}: original {want} port {got}')
        other += 1
    pm_floors.floors(cases=(cases, 9216), others=(other, 40))
    print(f'ok: {cases} power cases agree')


if __name__ == '__main__':
    main()
