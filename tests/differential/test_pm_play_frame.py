"""PM's key frame of a motion (zeff_anim_events.cpp play_frame, 0x51df10; MW3's 0x505710) against PM's exe: random
frames of a place, a rotation and a scale track, linear or cubic, two after one another in an SI script's data (PM's
motions take their frames from the anim file's scripts), played once up to a time inside the first frame or past it
(then on to the next frame, 0x51deb0, checked by its flags) on an object3d. PM normalises the rotation's quaternion (0x53eec0 after 0x53ef90), MW3 does not. Compared: the place,
rotation and scale the node is given (the exe's setters stubbed), the event's bytes after and the time cursor.

    cmake --build build --target pmprobe
    .venv/bin/python -P tests/differential/test_pm_play_frame.py [cases]
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import pm_floors  # noqa: E402
import test_pm_gamez as g  # noqa: E402
import test_pm_zar  # noqa: E402

PLAY, SET_POS, SET_ROT, SET_SCALE = 0x51df10, 0x510300, 0x50ffe0, 0x50fe80
# The rotation's Euler angles (0x53d590) take _CIasin from the import table: st0 = atan2(x, sqrt(1 - x*x)).
IAT_ASIN, ASIN = 0x5de600, bytes.fromhex('d9c0 d8c8 d9e8 dee1 d9fa d9f3 c3')
F = ctypes.POINTER(ctypes.c_float)


def f32(v):
    return struct.unpack('<f', struct.pack('<f', v))[0]


def main():
    cases = int(sys.argv[1]) if len(sys.argv) > 1 else 6000
    p = test_pm_zar.load_pm_probe()
    p.probe_pm_play_frame.argtypes = [ctypes.c_float, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int32, F, F]
    emu = g.Emulated()
    emu.cpu.fpu_control(0x27f)
    got_exe = {}

    def setter(key):
        def fn(c):
            got_exe[key] = struct.unpack('<3f', struct.pack('<3I', c.arg(1), c.arg(2), c.arg(3)))
            return 0
        return fn
    for a, k in ((SET_POS, 'pos'), (SET_ROT, 'rot'), (SET_SCALE, 'scale')):
        emu.native_stub(a, setter(k))
    seq, ev, node, data, script, tp = emu.alloc(0x40), emu.alloc(0x40), emu.alloc(0x100), emu.alloc(0x200), emu.alloc(0x1c), emu.alloc(4)
    emu.w32(node + 0x34, 5)
    emu.w32(IAT_ASIN, emu.cpu.code(ASIN))
    rng = random.Random(0x51df10)
    worst, rots, past = 0.0, 0, 0
    for n in range(cases):
        def frame_bytes(flags, start, end):
            fr = bytearray(0xc + 0x4c * bin(flags).count('1'))
            struct.pack_into('<Iff', fr, 0, flags, start, end)
            for k in range(bin(flags).count('1')):
                base = 0xc + 0x4c * k
                span = end - start + 2
                for i in range(19):  # a cubic's coefficient of t^k scaled by span^-k, as the files' curves are
                    k = (i - 7) % 4 if i >= 7 else 0
                    struct.pack_into('<f', fr, base + 4 * i, f32(rng.uniform(-2, 2) / span ** k))
                q = [rng.uniform(-1, 1) for _ in range(4)]
                if rng.random() < .7:  # a unit base quaternion, as the files' are
                    sq = sum(x * x for x in q) ** .5
                    q = [x / sq for x in q]
                struct.pack_into('<4f', fr, base, *map(f32, q))
            return fr
        flags = rng.choice((1, 2, 4, 3, 6, 7, 2, 2))
        start = f32(rng.uniform(0, 5))
        end = f32(start + rng.uniform(0.1, 10))
        fr = frame_bytes(flags, start, end)
        nxt = frame_bytes(flags if rng.random() < .5 else rng.choice((1, 2, 4, 3, 6, 7)), end, f32(end + rng.uniform(0.1, 5)))
        blob = bytes(fr + nxt)
        sc = bytearray(0x1c)
        struct.pack_into('<i', sc, 0xc, rng.choice((0, 1)))
        struct.pack_into('<i', sc, 0x14, len(blob) if rng.random() < .8 else len(fr))
        e = bytearray(0x40)
        struct.pack_into('<f', e, 0x18, f32(rng.uniform(0, 2)))
        struct.pack_into('<i', e, 0x20, rng.randrange(0, 4))
        seq_time = f32(rng.uniform(start, end) if rng.random() < .7 else rng.uniform(end, end + 6))
        cursor = f32(rng.uniform(0, seq_time))
        # the exe
        emu.cpu._uc.mem_write(seq, b'\0' * 0x40)
        emu.cpu._uc.mem_write(seq + 0x28, struct.pack('<f', seq_time))
        emu.cpu._uc.mem_write(ev, bytes(e))
        emu.cpu._uc.mem_write(data, blob)
        struct.pack_into('<I', sc, 0x18, data)
        emu.cpu._uc.mem_write(script, bytes(sc))
        emu.cpu._uc.mem_write(tp, struct.pack('<f', cursor))
        got_exe.clear()
        emu.call(PLAY, args=[seq, ev, script, node, data, tp])
        want_e, want_t = emu.read(ev, 0x40), struct.unpack('<f', emu.read(tp, 4))[0]
        # the port
        ge = ctypes.create_string_buffer(bytes(e), 0x40)
        t = ctypes.c_float(cursor)
        out = (ctypes.c_float * 9)()
        p.probe_pm_play_frame(seq_time, ge, bytes(sc), blob, 0, ctypes.byref(t), out)
        where = f'case {n} flags {flags} cubic {sc[0xc]} past the end {seq_time > end}'
        if ge.raw != want_e or t.value != want_t:
            raise SystemExit(f'{where}: the event or the time differ ({t.value} against {want_t})')
        port = {'pos': tuple(out[0:3]), 'rot': tuple(out[3:6]), 'scale': tuple(out[6:9])}
        for k, v in got_exe.items():
            for a, b in zip(v, port[k]):
                d = abs(a - b) / max(1.0, abs(a))
                worst = max(worst, d)
                if not d < 1e-5:
                    raise SystemExit(f'{where} {k}: {port[k]}, the exe {v}')
        rots += 'rot' in got_exe
        past += seq_time > end
    pm_floors.floors(cases=(cases, 6000), rotations=(rots, 2000), past_the_end=(past, 1000))
    print(f'ok: {cases} frames, {rots} with a rotation; the worst relative difference {worst:.2g}')


if __name__ == '__main__':
    main()
