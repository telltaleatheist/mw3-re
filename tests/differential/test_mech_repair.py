"""Mech::repair against the original x86 (docs/vehicles.md, The repair;
mech3/mech_repair): 0x4154e0 with the limp's end 0x415a90, the weapons'
ammo from the store (0x484960, the bins 0x4b4cf0 / 0x4b4d00 / 0x4b4dc0,
the item 0x41ebd0 and find_like 0x4714c0), the armour's cost (0x41ec00,
0x424760, the store's remove vt+0x14), coolant, jump fuel, the AMS, the
player's HUD (0x430aa0, 0x446520, 0x43be10) and the network bits.

    cmake --build build && .venv/bin/python tests/differential/test_mech_repair.py [runs]

The parts are fake objects whose calls (vt+0x3c set_internal, +0x40
set_armor, +0x44 get_armor, +0x4c get_armor_scale) are stubbed; the
weapons, their bins and the manager are laid out as the original's; the
store is the original's own (0x470ce0 and its adds). Compared: the traces,
the bins, the manager's ammo values, the mech's fields and the store's items.
"""
import ctypes
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import CALLBACK, Emu, Replay, Trace, compare, load_probe, rnd  # noqa: E402

U, F, I = ctypes.c_uint32, ctypes.c_float, ctypes.c_int32
COVER = {}
PARTS = ('leg0', 'leg1', 'arm0', 'arm1', 'head', 'torso')
KIND = {0x5c0fc0: 1, 0x5bd150: 2, 0x5bd180: 3}
USES = {0, 1, 2, 3, 9, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 26, 27, 28, 29, 30, 31, 33, 34, 35, 36}  # 0x5e6504
ADD = {1: 0x471840, 2: 0x471940, 3: 0x471a20}
# floor (an import): x87 rounding down, the precision kept (test_debriefing.py).
FLOOR = bytes.fromhex('83ec04 d93c24 668b0424 6625fff3 660d0004 6689442402 d96c2402 dd442408 d9fc d92c24 83c404 c3')


def cover(label, cond=True):
    COVER.setdefault(label, 0)
    if cond:
        COVER[label] += 1


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


