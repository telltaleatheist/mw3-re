"""The ported camera-space near/far clippers (docs/drafts/clip3d.md) against the
original x86: 0x5275e0 flat, 0x527a30 lit (untextured), 0x527fd0 textured,
0x5286e0 textured with per-vertex light and fog, and the last two again with
projected-shadow UV sets (0x528bb0, 0x529120).

    cmake --build build && .venv/bin/python tests/differential/test_clip3d.py [cases]

Random polygons (3..20 vertices; also 0..2) in front of, behind and across the
near and far planes, degenerate ones, vertices exactly on a plane, huge values,
infinities and NaNs. The clip struct, the work buffers and the clippers' own
scratch get the same random bytes on both sides; afterwards the return value,
the count and every byte of all six buffers must match.

The emulator runs twice: with the x87 at 24-bit precision (what Direct3D sets
in the game) everything must be bit-identical; at the power-on 64-bit
precision, interpolated floats may differ in the last bits (relative 1e-6).
Instructions of the six functions never executed are listed at the end.
"""
import ctypes
import math
import random
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import ROOT, Original  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
P = ctypes.c_char_p
probe.probe_clip3d.argtypes = [ctypes.c_int, P, ctypes.POINTER(ctypes.c_int32)] + [ctypes.c_void_p] * 8 + \
    [ctypes.c_int32]
probe.probe_clip3d.restype = ctypes.c_int32

FUNCS = [(0x5275e0, 0x5277d0, 'flat'), (0x527a30, 0x527d20, 'lit'),
         (0x527fd0, 0x5283c0, 'textured'), (0x5286e0, 0x528bb0, 'textured lit'),
         (0x528bb0, 0x529120, 'textured shadow'), (0x529120, 0x529790, 'textured lit shadow')]
SHADOWED = (4, 5)
SHADOW_COUNT = 0x7489a8
CLIP = 0x7402f8
# (name, address, size, float layout): order = probe_clip3d's buffers.
BUFFERS = [('cam', 0x7476a4), ('rec', 0x7481a8), ('tl', 0x72f41c),
           ('tl_out', 0x63d890), ('cam_out', 0x63e290), ('rec_out', 0x63f390),
           ('shadow_uv', 0x7489c8), ('shadow_out', 0x63e590)]
sizes = (ctypes.c_int32 * 8)()
probe.probe_clip3d_sizes(sizes)
SIZES = list(sizes)
MAX_OUT = 64  # the scratch holds 64 vertices; keep outputs inside it


def f32(x):
    if math.isfinite(x) and abs(x) > 3.4028234e38:
        x = math.copysign(math.inf, x)
    return struct.pack('<f', x)


def rnd_float(rng):
    k = rng.random()
    if k < 0.6:
        return rng.uniform(-50, 50)
    if k < 0.9:
        return rng.choice((-1, 1)) * 10 ** rng.uniform(-30, 30)
    return rng.choice((0.0, -0.0, 1e38, -1e38, 3.4e38, float('inf'), float('-inf'), float('nan')))


def z_value(rng, kind, near, far):
    if kind == 'front':
        return rng.uniform(near, far)
    if kind == 'behind':
        return rng.uniform(-far, near) if rng.random() < 0.5 else near - rng.random() * 1e-3
    if kind == 'beyond':
        return rng.uniform(far, 4 * far)
    if kind == 'on_near':
        return near
    if kind == 'on_far':
        return far
    if kind == 'huge':
        return rnd_float(rng)
    return rng.uniform(-far, 2 * far)


