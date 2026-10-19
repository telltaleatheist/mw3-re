# Exploration (render-shadows, 2026-10-20): the caster image builder 0x542e70
# (with its node walk 0x4f7670 / 0x543330 and the software polygon filler
# 0x55d010) run in the original on c1 world objects, at 64-bit and at 24-bit
# x87 precision (the game's under Direct3D). Prints, per node and precision,
# how many shadow pixels come out other than the image's lightest colour.
import struct, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import os  # noqa: E402
os.environ['MW3_X87'] = '64'
import test_modeldraw as md  # noqa: E402
from test_gamez import SET_FORMAT  # noqa: E402

W = H = 64
orig = md.Original('c1')
cpu = orig.cpu
cpu.call(SET_FORMAT, ecx=5, edx=6, stack_args=(5, 0, 0, 0))
cpu.u32(0x800828, 0x564080)  # the span filler the software renderer's init installs (0x557bf2)
picked = []
for i in range(orig.i32(0x60bf34)):
    a = orig.node_addr(i)
    h = cpu.read(a, 0xd4)
    cls, = struct.unpack_from('<i', h, 0x34)
    model, = struct.unpack_from('<I', h, 0x3c)
    if cls == 5 and model and h[0x24] & 4:
        polys = struct.unpack('<i', cpu.read(model + 0x10, 4))[0]
        if 20 <= polys <= 400:
            picked.append(i)
picked = picked[::max(1, len(picked) // 12)][:12]
image = orig.emu.alloc(0x14)
pixels = orig.emu.alloc(W * H * 2)
cpu.write(image, struct.pack('<ihhIII', 0, W, H, 0, 0, pixels))
rec = orig.emu.alloc(0x90)
light = orig.emu.alloc(12)
cpu.write(light, struct.pack('<3f', 0.35, -0.8, 0.3))
ident = orig.emu.alloc(48)
cpu.write(ident, struct.pack('<12f', 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0))
for i in picked:
    node = orig.node_addr(i)
    out = []
    for cw, name in ((0x037f, '64-bit'), (0x007f, '24-bit')):
        cpu.fpu_control(cw)
        body = bytearray(0x8c)
        struct.pack_into('<I', body, 0, node)
        struct.pack_into('<I', body, 0x18, 0x8410)
        struct.pack_into('<I', body, 0x58, 1)
        struct.pack_into('<I', body, 0x5c, image)
        cpu.write(rec, bytes(body))
        cpu.write(pixels, b'\xee' * (W * H * 2))
        cpu.u32(md.STACK_SLOT, ident)
        cpu.u32(md.STACK_FLAG, 1)
        cpu.u32(md.STACK_TOP_P, md.STACK_SLOT)
        cpu.u32(md.STACK_FLAG_P, md.STACK_FLAG)
        centre_y = struct.unpack('<f', cpu.read(node + 0x68, 4))[0]
        cpu.call(0x542e70, ecx=rec, edx=light, stack_args=(struct.unpack('<I', struct.pack('<f', centre_y - 10))[0],))
        px = struct.unpack(f'<{W * H}H', cpu.read(pixels, W * H * 2))
        lightest = max(set(px), key=px.count)
        out.append(f'{name}: {sum(p != lightest for p in px)} shaded pixels, {len(set(px))} values '
                   f'({min(px):#06x}..{max(px):#06x})')
    print(f'node {i}: ' + '; '.join(out))
