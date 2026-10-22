"""The combat manoeuvre (docs/ai.md 17.2) against the original x86: its
states 0x4caae0 / 0x4cabe0 over 0x4ca440 (the dtor 0x4ca570), the run
0x4ca600, the suspend 0x4ca5f0 into 0x60b450's state (0x4ca3b0: its run
0x4ca420 and resume 0x4ca410), and the brain's manoeuvre 0x4e96b0 that
pushes it.

    cmake --build build && .venv/bin/python tests/differential/test_ai_manoeuvre.py

The brain, its orders, vehicle and controls are test_ai_node.py's. Added:
the real behaviours at 0x604088, 0x604098 and 0x60b450; the vehicle's jets'
fuel (vt+0x148) and speed (vt+0x114, answered from the trace); the hit
record's check (0x4aec80, as the trace's IControls vt+0xa0) and attacker
(+0x0c); the vehicle's target contact (+0x6c) and the lead point (0x41d940,
from the trace: the target's place moved by (1.5, 0, -2.5)); three
contacts' seen objects and their places (0x5a8630). Each case puts a
manoeuvre on the current order or pushes one through the brain, then runs,
resumes, suspends, pushes and pops orders over it and changes the world
(the clock, the speed, the fuel, airborne, the height, the targets); after
each call the calls, the state (its +0x08..+0x2c and behaviour) and the
brain's +0xf0, +0xf8, +0x104 and orders must match, at 24-bit x87.
"""
import ctypes
import math
import os
import random
import struct
import sys
from pathlib import Path

os.environ.setdefault('MW3_X87', '24')
sys.path.insert(0, str(Path(__file__).parent))
import test_ai_node as tn  # noqa: E402
import test_ai_orders as to  # noqa: E402
from calltrace import Trace  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402

U, I = ctypes.c_uint32, ctypes.c_int32
OWNER = tn.OWNER
SET_BEHAVIOUR, MANOEUVRE = 0x4e8120, 0x4e96b0
BEHS = {0: 0x604098, 1: 0x604088, 2: 0x60b450}
MAN_VT, MAN2_VT, AFTER_VT = 0x5cb8e8, 0x5cb908, 0x5cb8a0
CONTROLS = OWNER + 0xc30  # the IControls base (test_ai_frame.py): the hit record at +4, the attacker at +0xc
f2i, fi = tn.f2i, tn.fi
COVER = {
    0x4ca4b6: 'made: jets', 0x4ca4ba: 'made: no jets', 0x4ca653: 'fuel low: off', 0x4ca66b: 'fuel high: on',
    0x4ca682: 'time up', 0x4ca6a1: 'airborne speed', 0x4ca6a8: 'ground speed', 0x4ca6c5: 'backing',
    0x4ca6e0: 'backing fast', 0x4ca6fa: 'forward', 0x4ca715: 'forward fast', 0x4ca722: 'still',
    0x4caaa7: 'braked', 0x4ca9da: 'the target sought', 0x4ca9e7: 'an enemy', 0x4ca9f4: 'no enemy',
    0x4caa15: 'the attacker', 0x4caa35: 'anyone', 0x4caa53: 'a target', 0x4ca749: 'facing',
    0x4ca77d: 'the lead asked', 0x4ca78c: 'the target\'s place', 0x4ca7d9: 'behind', 0x4ca7ef: 'behind, no turn',
    0x4ca8fa: 'ahead', 0x4ca810: 'off ahead', 0x4ca853: 'off behind', 0x4ca865: 'full left',
    0x4ca87f: 'left', 0x4ca897: 'left full', 0x4ca8b3: 'left least', 0x4ca8c7: 'right', 0x4ca8df: 'right full',
    0x4ca8f3: 'right least', 0x4ca922: 'ended: not attacking', 0x4ca964: 'jets', 0x4ca96a: 'no jets',
    0x4ca99e: 'thrust', 0x4ca9a3: 'throttle', 0x4ca5f0: 'suspended', 0x4ca420: 'after: run',
    0x4ca410: 'after: resumed', 0x4ca570: 'gone'}
REACHED = set()


def script(seed):
    s = tn.script(seed)
    r = lambda salt, choices: (lambda n, args: random.Random(f'{seed}:{salt}:{n}').choice(choices))  # noqa: E731
    s['vehicle.vt+0x114'] = r('spd', [f2i(x) for x in (0.0, 0.5, -0.5, 1.0, 3.0, -3.0, 8.0, -9.0, 20.0, -20.0, 1.2, 1.02, -1.03)])
    s['0x0041d940'] = r('lead', [0, 1, 1])
    s['0x004d6b90'] = r('pick', [0, 0, 1, 2, 3])
    return s


