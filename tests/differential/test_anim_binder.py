"""The animations' binder 0x4d4b30 (with 0x4d4c80, 0x4d4e50, 0x508f40,
0x50b570, 0x50b5e0) on every shipped world, against the original x86
(docs/ai.md section 18).

    cmake --build build && .venv/bin/python tests/differential/test_anim_binder.py [missions...]

Both sides read the world and run the animations' post-load pass as
test_anim_run.py does; then the registry is built (0x4d4840, the listener
0x4e78d0 stubbed) and the binder runs. The node's entity (0x40eee0) and the
valve by the node's name (0x4d3c40) are answered from one script: every
third node no entity, the valves by name. Compared: the calls out in order,
then each def's callback, record (its list place), owner and valve. Then the
mission's end (0x4d4700) and the binder again, the records reused, compared
the same way. Last a made-up round on the same defs: some with callback
events freed (status 5), their flag flipped, def 0 given another's
sequences; the binder once more.
"""
import ctypes
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_anim_run import COUNT, DEFS, Emu, Port  # noqa: E402
from test_gamez import WORLDS, ZBD, probe  # noqa: E402
from calltrace import Trace, compare, ret_pop  # noqa: E402

I32 = ctypes.c_int32
REG, HEAD, OURS = 0x60b518, 0x60b520, 0x4d4b10
probe.probe_aown_world_dump.argtypes = [ctypes.POINTER(I32), I32]
probe.probe_aown_world_dump.restype = I32
probe.probe_aown_world_def.argtypes = [I32, I32, ctypes.c_uint32]
probe.probe_aown_world_seqs.argtypes = [I32, I32]


def fnv(s):
    h = 0x811c9dc5
    for c in s.encode('latin-1'):
        h = ((h ^ c) * 0x01000193) & 0xffffffff
    return h


def entity(i):
    return 0 if i % 3 == 0 else 0x50000000 + i


def valve(name):
    return 0x60000000 | (fnv(name) & 0xffffff)


SCRIPT = {'0x0040eee0': lambda n, a: entity(a[0]), '0x004d3c40': lambda n, a: valve(a[0])}


class Bound:
    """The original's side: the registry on test_anim_run's emulator."""

    def __init__(self, emu):
        self.emu = emu
        self.log = []
        cpu = emu.cpu
        cpu.stub_function(0x4e78d0, lambda c: c.ecx, ret_pop(0x4e78d0) or 0)
        cpu.call(0x4d4840)

        def node_entity(c):
            i = emu.index(emu.u(c.ecx))
            self.log.append(('0x0040eee0', (i & 0xffffffff,)))
            return entity(i)

        def valve_get(c):
            if c.ecx == 0:
                return 0
            name = emu.cstr(c.ecx).decode('latin-1')
            self.log.append(('0x004d3c40', (name,)))
            return valve(name)
        cpu.stub_function(0x40eee0, node_entity, ret_pop(0x40eee0) or 0)
        cpu.stub_function(0x4d3c40, valve_get, ret_pop(0x4d3c40) or 0)

    def bind(self):
        self.log = []
        self.emu.cpu.call(0x4d4b30, count=2_000_000_000)

    def end(self):
        self.emu.cpu.call(0x4d4700, ecx=REG)

    def dump(self):
        e = self.emu
        head, recs, n = e.u(HEAD), [], e.u(e.u(HEAD))
        while n != head:
            recs.append(e.u(n + 8))
            n = e.u(n)
        count = struct.unpack('<h', e.cpu.read(COUNT, 2))[0]
        base = e.u(DEFS)
        out = []
        for i in range(1, count):
            cb, rec = e.u(base + 0x13c * i + 0x6c), e.u(base + 0x13c * i + 0x70)
            kind = 0 if cb == 0 else 1 if cb == OURS else 2
            if kind == 1:
                out.append((1, recs.index(rec) if rec in recs else -1, e.u(rec), e.u(rec + 4)))
            else:
                out.append((kind, -1, 0, 0))
        return out


