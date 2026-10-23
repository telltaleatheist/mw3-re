"""A vehicle's points and zone at setup against the original x86
(docs/vehicles.md, The setup's points and zone): 0x49f660 (the controls'
vt+0x18 0x4accd0 through the AI controls' vtable, each of the controls'
points at the motion's collision point off the entity's position) and
0x49f710 (the ground under it from 500 up with the node's flag 8 off, the
pick 0x4abd90, the polygon's zone record into the vehicle's +0x64, its id
the one shared with the view's camera 0x53cd20, else its first, onto the
node 0x4eb810).

    cmake --build build && .venv/bin/python tests/differential/test_veh_setup.py [runs]

Each run: 0..12 points, random collision points and position, 0..6 ground
hits (heights about the vehicle, water among them, random zone records with
0xff now and then), the query failing now and then, the camera's zones
random or empty, zones on or off. The x87 at 24 bits. Compared: the traces,
the 15 points, the vehicle's zone record and the gravity word.
"""
import ctypes
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import CALLBACK, Emu, Replay, Trace, compare, load_probe, rnd  # noqa: E402

AI_VT = 0x5cd880
GRAVITY = 0x609a14
ZONES_ON = 0x5ea1d0


def fb(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


def bf(b):
    return struct.unpack('<f', struct.pack('<I', b))[0]


def zone_record(r):
    n = r.choice([0, 1, 1, 2, 3])
    ids = [r.choice([3, 5, 7, 9, 0xff]) if r.random() < 0.25 else r.choice([3, 5, 7, 9]) for _ in range(3)]
    return bytes([n] + ids)


def run(probe, seed, gravity):
    r = rnd(seed, 'setup')
    t = Trace()
    script = t.script
    e = Emu(t)
    e.heap_stubs()
    e.cpu.fpu_control(0x007f)
    e.w32(GRAVITY, fb(gravity))
    zones_on = int(r.random() < 0.8)
    e.w32(ZONES_ON, zones_on)
    probe.probe_zones_mode(zones_on)
    v, ctl, node = e.world_alloc(0x100), e.world_alloc(0xa70), e.world_alloc(0x40)
    e.cpu.write(v, b'\0' * 0x100)
    e.cpu.write(ctl, b'\0' * 0xa70)
    e.w32(v + 4, node)
    e.w32(v + 0xe4, ctl)
    e.w32(ctl, AI_VT)
    count = r.randrange(13)
    e.w32(ctl + 0x214, count)
    coll = [r.uniform(-8, 8) for _ in range(36)]
    for i, f in enumerate(coll):
        e.w32(ctl + 0x764 + 0x194 + 4 * i, fb(f))
    pos = e.world_alloc(0x10)
    p = [r.uniform(-3000, 3000), r.uniform(-50, 80), r.uniform(-3000, 3000)]
    for i, f in enumerate(p):
        e.w32(pos + 4 * i, fb(f))
    e.stub(0x5a85e0, spec='c')
    script['0x005a85e0'] = pos
    flag = r.choice([0, 1])

    def flag_get(em, key, a, pop):
        em.w32(a[1], flag)
        em.t.handle(key, (a[0],))
        em.t.override(key, flag)
        em._ret(0, pop)
    e.stub(0x4eb660, spec='c,d', fn=flag_get)
    e.stub(0x4eb620, spec='c,d')
    # The ground: the record written where the caller asks and kept at `keep`.
    keep = e.world_alloc(0x504)
    n = r.choice([0, 1, 2, 3, 6])
    mats = [0, e.world_alloc(0x40), e.world_alloc(0x40)]
    e.w32(mats[1] + 0x20, 1)  # water
    e.w32(mats[2] + 0x20, 0)
    rec = bytearray(0x504)
    rec[0] = n
    for i in range(n):
        h = 4 + 0x28 * i
        struct.pack_into('<3f', rec, h + 0xc, p[0], p[1] + r.uniform(-10, 10), p[2])
        rec[h + 0x18:h + 0x1c] = zone_record(r)
        struct.pack_into('<I', rec, h + 0x20, r.choice(mats))
    e.cpu.write(keep, bytes(rec))
    fails = r.random() < 0.15

    def ground(em, key, a, pop):
        em.cpu.write(a[1], bytes(rec))
        em.t.handle(key, (a[2], a[3], a[4]))
        v_ = 1 if fails else 0
        em.t.override(key, v_)
        em._ret(v_, pop)
    e.stub(0x4edab0, spec='c,d,s0,s1,s2', fn=ground)
    e.stub(0x4eb810, spec='c,d', fn=lambda em, k, a, pop: (em.t.handle(k, (a[0], a[1] & 0xff)), em._ret(0, pop)))
    view, cam, data = e.world_alloc(0x10), e.world_alloc(0x40), e.world_alloc(0x200)
    e.w32(0x5f4c8c, view)
    e.w32(view + 4, cam)
    e.w32(cam + 0x38, data)
    e.cpu.write(data + 0x1e4, zone_record(r))
    e.w32(0x608c84, e.world_alloc(0x10))  # the world (its +4 is ECX for the query)
    probe.probe_vb_pts_setup(v, ctl, node, count, (ctypes.c_float * 36)(*[bf(fb(x)) for x in coll]), keep)
    rep = Replay(t, e, e.snapshot())
    probe.probe_app_set_callback(rep.callback())
    e.call(0x49f660, ecx=v)
    probe.probe_vb_pts_run()
    name = f'setup seed {seed} ({count} points, {n} hits{", failing" if fails else ""})'
    if not compare(name, t.log, rep.log):
        return False
    pts = (ctypes.c_float * 45)()
    zones = (ctypes.c_uint8 * 4)()
    probe.probe_vb_pts_image(pts, zones)
    for i in range(45):
        want, got = e.r32(ctl + 0x160 + 4 * i), fb(pts[i])
        if want != got:
            print(f'{name}: point {i // 3}.{"xyz"[i % 3]} original {bf(want)} rebuild {pts[i]}')
            return False
    want = bytes(e.cpu.read(v + 0x64, 4))
    if want != bytes(zones):
        print(f'{name}: zones original {want.hex()} rebuild {bytes(zones).hex()}')
        return False
    if e.r32(ctl + 0x1c) != fb(probe.probe_vb_gravity_x1c()):
        print(f'{name}: +0x1c original {bf(e.r32(ctl + 0x1c))} rebuild {probe.probe_vb_gravity_x1c()}')
        return False
    return True


def main():
    probe = load_probe()
    probe.probe_app_set_callback.argtypes = [CALLBACK]
    probe.probe_vb_pts_setup.argtypes = [ctypes.c_uint32] * 3 + [ctypes.c_int32, ctypes.c_void_p, ctypes.c_uint32]
    probe.probe_vb_pts_image.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    probe.probe_vb_gravity_x1c.restype = ctypes.c_float
    probe.probe_vc_gravity.restype = ctypes.c_float
    probe.probe_zones_mode.argtypes = [ctypes.c_int32]
    probe.probe_vb_hooks(1)
    gravity = probe.probe_vc_gravity()
    args = [a for a in sys.argv[1:] if not a.startswith('-')]
    runs = int(args[0]) if args else 600
    for seed in range(runs):
        if not run(probe, seed, gravity):
            sys.stdout.flush(); os._exit(1)
    print(f'ok: {runs} setups (points, ground, zones) as the original')
    sys.stdout.flush(); os._exit(0)


if __name__ == '__main__':
    main()
