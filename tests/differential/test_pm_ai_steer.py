"""PM's steering against the exe: a plan of steps on one (0x4dc0f0's) with a vehicle, contacts and their
places made here: set 0x4dc7a0, the frame 0x4dcb30, the measure 0x4dc9d0 with the push 0x4dc480,
0x4dc400, near 0x4dc210, the end 0x4dc270 and the carrot 0x4dc290. Each step's fields compared.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_ai_steer.py [runs]
"""
import ctypes
import math
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import pm_floors  # noqa: E402
import test_pm_zar  # noqa: E402

CTOR, SET, UPDATE, MEASURE, ADVANCE, NEAR, END, CARROT = 0x4dc0f0, 0x4dc7a0, 0x4dcb30, 0x4dc9d0, 0x4dc400, 0x4dc210, 0x4dc270, 0x4dc290
PLACE, ORDER, COPYSIGN = 0x5cb260, 0x4fa470, 0x5de5bc


def bits(v):
    return struct.unpack('<I', struct.pack('<f', v))[0]


class Exe(test_pm_zar.Emulated):
    def __init__(self):
        super().__init__()
        self.places, self.halt, self.steers = {}, False, True
        # _copysign(x, y): fld x; fabs; y's sign; fchs
        self.w32(COPYSIGN, self.cpu.code(bytes.fromhex('dd442404 d9e1 f644241380 7402 d9e0 c3')))
        self.steer = self.alloc(0xc0)
        self.vehicle = self.alloc(0x100)
        self.vt = self.alloc(0x200)
        self.w32(self.vehicle, self.vt)
        self.places[self.vehicle] = self.alloc(12)
        self.native_stub(PLACE, lambda c: self.places[c.ecx])
        self.native_stub(ORDER, lambda c: 1 if self.steers else 0)
        self.w32(self.vt + 0x150, self.native_code(lambda c: 1 if self.halt else 0))
        self.list = self.alloc(4 * 16)
        self.w32(self.vehicle + 0xac, self.list)
        self.w32(self.vehicle + 0xb0, self.list)
        self.entity_vt = {}
        self.call(CTOR, ecx=self.steer)
        self.w32(self.steer + 0x9c, 0)
        self.n = 0

    def w32(self, a, v):
        self.cpu._uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def native_code(self, fn):
        a = self.cpu.code(b'\xc3')
        self.native_stub(a, fn)
        return a

    def fs(self, a, vals):
        self.cpu._uc.mem_write(a, struct.pack(f'<{len(vals)}f', *vals))

    def contact(self, e, target, dist):
        """The contact's +4 an object whose vt+0x2c answers the entity; the entity's vt+0x48 its target."""
        c, holder, hvt, evt = self.alloc(0x20), self.alloc(8), self.alloc(0x40), self.alloc(0x60)
        self.w32(holder, hvt)
        self.w32(hvt + 0x2c, self.native_code(lambda _c, v=e: v))
        if e:
            self.w32(e, evt)
        self.w32(evt + 0x48, self.native_code(lambda _c, v=target: v))
        self.w32(c + 4, holder)
        self.fs(c + 0xc, [dist])
        r = struct.unpack('<I', bytes(self.cpu._uc.mem_read(self.vehicle + 0xb0, 4)))[0]
        self.w32(r, c)
        self.w32(self.vehicle + 0xb0, r + 4)

    def words(self):
        def g(o):
            return struct.unpack('<I', bytes(self.cpu._uc.mem_read(self.steer + o, 4)))[0]
        return ' '.join(f'{g(o):x}' for o in (4, 8, 0xc, 0x10, 0x14, 0x18, 0x1c, 0x20, 0x24, 0x28, 0x2c, 0x34, 0x38)) + f' {g(0x9c)}\n'


def plan(seed):
    r = random.Random(f'steer:{seed}')
    near = r.choice([10, 40, 80, 300])

    def pt():
        return [r.uniform(-near, near), r.uniform(-5, 5), r.uniform(-near, near)]
    lines = [('P', 1, pt()), ('F', [r.uniform(-1, 1), r.uniform(-1, 1)])]
    es = [r.randrange(2, 9) for _ in range(r.randrange(0, 5))]
    for e in es:
        lines.append(('P', e, pt()))
    for e in es:
        lines.append(('C', e if r.random() > 0.1 else 0, r.choice([1, 1, 1, 0]), r.uniform(0, near * near * 2)))
    for _ in range(r.randrange(4, 16)):
        k = r.random()
        if r.random() < 0.08:
            lines.append(('T', r.uniform(-20, 20)))
        if es and r.random() < 0.25:  # a contact just ahead of or behind the vehicle, near the line
            lines.append(('Q', r.choice(es), r.uniform(-45, 45), r.uniform(-20, 20)))
        if k < 0.25:
            to = pt() if r.random() > 0.1 else None
            fr = pt()
            lines.append(('S', to or fr, fr, r.choice([0.0, 1.0, 5.0, 20.0])))
        elif k < 0.55:
            lines.append(('U', r.choice([0.0, 0.03, 0.033, 0.1])))
        elif k < 0.65:
            lines.append(('M',))
        elif k < 0.72:
            lines.append(('A', r.uniform(-30, 30)))
        elif k < 0.8:
            lines.append(('P', 1, pt()))
        elif k < 0.86:
            lines.append(('H', r.choice([0, 0, 0, 1]), r.choice([1, 1, 0])))
        elif k < 0.92:
            lines.append(('K', r.choice([1, 1, 0]), r.choice([1, 1, 0])))
        elif k < 0.96:
            lines.append(('N',))
        else:
            lines.append(('R',))
    return lines