def made_up(emu, mission, bound):
    """Defs with callback events made freed (status 5) or without the flag, some
    without events given it, and def 0 (never walked) another's sequences."""
    import random
    r = random.Random(fnv(mission))
    count = struct.unpack('<h', emu.cpu.read(COUNT, 2))[0]
    base = emu.u(DEFS)
    for i in r.sample(bound, min(40, len(bound))):
        d = base + 0x13c * i
        status, flags = emu.cpu.read(d + 0x98, 1)[0], emu.u(d + 0x94)
        k = r.random()
        if k < 0.3:
            status = 5
        elif k < 0.6:
            flags ^= 0x10
        emu.cpu.write(d + 0x98, bytes([status]))
        emu.cpu.u32(d + 0x94, flags)
        probe.probe_aown_world_def(i, status, flags)
    src = next((i for i in range(1, count) if emu.cpu.read(base + 0x13c * i + 0x94, 1)[0] & 0x10
                and emu.cpu.read(base + 0x13c * i + 0x98, 1)[0] != 5 and emu.cpu.read(base + 0x13c * i + 0x108, 1)[0]), 0)
    if src:
        emu.cpu.u32(base + 0xc4, emu.u(base + 0x13c * src + 0xc4))
        emu.cpu.write(base + 0x108, emu.cpu.read(base + 0x13c * src + 0x108, 1))
        emu.cpu.u32(base + 0x94, emu.u(base + 0x94) | 0x10)
        probe.probe_aown_world_seqs(0, src)
        probe.probe_aown_world_def(0, emu.cpu.read(base + 0x98, 1)[0], emu.u(base + 0x94))


def port_dump():
    buf = (I32 * (4 * 4096))()
    n = probe.probe_aown_world_dump(buf, len(buf))
    return [(buf[4 * i], buf[4 * i + 1], buf[4 * i + 2] & 0xffffffff, buf[4 * i + 3] & 0xffffffff) for i in range(n)]


def check(name, a_log, b_log, sa, sb):
    if not compare(name, a_log, b_log):
        sys.exit(1)
    if sa != sb:
        bad = next(i for i, (x, y) in enumerate(zip(sa, sb)) if x != y) if len(sa) == len(sb) else -1
        print(f'{name}: def {bad + 1} differs: original {sa[bad] if bad >= 0 else len(sa)}, '
              f'rebuild {sb[bad] if bad >= 0 else len(sb)}')
        sys.exit(1)


def run(mission, totals):
    world, anim = ZBD / mission / 'gamez.zbd', ZBD / mission / 'anim.zbd'
    emu, port = Emu(), Port()
    emu.open(world, anim)
    port.open(world, anim)
    table = [0.5] * 200
    emu.set_random(table)
    port.set_random(table)
    emu.load()
    port.load()
    a = Bound(emu)
    t = Trace(dict(SCRIPT))
    probe.probe_app_set_callback(t.callback())
    probe.probe_aown_world_reset()
    for rnd in ('bind', 'bind again', 'made-up'):
        if rnd == 'bind again':
            a.end()
            probe.probe_aown_end()
        if rnd == 'made-up':
            made_up(emu, mission, [i + 1 for i, x in enumerate(sa_kept) if x[0] == 1])
        t.reset()
        a.bind()
        probe.probe_aown_world_bind()
        sa = a.dump()
        check(f'{mission} {rnd}', a.log, t.log, sa, port_dump())
        if rnd == 'bind':
            calls = len(a.log)
        if rnd == 'bind again':
            sa_kept = sa
    sa = sa_kept
    bound = [s for s in sa if s[0] == 1]
    totals['defs'] += len(sa)
    totals['bound'] += len(bound)
    totals['with owner'] += sum(1 for s in bound if s[2])
    totals['with valve'] += sum(1 for s in bound if s[3])
    totals['calls'] += calls
    print(f'{mission}: {len(sa)} defs, {len(bound)} bound ({sum(1 for s in bound if s[2])} owners, '
          f'{sum(1 for s in bound if s[3])} valves), {calls} calls, twice identical, made-up round identical', flush=True)


def main():
    missions = sys.argv[1:] or WORLDS
    totals = {'defs': 0, 'bound': 0, 'with owner': 0, 'with valve': 0, 'calls': 0}
    for m in missions:
        run(m, totals)
    print(f'ok: {len(missions)} worlds ({", ".join(f"{v} {k}" for k, v in totals.items())}), as the original')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()