def fb(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def i32(v):
    return v - (1 << 32) if v & 0x80000000 else v


def make_case(seed):
    r = rnd(seed, 'repair')
    c = {}
    c['ferro'] = r.random() < 0.4
    c['tech'] = r.choice([0, 1, 1, 2])
    c['tons'] = f32(r.choice([r.uniform(2, 20), 8.0, 12.5, 0.0]))
    c['ams'] = r.random() < 0.4
    c['eq0'] = r.choice([0, 1, 2])
    c['jets'] = r.choice([0, 0, 4, -1])
    c['rounds'] = r.randrange(0, 300)
    c['limp'] = r.choice([-1, 0, 1, 2])
    c['anim'] = r.random() < 0.6
    c['viewed'] = r.random() < 0.5
    c['f'] = [c['tons'], f32(r.uniform(0, 1)), f32(r.uniform(0, 1)), f32(r.uniform(0, 500)),
              f32(r.choice([0.0, r.uniform(0.0001, 0.05)]))]
    # Each part's armour and scale by section (the torso's six).
    c['armor'] = {p: [(f32(r.choice([0.0, 1.0, r.uniform(0, 1)])), f32(r.choice([r.uniform(0.3, 8), 100.0 / 18])))
                      for _ in range(6 if p == 'torso' else 1)] for p in PARTS}
    nb = r.randrange(0, 5)
    c['bins'] = [(r.randrange(0, 40), 0, r.random() < 0.15, fb(r.uniform(0, 15)), r.random() < 0.5) for _ in range(nb)]
    c['bins'] = [(a, a + r.choice([0, 0, r.randrange(1, 30)]), inf, d, wn) for a, _, inf, d, wn in c['bins']]
    ids = [3, 7, 12, 20, 25, 30]
    weapons = []
    for _ in range(r.randrange(0, 7)):
        present = r.random() < 0.9
        index = r.choice([-1, r.randrange(0, 0x28)])
        tid = r.choice(ids)
        b = r.randrange(-1, nb) if nb else -1
        weapons.append((int(present), index, tid, b))
    c['weapons'] = weapons
    items = []
    for _ in range(r.randrange(0, 7)):
        k = r.choice([1, 2, 2, 3, 3])
        if k == 2:
            items.append((2, r.choice(ids), -1, r.choice([0, 1, r.randrange(1, 50)]), int(r.random() < 0.2)))
        elif k == 3:
            items.append((3, r.choice([1, 2, 1, 2, 3]), r.choice([0, 1, 2]), r.choice([0, 1, r.randrange(1, 12)]),
                          int(r.random() < 0.2)))
        else:
            items.append((1, r.choice(ids), r.choice([0, 1]), 1, 0))
    c['items'] = items
    c['hud'] = r.choice([0x2d001000, 0x2d002000])
    return c


def armor_answer(c, part):
    def get(i):
        def fn(n, args):
            s = args[0] if part == 'torso' else 0
            return fb(c['armor'][part][s if s < len(c['armor'][part]) else 0][i])
        return fn
    return get


def run_original(c):
    t = Trace()
    e = Emu(t)
    e.heap_stubs()
    e.cpu.fpu_control(0x007f)  # play's 24-bit precision
    e.cpu.import_code(0x5bb66c, FLOOR)
    for p in PARTS:
        t.script[f'{p}.vt+0x44'] = armor_answer(c, p)(0)
        t.script[f'{p}.vt+0x4c'] = armor_answer(c, p)(1)
    t.script['0x00430aa0'] = c['hud']
    scratch = e.alloc(16)
    thunk = e.thunk(b'\xd9\x05' + struct.pack('<I', scratch) + b'\xc2\x04\x00')

    def fret(em, key, a, pop):
        v = em.t.handle(key, a)
        em.w32(scratch, v)
        em._ret(v, pop, then=thunk)
    parts = {p: e.object(p, size=0x200, slots={0x3c: ('s0,s1', 8), 0x40: ('s0,s1', 8),
                                                0x44: ('s0', 4, fret), 0x4c: ('s0', 4, fret)}) for p in PARTS}
    for addr in (0x430aa0, 0x446520, 0x43be10):
        e.stub(addr, spec='c')
    e.stub(0x4152c0, key='anim_free', spec='c')
    t.silent.add('anim_free')

    mech = e.alloc(0x6c0)
    for off, p in zip((0x38c, 0x390, 0x394, 0x398, 0x3a4, 0x39c), PARTS):
        e.w32(mech + off, parts[p])
    cfg = e.alloc(0xd8)
    e.w8(cfg + 0x34, int(c['ferro']))
    e.w32(cfg + 0x30, c['tech'])
    e.wf(cfg + 0x2c, c['tons'])
    e.w32(mech + 0x3ec, cfg)
    e.w32(mech + 0x3ac, c['eq0'])
    e.w32(mech + 0x3d8, c['jets'] & 0xffffffff)
    ams = 0
    if c['ams']:
        ams = e.alloc(0x20)
        e.w32(ams + 0x18, c['rounds'])
        e.w32(mech + 0x3f0, ams)
    e.w32(mech + 0x5ac, c['limp'] & 0xffffffff)
    if c['anim']:
        e.w32(mech + 0x450, e.alloc(0xa0))
    e.wf(mech + 0x56c, c['f'][1])
    e.wf(mech + 0x564, c['f'][2])
    e.w32(mech + 0x684, 0x684)
    view = e.world_alloc(0x70)
    e.w32(view + 0x64, mech if c['viewed'] else 0)
    e.w32(0x5f4c8c, view)

    man = e.alloc(0x94)
    e.wf(man + 0x8c, c['f'][3])
    e.wf(man + 0x90, c['f'][4])
    bins = []
    for amount, mx, inf, dmg, warned in c['bins']:
        b = e.alloc(0x18)
        e.w32(b, 100)
        e.w32(b + 4, amount)
        e.w32(b + 8, mx)
        e.w8(b + 0xc, int(inf))
        e.w8(b + 0xd, int(warned))
        e.w32(b + 0x10, man)
        e.w32(b + 0x14, dmg)
        bins.append(b)
    for k, (present, index, tid, bi) in enumerate(c['weapons']):
        if not present:
            continue
        w = e.alloc(0x60)
        d = e.alloc(0x1f4)
        ty = e.alloc(0x10)
        e.w32(ty, tid)
        e.w32(d + 0x1f0, ty)
        e.w32(w + 0x28, index & 0xffffffff)
        e.w32(w + 0x2c, d)
        e.w32(w + 0x58, bins[bi] if bi >= 0 else 0)
        e.w32(man + 0x40 + 4 * k, w)
    e.w32(man + 0x24, len(c['weapons']))
    e.w32(mech + 0xd4, man)

    store = e.alloc(0x14)
    e.call(0x470ce0, ecx=store, stack=(0,))
    for k, ident, tech, count, b10 in c['items']:
        args = (ident, 1) if k == 2 else (ident, tech, 1) if k == 3 else (ident, tech)
        it = e.call(ADD[k], ecx=store, stack=args)
        if it:
            e.w32(it + 4, count)
            e.w8(it + 0x10, b10)
    e.stub(0x479430, key='store', fn=lambda em, key, a, pop: em._ret(store, pop))
    snap = e.snapshot()
    t.reset()
    ret = e.call(0x4154e0, ecx=mech, stack=(fb(0.25),)) & 0xff

    out = []
    for b in bins:
        out += [e.r32(b + 4), e.r32(b + 8), e.r8(b + 0xd)]
    out += [e.r32(man + 0x8c), e.r32(man + 0x90), e.r32(mech + 0x56c), e.r32(mech + 0x564),
            e.r32(ams + 0x18) if ams else c['rounds'], e.r32(mech + 0x5ac), int(e.r32(mech + 0x450) != 0),
            e.r32(mech + 0x680)]
    head = e.r32(store + 8)
    items, n = [], e.r32(head)
    while n != head:
        it = e.r32(n + 8)
        kind = KIND.get(e.r32(it), 0)
        ident = e.r32(it + 0x18) if kind == 3 else e.r32(it + 0x14)
        tech = e.r32(it + 0x1c) if kind == 3 else e.r32(it + 0x18) if kind == 1 else 0xffffffff
        items += [kind, ident, tech, e.r32(it + 4), e.r8(it + 0x10)]
        n = e.r32(n)
    out += [len(items) // 5] + items
    return t, e, snap, ret, out


def run_case(probe, seed, verbose=False):
    c = make_case(seed)
    t, e, snap, ret, want = run_original(c)
    rep = Replay(t, e, snap)
    probe.probe_app_set_callback(rep.callback())
    ints = [int(c['ferro']), c['tech'], c['eq0'], c['jets'], int(c['ams']), c['rounds'], c['limp'], int(c['anim']),
            int(c['viewed']), len(c['weapons'])]
    w = [x for wp in c['weapons'] for x in wp] or [0]
    b = [x for a, m, inf, d, wn in c['bins'] for x in (a, m, int(inf), i32(d), wn)] or [0]
    s = [x for it in c['items'] for x in it] or [0]
    probe.probe_rp_setup((I * 10)(*ints), (F * 5)(*c['f']), (I * len(w))(*w), len(c['bins']), (I * len(b))(*b),
                         len(c['items']), (I * len(s))(*s))
    rep.reset()
    got_ret = probe.probe_rp_run(F(0.25))
    out = (U * 400)()
    k = probe.probe_rp_image(len(c['bins']), out)
    got = list(out[:k])

    ok = compare(f'seed {seed}', t.log, rep.log)
    if ok and (ret != got_ret or want != got):
        print(f'seed {seed}: return {ret} / {got_ret}\n  original {want}\n  rebuild  {got}')
        ok = False
    if not ok:
        print(f'  case {c}')
        return False
    keys = [k for k, _ in t.log]
    found_armour = any(k.endswith('vt+0x40') for k in keys)
    cover('armour not found', not found_armour)
    cover('viewed', c['viewed'] and '0x0043be10' in keys)
    cover('limp ended', c['limp'] in (0, 1) and c['anim'] and want[3 * len(c['bins']) + 5] == 0xffffffff)
    cover('limp kept', c['limp'] == 2 and c['anim'])
    cover('ams', c['ams'])
    cover('jets', c['jets'] > 0)
    cover('no jets', c['jets'] <= 0)
    for present, index, tid, bi in c['weapons']:
        if not present or index not in USES or bi < 0:
            cover('weapon skipped')
            continue
        amount, mx, inf, _, _ = c['bins'][bi]
        it = next((x for x in c['items'] if x[0] == 2 and x[1] == tid), None)
        cover('ammo none', it is None)
        cover('ammo infinite bin', inf)
        if it is not None and not inf and mx > amount:
            cover('ammo free', it[4] == 1)
            cover('ammo short', it[4] == 0 and it[3] < mx - amount)
            cover('ammo enough', it[4] == 0 and it[3] >= mx - amount)
    if found_armour:
        kind = 2 if c['ferro'] else 1
        it = next((x for x in c['items'] if x[0] == 3 and x[1] == kind and x[2] == c['tech']), None)
        cover('armour free', it is not None and it[4] == 1)
        n = 3 * len(c['bins']) + 8
        after = [tuple(want[n + 1 + 5 * j:n + 6 + 5 * j]) for j in range(want[n])]
        still = next((x for x in after if x[0] == 3 and x[1] == kind and x[2] == c['tech']), None)
        cover('armour paid', it is not None and it[4] == 0 and (still is None or still[3] < it[3]))
        cover('armour all gone', it is not None and it[4] == 0 and still is None)
        cover('armour full (no cost)', it is not None and it[4] == 0 and still is not None and still[3] == it[3])
        cover('ferro tech 1', c['ferro'] and c['tech'] == 1)
        cover('ferro other', c['ferro'] and c['tech'] != 1)
    return True


def main():
    probe = load_probe()
    probe.probe_app_set_callback.argtypes = [CALLBACK]
    P = ctypes.c_void_p
    probe.probe_rp_setup.argtypes = [P, P, P, I, P, I, P]
    probe.probe_rp_run.argtypes = [F]
    probe.probe_rp_run.restype = U
    probe.probe_rp_image.argtypes = [I, P]
    probe.probe_rp_hooks(1)
    args = [a for a in sys.argv[1:] if not a.startswith('-')]
    runs = int(args[0]) if args else 800
    for seed in range(runs):
        if not run_case(probe, seed):
            sys.stdout.flush()
            os._exit(1)
    missed = sorted(k for k, n in COVER.items() if not n)
    if missed:
        print(f'not reached: {missed}')
        sys.stdout.flush()
        os._exit(1)
    print(f'ok: {runs} repairs as the original ({len(COVER)} branches reached)')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()
