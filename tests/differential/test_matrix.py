"""The ported matrix stack and camera maths against the original x86 code.

    cmake --build build && .venv/bin/python tests/differential/test_matrix.py

Random matrices, angles (with the gimbal cases of 0x523bc0), scales, cameras,
fovs and window rects. Floats compare within a tolerance scaled to the terms
summed (x87 against SSE; play-accurate, not bit-identical). docs/drafts/matrix.md.
"""
import ctypes
import math
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import ROOT, Original  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBP, UC_X86_REG_EDX, UC_X86_REG_ESI  # noqa: E402

LIB = os.environ.get('MW3PROBE') or next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
F = ctypes.c_float
fp = ctypes.POINTER(F)
i32p = ctypes.POINTER(ctypes.c_int32)
probe.probe_matrix_points.argtypes = [ctypes.c_int, fp, ctypes.c_int32, fp, fp, ctypes.c_int32, fp, F]

SLOTS, FLAGS, PSLOT, PFLAG = 0x63d760, 0x63d7e8, 0x5eda90, 0x5eda8c
VIEW, CAMERA, HW = 0x73fbc0, 0x73fc00, 0x72f348
IAT_ASIN, IAT_COPYSIGN = 0x5bb5f4, 0x5bb5ac
# _CIasin: st0 = atan2(x, sqrt(1 - x*x)); _copysign(double x, double y) -> st0.
ASIN = bytes.fromhex('d9c0 d8c8 d9e8 dee1 d9fa d9f3 c3')
COPYSIGN = bytes.fromhex('8b442410 2500000080 8b4c2408 81e1ffffff7f 0bc8 894c2408 dd442404 c3')
# The Projection fields in the probe's order, with their addresses.
PROJ = [0x63d338, 0x63d33c, 0x63d748, 0x63d74c, 0x63d750, 0x63d754, 0x63d740, 0x63d744, 0x63d758,
        0x63d75c, 0x63d7e0, 0x63d7e4, 0x73fb80, 0x73fb88, 0x73fb8c, 0x73fb90, 0x73fb94, 0x73fb98,
        0x73fb9c, 0x73fba0, 0x73fc30]
STACK_OPS = {0: 0x51fee0, 1: 0x521670, 2: 0x520330, 3: 0x520480, 4: 0x520610, 5: 0x5207b0,
             6: 0x520960, 7: 0x51fb30, 8: 0x51fd40, 9: 0x51fd70, 10: 0x51fde0, 11: 0x51ea30,
             12: 0x51ea60, 13: 0x51e930, 14: 0x51e9d0}
POINT_OPS = {0: 0x5225b0, 1: 0x522640, 2: 0x5226e0, 3: 0x5227a0, 4: 0x522910, 6: 0x5220e0}

fails = []
NAN = float('nan')


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


