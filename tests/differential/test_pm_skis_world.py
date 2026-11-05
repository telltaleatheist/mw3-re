"""MechSkis::Update 0x477f90 on the real world and the real ground query 0x5003a0 against the port's (test_pm_skis.py
has made-up answers, so never the port's call to zcls::ground_query): c1's world at the stop in c1m1
(docs/pm-mech-step.md) and around it. Soils are 0 on both sides, since the materials are each side's own. Compared: the
skis' ground, clearance, vertical speeds, surfaces and slopes, the mech's push and vertical speed, airborne.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_skis_world.py
"""
import ctypes
import math
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import pm_floors  # noqa: E402
import test_pm_di as di  # noqa: E402
import test_pm_gamez as g  # noqa: E402
import test_pm_skis as ts  # noqa: E402
import test_pm_zar  # noqa: E402

WORLD = g.ZBD / 'c1' / 'gamez.zbd'
FLOATS = [0x124 + 4 * k for k in range(7)] + [0x140 + 4 * k for k in range(7)] + [0x15c + 4 * k for k in range(7)] + [
    0x2c8 + 4 * k for k in range(7)]
STOP = (4529.2, 28.0, -4517.0, math.radians(-8.26))


def main():
    p = test_pm_zar.load_pm_probe()
    P = ctypes.c_void_p
    p.probe_skis_camera.argtypes = [ctypes.c_char_p, ctypes.c_int]
    p.probe_skis.argtypes = [ctypes.c_char_p, ctypes.c_char_p, P, ctypes.c_char_p, P, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_char_p, P, P,
                             ctypes.c_int, ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float, P, P]
    if p.probe_pm_draw_load(str(WORLD).encode()) != 0:
        raise SystemExit('probe load failed')
    rng = random.Random(0x477f90)
    emu = make_bench()
    slot, world = emu.world_node()
    emu.w32(emu.world_obj + 4, world)
    p.probe_skis_real_world(slot)
    p.probe_skis_camera(None, 0)
    emu.w32(ts.MODE, 0)
    for k in range(32):
        emu.w32(ts.SOIL_TABLE + 4 * k, 0)
    vt = emu.alloc(0x200)
    emu.wr(vt, emu.raw(0x5dfaac, 0x200))
    emu.w32(vt + 0x64, 0x406ce0)
    total = kicks = high = 0
    places = [STOP] + [(STOP[0] + rng.uniform(-200, 200), 28.0, STOP[2] + rng.uniform(-200, 200), rng.uniform(-3.1, 3.1)) for _ in range(149)]
    dt = ts.f32(1 / 30)
    for x, y, z, yaw in places:
        emu.wr(emu.skis, ts.fresh_skis(rng, False))
        emu.w32(emu.skis, emu.mech)
        for i in range(2):
            emu.w32(emu.skis + 0x3c + 4 * i, emu.wrap[i]), emu.w32(emu.wrap[i] + 4, emu.nodes[i])
            emu.wr(emu.mats[i], struct.pack('<12f', 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0))
        emu.wr(emu.mech, bytes(0x800))
        emu.w32(emu.mech, vt)
        emu.wr(emu.mech + 0x68, struct.pack('<f', 12.0))
        for i in range(2):
            emu.w32(emu.mech + 0x39c + 4 * i, emu.legs[i]), emu.w32(emu.legs[i] + 0x78, emu.feet[i])
            emu.wr(emu.feet[i], bytes(ts.FOOT)), emu.w32(emu.feet[i] + 0x18, emu.feet[i] + 0xf0)
        emu.wr(ts.GRAVITY, struct.pack('<f', 30.0)), emu.wr(ts.CEILING, struct.pack('<f', 150.0))
        for frame in range(4):
            fx, fz = -math.sin(yaw), -math.cos(yaw)
            pend = ts.yaw_matrix(yaw, x, y, z)
            emu.wr(emu.mech + 8, b'\x01')
            emu.wr(emu.mech + 0x6f8, struct.pack('<12f', *pend))
            emu.wr(emu.pos, struct.pack('<3f', ts.f32(x), ts.f32(y), ts.f32(z)))
            emu.wr(emu.mech + 0x24, struct.pack('<3f', ts.f32(fx), 0.0, ts.f32(fz)))
            emu.wr(emu.mech + 0x48, struct.pack('<4f', ts.f32(math.cos(yaw / 2)), 0.0, ts.f32(math.sin(yaw / 2)), 0.0))
            emu.w32(emu.mech + 0x6ac, 0xffffffff)
            emu.w32(emu.mech + 0xc, emu.transform)
            emu.wr(emu.transform, struct.pack('<12f', *pend))
            skis_in, feet_in, mats_in = emu.raw(emu.skis, ts.SKIS), b''.join(emu.raw(f, ts.FOOT) for f in emu.feet), b''.join(emu.raw(m, 48) for m in emu.mats)
            mech_raw = emu.raw(emu.mech, 0x800)
            emu.events = []
            emu.call(ts.UPDATE, ecx=emu.skis, args=[ts.f2u(dt)])
            want_air = emu.cpu._uc.reg_read(ts.UC_X86_REG_EAX) & 0xff
            skis_out, mech_out = emu.raw(emu.skis, ts.SKIS), emu.raw(emu.mech, 0x800)
            img = bytearray(ts.MECH)
            img[8] = mech_raw[8]
            for port, pm in ts.FIELDS:
                img[port:port + 4] = mech_raw[pm:pm + 4]
            ps, pm = ctypes.create_string_buffer(skis_in, ts.SKIS), ctypes.create_string_buffer(bytes(img), ts.MECH)
            pmat = (ctypes.c_char * 96).from_buffer_copy(mats_in)
            ptr = (ctypes.c_char * 48).from_buffer_copy(struct.pack('<12f', *pend))
            after = (ctypes.c_char * 84)()
            pairs = (ctypes.c_uint32 * 2)()
            out, n = (ctypes.c_int32 * 8192)(), ctypes.c_int32(8192)
            air = p.probe_skis(ps, pm, ptr, feet_in, pmat, emu.nodes[0], emu.nodes[1], bytes(ts.REC * 7), after, pairs, 0, ts.f32(y), 30.0, 150.0, dt, out, ctypes.byref(n))
            where = f'at ({x:.1f}, {z:.1f}) frame {frame}'
            for off in FLOATS:
                a, b = struct.unpack_from('<f', skis_out, off)[0], struct.unpack_from('<f', ps.raw, off)[0]
                if a != b and abs(a - b) > 2e-3:
                    raise SystemExit(f'{where}: skis {off:#x}: exe {a} port {b}')
            for port_off, pm_off in ts.FIELDS:
                if port_off in ts.COMPARED:
                    a, b = struct.unpack_from('<f', mech_out, pm_off)[0], struct.unpack_from('<f', pm.raw, port_off)[0]
                    if a != b and abs(a - b) > 2e-3:
                        raise SystemExit(f'{where}: mech {pm_off:#x}: exe {a} port {b}')
            if want_air != (air & 0xff):
                raise SystemExit(f'{where}: airborne exe {want_air} port {air}')
            ground = struct.unpack_from('<7f', skis_out, 0x124)
            total += 1
            kicks += skis_out[5] != 0
            high += max(ground) - min(ground) > 3.5
    p.probe_skis_real_world(-1)
    pm_floors.floors(frames=(total, 600), a_slide_or_kick=(kicks, 10), a_step_over_3_5=(high, 10))
    print(f'ok: {total} frames on c1\'s real ground identical; {kicks} slid or kicked, {high} with a step over 3.5')


def make_bench():
    """test_pm_skis's bench on test_pm_di's: the same Emulated base, its world loaded first."""
    emu = di.Emulated(WORLD)
    ts_init = ts.Emulated.__init__
    # test_pm_skis's __init__ on the loaded bench, with no stub on the query and no second load.
    real_native = emu.native_stub
    emu.native_stub = lambda addr, fn, pop=0: None if addr == ts.QUERY else real_native(addr, fn, pop)
    di_init = di.Emulated.__init__
    di.Emulated.__init__ = lambda self: None
    try:
        emu.__class__ = type('Bench', (ts.Emulated, di.Emulated), {})
        ts_init(emu, random.Random(0))
    finally:
        di.Emulated.__init__ = di_init
    return emu


if __name__ == '__main__':
    try:
        main()
    except SystemExit as ex:
        if isinstance(ex.code, str):
            print(ex.code)
        sys.stdout.flush()
        import os
        os._exit(0 if ex.code in (None, 0) else 1)
    sys.stdout.flush()
    import os
    os._exit(0)
