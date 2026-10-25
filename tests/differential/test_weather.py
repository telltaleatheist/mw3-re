"""The rain and the snow against the original x86 (docs/weather.md): the
constructors 0x5a60a0 / 0x5a5ad0 (over the base 0x5a5470 and its reset
0x5a56e0), then frame after frame the updates 0x5a6110 / 0x5a5b40, which run
the draw (0x59ffb0 -> 0x5a5360 -> 0x5a57d0) themselves. rand, the camera node's place and angles (0x4ed530,
0x4ed5d0) and the device's reload, draw and flush are stubs both sides log;
the x87 at 24 bits, as in play.

    cmake --build build && .venv/bin/python tests/differential/test_weather.py [cases]

Each case (odd seeds snow): 0 to 300 particles, then up to 12 frames (snow
40, the camera never jumping: flakes keep their places), each with its own dt,
colour (often the last one), alphas, wind, gravity, down (mostly straight),
the camera's place (small moves, sometimes a jump) and angles, hidden
sometimes, the object's rect, and the manager or none with either rect.
Compared after the constructor and each frame: the current buffer, the
summed time, the camera's last place, the rect the object holds, every
streak and both buffers' places, the ramp's first row, and the calls,
exactly but for what the x87's fsin / fcos (the wind's heading, the camera's
matrix) against libm's can move: 8 ulps, 1e-7 of a place, or a pixel; the
frames that needed it are counted. The snow keeps its places, so the port
takes the emulator's after each frame (the ulps would add up). A frame
starting with no rect is compared on the places only: the exe's width and
height are unset there (FIX (bug), the screen's).
"""
import ctypes
import math
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import Emu, Trace, compare, load_probe  # noqa: E402

U32, I32 = ctypes.c_uint32, ctypes.c_int32
# The rain's and the snow's constructor, update, summed time and camera's last place.
KINDS = {'rain': (0x5a60a0, 0x5a6110, 0x73653c, 0x736528), 'snow': (0x5a5ad0, 0x5a5b40, 0x736538, 0x736518)}
HW = 0x72f348
DRAW, RELOAD, FLUSH = 0x72f3bc, 0x72f374, 0x72f3e4
TL = 0x72f41c
TEX = 0x7700
COUNT = {'cases': 0, 'snow': 0, 'frames': 0, 'particles': 0, 'quads': 0, 'reloads': 0, 'hidden': 0, 'no rect': 0,
         'no manager': 0, 'frames within an ulp or a pixel': 0}


def die():
    sys.stdout.flush()
    os._exit(1)


