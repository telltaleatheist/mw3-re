"""A vehicle's collisions against the original x86 (docs/vehicles.md, The
collisions): 0x49ff80 with the sweep 0x4a0180, the query 0x4a0910, the
sorting 0x4a09e0, the ceiling response 0x4a0f50 and the lists' free
0x4a0100, the original's own; the world's segment query 0x4efcd0, the
node's flag 0x4eb6d0, a node's object 0x40eee0 and the wall response
0x4a0b40 stubbed (2c4b ports the last).

    cmake --build build && .venv/bin/python tests/differential/test_veh_collide.py [runs]

Each run: a vehicle's speed, slip and turn rate (still, turning, moving
ahead or back, sliding either side), its points this and last frame, its
matrices, the crossings each sweep finds (none to three: objects, ceilings,
walls, floors of plain or water soil), a remote side's collision record
now and then (its frame current or stale). The x87 at 24 bits. Compared:
the traces (the segments' points, each response's crossing), the vertical
speed, the points, the support speeds and the flags.
"""
import ctypes
import math
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import CALLBACK, Emu, Replay, Trace, compare, load_probe, rnd  # noqa: E402
import test_veh_suspension as S  # noqa: E402

FRAME = 0x72f338


def run(probe, seed):
    r = rnd(seed, 'collide')
    t = Trace()
    script = t.script
    e = Emu(t)
    e.heap_stubs()
    e.cpu.fpu_control(0x007f)
    v, ctl, node = e.world_alloc(0x100), e.world_alloc(0xa70), e.world_alloc(0x40)
    e.cpu.write(v, b'\0' * 0x100)
    e.cpu.write(ctl, b'\0' * 0xa70)
    e.w32(v, 0x5c88e8)
    e.w32(v + 4, node)
    e.w32(v + 0xe4, ctl)
    rot = S.attitude(r, False)[0]
    pos = [r.uniform(-2000, 2000), r.uniform(-20, 60), r.uniform(-2000, 2000)]
    m = [S.f32(x) for x in rot + pos]
    last = [S.f32(x + r.uniform(-0.02, 0.02)) for x in rot] + [S.f32(p + r.uniform(-1, 1)) for p in pos]
    kind = r.choice(['still', 'turn', 'ahead', 'back', 'right', 'left'])
    x70 = [0.0, 0.0, 0.0]
    if kind == 'ahead':
        x70[2] = -r.uniform(1, 30)
    elif kind == 'back':
        x70[2] = r.uniform(1, 10)
    elif kind in ('right', 'left'):
        x70 = [r.uniform(1.2, 8) * (1 if kind == 'right' else -1), 0.0, r.uniform(-20, 20)]
    elif kind == 'still':
        x70 = [r.uniform(-0.05, 0.05), 0.0, r.uniform(-0.05, 0.05)]
    spin = r.uniform(-2, 2) if kind in ('turn', 'still') else r.uniform(-0.3, 0.3)
    f = m + last + [S.f32(x) for x in x70] + [S.f32(spin), S.f32(r.uniform(-5, 5))]
    now = [S.f32(r.uniform(-2000, 2000)) for _ in range(45)]
    then = [S.f32(x + r.uniform(-2, 2)) for x in now]
    coll = [S.f32(r.uniform(-4, 4)) for _ in range(36)]
    speeds = [S.f32(r.uniform(-4, 4)) for _ in range(4)]
    cd = [r.choice([0.5, S.f32(r.uniform(0, 1))]), r.choice([0.15, S.f32(r.uniform(0, 0.5))])]
    yaw = S.f32(r.uniform(-3, 3))
    f += now + then + coll + speeds + cd + [yaw]
    u = [12, 4, int(r.random() < 0.2)]
    put = lambda a, x: e.w32(a, S.fb(x))  # noqa: E731
    mat = e.world_alloc(0x30)
    for i in range(12):
        put(mat + 4 * i, m[i])
        put(ctl + 0x278 + 4 * i, last[i])
    e.w32(v + 0xc, mat)
    for i in range(3):
        put(ctl + 0x70 + 4 * i, x70[i])
    put(ctl + 0x54, spin)
    put(ctl + 0x68, f[12 + 12 + 3 + 1])
    for i in range(45):
        put(ctl + 0xac + 4 * i, now[i])
        put(ctl + 0x160 + 4 * i, then[i])
    mb = ctl + 0x764
    for i in range(36):
        put(mb + 0x194 + 4 * i, coll[i])
    for i in range(4):
        put(mb + 0x14c + 4 * i, speeds[i])
    put(mb + 0x280, cd[0])
    put(mb + 0x284, cd[1])
    put(ctl + 0x330, yaw)
    e.w32(ctl + 0x214, u[0])
    e.w32(mb + 0x84, u[1])
    e.w32(ctl + 0x2c, u[2])
    frame = r.randrange(100, 100000)
    e.w32(FRAME, frame)
    net = 0
    if r.random() < 0.15:
        net = e.world_alloc(0x40)
        e.w32(net, frame if r.random() < 0.8 else frame - 1)
        e.w32(net + 4, frame - r.choice([1, 2, 4, 5, 9]))
        e.cpu.write(net + 8, bytes([1, 0, 0, int(r.random() < 0.9)]))
        for i in range(6):
            put(net + 0x24 + 4 * i, S.f32(r.uniform(-1, 1)))
        e.w32(v + 0xd8, net)
    entity = e.world_alloc(0x10)
    for i in range(3):
        put(entity + 4 * i, pos[i])
    e.stub(0x5a85e0, spec='c')
    script['0x005a85e0'] = entity
    e.stub(0x4eb6d0, spec='c,d')
    e.w32(0x608c84, e.world_alloc(0x10))  # the world (its +4 is the query's ECX)
    # The sweeps' crossings: a record per segment, mirrored at `keep`.
    keep = e.world_alloc(0x504 * 15)
    water = e.world_alloc(0x40)
    e.w32(water + 0x20, 1)
    objects = {}
    recs = bytearray(0x504 * 15)
    for i in range(15):
        n = r.choice([0, 0, 0, 1, 1, 2, 3])
        recs[0x504 * i] = n
        for h in range(n):
            o = 0x504 * i + 4 + 0x28 * h
            ny = r.choice([r.uniform(-1, -0.91), r.uniform(-0.89, 0.7), r.uniform(0.72, 1.0)])
            nx = r.uniform(-1, 1) * math.sqrt(max(0.0, 1 - ny * ny))
            nz = math.sqrt(max(0.0, 1 - ny * ny - nx * nx))
            if r.random() < 0.05:  # flat or NaN (an object's crossing may be either)
                nx, ny, nz = r.choice([(0.0, 1.0, 0.0), (float('nan'), 0.5, 0.0), (0.0, -1.0, float('nan'))])
            struct.pack_into('<6f', recs, o, nx, ny, nz, r.uniform(-2000, 2000), r.uniform(-20, 60), r.uniform(-2000, 2000))
            hn = e.world_alloc(0x100)
            e.cpu.write(hn, b'\0' * 0x100)
            if r.random() < 0.2:  # a node with its own handlers, slot 1 set or not
                table = e.world_alloc(0x20)
                e.cpu.write(table, b'\0' * 0x20)
                e.w32(table + 0xc, r.choice([0, e.world_alloc(0x10)]))
                e.w32(hn + 0xbc, table)
            struct.pack_into('<I', recs, o + 0x20, r.choice([0, 0, water]))
            struct.pack_into('<I', recs, o + 0x24, hn)
            if r.random() < 0.15:
                objects[hn] = e.world_alloc(0x10)
    e.cpu.write(keep, bytes(recs))

    def segs(em, key, a, pop):
        points, out = a[2], a[3]
        em.t.handle(key, (points,))
        for i in range(points):
            em.t.handle('0x004efcd0.point', tuple(em.r32(a[1] + 12 * i + 4 * j) for j in range(3)))
        em.cpu.write(out, bytes(recs[:0x504 * (points // 2)]))
        em._ret(0, pop)
    e.stub(0x4efcd0, spec='c,d,s0,s1', fn=segs)

    def obj(em, key, a, pop):
        n = em.r32(a[0])
        o = objects.get(n, 0)
        em.t.handle(key, (n,))
        em.t.override(key, o)
        em._ret(o, pop)
    e.stub(0x40eee0, spec='c', fn=obj)

    def touch(em, key, a, pop):  # ECX the segment's start, EDX the crossing; its float answer is dropped
        em.t.handle(key, (em.r32(a[1] + 0x24),) + tuple(em.r32(a[0] + 4 * j) for j in range(3)) + (a[2],))
        em._ret(0, pop)
    e.stub(0x5836b0, spec='c,d,s0', fn=touch)

    def set_position(em, key, a, pop):
        em.t.handle('vt+0x18', (a[0],) + tuple(em.r32(a[1] + 4 * j) for j in range(3)))
        em._ret(0, pop)
    e.stub(0x5a8550, spec='c,s0', fn=set_position)
    heap = {}

    def malloc(em, key, a, pop):
        p = em.world_alloc(max(a[0], 4))
        em._ret(p, 0)
    e.import_stub(0x5bb664, 'malloc', 's0', 0, malloc)
    e.import_stub(0x5bb65c, 'free', 's0', 0, lambda em, k, a, pop: em._ret(0, 0))
    t.silent |= {'malloc', 'free'}
    nv = [1] + [e.r32(net + o) for o in (0, 4, 8, 0x24, 0x28, 0x2c, 0x30, 0x34, 0x38)] if net else [0] * 10
    probe.probe_vco_setup(v, ctl, node, keep, (ctypes.c_uint32 * 10)(*nv), (ctypes.c_float * len(f))(*f), (ctypes.c_uint32 * 3)(*u))
    rep = Replay(t, e, e.snapshot())
    probe.probe_app_set_callback(rep.callback())
    e.call(0x49ff80, ecx=v)
    probe.probe_vco_run()
    name = f'collide seed {seed} ({kind}{", remote record" if net else ""})'
    if not compare(name, t.log, rep.log, ulps={'0x004efcd0.point': (0, 1, 2), 'vt+0x18': (1, 2, 3)}):
        return False
    w = [S.bf(e.r32(ctl + 0x68))] + [S.bf(e.r32(ctl + 0xac + 4 * i)) for i in range(45)] + \
        [S.bf(e.r32(mb + 0x14c + 4 * i)) for i in range(4)] + \
        [S.bf(e.r32(ctl + o)) for o in (0x64, 0x6c, 0x70, 0x74, 0x78, 0x54)]
    got, gu = (ctypes.c_float * 56)(), (ctypes.c_uint32 * 5)()
    probe.probe_vco_image(got, gu)
    for i, (a, b) in enumerate(zip(w, got)):
        if not S.same(a, b):
            print(f'{name}: field {i} original {a} rebuild {b}')
            return False
    wu = [e.r32(ctl + 0x36c), e.r32(ctl + 0x374), e.r32(ctl + 0x378), e.r32(ctl + 0x37c), e.r32(ctl + 0x2c)]
    if wu != list(gu):
        print(f'{name}: flags original {wu} rebuild {list(gu)}')
        return False
    return True


def main():
    probe = load_probe()
    probe.probe_app_set_callback.argtypes = [CALLBACK]
    probe.probe_vco_setup.argtypes = [ctypes.c_uint32] * 4 + [ctypes.c_void_p] * 3
    probe.probe_vco_image.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    probe.probe_vb_hooks(1)
    args = [a for a in sys.argv[1:] if not a.startswith('-')]
    runs = int(args[0]) if args else 600
    for seed in range(runs):
        if not run(probe, seed):
            sys.stdout.flush(); os._exit(1)
    print(f'ok: {runs} collisions as the original')
    sys.stdout.flush(); os._exit(0)


if __name__ == '__main__':
    main()
