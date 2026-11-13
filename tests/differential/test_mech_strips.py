"""The hardware mode's triangle stripper 0x552540 (docs/mechlib.md, 0x4fc8b0) against the exe on every
mechlib model: each read by the original's reader in the emulator (its materials world-like records, one per
distinct library material, flag byte +1 from the file), the original's stripper run on it (edx 0x7fffffff, 4,
as 0x4fc8b0 calls it), and the port's zmod::strip_model on the same model as the reader left it
(probe_strips.cpp strip_port). The whole polygon array after is compared: the count, each polygon's flags,
priority, material, zone and its vertex, normal, UV and colour lists, byte for byte.

    cmake --build build --target mw3probe && .venv/bin/python -P tests/differential/test_mech_strips.py [--verbose]

Then the same on SYNTH (default 3000) random small models (seeded): triangles, quads, strips of 3 and 4
and longer strips (odd and even) along the rows and columns of a grid, overlapping, with now and then a different material, priority, zone, normal,
colour or a UV off by a fraction, the UVs of each polygon shifted by whole repeats. The shipped models never
reach some of the join test's (0x5503c0) and the join's (0x551190) branches; these do.

BREAK=1 runs the port with a strip limit one under the longest strip the exe makes on the library (printed),
which must change some model.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import FTOL, IAT_FTOL, ROOT  # noqa: E402
from test_mechlib import MECHLIB, READ_MODEL, HANDLE, Emulated, entries, walk_tree  # noqa: E402

STRIP, NEW, DELETE, LOCK, UNLOCK = 0x552540, 0x5abd26, 0x5abd1a, 0x5bb4f8, 0x5bb4f4
LONGEST = 26  # the longest strip the exe makes on the 1.2 library (this test prints it)

probe = ctypes.CDLL(str(next((ROOT / 'build').glob('libmw3probe.*'))))
probe.strip_load.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_char_p,
                             ctypes.c_int, ctypes.c_char_p, ctypes.c_int]
probe.strip_port.argtypes = [ctypes.c_int, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p, ctypes.c_int,
                             ctypes.POINTER(ctypes.c_int)]
CAP = 1 << 24
OUT = (ctypes.c_uint8 * CAP)()


def dump(emu, m, table, nmat):
    r, u = emu.cpu.read, emu.u32
    polys = u(m + 0x10)
    t = u(m + 0x34)
    out = bytearray()
    for i in range(polys):
        p = t + 0x24 * i
        flags, prio, vi, ni, uv, col = struct.unpack('<IiIIII', r(p, 24))
        mat, zone = struct.unpack('<II', r(p + 0x1c, 8))
        mi = (mat - table) // 0x2c if table <= mat < table + 0x2c * nmat else -1
        k = flags & 0xff
        out += struct.pack('<IiiI', flags, prio, mi, zone)
        for ptr, size, cond in ((vi, 4, True), (ni, 4, bool(flags & 0x200)), (uv, 8, True), (col, 12, True)):
            on = bool(ptr) and cond and k > 0
            out += bytes([on])
            if on:
                out += r(ptr, size * k)
    return polys, bytes(out)


W, H = 5, 4  # the synthetic models' grid of vertices


def synthetic_polys(rng):
    """A random model on the grid: (flags, priority, material, zone, verts, normals, uvs, colours) each."""
    def corners(x, y):
        a = y * W + x
        return a, a + 1, a + W + 1, a + W

    def rot(seq):
        r = rng.randrange(len(seq))
        seq = seq[r:] + seq[:r]
        return seq[::-1] if rng.random() < 0.3 else seq

    shapes = []
    for y in range(H - 1):
        for x in range(W - 1):
            if rng.random() > 0.85:
                continue
            a, b, c, d = corners(x, y)
            kind = rng.randrange(6)
            if kind == 0:
                shapes.append((rot([a, b, c, d]), False))
            elif kind == 1:
                tris = [[a, b, c], [a, c, d]] if rng.random() < 0.5 else [[a, b, d], [b, c, d]]
                shapes += [(t if rng.random() < 0.8 else t[::-1], False) for t in tris]
            elif kind == 2:
                z = rng.choice([[a, b, d, c], [b, c, a, d], [c, d, b, a], [d, a, c, b]])
                shapes.append((z[::-1] if rng.random() < 0.5 else z, True))
            elif kind == 3:
                shapes.append((rot([a, b, c]), False))
            elif kind == 5:  # a strip of one triangle
                shapes.append((rot(rng.choice([[a, b, c], [a, c, d], [a, b, d], [b, c, d]])), True))
            else:
                shapes.append((rot([a, b, c, d, a + 2 * W]) if y + 2 < H else rot([a, c, d]), False))
    for _ in range(rng.randrange(5)):  # longer strips along a row or a column, odd counts too
        if rng.random() < 0.5:
            y, x0 = rng.randrange(H - 1), rng.randrange(W - 1)
            k = rng.randint(1, W - 1 - x0)
            pairs = [(y * W + x, (y + 1) * W + x) for x in range(x0, x0 + k + 1)]
        else:
            x, y0 = rng.randrange(W - 1), rng.randrange(H - 1)
            k = rng.randint(1, H - 1 - y0)
            pairs = [(y * W + x, y * W + x + 1) for y in range(y0, y0 + k + 1)]
        first = rng.random() < 0.5
        z = [v for p in pairs for v in (p if first else p[::-1])]
        if rng.random() < 0.5:
            z = z[:-1]
        shapes.append((z[::-1] if rng.random() < 0.5 else z, True))
    rng.shuffle(shapes)
    out = []
    for verts, strip in shapes:
        k = len(verts)
        flags = k | (0x400 if strip else 0)
        flags |= 0x200 if rng.random() < 0.97 else 0
        flags |= 0x100 if rng.random() < 0.04 else 0
        prio = 0 if rng.random() < 0.96 else 1
        mat = 0 if rng.random() < 0.9 else rng.choice([1, 2])
        zone = 0 if rng.random() < 0.96 else 1 << rng.randrange(8)
        normals = [v if rng.random() < 0.98 else v + 50 for v in verts]
        ou, ov = (rng.randint(-2, 2), rng.randint(-2, 2)) if rng.random() < 0.7 else (0, 0)
        uvs = []
        for v in verts:
            u, w = (v % W) * 0.3 + 0.1 + ou, (v // W) * 0.45 - 0.2 + ov
            e = rng.random()
            if e < 0.02:
                u += 0.01
            elif e < 0.04:
                w += 0.0005
            uvs += [u, w]
        cols = []
        for v in verts:
            cols += [1.0, 1.0, 1.0] if rng.random() < 0.98 else [0.5, 1.0, 1.0]
        out.append((flags, prio, mat, zone, verts, normals, uvs, cols))
    return out


def synthetic(emu, count, limit, verbose):
    """The exe and the port on `count` random models; returns (models changed by the exe, models differing)."""
    cpu = emu.cpu
    rng = random.Random(1999)
    mat_flags = bytes([1, 1, 0])
    changed = bad = 0
    for n in range(count):
        emu.next = 0x40000000
        table = emu.zeroed(0x2c * len(mat_flags))
        for i, f in enumerate(mat_flags):
            cpu.write(table + 0x2c * i + 1, bytes([f]))
        polys = synthetic_polys(rng)
        m = emu.zeroed(0x60)
        t = emu.zeroed(0x24 * len(polys))
        cpu.u32(m + 0x10, len(polys))
        cpu.u32(m + 0x14, W * H)
        cpu.u32(m + 0x34, t)
        for i, (flags, prio, mat, zone, verts, normals, uvs, cols) in enumerate(polys):
            arrs = []
            for fmt, vals in (('I', verts), ('I', normals), ('f', uvs), ('f', cols)):
                a = emu.alloc(4 * len(vals))
                cpu.write(a, struct.pack(f'<{len(vals)}{fmt}', *vals))
                arrs.append(a)
            cpu.write(t + 0x24 * i, struct.pack('<IiIIIIIII', flags, prio, *arrs, emu.alloc(4 * len(verts)),
                                                table + 0x2c * mat, zone))
        header = cpu.read(m, 0x5c)
        n0, before = dump(emu, m, table, len(mat_flags))
        cpu.call(STRIP, ecx=m, edx=0x7fffffff, stack_args=(4,), count=2_000_000_000)
        n1, after = dump(emu, m, table, len(mat_flags))
        vbuf = (ctypes.c_float * (3 * W * H))()
        used = probe.strip_load(0, header, vbuf, W * H, before, n0, mat_flags, len(mat_flags))
        assert used == len(before), (n, used, len(before))
        np_ = ctypes.c_int()
        w = probe.strip_port(0, limit, 4, OUT, CAP, ctypes.byref(np_))
        changed += n1 != n0
        if np_.value != n1 or bytes(OUT[:w]) != after:
            bad += 1
            if verbose or bad <= 5:
                print(f'FAIL synthetic model {n}: polygons {n0} -> exe {n1}, port {np_.value}')
    return changed, bad


def main():
    verbose = '--verbose' in sys.argv
    limit = LONGEST - 1 if os.environ.get('BREAK') == '1' else 0x7fffffff
    data = MECHLIB.read_bytes()
    emu = Emulated(data)
    cpu = emu.cpu
    cpu.import_code(IAT_FTOL, FTOL)
    cpu.stub_function(NEW, lambda c: emu.zeroed(c.arg(0)))
    cpu.stub_function(DELETE, lambda c: 0)
    cpu.stub_import(LOCK, lambda c: 0)
    cpu.stub_import(UNLOCK, lambda c: 0)
    mats = []
    for name, start, length in entries(data):
        if name == 'materials':
            count, = struct.unpack_from('<I', data, start)
            pos = start + 4
            for _ in range(count):
                want = emu.material(pos)
                mats.append(want[4:4 + 0x28])
                pos += struct.unpack_from('<i', want)[0]
    distinct, of = [], []
    for raw in mats:
        if raw not in distinct:
            distinct.append(raw)
        of.append(distinct.index(raw))
    table = cpu.alloc(0x2c * len(distinct))
    for i, raw in enumerate(distinct):
        cpu.write(table + 0x2c * i, raw + bytes(4))
    flags = bytes(raw[1] for raw in distinct)

    models = changed = bad = polys0 = polys1 = longest = 0
    for name, start, length in entries(data):
        if not name.endswith('.flt'):
            continue
        offsets = []
        walk_tree(data, start, offsets)
        for off in offsets:
            emu.next, emu.pos = 0x40000000, off
            node = emu.alloc(0xd0)
            cpu.write(node, bytes(0xd0))
            cpu.u32(node + 0x3c, 1)
            cpu.call(READ_MODEL, stack_args=(HANDLE, node))
            m = emu.u32(node + 0x3c)
            t = emu.u32(m + 0x34)
            for i in range(emu.u32(m + 0x10)):
                p = t + 0x24 * i + 0x1c
                cpu.u32(p, table + 0x2c * of[emu.u32(p)])
            header = cpu.read(m, 0x5c)
            nv = emu.u32(m + 0x14)
            verts = struct.unpack(f'<{3 * nv}f', cpu.read(emu.u32(m + 0x38), 12 * nv)) if nv else ()
            n0, before = dump(emu, m, table, len(distinct))
            cpu.call(STRIP, ecx=m, edx=0x7fffffff, stack_args=(4,), count=2_000_000_000)
            n1, after = dump(emu, m, table, len(distinct))
            t1 = emu.u32(m + 0x34)
            for i in range(n1):
                f = emu.u32(t1 + 0x24 * i)
                if f & 0x400:
                    longest = max(longest, f & 0xff)
            vbuf = (ctypes.c_float * max(len(verts), 1))(*verts)
            used = probe.strip_load(0, header, vbuf, nv, before, n0, flags, len(flags))
            assert used == len(before), (name, off, used, len(before))
            np_ = ctypes.c_int()
            w = probe.strip_port(0, limit, 4, OUT, CAP, ctypes.byref(np_))
            mine = bytes(OUT[:w])
            models += 1
            changed += n1 != n0
            polys0 += n0
            polys1 += n1
            if np_.value != n1 or mine != after:
                bad += 1
                if verbose or bad <= 5:
                    print(f'FAIL {name} +{off - start:#x}: polygons {n0} -> exe {n1}, port {np_.value}')
    print(f'{models} models, {changed} stripped by the exe ({polys0} polygons -> {polys1}), longest strip {longest}')
    count = int(os.environ.get('SYNTH', '3000'))
    s_changed, s_bad = synthetic(emu, count, limit, verbose)
    print(f'{count} synthetic models, {s_changed} stripped by the exe, {s_bad} differ')
    bad += s_bad
    if bad:
        print(f'{bad} differ')
        sys.stdout.flush(); os._exit(1)
    print(f'ok: every model\'s polygon array identical to the exe\'s')
    sys.stdout.flush(); os._exit(0)


if __name__ == '__main__':
    main()
