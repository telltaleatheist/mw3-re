"""MakeShadows (0x5487d0; ported for preservation, the engine's entry left null) and the baked shadow builder under
it (0x548650, 0x5484c0, 0x548600, 0x548250) against the original x86, on every shipped world as gamez.zbd has it
("sunlight" and "world1" are in each): the walk over the grid's cells and the world's children (active, intersect
nodes; object3ds and the children of LoDs), each object's matrix on the stack, each model's points through it (with
its morph), and each point's segment toward the light. The segment query (0x4eed90, ported and checked on its
own) is the same stand-in on both sides: a point is shadowed by a hash of its place, so every model's colours, the
segments asked (their order and both ends) and the matrices meet. Compared: every used model's polygon colours
and a running checksum of the segments.

    cmake --build build && python3 tests/differential/test_make_shadows.py
"""
import ctypes
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_gamez as tg  # noqa: E402

probe = tg.probe
probe.probe_make_shadows.argtypes = [ctypes.c_char_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p]
probe.probe_make_shadows.restype = ctypes.c_int
MAKE_SHADOWS, SEGMENT_LIST = 0x5487d0, 0x4eed90
POOL, POOL_SIZE = 0x7402e4, 0x7402e0


def shadowed(frm):
    """The stand-in query's answer, the probe's shadow_test in tests/probe.cpp."""
    x, y, z = frm
    return ((x ^ (y * 31) ^ (z * 131)) & 0xffffffff) % 3 == 0


def mix(h, v):
    return ((h ^ v) * 0x01000193) & 0xffffffff


class Query:
    def __init__(self, cpu):
        self.cpu, self.hash, self.calls = cpu, 0x811c9dc5, 0

    def __call__(self, c):
        out = c.edx
        frm = struct.unpack('<3I', struct.pack('<3f', *(struct.unpack('<f', struct.pack('<I', c.arg(i)))[0]
                                                         for i in range(3))))
        to = [c.arg(3 + i) for i in range(3)]
        for v in (*frm, *to):
            self.hash = mix(self.hash, v)
        self.calls += 1
        hit = shadowed(frm)
        c.write(out, bytes([1 if hit else 0, 0]))
        return 0 if hit else 1


def colours(cpu):
    """Every used model's polygons' colours, in slot order."""
    out = bytearray()
    size = struct.unpack('<i', cpu.read(POOL_SIZE, 4))[0]
    pool = struct.unpack('<I', cpu.read(POOL, 4))[0]
    for i in range(size):
        slot = pool + 0x60 * i
        polys = struct.unpack('<i', cpu.read(slot + 0x10, 4))[0]
        if struct.unpack('<i', cpu.read(slot + 0xc, 4))[0] < 1 or polys <= 0:
            continue
        arr = struct.unpack('<I', cpu.read(slot + 0x34, 4))[0]
        for k in range(polys):
            flags, = struct.unpack('<I', cpu.read(arr + 0x24 * k, 4))
            col, = struct.unpack('<I', cpu.read(arr + 0x24 * k + 0x14, 4))
            out += cpu.read(col, 12 * (flags & 0xff))
    return bytes(out)


def main():
    emu = tg.Emulated()
    cpu = emu.cpu
    buf = ctypes.create_string_buffer(64 << 20)
    total_calls = 0
    current = []  # the world's query; the stub is hooked once
    cpu.stub_function(SEGMENT_LIST, lambda c: current[0](c), pop_bytes=0x18)
    for w in tg.WORLDS:
        path = tg.ZBD / w / 'gamez.zbd'
        read = emu.read_zbd(path.read_bytes(), str(path).encode())
        assert struct.unpack_from('<i', read)[0] == 0, w
        q = Query(cpu)
        current[:] = [q]
        # The model draw's point buffers (zModel's start makes them): the morph's and the transform's.
        for a in (0x74a288, 0x74a28c):
            cpu.u32(a, emu.alloc(12 * 0x10000))
        cpu.call(MAKE_SHADOWS, count=2_000_000_000)
        want = colours(cpu)
        stats = (ctypes.c_uint32 * 2)()
        n = probe.probe_make_shadows(str(path).encode(), buf, len(buf), stats)
        got = buf.raw[:n]
        if (stats[0], stats[1]) != (q.calls, q.hash):
            raise SystemExit(f'{w}: segments {stats[0]} (checksum {stats[1]:#x}) vs {q.calls} ({q.hash:#x})')
        if got != want:
            at = next((i for i in range(min(len(got), len(want))) if got[i] != want[i]), None)
            raise SystemExit(f'{w}: colours {len(got)} vs {len(want)} bytes, first difference at {at}')
        print(f'{w}: {q.calls} segments, {len(want) // 12} colours as the original')
        total_calls += q.calls
    print(f'ok: MakeShadows on {len(tg.WORLDS)} worlds as the original ({total_calls} segments)')


if __name__ == '__main__':
    main()
