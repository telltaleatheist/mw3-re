"""A field base lost against the original x86 (docs/vehicles.md, A mobile
field base lost; mech3/mech_fieldbase_cut): the store cut of 0x4590b0 on a
campaign game, the bases one fewer, the mechs kept at random (0x471c10 into
+0x78, 0x472a30 out), the rest by weight (0x45b900, the items' weights, the
copies vt+0 and removes vt+0x14, the lab's rounds 0x4b4900 for ammo) and
the messages 0xf05..0xf07 (0x40fe70).

    cmake --build build && .venv/bin/python tests/differential/test_field_cut.py [runs]

The game's stores are the original's own (0x470ce0 and its adds), the
weapons' lab data from weapons.zrd (test_mechsave.py) on both sides; the
draws (rand), the mechs' names, 0x4b4900 and the messages are stubbed, the
basic vehicle's death 0x4ad2d0 silent. Compared: the traces, the base count
and both stores' items.
"""
import ctypes
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import CALLBACK, Emu, Replay, Trace, compare, load_probe, rnd  # noqa: E402
from test_mechsave import WEAPON_NAMES, WEAPONS, weapon_labs  # noqa: E402
from test_zreader import READER  # noqa: E402

U, I = ctypes.c_uint32, ctypes.c_int32
COVER = {}
FLOOR = bytes.fromhex('83ec04 d93c24 668b0424 6625fff3 660d0004 6689442402 d96c2402 dd442408 d9fc d92c24 83c404 c3')
CEIL = bytes.fromhex('83ec04 d93c24 668b0424 6625fff3 660d0008 6689442402 d96c2402 dd442408 d9fc d92c24 83c404 c3')
ADD = {0: 0x471c10, 1: 0x471840, 2: 0x471940, 3: 0x471a20, 4: 0x471b20}
KIND = {0x5bc160: 0, 0x5c0fc0: 1, 0x5bd150: 2, 0x5bd180: 3, 0x5c3700: 4}
ROUNDS = [1, 10, 20, 50, 100, 7]


def cover(label, cond=True):
    COVER.setdefault(label, 0)
    if cond:
        COVER[label] += 1


def lab_rounds(n, args):
    return min(args[1], ROUNDS[args[0] % len(ROUNDS)])


def make_case(seed, weapons):
    r = rnd(seed, 'cut')
    c = {'bases': r.choice([1, 2, 2, 3, 3, 3, 0])}
    items = []
    many = r.random() < 0.08
    for _ in range(r.randrange(0, 40 if many else 10)):
        k = r.choice([0, 0, 1, 2, 2, 3, 3, 4])
        if many:
            k = 0
        b10 = int(r.random() < 0.08)
        if k == 0:
            items.append((0, r.randrange(0, 19), -1, 1 if many else r.choice([1, 1, 2, 3]), b10))
        elif k == 1:
            items.append((1, r.choice(weapons), r.choice([1, 2]), r.choice([1, 1, 2, 4]), b10))
        elif k == 2:
            items.append((2, r.choice(weapons), -1, r.choice([1, 15, 120, 300, 2000]), b10))
        elif k == 3:
            items.append((3, r.randrange(0, 15), r.choice([1, 2]), r.choice([1, 2, 6, 20]), b10))
        else:
            items.append((4, r.randrange(0, 80), r.choice([0, 1]), r.choice([1, 2]), b10))
    if r.random() < 0.2:  # over 300 t a base: the cap is the limit
        items.append((3, 3, 2, r.choice([700, 1200, 2500]), 0))
    if many:  # 64 exactly from the first sixteen, the rest past the scan
        items = [(0, i, -1, 4, 0) for i in range(16)] + items[:6]
    c['items'] = items
    c['seed'] = seed
    return c


