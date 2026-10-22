"""The brain's attack (docs/ai.md 17.2) against the original x86: 0x4e9ca0
with its net 0x4c0810 (its part 0x4c0a70 -> 0x4c0a90, dtor 0x4c0b50), its
three behaviours (0x5cd920, 0x4caca0, 0x4cad70) and states (0x4cac20,
0x4cace0, 0x4cadb0: their resumes 0x4cac80, 0x4cad30, 0x4cae10 and the way
home 0x4cae90 with 0x4ce250, 0x4c8d10, 0x4c8df0).

    cmake --build build && .venv/bin/python tests/differential/test_ai_attack.py

The order stack and its fake nets and behaviours are test_ai_orders.py's.
Added: the brain's virtual base (its vehicle at +0xf0, the controls at
+0x2b0: vt+0x14 stop, vt+0x1c the throttle, vt+0x20 / vt+0x28 the throttle
and turn set), the fields the states read and write (+0x28, +0x2c, +0x3c,
+0x58..+0x60, +0x84, +0xdc, +0xe4, +0xe8, +0xec, +0x230), the vehicle's
position (0x5a85e0), forward (+0x24) and stop share (vt+0x12c), the
boundary (0x4c3000), the nearest step (0x4d51d0) and its position (vt+0x18),
znet's registry (0x4c7b80). Each case builds a stack, attacks on one of its
nets, then runs, pops and changes the world at random; after each step the
calls, the stack (the attack's parts and states with their home points,
nets, waypoints and turns) and the brain's fields must match, at 24-bit x87.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

os.environ.setdefault('MW3_X87', '24')
sys.path.insert(0, str(Path(__file__).parent))
import test_ai_orders as to  # noqa: E402
from calltrace import Trace  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402

U, I = ctypes.c_uint32, ctypes.c_int32
ATTACK, OWNER = 0x4e9ca0, 0x2c7f0000  # the probe's kOwner, the vehicle's handle in the traces
VT_PART, VT_END, VT_REARM, VT_RETURN = 0x5cac30, 0x5cb928, 0x5cba00, 0x5cbad8
f2i = to.f2i
COVER = {0x4cb05f: 'home: arrived', 0x4cb06c: 'home: driving', 0x4cb0b0: 'home: throttle let fall',
         0x4cb0c4: 'home: capped', 0x4caf0f: 'home: pulled in', 0x4cb013: 'home: turning',
         0x4caff9: 'home: behind, full', 0x4cae57: 'resume: no position', 0x4cad45: 'rearm: off for good', 0x4cad57: 'rearm: a delay', 0x4cb049: 'home: dead ahead',
         0x4c0b6c: 'part: the last home', 0x4c0b8e: 'part: stacked', 0x4caeed: 'home: the speed share'}
REACHED = set()


def script(seed):
    s = to.script(seed)
    r = lambda salt, choices: (lambda n, args: random.Random(f'{seed}:{salt}:{n}').choice(choices))  # noqa: E731
    s['ictl.vt+0x1c'] = r('thr', [f2i(x) for x in (0.0, 0.3, 1.0, 0.8, 0.05)])
    s['vehicle.vt+0x12c'] = r('share', [f2i(x) for x in (0.0, 0.5, 1.0, 0.25, 0.9)])
    s['0x004c3000'] = r('pull', [0, 0, 0, 0, 1])
    s['0x004d51d0'] = r('near', [1, 2, 3, 4, 1, 2])  # none: the exe reads through null (the port's FIX)
    return s


class Original(to.Original):
    def __init__(self, seed):
        super().__init__(seed)
        e = self.e
        self.t.script = script(seed)
        e.call(0x4c7b80, ecx=0x603fd8)  # znet's registry
        e.w32(0x604058, 0x5bc988)  # the stand behaviour (its start-up 0x410610): the attack's net's step
        b = self.brain = e.alloc(0x400)
        e.w32(b + 0x190, b)
        vb = e.world_alloc(0x10)
        e.w32(b + 0xe0, vb)
        e.w32(vb + 4, 0x1d0)  # the virtual base at +0x2b0: the vehicle at +0xf0 (its +0x10)
        cvt = e.world_alloc(0x40)
        e.w32(b + 0x2b0, cvt)
        e.w32(cvt + 0x14, e.new_stub('ictl.vt+0x14', '', 0))
        e.w32(cvt + 0x1c, self.fld_thunk('ictl.vt+0x1c', 0))
        e.w32(cvt + 0x20, e.new_stub('ictl.vt+0x20', 's0', 4))
        e.w32(cvt + 0x28, e.new_stub('ictl.vt+0x28', 's0', 4))
        self.v = e.world_alloc(0x100)
        vvt = e.world_alloc(0x200)
        e.w32(self.v, vvt)
        e.w32(vvt + 0x12c, self.fld_thunk('vehicle.vt+0x12c', 4, 's0'))
        e.w32(b + 0xf0 + 0x1d0, self.v)
        self.pos = e.world_alloc(0x10)
        e.stub(0x5a85e0, '0x005a85e0', 'c', None, self.position)
        e.stub(0x4c3000, '0x004c3000', 'c,s0', None, self.pull)
        e.stub(0x4d51d0, '0x004d51d0', 'c,s0', None, self.nearest)
        for k in (1, 2, 3, 4):
            beh = self.beh_addr[k]
            e.w32(e.r32(beh) + 0x18, e.new_stub('@beh.vt+0x18', 'c', 0, self.step_position))
        self.mark = e.world_alloc(0x10)
        self.step_on = {}
        for a in COVER:
            e.uc.hook_add(UC_HOOK_CODE, lambda uc, ad, sz, u: REACHED.add(ad), begin=a, end=a)

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

    def position(self, e, key, a, pop):
        self.t.handle(key, [OWNER if a[0] == self.v else a[0]])
        e._ret(self.pos, pop)

    def pull(self, e, key, a, pop):
        r = self.t.handle(key, [OWNER if a[0] == self.v else a[0]])
        e.cpu.write(a[1], bytes(12))  # the port's hook leaves its zeros (halved)
        e._ret(r, pop)

    def nearest(self, e, key, a, pop):
        k = self.t.handle(key, [e.r32(a[0] + 4) if a[0] else 0])
        e._ret(self.beh_addr.get(k, 0) if k else 0, pop)

    def step_position(self, e, key, a, pop):
        beh = a[0]
        k = next(i for i, x in self.beh_addr.items() if x == beh)
        e._ret(beh + 0x10 if self.step_on.get(k) else 0, pop)

    def world(self, w):
        e, b = self.e, self.brain
        for off, x in zip((0x28, 0x2c, 0x3c, 0x58, 0x5c, 0x60, 0x84, 0xec, 0xdc, 0xe8, 0x230), w[:11]):
            e.w32(b + off, x)
        e.cpu.write(self.pos, struct.pack('<3I', *w[11:14]))
        e.cpu.write(self.v + 0x24, struct.pack('<3I', w[14], 0, w[15]))
        for k in (1, 2, 3, 4):
            q = w[16 + 4 * (k - 1):20 + 4 * (k - 1)]
            self.step_on[k] = q[0]
            e.cpu.write(self.beh_addr[k] + 0x10, struct.pack('<3I', *q[1:4]))

    def home_mark(self, on):
        self.e.w32(self.brain + 0xe4, self.mark if on else 0)

    def home_code(self, h, orders):
        if not h:
            return 0
        if h == self.mark:
            return 1
        for i, o in enumerate(orders):
            if self.e.r32(o + 0x30) == VT_PART and h == o + 0x38:
                return 100 + i
        return 2

    def attack_stack(self):
        e = self.e
        rows = []
        orders = self.orders()
        for o in orders:
            part, state = e.r32(o + 0xc), e.r32(o + 0x10)
            real_part = part and e.r32(part) == VT_PART
            real_state = state and e.r32(state) in (VT_END, VT_REARM, VT_RETURN)
            row = [e.cpu.read(o + 4, 1)[0], int(e.r32(o + 8) == self.brain),
                   e.r32(part + 4) if part and not real_part else 0,
                   e.r32(e.r32(part + 8) + 4) if part and not real_part else 0,
                   e.r32(state + 0xc) if state and not real_state else 0,
                   e.r32(e.r32(state + 8) + 4) if state and not real_state else 0]
            row += [e.r32(o + 0x14 + 4 * i) for i in range(7)]
            row += [0] * 9
            if real_part:
                row[13] = e.r32(e.r32(part + 0x18) + 4)
                row[14:17] = [e.r32(part + 8 + 4 * k) for k in range(3)]
                row[17] = self.home_code(e.r32(part + 0x14), orders)
            if real_state:
                vt = e.r32(state)
                row[18] = {VT_END: 1, VT_REARM: 2, VT_RETURN: 3}[vt]
                if vt == VT_RETURN:
                    n = e.r32(state + 8)
                    row[19] = e.r32(n + 4) if n else 0
                    row[20:22] = [e.r32(state + 0xc), e.r32(state + 0x10)]
            rows.append(row)
        return rows

    def brain_row(self):
        e, b = self.e, self.brain
        orders = self.orders()
        out = [self.home_code(e.r32(b + 0xe4), orders), e.r32(b + 0xe8), e.r32(b + 0x230), e.r32(b + 0xdc),
               e.r32(b + 0x3c), e.r32(0x60b328)] + [0] * 8
        k = 0
        for o in orders:
            st = e.r32(o + 0x10)
            if st and e.r32(st) == VT_RETURN and k < 4:
                out[6 + 2 * k], out[7 + 2 * k] = e.r32(st + 0x14), e.r32(st + 0x18)
                k += 1
        return out


def setup_probe():
    probe = to.setup_probe()
    probe.probe_ao_attack_world.argtypes = [ctypes.c_void_p]
    probe.probe_ao_attack_home.argtypes = [I]
    probe.probe_ao_attack.argtypes = [I]
    probe.probe_ao_attack.restype = I
    probe.probe_ao_attack_stack.argtypes = [ctypes.c_void_p, I]
    probe.probe_ao_attack_stack.restype = I
    probe.probe_ao_attack_brain.argtypes = [ctypes.c_void_p]
    probe.probe_ao_turn_params.argtypes = [ctypes.c_void_p]
    return probe


def world(rng):
    rf = lambda lo, hi: f2i(rng.uniform(lo, hi))  # noqa: E731
    near, far = rng.uniform(5, 60), rng.uniform(20, 200)
    w = [f2i(near), f2i(near + far), rng.choice([0, 1]), rf(0, 30), rng.choice([0, 0, 1]), rng.choice([0, 0, 1]),
         f2i(rng.choice([0.0, 0.0, 0.7, 1.0, 0.4])), f2i(rng.choice([0.5, 1.0, 0.8])),
         f2i(rng.choice([3.4028235e38, 50.0])), f2i(rng.choice([3.4028235e38, 120.0])), rf(0, 1)]
    w += [rf(-300, 300), rf(0, 20), rf(-300, 300)]
    a = rng.uniform(-3.2, 3.2)
    import math
    w += [f2i(math.sin(a)), f2i(math.cos(a))]
    for _ in range(4):
        w += [int(rng.random() < 0.8), rf(-300, 300), rf(0, 20), rf(-300, 300)]
    if rng.random() < 0.3:  # a step right ahead or behind of the vehicle, or near it
        k = rng.randrange(4)
        px, pz = struct.unpack('<f', struct.pack('<I', w[11]))[0], struct.unpack('<f', struct.pack('<I', w[13]))[0]
        fx, fz = math.sin(a), math.cos(a)
        d = rng.choice([30.0, -30.0, 2.0, 0.0])
        w[16 + 4 * k:20 + 4 * k] = [1, f2i(px + fx * d), w[12], f2i(pz + fz * d)]
    return w


def run_case(probe, seed, rounds):
    rng = random.Random(seed)
    orig = Original(seed)
    # [0x60b478], the start-up 0x4c7900's turn settings: the port's (test_ai_node.py checks them).
    tp = (U * 4)()
    probe.probe_ao_turn_params(tp)
    orig.e.cpu.write(0x60b478, struct.pack('<4I', *tp))
    port = Trace(script(seed))
    probe.probe_app_set_callback(port.callback())
    probe.probe_ao_new()
    stats = {}

    def check(what):
        a, b = orig.attack_stack(), []
        buf = (U * (22 * 400))()
        n = probe.probe_ao_attack_stack(buf, 22 * 400)
        b = [list(buf[22 * i:22 * i + 22]) for i in range(n)]
        br = (U * 14)()
        probe.probe_ao_attack_brain(br)
        if orig.t.log != port.log:
            i = next((j for j, (x, y) in enumerate(zip(orig.t.log, port.log)) if x != y),
                     min(len(orig.t.log), len(port.log)))
            raise SystemExit(f'seed {seed} {what}: calls differ at {i}:\n  before {orig.t.log[max(0, i - 4):i]}'
                             f'\n  exe  {orig.t.log[i:i + 4]}\n  port {port.log[i:i + 4]}')
        if a != b:
            raise SystemExit(f'seed {seed} {what}: stacks differ\n  exe  {a}\n  port {b}')
        if orig.brain_row() != list(br):
            raise SystemExit(f'seed {seed} {what}: brain {orig.brain_row()} vs {list(br)}')
        return len(a)

    for k in range(rounds):
        w = world(rng)
        orig.world(w)
        probe.probe_ao_attack_world((U * len(w))(*w))
        mark = int(rng.random() < 0.5)
        orig.home_mark(mark)
        probe.probe_ao_attack_home(mark)
        # A stack: the first order (made once), sometimes another, then the
        # attack on one of the nets.
        net = rng.choice([1, 2, 3, 4])
        if not orig.orders():
            b = rng.choice(to.BEHS)
            orig.op(0, net, b, 0)
            probe.probe_ao_op(0, net, b, 0)
        if rng.random() < 0.4:
            n2, b = rng.choice([1, 2, 3, 4]), rng.choice(to.BEHS)
            orig.op(1, n2, b, 0)
            probe.probe_ao_op(1, n2, b, 0)
        check('base')
        ra = orig.e.call(ATTACK, ecx=orig.brain, stack=(orig.net_addr[net],)) & 0xff
        rb = probe.probe_ao_attack(net) & 0xff
        # 0x4cadb0 leaves the waypoint as the deque slot held it (in play the
        # fight is pushed on top at once and the resume sets it first): the
        # port's zeros.
        top = orig.orders()[-1]
        orig.e.cpu.write(top + 0x54 + 0xc, bytes(12))
        if ra != rb:
            raise SystemExit(f'seed {seed}: attack answers {ra} vs {rb}')
        check('attack')
        stats['attacks'] = stats.get('attacks', 0) + 1
        if rng.random() < 0.8:  # the fight, as the brain's frame pushes it next: its pop resumes the way home
            n2, b = rng.choice([1, 2, 3, 4]), rng.choice([1, 2, 3, 4])
            orig.op(1, n2, b, 0)
            probe.probe_ao_op(1, n2, b, 0)
            check('fight')
        for step in range(rng.randrange(1, 40)):
            size = len(orig.orders())
            r = rng.random()
            if r < 0.15 and size > 1:
                orig.op(7, 0, 0, 0)
                probe.probe_ao_op(7, 0, 0, 0)
                what = 'pop'
            elif r < 0.25:
                w = world(rng)
                orig.world(w)
                probe.probe_ao_attack_world((U * len(w))(*w))
                what = 'world'
            else:
                f = f2i(rng.choice([1 / 30, 0.25]))
                orig.op(16, 0, 0, f)
                probe.probe_ao_op(16, 0, 0, f)
                what = 'run'
            stats[what] = stats.get(what, 0) + 1
            if check(f'round {k} step {step} {what}') <= 1:
                break
        # Back to one order for the next round.
        while len(orig.orders()) > 1:
            orig.op(7, 0, 0, 0)
            probe.probe_ao_op(7, 0, 0, 0)
            check(f'round {k} unwind')
    probe.probe_ao_done()
    return stats


def main():
    probe = setup_probe()
    cases = int(os.environ.get('ATTACK_CASES', '200'))
    total = {}
    for seed in range(cases):
        for k, v in run_case(probe, seed, 6).items():
            total[k] = total.get(k, 0) + v
    never = [COVER[a] for a in COVER if a not in REACHED]
    if never:
        raise SystemExit(f'branches never reached: {never}')
    print(f'ok: {cases} cases, {total.get("attacks", 0)} attacks, {total.get("run", 0)} runs, '
          f'{total.get("pop", 0)} pops identical; {len(COVER)} branches reached')


if __name__ == '__main__':
    code = 0
    try:
        main()
    except SystemExit as e:
        print(e)
        code = 1
    sys.stdout.flush()
    os._exit(code)
