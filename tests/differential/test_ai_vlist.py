"""The vlist triggers (docs/ai.md 17.5) against the original x86: the
brain's 0x4e9550 over the settings' list 0x4d9290, each entry's check and
set 0x4d8e70.

    cmake --build build && .venv/bin/python tests/differential/test_ai_vlist.py

The brain, its vehicle (health and now heat as x87 `fld` thunks answered
from the trace, position, contacts and what they see, the weapon manager's
ammo value) and its IControls are test_ai_frame.py's; the IControls'
vt+0x9c is the exe's own 0x4aed10 (the ammo value, 1 without a manager).
Added: the list at the settings' +0xd0 (the exe's std::list: an owner,
its head node, nodes with the 0x20-byte entry at +0x08), four valves (a
value holder whose vt+0x04 reads it; the set 0x4d3c00 stores the value)
and the port's real Valves beside them, two regions (their inside test
0x4c57d0 answered from the trace), the clear line 0x4d5980 likewise. Each
case is a random list of 0 to 6 entries (flags, values, ranges, regions,
valves or none, NaNs among the numbers) run with random valve values, a
picked flag and target; the calls, the picked flag and target after and
the four valves must match, at 24-bit x87.
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
VLIST = 0x4e9550
PLAYER_TICK = 0x4d8990  # the player's controls' IControls vt+0xa4
OWNER = tg.OWNER
COVER = {0x4d8e81: 'a valve asked', 0x4d8ea2: 'not at its value', 0x4d8eaf: 'hit lately asked',
         0x4d8ec8: 'heat asked', 0x4d8ee7: 'health asked', 0x4d8f02: 'ammo asked', 0x4d8f25: 'the pick',
         0x4d8f41: 'the near range', 0x4d8f53: 'the line range', 0x4d8f60: 'the clear line', 0x4d8f82: 'a region',
         0x4d8f98: 'inside or out', 0x4d8f9c: 'the inside test', 0x4d8fbc: 'the valve set'}
REACHED = set()


def script(seed):
    s = tf.script(seed)
    s['owner.vt+0x9c'] = lambda n, args: tf.f2i(tg.rfloat(random.Random(f'{seed}:heat:{n}'), 0.0, 2.0))
    s['0x004d5980'] = lambda n, args: random.Random(f'{seed}:line:{n}').choice([0, 1])
    s['0x004c57d0'] = lambda n, args: random.Random(f'{seed}:inside:{n}').choice([0, 1])
    s['ictl.vt+0xa0'] = lambda n, args: random.Random(f'{seed}:hitv:{n}').choice([0, 0, 1])
    return s


class Original(tf.Original):
    def __init__(self, seed):
        super().__init__(seed)
        e, b = self.e, self.brain
        self.t.script = script(seed)
        # The vehicle's heat, vt+0x9c: fld [val]; ret, answered on entry.
        self.heat_val = OWNER + 0x8c0
        thunk = OWNER + 0x980
        e.cpu.write(thunk, b'\xd9\x05' + struct.pack('<I', self.heat_val) + b'\xc3')
        e.w32(OWNER + 0x400 + 0x9c, thunk)
        e.uc.hook_add(UC_HOOK_CODE, self.heat_hit, begin=thunk, end=thunk)
        # The IControls' vt+0x9c: the exe's (the vehicle's manager's ammo value).
        e.w32(e.r32(OWNER + 0xc30) + 0x9c, 0x4aed10)
        # Valves: +0x04 the holder, its vt+0x04 the value at +0x04.
        hvt = e.world_alloc(0x20)
        e.w32(hvt + 4, e.new_stub('@holder.vt+0x4', '', 0, self.holder_get))
        self.valves = []
        for _ in range(4):
            v, holder = e.world_alloc(0x10), e.world_alloc(0x10)
            e.w32(v + 4, holder)
            e.w32(holder, hvt)
            self.valves.append(v)
        e.stub(0x4d3c00, '@0x004d3c00', 's0', 4, self.valve_set)
        self.regions = [e.world_alloc(0x20) for _ in range(2)]
        e.stub(0x4c57d0, '0x004c57d0', 's0', 4, self.inside)
        e.stub(0x4d5980, '0x004d5980', 'c,s0,s1', 8)
        self.list_owner = e.world_alloc(0x10)
        self.head = e.world_alloc(0x10)
        self.nodes = [e.world_alloc(0x30) for _ in range(8)]
        self.flags = OWNER + 0xe00  # the picked flag and the target, in exe memory
        # The player's controls (0x4d8990's view): a vbptr at +0x04 whose +4 is
        # 0x148 (the IControls at +0x14c, its owner at +0x10, the brain's
        # IControls vtable for vt+0xa0), the settings' vlist at +0x12c.
        self.pc = e.world_alloc(0x180)
        vbt = e.world_alloc(0x10)
        e.w32(vbt + 4, 0x148)
        e.w32(self.pc + 4, vbt)
        # Its own vtable: test_ai_frame's IControls' (at +0xc30), with vt+0xa0
        # the exe's 0x4aed40 over the hit record at +0x150 (IControls +0x04).
        vt = e.world_alloc(0x100)
        e.cpu.write(vt, bytes(e.cpu.read(e.r32(OWNER + 0xc30), 0x100)))
        e.w32(vt + 0xa0, 0x4aed40)
        e.w32(self.pc + 0x14c, vt)
        e.w32(self.pc + 0x14c + 0x10, OWNER)
        for addr in COVER:
            e.uc.hook_add(UC_HOOK_CODE, lambda uc, ad, sz, u: REACHED.add(ad), begin=addr, end=addr)

    def heat_hit(self, uc, address, size, user):
        self.e.w32(self.heat_val, self.t.handle('owner.vt+0x9c', [OWNER]))

    def holder_get(self, e, key, a, pop):
        e._ret(e.r32(e.uc.reg_read(to.UC_X86_REG_ECX) + 4), pop)

    def valve_set(self, e, key, a, pop):
        v = e.uc.reg_read(to.UC_X86_REG_ECX)
        holder = e.r32(v + 4)
        old = e.r32(holder + 4)
        e.w32(holder + 4, a[0])
        e._ret(old, pop)

    def inside(self, e, key, a, pop):
        r = e.uc.reg_read(to.UC_X86_REG_ECX)
        e._ret(self.t.handle(key, [self.regions.index(r) + 1]), pop)

    def vlist_set(self, rows, values):
        e, b = self.e, self.brain
        for v, x in zip(self.valves, values):
            e.w32(e.r32(v + 4) + 4, x)
        if rows is None:
            e.w32(b + 0xd0, 0)
            return
        e.w32(b + 0xd0, self.list_owner)
        e.w32(self.list_owner + 4, self.head)
        prev = self.head
        for k, w in enumerate(rows):
            nd = self.nodes[k]
            e.w32(prev, nd)
            e.w32(nd + 4, prev)
            body = struct.pack('<5I', *w[:5]) + struct.pack('<I', self.regions[w[5] - 1] if w[5] else 0) + \
                bytes([w[6], w[7], 0, 0]) + struct.pack('<I', self.valves[w[8] - 1] if w[8] else 0)
            e.cpu.write(nd + 8, body)
            prev = nd
        e.w32(prev, self.head)
        e.w32(self.head + 4, prev)

    def vlist(self, picked, target):
        e, b = self.e, self.brain
        e.cpu.write(self.flags, bytes([picked]) + bytes(3))
        e.w32(self.flags + 4, self.contact[target - 1] if target else 0)
        e.call(VLIST, ecx=b, stack=(OWNER, self.flags, self.flags + 4))
        t = e.r32(self.flags + 4)
        out = [e.cpu.read(self.flags, 1)[0], self.contact.index(t) + 1 if t in self.contact else 0]
        out += [e.r32(e.r32(v + 4) + 4) for v in self.valves]
        e.w32(b + 0xd0, 0)
        return out


    def player_vlist(self, hit):
        e, b = self.e, self.brain
        e.cpu.write(self.pc + 0x150, struct.pack('<3I', *hit))
        e.w32(self.pc + 0x12c, e.r32(b + 0xd0))
        e.call(PLAYER_TICK, ecx=self.pc + 0x14c)
        e.w32(self.pc + 0x12c, 0)
        e.w32(b + 0xd0, 0)
        return [e.r32(e.r32(v + 4) + 4) for v in self.valves]


def setup_probe():
    probe = tf.setup_probe()
    probe.probe_ao_vlist_set.argtypes = [ctypes.c_void_p, I, ctypes.c_void_p, ctypes.c_void_p]
    probe.probe_ao_vlist.argtypes = [I, I, ctypes.c_void_p]
    probe.probe_ao_player_vlist.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    return probe


def entry(rng):
    rf = tg.rfloat
    w = [tf.f2i(rf(rng, 0, 1)), tf.f2i(rf(rng, 0, 1)), tf.f2i(rf(rng, 0, 2)),
         tf.f2i(rf(rng, 0, 4e6)), tf.f2i(rf(rng, 0, 4e6))]
    w += [rng.choice([0, 0, 1, 2])]
    flags = rng.getrandbits(8) & rng.choice([0xff, 0xfe, 0xc2, 0x3e, 0x02, 0x80, 0x40])
    w += [flags, rng.choice([0, 1, 2, 3, rng.getrandbits(8)]), rng.choice([0, 1, 2, 3, 4, 4])]
    return w


def run_case(probe, seed, checks):
    rng = random.Random(seed)
    orig = Original(seed)
    port = Trace(script(seed))
    probe.probe_app_set_callback(port.callback())
    probe.probe_ao_new()
    stats = {'sets': 0, 'checks': 0}
    for k in range(checks):
        raw, x22c, f, has = tg.fields(rng)
        orig.gate_set(raw, x22c, f, has)
        probe.probe_ao_gate_set(raw, x22c, (U * 12)(*f), (I * 3)(*has))
        w = tf.frame_state(rng, 100.0)
        orig.frame_set(w)
        probe.probe_ao_frame_set((U * 18)(*w))
        rows = None if rng.random() < 0.05 else [entry(rng) for _ in range(rng.randrange(0, 7))]
        values = [rng.choice([0, 0, 1, 1, 2, -1]) for _ in range(4)]
        picked, target = int(rng.random() < 0.3), rng.choice([0, 1, 2, 3])
        orig.vlist_set(rows, values)
        flat = [x for r in (rows or []) for x in r] or [0]
        probe.probe_ao_vlist_set((U * len(flat))(*flat), -1 if rows is None else len(rows), (I * 4)(*values),
                                 (U * 3)(*orig.seen))
        start = len(orig.t.log)
        a = orig.vlist(picked, target)
        out = (I * 6)()
        probe.probe_ao_vlist(picked, target, out)
        b = [x & 0xffffffff for x in out]
        a = [x & 0xffffffff for x in a]
        if orig.t.log != port.log:
            i = next((j for j, (x, y) in enumerate(zip(orig.t.log, port.log)) if x != y),
                     min(len(orig.t.log), len(port.log)))
            raise SystemExit(f'seed {seed} check {k}: calls differ at {i}:\n  before {orig.t.log[start:i]}\n'
                             f'  exe  {orig.t.log[i:i + 4]}\n  port {port.log[i:i + 4]}\n  rows {rows}')
        if a != b:
            raise SystemExit(f'seed {seed} check {k}: exe {a}, port {b}\n  rows {rows} values {values}')
        stats['checks'] += 1
        stats['sets'] += sum(1 for x, y in zip(values, a[2:]) if (x & 0xffffffff) != y)
        # The same list on the player's controls (0x4d8990): nothing picked yet.
        orig.vlist_set(rows, values)
        probe.probe_ao_vlist_set((U * len(flat))(*flat), -1 if rows is None else len(rows), (I * 4)(*values),
                                 (U * 3)(*orig.seen))
        start = len(orig.t.log)
        now = struct.unpack('<f', struct.pack('<I', f[6]))[0]
        last = rng.choice([now, now - 0.5, now - 1.0, now - 2.0, 0.0, float('nan')])
        hit = [tf.f2i(last), tf.f2i(last - 1.0), rng.choice([0, OWNER])]
        a = [x & 0xffffffff for x in orig.player_vlist(hit)]
        out = (I * 4)()
        probe.probe_ao_player_vlist((U * 3)(*hit), out)
        b = [x & 0xffffffff for x in out]
        if orig.t.log != port.log:
            i = next((j for j, (x, y) in enumerate(zip(orig.t.log, port.log)) if x != y),
                     min(len(orig.t.log), len(port.log)))
            raise SystemExit(f'seed {seed} check {k} (player): calls differ at {i}:\n  before {orig.t.log[start:i]}\n'
                             f'  exe  {orig.t.log[i:i + 4]}\n  port {port.log[i:i + 4]}\n  rows {rows}')
        if a != b:
            raise SystemExit(f'seed {seed} check {k} (player): exe {a}, port {b}\n  rows {rows} values {values}')
        stats['player'] = stats.get('player', 0) + 1
        stats['player sets'] = stats.get('player sets', 0) + sum(1 for x, y in zip(values, a) if (x & 0xffffffff) != y)
    probe.probe_ao_done()
    return stats


def main():
    probe = setup_probe()
    cases = int(os.environ.get('VLIST_CASES', '200'))
    total = {}
    for seed in range(cases):
        for k, v in run_case(probe, seed, 50).items():
            total[k] = total.get(k, 0) + v
    never = [COVER[a] for a in COVER if a not in REACHED]
    if never:
        raise SystemExit(f'branches never reached: {never}')
    print(f'ok: {cases} cases, {total["checks"]} lists run, {total["sets"]} valves set, identical; '
          f'the player\'s tick {total["player"]} runs, {total["player sets"]} valves set; '
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
