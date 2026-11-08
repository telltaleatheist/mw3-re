"""Pirate's Moon's Pilot Statistics in multiplayer, the pilot screen's block at 0x491a1e (piece 6's end, pm-tests.md),
against multiplayer_stat_lines through probe_pm_stat_lines. The exe is entered at the block through a thunk that gives
it the frame its function made (0x110 bytes and four saved registers; esi the screen, ebp the multiplayer game with its
kills at +0xb4 and shots at +0xcc), so it leaves by its own epilogue. sprintf (the import [0x5de658]) is modelled with
C's rules (%d, %6.2f), the list's add 0x42fd80 logs each line, the label (the screen's +0x110 list empty) is skipped.
The x87 runs at 53 bits, the reference runtime's (CLAUDE.md); the rate is kills x 1000.0f / shots.

    cmake --build build --target pmprobe && MW3_EXE=pm .venv/bin/python tests/differential/test_pm_pilot_stats.py
"""
import ctypes
import os
import random
import re
import struct
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
os.environ['MW3_EXE'] = 'pm'
os.environ.setdefault('MW3_X87', '53')
import test_pm_zar  # noqa: E402

BLOCK, SPRINTF = 0x491a1e, 0x5de658


class Exe:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        f = z.cpu.code(b'\xc3')
        z.native_stub(f, self.sprintf)
        self.uc.mem_write(SPRINTF, struct.pack('<I', f))
        z.native_stub(0x596e50, lambda c: z.string('label'))
        z.native_stub(0x42fd80, lambda c: self.lines.append(z.cstr(c.arg(0)).decode('latin1')) or 0, 8)
        self.sc, self.g = z.alloc(0x200), z.alloc(0x100)
        thunk = (b'\x81\xec\x10\x01\x00\x00' + b'\x53\x55\x56\x57' + b'\xbe' + struct.pack('<I', self.sc) +
                 b'\xbd' + struct.pack('<I', self.g) + b'\xb8' + struct.pack('<I', BLOCK) + b'\xff\xe0')
        self.thunk = z.cpu.code(thunk)

    def sprintf(self, c):
        fmt = self.z.cstr(c.arg(1)).decode('latin1')
        k, out = 2, ''
        for part in re.split(r'(%[-0-9.]*[dfs])', fmt):
            if part.startswith('%') and part[-1] == 'd':
                out += part % struct.unpack('<i', struct.pack('<I', c.arg(k)))[0]
                k += 1
            elif part.startswith('%') and part[-1] == 'f':
                out += part % struct.unpack('<d', struct.pack('<II', c.arg(k), c.arg(k + 1)))[0]
                k += 2
            else:
                out += part
        self.uc.mem_write(c.arg(0), out.encode('latin1') + b'\0')
        return len(out)

    def run(self, kills, shots):
        self.lines = []
        self.uc.mem_write(self.sc + 0x110, bytes(8))
        self.uc.mem_write(self.g + 0xb4, struct.pack('<i', kills)), self.uc.mem_write(self.g + 0xcc, struct.pack('<i', shots))
        self.z.call(self.thunk)
        return self.lines


def main():
    probe = test_pm_zar.load_pm_probe()
    probe.probe_pm_stat_lines.argtypes = [ctypes.c_int32, ctypes.c_int32, ctypes.c_char_p, ctypes.c_int32]
    exe = Exe()
    rng = random.Random(0x491a1e)
    cases = [(0, 0), (1, 0), (0, 1), (1, 3), (2, 3), (1, 8), (5, 400), (-1, 7), (7, -3), (2 ** 31 - 1, 1), (1, 2 ** 31 - 1)]
    cases += [(rng.randrange(0, 200), rng.choice([0, rng.randrange(1, 50), rng.randrange(1, 20000)])) for _ in range(5000)]
    cases += [(k, s) for s in range(1, 200) for k in range(0, 40)]  # every rate near the roundings
    rated = 0
    for i, (kills, shots) in enumerate(cases):
        theirs = exe.run(kills, shots)
        buf = ctypes.create_string_buffer(512)
        probe.probe_pm_stat_lines(kills, shots, buf, 512)
        mine = buf.value.decode('latin1').split('\n')[:-1]
        if mine != theirs:
            print(f'case {i} kills {kills} shots {shots}:\n  exe  {theirs}\n  port {mine}', flush=True)
            os._exit(1)
        rated += shots != 0
    print(f'{len(cases)} pilot statistics identical ({rated} with a rate); e.g. {theirs}', flush=True)
    os._exit(0)


if __name__ == '__main__':
    main()
