"""The ported saved loadouts (docs/mechdef-head.md) against the original x86:
Config::Load 0x424600 with its checksum, weight and Validate, and
ConfigList::LoadSaved 0x424b70 on whole configs\\<mech>.mw3 archives. The
port drops the checksum trap (Load no longer rejects edited files), so the
original runs with the same change: Load's `push 1` into Validate becomes
`push 0`. The checksum itself is still compared on every config.

    cmake --build build && .venv/bin/python tests/differential/test_mechsave.py

The files are written by the original: each mech's built-in loadouts are
read from its dfn file (0x46ba95..0x46cb3c), some are changed in memory (armor,
engine, sinks, jump jets, an unplaced item, another mech's index), and each is
saved by Config::Save 0x424480, which stores a matching checksum. Copies are
then damaged byte by byte, and items of kinds 0, 4 and 5 are spliced in. The
weapons' mech-lab blocks (weights and slots, from weapons.zrd at run time) are
made up and handed to both sides.
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools' / 'analysis'))
from original import ROOT, STACK_TOP, FTOL, IAT_FTOL  # noqa: E402
from test_zreader import READER, entries, HANDLE  # noqa: E402
from test_mechdef import Emulated as DfnEmu, FLOOR, IAT_FLOOR, LIST_INIT, DAMAGE_MODE, MSG_HOOK, MECHS, \
    DFN_CTOR, HEAD_BLOCK, CONFIGS_END, NO_CONFIGS, HEAD_FAIL, IAT_NEW, probe  # noqa: E402
from unicorn import UC_HOOK_CODE, UcError  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBP, UC_X86_REG_ECX, UC_X86_REG_EIP, UC_X86_REG_ESP  # noqa: E402

SAVE, LOAD, CHECKSUM, WEIGHT, UNPLACED, JJ, JJ_LIMIT, VALIDATE = (0x424480, 0x424600, 0x4255b0, 0x425300, 0x424220,
                                                                   0x425ff0, 0x425fb0, 0x4254d0)
LIST_CTOR, LOAD_SAVED, CONFIG_CTOR, FIND_WEAPON = 0x424a90, 0x424b70, 0x423d00, 0x581ed0
WEAPON_COUNT, EQUIP_COUNT = 0x425ea0, 0x473570
SLOTS_LEFT, FERRO_SLOTS, ENDO_SLOTS, ENGINE_SLOTS, SINK_LIMIT, SINK_ITEMS = (0x425d80, 0x425e40, 0x425e80, 0x425e60,
                                                                           0x426070, 0x425f30)
SAVE_ALL, DIR_EXISTS, WARCH = 0x424f60, 0x576080, 0x9abc
ENGINE_WEIGHT, ENGINE_RATING, ENGINE_TABLE = 0x432f40, 0x432fa0, 0x5e2f50
LOAD_VALIDATE = 0x424744  # Load's `push 1` into Validate
ITEMS_KIND_READ = 0x4715af  # 0x471540 has just read an item's kind
ITEM_LOAD_ID = 0x472b21  # just after 0x472b20 pushes ECX as the id's slot
IAT = {'strncmp': 0x5bb654, 'free': 0x5bb65c, 'ceil': 0x5bb61c, 'writefile': 0x5bb17c, 'sprintf': 0x5bb658,
       'createfile': 0x5bb0c8, 'getlasterror': 0x5bb178, 'formatmessage': 0x5bb0e8, 'localfree': 0x5bb0f0,
       'getfilesize': 0x5bb130, 'setfilepointer': 0x5bb0d8, 'closehandle': 0x5bb0d0, 'realloc': 0x5bb610}
CEIL = FLOOR.replace(bytes.fromhex('660d0004'), bytes.fromhex('660d0008'))
ARCH = 0x5678
WEAPONS = 37
VT_KIND = {0x5bc160: 0, 0x5c0fc0: 1, 0x5bd150: 2, 0x5bd180: 3, 0x5c3700: 4}
DIR = ROOT / 'build' / 'mechsave'
WEAPON_NAMES = [b'AC2', b'AC5', b'AC10', b'AC20', b'ERLaser_Small', b'ERLaser_Medium', b'ERLaser_Large', b'ERPPC',
                b'Flamer', b'GaussRifle', b'Laser_Small', b'Laser_Medium', b'Laser_Large', b'LB2XAC', b'LB5XAC',
                b'LB10XAC', b'LB20XAC', b'LRM5', b'LRM10', b'LRM15', b'LRM20', b'MGUN', b'NARC', b'PulseLaser_Small',
                b'PulseLaser_Medium', b'PulseLaser_Large', b'SRM2', b'SRM4', b'SRM6', b'StreakSRM2', b'StreakSRM4',
                b'StreakSRM6', b'TAG', b'UltraAC2', b'UltraAC5', b'UltraAC10', b'UltraAC20']

probe.probe_set_weapon_labs.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
probe.probe_config_blob.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_void_p, ctypes.c_int]
probe.probe_load_saved.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_void_p, ctypes.c_int]
probe.probe_load_and_save.argtypes = [ctypes.c_char_p, ctypes.c_int]
probe.probe_config_copy.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_void_p, ctypes.c_int]
COPY = 0x424010  # the slot copy's (ECX the name, EDX the loadout; no store, flag 0)
DFN_CACHE = 0x5e60b0  # 0x004846a0's table, 0x3c apart


def fake_parts(mech):
    """The firepoints' parts of the stand-in dfns both sides get for the copy's recount."""
    return [((i * 5 + mech) % 10) - 1 for i in range(8)]


def weapon_labs(data):
    """Each weapon's MASS from weapons.zrd (tons and slots, Clan then Inner
    Sphere; tons per round = 1 / (rounds per ton x salvo)), as zWeapon builds
    it. Flamer and NARC get no block, to try that path."""
    import zrd
    start, length = next((s, n) for name, s, n in entries(data) if name.lower() == 'weapons.zrd')
    tree, _ = zrd.node(data[start:start + length], 0)
    defs = {}

    def walk(lst):
        for i, item in enumerate(lst):
            if isinstance(item, str) and i + 1 < len(lst) and isinstance(lst[i + 1], list) and 'MASS' in lst[i + 1]:
                defs[item.lower()] = lst[i + 1]
            elif isinstance(item, list):
                walk(item)
    walk(tree)

    def get(lst, key):
        return lst[lst.index(key) + 1] if key in lst and lst.index(key) + 1 < len(lst) else None

    labs = []
    for i, name in enumerate(WEAPON_NAMES):
        d = defs.get(name.decode().lower())
        if d is None or i in (8, 22):
            labs.append(None)
            continue
        mass = get(d, 'MASS')
        clan, sphere, ammo = get(mass, 'clan') or [0, 0], get(mass, 'sphere') or [0, 0], get(mass, 'ammo')
        salvo = get(d, 'SALVO')
        true = get(salvo, 'truecount') if isinstance(salvo, list) else None
        per_ton = (ammo[0] if ammo else 0) * (true[0] if true else 1)
        labs.append((float(clan[0]), float(sphere[0]), int(clan[1]), int(sphere[1]),
                     1.0 / per_ton if per_ton else 0.0))
    return labs


class Emulated(DfnEmu):
    def __init__(self, data, labs):
        super().__init__(data)
        cpu = self.cpu
        cpu.import_code(IAT_FTOL, FTOL)
        cpu.import_code(IAT_FLOOR, FLOOR)
        cpu.import_code(IAT['ceil'], CEIL)
        cpu.stub_import(IAT['strncmp'], self.strncmp)
        cpu.stub_import(IAT['free'], lambda c: 0)
        cpu.stub_import(IAT['writefile'], self.write_file, pop_bytes=20)
        cpu.stub_import(IAT['sprintf'], self.sprintf)
        cpu.stub_import(IAT['createfile'], self.create_file, pop_bytes=28)
        cpu.stub_import(IAT['getlasterror'], lambda c: 2)
        cpu.stub_import(IAT['formatmessage'], lambda c: 0, pop_bytes=28)
        cpu.stub_import(IAT['localfree'], lambda c: 0, pop_bytes=4)
        cpu.stub_import(IAT['getfilesize'], lambda c: len(self.wbuf if c.arg(0) == WARCH else self.arch), pop_bytes=8)
        cpu.stub_function(DIR_EXISTS, lambda c: 1)
        cpu.stub_import(0x5bb648, self.strncpy)
        cpu.stub_import(IAT['setfilepointer'], self.set_file_pointer, pop_bytes=16)
        cpu.stub_import(IAT['closehandle'], lambda c: 1, pop_bytes=4)
        cpu.stub_import(IAT['realloc'], self.realloc)
        cpu.stub_function(FIND_WEAPON, self.find_weapon)
        cpu.uc.hook_add(UC_HOOK_CODE, self.item_id, begin=ITEM_LOAD_ID, end=ITEM_LOAD_ID)
        cpu.write(LOAD_VALIDATE, b'\x6a\x00')  # push 0: the port drops the checksum trap (FIX (trap))
        cpu.uc.hook_add(UC_HOOK_CODE, self.items_end, begin=ITEMS_KIND_READ, end=ITEMS_KIND_READ)
        self.patched = {}
        for fn in (ENGINE_WEIGHT, ENGINE_RATING):
            cpu.uc.hook_add(UC_HOOK_CODE, self.engine_entry, begin=fn, end=fn)
        cpu.stub_import(IAT_NEW, self.new)  # zeroed: fields the exe leaves unset read 0, as in the port
        cpu.write(DAMAGE_MODE, b'\0')
        cpu.u32(MSG_HOOK, 0)
        self.weapons = {}
        for name, lab in zip(WEAPON_NAMES, labs):
            if lab is None:
                continue
            obj, blk = cpu.alloc(0x200), cpu.alloc(0x40)
            cpu.write(blk, bytes(0x40))
            cpu.write(blk + 0x1c, struct.pack('<fi', lab[0], lab[2]))
            cpu.write(blk + 0x28, struct.pack('<fi', lab[1], lab[3]))
            cpu.write(blk + 0x34, struct.pack('<f', lab[4]))
            cpu.u32(obj + 0x1f0, blk)
            self.weapons[name] = obj
        self.sizes = {}
        self.out, self.arch, self.apos, self.opened, self.short = bytearray(), b'', 0, None, False
        self.wbuf, self.wpos = bytearray(), 0
        self.wslot = cpu.alloc(12)
        code = b'\x8b\x0d' + struct.pack('<I', self.wslot) + b'\xff\x35' + struct.pack('<I', self.wslot + 4)
        code += b'\xb8' + struct.pack('<I', WEIGHT) + b'\xff\xd0' + b'\xd9\x1d' + struct.pack('<I', self.wslot + 8)
        self.wthunk = cpu.code(code + b'\xc3')
        self.base_heap = cpu.heap_next

    def item_id(self, uc, address, size, user):
        """The item load's id starts as ECX (the item's address) when the file
        ends before it; the port's INT32_MAX stands in for that (FIX (A))."""
        uc.mem_write(uc.reg_read(UC_X86_REG_ESP), struct.pack('<I', 0x7fffffff))

    def items_end(self, uc, address, size, user):
        """The port's stop at the file's end (FIX (bug)): after a short read of
        an item's kind the exe would count the rest down making nothing."""
        if self.short:
            uc.mem_write(uc.reg_read(UC_X86_REG_ESP) + 0x10, bytes(4))

    def engine_entry(self, uc, address, size, user):
        """An engine index past the table: the exe reads whatever follows it,
        the port 0 (FIX (A)). The entry it reads is zeroed until the case ends."""
        idx = self.i32(uc.reg_read(UC_X86_REG_ECX))
        if 0 <= idx < 79:
            return
        a = (ENGINE_TABLE + 12 * idx) & 0xffffffff
        if a in self.patched:
            return
        page = a & ~0xfff
        for p in (page, page + 0x1000):
            try:
                uc.mem_read(p, 1)
            except UcError:
                uc.mem_map(p, 0x1000)
        self.patched[a] = uc.mem_read(a, 12)
        uc.mem_write(a, bytes(12))

    def unpatch(self):
        for a, b in self.patched.items():
            self.cpu.uc.mem_write(a, bytes(b))
        self.patched = {}

    def new(self, c):
        a = c.alloc(max(c.arg(0), 1))
        c.write(a, bytes(max(c.arg(0), 1)))
        return a

    def strncpy(self, c):
        dst, src, n = c.arg(0), self.cstr(c.arg(1)), c.arg(2)
        c.write(dst, (src + bytes(n))[:n])
        return dst

    def find_weapon(self, c):
        return self.weapons.get(self.cstr(c.ecx), 0)

    def realloc(self, c):
        old, n = c.arg(0), c.arg(1)
        a = c.alloc(max(n, 1))
        if old:
            c.write(a, c.read(old, min(self.sizes.get(old, n), n)))
        self.sizes[a] = n
        return a

    def sprintf(self, c):
        fmt, out, k = self.cstr(c.arg(1)), bytearray(), 2
        i = 0
        while i < len(fmt):
            if fmt[i:i + 2] == b'%s':
                out += self.cstr(c.arg(k))
                k, i = k + 1, i + 2
            else:
                assert fmt[i] != ord('%'), fmt
                out.append(fmt[i])
                i += 1
        c.write(c.arg(0), bytes(out) + b'\0')
        return len(out)

    def write_file(self, c):
        if c.arg(0) == WARCH:
            data = self.cpu.read(c.arg(1), c.arg(2))
            self.wbuf[self.wpos:self.wpos + len(data)] = data
            self.wpos += len(data)
            self.cpu.u32(c.arg(3), c.arg(2))
            return 1
        self.out += self.cpu.read(c.arg(1), c.arg(2))
        self.cpu.u32(c.arg(3), c.arg(2))
        return 1

    def create_file(self, c):
        self.opened = self.cstr(c.arg(0))
        if c.arg(1) & 0x40000000:  # GENERIC_WRITE: SaveAll's CREATE_ALWAYS
            self.wbuf, self.wpos = bytearray(), 0
            return WARCH
        if self.arch is None:
            return 0xffffffff
        self.apos = 0
        return ARCH

    def set_file_pointer(self, c):
        handle, dist, how = c.arg(0), struct.unpack('<i', struct.pack('<I', c.arg(1)))[0], c.arg(3)
        if handle == WARCH:
            self.wpos = {0: 0, 1: self.wpos, 2: len(self.wbuf)}[how] + dist
            return self.wpos
        assert handle == ARCH and c.arg(2) == 0
        self.apos = {0: 0, 1: self.apos, 2: len(self.arch)}[how] + dist
        return self.apos

    def read_file(self, c):
        if c.arg(0) != ARCH:
            pos = self.pos
            r = super().read_file(c)
            self.short = self.pos - pos < c.arg(2)
            return r
        buf, n, out = c.arg(1), c.arg(2), c.arg(3)
        chunk = self.arch[self.apos:self.apos + n] if self.apos >= 0 else b''
        self.apos += len(chunk)
        self.short = len(chunk) < n
        c.write(buf, chunk)
        c.u32(out, len(chunk))
        return 1

    def stock_configs(self, offset, index):
        """The dfn's built-in loadouts, as the original reader makes them."""
        cpu = self.cpu
        self.load(offset)
        self.reports = []
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
        lst = self.u32(dfn + 0x20)
        first, last = self.u32(lst + 0xc), self.u32(lst + 0x10)
        return [self.u32(first + 4 * k) for k in range((last - first) // 4)]

    def save(self, c):
        self.cpu.write(c + 0x1c, b'\x01')  # only the player's own are saved, and the checksum covers it
        self.out = bytearray()
        self.cpu.call(SAVE, ecx=c, stack_args=(HANDLE,))
        return bytes(self.out)

    def items(self, c):
        out, node, sentinel = [], self.u32(self.u32(c + 8)), self.u32(c + 8)
        while node != sentinel:
            out.append(self.u32(node + 8))
            node = self.u32(node)
        return out

    def dump_config(self, c):
        r, u, i32 = self.cpu.read, self.u32, self.i32
        out = bytearray(self.cstr(u(c + 0x18)) + b'\0')
        out += r(c + 0x14, 4) + r(c + 0x70, 0x18)
        out += bytes(r(c + o, 1)[0] for o in (0x1c, 0x1d, 0x1f, 0x20, 0x34, 0x28, 0x6c))
        out += r(c + 0x88, 4) + r(c + 0x2c, 8) + r(c + 0x38, 0x2c) + r(c + 0x24, 4) + r(c + 0x64, 8)
        items = self.items(c)
        assert i32(c + 0xc) == len(items)
        out += struct.pack('<i', len(items))
        for it in items:
            kind = VT_KIND[u(it)]
            out += struct.pack('<iiii', kind, i32(it + 4), i32(it + 8), i32(it + 0xc)) + r(it + 0x10, 1)
            out += r(it + 0x18, 4) if kind == 3 else r(it + 0x14, 4)
            if kind == 1:
                out += r(it + 0x18, 4)
            elif kind == 3:
                out += r(it + 0x1c, 4)
            elif kind == 4:
                out += r(it + 0x18, 1)
        return bytes(out)

    def fweight(self, c, what):
        """Weight(what) on config c; one thunk, since it returns on the x87 stack."""
        cpu = self.cpu
        cpu.u32(self.wslot, c)
        cpu.u32(self.wslot + 4, what & 0xffffffff)
        cpu.call(self.wthunk)
        return cpu.read(self.wslot + 8, 4)

    def blob(self, name, blob):
        """What probe_config_blob reports, from the original."""
        cpu = self.cpu
        mark = cpu.heap_next
        p = cpu.alloc(len(name) + 1)
        cpu.write(p, name + b'\0')
        c = cpu.alloc(0xd8)
        cpu.write(c, bytes(0xd8))  # the ctor leaves +0x1f as the heap had it; the port has 0
        cpu.call(CONFIG_CTOR, ecx=c, stack_args=(0xffffffff, p))
        saved = self.data, self.pos
        self.data, self.pos = blob, 0
        cpu.call(LOAD, ecx=c, stack_args=(HANDLE,))
        eax = lambda: cpu.uc.reg_read(UC_X86_REG_EAX)
        out = bytearray([eax() & 0xff]) + struct.pack('<i', self.pos)
        self.data, self.pos = saved
        out += self.dump_config(c)
        cpu.call(CHECKSUM, ecx=c)
        out += struct.pack('<I', eax())
        for k in range(-1, 9):
            out += self.fweight(c, k)
        cpu.call(UNPLACED, ecx=c)
        out.append(eax() & 0xff)
        cpu.call(JJ, ecx=c)
        out += struct.pack('<I', eax())
        cpu.call(JJ_LIMIT, ecx=c)
        out += struct.pack('<I', eax())
        cpu.call(VALIDATE, ecx=c, stack_args=(0,))
        out.append(eax() & 0xff)
        # the critical slots: left (of the free count as loaded), ferro-fibrous, endo steel, the engine's, sinks
        for fn in (SLOTS_LEFT, FERRO_SLOTS, ENDO_SLOTS, ENGINE_SLOTS, SINK_LIMIT, SINK_ITEMS):
            cpu.call(fn, ecx=c)
            out += struct.pack('<I', eax())
        # the mech lab's counts (0x00453840's): weapons with TAG, and equipment by kind and tech
        cpu.call(WEAPON_COUNT, ecx=c)
        out += struct.pack('<I', eax())
        for kind in range(15):
            for tech in (1, 2, 3):
                cpu.call(EQUIP_COUNT, ecx=c, stack_args=(kind, tech))
                out += struct.pack('<I', eax())
        self.unpatch()
        cpu.heap_next = mark
        return bytes(out)

    def seed_dfns(self):
        """Stand-in dfns in 0x004846a0's cache: only their firepoints' parts are read."""
        cpu = self.cpu
        for m in range(19):
            fps = []
            for part in fake_parts(m):
                f = cpu.alloc(0x20)
                cpu.write(f, bytes(0x20))
                cpu.u32(f + 0xc, part & 0xffffffff)
                fps.append(f)
            arr, vec, dfn = cpu.alloc(4 * len(fps)), cpu.alloc(0x10), cpu.alloc(0x40)
            for i, f in enumerate(fps):
                cpu.u32(arr + 4 * i, f)
            cpu.u32(vec + 4, arr)
            cpu.u32(vec + 8, arr + 4 * len(fps))
            cpu.write(dfn, bytes(0x40))
            cpu.u32(dfn + 0x24, vec)
            cpu.u32(DFN_CACHE + 0x3c * m, dfn)

    def copy(self, name, blob):
        """What probe_config_copy reports: the loadout loaded, then copied as a slot's is."""
        cpu = self.cpu
        mark = cpu.heap_next
        p = cpu.alloc(len(name) + 1)
        cpu.write(p, name + b'\0')
        c = cpu.alloc(0xd8)
        cpu.write(c, bytes(0xd8))
        cpu.call(CONFIG_CTOR, ecx=c, stack_args=(0xffffffff, p))
        saved = self.data, self.pos
        self.data, self.pos = blob, 0
        cpu.call(LOAD, ecx=c, stack_args=(HANDLE,))
        self.data, self.pos = saved
        self.seed_dfns()  # on this call's heap: the loads before reuse it
        cpu.call(COPY, ecx=self.u32(c + 0x18), edx=c, stack_args=(0, 0))
        d = cpu.uc.reg_read(UC_X86_REG_EAX)
        out = self.dump_config(d) + cpu.read(d + 0x8c, 0x28)
        self.unpatch()
        cpu.heap_next = mark
        return bytes(out)

    def load_and_save(self, mech, arch):
        """LoadSaved on the file, then SaveAll: the file it writes."""
        cpu = self.cpu
        mark = cpu.heap_next
        self.arch = arch
        lst = cpu.alloc(0x18)
        cpu.call(LIST_CTOR, ecx=lst, stack_args=(mech, 0))
        cpu.call(LOAD_SAVED, ecx=lst)
        cpu.call(SAVE_ALL, ecx=lst)
        ok = cpu.uc.reg_read(UC_X86_REG_EAX) & 0xff
        self.unpatch()
        cpu.heap_next = mark
        return ok, mask_table(bytes(self.wbuf))

    def load_saved(self, mech, arch):
        cpu = self.cpu
        mark = cpu.heap_next
        self.arch = arch
        lst = cpu.alloc(0x18)
        cpu.call(LIST_CTOR, ecx=lst, stack_args=(mech, 0))
        cpu.call(LOAD_SAVED, ecx=lst)
        out = bytearray([cpu.uc.reg_read(UC_X86_REG_EAX) & 0xff])
        assert self.opened == b'configs\\' + MECHS[mech].encode() + b'.mw3', self.opened
        first, last = self.u32(lst + 0xc), self.u32(lst + 0x10)
        out += struct.pack('<i', (last - first) // 4)
        for k in range((last - first) // 4):
            out += self.dump_config(self.u32(first + 4 * k))
        self.unpatch()
        cpu.heap_next = mark
        return bytes(out)


def mask_table(data):
    """An archive with each entry's flags, comment and file time zeroed: the
    exe fills them from its stack (FIX (A): zero in the port)."""
    if len(data) < 8:
        return data
    count = struct.unpack_from('<I', data, len(data) - 4)[0]
    out = bytearray(data)
    table = len(data) - 8 - 0x94 * count
    for i in range(count):
        e = table + 0x94 * i
        out[e + 0x48:e + 0x94] = bytes(0x4c)
    return bytes(out)


def archive(files):
    data, table = b'', b''
    for name, body in files:
        table += struct.pack('<II', len(data), len(body)) + name.ljust(64, b'\0')[:64] + bytes(0x94 - 0x48)
        data += body
    return data + table + struct.pack('<II', 1, len(files))


def item_records(blob):
    """Offset of the item count and of each item record in a saved config."""
    n = struct.unpack_from('<I', blob)[0]
    at = 4 + n
    count = struct.unpack_from('<I', blob, at)[0]
    offs, p = [], at + 4
    for _ in range(count):
        offs.append(p)
        kind = struct.unpack_from('<I', blob, p)[0]
        p += 4 + 16 + (4 if kind in (1, 3) else 1 if kind == 4 else 0)
    return at, offs


def splice(blob, record):
    """The config with one more item record at the front of its list."""
    at, offs = item_records(blob)
    count = struct.unpack_from('<I', blob, at)[0]
    return blob[:at] + struct.pack('<I', count + 1) + record + blob[at + 4:]


MUTATIONS = [
    ('stock', None),
    ('armor +5', lambda e, c: e.cpu.u32(c + 0x38, e.i32(c + 0x38) + 5)),
    ('armor tons 0', lambda e, c: e.cpu.write(c + 0x2c, struct.pack('<f', 0.0))),
    ('engine 400', lambda e, c: e.cpu.u32(c + 0x24, 78)),
    ('engine xl', lambda e, c: e.cpu.write(c + 0x28, b'\x01')),
    ('engine index 130', lambda e, c: e.cpu.u32(c + 0x24, 130)),
    ('sinks 30', lambda e, c: e.cpu.u32(c + 0x68, 30)),
    ('endo', lambda e, c: e.cpu.write(c + 0x20, b'\x01')),
    ('mech elemental', lambda e, c: e.cpu.u32(c + 0x14, 7)),
    ('mech 25', lambda e, c: e.cpu.u32(c + 0x14, 25)),
    ('unplaced', lambda e, c: [e.cpu.u32(it + 0xc, 0xffffffff) for it in e.items(c)[:1]]),
    ('jump jets', lambda e, c: [e.cpu.u32(it + 0x18, 11) for it in e.items(c) if VT_KIND[e.u32(it)] == 3]),
]


def compare_blob(what, want, n, buf):
    """As compare, with the ten weights (x87 sums) within float rounding."""
    got = buf.raw[:max(n, 0)]
    # The weights' 40 bytes, then 34 (the flags, jump jets, the slots' 24) and the lab's counts (4 + 45 x 4).
    w0, w1 = -(74 + 184), -(34 + 184)
    if len(got) == len(want) and got[:w0] + got[w1:] == want[:w0] + want[w1:]:
        a, b = struct.unpack('<10f', want[w0:w1]), struct.unpack('<10f', got[w0:w1])
        if all(x == y or abs(x - y) <= 1e-6 * max(1.0, abs(x)) for x, y in zip(a, b)):
            return
    compare(what, want, n, buf)


def compare(what, want, n, buf):
    got = buf.raw[:max(n, 0)]
    if got != want and '--decode' in sys.argv:
        sys.path.insert(0, sys.argv[sys.argv.index('--decode') + 1])
        import decode
        decode.diff(want, got)
    if got != want:
        at = next((i for i in range(min(n, len(want))) if got[i] != want[i]), None)
        raise SystemExit(f'{what}: original {len(want)} bytes, port {n}, first difference at {at}')


def main():
    data = READER.read_bytes()
    labs = weapon_labs(data)
    table = (ctypes.c_float * (WEAPONS * 5))()
    present = (ctypes.c_uint8 * WEAPONS)()
    for i, lab in enumerate(labs):
        if lab:
            present[i] = 1
            table[5 * i:5 * i + 5] = [lab[0], lab[1], 0, 0, lab[4]]
            ctypes.memmove(ctypes.addressof(table) + 20 * i + 8, struct.pack('<ii', lab[2], lab[3]), 8)
    probe.probe_set_weapon_labs(table, present)
    emu = Emulated(data, labs)
    buf = ctypes.create_string_buffer(1 << 20)
    rng = random.Random(7)
    (DIR / 'configs').mkdir(parents=True, exist_ok=True)
    blobs = valid = files = saves = copies = 0
    stats = {}
    dfns = [(n, s) for n, s, _ in entries(data) if n.startswith('dfn_')]
    for name, start in dfns:
        mech = MECHS.index(name[4:-4])
        saved = []
        for label, change in MUTATIONS:
            for k, c in enumerate(emu.stock_configs(start, mech)):
                if change:
                    if k % 2:
                        continue  # half the configs per change keeps the run short
                    change(emu, c)
                cname = emu.cstr(emu.u32(c + 0x18))
                saved.append((cname, emu.save(c), label))
        cases = []
        for cname, blob, label in saved:
            if label == 'stock' and '--layout' in sys.argv:
                print(cname, len(blob), item_records(blob))
            cases.append((cname, blob, label))
            if label != 'stock':
                continue
            count_at = item_records(blob)[0]
            for _ in range(6):
                b = bytearray(blob)
                at = rng.randrange(len(b))
                # a huge name length crashes the original, a huge item count hangs it (both fixed in the port)
                while at < 4 or count_at <= at < count_at + 4:
                    at = rng.randrange(len(b))
                b[at] ^= 1 << rng.randrange(8)
                cases.append((cname, bytes(b), f'damaged at {at}'))
            cases.append((cname, blob[:rng.randrange(4, len(blob))], 'cut short'))
            cases.append((cname, splice(blob, struct.pack('<I4i', 0, 2, 9, 3, 1)), 'kind 0 item'))
            cases.append((cname, splice(blob, struct.pack('<I4iB', 4, 1, 40, -1, 0, 1)), 'kind 4 item'))
            cases.append((cname, splice(blob, struct.pack('<I4i', 5, 1, 2, 3, 4)), 'kind 5 item'))
        for cname, blob, label in cases:
            try:
                want = emu.blob(cname, blob)
            except Exception as e:
                eip = emu.cpu.uc.reg_read(UC_X86_REG_EIP)
                raise SystemExit(f'{name} {cname} ({label}): original faults at {eip:#x}: {e}')
            compare_blob(f'{name} {cname.decode()} ({label})', want,
                    probe.probe_config_blob(cname, blob, len(blob), buf, len(buf)), buf)
            blobs += 1
            valid += want[0]
            if label == 'stock' or label in [m for m, _ in MUTATIONS] or label == 'kind 0 item':
                # a copied engine item (kinds 4) writes through null in the exe; damaged ones are not copied in play
                compare_blob(f'{name} {cname.decode()} ({label}) copied', emu.copy(cname, blob),
                             probe.probe_config_copy(cname, blob, len(blob), buf, len(buf)), buf)
                copies += 1
            if '--stats' in sys.argv:
                key = label.split(' at ')[0]
                stats.setdefault(key, [0, 0, set()])
                stats[key][0] += 1
                stats[key][1] += want[0]
                stats[key][2].add(want[-1:].hex() + '/' + want[-10:-9].hex())
        # whole files: every case, then the version entry's variants and no file
        body = [(cname, blob) for cname, blob, _ in cases]
        version = (b'version', struct.pack('<I', 15))
        for label, files_in in (('all', [version] + body), ('version last', body[:3] + [version]),
                                ('version 14', [(b'version', struct.pack('<I', 14))] + body[:3]),
                                ('version 8 bytes', [(b'version', struct.pack('<Q', 15))] + body[:3]),
                                ('version 2 bytes', [(b'version', struct.pack('<H', 15))] + body[:3]),
                                ('no version', body[:3]), ('empty', [version])):
            arch = archive(files_in)
            want = emu.load_saved(mech, arch)
            (DIR / 'configs' / f'{MECHS[mech]}.mw3').write_bytes(arch)
            compare(f'{name} file ({label})', want, probe.probe_load_saved(str(DIR).encode(), mech, buf, len(buf)),
                    buf)
            files += 1
            ok, want = emu.load_and_save(mech, arch)
            (DIR / 'configs' / f'{MECHS[mech]}.mw3').write_bytes(arch)
            got_ok = probe.probe_load_and_save(str(DIR).encode(), mech)
            got = (DIR / 'configs' / f'{MECHS[mech]}.mw3').read_bytes()
            if (ok, want) != (got_ok, got):
                at = next((i for i in range(min(len(got), len(want))) if got[i] != want[i]), None)
                raise SystemExit(f'{name} saved again ({label}): original {ok} {len(want)} bytes, port {got_ok} '
                                 f'{len(got)}, first difference at {at}')
            saves += 1
        (DIR / 'configs' / f'{MECHS[mech]}.mw3').unlink()
        want = emu.load_saved(mech, None)
        compare(f'{name} no file', want, probe.probe_load_saved(str(DIR).encode(), mech, buf, len(buf)), buf)
        files += 1
    for k, v in stats.items():
        print(k, v)
    print(f'ok: {blobs} saved loadouts load identically ({valid} accepted: checksum, weights, slots, jump jets '
          f'and armor agree); {files} configs\\<mech>.mw3 files identical; '
          f'{saves} written back identically; {copies} copied identically (slots counted)')


if __name__ == '__main__':
    main()
