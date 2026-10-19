"""The ported power on/off (0x41cc30) and power-down completion (0x41cac0)
against the original x86, over every combination of the flags they read.

    cmake --build build && .venv/bin/python tests/differential/test_power.py
"""
import ctypes
import itertools
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import ROOT, Original  # noqa: E402
from unicorn.x86_const import UC_X86_REG_ECX  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
POWER, FINISH = 0x41cc30, 0x41cac0
# (offset, kind) for the int fields in probe order (pending .. sound_flags)
INTS = [(0xdc, 'b'), (0xdd, 'b'), (0xde, 'b'), (0x578, 'i'), (0x584, 'b'), (0x587, 'b'),
        (0x5b8, 'i'), (0x5bc, 'i'), (0x5c0, 'i'), (0x5d4, 'i'), (0x5d8, 'i'), (0x688, 'i')]


class Emulated:
    def __init__(self):
        cpu = self.cpu = Original()
        self.mech = cpu.alloc(0x6d8)
        ctl, vbt, vt = cpu.alloc(0x40), cpu.alloc(8), cpu.alloc(0x60)
        cpu.u32(ctl + 4, vbt)
        cpu.u32(vbt + 4, 0x10)
        cpu.u32(ctl + 0x14, vt)
        stop = cpu.code(b'\xc2\x04\x00')
        cpu.u32(vt + 0x4c, stop)
        cpu.stub_function(stop, lambda c: self.log(9, c.arg(0), 0), 4)
        self.ctl = ctl
        self.pl = cpu.alloc(0x80)
        cpu.u32(0x5f4c8c, self.pl)
        self.screen_obj, self.pos = cpu.alloc(0x10), cpu.alloc(0x10)
        self.owner = cpu.alloc(0x40)
        cpu.stub_function(0x430aa0, lambda c: self.screen_obj if self.screen else 0)
        cpu.stub_function(0x43bb20, lambda c: self.log(10, self.byte, 0) or self.byte)
        cpu.stub_function(0x43bb50, lambda c: self.log(11, c.arg(0) & 0xff, 0), 4)
        cpu.stub_function(0x56e9f0, lambda c: self.log(3, c.uc.reg_read(UC_X86_REG_ECX), 0))
        cpu.stub_function(0x5a85e0, lambda c: self.pos)
        cpu.stub_function(0x432990, self.play, 0x14)

    def log(self, *e):
        self.events.extend(e)
        return 0

    def play(self, c):
        h = self.next_sound
        self.next_sound += 1
        self.log(2, c.arg(0), h)
        return h

    def run(self, op, f, i):
        cpu, m = self.cpu, self.mech
        cpu.write(m, bytes(0x6d8))
        cpu.u32(m, 0x5bcef8)
        cpu.u32(m + 0x37c, self.ctl)
        cpu.u32(m + 0x684, self.owner)
        cpu.write(m + 0x3c, struct.pack('<3f', *f))
        for (off, kind), v in zip(INTS, i):
            cpu.write(m + off, bytes([v & 0xff]) if kind == 'b' else struct.pack('<i', v))
        cpu.u32(self.pl + 0x64, m if i[12] else 0)
        self.screen, self.byte = i[13], i[14]
        self.events, self.next_sound = [], 100
        if op == 2:
            cpu.call(FINISH, ecx=m)
        else:
            cpu.call(POWER, ecx=m, stack_args=(op,))
        fo = list(struct.unpack('<3f', cpu.read(m + 0x3c, 12)))
        io = []
        for off, kind in INTS[:11]:
            raw = cpu.read(m + off, 1 if kind == 'b' else 4)
            io.append(raw[0] if kind == 'b' else struct.unpack('<i', raw)[0])
        return fo, io, self.events


def main():
    emu = Emulated()
    cases = 0
    for op in (0, 1, 2):
        for pending, shut, down, anim, nopd, flags, player, screen in itertools.product(
                (0, 1), (0, 1), (0, 1), (0, 1, 10, 12, 13, 5), (0, 1), (0, 1), (0, 1), (0, 1)):
            f = [1.5, -2.0, 3.25]
            i = [pending, shut, down, anim, nopd, 7, 11, 0, 13, 0, 0, flags, player, screen, 0x5a]
            want = emu.run(op, f, i)
            pf, pi, plog = (ctypes.c_float * 3)(*f), (ctypes.c_int32 * 15)(*i), (ctypes.c_int32 * 64)()
            n = probe.probe_power(op, pf, pi, plog, 64)
            got = (list(pf), list(pi)[:11], list(plog)[:n])
            if want != got:
                raise SystemExit(f'op {op} case {i}:\n  original {want}\n  port     {got}')
            cases += 1
    print(f'ok: {cases} power cases agree')


if __name__ == '__main__':
    main()
