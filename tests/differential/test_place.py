"""Putting a mech on the ground (docs/mission-start.md section 11) on the six
mission worlds, against the original x86:

- the point query 0x4edab0 (zclass\\cls_di.c) with its node walk (0x4eddb0,
  0x4ee140, 0x4ee220), box test (0x4f12e0 with 0x4ec430, 0x523040) and the
  model's polygon test (0x54be40, 0x54d640);
- the pick 0x4abd90 on crafted records (water, no material, heights around
  the step);
- the placement 0x4161c0 whole on a mech image: the query, the pick, the
  world's attach ([0x608c84] vtable +0), Mech::SetPlace 0x418f30 and
  0x418e90 with the controls' reset 0x426c80, the collider's points
  0x422500 and the skis' reset 0x475fb0 (0x4764a0, 0x475c80);
- the pilot's spawn 0x4782f0 on a mech image.

    cmake --build build && .venv/bin/python tests/differential/test_place.py [queries per world]

Each world is read by the original's reader into emulator memory and by the
port's loaders (test_scenewalk). Points are drawn over the world (on and off
the grid, with and without its wrapping, with zModel's zone mode 0 and 1, at
heights around the ground the original finds); half way through each world
a quarter of the polygons become strips and some nodes get INTERSECT_BBOX,
as in test_di. The records (all 32 slots, pointers as indices), the
results, and every few queries each model's cached polygon and each node's
flags must match bit for bit at 24-bit precision. A placement runs at each
point too: the mech's fields, its matrix, controls, collider, skis and ski
matrices must match bit for bit, and so must the calls out (the query with
the zone mode it saw, the attach, vtable +0x18, the zEntity update, the
ski matrices, the zone mode after).
"""
import ctypes
import math
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_di import REC, World, f32, port_state, probe  # noqa: E402
from test_gamez import WORLDS, ZBD  # noqa: E402
from test_scenewalk import f2u  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402

probe.pl_point.argtypes = [ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_int, ctypes.c_void_p]
probe.pl_below.argtypes = probe.pl_point.argtypes
probe.pl_pick.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_float, ctypes.c_float, ctypes.c_int,
                          ctypes.c_void_p]
probe.pl_place.argtypes = [ctypes.c_void_p, ctypes.c_float, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int,
                           ctypes.c_void_p]
probe.pl_add_world_children.argtypes = [ctypes.c_void_p, ctypes.c_int]
probe.pl_polygon.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_float, ctypes.c_float, ctypes.c_void_p,
                             ctypes.c_void_p]
probe.pl_spawn.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint8, ctypes.c_int, ctypes.c_uint32, ctypes.c_int,
                           ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p]

POINT, PICK, PLACE, SPAWN, POLYGON, BELOW = 0x4edab0, 0x4abd90, 0x4161c0, 0x4782f0, 0x54d640, 0x4eda10
# Branches the point query must reach (counted in the original as it runs).
# The worlds hold no sequence (7) or animated (8) node and no fading one:
# those branches are 0x4ee2f0's (test_di) for one point, and not reached.
COVER = {'camera': 0x4edfbb, 'lod': 0x4edf58, 'light': 0x4ee220,
         'box test': 0x4f12e0, 'box through a matrix': 0x4ec54b, 'wrapped cell': 0x4edb9c,
         'strip': 0x54c2b5, 'cached strip': 0x54c0ee}
COVERED = dict.fromkeys(COVER, 0)
ZONE_MODE, WORLD_OBJ, CHEAT = 0x5ea1d0, 0x608c84, 0x608e40
MECH_VT = 0x5bcef8
# The probe's image (probe_place.cpp): the mech's fields at the exe's
# offsets, then the blocks the mech points at.
T_OFF, C_OFF, S_OFF, M_OFF, IMAGE = 0x400, 0x440, 0x800, 0xb00, 0xb60
NODE_MARK, SKI_MARKS = 0x5150, (0x5160, 0x5170)
# The ranges compared: the mech's (velocity .. +0x64, the collider), the
# controls' floats, the skis' fields the placement touches.
MECH_RANGES = [(0x30, 0x64), (0x120, 0x340)]
CTL_RANGES = [(0x10, 0x34), (0x40, 0x50)]
SKI_RANGES = [(0x44, 0x6c), (0x78, 0x7c), (0xd0, 0x140), (0x2ec, 0x2f4), (0x2f8, 0x300)]


