"""The AI's regions against the original x86 (docs/ai.md, Regions): the lookup
by name 0x4c5c00 with its list (0x4c5e50, 0x4c5f00), the short-lived net
builder it uses (CZAiNetEdit 0x4bcba0 / 0x4bd020 with its nodes 0x4c6d30 /
0x4c6da0, the link pass 0x4d53f0 / 0x410590, arrangenodes 0x4c74a0), the
loop check 0x4bf000, the edges 0x4bf1b0 / 0x4bf3f0 / 0x4c5960, the numbered
objects' registry (0x58aec0, 0x58b1c0, 0x58b160, 0x58b310), and the
inside test 0x4c57d0 / 0x4c5a30.

    cmake --build build && .venv/bin/python tests/differential/test_ai_regions.py

Every reg_*.zrd in every mission's reader archive, each looked up twice, plus
every net file and a missing name as regions; then made-up files reaching the
paths the shipped ones do not. The lookups run at the load's 53 bits; then
points round each region (its corners, the middles of its sides, inside and
outside it) are tested in turn at the 24 bits of play, so where each test
starts (the side the last one failed at) is checked too. Both sides' output
streams (tests/probe_ai_net.cpp) and their logs of zerr reports must match.
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from ai_world import ROOT, Emulated, RegionsRun, archive, mission_archives, ser  # noqa: E402
import zrd  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
probe.probe_regions_run.argtypes = [ctypes.c_char_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p,
                                    ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p, ctypes.c_int,
                                    ctypes.c_void_p]
WORK = ROOT / 'build' / 'ai_regions'
CAP, LOGCAP = 1 << 22, 1 << 18
# Files the original crashes on, which the port reads (docs/fixes.md).
CRASHES = {'no name'}
# Places in the original the files must reach.
COVER = {0x4bf086: 'placeholder skipped', 0x4bf132: 'link to nothing', 0x4bf197: 'not a loop',
         0x4bf18a: 'a loop', 0x4c5ca6: 'name mismatch', 0x4c5cb5: 'region kept', 0x4c59e6: 'zero-length side',
         0x4bce63: 'duplicate node', 0x4c74d0: 'arrangenodes', 0x4bf199: 'not a region file',
         0x4c5843: 'inside test wraps round'}


def port_events(buf, n):
    out, i = [], 0
    while i < n:
        k = buf[i]
        out.append(tuple(buf[i + 1:i + 1 + k]))
        i += 1 + k
    return out


def port_regions(files, names, points):
    path = WORK / 'regions.zbd'
    path.write_bytes(archive(files))
    keep = [ctypes.c_char_p(k.encode()) for k in names]
    arr = (ctypes.c_char_p * max(len(names), 1))(*keep)
    flat, first, counts = [], [], []
    for pts in points:
        first.append(len(flat) // 3), counts.append(len(pts))
        for p in pts:
            flat += list(p)
    fa = (ctypes.c_float * max(len(flat), 1))(*flat)
    fi = (ctypes.c_int32 * max(len(first), 1))(*first)
    co = (ctypes.c_int32 * max(len(counts), 1))(*counts)
    out = (ctypes.c_uint8 * CAP)()
    log = (ctypes.c_uint32 * LOGCAP)()
    nlog = ctypes.c_int()
    r = probe.probe_regions_run(str(path).encode(), arr, len(names), fa, fi, co, out, CAP, log, LOGCAP,
                                ctypes.byref(nlog))
    if r < 0:
        raise SystemExit(f'probe_regions_run: {r}')
    return bytes(out[:r]), port_events(log, nlog.value)


def first_diff(a, b):
    for i in range(min(len(a), len(b))):
        if a[i] != b[i]:
            return i
    return min(len(a), len(b))


def positions(data):
    """The node positions of a net or region file, in file order."""
    try:
        root, _ = zrd.node(data, 0)
        top = root[0]
    except Exception:
        return []
    out = []
    for i in range(0, len(top) - 1, 2):
        v = top[i + 1]
        if isinstance(top[i], str) and top[i].startswith('node_') and isinstance(v, list) and len(v) > 1 \
                and isinstance(v[1], list) and len(v[1]) == 3 and all(isinstance(x, (int, float)) for x in v[1]):
            out.append(tuple(float(x) for x in v[1]))
    return out


def points_for(data, rng, n=24):
    """Corners, middles of sides, the centre, and points in and round the box."""
    ps = positions(data)
    if not ps:
        return [(0.0, 0.0, 0.0)]
    out = list(ps)
    for a, b in zip(ps, ps[1:] + ps[:1]):
        out.append(tuple((x + y) / 2 for x, y in zip(a, b)))
    cx = sum(p[0] for p in ps) / len(ps)
    cz = sum(p[2] for p in ps) / len(ps)
    out.append((cx, 0.0, cz))
    lo = [min(p[k] for p in ps) for k in range(3)]
    hi = [max(p[k] for p in ps) for k in range(3)]
    for _ in range(n):
        out.append(tuple(rng.uniform(lo[k] - 0.25 * (hi[k] - lo[k]) - 1, hi[k] + 0.25 * (hi[k] - lo[k]) + 1)
                         for k in range(3)))
    out.append(out[-1])
    return out


class Checker:
    def __init__(self):
        self.emu = Emulated()
        self.emu.cover(COVER)
        self.cases = self.fails = 0
        self.found = 0

    def regions(self, label, files, names, points):
        self.cases += 1
        try:
            mine, mlog = RegionsRun(self.emu).run(files, names, points)
        except Exception as e:  # the original crashed
            if label in CRASHES:  # a FIX (bug) case: the port must read it without crashing
                port_regions(files, names, points)
            else:
                self.fails += 1
                print(f'CRASH {label}: {e}')
            reached = self.emu.reached
            self.emu = Emulated()
            self.emu.cover(COVER)
            self.emu.reached = reached
            return
        port, plog = port_regions(files, names, points)
        if mine != port or mlog != plog:
            self.fails += 1
            if self.fails <= 10:
                i = first_diff(mine, port)
                print(f'MISMATCH {label}: stream {len(mine)} vs {len(port)} bytes, first difference at {i}; '
                      f'logs {len(mlog)} vs {len(plog)}')
                print('  original', mine[max(0, i - 16):i + 32].hex())
                print('  port    ', port[max(0, i - 16):i + 32].hex())
                if mlog != plog:
                    j = first_diff(mlog, plog)
                    print('  log original', mlog[j:j + 4], 'port', plog[j:j + 4])
        self.count(mine, len(names))
        return mine

    def count(self, b, n):
        """Tallies the regions made and their edges, from the stream."""
        p = 0
        for _ in range(n):
            found, = struct.unpack_from('<I', b, p)
            p += 4
            if not found:
                continue
            _, ln = struct.unpack_from('<II', b, p)
            p += 8 + ln
            edges, _ = struct.unpack_from('<II', b, p)
            p += 8 + edges * 24
            self.found += 1
            self.edges = getattr(self, 'edges', 0) + edges


def shipped(check, rng):
    n = 0
    for path, files in mission_archives():
        regs = sorted(k[:-4] for k in files if k.lower().startswith('reg_') and k.lower().endswith('.zrd'))
        nets = sorted(k[:-4] for k in files if k.lower().startswith('net_') and k.lower().endswith('.zrd'))
        if not regs:
            continue
        names = regs + regs + nets + ['reg_no_such']
        pts = [points_for(files.get(k + '.zrd', b''), rng) for k in names]
        check.regions(f'{path.parent.name}/{path.name}', files, names, pts)
        n += len(regs)
    return n


# --- made-up files -------------------------------------------------------------
def node(pos=(1.0, 2.0, 3.0), links=(1,), props=()):
    return [12, list(pos), list(links)] + list(props)


def reg_file(name='reg_t', version=(108,), nodes=()):
    top = []
    if version is not None:
        top += ['version', list(version)]
    if name is not None:
        top += ['name', [name]]
    for k, v in nodes:
        top += [k, v]
    return ser([top])


def ring(n, r=50.0, cx=10.0, cz=-20.0, y=3.0, start=0, step=1, jitter=0.0, rng=None):
    import math
    out = []
    for i in range(n):
        a = 2 * math.pi * i / n * (1 if step > 0 else -1)
        j = rng.uniform(-jitter, jitter) if rng else 0.0
        out.append((f'node_{start + i:02d}', node((cx + (r + j) * math.cos(a), y, cz + (r + j) * math.sin(a)),
                                                  [start + (i + 1) % n])))
    return out


def variants(rng):
    out = []
    sq = [('node_00', node((0.0, 0.0, 0.0), [1])), ('node_01', node((10.0, 1.0, 0.0), [2])),
          ('node_02', node((10.0, 2.0, 10.0), [3])), ('node_03', node((0.0, 3.0, 10.0), [0]))]
    out.append(('square', {'reg_t.zrd': reg_file(nodes=sq)}, ['reg_t']))
    out.append(('square reversed', {'reg_t.zrd': reg_file(nodes=[
        ('node_00', node((0.0, 0.0, 0.0), [3])), ('node_01', node((10.0, 1.0, 0.0), [0])),
        ('node_02', node((10.0, 2.0, 10.0), [1])), ('node_03', node((0.0, 3.0, 10.0), [2]))])}, ['reg_t']))
    for v in [None, (105,), (105, 'mech3'), (106,), (107,), (109,)]:
        out.append((f'version {v}', {'reg_t.zrd': reg_file(version=v, nodes=sq)}, ['reg_t']))
    out.append(('name mismatch', {'reg_t.zrd': reg_file(name='reg_other', nodes=sq)}, ['reg_t', 'reg_t']))
    out.append(('name case', {'reg_t.zrd': reg_file(name='REG_T', nodes=sq)}, ['reg_t']))
    out.append(('lookup case', {'reg_t.zrd': reg_file(nodes=sq)}, ['reg_t', 'REG_T']))
    out.append(('no name', {'reg_t.zrd': reg_file(name=None, nodes=sq)}, ['reg_t']))
    out.append(('one node', {'reg_t.zrd': reg_file(nodes=[('node_00', node(links=[0]))])}, ['reg_t']))
    out.append(('two nodes', {'reg_t.zrd': reg_file(nodes=[('node_00', node((0.0, 0.0, 0.0), [1])),
                                                           ('node_01', node((5.0, 0.0, 0.0), [0]))])}, ['reg_t']))
    out.append(('self loop', {'reg_t.zrd': reg_file(nodes=[('node_00', node((0.0, 0.0, 0.0), [0])),
                                                           ('node_01', node((5.0, 0.0, 0.0), [0]))])}, ['reg_t']))
    out.append(('open chain', {'reg_t.zrd': reg_file(nodes=sq[:3] + [('node_03', node((0.0, 3.0, 10.0), []))])},
                ['reg_t']))
    out.append(('inner loop', {'reg_t.zrd': reg_file(nodes=sq[:3] + [('node_03', node((0.0, 3.0, 10.0), [1]))])},
                ['reg_t']))
    out.append(('to nothing', {'reg_t.zrd': reg_file(nodes=sq[:3] + [('node_03', node((0.0, 3.0, 10.0), [9]))])},
                ['reg_t']))
    out.append(('to a hole', {'reg_t.zrd': reg_file(nodes=sq[:3] + [('node_03', node((0.0, 3.0, 10.0), [5])),
                                                                    ('node_06', node(links=[0]))])}, ['reg_t']))
    out.append(('negative link', {'reg_t.zrd': reg_file(nodes=sq[:3] + [('node_03', node((0.0, 3.0, 10.0), [-1]))])},
                ['reg_t']))
    out.append(('second links', {'reg_t.zrd': reg_file(nodes=[
        ('node_00', node((0.0, 0.0, 0.0), [1, 3])), ('node_01', node((10.0, 1.0, 0.0), [2, 0])),
        ('node_02', node((10.0, 2.0, 10.0), [3, 1])), ('node_03', node((0.0, 3.0, 10.0), [0, 2]))])}, ['reg_t']))
    # The start skips the slots without a node, with its shrinking budget.
    for gap in (1, 2, 3, 5):
        out.append((f'hole at start {gap}', {'reg_t.zrd': reg_file(nodes=ring(4, start=gap))}, ['reg_t']))
    out.append(('holes in the middle', {'reg_t.zrd': reg_file(nodes=[
        ('node_00', node((0.0, 0.0, 0.0), [4])), ('node_04', node((10.0, 1.0, 0.0), [8])),
        ('node_08', node((10.0, 2.0, 10.0), [0]))])}, ['reg_t']))
    out.append(('duplicate', {'reg_t.zrd': reg_file(nodes=sq + [('node_01', node((99.0, 0.0, 99.0), [0]))])}, ['reg_t']))
    out.append(('zero side', {'reg_t.zrd': reg_file(nodes=[
        ('node_00', node((0.0, 0.0, 0.0), [1])), ('node_01', node((0.0, 5.0, 0.0), [2])),
        ('node_02', node((10.0, 2.0, 10.0), [3])), ('node_03', node((0.0, 3.0, 10.0), [0]))])}, ['reg_t']))
    out.append(('list-first node', {'reg_t.zrd': reg_file(nodes=sq[:3] + [('node_03', [[0]])])}, ['reg_t']))
    out.append(('arrangenodes', {'reg_t.zrd': reg_file(nodes=[
        ('node_00', node((0.0, 0.0, 0.0), [1, 2, 3], ['arrangenodes', [25.0]])),
        ('node_01', node((10.0, 1.0, 3.0), [2])), ('node_02', node((-4.0, 2.0, 10.0), [3])),
        ('node_03', node((2.0, 3.0, -7.0), [1]))])}, ['reg_t']))
    out.append(('arrange self', {'reg_t.zrd': reg_file(nodes=[
        ('node_00', node((0.0, 0.0, 0.0), [1, 0], ['arrangenodes', [7.5]])),
        ('node_01', node((10.0, 1.0, 3.0), [2, 3], ['arrangenodes', [3.0]])),
        ('node_02', node((-4.0, 2.0, 10.0), [0])), ('node_03', node((2.0, 3.0, -7.0), [9]))])}, ['reg_t']))
    # A centre off the loop spreads the loop's nodes round it, at odd angles.
    out.append(('arranged loop', {'reg_t.zrd': reg_file(nodes=[
        ('node_00', node((40.0, 0.0, 3.0), [1])), ('node_01', node((-7.0, 1.0, 30.0), [2])),
        ('node_02', node((-9.0, 2.0, -33.0), [0])),
        ('node_03', node((1.5, 0.5, 2.5), [0, 2, 1], ['arrangenodes', [25.3]]))])}, ['reg_t']))
    out.append(('many regions', {f'reg_{k}.zrd': reg_file(name=f'reg_{k}', nodes=ring(3 + k, r=10.0 + k))
                                 for k in range(6)}, [f'reg_{k}' for k in range(6)] + ['reg_2', 'reg_0']))
    for i in range(60):
        n = rng.randrange(3, 12)
        nodes = ring(n, r=rng.uniform(5, 500), cx=rng.uniform(-3000, 3000), cz=rng.uniform(-3000, 3000),
                     y=rng.uniform(-50, 50), jitter=rng.uniform(0, 3), rng=rng)
        if rng.random() < 0.3:
            k = rng.randrange(n)
            name, v = nodes[k]
            v[2] = [rng.randrange(-2, n + 2)] + v[2]
        out.append((f'random {i}', {'reg_t.zrd': reg_file(nodes=nodes)}, ['reg_t']))
    return out


def main():
    rng = random.Random(1999)
    WORK.mkdir(parents=True, exist_ok=True)
    check = Checker()
    n = shipped(check, rng)
    print(f'shipped: {n} region files in {check.cases} archives, {check.found} lookups gave a region '
          f'({check.edges} edges)')
    vs = variants(rng)
    for label, files, names in vs:
        pts = [points_for(files.get(k + '.zrd', b''), rng, 12) for k in names]
        check.regions(label, files, names, pts)
    print(f'variants: {len(vs)}')
    missed = sorted(set(COVER) - check.emu.reached)
    for a in missed:
        print(f'NOT REACHED {a:#x}: {COVER[a]}')
    if check.fails or missed:
        sys.exit(f'ai regions: {check.fails} of {check.cases} cases differ')
    print(f'ai regions: {check.cases} cases identical')


if __name__ == '__main__':
    main()
