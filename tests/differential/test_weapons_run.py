"""Weapons at run time (docs/weapons.md) against the original x86: the weapon
manager (Vehicle +0xd4, 0x4b00d0..0x4b2da0), its groups (0x4b1d80..0x4b21f0),
the ammo bins (0x4b4cc0..0x4b4dc0), projectile weapons (vtable 0x5c9088) and
beams (0x5c90c0), built from loadout items and fired, held, released,
chain/single/group fire, alpha strikes, salvos, lock-on, recharge, heat,
ammo use, jettison, crits and cook-offs, part loss, the arm restriction,
network fire and the vehicle form.

    cmake --build build && .venv/bin/python tests/differential/test_weapons_run.py

The definitions are the real weapons.zrd, read on both sides as in
test_weapons_zrd.py. The world the weapons touch is fake on both sides and
the same: the owner mech (its parts, nodes, firepoints, FCS, heat, loadout
items), the vehicle and its entity, a target, the view, the HUD, sounds,
zWeapon's spawn and beam calls. Every call out is logged on both sides; after
each operation the manager, its five groups, every weapon and bin, the zWeapon
fire state and each def's fire effect must match exactly at 24-bit x87
precision.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

os.environ.setdefault('MW3_X87', '24')
sys.path.insert(0, str(Path(__file__).parent))
from original import STACK_TOP  # noqa: E402,F401
import test_weapons_zrd as zr  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

probe = zr.probe
probe.probe_wm_world.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int]
probe.probe_wm_new.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p, ctypes.c_int, ctypes.c_uint32]
probe.probe_wm_op.argtypes = [ctypes.c_int, ctypes.c_void_p]
probe.probe_wm_op.restype = ctypes.c_int32

IAT_RAND, IAT_NEW, IAT_DELETE = 0x5bb668, 0x5bb4bc, 0x5bb4c8
VT_WEAPON_ITEM, VT_AMMO_ITEM, VT_EQUIP_ITEM = 0x5c0fc0, 0x5bd150, 0x5bd180
F = {'manager_ctor': 0x4b00d0, 'reset': 0x4b0290, 'find': 0x4b03c0, 'add': 0x4b0400, 'add_vehicle': 0x4b0460,
     'next_group': 0x4b04f0, 'prev_group': 0x4b0610, 'select_group': 0x4b0730, 'next_weapon': 0x4b0790,
     'prev_weapon': 0x4b07e0, 'mode_sound': 0x4b0830, 'net_fire': 0x4b08c0, 'alpha': 0x4b0a70,
     'jettison': 0x4b0ae0, 'set_aim': 0x4b0bc0, 'set_online': 0x4b0bf0, 'set_trigger': 0x4b0c20,
     'update': 0x4b0c30, 'critical': 0x4b0de0, 'part_lost': 0x4b1040, 'destroy_all': 0x4b1110,
     'unlimited': 0x4b0220, 'arms': 0x4b2da0, 'set_group': 0x4b1e10, 'forget': 0x4b0370, 'add_damage': 0x4b2d50,
     'bin_add': 0x4b4dc0, 'can_fire': 0x4b3a20, 'min_range': 0x4b3990, 'max_range': 0x4b39a0,
     'max_range_sq': 0x4b39e0}
NAMES = {1: 'part_by_id', 2: 'node_part', 3: 'equipment_in_part', 4: 'heat', 5: 'add_heat', 6: 'recoil',
         7: 'net_event', 8: 'stats_shot', 9: 'lock_notice', 10: 'entity_id', 11: 'node_position', 12: 'spawn',
         13: 'radar_track', 14: 'beam_create', 15: 'beam_start', 16: 'beam_stop', 17: 'beam_free',
         18: 'mech_sound', 19: 'play_loop', 20: 'play_at', 21: 'stop_sound', 22: 'destroyed_sounds',
         23: 'voice_destroyed', 24: 'hud_groups', 25: 'hud_weapons', 26: 'hud_fire', 27: 'torso_aim',
         30: 'query', 31: 'type_ready', 32: 'lab_rounds'}
# World slots (probe_weapons_run.cpp, enum W)
W = ['part_mech', 'player', 'mission_player', 'stats', 'lock_targets', 'vehicle', 'entity', 'mech', 'vehicle_node',
     'entity_node', 'x64', 'record', 'target', 'target_mech', 'tm_ecm', 'entity_id', 'target_x10', 'target_key',
     'target_proj', 'allies', 'team', 'owner_x58', 'fcs', 'control', 'heat', 'screen', 'highlight', 'view',
     'view_kind', 'torso_aim', 'torso_look', 'rot_a', 'rot_a1', 'rot_a2', 'rot_a3', 'rot_b', 'rot_b1', 'rot_b2',
     'rot_b3', 'clock', 'dt',
     'vel', 'vel1', 'vel2', 'spawn', 'beam', 'net_id', 'net_id1', 'net_id2', 'net_t', 'net_t1', 'net_t2', 'time']
WI = {k: i for i, k in enumerate(W)}
MECH_WEAPONS = list(range(37))
BEAMS = {4, 5, 6, 10, 11, 12, 23, 24, 25, 32}


def f2i(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def i2f(i):
    return struct.unpack('<f', struct.pack('<I', i & 0xffffffff))[0]


def f32(x):
    return i2f(f2i(x))


s32 = zr.s32


class MsvcRand:
    def __init__(self, seed):
        self.state = seed

    def __call__(self, cpu=None):
        self.state = (self.state * 214013 + 2531011) & 0xffffffff
        return (self.state >> 16) & 0x7fff


class Emulated(zr.Emulated):
    """The original reader's machine, then the run-time world on top."""

    def __init__(self):
        super().__init__()
        cpu = self.cpu
        crater = [0] * 10
        real = zr.real_bytes()
        rc = self.run(real, crater, 1, 1)
        assert rc == 0
        self.defs_base = self.u32(0x9a0eac)
        self.ndefs = self.u32(0x9a0ea8)
        # the port reads the same file the same way
        zr.WORK.mkdir(parents=True, exist_ok=True)
        arch = zr.WORK / 'run.zbd'
        zr.write_archive(arch, real)
        buf = (ctypes.c_int32 * 400000)()
        n = ctypes.c_int()
        assert probe.probe_zwep_run(str(arch).encode(), (ctypes.c_uint32 * 10)(*crater), 1, 1, buf, 400000,
                                    ctypes.byref(n)) == 0
        for k in range(0, self.ndefs, 3):
            d = self.defs_base + 0x1f4 * k
            cpu.u32(d + 0x84, self.u32(d + 0x84) | 2)
            probe.probe_wm_fire_flag(k, self.u32(d + 0x84))
        self.def_names = {self.cstr(self.u32(self.defs_base + 0x1f4 * k)).decode(): k for k in range(self.ndefs)}
        self.last_hit = self.last_pos = None
        self.events = []
        cpu.stub_import(IAT_NEW, self.new)
        cpu.stub_import(IAT_DELETE, lambda c: 0)
        cpu.stub_import(IAT_RAND, lambda c: self.rand())
        self.rand = MsvcRand(1)
        huge = self.zalloc(8)  # MSVCRT's _HUGE, a data import: +inf
        cpu.write(huge, struct.pack('<d', float('inf')))
        cpu.u32(0x5bb5bc, huge)
        a = self.zalloc
        self.k = a(0x200)  # scripted values the little routines read
        self.ks = {}
        # objects
        self.V, self.E, self.M, self.MP, self.X, self.Y = a(0x100), a(0x100), a(0x700), a(0x700), a(0x40), a(0x100)
        self.P, self.D, self.VEC, self.CT, self.TO = a(0x20), a(0x40), a(0x10), a(0x20), a(0x100)
        self.S = a(0x18000)
        self.PL, self.CAM, self.VO, self.MS = a(0x80), a(0x10), a(0x80), a(0x40)
        self.T, self.TT, self.TM, self.R = a(0x10), a(0x10), a(0x400), a(0x10)
        self.NET = [a(0x700) for _ in range(2)]
        self.netlist = a(0x40)
        self.stats_obj = a(0x10)
        self.hit, self.proj = a(0x40), a(0x600)
        self.QA = a(0x10)
        self.cfg = a(0x40)
        self.parts = [a(0x60) for _ in range(16)]
        self.nodes = [a(0x40) for _ in range(16)]
        self.ndata = [a(0x100) for _ in range(16)]  # +0x78 axis
        self.fp_mem = [a(0x20) for _ in range(16)]
        self.fp_name = a(0x10)
        cpu.write(self.fp_name, b'fp\0')
        self.spawn_base = a(0x600 * 64)
        self.items_mem = [a(0x20) for _ in range(64)]
        self.cfg_items = [a(0x20) for _ in range(40)]
        self.list_nodes = [a(0x10) for _ in range(48)]

        def ret_k(name, size=4):
            slot = self.slot(name)
            return cpu.code(b'\xa1' + struct.pack('<I', slot) + (b'\xc3' if size == 4 else b''))

        def vt(n=0x180):
            return a(n)
        # vehicle: vt+0x2c the entity
        vv = vt()
        cpu.u32(self.V, vv)
        cpu.u32(vv + 0x2c, ret_k('entity'))
        # entity: vt+0x50 the mech
        ve = vt()
        cpu.u32(self.E, ve)
        cpu.u32(ve + 0x50, ret_k('mech'))
        # mech vtable (also the mission player's and the net targets')
        vm = vt(0x200)
        for obj in (self.M, self.MP):
            cpu.u32(obj, vm)
        heat_stub = cpu.code(b'\xc3')
        cpu.stub_function(heat_stub, lambda c: self.log(4))
        cpu.u32(vm + 0x9c, cpu.code(b'\xb8' + struct.pack('<I', heat_stub) + b'\xff\xd0' + b'\xd9\x05' +
                                    struct.pack('<I', self.slot('heat')) + b'\xc3'))
        net = cpu.code(b'\xc2\x08\x00')
        cpu.stub_function(net, lambda c: self.log(7, c.ecx, c.arg(0), self.slot_of(c.arg(1))), 8)
        cpu.u32(vm + 0x168, net)
        cpu.u32(vm + 0x58, ret_k('owner_x58'))
        cpu.u32(vm + 0x24, ret_k('X'))
        cpu.u32(vm + 0x160, ret_k('mp_id'))
        # X vt+0x28 -> Y, Y+0xc0 the team (the mission player's, through vt+0x24 -> X)
        vx = vt()
        cpu.u32(self.X, vx)
        cpu.u32(vx + 0x28, ret_k('Y'))
        # pilot, dfn, firepoints
        cpu.u32(self.M + 0x684, self.P)
        cpu.u32(self.P + 0xc, 3)
        cpu.u32(self.D + 0x24, self.VEC)
        cpu.u32(self.M + 0x3ec, self.cfg)
        cpu.u32(self.M + 0x37c, self.CT)
        cpu.u32(self.M + 0x39c, self.TO)
        # target: vt+0x24 key, +0x2c -> TT, +0xc x10; TT vt+0x50 the mech, +0x58 the projectile target
        vtg = vt()
        for obj in (self.T, *self.NET):
            cpu.u32(obj, vtg)
        cpu.u32(vtg + 0x24, ret_k('target_key'))
        cpu.u32(vtg + 0x2c, ret_k('TT'))
        cpu.u32(vtg + 0x0c, ret_k('target_x10'))
        vtt = vt()
        cpu.u32(self.TT, vtt)
        cpu.u32(vtt + 0x50, ret_k('target_mech'))
        cpu.u32(vtt + 0x58, ret_k('target_proj'))
        cpu.u32(self.R + 4, self.T)
        # the camera: [0x5f4c8c]+0x58 vt+0x1c the view object
        cpu.u32(0x5f4c8c, self.PL)
        vc = vt()
        cpu.u32(self.CAM, vc)
        cpu.u32(vc + 0x1c, ret_k('view'))
        cpu.u32(self.PL + 0x58, self.CAM)
        cpu.u32(0x60861c, self.MS)
        # parts: vtable +0x24 recoil
        vp = vt()
        rec = cpu.code(b'\xc2\x04\x00')
        cpu.stub_function(rec, lambda c: self.log(6, c.ecx, c.arg(0)), 4)
        cpu.u32(vp + 0x24, rec)
        for p in self.parts:
            cpu.u32(p, vp)
        for i, (n, d) in enumerate(zip(self.nodes, self.ndata)):
            cpu.u32(n + 0x38, d)
        stubs = {
            0x4846a0: (lambda c: self.D, 0),
            0x41d510: (self.part_by_id, 4),
            0x460000: (lambda c: self.ks['part_mech'], 0),
            0x40ef30: (self.node_part, 0),
            0x460c10: (self.equip, 8),
            0x419e40: (lambda c: self.log(5, c.ecx, c.arg(0)), 4),
            0x433630: (lambda c: self.check(c.ecx == self.stats_obj) or self.log(8, c.arg(0)), 4),
            0x41abd0: (lambda c: self.log(9, c.ecx, c.arg(0), self.def_index(c.arg(1))), 8),
            0x5a8630: (lambda c: self.log(10) or self.ks['entity_id'], 0),
            0x4ed650: (self.node_position, 0),
            0x585010: (self.spawn, 0x18),
            0x584a70: (self.beam_create, 0x14),
            0x581230: (lambda c: self.log(15, c.ecx), 0),
            0x581400: (lambda c: self.log(16, c.ecx), 0),
            0x584b40: (lambda c: self.log(17, c.ecx), 0),
            0x41e770: (lambda c: self.check(c.arg(0) == 0x3f800000 and c.arg(1) == 0xbf800000) or
                       self.log(18, c.ecx), 8),
            0x56e3d0: (self.play_loop, 4),
            0x56e7e0: (self.play_at, 8),
            0x56e9f0: (lambda c: self.log(21, c.ecx), 0),
            0x4e6dc0: (lambda c: self.log(22, c.edx, c.ecx), 0x14),
            0x4853d0: (lambda c: self.log(23, c.ecx), 0),
            0x430aa0: (lambda c: self.check(c.ecx == self.P) or (self.S if self.ks['screen'] else 0), 0),
            0x446240: (lambda c: self.log(24, int(c.ecx == self.S + 0x6520)), 0),
            0x446520: (lambda c: self.log(25, int(c.ecx == self.S + 0x6520)), 0),
            0x4464c0: (lambda c: self.log(26, int(c.ecx == self.S + 0x6520)), 0),
            0x465940: (lambda c: self.check(c.ecx == self.TO) or self.log(27) or self.ks['torso_aim'], 0),
            0x5ac5c0: (lambda c: 0, 0),
        }
        for addr, (fn, pop) in stubs.items():
            cpu.stub_function(addr, fn, pop)
        self.spawned = []
        self.sound_n = 0
        self.beam_n = 0
        self.spawn_n = 0
        self.weapon_addrs = []
        self.reached = set()
        reg = cpu.uc.reg_read
        self.cover = {
            0x4b0c9b: 'press', 0x4b0d38: 'held', 0x4b0cd5: 'release', 0x4b0cfd: 'chain step', 0x4b0d54: 'alpha end',
            0x4b218a: 'beam ground fx off', 0x4b2ce4: 'salvo continues', 0x4b3410: 'FCS tracking',
            0x4b3436: 'lock without target', 0x4b3506: 'missile target', 0x4b3520: 'fire attached',
            0x4b3240: 'out of ammo note', 0x4b31f0: 'looped sound', 0x4b4384: 'beam recharge sound',
            0x4b43d4: 'hot recharge', 0x4b4402: 'recharge floor', 0x4b42f2: 'timed heat', 0x4b434a: 'beam drained',
            0x4b406e: 'beam under water', 0x4b4009: 'beam not full', 0x4b38c7: 'aim clamped',
            0x4b3761: 'salvo scatter', 0x4b26a1: 'firepoint spread',
            0x4b0f1e: 'ammo crit', 0x4b0ff5: 'weapon crit', 0x4b0eb6: 'equipment item in crit',
            0x4b1086: 'part weapons lost', 0x4b10c7: 'empty group, next', 0x4b2ede: 'torso turned',
            0x4b2f48: 'right arm', 0x4b2de6: 'left view', 0x4b2df4: 'right view', 0x4b2fed: 'enabled',
            0x4b2fd9: 'disabled', 0x4b0b61: 'jettison one', 0x4b0b11: 'jettison group', 0x4b09fd: 'net target',
            0x4b0949: 'net trigger', 0x4b095f: 'net held', 0x4b0901: 'net release', 0x4b0975: 'net stop',
            0x4b30f7: 'destroyed: HUD weapon', 0x4b3158: 'destroyed: next weapon', 0x4b4d4a: 'ammo record',
            0x4b2b18: 'vehicle bin', 0x4b2a28: 'vehicle firepoint', 0x4b1fba: 'next found', 0x4b2077: 'prev found',
            0x4b05cb: 'group changed', 0x4b06e4: 'prev group changed', 0x4b4bc0: 'never',
            0x4b4639: 'burst pellet', 0x51e75c: 'burst straight along x', 0x4afff5: 'airstrike missile',
            0x4afdfe: 'salvo mark near', 0x4afe32: 'salvo mark expired', 0x4afd6d: 'salvo mark added',
            0x4b493e: 'lab rounds 100', 0x4b494d: 'lab rounds 20', 0x4b492a: 'lab rounds capped',
            0x4b4c83: 'type ready',
        }
        del self.cover[0x4b4bc0]

        def hit(uc, address, size, user):
            self.reached.add(address)
        for addr in self.cover:
            cpu.uc.hook_add(UC_HOOK_CODE, hit, begin=addr, end=addr)

    def check(self, ok):
        if not ok:
            import traceback
            where = traceback.extract_stack()[-2]
            raise SystemExit(f'a stub was called with unexpected registers (line {where.lineno}: {where.line})')
        return 0

    # --- memory helpers -----------------------------------------------------
    def zalloc(self, n):
        p = self.cpu.alloc(n)
        self.cpu.write(p, bytes(n))
        return p

    def new(self, cpu):
        return self.zalloc(max(cpu.arg(0), 1))

    def slot(self, name):
        if name not in self.ks_slots():
            self._slots[name] = self.k + 4 * len(self._slots)
        return self._slots[name]

    def ks_slots(self):
        if not hasattr(self, '_slots'):
            self._slots = {}
        return self._slots

    def setk(self, name, value):
        self.ks[name] = value
        self.cpu.u32(self.slot(name), value)

    def f(self, p):
        return struct.unpack('<I', self.cpu.read(p, 4))[0]

    def vec(self, p):
        return list(struct.unpack('<3I', self.cpu.read(p, 12))) if p else [0, 0, 0]

    def def_index(self, d):
        return s32(self.u32(d + 0x10)) if d else -1

    def slot_of(self, w):
        if w == 0:
            return -1
        m = self.mgr
        n = self.u32(m + 0x24)
        for i in range(n):
            if self.u32(m + 0x40 + 4 * i) == w:
                return i
        return -2

    # --- stubs ----------------------------------------------------------------
    def part_by_id(self, c):
        i = s32(c.arg(0))
        self.check(c.ecx == self.ks['mech'])
        self.log(1, i)
        return self.world_parts[i][0] if 0 <= i < 16 else 0

    def node_part(self, c):
        n = self.u32(c.ecx)
        self.log(2, n)
        for row in self.world_parts:
            if row[1] and row[1] == n:
                return row[0]
        return 0

    def equip(self, c):
        kind, part = s32(c.arg(0)), s32(c.arg(1))
        self.log(3, kind, part)
        return int(0 <= part < 16 and self.world_parts[part][11] != 0)

    def node_position(self, c):
        n, v = c.ecx, c.edx
        vals = self.vec(v)
        self.log(11, n, *vals)
        row = next((r for r in self.world_parts if r[1] and r[1] == n), None)
        if row is None or row[4]:
            return 1
        out = [f2i(i2f(vals[k]) + row[8 + k]) for k in range(3)]
        self.cpu.write(v, struct.pack('<3I', *out))
        return 0

    def spawn(self, c):
        d, node = c.ecx, c.edx
        x64, mz, dr, vel, p5, p6 = (c.arg(i) for i in range(6))
        self.log(12, self.def_index(d), node, x64, *self.vec(mz), *self.vec(dr), *self.vec(vel), s32(p5), s32(p6))
        if not self.ks['spawn']:
            return 0
        blk = self.ks['spawn'] + 0x600 * (self.spawn_n % 64)
        self.spawn_n += 1
        self.cpu.write(blk + 0x5d0, bytes(8))
        self.spawned.append((len(self.events), blk))
        return blk

    def beam_create(self, c):
        d, on = c.ecx, c.edx
        zero, node, mz, dr, one = (c.arg(i) for i in range(5))
        self.check(zero == 0 and one == 1)
        self.log(14, self.def_index(d), on, node, *self.vec(mz), *self.vec(dr))
        if not self.ks['beam']:
            return 0
        b = self.ks['beam'] + 0x10 * (self.beam_n % 256)
        self.beam_n += 1
        return b

    def play_loop(self, c):
        self.check(c.arg(0) == 0x3f800000)
        self.log(19, c.ecx)
        self.sound_n += 1
        return 0x7a000000 + 0x10 * self.sound_n

    def play_at(self, c):
        self.check(c.arg(0) == 0x3f800000 and c.arg(1) == 0)
        self.log(20, c.ecx, *self.vec(c.edx))
        self.sound_n += 1
        return 0x7a000000 + 0x10 * self.sound_n

    def log(self, *e):
        self.events.append(tuple(s32(v) for v in e))
        return 0

    def events_with_radar(self):
        """The spawn log with the radar hand-offs 0x4b4a40 made in memory."""
        out = list(self.events)
        for pos, blk in reversed(self.spawned):
            fn = self.u32(blk + 0x5d0)
            if fn:
                self.check(self.u32(blk + 0x5d4) == blk and fn in (0x4b4a20, 0x4b4a30))
                out.insert(pos, (13, s32(blk), int(fn == 0x4b4a30)))
        return out


