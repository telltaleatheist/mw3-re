"""The pilot and its files (docs/pilot.md) against the original x86.

    cmake --build build && .venv/bin/python -P tests/differential/test_pilot.py [runs] [first] [--verbose]

The original's pilot module (0x004779f0..0x004794f0) and its games
(0x00433cc0..0x004379e0, 0x00470ce0..0x00473400) run in the emulator over a
fake file system behind CreateFileA, ReadFile, WriteFile, SetFilePointer,
CloseHandle, DeleteFileA, MoveFileA, FindFirstFileA / FindNextFileA,
_access and _splitpath (case-insensitive, case-preserving, as NTFS); the port
(tests/probe_pilot.cpp) runs the same plan on a real folder as its write dir.
The plan: crafted files (some damaged), the listing (0x00478560), new pilots
(0x004788b0), field changes in every game, items added, merged and taken from
the stores (0x00472630, 0x00471940, 0x00471d20, 0x00472a30), lancemates
(the mission start's 0x004351c0 and 0x00435230 too) and slot configurations, saved campaigns (0x004345a0), renames (0x00477fe0),
deletions (0x00477f50), selections (0x00478250, 0x00478ed0), saves, the
debug start (0x00478a40), deleting the others (0x004785f0); then every file
damaged in place (cut short, a count changed, a byte flipped, the header
broken) and listed again. Compared: the calls out (the store fill 0x00470e30,
a new campaign's mechs 0x004733b0, messages, the slots' configurations
0x004242a0 / 0x00424480, which are stubs on both sides), a dump of every
pilot, game, store, item and slot, and every file's bytes.

The port's stops at a file's end (FIX (bug), pilot.md) are applied to the
original by code hooks so the rest compares: the stores' item lists
(0x004715af), the campaigns' lancemates (0x00434244) and the pilot's
campaigns (0x00479366); an item's id cut off by the end is pinned to
INT32_MAX (0x00472b21), and a header shorter than 12 bytes reads as zeros (the
exe's stack garbage).
"""
import ctypes
import fnmatch
import os
import random
import shutil
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import ROOT, Original  # noqa: E402
from twod_world import IAT  # noqa: E402
from calltrace import load_probe  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn.x86_const import UC_X86_REG_ESP  # noqa: E402

probe = load_probe()
probe.probe_pilot_run.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]

NEW, DELETE = 0x5abd26, 0x5abd1a
CS_CTOR, CS_CTOR_S, CS_DTOR, CS_FORMAT, CS_BUFFER, ATEXIT = 0x5abdb0, 0x5abe04, 0x5abda4, 0x5abdaa, 0x5abdc8, 0x5ac140
LIST_INIT, LOAD_ALL, CREATE, DELETE_PILOT, RENAME, FIND = 0x4779f0, 0x478560, 0x4788b0, 0x477f50, 0x477fe0, 0x478be0
SELECT, SELECT_CAMPAIGN, OTHERS, OPEN, HEADER, DEBUG = 0x478250, 0x478ed0, 0x4785f0, 0x478ce0, 0x4794f0, 0x478a40
NEW_CAMPAIGN, SLOT_CTOR = 0x4345a0, 0x433cc0
SET_MATE, SET_MATES = 0x4351c0, 0x435230
ADD_COPY, ADD_AMMO, REMOVE, REMOVE_MECH = 0x472630, 0x471940, 0x471d20, 0x472a30
PILOTS, COUNT, CURRENT = 0x60860c, 0x608610, 0x60861c
RECORD, LANCE_RESET, STORE_CTOR, SALVAGE = 0x478f10, 0x435240, 0x470ce0, 0x609990
CONFIG_COPY, RECOUNT, SAVE_WORLD, MESSAGE_BOX = 0x424010, 0x425bd0, 0x5a8a70, 0x4ac150
ITEMS_KIND_READ, ITEM_LOAD_ID, MATES_LOOP, CAMPAIGNS_LOOP = 0x4715af, 0x472b21, 0x434244, 0x479366
HEADER_READS = [(0x478ce0, 0x478ed0), (0x478700, 0x4788b0)]
VT_KIND = {0x5bc160: 0, 0x5c0fc0: 1, 0x5bd150: 2, 0x5bd180: 3, 0x5c3700: 4}
KIND_VT = {v: k for k, v in VT_KIND.items()}
KIND_SIZE = {0: 0x18, 1: 0x1c, 2: 0x18, 3: 0x20, 4: 0x1c}
GAME_VT = {0x5bee08: 0, 0x5bec40: 1, 0x5bed50: 2, 0x5becf8: 3, 0x5beda8: 4}
BIG = 0x40000000  # a region for the huge or empty allocations a damaged count asks for
DIR = ROOT / 'build' / 'pilot'

# The fields a plan may change: (game, offset, bytes); game 1 is a campaign.
FIELDS = [(g, o, 4) for g in range(5) for o in (0x24, 0x30, 0x34, 0x2c)] + \
    [(1, 0x38, 1), (1, 0x9c, 4), (1, 0xa0, 4), (1, 0xa4, 4), (1, 0xa8, 4), (1, 0xac, 4), (1, 0xb0, 1),
     (1, 0xb4, 4), (1, 0xb8, 4), (1, 0xbc, 4), (1, 0xc0, 4), (1, 0xc4, 4)] + \
    [(2, o, 4) for o in range(0x4c, 0x6c, 4)] + [(2, 0x6c, 4), (2, 0x74, 4), (2, 0x78, 4), (2, 0x7c, 4),
                                                   (2, 0x170, 4), (2, 0x174, 4)] + \
    [(3, 0x70, 4), (3, 0x74, 4), (3, 0x78, 4), (3, 0x7c, 4), (3, 0x84, 4), (3, 0x8c, 1), (3, 0x8d, 1),
     (3, 0x8e, 1), (3, 0x8f, 1), (3, 0x90, 4), (3, 0x94, 4), (3, 0x98, 4), (3, 0x9c, 4), (3, 0xa8, 1)] + \
    [(4, 0x38, 4), (4, 0x3c, 1), (4, 0x3d, 1), (4, 0x3e, 1), (4, 0x3f, 1), (4, 0x40, 4), (0, 0x38, 4)]
SETUP = [0x605820 + 4 * i for i in range(5)] + [0x605838 + 4 * i for i in range(6)] + \
    [0x605858 + 4 * i for i in range(6)] + [0x605874, 0x605878, 0x60587c]
NAMES = ['Bob', 'bob', 'BOB', 'Alice', 'Zed', 'Major Tom', 'x', 'Ann_2', 'A.B', 'DefaultPlayer', 'Kerensky',
         'zz top', 'Q']


def i32(v):
    return struct.unpack('<i', struct.pack('<I', v & 0xffffffff))[0]


def hx(s):
    return s.encode('latin1').hex() or '-'


