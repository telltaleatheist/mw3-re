"""Which torso section the original's 0x460350 picks for shots from every side.

    .venv/bin/python tests/differential/explore/damage_sections.py

A torso 2 wide and 1.5 deep (half sizes, so the centre strip is +-0.4) with an
identity matrix; shots come from 30 units away at each bearing and aim at a
point on the torso's centre line offset sideways by `aim`. The mech faces -z
(right-handed, +x to its right). Prints a table: rows are bearings (0 = from
straight ahead, 90 = from the mech's right), columns the sideways aim.
"""
import math
import os
import struct
import sys
from pathlib import Path

os.environ.setdefault('MW3_X87', '24')
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from original import FTOL, IAT_FTOL, Original  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

NAMES = ['CT front', 'LT front', 'RT front', 'CT rear', 'LT rear', 'RT rear']


def main():
    cpu = Original()
    cpu.import_code(IAT_FTOL, FTOL)
    torso, node, data = cpu.alloc(0x200), cpu.alloc(0x100), cpu.alloc(0x100)
    cpu.u32(torso, 0x5c29e0)
    cpu.u32(torso + 8, node)
    cpu.u32(node + 0x38, data)
    cpu.write(data + 0x60, struct.pack('<12f', 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0))
    cpu.write(torso + 0x7c, struct.pack('<2f', 2.0, 1.5))
    for off in (0x38, 0x1a0, 0x1a4):  # all sections intact
        cpu.write(torso + off, struct.pack('<f', 1.0))
    shot = cpu.alloc(0x20)
    aims = [-1.6, -0.8, 0.0, 0.8, 1.6]
    print('bearing  ' + ''.join(f'{a:>11}' for a in aims))
    for bearing in range(0, 360, 15):
        row = []
        t = math.radians(bearing)
        # ahead is -z, the mech's right +x
        sx, sz = 30 * math.sin(t), -30 * math.cos(t)
        for aim in aims:
            # the aim point: on the line through the centre across the shot
            ax, az = aim * math.cos(t), aim * math.sin(t)
            # shot[0] the shooter's side, shot[1] the point hit (0x583480's order)
            cpu.write(shot, struct.pack('<6f', sx + ax, 0, sz + az, ax, 0, az))
            cpu.call(0x460350, ecx=torso, stack_args=(shot, 0))
            row.append(NAMES[cpu.uc.reg_read(UC_X86_REG_EAX)])
        print(f'{bearing:>7}  ' + ''.join(f'{r:>11}' for r in row))


if __name__ == '__main__':
    main()