class World:
    """The world for one manager: the same values for the emulator's memory and
    the probe's tables."""

    def __init__(self, emu, rng, mech):
        self.emu, self.rng = emu, rng
        e = emu
        self.v = {k: 0 for k in W}
        v = self.v
        v['vehicle'], v['entity'] = e.V, e.E
        v['mech'] = e.M if mech else 0
        v['part_mech'] = e.M if mech or rng.random() < 0.5 else 0
        v['player'] = rng.choice([v['mech'], v['mech'], e.MP]) if mech else e.MP
        v['mission_player'] = rng.choice([v['mech'], v['mech'], e.MP]) if mech else e.MP
        v['vehicle_node'] = 0x6100
        v['entity_node'] = 0x6200
        v['x64'] = e.E + 0x64
        v['stats'] = int(rng.random() < 0.5)
        v['lock_targets'] = int(rng.random() < 0.5)
        v['team'] = rng.choice([0, 1, 2, 3])
        v['allies'] = rng.choice([0, 1, 2, 6])
        v['spawn'] = e.spawn_base if rng.random() < 0.9 else 0
        v['beam'] = 0x7c000000 if rng.random() < 0.95 else 0
        v['highlight'] = -1
        v['clock'] = f2i(f32(rng.uniform(0, 300)))
        v['dt'] = f2i(f32(rng.choice([1 / 30, 1 / 60, 0.1, 0.05, 0.0005])))
        for k in ('vel', 'vel1', 'vel2'):
            v[k] = f2i(f32(rng.uniform(-20, 20)))
        v['heat'] = f2i(f32(rng.choice([0.0, 0.3, 0.6, 1.0, 1.4, 2.0])))
        v['net_id1'], v['net_id2'] = 0x5501, 0x5502
        v['net_t1'], v['net_t2'] = e.NET
        v['mp_id'] = 0x7fff0000
        # parts by id: (part, node, node id, water, fail, axis3, offset3, equip, named)
        self.parts = []
        for pid in range(16):
            present = True  # a missing firing part crashes both (0x4b2708)
            axis = [f32(rng.uniform(-1, 1)) for _ in range(3)]
            off = [rng.randrange(-16, 17) / 8 for _ in range(3)]
            self.parts.append([e.parts[pid] if present else 0, e.nodes[pid] if rng.random() < 0.95 else 0,
                               pid if rng.random() < 0.95 else rng.randrange(16), int(rng.random() < 0.08),
                               int(rng.random() < 0.03), *axis, *off, int(rng.random() < 0.3),
                               int(rng.random() < 0.5)])
        # firepoints: (radius, share, part, model_part, pos3)
        self.fps = []
        for i in range(rng.randrange(2, 12)):
            part = rng.choice([1, 2, 3, 6, 7, 8, 2, 3])
            mp = part if rng.random() < 0.9 else rng.randrange(1, 16)
            self.fps.append([f32(rng.choice([0.5, 0.3, 0.2, 0.0])), f32(rng.choice([0.5, 0.3, 0.0])), part, mp,
                             *[rng.randrange(-24, 25) / 8 for _ in range(3)]])
        self.items = []
        self.script(rng)

    def script(self, rng):
        e, v = self.emu, self.v
        v['owner_x58'] = int(rng.random() < 0.1)
        v['fcs'] = rng.choice([0, 0, 1, 2])
        v['control'] = rng.choice([0, 1, 1])
        v['screen'] = int(rng.random() < 0.8)
        v['view'] = e.VO if rng.random() < 0.8 else 0
        v['view_kind'] = rng.choice([0, 1, 2, 2, 2, 5])
        v['torso_aim'] = e.QA
        v['torso_look'] = e.TO + 0xa4 if rng.random() < 0.8 else 0
        for k in ('rot_a', 'rot_b'):
            q = [rng.uniform(-1, 1) for _ in range(4)]
            n = sum(x * x for x in q) ** 0.5 or 1
            for s, x in zip(('', '1', '2', '3'), q):
                v[k + s] = f2i(f32(x / n))
        has_t = rng.random() < 0.7
        v['record'] = e.R if rng.random() < 0.9 else 0
        v['target'] = e.T if has_t else 0
        v['target_mech'] = e.TM if rng.random() < 0.7 else 0
        v['tm_ecm'] = rng.choice([0, 0, 1])
        v['entity_id'] = rng.randrange(1, 1000)
        v['target_x10'] = rng.randrange(1, 1000)
        v['target_key'] = rng.choice([0x6900, 0x6904])
        v['target_proj'] = rng.choice([0, 0x6a00])
        v['heat'] = f2i(f32(rng.choice([0.0, 0.3, 0.6, 1.0, 1.4, 2.0, f32(rng.uniform(0, 2))])))
        v['clock'] = f2i(f32(i2f(v['clock']) + rng.choice([0.0, 0.05, 0.1, 0.3, 1.0])))
        v['time'] = f2i(f32(i2f(v['time']) + rng.choice([0.0, 0.05, 0.5, 1.0, 3.0])))
        v['dt'] = f2i(f32(rng.choice([1 / 30, 1 / 60, 0.1, 0.05, 0.0005, 0.25])))
        for p in self.parts:
            if rng.random() < 0.05:
                p[3] ^= 1
        n = self.emu.u32(self.emu.mgr + 0x24) if getattr(self.emu, 'mgr', 0) else 0
        v['highlight'] = rng.randrange(-1, n) if n else -1

    def push(self):
        """Writes the world to the emulator's memory and to the probe."""
        e, v, cpu = self.emu, self.v, self.emu.cpu
        e.world_parts = self.parts
        for name in ('entity', 'mech', 'part_mech', 'owner_x58', 'target_key', 'target_x10', 'target_mech', 'target_proj', 'view',
                     'entity_id', 'screen', 'torso_aim', 'torso_look', 'spawn', 'beam', 'mp_id'):
            e.setk(name, v[name])
        cpu.write(e.QA, struct.pack('<4I', *(v['rot_a' + s] for s in ('', '1', '2', '3'))))
        cpu.write(e.TO + 0xa4, struct.pack('<4I', *(v['rot_b' + s] for s in ('', '1', '2', '3'))))
        e.setk('heat', v['heat'])
        e.setk('TT', e.TT)
        e.setk('Y', e.Y)
        e.setk('X', e.X)
        cpu.u32(e.PL + 0x64, v['player'])
        cpu.u32(e.MS + 0x14, v['mission_player'])
        cpu.u32(0x608620, e.stats_obj if v['stats'] else 0)
        cpu.write(0x605fb4, bytes([v['lock_targets']]))
        cpu.u32(0x72c530, v['clock'])
        cpu.u32(0x72c52c, v['dt'])
        cpu.u32(0x60991c, v['time'])
        cpu.u32(e.V + 4, v['vehicle_node'])
        cpu.write(e.V + 0x30, struct.pack('<3I', v['vel'], v['vel1'], v['vel2']))
        cpu.u32(e.E + 4, v['entity_node'])
        cpu.u32(e.E + 0x6c, v['record'])
        cpu.u32(e.E + 0xc8, v['allies'])
        cpu.u32(e.Y + 0xc0, v['team'])
        for obj in (e.M, e.MP):
            cpu.u32(obj + 0x3b8, v['fcs'])
        cpu.u32(e.TM + 0x3cc, v['tm_ecm'])
        cpu.u32(e.CT + 0xc, v['control'])
        cpu.write(e.TO + 0xa2, bytes([1 if v['torso_look'] else 0]))
        cpu.u32(e.VO + 0x40, v['view_kind'])
        cpu.u32(e.R + 4, v['target'])
        slot = v['highlight']
        w = e.u32(e.mgr + 0x40 + 4 * slot) if slot >= 0 and getattr(e, 'mgr', 0) else 0
        cpu.u32(e.S + 0x1795c, w)
        # net list [0x605f84]: sentinel and two entries whose +8 objects carry +0x6fc ids
        head, n1, n2 = e.list_nodes[0], e.list_nodes[1], e.list_nodes[2]
        ring = [head, n1, n2]
        for i, nd in enumerate(ring):
            cpu.u32(nd, ring[(i + 1) % 3])
            cpu.u32(nd + 4, ring[i - 1])
        cpu.u32(n1 + 8, e.NET[0])
        cpu.u32(n2 + 8, e.NET[1])
        cpu.u32(0x605f84, head)
        cpu.u32(e.NET[0] + 0x6fc, v['net_id1'])
        cpu.u32(e.NET[1] + 0x6fc, v['net_id2'])
        for pid, row in enumerate(self.parts):
            part, node = e.parts[pid], e.nodes[pid]
            cpu.u32(part + 8, row[1])
            cpu.write(part + 0x49, bytes([row[3]]))
            cpu.u32(node + 0x28, row[2])
            cpu.write(e.ndata[pid] + 0x78, struct.pack('<3f', *row[5:8]))
            cpu.write(node, b'fp\0' if row[12] else b'nofp\0')
        # the probe
        vals = []
        for k in W:
            vals.append(v[k] if k != 'net_id' else 0)
        vals[WI['net_id']] = 0
        ints = (ctypes.c_int32 * len(W))(*[s32(x) for x in vals])
        rows = []
        for row in self.parts:
            rows += [i2f(row[0]), i2f(row[1]), i2f(row[2]), i2f(row[3]), i2f(row[4]), *row[5:11], i2f(row[11]),
                     i2f(row[12])]
        probe.probe_wm_world(ints, (ctypes.c_float * len(rows))(*rows), 16)


