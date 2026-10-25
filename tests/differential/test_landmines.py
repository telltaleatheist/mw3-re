"""The mission's landmines against the original x86 (docs/landmines.md): the
load 0x4135c0 with zReader's find 0x5569e0 and the list's insert 0x4137c0
run as they are, and the check 0x413560 with its distance 0x51dfa0 and the
detonation 0x413470. The x87 at 24 bits, as in play.

    cmake --build build && .venv/bin/python tests/differential/test_landmines.py [cases]

Load cases: no file, or a tree with "weapon" right or wrong (missing, not a
one-string list, unknown to zWeapon), "radius" right or wrong (missing, an
int, two values), the mission's block missing or holding up to 40 items of
which some are not three floats (an int, two or four values, not a list),
other missions' blocks beside it, and up to 5 mines already listed. The
reader's load 0x556740, free 0x556820 and zWeapon's find 0x581ed0 are stubs
both sides log. Compared: the answer, the weapon, the radius and its square,
the list.

Check cases: up to 12 mines (armed or not, near, at the edge, far, NaN), a
radius, the place (NaN sometimes), IMPACT_PROXIMITY 0 or not; the ground
query 0x4edab0 (found or not, its hit), the impact 0x5826b0, the splash
query 0x582920 and splash 0x5829b0 are stubs both sides log with what they
are given. Compared: the answer, the list after, the stand-in shot.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import Emu, Trace, compare, load_probe  # noqa: E402

U32, I32 = ctypes.c_uint32, ctypes.c_int32
LIST, WEAPON, RADIUS, RADIUS_SQ, SHOT = 0x604e48, 0x604e5c, 0x5e1f58, 0x604e40, 0x604e60
LOAD, CHECK = 0x4135c0, 0x413560
COUNT = {'loads': 0, 'no file': 0, 'loads failed': 0, 'mines read': 0, 'items skipped': 0, 'default radius': 0,
         'checks': 0, 'mines set off': 0, 'no ground': 0, 'splashes': 0, 'NaN places': 0}


def die():
    sys.stdout.flush()
    os._exit(1)


def f2u(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


def u2f(u):
    return struct.unpack('<f', struct.pack('<I', u))[0]


# A tree: ('i', v) | ('f', v) | ('s', text) | ('l', [items]).
def flat(node, out, strs):
    kind, v = node
    if kind == 'i':
        out += [1, v]
    elif kind == 'f':
        out += [2, f2u(v)]
    elif kind == 's':
        out += [3, len(strs)]
        strs += v.encode() + b'\0'
    else:
        out += [4, len(v)]
        for c in v:
            flat(c, out, strs)


def place(e, node, at):
    kind, v = node
    if kind == 'i':
        e.cpu.write(at, struct.pack('<Ii', 1, v))
    elif kind == 'f':
        e.cpu.write(at, struct.pack('<If', 2, v))
    elif kind == 's':
        e.cpu.write(at, struct.pack('<II', 3, e.world_string(v)))
    else:
        lst = e.world_alloc(8 * (len(v) + 2))
        e.cpu.write(lst, struct.pack('<Ii', 1, len(v) + 1))
        for i, c in enumerate(v):
            place(e, c, lst + 8 * (i + 1))
        e.cpu.write(at, struct.pack('<II', 4, lst))


def tree_case(r, block, skipped):
    items = []
    w = r.random()
    if w < 0.85:
        items += [('s', 'weapon'), ('l', [('s', r.choice(['LANDMINE', 'MINE_X']))])]
    elif w < 0.92:
        items += [('s', 'weapon'), ('l', [('i', 3)])]
    elif w < 0.96:
        items += [('s', 'weapon'), ('l', [('s', 'LANDMINE'), ('s', 'B')])]
    k = r.random()
    if k < 0.7:
        items += [('s', 'radius'), ('l', [('f', r.choice([20.0, 15.5, 0.0, r.uniform(1, 60), -5.0]))])]
    elif k < 0.8:
        items += [('s', 'radius'), ('l', [('i', 20)])]
    elif k < 0.9:
        items += [('s', 'radius'), ('l', [('f', 10.0), ('f', 2.0)])]
    for name in ('m1', 'm2', 'm3', 'm4'):
        if name != block and r.random() < 0.5:
            items += [('s', name), ('l', [('l', [('f', 1.0), ('f', 2.0), ('f', 3.0)])])]
    if r.random() < 0.9:
        mines = []
        for _ in range(r.randrange(0, 41)):
            x = r.random()
            if x < 0.8:
                mines.append(('l', [('f', r.uniform(-2000, 2000)), ('f', r.uniform(0, 60)), ('f', r.uniform(-2000, 2000))]))
            elif x < 0.85:
                mines.append(('l', [('f', 1.0), ('i', 2), ('f', 3.0)]))
            elif x < 0.9:
                mines.append(('l', [('f', 1.0), ('f', 2.0)]))
            elif x < 0.95:
                mines.append(('l', [('f', 1.0), ('f', 2.0), ('f', 3.0), ('f', 4.0)]))
            else:
                mines.append(('f', 5.0))
            skipped[0] += x >= 0.8
        items += [('s', block), ('l', mines)]
    r.shuffle(pairs := [items[i:i + 2] for i in range(0, len(items), 2)])
    return ('l', [('l', [x for p in pairs for x in p])])


def old_mines(r, n):
    out = []
    for _ in range(n):
        out += [r.choice([0, 1]), f2u(r.uniform(-50, 50)), f2u(r.uniform(0, 9)), f2u(r.uniform(-50, 50))]
    return out


def set_list(e, words):
    n = len(words) // 4
    a = e.world_alloc(16 * max(n, 1)) if n else 0
    for i in range(n):
        e.cpu.write(a + 16 * i, struct.pack('<B3xIII', words[4 * i], *words[4 * i + 1:4 * i + 4]))
    e.w32(LIST + 4, a)
    e.w32(LIST + 8, a + 16 * n if n else 0)
    e.w32(LIST + 12, a + 16 * n if n else 0)


def get_list(e):
    a, b = e.r32(LIST + 4), e.r32(LIST + 8)
    out = []
    for p in range(a, b, 16):
        out += [e.r8(p), e.r32(p + 4), e.r32(p + 8), e.r32(p + 12)]
    return out


def load_case(probe, seed):
    r = random.Random(seed)
    block = r.choice(['m1', 'm2', 'm3', 'm4'])
    skipped = [0]
    tree = tree_case(r, block, skipped) if r.random() < 0.93 else None
    defs = [0, 0x7a0000 + 0x200 * r.randrange(1, 8)]
    t = Trace({'0x00581ed0': r.choice(defs) if r.random() < 0.15 else defs[1]})
    e = Emu(t)
    e.cpu.fpu_control(0x007f)
    e.heap_stubs()
    root = 0
    if tree:
        root = e.world_alloc(16)
        place(e, tree, root)

    def load(emu, key, a, pop):
        emu.t.handle(key, a)
        emu._ret(root, pop)
    e.stub(0x556740, '0x00556740', 'zc', None, load)
    e.stub(0x556820, '0x00556820', '')
    e.stub(0x581ed0, '0x00581ed0', 'zc')
    old = old_mines(r, r.randrange(0, 6))
    set_list(e, old)
    g = [r.choice([0, defs[1]]), f2u(r.uniform(1, 40)), f2u(r.uniform(1, 400))]
    e.w32(WEAPON, g[0])
    e.w32(RADIUS, g[1])
    e.w32(RADIUS_SQ, g[2])
    t.reset()
    ret_a = e.call(LOAD, ecx=LIST, stack=(e.world_string('landmines.zrd'), e.world_string(block))) & 0xff
    got_a = get_list(e)
    glob_a = [e.r32(WEAPON), e.r32(RADIUS), e.r32(RADIUS_SQ)]
    log_a = list(t.log)
    tb = Trace({'0x00581ed0': t.script['0x00581ed0']})
    probe.probe_app_set_callback(tb.callback())
    words, strs = [], bytearray()
    if tree:
        flat(tree, words, strs)
    gl = (U32 * 3)(*g)
    cap = 400
    out = (U32 * (4 * cap))()
    res = probe.probe_mines_load((I32 * max(len(words), 1))(*words), len(words), bytes(strs) + b'\0', b'landmines.zrd',
                                 block.encode(), (U32 * max(len(old), 1))(*old), len(old) // 4, gl, out, cap)
    ret_b, n_b = res & 1, res >> 1
    got_b = list(out)[:4 * n_b]
    what = f'load seed {seed} (block {block}, file {"yes" if tree else "no"})'
    if not compare(what, log_a, tb.log):
        die()
    if ret_a != ret_b or got_a != got_b or glob_a != list(gl):
        print(f'{what}: answer {ret_a}/{ret_b}, globals {[hex(x) for x in glob_a]} / {[hex(x) for x in gl]}, '
              f'list {len(got_a) // 4}/{len(got_b) // 4} mines')
        die()
    COUNT['loads'] += 1
    COUNT['no file'] += tree is None
    COUNT['loads failed'] += ret_a == 0
    COUNT['mines read'] += (len(got_a) - len(old)) // 4
    COUNT['items skipped'] += skipped[0] if ret_a == 1 else 0
    COUNT['default radius'] += ret_a == 1 and tree is not None and glob_a[1] == f2u(20.0)


def check_case(probe, seed):
    r = random.Random(seed)
    t = Trace({'0x00582920': lambda n, a: r.choice([0, 1]), '0x005826b0': 0, '0x005829b0': 0})
    e = Emu(t)
    e.cpu.fpu_control(0x007f)
    radius = r.choice([20.0, r.uniform(1, 50), 0.0])
    rsq = f2u(radius * radius)
    at = [r.uniform(-100, 100), r.uniform(0, 30), r.uniform(-100, 100)]
    if r.random() < 0.05:
        at[r.randrange(3)] = float('nan')
        COUNT['NaN places'] += 1
    mines = []
    for _ in range(r.randrange(0, 13)):
        k = r.random()
        if k < 0.4:
            d = [r.uniform(-1, 1) * radius for _ in range(3)]
            p = [at[i] + d[i] for i in range(3)]
        elif k < 0.5:
            p = [at[0] + radius, at[1], at[2]]
        elif k < 0.95:
            p = [r.uniform(-200, 200), r.uniform(0, 30), r.uniform(-200, 200)]
        else:
            p = [float('nan'), 1.0, 2.0]
        mines += [1 if r.random() < 0.85 else 0] + [f2u(v) for v in p]
    n = len(mines) // 4
    def_addr = e.world_alloc(0x1f4)
    prox = r.choice([0.0, 40.0, r.uniform(1, 50)])
    e.w32(def_addr + 0x3c, f2u(prox))
    ground = []
    for _ in range(n):
        ground += [r.choice([0, 0, 0, 1]), f2u(r.uniform(-1, 1)), f2u(r.uniform(0, 40)), r.getrandbits(32),
                   0x10000 * r.randrange(1, 100)]
    gi = [0]

    def ground_fn(emu, key, a, pop):
        emu.t.handle(key, a)
        w = ground[gi[0]:gi[0] + 5]
        gi[0] += 5
        hit = emu.uc.reg_read(__import__('unicorn').x86_const.UC_X86_REG_EDX) + 4
        emu.cpu.write(hit, struct.pack('<9I', w[1], w[1] ^ 0x80000000, (w[1] + 1) & 0xffffffff, 0x1234, w[2], 0x5678,
                                        w[3], (w[3] * 3) & 0xffffffff, w[4]))
        emu.w32(hit + 0x24, w[4] + 0x40)
        emu._ret(w[0], pop)
    world_obj = e.world_alloc(16)
    e.w32(0x608c84, world_obj)
    e.w32(world_obj + 4, 0x55aa0000)
    e.stub(0x4edab0, '0x004edab0', 's0,s1,s2', None, ground_fn)

    def impact_fn(emu, key, a, pop):
        hit, p = a[1], a[2]
        h = [emu.r32(hit + 4 * i) for i in range(10)]
        args = [a[0]] + h[0:3] + h[3:6] + h[6:8] + h[8:10] + [emu.r32(p + 0x24 + 4 * i) for i in range(3)] + \
               [emu.r32(p + 0x48 + 4 * i) for i in range(3)] + [emu.r32(p + 0x5bc), a[3]]
        emu._ret(emu.t.handle(key, args), pop)
    e.stub(0x5826b0, '0x005826b0', 'c,d,s0,s1', None, impact_fn)

    def query_fn(emu, key, a, pop):
        p = a[1]
        args = [a[0]] + [emu.r32(p + 0x48 + 4 * i) for i in range(3)] + [emu.r32(p + 0x5bc), a[2]]
        emu._ret(emu.t.handle(key, args), pop)
    e.stub(0x582920, '0x00582920', 'c,d,s0', None, query_fn)

    def splash_fn(emu, key, a, pop):
        emu._ret(emu.t.handle(key, [a[0], a[1]]), pop)
    e.stub(0x5829b0, '0x005829b0', 'c,s1', None, splash_fn)
    set_list(e, mines)
    e.w32(WEAPON, def_addr)
    e.w32(RADIUS_SQ, rsq)
    at_addr = e.world_alloc(12)
    e.cpu.write(at_addr, struct.pack('<3f', *at))
    t.reset()
    ret_a = e.call(CHECK, ecx=LIST, stack=(at_addr,)) & 0xff
    got_a = get_list(e)
    shot_a = [e.r32(SHOT + 0x24 + 4 * i) for i in range(3)] + [e.r32(SHOT + 0x48 + 4 * i) for i in range(3)] + \
             [e.r32(SHOT + 0x5bc)]
    log_a = list(t.log)
    answers = [a for k, a in t.answers.items() if k[0] == '0x00582920']
    tb = Trace({'0x00582920': answers or [0], '0x005826b0': 0, '0x005829b0': 0})
    probe.probe_app_set_callback(tb.callback())
    out = (U32 * max(len(mines), 1))()
    shot = (U32 * 7)()
    ret_b = probe.probe_mines_check((U32 * max(len(mines), 1))(*mines), n, rsq, def_addr, f2u(prox),
                                    (U32 * max(len(ground), 1))(*ground), n, (U32 * 3)(*[f2u(v) for v in at]), out, shot)
    got_b = list(out)[:len(mines)]
    what = f'check seed {seed} ({n} mines, radius {radius:.2f}, proximity {prox:.1f})'
    if not compare(what, log_a, tb.log):
        die()
    if ret_a != ret_b or got_a != got_b or shot_a != list(shot):
        print(f'{what}: answer {ret_a}/{ret_b}, list {got_a} / {got_b}, shot {shot_a} / {list(shot)}')
        die()
    COUNT['checks'] += 1
    fired = sum(1 for i in range(n) if mines[4 * i] and not got_a[4 * i])
    COUNT['mines set off'] += fired
    COUNT['no ground'] += sum(1 for k, a in log_a if k == '0x004edab0') - sum(1 for k, a in log_a if k == '0x005826b0')
    COUNT['splashes'] += sum(1 for k, a in log_a if k == '0x005829b0')


def main():
    probe = load_probe()
    probe.probe_mines_load.argtypes = [ctypes.POINTER(I32), I32, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p,
                                       ctypes.POINTER(U32), I32, ctypes.POINTER(U32), ctypes.POINTER(U32), I32]
    probe.probe_mines_check.argtypes = [ctypes.POINTER(U32), I32, U32, U32, U32, ctypes.POINTER(U32), I32,
                                        ctypes.POINTER(U32), ctypes.POINTER(U32), ctypes.POINTER(U32)]
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 2000
    for seed in range(count):
        load_case(probe, seed)
        check_case(probe, seed)
    print(f'ok: {count} loads and checks ({", ".join(f"{v} {k}" for k, v in COUNT.items())}), as the original')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()