def fbits(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def arr(vals, n=None):
    vals = list(vals) + [0.0] * ((n or len(vals)) - len(vals))
    return (F * len(vals))(*vals)


def check(what, got, want, bound, eps=1e-6):
    """got: rebuilt, want: original; bound: per value, the size of the terms summed."""
    for i, (a, b) in enumerate(zip(got, want)):
        b_ = bound[i] if isinstance(bound, (list, tuple)) else bound
        if math.isnan(a) and math.isnan(b):
            continue
        if not abs(a - b) <= eps * (b_ + abs(b)) + 1e-30:
            fails.append(f'{what}: [{i}] rebuilt {a!r} original {b!r}')
            return False
    return True


def euler_mat(rnd, tmax=100.0, smax=3.0):
    """A random 3x4 made the way the game makes them (python reference)."""
    a, b, c = (rnd.uniform(-math.pi, math.pi) for _ in range(3))
    ca, sa, cb, sb, cc, sc = math.cos(a), math.sin(a), math.cos(b), math.sin(b), math.cos(c), math.sin(c)
    s = [rnd.choice([1.0, rnd.uniform(0.2, smax)]) for _ in range(3)]
    r = [sb * sa * sc + cc * cb, sc * ca, sc * cb * sa - cc * sb,
         sb * sa * cc - sc * cb, cc * ca, sc * sb + cc * cb * sa,
         sb * ca, -sa, cb * ca]
    r = [r[i] * s[i // 3] for i in range(9)]
    return [f32(x) for x in r + [rnd.uniform(-tmax, tmax) for _ in range(3)]]


def any_mat(rnd):
    return euler_mat(rnd) if rnd.random() < 0.7 else [f32(rnd.uniform(-10, 10)) for _ in range(12)]


def mul_bound(l, t, translation=True):
    """Per element: sum of |terms| of l * t."""
    b = [sum(abs(l[3 * i + k] * t[3 * k + j]) for k in range(3)) for i in range(3) for j in range(3)]
    b += [sum(abs(l[9 + k] * t[3 * k + j]) for k in range(3)) + abs(t[9 + j]) for j in range(3)]
    return b if translation else b[:9]


class Emu:
    def __init__(self):
        self.o = o = Original()
        o.import_code(IAT_ASIN, ASIN)
        o.import_code(IAT_COPYSIGN, COPYSIGN)
        self.below, self.top, self.new = o.alloc(48), o.alloc(48), o.alloc(48)
        self.a, self.b, self.c, self.d = o.alloc(48), o.alloc(48), o.alloc(48), o.alloc(48)
        self.src, self.dst, self.extra = o.alloc(12 * 64), o.alloc(12 * 64), o.alloc(12 * 64)
        self.model = o.alloc(0x100)
        self.frame = o.alloc(0x400) + 0x200

    def wf(self, addr, vals):
        self.o.write(addr, struct.pack(f'<{len(vals)}f', *vals))

    def rf(self, addr, n):
        return list(struct.unpack(f'<{n}f', self.o.read(addr, 4 * n)))

    def r32(self, addr):
        return struct.unpack('<I', self.o.read(addr, 4))[0]

    def setup(self, below, below_flag, top, flag):
        o = self.o
        self.wf(self.below, below)
        self.wf(self.top, top)
        o.u32(SLOTS, self.below), o.u32(SLOTS + 4, self.top)
        o.u32(FLAGS, below_flag), o.u32(FLAGS + 4, flag)
        o.u32(PSLOT, SLOTS + 4), o.u32(PFLAG, FLAGS + 4)

    def stack(self, op, below, below_flag, top, flag, a, mode):
        o = self.o
        self.setup(below, below_flag, top, flag)
        addr = STACK_OPS[op]
        if op == 0:
            self.wf(self.a, a)
            o.call(addr, ecx=self.a, edx=mode)
        elif op == 1:
            self.wf(self.a, a[0:3]), self.wf(self.b, a[3:6]), self.wf(self.c, a[6:9])
            o.call(addr, ecx=self.a, edx=self.b, stack_args=(self.c,))
        elif op in (2, 3):
            o.call(addr, stack_args=tuple(fbits(x) for x in a[0:3]))
        elif op in (4, 5, 6):
            o.call(addr, stack_args=(fbits(a[0]),))
        elif op in (8, 9):
            self.wf(self.a, a)
            o.call(addr, ecx=self.a)
        elif op == 13:
            o.call(addr, ecx=self.new)
        elif op == 14:
            self.wf(self.new, a)
            o.call(addr, ecx=self.new)
        else:
            o.call(addr)
        slot = self.r32(PSLOT)
        m = self.rf(self.r32(slot), 12)
        return m, self.r32(self.r32(PFLAG)), (slot - SLOTS) // 4

    def put_view(self, v, c):
        self.wf(VIEW, v), self.wf(CAMERA, c)


def rebuilt_stack(op, below, below_flag, top, flag, a, mode):
    t, fl = arr(top), ctypes.c_int32(flag)
    idx = probe.probe_matrix_stack(op, arr(below), below_flag, t, ctypes.byref(fl), arr(a, 12), mode)
    return list(t), fl.value, idx


def test_stack(emu, rnd):
    cases = 0
    for n in range(3000):
        op = n % 15
        below, top = any_mat(rnd), any_mat(rnd)
        flag, below_flag = rnd.choice([0, 1]), rnd.choice([0, 1])
        mode = rnd.choice([0, 1, 2, 3])
        if op == 1:
            a = [f32(rnd.uniform(-7, 7)) for _ in range(3)]
            # 0, -0 and NaN translations and 1 and NaN scales take the skip branches
            a += [f32(rnd.choice([0.0, -0.0, NAN, rnd.uniform(-100, 100)])) for _ in range(3)]
            a += [f32(rnd.choice([1.0, NAN, rnd.uniform(-3, 3)])) for _ in range(3)]
        elif op in (4, 5, 6):
            a = [f32(rnd.uniform(-10, 10))]
        elif op in (2, 3):
            a = [f32(rnd.uniform(-50, 50)) for _ in range(3)]
        else:
            a = any_mat(rnd)
        if op in (7, 11, 12):
            v, c = any_mat(rnd), any_mat(rnd)
            emu.put_view(v, c)
            probe.probe_matrix_put_view(arr(v), arr(c))
        want, wflag, widx = emu.stack(op, below, below_flag, top, flag, a, mode)
        got, gflag, gidx = rebuilt_stack(op, below, below_flag, top, flag, a, mode)
        what = f'stack op {op} mode {mode} flag {flag}'
        if (gflag, gidx) != (wflag, widx):
            fails.append(f'{what}: flag/index rebuilt {gflag},{gidx} original {wflag},{widx}')
        # bounds: the terms each element sums
        if op == 0:
            n_cmp = 9 if (mode == 2 and not flag) else 12  # mode 2: stack garbage in row 3
            bound = mul_bound(a, top) if not flag else [0.0] * 12
            check(what, got[:n_cmp], want[:n_cmp], bound[:n_cmp])
        elif op == 1:
            # |local| per element: scale (rows) and translation; its 3x3 terms are sin/cos products <= 1
            loc = [abs(a[6 + i // 3]) for i in range(9)] + [abs(x) for x in a[3:6]]
            loc = [1.0 if math.isnan(x) else x for x in loc]
            check(what, got, want, mul_bound(loc, [abs(x) for x in top]) if not flag else [x + 1 for x in loc])
        elif op == 7:
            check(what, got, want, mul_bound(below, v))
        else:
            mag = max(abs(x) for x in top)
            arg = max(abs(x) for x in a)
            bound = {2: mag * arg, 3: 4 * mag * max(1, arg), 4: 2 * mag, 5: 2 * mag, 6: 2 * mag}.get(op, 0.0)
            check(what, got, want, bound)
        cases += 1
    return cases


def test_view(emu, rnd):
    cases = 0
    for _ in range(500):
        w = euler_mat(rnd, 2000.0, 1.0) if rnd.random() < 0.8 else any_mat(rnd)
        emu.wf(emu.a, w)
        emu.o.call(0x520b00, ecx=emu.a)
        want = emu.rf(VIEW, 12) + emu.rf(CAMERA, 12)
        out = arr([], 24)
        probe.probe_matrix_set_view(arr(w), out)
        # the 3x3 parts are copies and sign flips (exact); row 3 sums t * rotation
        rot = max(abs(x) for x in w[:9])
        check('set_view', list(out), want, [0.0] * 9 + [3 * rot * max(abs(x) for x in w[9:])] * 3 + [0.0] * 12)
        cases += 1
    return cases


def test_euler(emu, rnd):
    """0x523bc0 on euler-built matrices, the gimbal edges and odd inputs."""
    cases = 0
    edges = [1.0, -1.0, f32(1.0000001), f32(-1.0000001), f32(0.99999994), f32(-0.99999994), 2.0, -3.0,
             float('nan'), 0.0]
    for n in range(1500):
        if n < 600:
            m = euler_mat(rnd, 100.0, 1.0)
        elif n < 1200:
            m = [f32(rnd.uniform(-2, 2)) for _ in range(12)]
            m[7] = rnd.choice(edges)
            if rnd.random() < 0.3:
                m[0] = m[2] = 0.0  # atan2(0, 0) in the gimbal branch
        else:
            # straight up/down: a = +-pi/2 exactly, as the game's pitch clamp builds
            a = rnd.choice([math.pi / 2, -math.pi / 2])
            emu.wf(emu.a, [f32(a), f32(rnd.uniform(-3, 3)), f32(rnd.uniform(-3, 3))])
            emu.wf(emu.b, [0, 0, 0]), emu.wf(emu.c, [1, 1, 1])
            emu.setup([0] * 12, 0, [0] * 12, 1)
            emu.o.call(0x521670, ecx=emu.a, edx=emu.b, stack_args=(emu.c,))
            m = emu.rf(emu.top, 12)
        emu.wf(emu.a, m)
        emu.o.call(0x523bc0, ecx=emu.a, edx=emu.b)
        want = emu.rf(emu.b, 3)
        out = arr([], 3)
        probe.probe_matrix_to_euler(arr(m), out)
        check(f'matrix_to_euler m7={m[7]!r}', list(out), want, 1.0, 1e-6)
        cases += 1
    return cases


def test_surface(emu, rnd):
    """The impact's turn to a surface: 0x525a50 from (0, 1, 0) to a unit normal
    (as 0x4020d0 leaves it), then 0x525f80 (0x525740, 0x523bc0)."""
    cases = 0
    for n in range(1500):
        if n < 40:
            v = [rnd.choice([0.0, 1.0, -1.0]) for _ in range(3)] or [0, 1, 0]
            if not any(v):
                v = [0.0, -1.0, 0.0]
        else:
            v = [rnd.uniform(-1, 1) for _ in range(3)]
            if rnd.random() < 0.3:
                v[1] = abs(v[1]) + 2.0  # mostly up, as the ground is
        length = math.sqrt(sum(x * x for x in v))
        v = [f32(x / length) for x in v]
        up, q = emu.a, emu.c
        emu.wf(up, [0.0, 1.0, 0.0])
        emu.wf(emu.b, v)
        emu.o.call(0x525a50, ecx=up, edx=emu.b, stack_args=(q,))
        emu.o.call(0x525f80, ecx=q, edx=emu.top)
        want = emu.rf(emu.top, 3)
        out = arr([], 3)
        probe.probe_surface_angles(arr(v), out)
        check(f'surface angles {v}', list(out), want, 1.0, 1e-5)
        cases += 1
    return cases


def test_invert(emu, rnd):
    cases = 0
    for n in range(800):
        m = any_mat(rnd)
        if n % 10 == 0:
            m[3:6] = [2 * x for x in m[0:3]]  # singular
            m[6:9] = [m[0] + m[3], m[1] + m[4], m[2] + m[5]]
            m = [f32(x) for x in m]
        t = n % 2
        init = [f32(rnd.uniform(-1, 1)) for _ in range(12)]
        emu.wf(emu.a, m), emu.wf(emu.b, init)
        emu.o.call(0x523c50, ecx=emu.a, edx=emu.b, stack_args=(t,))
        wr = emu.o.uc.reg_read(UC_X86_REG_EAX)
        want = emu.rf(emu.b, 12)
        out = arr(init)
        gr = probe.probe_matrix_invert(arr(m), out, t)
        if gr != wr:
            fails.append(f'invert: returns rebuilt {gr} original {wr}')
            continue
        # cofactors over the determinant: the error grows with the condition
        det = abs(m[0] * (m[8] * m[4] - m[5] * m[7]) + m[1] * (m[6] * m[5] - m[3] * m[8])
                  + m[2] * (m[3] * m[7] - m[6] * m[4])) or 1
        mag = max(abs(x) for x in m)
        check('invert', list(out), want, 20 * mag ** 4 / det * max(1, mag))
        cases += 1
    return cases


def test_projection(emu, rnd):
    cases = 0
    for n in range(400):
        w, h = rnd.choice([(640, 480), (800, 600), (1024, 768), (512, 384), (rnd.randint(1, 2000), rnd.randint(1, 1500))])
        hf = rnd.uniform(0.2, 2.8)
        vf = hf * h / w if rnd.random() < 0.5 else rnd.uniform(0.2, 2.8)
        a = [f32(x) for x in (rnd.choice([0, rnd.randint(0, 400)]), rnd.choice([0, rnd.randint(0, 300)]),
                             w * 0.5, h * 0.5, 1 / math.tan(hf / 2), 1 / math.tan(vf / 2),
                             rnd.choice([0.1, rnd.uniform(0.01, 10)]), rnd.choice([800.0, rnd.uniform(10, 5000)]))]
        hw = n % 2
        o = emu.o
        o.u32(HW, hw)
        o.call(0x521c60, stack_args=tuple(fbits(x) for x in a))
        o.call(0x521c10, stack_args=(fbits(f32(hf)), fbits(f32(vf))))
        want = [emu.rf(x, 1)[0] for x in PROJ]
        o.call(0x51e8d0)
        want += list(struct.unpack('<2f', struct.pack('<2I', o.uc.reg_read(UC_X86_REG_EAX), o.uc.reg_read(UC_X86_REG_EDX))))
        out = arr([], 23)
        probe.probe_matrix_projection(arr(a + [f32(hf), f32(vf)]), hw, out)
        got = list(out)
        check(f'set_projection hw={hw}', got, want, 0.0, 1e-6)
        cases += 1
        # the projections on these constants
        for op, addr in ((0, 0x5234f0), (1, 0x523540), (2, 0x523590), (3, 0x5235f0), (4, 0x521db0), (5, 0x521de0)):
            count = rnd.randint(0 if op >= 2 else 1, 40)  # project and project_d3d are do-whiles
            pts = []
            for _ in range(count):
                if op < 2 or op > 3:
                    z = rnd.choice([rnd.uniform(0.05, 1000), rnd.uniform(-50, -0.01), a[6]])
                    pts += [rnd.uniform(-2, 2) * z, rnd.uniform(-2, 2) * z, z]
                else:  # screen points with 1/z or near/z
                    pts += [rnd.uniform(-100, 2100), rnd.uniform(-100, 1600), rnd.uniform(1e-4, 20)]
            pts = [f32(x) for x in pts]
            emu.wf(emu.src, pts), emu.wf(emu.dst, pts)
            if op < 4:
                o.call(addr, ecx=emu.src, edx=emu.dst, stack_args=(count,))
            else:
                o.call(addr, ecx=emu.dst, edx=count)
            want = emu.rf(emu.dst, 3 * count)
            dst = arr(pts)
            probe.probe_matrix_project(op, arr(pts), dst, count)
            p = dict(zip(PROJ, PROJ_all(emu)))
            if op < 2:    # x * f * rz + c
                bound = [abs(want[i]) + abs(p[0x73fb88 + 4 * (i % 3 == 1)]) for i in range(3 * count)]
            elif op < 4:  # (s - c) * inv_f * z
                bound = [(abs(pts[i]) + abs(p[0x73fb88 + 4 * (i % 3 == 1)])) * abs(p[0x63d758 + 4 * (i % 3 == 1)])
                         * abs(want[i - i % 3 + 2]) for i in range(3 * count)]
            else:
                bound = 0.0
            check(f'project op {op} hw={hw}', list(dst), want, bound)
            cases += 1
    return cases


def PROJ_all(emu):
    return [emu.rf(x, 1)[0] for x in PROJ]


def test_points(emu, rnd):
    cases = 0
    for n in range(1500):
        op = rnd.choice([0, 1, 2, 3, 4, 5, 6])
        top = any_mat(rnd)
        flag = 1 if rnd.random() < 0.15 else 0
        count = rnd.randint(1 if op == 5 else 0, 30)
        src = [f32(rnd.uniform(-500, 500)) for _ in range(3 * count)]
        extra = [f32(rnd.uniform(-5, 5)) for _ in range(3 * count)]
        guard = [f32(rnd.uniform(-1, 1)) for _ in range(3 * 32)]
        f = f32(rnd.uniform(0, 1))
        init = src + guard[3 * count:] if op in (0, 1, 3) else guard
        emu.setup([0] * 12, 0, top, flag)
        emu.wf(emu.src, src + guard[3 * count:]), emu.wf(emu.dst, init), emu.wf(emu.extra, extra)
        if op in (0, 1, 3):
            emu.o.call(POINT_OPS[op], ecx=emu.dst, edx=count)
        elif op in (2, 4):
            emu.o.call(POINT_OPS[op], ecx=emu.src, edx=emu.dst, stack_args=(count,))
        elif op == 6:
            emu.o.call(POINT_OPS[op], ecx=emu.dst, edx=emu.src, stack_args=(emu.extra, count, fbits(f)))
        else:
            # the inlined transform in 0x53f2e0, run from 0x53f37d to 0x53f426
            model = emu.model
            emu.o.u32(model + 0x14, count), emu.o.u32(model + 0x38, emu.src)
            emu.o.u32(0x74639c, emu.dst)
            uc = emu.o.uc
            uc.reg_write(UC_X86_REG_ESI, model)
            uc.reg_write(UC_X86_REG_EBP, emu.frame)
            uc.emu_start(0x53f37d, 0x53f426, count=1_000_000)
        want = emu.rf(emu.dst, 96)
        dst = arr(init)
        probe.probe_matrix_points(op, arr(top), flag, arr(src + guard[3 * count:]), dst, count, arr(extra, 96), f)
        # per value: the terms summed (rows of the top against the point)
        bound = [0.0] * 96
        for k in range(count):
            v = [abs(x) for x in src[3 * k:3 * k + 3]]
            t = [abs(x) for x in top]
            for j in range(3):
                bound[3 * k + j] = (sum(v[i] * t[3 * i + j] for i in range(3)) + sum(v[i] * t[3 * j + i] for i in range(3))
                                    + t[9 + j] * (1 + sum(t[3 * j + i] for i in range(3))) + abs(extra[3 * k + j]))
        check(f'points op {op} flag {flag} count {count}', list(dst), want, bound)
        cases += 1
    return cases


def main():
    rnd = random.Random(1999)
    emu = Emu()
    counts = {}
    for name, fn in (('stack', test_stack), ('view', test_view), ('euler', test_euler), ('surface', test_surface), ('invert', test_invert),
                     ('projection', test_projection), ('points', test_points)):
        before = len(fails)
        counts[name] = fn(emu, rnd)
        if len(fails) > before:
            print(f'{name}: {len(fails) - before} mismatches, first: {fails[before]}')
    total = sum(counts.values())
    if fails:
        raise SystemExit(f'{len(fails)} of {total} cases differ')
    print(f'{total} cases ({", ".join(f"{k} {v}" for k, v in counts.items())}): '
          'the rebuilt matrix maths matches the original x86 within float tolerance')


if __name__ == '__main__':
    main()
