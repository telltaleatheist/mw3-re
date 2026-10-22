"""The firing's volley plan pick (docs/ai.md 17.7) against the original
x86: 0x4b6790 with its scorer 0x4b6b30, the candidates' helpers (0x4b6aa0,
0x4b6ac0, 0x4b6b10, 0x4b78e0, 0x4b7960, 0x4b7990, 0x4b7920), the partial
sort 0x4b79e0 with its comparator 0x4b5b60, the target taken 0x4b5a80 and
the volley 0x4b5a20; the plan's slots 0x4b6af0 (in reach), 0x4b6e10 (ready)
and 0x4b6fa0 (take); the sensor range 0x4d6d00.

    cmake --build build && .venv/bin/python tests/differential/test_ai_plan.py

The brain and its vehicle are test_ai_frame.py's. Added: four entities at
fixed addresses (their +0x08 active byte, team at +0xc0 behind vt+0x28,
health and heat as `fld` thunks answered from the trace, positions), six
contacts on them (+0x04 an object whose vt+0x2c answers the entity; random
dist2 and deltas, the distance the exe's own 0x4e7030), the vehicle's
allies, forward, target (+0x6c), range (+0xa0), weapon manager (+0xd4),
bearer (vt+0x40), can-face (vt+0x8c), target select (vt+0xc0: taken, the
target set), in reach (vt+0x150) answered from the trace; the clear line
0x4d5980, the volley 0x4b1860 and the plan's track (its vt+0x10, item 12b2)
answered from the trace; the firing's range, obstacle and the brain's aiv
+0x30. Each case picks, then asks the plan's slots; after each call the
calls, the plan (its slot, score, flags, kind, bearer), the ranges, the
obstacle, the vehicle's target and the contacts' distances must match, at
24-bit x87.
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
PICK = 0x4b6790
ENT = OWNER + 0x4000  # + 0x400 each, the probe's kPlanEnt
PLAN_VT = 0x5c9460
f2i = tf.f2i
COVER = {0x4b6a63: 'no contacts', 0x4b6a29: 'none taken: wider', 0x4b6a43: 'range doubled',
         0x4b6997: 'range kept past the target', 0x4b697d: 'capped at +0x30', 0x4b69cd: 'a plan',
         0x4b6aad: 'x', 0x4b6dda: 'rejected', 0x4b6b9f: 'too near', 0x4b6bb6: 'out of range',
         0x4b6beb: 'an ally', 0x4b6c05: 'within 40 m', 0x4b6c44: 'too steep', 0x4b6cef: 'near the obstacle',
         0x4b6cf5: 'the obstacle cleared', 0x4b6d5b: 'cannot face', 0x4b6dac: 'no clear line',
         0x4b68e7: 'removed', 0x4b7a5c: 'swapped', 0x4b5aa0: 'taken asked', 0x4b5a59: 'the volley',
         0x4b6fdc: 'take: alive', 0x4b7000: 'take: tracked', 0x4b6e22: 'ready'}
COVER.pop(0x4b6aad)
REACHED = set()


def script(seed):
    s = tf.script(seed)
    r = lambda salt, choices: (lambda n, args: random.Random(f'{seed}:{salt}:{n}').choice(choices))  # noqa: E731
    s['owner.vt+0x98'] = lambda n, args: f2i(random.Random(f'{seed}:hp:{n}').choice([1.0, 0.5, 0.25, 0.9]))
    s['owner.vt+0x9c'] = lambda n, args: f2i(random.Random(f'{seed}:heat:{n}').choice([0.0, 0.3, 1.0, 1.4]))
    s['vehicle.vt+0x8c'] = r('face', [1, 1, 0])
    s['vehicle.vt+0xc0'] = r('select', [1, 1, 1, 0])
    s['vehicle.vt+0x150'] = r('reach', [1, 1, 0, 2])
    s['0x004d5980'] = r('line', [1, 1, 0])
    s['0x004b1860'] = r('volley', [0, 1, 2, 3, 1])
    s['0x004b6e30'] = r('track', [1, 1, 0])
    return s


class Original(tf.Original):
    def __init__(self, seed):
        super().__init__(seed)
        e, b = self.e, self.brain
        self.t.script = script(seed)
        e.call(0x4b5b00)  # [0x60b2b4] = 0.5 squared
        e.call(0x4b5b30)  # [0x60b2b8]: the rejected candidate (its slot 0x60b2f4)
        ovt = OWNER + 0x400
        e.w32(ovt + 0x40, e.new_stub('@owner.vt+0x40', '', 0, self.bearer_of))
        e.w32(ovt + 0x8c, e.new_stub('vehicle.vt+0x8c', 's0', 4, self.owner_call))
        e.w32(ovt + 0xc0, e.new_stub('vehicle.vt+0xc0', 's0,s1', 8, self.select))
        e.w32(ovt + 0x150, e.new_stub('vehicle.vt+0x150', 's0', 4, self.reach))
        e.stub(0x4d5980, '0x004d5980', 'c,s0,s1', 8, self.line)
        e.stub(0x4b1860, '0x004b1860', 's0', 4, self.volley)
        e.stub(0x5a8630, '@0x005a8630', '', 0, self.lock_pos)
        e.w32(PLAN_VT + 0x10, e.new_stub('0x004b6e30', 's0', 4, self.track))
        # Entities 1..4: vtable +0x28 itself (0x4d2a70), +0x98 / +0x9c fld thunks.
        self.ents = [0]
        for k in range(1, 5):
            en = ENT + 0x400 * k
            vt = en + 0x200
            e.w32(en, vt)
            e.w32(vt + 0x28, 0x4d2a70)
            for slot, key, off in ((0x98, 'owner.vt+0x98', 0x300), (0x9c, 'owner.vt+0x9c', 0x320)):
                thunk = en + off
                e.cpu.write(thunk, b'\xd9\x05' + struct.pack('<I', en + off + 0x10) + b'\xc3')
                e.w32(vt + slot, thunk)
                e.uc.hook_add(UC_HOOK_CODE, self.fld_hit, begin=thunk, end=thunk, user_data=(key, en, en + off + 0x10))
            self.ents.append(en)
        # Contacts and what each sees (vt+0x2c: its entity).
        self.cts = [e.world_alloc(0x30) for _ in range(6)]
        self.seen = [e.world_alloc(0x10) for _ in range(6)]
        svt = e.world_alloc(0x40)
        e.w32(svt + 0x2c, e.new_stub('@seen.vt+0x2c', '', 0, self.seen_entity))
        for k in range(6):
            e.w32(self.seen[k], svt)
            e.w32(self.cts[k] + 4, self.seen[k])
        self.seen_ent = [0] * 6
        self.vec = e.world_alloc(0x40)
        self.wm = e.world_alloc(0x100)
        self.has_bearer = 1
        e.stub(0x5a85e0, '0x005a85e0', 'c', 0, self.position)
        for addr in COVER:
            e.uc.hook_add(UC_HOOK_CODE, lambda uc, ad, sz, u: REACHED.add(ad), begin=addr, end=addr)

    def ecx(self):
        return self.e.uc.reg_read(to.UC_X86_REG_ECX)

    def fld_hit(self, uc, address, size, user):
        key, en, val = user
        self.e.w32(val, self.t.handle(key, [en]))

    def position(self, e, key, a, pop):
        self.t.handle(key, a)
        o = a[0]
        e._ret(o + 0x40 if o in self.ents[1:] else self.pos, 0)

    def bearer_of(self, e, key, a, pop):
        e._ret(OWNER if self.has_bearer else 0, pop)

    def owner_call(self, e, key, a, pop):
        e._ret(self.t.handle(key, [OWNER]), pop)

    def reach(self, e, key, a, pop):
        e._ret(self.t.handle(key, [self.ecx(), a[0]]), pop)

    def contact_no(self, c):
        return self.cts.index(c) + 1 if c in self.cts else 0

    def select(self, e, key, a, pop):
        r = self.t.handle(key, [self.contact_no(a[0])])
        if r & 0xff:
            e.w32(OWNER + 0x6c, a[0])
        e._ret(r, pop)

    def line(self, e, key, a, pop):
        e._ret(self.t.handle(key, [a[0], a[1], 1 if a[2] else 0]), pop)

    def volley(self, e, key, a, pop):
        e._ret(self.t.handle(key, [self.contact_no(a[0])]), pop)

    def lock_pos(self, e, key, a, pop):
        e._ret(self.ecx() + 0x40, pop)

    def track(self, e, key, a, pop):
        e._ret(self.t.handle(key, [a[0]]), pop)

    def seen_entity(self, e, key, a, pop):
        k = self.seen.index(self.ecx())
        e._ret(self.ents[self.seen_ent[k]] if self.seen_ent[k] else 0, pop)

    def plan_set(self, w):
        e, b = self.e, self.brain
        for k in range(1, 5):
            en, x = self.ents[k], w[5 * (k - 1):5 * k]
            e.cpu.write(en + 8, bytes([x[0]]))
            e.w32(en + 0xc0, x[1])
            e.cpu.write(en + 0x40, struct.pack('<3I', *x[2:5]))
        v = w[20:32]
        e.w32(OWNER + 0xc4, v[0])
        e.w32(OWNER + 0x24, v[1])
        e.w32(OWNER + 0x2c, v[2])
        e.cpu.write(self.pos, struct.pack('<3I', *v[3:6]))
        e.w32(OWNER + 0xd4, self.wm if v[6] else 0)
        self.has_bearer = v[7]
        f = b + 0x108
        e.w32(f + 0x24, v[8])
        e.w32(f + 0x28, self.ents[v[9]] if v[9] else 0)
        e.w32(f + 0x2c, b)
        e.w32(b + 0x30, v[10])
        c = w[32:]
        n = c[0]
        for k in range(6):
            ct = self.cts[k]
            e.cpu.write(ct + 8, bytes(0x28))
            r = c[1 + 5 * k:6 + 5 * k] if k < n else [0, 0, 0, 0, 0]
            e.w32(ct + 0xc, r[0])
            e.cpu.write(ct + 0x10, struct.pack('<3I', *r[1:4]))
            e.w32(ct + 0x1c, f2i(-1.0))
            self.seen_ent[k] = r[4]
            e.w32(self.vec + 4 * k, ct)
        e.w32(OWNER + 0xac, self.vec)
        e.w32(OWNER + 0xb0, self.vec + 4 * n)
        e.w32(OWNER + 0x6c, self.cts[v[11] - 1] if v[11] else 0)
        e.w32(OWNER + 0xa0, v[8])  # the vehicle's sensor range as the firing's
        e.w32(OWNER + 0xa4, 0)
        self.plan = 0

    def slot_code(self, slot):
        if slot == OWNER + 0x6c:
            return 0xff
        if self.vec <= slot < self.vec + 0x18:
            return (slot - self.vec) // 4 + 1
        return 0 if slot == 0 else 0xffffffff

    def plan_op(self, op, f):
        e, b = self.e, self.brain
        out = [0] * 24
        if op == 0:
            self.plan = e.call(PICK, ecx=OWNER, edx=b + 0x108)
            out[0] = int(self.plan != 0)
        elif self.plan:
            p = self.plan
            if op == 1:
                out[0] = e.call(e.r32(PLAN_VT + 4), ecx=p, stack=(f,)) & 0xff
            elif op == 2:
                out[0] = e.call(e.r32(PLAN_VT + 0x18), ecx=p) & 0xff
            else:
                spent = self.vec + 0x30
                e.cpu.write(spent, b'\0')
                r = e.call(e.r32(PLAN_VT), ecx=p, stack=(spent,))
                out[0] = int(r == p) | (e.cpu.read(spent, 1)[0] << 8)
        if self.plan:
            p = self.plan
            out[1] = 1
            out[2] = self.slot_code(e.r32(p + 4))
            out[3] = e.r32(p + 8)
            out[4], out[5] = e.cpu.read(p + 0xc, 2)
            out[6], out[7] = e.r32(p + 0x10), e.r32(p + 0x14)
        f0 = b + 0x108
        out[8] = e.r32(f0 + 0x24)
        o = e.r32(f0 + 0x28)
        out[9] = self.ents.index(o) if o in self.ents[1:] else 0
        out[10], out[11] = e.r32(OWNER + 0xa0), e.r32(OWNER + 0xa4)
        out[12] = self.contact_no(e.r32(OWNER + 0x6c))
        out[13:19] = [e.r32(c + 0x1c) for c in self.cts]
        return out


def setup_probe():
    probe = tf.setup_probe()
    probe.probe_ao_plan_set.argtypes = [ctypes.c_void_p]
    probe.probe_ao_plan.argtypes = [I, U, ctypes.c_void_p]
    probe.probe_ao_plan_entity.argtypes = [I, I]
    return probe


def plan_world(rng):
    rf = lambda lo, hi: f2i(rng.uniform(lo, hi))  # noqa: E731
    w = []
    for _ in range(4):
        w += [int(rng.random() > 0.15), rng.choice([1, 2, 4, 2]), rf(-300, 300), rf(-5, 20), rf(-300, 300)]
    ang = rng.uniform(-3.14, 3.14)
    import math
    w += [rng.choice([0, 1, 2, 3]), f2i(math.sin(ang)), f2i(math.cos(ang)), rf(-50, 50), rf(0, 10), rf(-50, 50),
          int(rng.random() > 0.1), int(rng.random() > 0.1), rf(50, 900), rng.choice([0, 0, 0, 1, 2, 3, 4]),
          f2i(rng.choice([1000.0, 600.0, 300.0])), 0]
    n = rng.choice([0, 1, 2, 3, 4, 5, 6, 6])
    w += [n]
    for _ in range(6):
        d = rng.uniform(-1, 1), rng.choice([rng.uniform(-0.3, 0.3), rng.uniform(-1, 1)]), rng.uniform(-1, 1)
        scale = rng.choice([rng.uniform(1, 40), rng.uniform(40, 400), rng.uniform(400, 1200), 0.5])
        delta = [x * scale for x in d]
        dist2 = sum(x * x for x in delta) if rng.random() > 0.05 else rng.choice([0.5, 1e9])
        w += [f2i(dist2)] + [f2i(x) for x in delta] + [rng.choice([0, 1, 2, 3, 4, 1, 2])]
    w[31] = rng.choice([0] + list(range(1, n + 1))) if n else 0
    ob = w[29]
    if ob and n and rng.random() < 0.6:  # the obstacle near the line to the first contact
        import struct as _s
        f = lambda x: _s.unpack('<f', _s.pack('<I', x))[0]  # noqa: E731
        pos = [f(x) for x in w[23:26]]
        d = [f(x) for x in w[34:37]]
        ln = sum(x * x for x in d) ** 0.5 or 1.0
        t = rng.choice([rng.uniform(2, 9), rng.uniform(10, 60)])
        off = rng.choice([0.0, rng.uniform(-3, 3), rng.uniform(-30, 30)])
        p = [pos[0] + d[0] / ln * t + off, pos[1] + d[1] / ln * t, pos[2] + d[2] / ln * t - off]
        w[5 * (ob - 1) + 2:5 * (ob - 1) + 5] = [f2i(x) for x in p]
        w[5 * (ob - 1)] = 1
    return w


def run_case(probe, seed, rounds, stats):
    rng = random.Random(seed)
    orig = Original(seed)
    port = Trace(script(seed))
    probe.probe_app_set_callback(port.callback())
    probe.probe_ao_new()
    probe.probe_ao_op(0, 1, 1, 0)
    orig.op(0, 1, 1, 0)
    for k in range(rounds):
        w = plan_world(rng)
        orig.plan_set(w)
        probe.probe_ao_plan_set((U * len(w))(*w))
        for op in [0] + [rng.choice([1, 2, 3, 3, 4]) for _ in range(rng.randrange(0, 6))]:
            f = f2i(rng.uniform(0, 500))
            if op == 4:  # an entity dies or comes back between the plan's calls
                k, on = rng.randrange(1, 5), int(rng.random() < 0.3)
                orig.e.cpu.write(orig.ents[k] + 8, bytes([on]))
                probe.probe_ao_plan_entity(k, on)
                continue
            start = len(orig.t.log)
            a = orig.plan_op(op, f)
            out = (U * 24)()
            probe.probe_ao_plan(op, f, out)
            b = list(out)
            if orig.t.log != port.log:
                i = next((j for j, (x, y) in enumerate(zip(orig.t.log, port.log)) if x != y),
                         min(len(orig.t.log), len(port.log)))
                raise SystemExit(f'seed {seed} round {k} op {op}: calls differ at {i}:\n  before '
                                 f'{orig.t.log[start:i][-5:]}\n  exe  {orig.t.log[i:i + 4]}\n  port {port.log[i:i + 4]}')
            if a != b:
                names = ['answer', 'plan', 'slot', 'score', 'quick', 'line', 'kind', 'bearer', 'range', 'obstacle',
                         'v.range', 'v.range2', 'v.target'] + [f'dist{j}' for j in range(6)]
                bad = [f'{n} {x:#x} vs {y:#x}' for n, x, y in zip(names, a, b) if x != y]
                raise SystemExit(f'seed {seed} round {k} op {op}: {bad}')
            stats[op] = stats.get(op, 0) + 1
            if op == 0:
                stats['plans'] = stats.get('plans', 0) + a[0]
    probe.probe_ao_plan_done()
    probe.probe_ao_done()


def main():
    probe = setup_probe()
    cases = int(os.environ.get('PLAN_CASES', '300'))
    stats = {}
    for seed in range(cases):
        run_case(probe, seed, 20, stats)
    never = [COVER[a] for a in COVER if a not in REACHED]
    if never:
        raise SystemExit(f'branches never reached: {never}')
    print(f'ok: {cases} cases, {stats.get(0, 0)} picks ({stats.get("plans", 0)} plans), {stats.get(1, 0)} in reach, '
          f'{stats.get(2, 0)} ready, {stats.get(3, 0)} takes identical; {len(COVER)} branches reached')


if __name__ == '__main__':
    code = 0
    try:
        main()
    except SystemExit as e:
        print(e)
        code = 1
    sys.stdout.flush()
    os._exit(code)
