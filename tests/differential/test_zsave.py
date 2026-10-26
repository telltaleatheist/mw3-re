"""zSave (docs/save.md) against the original x86.

    cmake --build build && .venv/bin/python -P tests/differential/test_zsave.py [runs] [first] [--verbose] [--nofix]

The original's zSave (0x005a8a20..0x005a9970) and the archive code under it
(zutl_zar, 0x005764e0..0x00576b30) run in the emulator over a fake file
system behind CreateFileA, ReadFile, WriteFile, SetFilePointer, GetFileSize
and CloseHandle; the port (tests/probe_zsave.cpp) runs the same plan on a real
folder as its write dir. Fake registrants (up to eight names, some alike or
missing on a load) write the plan's entries from an x86 routine through
0x005a8a50 and log what each load gives them, one of them asking the load to
stop (+0x30). The plan: registrations at random priorities, saves of every
kind, the header's kind read, loads with the right kind, any kind and another,
loads with registrants missing, then every file damaged in place (cut short,
bytes written over the table's count, an entry's start, length or name, the
header's entry, the version) and loaded again. Compared: every call's result,
each load call (registrant, sub-name, size, bytes) and every file's bytes
(the exe's entries carry its stack's leftovers in the flags, comment and file
time; they are zeroed after each save, as the port writes them).

The port's fixes (FIX (bug), save.md) are applied to the original by code
hooks so the rest compares: a table count the file cannot hold fails the
open (0x005767b0), an entry name without "/<sub>" is skipped (the strncpy of
a null at 0x005a8f30 / 0x005a8f50) and an entry longer than the file is
skipped (0x005a9012). --nofix leaves them out, to see what the exe does.
"""
import ctypes
import random
import shutil
import struct
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import Original  # noqa: E402
from twod_world import IAT  # noqa: E402
from calltrace import load_probe  # noqa: E402
from unicorn import UC_HOOK_CODE, UcError  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EIP, UC_X86_REG_ESP  # noqa: E402

probe = load_probe()
probe.probe_zsave_run.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]

NEW, DELETE = 0x5abd26, 0x5abd1a
INIT, SHUTDOWN, ADD, SAVE, LOAD, KIND = 0x5a8b30, 0x5a8bb0, 0x5a8a20, 0x5a8a70, 0x5a8a90, 0x5a90d0
MANAGER, WRITE = 0x736580, 0x5a8a50
READ_TABLE, OWNER_COPIED, SUB_COPIED, NEXT_ENTRY, SIZE_READ = 0x5767b0, 0x5a8f32, 0x5a8f52, 0x5a9081, 0x5a9012
FOUND = 0x5a8fe2  # cl: the entry's owner is registered
NAMES = ['Anim', 'RunningAnim', 'AnimActivation', 'GWWorld', 'Weapons', 'zDEClient', 'anim', 'W']
PATHS = ['pilots//games//bob0102.mw3', 'pilots//games//bob0103.mw3', 'save.zbd']
MAX_CHUNKS = 16


def key(path):
    p = path.replace('\\', '/').lower()
    while '//' in p:
        p = p.replace('//', '/')
    return p


def hx(b):
    return b.hex() if b else '-'


def s32(v):
    return struct.unpack('<i', struct.pack('<I', v & 0xffffffff))[0]


