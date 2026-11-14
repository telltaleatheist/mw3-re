"""WriteZBD on Pirate's Moon's exe (GameZWriteZBDFile, 0x5168e0; ported for preservation, the engine's entry left
null; MW3's test_write_zbd.py): each of PM's worlds read by PM's loader (test_pm_gamez.py's bench) and by the port,
then written back by both: the texture directory (0x531870), the materials (0x555ae0), the models (0x557d50, with
the material sort 0x55c470) and the live nodes (0x515fb0, 0x516260, 0x516720, 0x516800) compared byte for byte,
except the time stamp and the words where the exe wrote a heap pointer of its own (the reader replaces them:
docs/gamez.md, Writing a world back), with the reports. Then each file read back by its own side gives the same
world, and the port's file list has the written file.

    cmake --build build --target pmprobe && python3 -P tests/differential/test_pm_write_zbd.py [scratch dir]
"""
import ctypes
import os
import struct
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
os.environ['MW3_PM_MOVED'] = '1'  # this test's addresses are PM's own
import test_pm_gamez as tg  # noqa: E402
import test_pm_zar  # noqa: E402
from twod_world import IAT  # noqa: E402

# The exe's heap where no float or count of the worlds falls (0x7a000000 is about 1.7e35, below infinity at
# 0x7f800000), so a word there is one of its pointers.
tg.HEAP, tg.HEAP_SIZE = 0x7a000000, 0x5800000
WRITE_ZBD, LOG_ERROR = 0x5168e0, 0x406cb0


