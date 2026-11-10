"""The net view (gamez/znetwork/znet_view, docs/editor.md, The net view) against the original x86: a link
between two marks brought to the screen (0x58ba40 with the marks' steps 0x58b890 / 0x59b7b0 / 0x5234f0 /
0x59b7e0, the depth cut 0x59b370, the box cut 0x59b490, the direction 0x58b990 and both arrow pairs 0x58da70 /
0x58dfd0 -> 0x58d9a0), the circles placed (0x58ca90) and hit (0x58d0e0), the back arrows hit (0x58d6b0).
Random cameras, clips and ends in front of the camera, behind it, past the far plane and off the screen, in
the exe's layouts in emulator memory (the marks 0x74 bytes, the link 0x120, the exe's own zTwoD vtables
0x5cf150 / 0x5cf1d0 / 0x5cf2d8; each end's element a two-slot vtable of x86 stubs) and as the port's.

    cmake --build build --target mw3probe && .venv/bin/python -P tests/differential/test_net_view.py [runs]

Every field must match bit for bit at the emulator's (Direct3D's) 24 bits. MW3_PRECISION=53 runs both at
full precision, where the exe keeps the transform's sums in the x87's registers and the port rounds each to a
float: there every state, flag and hit must match, a drawn point (the line's ends, the heads', a circle's
place) may be a pixel off (counted), and the largest gap between the floats is printed.
Breaks, made by hand in the port, each must fail: the depth cut (twod_shapes.cpp clip_depth) moving the far
ends first; the heads' points (znet_view.cpp head) rounded without the half; a circle's hit by its radius
alone, not times its ring.
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
from original import FTOL, IAT_FTOL, Original  # noqa: E402

LINK_UPDATE, PLACE, HIT, ARROW_HIT, SET_CLIP = 0x58ba40, 0x58ca90, 0x58d0e0, 0x58d6b0, 0x59b2f0
IDENT_PTR, SLOT_PTR = 0x5eda8c, 0x5eda90
FX, FY, CX, CY = 0x73fb9c, 0x73fba0, 0x73fb88, 0x73fb8c
HEIGHT = 0x730804
VT_MARK, VT_AHEAD, VT_BACK = 0x5cf150, 0x5cf1d0, 0x5cf2d8
HIDDEN = 0x10

probe = load_probe()
probe.probe_net_view.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


def hx(x):
    return '%08x' % struct.unpack('<I', struct.pack('<f', x))[0]


def rotation(rng):
    a, b, c = (rng.uniform(-math.pi, math.pi) for _ in range(3))
    ca, sa, cb, sb, cc, sc = math.cos(a), math.sin(a), math.cos(b), math.sin(b), math.cos(c), math.sin(c)
    rx = [[1, 0, 0], [0, ca, sa], [0, -sa, ca]]
    ry = [[cb, 0, -sb], [0, 1, 0], [sb, 0, cb]]
    rz = [[cc, sc, 0], [-sc, cc, 0], [0, 0, 1]]

    def mul(p, q):
        return [[sum(p[i][k] * q[k][j] for k in range(3)) for j in range(3)] for i in range(3)]
    return mul(mul(rx, ry), rz)


def scenario(rng):
    m = [f32(x) for row in rotation(rng) for x in row] + [f32(rng.uniform(-200, 200)) for _ in range(3)]
    ident = 1 if rng.random() < 0.1 else 0
    w, h = rng.choice([(640, 480), (800, 600), (320, 240), (1024, 768)])
    fx = f32(w / 2 / math.tan(rng.uniform(0.4, 0.8)))
    fy = f32(fx * rng.choice([1.0, 0.75, 1.2]))
    cam = dict(m=m, ident=ident, fx=fx, fy=fy, cx=f32(w / 2), cy=f32(h / 2),
               mn=[0.0, 0.0, f32(rng.uniform(0.5, 5))], mx=[f32(w - 0.9375), f32(h - 0.9375), f32(rng.uniform(40, 3000))],
               height=float(h))

    def end():
        # A point in view space where each case is likely, then back to the world through the inverse.
        kind = rng.random()
        z = (rng.uniform(cam['mn'][2] * 1.01, cam['mx'][2] * 0.99) if kind < 0.6 else
             rng.uniform(-50, cam['mn'][2]) if kind < 0.8 else rng.uniform(cam['mx'][2], cam['mx'][2] * 2))
        spread = (rng.uniform(0.2, 0.9) if rng.random() < 0.7 else rng.uniform(1.0, 3.0)) * abs(z) + rng.uniform(0, 5)
        v = [rng.uniform(-spread, spread) * w / (2 * fx), rng.uniform(-spread, spread) * h / (2 * fy), z]
        if ident:
            p = v
        else:
            r = [m[0:3], m[3:6], m[6:9]]
            t = m[9:12]
            d = [v[i] - t[i] for i in range(3)]
            p = [sum(d[j] * r[i][j] for j in range(3)) for i in range(3)]  # rows are the axes: p = (v - t) R^T
        return dict(pos=[f32(x) for x in p], placed=rng.random() < 0.9, ring=0.75 if rng.random() < 0.8 else f32(rng.uniform(0.3, 1.5)))
    return cam, end(), end(), rng.choice([1, 3])


def plan_text(cam, a, b, kind, ops):
    lines = ['C ' + ' '.join(hx(x) for x in cam['m']) + ' %d ' % cam['ident'] +
             ' '.join(hx(x) for x in [cam['fx'], cam['fy'], cam['cx'], cam['cy']] + cam['mn'] + cam['mx'] + [cam['height']])]
    for name, e in (('a', a), ('b', b)):
        lines.append('N %s %s %d %s' % (name, ' '.join(hx(x) for x in e['pos']), 1 if e['placed'] else 0, hx(e['ring'])))
    lines.append('L %d' % kind)
    for op in ops:
        lines.append(' '.join(str(x) for x in op))
    return '\n'.join(lines) + '\n'


def port(cam, a, b, kind, ops):
    buf = ctypes.create_string_buffer(1 << 16)
    n = probe.probe_net_view(plan_text(cam, a, b, kind, ops).encode(), buf, len(buf))
    assert n >= 0
    return [[int(x) for x in line.split()] for line in buf.value.decode().splitlines()]


class Exe:
    def __init__(self):
        self.o = Original()
        o = self.o
        o.import_code(IAT_FTOL, FTOL)
        self.elem_vt = o.alloc(0x40)
        o.u32(self.elem_vt + 0x1c, o.code(b'\x8d\x41\x04\xc3'))  # lea eax, [ecx+4]; ret: the place
        o.u32(self.elem_vt + 0x14, o.code(b'\x8a\x41\x10\xc3'))  # mov al, [ecx+0x10]; ret: placed
        self.ident = o.alloc(4)
        self.slot = o.alloc(4)
        self.mat = o.alloc(48)
        o.u32(IDENT_PTR, self.ident)
        o.u32(SLOT_PTR, self.slot)
        o.u32(self.slot, self.mat)
        self.mn, self.mx = o.alloc(12), o.alloc(12)

    def wf(self, addr, vals):
        self.o.write(addr, struct.pack('<%df' % len(vals), *vals))

    def setup(self, cam, a, b, kind):
        o = self.o
        self.wf(self.mat, cam['m'])
        o.u32(self.ident, cam['ident'])
        self.wf(FX, [cam['fx']]), self.wf(FY, [cam['fy']]), self.wf(CX, [cam['cx']]), self.wf(CY, [cam['cy']])
        self.wf(self.mn, cam['mn']), self.wf(self.mx, cam['mx'])
        o.call(SET_CLIP, ecx=self.mn, edx=self.mx)
        self.wf(HEIGHT, [cam['height']])
        self.marks = []
        for e in (a, b):
            el = o.alloc(0x20)
            o.write(el, bytes(0x20))
            o.u32(el, self.elem_vt)
            self.wf(el + 4, e['pos'])
            o.write(el + 0x10, bytes([1 if e['placed'] else 0]))
            mk = o.alloc(0x74)
            o.write(mk, bytes(0x74))
            o.u32(mk + 0xc, el)
            o.u32(mk + 0x2c, VT_MARK)
            o.u32(mk + 0x2c + 0xc, HIDDEN)
            o.u32(mk + 0x2c + 0x34, mk)
            self.wf(mk + 0x6c, [1.0, e['ring']])
            self.marks.append(mk)
        lk = o.alloc(0x120)
        o.write(lk, bytes(0x120))
        o.u32(lk + 8, self.marks[0])
        o.u32(lk + 0xc, self.marks[1])
        o.u32(lk + 0x10, kind)
        for off, vt in ((0x14, VT_AHEAD), (0x88, VT_BACK)):
            o.u32(lk + off, vt)
            o.u32(lk + off + 0xc, HIDDEN)
            o.u32(lk + off + 0x34, lk)
        self.link = lk

    def words(self, addr, n):
        return list(struct.unpack('<%dI' % n, self.o.read(addr, 4 * n)))

    def run(self, cam, a, b, kind, ops):
        self.setup(cam, a, b, kind)
        out = []
        o, lk = self.o, self.link
        for op in ops:
            v = []
            if op[0] == 'U':
                o.call(LINK_UPDATE, ecx=lk)
                for mk in self.marks:
                    v += self.words(mk + 0x10, 7) + self.words(mk + 0x68, 2)
                v += self.words(lk + 0xfc, 4) + self.words(lk + 0x10c, 4) + self.words(lk + 0x11c, 1)
                for off in (0x14, 0x88):
                    v.append(self.words(lk + off + 0xc, 1)[0] & HIDDEN)
                    v += self.words(lk + off + 0x3c, 2) + self.words(lk + off + 0x44, 12)
            elif op[0] == 'H':
                for mk in self.marks:
                    o.call(HIT, ecx=mk + 0x2c, stack_args=(op[1] & 0xffffffff, op[2] & 0xffffffff))
                    v.append(self.eax() & 0xff)
            elif op[0] == 'A':
                o.call(ARROW_HIT, ecx=lk + 0x88, stack_args=(op[1] & 0xffffffff, op[2] & 0xffffffff))
                v.append(self.eax())
            elif op[0] == 'P':
                for mk in self.marks:
                    o.call(PLACE, ecx=mk + 0x2c)
                    v.append(self.words(mk + 0x2c + 0xc, 1)[0] & HIDDEN)
                    v += self.words(mk + 0x2c + 0x14, 2)
            out.append(v)
        return out

    def eax(self):
        from unicorn.x86_const import UC_X86_REG_EAX
        return self.o.uc.reg_read(UC_X86_REG_EAX)


def ops_for(rng, cam):
    ops = [('U',), ('P',)]
    for _ in range(4):
        ops.append(('H', rng.randrange(-20, int(cam['mx'][0]) + 20), rng.randrange(-20, int(cam['mx'][1]) + 20)))
    ops.append(('U',))
    return ops


# The 'U' line's floats: each end's view and screen point, size and radius; the direction and its normal.
U_FLOATS = set(range(0, 6)) | {7, 8} | set(range(9, 15)) | {16, 17} | set(range(18, 22))
TOLERANT = os.environ.get('MW3_PRECISION') == '53'


U_POINTS = set(range(22, 26)) | set(range(30, 42)) | set(range(45, 57))
GAP = [0.0]
PIXEL = [0]


def same(p, e, ops):
    if not TOLERANT:
        return p == e
    if len(p) != len(e):
        return False
    for x, y, op in zip(p, e, ops):
        if len(x) != len(y):
            return False
        for i, (u, v) in enumerate(zip(x, y)):
            if op[0] == 'U' and i in U_FLOATS:
                fu, fv = (struct.unpack('<f', struct.pack('<I', w))[0] for w in (u, v))
                if fu != fv and math.isfinite(fu) and math.isfinite(fv):
                    GAP[0] = max(GAP[0], abs(fu - fv) / max(abs(fu), abs(fv), 1.0))
            elif u != v:
                point = (op[0] == 'U' and i in U_POINTS) or (op[0] == 'P' and i % 3)
                if not point or abs(ctypes.c_int32(u).value - ctypes.c_int32(v).value) > 1:
                    return False
                PIXEL[0] += 1
    return True


def main():
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 600
    rng = random.Random(int(os.environ.get('SEED', '30')))
    exe = Exe()
    bad = 0
    seen = set()
    count = dict.fromkeys(['shown', 'heads', 'placed', 'circle hits', 'arrow hits'], 0)
    for r in range(runs):
        cam, a, b, kind = scenario(rng)
        ops = ops_for(rng, cam)
        p = port(cam, a, b, kind, ops)
        e = exe.run(cam, a, b, kind, ops)
        if not same(p, e, ops):
            bad += 1
            if bad <= 3:
                print('run %d differs (kind %d):' % (r, kind))
                for i, (x, y) in enumerate(zip(p, e)):
                    if x != y:
                        print('  op %s\n  port %s\n  exe  %s' % (ops[i], x, y))
                        break
            continue
        u = e[0]
        seen.add((u[6], u[15]))  # both ends' states
        count['shown'] += u[26] == 1
        count['heads'] += any(u[28:30]) or any(u[43:45])
        # A second pass: hits by each circle's centre and edge, and the back heads' tips, from what was placed.
        ops2 = [('U',), ('P',)]
        for k, mk in enumerate((0, 9)):
            x, y = struct.unpack('<ff', struct.pack('<II', u[mk + 3], u[mk + 4]))
            rad = struct.unpack('<f', struct.pack('<I', u[mk + 8]))[0]
            for dx, dy in ((0, 0), (int(rad), 0), (0, int(rad * 0.75) + 1), (int(rad * 0.75), 0)):
                if abs(x) < 1e6 and abs(y) < 1e6:
                    ops2.append(('H', int(x) + dx, int(y) + dy))
        heads = e[0][-12:]
        for i in (2, 8):
            ops2.append(('A', ctypes.c_int32(heads[i]).value - 1, ctypes.c_int32(heads[i + 1]).value))
            ops2.append(('A', ctypes.c_int32(heads[i]).value + 2, ctypes.c_int32(heads[i + 1]).value + 1))
        p2, e2 = port(cam, a, b, kind, ops2), exe.run(cam, a, b, kind, ops2)
        count['placed'] += sum(1 for k in (0, 3) if e2[1][k] == 0)
        count['circle hits'] += sum(sum(x) for x, op in zip(e2, ops2) if op[0] == 'H')
        count['arrow hits'] += sum(sum(x) for x, op in zip(e2, ops2) if op[0] == 'A')
        if not same(p2, e2, ops2):
            bad += 1
            if bad <= 3:
                print('run %d (hits) differs:' % r)
                for i, (x, y) in enumerate(zip(p2, e2)):
                    if x != y:
                        print('  op %s\n  port %s\n  exe  %s' % (ops2[i], x, y))
                        break
    states = sorted('%x%x' % s for s in seen)
    print('%d runs, %d differ; end states seen: %s' % (runs, bad, ' '.join(states)))
    print('  ' + ', '.join('%s %d' % kv for kv in count.items()))
    if TOLERANT:
        print('  the floats\' largest gap: %.2g of their size (1 near 0); points a pixel off: %d' % (GAP[0], PIXEL[0]))
    return 1 if bad else 0


if __name__ == '__main__':
    rc = main()
    sys.stdout.flush()
    os._exit(rc)