class Emulated:
    def __init__(self, fixes=True):
        self.cpu = cpu = Original()
        self.fixes = fixes
        self.files = {}        # key -> bytearray
        self.handles = {}      # handle -> [key, pos]
        self.next_handle = 0x100
        self.sizes = {}
        self.tok = None
        self.null_copy = False
        self.log = []
        self.fixed = []        # which of the port's fixes the run needed
        self.stops = set()
        for name, fn, pop in (('CreateFileA', self.create_file, 28), ('ReadFile', self.read_file, 20),
                              ('WriteFile', self.write_file, 20), ('SetFilePointer', self.seek, 16),
                              ('CloseHandle', self.close, 4), ('GetFileSize', self.file_size, 8),
                              ('GetFileTime', self.file_time, 16), ('GetLastError', lambda c: 2, 0),
                              ('FormatMessageA', lambda c: 0, 28), ('LocalFree', lambda c: 0, 4),
                              ('_stricmp', self.stricmp, 0), ('strtok', self.strtok, 0),
                              ('strncpy', self.strncpy, 0), ('sprintf', self.sprintf, 0),
                              ('realloc', self.realloc, 0), ('malloc', lambda c: self.alloc(c.arg(0)), 0),
                              ('free', lambda c: 0, 0), ('_strdup', self.strdup, 0)):
            cpu.stub_import(IAT[name], fn, pop)
        cpu.stub_function(NEW, lambda c: self.alloc(c.arg(0)))
        cpu.stub_function(DELETE, lambda c: 0)
        uc = cpu.uc
        uc.hook_add(UC_HOOK_CODE, self.table_check, begin=READ_TABLE, end=READ_TABLE)
        uc.hook_add(UC_HOOK_CODE, self.copied, begin=OWNER_COPIED, end=OWNER_COPIED)
        uc.hook_add(UC_HOOK_CODE, self.copied, begin=SUB_COPIED, end=SUB_COPIED)
        uc.hook_add(UC_HOOK_CODE, self.size_check, begin=SIZE_READ, end=SIZE_READ)
        uc.hook_add(UC_HOOK_CODE, self.found, begin=FOUND, end=FOUND)
        self.missing = 0
        # The registrants: their names, an entry table each ([count] then
        # [sub, data, size] rows), the save routine and the load stub.
        self.name_at = [self.string(n) for n in NAMES]
        self.tables = [self.alloc(4 + 12 * MAX_CHUNKS) for _ in NAMES]
        self.save_fn = cpu.code(bytes.fromhex(
            '535657' '89ce' '89d3' '8b3b' '83c304'             # push ebx/esi/edi; esi ctx; ebx table; edi count
            '85ff' '7417' '89f1' '8b13' 'ff7308' 'ff7304'      # loop: ...; ecx ctx; edx sub; push size, data
            'b8' + struct.pack('<I', WRITE).hex() + 'ffd0'    # call 0x005a8a50
            '83c30c' '4f' 'ebe5' '5f5e5bc3'))
        self.load_fn = cpu.code(b'\xc3' * 16)
        cpu.stub_function(self.load_fn, self.load_call, pop_bytes=12)

    # memory
    def r32(self, a):
        return struct.unpack('<I', self.cpu.read(a, 4))[0]

    def cstr(self, a):
        if not a:
            return b''
        out = bytearray()
        while True:
            b = self.cpu.read(a + len(out), 64)
            i = b.find(b'\0')
            if i >= 0:
                return bytes(out + b[:i])
            out += b

    def alloc(self, n):
        if n > 0x100000:
            return 0  # the heap refuses it
        a = self.cpu.alloc(max(n, 4))
        self.cpu.write(a, b'\0' * max(n, 4))
        self.sizes[a] = n
        return a

    def string(self, s):
        b = s.encode('latin1') if isinstance(s, str) else s
        a = self.alloc(len(b) + 1)
        self.cpu.write(a, b + b'\0')
        return a

    # the C runtime
    def strdup(self, c):
        return self.string(self.cstr(c.arg(0)))

    def realloc(self, c):
        p, n = c.arg(0), c.arg(1)
        a = self.alloc(n)
        if a and p:
            self.cpu.write(a, self.cpu.read(p, min(self.sizes.get(p, 0), n)))
        return a

    def stricmp(self, c):
        a, b = self.cstr(c.arg(0)).lower(), self.cstr(c.arg(1)).lower()
        return ((a > b) - (a < b)) & 0xffffffff

    def strncpy(self, c):
        d, s, n = c.arg(0), c.arg(1), c.arg(2)
        if not s:
            self.null_copy = True  # the exe reads address 0 here
            return d
        src = self.cstr(s)[:n]
        self.cpu.write(d, src + b'\0' * (n - len(src)))
        return d

    def strtok(self, c):
        s, delims = c.arg(0), set(self.cstr(c.arg(1)))
        p = s if s else self.tok
        if not p:
            return 0
        while self.cpu.read(p, 1)[0] and self.cpu.read(p, 1)[0] in delims:
            p += 1
        if not self.cpu.read(p, 1)[0]:
            self.tok = p
            return 0
        start = p
        while self.cpu.read(p, 1)[0] and self.cpu.read(p, 1)[0] not in delims:
            p += 1
        if self.cpu.read(p, 1)[0]:
            self.cpu.write(p, b'\0')
            p += 1
        self.tok = p
        return start

    def sprintf(self, c):
        fmt, args, out, i, n = self.cstr(c.arg(1)), 2, bytearray(), 0, 0
        while i < len(fmt):
            if fmt[i:i + 2] == b'%s':
                out += self.cstr(c.arg(args))
                args, i = args + 1, i + 2
            elif fmt[i:i + 2] == b'%d':
                out += str(s32(c.arg(args))).encode()
                args, i = args + 1, i + 2
            else:
                out.append(fmt[i])
                i += 1
        self.cpu.write(c.arg(0), bytes(out) + b'\0')
        return len(out)

    # the files
    def create_file(self, c):
        k, disp = key(self.cstr(c.arg(0)).decode('latin1')), c.arg(4)
        if disp in (2, 5) or (disp in (1, 4) and k not in self.files):
            if disp == 5 and k not in self.files:
                return 0xffffffff
            self.files[k] = bytearray()
        elif k not in self.files:
            return 0xffffffff
        h = self.next_handle
        self.next_handle += 4
        self.handles[h] = [k, 0]
        return h

    def read_file(self, c):
        h, buf, n, got = c.arg(0), c.arg(1), c.arg(2), c.arg(3)
        if h not in self.handles:
            return 0
        k, pos = self.handles[h]
        data = bytes(self.files[k][pos:pos + n])
        if data:
            self.cpu.write(buf, data)
        self.handles[h][1] = pos + len(data)
        if got:
            self.cpu.u32(got, len(data))
        return 1

    def write_file(self, c):
        h, buf, n, done = c.arg(0), c.arg(1), c.arg(2), c.arg(3)
        if h not in self.handles:
            return 0
        k, pos = self.handles[h]
        f = self.files[k]
        if len(f) < pos:
            f.extend(bytes(pos - len(f)))
        f[pos:pos + n] = self.cpu.read(buf, n)
        self.handles[h][1] = pos + n
        if done:
            self.cpu.u32(done, n)
        return 1

    def seek(self, c):
        h, dist, method = c.arg(0), s32(c.arg(1)), c.arg(3)
        if h not in self.handles:
            return 0xffffffff
        k, pos = self.handles[h]
        base = (0, pos, len(self.files[k]))[method]
        if base + dist < 0:
            return 0xffffffff  # ERROR_NEGATIVE_SEEK: the position is kept
        self.handles[h][1] = base + dist
        return base + dist

    def close(self, c):
        return 1 if self.handles.pop(c.arg(0), None) is not None else 0

    def file_size(self, c):
        h = c.arg(0)
        return len(self.files[self.handles[h][0]]) if h in self.handles else 0xffffffff

    def file_time(self, c):
        for a in (c.arg(1), c.arg(2), c.arg(3)):
            if a:
                self.cpu.write(a, bytes(8))
        return 1

    # the port's fixes, applied to the original (module docstring)
    def ret(self, uc, eax):
        esp = uc.reg_read(UC_X86_REG_ESP)
        uc.reg_write(UC_X86_REG_EAX, eax)
        uc.reg_write(UC_X86_REG_EIP, self.r32(esp))
        uc.reg_write(UC_X86_REG_ESP, esp + 4)

    def table_check(self, uc, address, size, user):
        h = self.r32(uc.reg_read(UC_X86_REG_ECX) + 4)
        if h not in self.handles:
            return
        f = self.files[self.handles[h][0]]
        if len(f) < 8:
            return
        version, count = struct.unpack_from('<Ii', f, len(f) - 8)
        if version == 1 and (count < 0 or count * 0x94 > len(f) - 8):
            self.fixed.append('table' if self.fixes else 'unfixed table')
            if self.fixes:
                self.ret(uc, 0)

    def copied(self, uc, address, size, user):
        if not self.null_copy:
            return
        self.null_copy = False
        if not self.fixes:
            self.fixed.append('unfixed name')
            raise UcError(0)  # the exe reads address 0
        self.fixed.append('name')
        uc.reg_write(UC_X86_REG_ESP, uc.reg_read(UC_X86_REG_ESP) + 0xc)
        uc.reg_write(UC_X86_REG_EIP, NEXT_ENTRY)

    def size_check(self, uc, address, size, user):
        esp = uc.reg_read(UC_X86_REG_ESP)
        n = self.r32(esp + 0x20)
        h = self.r32(self.r32(MANAGER) + 0xc + 4)
        if h in self.handles and n > len(self.files[self.handles[h][0]]):
            self.fixed.append('long' if self.fixes else 'unfixed long')
            if self.fixes:
                uc.reg_write(UC_X86_REG_EIP, NEXT_ENTRY)

    def found(self, uc, address, size, user):
        if uc.reg_read(UC_X86_REG_ECX) & 0xff == 0:
            self.missing += 1

    # the registrants
    def load_call(self, c):
        ctx = c.ecx
        mgr, data = self.r32(ctx), self.r32(ctx + 4)
        user = self.r32(data + 0x10)
        k = self.tables.index(user)
        sub = self.cstr(c.edx)
        buf, n = c.arg(0), c.arg(1)
        body = self.cpu.read(buf, n) if n else b''
        self.log.append(f'load {k} {hx(sub)} {n} {hx(body)}')
        if k in self.stops:
            self.cpu.u32(mgr + 0x30, 1)
        return 0

    def set_chunks(self, k, chunks):
        t = self.tables[k]
        self.cpu.u32(t, len(chunks))
        for i, (sub, data) in enumerate(chunks):
            d = self.alloc(len(data)) if data else 0
            if data:
                self.cpu.write(d, data)
            self.cpu.u32(t + 4 + 12 * i, self.string(sub))
            self.cpu.u32(t + 8 + 12 * i, d)
            self.cpu.u32(t + 12 + 12 * i, len(data))

    def normalize(self, k):
        """The table's flags, comment and file time zeroed (the exe's stack's)."""
        f = self.files.get(k)
        if not f or len(f) < 8:
            return
        version, count = struct.unpack_from('<Ii', f, len(f) - 8)
        start = len(f) - 8 - count * 0x94
        if version != 1 or count < 0 or start < 0:
            return
        for i in range(count):
            e = start + i * 0x94
            f[e + 0x48:e + 0x94] = bytes(0x4c)

    def run(self, plan):
        chunks = [[] for _ in NAMES]
        cpu = self.cpu
        for line in plan:
            w = line.split()
            op = w[0]
            if op == 'I':
                cpu.call(INIT)
            elif op == 'X':
                cpu.call(SHUTDOWN)
            elif op == 'R':
                k, prio = int(w[1]), int(w[2])
                cpu.call(ADD, ecx=self.name_at[k], edx=self.save_fn,
                         stack_args=(self.load_fn, prio & 0xffffffff, self.tables[k]))
            elif op == 'C':
                k = int(w[1])
                chunks[k].append((bytes.fromhex(w[2]), b'' if w[3] == '-' else bytes.fromhex(w[3])))
                self.set_chunks(k, chunks[k])
            elif op == 'Z':
                chunks[int(w[1])] = []
                self.set_chunks(int(w[1]), [])
            elif op == 'P':
                self.stops.add(int(w[1]))
            elif op in 'SL':
                path = bytes.fromhex(w[1]).decode('latin1')
                cpu.call(SAVE if op == 'S' else LOAD, ecx=self.string(path), edx=int(w[2]) & 0xffffffff)
                self.log.append(f'{op} {1 if cpu.uc.reg_read(UC_X86_REG_EAX) & 0xff else 0}')
                if op == 'S':
                    self.normalize(key(path))
            elif op == 'K':
                path = bytes.fromhex(w[1]).decode('latin1')
                cpu.call(KIND, ecx=self.r32(MANAGER), stack_args=(self.string(path),))
                self.log.append(f'K {s32(cpu.uc.reg_read(UC_X86_REG_EAX))}')
            elif op == 'U':
                f = self.files.get(key(bytes.fromhex(w[1]).decode('latin1')))
                if f is not None:
                    del f[int(w[2]):]
            elif op == 'B':
                f = self.files.get(key(bytes.fromhex(w[1]).decode('latin1')))
                if f is not None:
                    pos, data = int(w[2]), bytes.fromhex(w[3])
                    if len(f) < pos + len(data):
                        f.extend(bytes(pos + len(data) - len(f)))
                    f[pos:pos + len(data)] = data
        if self.r32(MANAGER):
            cpu.call(SHUTDOWN)


