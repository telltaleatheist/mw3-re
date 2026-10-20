"""The parts' set_pose and the leg IK against the original x86
(docs/movement-pose.md).

    cmake --build build && .venv/bin/python tests/differential/test_pose.py

The mech's twelve parts sit in emulator memory with their real vtables
(pose_world.py), each on a node whose matrix they write. Each trial gives
the legs a random geometry (thigh and calf lengths, the IK constants as
0x4637b0 makes them, the hip joint's place, the knee's direction), then one
call: a leg's IK 0x463e40 (with the solve 0x463c90, the thigh's and calf's
base set_pose 0x460280 and the foot's 0x4629b0), the solve alone, or a
part's set_pose (the base with and without a pending pose, the hip's
0x4624b0 with its inverse and the feet's ground updates, the foot's).
Every field both sides keep, every node matrix and every call out must
match bit for bit at 24-bit x87, ankles out of reach included.
"""
import os
import random
import struct
import sys
from pathlib import Path

os.environ.setdefault('MW3_X87', '24')
sys.path.insert(0, str(Path(__file__).parent))
import pose_world as W  # noqa: E402


def scenario(rng):
    f32 = W.f32
    parts = bytearray(W.IMAGE * W.NPARTS)
    nodes = bytearray(48 * W.NPARTS)
    for k in range(W.NPARTS):
        b = W.IMAGE * k
        W.put(parts, b + 0x4a, 'b', rng.random() < 0.15)
        for j in range(3):
            W.put(parts, b + 0x4c + 4 * j, 'f', f32(rng.uniform(-3, 3)))
        q = [rng.uniform(-1, 1) for _ in range(4)]
        n = sum(x * x for x in q) ** 0.5
        for j in range(4):
            W.put(parts, b + 0x58 + 4 * j, 'f', f32(q[j] / n))
        m = [rng.uniform(-1, 1) for _ in range(9)] + [rng.uniform(-4, 4) for _ in range(3)]
        struct.pack_into('<12f', nodes, 48 * k, *[f32(v) for v in m])
    for i in range(2):
        b = W.IMAGE * (W.LEG0 + i)
        a, c = f32(rng.uniform(2, 7)), f32(rng.uniform(2, 7))
        W.put(parts, b + 0x7c, 'b', rng.random() < 0.3)
        W.put(parts, b + 0xac, 'f', a)
        W.put(parts, b + 0xb8, 'f', c)
        W.put(parts, b + 0xb4, 'f', f32(rng.uniform(-2, 2)))
        k_ = f32(-(f32(c * c) + f32(a * a)))
        W.put(parts, b + 0xcc, 'f', k_)
        W.put(parts, b + 0xd0, 'f', f32(1.0 / f32(f32(c * a) + f32(c * a))))
    hb = W.IMAGE * W.HIP
    for j in range(3):
        W.put(parts, hb + 0x98 + 4 * j, 'f', f32(rng.choice([0.0, rng.uniform(-2, 0.5)])))
    mech = bytearray(W.MECH_SIZE)
    present = (1 << W.NPARTS) - 1
    if rng.random() < 0.1:
        present &= ~(1 << rng.randrange(W.NPARTS))
    return {'mech': bytes(mech), 'parts': bytes(parts), 'nodes': bytes(nodes), 'present': present}


def target(rng, s, leg):
    """An ankle position for a leg: near, at and beyond its reach."""
    b = W.IMAGE * (W.LEG0 + leg)
    a, c = struct.unpack_from('<f', s['parts'], b + 0xac)[0], struct.unpack_from('<f', s['parts'], b + 0xb8)[0]
    t = struct.unpack_from('<3f', s['nodes'], 48 * (W.LEG0 + leg) + 36)
    reach = (a + c) * rng.choice([0.2, 0.5, 0.8, 0.95, 1.0, 1.05, 1.5, rng.uniform(0, 2)])
    d = [rng.uniform(-1, 1) for _ in range(3)]
    n = sum(x * x for x in d) ** 0.5 or 1.0
    return [W.f32(t[i] + d[i] / n * reach) for i in range(3)]


def unit_q(rng):
    q = [rng.uniform(-1, 1) for _ in range(4)]
    n = sum(x * x for x in q) ** 0.5
    return [W.f32(x / n) for x in q]


def main():
    world = W.World()
    rng = random.Random(1999)
    trials = int(sys.argv[1]) if len(sys.argv) > 1 else 20000
    ops = set()
    for n in range(trials):
        s = scenario(rng)
        r = rng.random()
        if r < 0.55:
            op, leg = 0, rng.randrange(2)
            args = [float(leg)] + target(rng, s, leg) + unit_q(rng)
            fn, regs = 0x463e40, None
        elif r < 0.7:
            op, leg = 2, rng.randrange(2)
            args = [float(leg)] + target(rng, s, leg)
        else:
            op = 1
            k = rng.choice([W.LEG0, W.CALF1, W.FOOT0, W.FOOT1, W.HIP, W.HIP, W.ARM0, W.HAND1])
            hasp = k in (W.FOOT0, W.FOOT1, W.HIP) or rng.random() < 0.7
            hasq = rng.random() < 0.7
            args = [float(k), float(hasp)] + [W.f32(rng.uniform(-5, 5)) for _ in range(3)] + [float(hasq)] + unit_q(rng)
        world.load(s)
        cpu = world.cpu
        sp = cpu.alloc(64)
        if op == 0:
            cpu.write(sp, struct.pack('<7f', *args[1:8]))
            cpu.call(0x463e40, ecx=world.parts[W.LEG0 + leg], stack_args=(sp, sp + 12))
        elif op == 2:
            cpu.write(sp, struct.pack('<3f', *args[1:4]))
            cpu.call(0x463c90, ecx=world.parts[W.LEG0 + leg], stack_args=(sp, 0))
        else:
            k = int(args[0])
            cpu.write(sp, struct.pack('<3f', *args[2:5]) + struct.pack('<4f', *args[6:10]))
            vt = struct.unpack('<I', cpu.read(W.VTABLES[k] + 0x2c, 4))[0]
            cpu.call(vt, ecx=world.parts[k], stack_args=(sp if args[1] else 0, sp + 12 if args[5] else 0))
        want, wlog = world.store()
        W.port_load(s)
        W.port_op(op, args)
        got, glog = W.port_store()
        W.compare(f'trial {n} op {op} {args[:2]}', want, wlog, got, glog)
        ops.add((op, int(args[0]) if op == 1 else -1))
    print(f'ok: {trials} IK solves and set_poses identical bit for bit; {len(ops)} kinds')


if __name__ == '__main__':
    main()
