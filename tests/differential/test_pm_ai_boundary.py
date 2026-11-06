"""PM's mission boundary against the exe, with polygons made here: the polygon 0x4c9af0 with its side
passes, the via lists 0x4cf350, the state switch 0x4cb7b0 and the query 0x4cc7f0 through all it calls.
Every corner's words, the answer and the query's state compared. The polygons aren't the shipped ones:
stars of three size classes and all four colours, queries across, along and near their sides.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_ai_boundary.py [areas]
"""
import ctypes
import os
import math
import random
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
os.environ['MW3_X87'] = '53'  # the reference's precision, only: docs/pm-tests.md
import pm_floors  # noqa: E402
import test_pm_zar  # noqa: E402

TEMPLATE, AREA, POLY, QUERY, VIA, STATE = 0x4c8140, 0x4cb370, 0x4c9af0, 0x4cc7f0, 0x4cf350, 0x4cb7b0
CSQRT = 0x5cefe2
CLAMP, CURRENT, TIME, PLACE = 0x4ccaf0, 0x62fa48, 0x8c4b60, 0x5cb260
COLOURS = {'blue': (0, 0, 255), 'cyan': (0, 255, 255), 'red': (255, 0, 0), 'yellow': (255, 255, 0)}
MESSAGES = (0x39, 0x3a, 0x3b, 0x3c, 0x40)
STATES = {0x39: 1, 0x3a: 0, 0x3b: 3, 0x3c: 2, 0x40: 0}


def nan(text):  # a NaN is a NaN: x87's is negative, ARM's positive
    return re.sub(r'\b[7f]f[89a-f][0-9a-f]{5}\b', 'nan', text)


