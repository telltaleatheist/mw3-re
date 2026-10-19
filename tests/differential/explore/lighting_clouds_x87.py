# Exploration (lighting draft, 2026-10-19): the cloud pattern sampler 0x543ac0
# in the original at 64-bit and at 24-bit x87 precision (the game's, under
# Direct3D). Prints how many distinct values each gives over random points.
import random, struct, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from original import Original  # noqa: E402

CLOUDS = 0x5ef860
for cw, name in ((0x037f, '64-bit'), (0x027f, '53-bit'), (0x007f, '24-bit')):
    cpu = Original()
    cpu.fpu_control(cw)
    out = cpu.alloc(16)
    stub = cpu.code_next
    body = b'\xff\x74\x24\x08\xff\x74\x24\x08\xe8' + struct.pack('<i', 0x543ac0 - (stub + 13))
    body += b'\xd9\x1d' + struct.pack('<I', out) + b'\xc3'
    cpu.code(body)
    # c1's fluctuate: period 8, wavelength 600, base 0.7, amplitude 0.3.
    cpu.u32(CLOUDS, 1)
    cpu.write(CLOUDS + 4, struct.pack('<8f', 6.2831855 / 8, 6.2831855 / 600, 0.7, 0.3, 0, 1, 1.0, 2.0))
    rng = random.Random(1)
    vals = set()
    for _ in range(2000):
        x, z = rng.uniform(-3000, 3000), rng.uniform(-3000, 3000)
        cpu.call(stub, stack_args=tuple(struct.unpack('<I', struct.pack('<f', v))[0] for v in (x, z)))
        vals.add(round(struct.unpack('<f', cpu.read(out, 4))[0], 6))
    print(f'{name}: {len(vals)} distinct values, min {min(vals)}, max {max(vals)}')
