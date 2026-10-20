"""The projected-shadow casters and their images against the original x86
(docs/render-shadows.md), at the game's 24-bit x87 and at full precision.

    cmake --build build && .venv/bin/python tests/differential/test_shadow_image.py [runs per tree]

Precisions (the port's switch zmod::shadow_state().precision):
  game  the original at 24 bits as in play (blank images), port k24: identical.
  fix   the original at 24 bits but 0x55d010 at 53 (the candidate FIX), port
        k53: identical.
  x53   the original at 53 bits throughout (the CRT's setting, which Owen's
        Windows 11 + DDrawCompat appears to keep), port k53: within the
        last bits of the float code (counted and reported).
  full  the original at 64 bits throughout, port k53: likewise.
In every precision the original's 0x55d010 gets a zeroed stack (its edge
lists start as garbage there) and the port's level-edge FIX (bug) at the four
places it takes the garbage path (counted).

1. The polygon filler 0x55d010 alone on random polygons (level edges, row
   centres hit exactly, both windings): the image bytes.
2. The casters on every node tree of mechlib.zbd with the six worlds' suns
   (and random lights): create, size, node, enable, colour, position, draw
   (the node walk 0x4f7670 / 0x4f7750 / 0x5432d0, 0x543330, 0x55d010, the
   blur and ramp), upload ("gModShadow" through 0x57b130 with DirectDraw
   stubbed as test_texload.py does), a second frame (the reload 0x57baa0),
   release (0x57bb00) and delete, in random orders and numbers: after every
   call the caster list, every field, the image bytes, the texture records
   and the module's globals; after each draw every node's flags, class data
   and model header; every upload's RGBA.
3. The per-mech upkeep 0x4814e0 with every call out recorded (calltrace):
   random object lists, players, distances, clouds, suns, ground objects.

    --part N   run one part only
"""
import ctypes
import math
import os
import random
import struct
import sys
from fractions import Fraction
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_mechlib as ml  # noqa: E402
from original import ROOT, Original  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDI,  # noqa: E402
                                UC_X86_REG_ESI)

# MW3_PROBE=<lib>: another build of libmw3probe (the break tests).
probe = ctypes.CDLL(os.environ['MW3_PROBE']) if os.environ.get('MW3_PROBE') else ml.probe
P = ctypes.c_void_p
probe.probe_si_tree.argtypes = [ctypes.c_char_p, ctypes.c_long]
probe.probe_si_bytes.argtypes = [ctypes.c_int] * 4 + [P, ctypes.c_int]
probe.probe_si_state.argtypes = [ctypes.c_int, ctypes.c_float, ctypes.c_int]
probe.probe_si_op.argtypes = [ctypes.c_int] * 4 + [P]
probe.probe_si_dump.argtypes = [P, ctypes.c_int]
probe.probe_si_nodes.argtypes = [P, ctypes.c_int, P, ctypes.c_int]
probe.probe_si_log.argtypes = [P, ctypes.c_int]
probe.probe_si_reports.argtypes = [P, ctypes.c_int]
probe.probe_si_fill.argtypes = [P, ctypes.c_int, ctypes.c_int, P, ctypes.c_int, ctypes.c_uint32, ctypes.c_int,
                                ctypes.c_int]

SET_FORMAT, SET_TARGET, FILL = 0x5792c0, 0x557f90, 0x55d010
INIT, NEW, DELETE, SIZE, NODE, POSITION, COLOR, UPLOAD, RELEASE, ENABLE, DRAW = (
    0x542bb0, 0x542c20, 0x542ca0, 0x542d30, 0x542dd0, 0x542de0, 0x542e00, 0x542e10, 0x542e40, 0x542e60, 0x542e70)
CREATE, RELOAD, FREE_REC = 0x57b130, 0x57baa0, 0x57bb00
FILL_CALLS = (0x543654, 0x5436f7)
LEVEL_EDGES = {0x55d16e: (UC_X86_REG_EDI, UC_X86_REG_EBX), 0x55d285: (UC_X86_REG_ESI, UC_X86_REG_EDI),
               0x55d3b0: (UC_X86_REG_EDI, UC_X86_REG_EBX), 0x55d4c7: (UC_X86_REG_ESI, UC_X86_REG_EDI)}
HEAD, BLACK, WHITE, CURRENT, SHADOWS = 0x64fbe0, 0x64fbd8, 0x64fbdc, 0x73ff84, 0x74a264
TARGET, WINDING, SPAN_FN = 0x8007c8, 0x74a3c8, 0x8007c8 + 0x60
SLOTS, FLAGS, PSLOT, PFLAG = 0x63d760, 0x63d7e8, 0x5eda90, 0x5eda8c
MORPH, MORPH_FACTOR = 0x60bc20, 0x60bac8
ZERR, D3DERR, FPRINTF = 0x49bfd0, 0x580550, 0x5bb62c
DEFAULT_RECORD = 0x80931c
MAGIC = Fraction(6755399441055744)
CW = {'game': 0x007f, 'fix': 0x007f, 'x53': 0x027f, 'full': 0x037f}
PORT_PRECISION = {'game': 0, 'fix': 1, 'x53': 1, 'full': 1}


class Fail(Exception):
    pass


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


def fb(*xs):
    return struct.pack(f'<{len(xs)}f', *xs)


def round_bits(r, bits):
    """r (a positive Fraction) rounded to `bits` significant bits, ties to even."""
    e = r.numerator.bit_length() - r.denominator.bit_length()
    if Fraction(2) ** e > r:
        e -= 1
    ulp = Fraction(2) ** (e - bits + 1)
    return round(r / ulp) * ulp


def magic_x(x, precision):
    """0x55d010's 16.16 value of x (a float) at the x87 precision `precision`."""
    r = MAGIC + Fraction(x) * 65536
    if precision == 24:
        r = round_bits(r, 24)
    else:
        r = round_bits(r, 53)
    return int(r) & 0xffffffff


