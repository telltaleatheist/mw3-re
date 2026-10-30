"""The ported puffer system (gmod_puff, docs/puffers.md) against the original
x86 in Unicorn, the FPU at 24 bits as in play.

    cmake --build build && .venv/bin/python tests/differential/test_puffers.py [runs]
      --fixed     the original with the near-fade fix patched in, against the
                  port with zfix bugs on (the port's FIX (bug))
      MW3_DUMP=1  the first difference in full

Parts, each against the original function on the same inputs:

1. zmth::exp_neg (0x523f70, its table built at 24 bits) on every table step
   and random inputs; zmth::euler_to_quat (0x525340) on random angles.
2. Simulation runs: random puffers made by 0x54a710 and set through every
   setter (0x54ab40..0x54b460: textures timed or not, colours, growth, fades,
   friction, numbers, intervals by time and distance, nodes), the wind
   (0x549410, 0x54b630..0x54b670), then many frames of the tick 0x5490e0
   with random frame steps, moving emitters and nodes, switching puffers on
   and off. Every puffer and particle (all fields, every track and iterator)
   and the wind compared after every call. The calls into code not ported
   (rand from the same seed, the node's world rotation and position
   0x4ed5d0 / 0x4ed530, the tick node's zClass calls, zerr) answer the same
   on both sides and are recorded. Shutdown 0x5495a0 ends each run.
3. Drawing: 0x5496c0 (0x5489a0, the cull 0x541760, the sprite 0x567720)
   under random cameras and screen rects, the draw table entries [0x72f3c0]
   / [0x72f3bc] recorded: entry, count, record, alpha, the four TL vertices
   0x567720 writes and the colours in the work records.
4. The animation event 0x5035b0 (with 0x509520 / 0x509260 and the texture
   lookups 0x518220 / 0x518210) on random events against a def with puffer
   and node refs: the puffers, the def's puffer list and the index cached in
   the event. Then the event-made puffers run frames.
5. On the world c1 (read by both, as test_scenewalk.py): 0x4ed5d0 / 0x4ed530
   (zcls::node_world_euler / _position) on every node, then puffers at real
   nodes run frames with the real ones on both sides.

The original runs as shipped and the port with zfix bugs off; with --fixed
the original has the near-fade fix patched in (0x548b4c: +0x3c -> +0x34) and
the port its FIX (bug) on, so both paths are checked.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

os.environ.setdefault('MW3_X87', '24')
sys.path.insert(0, str(Path(__file__).parent))
from original import FTOL, IAT_FTOL, ROOT, Original  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
probe.pf_reset.argtypes = [ctypes.c_uint32, ctypes.c_int32, ctypes.c_int32]
probe.pf_set.argtypes = [ctypes.c_int32, ctypes.c_int32, ctypes.c_void_p]
probe.pf_wind.argtypes = [ctypes.c_int32, ctypes.c_void_p]
probe.pf_tick.argtypes = [ctypes.c_float]
probe.pf_nodes.argtypes = [ctypes.c_int32, ctypes.c_void_p, ctypes.c_void_p]
probe.pf_camera.argtypes = [ctypes.c_void_p] * 4 + [ctypes.c_int32]
probe.pf_dump.argtypes = [ctypes.c_void_p, ctypes.c_int32]
probe.pf_log.argtypes = [ctypes.c_void_p, ctypes.c_int32]
probe.pf_def.argtypes = [ctypes.c_int32, ctypes.c_char_p, ctypes.c_void_p, ctypes.c_int32, ctypes.c_void_p,
                         ctypes.c_int32]
probe.pf_event.argtypes = [ctypes.c_void_p, ctypes.c_int32]
probe.pf_def_dump.argtypes = [ctypes.c_void_p]
probe.pf_exp.argtypes = [ctypes.c_float]
probe.pf_exp.restype = ctypes.c_float
probe.pf_quat.argtypes = [ctypes.c_float] * 3 + [ctypes.c_void_p]
probe.pf_node_world.argtypes = [ctypes.c_int32, ctypes.c_void_p]

FIXED = '--fixed' in sys.argv
if FIXED:
    sys.argv.remove('--fixed')
    probe.probe_set_bugs(1)  # the port's FIX (bug) on, against the patched original
DUMP = os.environ.get('MW3_DUMP') == '1'

IAT_RAND, IAT_REALLOC, IAT_STRNCPY = 0x5bb668, 0x5bb610, 0x5bb648
NEW, DELETE, ATEXIT, ZERR = 0x5abd26, 0x5abd1a, 0x5ac140, 0x49bfd0
PUFFERS, PCOUNT, PARTS, QCOUNT, TICK_NODE = 0x72be04, 0x72be08, 0x72be14, 0x72be18, 0x72be58
WIND = (0x72be28, 0x72be2c, 0x72be30, 0x72be38, 0x72be3c, 0x72be40, 0x72be44, 0x72be48, 0x72be4c, 0x72be50,
        0x72be54)
TEX_TABLE, TEX_COUNT, DEFAULT_ENTRY = 0x610484, 0x610480, 0x5ecbf8
DT, VIEW, CAMERA_PTR, RECT, HARDWARE = 0x72c52c, 0x73fbc0, 0x7402f4, 0x7402f8, 0x72f348
PROJ = (0x73fb80, 0x73fb88, 0x73fb8c, 0x73fb90, 0x73fb98, 0x73fc30)
TL, WORK = 0x72f41c, 0x7481a8
CREATE, TICK, RESET, SHUTDOWN, DRAW, EVENT = 0x54a710, 0x5490e0, 0x549410, 0x5495a0, 0x5496c0, 0x5035b0
EXP, QUAT, EULER, POSITION = 0x523f70, 0x525340, 0x4ed5d0, 0x4ed530
HEAP = 0x40000000
# op -> (address, kind): kind e = EDX int, v = EDX vec3, f = stack float, ff = two
# stack floats, x = no argument, t = EDX texture entry, tt = entry + stack time,
# c = stack (time, r, g, b, a), g = stack (time, growth), n = EDX node.
SETTERS = {0: (0x54ab40, 'e'), 1: (0x54ab60, 'n'), 2: (0x54ab70, 'v'), 3: (0x54ab90, 'v'), 4: (0x54abb0, 'v'),
           5: (0x54abd0, 'v'), 6: (0x54abf0, 'v'), 7: (0x54ac10, 'v'), 8: (0x54ac30, 'e'), 9: (0x54ac40, 'f'),
           10: (0x54ac70, 'f'), 11: (0x54ac80, 'f'), 12: (0x54ac90, 'f'), 13: (0x54aca0, 'f'),
           14: (0x54acb0, 'f'), 15: (0x54acc0, 'f'), 16: (0x54acd0, 'f'), 17: (0x54ace0, 'ff'),
           18: (0x54ad30, 'ff'), 19: (0x54ad80, 'f'), 20: (0x54ad90, 'f'), 21: (0x54ada0, 'e'),
           22: (0x54adb0, 'x'), 23: (0x54adf0, 't'), 24: (0x54afc0, 'tt'), 25: (0x54b190, 'x'),
           26: (0x54b1d0, 'c'), 27: (0x54b420, 'x'), 28: (0x54b460, 'g')}
WIND_SET = {0: 0x54b630, 1: 0x54b650, 2: 0x54b660, 3: 0x54b670}
NODE_STRIDE = 0x10000   # fake node n at n * 0x10000 on both sides
TICK_NODE_ID = 0x77
REAL = 0x100000         # node ids from here: world pool index + REAL

FTOL_WORDS = 1 << 22
buf = (ctypes.c_int32 * FTOL_WORDS)()


def f2u(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def u2f(u):
    return struct.unpack('<f', struct.pack('<I', u & 0xffffffff))[0]


def s32(x):
    x &= 0xffffffff
    return x - (1 << 32) if x >= 1 << 31 else x


def f32(x):
    return u2f(f2u(x))


class MsvcRand:
    def __init__(self, seed):
        self.state = seed

    def __call__(self, cpu=None):
        self.state = (self.state * 214013 + 2531011) & 0xffffffff
        return (self.state >> 16) & 0x7fff


# ---------------------------------------------------------------- the original

class Emu:
    def __init__(self, world=None, heap=HEAP):
        """world: phase B, the test_scenewalk Walk whose emulator (with a world
        read) runs the puffers, with the real 0x4ed5d0 / 0x4ed530."""
        self.world = world
        if world:
            cpu = self.cpu = world.cpu
        else:
            cpu = self.cpu = Original()
            cpu.import_code(IAT_FTOL, FTOL)
        cpu.fpu_control(0x007f)  # play precision
        cpu.uc.mem_map(heap, 0x8000000)
        self.next, self.sizes = heap, {}
        self.rand = MsvcRand(1)
        self.log = []
        self.nodes = {}
        cpu.stub_function(NEW, lambda c: self.alloc(c.arg(0)))
        cpu.stub_function(DELETE, lambda c: 0)
        cpu.stub_function(ATEXIT, lambda c: 0)
        cpu.stub_import(IAT_RAND, lambda c: self.rand())
        cpu.stub_import(IAT_REALLOC, self.realloc)
        cpu.stub_import(IAT_STRNCPY, self.strncpy)
        cpu.stub_function(ZERR, self.zerr)
        cpu.stub_function(0x4fb740, lambda c: (self.log.append([1]), TICK_NODE_ID * NODE_STRIDE)[1])
        cpu.stub_function(0x4eb5b0, lambda c: (self.log.append([2, self.nid(c.ecx), s32(c.edx)]), 0)[1])
        cpu.stub_function(0x4eb450, lambda c: (self.log.append(
            [3, self.nid(c.ecx), 1 if c.edx == TICK else 2 if c.edx else 0]), 0)[1])
        cpu.stub_function(0x4eaea0, lambda c: (self.log.append([4, self.nid(c.ecx)]), 0)[1])
        if not world:  # on the world the node update (0x4f7e00) runs for real on both sides
            cpu.stub_function(EULER, lambda c: self.node_table(c, 7, 0))
            cpu.stub_function(POSITION, lambda c: self.node_table(c, 8, 1))
        cpu.stub_import(0x72f3c0, lambda c: self.draw(c, 0), 8)
        cpu.stub_import(0x72f3bc, lambda c: self.draw(c, 1), 12)
        cpu.stub_function(0x566620, lambda c: self.log.append(['software']))
        if FIXED:
            cpu.write(0x548b4c, b'\x34')  # the port's FIX (bug): near_start, not far_start
        cpu.call(0x548d90)  # the two lists' static constructors
        cpu.call(0x548e60)
        self.scratch = self.alloc(0x400)
        self.camera_data = self.alloc(0x200)

    def alloc(self, n):
        a = self.next
        self.next += (max(n, 1) + 0xf) & ~0xf
        self.sizes[a] = n
        return a

    def realloc(self, c):
        old, n = c.arg(0), c.arg(1)
        a = self.alloc(n)
        if old:
            c.write(a, c.read(old, min(self.sizes.get(old, 0), n)))
        return a

    def strncpy(self, c):
        dst, src, n = c.arg(0), c.arg(1), c.arg(2)
        s = self.cpu.read(src, n).split(b'\0')[0]
        c.write(dst, (s + bytes(n))[:n])
        return dst

    def zerr(self, c):
        fmt = c.read(c.arg(3), 256).split(b'\0')[0].decode('latin-1')
        if '%s' in fmt:
            fmt = fmt.replace('%s', c.read(c.arg(4), 64).split(b'\0')[0].decode('latin-1'), 1)
        self.log.append([5, s32(c.arg(2)), len(fmt)] + [ord(ch) for ch in fmt])
        return 0

    def nid(self, p):
        if p == 0:
            return 0
        if self.world and self.world.node_index(p) >= 0:
            return REAL + self.world.node_index(p)
        return p // NODE_STRIDE

    def node_table(self, c, kind, which):
        i = self.nid(c.ecx)
        self.log.append([kind, i])
        c.write(c.edx, struct.pack('<3f', *self.nodes[i][which]))
        return 0

    def draw(self, c, entry):
        tex = c.edx
        rec = -2 if tex == 0 else -1 if tex == 0x6fff0000 else (tex - 0x70000000) // 16
        out = [6, entry, s32(c.ecx), rec, s32(c.arg(0 if entry == 0 else 1)), s32(c.arg(1 if entry == 0 else 2))]
        tl = c.read(TL, 4 * 32)
        for i in range(4):
            out += [s32(x) for x in struct.unpack_from('<3I', tl, 32 * i)]
            out += [s32(x) for x in struct.unpack_from('<2I', tl, 32 * i + 0x18)]
        if entry == 0:
            w = c.read(WORK, 4 * 32)
            for i in range(4):
                out += [s32(x) for x in struct.unpack_from('<3I', w, 32 * i + 0xc)]
        self.log.append(out)
        return 0

    # --- memory ---
    def u32(self, a):
        return struct.unpack('<I', self.cpu.read(a, 4))[0]

    def reset(self, seed, ntex):
        cpu = self.cpu
        cpu.call(SHUTDOWN)
        cpu.write(0x72be28, bytes(0x34))
        self.rand = MsvcRand(seed)
        for i in range(ntex):
            cpu.write(TEX_TABLE + 0x28 * i, struct.pack('<II20sIII', 0, 0x70000000 + 16 * i,
                                                         f'tex{i}'.encode(), 1, 0, 0))
        cpu.u32(TEX_COUNT, ntex)
        cpu.u32(DEFAULT_ENTRY + 4, 0x6fff0000)
        cpu.u32(HARDWARE, 1)
        self.log = []

    def puffer_list(self):
        head = self.u32(PUFFERS)
        out, n = [], self.u32(head)
        while n != head:
            out.append(self.u32(n + 8))
            n = self.u32(n)
        return out

    def particle_list(self):
        head = self.u32(PARTS)
        out, n = [], self.u32(head)
        while n != head:
            out.append(self.u32(n + 8))
            n = self.u32(n)
        return out

    def create(self):
        self.cpu.call(CREATE)
        return len(self.puffer_list()) - 1

    def set(self, op, index, a):
        cpu = self.cpu
        p = self.puffer_list()[index]
        addr, kind = SETTERS[op]
        if kind == 'e':
            cpu.call(addr, ecx=p, edx=a[0])
        elif kind == 'n':
            cpu.call(addr, ecx=p, edx=self.node_ptr(s32(a[0])))
        elif kind == 'v':
            cpu.write(self.scratch, struct.pack('<3I', *a[:3]))
            cpu.call(addr, ecx=p, edx=self.scratch)
        elif kind == 'f':
            cpu.call(addr, ecx=p, stack_args=(a[0],))
        elif kind == 'ff':
            cpu.call(addr, ecx=p, stack_args=(a[0], a[1]))
        elif kind == 'x':
            cpu.call(addr, ecx=p)
        elif kind == 't':
            cpu.call(addr, ecx=p, edx=self.entry(s32(a[0])))
        elif kind == 'tt':
            cpu.call(addr, ecx=p, edx=self.entry(s32(a[0])), stack_args=(a[1],))
        elif kind == 'c':
            cpu.call(addr, ecx=p, stack_args=tuple(a[:5]))
        elif kind == 'g':
            cpu.call(addr, ecx=p, stack_args=(a[0], a[1]))

    def node_ptr(self, i):
        if i == 0:
            return 0
        if i >= REAL:
            return self.world.node_ptr(i - REAL)
        return i * NODE_STRIDE

    def entry(self, i):
        return DEFAULT_ENTRY if i < 0 else TEX_TABLE + 0x28 * i

    def wind(self, op, v):
        if op == 0:
            self.cpu.write(self.scratch, struct.pack('<3f', *v))
            self.cpu.call(WIND_SET[0], ecx=self.scratch)
        else:
            self.cpu.call(WIND_SET[op], stack_args=(f2u(v[0]),))

    def tick_reset(self):
        self.cpu.call(RESET)

    def shutdown(self):
        self.cpu.call(SHUTDOWN)

    def tick(self, dt):
        self.cpu.u32(DT, f2u(dt))
        self.cpu.call(TICK)

    def camera_set(self, view, proj, cam, rect, hardware):
        cpu = self.cpu
        cpu.write(VIEW, struct.pack('<12f', *view))
        for a, v in zip(PROJ, proj):
            cpu.write(a, struct.pack('<f', v))
        for off, k in ((0x2c, 0), (0xb8, 3), (0x1b8, 6), (0xc4, 9), (0x1c4, 12)):
            cpu.write(self.camera_data + off, struct.pack('<3f', *cam[k:k + 3]))
        cpu.u32(CAMERA_PTR, self.camera_data)
        cpu.write(RECT + 4, struct.pack('<2f', rect[0], rect[1]))
        cpu.write(RECT + 0x1c, struct.pack('<2f', rect[2], rect[3]))
        cpu.u32(HARDWARE, hardware)

    def draw_all(self):
        if self.world:  # test_scenewalk's Walk stubs 0x5496c0: run it from its second instruction
            self.cpu.uc.reg_write(UC_X86_REG_EAX, self.u32(QCOUNT))
            self.cpu.call(DRAW + 5)
        else:
            self.cpu.call(DRAW)

    def tex_id(self, p):
        if p == 0:
            return -2
        if p == DEFAULT_ENTRY:
            return -1
        return (p - TEX_TABLE) // 0x28

    def tracks(self, offs, out):
        """The three tracks whose begin pointers are at offs: textures, colours,
        growth (end pointers follow each)."""
        cpu = self.cpu
        b, e = self.u32(offs[0]), self.u32(offs[0] + 4)
        out.append((e - b) // 8)
        for k in range(b, e, 8):
            t, p = struct.unpack('<II', cpu.read(k, 8))
            out += [s32(t), self.tex_id(p)]
        for o, size in ((offs[1], 0x14), (offs[2], 8)):
            b, e = self.u32(o), self.u32(o + 4)
            out.append((e - b) // size)
            out += [s32(x) for x in struct.unpack(f'<{(e - b) // 4}I', cpu.read(b, e - b))] if e > b else []

    def dump(self):
        cpu = self.cpu
        out = [s32(self.u32(a)) for a in WIND]
        out.append(self.nid(self.u32(TICK_NODE)))
        ps = self.puffer_list()
        if self.u32(PCOUNT) != len(ps):
            raise SystemExit(f'puffer count {self.u32(PCOUNT)} vs list {len(ps)}')
        out.append(len(ps))
        for p in ps:
            raw = cpu.read(p, 0xe4)
            w = lambda o: s32(struct.unpack_from('<I', raw, o)[0])  # noqa: E731
            out += [raw[0], w(4), raw[8]]
            self.tracks((p + 0x10, p + 0x20, p + 0x30), out)
            out.append(raw[0x3c])
            out += [w(o) for o in range(0x40, 0x70, 4)]
            out.append(self.nid(w(0x70) & 0xffffffff))
            out += [w(o) for o in range(0x74, 0x80, 4)]
            out.append(raw[0x80])
            out += [w(o) for o in range(0x84, 0xe4, 4)]
        qs = self.particle_list()
        if self.u32(QCOUNT) != len(qs):
            raise SystemExit(f'particle count {self.u32(QCOUNT)} vs list {len(qs)}')
        out.append(len(qs))
        for q in qs:
            raw = cpu.read(q, 0x90)
            w = lambda o: struct.unpack_from('<I', raw, o)[0]  # noqa: E731
            out += [s32(w(o)) for o in range(0, 0x54, 4)]
            out.append((w(0x54) - w(0x5c)) // 8)
            out.append((w(0x68) - w(0x70)) // 0x14)
            out.append((w(0x8c) - w(0x80)) // 8)
            self.tracks((q + 0x5c, q + 0x70, q + 0x80), out)
        return out

    # --- the animation event ---
    def def_setup(self, names, ref_puffer, node_ids, count_byte, def_node):
        cpu = self.cpu
        d = self.def_addr = self.alloc(0x13c)
        cpu.write(d, bytes(0x13c))
        cpu.write(d, b'puffanim\0')
        cpu.write(d + 0x10c, bytes([count_byte]))
        ps = self.puffer_list()
        if names:
            arr = self.alloc(0x2c * len(names))
            for i, (nm, rp) in enumerate(zip(names, ref_puffer)):
                cpu.write(arr + 0x2c * i, nm + struct.pack('<II', ps[rp] if rp >= 0 else 0, 0))
            cpu.u32(d + 0x120, arr)
        cpu.write(d + 0x10a, bytes([len(node_ids)]))
        if node_ids:
            arr = self.alloc(0x28 * len(node_ids))
            for i, n in enumerate(node_ids):
                cpu.write(arr + 0x28 * i, bytes(0x24) + struct.pack('<I', self.node_ptr(n)))
            cpu.u32(d + 0x118, arr)
        cpu.u32(d + 0x74, self.node_ptr(def_node))

    def event(self, ev):
        e = self.alloc(len(ev))
        self.cpu.write(e, ev)
        self.cpu.call(EVENT, ecx=self.def_addr, edx=e)
        return s32(self.cpu.uc.reg_read(UC_X86_REG_EAX)), self.cpu.read(e, len(ev))

    def def_dump(self):
        cpu, d = self.cpu, self.def_addr
        n = cpu.read(d + 0x10c, 1)[0]
        out = [n]
        arr = self.u32(d + 0x120)
        ps = self.puffer_list()
        for i in range(n if arr else 0):
            rec = cpu.read(arr + 0x2c * i, 0x2c)
            out += [s32(x) for x in struct.unpack_from('<9I', rec, 0)]
            p = struct.unpack_from('<I', rec, 0x24)[0]
            out.append(ps.index(p) if p in ps else -1)
        return out


# ---------------------------------------------------------------- the port

class Port:
    def reset(self, seed, ntex, real=0):
        probe.pf_reset(seed, ntex, real)

    def create(self):
        return probe.pf_create()

    def set(self, op, index, a):
        probe.pf_set(op, index, (ctypes.c_uint32 * 5)(*[x & 0xffffffff for x in (list(a) + [0] * 5)[:5]]))

    def wind(self, op, v):
        probe.pf_wind(op, (ctypes.c_float * 3)(*(list(v) + [0, 0, 0])[:3]))

    def tick_reset(self):
        probe.pf_tick_reset()

    def shutdown(self):
        probe.pf_shutdown()

    def tick(self, dt):
        probe.pf_tick(dt)

    def camera_set(self, view, proj, cam, rect, hardware):
        probe.pf_camera((ctypes.c_float * 12)(*view), (ctypes.c_float * 6)(*proj), (ctypes.c_float * 15)(*cam),
                        (ctypes.c_float * 4)(*rect), hardware)

    def draw_all(self):
        probe.pf_draw()

    def dump(self):
        n = probe.pf_dump(buf, FTOL_WORDS)
        return list(buf[:n])

    def take_log(self):
        n = probe.pf_log(buf, FTOL_WORDS)
        w, out, i = list(buf[:n]), [], 0
        sizes = {1: 1, 2: 3, 3: 3, 4: 2, 7: 2, 8: 2}
        while i < n:
            k = w[i]
            if k in sizes:
                out.append(w[i:i + sizes[k]])
                i += sizes[k]
            elif k == 5:
                m = w[i + 2]
                out.append(w[i:i + 3 + m])
                i += 3 + m
            elif k == 6:
                m = 6 + 20 + (12 if w[i + 1] == 0 else 0)
                out.append(w[i:i + m])
                i += m
            else:
                raise SystemExit(f'bad port log word {k}')
        return out

    def def_setup(self, names, ref_puffer, node_ids, count_byte, def_node):
        self.def_node = def_node
        probe.pf_def(len(names), b''.join(names), (ctypes.c_int32 * max(1, len(names)))(*ref_puffer),
                     len(node_ids), (ctypes.c_int32 * max(1, len(node_ids)))(*node_ids), count_byte)

    def event(self, ev):
        b = ctypes.create_string_buffer(bytes(ev), len(ev))
        r = probe.pf_event(b, self.def_node)
        return r, b.raw[:len(ev)]

    def def_dump(self):
        out = (ctypes.c_int32 * 4096)()
        n = probe.pf_def_dump(out)
        return list(out[:n])


# ---------------------------------------------------------------- comparing

def parse(words):
    """The dump as (value, is_float) pairs, so floats compare with a tolerance."""
    out, i = [], 0

    def take(n, fl):
        nonlocal i
        out.extend((w, fl) for w in words[i:i + n])
        i += n

    def tracks():
        n = words[i]
        take(1, False)
        for _ in range(n):
            take(1, True), take(1, False)
        n = words[i]
        take(1, False)
        take(5 * n, True)
        n = words[i]
        take(1, False)
        take(2 * n, True)
    take(11, True), take(1, False)
    n = words[i]
    take(1, False)
    for _ in range(n):
        take(3, False)
        tracks()
        take(1, False), take(12, True), take(1, False), take(3, True), take(1, False), take(24, True)
    n = words[i]
    take(1, False)
    for _ in range(n):
        take(21, True), take(3, False)
        tracks()
    if i != len(words):
        raise SystemExit(f'dump parse ended at {i} of {len(words)}')
    return out


def close(a, b, tol):
    if a == b:
        return True
    x, y = u2f(a), u2f(b)
    if x != x and y != y:
        return True
    return abs(x - y) <= tol * max(1.0, abs(x), abs(y))


class Compare:
    def __init__(self):
        self.exact = self.approx = 0
        self.worst = 0.0

    def state(self, what, want, got, tol):
        if len(want) != len(got):
            self.fail(what, f'dump lengths {len(want)} vs {len(got)}', want, got)
        if want == got:
            self.exact += len(want)
            return
        pw, pg = parse(want), parse(got)
        if len(pw) != len(pg):
            self.fail(what, 'structure', want, got)
        for k, ((a, fa), (b, _)) in enumerate(zip(pw, pg)):
            if a == b:
                self.exact += 1
            elif fa and close(a, b, tol):
                self.approx += 1
                x, y = u2f(a), u2f(b)
                self.worst = max(self.worst, abs(x - y) / max(1.0, abs(x), abs(y)))
            else:
                self.fail(what, f'word {k}: original {a:#x} ({u2f(a) if fa else a}) rebuilt {b:#x} '
                                f'({u2f(b) if fa else b})', want, got)

    def log(self, what, want, got, tol):
        if len(want) != len(got):
            first = next((k for k, (a, b) in enumerate(zip(want, got)) if a != b), min(len(want), len(got)))
            raise SystemExit(f'{what}: {len(want)} recorded calls vs {len(got)}, first difference at call {first}: '
                             f'{want[first][:8] if first < len(want) else None} vs '
                             f'{got[first][:8] if first < len(got) else None}')
        for a, b in zip(want, got):
            if a == b:
                continue
            if len(a) != len(b) or a[0] != b[0] or a[0] != 6 or a[:4] != b[:4] or a[5] != b[5]:
                raise SystemExit(f'{what}: call {a[:12]} vs {b[:12]}')
            for k in range(4, len(a)):
                if a[k] != b[k] and not close(a[k] & 0xffffffff, b[k] & 0xffffffff, tol):
                    raise SystemExit(f'{what}: draw word {k}: {u2f(a[k])} vs {u2f(b[k])}\n  {a}\n  {b}')

    def fail(self, what, msg, want, got):
        if DUMP:
            print('original', want[:400])
            print('rebuilt ', got[:400])
        raise SystemExit(f'{what}: {msg}')


# ---------------------------------------------------------------- inputs

def rf(rng, choices):
    c = rng.choice(choices)
    return c(rng) if callable(c) else c


def vec(rng, scale):
    return [f32(rng.uniform(-scale, scale)) for _ in range(3)]


def random_settings(rng, node_ids, ntex, exact=False):
    """A list of (op, args) setting up one puffer. exact: lifetimes, key
    times and intervals in powers of two, for runs of 0.125 s frames."""
    ops = []

    def f(x):
        return f2u(f32(x))
    ops.append((0, [rng.choice([1, 1, 1, 0, 2])]))
    if rng.random() < 0.5:
        ops.append((1, [rng.choice(node_ids)]))
    ops.append((2, [f2u(x) for x in vec(rng, 50)]))
    for op, s in ((3, 3), (4, 3), (5, 2), (6, 2), (7, 2)):
        if rng.random() < 0.7:
            ops.append((op, [f2u(x) for x in vec(rng, s)]))
    ops.append((8, [0 if exact else rng.choice([0, 0, 1, 2])]))
    ops.append((9, [f(rf(rng, [0.125, 0.25] if exact else
                         [0.1, 0.2, 0.05, 1.0, 2.0, 0.0, lambda r: r.uniform(0.01, 3)]))]))
    lo, hi = rng.uniform(0.1, 2), rng.uniform(0.1, 3)
    ops += [(10, [f(lo)]), (11, [f(hi)])]
    lo, hi = rf(rng, [0.5, lambda r: r.uniform(0.05, 2)]), rf(rng, [0.75, 0.1, lambda r: r.uniform(0.05, 3)])
    if exact:  # whole frames: ages land exactly on lifetimes and key times
        lo = hi = rng.choice([0.25, 0.5, 1.0])
    ops += [(12, [f(lo)]), (13, [f(hi)])]
    if not exact and rng.random() < 0.4:
        ops += [(14, [f(rng.uniform(0, 0.3))]), (15, [f(rng.uniform(0, 0.6))])]
    ops.append((16, [f(rf(rng, [0.0, 0.5, 0.25, lambda r: r.uniform(0, 3)]))]))
    if rng.random() < 0.4:
        a = rng.uniform(1, 40)
        ops.append((17, [f(a), f(rf(rng, [a, a + rng.uniform(1, 60)]))]))
    if rng.random() < 0.7:
        a = rng.uniform(5, 200)
        ops.append((18, [f(a), f(rf(rng, [a, a + rng.uniform(1, 300)]))]))
    ops.append((19, [f(rf(rng, [0.0, 1.0, 1.75, 0.3, lambda r: r.uniform(0, 6), lambda r: r.uniform(0, 0.02)]))]))
    if rng.random() < 0.5:
        ops.append((20, [f(rf(rng, [1.0, 0.0, lambda r: r.uniform(0, 3)]))]))
    if rng.random() < 0.3:
        ops.append((21, [rng.choice([0, 1, 2, 3, -1])]))
    if rng.random() < 0.85:
        ops.append((22, []))
        timed = rng.random() < 0.3
        for k in range(rng.randint(0, 4)):
            t = rng.choice(range(-1, ntex))
            ops.append((24, [t, f(k / 4 + rng.uniform(-0.05, 0.2))]) if timed else (23, [t]))
    if rng.random() < 0.4:
        ops.append((25, []))
        n = rng.randint(1, 5)
        times = sorted(rng.choice([0.0, 0.25, 0.5, 0.75]) if exact else rng.uniform(0, 1) for _ in range(n))
        if rng.random() < 0.2:
            rng.shuffle(times)
        for t in times:
            ops.append((26, [f(t), f(rng.uniform(0, 255)), f(rng.uniform(0, 255)), f(rng.uniform(0, 255)),
                             f(rf(rng, [1.0, 0.0, lambda r: r.uniform(0, 1.2)]))]))
    if rng.random() < 0.8:
        ops.append((27, []))
        n = rng.choice([3, 5] if exact else [0, 1, 2, 2, 2, 3, 5])
        for k in range(n):
            ops.append((28, [f(k / max(1, n - 1) if rng.random() < 0.8 or exact else rng.uniform(0, 1)),
                             f(rf(rng, [1.0, 2.0, 2.5, 0.15, lambda r: r.uniform(0, 4)]))]))
    return ops


def node_tables(rng, n, old=None, step=0.0):
    out = []
    for i in range(n):
        if old:
            e, p = old[i]
            e = [f32(x + rng.uniform(-step, step)) for x in e]
            p = [f32(x + rng.uniform(-step * 30, step * 30)) for x in p]
        else:
            e = [f32(rng.uniform(-3.2, 3.2)) for _ in range(3)]
            p = vec(rng, 100)
        out.append((e, p))
    return out


def random_camera(rng, centre):
    """A view looking at `centre` from a few units to a few hundred away."""
    import math
    dist = rf(rng, [lambda r: r.uniform(2, 30), lambda r: r.uniform(30, 300), lambda r: r.uniform(300, 900)])
    yaw, pitch = rng.uniform(-math.pi, math.pi), rng.uniform(-0.8, 0.8)
    fwd = [math.cos(pitch) * math.sin(yaw), math.sin(pitch), math.cos(pitch) * math.cos(yaw)]
    jitter = [rng.uniform(-0.3, 0.3) for _ in range(3)]
    fwd = [a + b for a, b in zip(fwd, jitter)]
    n = math.sqrt(sum(a * a for a in fwd))
    fwd = [a / n for a in fwd]
    pos = [c - d * dist for c, d in zip(centre, fwd)]
    up = [0.0, 1.0, 0.0]
    right = [up[1] * fwd[2] - up[2] * fwd[1], up[2] * fwd[0] - up[0] * fwd[2], up[0] * fwd[1] - up[1] * fwd[0]]
    n = math.sqrt(sum(a * a for a in right))
    right = [a / n for a in right]
    down = [fwd[1] * right[2] - fwd[2] * right[1], fwd[2] * right[0] - fwd[0] * right[2],
            fwd[0] * right[1] - fwd[1] * right[0]]
    m = []
    for k in range(3):
        m += [right[k], down[k], fwd[k]]
    m += [-sum(p * r for p, r in zip(pos, right)), -sum(p * r for p, r in zip(pos, down)),
          -sum(p * r for p, r in zip(pos, fwd))]
    near = rf(rng, [1.0, 0.5, lambda r: r.uniform(0.1, 4)])
    far = rf(rng, [1000.0, lambda r: r.uniform(dist * 0.5, dist * 2 + 50)])
    fx = rf(rng, [320.0, lambda r: r.uniform(150, 900)])
    fy = fx * rng.choice([1.0, 0.75, 1.2])
    proj = [near, rng.choice([320.0, 400.0, rng.uniform(0, 800)]), rng.choice([240.0, 300.0, rng.uniform(0, 600)]),
            1.0 / near, fy / near, fx / near]
    cam = pos + [p + d * near for p, d in zip(pos, fwd)] + fwd + [p + d * far for p, d in zip(pos, fwd)] + \
        [-d for d in fwd]
    rect = rng.choice([[0.0, 0.0, 640.0, 480.0], [0.0, 0.0, 800.0, 600.0],
                       [rng.uniform(0, 200), rng.uniform(0, 200), rng.uniform(300, 640), rng.uniform(250, 480)]])
    return [f32(x) for x in m], [f32(x) for x in proj], [f32(x) for x in cam], [f32(x) for x in rect]


# ---------------------------------------------------------------- tests

def both(emu, port, name, *args):
    getattr(emu, name)(*args)
    getattr(port, name)(*args)


def check_math(emu, rng):
    cpu = emu.cpu
    xs = [i / 51.0 for i in range(256)] + [(i + 0.5) / 51 for i in range(256)] + \
         [0.0, -0.0, -1.0, 5.0, 5.0001, 4.999, 1e-9, -1e-9, float('inf'), float('-inf'), float('nan'), 100.0] + \
         [rng.uniform(-1, 6) for _ in range(2000)] + [rng.uniform(0, 0.05) for _ in range(500)]
    bad = 0
    for x in xs:
        x = f32(x)
        cpu.call(EXP, stack_args=(f2u(x),))
        want = emu_fstp(emu)  # the result is on st(0)
        got = f2u(probe.pf_exp(x))
        if want != got and not (u2f(want) != u2f(want) and u2f(got) != u2f(got)):
            bad += 1
            if bad < 5:
                print(f'  exp_neg({x!r}): original {u2f(want)!r} rebuilt {u2f(got)!r}')
    table = struct.unpack('<256f', cpu.read(0x63d340, 1024))
    if bad:
        raise SystemExit(f'exp_neg: {bad} of {len(xs)} differ')
    qbad = 0
    out = (ctypes.c_float * 4)()
    q = emu.scratch
    for k in range(3000):
        a = [f32(rf(rng, [lambda r: r.uniform(-3.2, 3.2), lambda r: r.uniform(-50, 50), 0.0, 1.5707964]))
             for _ in range(3)]
        cpu.call(QUAT, ecx=q, stack_args=tuple(f2u(x) for x in a))
        want = struct.unpack('<4I', cpu.read(q, 16))
        probe.pf_quat(*a, out)
        got = tuple(f2u(x) for x in out)
        if want != got:
            if not all(close(w, g, 1e-6) for w, g in zip(want, got)):
                raise SystemExit(f'euler_to_quat{a}: {[u2f(w) for w in want]} vs {list(out)}')
            qbad += 1
    return len(xs), table[1], qbad


def emu_fstp(emu):
    """The x87 st(0) a function returned, stored as a float."""
    cpu = emu.cpu
    if not hasattr(emu, '_fstp'):
        emu._fstp_slot = emu.alloc(4)
        emu._fstp = cpu.code(b'\xd9\x1d' + struct.pack('<I', emu._fstp_slot) + b'\xc3')
    cpu.call(emu._fstp)
    return emu.u32(emu._fstp_slot)


def run_sim(emu, port, rng, cmp, run, frames, stats, events=False, world_nodes=None, tol=0.0):
    """world_nodes: phase B, the real node ids puffers may sit at. tol:
    relative, per float (phase A is bit for bit)."""
    seed = rng.getrandbits(31)
    ntex = rng.randint(0, 8)
    emu.reset(seed, ntex)
    port.reset(seed, ntex, int(world_nodes is not None))
    nnodes = 4
    node_ids = world_nodes or list(range(1, nnodes + 1))
    if world_nodes is None:
        nodes = node_tables(rng, nnodes)
        emu.nodes = {i + 1: n for i, n in enumerate(nodes)}
        flat = [x for e, p in nodes for x in e], [x for e, p in nodes for x in p]
        probe.pf_nodes(nnodes, (ctypes.c_float * 12)(*flat[0]), (ctypes.c_float * 12)(*flat[1]))
    both(emu, port, 'tick_reset')
    if rng.random() < 0.9:
        both(emu, port, 'wind', 0, vec(rng, 3) if rng.random() < 0.3 else [0.0, 0.0, 0.0])
        both(emu, port, 'wind', 1, [f32(rf(rng, [13.0, 0.0, lambda r: r.uniform(0, 30)]))])
        both(emu, port, 'wind', 2, [f32(rf(rng, [2.0, lambda r: r.uniform(0, 8)]))])
        both(emu, port, 'wind', 3, [f32(rf(rng, [0.17453292, lambda r: r.uniform(0, 3)]))])
    count = rng.randint(1, 6)
    dtc = rf(rng, [1 / 60, 1 / 30, 0.125, None, None])
    for k in range(count):
        a, b = emu.create(), port.create()
        if a != b:
            raise SystemExit('create index')
        for op, args in random_settings(rng, node_ids, ntex, dtc == 0.125 and rng.random() < 0.7):
            emu.set(op, k, args)
            port.set(op, k, args)
            stats['setters'] += 1
    if events:
        run_events(emu, port, rng, cmp, stats, node_ids, ntex)
    cmp.state(f'run {run} setup', emu.dump(), port.dump(), 0)
    cmp.log(f'run {run} setup log', emu.log, port.take_log(), 0)
    emu.log = []
    centre = [0.0, 0.0, 0.0]
    for fr in range(frames):
        what = f'run {run} frame {fr}'
        dt = f32(dtc if dtc else rf(rng, [1 / 60, 1 / 30, 1 / 85, 0.125, 0.0, lambda r: r.uniform(0.002, 0.2)]))
        if world_nodes is None and rng.random() < 0.3:  # nodes move and turn
            nodes = node_tables(rng, nnodes, nodes, rng.uniform(0, 0.3))
            emu.nodes = {i + 1: n for i, n in enumerate(nodes)}
            flat = [x for e, p in nodes for x in e], [x for e, p in nodes for x in p]
            probe.pf_nodes(nnodes, (ctypes.c_float * 12)(*flat[0]), (ctypes.c_float * 12)(*flat[1]))
        if rng.random() < 0.2:  # an emitter moves (distance intervals; teleports over 200)
            k = rng.randrange(count)
            t = vec(rng, rf(rng, [5, 50, 400]))
            both(emu, port, 'set', 2, k, [f2u(x) for x in t])
        if rng.random() < 0.03:
            k = rng.randrange(count)
            both(emu, port, 'set', 0, k, [rng.choice([0, 1])])
        both(emu, port, 'tick', dt)
        stats['frames'] += 1
        want = emu.dump()
        cmp.state(what, want, port.dump(), tol)
        cmp.log(what + ' calls', emu.log, port.take_log(), 0)
        emu.log = []
        if rng.random() < 0.35:
            qs = emu.particle_list()
            if qs:
                q = qs[rng.randrange(len(qs))]
                centre = list(struct.unpack('<3f', emu.cpu.read(q, 12)))
            view, proj, cam, rect = random_camera(rng, centre)
            both(emu, port, 'camera_set', view, proj, cam, rect, 1)
            both(emu, port, 'draw_all')
            stats['draws'] += 1
            calls = emu.log
            stats['sprites'] += sum(1 for c in calls if c[0] == 6)
            stats['coloured'] += sum(1 for c in calls if c[0] == 6 and c[1] == 0)
            cmp.log(what + ' draw', calls, port.take_log(), tol * 10)
            emu.log = []
            cmp.state(what + ' after draw', emu.dump(), port.dump(), tol)
        stats['max_particles'] = max(stats['max_particles'], len(emu.particle_list()))
    both(emu, port, 'shutdown')
    cmp.state(f'run {run} shutdown', emu.dump(), port.dump(), 0)
    cmp.log(f'run {run} shutdown log', emu.log, port.take_log(), 0)
    emu.log = []


def random_event(rng, refs, made, nnodes, ntex):
    """refs: the def's ref names (ref 1 on); made: which have a puffer (a
    cached index must name one: the exe reads it unchecked)."""
    ev = bytearray(0x250)
    struct.pack_into('<BBHIf', ev, 0, 42, 0, 0, 0x250, 0.0)
    name = rng.choice(refs + [b'newpuff%d' % rng.randint(0, 3)])
    ev[0xc:0xc + len(name)] = name
    good = [i + 1 for i, m in enumerate(made) if m]
    struct.pack_into('<i', ev, 0x2c, rng.choice([0, 0, -1] + good[:1] * 2 + good))
    flags = 0
    for bit in range(23):
        if rng.random() < 0.4:
            flags |= 1 << bit
    struct.pack_into('<I', ev, 0x30, flags)
    struct.pack_into('<i', ev, 0x34, rng.choice([0, -200, -1] + list(range(1, nnodes + 1)) * 2))
    struct.pack_into('<i', ev, 0x38, rng.choice([0, 1, 1, 5]))
    for off in range(0x3c, 0x84, 4):
        struct.pack_into('<f', ev, off, rng.uniform(-20, 20))
    struct.pack_into('<i', ev, 0x84, rng.choice([0, 1]))
    struct.pack_into('<f', ev, 0x88, rng.choice([0.0, 0.1, 0.2, 1.0, 2.0, rng.uniform(0.02, 3)]))
    lo = rng.uniform(0.1, 2)
    struct.pack_into('<2f', ev, 0x8c, lo, lo + rng.uniform(0, 2))
    struct.pack_into('<2f', ev, 0x94, rng.uniform(0.1, 2), rng.uniform(0.1, 3))
    struct.pack_into('<2f', ev, 0x9c, rng.uniform(0, 0.2), rng.uniform(0.2, 0.5))
    struct.pack_into('<f', ev, 0xa4, rng.uniform(0, 2))
    a = rng.uniform(1, 50)
    struct.pack_into('<2f', ev, 0xa8, a, a + rng.choice([0.0, rng.uniform(1, 50)]))
    a = rng.uniform(10, 300)
    struct.pack_into('<2f', ev, 0xb0, a, a + rng.choice([0.0, rng.uniform(1, 300)]))
    struct.pack_into('<3f', ev, 0xb4 + 4, rng.choice([0.0, 1.0, 1.75]), rng.uniform(0, 2), 0.0)
    struct.pack_into('<i', ev, 0xc0, rng.choice([1, 2, 0, 3, 0x3f800000 & 0]))
    struct.pack_into('<i', ev, 0xc4, rng.choice([0, 0, 1]))
    for i in range(rng.choice([0, 1, 2, 3, 6])):
        nm = rng.choice([b'tex%d' % rng.randint(0, max(0, ntex - 1)), b'nosuch', b'tex1'])
        struct.pack_into('<f', ev, 0xc8 + 0x24 * i, i / 3)
        ev[0xcc + 0x24 * i:0xcc + 0x24 * i + len(nm)] = nm
    n = rng.randint(0, 6)
    struct.pack_into('<i', ev, 0x1a0, n)
    for i in range(n):
        struct.pack_into('<5f', ev, 0x1a4 + 0x14 * i, i / 5, rng.uniform(0, 255), rng.uniform(0, 255),
                         rng.uniform(0, 255), rng.uniform(0, 1))
    n = rng.randint(0, 6)
    struct.pack_into('<i', ev, 0x21c, n)
    for i in range(n):
        struct.pack_into('<2f', ev, 0x220 + 8 * i, i / max(1, n - 1), rng.uniform(0.2, 3))
    return bytes(ev)


def run_events(emu, port, rng, cmp, stats, node_choice, ntex):
    nref = rng.choice([0, 1, 2, 4, 6])
    count = len(emu.puffer_list())
    names = [b'\0' * 36] + [(b'puff%d' % i).ljust(36, b'\0') for i in range(1, nref)] if nref else []
    refs = [rng.randrange(count) if rng.random() < 0.5 else -1] + \
        [rng.choice([-1, rng.randrange(count)]) for _ in range(1, nref)] if nref else []
    if nref and refs[0] >= 0:  # ref 0, never searched: give it a name and a puffer
        names[0] = b'puff1'.ljust(36, b'\0')
    node_ids = [0] + [rng.choice(node_choice) for _ in range(rng.randint(0, 4))]
    def_node = rng.choice(node_choice)
    both(emu, port, 'def_setup', names, refs, node_ids, len(names), def_node)
    for k in range(rng.randint(4, 16)):
        d = emu.def_dump()
        made = [d[1 + 10 * i + 9] >= 0 for i in range(1, d[0])]
        names_now = [struct.pack('<9i', *d[1 + 10 * i:10 + 10 * i]).split(b'\0')[0] for i in range(1, d[0])]
        ev = random_event(rng, names_now, made, len(node_ids) - 1, ntex)
        a, b = emu.event(ev), port.event(ev)
        stats['events'] += 1
        if a != b:
            raise SystemExit(f'event {k}: returned {a[0]} / cached index {struct.unpack_from("<i", a[1], 0x2c)} vs '
                             f'{b[0]} / {struct.unpack_from("<i", b[1], 0x2c)}')
        cmp.state(f'event {k}', emu.dump(), port.dump(), 0)
        if emu.def_dump() != port.def_dump():
            raise SystemExit(f'event {k}: def lists {emu.def_dump()} vs {port.def_dump()}')
        cmp.log(f'event {k} log', emu.log, port.take_log(), 0)
        emu.log = []


def check_world(rng, cmp, stats, runs):
    """Phase B on c1: 0x4ed5d0 / 0x4ed530 on every node, then puffers at real
    nodes, with the real ones on both sides."""
    from test_scenewalk import Walk, ZBD
    probe.sw_load.argtypes = [ctypes.c_char_p]
    walk = Walk()
    path = ZBD / 'c1' / 'gamez.zbd'
    walk.load(path)
    if probe.sw_load(str(path).encode()) != 0:
        raise SystemExit('c1: the port could not read the world')
    emu = Emu(walk, 0x50000000)
    cpu = walk.cpu
    out = (ctypes.c_float * 6)()
    slot = emu.alloc(32)
    objects = []
    n = 0
    for i in list(range(20000)) + [-1]:
        try:
            p = walk.node_ptr(i) if i >= 0 else 0
        except IndexError:
            continue
        if p and (walk.u32(p + 0x34) not in (1, 2, 5, 6, 8, 9, 10) or walk.u32(p + 0x38) == 0):
            continue  # free slots and classes without a matrix
        cpu.call(EULER, ecx=p, edx=slot)
        a = cpu.uc.reg_read(UC_X86_REG_EAX)
        cpu.call(POSITION, ecx=p, edx=slot + 12)
        b = cpu.uc.reg_read(UC_X86_REG_EAX)
        want = struct.unpack('<6I', cpu.read(slot, 24))
        r = probe.pf_node_world(i, out)
        got = tuple(f2u(x) for x in out)
        # Last bits can differ: node_to_world's chain (zcls, test_scenewalk.py).
        if (a | b << 4) != r or not (a & b & 1 or all(close(w, g, 1e-6) for w, g in zip(want, got))):
            raise SystemExit(f'node {i}: {a | b << 4} {[u2f(w) for w in want]} vs {r} {list(out)}')
        n += 1
        stats['world_exact'] += want == got or bool(a & b & 1)
        if p and walk.u32(p + 0x34) == 5:
            objects.append(REAL + i)
    stats['world_nodes'] = n
    port = Port()
    tol = 1e-5  # the node positions feed every particle's start
    for run in range(runs):
        ids = rng.sample(objects, 6)
        run_sim(emu, port, rng, cmp, f'c1 {run}', rng.choice([40, 100]), stats, events=run % 2 == 1,
                world_nodes=ids, tol=tol)


def main():
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 60
    rng = random.Random(20261020)
    emu, port = Emu(), Port()
    cmp = Compare()
    n, t1, qapprox = check_math(emu, rng)
    print(f'exp_neg: {n} inputs identical (table[1] = {t1!r}); euler_to_quat: 3000 angles, '
          f'{3000 - qapprox} identical, {qapprox} within 1e-6')
    stats = dict(setters=0, frames=0, draws=0, sprites=0, coloured=0, events=0, max_particles=0, world_exact=0)
    for run in range(runs):
        run_sim(emu, port, rng, cmp, run, rng.choice([40, 80, 150, 300]), stats, events=run % 2 == 1)
    print(f'simulation: {runs} runs, {stats["setters"]} setter calls, {stats["events"]} animation events, '
          f'{stats["frames"]} frames (up to {stats["max_particles"]} particles), {stats["draws"]} draws with '
          f'{stats["sprites"]} sprites ({stats["coloured"]} coloured)')
    print(f'  {cmp.exact} words identical, {cmp.approx} within tolerance (worst {cmp.worst:.2e})')
    world = Compare()
    check_world(rng, world, stats, max(2, runs // 10))
    print(f'world c1: 0x4ed5d0 / 0x4ed530 on {stats["world_nodes"]} nodes and null, {stats["world_exact"]} '
          f'identical, the rest within 1e-6; {max(2, runs // 10)} runs with puffers at its object3d nodes: '
          f'{world.exact} words identical, {world.approx} within 1e-5 (worst {world.worst:.2e})')
    print('ALL OK')


if __name__ == '__main__':
    main()
