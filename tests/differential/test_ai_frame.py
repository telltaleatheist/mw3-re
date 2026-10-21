"""The brain's frame 0x4e9710 (docs/ai.md 17.1) against the original x86.

    cmake --build build && .venv/bin/python tests/differential/test_ai_frame.py

The brain, its order stack and the fake nets and behaviours are
test_ai_orders.py's, the vehicle (health, position, contacts) and the gates
test_ai_gates.py's. Added: the brain's own vtable (vt+0x04 the controls'
smoothing, vt+0x1c the firing's no-op), its IControls (vt+0x14 the stop,
vt+0xa0 hit lately), its own order (+0x194, vt+0x04), three contacts with
the objects they see and those objects' vehicles (vt+0xd8 their own nets,
+0xa4 their ranges), the vehicle's vt+0xa0 (powered down) and weapon
manager (+0xd4: its ammo value 0x4b4ed0 an x87 return), and stubs for
what later items port: the radio's
lines 0x4e6d50, the firing's reset 0x4e9590 and run 0x4e9fb0, the target's
behaviour 0x4c0e10 and the attack 0x4e9ca0; the pick 0x4d6b90 and rand are
answered from the trace. Each case runs frames from one order with random
settings, flags, timers, contacts, the AI switch [0x60b938], the intensity
[0x608e54] and the time; after each frame the answer, the awake count
[0x6099c0], the fields the frame writes (+0x3c, +0xcc, +0xdc, +0xe8, +0xf4,
+0x100, +0x230, +0x22c, +0x104), every call and the stack must match, at
24-bit x87. Not made: a NaN attack range with no target vehicle (the exe
calls through null there; the port's FIX (bug) attacks no one).
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

os.environ.setdefault('MW3_X87', '24')
sys.path.insert(0, str(Path(__file__).parent))
import test_ai_gates as tg  # noqa: E402
import test_ai_orders as to  # noqa: E402
from calltrace import Trace  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402

U, I = ctypes.c_uint32, ctypes.c_int32
FRAME = 0x4e9710
OWNER = tg.OWNER
VEHICLES = OWNER + 0x2000
COVER = {0x4e9771: 'AI off', 0x4e978b: 'powered down', 0x4e97d7: 'engaging from now', 0x4e97ff: 'the player contact',
         0x4e9807: 'the pick', 0x4e9831: 'out of sight', 0x4e9875: 'out of ammo', 0x4e9894: 'low ammo',
         0x4e990c: 'the firing again', 0x4e99c7: 'a target vehicle', 0x4e99de: 'in range', 0x4e9a13: 'the attack',
         0x4e9a9a: 'a new wait', 0x4e9af4: 'the wait over', 0x4e9b7d: 'the own order',
         0x4e9bda: 'asleep, its attacker near', 0x4e9c18: 'asleep', 0x4e9c27: 'the hurt bit set'}
REACHED = set()


def f2i(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def script(seed):
    def chance(salt, p, yes=1, no=0):
        return lambda n, args: yes if random.Random(f'{seed}:{salt}:{n}').random() < p else no
    s = to.script(seed)
    s['owner.vt+0x98'] = tg.health_script(seed)
    s['state.vt+0x14'] = chance('again', 0.15)
    s['owner.vt+0xa0'] = chance('down', 0.15)
    s['ictl.vt+0xa0'] = chance('hit', 0.5)
    s['0x004d6b90'] = lambda n, args: random.Random(f'{seed}:pick:{n}').choice([0, 1, 2, 3])
    s['rand'] = lambda n, args: random.Random(f'{seed}:rand:{n}').randrange(32768)
    s['0x004b4ed0'] = lambda n, args: f2i(random.Random(f'{seed}:ammo:{n}').choice([0.0, 0.05, 0.1, 0.2, 0.25, 0.8, 1.0]))
    s['vehicle.vt+0xd8'] = lambda n, args: random.Random(f'{seed}:vnet:{n}').choice([0, 1, 2, 3])
    s['0x004c0e10'] = lambda n, args: random.Random(f'{seed}:abeh:{n}').choice([0, 1, 2])
    s['0x004e9ca0'] = chance('attack', 0.7)
    return s


class Original(tg.Original):
    def __init__(self, seed):
        super().__init__(seed)
        e, b = self.e, self.brain
        self.t.script = script(seed)
        # The brain's vtable: vt+0x04 the smoothing, vt+0x1c the firing's no-op (0x4b6350).
        vt = e.world_alloc(0x80)
        e.w32(b, vt)
        e.w32(vt + 0x4, e.new_stub('brain.vt+0x4', 's0', 4, self.logged))
        e.w32(vt + 0x1c, e.new_stub('@brain.vt+0x1c', 's0', 4))
        # IControls at the vbase's place: vt+0x14 the stop, vt+0xa0 hit lately.
        ictl = OWNER + 0xc30
        ivt = e.world_alloc(0x100)
        e.w32(ictl, ivt)
        e.w32(ivt + 0x14, e.new_stub('ictl.vt+0x14', '', 0, self.logged))
        e.w32(ivt + 0xa0, e.new_stub('ictl.vt+0xa0', '', 0, self.logged))
        # The own order's vt+0x04.
        ovt = e.world_alloc(0x40)
        e.w32(b + 0x194, ovt)
        e.w32(ovt + 0x4, e.new_stub('own.vt+0x4', 's0', 4, self.logged))
        # The vehicle's vt+0xa0 (powered down).
        e.w32(OWNER + 0x400 + 0xa0, e.new_stub('owner.vt+0xa0', 'c', 0, self.logged))
        # Contacts 1..3, what each sees (+0x04: vt+0x2c itself, vt+0x40 its vehicle), two vehicles.
        self.contact = [e.world_alloc(0x30) for _ in range(3)]
        self.seen = [e.world_alloc(0x20) for _ in range(3)]
        for k in range(3):
            svt = e.world_alloc(0x80)
            e.w32(self.seen[k], svt)
            e.w32(svt + 0x2c, e.new_stub('@seen.vt+0x2c', '', 0, self.this))
            e.w32(svt + 0x40, e.new_stub('@seen.vt+0x40', '', 0, self.vehicle_of))
            e.w32(self.contact[k] + 4, self.seen[k])
        for k in range(2):
            v = VEHICLES + 0x100 * k
            vvt = e.world_alloc(0x100)
            e.w32(v, vvt)
            e.w32(vvt + 0xd8, e.new_stub('vehicle.vt+0xd8', 'c', 0, self.vnet))
        self.radio = e.world_alloc(0x40)
        e.w32(self.radio + 0x24, 9)
        e.w32(self.radio + 0x28, 10)
        self.manager = e.world_alloc(0x100)
        # The stubs of the later items.
        # The steering's defaults (0x4ce620): +0x34 20, +0x38 cos(pi/4).
        e.cpu.write(b + 0x284, struct.pack('<ff', 20.0, 0.70710677))
        e.stub(0x4d6b90, '0x004d6b90', 'c,s0', 4, self.pick)
        e.stub(0x4e6d50, '0x004e6d50', 'c', 4)
        e.stub(0x4e9590, '0x004e9590', '', 0)
        e.stub(0x4e9fb0, '0x004e9fb0', 's0', 4)
        e.stub(0x4c0e10, '0x004c0e10', 'c', 0, self.abeh)
        e.stub(0x4e9ca0, '0x004e9ca0', 's0', 4, self.attack)
        e.import_stub(0x5bb668, 'rand', '', 0)
        # 0x4b4ed0, the ammo value: fld [val]; ret, the value answered on entry.
        self.ammo_val = OWNER + 0x880
        e.cpu.write(0x4b4ed0, b'\xd9\x05' + struct.pack('<I', self.ammo_val) + b'\xc3')
        e.uc.hook_add(UC_HOOK_CODE, self.ammo_hit, begin=0x4b4ed0, end=0x4b4ed0)
        for addr in COVER:
            e.uc.hook_add(UC_HOOK_CODE, lambda uc, ad, sz, u: REACHED.add(ad), begin=addr, end=addr)
        self.seen_vehicle = [0, 0, 0]
        # The start-up's cosines (0x4ce5b0..0x4ce610): cos 45, 10, 20, 135 degrees.
        import math as _m
        for addr, ang in ((0x60b4a0, 0.7853981633975001), (0x60b4a4, 0.17453292519944444),
                          (0x60b49c, 0.3490658503988), (0x60b494, 2.3561944901919)):
            e.cpu.write(addr, struct.pack('<f', _m.cos(ang)))
        # MSVCRT's _copysign (two doubles, cdecl; the steering's): fld x; fabs;
        # the sign of y's top byte; fchs.
        e.cpu.import_code(0x5bb5ac, bytes.fromhex('dd442404 d9e1 f644241380 7402 d9e0 c3'))

    def state_call(self, e, key, a, pop):
        this = a[0]
        r = self.t.handle(key, [e.r32(this + 4)] + list(a[1:]))
        if key == 'state.vt+0x18':
            r = e.r32(this + 8)
        if key == 'state.vt+0x14' and r & 1:
            e.cpu.write(self.brain + 0x104, b'\x01')  # asks to run again
        e._ret(r, pop)

    def logged(self, e, key, a, pop):
        e._ret(self.t.handle(key, a), pop)

    def this(self, e, key, a, pop):
        e._ret(e.uc.reg_read(to.UC_X86_REG_ECX), pop)

    def vehicle_of(self, e, key, a, pop):
        k = self.seen.index(e.uc.reg_read(to.UC_X86_REG_ECX))
        e._ret(self.seen_vehicle[k], pop)

    def vnet(self, e, key, a, pop):
        n = self.t.handle(key, a)
        e._ret(self.net_addr.get(n, 0), pop)

    def pick(self, e, key, a, pop):
        n = self.t.handle(key, a)
        e._ret(self.contact[n - 1] if n else 0, pop)

    def abeh(self, e, key, a, pop):
        net = a[0]
        n = self.t.handle(key, [e.r32(net + 4)])
        e._ret(self.beh_addr.get(n, 0), pop)

    def attack(self, e, key, a, pop):
        net = a[0]
        e._ret(self.t.handle(key, [e.r32(net + 4) if net else 0]), pop)

    def ammo_hit(self, uc, address, size, user):
        self.e.w32(self.ammo_val, self.t.handle('0x004b4ed0', []))

    def frame_set(self, w):
        e, b = self.e, self.brain
        e.cpu.write(0x60b938, bytes([w[0]]))
        e.w32(OWNER + 0xd4, self.manager if w[1] else 0)
        e.w32(0x608e54, w[2])
        for k in range(3):
            e.w32(self.contact[k] + 0xc, w[3 + k])
            self.seen_vehicle[k] = VEHICLES + 0x100 * (w[6 + k] - 1) if w[6 + k] else 0
        for k in range(2):
            e.w32(VEHICLES + 0x100 * k + 0xa4, w[9 + k])
        e.w32(OWNER + 0x6c, self.contact[w[11] - 1] if w[11] else 0)
        e.w32(OWNER + 0x98, self.contact[w[17] - 1] if w[17] else 0)
        e.cpu.write(b + 0x2a4, bytes([w[12]]))
        e.w32(b + 0xdc, w[13])
        e.w32(b + 0xf0, w[14])
        e.w32(b + 0x100, w[15])
        e.w32(b + 0x2a8, self.radio if w[16] else 0)
        e.w32(0x6099c0, 0)

    def frame(self, dt):
        e, b = self.e, self.brain
        r = e.call(FRAME, ecx=b, stack=(dt,)) & 0xff
        out = [r, e.r32(0x6099c0), e.r32(b + 0x3c), e.cpu.read(b + 0xcc, 1)[0]] + \
              [e.r32(b + o) for o in (0xdc, 0xe8, 0xf4, 0x100, 0x230)] + \
              [e.cpu.read(b + 0x22c, 1)[0], e.cpu.read(b + 0x104, 1)[0]]
        return out


def setup_probe():
    probe = tg.setup_probe()
    probe.probe_ao_frame_set.argtypes = [ctypes.c_void_p]
    probe.probe_ao_frame.argtypes = [U, ctypes.c_void_p]
    return probe


def frame_state(rng, now):
    rf = tg.rfloat
    d2 = [rf(rng, 0, 4e6) for _ in range(3)]
    w = [int(rng.random() < 0.92), int(rng.random() < 0.7), f2i(rng.choice([0.0, 0.5, 1.0, 2.0]))]
    w += [f2i(x) for x in d2]
    w += [rng.choice([0, 0, 1, 2]) for _ in range(3)]
    w += [f2i(rf(rng, 0, 4e6)) for _ in range(2)]
    w += [rng.choice([0, 0, 1, 2, 3]), int(rng.random() < 0.3)]
    w += [f2i(rng.choice([3.4028235e38, now - 1, now + 5, rf(rng, 0, 600)])), rng.choice([0, 0, 0, 1]),
          f2i(rng.choice([now - 1, now + 3, 0.0, rf(rng, 0, 600)])), int(rng.random() < 0.5),
          rng.choice([0, 0, 1, 2, 3])]
    return w


def run_case(probe, seed, frames):
    rng = random.Random(seed)
    orig = Original(seed)
    port = Trace(script(seed))
    probe.probe_app_set_callback(port.callback())
    probe.probe_ao_new()
    first = (rng.choice(to.NETS), rng.choice(to.BEHS))
    orig.op(0, *first, 0)
    probe.probe_ao_op(0, *first, 0)
    stats = {}
    now = rng.uniform(0, 300)
    for k in range(frames):
        raw, x22c, f, has = tg.fields(rng)
        raw = bytearray(raw)
        for off in (0x20, 0x64, 0x68, 0x4c, 0xb0, 0xb4):  # finite ranges and times (NaN attack range: not made)
            raw[off:off + 4] = struct.pack('<f', rng.uniform(0, 4e6) if off == 0x20 else rng.uniform(0, 30))
        for off, choices in ((0x5c, [0, 0, 1]), (0x80, [0, 1, 1, 2]), (0x9c, [0, 1]), (0xa0, [0, 1, 1]),
                             (0xa4, [0, 1, 2])):
            raw[off:off + 4] = struct.pack('<i', rng.choice(choices))
        raw = bytes(raw)
        now += rng.choice([1 / 30, 0.5, 2.0])
        f[6] = f2i(now)
        if rng.random() < 0.5:
            f[3] = f2i(now + rng.choice([-5.0, 5.0]))  # the attack's end, near
        orig.gate_set(raw, x22c, f, has)
        probe.probe_ao_gate_set(raw, x22c, (U * 12)(*f), (I * 3)(*has))
        w = frame_state(rng, now)
        orig.frame_set(w)
        probe.probe_ao_frame_set((U * 18)(*w))
        dt = f2i(rng.choice([1 / 30, 1 / 60, 0.1]))
        start = len(orig.t.log)
        a = orig.frame(dt)
        out = (U * 11)()
        probe.probe_ao_frame(dt, out)
        b = list(out)
        if orig.t.log != port.log:
            i = next((j for j, (x, y) in enumerate(zip(orig.t.log, port.log)) if x != y),
                     min(len(orig.t.log), len(port.log)))
            raise SystemExit(f'seed {seed} frame {k}: calls differ at {i} (frame calls from {start}) w {w} x20 {struct.unpack("<f", raw[0x20:0x24])[0]}:\n'
                             f'  exe  {orig.t.log[start:i]}\n'
                             f'  exe  {orig.t.log[i:i + 5]}\n  port {port.log[i:i + 5]}')
        if a != b:
            names = ['answer', 'awake', '+0x3c', '+0xcc', '+0xdc', '+0xe8', '+0xf4', '+0x100', '+0x230', '+0x22c',
                     '+0x104']
            bad = [f'{n} {x:#x} vs {y:#x}' for n, x, y in zip(names, a, b) if x != y]
            raise SystemExit(f'seed {seed} frame {k}: {bad}')
        if orig.stack() != to.port_stack(probe):
            raise SystemExit(f'seed {seed} frame {k}: stacks differ')
        stats[a[0]] = stats.get(a[0], 0) + 1
        if len(orig.stack()) > 40:  # keep it bounded: the attack and the manoeuvre push
            for _ in range(20):
                orig.op(7, 0, 0, 0)
                probe.probe_ao_op(7, 0, 0, 0)
    probe.probe_ao_done()
    return stats, len(orig.t.log)


def main():
    probe = setup_probe()
    cases = int(os.environ.get('FRAME_CASES', '200'))
    total, calls = {}, 0
    for seed in range(cases):
        stats, n = run_case(probe, seed, 60)
        calls += n
        for k, v in stats.items():
            total[k] = total.get(k, 0) + v
    never = [COVER[a] for a in COVER if a not in REACHED]
    if never:
        raise SystemExit(f'branches never reached: {never}')
    print(f'ok: {cases} cases, {sum(total.values())} frames, {calls} calls identical '
          f'(answered 1: {total.get(1, 0)}, 0: {total.get(0, 0)}); {len(COVER)} branches reached')


if __name__ == '__main__':
    code = 0
    try:
        main()
    except SystemExit as e:
        print(e)
        code = 1
    sys.stdout.flush()
    os._exit(code)
