"""The ported world texture loading against the original x86
(docs/drafts/bake-textures.md).

    cmake --build build && .venv/bin/python tests/differential/test_texload.py

For each run (a world, its texture variant, mech detail, Direct3D or software
with the libraries' r prefix to match): the original reads the world's texture
directory (0x518160), registers the libraries as mission load does (0x5182a0
"texture" and "mechtex"/"mechtexS", 0x5184c0 "image"), then loads the group
(0x519080) with the C runtime's files and memory served from Python and
DirectDraw stubbed as test_drawtable.py does; texture records come from the
original 0x57b130. The port does the same on a recording platform::Render3D.
Compared: the whole table after (names, states, groups, mip links, which image
each entry got and its fields, the record's type and address modes), the
library list left current, the texture-memory count, the reports, and every
upload in order (size and the RGBA decoded from the surface the original
filled).
"""
import array
import ctypes
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import FTOL, IAT_FTOL, ROOT, Original  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
P = ctypes.c_void_p
probe.probe_texload.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p] + [ctypes.c_int] * 5 + \
    [P, P, ctypes.c_int]
probe.probe_texload_upload.argtypes = [ctypes.c_int, P, P, ctypes.c_int]

ZBD = ROOT / 'game' / 'mw3-1.2' / 'zbd'
IAT = {'fread': 0x5bb594, 'fseek': 0x5bb59c, 'fclose': 0x5bb630, 'malloc': 0x5bb664, 'calloc': 0x5bb60c,
       'realloc': 0x5bb610, 'free': 0x5bb65c, 'stricmp': 0x5bb580, 'strstr': 0x5bb5f8, 'strchr': 0x5bb63c,
       'strrchr': 0x5bb5a8, 'sprintf': 0x5bb658}
NEW, EXISTS, OPEN_PATH, ZERR, D3DERR = 0x5abd26, 0x5187a0, 0x5763b0, 0x49bfd0, 0x580550
TEXDIR, NEW_LIB, IMAGE_LIB, LOAD_GROUP, CREATE = 0x518160, 0x5182a0, 0x5184c0, 0x519080, 0x57b130
TABLE, COUNT, GROUP, LISTS, CUR_LIST, MEMORY = 0x610484, 0x610480, 0x6384d4, (0x6384f0, 0x638500), 0x63851c, 0x638524
DEFAULT_IMAGE, DEFAULT_RECORD = 0x5ecbc0, 0x80931c
BIG = 0x40000000
# Runs: world, texture variant, low-detail mechs, device (1 Direct3D, 0 software).
# Display: max width, max height, caps (2 power of two only), 4444 support.
CARD = (256, 256, 2, 1)      # a Direct3D 6 era card
SMALL = (64, 128, 2, 0)      # refuses the larger textures (default records, reports), no 4444
RUNS = [('c1', 0, 0, 1, CARD), ('c1', 0, 0, 0, CARD), ('c2', 2, 1, 1, CARD), ('c3', 3, 0, 1, CARD),
        ('c3', 0, 0, 0, CARD), ('c4', 1, 0, 1, SMALL), ('c4b', 0, 1, 1, CARD), ('t1', 3, 0, 1, SMALL)]


class Fail(Exception):
    pass


def expand(p, mask):
    shift = (mask & -mask).bit_length() - 1
    bits = bin(mask).count('1')
    v = (p & mask) >> shift
    r, filled = 0, 0
    while filled < 8:
        r = (r << bits) | v
        filled += bits
    return r >> (filled - 8)


TABLES = {}


def decode(data, fmt):
    _, rm, gm, bm, am = fmt
    key = (rm, gm, bm, am)
    if key not in TABLES:
        TABLES[key] = [bytes((expand(p, rm), expand(p, gm), expand(p, bm), expand(p, am) if am else 255))
                       for p in range(65536)]
    t = TABLES[key]
    return b''.join(map(t.__getitem__, array.array('H', data)))


