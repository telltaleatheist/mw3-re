"""Ground following on the real mission worlds: the mech's ground probes,
MechSkis::Update (0x4765b0) with its helpers (0x475c80, 0x475de0, 0x4764a0,
0x477820, 0x477940, 0x477620, 0x525840, 0x51e3d0, 0x4020d0), against the
original x86 (docs/movement-ground.md).

    cmake --build build && .venv/bin/python tests/differential/test_skis.py [frames per walk]

Each world is read by the original reader into emulator memory
(test_scenewalk), and the original's ground query 0x4ee2f0 (zclass\\cls_di.c,
not ported) runs on it for real; the port's query answers with the records
the original wrote (and the probes as it left them). A driver walks, turns,
jets and drops mechs (seven probes and the far one-probe mode, blocked
flags, ski widths) across each world. Before every frame the port gets the
original's state; after it, the skis, the mech's vertical speed and push,
the ski node matrices and every call out (the query's probes and top, the
node matrices, the zone) must match bit for bit at 24-bit precision, except
the ski pitch's sine and cosine (the CRT asin under the game's x87 settings
is approximated here, docs/movement.md, Ported and checked).
"""
import ctypes
import math
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import FTOL, IAT_FTOL, ROOT  # noqa: E402
from test_scenewalk import ZBD, Walk, f2u, u2f  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
probe.probe_skis_camera.argtypes = [ctypes.c_char_p, ctypes.c_int]
probe.probe_skis.argtypes = ([ctypes.c_char_p, ctypes.c_char_p, ctypes.c_void_p, ctypes.c_char_p, ctypes.c_void_p,
                              ctypes.c_uint32, ctypes.c_uint32, ctypes.c_char_p, ctypes.c_void_p, ctypes.c_void_p,
                              ctypes.c_int, ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float,
                              ctypes.c_void_p, ctypes.c_void_p])

UPDATE, QUERY, QUERY_RET = 0x4765b0, 0x4ee2f0, 0x476a35
RECORDS, SOIL_TABLE, GRAVITY, CEILING = 0x6061d0, 0x608c00, 0x5e5d34, 0x5e5d44
SKIS, MECH, FOOT = 0x300, 0x6d8, 0x100
# Fields compared: the skis' whole state (not the node wrappers) and the mech's.
SKIS_FIELDS = [0x5] + list(range(0x8, 0x38)) + list(range(0x44, 0x300))
MECH_FIELDS = list(range(0x3c, 0x48)) + list(range(0x5a0, 0x5ac))
TRIG = set(range(0x44, 0x54))   # sin, cos of the ski pitch: compared within 2e-7
SOILS = [0, 2, 3, 4, 5, 1, 10, 9, 6, 8, 3, 2]   # a pattern over the material index (0x47f400 not run)


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


