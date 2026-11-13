"""Does the hardware mode's triangle stripper change any hit test?
(docs/mechlib.md, 0x4fc8b0 / 0x552540.)

    cmake --build build && .venv/bin/python tests/differential/explore/strip_hits.py [grid]

Every mechlib model is read by the original's reader in the emulator, its
materials pointed at world-like records (one per distinct library material,
flag byte +1 from the file), and the original stripper 0x552540 (edx
0x7fffffff, 4, as 0x4fc8b0 calls it) run on it. The model before and after
goes to the port's model_ground (0x54c5f0) and model_segments (0x54f450),
which match the exe bit for bit with strips (test_di.py), over a grid of
probes and segments, each side keeping its own cached polygon (+0x58) as in
play. Reported per model: queries whose records differ in order, in
content (as sets), and in the answer a caller takes (the highest ground hit,
the nearest crossing).
"""
import ctypes
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from original import FTOL, IAT_FTOL, ROOT  # noqa: E402
from test_mechlib import MECHLIB, READ_MODEL, HANDLE, Emulated, entries, walk_tree  # noqa: E402

import os  # noqa: E402
SHOW = tuple(os.environ.get('STRIP_SHOW', ':-1').split(':'))
SHOW = (SHOW[0], int(SHOW[1]))  # 'mech_x.flt:offset' prints that model's big differences
TOL = 1e-3  # metres (and normal components): rounding below this is left alone
STRIP, NEW, DELETE, LOCK, UNLOCK = 0x552540, 0x5abd26, 0x5abd1a, 0x5bb4f8, 0x5bb4f4

probe = ctypes.CDLL(str(next((ROOT / 'build').glob('libmw3probe.*'))))
probe.strip_load.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_char_p,
                             ctypes.c_int, ctypes.c_char_p, ctypes.c_int]
probe.strip_ground.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_int, ctypes.c_float, ctypes.c_void_p]
probe.strip_segments.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p]
OUT = (ctypes.c_uint32 * 200000)()


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


