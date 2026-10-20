"""zRender's 16-bit 2D line against the original x86 (docs/twod.md, The edit bar).

    cmake --build build && .venv/bin/python -P tests/differential/test_twod_line.py [runs]

The original 0x00563b80 ([0x008008c0] in 16-bit modes, behind 0x00562940:
the edit bar's caret, the HUD's lines) and the port's zrndr::line16 draw the
same random line into the same random 16-bit target (odd widths, padded
pitches): steep and shallow, every direction, points, horizontal and
vertical runs. The targets must be identical. Only lines inside the target
are compared: the original clips nothing (FIX (bug), the port skips pixels
off the target), so one off the edge writes into the neighbouring row or
past the buffer; for those the port is checked to draw only the pixels on it.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import load_probe  # noqa: E402
from original import Original  # noqa: E402

LINE = 0x563b80
TARGET = (0x8007c8, 0x8007cc, 0x8007d0, 0x8007d4)
probe = load_probe()
probe.probe_line16.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_int32,
                               ctypes.c_int32, ctypes.c_int32, ctypes.c_int32, ctypes.c_uint16]


def case(r, inside):
    tw, th = r.randrange(1, 90), r.randrange(1, 70)
    pitch = 2 * tw + 2 * r.choice([0, 0, 1, 3])
    if inside:
        pts = [r.randrange(tw), r.randrange(th), r.randrange(tw), r.randrange(th)]
        if r.random() < 0.2:
            pts[2] = pts[0]
        elif r.random() < 0.2:
            pts[3] = pts[1]
    else:
        pts = [r.randrange(-40, tw + 40), r.randrange(-40, th + 40), r.randrange(-40, tw + 40),
               r.randrange(-40, th + 40)]
    target = bytes(r.randrange(256) for _ in range(pitch * th))
    return {'tw': tw, 'th': th, 'pitch': pitch, 'pts': pts, 'color': r.randrange(1 << 16), 'target': target}


def original(cpu, c):
    cpu.heap_next = 0x10000000
    tgt = cpu.alloc(len(c['target']))
    cpu.write(tgt, c['target'])
    for a, v in zip(TARGET, (tgt, c['tw'], c['th'], c['pitch'])):
        cpu.u32(a, v)
    x0, y0, x1, y1 = c['pts']
    cpu.call(LINE, ecx=tgt, edx=x0 & 0xffffffff, stack_args=(y0 & 0xffffffff, x1 & 0xffffffff, y1 & 0xffffffff,
                                                             c['color']))
    return cpu.read(tgt, len(c['target']))


def port(c):
    tgt = ctypes.create_string_buffer(c['target'], len(c['target']))
    probe.probe_line16(tgt, c['tw'], c['th'], c['pitch'], *c['pts'], c['color'])
    return tgt.raw


def expected_clipped(c):
    """The line's pixels on the target (the same steps as the original), over the target."""
    t = bytearray(c['target'])
    x0, y0, x1, y1 = c['pts']
    dx, dy = abs(x1 - x0), abs(y1 - y0)
    sx, sy = (1 if x1 >= x0 else -1), (1 if y1 >= y0 else -1)
    x, y = x0, y0
    major, minor = (dx, dy) if dx > dy else (dy, dx)
    err = major >> 1
    for _ in range(major + 1):
        if 0 <= x < c['tw'] and 0 <= y < c['th']:
            t[y * c['pitch'] + 2 * x:y * c['pitch'] + 2 * x + 2] = struct.pack('<H', c['color'])
        err += minor
        if dx > dy:
            x += sx
            if err > major:
                err -= major
                y += sy
        else:
            y += sy
            if err > major:
                err -= major
                x += sx
    return bytes(t)


def main():
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    cpu = Original()
    bad = 0
    for i in range(runs):
        r = random.Random(f'line:{i}')
        c = case(r, True)
        a, b = original(cpu, c), port(c)
        if a != b:
            bad += 1
            if bad <= 5:
                print(f'run {i}: differ; {c["tw"]}x{c["th"]} pitch {c["pitch"]} {c["pts"]}')
        if a != expected_clipped(c):
            bad += 1
            print(f'run {i}: the clipped model disagrees with the original')
    off = 0
    for i in range(runs // 3):
        c = case(random.Random(f'off:{i}'), False)
        off += 1
        if port(c) != expected_clipped(c):
            bad += 1
            if bad <= 5:
                print(f'off {i}: the port drew off the target or missed a pixel on it')
    print(f'zTwoD line: {runs}/{runs} lines inside the target {"identical" if not bad else "checked"}; '
          f'{off} lines past the edges clipped; {bad} differences')
    return 1 if bad else 0


if __name__ == '__main__':
    code = 1
    try:
        code = main()
    except BaseException:
        import traceback
        traceback.print_exc()
    sys.stdout.flush()
    os._exit(code)