def build_dfn(emu, world):
    cpu = emu.cpu
    arr = emu.zalloc(4 * max(1, len(world.fps)))
    for i, fp in enumerate(world.fps):
        m = emu.fp_mem[i]
        cpu.write(m, struct.pack('<IffiI3f', emu.fp_name, fp[0], fp[1], fp[2], fp[3], *fp[4:7]))
        cpu.u32(arr + 4 * i, m)
    cpu.u32(emu.VEC + 4, arr)
    cpu.u32(emu.VEC + 8, arr + 4 * len(world.fps))


def build_config(emu, items):
    cpu = emu.cpu
    pool = list(emu.list_nodes[3:])
    head = pool.pop(0)
    mems = []
    for n, (kind, count, group, part, ident, extra) in enumerate(items):
        it = emu.cfg_items[n]
        cpu.write(it, bytes(0x20))
        cpu.u32(it, {1: VT_WEAPON_ITEM, 2: VT_AMMO_ITEM, 3: VT_EQUIP_ITEM}[kind])
        cpu.u32(it + 4, count)
        cpu.u32(it + 8, group)
        cpu.u32(it + 0xc, part)
        cpu.u32(it + 0x14, ident if kind != 3 else 0)
        cpu.u32(it + 0x18, extra)
        mems.append(it)
    ring = [head] + [pool.pop(0) for _ in mems]
    for i, nd in enumerate(ring):
        cpu.u32(nd, ring[(i + 1) % len(ring)])
        cpu.u32(nd + 4, ring[i - 1])
    for nd, it in zip(ring[1:], mems):
        cpu.u32(nd + 8, it)
    cpu.u32(emu.cfg + 8, head)
    cpu.u32(emu.cfg + 0xc, len(mems))