# ---------------------------------------------------------------- the original
class ShadowEmu:
    def __init__(self, data):
        self.ml = ml.Emulated(data)
        cpu = self.cpu = self.ml.cpu
        self.ml.next = ml.BIG
        # malloc'd memory reads as zero (the port's records start zeroed; the
        # original's fields 0x542c20 leaves are malloc's garbage).
        cpu.stub_import(ml.IAT_MALLOC, lambda c: self.ml.zeroed(c.arg(0)))
        cpu.call(SET_FORMAT, ecx=5, edx=6, stack_args=(5, 0, 0, 0))
        cpu.u32(0x74639c, 0x74039c)
        cpu.u32(0x800828, 0x564080)
        cpu.u32(0x72f348, 1)
        self.base = cpu.alloc(48)
        for a, v in ((0x8026e0, 1), (0x8093c8, 0x40), (0x809438, 1), (0x80943c, 4), (0x809454, 4)):
            cpu.u32(a, v)
        self.reports = []
        cpu.stub_function(ZERR, lambda c: self.reports.append((c.arg(0), c.arg(2))))
        cpu.stub_function(D3DERR, lambda c: self.fail('d3d error'), 4)
        self.printed = []
        cpu.stub_import(FPRINTF, lambda c: self.printed.append(c.arg(2)))
        self.setup_d3d()
        # 0x55d010 through a trampoline: the stack below cleared, and at
        # 'fix' the x87 at 53 bits around it.
        cw53, cw24 = cpu.alloc(4), cpu.alloc(4)
        cpu.u32(cw53, 0x027f), cpu.u32(cw24, 0x007f)
        clear = (b'\x57\x51\x50'                      # push edi, ecx, eax
                 b'\x8d\xbc\x24\x00\xf0\xff\xff'      # lea edi, [esp - 0x1000]
                 b'\xb9\x00\x04\x00\x00'              # mov ecx, 0x400
                 b'\x31\xc0\xfc\xf3\xab'              # xor eax, eax; cld; rep stosd
                 b'\x58\x59\x5f')                     # pop eax, ecx, edi
        # The call sites are patched once (Unicorn keeps translated code); a
        # flag in memory chooses the precision.
        self.fix_flag = cpu.alloc(4)
        cpu.u32(self.fix_flag, 0)
        test = b'\x83\x3d' + struct.pack('<I', self.fix_flag) + b'\x00\x74\x06'  # cmp [flag], 0; je +6
        pre = test + b'\xd9\x2d' + struct.pack('<I', cw53)
        post = test + b'\xd9\x2d' + struct.pack('<I', cw24)
        addr = cpu.code_next
        head = clear + pre + b'\xff\x74\x24\x04\xe8'   # push dword [esp + 4]; call FILL
        rel = FILL - (addr + len(head) + 4)
        self.fill_entry = cpu.code(head + struct.pack('<i', rel) + post + b'\xc2\x04\x00')  # ret 4
        for site in FILL_CALLS:
            cpu.write(site + 1, struct.pack('<i', self.fill_entry - (site + 5)))
        self.level_hits = 0
        for addr, regs in LEVEL_EDGES.items():
            cpu.uc.hook_add(UC_HOOK_CODE, lambda uc, a, s, u, r=regs: self.level_edge(r), begin=addr, end=addr)
        cpu.uc.hook_add(UC_HOOK_CODE, lambda uc, a, s, u: self.created(), begin=0x542e2d, end=0x542e2d)
        cpu.uc.hook_add(UC_HOOK_CODE, lambda uc, a, s, u: self.pending.append(['create']), begin=CREATE, end=CREATE)
        cpu.uc.hook_add(UC_HOOK_CODE, lambda uc, a, s, u: self.reloading(), begin=RELOAD, end=RELOAD)
        cpu.uc.hook_add(UC_HOOK_CODE, lambda uc, a, s, u: self.freeing(), begin=FREE_REC, end=FREE_REC)
        self.precision = 24
        self.cw_code = {}

    def fail(self, what):
        raise Fail(f'original: unexpected {what}')

    def u32(self, a):
        return struct.unpack('<I', self.cpu.read(a, 4))[0]

    def i32(self, a):
        return struct.unpack('<i', self.cpu.read(a, 4))[0]

    def set_mode(self, mode):
        cpu = self.cpu
        if mode not in self.cw_code:  # one fldcw routine each (Original.fpu_control writes a new one)
            slot = cpu.alloc(4)
            cpu.u32(slot, CW[mode])
            self.cw_code[mode] = cpu.code(b'\xd9\x2d' + struct.pack('<I', slot) + b'\xc3')
        cpu.call(self.cw_code[mode])
        self.precision = {'game': 24, 'fix': 53, 'x53': 53, 'full': 64}[mode]
        cpu.u32(self.fix_flag, int(mode == 'fix'))

    def level_edge(self, regs):
        entry = self.cpu.uc.reg_read(regs[0])
        x = struct.unpack('<f', self.cpu.read(self.cpu.uc.reg_read(regs[1]), 4))[0]
        self.cpu.u32(entry, 0)
        self.cpu.u32(entry + 8, magic_x(x, 24 if self.precision == 24 else 53))
        self.level_hits += 1

    # DirectDraw, as test_texload.py stubs it.
    def setup_d3d(self):
        cpu = self.cpu
        dd, ddt = cpu.alloc(16), cpu.alloc(0x100)
        cpu.u32(dd, ddt)
        cpu.u32(0x809358, dd)

        def method(table, off, fn, pop):
            addr = cpu.code(b'\xc3')
            cpu.u32(table + off, addr)
            cpu.stub_function(addr, fn, pop)
        self.surf_tab, self.tex_tab = cpu.alloc(0x100), cpu.alloc(0x100)
        method(ddt, 0x18, self.create_surface, 16)
        method(ddt, 0x14, lambda c: self.fail('CreatePalette'), 20)
        method(self.surf_tab, 0x00, self.query, 12)
        method(self.surf_tab, 0x08, lambda c: 0, 4)
        method(self.surf_tab, 0x64, self.lock, 20)
        method(self.surf_tab, 0x80, self.unlock, 8)
        method(self.surf_tab, 0x7c, lambda c: self.fail('SetPalette'), 8)
        method(self.tex_tab, 0x08, lambda c: 0, 4)
        method(self.tex_tab, 0x14, lambda c: 0, 8)
        self.caps, self.dev = cpu.alloc(0x200), cpu.alloc(0xc00)
        cpu.u32(0x80c278, self.caps)
        cpu.u32(0x809644, self.dev)
        self.default_record = cpu.alloc(0x18)
        cpu.u32(DEFAULT_RECORD, self.default_record)
        cpu.u32(0x802718, 16)
        cpu.write(0x80272c, struct.pack('<3I', 0xf800, 0x07e0, 0x001f))
        cpu.write(self.caps + 0x148, struct.pack('<II', 4096, 4096))
        cpu.u32(0x809448, 0)
        cpu.u32(self.dev + 0xb04, 1)
        cpu.u32(0x72f364, CREATE)
        cpu.u32(0x72f374, RELOAD)
        cpu.u32(0x72f378, FREE_REC)

    def new_obj(self, tab):
        a = self.ml.alloc(16)
        self.cpu.u32(a, tab)
        return a

    def create_surface(self, c):
        desc = c.arg(1)
        h, w = struct.unpack('<II', c.read(desc + 8, 8))
        fmt = struct.unpack('<5I', c.read(desc + 0x48 + 0xc, 20))
        s = self.new_obj(self.surf_tab)
        self.surfaces[s] = [w, h, fmt, 0]
        self.cpu.u32(c.arg(2), s)
        return 0

    def query(self, c):
        self.cpu.u32(c.arg(2), self.new_obj(self.tex_tab))
        return 0

    def lock(self, c):
        s, desc = c.arg(0), c.arg(2)
        w, h, fmt, mem = self.surfaces[s]
        if not mem:
            mem = self.surfaces[s][3] = self.ml.alloc(2 * w * h)
            self.cpu.write(mem, b'\xee' * (2 * w * h))
        self.cpu.u32(desc + 0x10, 2 * w)
        self.cpu.u32(desc + 0x24, mem)
        return 0

    def unlock(self, c):
        from test_texload import decode
        w, h, fmt, mem = self.surfaces[c.arg(0)]
        if not self.pending:
            self.fail('unlock outside a texture call')
        rgba = decode(self.cpu.read(mem, 2 * w * h), fmt)
        p = self.pending[-1]
        if p[0] == 'update':
            self.log.append((2, p[1], w, h, rgba))
            self.pending.pop()
        else:
            p += [w, h, rgba]
        return 0

    def created(self):
        rec = self.cpu.uc.reg_read(UC_X86_REG_EAX)
        if rec == self.default_record:
            self.pending.pop()
            return
        self.textures[rec] = len(self.textures) + 1
        kind, w, h, rgba = self.pending.pop()
        self.log.append((1, w, h, self.i32(rec + 0xc), rgba))

    def reloading(self):
        rec = self.cpu.uc.reg_read(UC_X86_REG_ECX)
        self.pending.append(['update', self.textures.get(rec, -2)])

    def freeing(self):
        rec = self.cpu.uc.reg_read(UC_X86_REG_ECX)
        if rec not in (self.default_record,):
            self.log.append((3, self.textures.get(rec, -2)))

    def take_log(self):
        self.pending = []
        log = self.log
        self.log = []
        return log

    # --------------------------------------------- a run
    def start(self, offset, mode):
        cpu = self.cpu
        self.ml.tree(offset, 0)
        self.set_mode(mode)
        u = self.u32
        bs = self.i32(ml.BLOCK_SIZE)
        self.blocks = [u(u(ml.BLOCKS) + 4 * b) for b in range(self.i32(ml.NBLOCKS))]
        self.bs = bs
        self.total = self.i32(ml.TOTAL)
        for a in (HEAD, BLACK, WHITE, CURRENT, SHADOWS, WINDING, MORPH, MORPH_FACTOR):
            cpu.u32(a, 0)
        cpu.write(TARGET, bytes(0x2c))
        cpu.u32(0x8007d8, 2)
        cpu.u32(0x74a76c, 0)
        cpu.write(self.base, fb(1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0))
        # As the port's reset_stack, push(base), load_identity: slot 1.
        cpu.u32(SLOTS + 4, self.base), cpu.u32(FLAGS + 4, 1)
        cpu.u32(PSLOT, SLOTS + 4), cpu.u32(PFLAG, FLAGS + 4)
        self.surfaces, self.textures, self.pending, self.log = {}, {}, [], []
        self.ids, self.dead = {}, set()
        self.reports, self.printed = [], []
        self.level_hits = 0

    def node_addr(self, i):
        return self.blocks[i // self.bs] + (i % self.bs) * 0xd4

    def node_index(self, a):
        if a == 0:
            return -1
        for b, base in enumerate(self.blocks):
            if base <= a < base + self.bs * 0xd4:
                return b * self.bs + (a - base) // 0xd4
        return -2

    def bytes_at(self, kind, i, off, n, data=None):
        a = self.node_addr(i)
        if kind == 1:
            a = self.u32(a + 0x38)
        elif kind == 2:
            a = self.u32(a + 0x3c)
        elif kind == 3:  # polygon `off`'s flags
            m = self.u32(a + 0x3c)
            a, off = (self.u32(m + 0x34) + 0x24 * off, 0) if m else (0, 0)
        if not a:
            return None
        if data is not None:
            self.cpu.write(a + off, data)
            return data
        return self.cpu.read(a + off, n)

    def op(self, op, cid=0, a=0, b=0, f=()):
        cpu = self.cpu
        rec = self.ids.get(cid, 0)
        if op == 0:
            cpu.call(INIT)
        elif op == 1:
            cpu.call(NEW)
            r = cpu.uc.reg_read(UC_X86_REG_EAX)
            self.ids[len(self.ids) + 1] = r
            return len(self.ids)
        elif op == 2:
            cpu.call(SIZE, ecx=rec, edx=a, stack_args=(b,))
        elif op == 3:
            cpu.call(NODE, ecx=rec, edx=self.node_addr(a) if a >= 0 else 0)
        elif op == 4:
            cpu.call(ENABLE, ecx=rec, edx=a)
        elif op == 5:
            cpu.call(COLOR, ecx=rec, edx=a)
        elif op == 6:
            p = self.ml.alloc(12)
            cpu.write(p, fb(*f[:3]))
            cpu.call(POSITION, ecx=rec, edx=p)
        elif op == 7:
            p = self.ml.alloc(12)
            cpu.write(p, fb(*f[:3]))
            cpu.call(DRAW, ecx=rec, edx=p, stack_args=(struct.unpack('<I', fb(f[3]))[0],))
            return struct.unpack('<i', struct.pack('<I', cpu.uc.reg_read(UC_X86_REG_EAX)))[0]
        elif op == 8:
            cpu.call(UPLOAD, ecx=rec)
        elif op == 9:
            cpu.call(RELEASE, ecx=rec)
        elif op == 10:
            cpu.call(DELETE, ecx=rec)
            self.dead.add(cid)
        return 0

    def dump(self):
        u, i32 = self.u32, self.i32
        rid = {v: k for k, v in self.ids.items() if k not in self.dead}

        def cid(a):
            return 0 if a == 0 else rid.get(a, -1)
        out = [struct.pack('<5i', cid(u(HEAD)), i32(BLACK), i32(WHITE), cid(u(CURRENT)), i32(SHADOWS))]
        t = self.cpu.read(TARGET, 0x2c)
        width, height, pitch, nbytes = struct.unpack_from('<4i', t, 4)
        rect = struct.unpack_from('<4i', t, 0x1c)
        out.append(struct.pack('<9i', width, height, pitch, nbytes, *rect, i32(WINDING)))
        out.append(struct.pack('<i', (u(PSLOT) - SLOTS) // 4))
        c, guard = u(HEAD), 0
        while c and guard < 64:
            r = self.cpu.read(c, 0x8c)
            out.append(struct.pack('<ii', cid(c), self.node_index(struct.unpack_from('<I', r, 0)[0])))
            out.append(r[4:0x18] + r[0x18:0x1c] + r[0x1c:0x2c] + r[0x2c:0x38] + r[0x38:0x44] + r[0x44:0x4c])
            out.append(r[0x58:0x5c])
            img, tex = struct.unpack_from('<II', r, 0x5c)
            out.append(struct.pack('<i', int(img != 0)))
            if img:
                m = self.cpu.read(img, 0x38)
                w, h = struct.unpack_from('<hh', m, 4)
                pix, alpha = struct.unpack_from('<II', m, 0x10)
                out.append(struct.pack('<7i', struct.unpack_from('<i', m, 0)[0], w, h, m[9],
                                       struct.unpack_from('<i', m, 0x34)[0], int(alpha != 0), int(pix != 0)))
                if pix:
                    out.append(self.cpu.read(pix, 2 * w * h))
            if tex == 0:
                out.append(struct.pack('<i', 0))
            elif tex == self.default_record:
                out.append(struct.pack('<i', -2))
            else:
                out.append(struct.pack('<4i', self.textures.get(tex, -3), *struct.unpack('<3i', self.cpu.read(tex + 0xc, 12))))
            prev, nxt = struct.unpack_from('<II', r, 0x84)
            out.append(struct.pack('<ii', cid(prev), cid(nxt)))
            c = nxt
            guard += 1
        return b''.join(out)

    def nodes(self, live):
        out = []
        for k in live:
            raw = self.cpu.read(self.node_addr(k), 0xd4)
            cls, = struct.unpack_from('<i', raw, 0x34)
            data, model = struct.unpack_from('<II', raw, 0x38)
            out.append(struct.pack('<i', k) + raw[0x24:0x28])
            if cls == 5 and data:
                out.append(self.cpu.read(data, 0x90))
            elif cls == 6 and data:
                out.append(self.cpu.read(data, 0x50))
            if model:
                out.append(self.cpu.read(model + 8, 4) + self.cpu.read(model + 0x24, 4))
        return b''.join(out)

    def fill(self, pixels, w, h, verts, colour, winding):
        cpu = self.cpu
        pix = self.ml.alloc(2 * w * h)
        cpu.write(pix, pixels)
        rect = self.ml.alloc(16)
        cpu.write(rect, struct.pack('<4i', 0, 0, w - 1, h - 1))
        cpu.call(SET_TARGET, ecx=pix, edx=rect, stack_args=(0x10, 2 * w))
        cpu.u32(WINDING, winding)
        v = self.ml.alloc(12 * len(verts))
        cpu.write(v, b''.join(fb(x, y, 0) for x, y in verts))
        cpu.call(self.fill_entry, ecx=v, edx=len(verts), stack_args=(colour,))
        return cpu.read(pix, 2 * w * h)


# ---------------------------------------------------------------- the port
BUF = ctypes.create_string_buffer(1 << 22)
LOG = (ctypes.c_int32 * (1 << 21))()


def port_dump():
    n = probe.probe_si_dump(BUF, len(BUF))
    if n < 0:
        raise Fail('probe dump overflow')
    return BUF.raw[:n]


def port_nodes(live):
    n = probe.probe_si_nodes((ctypes.c_int32 * len(live))(*live), len(live), BUF, len(BUF))
    if n < 0:
        raise Fail('probe_si_nodes')
    return BUF.raw[:n]


def port_log():
    n = probe.probe_si_log(LOG, len(LOG))
    w = LOG[:n]
    out, i = [], 0
    while i < n:
        k = w[i]
        if k == 1:
            wd, ht, ty = w[i + 1:i + 4]
            rgba = struct.pack(f'<{wd * ht}i', *w[i + 4:i + 4 + wd * ht])
            out.append((1, wd, ht, ty, rgba))
            i += 4 + wd * ht
        elif k == 2:
            tid, wd, ht = w[i + 1:i + 4]
            rgba = struct.pack(f'<{wd * ht}i', *w[i + 4:i + 4 + wd * ht])
            out.append((2, tid, wd, ht, rgba))
            i += 4 + wd * ht
        elif k == 3:
            out.append((3, w[i + 1]))
            i += 2
        else:
            raise Fail(f'bad log word {k}')
    return out


def port_reports():
    buf = (ctypes.c_int32 * 256)()
    n = probe.probe_si_reports(buf, 256)
    return [(buf[2 * i], buf[2 * i + 1]) for i in range(n // 2)]


def port_op(op, cid=0, a=0, b=0, f=()):
    fl = (ctypes.c_float * 4)(*(list(f) + [0] * (4 - len(f))))
    return probe.probe_si_op(op, cid, a & 0xffffffff if a >= 0 else a, b, fl)


def port_bytes(write, kind, i, off, data_or_n):
    if write:
        b = ctypes.create_string_buffer(bytes(data_or_n), len(data_or_n))
        return probe.probe_si_bytes(1, kind, i, off, b, len(data_or_n))
    b = ctypes.create_string_buffer(data_or_n)
    r = probe.probe_si_bytes(0, kind, i, off, b, data_or_n)
    return b.raw[:data_or_n] if r >= 0 else None


# ---------------------------------------------------------------- comparing
class Stats:
    def __init__(self):
        self.d = {}

    def add(self, k, n=1):
        self.d[k] = self.d.get(k, 0) + n


def first_diff(a, b):
    for i in range(min(len(a), len(b))):
        if a[i] != b[i]:
            return i
    return min(len(a), len(b))


def max_rel(a, b):
    """Byte strings of equal length, word by word as floats: the largest relative
    difference; inf where a word that is not a normal float (flags, NaN) differs."""
    if len(a) != len(b):
        return math.inf
    worst = 0.0
    for i in range(0, len(a) - len(a) % 4, 4):
        if a[i:i + 4] == b[i:i + 4]:
            continue
        x, y = struct.unpack('<f', a[i:i + 4])[0], struct.unpack('<f', b[i:i + 4])[0]
        m = max(abs(x), abs(y))
        if not (math.isfinite(x) and math.isfinite(y)) or m < 1e-30:
            return math.inf
        worst = max(worst, abs(x - y) / m)
    return worst


FULL_TOL = 1e-3  # relative: a box 1/width under cancellation magnifies the last bits


def close_floats(a, b, stats=None, what=''):
    r = max_rel(a, b)
    if stats is not None and r < math.inf:
        k = f'{what}: max relative difference'
        stats.d[k] = max(stats.d.get(k, 0), r)
    return r <= FULL_TOL


def fields_close(x, y, stats, mode):
    """A record's floats: all but the scales within FULL_TOL; the scales (1 / box
    width) by their widths, within FULL_TOL of the box's magnitude (a narrow
    box's width loses the last bits of its coordinates)."""
    if not close_floats(x[:64], y[:64], stats, f'{mode} records'):
        return False
    fx, fy = struct.unpack('<18f', x), struct.unpack('<18f', y)
    big = max(1.0, *(abs(v) for v in fx[10:16] if math.isfinite(v)))
    for a, b in zip(fx[16:18], fy[16:18]):
        if a == b or (a != a and b != b):
            continue
        if not (a and b and math.isfinite(a) and math.isfinite(b)) or abs(1 / a - 1 / b) > FULL_TOL * big * 1e-3:
            return False
    return True


def parse_dump(b):
    """A dump as (globals, [caster: (ids, floats, enabled, image meta, pixels, texture, links)])."""
    head, p = b[:60], 60
    casters = []
    while p < len(b):
        ids, fl = b[p:p + 8], b[p + 8:p + 8 + 0x48]
        p += 8 + 0x48
        en = b[p:p + 4]
        has, = struct.unpack_from('<i', b, p + 4)
        p += 8
        meta, pix = b'', b''
        if has:
            meta = b[p:p + 28]
            w, h = struct.unpack_from('<ii', meta, 4)
            haspix, = struct.unpack_from('<i', meta, 24)
            p += 28
            if haspix:
                pix = b[p:p + 2 * w * h]
                p += 2 * w * h
        t, = struct.unpack_from('<i', b, p)
        tex = b[p:p + (16 if t > 0 or t == -3 else 4)]
        p += len(tex)
        links = b[p:p + 8]
        p += 8
        casters.append((ids, fl, en, meta, pix, tex, links))
    return head, casters


NAMES = ('ids', 'fields', 'enabled', 'image', 'pixels', 'texture', 'links')


def compare_dumps(want, got, mode, stats, where):
    if want == got:
        return
    hw, cw = parse_dump(want)
    hg, cg = parse_dump(got)
    if hw != hg:
        raise Fail(f'{where}: globals {struct.unpack("<15i", hw)} vs {struct.unpack("<15i", hg)}')
    if len(cw) != len(cg):
        raise Fail(f'{where}: {len(cw)} casters vs {len(cg)}')
    for k, (a, b) in enumerate(zip(cw, cg)):
        for j, (name, x, y) in enumerate(zip(NAMES, a, b)):
            if x == y:
                continue
            if mode in ('full', 'x53') and name == 'fields' and fields_close(x, y, stats, mode):
                stats.add(f'{mode}: record floats not bit-identical')
                continue
            if mode in ('full', 'x53') and name == 'pixels' and len(x) == len(y):
                stats.add(f'{mode}: images differing')
                stats.add(f'{mode}: pixels differing', sum(x[i:i + 2] != y[i:i + 2] for i in range(0, len(x), 2)))
                continue
            i = first_diff(x, y)
            if name == 'fields':
                fx, fy = struct.unpack('<18f', x), struct.unpack('<18f', y)
                raise Fail(f'{where}: caster {k} fields: ' +
                           ', '.join(f'[{j}] {a!r} vs {b!r}' for j, (a, b) in enumerate(zip(fx, fy))
                                     if struct.pack('<f', a) != struct.pack('<f', b)))
            raise Fail(f'{where}: caster {k} {name} at byte {i}: {x[i - i % 4:i - i % 4 + 8].hex()} '
                       f'vs {y[i - i % 4:i - i % 4 + 8].hex()}')


# ---------------------------------------------------------------- part 1: the filler
def random_polygon(rng, w, h):
    n = rng.choice([3, 3, 4, 4, 5, 6, 8, 10, 12])
    xmax, ymax = w - 0.5, h - 0.5

    def snap(v, top):
        r = rng.random()
        if r < 0.15:
            v = math.floor(v) + 0.5
        elif r < 0.25:
            v = math.floor(v) + 0.5 + rng.choice([2 ** -18, 2 ** -17, -2 ** -18, 2 ** -12])
        elif r < 0.3:
            v = float(round(v))
        return min(max(v, 0.0), top)
    if rng.random() < 0.6:  # convex, around a centre, either way round
        cx, cy = rng.uniform(0, xmax), rng.uniform(0, ymax)
        rad = rng.uniform(0.3, max(w, h) * 0.6)
        angles = sorted(rng.uniform(0, 2 * math.pi) for _ in range(n))
        if rng.random() < 0.5:
            angles.reverse()
        pts = [(cx + rad * math.cos(a), cy + rad * math.sin(a)) for a in angles]
    else:  # anything: concave, crossing, degenerate
        pts = [(rng.uniform(0, xmax), rng.uniform(0, ymax)) for _ in range(n)]
    pts = [(f32(snap(x, xmax)), f32(snap(y, ymax))) for x, y in pts]
    if rng.random() < 0.3:  # a level edge
        i = rng.randrange(n)
        j = (i + 1) % n
        pts[j] = (pts[j][0], pts[i][1])
    return pts


def part_fill(emu, rng, count, stats):
    for mode in ('game', 'fix', 'x53', 'full'):
        emu.set_mode(mode)
        drew = 0
        for case in range(count):
            w, h = rng.choice([(64, 64), (32, 32), (17, 9)])
            pts = random_polygon(rng, w, h)
            colour = rng.randrange(1, 0x10000)
            winding = rng.randint(0, 1)
            base = bytes([0x55, 0xaa]) * (w * h)
            hits = emu.level_hits
            want = emu.fill(base, w, h, pts, colour, winding)
            stats.add(f'{mode}: level-edge entries (fill)', emu.level_hits - hits)
            pix = ctypes.create_string_buffer(base, len(base))
            v = (ctypes.c_float * (3 * len(pts)))(*[c for x, y in pts for c in (x, y, 0.0)])
            probe.probe_si_fill(pix, w, h, v, len(pts), colour, winding, PORT_PRECISION[mode])
            got = pix.raw[:len(base)]
            drew += want != base
            if want != got:
                if mode in ('full', 'x53'):
                    stats.add(f'{mode}: fills differing')
                    stats.add(f'{mode}: fill pixels differing',
                              sum(want[i:i + 2] != got[i:i + 2] for i in range(0, len(want), 2)))
                    continue
                i = first_diff(want, got)
                raise Fail(f'fill {mode} case {case}: pixel ({(i // 2) % w}, {(i // 2) // w}) '
                           f'{want[i:i + 2].hex()} vs {got[i:i + 2].hex()}; winding {winding}, {pts}')
        stats.add(f'{mode}: polygons that filled anything (of {count})', drew)


# ---------------------------------------------------------------- part 2: the casters
def real_suns():
    """Every directed light of the six worlds after the original's light update."""
    from test_gamez import Emulated as WorldEmu
    import test_lighting as tl
    rng = random.Random(7)
    emu = WorldEmu()
    emu.zerr_stubbed = True
    emu.reports = []
    cpu = emu.cpu
    cpu.stub_function(ZERR, lambda c: 0)
    suns = []
    for world in tl.WORLDS:
        path = tl.ZBD / world / 'gamez.zbd'
        emu.read_zbd(path.read_bytes(), str(path).encode())
        i32 = emu.i32
        bs, cap = i32(0x5ea830), i32(0x60bf34)
        blocks = [i32(i32(0x60bf38) + 4 * b) & 0xffffffff for b in range(i32(0x60bf30))]
        world_node = None
        for i in range(cap):
            a = blocks[i // bs] + (i % bs) * 0xd4
            h = cpu.read(a, 0xd4)
            if struct.unpack_from('<I', h, 0xd0)[0] & 0x1000000 and struct.unpack_from('<i', h, 0x34)[0] == 2:
                world_node = a
                break
        wd = i32(world_node + 0x38) & 0xffffffff
        n, dp = i32(wd + 0xa0), i32(wd + 0xa8) & 0xffffffff
        base = cpu.alloc(48)
        cpu.write(tl.VIEW, tl.floats(*tl.rand_matrix(rng)))
        cpu.u32(SLOTS + 4, base), cpu.u32(FLAGS + 4, 0), cpu.u32(PSLOT, SLOTS + 4), cpu.u32(PFLAG, FLAGS + 4)
        cpu.u32(0x72f348, 1)
        cpu.call(tl.WORLD_UPDATE, ecx=world_node)
        for k in range(n):
            d = i32(dp + 4 * k) & 0xffffffff
            if i32(d + 0xb0) & 8:
                direction = struct.unpack('<3f', cpu.read(d + 0x84, 12))
                diffuse, ambient = struct.unpack('<2f', cpu.read(d + 0x9c, 8))
                suns.append((world, direction, diffuse, ambient))
    return suns


def rotation(rng):
    """Rows of a yaw about y, sometimes tilted about x."""
    yaw = rng.uniform(-math.pi, math.pi)
    tilt = rng.uniform(-0.3, 0.3) if rng.random() < 0.3 else 0.0
    cy, sy, ct, st = math.cos(yaw), math.sin(yaw), math.cos(tilt), math.sin(tilt)
    return [f32(v) for v in (cy, 0.0, -sy, sy * st, ct, cy * st, sy * ct, -st, cy * ct)]


class Run:
    """One sequence of caster calls on one tree, made on both sides and compared after each."""

    def __init__(self, emu, mode, stats, where):
        self.emu, self.mode, self.stats, self.where = emu, mode, stats, where

    def both_bytes(self, kind, i, off, data):
        if self.emu.bytes_at(kind, i, off, len(data), data) is not None:
            port_bytes(1, kind, i, off, data)

    def get_both(self, kind, i, off, n):
        a = self.emu.bytes_at(kind, i, off, n)
        b = port_bytes(0, kind, i, off, n)
        if a != b:
            raise Fail(f'{self.where}: node {i} kind {kind} +{off:#x} differs before the run')
        return a

    def op(self, op, cid=0, a=0, b=0, f=()):
        want = self.emu.op(op, cid, a, b, f)
        got = port_op(op, cid, a, b, f)
        tag = f'{self.where} op {op}'
        if want != got:
            raise Fail(f'{tag}: returned {want} vs {got}')
        compare_dumps(self.emu.dump(), port_dump(), self.mode, self.stats, tag)
        lw, lg = self.emu.take_log(), port_log()
        if [x[:-1] if x[0] != 3 else x for x in lw] != [y[:-1] if y[0] != 3 else y for y in lg]:
            raise Fail(f'{tag}: texture calls {[x[:4] for x in lw]} vs {[y[:4] for y in lg]}')
        for x, y in zip(lw, lg):
            self.stats.add(f'{self.mode}: texture calls {("", "create", "update", "release")[x[0]]}')
            if x[0] != 3 and x[-1] != y[-1]:
                if self.mode not in ('full', 'x53'):
                    i = first_diff(x[-1], y[-1])
                    raise Fail(f'{tag}: upload RGBA at pixel {i // 4}: {x[-1][i - i % 4:][:4].hex()} vs '
                               f'{y[-1][i - i % 4:][:4].hex()}')
                self.stats.add(f'{self.mode}: uploads differing')
        rw, rg = self.emu.reports, port_reports()
        if rw != rg:
            raise Fail(f'{tag}: reports {rw} vs {rg}')
        self.emu.reports = []
        if op == 7:
            nw, ng = self.emu.nodes(self.live), port_nodes(self.live)
            if nw != ng:
                if self.mode not in ('full', 'x53') or not close_floats(nw, ng, self.stats, f'{self.mode} node data'):
                    i = first_diff(nw, ng)
                    if os.environ.get('MW3_DUMP'):
                        self.locate(nw, i)
                    raise Fail(f'{tag}: node state at byte {i} of {len(nw)}: {nw[i - i % 4:][:8].hex()} vs '
                               f'{ng[i - i % 4:][:8].hex()}')
                self.stats.add(f'{self.mode}: node states not bit-identical')
        return want


def _locate(self, nw, i):
    p = 0
    for k in self.live:
        raw = self.emu.cpu.read(self.emu.node_addr(k), 0xd4)
        cls, = struct.unpack_from('<i', raw, 0x34)
        data, model = struct.unpack_from('<II', raw, 0x38)
        size = 8 + (0x90 if cls == 5 and data else 0x50 if cls == 6 and data else 0) + (8 if model else 0)
        if p <= i < p + size:
            parent = struct.unpack_from('<I', raw, 0x58)[0]
            print(f'node {k} cls {cls} byte {i - p} (data +{i - p - 8:#x}); flags {struct.unpack_from("<I", nw, p + 4)[0]:#x}')
            return
        p += size


Run.locate = _locate


def one_run(emu, path, start, name, mode, rng, suns, stats):
    emu.start(start, mode)
    root = probe.probe_si_tree(str(path).encode(), start)
    probe.probe_si_init()
    morph = int(rng.random() < 0.15)
    factor = f32(rng.uniform(-0.5, 1.5)) if rng.random() < 0.9 else 0.0
    probe.probe_si_state(morph, factor, PORT_PRECISION[mode])
    emu.cpu.u32(MORPH, morph)
    emu.cpu.write(MORPH_FACTOR, fb(factor))
    run = Run(emu, mode, stats, f'{mode} {name}')
    live = [k for k in range(emu.total) if any(emu.cpu.read(emu.node_addr(k) + 0x24, 0x14))]
    run.live = live
    if root not in live:
        raise Fail(f'{name}: port root {root} not a node of the original')
    for k in live:
        raw = emu.cpu.read(emu.node_addr(k), 0xd4)
        cls, = struct.unpack_from('<i', raw, 0x34)
        # The frame's node update (0x4f7f70) has run: no node is left changed
        # (bit 0, list 7), so 0x4ed280 does not call 0x4f7e00.
        flags = struct.unpack_from('<I', raw, 0x24)[0] & ~3
        if k != root:
            if rng.random() < 0.04:
                flags &= ~4
            if rng.random() < 0.08:
                flags ^= 0x80000
        run.both_bytes(0, k, 0x24, struct.pack('<I', flags))
        if cls == 5 and struct.unpack_from('<I', raw, 0x38)[0]:
            df, = struct.unpack('<I', run.get_both(1, k, 0, 4))
            df = (df | 0x20) if rng.random() < 0.9 else (df & ~0x20)
            if rng.random() < 0.03:
                df |= 8
            run.both_bytes(1, k, 0, struct.pack('<I', df))
        if cls == 6 and struct.unpack_from('<I', raw, 0x38)[0]:
            run.both_bytes(1, k, 4, fb(rng.choice([0.0, -1.0, 2.0, f32(rng.uniform(-5, 5))])))
        model = struct.unpack_from('<I', raw, 0x3c)[0]
        if k != root and rng.random() < 0.06:  # other classes: a LoD (0x4f7750), one casting nothing, a bad one
            cls = rng.choice([6, 6, 6, 7, 1, 11, 3])
            run.both_bytes(0, k, 0x34, struct.pack('<i', cls))
            stats.add(f'nodes made class {cls}')
            if cls == 6:  # its +4 (the object3d's bytes here): cast when not above 0
                run.both_bytes(1, k, 4, fb(rng.choice([0.0, -1.0, 2.0, float('nan')])))
        if model:
            npolys = emu.u32(model + 0x10)
            for j in range(npolys):
                if rng.random() < 0.08:  # a strip (mechlib has none)
                    pf, = struct.unpack('<I', run.get_both(3, k, j, 4))
                    run.both_bytes(3, k, j, struct.pack('<I', pf | 0x400))
                    stats.add('polygons made strips')
        if model and rng.random() < 0.1:
            mf, = struct.unpack('<I', run.get_both(2, k, 8, 4))
            run.both_bytes(2, k, 8, struct.pack('<I', mf | 8))
            run.both_bytes(2, k, 0x24, fb(f32(rng.uniform(-1, 1))))

    def pose():
        pos = [f32(rng.uniform(-800, 800)), f32(rng.uniform(-30, 120)), f32(rng.uniform(-800, 800))]
        run.both_bytes(1, root, 0x30, fb(*(rotation(rng) + pos)))
        df, = struct.unpack('<I', run.get_both(1, root, 0, 4))
        run.both_bytes(1, root, 0, struct.pack('<I', (df | 0x20) & ~8))
        return pos

    def light():
        r = rng.random()
        if r < 0.75 and suns:
            return list(rng.choice(suns)[1])
        if r < 0.97:
            d = [rng.uniform(-1, 1), rng.uniform(-1, -0.05), rng.uniform(-1, 1)]
        else:
            d = [rng.uniform(-1, 1), rng.choice([0.0, 0.3, float('nan')]), rng.uniform(-1, 1)]
        n = math.sqrt(sum(x * x for x in d if x == x)) or 1
        return [f32(x / n) for x in d]

    pos = pose()
    run.op(0)
    ids = []
    for _ in range(rng.choice([1, 1, 1, 2, 3])):
        cid = run.op(1)
        ids.append(cid)
        size = rng.choice([64, 64, 32, 32, 16])
        run.op(2, cid, size, size)
        if rng.random() < 0.1:
            run.op(2, cid, size, size)  # the same size: kept
        run.op(3, cid, root if rng.random() < 0.8 else rng.choice(live))
        run.op(4, cid, 0 if rng.random() < 0.05 else 1)
    for frame in range(rng.choice([1, 2, 2, 3])):
        if frame:
            pos = pose()
        for cid in rng.sample(ids, len(ids)):
            g = rng.randrange(256)
            run.op(5, cid, port_op(11, g, g, g))
            run.op(6, cid, 0, 0, pos)
            ground = f32(pos[1] + rng.uniform(-4, 3))
            hits = emu.level_hits
            r = run.op(7, cid, 0, 0, light() + [ground])
            stats.add(f'{mode}: level-edge entries (casters)', emu.level_hits - hits)
            stats.add(f'{mode}: draws returning {r}')
            if r == 0:
                shaded = 0
                for c in parse_dump(port_dump())[1]:
                    if struct.unpack_from('<i', c[0])[0] == cid:
                        shaded = sum(c[4][i:i + 2] != b'\xff\xff' for i in range(0, len(c[4]), 2))
                stats.add(f'{mode}: images drawn', 1)
                stats.add(f'{mode}: images with a shadow', int(shaded > 0))
                stats.add(f'{mode}: shaded pixels', shaded)
            run.op(8, cid)
    for cid in rng.sample(ids, len(ids)):
        if rng.random() < 0.8:
            run.op(9, cid)
        if rng.random() < 0.8:
            run.op(10, cid)
    stats.add(f'{mode}: runs')


def part_casters(emu, data, rng, runs, stats):
    suns = real_suns()
    stats.add('world suns', len(suns))
    trees = [(name, start) for name, start, length in ml.entries(data) if name.endswith('.flt')]
    stats.add('trees', len(trees))
    # The port prints "Unrecognized node rendering type" for the bad-class
    # nodes, as the original does: silenced here.
    sys.stderr.flush()
    saved = os.dup(2)
    os.dup2(os.open(os.devnull, os.O_WRONLY), 2)
    try:
        for mode in ('game', 'fix', 'x53', 'full'):
            for name, start in trees:
                for _ in range(runs):
                    one_run(emu, ml.MECHLIB, start, name, mode, rng, suns, stats)
    finally:
        os.dup2(saved, 2)


# ---------------------------------------------------------------- part 3: the upkeep
UPKEEP = 0x4814e0


def upkeep_case(rng, suns, case, cw):
    """One run of 0x4814e0 in a random world of objects, on both sides; the
    traces, the result's writes (mech +0xcc) and the shades (for the stats)."""
    from calltrace import Emu as CallEmu, Replay, Trace, compare
    answers = {}
    trace = Trace(script=answers, seed=case)
    e = CallEmu(trace)
    cpu = e.cpu
    cpu.fpu_control(cw)
    cpu.call(SET_FORMAT, ecx=5, edx=6, stack_args=(5, 0, 0, 0))
    wf = lambda a, v: cpu.write(a, fb(v))  # noqa: E731

    def vt(slot):
        t = e.world_alloc(0x100)
        e.w32(t + slot, e.new_stub(f'vt+{slot:#x}', 'c', 0))
        return t

    sun = 0
    if rng.random() < 0.93:
        sun = e.world_alloc(16)
        if rng.random() < 0.97:
            node, light = e.world_alloc(0x40), e.world_alloc(0xd0)
            e.w32(sun + 4, node)
            e.w32(node + 0x38, light)
            if suns and rng.random() < 0.7:
                _, d, diffuse, ambient = rng.choice(suns)
            else:
                d = [rng.uniform(-1, 1), rng.choice([rng.uniform(-1, 0), 0.0, 0.4]), rng.uniform(-1, 1)]
                diffuse, ambient = rng.choice([0.0, 1.0, rng.uniform(0, 2)]), rng.choice([0.0, rng.uniform(0, 1)])
            cpu.write(light + 0x84, fb(*d))
            wf(light + 0x9c, diffuse)
            wf(light + 0xa0, ambient)
    e.w32(0x6098d4, sun)
    pilot, player = e.world_alloc(0x40), e.world_alloc(0x500)
    e.w32(0x60861c, pilot)
    e.w32(pilot + 0x14, player)
    centre = [rng.uniform(-500, 500), rng.uniform(0, 50), rng.uniform(-500, 500)]
    positions = {}

    def place(ent, near):
        p = e.world_alloc(12)
        r = rng.choice([0, 30, 50, 70, 99.9, 100, 100.1, 150, 400]) * rng.uniform(0.9, 1.1)
        a = rng.uniform(0, 2 * math.pi)
        cpu.write(p, fb(near[0] + r * math.cos(a), near[1] + rng.uniform(-10, 10), near[2] + r * math.sin(a)))
        positions[ent] = p
    place(player, centre)
    n = rng.choice([0, 1, 2, 3, 5, 8])
    lst = e.world_alloc(4 * max(n, 1))
    e.w32(0x73655c, lst)
    e.w32(0x736560, lst + 4 * n)
    objects, controllers = {}, {}
    vt2c, vt50 = vt(0x2c), vt(0x50)
    for i in range(n):
        obj, mech = e.world_alloc(16), e.world_alloc(0x100)
        e.w32(obj, vt2c)
        e.w32(mech, vt50)
        e.w32(lst + 4 * i, obj)
        objects[obj] = mech
        e.w32(mech + 4, 0x6600 + 0x10 * i)
        if rng.random() < 0.3:
            e.w32(mech + 0xcc, 0x7000 + 0x10 * i)
        place(mech, centre)
        r = rng.random()
        if r < 0.3:
            ctrl = player
        elif r < 0.7:
            ctrl = e.world_alloc(0x500)
        else:
            ctrl = 0
        if ctrl:
            ground = e.world_alloc(0x200)
            e.w32(ctrl + 0x41c, ground)
            wf(ground + 0x13c, rng.uniform(-20, 60))
        controllers[mech] = ctrl
    created = []
    answers.update({
        'vt+0x2c': lambda k, a: objects[a[0]],
        'vt+0x50': lambda k, a: controllers[a[0]],
        '0x005a85e0': lambda k, a: positions[a[0]],
        '0x00526a80': lambda k, a: int(rng.random() < 0.92),
        '0x00543ac0': lambda k, a: struct.unpack('<I', fb(rng.choice([1.0, rng.uniform(0.2, 1.3)])))[0],
        '0x00542c20': lambda k, a: created.append(0x7800 + 0x10 * k) or created[-1],
        '0x00542e70': lambda k, a: rng.randint(0, 1),
    })
    trace.script = answers  # (an empty dict given to Trace is replaced)
    slot = e.alloc(4)
    thunk = e.thunk(b'\xd9\x05' + struct.pack('<I', slot) + b'\xc2\x08\x00')  # fld [slot]; ret 8

    def clouds(emu, key, a, pop):
        cpu.u32(slot, emu.t.handle(key, a))
        emu._ret(0, pop, then=thunk)
    e.stub(0x526a80, spec='')
    e.stub(0x5a85e0, spec='c', pop=0)
    e.stub(0x543ac0, spec='s0,s1', pop=8, fn=clouds)
    e.stub(NEW, spec='', pop=0)
    for addr, spec, pop in ((SIZE, 'c,d,s0', 4), (NODE, 'c,d', 0), (ENABLE, 'c,d', 0), (COLOR, 'c,d', 0),
                            (POSITION, 'c,d', 0), (DRAW, 'c,d,s0', 4), (UPLOAD, 'c', 0), (RELEASE, 'c', 0),
                            (DELETE, 'c', 0)):
        e.stub(addr, spec=spec, pop=pop)
    snap = e.snapshot()
    e.watch_writes([(e.world_base, e.world_next)])
    e.call(UPKEEP)
    want = list(trace.log)
    rep = Replay(trace, e, snap)
    probe.probe_app_set_callback(rep.callback())
    probe.probe_si_upkeep()
    got = rep.log
    same = compare(f'upkeep case {case}', want, got)
    written = {a: cpu.read(a, 1)[0] for a in e.writes}
    pokes = {a: v for a, v in rep.shadow.items() if a in written or a >= e.world_base}
    same_writes = all(rep.shadow.get(a) == v for a, v in written.items()) and set(pokes) <= set(written)
    shades = [x[1][1] for x in want if x[0] == '0x00542e00']
    return same, same_writes, want, shades


def part_upkeep(rng, count, stats):
    suns = real_suns()
    probe.probe_app_set_callback.argtypes = [ctypes.c_void_p]
    from calltrace import CALLBACK
    probe.probe_app_set_callback.argtypes = [CALLBACK]
    for label, cw in (('game', 0x7f), ('x53', 0x27f), ('full', 0x37f)):
        bad = 0
        for case in range(count):
            same, same_writes, want, shades = upkeep_case(rng, suns, case, cw)
            if not same_writes:
                raise Fail(f'upkeep {label} case {case}: mech +0xcc writes differ')
            if not same:
                if label == 'game':
                    raise Fail(f'upkeep {label} case {case}: traces differ')
                bad += 1
            stats.add(f'upkeep {label}: calls', len(want))
            for k in {x[0] for x in want}:
                stats.add(f'upkeep {label}: cases calling {k}')
            stats.add(f'upkeep {label}: shades not white', sum(s != 0xffff for s in shades))
        stats.add(f'upkeep {label}: cases (traces differing: {bad})', count)


# ---------------------------------------------------------------- main
def main():
    args = sys.argv[1:]
    part = None
    if '--part' in args:
        i = args.index('--part')
        part = int(args[i + 1])
        del args[i:i + 2]
    runs = int(args[0]) if args else 3
    rng = random.Random(0x5ad0)
    data = ml.MECHLIB.read_bytes()
    emu = ShadowEmu(data)
    stats = Stats()
    try:
        if part in (None, 1):
            probe.probe_si_init()
            part_fill(emu, rng, 1500, stats)
        if part in (None, 2):
            part_casters(emu, data, rng, runs, stats)
        if part in (None, 3):
            part_upkeep(rng, 400, stats)
    except Fail as e:
        print('FAIL', e)
        sys.exit(1)
    for k, v in sorted(stats.d.items()):
        print(f'  {k}: {v}')
    print('ok')


if __name__ == '__main__':
    main()
