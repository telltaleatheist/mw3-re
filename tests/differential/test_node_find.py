"""The finds by name the vehicle builder 0x4b4660 uses, against the original
x86 (docs/weapons.md, The vehicles' weapons):

- 0x4eb0d0 (zClass): the node itself, then each child's tree first to last,
  the name matched whole;
- 0x4abb60: the same walk, the name a prefix (strncmp to its length).

    cmake --build build && .venv/bin/python -P tests/differential/test_node_find.py

Each world's gamez.zbd is read whole by both sides (test_destroy.py's
load). Then made-up trees of run-time object3d nodes go under it: under the
world, the file's nodes and each other. Their names are chosen so that a
prefix, a name in two places and a longer name all occur.

Then random finds from random nodes, and from the null node, by these
names:
- the start's own;
- names in its tree;
- names elsewhere;
- prefixes of names;
- names longer than any;
- junk.

Compared: the node found (by pool index), and after every null-node call
the whole pool dump, whose reports the null node's "Null node pointer."
(0x4eb0d0) adds to.

NODE_FIND_WORLDS=c1 runs one world (the break tests).
"""
import ctypes
import os
import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_destroy as D  # noqa: E402
import test_gamez as G  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

probe = G.probe
probe.probe_node_name.argtypes = [ctypes.c_int, ctypes.c_char_p]
probe.probe_node_find.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_int]

NAMES = [b'firepoint', b'firepoint01', b'firepoint02', b'fire', b'gun', b'gun_l', b'gun_r', b'turret',
         b'turret_base', b'fp', b'healthy', b'destroyed', b'apc', b'bulldog']


class Emulated(D.Emulated):
    def __init__(self):
        super().__init__()
        self.cpu.stub_import(0x5bb654, self.strncmp)  # 0x4abb60's prefix compare

    def strncmp(self, c):
        a, b, n = self.cstr(c.arg(0)), self.cstr(c.arg(1)), c.arg(2)
        a, b = a[:n], b[:n]
        return 0 if a == b else (-1 if a < b else 1) & 0xffffffff

    def set_name(self, index, name):
        self.cpu.write(self.node(index), name[:0x23].ljust(0x24, b'\0'))

    def name(self, index):
        return self.cpu.read(self.node(index), 0x24).split(b'\0', 1)[0]

    def find(self, start, name, prefix):
        """ECX the node, EDX the name, both by register."""
        p = self.calloc(len(name) + 1)
        self.cpu.write(p, name + b'\0')
        self.cpu.call(0x4abb60 if prefix else 0x4eb0d0, ecx=self.node(start) if start >= 0 else 0, edx=p)
        r = self.cpu.uc.reg_read(UC_X86_REG_EAX)
        return self.index(r) if r else -1

    def children(self, index):
        n = self.node(index)
        count, arr = self.i32(n + 0x5c), self.i32(n + 0x60) & 0xffffffff
        return [self.index(self.i32(arr + 4 * i) & 0xffffffff) for i in range(max(count, 0))] if arr else []

    def tree(self, index, depth=0):
        out = [index]
        if depth < 12:
            for c in self.children(index):
                if c >= 0:
                    out += self.tree(c, depth + 1)
        return out


def main():
    emu = Emulated()
    os.chdir(G.ZBD.parent)
    buf = ctypes.create_string_buffer(64 << 20)
    seed = int(sys.argv[1]) if len(sys.argv) > 1 else 1
    worlds = os.environ.get('NODE_FIND_WORLDS', ','.join(G.WORLDS)).split(',')
    totals = {'whole': 0, 'prefix': 0, 'found': 0, 'none': 0, 'null': 0, 'prefix_only': 0}
    for w in worlds:
        name = f'zbd/{w}/gamez.zbd'
        if emu.load(G.ZBD / w / 'gamez.zbd', name) != probe.probe_destroy_load(name.encode()):
            raise SystemExit(f'{w}: the read differs')
        rng = random.Random(seed * 100 + G.WORLDS.index(w))
        listed = emu.listed()
        world_nodes = [i for i in listed if emu.info(i)[0] == 2]
        objects = [i for i in listed if emu.info(i)[0] == 5]
        # Made-up trees: run-time object3d nodes, named from NAMES (a number
        # after some, so a prefix and a whole name both occur).
        made = []
        for k in range(150):
            parent = rng.choice(made) if made and rng.random() < 0.6 else rng.choice(world_nodes + objects[:200])
            op = (8, parent, -1, 0)
            want, got = emu.op(*op), probe.probe_destroy_op(*op)
            if want != got:
                raise SystemExit(f'{w} make {k}: original {want}, port {got}')
            n = rng.choice(NAMES) + (str(rng.randrange(4)).encode() if rng.random() < 0.3 else b'')
            emu.set_name(want, n)
            probe.probe_node_name(want, n)
            made.append(want)
        D.compare(f'{w} after the made-up trees', emu.dump(1), buf, probe.probe_destroy_dump(1, buf, len(buf)))
        starts = made + objects[:300] + world_nodes
        for q in range(600):
            start = -1 if q % 50 == 0 else rng.choice(starts)
            kind = rng.random()
            if start >= 0 and kind < 0.15:
                n = emu.name(start)
            elif start >= 0 and kind < 0.45:
                n = emu.name(rng.choice(emu.tree(start)))
            elif kind < 0.6:
                n = emu.name(rng.choice(starts))
            elif kind < 0.8:
                full = emu.name(rng.choice(starts)) or b'x'
                n = full[:rng.randrange(1, len(full) + 1)]
            elif kind < 0.9:
                n = rng.choice(NAMES) + b'_longer_than_any'
            else:
                n = bytes(rng.choice(b'abcdefgh_0123') for _ in range(rng.randrange(1, 12)))
            for prefix in (0, 1):
                want = emu.find(start, n, prefix)
                got = probe.probe_node_find(start, n, prefix)
                if want != got:
                    raise SystemExit(f'{w} find {q} {"prefix" if prefix else "whole"} {n!r} from {start}: '
                                     f'original {want}, port {got}')
                totals['prefix' if prefix else 'whole'] += 1
                totals['found' if want >= 0 else 'none'] += 1
            if emu.find(start, n, 1) >= 0 and emu.find(start, n, 0) < 0:
                totals['prefix_only'] += 1
            if start < 0:
                totals['null'] += 1
                D.compare(f'{w} find {q} from the null node', emu.dump(0), buf,
                          probe.probe_destroy_dump(0, buf, len(buf)))
        print(f'{w}: 150 made-up nodes, 1200 finds identical')
    print(f"node finds: {totals['whole']} whole and {totals['prefix']} prefix finds identical "
          f"({totals['found']} found, {totals['none']} none, {totals['prefix_only']} names only the prefix find "
          f"matched, {totals['null']} from the null node with its report)")


if __name__ == '__main__':
    main()
