"""The controls' hit handlers (docs/ai.md 17.8b) against the original x86:
the base 0x4aec90 (IControls vt+0x94: the hit record 0x4aec10, the
attacker's controls told when it counts the vehicle an ally) and the AI's
0x4ea520 over it (the radio's call_help and friendly_fire, the lance turning
on the player past 3 s of his hits, 0x4aec70).

    cmake --build build && .venv/bin/python tests/differential/test_ai_hits.py

The brain, its vehicle and IControls are test_ai_frame.py's (the IControls
at the vehicle's +0xc30, the hit record at its +0x04). Added: the vehicle's
vt+0x28 (itself: its teams at +0xc0 / +0xc4, its contact on the player at
+0x98), four attackers at the plan test's entity addresses (vt+0x28, +0x40,
+0x50 themselves, vt+0x140 controls whose vt+0x98 is logged), the player's
vehicle at [0x60861c] +0x14, the radio's two lines (+0x00, +0x20), 0x4d5dc0
stubbed. Each case: random teams (sharing bits or not), an attacker or
none or the vehicle itself, the player one of them or none, a radio or
none, the player's contact near or far or NaN, the record and the time
(a run under 3 s or over, a new attacker, NaNs); the calls and the record
must match, at 24-bit x87.
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
import test_ai_gates as tg  # noqa: E402
import test_ai_orders as to  # noqa: E402
from calltrace import Trace  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402

U, I = ctypes.c_uint32, ctypes.c_int32
OWNER = tg.OWNER
ENT = OWNER + 0x4000  # the probe's kPlanEnt: entity k at ENT + 0x400 k
BASE, AI = 0x4aec90, 0x4ea520
COVER = {0x4ea68c: 'last_shot', 0x4ea6ef: 'ejecting', 0x4aec32: 'a run started', 0x4aecda: 'an ally hit it', 0x4ea58d: 'an enemy hit it',
         0x4ea5b9: 'call_help', 0x4ea5cb: 'an ally attacker', 0x4ea5eb: 'friendly_fire',
         0x4ea61a: 'the lance turns'}
REACHED = set()
NAN = float('nan')


class Original(tf.Original):
    def __init__(self, seed):
        super().__init__(seed)
        e = self.e
        # The vehicle's vt+0x28: itself.
        e.w32(OWNER + 0x400 + 0x28, e.new_stub('@owner.vt+0x28', '', 0, self.this))
        self.player = e.world_alloc(0x40)  # the player's contact
        for k in range(1, 5):
            ent = ENT + 0x400 * k
            vt = e.world_alloc(0x200)
            e.w32(ent, vt)
            for slot in (0x28, 0x40, 0x50):
                e.w32(vt + slot, e.new_stub(f'@ent.vt+{slot:#x}', '', 0, self.this))
            ctl, cvt = e.world_alloc(0x10), e.world_alloc(0x100)
            e.w32(ctl, cvt)
            e.w32(cvt + 0x98, e.new_stub('attacker.ictl.vt+0x98', 's0', 4, self.ally_hit))
            e.w32(ctl + 4, k)
            e.w32(vt + 0x140, e.new_stub('@ent.vt+0x140', '', 0, lambda e2, key, a, pop, c=ctl: e2._ret(c, pop)))
        self.pilot = e.world_alloc(0x40)
        e.w32(0x60861c, self.pilot)
        self.radio2 = e.world_alloc(0x40)
        e.w32(self.radio2 + 0x00, 0)  # call_help
        e.w32(self.radio2 + 0x04, 1)  # last_shot
        e.w32(self.radio2 + 0x18, 6)  # ejecting
        e.w32(self.radio2 + 0x20, 8)  # friendly_fire
        e.w32(e.r32(OWNER + 0xc30) + 0xa4, e.new_stub('ictl.vt+0xa4', '', 0, self.logged))
        e.stub(0x4d5dc0, '0x004d5dc0', '', 0)
        self.stood_down = 0
        e.stub(0x4b6700, '@0x004b6700', '', 0, self.stand_down)
        for addr in COVER:
            e.uc.hook_add(UC_HOOK_CODE, lambda uc, ad, sz, u: REACHED.add(ad), begin=addr, end=addr)

    def this(self, e, key, a, pop):
        e._ret(e.uc.reg_read(to.UC_X86_REG_ECX), pop)

    def lines(self, kind, w):
        e, b = self.e, self.brain
        e.w32(OWNER + 0xc0, w[0])
        e.w32(OWNER + 0xc4, w[1])
        for k in range(1, 5):
            e.w32(ENT + 0x400 * k + 0xc0, w[5 + 2 * (k - 1)])
            e.w32(ENT + 0x400 * k + 0xc4, w[6 + 2 * (k - 1)])
        e.w32(b + 0x2a8, self.radio2 if w[14] else 0)
        if kind == 2:
            victim = ENT + 0x400 * w[4] if w[4] and w[4] != 5 else ENT + 0x400
            e.call(0x4ea630, ecx=b + 0x2b0, stack=(victim,))
        else:
            e.call(0x4ea6b0, ecx=b + 0x2b0)
        e.w32(b + 0x2a8, 0)

    def stand_down(self, e, key, a, pop):
        self.stood_down += 1
        e._ret(0, pop)

    def hit_ally(self, victim):
        e, b = self.e, self.brain
        e.w32(b + 0x108 + 0x28, 0)
        n = self.stood_down
        e.call(0x4ea500, ecx=b + 0x2b0, stack=(victim,))
        return e.r32(b + 0x108 + 0x28), self.stood_down - n

    def ally_hit(self, e, key, a, pop):
        ctl = e.uc.reg_read(to.UC_X86_REG_ECX)
        self.t.handle(key, [e.r32(ctl + 4), a[0]])
        e._ret(0, pop)

    def hit(self, kind, w):
        e, b = self.e, self.brain
        e.w32(OWNER + 0xc0, w[0])
        e.w32(OWNER + 0xc4, w[1])
        e.w32(self.player + 0xc, w[3])
        e.w32(OWNER + 0x98, self.player if w[2] else 0)
        attacker = OWNER if w[4] == 5 else ENT + 0x400 * w[4] if w[4] else 0
        for k in range(1, 5):
            e.w32(ENT + 0x400 * k + 0xc0, w[5 + 2 * (k - 1)])
            e.w32(ENT + 0x400 * k + 0xc4, w[6 + 2 * (k - 1)])
        e.w32(self.pilot + 0x14, ENT + 0x400 * w[13] if w[13] else 0)
        e.w32(b + 0x2a8, self.radio2 if w[14] else 0)
        e.cpu.write(OWNER + 0xc34, struct.pack('<3I', w[15], w[16], w[17]))
        e.w32(0x60991c, w[18])
        args = (0x1111, 0x2222, tf.f2i(10.0), attacker)
        if kind:
            e.call(AI, ecx=b + 0x2b0, stack=args)
        else:
            e.call(BASE, ecx=OWNER + 0xc30, stack=args)
        e.w32(b + 0x2a8, 0)
        e.w32(OWNER + 0x98, 0)
        return list(struct.unpack('<3I', bytes(e.cpu.read(OWNER + 0xc34, 12))))


def setup_probe():
    probe = tf.setup_probe()
    probe.probe_ao_hit.argtypes = [I, ctypes.c_void_p, ctypes.c_void_p]
    probe.probe_ao_lines.argtypes = [I, ctypes.c_void_p]
    probe.probe_ao_hit_ally.argtypes = [U]
    probe.probe_ao_hit_ally.restype = U
    return probe


def teams(rng):
    t = 1 << rng.randrange(4)
    allies = rng.choice([0, t, 1 << rng.randrange(4), rng.getrandbits(4), 0xffffffff & ~1])
    return t, allies


def case(rng):
    f = tf.f2i
    w = list(teams(rng))
    w += [rng.choice([0, 1, 1, 1]), f(rng.choice([100.0, 499.0, 500.0, 501.0, 4e4, NAN]))]
    w += [rng.choice([0, 1, 2, 3, 4, 4, 5])]
    for _ in range(4):
        w += list(teams(rng))
    w += [rng.choice([0, 1, 2, 3, 4, 4])]
    w += [rng.choice([0, 1, 1])]
    now = rng.choice([0.0, 5.0, 100.0, 1234.5])
    last = rng.choice([now, now - 1.0, now - 2.9, now - 3.0, now - 3.1, now - 10.0, NAN])
    start = rng.choice([last, last - 1.0, last - 2.9, last - 3.0, last - 3.5, last - 9.0, NAN])
    by = rng.choice([0, ENT + 0x400 * rng.randrange(1, 5), OWNER])
    w += [f(last), f(start), by, f(now)]
    if w[4] and rng.random() < 0.5:  # the same attacker as last time
        w[17] = OWNER if w[4] == 5 else ENT + 0x400 * w[4]
    return w


def run_case(probe, seed, checks):
    rng = random.Random(seed)
    orig = Original(seed)
    port = Trace(tf.script(seed))
    probe.probe_app_set_callback(port.callback())
    probe.probe_ao_new()
    for k in range(checks):
        kind = rng.choice([0, 1, 1])
        w = case(rng)
        start = len(orig.t.log)
        a = orig.hit(kind, w)
        out = (U * 3)()
        probe.probe_ao_hit(kind, (U * len(w))(*w), out)
        b = list(out)
        if orig.t.log != port.log:
            i = next((j for j, (x, y) in enumerate(zip(orig.t.log, port.log)) if x != y),
                     min(len(orig.t.log), len(port.log)))
            raise SystemExit(f'seed {seed} check {k} (kind {kind}): calls differ at {i}:\n'
                             f'  before {orig.t.log[start:i]}\n  exe  {orig.t.log[i:i + 4]}\n'
                             f'  port {port.log[i:i + 4]}\n  w {w}')
        if a != b:
            raise SystemExit(f'seed {seed} check {k} (kind {kind}): record exe {a}, port {b}\n  w {w}')
        # 0x4ea630 (a kill) or 0x4ea6b0 (its death's line).
        lk = rng.choice([2, 3])
        if w[4] in (0, 5):
            w[4] = 1
        start = len(orig.t.log)
        orig.lines(lk, w)
        probe.probe_ao_lines(lk, (U * len(w))(*w))
        if orig.t.log != port.log:
            raise SystemExit(f'seed {seed} check {k} (line {lk}): exe {orig.t.log[start:]}, port {port.log[start:]}\n  w {w}')
        # 0x4ea500: it hit an ally (the firing's obstacle, then its stand-down).
        victim = ENT + 0x400 * rng.randrange(1, 5)
        obstacle, stood = orig.hit_ally(victim)
        mark = len(port.log)
        mine = probe.probe_ao_hit_ally(victim)
        del port.log[mark:]  # the port's stand-down runs (test_ai_firing.py's), the exe's is stubbed
        if obstacle != victim or stood != 1 or mine != victim:
            raise SystemExit(f'seed {seed} check {k}: 0x4ea500 exe {obstacle:#x} ({stood}), port {mine:#x}')
    probe.probe_ao_done()
    return checks


def main():
    probe = setup_probe()
    cases = int(os.environ.get('HIT_CASES', '200'))
    n = sum(run_case(probe, seed, 50) for seed in range(cases))
    never = [COVER[a] for a in COVER if a not in REACHED]
    if never:
        raise SystemExit(f'branches never reached: {never}')
    print(f'ok: {cases} cases, {n} hits, calls and records identical, 0x4ea500 the same each; '
          f'{len(COVER)} branches reached')


if __name__ == '__main__':
    code = 0
    try:
        main()
    except SystemExit as e:
        print(e)
        code = 1
    sys.stdout.flush()
    os._exit(code)
