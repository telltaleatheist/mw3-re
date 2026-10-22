"""The mission boundary's query against the original x86 (docs/ai.md
section 11): 0x4c2b00 (the state reset 0x4c2a80, each polygon's pass
0x4c2080 over 0x4c1410, the nearest outline point 0x4c2470 / 0x4c22c0, the
way round 0x4c2f20, the clear line 0x4c2e30 over 0x4c25f0 / 0x4c1770), the
pull 0x4c3000 that wraps it (its vehicle record left out of the port; the
caller's in and out apart and the same) and the messages 0x4c3bd0 that
switch a named shape's colour, on every mission's area of the six worlds as
test_inout.py loads them.

Segments are drawn around each area's polygons: both ends anywhere in their
box and past it, one end near a corner, short hops along a side, ends inside
a polygon, ends on a corner. Each answer (which point: `to`, the near or far
point, or a corner), its value, the area's whole state after it (pointers as
ids), the flags and the normal must match bit for bit at 24-bit precision.

    cmake --build build && .venv/bin/python tests/differential/test_inout_query.py [queries] [worlds...]
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_inout import OPERATION, Inout, probe, ZBD, WORLDS  # noqa: E402

probe.inout_query.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int]
probe.inout_pull_probe.argtypes = [ctypes.c_void_p] * 3 + [ctypes.c_int]
probe.inout_message.argtypes = [ctypes.c_char_p, ctypes.c_int]
QUERY, PULL, MESSAGE, CURRENT, POSITION, ANIM_NODE, ANIM_NODE2 = (0x4c2b00, 0x4c3000, 0x4c3bd0, 0x60b344,
                                                                  0x5a85e0, 0x50b5e0, 0x50b620)


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


class Query(Inout):
    def __init__(self, name):
        super().__init__(name)
        cpu = self.cpu
        cpu.fpu_control(0x007f)
        self.pts = self.alloc(0x40)
        # The vehicle 0x4c3000 is given: vt+0x2c an entity whose vt+0x40 is
        # the vehicle with its record (+0xd8); its place (0x5a85e0) the from point.
        self.veh = self.alloc(0x100)
        ent, vt_e, vt_v = self.alloc(0x10), self.alloc(0x100), self.alloc(0x100)
        cpu.write(self.veh, bytes(0x100))
        cpu.u32(self.veh, vt_v)
        cpu.u32(vt_v + 0x2c, self.method(ent))
        cpu.u32(ent, vt_e)
        cpu.u32(vt_e + 0x40, self.method(self.veh))
        cpu.stub_function(POSITION, lambda c: self.pts)
        self.anim_name = self.alloc(0x40)
        cpu.stub_function(ANIM_NODE, lambda c: self.anim_name)
        cpu.stub_function(ANIM_NODE2, lambda c: 0)

    def zero_leftovers(self):
        """Each area's +0x78..+0x88 are the stack's leftovers in the exe (the
        temporary area 0x4c26b0 copies them unset); every query sets the flags,
        and only the left-out vehicle record reads the normal. The port's are 0."""
        vec = self.u32(0x60b33c)
        if vec:
            for a in range(self.u32(vec + 4), self.u32(vec + 8), 0x88):
                self.cpu.write(a + 0x78, bytes(0x10))

    def query(self, frm, to):
        cpu = self.cpu
        a = self.u32(CURRENT)
        cpu.write(self.pts, struct.pack('<6f', *frm, *to))
        f, t = self.pts, self.pts + 12
        cpu.call(QUERY, ecx=a, stack_args=(f, t), count=50_000_000)
        r = cpu.uc.reg_read(19)
        kind = 0 if r == t else 1 if r == a + 0x3c else 2 if r == a + 0x48 else None
        if kind is None:
            v = self.vertex_id(a, r - 0xc)
            kind = v + 2 if v not in (0, 0xffffffff) else 0xffffffff
        out = struct.pack('<I', kind) + bytes(cpu.read(r, 12)) + self.query_state(a, f, t)
        out += bytes(cpu.read(a + 0x78, 2)) + b'\0\0' + bytes(cpu.read(a + 0x7c, 12))
        return out

    def pull(self, frm, to, same):
        cpu = self.cpu
        cpu.write(self.pts, struct.pack('<9f', *frm, *to, 0.0, 0.0, 0.0))
        out = self.pts + 12 if same else self.pts + 24
        cpu.call(PULL, ecx=self.veh, edx=self.pts + 12, stack_args=(out,), count=50_000_000)
        return cpu.uc.reg_read(19) & 0xff, bytes(cpu.read(self.pts + 12, 24))

    def message(self, name, msg):
        self.cpu.write(self.anim_name, name + b'\0')
        self.cpu.call(MESSAGE, ecx=0x1234, edx=msg)

    def area_polygons(self):
        a = self.u32(CURRENT)
        out = []
        for p in self.polygons(a):
            vb, ve = self.u32(p + 8), self.u32(p + 0xc)
            corners = [struct.unpack('<3f', bytes(self.cpu.read(v, 12))) for v in range(vb, ve, 0x1c)]
            nm = self.cstr(self.u32(p + 0x3c)) if self.u32(p + 0x3c) else b''
            out.append((corners, nm))
        return out


