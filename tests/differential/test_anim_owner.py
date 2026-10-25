"""The animation-owner registry against the original x86 (docs/ai.md section
18): the claim 0x4d4f20 (with 0x4d46f0, 0x45b800, 0x508f00 / 0x508f20 /
0x508f30, run as they are), the release 0x4d5100 -> 0x4d47e0, the callback
0x4d4b10 -> 0x4d4a20 (with 0x4d4980, viaNode), the mission's end 0x4d4700
and the world's 0x4d4740.

    cmake --build build && .venv/bin/python tests/differential/test_anim_owner.py [cases]

Each case builds the registry (its constructor 0x4d4840, the listener
0x4e78d0 stubbed), six animations (no callback, ours with a record no list
holds or none, another's), owners (none, two mechs, a turret, a target: a
vt+0x74 each), three valves and the player's vehicle, then runs 40 random
operations: claims, releases, messages (every value 0..0x3f across the
cases, on the animation's record or none), a listed record's valve set, the
mission's end, and last perhaps the world's free (the animations go next). Compared after each: the traces, every
animation's callback, record (by list place), owner and valve, and the list.
With no viaNode the exe places the vehicle at its stack's leftovers (FIX
(bug): the port places nothing); those cases check the original's trace up
to the departure.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import Emu, Trace, compare, load_probe  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402

U32 = ctypes.c_uint32
I32 = ctypes.c_int32
REG, HEAD, OURS, PILOT = 0x60b518, 0x60b520, 0x4d4b10, 0x60861c
OWNERS = ['mech', 'mech2', 'turret', 'target']

# The branches the cases must reach (address: what).
COVER = {
    0x4d4f92: 'claim: no callback', 0x4d4fa3: 'claim: another\'s callback', 0x4d4ffc: 'claim: ours, no record',
    0x4d4fe9: 'claim: ours, a record no list holds', 0x4d4ff3: 'claim: ours, a listed record',
    0x4d5018: 'claim: taken', 0x4d504f: 'claim: a new record', 0x4d50a3: 'claim: the record kept',
    0x4d4804: 'release: a record let go', 0x4d4b14: 'callback: a record', 0x4d4a33: 'handler: the owner told',
    0x4d4a4e: 'handler: 0x11', 0x4d4a6d: 'handler: a valve off', 0x4d4a81: 'handler: a valve on',
    0x4d4a8e: 'handler: the inout tests', 0x4d49c6: 'viaNode: relative', 0x4d4718: 'end: a record emptied',
    0x4d4763: 'free: a record deleted', 0x4d479a: 'free: a node unlinked',
}
REACHED = set()
MSGS = set()
COUNT = {'claims taken': 0, 'messages': 0, 'calls': 0, 'no viaNode': 0}


def die():
    sys.stdout.flush()
    os._exit(1)


def f2u(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


def setup_probe(probe):
    probe.probe_aown_poke.argtypes = [U32, U32]
    probe.probe_aown_name.argtypes = [U32, ctypes.c_char_p]
    probe.probe_aown_anim.argtypes = [U32, I32, I32, U32, U32]
    probe.probe_aown_claim.argtypes = [U32, U32]
    probe.probe_aown_claim.restype = I32
    probe.probe_aown_release.argtypes = [U32]
    probe.probe_aown_message.argtypes = [U32, I32, I32]
    probe.probe_aown_set_valve.argtypes = [I32, U32]
    probe.probe_aown_state.argtypes = [U32, ctypes.POINTER(I32)]
    probe.probe_aown_list.argtypes = [ctypes.POINTER(U32), I32]
    probe.probe_aown_list.restype = I32


class Case:
    """One case's world, in the emulator and mirrored for the probe."""

    def __init__(self, seed):
        self.r = r = random.Random(seed)
        self.t = Trace()
        e = self.e = Emu(self.t)
        e.cpu.fpu_control(0x007f)
        e.heap_stubs()
        e.stub(0x4e78d0, '@listener', 'c')
        e.call(0x4d4840)
        self.pokes = []
        self.names = {}
        self.owners = [0]
        for n in OWNERS:
            o = e.object(n, slots={0x74: ('s0,s1', 8)})
            self.names[o] = n
            self.owners.append(o)
        self.valves = [e.world_alloc(0x10) for _ in range(3)]
        e.stub(0x4d3c00, '0x004d3c00', 'c,s0')
        e.stub(0x4c3bd0, '0x004c3bd0', 'c,d')
        # viaNode's calls: the node's floats at +0x10 / +0x20, the vehicle's place at +0x30.
        veh = e.object('player', slots={0xdc: ('c,s0,s1', 8, self.place)})
        for i in range(3):
            self.put(veh + 0x30 + 4 * i, f2u(r.uniform(-500, 500)))
        pilot = e.world_alloc(0x20)
        self.put(PILOT, pilot)
        self.put(pilot + 0x14, veh)
        node = e.world_alloc(0x40)
        for i in range(6):
            self.put(node + 0x10 + 4 * i, f2u(r.uniform(-50, 50)))
        self.node = node if r.random() < 0.75 else 0
        self.t.script['0x004f7fe0'] = self.node
        e.stub(0x4f7fe0, '0x004f7fe0', 'c,zd')
        e.stub(0x4fc020, None, 'c,d,s0,s1', None, self.floats('0x004fc020', 0x10))
        e.stub(0x4fbe90, None, 'c,d,s0,s1', None, self.floats('0x004fbe90', 0x20))

        def place_of(emu, key, a, pop):
            emu.t.handle(key, (a[0],))
            emu._ret(a[0] + 0x30, pop)
        e.stub(0x5a85e0, '0x005a85e0', 'c', None, place_of)
        hook_cover(e)
        # The animations.
        self.anims = []
        self.setup = []
        for i in range(6):
            a = e.world_alloc(0x140)
            kind = r.choice([0, 0, 1, 1, 2])
            has, owner, valve = r.random() < 0.6, r.choice(self.owners), r.choice([0] + self.valves)
            if kind == 1:
                e.w32(a + 0x6c, OURS)
                if has:
                    rec = e.world_alloc(8)
                    e.w32(rec, owner)
                    e.w32(rec + 4, valve)
                    e.w32(a + 0x70, rec)
            elif kind == 2:
                e.w32(a + 0x6c, 0x4d4a20)
                e.w32(a + 0x70, e.world_alloc(8))
            self.anims.append(a)
            self.setup.append((a, kind, 1 if has else 0, owner, valve))

    def put(self, a, v):
        self.e.w32(a, v)
        self.pokes.append((a, v))

    def floats(self, key, off):
        def fn(emu, k, a, pop):
            emu.t.handle(key, (a[0],))
            n = a[0]
            if n:
                for i, p in enumerate((a[1], a[2], a[3])):
                    emu.w32(p, emu.r32(n + off + 4 * i))
            emu._ret(0, 8)
        return fn

    def place(self, emu, key, a, pop):
        v, p, yaw = a
        emu.t.handle(key, (v, emu.r32(p), emu.r32(p + 4), emu.r32(p + 8), yaw))
        emu._ret(0, 8)

    # The original's state, as the probe gives it.
    def records(self):
        e = self.e
        head = e.r32(HEAD)
        out, n = [], e.r32(head)
        while n != head:
            out.append(e.r32(n + 8))
            n = e.r32(n)
        return out

    def state(self):
        e = self.e
        recs = self.records()
        st = []
        for a in self.anims:
            cb, rec = e.r32(a + 0x6c), e.r32(a + 0x70)
            kind = 0 if cb == 0 else 1 if cb == OURS else 2
            if kind != 1:
                st.append((0, -1, 0, 0) if kind == 0 else (2, -4, 0, 0))
                continue
            stray = rec >= e.world_base
            i = recs.index(rec) if rec in recs else -1
            alive = rec and (i >= 0 or stray)
            st.append((1, -1 if not rec else i if i >= 0 else -2 if alive else -3,
                       e.r32(rec) if alive else 0, e.r32(rec + 4) if alive else 0))
        lst = [(e.r32(x), e.r32(x + 4)) for x in recs]
        return st, lst


