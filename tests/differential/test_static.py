"""zRender's static against the original x86 (docs/hud.md section 21): the
noise table 0x5581c0 (rand's low bytes), then the fill 0x558c10 over a
buffer set as the finish target (0x558290), its rect or none, its level and
three colours. rand is a stub both sides log; the x87 at 24 bits, as in
play.

    cmake --build build && .venv/bin/python tests/differential/test_static.py [cases]

Each case: a fresh table, then up to 6 fills on a target of 16..700 x
8..500 pixels (pitch at least its width) of random pixels: a rect inside
it (sometimes empty or one pixel wide) or none (the target less its last
column and row), a level from 0 to 0.9 (the HUD's clamp) or past it,
colours random. Compared: the table, every pixel and the calls.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import Emu, Trace, compare, load_probe  # noqa: E402

U8, U16, U32, I32 = ctypes.c_uint8, ctypes.c_uint16, ctypes.c_uint32, ctypes.c_int32
INIT, TARGET, FILL = 0x5581c0, 0x558290, 0x558c10
NOISE = 0x72c1dc
COUNT = {'cases': 0, 'fills': 0, 'no rect': 0, 'pixels': 0, 'changed': 0}


def die():
    sys.stdout.flush()
    os._exit(1)


def run_case(probe, seed):
    r = random.Random(seed)
    script = {'rand': lambda n, a: random.Random(f'{seed}:rand:{n}').randrange(32768)}
    t, tb = Trace(), Trace()
    t.script = tb.script = script
    e = Emu(t)
    e.cpu.fpu_control(0x007f)
    e.import_stub(0x5bb668, 'rand', '', 0)
    e.import_stub(0x5bb664, '@malloc', 's0', 0, lambda emu, key, a, pop: emu._ret(emu.alloc(a[0]), 0))
    probe.probe_app_set_callback(tb.callback())
    e.call(INIT)
    probe.probe_static_init()
    noise_a = e.cpu.read(e.r32(NOISE), 0x6400)
    noise_b = (U8 * 0x6400)()
    probe.probe_static_noise(noise_b)
    if bytes(noise_a) != bytes(noise_b):
        print(f'seed {seed}: the noise differs')
        die()
    if not compare(f'seed {seed} (the table)', t.log, tb.log):
        die()
    t.log.clear()
    tb.log.clear()
    for _ in range(r.randrange(1, 7)):
        w, h = r.randrange(16, 701), r.randrange(8, 501)
        pitch = w + r.choice([0, 0, r.randrange(1, 40)])
        pix = [r.getrandbits(16) for _ in range(pitch * h)]
        buf = e.alloc(2 * pitch * h)
        e.cpu.write(buf, struct.pack(f'<{len(pix)}H', *pix))
        e.call(TARGET, ecx=buf, edx=w, stack=(h, 2 * pitch))
        has = 1 if r.random() < 0.85 else 0
        if has:
            left, top = r.randrange(0, w), r.randrange(0, h)
            right = r.choice([left, left + 1, r.randrange(left, w + 1)])
            bottom = r.choice([top, top + 1, r.randrange(top, h + 1)])
        else:
            left = top = right = bottom = 0
            COUNT['no rect'] += 1
        rect = e.alloc(16)
        e.cpu.write(rect, struct.pack('<4i', left, top, right, bottom))
        level = r.choice([0.0, 0.9, 1.0, r.uniform(0, 0.9), r.uniform(0, 0.9)])
        lo, hi = struct.unpack('<2I', struct.pack('<d', level))
        cols = [r.getrandbits(16) for _ in range(3)]
        e.call(FILL, ecx=rect if has else 0, edx=cols[0], stack=(lo, hi, cols[1], cols[2]))
        out_a = struct.unpack(f'<{len(pix)}H', e.cpu.read(buf, 2 * len(pix)))
        out_b = (U16 * len(pix))(*pix)
        probe.probe_static_fill(out_b, w, h, pitch, (I32 * 4)(left, top, right, bottom), has, lo, hi, *cols)
        if not compare(f'seed {seed} (a fill)', t.log, tb.log):
            die()
        if list(out_a) != list(out_b):
            k = next(i for i, (x, y) in enumerate(zip(out_a, out_b)) if x != y)
            print(f'seed {seed}: pixel {k} ({k % pitch}, {k // pitch}) of {w}x{h}: {out_a[k]:#06x} / {out_b[k]:#06x}')
            die()
        t.log.clear()
        tb.log.clear()
        COUNT['fills'] += 1
        COUNT['pixels'] += len(pix)
        COUNT['changed'] += sum(1 for x, y in zip(pix, out_a) if x != y)
    COUNT['cases'] += 1


def main():
    probe = load_probe()
    probe.probe_static_noise.argtypes = [ctypes.POINTER(U8)]
    probe.probe_static_fill.argtypes = [ctypes.POINTER(U16), I32, I32, I32, ctypes.POINTER(I32), I32, U32, U32,
                                        U32, U32, U32]
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 300
    for seed in range(count):
        run_case(probe, seed)
    print(f'ok: {count} cases ({", ".join(f"{v} {k}" for k, v in COUNT.items())}), as the original')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()
