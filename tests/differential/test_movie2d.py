"""A movie and a line in a dialog against the original x86 (docs/twod.md, Movies in a dialog; Lines in a
dialog). No shipped dialog has either, so both are ported for preservation.

    cmake --build build --target mw3probe && .venv/bin/python tests/differential/test_movie2d.py [runs]

Movie2D (vtable 0x5d0e28): random states (place, flags, timer, time shown, colour key, a stream or none
with its size, flags, length (some 0: the exe divides by it, never generated there) and rate, a
background or none with its size and rect), then random operations: the start 0x5a7d10, the colour key
0x5a7c30, the update 0x5a7c50 (through Object2D's 0x59ffb0, which draws when not managed), the draw
0x5a7ca0 and the erase 0x5a7cd0. The stream's frame step 0x59aaf0 and the blit 0x55b1f0 are stubs on the
exe's side and the test host's on the port's: every call compared, then the object's fields.

Line2D's reader 0x5a6a10 (with Object2D's 0x59feb0 and zReader's find 0x5569e0): random entries with or
without COLOR, ENDP_REL, ENDP_ABS and POSITION, the line made at a random place; compared: the result, the
start, the end and the colour (565). Lists shorter than their values are not generated (the exe reads
past them; the port's FIX (A) reads 0).
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import load_probe  # noqa: E402
from original import FTOL, IAT_FTOL, Original  # noqa: E402
from ai_world import ser  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

U, I = ctypes.c_uint32, ctypes.c_int32
MOVIE_VT, LINE_VT = 0x5d0e28, 0x5d06e0


def fb(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def s32(v):
    return struct.unpack('<i', struct.pack('<I', v & 0xffffffff))[0]


class Emulated:
    def __init__(self):
        self.cpu = cpu = Original()
        cpu.fpu_control(0x007f)  # play precision (MW3_PRECISION=53: the reference runtime's)
        cpu.import_code(IAT_FTOL, FTOL)
        for a, v in ((0x802738, 8), (0x80273c, 3), (0x802740, 3), (0x802744, 0xf8), (0x802748, 0xfc)):
            cpu.u32(a, v)  # 565, as test_screen_tint.py sets it
        cpu.stub_function(0x59aaf0, self.frame, 4)
        cpu.stub_function(0x55b1f0, self.blit, 0xc)
        self.obj, self.stream, self.bg = cpu.alloc(0x144), cpu.alloc(0x1e4), cpu.alloc(0x38)
        self.rect = cpu.alloc(0x10)
        self.calls = []

    def r32(self, a):
        return struct.unpack('<I', self.cpu.read(a, 4))[0]

    def frame(self, c):
        self.calls += [1, c.arg(0)]
        return 0

    def blit(self, c):
        which = 1 if c.ecx == self.stream else 2 if c.ecx == self.bg else 0
        rect = c.arg(2)
        r = list(struct.unpack('<4I', c.read(rect, 16))) if rect else [0, 0, 0, 0]
        self.calls += [2, which, c.edx, c.arg(0), c.arg(1) & 0xffff, int(rect != 0)] + r
        return 0

    def movie(self, st, ops):
        cpu, o = self.cpu, self.obj
        cpu.write(o, bytes(0x144))
        cpu.u32(o, MOVIE_VT)
        for off, v in ((0x14, st[0]), (0x18, st[1]), (0x0c, st[2]), (0x10, st[3]), (0x38, st[4]), (0x3c, st[5])):
            cpu.u32(o + off, v)
        cpu.write(self.stream, bytes(0x1e4))
        if st[6]:
            cpu.write(self.stream + 4, struct.pack('<hhBB', st[7], st[8], 0, st[9]))
            cpu.u32(self.stream + 0x4c, st[10]), cpu.u32(self.stream + 0xec, st[11])
            cpu.u32(o + 0x34, self.stream)
        cpu.write(self.bg, bytes(0x38))
        if st[12]:
            cpu.write(self.bg + 4, struct.pack('<hh', st[13], st[14]))
            cpu.u32(o + 0x1c, self.bg)
        cpu.write(o + 0x20, struct.pack('<4i', *[s32(v) for v in st[15:19]]))
        out = []
        for op, a in ops:
            self.calls = []
            if op == 1:
                cpu.call(0x5a7d10, ecx=o)
            elif op == 2:
                cpu.call(0x5a7c30, ecx=o, stack_args=(a,))
            elif op == 3:
                cpu.call(0x5a7c50, ecx=o, stack_args=(a,))
            elif op == 4:
                cpu.call(0x5a7ca0, ecx=o)
            else:
                cpu.call(0x5a7cd0, ecx=o)
            out += [len(self.calls)] + self.calls
            out += [self.r32(o + off) for off in (0x14, 0x18, 0x0c, 0x10, 0x38)]
            out += [self.r32(o + 0x3c) & 0xffff] + [self.r32(o + 0x20 + 4 * i) for i in range(4)]
            out.append(self.cpu.read(self.stream + 9, 1)[0] if st[6] else 0)
        return out

    def line(self, items, x0, y0, x1, y1):
        cpu, o = self.cpu, self.obj
        cpu.write(o, bytes(0x4c))
        cpu.call(0x5a67e0, ecx=o, stack_args=(x0 & 0xffffffff, y0 & 0xffffffff, x1 & 0xffffffff, y1 & 0xffffffff, 0))
        tree = cpu.alloc(8)
        put_node(cpu, tree, items)
        cpu.call(0x5a6a10, ecx=o, stack_args=(tree, 0))
        r = cpu.uc.reg_read(UC_X86_REG_EAX) & 0xff
        return [r] + [self.r32(o + off) for off in (0x14, 0x18, 0x34, 0x38, 0x3c)]


def put_node(cpu, a, v):
    if isinstance(v, list):
        items = cpu.alloc(8 * (len(v) + 1))
        cpu.u32(items, 1)
        cpu.u32(items + 4, len(v) + 1)
        for i, x in enumerate(v):
            put_node(cpu, items + 8 * (i + 1), x)
        cpu.u32(a, 4)
        cpu.u32(a + 4, items)
    elif isinstance(v, str):
        s = cpu.alloc(len(v) + 1)
        cpu.write(s, v.encode('latin-1') + b'\0')
        cpu.u32(a, 3)
        cpu.u32(a + 4, s)
    elif isinstance(v, float):
        cpu.u32(a, 2)
        cpu.u32(a + 4, fb(v))
    else:
        cpu.u32(a, 1)
        cpu.u32(a + 4, v & 0xffffffff)


def movie_case(r):
    has_stream = r.random() < 0.85
    st = [r.randint(-40, 600), r.randint(-40, 400), r.choice([0x00, 0x10, 0x01, 0x11, 0x1c, 0x02, 0x08]),
          fb(r.choice([0.0, 0.5, -1.0, 2.0])), fb(r.choice([0.0, r.uniform(0, 30), 1.0 / 3])), r.randrange(0x10000),
          int(has_stream), r.randint(1, 640), r.randint(1, 480), r.choice([0, 2, 1, 0x80]),
          r.choice([1, 2, 15, 120, 1000]), r.choice([0, 1, 10, 15, 24, 30]),
          int(r.random() < 0.6), r.randint(1, 800), r.randint(1, 600)]
    st += [r.randint(-10, 300), r.randint(-10, 300), r.randint(0, 800), r.randint(0, 600)]
    ops = []
    for _ in range(r.randint(1, 10)):
        op = r.choice([1, 2, 3, 3, 3, 4, 5])
        a = r.randrange(0x10000) if op == 2 else fb(r.choice([0.0, 1 / 30, r.uniform(0, 0.2), 0.5])) if op == 3 else 0
        ops.append((op, a))
    return st, ops


def line_case(r):
    items = []
    if r.random() < 0.6:
        items += ['COLOR', [r.randrange(256) for _ in range(3)]]
    if r.random() < 0.5:
        items += ['ENDP_REL', [r.randint(-200, 200), r.randint(-200, 200)]]
    if r.random() < 0.5:
        items += ['ENDP_ABS', [r.randint(-50, 700), r.randint(-50, 500)]]
    if r.random() < 0.6:
        items += ['POSITION', [r.randint(-50, 700), r.randint(-50, 500)]]
    if r.random() < 0.2:
        items += ['OTHER', [1]]
    return items, [r.randint(-100, 700) for _ in range(4)]


def main():
    probe = load_probe()
    P = ctypes.POINTER
    probe.probe_movie2d.argtypes = [P(U), P(U), I, P(U), I]
    probe.probe_movie2d.restype = I
    probe.probe_line2d_read.argtypes = [ctypes.c_char_p, I, I, I, I, I, P(U)]
    emu = Emulated()
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 2000
    r = random.Random(0x5a7c50)
    seen = {'ops': 0, 'frames': 0, 'blits': 0, 'keyed': 0, 'lines': 0, 'rel': 0, 'abs': 0, 'color': 0}
    for i in range(runs):
        st, ops = movie_case(r)
        if st[6] and st[10] == 0:
            st[10] = 1
        want = emu.movie(st, ops)
        buf = (U * 4096)()
        flat = [w for o in ops for w in o]
        n = probe.probe_movie2d((U * 19)(*[v & 0xffffffff for v in st]), (U * len(flat))(*flat), len(ops), buf, 4096)
        got = list(buf[:n])
        if want != got:
            k = next((j for j in range(min(len(want), len(got))) if want[j] != got[j]), min(len(want), len(got)))
            print(f'movie {i}: differs at word {k}; state {st}, ops {ops}')
            print('  original', want[max(0, k - 4):k + 8])
            print('  port    ', got[max(0, k - 4):k + 8])
            raise SystemExit(1)
        seen['ops'] += len(ops)
        j = 0
        for _ in ops:
            c = want[j]
            calls = want[j + 1:j + 1 + c]
            q = 0
            while q < len(calls):
                if calls[q] == 1:
                    seen['frames'] += 1
                    q += 2
                else:
                    seen['blits'] += 1
                    seen['keyed'] += calls[q + 1] == 1
                    q += 10
            j += 1 + c + 11
    for i in range(runs):
        items, xy = line_case(r)
        want = emu.line(items, *xy)
        data = ser(items)
        out = (U * 6)()
        probe.probe_line2d_read(data, len(data), *xy, out)
        got = list(out)
        if want != got:
            print(f'line {i}: original {want}, port {got}; {items} at {xy}')
            raise SystemExit(1)
        seen['lines'] += 1
        seen['rel'] += 'ENDP_REL' in items
        seen['abs'] += 'ENDP_ABS' in items
        seen['color'] += 'COLOR' in items
    print(f'movie2d and line2d: {runs} movies and {runs} lines identical; {seen}')
    if runs >= 2000 and min(seen.values()) < runs // 20:
        print('too little reached')
        raise SystemExit(1)


if __name__ == '__main__':
    main()
