"""The emulator side shared by the AI tests (docs/ai.md): the original's AI
reader files run in Unicorn with the C runtime and every unported call stood
in for, plus the byte stream tests/probe_ai.cpp writes for the port, built
here from the original's memory.

Reader trees come from the original's own zReader (0x557180) reading the
file's bytes through a stubbed ReadFile, as test_weapons_zrd.py does.
"""
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools' / 'analysis'))
from original import FTOL, IAT_FTOL, ROOT, Original  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP  # noqa: E402
import zrd  # noqa: E402

ZBD = ROOT / 'game' / 'mw3-1.2' / 'zbd'
IAT = {'readfile': 0x5bb0c4, 'malloc': 0x5bb664, 'free': 0x5bb65c, 'strdup': 0x5bb670, 'strtol': 0x5bb5b4,
       'strrchr': 0x5bb5a8, 'strncpy': 0x5bb648, 'sprintf': 0x5bb658, 'strncmp': 0x5bb654, 'snprintf': 0x5bb638,
       'lockit': 0x5bb4f8, 'unlockit': 0x5bb4f4, 'calloc': 0x5bb60c, 'realloc': 0x5bb610, 'stricmp': 0x5bb580}
NEW, DELETE, LOAD, READ_NODE, FIND_LOOSE, LOCALIZE, SOUND, ZERR, ATEXIT, REGION = (
    0x5abd26, 0x5abd1a, 0x556740, 0x557180, 0x556240, 0x575fb0, 0x56f6f0, 0x49bfd0, 0x5ac140, 0x4c5c00)
AIV_PARSE = 0x4df730
VLIST_DEFAULTS = struct.pack('<7I', 0x3e800000, 0x3dcccccd, 0x3f4ccccd, 0, 0x45610000, 0, 0)
HANDLE = 0x1234
BIG = 0x40000000
# The AI globals the static initializers set up (zeroed before each run).
GLOBALS = [(0x60b500, 0x10), (0x60b5a0, 0x30), (0x60b5e0, 0x30)]


def fnv(b):
    h = 2166136261
    for c in b:
        h = ((h ^ c) * 16777619) & 0xffffffff
    return h


def s32(v):
    v &= 0xffffffff
    return v - (1 << 32) if v & 0x80000000 else v


class Out:
    def __init__(self):
        self.b = bytearray()

    def u(self, v):
        self.b += struct.pack('<I', v & 0xffffffff)

    def s(self, b):
        if b is None:
            self.u(0xffffffff)
        else:
            self.u(len(b))
            self.b += b


def printf(fmt, args):
    """The few printf forms the AI code uses: %s %d %02d %%."""
    out, i, a = bytearray(), 0, iter(args)
    while i < len(fmt):
        c = fmt[i]
        if c != 0x25:
            out.append(c)
            i += 1
            continue
        j = i + 1
        while fmt[j] in b'0123456789.-':
            j += 1
        spec, conv = fmt[i + 1:j].decode(), chr(fmt[j])
        if conv == '%':
            out += b'%'
        elif conv == 's':
            out += next(a)
        elif conv == 'd':
            out += (('%' + spec + 'd') % s32(next(a))).encode()
        else:
            raise SystemExit(f'printf: %{spec}{conv}')
        i = j + 1
    return bytes(out)


