"""The hit factors 0x413e60 sets at the mechs' set-up against the original
x86 (docs/damage.md, The hit factors): by the network flag [0x608e45], the
damage scale 0x5e1f68 (the scripts' DamageScale), a leg's hit scale
0x5e1f74 and the limp structure 0x5e1f78. The rest of 0x413e60 (the radar
blips' images, the motions, the cockpit sounds) is stubbed. Also: the port's
values before it equal the exe's data.

    cmake --build build && .venv/bin/python tests/differential/test_hit_globals.py

BREAK=1 swaps the port's flag, BREAK=2 moves its values before; each must fail.
"""
import ctypes
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_weiler import Emulated, probe  # noqa: E402

probe.probe_hit_globals.argtypes = [ctypes.c_int, ctypes.POINTER(ctypes.c_float)]
GLOBALS = (0x5e1f68, 0x5e1f74, 0x5e1f78)
NETWORK = 0x608e45


def main():
    brk = int(os.environ.get('BREAK', '0'))
    emu = Emulated()
    cpu = emu.cpu
    for addr in (0x576140, 0x576100, 0x5a9820, 0x4141a0):
        cpu.stub_function(addr, lambda c: 1)
    cpu.stub_function(0x5abd26, lambda c: 0)  # operator new: none, so no blip images are made
    data = [struct.unpack('<f', cpu.read(a, 4))[0] for a in GLOBALS]
    fails = 0
    # Before 0x413e60: the exe's data, the port's defaults (hit_globals_reset).
    probe.probe_hit_globals.restype = None
    for network in (0, 1):
        emu.begin()
        for a, v in zip(GLOBALS, data):
            cpu.write(a, struct.pack('<f', v))
        cpu.write(NETWORK, bytes([network]))
        emu.call(0x413e60)
        theirs = [struct.unpack('<f', cpu.read(a, 4))[0] for a in GLOBALS]
        out = (ctypes.c_float * 3)()
        probe.probe_hit_globals(network ^ (1 if brk == 1 else 0), out)
        mine = list(out)
        ok = mine == theirs
        fails += not ok
        print(f'{"ok  " if ok else "FAIL"} network {network}: port {mine}, original {theirs}')
    out = (ctypes.c_float * 3)()
    probe.probe_hit_globals(-1, out)
    before = [v + (0.1 if brk == 2 else 0) for v in out]
    ok = before == data
    fails += not ok
    print(f'{"ok  " if ok else "FAIL"} before: port {before}, original {data}')
    if fails:
        print(f'{fails} of 3 differ')
        sys.stdout.flush(); os._exit(1)
    print(f'ok: both identical (the exe\'s data before: {data})')
    sys.stdout.flush(); os._exit(0)


if __name__ == '__main__':
    main()
