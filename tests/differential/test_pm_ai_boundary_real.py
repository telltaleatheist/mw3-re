"""PM's mission boundary against the exe on the shipped polygons: every world's areas as the port's
loader reads them (campaign, standalone, Instant Action, training), each area's polygons built by the
exe's constructor and the port's, every corner's words compared, then queries around each corner and
side. The loader itself isn't compared (test_pm_ai_inout has its choice of areas, the counts are in
pm-map), nor the ground under the corners: heights are as the file has them, and queries are in x and z.
Via nodes aren't here: they wait for the net records.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_ai_boundary_real.py [worlds]
"""
import ctypes
import os
import math
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
os.environ['MW3_X87'] = '53'  # the reference's precision, only: docs/pm-tests.md
import pm_floors  # noqa: E402
import test_pm_ai_boundary as tb  # noqa: E402
import test_pm_zar  # noqa: E402
from original import ROOT  # noqa: E402

PM = ROOT / 'game' / 'pm'
GAMES = {1: 'campaign', 5: 'standalone', 2: 'instant action', 4: 'training'}
KINDS = {0: 'red', 1: 'yellow', 2: 'blue', 3: 'cyan'}


def areas_of(p, buf, world, game):
    n = p.probe_pm_boundary_world(str(world).encode(), game, buf, len(buf))
    if n < 0:
        raise SystemExit(f'{world}: the port read gave {n}')
    out = {}
    for line in buf.value.decode('latin1').splitlines():
        f = line.split()
        c = int(f[3])
        words = [int(x, 16) for x in f[4:4 + 3 * c]]
        xyz = list(struct.unpack(f'<{len(words)}f', struct.pack(f'<{len(words)}I', *words)))
        out.setdefault(int(f[0]), []).append((KINDS[int(f[1])], '' if f[2] == '-' else f[2], xyz))
    return out


def candidates(r, polys, each):
    """Points off each side and round each corner, to places a vehicle would head for."""
    for kind, _, xyz in polys:
        n = len(xyz) // 3
        if n < 3:
            continue
        for i in range(n):
            if each < 1 and r.random() > each:
                continue
            for _ in range(max(1, int(each))):
                j = (i + 1) % n
                u = r.choice([0.0, 0.02, r.random(), 0.98])
                ax, az = xyz[3 * i] + (xyz[3 * j] - xyz[3 * i]) * u, xyz[3 * i + 2] + (xyz[3 * j + 2] - xyz[3 * i + 2]) * u
                nx, nz = xyz[3 * j + 2] - xyz[3 * i + 2], xyz[3 * i] - xyz[3 * j]
                ln = (nx * nx + nz * nz) ** 0.5 or 1.0
                w = r.choice([3.0, 12.0])
                by = r.choice([w + r.uniform(-1.6, 1.6), w + r.uniform(-1.6, 1.6), r.uniform(-20, 20), r.uniform(-400, 400)]) * r.choice([1, -1])
                a = [ax + nx / ln * by, xyz[3 * i + 1], az + nz / ln * by]
                pick = r.randrange(3)
                if pick == 0:  # along the outline, a side or several on
                    b = tb.near_at(r, xyz, i + r.randint(-6, 6), r.choice([3, 30, 300, 900]))
                elif pick == 1:  # across to any side
                    b = tb.near_at(r, xyz, r.randrange(n), r.choice([5, 200]))
                else:  # straight on, the way a mech walks
                    t = r.uniform(30, 3000) * r.choice([1, -1])
                    d = r.random() * 6.283
                    b = [a[0] + t * math.cos(d), a[1], a[2] + t * math.sin(d)]
                yield a, b


