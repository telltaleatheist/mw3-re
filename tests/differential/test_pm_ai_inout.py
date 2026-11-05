"""Pirate's Moon's mission boundary, its choice of areas (inout_variant and
InOutVariant::match, src/mech3/zainet_inout.cpp on the pm branch; docs/pm-map.md
step 5) against PM's exe 0x4cdb30 and 0x4cdce0 in Unicorn: every game type, the
mission numbers, and child names, the shipped and odd ones.

    cmake --build build && .venv/bin/python tests/differential/test_pm_ai_inout.py
"""
import ctypes
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import pm_floors  # noqa: E402
from original import ROOT, Original  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

probe = ctypes.CDLL(str(next((ROOT / 'build').glob('libpmprobe.*'))))
probe.probe_pm_inout_variant.argtypes = [ctypes.c_int32, ctypes.c_int32, ctypes.c_char_p, ctypes.c_void_p,
                                         ctypes.c_void_p]
VARIANT, MATCH, PILOT = 0x4cdb30, 0x4cdce0, 0x62c40c
SNPRINTF, STRNICMP, STRTOL, STRICMP = 0x5de638, 0x5de57c, 0x5de5c0, 0x5de674
CHILDREN = ['inoutm1', 'inoutm5', 'inoutm10', 'INOUTM2', 'inoutm', 'inoutm1x', 'inoutm0', 'inoutm0x2', 'inoutsa1',
            'inoutsa4', 'inoutsa', 'inoutia', 'INOUTIA', 'inoutia1', 'inoutt', 'inoutmp', 'inout', 'other', 'inoutm11']


def setup():
    cpu = Original(exe='pm')
    cpu.tr = lambda a, size=1: a

    def text(a):
        out = b''
        while cpu.read(a, 1) != b'\0':
            out += cpu.read(a, 1)
            a += 1
        return out

    def snprintf(c):  # (buffer, size, "%s%s", a, b): the format is the only one here
        t = text(c.arg(3)) + text(c.arg(4))
        cpu.write(c.arg(0), t[:c.arg(1)] + (b'\0' if len(t) < c.arg(1) else b''))
        return len(t) if len(t) <= c.arg(1) else 0xffffffff

    def stricmp(c):
        a, b = text(c.arg(0)).lower(), text(c.arg(1)).lower()
        return 0 if a == b else (1 if a > b else 0xffffffff)

    def strnicmp(c):
        n = c.arg(2)
        a, b = text(c.arg(0))[:n].lower(), text(c.arg(1))[:n].lower()
        return 0 if a == b else (1 if a > b else 0xffffffff)

    def strtol(c):  # base 0: C's rules, as the port's std::strtol
        a = c.arg(0)
        t = text(a).decode('latin-1')
        m = re.match(r'[ \t]*[-+]?(0[xX][0-9a-fA-F]+|0[0-7]*|[1-9][0-9]*)', t)
        if not m:
            cpu.write(c.arg(1), struct.pack('<I', a))
            return 0
        cpu.write(c.arg(1), struct.pack('<I', a + m.end()))
        return int(m.group(0).strip(), 0 if not re.match(r'[-+]?0[0-7]+$', m.group(0).strip()) else 8) & 0xffffffff
    cpu.stub_import(SNPRINTF, snprintf)
    cpu.stub_import(STRICMP, stricmp)
    cpu.stub_import(STRNICMP, strnicmp)
    cpu.stub_import(STRTOL, strtol)
    return cpu


class Case:
    """One game type and mission: the variant made once, each child matched on it."""

    def __init__(self, type_, mission):
        cpu = self.cpu = setup()
        w32 = lambda a, v: cpu.write(a, struct.pack('<I', v & 0xffffffff))  # noqa: E731
        P, G, vt = cpu.alloc(0x100), cpu.alloc(0x40), cpu.alloc(0x100)
        self.out, self.s = cpu.alloc(0x40), cpu.alloc(0x40)
        w32(PILOT, P)
        w32(P + 0x6c, G)
        w32(G, vt)
        for off, val in ((0x28, type_), (0x48, mission)):
            code = cpu.code(b'\xc3')
            cpu.stub_function(code, lambda c, val=val: val, 0)
            w32(vt + off, code)
        cpu.call(VARIANT, ecx=self.out)

    def match(self, child):
        cpu, out = self.cpu, self.out
        index, count, game, prefix = struct.unpack('<4i', cpu.read(out, 16))
        numbered, valid = cpu.read(out + 0x10, 2)
        name = bytes(cpu.read(out + 0x12, 9)).split(b'\0')[0] if valid else b''
        cpu.write(self.s, child.encode() + b'\0')
        cpu.call(MATCH, ecx=out, stack_args=(self.s,))
        m = struct.unpack('<i', struct.pack('<I', cpu._uc.reg_read(UC_X86_REG_EAX)))[0]
        return [index, count, game, prefix, numbered, valid, m], name


def main():
    n = 0
    for type_ in (0, 1, 2, 3, 4, 5, 6):
        for mission in (1, 2, 5, 8, 10, 11):
            case = Case(type_, mission)
            for child in CHILDREN:
                # A type without a variant leaves the exe's count unset; the port's is 0.
                e, en = case.match(child)
                if type_ not in (1, 2, 4, 5):
                    e[1] = 0
                o, nm = (ctypes.c_int32 * 7)(), ctypes.create_string_buffer(9)
                probe.probe_pm_inout_variant(type_, mission, child.encode(), o, nm)
                p = list(o)
                if e != p or en != nm.value:
                    print(f'type {type_} mission {mission} child {child}:\n  exe  {e} {en}\n  port {p} {nm.value}')
                    sys.exit(1)
                n += 1
    pm_floors.floors(variants=(n, 798))
    print(f'ok: {n} variants and matches identical')


if __name__ == '__main__':
    main()