def ground(slot, probes, top):
    buf = (ctypes.c_float * len(probes))(*probes)
    n = probe.strip_ground(slot, buf, len(probes) // 3, top, OUT)
    return list(OUT[:n])


def segments(slot, segs):
    buf = (ctypes.c_float * len(segs))(*segs)
    n = probe.strip_segments(slot, buf, len(segs) // 6, OUT)
    return list(OUT[:n])


def records(words, per):
    """Splits a probe's output into per-probe (count, [hit tuples]) without the cache word."""
    recs, i = [], 0
    while i < len(words) - 1:
        c = words[i]
        hits = [tuple(words[i + 1 + per * j:i + 1 + per * (j + 1)]) for j in range(c)]
        recs.append(hits)
        i += 1 + per * c
    return recs


def f(w):
    return struct.unpack('<f', struct.pack('<I', w))[0]


def ground_answer(hits):
    return max((f(h[3]) for h in hits), default=None)


def seg_answer(hits, start):
    best = None
    for h in hits:
        p = [f(w) for w in h[3:6]]
        d = sum((a - b) ** 2 for a, b in zip(p, start))
        if best is None or d < best:
            best = d
    return best


def main():
    grid = int(sys.argv[1]) if len(sys.argv) > 1 else 12
    data = MECHLIB.read_bytes()
    emu = Emulated(data)
    cpu = emu.cpu
    cpu.import_code(IAT_FTOL, FTOL)
    cpu.stub_function(NEW, lambda c: emu.zeroed(c.arg(0)))
    cpu.stub_function(DELETE, lambda c: 0)
    cpu.stub_import(LOCK, lambda c: 0)
    cpu.stub_import(UNLOCK, lambda c: 0)
    # the library's materials: one world-like record per distinct file record
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

    totals = dict(models=0, stripped=0, polys0=0, polys1=0, queries=0, bits=0, count=0, content=0, answer=0)
    worst = []
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
            vbuf = (ctypes.c_float * max(len(verts), 1))(*verts)
            for slot, (n, blob) in enumerate(((n0, before), (n1, after))):
                used = probe.strip_load(slot, header, vbuf, nv, blob, n, flags, len(flags))
                assert used == len(blob), (name, off, used, len(blob))
            totals['models'] += 1
            totals['stripped'] += n1 != n0
            totals['polys0'] += n0
            totals['polys1'] += n1
            if not nv or n0 == n1:
                continue
            xs, ys, zs = verts[0::3], verts[1::3], verts[2::3]
            x0, x1, y0, y1, z0, z1 = min(xs), max(xs), min(ys), max(ys), min(zs), max(zs)
            diff = dict(bits=0, count=0, content=0, answer=0, queries=0, worst=0.0, shown=0)

            def compare(a, b, per, answer, q=()):
                ra, rb = records(a, per), records(b, per)
                diff['queries'] += 1
                if ra == rb:
                    return
                diff['bits'] += 1
                nf = per - (2 if per == 6 else 1)  # the float fields lead each hit
                if [len(h) for h in ra] != [len(h) for h in rb]:
                    diff['count'] += 1
                else:
                    for ha, hb in zip(ra, rb):
                        ka = sorted(tuple(round(f(w), 3) for w in h[:nf]) + h[nf:] for h in ha)
                        kb = sorted(tuple(round(f(w), 3) for w in h[:nf]) + h[nf:] for h in hb)
                        if any(x[nf:] != y[nf:] or max(abs(u - v) for u, v in zip(x[:nf], y[:nf])) > TOL
                               for x, y in zip(ka, kb)):
                            diff['content'] += 1
                            break
                for ha, hb in zip(ra, rb):
                    x, y = answer(ha), answer(hb)
                    d = float('inf') if (x is None) != (y is None) else abs(x - y) if x is not None else 0.0
                    if d > TOL:
                        if SHOW == (name, off - start) and d > 0.1 and diff['shown'] < 4:
                            diff['shown'] += 1
                            print('query', [round(v, 3) for v in q], 'answers', x, y)
                            for side, r in (('plain', ra), ('strip', rb)):
                                print('  ', side, [[round(f(w), 3) for w in h[:nf]] + list(h[nf:]) for h in r[0]])
                        diff['answer'] += 1
                        diff['worst'] = max(diff['worst'], d)
                        break

            for i in range(grid):
                for j in range(grid):
                    x = x0 + (x1 - x0) * (i + 0.5) / grid
                    z = z0 + (z1 - z0) * (j + 0.5) / grid
                    rad = 0.08 * max(x1 - x0, z1 - z0)
                    sets = [[x, 0.0, z], [x, 0.0, z] + sum(([x + dx * rad, 0.0, z + dz * rad] for dx, dz in
                                                             ((1, 0), (-1, 0), (0, 1), (0, -1), (.7, .7), (-.7, -.7))), [])]
                    for probes in sets:
                        for top in (y1 + 1.0, (y0 + y1) / 2):
                            compare(ground(0, probes, top), ground(1, probes, top), 6, ground_answer, probes[:3] + [top])
                    y = y0 + (y1 - y0) * (j + 0.5) / grid
                    shots = [([x, y1 + 1, z], [x, y0 - 1, z]),
                             ([x0 - 1, y, z], [x1 + 1, y, z]), ([x1 + 1, y, z], [x0 - 1, y, z]),
                             ([x, y, z0 - 1], [x, y, z1 + 1]), ([x, y, z1 + 1], [x, y, z0 - 1])]
                    for a, b in shots:
                        compare(segments(0, a + b), segments(1, a + b), 7, lambda h, a=a: seg_answer(h, a), a + b)
                    pack = sum((a + b for a, b in shots[1:]), [])
                    compare(segments(0, pack), segments(1, pack), 7, lambda h, a=shots[1][0]: seg_answer(h, a), pack[:6])
            for k in ('bits', 'count', 'content', 'answer', 'queries'):
                totals[k] += diff[k]
            worst.append((diff['answer'], diff['count'] + diff['content'], diff['bits'], diff['queries'], name, off - start, n0, n1,
                          diff['worst']))
    worst.sort(reverse=True)
    print(f"{totals['models']} models, {totals['stripped']} changed by the stripper "
          f"({totals['polys0']} polygons -> {totals['polys1']})")
    print(f"{totals['queries']} queries on the changed models: {totals['bits']} records differ in any bit, "
          f"{totals['count']} in the number of hits, {totals['content']} beyond {TOL} in a hit, "
          f"{totals['answer']} in the answer taken (the highest ground hit, the nearest crossing) beyond {TOL}")
    print(f"models with any bit different: {sum(1 for w in worst if w[2])}; with a different answer: "
          f"{sum(1 for w in worst if w[0])}")
    for w in worst[:10]:
        print(f'  {w[4]} model at +{w[5]}: answer {w[0]} (worst {w[8]:.4g}), hits {w[1]}, bits {w[2]} of '
              f'{w[3]} queries ({w[6]} -> {w[7]} polygons)')


if __name__ == '__main__':
    main()