class Original(tn.Original):
    def __init__(self, seed):
        super().__init__(seed)
        e = self.e
        self.t.script = script(seed)
        for a, vt in ((0x604088, 0x5bcaf8), (0x604098, 0x5bcbb0), (0x60b450, 0x5cb7e8)):
            e.cpu.write(a, struct.pack('<3I', vt, 0, 0))
        nvt = e.r32(OWNER)
        self.fuel = e.world_alloc(0x10)
        fuel_thunk = e.thunk(b'\xd9\x05' + struct.pack('<I', self.fuel) + b'\xc3')
        e.w32(nvt + 0x148, fuel_thunk)
        e.w32(nvt + 0x114, self.fld_thunk('vehicle.vt+0x114', 0, 'c'))
        e.stub(0x4aec80, 'ictl.vt+0xa0', '', 4, self.logged)
        e.stub(0x41d940, '0x0041d940', 's0', 4, self.lead)
        e.stub(0x5a8630, '@0x005a8630', 'c', 0, self.lock_position)
        e.stub(0x4d6b90, '0x004d6b90', 'c,s0', 4, self.pick_contact)  # the node test's pick() shadows the frame's
        self.seen_pos = {}
        self.zero = e.world_alloc(0x10)
        for addr in COVER:
            e.uc.hook_add(UC_HOOK_CODE, lambda uc, ad, sz, u: REACHED.add(ad), begin=addr, end=addr)

    def fld_thunk(self, key, pop, spec=''):
        """`fld` of a value the trace answers, then ret: a float slot."""
        e = self.e
        val = e.alloc(0x10)
        code = b'\xd9\x05' + struct.pack('<I', val) + (b'\xc2' + struct.pack('<H', pop) if pop else b'\xc3')
        t = e.thunk(code)

        def hit(uc, address, size, user):
            e.w32(val, self.t.handle(key, e.args(spec)))
        e.uc.hook_add(UC_HOOK_CODE, hit, begin=t, end=t)
        return t

    def pick_contact(self, e, key, a, pop):
        n = self.t.handle(key, a)
        e._ret(self.contact[n - 1] if n else 0, pop)

    def lock_position(self, e, key, a, pop):
        e._ret(self.seen_pos.get(a[0], self.zero), pop)  # an attacker not seen: the probe's zeros

    def lead(self, e, key, a, pop):
        c = e.r32(OWNER + 0x6c)
        t = e.r32(c + 4)
        r = self.t.handle(key, [t])
        if r & 1:
            p = struct.unpack('<3f', e.cpu.read(self.seen_pos[t], 12))
            e.cpu.write(a[0], struct.pack('<3f', p[0] + 1.5, p[1], p[2] - 2.5))
        e._ret(r, pop)

    def man_world(self, w):
        e, b = self.e, self.brain
        e.w32(self.fuel, w[0])
        e.w32(CONTROLS + 0xc, w[1])
        e.w32(OWNER + 0x6c, self.contact[w[2] - 1] if w[2] else 0)
        for k in range(3):
            e.w32(self.contact[k] + 4, w[3 + k])
            obj = w[21 + k]
            if obj not in self.seen_pos:
                self.seen_pos[obj] = e.world_alloc(0x10)
            e.cpu.write(self.seen_pos[obj], struct.pack('<3I', *w[6 + 3 * k:9 + 3 * k]))
        for off, x in zip((0x6c, 0x70, 0x7c), w[15:18]):
            e.w32(b + off, x)
        e.cpu.write(b + 0xcc, bytes([w[18]]))
        e.cpu.write(b + 0xf8, bytes([w[19]]))
        e.w32(b + 0xa4, w[20])

    def man(self, op, which, dt):
        e, b = self.e, self.brain
        cur = self.orders()[-1]
        st = e.r32(cur + 0x10)
        vt = e.r32(st) if st else 0
        if op == 0:
            e.call(SET_BEHAVIOUR, ecx=cur, stack=(BEHS[which],))
        elif op == 4:
            e.call(MANOEUVRE, ecx=b)
        elif op in (1, 2, 3) and vt in (MAN_VT, MAN2_VT, AFTER_VT):
            slot = {1: 0x14, 2: 0x10, 3: 0x0c}[op]
            e.call(e.r32(vt + slot), ecx=st, stack=(dt,) if op == 1 else ())
        out = [0] * 24
        cur = self.orders()[-1]
        st = e.r32(cur + 0x10)
        vt = e.r32(st) if st else 0
        if vt in (MAN_VT, MAN2_VT):
            out[0] = 1
            out[1:11] = [e.r32(st + 8 + 4 * k) for k in range(10)]
        elif vt == AFTER_VT:
            out[0] = 2
        elif vt == self.state_vt:
            out[0] = -e.r32(st + 0xc) & 0xffffffff
        if out[0] in (1, 2):
            out[11] = e.call(e.r32(vt + 0x18), ecx=st)
        out[12] = e.r32(b + 0xf0)
        out[13] = e.cpu.read(b + 0xf8, 1)[0]
        out[14] = e.cpu.read(b + 0x104, 1)[0]
        e.cpu.write(b + 0x104, b'\0')
        out[15] = len(self.orders())
        return out