class Emulated:
    def __init__(self):
        cpu = self.cpu = Original()
        cpu.import_code(IAT_FTOL, FTOL)
        # Load-time code: the x87 at 53 bits, as the CRT start-up sets it
        # (CLAUDE.md); MW3_X87=24 runs it at the in-play 24 instead.
        if os.environ.get('MW3_X87') != '24':
            cpu.fpu_control(0x027f)
        cpu.uc.mem_map(BIG, 0x8000000)
        self.next = BIG
        cpu.stub_import(IAT['readfile'], self.read_file, pop_bytes=20)
        cpu.stub_import(IAT['malloc'], lambda c: self.alloc(c.arg(0)))
        cpu.stub_import(IAT['calloc'], lambda c: self.calloc(c.arg(0) * c.arg(1)))
        cpu.stub_import(IAT['free'], lambda c: 0)
        cpu.stub_import(IAT['strdup'], lambda c: self.new_string(self.cstr(c.arg(0))))
        cpu.stub_import(IAT['strtol'], self.strtol)
        cpu.stub_import(IAT['strrchr'], self.strrchr)
        cpu.stub_import(IAT['strncpy'], self.strncpy)
        cpu.stub_import(IAT['sprintf'], self.sprintf)
        cpu.stub_import(IAT['snprintf'], self.snprintf)
        cpu.stub_import(IAT['strncmp'], self.strncmp)
        cpu.stub_import(IAT['stricmp'], self.stricmp)
        cpu.stub_import(IAT['lockit'], lambda c: c.ecx)
        cpu.stub_import(IAT['unlockit'], lambda c: 0)
        stubs = {
            NEW: (lambda c: self.alloc(c.arg(0)), 0),
            DELETE: (lambda c: 0, 0),
            LOAD: (self.load, 4),
            FIND_LOOSE: (lambda c: self.log(2, fnv(self.cstr(c.ecx))), 0),
            LOCALIZE: (self.localize, 0),
            SOUND: (self.sound, 0),
            ZERR: (self.zerr, 0),
            ATEXIT: (lambda c: 0, 0),
            REGION: (self.region, 0),
        }
        for addr, (fn, pop) in stubs.items():
            cpu.stub_function(addr, fn, pop)
        self.trees = {}
        self.reached = set()
        # The aiv reader's old-tag scratch is uninitialised stack in the exe;
        # the port takes zeros and the order defaults (FIX (bug)): pin them.
        cpu.uc.hook_add(UC_HOOK_CODE, self.pin_aiv_stack, begin=AIV_PARSE, end=AIV_PARSE)
        self.base_heap = cpu.heap_next

    def cover(self, addrs):
        """Records which of these instructions the original runs."""
        def hit(uc, address, size, user):
            self.reached.add(address)
        for a in addrs:
            self.cpu.uc.hook_add(UC_HOOK_CODE, hit, begin=a, end=a)

    # --- C runtime -------------------------------------------------------------
    def alloc(self, n):
        a = self.next
        self.next += (max(n, 1) + 0xf) & ~0xf
        self.cpu.write(a, bytes((max(n, 1) + 0xf) & ~0xf))
        return a

    def pin_aiv_stack(self, uc, address, size, user):
        from unicorn.x86_const import UC_X86_REG_ESP
        base = uc.reg_read(UC_X86_REG_ESP) - 0x88
        uc.mem_write(base + 0x10, bytes(0x30))
        uc.mem_write(base + 0x40, VLIST_DEFAULTS)

    def calloc(self, n):
        a = self.alloc(n)
        self.cpu.write(a, bytes(n))
        return a

    def new_string(self, b):
        p = self.alloc(len(b) + 1)
        self.cpu.write(p, b + b'\0')
        return p

    def cstr(self, p):
        if p == 0:
            return None
        out = bytearray()
        while True:
            b = self.cpu.read(p + len(out), 64)
            i = b.find(0)
            if i >= 0:
                return bytes(out + b[:i])
            out += b

    def strtol(self, c):
        p, end, base = c.arg(0), c.arg(1), c.arg(2)
        assert base == 10
        s = self.cstr(p)
        i = 0
        while i < len(s) and s[i] in b' \t\n\r\f\v':
            i += 1
        j = i + (1 if i < len(s) and s[i] in b'+-' else 0)
        k = j
        while k < len(s) and 0x30 <= s[k] <= 0x39:
            k += 1
        if k == j:
            v, k = 0, 0
        else:
            v = int(s[i:k])
            v = max(-0x80000000, min(0x7fffffff, v))
        if end:
            self.cpu.u32(end, p + k)
        return v & 0xffffffff

    def strrchr(self, c):
        p, ch = c.arg(0), c.arg(1) & 0xff
        s = self.cstr(p)
        i = s.rfind(bytes([ch])) if ch else len(s)
        return 0 if i < 0 else p + i

    def strncpy(self, c):
        d, s, n = c.arg(0), self.cstr(c.arg(1)), c.arg(2)
        self.cpu.write(d, (s[:n] + bytes(max(0, n - len(s))))[:n])
        return d

    def varargs(self, c, first):
        return [c.arg(k) for k in range(first, first + 8)]

    def sprintf(self, c):
        d, fmt = c.arg(0), self.cstr(c.arg(1))
        out = self.format(fmt, self.varargs(c, 2))
        self.cpu.write(d, out + b'\0')
        return len(out)

    def snprintf(self, c):
        d, n, fmt = c.arg(0), c.arg(1), self.cstr(c.arg(2))
        out = self.format(fmt, self.varargs(c, 3))
        self.cpu.write(d, (out + b'\0')[:n])
        return len(out) if len(out) < n else -1

    def format(self, fmt, raw):
        # Turn each %s argument into its string, in order.
        args, k, i = [], 0, 0
        while i < len(fmt):
            if fmt[i] == 0x25:
                j = i + 1
                while fmt[j] in b'0123456789.-':
                    j += 1
                if fmt[j] != 0x25:
                    args.append(self.cstr(raw[k]) if fmt[j] == 0x73 else raw[k])
                    k += 1
                i = j + 1
            else:
                i += 1
        return printf(fmt, args)

    def strncmp(self, c):
        a, b, n = self.cstr(c.arg(0))[:c.arg(2)], self.cstr(c.arg(1))[:c.arg(2)], c.arg(2)
        return ((a > b) - (a < b)) & 0xffffffff

    def stricmp(self, c):
        a, b = self.cstr(c.arg(0)).lower(), self.cstr(c.arg(1)).lower()
        return ((a > b) - (a < b)) & 0xffffffff

    def read_file(self, cpu):
        h, buf, n, out = cpu.arg(0), cpu.arg(1), cpu.arg(2), cpu.arg(3)
        assert h == HANDLE
        chunk = self.data[self.pos:self.pos + n]
        self.pos += len(chunk)
        cpu.write(buf, chunk)
        cpu.u32(out, len(chunk))
        return 1

    # --- stubs ----------------------------------------------------------------
    def log(self, *e):
        self.events.append(tuple(v & 0xffffffff for v in e))
        return 0

    def localize(self, c):
        s = self.cstr(c.ecx)
        self.log(3, fnv(s))
        return self.new_string(b'L:' + s)

    def zerr(self, c):
        # 0x49bfd0 is also every empty virtual slot: the aiv file's parse
        # (called from the load at 0x4decad) is one.
        if self.u32(c.uc.reg_read(UC_X86_REG_ESP)) == 0x4decb0:
            return 0
        return self.log(1, c.arg(0), c.arg(2))

    def region(self, c):
        s = self.cstr(c.ecx)
        self.log(6, fnv(s))
        return 0x6000000 | (fnv(s) & 0xffffff)

    def sound(self, c):
        s = self.cstr(c.ecx)
        self.log(4, fnv(s))
        return 0x5000000 | (fnv(s) & 0xffffff)

    def load(self, c):
        name = self.cstr(c.ecx)
        assert c.edx == 0 and c.arg(0) == 0
        trees = self.trees.get(name.decode("latin-1").lower())
        if not trees:
            return 0
        return trees.pop(0)

    def build(self, data):
        """A reader tree from the bytes, by the original's reader (before a run:
        Unicorn cannot run code from inside a stub)."""
        self.data, self.pos = data, 0
        root = self.alloc(8)
        self.cpu.call(READ_NODE, ecx=HANDLE, edx=root)
        return root

    # --- memory ----------------------------------------------------------------
    def u32(self, p):
        return struct.unpack('<I', self.cpu.read(p, 4))[0]

    def u8(self, p):
        return self.cpu.read(p, 1)[0]

    def eax(self):
        return self.cpu.uc.reg_read(UC_X86_REG_EAX)

    def reset(self, files):
        """A fresh heap and fresh AI globals, the static initializers run again."""
        cpu = self.cpu
        self.next, self.events = BIG, []
        cpu.heap_next = self.base_heap
        # Three trees a file and archive: a run loads one at most that often.
        sets = files if isinstance(files, list) else [files]
        self.tree_sets = [{k.lower(): [self.build(v) for _ in range(3)] for k, v in fs.items()} for fs in sets]
        self.trees = self.tree_sets[0]
        for a, n in GLOBALS:
            cpu.write(a, bytes(n))
        cpu.call(0x4d3d70)          # the valves' list (0x0060b500)
        cpu.call(0x4dc150)          # the net files' map (0x0060b5a8)
        cpu.call(0x4df620)          # aiv.zrd's file object (0x0060b5e0)

    # --- what the port's probe writes ------------------------------------------
    def valves(self):
        head = self.u32(0x60b504)
        out, p = [], self.u32(head)
        while p != head:
            out.append(self.u32(p + 8))
            p = self.u32(p)
        return out

    def valve_token(self, v, vlist):
        return 0 if v == 0 else vlist.index(v) + 1 if v in vlist else 0xdead

    def put_record(self, o, r):
        u, u8 = self.u32, self.u8
        vl = self.valves()
        for off in range(0x04, 0x34, 4):
            o.u(u(r + off))
        o.u(self.valve_token(u(r + 0x34), vl)), o.u(u8(r + 0x38)), o.u(u(r + 0x3c))
        for off in range(0x40, 0x68, 4):
            o.u(u(r + off))
        o.u(u(r + 0x68)), o.u(u(r + 0x6c))
        o.u(u8(r + 0x70) | u8(r + 0x71) << 8 | u8(r + 0x72) << 16), o.u(u(r + 0x74) & 0xffff)
        o.u(self.valve_token(u(r + 0x78), vl)), o.u(self.valve_token(u(r + 0x7c), vl))
        o.u(u8(r + 0x80)), o.u(u(r + 0x84))
        o.u(u8(r + 0x88) | u8(r + 0x89) << 8), o.u(u(r + 0x8c)), o.u(self.valve_token(u(r + 0x90), vl))
        o.u(u(r + 0x94))
        o.u(u8(r + 0x98) | u8(r + 0x99) << 8)
        o.s(self.cstr(u(r + 0x9c))), o.s(self.cstr(u(r + 0xa0))), o.s(self.cstr(u(r + 0xa8)))
        links = u(r + 0xa4)
        b, e = u(links + 4), u(links + 8)
        o.u((e - b) // 4 if b else 0)
        for p in range(b, e, 4):
            o.u(u(p))
        vu = u(r + 0xac)
        if vu == 0:
            o.u(0xffffffff)
        else:
            b, e = u(vu + 4), u(vu + 8)
            o.u((e - b) // 8 if b else 0)
            for p in range(b, e, 8):
                o.u(self.valve_token(u(p), vl)), o.u(u(p + 4))
        va = u(r + 0xb0)
        if va == 0:
            o.u(0xffffffff)
        else:
            o.u(self.valve_token(u(va), vl)), o.u(u(va + 4))

    def put_node(self, o, r):
        if r == 0:
            o.u(0)
        else:
            o.u(1)
            self.put_record(o, r)

    def put_file(self, o, ref, named):
        cpu, u = self.cpu, self.u32
        f = u(ref)
        o.u(1 if f else 0)
        if not f:
            return
        h = f + 0xf4
        for off in (0, 4, 8, 0xc, 0x10, 0x14):
            o.u(u(h + off))
        o.s(self.cstr(u(h + 0x18))), o.u(u(h + 0x1c))
        o.u(u(f + 0x1c)), o.u(u(f + 0x10)), o.u(1 if u(f + 0xc) else 0)
        cpu.call(0x4dc670, ecx=ref)
        n = s32(self.eax())
        o.u(n)
        slot = self.alloc(4)
        for i in range(-1, n + 1):
            cpu.u32(slot, 1)
            cpu.call(0x4dc640, ecx=ref, stack_args=(i & 0xffffffff, slot))
            r = self.eax()
            k = u(slot)
            o.s(b'<unset>' if k == 1 else self.cstr(k))
            self.put_node(o, r)
        for key in named:
            p = self.new_string(key)
            cpu.call(0x4de830, ecx=f, stack_args=(p,))
            self.put_node(o, self.eax())

    def net_files(self):
        """The map at 0x0060b5a8 in order: (name, file)."""
        u = self.u32
        nil, head = u(0x60b5c4), u(0x60b5ac)
        out = []

        def walk(n):
            if n == nil or n == 0:
                return
            walk(u(n))
            out.append((self.cstr(u(n + 0xc)), u(n + 0x10)))
            walk(u(n + 8))
        walk(u(head + 4))
        return out

    def run_nets(self, files, steps, named):
        """steps: (net, name or None, op) as probe_ainet_run takes them."""
        cpu = self.cpu
        self.reset(files)
        o, refs = Out(), []
        for net, name, op in steps:
            if op == 0:
                ref = self.alloc(4)
                p = self.new_string(name.encode()) if name is not None else 0
                cpu.call(0x4da570, ecx=ref, stack_args=(net & 0xffffffff, p))
                refs.append(ref)
                self.put_file(o, ref, named)
            elif op == 1:
                if refs:
                    cpu.call(0x4da8e0, ecx=refs.pop())
                o.u(0x77777777)
            elif op == 3:
                self.trees = self.tree_sets[net]
            elif refs:
                self.put_file(o, refs[-1], named)
        for ref in refs:
            cpu.call(0x4da8e0, ecx=ref)
        vl = self.valves()
        o.u(len(vl))
        for v in vl:
            o.s(self.cstr(self.u32(v)))
        nf = self.net_files()
        o.u(len(nf))
        for name, f in nf:
            o.s(name), o.u(self.u32(f + 0x10)), o.u(1 if self.u32(f + 0xc) else 0)
        return bytes(o.b), self.events


def archive(files):
    """A version-1 archive of {name: bytes} (zreader.md)."""
    body, table = bytearray(), bytearray()
    for name, data in files.items():
        table += struct.pack('<II', len(body), len(data)) + name.encode().ljust(64, b'\0') + bytes(0x94 - 72)
        body += data
    return bytes(body + table + struct.pack('<II', 1, len(files)))


def ser(v):
    """A reader tree to the file's bytes (zreader.md)."""
    if isinstance(v, list):
        return struct.pack('<iI', 4, len(v) + 1) + b''.join(ser(x) for x in v)
    if isinstance(v, (bytes, str)):
        e = v.encode('latin-1') if isinstance(v, str) else v
        return struct.pack('<iI', 3, len(e)) + e
    if isinstance(v, float):
        return struct.pack('<if', 2, v)
    return struct.pack('<ii', 1, v)


def mission_archives():
    """Every mission's reader archive but reader.zbd: (path, {name: bytes})."""
    out = []
    for p in sorted(ZBD.glob('*/reader*.zbd')):
        if p.name == 'reader.zbd':
            continue
        data = p.read_bytes()
        out.append((p, {n: data[s:s + k] for n, s, k in zrd.entries(data)}))
    return out


# --- aiv.zrd -------------------------------------------------------------------
class AivRun:
    """What probe_aiv_run writes, from the original's memory."""
    AIV = 0x60b5e0

    def __init__(self, emu):
        self.e = emu

    def put_record(self, o, r):
        e, u = self.e, self.e.u32
        if r == 0:
            o.u(0)
            return
        o.u(1)
        for off in range(0x04, 0xd0, 4):
            o.u(u(r + off))
        o.u(u(r + 0xdc)), o.u(u(r + 0xe0))
        o.s(e.cstr(u(r + 0xd4))), o.s(e.cstr(u(r + 0xd8)))
        owner = u(r + 0xd0)
        if owner == 0:
            o.u(0xffffffff)
            return
        head = u(owner + 4)
        vl = e.valves()
        items, p = [], u(head)
        while p != head:
            items.append(p + 8)
            p = u(p)
        o.u(len(items))
        for it in items:
            for off in range(0, 0x1c, 4):
                o.u(u(it + off))
            o.u(e.valve_token(u(it + 0x1c), vl))

    def put_file(self, o):
        u, cpu = self.e.u32, self.e.cpu
        o.u(u(self.AIV + 0x10)), o.u(1 if u(self.AIV + 0xc) else 0), o.u(u(self.AIV + 0x1c))
        cpu.call(0x4decf0, ecx=self.AIV)
        o.u(self.e.eax())

    def run(self, files, names):
        e, cpu = self.e, self.e.cpu
        e.reset(files)
        o = Out()
        rec = e.alloc(0xe4)
        cpu.call(0x4e0490, ecx=rec)
        self.put_file(o)
        cpu.call(0x4decf0, ecx=self.AIV)
        n = s32(e.eax())
        slot = e.alloc(4)
        for i in range(-1, n + 1):
            cpu.u32(slot, 0)
            cpu.call(0x4e0600, ecx=rec, stack_args=(i & 0xffffffff, slot))
            r = e.eax()
            o.s(e.cstr(e.u32(slot)))
            self.put_record(o, r)
        for name in names:
            p = e.new_string(name)
            cpu.call(0x4e0680, ecx=rec, stack_args=(p,))
            self.put_record(o, e.eax())
        cpu.call(0x4e04f0, ecx=rec)
        self.put_file(o)
        vl = e.valves()
        o.u(len(vl))
        for v in vl:
            o.s(e.cstr(e.u32(v)))
        return bytes(o.b), e.events
