"""The ported zReader against the original x86 code, on every entry of reader.zbd.

    cmake --build build && .venv/bin/python tests/differential/test_zreader.py

For each of the 178 files in the archive, the original zRdrReadNode (0x557180)
runs in the emulator with ReadFile and malloc served from Python, and the C++
read_node runs on the same bytes. The trees must be identical. Then, for every
string in the tree used as a key, the original find (0x5569e0), get_float
(0x556a60), get_int (0x556ae0) and the keyed list and boolean getters
(0x5570b0-0x557150) must agree with the port. MSVCRT's _ftol, which the integer
lists call, runs as the same x86 code (original.FTOL).
"""
import ctypes
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import FTOL, IAT_FTOL, ROOT, Original  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

class _NoProbe:
    """MW3's probe where it isn't built (MW3_EXE=pm on the pm branch): the modules that import this one set its
    functions' types at import, which works; calling one stops the run."""
    class _Fn:
        def __init__(self, name):
            self.name = name

        def __call__(self, *a, **k):
            raise SystemExit(f'{self.name}: MW3\'s probe (mw3probe) is not part of this build; this test runs on MW3')

    def __getattr__(self, name):
        if name.startswith('__'):
            raise AttributeError(name)
        fn = _NoProbe._Fn(name)
        object.__setattr__(self, name, fn)
        return fn


if os.environ.get('MW3_EXE') == 'pm':
    # Its reader probe is MW3's (mw3probe); modules run on PM's exe import this one for READER and the emulator.
    probe = _NoProbe()
else:
    LIB = next((ROOT / 'build').glob('libmw3probe.*'))
    probe = ctypes.CDLL(str(LIB))
    probe.probe_zrdr_load.argtypes = [ctypes.c_char_p, ctypes.c_long]
    probe.probe_zrdr_find.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int,
                                      ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_float),
                                      ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int32)]
    probe.probe_zrdr_lists.argtypes = [ctypes.c_char_p, ctypes.c_uint32] + [ctypes.c_void_p] * 7
READER = ROOT / 'game' / 'mw3-1.2' / 'zbd' / 'reader.zbd'

READ_NODE, FIND, GET_FLOAT, GET_INT = 0x557180, 0x5569e0, 0x556a60, 0x556ae0
IAT_READFILE, IAT_MALLOC, IAT_STRICMP = 0x5bb0c4, 0x5bb664, 0x5bb580
# Keyed getters with their output size in 4-byte slots (get_bool writes a byte).
LISTS = ((0x5570b0, 3), (0x5570d0, 2), (0x5570f0, 3), (0x557110, 2), (0x557130, 1), (0x557150, 1))
FILL = 0x7fa5a5a5  # a NaN no getter writes, so a slot left alone shows
HANDLE = 0x1234


class Emulated:
    """The original reader, reading from a byte string as if from a file."""

    def __init__(self, data):
        self.data, self.pos = data, 0
        self.cpu = Original()
        self.cpu.stub_import(IAT_READFILE, self.read_file, pop_bytes=20)
        self.cpu.stub_import(IAT_MALLOC, lambda c: c.alloc(max(c.arg(0), 1)))
        self.cpu.import_code(IAT_FTOL, FTOL)
        self.cpu.stub_import(IAT_STRICMP, self.stricmp)
        self.base_heap = self.cpu.heap_next

    def stricmp(self, cpu):
        # MSVCRT _stricmp in the C locale: ASCII letters folded to lower case.
        a, b = self.cstr(cpu.arg(0)).lower(), self.cstr(cpu.arg(1)).lower()
        return (a > b) - (a < b)

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

    def lists(self, key):
        cpu = self.cpu
        k = cpu.alloc(len(key) + 1)
        cpu.write(k, key + b'\0')
        out = []
        for addr, slots in LISTS:
            p = cpu.alloc(12)
            cpu.write(p, struct.pack('<III', FILL, FILL, FILL))
            cpu.call(addr, ecx=self.root, edx=k, stack_args=(p,))
            ok = cpu.uc.reg_read(UC_X86_REG_EAX) & 0xff
            vals = struct.unpack(f'<{slots}I', cpu.read(p, 4 * slots))
            if addr == 0x557150:
                vals = (vals[0] & 0xff,)
            out.append((ok != 0, vals))
        return out