def setup_probe():
    probe = tn.setup_probe()
    probe.probe_ao_man_world.argtypes = [ctypes.c_void_p]
    probe.probe_ao_man.argtypes = [I, I, U, ctypes.c_void_p]
    return probe


def man_world(rng, orig, pos):
    seen = [orig.e.world_alloc(0x10) for _ in range(3)] if not hasattr(orig, 'seen_objs') else orig.seen_objs
    orig.seen_objs = seen
    pick = lambda: rng.choice(seen + [0])  # noqa: E731
    w = [f2i(rng.choice([1.0, 0.05, 0.1, 0.5, 0.7, 0.6, rng.uniform(0, 1)])), pick() if rng.random() < 0.7 else 0x5000,
         rng.choice([0, 1, 2, 3])]
    w += [rng.choice(seen) if rng.random() < 0.85 else 0 for _ in range(3)]
    for _ in range(3):  # now and then right on the vehicle (no way: none), or a NaN place (a NaN turn)
        on, nan = rng.random() < 0.08, rng.random() < 0.04
        w += [0x7fc00000 if nan else pos[0] if on else f2i(rng.uniform(-60, 60)), f2i(rng.uniform(0, 8)),
              pos[2] if on else f2i(rng.uniform(-60, 60))]
    lo = rng.choice([1.0, 3.0, 0.0, rng.uniform(0, 5)])
    w += [f2i(lo), f2i(lo + rng.choice([0.0, 2.0, 5.0, rng.uniform(0, 10)])), f2i(rng.choice([3.4028235e38, 5.0, 0.0, -1.0]))]
    w += [rng.choice([0, 0x40, 0x41, 0x44]), int(rng.random() < 0.5), rng.choice([0, 1, 1, 0, 2])]
    return w + seen  # the three seen objects, whose places w[6:15] are


