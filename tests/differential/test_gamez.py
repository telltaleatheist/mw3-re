"""The ported world-file readers against the original x86, on every mission's
gamez.zbd (docs/gamez.md). So far: the texture directory (0x518160).

    cmake --build build && .venv/bin/python tests/differential/test_gamez.py
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
probe.probe_texture_directory.argtypes = [ctypes.c_char_p, ctypes.c_long, ctypes.c_int,
                                          ctypes.c_int, ctypes.c_void_p, ctypes.c_int]
ZBD = ROOT / 'game' / 'mw3-1.2' / 'zbd'
WORLDS = ['c1', 'c2', 'c3', 'c4', 'c4b', 't1']
TEXDIR, TABLE, COUNT, GROUP = 0x518160, 0x610484, 0x610480, 0x6384d4
IAT_FREAD = 0x5bb594
HANDLE = 0x44


class Emulated:
    def __init__(self):
        self.cpu = Original()
        self.cpu.stub_import(IAT_FREAD, self.fread)

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
    total = 0
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
        print(f'{w}: {count} textures identical')
    print(f'ok: {total} texture directory entries across {len(WORLDS)} worlds')


if __name__ == '__main__':
    main()
