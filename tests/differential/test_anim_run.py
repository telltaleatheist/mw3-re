"""The animation runtime (zeff_anim_run, zeff_anim_events, the post-load pass
0x50ae60 in zeff_anim_init; docs/anim-postload.md, docs/anim-run.md) against
the original x86 in Unicorn, the FPU at 24 bits as in play.

    cmake --build build && .venv/bin/python tests/differential/test_anim_run.py [frames, 60] [missions...]
      MW3_DUMP=1   print the first difference's neighbourhood

For each mission both sides read the world (gamez.zbd) and run the post-load
pass, which reads anim.zbd itself, registers the damage callbacks, runs the
reset sequences and starts the startup animations; the whole state is
compared. Then frames of the action lists (0x4f7d80) run the animations,
with calls in between chosen at random: starts (plain, at a pose, at a
node), resets, stops, invalidations, hits and rams, the player position,
the zone, the level, roots marked drawn. Each frame compares every def
(with its instances, sequences and lists), every puffer, the nodes the defs
touch, the node lists, the damage records, the anim log and the calls into
code not ported; every 25th frame and the last compare the event blobs and
the whole node pool too.

Not ported, recorded on both sides with the same answers: zSound's play
calls (0x56e3d0, 0x56e7e0), the ground probe (0x4edab0: a deterministic
grid of hits), the "if" event's ray (0x507220), the node update and the
frame update run for real on both sides (0x4f7e00, 0x4f7f70,
docs/action-lists.md; MW3_STUB_UPDATE=1 records them instead), the puffer tick (0x5490e0, test_puffers covers it), the unshipped events 3,
16, 20, 21 and the 2D ones 36, 37, 38 (whole handlers), the weapon lookup
and detonation (0x581ed0 / 0x5822c0), zerr.
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
from test_gamez import WORLDS, ZBD, Emulated as WorldEmu, probe  # noqa: E402

DUMP = os.environ.get('MW3_DUMP') == '1'
# The node and frame updates (0x4f7e00, 0x4f7f70) run for real on both sides;
# MW3_STUB_UPDATE=1 records them instead (as before they were ported).
STUB_UPDATE = os.environ.get('MW3_STUB_UPDATE') == '1'

probe.ar_open.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
probe.ar_random.argtypes = [ctypes.c_void_p]
probe.ar_stub_update.argtypes = [ctypes.c_int]
probe.ar_frame.argtypes = [ctypes.c_float]
probe.ar_start.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_float, ctypes.c_float, ctypes.c_float]
probe.ar_start_at.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_void_p]
probe.ar_restart.argtypes = [ctypes.c_int]
probe.ar_stop.argtypes = [ctypes.c_int]
probe.ar_invalidate.argtypes = [ctypes.c_int]
probe.ar_collide.argtypes = [ctypes.c_int, ctypes.c_float]
probe.ar_hit.argtypes = [ctypes.c_int, ctypes.c_uint32, ctypes.c_float]
probe.ar_player.argtypes = [ctypes.c_float] * 3
probe.ar_zone.argtypes = [ctypes.c_char_p]
probe.ar_level.argtypes = [ctypes.c_int]
probe.ar_drawn.argtypes = [ctypes.c_int]
probe.ar_dump.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_int]

ZERR, IAT_SPRINTF, IAT_STRNCPY, IAT_RAND = 0x49bfd0, 0x5bb658, 0x5bb648, 0x5bb668
NEW, DELETE, ATEXIT = 0x5abd26, 0x5abd1a, 0x5ac140
BLOCK, DEFS, COUNT, WORLD = 0x73f720, 0x73f72c, 0x73f72a, 0x73f738
RANDOM, RANDOM_INDEX, STAGGER, STOPPED, LEVEL = 0x73f400, 0x60c5f4, 0x60c1d8, 0x60c1d4, 0x60c1d0
DT = 0x72c52c
PUFFERS, TEX_TABLE, DEFAULT_ENTRY = 0x72be04, 0x610484, 0x5ecbf8
LOG, LOG_COUNT = 0x60c708, 0x60c710
KILLED, FRACTION = 0x9a1788, 0x9a178c
LIST_HEADS = 0x5ea6d0
ACTIONS = {0x507de0: 1, 0x508260: 2, 0x5085b0: 3}
DAMAGE_CBS = {0x502df0: 1, 0x502e60: 2}
FAKE_SURFACE = 0x6e000000
FAKE_WEAPON = 0x6f000000
EVENTS = {}  # event type -> times the original's runner dispatched it


def f2u(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def u2f(u):
    return struct.unpack('<f', struct.pack('<I', u & 0xffffffff))[0]


def s32(x):
    x &= 0xffffffff
    return x - (1 << 32) if x >= 1 << 31 else x


def fnv(b):
    h = 2166136261
    for c in b:
        h = ((h ^ c) * 16777619) & 0xffffffff
    return h


class MsvcRand:
    def __init__(self, seed):
        self.state = seed

    def __call__(self, cpu=None):
        self.state = (self.state * 214013 + 2531011) & 0xffffffff
        return (self.state >> 16) & 0x7fff


class Emu(WorldEmu):
    """The original: test_gamez's world reader, then 0x50ae60 and the frames."""

    def __init__(self):
        super().__init__()
        cpu = self.cpu
        self.trace = []
        self.finds = self.rays = 0
        self.zerr_stubbed = True
        cpu.stub_function(ZERR, lambda c: (self.trace.extend([5, c.arg(0), c.arg(2)]), 0)[1])
        cpu.stub_import(IAT_SPRINTF, self.sprintf)
        cpu.stub_import(IAT_STRNCPY, self.strncpy)
        cpu.stub_import(IAT_RAND, MsvcRand(1))
        cpu.stub_function(NEW, lambda c: self.alloc_fixed(c.arg(0)))  # apart from the world's heap
        cpu.stub_function(DELETE, lambda c: 0)
        cpu.stub_function(ATEXIT, lambda c: 0)
        cpu.stub_function(0x56f6f0, self.find_sound)
        cpu.stub_function(0x50d9e0, lambda c: 0xffffffff)
        cpu.stub_function(0x4f9b50, lambda c: 0, 8)  # the tree copy (ret 8): fails
        cpu.stub_function(0x56e3d0, lambda c: (self.trace.extend([1, c.ecx, c.arg(0)]), 0)[1], 4)
        cpu.stub_function(0x56e7e0, self.play_at, 8)
        cpu.stub_function(0x4edab0, self.ground, 12)
        cpu.stub_function(0x507220, self.ray, 8)
        if STUB_UPDATE:
            cpu.stub_function(0x4f7e00, lambda c: (self.trace.extend([6, self.index(c.ecx)]), 0)[1])
            cpu.stub_function(0x4f7f70, lambda c: (self.trace.append(7), 0)[1])
        cpu.stub_function(0x5490e0, lambda c: (self.trace.append(8), 0)[1])
        for addr, kind, pop in ((0x5031b0, 3, 0), (0x505df0, 16, 0), (0x503c10, 20, 4), (0x503d50, 21, 4),
                                (0x5074b0, 36, 4), (0x5076c0, 37, 4)):
            cpu.stub_function(addr, lambda c, k=kind: (self.trace.extend([9, k, self.def_id(c.ecx)]), 2)[1], pop)
        cpu.stub_function(0x507970, lambda c: (self.trace.extend([9, 38, 0]), 2)[1])
        cpu.stub_function(0x581ed0, lambda c: (self.trace.extend([10, fnv(self.cstr(c.ecx))]), FAKE_WEAPON)[1])
        cpu.stub_function(0x5822c0, lambda c: (self.trace.extend([11] + list(struct.unpack(
            '<3I', c.read(c.edx, 12)))), 0)[1])
        # The port's FIX (bug) in 0x51fee0 (fixes.md): mode 2 leaves its temp's
        # row 3 unwritten and copies stack leftovers into the translation; the
        # motion event reaches it (0x5047f5, 0x504979). Zeroed here as in the port.
        from unicorn import UC_HOOK_CODE
        from unicorn.x86_const import UC_X86_REG_EBP, UC_X86_REG_EDX

        def mode2(uc, addr, size, _):
            if uc.reg_read(UC_X86_REG_EDX) == 2:
                uc.mem_write(uc.reg_read(UC_X86_REG_EBP) - 0x18, bytes(12))
        cpu.uc.hook_add(UC_HOOK_CODE, mode2, begin=0x51ff18, end=0x51ff18)
        # MSVCRT's _CIasin, _copysign and floor as x87 code (test_scenewalk.py).
        from test_scenewalk import ASIN, COPYSIGN, FLOOR, IAT_ASIN, IAT_COPYSIGN, IAT_FLOOR
        cpu.import_code(IAT_FLOOR, FLOOR)
        cpu.import_code(IAT_ASIN, ASIN)
        cpu.import_code(IAT_COPYSIGN, COPYSIGN)
        from unicorn.x86_const import UC_X86_REG_EDX as EDX_

        def count_event(uc, addr, size, _):
            t = self.cpu.read(uc.reg_read(EDX_), 1)[0]
            EVENTS[t] = EVENTS.get(t, 0) + 1
        cpu.uc.hook_add(UC_HOOK_CODE, count_event, begin=0x507a6e, end=0x507a6e)
        cpu.call(0x548d90)  # the puffer lists' static constructors
        cpu.call(0x548e60)
        self.scratch = self.alloc_fixed(0x400)
        self.surfaces = {}
        self.def_ids = {}

    # --- memory helpers ---
    def alloc_fixed(self, n):
        a = self.cpu.alloc(n)
        self.cpu.write(a, bytes(n))
        return a

    def u(self, a):
        return struct.unpack('<I', self.cpu.read(a, 4))[0]

    def cstr(self, p):
        out = bytearray()
        while (b := self.cpu.read(p + len(out), 1)) != b'\0':
            out += b
        return bytes(out)

    # --- stubs ---
    def sprintf(self, c):
        fmt = self.cstr(c.arg(1))
        if fmt == b'_%s':
            s = b'_' + self.cstr(c.arg(2))
        elif fmt == b'%s':
            s = self.cstr(c.arg(2))
        else:
            s = b''
        c.write(c.arg(0), s + b'\0')
        return len(s)

    def strncpy(self, c):
        dst, src, n = c.arg(0), self.cstr(c.arg(1)), c.arg(2)
        c.write(dst, (src + bytes(n))[:n])
        return dst

    def find_sound(self, c):
        k = self.finds
        self.finds += 1
        return 0x70000000 + 0x10 * k

    def play_at(self, c):
        self.trace.extend([2, c.ecx] + list(struct.unpack('<3I', c.read(c.edx, 12))) + [c.arg(0)])
        return 0

    def surface(self, kind):
        if kind not in self.surfaces:
            p = self.alloc_fixed(0x40)
            self.cpu.write(p + 0x20, struct.pack('<i', kind))
            self.surfaces[kind] = p
        return self.surfaces[kind]

    def ground(self, c):
        x, y, z = (u2f(c.arg(i)) for i in range(3))
        self.trace.extend([3, c.arg(0), c.arg(1), c.arg(2)])
        ix, iz = math.floor(x / 8.0), math.floor(z / 8.0)
        h5 = (ix * 7 + iz * 13) % 5
        count = 0 if h5 == 4 else h5 % 2 + 1
        buf = c.edx
        c.write(buf, bytes([count]))
        e0 = buf + 4
        c.write(e0 + 0x10, struct.pack('<f', float(-h5) * 2.0))
        c.write(e0 + 0x20, struct.pack('<I', self.surface(1 if h5 % 3 == 1 else 4) if h5 % 3 else 0))
        e1 = buf + 4 + 0x28
        c.write(e1 + 0x10, struct.pack('<f', float(-h5) * 2.0 + 6.0))
        c.write(e1 + 0x20, struct.pack('<I', self.surface(2)))
        return 0

    def ray(self, c):
        node, pos = c.ecx, c.edx
        p = list(struct.unpack('<3I', c.read(pos, 12))) if pos else [0, 0, 0]
        height = self.u(c.arg(0)) if c.arg(0) else f2u(50.0)
        self.trace.extend([4, self.index(node) & 0xffffffff, 1 if pos else 0] + p + [height])
        hit = 1 if self.rays % 3 == 0 else 0
        self.rays += 1
        c.write(c.arg(1), struct.pack('<i', hit))
        return 0

    # --- pool and ids ---
    def blocks(self):
        return [self.u(self.u(0x60bf38) + 4 * b) for b in range(self.i32(0x60bf30))]

    def index(self, p):
        if p == 0:
            return -1
        bs = self.i32(0x5ea830)
        for b, base in enumerate(self.blocks()):
            if base <= p < base + bs * 0xd4:
                return b * bs + (p - base) // 0xd4
        return -2

    def node_ptr(self, i):
        bs = self.i32(0x5ea830)
        return self.blocks()[i // bs] + (i % bs) * 0xd4

    def build_ids(self):
        self.def_ids = {}
        count = struct.unpack('<h', self.cpu.read(COUNT, 2))[0]
        base = self.u(DEFS)
        for i in range(count):
            d, depth = base + 0x13c * i, 0
            while d:
                self.def_ids[d] = i | (depth << 16)
                d = self.u(d + 0x138)
                depth += 1

    def def_id(self, p):
        if p == 0:
            return 0xffffffff
        return self.def_ids.get(p, 0xfffffffe)

    def puffers(self):
        head = self.u(PUFFERS)
        out, n = [], self.u(head)
        while n != head:
            out.append(self.u(n + 8))
            n = self.u(n)
        return out

    # --- driving ---
    def open(self, world, anim):
        cpu = self.cpu
        for a, v in ((0x72f338, 0), (0x73f3a8, 0), (0x5e9e2c, 1), (0x60b94c, 0), (0x60be40, 0), (0x60be44, 0)):
            cpu.u32(a, v)
        self.open_ok = True
        head = self.read_zbd(world.read_bytes(), str(world).encode())
        assert struct.unpack_from('<i', head)[0] == 0
        self.trace = []
        cpu.u32(0x74a288, self.calloc(12 * 0x4000))  # the morph scratch (0x526510), for the model boxes
        cpu.write(BLOCK, bytes(0x44))
        cpu.write(0x60c570, bytes(0x80))
        cpu.call(0x509150, ecx=1)
        p = self.alloc(len(str(anim)) + 1)
        cpu.write(p, str(anim).encode() + b'\0')
        cpu.call(0x5090f0, ecx=p)
        worlds = self.u(LIST_HEADS + 4 * 13)
        cpu.u32(WORLD, self.u(self.u(worlds)))
        self.anim_data = anim.read_bytes()
        cpu.u32(DT, 0)

    def set_random(self, table):
        self.cpu.write(RANDOM, struct.pack('<200f', *table))

    def load(self):
        self.data, self.pos, self.open_ok = self.anim_data, 0, True
        p = self.alloc(16)
        self.cpu.write(p, b'anim.zrd\0')
        self.cpu.call(0x50ae60, ecx=p, count=4_000_000_000)
        self.build_ids()

    def frame(self, dt):
        self.cpu.u32(DT, f2u(dt))
        self.guard(lambda: self.cpu.call(0x4f7d80, count=2_000_000_000))
        self.build_ids()

    def guard(self, fn):
        try:
            fn()
        except Exception as e:
            from unicorn.x86_const import UC_X86_REG_EIP, UC_X86_REG_ESP
            uc = self.cpu.uc
            regs = {n: uc.reg_read(getattr(__import__('unicorn.x86_const', fromlist=['x']), 'UC_X86_REG_' + n))
                    for n in ('EIP', 'EAX', 'EBX', 'ECX', 'EDX', 'ESI', 'EDI', 'EBP', 'ESP')}
            esp = regs['ESP']
            stack = struct.unpack('<16I', self.cpu.read(esp, 64))
            raise SystemExit(f'original stopped: {e} ' + ' '.join(f'{k}={v:#x}' for k, v in regs.items()) +
                             '\n stack ' + ' '.join(f'{v:#x}' for v in stack))

    def defp(self, i):
        return self.u(DEFS) + 0x13c * i

    def start(self, d, node, x, y, z):
        self.cpu.call(0x508ae0, ecx=self.defp(d), edx=self.node_ptr(node) if node >= 0 else 0,
                      stack_args=(f2u(x), f2u(y), f2u(z)))
        self.build_ids()

    def start_at(self, d, node, nine):
        self.cpu.call(0x5085e0, ecx=self.defp(d), edx=self.node_ptr(node) if node >= 0 else 0,
                      stack_args=tuple(f2u(v) for v in nine))
        self.build_ids()

    def restart(self, d):
        self.cpu.call(0x508460, ecx=self.defp(d))
        self.build_ids()

    def stop(self, d):
        self.cpu.call(0x506ca0, ecx=self.defp(d))
        self.build_ids()

    def invalidate(self, d):
        self.cpu.call(0x5084c0, ecx=self.defp(d))

    def collide(self, d, dmg):
        self.cpu.call(0x502df0, ecx=self.defp(d), stack_args=(f2u(dmg),))
        self.build_ids()

    def hit(self, d, flags, dmg):
        h = self.scratch
        self.cpu.write(h, bytes(0x80))
        self.cpu.u32(h + 0x6c, flags)
        self.cpu.call(0x502e60, ecx=self.defp(d), edx=h, stack_args=(0, f2u(dmg)))
        self.build_ids()

    def player(self, x, y, z):
        self.cpu.write(self.scratch + 0x100, struct.pack('<3f', x, y, z))
        self.cpu.call(0x502d40, ecx=self.scratch + 0x100)

    def zone(self, z):
        self.cpu.write(self.scratch + 0x110, bytes(z))
        self.cpu.call(0x502d70, ecx=self.scratch + 0x110)

    def level(self, n):
        self.cpu.call(0x508fb0, ecx=n)

    def drawn(self, node):
        p = self.node_ptr(node)
        self.cpu.u32(p + 0x24, self.u(p + 0x24) | 0x80000000)

    # --- the dump (as ar_dump) ---
    def dump(self, full):
        r, u = self.cpu.read, self.u
        out = []
        self.records = {}
        self.marks = marks = []
        W = out.append

        def raw_words(addr, n, repl=None):
            b = bytearray(r(addr, n))
            for off, v in (repl or {}).items():
                b[off:off + 4] = struct.pack('<I', v & 0xffffffff)
            out.extend(struct.unpack(f'<{n // 4}I', bytes(b)))

        def blob(addr, n):
            b = r(addr, n) + bytes((-n) % 4)
            out.extend(struct.unpack(f'<{len(b) // 4}I', b))

        ix = lambda p: self.index(p) & 0xffffffff

        b = bytearray(r(BLOCK, 0x44))
        for o in (0x0c, 0x14, 0x18):
            b[o:o + 4] = bytes(4)
        out.extend(struct.unpack('<17I', bytes(b)))
        W(r(STAGGER, 1)[0])
        W(u(RANDOM_INDEX)), W(u(STOPPED)), W(u(LEVEL)), W(u(KILLED)), W(u(FRACTION))
        count = struct.unpack('<h', r(COUNT, 2))[0]
        W(count & 0xffffffff)
        base = u(DEFS)
        pufs = self.puffers()
        pid = lambda p: 0xffffffff if p == 0 else (pufs.index(p) if p in pufs else 0xfffffffe)
        touched = []

        def seq_off(cur, ev, size):
            if ev and ev <= cur <= ev + max(size, 0):
                return cur - ev
            return 0xffffffff

        for i in range(count):
            d, depth = base + 0x13c * i, 0
            while d:
                nm = r(d, 0x20).split(bytes(1))[0].decode('latin-1')
                marks.append((len(out), f'def {i} depth {depth} {nm}'))
                W(depth)
                rec = r(d, 0x13c)
                g = lambda o: struct.unpack_from('<I', rec, o)[0]
                gi = lambda o: struct.unpack_from('<i', rec, o)[0]
                repl = {0x40: ix(g(0x40)), 0x64: ix(g(0x64)), 0x68: ix(g(0x68)), 0x6c: 1 if g(0x6c) else 0,
                        0x70: 0, 0x74: ix(g(0x74)), 0x84: ix(g(0x84)), 0xc4: 0,
                        0xfc: seq_off(g(0xfc), g(0x100), gi(0x104)), 0x100: 0,
                        0x138: self.def_id(g(0x138))}
                for o in range(0x114, 0x138, 4):
                    repl[o] = 0
                raw_words(d, 0x13c, repl)
                seqs = g(0xc4)
                marks.append((len(out), f'def {i} seqs'))
                for k in range(rec[0x108]):
                    s = seqs + 0x40 * k
                    sr = r(s, 0x40)
                    sev, ssize = struct.unpack_from('<Ii', sr, 0x38)
                    raw_words(s, 0x40, {0x34: seq_off(struct.unpack_from('<I', sr, 0x34)[0], sev, ssize), 0x38: 0})
                    if full and ssize > 0:
                        blob(sev, ssize)
                if full and gi(0x104) > 0:
                    blob(g(0x100), gi(0x104))
                marks.append((len(out), f'def {i} lists'))
                touched += [g(0x40), g(0x64), g(0x68)]
                for cnt, ptr, size, kind in ((0x109, 0x114, 0x60, 'n'), (0x10a, 0x118, 0x28, 'n'),
                                             (0x10b, 0x11c, 0x2c, 'n'), (0x10c, 0x120, 0x2c, 'p'),
                                             (0x10d, 0x124, 0x2c, 'n'), (0x10e, 0x128, 0x24, 's'),
                                             (0x110, 0x130, 0x30, 'q'), (0x112, 0x134, 0x48, 'a')):
                    lst = g(ptr)
                    for k in range(rec[cnt]):
                        e = lst + size * k
                        if kind == 'n':
                            v = u(e + 0x24)
                            raw_words(e, size, {0x24: ix(v)})
                            touched.append(v)
                        elif kind == 'p':
                            raw_words(e, size, {0x24: pid(u(e + 0x24))})
                        elif kind == 's':
                            raw_words(e, size)
                        elif kind == 'q':
                            raw_words(e, size, {0x28: self.def_id(u(e + 0x28)), 0x2c: ix(u(e + 0x2c))})
                        else:
                            raw_words(e, size, {0x44: self.def_id(u(e + 0x44))})
                d = g(0x138)
                depth += 1
            W(0xffffffff)
        W(len(pufs))
        for k, p in enumerate(pufs):
            marks.append((len(out), f'puffer {k}'))
            self.dump_puffer(p, out)
        cap, live, free = self.i32(0x60bf34), self.i32(0x60bf3c), self.i32(0x5ea834)
        out += [cap & 0xffffffff, live & 0xffffffff, free & 0xffffffff]
        if full:
            for i in range(cap):
                marks.append((len(out), f'node {i}'))
                self.dump_node(self.node_ptr(i), True, out)
        else:
            world = u(u(u(LIST_HEADS + 4 * 13)))
            ids = sorted({self.index(p) for p in touched + [world] if p})
            W(len(ids))
            for i in ids:
                marks.append((len(out), f'node {i}'))
                W(i & 0xffffffff)
                self.dump_node(self.node_ptr(i), False, out)
        for li in range(16):
            marks.append((len(out), f'list {li}'))
            if not full and li == 6:
                continue
            head = u(LIST_HEADS + 4 * li)
            items, p = [], u(head)
            while p:
                items.append(p)
                p = u(p + 8)
            W(len(items))
            for p in items:
                W(ix(u(p))), W(u(p + 0xc))
            W(u(head + 8))
        recs = sorted(self.records.items(), key=lambda kv: kv[1])
        marks.append((len(out), 'damage records'))
        W(len(recs))
        for rp, _ in recs:
            for slot in range(2):
                owner, cb = u(rp + 8 * slot), u(rp + 8 * slot + 4)
                W(self.def_id(owner)), W(DAMAGE_CBS.get(cb, 3) if cb else 0)
        n = self.i32(LOG_COUNT)
        marks.append((len(out), 'log'))
        W(n & 0xffffffff)
        recs = u(LOG)
        for i in range(n):
            e = recs + 0x50 * i
            t = self.i32(e)
            raw_words(e, 0x2c)
            words = {1: 9, 2: 3, 3: 7, 4: 8}.get(t, 0)
            if words:
                raw_words(e + 0x2c, 4 * words)
        marks.append((len(out), 'trace'))
        W(len(self.trace))
        out += self.trace
        self.trace = []
        return out

    def dump_puffer(self, p, out):
        r, u = self.cpu.read, self.u
        b = r(p, 0xe4)
        g = lambda o: struct.unpack_from('<I', b, o)[0]

        def tex(e):
            if e == 0:
                return 0xfffffffd
            if e == DEFAULT_ENTRY:
                return 0xffffffff
            if TEX_TABLE <= e < TEX_TABLE + 0x28 * 0x1000 and (e - TEX_TABLE) % 0x28 == 0:
                return (e - TEX_TABLE) // 0x28
            return 0xfffffffe
        out += [b[0], g(4), b[8]]
        beg, end = g(0x10), g(0x14)
        out.append((end - beg) // 8)
        for k in range(beg, end, 8):
            t, e = struct.unpack('<II', r(k, 8))
            out += [t, tex(e)]
        beg, end = g(0x20), g(0x24)
        out.append((end - beg) // 0x14)
        for k in range(beg, end, 0x14):
            out += list(struct.unpack('<5I', r(k, 0x14)))
        beg, end = g(0x30), g(0x34)
        out.append((end - beg) // 8)
        for k in range(beg, end, 8):
            out += list(struct.unpack('<2I', r(k, 8)))
        out.append(b[0x3c])
        out += [g(o) for o in range(0x40, 0x70, 4)]
        out.append(self.index(g(0x70)) & 0xffffffff)
        out += [g(0x74), g(0x78), g(0x7c), b[0x80]]
        out += [g(o) for o in range(0x84, 0xe4, 4)]

    def dump_node(self, p, full, out):
        r, u = self.cpu.read, self.u
        h = r(p, 0xd4)
        g = lambda o: struct.unpack_from('<I', h, o)[0]
        gi = lambda o: struct.unpack_from('<i', h, o)[0]
        owner = g(0x40)
        if owner in self.def_ids:
            owner = 0x80000000 | self.def_ids[owner]
        act = g(0x48)
        rec = g(0xbc)
        if rec:
            self.records.setdefault(rec, len(self.records))
        b = bytearray(h[:0xd0])
        for o, v in ((0x38, 0), (0x3c, 0), (0x40, owner), (0x48, ACTIONS.get(act, 4) if act else 0), (0x58, 0),
                     (0x60, 0), (0xbc, self.records[rec] if rec else 0xffffffff), (0xcc, 1 if g(0xcc) else 0)):
            b[o:o + 4] = struct.pack('<I', v & 0xffffffff)
        out.extend(struct.unpack('<52I', bytes(b)))
        out.append(g(0xd0))

        def nodes(lst, n):
            out.append(1 if lst else 0)
            if lst:
                out.extend(self.index(v) & 0xffffffff for v in struct.unpack(f'<{n}I', r(lst, 4 * n)))
        nodes(g(0x58), gi(0x54))
        nodes(g(0x60), gi(0x5c))
        m = g(0x3c)
        if m:
            out += [u(m + 8), u(m + 0x24)]
            polys, npolys = u(m + 0x34), self.i32(m + 0x10)
            mat = u(polys + 0x1c) if npolys and polys else 0
            cyc = u(mat + 0x24) if mat else 0
            if mat and cyc:
                e = u(mat + 0x10)
                out += [(e - TEX_TABLE) // 0x28 if TEX_TABLE <= e < TEX_TABLE + 0x28000 else
                        (0xffffffff if e == DEFAULT_ENTRY else 0xfffffffe if e else 0xfffffffd), u(cyc + 8)]
            else:
                out += [0xfffffff7, 0xfffffff7]
        else:
            out += [0xfffffff8] * 4
        d, cls = g(0x38), gi(0x34)
        if not d:
            return

        def raw(addr, n, repl):
            bb = bytearray(r(addr, n))
            for o, v in repl.items():
                bb[o:o + 4] = struct.pack('<I', v & 0xffffffff)
            out.extend(struct.unpack(f'<{n // 4}I', bytes(bb)))
        if cls == 1:
            raw(d, 0x1e8, {0: 0, 4: 0, 8: 0, 0xc: 0})
        elif cls == 5:
            raw(d, 0x90, {})
        elif cls == 9:
            raw(d, 0xd0, {0xcc: 0})
            nodes(u(d + 0xcc), self.i32(d + 0xc8))
        elif cls == 10:
            raw(d, 0x94, {4: 0, 0x90: 0})
            nodes(u(d + 0x90), self.i32(d + 0x8c))
        elif cls == 2:
            raw(d, 0xbc, {0xc: 0, 0x90: 0, 0xa4: 0, 0xa8: 0, 0xb0: 0, 0xb4: 0})
            nodes(u(d + 0xa4), self.i32(d + 0xa0))
            nodes(u(d + 0xb0), self.i32(d + 0xac))
            rows_p, nz, nx = u(d + 0x90), self.i32(d + 0x8c), self.i32(d + 0x88)
            rows = [u(rows_p + 4 * z) for z in range(nz)]
            pend = u(d + 0xc)
            for k in range(self.i32(d + 4)):
                e = u(pend + 4 * k)
                at = 0xffffffff
                for z, row in enumerate(rows):
                    if row <= e < row + 0x48 * nx and (e - row) % 0x48 == 0:
                        at = z * 0x10000 + (e - row) // 0x48
                out.append(at)
            if full:
                for z, row in enumerate(rows):
                    for x in range(nx):
                        pa = row + 0x48 * x
                        out.append(u(pa))
                        nodes(u(pa + 0x3c), struct.unpack('<h', r(pa + 0x3a, 2))[0])


class Port:
    def __init__(self):
        self.buf = (ctypes.c_uint32 * (1 << 23))()

    def open(self, world, anim):
        assert probe.ar_open(str(world).encode(), str(anim).encode()) == 0
        probe.ar_stub_update(int(STUB_UPDATE))

    def set_random(self, table):
        probe.ar_random(struct.pack('<200f', *table))

    def load(self):
        probe.ar_load()

    def frame(self, dt):
        probe.ar_frame(dt)

    def start(self, d, node, x, y, z):
        probe.ar_start(d, node, x, y, z)

    def start_at(self, d, node, nine):
        probe.ar_start_at(d, node, struct.pack('<9f', *nine))

    def restart(self, d):
        probe.ar_restart(d)

    def stop(self, d):
        probe.ar_stop(d)

    def invalidate(self, d):
        probe.ar_invalidate(d)

    def collide(self, d, dmg):
        probe.ar_collide(d, dmg)

    def hit(self, d, flags, dmg):
        probe.ar_hit(d, flags, dmg)

    def player(self, x, y, z):
        probe.ar_player(x, y, z)

    def zone(self, z):
        probe.ar_zone(bytes(z))

    def level(self, n):
        probe.ar_level(n)

    def drawn(self, node):
        probe.ar_drawn(node)

    def dump(self, full):
        n = probe.ar_dump(full, self.buf, len(self.buf))
        assert n <= len(self.buf)
        return list(self.buf[:n])


class Compare:
    marks = []

    def __init__(self, tol=1e-4):
        self.tol, self.words, self.approx, self.worst = tol, 0, 0, 0.0

    def __call__(self, what, want, got):
        if len(want) != len(got):
            at = next((i for i in range(min(len(want), len(got))) if want[i] != got[i]), min(len(want), len(got)))
            self.fail(what, want, got, at, f'lengths {len(want)} vs {len(got)}')
        for i, (a, b) in enumerate(zip(want, got)):
            if a == b:
                continue
            x, y = u2f(a), u2f(b)
            if math.isfinite(x) and math.isfinite(y) and abs(x - y) <= self.tol * max(1.0, abs(x), abs(y)):
                self.approx += 1
                self.worst = max(self.worst, abs(x - y) / max(1.0, abs(x), abs(y)))
                continue
            self.fail(what, want, got, i, '')
        self.words += len(want)

    def fail(self, what, want, got, at, note):
        lo = max(0, at - 8)
        where = [m for m in self.marks if m[0] <= at]
        where = f'({where[-1][1]} +{at - where[-1][0]})' if where else ''
        lines = [f'{what}: first difference at word {at} {where} {note}']
        if True:
            for k in range(lo, min(at + 8, max(len(want), len(got)))):
                a = want[k] if k < len(want) else None
                b = got[k] if k < len(got) else None
                mark = '<<' if a != b else ''
                fa = f'{u2f(a):.6g}' if a is not None else '-'
                fb = f'{u2f(b):.6g}' if b is not None else '-'
                lines.append(f'  {k:8d} {a!s:>12} {b!s:>12}  ({fa} / {fb}) {mark}')
        raise SystemExit('\n'.join(lines))


def random_table(rng):
    return [struct.unpack('<f', struct.pack('<f', rng.randrange(32768) * 3.051851e-05))[0] for _ in range(200)]


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


def run_mission(mission, frames, seed, cmp, stats):
    world, anim = ZBD / mission / 'gamez.zbd', ZBD / mission / 'anim.zbd'
    rng = random.Random(seed)
    emu, port = Emu(), Port()
    emu.open(world, anim)
    port.open(world, anim)
    table = random_table(rng)
    emu.set_random(table)
    port.set_random(table)
    emu.load()
    port.load()
    want = emu.dump(True)
    cmp.marks = emu.marks
    cmp(f'{mission} load', want, port.dump(True))
    count = struct.unpack('<h', emu.cpu.read(COUNT, 2))[0]
    base = emu.u(DEFS)
    acts = {}
    for i in range(1, count):
        acts.setdefault(emu.cpu.read(base + 0x13c * i + 0x99, 1)[0], []).append(i)
    gated = [i for i in range(1, count) if emu.cpu.read(base + 0x13c * i + 0x110, 1)[0]]  # with prerequisites
    wd = emu.u(emu.u(WORLD) + 0x38)
    x0, z0 = struct.unpack('<2f', emu.cpu.read(wd + 0x34, 8))
    x1, z1 = struct.unpack('<f', emu.cpu.read(wd + 0x44, 4))[0], struct.unpack('<f', emu.cpu.read(wd + 0x48, 4))[0]
    roots = [emu.index(emu.u(base + 0x13c * i + 0x64)) for i in range(1, count)]
    both = (emu, port)
    for f in range(frames):
        for _ in range(rng.choice((0, 0, 1, 1, 2, 4))):
            k = rng.random()
            d = rng.choice(gated) if gated and rng.random() < 0.15 else rng.randrange(1, count)
            if k < 0.35:
                v = [f32(rng.uniform(-3, 3)) for _ in range(3)] if rng.random() < 0.3 else [0.0, 0.0, 0.0]
                for s in both:
                    s.start(d, -1, *v)
                stats['start'] += 1
            elif k < 0.45:
                nine = [f32(rng.uniform(x0, x1)), f32(rng.uniform(-5, 30)), f32(rng.uniform(z1, z0))] + \
                       [f32(rng.uniform(-3.2, 3.2)) for _ in range(3)] + [f32(rng.uniform(-2, 2)) for _ in range(3)]
                for s in both:
                    s.start_at(d, -1, nine)
                stats['start_at'] += 1
            elif k < 0.5:
                node = rng.choice(roots)
                for s in both:
                    s.start(d, node, 0.0, 0.0, 0.0)
                stats['start_node'] += 1
            elif k < 0.6:
                for s in both:
                    s.restart(d)
                stats['reset'] += 1
            elif k < 0.68:
                for s in both:
                    s.stop(d)
                stats['stop'] += 1
            elif k < 0.7:
                for s in both:
                    s.invalidate(d)
                stats['invalidate'] += 1
            elif k < 0.8:
                pool = acts.get(1, []) + acts.get(2, [])
                if pool:
                    dd, dmg = rng.choice(pool), f32(rng.choice((0.0, 5.0, 50.0, 500.0)))
                    for s in both:
                        s.collide(dd, dmg)
                    stats['ram'] += 1
            elif k < 0.9:
                pool = acts.get(0, []) + acts.get(2, [])
                if pool:
                    dd, dmg = rng.choice(pool), f32(rng.choice((0.0, 5.0, 50.0, 500.0)))
                    fl = 0x400 if rng.random() < 0.1 else 0
                    for s in both:
                        s.hit(dd, fl, dmg)
                    stats['hit'] += 1
            elif k < 0.94:
                p = (f32(rng.uniform(x0, x1)), f32(rng.uniform(-5, 50)), f32(rng.uniform(z1, z0)))
                for s in both:
                    s.player(*p)
                stats['player'] += 1
            elif k < 0.96:
                z = [rng.randrange(1, 4)] + [rng.choice((0, 1, 2, 3, 4, 0xff)) for _ in range(3)]
                for s in both:
                    s.zone(z)
                stats['zone'] += 1
            elif k < 0.97:
                lv = rng.randrange(4)
                for s in both:
                    s.level(lv)
            else:
                node = rng.choice(roots)
                for s in both:
                    s.drawn(node)
        dt = f32(rng.choice((1 / 30, 1 / 60, 0.05, 0.1, 0.0167, 0.125)))
        for s in both:
            s.frame(dt)
        full = (f + 1) % 25 == 0 or f == frames - 1 or f >= int(os.environ.get('MW3_FULL_FROM', '1000000'))
        want = emu.dump(full)
        cmp.marks = emu.marks
        cmp(f'{mission} frame {f}', want, port.dump(full))
        stats['frames'] += 1
    running = sum(1 for i in range(1, count) if emu.cpu.read(base + 0x13c * i + 0x98, 1)[0] == 2)
    stats['running_end'] += running


def main():
    args = [a for a in sys.argv[1:]]
    frames = int(args.pop(0)) if args and args[0].isdigit() else 60
    missions = args or WORLDS
    cmp = Compare()
    stats = {k: 0 for k in ('frames', 'start', 'start_at', 'start_node', 'reset', 'stop', 'invalidate', 'ram',
                            'hit', 'player', 'zone', 'running_end')}
    for i, m in enumerate(missions):
        run_mission(m, frames, 1000 + i, cmp, stats)
        print(f'{m}: ok ({stats["frames"]} frames so far)', flush=True)
    print(f'ok: {len(missions)} missions, post-load and {stats["frames"]} frames identical; '
          f'{cmp.words} words compared, {cmp.approx} within tolerance (worst {cmp.worst:.2e}); '
          f'calls: {", ".join(f"{k} {v}" for k, v in stats.items() if k != "frames")}; events run by type: '
          f'{", ".join(f"{k}:{v}" for k, v in sorted(EVENTS.items()))}')


if __name__ == '__main__':
    main()