# --- state ------------------------------------------------------------------------
WEAPON_FIELDS = [(0x04, 'i'), (0x08, 'i'), (0x0c, 'i'), (0x10, 'i'), (0x14, 'i'), (0x18, 'i'), (0x1c, 'i'),
                 (0x20, 'i'), (0x24, 'i'), (0x28, 'i'), ('def', 'x'), (0x30, 'i'), (0x34, 'i'), (0x38, 'i'),
                 (0x3c, 'i'), (0x40, 'i'), (0x44, 'i'), (0x48, 'i'), (0x4c, 'i'), (0x50, 'i'), (0x54, 'i'),
                 ('bin', 'x'), (0x5c, 'b'), (0x5d, 'b'), (0x60, 'i'), (0x64, 'i'), (0x68, 'i'), (0x6c, 'i'),
                 (0x70, 'i'), (0x74, 'b'), (0x78, 'i'), (0x7c, 'i'), ('fp', 'x'), (0x84, 'i'), (0x88, 'i'),
                 (0x8c, 'i'), ('item', 'x')]
LABELS = (['trigger_down', 'trigger_was', 'state', 'alpha_end', 'aim.x', 'aim.y', 'aim.z', 'aim_valid', 'mode',
           'destroyed', 'count', 'group', 'owner', 'vehicle', 'has_target', 'online', 'ammo_value',
           'ammo_value_inv'])
