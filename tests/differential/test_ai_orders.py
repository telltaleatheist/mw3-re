"""The brain's order stack (docs/ai.md 17.2) against the original x86: the
push 0x4e9d80 with an order's parts 0x4e80b0, the pushes 0x4e86a0,
0x4e87c0, 0x4e88c0, the remake 0x4e82e0, the state's replacement 0x4e8120,
the pops 0x4e8380, 0x4e84a0, an order's own pop 0x4e8170 and run 0x4e8150,
the topmost unmarked order 0x4e8c60, the unwind 0x4e8990, the stop 0x4ea4e0,
may it fire 0x4e9070, and from the IControls side 0x4e8660, 0x4e8960 and
0x4e9100.

    cmake --build build && .venv/bin/python tests/differential/test_ai_orders.py

The nets and behaviours are fakes on both sides: a net's vt+0x5c builds a
numbered part in the order (in the exe in place at +0x30: vtable, number,
the net), a behaviour's vt+0x8c a numbered state (+0x54), and every call on
a part or state goes to one calltrace.Trace, answered from one script (a
state's vt+0x04 / +0x08 by occurrence). Nets 1..4 are compiled, 5 is not
(its vt+0x34 cast answers 0), 100 is the built-in net at 0x604008; the
behaviour 100 is the default one at 0x604068. Each case is a random run of
operations from an empty brain; after each one the call trace so far, the
answer and the whole stack (each order's mark, owner, part and state by
number with their net and behaviour, +0x14..+0x2f) must match. The deque's
blocks and map are the C++ library's in the port: their allocations are
not compared. Operations the exe would crash on are not made: popping the
last order (0x4e8380 then resumes through a null block), an order's run or
state with none left after 0x4e9100 emptied the stack (the next case starts
again with a push). 0x4e9100's search for a net no order is on walks past a
full last block into the deque's map (the push takes its next block only
when it needs it), which crashes the exe: the port answers no (FIX (bug));
those cases are counted, not made.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import CALLBACK, Emu, Trace, load_probe  # noqa: E402
from unicorn.x86_const import UC_X86_REG_ECX  # noqa: E402

U, I = ctypes.c_uint32, ctypes.c_int32
F = {'push0': 0x4e9d80, 'reset': 0x4e80b0, 'push': 0x4e87c0, 'push_net': 0x4e86a0, 'push_beh': 0x4e88c0,
     'remake': 0x4e82e0, 'set_beh': 0x4e8120, 'pop': 0x4e8380, 'pop_if': 0x4e84a0, 'pop_if_obj': 0x4e8660,
     'push_obj': 0x4e8960, 'clear_if_obj': 0x4e9100, 'top': 0x4e8c60, 'unwind': 0x4e8990, 'stop': 0x4ea4e0,
     'may_fire': 0x4e9070, 'run': 0x4e8150, 'order_pop': 0x4e8170}
NETS = [1, 2, 3, 4, 5, 100]
BEHS = [1, 2, 3, 4, 100]
BUILTIN, DEFAULT = 0x604008, 0x604068
BLOCK = 0xf70


def script(seed):
    def boolish(salt):
        return lambda n, args: int(random.Random(f'{seed}:{salt}:{n}').random() < 0.6)
    return {'state.vt+0x4': boolish('b'), 'state.vt+0x8': boolish('f')}


class Original:
    def __init__(self, seed):
        self.t = Trace(script(seed))
        e = self.e = Emu(self.t)
        e.heap_stubs()
        self.serial = 0
        self.net_addr, self.beh_addr = {}, {}
        self.part_vt = self.vtable('part', {0: 's0', 4: 's0', 8: ''}, self.part_call)
        self.state_vt = self.vtable('state', {0: 's0', 4: '', 8: '', 0xc: '', 0x10: '', 0x14: 's0', 0x18: ''},
                                    self.state_call)
        for n in NETS:
            a = BUILTIN if n == 100 else e.world_alloc(0x20)
            vt = e.world_alloc(0x100)
            e.w32(a, vt)
            e.w32(a + 4, n)
            e.w32(vt + 0x5c, e.new_stub('net.vt+0x5c', 's0,s1', 8, self.make_part))
            e.w32(vt + 0x34, e.new_stub('@net.vt+0x34', '', 0, self.cast))
            self.net_addr[n] = a
        for b in BEHS:
            a = DEFAULT if b == 100 else e.world_alloc(0x20)
            vt = e.world_alloc(0x100)
            e.w32(a, vt)
            e.w32(a + 4, b)
            e.w32(vt + 0x8c, e.new_stub('beh.vt+0x8c', 's0,s1', 8, self.make_state))
            self.beh_addr[b] = a
        self.ids = {v: k for k, v in self.net_addr.items()}
        self.brain = e.alloc(0x2b0)
        e.w32(self.brain + 0x190, self.brain)  # the deque's owner

    def vtable(self, name, slots, fn):
        e = self.e
        vt = e.world_alloc(0x40)
        pops = {'': 0, 's0': 4}
        for off, spec in slots.items():
            e.w32(vt + off, e.new_stub(f'{name}.vt+{off:#x}', 'c' + (',' + spec if spec else ''), pops[spec], fn))
        return vt

    # The fakes, built in place: vtable, number, the net or behaviour.
    def make_part(self, e, key, a, pop):
        place = a[0]
        net = e.uc.reg_read(UC_X86_REG_ECX)
        self.serial += 1
        e.cpu.write(place, struct.pack('<III', self.part_vt, self.serial, net))
        self.t.handle(key, [e.r32(net + 4), self.serial])
        e._ret(place, pop)

    def make_state(self, e, key, a, pop):
        place, order = a[0], a[1]
        beh = e.uc.reg_read(UC_X86_REG_ECX)
        self.serial += 1
        # As the real states: the order at +0x04 (0x4c87f0 reads it back through a new state).
        e.cpu.write(place, struct.pack('<IIII', self.state_vt, order, beh, self.serial))
        self.t.handle(key, [e.r32(beh + 4), self.serial])
        e._ret(place, pop)

    def cast(self, e, key, a, pop):
        net = e.uc.reg_read(UC_X86_REG_ECX)
        e._ret(0 if e.r32(net + 4) == 5 else net, pop)

    def part_call(self, e, key, a, pop):
        this = a[0]
        args = [e.r32(this + 4)] + list(a[1:])
        r = self.t.handle(key, args)
        if key == 'part.vt+0x8':
            r = e.r32(this + 8)
        e._ret(r, pop)

    def state_call(self, e, key, a, pop):
        this = a[0]
        r = self.t.handle(key, [e.r32(this + 0xc)] + list(a[1:]))
        if key == 'state.vt+0x18':
            r = e.r32(this + 8)
        e._ret(r, pop)

    def orders(self):
        """The deque's orders, first to last."""
        e, b = self.e, self.brain
        first, last, cur, node = (e.r32(b + 0x164 + 4 * i) for i in range(4))
        end = e.r32(b + 0x17c)
        out = []
        p = cur
        while p != end:
            out.append(p)
            p += 0x98
            if p == last and p != end:
                node += 4
                p = e.r32(node)
                last = p + BLOCK
        return out

    def stack(self):
        e = self.e
        rows = []
        for o in self.orders():
            part, state = e.r32(o + 0xc), e.r32(o + 0x10)
            assert part in (0, o + 0x30) and state in (0, o + 0x54), 'parts not in place'
            row = [e.cpu.read(o + 4, 1)[0], int(e.r32(o + 8) == self.brain),
                   e.r32(part + 4) if part else 0, e.r32(e.r32(part + 8) + 4) if part else 0,
                   e.r32(state + 0xc) if state else 0, e.r32(e.r32(state + 8) + 4) if state else 0]
            row += [e.r32(o + 0x14 + 4 * i) for i in range(7)]
            rows.append(row)
        return rows

    def op(self, op, a, b, f):
        e, br = self.e, self.brain
        net = self.net_addr.get(a, 0)
        beh_a, beh_b = self.beh_addr.get(a, 0), self.beh_addr.get(b, 0)
        al = lambda r: r & 0xff  # noqa: E731
        orders = self.orders()
        cur = orders[-1] if orders else 0
        if op == 0:
            e.call(F['push0'], ecx=br + 0x160)
            return al(e.call(F['reset'], ecx=self.orders()[-1], stack=(net, beh_b)))
        if op == 1:
            return al(e.call(F['push'], ecx=br, stack=(net, beh_b)))
        if op == 2:
            return al(e.call(F['push_net'], ecx=br, stack=(net,)))
        if op == 3:
            return al(e.call(F['push_beh'], ecx=br, stack=(beh_a,)))
        if op == 4:
            return al(e.call(F['remake'], ecx=br, stack=(net,)))
        if op == 5:
            return al(e.call(F['set_beh'], ecx=cur, stack=(beh_a,)))
        if op == 6:
            return al(e.call(F['reset'], ecx=cur, stack=(net, beh_b)))
        if op == 7:
            e.call(F['pop'], ecx=br)
            return 0
        if op == 8:
            return e.call(F['pop_if'], ecx=br, stack=(net,))
        if op == 9:
            return e.call(F['pop_if_obj'], ecx=br + 0x2b0, stack=(net,))
        if op == 10:
            return e.call(F['push_obj'], ecx=br + 0x2b0, stack=(net,))
        if op == 11:
            return e.call(F['clear_if_obj'], ecx=br + 0x2b0, stack=(net,))
        if op == 12:
            r = e.call(F['top'], ecx=br)
            return orders.index(r) if r in orders else -1
        if op == 13:
            e.call(F['unwind'], ecx=br, stack=(orders[a] if 0 <= a < len(orders) else 0,))
            return 0
        if op == 14:
            return al(e.call(F['stop'], ecx=br, stack=(0,)))
        if op == 15:
            return al(e.call(F['may_fire'], ecx=br))
        if op == 16:
            e.call(F['run'], ecx=cur, stack=(f,))
            return 0
        if op == 17:
            return al(e.call(F['order_pop'], ecx=cur))
        if op == 18:
            if 0 <= a < len(orders):
                e.cpu.write(orders[a] + 4, bytes([b]))
            return 0
        raise ValueError(op)

    def fields(self, w):
        self.e.cpu.write(self.orders()[-1] + 0x14, struct.pack('<7I', *w))


