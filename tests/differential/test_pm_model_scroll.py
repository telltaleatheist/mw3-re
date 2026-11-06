"""PM's texture scroll against its exe: the step 0x547ab0, MW3's 0x541ad0 on a polygon's first material
and UV list, its count in ten bits. Vehicles' tracks and a turret's effect run on it (node->pm_model).

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_model_scroll.py [cases]

Models of 0 to 6 polygons, textured or not, UVs in random ranges (some NaN), speeds 0, NaN or random on
either axis, the frame the same or not; compared: the result, the header and every UV bit for bit.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

os.environ['MW3_EXE'] = 'pm'
sys.path.insert(0, str(Path(__file__).parent))
from calltrace import Emu, Trace  # noqa: E402
import pm_floors  # noqa: E402
import test_pm_zar  # noqa: E402
from test_model_fx import f2u, speed, uv_value  # noqa: E402

FRAME, DT = 0x72f338, 0x72c52c  # MW3's: the emulator takes them to PM's
I32 = ctypes.c_int32
probe = test_pm_zar.load_pm_probe()
probe.probe_pm_scroll_tick.argtypes = [ctypes.POINTER(ctypes.c_uint8), I32, ctypes.POINTER(I32), ctypes.POINTER(I32),
                                       ctypes.POINTER(ctypes.c_float), ctypes.c_uint32, ctypes.c_float, I32, I32]
probe.probe_pm_scroll_tick.restype = I32


def main():
    cases = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    for cw in (0x027f, 0x007f):  # 53 bits, the reference's, and 24
        print(f'fpu {cw:#x}:', end=' ')
        run(cases, cw)


def run(cases, cw):
    e = Emu(Trace())
    e.cpu.fpu_control(cw)
    e.heap_stubs()
    moved = 0
    arena = e.world_next
    for seed in range(cases):
        e.world_next = arena  # each case's model starts over: the world memory is 16 MB
        r = random.Random(seed)
        n = r.randrange(0, 7)
        counts = [r.choice([1, 3, 3, 4, 4, 5, 8, 300]) for _ in range(n)]
        textured = [1 if r.random() < 0.8 else 0 for _ in range(n)]
        uvs = []
        for c in counts:
            bu, bv = r.uniform(-3, 3), r.uniform(-3, 3)
            if r.random() < 0.3:
                bu, bv = float(r.randrange(-3, 4)), float(r.randrange(-3, 4))
            for _ in range(c):
                uvs += [uv_value(r, bu), uv_value(r, bv)]
        header = bytearray(r.randbytes(0x64))
        header[0x28:0x2c] = struct.pack('<f', speed(r))
        header[0x2c:0x30] = struct.pack('<f', speed(r))
        header[0x10:0x14] = struct.pack('<I', n)
        frame = r.randrange(1, 1000)
        header[0x30:0x34] = struct.pack('<I', frame if r.random() < 0.15 else frame - 1)
        dt = r.choice([1 / 30, 1 / 60, r.uniform(0, 0.125), 0.0])
        m = e.world_alloc(0x68)
        polys = e.world_alloc(0x28 * max(n, 1))
        header[0x34:0x38] = struct.pack('<I', polys)
        e.cpu.write(m, bytes(header))
        m_uv = e.world_alloc(4 * max(len(uvs), 2))
        if uvs:
            e.cpu.write(m_uv, struct.pack(f'<{len(uvs)}f', *uvs))
        at = m_uv
        for i, c in enumerate(counts):
            p, mat, lists = polys + 0x28 * i, e.world_alloc(0x2c), e.world_alloc(8)
            e.cpu.write(mat + 1, bytes([textured[i] | (r.randrange(0, 0x80) & ~1)]))
            e.w32(lists, mat), e.w32(lists + 4, at)
            e.w32(p, (r.randrange(0, 0x40) << 10) | c)
            e.w32(p + 0x14, lists), e.w32(p + 0x18, lists + 4)
            at += 8 * c
        e.w32(FRAME, frame), e.w32(DT, f2u(dt))
        ra = e.call(0x541ad0, ecx=m) & 0xffffffff  # 0x547ab0
        ha = bytes(e.cpu.read(m, 0x34))
        ua = bytes(e.cpu.read(m_uv, 4 * len(uvs))) if uvs else b''
        hb = (ctypes.c_uint8 * 0x64).from_buffer_copy(bytes(header))
        ub = (ctypes.c_float * max(len(uvs), 1))()
        ub_raw = struct.pack(f'<{len(uvs)}f', *uvs)
        ctypes.memmove(ub, ub_raw, len(ub_raw))
        rb = probe.probe_pm_scroll_tick(hb, n, (I32 * max(n, 1))(*counts), (I32 * max(n, 1))(*textured), ub, frame, dt, 0, int(cw == 0x027f))
        ok = (ra & 0xffffffff) == (rb & 0xffffffff) and ha == bytes(hb)[:0x34] and ua == bytes(ub)[:len(ua)]
        if not ok:
            print('differs at seed', seed, hex(ra), rb, ha == bytes(hb)[:0x34], ua == bytes(ub)[:len(ua)])
            bad = [i for i in range(0, len(ua), 4) if ua[i:i + 4] != bytes(ub)[i:i + 4]][:4]
            print('  dt', dt, 'speeds', struct.unpack_from('<2f', header, 0x28), 'at', bad,
                  [(struct.unpack('<f', ua[i:i + 4])[0], struct.unpack('<f', bytes(ub)[i:i + 4])[0], uvs[i // 4]) for i in bad])
            sys.exit(1)
        moved += ua != ub_raw
    print(f'ok: {cases} steps identical, {moved} moved UVs')
    pm_floors.floors(moved=(moved, cases // 4))


if __name__ == '__main__':
    main()