class Ground(Walk):
    """One world, loaded before the stand-ins go in (the reader uses them)."""
    def __init__(self, name, rng=None):
        super().__init__()
        cpu = self.cpu
        cpu.import_code(IAT_FTOL, FTOL)
        a = self.alloc
        if name:
            self.load_world(name)
        else:   # rough made-up ground under each probe: steps, slopes, water, misses
            self.world, self.rng = a(0x100), rng
            self.area = (-500.0, -500.0, 1000.0, 1000.0)
            self.fake_mats = [a(0x40) for _ in range(6)]
            for k, m in enumerate(self.fake_mats):
                cpu.u32(m + 0x20, k)
            cpu.stub_function(QUERY, self.canned, 0xc)
        self.mech, self.skis, self.transform = a(0x700), a(SKIS), a(48)
        self.legs, self.feet = [a(0xd4), a(0xd4)], [a(FOOT), a(FOOT)]
        self.wrap = [a(8), a(8)]
        self.nodes = [a(0x10), a(0x10)]
        self.mats = [a(48), a(48)]
        self.pos = a(12)
        self.world_obj = a(8)
        cpu.u32(0x608c84, self.world_obj)
        cpu.u32(self.world_obj + 4, self.world)
        cpu.stub_function(0x4fc3f0, lambda c: self.mats[self.nodes.index(c.ecx)])
        cpu.stub_function(0x4fc300, self.set_matrix)
        cpu.stub_function(0x5a85e0, lambda c: self.pos)
        # 0x476550 runs: the view [0x5f4c8c] +4 its camera node, +0x38 its data,
        # +0x1e4 the zone list (set per case); zEntity's 0x5a80d0 recorded.
        self.view, self.cam_node, self.cam_data = a(8), a(0x40), a(0x200)
        cpu.u32(self.view + 4, self.cam_node)
        cpu.u32(self.cam_node + 0x38, self.cam_data)
        cpu.stub_function(0x5a80d0, lambda c: self.log(5, struct.unpack('<i', cpu.read(c.arg(0), 4))[0], c.arg(1)), 8)
        cpu.stub_function(0x49bfd0, lambda c: self.fail('the original reported an error'))
        if name:
            cpu.uc.hook_add(UC_HOOK_CODE, self.query_in, begin=QUERY, end=QUERY)
        cpu.uc.hook_add(UC_HOOK_CODE, self.query_out, begin=QUERY_RET, end=QUERY_RET)
        for k in range(33):  # 0x608c00..0x608c80; 0x608c84 is the world
            cpu.u32(SOIL_TABLE + 4 * k, SOILS[k % len(SOILS)])

    def fail(self, why):
        raise SystemExit(why)

    def log(self, *e):
        self.events.append(len(e))
        self.events.extend(v & 0xffffffff for v in e)

    def set_matrix(self, c):
        side = self.nodes.index(c.ecx)
        self.log(3, side)
        for v in struct.unpack('<12I', self.cpu.read(c.edx, 48)):
            self.log(4, v)

    def query_in(self, uc, address, size, data):
        c = self.cpu
        start = (c.edx - (self.skis + 0x7c)) // 12
        self.q_start, self.q_count = start, c.arg(0)
        self.log(1, start, c.arg(0), c.arg(1))
        for k in range(c.arg(0)):
            self.log(2, *struct.unpack('<3I', c.read(c.edx + 12 * k, 12)))

    def canned(self, c):
        self.query_in(None, 0, 0, None)
        rng, out, n = self.rng, c.arg(2), c.arg(0)
        base = self.ground_y
        for k in range(n):
            p = struct.unpack('<3f', c.read(c.edx + 12 * k, 12))
            rec = bytearray(0x504)
            hits = rng.choice([0, 1, 1, 2, 3, 5])
            rec[0] = hits
            for j in range(hits):
                ny = f32(rng.choice([1.0, 0.95, 0.8, 0.6, rng.uniform(0.2, 1.0)]))
                nx = f32(rng.uniform(-1, 1) * math.sqrt(max(0.0, 1 - ny * ny)))
                h = f32(base + rng.choice([0.0, 0.0, 0.3, -0.5, 3.5, -4.0, 8.0, rng.uniform(-6, 6)]) +
                        p[2] * 0.01 * rng.choice([0, 0, 1, 5]))
                mat = rng.choice(self.fake_mats + [0])
                struct.pack_into('<3f', rec, 4 + 0x28 * j, nx, ny, 0.0)
                struct.pack_into('<f', rec, 4 + 0x28 * j + 0x10, h)
                # A zone record as the worlds hold them: a count up to 3, then the ids.
                zr = rng.randrange(1 << 32)
                zone = bytes([zr % 4] + [(0, 1, 2, 3, 5, 8, 0xff)[(zr >> (8 * i)) % 7] for i in (1, 2, 3)])
                struct.pack_into('<4s3I', rec, 4 + 0x28 * j + 0x18, zone, rng.randrange(1 << 16),
                                 mat, rng.choice([0, 0x5000000]))
            c.write(out + 0x504 * k, bytes(rec))
        return 0

    def query_out(self, uc, address, size, data):
        self.records = self.cpu.read(RECORDS, 7 * 0x504)
        self.after = self.cpu.read(self.skis + 0x7c, 7 * 12)

    def load_world(self, name):
        self.load(ZBD / name / 'gamez.zbd')
        self.cpu.u32(0x74a28c, self.alloc(0x200000))
        self.cpu.u32(0x74a288, self.alloc(0x200000))
        self.world = self.camera_world()
        wd = self.u32(self.world + 0x38)
        self.area = (self.f32(wd + 0x34), self.f32(wd + 0x38), self.f32(wd + 0x64) * self.u32(wd + 0x88),
                     self.f32(wd + 0x68) * self.u32(wd + 0x8c))

    def ground_at(self, x, z):
        pb, out = self.alloc(12), self.alloc(0x504)
        self.cpu.write(pb, struct.pack('<3f', x, 0.0, z))
        self.cpu.write(out, bytes(0x504))
        self.cpu.call(QUERY, ecx=self.world, edx=pb, stack_args=(1, f2u(2000.0), out))
        n = self.cpu.read(out, 1)[0]
        hs = [struct.unpack('<f', self.cpu.read(out + 4 + 0x28 * k + 0x10, 4))[0] for k in range(n)]
        return max(hs) if hs else None

    def soils_of(self, records):
        out = {}
        for k in range(7):
            n = records[0x504 * k]
            for j in range(n):
                mat = struct.unpack_from('<I', records, 0x504 * k + 4 + 0x28 * j + 0x20)[0]
                if mat and mat not in out:
                    idx = self.u32(mat + 0x20)
                    out[mat] = self.u32(SOIL_TABLE + 4 * idx)
        return out


