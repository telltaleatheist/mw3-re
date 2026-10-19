# Exploration (render-shadows, 2026-10-20): the colour 0x57ea20 gives a
# projected-light triangle (vertex colour times the light multiplier, rounded
# with the 1.5 * 2^52 trick) at 64-bit and 24-bit x87 precision.
import struct, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import os  # noqa: E402
os.environ['MW3_X87'] = '64'
import test_drawtable as dt  # noqa: E402

emu = dt.Emu()
cpu = emu.cpu
emu.setup([(0x1000 + i, 1, 1, 1) for i in range(16)])
emu.reset(0, 0, 0)
emu.set([6, 0, 2, 2, 1, 1, 1], [-1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0], [0, 0, 0], 0, dt.FORMATS[565])
light = cpu.alloc(36)
for cw, name in ((0x037f, '64-bit'), (0x007f, '24-bit')):
    cpu.fpu_control(cw)
    for mult in (0.5, 1.0, 1.4):
        cpu.write(light, struct.pack('<9f', *([mult] * 9)))
        work = b''.join(struct.pack('<8f', 0, 0, 0, 200.0, 120.0, 40.0, 0, 0) for _ in range(64))
        cpu.write(dt.WORK, work)
        cpu.write(dt.TL, bytes(64 * 32))
        cpu.call(0x57ea20, ecx=light, edx=0, stack_args=(3, emu.tex(0), 0, dt.fbits(1.0), 0))
        diffuse = struct.unpack_from('<I', cpu.read(dt.TL + 16, 4))[0]
        print(f'{name}: colour (200, 120, 40) x {mult} -> diffuse {diffuse:#010x}')
