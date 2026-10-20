"""mechlib's trees loaded, resolved and copied, against the original x86
(docs/mechlib.md, "Loading a tree" and "Copying a tree").

    cmake --build build && .venv/bin/python -P tests/differential/test_tree_copy.py

A mission world is read first by both sides (its texture, material and
model tables, as when mechs are built). The library's 442 materials are
read (0x4fc470); then each of the 57 trees is read (0x4fcdc0), its
materials resolved (0x4fcd40: the texture by name 0x518c60, the world's
equal material or a new one 0x536ad0) and copied in four ways: as a mech
copies its model (0x4f9af0(node, 1, 1): every model and its materials
copied), shared models (0x4f9af0(node, 0, 0)), models copied with shared
materials (0x4f9af0(node, 1, 0)) and own materials only where they change
(0x4f9b50(node, 1, 1)); then some copies and roots are taken down again
(0x4f9050, 0x4f84b0). After every call: its result, the node pool with
the lists and items, every model a node holds (its bytes, arrays,
polygons with their materials by table index), the material table (every
used material and the free list) and the texture table.

TREE_COPY_WORLD picks the world (t1 by default; c1 has the most materials).
"""
import ctypes
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_destroy as D  # noqa: E402
import test_gamez as G  # noqa: E402
import test_mechlib as L  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

probe = G.probe
probe.probe_copy_world.argtypes = [ctypes.c_char_p]
probe.probe_copy_lib.argtypes = [ctypes.c_char_p]
probe.probe_copy_load.argtypes = [ctypes.c_char_p]
probe.probe_copy.argtypes = [ctypes.c_int] * 4
probe.probe_copy_dump.argtypes = [ctypes.c_void_p, ctypes.c_int]

IAT_READFILE, IAT_STRCHR, IAT_STRRCHR = 0x5bb0c4, 0x5bb63c, 0x5bb5a8
LIB_HANDLE = 0x66
MAT_BLOCKS, MAT_NBLOCKS, MAT_CAP, MAT_COUNT, MAT_FREE, MAT_USED, MAT_LAST = (
    0x63fbc8, 0x63fbc0, 0x63fbc4, 0x63fbcc, 0x5edd54, 0x5edd58, 0x63fbd0)
TEX_TABLE, TEX_COUNT = 0x610484, 0x610480
WORLD = os.environ.get('TREE_COPY_WORLD', 't1')
COPIES = [(0, 1, 1, 'mech copy'), (0, 0, 0, 'shared models'), (0, 1, 0, 'models, shared materials'),
          (1, 1, 1, 'own changing materials')]


