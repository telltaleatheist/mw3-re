"""zEntity3d's node binding against the original x86 (docs/vehicles.md, The
node binding): the bind 0x40eaa0 (with 0x40eb10, 0x40eb30, 0x40eb80 and their
leaves) and the unbind 0x40ed00 (0x40e990, 0x40e960, 0x40e9d0, 0x40e9b0,
0x40e8f0, 0x40ed80, 0x40eca0 and theirs), run with the tree walks 0x40ea30 /
0x40ec20 as they are.

    cmake --build build && .venv/bin/python tests/differential/test_entity_bind.py [cases]

Each case: a tree of 1 to 12 nodes (depth up to 4) with kind words drawn from
the bits that matter (16..22, 0xf000's, a part's low byte, mixed, 0, random),
owners among six objects or none, random flags; six objects whose vtables
answer the casts (vt+0x40, +0x48, +0x50, +0x60, +0xdc, +0xe4, +0xec) with
another object or 0, and whose +0x04 is a part's mech (0x460000, stubbed to
return it). Then a bind or an unbind of the tree for one object. Compared: every
node's +0x28, +0x40 and +0x24.
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
BIND, UNBIND = 0x40eaa0, 0x40ed00
SLOTS = (0x40, 0x48, 0x50, 0x60, 0xdc, 0xe4, 0xec)
KINDS = [0, 0x10000, 0x20000, 0x40000, 0x80000, 0x100000, 0x200000, 0x400000, 0x1000, 0x2000, 0x05, 0x0b,
         0x80005, 0x41000, 0x300000, 0x60000]
COUNT = {'binds': 0, 'unbinds': 0, 'nodes changed': 0, 'cases with no change': 0}


def die():
    sys.stdout.flush()
    os._exit(1)


def run_case(probe, seed):
    r = random.Random(seed)
    t = Trace()
    e = Emu(t)
    objs = [e.world_alloc(0x20) for _ in range(6)]
    table = {}
    for o in objs:
        vt = e.world_alloc(0x100)
        e.w32(o, vt)
        mech4 = r.choice(objs + [0, 0])
        e.w32(o + 4, mech4)
        answers = {s: (r.choice(objs) if r.random() < 0.35 else 0) for s in SLOTS}
        for s in SLOTS:
            def fn(emu, key, a, pop, v=answers[s]):
                emu._ret(v, pop)
            e.w32(vt + s, e.new_stub(f'o{o:#x}.vt+{s:#x}', '', 0, fn))
        table[o] = [mech4] + [answers[s] for s in SLOTS]

    def part_mech(emu, key, a, pop):
        ecx = a[0]
        emu._ret(emu.r32(ecx + 4) if ecx else 0, pop)
    e.stub(0x460000, '0x00460000', 'c', None, part_mech)
    t.silent.add('0x00460000')
    n = r.randrange(1, 13)
    nodes = []
    for i in range(n):
        parent = -1 if i == 0 else r.randrange(0, i)
        kind = r.choice(KINDS) if r.random() < 0.85 else r.getrandbits(32)
        owner = r.choice(objs + [0])
        flags = r.getrandbits(32) & 0x3fff
        nodes.append([kind, owner, flags, parent])
    # A bind usually starts from an unbound tree; an unbind from a bound one.
    op = r.randrange(2)
    addrs = [e.world_alloc(0x100) for _ in range(n)]
    for i, (kind, owner, flags, parent) in enumerate(nodes):
        a = addrs[i]
        e.w32(a + 0x24, flags)
        e.w32(a + 0x28, kind)
        e.w32(a + 0x40, owner)
        kids = [addrs[j] for j in range(n) if nodes[j][3] == i]
        e.w32(a + 0x5c, len(kids))
        arr = e.world_alloc(4 * max(len(kids), 1))
        for k, c in enumerate(kids):
            e.w32(arr + 4 * k, c)
        e.w32(a + 0x60, arr)
    holder = e.world_alloc(8)
    e.w32(holder, addrs[0])
    self = r.choice(objs)
    e.call(BIND if op == 0 else UNBIND, ecx=holder, stack=(self,))
    got_a = []
    for a in addrs:
        got_a += [e.r32(a + 0x28), e.r32(a + 0x40), e.r32(a + 0x24)]
    flat_nodes = [v & 0xffffffff for nd in nodes for v in nd]
    flat_objs = []
    for o in objs:
        flat_objs += [o] + table[o]
    out = (U32 * (3 * n))()
    probe.probe_entity_bind(op, (U32 * len(flat_nodes))(*flat_nodes), n, (U32 * len(flat_objs))(*flat_objs), len(objs),
                            self, out)
    got_b = list(out)
    if got_a != got_b:
        print(f'seed {seed} ({"bind" if op == 0 else "unbind"}, {n} nodes, self {self:#x}):')
        for i in range(n):
            print(f'  node {i} in {[hex(v) for v in nodes[i][:3]]} parent {nodes[i][3]}: '
                  f'{[hex(v) for v in got_a[3 * i:3 * i + 3]]} / {[hex(v) for v in got_b[3 * i:3 * i + 3]]}')
        die()
    COUNT['binds' if op == 0 else 'unbinds'] += 1
    changed = sum(1 for i in range(n) if got_a[3 * i:3 * i + 3] != [nodes[i][0], nodes[i][1], nodes[i][2]])
    COUNT['nodes changed'] += changed
    COUNT['cases with no change'] += changed == 0


def main():
    probe = load_probe()
    probe.probe_entity_bind.argtypes = [ctypes.c_int32, ctypes.POINTER(U32), ctypes.c_int32, ctypes.POINTER(U32),
                                        ctypes.c_int32, U32, ctypes.POINTER(U32)]
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 5000
    for seed in range(count):
        run_case(probe, seed)
    print(f'ok: {count} cases ({", ".join(f"{v} {k}" for k, v in COUNT.items())}), as the original')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()