def unhx(h):
    return '' if h == '-' else bytes.fromhex(h).decode('latin1')


def key(path):
    p = path.replace('\\', '/')
    while '//' in p:
        p = p.replace('//', '/')
    return p.lower()


def display(path):
    p = path.replace('\\', '/')
    while '//' in p:
        p = p.replace('//', '/')
    return p


class FS:
    """Files by lower-cased path: [the name as made, its bytes]."""

    def __init__(self):
        self.files = {}

    def get(self, path):
        return self.files.get(key(path))

    def put(self, path, data):
        f = self.get(path)
        if f:
            f[1] = bytearray(data)
        else:
            self.files[key(path)] = [display(path), bytearray(data)]

    def damage(self, line):
        op, path, *rest = line.split()
        f = self.get(unhx(path))
        if not f:
            return
        if op == 'B':
            pos, data = int(rest[0], 16), bytes.fromhex(rest[1])
            if len(f[1]) < pos + len(data):
                f[1].extend(bytes(pos + len(data) - len(f[1])))
            f[1][pos:pos + len(data)] = data
        elif op == 'U':
            del f[1][int(rest[0], 16):]

    def snapshot(self):
        return sorted((n, bytes(b)) for n, b in self.files.values())


# ---- the original ----

class Files:
    """The file system's imports and the end-of-file hooks, on an emulator's cpu
    (test_pilotscreen.py shares them)."""

    def __init__(self, cpu, fs):
        self.cpu, self.fs = cpu, fs
        self.handles = {}
        self.next_handle = 0x100
        self.short = False
        self.listing = []
        for name, fn, pop in (('CreateFileA', self.create_file, 28), ('ReadFile', self.read_file, 20),
                              ('WriteFile', self.write_file, 20), ('SetFilePointer', self.seek, 16),
                              ('CloseHandle', self.close, 4), ('DeleteFileA', self.delete_file, 4),
                              ('MoveFileA', self.move_file, 8), ('FindFirstFileA', self.find_first, 8),
                              ('FindNextFileA', self.find_next, 8), ('_mkdir', lambda c: 0, 0),
                              ('_access', self.access, 0), ('_splitpath', self.splitpath, 0)):
            cpu.stub_import(IAT[name], fn, pop)
        uc = cpu.uc
        uc.hook_add(UC_HOOK_CODE, self.items_end, begin=ITEMS_KIND_READ, end=ITEMS_KIND_READ)
        uc.hook_add(UC_HOOK_CODE, self.item_id, begin=ITEM_LOAD_ID, end=ITEM_LOAD_ID)
        uc.hook_add(UC_HOOK_CODE, self.mates_end, begin=MATES_LOOP, end=MATES_LOOP)
        uc.hook_add(UC_HOOK_CODE, self.campaigns_end, begin=CAMPAIGNS_LOOP, end=CAMPAIGNS_LOOP)

    # memory
    def r32(self, a):
        return struct.unpack('<I', self.cpu.read(a, 4))[0]

    def r8(self, a):
        return self.cpu.read(a, 1)[0]

    def cstr(self, a):
        if not a:
            return ''
        out = bytearray()
        while True:
            b = self.cpu.read(a + len(out), 64)
            i = b.find(b'\0')
            if i >= 0:
                out += b[:i]
                return out.decode('latin1')
            out += b

    def alloc(self, n):
        n = max(n, 4)
        if n >= 0x10000:
            a = self.big
            self.big += 0x1000000
            if self.big >= BIG + 0x10000000:
                self.big = BIG
            self.cpu.write(a, b'\0' * 0x10000)
            return a
        a = self.cpu.alloc(n)
        self.cpu.write(a, b'\0' * n)
        return a

    def new(self, c):
        n = c.arg(0)
        if n == 0:
            n = 0x10000  # an empty vector given a damaged capacity: room for what is read before the end
        return self.alloc(n)

    def string(self, s):
        a = self.alloc(len(s) + 1)
        self.cpu.write(a, s.encode('latin1') + b'\0')
        return a

    # the file system
    def create_file(self, c):
        path, disp = self.cstr(c.arg(0)), c.arg(4)
        if not self.fs.get(path):
            if disp != 4:  # OPEN_EXISTING
                return 0xffffffff
            self.fs.put(path, b'')
        h = self.next_handle
        self.next_handle += 4
        self.handles[h] = [key(path), 0]
        return h

    def read(self, h, n):
        if h not in self.handles:
            return b'', 0
        k, pos = self.handles[h]
        data = bytes(self.fs.files[k][1][pos:pos + n])
        self.handles[h][1] = pos + len(data)
        if len(data) < n:
            self.short = True
        return data, len(data)

    def write(self, h, data):
        k, pos = self.handles[h]
        buf = self.fs.files[k][1]
        if len(buf) < pos:
            buf.extend(b'\0' * (pos - len(buf)))
        buf[pos:pos + len(data)] = data
        self.handles[h][1] = pos + len(data)

    def read_file(self, c):
        h, buf, n, got = c.arg(0), c.arg(1), c.arg(2), c.arg(3)
        if h not in self.handles:
            return 0
        data, m = self.read(h, n)
        ret = self.r32(self.cpu.uc.reg_read(UC_X86_REG_ESP))
        if m < n and any(lo <= ret < hi for lo, hi in HEADER_READS):
            data += b'\0' * (n - m)  # the port's zeroed header fields
        self.cpu.write(buf, data)
        if got:
            self.cpu.u32(got, m)
        return 1

    def write_file(self, c):
        h, buf, n, done = c.arg(0), c.arg(1), c.arg(2), c.arg(3)
        if h not in self.handles:
            return 0
        self.write(h, self.cpu.read(buf, n))
        if done:
            self.cpu.u32(done, n)
        return 1

    def seek(self, c):
        h = c.arg(0)
        if h not in self.handles:
            return 0xffffffff
        self.handles[h][1] = c.arg(1)
        self.short = False  # fseek clears the port's end-of-file flag
        return c.arg(1)

    def close(self, c):
        return 1 if self.handles.pop(c.arg(0), None) is not None or c.arg(0) == 0x7000 else 0

    def delete_file(self, c):
        return 1 if self.fs.files.pop(key(self.cstr(c.arg(0))), None) is not None else 0

    def move_file(self, c):
        a, b = self.cstr(c.arg(0)), self.cstr(c.arg(1))
        f = self.fs.get(a)
        if not f or (self.fs.get(b) and key(a) != key(b)):
            return 0
        del self.fs.files[key(a)]
        self.fs.files[key(b)] = [display(b), f[1]]
        return 1

    def access(self, c):
        return 0 if self.fs.get(self.cstr(c.arg(0))) else 0xffffffff

    def find_first(self, c):
        pat = key(self.cstr(c.arg(0)))
        d, p = pat.rsplit('/', 1)
        names = [n for k, (n, _) in self.fs.files.items() if k.rsplit('/', 1)[0] == d and
                 fnmatch.fnmatch(k.rsplit('/', 1)[1], p)]
        self.listing = sorted((n.rsplit('/', 1)[1] for n in names), key=lambda n: n.upper())
        if not self.listing:
            return 0xffffffff
        self.entry(c.arg(1))
        return 0x7000

    def find_next(self, c):
        if not self.listing:
            return 0
        self.entry(c.arg(1))
        return 1

    def entry(self, buf):
        name = self.listing.pop(0)
        self.cpu.write(buf, b'\0' * 0x140)
        self.cpu.u32(buf, 0x20)
        self.cpu.write(buf + 0x2c, name.encode('latin1') + b'\0')

    def splitpath(self, c):
        path = self.cstr(c.arg(0))
        cut = max(path.rfind('\\'), path.rfind('/'), path.rfind(':'))
        name = path[cut + 1:]
        dot = name.rfind('.')
        fname = name[:dot] if dot >= 0 else name
        if c.arg(3):
            self.cpu.write(c.arg(3), fname.encode('latin1') + b'\0')
        return 0

    def strncmp(self, c):
        a = self.cstr(c.arg(0)).encode('latin1')[:c.arg(2)]
        b = self.cstr(c.arg(1)).encode('latin1')[:c.arg(2)]
        return ((a > b) - (a < b)) & 0xffffffff

    # the port's stops at the end (module docstring)
    def items_end(self, uc, address, size, user):
        if self.short:
            uc.mem_write(uc.reg_read(UC_X86_REG_ESP) + 0x10, bytes(4))

    def item_id(self, uc, address, size, user):
        uc.mem_write(uc.reg_read(UC_X86_REG_ESP), struct.pack('<I', 0x7fffffff))

    def mates_end(self, uc, address, size, user):
        if self.short:
            uc.mem_write(uc.reg_read(UC_X86_REG_ESP) + 0x10, struct.pack('<I', 1))

    def campaigns_end(self, uc, address, size, user):
        if self.short:
            uc.mem_write(uc.reg_read(UC_X86_REG_ESP) + 0x10, bytes(4))



