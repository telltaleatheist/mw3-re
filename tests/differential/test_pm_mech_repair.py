"""PM's field base repair 0x415fc0 against the port's Mech::repair (mech3/mech_repair, PM's on this branch), as
test_mech_repair.py does MW3's 0x4154e0, on PM's exe and PM's layout: the internals (0x415e90), each weapon's ammo
(the game's type through [0x62c40c]+0x6c vt+0x28: in a campaign from the store by need, as MW3; outside one every
bin refilled free), the armour's lookup and cost only in a campaign and the armour made full either way (0x415f10),
coolant, jump fuel, the AMS, the limp's end with PM's speed set back (0x4164c0, 0x414c00), the player's HUD and the
network bits.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_mech_repair.py [runs]

The parts are fake objects whose calls (vt+0x3c set_internal, +0x40 set_armor, +0x44 get_armor, +0x4c
get_armor_scale) are traced and answered alike on both sides; the weapons, their bins, the manager and the store are
PM's own code on PM's layout (the manager's count +0x28 and list +0x44, a weapon's index +0x2c, definition +0x30 and
bin +0x5c, a definition's type +0x200). Compared: the traces, the bins, the manager's ammo values, the mech's
fields, its top and walking speeds and the store's items.
"""
import ctypes
import os
import struct
import sys
from pathlib import Path

os.environ['MW3_EXE'] = 'pm'
os.environ['MW3_PM_MOVED'] = '1'  # PM's own addresses, no map (pm_moved.py's mode)
sys.path.insert(0, str(Path(__file__).parent))
from calltrace import CALLBACK, Emu, Replay, Trace, compare, load_probe, rnd  # noqa: E402

U, F, I = ctypes.c_uint32, ctypes.c_float, ctypes.c_int32
COVER = {}
PARTS = ('leg0', 'leg1', 'arm0', 'arm1', 'head', 'torso')
PART_AT = {'leg0': 0x39c, 'leg1': 0x3a0, 'arm0': 0x3a4, 'arm1': 0x3a8, 'torso': 0x3ac, 'head': 0x3b4}
ADD = {1: 0x473ba0, 2: 0x473c90, 3: 0x473d70}  # MW3's 0x471840, 0x471940, 0x471a20
REPAIR, STORE_CTOR, STORE_OF, NEW, DELETE = 0x415fc0, 0x473170, 0x47b6b0, 0x5ce8c6, 0x5ce8c0
APP, VIEW = 0x62c40c, 0x61874c
FLOOR = bytes.fromhex('83ec04 d93c24 668b0424 6625fff3 660d0004 6689442402 d96c2402 dd442408 d9fc d92c24 83c404 c3')
IAT_FLOOR = 0x5de61c
KIND = {}  # PM's item vtables -> kind, read from the store's own adds


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
    r = rnd(seed, 'pm-repair')
    c = {}
    c['campaign'] = r.random() < 0.5
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
    c['mech'] = -1  # out of the table: 10 t on both sides (0x486830, max_mass)
    c['engine'] = r.randrange(0, 12)
    c['f'] = [c['tons'], f32(r.uniform(0, 1)), f32(r.uniform(0, 1)), f32(r.uniform(0, 500)),
              f32(r.choice([0.0, r.uniform(0.0001, 0.05)])), f32(r.uniform(5, 40)), f32(r.uniform(3, 24))]
    c['armor'] = {p: [(f32(r.choice([0.0, 1.0, r.uniform(0, 1)])), f32(r.choice([r.uniform(0.3, 8), 100.0 / 18])))
                      for _ in range(6 if p == 'torso' else 1)] for p in PARTS}
    nb = r.randrange(0, 5)
    # amount, max (0 to 30 more: full, partly full, empty), infinite, damage, warned
    bins = []
    for _ in range(nb):
        mx = r.choice([0, r.randrange(1, 40)])
        amount = r.choice([0, mx, r.randrange(0, mx + 1)])
        bins.append((amount, mx, r.random() < 0.15, fb(r.uniform(0, 15)), r.random() < 0.5))
    c['bins'] = bins
    ids = [3, 7, 12, 20, 25, 30]
    weapons = []
    for _ in range(r.randrange(0, 7)):
        present = r.random() < 0.9
        index = r.choice([-1, r.randrange(0, 0x38)])
        weapons.append((int(present), index, r.choice(ids), r.randrange(-1, nb) if nb else -1))
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


