"""The missions' nav waynets built, against the original x86 (docs/vehicles.md,
The nav waynet): every shipped version-107 file (waynet_N, waynet_iaN,
waynet_mpN in each reader archive) through the builder (CZAiNetEdit 0x4bcba0
with the build flag) into a waynet (0x4be690: the waynet 0x479640, the slot
count 0x4beb00 / 0x4bec10 / 0x4beb90, the points 0x47a320 with their nav
lines), then made-up files: properties, edge nodes, flags, no nodes; and
files linking to nodes that are not there, run on the port alone (the exe
reads its slot map out of range there: FIX (bug)).

    cmake --build build && .venv/bin/python tests/differential/test_nav_waynet.py

Both sides write the waynet (its id, label and elements: a null step, or a
point's flags, nav line, first link, place, name and label) and log zerr,
localize and zSound's finds; the streams and logs must match.
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from unicorn.x86_const import UC_X86_REG_EIP  # noqa: E402
from ai_world import IAT, ROOT, Emulated, Out, archive, ser  # noqa: E402
from test_ai_nets import first_diff, port_events  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
probe.probe_waynets_run.argtypes = [ctypes.c_char_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p, ctypes.c_int,
                                    ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p]
WORK = ROOT / 'build' / 'nav_waynet'
CAP, LOGCAP = 1 << 22, 1 << 18
NULL_STEP, POINT_VT = 0x603fa0, 0x5c3828
COVER = {0x4be6ea: 'a waynet made', 0x4be9c8: 'a point', 0x4bec85: 'a positioned node counted',
         0x4bed2b: 'an edge node starting at it', 0x47a441: 'a nav line looked up'}


def emu_run(e, files, names):
    e.reset(files)
    o = Out()
    for i, name in enumerate(names):
        cpu = e.cpu
        edit = e.alloc(0x44)
        p = e.new_string(name.encode())
        cpu.call(0x4bcba0, ecx=edit, stack_args=(10000 + i, p, 1), count=2_000_000_000)
        w = e.u32(edit + 0x34)
        cpu.call(0x4bd020, ecx=edit, count=200_000_000)
        if not w or e.u32(w) != 0x5c37b0:
            o.u(0)
            continue
        o.u(e.u32(w + 4))
        lab = e.u32(w + 0x1c)
        o.s(e.cstr(lab) if lab else None)
        b, end = e.u32(w + 0xc), e.u32(w + 0x10)
        els = [e.u32(q) for q in range(b, end, 4)] if b else []
        o.u(len(els))
        for q in els:
            if e.u32(q) != POINT_VT:
                o.u(0xfffffffe if q == NULL_STEP else 0xfffffffd)
                continue
            o.u(POINT_VT)
            for off in range(0xc, 0x11):
                o.u(e.u8(q + off))
            o.u(e.u32(q + 0x14)), o.u(e.u32(q + 0x30)), o.u(e.u8(q + 0x34))
            for k in range(3):
                o.u(e.u32(q + 0x1c + 4 * k))
            for off in (0x28, 0x2c):
                s = e.u32(q + off)
                o.s(e.cstr(s) if s else None)
    return bytes(o.b), e.events


def port_run(files, names):
    WORK.mkdir(parents=True, exist_ok=True)
    path = WORK / 'waynets.zbd'
    path.write_bytes(archive(files))
    arr = (ctypes.c_char_p * max(len(names), 1))(*[n.encode() for n in names])
    out = (ctypes.c_uint8 * CAP)()
    log = (ctypes.c_uint32 * LOGCAP)()
    nlog = ctypes.c_int()
    r = probe.probe_waynets_run(str(path).encode(), arr, len(names), out, CAP, log, LOGCAP, ctypes.byref(nlog))
    if r < 0:
        raise SystemExit(f'probe_waynets_run: {r}')
    return bytes(out[:r]), port_events(log, nlog.value)


class Checker:
    def __init__(self):
        self.e = Emulated()
        self.e.cover(COVER)
        # MSVCRT's _strdup(NULL) is NULL (the waynet's name from a file without one).
        e = self.e
        e.cpu.stub_import(IAT['strdup'], lambda c: e.new_string(e.cstr(c.arg(0))) if c.arg(0) else 0)
        self.cases = self.fails = 0
        self.points = 0
        self.crashes = {}

    def fresh(self):
        reached = self.e.reached
        self.__init__emu()
        self.e.reached = reached

    def __init__emu(self):
        self.e = Emulated()
        self.e.cover(COVER)
        e = self.e
        e.cpu.stub_import(IAT['strdup'], lambda c: e.new_string(e.cstr(c.arg(0))) if c.arg(0) else 0)

    def check(self, label, files, names):
        self.cases += 1
        try:
            mine, mlog = emu_run(self.e, files, names)
        except Exception as ex:  # the original crashed: a FIX (bug) case, the port must not
            eip = self.e.cpu.uc.reg_read(UC_X86_REG_EIP)
            self.crashes.setdefault(eip, 0)
            self.crashes[eip] += 1
            port_run(files, names)
            self.fresh()
            return
        port, plog = port_run(files, names)
        if mine != port or mlog != plog:
            self.fails += 1
            if self.fails <= 8:
                i = first_diff(mine, port)
                print(f'MISMATCH {label}: stream {len(mine)} vs {len(port)} bytes, first difference at {i}; '
                      f'logs {len(mlog)} vs {len(plog)}')
                print('  original', mine[max(0, i - 16):i + 32].hex())
                print('  port    ', port[max(0, i - 16):i + 32].hex())
                if mlog != plog:
                    j = first_diff(mlog, plog)
                    print('  log original', mlog[j:j + 4], 'port', plog[j:j + 4])
        self.points += mine.count(struct.pack('<I', POINT_VT))


def node(pos=(1.0, 2.0, 3.0), links=(), props=(), edge=False):
    head = [list(links)] if edge else [12, list(pos), list(links)]
    return head + [x for p in props for x in p]


def waynet_file(name, nodes, version=107, with_name=True):
    top = ['version', [version]] + (['name', [name]] if with_name else [])
    for k, v in nodes:
        top += [k, v]
    return ser([top])


FLAGS = [['hidden', [1]], ['fixed', [1]], ['autonext', [0]], ['visible', [0]], ['auxname', ['Able']],
         ['auxname', ['Baker']], ['name', ['MSG_WPT_ABLE']], ['speed', [1, 0.5]], ['wait_time', [1, 3.0]],
         ['jump', [1, 12.0]], ['gate', ['g1']]]


def made_up(rng, broken=False):
    names, files = [], {}
    for k in range(rng.randrange(1, 4)):
        name = f'waynet_{k + 1}'
        n = rng.randrange(0, 6)
        nodes = []
        for i in range(n):
            lo, hi = (-1, n + 2) if broken else (0, max(n, 1))
            links = [rng.randrange(lo, hi) for _ in range(rng.randrange(0, 4))]
            props = rng.sample(FLAGS, rng.randrange(0, 3))
            pos = [round(rng.uniform(-500, 500), 1), round(rng.uniform(0, 30), 1), round(rng.uniform(-500, 500), 1)]
            nodes.append((f'node_{i:02d}', node(pos, links, props, rng.random() < 0.15)))
        files[f'{name}.zrd'] = waynet_file(name, nodes, rng.choice([107, 107, 107, 106]), rng.random() < 0.9)
        names.append(name)
    return files, names


def all_archives():
    """Every mission's reader archives, reader.zbd included: (path, {name: bytes})."""
    import zrd
    from ai_world import ZBD
    out = []
    for p in sorted(ZBD.glob('*/reader*.zbd')):
        data = p.read_bytes()
        out.append((p, {n: data[s:s + k] for n, s, k in zrd.entries(data)}))
    return out


def main():
    c = Checker()
    shipped = 0
    for path, files in all_archives():
        names = sorted(k[:-4] for k in files if k.lower().startswith('waynet') and k.lower().endswith('.zrd'))
        if names:
            c.check(f'{path.parent.name}/{path.name}', files, names)
            shipped += len(names)
    rng = random.Random(1)
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 300
    for k in range(runs):
        files, names = made_up(rng)
        c.check(f'made-up {k}', files, names)
    # Links to nodes that are not there: the exe reads its slot map at -1 or
    # past its end (whatever the heap holds there; FIX (bug)): the port alone,
    # which must not crash.
    for k in range(runs // 3):
        port_run(*made_up(rng, broken=True))
    missed = [v for a, v in COVER.items() if a not in c.e.reached]
    if c.fails or missed:
        raise SystemExit(f'{c.fails} of {c.cases} differ; not reached: {missed}')
    if c.crashes:
        print('original crashed at', {hex(k): v for k, v in c.crashes.items()}, '(the port did not)')
    print(f'ok: {shipped} shipped waynets and {runs} made-up files, {c.points} points, as the original '
          f'({len(COVER)} branches reached)')


if __name__ == '__main__':
    main()
