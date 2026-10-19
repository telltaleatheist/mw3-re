"""The ported model and material readers against the original x86, on every
model and material in mechlib.zbd.

    cmake --build build && .venv/bin/python tests/differential/test_mechlib.py

A Python walk of the file in the exe's read order (docs/mechlib.md) finds every
model and material; the original reader (0x4fcf20 models, 0x4fc570 materials)
runs on each in the emulator, the port on the same offset. Everything read and
the bytes consumed must match.
"""
import ctypes
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import ROOT, Original  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
for fn in (probe.probe_mechlib_model, probe.probe_mechlib_material):
    fn.argtypes = [ctypes.c_char_p, ctypes.c_long, ctypes.c_void_p, ctypes.c_int]
MECHLIB = ROOT / 'game' / 'mw3-1.2' / 'zbd' / 'mechlib.zbd'
READ_MODEL, READ_MATERIAL, MODEL_POOL, READ_TREE = 0x4fcf20, 0x4fc570, 0x538fe0, 0x4fcdc0
probe.probe_mechlib_tree.argtypes = [ctypes.c_char_p, ctypes.c_long, ctypes.c_int, ctypes.c_void_p, ctypes.c_int]
IAT_CALLOC, IAT_REALLOC, IAT_FREE, IAT_SPRINTF = 0x5bb60c, 0x5bb610, 0x5bb65c, 0x5bb658
FRAME, EXPIRY = 0x72f338, 0x73f3a8
BLOCK_SIZE, NODE_FREE, BLOCKS, NBLOCKS, TOTAL, LIVE = 0x5ea830, 0x5ea834, 0x60bf38, 0x60bf30, 0x60bf34, 0x60bf3c
ITEM_CHUNKS, ITEM_NCHUNKS, ITEM_FREE, CHUNK_SIZE = 0x60bf18, 0x60bf14, 0x60bf0c, 0x5ea750
LIST_HEADS, LIST_TAILS = 0x5ea6d0, 0x5ea710
IAT_READFILE, IAT_MALLOC = 0x5bb0c4, 0x5bb664
HANDLE = 0x66
BIG = 0x40000000


def entries(data):
    _, count = struct.unpack_from('<II', data, len(data) - 8)
    table = len(data) - 8 - count * 0x94
    for i in range(count):
        start, length = struct.unpack_from('<II', data, table + i * 0x94)
        name = data[table + i * 0x94 + 8:table + i * 0x94 + 72].split(b'\0')[0].decode()
        yield name, start, length


def walk_tree(data, pos, models):
    """Node, its object3d, its model, then its children; returns the end."""
    node = data[pos:pos + 0xd0]
    ntype, = struct.unpack_from('<I', node, 0x34)
    has_model, = struct.unpack_from('<I', node, 0x3c)
    children, = struct.unpack_from('<I', node, 0x5c)
    pos += 0xd0
    if ntype == 5:
        pos += 0x90
    if has_model:
        models.append(pos)
        polys, verts, norms, morphs = struct.unpack_from('<4I', data, pos + 0x10)
        p = pos + 0x5c + 12 * (verts + norms + morphs)
        table = p
        p += 0x24 * polys
        for i in range(polys):
            flags, _, _, nptr, uptr, cptr = struct.unpack_from('<IiIIII', data, table + 0x24 * i)
            k = flags & 0xff
            p += 4 * k
            if flags & 0x200 and nptr:
                p += 4 * k
            if uptr:
                p += 8 * k
            if cptr:
                p += 12 * k
        pos = p
    for _ in range(children):
        pos = walk_tree(data, pos, models)
    return pos


