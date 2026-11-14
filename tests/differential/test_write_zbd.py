"""WriteZBD (GameZWriteZBDFile, 0x4fe8e0; ported for preservation, the engine's entry left null) against the
original x86: every shipped world read (0x4ff520) and written back by both, the texture directory (0x5180a0), the
materials (0x5360c0), the models (0x5380e0) and the nodes (0x4fe6c0, 0x4fe2d0, 0x4fe210) compared byte for byte,
except the words where the exe wrote a heap pointer of its own (the reader replaces them: docs/gamez.md, Writing a
world back). Then each file read back by its own side gives the same world.

    cmake --build build && python3 tests/differential/test_write_zbd.py [scratch dir]
"""
import ctypes
import struct
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_gamez as tg  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

probe = tg.probe
probe.probe_write_zbd.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_void_p, ctypes.c_int]
probe.probe_write_zbd.restype = ctypes.c_int
probe.probe_write_zbd_cycle0.argtypes = probe.probe_write_zbd.argtypes
WRITE_ZBD = 0x4fe8e0
# The exe's heap where no float or count of the data falls (0x7a000000 is about 1.7e35), so a word there is one of
# its pointers; test_gamez's 0x40000000 is the floats 2..512.
tg.BIG = 0x7a000000
IAT_FWRITE, IAT_FTELL, IAT_FSEEK, IAT_FOPEN, IAT_FCLOSE = 0x5bb5a0, 0x5bb598, 0x5bb59c, 0x5bb628, 0x5bb630
HEAP_LO, HEAP_HI = tg.BIG, tg.BIG + 0x4000000


class Writer:
    """The exe's writes into memory."""

    def __init__(self, emu):
        self.emu, self.out, self.pos = emu, bytearray(), 0

    def fwrite(self, c):
        p, size, n = c.arg(0), c.arg(1), c.arg(2)
        if size * n == 0:
            return 0  # MSVC's fwrite: nothing written counts 0
        data = c.read(p, size * n)
        end = self.pos + len(data)
        if end > len(self.out):
            self.out.extend(bytes(end - len(self.out)))
        self.out[self.pos:end] = data
        self.pos = end
        return n

    def fseek(self, c):
        assert c.arg(2) == 0
        self.pos = c.arg(1)
        return 0

    def run(self, path):
        cpu = self.emu.cpu
        cpu.stub_import(IAT_FOPEN, lambda c: tg.HANDLE)
        cpu.stub_import(IAT_FWRITE, self.fwrite)
        cpu.stub_import(IAT_FTELL, lambda c: self.pos)
        cpu.stub_import(IAT_FSEEK, self.fseek)
        cpu.stub_import(IAT_FCLOSE, lambda c: 0)
        self.emu.reports = []
        p = self.emu.alloc(len(path) + 1)
        cpu.write(p, path + b'\0')
        cpu.call(WRITE_ZBD, ecx=p)
        res = struct.unpack('<i', struct.pack('<I', cpu.uc.reg_read(UC_X86_REG_EAX)))[0]
        # The reader's stubs back for the next world.
        cpu.stub_import(IAT_FOPEN, lambda c: tg.HANDLE if self.emu.open_ok else 0)
        cpu.stub_import(IAT_FSEEK, self.emu.fseek)
        return res, bytes(self.out), [x for r in self.emu.reports for x in r]


def masked(want, got):
    """Both with the words where the exe wrote one of its heap pointers zeroed; the count of such words."""
    w, g = bytearray(want), bytearray(got)
    n = 0
    for i in range(0, min(len(w), len(g)) - 3, 4):
        if HEAP_LO <= struct.unpack_from('<I', w, i)[0] < HEAP_HI:
            w[i:i + 4] = g[i:i + 4] = bytes(4)
            n += 1
    return bytes(w), bytes(g), n


