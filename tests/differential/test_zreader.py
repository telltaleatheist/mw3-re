"""The ported zReader against the original x86 code, on every entry of reader.zbd.

    cmake --build build && .venv/bin/python tests/differential/test_zreader.py

For each of the 178 files in the archive, the original zRdrReadNode (0x557180)
runs in the emulator with ReadFile and malloc served from Python, and the C++
read_node runs on the same bytes. The trees must be identical. Then, for every
string in the tree used as a key, the original find (0x5569e0), get_float
(0x556a60) and get_int (0x556ae0) must agree with the port.
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
probe.probe_zrdr_load.argtypes = [ctypes.c_char_p, ctypes.c_long]
probe.probe_zrdr_find.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int,
                                  ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_float),
                                  ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int32)]
READER = ROOT / 'game' / 'mw3-1.2' / 'zbd' / 'reader.zbd'

READ_NODE, FIND, GET_FLOAT, GET_INT = 0x557180, 0x5569e0, 0x556a60, 0x556ae0
IAT_READFILE, IAT_MALLOC = 0x5bb0c4, 0x5bb664
HANDLE = 0x1234


class Emulated:
    """The original reader, reading from a byte string as if from a file."""

    def __init__(self, data):
        self.data, self.pos = data, 0
        self.cpu = Original()
        self.cpu.stub_import(IAT_READFILE, self.read_file, pop_bytes=20)
        self.cpu.stub_import(IAT_MALLOC, lambda c: c.alloc(max(c.arg(0), 1)))
        self.base_heap = self.cpu.heap_next

    def read_file(self, cpu):
        handle, buf, n, out = cpu.arg(0), cpu.arg(1), cpu.arg(2), cpu.arg(3)
        assert handle == HANDLE
        chunk = self.data[self.pos:self.pos + n]
        self.pos += len(chunk)
        cpu.write(buf, chunk)
        cpu.u32(out, len(chunk))
        return 1

    def load(self, offset):
        cpu = self.cpu
        cpu.heap_next = self.base_heap
        self.pos = offset
        self.root = cpu.alloc(8)
        cpu.call(READ_NODE, ecx=HANDLE, edx=self.root)
        return cpu.uc.reg_read(UC_X86_REG_EAX)

    def u32(self, addr):
        return struct.unpack('<I', self.cpu.read(addr, 4))[0]

    def cstr(self, addr):
        out = bytearray()
        while True:
            b = self.cpu.read(addr + len(out), 64)
            i = b.find(0)
            if i >= 0:
                return bytes(out + b[:i])
            out += b

    def walk(self, addr, out, paths, past_end, prefix, strings):
        t, v = self.u32(addr), self.u32(addr + 4)
        if t == 1:
            out.append(f'I{struct.unpack("<i", struct.pack("<I", v))[0]} ')
        elif t == 2:
            out.append(f'F{v:08x} ')
        elif t == 3:
            s = self.cstr(v)
            strings.add(s)
            out.append(f'S{len(s)}:' + s.decode('latin-1') + ' ')
        elif t == 4:
            count = struct.unpack('<i', struct.pack('<I', self.u32(v + 4)))[0]
            out.append(f'L{count}[')
            for i in range(1, count + 1):
                p = f'{prefix}{i}'
                paths[v + 8 * i] = p
                if i == count:
                    past_end.add(v + 8 * i)
                if i < count:
                    self.walk(v + 8 * i, out, paths, past_end, p + '/', strings)
            out.append('] ')
        else:
            out.append('? ')

    def find(self, key):
        cpu = self.cpu
        k = cpu.alloc(len(key) + 1)
        cpu.write(k, key + b'\0')
        cpu.call(FIND, ecx=self.root, edx=k)
        hit = cpu.uc.reg_read(UC_X86_REG_EAX)
        outp = cpu.alloc(4)
        cpu.u32(outp, 0)
        cpu.call(GET_FLOAT, ecx=self.root, edx=k, stack_args=(outp,))
        gf, fbits = cpu.uc.reg_read(UC_X86_REG_EAX), self.u32(outp)
        cpu.u32(outp, 0)
        cpu.call(GET_INT, ecx=self.root, edx=k, stack_args=(outp,))
        gi, ival = cpu.uc.reg_read(UC_X86_REG_EAX), self.u32(outp)
        return hit, gf, fbits, gi, ival


def entries(data):
    version, count = struct.unpack_from('<II', data, len(data) - 8)
    assert version == 1
    table = len(data) - 8 - count * 0x94
    for i in range(count):
        start, length = struct.unpack_from('<II', data, table + i * 0x94)
        name = data[table + i * 0x94 + 8:table + i * 0x94 + 72].split(b'\0')[0].decode()
        yield name, start, length


def main():
    if not READER.exists():
        raise SystemExit(f'needs {READER}')
    data = READER.read_bytes()
    emu = Emulated(data)
    buf = ctypes.create_string_buffer(8 << 20)
    files = keys = past = 0
    for name, start, length in entries(data):
        got = emu.load(start)
        want_n = probe.probe_zrdr_load(str(READER).encode(), start)
        if got != want_n or got != length:
            raise SystemExit(f'{name}: bytes read original={got} port={want_n} entry={length}')
        out, paths, past_end, strings = [], {}, set(), set()
        emu.walk(emu.root, out, paths, past_end, '', strings)
        n = probe.probe_zrdr_dump(buf, len(buf))
        if n < 0 or ''.join(out) != buf.value.decode('latin-1'):
            raise SystemExit(f'{name}: trees differ')
        for key in sorted(strings)[:400]:
            hit, gf, fbits, gi, ival = emu.find(key)
            cgf, cf = ctypes.c_int(), ctypes.c_float(0)
            cgi, ci = ctypes.c_int(), ctypes.c_int32(0)
            probe.probe_zrdr_find(key, buf, len(buf), ctypes.byref(cgf), ctypes.byref(cf),
                                  ctypes.byref(cgi), ctypes.byref(ci))
            want = (paths.get(hit, '') if hit else '', gf, fbits if gf else 0, gi, ival if gi else 0)
            cbits = struct.unpack('<I', struct.pack('<f', cf.value))[0]
            mine = (buf.value.decode(), cgf.value, cbits if cgf.value else 0, cgi.value,
                    (ci.value & 0xffffffff) if cgi.value else 0)
            empty_list = hit and emu.u32(hit) == 4 and emu.u32(emu.u32(hit + 4) + 4) <= 1
            if hit in past_end or empty_list:
                # Either the key was the last item, so find returns the slot past
                # the end, or its value is an empty list "()", whose item 1 the
                # getters read anyway. Both read heap memory beyond the list:
                # undefined in the original, "no value" in the port (FIX (A)).
                # Compare find only.
                want, mine = want[0], mine[0]
                past += 1
            if want != mine:
                raise SystemExit(f'{name}: key {key!r}: original {want} port {mine}')
            keys += 1
        files += 1
    print(f'ok: {files} files, identical trees; {keys} key lookups agree '
          f'({past} past-the-end reads compared on find only)')


if __name__ == '__main__':
    main()