def main():
    worlds = sorted(PM.glob('zbd/c*/gamez.zbd'))[:int(sys.argv[1]) if len(sys.argv) > 1 else 4]
    p = test_pm_zar.load_pm_probe()
    F = ctypes.POINTER(ctypes.c_float)
    p.probe_pm_boundary_polygon.argtypes = [ctypes.c_float] * 3 + [ctypes.c_int, F, ctypes.c_float, ctypes.c_char_p]
    p.probe_pm_boundary_dump.argtypes = [ctypes.c_char_p, ctypes.c_int]
    p.probe_pm_boundary_query.argtypes = [F, F, ctypes.c_int, ctypes.c_int, ctypes.c_char_p, ctypes.c_int]
    p.probe_pm_boundary_world.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_int]
    buf = ctypes.create_string_buffer(1 << 24)
    fa = lambda v: (ctypes.c_float * len(v))(*v)  # noqa: E731
    r = random.Random(1999)
    bad = queries = polygons = corners = areas = screened = 0
    outcomes, cases, classes = {}, {}, {}
    for world in worlds:
        for game, label in GAMES.items():
            for index, polys in sorted(areas_of(p, buf, world.relative_to(ROOT), game).items()):
                polys = [x for x in polys if len(x[2]) >= 9]
                if not polys:
                    continue
                areas += 1
                polygons += len(polys)
                emu = tb.Exe()
                p.probe_pm_boundary_begin()
                for kind, name, xyz in polys:
                    corners += len(xyz) // 3
                    emu.polygon(tb.COLOURS[kind], xyz, 3.0, name or 'x')
                    p.probe_pm_boundary_polygon(*tb.COLOURS[kind], len(xyz) // 3, fa(xyz), 3.0, (name or 'x').encode())
                n = p.probe_pm_boundary_dump(buf, len(buf))
                ours = tb.nan(buf.value.decode()).splitlines() if n >= 0 else []
                theirs = tb.nan('\n'.join(emu.dump())).splitlines()
                where = f'{world.parent.name} {label} {index}'
                if ours != theirs:
                    bad += 1
                    print(f'{where}: {len(theirs)} and {len(ours)} lines')
                    for j, (x, y) in enumerate(zip(theirs, ours)):
                        if x != y:
                            print(f'  line {j}\n  exe  {x[:400]}\n  port {y[:400]}')
                            break
                    p.probe_pm_boundary_end()
                    continue
                for line in theirs:
                    if line.endswith('|'):
                        w = line.split()[1]
                        classes[w] = classes.get(w, 0) + 1
                sides = sum(len(x[2]) // 3 for x in polys)
                # Every candidate through the port; the exe gets a sample, and every rare case.
                sample = min(1.0, 300.0 / (sides * 40))
                rare = 0
                for a, b in candidates(r, polys, 40):
                    fa_, fb_ = fa(a), fa(b)
                    n = p.probe_pm_boundary_query(fa_, fb_, -1, 0, buf, len(buf))
                    ours = buf.value.decode() if n >= 0 else 'probe failed'
                    screened += 1
                    f = ours.split()
                    case = p.probe_pm_boundary_case() if len(f) > 4 and f[4] in ('15', '16') else 0
                    if case:
                        cases[case] = cases.get(case, 0) + 1
                    if case:
                        rare += 1
                    if (not case or rare > 100) and r.random() > sample:  # no more than 100 of them an area
                        continue
                    theirs = emu.query(a, b, -1, 0)
                    queries += 1
                    o = theirs.split()[4]
                    outcomes[o] = outcomes.get(o, 0) + 1
                    if tb.nan(theirs) != tb.nan(ours):
                        bad += 1
                        print(f'{where} {a} {b}\n  exe  {theirs}\n  port {ours}')
                        break
                p.probe_pm_boundary_end()
                print(f'{where}: {len(polys)} polygons, {sides} corners; so far {screened} screened, {queries} on the exe, rare {cases}, {bad} differ', flush=True)
                if bad >= 4:
                    break
            if bad >= 4:
                break
    print(f'{len(worlds)} worlds, {areas} areas, {polygons} polygons, {corners} corners (widths {classes}); {screened} screened, '
          f'{queries} on the exe, outcomes {dict(sorted(outcomes.items(), key=lambda x: int(x[0])))}, rare cases {cases}: {bad} differ')
    if not bad:
        pm_floors.floors(worlds=(len(worlds), 4 if len(sys.argv) < 2 else 1), areas=(areas, 25 if len(sys.argv) < 2 else 1), nearest_side=(cases.get(1, 0), 20 if len(sys.argv) < 2 else 0))
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