def rnd_floats(rng, n, lo=-50.0, hi=50.0):
    return struct.pack(f'<{n}f', *(f32(rng.uniform(lo, hi)) for _ in range(n)))


class Place(World):
    """test_di's world with a crafted mech and the placement's calls out
    stubbed and logged."""

    def __init__(self, name):
        super().__init__(name)
        cpu = self.cpu
        self.log = []
        self.mech = self.alloc(0x6d8)
        vt = self.alloc(0x200)
        cpu.write(vt, cpu.read(MECH_VT, 0x200))
        self.setpos, self.moved, self.attach, self.update = (cpu.code(b'\xc3') for _ in range(4))
        cpu.u32(vt + 0x18, self.setpos)
        cpu.u32(vt + 0x1c, self.moved)
        cpu.u32(vt + 0x34, self.update)
        cpu.u32(self.mech, vt)
        self.node = self.alloc(0xd4)
        cpu.write(self.node, bytes(0xd4))
        cpu.u32(self.node + 0x54, 1)  # a parent: 0x5a85c0 goes on to vtable +0x1c
        cpu.u32(self.mech + 4, self.node)
        self.transform, self.controls, self.skis = self.alloc(48), self.alloc(0x60), self.alloc(0x300)
        cpu.u32(self.mech + 0xc, self.transform)
        cpu.u32(self.mech + 0x37c, self.controls)
        cpu.u32(self.mech + 0x41c, self.skis)
        self.ski_nodes = [self.alloc(16), self.alloc(16)]
        self.ski_wraps = [self.alloc(16), self.alloc(16)]  # skis +0x3c, +0x40: wrappers whose +4 is the node
        for k in range(2):
            cpu.u32(self.ski_wraps[k] + 4, self.ski_nodes[k])
        self.ski_mats = [self.alloc(48), self.alloc(48)]
        obj, ovt = self.alloc(8), self.alloc(4)
        cpu.u32(ovt, self.attach)
        cpu.u32(obj, ovt)
        cpu.u32(obj + 4, self.world)
        cpu.u32(WORLD_OBJ, obj)
        cpu.stub_function(self.attach, lambda c: self.log.extend(
            [2, NODE_MARK if c.arg(0) == self.node else c.arg(0)]), 4)
        cpu.stub_function(self.setpos, lambda c: self.log.extend(
            [3] + list(struct.unpack('<3I', cpu.read(c.arg(0), 12)))), 4)
        cpu.stub_function(self.moved, lambda c: self.log.append(4))
        cpu.stub_function(self.update, lambda c: self.log.extend(
            [22, c.arg(0), self.u32(c.ecx + 0x688)]), 4)
        cpu.stub_function(0x4fc3f0, lambda c: self.ski_mats[self.ski_nodes.index(c.ecx)])
        cpu.stub_function(0x4fc300, lambda c: self.log.extend(
            [5, self.ski_nodes.index(c.ecx)] + list(struct.unpack('<12I', cpu.read(c.edx, 48)))))
        cpu.uc.hook_add(UC_HOOK_CODE, self.query_in, begin=POINT, end=POINT)
        for key, a in COVER.items():
            cpu.uc.hook_add(UC_HOOK_CODE, lambda uc, ad, n, k: COVERED.__setitem__(k, COVERED[k] + 1), user_data=key,
                            begin=a, end=a)
        self.spawn_mode = False
        self.spawn_option = 0
        self.weapons = self.alloc(0x30)
        self.pilot = self.alloc(0x100)

    def query_in(self, uc, address, size, user):
        if self.logging:
            c = self.cpu
            self.log.extend([1, self.u32(ZONE_MODE), c.arg(0), c.arg(1), c.arg(2)])

    def add_world_children(self, classes):
        """The first node of each class becomes a world child on both sides."""
        bs, picked = self.u32(0x5ea830), []
        for b, base in enumerate(self.blocks):
            for i in range(bs):
                a = base + 0xd4 * i
                if self.u32(a + 0x38) and self.u32(a + 0x34) in classes and self.u32(a + 0x54) == 0:
                    classes = [c for c in classes if c != self.u32(a + 0x34)]
                    picked.append((b * bs + i, a))
        had, old = self.u32(self.world + 0x5c), self.u32(self.world + 0x60)
        new = self.alloc(4 * (had + len(picked)))
        self.cpu.write(new, self.cpu.read(old, 4 * had) + b''.join(struct.pack('<I', a) for _, a in picked))
        self.cpu.u32(self.world + 0x60, new)
        self.cpu.u32(self.world + 0x5c, had + len(picked))
        idx = (ctypes.c_int32 * len(picked))(*(i for i, _ in picked))
        probe.pl_add_world_children(idx, len(picked))
        return len(picked)

    def polygon(self, verts, x, z, slot):
        cpu = self.cpu
        if not hasattr(self, 'poly_buf'):
            self.poly_buf, self.poly_slot = self.alloc(12 * 16), self.alloc(0x28)
        n = len(verts) // 3
        cpu.write(self.poly_buf, struct.pack(f'<{len(verts)}f', *verts))
        cpu.write(self.poly_slot, struct.pack('<5I', *slot) + bytes(0x14))
        cpu.call(POLYGON, ecx=self.poly_slot, edx=self.poly_buf, stack_args=(f2u(x), f2u(z), n))
        return [cpu.uc.reg_read(19)] + list(struct.unpack('<4I', cpu.read(self.poly_slot, 12) +
                                                          cpu.read(self.poly_slot + 0x10, 4)))

    def point(self, x, y, z, zone_mode):
        cpu = self.cpu
        cpu.u32(ZONE_MODE, zone_mode)
        self.reports = []
        self.logging = False
        cpu.call(POINT, ecx=self.world, edx=self.out, stack_args=(f2u(x), f2u(y), f2u(z)), count=500_000_000)
        cpu.u32(ZONE_MODE, 1)
        r = cpu.uc.reg_read(19)
        rec = cpu.read(self.out, REC)
        out = [rec[0]]
        for j in range(32):
            h = struct.unpack_from('<10I', rec, 4 + 0x28 * j)
            out.extend(h[:8])
            out.append(self.material_index(h[8]) & 0xffffffff)
            out.append(self.node_index(h[9]) & 0xffffffff if h[9] else 0xffffffff)
        return r, out, [rep[1] for rep in self.reports]

    def below(self, x, y, z, zone_mode):
        # 0x4eda10: ECX the world, EDX the point, the record on the stack.
        cpu = self.cpu
        if not hasattr(self, 'below_at'):
            self.below_at = self.alloc(12)
            # The scene walk's harness stubs 0x4eda10 (its zone probe): enter it
            # past the stub through its first two instructions (push ebx; push ebp).
            at = cpu.code_next
            self.below_entry = cpu.code(b'\x53\x55\xe9' + struct.pack('<i', BELOW + 2 - (at + 7)))
        cpu.write(self.below_at, struct.pack('<3f', x, y, z))
        cpu.u32(ZONE_MODE, zone_mode)
        self.reports = []
        self.logging = False
        cpu.call(self.below_entry, ecx=self.world, edx=self.below_at, stack_args=(self.out,), count=500_000_000)
        cpu.u32(ZONE_MODE, 1)
        rec = cpu.read(self.out, REC)
        out = [rec[0]]
        for j in range(32):
            h = struct.unpack_from('<10I', rec, 4 + 0x28 * j)
            out.extend(h[:8])
            out.append(self.material_index(h[8]) & 0xffffffff)
            out.append(self.node_index(h[9]) & 0xffffffff if h[9] else 0xffffffff)
        return out

    def pick(self, heights, soils, y, step, water_not_ground):
        cpu = self.cpu
        if not hasattr(self, 'pick_code'):
            self.pick_rec, self.pick_mats, self.pick_outs, self.pick_args = (
                self.alloc(REC), self.alloc(0x2c * 32), self.alloc(16), self.alloc(12))
            o, a = self.pick_outs, self.pick_args
            push_imm = lambda v: b'\x68' + struct.pack('<I', v)  # noqa: E731
            push_mem = lambda v: b'\xff\x35' + struct.pack('<I', v)  # noqa: E731
            code = (b'\xb9' + struct.pack('<I', 0) + b'\xba' + struct.pack('<I', self.pick_rec)
                    + push_imm(o + 12) + push_imm(o + 8) + push_mem(a + 8) + push_imm(o + 4)
                    + push_mem(a + 4) + push_mem(a))
            self.pick_code = cpu.code(code + b'\x90' * 5 + b'\xd9\x1d' + struct.pack('<I', o) + b'\xc3')
            at = self.pick_code + len(code)
            cpu.write(at, b'\xe8' + struct.pack('<i', PICK - (at + 5)))
        rec, mats, outs = self.pick_rec, self.pick_mats, self.pick_outs
        cpu.write(mats, bytes(0x2c * 32))
        buf = bytearray(REC)
        buf[0] = len(heights)
        for i, (h, s) in enumerate(zip(heights, soils)):
            struct.pack_into('<f', buf, 4 + 0x28 * i + 0x10, h)
            if s >= 0:
                cpu.u32(mats + 0x2c * i + 0x20, s)
                struct.pack_into('<I', buf, 4 + 0x28 * i + 0x20, mats + 0x2c * i)
        cpu.write(rec, bytes(buf))
        cpu.write(outs, struct.pack('<4i', -7, -7, -7, -7))
        cpu.write(self.pick_args, struct.pack('<ffI', y, step, water_not_ground))
        cpu.call(self.pick_code)
        return list(struct.unpack('<4I', cpu.read(outs, 16)))

    def place(self, pos, yaw, image):
        """The placement on the image's state; the image as the original left
        it, the log and the result."""
        cpu = self.cpu
        m = self.mech
        for a, b in MECH_RANGES:
            cpu.write(m + a, image[a:b])
        cpu.write(self.transform, image[T_OFF:T_OFF + 48])
        cpu.write(self.controls, image[C_OFF:C_OFF + 0x60])
        cpu.write(self.skis, image[S_OFF:S_OFF + 0x300])
        cpu.u32(self.skis + 0x3c, self.ski_wraps[0])
        cpu.u32(self.skis + 0x40, self.ski_wraps[1])
        for k in range(2):
            cpu.write(self.ski_mats[k], image[M_OFF + 48 * k:M_OFF + 48 * k + 48])
        p = self.alloc(12)
        cpu.write(p, struct.pack('<3f', *pos))
        cpu.u32(ZONE_MODE, 1)
        self.log = []
        self.logging = True
        self.reports = []
        cpu.call(PLACE, ecx=m, stack_args=(p, f2u(yaw)), count=500_000_000)
        self.logging = False
        r = cpu.uc.reg_read(19)
        self.log.extend([6, self.u32(ZONE_MODE)])
        out = bytearray(image)
        for a, b in MECH_RANGES:
            out[a:b] = cpu.read(m + a, b - a)
        out[T_OFF:T_OFF + 48] = cpu.read(self.transform, 48)
        out[C_OFF:C_OFF + 0x60] = cpu.read(self.controls, 0x60)
        out[S_OFF:S_OFF + 0x300] = cpu.read(self.skis, 0x300)
        for k in range(2):
            out[M_OFF + 48 * k:M_OFF + 48 * k + 48] = cpu.read(self.ski_mats[k], 48)
        return r, bytes(out), list(self.log), [rep[1] for rep in self.reports]

    def spawn(self, pos, yaw, cheat, option, pilot_80, has_weapons, image):
        """0x4782f0 on a mech image with 0x4161c0 stubbed: the image's
        fields as the original left them, and the log."""
        cpu = self.cpu
        m = self.mech
        for off, n in ((0x630, 1), (0x688, 4), (0xdc, 1), (0xde, 1), (0x587, 1)):
            cpu.write(m + off, image[off:off + n])
        cpu.write(self.weapons + 0x1c, image[0x700:0x704])
        cpu.u32(m + 0xd4, self.weapons if has_weapons else 0)
        cpu.u32(self.pilot + 0x14, m)
        cpu.u32(self.pilot + 0x80, pilot_80)
        cpu.write(CHEAT, bytes([cheat]))
        self.spawn_option = option
        p = self.alloc(12)
        cpu.write(p, struct.pack('<3f', *pos))
        self.log = []
        cpu.call(SPAWN, ecx=self.pilot, stack_args=(p, yaw), count=5_000_000)
        out = bytearray(image)
        for off, n in ((0x630, 1), (0x688, 4), (0xdc, 1), (0xde, 1), (0x587, 1)):
            out[off:off + n] = cpu.read(m + off, n)
        if has_weapons:
            out[0x700:0x704] = cpu.read(self.weapons + 0x1c, 4)
        return cpu.uc.reg_read(19), bytes(out), list(self.log)

    def stub_spawn_calls(self):
        cpu = self.cpu
        cpu.stub_function(PLACE, lambda c: self.log.extend(
            [23] + list(struct.unpack('<3I', cpu.read(c.arg(0), 12))) + [c.arg(1)]) or 1, 8)
        cpu.stub_function(0x40cf20, lambda c: self.log.append(21) or self.spawn_option)