def yaw_matrix(a, x, y, z):
    c, s = math.cos(a), math.sin(a)
    return [f32(c), 0.0, f32(-s), 0.0, 1.0, 0.0, f32(s), 0.0, f32(c), f32(x), f32(y), f32(z)]


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
    struct.pack_into('<5f', b, 0x2ec, length, half, f32(rng.choice([0.0, 0.0, half, half * 0.5])),
                     f32(1.0 / half), f32(1.0 / (half * half)))
    return b


def main():
    frames = int(sys.argv[1]) if len(sys.argv) > 1 else 300
    rng = random.Random(1999)
    cam_rng = random.Random(476550)
    total = airborne = water = slides = simple_frames = 0
    worlds = ['c1', 'c2', 'c3', 'c4', 'c4b', 't1', None]
    for wname in worlds:
        emu = Ground(wname, rng)
        x0, z0, w, d = emu.area
        for walk in range(4 if wname else 24):
            cpu = emu.cpu
            elemental = rng.random() < 0.2
            for _ in range(50):
                x, z = x0 + rng.uniform(0.2, 0.8) * w, z0 + rng.uniform(0.2, 0.8) * d
                g = emu.ground_at(x, z) if wname else 0.0
                if g is not None:
                    break
            yaw = rng.uniform(-math.pi, math.pi)
            y = g + rng.choice([0.0, 0.5, 8.0, 40.0])
            cpu.write(emu.skis, fresh_skis(rng, elemental))
            cpu.u32(emu.skis, emu.mech)
            for i in range(2):
                cpu.u32(emu.skis + 0x3c + 4 * i, emu.wrap[i])
                cpu.u32(emu.wrap[i] + 4, emu.nodes[i])
                cpu.write(emu.mats[i], struct.pack('<12f', 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0))
            mech = bytearray(MECH)
            mech[0x8] = 1
            struct.pack_into('<f', mech, 0x68, f32(rng.choice([4.0, 9.5, 12.0, 14.5])))
            cpu.write(emu.mech, mech)
            cpu.u32(emu.mech + 0xc, emu.transform)
            for i in range(2):
                cpu.u32(emu.mech + 0x38c + 4 * i, emu.legs[i])
                cpu.u32(emu.legs[i] + 0x78, emu.feet[i])
                cpu.write(emu.feet[i], bytes(FOOT))
                cpu.u32(emu.feet[i] + 0x18, emu.feet[i] + 0xf0)
            speed, turn, dt = rng.choice([0.0, 5.0, 12.0, 25.0]), 0.0, f32(rng.choice([1 / 30, 1 / 60, 0.1]))
            ceiling = f32(rng.choice([150.0, 150.0, y + 15.0]))
            gravity = f32(rng.choice([30.0, 30.0, 15.0]))
            for k in range(33):  # a soil per material index, drawn per walk (0x47f400 not run)
                cpu.u32(0x608c00 + 4 * k, rng.choice([0, 2, 3, 5, 1, 1, 10, 10, 8]))
            cpu.write(GRAVITY, struct.pack('<f', gravity))
            cpu.write(CEILING, struct.pack('<f', ceiling))
            emu.ground_y = g
            for frame in range(frames):
                if not wname and rng.random() < 0.05:
                    emu.ground_y = f32(y + rng.choice([-2.0, 0.0, 0.0, 1.0, -10.0]))
                # The driver: what the rest of Mech::Update would do around the skis.
                if rng.random() < 0.05:
                    speed = rng.choice([0.0, 5.0, 12.0, 25.0, 40.0])
                if rng.random() < 0.05:
                    turn = rng.choice([0.0, 0.0, 0.8, -0.8])
                if rng.random() < 0.02:
                    dt = f32(rng.choice([1 / 30, 1 / 60, 0.1, 0.02]))
                yaw += turn * dt
                vy = u2f(emu.u32(emu.mech + 0x40))
                push = struct.unpack('<3f', cpu.read(emu.mech + 0x5a0, 12))
                fx, fz = -math.sin(yaw), -math.cos(yaw)
                vx_w = -speed * fx * 0 + speed * fx + push[0]
                vz_w = speed * fz + push[2]
                x, y, z = x + vx_w * dt, y + vy * dt, z + vz_w * dt
                if not (x0 < x < x0 + w and z0 < z < z0 + d):
                    yaw += math.pi
                    x, z = min(max(x, x0 + 1), x0 + w - 1), min(max(z, z0 + 1), z0 + d - 1)
                cpu.write(emu.transform, struct.pack('<12f', *yaw_matrix(yaw, x, y, z)))
                cpu.write(emu.pos, struct.pack('<3f', x, y, z))
                cpu.write(emu.mech + 0x24, struct.pack('<3f', f32(fx), 0.0, f32(fz)))
                cpu.write(emu.mech + 0x3c, struct.pack('<3f', f32(rng.uniform(-1, 1)), vy, f32(-speed)))
                cpu.write(emu.mech + 0x48, struct.pack('<4f', f32(math.cos(yaw / 2)), 0.0, f32(math.sin(yaw / 2)), 0.0))
                push = [f32(p - p * 4.5 * dt) for p in push]
                cpu.write(emu.mech + 0x5a0, struct.pack('<3f', *push))
                jet = rng.random() < 0.03 or (frame % 97 < 12 and walk == 1)
                cpu.write(emu.mech + 0x594, struct.pack('<3f', 0.0, f32((8 * 0.9 + gravity) * dt) if jet else 0.0, 0.0))
                cpu.write(emu.mech + 0x374, bytes(rng.random() < 0.03 for _ in range(4)))
                detail = 0xffffffff if rng.random() < 0.9 else rng.choice([0, 0x10, 0x1c])
                cpu.u32(emu.mech + 0x688, detail)
                cpu.write(emu.mech + 0x8, bytes([rng.random() > 0.02]))
                for i in range(2):
                    cpu.write(emu.feet[i] + 0x48, bytes([rng.random() < 0.01]))
                    fz_local = f32(math.sin(frame * 0.3 + i * math.pi) * 2.0)
                    cpu.write(emu.feet[i] + 0xf0, struct.pack('<3f', 0.0, 0.0, fz_local))
                skis_in = cpu.read(emu.skis, SKIS)
                mech_in = cpu.read(emu.mech, MECH)
                feet_in = b''.join(cpu.read(f, FOOT) for f in emu.feet)
                mats_in = b''.join(cpu.read(m, 48) for m in emu.mats)
                transform = cpu.read(emu.transform, 48)
                emu.events = []
                emu.records = emu.after = None
                # The camera's zones for 0x476550: none, some, all (0xff); the
                # mode off now and then. The exe always has its view. Their own
                # generator, so the walks stay the ones drawn before.
                k = cam_rng.randrange(4)
                cam = bytes([k] + [cam_rng.choice([0, 1, 2, 3, 5, 8, 0xff]) for _ in range(3)])
                mode = 0 if cam_rng.random() < 0.05 else 1
                cpu.u32(0x5f4c8c, emu.view)
                cpu.write(emu.cam_data + 0x1e4, cam)
                cpu.u32(0x5ea1d0, mode)
                probe.probe_skis_camera(cam, mode)
                try:
                    cpu.call(UPDATE, ecx=emu.skis, stack_args=(f2u(dt),), count=500_000_000)
                except Exception:
                    regs = {n: hex(cpu.uc.reg_read(r)) for n, r in (('eip', 26), ('ecx', 22), ('eax', 19), ('edx', 24), ('esi', 29))}
                    raise SystemExit(f'{wname} frame {frame}: the original faulted at {regs}')
                want_air = cpu.uc.reg_read(0x13) & 0xff  # EAX low byte (UC_X86_REG_EAX = 19)
                skis_out, mech_out = cpu.read(emu.skis, SKIS), cpu.read(emu.mech, MECH)
                mats_out = b''.join(cpu.read(m, 48) for m in emu.mats)
                soils = emu.soils_of(emu.records)
                pairs = (ctypes.c_uint32 * (2 * max(1, len(soils))))(*[v for kv in soils.items() for v in kv])
                ps, pm = ctypes.create_string_buffer(skis_in, SKIS), ctypes.create_string_buffer(mech_in, MECH)
                pmat = (ctypes.c_char * 96).from_buffer_copy(mats_in)
                ptr = (ctypes.c_char * 48).from_buffer_copy(transform)
                after = (ctypes.c_char * 84).from_buffer_copy(emu.after)
                out, n = (ctypes.c_int32 * 8192)(), ctypes.c_int32(8192)
                got_air = probe.probe_skis(ps, pm, ptr, feet_in, pmat, emu.nodes[0], emu.nodes[1], emu.records,
                                           after, pairs, len(soils), u2f(struct.unpack('<I', transform[40:44])[0]),
                                           gravity, ceiling, dt, out, ctypes.byref(n))
                glog = [v & 0xffffffff for v in out[:n.value]]
                where = f'{wname} walk {walk} frame {frame}'
                bad = []
                for off in SKIS_FIELDS:
                    a, b = skis_out[off], ps.raw[off]
                    if a != b:
                        o4 = off & ~3
                        if o4 in TRIG:
                            x1 = struct.unpack('<f', skis_out[o4:o4 + 4])[0]
                            x2 = struct.unpack('<f', ps.raw[o4:o4 + 4])[0]
                            if abs(x1 - x2) <= 2e-7:
                                continue
                        bad.append(hex(o4))
                if bad:
                    raise SystemExit(f'{where}: skis differ at {sorted(set(bad))[:10]}')
                for off in MECH_FIELDS:
                    if mech_out[off] != pm.raw[off]:
                        raise SystemExit(f'{where}: mech differs at {off:#x}')
                if bytes(pmat) != mats_out or want_air != got_air:
                    same = all(abs(a - b) <= 2e-7 for a, b in zip(struct.unpack('<24f', bytes(pmat)),
                                                                   struct.unpack('<24f', mats_out)))
                    if not same or want_air != got_air:
                        raise SystemExit(f'{where}: ski matrices or airborne differ ({want_air} {got_air})')
                if emu.events != glog:
                    w1 = [v for v in emu.events]
                    diffs = [i for i, (p, q) in enumerate(zip(w1, glog)) if p != q]
                    # the logged ski matrices hold the same sine and cosine
                    for i in diffs:
                        if abs(u2f(w1[i]) - u2f(glog[i])) > 2e-7 or w1[i - 2] != 2 or w1[i - 1] != 4:
                            raise SystemExit(f'{where}: calls differ at {i}: {w1[max(0, i - 6):i + 4]} vs '
                                             f'{glog[max(0, i - 6):i + 4]}')
                total += 1
                airborne += want_air
                water += any(struct.unpack_from('<i', skis_out, 0x178 + 4 * j)[0] for j in range(7))
                slides += skis_out[5]
                simple_frames += not (detail & 0x10)
                # carry the original's state on
                y = u2f(struct.unpack('<I', transform[40:44])[0])
    if not (airborne and slides and simple_frames):
        raise SystemExit(f'coverage: {airborne} airborne, {slides} slides, {simple_frames} one-probe frames')
    print(f'ok: {total} frames on the {len(worlds) - 1} worlds and on made-up ground identical; {airborne} airborne, {slides} pushed, '
          f'{water} in water, {simple_frames} one-probe frames')


if __name__ == '__main__':
    main()
