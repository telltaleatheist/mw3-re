"""The parts' setups and updates against the original x86
(docs/movement-ground.md, The part updates).

    cmake --build build && .venv/bin/python tests/differential/test_parts.py [trials]

A mech in emulator memory with its twelve parts on their real vtables, each
on a node (an object3d stand-in) whose 3x4 its +0x14 / +0x18 point into, the
skis, the controls on the real IControls vtable, the pilot and its soils,
the HUD screen, the weapons' target. Each trial makes a random mech and runs
one call: the hip's update 0x4623a0, a leg's 0x463ee0 (the foot's 0x463110:
the counter-spin, the touch and plant with their force feedback, splash,
footprint 0x462fa0 and step sound), a foot's ground update 0x462a10 (the
skis' height and water, the tilt 0x462de0, the ground states, the soil, what
it steps on), an arm's update 0x466510 (the aim 0x466840, the recoil
0x4666f0) or kick 0x466610, the torso's 0x464e80 (the AI's aim 0x465d10 /
0x465bc0, the player's rate 0x464ce0 and absolute modes, centring the legs,
the servo sound, the shake spring, the node 0x4659c0 / 0x465940), a hand's
base update 0x461730, the hip's, a leg's (with the calf's and foot's) and an
arm's (with the hand's) setups 0x462130, 0x4637b0, 0x462860, 0x4663e0,
0x466e80. Every field both sides keep, every node matrix, the controls'
throttle and turn, the return value and every call out must match bit for
bit at 24-bit x87, but for a footprint's first angle (the CRT asin through
matrix_to_euler, within 2e-6 as in test_matrix.py). A part without a node
reads its world position as zeros (the exe reads stale stack).
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
from original import FTOL, IAT_FTOL, ROOT, STACK_TOP, Original  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBP, UC_X86_REG_EIP, UC_X86_REG_ESI, UC_X86_REG_ESP  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
P = ctypes.c_void_p
probe.probe_parts_load.argtypes = [P, P, P, ctypes.c_uint32, P, P, P, P]
probe.probe_parts_op.argtypes = [ctypes.c_int, P]
probe.probe_parts_op.restype = ctypes.c_float
probe.probe_parts_store.argtypes = [P, P, P, P, P, ctypes.c_int]

IAT_ASIN = 0x5bb5f4
ASIN = bytes.fromhex('d9c0 d8c8 d9e8 dee1 d9fa d9f3 c3')  # test_matrix.py's _CIasin
MECH_VT, MECH_SIZE, IMAGE, CTL_VT, DEF_SIZE = 0x5bcef8, 0x6e0, 0x1b4, 0x5bd428, 0x1a8
LEG0, LEG1, CALF0, CALF1, FOOT0, FOOT1, HIP, TORSO, ARM0, ARM1, HAND0, HAND1 = range(12)
NPARTS, MECH_NODE, SHADOW0, FX_NODE0, NNODES = 12, 12, 13, 15, 17
IDS = [4, 5, 11, 12, 13, 14, 15, 1, 2, 3, 9, 10]
VTABLES = [0x5c2978, 0x5c2978, 0x5c2b18, 0x5c2b18, 0x5c2910, 0x5c2910, 0x5c28a8, 0x5c29e0, 0x5c2a48, 0x5c2a48,
           0x5c2ab0, 0x5c2ab0]
MECH_SLOTS = {LEG0: 0x38c, LEG1: 0x390, ARM0: 0x394, ARM1: 0x398, TORSO: 0x39c, HIP: 0x3a0}
SOIL_TABLE, CAMERA, MISSION, STATS = 0x608c00, 0x5f4c8c, 0x60861c, 0x608620
# env words, probe_parts.cpp's order.
FLAGS, SCREEN_FLAGS, TARGET, POSITION, MECH_MATRIX, EULER, LOOK, SOIL, STEPS, PRINTS, PLAYING, WORLD = (
    0, 1, 2, 5, 8, 20, 23, 26, 38, 49, 60, 63)
DUST = WORLD + 3 * NNODES  # roster, dust, splash, the mech node's flag 8, hit count, 8 x (height, material)
VMODE = DUST + 5 + 16  # the view's mode: bits 0..2 its id, bit 4 there at all
PARTSEL = VMODE + 1  # the part aim: 1 a target, 2 it is seen, 4 it is a mech (this one), 8 a targeting computer
TBOX = PARTSEL + 1  # the torso's setup: 1 torso_polys found (node 16), 2 it has a box; then the box's 6 floats
ENV = TBOX + 7
WORLD_OBJ = 0x608c84


def F(o, n=1):
    return [(o + 4 * k, 'f') for k in range(n)]


BASE = [(0x48, 'b'), (0x49, 'b'), (0x4a, 'b')] + F(0x4c, 7)
LEG = BASE + [(0x7c, 'b')] + F(0x80, 2 + 8 + 11)
FOOT = BASE + F(0x74) + F(0x7c, 3) + [(0x78, 'i'), (0x88, 'b')] + F(0x8c, 2) + [(0x94, 'i'), (0xac, 'b'), (0xc0, 'b')] + \
    F(0xc4, 4) + F(0xd4, 4) + [(0xe4, 'i'), (0xe8, 'i')]
HIP_F = BASE + F(0x74, 8) + F(0x94, 4) + F(0xa4, 3)
TORSO_F = BASE + [(0x84, 'i'), (0x88, 'i')] + F(0x8c, 4) + [(0x9c, 'i'), (0xa0, 'b'), (0xa1, 'b'), (0xa2, 'b')] + \
    F(0xa4, 10) + F(0xd0, 2) + F(0xd8, 6) + F(0xf0, 4) + F(0x100, 6) + F(0x118, 4) + F(0x128, 4) + F(0x138, 5) + \
    F(0x158, 2) + F(0x160, 6) + [(0x1b0, 'i')] + F(0x7c, 2) + F(0x14c, 3)
ARM_F = BASE + F(0x2c) + F(0x34) + [(0x74, 'i')] + F(0x78, 5) + F(0x8c, 3) + F(0x98, 2) + F(0xa4, 4) + F(0xb4, 6) + \
    [(0xcc, 'b')]
HAND_F = BASE + F(0x74, 4) + F(0x84, 3)
FIELDS = [LEG, LEG, BASE, BASE, FOOT, FOOT, HIP_F, TORSO_F, ARM_F, ARM_F, HAND_F, HAND_F]
MECH_FIELDS = F(0x24, 3) + F(0x30, 3) + F(0x3c, 3) + F(0x4bc) + F(0x508, 2) + F(0x518) + F(0x520) + F(0x534) + \
    [(0x584, 'b'), (0x585, 'b'), (0x586, 'b'), (0x5c8, 'i'), (0x5e4, 'i'), (0x5e8, 'i'), (0x688, 'i'),
     (0xdc, 'b'), (0xde, 'b')] + F(0xec, 3) + [(0xf8, 'i')]
CTL_FIELDS = [(0x08, 'i'), (0x0c, 'i')] + F(0x10, 2)


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


def u(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def u2f(x):
    return struct.unpack('<f', struct.pack('<I', x))[0]


def put(buf, off, kind, v):
    if kind == 'f':
        struct.pack_into('<f', buf, off, v)
    elif kind == 'b':
        buf[off] = int(v) & 0xff
    else:
        struct.pack_into('<I', buf, off, int(v) & 0xffffffff)


def pick(img, fields):
    out = bytearray(len(img))
    for off, kind in fields:
        n = 1 if kind == 'b' else 4
        out[off:off + n] = img[off:off + n]
    return bytes(out)


def get(img, off):
    return struct.unpack_from('<I', img, off)[0]


class World:
    """The original's side."""

    def __init__(self):
        cpu = self.cpu = Original()
        cpu.import_code(IAT_FTOL, FTOL)
        cpu.import_code(IAT_ASIN, ASIN)
        a = cpu.alloc
        self.mech, self.mvt = a(MECH_SIZE), a(0x200)
        self.parts = [a(IMAGE) for _ in range(NPARTS)]
        self.nodes = [a(0x100) for _ in range(NNODES)]
        self.data = [a(0x60) for _ in range(NNODES)]
        self.skis, self.ctl, self.vbt, self.pilot, self.soils = a(0x300), a(0x74), a(8), a(0x240), a(0x58)
        self.screen, self.weapons, self.dfn, self.transform, self.pos = a(0x1520), a(0x20), a(DEF_SIZE), a(48), a(12)
        self.look, self.lvt, self.ldata = a(0x80), a(0x20), a(0x80)
        # The view's mode for 0x00426780: a mech camera (vt+0x1c itself) or the base (null).
        self.vmode, self.vmvt = a(0x80), a(0x40)
        self.camera, self.mission, self.stats = a(0x80), a(0x20), a(0x10)
        self.mats = [a(0x24) for _ in range(12)]
        self.steps = [a(0xc0) for _ in range(7)]
        self.record = a(0x10)
        self.fx = [0, a(8), a(8)]
        self.slot = a(4)
        self.events = []
        cpu.write(self.mvt, cpu.read(MECH_VT, 0x200))
        self.vstub(self.mvt + 0x170, lambda c: self.log(35, c.arg(0)), 4)
        self.vstub(self.mvt + 0x10c, lambda c: self.log(41) or self.look, 4)
        self.vstub(self.lvt + 0x1c, lambda c: self.ldata, 0)
        cpu.u32(self.look, self.lvt)
        self.vstub(self.vmvt + 0x1c, lambda c: c.ecx, 0)
        s = cpu.stub_function
        s(0x4ed530, self.world_position)
        s(0x4ed280, self.node_to_world)
        s(0x4ed5d0, self.node_euler)
        s(0x4fc300, self.node_set)
        s(0x4eb170, lambda c: self.log(23, self.node_index(c.ecx), c.edx))
        s(0x45fb80, lambda c: self.log(24, self.fx_id(c.ecx)) or self.playing[self.fx_id(c.ecx)])
        s(0x45fb50, lambda c: self.log(25, self.fx_id(c.ecx)))
        s(0x45faf0, lambda c: self.log(26, self.fx_id(c.ecx), c.arg(0)), 4)
        s(0x4fc0b0, lambda c: self.log(27, self.node_index(c.ecx) - FX_NODE0 + 1, c.arg(0), c.arg(1), c.arg(2)), 12)
        s(0x45f980, lambda c: self.log(39) or 7, 4)
        # What a foot steps on returns a float (popped): log, then fldz; ret 4.
        self.fldz_ret4 = cpu.code(b'\xd9\xee\xc2\x04\x00')
        cpu.uc.hook_add(UC_HOOK_CODE, self.step_on, begin=0x5836b0, end=0x5836b0)
        s(0x5a85e0, lambda c: self.log(42) or self.pos)
        s(0x4337c0, lambda c: self.log(29, c.arg(0)), 4)
        s(0x508aa0, lambda c: self.log(30, c.ecx, c.edx, *[c.arg(k) for k in range(9)]), 36)
        s(0x56e7e0, lambda c: self.log(31, c.ecx, *struct.unpack('<3I', c.read(c.edx, 12)), c.arg(0)), 8)
        s(0x446520, lambda c: self.log(32))
        s(0x432990, self.servo, 20)
        s(0x56e9f0, lambda c: self.log(34, c.ecx))
        s(0x41b8d0, lambda c: self.log(36, self.part_index(c.arg(0))), 4)
        s(0x45fe00, lambda c: self.log(37, self.part_index(c.ecx)), 4)
        s(0x4f7db0, lambda c: self.log(38, self.node_index(c.ecx)))
        s(0x466df0, lambda c: self.log(40, self.part_index(c.ecx)), 4)
        s(0x41e9a0, lambda c: self.log(44) or self.dfn)
        s(0x4846a0, lambda c: self.log(45) or self.dfn)
        s(0x430aa0, lambda c: self.log(43) or (self.screen if self.flags >> 3 & 1 else 0))
        s(0x4edab0, self.ground_at, 12)
        s(0x4eb660, self.flag8_get)
        s(0x4eb620, lambda c: self.log(49, self.node_index(c.ecx), c.edx))
        s(0x508c10, lambda c: self.log(46, c.ecx, self.node_index(c.edx), c.arg(0), c.arg(1), c.arg(2)), 12)
        s(0x51b440, lambda c: self.log(50, c.arg(0), c.arg(1)), 8)  # zInput's mouse set (the centring)
        # The part aim: the target contact's mech (0x004e6ff0), a vehicle's
        # position (0x005a8630), a node's local point to the world (0x004ed650).
        self.contact = a(0x30)
        s(0x4e6ff0, lambda c: self.log(55) or (self.mech if self.env[PARTSEL] & 4 else 0))
        s(0x5a8630, lambda c: self.log(53, c.ecx) or self.pos)
        s(0x4ed650, self.world_point)
        # The torso's setup: torso_polys under its node (0x004f9d60), a node's box (0x004ec9c0).
        s(0x4f9d60, lambda c: self.log(56, self.node_index(c.ecx), int(c.read(c.edx, 12) == b'torso_polys\0'))
          or (self.nodes[16] if self.env[TBOX] & 1 else 0))
        self.tbox = a(24)
        s(0x4ec9c0, self.node_box)
        self.aim = a(24)
        self.world_obj = a(8)
        # 0x41d240 (ECX the mech, ECX kept) on a distance, its st0 kept.
        self.share_d = a(8)
        self.share = cpu.code(b'\xff\x35' + struct.pack('<I', self.share_d) + b'\xb8' + struct.pack('<I', 0x41d240) +
                              b'\xff\xd0\xd9\x1d' + struct.pack('<I', self.slot) + b'\xc3')
        # A thunk that pushes the ground update's 0 and keeps its st0.
        self.ground = cpu.code(b'\x6a\x00\xb8' + struct.pack('<I', 0x462a10) + b'\xff\xd0\xd9\x1d' +
                               struct.pack('<I', self.slot) + b'\xc3')

    def vstub(self, slot, fn, pop):
        addr = self.cpu.code(b'\xc3')
        self.cpu.u32(slot, addr)
        self.cpu.stub_function(addr, fn, pop)

    def log(self, *e):
        self.events.append(len(e))
        self.events.extend(v & 0xffffffff for v in e)

    def node_index(self, n):
        return self.nodes.index(n) if n in self.nodes else -1

    def part_index(self, p):
        return self.parts.index(p) if p in self.parts else -1

    def fx_id(self, e):
        return self.fx.index(e) if e in self.fx[1:] else e

    def ground_at(self, c):
        self.log(47, self.node_index(c.ecx), c.arg(0), c.arg(1), c.arg(2))
        e = self.env[DUST:]
        out = bytearray(0x504)
        out[0] = e[4]
        for k in range(e[4]):
            struct.pack_into('<I', out, 4 + 0x28 * k + 0x10, e[5 + 2 * k])
            m = e[6 + 2 * k]
            struct.pack_into('<I', out, 4 + 0x28 * k + 0x20, self.mats[m] if m else 0)
        c.write(c.edx, out)
        return 0

    def flag8_get(self, c):
        self.log(48, self.node_index(c.ecx))
        c.u32(c.edx, self.env[DUST + 3])
        return 0

    def run_at(self, start, until, regs):
        uc = self.cpu.uc
        for r_, v in regs.items():
            uc.reg_write(r_, v & 0xffffffff)
        uc.emu_start(start, until, count=5_000_000)
        if uc.reg_read(UC_X86_REG_EIP) != until:
            raise RuntimeError(f'{start:#x} did not reach {until:#x}')

    def step_on(self, uc, address, size, user):
        c = self.cpu
        self.log(28, (c.edx - self.skis - 0x194) // 0x28, *struct.unpack('<3I', c.read(c.ecx, 12)), c.arg(0))
        uc.reg_write(UC_X86_REG_EIP, self.fldz_ret4)

    def node_box(self, c):
        self.log(57, self.node_index(c.ecx))
        if not self.env[TBOX] & 2:
            return 1
        self.cpu.write(self.tbox, self.env_bytes(TBOX + 1, 6))
        self.cpu.u32(c.edx, self.tbox)
        return 0

    def world_point(self, c):
        i = self.node_index(c.ecx)
        p = struct.unpack('<3f', c.read(c.edx, 12))
        self.log(54, i, *struct.unpack('<3I', c.read(c.edx, 12)))
        w = struct.unpack('<3f', self.env_bytes(WORLD + 3 * max(i, 0), 3))
        c.write(c.edx, struct.pack('<3f', *(p[k] + w[k] for k in range(3))))
        return 0

    def world_position(self, c):
        i = self.node_index(c.ecx)
        self.log(20, i)
        if i < 0:  # no node: the exe reads what was on its stack, the port zeros
            c.write(c.edx, bytes(12))
            return 1
        c.write(c.edx, self.env_bytes(WORLD + 3 * i, 3))
        return 0

    def node_to_world(self, c):
        self.log(21, self.node_index(c.ecx), c.edx)
        top = struct.unpack('<I', c.read(struct.unpack('<I', c.read(0x5eda90, 4))[0], 4))[0]
        c.write(top, self.env_bytes(MECH_MATRIX, 12))
        c.u32(struct.unpack('<I', c.read(0x5eda8c, 4))[0], 0)
        return 0

    def node_euler(self, c):
        self.log(22, self.node_index(c.ecx))
        c.write(c.edx, self.env_bytes(EULER, 3))
        return 0

    def node_set(self, c):
        m = struct.unpack('<12I', c.read(c.edx, 48))
        self.log(14, self.node_index(c.ecx), *m)
        return 0

    def servo(self, c):
        args = [c.arg(k) for k in range(5)]
        if args[3:] != [0, u(-1.0)]:
            raise SystemExit(f'sound args {args}')
        self.log(33, args[0], args[1], 1 if args[2] else 0)
        return 9

    def env_bytes(self, i, n):
        return struct.pack(f'<{n}I', *self.env[i:i + n])

    def load(self, s):
        cpu = self.cpu
        self.env, self.flags = s['env'], s['env'][FLAGS]
        self.playing = s['env'][PLAYING:PLAYING + 3]
        cpu.write(self.mech, s['mech'])
        cpu.u32(self.mech, self.mvt)
        cpu.u32(self.mech + 4, self.nodes[MECH_NODE])
        cpu.write(self.transform, self.env_bytes(MECH_MATRIX, 12))
        cpu.u32(self.mech + 0xc, self.transform)
        cpu.u32(self.mech + 0xd4, self.weapons)
        cpu.u32(self.mech + 0x37c, self.ctl)
        cpu.u32(self.mech + 0x41c, self.skis)
        cpu.u32(self.mech + 0x684, self.pilot)
        for k, off in MECH_SLOTS.items():
            cpu.u32(self.mech + off, self.parts[k])
        cpu.write(self.pos, self.env_bytes(POSITION, 3))
        for k in range(NNODES):
            node, data = self.nodes[k], self.data[k]
            cpu.write(node, bytes(0x100))
            cpu.u32(node + 0x34, 5)
            cpu.u32(node + 0x38, data)
            if k < NPARTS:
                cpu.u32(node + 0x28, IDS[k])
            cpu.write(data, bytes(0x30) + s['nodes'][48 * k:48 * (k + 1)])
        for k in range(NPARTS):
            p = self.parts[k]
            img = bytearray(s['parts'][k * IMAGE:(k + 1) * IMAGE])
            if k in (FOOT0, FOOT1):
                put(img, 0x78, 'i', self.nodes[SHADOW0 + k - FOOT0] if get(img, 0x78) else 0)
                e = get(img, 0xe4)
                put(img, 0xe4, 'i', self.fx[e] if e in (1, 2) else e)
            if k in (ARM0, ARM1):
                put(img, 0x74, 'i', self.parts[HAND0 + k - ARM0] if get(img, 0x74) else 0)
            cpu.write(p, img)
            cpu.u32(p, VTABLES[k])
            cpu.u32(p + 4, self.mech)
            cpu.u32(p + 8, self.nodes[k] if s['present'] >> k & 1 else 0)
            cpu.u32(p + 0x14, self.data[k] + 0x30)
            cpu.u32(p + 0x18, self.data[k] + 0x54)
        for i in range(2):
            cpu.u32(self.parts[LEG0 + i] + 0x74, self.parts[CALF0 + i])
            cpu.u32(self.parts[LEG0 + i] + 0x78, self.parts[FOOT0 + i])
        skis = bytearray(s['skis'])
        for j in range(7):
            o = 0x194 + 0x28 * j
            m, n = get(skis, o + 0x20), get(skis, o + 0x24)
            put(skis, o + 0x20, 'i', self.mats[m] if m else 0)
            put(skis, o + 0x24, 'i', self.steps[n - 1] if n else 0)
        cpu.write(self.skis, skis)
        for k in range(12):
            cpu.u32(self.mats[k] + 0x20, k)
        cpu.write(SOIL_TABLE, self.env_bytes(SOIL, 12))
        for n in self.steps:
            cpu.u32(n + 0xbc, self.record)
        cpu.u32(self.record + 0xc, 0x1234)
        cpu.write(self.ctl, s['ctl'])
        cpu.u32(self.ctl + 4, self.vbt)
        cpu.write(self.vbt, struct.pack('<II', 0, 0x5c))
        cpu.u32(self.ctl + 0x60, CTL_VT)
        cpu.u32(self.pilot + 0xc, s['env'][DUST])
        cpu.u32(self.pilot + 0x204, s['env'][DUST + 1])
        cpu.u32(self.pilot + 0x208, s['env'][DUST + 2])
        cpu.u32(WORLD_OBJ, self.world_obj)
        cpu.u32(self.world_obj + 4, self.nodes[FX_NODE0])
        cpu.u32(self.pilot + 0x230, self.soils)
        cpu.write(self.soils, self.env_bytes(STEPS, 22))
        cpu.u32(self.screen + 0x1514, s['env'][SCREEN_FLAGS])
        cpu.write(self.weapons, bytes(0x20))
        cpu.write(self.weapons + 0xc, self.env_bytes(TARGET, 3))
        cpu.write(self.weapons + 0x18, bytes([self.flags >> 4 & 1]))
        for k, off in enumerate((0x68, 0x70, 0x74)):
            cpu.write(self.ldata + off, self.env_bytes(LOOK + k, 1))
        for i in range(2):
            cpu.u32(self.fx[1 + i] + 4, self.nodes[FX_NODE0 + i])
        cpu.write(self.dfn, s['def'])
        cpu.u32(CAMERA, self.camera)
        cpu.u32(self.camera + 0x64, self.mech if self.flags & 1 else 0)
        cpu.write(self.vmode, bytes(0x80))
        cpu.u32(self.vmode, self.vmvt)  # a mech camera (the base's null would crash 0x00426780)
        cpu.u32(self.vmode + 4, s['env'][VMODE] & 7)
        cpu.u32(self.camera + 0x58, self.vmode if s['env'][VMODE] & 0x10 else 0)
        cpu.write(self.look + 4, bytes(0x7c))
        cpu.u32(self.ctl + 0x70, self.mech)
        sel = s['env'][PARTSEL]
        cpu.u32(self.mech + 0x6c, self.contact if sel & 1 else 0)
        cpu.write(self.contact, bytes(0x30))
        cpu.u32(self.contact + 4, 0x7777 if sel & 2 else 0)
        cpu.u32(self.mech + 0x3e4, 1 if sel & 8 else 0)
        cpu.u32(MISSION, self.mission)
        cpu.u32(self.mission + 0x14, self.mech if self.flags & 2 else 0)
        cpu.u32(STATS, self.stats if self.flags & 4 else 0)
        self.events = []

    def store(self):
        r = self.cpu.read
        parts = []
        for k in range(NPARTS):
            img = bytearray(r(self.parts[k], IMAGE))
            if k in (FOOT0, FOOT1):
                put(img, 0x78, 'i', 1 if get(img, 0x78) else 0)
                put(img, 0xe4, 'i', self.fx_id(get(img, 0xe4)))
            if k in (ARM0, ARM1):
                put(img, 0x74, 'i', 1 if get(img, 0x74) else 0)
            parts.append(pick(bytes(img), FIELDS[k]))
        return {'mech': pick(r(self.mech, MECH_SIZE), MECH_FIELDS), 'parts': b''.join(parts),
                'ctl': pick(r(self.ctl, 0x74), CTL_FIELDS),
                'nodes': b''.join(r(d + 0x30, 48) for d in self.data)}, self.events

    def run(self, op, a):
        cpu, p = self.cpu, self.parts
        i = int(a[1])
        dt = u(a[0])
        vt = lambda k, off: struct.unpack('<I', cpu.read(VTABLES[k] + off, 4))[0]  # noqa: E731
        if op == 0:
            cpu.call(0x4623a0, ecx=p[HIP], stack_args=(dt,))
        elif op == 1:
            cpu.call(0x463ee0, ecx=p[LEG0 + i], stack_args=(dt,))
        elif op == 2:
            cpu.call(self.ground, ecx=p[FOOT0 + i])
            return struct.unpack('<f', cpu.read(self.slot, 4))[0]
        elif op == 3:
            cpu.call(0x466510, ecx=p[ARM0 + i], stack_args=(dt,))
        elif op == 4:
            cpu.call(0x466610, ecx=p[ARM0 + i], stack_args=(u(a[2]),))
        elif op == 5:
            cpu.call(0x464e80, ecx=p[TORSO], stack_args=(dt,))
        elif op == 6:
            cpu.call(vt(HAND0, 0x10), ecx=p[HAND0 + i], stack_args=(dt,))
        elif op in (7, 8, 9, 23):
            k = {7: HIP, 8: LEG0 + i, 9: ARM0 + i, 23: TORSO}[op]
            cpu.call(vt(k, 4), ecx=p[k])
            return float(cpu.uc.reg_read(UC_X86_REG_EAX) & 0xff != 0)
        elif op == 10:
            cpu.call(0x41bda0, ecx=self.mech, stack_args=(i,))
        elif op == 11:
            cpu.call(0x4622a0, ecx=p[HIP], stack_args=(u(a[2]),))
        elif op == 12:
            cpu.write(self.slot, struct.pack('<3f', a[0], a[2], a[3]))
            cpu.call(0x4621f0, ecx=p[HIP], stack_args=(self.slot,))
        elif op == 13:
            esp = STACK_TOP - 0x3000
            self.run_at(0x4164f3, 0x41660b, {UC_X86_REG_ESP: esp, UC_X86_REG_ESI: self.mech})
        elif op == 15:
            cpu.call(0x464ac0, ecx=p[TORSO], stack_args=(i,))
        elif op == 16:
            cpu.call(0x464c90, ecx=p[TORSO], stack_args=(i,))
        elif op == 17:  # the aim: a direction (none when a3 < -1.5), its clamped one logged
            cpu.write(self.aim, struct.pack('<3f', a[2], a[3], a[0]) + bytes(12))
            cpu.call(0x465ea0, ecx=p[TORSO], stack_args=(0 if a[3] < -1.5 else self.aim, self.aim + 12, i))
            self.log(51, *struct.unpack('<3I', cpu.read(self.aim + 12, 12)))
            return float(cpu.uc.reg_read(UC_X86_REG_EAX) & 0xff != 0)
        elif op == 18:
            cpu.call(0x465a00, ecx=p[TORSO], stack_args=(i + 1,))
        elif op == 19:
            cpu.call(0x466190, ecx=p[TORSO], stack_args=(int(a[2] > 0) + i,))
        elif op == 20:  # the controls' view state, then the view, the cockpit's look rate, the mode's torso look
            cpu.call(0x426780, ecx=self.ctl, stack_args=(int(a[1]) + 2 * int(a[2] > 0),))
            self.log(52, struct.unpack('<I', cpu.read(self.ctl + 0xc, 4))[0],
                     struct.unpack('<I', cpu.read(self.look + 0x3c, 4))[0], cpu.read(self.vmode + 0x54, 1)[0])
        elif op == 21:  # a part chosen by the keypad's id
            cpu.call(0x41d810, ecx=self.mech, stack_args=(int(a[3] * 4 + 8) % 16,))
        elif op == 22:
            cpu.call(0x41d6f0, ecx=self.mech)
            return float(cpu.uc.reg_read(UC_X86_REG_EAX) & 0xff != 0)
        elif op == 24:  # the mech's vt+0x8c: may the torso face it
            cpu.write(self.aim, struct.pack('<3f', a[2], a[3], a[0]))
            cpu.call(0x41dc70, ecx=self.mech, stack_args=(self.aim,))
            return float(cpu.uc.reg_read(UC_X86_REG_EAX) & 0xff != 0)
        elif op == 25:  # vt+0x90: the direction clamped to the torso's limits, logged
            cpu.write(self.aim, struct.pack('<3f', a[2], a[3], a[0]) + bytes(12))
            cpu.call(0x41dca0, ecx=self.mech, stack_args=(self.aim, self.aim + 12))
            self.log(53, *struct.unpack('<3I', cpu.read(self.aim + 12, 12)))
            return float(cpu.uc.reg_read(UC_X86_REG_EAX) & 0xff != 0)
        elif op == 26:  # vt+0x150: the torso's aim cosine above it (i: asked at its facing, +0x98)
            c = struct.unpack('<I', cpu.read(p[TORSO] + 0x98, 4))[0] if i else u(a[2])
            cpu.call(0x41dba0, ecx=self.mech, stack_args=(c,))
            return float(cpu.uc.reg_read(UC_X86_REG_EAX) & 0xff != 0)
        elif op == 27:  # vt+0x154: the AI's aim (none when a3 < -1.5)
            cpu.write(self.aim, struct.pack('<3f', a[2], a[3], a[0]))
            cpu.call(0x41dbd0, ecx=self.mech, stack_args=(0 if a[3] < -1.5 else self.aim, 0))
        elif op == 28:  # vt+0x08: the velocity others lead it by, logged
            cpu.call(0x41d400, ecx=self.mech, stack_args=(self.aim,))
            self.log(54, *struct.unpack('<3I', cpu.read(self.aim, 12)))
        elif op == 29:  # vt+0x12c: the throttle share that stops within a distance
            cpu.write(self.share_d, struct.pack('<f', f32(a[0] * 100.0)))
            cpu.call(self.share, ecx=self.mech)
            return struct.unpack('<f', cpu.read(self.slot, 4))[0]
        elif op == 14:
            ebp = STACK_TOP - 0x2000
            self.run_at(0x416d3a, 0x416e91, {UC_X86_REG_EBP: ebp, UC_X86_REG_ESP: ebp - 0x640,
                                             UC_X86_REG_ESI: self.mech})
        return 0.0


def unit(rng, n=3):
    v = [rng.uniform(-1, 1) for _ in range(n)]
    s = math.sqrt(sum(x * x for x in v)) or 1.0
    return [f32(x / s) for x in v]


def rnd(rng, lo, hi, zero=0.0):
    return 0.0 if rng.random() < zero else f32(rng.uniform(lo, hi))


def scenario(rng, op):
    parts = bytearray(IMAGE * NPARTS)
    nodes = bytearray(48 * NNODES)
    for k in range(NNODES):
        m = [rng.uniform(-1, 1) for _ in range(9)] + [rng.uniform(-4, 4) for _ in range(3)]
        if k == HIP or (k in (LEG0, LEG1, CALF0, CALF1, FOOT0, FOOT1) and rng.random() < 0.5):
            m[:9] = [1, 0, 0, 0, 1, 0, 0, 0, 1]
        struct.pack_into('<12f', nodes, 48 * k, *[f32(v) for v in m])
    for k in range(NPARTS):
        b = IMAGE * k
        put(parts, b + 0x48, 'b', rng.random() < 0.1)
        put(parts, b + 0x49, 'b', rng.random() < 0.5)
        put(parts, b + 0x4a, 'b', rng.random() < 0.15)
        for j in range(3):
            put(parts, b + 0x4c + 4 * j, 'f', rnd(rng, -3, 3))
        for j, v in enumerate(unit(rng, 4)):
            put(parts, b + 0x58 + 4 * j, 'f', v)
    for i in range(2):  # legs
        b = IMAGE * (LEG0 + i)
        put(parts, b + 0x7c, 'b', rng.random() < 0.3)
        for o in range(0x80, 0xd4, 4):
            put(parts, b + o, 'f', rnd(rng, -3, 3))
    for i in range(2):  # feet
        b = IMAGE * (FOOT0 + i)
        put(parts, b + 0x74, 'f', rnd(rng, 0, 0.5, 0.3))
        n = unit(rng)
        n[1] = abs(n[1])
        for j, v in enumerate(n):
            put(parts, b + 0x7c + 4 * j, 'f', v)
        put(parts, b + 0x78, 'i', rng.random() < 0.2)
        put(parts, b + 0x88, 'b', rng.random() < 0.5)
        put(parts, b + 0x8c, 'f', rnd(rng, -1, 2))
        put(parts, b + 0x90, 'f', rnd(rng, -1, 2))
        put(parts, b + 0x94, 'i', rng.choice([0, 0, 1, 1, 2, 2, 3, 3]))
        put(parts, b + 0xac, 'b', rng.random() < 0.4)
        put(parts, b + 0xc0, 'b', rng.random() < 0.3)
        for j, v in enumerate(unit(rng, 4)):
            put(parts, b + 0xc4 + 4 * j, 'f', v)
        put(parts, b + 0xd4, 'f', rnd(rng, -2, 2))
        put(parts, b + 0xd8, 'f', rng.choice([0.0, f32(0.005), f32(-0.01), f32(0.01), rnd(rng, -1, 1)]))
        put(parts, b + 0xdc, 'f', rnd(rng, 0, 3))
        put(parts, b + 0xe0, 'f', rnd(rng, -2, 2))
        put(parts, b + 0xe4, 'i', rng.choice([0, 1 + i, 1 + i]))
        put(parts, b + 0xe8, 'i', rng.randrange(11))
    b = IMAGE * HIP
    for j, v in enumerate(unit(rng, 4)):
        put(parts, b + 0x74 + 4 * j, 'f', v)
    for j, v in enumerate(unit(rng, 4)):
        put(parts, b + 0x84 + 4 * j, 'f', v)
    put(parts, b + 0x9c, 'f', rnd(rng, -3, 0.3, 0.2))
    put(parts, b + 0xa8, 'f', rng.choice([0.0, rnd(rng, -3, 0.5)]))
    b = IMAGE * TORSO
    put(parts, b + 0x84, 'i', rng.choice([0, 0, 1, 1, 1, 2]))
    put(parts, b + 0x88, 'i', rng.random() < 0.5)
    for j, v in enumerate(unit(rng)):
        put(parts, b + 0x8c + 4 * j, 'f', v)
    put(parts, b + 0x9c, 'i', rng.random() < 0.7)
    for o in (0xa0, 0xa1, 0xa2):
        put(parts, b + o, 'b', rng.random() < 0.3)
    for j, v in enumerate(unit(rng, 4)):
        put(parts, b + 0xa4 + 4 * j, 'f', v)
    for o in range(0xb4, 0xcc, 4):
        put(parts, b + o, 'f', rnd(rng, -1.2, 1.2, 0.4))
    for o in list(range(0xd0, 0xf0, 4)) + list(range(0x100, 0x118, 4)):
        put(parts, b + o, 'f', rnd(rng, -0.5, 0.5, 0.2))
    for o in (0xf0, 0x118, 0x128):
        for j, v in enumerate(unit(rng, 4)):
            put(parts, b + o + 4 * j, 'f', v)
    put(parts, b + 0x138, 'f', rnd(rng, -0.8, 0.8, 0.2))
    put(parts, b + 0x13c, 'f', rnd(rng, -2, 2, 0.2))
    put(parts, b + 0x140, 'f', rnd(rng, -1, 0))
    put(parts, b + 0x144, 'f', rnd(rng, 0, 1))
    put(parts, b + 0x148, 'f', rnd(rng, 0, 2))
    put(parts, b + 0x158, 'f', rnd(rng, 0, 20))
    put(parts, b + 0x15c, 'f', rnd(rng, 0, 10))
    for o in (0x160, 0x16c):
        for j, v in enumerate(unit(rng)):
            put(parts, b + o + 4 * j, 'f', v)
    put(parts, b + 0x1b0, 'i', rng.choice([0, 1, 2, 2]))
    for i in range(2):  # arms
        b = IMAGE * (ARM0 + i)
        put(parts, b + 0x74, 'i', rng.random() < 0.8)
        put(parts, b + 0x34, 'f', rng.choice([0.0, rnd(rng, 0, 1), f32(0.0011)]))
        for o in range(0x78, 0x98, 4):
            put(parts, b + o, 'f', rnd(rng, -1, 1))
        put(parts, b + 0x98, 'f', rnd(rng, -1, 1))
        put(parts, b + 0x9c, 'f', rnd(rng, -1.5, 1.5))
        for j, v in enumerate(unit(rng, 4)):
            put(parts, b + 0xa4 + 4 * j, 'f', v)
        put(parts, b + 0xb4, 'f', rnd(rng, 0, 10))
        put(parts, b + 0xb8, 'f', rnd(rng, -1, 0))
        put(parts, b + 0xbc, 'f', rnd(rng, 0, 1))
        put(parts, b + 0xc0, 'f', rnd(rng, -1.5, 0))
        put(parts, b + 0xc4, 'f', rnd(rng, 0, 1.5))
        put(parts, b + 0xc8, 'f', rnd(rng, 0, 1))
        put(parts, b + 0xcc, 'b', rng.random() < 0.3)
    for i in range(2):  # hands
        b = IMAGE * (HAND0 + i)
        for j, v in enumerate(unit(rng, 4)):
            put(parts, b + 0x74 + 4 * j, 'f', v)
        for j in range(3):
            put(parts, b + 0x84 + 4 * j, 'f', rnd(rng, -2, 2))
    mech = bytearray(MECH_SIZE)
    for j, v in enumerate(unit(rng)):
        put(mech, 0x24 + 4 * j, 'f', v)
    for j in range(3):
        put(mech, 0x3c + 4 * j, 'f', rnd(rng, -20, 20))
    put(mech, 0x508, 'f', rnd(rng, 3, 10))
    put(mech, 0x50c, 'f', rnd(rng, 0.5, 4))
    put(mech, 0x518, 'f', rnd(rng, 10, 40))
    put(mech, 0x520, 'f', rnd(rng, 0.5, 2))
    put(mech, 0x534, 'f', rng.choice([rnd(rng, 1, 30), 0.0, -5.0, rnd(rng, 0.1, 2)]))
    put(mech, 0x584, 'b', rng.random() < 0.2)
    put(mech, 0x585, 'b', rng.random() < 0.3)
    put(mech, 0x5c8, 'i', rng.choice([0, 0, 5]))
    put(mech, 0xdc, 'b', rng.random() < 0.1)  # powering down / down (vt+0xa0, 0x0041cc10)
    put(mech, 0xde, 'b', rng.random() < 0.1)
    put(mech, 0x688, 'i', rng.choice([0xffffffff, 0xffffffff, 0x1d, 0x1c, 0x10, 0x14, 0x18, 0x1b, 0x13, 0]))
    skis = bytearray(0x300)
    put(skis, 5, 'b', rng.random() < 0.3)
    for o in range(0x54, 0x6c, 4):
        put(skis, o, 'f', rnd(rng, -0.1, 0.1) if o < 0x5c else rnd(rng, -1, 1) if o < 0x64 else rnd(rng, -3, 3))
    for j in range(7):
        for k in range(3):
            put(skis, 0xd0 + 0xc * j + 4 * k, 'f', rnd(rng, -6, 6))
        put(skis, 0x15c + 4 * j, 'f', rng.choice([f32(-300.0), rnd(rng, -3, 3)]))
        put(skis, 0x178 + 4 * j, 'i', rng.choice([0, 1, 1, 2]))
        o = 0x194 + 0x28 * j
        n = unit(rng)
        n[1] = rng.choice([1.0, 0.0, abs(n[1]), abs(n[1]), -abs(n[1])])
        for k in range(3):
            put(skis, o + 4 * k, 'f', n[k])
        for k in range(3):
            put(skis, o + 0xc + 4 * k, 'f', rnd(rng, -10, 10))
        put(skis, o + 0x20, 'i', rng.choice([0, rng.randrange(1, 12)]))
        put(skis, o + 0x24, 'i', rng.choice([0, j + 1]))
    put(skis, 0x2f0, 'f', rnd(rng, 2, 6))
    put(skis, 0x2f4, 'f', rng.choice([0.0, rnd(rng, 0, 8)]))
    ctl = bytearray(0x74)
    put(ctl, 0x10, 'f', rnd(rng, -1, 1, 0.2))
    put(ctl, 0x14, 'f', rnd(rng, -1, 1, 0.5))
    put(ctl, 0x08, 'i', rng.choice([0, 1, 4, 5]))
    put(ctl, 0x0c, 'i', rng.randrange(4))
    env = [0] * ENV
    env[FLAGS] = rng.getrandbits(5)
    env[SCREEN_FLAGS] = rng.choice([0, 0x10, 0xffffffff, 0xffffffef])
    env[VMODE] = rng.choice([0x10, 0x11, 0x11, 0x12, 0x13, 0x15])  # a view on a mech always has a mode
    env[PARTSEL] = rng.choice([0xf, 0xf, 0xf, 0xd, 0xb, 0x7, 0xe, 0x3, 0])
    env[TBOX] = rng.choice([3, 3, 2, 1, 0])
    lo = [rnd(rng, -6, 0) for _ in range(3)]
    for k in range(3):
        env[TBOX + 1 + k] = u(lo[k])
        env[TBOX + 4 + k] = u(lo[k] + rnd(rng, 0, 8))
    put(mech, 0xf8, 'i', rng.choice([-1, -1, 1, 2, 6, 7, 8, 11, 13, 15, 3]))
    for k in range(3):
        put(mech, 0xec + 4 * k, 'f', rnd(rng, -9, 9))
    for k in range(3):
        env[TARGET + k] = u(rnd(rng, -50, 50))
        env[POSITION + k] = u(rnd(rng, -50, 50))
        env[EULER + k] = u(rnd(rng, -3, 3))
    m = unit(rng) + unit(rng) + unit(rng) + [rng.uniform(-5, 5) for _ in range(3)]
    if rng.random() < 0.4:
        a = rng.uniform(-3, 3)
        m[:9] = [math.cos(a), 0, -math.sin(a), 0, 1, 0, math.sin(a), 0, math.cos(a)]
    env[MECH_MATRIX:MECH_MATRIX + 12] = [u(f32(v)) for v in m]
    env[LOOK] = u(rnd(rng, 0, 2))
    env[LOOK + 1] = u(rnd(rng, -1, 0))
    env[LOOK + 2] = u(rnd(rng, 0, 1))
    for k in range(12):
        env[SOIL + k] = rng.randrange(11)
    for k in range(22):
        env[STEPS + k] = rng.choice([0, 0x40 + k])
    env[PLAYING + 1] = rng.random() < 0.5
    env[PLAYING + 2] = rng.random() < 0.5
    for k in range(NNODES):
        for j in range(3):
            env[WORLD + 3 * k + j] = u(rnd(rng, -5, 5))
    for i in range(2):  # most feet near the ground: their ski's parabola at the foot's z, give or take
        if rng.random() < 0.7:
            z = struct.unpack_from('<f', nodes, 48 * (FOOT0 + i) + 44)[0]
            a_, b_, c_ = (struct.unpack_from('<f', skis, o + 4 * i)[0] for o in (0x54, 0x5c, 0x64))
            env[WORLD + 3 * (FOOT0 + i) + 1] = u(f32(z * z * a_ - b_ * z + c_ + rng.uniform(-0.3, 0.8)))
    env[DUST] = rng.choice([3, 3, 7])
    env[DUST + 1] = rng.choice([0, 0x61])
    env[DUST + 2] = rng.choice([0, 0x62])
    env[DUST + 3] = rng.choice([0, 1])
    env[DUST + 4] = rng.choice([0, 1, 2, 3, 8])
    ty = u2f(env[WORLD + 3 * TORSO + 1])
    for k in range(8):
        env[DUST + 5 + 2 * k] = u(f32(ty - 50.0) if rng.random() < 0.15 else f32(ty + rng.uniform(-70, 70)))
        env[DUST + 6 + 2 * k] = rng.choice([0, 1, 1, rng.randrange(2, 12)])
    for j in range(3):
        put(mech, 0x30 + 4 * j, 'f', rnd(rng, -20, 20))
    put(mech, 0x4bc, 'f', rnd(rng, 0, 0.3))
    if rng.random() < 0.2:  # the throttle right at the step's top speed
        put(ctl, 0x10, 'f', struct.unpack_from('<f', mech, 0x4bc)[0] * rng.choice([1, -1]))
    put(mech, 0x586, 'b', rng.random() < 0.5)
    put(mech, 0x5e4, 'i', rng.choice([0, 3]))
    put(mech, 0x5e8, 'i', rng.choice([0, 4]))
    dfn = bytearray(DEF_SIZE)
    put(dfn, 0x148, 'f', rng.choice([0.0, rnd(rng, 0.5, 3), rnd(rng, -2, -0.1)]))
    for o in range(0x160, 0x174, 4):  # the shake's scales, frequency and damping (the torso's setup)
        put(dfn, o, 'f', rnd(rng, 0, 4))
    for o in range(0x174, 0x1a4, 4):
        put(dfn, o, 'f', rnd(rng, -1.5, 1.5) if o != 0x18c else rnd(rng, 0, 10))
    put(dfn, 0x1a4, 'b', rng.random() < 0.3)
    present = (1 << NPARTS) - 1
    if op < 21 and rng.random() < (0.4 if op >= 7 else 0.15):  # 21, 22: the part aim's parts have nodes
        present &= ~(1 << rng.choice([HIP, LEG0, LEG1, CALF0, FOOT1, ARM0, HAND1] if op >= 7 else range(NPARTS)))
    return {'mech': bytes(mech), 'parts': bytes(parts), 'nodes': bytes(nodes), 'present': present,
            'skis': bytes(skis), 'ctl': bytes(ctl), 'env': env, 'def': bytes(dfn)}


def port_run(s, op, args):
    env = (ctypes.c_uint32 * ENV)(*s['env'])
    probe.probe_parts_load(s['mech'], s['parts'], ctypes.create_string_buffer(s['nodes'], len(s['nodes'])),
                           s['present'], s['skis'], s['ctl'], env, s['def'])
    ret = probe.probe_parts_op(op, (ctypes.c_float * 4)(*args))
    mech = ctypes.create_string_buffer(MECH_SIZE)
    parts = ctypes.create_string_buffer(IMAGE * NPARTS)
    nodes = ctypes.create_string_buffer(48 * NNODES)
    ctl = ctypes.create_string_buffer(0x74)
    out = (ctypes.c_int32 * 16384)()
    n = probe.probe_parts_store(mech, parts, nodes, ctl, out, 16384)
    got = {'mech': pick(mech.raw, MECH_FIELDS),
           'parts': b''.join(pick(parts.raw[k * IMAGE:(k + 1) * IMAGE], FIELDS[k]) for k in range(NPARTS)),
           'ctl': pick(ctl.raw, CTL_FIELDS), 'nodes': nodes.raw}
    return ret, got, [v & 0xffffffff for v in out[:n]]


def compare(what, want, wlog, got, glog):
    for key in want:
        if want[key] != got[key]:
            w, g = want[key], got[key]
            diff = [o for o in range(0, len(w), 4) if w[o:o + 4] != g[o:o + 4]]
            where = [(o // IMAGE, hex(o % IMAGE)) for o in diff] if key == 'parts' else \
                [(o // 48, o % 48 // 4) for o in diff] if key == 'nodes' else [hex(o) for o in diff]
            vals = [(struct.unpack('<f', w[o:o + 4])[0], struct.unpack('<f', g[o:o + 4])[0]) for o in diff[:4]]
            raise SystemExit(f'{what}: {key} differs at {where[:8]} {vals}')
    # A footprint's first angle comes from the CRT asin (0x00523bc0 through
    # _CIasin); the port's matrix_to_euler takes the C library's: within 2e-6.
    wlog, glog = list(wlog), list(glog)
    i = 0
    while i < min(len(wlog), len(glog)) and wlog[i] == glog[i]:
        if wlog[i + 1] == 30:
            a, b = struct.unpack('<2f', struct.pack('<2I', wlog[i + 7], glog[i + 7]))
            if abs(a - b) <= 2e-6:
                glog[i + 7] = wlog[i + 7]
        i += wlog[i] + 1
    if wlog != glog:
        k = next(i for i in range(min(len(wlog), len(glog)) + 1) if i >= len(wlog) or i >= len(glog) or
                 wlog[i] != glog[i])
        raise SystemExit(f'{what}: calls differ at {k}\n  original {wlog[max(0, k - 8):k + 24]}\n'
                         f'  port     {glog[max(0, k - 8):k + 24]}')


OPS = [0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 5, 5, 5, 5, 6, 7, 8, 8, 9, 9, 10, 10, 11, 12, 13, 13, 14, 14,
       15, 16, 16, 17, 17, 17, 18, 18, 18, 19, 20, 20, 20, 20, 21, 21, 21, 21, 22, 22, 23, 23, 23,
       24, 24, 24, 25, 25, 25, 26, 26, 27, 27, 27, 28, 28, 28, 29, 29, 29]
NAMES = ['hip', 'leg', 'ground', 'arm', 'kick', 'torso', 'hand', 'hip setup', 'leg setup', 'arm setup', 'crouch',
         'duck', 'crouch target', 'landing dust', 'action dust', 'pitch lock', 'hold view', 'aim', 'centre',
         'torso mode', 'view state', 'target part', 'part aim', 'torso setup', 'can face', 'clamp aim',
         'faces within', 'torso aim', 'lead velocity', 'stop share']


def main():
    world = World()
    rng = random.Random(1999)
    trials = int(sys.argv[1]) if len(sys.argv) > 1 else 20000
    counts = [0] * len(NAMES)
    kinds = set()
    for n in range(trials):
        op = rng.choice(OPS)
        s = scenario(rng, op)
        dt = rng.choice([0.0, f32(1 / 30), f32(1 / 60), rnd(rng, 0.001, 0.2), rnd(rng, 0.05, 0.4)])
        args = [dt, float(rng.randrange(2)), rng.choice([rnd(rng, -0.5, 1.5), rnd(rng, -5, 0)]), rnd(rng, -2, 2)]
        world.load(s)
        wret = world.run(op, args)
        want, wlog = world.store()
        gret, got, glog = port_run(s, op, args)
        what = f'trial {n} {NAMES[op]} {args[:3]}'
        if struct.pack('<f', wret) != struct.pack('<f', gret):
            raise SystemExit(f'{what}: returns {wret} {gret}')
        compare(what, want, wlog, got, glog)
        counts[op] += 1
        i = 0
        while i < len(wlog):
            kinds.add(wlog[i + 1])
            i += wlog[i] + 1
    print(f'ok: {trials} part setups and updates identical bit for bit '
          f'({", ".join(f"{NAMES[k]} {counts[k]}" for k in range(len(NAMES)))}); {len(kinds)} kinds of call out')


if __name__ == '__main__':
    main()