def setup_probe():
    probe = load_probe()
    probe.probe_app_set_callback.argtypes = [CALLBACK]
    probe.probe_ao_op.argtypes = [I, I, I, U]
    probe.probe_ao_op.restype = I
    probe.probe_ao_fields.argtypes = [ctypes.c_void_p]
    probe.probe_ao_stack.argtypes = [ctypes.c_void_p, I]
    probe.probe_ao_stack.restype = I
    return probe


def port_stack(probe):
    buf = (U * (13 * 4000))()
    n = probe.probe_ao_stack(buf, 13 * 4000)
    assert n >= 0
    return [list(buf[13 * i:13 * i + 13]) for i in range(n)]


def f2i(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


class Case:
    """A random run of operations, legal for the exe."""

    def __init__(self, rng, length):
        self.rng, self.length = rng, length
        self.grow = rng.random() < 0.3  # pushes first: stacks past one block (26) and the map's growth

    def ops(self, size, marks):
        """The next operation given the stack's size and marks."""
        rng = self.rng
        some_net = lambda: rng.choice(NETS + [0])  # noqa: E731
        some_beh = lambda: rng.choice(BEHS + [0])  # noqa: E731
        if size == 0:
            return 0, some_net(), some_beh(), 0
        if self.grow and size < 90 and rng.random() < 0.6:
            return rng.choice([(1, some_net(), some_beh(), 0), (2, some_net(), 0, 0), (3, some_beh(), 0, 0),
                               (10, some_net(), 0, 0)])
        r = rng.random()
        deep = size > rng.choice([3, 8, 30, 60])  # the stacks grow past a block (26) now and then
        if r < 0.10 and not deep:
            return 1, some_net(), some_beh(), 0
        if r < 0.16 and not deep:
            return 2, some_net(), 0, 0
        if r < 0.22 and not deep:
            return 3, some_beh(), 0, 0
        if r < 0.25:
            return 4, some_net(), 0, 0
        if r < 0.29:
            return 5, rng.choice(BEHS), 0, 0
        if r < 0.32:
            return 6, some_net(), some_beh(), 0
        if r < 0.40 and size > 1:
            return rng.choice([7, 17]), 0, 0, 0
        if r < 0.48:
            return rng.choice([8, 9]), some_net(), 0, 0  # guarded below for the last order
        if r < 0.52 and not deep:
            return 10, some_net(), 0, 0
        if r < 0.54:
            return 11, some_net(), 0, 0
        if r < 0.58:
            return 12, 0, 0, 0
        if r < 0.64 and size > 1:
            return 13, rng.randrange(-1, size + 1), 0, 0
        if r < 0.68:
            return 14, 0, 0, 0
        if r < 0.74:
            return 15, 0, 0, 0
        if r < 0.80:
            return 16, 0, 0, f2i(rng.choice([1 / 30, 0.25, 0.0]))
        if r < 0.92:
            return 18, rng.randrange(size), rng.choice([0, 1, 1, 2]), 0
        return 19, 0, 0, 0


def legal(op, a, stack, net_of_current):
    """Whether the exe survives it: no pop of the last order."""
    size = len(stack)
    if size == 1 and op in (8, 9) and a and net_of_current == a and (op == 8 or a != 5):  # 5 casts to none
        return False
    if op == 14 and size == 1 and stack[0][0]:
        return True  # all marked, one left: the exe unwinds to the slot before it, nothing goes
    if op == 13 and size <= 1:
        return False
    return True


def run_case(probe, seed, length):
    rng = random.Random(seed)
    orig = Original(seed)
    port = Trace(script(seed))
    probe.probe_app_set_callback(port.callback())
    probe.probe_ao_new()
    case = Case(rng, length)
    stack = []
    stats = {}
    for step in range(length):
        op, a, b, f = case.ops(len(stack), None)
        if not legal(op, a, stack, stack[-1][3] if stack else 0):
            continue
        b0 = orig.brain
        if (op == 11 and a and a != 5 and all(r[3] != a for r in stack)
                and orig.e.r32(b0 + 0x17c) == orig.e.r32(b0 + 0x178)):
            # FIX (bug): the exe's search steps past a full last block into the map (a crash);
            # the port answers no. Counted, not made.
            stats['clear_past_full_block'] = stats.get('clear_past_full_block', 0) + 1
            continue
        if op == 19:
            w = [rng.getrandbits(32)] + [f2i(rng.uniform(-500, 500)) for _ in range(6)]
            orig.fields(w)
            probe.probe_ao_fields((U * 7)(*w))
        else:
            try:
                ra = orig.op(op, a, b, f)
            except Exception as ex:
                raise SystemExit(f'seed {seed} step {step} op {op} ({a}, {b}) size {len(stack)} '
                                 f'marks {[r[0] for r in stack]} nets {[r[3] for r in stack]}: the exe: {ex}')
            rb = probe.probe_ao_op(op, a, b, f)
            if (ra & 0xffffffff) != (rb & 0xffffffff) and op not in (5, 7, 13, 16, 17, 18):
                raise SystemExit(f'seed {seed} step {step} op {op} ({a}, {b}): exe {ra}, port {rb}')
        if orig.t.log != port.log:
            i = next((k for k, (x, y) in enumerate(zip(orig.t.log, port.log)) if x != y),
                     min(len(orig.t.log), len(port.log)))
            raise SystemExit(f'seed {seed} step {step} op {op} ({a}, {b}): calls differ at {i}:\n'
                             f'  exe  {orig.t.log[i:i + 4]}\n  port {port.log[i:i + 4]}')
        stack = orig.stack()
        ps = port_stack(probe)
        if stack != ps:
            raise SystemExit(f'seed {seed} step {step} op {op} ({a}, {b}): stacks differ\n'
                             f'  exe  {stack[-3:]}\n  port {ps[-3:]}')
        stats[op] = stats.get(op, 0) + 1
        stats['deepest'] = max(stats.get('deepest', 0), len(stack))
    probe.probe_ao_done()
    return stats, len(orig.t.log)


def main():
    probe = setup_probe()
    cases = int(os.environ.get('ORDER_CASES', '300'))
    total, calls = {}, 0
    for seed in range(cases):
        stats, n = run_case(probe, seed, random.Random(seed).choice([20, 60, 200, 600]))
        calls += n
        for k, v in stats.items():
            total[k] = max(total.get(k, 0), v) if k == 'deepest' else total.get(k, 0) + v
    missing = [op for op in list(range(20)) + ['clear_past_full_block'] if op not in total]
    if missing:
        raise SystemExit(f'operations never made: {missing}')
    if total['deepest'] <= 39:  # 13 orders in the first block, 26 in the next: a third grows the map
        raise SystemExit(f'no stack grew the map ({total["deepest"]})')
    ops = sum(v for k, v in total.items() if isinstance(k, int))
    print(f'ok: {cases} cases, {ops} operations, {calls} calls identical; deepest stack {total["deepest"]}; '
          + ', '.join(f'op{k} {v}' for k, v in sorted((k, v) for k, v in total.items() if isinstance(k, int)))
          + f'; the exe would crash in 0x4e9100 {total["clear_past_full_block"]} times (FIX (bug))')


if __name__ == '__main__':
    code = 0
    try:
        main()
    except SystemExit as e:
        print(e)
        code = 1
    sys.stdout.flush()
    os._exit(code)
