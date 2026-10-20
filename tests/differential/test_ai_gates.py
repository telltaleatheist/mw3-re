"""The brain's gates (docs/ai.md 17.3) against the original x86: may it break
off to attack 0x4e8f10, must it break off 0x4e8dd0, the brain's vt+0x0c
0x4e9640 and vt+0x10 0x4e9650 (does it see its target), the manoeuvre
0x4e96b0.

    cmake --build build && .venv/bin/python tests/differential/test_ai_gates.py

The brain and the fake nets, behaviours and order stack are
test_ai_orders.py's (the manoeuvre pushes on it: the behaviours at 0x604088
and 0x604098 are fakes too). The pilot's vehicle is a fake: its health
(vt+0x98, an x87 return: a `fld` thunk whose value the trace answers), its
position (0x5a85e0) and the two contacts the brain reads (+0x80, +0x98:
their squared distance +0x0c). Each check sets random fields on both sides
(the settings at +0x04..+0xcf with the flags byte +0xcc, the hurt flag
+0x22c, the home point +0xe4, +0xe8, +0x230, +0xf4), a game time and
position, NaNs among them, then compares the answer, +0x22c, +0xf4, the
calls (the health and position asked, the state's vt+0x04, the manoeuvre's
push) and the stack. The x87 runs at 24 bits, as in play.
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
import test_ai_orders as to  # noqa: E402
from calltrace import Trace  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402

U, I = ctypes.c_uint32, ctypes.c_int32
G = {0: 0x4e8f10, 1: 0x4e8dd0, 2: 0x4e9640, 3: 0x4e9650, 4: 0x4e96b0}
to.BEHS = to.BEHS + [88, 98]
OWNER = 0x2c7f0000  # the probe's kOwner: the emulator's world lies there too
MANOEUVRES = {88: 0x604088, 98: 0x604098}
# The exe's branches the run must reach (0x4e8f10, 0x4e8dd0, 0x4e9650, 0x4e96b0).
COVER = {0x4e8f34: 'net asked', 0x4e8f48: 'state asked', 0x4e8f5b: 'hurt decided', 0x4e8f78: 'hurt',
         0x4e8f9a: 'leash measured', 0x4e8fff: 'leash passed', 0x4e9014: 'time asked', 0x4e902c: 'health floor asked',
         0x4e904a: 'refused', 0x4e9050: 'allowed', 0x4e905e: 'engaging off or refused',
         0x4e8e10: 'break-off hurt', 0x4e8e32: 'break-off leash', 0x4e8ec4: 'break-off health floor',
         0x4e8ee8: 'break-off no', 0x4e8ef3: 'break-off yes',
         0x4e967d: 'the player contact', 0x4e9685: 'the lance contact', 0x4e969c: 'in sight', 0x4e96a0: 'out of sight',
         0x4e96c1: 'manoeuvre 1', 0x4e96d7: 'manoeuvre 0', 0x4e96ed: 'no manoeuvre'}
REACHED = set()


def f2i(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def rfloat(rng, lo, hi):
    r = rng.random()
    if r < 0.03:
        return float('nan')
    if r < 0.06:
        return rng.choice([lo, hi, 0.0])
    return rng.uniform(lo, hi)


def health_script(seed):
    def ans(n, args):
        rng = random.Random(f'{seed}:h:{n}')
        return f2i(rfloat(rng, 0.0, 1.0))
    return ans


class Original(to.Original):
    def __init__(self, seed):
        to.BUILTIN, to.DEFAULT = 0x604008, 0x604068
        super().__init__(seed)
        e = self.e
        self.t.script['owner.vt+0x98'] = health_script(seed)
        for b, a in MANOEUVRES.items():  # the behaviours the manoeuvre pushes
            vt = e.world_alloc(0x100)
            e.w32(a, vt)
            e.w32(a + 4, b)
            e.w32(vt + 0x8c, e.new_stub('beh.vt+0x8c', 's0,s1', 8, self.make_state))
            self.beh_addr[b] = a
        for n, a in self.net_addr.items():  # vt+0x54: compiled nets let a pilot break off
            e.w32(e.r32(a) + 0x54, e.new_stub('@net.vt+0x54', '', 0, self.allows))
        # The owner at the probe's address, with its health thunk and contacts.
        e.uc.mem_map(OWNER, 0x10000)
        e.w32(OWNER, OWNER + 0x400)
        self.health_val = OWNER + 0x800
        thunk = OWNER + 0x900
        e.cpu.write(thunk, b'\xd9\x05' + struct.pack('<I', self.health_val) + b'\xc3')  # fld [val]; ret
        e.w32(OWNER + 0x400 + 0x98, thunk)
        e.uc.hook_add(UC_HOOK_CODE, self.health_hit, begin=thunk, end=thunk)
        self.pos = OWNER + 0xa00
        self.home = OWNER + 0xa40
        self.contacts = (OWNER + 0xb00, OWNER + 0xb40)
        e.stub(0x5a85e0, '0x005a85e0', 'c', 0, self.position)
        for addr in COVER:
            e.uc.hook_add(UC_HOOK_CODE, lambda uc, ad, sz, u: REACHED.add(ad), begin=addr, end=addr)
        # The brain finds its vehicle through its virtual base (+0xe0: a table
        # whose +4 is the IControls base's offset; the vehicle at that base's +0x10).
        vbt, slot = OWNER + 0xc00, OWNER + 0xc40
        e.w32(slot, OWNER)
        e.w32(vbt + 4, (slot - 0x10 - 0xe0 - self.brain) & 0xffffffff)
        e.w32(self.brain + 0xe0, vbt)

    def allows(self, e, key, a, pop):
        net = e.uc.reg_read(to.UC_X86_REG_ECX)
        e._ret(int(e.r32(net + 4) != 5), pop)

    def health_hit(self, uc, address, size, user):
        self.e.w32(self.health_val, self.t.handle('owner.vt+0x98', [OWNER]))

    def position(self, e, key, a, pop):
        self.t.handle(key, a)
        e._ret(self.pos, 0)

    def gate_set(self, raw, x22c, f, has):
        e, b = self.e, self.brain
        e.cpu.write(b + 4, raw[4:0xd0])
        e.cpu.write(b + 0x22c, bytes([x22c]))
        e.cpu.write(self.home, struct.pack('<3I', *f[0:3]))
        e.w32(b + 0xe4, self.home if has[0] else 0)
        e.w32(b + 0xe8, f[3])
        e.w32(b + 0x230, f[4])
        e.w32(b + 0xf4, f[5])
        e.w32(0x60991c, f[6])
        e.cpu.write(self.pos, struct.pack('<3I', *f[7:10]))
        e.w32(self.contacts[0] + 0xc, f[10])
        e.w32(self.contacts[1] + 0xc, f[11])
        e.w32(OWNER + 0x80, self.contacts[0] if has[1] else 0)
        e.w32(OWNER + 0x98, self.contacts[1] if has[2] else 0)

    def gate(self, op, net, with_state):
        e, b = self.e, self.brain
        if op == 0:
            cur = self.orders()[-1]
            r = e.call(G[0], ecx=b, stack=(OWNER, self.net_addr.get(net, 0), e.r32(cur + 0x10) if with_state else 0))
        elif op == 1:
            r = e.call(G[1], ecx=b, stack=(OWNER,))
        elif op == 4:
            e.call(G[4], ecx=b)
            r = 0
        else:
            r = e.call(G[op], ecx=b)
        if op in (2, 3):
            r &= 0xff
        return [r & 0xffffffff, e.cpu.read(b + 0x22c, 1)[0], e.r32(b + 0xf4)]


def setup_probe():
    probe = to.setup_probe()
    probe.probe_ao_gate_set.argtypes = [ctypes.c_void_p, U, ctypes.c_void_p, ctypes.c_void_p]
    probe.probe_ao_gate.argtypes = [I, I, I, ctypes.c_void_p]
    return probe


def fields(rng):
    raw = bytearray(rng.getrandbits(8) for _ in range(0xd0))

    def put(off, fmt, v):
        raw[off:off + 4] = struct.pack(fmt, v)
    put(0x3c, '<i', rng.choice([0, 1, 1, 1, rng.getrandbits(31)]))
    put(0x24, '<f', rfloat(rng, 0, 4e6))
    put(0x1c, '<f', rfloat(rng, 0, 4e6))
    put(0x50, '<f', rfloat(rng, 0, 1))
    put(0x54, '<i', rng.choice([0, 0, 1, rng.getrandbits(31)]))
    put(0x88, '<i', rng.choice([0, 0, 1, 2]))
    put(0x8c, '<i', rng.choice([0, 1, 5]))
    put(0xa4, '<i', rng.choice([0, 1, 0, 1, -1, 2]))
    raw[0xcc] = rng.getrandbits(8)
    home = [rfloat(rng, -2000, 2000) for _ in range(3)]
    pos = [h + rfloat(rng, -1500, 1500) if not math.isnan(h) else 0.0 for h in home]
    now = rfloat(rng, 0, 600)
    f = [f2i(x) for x in home] + [f2i(rfloat(rng, 0, 600)), f2i(rfloat(rng, 0, 1)), f2i(rfloat(rng, 0, 600)),
                                  f2i(now)] + [f2i(x) for x in pos] + [f2i(rfloat(rng, 0, 4e6)) for _ in range(2)]
    has = [int(rng.random() < 0.8), int(rng.random() < 0.7), int(rng.random() < 0.7)]
    return bytes(raw), rng.choice([2, 2, 2, 0, 1]), f, has


def run_case(probe, seed, checks):
    rng = random.Random(seed)
    orig = Original(seed)
    port = Trace(to.script(seed))
    port.script['owner.vt+0x98'] = health_script(seed)
    probe.probe_app_set_callback(port.callback())
    probe.probe_ao_new()
    case = to.Case(rng, checks)
    stats = {}
    stack = []

    def step(op, a, b, f):
        ra = orig.op(op, a, b, f)
        rb = probe.probe_ao_op(op, a, b, f)
        return ra, rb
    step(0, rng.choice(to.NETS), rng.choice(to.BEHS), 0)
    for k in range(checks):
        if rng.random() < 0.3:  # move the stack on (the current net and state)
            op, a, b, f = case.ops(len(stack), None)
            if op in (0, 7, 8, 9, 11, 13, 17, 19) or not to.legal(op, a, stack, stack[-1][3] if stack else 0):
                continue
            step(op, a, b, f)
        raw, x22c, f, has = fields(rng)
        orig.gate_set(raw, x22c, f, has)
        probe.probe_ao_gate_set(raw, x22c, (U * 12)(*f), (I * 3)(*has))
        op = rng.choice([0, 0, 0, 1, 1, 2, 3, 3, 4])
        net = rng.choice(to.NETS + [0])
        ws = int(rng.random() < 0.7)
        a = orig.gate(op, net, ws)
        out = (U * 3)()
        probe.probe_ao_gate(op, net, ws, out)
        b = list(out)
        if a != b:
            raise SystemExit(f'seed {seed} check {k} gate {op:#x} net {net} state {ws}: exe {a}, port {b}\n'
                             f'  flags {raw[0xcc]:#x} x22c {x22c} has {has}')
        if orig.t.log != port.log:
            i = next((j for j, (x, y) in enumerate(zip(orig.t.log, port.log)) if x != y),
                     min(len(orig.t.log), len(port.log)))
            raise SystemExit(f'seed {seed} check {k} gate {op}: calls differ at {i}:\n'
                             f'  exe  {orig.t.log[i:i + 4]}\n  port {port.log[i:i + 4]}')
        stack = orig.stack()
        if stack != to.port_stack(probe):
            raise SystemExit(f'seed {seed} check {k} gate {op}: stacks differ')
        key = (op, a[0] & 0xff)
        stats[key] = stats.get(key, 0) + 1
    probe.probe_ao_done()
    return stats, len(orig.t.log)


def main():
    probe = setup_probe()
    cases = int(os.environ.get('GATE_CASES', '200'))
    total, calls = {}, 0
    for seed in range(cases):
        stats, n = run_case(probe, seed, 100)
        calls += n
        for k, v in stats.items():
            total[k] = total.get(k, 0) + v
    need = [(0, 0), (0, 1), (1, 0), (1, 1), (2, 0), (2, 1), (3, 0), (3, 1), (4, 0)]
    missing = [k for k in need if k not in total]
    if missing:
        raise SystemExit(f'answers never seen: {missing}')
    never = [COVER[a] for a in COVER if a not in REACHED]
    if never:
        raise SystemExit(f'branches never reached: {never}')
    print(f'ok: {cases} cases, {sum(total.values())} checks, {calls} calls identical; '
          + ', '.join(f'gate{op} {r}: {v}' for (op, r), v in sorted(total.items())) + f'; {len(COVER)} branches reached')


if __name__ == '__main__':
    code = 0
    try:
        main()
    except SystemExit as e:
        print(e)
        code = 1
    sys.stdout.flush()
    os._exit(code)