def item_kinds(e):
    """PM's item vtables: one of each kind added to a scratch store."""
    st = e.alloc(0x14)
    e.call(STORE_CTOR, ecx=st, stack=(0,))
    for k, args in ((1, (3, 0)), (2, (3, 1)), (3, (1, 1, 1))):
        it = e.call(ADD[k], ecx=st, stack=args)
        KIND[e.r32(it)] = k


def run_original(c):
    t = Trace()
    e = Emu(t)
    e.heap_stubs(new=NEW, delete=DELETE)
    e.cpu.fpu_control(0x007f)  # play's 24-bit precision, as MW3's test
    e.cpu.import_code(IAT_FLOOR, FLOOR)
    if not KIND:
        item_kinds(e)
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
    # The HUD under MW3's names, as the port's probe calls them (PM's 0x430e30, 0x4480a0, 0x43cef0).
    e.stub(0x430e30, key='0x00430aa0', spec='c')
    e.stub(0x4480a0, key='0x00446520', spec='c')
    e.stub(0x43cef0, key='0x0043be10', spec='c')
    e.stub(0x415cd0, key='anim_free', spec='c')  # the limp animation's destructor; its delete is the heap's
    e.stub(0x41fc00, key='anim_speed', spec='c')  # 0x414c00's per-animation stride (none left: anims[11] went)
    e.stub(0x432d40, key='gait_speed', spec='c')  # 0x414c00's gaits (the port's: none without a pilot)
    t.silent |= {'anim_free', 'anim_speed', 'gait_speed'}

    mech = e.alloc(0x700)
    for p in PARTS:
        e.w32(mech + PART_AT[p], parts[p])
    cfg = e.alloc(0x40)
    e.w8(cfg + 0x34, int(c['ferro']))
    e.w32(cfg + 0x30, c['tech'])
    e.wf(cfg + 0x2c, c['tons'])
    e.w32(cfg + 0x14, c['mech'] & 0xffffffff)
    e.w32(cfg + 0x24, c['engine'])
    e.w32(mech + 0x3fc, cfg)
    e.w32(mech + 0x3bc, c['eq0'])
    e.w32(mech + 0x3e8, c['jets'] & 0xffffffff)
    ams = 0
    if c['ams']:
        ams = e.alloc(0x20)
        e.w32(ams + 0x18, c['rounds'])
        e.w32(mech + 0x400, ams)
    e.w32(mech + 0x5d0, c['limp'] & 0xffffffff)
    if c['anim']:
        e.w32(mech + 0x460, e.alloc(0xa0))
    e.wf(mech + 0x590, c['f'][1])
    e.wf(mech + 0x588, c['f'][2])
    e.wf(mech + 0x528, c['f'][5])
    e.wf(mech + 0x520, c['f'][6])
    e.w32(mech + 0x6a8, 0x684)  # the HUD's owner, as the port's probe names it
    view = e.world_alloc(0x70)
    e.w32(view + 0x64, mech if c['viewed'] else 0)
    e.w32(VIEW, view)
    game = e.alloc(0x20)
    gvt = e.alloc(0x40)
    e.w32(game, gvt)
    e.w32(gvt + 0x28, e.thunk(b'\xb8' + struct.pack('<I', 1 if c['campaign'] else 3) + b'\xc3'))
    app = e.alloc(0x80)
    e.w32(app + 0x6c, game)
    e.w32(APP, app)

    man = e.alloc(0x100)
    e.wf(man + 0x90, c['f'][3])
    e.wf(man + 0x94, c['f'][4])
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
        w = e.alloc(0x80)
        d = e.alloc(0x210)
        ty = e.alloc(0x10)
        e.w32(ty, tid)
        e.w32(d + 0x200, ty)
        e.w32(w + 0x2c, index & 0xffffffff)
        e.w32(w + 0x30, d)
        e.w32(w + 0x5c, bins[bi] if bi >= 0 else 0)
        e.w32(man + 0x44 + 4 * k, w)
    e.w32(man + 0x28, len(c['weapons']))
    e.w32(mech + 0xd4, man)

    store = e.alloc(0x14)
    e.call(STORE_CTOR, ecx=store, stack=(0,))
    for k, ident, tech, count, b10 in c['items']:
        args = (ident, 1) if k == 2 else (ident, tech, 1) if k == 3 else (ident, tech)
        it = e.call(ADD[k], ecx=store, stack=args)
        if it:
            e.w32(it + 4, count)
            e.w8(it + 0x10, b10)
    e.stub(STORE_OF, key='store', fn=lambda em, key, a, pop: em._ret(store, pop))
    snap = e.snapshot()
    t.reset()
    ret = e.call(REPAIR, ecx=mech, stack=(fb(0.25),)) & 0xff

    out = []
    for b in bins:
        out += [e.r32(b + 4), e.r32(b + 8), e.r8(b + 0xd)]
    out += [e.r32(man + 0x90), e.r32(man + 0x94), e.r32(mech + 0x590), e.r32(mech + 0x588),
            e.r32(ams + 0x18) if ams else c['rounds'], e.r32(mech + 0x5d0), int(e.r32(mech + 0x460) != 0),
            e.r32(mech + 0x6a4), e.r32(mech + 0x528), e.r32(mech + 0x520)]
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
    probe.probe_rp_pm(1, int(c['campaign']))
    ints = [int(c['ferro']), c['tech'], c['eq0'], c['jets'], int(c['ams']), c['rounds'], c['limp'], int(c['anim']),
            int(c['viewed']), len(c['weapons']), c['mech'], c['engine']]
    w = [x for wp in c['weapons'] for x in wp] or [0]
    b = [x for a, m, inf, d, wn in c['bins'] for x in (a, m, int(inf), i32(d), wn)] or [0]
    s = [x for it in c['items'] for x in it] or [0]
    probe.probe_rp_setup((I * 12)(*ints), (F * 7)(*c['f']), (I * len(w))(*w), len(c['bins']), (I * len(b))(*b),
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
    camp = c['campaign']
    cover('campaign', camp)
    cover('not a campaign', not camp)
    armour_paid = any(k.startswith('store') for k in keys)
    cover('viewed', c['viewed'] and '0x0043be10' in keys)
    nb = len(c['bins'])
    cover('limp ended (speed set)', c['limp'] in (0, 1) and c['anim'] and want[3 * nb + 5] == 0xffffffff)
    cover('limp kept', c['limp'] == 2 and c['anim'])
    for present, index, tid, bi in c['weapons']:
        if not present or bi < 0:
            continue
        amount, mx, inf, _, _ = c['bins'][bi]
        if inf:
            cover('infinite bin')
            continue
        after = want[3 * bi]
        cover('empty bin refilled free', not camp and amount == 0 and mx > 0 and after == mx)
        cover('partly full bin refilled free', not camp and 0 < amount < mx and after == mx)
        cover('campaign bin from the store', camp and mx > amount and after > amount)
        cover('campaign bin left short', camp and mx > amount and after < mx)
    n = 3 * nb + 10
    before = [x for x in c['items'] if x[0] == 3]
    after_items = [tuple(want[n + 1 + 5 * j:n + 6 + 5 * j]) for j in range(want[n])]
    kind = 2 if c['ferro'] else 1
    it = next((x for x in c['items'] if x[0] == 3 and x[1] == kind and x[2] == c['tech']), None)
    still = next((x for x in after_items if x[0] == 3 and x[1] == kind and x[2] == c['tech']), None)
    cover('armour skipped outside a campaign (its item untouched)', not camp and it is not None and it[4] == 0
          and it[3] > 0 and still is not None and still[3] == it[3])
    cover('armour paid in a campaign', camp and it is not None and it[4] == 0 and (still is None or still[3] < it[3]))
    cover('armour made full', keys.count('torso.vt+0x40') == 6)
    del armour_paid, before
    return True


def main():
    probe = load_probe()
    probe.probe_app_set_callback.argtypes = [CALLBACK]
    P = ctypes.c_void_p
    probe.probe_rp_setup.argtypes = [P, P, P, I, P, I, P]
    probe.probe_rp_run.argtypes = [F]
    probe.probe_rp_run.restype = U
    probe.probe_rp_image.argtypes = [I, P]
    probe.probe_rp_pm.argtypes = [I, I]
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