def port_lists(key):
    v3, v2 = (ctypes.c_uint32 * 3)(), (ctypes.c_uint32 * 2)()
    iv3, iv2 = (ctypes.c_uint32 * 3)(), (ctypes.c_uint32 * 2)()
    bi, b, ok = ctypes.c_uint32(), ctypes.c_uint8(), (ctypes.c_int * 6)()
    probe.probe_zrdr_lists(key, FILL, v3, v2, iv3, iv2, ctypes.byref(bi), ctypes.byref(b), ok)
    vals = (tuple(v3), tuple(v2), tuple(iv3), tuple(iv2), (bi.value,), (b.value,))
    return [(ok[i] != 0, vals[i]) for i in range(6)]


def entries(data):
    version, count = struct.unpack_from('<II', data, len(data) - 8)
    assert version == 1
    table = len(data) - 8 - count * 0x94
    for i in range(count):
        start, length = struct.unpack_from('<II', data, table + i * 0x94)
        name = data[table + i * 0x94 + 8:table + i * 0x94 + 72].split(b'\0')[0].decode()
        yield name, start, length


def archive(files):
    """A version-1 archive of [(name, bytes)] (zreader.md)."""
    body, table = b'', b''
    for name, data in files:
        table += struct.pack('<II', len(body), len(data)) + name.encode().ljust(64, b'\0') + bytes(0x94 - 72)
        body += data
    return body + table + struct.pack('<II', 1, len(files)), len(body)


def tree_bytes(r, depth=0):
    """A random node tree's bytes."""
    k = r.choice([1, 1, 2, 3, 4, 4]) if depth < 3 else r.choice([1, 2, 3])
    if k == 1:
        return struct.pack('<ii', 1, r.randrange(-50, 50))
    if k == 2:
        return struct.pack('<if', 2, r.choice([0.5, -2.0, 1e9]))
    if k == 3:
        s = bytes(r.choice(b'abcxyz_0') for _ in range(r.randrange(0, 9)))
        return struct.pack('<iI', 3, len(s)) + s
    items = [tree_bytes(r, depth + 1) for _ in range(r.randrange(0, 5))]
    return struct.pack('<ii', 4, len(items) + 1) + b''.join(items)


def tails(buf):
    """Entries whose lists say more items than they hold (the community's hand-made mpinfo.zrd and aiv.zrd
    entries): the original reads on into the entries after it, and the port's archive read (zrdr::load, bounded
    by the archive's own data) must give the same tree. Where the original's read leaves the archive's data
    (the table, the file's end) the port stops and leaves the items owed empty: only its start is compared."""
    import random
    import tempfile
    probe.probe_zrdr_load_entry.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
    same = cut = odd = 0
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / 'tails.zbd'
        for seed in range(300):
            r = random.Random(f'tail:{seed}')
            n = r.randrange(2, 6)
            files = []
            for k in range(n):
                items = [tree_bytes(r, 1) for _ in range(r.randrange(1, 6))]
                files.append([f'e{k}.zrd', items])
            victim = r.randrange(n)
            more = r.choice([1, 1, 1, 2, 3, 12])
            bad_word = r.random() < 0.15  # a word that is no node type in the victim (a hand edit)
            blobs = []
            for k, (name, items) in enumerate(files):
                count = len(items) + 1 + (more if k == victim else 0)
                body = b''.join(items)
                if bad_word and k == victim:
                    body = struct.pack('<i', r.choice([0, 9, 1024])) + body
                    count += 1
                blobs.append((name, struct.pack('<ii', 4, count) + body))
            data, data_end = archive(blobs)
            path.write_bytes(data)
            start = sum(len(b) for _, b in blobs[:victim])
            emu = Emulated(data)
            emu.load(start)
            inside = emu.pos <= data_end
            out = []
            emu.walk(emu.root, out, {}, set(), '', set())
            want = ''.join(out)
            got_ok = probe.probe_zrdr_load_entry(str(path).encode(), blobs[victim][0].encode())
            if got_ok != 1:
                raise SystemExit(f'tail seed {seed}: the port did not read the entry ({got_ok}); original read to '
                                 f'{emu.pos:#x} of data {data_end:#x}')
            probe.probe_zrdr_dump(buf, len(buf))
            got = buf.value.decode('latin-1')
            if inside:
                if want != got:
                    raise SystemExit(f'tail seed {seed}: trees differ\n  original {want[:300]}\n  port     {got[:300]}')
                same += 1
                odd += bad_word
            else:
                # The items read before the data ran out are the original's; after that the port's are empty.
                head = 0
                while head < min(len(want), len(got)) and want[head] == got[head]:
                    head += 1
                rest = got[head:].replace('I0 ', '').replace(']', '').strip()
                if rest and not got[:head].endswith(' '):
                    rest = got[got.rfind(' ', 0, head) + 1:].replace('I0 ', '').replace(']', '').strip()
                if rest:
                    raise SystemExit(f'tail seed {seed}: past the data the port holds more than empty items: {got[head:head + 200]!r}')
                cut += 1
    return same, cut, odd