class Exe(test_pm_zar.Emulated):
    def __init__(self):
        super().__init__()
        self.native_stub(0x5cee12, lambda c: 0)  # atexit
        self.native_stub(PLACE, lambda c: self.alloc(12))  # the vehicle's place, only into the record
        self.native_stub(0x5ce8c6, lambda c: self.alloc(c.arg(0)))  # operator new
        self.native_stub(0x5ce8c0, lambda c: 0)  # operator delete
        self.cpu._uc.mem_write(CSQRT, b'\xd9\xfa\xc3')  # _CIsqrt: fsqrt; ret
        self.call(TEMPLATE)
        self.area = self.alloc(0xc0)
        self.call(AREA, ecx=self.area)
        self.polys = []
        self.begin = self.alloc(4 * 64)
        self.w32(self.area + 8, self.begin)
        self.w32(self.area + 0xc, self.begin)
        self.w32(self.area + 0x10, self.begin + 4 * 64)
        self.vt = self.alloc(0x40)
        self.w32(self.vt + 0x1c, self.cpu.code(b'\x8d\x41\x40\xc3'))  # lea eax, [ecx + 0x40]; ret: the node's place
        self.ids, self.others = {}, {}

    def w32(self, a, v):
        self.cpu._uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def floats(self, a, vals):
        self.cpu._uc.mem_write(a, struct.pack(f'<{len(vals)}f', *vals))

    def polygon(self, colour, xyz, width, name):
        n = len(xyz) // 3
        verts, idx, spec, mat, mats = self.alloc(4 * len(xyz)), self.alloc(4 * n), self.alloc(0x30), self.alloc(0x20), self.alloc(4)
        self.floats(verts, xyz)
        for i in range(n):
            self.w32(idx + 4 * i, i)
        self.floats(mat + 4, colour)
        self.w32(mats, mat)
        self.w32(spec, n)
        self.w32(spec + 8, idx)
        self.w32(spec + 0x10, 1)
        self.w32(spec + 0x14, mats)
        p = self.alloc(0x64)
        self.call(POLY, ecx=p, args=[self.string(name), spec, verts, struct.unpack('<I', struct.pack('<f', width))[0]])
        self.w32(self.begin + 4 * len(self.polys), p)
        self.polys.append(p)
        self.w32(self.area + 0xc, self.begin + 4 * len(self.polys))

    def node(self, pos):
        n = self.alloc(0x60)
        self.w32(n, self.vt)
        self.floats(n + 0x40, pos)
        return n

    def via(self, pos, other, index, other_index):
        a = self.node(pos)
        self.ids[a] = index + 1
        b = self.others.get(other_index)
        if not b:
            b = self.others[other_index] = self.node(other)
        self.w32(a + 0x30, b)
        self.w32(b + 0x30, b)
        v = self.alloc(0x10)
        self.w32(v, 0)
        self.w32(v + 4, a)
        walker = self.alloc(0x10)
        self.w32(walker + 4, v)
        for p in self.polys:
            self.call(VIA, ecx=walker, args=[p])

    def off_side(self, r, poly, by):
        p = self.polys[poly]
        i = r.randrange((self.r32(p + 0xc) - self.r32(p + 8)) // 0x54)
        c = self.r32(p + 8) + 0x54 * i
        f = lambda o: struct.unpack('<f', struct.pack('<I', self.r32(c + o)))[0]  # noqa: E731
        t = r.uniform(0.1, 0.9) * f(0x24)
        return [f(0) + t * f(0x38) + by * f(0x2c), 0.0, f(8) + t * f(0x40) + by * f(0x34)], i

    def corner_id(self, c):
        if not c:
            return -1
        for i, p in enumerate(self.polys):
            b, e = self.r32(p + 8), self.r32(p + 0xc)
            if b <= c < e:
                return i * 1000 + (c - b) // 0x54
        return -2

    def poly_id(self, p):
        return self.polys.index(p) if p in self.polys else (-1 if not p else -2)

    def fs(self, a, n=1):
        return ' '.join(f'{self.r32(a + 4 * k):x}' for k in range(n))

    def vias_of(self, begin, end):
        o = ''
        for v in range(begin, end, 0x10):
            o += f'{self.corner_id(self.r32(v + 0xc))} {self.corner_id(self.r32(v + 8))} '
        return o

    def b1(self, a):
        return bytes(self.cpu._uc.mem_read(a, 1))[0]

    def dump(self):
        lines = []
        for p in self.polys:
            name = self.r32(p + 0x4c)
            lines.append(f'{self.fs(p + 0x14, 2)} {self.fs(p + 0x1c, 6)} {self.fs(p + 0x34, 4)} {self.b1(p + 0x44):x} {self.b1(p + 0x45):x} '
                         f'{self.b1(p + 0x46):x} {self.r32(p + 0x48):x} {self.cstr(name).decode() if name else "-"} '
                         f'{self.vias_of(self.r32(p + 0x58), self.r32(p + 0x5c))}|')
            for c in range(self.r32(p + 8), self.r32(p + 0xc), 0x54):
                lines.append(f'{self.fs(c, 11)} {self.fs(c + 0x2c, 6)} {self.vias_of(self.r32(c + 0x48), self.r32(c + 0x4c))}')
        return lines

    def query(self, frm, to, first, ways):
        a, b = self.alloc(12), self.alloc(12)
        self.floats(a, frm)
        self.floats(b, to)
        self.w32(self.area + 0x1c, self.polys[first] if first >= 0 else 0)
        r = self.call(QUERY, ecx=self.area, args=[a, b, ways])
        A = self.area
        i = lambda o: struct.unpack('<i', bytes(self.cpu._uc.mem_read(A + o, 4)))[0]  # noqa: E731
        c = lambda o: self.corner_id(self.r32(A + o))  # noqa: E731
        return (f'{1 if r == b else 0} {self.fs(r, 3)} {i(0xbc)} {self.poly_id(self.r32(A + 0x1c))} {c(0x20)} {c(0x24)} {c(0x28)} {c(0x2c)} {c(0x68)} '
                f'{self.fs(A + 0x30, 3)} {self.fs(A + 0x3c, 6)} {i(0x54)} {i(0x58)} {i(0x5c)} {self.fs(A + 0x64)} {self.fs(A + 0x6c, 3)} '
                f'{self.fs(A + 0x78, 3)} {self.fs(A + 0x84, 3)} {self.b1(A + 0xa8):x} {self.b1(A + 0xa9):x} ')

    def vehicle(self, jets):
        o, vt = self.alloc(0x400), self.alloc(0x200)
        self.w32(o, vt)
        same = self.cpu.code(b'\x89\xc8\xc3')  # mov eax, ecx; ret: itself, its own mech
        for slot in (0x2c, 0x48, 0x58):
            self.w32(vt + slot, same)
        self.w32(o + 1000, jets)
        return o

    def pull(self, v, pos, to, out_same):
        a, b, c = self.alloc(12), self.alloc(12), self.alloc(12)
        self.floats(a, pos)
        self.floats(b, to)
        out = b if out_same else c
        got = self.call(CLAMP, args=[v, a, b, out, 0]) & 0xff
        rec = self.r32(v + 0xd8)
        via = self.r32(rec + 0x4c)
        node = self.ids[self.r32(via + 4)] if via else 0
        return f'{got} {self.b1(rec + 0x50)} {node} {self.fs(out, 3) if got else ""}'

    def state(self, name, kind):
        for p in self.polys:
            self.call(STATE, ecx=p, args=[self.string(name), kind])


def star(r, cx, cz, size, n):
    pts = []
    for i in range(n):
        a = 2 * math.pi * i / n
        d = size * r.uniform(0.55, 1.0)
        pts += [cx + d * math.cos(a), r.choice([0.0, r.uniform(-20, 20)]), cz + d * math.sin(a)]
    return pts


def near_side(r, xyz, spread):
    return near_at(r, xyz, r.randrange(len(xyz) // 3), spread)


def near_at(r, xyz, i, spread):
    n = len(xyz) // 3
    i %= n
    j = (i + 1) % n
    t = r.random()
    return [xyz[3 * i] + (xyz[3 * j] - xyz[3 * i]) * t + r.uniform(-spread, spread), r.uniform(-5, 5),
            xyz[3 * i + 2] + (xyz[3 * j + 2] - xyz[3 * i + 2]) * t + r.uniform(-spread, spread)]


def main():
    areas = int(sys.argv[1]) if len(sys.argv) > 1 else 200
    p = test_pm_zar.load_pm_probe()
    F = ctypes.POINTER(ctypes.c_float)
    p.probe_pm_boundary_polygon.argtypes = [ctypes.c_float] * 3 + [ctypes.c_int, F, ctypes.c_float, ctypes.c_char_p]
    p.probe_pm_boundary_dump.argtypes = [ctypes.c_char_p, ctypes.c_int]
    p.probe_pm_boundary_via.argtypes = [F, F, ctypes.c_int, ctypes.c_int]
    p.probe_pm_boundary_state.argtypes = [ctypes.c_char_p, ctypes.c_int]
    p.probe_pm_boundary_query.argtypes = [F, F, ctypes.c_int, ctypes.c_int, ctypes.c_char_p, ctypes.c_int]
    buf = ctypes.create_string_buffer(1 << 22)
    fa = lambda v: (ctypes.c_float * len(v))(*v)  # noqa: E731
    r = random.Random(1999)
    queries = bad = pulls = pulled = hunted = 0
    outcomes, cases = {}, {}
    for k in range(areas):
        emu = Exe()
        p.probe_pm_boundary_begin()
        shapes = []
        for i in range(r.randint(1, 5)):
            size = r.choice([r.uniform(30, 300), r.uniform(800, 2500), r.uniform(3500, 5000), r.uniform(6500, 9000)])
            if k % 2 and i == 0:
                size = r.choice([r.uniform(3500, 5000), r.uniform(6500, 9000)])  # a long way round
            xyz = star(r, r.uniform(-size, size), r.uniform(-size, size), size, r.randint(3, 24))
            colour = 'red' if k % 2 and i == 0 else r.choice(list(COLOURS))
            name = r.choice(['a', 'b', 'door'])
            emu.polygon(COLOURS[colour], xyz, 3.0, name)
            p.probe_pm_boundary_polygon(*COLOURS[colour], len(xyz) // 3, fa(xyz), 3.0, name.encode())
            shapes.append(xyz)
        ours = buf.value.decode() if p.probe_pm_boundary_dump(buf, len(buf)) >= 0 else ''
        others = {}
        for i in range(r.randint(0, 6) + (30 if k % 2 else 0)):
            s = shapes[0] if k % 2 else r.choice(shapes)
            a, b = near_side(r, s, 40), near_side(r, s, 40)
            o = r.choice(list(others)) if others and r.random() < 0.3 else i  # two vias to one node
            b = others.setdefault(o, b)
            emu.via(a, b, i, o)
            p.probe_pm_boundary_via(fa(a), fa(b), i, o)
        p.probe_pm_boundary_dump(buf, len(buf))
        ours = nan(buf.value.decode()).splitlines()
        theirs = nan('\n'.join(emu.dump())).splitlines()
        if ours != theirs:
            bad += 1
            print(f'area {k}: {len(theirs)} and {len(ours)} lines')
            for j, (x, y) in enumerate(zip(theirs, ours)):
                if x != y:
                    print(f'  line {j}\n  exe  {x[:500]}\n  port {y[:500]}')
                    break
            p.probe_pm_boundary_end()
            if bad >= 4:
                break
            continue
        for q in range(120):
            if r.random() < 0.05:
                name, message = r.choice(['a', 'b', 'door']), r.choice(MESSAGES)
                emu.state(name, STATES[message])
                p.probe_pm_boundary_state(name.encode(), message)
            s = r.choice(shapes)
            size = max(abs(v) for v in s) + 1
            pick = lambda: r.choice([near_side(r, s, 2), near_side(r, s, 20), near_side(r, s, 300),  # noqa: E731
                                     [r.uniform(-size, size), 0.0, r.uniform(-size, size)]])
            a, b = pick(), pick()
            if k % 2 and r.random() < 0.5:  # just off the long outline, to somewhere round it
                by = r.choice([r.uniform(-1.4, 1.4), r.uniform(1.5, 2.4), r.uniform(-900, 900)])
                a, i = emu.off_side(r, 0, by)
                # Often across a side or two on: the side it's nearest isn't the one it crosses.
                b = near_at(r, shapes[0], i + r.choice([-3, -2, -1, 1, 2, 3, 5, 9]), r.choice([5, 200, 900]))
            first = r.choice([-1, -1, r.randrange(len(shapes))])
            ways = r.choice([0, 1, 1])
            theirs = emu.query(a, b, first, ways)
            n = p.probe_pm_boundary_query(fa(a), fa(b), first, ways, buf, len(buf))
            ours = buf.value.decode() if n >= 0 else 'probe failed'
            queries += 1
            o = theirs.split()[4]
            outcomes[o] = outcomes.get(o, 0) + 1
            if nan(theirs) != nan(ours):
                bad += 1
                print(f'area {k} query {q} {a} {b} first {first} ways {ways}\n  exe  {theirs}\n  port {ours}')
                break
        if k % 2:
            # The rare case, just off one side of the long outline, its first crossing another, the long
            # way round (0x4cc32c's branch). Every query goes to both, so the port can't leave a case out.
            xyz, n = shapes[0], len(shapes[0]) // 3
            for q in range(4000):
                i = r.randrange(n)
                by = r.choice([3, 12]) * r.choice([1, -1])
                by += r.uniform(-1.6, 1.6)
                j = (i + 1) % n
                u = r.random()
                ax, az = xyz[3 * i] + (xyz[3 * j] - xyz[3 * i]) * u, xyz[3 * i + 2] + (xyz[3 * j + 2] - xyz[3 * i + 2]) * u
                nx, nz = xyz[3 * j + 2] - xyz[3 * i + 2], xyz[3 * i] - xyz[3 * j]
                ln = math.hypot(nx, nz)
                a = [ax + nx / ln * by, 0.0, az + nz / ln * by]
                b = near_at(r, xyz, i + r.randint(-6, 6), r.choice([3, 30, 300, 900]))
                fa_, fb_ = fa(a), fa(b)
                ours = buf.value.decode() if p.probe_pm_boundary_query(fa_, fb_, -1, 0, buf, len(buf)) >= 0 else ''
                theirs = emu.query(a, b, -1, 0)
                if nan(theirs) != nan(ours):
                    bad += 1
                    print(f'area {k} hunted {a} {b}\n  exe  {theirs}\n  port {ours}')
                    break
                f = ours.split()
                case = p.probe_pm_boundary_case() if len(f) >= 28 and f[4] in ('15', '16') else 0
                if case:
                    cases[case] = cases.get(case, 0) + 1
                    hunted += 1
        # The clamp, three vehicles in turn: the polygon each met last counts on its next query.
        emu.w32(CURRENT, emu.area)
        vehicles = [emu.vehicle(0), emu.vehicle(1), emu.vehicle(2)]
        out = (ctypes.c_float * 3)()
        for q in range(40):
            i = r.randrange(3)
            s = r.choice(shapes)
            a, b = near_side(r, s, r.choice([2, 20, 300])), near_side(r, s, r.choice([2, 20, 300]))
            same = r.random() < 0.3  # `in` as `out`: then the height is the pulled point's
            theirs = emu.pull(vehicles[i], a, b, same)
            to = fa(b)
            got = p.probe_pm_boundary_pull(i + 1, fa(a), to, 1 if i else 0, to if same else out)
            at, via, got = (got >> 1) & 1, got >> 2, got & 1
            words = ' '.join(f'{w:x}' for w in struct.unpack('<3I', bytes(to if same else out))) if got else ''
            ours = f'{got} {at} {via} {words}'
            pulls += 1
            pulled += got
            if nan(theirs) != nan(ours):
                bad += 1
                print(f'area {k} pull {q} vehicle {i}\n  exe  {theirs}\n  port {ours}')
                break
        p.probe_pm_boundary_end()
        if bad >= 4:
            break
    print(f'hunted {cases}; {pulls} pulls, {pulled} moved; {areas} areas, {queries} queries, outcomes {dict(sorted(outcomes.items(), key=lambda x: int(x[0])))}: {bad} differ')
    if not bad:
        pm_floors.floors(queries=(queries, areas * 100), pulled=(pulled, areas), outcomes=(len(outcomes), 8 if areas >= 60 else 1))
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