def main():
    scratch = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(tempfile.mkdtemp())
    emu = tg.Emulated()
    buf = ctypes.create_string_buffer(64 << 20)
    worlds = 0
    for w in tg.WORLDS:
        path = tg.ZBD / w / 'gamez.zbd'
        data = path.read_bytes()
        read = emu.read_zbd(data, str(path).encode())
        assert struct.unpack_from('<i', read)[0] == 0, w
        res, want, want_reports = Writer(emu).run(b'out.zbd')
        out_path = scratch / 'out.zbd'
        rep = (ctypes.c_int32 * 64)()
        n = probe.probe_write_zbd(str(path).encode(), str(scratch).encode(), rep, 64)
        got = out_path.read_bytes()
        if rep[0] != 0 or rep[1] != res or list(rep[2:2 + n]) != want_reports:
            raise SystemExit(f'{w}: results {rep[0]}, {rep[1]} vs {res}, reports {list(rep[2:2 + n])} vs '
                             f'{want_reports}')
        mw, mg, pointers = masked(want, got)
        if mw != mg:
            at = next((i for i in range(min(len(mw), len(mg))) if mw[i] != mg[i]), None)
            (scratch / f'{w}_want.zbd').write_bytes(want)
            raise SystemExit(f'{w}: {len(want)} vs {len(got)} bytes, first difference at {at:#x}'
                             if at is not None else f'{w}: {len(want)} vs {len(got)} bytes')
        # Read back: the port's file by the port as the exe's own file by the exe (the world, lists and fog). A pool
        # that is not whole blocks (t1's 16,000 slots) comes back whole blocks, its free list longer, in both.
        size = struct.unpack_from('<i', got, 0x18)[0]
        k = probe.probe_read_zbd(str(out_path).encode(), size, buf, len(buf))
        if buf.raw[:k] != emu.read_zbd(want, str(out_path).encode()):
            raise SystemExit(f'{w}: the written files read back differently')
        print(f'{w}: {len(got)} bytes as the original ({pointers} of its own pointers left out), read back alike')
        worlds += 1
    # A cycle of no textures (none in a file: the reader refuses one), the first used cycled material's made so
    # after the read: its 0-byte fwrite fails in the exe (MSVC's), reported, the write's materials 0.
    cycled = 0
    for w in tg.WORLDS:
        path = tg.ZBD / w / 'gamez.zbd'
        emu.read_zbd(path.read_bytes(), str(path).encode())
        r32 = lambda a: struct.unpack('<I', emu.cpu.read(a, 4))[0]  # noqa: E731
        blocks, block, i = r32(0x63fbc8), r32(0x5edd50), struct.unpack('<i', emu.cpu.read(0x5edd58, 4))[0]
        while i >= 0:
            m = r32(blocks + 4 * (i // block)) + 0x2c * (i % block)
            if emu.cpu.read(m + 1, 1)[0] & 4:
                break
            i = struct.unpack('<h', emu.cpu.read(m + 0x2a, 2))[0]
        if i < 0:
            continue
        emu.cpu.u32(r32(m + 0x24) + 0x10, 0)
        res, want, want_reports = Writer(emu).run(b'out.zbd')
        rep = (ctypes.c_int32 * 64)()
        n = probe.probe_write_zbd_cycle0(str(path).encode(), str(scratch).encode(), rep, 64)
        got = (scratch / 'out.zbd').read_bytes()
        if rep[1] != res or list(rep[2:2 + n]) != want_reports or (0x200, 0x2f5) != tuple(want_reports[:2]):
            raise SystemExit(f'{w} with a cycle of no textures: {rep[1]} vs {res}, reports {list(rep[2:2 + n])} vs '
                             f'{want_reports}')
        mw, mg, _ = masked(want, got)
        if mw != mg:
            raise SystemExit(f'{w} with a cycle of no textures: the files differ ({len(want)} vs {len(got)} bytes)')
        print(f'{w}: a cycle of no textures reported (0x2f5) and the write gone on, as the original')
        cycled += 1
    if cycled == 0:
        raise SystemExit('no world has a cycled material')
    print(f'ok: {worlds} worlds written back as the original; {cycled} with a cycle of no textures')


if __name__ == '__main__':
    main()