def hook_cover(e):
    def hit(uc, address, size, user):
        REACHED.add(address)
    for a in COVER:
        e.uc.hook_add(UC_HOOK_CODE, hit, begin=a, end=a)


def probe_state(probe, case):
    st = []
    for i, a in enumerate(case.anims):
        out = (I32 * 4)()
        probe.probe_aown_state(a, out)
        kind = out[0]
        st.append((kind, out[1], out[2] & 0xffffffff, out[3] & 0xffffffff) if kind == 1 else
                  (0, -1, 0, 0) if kind == 0 else (2, -4, 0, 0))
    buf = (U32 * 512)()
    n = probe.probe_aown_list(buf, 256)
    return st, [(buf[2 * i], buf[2 * i + 1]) for i in range(n)]


def ops(case):
    r = case.r
    freed = False
    for _ in range(40):
        x = r.random()
        if x < 0.38:
            yield ('claim', r.choice(case.owners), r.choice(case.anims))
        elif x < 0.50:
            yield ('release', r.choice(case.owners))
        elif x < 0.85:
            if r.random() < 0.5:
                msg = r.randrange(0x40)
            else:
                msg = r.choice([0, 0x11, 0x29, 0x2a, 0x2b, 0x30, 0x31, 0x32, 0x38, 0x39, 0x3c, 0x3d])
            yield ('message', r.choice(case.anims), 1 if r.random() < 0.1 else 0, msg)
        elif x < 0.95:
            yield ('valve', r.randrange(6), r.choice(case.valves))
        elif x < 0.98:
            yield ('end',)
        elif not freed:
            freed = True
            yield ('free',)


