"""The player's lance orders on a pilot's brain (docs/ai.md 17.6) against the
original x86: the wrappers 0x4ea000..0x4ea230 and their answer 0x4e9fd0 over
the brain's slots (attack my target 0x4ea450 / 0x4ea4a0, form on me 0x4ea4b0
on the formation net 0x603fb8 built by the exe's 0x4c0570, enemy only
0x4ea390, ally only 0x4ea3f0, nothing 0x58b380, stop 0x4ea4e0; the refit
0x4ea260 stubbed), then the orders they push run a few frames.

    cmake --build build && .venv/bin/python tests/differential/test_ai_lance.py

The world is test_ai_attack.py's (the brain, its vehicle and controls, the
target vehicle with its own net 0x4d70f0, three contacts on it or on
nothing). Added: the brain's vtable with the exe's slots +0x20..+0x48
(0x5cc698's), the target's vt+0x28 (its teams at +0xc0 / +0xc4), the radio
(+0x2a8) with its slots numbered, 0x4e6d50 stubbed. Each case: random
orders 0..12 (but 2, the command mode, item L4c) on a contact or none, the
teams sharing bits or not, the radio on or off; the answers, the calls, the
order stacks and the steering must match, at 24-bit x87.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

os.environ.setdefault('MW3_X87', '24')
sys.path.insert(0, str(Path(__file__).parent))
import test_ai_attack as ta  # noqa: E402
import test_ai_orders as to  # noqa: E402
from calltrace import Trace  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402

U, I = ctypes.c_uint32, ctypes.c_int32
WRAP = {0: 0x4ea000, 1: 0x4ea070, 3: 0x4ea0b0, 4: 0x4ea0f0, 5: 0x4ea120, 6: 0x4ea150, 7: 0x4ea180, 8: 0x4ea040,
        9: 0x4ea1c0, 10: 0x4ea200, 11: 0x4ea230, 12: 0x4ea180}
VT_FORM_PART = 0x5cabc0
COVER = {0x4ea47c: 'attack: pushed', 0x4ea3d2: 'enemy only: taken', 0x4ea433: 'ally only: taken',
         0x4e9ff0: 'disregard_order', 0x4e9fe7: 'a line', 0x4ea4bf: 'form on me'}
REACHED = set()


def script(seed):
    s = ta.script(seed)
    s['0x004ea260'] = lambda n, args: random.Random(f'{seed}:refit:{n}').choice([0, 1])
    return s


class Original(ta.Original):
    def __init__(self, seed):
        super().__init__(seed)
        e, b = self.e, self.brain
        self.t.script = script(seed)
        bvt = e.world_alloc(0x100)
        for off in range(0x20, 0x4c, 4):
            e.w32(bvt + off, e.r32(0x5cc698 + off))
        e.w32(b, bvt)
        e.w32(e.r32(self.target) + 0x28, e.new_stub('@target.vt+0x28', 'c', 0, lambda emu, k, a, pop: emu._ret(a[0], pop)))
        e.w32(e.r32(self.v) + 0x28, e.new_stub('@vehicle.vt+0x28', 'c', 0, lambda emu, k, a, pop: emu._ret(a[0], pop)))
        e.call(0x4c0570, ecx=0x603fb8)  # the formation net, as the start-up's 0x410450 makes it
        self.radio = e.world_alloc(0x40)
        for k in range(16):
            e.w32(self.radio + 4 * k, k)
        e.stub(0x4e6d50, '0x004e6d50', 'c', 4)
        e.stub(0x4ea260, '0x004ea260', '', 4)  # its argument (the contact) unread
        for addr in COVER:
            e.uc.hook_add(UC_HOOK_CODE, lambda uc, ad, sz, u: REACHED.add(ad), begin=addr, end=addr)

    def lance(self, order, k, radio, teams):
        e, b = self.e, self.brain
        e.w32(self.v + 0xc0, teams[0])
        e.w32(self.v + 0xc4, teams[1])
        e.w32(self.target + 0xc0, teams[2])
        e.w32(self.target + 0xc4, teams[3])
        e.w32(b + 0x2a8, self.radio if radio else 0)
        r = e.call(WRAP[order], ecx=b, stack=(self.contacts[k - 1] if k else 0,))
        e.w32(b + 0x2a8, 0)
        return r & 0xffffffff

    def attack_stack(self):
        rows = super().attack_stack()
        for o, row in zip(self.orders(), rows):
            part = self.e.r32(o + 0xc)
            if part and self.e.r32(part) == VT_FORM_PART:
                row[2:4] = [0, 0]
                row[13] = 0x20000
        return rows


def setup_probe():
    probe = ta.setup_probe()
    probe.probe_ao_lance.argtypes = [I, U, I, ctypes.c_void_p]
    probe.probe_ao_lance.restype = I
    return probe


def teams(rng):
    t = [1 << rng.randrange(3), 0, 1 << rng.randrange(3), 0]
    t[1] = rng.choice([t[0], t[2], t[0] | t[2], 0])
    t[3] = rng.choice([t[2], t[0], 0])
    return t


def run_case(probe, seed, rounds):
    rng = random.Random(seed)
    orig = Original(seed)
    tp = (U * 4)()
    probe.probe_ao_turn_params(tp)
    orig.e.cpu.write(0x60b478, struct.pack('<4I', *tp))
    port = Trace(script(seed))
    probe.probe_app_set_callback(port.callback())
    probe.probe_ao_new()
    probe.probe_ao_set_now(0)
    orig.e.w32(0x60991c, 0)
    stats = {}

    def check(what):
        a = orig.attack_stack()
        buf = (U * (ta.ROW * 400))()
        n = probe.probe_ao_attack_stack(buf, ta.ROW * 400)
        b = [list(buf[ta.ROW * i:ta.ROW * i + ta.ROW]) for i in range(n)]
        if orig.t.log != port.log:
            i = next((j for j, (x, y) in enumerate(zip(orig.t.log, port.log)) if x != y),
                     min(len(orig.t.log), len(port.log)))
            raise SystemExit(f'seed {seed} {what}: calls differ at {i}:\n  before {orig.t.log[max(0, i - 4):i]}'
                             f'\n  exe  {orig.t.log[i:i + 4]}\n  port {port.log[i:i + 4]}')
        if a != b:
            raise SystemExit(f'seed {seed} {what}: stacks differ\n  exe  {a}\n  port {b}')
        sb = (ctypes.c_uint8 * 0x48)()
        probe.probe_ao_steer_block(sb)
        if bytes(sb) != bytes(orig.e.cpu.read(orig.brain + 0x250, 0x48)):
            raise SystemExit(f'seed {seed} {what}: the steering differs')
        return len(a)

    for k in range(rounds):
        w = ta.world(rng)
        orig.world(w)
        probe.probe_ao_attack_world((U * len(w))(*w))
        if not orig.orders():
            net, beh = rng.choice([1, 2, 3, 4]), rng.choice(to.BEHS)
            orig.op(0, net, beh, 0)
            probe.probe_ao_op(0, net, beh, 0)
        # The target vehicle near the pilot, active or not; the contacts on it.
        tpos = [w[11], w[12], w[13]]
        px, py, pz = (struct.unpack('<f', struct.pack('<I', x))[0] for x in tpos)
        tp3 = [ta.f32(px + rng.uniform(-200, 200)), ta.f32(py), ta.f32(pz + rng.uniform(-200, 200))]
        fw = (tp3, int(rng.random() < 0.85), 0, ta.f2i(1e6), ta.f2i(rng.uniform(0, 100)), 1)
        orig.fight_world(*fw)
        probe.probe_ao_set_now(fw[4])
        probe.probe_ao_fight_world((ctypes.c_float * 3)(*tp3), *fw[1:])
        ow = []
        for _ in range(3):
            ow += [ta.f2i(rng.uniform(100, 4e4)), int(rng.random() < 0.7)]
        ow += [ta.f2i(1e4), ta.f2i(30.0), 0, 0]
        orig.own_world(ow)
        probe.probe_ao_own_world((U * 10)(*ow))
        check('world')
        for step in range(rng.randrange(1, 6)):
            order = rng.choice([0, 0, 1, 1, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12])
            # A contact always (the dispatcher's, on the player's target); one that
            # sees nothing is read through null by the only-orders (the port's FIX).
            kc, radio, t = rng.choice([1, 2, 3]), int(rng.random() < 0.7), teams(rng)
            if order in (3, 4, 10, 11):
                seeing = [i + 1 for i in range(3) if orig.contact_on[i]]
                if not seeing:
                    continue
                kc = rng.choice(seeing)
            ra = orig.lance(order, kc, radio, t)
            rb = probe.probe_ao_lance(order, kc, radio, (U * 4)(*t)) & 0xffffffff
            if ra != rb:
                raise SystemExit(f'seed {seed} round {k}: order {order} answers exe {ra}, port {rb}')
            stats[f'order {order}'] = stats.get(f'order {order}', 0) + 1
            stats['taken'] = stats.get('taken', 0) + (ra == 1)
            check(f'round {k} order {order}')
            for _ in range(rng.randrange(0, 4)):  # what it pushed, run
                f = ta.f2i(rng.choice([1 / 30, 0.25]))
                orig.op(16, 0, 0, f)
                probe.probe_ao_op(16, 0, 0, f)
                check(f'round {k} order {order} run')
                stats['runs'] = stats.get('runs', 0) + 1
        while len(orig.orders()) > 1:
            orig.op(7, 0, 0, 0)
            probe.probe_ao_op(7, 0, 0, 0)
            check(f'round {k} unwind')
    probe.probe_ao_done()
    return stats


def main():
    probe = setup_probe()
    cases = int(os.environ.get('LANCE_CASES', '200'))
    total = {}
    for seed in range(cases):
        for k, v in run_case(probe, seed, 6).items():
            total[k] = total.get(k, 0) + v
    never = [COVER[a] for a in COVER if a not in REACHED]
    if never:
        raise SystemExit(f'branches never reached: {never}')
    orders = sum(v for k, v in total.items() if k.startswith('order'))
    print(f'ok: {cases} cases, {orders} orders ({total.get("taken", 0)} taken), {total.get("runs", 0)} runs after, '
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
