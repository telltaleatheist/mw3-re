"""A mech's parts in emulator memory for the pose tests (test_pose.py and the
gait, jump and action pose tests), with the same layout tests/probe_pose.cpp
reads and writes: the mech, its twelve parts with their real vtables, and a
node per part (an object3d stand-in, class 5) whose 3x4 matrix the part's
+0x14 / +0x18 point into. Node sets (0x4fc300) and the feet's ground update
(0x462a10) are logged as (count, code, args...).
"""
import ctypes
import struct

from original import FTOL, IAT_FTOL, ROOT, Original
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EBP, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_EDX,
                               UC_X86_REG_EIP, UC_X86_REG_ESI, UC_X86_REG_ESP)
from original import RETURN, STACK_TOP

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
probe.probe_pose_load.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_void_p, ctypes.c_uint32,
                                  ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]
probe.probe_pose_op.argtypes = [ctypes.c_int, ctypes.c_void_p]
probe.probe_pose_store.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int]

MECH_VT, MECH_SIZE, IMAGE = 0x5bcef8, 0x6e0, 0x1b4
LEG0, LEG1, CALF0, CALF1, FOOT0, FOOT1, HIP, TORSO, ARM0, ARM1, HAND0, HAND1 = range(12)
NPARTS = 12
TOE0, SKI0, NNODES, TABLE = 12, 20, 22, 120
# The scripted animation's table: hip 0, feet 7 / 14, torso 21, torso rotation 28, arms 32 (ok, q),
# hands 42, toes 52 + 20 leg + 5 toe (ok, q), thighs 92 + 7 leg, calves 106 + 7 leg.
AVT_SIZE = 0x50
VTABLES = [0x5c2978, 0x5c2978, 0x5c2b18, 0x5c2b18, 0x5c2910, 0x5c2910, 0x5c28a8, 0x5c29e0, 0x5c2a48, 0x5c2a48,
           0x5c2ab0, 0x5c2ab0]
MECH_SLOTS = {LEG0: 0x38c, LEG1: 0x390, ARM0: 0x394, ARM1: 0x398, TORSO: 0x39c, HIP: 0x3a0}
NODE_SET, FOOT_GROUND = 0x4fc300, 0x462a10

BASE = [(0x48, 'b'), (0x4a, 'b')] + [(0x4c + 4 * k, 'f') for k in range(7)]
LEG = BASE + [(0x7c, 'b')] + [(0x80 + 4 * k, 'f') for k in range(2 + 8 + 11)]
FOOT = BASE + [(0x8c, 'f'), (0x94, 'i')] + [(0xc4 + 4 * k, 'f') for k in range(4)]
HIP_F = BASE + [(0x74 + 4 * k, 'f') for k in range(8)] + [(0x98 + 4 * k, 'f') for k in range(3)] + [(0xa8, 'f')]
TORSO_F = BASE + [(0x128 + 4 * k, 'f') for k in range(4)]
FIELDS = [LEG, LEG, BASE, BASE, FOOT, FOOT, HIP_F, TORSO_F, BASE, BASE, BASE, BASE]
MECH_FIELDS = [(0x3c, 'f'), (0x40, 'f'), (0x44, 'f'), (0x578, 'i'), (0x58c, 'i'), (0x688, 'i')]


