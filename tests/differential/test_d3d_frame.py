"""The screen tint and the Direct3D frame's lock against the original
(docs/render-frame.md).

    cmake --build build && .venv/bin/python tests/differential/test_d3d_frame.py

In Unicorn at the 24-bit x87 precision of play, against the port through
tests/probe_frame.cpp:
- the tint 0x558540 in both video modes: the software state it keeps
  (0x7fb2dc..) or, in Direct3D, the quad 0x57dd80 fills (run natively);
- the software finish 0x558610 on random 16-bit buffers, with every row
  routine (scalar and MMX, RGB555 and RGB565), random rects reaching outside
  the buffer, colours and alphas: the buffer and the constants it leaves. The
  scalar rows' extra pair past the buffer's end is the port's FIX (bug):
  compared inside the buffer, the original's write past it counted;
- the 3D lock 0x576d00 / 0x576d60 with the surface lock 0x577590 /
  0x577600 under it, DirectDraw's Lock and Unlock stubbed: the record, the
  fillers', finish and text targets it sets, and the Lock and Unlock counts.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import FTOL, IAT_FTOL, Original  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
probe = ctypes.CDLL(str(ROOT / 'build' / ('libmw3probe.dylib' if sys.platform == 'darwin' else 'libmw3probe.so')))
probe.probe_frame_finish.restype = ctypes.c_uint32

FORMATS = ((5, 6, 5, 6), (5, 5, 5, 5))
TINT = 0x7fb2dc
FINISH_T = 0x72c1e4
BACK = 0x8027a4


def s32(x):
    return struct.unpack('<i', struct.pack('<I', x & 0xffffffff))[0]


class Emu:
    def __init__(self):
        c = self.cpu = Original()
        c.import_code(IAT_FTOL, FTOL)
        c.fpu_control(0x7f)
        self.cpuinfo = c.alloc(16)
        c.u32(0x8008d8, self.cpuinfo)
        self.rect = c.alloc(16)
        self.buf = c.alloc(0x200000)
        self.desc_fill = (0, 0, 0, 0)
        self.lock_result = 0
        self.locks = self.unlocks = 0
        c.stub_function(0x577630, self.lock, 0)
        c.stub_function(0x577690, self.unlock, 0)
        c.u32(0x809340, 0x577590)
        c.u32(0x80933c, 0x577600)

    def lock(self, c):
        self.locks += 1
        if self.lock_result:
            return self.lock_result
        w, h, p, px = self.desc_fill
        d = c.edx
        c.write(d + 8, struct.pack('<III', h, w, p))
        c.u32(d + 0x24, px)
        return 0

    def unlock(self, c):
        self.unlocks += 1
        return 0

    def format(self, r, g, b, mmx, fmt2d):
        c = self.cpu
        rs, gs, bsr = r + g + b - 8, g + b - 8, 8 - b
        rm, gm, bm = ((1 << r) - 1) << (8 - r), ((1 << g) - 1) << (8 - g), ((1 << b) - 1) << (8 - b)
        c.write(0x80272c, struct.pack('<3I', rm << rs, gm << gs, bm >> bsr))
        c.write(0x802738, struct.pack('<3I', rs, gs, bsr))
        c.write(0x802744, struct.pack('<3I', rm, gm, bm))
        c.write(self.cpuinfo, bytes([4 if mmx else 0]))
        c.u32(0x74a738, fmt2d)

    def r32(self, a):
        return struct.unpack('<I', bytes(self.cpu.read(a, 4)))[0]


def fmt_setup(rng, emu):
    f = rng.choice(FORMATS)
    mmx = rng.random() < 0.6
    emu.format(*f[:3], mmx, f[3])
    probe.probe_frame_format(*f[:3], int(mmx), f[3])
    return f, mmx


def rand_alpha(rng):
    a = rng.choice([0.0, 1.0, 0.5, rng.random(), 0.999, 1.0 / 3.0, 0.0625])
    if rng.random() < 0.7:  # the fade's alphas are floats
        a = struct.unpack('<f', struct.pack('<f', a))[0]
    return struct.unpack('<II', struct.pack('<d', a))


def test_tint(rng, emu, runs=3000):
    c = emu.cpu
    for n in range(runs):
        fmt_setup(rng, emu)
        w, h = rng.choice([(640, 480), (320, 240), (800, 600), (17, 3)])
        c.write(FINISH_T, struct.pack('<5i', 0, w, h, w * 2, w))
        probe.probe_frame_finish_target(None, w, h, w * 2)
        colour = rng.getrandbits(16) | (rng.getrandbits(16) << 16 if rng.random() < 0.2 else 0)
        has = int(rng.random() < 0.5)
        rect = [rng.randint(-20, 700), rng.randint(-20, 500), rng.randint(-20, 700), rng.randint(-20, 500)]
        c.write(emu.rect, struct.pack('<4i', *rect))
        lo, hi = rand_alpha(rng)
        d3d = int(rng.random() < 0.5)
        c.u32(0x72f348, d3d)
        c.call(0x558540, ecx=colour, edx=emu.rect if has else 0, stack_args=(lo, hi))
        probe.probe_frame_tint(colour, has, (ctypes.c_int32 * 4)(*rect), lo, hi, d3d)
        if d3d:
            want = [emu.r32(0x8095bc)] + list(struct.unpack('<32I', bytes(c.read(0x8095c0, 128))))
            got = (ctypes.c_uint32 * 33)()
            probe.probe_frame_quad(got)
            what = 'quad'
        else:
            want = list(struct.unpack('<6I', bytes(c.read(TINT, 24)))) + list(struct.unpack('<2I', bytes(c.read(0x7fb2f8, 8))))
            got = (ctypes.c_uint32 * 8)()
            probe.probe_frame_tint_get(got)
            what = 'software tint'
        if want != list(got):
            raise SystemExit(f'tint {n} ({what}, colour {colour:#x}, rect {rect if has else None}, d3d {d3d}): '
                             f'original {[hex(x) for x in want]}\n  rebuilt {[hex(x) for x in got]}')
    return f'{runs} tints identical (software state, Direct3D quad)'


def test_finish(rng, emu, runs=600):
    c = emu.cpu
    rows = {0: 0, 1: 0, 2: 0, 3: 0}
    past = 0
    for n in range(runs):
        f, mmx = fmt_setup(rng, emu)
        w = rng.choice([640, 320, 17, 6, 1, 33])
        h = rng.choice([480, 2, 9, 1, 40])
        if w * h > 640 * 480:
            h = 480
        pitch = w * 2 + rng.choice([0, 0, 4, 64])
        size = pitch * h
        data = bytes(rng.getrandbits(8) for _ in range(size)) if size < 40000 else os.urandom(size)
        guard = 64
        c.write(emu.buf, data + b'\xa5' * guard)
        buf = (ctypes.c_uint16 * (size // 2 + guard))()
        ctypes.memmove(buf, data, size)
        c.write(FINISH_T, struct.pack('<5i', emu.buf, w, h, pitch, pitch // 2))
        probe.probe_frame_finish_target(buf, w, h, pitch)
        state = [int(rng.random() < 0.9), rng.choice([0, 0, -3, 1, w // 2]), rng.choice([0, 0, -1, 1, h // 2]),
                 rng.choice([w - 1, w - 1, w, w + 10, w // 3, 0]), rng.choice([h, h, h + 5, h // 2, 0]),
                 rng.getrandbits(16)]
        # A right edge left of the left one runs the original's rows wild (a crash): not tested.
        state[3] = max(state[3], max(state[1], 0))
        lo, hi = rand_alpha(rng)
        st = state + [lo, hi]
        c.write(TINT, struct.pack('<6I', *[x & 0xffffffff for x in state]) + bytes(4) + struct.pack('<II', lo, hi))
        probe.probe_frame_tint_set((ctypes.c_uint32 * 8)(*[x & 0xffffffff for x in st]))
        c.call(0x558610)
        k = (ctypes.c_uint32 * 7)()
        probe.probe_frame_finish(k)
        out = bytes(c.read(emu.buf, size + guard))
        mine = bytes(buf)[:size]
        if out[:size] != mine:
            i = next(j for j in range(0, size, 2) if out[j:j + 2] != mine[j:j + 2])
            raise SystemExit(f'finish {n} ({w}x{h} pitch {pitch}, {f}, mmx {mmx}, state {state}): pixel at byte {i} '
                             f'(row {i // pitch}, x {i % pitch // 2}) original {out[i:i + 2].hex()} rebuilt {mine[i:i + 2].hex()}')
        past += out[size:] != b'\xa5' * guard
        if state[0]:
            want = [emu.r32(a) for a in (0x72c1bc, 0x72c1fc, 0x72c200, 0x72c204, 0x72c1d0, 0x72c1b8)]
            fn = emu.r32(0x72c1f8)
            want.append({0x5582c0: 0, 0x558320: 1, 0x558380: 2, 0x558460: 3}[fn])
            if want != list(k):
                raise SystemExit(f'finish {n}: constants original {[hex(x) for x in want]} rebuilt {[hex(x) for x in k]}')
            rows[want[-1]] += 1
    return (f'{runs} finishes identical inside the buffer (rows: scalar 555 {rows[0]}, scalar 565 {rows[1]}, '
            f'MMX 555 {rows[2]}, MMX 565 {rows[3]}; {past} wrote past the buffer in the original, FIX (bug))')


def test_lock(rng, emu, runs=2000):
    c = emu.cpu
    for n in range(runs):
        rec = [rng.choice([640, 0, 320]), rng.choice([480, 0]), rng.choice([1280, 0, 1300]), rng.randint(0, 1),
               rng.choice([0, 0x1234560]), int(rng.random() < 0.2)]
        fill = [rng.choice([640, 800, 17]), rng.choice([480, 600, 3]), rng.choice([1280, 1600, 1664, 40]),
                rng.choice([0x3000000, 0x3100040])]
        result = 0 if rng.random() < 0.85 else 0x887601c2
        emu.desc_fill, emu.lock_result, emu.locks, emu.unlocks = tuple(fill), result, 0, 0
        c.write(BACK, struct.pack('<6I', *rec))
        sw, sh, sb, screen_w = rng.choice([640, 7]), rng.choice([480, 9]), rng.choice([2, 4]), rng.choice([640, 800])
        c.write(0x8007cc, struct.pack('<iii', sw, sh, 0))
        c.u32(0x8007d8, sb)
        c.u32(0x74a75c, screen_w)
        probe.probe_frame_soft_set(sw, sh, sb, screen_w)
        probe.probe_frame_back_set((ctypes.c_int32 * 6)(*rec), ctypes.c_void_p(rec[4]),
                                   (ctypes.c_int32 * 3)(*fill[:3]), ctypes.c_void_p(fill[3]), result)
        which = 0 if rng.random() < 0.7 else 1
        c.call(0x576d00 if which == 0 else 0x576d60)
        probe.probe_frame_lock(which)
        want = list(struct.unpack('<6I', bytes(c.read(BACK, 24))))
        want += [emu.r32(0x8007c8), emu.r32(0x8007cc), emu.r32(0x8007d0), emu.r32(0x8007d4), emu.r32(0x8007d8),
                 emu.r32(0x74a76c), emu.r32(0x8007c8), emu.r32(0x8007d4)]
        want += list(struct.unpack('<5I', bytes(c.read(FINISH_T, 20))))
        want += list(struct.unpack('<4I', bytes(c.read(0x736328 + 0x18, 16))))
        want += [emu.locks, emu.unlocks]
        got = (ctypes.c_uint64 * 25)()
        probe.probe_frame_lock_get(got)
        got = [x & 0xffffffff for x in got]
        if which == 1:  # only the record and the counts matter after an unlock
            want, got = want[:6] + want[-2:], got[:6] + got[-2:]
        if want != got:
            raise SystemExit(f'lock {n} ({"lock" if which == 0 else "unlock"}, record {rec}, fill {fill}, result {result:#x}):\n'
                             f'  original {[hex(x) for x in want]}\n  rebuilt  {[hex(x) for x in got]}')
    return f'{runs} locks and unlocks identical (record, targets, DirectDraw calls)'


def main():
    rng = random.Random(int(sys.argv[1]) if len(sys.argv) > 1 else 7)
    emu = Emu()
    print(test_tint(rng, emu))
    print(test_finish(rng, emu))
    print(test_lock(rng, emu))
    print('ok: the screen tint, the software finish and the 3D lock match the original (24-bit FPU)')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    try:
        main()
    except SystemExit as e:
        print(e)
        sys.stdout.flush()
        os._exit(1)