def table(f):
    if len(f) < 8:
        return None
    version, count = struct.unpack_from('<Ii', f, len(f) - 8)
    start = len(f) - 8 - count * 0x94
    if version != 1 or count < 0 or start < 0:
        return None
    return start, count


def make_plan(rng):
    plan = ['I']
    hexp = [p.encode('latin1').hex() for p in PATHS]
    regs = rng.sample(range(len(NAMES)), rng.randint(1, len(NAMES)))

    def word():
        alpha = 'abcdefghijXYZ0123456789_'
        s = ''.join(rng.choice(alpha) for _ in range(rng.randint(1, 10)))
        r = rng.random()
        if r < 0.05:
            s += ' tail'      # the sub-name ends at a space
        elif r < 0.08:
            s += '/more'      # a "/" inside the sub-name
        return s.encode('latin1').hex()

    def registrations(ks):
        for k in ks:
            plan.append(f'R {k} {rng.choice([0x32, 0x33, 0x34, 1000, 1000, rng.randint(-5, 2000)])}')
            if rng.random() < 0.15:
                plan.append(f'R {k} {rng.randint(0, 2000)}')  # the same name again: refused

    registrations(regs)
    for k in range(len(NAMES)):
        for _ in range(rng.choice([0, 1, 2, 3, 4, 7])):
            n = rng.choice([0, 1, 4, 8, 0x34, rng.randint(0, 200)])
            plan.append(f'C {k} {word()} {hx(bytes(rng.randrange(256) for _ in range(n)))}')
    if rng.random() < 0.3:
        plan.append(f'P {rng.choice(regs)}')
    for p in rng.sample(hexp, rng.randint(1, len(hexp))):
        plan.append(f'S {p} {rng.choice([2, 2, 2, 0, 1, -1, 7])}')
        plan.append(f'K {p}')
        plan.append(f'L {p} {rng.choice([2, -1, 3])}')
        plan.append(f'L {p} -1')
    plan.append(f'L {hexp[0]}00 2')  # no such file
    if rng.random() < 0.6:  # the registrants made again, some missing
        plan += ['X', 'I']
        registrations(rng.sample(regs, rng.randint(0, len(regs))))
        for p in hexp:
            plan.append(f'L {p} -1')
    return plan


