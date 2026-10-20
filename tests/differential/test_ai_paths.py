"""How the AI finds its way, against the original x86 (docs/ai.md section
10): the nearest step to a point (0x4d51d0), and the way finder (0x4c7c10 /
0x4c7cb0 over 0x4c7d00: the search 0x4c7f70 / 0x4c7e80 / 0x4c7fa0 / 0x4c8140
with the net's mark 0x4c0e70, the way kept 0x4c8430 / 0x4c83c0 / 0x4c8330 /
0x4c8270) with the steps' links (0x4c9e40 / 0x4c9e00, 0x4c9460 / 0x4c93d0)
and their tests of the pilot (valves 0x4ca260, jumps 0x4cb2c0, gates
0x4cd5d0, valve unions 0x4d4600).

    cmake --build build && .venv/bin/python tests/differential/test_ai_paths.py

Every mission's nets built (as test_ai_nets.py), then in each: the nearest
step to points round its nodes, and ways between random pairs of its nodes
for pilots standing near the start, with and without jump jets, named for
a gate or not, under random valve settings, each way then followed step by
step. Per-frame code: the x87 at 24 bits. Both sides' output streams
(tests/probe_ai_net.cpp: steps as slots, ways as lists of slots with their
lengths) must match.
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools' / 'analysis'))
from ai_world import ROOT, Emulated, PathsRun, archive, mission_archives, ser  # noqa: E402
import zrd  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
probe.probe_paths_run.argtypes = [ctypes.c_char_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p, ctypes.c_int,
                                  ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p,
                                  ctypes.c_int, ctypes.c_void_p]
WORK = ROOT / 'build' / 'ai_paths'
CAP, LOGCAP = 1 << 24, 1 << 20
COVER = {0x4c7ef8: 'pilot at the target', 0x4c7f3f: 'search', 0x4c7fbb: 'target reached',
         0x4c80af: 'way measured', 0x4ca277: 'valve open', 0x4cb2f7: 'jump jets', 0x4cd625: 'gate opens',
         0x4c8312: 'way still open', 0x4c7cd9: 'searched again', 0x4d5231: 'nearer step'}


def f2i(x):
    return struct.unpack('<i', struct.pack('<f', x))[0]


def port_events(buf, n):
    out, i = [], 0
    while i < n:
        k = buf[i]
        out.append(tuple(buf[i + 1:i + 1 + k]))
        i += 1 + k
    return out


def port_paths(files, nets, queries, names):
    path = WORK / 'paths.zbd'
    path.write_bytes(archive(files))
    arr = (ctypes.c_int32 * max(len(nets), 1))(*nets)
    flat = [w for q in queries for w in q]
    qa = (ctypes.c_int32 * max(len(flat), 1))(*flat)
    keep = [ctypes.c_char_p(k) for k in names]
    na = (ctypes.c_char_p * max(len(names), 1))(*keep)
    out = (ctypes.c_uint8 * CAP)()
    log = (ctypes.c_uint32 * LOGCAP)()
    nlog = ctypes.c_int()
    r = probe.probe_paths_run(str(path).encode(), arr, len(nets), qa, len(queries), na, len(names), out, CAP, log,
                              LOGCAP, ctypes.byref(nlog))
    if r < 0:
        raise SystemExit(f'probe_paths_run: {r}')
    return bytes(out[:r]), port_events(log, nlog.value)


def first_diff(a, b):
    for i in range(min(len(a), len(b))):
        if a[i] != b[i]:
            return i
    return min(len(a), len(b))


def walk(v, out):
    if isinstance(v, list):
        for x in v:
            walk(x, out)
    else:
        out.append(v)


def net_info(data):
    """(positions of the positioned nodes in number order, gate names, valve names)."""
    try:
        top = zrd.node(data, 0)[0][0]
    except Exception:
        return [], [], []
    recs, gates, valves = {}, set(), set()
    for i in range(0, len(top) - 1, 2):
        k, v = top[i], top[i + 1]
        if not (isinstance(k, str) and k.startswith('node_') and isinstance(v, list) and v):
            continue
        try:
            idx = int(k[5:])
        except ValueError:
            continue
        if isinstance(v[0], int) and len(v) > 1 and isinstance(v[1], list) and len(v[1]) == 3:
            recs.setdefault(idx, tuple(float(x) for x in v[1]))
        rest = v[3:] if isinstance(v[0], int) else v[1:]
        for j in range(0, len(rest) - 1, 2):
            key, val = rest[j], rest[j + 1]
            flat = []
            walk(val, flat)
            names = [x for x in flat if isinstance(x, str)]
            if key == 'gate':
                gates.update(names)
            elif key in ('valve', 'valveunion', 'valve_assign', 'deploy', 'mcbwait'):
                valves.update(names)
    return [recs[i] for i in sorted(recs)], sorted(gates), sorted(valves)


# The search is a depth-first walk over every way without loops: in the big
# nets with many crossings it runs for minutes in the emulator (which also
# never frees the way's list cells), so ways are asked only in nets this small.
MAX_PATH_NODES = 30


def queries_for(files, rng, paths=6, near=6):
    nets, qs, names = [], [], [b'nobody']
    for n in range(1, 100):
        data = files.get(f'net_{n:02d}.zrd')
        if data is None:
            continue
        nets.append(n)
        pos, gates, valves = net_info(data)
        for g in gates:
            if g.encode() not in names:
                names.append(g.encode())
        for v in valves:
            if v.encode() not in names:
                names.append(v.encode())
        if not pos:
            continue
        for _ in range(near):
            p = rng.choice(pos)
            pt = [p[k] + rng.uniform(-60, 60) for k in range(3)]
            qs.append([1, n, f2i(pt[0]), f2i(pt[1]), f2i(pt[2]), 0, 0, 0, 0, 0, 0, 0])
        for _ in range(paths if len(pos) <= MAX_PATH_NODES else 0):
            for v in valves:
                if rng.random() < 0.5:
                    qs.append([3, n, names.index(v.encode()), rng.choice([0, 0, 1, 1, 2]), 0, 0, 0, 0, 0, 0, 0, 0])
            a, b = rng.randrange(len(pos)), rng.randrange(len(pos))
            p = pos[a] if rng.random() < 0.8 else pos[b]
            stand = [p[k] + rng.uniform(-8, 8) for k in range(3)]
            name = rng.randrange(len(names)) if rng.random() < 0.5 else 0
            # Half the time a valve changes once the way is found: the way may
            # shut, and the finder must look again.
            flip, value = 0, 0
            if valves and rng.random() < 0.5:
                flip, value = names.index(rng.choice(valves).encode()), rng.choice([0, 1, 2])
            qs.append([2, n, a, b, rng.choice([0, 1]), name, f2i(stand[0]), f2i(stand[1]), f2i(stand[2]),
                       rng.randrange(1, 8), flip, value])
    return nets, qs, names


class Checker:
    def __init__(self):
        self.emu = self.fresh()
        self.cases = self.fails = 0
        self.queries = 0

    @staticmethod
    def fresh():
        e = Emulated()
        e.cover(COVER)
        e.guard = 0xff   # as test_ai_nets.py
        return e

    def paths(self, label, files, nets, qs, names):
        self.cases += 1
        self.queries += len(qs)
        try:
            mine, mlog = PathsRun(self.emu).run_paths(files, nets, qs, names)
        except Exception as e:  # the original crashed
            self.fails += 1
            print(f'CRASH {label}: {e}')
            reached = self.emu.reached
            self.emu = self.fresh()
            self.emu.reached = reached
            return
        port, plog = port_paths(files, nets, qs, names)
        if mine != port or mlog != plog:
            self.fails += 1
            if self.fails <= 10:
                i = first_diff(mine, port)
                print(f'MISMATCH {label}: stream {len(mine)} vs {len(port)} bytes, first difference at {i}; '
                      f'logs {len(mlog)} vs {len(plog)}')
                print('  original', mine[max(0, i - 16):i + 32].hex())
                print('  port    ', port[max(0, i - 16):i + 32].hex())


def node(pos=(1.0, 2.0, 3.0), links=(1,), props=()):
    return [12, list(pos), list(links)] + list(props)


def net_file(name='net_01', nodes=()):
    top = ['version', [106], 'name', [name]]
    for k, v in nodes:
        top += [k, v]
    return ser([top])


def variants():
    """Small nets where the way is known: a ladder with a jump, a gate and a
    valve switch on the two sides."""
    out = []
    ladder = [('node_00', node((0.0, 0.0, 0.0), [1, 2])),
              ('node_01', node((50.0, 0.0, 0.0), [3], ['jump', [1, 30.0]])),
              ('node_02', node((0.0, 0.0, 80.0), [3], ['gate', ['tank_7']])),
              ('node_03', node((50.0, 0.0, 80.0), [4], ['valve', [1, 'v_ladder']])),
              ('node_04', node((100.0, 0.0, 80.0), [5, 0])),
              ('node_05', node((150.0, 0.0, 80.0), [0])),
              ('node_06', [[0, 2], 'valveunion', [[1, 'u_a', 1], [1, 'u_b', 2]]])]
    out.append(('ladder', {'net_05.zrd': net_file('net_05', ladder)}))
    ring = [(f'node_{i:02d}', node((float(30 * i), 0.0, float(7 * (i % 3))), [(i + 1) % 9, (i + 4) % 9]))
            for i in range(9)]
    out.append(('ring', {'net_07.zrd': net_file('net_07', ring)}))
    return out


def main():
    rng = random.Random(1999_10)
    WORK.mkdir(parents=True, exist_ok=True)
    check = Checker()
    n = 0
    for path, files in mission_archives():
        nets, qs, names = queries_for(files, rng)
        if not nets:
            continue
        check.paths(f'{path.parent.name}/{path.name}', files, nets, qs, names)
        n += 1
    print(f'shipped: {n} archives, {check.queries} queries')
    for label, files in variants():
        for k in range(12):
            nets, qs, names = queries_for(files, rng, paths=10, near=4)
            names += [b'tank_7', b'u_a', b'u_b', b'v_ladder']
            check.paths(f'{label} {k}', files, nets, qs, names)
    # A fork: the short way shut by its valve once found, so the finder must
    # look again and take the long one.
    fork = [('node_00', node((0.0, 0.0, 0.0), [1, 2])), ('node_01', node((10.0, 0.0, 0.0), [3], ['valve', [1, 'v_fork', 1]])),
            ('node_02', node((0.0, 0.0, 50.0), [3])), ('node_03', node((10.0, 0.0, 10.0), []))]
    names = [b'nobody', b'v_fork']
    for shut in (0, 1, 2):
        qs = [[3, 9, 1, 1] + [0] * 8,
              [2, 9, 0, 3, 0, 0, f2i(-1.0), 0, f2i(1.0), 5, 1, shut]]
        check.paths(f'fork {shut}', {'net_09.zrd': net_file('net_09', fork)}, [9], qs, names)
    missed = sorted(set(COVER) - check.emu.reached)
    for a in missed:
        print(f'NOT REACHED {a:#x}: {COVER[a]}')
    if check.fails or missed:
        sys.exit(f'ai paths: {check.fails} of {check.cases} cases differ')
    print(f'ai paths: {check.cases} cases identical ({check.queries} queries)')


if __name__ == '__main__':
    main()
