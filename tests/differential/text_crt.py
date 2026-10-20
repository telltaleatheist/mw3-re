"""MSVCRT's text functions the zReader text files and the controls file use,
as the emulator's stand-ins (the exe imports them from MSVCRT.dll). The same
grammar as the port's crt::msvc_strtol / msvc_strtod / msvc_scan_* (C locale)."""
import re
import struct

from unicorn import UC_HOOK_CODE

SPACE = b' \t\n\v\f\r'


def isspace(c):
    return 1 if 0 <= c < 0x80 and bytes([c]) in SPACE else 0


def strtol(s):
    """(value, chars used) of strtol(s, &end, 10)."""
    m = re.match(rb'[ \t\n\v\f\r]*([+-]?)(\d+)', s)
    if not m:
        return 0, 0
    v = int(m.group(2))
    if m.group(1) == b'-':
        v = -v
        v = max(v, -0x80000000)
    else:
        v = min(v, 0x7fffffff)
    return v, m.end()


def strtod(s):
    m = re.match(rb'[ \t\n\v\f\r]*([+-]?(\d+\.?\d*|\.\d+))([eEdD][+-]?\d+)?', s)
    if not m:
        return 0.0, 0
    t = m.group(1) + (b'e' + m.group(3)[1:] if m.group(3) else b'')
    try:
        v = float(t)
    except OverflowError:
        v = float('inf')
    return v, m.end()


def scan_hex(s):
    m = re.match(rb'[ \t\n\v\f\r]*(0[xX])?([0-9a-fA-F]*)', s)
    if not m.group(1) and not m.group(2):
        return 0, None
    v = 0
    for ch in m.group(2):
        v = ((v << 4) | int(chr(ch), 16)) & 0xffffffff
    return 1, v


def scan_dec(s):
    m = re.match(rb'[ \t\n\v\f\r]*([+-]?)(\d+)', s)
    if not m:
        return 0, None
    v = int(m.group(2)) & 0xffffffff
    if m.group(1) == b'-':
        v = (-v) & 0xffffffff
    return 1, v


def install(cpu, files=None, iat=None):
    """The CRT and file imports on an original.Original. `files` maps a handle
    to bytes for ReadFile; malloc is a bump allocator; strtod returns in ST0
    through a little x86 thunk."""
    st = {'pos': {}, 'printf': 0}
    files = files if files is not None else {}

    def cstr(a, limit=0x10000):
        b = cpu.read(a, 64)
        out = bytearray()
        while 0 not in b:
            out += b
            b = cpu.read(a + len(out), 64)
        return bytes(out + b[:b.index(0)])

    def read_file(c):
        h, buf, n, out = c.arg(0), c.arg(1), c.arg(2), c.arg(3)
        data, pos = files[h], st['pos'].get(h, 0)
        chunk = data[pos:pos + n]
        st['pos'][h] = pos + len(chunk)
        c.write(buf, chunk)
        c.u32(out, len(chunk))
        return 1

    def set_pointer(c):
        st['pos'][c.arg(0)] = c.arg(1)
        return c.arg(1)

    def malloc(c):
        a = c.alloc(max(c.arg(0), 1))
        c.write(a, b'\0' * max(c.arg(0), 1))
        return a

    def strdup(c):
        s = cstr(c.arg(0))
        a = c.alloc(len(s) + 1)
        c.write(a, s + b'\0')
        return a

    def strtol_(c):
        p, endp = c.arg(0), c.arg(1)
        v, n = strtol(cstr(p))
        if endp:
            c.u32(endp, p + n)
        return v & 0xffffffff

    def printf(c):
        st['printf'] += 1
        return 0

    cpu.stub_import(0x5bb0c4, read_file, pop_bytes=20)    # ReadFile
    cpu.stub_import(0x5bb0d8, set_pointer, pop_bytes=16)  # SetFilePointer
    cpu.stub_import(0x5bb664, malloc)                     # malloc
    cpu.stub_import(0x5bb65c, lambda c: 0)                # free
    cpu.stub_import(0x5bb670, strdup)                     # _strdup
    cpu.stub_import(0x5bb5b4, strtol_)                    # strtol
    cpu.stub_import(0x5bb564, lambda c: isspace(struct.unpack('<i', struct.pack('<I', c.arg(0)))[0]))
    cpu.stub_import(0x5bb5f0, printf)                     # printf
    # strtod: the hook computes the double before the thunk's fld runs.
    slot = cpu.alloc(8)
    thunk = cpu.code(b'\xdd\x05' + struct.pack('<I', slot) + b'\xc3')  # fld qword [slot]; ret
    cpu.u32(0x5bb560, thunk)

    def strtod_hook(uc, address, size, user):
        p, endp = cpu.arg(0), cpu.arg(1)
        v, n = strtod(cstr(p))
        cpu.write(slot, struct.pack('<d', v))
        if endp:
            cpu.u32(endp, p + n)
    cpu.uc.hook_add(UC_HOOK_CODE, strtod_hook, begin=thunk, end=thunk)
    st['cstr'] = cstr
    return st
