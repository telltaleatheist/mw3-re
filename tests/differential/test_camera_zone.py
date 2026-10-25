"""zClass's camera zone set 0x4f6c80 against the original x86 (docs/view.md
section 1).

    cmake --build build && .venv/bin/python tests/differential/test_camera_zone.py [cases]

The camera node (or none, or no data, or another class) with its +0x1e0 and
+0x1e4 random; the zone list 8 bytes (counts 0..5, ids 1..80, 0xff among
them at times, and what follows the four bytes, which a count over 3 reads).
Compared: the answer and the camera's two words after.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import Emu, Trace, load_probe  # noqa: E402

U32 = ctypes.c_uint32
COUNT = {'set': 0, 'kept (0xff)': 0, 'kept (none)': 0, 'refused': 0}


def die():
    sys.stdout.flush()
    os._exit(1)


def main():
    probe = load_probe()
    probe.probe_cz_set.argtypes = [ctypes.c_int32, ctypes.c_int32, ctypes.POINTER(U32), ctypes.POINTER(ctypes.c_uint8)]
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 5000
    e = None
    for seed in range(count):
        if seed % 500 == 0:
            e = Emu(Trace())
            e.stub(0x49bfd0, '@zerr', '')
        r = random.Random(seed)
        kind = r.choice([2] * 8 + [0, 1])
        cls = 1 if r.random() < 0.9 else r.choice([0, 5, 2])
        cam = [r.choice([0, 1, r.getrandbits(32)]), r.getrandbits(32)]
        n = r.choice([0, 1, 1, 2, 2, 3, 3, 4, 5])
        zone = [n] + [r.choice([1, 2, 22, 44, 71, r.randrange(1, 81), 0xff if r.random() < 0.3 else 1]) for _ in range(7)]
        node = e.world_alloc(0xd4)
        data = e.world_alloc(0x1e8)
        e.w32(node + 0x34, cls)
        e.w32(node + 0x38, 0 if kind == 1 else data)
        e.w32(data + 0x1e0, cam[0]), e.w32(data + 0x1e4, cam[1])
        z = e.world_alloc(8)
        e.cpu.write(z, bytes(zone))
        ret_a = e.call(0x4f6c80, ecx=0 if kind == 0 else node, edx=z)
        got_a = [e.r32(data + 0x1e0), e.r32(data + 0x1e4)]
        c = (U32 * 2)(*cam)
        ret_b = probe.probe_cz_set(kind, cls, c, (ctypes.c_uint8 * 8)(*zone))
        if (ret_a & 0xffffffff) != (ret_b & 0xffffffff) or got_a != list(c):
            print(f'seed {seed} (kind {kind}, class {cls}, zone {zone}): answer {ret_a}/{ret_b}, camera '
                  f'{[hex(x) for x in got_a]} / {[hex(x) for x in c]}')
            die()
        if ret_a & 0xffffffff:
            COUNT['refused'] += 1
        elif got_a != cam:
            COUNT['set'] += 1
        elif n and 0xff in zone[1:1 + n]:
            COUNT['kept (0xff)'] += 1
        else:
            COUNT['kept (none)'] += 1
    print(f'ok: {count} cases ({", ".join(f"{v} {k}" for k, v in COUNT.items())}), as the original')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()