WLABELS = ['lock_flag', 'lock_target', 'lock_id', 'lock_x10', 'proj_target', 'rating', 'slot', 'node', 'part',
           'index', 'def', 'reload', 'target.x', 'target.y', 'target.z', 'muzzle.x', 'muzzle.y', 'muzzle.z',
           'dir.x', 'dir.y', 'dir.z', 'bin', 'locked', 'fcs', 'group', 'salvo', 'salvo_left', 'x6c', 'damage',
           'enabled', 'loop_sound', 'recharge_sound', 'firepoint', 'spread.x', 'spread.y', 'spread.z', 'item',
           'firing', 'beam', 'bin.refs', 'bin.amount', 'bin.max', 'bin.infinite', 'bin.warned', 'bin.manager',
           'bin.damage']


def read_state(emu, world):
    cpu, u = emu.cpu, emu.u32
    m = emu.mgr

    def b(p):
        return cpu.read(p, 1)[0]
    o = [b(m), b(m + 1), s32(u(m + 4)), u(m + 8), u(m + 0xc), u(m + 0x10), u(m + 0x14), b(m + 0x18),
         s32(u(m + 0x1c)), s32(u(m + 0x20)), u(m + 0x24), s32(u(m + 0x28)), u(m + 0x80), u(m + 0x84), b(m + 0x88),
         b(m + 0x89), u(m + 0x8c), u(m + 0x90)]
    n = u(m + 0x24)
    weapons = [u(m + 0x40 + 4 * i) for i in range(n)]

    def slot(w):
        if w == 0:
            return -1
        return weapons.index(w) if w in weapons else -2

    for g in range(5):
        gp = u(m + 0x2c + 4 * g)
        cnt = u(gp + 8)
        o += [s32(u(gp + 4)), cnt, s32(u(gp + 0xc)), slot(u(gp + 0x10))]
        o += [slot(u(gp + 0x14 + 4 * i)) for i in range(cnt)]
    fps = [emu.fp_mem[i] for i in range(len(world.fps))]
    items = emu.weapon_items

    def bin_token(bp):
        if bp == 0:
            return -1
        for i, w in enumerate(weapons):
            if u(w + 0x58) == bp:
                return i
        return -2

    for i, w in enumerate(weapons):
        for off, kind in WEAPON_FIELDS:
            if off == 'def':
                o.append(emu.def_index(u(w + 0x2c)))
            elif off == 'bin':
                o.append(bin_token(u(w + 0x58)))
            elif off == 'fp':
                p = u(w + 0x80)
                o.append(fps.index(p) if p in fps else (-1 if p == 0 else -2))
            elif off == 'item':
                p = u(w + 0x90)
                o.append(items.index(p) if p in items else (-1 if p == 0 else -2))
            elif kind == 'b':
                o.append(b(w + off))
            else:
                o.append(s32(u(w + off)))
        if u(w) == 0x5c90c0:
            o += [b(w + 0x98), s32(u(w + 0xa0))]
        else:
            o += [0, 0]
        bp = u(w + 0x58)
        if bp and bin_token(bp) == i:
            o += [s32(u(bp)), s32(u(bp + 4)), s32(u(bp + 8)), b(bp + 0xc), b(bp + 0xd), int(u(bp + 0x10) != 0),
                  s32(u(bp + 0x14))]
        else:
            o += [0] * 7

    def wtoken(p):
        if p == 0:
            return -1
        for i, w in enumerate(weapons):
            if p == w + 4:
                return i * 0x100 + 4
            if p == w + 8:
                return i * 0x100 + 8
        return -2
    o += [wtoken(u(0x9a0ee0)), wtoken(u(0x9a0ee4)), s32(u(0x9a17cc)), s32(u(0x9a17d0)), s32(u(0x9a17d4)),
          s32(u(0x9a17d8)), s32(u(0x9a17dc)), s32(u(0x9a17e0)), s32(u(0x60b264)), cpu.read(0x60b21c, 1)[0] & 1]
    for k in range(emu.ndefs):
        d = emu.defs_base + 0x1f4 * k
        p = u(d + 0xdc)
        tok = -1 if p == 0 else next((0x100 + i for i, fp in enumerate(fps) if p == fp + 0x14), -2)
        o += [s32(u(d + 0xd0)), s32(u(d + 0xd4)), s32(u(d + 0xd8)), tok]
    for k in range(emu.ndefs):
        t = u(emu.defs_base + 0x1f4 * k + 0x1f0)
        if not t:
            continue
        head = u(t + 0x84)
        marks, nd = [], u(head)
        while nd != head:
            r = u(nd + 8)
            marks += [u(r), u(r + 4), u(r + 8), u(r + 0xc)]
            nd = u(nd)
        o += [u(t + 0x88)] + marks + [u(t + 0xa4), u(t + 0xa8), u(t + 0xac), u(t + 0xc4)]
    return [s32(x) for x in o]