class World:
    def __init__(self, labs):
        self.labs = labs
        t = self.t = Trace()
        e = self.e = Emu(t)
        e.heap_stubs()
        e.cpu.fpu_control(0x007f)  # play's 24-bit precision
        e.cpu.import_code(0x5bb66c, FLOOR)
        e.cpu.import_code(0x5bb61c, CEIL)
        e.import_stub(0x5bb668, 'rand')

        def cs(s):
            a = e.world_alloc(len(s) + 16)
            e.w32(a + 4, len(s))
            e.cpu.write(a + 12, s.encode('latin1') + b'\0')
            return a + 12
        e.stub(0x5abe04, '@cs', 'c,s0', 4, lambda em, k, a, p: (em.w32(a[0], cs(em.cstr(a[1]))), em._ret(a[0], 4)))
        e.stub(0x5abe5e, '@cs+=', 'c,s0', 4, lambda em, k, a, p: (
            em.w32(a[0], cs(em.cstr(em.r32(a[0])) + em.cstr(a[1]))), em._ret(a[0], 4)))
        e.stub(0x5abda4, '@cs~', '', 0, lambda em, k, a, p: em._ret(0, 0))

        def sprintf(em, key, a, pop):
            buf, fmt, lo, hi = em.args('s0,s1,s2,s3')
            x = struct.unpack('<d', struct.pack('<II', lo, hi))[0]
            assert em.cstr(fmt) == '%.2f'
            s = ('%.2f' % x).encode() + b'\0'
            em.cpu.write(buf, s)
            em._ret(len(s) - 1, 0)
        e.import_stub(0x5bb658, '@sprintf', '', 0, sprintf)

        def message(em, key, a, pop):
            secs, ident, p = em.args('s0,s1,s2')
            text = em.cstr(p) if ident in (0xf05, 0xf06) else 0
            em._ret(em.t.handle(key, (secs, ident, text)), 0)
        e.stub(0x40fe70, pop=0, fn=message)

        def name(em, key, a, pop):
            em.t.handle(key, a)
            em._ret(em.world_string(f'mech{a[0]}'), 0)
        e.stub(0x484650, spec='c', pop=0, fn=name)
        e.stub(0x4b4900, spec='c,d', pop=0)
        t.script['0x004b4900'] = lab_rounds
        e.stub(0x4ad2d0, key='@death', spec='c')

        # weapons.zrd's lab data where the weights look (0x581ed0 then +0x1f0).
        self.defs = {}
        for wname, lab in zip(WEAPON_NAMES, labs):
            if lab is None:
                continue
            obj, blk = e.alloc(0x200), e.alloc(0x40)
            e.cpu.write(blk + 0x1c, struct.pack('<fi', lab[0], lab[2]))
            e.cpu.write(blk + 0x28, struct.pack('<fi', lab[1], lab[3]))
            e.cpu.write(blk + 0x34, struct.pack('<f', lab[4]))
            e.w32(obj + 0x1f0, blk)
            self.defs[wname.decode() if isinstance(wname, bytes) else wname] = obj
        e.stub(0x581ed0, key='@def', spec='zc', fn=lambda em, k, a, p: em._ret(self.defs.get(a[0], 0), p))

    def run(self, c):
        e, t = self.e, self.t
        r = rnd(c['seed'], 'rand')
        n_mechs = sum(it[3] for it in c['items'] if it[0] == 0)
        top = 32766 if n_mechs >= 64 else 32767  # a draw of 64 reads past the exe's array

        def draw(n, args):
            return r.choice([0, top, r.randrange(0, top + 1), r.randrange(0, top + 1)])
        t.script['rand'] = draw
        game = e.alloc(0xd0)
        e.call(0x470ce0, ecx=game + 8, stack=(0,))
        e.call(0x470ce0, ecx=game + 0x78, stack=(0,))
        e.w32(game + 0xa8, c['bases'])
        for k, ident, tech, count, b10 in c['items']:
            args = {0: (ident,), 1: (ident, tech), 2: (ident, 1), 3: (ident, tech, 1), 4: (ident, tech)}[k]
            it = e.call(ADD[k], ecx=game + 8, stack=args)
            if it:
                e.w32(it + 4, count)
                e.w8(it + 0x10, b10)
        e.stub(0x479440, key='@campaign', fn=lambda em, k, a, p: em._ret(game, p))
        snap = e.snapshot()
        t.reset()
        e.call(0x4590b0, ecx=0x1234)
        out = [e.r32(game + 0xa8)]
        for st in (game + 8, game + 0x78):
            head = e.r32(st + 8)
            items, node = [], e.r32(head)
            while node != head:
                it = e.r32(node + 8)
                kind = KIND.get(e.r32(it), 9)
                tech = {1: e.r32(it + 0x18), 3: e.r32(it + 0x1c), 4: e.r8(it + 0x18)}.get(kind, 0xffffffff)
                ident = e.r32(it + 0x18) if kind == 3 else e.r32(it + 0x14)
                items += [kind, ident, tech, e.r32(it + 4), e.r8(it + 0x10)]
                node = e.r32(node)
            out += [len(items) // 5] + items
        return snap, out


def run_case(probe, world, seed, weapons):
    c = make_case(seed, weapons)
    snap, want = world.run(c)
    t = world.t
    rep = Replay(t, world.e, snap)
    rep.script['0x004b4900'] = lab_rounds
    probe.probe_app_set_callback(rep.callback())
    flat = [x for it in c['items'] for x in it] or [0]
    probe.probe_fc_setup(c['bases'], len(c['items']), (I * len(flat))(*flat))
    rep.reset()
    probe.probe_fc_run()
    out = (U * 2000)()
    got = list(out[:probe.probe_fc_image(out)])
    ok = compare(f'seed {seed}', t.log, rep.log)
    if ok and want != got:
        print(f'seed {seed}:\n  original {want}\n  rebuild  {got}')
        ok = False
    if not ok:
        print(f'  case {c}')
        return False
    keys = [k for k, _ in t.log]
    left = c['bases'] - 1
    mechs = sum(it[3] for it in c['items'] if it[0] == 0)
    cover('no base left', left <= 0)
    cover('two left', left == 2)
    cover('one left', left == 1)
    cover('mechs cut', '0x00484650' in keys)
    cover('mechs fewer than bases', left > 0 and 0 < mechs < left)
    cover('64 mechs', left > 0 and mechs >= 64)
    cover('ammo cut by rounds', '0x004b4900' in keys)
    n = 1 + 1 + 5 * want[1]
    moved = want[n]
    cover('items moved', moved > 0 and any(want[n + 1 + 5 * j] != 0 for j in range(moved)))
    gone = {(want[n + 1 + 5 * j], want[n + 2 + 5 * j]) for j in range(moved)}
    cover('unlimited copied', any(it[4] and it[0] and (it[0], it[1]) in gone for it in c['items']))
    tons = [a for k, a in t.log if k == '0x0040fe70' and a[1] == 0xf06]
    tons_before = sum(it[3] for it in c['items'] if it[0] == 3 and it[1] == 3)
    cover('the 300 t cap', left > 0 and tons_before * (0.7 if left == 2 else 0.5) > 300 * left)
    cover('nothing lost', tons and tons[0][2] == '0.00')
    return True


def main():
    probe = load_probe()
    probe.probe_app_set_callback.argtypes = [CALLBACK]
    probe.probe_fc_setup.argtypes = [I, I, ctypes.c_void_p]
    probe.probe_fc_image.argtypes = [ctypes.c_void_p]
    probe.probe_set_weapon_labs.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    labs = weapon_labs(READER.read_bytes())
    table = (ctypes.c_float * (WEAPONS * 5))()
    present = (ctypes.c_uint8 * WEAPONS)()
    for i, lab in enumerate(labs):
        if lab:
            present[i] = 1
            table[5 * i:5 * i + 5] = [lab[0], lab[1], 0, 0, lab[4]]
            ctypes.memmove(ctypes.addressof(table) + 20 * i + 8, struct.pack('<ii', lab[2], lab[3]), 8)
    probe.probe_set_weapon_labs(table, present)
    probe.probe_fc_hooks(1)
    weapons = [i for i, lab in enumerate(labs) if lab]
    world = World(labs)
    args = [a for a in sys.argv[1:] if not a.startswith('-')]
    runs = int(args[0]) if args else 600
    for seed in range(runs):
        if not run_case(probe, world, seed, weapons):
            sys.stdout.flush()
            os._exit(1)
    missed = sorted(k for k, n in COVER.items() if not n)
    if missed:
        print(f'not reached: {missed}')
        sys.stdout.flush()
        os._exit(1)
    print(f'ok: {runs} field bases lost as the original ({len(COVER)} branches reached)')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()
