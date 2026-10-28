"""The support scripts' render setters against the original x86 (docs/interp.md,
The live calls): SetBFETolerance 0x53cef0 and its getter 0x53cf00 (0x5ef73c),
SetInverseZTolerance 0x526df0 (0x74a284), SetVertexShading 0x526a00
(0x749808), VideoSetDither 0x579240 (0x802700), each from the data's values
with random arguments and the shipped scripts' own. The tolerances
(0x538070 / 0x538080) are in test_gmod_add.py, on the adds they steer.

    cmake --build build && .venv/bin/python tests/differential/test_script_setters.py

BREAK=1 moves the port's backface, BREAK=2 its getter's answer; each must fail.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_weiler import Emulated, probe  # noqa: E402

probe.probe_script_setter.argtypes = [ctypes.c_int, ctypes.c_float, ctypes.c_int, ctypes.POINTER(ctypes.c_float)]
probe.probe_script_setter.restype = None
BFE, INVZ, SHADING, DITHER = 0x5ef73c, 0x74a284, 0x749808, 0x802700


def f32(v):
    return struct.unpack('<f', struct.pack('<f', v))[0]


def main():
    rng = random.Random(int(os.environ.get('SEED', '1')))
    brk = int(os.environ.get('BREAK', '0'))
    emu = Emulated()
    cpu = emu.cpu
    data_bfe = cpu.read(BFE, 4)
    fslot = cpu.alloc(16)
    fstp = cpu.code(b'\xd9\x1d' + struct.pack('<I', fslot) + b'\xc3')  # fstp dword [fslot]; ret
    runs = fails = 0
    for k in range(400):
        op = k % 4
        f = rng.choice((0.00005, 0.01, 0.0, -1.5, rng.uniform(-1, 1)))
        i = rng.choice((0, 1, 2, -1))
        emu.begin()
        cpu.write(BFE, data_bfe)
        cpu.write(INVZ, struct.pack('<f', 0.02))  # 0x526510's, with a device
        cpu.write(SHADING, b'\0' * 4)
        cpu.write(DITHER, b'\0' * 4)
        fbits = struct.unpack('<I', struct.pack('<f', f))[0]
        got = 0.0
        if op == 0:
            emu.call(0x53cef0, stack=(fbits,))
            emu.call(0x53cf00)
            cpu.call(fstp)  # its st(0), stored
            got = struct.unpack('<f', cpu.read(fslot, 4))[0]
        elif op == 1:
            emu.call(0x526df0, stack=(fbits,))
        elif op == 2:
            emu.call(0x526a00, ecx=i & 0xffffffff)
        else:
            emu.call(0x579240, ecx=i & 0xffffffff)
        theirs = [struct.unpack('<f', cpu.read(BFE, 4))[0], struct.unpack('<f', cpu.read(INVZ, 4))[0],
                  struct.unpack('<i', cpu.read(DITHER if op == 3 else SHADING, 4))[0], got]
        out = (ctypes.c_float * 4)()
        probe.probe_script_setter(op, f, i, out)
        mine = [out[0] + (0.001 if brk == 1 else 0), out[1], struct.unpack('<i', struct.pack('<f', out[2]))[0],
                out[3] + (0.5 if brk == 2 and op == 0 else 0)]
        mine[0] = f32(mine[0])
        runs += 1
        if mine != theirs:
            fails += 1
            if fails <= 3:
                print(f'FAIL op {op} f {f} i {i}: port {mine}, original {theirs}')
    if fails:
        print(f'{fails} of {runs} runs differ')
        sys.stdout.flush(); os._exit(1)
    print(f'ok: {runs} runs identical (BFE and its getter, inverse z, vertex shading, dither)')
    sys.stdout.flush(); os._exit(0)


if __name__ == '__main__':
    main()
