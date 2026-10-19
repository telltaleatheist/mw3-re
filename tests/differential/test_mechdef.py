"""The ported mech definition pieces against the original x86 on every
dfn_*.zrd in reader.zbd (docs/mechdef-skins.md, docs/mechdef-parts.md). So far:
the MechDfn constructor and the skins, firepoints and part-slot blocks.

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
from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBP, UC_X86_REG_EIP, UC_X86_REG_ESP  # noqa: E402

probe.probe_firepoints.argtypes = [ctypes.c_char_p, ctypes.c_long, ctypes.c_void_p, ctypes.c_int]
probe.probe_part_id.argtypes = [ctypes.c_char_p]
probe.probe_part_name.restype = ctypes.c_char_p
DFN_CTOR, FIREPOINTS_BLOCK, FIREPOINTS_END, PART_ID, PART_NAME = 0x46b890, 0x46bfd8, 0x46c1f0, 0x484e40, 0x484cc0
PART_NAMES = ['hip', 'torso', 'larm', 'rarm', 'lhand', 'rhand', 'lthigh', 'rthigh', 'lleg', 'rleg', 'lcalf',
              'rcalf', 'lfoot', 'rfoot', 'head', 'ltorso', 'rtorso', 'none', 'Torso', 'lar', '']
probe.probe_dfn_head.argtypes = [ctypes.c_char_p, ctypes.c_long, ctypes.c_int, ctypes.c_void_p, ctypes.c_int]
HEAD_BLOCK, HEAD_END, HEAD_FAIL = 0x46ba95, 0x46be07, 0x46cb60
MECHS = ['annihilator', 'avatar', 'blackhawk', 'bushwacker', 'cauldron', 'champion', 'daishi', 'elemental',
         'firefly', 'madcat', 'orion', 'owens', 'puma', 'shadowcat', 'strider', 'sunder', 'supernova', 'thor',
         'vulture']
probe.probe_dfn_configs.argtypes = [ctypes.c_char_p, ctypes.c_long, ctypes.c_int, ctypes.c_int, ctypes.c_void_p,
                                    ctypes.c_int]
CONFIGS_END, NO_CONFIGS = 0x46cb3c, 0x46cb44
IAT_STRNCMP, ZERR = 0x5bb654, 0x49bfd0
VT_KIND = {0x5c0fc0: 1, 0x5bd150: 2, 0x5bd180: 3}
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
            self.run_range(SKINS_BLOCK, SKINS_END)
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

    def firepoints(self, offset):
        """The MechDfn ctor, then the firepoints and part-slot blocks."""
        cpu, u = self.cpu, self.u32
        self.load(offset)
        dfn = cpu.alloc(0x1a8)
        cpu.write(dfn, bytes(0x1a8))
        cpu.call(DFN_CTOR, ecx=dfn, stack_args=(7, self.root))
        cpu.uc.reg_write(UC_X86_REG_EBP, dfn)
        cpu.uc.reg_write(UC_X86_REG_ESP, STACK_TOP - 0x2000)
        self.run_range(FIREPOINTS_BLOCK, FIREPOINTS_END)
        image = bytearray(cpu.read(dfn, 0x1a8))
        names = []
        for o in [8, 0x1c, 0x20, 0x24] + [0x28 + 0x10 * i for i in range(18)]:
            if o >= 0x28:
                names.append(self.name(u(dfn + o)))
            image[o:o + 4] = bytes(4)
        out = bytes(image) + b''.join(names)
        vec = u(dfn + 0x24)
        first, last = u(vec + 4), u(vec + 8)
        out += struct.pack('<i', (last - first) // 4)
        for k in range((last - first) // 4):
            f = u(first + 4 * k)
            out += self.name(u(f)) + cpu.read(f + 4, 12)
        return out

    def run_range(self, begin, *ends):
        """Run a code range of the reader until any of `ends`. Stop hooks, not
        emu_start's end address: Unicorn misses that inside a block translated
        by an earlier run (each block starts where the one before ends)."""
        if not hasattr(self, 'ends'):
            def stop(uc, address, size, user):
                if address in self.ends:
                    uc.emu_stop()
            self.cpu.uc.hook_add(UC_HOOK_CODE, stop, begin=0x46ba40, end=0x46cb80)
        self.ends = set(ends)
        self.cpu.uc.emu_start(begin, 0, count=100_000_000)
        self.ends = set()

    def configs(self, offset, index, localized):
        """The ctor, then 0x46ba95 through the loadouts (0x46cb3c)."""
        from original import FTOL, IAT_FTOL
        cpu, u, i32 = self.cpu, self.u32, self.i32
        if not getattr(self, 'config_stubs', False):
            cpu.import_code(IAT_FTOL, FTOL)
            cpu.import_code(IAT_FLOOR, FLOOR)
            cpu.stub_import(IAT_STRNCMP, self.strncmp)
            cpu.stub_function(ZERR, lambda c: self.reports.append((c.arg(0), c.arg(2))))
            self.config_stubs = True
        self.load(offset)
        self.reports = []
        self.fetches = 0
        for a in (ENTRY_COUNT, SETS, HOLES, HOLES + 4, HOLES + 8, HOLES + 12):
            cpu.u32(a, 0)
        cpu.write(DAMAGE_MODE, b'\0')
        cpu.u32(MSG_HOOK, self.msg_hook if localized else 0)
        cpu.call(LIST_INIT)
        dfn = cpu.alloc(0x1a8)
        cpu.write(dfn, bytes(0x1a8))
        cpu.call(DFN_CTOR, ecx=dfn, stack_args=(index, self.root))
        esp = STACK_TOP - 0x2000
        cpu.write(esp, bytes(0xc0))
        cpu.u32(esp + 0x3c, dfn)
        cpu.uc.reg_write(UC_X86_REG_EBP, dfn)
        cpu.uc.reg_write(UC_X86_REG_ESP, esp)
        self.run_range(HEAD_BLOCK, CONFIGS_END, NO_CONFIGS, HEAD_FAIL)
        eip = cpu.uc.reg_read(UC_X86_REG_EIP)
        out = bytearray([{CONFIGS_END: 1, NO_CONFIGS: 0, HEAD_FAIL: 0xff}[eip]])
        cl = u(dfn + 0x20)
        if cl:
            first, last = u(cl + 0xc), u(cl + 0x10)
            out += struct.pack('<iii', i32(cl), i32(cl + 4), (last - first) // 4)
            for k in range((last - first) // 4):
                c = u(first + 4 * k)
                raw = cpu.read(c, 0xd8)
                out += self.name(u(c + 0x18))
                out += bytes(raw[o] for o in (0x10, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x28, 0x34, 0x6c))
                out += raw[0x14:0x18] + raw[0x24:0x28] + raw[0x2c:0x34] + raw[0x38:0x64] + raw[0x64:0x6c]
                out += raw[0x74:0x78] + raw[0x7c:0x88]
                sentinel = u(c + 8)
                items, node = [], u(sentinel)
                while node != sentinel:
                    items.append(u(node + 8))
                    node = u(node)
                out += struct.pack('<i', i32(c + 0xc))
                assert i32(c + 0xc) == len(items)
                for it in items:
                    kind = VT_KIND[u(it)]
                    out += struct.pack('<iiii', kind, i32(it + 4), i32(it + 8), i32(it + 0xc)) + cpu.read(it + 0x10, 1)
                    out += cpu.read(it + 0x14, 8) if kind == 1 else cpu.read(it + 0x14, 4) if kind == 2 else cpu.read(it + 0x18, 8)
        else:
            out += struct.pack('<i', -1)
        vec = u(dfn + 0x24)
        if vec:
            first, last = u(vec + 4), u(vec + 8)
            out += struct.pack('<i', (last - first) // 4)
            for k in range((last - first) // 4):
                f = u(first + 4 * k)
                out += self.name(u(f)) + cpu.read(f + 4, 12)
        else:
            out += struct.pack('<i', -1)
        out += struct.pack('<i', len(self.reports)) + b''.join(struct.pack('<ii', *r) for r in self.reports)
        return bytes(out)

    def strncmp(self, c):
        n = c.arg(2)
        a, b = self.cstr(c.arg(0))[:n], self.cstr(c.arg(1))[:n]
        return (a > b) - (a < b)

    def head(self, offset, index):
        """The MechDfn ctor, then the head 0x46ba95..0x46be07 (stops early at
        0x46cb60 on a bad version)."""
        cpu = self.cpu
        self.load(offset)
        dfn = cpu.alloc(0x1a8)
        cpu.write(dfn, bytes(0x1a8))
        cpu.call(DFN_CTOR, ecx=dfn, stack_args=(index, self.root))
        esp = STACK_TOP - 0x2000
        cpu.write(esp, bytes(0xc0))
        cpu.u32(esp + 0x3c, dfn)
        cpu.uc.reg_write(UC_X86_REG_EBP, dfn)
        cpu.uc.reg_write(UC_X86_REG_ESP, esp)
        self.run_range(HEAD_BLOCK, HEAD_END, HEAD_FAIL)
        ok = cpu.uc.reg_read(UC_X86_REG_EIP) == HEAD_END
        image = cpu.read(dfn, 0x1a8)
        out = bytes([ok]) + image[0xc:0x1c] + image[0x148:0x1a8]
        if ok:
            out += cpu.read(esp + 0x44, 4) + cpu.read(esp + 0x54, 4) + cpu.read(esp + 0x58, 12)
        return out

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


# MSVCRT floor(double) in st0: round toward -inf through the control word.
FLOOR = bytes.fromhex('83ec04 d93c24 668b0424 6625fff3 660d0004 6689442402 d96c2402 dd442408 d9fc d92c24 83c404 c3')
IAT_FLOOR = 0x5bb66c
HELPERS = {'mech_tech': 0x484720, 'mech_name': 0x484650, 'tech_id': 0x4852b0, 'equip_kind': 0x4849a0,
           'weapon_index': 0x4848a0, 'engine_index': 0x432e30, 'armor_points': 0x424760}


def check_helpers(emu):
    """The table helpers over every index, name (with case variants and junk),
    rating and armor tonnage."""
    from original import FTOL, IAT_FTOL
    cpu = emu.cpu
    cpu.import_code(IAT_FTOL, FTOL)
    cpu.import_code(IAT_FLOOR, FLOOR)
    eax = lambda: struct.unpack('<i', struct.pack('<I', cpu.uc.reg_read(UC_X86_REG_EAX)))[0]
    for fn in ('mech_tech', 'tech_id', 'equip_kind', 'weapon_index', 'engine_index', 'armor_points'):
        getattr(probe, f'probe_{fn}').restype = ctypes.c_int
    probe.probe_mech_name.restype = ctypes.c_char_p
    probe.probe_armor_points.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_float]
    checks = 0

    def same(what, want, got):
        nonlocal checks
        checks += 1
        if want != got:
            raise SystemExit(f'{what}: original {want!r}, port {got!r}')

    for i in range(-3, 25):
        cpu.call(HELPERS['mech_tech'], ecx=i & 0xffffffff)
        same(f'mech_tech({i})', eax(), probe.probe_mech_tech(i))
        cpu.call(HELPERS['mech_name'], ecx=i & 0xffffffff)
        same(f'mech_name({i})', emu.cstr(cpu.uc.reg_read(UC_X86_REG_EAX)), probe.probe_mech_name(i))
    names = ['clan', 'sphere', 'Clan', 'any', '', 'ams', 'armor', 'armorff', 'fcs', 'bap', 'case', 'c3',
             'c3slave', 'ecm', 'sink', 'dsink', 'jjets', 'masc', 'tag', 'tc', 'TAG', 'Sink', 'jjet', 'x']
    names += ['AC2', 'ac2', 'Ac20', 'ERLaser_Small', 'erlaser_small', 'PPC', 'ERPPC', 'LB10XAC', 'lrm20',
              'MGUN', 'NARC', 'StreakSRM6', 'UltraAC20', 'UltraAC', 'Gauss', 'GaussRifle', 'Flamer']
    p = cpu.alloc(64)
    for n in names:
        cpu.write(p, n.encode() + b'\0')
        for fn in ('tech_id', 'equip_kind', 'weapon_index'):
            cpu.call(HELPERS[fn], ecx=p)
            same(f'{fn}({n!r})', eax(), getattr(probe, f'probe_{fn}')(n.encode()))
    cpu.call(HELPERS['tech_id'], ecx=0)
    same('tech_id(null)', eax(), probe.probe_tech_id(None))
    cpu.call(HELPERS['weapon_index'], ecx=0)
    same('weapon_index(null)', eax(), probe.probe_weapon_index(None))
    for r in range(-10, 520):
        cpu.call(HELPERS['engine_index'], ecx=r & 0xffffffff)
        same(f'engine_index({r})', eax(), probe.probe_engine_index(r))
    for tech in (1, 2, 3):
        for ferro in (0, 1):
            for k in range(0, 201):
                tons = k * 0.5
                bits = struct.unpack('<I', struct.pack('<f', tons))[0]
                cpu.call(HELPERS['armor_points'], ecx=tech, edx=ferro, stack_args=(bits,))
                same(f'armor_points({tech}, {ferro}, {tons})', eax(), probe.probe_armor_points(tech, ferro, tons))
    return checks


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
    for name, start, _ in entries(data):
        if name.startswith('dfn_'):
            want = emu.firepoints(start)
            n = probe.probe_firepoints(str(READER).encode(), start, buf, len(buf))
            if buf.raw[:max(n, 0)] != want:
                raise SystemExit(f'{name} firepoints: original {len(want)} bytes, port {n}')
    p = emu.cpu.alloc(16)
    for part in PART_NAMES:
        emu.cpu.write(p, part.encode() + b'\0')
        emu.cpu.call(PART_ID, ecx=p)
        want = struct.unpack('<i', struct.pack('<I', emu.cpu.uc.reg_read(UC_X86_REG_EAX)))[0]
        if probe.probe_part_id(part.encode()) != want:
            raise SystemExit(f'part_id({part!r}): original {want}')
    for i in range(-3, 20):
        emu.cpu.call(PART_NAME, ecx=i & 0xffffffff)
        want = emu.cstr(emu.cpu.uc.reg_read(UC_X86_REG_EAX))
        if probe.probe_part_name(i) != want:
            raise SystemExit(f'part_name({i}): original {want}')
    for name, start, _ in entries(data):
        if name.startswith('dfn_'):
            idx = MECHS.index(name[4:-4])
            for index in (idx, -1):  # -1: an out-of-range mech index (tech 3)
                want = emu.head(start, index)
                n = probe.probe_dfn_head(str(READER).encode(), start, index, buf, len(buf))
                if buf.raw[:max(n, 0)] != want:
                    at = next((i for i in range(min(n, len(want))) if buf.raw[i] != want[i]), None)
                    raise SystemExit(f'{name} head (index {index}): original {len(want)} bytes, port {n}, '
                                     f'first difference at {at}')
    configs = 0
    for name, start, _ in entries(data):
        if name.startswith('dfn_'):
            for localized in (0, 1):
                want = emu.configs(start, MECHS.index(name[4:-4]), localized)
                n = probe.probe_dfn_configs(str(READER).encode(), start, MECHS.index(name[4:-4]), localized, buf,
                                            len(buf))
                if buf.raw[:max(n, 0)] != want:
                    at = next((i for i in range(min(n, len(want))) if buf.raw[i] != want[i]), None)
                    raise SystemExit(f'{name} configurations (localized {localized}): original {len(want)} bytes, '
                                     f'port {n}, first difference at {at}')
            configs += struct.unpack_from('<i', want, 9)[0]
    checks = check_helpers(emu)
    print(f'ok: skins of {files} mech definitions identical four ways each; firepoints, part slots and '
          f'the MechDfn defaults, the head and {configs} loadouts identical; part ids and names agree; '
          f'{checks} table helper calls agree')


if __name__ == '__main__':
    main()
