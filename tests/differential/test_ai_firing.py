"""The pilot's firing (docs/ai.md 17.7) against the original x86: the state
machine 0x4b5b80 at the brain's +0x108 with its drop 0x4b6690, the brain's
stand-down 0x4b6700, the look-around's start 0x4b63d0 and sweep 0x4b63f0,
the line-of-fire check 0x4b5940 (the contacts' distance 0x4e7030, the cone
0x4b5860 made at start-up), the brain's vt+0x14 0x4e95a0; and the AI
controls' IControls vt+0x80 0x4d8930 (scan_aim's manoeuvre).

    cmake --build build && .venv/bin/python tests/differential/test_ai_firing.py

The brain, its orders, behaviours and vehicle are test_ai_frame.py's (the
manoeuvres its fakes 88 and 98). Added: three fake plans (vt+0x04, +0x10,
+0x18 answered from the trace; +0x04 a pointer to one of three target slots,
+0x0c / +0x0d / +0x10 from the case), two fake sources (vt+0x00 answers a
plan and whether it is spent), operator delete logging theirs; the weapon
manager (its 0x4b04c0, 0x4b0bc0, 0x4b0a70 answered from the trace) and two
weapons (0x4b4ba0, vt+0x14 from the trace, +0x30 the reload, +0x2c's
+0x1f0 a type whose 0x4b4c90 is logged); the volley pick 0x4b6790 and the
aim's 0x4b72a0 / 0x4b77e0 (item 12b) answered from the trace; the vehicle's
target (+0x6c), range (+0xa0), +0xbc, contacts (+0xac..+0xb0, six with
random deltas, their distance worked out by the exe's own 0x4e7030), its
mech (vt+0x50) and torso (+0x39c, 0x465a00 logged); the IControls' vt+0x10
and vt+0x80. Each case loads a random firing state (every phase, plans and
sources present or not, times around now) and runs it, stands it down or
asks scan_aim; after each the calls, the firing's words, its sources, the
look-around, the brain's +0xf8 / +0xfc, the answer and the contacts'
distances must match, at 24-bit x87.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

os.environ.setdefault('MW3_X87', '24')
sys.path.insert(0, str(Path(__file__).parent))
import test_ai_frame as tf  # noqa: E402
import test_ai_orders as to  # noqa: E402
from calltrace import Trace  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402

U, I = ctypes.c_uint32, ctypes.c_int32
OWNER = tf.OWNER
RUN, STAND_DOWN, SCAN_AIM, CONE = 0x4b5b80, 0x4b6700, 0x4d8930, 0x4b5920
f2i = tf.f2i
COVER = {0x4b5be9: 'start', 0x4b5c14: 'the first source', 0x4b5c45: 'the second source', 0x4b5c54: 'the pick',
         0x4b5c7d: 'no plan', 0x4b5cad: 'quick', 0x4b5cbc: 'the delay, aim started', 0x4b5d23: 'tracked',
         0x4b5d60: 'in reach to fire', 0x4b5dd6: 'an alpha strike', 0x4b5e3c: 'fire on', 0x4b5eb3: 'a burst',
         0x4b5ef1: 'dropped', 0x4b5f1e: 'no target', 0x4b5f78: 'ready, reloaded', 0x4b5fc6: 'the line check',
         0x4b600c: 'a contact weighed', 0x4b6060: 'the burst over', 0x4b60c2: 'resting', 0x4b60f6: 'rested',
         0x4b610d: 'burst: no target', 0x4b617a: 'burst: reloading', 0x4b6202: 'burst: fire check',
         0x4b6294: 'fire', 0x4b62f0: 'held', 0x4b5985: 'the cone narrowing', 0x4b599e: 'past the cone',
         0x4b59a6: 'inside 30 m', 0x4b5a08: 'in the line', 0x4b6406: 'look behind', 0x4b6447: 'look left',
         0x4b6484: 'look right', 0x4b64d1: 'look again', 0x4d8963: 'scan_aim: a manoeuvre'}
REACHED = set()


def script(seed):
    s = tf.script(seed)
    r = lambda salt, choices: (lambda n, args: random.Random(f'{seed}:{salt}:{n}').choice(choices))  # noqa: E731
    s['plan.vt+0x4'] = r('reach', [1, 1, 1, 0, 2])
    s['plan.vt+0x10'] = r('track', [1, 1, 1, 0, 2])
    s['plan.vt+0x18'] = r('ready', [1, 1, 0, 3])
    s['source.vt+0x0'] = r('source', [0, 0x100, 1, 2, 0x101, 3])
    s['0x004b6790'] = r('pick', [0, 1, 2, 3, 1])
    s['0x004b04c0'] = r('current', [1, 2])
    s['0x004b4ba0'] = r('typeready', [1, 1, 0])
    s['weapon.vt+0x14'] = r('ammo', [5, 5, 0, 0xffffffff])
    return s


class Original(tf.Original):
    def __init__(self, seed):
        super().__init__(seed)
        e, b = self.e, self.brain
        self.t.script = script(seed)
        # MSVCRT's _hypot (two doubles, cdecl): fld x; fmul st; fld y; fmul st; faddp; fsqrt.
        e.cpu.import_code(0x5bb5b0, bytes.fromhex('dd442404 d8c8 dd44240c d8c8 dec1 d9fa c3'))
        e.call(CONE)  # [0x60b2c8]: the start-up's line cone
        bvt = e.r32(b)
        e.w32(bvt + 0x14, 0x4e95a0)  # the brain's vt+0x14: +0xf8 = 0
        ivt = e.r32(OWNER + 0xc30)
        e.w32(ivt + 0x10, e.new_stub('ictl.vt+0x10', 's0', 4, self.logged))
        e.w32(ivt + 0x80, e.new_stub('ictl.vt+0x80', '', 0, self.logged))
        # Plans 1..3 and sources 1..2: vtables of stubs, numbered at +0x30.
        pvt, svt = e.world_alloc(0x40), e.world_alloc(0x40)
        e.w32(pvt + 0x4, e.new_stub('plan.vt+0x4', 's0', 4, self.plan_call))
        e.w32(pvt + 0x10, e.new_stub('plan.vt+0x10', 's0', 4, self.plan_call))
        e.w32(pvt + 0x18, e.new_stub('plan.vt+0x18', '', 0, self.plan_call))
        e.w32(svt + 0x0, e.new_stub('source.vt+0x0', 's0', 4, self.source_take))
        self.slots = e.world_alloc(0x10)
        self.plans = [0] + [e.world_alloc(0x40) for _ in range(3)]
        for k in (1, 2, 3):
            e.w32(self.plans[k], pvt)
            e.w32(self.plans[k] + 0x30, k)
        self.sources = [0] + [e.world_alloc(0x40) for _ in range(2)]
        for k in (1, 2):
            e.w32(self.sources[k], svt)
            e.w32(self.sources[k] + 0x30, 0x100 + k)
        e.stub(0x5abd1a, '@delete', 's0', 0, self.delete)
        # The manager and two weapons: +0x30 the reload, vt+0x14 the ammo,
        # +0x2c a def whose +0x1f0 is the weapon's type.
        self.wm = e.world_alloc(0x100)
        wvt = e.world_alloc(0x40)
        e.w32(wvt + 0x14, e.new_stub('weapon.vt+0x14', '', 0, self.weapon_call))
        self.weapons, self.types = [0], {}
        for k in (1, 2):
            w, d, ty = e.world_alloc(0x40), e.world_alloc(0x200), e.world_alloc(0x100)
            e.w32(w, wvt)
            e.w32(w + 0x2c, d)
            e.w32(d + 0x1f0, ty)
            self.weapons.append(w)
            self.types[ty] = k
        e.stub(0x4b04c0, '0x004b04c0', 'c', 0, self.current)
        e.stub(0x4b0bc0, '0x004b0bc0', 'c,s0', 4, self.wm_call)
        e.stub(0x4b0a70, '0x004b0a70', 'c,s0', 4, self.wm_call)
        e.stub(0x4b4ba0, '0x004b4ba0', 'c', 0, self.weapon_call)
        e.stub(0x4b4c90, '0x004b4c90', 'c', 0, self.type_call)
        e.stub(0x4b6790, '0x004b6790', '', 0, self.pick)
        e.stub(0x4b72a0, '0x004b72a0', 's0,s1,s2', 12, self.aim_call)
        e.stub(0x4b77e0, '0x004b77e0', 's0,s1,s2,s3,s4', 20, self.aim_call)
        # The vehicle's mech (vt+0x50) and its torso (+0x39c).
        self.torso = e.world_alloc(0x40)
        e.w32(OWNER + 0x400 + 0x50, e.new_stub('@owner.vt+0x50', '', 0, self.mech_of))
        e.w32(OWNER + 0x39c, self.torso)
        e.stub(0x465a00, '0x00465a00', 's0', 4, self.logged)
        self.has_mech = 0
        self.aim = e.world_alloc(0x30)
        self.fire_contacts = [e.world_alloc(0x30) for _ in range(6)]
        self.vec = e.world_alloc(0x40)
        for addr in COVER:
            e.uc.hook_add(UC_HOOK_CODE, lambda uc, ad, sz, u: REACHED.add(ad), begin=addr, end=addr)

    def ecx(self):
        return self.e.uc.reg_read(to.UC_X86_REG_ECX)

    def plan_call(self, e, key, a, pop):
        e._ret(self.t.handle(key, [e.r32(self.ecx() + 0x30)] + list(a)), pop)

    def source_take(self, e, key, a, pop):
        r = self.t.handle(key, [e.r32(self.ecx() + 0x30) - 0x100])
        e.cpu.write(a[0], bytes([(r >> 8) & 1]))
        e._ret(self.plans[r & 0xff] if 1 <= (r & 0xff) <= 3 else 0, pop)

    def delete(self, e, key, a, pop):
        if a[0] in self.plans[1:] or a[0] in self.sources[1:]:
            self.t.handle('fire.delete', [e.r32(a[0] + 0x30)])
        e._ret(0, pop)

    def current(self, e, key, a, pop):
        e._ret(self.weapons[self.t.handle(key, [1])], pop)

    def wm_call(self, e, key, a, pop):
        e._ret(self.t.handle(key, [1 if self.ecx() else 0] + list(a[1:])), pop)  # a[0] is ECX

    def weapon_call(self, e, key, a, pop):
        e._ret(self.t.handle(key, [self.weapons.index(self.ecx())]), pop)

    def type_call(self, e, key, a, pop):
        e._ret(self.t.handle(key, [self.types[self.ecx()]]), pop)

    def pick(self, e, key, a, pop):
        r = self.t.handle(key, [])
        e._ret(self.plans[r] if 1 <= r <= 3 else 0, pop)

    def contact_no(self, c):
        return self.fire_contacts.index(c) + 1 if c in self.fire_contacts else 0

    def aim_call(self, e, key, a, pop):
        if key == '0x004b72a0':
            args = [self.contact_no(a[0]), self.weapons.index(a[1])]
        else:
            args = [a[0], self.contact_no(a[1]), self.weapons.index(a[2]), 1]
        e._ret(self.t.handle(key, args), pop)

    def mech_of(self, e, key, a, pop):
        e._ret(OWNER if self.has_mech else 0, pop)

    def fire_set(self, w):
        e, b = self.e, self.brain
        f = b + 0x108
        words = list(w[0:11])
        words[5] = self.plans[w[5]] if w[5] else 0
        e.cpu.write(f, struct.pack('<11I', *words))
        e.w32(f + 0x2c, b)
        e.w32(f + 0x34, 0x5c9458)  # the look-around's vtable
        e.w32(f + 0x54, self.aim)
        e.w32(f + 0x30, self.sources[w[11]] if w[11] else 0)
        e.w32(f + 0x50, self.sources[w[12]] if w[12] else 0)
        e.cpu.write(f + 0x38, struct.pack('<I5I', f, *w[13:18]))
        e.w32(b + 0xf0, w[18])
        e.cpu.write(b + 0xf8, bytes([w[19]]))
        e.w32(b + 0xfc, w[20])
        for off, x in zip((0x34, 0x38, 0xa8, 0xac, 0x98), w[21:26]):
            e.w32(b + off, x)
        n = w[48]
        for k in range(6):
            c = self.fire_contacts[k]
            e.cpu.write(c, bytes(0x30))
            e.cpu.write(c + 0x10, struct.pack('<3I', *(w[49 + 3 * k:52 + 3 * k] if k < n else [0, 0, 0])))
            e.w32(c + 0x1c, f2i(-1.0))
            e.w32(self.vec + 4 * k, c)
        e.w32(OWNER + 0xac, self.vec)
        e.w32(OWNER + 0xb0, self.vec + 4 * n)
        e.w32(OWNER + 0x6c, self.fire_contacts[w[26] - 1] if w[26] else 0)
        e.w32(OWNER + 0xa0, w[27])
        e.cpu.write(OWNER + 0xbc, bytes([w[28]]))
        e.w32(OWNER + 0xd4, self.wm if w[29] else 0)
        self.has_mech = w[30]
        for k in (1, 2):
            e.w32(self.weapons[k] + 0x30, w[30 + k])
        for k in range(3):
            d = w[33 + 4 * k:37 + 4 * k]
            p = self.plans[k + 1]
            e.w32(p + 4, self.slots + 4 * d[0])
            e.cpu.write(p + 0xc, bytes([d[1], d[2]]))
            e.w32(p + 0x10, d[3])
        for k in range(3):
            e.w32(self.slots + 4 * k, self.fire_contacts[w[45 + k] - 1] if w[45 + k] else 0)

    def fire(self, op, dt):
        e, b = self.e, self.brain
        f = b + 0x108
        answer = 0
        if op == 0:
            e.call(RUN, ecx=f, stack=(dt,))
        elif op == 1:
            e.call(STAND_DOWN, ecx=f)
        else:
            answer = e.call(SCAN_AIM, ecx=b + 0x2b0) & 0xff
        out = [0] * 32
        words = [e.r32(f + 4 * k) for k in range(11)]
        p = words[5]
        words[5] = self.plans.index(p) if p in self.plans[1:] else (0 if p == 0 else 0xbad)
        out[0:11] = words
        s1, s2 = e.r32(f + 0x30), e.r32(f + 0x50)
        out[11] = self.sources.index(s1) if s1 in self.sources[1:] else (0 if s1 == 0 else 0xbad)
        out[12] = self.sources.index(s2) if s2 in self.sources[1:] else (0 if s2 == 0 else 0xbad)
        out[13:18] = [e.r32(f + 0x3c + 4 * k) for k in range(5)]
        out[18] = int(e.r32(f + 0x38) == f)
        out[19] = e.cpu.read(b + 0xf8, 1)[0]
        out[20] = e.r32(b + 0xfc)
        out[21] = answer
        out[22:28] = [e.r32(c + 0x1c) for c in self.fire_contacts]
        return out


def setup_probe():
    probe = tf.setup_probe()
    probe.probe_ao_fire_set.argtypes = [ctypes.c_void_p]
    probe.probe_ao_fire.argtypes = [I, U, ctypes.c_void_p]
    probe.probe_ao_set_now.argtypes = [U]
    return probe


def fire_state(rng, now):
    rf = lambda lo, hi: f2i(rng.uniform(lo, hi))  # noqa: E731
    w = [rng.choice([0, 1, 2, 3, 4, 5, 6, 7, 8, 8]), f2i(now + rng.choice([-2.0, -0.5, 0.0, 0.3, 2.0, 6.0])),
         rf(0, 1), rf(0, 2), rf(0, 4), rng.choice([0, 1, 2, 3, 1, 2]), rf(0, 500), rf(0, 2), rf(0, 500), rf(0, 1000), 0]
    if w[0] in (2, 3, 4, 7) and not w[5]:  # those phases run on a plan (the exe reads it)
        w[5] = rng.choice([1, 2, 3])
    w += [rng.choice([0, 0, 1]), rng.choice([0, 0, 2])]
    w += [f2i(now + rng.choice([-1.0, 1.0, 3.0]))] + [f2i(rng.choice([0.0, -1.0, 1.0])) for _ in range(3)]
    w += [rng.choice([0, 1, 2, 3, 4, 5])]
    w += [rng.choice([0, 0, 1]), rng.randrange(2), f2i(now + rng.choice([-1.0, 0.0, 1.0]))]
    w += [rf(0, 1), rf(0, 3), rf(0, 5), rf(0, 2), rng.choice([0, 1, 1])]
    w += [rng.choice([0, 1, 2, 3, 4, 5, 6, 1]), rf(100, 1000), rng.choice([0, 0, 0x10, 0x1f]), int(rng.random() > 0.05),
          rng.randrange(2), rf(0, 2), rf(0, 2)]
    for _ in range(3):
        w += [rng.randrange(3), rng.randrange(2), rng.choice([0, 1, 1]), rng.choice([0, 1, 3, 3])]
    w += [rng.choice([1, 2, 3, 4, 5, 6]) for _ in range(3)]  # never empty: the line check reads it
    n = rng.randrange(0, 7)
    w += [n]
    for _ in range(6):
        if rng.random() < 0.3:  # near the line to slot 0's contact
            w += [f2i(rng.uniform(-5, 5)), f2i(rng.uniform(-2, 2)), f2i(rng.choice([rng.uniform(5, 40), rng.uniform(40, 250)]))]
        else:
            w += [f2i(rng.uniform(-300, 300)), f2i(rng.uniform(-20, 20)), f2i(rng.uniform(-300, 300))]
    return w


def run_case(probe, seed, calls, stats):
    rng = random.Random(seed)
    orig = Original(seed)
    port = Trace(script(seed))
    probe.probe_app_set_callback(port.callback())
    probe.probe_ao_new()
    probe.probe_ao_op(0, 1, 1, 0)
    orig.op(0, 1, 1, 0)
    now = rng.uniform(10, 400)
    for k in range(calls):
        if k % 6 == 0:
            g = tf.frame_state(rng, now)
            orig.frame_set(g)
            probe.probe_ao_frame_set((U * 18)(*g))
            w = fire_state(rng, now)
            orig.fire_set(w)
            probe.probe_ao_fire_set((U * len(w))(*w))
        now += rng.choice([0.0, 0.1, 0.5, 2.0])
        orig.e.w32(0x60991c, f2i(now))
        probe.probe_ao_set_now(f2i(now))
        op = rng.choice([0, 0, 0, 0, 0, 0, 1, 2])
        dt = f2i(rng.choice([1 / 30, 0.1]))
        start = len(orig.t.log)
        a = orig.fire(op, dt)
        out = (U * 32)()
        probe.probe_ao_fire(op, dt, out)
        b = list(out)
        if orig.t.log != port.log:
            i = next((j for j, (x, y) in enumerate(zip(orig.t.log, port.log)) if x != y),
                     min(len(orig.t.log), len(port.log)))
            raise SystemExit(f'seed {seed} call {k} op {op}: calls differ at {i}:\n  before {orig.t.log[start:i][-5:]}\n'
                             f'  exe  {orig.t.log[i:i + 4]}\n  port {port.log[i:i + 4]}')
        if a != b:
            names = ['phase', 'time', 'quick', 'delay', 'rest', 'plan', 'reach', 'track', 'fire', 'range', 'x28',
                     'first', 'second', 'until', 'dir.x', 'dir.y', 'dir.z', 'look', 'owner', 'xf8', 'xfc', 'answer'] + \
                [f'contact{j}' for j in range(6)]
            bad = [f'{n} {x:#x}/{tf.i2f(x) if hasattr(tf, "i2f") else x} vs {y:#x}' for n, x, y in zip(names, a, b) if x != y]
            raise SystemExit(f'seed {seed} call {k} op {op}: {bad}')
        stats[op] = stats.get(op, 0) + 1
        stats['phases'] = stats.get('phases', set()) | {a[0]}
    probe.probe_ao_fire_done()
    probe.probe_ao_done()


def main():
    probe = setup_probe()
    cases = int(os.environ.get('FIRE_CASES', '300'))
    stats = {}
    for seed in range(cases):
        run_case(probe, seed, 60, stats)
    never = [COVER[a] for a in COVER if a not in REACHED]
    if never:
        raise SystemExit(f'branches never reached: {never}')
    print(f'ok: {cases} cases, {stats.get(0, 0)} runs, {stats.get(1, 0)} stand-downs, {stats.get(2, 0)} scan_aims '
          f'identical; {len(COVER)} branches reached')


if __name__ == '__main__':
    code = 0
    try:
        main()
    except SystemExit as e:
        print(e)
        code = 1
    sys.stdout.flush()
    os._exit(code)