def port_point(x, y, z, zone_mode):
    out = (ctypes.c_int32 * (1 + 32 * 10))()
    r = probe.pl_point(x, y, z, zone_mode, out)
    return r, [v & 0xffffffff for v in out]


def port_below(x, y, z, zone_mode):
    out = (ctypes.c_int32 * (1 + 32 * 10))()
    probe.pl_below(x, y, z, zone_mode, out)
    return [v & 0xffffffff for v in out]


def port_pick(heights, soils, y, step, water_not_ground):
    n = len(heights)
    hs = (ctypes.c_float * max(n, 1))(*heights)
    ss = (ctypes.c_int32 * max(n, 1))(*soils)
    out = (ctypes.c_int32 * 4)()
    probe.pl_pick(n, hs, ss, y, step, water_not_ground, out)
    return [v & 0xffffffff for v in out]


def port_place(pos, yaw, image):
    buf = ctypes.create_string_buffer(bytes(image), IMAGE)
    p = (ctypes.c_float * 3)(*pos)
    log = (ctypes.c_int32 * 512)()
    r = ctypes.c_int32()
    n = probe.pl_place(p, yaw, buf, log, 512, ctypes.byref(r))
    if n > 512:
        raise SystemExit('the port\'s log overflowed')
    return r.value, buf.raw, [v & 0xffffffff for v in log[:n]]


