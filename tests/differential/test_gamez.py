"""The ported world-file readers against the original x86, on every mission's
gamez.zbd (docs/gamez.md): the texture directory (0x518160), the
materials (0x5363d0, with the display's colour packing 0x5792c0/0x5793c0 set
to 5-6-5 and 5-5-5), the models (0x538e30, 0x538a70) and the node pool
(0x4ff200, with the class lists and the fog the world node pushes).
A path as the first argument saves a node mismatch's two dumps there.

    cmake --build build && .venv/bin/python tests/differential/test_gamez.py
"""
import ctypes
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import FTOL, IAT_FTOL, ROOT, Original  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
probe.probe_materials.argtypes = [ctypes.c_char_p, ctypes.c_long, ctypes.c_int, ctypes.c_int,
                                  ctypes.c_int, ctypes.c_void_p, ctypes.c_int]
probe.probe_pack_rgb.argtypes = [ctypes.c_int] * 3 + [ctypes.c_float] * 3
PACK_RGB = 0x5793c0
probe.probe_models.argtypes = [ctypes.c_char_p, ctypes.c_long, ctypes.c_long, ctypes.c_void_p, ctypes.c_int]
MODELS, POOL, POOL_SIZE, POOL_COUNT, POOL_FREE = 0x538e30, 0x7402e4, 0x7402e0, 0x7402e8, 0x7402ec
probe.probe_nodes.argtypes = [ctypes.c_char_p, ctypes.c_long, ctypes.c_long, ctypes.c_long, ctypes.c_int,
                              ctypes.c_int, ctypes.c_void_p, ctypes.c_int]
NODES, ZERR, READ_ZBD = 0x4ff200, 0x49bfd0, 0x4ff520
IAT_FOPEN, IAT_FSEEK, IAT_FCLOSE = 0x5bb628, 0x5bb59c, 0x5bb630
probe.probe_read_zbd.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_void_p, ctypes.c_int]
NODE_BLOCKS, NODE_NBLOCKS, NODE_CAP, NODE_LIVE, NODE_FREE = 0x60bf38, 0x60bf30, 0x60bf34, 0x60bf3c, 0x5ea834
LIST_HEADS, LIST_TAILS, ITEM_CHUNKS, ITEM_NCHUNKS, ITEM_FREE = 0x5ea6d0, 0x5ea710, 0x60bf18, 0x60bf14, 0x60bf0c
FOG, FOG_COLOR, CLEAR_COLOR, SOFT_FORMAT = 0x74a22c, 0x800548, 0x802794, 0x74a740
probe.probe_texture_directory.argtypes = [ctypes.c_char_p, ctypes.c_long, ctypes.c_int,
                                          ctypes.c_int, ctypes.c_void_p, ctypes.c_int]
ZBD = ROOT / 'game' / 'mw3-1.2' / 'zbd'
WORLDS = ['c1', 'c2', 'c3', 'c4', 'c4b', 't1']
TEXDIR, TABLE, COUNT, GROUP = 0x518160, 0x610484, 0x610480, 0x6384d4
IAT_FREAD = 0x5bb594
MATERIALS, SET_FORMAT = 0x5363d0, 0x5792c0
IAT = {'malloc': 0x5bb664, 'calloc': 0x5bb60c, 'realloc': 0x5bb610, 'free': 0x5bb65c}
BIG = 0x40000000
BLOCKS, CAPACITY, COUNT_M, FREE_HEAD, USED_HEAD = 0x63fbc8, 0x63fbc4, 0x63fbcc, 0x5edd54, 0x5edd58
HANDLE = 0x44
SCRATCH = Path(sys.argv[1]) if len(sys.argv) > 1 else None  # where to dump a mismatch