def run_case(probe, seed):
    case = Case(seed)
    e = case.e
    t = Trace(dict(case.t.script))
    probe.probe_app_set_callback(t.callback())
    probe.probe_aown_reset()
    for a, v in case.pokes:
        probe.probe_aown_poke(a, v)
    for o, n in case.names.items():
        probe.probe_aown_name(o, n.encode())
    for s in case.setup:
        probe.probe_aown_anim(*s)
    for op in ops(case):
        case.t.reset()
        t.reset()
        name = f'seed {seed} {op[0]} {op[1:]}'
        depart = False
        if op[0] == 'claim':
            ra = e.call(0x4d4f20, ecx=op[1], edx=op[2]) & 0xff
            rb = probe.probe_aown_claim(op[1], op[2])
            COUNT['claims taken'] += ra
            if ra != rb:
                print(f'{name}: original {ra}, rebuild {rb}')
                die()
        elif op[0] == 'release':
            e.call(0x4d5100, ecx=op[1])
            probe.probe_aown_release(op[1])
        elif op[0] == 'message':
            a, null, msg = op[1:]
            rec = 0 if null else e.r32(a + 0x70)
            if e.r32(a + 0x6c) != OURS and not null:
                continue  # the exe calls the callback only when it is ours
            MSGS.add(msg)
            COUNT['messages'] += 1
            e.call(OURS, ecx=a, edx=rec, stack=(msg,))
            probe.probe_aown_message(a, null, msg)
            depart = msg == 0x11 and rec and not case.node
        elif op[0] == 'valve':
            recs = case.records()
            if op[1] >= len(recs):
                continue
            e.w32(recs[op[1]] + 4, op[2])
            probe.probe_aown_set_valve(op[1], op[2])
        elif op[0] == 'end':
            e.call(0x4d4700, ecx=REG)
            probe.probe_aown_end()
        else:
            e.call(0x4d4740, ecx=REG)
            probe.probe_aown_free()
        a_log = case.t.log
        COUNT['calls'] += len(a_log)
        if depart:
            COUNT['no viaNode'] += 1
            # FIX (bug): the port stops at the missing node.
            cut = next(i for i, c in enumerate(a_log) if c[0] == '0x004fc020')
            assert a_log[cut][1] == (0,) and a_log[-1][0] == 'player.vt+0xdc', a_log
            a_log = a_log[:cut]
        if not compare(name, a_log, t.log):
            die()
        sa, sb = case.state(), probe_state(probe, case)
        if sa != sb:
            print(f'{name}: state differs\n  original {sa}\n  rebuild  {sb}')
            die()
        if op[0] == 'free':
            break  # the animations go next (0x0050b380)
    return len(case.t.log)


def main():
    probe = load_probe()
    setup_probe(probe)
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 400
    for seed in range(count):
        run_case(probe, seed)
    missing = [v for k, v in COVER.items() if k not in REACHED]
    if missing:
        print('never reached: ' + ', '.join(missing))
        die()
    unsent = sorted(set(range(0x40)) - MSGS)
    if unsent:
        print('messages never sent: ' + ', '.join(f'{m:#x}' for m in unsent))
        die()
    print(f'ok: {count} cases ({", ".join(f"{v} {k}" for k, v in COUNT.items())}), every message 0..0x3f, '
          f'{len(COVER)} branches, as the original')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()
