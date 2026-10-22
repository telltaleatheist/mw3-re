"""The mission boundary's reader against the original x86 (docs/ai.md
section 11): the listener's load 0x4c3da0 -> 0x4c3930 (the variant 0x4c36c0
and its match 0x4c3850, the walk 0x4c3e50 / 0x4c3ed0 through the matrix
stack, the models 0x4c3380 with the corners on the ground 0x4c4240, the
polygons 0x4c32c0 / 0x4c1ab0 and their sides 0x4c11f0, the unlink and the
subtree's deletion 0x4f9050), then the selection 0x4c3b80, on every
mission world read by the original's 0x4ff520 and by the port's loaders.

Cases per world: each game type (campaign with the world's operation and
each of its missions, Instant Action, multiplayer, training, an unknown
one), no pilot, the ground off, and `inout` moved (turned and shifted, its
children too, some off the tree-valid path) to run the vertices' transform
and the walk's matrices; the shipped nodes are all at the identity. (An
operation outside 1..4 makes no areas and the exe writes the children past
them; the port's FIX skips them.) The areas (each polygon's kind, flags,
corners, offsets, side lengths, box, perimeter and name, the area's other
fields), the current area, whether `inout` survives and the error reports
must match bit for bit at 24-bit precision.

    cmake --build build && .venv/bin/python tests/differential/test_inout.py [worlds...]
"""
import ctypes
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_di import World, probe, ZBD, WORLDS  # noqa: E402

probe.inout_run.argtypes = [ctypes.c_int] * 5 + [ctypes.c_void_p, ctypes.c_int]
probe.inout_move.argtypes = [ctypes.c_void_p]
# `inout` turned 30 degrees about y and shifted, its children 20 more and shifted
# again: the shipped nodes are all at the identity, which leaves 0x4c3520's
# transform unused.
MOVE = struct.pack('<24f', 0.8660254, 0.0, -0.5, 0.0, 1.0, 0.0, 0.5, 0.0, 0.8660254, 123.25, 4.5, -87.75,
                   0.9396926, 0.0, 0.3420201, 0.0, 1.0, 0.0, -0.3420201, 0.0, 0.9396926, -6.5, 0.0, 11.0)
LOAD, SELECT, LISTENER = 0x4c3da0, 0x4c3b80, 0x60b338
AREAS, CURRENT, PILOT, WORLD_OBJ = 0x60b33c, 0x60b344, 0x60861c, 0x608c84
NEW, DELETE, FIND = 0x5abd26, 0x5abd1a, 0x4f7fe0
FLT_MAX, TEMPLATE, CISQRT = 0x5bb4fc, 0x4c19f0, 0x5bb5b8  # std::numeric_limits<float>::max
IAT = {'strdup': 0x5bb670, 'strncmp': 0x5bb654, 'snprintf': 0x5bb638, 'strnicmp': 0x5bb604,
       'strtol': 0x5bb5b4, 'stricmp': 0x5bb580, 'free': 0x5bb65c}