def damage(rng, plan, files):
    """Damage lines for the saved files (their layout known from a run)."""
    out = []
    for k, f in files.items():
        p = next((h for h in (x.encode('latin1').hex() for x in PATHS) if key(bytes.fromhex(h).decode()) == k), None)
        t = table(f)
        if p is None or t is None:
            continue
        start, count = t
        for _ in range(rng.randint(1, 3)):
            r = rng.random()
            e = start + 0x94 * rng.randrange(count) if count else start
            if r < 0.15:
                out.append(f'U {p} {rng.randrange(len(f))}')
            elif r < 0.25:
                out.append(f'U {p} {rng.randint(0, 8)}')
            elif r < 0.35:
                out.append(f'B {p} {len(f) - 4} {struct.pack("<i", rng.choice([count + 1, count + 40, 0x7fffffff, -1, -0x40, 0])).hex()}')
            elif r < 0.42:
                out.append(f'B {p} {len(f) - 8} {struct.pack("<I", rng.choice([0, 2, 0xffffffff])).hex()}')
            elif r < 0.55:
                out.append(f'B {p} {e} {struct.pack("<I", rng.choice([len(f), len(f) + 100, 0xfffffff0, rng.randrange(len(f))])).hex()}')
            elif r < 0.7:
                out.append(f'B {p} {e + 4} {struct.pack("<I", rng.choice([0, len(f), len(f) + 1, 0x7fffffff, 0xffffffff, rng.randrange(64)])).hex()}')
            elif r < 0.75:
                # the header's entry (the first): its length or its first word
                h = start
                out.append(rng.choice([f'B {p} {h + 4} {struct.pack("<I", rng.choice([0, 4, 7, 9, 12])).hex()}',
                                       f'B {p} {struct.unpack_from("<I", f, h)[0]} {struct.pack("<I", rng.choice([0, 7, 9])).hex()}']))
            elif r < 0.85:
                name = rng.choice([b'', b'Anim', b'/', b'Anim/', b'/sub', b'Nobody/x', b'zSaveHeader', b'W/a b', b'Weapons/z',
                                   b'anim/c', b'ANIM/d'])
                out.append(f'B {p} {e + 8} {(name + bytes(1)).hex()}')
            else:
                out.append(f'B {p} {rng.randrange(len(f))} {bytes(rng.randrange(256) for _ in range(rng.randint(1, 8))).hex()}')
        out.append(f'K {p}')
        out.append(f'L {p} {rng.choice([2, -1])}')
        out.append(f'L {p} -1')
    return out


