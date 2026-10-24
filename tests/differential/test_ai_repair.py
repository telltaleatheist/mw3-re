"""The AI at the field base's dock (docs/vehicles.md, The AI at the dock;
docs/ai.md 17.6) against the original x86: the refit order 0x4ea260 (the
lance's 7 / 12 through the brain's vt+0x3c) with 0x47a780, the repair
behaviour 0x4d08d0, its check 0x4d0920 and state maker 0x4d0990, the state
0x4d0a10 (its approach point), its run 0x4d0ce0 with 0x4d0c60, and its end
0x4d0ba0.

    cmake --build build && .venv/bin/python tests/differential/test_ai_repair.py

The world is test_ai_lance.py's (test_ai_attack's brain, vehicle, controls
and steering, the exe's brain slots). Added: the bases up (0x45a000), the
dock's ask (0x47a780), the mcbwaynet at [0x60b544] with its selected point
(0x58b060, the point's vt+0x18), the dock's flags [0x60b4d0] / [0x60b4cc],
the vehicle's mech (vt+0x50, itself) and power (0x41cc30), powered down
(vt+0xa0). Each case: a world, an order, the refit, then frames with the
pilot moved, the steering sometimes left at its end (arrived), the flags,
the clock and the power changing; the answers, calls, order stacks (the
repair state's phase, fights, stopping distance, end time and approach
point), steering and the brain's +0x80, +0x2a4, +0x298 must match, at
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
import test_ai_attack as ta  # noqa: E402
import test_ai_lance as tl  # noqa: E402
import test_ai_orders as to  # noqa: E402
from calltrace import Trace  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn.x86_const import UC_X86_REG_ECX  # noqa: E402

U, I, F = ctypes.c_uint32, ctypes.c_int32, ctypes.c_float
VT_REPAIR = 0x5cca28
COVER = {0x4ea2c4: 'refit: asked', 0x4ea347: 'refit: none', 0x4d0b64: 'state: no dock point',
         0x4d0d23: 'out: pulled', 0x4d0e15: 'out: the approach set', 0x4d0ea8: 'to the approach: walking',
         0x4d0e6d: 'at the approach', 0x4d0f87: 'to the dock: walking', 0x4d0f5f: 'at the dock',
         0x4d11f2: 'closing in: walking', 0x4d1046: 'closed in', 0x4d1096: 'waiting for the bases',
         0x4d10a6: 'powered down', 0x4d1112: 'down: checked', 0x4d113e: 'down: timed out or pushed off',
         0x4d11a3: 'down: staying', 0x4d11b3: 'up again, done', 0x4d11e1: 'popped',
         0x4d0c1f: 'end: fights back', 0x4d0bec: 'end: powered up'}
REACHED = set()


def script(seed):
    s = tl.script(seed)
    r = lambda salt, choices: (lambda n, args: random.Random(f'{seed}:{salt}:{n}').choice(choices))  # noqa: E731
    s['0x0045a000'] = r('bases', [0, 1, 2, 3, 3])
    s['0x0047a780'] = r('ask', [0, 1, 1, 1])
    s['0x0058b060'] = r('point', [0, 1, 1, 1, 1])
    s['point.vt+0x18'] = r('place', [0, 1, 1, 1, 1, 1])
    s['owner.vt+0xa0'] = r('down', [0, 1, 1, 1])
    return s


class Original(ta.Original):
    def __init__(self, seed):
        super().__init__(seed)
        e, b = self.e, self.brain
        self.t.script = script(seed)
        # test_ai_lance's: the exe's brain slots, the target's and vehicle's
        # vt+0x28, the formation net, the radio; the refit left to run.
        bvt = e.world_alloc(0x100)
        for off in range(0x20, 0x4c, 4):
            e.w32(bvt + off, e.r32(0x5cc698 + off))
        e.w32(b, bvt)
        e.w32(e.r32(self.target) + 0x28, e.new_stub('@target.vt+0x28', 'c', 0, lambda emu, k, a, pop: emu._ret(a[0], pop)))
        e.w32(e.r32(self.v) + 0x28, e.new_stub('@vehicle.vt+0x28', 'c', 0, lambda emu, k, a, pop: emu._ret(a[0], pop)))
        e.call(0x4c0570, ecx=0x603fb8)
        self.radio = e.world_alloc(0x40)
        for k in range(16):
            e.w32(self.radio + 4 * k, k)
        e.stub(0x4e6d50, '0x004e6d50', 'c', 4)
        # floor (an import; the approach point's snap): x87 rounding down.
        e.cpu.import_code(0x5bb66c, bytes.fromhex(
            '83ec04 d93c24 668b0424 6625fff3 660d0004 6689442402 d96c2402 dd442408 d9fc d92c24 83c404 c3'))
        # The field bases' side.
        e.stub(0x45a000, '0x0045a000', 'c', None)
        own = lambda a: [ta.OWNER if x == self.v else x for x in a]  # noqa: E731

        def ask(em, key, a, pop):
            em._ret(em.t.handle(key, own(a)), pop)
        e.stub(0x47a780, '0x0047a780', 'd', None, ask)
        self.mcb = e.world_alloc(0x48)
        e.w32(self.mcb + 0x20, 2)
        self.point = e.world_alloc(0x40)
        self.place = e.world_alloc(0x10)
        pvt = e.world_alloc(0x40)
        e.w32(self.point, pvt)

        def place(em, key, a, pop):
            em._ret(self.place if em.t.handle(key, []) & 0xff else 0, pop)
        e.w32(pvt + 0x18, e.new_stub('point.vt+0x18', '', 0, place))

        def point(em, key, a, pop):
            em._ret(self.point if em.t.handle(key, []) & 0xff else 0, pop)
        e.stub(0x58b060, '0x0058b060', 's0', None, point)
        vvt = e.r32(self.v)
        self.not_mech = 0
        e.w32(vvt + 0x50, e.new_stub('@vehicle.vt+0x50', '', 0,
                                     lambda emu, k, a, pop: emu._ret(0 if self.not_mech else self.v, pop)))
        e.w32(vvt + 0xa0, e.new_stub('owner.vt+0xa0', '', 0, lambda emu, k, a, pop: emu._ret(emu.t.handle(k, [ta.OWNER]), pop)))

        def power(em, key, a, pop):
            em._ret(em.t.handle(key, own(a)), pop)
        e.stub(0x41cc30, '0x0041cc30', 'c,s0', None, power)
        for addr in COVER:
            e.uc.hook_add(UC_HOOK_CODE, lambda uc, ad, sz, u: REACHED.add(ad), begin=addr, end=addr)

        # The state is built in place in the order (+0x54): its +0x14..+0x1c
        # and +0x28..+0x43 hold what the slot held until written (never read
        # before; the approach point stays so without a dock point). Zeroed
        # as the constructor starts, as the port's.
        def zero(uc, address, size, user):
            st = uc.reg_read(UC_X86_REG_ECX)
            e.cpu.write(st + 0x14, b'\0' * 0x30)
        e.uc.hook_add(UC_HOOK_CODE, zero, begin=0x4d0a10, end=0x4d0a10)

    def repair_world(self, mcb, drive, contact, point, own_fights, x2a4, not_mech=0):
        e, b = self.e, self.brain
        self.not_mech = not_mech
        e.w32(0x60b544, self.mcb if mcb else 0)
        e.cpu.write(0x60b4d0, bytes([drive]))
        e.cpu.write(0x60b4cc, bytes([contact]))
        e.cpu.write(self.place, struct.pack('<3f', *point))
        e.w32(b + 0x80, own_fights)
        e.cpu.write(b + 0x2a4, bytes([x2a4]))

    def lance(self, order, k):
        r = self.e.call(tl.WRAP[order], ecx=self.brain, stack=(self.contacts[k - 1] if k else 0,))
        return r & 0xffffffff

    def brain_bits(self):
        e, b = self.e, self.brain
        return [e.r32(b + 0x80), e.r8(b + 0x2a4)] + [e.r32(b + 0x298 + 4 * k) for k in range(3)]

    def attack_stack(self):
        rows = super().attack_stack()
        e = self.e
        for o, row in zip(self.orders(), rows):
            part = e.r32(o + 0xc)
            if part and e.r32(part) == tl.VT_FORM_PART:
                row[2:4] = [0, 0]
                row[13] = 0x20000
            st = e.r32(o + 0x10)
            if st and e.r32(st) == VT_REPAIR:
                row[4:6] = [0, 0]
                row[18] = 9
                row[19] = e.r32(st + 0x24)
                row[20] = e.r8(st + 0x14)
                row[21:24] = [e.r32(st + 0x18), e.r32(st + 0x1c), e.r32(st + 0x20)]
                row[24:27] = [e.r32(st + 0x38 + 4 * k) for k in range(3)]
                row[27] = int(e.r32(st + 0x34) != 0)
        return rows


def setup_probe():
    probe = tl.setup_probe()
    probe.probe_ao_repair_world.argtypes = [I, I, I, ctypes.c_void_p, I, I, I]
    probe.probe_ao_repair_brain.argtypes = [ctypes.c_void_p]
    probe.probe_ao_steer_load.argtypes = [ctypes.c_void_p]
    return probe


def f2(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


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
            if os.environ.get('REPAIR_DEBUG'):
                sb = (ctypes.c_uint8 * 0x48)()
                probe.probe_ao_steer_block(sb)
                print('exe steer', struct.unpack('<18f', bytes(orig.e.cpu.read(orig.brain + 0x250, 0x48))))
                print('port steer', struct.unpack('<18f', bytes(sb)))
                print('exe row', orig.attack_stack()[-1], '\nport row', b[-1] if b else None)
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
        bb = (U * 5)()
        probe.probe_ao_repair_brain(bb)
        if list(bb) != orig.brain_bits():
            raise SystemExit(f'seed {seed} {what}: the brain differs\n  exe  {orig.brain_bits()}\n  port {list(bb)}')
        return a

    def set_world(w, now, pos):
        w = list(w)
        w[11:14] = [f2(x) for x in pos]
        orig.world(w)
        probe.probe_ao_attack_world((U * len(w))(*w))
        orig.e.w32(0x60991c, f2(now))
        probe.probe_ao_set_now(f2(now))

    for k in range(rounds):
        w = ta.world(rng)
        pt = [ta.f32(rng.uniform(-300, 300)), ta.f32(rng.uniform(-5, 20)), ta.f32(rng.uniform(-300, 300))]
        pos = [ta.f32(pt[0] + rng.uniform(-120, 120)), ta.f32(pt[1]), ta.f32(pt[2] + rng.uniform(-120, 120))]
        now = ta.f32(rng.uniform(0, 500))
        set_world(w, now, pos)
        if not orig.orders():
            net, beh = rng.choice([1, 2, 3, 4]), rng.choice(to.BEHS)
            orig.op(0, net, beh, 0)
            probe.probe_ao_op(0, net, beh, 0)
        rw = [int(rng.random() < 0.9), int(rng.random() < 0.7), int(rng.random() < 0.4), pt,
              rng.choice([0, 1, 1]), 0, int(rng.random() < 0.1)]
        orig.repair_world(*rw)
        probe.probe_ao_repair_world(rw[0], rw[1], rw[2], (F * 3)(*pt), rw[4], rw[5], rw[6])
        check(f'round {k} world')
        order = rng.choice([7, 12])
        ra = orig.lance(order, rng.choice([0, 1, 2, 3]))
        rb = probe.probe_ao_lance(order, 0, 0, (U * 4)()) & 0xffffffff
        if ra != rb:
            raise SystemExit(f'seed {seed} round {k}: the refit answers exe {ra}, port {rb}')
        stats['refits'] = stats.get('refits', 0) + 1
        stats['taken'] = stats.get('taken', 0) + (ra == 1)
        check(f'round {k} refit')
        for f in range(rng.randrange(1, 40)):
            rows = orig.attack_stack()
            if not rows or rows[-1][18] != 9:
                break
            ph = rows[-1][19]
            # Between frames: the pilot moved (toward where it walks, or
            # pushed off), the steering left at its end now and then, the
            # clock on, the flags and the power changed.
            r = rng.random()
            if r < 0.3:
                pos = [ta.f32(pt[0] + rng.uniform(-14, 14)), pos[1], ta.f32(pt[2] + rng.uniform(-14, 14))]
            elif r < 0.4:
                pos = [ta.f32(pos[0] + rng.uniform(-30, 30)), pos[1], ta.f32(pos[2] + rng.uniform(-30, 30))]
            now = ta.f32(now + rng.choice([1 / 30, 0.5, 4.0, 12.0, 40.0]))
            set_world(w, now, pos)
            if rng.random() < 0.15:
                rw[1], rw[2] = int(rng.random() < 0.8), int(rng.random() < 0.6)
                orig.repair_world(*rw[:4], orig.e.r32(orig.brain + 0x80), orig.e.r8(orig.brain + 0x2a4), rw[6])
                probe.probe_ao_repair_world(rw[0], rw[1], rw[2], (F * 3)(*pt), orig.e.r32(orig.brain + 0x80),
                                            orig.e.r8(orig.brain + 0x2a4), rw[6])
            r = rng.random()
            if r < 0.65:  # arrived (the carrot at its floor, the pilot on it), or close by
                sb = bytearray(orig.e.cpu.read(orig.brain + 0x250, 0x48))
                floor = struct.unpack_from('<f', sb, 0x14)[0]
                if r < 0.45:
                    struct.pack_into('<f', sb, 0x1c, floor - rng.choice([0.0, 1.0]))
                    struct.pack_into('<ff', sb, 0x2c, rng.uniform(-0.5, 0.5), rng.uniform(-0.5, 0.5))
                else:  # still walking, within the near distance or just past it
                    slow = rows[-1][21]
                    slow = struct.unpack('<f', struct.pack('<I', slow))[0]
                    struct.pack_into('<f', sb, 0x1c, floor + 1.0)
                    struct.pack_into('<ff', sb, 0x2c, slow * rng.uniform(0.1, 0.3), slow * rng.uniform(0.0, 0.15))
                orig.e.cpu.write(orig.brain + 0x250, bytes(sb))
                probe.probe_ao_steer_load((ctypes.c_uint8 * 0x48).from_buffer_copy(sb))
            dt = ta.f2i(rng.choice([1 / 30, 0.25]))
            orig.op(16, 0, 0, dt)
            probe.probe_ao_op(16, 0, 0, dt)
            check(f'round {k} frame {f} phase {ph}')
            stats['runs'] = stats.get('runs', 0) + 1
        while len(orig.orders()) > 1:
            orig.op(7, 0, 0, 0)
            probe.probe_ao_op(7, 0, 0, 0)
            check(f'round {k} unwind')
    probe.probe_ao_done()
    return stats


def main():
    probe = setup_probe()
    cases = int(os.environ.get('REPAIR_CASES', '300'))
    total = {}
    for seed in range(cases):
        for k, v in run_case(probe, seed, 6).items():
            total[k] = total.get(k, 0) + v
    never = [COVER[a] for a in COVER if a not in REACHED]
    if never:
        raise SystemExit(f'branches never reached: {never}')
    print(f'ok: {cases} cases, {total.get("refits", 0)} refits ({total.get("taken", 0)} taken), '
          f'{total.get("runs", 0)} runs, identical; {len(COVER)} branches reached')


if __name__ == '__main__':
    main()
