"""PM's ground probes, MechSkis::Update 0x477f90 with its helpers, against mech3/mech_skis (docs/movement-ground.md,
docs/pm-mech-step.md), on rough made-up ground under each probe: steps, slopes, water, misses, soils. The real ground query is
test_pm_di's. Before each frame the port gets the exe's state; after it the skis, the mech's vertical speed and push, the prediction
and every call out must match, the ski pitch's sine and cosine within 2e-7 as in test_skis.

    cmake --build build --target pmprobe
    .venv/bin/python -P tests/differential/test_pm_skis.py [frames per walk]
"""
import ctypes
import math
import os
import random
import struct
import sys
from pathlib import Path

os.environ.setdefault('MW3_X87', '24')
sys.path.insert(0, str(Path(__file__).parent))
import pm_floors  # noqa: E402
import test_pm_zar  # noqa: E402
from original import FTOL, IAT_FTOL  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP  # noqa: E402

UPDATE, QUERY = 0x477f90, 0x5003a0
SOIL_TABLE, GRAVITY, CEILING, VIEW, MODE, WORLD_OBJ = 0x62ca10, 0x607ec0, 0x607ed0, 0x61874c, 0x60d5e8, 0x62ca90
SKIS, MECH, FOOT, REC = 0x31c, 0x900, 0x100, 0x584
SKIS_FIELDS = [0x5] + list(range(0x8, 0x38)) + list(range(0x44, 0x31c))
TRIG = set(range(0x44, 0x54))
# (the port's image offset, PM's)
FIELDS = [(o, o) for o in (0x24, 0x28, 0x2c, 0x30, 0x34, 0x38, 0x3c, 0x40, 0x44, 0x48, 0x4c, 0x50, 0x54, 0x68)] + [
    (0x594, 0x5b8), (0x598, 0x5bc), (0x59c, 0x5c0), (0x5a0, 0x5c4), (0x5a4, 0x5c8), (0x5a8, 0x5cc), (0x688, 0x6ac), (0x7ec, 0x6ec),
    (0x7f0, 0x6f0), (0x7f4, 0x6f4)]
COMPARED = [0x30, 0x34, 0x38, 0x3c, 0x40, 0x44, 0x5a0, 0x5a4, 0x5a8, 0x7f0]


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