def port(plan, tmp):
    buf = ctypes.create_string_buffer(1 << 22)
    n = probe.probe_zsave_run('\n'.join(plan).encode('latin1'), str(tmp).encode(), buf, len(buf))
    files = {}
    for f in Path(tmp).rglob('*'):
        if f.is_file():
            files[key(str(f.relative_to(tmp)))] = f.read_bytes()
    return buf.value.decode('latin1')[:n].splitlines(), files


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    runs = int(args[0]) if args else 40
    first = int(args[1]) if len(args) > 1 else 0
    verbose, fixes = '--verbose' in sys.argv, '--nofix' not in sys.argv
    same, stats = 0, {}
    for seed in range(first, first + runs):
        rng = random.Random(seed)
        plan = make_plan(rng)
        # A first pass for the files' layout, then the damage and the loads.
        pre = Emulated(fixes)
        pre.run(plan)
        tail = damage(rng, plan, {k: bytes(v) for k, v in pre.files.items()})
        if tail:
            plan = plan + ['X', 'I'] + [f'R {k} {rng.randint(0, 2000)}' for k in range(len(NAMES)) if rng.random() < 0.8] + tail
        emu = Emulated(fixes)
        try:
            emu.run(plan)
            want_log, want_files, crashed = emu.log, {k: bytes(v) for k, v in emu.files.items()}, None
        except UcError as e:
            want_log, want_files, crashed = emu.log, None, str(e)
        tmp = Path(tempfile.mkdtemp(prefix='zsave'))
        try:
            got_log, got_files = port(plan, tmp)
        finally:
            shutil.rmtree(tmp)
        for f in emu.fixed:
            stats[f'fix {f}'] = stats.get(f'fix {f}', 0) + 1
        stats['loads'] = stats.get('loads', 0) + sum(1 for x in want_log if x.startswith('load'))
        stats['runs with a stopping load'] = stats.get('runs with a stopping load', 0) + (1 if emu.stops else 0)
        stats['entries without their registrant'] = stats.get('entries without their registrant', 0) + emu.missing
        stats['loads refused'] = stats.get('loads refused', 0) + sum(1 for x in want_log if x == 'L 0')
        if crashed:
            stats['the original crashed'] = stats.get('the original crashed', 0) + 1
            ok = got_log[:len(want_log)] == want_log
            if verbose or not fixes:
                print(f'seed {seed}: the original crashed ({crashed}) after {len(want_log)} log lines, '
                      f'last damage seen: {emu.fixed[-1] if emu.fixed else "none"}; port prefix {"same" if ok else "differs"}')
            same += ok and not fixes
            continue
        ok = got_log == want_log and got_files == want_files
        if not ok:
            print(f'seed {seed}: differs')
            for i, (a, b) in enumerate(zip(want_log + ['(end)'] * 999, got_log + ['(end)'] * 999)):
                if a != b:
                    print(f'  line {i}: original {a!r}\n           port     {b!r}')
                    break
                if a == b == '(end)':
                    break
            if got_files != want_files:
                for k in sorted(set(want_files) | set(got_files)):
                    if want_files.get(k) != got_files.get(k):
                        print(f'  file {k}: original {len(want_files.get(k) or b"")} bytes, port {len(got_files.get(k) or b"")}')
            if verbose:
                print('\n'.join(plan))
        same += ok
    detail = ', '.join(f'{k} x{v}' for k, v in sorted(stats.items()))
    print(f'zsave: {same}/{runs} runs identical ({detail})')
    return 0 if same == runs else 1


if __name__ == '__main__':
    sys.exit(main())