def ranges(image):
    """The compared parts of an image: the mech's fields, the matrix, the
    controls' floats, the skis' fields and the ski matrices."""
    parts = [image[a:b] for a, b in MECH_RANGES]
    parts.append(image[T_OFF:T_OFF + 48])
    parts += [image[C_OFF + a:C_OFF + b] for a, b in CTL_RANGES]
    parts += [image[S_OFF + a:S_OFF + b] for a, b in SKI_RANGES]
    parts.append(image[M_OFF:M_OFF + 96])
    return b''.join(parts)


def first_diff(a, b):
    i = next(i for i in range(min(len(a), len(b))) if a[i] != b[i]) if a != b else -1
    return i


def make_image(rng):
    img = bytearray(IMAGE)
    img[0x30:0x64] = rnd_floats(rng, 13, -5, 5)
    q = [rng.uniform(-1, 1) for _ in range(4)]
    img[0x48:0x58] = struct.pack('<4f', *q)
    count = rng.choice([0, 1, 11, 11, 15, rng.randrange(1, 16)])
    struct.pack_into('<i', img, 0x120, count)
    img[0x124:0x340] = rnd_floats(rng, 135, -12, 12)
    img[T_OFF:T_OFF + 48] = rnd_floats(rng, 12, -1, 1)
    img[C_OFF:C_OFF + 0x60] = rnd_floats(rng, 24, -1, 1)
    img[S_OFF:S_OFF + 0x300] = rnd_floats(rng, 0xc0, -30, 30)
    ht, half = f32(rng.uniform(1, 4)), f32(rng.choice([6.0, 2.0, rng.uniform(2, 7)]))
    struct.pack_into('<f', img, S_OFF + 0x78, ht)
    struct.pack_into('<4f', img, S_OFF + 0x2ec, 2 * half, half, 0.0, 0.0)
    struct.pack_into('<2f', img, S_OFF + 0x2f8, f32(1 / half), f32(1 / (half * half)))
    img[M_OFF:M_OFF + 96] = rnd_floats(rng, 24, -2, 2)
    return bytes(img)