class Emulated(Files):
    def __init__(self, fs):
        self.log = []
        cpu = Original()
        cpu.uc.mem_map(BIG, 0x10000000)
        self.big = BIG
        Files.__init__(self, cpu, fs)
        cpu.stub_function(NEW, self.new)
        cpu.stub_function(DELETE, lambda c: 0)
        empty = self.string('')
        cpu.stub_function(CS_CTOR, lambda c: (c.u32(c.ecx, empty), c.ecx)[1])
        cpu.stub_function(CS_CTOR_S, lambda c: (c.u32(c.ecx, self.string(self.cstr(c.arg(0)))), c.ecx)[1], 4)
        cpu.stub_function(CS_DTOR, lambda c: 0)
        cpu.stub_function(CS_FORMAT, self.format)
        cpu.stub_function(CS_BUFFER, lambda c: self.r32(c.ecx), 4)
        cpu.stub_function(ATEXIT, lambda c: 0)
        cpu.stub_function(0x576040, self.message)
        cpu.stub_function(0x470e30, self.store_fill)
        cpu.stub_function(0x4733b0, self.add_mech, 8)
        cpu.stub_function(0x484840, self.missions)
        cpu.stub_function(0x4242a0, self.config_load)
        cpu.stub_function(0x424480, self.config_save, 4)
        cpu.stub_function(0x424350, lambda c: 0)
        cpu.stub_function(CONFIG_COPY, self.config_copy, 8)
        cpu.stub_function(RECOUNT, lambda c: self.log.append(f'recount {i32(self.r32(c.ecx))}') or 0)
        cpu.stub_function(SAVE_WORLD, lambda c: self.log.append(f'save_world {self.cstr(c.ecx)} {c.edx}') or 1)
        cpu.stub_function(MESSAGE_BOX, self.message_box)
        self.answers = []
        self.salvage = self.alloc(0x14)
        cpu.call(STORE_CTOR, ecx=self.salvage, stack_args=(0,))
        cpu.u32(SALVAGE, self.salvage)
        cpu.stub_import(IAT['strncmp'], self.strncmp, 0)
        cpu.stub_import(IAT['free'], lambda c: 0, 0)
        cpu.call(LIST_INIT)
        for a in SETUP:
            cpu.u32(a, 0)
        cpu.u32(CURRENT, 0)

    # the stubs
    def format(self, c):
        fmt = self.cstr(c.arg(1))
        args, n, out, i = [], 2, '', 0
        while i < len(fmt):
            ch = fmt[i]
            if ch != '%':
                out += ch
                i += 1
                continue
            j = i + 1
            while fmt[j] in '0123456789':
                j += 1
            spec = fmt[i:j + 1]
            if fmt[j] == 's':
                out += self.cstr(c.arg(n))
            else:
                out += spec % i32(c.arg(n))
            n += 1
            i = j + 1
        self.cpu.u32(c.arg(0), self.string(out))
        return c.arg(0)

    def message(self, c):
        self.log.append(f'message {c.ecx}')
        return self.string(f'MSG_{c.ecx}')

    def store_fill(self, c):
        self.log.append(f'store_fill {GAME_VT[self.r32(c.ecx - 8)]}')
        return 0

    def add_mech(self, c):
        self.log.append(f'add_mech {i32(c.arg(0))} {self.cstr(c.arg(1))}')
        return 1

    def missions(self, c):
        self.log.append(f'missions {i32(c.ecx)}')
        return i32(c.ecx) + 3

    def config_load(self, c):
        data, n = self.read(c.edx, 4)
        k = struct.unpack('<i', data)[0] if n == 4 else -1
        self.log.append(f'config_load {k}')
        if n != 4:
            return 0
        return self.fake_config(k)

    def config_copy(self, c):
        k = i32(self.r32(c.edx))
        self.log.append(f'config_copy {k}')
        return self.fake_config(k)

    def message_box(self, c):
        a = self.answers.pop(0) if self.answers else 1
        self.log.append(f'ask {c.arg(0)} {c.arg(1)} {c.arg(2)} {a}')
        return a

    def config_save(self, c):
        k = self.r32(c.ecx)
        self.log.append(f'config_save {i32(k)}')
        self.write(c.arg(0), struct.pack('<I', k))
        return 0

    # objects
    def pilots(self):
        head = self.r32(PILOTS)
        out, n = [], self.r32(head)
        while n != head:
            out.append(self.r32(n + 8))
            n = self.r32(n)
        return out

    def find(self, name):
        return self.call(FIND, ecx=self.string(name))

    def call(self, addr, ecx=0, edx=0, stack=()):
        self.cpu.call(addr, ecx=ecx, edx=edx, stack_args=stack)
        from unicorn.x86_const import UC_X86_REG_EAX
        return self.cpu.uc.reg_read(UC_X86_REG_EAX)

    def vec(self, a):
        first, last = self.r32(a), self.r32(a + 4)
        return [self.r32(first + 4 * i) for i in range((last - first) // 4)] if first else []

    def push(self, a, v):
        items = self.vec(a) + [v]
        arr = self.alloc(4 * len(items))
        for i, x in enumerate(items):
            self.cpu.u32(arr + 4 * i, x)
        self.cpu.u32(a, arr)
        self.cpu.u32(a + 4, arr + 4 * len(items))
        self.cpu.u32(a + 8, arr + 4 * len(items))

    def game(self, p, g, i):
        if g == 1:
            c = self.vec(p + 0x60)
            return c[i] if 0 <= i < len(c) else 0
        return self.r32(p + {0: 0x7c, 2: 0x70, 3: 0x74, 4: 0x78}[g])

    def item(self, kind, ident, extra):
        o = self.alloc(KIND_SIZE[kind])
        self.cpu.u32(o, KIND_VT[kind])
        self.cpu.u32(o + 0xc, 0xffffffff)
        if kind == 3:
            self.cpu.u32(o + 0x18, ident)
            self.cpu.u32(o + 0x1c, extra)
        else:
            self.cpu.u32(o + 0x14, ident)
            if kind == 1:
                self.cpu.u32(o + 0x18, extra)
            if kind == 4:
                self.cpu.write(o + 0x18, bytes([extra & 0xff]))
        return o

    def fake_config(self, k):
        """A stand-in configuration: its key at +0, as its mech too (+0x14), +0x1f modified."""
        o = self.alloc(0x20)
        self.cpu.write(o, bytes(0x20))
        self.cpu.u32(o, k & 0xffffffff)
        self.cpu.u32(o + 0x14, k & 0xffffffff)
        head = self.alloc(0xc)  # its items, as probe_pilot.cpp's fake_config: a weapon and its ammo
        self.cpu.u32(head, head)
        self.cpu.u32(head + 4, head)
        self.cpu.u32(o + 8, head)
        w = abs(k) % 4 * (1 if k >= 0 else -1)  # C's %
        for it in (self.item(1, w & 0xffffffff, 1), self.item(2, w & 0xffffffff, 0)):
            self.cpu.u32(it + 4, 1 if VT_KIND[self.r32(it)] == 1 else 10 + (k & 0xff))
            node, last = self.alloc(0xc), self.r32(head + 4)
            self.cpu.u32(node, head)
            self.cpu.u32(node + 4, last)
            self.cpu.u32(node + 8, it)
            self.cpu.u32(last, node)
            self.cpu.u32(head + 4, node)
        self.cpu.u32(o + 0xc, 2)
        return o

    def run(self, plan):
        self.log = []
        dump = []
        for line in plan:
            p = line.split()
            op = p[0]
            if op in ('B', 'U'):
                self.fs.damage(line)
            elif op == 'F':
                self.fs.put(unhx(p[1]), bytes.fromhex(p[2]) if p[2] != '-' else b'')
            elif op == 'C':
                r = self.call(CREATE, ecx=self.string(unhx(p[1])))
                self.log.append('created ' + (self.cstr(r + 0x18) if r else '-'))
            elif op == 'L':
                self.call(LOAD_ALL)
            elif op == 'D':
                pl = self.find(unhx(p[1]))
                if pl and pl == self.r32(CURRENT):
                    self.cpu.u32(CURRENT, 0)
                self.call(DELETE_PILOT, ecx=self.string(unhx(p[1])))
            elif op == 'R':
                pl = self.find(unhx(p[1]))
                if pl:
                    self.call(RENAME, ecx=pl, stack=(self.string(unhx(p[2])),))
            elif op == 'S':
                pl = self.find(unhx(p[1]))
                if pl:
                    self.call(SELECT, ecx=pl, stack=(int(p[2]) & 0xffffffff,))
            elif op == 'I':
                pl = self.find(unhx(p[1]))
                if pl:
                    r = self.call(SELECT_CAMPAIGN, ecx=pl, stack=(int(p[2]) & 0xffffffff,))
                    self.log.append(f'select_campaign {r & 0xff}')
            elif op == 'X':
                self.cpu.u32(CURRENT, 0 if p[1] == '-' else self.find(unhx(p[1])))
            elif op == 'O':
                self.call(OTHERS)
            elif op == 'Z':
                self.cpu.u32(CURRENT, 0)
                self.call(OTHERS)
            elif op == 'W':
                pl = self.find(unhx(p[1]))
                if pl:
                    self.call(OPEN, ecx=pl)
                    ok = self.call(HEADER, ecx=pl) & 0xff
                    if ok:
                        self.call(self.r32(self.r32(pl) + 4), ecx=pl)
                    h = self.r32(pl + 0x10)
                    if h != 0xffffffff:
                        self.handles.pop(h, None)
                    self.cpu.u32(pl + 0x10, 0xffffffff)
                    self.log.append(f'saved {1 if ok else 0}')
            elif op == 'G':
                r = self.call(DEBUG, ecx=int(p[1]) & 0xffffffff, edx=int(p[2]) & 0xffffffff)
                self.log.append('debug ' + (self.cstr(r + 0x18) if r else '-'))
            elif op == 'N':
                pl = self.find(unhx(p[1]))
                if pl:
                    o = self.alloc(200)
                    self.call(NEW_CAMPAIGN, ecx=o, stack=(pl, int(p[2]) & 0xffffffff, int(p[3]) & 0xffffffff))
                    self.push(pl + 0x60, o)
            elif op == 'V':
                pl = self.find(unhx(p[1]))
                g = self.game(pl, int(p[2]), int(p[3])) if pl else 0
                if g:
                    off, v = int(p[4], 16), int(p[5], 16)
                    size = next((s for gg, o, s in FIELDS if o == off and gg == int(p[2])), 4)
                    self.cpu.write(g + off, struct.pack('<I', v)[:size])
            elif op == 'Y':
                self.cpu.u32(int(p[1], 16), int(p[2], 16))
            elif op in 'AMQHJ':
                pl = self.find(unhx(p[1]))
                g = self.game(pl, int(p[2]), int(p[3])) if pl else 0
                off = int(p[4], 16)
                if not g or (off != 8 and int(p[2]) != 1):
                    continue
                s = g + off
                a = [int(x) for x in p[5:]]
                if op == 'A':
                    kind, n, ident, extra, group, part = a
                    it = self.item(kind, ident & 0xffffffff, extra & 0xffffffff)
                    self.cpu.u32(it + 4, n & 0xffffffff)
                    self.cpu.u32(it + 8, group & 0xffffffff)
                    self.cpu.u32(it + 0xc, part & 0xffffffff)
                    self.call(ADD_COPY, ecx=s, stack=(it, n & 0xffffffff))
                elif op == 'M':
                    self.call(ADD_AMMO, ecx=s, stack=(a[0] & 0xffffffff, a[1] & 0xffffffff))
                elif op == 'Q':
                    kind, ident, extra, n = a
                    it = self.item(kind, ident & 0xffffffff, extra & 0xffffffff)
                    r = self.call(REMOVE, ecx=s, stack=(it, n & 0xffffffff))
                    self.log.append(f'removed {r & 0xff}')
                elif op == 'J':
                    head = self.r32(s + 8)
                    first = self.r32(head)
                    if first != head:
                        self.cpu.write(self.r32(first + 8) + 0x10, b'\x01')
                else:
                    self.call(REMOVE_MECH, ecx=s, stack=(a[0] & 0xffffffff,))
            elif op == 'T':
                pl = self.find(unhx(p[1]))
                g = self.game(pl, 1, int(p[2])) if pl else 0
                if g:
                    mech, x14, skin, k = (int(x) for x in p[3:7])
                    sl = self.alloc(0x18)
                    self.call(SLOT_CTOR, ecx=sl, stack=(mech & 0xffffffff, 0, skin & 0xffffffff))
                    self.cpu.u32(sl + 0x14, x14 & 0xffffffff)
                    if k:
                        self.cpu.u32(sl + 8, self.fake_config(k))
                    self.push(g + 0x90, sl)
            elif op == 'h':  # the mission start's lancemate i: a new slot (mech, 0, 0) and its record
                pl = self.find(unhx(p[1]))
                g = self.game(pl, 1, int(p[2])) if pl else 0
                if g:
                    i, mech, x14 = (int(x) for x in p[3:6])
                    sl = self.alloc(0x18)
                    self.call(SLOT_CTOR, ecx=sl, stack=(mech & 0xffffffff, 0, 0))
                    self.cpu.u32(sl + 0x14, x14 & 0xffffffff)
                    self.call(SET_MATE, ecx=g, stack=(i & 0xffffffff, sl))
            elif op == 'k':  # and their count
                pl = self.find(unhx(p[1]))
                g = self.game(pl, 1, int(p[2])) if pl else 0
                if g:
                    self.call(SET_MATES, ecx=g, stack=(int(p[3]) & 0xffffffff,))
            elif op == 'K':
                pl = self.find(unhx(p[1]))
                g = self.game(pl, int(p[2]), int(p[3])) if pl else 0
                if g:
                    k = int(p[4])
                    self.cpu.u32(g + 0x28, self.fake_config(k) if k else 0)
            elif op == 'P':  # the save after a mission, with the overwrite question's answer
                pl = self.find(unhx(p[1]))
                if pl:
                    self.answers = [int(p[3])]
                    r = self.call(RECORD, ecx=pl, stack=(int(p[2]),))
                    self.log.append(f'recorded {r & 0xff}')
            elif op == 'l':
                pl = self.find(unhx(p[1]))
                g = self.game(pl, 1, int(p[2])) if pl else 0
                if g:
                    self.call(LANCE_RESET, ecx=g, stack=(int(p[3]),))
            elif op == 's':  # an item in the mission's salvage
                kind, n, ident, extra, group, part = (int(x) for x in p[1:7])
                it = self.item(kind, ident & 0xffffffff, extra & 0xffffffff)
                self.cpu.u32(it + 8, group & 0xffffffff)
                self.cpu.u32(it + 0xc, part & 0xffffffff)
                self.call(ADD_COPY, ecx=self.salvage, stack=(it, n & 0xffffffff))
            elif op == 'f':  # every configuration of a campaign marked modified
                pl = self.find(unhx(p[1]))
                g = self.game(pl, 1, int(p[2])) if pl else 0
                if g:
                    for sl in [g + 0x20] + self.vec(g + 0x90):
                        cfg = self.r32(sl + 8)
                        if cfg:
                            self.cpu.write(cfg + 0x1f, b'\x01')
            elif op == 'E':
                dump.append(self.dump())
        return '\n'.join(self.log) + '\n--\n' + ''.join(dump)

    # the dump probe_pilot.cpp writes
    def dump(self):
        o = []
        cur = self.r32(CURRENT)
        ps = self.pilots()
        o.append(f'pilots {len(ps)} current "{self.cstr(cur + 0x18) if cur else "-"}"\n')
        for p in ps:
            o.append(self.dump_pilot(p))
        o.append('setup ' + ''.join(f'{i32(self.r32(a))} ' for a in SETUP[:5]))
        o.append(''.join(f'{i32(self.r32(0x605838 + 4 * i))} {i32(self.r32(0x605858 + 4 * i))} ' for i in range(6)))
        o.append(f'{i32(self.r32(0x605874))} {i32(self.r32(0x605878))} {i32(self.r32(0x60587c))}\n')
        return ''.join(o)

    def item_text(self, it):
        k = VT_KIND.get(self.r32(it), 99)
        s = f'[k{k} n{i32(self.r32(it + 4))} g{i32(self.r32(it + 8))} p{i32(self.r32(it + 0xc))} b{self.r8(it + 0x10)}'
        if k == 3:
            s += f' id{i32(self.r32(it + 0x18))} t{i32(self.r32(it + 0x1c))}'
        else:
            s += f' id{i32(self.r32(it + 0x14))}'
            if k == 1:
                s += f' t{i32(self.r32(it + 0x18))}'
            if k == 4:
                s += f' x{self.r8(it + 0x18)}'
        return s + '] '

    def store(self, name, s):
        head = self.r32(s + 8)
        items, n = [], self.r32(head)
        while n != head:
            items.append(self.r32(n + 8))
            n = self.r32(n)
        return f'{name}{{b{self.r8(s + 4)} f{self.r8(s + 0x10)} n{i32(self.r32(s + 0xc))} ' + \
            ''.join(self.item_text(i) for i in items) + '} '

    def slot(self, name, s):
        cfg = self.r32(s + 8)
        k = i32(self.r32(cfg)) if cfg else -999
        return f'{name}({i32(self.r32(s + 4))},{i32(self.r32(s + 0xc))},{i32(self.r32(s + 0x10))},' \
            f'{i32(self.r32(s + 0x14))},{k}) '

    def game_text(self, g):
        return self.store('store', g + 8) + self.slot('slot', g + 0x20)

    def dump_pilot(self, p):
        kv = lambda k, v: f'{k}={v} '  # noqa: E731
        o = [f'pilot "{self.cstr(p + 0x18)}" ', kv('magic', i32(self.r32(p + 4))), kv('version', i32(self.r32(p + 8))),
             kv('x0c', i32(self.r32(p + 0xc))), kv('open', int(self.r32(p + 0x10) != 0xffffffff)),
             kv('dirty', self.r8(p + 0x58)), kv('b59', self.r8(p + 0x59)), kv('mech', self.r32(p + 0x14)),
             kv('x80', i32(self.r32(p + 0x80)))]
        lab, act, multi, train = (self.r32(p + x) for x in (0x7c, 0x70, 0x74, 0x78))
        camps = self.vec(p + 0x60)
        c = self.r32(p + 0x6c)
        cur = {lab: -1, act: -2, multi: -3, train: -4}.get(c, -100)
        if c in camps:
            cur = camps.index(c)
        if not c:
            cur = -99
        o += [kv('cur', cur), kv('ncamp', len(camps))]
        o.append('\n lab ' + self.game_text(lab) + kv('level', i32(self.r32(lab + 0x38))))
        o += [self.slot('l', lab + 0x3c + 0x18 * i) for i in range(3)]
        o.append('\n action ' + self.game_text(act))
        o += [f'{i32(self.r32(act + x))} ' for x in range(0x4c, 0x6c, 4)]
        o += [kv(k, i32(self.r32(act + x))) for k, x in (('x6c', 0x6c), ('x74', 0x74), ('x78', 0x78), ('x7c', 0x7c),
                                                       ('x170', 0x170), ('x174', 0x174))]
        o.append(kv('x178', struct.unpack('<h', self.cpu.read(act + 0x178, 2))[0]))
        o += [self.slot('l', act + 0x80 + 0x18 * i) for i in range(10)]
        o.append('\n multi ' + self.game_text(multi))
        o += [kv(k, i32(self.r32(multi + x)) if k != 'x74' else self.r32(multi + x))
              for k, x in (('x70', 0x70), ('x74', 0x74), ('x78', 0x78), ('x7c', 0x7c), ('x84', 0x84))]
        o += [kv(k, self.r8(multi + x)) for k, x in (('b8c', 0x8c), ('b8d', 0x8d), ('b8e', 0x8e), ('b8f', 0x8f))]
        o += [kv(k, i32(self.r32(multi + x))) for k, x in (('x90', 0x90), ('x94', 0x94), ('kills', 0x98),
                                                         ('deaths', 0x9c))]
        o.append(kv('ba8', self.r8(multi + 0xa8)))
        o.append('\n training ' + self.game_text(train) + kv('x38', i32(self.r32(train + 0x38))))
        o += [f'{self.r8(train + 0x3c + i)} ' for i in range(4)]
        o.append(kv('mission', i32(self.r32(train + 0x40))))
        for i, cg in enumerate(camps):
            o.append(f'\n campaign{i} ' + self.game_text(cg))
            o += [kv('b38', self.r8(cg + 0x38)), kv('x9c', i32(self.r32(cg + 0x9c))),
                  kv('mission', i32(self.r32(cg + 0xa0))), kv('campaign', i32(self.r32(cg + 0xa4))),
                  kv('a8', i32(self.r32(cg + 0xa8))), kv('ac', i32(self.r32(cg + 0xac))), kv('b0', self.r8(cg + 0xb0))]
            o += [f'{i32(self.r32(cg + 0xb4 + 4 * k))} ' for k in range(4)]
            o.append(kv('c4', i32(self.r32(cg + 0xc4))))
            o += [self.store(n, cg + x) for n, x in (('s3c', 0x3c), ('s50', 0x50), ('s64', 0x64), ('s78', 0x78))]
            o.append('mates ')
            o += [self.slot('m', s) for s in self.vec(cg + 0x90)]
        o.append('\n')
        return ''.join(o)


# ---- the file's layout, for damage aimed at its counts ----

def layout(data):
    """(offset, what) of each count and field in a pilot file, as far as it parses."""
    out, pos = [], [12]

    def take(n, what):
        if pos[0] + n > len(data):
            raise EOFError
        out.append((pos[0], what, n))
        pos[0] += n
        return int.from_bytes(data[pos[0] - n:pos[0]], 'little')

    def slot():
        take(4, 'f')
        take(4, 'f')
        take(4, 'f')
        if take(4, 'flag'):
            take(4, 'f')

    def store():
        for _ in range(min(take(4, 'items'), 200)):
            kind = take(4, 'kind')
            for _ in range(4):
                take(4, 'f')
            if kind in (1, 3):
                take(4, 'f')
            elif kind == 4:
                take(1, 'f')
            elif kind not in (0, 2):
                raise EOFError

    try:
        for sizes in ((4, 4), (4, 4, 4, 4, 1, 1, 1, 1, 4, 4, 1, 4), (4,) * 31, ()):
            slot()
            store()
            for n in sizes:
                take(n, 'f')
        for _ in range(min(take(4, 'campaigns'), 50)):
            slot()
            store()
            for n in (4, 4, 1, 4, 4, 4, 16):
                take(n, 'f')
            for _ in range(4):
                store()
            for _ in range(min(take(4, 'mates'), 50)):
                slot()
    except EOFError:
        pass
    return out


def damage(r, name, data):
    lines = []
    for _ in range(r.choice([1, 1, 2, 3])):
        roll = r.random()
        fields = layout(data)
        if roll < 0.25:
            lines.append(f'U {hx(name)} {r.randrange(0, len(data) + 1):x}')
        elif roll < 0.35:
            pos = r.randrange(0, 12)
            lines.append(f'B {hx(name)} {pos:x} {r.randrange(256):02x}')
        elif roll < 0.75 and fields:
            counts = [f for f in fields if f[1] in ('items', 'campaigns', 'mates', 'flag', 'kind')]
            off, what, n = r.choice(counts or fields)
            if what == 'kind':
                v = r.choice([0, 1, 2, 3, 4, 5, 0xffffffff])
            elif what == 'flag':
                v = r.choice([0, 1, 2])
            else:
                v = r.choice([0, 1, 2, 5, 0x40, 0x7fffffff, 0x80000000, 0xffffffff])
            lines.append(f'B {hx(name)} {off:x} {struct.pack("<I", v).hex()}')
        elif fields:
            off, what, n = r.choice([f for f in fields if f[1] == 'f'] or fields)
            lines.append(f'B {hx(name)} {off:x} {r.randrange(256):02x}')
    return lines


# ---- the plan ----

def mutate(r, name):
    lines = []
    stores = [0x3c, 0x50, 0x64, 0x78]
    for _ in range(r.randrange(1, 12)):
        roll = r.random()
        g = r.randrange(5)
        i = r.randrange(3)
        off = 8 if g != 1 or r.random() < 0.4 else r.choice(stores)
        if roll < 0.35:
            gg, foff, size = r.choice(FIELDS)
            v = r.choice([0, 1, 2, 3, 7, 0xff, 0x12345678, 0xffffffff, r.randrange(1 << 32)])
            lines.append(f'V {hx(name)} {gg} {i} {foff:x} {v:x}')
        elif roll < 0.4:
            lines.append(f'Y {r.choice(SETUP):x} {r.randrange(1 << 32):x}')
        elif roll < 0.64:
            lines.append(f'A {hx(name)} {g} {i} {off:x} {r.randrange(5)} {r.choice([1, 2, 5, -3, 0])} {r.randrange(4)} '
                         f'{r.choice([1, 3])} {r.randrange(3)} {r.choice([-1, 0, 4])}')
        elif roll < 0.71:
            lines.append(f'M {hx(name)} {g} {i} {off:x} {r.choice([0, 1, 2, 17])} {r.choice([10, -20, 100, -5000])}')
        elif roll < 0.77:
            lines.append(f'Q {hx(name)} {g} {i} {off:x} {r.randrange(5)} {r.randrange(4)} {r.choice([1, 3])} '
                         f'{r.choice([1, 2, 9])}')
        elif roll < 0.81:
            lines.append(f'H {hx(name)} {g} {i} {off:x} {r.choice([1, 2, 3])}')
        elif roll < 0.84:
            lines.append(f'J {hx(name)} {g} {i} {off:x}')
        elif roll < 0.92:
            lines.append(f'T {hx(name)} {i} {r.randrange(20)} {r.randrange(3)} {r.randrange(4)} '
                         f'{r.choice([0, 0, 5, 11])}')
        else:
            lines.append(f'K {hx(name)} {g} {i} {r.choice([0, 3, 77])}')
    return lines


def campaign_steps(r, n):
    """A campaign played on: its stores, lancemates and loadouts set, the
    mission's salvage, then the save after the mission (0x00478f10)."""
    c, m = r.choice([(1, 1), (1, 3), (1, 4), (2, 4), (3, 6), (4, 3), (4, 4), (4, 6), (5, 1), (0, 9)])
    i = r.randrange(3)
    lines = [f'C {hx(n)}', f'I {hx(n)} 0', f'V {hx(n)} 1 0 a4 {c & 0xffffffff:x}',
             f'V {hx(n)} 1 0 a0 {m & 0xffffffff:x}']
    if r.random() < 0.5:  # a saved game of the next mission already there
        nc, nm = (c, m + 1) if m + 1 <= c + 3 else (c + 1, 1)
        lines.append(f'N {hx(n)} {nc} {nm}')
    for _ in range(r.randrange(0, 6)):
        lines.append(f'A {hx(n)} 1 0 {r.choice([8, 0x3c, 0x50, 0x64, 0x78]):x} {r.randrange(5)} '
                     f'{r.choice([1, 2, 5])} {r.randrange(3)} {r.choice([1, 3])} {r.randrange(3)} {r.choice([-1, 2])}')
    mates = r.randrange(0, 3)
    for _ in range(mates):
        mech = r.choice([3, 3, 0x13, r.randrange(20)])  # the player's mech again, and 0x13, which is never listed
        lines.append(f'T {hx(n)} 0 {mech} {r.randrange(3)} {r.randrange(4)} {r.choice([0, mech, mech, 77])}')
    if r.random() < 0.5:  # the mission start's lancemates (player_setup): each a slot there or the next
        k = 0
        for _ in range(r.randrange(1, 4)):
            i = k if r.random() < 0.6 or k >= mates else r.randrange(mates + 1)
            lines.append(f'h {hx(n)} 0 {i} {r.randrange(19)} {r.randrange(-1, 30)}')
            mates += i == mates
            k += 1
        lines.append(f'k {hx(n)} 0 {r.choice([k, k, 0, 5, -1])}')
    if r.random() < 0.6:
        mech = r.choice([3, 5])
        lines += [f'V {hx(n)} 1 0 24 {mech:x}', f'K {hx(n)} 1 0 {r.choice([mech, mech, 0, 9])}']
    if r.random() < 0.7:
        lines.append(f'V {hx(n)} 1 0 9c {r.choice([0, 1, 2, 5, 0xffffffff]):x}')
    for off in (0xa8, 0xac, 0xb4, 0xb8, 0xbc, 0xc0, 0xc4, 0xb0, 0x2c, 0x34):  # the fields the next game takes or not
        if r.random() < 0.5:
            lines.append(f'V {hx(n)} 1 0 {off:x} {r.choice([0, 1, 2, 5, 9, 0x101]):x}')
    if r.random() < 0.4:
        lines.append(f'f {hx(n)} 0')
    for _ in range(r.randrange(0, 4)):
        lines.append(f's {r.randrange(5)} {r.choice([1, 2, 5])} {r.randrange(3)} {r.choice([1, 3])} {r.randrange(3)} -1')
    if r.random() < 0.2:
        lines.append(f'l {hx(n)} 0 {r.randrange(2)}')
    if r.random() < 0.2:
        lines.append(f'X {hx(n)}')
    lines.append(f'P {hx(n)} {int(r.random() < 0.6)} {int(r.random() < 0.6)}')
    if r.random() < 0.3:
        lines.append(f'P {hx(n)} {int(r.random() < 0.5)} {int(r.random() < 0.5)}')
    lines.append('E')
    return lines


def initial_files(r):
    lines = []
    for _ in range(r.choice([0, 0, 1, 2])):
        n = r.choice(NAMES)
        kind = r.random()
        if kind < 0.4:
            data = struct.pack('<iii', 1, 0x2f, 0) + bytes(r.randrange(256) for _ in range(r.randrange(0, 40)))
        elif kind < 0.7:
            data = struct.pack('<iii', r.choice([0, 1, 2]), r.choice([0x2f, 0x2e, 15]), 0) + bytes(r.randrange(0, 4))
        else:
            data = bytes(r.randrange(256) for _ in range(r.randrange(0, 11)))
        path = 'pilots\\' + n + '.mw3'
        lines.append(f'F {hx(path)} {data.hex() or "-"}')
    return lines


def make_plan(seed):
    r = random.Random(f'pilot:{seed}')
    plan = initial_files(r)
    if r.random() < 0.8:
        plan.append('L')
    names = []
    for _ in range(r.randrange(25, 70)):
        roll = r.random()
        n = r.choice(names) if names and r.random() < 0.8 else r.choice(NAMES)
        if roll < 0.15:
            plan.append(f'C {hx(n if r.random() < 0.95 else "")}')
            names.append(n)
        elif roll < 0.50:
            plan += mutate(r, n)
        elif roll < 0.54:
            c, m = r.choice([(1, 1), (1, 2), (2, 3), (3, 6), (4, 1)])
            plan.append(f'N {hx(n)} {c} {m}')
            if r.random() < 0.7:  # its saved game, so a reload keeps it
                plan.append(f'F {hx(f"pilots//games//{n}{c:02d}{m:02d}.mw3")} 00')
        elif roll < 0.57:
            plan.append(f'R {hx(n)} {hx(r.choice(NAMES))}')
        elif roll < 0.59:
            plan.append(f'D {hx(n)}')
        elif roll < 0.61:
            plan += ['Z', 'L']  # everything saved and read back
        elif roll < 0.66:
            plan.append(f'S {hx(n)} {r.choice([0, 1, 2, 3, 4, 5, -1])}')
        elif roll < 0.70:
            plan.append(f'I {hx(n)} {r.choice([-1, 0, 1, 2, 5])}')
        elif roll < 0.74:
            plan.append(f'X {hx(n) if r.random() < 0.8 else "-"}')
        elif roll < 0.82:
            plan.append(f'W {hx(n)}')
        elif roll < 0.84:
            plan.append(f'G {r.choice([0, 1, 2, 4, 7])} {r.choice([0, 1, 3, 9])}')
        elif roll < 0.86:
            plan.append('O')
        elif roll < 0.88:
            plan.append('L')
        elif roll < 0.93:  # a saved campaign filled, saved, read back (its stores merged), shown
            c, m = r.choice([(1, 2), (2, 3), (3, 1), (4, 6)])
            plan += [f'C {hx(n)}', f'N {hx(n)} {c} {m}', f'F {hx(f"pilots//games//{n}{c:02d}{m:02d}.mw3")} 00']
            for _ in range(r.randrange(2, 8)):
                plan.append(f'A {hx(n)} 1 1 {r.choice([8, 0x3c, 0x50, 0x64, 0x78]):x} {r.randrange(5)} '
                            f'{r.choice([1, 2, 5])} {r.randrange(3)} {r.choice([1, 3])} {r.randrange(3)} -1')
            plan += [f'V {hx(n)} 1 1 a8 {r.randrange(9):x}', f'V {hx(n)} 1 1 ac {r.randrange(9):x}', 'E', 'Z', 'L', 'E']
        elif roll < 0.96:
            plan += campaign_steps(r, n)
        elif roll < 0.97:
            plan.append('E')
    plan += ['E', 'Z', 'E']
    return plan, r


def damage_plan(r, fs):
    lines = []
    for n, data in fs.snapshot():
        if n.lower().startswith('pilots/') and n.count('/') == 1 and r.random() < 0.7:
            lines += damage(r, n.replace('/', '\\'), data)
    lines += ['L', 'E']
    for _ in range(r.randrange(0, 6)):
        ps = [n.rsplit('/', 1)[1][:-4] for n, _ in fs.snapshot() if n.count('/') == 1 and n.lower().endswith('.mw3')]
        if not ps:
            break
        n = r.choice(ps)
        lines += r.choice([[f'W {hx(n)}'], [f'S {hx(n)} 1'], mutate(r, n), [f'R {hx(n)} {hx(r.choice(NAMES))}']])
    lines += ['E', 'Z', 'E']
    return lines


def folder_files(d):
    out = []
    for p in sorted(d.rglob('*')):
        if p.is_file():
            out.append((str(p.relative_to(d)), p.read_bytes()))
    return sorted(out)


def run_case(seed, verbose):
    plan, r = make_plan(seed)
    fs = FS()
    orig = Emulated(fs)
    first = orig.run(plan)
    plan2 = damage_plan(r, fs)
    second = orig.run(plan2)
    fs_o = fs.snapshot()
    d = DIR / str(seed)
    shutil.rmtree(d, ignore_errors=True)
    d.mkdir(parents=True)
    buf = ctypes.create_string_buffer(1 << 24)
    full = plan + ['--damage--'] + plan2
    n = probe.probe_pilot_run(str(d).encode(), '\n'.join(full).encode(), buf, len(buf))
    assert n >= 0, 'probe failed'
    port = buf.value.decode('latin1')
    want = first + second
    # The port's output is one log and the dumps; the original's is in two halves.
    o_log = [x for x in (first.split('\n--\n')[0] + '\n' + second.split('\n--\n')[0]).split('\n') if x]
    o_dump = first.split('\n--\n')[1] + second.split('\n--\n')[1]
    p_log = [x for x in port.split('\n--\n')[0].split('\n') if x]
    p_dump = port.split('\n--\n')[1]
    fs_p = folder_files(d)
    ok = True
    if o_log != p_log:
        ok = False
        for i, (a, b) in enumerate(zip(o_log, p_log)):
            if a != b:
                print(f'pilot seed {seed}: call {i} differs: original {a!r}, rebuild {b!r}')
                break
        else:
            print(f'pilot seed {seed}: {len(o_log)} calls against {len(p_log)}')
    if o_dump != p_dump:
        ok = False
        for a, b in zip(o_dump.split('\n'), p_dump.split('\n')):
            if a != b:
                ta, tb = a.split(' '), b.split(' ')
                print(f'pilot seed {seed}: dumps differ\n  original: {" ".join(x for x in ta if x not in tb)[:300]}'
                      f'\n  rebuild:  {" ".join(x for x in tb if x not in ta)[:300]}\n  in: {a[:200]}')
                break
        else:
            print(f'pilot seed {seed}: dumps differ in length')
    if [(k.lower(), v) for k, v in fs_o] != [(k.lower(), v) for k, v in fs_p] or \
            [k for k, _ in fs_o] != [k for k, _ in fs_p]:
        ok = False
        print(f'pilot seed {seed}: files differ')
        for (ka, va), (kb, vb) in zip(fs_o, fs_p):
            if ka != kb or va != vb:
                pos = next((i for i in range(min(len(va), len(vb))) if va[i] != vb[i]), min(len(va), len(vb)))
                print(f'  {ka} ({len(va)}) / {kb} ({len(vb)}), from byte {pos}')
                break
        else:
            print(f'  {[k for k, _ in fs_o]} / {[k for k, _ in fs_p]}')
    if ok:
        shutil.rmtree(d, ignore_errors=True)
    if verbose:
        print(f'pilot seed {seed}: {len(plan) + len(plan2)} steps, {len(o_log)} calls, {len(fs_o)} files'
              + (' ok' if ok else ''))
    return ok, len(o_log), len(fs_o), sum(len(v) for _, v in fs_o)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    runs = int(args[0]) if args else 40
    first = int(args[1]) if len(args) > 1 else 0
    verbose = '--verbose' in sys.argv
    passed = calls = files = size = 0
    for seed in range(first, first + runs):
        ok, c, f, s = run_case(seed, verbose)
        passed += ok
        calls += c
        files += f
        size += s
    print(f'pilot: {passed}/{runs} runs identical ({calls} calls, {files} files, {size} bytes compared)')
    return 0 if passed == runs else 1


if __name__ == '__main__':
    # Out with os._exit: the interpreter's own shutdown can hang with the
    # emulator and the probe's callbacks in one process (macOS).
    code = 1
    try:
        code = main()
    except BaseException:
        import traceback
        traceback.print_exc()
    sys.stdout.flush()
    sys.stderr.flush()
    os._exit(code)
