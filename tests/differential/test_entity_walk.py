"""The entity manager's walk (0x5a8850, gamez/zentity) against the original x86.

    cmake --build build && .venv/bin/python tests/differential/test_entity_walk.py [trials]

The manager at 0x736558 holds n entities; each one's vt+0x34 is a little
routine that asks the budget (0x5a8910), logs (entity, allowed) and, when
allowed and the scenario says so, counts a full update (0x5a8960), as
Mech::Update does. Entities with +0x09 clear are skipped. The log, the walk's
order and the budget after (the updating flag, resume, current, left,
per_frame) must match the port's (tests/probe_entities.cpp).
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import ROOT, Original  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
P, I = ctypes.c_void_p, ctypes.c_int32
probe.probe_entity_walk.argtypes = [I, P, P, P, P]
probe.probe_entity_walk.restype = I

WALK, ALLOWS, SPEND, MANAGER, UPDATING = 0x5a8850, 0x5a8910, 0x5a8960, 0x736558, 0x73657c
MAX = 40


class Emulated:
    def __init__(self):
        cpu = self.cpu = Original()
        self.log_ = []
        log = cpu.code(b'\xc3')
        cpu.stub_function(log, lambda c: self.log_.extend([self.index[c.arg(0)], c.arg(1) & 0xff]), 0)
        mgr = struct.pack('<I', MANAGER)

        def routine(spend):
            # push ecx; mov ecx, MANAGER; call ALLOWS; pop ecx; movzx edx, al; push edx; push ecx;
            # call LOG; add esp, 8; [test dl, dl; jz done; mov ecx, MANAGER; call SPEND]; done: ret 4
            def rel(at, to):
                return struct.pack('<i', to - (at + 5))
            base = cpu.code_next
            code = b'\x51\xb9' + mgr
            code += b'\xe8' + rel(base + len(code), ALLOWS)
            code += b'\x59\x0f\xb6\xd0\x52\x51'
            code += b'\xe8' + rel(base + len(code), log)
            code += b'\x83\xc4\x08'
            if spend:
                code += b'\x84\xd2\x74\x0a\xb9' + mgr
                code += b'\xe8' + rel(base + len(code), SPEND)
            code += b'\xc2\x04\x00'
            return cpu.code(code)
        self.vts = []
        for spend in (0, 1):
            vt = cpu.alloc(0x40)
            cpu.u32(vt + 0x34, routine(spend))
            self.vts.append(vt)
        self.ents = [cpu.alloc(0x10) for _ in range(MAX)]
        self.index = {e: i for i, e in enumerate(self.ents)}
        self.slots = cpu.alloc(4 * MAX)
        for i, e in enumerate(self.ents):
            cpu.u32(self.slots + 4 * i, e)

    def run(self, s):
        cpu, n = self.cpu, s['n']
        for i in range(n):
            cpu.u32(self.ents[i], self.vts[s['spend'][i]])
            cpu.write(self.ents[i] + 9, bytes([s['live'][i]]))
        b = s['budget']
        slot = lambda i: self.slots + 4 * i  # noqa: E731
        cpu.u32(MANAGER + 4, self.slots)
        cpu.u32(MANAGER + 8, slot(n))
        cpu.u32(MANAGER + 0x10, slot(b[1]))
        cpu.u32(MANAGER + 0x14, slot(b[2]))
        cpu.u32(MANAGER + 0x18, b[3])
        cpu.u32(MANAGER + 0x1c, b[4])
        cpu.u32(UPDATING, b[0])
        self.log_ = []
        cpu.call(WALK, ecx=MANAGER, stack_args=(0x3d088889,))
        u = lambda a: struct.unpack('<I', cpu.read(a, 4))[0]  # noqa: E731
        idx = lambda p: (p - self.slots) // 4 if self.slots <= p < slot(n) else -1  # noqa: E731
        out = [u(UPDATING), idx(u(MANAGER + 0x10)), idx(u(MANAGER + 0x14)),
               struct.unpack('<i', cpu.read(MANAGER + 0x18, 4))[0], struct.unpack('<i', cpu.read(MANAGER + 0x1c, 4))[0]]
        return out, self.log_


def port(s):
    n = s['n']
    live = (ctypes.c_uint8 * n)(*s['live'])
    spend = (ctypes.c_uint8 * n)(*s['spend'])
    b = (ctypes.c_int32 * 5)(*s['budget'])
    log = (ctypes.c_int32 * (2 * MAX))()
    k = probe.probe_entity_walk(n, live, spend, b, log)
    return list(b), list(log[:k])


def scenario(rng):
    n = rng.randrange(1, MAX + 1)
    per = rng.choice((0x40, 0x40, 0, -1, -3, 1, 2, 3, 5, rng.randrange(-5, 50)))
    return {
        'n': n, 'live': [int(rng.random() < 0.85) for _ in range(n)], 'spend': [int(rng.random() < 0.8) for _ in range(n)],
        'budget': [int(rng.random() < 0.1), rng.randrange(n), rng.randrange(n), rng.randrange(-4, 70), per],
    }


def main():
    trials = int(sys.argv[1]) if len(sys.argv) > 1 else 5000
    rng = random.Random(0x5a8850)
    e = Emulated()
    bad = 0
    for k in range(trials):
        s = scenario(rng)
        want, got = e.run(s), port(s)
        if want != got:
            bad += 1
            if bad <= 5:
                print(f'mismatch {k}: {s}\n  original {want}\n  port     {got}')
    print(f'{trials - bad}/{trials} walks identical')
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    # Leave without Unicorn's teardown (it can hang at exit): the status
    # SystemExit carries, 1 for any other fault.
    code = 0
    try:
        main()
    except SystemExit as ex:
        if ex.code not in (None, 0):
            print(ex.code if isinstance(ex.code, str) else '', flush=True)
        code = 0 if ex.code in (None, 0) else 1
    except BaseException:
        import traceback
        traceback.print_exc()
        code = 1
    sys.stdout.flush()
    sys.stderr.flush()
    os._exit(code)