def read_port():
    buf = (ctypes.c_int32 * 20000)()
    n = probe.probe_wm_state(buf, 20000)
    assert n >= 0
    return list(buf[:n])


def port_log():
    buf = (ctypes.c_int32 * 100000)()
    n = probe.probe_wm_log(buf, 100000)
    assert n >= 0
    return zr.port_events(buf, n)


def labels(a):
    """The name of each item of a state dump (read_state's layout)."""
    out = list(LABELS)
    i = len(out)
    for g in range(5):
        cnt = a[i + 1] if i + 1 < len(a) else 0
        out += [f'group{g}.index', f'group{g}.count', f'group{g}.current', f'group{g}.current_weapon']
        out += [f'group{g}.weapon{k}' for k in range(cnt)]
        i = len(out)
    n = a[10]
    for w in range(n):
        out += [f'weapon{w}.{x}' for x in WLABELS]
    out += ['zwep.lock_flag', 'zwep.lock_target', 'zwep.beam_ground_fx', 'zwep.damage_scale',
            'zwep.beam_range_scale', 'zwep.track0', 'zwep.track1', 'zwep.terrain_follow', 'cos35', 'cos35_made']
    for d in range(64):
        out += [f'def{d}.fire.{x}' for x in ('sound_choice', 'animation_choice', 'attach_node', 'attach_offset')]
    return out


def describe(a, b):
    """Names the first fields that differ in two state dumps."""
    names = labels(a)
    for i, (x, y) in enumerate(zip(a, b)):
        if x != y:
            name = names[i] if i < len(names) else f'item {i}'
            return f'{name} ({x:#x} vs {y:#x}; {i2f(x)!r} vs {i2f(y)!r})'
    return f'lengths {len(a)} vs {len(b)}'


def show(events):
    return '\n    '.join(f'{NAMES.get(e[0], e[0])}{tuple(hex(x & 0xffffffff) for x in e[1:])}' for e in events)


