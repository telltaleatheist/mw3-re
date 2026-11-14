"""Pirate's Moon's node re-read 0x517ac0 / 0x517b30 with its model re-read 0x558440 (the crater reset's: 0x518b70)
against the port's pm::zcls::reread (pm/gamez/zclass/cls_zbd.cpp, pm/gamez/zmodel/gmod_const.cpp), on PM's exe.

Each world read alone (the re-read runs only when exactly one gamez.zbd was read: [0x639c2c] == 1, its first in
[0x639c28]), as test_pm_gamez.py reads it on both sides; then a list of nodes re-read, alone and with their children,
cratered-ground nodes and others, some twice; then every model and node compared (the slots the new models take, the
old ones let go, each node's model). Two worlds read: the exe re-reads nothing (answer 1), and neither does the port.
Under Direct3D the exe hands each re-read model to its stripper (0x570b70, stubbed here and in test_pm_gamez; not
ported, docs/pm-map.md): the count is printed.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_reread.py
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_pm_gamez as g  # noqa: E402
import test_pm_zar  # noqa: E402

REREAD, POOL_READY = 0x517ac0, 0x8d4ae4


class Exe(g.Emulated):
    item_set = True  # +0xcc compared as set or not (its changed-list item, a pointer)

    def __init__(self):
        super().__init__()
        self.cpu.stub_import(g.IAT['fopen'], self.fopen)
        self.cpu.stub_import(g.IAT['fclose'], lambda c: 0)

    def fopen(self, c):
        self.pos = 0  # the world's bytes, as the stack left them
        return g.HANDLE

    def node_at(self, i):
        blocks = self.i32(g.N_BLOCKS) & 0xffffffff
        return (self.i32(blocks + 4 * (i // 2500)) & 0xffffffff) + 0xd4 * (i % 2500)


def picks(exe, rng):
    """(index, recurse): nodes with a model (+0x3c a pointer once read), ground ones (flags +0x24, 0x10000) first, some with
    their children (only where every node below has a model: the exe reads garbage for one without, which the port
    refuses: FIX (bug)), a few twice."""
    cap = exe.i32(g.N_CAPACITY)
    has = {}
    kids = {}
    for i in range(cap):
        s = exe.node_at(i)
        has[i] = exe.i32(s + 0x3c) != 0
        n = struct.unpack('<h', exe.read(s + 0x56, 2))[0]
        arr = exe.i32(s + 0x5c) & 0xffffffff
        kids[i] = [exe.pool_index(exe.i32(arr + 4 * k), g.N_BLOCKS, g.N_NBLOCKS, 2500, 0xd4) for k in range(max(n, 0))] if n > 0 else []

    def whole(i, depth=0):
        return depth < 50 and has.get(i, False) and all(whole(c, depth + 1) for c in kids[i])
    flags = lambda i: exe.i32(exe.node_at(i) + 0x24)  # +0x24 the flags; 0x10000 ground the craters touch
    with_model = [i for i in range(cap) if has[i]]
    ground = [i for i in with_model if flags(i) & 0x10000]
    rest = [i for i in with_model if not flags(i) & 0x10000]
    out = [(i, 0) for i in rng.sample(ground, min(25, len(ground)))]
    out += [(i, 0) for i in rng.sample(rest, min(15, len(rest)))]
    trees = [i for i in with_model if kids[i] and whole(i)]
    out += [(i, 1) for i in rng.sample(trees, min(6, len(trees)))]
    out += rng.sample(out, min(5, len(out)))
    return out, len(ground), len(trees)


def main():
    p = test_pm_zar.load_pm_probe()
    p.probe_pm_gamez_reread.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_void_p, ctypes.c_int]
    buf = ctypes.create_string_buffer(1 << 27)
    exe = Exe()
    rng = random.Random(0x517ac0)
    bad = rereads = stripped = 0
    cases = [[g.ZBD / w / 'gamez.zbd'] for w in g.WORLDS] + [[g.ZBD / 'c3' / 'gamez.zbd', g.ZBD / 'c1' / 'gamez.zbd']]
    for paths in cases:
        w = '+'.join(Path(x).parent.name for x in paths)
        path = '|'.join(map(str, paths))
        exe.stack(paths, 5, 6, 5, models=True, nodes=True)
        exe.w32(POOL_READY, 1)  # the model pool's ready flag, set in play (the bench's read leaves it 0)
        todo, nground, ntrees = picks(exe, rng)
        exe.converted = []
        answers = b''
        for i, r in todo:
            answers += struct.pack('<i', exe.signed(exe.call(REREAD, args=[exe.node_at(i), r])))
        stripped += sum(1 for kind, _ in exe.converted if kind == 'strips')
        exe.m_unread = exe.n_unread = 1 << 30
        head = answers + exe.model_dump()
        want = head + exe.node_dump()
        n = p.probe_pm_gamez_reread(path.encode(), '|'.join(f'{i},{r}' for i, r in todo).encode(), buf, len(buf))
        if n < 0:
            raise SystemExit(f'{w}: probe {n}')
        got = buf.raw[:n]
        rereads += len(todo)
        zeros = sum(1 for k in range(len(todo)) if struct.unpack_from('<i', answers, 4 * k)[0] == 0)
        if want != got:
            bad += 1
            at = next((k for k in range(min(len(want), len(got))) if want[k] != got[k]), min(len(want), len(got)))
            print(f'{w}: differ at byte {at} of {len(want)}/{len(got)}')
            print('exe: ', want[max(0, at - 16):at + 32].hex())
            print('port:', got[max(0, at - 16):at + 32].hex())
            if at >= len(head):  # which node, and where in its slot
                rel = at - len(head)
                mark = max((m for m in exe.node_marks if m[0] <= rel), default=None)
                if mark:
                    print(f'  node {mark[1]}, byte {rel - mark[0]} of its record')
        else:
            print(f'{w}: {len(todo)} re-reads ({zeros} read a model again; {nground} ground nodes, {ntrees} whole trees), identical')
    print(f'{len(cases)} cases (two worlds read: none re-read), {rereads} re-reads, {stripped} models the exe hands its stripper (Direct3D), '
          f'{bad} differ')
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
