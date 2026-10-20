# Exploration (render-shadows, 2026-10-20): each mech's shadow image as the
# original builds it (0x542e70 and its node walk, 0x543330, 0x55d010) on the
# mechlib.zbd trees in their rest pose, under each world's sun, at the game's
# 24-bit x87 and with 0x55d010 at 53 bits (the candidate FIX). Prints per mech:
# the models cast and those dropped whole because a vertex left the image
# (0x543330's bounds test), the polygons filled, and the shaded pixels.
import struct
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import test_mechlib as ml  # noqa: E402
import test_shadow_image as si  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn.x86_const import UC_X86_REG_ECX  # noqa: E402

data = ml.MECHLIB.read_bytes()
emu = si.ShadowEmu(data)
cpu = emu.cpu
count = {'models': 0, 'cast': 0, 'fills': 0}


def entered(uc, a, s, u):
    model = emu.u32(uc.reg_read(UC_X86_REG_ECX) + 0x3c)
    if model and emu.u32(model + 0x10):
        count['models'] += 1


cpu.uc.hook_add(UC_HOOK_CODE, entered, begin=0x543330, end=0x543330)
cpu.uc.hook_add(UC_HOOK_CODE, lambda uc, a, s, u: count.__setitem__('cast', count['cast'] + 1),
                begin=0x543575, end=0x543575)
cpu.uc.hook_add(UC_HOOK_CODE, lambda uc, a, s, u: count.__setitem__('fills', count['fills'] + 1),
                begin=si.FILL, end=si.FILL)
suns = si.real_suns()
black = 0  # pack(0, 0, 0): the darkest ramp
print(f'{"tree":28s} {"models":>6s} {"cast":>5s} {"dropped":>7s} {"fills":>6s} '
      + ' '.join(f'{w:>6s}' for w, *_ in suns) + '   (shaded pixels of 64x64 at 53 bits; 24 bits: all 0)')
for name, start, length in ml.entries(data):
    if not name.endswith('.flt') or not name.startswith('mech_'):
        continue
    row, totals = [], None
    for mode in ('game', 'fix'):
        for world, d, diffuse, ambient in suns:
            emu.start(start, mode)
            root = struct.unpack('<i', emu.ml.tree(start, 0)[4:8])[0]
            emu.start(start, mode)
            for k in range(emu.total):
                a = emu.node_addr(k)
                cpu.u32(a + 0x24, emu.u32(a + 0x24) & ~3)
                d38 = emu.u32(a + 0x38)
                if struct.unpack('<i', cpu.read(a + 0x34, 4))[0] == 5 and d38:
                    cpu.u32(d38, emu.u32(d38) | 0x20)
            for key in count:
                count[key] = 0
            cid = emu.op(0) or emu.op(1)
            emu.op(2, cid, 64, 64)
            emu.op(3, cid, root)
            emu.op(4, cid, 1)
            emu.op(5, cid, black)
            emu.op(6, cid, 0, 0, (0.0, 0.0, 0.0))
            r = emu.op(7, cid, 0, 0, list(d) + [0.0])
            img = emu.u32(emu.ids[cid] + 0x5c)
            pix = cpu.read(emu.u32(img + 0x10), 64 * 64 * 2)
            shaded = sum(pix[i:i + 2] != b'\xff\xff' for i in range(0, len(pix), 2))
            if mode == 'fix':
                row.append(shaded)
                if totals is None:
                    totals = dict(count)
            elif shaded:
                raise SystemExit(f'{name}: shaded at 24 bits')
    print(f'{name:28s} {totals["models"]:6d} {totals["cast"]:5d} {totals["models"] - totals["cast"]:7d} '
          f'{totals["fills"]:6d} ' + ' '.join(f'{v:6d}' for v in row))
