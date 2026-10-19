"""The ported mech definition pieces against the original x86 on every
dfn_*.zrd in reader.zbd (docs/mechdef-skins.md). So far: the skins block.

    cmake --build build && .venv/bin/python tests/differential/test_mechdef.py

The original's skins block (0x46be07..0x46bfd8 inside the mech reader
0x46ba40) runs on a tree read by the original zReader, with EBP pointing at a
blank mech definition; the port's read_skins runs on the port's tree. Each
file runs four ways: with and without the localized camo names, and with
and without the damage mode that fetches the bullet-hole textures (read
twice there, since only the first set fetches them).
"""
import ctypes
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import STACK_TOP  # noqa: E402
from test_zreader import READER, Emulated as ReaderEmu, entries, probe  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EBP, UC_X86_REG_ESP  # noqa: E402

probe.probe_skins.argtypes = [ctypes.c_char_p, ctypes.c_long, ctypes.c_int, ctypes.c_int,
                              ctypes.c_void_p, ctypes.c_int]
SKINS_BLOCK, SKINS_END = 0x46be07, 0x46bfd8
IAT_NEW, IAT_DELETE, IAT_STRDUP = 0x5bb4bc, 0x5bb4c8, 0x5bb670
LIST_INIT, ENTRY_LIST, ENTRY_COUNT, HOLES, SETS, DAMAGE_MODE = 0x473ae0, 0x6061bc, 0x6061c0, 0x6061a8, 0x6061c8, 0x6061cc
MSG_HOOK, MSG_TEXT, FETCH = 0x72c670, 0x576040, 0x5179d0
KEYS = ['MSG_SKIN_PRIMARY'] + [f'MSG_SKIN_VARIANT{c}' for c in 'BCDEFGH']
NAMES = ['GUNMETAL', 'SUNFIRE', 'DESERT', 'WASP', 'SLATE', 'JUNGLE', 'ARCTIC', 'SPECTRE']


class Emulated(ReaderEmu):
    def __init__(self, data):
        super().__init__(data)
        cpu = self.cpu
        cpu.stub_import(IAT_NEW, lambda c: c.alloc(max(c.arg(0), 1)))
        cpu.stub_import(IAT_DELETE, lambda c: 0)
        cpu.stub_import(IAT_STRDUP, self.strdup)
        self.names = {}
        for i, n in enumerate(NAMES):
            p = cpu.alloc(len(n) + 1)
            cpu.write(p, n.encode() + b'\0')
            self.names[0xf10 + i] = p
        self.msg_hook = cpu.code(b'\xc3')
        cpu.stub_function(self.msg_hook, self.get_id)
        cpu.stub_function(MSG_TEXT, lambda c: self.names[c.ecx])
        cpu.stub_function(FETCH, self.fetch)
        self.base_heap = cpu.heap_next  # load() rewinds the heap to here

    def strdup(self, c):
        s = self.cstr(c.arg(0))
        p = c.alloc(len(s) + 1)
        c.write(p, s + b'\0')
        return p

    def get_id(self, c):
        s = self.cstr(c.arg(0)).decode('latin-1')
        return 0xf10 + KEYS.index(s) if s in KEYS else 0

    def fetch(self, c):
        k = self.fetches
        self.fetches += 1
        return 0 if self.cstr(c.ecx) == b'hole_8x8b' else 0x60000000 + 0x10 * k

    def skins(self, offset, localized, damage_mode):
        cpu, u = self.cpu, self.u32
        self.load(offset)
        self.fetches = 0
        for a in (ENTRY_COUNT, SETS, HOLES, HOLES + 4, HOLES + 8, HOLES + 12):
            cpu.u32(a, 0)
        cpu.write(DAMAGE_MODE, bytes([damage_mode]))
        cpu.u32(MSG_HOOK, self.msg_hook if localized else 0)
        cpu.call(LIST_INIT)
        for _ in range(2 if damage_mode else 1):  # holes are fetched by the first set only
            dfn = cpu.alloc(0x1a8)
            cpu.write(dfn, bytes(0x1a8))
            cpu.u32(dfn + 8, self.root)
            cpu.uc.reg_write(UC_X86_REG_EBP, dfn)
            cpu.uc.reg_write(UC_X86_REG_ESP, STACK_TOP - 0x2000)
            cpu.uc.emu_start(SKINS_BLOCK, SKINS_END, count=50_000_000)
        out = bytearray()
        s = u(dfn + 0x1c)
        out.append(s != 0)
        if s:
            first, last = u(s + 4), u(s + 8)
            out += struct.pack('<i', (last - first) // 4)
            for k in range((last - first) // 4):
                self.skin(u(first + 4 * k), out)
            self.skin(u(s + 0x10), out)
        out += struct.pack('<ii', self.i32(ENTRY_COUNT), self.i32(SETS))
        sentinel, names = u(ENTRY_LIST), []
        node = u(sentinel)
        while node != sentinel:
            names.append(self.name(u(u(node + 8) + 8)))
            node = u(node)
        out += struct.pack('<i', len(names)) + b''.join(names)
        first, last = u(HOLES + 4), u(HOLES + 8)
        out += struct.pack('<i', (last - first) // 4) + self.cpu.read(first, last - first) if first else struct.pack('<i', 0)
        return bytes(out)

    def i32(self, a):
        return struct.unpack('<i', self.cpu.read(a, 4))[0]

    def name(self, p):
        return b'\xff' if p == 0 else self.cstr(p) + b'\0'

    def skin(self, s, out):
        u = self.u32
        out.append(s != 0)
        if not s:
            return
        out += self.name(u(s)) + self.cpu.read(s + 0x14, 1)
        first, last = u(s + 8), u(s + 0xc)
        out += struct.pack('<i', (last - first) // 4)
        for k in range((last - first) // 4):
            e = u(first + 4 * k)
            raw = self.cpu.read(e, 0x1c)
            tex, cached, name, alt, dmg, b14, refs = struct.unpack('<IB3xIIIB3xi', raw)
            out += self.name(name) + bytes([alt == name])
            if alt != name:
                out += self.name(alt)
            out += bytes([cached, dmg == 0, b14]) + struct.pack('<i', refs) + bytes([tex == 0])


def main():
    data = READER.read_bytes()
    emu = Emulated(data)
    buf = ctypes.create_string_buffer(1 << 20)
    files = 0
    for name, start, _ in entries(data):
        if not name.startswith('dfn_'):
            continue
        for localized in (0, 1):
            for damage_mode in (0, 1):
                want = emu.skins(start, localized, damage_mode)
                n = probe.probe_skins(str(READER).encode(), start, localized, damage_mode, buf, len(buf))
                got = buf.raw[:max(n, 0)]
                if got != want:
                    at = next((i for i in range(min(n, len(want))) if got[i] != want[i]), None)
                    raise SystemExit(f'{name} localized={localized} damage={damage_mode}: original '
                                     f'{len(want)} bytes, port {n}, first difference at {at}')
        files += 1
    print(f'ok: skins of {files} mech definitions identical, four ways each')


if __name__ == '__main__':
    main()
