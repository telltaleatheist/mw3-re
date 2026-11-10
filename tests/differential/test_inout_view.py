"""The mission boundary's walls (the vehicle editor's Show InOut; docs/editor.md, The vehicle editor's pages) against
the original x86: the builder 0x4c42e0 and its polygon step 0x4c43b0 (with the corners' 0x4c45f0 and the triangle
adds 0x4c4520 / 0x4c4590), on random polygons. The colours asked (0x50de00) and the triangles added (0x53c6c0) are
recorded on both sides, the adds answering 0, 1 or 2 from the same list; then each polygon's "model too full", the
counts and the last answer.

    cmake --build build --target mw3probe && .venv/bin/python -P tests/differential/test_inout_view.py [runs]

Breaks, by hand in zainet_inout_view.cpp, each must fail: the wall 9 m high; yellow for kind 2; the 1024 test as >;
the second triangle's swap of b and c left out.
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import load_probe  # noqa: E402
from original import Original  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

WALLS, ADD_POLY, CORNERS_STEP = 0x4c42e0, 0x4c43b0, 0x4c45f0
COLOR, MODEL_ADD = 0x50de00, 0x53c6c0
MODEL = 0x00badd00


class Tri(ctypes.Structure):
    _fields_ = [('n', ctypes.c_int32), ('material', ctypes.c_int32), ('flag', ctypes.c_int32),
                ('zone', ctypes.c_uint8 * 4), ('pts', ctypes.c_float * 9)]


class Out(ctypes.Structure):
    _fields_ = [('full', ctypes.c_int32 * 64), ('tris', Tri * 2048), ('n_tris', ctypes.c_int32),
                ('colors', (ctypes.c_float * 3) * 256), ('n_colors', ctypes.c_int32), ('triangles', ctypes.c_int32),
                ('vertices', ctypes.c_int32), ('last', ctypes.c_int32)]


probe = load_probe()
probe.probe_inout_walls.argtypes = [ctypes.c_int32, ctypes.c_int32, ctypes.POINTER(ctypes.c_int32),
                                    ctypes.POINTER(ctypes.c_uint8), ctypes.POINTER(ctypes.c_int32),
                                    ctypes.POINTER(ctypes.c_float), ctypes.POINTER(ctypes.c_int32), ctypes.c_int32,
                                    ctypes.POINTER(Out)]


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


def case(rng):
    n = rng.randrange(1, 7)
    polys = []
    for _ in range(n):
        k = rng.choice([1, 2, 3, 4, 8, 20, 60, 150, 340, 342, 400])
        corners = [(f32(rng.uniform(-3000, 3000)), f32(rng.uniform(-50, 200)), f32(rng.uniform(-3000, 3000)))
                   for _ in range(k)]
        if rng.random() < 0.2 and k > 1:
            corners[1] = corners[0]  # a repeated corner
        polys.append(dict(corners=corners, tested=rng.choice([0, 1, 1, 1]), kind=rng.choice([0, 1, 2, 3, 4, -1])))
    answers = [rng.choice([0, 0, 0, 1, 2]) for _ in range(rng.randrange(1, 9))]
    return dict(start=rng.choice([0, 0, 100, 500, 900, 1000, 1021, 1023, 1024, 2000]), polys=polys, answers=answers)


RUN = {}  # the run in hand: its case, records and next answer (the stubs are set once)


def setup(o):
    def color(o):
        rgb = [struct.unpack('<f', struct.pack('<I', o.arg(k)))[0] for k in range(3)]
        RUN['colors'].append(tuple(rgb))
        return len(RUN['colors'])

    def add(o):
        if o.ecx != MODEL:
            raise RuntimeError('0x53c6c0 on %#x' % o.ecx)
        pts = struct.unpack('<9f', o.read(o.arg(0), 36))
        RUN['tris'].append((o.edx, o.arg(6), o.arg(8), o.read(o.arg(9), 4), pts))
        ans = RUN['answers']
        a = ans[RUN['next'] % len(ans)]
        RUN['next'] += 1
        return a

    o.stub_function(COLOR, color, 12)
    o.stub_function(MODEL_ADD, add, 0x2c)


def run_exe(o, c):
    RUN.clear()
    RUN.update(tris=[], colors=[], answers=c['answers'], next=0)
    rec = RUN
    b = o.alloc(0x40)
    o.write(b, bytes(0x40))
    o.call(WALLS, ecx=b, stack_args=(MODEL,))
    o.u32(b + 4, c['start'])
    full = []
    for p in c['polys']:
        verts = o.alloc(0x1c * len(p['corners']))
        for i, v in enumerate(p['corners']):
            o.write(verts + 0x1c * i, struct.pack('<3f', *v) + bytes(0x10))
        poly = o.alloc(0x40)
        o.write(poly, bytes(0x40))
        o.u32(poly + 0x08, verts)
        o.u32(poly + 0x0c, verts + 0x1c * len(p['corners']))
        o.write(poly + 0x35, bytes([p['tested']]))
        o.u32(poly + 0x38, p['kind'])
        slot = o.alloc(4)
        o.u32(slot, poly)
        o.call(ADD_POLY, ecx=b, stack_args=(slot,))
        full.append(o.uc.reg_read(UC_X86_REG_EAX) & 0xff)  # AL
    tri, verts, last = struct.unpack('<iI', o.read(b, 8)) + struct.unpack('<i', o.read(b + 0x3c, 4))
    return dict(full=full, tris=rec['tris'], colors=rec['colors'], counts=(tri, verts, last))


def run_port(c):
    counts = (ctypes.c_int32 * len(c['polys']))(*[len(p['corners']) for p in c['polys']])
    tested = (ctypes.c_uint8 * len(c['polys']))(*[p['tested'] for p in c['polys']])
    kinds = (ctypes.c_int32 * len(c['polys']))(*[p['kind'] for p in c['polys']])
    flat = [x for p in c['polys'] for v in p['corners'] for x in v]
    corners = (ctypes.c_float * max(1, len(flat)))(*flat)
    answers = (ctypes.c_int32 * len(c['answers']))(*c['answers'])
    out = Out()
    probe.probe_inout_walls(c['start'], len(c['polys']), counts, tested, kinds, corners, answers, len(c['answers']),
                            ctypes.byref(out))
    tris = [(t.n, t.material, t.flag, bytes(t.zone), tuple(t.pts)) for t in out.tris[:min(out.n_tris, 2048)]]
    return dict(full=list(out.full[:len(c['polys'])]), tris=tris, n_tris=out.n_tris,
                colors=[tuple(x) for x in out.colors[:out.n_colors]],
                counts=(out.triangles, out.vertices & 0xffffffff, out.last))


def main():
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 400
    rng = random.Random(4730)
    o = Original()
    setup(o)
    bad = 0
    seen = dict(full=0, tris=0, drawn=0)
    for n in range(runs):
        c = case(rng)
        heap = o.heap_next
        want = run_exe(o, c)
        o.heap_next = heap
        got = run_port(c)
        seen['full'] += sum(want['full'])
        seen['tris'] += len(want['tris'])
        if got['n_tris'] > 2048:
            print('run %d: more than 2048 triangles, raise the probe\'s table' % n)
            bad += 1
            continue
        for k in ('full', 'colors', 'tris', 'counts'):
            if want[k] != got[k]:
                bad += 1
                if bad <= 8:
                    print('run %d: %s differs' % (n, k))
                    if k == 'tris':
                        for i, (a, b) in enumerate(zip(want[k], got[k])):
                            if a != b:
                                print('  triangle %d\n  exe  %r\n  port %r' % (i, a, b))
                                break
                        print('  %d against %d' % (len(want[k]), len(got[k])))
                    else:
                        print('  exe  %r\n  port %r' % (want[k][:12], got[k][:12]))
                break
    if bad:
        print('FAIL: %d of %d runs differ' % (bad, runs))
        sys.exit(1)
    print('ok: %d runs the same: %d triangles added, %d polygons too many for the model' % (runs, seen['tris'],
                                                                                          seen['full']))


if __name__ == '__main__':
    main()