def make_case(rng, which):
    near = rng.choice((0.1, 0.1, 1.0, rng.uniform(0.01, 50), 0.0, -1.0))
    far = rng.choice((800.0, 800.0, rng.uniform(1, 2000), near, near + 1))
    if rng.random() < 0.03:
        near, far = rnd_float(rng), rnd_float(rng)
    n = rng.choice((0, 1, 2)) if rng.random() < 0.05 else rng.randint(3, 20)
    style = rng.choice(('front', 'behind', 'beyond', 'near', 'far', 'both', 'on', 'degenerate',
                        'huge', 'mixed'))
    pts = []
    for _ in range(n):
        if style in ('front', 'behind', 'beyond', 'huge'):
            kind = style
        elif style == 'near':
            kind = rng.choice(('front', 'behind', 'on_near'))
        elif style == 'far':
            kind = rng.choice(('front', 'beyond', 'on_far'))
        elif style == 'both':
            kind = rng.choice(('front', 'behind', 'beyond'))
        elif style == 'on':
            kind = rng.choice(('on_near', 'on_far', 'front', 'behind'))
        else:
            kind = 'mixed'
        x, y = (rnd_float(rng), rnd_float(rng)) if style == 'huge' else \
            (rng.uniform(-100, 100), rng.uniform(-100, 100))
        pts.append([x, y, z_value(rng, kind, near, far)])
    if style == 'degenerate' and n:
        p = pts[0]
        pts = [list(p) if rng.random() < 0.7 else [p[0], p[1], rng.uniform(-1, 1)] for _ in pts]
    mask = rng.choice((0x10, 0x20, 0x30, 0x30, 0x30, 0)) | rng.randrange(16) | \
        (rng.getrandbits(32) & ~0x3f if rng.random() < 0.2 else 0)
    clip = bytearray(rng.randbytes(0x24))
    struct.pack_into('<I', clip, 0, mask)
    struct.pack_into('<f', clip, 0xc, near)
    struct.pack_into('<f', clip, 0x18, far)
    bufs = [bytearray(rng.randbytes(s)) for s in SIZES]
    for i, p in enumerate(pts):
        bufs[0][12 * i:12 * i + 12] = b''.join(f32(v) for v in p)
    raw = rng.random() < 0.1
    for i in range(n):  # sensible work records and UVs, in some cases raw bytes
        if not raw:
            bufs[1][32 * i:32 * i + 24] = b''.join(f32(rnd_float(rng) if style == 'huge' else
                                                       rng.uniform(0, 255)) for _ in range(6))
            bufs[2][32 * i + 24:32 * i + 32] = f32(rng.uniform(-2, 2)) + f32(rng.uniform(-2, 2))
    shadows = rng.randint(0, 7) if which in SHADOWED else 0
    for sh in range(shadows):
        for i in range(n):
            if not raw:
                o = 0x200 * sh + 8 * i
                bufs[6][o:o + 8] = f32(rng.uniform(-2, 2)) + f32(rng.uniform(-2, 2))
    return bytes(clip), n, bufs, shadows, extreme(clip, n, bufs, shadows)


def extreme(clip, n, bufs, shadows):
    """True when an input float is so large or small that a product or
    difference can leave the float range, where the x87's wider exponent
    keeps what a float loses (or is a NaN or infinity)."""
    words = [clip[0xc:0x10], clip[0x18:0x1c]]
    for i in range(n):
        words += [bufs[0][12 * i + k:12 * i + k + 4] for k in (0, 4, 8)]
        words += [bufs[1][32 * i + k:32 * i + k + 4] for k in range(0, 24, 4)]
        words += [bufs[2][32 * i + k:32 * i + k + 4] for k in (24, 28)]
        words += [bufs[6][0x200 * sh + 8 * i + k:0x200 * sh + 8 * i + k + 4]
                  for sh in range(shadows) for k in (0, 4)]
    for w in words:
        v = struct.unpack('<f', w)[0]
        if not math.isfinite(v) or abs(v) > 1e18 or 0 < abs(v) < 1e-18:
            return True
    return False


class Emulated:
    def __init__(self, cw):
        cpu = self.cpu = Original()
        self.count_addr = cpu.alloc(4)
        cwaddr = cpu.alloc(4)
        cpu.write(cwaddr, struct.pack('<H', cw))
        setcw = cpu.code(b'\xd9\x2d' + struct.pack('<I', cwaddr) + b'\xc3')  # fldcw [cw]; ret
        cpu.call(setcw)
        self.seen = set()
        lo, hi = FUNCS[0][0], FUNCS[-1][1] - 1
        cpu.uc.hook_add(UC_HOOK_CODE, lambda uc, a, s, u: self.seen.add(a), begin=lo, end=hi)

    def run(self, which, clip, n, bufs, shadows):
        cpu = self.cpu
        cpu.write(CLIP, clip)
        cpu.u32(SHADOW_COUNT, shadows)
        cpu.u32(self.count_addr, n)
        for (_, addr), b in zip(BUFFERS, bufs):
            cpu.write(addr, b)
        cpu.call(FUNCS[which][0], ecx=CLIP, edx=self.count_addr)
        ret = cpu.uc.reg_read(UC_X86_REG_EAX)
        count = struct.unpack('<i', cpu.read(self.count_addr, 4))[0]
        return ret, count, [cpu.read(addr, len(b)) for (_, addr), b in zip(BUFFERS, bufs)]


def run_port(which, clip, n, bufs, shadows):
    count = ctypes.c_int32(n)
    out = [ctypes.create_string_buffer(bytes(b), len(b)) for b in bufs]
    ret = probe.probe_clip3d(which, clip, ctypes.byref(count), *out, shadows)
    return ret, count.value, [o.raw for o in out]


