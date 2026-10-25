"""The lens flares against the original x86 (docs/render.md, Lens flares): the
list 0x567420, the pick 0x567650 / 0x5676c0, the textures 0x5676d0 and the
draw 0x567d30 with its four squares 0x567b90. The square 0x567720 is a stub
both sides log with what it is given. The x87 at 24 bits, as in play.

    cmake --build build && .venv/bin/python tests/differential/test_flares.py [cases]

Each case: a device or none, the near plane, the clip rect's left and top and
the point rect's right and bottom, four textures (one missing sometimes);
up to 80 lights (on or off; full and gone depths in either order, equal,
zero, NaN) and up to 700 flares on them (past 650 dropped; the pick sees only
the first 64; depths zero sometimes; no light sometimes), added with a
device only (without one the exe rescales the depth for the software path,
not ported); a pick from a random start, then every picked one drawn.
Compared: the list, the picked indexes, every square's arguments.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import Emu, Trace, compare, load_probe  # noqa: E402

U32, I32 = ctypes.c_uint32, ctypes.c_int32
ADD, PICK, TEXTURE, DRAW = 0x567420, 0x567650, 0x5676d0, 0x567d30
COUNT_ = 0x7fb300
LIST = 0x7fb304
PICKED_N, PICKED = 0x800444, 0x800448
ON, TEXTURES, HW, NEAR = 0x72c250, 0x72c258, 0x72f348, 0x73fb80
CLIP_L, CLIP_T, PT_R, PT_B = 0x7402fc, 0x740300, 0x740324, 0x740328
COUNT = {'cases': 0, 'flares': 0, 'dropped': 0, 'picked': 0, 'drawn': 0, 'squares': 0, 'no device': 0, 'off': 0}


def die():
    sys.stdout.flush()
    os._exit(1)


def f2u(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


def run_case(probe, seed):
    r = random.Random(seed)
    t = Trace()
    e = Emu(t)
    e.cpu.fpu_control(0x007f)
    e.stub(0x567720, '0x00567720', 'c,d,s0,s1,s2', None,
           lambda emu, key, a, pop: emu._ret(emu.t.handle(key, [emu.r32(a[0]), emu.r32(a[0] + 4), emu.r32(a[0] + 8),
                                                                a[1], a[2], a[3], a[4]]), pop))
    hw = 1 if r.random() < 0.85 else 0
    near = f2u(r.choice([0.5, 1.0, r.uniform(0.1, 4.0)]))
    rect = [f2u(r.choice([0.0, r.uniform(0, 50)])), f2u(r.choice([0.0, r.uniform(0, 50)])),
            f2u(r.choice([639.0, 799.0, r.uniform(300, 1300)])), f2u(r.choice([479.0, 599.0, r.uniform(200, 1000)]))]
    textures = [0x10000 * (i + 1) for i in range(4)]
    if r.random() < 0.1:
        textures[r.randrange(4)] = 0
    e.w32(HW, hw)
    e.w32(NEAR, near)
    for a, v in zip((CLIP_L, CLIP_T, PT_R, PT_B), rect):
        e.w32(a, v)
    for i, tx in enumerate(textures):
        e.call(TEXTURE, ecx=i, edx=tx)
    probe.probe_flares_reset(hw, near, (U32 * 4)(*rect), (U32 * 4)(*textures))
    lights = []
    for _ in range(r.randrange(1, 81)):
        a = e.world_alloc(0x20)
        on = 0 if r.random() < 0.15 else r.getrandbits(32) | 1
        full = r.choice([5.0, 30.0, r.uniform(0, 100)])
        gone = r.choice([5000.0, r.uniform(0, 10000), full, 0.0, float('nan')])
        if r.random() < 0.1:
            full, gone = gone, full
        e.w32(a + 0x0c, on)
        e.w32(a + 0x14, f2u(full))
        e.w32(a + 0x18, f2u(gone))
        probe.probe_flare_light(a, on, f2u(full), f2u(gone))
        lights.append(a)
    if not hw:
        COUNT['no device'] += 1
    adds = r.choice([r.randrange(0, 20), r.randrange(0, 80), r.randrange(600, 700)]) if hw else 0
    pw, ps = e.world_alloc(12), e.world_alloc(12)
    for _ in range(adds):
        w = [f2u(r.uniform(-3000, 3000)) for _ in range(3)]
        s = [f2u(r.uniform(-100, 1400)), f2u(r.uniform(-100, 1100)),
             f2u(r.choice([0.0, r.uniform(1e-5, 0.05), r.uniform(0, 1)]))]
        color = r.getrandbits(16)
        light = r.choice(lights) if r.random() < 0.95 else 0
        e.cpu.write(pw, struct.pack('<3I', *w))
        e.cpu.write(ps, struct.pack('<3I', *s))
        e.call(ADD, ecx=pw, edx=ps, stack=(color, light))
        probe.probe_flare_add((U32 * 3)(*w), (U32 * 3)(*s), color, light)
    if not hw:  # the list as a device left it: put the same flares in both
        for _ in range(r.randrange(0, 70)):
            i = e.r32(COUNT_)
            rec = [f2u(r.uniform(-3000, 3000)) for _ in range(3)] + [f2u(r.uniform(-100, 1400)), f2u(r.uniform(-100, 1100)),
                                                                     f2u(r.uniform(1e-5, 0.05))]
            light = r.choice(lights)
            color = r.getrandbits(16)
            e.cpu.write(LIST + 0x20 * i, struct.pack('<8I', *rec, color, light))
            e.w32(COUNT_, i + 1)
            probe.probe_flare_add((U32 * 3)(*rec[:3]), (U32 * 3)(*rec[3:6]), color, light)
    n_a = e.r32(COUNT_)
    list_a = [e.r32(LIST + 4 * k) for k in range(8 * min(n_a, 650))]
    out = (U32 * (8 * 650))()
    n_b = probe.probe_flare_list(out, 650)
    if n_a != n_b or list_a != list(out)[:8 * min(n_b, 650)]:
        print(f'seed {seed}: the list {n_a} / {n_b} flares differ')
        die()
    frm = r.choice([0, 0, r.randrange(0, max(n_a, 1) + 3)])
    got_a = e.call(PICK, ecx=frm)
    picked_a = [(e.r32(PICKED + 4 * k) - LIST) // 0x20 for k in range(got_a)]
    picked = (I32 * 64)()
    got_b = probe.probe_flare_pick(frm, picked)
    if got_a != got_b or picked_a != list(picked)[:got_b]:
        print(f'seed {seed}: picked {got_a} {picked_a[:10]} / {got_b} {list(picked)[:min(got_b, 10)]}')
        die()
    t.reset()
    tb = Trace()
    probe.probe_app_set_callback(tb.callback())
    for i in range(got_a):
        e.call(DRAW, ecx=i)
        probe.probe_flare_draw(i)
    if not compare(f'seed {seed} (device {hw}, {got_a} picked)', t.log, tb.log):
        die()
    COUNT['cases'] += 1
    COUNT['flares'] += n_a
    COUNT['dropped'] += max(0, adds - 650)
    COUNT['picked'] += got_a
    COUNT['squares'] += len(t.log)
    COUNT['drawn'] += len(t.log) // 4
    COUNT['off'] += textures.count(0) > 0


def main():
    probe = load_probe()
    probe.probe_flares_reset.argtypes = [I32, U32, ctypes.POINTER(U32), ctypes.POINTER(U32)]
    probe.probe_flare_light.argtypes = [U32, U32, U32, U32]
    probe.probe_flare_add.argtypes = [ctypes.POINTER(U32), ctypes.POINTER(U32), U32, U32]
    probe.probe_flare_pick.argtypes = [I32, ctypes.POINTER(I32)]
    probe.probe_flare_pick.restype = I32
    probe.probe_flare_draw.argtypes = [I32]
    probe.probe_flare_list.argtypes = [ctypes.POINTER(U32), I32]
    probe.probe_flare_list.restype = I32
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 2000
    for seed in range(count):
        run_case(probe, seed)
    print(f'ok: {count} cases ({", ".join(f"{v} {k}" for k, v in COUNT.items())}), as the original')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()
