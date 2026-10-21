"""The plain behaviour states (docs/ai.md 17.2) against the original x86:
the base state's run 0x4c8690, the default behaviour's 0x4c87f0, the null
step's (0x4c8700), 0x604058's (0x4c8940 / 0x4c89a0), 0x604028's (0x4c88c0),
the shared behaviour's 0x4cbeb0, each made in an order by its behaviour's
factory (vt+0x8c through 0x4e8120) and asked its slots (+0x04, +0x08,
+0x18).

    cmake --build build && .venv/bin/python tests/differential/test_ai_states.py

The brain, its order stack, fake nets and behaviours, its vehicle and its
IControls (now with vt+0x10, the fire) are test_ai_frame.py's. Added: the
real behaviours in the emulator (the default one built with its vtable
0x5cb278 beside the fake at 0x604068 the stack's pushes use; the shared one
at 0x60b460, the null step at 0x603fa0, 0x604058, 0x604028 with theirs),
the formation net 0x603fb8 as a fake net, the player's pilot [0x60861c]
(its +0x6c lance object's vt+0x48 and vt+0x2c, its vehicle +0x14), the
nearest step 0x4d51d0 and a vehicle net's follow behaviour 0x4c0de0
answered from the trace. Each case makes these states on the current order
at random, runs them and asks them, between stack operations; after each
call the answer, the calls, every order (its part, its state: a fake's
number or the real one's class, +0x24..+0x2f, its owner) and the brain's
+0x104 and +0x298 must match.
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
SET_BEHAVIOUR = 0x4e8120
# The port's order of the real behaviours (probe_ao_state), their vtables.
REAL = [('default', None, 0x5cb278), ('shared', 0x60b460, 0x5cbe18), ('null', 0x603fa0, 0x5bc8d0),
        ('stand', 0x604058, 0x5bc988), ('hold', 0x604028, 0x5bca40)]
STATE_VT = {0x5cb330: 'default', 0x5cbed0: 'shared', 0x5cb258: 'null', 0x5cb370: 'stand', 0x5cb350: 'hold'}
COVER = {0x4c86b0: 'the base run', 0x4c8856: 'the default: the nearest step', 0x4c887c: 'the default: replaced',
         0x4cbf12: 'a lancemate', 0x4cbf43: 'the formation', 0x4cbf7e: "the player's net",
         0x4cbfad: 'the shared: the nearest step', 0x4cc008: 'the shared: pushed'}
REACHED = set()


def script(seed):
    s = tf.script(seed)
    s['0x004d51d0'] = lambda n, args: random.Random(f'{seed}:near:{n}').choice([0, 1, 2, 3, 4])
    s['0x004c0de0'] = lambda n, args: random.Random(f'{seed}:follow:{n}').choice([0, 1, 2])
    s['lance.vt+0x2c'] = lambda n, args: random.Random(f'{seed}:lance:{n}').choice([4, 4, 0, 1, 3])
    s['vehicle.vt+0xd8'] = lambda n, args: random.Random(f'{seed}:vnet2:{n}').choice([0, 1, 2, 3])
    return s


class Original(tf.Original):
    def __init__(self, seed):
        super().__init__(seed)
        e, b = self.e, self.brain
        self.t.script = script(seed)
        self.real = []
        for name, addr, vt in REAL:
            a = addr or e.world_alloc(0x10)
            e.cpu.write(a, struct.pack('<III', vt, 0, 0))
            self.real.append(a)
        # The formation net 0x603fb8: a fake net, number 200.
        nvt = e.world_alloc(0x100)
        e.w32(0x603fb8, nvt)
        e.w32(0x603fb8 + 4, 200)
        e.w32(nvt + 0x5c, e.new_stub('net.vt+0x5c', 's0,s1', 8, self.make_part))
        e.w32(nvt + 0x34, e.new_stub('@net.vt+0x34', '', 0, self.cast))
        e.w32(nvt + 0x54, e.new_stub('@net.vt+0x54', '', 0, self.allows))
        self.net_addr[200] = 0x603fb8
        # IControls vt+0x10: the fire.
        e.w32(e.r32(OWNER + 0xc30) + 0x10, e.new_stub('ictl.vt+0x10', 's0', 4, self.logged))
        # The player's pilot, its lance object and its vehicle.
        self.pilot = e.world_alloc(0x40)
        lance = e.world_alloc(0x10)
        lvt = e.world_alloc(0x40)
        e.w32(lance, lvt)
        e.w32(lvt + 0x48, e.new_stub('lance.vt+0x48', '', 0, self.logged))
        e.w32(lvt + 0x2c, e.new_stub('lance.vt+0x2c', '', 0, self.logged))
        e.w32(self.pilot + 0x6c, lance)
        # The nearest step's nodes: each one's vt+0x74 its behaviour.
        self.nodes = []
        for k in range(1, 5):
            node, nvt2 = e.world_alloc(0x10), e.world_alloc(0x80)
            e.w32(node, nvt2)
            e.w32(nvt2 + 0x74, e.new_stub('@node.vt+0x74', '', 0, self.node_beh))
            e.w32(node + 4, k)
            self.nodes.append(node)
        e.stub(0x4d51d0, '0x004d51d0', 's0', 4, self.nearest)
        e.stub(0x4c0de0, '0x004c0de0', '', 0, self.follow)
        for addr in COVER:
            e.uc.hook_add(UC_HOOK_CODE, lambda uc, ad, sz, u: REACHED.add(ad), begin=addr, end=addr)

    def node_beh(self, e, key, a, pop):
        e._ret(self.beh_addr[e.r32(e.uc.reg_read(to.UC_X86_REG_ECX) + 4)], pop)

    def nearest(self, e, key, a, pop):
        net = e.uc.reg_read(to.UC_X86_REG_ECX)
        n = self.t.handle(key, [e.r32(net + 4)])
        e._ret(self.nodes[n - 1] if n else 0, pop)

    def follow(self, e, key, a, pop):
        net = e.uc.reg_read(to.UC_X86_REG_ECX)
        n = self.t.handle(key, [e.r32(net + 4)])
        e._ret(self.beh_addr.get(n, 0), pop)

    def world(self, xbc, pilot, pv):
        e = self.e
        e.cpu.write(OWNER + 0xbc, bytes([xbc]))
        e.w32(0x60861c, self.pilot if pilot else 0)
        e.w32(self.pilot + 0x14, pv)

    def state(self, op, which, dt):
        e = self.e
        cur = self.orders()[-1]
        st = e.r32(cur + 0x10)
        if op == 0:
            return e.call(SET_BEHAVIOUR, ecx=cur, stack=(self.real[which],)) & 0xff
        vt = e.r32(st)
        if op == 1:
            e.call(e.r32(vt + 0x14), ecx=st, stack=(dt,))
            return 0
        if op in (2, 3):
            return e.call(e.r32(vt + (4 if op == 2 else 8)), ecx=st) & 0xff
        r = e.call(e.r32(vt + 0x18), ecx=st)
        if vt == self.state_vt:
            return -1  # a fake's behaviour: the port's fake, not one of the five
        return {0: 0, 0x604068: 1, 0x60b460: 2, 0x603fa0: 3, 0x604058: 4, 0x604028: 5}.get(r, -1)

    def snapshot(self):
        e, b = self.e, self.brain
        rows = []
        for o in self.orders():
            part, st = e.r32(o + 0xc), e.r32(o + 0x10)
            vt = e.r32(st) if st else 0
            code = e.r32(st + 0xc) if vt == self.state_vt else vt
            rows.append([e.r32(part + 4) if part else 0, e.r32(e.r32(part + 8) + 4) if part else 0, code,
                         e.cpu.read(o + 4, 1)[0], e.r32(o + 0x24), e.r32(o + 0x28), e.r32(o + 0x2c),
                         int(e.r32(o + 8) == b)])
        tail = [e.cpu.read(b + 0x104, 1)[0]] + [e.r32(b + 0x298 + 4 * k) for k in range(3)]
        return rows, tail


def setup_probe():
    probe = tf.setup_probe()
    probe.probe_ao_state_world.argtypes = [U, I, U]
    probe.probe_ao_state.argtypes = [I, I, U]
    probe.probe_ao_state.restype = I
    probe.probe_ao_state_snapshot.argtypes = [ctypes.c_void_p, I]
    probe.probe_ao_state_snapshot.restype = I
    return probe


def port_snapshot(probe):
    buf = (U * 4000)()
    n = probe.probe_ao_state_snapshot(buf, 4000)
    assert n >= 0
    rows = [list(buf[8 * i:8 * i + 8]) for i in range(n)]
    return rows, list(buf[8 * n:8 * n + 4])


def run_case(probe, seed, calls):
    rng = random.Random(seed)
    orig = Original(seed)
    port = Trace(script(seed))
    probe.probe_app_set_callback(port.callback())
    probe.probe_ao_new()
    first = (rng.choice(to.NETS), rng.choice(to.BEHS))
    orig.op(0, *first, 0)
    probe.probe_ao_op(0, *first, 0)
    case = to.Case(rng, calls)
    stats = {}
    for k in range(calls):
        if k % 10 == 0:
            raw, x22c, f, has = tg.fields(rng)
            orig.gate_set(raw, x22c, f, has)
            probe.probe_ao_gate_set(raw, x22c, (U * 12)(*f), (I * 3)(*has))
            xbc = rng.choice([0, 0x10, 0x10, 0x11, 0x08])
            pilot = int(rng.random() < 0.8)
            pv = rng.choice([0, tf.VEHICLES, tf.VEHICLES + 0x100])
            orig.world(xbc, pilot, pv)
            probe.probe_ao_state_world(xbc, pilot, pv)
        r = rng.random()
        stack = orig.stack()
        if r < 0.15:
            op, a, b, f = case.ops(len(stack), None)
            if op in (0, 11, 13, 19) or not to.legal(op, a, stack, stack[-1][3] if stack else 0) \
                    or (op in (7, 17) and len(stack) < 2):
                continue
            if op in (8, 9) and len(stack) == 1:
                continue
            ra = orig.op(op, a, b, f)
            rb = probe.probe_ao_op(op, a, b, f)
            what = f'stack op {op}'
        else:
            if r < 0.40:
                op, which = 0, rng.randrange(5)
            else:
                op, which = rng.choice([1, 1, 1, 2, 3, 4]), 0
            dt = tf.f2i(rng.choice([1 / 30, 0.1]))
            ra = orig.state(op, which, dt)
            rb = probe.probe_ao_state(op, which, dt)
            what = f'state op {op} ({which})'
            if (ra & 0xffffffff) != (rb & 0xffffffff):
                raise SystemExit(f'seed {seed} call {k} {what}: exe {ra}, port {rb}')
            stats[op] = stats.get(op, 0) + 1
        if orig.t.log != port.log:
            i = next((j for j, (x, y) in enumerate(zip(orig.t.log, port.log)) if x != y),
                     min(len(orig.t.log), len(port.log)))
            raise SystemExit(f'seed {seed} call {k} {what}: calls differ at {i}:\n'
                             f'  exe  {orig.t.log[i:i + 4]}\n  port {port.log[i:i + 4]}')
        a_snap, b_snap = orig.snapshot(), port_snapshot(probe)
        if a_snap != b_snap:
            raise SystemExit(f'seed {seed} call {k} {what}: states differ\n  exe  {a_snap[0][-3:]} {a_snap[1]}\n'
                             f'  port {b_snap[0][-3:]} {b_snap[1]}')
        if len(orig.orders()) > 40:
            for _ in range(20):
                orig.op(7, 0, 0, 0)
                probe.probe_ao_op(7, 0, 0, 0)
    probe.probe_ao_done()
    return stats


def main():
    probe = setup_probe()
    cases = int(os.environ.get('STATE_CASES', '200'))
    total = {}
    for seed in range(cases):
        for k, v in run_case(probe, seed, 80).items():
            total[k] = total.get(k, 0) + v
    never = [COVER[a] for a in COVER if a not in REACHED]
    if never:
        raise SystemExit(f'branches never reached: {never}')
    print(f'ok: {cases} cases, {total.get(0, 0)} states made, {total.get(1, 0)} runs, '
          f'{total.get(2, 0) + total.get(3, 0) + total.get(4, 0)} slots asked, identical; {len(COVER)} branches reached')


if __name__ == '__main__':
    code = 0
    try:
        main()
    except SystemExit as e:
        print(e)
        code = 1
    sys.stdout.flush()
    os._exit(code)
