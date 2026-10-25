"""zWeapon's glow on a vehicle against the original x86 (docs/vehicles.md, The
glow): the clear 0x5869d0, a hit 0x586a30, the end 0x586af0, with the light
pool's take 0x586b20 and return 0x586b70 run as they are. The x87 at 24 bits,
as in play.

    cmake --build build && .venv/bin/python tests/differential/test_vehicle_glow.py [cases]

Each case: a pool of 0 to 4 lights ([0x9a0ebc], linked through +0x40), one
to three glows (0x18 bytes each, the node after at +0x18) whose +0x08 starts
at 0 or a random value (nothing in the exe writes it but the clear),
definitions with FREEZE (+0x6c bit 10) or not and a colour (+0x1d4), then
up to 14 ops at random: hits with shares in range, past it, negative, NaN;
ends; clears. The zClass calls (the light's range 0x4fadf0, colour
0x4fb510, the world's add 0x4f2d50 and remove 0x4f2e00, the node's child
0x4eb9b0 / 0x4ebb40) are stubs both sides log. Compared after each op: the
answer, the glow's six words, the pool's head and its chain, the calls.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import Emu, Trace, compare, load_probe  # noqa: E402

U32 = ctypes.c_uint32
POOL, WORLD = 0x9a0ebc, 0x9a0ea0
CLEAR, HIT, END = 0x5869d0, 0x586a30, 0x586af0
COUNT = {'hits': 0, 'lights taken': 0, 'pool empty at a hit': 0, 'ends': 0, 'answers 2': 0, 'answers 1': 0,
         'answers 0': 0, 'clears': 0}


def die():
    sys.stdout.flush()
    os._exit(1)


def f2u(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


def chain(read, head):
    out, n = [], head
    while n and len(out) < 10:
        out.append(n)
        n = read(n)
    return out


def run_case(probe, seed):
    r = random.Random(seed)
    t = Trace()
    e = Emu(t)
    e.cpu.fpu_control(0x007f)
    for addr, key, spec in ((0x4fadf0, '0x004fadf0', 'c,s0,s1'), (0x4fb510, '0x004fb510', 'c,s0,s1,s2'),
                            (0x4f2d50, '0x004f2d50', 'd'), (0x4f2e00, '0x004f2e00', 'd'),
                            (0x4eb9b0, '0x004eb9b0', 'c,d'), (0x4ebb40, '0x004ebb40', 'c,d')):
        e.stub(addr, key, spec)
    e.w32(WORLD, e.world_alloc(0x40))
    lights = [e.world_alloc(0x48) for _ in range(r.randrange(0, 5))]
    for i, l in enumerate(lights):
        e.w32(l + 0x40, lights[i + 1] if i + 1 < len(lights) else 0)
    e.w32(POOL, lights[0] if lights else 0)
    probe.probe_glow_pool((U32 * max(len(lights), 1))(*lights), len(lights))
    glows = []
    for _ in range(r.randrange(1, 4)):
        g = e.world_alloc(0x1c)
        words = [r.getrandbits(32) & 0xfc, 0, 0, 0, 0, 0]
        if r.random() < 0.3:
            words[2] = f2u(r.choice([-1.0, -0.6, -0.5, -0.2, 0.0, 0.3, float('nan')]))
        node = e.world_alloc(0x100)
        e.cpu.write(g, struct.pack('<6I', *words))
        e.w32(g + 0x18, node)
        glows.append([g, words, node])
    defs = []
    for _ in range(2):
        d = e.world_alloc(0x1f4)
        flags = r.getrandbits(32) & ~0x400 | (0x400 if r.random() < 0.4 else 0)
        color = [f2u(r.uniform(0, 1)) for _ in range(3)]
        e.w32(d + 0x6c, flags)
        e.cpu.write(d + 0x1d4, struct.pack('<3I', *color))
        defs.append((d, flags, color))
    for k in range(r.randrange(1, 15)):
        gi = r.randrange(len(glows))
        g, words, node = glows[gi]
        roll = r.random()
        op = 1 if roll < 0.7 else 2 if roll < 0.9 else 0
        d, flags, color = r.choice(defs)
        amount = r.choice([r.uniform(0, 1), r.uniform(-1, 1), r.uniform(1, 3), 0.0, 1.0, float('nan')])
        pool_before = e.r32(POOL)
        t.reset()
        if op == 0:
            e.call(CLEAR, ecx=g)
            ret_a = 0
        elif op == 1:
            ret_a = e.call(HIT, ecx=d, edx=g, stack=(f2u(amount),))
        else:
            e.call(END, ecx=g)
            ret_a = 0
        got_a = list(struct.unpack('<6I', bytes(e.cpu.read(g, 24))))
        head_a = e.r32(POOL)
        chain_a = chain(lambda n: e.r32(n + 0x40), head_a)
        log_a = list(t.log)
        tb = Trace()
        probe.probe_app_set_callback(tb.callback())
        gw = (U32 * 7)(*words, node)
        head = U32()
        ret_b = probe.probe_glow_op(op, gw, d, flags, (U32 * 3)(*color), f2u(amount), ctypes.byref(head))
        got_b = list(gw)[:6]
        chain_b = chain(probe.probe_glow_link, head.value)
        what = f'seed {seed} op {k} ({["clear", "hit", "end"][op]}, glow {gi}, amount {amount}, pool {len(chain(lambda n: e.r32(n + 0x40), pool_before))})'
        if not compare(what, log_a, tb.log):
            die()
        if (op == 1 and (ret_a & 0xffffffff) != (ret_b & 0xffffffff)) or got_a != got_b or head_a != head.value or chain_a != chain_b:
            print(f'{what}: answer {ret_a}/{ret_b}, glow {[hex(x) for x in got_a]} / {[hex(x) for x in got_b]}, '
                  f'pool {hex(head_a)} {chain_a} / {hex(head.value)} {chain_b}')
            die()
        glows[gi][1] = got_a
        if op == 1:
            COUNT['hits'] += 1
            COUNT[f'answers {ret_a & 0xffffffff}'] += 1
            took = words[4] == 0 and got_a[4] != 0
            COUNT['lights taken'] += took
            COUNT['pool empty at a hit'] += words[4] == 0 and pool_before == 0
        elif op == 2:
            COUNT['ends'] += 1
        else:
            COUNT['clears'] += 1


def main():
    probe = load_probe()
    probe.probe_glow_pool.argtypes = [ctypes.POINTER(U32), ctypes.c_int32]
    probe.probe_glow_op.argtypes = [ctypes.c_int32, ctypes.POINTER(U32), U32, U32, ctypes.POINTER(U32), U32,
                                    ctypes.POINTER(U32)]
    probe.probe_glow_link.argtypes = [U32]
    probe.probe_glow_link.restype = U32
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 2000
    for seed in range(count):
        run_case(probe, seed)
    print(f'ok: {count} cases ({", ".join(f"{v} {k}" for k, v in COUNT.items())}), as the original')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()
