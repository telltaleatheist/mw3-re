"""What 0x529ce0 does with vertices exactly on the clip rect's edges.

    .venv/bin/python tests/differential/explore/clip2d_edges.py

Runs the original on a few hand-made polygons in a 0..640 x 0..480 rect and
prints the vertices that come out (docs/drafts/clip2d.md, Quirks).
"""
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parents[1]))
from original import HEAP, Original  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

TL, RECT, COUNT, CW = 0x72f41c, HEAP, HEAP + 0x40, HEAP + 0x44
cpu = Original()
cpu.write(CW, struct.pack('<H', 0x007f))
fldcw = cpu.code(b'\xd9\x2d' + struct.pack('<I', CW) + b'\xc3')
cpu.write(RECT, struct.pack('<Iff4fff', 0, 0, 0, 0, 0, 0, 0, 640, 480))


def run(name, pts):
    cpu.call(fldcw)
    for i, (x, y) in enumerate(pts):
        cpu.write(TL + i * 0x20, struct.pack('<fff', x, y, 0.5))
    cpu.u32(COUNT, len(pts))
    cpu.call(0x529ce0, ecx=RECT, edx=COUNT)
    n = struct.unpack('<i', cpu.read(COUNT, 4))[0]
    ret = cpu.uc.reg_read(UC_X86_REG_EAX)
    out = [struct.unpack('<ff', cpu.read(TL + i * 0x20, 8)) for i in range(n)]
    print(f'{name}: {len(pts)} in -> ret {ret}, {n} out: {out}')


run('full-screen quad', [(0, 0), (640, 0), (640, 480), (0, 480)])
run('quad touching the left edge only', [(0, 10), (100, 10), (100, 100), (0, 100)])
run('quad touching the right edge only', [(540, 10), (640, 10), (640, 100), (540, 100)])
run('triangle with one vertex on the right edge', [(500, 10), (640, 50), (500, 100)])
run('quad wholly at x = 640', [(640, 10), (640, 10), (640, 100), (640, 100)])
run('sliver along x = 0', [(0, 10), (0, 100), (0, 200)])