def f2u(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


def rand_script(seed):
    def rand(n, args):
        r = random.Random(f'{seed}:rand:{n}')
        return r.choice([r.randrange(32768)] * 6 + [0, 32767, 16383])
    return {'rand': rand}


def draw_log(emu, key, a, pop):
    v = []
    for k in range(4):
        b = TL + 0x20 * k
        v += [emu.r32(b), emu.r32(b + 4), emu.r32(b + 8), emu.r32(b + 0x18), emu.r32(b + 0x1c)]
    emu._ret(emu.t.handle(key, [a[0], a[1], a[2], a[3], a[4]] + v), pop)


def camera_out(vals):
    def fn(emu, key, a, pop):
        emu.cpu.write(a[1], struct.pack('<3I', *[f2u(x) for x in vals()]))
        emu._ret(0, pop)
    return fn


def emu_state(e, obj, n, rects, TIME, LAST):
    rect = e.r32(obj + 0x34)
    out = [e.r32(obj + 0x60), e.r32(TIME), e.r32(LAST), e.r32(LAST + 4), e.r32(LAST + 8),
           0 if not rect else rects.index(rect) + 1]
    st, p0, p1 = e.r32(obj + 0x38), e.r32(obj + 0x58), e.r32(obj + 0x5c)
    for i in range(n):
        s = st + 0x20 * i
        out += [e.r32(s), e.r32(s + 4), e.r32(s + 8), e.r32(s + 0xc), e.r32(s + 0x10) & 0xffff,
                e.r32(s + 0x14), e.r32(s + 0x18), e.r32(s + 0x1c)]
        out += [e.r32(p + 12 * i + 4 * k) for p in (p0, p1) for k in range(3)]
    img = e.r32(obj + 0x84)
    pix, alp = e.r32(img + 0x10), e.r32(img + 0x14)
    out += list(struct.unpack('<16H', e.cpu.read(pix, 32)))
    out += list(e.cpu.read(alp, 16))
    return out


def places_only(state, n):
    """The places of a state (no streaks, no rect)."""
    keep = state[:5]
    for i in range(n):
        b = 6 + 14 * i
        keep += state[b + 8:b + 14]
    return keep


def close(a, b, kind):
    """Equal, or within what the x87's fsin / fcos against libm's can move
    (the wind's heading, the camera's matrix): 8 ulps of a float ('f'; a short
    d's normalize magnifies them into the streak's u), a
    pixel of an int ('i') or of a pixel float ('p'), a place ('q') 8 ulps
    or 1e-7 (a place near 0 is a sum that cancels: d's ulps, |d| <= 1); 'x'
    exact."""
    if a == b:
        return True
    if kind == 'f':
        return (a ^ b) >> 31 == 0 and abs(a - b) <= 8
    if kind == 'i':
        return abs(((a ^ 0x80000000) - 0x80000000) - ((b ^ 0x80000000) - 0x80000000)) <= 1
    if kind == 'p':
        fa, fb = struct.unpack('<2f', struct.pack('<2I', a, b))
        return abs(fa - fb) <= 1.0
    if kind == 'q':
        fa, fb = struct.unpack('<2f', struct.pack('<2I', a, b))
        return ((a ^ b) >> 31 == 0 and abs(a - b) <= 8) or abs(fa - fb) <= 1e-7
    return False


def state_kinds(n):
    return 'xffffx' + ('iiiixffx' + 'q' * 6) * n + 'x' * 32


DRAW_KINDS = 'xxxxx' + 'ppfff' * 4


def run_case(probe, seed):
    r = random.Random(seed)
    script = rand_script(seed)
    t = Trace()
    t.script = script
    e = Emu(t)
    e.cpu.fpu_control(0x007f)
    e.heap_stubs()
    e.import_stub(0x5bb668, 'rand', '', 0)
    place, angles = [0, 0, 0], [0, 0, 0]
    e.stub(0x4ed530, '@place', 'c,d', None, camera_out(lambda: place))
    e.stub(0x4ed5d0, '@angles', 'c,d', None, camera_out(lambda: angles))
    screen = e.world_alloc(16)
    e.cpu.write(screen, struct.pack('<4i', 0, 0, 640, 480))
    e.stub(0x579880, '@screen', '', None, lambda emu, key, a, pop: emu._ret(screen, pop))
    for a, k in ((0x579af0, '@begin'), (0x579b10, '@end'), (0x576d70, '@locked')):
        e.stub(a, k, '', None, lambda emu, key, a, pop: emu._ret(0, pop))
    e.w32(DRAW, e.new_stub('draw', 'c,d,s0,s1,s2', 12, draw_log))
    e.w32(RELOAD, e.new_stub('reload', 'c,d,s0', 4,
                             lambda emu, key, a, pop: emu._ret(emu.t.handle(key, [a[0]]), pop)))
    e.w32(FLUSH, e.new_stub('flush', '', 0))
    t.silent |= {'@place', '@angles', '@screen', '@begin', '@end', '@locked'}

    tb = Trace()
    tb.script = script
    probe.probe_app_set_callback(tb.callback())

    kind = 'snow' if seed % 2 else 'rain'
    CTOR, UPDATE, TIME, LAST = KINDS[kind]
    n = r.choice([0, 1, r.randrange(1, 40), r.randrange(40, 301)])
    e.w32(HW, 0)
    obj = e.alloc(0x98)
    e.call(CTOR, ecx=obj, stack=(n,))
    probe.probe_weather_new(n, TEX, kind == 'snow')
    e.w32(HW, 1)
    img, pix, alp = e.world_alloc(0x40), e.world_alloc(0x100), e.world_alloc(0x80)
    e.w32(img + 0x10, pix)
    e.w32(img + 0x14, alp)
    e.w32(obj + 0x84, img)
    e.w32(obj + 0x88, TEX)
    e.w32(obj + 0x50, 0x1000)
    rects = [e.world_alloc(16) for _ in range(3)]
    layer = e.world_alloc(0x40)
    size = 6 + 14 * n + 32
    out = (U32 * size)()

    tolerated = set()

    def check(what, emu_s, port_s, calls=True, kinds=None):
        kinds = kinds or state_kinds(n)
        if calls:
            same = len(t.log) == len(tb.log) and all(
                a[0] == b[0] and len(a[1]) == len(b[1]) and
                all(close(x, y, k) for x, y, k in zip(a[1], b[1], DRAW_KINDS if a[0] == 'draw' else 'x' * 8))
                for a, b in zip(t.log, tb.log))
            if not same and not compare(f'seed {seed} {what}', t.log, tb.log):
                die()
            if not same:
                tolerated.add(what)
        if emu_s != port_s and not (len(emu_s) == len(port_s) and all(
                close(x, y, k) for x, y, k in zip(emu_s, port_s, kinds))):
            k = next((i for i, (x, y, k) in enumerate(zip(emu_s, port_s, kinds)) if not close(x, y, k)),
                     min(len(emu_s), len(port_s)))
            print(f'seed {seed} {what}: word {k} of {len(emu_s)} / {len(port_s)} differs:',
                  emu_s[k:k + 1], port_s[k:k + 1])
            die()
        if emu_s != port_s:
            tolerated.add(what)
        t.log.clear()
        tb.log.clear()

    got = probe.probe_weather_state(out)
    check('after the constructor', emu_state(e, obj, n, rects, TIME, LAST), list(out)[:got])
    color = r.getrandbits(16)
    place = [r.uniform(-2000, 2000), r.uniform(0, 300), r.uniform(-2000, 2000)]
    unset = False
    for frame in range(r.randrange(1, 13) if kind == 'rain' else r.randrange(1, 40)):
        if r.random() < 0.6:
            color = r.choice([color, r.getrandbits(16), 0x7fff])
        alphas = [r.choice([1.0, r.uniform(0, 1)]), r.choice([0.05, r.uniform(0, 1)])]
        wind = [r.uniform(-math.pi, math.pi), r.choice([1.0, 0.0, r.uniform(0, 30)])]
        gravity = r.choice([1.0, r.uniform(0, 20), 0.0])
        down = [0.0, 1.0, 0.0] if r.random() < 0.8 else [r.uniform(-1, 1) for _ in range(3)]
        if r.random() < 0.15 and kind == 'rain':
            place = [r.uniform(-2000, 2000), r.uniform(0, 300), r.uniform(-2000, 2000)]
        else:
            place = [p + r.uniform(-3, 3) for p in place]
        angles = [r.uniform(-1, 1), r.uniform(-math.pi, math.pi), r.choice([0.0, r.uniform(-0.3, 0.3)])]
        w = [color] + [f2u(x) for x in alphas + wind + [gravity] + down + place + angles]
        rv = []
        for _ in range(3):
            left, top = r.choice([0, 0, r.randrange(0, 200)]), r.choice([0, 0, r.randrange(0, 200)])
            rv += [left, top, left + r.choice([640, 800, 1024, r.randrange(1, 1600)]),
                   top + r.choice([480, 600, 768, r.randrange(1, 1200)])]
        has_rect = 1 if r.random() < 0.9 else 0
        managed = 1 if r.random() < 0.85 else 0
        r0, r1 = (1 if r.random() < 0.9 else 0), (1 if r.random() < 0.3 else 0)
        hidden = 1 if r.random() < 0.05 else 0
        # The emulator's side.
        e.w32(obj + 0x44, (e.r32(obj + 0x44) & 0xffff0000) | color)
        for off, v in zip((0x48, 0x4c, 0x68, 0x6c, 0x70, 0x74, 0x78, 0x7c), w[1:9]):
            e.w32(obj + off, v)
        for i in range(3):
            e.cpu.write(rects[i], struct.pack('<4i', *rv[4 * i:4 * i + 4]))
        e.w32(obj + 0x34, rects[0] if has_rect else 0)
        e.w32(layer + 0x10, rects[1] if r0 else 0)
        e.w32(layer + 0x14, rects[2] if r1 else 0)
        e.w32(layer + 0x18, 0)
        e.w32(obj + 0x8, layer if managed else 0)
        flags = e.r32(obj + 0xc)
        e.w32(obj + 0xc, flags | 0x10 if hidden else flags & ~0x10)
        probe.probe_weather_set((U32 * 15)(*w), (I32 * 12)(*rv), has_rect, managed, r0, r1, hidden)
        dt = f2u(r.choice([1 / 30, r.uniform(0.005, 0.2)]))
        e.call(UPDATE, ecx=obj, stack=(dt,))
        probe.probe_weather_update(dt)
        got = probe.probe_weather_state(out)
        emu_s, port_s = emu_state(e, obj, n, rects, TIME, LAST), list(out)[:got]
        if not hidden:
            unset = not has_rect  # a hidden frame keeps the last one's streaks
        if not unset:
            COUNT['quads'] += sum(1 for c in t.log if c[0] == 'draw')
            COUNT['reloads'] += sum(1 for c in t.log if c[0] == 'reload')
            check(f'frame {frame}', emu_s, port_s)
        else:
            COUNT['no rect'] += 1
            check(f'frame {frame} (no rect)', places_only(emu_s, n), places_only(port_s, n), calls=False,
                  kinds='xffff' + 'q' * 6 * n)
        if kind == 'snow' and n:  # from the emulator's places on
            p0, p1 = e.r32(obj + 0x58), e.r32(obj + 0x5c)
            probe.probe_weather_places((U32 * (3 * n))(*struct.unpack(f'<{3 * n}I', e.cpu.read(p0, 12 * n))),
                                       (U32 * (3 * n))(*struct.unpack(f'<{3 * n}I', e.cpu.read(p1, 12 * n))))
        COUNT['frames'] += 1
        COUNT['hidden'] += hidden
        COUNT['no manager'] += 1 - managed
    COUNT['cases'] += 1
    COUNT['snow'] += kind == 'snow'
    COUNT['particles'] += n
    COUNT['frames within an ulp or a pixel'] += len(tolerated)


def main():
    probe = load_probe()
    probe.probe_weather_new.argtypes = [I32, U32, I32]
    probe.probe_weather_set.argtypes = [ctypes.POINTER(U32), ctypes.POINTER(I32), I32, I32, I32, I32, I32]
    probe.probe_weather_update.argtypes = [U32]
    probe.probe_weather_places.argtypes = [ctypes.POINTER(U32), ctypes.POINTER(U32)]
    probe.probe_weather_state.argtypes = [ctypes.POINTER(U32)]
    probe.probe_weather_state.restype = I32
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 400
    for seed in range(count):
        run_case(probe, seed)
    print(f'ok: {count} cases ({", ".join(f"{v} {k}" for k, v in COUNT.items())}), as the original')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()
