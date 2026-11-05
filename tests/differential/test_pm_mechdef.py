"""PM's mech definition reader 0x46d690 whole (with the ctor 0x46d3e0) against mech3/mech_reader, on all 25 dfn_*.zrd of
reader.zbd (docs/pm-map.md, docs/mechdef-head.md): the head's fields, the headlight's config, every loadout with its items,
every firepoint and every report.

    cmake --build build --target pmprobe
    .venv/bin/python -P tests/differential/test_pm_mechdef.py

Saved loadouts (0x4267f0) are stubbed out on both sides; the bullet-hole textures are not fetched.
"""
import ctypes
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_pm_zar  # noqa: E402
import test_pm_zreader as zr  # noqa: E402
from original import ROOT  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EIP, UC_X86_REG_ESP  # noqa: E402

CTOR, READ, LOAD_SAVED, REPORT, FETCH, SIZE = 0x46d3e0, 0x46d690, 0x4267f0, 0x406cb0, 0x5315c0, 0x1ac
IAT_FLOOR = 0x5bb66c  # MW3's, the harness translates it
FLOOR = bytes.fromhex('83ec04 d93c24 668b0424 6625fff3 660d0004 6689442402 d96c2402 dd442408 d9fc d92c24 83c404 c3')
VT = {0x5e36b4: 1, 0x5dfc40: 2, 0x5dfc70: 3}
MECHS = ['annihilator', 'atlas', 'avatar', 'awesome', 'blackhawk', 'bushwacker', 'cauldron', 'centurion', 'champion', 'clint',
         'daishi', 'elemental', 'firefly', 'madcat', 'masakari', 'orion', 'owens', 'puma', 'ryoken', 'shadowcat', 'strider',
         'sunder', 'supernova', 'thor', 'vulture']