def check_polygons(emu, rng, count):
    """0x54d640 on crafted polygons: triangles and quads, vertical walls
    (normal y 0), slivers, points on vertices and edges."""
    vertical = inside = 0
    for k in range(count):
        n = rng.choice([3, 3, 4, 5])
        kind = rng.choice(['flat', 'flat', 'wall', 'sliver'])
        cx, cz = rng.uniform(-100, 100), rng.uniform(-100, 100)
        if kind == 'wall':  # every vertex on one line in x and z
            dx, dz = rng.uniform(-1, 1), rng.uniform(-1, 1)
            ts = sorted(rng.uniform(-10, 10) for _ in range(n))
            verts = [v for t in ts for v in (cx + dx * t, rng.uniform(-10, 10), cz + dz * t)]
        else:
            r = 10 if kind == 'flat' else 0.01
            angles = sorted(rng.uniform(0, 2 * math.pi) for _ in range(n))
            if rng.random() < 0.5:
                angles.reverse()
            verts = [v for a in angles for v in (cx + r * math.cos(a) * rng.uniform(0.5, 1), rng.uniform(-10, 10),
                                                 cz + r * math.sin(a) * rng.uniform(0.5, 1))]
        verts = [f32(v) for v in verts]
        j = rng.randrange(n)
        x, z = rng.choice([(verts[3 * j], verts[3 * j + 2]),
                           (f32((verts[0] + verts[3]) / 2), f32((verts[2] + verts[5]) / 2)),
                           (f32(cx + rng.uniform(-12, 12)), f32(cz + rng.uniform(-12, 12)))])
        slot = [rng.getrandbits(32) for _ in range(5)]
        want = emu.polygon(verts, x, z, slot)
        out = (ctypes.c_int32 * 5)()
        probe.pl_polygon((ctypes.c_float * len(verts))(*verts), n, x, z, (ctypes.c_int32 * 5)(*slot), out)
        got = [v & 0xffffffff for v in out]
        if want != got:
            raise SystemExit(f'polygon {k} ({kind}) {verts} at {x} {z}: {want} vs {got}')
        inside += want[0]
        vertical += want[0] and want[2] in (0, 0x80000000)
    return inside, vertical


