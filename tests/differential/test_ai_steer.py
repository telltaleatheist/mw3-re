"""The brain's steering (docs/ai.md 17.4) against the original x86: the
segment set 0x4cea30 (with the direction 0x4ce530), the frame's update
0x4cec80 with the measure 0x4ceb50 and the projection 0x4ceb00, the carrot's
moves 0x4ce800 / 0x4ce820, the near test 0x4ce6a0, the carrot's point
0x4ce6f0, and the contacts' push 0x4ce780 / 0x4ce7d0 / 0x4ce9f0 / 0x4ce860.

    cmake --build build && .venv/bin/python tests/differential/test_ai_steer.py

The brain and its vehicle are test_ai_frame.py's (the position 0x5a85e0
answered per object). Added: the vehicle's contacts (its +0xac..+0xb0, a
std::vector of pointers, nearest first), each on an entity (its +0x04's
vt+0x2c) with a position and a flat forward (+0x24, +0x2c). Each case
places a segment, the vehicle near it and up to six contacts around it
(ahead, behind, to the sides, facing it or away, some past the push's
reach), then runs sets, updates with random times and rates, near tests
and carrot points; the steering block (0x48 bytes) loaded at random now
and then, NaNs among the numbers. After each call the answer, every call
out and the 0x48 bytes must match, at 24-bit x87.
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
import test_ai_frame as tf  # noqa: E402
import test_ai_gates as tg  # noqa: E402
import test_ai_orders as to  # noqa: E402
from calltrace import Trace  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402

U, I = ctypes.c_uint32, ctypes.c_int32
SET, UPDATE, NEAR, CARROT = 0x4cea30, 0x4cec80, 0x4ce6a0, 0x4ce6f0
OWNER = tg.OWNER
COVER = {0x4cebab: 'the carrot kept near', 0x4cec23: 'near: the contacts', 0x4ce7e2: 'a contact out of reach',
         0x4ce7ec: 'a contact in reach', 0x4ce8b7: 'ahead of the vehicle', 0x4ce8cd: 'behind the carrot',
         0x4ce8ed: 'within the band', 0x4ce917: 'room: pushed', 0x4ce97e: 'facing it', 0x4ce98d: 'head on',
         0x4cecaf: 'the side snapped', 0x4ceccd: 'the carrot at its floor', 0x4ce6e5: 'near', 0x4ce6ec: 'not near'}
REACHED = set()
f2i = tf.f2i


def fi(i):
    return struct.unpack('<f', struct.pack('<I', i & 0xffffffff))[0]


class Original(tf.Original):
    def __init__(self, seed):
        super().__init__(seed)
        e = self.e
        self.steer = self.brain + 0x250
        self.ents = []
        for _ in range(6):
            ent = e.world_alloc(0x60)
            evt = e.world_alloc(0x40)
            e.w32(ent, evt)
            e.w32(evt + 0x2c, e.new_stub('@ent.vt+0x2c', '', 0, self.this))
            self.ents.append(ent)
        self.cts = [e.world_alloc(0x30) for _ in range(6)]
        self.vec = e.world_alloc(0x40)
        self.args = e.world_alloc(0x40)
        e.stub(0x5a85e0, '0x005a85e0', 'c', 0, self.position)
        for addr in COVER:
            e.uc.hook_add(UC_HOOK_CODE, lambda uc, ad, sz, u: REACHED.add(ad), begin=addr, end=addr)

    def position(self, e, key, a, pop):
        self.t.handle(key, a)
        e._ret(a[0] + 0x40 if a[0] in self.ents else self.pos, 0)

    def world(self, ents, cs):
        e = self.e
        for k, (pos, fwd) in enumerate(ents):
            ent = self.ents[k]
            e.cpu.write(ent + 0x24, struct.pack('<I', fwd[0]))
            e.cpu.write(ent + 0x2c, struct.pack('<I', fwd[1]))
            e.cpu.write(ent + 0x40, struct.pack('<3I', *pos))
        for k, (d2, ent) in enumerate(cs):
            e.w32(self.cts[k] + 4, self.ents[ent])
            e.w32(self.cts[k] + 0xc, d2)
            e.w32(self.vec + 4 * k, self.cts[k])
        e.w32(OWNER + 0xac, self.vec)
        e.w32(OWNER + 0xb0, self.vec + 4 * len(cs))

    def call(self, op, f):
        e, s = self.e, self.steer
        out = [0, 0, 0]
        if op == 0:
            e.cpu.write(self.args, struct.pack('<6I', *f[:6]))
            e.call(SET, ecx=s, stack=(self.args, self.args + 12, f[6], OWNER))
        elif op == 1:
            e.call(UPDATE, ecx=s, stack=(f[0], OWNER))
        elif op == 2:
            out[0] = e.call(NEAR, ecx=s) & 0xff
        else:
            e.call(CARROT, ecx=s, stack=(self.args,))
            out = [e.r32(self.args + 4 * k) for k in range(3)]
        return out + [e.r32(s + 4 * k) for k in range(18)]


def setup_probe():
    probe = tf.setup_probe()
    probe.probe_ao_steer_world.argtypes = [ctypes.c_void_p, I, ctypes.c_void_p, I]
    probe.probe_ao_steer.argtypes = [I, ctypes.c_void_p, ctypes.c_void_p]
    probe.probe_ao_steer_load.argtypes = [ctypes.c_void_p]
    return probe


def scene(rng):
    """A segment, the vehicle near its start and contacts around it."""
    nan = float('nan')
    frm = [rng.uniform(-500, 500), rng.uniform(0, 50), rng.uniform(-500, 500)]
    ang = rng.uniform(-math.pi, math.pi)
    ln = rng.choice([0.0, rng.uniform(5, 300), rng.uniform(1, 30)])
    d = (math.cos(ang), math.sin(ang))
    to_ = [frm[0] + d[0] * ln, frm[1] + rng.uniform(-5, 5), frm[2] + d[1] * ln]
    own = [frm[0] + d[0] * rng.uniform(-30, 60) + d[1] * rng.uniform(-25, 25), frm[1],
           frm[2] + d[1] * rng.uniform(-30, 60) - d[0] * rng.uniform(-25, 25)]
    if rng.random() < 0.02:
        own[0] = nan
    ents = []
    for _ in range(rng.randrange(0, 7)):
        u, v = rng.uniform(-40, 120), rng.choice([rng.uniform(-12, 12), rng.uniform(-3, 3)])
        p = [own[0] + d[0] * u + d[1] * v, own[1], own[2] + d[1] * u - d[0] * v]
        fa = rng.choice([ang + math.pi + rng.uniform(-0.6, 0.6), ang + rng.uniform(-0.5, 0.5),
                         rng.uniform(-math.pi, math.pi)])
        ents.append(([f2i(x) for x in p], [f2i(math.cos(fa)), f2i(math.sin(fa))]))
    cs = sorted(((rng.choice([rng.uniform(0, 3000), rng.uniform(0, 30000)]), k) for k in range(len(ents))))
    cs = [(f2i(dd if rng.random() > 0.02 else nan), k) for dd, k in cs]
    return frm, to_, own, ents, cs


def run_case(probe, seed, calls):
    rng = random.Random(seed)
    orig = Original(seed)
    port = Trace(tf.script(seed))
    probe.probe_app_set_callback(port.callback())
    probe.probe_ao_new()
    probe.probe_ao_op(0, 1, 1, 0)
    orig.op(0, 1, 1, 0)
    stats = {}
    frm = to_ = own = None
    for k in range(calls):
        if k % 25 == 0:
            frm, to_, own, ents, cs = scene(rng)
            raw, x22c, f, has = tg.fields(rng)
            f[7:10] = [f2i(x) for x in own]
            orig.gate_set(raw, x22c, f, has)
            probe.probe_ao_gate_set(raw, x22c, (U * 12)(*f), (I * 3)(*has))
            for kk, (pos, fwd) in enumerate(ents):
                pass
            orig.world(ents, cs)
            flat = [x for kk, (pos, fwd) in enumerate(ents) for x in [orig.ents[kk]] + pos + fwd] or [0]
            flat2 = [x for d2, ent in cs for x in (d2, ent)] or [0]
            probe.probe_ao_steer_world((U * len(flat))(*flat), len(ents), (U * len(flat2))(*flat2), len(cs))
        r = rng.random()
        if r < 0.03:  # the block loaded outright
            blk = [f2i(tg.rfloat(rng, -200, 200)) for _ in range(18)]
            blk[13] = f2i(rng.choice([20.0, 5.0, 60.0]))
            raw = struct.pack('<18I', *blk)
            orig.e.cpu.write(orig.steer, raw)
            probe.probe_ao_steer_load(raw)
            continue
        if r < 0.15 or k % 25 == 0:
            op = 0
            fl = [f2i(x) for x in to_ + frm] + [f2i(rng.choice([0.0, 2.0, 10.0, rng.uniform(0, 30)]))]
        elif r < 0.75:
            op = 1
            fl = [f2i(rng.choice([1 / 30, 1 / 60, 0.25, 0.0])), 0, 0, 0, 0, 0, 0]
            if rng.random() < 0.3:  # the side's target moved, as the states do
                tgt = f2i(rng.uniform(-20, 20))
                orig.e.w32(orig.steer + 0x24, tgt)
                cur = (U * 21)()
                probe.probe_ao_steer(2, (U * 7)(), cur)
                blk = list(cur)[3:]
                blk[9] = tgt
                probe.probe_ao_steer_load(struct.pack('<18I', *blk))
        elif r < 0.88:
            op, fl = 2, [0] * 7
        else:
            op, fl = 3, [0] * 7
        start = len(orig.t.log)
        a = orig.call(op, fl)
        out = (U * 21)()
        probe.probe_ao_steer(op, (U * 7)(*fl), out)
        b = list(out)
        if orig.t.log != port.log:
            i = next((j for j, (x, y) in enumerate(zip(orig.t.log, port.log)) if x != y),
                     min(len(orig.t.log), len(port.log)))
            raise SystemExit(f'seed {seed} call {k} op {op}: calls differ at {i}:\n  before {orig.t.log[start:i][-4:]}\n'
                             f'  exe  {orig.t.log[i:i + 4]}\n  port {port.log[i:i + 4]}')
        if a != b:
            names = ['r0', 'r1', 'r2', 'start.x', 'start.y', 'start.z', 'dir.x', 'dir.z', 'along_min', 'length',
                     'along_at', 'across', 'across_to', 'rate', 'along', 'side', 'near', 'x38', 'x3c', 'x40', 'x44']
            bad = [f'{n} {fi(x)} vs {fi(y)}' for n, x, y in zip(names, a, b) if x != y]
            raise SystemExit(f'seed {seed} call {k} op {op}: {bad}')
        stats[op] = stats.get(op, 0) + 1
    probe.probe_ao_done()
    return stats


def main():
    probe = setup_probe()
    cases = int(os.environ.get('STEER_CASES', '200'))
    total = {}
    for seed in range(cases):
        for k, v in run_case(probe, seed, 150).items():
            total[k] = total.get(k, 0) + v
    never = [COVER[a] for a in COVER if a not in REACHED]
    if never:
        raise SystemExit(f'branches never reached: {never}')
    print(f'ok: {cases} cases, {total.get(0, 0)} sets, {total.get(1, 0)} updates, {total.get(2, 0)} near tests, '
          f'{total.get(3, 0)} carrots identical; {len(COVER)} branches reached')


if __name__ == '__main__':
    code = 0
    try:
        main()
    except SystemExit as e:
        print(e)
        code = 1
    sys.stdout.flush()
    os._exit(code)
