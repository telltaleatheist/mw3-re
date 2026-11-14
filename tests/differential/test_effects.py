"""zEffect's effect table (src/gamez/zeffect/zeff_init, docs/weapon-effects.md) against the original: effects.zrd's
entries read (0x50d2c0 with 0x50d610), spawns (0x50d650 with 0x50d820, 0x50d880, 0x50d6a0, 0x50d7a0), the copies'
ticks (0x50d910) and the free (0x50d570). No shipped file has an entry; the blocks here are made up: entries with
and without a node, a gfx found on the node or down its children or nowhere, MAPS, SPEED and LOOPING present or not.
The spawns land near and far from the camera (under 56.25 squared, under 1600, beyond); the ticks run the copies
through their growth, shrink and return to the free list, and later spawns take them back. Every call out of zEffect
(the nodes, the model's cycle, the textures, the reports) goes to one trace, answered by one script on both sides;
the exe's reads of a node's fields are the port's silent '@' calls, answered from the same fake nodes.

    cmake --build build --target mw3probe && .venv/bin/python tests/differential/test_effects.py [cases]
"""
import ctypes
import math
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import Emu, Trace, load_probe  # noqa: E402
from ai_world import ser  # noqa: E402

U32 = ctypes.c_uint32
DT = 0x72c52c
TABLE_GLOBALS = (0x73f3c0, 0x73f3c8, 0x73f3cc, 0x73f3d0, 0x73f3d4, 0x73f3d8, 0x73f3dc, 0x73f3e0, 0x73f3e4, 0x73f3e8)