class Emulated(zr.Emulated):
    def __init__(self):
        super().__init__()
        self.reports = []
        self.native_stub(LOAD_SAVED, lambda c: 0)
        self.native_stub(REPORT, self.report)
        self.native_stub(FETCH, lambda c: 0)
        self.native_stub(0x427360, lambda c: 1, 4)  # validate: it needs the weapons' data, seamed on both sides
        for addr, fn in ((0x5ce8e4, self.cstring_new), (0x5ce8d8, lambda c: 0), (0x5ce8de, self.cstring_format)):
            self.native_stub(addr, fn)

    def w32(self, a, v):
        self.cpu._uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def cstring_new(self, c):
        self.w32(c.ecx, self.alloc(256))
        return c.ecx

    def cstring_format(self, c):  # only the headlight's "light%d" reads what it makes
        s = self.cstr(c.arg(1)).decode('latin1').replace('%d', str(c.arg(2)))[:200]
        self.cpu._uc.mem_write(self.r32(c.ecx), s.encode() + b'\0')
        return 0

    def report(self, c):
        self.reports.append((c.arg(0), c.arg(2)))
        return 0

    def name(self, p):
        return b'\xff' if p == 0 else self.cstr(p) + b'\0'

    def skin(self, s, out):
        u = self.r32
        out.append(s != 0)
        if not s:
            return
        out += self.name(u(s)) + self.raw(s + 0x14, 1)
        first, last = u(s + 8), u(s + 0xc)
        out += struct.pack('<i', (last - first) // 4)
        for k in range((last - first) // 4):
            e = u(first + 4 * k)
            tex, cached, name, alt, dmg, b14, refs = struct.unpack('<IB3xIIIB3xi', self.raw(e, 0x1c))
            out += self.name(name) + bytes([alt == name])
            if alt != name:
                out += self.name(alt)
            out += bytes([cached, dmg == 0, b14]) + struct.pack('<i', refs) + bytes([tex == 0])

    def raw(self, a, n):
        return bytes(self.cpu._uc.mem_read(a, n))

    def dump(self, dfn):
        u, i = self.r32, self.i32
        out = bytearray(self.raw(dfn + 0x10, 0xc) + self.raw(dfn + 0x148, 0x60))
        cl = u(dfn + 0x20)
        if cl:
            first, last = u(cl + 0xc), u(cl + 0x10)
            out += struct.pack('<iii', i(cl), i(cl + 4), (last - first) // 4)
            for k in range((last - first) // 4):
                c = u(first + 4 * k)
                raw = self.raw(c, 0xd8)
                out += self.name(u(c + 0x18))
                out += bytes(raw[o] for o in (0x10, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x28, 0x34, 0x6c))
                out += raw[0x14:0x18] + raw[0x24:0x28] + raw[0x2c:0x34] + raw[0x38:0x64] + raw[0x64:0x6c]
                out += raw[0x74:0x78] + raw[0x7c:0x88]
                sentinel, items, node = u(c + 8), [], None
                node = u(sentinel)
                while node != sentinel:
                    items.append(u(node + 8))
                    node = u(node)
                out += struct.pack('<i', i(c + 0xc))
                for it in items:
                    kind = VT.get(u(it))
                    if kind is None:
                        raise SystemExit(f'item vtable {u(it):#x}')
                    out += struct.pack('<iiii', kind, i(it + 4), i(it + 8), i(it + 0xc)) + self.raw(it + 0x10, 1)
                    out += {1: self.raw(it + 0x14, 8), 2: self.raw(it + 0x14, 4), 3: self.raw(it + 0x18, 8)}[kind]  # PM's equipment: +0x14 unused
        else:
            out += struct.pack('<i', -1)
        vec = u(dfn + 0x24)
        if vec:
            first, last = u(vec + 4), u(vec + 8)
            out += struct.pack('<i', (last - first) // 4)
            for k in range((last - first) // 4):
                f = u(first + 4 * k)
                out += self.name(u(f + 0x5c)) + self.raw(f + 0x60, 12) + self.raw(f, 0x14)
        else:
            out += struct.pack('<i', -1)
        for k in range(18):
            o = dfn + 0x28 + 0x10 * k
            out += self.name(u(o)) + self.raw(o + 4, 12)
        out += self.raw(u(dfn + 0x1a8), 0x3d)
        s = u(dfn + 0x1c)
        out.append(s != 0)
        if s:
            first, last = u(s + 4), u(s + 8)
            out += struct.pack('<i', (last - first) // 4)
            for k in range((last - first) // 4):
                self.skin(u(first + 4 * k), out)
            self.skin(u(s + 0x10), out)
        out += struct.pack('<i', len(self.reports)) + b''.join(struct.pack('<ii', *r) for r in self.reports)
        return bytes(out)


def main():
    rel = 'zbd/reader.zbd'
    data = (ROOT / 'game' / 'pm' / rel).read_bytes()
    emu = Emulated()
    emu.files[test_pm_zar.key(rel)] = bytearray(data)
    emu.cpu.import_code(IAT_FLOOR, FLOOR)
    if not emu.call(zr.OPEN_ARCHIVE, args=[emu.string(rel.replace('/', '\\')), 1]):
        raise SystemExit('PM does not open reader.zbd')
    emu.call(0x475860)  # the skins' registry (MW3's 0x473ae0)
    p = test_pm_zar.load_pm_probe()
    p.probe_pm_dfn.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_int]
    buf = ctypes.create_string_buffer(1 << 22)
    bad = configs = near = 0
    for index, mech in enumerate(MECHS):
        name = f'dfn_{mech}.zrd'
        emu.reports = []
        root = emu.call(zr.LOAD, args=[emu.string(name), 0, 0])
        dfn = emu.alloc(SIZE)
        emu.call(CTOR, ecx=dfn, args=[index, root])
        try:
            ok = emu.call(READ, ecx=dfn) & 0xff
        except Exception as ex:
            uc = emu.cpu._uc
            raise SystemExit(f'{name}: eip {uc.reg_read(UC_X86_REG_EIP):#x} ret {emu.r32(uc.reg_read(UC_X86_REG_ESP)):#x}: {ex}')
        want = bytes([ok, emu.raw(dfn + 4, 1)[0]]) + emu.dump(dfn)
        n = p.probe_pm_dfn(str(ROOT / 'game' / 'pm').encode(), name.encode(), index, buf, len(buf))
        got = buf.raw[:max(n, 0)]
        # The head's floats within a few units in the last place (the x87's sine, CLAUDE.md); all else exact.
        H0, H1 = 2 + 0xc, 2 + 0xc + 0x60
        if len(got) == len(want):
            for o in range(H0, H1, 4):
                a_, b_ = struct.unpack_from('<i', want, o)[0], struct.unpack_from('<i', got, o)[0]
                if a_ != b_ and abs(a_ - b_) <= 4:
                    near += 1
                    got = got[:o] + want[o:o + 4] + got[o + 4:]
        if got != want:
            bad += 1
            at = next((i for i in range(min(len(got), len(want))) if got[i] != want[i]), min(len(got), len(want)))
            print(f'{name}: exe {len(want)} bytes, port {n}, first difference at {at}:\n  exe  {want[max(at-16,0):at+24].hex()}\n  port {got[max(at-16,0):at+24].hex()}')
        else:
            configs += struct.unpack_from('<i', want, 2 + 0x6c + 8)[0]
    print(f'{len(MECHS) - bad}/{len(MECHS)} definitions identical, {configs} loadouts, {near} head floats within 4 places')
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    code = 0
    try:
        main()
    except SystemExit as ex:
        if isinstance(ex.code, str):
            print(ex.code)
        code = 0 if ex.code in (None, 0) else 1
    except BaseException:
        import traceback
        traceback.print_exc()
        code = 1
    sys.stdout.flush()
    os._exit(code)
