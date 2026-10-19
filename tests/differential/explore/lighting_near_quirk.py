# Exploration (lighting draft, 2026-10-19): a point light's falloff on a
# polygon's vertices at several distances in the original 0x545720, with the
# defaults of a new light (near 32, far 64). Prints the point-light term +4.
import struct, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import os
os.environ.setdefault('MW3_X87', '24')
from test_lighting import Emu, f32, floats  # noqa: E402

emu = Emu()
d = bytearray(0xd0)
struct.pack_into('<3f', d, 0x78, 0.0, 0.0, 100.0)            # light at z = 100
struct.pack_into('<5f', d, 0x9c, 1.0, 0.0, 1.0, 1.0, 1.0)    # diffuse 1, ambient 0, white
struct.pack_into('<I', d, 0xb0, 0x212)                       # ranged, positioned, on
struct.pack_into('<5f', d, 0xb4, 32.0, 64.0, 1024.0, 4096.0, 1 / 32)
emu.setup(bytes(d), [0x108011c], 1, (0, 0, 0))
dists = [1, 4, 5, 5.6, 6, 10, 20, 31, 32, 33, 40, 50, 63, 70]
# One polygon whose vertices sit at those distances in front of the light.
cam = [c for r in dists for c in (0.0, 0.0, 100.0 - r)]
emu.reaching((0, 0, 70), 40.0, 0, 1, [0.0] * 1024)  # partly inside near: not "full"
state = [0.0] * 37
state[9:21] = [1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0]
r = emu.shade(None, [0, 0, -1], len(dists), [0, 0], 0, 2, cam, 0, [0.0] * 3 * len(dists),
              [0.0] * 8 * len(dists), state)
for i, dist in enumerate(dists):
    print(f'distance {dist:5}: point light {r[3][8 * i + 1]:.3f}')
