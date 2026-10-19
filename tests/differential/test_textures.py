"""The ported texture library reader against the original x86, on every texture.

    cmake --build build && .venv/bin/python tests/differential/test_textures.py

For each texture library in game/mw3-1.2/zbd, the original open (0x5187c0)
and load (0x518db0) run in the emulator with the C runtime's file and memory
calls served from Python; the C++ port opens the same file. For every texture
the size, flags, pixels, alpha and palette must be byte-identical. One library
also runs with the display in RGB555 (format 5), which converts the colours.
"""
import ctypes
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import ROOT, Original  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EDX  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
probe.probe_tex_open.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
probe.probe_tex_load.argtypes = [ctypes.c_char_p, ctypes.c_void_p, ctypes.c_int]
ZBD = ROOT / 'game' / 'mw3-1.2' / 'zbd'
LIBRARIES = ['rmechtex16.zbd', 'rmechtexs.zbd', 'rmechtex.zbd', 'rimage.zbd']

OPEN, LOAD, OPEN_PATH = 0x5187c0, 0x518db0, 0x5763b0
IAT = {'fread': 0x5bb594, 'fseek': 0x5bb59c, 'fclose': 0x5bb630, 'malloc': 0x5bb664,
       'calloc': 0x5bb60c, 'realloc': 0x5bb610, 'free': 0x5bb65c, 'stricmp': 0x5bb580}
ENABLED, DISPLAY = 0x5ecc20, 0x802720
BIG = 0x40000000   # a large heap for the textures
HANDLE = 0x77


class Emulated:
    def __init__(self):
        cpu = self.cpu = Original()
        cpu.uc.mem_map(BIG, 0x10000000)
        self.next = BIG
        self.sizes = {}
        cpu.stub_import(IAT['fread'], self.fread)
        cpu.stub_import(IAT['fseek'], self.fseek)
        cpu.stub_import(IAT['fclose'], lambda c: 0)
        cpu.stub_import(IAT['malloc'], lambda c: self.alloc(c.arg(0)))
        cpu.stub_import(IAT['calloc'], self.calloc)
        cpu.stub_import(IAT['realloc'], self.realloc)
        cpu.stub_import(IAT['free'], lambda c: 0)
        cpu.stub_import(IAT['stricmp'], self.stricmp)
        cpu.stub_function(OPEN_PATH, lambda c: HANDLE, 4)
        cpu.u32(ENABLED, 1)

    def alloc(self, n):
        a = self.next
        self.next += (max(n, 1) + 0xf) & ~0xf
        self.sizes[a] = n
        return a

    def calloc(self, c):
        n = c.arg(0) * c.arg(1)
        a = self.alloc(n)
        c.write(a, bytes(n))
        return a

    def realloc(self, c):
        old, n = c.arg(0), c.arg(1)
        a = self.alloc(n)
        if old:
            c.write(a, c.read(old, min(self.sizes.get(old, 0), n)))
        return a

    def cstr(self, a):
        out = bytearray()
        while True:
            b = self.cpu.read(a + len(out), 32)
            i = b.find(0)
            if i >= 0:
                return bytes(out + b[:i])
            out += b

    def stricmp(self, c):
        a, b = self.cstr(c.arg(0)).lower(), self.cstr(c.arg(1)).lower()
        return (a > b) - (a < b)

    def fread(self, c):
        buf, size, count, f = c.arg(0), c.arg(1), c.arg(2), c.arg(3)
        assert f == HANDLE
        want = size * count
        chunk = self.data[self.pos:self.pos + want]
        self.pos += len(chunk)
        c.write(buf, chunk)
        return len(chunk) // size if size else 0

    def fseek(self, c):
        assert c.arg(0) == HANDLE and c.arg(2) == 0
        self.pos = c.arg(1)
        return 0

    def open(self, name, data, fmt):
        cpu = self.cpu
        self.data, self.pos = data, 0
        self.next = BIG
        for a in (DISPLAY, DISPLAY + 4, DISPLAY + 8):
            cpu.u32(a, fmt)
        cpu.u32(0x63850c, 0)   # global palette count
        cpu.u32(0x638510, 0)
        cpu.u32(0x638514, 0)   # light tables: none (hardware rendering)
        self.lib = self.alloc(0xac)
        cpu.write(self.lib, bytes(0xac))
        cpu.write(self.lib, name.encode() + b'\0')
        cpu.call(OPEN, ecx=self.lib)
        ok = cpu.uc.reg_read(UC_X86_REG_EAX) & 0xff
        self.mark = self.next
        return struct.unpack('<i', cpu.read(self.lib + 0x90, 4))[0] if ok else -1

    def load(self, name):
        cpu = self.cpu
        self.next = self.mark
        k = self.alloc(len(name) + 1)
        cpu.write(k, name + b'\0')
        cpu.call(LOAD, ecx=self.lib, stack_args=(k,))
        t = cpu.uc.reg_read(UC_X86_REG_EAX)
        if not t:
            return None
        raw = cpu.read(t, 0x38)
        count, w, h = struct.unpack_from('<ihh', raw, 0)
        b8, flags = raw[8], raw[9]
        stretch, pal_count = struct.unpack_from('<hh', raw, 0xc)
        pixels, alpha, palette = struct.unpack_from('<III', raw, 0x10)
        width2, = struct.unpack_from('<i', raw, 0x34)
        out = raw[0:4] + raw[4:8] + bytes([b8, flags]) + raw[0xc:0x10] + raw[0x34:0x38]
        bpp = 2 if flags & 1 else 1
        if pixels:
            out += cpu.read(pixels, count if pal_count else bpp * count)
        if alpha:
            out += cpu.read(alpha, count)
        if palette:
            out += cpu.read(palette, 0x200 if flags & 0x10 else 2 * pal_count)
        return out


def names(data):
    count, = struct.unpack_from('<i', data, 12)
    return [data[24 + 40 * i:24 + 40 * i + 32].split(b'\0')[0] for i in range(count)]


def main():
    emu = Emulated()
    buf = ctypes.create_string_buffer(8 << 20)
    total = 0
    runs = [(n, 6) for n in LIBRARIES] + [('rmechtexs.zbd', 5)]
    for lib, fmt in runs:
        path = ZBD / lib
        if not path.exists():
            raise SystemExit(f'needs {path}')
        data = path.read_bytes()
        got = emu.open(lib, data, fmt)
        want = probe.probe_tex_open((str(ZBD) + '/').encode(), lib.encode(), fmt)
        if got != want:
            raise SystemExit(f'{lib}: open original {got} port {want}')
        for name in names(data):
            a = emu.load(name)
            n = probe.probe_tex_load(name, buf, len(buf))
            b = buf.raw[:n] if n >= 0 else None
            if a != b:
                raise SystemExit(f'{lib} {name!r}: original {len(a or b"")} bytes, port {n}')
            total += 1
        print(f'{lib} (display format {fmt}): {got} textures identical')
    print(f'ok: {total} textures')


if __name__ == '__main__':
    main()