def f2u(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


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


class World:
    """The original's side."""

    def __init__(self, cpu=None):
        cpu = self.cpu = cpu or Original()
        cpu.import_code(IAT_FTOL, FTOL)
        a = cpu.alloc
        self.mech = a(MECH_SIZE)
        self.parts = [a(IMAGE) for _ in range(NPARTS)]
        self.nodes = [a(0x100) for _ in range(NNODES)]
        self.data = [a(0x60) for _ in range(NNODES)]
        self.anim, self.avt, self.table = a(0xf0), a(AVT_SIZE * 4), a(TABLE * 4)
        self.skis, self.wraps = a(0x300), [a(8), a(8)]
        self.toe_nodes, self.toe_q = [a(16), a(16)], [a(64), a(64)]
        self.scratch = a(0x40)
        self.view, self.camera = a(0x10), a(0x80)
        vt = [0] * AVT_SIZE
        stubs = {0x1c: (self.a_ski, 8), 0x24: (lambda c: self.a_copy(c, 7 + 7 * c.arg(0), 1, 2, False), 12),
                 0x28: (lambda c: self.a_part(c, 32 + 5 * c.arg(0)), 12),
                 0x2c: (lambda c: self.a_part(c, 42 + 5 * c.arg(0)), 12),
                 0x30: (lambda c: self.a_copy(c, 0, 0, 1, False), 8), 0x34: (lambda c: self.a_copy(c, 21, 0, 1, False), 8),
                 0x38: (lambda c: self.a_rot(c, 28, 0), 4),
                 0x3c: (lambda c: self.a_toe(c), 12)}
        for off, (fn, pop) in stubs.items():
            addr = cpu.code(b'\xc3')
            vt[off // 4] = addr
            cpu.stub_function(addr, fn, pop)
        cpu.write(self.avt, struct.pack(f'<{AVT_SIZE}I', *vt))
        cpu.stub_function(0x4210d0, lambda c: self.a_copy(c, 92 + 7 * c.arg(0), 1, 2, False), 12)
        cpu.stub_function(0x421080, lambda c: self.a_copy(c, 106 + 7 * c.arg(0), 1, 2, False), 12)
        self.events = []
        cpu.stub_function(NODE_SET, self.node_set)
        self.fldz_ret4 = cpu.code(b'\xd9\xee\xc2\x04\x00')
        cpu.uc.hook_add(UC_HOOK_CODE, self.foot_ground, begin=FOOT_GROUND, end=FOOT_GROUND)

    def tab(self, i, n):
        return self.cpu.read(self.table + 4 * i, 4 * n)

    def a_copy(self, c, o, ip, iq, _):
        c.write(c.arg(ip), self.tab(o, 3))
        c.write(c.arg(iq), self.tab(o + 3, 4))
        return 0

    def a_rot(self, c, o, iq):
        c.write(c.arg(iq), self.tab(o, 4))
        return 0

    def a_part(self, c, o):
        if struct.unpack('<f', self.tab(o, 1))[0] == 0:
            return 0
        if c.arg(1):
            c.write(c.arg(1), bytes(12))
        if c.arg(2):
            c.write(c.arg(2), self.tab(o + 1, 4))
        return 1

    def a_toe(self, c):
        o = 52 + 20 * c.arg(0) + 5 * c.arg(1)
        if struct.unpack('<f', self.tab(o, 1))[0] == 0:
            return 0
        c.write(c.arg(2), self.tab(o + 1, 4))
        return 1

    def a_ski(self, c):
        m = c.arg(1)
        self.log(16, c.arg(0), 1 if m else 0, *(struct.unpack('<12I', c.read(m, 48)) if m else [0] * 12))
        return 0

    def log(self, *e):
        self.events.append(len(e))
        self.events.extend(v & 0xffffffff for v in e)

    def node_set(self, c):
        n = self.nodes.index(c.ecx) if c.ecx in self.nodes else -1
        m = struct.unpack('<12I', c.read(c.uc.reg_read(UC_X86_REG_EDX), 48))
        self.log(14, n, *m)
        return 0

    def foot_ground(self, uc, address, size, user):
        c = self.cpu
        i = self.parts.index(c.ecx) - FOOT0 if c.ecx in self.parts else -1
        self.log(15, i)
        uc.reg_write(UC_X86_REG_EIP, self.fldz_ret4)

    def load(self, s):
        cpu = self.cpu
        cpu.write(self.mech, s['mech'])
        cpu.u32(self.mech, MECH_VT)
        for k, off in MECH_SLOTS.items():
            cpu.u32(self.mech + off, self.parts[k])
        for k in range(NPARTS):
            p = self.parts[k]
            cpu.write(p, s['parts'][k * IMAGE:(k + 1) * IMAGE])
            cpu.u32(p, VTABLES[k])
            cpu.u32(p + 4, self.mech)
            node, data = self.nodes[k], self.data[k]
            cpu.write(node, bytes(0x100))
            cpu.u32(node + 0x34, 5)
            cpu.u32(node + 0x38, data)
            cpu.write(data, bytes(0x30) + s['nodes'][48 * k:48 * (k + 1)])
            cpu.u32(p + 8, node if s['present'] >> k & 1 else 0)
            cpu.u32(p + 0x14, data + 0x30)
            cpu.u32(p + 0x18, data + 0x54)
        for k in range(NPARTS, NNODES):
            node, data = self.nodes[k], self.data[k]
            cpu.write(node, bytes(0x100))
            cpu.u32(node + 0x34, 5)
            cpu.u32(node + 0x38, data)
            cpu.write(data, bytes(0x30) + s['nodes'][48 * k:48 * (k + 1)])
        cpu.write(self.table, s['table'])
        cpu.write(self.anim, bytes(0xf0))
        cpu.u32(self.anim, self.avt)
        for a in range(13):
            cpu.u32(self.mech + 0x424 + 4 * a, 0)
        cpu.u32(self.mech + 0x42c, self.anim)
        if s['present'] >> 31 & 1:
            cpu.u32(self.mech + 0x454, self.anim)
        cpu.u32(self.mech + 0x41c, self.skis)
        cpu.u32(0x5f4c8c, self.camera)
        cpu.u32(self.camera + 0x58, self.view)
        cpu.u32(self.view + 4, 1 if s['present'] >> 30 & 1 else 0)
        cpu.u32(self.camera + 0x64, self.mech if s['present'] >> 29 & 1 else 0)
        for i in range(2):
            cpu.u32(self.skis + 0x3c + 4 * i, self.wraps[i])
            cpu.u32(self.wraps[i] + 4, self.nodes[SKI0 + i])
            f = self.parts[FOOT0 + i]
            cpu.u32(f + 0xa0, s['toes'][i])
            cpu.u32(f + 0xa4, self.toe_nodes[i])
            cpu.u32(f + 0xa8, self.toe_q[i])
            for k in range(4):
                cpu.u32(self.toe_nodes[i] + 4 * k, self.nodes[TOE0 + 4 * i + k])
            cpu.write(self.toe_q[i], s['toe_q'][64 * i:64 * (i + 1)])
        for i in range(2):
            cpu.u32(self.parts[LEG0 + i] + 0x74, self.parts[CALF0 + i])
            cpu.u32(self.parts[LEG0 + i] + 0x78, self.parts[FOOT0 + i])
            cpu.u32(self.parts[ARM0 + i] + 0x74, self.parts[HAND0 + i])
        self.events = []

    def store(self):
        r = self.cpu.read
        return {'mech': pick(r(self.mech, MECH_SIZE), MECH_FIELDS),
                'parts': b''.join(pick(r(self.parts[k], IMAGE), FIELDS[k]) for k in range(NPARTS)),
                'nodes': b''.join(r(d + 0x30, 48) for d in self.data) + r(self.toe_q[0], 64) + r(self.toe_q[1], 64)}, \
            self.events

    def run_at(self, start, esp, regs):
        """Runs from `start` (inside a function) with these registers until it returns to RETURN."""
        uc = self.cpu.uc
        uc.reg_write(UC_X86_REG_ESP, esp)
        for r, v in regs.items():
            uc.reg_write(r, v & 0xffffffff)
        try:
            uc.emu_start(start, RETURN, count=5_000_000)
        except Exception as e:
            raise RuntimeError(f"{start:#x}: {e} at {uc.reg_read(UC_X86_REG_EIP):#x}") from None
        if uc.reg_read(UC_X86_REG_EIP) != RETURN:
            raise RuntimeError(f'{start:#x} did not return')

    def gait_pose(self, jets, saved):
        """Move 0x417a80 from its pose block 0x417bbc: EDI the mech, EBX the
        animation, ESI 0, [ebp+8] the jets, [ebp-0x40] the saved velocity."""
        cpu = self.cpu
        ebp = STACK_TOP - 0x2000
        cpu.u32(ebp + 4, RETURN)
        cpu.u32(ebp + 8, jets)
        cpu.u32(ebp + 0xc, self.anim)  # Move's animation argument, reloaded in the toe loop
        cpu.write(ebp - 0x40, struct.pack('<3f', *saved))
        self.run_at(0x417bbc, ebp - 0x98 - 12, {UC_X86_REG_EBP: ebp, UC_X86_REG_EDI: self.mech,
                                               UC_X86_REG_EBX: self.anim, UC_X86_REG_ESI: 0})

    def jump_pose(self):
        """JumpJets 0x4173b0 from 0x4177e7: ESI the mech, EBP firing; the four
        saved registers under its 0x80 bytes of locals."""
        esp = STACK_TOP - 0x2000
        self.cpu.u32(esp + 0x90, RETURN)
        self.run_at(0x4177e7, esp, {UC_X86_REG_ESI: self.mech, UC_X86_REG_EBP: 1})

    def action_pose(self, hip, q):
        """ActionUpdate 0x416860 from 0x416fb8: ESI the mech, EDI the action,
        the hip at [ebp-0x30] and its rotation at [ebp-0x90]; the left foot's
        sample call's three arguments already pushed."""
        cpu = self.cpu
        ebp = STACK_TOP - 0x2000
        cpu.u32(ebp + 4, RETURN)
        cpu.write(ebp - 0x30, struct.pack('<3f', *hip))
        cpu.write(ebp - 0x90, struct.pack('<4f', *q))
        cpu.u32(ebp - 0x14, self.anim)  # the toe loop reloads the action from here
        esp = ebp - 0x5c4 - 12 - 12
        cpu.write(esp, struct.pack('<III', 0, ebp - 0x74, ebp - 0x40))
        self.run_at(0x416fb8, esp, {UC_X86_REG_EBP: ebp, UC_X86_REG_ESI: self.mech, UC_X86_REG_EDI: self.anim,
                                    UC_X86_REG_ECX: self.anim})


def port_load(s):
    probe.probe_pose_load(s['mech'], s['parts'], ctypes.create_string_buffer(s['nodes'], len(s['nodes'])),
                          s['present'], ctypes.create_string_buffer(s['table'], len(s['table'])),
                          (ctypes.c_uint32 * 2)(*s['toes']), ctypes.create_string_buffer(s['toe_q'], 128))


def port_store():
    mech = ctypes.create_string_buffer(MECH_SIZE)
    parts = ctypes.create_string_buffer(IMAGE * NPARTS)
    nodes = ctypes.create_string_buffer(48 * NNODES + 128)
    out = (ctypes.c_int32 * 8192)()
    n = probe.probe_pose_store(mech, parts, nodes, out, 8192)
    log = [v & 0xffffffff for v in out[:n]]
    return {'mech': pick(mech.raw, MECH_FIELDS),
            'parts': b''.join(pick(parts.raw[k * IMAGE:(k + 1) * IMAGE], FIELDS[k]) for k in range(NPARTS)),
            'nodes': nodes.raw}, log


def port_op(op, args):
    return probe.probe_pose_op(op, (ctypes.c_float * 32)(*args))


def compare(what, want, wlog, got, glog):
    for key in want:
        if want[key] != got[key]:
            w, g = want[key], got[key]
            diff = [o for o in range(0, len(w), 4) if w[o:o + 4] != g[o:o + 4]]
            where = [(o // IMAGE, hex(o % IMAGE)) for o in diff] if key == 'parts' else \
                [(o // 48, o % 48 // 4) for o in diff] if key == 'nodes' else [hex(o) for o in diff]
            vals = [(struct.unpack('<f', w[o:o + 4])[0], struct.unpack('<f', g[o:o + 4])[0]) for o in diff[:4]]
            raise SystemExit(f'{what}: {key} differs at {where[:8]} {vals}')
    if wlog != glog:
        raise SystemExit(f'{what}: calls differ\n  original {wlog[:60]}\n  port     {glog[:60]}')