def fb(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


def put_node(e, a, v):
    if isinstance(v, list):
        items = e.world_alloc(8 * (len(v) + 1))
        e.w32(items, 1)
        e.w32(items + 4, len(v) + 1)
        for i, x in enumerate(v):
            put_node(e, items + 8 * (i + 1), x)
        e.w32(a, 4)
        e.w32(a + 4, items)
    elif isinstance(v, str):
        e.w32(a, 3)
        e.w32(a + 4, e.world_string(v))
    elif isinstance(v, float):
        e.w32(a, 2)
        e.w32(a + 4, fb(v))
    else:
        e.w32(a, 1)
        e.w32(a + 4, v & 0xffffffff)


class World:
    """The fake nodes: each a block in the emulator's world memory (its +0x24 flags, +0x40 the copy, +0x54 a
    parent, +0x5c / +0x60 its children) and the script's answers about it."""

    def __init__(self, e, r):
        self.e, self.r = e, r
        self.models = {}   # node -> its model (0 none)
        self.children = {}

    def node(self, model=0, children=()):
        a = self.e.world_alloc(0x80)
        self.e.w32(a + 0x24, 4)
        kids = self.e.world_alloc(4 * max(1, len(children)))
        for i, c in enumerate(children):
            self.e.w32(kids + 4 * i, c)
        self.e.w32(a + 0x5c, len(children))
        self.e.w32(a + 0x60, kids)
        self.models[a] = model
        self.children[a] = list(children)
        return a


def make_case(r, e):
    w = World(e, r)
    camera = (r.uniform(-50, 50), r.uniform(0, 30), r.uniform(-50, 50))
    names, nodes, copies = {}, [], []
    entries = []
    for k in range(r.randint(1, 5)):
        node_name = f'fx{k}'
        kind = r.choice(['none', 'own', 'child', 'deep', 'nowhere'])
        if kind == 'none':
            nodes.append(0)
        else:
            gfx = 0x7a000 + 0x10 * k
            if kind == 'own':
                n = w.node(gfx)
            elif kind == 'child':
                n = w.node(0, [w.node(0), w.node(gfx)])
            elif kind == 'deep':
                n = w.node(0, [w.node(0, [w.node(gfx)])])
            else:
                n = w.node(0, [w.node(0)])
            nodes.append(n)
        names[node_name] = nodes[-1]
        item = [node_name]
        if r.random() < 0.9:
            item += ['NAME', [f'effect_{k}']]
        item += ['MAPS', [f'tex{k}_{j}' for j in range(r.randint(0, 4))]]  # always: the exe reads a missing one through 0
        if r.random() < 0.8:
            item += ['SPEED', [r.choice([0.5, 1.0, 12.0, r.uniform(0, 30)])]]
        if r.random() < 0.7:
            item += ['LOOPING', [r.choice(['ON', 'OFF', 'on'])]]
        entries.append(item)
    tree = ['EFFECTS', entries]

    # The copies node_copy hands out, each a node with its own gfx (or none, now and then).
    def copy_answer(n, args):
        src = args[0]
        if src == 0 or r.random() < 0.05:
            return 0
        gfx = w.models.get(src) or 0x7b000 + 0x10 * len(copies)
        c = w.node(gfx if r.random() < 0.95 else 0, [])
        copies.append(c)
        return c
    return w, tree, camera, names, copies, copy_answer


STUBS = {
    0x4f7fe0: 'c,zd', 0x4f9af0: 'c,d,s0', 0x4fbc10: 'c,s0,s1,s2', 0x4fc0b0: 'c,s0,s1,s2', 0x53bc10: 'c',
    0x4eb9b0: 'c,d', 0x4eb170: 'c,d', 0x4eb620: 'c,d', 0x4eb6d0: 'c,d', 0x4eb450: 'c,d', 0x4ebb40: 'c,d',
    0x4f9050: 'c', 0x539cc0: 'c,d', 0x53bc30: 'c,d', 0x53bcf0: 'c,s0', 0x53bcd0: 'c,d', 0x518c60: 'zc',
    0x53bd10: 'c,d', 0x5190d0: '', 0x49bfd0: 's0,s2,zs4',
}


class Exe:
    def __init__(self, t):
        self.t = t
        e = self.e = Emu(t)
        e.heap_stubs()
        for iat, name in ((0x5bb664, 'malloc'), (0x5bb60c, 'calloc'), (0x5bb65c, 'free')):
            e.import_stub(iat, name, 's0,s1', 0, self._alloc(name))
        e.import_stub(0x5bb62c, 'fprintf', 's0,zs1,s2,s3,zs4,zs5', 0, self._fprintf)
        self.t.silent |= {'malloc', 'calloc', 'free'}
        for addr, spec in STUBS.items():
            e.stub(addr, f'{addr:#010x}', spec)
        e.stub(0x4eb420, '0x004eb420', 'c,d', None, self._node_model)
        e.stub(0x4ed530, '0x004ed530', 'c,d', None, self._camera)
        e.stub(0x556740, 'load', '', None, lambda em, k, a, p: em._ret(self.tree, p))
        e.stub(0x556820, 'free_node', 'c', None, lambda em, k, a, p: em._ret(0, p))
        e.stub(0x508fc0, 'anim_init', '', None, lambda em, k, a, p: em._ret(0, p))  # 0x50d270's anim start
        self.t.silent |= {'load', 'free_node', 'anim_init'}
        for a in (0x72be80, 0x72be84, 0x72be88, 0x72be8c):  # the list nodes' pool (0x555cd0, as test_sound makes it)
            e.w32(a, 0)
        e.call(0x555cd0, ecx=0x200)

    def _alloc(self, name):
        def fn(em, key, a, pop):
            if name == 'free':
                em._ret(0, 0)
            elif name == 'calloc':
                em._ret(em.alloc(max(a[0] * a[1], 4)), 0)
            else:
                em._ret(em.alloc(max(a[0], 4)), 0)
        return fn

    def _fprintf(self, em, key, a, pop):
        self.t.handle('fprintf', [a[1], a[4], a[5]])
        em._ret(0, 0)

    def _node_model(self, em, key, a, pop):
        v = self.t.handle('0x004eb420', [a[0]])
        em.w32(a[1], v)
        em._ret(v, pop)

    def _camera(self, em, key, a, pop):
        self.t.handle('0x004ed530', [a[0]])
        for i in range(3):
            em.w32(a[1] + 4 * i, fb(self.camera[i]))
        em._ret(0, pop)

    def run(self, tree, world, camera_node, camera, dts, plan):
        e = self.e
        for g in TABLE_GLOBALS:
            e.w32(g, 0)
        self.camera = camera
        self.tree = e.world_alloc(8)
        put_node(e, self.tree, tree)
        e.call(0x50d2c0, ecx=world, edx=camera_node, stack=(0,))
        kept = []
        for op in plan():
            if op[0] == 1:
                idx, at = op[1], op[2]
                p = e.world_alloc(12)
                for i in range(3):
                    e.w32(p + 4 * i, fb(at[i]))
                self.t.handle('@op', [1, idx])
                e.call(0x50d650, ecx=idx & 0xffffffff, edx=p)
                kept.append((1, idx & 0xffffffff, fb(at[0]), fb(at[1]), fb(at[2])))
            elif op[0] == 2:
                node = op[1]
                if not e.r32(node + 0x40):
                    continue  # not running: the exe reads the copy through 0
                e.w32(DT, fb(dts[len(kept) % len(dts)]))
                self.t.handle('@op', [2, node])
                e.call(0x50d910, ecx=node)
                kept.append((2, node, 0, 0, 0))
        self.reused = e.r32(0x73f3e8)  # copies taken back from the free list
        self.t.handle('@op', [3, 0])
        e.call(0x50d570)
        return kept


def main():
    cases = int(sys.argv[1]) if len(sys.argv) > 1 else 300
    probe = load_probe()
    probe.probe_effects_run.argtypes = [ctypes.c_char_p, ctypes.c_int32, U32, U32, ctypes.POINTER(U32), ctypes.c_int32]
    r = random.Random(0x50d2c0)
    bad = 0
    counts = {'calls': 0, 'scales': 0, 'reused': 0, 'freed': 0}
    for c in range(cases):
        t = Trace()
        exe = Exe(t)
        w, tree, camera, names, copies, copy_answer = make_case(r, exe.e)
        world, camera_node = 0x7c000, 0x7d000
        dts = [r.choice([1 / 30, 0.05, 0.1, 0.25])]
        script = {
            '0x004f7fe0': lambda n, a: names.get(a[1], 0),
            '0x004eb420': lambda n, a: w.models.get(a[0], 0),
            '0x004f9af0': copy_answer,
            '0x00518c60': lambda n, a: 0x7e000 + n,
        }
        t.script = script
        nspawn = len(names)

        def plan():
            for _ in range(r.randint(1, 30)):
                if r.random() < 0.4 or not copies:
                    d = r.choice([3.0, 20.0, 60.0, 0.0])
                    ang = r.uniform(0, 6.28)
                    yield (1, r.choice(list(range(nspawn)) + [-1]), (camera[0] + d * math.cos(ang), camera[1],
                                                                     camera[2] + d * math.sin(ang)))
                else:
                    for _ in range(r.randint(1, 12)):
                        yield (2, r.choice(copies))
        kept = exe.run(tree, world, camera_node, camera, dts, plan)
        want = list(t.log)
        # The port, the same answers: node_copy's handles replayed in order, the fake nodes read for the '@' keys.
        copied = [a for (k, n), a in t.answers.items() if k == '0x004f9af0']
        t2 = Trace()
        e = exe.e
        dt_now = [0.0]
        script2 = dict(script)
        script2['0x004f9af0'] = lambda n, a: copied[n] if n < len(copied) else 0
        script2['@node+0x5c'] = lambda n, a: e.r32(a[0] + 0x5c)
        script2['@node+0x60'] = lambda n, a: e.r32(e.r32(a[0] + 0x60) + 4 * a[1])
        script2['@node+0x24'] = lambda n, a: e.r32(a[0] + 0x24)
        script2['@node+0x54'] = lambda n, a: e.r32(a[0] + 0x54)
        script2['@dt'] = lambda n, a: fb(dts[0])
        script2['@camera'] = lambda n, a: fb(camera[a[0]])
        t2.script = script2

        def note(n, a):
            if a[0] == 2:
                dt_now[0] = dts[0]
            return 0
        script2['@op'] = note
        probe.probe_app_set_callback(t2.callback())
        flat = [v for o in kept for v in o]
        data = ser(tree)
        probe.probe_effects_run(data, len(data), world, camera_node, (U32 * max(1, len(flat)))(*flat), len(kept))
        got = [x for x in t2.log if not x[0].startswith('@')]
        want = [x for x in want if not x[0].startswith('@')]
        counts['calls'] += len(want)
        counts['reused'] += exe.reused
        counts['scales'] += sum(1 for k, _ in want if k == '0x004fbc10')
        counts['freed'] += sum(1 for k, a in want if k == '0x004eb450' and a[1] == 0)
        if got != want:
            bad += 1
            if bad <= 3:
                k = next(i for i in range(min(len(got), len(want)) + 1) if i >= min(len(got), len(want)) or got[i] != want[i])
                print(f'case {c}: differ at call {k} of {len(want)} / {len(got)}')
                print('  exe ', want[max(0, k - 2):k + 3])
                print('  port', got[max(0, k - 2):k + 3])
    print(f'{cases} cases, {bad} differ; {counts}')
    sys.stdout.flush()
    short = cases >= 300 and (counts['reused'] < cases // 10 or counts['freed'] < cases or counts['scales'] < cases * 5)
    if short:
        print('too little reached')
    os._exit(1 if bad or short else 0)


if __name__ == '__main__':
    main()
