"""PM's model setters against its exe: the cycle's reset 0x55ba20 (the first polygon's materials), frame 0x55bb20,
looping 0x55bba0, speed 0x55bbc0 (each material in turn), its map 0x55bc10 and the priority 0x55b7e0. The script's cycled
textures and the animations' cycle event run on them (zinterp.cpp, zeff_anim_events.cpp); at 53 and 24 bits.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_model_set.py [cases]

Models of 1 to 5 polygons, each with 1 to 3 materials from up to 4, some sharing, some with a cycle of 1 to 5 textures.
Compared: the result, every material's texture and its cycle's index, speed, looping and slots, the priorities.
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import pm_floors  # noqa: E402
import test_pm_zar  # noqa: E402
from original import Original  # noqa: E402

I32 = ctypes.c_int32
probe = test_pm_zar.load_pm_probe()
probe.probe_pm_model_set.argtypes = [I32, ctypes.POINTER(I32), ctypes.POINTER(I32), I32, ctypes.POINTER(I32), I32, I32,
                                     ctypes.c_float, ctypes.POINTER(ctypes.c_uint32)]
probe.probe_pm_model_set.restype = I32
FN = {0: 0x55ba20, 1: 0x55bb20, 2: 0x55bbc0, 3: 0x55bba0, 4: 0x55bc10, 5: 0x55b7e0}


def fbits(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


def case(cpu, seed):
    r = random.Random(seed)
    npoly, nmat = r.randrange(1, 6), r.randrange(1, 5)
    cycle_n = [r.choice([0, 1, 2, 3, 5]) for _ in range(nmat)]
    list_n = [r.randrange(1, 4) for _ in range(npoly)]
    lists = [r.randrange(nmat) for _ in range(npoly * 3)]
    op = r.randrange(6)
    ia = r.choice([0, 1, 2, 3, 7, -1, r.randrange(-50, 50)]) if op != 4 else 0x7770 + seed % 16
    fa = r.choice([0.0, 0.5, 1.0, 10.0, r.uniform(-30, 30)])
    # The exe's.
    mats = cpu.alloc(0x2c * nmat)
    cycles = [0] * nmat
    for i in range(nmat):
        m = mats + 0x2c * i
        cpu.write(m, bytes(0x2c))
        cpu.write(m + 0x10, struct.pack('<I', 0x900 + i))
        if cycle_n[i]:
            t = cpu.alloc(4 * (cycle_n[i] + 1))
            cpu.write(t, struct.pack(f'<{cycle_n[i]}I', *[0x1000 * (i + 1) + k for k in range(cycle_n[i])]) + bytes(4))
            c = cycles[i] = cpu.alloc(0x1c)
            cpu.write(c, struct.pack('<IIffIII', 0, 0, 0.0, 0.0, cycle_n[i], cycle_n[i], t))
            cpu.write(m + 0x24, struct.pack('<I', c))
    polys = cpu.alloc(0x28 * npoly)
    for i in range(npoly):
        ml = cpu.alloc(12)
        cpu.write(ml, struct.pack('<3I', *[mats + 0x2c * lists[i * 3 + j] for j in range(3)]))
        cpu.write(polys + 0x28 * i, struct.pack('<10I', 0, 0, 0, 0, list_n[i], ml, 0, 0, 0, 0))
    model = cpu.alloc(0x68)
    cpu.write(model, bytes(0x68))
    cpu.write(model + 0x10, struct.pack('<I', npoly))
    cpu.write(model + 0x34, struct.pack('<I', polys))
    cpu.call(FN[op], stack_args=(model, fbits(fa) if op == 2 else ia & 0xffffffff) if op else (model,))
    got = []
    for i in range(nmat):
        got.append(struct.unpack('<I', bytes(cpu.read(mats + 0x2c * i + 0x10, 4)))[0])
        got += list(struct.unpack('<IIIII', bytes(cpu.read(cycles[i] + 8, 4)) + bytes(cpu.read(cycles[i] + 0xc, 4)) +
                                  bytes(cpu.read(cycles[i], 4)) + bytes(cpu.read(cycles[i] + 0x14, 4)) + b'\0' * 4)[:4]) if cycles[i] else [0, 0, 0, 0]
    got += [struct.unpack('<I', bytes(cpu.read(polys + 0x28 * i + 4, 4)))[0] for i in range(npoly)]
    # The port's.
    out = (ctypes.c_uint32 * (nmat * 5 + npoly))()
    probe.probe_pm_model_set(npoly, (I32 * npoly)(*list_n), (I32 * (npoly * 3))(*lists), nmat, (I32 * nmat)(*cycle_n), op, ia, fa, out)
    return op, got, list(out)


def main():
    cases = int(sys.argv[1]) if len(sys.argv) > 1 else 4000
    for cw in (0x027f, 0x007f):
        cpu = Original('pm')
        cpu.tr = lambda a, size=1: a
        cpu.fpu_control(cw)
        ops = [0] * 6
        arena = cpu.heap_next if hasattr(cpu, 'heap_next') else None
        for seed in range(cases):
            if arena is not None:
                cpu.heap_next = arena
            op, a, b = case(cpu, seed)
            ops[op] += 1
            if a != b:
                print('differs at seed', seed, 'op', op, '\n ', [hex(x) for x in a], '\n ', [hex(x) for x in b])
                sys.exit(1)
        print(f'fpu {cw:#x}: ok: {cases} models identical, by op {ops}')
        pm_floors.floors(reset=(ops[0], cases // 8), frame=(ops[1], cases // 8))


if __name__ == '__main__':
    main()
