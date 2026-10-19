"""The ported world-file readers against the original x86, on every mission's
gamez.zbd (docs/gamez.md). So far: the texture directory (0x518160) and the
materials (0x5363d0, with the display's colour packing 0x5792c0/0x5793c0 set
to 5-6-5 and 5-5-5).

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


def main():
    emu = Emulated()
    buf = ctypes.create_string_buffer(1 << 20)
    total = mtotal = 0
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
            if n < 0 or buf.raw[:n] != want:
                at = next((i for i in range(min(n, len(want))) if buf.raw[i] != want[i]), None)
                raise SystemExit(f'{w} materials {fmt}: {len(want)} vs {n} bytes, first difference at {at}')
        mats = struct.unpack_from('<i', data, moff + 4)[0]
        mtotal += mats
        print(f'{w}: {count} textures and {mats} materials identical')
    print(f'ok: {total} texture directory entries and {mtotal} materials across {len(WORLDS)} worlds')


if __name__ == '__main__':
    main()