class Emulated:
    def __init__(self, data):
        cpu = self.cpu = Original()
        cpu.uc.mem_map(BIG, 0x4000000)
        self.data = data
        cpu.stub_import(IAT_READFILE, self.read_file, pop_bytes=20)
        cpu.stub_import(IAT_MALLOC, lambda c: self.alloc(c.arg(0)))
        cpu.stub_function(MODEL_POOL, self.pool)
        self.sizes = {}
        cpu.stub_import(IAT_CALLOC, lambda c: self.zeroed(c.arg(0) * c.arg(1)))
        cpu.stub_import(IAT_REALLOC, self.realloc)
        cpu.stub_import(IAT_FREE, lambda c: 0)
        cpu.stub_import(IAT_SPRINTF, self.sprintf)

    def alloc(self, n):
        a = self.next
        self.next += (max(n, 1) + 0xf) & ~0xf
        if hasattr(self, 'sizes'):
            self.sizes[a] = n
        return a

    def zeroed(self, n):
        a = self.alloc(n)
        self.cpu.write(a, bytes(n))
        return a

    def realloc(self, c):
        old, n = c.arg(0), c.arg(1)
        a = self.alloc(n)
        if old:
            c.write(a, c.read(old, min(self.sizes.get(old, 0), n)))
        return a

    def sprintf(self, c):
        s = self.cstr(c.arg(2))
        c.write(c.arg(0), s + b'\0')
        return len(s)

    def cstr(self, p):
        out = bytearray()
        while (b := self.cpu.read(p + len(out), 1)) != b'\0':
            out += b
        return bytes(out)

    def tree(self, offset, frame):
        cpu, u = self.cpu, self.u32
        self.next, self.pos = BIG, offset
        for a in (BLOCKS, NBLOCKS, TOTAL, LIVE, ITEM_CHUNKS, ITEM_NCHUNKS, ITEM_FREE, EXPIRY):
            cpu.u32(a, 0)
        cpu.write(0x60be4c, bytes(0x60bf0c - 0x60be4c))
        cpu.u32(NODE_FREE, 0xffffffff)
        cpu.u32(FRAME, frame)
        lib = self.zeroed(0x40)
        cpu.call(READ_TREE, ecx=lib, stack_args=(HANDLE, 0))
        root = cpu.uc.reg_read(UC_X86_REG_EAX)
        bs, cs = self.i32(BLOCK_SIZE), self.i32(CHUNK_SIZE)
        blocks = [u(u(BLOCKS) + 4 * b) for b in range(self.i32(NBLOCKS))]
        chunks = [u(u(ITEM_CHUNKS) + 4 * c) for c in range(self.i32(ITEM_NCHUNKS))]

        def index(p):
            for b, base in enumerate(blocks):
                if base <= p < base + bs * 0xd4:
                    return b * bs + (p - base) // 0xd4
            return -1 if p == 0 else -2

        def item(p):
            for c, base in enumerate(chunks):
                if p and base <= p < base + cs * 0x10:
                    return struct.pack('<ii', c, (p - base) // 0x10)
            return struct.pack('<ii', -1, -1)

        out = [struct.pack('<iiii', self.pos - offset, index(root), self.i32(lib + 0x3c), self.i32(EXPIRY)),
               struct.pack('<iii', self.i32(TOTAL), self.i32(LIVE), self.i32(NODE_FREE))]
        for k in range(self.i32(TOTAL)):
            a = blocks[k // bs] + (k % bs) * 0xd4
            raw = bytearray(cpu.read(a, 0xd4))
            cls, flags = struct.unpack_from('<i', raw, 0x34)[0], struct.unpack_from('<I', raw, 0x24)[0]
            if cls == 0 and flags == 0:
                continue
            data, model, parents, children, chg = (struct.unpack_from('<I', raw, o)[0] for o in (0x38, 0x3c, 0x58, 0x60, 0xcc))
            np, nc = struct.unpack_from('<i', raw, 0x54)[0], struct.unpack_from('<i', raw, 0x5c)[0]
            for o in (0x38, 0x3c, 0x58, 0x60, 0xcc):
                raw[o:o + 4] = bytes(4)
            out.append(struct.pack('<i', k) + bytes(raw) + bytes([model != 0]))
            out += [struct.pack('<i', index(u(parents + 4 * j))) for j in range(np)]
            out += [struct.pack('<i', index(u(children + 4 * j))) for j in range(nc)]
            out.append(item(chg))
            if cls == 5 and data:
                out.append(cpu.read(data, 0x90))
        for li in (6, 7):
            head, tail = u(LIST_HEADS + 4 * li), u(LIST_TAILS + 4 * li)
            items, p = [], u(head)
            while p:
                items.append(p)
                p = u(p + 8)
            out.append(struct.pack('<i', len(items)))
            for p in items:
                out.append(struct.pack('<i', index(u(p))) + item(p) + struct.pack('<i', self.i32(p + 0xc)))
            out.append(item(u(tail)))
        return b''.join(out)

    def i32(self, a):
        return struct.unpack('<i', self.cpu.read(a, 4))[0]

    def pool(self, c):
        a = self.alloc(0x60)
        c.write(a, bytes(0x60))
        return a

    def read_file(self, c):
        h, buf, n, out = c.arg(0), c.arg(1), c.arg(2), c.arg(3)
        assert h == HANDLE
        chunk = self.data[self.pos:self.pos + n]
        self.pos += len(chunk)
        c.write(buf, chunk)
        if out:
            c.u32(out, len(chunk))
        return 1

    def u32(self, a):
        return struct.unpack('<I', self.cpu.read(a, 4))[0]

    def model(self, offset):
        cpu, r = self.cpu, self.cpu.read
        self.next, self.pos = BIG, offset
        node = self.alloc(0xd0)
        cpu.write(node, bytes(0xd0))
        cpu.u32(node + 0x3c, 1)
        cpu.call(READ_MODEL, stack_args=(HANDLE, node))
        m = self.u32(node + 0x3c)
        out = struct.pack('<i', self.pos - offset) + r(m, 0x34) + r(m + 0x48, 0x5c - 0x48)
        polys, verts, norms, morphs = struct.unpack('<4I', r(m + 0x10, 16))
        for count, ptr in ((verts, 0x38), (norms, 0x3c), (morphs, 0x44)):
            if count:
                out += r(self.u32(m + ptr), count * 12)
        table = self.u32(m + 0x34)
        for i in range(polys):
            p = table + 0x24 * i
            flags, prio, vi, ni, uv, col = struct.unpack('<IiIIII', r(p, 24))
            mat, zone = struct.unpack('<II', r(p + 0x1c, 8))
            k = flags & 0xff
            out += struct.pack('<IiII', flags, prio, mat, zone)
            out += bytes([vi != 0 and k != 0, ni != 0 and bool(flags & 0x200),
                          uv != 0, col != 0])
            if vi and k:
                out += r(vi, 4 * k)
            if ni and flags & 0x200:
                out += r(ni, 4 * k)
            if uv:
                out += r(uv, 8 * k)
            if col:
                out += r(col, 12 * k)
        return out

    def material(self, offset):
        cpu = self.cpu
        self.next, self.pos = BIG, offset
        obj = self.alloc(0xc)
        cpu.write(obj, bytes(0xc))
        cpu.call(READ_MATERIAL, ecx=obj, stack_args=(HANDLE,))
        raw, name = self.u32(obj), self.u32(obj + 8)
        out = struct.pack('<i', self.pos - offset) + cpu.read(raw, 0x28)
        if name:
            s = bytearray()
            while True:
                b = cpu.read(name + len(s), 1)
                s += b
                if b == b'\0':
                    break
            out += bytes(s)
        return out


def main():
    data = MECHLIB.read_bytes()
    emu = Emulated(data)
    buf = ctypes.create_string_buffer(8 << 20)
    models = materials = trees = nodes = 0
    for name, start, length in entries(data):
        if name == 'materials':
            count, = struct.unpack_from('<I', data, start)
            pos = start + 4
            for i in range(count):
                want = emu.material(pos)
                n = probe.probe_mechlib_material(str(MECHLIB).encode(), pos, buf, len(buf))
                if buf.raw[:n] != want:
                    raise SystemExit(f'material {i}: original {want[:16]!r} port {buf.raw[:16]!r}')
                pos += struct.unpack_from('<i', want)[0]
                materials += 1
            if pos != start + length:
                raise SystemExit(f'materials end at {pos - start} of {length}')
        elif name.endswith('.flt'):
            offsets = []
            end = walk_tree(data, start, offsets)
            if end != start + length:
                raise SystemExit(f'{name}: walk ends at {end - start} of {length}')
            for frame in (0, 300):
                want = emu.tree(start, frame)
                n = probe.probe_mechlib_tree(str(MECHLIB).encode(), start, frame, buf, len(buf))
                got = buf.raw[:max(n, 0)]
                if got != want:
                    at = next((i for i in range(min(n, len(want))) if got[i] != want[i]), None)
                    raise SystemExit(f'{name} tree (frame {frame}): original {len(want)} bytes, port {n}, '
                                     f'first difference at {at}')
            trees += 1
            nodes += struct.unpack_from('<i', want, 20)[0]
            for off in offsets:
                want = emu.model(off)
                n = probe.probe_mechlib_model(str(MECHLIB).encode(), off, buf, len(buf))
                if n < 0 or buf.raw[:n] != want:
                    raise SystemExit(f'{name} model at {off - start}: original {len(want)} port {n}')
                models += 1
    print(f'ok: {models} models and {materials} materials identical; {trees} node trees ({nodes} nodes) '
          f'identical, lists included')


if __name__ == '__main__':
    main()