def is_nan(w):
    return (struct.unpack('<I', w)[0] & 0x7fffffff) > 0x7f800000


def compare(want, got, stats):
    """Returns a problem or None. Every 4-byte word must match, except that a
    NaN may carry another sign (x87 makes 0xffc00000, the host 0x7fc00000) and,
    when `stats` is given (64-bit x87), floats may differ: the worst relative
    difference is kept there."""
    if want[0] != got[0]:
        return f'returned {got[0]}, original {want[0]}'
    if want[1] != got[1]:
        return f'count {got[1]}, original {want[1]}'
    for (name, _), w, g in zip(BUFFERS, want[2], got[2]):
        if w == g:
            continue
        for off in range(0, len(w), 4):
            a, b = w[off:off + 4], g[off:off + 4]
            if a == b:
                continue
            if is_nan(a) and is_nan(b):
                STATS['nan'] += 1
                continue
            if stats is None:
                return f'{name}+{off:#x}: {b.hex()} vs original {a.hex()}'
            fa, fb = struct.unpack('<f', a)[0], struct.unpack('<f', b)[0]
            stats['words'] += 1
            if math.isfinite(fa) and math.isfinite(fb):
                rel = abs(fa - fb) / max(abs(fa), abs(fb)) if fa != fb else 0.0
                stats['worst'] = max(stats['worst'], rel)
                stats['over_1e-6'] += rel > 1e-6
            else:
                stats['inf'] += 1
    return None


STATS = {'nan': 0}


def instructions(lo, hi):
    pat = re.compile(r'^\s+([0-9a-f]+):\s+\t(\S.*)$')
    out = {}
    for line in (ROOT / 'analysis-output' / 'mech3.asm').open():
        m = pat.match(line)
        if m:
            a = int(m.group(1), 16)
            if lo <= a < hi and m.group(2).strip() != 'nop':
                out[a] = m.group(2).strip()
            elif a >= hi:
                break
    return out


def main():
    cases = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    rng = random.Random(0x5275e0)
    emus = {'24-bit': Emulated(0x007f), '64-bit': Emulated(0x037f)}
    loose = {'words': 0, 'worst': 0.0, 'over_1e-6': 0, 'inf': 0}
    wide24, wide_cases = dict(loose), 0
    fails, outcomes = 0, {'dropped': 0, 'unchanged': 0, 'clipped': 0}
    for c in range(cases):
        for which in range(len(FUNCS)):
            clip, n, bufs, shadows, wide = make_case(rng, which)
            wide_cases += wide
            got = run_port(which, clip, n, bufs, shadows)
            for name, emu in emus.items():
                want = emu.run(which, clip, n, bufs, shadows)
                if want[1] > MAX_OUT:
                    continue
                stats = loose if name == '64-bit' else wide24 if wide else None
                problem = compare(want, got, stats)
                if problem:
                    fails += 1
                    if fails <= 10:
                        print(f'case {c} {FUNCS[which][2]} ({name} x87), n={n}: {problem}')
            outcomes['dropped' if not got[0] else 'unchanged' if got[2] == [bytes(b) for b in bufs]
                     else 'clipped'] += 1
    total = cases * len(FUNCS)
    print('outcomes: ' + ', '.join(f'{k} {v}' for k, v in outcomes.items()) +
          f'; NaNs differing only in sign: {STATS["nan"]}')
    print(f'64-bit x87 (power-on precision, the game runs at 24-bit): {loose["words"]} floats differ, '
          f'worst relative {loose["worst"]:.2g}, {loose["over_1e-6"]} over 1e-6, '
          f'{loose["inf"]} inf vs finite')
    print(f'24-bit x87, {wide_cases} cases with extreme inputs: {wide24["words"]} floats differ, '
          f'worst relative {wide24["worst"]:.2g}, {wide24["inf"]} inf vs finite')
    seen = set().union(*(e.seen for e in emus.values()))
    for lo, hi, name in FUNCS:
        missed = [a for a in instructions(lo, hi) if a not in seen]
        print(f'{name}: ' + ('every instruction executed' if not missed else
                             'never executed: ' + ', '.join(f'{a:#x}' for a in missed)))
    if fails:
        print(f'FAIL: {fails} mismatches in {total} polygons x 2 precisions')
        sys.exit(1)
    print(f'clip3d: {total} polygons identical to the original (24-bit x87)')


if __name__ == '__main__':
    main()