def place_near(x, ln):
    def f(o):
        return struct.unpack('<f', bytes(x.cpu._uc.mem_read(x.steer + o, 4)))[0]
    v = struct.unpack('<3f', bytes(x.cpu._uc.mem_read(x.places[x.vehicle], 12)))
    return [v[0] + f(0x10) * ln[2] + f(0x14) * ln[3], v[1], v[2] + f(0x14) * ln[2] - f(0x10) * ln[3]]


def run_exe(x, lines):
    placed.clear()
    out = []
    ids = {1: x.vehicle}

    def ent(i):
        if i == 0:
            return 0
        if i not in ids:
            ids[i] = x.alloc(0x100)
            x.places[ids[i]] = x.alloc(12)
        return ids[i]
    for ln in lines:
        op = ln[0]
        if op == 'T':
            x.fs(x.steer + 0x28, [ln[1]])
        elif op == 'Q':  # by the steering's own measure, so both sides place it alike
            ln = ln + (place_near(x, ln),)
            x.fs(x.places[ent(ln[1])], ln[4])
            placed.append(ln[4])
        elif op == 'P':
            x.fs(x.places[ent(ln[1])], ln[2])
        elif op == 'F':
            x.fs(x.vehicle + 0x24, [ln[1][0]])
            x.fs(x.vehicle + 0x2c, [ln[1][1]])
        elif op == 'H':
            x.halt, x.steers = bool(ln[1]), bool(ln[2])
        elif op == 'K':
            x.cpu._uc.mem_write(x.steer + 0x48, bytes([ln[1]]))
            x.cpu._uc.mem_write(x.steer + 0x58, bytes([ln[2]]))
        elif op == 'C':
            x.contact(ent(ln[1]), ln[2], ln[3])
        elif op == 'S':
            a, b = x.alloc(12), x.alloc(12)
            x.fs(a, ln[1])
            x.fs(b, ln[2])
            x.call(SET, ecx=x.steer, args=(a, b, bits(ln[3]), x.vehicle))
        elif op == 'U':
            x.call(UPDATE, ecx=x.steer, args=(bits(ln[1]), x.vehicle))
        elif op == 'M':
            x.call(MEASURE, ecx=x.steer, args=(0, x.vehicle))
        elif op == 'A':
            x.call(ADVANCE, ecx=x.steer, args=(bits(ln[1]),))
        elif op == 'N':
            n = x.call(NEAR, ecx=x.steer) & 0xff
            e = x.call(END, ecx=x.steer) & 0xff
            out.append(('near ' if n else 'far ') + ('end\n' if e else 'on\n'))
        elif op == 'R':
            a = x.alloc(12)
            x.call(CARROT, ecx=x.steer, args=(a,))
            out.append(' '.join(f'{v:x}' for v in struct.unpack('<3I', bytes(x.cpu._uc.mem_read(a, 12)))) + '\n')
        out.append(x.words())
    return ''.join(out)


placed = []


def text(lines):
    o = []
    at = iter(placed)
    for ln in lines:
        op = ln[0]
        if op == 'T':
            o.append(f'T {bits(ln[1]):x}')
        elif op == 'Q':
            o.append(f'P {ln[1]} ' + ' '.join(f'{bits(v):x}' for v in next(at)))
        elif op == 'P':
            o.append(f'P {ln[1]} ' + ' '.join(f'{bits(v):x}' for v in ln[2]))
        elif op == 'F':
            o.append('F ' + ' '.join(f'{bits(v):x}' for v in ln[1]))
        elif op in ('H', 'K'):
            o.append(f'{op} {ln[1]} {ln[2]}')
        elif op == 'C':
            o.append(f'C {ln[1]} {ln[2]} {bits(ln[3]):x}')
        elif op == 'S':
            o.append('S ' + ' '.join(f'{bits(v):x}' for v in ln[1] + ln[2] + [ln[3]]))
        elif op in ('U', 'A'):
            o.append(f'{op} {bits(ln[1]):x}')
        else:
            o.append(op)
    return '\n'.join(o) + '\n'


def close(a, b):
    if a == b:
        return True
    x, y = (struct.unpack('<f', struct.pack('<I', int(v, 16)))[0] for v in (a, b))
    return abs(x - y) <= 1e-3 + 1e-4 * max(abs(x), abs(y))


def main():
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 300
    lib = test_pm_zar.load_pm_probe()
    lib.probe_pm_ai_steer.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
    buf = ctypes.create_string_buffer(1 << 20)
    bad = steps = 0
    for seed in range(runs):
        lines = plan(seed)
        x = Exe()
        a = run_exe(x, lines).split('\n')
        n = lib.probe_pm_ai_steer(text(lines).encode(), buf, len(buf))
        assert n >= 0
        b = buf.value.decode().split('\n')
        steps += len(a)
        for i, (p, q) in enumerate(zip(a, b)):
            pw, qw = p.split(), q.split()
            floats = {14: 13, 3: 3, 2: 0}.get(len(pw), 0)  # a step's fields and count, the carrot, near and end
            if len(pw) != len(qw) or pw[floats:] != qw[floats:] or not all(
                    u == v or close(u, v) for u, v in zip(pw[:floats], qw[:floats])):
                print(f'seed {seed} line {i}:\n  exe  {p}\n  port {q}')
                bad += 1
                break
        else:
            if len(a) != len(b):
                print(f'seed {seed}: {len(a)} lines against {len(b)}')
                bad += 1
        if bad >= 6:
            break
    print(f'{runs} plans, {steps} lines: {bad} differ')
    pm_floors.floors(plans=(runs, 100), lines=(steps, 1000))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
