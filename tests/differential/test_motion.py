"""The ported motion reader against the original x86, on every motion.

    cmake --build build && .venv/bin/python tests/differential/test_motion.py

For each entry of motion.zbd, the original CZSIScript constructor (0x5a9920)
and reader (0x5a9c70) run in the emulator with ReadFile and the allocators
served from Python; the port reads the same bytes. Header fields, part names,
translations, rotations and scales must be identical.
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
probe.probe_motion.argtypes = [ctypes.c_char_p, ctypes.c_long, ctypes.c_void_p, ctypes.c_int]
MOTION = ROOT / 'game' / 'mw3-1.2' / 'zbd' / 'motion.zbd'
CTOR, READ, VTABLE = 0x5a9920, 0x5a9c70, 0x5d0f20
IAT_READFILE, IAT_MALLOC, IAT_CALLOC, IAT_NEW = 0x5bb0c4, 0x5bb664, 0x5bb60c, 0x5bb4bc
HANDLE = 0x55
BIG = 0x40000000


class Emulated:
    def __init__(self, data):
        cpu = self.cpu = Original()
        cpu.uc.mem_map(BIG, 0x4000000)
        self.data = data
        cpu.stub_import(IAT_READFILE, self.read_file, pop_bytes=20)
        cpu.stub_import(IAT_MALLOC, lambda c: self.alloc(c.arg(0)))
        cpu.stub_import(IAT_NEW, lambda c: self.alloc(c.arg(0)))
        cpu.stub_import(IAT_CALLOC, self.calloc)

    def alloc(self, n):
        a = self.next
        self.next += (max(n, 1) + 0xf) & ~0xf
        return a

    def calloc(self, c):
        n = c.arg(0) * c.arg(1)
        a = self.alloc(n)
        c.write(a, bytes(n))
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

    def cstr(self, a):
        out = bytearray()
        while True:
            b = self.cpu.read(a + len(out), 64)
            i = b.find(0)
            if i >= 0:
                return bytes(out + b[:i])
            out += b

    def load(self, offset):
        cpu = self.cpu
        self.next, self.pos = BIG, offset
        s = self.alloc(0x88)
        cpu.write(s, bytes(0x88))
        cpu.u32(s, VTABLE)
        cpu.call(CTOR, ecx=s)
        cpu.call(READ, ecx=s, stack_args=(HANDLE,))
        ok = cpu.uc.reg_read(UC_X86_REG_EAX) & 0xff
        r = cpu.read
        out = bytes([ok]) + r(s + 4, 4) + r(s + 8, 4) + r(s + 0xc, 4) + r(s + 0x10, 1)
        out += r(s + 0x54, 16) + r(s + 0x64, 8) + r(s + 0x74, 4)
        frames, parts, table = self.u32(s + 0x64), self.u32(s + 0x68), self.u32(s + 0x6c)
        n = frames + 1
        for i in range(parts if ok else 0):
            p = self.u32(table + 4 * i)
            out += self.cstr(self.u32(p + 0x44)) + b'\0'
            out += r(self.u32(p), n * 12) + r(self.u32(p + 8), n * 16)
            if self.u32(p + 4):
                out += r(self.u32(p + 4), n * 12)
        return out


def entries(data):
    version, count = struct.unpack_from('<II', data, len(data) - 8)
    table = len(data) - 8 - count * 0x94
    for i in range(count):
        start, length = struct.unpack_from('<II', data, table + i * 0x94)
        name = data[table + i * 0x94 + 8:table + i * 0x94 + 72].split(b'\0')[0].decode()
        yield name, start, length


def main():
    data = MOTION.read_bytes()
    emu = Emulated(data)
    buf = ctypes.create_string_buffer(4 << 20)
    count = frames = 0
    for name, start, length in entries(data):
        want = emu.load(start)
        n = probe.probe_motion(str(MOTION).encode(), start, buf, len(buf))
        if n < 0 or buf.raw[:n] != want:
            raise SystemExit(f'{name}: original {len(want)} bytes, port {n}')
        if emu.pos != start + length:
            raise SystemExit(f'{name}: read {emu.pos - start} of {length} bytes')
        count += 1
        frames += struct.unpack_from('<I', want, 34)[0]
    print(f'ok: {count} motions identical ({frames} frames), each read to its exact end')


if __name__ == '__main__':
    main()
