"""Pirate's Moon's mech lab part highlight 0x451730 in the emulator, against the table the port's test
(tests/pm/test_pm_lab_highlight.cpp, pm_lab_highlight_lights_the_head_with_the_torso) expects of PM: the per-part
setter 0x4516d0 (tree, name, weight, on) is stubbed and each part's last call kept; the lab's tree (0x450fb0)
answered with a token. Sections 0..8: "-" off, "0" on at 0, "3" on at [0x606634] (0.3).

    .venv/bin/python -P tests/differential/test_pm_lab_highlight.py
"""
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_pm_zar  # noqa: E402

HIGHLIGHT, PART, TREE = 0x451730, 0x4516d0, 0x450fb0
PARTS = ('torso', 'larm', 'rarm', 'lthigh', 'lfoot', 'rthigh', 'rfoot', 'head')
# The port's PM table (test_pm_lab_highlight.cpp), section by section.
WANT = ('--------', '300----3', '-3------', '--3-----', '---33---', '-----33-', '--------', '300----3', '300----3')


def main():
    z = test_pm_zar.Emulated()
    u = z.cpu._uc
    state = {}

    def part(c):
        name = bytes(u.mem_read(c.arg(1), 16)).split(b'\0')[0].decode()
        weight = struct.unpack('<f', struct.pack('<I', c.arg(2)))[0]
        state[name] = '-' if not c.arg(3) else ('3' if weight > 0 else '0')
        return 0

    z.native_stub(PART, part, 0)
    z.native_stub(TREE, lambda c: 0x5150, 0)
    this = z.alloc(0x40)
    u.mem_write(this + 0x2c, struct.pack('<I', 1))
    bad = 0
    for s, want in enumerate(WANT):
        state.clear()
        z.call(HIGHLIGHT, ecx=this, args=(s,))
        got = ''.join(state.get(p, '?') for p in PARTS)
        print(f'section {s}: {got}' + ('' if got == want else f'  FAIL, the port wants {want}'))
        bad += got != want
    print(f'9 sections, {bad} differ')
    sys.stdout.flush()
    os._exit(1 if bad else 0)


if __name__ == '__main__':
    main()
