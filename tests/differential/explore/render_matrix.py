"""Scratch: the original's matrix conventions, run in Unicorn (docs/drafts/render.md).

Checks 0x521670 (euler + translation + scale -> 3x4, multiplied onto the matrix
stack top), 0x51fee0 (multiply a 3x4 onto the top), 0x520b00 (camera world
matrix -> view matrix at 0x73fbc0) and 0x5234f0 (camera space -> screen) against
the formulas written in the draft.
"""
import math
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from original import Original  # noqa: E402

STACK_PTR, FLAG_PTR = 0x5eda90, 0x5eda8c


def f3(o, a, v):
    o.write(a, struct.pack('<3f', *v))


def top(o):
    p = struct.unpack('<I', o.read(STACK_PTR, 4))[0]
    m = struct.unpack('<I', o.read(p, 4))[0]
    return list(struct.unpack('<12f', o.read(m, 48)))


def set_top(o, mat, flag):
    p = struct.unpack('<I', o.read(STACK_PTR, 4))[0]
    m = struct.unpack('<I', o.read(p, 4))[0]
    o.write(m, struct.pack('<12f', *mat))
    fp = struct.unpack('<I', o.read(FLAG_PTR, 4))[0]
    o.u32(fp, flag)


def euler(a, b, c, t, s):
    """The draft's formula for 0x521670: rows are the local axes in the parent."""
    ca, sa, cb, sb, cc, sc = math.cos(a), math.sin(a), math.cos(b), math.sin(b), math.cos(c), math.sin(c)
    r0 = [sb * sa * sc + cc * cb, sc * ca, sc * cb * sa - cc * sb]
    r1 = [sb * sa * cc - sc * cb, cc * ca, sc * sb + cc * cb * sa]
    r2 = [sb * ca, -sa, cb * ca]
    return [x * s[0] for x in r0] + [x * s[1] for x in r1] + [x * s[2] for x in r2] + list(t)


def rx(a):  # row-vector rotations: p' = p R
    c, s = math.cos(a), math.sin(a)
    return [[1, 0, 0], [0, c, s], [0, -s, c]]


def ry(a):
    c, s = math.cos(a), math.sin(a)
    return [[c, 0, -s], [0, 1, 0], [s, 0, c]]


def rz(a):
    c, s = math.cos(a), math.sin(a)
    return [[c, s, 0], [-s, c, 0], [0, 0, 1]]


def mm(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(3)) for j in range(3)] for i in range(3)]


def mul34(l, t):
    """Row vectors: result = L * T (L applied first, then T)."""
    L = [l[0:3], l[3:6], l[6:9]]
    T = [t[0:3], t[3:6], t[6:9]]
    r = mm(L, T)
    tr = [sum(l[9 + k] * t[3 * k + j] for k in range(3)) + t[9 + j] for j in range(3)]
    return r[0] + r[1] + r[2] + tr


def close(a, b, eps=1e-4):
    return all(abs(x - y) <= eps * max(1, abs(y)) for x, y in zip(a, b))


def main():
    o = Original()
    buf = o.alloc(48)
    slot = struct.unpack('<I', o.read(STACK_PTR, 4))[0]
    o.u32(slot, buf)   # the stack slot starts empty (bss); 0x51e9d0 pushes caller buffers
    rnd = random.Random(3)
    rot, tra, sca = o.alloc(12), o.alloc(12), o.alloc(12)
    bad = 0
    for _ in range(200):
        a, b, c = (rnd.uniform(-3, 3) for _ in range(3))
        t = [rnd.uniform(-100, 100) for _ in range(3)]
        s = [rnd.choice([1.0, rnd.uniform(0.2, 3)]) for _ in range(3)]
        f3(o, rot, (a, b, c)); f3(o, tra, t); f3(o, sca, s)
        # 1. identity flag set: the euler matrix is stored as is
        set_top(o, [0] * 12, 1)
        o.call(0x521670, ecx=rot, edx=tra, stack_args=(sca,))
        got = top(o)
        want = euler(a, b, c, t, s)
        bad += not close(got, want)
        # R = Rz(c) Rx(a) Ry(b) in row-vector form (roll, then pitch, then yaw)
        R = mm(mm(rz(c), rx(a)), ry(b))
        bad += not close(got[0:9], [R[i][j] * s[i] for i in range(3) for j in range(3)])
        # 2. onto a non-identity top: result = local * top
        parent = euler(*(rnd.uniform(-3, 3) for _ in range(3)), [rnd.uniform(-50, 50) for _ in range(3)], [1, 1, 1])
        set_top(o, parent, 0)
        o.call(0x521670, ecx=rot, edx=tra, stack_args=(sca,))
        bad += not close(top(o), mul34(want, parent))
        # 3. 0x51fee0 mode 0 (full 3x4) and mode 2 (rotation only)
        loc = o.alloc(48)
        o.write(loc, struct.pack('<12f', *want))
        set_top(o, parent, 0)
        o.call(0x51fee0, ecx=loc, edx=0)
        bad += not close(top(o), mul34(want, parent))
        set_top(o, parent, 0)
        o.call(0x51fee0, ecx=loc, edx=2)
        g = top(o)
        # mode 2 leaves the translation as whatever its stack frame held (quirk)
        bad += not close(g[0:9], mul34(want, parent)[0:9])
    print('euler/multiply mismatches:', bad)

    # 4. the view matrix: camera world matrix W -> V at 0x73fbc0
    bad = 0
    for _ in range(100):
        W = euler(*(rnd.uniform(-3, 3) for _ in range(3)), [rnd.uniform(-500, 500) for _ in range(3)], [1, 1, 1])
        m = o.alloc(48)
        o.write(m, struct.pack('<12f', *W))
        o.call(0x520b00, ecx=m)
        V = list(struct.unpack('<12f', o.read(0x73fbc0, 48)))
        # W' = rows x, -y, -z, t; V = inverse(W') so p_cam = p_world * V
        r0, r1, r2, t = W[0:3], [-x for x in W[3:6]], [-x for x in W[6:9]], W[9:12]
        R = [r0, r1, r2]
        Vr = [[R[j][i] for j in range(3)] for i in range(3)]
        Vt = [-sum(t[k] * Vr[k][j] for k in range(3)) for j in range(3)]
        bad += not close(V, Vr[0] + Vr[1] + Vr[2] + Vt, 1e-3)
        # a point one unit in front of the camera (along -local z) lands at cam z = +1
        p = [t[i] - W[6 + i] for i in range(3)]
        cam = [sum(p[k] * V[3 * k + j] for k in range(3)) + V[9 + j] for j in range(3)]
        bad += not close(cam, [0, 0, 1], 1e-3)
    print('view matrix mismatches:', bad)

    # 5. projection 0x5234f0: camera space (x, y, z) -> (sx, sy, 1/z)
    for g, v in ((0x73fb9c, 320.0), (0x73fba0, 240.0), (0x73fb88, 320.0), (0x73fb8c, 240.0)):
        o.write(g, struct.pack('<f', v))
    src, dst = o.alloc(12), o.alloc(12)
    f3(o, src, (10.0, 5.0, 20.0))
    o.call(0x5234f0, ecx=src, edx=dst, stack_args=(1,))
    print('project (10,5,20):', struct.unpack('<3f', o.read(dst, 12)), 'expect (480, 300, 0.05)')


if __name__ == '__main__':
    main()