class Writer:
    """The exe's writes into memory, its reports kept."""

    def __init__(self, emu):
        self.emu, self.out, self.pos, self.reports = emu, bytearray(), 0, []

    def fwrite(self, c):
        p, size, n = c.arg(0), c.arg(1), c.arg(2)
        if size == 0 or n == 0:
            return 0  # MSVC's fwrite: nothing written counts 0
        data = bytes(self.emu.cpu._uc.mem_read(p, size * n))
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

    def time(self, c):
        if c.arg(0):
            self.emu.cpu._uc.mem_write(c.arg(0), struct.pack('<Q', 0x12345678))
        return 0x12345678

    def malloc(self, c):  # uninitialised in the exe: 0xff here, so the node remap's unset entries read -1
        a = self.emu.sized(c.arg(0))
        self.emu.cpu._uc.mem_write(a, b'\xff' * max(c.arg(0), 4))
        return a

    def run(self, name, drop):
        emu, cpu = self.emu, self.emu.cpu
        for i in range(drop - 1, emu.i32(tg.N_CAPACITY), drop) if drop else ():
            a = emu.r32(emu.r32(tg.N_BLOCKS) + 4 * (i // 2500)) + 0xd4 * (i % 2500) + 0xd0
            emu.w32(a, emu.r32(a) & ~0x2000000)
        cpu.stub_import(IAT['malloc'], self.malloc)
        cpu.stub_import(IAT['fopen'], lambda c: tg.HANDLE)
        cpu.stub_import(IAT['fwrite'], self.fwrite)
        cpu.stub_import(IAT['ftell'], lambda c: self.pos)
        cpu.stub_import(IAT['fseek'], self.fseek)
        cpu.stub_import(IAT['fclose'], lambda c: 0)
        cpu.stub_import(IAT['time'], self.time)
        emu.native_stub(LOG_ERROR, lambda c: self.reports.extend((c.arg(0), c.arg(2))) or 0)
        res = emu.signed(emu.call(WRITE_ZBD, args=[emu.string(name)]))
        # The reader's stubs back.
        cpu.stub_import(IAT['ftell'], lambda c: emu.pos)
        cpu.stub_import(IAT['malloc'], lambda c: emu.sized(c.arg(0)))
        cpu.stub_import(IAT['fseek'], emu.fseek)
        emu.native_stub(LOG_ERROR, lambda c: 0)
        files = [emu.cstr(emu.r32(emu.r32(tg.FILES) + 0x2c * i)) for i in range(emu.i32(tg.NFILES))]
        return res, bytes(self.out), self.reports, files


def masked(want, got, heap_end):
    """Both with the time stamp and the words where the exe wrote one of its heap pointers zeroed; their count."""
    w, g = bytearray(want), bytearray(got)
    w[8:0xc] = g[8:0xc] = bytes(4)
    n = 0
    for i in range(0, min(len(w), len(g)) - 3, 4):
        if tg.HEAP <= struct.unpack_from('<I', w, i)[0] < heap_end:
            w[i:i + 4] = g[i:i + 4] = bytes(4)
            n += 1
    return bytes(w), bytes(g), n


def main():
    scratch = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(tempfile.mkdtemp(prefix='pm_write_zbd_'))
    p = test_pm_zar.load_pm_probe()
    p.probe_pm_write_zbd.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_void_p, ctypes.c_int]
    p.probe_pm_write_zbd.restype = ctypes.c_int
    p.probe_pm_gamez_stack.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_int, ctypes.c_int,
                                       ctypes.c_int, ctypes.c_void_p, ctypes.c_int]
    buf = ctypes.create_string_buffer(1 << 27)
    emu = tg.Emulated()
    worlds = 0
    # Each world whole, then c1 and c3 with every 97th node not written (every shipped node is: the remap and the
    # lists naming one not written are reached only so).
    for w, drop in [(w, 0) for w in tg.WORLDS] + [('c1', 97), ('c3', 97)]:
        path = tg.ZBD / w / 'gamez.zbd'
        emu.stack([path], 5, 6, 5, models=True, nodes=True)
        res, want, want_reports, files = Writer(emu).run('out.zbd', drop)
        heap_end = emu.heap
        assert heap_end < tg.HEAP + tg.HEAP_SIZE
        rep = (ctypes.c_int32 * 256)()
        nfiles = p.probe_pm_write_zbd(str(path).encode(), str(scratch).encode(), drop, rep, 256)
        got = (scratch / 'out.zbd').read_bytes()
        got_reports = list(rep[2:2 + rep[1]])
        if rep[0] != res or got_reports != want_reports:
            raise SystemExit(f'{w}: result {rep[0]} vs {res}, reports {got_reports} vs {want_reports}')
        if nfiles != len(files) or files[-1] != b'out.zbd':
            raise SystemExit(f'{w}: the file list: {nfiles} vs {files}')
        mw, mg, pointers = masked(want, got, heap_end)
        if mw != mg:
            at = next((i for i in range(min(len(mw), len(mg))) if mw[i] != mg[i]), None)
            (scratch / f'{w}_want.zbd').write_bytes(want)
            (scratch / f'{w}_got.zbd').write_bytes(got)
            raise SystemExit(f'{w}: {len(want)} vs {len(got)} bytes, first difference at {at:#x}'
                             if at is not None else f'{w}: {len(want)} vs {len(got)} bytes')
        name = f'{w}' + (f' without every {drop}th node' if drop else '')
        if drop:  # lists naming nodes not written (-1): not read back
            print(f'{name}: {len(got)} bytes as PM\'s exe ({pointers} of its own pointers left out)')
            worlds += 1
            continue
        # Read back: the port's file by the port as the exe's own file by the exe.
        exe_file = scratch / f'{w}_exe.zbd'
        exe_file.write_bytes(want)
        for nodes in (False, True):  # the models dumped, then the nodes (the stack dumps one or the other)
            back = emu.stack([exe_file], 5, 6, 5, models=True, nodes=nodes)
            n = p.probe_pm_gamez_stack(str(scratch / 'out.zbd').encode(), 5, 6, 5, 1 + nodes, buf, len(buf))
            if n < 0 or buf.raw[:n] != back:
                raise SystemExit(f'{w}: the written files read back differently ({"nodes" if nodes else "models"}, '
                                 f'{n} vs {len(back)} bytes)')
        print(f'{w}: {len(got)} bytes as PM\'s exe ({pointers} of its own pointers left out, {len(want_reports) // 2} '
              f'reports), read back alike')
        worlds += 1
    print(f'ok: {worlds} runs (PM\'s 4 worlds, 2 with nodes not written) written back as PM\'s exe writes them')


if __name__ == '__main__':
    main()