# --- operations ----------------------------------------------------------------------
class Run:
    def __init__(self, emu, rng):
        self.emu, self.rng = emu, rng

    def new_manager(self):
        emu, rng, cpu = self.emu, self.rng, self.emu.cpu
        mech = rng.random() < 0.9
        self.world = w = World(emu, rng, mech)
        build_dfn(emu, w)
        items = []
        for _ in range(rng.randrange(0, 14)):
            kind = rng.choice([2, 2, 2, 3, 1])
            part = rng.choice([1, 2, 3, 6, 7, 8, -1, 4])
            if kind == 3:
                items.append((3, 1, 0, part, 0, rng.choice([5, 5, 3, 0, 11])))
            else:
                items.append((kind, rng.choice([1, 20, 40]), 0, part, rng.randrange(37), 1))
        w.items = items
        build_config(emu, items)
        for k in range(emu.ndefs):  # a fire effect left attached to the last manager's firepoints
            cpu.write(emu.defs_base + 0x1f4 * k + 0xd8, bytes(8))
        emu.mgr = emu.zalloc(0x94)
        emu.spawn_n = emu.beam_n = emu.sound_n = 0
        emu.weapon_items = []
        w.push()
        seed = rng.randrange(1 << 31)
        emu.rand = MsvcRand(seed)
        flat = [x for fp in w.fps for x in fp + [0.0]]
        iflat = [x for it in items for x in it]
        fbuf = (ctypes.c_float * max(1, len(flat)))()
        for i, x in enumerate(flat):
            fbuf[i] = x if i % 8 not in (2, 3) else i2f(x)
        probe.probe_wm_new(fbuf, len(w.fps), (ctypes.c_int32 * max(1, len(iflat)))(*iflat), len(items), seed)
        emu.events, emu.spawned = [], []
        cpu.call(F['manager_ctor'], ecx=emu.mgr, stack_args=(emu.V,))
        probe.probe_wm_reset_fire()
        # the zWeapon fire state as the startup 0x583750 leaves it
        for addr, val in ((0x9a0ee0, 0), (0x9a0ee4, 0), (0x9a17cc, 1), (0x9a17d0, 0x3f800000), (0x9a17d4, 0x3f800000),
                          (0x9a17d8, 0x3f800000), (0x9a17dc, 0x3f800000), (0x9a17e0, 0)):
            cpu.u32(addr, val)
        self.compare('new manager', 0, 0)

    def compare(self, label, a_res, b_res):
        emu = self.emu
        a_ev, b_ev = emu.events_with_radar(), port_log()
        sa, sb = read_state(emu, self.world), read_port()
        if a_ev != b_ev or sa != sb or a_res != b_res:
            first = next((i for i, (x, y) in enumerate(zip(a_ev, b_ev)) if x != y), min(len(a_ev), len(b_ev)))
            raise SystemExit(f'{label}: result {a_res} vs {b_res}; state {describe(sa, sb) if sa != sb else "same"}'
                             f"\n  calls differ at {first} ({len(a_ev)} vs {len(b_ev)}):\n  original:\n    {show(a_ev[first:first + 6])}"
                             f'\n  port:\n    {show(b_ev[first:first + 6])}')
        emu.events, emu.spawned = [], []
        return len(a_ev)

    def op(self, op, args):
        emu, cpu = self.emu, self.emu.cpu
        self.world.push()
        emu.events, emu.spawned = [], []
        m = emu.mgr
        u = emu.u32
        res = 0
        al = lambda: cpu.uc.reg_read(UC_X86_REG_EAX) & 0xff  # noqa: E731
        eax = lambda: s32(cpu.uc.reg_read(UC_X86_REG_EAX))  # noqa: E731
        if op == 0:
            count, group, fpi, weapon, tech = args
            it = emu.items_mem[len(emu.weapon_items) % 64]
            cpu.write(it, bytes(0x20))
            cpu.write(it, struct.pack('<IIiiIii', VT_WEAPON_ITEM, count, group, fpi, 0, weapon, tech))
            emu.weapon_items.append(it)
            cpu.call(F['add'], ecx=m, stack_args=(it,))
            res = emu.slot_of(cpu.uc.reg_read(UC_X86_REG_EAX))
        elif op == 1:
            cpu.call(F['find'], ecx=m, stack_args=(args[0], 0))
            w = cpu.uc.reg_read(UC_X86_REG_EAX)
            if w and u(w + 0x58):
                cpu.call(F['bin_add'], ecx=u(w + 0x58), stack_args=(args[1] & 0xffffffff,))
            res = emu.slot_of(w)
        elif op == 2:
            cpu.call(F['set_trigger'], ecx=m, stack_args=(args[0],))
            cpu.call(F['update'], ecx=m, stack_args=(args[1],))
        elif op == 3:
            cpu.call(F['select_group'], ecx=m, stack_args=(args[0],))
            res = al()
        elif op in (4, 5):
            cpu.call(F['next_group' if op == 4 else 'prev_group'], ecx=m)
            res = al()
        elif op in (6, 7):
            cpu.call(F['next_weapon' if op == 6 else 'prev_weapon'], ecx=m)
        elif op == 8:
            cpu.u32(m + 0x1c, (u(m + 0x1c) + 1) % 3)
            cpu.call(F['mode_sound'], ecx=m)
        elif op == 9:
            cpu.call(F['alpha'], ecx=m, stack_args=(args[0],))
        elif op == 10:
            cpu.call(F['jettison'], ecx=m)
            res = al()
        elif op == 11:
            p = emu.zalloc(12)
            cpu.write(p, struct.pack('<3I', *args[1:4]))
            cpu.call(F['set_aim'], ecx=m, stack_args=(p if args[0] else 0,))
        elif op == 12:
            cpu.call(F['set_online'], ecx=m, stack_args=(args[0],))
        elif op == 13:
            cpu.call(F['critical'], ecx=m, stack_args=(args[0] & 0xffffffff, args[1]))
            res = al()
        elif op == 14:
            cpu.call(F['part_lost'], ecx=m, stack_args=(args[0],))
        elif op == 15:
            cpu.call(F['destroy_all'], ecx=m)
        elif op == 16:
            cpu.call(F['unlimited'], ecx=m, stack_args=(args[0],))
        elif op == 17:
            cpu.call(F['arms'], ecx=m)
            res = al()
        elif op == 18:
            cpu.call(F['net_fire'], ecx=m, stack_args=(args[0], args[1], args[2]))
            res = al()
        elif op == 19:
            if args[0] < u(m + 0x24):
                cpu.call(F['set_group'], ecx=u(m + 0x40 + 4 * args[0]), stack_args=(args[1] & 0xffffffff,))
                res = al()
        elif op == 20:
            cpu.call(F['forget'], ecx=m, stack_args=(args[0],))
        elif op == 21:
            if args[0] < u(m + 0x24):
                cpu.call(F['add_damage'], ecx=u(m + 0x40 + 4 * args[0]), stack_args=(args[1],))
        elif op == 22:
            cpu.call(F['reset'], ecx=m)
            # zWeapon's lock pointers may point into the freed weapons now; the port's
            # allocator reuses that memory, so both are cleared here
            cpu.u32(0x9a0ee0, 0)
            cpu.u32(0x9a0ee4, 0)
        elif op == 23:
            cpu.write(m + 0x88, bytes([args[0]]))
        elif op == 24:
            cpu.call(F['add_vehicle'], ecx=m, stack_args=(args[0], args[1], 0, args[2] & 0xffffffff))
            res = emu.slot_of(cpu.uc.reg_read(UC_X86_REG_EAX))
        elif op == 25:
            if args[0] < u(m + 0x24):
                res = self.query(u(m + 0x40 + 4 * args[0]))
        elif op in (26, 27, 28):
            d = emu.defs_base + 0x1f4 * args[0]
            hit, p = emu.hit, emu.proj
            cpu.write(hit + 0xc, struct.pack('<3I', *args[1:4]))
            cpu.u32(p + 4, args[4])
            cpu.write(p + 0x3c, struct.pack('<6I', *args[5:11]))
            cpu.u32(p + 0x58c, args[11])
            fn = {26: 0x4afe70, 27: 0x4afea0, 28: 0x4b44d0}[op]
            cpu.call(fn, ecx=d, edx=hit, stack_args=(p,))
            res = eax()
        elif op == 29:
            if args[0] < u(m + 0x24):
                w = u(m + 0x40 + 4 * args[0])
                cpu.call(0x4b4ba0, ecx=w)
                emu.log(31, al())
                t = u(u(w + 0x2c) + 0x1f0) if u(w + 0x2c) else 0
                if args[1] and t:
                    cpu.call(0x4b4c90, ecx=t)
            if args[2]:
                cpu.call(0x4b4bd0)
            cpu.call(0x4b4900, ecx=args[3], edx=args[4])
            emu.log(32, eax())
        b_res = probe.probe_wm_op(op, (ctypes.c_int32 * 16)(*[s32(x) for x in list(args) + [0] * (16 - len(args))]))
        if op in (3, 4, 5, 10, 13, 17, 18, 19):
            b_res &= 0xff
        if op == 25:
            b_res = 0
        return self.compare(f'op {op} {args}', res, b_res)

    def query(self, w):
        """vt+0x10 dps, +0x34 ready, +0x14 ammo, can fire, the range getters."""
        emu, cpu = self.emu, self.emu.cpu
        vt = emu.u32(w)
        out = []
        for slot in (0x10, 0x34):
            cpu.call(emu.u32(vt + slot), ecx=w)
            cpu.call(emu.fstp)
            out.append(emu.f(emu.fslot))
        cpu.call(emu.u32(vt + 0x14), ecx=w)
        out.append(s32(cpu.uc.reg_read(UC_X86_REG_EAX)))
        cpu.call(F['can_fire'], ecx=w)
        out.append(cpu.uc.reg_read(UC_X86_REG_EAX) & 0xff)
        for fn in ('min_range', 'max_range', 'max_range_sq'):
            cpu.call(F[fn], ecx=w)
            cpu.call(emu.fstp)
            out.append(emu.f(emu.fslot))
        emu.log(30, *out)
        return 0

    def callback_op(self):
        """A def callback as zWeapon's impact runs it: the salvo spread on a
        missile def, TAG's airstrike, an LB-X burst."""
        rng, emu = self.rng, self.emu
        kind = rng.choice([26, 26, 27, 28, 28])
        names = {26: ['LRM20', 'LRM10', 'SRM6', 'StreakSRM4', 'LRM5'], 27: ['TAG'],
                 28: ['LB2XAC', 'LB5XAC', 'LB10XAC', 'LB20XAC']}[kind]
        d = emu.def_names[rng.choice(names)]
        hit = [f2i(f32(rng.uniform(-200, 200))) for _ in range(3)]
        if kind == 26 and emu.last_hit and rng.random() < 0.6:
            hit = [f2i(f32(i2f(x) + rng.uniform(-5, 5))) for x in emu.last_hit]
        emu.last_hit = hit
        dv = [rng.uniform(-1, 1) for _ in range(3)]
        n = sum(x * x for x in dv) ** 0.5 or 1
        dv = [f2i(f32(x / n)) for x in dv]
        if rng.random() < 0.1:
            dv[0] = 0
        pos = [f2i(f32(rng.uniform(-200, 200))) for _ in range(3)]
        if kind == 26 and emu.last_pos and rng.random() < 0.6:  # the salvo marks key on the shot's position
            pos = [f2i(f32(i2f(x) + rng.uniform(-6, 6))) for x in emu.last_pos]
        emu.last_pos = pos
        speed = f2i(f32(rng.choice([400.0, 0.0, rng.uniform(50, 900)])))
        return kind, (d, *hit, 0x6300, *dv, *pos, speed)

    def random_op(self):
        rng, emu = self.rng, self.emu
        n = emu.u32(emu.mgr + 0x24)
        r = rng.random()
        if r < 0.10 and n < 15:
            pool = MECH_WEAPONS
            return 0, (rng.choice([1, 1, 2]), rng.randrange(-1, 7), rng.randrange(len(self.world.fps)),
                       rng.choice(pool), rng.choice([1, 2]))
        if r < 0.14:
            have = [emu.u32(emu.u32(emu.mgr + 0x40 + 4 * i) + 0x28) for i in range(n)]
            idx = rng.choice(have) if have and rng.random() < 0.8 else rng.randrange(37)
            return 1, (idx, rng.choice([1, 5, 20, 40, 100, 0]))
        no_group = emu.u32(emu.mgr + 0x28) == 0xffffffff  # the original calls through a null group then
        if r < 0.50:
            dt = f2i(f32(rng.choice([1 / 30, 1 / 60, 0.1, 0.0005, 0.25, 0.5, 2.0])))
            down = int(rng.random() < 0.65 and not no_group)
            return 2, (down, dt)
        if r < 0.55:
            return 3, (rng.randrange(5),)
        if r < 0.58:
            return rng.choice([4, 5]), ()
        if r < 0.61 and emu.u32(emu.mgr + 0x28) != 0xffffffff:
            return rng.choice([6, 7]), ()
        if r < 0.63:
            return 8, ()
        if r < 0.65:
            return 9, (f2i(f32(rng.choice([0.2, 0.0, 1.0]))),)
        if r < 0.67 and emu.u32(emu.mgr + 0x28) != 0xffffffff:
            return 10, ()
        if r < 0.71:
            return 11, (int(rng.random() < 0.9), *[f2i(f32(rng.uniform(-300, 300))) for _ in range(3)])
        if r < 0.72:
            return 12, (int(rng.random() < 0.8),)
        if r < 0.75 and self.world.v['mech']:
            return 13, (rng.choice([-1, 1, 2, 3, 6, 7, 8, 4]), int(rng.random() < 0.5))
        if r < 0.76:
            return 14, (rng.choice([1, 2, 3, 6, 7, 8]),)
        if r < 0.765:
            return 15, ()
        if r < 0.78:
            return 16, (int(rng.random() < 0.5),)
        if r < 0.82:
            return 17, ()
        if r < 0.85 and n and not no_group:
            return 18, (rng.randrange(n + 1), rng.choice([0, 0x5501, 0x5502, 0x5509]), rng.randrange(5))
        if r < 0.88 and n:
            return 19, (rng.randrange(n), rng.randrange(5))  # group -1 crashes both later (0x4b0745)
        if r < 0.89:
            return 20, (self.emu.T,)  # a null target crashes both (its vt+0x24)
        if r < 0.91 and n:
            return 21, (rng.randrange(n), f2i(f32(rng.choice([0.2, 0.5, 1.0, -0.3]))))
        if r < 0.912:
            return 22, ()
        if r < 0.925:
            return self.callback_op()
        if r < 0.94:
            return 23, (int(rng.random() < 0.7),)
        if r < 0.95 and n < 15 and not self.world.v['mech']:
            pid = rng.randrange(1, 16)
            node = self.world.parts[pid][1] or self.emu.nodes[pid]
            # beams on a vehicle without a mech crash both (no part, no heat), 0x4b4067
            return 24, (rng.choice([i for i in MECH_WEAPONS if i not in BEAMS] + [-1]), node, rng.randrange(5))
        if n and rng.random() < 0.5:
            return 25, (rng.randrange(n),)
        if rng.random() < 0.5:
            return 29, (rng.randrange(max(n, 1)), int(rng.random() < 0.5), int(rng.random() < 0.1),
                        rng.randrange(-1, 38), rng.choice([0, 5, 21, 100, 151, 300]))
        return 2, (0, f2i(f32(1 / 30)))