class Emu:
    def __init__(self, world_dir, display):
        self.display = display
        cpu = self.cpu = Original()
        cpu.import_code(IAT_FTOL, FTOL)
        cpu.fpu_control(0x7f)
        cpu.uc.mem_map(BIG, 0x20000000)
        self.next, self.sizes, self.files, self.handles = BIG, {}, {}, {}
        self.dirs = [world_dir, ZBD]
        self.reports = []
        for k, fn in (('fread', self.fread), ('fseek', self.fseek), ('fclose', lambda c: 0),
                      ('malloc', lambda c: self.alloc(c.arg(0))), ('calloc', self.calloc),
                      ('realloc', self.realloc), ('free', lambda c: 0), ('stricmp', self.stricmp),
                      ('strstr', self.strstr), ('strchr', lambda c: self.strchr(c, False)),
                      ('strrchr', lambda c: self.strchr(c, True)), ('sprintf', self.sprintf)):
            cpu.stub_import(IAT[k], fn)
        cpu.stub_function(NEW, lambda c: self.calloc_n(c.arg(0)))
        cpu.stub_function(EXISTS, lambda c: int(self.find(self.cstr(c.ecx)) is not None))
        cpu.stub_function(OPEN_PATH, self.open_path, 4)
        cpu.stub_function(ZERR, lambda c: self.reports.append((c.arg(0), c.arg(2))))
        cpu.stub_function(D3DERR, lambda c: self.fail('d3d error'), 4)
        # The libraries' two lists: sentinels pointing at themselves.
        for lst in LISTS:
            node = self.alloc(12)
            cpu.u32(node, node), cpu.u32(node + 4, node)
            cpu.u32(lst + 4, node), cpu.u32(lst + 8, 0)
        cpu.u32(0x5ecc20, 1)          # libraries enabled
        for a, v in ((0x802720, 5), (0x802724, 6), (0x802728, 5)):
            cpu.u32(a, v)
        cpu.u32(0x638514, 0)          # no light tables (hardware): no shaded palettes
        self.setup_d3d()

    # ------------------------------------------------ C runtime
    def alloc(self, n):
        a = self.next
        self.next += (max(n, 1) + 0xf) & ~0xf
        self.sizes[a] = n
        return a

    def calloc_n(self, n):
        a = self.alloc(n)
        self.cpu.write(a, bytes(n))
        return a

    def calloc(self, c):
        return self.calloc_n(c.arg(0) * c.arg(1))

    def realloc(self, c):
        old, n = c.arg(0), c.arg(1)
        a = self.alloc(n)
        if old:
            c.write(a, c.read(old, min(self.sizes.get(old, 0), n)))
        return a

    def cstr(self, a):
        out = bytearray()
        while True:
            b = self.cpu.read(a + len(out), 64)
            i = b.find(0)
            if i >= 0:
                return bytes(out + b[:i])
            out += b

    def stricmp(self, c):
        a, b = self.cstr(c.arg(0)).lower(), self.cstr(c.arg(1)).lower()
        return (a > b) - (a < b)

    def strstr(self, c):
        s, sub = self.cstr(c.arg(0)), self.cstr(c.arg(1))
        i = s.find(sub)
        return 0 if i < 0 else c.arg(0) + i

    def strchr(self, c, last):
        s, ch = self.cstr(c.arg(0)), c.arg(1) & 0xff
        i = s.rfind(bytes([ch])) if last else s.find(bytes([ch]))
        return 0 if i < 0 else c.arg(0) + i

    def sprintf(self, c):
        fmt, out, k = self.cstr(c.arg(1)), bytearray(), 2
        i = 0
        while i < len(fmt):
            if fmt[i] == 0x25 and i + 1 < len(fmt):
                f = fmt[i + 1]
                if f == ord('s'):
                    out += self.cstr(c.arg(k))
                elif f == ord('d'):
                    out += str(struct.unpack('<i', struct.pack('<I', c.arg(k)))[0]).encode()
                else:
                    raise Fail(f'sprintf %{chr(f)}')
                k += 1
                i += 2
            else:
                out.append(fmt[i])
                i += 1
        c.write(c.arg(0), bytes(out) + b'\0')
        return len(out)

    def find(self, name):
        for d in self.dirs:
            p = Path(d) / name.decode()
            if p.exists():
                return p
        return None

    def open_path(self, c):
        name = self.cstr(c.edx)
        p = self.find(name)
        if p is None:
            return 0
        h = 0x100 + len(self.files)
        self.files[h] = [p.read_bytes(), 0]
        return h

    def fread(self, c):
        buf, size, count, h = c.arg(0), c.arg(1), c.arg(2), c.arg(3)
        f = self.files[h]
        chunk = f[0][f[1]:f[1] + size * count]
        f[1] += len(chunk)
        c.write(buf, chunk)
        return len(chunk) // size if size else 0

    def fseek(self, c):
        assert c.arg(2) == 0
        self.files[c.arg(0)][1] = c.arg(1)
        return 0

    def fail(self, what):
        raise Fail(f'original: unexpected {what}')

    # ------------------------------------------------ DirectDraw (as test_drawtable.py)
    def setup_d3d(self):
        cpu = self.cpu
        dd, ddt = cpu.alloc(16), cpu.alloc(0x100)
        cpu.u32(dd, ddt)
        cpu.u32(0x809358, dd)

        def method(table, off, fn, pop):
            addr = cpu.code(b'\xc3')
            cpu.u32(table + off, addr)
            cpu.stub_function(addr, fn, pop)
        self.surf_tab, self.tex_tab = cpu.alloc(0x100), cpu.alloc(0x100)
        method(ddt, 0x18, self.create_surface, 16)
        method(ddt, 0x14, lambda c: self.fail('CreatePalette'), 20)
        method(self.surf_tab, 0x00, self.query, 12)
        method(self.surf_tab, 0x08, lambda c: 0, 4)
        method(self.surf_tab, 0x64, self.lock, 20)
        method(self.surf_tab, 0x80, self.unlock, 8)
        method(self.surf_tab, 0x7c, lambda c: self.fail('SetPalette'), 8)
        method(self.tex_tab, 0x08, lambda c: 0, 4)
        method(self.tex_tab, 0x14, lambda c: 0, 8)
        self.caps, self.dev = cpu.alloc(0x200), cpu.alloc(0xc00)
        cpu.u32(0x80c278, self.caps)
        cpu.u32(0x809644, self.dev)
        self.default_record = cpu.alloc(0x18)
        cpu.u32(DEFAULT_RECORD, self.default_record)
        cpu.u32(0x802718, 16)
        cpu.write(0x80272c, struct.pack('<3I', 0xf800, 0x07e0, 0x001f))
        d = self.display
        cpu.write(self.caps + 0x148, struct.pack('<II', d[0], d[1]))
        cpu.u32(0x809448, d[2])
        cpu.u32(self.dev + 0xb04, d[3])
        cpu.u32(0x72f364, CREATE)
        self.surfaces, self.uploads, self.upload_of = {}, [], {}

    def new_obj(self, tab):
        a = self.alloc(16)
        self.cpu.u32(a, tab)
        return a

    def create_surface(self, c):
        desc = c.arg(1)
        h, w = struct.unpack('<II', c.read(desc + 8, 8))
        fmt = struct.unpack('<5I', c.read(desc + 0x48 + 0xc, 20))
        s = self.new_obj(self.surf_tab)
        self.surfaces[s] = [w, h, fmt, 0]
        self.cpu.u32(c.arg(2), s)
        return 0

    def query(self, c):
        self.cpu.u32(c.arg(2), self.new_obj(self.tex_tab))
        return 0

    def lock(self, c):
        s, desc = c.arg(0), c.arg(2)
        w, h, fmt, mem = self.surfaces[s]
        if not mem:
            mem = self.surfaces[s][3] = self.alloc(2 * w * h)
            self.cpu.write(mem, b'\xee' * (2 * w * h))
        self.cpu.u32(desc + 0x10, 2 * w)
        self.cpu.u32(desc + 0x24, mem)
        return 0

    def unlock(self, c):
        s = c.arg(0)
        w, h, fmt, mem = self.surfaces[s]
        self.upload_of[s] = len(self.uploads)
        self.uploads.append((w, h, decode(self.cpu.read(mem, 2 * w * h), fmt)))
        return 0

    # ------------------------------------------------ the run
    def run(self, gamez, variant, low_detail, device, group):
        cpu = self.cpu
        h = 0x80
        self.files[h] = [gamez, struct.unpack_from('<I', gamez, 0x0c)[0]]
        cpu.u32(COUNT, 0), cpu.u32(GROUP, 0)
        cpu.call(TEXDIR, ecx=struct.unpack_from('<I', gamez, 8)[0], edx=h)
        names = []
        for n in (b'texture', b'mechtexS' if low_detail else b'mechtex'):
            p = self.alloc(16)
            cpu.write(p, n + b'\0')
            names.append(p)
        hardware = int(device != 0)
        cpu.call(NEW_LIB, ecx=names[0], edx=variant, stack_args=(hardware,))
        cpu.call(NEW_LIB, ecx=names[1], edx=0, stack_args=(hardware,))
        cpu.call(IMAGE_LIB, ecx=1)
        cpu.u32(0x72f348, device)
        cpu.call(LOAD_GROUP, ecx=group & 0xffffffff, count=4_000_000_000)
        return self.dump()

    def i32(self, a):
        return struct.unpack('<i', self.cpu.read(a, 4))[0]

    def dump(self):
        cpu, i32 = self.cpu, self.i32
        count = i32(COUNT)
        b = struct.pack('<iiii', count, i32(CUR_LIST), i32(MEMORY), len(self.reports))
        b += b''.join(struct.pack('<ii', *r) for r in self.reports)
        first = {}
        self.stats = {'default images': 0, 'default records': 0, 'mips': 0}
        for i in range(count):
            e = cpu.read(TABLE + 0x28 * i, 0x28)
            img, rec = struct.unpack_from('<II', e, 0)
            state, grp, mip = struct.unpack_from('<iiI', e, 0x1c)
            b += e[8:0x1c] + struct.pack('<ii', state, grp)
            b += struct.pack('<i', -1 if mip == 0 else (mip - TABLE) // 0x28)
            if img == 0:
                kind = -1
            elif img == DEFAULT_IMAGE:
                kind = -2
                self.stats['default images'] += 1
            else:
                kind = first.setdefault(img, i)
            b += struct.pack('<i', kind)
            r = struct.unpack('<6I', cpu.read(rec, 0x18)) if rec and rec != self.default_record else None
            self.stats['default records'] += rec == self.default_record
            self.stats['mips'] += mip != 0
            b += struct.pack('<i', -1 if rec == 0 else -2 if rec == self.default_record else self.upload_of[r[0]])
            if img:
                t = cpu.read(img, 0x38)
                b += t[0:0x10] + t[0x1c:0x20] + t[0x24:0x30] + t[0x34:0x38]
                b += bytes(int(struct.unpack_from('<I', t, o)[0] != 0) for o in (0x10, 0x14, 0x18))
            if r:
                b += struct.pack('<3i', r[3], r[4], r[5])
        return b


def describe(a, b):
    """Where two table dumps first differ, by entry."""
    if a[:16] != b[:16]:
        return f'header {struct.unpack_from("<4i", a)} vs {struct.unpack_from("<4i", b)}'
    k = next(i for i in range(min(len(a), len(b))) if a[i] != b[i]) if a != b[:len(a)] else min(len(a), len(b))
    return f'first difference at byte {k}: {a[max(0, k - 24):k + 24].hex()} vs {b[max(0, k - 24):k + 24].hex()}'


def main():
    buf = ctypes.create_string_buffer(1 << 22)
    info = (ctypes.c_int32 * 3)()
    rgba = ctypes.create_string_buffer(4096 * 4096 * 4)
    total = 0
    try:
        for world, variant, low, device, display in RUNS:
            wdir = ZBD / world
            gamez = (wdir / 'gamez.zbd').read_bytes()
            emu = Emu(wdir, display)
            want = emu.run(gamez, variant, low, device, 0)
            n = probe.probe_texload(str(wdir / 'gamez.zbd').encode(), str(wdir).encode(), str(ZBD).encode(),
                                    variant, low, int(device != 0), 0, device,
                                    (ctypes.c_int32 * 4)(*display), buf, len(buf))
            if n < 0:
                raise Fail(f'{world}: probe_texload {n}')
            got = buf.raw[:n]
            if want != got:
                raise Fail(f'{world} variant {variant} device {device}: table {describe(want, got)}')
            ups = probe.probe_texload_uploads()
            if ups != len(emu.uploads):
                raise Fail(f'{world}: {len(emu.uploads)} uploads vs {ups}')
            for i, (w, h, px) in enumerate(emu.uploads):
                m = probe.probe_texload_upload(i, info, rgba, len(rgba))
                if (w, h) != (info[0], info[1]) or px != rgba.raw[:m]:
                    k = next((j for j in range(0, min(len(px), m), 4) if px[j:j + 4] != rgba.raw[j:j + 4]), -1)
                    raise Fail(f'{world} upload {i}: {w}x{h} vs {info[0]}x{info[1]}, texel {k // 4}')
            count, cur, mem, nrep = struct.unpack_from('<4i', want)
            extra = ', '.join(f'{v} {k}' for k, v in emu.stats.items())
            print(f'{world} variant {variant}{" low" if low else ""} {"d3d" if device else "software"} {display}: '
                  f'{count} entries, {len(emu.uploads)} uploads, {mem} bytes, {nrep} reports, {extra}, '
                  f'list {cur}: identical')
            total += 1
    except Fail as e:
        print('FAIL', e)
        sys.exit(1)
    print(f'texload: {total} runs identical')


if __name__ == '__main__':
    main()