def seen_in_the_original():
    """What the original's reader does with the hand-edit faults the port stops at, seen here rather than read
    from its code (docs/modding.md says which sentences rest on this): a word that is no node type, a list
    count no file could hold, a text length past the file. malloc answers 0 for a request over 64 MB, as a
    32-bit Windows heap would for these sizes."""
    from unicorn import UcError
    out = []

    def run(name, body, pad=b''):
        emu = Emulated(body + pad)
        asked = []

        def malloc(c):
            n = c.arg(0)
            asked.append(n)
            return 0 if n > (64 << 20) else c.alloc(max(n, 1))
        emu.cpu.stub_import(IAT_MALLOC, malloc)
        try:
            emu.load(0)
            what = f'returns having read to byte {emu.pos} of {len(body)}'
        except UcError as e:
            what = f'faults ({e})'
        big = [n for n in asked if n > (64 << 20)]
        out.append(f'{name}: the original {what}' + (f'; it asked for {big[0]:,} bytes, refused' if big else ''))
        return what

    p = struct.pack
    # kind 3: (list of 3: a word 9, int 6), then another entry's (list of 2: int 7)
    w = run('a word that is no node type', p('<ii', 4, 3) + p('<i', 9) + p('<ii', 1, 6), p('<iiii', 4, 2, 1, 7))
    assert 'returns' in w, w  # it reads on, out of step: the 9 one item, then "1" as a type...
    # kind 4: a list of a billion; a list of -1233
    assert 'faults' in run('a list count of 1000000000', p('<ii', 4, 1000000000) + p('<ii', 1, 7))
    assert 'faults' in run('a list count of -1233', p('<ii', 4, -1233) + p('<ii', 1, 7))
    # kind 5: a text said to be a megabyte long in a 13-byte file; one said to be 2 GB
    run('a text said to be 1048576 bytes long', p('<ii', 4, 2) + p('<iI', 3, 1 << 20) + b'x')
    run('a text said to be 2147483647 bytes long', p('<ii', 4, 2) + p('<iI', 3, 0x7fffffff) + b'x')
    return out


def main():
    if not READER.exists():
        raise SystemExit(f'needs {READER}')
    data = READER.read_bytes()
    emu = Emulated(data)
    buf = ctypes.create_string_buffer(8 << 20)
    files = keys = past = lists = 0
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
            if not (hit in past_end or empty_list):
                got_l, want_l = emu.lists(key), port_lists(key)
                if got_l != want_l:
                    raise SystemExit(f'{name}: key {key!r}: lists original {got_l} port {want_l}')
                lists += 1
            keys += 1
        files += 1
    for line in seen_in_the_original():
        print('seen: ' + line)
    same, cut, odd = tails(buf)
    print(f'ok: {same} made-up entries with a list longer than the entry read identically through the archive '
          f'({odd} with a word that is no node type); {cut} whose read leaves the archive\'s data: the same start, the rest empty')
    print(f'ok: {files} files, identical trees; {keys} key lookups agree '
          f'({past} past-the-end reads compared on find only); '
          f'list and boolean getters agree on {lists} keys')


if __name__ == '__main__':
    main()
