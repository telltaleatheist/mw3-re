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
from unicorn.x86_const import UC_X86_REG_EDX, UC_X86_REG_EIP, UC_X86_REG_ESP

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
probe.probe_pose_load.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_void_p, ctypes.c_uint32]
probe.probe_pose_op.argtypes = [ctypes.c_int, ctypes.c_void_p]
probe.probe_pose_store.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int]

MECH_VT, MECH_SIZE, IMAGE = 0x5bcef8, 0x6e0, 0x1b4
LEG0, LEG1, CALF0, CALF1, FOOT0, FOOT1, HIP, TORSO, ARM0, ARM1, HAND0, HAND1 = range(12)
NPARTS = 12
VTABLES = [0x5c2978, 0x5c2978, 0x5c2b18, 0x5c2b18, 0x5c2910, 0x5c2910, 0x5c28a8, 0x5c29e0, 0x5c2a48, 0x5c2a48,
           0x5c2ab0, 0x5c2ab0]
MECH_SLOTS = {LEG0: 0x38c, LEG1: 0x390, ARM0: 0x394, ARM1: 0x398, TORSO: 0x39c, HIP: 0x3a0}
NODE_SET, FOOT_GROUND = 0x4fc300, 0x462a10

BASE = [(0x48, 'b'), (0x4a, 'b')] + [(0x4c + 4 * k, 'f') for k in range(7)]
LEG = BASE + [(0x7c, 'b')] + [(0x80 + 4 * k, 'f') for k in range(2 + 8 + 11)]
FOOT = BASE + [(0x8c, 'f'), (0x94, 'i')] + [(0xc4 + 4 * k, 'f') for k in range(4)]
HIP_F = BASE + [(0x74 + 4 * k, 'f') for k in range(8)] + [(0x98 + 4 * k, 'f') for k in range(3)] + [(0xa8, 'f')]
FIELDS = [LEG, LEG, BASE, BASE, FOOT, FOOT, HIP_F, BASE, BASE, BASE, BASE, BASE]
MECH_FIELDS = [(0x688, 'i')]


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
        self.nodes = [a(0x100) for _ in range(NPARTS)]
        self.data = [a(0x60) for _ in range(NPARTS)]
        self.events = []
        cpu.stub_function(NODE_SET, self.node_set)
        self.fldz_ret4 = cpu.code(b'\xd9\xee\xc2\x04\x00')
        cpu.uc.hook_add(UC_HOOK_CODE, self.foot_ground, begin=FOOT_GROUND, end=FOOT_GROUND)

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
        for i in range(2):
            cpu.u32(self.parts[LEG0 + i] + 0x74, self.parts[CALF0 + i])
            cpu.u32(self.parts[LEG0 + i] + 0x78, self.parts[FOOT0 + i])
            cpu.u32(self.parts[ARM0 + i] + 0x74, self.parts[HAND0 + i])
        self.events = []

    def store(self):
        r = self.cpu.read
        return {'mech': pick(r(self.mech, MECH_SIZE), MECH_FIELDS),
                'parts': b''.join(pick(r(self.parts[k], IMAGE), FIELDS[k]) for k in range(NPARTS)),
                'nodes': b''.join(r(d + 0x30, 48) for d in self.data)}, self.events


def port_load(s):
    probe.probe_pose_load(s['mech'], s['parts'], ctypes.create_string_buffer(s['nodes'], len(s['nodes'])),
                          s['present'])


def port_store():
    mech = ctypes.create_string_buffer(MECH_SIZE)
    parts = ctypes.create_string_buffer(IMAGE * NPARTS)
    nodes = ctypes.create_string_buffer(48 * NPARTS)
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
            raise SystemExit(f'{what}: {key} differs at {where[:8]}')
    if wlog != glog:
        raise SystemExit(f'{what}: calls differ\n  original {wlog[:60]}\n  port     {glog[:60]}')