class Inout(World):
    def __init__(self, name):
        super().__init__(name)
        cpu = self.cpu
        cpu.stub_function(NEW, lambda c: self.alloc(c.arg(0)))
        cpu.stub_function(DELETE, lambda c: 0)
        cpu.stub_import(IAT['free'], lambda c: 0)
        cpu.stub_import(IAT['strdup'], lambda c: self.string(self.cstr(c.arg(0))))
        cpu.stub_import(IAT['strncmp'], lambda c: self.cmp(self.cstr(c.arg(0))[:c.arg(2)],
                                                            self.cstr(c.arg(1))[:c.arg(2)]))
        cpu.stub_import(IAT['stricmp'], lambda c: self.cmp(self.cstr(c.arg(0)).lower(), self.cstr(c.arg(1)).lower()))
        cpu.stub_import(IAT['strnicmp'], lambda c: self.cmp(self.cstr(c.arg(0))[:c.arg(2)].lower(),
                                                             self.cstr(c.arg(1))[:c.arg(2)].lower()))
        cpu.stub_import(IAT['snprintf'], self.snprintf)
        cpu.stub_import(IAT['strtol'], self.strtol)
        big = self.alloc(4)
        cpu.write(big, struct.pack('<f', 3.4028234663852886e38))
        cpu.import_code(FLT_MAX, b'\xd9\x05' + struct.pack('<I', big) + b'\xc3')  # fld [big]
        cpu.import_code(CISQRT, b'\xd9\xfa\xc3')  # _CIsqrt: fsqrt on st0
        cpu.call(TEMPLATE)  # the start-up's 0x4c19f0: the query template's FLT_MAX
        # The pilot ([0x60861c]) and its game (+0x6c) with vt+0x2c / +0x48 / +0x4c.
        self.game = self.alloc(0x10)
        self.vt = self.alloc(0x60)
        cpu.u32(self.game, self.vt)
        self.pilot = self.alloc(0x70)
        cpu.u32(self.pilot + 0x6c, self.game)
        self.wobj = self.alloc(8)
        cpu.u32(self.wobj + 4, self.world)

    def cstr(self, a):
        out = b''
        while True:
            b = self.cpu.read(a + len(out), 64)
            i = b.find(b'\0')
            if i >= 0:
                return out + b[:i]
            out += b

    def string(self, s):
        p = self.alloc(len(s) + 1)
        self.cpu.write(p, s + b'\0')
        return p

    @staticmethod
    def cmp(a, b):
        return (a > b) - (a < b) & 0xffffffff

    def snprintf(self, c):
        assert self.cstr(c.arg(2)) == b'%s%s'
        s = (self.cstr(c.arg(3)) + self.cstr(c.arg(4)))
        n = c.arg(1)
        self.cpu.write(c.arg(0), s[:n] + (b'\0' if len(s) < n else b''))
        return len(s) if len(s) <= n else 0xffffffff

    def strtol(self, c):
        s = self.cstr(c.arg(0))
        assert c.arg(2) == 0
        i = 0
        while i < len(s) and s[i:i + 1].isdigit():
            i += 1
        if c.arg(1):
            self.cpu.u32(c.arg(1), c.arg(0) + i if i else c.arg(0))
        return int(s[:i]) if i else 0

    def polygons(self, a):
        return [self.u32(pp) for pp in range(self.u32(a + 8), self.u32(a + 0xc), 4)]

    def vertex_id(self, a, v):
        if not v:
            return 0
        for i, p in enumerate(self.polygons(a)):
            vb, ve = self.u32(p + 8), self.u32(p + 0xc)
            if vb <= v < ve:
                return ((i << 16) | (v - vb) // 0x1c) + 1
        return 0xffffffff

    def query_state(self, a, frm, to):
        """The area's +0x14 as tests/probe_inout.cpp's put_query writes it."""
        q = a + 0x14
        polys = self.polygons(a)
        out = bytearray()

        def u32(v):
            out.extend(struct.pack('<I', v & 0xffffffff))
        for k in (0, 4):
            p = self.u32(q + k)
            u32(0 if not p else 1 if p == frm else 2 if p == to else -1)
        p = self.u32(q + 8)
        u32(0 if not p else polys.index(p) + 1 if p in polys else -1)
        for k in (0xc, 0x10, 0x14, 0x18):
            u32(self.vertex_id(a, self.u32(q + k)))
        out.extend(self.cpu.read(q + 0x1c, 0x30))
        return bytes(out)

    def method(self, value):
        return self.cpu.code(b'\xb8' + struct.pack('<I', value & 0xffffffff) + b'\xc3')

    def move(self):
        cpu = self.cpu
        name = self.string(b'inout')
        cpu.call(FIND, ecx=6, edx=name)
        root = cpu.uc.reg_read(19)
        if not root or self.u32(root + 0x34) != 5:
            return 0
        cpu.write(self.u32(root + 0x38) + 0x30, MOVE[:48])

        def stale(n):
            if self.u32(n + 0x34) == 5:
                d = self.u32(n + 0x38)
                cpu.u32(d, self.u32(d) | 0x20)
            for i in range(self.u32(n + 0x5c)):
                stale(self.u32(self.u32(n + 0x60) + 4 * i))
        stale(root)
        d = self.u32(root + 0x38)
        cpu.u32(d, self.u32(d) & ~8)  # a matrix of its own
        for i in range(self.u32(root + 0x5c)):  # each child shifted too
            c = self.u32(self.u32(root + 0x60) + 4 * i)
            if self.u32(c + 0x34) == 5:
                cd = self.u32(c + 0x38)
                cpu.write(cd + 0x30, MOVE[48:])
                cpu.u32(cd, self.u32(cd) & ~8)
                if i & 1:  # every other one off the tree-valid path
                    cpu.u32(c + 0x24, self.u32(c + 0x24) & ~0x80000)
        return 1

    def run(self, have, game, op, mission, ground, moved=0):
        cpu = self.cpu
        if moved and not self.move():
            raise SystemExit('no object3d inout to move')
        cpu.u32(self.vt + 0x2c, self.method(game))
        cpu.u32(self.vt + 0x48, self.method(op))
        cpu.u32(self.vt + 0x4c, self.method(mission))
        cpu.u32(PILOT, self.pilot if have else 0)
        cpu.u32(WORLD_OBJ, self.wobj if ground else 0)
        cpu.u32(AREAS, 0)
        cpu.u32(CURRENT, 0)
        self.reports = []
        cpu.call(LOAD, ecx=LISTENER, count=2_000_000_000)
        cpu.call(SELECT, count=1_000_000)
        out = bytearray()

        def u32(v):
            out.extend(struct.pack('<I', v & 0xffffffff))
        vec = self.u32(AREAS)
        cur = self.u32(CURRENT)
        if not vec:
            u32(-1)
        else:
            b, e = self.u32(vec + 4), self.u32(vec + 8)
            u32((e - b) // 0x88)
        current = -1
        if vec:
            for a in range(self.u32(vec + 4), self.u32(vec + 8), 0x88):
                if a == cur:
                    current = (a - self.u32(vec + 4)) // 0x88
                pb, pe = self.u32(a + 8), self.u32(a + 0xc)
                u32((pe - pb) // 4)
                for pp in range(pb, pe, 4):
                    p = self.u32(pp)
                    u32(self.u32(p + 0x38))
                    out.extend(cpu.read(p + 0x34, 2) + b'\0\0')
                    out.extend(cpu.read(p + 0x14, 4))
                    out.extend(cpu.read(p + 0x18, 4))
                    out.extend(cpu.read(p + 0x1c, 24))
                    nm = self.cstr(self.u32(p + 0x3c)) if self.u32(p + 0x3c) else b''
                    u32(len(nm))
                    out.extend(nm)
                    vb, ve = self.u32(p + 8), self.u32(p + 0xc)
                    u32((ve - vb) // 0x1c)
                    out.extend(cpu.read(vb, ve - vb))
                out.extend(self.query_state(a, 0, 0))
                out.extend(cpu.read(a + 0x60, 24))
        u32(current)
        name = self.string(b'inout')
        cpu.call(FIND, ecx=6, edx=name)
        u32(1 if cpu.uc.reg_read(19) else 0)
        u32(len(self.reports))
        return bytes(out)


# Each world's operation (c4 and c4b share op 4: m1..m3 and m4..m6). Another
# operation with fewer missions than the world has boundaries makes the exe
# write the extra ones past its vector (the FIX), so the cases keep to it.
OPERATION = {'c1': 1, 'c2': 2, 'c3': 3, 'c4': 4, 'c4b': 4, 't1': 1}


def cases(w):
    op = OPERATION[w]
    last = 4 if op < 3 else 6
    out = [(1, 1, op, m, 1) for m in range(1, last + 1)]
    out += [(1, 2, 0, 0, 1), (1, 3, 0, 0, 1), (1, 4, 0, 0, 1), (1, 5, 0, 0, 1), (0, 1, op, 1, 1),
            (1, 1, op, 1, 0), (1, 1, op, last, 1, 1), (1, 2, 0, 0, 1, 1), (1, 4, 0, 0, 1, 1)]
    return out


def describe(blob):
    n, = struct.unpack_from('<i', blob, 0)
    if n < 0:
        return 'no areas'
    o, counts = 4, []
    for _ in range(n):
        k, = struct.unpack_from('<I', blob, o)
        o += 4
        counts.append(k)
        for _ in range(k):
            o += 4 + 4 + 4 + 4 + 24
            ln, = struct.unpack_from('<I', blob, o)
            o += 4 + ln
            nv, = struct.unpack_from('<I', blob, o)
            o += 4 + nv * 0x1c
        o += 0x4c + 24
    return f'{n} areas, polygons {counts}'


def main():
    worlds = sys.argv[1:] or WORLDS
    buf = ctypes.create_string_buffer(8 << 20)
    total = 0
    for w in worlds:
        path = str(ZBD / w / 'gamez.zbd').encode()
        for case in cases(w):
            emu = Inout(w)
            if probe.sw_load(path) != 0:
                raise SystemExit(f'{w}: the port could not read the world')
            want = emu.run(*case)
            if case[5:] and not probe.inout_move(MOVE):
                raise SystemExit(f'{w}: the port has no object3d inout')
            n = probe.inout_run(*case[:5], buf, len(buf))
            got = buf.raw[:n]
            if got != want:
                at = next((i for i in range(min(len(got), len(want))) if got[i] != want[i]), min(len(got), len(want)))
                raise SystemExit(f'{w} case {case}: {len(want)} vs {n} bytes, first difference at {at}\n'
                                 f'  original: {describe(want)}\n  port: {describe(got) if n >= 4 else n}')
            total += 1
            if case == (1, 1, OPERATION[w], 1, 1) or case[1] == 2 and not case[5:]:
                print(f'{w} {case}: {describe(want)}')
    print(f'ok: {total} mission loads identical across {len(worlds)} worlds')


if __name__ == '__main__':
    main()
