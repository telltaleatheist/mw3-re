"""A world point on the screen (0x0053cf10) and a render point out to the
window (0x0040da30) against the original x86 (docs/hud.md section 14).

    cmake --build build && .venv/bin/python -P tests/differential/test_project_point.py [runs]

Each run: a random view matrix (a camera looking from a random place), focal
lengths and centre, near plane, the point rect and the clip's right edge,
and a world point in front of, beside or behind the camera; the original
runs on its own matrix stack (push, load_view, pop) at 24-bit precision.
Compared: the outcode (behind the camera only its 8: the original ORs in the
low bits of the input's address, FIX (bug)) and the point within float
tolerance. Then 0x0040da30 on random render and window rects, doubled or not.
"""
import ctypes
import math
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import load_probe  # noqa: E402
from original import Original  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

probe = load_probe()
F = ctypes.c_float
probe.probe_project_point.restype = ctypes.c_uint32
probe.probe_project_point.argtypes = [ctypes.POINTER(F)] * 5
probe.probe_render_to_screen.argtypes = [ctypes.POINTER(ctypes.c_int32), ctypes.c_int32, ctypes.POINTER(F)]

VIEW, FX, FY, CX, CY = 0x73fbc0, 0x73fb9c, 0x73fba0, 0x73fb88, 0x73fb8c
NEAR, CLIP_RIGHT, POINT_RECT = 0x740304, 0x740314, 0x74031c
RENDER, DISPLAY, WINDOW, DOUBLED = 0x5f4d1c, 0x5f4d20, 0x5f4d24, 0x5f4d4c


def fl(*v):
    return struct.pack(f'<{len(v)}f', *v)


def unf(b):
    return list(struct.unpack(f'<{len(b) // 4}f', b))


def view_matrix(r):
    """World -> camera for a camera at p turned by yaw and pitch (rows: x, y, z axes, translation)."""
    yaw, pitch = r.uniform(-math.pi, math.pi), r.uniform(-0.6, 0.6)
    cy, sy, cp, sp = math.cos(yaw), math.sin(yaw), math.cos(pitch), math.sin(pitch)
    right = (cy, 0.0, -sy)
    up = (sy * sp, cp, cy * sp)
    fwd = (sy * cp, -sp, cy * cp)
    p = [r.uniform(-500, 500), r.uniform(0, 50), r.uniform(-500, 500)]
    # p' = p * M: columns are the camera axes
    m = [right[0], up[0], fwd[0], right[1], up[1], fwd[1], right[2], up[2], fwd[2]]
    t = [-sum(p[i] * m[3 * i + j] for i in range(3)) for j in range(3)]
    return m + t, p, fwd, right, up


def close(a, b, eps):
    return abs(a - b) <= eps * max(1.0, abs(a), abs(b))


def main():
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    cpu = Original()
    cpu.fpu_control(0x007f)  # 24-bit, as in play
    point, out = cpu.alloc(16), cpu.alloc(16)
    r = random.Random(53)
    seen = {}
    for n in range(runs):
        m, cam, fwd, right, up = view_matrix(r)
        w, h = r.choice([(640, 480), (800, 600), (1024, 768)])
        proj = [r.uniform(200, 900), r.uniform(200, 900), w / 2 + r.uniform(-20, 20), h / 2 + r.uniform(-20, 20)]
        near = r.choice([0.5, 1.0, 2.0, 10.0])
        rect = [r.choice([0.0, 5.0]), r.choice([0.0, 5.0]), w - r.choice([1.0, 6.0]), h - r.choice([1.0, 6.0])]
        clip_right = w - r.choice([0.0, 1.0])
        kind = r.choice(['front', 'front', 'side', 'behind', 'near'])
        d = {'front': r.uniform(near, 2000), 'side': r.uniform(near, 300), 'behind': -r.uniform(0, 2000),
             'near': r.uniform(-near, near * 1.5)}[kind]
        sx = r.uniform(-1.5, 1.5) * d if kind != 'side' else r.uniform(-8, 8) * max(d, 1)
        sy = r.uniform(-1.2, 1.2) * d if kind != 'side' else r.uniform(-8, 8) * max(d, 1)
        world = [cam[i] + fwd[i] * d + right[i] * sx + up[i] * sy for i in range(3)]
        cpu.write(VIEW, fl(*m))
        for a, v in ((FX, proj[0]), (FY, proj[1]), (CX, proj[2]), (CY, proj[3]), (NEAR, near),
                     (CLIP_RIGHT, clip_right)):
            cpu.write(a, fl(v))
        cpu.write(POINT_RECT, fl(*rect))
        cpu.write(point, fl(*world))
        cpu.call(0x53cf10, ecx=point, edx=out)
        code_o = cpu.uc.reg_read(UC_X86_REG_EAX)
        got_o = unf(bytes(cpu.read(out, 12)))
        o3 = (F * 3)()
        code_r = probe.probe_project_point((F * 12)(*m), (F * 4)(*proj), (F * 6)(near, clip_right, *rect),
                                           (F * 3)(*world), o3)
        got_r = list(o3)
        behind = d <= near * 1.001  # the camera-space z (FIX (bug): only the 8 compared there)
        same_code = code_o == code_r or (behind and code_o & 8 and code_r == 8)
        ok = same_code and all(close(a, b, 2e-4) for a, b in zip(got_o, got_r))
        seen[kind] = seen.get(kind, 0) + 1
        if not ok:
            print(f'run {n} ({kind}): original code {code_o:#x} {got_o}, rebuilt code {code_r:#x} {got_r}')
            sys.stdout.flush()
            os._exit(1)
    # 0x0040da30
    rects = cpu.alloc(0x80)
    for n in range(runs):
        rr = [r.randrange(0, 50), r.randrange(0, 50)]
        rr += [rr[0] + r.randrange(100, 700), rr[1] + r.randrange(100, 500)]
        wr = [r.randrange(0, 50), r.randrange(0, 50)]
        wr += [wr[0] + r.randrange(100, 1400), wr[1] + r.randrange(100, 1000)]
        doubled = r.choice([0, 1])
        for i, (g, rc) in enumerate(((RENDER, rr), (DISPLAY, rr), (WINDOW, wr))):
            base = rects + 0x28 * i
            x, y, xl, yl = rc
            cpu.write(base, struct.pack('<8i', x, y, xl + 1, yl + 1, xl + 1 - x, yl + 1 - y, xl, yl))
            box = base + 0x20
            cpu.u32(box, base)
            cpu.u32(g, box)
        cpu.u32(DOUBLED, doubled)
        p = [r.uniform(-20, 800), r.uniform(-20, 600)]
        cpu.write(point, fl(*p))
        cpu.call(0x40da30, ecx=point)
        got_o = unf(bytes(cpu.read(point, 8)))
        pr = (F * 2)(*p)
        probe.probe_render_to_screen((ctypes.c_int32 * 8)(*rr, *wr), doubled, pr)
        if not all(close(a, b, 1e-6) for a, b in zip(got_o, list(pr))):
            print(f'render_to_screen run {n}: original {got_o}, rebuilt {list(pr)} ({rr}, {wr}, {doubled})')
            sys.stdout.flush()
            os._exit(1)
    print(f'{runs} points ({", ".join(f"{k} {v}" for k, v in sorted(seen.items()))}) and {runs} render points: '
          'the rebuilt projection matches the original')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()