def f2u(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def u2f(x):
    return struct.unpack('<f', struct.pack('<I', x))[0]


class Emulated(test_pm_zar.Emulated):
    def __init__(self, rng):
        self.step = None
        super().__init__()
        a = self.alloc
        self.cpu.import_code(IAT_FTOL, FTOL)
        # _CIasin through its thunk's import slot, on the x87 stack (modeldraw_common's).
        slot = struct.unpack('<I', self.raw(0x5cefd8, 4))[0]
        self.w32(slot, self.cpu.code(b'\xd9\xc0\xd8\xc8\xd9\xe8\xde\xe1\xd9\xfa\xd9\xf3\xc3'))
        self.rng, self.ground_y = rng, 0.0
        self.mech, self.skis = a(0x800), a(SKIS)
        self.transform = a(48)
        self.legs, self.feet = [a(0xd4), a(0xd4)], [a(FOOT), a(FOOT)]
        self.wrap, self.nodes, self.mats = [a(8), a(8)], [a(0x10), a(0x10)], [a(48), a(48)]
        self.pos, self.world, self.world_obj, self.view, self.cam_node, self.cam_data = a(12), a(8), a(8), a(8), a(0x40), a(0x200)
        self.fake_mats = [a(0x40) for _ in range(6)]
        for k, m in enumerate(self.fake_mats):
            self.w32(m + 0x20, k)
        self.w32(WORLD_OBJ, self.world_obj), self.w32(self.world_obj + 4, self.world)
        self.w32(VIEW, self.view), self.w32(self.view + 4, self.cam_node), self.w32(self.cam_node + 0x38, self.cam_data)
        self.native_stub(QUERY, self.canned, 0)
        self.native_stub(0x510630, lambda c: self.mats[self.nodes.index(c.arg(0))], 0)
        self.native_stub(0x510540, self.set_matrix, 0)
        self.native_stub(0x5cb260, lambda c: self.pos, 0)
        self.native_stub(0x5cad40, lambda c: self.log(5, struct.unpack('<i', self.raw(c.arg(0), 4))[0], c.arg(1) & 0xff), 8)
        self.native_stub(0x4fcf70, lambda c: 0, 0)
        self.native_stub(0x4fcf20, lambda c: 0, 0)
        self.cpu.stubs[0x406cb0] = (lambda c: 0, 0)

    def raw(self, a, n):
        return bytes(self.cpu._uc.mem_read(a, n))

    def wr(self, a, b):
        self.cpu._uc.mem_write(a, bytes(b))

    def w32(self, a, v):
        self.wr(a, struct.pack('<I', v & 0xffffffff))

    def log(self, *e):
        self.events.append(len(e))
        self.events.extend(v & 0xffffffff for v in e)

    def set_matrix(self, c):
        self.log(3, self.nodes.index(c.arg(0)))
        for v in struct.unpack('<12I', self.raw(c.arg(1), 48)):
            self.log(4, v)
        return 0

    def canned(self, c):
        probes, n, top, out = c.arg(1), c.arg(2), c.arg(3), c.arg(4)
        start = (probes - (self.skis + 0x7c)) // 12
        self.log(1, start, n, top)
        rng = self.rng
        recs = self.records
        for k in range(n):
            self.log(2, *struct.unpack('<3I', self.raw(probes + 12 * k, 12)))
            p = struct.unpack('<3f', self.raw(probes + 12 * k, 12))
            rec = bytearray(REC)
            if self.step is not None:  # one hit under each probe: a ledge under the two front ones
                high = start + k in (0, 3)
                struct.pack_into('<3f', rec, 4, *((-0.37, 0.93, 0.0) if high else (0.0, 1.0, 0.0)))
                struct.pack_into('<f', rec, 4 + 0x10, f32(self.ground_y + (self.step if high else 0.0)))
                rec[0] = 1
                self.wr(out + REC * k, rec)
                recs[start + k] = bytes(rec)
                continue
            hits = rng.choice([0, 1, 1, 2, 3, 5])
            rec[0] = hits
            for j in range(hits):
                ny = f32(rng.choice([1.0, 0.95, 0.8, 0.6, rng.uniform(0.2, 1.0)]))
                nx = f32(rng.uniform(-1, 1) * math.sqrt(max(0.0, 1 - ny * ny)))
                h = f32(self.ground_y + rng.choice([0.0, 0.0, 0.3, -0.5, 3.5, -4.0, 8.0, rng.uniform(-6, 6)]) + p[2] * 0.01 * rng.choice([0, 0, 1, 5]))
                mat = rng.choice(self.fake_mats + [0])
                struct.pack_into('<3f', rec, 4 + 0x2c * j, nx, ny, 0.0)
                struct.pack_into('<f', rec, 4 + 0x2c * j + 0x10, h)
                zr = rng.randrange(1 << 32)
                zone = bytes([zr % 4] + [(0, 1, 2, 3, 5, 8, 0xff)[(zr >> (8 * i)) % 7] for i in (1, 2, 3)])
                struct.pack_into('<4s3I', rec, 4 + 0x2c * j + 0x18, zone, rng.randrange(1 << 16), mat, rng.choice([0, 0x5000000]))
            self.wr(out + REC * k, rec)
            recs[start + k] = bytes(rec)
        self.after = self.raw(self.skis + 0x7c, 7 * 12)
        return 0


def fresh_skis(rng, elemental):
    b = bytearray(SKIS)
    length = 4.0 if elemental else 12.0
    half = length * 0.5
    hw = f32(rng.choice([1.5, 2.5, 3.0]))
    struct.pack_into('<f', b, 0x78, hw)
    b[0x6c] = b[0x6d] = 1
    for side in range(2):
        sx = -hw if side == 0 else hw
        for k, z in enumerate((-half, 0.0, half)):
            struct.pack_into('<3f', b, 0xd0 + 0xc * (3 * side + k), sx, 0.0, z)
        struct.pack_into('<f', b, 0x4c + 4 * side, 1.0)
    struct.pack_into('<5f', b, 0x308, length, half, f32(rng.choice([0.0, 0.0, half, half * 0.5])), f32(1.0 / half), f32(1.0 / (half * half)))
    return b


def yaw_matrix(a, x, y, z):
    c, s = f32(math.cos(a)), f32(math.sin(a))
    return [c, 0.0, -s, 0.0, 1.0, 0.0, s, 0.0, c, f32(x), f32(y), f32(z)]


def main():
    frames = int(sys.argv[1]) if len(sys.argv) > 1 else 200
    p = test_pm_zar.load_pm_probe()
    P = ctypes.c_void_p
    p.probe_skis_camera.argtypes = [ctypes.c_char_p, ctypes.c_int]
    p.probe_skis.argtypes = [ctypes.c_char_p, ctypes.c_char_p, P, ctypes.c_char_p, P, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_char_p, P, P,
                             ctypes.c_int, ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float, P, P]
    rng = random.Random(1999)
    emu = Emulated(rng)
    total = airborne = slides = simple = kicks = 0
    # The last walks stand at a ledge, the stop in c1m1 (docs/pm-mech-step.md): 3.52 up ahead, level behind, creeping.
    for walk in range(28):
        emu.step = (3.52 if walk % 2 else 3.4) if walk >= 24 else None
        elemental = rng.random() < 0.2 and emu.step is None
        emu.wr(emu.skis, fresh_skis(rng, elemental))
        emu.w32(emu.skis, emu.mech)
        for i in range(2):
            emu.w32(emu.skis + 0x3c + 4 * i, emu.wrap[i]), emu.w32(emu.wrap[i] + 4, emu.nodes[i])
            emu.wr(emu.mats[i], struct.pack('<12f', 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0))
        emu.wr(emu.mech, bytes(0x800))
        emu.wr(emu.mech + 8, b'\x01')
        struct_h = f32(rng.choice([4.0, 9.5, 12.0, 14.5]))
        emu.wr(emu.mech + 0x68, struct.pack('<f', struct_h))
        vt = emu.alloc(0x200)
        emu.wr(vt, emu.raw(0x5dfaac, 0x200))
        emu.w32(emu.mech, vt)
        for i in range(2):
            emu.w32(emu.mech + 0x39c + 4 * i, emu.legs[i]), emu.w32(emu.legs[i] + 0x78, emu.feet[i])
            emu.wr(emu.feet[i], bytes(FOOT)), emu.w32(emu.feet[i] + 0x18, emu.feet[i] + 0xf0)
        x, y, z, yaw = rng.uniform(-100, 100), rng.choice([0.0, 0.5, 8.0, 40.0]), rng.uniform(-100, 100), rng.uniform(-3, 3)
        emu.ground_y = 0.0
        speed, dt = rng.choice([0.0, 5.0, 12.0, 25.0]), f32(rng.choice([1 / 30, 1 / 60, 0.1]))
        gravity, ceiling = f32(rng.choice([30.0, 30.0, 15.0])), f32(rng.choice([150.0, 150.0, y + 15.0]))
        for k in range(32):  # the world pointer follows
            emu.w32(SOIL_TABLE + 4 * k, rng.choice([0, 2, 3, 5, 1, 1, 10, 10, 8, 7]))
        emu.wr(GRAVITY, struct.pack('<f', gravity)), emu.wr(CEILING, struct.pack('<f', ceiling))
        if emu.step is not None:
            y, emu.ground_y, speed, dt = 28.0, 28.0, 1.0, f32(1 / 30)
        for frame in range(frames):
            if rng.random() < 0.05 and emu.step is None:
                emu.ground_y = f32(y + rng.choice([-2.0, 0.0, 0.0, 1.0, -10.0]))
            if rng.random() < 0.05 and emu.step is None:
                speed = rng.choice([0.0, 5.0, 12.0, 25.0, 40.0])
            vy = struct.unpack('<f', emu.raw(emu.mech + 0x40, 4))[0]
            push = struct.unpack('<3f', emu.raw(emu.mech + 0x5c4, 12))
            fx, fz = -math.sin(yaw), -math.cos(yaw)
            x, y, z = x + (speed * fx + push[0]) * dt, y + vy * dt, z + (speed * fz + push[2]) * dt
            pend = yaw_matrix(yaw, x, y, z)
            emu.wr(emu.mech + 0x6f8, struct.pack('<12f', *pend))
            emu.wr(emu.pos, struct.pack('<3f', f32(x), f32(y), f32(z)))
            emu.wr(emu.mech + 0x24, struct.pack('<3f', f32(fx), 0.0, f32(fz)))
            emu.wr(emu.mech + 0x3c, struct.pack('<3f', f32(rng.uniform(-1, 1)), vy, f32(-speed)))
            emu.wr(emu.mech + 0x48, struct.pack('<4f', f32(math.cos(yaw / 2)), 0.0, f32(math.sin(yaw / 2)), 0.0))
            emu.wr(emu.mech + 0x5c4, struct.pack('<3f', *[f32(v - v * 4.5 * dt) for v in push]))
            jet = (rng.random() < 0.03 or (frame % 97 < 12 and walk == 1)) and emu.step is None
            emu.wr(emu.mech + 0x5b8, struct.pack('<3f', 0.0, f32(38 * 0.9 * dt) if jet else 0.0, 0.0))
            emu.wr(emu.mech + 0x384, bytes(rng.random() < 0.03 for _ in range(4)))
            detail = 0xffffffff if rng.random() < 0.9 or emu.step is not None else rng.choice([0, 0x10, 0x1c])
            emu.w32(emu.mech + 0x6ac, detail)
            emu.wr(emu.mech + 8, bytes([rng.random() > 0.02 or emu.step is not None]))
            for i in range(2):
                emu.wr(emu.feet[i] + 0x48, bytes([rng.random() < 0.01 and emu.step is None]))
                emu.wr(emu.feet[i] + 0xf0, struct.pack('<3f', 0.0, 0.0, f32(math.sin(frame * 0.3 + i * math.pi) * 2.0)))
            cam = bytes([rng.randrange(4)] + [rng.choice([0, 1, 2, 3, 5, 8, 0xff]) for _ in range(3)])
            mode = 0 if rng.random() < 0.05 else 1
            emu.wr(emu.cam_data + 0x1e4, cam), emu.w32(MODE, mode)
            p.probe_skis_camera(cam, mode)
            low, remote = rng.random() < 0.1 and emu.step is None, rng.random() < 0.3 and emu.step is None
            emu.wr(0x6293a8, bytes([low]))
            emu.w32(vt + 0x64, 0x402250 if remote else 0x406ce0)
            emu.wr(emu.transform, struct.pack('<12f', *[f32(rng.uniform(-9, 9)) for _ in range(12)]))
            emu.w32(emu.mech + 0xc, emu.transform)
            skis_in, feet_in, mats_in = emu.raw(emu.skis, SKIS), b''.join(emu.raw(f, FOOT) for f in emu.feet), b''.join(emu.raw(m, 48) for m in emu.mats)
            mech_raw = emu.raw(emu.mech, 0x800)
            emu.events, emu.records, emu.after = [], {}, None
            try:
                emu.call(UPDATE, ecx=emu.skis, args=[f2u(dt)])
            except Exception as ex:
                uc = emu.cpu._uc
                raise SystemExit(f'walk {walk} frame {frame}: eip {uc.reg_read(UC_X86_REG_EIP):#x} ret {emu.r32(uc.reg_read(UC_X86_REG_ESP)):#x}: {ex}')
            want_air = emu.cpu._uc.reg_read(UC_X86_REG_EAX) & 0xff
            skis_out, mech_out, mats_out = emu.raw(emu.skis, SKIS), emu.raw(emu.mech, 0x800), b''.join(emu.raw(m, 48) for m in emu.mats)
            hits = b''.join(emu.records.get(k, bytes(REC)) for k in range(7))
            img = bytearray(MECH)
            img[8] = mech_raw[8]
            for port, pm in FIELDS:
                img[port:port + 4] = mech_raw[pm:pm + 4]
            for k in range(4):
                img[0x374 + k] = mech_raw[0x384 + k]
            img[0x7b0], img[0x7b1] = low, remote
            ps, pm = ctypes.create_string_buffer(skis_in, SKIS), ctypes.create_string_buffer(bytes(img), MECH)
            pmat = (ctypes.c_char * 96).from_buffer_copy(mats_in)
            ptr = (ctypes.c_char * 48).from_buffer_copy(struct.pack('<12f', *pend))
            after = (ctypes.c_char * 84).from_buffer_copy(emu.after or bytes(84))
            soils = {}
            for k in range(7):
                r = hits[REC * k:REC * (k + 1)]
                for j in range(r[0]):
                    mat = struct.unpack_from('<I', r, 4 + 0x2c * j + 0x20)[0]
                    if mat:
                        soils[mat] = emu.r32(SOIL_TABLE + 4 * emu.r32(mat + 0x20))
            pairs = (ctypes.c_uint32 * (2 * max(1, len(soils))))(*[v for kv in soils.items() for v in kv] or [0, 0])
            out, n = (ctypes.c_int32 * 8192)(), ctypes.c_int32(8192)
            got_air = p.probe_skis(ps, pm, ptr, feet_in, pmat, emu.nodes[0], emu.nodes[1], hits, after, pairs, len(soils), f32(y), gravity, ceiling, dt, out, ctypes.byref(n))
            glog = [v & 0xffffffff for v in out[:n.value]]
            where = f'walk {walk} frame {frame}'
            if frame == 1 and walk == 0 and os.environ.get('DBG'):
                print('down', skis_out[0x6c], skis_out[0x6d], 'in', skis_in[0x6c], skis_in[0x6d], 'slid', skis_out[5])
                print('ground', struct.unpack_from('<7f', skis_out, 0x124), 'clear', struct.unpack_from('<7f', skis_out, 0x2c8))
                print('push in', struct.unpack('<3f', mech_raw[0x5c4:0x5d0]), 'exe', struct.unpack('<3f', mech_out[0x5c4:0x5d0]), 'port', struct.unpack('<3f', pm.raw[0x5a0:0x5ac]))
                print('forward', struct.unpack('<3f', mech_raw[0x24:0x30]), 'q', struct.unpack('<4f', mech_raw[0x48:0x58]), 'dt', dt, 'soils', soils)
            bad = []
            for off in SKIS_FIELDS:
                if skis_out[off] != ps.raw[off]:
                    o4 = off & ~3
                    if o4 in TRIG and abs(struct.unpack('<f', skis_out[o4:o4 + 4])[0] - struct.unpack('<f', ps.raw[o4:o4 + 4])[0]) <= 2e-7:
                        continue
                    bad.append(hex(o4))
            if bad:
                print(f'{where}: skis differ at {sorted(set(bad))[:10]}')
                for h in sorted(set(bad))[:4]:
                    o = int(h, 16)
                    print(f'  {h}: exe {struct.unpack("<f", skis_out[o:o+4])[0]} port {struct.unpack("<f", ps.raw[o:o+4])[0]}')
                sys.exit(1)
            for port, pmoff in FIELDS:
                if port in COMPARED and mech_out[pmoff:pmoff + 4] != pm.raw[port:port + 4]:
                    print(f'{where}: mech {pmoff:#x}: exe {struct.unpack("<f", mech_out[pmoff:pmoff+4])[0]} port {struct.unpack("<f", pm.raw[port:port+4])[0]}')
                    sys.exit(1)
            if bytes(ptr) != mech_out[0x6f8:0x728]:
                print(f'{where}: prediction: exe {struct.unpack("<12f", mech_out[0x6f8:0x728])[9:]} port {struct.unpack("<12f", bytes(ptr))[9:]}')
                sys.exit(1)
            if want_air != (got_air & 0xff):
                print(f'{where}: airborne exe {want_air} port {got_air}')
                sys.exit(1)
            same = all(abs(a_ - b_) <= 2e-7 for a_, b_ in zip(struct.unpack('<24f', bytes(pmat)), struct.unpack('<24f', mats_out)))
            if not same:
                print(f'{where}: ski matrices differ')
                sys.exit(1)
            if emu.events != glog:
                diffs = [i for i, (q, r) in enumerate(zip(emu.events, glog)) if q != r]
                if len(emu.events) != len(glog) or any(abs(u2f(emu.events[i]) - u2f(glog[i])) > 2e-7 or emu.events[i - 2] != 2 or emu.events[i - 1] != 4 for i in diffs):
                    i = diffs[0] if diffs else min(len(glog), len(emu.events))
                    print(f'{where}: calls differ at {i}: exe {emu.events[max(0, i - 6):i + 4]} port {glog[max(0, i - 6):i + 4]}')
                    sys.exit(1)
            total += 1
            kicks += emu.step is not None and skis_out[5] != 0
            airborne += want_air
            slides += skis_out[5]
            simple += not (detail & 0x10)
    pm_floors.floors(frames=(total, 4800), airborne=(airborne, 600), slides=(slides, 1800), one_probe=(simple, 80), ledge=(kicks, 100))
    print(f'ok: {total} frames; {airborne} airborne, {slides} slides, {simple} one-probe, {kicks} at a ledge')


if __name__ == '__main__':
    code = 0
    try:
        main()
    except SystemExit as ex:
        if isinstance(ex.code, str):
            print(ex.code)
        code = 0 if ex.code in (None, 0) else 1
    except BaseException:
        import traceback
        traceback.print_exc()
        code = 1
    sys.stdout.flush()
    os._exit(code)
