"""The outside cameras' wall pull-in, 0x0040b310 with the nearest hit
0x004a3290 (docs/view.md), against the port, the x87 at 24 bits.

The camera's place, the look point and the look's direction are random; the
two segment queries (0x004efcd0, tested on the worlds by test_di.py) are
answered alike on both sides from a script: per query and record up to four
crossings, each with a point, a node or none, and a material (none, soil 0,
or soil 1: water, which the nearest hit drops). The target's node switches
(0x004eb6d0), the segments asked for and the camera's final place must match.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import load_probe  # noqa: E402
from original import Original  # noqa: E402

VIEW, WORLD_OBJ = 0x5f4c8c, 0x608c84

probe = load_probe()
probe.probe_pull_in.argtypes = [ctypes.c_void_p] * 6 + [ctypes.c_int32]
probe.probe_pull_in.restype = ctypes.c_int32


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


def bits(x):
    return struct.unpack('<i', struct.pack('<f', x))[0]


class Emu:
    def __init__(self):
        o = self.o = Original()
        o.fpu_control(0x007f)
        a = o.alloc
        self.view, self.mech, self.world = a(0x80), a(0x10), a(0x10)
        self.pos, self.at, self.back = a(12), a(12), a(12)
        self.soil0, self.soil1 = a(0x2c), a(0x2c)
        o.write(self.soil0, bytes(0x2c))
        o.write(self.soil1, bytes(0x20) + struct.pack('<I', 1) + bytes(8))
        o.u32(VIEW, self.view)
        o.u32(self.view + 0x64, self.mech)
        o.u32(self.mech + 4, 0x4444)
        o.u32(WORLD_OBJ, self.world)
        o.u32(self.world + 4, 0x7070)
        o.stub_function(0x4eb6d0, lambda c: self.log.extend([1, c.ecx, c.edx]))
        o.stub_function(0x4efcd0, self.segments, 8)

    def segments(self, c):
        assert c.ecx == 0x7070
        self.log.extend([2, c.arg(0)] + list(struct.unpack('<12i', c.read(c.edx, 48))))
        out = c.arg(1)
        for r in range(2):
            s = self.script[(self.query * 2 + r) * 21:(self.query * 2 + r + 1) * 21]
            rec = bytearray(0x504)
            rec[0] = s[0]
            for k in range(s[0]):
                e = s[1 + 5 * k:6 + 5 * k]
                o = 4 + 0x28 * k
                rec[o + 0xc:o + 0x18] = struct.pack('<3i', *e[:3])
                rec[o + 0x20:o + 0x24] = struct.pack('<I', {0: 0, 1: self.soil0, 2: self.soil1}[e[4]])
                rec[o + 0x24:o + 0x28] = struct.pack('<I', 0x5000 + k if e[3] else 0)
            c.write(out + 0x504 * r, bytes(rec))
        self.query += 1
        return 0

    def run(self, pos, at, back, script):
        o = self.o
        self.log, self.script, self.query = [], script, 0
        o.write(self.pos, struct.pack('<3f', *pos))
        o.write(self.at, struct.pack('<3f', *at))
        o.write(self.back, struct.pack('<3f', *back))
        o.call(0x40b310, ecx=self.pos, edx=self.at, stack_args=(self.back,))
        return list(struct.unpack('<3i', o.read(self.pos, 12))), self.log


def port(pos, at, back, script):
    out = (ctypes.c_float * 3)()
    log = (ctypes.c_int32 * 256)()
    n = probe.probe_pull_in((ctypes.c_float * 3)(*pos), (ctypes.c_float * 3)(*at), (ctypes.c_float * 3)(*back),
                            (ctypes.c_int32 * 84)(*script), out, log, 256)
    return [bits(v) for v in out], list(log[:n])


def scenario(rng):
    at = [f32(rng.uniform(-200, 200)) for _ in range(3)]
    back = [f32(rng.uniform(-1, 1)) for _ in range(3)]
    if rng.random() < 0.1:
        back[0] = back[2] = 0.0  # straight up or down: flattened across is (0, 0, 0)
    d = rng.uniform(2, 30)
    pos = [f32(at[k] - back[k] * d) for k in range(3)]
    script = []
    empty = rng.random() < 0.15  # nothing crossed: the camera stays
    for _ in range(4):
        n = 0 if empty else rng.choice((0, 0, 1, 1, 2, 3, 4))
        rec = [n]
        for k in range(4):
            p = [bits(f32(at[j] + rng.uniform(-20, 20))) for j in range(3)]
            rec += p + [int(rng.random() < 0.75), rng.choice((0, 0, 1, 2))] if k < n else [0] * 5
        script += rec
    return pos, at, back, script


def main():
    n = int(os.environ.get('PULL_IN_CASES', '3000'))
    rng = random.Random(0x40b310)
    emu = Emu()
    failed = moved = water = 0
    for case in range(n):
        pos, at, back, script = scenario(rng)
        want = emu.run(pos, at, back, script)
        got = port(pos, at, back, script)
        if want != got:
            failed += 1
            if failed <= 5:
                print(f'FAIL case {case}: pos {want[0]} vs {got[0]}\n  log want {want[1]}\n  log got  {got[1]}')
        moved += want[0] != [bits(v) for v in pos]
        water += any(script[r * 21 + 1 + 5 * k + 4] == 2 for r in range(4) for k in range(script[r * 21]))
    print(f'ok: {n} pull-ins identical ({moved} moved the camera, {water} with water crossings)' if not failed
          else f'{failed} of {n} differ')
    sys.stdout.flush()
    os._exit(1 if failed else 0)


main()
