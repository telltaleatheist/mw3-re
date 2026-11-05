"""Pirate's Moon's mission-start listeners (valves_mission_started, src/mech3/
zai_valve.cpp on the pm branch; docs/pm-map.md step 5) against PM's exe 0x4944e0
and 0x4e4b00 in Unicorn: the valve on_mission_start on where the data named it,
bapon (found or made) by the player's Beagle count (the mech's +0x3cc).

    cmake --build build && .venv/bin/python tests/differential/test_pm_ai_start.py [runs]

The exe's two listeners and its set (0x4e40d0) run for real; its find 0x4e41f0
and found-or-made 0x4e4100 answer from a registry here (the port's are checked by
test_ai_valve on MW3).
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import ROOT, Original  # noqa: E402

probe = ctypes.CDLL(str(next((ROOT / 'build').glob('libpmprobe.*'))))
probe.probe_pm_ai_start.argtypes = [ctypes.c_int32] * 5 + [ctypes.c_void_p]
START, BAP, GET, FIND = 0x4944e0, 0x4e4b00, 0x4e4100, 0x4e41f0
PILOT = 0x62c40c


def run_exe(named_start, start, named_bap, bap, player_bap):
    cpu = Original(exe='pm')
    cpu.tr = lambda a, size=1: a
    w32 = lambda a, v: cpu.write(a, struct.pack('<I', v & 0xffffffff))  # noqa: E731
    r32 = lambda a: struct.unpack('<I', cpu.read(a, 4))[0]  # noqa: E731
    P, M, vt = cpu.alloc(0x100), cpu.alloc(0x800), cpu.alloc(0x40)
    w32(PILOT, P)
    w32(P + 0x14, M)
    w32(M + 0x3cc, player_bap)
    get_code, set_code = cpu.code(b'\xc3'), cpu.code(b'\xc3')
    cpu.stub_function(get_code, lambda c: r32(c.ecx + 4), 0)
    cpu.stub_function(set_code, lambda c: w32(c.ecx + 4, c.arg(0)) or 0, 4)
    w32(vt + 4, get_code)
    w32(vt + 8, set_code)
    valves = {}

    def make(name, value=0):
        v, cell = cpu.alloc(0x20), cpu.alloc(0x10)
        w32(v + 4, cell)
        w32(cell, vt)
        w32(cell + 4, value)
        valves[name] = v
        return v

    def name_of(a):
        out = b''
        while cpu.read(a, 1) != b'\0':
            out += cpu.read(a, 1)
            a += 1
        return out
    cpu.stub_function(FIND, lambda c: valves.get(name_of(c.arg(0)), 0), 0)
    cpu.stub_function(GET, lambda c: valves.get(name_of(c.arg(0))) or make(name_of(c.arg(0))), 0)
    if named_start:
        make(b'on_mission_start', start)
    if named_bap:
        make(b'bapon', bap)
    cpu.call(START)
    cpu.call(BAP)

    def value(name):
        return struct.unpack('<i', cpu.read(r32(valves[name] + 4) + 4, 4))[0] if name in valves else -1
    return [value(b'on_mission_start'), value(b'bapon')]


def main():
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 200
    r = random.Random(1999)
    for k in range(runs):
        a = (r.choice([0, 1]), r.choice([0, 1, 5]), r.choice([0, 1]), r.choice([0, 1, 5]), r.choice([0, 1, 2, -1]))
        e = run_exe(*a)
        o = (ctypes.c_int32 * 2)()
        probe.probe_pm_ai_start(*a, o)
        if e != list(o):
            print(f'run {k}: {a}: exe {e} port {list(o)}')
            sys.exit(1)
    print(f'ok: {runs} mission starts: both valves identical')


if __name__ == '__main__':
    main()
