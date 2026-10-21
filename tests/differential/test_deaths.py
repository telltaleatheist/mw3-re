"""A mech's death's small pieces (c1m1 item 2) against the original x86
(docs/damage.md, The death): the parts' effect-end callback 0x461c90; the
canned effects 0x432830 (the eject seat, the jump jets' flames) and their
node copies freed by 0x431800; the death's preparation 0x41d070 (the eject,
the hip's sink, the sound, the player's loss); zEffect's node checks
0x506ae0 / 0x506b60. Outside calls stood in on both sides; the x87 at 24
bits.

    cmake --build build && .venv/bin/python tests/differential/test_deaths.py

BREAK=<n> breaks the port's inputs in a known way (see main); each must fail.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import FTOL, IAT_FTOL, ROOT, Original  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
P, I, U, F = ctypes.c_void_p, ctypes.c_int32, ctypes.c_uint32, ctypes.c_float
probe.probe_effect_ended.argtypes = [I, I, P, U, I, P, I]
probe.probe_canned_setup.argtypes = [P]
probe.probe_canned.argtypes = [I, U, P, P, P, U, P, I]
probe.probe_canned_free.argtypes = [P, I]
probe.probe_death_prep.argtypes = [I, U, U, I, P, U, U, P, I]
probe.probe_anim_nodes.argtypes = [U, U, P, I, P, I, P, I, P, I]
probe.probe_fly.argtypes = [I, I, I, I, I, I, U, U, P, P, I, P, P, I]
probe.probe_fade.argtypes = [I, I, I, F, P, I]
probe.probe_release.argtypes = [I, P, P, I]
probe.probe_marks.argtypes = [P, I, P, I]
probe.probe_on_kill.argtypes = [I, U, I, P, I, U, I, U, I, I, U, U, P, I]
CAP = 1 << 16
BUF = ctypes.create_string_buffer(CAP)
ZERO = 0x63d870
LIST, LIST_COUNT, PILOT_GAME = 0x605714, 0x605718, 0x60861c
OP_NEW, OP_DELETE = 0x5abd26, 0x5abd1a


def fb(v):
    return struct.unpack('<I', struct.pack('<f', v))[0]


def arr(t, vals):
    return (t * max(len(vals), 1))(*vals)


class Emulated:
    def __init__(self, stub_canned, fly=False):
        cpu = self.cpu = Original()
        cpu.import_code(IAT_FTOL, FTOL)
        cpu.fpu_control(0x007f)
        assert cpu.read(ZERO, 12) == bytes(12), 'the zero vector'
        cpu.stub_function(OP_NEW, lambda c: self.zalloc(c.arg(0)))
        cpu.stub_function(OP_DELETE, lambda c: 0)
        if not fly:  # the fly cases stand these in their own way
            cpu.stub_function(0x4f9af0, self.copy, 4)
        cpu.stub_function(0x4fc0b0, self.place, 0xc)
        cpu.stub_function(0x508aa0, self.start, 0x24)
        if not fly:
            cpu.stub_function(0x4ebb40, self.unparent)
        cpu.stub_function(0x506bc0, self.anims_off, 4)
        cpu.stub_function(0x4f9050, self.destroy)
        cpu.stub_function(0x4ed530, self.world_pos)
        if stub_canned:
            cpu.stub_function(0x432830, self.canned, 0x10)
        cpu.stub_function(0x4622a0, self.duck, 4)
        cpu.stub_function(0x432990, self.sound, 0x14)
        cpu.stub_function(0x48f9e0, self.player_died)
        head = self.zalloc(12)
        cpu.write(head, struct.pack('<II', head, head))
        cpu.u32(LIST, head)
        cpu.u32(LIST_COUNT, 0)
        self.game = self.zalloc(0x80)
        cpu.u32(PILOT_GAME, self.game)
        self.pilot = self.zalloc(0x234)
        self.ids = {}
        self.out = bytearray()
        self.bad = None

    def zalloc(self, n):
        p = self.cpu.alloc(max(n, 4))
        self.cpu.write(p, bytes(max(n, 4)))
        return p

    def log(self, *v):
        for x in v:
            self.out += struct.pack('<I', x & 0xffffffff)

    def logf(self, a, n=1):
        self.out += self.cpu.read(a, 4 * n)

    def r32(self, a):
        return struct.unpack('<I', self.cpu.read(a, 4))[0]

    # --- the canned effects' calls ---
    def copy(self, c):
        r = self.copies.pop(0)
        node = 0
        if r:
            node = self.zalloc(0x60)
            parents = self.rng.randint(0, 2)
            arr_ = self.zalloc(8)
            self.cpu.write(arr_, struct.pack('<II', 0x7100, 0x7200))
            self.cpu.write(node + 0x54, struct.pack('<II', parents, arr_))
            self.ids[node] = (r, parents)
        self.log(1, c.ecx, r)
        return node

    def nid(self, node):
        return self.ids[node][0] if node in self.ids else node

    def place(self, c):
        self.log(2, self.nid(c.ecx), c.arg(0), c.arg(1), c.arg(2))

    def start(self, c):
        self.log(3, c.ecx, self.nid(c.edx), *[c.arg(i) for i in range(9)])
        return self.started

    def unparent(self, c):
        node = c.edx
        n, a = struct.unpack('<II', self.cpu.read(node + 0x54, 8))
        if c.ecx != self.r32(a) or n == 0:
            self.bad = 'unparent'
        self.cpu.write(node + 0x54, struct.pack('<I', n - 1))
        self.cpu.u32(a, self.r32(a + 4))
        self.removed[node] = self.removed.get(node, 0) + 1

    def anims_off(self, c):
        if c.edx != 1 or c.arg(0) != 1:
            self.bad = 'anims_off args'
        self.stopped.append(c.ecx)

    def destroy(self, c):
        node = c.ecx
        if self.removed.get(node, 0) != self.ids[node][1] or self.stopped[-1:] != [node]:
            self.bad = 'free order'
        self.log(4, self.nid(node))

    # --- the death's calls ---
    def world_pos(self, c):
        self.log(0x10, c.ecx)
        self.cpu.write(c.edx, struct.pack('<3f', *self.hip_at))

    def canned(self, c):
        at, vel = c.arg(2), c.arg(3)
        self.log(0x11, c.arg(0), c.arg(1))
        self.logf(at, 3)
        self.log(0 if vel == ZERO else 1)
        if at == ZERO or c.ecx != self.pilot:
            self.bad = 'canned args'
        return self.canned_ret

    def duck(self, c):
        self.log(0x12, c.arg(0))

    def sound(self, c):
        if (c.arg(1), c.arg(2), c.arg(3), c.arg(4)) != (fb(1.0), 0, 0, fb(-1.0)) or c.ecx != self.pilot:
            self.bad = 'sound args'
        self.log(0x13, c.arg(0))
        return self.sound_ret

    def player_died(self, c):
        if c.ecx != 0x6098d0:
            self.bad = 'player_died'
        self.log(0x14)

    # --- the cases ---
    def effect_ended(self, has_node, nid, handles, effect, value):
        self.out = bytearray()
        part = self.zalloc(0x1b4)
        if has_node:
            node = self.zalloc(0x60)
            self.cpu.u32(node + 0x28, nid)
            self.cpu.u32(part + 8, node)
        self.cpu.write(part + 0x6c, struct.pack('<4I', *handles))
        self.cpu.call(0x461c90, ecx=effect, edx=part, stack_args=(value,))
        self.logf(part + 0x6c, 4)
        return bytes(self.out)

    def canned_setup(self, slots):
        for i in range(5):
            self.cpu.write(self.pilot + 0x14c + 0x14 * i, struct.pack('<5I', *slots[5 * i:5 * i + 5]))

    def canned_case(self, slot, node, at, vel, copies, started, rng):
        self.out = bytearray()
        self.copies, self.started, self.rng = list(copies), started, rng
        def vec(v):
            if v is None:
                return ZERO
            a = self.zalloc(12)
            self.cpu.write(a, struct.pack('<3f', *v))
            return a
        self.cpu.call(0x432830, ecx=self.pilot, stack_args=(slot, node, vec(at), vec(vel)))
        self.log(self.cpu.uc.reg_read(UC_X86_REG_EAX))
        return bytes(self.out)

    def canned_free(self):
        self.out = bytearray()
        self.removed, self.stopped = {}, []
        self.cpu.call(0x431800)
        self.log(0xf)
        if self.r32(LIST_COUNT) != 0:
            self.bad = 'list count'
        return bytes(self.out)

    def death_prep(self, state, detail, x634, player, hip_at, canned, sound):
        self.out = bytearray()
        self.hip_at, self.canned_ret, self.sound_ret = hip_at, canned, sound
        m = self.zalloc(0x6d8)
        hip = self.zalloc(0xb0)
        self.cpu.u32(hip + 8, 0x6000)
        self.cpu.u32(m + 0x3a0, hip)
        self.cpu.u32(m + 0x684, self.pilot)
        self.cpu.u32(m + 0x688, detail)
        self.cpu.u32(m + 0x634, x634)
        self.cpu.u32(m + 0x578, state)
        self.cpu.u32(self.game + 0x14, m if player else 0)
        self.cpu.call(0x41d070, ecx=m)
        self.log(0xf, self.r32(m + 0x634), self.r32(m + 0x5e0))
        return bytes(self.out)

    # --- the kill credit 0x41b420 ---
    def vstub(self, fn, pop=0):
        a = self.cpu.code(b'\xc3')
        self.cpu.stub_function(a, fn, pop)
        return a

    def kill_setup(self):
        cpu = self.cpu
        cpu.stub_function(0x472f00, lambda c: self.log(0x22, 1 if c.arg(0) == self.k_vmech else 0), 4)
        cpu.stub_function(0x472ec0, lambda c: self.log(0x23, c.arg(0)), 4)
        # One vtable for the victim, its views and the player: +0x2c, +0x50,
        # +0x40, +0x48, +0x28 by what ECX is.
        vt = self.zalloc(0x200)
        def view(slot):
            def f(c):
                if slot == 0x28:
                    return c.ecx
                if slot == 0x2c:
                    return self.k_info_block if self.k_info else 0
                return {0x50: self.k_vmech, 0x40: self.k_vehicle, 0x48: self.k_building}[slot]
            return f
        for slot in (0x28, 0x2c, 0x40, 0x48, 0x50):
            cpu.u32(vt + slot, self.vstub(view(slot)))
        self.k_vt = vt
        gvt = self.zalloc(0x40)
        cpu.u32(gvt + 0x2c, self.vstub(lambda c: self.k_game))
        go = self.zalloc(8)
        cpu.u32(go, gvt)
        cpu.u32(self.game + 0x6c, go)
        cvt = self.zalloc(0x100)
        cpu.u32(cvt + 0xb4, self.vstub(lambda c: self.log(0x21, self.k_info if c.arg(0) == self.k_info_block else 0xdead), 4))
        self.k_cvt = cvt

    def on_kill(self, roster, allies, xf8, counters, killer_is_player, player_team, game, info, has_mech,
                victim_roster, vehicle, building):
        cpu = self.cpu
        if not hasattr(self, 'k_vt'):
            self.kill_setup()
        self.out = bytearray()
        def mech(r):
            m = self.zalloc(0x6d8)
            pl = self.zalloc(0x234)
            cpu.u32(pl + 0xc, r)
            cpu.u32(m + 0x684, pl)
            cpu.u32(m, self.k_vt)
            return m
        k = mech(roster)
        cpu.u32(k + 0xc4, allies)
        cpu.u32(k + 0xf8, xf8)
        cpu.write(k + 0x3f4, struct.pack('<6I', *counters))
        ctl = self.zalloc(0x40)
        vb = self.zalloc(8)
        cpu.u32(vb + 4, 0x10)
        cpu.u32(ctl + 4, vb)
        cpu.u32(ctl + 4 + 0x10, self.k_cvt)
        cpu.u32(k + 0x37c, ctl)
        p = k
        if not killer_is_player:
            p = mech(0)
            cpu.u32(p + 0xc0, player_team)
        cpu.u32(self.game + 0x14, p)
        self.k_game, self.k_info, self.k_vehicle, self.k_building = game, info, vehicle, building
        self.k_info_block = self.zalloc(8)
        cpu.u32(self.k_info_block, self.k_vt)
        self.k_vmech = mech(victim_roster) if has_mech else 0
        victim = self.zalloc(8)
        cpu.u32(victim, self.k_vt)
        cpu.call(0x41b420, ecx=k, stack_args=(victim,))
        self.log(0xf, self.r32(k + 0xf8))
        self.logf(k + 0x3f4, 6)
        return bytes(self.out)

    # --- the parts' destructors ---
    def release(self, kind, w):
        cpu = self.cpu
        if not hasattr(self, 'rl_setup'):
            self.rl_setup = True
            cpu.stub_function(0x506ca0, lambda c: self.log(0x60, c.ecx))
            cpu.stub_function(0x4e66a0, lambda c: self.log(0x61, c.ecx, c.arg(0)), 4)
            cpu.stub_function(0x5ac5c0, lambda c: 0)
            def rename(c):
                self.log(0x62, c.ecx, len(cpu.read(c.edx, 32).split(b'\0')[0]))
            cpu.stub_function(0x4eb300, rename)
            cpu.stub_function(0x4eb8a0, lambda c: self.log(0x63, 5, c.edx))
            cpu.stub_function(0x4eb8e0, lambda c: self.log(0x63, 6, c.edx))
            cpu.stub_function(0x4600b0, lambda c: self.log(0x64, c.ecx))
            cpu.stub_function(0x45fb50, lambda c: self.log(0x66, c.ecx))
            cpu.stub_function(0x45fab0, lambda c: self.log(0x67, c.ecx))
            cpu.stub_import(0x5bb65c, lambda c: 0)
            self.rl_vt = {}
            for cid in (9, 11, 13):
                vt = self.zalloc(8)
                cpu.u32(vt, self.vstub(lambda c, i=cid: self.log(0x65, i), 4))
                self.rl_vt[cid] = vt
        self.out = bytearray()
        part = self.zalloc(0x1b4)
        cpu.write(part + 0x68, struct.pack('<III', w[2], w[1], w[0]))
        cpu.u32(part + 8, w[3])
        def child(cid):
            b = self.zalloc(0x10)
            cpu.u32(b, self.rl_vt[cid])
            return b
        fn = {0: 0x45fd40, 1: 0x464250, 2: 0x463740, 3: 0x466380, 4: 0x4628c0}[kind]
        if kind == 1:
            cpu.write(part + 0x74, struct.pack('<II', w[4], w[5]))
        elif kind == 2:
            cpu.write(part + 0x74, struct.pack('<II', child(11) if w[6] else 0, child(13) if w[7] else 0))
        elif kind == 3:
            cpu.u32(part + 0x74, child(9) if w[6] else 0)
        elif kind == 4:
            cpu.u32(part + 0xe4, w[8])
            cpu.write(part + 0xa4, struct.pack('<II', 0x1234 if w[9] else 0, 0x5678 if w[9] else 0))
        cpu.call(fn, ecx=part)
        self.log(0xf)
        return bytes(self.out)

    # --- the flown parts' fade 0x413cb0 ---
    def fade(self, count, ia, used, value):
        cpu = self.cpu
        if not hasattr(self, 'fd_setup'):
            self.fd_setup = True
            cpu.stub_function(0x479460, lambda c: self.fd_ia)
            def used_(c):
                self.log(0x51, 0x6000 if c.ecx == self.fd_mnode else c.ecx)
                cpu.u32(c.edx, self.fd_used)
                return 0
            cpu.stub_function(0x4fbb10, used_)
            cpu.stub_function(0x4fbb90, lambda c: self.log(0x52, self.fd_map(c.ecx), c.edx))
            def val(c):
                self.log(0x53, 0x6000 if c.ecx == self.fd_mnode else c.ecx)
                cpu.write(c.edx, struct.pack('<f', self.fd_value))
                return 0
            cpu.stub_function(0x4fbaa0, val)
            cpu.stub_function(0x4fba20, lambda c: (self.log(0x54, self.fd_map(c.ecx)), self.logf(cpu.uc.reg_read(UC_X86_REG_ESP) + 4))[0], 4)
        self.out = bytearray()
        self.fd_ia, self.fd_used, self.fd_value = ia, used, value
        m = self.zalloc(0x6d8)
        self.fd_mnode = self.zalloc(0x100)
        cpu.u32(m + 4, self.fd_mnode)
        head = self.zalloc(12)
        items = []
        prev = head
        self.fd_nodes = {}
        for i in range(count):
            it = self.zalloc(12)
            nd = self.zalloc(0x100)
            self.fd_nodes[nd] = 0x7000 + 0x10 * i
            cpu.u32(it + 8, nd)
            items.append(it)
        ring = [head] + items
        for i, it in enumerate(ring):
            cpu.u32(it, ring[(i + 1) % len(ring)])
            cpu.u32(it + 4, ring[i - 1])
        cpu.u32(m + 0x6d0, head)
        cpu.u32(m + 0x6d4, count)
        cpu.call(0x413cb0, ecx=m, stack_args=(fb(1.0 / 30),))
        self.log(0xf, self.r32(m + 0x6d4))
        return bytes(self.out)

    def fd_map(self, v):
        return self.fd_nodes.get(v, v)

    # --- the surface marks' setters ---
    def marks(self, ops):
        self.out = bytearray()
        cpu = self.cpu
        for a in (0x74a2ac, 0x74a2b0, 0x74a2b4, 0x74a2b8, 0x74a2bc, 0x74a2c0, 0x74a2c4):
            cpu.u32(a, 0)
        for op, a, b in ops:
            if op == 1:
                cpu.call(0x5426a0, ecx=a)
            elif op == 2:
                cpu.call(0x5426b0, ecx=a)
            elif op == 3:
                cpu.call(0x5426e0, stack_args=(a, b))
            elif op == 4:
                cpu.call(0x542700, ecx=a)
            else:
                cpu.call(0x542710, ecx=a)
                self.log(1 if cpu.uc.reg_read(UC_X86_REG_EAX) & 0xff else 0)
            self.logf(0x74a2ac, 7)
            cpu.call(0x5426d0)
            self.log(1 if cpu.uc.reg_read(UC_X86_REG_EAX) else 0)
        return bytes(self.out)

    # --- a part flying off ---
    def fly_setup(self):
        cpu = self.cpu
        m = self.map_node
        def world(c):
            self.log(0x30)
            self.cpu.write(c.arg(0), struct.pack('<12f', *self.f_world))
        cpu.stub_function(0x461cf0, world, 4)
        cpu.stub_function(0x4f9af0, lambda c: (self.log(0x31, c.ecx), self.f_copy)[1], 4)
        cpu.stub_function(0x4fb740, lambda c: (self.log(0x32), self.f_create)[1])
        def report(c):
            name = self.cpu.read(c.arg(4), 16).split(b'\0')[0]
            self.log(0x33, len(name), c.arg(5))
        cpu.stub_function(0x49bfd0, report)
        cpu.stub_function(0x4eb300, lambda c: self.log(0x34, c.ecx, m(c.edx)))
        cpu.stub_function(0x4eb620, lambda c: self.log(0x35, m(c.ecx), c.edx))
        cpu.stub_function(0x4ebb40, lambda c: self.log(0x36, m(c.ecx), m(c.edx)))
        cpu.stub_function(0x4eb8a0, lambda c: self.log(0x38, 5, c.edx))
        cpu.stub_function(0x4eb8e0, lambda c: self.log(0x38, 6, c.edx))
        def pose(c):
            self.log(0x39, m(c.edx))
            self.logf(c.ecx, 12)
        cpu.stub_function(0x461d30, pose, 8)
        cpu.stub_function(0x4eb9b0, lambda c: self.log(0x3a, m(c.ecx), c.edx))
        cpu.stub_function(0x460180, lambda c: self.log(0x3c))
        cpu.stub_function(0x460050, lambda c: self.log(0x3d, c.arg(0)), 8)
        cpu.stub_function(0x462000, lambda c: self.log(0x3e, m(c.ecx)))
        cpu.stub_function(0x413bb0, lambda c: self.f_debris.append(m(c.arg(0))), 4)
        cpu.stub_function(0x4b1040, lambda c: self.log(0x40, c.arg(0)), 4)
        ov = self.zalloc(8)
        cpu.u32(ov, self.vstub(lambda c: self.log(0x37, m(c.arg(0))), 4))
        o = self.zalloc(8)
        cpu.u32(o, ov)
        cpu.u32(0x608c84, o)
        def set_node(c):
            self.log(0x3b, c.arg(0))
            self.cpu.u32(c.ecx + 0x14, self.f_newbuf)
        self.f_set_node = self.vstub(set_node, 4)
        self.f_arm_vt = []
        for side in (0, 1):
            vt = self.zalloc(0x80)
            cpu.u32(vt + 0xc, self.f_set_node)
            cpu.u32(vt + 0x20, self.vstub(lambda c, s=side: self.log(0x41, s, c.arg(0), c.arg(1)), 8))
            cpu.u32(vt + 0x3c, self.vstub(lambda c, s=side: self.log(0x42, s, c.arg(0), c.arg(1)), 8))
            self.f_arm_vt.append(vt)
        self.f_leg_vt = self.zalloc(0x80)
        cpu.u32(self.f_leg_vt + 0xc, self.f_set_node)
        self.f_torso_vt = self.zalloc(0x80)
        cpu.u32(self.f_torso_vt + 0x20, self.vstub(lambda c: self.log(0x43, c.arg(0), c.arg(1)), 8))

    def map_node(self, v):
        return {self.f_node: 0x5000, self.f_parent: 0x5100}.get(v, v)

    def fly(self, kind, side, roster, nid, stub, stubs, copy, create, world, local, parent, mech_vel):
        cpu = self.cpu
        if not hasattr(self, 'f_set_node'):
            self.f_node = self.f_parent = -1
            self.fly_setup()
        self.out = bytearray()
        self.f_world, self.f_copy, self.f_create, self.f_debris = world, copy, create, []
        self.f_newbuf = self.zalloc(48)
        def block(n, vt=0):
            b = self.zalloc(n)
            if vt:
                cpu.u32(b, vt)
            return b
        def node(i):
            nd = block(0x100)
            cpu.u32(nd + 0x28, i)
            return nd
        mech = block(0x6d8)
        pl = block(0x234)
        nm = block(8)
        cpu.write(nm, b'owens\0')
        cpu.u32(pl + 0xc, roster)
        cpu.u32(pl + 0x10, nm)
        if stubs:
            cpu.u32(pl + 0x210, 0x710)
            for i in range(4):
                cpu.u32(pl + 0x214 + 4 * i, 0x700 + i)
        cpu.u32(mech + 0x684, pl)
        cpu.write(mech + 0x30, struct.pack('<3f', *mech_vel))
        cpu.u32(mech + 0xd4, block(0x100))
        torso = block(0x1b4, self.f_torso_vt)
        cpu.u32(mech + 0x39c, torso)
        parts = {}
        for i in (0, 1):
            a = block(0xd0, self.f_arm_vt[i])
            hd = block(0x80)
            cpu.u32(hd + 4, mech), cpu.u32(hd + 8, node(9 + i))
            cpu.u32(a + 4, mech), cpu.u32(a + 8, node(2 + i)), cpu.u32(a + 0x74, hd)
            lg = block(0xd4, self.f_leg_vt)
            cf, ft = block(0x80), block(0xec)
            cpu.u32(cf + 4, mech), cpu.u32(cf + 8, node(11 + i))
            cpu.u32(ft + 4, mech), cpu.u32(ft + 8, node(13 + i))
            cpu.u32(lg + 4, mech), cpu.u32(lg + 8, node(4 + i)), cpu.u32(lg + 0x74, cf), cpu.u32(lg + 0x78, ft)
            cpu.u32(mech + 0x394 + 4 * i, a), cpu.u32(mech + 0x38c + 4 * i, lg)
            parts[(1, i)], parts[(2, i)] = a, lg
        x = parts[(kind, side)]
        self.f_node = node(nid)
        self.f_parent = block(0x100)
        arr_ = block(8)
        cpu.u32(arr_, self.f_parent)
        cpu.write(self.f_node + 0x54, struct.pack('<II', 1 if parent else 0, arr_))
        cpu.u32(x + 8, self.f_node)
        own = block(48)
        cpu.write(own, struct.pack('<12f', *local))
        cpu.u32(x + 0x14, own)
        vel = block(12)
        cpu.write(vel, struct.pack('<3f', 9, 9, 9))
        fn = 0x467100 if kind == 1 else 0x466fe0
        cpu.call(fn, ecx=x, stack_args=(block(12), block(12), vel, stub))
        r = self.cpu.uc.reg_read(UC_X86_REG_EAX)
        for d in self.f_debris:
            self.log(0x3f, d)
        self.log(0xf, self.map_node(r))
        self.logf(vel, 3)
        self.logf(self.f_newbuf, 12)
        return bytes(self.out)

    def anim_nodes(self, node, root, objects, nodes, queries):
        self.out = bytearray()
        d = self.zalloc(0x13c)
        o = self.zalloc(0x60 * max(len(objects), 1))
        n = self.zalloc(0x28 * max(len(nodes), 1))
        for i, v in enumerate(objects):
            self.cpu.u32(o + 0x60 * i + 0x24, v)
        for i, v in enumerate(nodes):
            self.cpu.u32(n + 0x28 * i + 0x24, v)
        self.cpu.u32(d + 0x40, node), self.cpu.u32(d + 0x64, root)
        self.cpu.write(d + 0x109, bytes([len(objects), len(nodes)]))
        self.cpu.u32(d + 0x114, o), self.cpu.u32(d + 0x118, n)
        for q in queries:
            self.cpu.call(0x506ae0, ecx=d, edx=q)
            self.log(1 if self.cpu.uc.reg_read(UC_X86_REG_EAX) else 0)
        self.cpu.call(0x506b60, ecx=d)
        for i in range(len(objects)):
            self.logf(o + 0x60 * i + 0x24)
        for i in range(len(nodes)):
            self.logf(n + 0x28 * i + 0x24)
        return bytes(self.out)


def main():
    rng = random.Random(int(os.environ.get('SEED', '1')))
    brk = int(os.environ.get('BREAK', '0'))
    emu = Emulated(False)
    dying = Emulated(True)
    flying = Emulated(False, fly=True)
    runs = fails = 0
    kinds = {}

    def check(kind, mine, theirs):
        nonlocal runs, fails
        runs += 1
        kinds[kind] = kinds.get(kind, 0) + 1
        for e in (emu, dying, flying):
            if e.bad:
                print(f'FAIL {kind}: the original called out wrongly ({e.bad})')
                e.bad = None
                fails += 1
        if mine != theirs:
            fails += 1
            if fails <= 3:
                print(f'FAIL {kind}: port {mine.hex()}\n     original {theirs.hex()}')

    # The effect-end callback: which handle the ending effect was.
    for _ in range(400):
        handles = [rng.choice((0, 0x11, 0x22, 0x33, 0x44)) for _ in range(4)]
        effect = rng.choice((0x11, 0x22, 0x33, 0x44, 0x55, 0))
        has_node, nid, value = rng.randint(0, 1), rng.choice((1, 1, 2, 0x101, -1)), rng.choice((0, 0, 1, 7))
        mine_value = (1 - value if value in (0, 1) else value) if brk == 1 else value
        n = probe.probe_effect_ended(has_node, nid, arr(U, handles), effect, mine_value, BUF, CAP)
        check('end', BUF.raw[:n], emu.effect_ended(has_node, nid, handles, effect, value))

    # The canned effects, a mission's worth of node copies, then their free.
    for _ in range(30):
        slots = []
        for i in range(5):
            slots += [rng.choice((0, 0x900 + i)), fb(rng.uniform(-2, 2)), fb(rng.choice((0.0, 3.6))),
                      fb(rng.uniform(-2, 2)), rng.choice((0, 0x800 + i, 0x800 + i))]
        emu.canned_setup(slots)
        probe.probe_canned_setup(arr(U, slots))
        for _ in range(rng.randint(0, 8)):
            slot = rng.randint(0, 4)
            node = rng.choice((0, 0, 0x4321))
            at = rng.choice((None, (rng.uniform(-500, 500), rng.uniform(0, 50), rng.uniform(-500, 500))))
            vel = rng.choice((None, (1.0, -2.5, 0.25)))
            copies = [rng.choice((0x1000 + rng.randint(0, 0xfff), 0x1000 + rng.randint(0, 0xfff), 0))]
            started = rng.choice((0, 0x5555))
            mine_at = (at[0] + 1.0, at[1], at[2]) if brk == 2 and at else at
            fa = arr(F, mine_at) if mine_at else None
            fv = arr(F, vel) if vel else None
            n = probe.probe_canned(slot, node, fa, fv, arr(U, copies), started, BUF, CAP)
            check('canned', BUF.raw[:n], emu.canned_case(slot, node, at, vel, copies, started, rng))
        n = probe.probe_canned_free(BUF, CAP)
        check('free', BUF.raw[:n], emu.canned_free())

    # The death's preparation.
    probe.probe_canned_setup(arr(U, [0] * 25))
    for _ in range(600):
        state = rng.choice((1, 2, 10, 11, 12, 13, 14))
        detail, x634 = rng.choice((0, 1, 0x3e, 0x3f)), rng.choice((0, 1, 0xff, 0x80000001))
        player = rng.randint(0, 1)
        hip_at = (rng.uniform(-900, 900), rng.uniform(0, 60), rng.uniform(-900, 900))
        canned, sound = rng.choice((0, 0x7000)), rng.choice((0, 0x7100))
        mine_state = 1 if brk == 3 and state in (12, 13) else state
        n = probe.probe_death_prep(mine_state, detail, x634, player, arr(F, hip_at), canned, sound, BUF, CAP)
        check('death_prep', BUF.raw[:n], dying.death_prep(state, detail, x634, player, hip_at, canned, sound))

    # zEffect's node checks.
    for _ in range(400):
        pool = [0x100 * k for k in range(1, 9)]
        objects = [rng.choice(pool + [0]) for _ in range(rng.randint(0, 5))]
        nodes = [rng.choice(pool + [0]) for _ in range(rng.randint(0, 5))]
        node, root = rng.choice(pool + [0]), rng.choice(pool + [0])
        queries = [rng.choice(pool + [0]) for _ in range(6)]
        mine_q = [q + (0x100 if brk == 4 and q == node else 0) for q in queries]
        n = probe.probe_anim_nodes(node, root, arr(U, objects), len(objects), arr(U, nodes), len(nodes),
                                   arr(U, mine_q), len(mine_q), BUF, CAP)
        check('anim_nodes', BUF.raw[:n], emu.anim_nodes(node, root, objects, nodes, queries))

    # The kill credit.
    for _ in range(600):
        roster = rng.choice((7, 9, 11, 0, 6, 15))
        allies, team = rng.choice((0, 1, 2, 3)), rng.choice((0, 1, 2))
        xf8 = rng.choice((-1, 3, 7))
        counters = [rng.randint(0, 5) for _ in range(3)] + [rng.randint(0, 3000) for _ in range(3)]
        killer_is_player, game = rng.randint(0, 1), rng.choice((1, 1, 2, 3, 4, 0))
        info = rng.choice((0, 0x4401, 0x4401, 0x4401))
        has_mech = rng.randint(0, 1)
        victim_roster = rng.randint(-1, 19)
        vehicle, building = rng.choice((0, 0x5501)), rng.choice((0, 0x6601))
        mine_roster = victim_roster
        if brk == 5 and has_mech:
            mine_roster = {9: 10, 10: 9}.get(victim_roster, victim_roster)
        mine_allies = allies ^ 1 if brk == 6 else allies
        n = probe.probe_on_kill(roster, mine_allies, xf8, arr(U, counters), killer_is_player, team, game, info,
                                has_mech, mine_roster, vehicle, building, BUF, CAP)
        check('on_kill', BUF.raw[:n], emu.on_kill(roster, allies, xf8, counters, killer_is_player, team, game, info,
                                                  has_mech, victim_roster, vehicle, building))

    # The parts' destructors.
    for _ in range(400):
        kind = rng.randint(0, 4)
        w = [rng.choice((0, 0x1100)), rng.choice((0, 0x1200)), rng.choice((0, 0, 0x1300)), rng.choice((0, 0x5000, 0x5000)),
             rng.choice((0, 0x1400)), rng.choice((0, 0x1500)), rng.randint(0, 1), rng.randint(0, 1),
             rng.choice((0, 0x1600)), rng.randint(0, 1)]
        if kind == 0:
            w[4] = w[5] = 0
        mine = list(w)
        if brk == 11:
            mine[0], mine[1] = mine[1], mine[0]
        n = probe.probe_release(kind, arr(U, mine), BUF, CAP)
        check('release', BUF.raw[:n], emu.release(kind, w))

    # The flown parts' fade.
    for _ in range(200):
        count, ia, used = rng.randint(0, 4), rng.randint(0, 1), rng.randint(0, 1)
        value = rng.choice((0.0, 1.0, 0.5, rng.uniform(0, 1), -0.25))
        n = probe.probe_fade(count, (1 - ia) if brk == 10 else ia, used, value, BUF, CAP)
        check('fade', BUF.raw[:n], emu.fade(count, ia, used, value))

    # The surface marks' setters (types 0..2: past them the exe writes on; the
    # rebuild's FIX (bug) ignores them).
    for _ in range(300):
        ops = []
        for _ in range(rng.randint(1, 10)):
            op = rng.randint(1, 5)
            if op == 1:
                ops.append((1, rng.randint(0, 2), 0))
            elif op == 3:
                ops.append((3, fb(rng.uniform(-2, 2)), fb(rng.uniform(-2, 2))))
            elif op == 4:
                ops.append((4, rng.choice((0, 1, 7)), 0))
            else:
                ops.append((op, rng.choice((0, 0x1000, 0x2000, 0x3000)), 0))
        mine = [(o, a + 1 if brk == 9 and o == 2 and a else a, b) for o, a, b in ops]
        flat = [x for t in mine for x in t]
        n = probe.probe_marks(arr(U, flat), len(mine), BUF, CAP)
        check('marks', BUF.raw[:n], emu.marks(ops))

    # A part flying off.
    def rot(rng):
        import math
        a, b = rng.uniform(-3.1, 3.1), rng.uniform(-1.5, 1.5)
        ca, sa, cb, sb = math.cos(a), math.sin(a), math.cos(b), math.sin(b)
        return [ca, 0, -sa, sa * sb, cb, ca * sb, sa * cb, -sb, ca * cb]
    for _ in range(500):
        kind, side = rng.randint(1, 2), rng.randint(0, 1)
        roster = rng.choice((2, 11, 11, 9, 7, 0))
        nid = rng.choice(((2 + side) if kind == 1 else (4 + side),) * 3 + (1, 0, 6, 3, 5))
        stub, stubs = rng.choice((0, 1, 1, 0x100)), rng.randint(0, 1)
        copy, create = rng.choice((0x900, 0x900, 0)), rng.choice((0xa00, 0xa00, 0))
        world = rot(rng) + [rng.uniform(-500, 500), rng.uniform(0, 30), rng.uniform(-500, 500)]
        local = [rng.uniform(-1, 1) for _ in range(9)] + [rng.uniform(-3, 3) for _ in range(3)]
        parent = rng.choice((1, 1, 0))
        mech_vel = [rng.uniform(-20, 20), rng.uniform(-3, 3), rng.uniform(-20, 20)]
        mine_side = side
        if brk == 7 and roster in (2, 11) and kind == 2:
            mine_side = 1 - side
        mine_world = list(world)
        if brk == 8:
            mine_world[0] = -mine_world[0]
        n = probe.probe_fly(kind, mine_side, roster, nid, stub, stubs, copy, create, arr(F, mine_world), arr(F, local),
                            parent, arr(F, mech_vel), BUF, CAP)
        check('fly', BUF.raw[:n], flying.fly(kind, side, roster, nid, stub, stubs, copy, create, world, local, parent,
                                          mech_vel))

    if fails:
        print(f'{fails} of {runs} runs differ')
        sys.stdout.flush(); os._exit(1)
    print(f'ok: {runs} runs identical ({", ".join(f"{k} {v}" for k, v in sorted(kinds.items()))}); '
          f'{len(emu.ids)} node copies made and freed, {sum(p for _, p in emu.ids.values())} parents taken off')
    sys.stdout.flush(); os._exit(0)


if __name__ == '__main__':
    main()
