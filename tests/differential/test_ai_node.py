"""The node state (docs/ai.md 17.4a) against the original x86: its maker
0x4c9150 and constructor 0x4ced40 (the context 0x4cecf0, steering's set
0x4cea30, the Mech's vt+0x118 0x41d1c0 and vt+0x120 0x41d210), its run
0x4cf140 with the walk 0x4cee80 / 0x4cee50, the turns 0x4cdd40 / 0x4cdee0,
the link pick 0x4ccf20 and the links' weights (0x4cd430, 0x4cd470, 0x4ca210,
0x4cd580 with 0x4c79f0 / 0x4c7960), its resume 0x4c9700; the speed and
valve steps' states (0x4c9f40, 0x4ca2e0); and the turn settings 0x4c7900.

    cmake --build build && .venv/bin/python tests/differential/test_ai_node.py

The brain, its order stack, its vehicle, contacts and steering are
test_ai_steer.py's. Added: the vehicle's vtable moved (its old place lies on
the Mech's +0x518) with the real vt+0x118 / +0x120 / +0x50 over random
+0x518 / +0x534, a flat forward at +0x24, a name at +0x04 and jump jets at
+0x3d8; the IControls' vt+0x20 / +0x28 and vt+0x30 (an `fld` thunk answered
from the trace); the mission boundary 0x4c3000 and the brain's way finder
0x4c7cb0 answered from the trace (a clamp halves the point's x and z on
both sides); the weights' scratch vector [0x60b444] and weight [0x60b44c];
the port's turn settings at [0x60b478]. Each case is a net of 3 to 6 real
nodes (some within 1 m of another or on it) linked to each other, to the
null step and through chains of real speed, valve, jump and gate steps (the
jump and gate steps' states fakes). The pilot is set on a node and walked:
runs, resumes, steering updates, the vehicle moved toward the carrot, the
clock run on, the valves, gate name and jump jets changed, the speed and
valve states run when the pick hands them the order; the link pick is also
asked alone at any node and heading. After each call the calls (the
position, the controls, rand, the boundary, the way finder, the states made
and destroyed), the order's state (a node state's node, phase and
+0x10..+0x24, another real state's step, or a fake's number), the steering
block and the brain's +0x298, +0xec and +0x104 must match (a pick alone:
the step picked and the running sums the exe leaves in [0x60b444]), at
24-bit x87. The turns are also run alone on random vectors.
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
import test_ai_orders as to  # noqa: E402
import test_ai_steer as ts  # noqa: E402
from calltrace import Trace  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EDX, UC_X86_REG_ESP  # noqa: E402

U, I = ctypes.c_uint32, ctypes.c_int32
OWNER = ts.OWNER
SET_BEHAVIOUR, TURN, TURN_BACK, TURN_INIT, PICK = 0x4e8120, 0x4cdd40, 0x4cdee0, 0x4c7900, 0x4ccf20
NODE_VT, NODE_STATE_VT, NULL_STEP = 0x5cc6f0, 0x5cc5c0, 0x603fa0
# The node kinds (0 node, 5 backup, 6 deploy) and their states' vtables.
NODE_VTS = {0: NODE_VT, 5: 0x5cc7a8, 6: 0x5cc898, 11: 0x5cbf30}  # node, backup, deploy, mcbwait
NODE_STATES = (NODE_STATE_VT, 0x5cc860, 0x5cc950)
BACKUP_RUN, DEPLOY_STATE_VT = 0x4cfad0, 0x5cc950
# The speed, valve, scan and two wait steps' states (0x4c9f40, 0x4ca2e0, 0x4cbca0, 0x4ca070, 0x4ca190).
# Then the mcbwait, sound and gate states (0x4ccda0, 0x4ccec0, 0x4cd690).
PASS_VT = (0x5cb540, 0x5cb7c8, 0x5cbc88, 0x5cb618, 0x5cb6f0, 0x5cbfe8, 0x5cc0c0, 0x5cc198)
WAIT_VT = (0x5cb618, 0x5cb6f0)
SCAN_VT = (0x5cbbd0, 0x5cbca8, 0x5cbd60)  # scan, canleave, canleave-not
SCAN_FN = ((0x4d9c30, 0x4d9c20), (0x4d9c50, 0x4d9c40), (0x4d9c70, 0x4d9c60))  # aim, hit, time: off, on
CANLEAVE_FN = {1: 0x4d9c80, 2: 0x4d9ca0}
STEP_VT = {1: 0x5cb488, 2: 0x5cb710, 3: 0x5cbaf8, 4: 0x5cc0e0, 8: 0x5cb560, 9: 0x5cb638, 10: 0x5cc008}  # speed, valve, jump, gate, the waits, sound
SOUND_BASE = 0x7000  # a sound step's fake sound: this plus its element's index
f2i, fi = tf.f2i, ts.fi
COVER = {
    0x4cf1bf: 'at the node: the pick', 0x4cf200: 'no link: stand', 0x4cf265: 'waiting', 0x4cf275: 'linked on',
    0x4cf2a9: 'walking', 0x4ceedf: 'the boundary clamp', 0x4cef44: 'near: along the segment',
    0x4cef70: 'near, behind: +dir', 0x4cef85: 'toward the carrot', 0x4cefd5: 'backing up', 0x4cf00c: 'forward',
    0x4cf118: 'no throttle', 0x4cf09a: 'along within 0.04', 0x4cf0eb: 'throttle at its top',
    0x4cf10d: 'throttle at its bottom', 0x4cdda0: 'behind: keep', 0x4cdec5: 'dead ahead', 0x4cde10: 'behind, to a side',
    0x4cde2e: 'turn one way', 0x4cde84: 'turn the other', 0x4cdeb7: 'the least turn', 0x4cdf36: 'facing away',
    0x4ce092: 'not facing away', 0x4cd02d: 'one link', 0x4cd086: 'the one link taken', 0x4cd0f4: 'a link weighed',
    0x4cd176: 'one at random', 0x4ca230: 'a valve open', 0x4cd5a4: 'a gate or jump passed',
    0x4cd5b9: 'a gate or jump refused', 0x4c7a20: 'a near link: the facing', 0x4c7a53: 'no facing',
    0x4c9700: 'resume', 0x4c9f40: 'a speed step run', 0x4ca2e0: 'a valve step run', 0x4ca2fd: 'a valve step: what follows',
    0x4cfb84: 'backup: turning its back', 0x4cfbf5: 'backup: at the node', 0x4cfc1b: 'backup: backing up',
    0x4cfd1d: 'backup: turning round', 0x4cfd69: 'backup: on', 0x4cfd59: 'backup: waiting',
    0x4d00b4: 'deploy: the fine settings', 0x4d017b: 'deploy: walking', 0x4d0157: 'deploy: at the node',
    0x4d0218: 'deploy: closing in', 0x4d02af: 'deploy: not settled', 0x4d02f7: 'deploy: settled',
    0x4d03c4: 'deploy: turning', 0x4d040c: 'deploy: faced', 0x4d045d: 'deploy: sliding', 0x4d0529: 'deploy: placed',
    0x4d062d: 'deploy: holding', 0x4d06ab: 'deploy: its valve set', 0x4d06fb: 'deploy: holding on',
    0x4d0766: 'deploy: on', 0x4d0864: 'deploy: waiting', 0x4cbcb9: 'a scan step: its tests',
    0x4cbcc7: 'a scan step: on', 0x4d9c20: 'scan aim on', 0x4d9c30: 'scan aim off', 0x4d9c40: 'scan hit on',
    0x4d9c50: 'scan hit off', 0x4d9c60: 'scan time on', 0x4d9c70: 'scan time off', 0x4d9c80: 'canleave',
    0x4d9ca0: 'canleave not', 0x4c9fc0: 'a wait made', 0x4ca070: 'a wait_interval run',
    0x4ca08a: 'a wait_interval over', 0x4ca096: 'a wait_interval still on', 0x4ca190: 'a wait_time run',
    0x4ca1a3: 'a wait_time over', 0x4ca1af: 'a wait_time still on', 0x4ca0e0: 'a wait_time made',
    0x4ccd00: 'an mcbwait made', 0x4ccdb9: 'an mcbwait: its time past', 0x4ccdd9: 'an mcbwait: its valve tested',
    0x4ccdf0: 'an mcbwait: its valve holds', 0x4cce13: 'an mcbwait: still waiting or on', 0x4cce17: 'an mcbwait: on',
    0x4ccec0: 'a sound step run', 0x4cd690: 'a gate step run'}
REACHED = set()


def script(seed):
    s = ts.tf.script(seed)
    s['0x004c3000'] = lambda n, args: random.Random(f'{seed}:inout:{n}').choice([0, 0, 0, 0, 1])
    s['0x004c7cb0'] = lambda n, args: random.Random(f'{seed}:path:{n}').choice([0] * 9 + [1, 2, 3, 4, 5, 9])
    s['ictl.vt+0x30'] = lambda n, args: f2i(random.Random(f'{seed}:tt:{n}').choice(
        [0.0, 0.0, 0.5, -0.7, 1.0, random.Random(f'{seed}:tt2:{n}').uniform(-1, 1)]))
    return s


class Original(ts.Original):
    def __init__(self, seed):
        super().__init__(seed)
        e, b = self.e, self.brain
        self.t.script = script(seed)
        # The vehicle's vtable moved off the Mech's +0x518 / +0x534.
        nvt = e.world_alloc(0x200)
        e.cpu.write(nvt, bytes(e.cpu.read(OWNER + 0x400, 0x200)))
        e.w32(OWNER, nvt)
        e.w32(nvt + 0x118, 0x41d1c0)
        e.w32(nvt + 0x120, 0x41d210)
        e.w32(nvt + 0x50, 0x4d2a70)
        self.names = {1: e.world_alloc(8), 0: e.world_alloc(8)}
        e.cpu.write(self.names[1], b'g1\0')
        e.cpu.write(self.names[0], b'g2\0')
        self.gate_name = e.world_alloc(8)
        e.cpu.write(self.gate_name, b'g1\0')
        # The IControls' throttle and turn, and the turn target (fld [val]; ret).
        ivt = e.r32(OWNER + 0xc30)
        e.w32(ivt + 0x20, e.new_stub('ictl.vt+0x20', 's0', 4, self.logged))
        e.w32(ivt + 0x28, e.new_stub('ictl.vt+0x28', 's0', 4, self.logged))
        self.tt_val = OWNER + 0xe40
        thunk = OWNER + 0xe80
        e.cpu.write(thunk, b'\xd9\x05' + struct.pack('<I', self.tt_val) + b'\xc3')
        e.w32(ivt + 0x30, thunk)
        e.uc.hook_add(UC_HOOK_CODE, self.tt_hit, begin=thunk, end=thunk)
        e.stub(0x4c3000, '0x004c3000', 'c', 4, self.inout)
        e.stub(0x4c7cb0, '0x004c7cb0', 's0', 4, self.path_next)  # the brain's way finder (+0x238)
        # The weights' scratch vector (reserved, empty) and the weight.
        vec, buf = e.world_alloc(0x10), e.world_alloc(0x40)
        e.cpu.write(vec, struct.pack('<4I', 0, buf, buf, buf + 0x40))
        e.w32(0x60b444, vec)
        e.w32(0x60b44c, 0xfffff)
        e.w32(NULL_STEP, 0x5bc8d0)
        # The jump step's state: a fake, numbered as the port's (the others'
        # are real); a sound step's sound played at a place (0x56e7e0).
        e.w32(STEP_VT[3] + 0x8c, e.new_stub('beh.vt+0x8c', 's0,s1', 8, self.step_state))
        e.stub(0x56e7e0, '0x0056e7e0', 'c,s0', 8, self.sound_at)
        # Three valves, the third "mcbdeploy" ([0x60b4a8], held at [0x60b4ac] 1):
        # their value holders the real 0x5cd558 (vt+0x04 its +0x04); the set
        # 0x4d3c00 stores the value.
        self.valves = []
        for _ in range(3):
            v, holder = e.world_alloc(0x10), e.world_alloc(0x10)
            e.w32(v + 4, holder)
            e.w32(holder, 0x5cd558)
            self.valves.append(v)
        e.w32(0x60b4a8, self.valves[2])
        e.w32(0x60b4ac, 1)
        e.w32(0x60b490, 0)  # the deploy run's fine turn settings made on its first call
        e.stub(0x4d3c00, '@0x004d3c00', 's0', 4, self.valve_set)
        # The vehicle's placement (vt+0xdc: the point and the yaw), logged.
        e.w32(nvt + 0xdc, e.new_stub('vehicle.vt+0xdc', 's0,s1', 8, self.place))
        # The backup run's aim, a stack vector it never sets: the port's FIX
        # (the segment's way back to the node) put there on entry.
        e.uc.hook_add(UC_HOOK_CODE, self.backup_aim, begin=BACKUP_RUN, end=BACKUP_RUN)
        self.finder = e.world_alloc(0x40)
        self.res = OWNER + 0xf00
        self.fstp = OWNER + 0xec0
        e.cpu.write(self.fstp, b'\xd9\x1d' + struct.pack('<I', self.res) + b'\xc3')  # fstp dword [res]; ret
        self.elems, self.elem_id = [], {}
        for addr in COVER:
            e.uc.hook_add(UC_HOOK_CODE, lambda uc, ad, sz, u: REACHED.add(ad), begin=addr, end=addr)

    def valve_set(self, e, key, a, pop):
        holder = e.r32(e.uc.reg_read(to.UC_X86_REG_ECX) + 4)
        old = e.r32(holder + 4)
        e.w32(holder + 4, a[0])
        e._ret(old, pop)

    def place(self, e, key, a, pop):
        self.t.handle(key, [OWNER] + [e.r32(a[0] + 4 * k) for k in range(3)] + [a[1]])
        e._ret(0, pop)

    def backup_aim(self, uc, address, size, user):
        e, s = self.e, self.brain + 0x250
        sp = uc.reg_read(UC_X86_REG_ESP)
        e.cpu.write(sp - 0x2c, struct.pack('<3I', e.r32(s + 0xc) ^ 0x80000000, 0, e.r32(s + 0x10) ^ 0x80000000))

    def tt_hit(self, uc, address, size, user):
        self.e.w32(self.tt_val, self.t.handle('ictl.vt+0x30', []))

    def inout(self, e, key, a, pop):
        r = self.t.handle(key, a)
        if r & 0xff:
            p = e.r32(e.uc.reg_read(UC_X86_REG_ESP) + 4)  # the point, its one stack argument
            for k in (0, 8):
                x = struct.unpack('<f', e.cpu.read(p + k, 4))[0]
                e.cpu.write(p + k, struct.pack('<f', x * 0.5))
        e._ret(r, pop)

    def path_next(self, e, key, a, pop):
        r = self.t.handle(key, [self.elem_id.get(a[0], -1) & 0xffffffff])
        e._ret(self.elems[r - 1] if 1 <= r <= len(self.elems) else 0, pop)

    def pick(self, which, heading, n):
        e = self.e
        hp = self.res + 0x40
        e.cpu.write(hp, struct.pack('<3I', *heading))
        r = e.call(PICK, ecx=self.elems[which], stack=(self.brain, hp))
        buf = e.r32(e.r32(0x60b444) + 4)
        out = [-2 & 0xffffffff if r == NULL_STEP else self.elem_id.get(r, 0 if r == 0 else -1) & 0xffffffff, n]
        return out + [e.r32(buf + 4 * k) for k in range(min(n, 8))]

    def sound_at(self, e, key, a, pop):
        pos = struct.unpack('<3I', bytes(e.cpu.read(e.uc.reg_read(UC_X86_REG_EDX), 12)))
        self.t.handle(key, [a[0], *pos, a[1]])
        e._ret(0, pop)

    def step_state(self, e, key, a, pop):
        place, order = a[0], a[1]
        beh = e.uc.reg_read(to.UC_X86_REG_ECX)
        self.serial += 1
        e.cpu.write(place, struct.pack('<IIII', self.state_vt, order, beh, self.serial))
        self.t.handle(key, [self.elem_id[beh], self.serial])
        e._ret(place, pop)

    def net(self, rows):
        e = self.e
        self.elems = []
        for r in rows:
            size = 0x40 if r[0] in NODE_VTS else 0x20
            self.elems.append(e.world_alloc(size))
        at = lambda k: NULL_STEP if k == -2 else self.elems[k]  # noqa: E731
        self.elem_id = {a: k + 1 for k, a in enumerate(self.elems)}
        for r, a in zip(rows, self.elems):
            if r[0] in NODE_VTS:
                links = [at(k) for k in r[4:8] if k != -1]
                arr = e.world_alloc(0x10)
                for k, l in enumerate(links):
                    e.w32(arr + 4 * k, l)
                e.cpu.write(a, struct.pack('<4I', NODE_VTS[r[0]], 0, 0, 0) + struct.pack('<3I', *r[1:4]) +
                            struct.pack('<4I', 0, arr, arr + 4 * len(links), arr + 0x10))
                if r[0] == 11:  # the mcbwait node's valve, value and time
                    e.cpu.write(a + 0x2c, struct.pack('<3I', self.valves[r[8] & 1], r[9], r[10]))
                if r[0] == 6:  # the deploy node's valves, values and target
                    e.cpu.write(a + 0x2c, struct.pack('<5I', self.valves[r[8] & 1], r[9], self.valves[r[10] & 1], r[11],
                                                      at(r[12])))
            elif r[0] == 7:  # a scan or canleave step: its tests (function, 0) at +0x14..+0x18
                sub = r[1] % 3
                fns = [CANLEAVE_FN[sub]] if sub else \
                    [SCAN_FN[k][r[2] >> (2 * k + 1) & 1] for k in range(3) if r[2] >> (2 * k) & 1]
                arr = e.world_alloc(0x20)
                for k, fn in enumerate(fns):
                    e.cpu.write(arr + 8 * k, struct.pack('<2I', fn, 0))
                e.cpu.write(a, struct.pack('<4I', SCAN_VT[sub], 0, 0, at(r[4])) +
                            struct.pack('<4I', 3, arr, arr + 8 * len(fns), arr + 0x18))
            else:
                e.cpu.write(a, struct.pack('<4I', STEP_VT[r[0]], 0, 0, at(r[4])))
                if r[0] in (1, 3, 8, 9):
                    e.w32(a + 0x10, r[1])
                elif r[0] == 10:
                    e.w32(a + 0x10, SOUND_BASE + self.elems.index(a))
                elif r[0] == 2:
                    e.w32(a + 0x10, self.valves[r[1] & 1])
                    e.w32(a + 0x14, r[2])
                else:
                    e.w32(a + 0x10, self.gate_name)

    def node_world(self, w):
        e, b = self.e, self.brain
        e.cpu.write(OWNER + 0x24, struct.pack('<3I', *w[0:3]))
        e.w32(b + 0xec, w[3])
        e.w32(OWNER + 0x518, w[4])
        e.w32(OWNER + 0x534, w[5])
        e.w32(0x60991c, w[6])
        e.w32(OWNER + 4, self.names[w[7]])
        e.w32(OWNER + 0x3d8, w[8])
        for v, x in zip(self.valves, w[9:11]):
            e.w32(e.r32(v + 4) + 4, x)
        e.cpu.write(self.pos, struct.pack('<3I', *w[11:14]))
        e.cpu.write(b + 0x298, struct.pack('<3I', *w[14:17]))
        e.w32(e.r32(self.valves[2] + 4) + 4, w[17])
        e.w32(OWNER + 0x40, w[18])
        e.w32(b + 0x238, self.finder if w[19] else 0)
        for k, off in enumerate((0x98, 0x9c, 0xa0, 0x3c)):
            e.w32(b + off, w[20 + k])
        e.w32(b + 0xdc, w[24])  # a way finder (its answers 0x4c7cb0's, from the trace)

    def node(self, op, which, dt):
        e, b = self.e, self.brain
        cur = self.orders()[-1]
        st = e.r32(cur + 0x10)
        if op == 0:
            e.call(SET_BEHAVIOUR, ecx=cur, stack=(self.elems[which],))
        elif e.r32(st) in NODE_STATES:
            e.call(e.r32(e.r32(st) + (0x14 if op == 1 else 0x10)), ecx=st, stack=(dt,) if op == 1 else ())
        elif op == 1 and e.r32(st) in PASS_VT:
            e.call(e.r32(e.r32(st) + 0x14), ecx=st, stack=(dt,))
        out = [0] * 46
        cur = self.orders()[-1]
        st = e.r32(cur + 0x10)
        vt = e.r32(st)
        if vt in NODE_STATES:
            out[0] = self.elem_id.get(e.r32(st + 8), -1) & 0xffffffff
            out[1:8] = [e.r32(st + 0xc + 4 * k) for k in range(7)]
        elif vt == self.state_vt:
            out[0] = -e.r32(st + 0xc) & 0xffffffff
        elif vt in PASS_VT or vt == 0x5cb258:  # a speed or valve step's state, the null step's
            beh = e.r32(st + 8) if vt != 0x5cb258 else NULL_STEP
            out[0] = 0x10000 | ((-2 if beh == NULL_STEP else self.elem_id.get(beh, -1)) & 0xffff)
        out[8:26] = [e.r32(b + 0x250 + 4 * k) for k in range(18)]
        out[26:29] = [e.r32(b + 0x298 + 4 * k) for k in range(3)]
        out[29], out[30] = e.r32(b + 0xec), e.cpu.read(b + 0x104, 1)[0]
        e.cpu.write(b + 0x104, b'\0')
        out[31] = e.r32(b + 0x80)
        if vt == DEPLOY_STATE_VT and out[1] >= 4:  # set on leaving phase 3
            out[32:35] = [e.r32(st + 0x2c + 4 * k) for k in range(3)]
        out[35:38] = [e.r32(e.r32(v + 4) + 4) for v in self.valves]
        out[40:45] = [e.r32(b + off) for off in (0x98, 0x9c, 0xa0, 0x3c, 0xdc)]
        if vt in WAIT_VT:
            out[45] = e.r32(st + 0xc)  # when it began
        return out

    def turn(self, op, f, params):
        e = self.e
        a = self.res + 0x10
        e.cpu.write(a, struct.pack('<6I', *f[:6]))
        pdot, pcross, prev = self.res + 4, self.res + 8, self.res + 0xc
        e.w32(prev, 0)
        if op == 0:
            e.call(TURN, ecx=a, edx=a + 12, stack=(pdot, pcross, f[6], params))
        else:
            e.call(TURN_BACK, ecx=a, edx=a + 12, stack=(pdot, pcross, f[6], prev, params))
        e.call(self.fstp)
        return [e.r32(self.res), e.r32(pdot), e.r32(pcross), e.cpu.read(prev, 1)[0]]


def setup_probe():
    probe = ts.setup_probe()
    probe.probe_ao_node_net.argtypes = [ctypes.c_void_p, I]
    probe.probe_ao_node_world.argtypes = [ctypes.c_void_p]
    probe.probe_ao_node.argtypes = [I, I, U, ctypes.c_void_p]
    probe.probe_ao_turn_params.argtypes = [ctypes.c_void_p]
    probe.probe_ao_turn.argtypes = [I, ctypes.c_void_p, ctypes.c_void_p]
    probe.probe_ao_pick.argtypes = [I, ctypes.c_void_p, ctypes.c_void_p]
    return probe


def build_net(rng):
    """Nodes first (plain, backup or deploy), then the steps their chains go
    through (13 words a row)."""
    n = rng.randrange(3, 7)
    nodes, near = [], {}
    for k in range(n):
        if k and rng.random() < 0.25:  # a node within 1 of another (or on it), each first in the other's links
            j = rng.randrange(k)
            p = list(nodes[j])
            d = rng.choice([1.0, 0.1, 0.0])
            p[0] += rng.uniform(-0.9, 0.9) * d
            p[2] += rng.uniform(-0.6, 0.6) * d
            near[k], near[j] = j, k
        else:
            p = [rng.uniform(-80, 80), rng.uniform(0, 8), rng.uniform(-80, 80)]
        nodes.append(p)
    rows = []
    for p in nodes:
        kind = rng.choice([0, 0, 0, 5, 6, 6, 11])
        if kind == 6:
            extra = [rng.randrange(2), rng.randrange(2), rng.randrange(2), rng.randrange(2), rng.randrange(n)]
        elif kind == 11:  # its valve, value, time (set against the clock later), two spare
            extra = [rng.randrange(2), rng.randrange(2), 0, 0, 0]
        else:
            extra = [0] * 5
        rows.append([kind] + [f2i(x) for x in p] + [-1, -1, -1, -1] + extra)
    for k in range(n):
        links = []
        for _ in range(rng.choice([0, 1, 1, 2, 2, 3, 4])):
            r = rng.random()
            if r < 0.08:
                links.append(-2)
                continue
            target = rng.randrange(n)
            if r < 0.55:
                links.append(target)
                continue
            first = len(rows)
            for depth in range(rng.choice([1, 1, 2])):
                kind = rng.choice([1, 2, 3, 4, 7, 7, 8, 9, 10])
                if kind == 8:  # wait_interval: seconds, mostly a clock step or two, now and then NaN
                    a = 0x7fc00000 if rng.random() < 0.04 else f2i(rng.uniform(1, 30) if rng.random() < 0.3 else
                                                             rng.choice([0.5, 2.0, 2.5, 4.0, 6.0]))
                elif kind == 9:  # wait_time: a game time, on the grid at times
                    a = f2i(rng.uniform(0, 520) if rng.random() < 0.3 else 0.5 * rng.randrange(0, 1040))
                else:
                    a = f2i(rng.uniform(1, 30)) if kind in (1, 3) else rng.randrange(3) if kind == 7 else rng.randrange(2)
                bb = rng.randrange(64) if kind == 7 else rng.choice([0, 1, 1])
                rows.append([kind, a, bb, 0, len(rows) + 1, -1, -1, -1, 0, 0, 0, 0, 0])
            rows[-1][4] = target
            links.append(first)
        if k in near:
            links = [near[k]] + links[:3]
        if rows[k][0] == 5 and rng.random() < 0.7:  # a backup dead end
            links = []
        rows[k][4:4 + len(links)] = links
    return rows, nodes


def world(rng, nodes):
    ang = rng.uniform(-math.pi, math.pi)
    fwd = [math.sin(ang), 0.0, math.cos(ang)] if rng.random() > 0.15 else [0.0, 0.0, 0.0]
    near = nodes[rng.randrange(len(nodes))]
    pos = [near[0] + rng.uniform(-40, 40), near[1], near[2] + rng.uniform(-40, 40)]
    last = nodes[rng.randrange(len(nodes))] if rng.random() < 0.7 else pos
    share = rng.choice([1.0, 1.0, 0.5, rng.uniform(0.2, 1.2)])
    decel = rng.choice([rng.uniform(0.3, 6), 2.0, 0.0 if rng.random() < 0.05 else 1.0])
    clock = rng.uniform(0, 500) if rng.random() < 0.3 else 0.5 * rng.randrange(0, 1000)  # mostly on the waits' grid
    w = [f2i(x) for x in fwd] + [f2i(share), f2i(rng.uniform(5, 50)), f2i(decel), f2i(clock)]
    w += [rng.randrange(2), rng.choice([0, 0, 2, 4]), rng.randrange(2), rng.randrange(2)]
    w += [f2i(x) for x in pos] + [f2i(x) for x in last]
    w += [rng.choice([0, 1, 1]), f2i(rng.choice([0.0, 0.05, -0.08, 0.15, 0.5, -2.0])), int(rng.random() < 0.2)]
    w += [rng.randrange(2) for _ in range(4)] + [f2i(rng.choice([3.4028235e38, 12.5, 0.0]))]
    return w


def run_case(probe, seed, calls, stats):
    rng = random.Random(seed)
    orig = Original(seed)
    port = Trace(script(seed))
    probe.probe_app_set_callback(port.callback())
    probe.probe_ao_new()
    probe.probe_ao_op(0, 1, 1, 0)
    orig.op(0, 1, 1, 0)
    orig.world([], [])
    orig.e.w32(orig.brain + 0x80, 1)  # the brain's own order runs: the port's default (aiv.zrd's 1)
    probe.probe_ao_steer_world((U * 1)(0), 0, (U * 1)(0), 0)
    params = (U * 4)()
    probe.probe_ao_turn_params(params)
    pa = orig.e.world_alloc(0x10)
    orig.e.cpu.write(pa, struct.pack('<4I', *params))
    orig.e.cpu.write(0x60b478, struct.pack('<4I', *params))
    rows, nodes = build_net(rng)
    w = world(rng, nodes)
    for r in rows:  # a wait_time mostly a clock step or two past the clock: met exactly, at times
        if r[0] == 9 and rng.random() < 0.7:
            r[1] = f2i(fi(w[6]) + rng.choice([0.0, 0.5, 2.0, 2.5, 4.0, 6.0]))  # one or two clock steps on
        if r[0] == 11:  # an mcbwait's time likewise, now and then NaN
            r[10] = 0x7fc00000 if rng.random() < 0.05 else f2i(fi(w[6]) + rng.choice([-1.0, 0.0, 0.5, 2.0, 6.0, 30.0]))
    orig.net(rows)
    flat = [x & 0xffffffff for r in rows for x in r]
    probe.probe_ao_node_net((U * len(flat))(*flat), len(rows))
    orig.node_world(w)
    probe.probe_ao_node_world((U * 25)(*w))
    node_rows = [k for k, r in enumerate(rows) if r[0] in NODE_VTS]
    out = (U * 46)()
    state, last = 0, None
    for k in range(calls):
        r = rng.random()
        start = len(orig.t.log)
        if state <= 0 or r < 0.06:
            op, which, dt = 0, rng.choice(node_rows), 0
        elif r < 0.5:
            op, which, dt = 1, 0, f2i(rng.choice([1 / 30, 1 / 60, 0.1]))
        elif r < 0.53:
            op, which, dt = 2, 0, 0
        elif r < 0.68:  # the link pick alone, at any node, any heading
            which = rng.choice(node_rows)
            ang = rng.uniform(-math.pi, math.pi)
            hd = [f2i(math.sin(ang)), 0, f2i(math.cos(ang))]
            o = (U * 10)()
            probe.probe_ao_pick(which, (U * 3)(*hd), o)
            a = orig.pick(which, hd, o[1])
            b = list(o)[:2 + min(o[1], 8)]
            if orig.t.log != port.log or a != b:
                raise SystemExit(f'seed {seed} call {k} pick at {which}: exe {a}, port {b}\n'
                                 f'  exe {orig.t.log[start:][-4:]}\n  port {port.log[start:][-4:]}')
            stats['picks'] = stats.get('picks', 0) + 1
            continue
        else:
            x = rng.random()
            if x < 0.35:  # steering's frame update
                fl = [f2i(rng.choice([1 / 30, 0.25, 1.0])), 0, 0, 0, 0, 0, 0]
                orig.call(1, fl)
                probe.probe_ao_steer(1, (U * 7)(*fl), (U * 21)())
            elif x < 0.62:  # the vehicle walked toward the carrot, the clock on
                cur = (U * 21)()
                probe.probe_ao_steer(3, (U * 7)(), cur)
                c = [fi(v) for v in list(cur)[:3]]
                p = [fi(v) for v in w[11:14]]
                t = rng.choice([0.3, 0.7, 1.0])
                w[11:14] = [f2i(a + (b - a) * t) if math.isfinite(b) else f2i(a) for a, b in zip(p, c)]
                w[6] = f2i(fi(w[6]) + rng.choice([0.5, 2.0, 6.0]))
                if rng.random() < 0.4:
                    ang = rng.uniform(-math.pi, math.pi)
                    w[0], w[2] = (f2i(math.sin(ang)), f2i(math.cos(ang))) if rng.random() > 0.15 else (0, 0)
                orig.node_world(w)
                probe.probe_ao_node_world((U * 25)(*w))
            elif x < 0.72:  # the steering block loaded: the carrot near the node
                blk = (U * 21)()
                probe.probe_ao_steer(2, (U * 7)(), blk)
                b = list(blk)[3:]
                b[7] = f2i(rng.choice([0.0, -1.0, fi(b[5]) - 0.5, rng.uniform(-2, 2)]))
                b[11] = f2i(rng.uniform(-3, 3))
                b[12] = f2i(rng.uniform(-3, 3))
                raw = struct.pack('<18I', *b)
                orig.e.cpu.write(orig.steer, raw)
                probe.probe_ao_steer_load(raw)
            else:
                if x < 0.8:  # valves, the gate's name, the jump jets, "mcbdeploy", settling
                    w[7], w[8], w[9], w[10] = rng.randrange(2), rng.choice([0, 2]), rng.randrange(2), rng.randrange(2)
                    w[17], w[18] = rng.choice([0, 1, 1]), f2i(rng.choice([0.0, 0.05, -0.15, 0.15, 0.5]))
                    w[19] = int(rng.random() < 0.25)
                elif x < 0.9:  # faced as the backup or deploy run wants it
                    if last and last[0] < 0x10000 and rows[last[0] - 1][0] == 6 and last[1] >= 4:
                        w[0], w[1], w[2] = last[32], 0, last[34]
                    else:
                        blk = (U * 21)()
                        probe.probe_ao_steer(2, (U * 7)(), blk)
                        sgn = rng.choice([0, 0x80000000])
                        w[0], w[1], w[2] = blk[6] ^ sgn, 0, blk[7] ^ sgn
                    w[18] = 0
                else:  # on the node (or near it)
                    if last and 0 < last[0] <= len(rows):
                        q = [fi(x) for x in rows[last[0] - 1][1:4]]
                        off = rng.choice([0.0, 0.01, 0.2, 0.7])
                        w[11:14] = [f2i(q[0] + off), f2i(q[1]), f2i(q[2] - off)]
                orig.node_world(w)
                probe.probe_ao_node_world((U * 25)(*w))
            continue
        a = orig.node(op, which, dt)
        probe.probe_ao_node(op, which, dt, out)
        b = list(out)
        if orig.t.log != port.log:
            i = next((j for j, (x, y) in enumerate(zip(orig.t.log, port.log)) if x != y),
                     min(len(orig.t.log), len(port.log)))
            raise SystemExit(f'seed {seed} call {k} op {op}: calls differ at {i}:\n  before {orig.t.log[start:i][-5:]}\n'
                             f'  exe  {orig.t.log[i:i + 4]}\n  port {port.log[i:i + 4]}')
        if a != b:
            names = ['state', 'phase', 'turn', 'x14', 'throttle', 'slow', 'inv_slow', 'retry'] + \
                [f'steer+{4 * j:#x}' for j in range(18)] + ['x298.x', 'x298.y', 'x298.z', 'xec', 'x104', 'x80'] + \
                ['face.x', 'face.y', 'face.z', 'valve1', 'valve2', 'mcbdeploy', '-', '-',
                 'scan_aim', 'scan_hit', 'scan_time', 'canleave', 'xdc']
            bad = [f'{n} {x:#x}/{fi(x)} vs {y:#x}/{fi(y)}' for n, x, y in zip(names, a, b) if x != y]
            raise SystemExit(f'seed {seed} call {k} op {op} which {which}: {bad}')
        state = a[0] if a[0] < 0x80000000 and a[0] != 0x1fffe else -1
        last = a
        stats[op] = stats.get(op, 0) + 1
        if op == 0 and a[0] == 0:
            raise SystemExit(f'seed {seed}: no node state made')
    for _ in range(40):  # the turns alone
        fwd = rng.choice([[rng.uniform(-1, 1), 0, rng.uniform(-1, 1)], [0, 0, 1], [0, 0, 0]])
        tgt = [rng.uniform(-1, 1), 0, rng.uniform(-1, 1)]
        if rng.random() < 0.3:  # nearly ahead or behind
            s = rng.choice([1, -1])
            tgt = [fwd[0] * s + rng.uniform(-0.1, 0.1), 0, fwd[2] * s + rng.uniform(-0.1, 0.1)]
        f = [f2i(x) for x in fwd + tgt] + [f2i(rng.choice([0.0, 0.5, -1.0, float('nan'), rng.uniform(-1, 1)]))]
        if rng.random() < 0.03:
            f[rng.randrange(6)] = f2i(float('nan'))
        for op in (0, 1):
            a = orig.turn(op, f, pa)
            o = (U * 4)()
            probe.probe_ao_turn(op, (U * 7)(*f), o)
            if a != list(o):
                raise SystemExit(f'seed {seed} turn {op} {[fi(x) for x in f]}: exe {[fi(x) for x in a[:3]]} {a[3]}, '
                                 f'port {[fi(x) for x in list(o)[:3]]} {o[3]}')
            stats['turns'] = stats.get('turns', 0) + 1
    probe.probe_ao_done()
    return orig, params


def main():
    probe = setup_probe()
    cases = int(os.environ.get('NODE_CASES', '300'))
    stats = {}
    orig = params = None
    for seed in range(cases):
        orig, params = run_case(probe, seed, 120, stats)
    # The turn settings: the exe's start-up 0x4c7900 against the port's.
    e = orig.e
    e.cpu.write(0x60b478, bytes(16))
    e.call(TURN_INIT)
    exe = list(struct.unpack('<4I', e.cpu.read(0x60b478, 16)))
    if exe != list(params):
        raise SystemExit(f'turn settings: exe {[fi(x) for x in exe]}, port {[fi(x) for x in params]}')
    never = [COVER[a] for a in COVER if a not in REACHED]
    if never:
        raise SystemExit(f'branches never reached: {never}')
    print(f'ok: {cases} cases, {stats.get(0, 0)} states made, {stats.get(1, 0)} runs, {stats.get(2, 0)} resumes, '
          f'{stats.get("picks", 0)} link picks, '
          f'{stats.get("turns", 0)} turns alone identical; the turn settings; {len(COVER)} branches reached')


if __name__ == '__main__':
    code = 0
    try:
        main()
    except SystemExit as e:
        print(e)
        code = 1
    sys.stdout.flush()
    os._exit(code)