class Emulated(D.Emulated):
    def __init__(self):
        super().__init__()
        cpu = self.cpu
        self.lib_data = L.MECHLIB.read_bytes()
        cpu.stub_import(IAT_READFILE, self.read_file, pop_bytes=20)
        cpu.stub_import(IAT_STRCHR, lambda c: self.strchr(c, False))
        cpu.stub_import(IAT_STRRCHR, lambda c: self.strchr(c, True))

    def read_file(self, c):
        h, buf, n, out = c.arg(0), c.arg(1), c.arg(2), c.arg(3)
        assert h == LIB_HANDLE
        chunk = self.lib_data[self.lpos:self.lpos + n]
        self.lpos += len(chunk)
        c.write(buf, chunk)
        if out:
            c.u32(out, len(chunk))
        return 1

    def strchr(self, c, last):
        p, ch = c.arg(0), c.arg(1) & 0xff
        s = self.cstr(p)
        i = s.rfind(bytes([ch])) if last else s.find(bytes([ch]))
        return p + i if i >= 0 else 0

    def open_lib(self):
        """The library object with its materials read (0x4fc9e0's loop of 0x4fc470)."""
        cpu = self.cpu
        start = next(s for n, s, _ in L.entries(self.lib_data) if n == 'materials')
        count, = struct.unpack_from('<I', self.lib_data, start)
        self.lpos = start + 4
        self.lib = self.calloc(0x40)
        mats = self.calloc(4 * count)
        for i in range(count):
            m = self.calloc(0xc)
            cpu.call(0x4fc470, ecx=m, stack_args=(LIB_HANDLE,))
            cpu.u32(mats + 4 * i, m)
        cpu.u32(self.lib + 0x20, mats)
        cpu.u32(self.lib + 0x24, mats + 4 * count)
        cpu.u32(self.lib + 0x28, mats + 4 * count)
        self.offsets = {n: s for n, s, _ in L.entries(self.lib_data)}
        return 1

    def load_tree(self, name):
        """0x4fc8f0 without its list: the tree read, then its materials resolved."""
        self.lpos = self.offsets[name]
        self.cpu.call(0x4fcdc0, ecx=self.lib, stack_args=(LIB_HANDLE, 0))
        root = self.cpu.uc.reg_read(UC_X86_REG_EAX)
        self.cpu.call(0x4fcd40, ecx=self.lib, stack_args=(root,))
        return self.index(root)

    def copy(self, root, mode, a, b):
        cpu, n = self.cpu, self.node(root)
        if mode == 0:
            cpu.call(0x4f9af0, ecx=n, edx=a, stack_args=(b,))
        elif mode == 1:
            cpu.call(0x4f9b50, ecx=n, stack_args=(a, b))
        elif mode == 2:
            cpu.call(0x4f9050, ecx=n)
            return self.ret()
        else:
            cpu.call(0x4f84b0, ecx=n)
            return self.ret()
        return self.index(cpu.uc.reg_read(UC_X86_REG_EAX))

    def material_index(self, p):
        bs = self.i32(0x5edd50)
        arr = self.i32(MAT_BLOCKS) & 0xffffffff
        for b in range(self.i32(MAT_NBLOCKS)):
            base = self.i32(arr + 4 * b) & 0xffffffff
            if base <= p < base + bs * 0x2c:
                return b * bs + (p - base) // 0x2c
        return -1

    def material(self, i):
        bs = self.i32(0x5edd50)
        return (self.i32((self.i32(MAT_BLOCKS) & 0xffffffff) + 4 * (i // bs)) & 0xffffffff) + (i % bs) * 0x2c

    def put_model(self, out, m):
        r, i32 = self.cpu.read, self.i32
        h = bytearray(r(m, 0x5c))
        for o in (0x34, 0x38, 0x3c, 0x40, 0x44):
            h[o:o + 4] = bytes(4)
        out.append(bytes(h))
        n = lambda off: struct.unpack_from('<i', r(m, 0x5c), off)[0]  # noqa: E731
        for cnt, ptr in ((0x14, 0x38), (0x18, 0x3c), (0x1c, 0x44)):
            p = i32(m + ptr) & 0xffffffff
            if p and n(cnt) > 0:
                out.append(r(p, n(cnt) * 12))
        lights = i32(m + 0x40) & 0xffffffff
        for k in range(n(0x20) if lights else 0):
            l = bytearray(r(lights + 0x4c * k, 0x4c))
            vc, vp = struct.unpack_from('<iI', l, 0xc)[0], struct.unpack_from('<I', l, 0x2c)[0]
            out.append(bytes(l[:0x2a]) + bytes(l[0x30:0x4c]))  # the packed colour, not its pointer
            if vc > 0:
                out.append(r(vp, vc * 12))
        polys = i32(m + 0x34) & 0xffffffff
        for k in range(n(0x10) if polys else 0):
            p = polys + 0x24 * k
            flags, prio, vi, ni, uv, col, scr, mat, zone = struct.unpack('<IiIIIIIII', r(p, 0x24))
            c = flags & 0xff
            out.append(struct.pack('<IiiI', flags, prio, self.material_index(mat) if mat else -1, zone))
            out.append(bytes([vi != 0, ni != 0, uv != 0, col != 0, scr != 0]))
            for ptr, size in ((vi, 4), (ni, 4), (uv, 8), (col, 12)):
                if ptr:
                    out.append(r(ptr, c * size))

    def dump(self, _pool=1):
        i32, r = self.i32, self.cpu.read
        out = [self.dump_world(i32(G.NODE_CAP), items=True)]
        out.append(struct.pack('<ii', i32(G.POOL_COUNT), i32(G.POOL_FREE)))
        pool = i32(G.POOL) & 0xffffffff
        seen = set()
        for k in range(i32(G.NODE_CAP)):
            m = i32(self.node(k) + 0x3c) & 0xffffffff
            if not m or m in seen:
                continue
            seen.add(m)
            out.append(struct.pack('<i', (m - pool) // 0x60))
            self.put_model(out, m)
        # The material table.
        last = i32(MAT_LAST) & 0xffffffff
        out.append(struct.pack('<5i', i32(MAT_CAP), i32(MAT_COUNT), i32(MAT_FREE), i32(MAT_USED),
                               self.material_index(last) if last else -1))
        free, i = 0, i32(MAT_FREE)
        while i >= 0:
            free += 1
            i = struct.unpack('<h', r(self.material(i) + 0x2a, 2))[0]
        out.append(struct.pack('<i', free))
        i = i32(MAT_USED)
        while i >= 0:
            raw = r(self.material(i), 0x2c)
            flags = raw[1]
            tex, = struct.unpack_from('<I', raw, 0x10)
            if flags & 1:
                tex = (tex - TEX_TABLE) // 0x28 if tex else -1
            out.append(struct.pack('<i', i) + raw[:0x10] + struct.pack('<i', s32(tex)) + raw[0x14:0x24] +
                       struct.pack('<i', struct.unpack_from('<I', raw, 0x24)[0] != 0) + raw[0x28:0x2c])
            i = struct.unpack_from('<h', raw, 0x2a)[0]
        # The texture table.
        count = i32(TEX_COUNT)
        out.append(struct.pack('<i', count))
        for k in range(count):
            e = r(TEX_TABLE + 0x28 * k, 0x28)
            out.append(e[8:0x1c] + e[0x1c:0x24])
        return b''.join(out)


def s32(v):
    return struct.unpack('<i', struct.pack('<I', v & 0xffffffff))[0]


def main():
    emu = Emulated()
    buf = ctypes.create_string_buffer(96 << 20)
    path = G.ZBD / WORLD / 'gamez.zbd'
    want = emu.load(path)
    got = probe.probe_copy_world(str(path).encode())
    if want != got:
        raise SystemExit(f'{WORLD}: read_zbd {want} vs {got}')
    if emu.open_lib() != probe.probe_copy_lib(str(L.MECHLIB).encode()):
        raise SystemExit('mechlib open differs')
    D.compare('after the library opened', emu.dump(), buf, probe.probe_copy_dump(buf, len(buf)))
    names = [n for n, _, _ in L.entries(emu.lib_data) if n.endswith('.flt')]
    names = names[:int(os.environ.get('TREE_COPY_LIMIT', len(names)))]  # a break test runs a few
    calls = trees = 0
    for name in names:
        want = emu.load_tree(name)
        got = probe.probe_copy_load(name.encode())
        if want != got:
            raise SystemExit(f'{name}: loaded at {want} vs {got}')
        D.compare(f'{name} loaded', emu.dump(), buf, probe.probe_copy_dump(buf, len(buf)))
        root = want
        copies = []
        for mode, a, b, what in COPIES:
            want = emu.copy(root, mode, a, b)
            got = probe.probe_copy(root, mode, a, b)
            if want != got:
                raise SystemExit(f'{name} {what}: original {want}, port {got}')
            D.compare(f'{name} {what}', emu.dump(), buf, probe.probe_copy_dump(buf, len(buf)))
            copies.append(want)
            calls += 1
        # Taken down again: the shared copy and the mech copy as a tree, the
        # root with the list delete (its children stay).
        for node, mode in ((copies[1], 2), (copies[0], 2), (root, 3)):
            want = emu.copy(node, mode, 0, 0)
            got = probe.probe_copy(node, mode, 0, 0)
            if want != got:
                raise SystemExit(f'{name} takedown {mode} of {node}: original {want}, port {got}')
            D.compare(f'{name} takedown {mode}', emu.dump(), buf, probe.probe_copy_dump(buf, len(buf)))
            calls += 1
        trees += 1
    print(f'ok: {trees} trees loaded and resolved on {WORLD}, {calls} copies and takedowns identical '
          f'(nodes, lists, models, materials {emu.i32(MAT_COUNT)}, textures {emu.i32(TEX_COUNT)})')


if __name__ == '__main__':
    try:
        main()
    except SystemExit as e:
        print(e)
        sys.stdout.flush()
        os._exit(1 if e.code else 0)
    sys.stdout.flush()
    os._exit(0)
