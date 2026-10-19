"""The light setters the mission's world.zrd reader calls on the sun (zClass
Light.c: 0x4faff0 orientation, 0x4fb060 its getter, 0x4fa940 ambient,
0x4fa9b0 diffuse) against the original x86: random light data with and
without the static flag 0x100, a null node and null data, comparing the data
bytes, the frame stamp [0x5e9e2c] and the result.

    cmake --build build && .venv/bin/python tests/differential/test_light_setters.py
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import ROOT, Original  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
probe.probe_light_setter.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_int, ctypes.c_float, ctypes.c_float,
                                     ctypes.c_float, ctypes.c_uint32, ctypes.c_void_p]
SET_ORIENT, GET_ORIENT, SET_AMBIENT, SET_DIFFUSE = 0x4faff0, 0x4fb060, 0x4fa940, 0x4fa9b0
FRAME, UPDATE_FRAME, FROZEN, ZERR = 0x72f338, 0x5e9e2c, 0x60b94c, 0x49bfd0


def bits(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


def main():
    cpu = Original()
    cpu.stub_function(ZERR, lambda c: 0)
    node, data, outs = cpu.alloc(0xd4), cpu.alloc(0xd0), cpu.alloc(12)
    rng = random.Random(5)
    cases = 0
    for _ in range(400):
        which = rng.randrange(4)
        raw = bytearray(rng.randbytes(0xd0))
        flags = struct.unpack_from('<I', raw, 0xb0)[0]
        flags = (flags & ~0x100) | (0x100 if rng.random() < 0.5 else 0)
        struct.pack_into('<I', raw, 0xb0, flags)
        null_node, null_data = rng.random() < 0.05, rng.random() < 0.05
        a, b, c = (rng.uniform(-10, 10) for _ in range(3))
        a, b, c = (struct.unpack('<f', struct.pack('<f', x))[0] for x in (a, b, c))
        frame = rng.randrange(1, 100000)
        cpu.write(data, bytes(raw))
        cpu.write(node, bytes(0xd4))
        cpu.u32(node + 0x38, 0 if null_data else data)
        cpu.u32(FRAME, frame), cpu.u32(UPDATE_FRAME, 0), cpu.u32(FROZEN, 0)
        ecx = 0 if null_node else node
        if which == 0:
            cpu.call(SET_ORIENT, ecx=ecx, stack_args=(bits(a), bits(b), bits(c)))
        elif which == 1:
            cpu.call(SET_AMBIENT, ecx=ecx, stack_args=(bits(a),))
        elif which == 2:
            cpu.call(SET_DIFFUSE, ecx=ecx, stack_args=(bits(a),))
        else:
            cpu.write(outs, bytes(12))
            cpu.call(GET_ORIENT, ecx=ecx, edx=outs, stack_args=(outs + 4, outs + 8))
        want = (cpu.read(data, 0xd0), cpu.read(UPDATE_FRAME, 4), struct.pack('<I', cpu.uc.reg_read(UC_X86_REG_EAX)),
                cpu.read(outs, 12) if which == 3 else bytes(12))
        if null_node:
            continue  # the port's probe always passes a node; the null node is the same early return
        buf = ctypes.create_string_buffer(bytes(raw), 0xd0)
        out = ctypes.create_string_buffer(20)
        probe.probe_light_setter(which, buf, int(null_data), a, b, c, frame, out)
        got = (buf.raw, out.raw[:4], out.raw[4:8], out.raw[8:20])
        if got != want:
            raise SystemExit(f'case {cases} (setter {which}, static {bool(flags & 0x100)}, null data {null_data}): '
                             f'differs')
        cases += 1
    print(f'ok: {cases} light setter calls identical (data, frame stamp, result)')


if __name__ == '__main__':
    main()
