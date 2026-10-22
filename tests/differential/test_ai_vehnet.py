"""A vehicle's own net (docs/ai.md 17.2) against the original x86: vt+0xd8
0x4d70f0 with the net 0x4c0090 (over 0x4bfbb0, znet's 0x58aec0 / 0x58b1c0)
and its seven behaviours (0x4c8600, 0x4d1c80), the getters 0x4c0de0,
0x4c0e10, 0x4c0e40, and its part: make_part 0x4c02f0 -> 0x4c0ba0, run
0x4c0d60, dtor 0x4c0dc0.

    cmake --build build && .venv/bin/python tests/differential/test_ai_vehnet.py

Each case makes the net twice on one vehicle (+0xd0 kept), reads its class,
id, steps' classes and the vehicle (+0x1c), asks the three getters, then
makes its part in an order of a brain with a mode (+0x40, -1..5), a vehicle
or none (+0xf0 through the virtual base +0xe0) and a home point (+0xe4) or
none; runs it (with a vehicle by then: without one the exe reads through
null, the port's FIX waits), destroys it. Compared: all of that, the part's
home point and pending flag, the brain's +0xe4 after each step.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import Emu, Trace, load_probe  # noqa: E402

U = ctypes.c_uint32
VT_NET, VEHNET_CTOR, FOLLOW, ATTACK, OWN = 0x4d70f0, 0x4c0090, 0x4c0de0, 0x4c0e10, 0x4c0e40
MAKE_PART, RUN, DTOR, REGISTRY_INIT, REGISTRY = 0x4c02f0, 0x4c0d60, 0x4c0dc0, 0x4c7b80, 0x603fd8
f2i = lambda x: struct.unpack('<I', struct.pack('<f', x))[0]  # noqa: E731


class Original:
    def __init__(self):
        self.t = Trace({})
        self.e = e = Emu(self.t)
        e.heap_stubs()
        e.call(REGISTRY_INIT, ecx=REGISTRY)
        self.pos = e.world_alloc(0x10)
        e.stub(0x5a85e0, '@0x005a85e0', 'c', None, lambda emu, key, a, pop: emu._ret(self.pos, pop))

    def run(self, mode, at_make, run, prev, pos):
        e = self.e
        out = [0] * 26
        v = e.world_alloc(0x100)
        n = e.call(VT_NET, ecx=v)
        out[0] = int(e.call(VT_NET, ecx=v) == n)
        out[1] = e.r32(n + 4)
        begin = e.r32(n + 0xc)
        steps = [e.r32(begin + 4 * i) for i in range((e.r32(n + 0x10) - begin) // 4)]
        assert len(steps) == 7, steps
        out[2:9] = [e.r32(s) for s in steps]

        def slot(p):
            return steps.index(p) if p in steps else 0xffffffff
        out[9], out[10], out[11] = (slot(e.call(f, ecx=n)) for f in (FOLLOW, ATTACK, OWN))
        out[23] = int(e.r32(n + 0x1c) == v)
        assert e.r32(n) == 0x5caaf0
        # The brain: a virtual base table whose +4 is 0, the vehicle at +0xf0.
        b, vb, part, order, mark = (e.world_alloc(0x200) for _ in range(5))
        e.w32(b + 0xe0, vb)
        e.w32(vb + 4, 0)
        e.w32(b + 0x40, mode & 0xffffffff)
        e.w32(b + 0xe4, mark if prev else 0)
        e.w32(b + 0xf0, v if at_make else 0)
        e.w32(order + 8, b)
        e.cpu.write(self.pos, struct.pack('<3f', *pos))

        def home_code():
            h = e.r32(b + 0xe4)
            return 2 if h == part + 8 else 1 if h == mark else 0
        e.call(MAKE_PART, ecx=n, stack=(part, order))
        out[12] = e.r8(part + 0x20)
        out[13] = slot(e.r32(part + 0x1c))
        out[14:17] = [e.r32(part + 8 + 4 * k) for k in range(3)]
        out[17] = home_code()
        if run:
            e.w32(b + 0xf0, v)
            e.call(RUN, ecx=part, stack=(f2i(0.1),))
            out[18] = e.r8(part + 0x20)
            out[19] = home_code()
            out[20:23] = [e.r32(part + 8 + 4 * k) for k in range(3)]
        e.call(DTOR, ecx=part)
        h = e.r32(b + 0xe4)
        out[24] = 1 if h == mark else 2 if h else 0
        e.call(0x4c02b0, ecx=n, stack=(1,))  # the net deleted: unregistered, its steps freed
        return out


def main():
    probe = load_probe()
    probe.probe_ao_vehnet.argtypes = [ctypes.c_int32] * 4 + [ctypes.c_void_p, ctypes.c_void_p]
    probe.probe_ao_new()
    port = Trace({})
    probe.probe_app_set_callback(port.callback())
    orig = Original()
    cases = int(os.environ.get('VEHNET_CASES', '400'))
    rng = random.Random(7)
    stats = {}
    for k in range(cases):
        mode = rng.choice([-1, 0, 1, 2, 3, 4, 5, 1, 2, 4])
        at_make, prev = int(rng.random() < 0.8), int(rng.random() < 0.5)
        run = int(rng.random() < 0.7)
        pos = [rng.uniform(-900, 900), rng.uniform(0, 60), rng.uniform(-900, 900)]
        a = orig.run(mode, at_make, run, prev, pos)
        out = (U * 26)()
        probe.probe_ao_vehnet(mode, at_make, run, prev, (ctypes.c_float * 3)(*pos), out)
        bb = list(out)
        if a != bb:
            names = ['cached', 'id'] + [f'step{i}' for i in range(7)] + ['follow', 'attack', 'own', 'pending',
                     'behaviour', 'home.x', 'home.y', 'home.z', 'home after make', 'pending after run',
                     'home after run', 'run.x', 'run.y', 'run.z', 'vehicle', 'home after dtor', '-']
            bad = [f'{nm} {x:#x} vs {y:#x}' for nm, x, y in zip(names, a, bb) if x != y]
            raise SystemExit(f'case {k} (mode {mode}, vehicle {at_make}, run {run}, prev {prev}): {bad}')
        key = (a[13] != 0xffffffff, a[12], run, prev)
        stats[key] = stats.get(key, 0) + 1
    probe.probe_ao_done()
    need = [(True, 0, 1, 1), (False, 0, 0, 0), (True, 1, 1, 0), (False, 1, 1, 1)]
    missing = [n for n in need if n not in stats]
    if missing:
        raise SystemExit(f'cases never reached: {missing}')
    print(f'ok: {cases} nets and parts identical (getters follow/attack/own at slots {a[9]}, {a[10]}, {a[11]}); '
          f'{len(stats)} kinds of case')


if __name__ == '__main__':
    code = 0
    try:
        main()
    except SystemExit as e:
        print(e)
        code = 1
    sys.stdout.flush()
    os._exit(code)