class Emulated:
    def __init__(self):
        self.cpu = Original()
        cpu = self.cpu
        cpu.stub_import(IAT_FREAD, self.fread)
        cpu.import_code(IAT_FTOL, FTOL)
        cpu.uc.mem_map(BIG, 0x4000000)
        self.next, self.sizes = BIG, {}
        cpu.stub_import(IAT['malloc'], lambda c: self.alloc(c.arg(0)))
        cpu.stub_import(IAT['calloc'], lambda c: self.calloc(c.arg(0) * c.arg(1)))
        cpu.stub_import(IAT['realloc'], self.realloc)
        cpu.stub_import(IAT['free'], lambda c: 0)

    def alloc(self, n):
        a = self.next
        self.next += (max(n, 1) + 0xf) & ~0xf
        self.sizes[a] = n
        return a

    def calloc(self, n):
        a = self.alloc(n)
        self.cpu.write(a, bytes(n))
        return a

    def realloc(self, c):
        old, n = c.arg(0), c.arg(1)
        a = self.alloc(n)
        if old:
            c.write(a, c.read(old, min(self.sizes.get(old, 0), n)))
        return a

    def i32(self, a):
        return struct.unpack('<i', self.cpu.read(a, 4))[0]

    def materials(self, data, offset, rb, gb, bb):
        cpu = self.cpu
        self.data, self.pos, self.next = data, offset, BIG
        cpu.call(SET_FORMAT, ecx=rb, edx=gb, stack_args=(bb, 0, 0, 0))
        for a in (BLOCKS, 0x63fbc0, CAPACITY, COUNT_M):
            cpu.u32(a, 0)
        cpu.call(MATERIALS, ecx=HANDLE)
        r = struct.unpack('<i', struct.pack('<I', cpu.uc.reg_read(UC_X86_REG_EAX)))[0]
        out = struct.pack('<iiii', r, self.i32(COUNT_M), self.i32(FREE_HEAD), self.i32(USED_HEAD))
        blocks = self.i32(BLOCKS)
        slot = lambda i: self.i32(blocks + 4 * (i // 1000)) + 0x2c * (i % 1000)
        used, i = set(), self.i32(USED_HEAD)
        while i != -1:
            used.add(i)
            i = struct.unpack('<h', cpu.read(slot(i) + 0x2a, 2))[0]
        for i in range(max(r, 0)):
            m = cpu.read(slot(i), 0x2c)
            tex = struct.unpack_from('<i', m, 0x10)[0]
            if i in used and m[1] & 1:
                p = tex & 0xffffffff
                tex = -1 if p == 0 else (p - TABLE) // 0x28
            out += m[0:0x10] + struct.pack('<i', tex) + m[0x14:0x24] + m[0x28:0x2c]
        i = self.i32(USED_HEAD)
        while i != -1:
            m = cpu.read(slot(i), 0x2c)
            if m[1] & 4:
                c = self.i32(slot(i) + 0x24)
                raw = cpu.read(c, 0x18)
                out += raw
                n = struct.unpack_from('<I', raw, 0x10)[0]
                arr = self.i32(c + 0x18)
                for k in range(n):
                    p = self.i32(arr + 4 * k) & 0xffffffff
                    out += struct.pack('<i', -1 if p == 0 else (p - TABLE) // 0x28)
            i = struct.unpack('<h', m[0x2a:0x2c])[0]
        return out

    def fread(self, c):
        buf, size, count, f = c.arg(0), c.arg(1), c.arg(2), c.arg(3)
        assert f == HANDLE
        chunk = self.data[self.pos:self.pos + size * count]
        self.pos += len(chunk)
        c.write(buf, chunk)
        return len(chunk) // size if size else 0

    def texture_directory(self, data, offset, count, group):
        cpu = self.cpu
        self.data, self.pos = data, offset
        cpu.u32(COUNT, (-7) & 0xffffffff)
        cpu.u32(GROUP, group)
        cpu.call(TEXDIR, ecx=count, edx=HANDLE)
        r = struct.unpack('<i', struct.pack('<I', cpu.uc.reg_read(UC_X86_REG_EAX)))[0]
        out = struct.pack('<ii', r, struct.unpack('<i', cpu.read(COUNT, 4))[0])
        for i in range(max(r, 0)):
            e = cpu.read(TABLE + 0x28 * i, 0x28)
            mip, = struct.unpack_from('<I', e, 0x24)
            idx = -1 if mip == 0 else (mip - TABLE) // 0x28
            out += e[8:28] + e[0x1c:0x24] + struct.pack('<i', idx)
        return out

    def models(self, data, moff, off):
        cpu, r, i32 = self.cpu, self.cpu.read, self.i32
        self.materials(data, moff, 5, 6, 5)
        for a in (POOL, POOL_SIZE, POOL_COUNT):
            cpu.u32(a, 0)
        self.data, self.pos = data, off
        cpu.call(MODELS, ecx=HANDLE)
        res = struct.unpack('<i', struct.pack('<I', cpu.uc.reg_read(UC_X86_REG_EAX)))[0]
        out = [struct.pack('<iii', res, i32(POOL_COUNT), i32(POOL_FREE))]
        blocks = i32(BLOCKS)
        mat_base = [i32(blocks + 4 * b) for b in range(64) if i32(blocks + 4 * b)]

        def mat_index(ptr):
            if ptr == 0:
                return -1
            for b, base in enumerate(mat_base):
                if base <= ptr < base + 1000 * 0x2c:
                    return b * 1000 + (ptr - base) // 0x2c
            return -2
        pool = i32(POOL)
        for i in range(max(res, 0)):
            m = pool + 0x60 * i
            h = r(m, 0x60)
            out.append(h[:0x34] + h[0x48:0x5c] + h[0x5c:0x60])
            polys, verts, norms, morphs, lights = struct.unpack_from('<5i', h, 0x10)
            for cnt, o in ((verts, 0x38), (norms, 0x3c), (morphs, 0x44)):
                if cnt > 0:
                    out.append(r(i32(m + o) & 0xffffffff, cnt * 12))
            la = i32(m + 0x40) & 0xffffffff
            for k in range(max(lights, 0)):
                l = r(la + 0x4c * k, 0x4c)
                out.append(l[:0x28] + l[0x28:0x2a] + l[0x30:0x4c])
                vc = struct.unpack_from('<i', l, 0x0c)[0]
                if vc > 0:
                    out.append(r(struct.unpack_from('<I', l, 0x2c)[0], vc * 12))
            pa = i32(m + 0x34) & 0xffffffff
            for k in range(max(polys, 0)):
                q = r(pa + 0x24 * k, 0x24)
                flags, prio, vi, ni, uv, col, _, mat, zone = struct.unpack('<IiIIIIIII', q)
                c = flags & 0xff
                out.append(struct.pack('<IiiI', flags, prio, mat_index(mat), zone))
                if c and vi:
                    out.append(r(vi, c * 4))
                if c and flags & 0x200 and ni:
                    out.append(r(ni, c * 4))
                textured = mat and r(mat + 1, 1)[0] & 1
                if textured and uv:
                    out.append(r(uv, c * 8))
                if col:
                    out.append(r(col, c * 12))
        return b''.join(out)

    def reset_world(self, free_head):
        cpu = self.cpu
        for a in (NODE_BLOCKS, NODE_NBLOCKS, NODE_CAP, NODE_LIVE, CLEAR_COLOR):
            cpu.u32(a, 0)
        cpu.u32(NODE_FREE, free_head & 0xffffffff)
        cpu.write(0x60be4c, bytes(0x60bf1c - 0x60be4c))
        cpu.write(FOG, bytes(0x38))
        cpu.write(FOG_COLOR, bytes(0xa0))
        cpu.write(SOFT_FORMAT, struct.pack('<IIIiii', 0xf800, 0x07e0, 0, 8, 3, 3))
        if not getattr(self, 'zerr_stubbed', False):
            cpu.stub_function(ZERR, lambda c: self.reports.append((c.arg(0), c.arg(2))))
            self.zerr_stubbed = True
        self.reports = []

    def report_bytes(self):
        return struct.pack('<i', len(self.reports)) + b''.join(struct.pack('<ii', *x) for x in self.reports)

    def nodes(self, data, moff, mdl_off, node_off, size, free_head):
        self.models(data, moff, mdl_off)
        self.reset_world(free_head)
        self.data, self.pos = data, node_off
        self.cpu.call(NODES, ecx=size, edx=HANDLE)
        res = struct.unpack('<i', struct.pack('<I', self.cpu.uc.reg_read(UC_X86_REG_EAX)))[0]
        return struct.pack('<i', res) + self.dump_world(size)

    def read_zbd(self, data, path, exists=True):
        """0x4ff520 on `data` as the file at `path` (bytes)."""
        cpu = self.cpu
        if not getattr(self, 'files_stubbed', False):
            cpu.stub_import(IAT_FOPEN, lambda c: HANDLE if self.open_ok else 0)
            cpu.stub_import(IAT_FSEEK, self.fseek)
            cpu.stub_import(IAT_FCLOSE, lambda c: 0)
            self.files_stubbed = True
        self.next, self.data, self.pos, self.open_ok = BIG, data, 0, exists
        for a in (BLOCKS, 0x63fbc0, CAPACITY, COUNT_M, POOL, POOL_SIZE, POOL_COUNT, COUNT):
            cpu.u32(a, 0)
        cpu.u32(GROUP, 3)
        cpu.call(SET_FORMAT, ecx=5, edx=6, stack_args=(5, 0, 0, 0))
        self.reset_world(-1)
        p = self.alloc(len(path) + 1)
        cpu.write(p, path + b'\0')
        cpu.call(READ_ZBD, ecx=p)
        res = struct.unpack('<i', struct.pack('<I', cpu.uc.reg_read(UC_X86_REG_EAX)))[0]
        out = struct.pack('<i', res) + self.report_bytes()
        out += struct.pack('<3i', self.i32(COUNT), self.i32(COUNT_M), self.i32(POOL_COUNT))
        if res == 0:
            out += self.dump_world(struct.unpack_from('<i', data, 0x18)[0])
        return out

    def fseek(self, c):
        assert c.arg(0) == HANDLE and c.arg(2) == 0
        self.pos = c.arg(1)
        return 0

    def dump_world(self, size):
        cpu, r, i32 = self.cpu, self.cpu.read, self.i32
        bs, nblocks, cap = i32(0x5ea830), i32(NODE_NBLOCKS), i32(NODE_CAP)
        blocks = [i32(i32(NODE_BLOCKS) + 4 * b) & 0xffffffff for b in range(nblocks)]
        pool = i32(POOL) & 0xffffffff

        def index(ptr):
            if ptr == 0:
                return -1
            for b, base in enumerate(blocks):
                if base <= ptr < base + bs * 0xd4:
                    return b * bs + (ptr - base) // 0xd4
            return -2

        def slot(i):
            return blocks[i // bs] + (i % bs) * 0xd4

        out = [struct.pack('<4i', cap, i32(NODE_LIVE), i32(NODE_FREE), nblocks)]

        def raw(addr, n, ptrs):
            b = bytearray(r(addr, n))
            for o in ptrs:
                b[o:o + 4] = bytes(4)
            out.append(bytes(b))

        def nodes(ptr, count):
            out.append(struct.pack('<i', ptr != 0))
            if ptr:
                out.append(b''.join(struct.pack('<i', index(v)) for v in struct.unpack(f'<{count}I', r(ptr, 4 * count))))

        for i in range(cap):
            a = slot(i)
            h = r(a, 0xd4)
            raw(a, 0xd0, (0x38, 0x3c, 0x48, 0x58, 0x60))
            out.append(h[0xd0:0xd4])
            model, action = struct.unpack_from('<II', h, 0x3c)[0], struct.unpack_from('<I', h, 0x48)[0]
            out.append(struct.pack('<ii', -1 if model == 0 else (model - pool) // 0x60, action != 0))
            if i >= size:
                continue
            cls, d, np, pp, nc, cp = struct.unpack_from('<iIiIiI', h, 0x34)[0:2] + struct.unpack_from('<iIiI', h, 0x54)
            nodes(pp, np)
            nodes(cp, nc)
            if cls == 0 or not d:
                continue
            if cls == 1:
                raw(d, 0x1e8, (0, 4, 8, 0xc))
                nodes(d, 4)
            elif cls == 2:
                raw(d, 0xbc, (0x90, 0xa4, 0xa8, 0xb0, 0xb4))
                for cnt, lp, dp in ((0xa0, 0xa4, 0xa8), (0xac, 0xb0, 0xb4)):
                    n, lst, dat = i32(d + cnt), i32(d + lp) & 0xffffffff, i32(d + dp) & 0xffffffff
                    nodes(lst, n)
                    for k in range(n if lst else 0):
                        node = i32(lst + 4 * k) & 0xffffffff
                        out.append(struct.pack('<i', i32(dat + 4 * k) == i32(node + 0x38)))
                rows, cols, rp = i32(d + 0x8c), i32(d + 0x88), i32(d + 0x90) & 0xffffffff
                for z in range(rows):
                    row = i32(rp + 4 * z) & 0xffffffff
                    for x in range(cols):
                        pa = row + 0x48 * x
                        raw(pa, 0x48, (0x3c,))
                        cnt = struct.unpack('<h', r(pa + 0x3a, 2))[0]
                        nodes(i32(pa + 0x3c) & 0xffffffff, cnt)
            elif cls in (3, 4, 5, 6):
                out.append(r(d, {3: 0xf8, 4: 0x1c, 5: 0x90, 6: 0x50}[cls]))
            elif cls == 9:
                raw(d, 0xd0, (0xcc,))
                nodes(i32(d + 0xcc) & 0xffffffff, i32(d + 0xc8))
            elif cls == 10:
                raw(d, 0x94, (0x90,))
                nodes(i32(d + 0x90) & 0xffffffff, i32(d + 0x8c))
        chunk_cnt, chunks_arr = i32(ITEM_NCHUNKS), i32(ITEM_CHUNKS) & 0xffffffff
        chunks = [i32(chunks_arr + 4 * c) & 0xffffffff for c in range(chunk_cnt)]
        cs = i32(0x5ea750)

        def item(ptr):
            for c, base in enumerate(chunks):
                if ptr and base <= ptr < base + cs * 0x10:
                    return struct.pack('<ii', c, (ptr - base) // 0x10)
            return struct.pack('<ii', -1, -1)

        for li in range(16):
            head = i32(LIST_HEADS + 4 * li) & 0xffffffff
            tail = i32(LIST_TAILS + 4 * li) & 0xffffffff
            items, p = [], i32(head) & 0xffffffff
            while p:
                items.append(p)
                p = i32(p + 8) & 0xffffffff
            out.append(struct.pack('<i', len(items)))
            for p in items:
                node, prev, _, rem = struct.unpack('<IIIi', r(p, 16))
                out.append(struct.pack('<i', index(node)) + item(p) + struct.pack('<i', rem) + item(prev))
            out.append(item(i32(tail) & 0xffffffff) + struct.pack('<i', i32(head + 8)))
        free, p = 0, i32(ITEM_FREE) & 0xffffffff
        while p:
            free += 1
            p = i32(p + 8) & 0xffffffff
        out.append(struct.pack('<i', free) + item(i32(ITEM_FREE) & 0xffffffff) + struct.pack('<i', chunk_cnt))
        out.append(r(FOG, 0x38))
        fc = r(FOG_COLOR, 0xa0)
        out.append(fc[:0x1a] + fc[0x1c:0xa0])
        out.append(r(CLEAR_COLOR, 4))
        out.append(self.report_bytes())
        return b''.join(out)


def check_pack_rgb(emu):
    import random
    rng = random.Random(7)
    cpu = emu.cpu
    p = emu.alloc(12)
    for fmt in ((5, 6, 5), (5, 5, 5), (4, 4, 4)):
        cpu.call(SET_FORMAT, ecx=fmt[0], edx=fmt[1], stack_args=(fmt[2], 0, 0, 0))
        for _ in range(3000):
            rgb = [rng.choice([rng.uniform(0, 255), float(rng.randrange(256)), rng.randrange(256) + 0.5])
                   for _ in range(3)]
            cpu.write(p, struct.pack('<3f', *rgb))
            cpu.call(PACK_RGB, ecx=p)
            want = cpu.uc.reg_read(UC_X86_REG_EAX) & 0xffff
            got = probe.probe_pack_rgb(*fmt, *rgb) & 0xffff
            if want != got:
                raise SystemExit(f'pack_rgb {fmt} {rgb}: original {want:#06x} port {got:#06x}')
    print('pack_rgb: 9000 colours identical in three display formats')


def check_read_zbd(emu, buf):
    """0x4ff520 on each whole world (long path), one short relative path, and
    broken files: bad signature, bad version, short header, header only,
    missing file, empty path."""
    import os
    tmp = ROOT / 'build' / 'test-gamez'
    tmp.mkdir(exist_ok=True)
    good = (ZBD / 't1' / 'gamez.zbd').read_bytes()
    cases = [(str(ZBD / w / 'gamez.zbd'), (ZBD / w / 'gamez.zbd').read_bytes(), True) for w in WORLDS]
    for name, data in (('signature', b'\0' + good[1:]), ('version', good[:4] + b'\x1a' + good[5:]),
                       ('short', good[:0x10]), ('header', good[:0x24])):
        (tmp / name).write_bytes(data)
        cases.append((str(tmp / name), data, True))
    cases += [(str(tmp / 'missing'), b'', False), ('', b'', False)]
    cwd = os.getcwd()
    os.chdir(ZBD.parent)
    cases.append(('zbd/c4b/gamez.zbd', (ZBD / 'c4b' / 'gamez.zbd').read_bytes(), True))
    try:
        for path, data, exists in cases:
            emu.open_ok = exists
            want = emu.read_zbd(data, path.encode(), exists)
            size = struct.unpack_from('<i', data, 0x18)[0] if len(data) >= 0x1c else 0
            n = probe.probe_read_zbd(path.encode(), size, buf, len(buf))
            got = buf.raw[:max(n, 0)]
            if got != want:
                at = next((i for i in range(min(n, len(want))) if got[i] != want[i]), None)
                raise SystemExit(f'read_zbd {path!r}: {len(want)} vs {n} bytes, first difference at {at}')
    finally:
        os.chdir(cwd)
    return f'read_zbd: {len(cases)} whole-file reads identical, failures included'


def main():
    emu = Emulated()
    check_pack_rgb(emu)
    buf = ctypes.create_string_buffer(64 << 20)
    total = mtotal = mdltotal = ntotal = 0
    for w in WORLDS:
        path = ZBD / w / 'gamez.zbd'
        data = path.read_bytes()
        count, offset = struct.unpack_from('<ii', data, 8)
        for c, g in ((count, 3), (0, 3), (0x1001, 3)):
            want = emu.texture_directory(data, offset, c, g)
            n = probe.probe_texture_directory(str(path).encode(), offset, c, g, buf, len(buf))
            if buf.raw[:n] != want:
                raise SystemExit(f'{w} count {c}: original {want[:8].hex()} port {buf.raw[:8].hex()}')
        total += count
        moff, = struct.unpack_from('<i', data, 0x10)
        for fmt in ((5, 6, 5), (5, 5, 5)):
            want = emu.materials(data, moff, *fmt)
            n = probe.probe_materials(str(path).encode(), moff, *fmt, buf, len(buf))
            got = buf.raw[:max(n, 0)]
            if n < 0 or got != want:
                at = next((i for i in range(min(n, len(want))) if got[i] != want[i]), None)
                raise SystemExit(f'{w} materials {fmt}: {len(want)} vs {n} bytes, first difference at {at}')
        mats = struct.unpack_from('<i', data, moff + 4)[0]
        mdl_off, = struct.unpack_from('<i', data, 0x14)
        want = emu.models(data, moff, mdl_off)
        n = probe.probe_models(str(path).encode(), moff, mdl_off, buf, len(buf))
        got = buf.raw[:max(n, 0)]
        if n < 0 or got != want:
            at = next((i for i in range(min(n, len(want))) if got[i] != want[i]), None)
            raise SystemExit(f'{w} models: {len(want)} vs {n} bytes, first difference at {at}')
        mdls = struct.unpack_from('<i', data, mdl_off + 4)[0]
        size, free_head, node_off = struct.unpack_from('<iii', data, 0x18)
        want = emu.nodes(data, moff, mdl_off, node_off, size, free_head)
        n = probe.probe_nodes(str(path).encode(), moff, mdl_off, node_off, size, free_head, buf, len(buf))
        got = buf.raw[:max(n, 0)]
        if n < 0 or got != want:
            at = next((i for i in range(min(n, len(want))) if got[i] != want[i]), None)
            open(SCRATCH / f'{w}_nodes_want.bin', 'wb').write(want) if SCRATCH else None
            open(SCRATCH / f'{w}_nodes_got.bin', 'wb').write(got) if SCRATCH else None
            raise SystemExit(f'{w} nodes: {len(want)} vs {n} bytes, first difference at {at}')
        mdltotal += mdls
        mtotal += mats
        live = struct.unpack_from('<i', want, 8)[0]
        ntotal += live
        print(f'{w}: {count} textures, {mats} materials, {mdls} models and {live} live nodes identical')
    print(check_read_zbd(emu, buf))
    print(f'ok: {total} texture directory entries, {mtotal} materials, {mdltotal} models and '
          f'{ntotal} live nodes (lists and fog included) across {len(WORLDS)} worlds')


if __name__ == '__main__':
    main()