BELOWS = [0, 0]  # 0x4eda10 queries, those that chose among several hits


def check_point(emu, rng, x, z, y, mode, where):
    r, want, reports = emu.point(x, y, z, mode)
    pr, got = port_point(x, y, z, mode)
    if r != pr:
        raise SystemExit(f'{where}: result {r} vs {pr}')
    if want != got:
        i = first_diff(want, got)
        raise SystemExit(f'{where}: records differ at slot {(i - 1) // 10} word {(i - 1) % 10}: '
                         f'{want[i]:#x} vs {got[i]:#x}')
    if reports:
        raise SystemExit(f'{where}: the original reported {reports}')
    wb, gb = emu.below(x, y, z, mode), port_below(x, y, z, mode)
    if wb != gb:
        i = first_diff(wb, gb)
        raise SystemExit(f'{where}: the kept hit (0x4eda10) differs at slot {(i - 1) // 10} word {(i - 1) % 10}: '
                         f'{wb[i]:#x} vs {gb[i]:#x}')
    BELOWS[0] += 1
    BELOWS[1] += wb[0] == 1 and want[0] > 1
    return want


def check_pick(emu, rng, heights, where):
    n = len(heights)
    soils = [rng.choice([-1, 0, 0, 1, 1, 2, 10]) for _ in range(n)]
    y = f32(rng.choice([heights[0] if heights else 0.0, rng.uniform(-60, 120)]) + rng.choice([0.0, 0.0, 1.0, -1.0]))
    step = f32(rng.choice([y + 10, 4.0, 20.0, 10.0, rng.uniform(-5, 30)]))
    if heights and rng.random() < 0.3:  # on the boundaries: a polygon, or the water, exactly step above y
        y, step = float(rng.randrange(-40, 40)), float(rng.randrange(-10, 20))
        heights = list(heights)
        heights[rng.randrange(n)] = y + step
        soils = [rng.choice([1, -1, 0]) for _ in range(n)]
    p4 = rng.choice([0, 1])
    want = emu.pick(heights, soils, y, step, p4)
    got = port_pick(heights, soils, y, step, p4)
    if want != got:
        raise SystemExit(f'{where}: pick {heights} {soils} y {y} step {step} {p4}: {want} vs {got}')