def segments(rng, polys, n):
    xs = [c[0] for p, _ in polys for c in p]
    zs = [c[2] for p, _ in polys for c in p]
    x0, x1, z0, z1 = min(xs), max(xs), min(zs), max(zs)
    w, h = x1 - x0, z1 - z0
    for _ in range(n):
        corners, _ = rng.choice(polys)
        c = rng.choice(corners)
        k = rng.random()
        y = rng.uniform(-20, 120)
        if k < 0.3:  # anywhere around the area
            a = (rng.uniform(x0 - 0.2 * w, x1 + 0.2 * w), y, rng.uniform(z0 - 0.2 * h, z1 + 0.2 * h))
            b = (rng.uniform(x0 - 0.2 * w, x1 + 0.2 * w), y, rng.uniform(z0 - 0.2 * h, z1 + 0.2 * h))
        elif k < 0.55:  # near a corner, out to anywhere
            a = (c[0] + rng.uniform(-40, 40), y, c[2] + rng.uniform(-40, 40))
            b = (rng.uniform(x0, x1), y, rng.uniform(z0, z1))
        elif k < 0.75:  # a short hop across a side
            i = corners.index(c)
            d = corners[(i + 1) % len(corners)]
            m = rng.random()
            px, pz = c[0] + (d[0] - c[0]) * m, c[2] + (d[2] - c[2]) * m
            a = (px + rng.uniform(-15, 15), y, pz + rng.uniform(-15, 15))
            b = (px + rng.uniform(-15, 15), y, pz + rng.uniform(-15, 15))
        elif k < 0.9:  # from inside the polygon (its corners' mean), out or in
            cx = sum(q[0] for q in corners) / len(corners)
            cz = sum(q[2] for q in corners) / len(corners)
            a = (cx + rng.uniform(-3, 3), y, cz + rng.uniform(-3, 3))
            b = (c[0] + rng.uniform(-60, 60), y, c[2] + rng.uniform(-60, 60))
            if rng.random() < 0.5:
                a, b = b, a
        else:  # on a corner exactly, or both ends the same
            a = (c[0], y, c[2])
            b = a if rng.random() < 0.3 else (rng.uniform(x0, x1), y, rng.uniform(z0, z1))
        yield tuple(map(f32, a)), tuple(map(f32, b))


def port_query(frm, to, buf):
    pts = (ctypes.c_float * 6)(*frm, *to)
    n = probe.inout_query(ctypes.addressof(pts), ctypes.addressof(pts) + 12, buf, len(buf))
    return buf.raw[:n]


def port_pull(frm, to, same):
    pts = (ctypes.c_float * 9)(*frm, *to, 0.0, 0.0, 0.0)
    base = ctypes.addressof(pts)
    r = probe.inout_pull_probe(base, base + 12, base + 24, same)
    return r, bytes(bytearray(pts)[12:36])


def main():
    queries = int(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1].isdigit() else 400
    worlds = [w for w in sys.argv[1:] if not w.isdigit()] or WORLDS
    rng = random.Random(1999)
    buf = ctypes.create_string_buffer(1 << 16)
    total = pulled = kinds = msgs = 0
    seen = set()
    for w in worlds:
        op = OPERATION[w]
        last = 4 if op < 3 else 6
        cases = [(1, 1, op, m, 1) for m in range(1, last + 1)] + [(1, 2, 0, 0, 1), (1, 4, 0, 0, 1)]
        for case in cases:
            emu = Query(w)
            if probe.sw_load(str(ZBD / w / 'gamez.zbd').encode()) != 0:
                raise SystemExit(f'{w}: the port could not read the world')
            emu.run(*case)
            probe.inout_run(*case, buf, len(buf))
            emu.zero_leftovers()
            if not emu.u32(CURRENT):
                continue
            polys = emu.area_polygons()
            if not polys:
                continue
            names = sorted({nm for _, nm in polys if nm})
            for i, (a, b) in enumerate(segments(rng, polys, queries)):
                where = f'{w} {case} query {i} {a} -> {b}'
                if names and rng.random() < 0.03:  # a message to a named shape
                    nm, msg = rng.choice(names), rng.choice([0x39, 0x3a, 0x3b, 0x3c, 0x3d])
                    emu.message(nm, msg)
                    probe.inout_message(nm, msg)
                    msgs += 1
                want = emu.query(a, b)
                got = port_query(a, b, buf)
                if want != got:
                    at = next((k for k in range(min(len(want), len(got))) if want[k] != got[k]), None)
                    raise SystemExit(f'{where}: answers differ at byte {at}\n  {want[:16].hex()}\n  {got[:16].hex()}')
                kind = struct.unpack_from('<I', want, 0)[0]
                seen.add(min(kind, 3))
                kinds += kind != 0
                if i % 8 == 0:
                    same = i % 16 == 0
                    want = emu.pull(a, b, same)
                    got = port_pull(a, b, same)
                    if want != got:
                        raise SystemExit(f'{where}: pulls differ (same buffer {same})\n  {want}\n  {got}')
                    pulled += want[0]
                total += 1
        print(f'{w}: {total} queries so far, {kinds} answered other than `to`, answers seen {sorted(seen)}')
    print(f'ok: {total} queries identical ({kinds} pulled; {pulled} pulls through 0x4c3000, {msgs} messages) '
          f'across {len(worlds)} worlds')


if __name__ == '__main__':
    main()