def main():
    emu = Emulated()
    rng = random.Random(2077)
    run = Run(emu, rng)
    managers = int(os.environ.get('WEAPON_MANAGERS', '600'))
    ops = calls = 0
    for k in range(managers):
        run.new_manager()
        # a loadout first, as the mech's configuration does it, then a fight
        for _ in range(rng.randrange(1, 9)):
            op, args = 0, (1, rng.randrange(0, 5), rng.randrange(len(run.world.fps)), rng.choice(MECH_WEAPONS), 1)
            if run.world.v['mech']:
                calls += run.op(op, args)
            ops += 1
        for _ in range(rng.randrange(0, 6)):
            calls += run.op(1, (rng.randrange(37), rng.choice([10, 20, 40, 120])))
        if rng.random() < 0.9:
            calls += run.op(3, (rng.randrange(5),))
        for step in range(rng.randrange(20, 120)):
            if rng.random() < 0.3:
                run.world.script(rng)
            op, args = run.random_op()
            if op == 0 and not run.world.v['mech']:
                continue
            try:
                calls += run.op(op, args)
            except SystemExit as e:
                raise SystemExit(f'manager {k} step {step}: {e}')
            ops += 1
    missing = sorted(emu.cover[a] for a in set(emu.cover) - emu.reached)
    if missing:
        raise SystemExit(f'coverage: never reached {missing}')
    print(f'ok: {managers} managers, {ops} operations agree; {calls} calls out matched; '
          f'all {len(emu.cover)} paths reached')


if __name__ == '__main__':
    main()