def check_place(emu, rng, pos, where):
    yaw = f32(rng.uniform(-math.pi, math.pi))
    image = make_image(rng)
    r, want, log, reports = emu.place(pos, yaw, image)
    pr, got, plog = port_place(pos, yaw, image)
    if (r, log) != (pr, plog):
        i = first_diff(log, plog)
        raise SystemExit(f'{where}: placement at {pos} yaw {yaw}: result {r} {pr}, logs differ at {i}:\n'
                         f'  {[hex(v) for v in log]}\n  {[hex(v) for v in plog]}')
    a, b = ranges(want), ranges(got)
    if a != b:
        i = first_diff(a, b)
        raise SystemExit(f'{where}: placement at {pos} yaw {yaw}: the image differs at compared byte {i}')
    if reports:
        raise SystemExit(f'{where}: the original reported {reports}')
    return log


def main():
    queries = int(sys.argv[1]) if len(sys.argv) > 1 else 600
    rng = random.Random(0x4161c0)
    total = hits = places = landed = picks = wrapped = stacked = 0
    for wi, name in enumerate(WORLDS):
        emu = Place(name)
        if probe.sw_load(str(ZBD / name / 'gamez.zbd').encode()) != 0:
            raise SystemExit(f'{name}: the port could not read the world')
        probe.di_reset()
        probe.pl_reset()
        attached = emu.add_world_children([1, 9])
        x0, z0, w, d = emu.area
        strips = boxed = 0
        for q in range(queries):
            if q == queries // 2:
                strips = emu.make_strips(rng)
                boxed = emu.set_flags(rng)
            wrap = int(rng.random() < 0.3)
            emu.cpu.u32(emu.wd + 0x50, wrap)
            probe.di_wrap(wrap)
            if rng.random() < 0.1:
                x = x0 + rng.choice([-0.3, 1.3, rng.uniform(-1, 2)]) * w
                z = z0 + rng.choice([-0.3, 1.3, rng.uniform(-1, 2)]) * d
                wrapped += wrap
            else:
                x, z = x0 + rng.uniform(0.01, 0.99) * w, z0 + rng.uniform(0.01, 0.99) * d
            if rng.random() < 0.2:  # on round coordinates, on polygons' edges and corners
                step = rng.choice([1.0, 4.0, 8.0, 16.0])
                x, z = round(x / step) * step, round(z / step) * step
            x, z = f32(x), f32(z)
            where = f'{name} query {q} at ({x}, {z}), wrap {wrap}'
            mode = rng.choice([0, 0, 1])
            top = check_point(emu, rng, x, z, 2000.0, mode, where + ' from 2000')
            ground = struct.unpack('<f', struct.pack('<I', top[5]))[0] if top[0] else 0.0
            y = f32(rng.choice([ground + rng.uniform(-15, 15), ground + rng.uniform(-150, 150), rng.uniform(-60, 120),
                                ground]))
            hs = sorted(struct.unpack('<f', struct.pack('<I', top[1 + 10 * i + 4]))[0] for i in range(top[0]))
            if len(hs) >= 2 and rng.random() < 0.7:
                # Stacked polygons h1 < h2 with y where the pick's reach, 2y + 10
                # (docs/mission-start.md section 11), takes one and y + 10 the other.
                i = rng.randrange(len(hs) - 1)
                h1, h2 = hs[i], hs[i + 1]
                y = f32((max((h2 - 10) / 2, h1 - 10) if h2 > 10 else max(h2 - 10, (h1 - 10) / 2)) + 100)
                stacked += 1
            rec = check_point(emu, rng, x, z, y, mode, where + f' from {y}')
            total += 2
            hits += rec[0]
            heights = [struct.unpack('<f', struct.pack('<I', rec[1 + 10 * i + 4]))[0] for i in range(rec[0])]
            check_pick(emu, rng, heights, where)
            check_pick(emu, rng, [f32(rng.uniform(-50, 50)) for _ in range(rng.randrange(0, 9))], where + ' crafted')
            picks += 2
            pos = (x, f32(y - 100.0), z)  # the placement looks from 100 above
            log = check_place(emu, rng, pos, where)
            places += 1
            landed += 3 in log and next(log[i + 2] for i in range(len(log)) if log[i] == 3) != f2u(pos[1])
            if q % 50 == 49 or q == queries - 1:
                models, flags = emu.state()
                pm, pf = port_state(len(models), len(flags))
                if models != pm:
                    i = first_diff(models, pm)
                    raise SystemExit(f'{where}: model {i} cached polygon {models[i]} vs {pm[i]}')
                if flags != pf:
                    i = first_diff(flags, pf)
                    raise SystemExit(f'{where}: node {i} flags {flags[i]:#x} vs {pf[i]:#x}')
        print(f'{name}: {queries} points, picks and placements identical ({attached} nodes added to the world); the '
              f'second half with {strips} polygons made strips and {boxed} nodes boxed')
        if wi == 0:
            inside, vertical = check_polygons(emu, rng, 3000)
            print(f'0x54d640: 3000 crafted polygons identical ({inside} holding the point, {vertical} of them vertical)')
            if not vertical:
                raise SystemExit('coverage: no vertical polygon held its point')
            emu.stub_spawn_calls()
            for k in range(200):
                image = bytearray(IMAGE)
                image[0x630] = rng.randrange(2)
                struct.pack_into('<I', image, 0x688, rng.getrandbits(32))
                image[0xdc], image[0xde], image[0x587] = rng.randrange(2), rng.randrange(2), rng.randrange(3)
                struct.pack_into('<i', image, 0x700, rng.randrange(3))
                pos = tuple(f32(rng.uniform(-3000, 3000)) for _ in range(3))
                yaw = f2u(rng.uniform(-4, 4))
                cheat, option, p80, weapons = rng.choice([0, 0, 1, 7]), rng.choice([0, 1]), rng.randrange(3), \
                    rng.choice([0, 1, 1])
                r, want, log = emu.spawn(pos, yaw, cheat, option, p80, weapons, bytes(image))
                buf = ctypes.create_string_buffer(bytes(image), IMAGE)
                plog = (ctypes.c_int32 * 64)()
                p = (ctypes.c_float * 3)(*pos)
                res = ctypes.c_int32()
                n = probe.pl_spawn(p, yaw, cheat, option, p80, weapons, buf, plog, 64, ctypes.byref(res))
                pr = res.value
                pl = [v & 0xffffffff for v in plog[:n]]
                pl = [v for v in pl if v != 20]  # the cheat flag is a read in the exe
                fields = [(0x630, 1), (0x688, 4), (0xdc, 1), (0xde, 1), (0x587, 1), (0x700, 4)]
                a = b''.join(want[o:o + k] for o, k in fields)
                b = b''.join(buf.raw[o:o + k] for o, k in fields)
                if (r, log, a) != (pr, pl, b):
                    raise SystemExit(f'spawn {k}: {r} {pr}\n  {log}\n  {pl}\n  {a.hex()}\n  {b.hex()}')
            print('the spawn 0x4782f0: 200 cases identical')
    print('reached:', ', '.join(f'{k} {v}' for k, v in COVERED.items()))
    if not hits or not landed or not wrapped or not all(COVERED.values()):
        raise SystemExit(f'coverage: {hits} hits, {landed} placements moved by the ground, {wrapped} wrapped points')
    if BELOWS[1] == 0:
        raise SystemExit('coverage: no kept-hit query chose among several hits')
    print(f'ok: {total} point queries ({hits} hits, {wrapped} wrapped off the grid), {picks} picks, {places} placements '
          f'({landed} moved to the ground, {stacked} over stacked polygons) on the {len(WORLDS)} worlds identical; '
          f'{BELOWS[0]} kept-hit queries (0x4eda10), {BELOWS[1]} choosing among stacked hits')


if __name__ == '__main__':
    main()
    sys.stdout.flush()
    os._exit(0)