def run_case(probe, seed, calls, stats):
    rng = random.Random(seed)
    orig = Original(seed)
    port = Trace(script(seed))
    probe.probe_app_set_callback(port.callback())
    probe.probe_ao_new()
    probe.probe_ao_op(0, 1, 1, 0)
    orig.op(0, 1, 1, 0)
    orig.world([], [])
    orig.e.w32(orig.brain + 0x80, 1)
    probe.probe_ao_steer_world((U * 1)(0), 0, (U * 1)(0), 0)
    params = (U * 4)()
    probe.probe_ao_turn_params(params)
    orig.e.cpu.write(0x60b478, struct.pack('<4I', *params))
    rows, nodes = [[0, f2i(0.0), f2i(0.0), f2i(0.0), -1, -1, -1, -1, 0, 0, 0, 0, 0]], [[0.0, 0.0, 0.0]]
    orig.net(rows)
    flat = [x & 0xffffffff for r in rows for x in r]
    probe.probe_ao_node_net((U * len(flat))(*flat), len(rows))
    w = tn.world(rng, nodes)
    w[8] = rng.choice([0, 0, 1, 2, 4])  # jump jets
    orig.node_world(w)
    probe.probe_ao_node_world((U * 29)(*w))
    mw = man_world(rng, orig, w[11:14])
    orig.man_world(mw)
    probe.probe_ao_man_world((U * 24)(*mw))
    out = (U * 24)()
    fakes = [b for b in to.BEHS if b not in (88, 98)]

    def compare(what, a, b):
        if orig.t.log != port.log:
            i = next((j for j, (x, y) in enumerate(zip(orig.t.log, port.log)) if x != y),
                     min(len(orig.t.log), len(port.log)))
            raise SystemExit(f'seed {seed} {what}: calls differ at {i}:\n  before {orig.t.log[max(0, i - 5):i]}\n'
                             f'  exe  {orig.t.log[i:i + 4]}\n  port {port.log[i:i + 4]}')
        if a != b:
            names = ['state', 'no_jets', 'jump', 'phase', 'turn', 'until', 'x1c', 'x20', 'x24', 'x28', 'target',
                     'behaviour', 'xf0', 'xf8', 'x104', 'orders']
            bad = [f'{n} {x:#x}/{fi(x)} vs {y:#x}/{fi(y)}' for n, x, y in zip(names, a, b) if x != y]
            raise SystemExit(f'seed {seed} {what}: {bad}')

    for k in range(calls):
        r = rng.random()
        top = orig.man(9, 0, 0)  # the state on top, nothing done
        probe.probe_ao_man(9, 0, 0, out)
        compare(f'call {k} look', top, list(out))
        if top[0] not in (1, 2) and r < 0.5 or r < 0.04:
            # Set on an order above the first (in play it is always pushed: a
            # pop of the first order runs the exe's deque off its start).
            op, which, dt = rng.choice([0, 0, 4]) if len(orig.orders()) > 1 else 4, rng.choice([0, 1, 1, 2]), 0
        elif r < 0.7:
            op, which, dt = 1, 0, f2i(1 / 30)
        elif r < 0.74:
            op, which, dt = 2, 0, 0
        elif r < 0.77:
            op, which, dt = 3, 0, 0
        elif r < 0.81:  # another order pushed on top (the manoeuvre suspended), at times popped again
            n2, b = rng.choice([1, 2, 3, 4]), rng.choice(fakes)
            orig.op(1, n2, b, 0)
            probe.probe_ao_op(1, n2, b, 0)
            if rng.random() < 0.6:
                orig.op(7, 0, 0, 0)
                probe.probe_ao_op(7, 0, 0, 0)
            stats['push'] = stats.get('push', 0) + 1
            continue
        elif r < 0.84:
            if len(orig.orders()) > 1:
                orig.op(7, 0, 0, 0)
                probe.probe_ao_op(7, 0, 0, 0)
                stats['pop'] = stats.get('pop', 0) + 1
            continue
        else:  # the world: the clock on, the vehicle's lot, the targets
            x = rng.random()
            w[6] = f2i(fi(w[6]) + rng.choice([0.25, 0.5, 1.0, 3.0]))
            if x < 0.4:
                ang = rng.uniform(-math.pi, math.pi)
                w[0], w[2] = f2i(math.sin(ang)), f2i(math.cos(ang))
                w[27] = int(rng.random() < 0.3)
                w[26] = f2i(rng.choice([0.0, 0.5, -3.0, 6.0, -12.0, rng.uniform(-15, 15)]))
                w[12] = f2i(rng.choice([0.0, 2.0, 6.0, 10.0]))  # the height (+0x7c's test)
            elif x < 0.7:
                mw = man_world(rng, orig, w[11:14])
                orig.man_world(mw)
                probe.probe_ao_man_world((U * 24)(*mw))
            else:
                mw[0] = f2i(rng.choice([0.05, 0.08, 0.1, 0.3, 0.6, 0.61, 0.9]))
                mw[19] = 0  # the firing let go (+0xf8)
                orig.man_world(mw)
                probe.probe_ao_man_world((U * 24)(*mw))
            orig.node_world(w)
            probe.probe_ao_node_world((U * 29)(*w))
            stats['world'] = stats.get('world', 0) + 1
            continue
        a = orig.man(op, which, dt)
        probe.probe_ao_man(op, which, dt, out)
        compare(f'call {k} op {op} which {which}', a, list(out))
        stats[op] = stats.get(op, 0) + 1
    probe.probe_ao_done()


def main():
    probe = setup_probe()
    cases = int(os.environ.get('MAN_CASES', '300'))
    stats = {}
    for seed in range(cases):
        run_case(probe, seed, 80, stats)
    never = [COVER[a] for a in COVER if a not in REACHED]
    if never:
        raise SystemExit(f'branches never reached: {never}')
    print(f'ok: {cases} cases, {stats.get(0, 0)} made, {stats.get(4, 0)} pushed by the brain, {stats.get(1, 0)} runs, '
          f'{stats.get(2, 0)} resumes, {stats.get(3, 0)} suspends, {stats.get("push", 0)} orders over it, '
          f'{stats.get("pop", 0)} pops, {stats.get("world", 0)} world changes identical; {len(COVER)} branches reached')


if __name__ == '__main__':
    code = 0
    try:
        main()
    except SystemExit as e:
        print(e)
        code = 1
    sys.stdout.flush()
    os._exit(code)
