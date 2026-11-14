"""Pirate's Moon's menu background and title, against PM's exe: the background's update 0x4885b0
(MW3's ScrollImage 0x40a210 moved a pixel a frame; PM 22 pixels a second, rounded, the fraction
kept, the view wrapped at the image and cut into four pieces with IntersectRect) and the title's
0x4883a0 (MW3's 0x42fb30 slid 2 pixels a frame; PM 50 a second). Runs of frames with random times
(0 to 0.5 s, many near 1/30 and 1/60), images smaller and larger than the screen, the view
anywhere, moving or not, carries to start; the title from any x, FastMenu or not. Compared after
every frame: the carries' bits, the view, each piece's flag, place and rect; the title's place.
User32's rect calls are Python stand-ins; Object2D::update (0x5c0220) is stubbed (the probe's
object hidden, so its update draws nothing); the x87 at 53 bits.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_menu_scroll.py [runs]
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

os.environ['MW3_EXE'] = 'pm'  # pm_iat reads the exe this names
# The menus at the reference runtime's 53 bits (CLAUDE.md, FPU precision); at 24 the kept fractions
# come out an ulp apart now and then, the port computing them in double.
os.environ.setdefault('MW3_X87', '53')
sys.path.insert(0, str(Path(__file__).parent))
import test_pm_zar  # noqa: E402
from original import FTOL, pm_iat  # noqa: E402

probe = test_pm_zar.load_pm_probe()
I32, F32 = ctypes.POINTER(ctypes.c_int32), ctypes.POINTER(ctypes.c_float)
probe.probe_pm_scroll.argtypes = [ctypes.c_int32] * 5 + [I32, F32, F32, ctypes.c_int32, I32]
probe.probe_pm_marquee.argtypes = [ctypes.c_int32] * 4 + [ctypes.c_float, ctypes.c_int32, F32, ctypes.c_int32, I32]
SCROLL, MARQUEE, SCREEN_W, SCREEN_H = 0x4885b0, 0x4883a0, 0x62d878, 0x62d87c


def fbits(f):
    return struct.unpack('<i', struct.pack('<f', f))[0]


def f32(f):
    return struct.unpack('<f', struct.pack('<f', f))[0]


class Exe:
    def __init__(self):
        self.e = e = test_pm_zar.Emulated()
        cpu = self.cpu = e.cpu
        rd = lambda a: struct.unpack('<4i', bytes(cpu._uc.mem_read(a, 16)))  # noqa: E731
        wr = lambda a, r: cpu._uc.mem_write(a, struct.pack('<4i', *r))  # noqa: E731
        empty = lambda r: r[2] <= r[0] or r[3] <= r[1]  # noqa: E731

        def intersect(c):
            a, b = rd(c.arg(1)), rd(c.arg(2))
            r = (max(a[0], b[0]), max(a[1], b[1]), min(a[2], b[2]), min(a[3], b[3]))
            wr(c.arg(0), (0, 0, 0, 0) if empty(r) else r)
            return 0 if empty(r) else 1

        def offset(c):
            r = rd(c.arg(0))
            dx, dy = struct.unpack('<2i', struct.pack('<2I', c.arg(1), c.arg(2)))
            wr(c.arg(0), (r[0] + dx, r[1] + dy, r[2] + dx, r[3] + dy))
            return 1
        def imp(name, fn, pop):  # PM's own import slot (stub_import maps MW3's) to a stub
            code = cpu.code(b'\xc2' + struct.pack('<H', pop))
            e.native_stub(code, fn, pop)
            cpu._uc.mem_write(pm_iat(name), struct.pack('<I', code))
        cpu._uc.mem_write(0x5de644, struct.pack('<I', cpu.code(FTOL)))  # MSVCRT's _ftol, 0x5cef9a's import
        imp('IntersectRect', intersect, 12)
        imp('SetRect', lambda c: wr(c.arg(0), [struct.unpack('<i', struct.pack('<I', c.arg(i)))[0]
                                              for i in (1, 2, 3, 4)]) or 1, 20)
        imp('IsRectEmpty', lambda c: int(empty(rd(c.arg(0)))), 4)
        imp('OffsetRect', offset, 12)
        e.native_stub(0x5c0220, lambda c: 0, 4)
        self.fast = 0
        e.native_stub(0x40ac30, lambda c: self.fast)
        self.obj = e.alloc(0x200)
        self.tex = e.alloc(0x40)
        # The title's virtuals: set_pos +0x0c, get_x +0x68, get_y +0x6c, on the object's x, y (+0x10, +0x14).
        vt = e.alloc(0x100)
        for slot, fn, pop in ((0x0c, self.set_pos, 8), (0x68, lambda c: self.r(self.obj + 0x10), 0),
                              (0x6c, lambda c: self.r(self.obj + 0x14), 0)):
            code = cpu.code(b'\xc3' if not pop else b'\xc2' + struct.pack('<H', pop))
            e.native_stub(code, fn, pop)
            self.w(vt + slot, code)
        self.vt = vt
        self.w_screen = 640
        e.native_stub(0x40b670, lambda c: self.w_screen)

    def r(self, a):
        return struct.unpack('<i', bytes(self.cpu._uc.mem_read(a, 4)))[0]

    def w(self, a, v):
        self.cpu._uc.mem_write(a, struct.pack('<i', v))

    def wf(self, a, v):
        self.cpu._uc.mem_write(a, struct.pack('<f', v))

    def set_pos(self, c):
        self.w(self.obj + 0x10, struct.unpack('<i', struct.pack('<I', c.arg(0)))[0])
        self.w(self.obj + 0x14, struct.unpack('<i', struct.pack('<I', c.arg(1)))[0])
        return 0

    def scroll(self, iw, ih, w, h, moving, view, carry, dts):
        o, u = self.obj, self.cpu._uc
        u.mem_write(o, b'\0' * 0x200)
        u.mem_write(self.tex + 4, struct.pack('<hh', iw, ih))
        self.w(o + 0x3c, self.tex)
        self.wf(o + 0xbc, 22.0), self.wf(o + 0xc0, 22.0), self.wf(o + 0xc4, carry[0]), self.wf(o + 0xc8, carry[1])
        u.mem_write(o + 0xf0, struct.pack('<4i', *view))
        u.mem_write(o + 0x140, bytes([moving]))
        self.w(SCREEN_W, w), self.w(SCREEN_H, h - 1)
        out = []
        for dt in dts:
            u.mem_write(o + 0xcc, b'\0' * 4)
            self.e.call(SCROLL, ecx=o, args=[fbits(dt) & 0xffffffff])
            m = bytes(u.mem_read(o + 0xc4, 0x80))
            f = list(struct.unpack('<2i', m[:8]))
            f += list(struct.unpack('<4i', m[0xf0 - 0xc4:0x100 - 0xc4]))
            f += list(m[0xcc - 0xc4:0xd0 - 0xc4])
            f += list(struct.unpack('<8i', m[0xd0 - 0xc4:0xf0 - 0xc4]))
            f += list(struct.unpack('<16i', m[0x100 - 0xc4:0x140 - 0xc4]))
            out.append(f)
        return out

    def marquee(self, iw, w, x, y, carry, fast, dts):
        o, u = self.obj, self.cpu._uc
        u.mem_write(o, b'\0' * 0x200)
        self.w(o, self.vt)
        u.mem_write(self.tex + 4, struct.pack('<hh', iw, 20))
        self.w(o + 0x3c, self.tex)
        self.w(o + 0x10, x), self.w(o + 0x14, y)
        self.wf(o + 0xbc, 50.0), self.wf(o + 0xc0, carry)
        self.fast, self.w_screen = fast, w
        out = []
        for dt in dts:
            self.e.call(MARQUEE, ecx=o, args=[fbits(dt) & 0xffffffff])
            out.append([self.r(o + 0x10), self.r(o + 0x14), self.r(o + 0xc0)])
        return out


def dts_for(rnd, n):
    pick = lambda: rnd.choice((1 / 30, 1 / 60, 1 / 30, rnd.uniform(0, 0.1), rnd.uniform(0, 0.5), 0.0, 0.033))  # noqa: E731
    return [f32(pick()) for _ in range(n)]


def main():
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 300
    rnd = random.Random(5)
    exe = Exe()
    bad = frames = 0
    for r in range(runs):
        n = rnd.randint(1, 40)
        dts = dts_for(rnd, n)
        if r % 3:
            w, h = rnd.choice(((640, 480), (853, 480), (800, 600)))
            iw, ih = rnd.choice(((1024, 1024), (640, 480), (700, 520), (512, 512), (2048, 768)))
            left, top = rnd.randint(0, iw - 1), rnd.randint(0, ih - 1)
            view = (left, top, left + w, top + h - 1)
            carry = (f32(rnd.uniform(-0.5, 0.5)), f32(rnd.uniform(-0.5, 0.5)))
            moving = rnd.choice((1, 1, 1, 0))
            theirs = exe.scroll(iw, ih, w, h, moving, view, carry, dts)
            out = (ctypes.c_int32 * (34 * n))()
            probe.probe_pm_scroll(iw, ih, w, h, moving, (ctypes.c_int32 * 4)(*view), (ctypes.c_float * 2)(*carry),
                                  (ctypes.c_float * n)(*dts), n, out)
            ours = [list(out[34 * f:34 * f + 34]) for f in range(n)]
            what = f'scroll image {iw}x{ih} screen {w}x{h} moving {moving} view {view} carry {carry}'
        else:
            w = rnd.choice((640, 853))
            iw = rnd.choice((300, 600, 900))
            x, y = rnd.randint(-iw, w), rnd.randint(0, 400)
            carry, fast = f32(rnd.uniform(-0.5, 0.5)), rnd.choice((0, 0, 0, 1))
            theirs = exe.marquee(iw, w, x, y, carry, fast, dts)
            out = (ctypes.c_int32 * (3 * n))()
            probe.probe_pm_marquee(iw, w, x, y, carry, fast, (ctypes.c_float * n)(*dts), n, out)
            ours = [list(out[3 * f:3 * f + 3]) for f in range(n)]
            what = f'marquee image {iw} screen {w} at {x},{y} carry {carry} fast {fast}'
        frames += n
        k = next((f for f in range(n) if theirs[f] != ours[f]), None)
        if k is not None:
            bad += 1
            if bad <= 4:
                print(f'run {r}: {what}\n  frame {k} dt {dts[k]}\n  exe  {theirs[k]}\n  port {ours[k]}')
    print(f'{runs} runs, {frames} frames: {bad} runs differ')
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    code = 0
    try:
        main()
    except SystemExit as ex:
        code = 0 if ex.code in (None, 0) else 1
    except BaseException:
        import traceback
        traceback.print_exc()
        code = 1
    sys.stdout.flush()
    import os
    os._exit(code)
