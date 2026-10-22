"""The firing's aim error (docs/ai.md 17.7) against the original x86: the
track 0x4b72a0 and the lead 0x4b77e0 with their frame 0x4b70b0, drift
0x4b71a0 and re-pick 0x4b74f0, on the aim object (0x4b7060's 0x24 bytes).

    cmake --build build && .venv/bin/python tests/differential/test_ai_aim.py

The world is test_ai_plan.py's (entities and their positions behind
0x5a8630, weapons 1 and 2 with their muzzles behind 0x4ed530, contacts on
the entities). Added: the difficulty's aim error [0x608e58], the vehicle's
mech (vt+0x50, or none) with its targeting computer (+0x3e4) and fire
control (+0x3b8) counts, the pilot's +0xb8 and +0xc0, the manager's aim
0x4b0bc0 (the point traced) and its +0x88; rand from the trace; the
start-up's [0x60b2ec] (0x4b7030). Each round sets a world and an aim (the
constructor's or a random one, kept across rounds otherwise), then tracks
and leads at a contact with random steps; after each call the calls, the
aim's nine words and the manager's +0x88 must match, at 24-bit x87.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

os.environ.setdefault('MW3_X87', '24')
sys.path.insert(0, str(Path(__file__).parent))
import test_ai_plan as tp  # noqa: E402
from calltrace import Trace  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402

U, I = ctypes.c_uint32, ctypes.c_int32
OWNER = tp.OWNER
TRACK, LEAD = 0x4b72a0, 0x4b77e0
f2i = tp.f2i
COVER = {0x4b72d7: 'track: an error', 0x4b74e4: 'track: none', 0x4b7113: 'a targeting aid',
         0x4b7802: 'lead: drifting', 0x4b78b1: 'lead: no error', 0x4b7888: 'lead: past the limit',
         0x4b759c: 'repick: in front', 0x4b76ea: 'repick: behind the muzzle', 0x4b75ad: 'repick: carried',
         0x4b77d7: 'repick: no error'}
RARE = {0x4b7655: 'repick: the offset 0'}  # the carried point exactly on the target
REACHED = set()


def script(seed):
    s = tp.script(seed)
    s['rand'] = lambda n, args: random.Random(f'{seed}:rand:{n}').choice(
        [random.Random(f'{seed}:r:{n}').randrange(32768), 0, 32767, 16383])
    return s


class Original(tp.Original):
    def __init__(self, seed):
        super().__init__(seed)
        e = self.e
        self.t.script = script(seed)
        e.call(0x4b7030)  # [0x60b2ec] = 2 / 100 (and [0x60b2b0])
        self.aim = e.world_alloc(0x40)
        e.w32(OWNER + 0x400 + 0x50, e.new_stub('@owner.vt+0x50', '', 0, self.mech_of))
        e.stub(0x4b0bc0, '0x004b0bc0', 'c,s0', 4, self.aim_at)
        self.has_mech = 1
        for addr in list(COVER) + list(RARE):
            e.uc.hook_add(UC_HOOK_CODE, lambda uc, ad, sz, u: REACHED.add(ad), begin=addr, end=addr)

    def mech_of(self, e, key, a, pop):
        e._ret(OWNER if self.has_mech else 0, pop)

    def aim_at(self, e, key, a, pop):
        wm, p = a[0], a[1]
        self.t.handle(key, [1 if wm == self.wm else wm] + [e.r32(p + 4 * k) for k in range(3)])
        e._ret(0, pop)

    def aim_set(self, w):
        """w: the error, the aid (the port's), +0xb8, +0xc0, the manager's +0x88,
        the aim's 9 words, then the mech (or none), its +0x3e4 and +0x3b8."""
        e, b = self.e, self.brain
        e.w32(0x608e58, w[0])
        e.w32(b + 0xb8, w[2])
        e.w32(b + 0xc0, w[3])
        e.cpu.write(self.wm + 0x88, bytes([w[4]]))
        a = w[5:14]
        e.cpu.write(self.aim, struct.pack('<6I', *a[0:6]) + bytes([a[6]]) + bytes(3) + struct.pack('<2I', *a[7:9]))
        self.has_mech = w[14]
        e.w32(OWNER + 0x3e4, w[15])
        e.w32(OWNER + 0x3b8, w[16])

    def aim_op(self, op, ct, wk, dt):
        e, b = self.e, self.brain
        c, w = self.cts[ct - 1], self.weapons[wk]
        if op == 0:
            e.call(TRACK, ecx=self.aim, stack=(c, w, b))
        else:
            e.call(LEAD, ecx=self.aim, stack=(dt, c, w, self.wm, b))
        out = [e.r32(self.aim + 4 * k) for k in range(6)]
        out += [e.cpu.read(self.aim + 0x18, 1)[0], e.r32(self.aim + 0x1c), e.r32(self.aim + 0x20)]
        out += [e.cpu.read(self.wm + 0x88, 1)[0]]
        return out


def setup_probe():
    probe = tp.setup_probe()
    probe.probe_ao_aim_set.argtypes = [ctypes.c_void_p]
    probe.probe_ao_aim.argtypes = [I, I, U, U, ctypes.c_void_p]
    return probe


def aim_world(rng, fresh):
    rf = lambda lo, hi: f2i(rng.uniform(lo, hi))  # noqa: E731
    has_mech, tc, fcs = int(rng.random() > 0.15), rng.choice([0, 0, 1, 2]), rng.choice([0, 0, 0, 1])
    aid = int(bool(has_mech and (tc > 0 or fcs > 0)))
    w = [f2i(rng.choice([1.2, 1.0, 0.6, 1.0, 0.0])), aid, f2i(rng.choice([0.0, 1.0, 0.5, rng.uniform(0, 3)])),
         f2i(rng.choice([0.5, 1.0, rng.uniform(0, 2)])), 0]
    if fresh == 1:  # the constructor's
        a = [0, 0, 0, f2i(1.0), 0, 0, 0, 0, f2i(1.0)]
    else:  # a random state: big offsets put the point behind the muzzle
        big = rng.random() < 0.3
        a = [rf(-500, 500) if big else rf(-20, 20) for _ in range(3)] + [rf(-10, 10) for _ in range(3)]
        a += [rng.choice([0, 1, 1]), rf(0, 600), f2i(rng.choice([-1.0, 0.0, rng.uniform(0, 400), 1e9]))]
    return w + a + [has_mech, tc, fcs]


def run_case(probe, seed, rounds, stats):
    rng = random.Random(seed)
    orig = Original(seed)
    port = Trace(script(seed))
    probe.probe_app_set_callback(port.callback())
    probe.probe_ao_new()
    probe.probe_ao_op(0, 1, 1, 0)
    orig.op(0, 1, 1, 0)
    for k in range(rounds):
        w = tp.plan_world(rng)
        n = w[32]
        cts = [j + 1 for j in range(n) if w[33 + 5 * j + 4]]
        orig.plan_set(w)
        probe.probe_ao_plan_set((U * len(w))(*w))
        fresh = 1 if k == 0 else rng.choice([0, 1, 2, 2, 2])
        if fresh or k == 0:
            aw = aim_world(rng, fresh)
            orig.aim_set(aw)
            probe.probe_ao_aim_set((U * len(aw))(*aw))
        if not cts:
            continue
        ct = rng.choice(cts)
        for op in [rng.choice([0, 1])] + [rng.choice([0, 1, 1, 1]) for _ in range(rng.randrange(0, 8))]:
            wk = rng.choice([1, 2])
            dt = f2i(rng.choice([1 / 30, 0.1, 0.5, 2.0, 10.0, 60.0]))
            start = len(orig.t.log)
            a = orig.aim_op(op, ct, wk, dt)
            out = (U * 10)()
            probe.probe_ao_aim(op, ct, wk, dt, out)
            b = list(out)
            if orig.t.log != port.log:
                i = next((j for j, (x, y) in enumerate(zip(orig.t.log, port.log)) if x != y),
                         min(len(orig.t.log), len(port.log)))
                raise SystemExit(f'seed {seed} round {k} op {op}: calls differ at {i}:\n  before '
                                 f'{orig.t.log[start:i][-5:]}\n  exe  {orig.t.log[i:i + 4]}\n  port {port.log[i:i + 4]}')
            if a != b:
                names = ['offset.x', 'offset.y', 'offset.z', 'drift.x', 'drift.y', 'drift.z', 'active', 'until',
                         'limit2', 'wm+0x88']
                bad = [f'{nm} {x:#x} vs {y:#x}' for nm, x, y in zip(names, a, b) if x != y]
                raise SystemExit(f'seed {seed} round {k} op {op}: {bad}')
            stats[op] = stats.get(op, 0) + 1
    probe.probe_ao_plan_done()
    probe.probe_ao_done()


def main():
    probe = setup_probe()
    cases = int(os.environ.get('AIM_CASES', '300'))
    stats = {}
    for seed in range(cases):
        run_case(probe, seed, 20, stats)
    never = [COVER[a] for a in COVER if a not in REACHED]
    if never:
        raise SystemExit(f'branches never reached: {never}')
    rare = [RARE[a] for a in RARE if a not in REACHED]
    print(f'ok: {cases} cases, {stats.get(0, 0)} tracks, {stats.get(1, 0)} leads identical; '
          f'{len(COVER)} branches reached' + (f' (not reached: {", ".join(rare)})' if rare else ''))


if __name__ == '__main__':
    code = 0
    try:
        main()
    except SystemExit as e:
        print(e)
        code = 1
    sys.stdout.flush()
    os._exit(code)
