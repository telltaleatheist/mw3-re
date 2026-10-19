"""The ported screen-space clippers against the original x86, on random polygons.

    cmake --build build && .venv/bin/python tests/differential/test_clip2d.py [cases]

0x529ce0 (flat), 0x52aaf0 (lit), 0x52bd10 (textured), 0x52dc80 (textured,
lit) and the shadowed 0x52e920 / 0x52fab0 run in the emulator on the same
polygon, clip rect and garbage-filled buffers as the port
(docs/drafts/clip2d.md). Return value, count and every byte of the six
buffers (TL 0x72f41c, work 0x7481a8, shadow sets 0x7489a8 and their partners
0x63d890 / 0x63f390 / 0x63e590) must be identical.

The emulator runs with the FPU at 24-bit precision, as under Direct3D 6
(the game does not ask DDSCL_FPUPRESERVE); the port's float steps then give
the same bits. NaNs compare equal whatever their sign (x87's default NaN is
negative, ARM's positive). A second, smaller run at the x87's power-on 64-bit
precision reports how far the results drift (play-accurate, not compared).
"""
import ctypes
import random
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import HEAP, ROOT, Original  # noqa: E402
from unicorn import UC_HOOK_BLOCK  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
probe.probe_clip2d.restype = ctypes.c_int32
probe.probe_clip2d.argtypes = [ctypes.c_int32, ctypes.c_char_p] + [ctypes.c_void_p] * 4 + \
    [ctypes.c_void_p] * 2 + [ctypes.POINTER(ctypes.c_int32)]
SLOTS = probe.probe_clip2d_slots()
SIZE = SLOTS * 0x20
SH_SETS, SH_VERTS = 7, 64
SH_SIZE = probe.probe_clip2d_shadow_size()      # 0x7489a8: count, 7 words, 7 sets of 64 (u, v)
SH_TMP_SIZE = SH_SETS * SH_VERTS * 8

TL, WORK, TL_TMP, WORK_TMP, SHADOW, SHADOW_TMP = (0x72f41c, 0x7481a8, 0x63d890, 0x63f390,
                                                  0x7489a8, 0x63e590)
BUFS = [(TL, SIZE), (WORK, SIZE), (TL_TMP, SIZE), (WORK_TMP, SIZE), (SHADOW, SH_SIZE),
        (SHADOW_TMP, SH_TMP_SIZE)]
NAMES = ('tl', 'work', 'tl_tmp', 'work_tmp', 'shadow', 'shadow_tmp')
CLIPPERS = [('flat', 0x529ce0, 0x52a250), ('lit', 0x52aaf0, 0x52b450),
            ('textured', 0x52bd10, 0x52c8d0), ('textured_lit', 0x52dc80, 0x52e920),
            ('textured_shadowed', 0x52e920, 0x52fab0),
            ('textured_lit_shadowed', 0x52fab0, 0x530d90)]  # 0x530043 inside is a false Ghidra entry
RECT, COUNT, CW = HEAP, HEAP + 0x40, HEAP + 0x44
F = struct.Struct('<f')


def f32(x):
    return F.unpack(F.pack(x))[0]


class Emulated:
    def __init__(self, cw):
        self.cpu = Original()
        self.cpu.write(CW, struct.pack('<H', cw))
        # fldcw [CW]; ret
        self.fldcw = self.cpu.code(b'\xd9\x2d' + struct.pack('<I', CW) + b'\xc3')
        self.blocks = set()
        self.cpu.uc.hook_add(UC_HOOK_BLOCK, lambda uc, a, s, u: self.blocks.add(a),
                             begin=0x529ce0, end=0x530d90)
        # 0x52b450 sits in this range: an unreferenced clipper, never called here.

    def run(self, addr, rect, bufs, count):
        cpu = self.cpu
        cpu.call(self.fldcw)
        cpu.write(RECT, rect)
        for (a, _), b in zip(BUFS, bufs):
            cpu.write(a, b)
        cpu.u32(COUNT, count)
        cpu.call(addr, ecx=RECT, edx=COUNT)
        ret = cpu.uc.reg_read(UC_X86_REG_EAX)
        n = struct.unpack('<i', cpu.read(COUNT, 4))[0]
        return ret, n, [cpu.read(a, size) for a, size in BUFS]


def port(which, rect, bufs, count):
    cbufs = [ctypes.create_string_buffer(b, len(b)) for b in bufs]
    n = ctypes.c_int32(count)
    ret = probe.probe_clip2d(which, rect, *[ctypes.addressof(b) for b in cbufs], ctypes.byref(n))
    return ret & 0xffffffff, n.value, [b.raw for b in cbufs]


# ---- inputs ---------------------------------------------------------------

def rand_rect(rng):
    kind = rng.random()
    if kind < 0.8:
        l, t = rng.choice([0.0, rng.uniform(-50, 200)]), rng.choice([0.0, rng.uniform(-50, 200)])
        r, b = l + rng.choice([640.0, rng.uniform(1, 800)]), t + rng.choice([480.0, rng.uniform(1, 600)])
    elif kind < 0.9:   # empty or inverted rect
        l, t = rng.uniform(0, 300), rng.uniform(0, 300)
        r, b = l - rng.choice([0.0, rng.uniform(0, 50)]), t - rng.choice([0.0, rng.uniform(0, 50)])
    else:              # huge
        l, t = -rng.uniform(1e3, 1e30), -rng.uniform(1e3, 1e30)
        r, b = rng.uniform(1e3, 1e30), rng.uniform(1e3, 1e30)
    head = rng.getrandbits(32)
    pad = [rng.uniform(-1e3, 1e3) for _ in range(4)]
    return struct.pack('<Iff4fff', head, l, t, *pad, r, b), (f32(l), f32(t), f32(r), f32(b))


def rand_poly(rng, edges):
    l, t, r, b = edges
    w, h = r - l, b - t
    n = rng.randint(3, 20)
    mode = rng.choice(['inside', 'outside', 'straddle', 'straddle', 'convex', 'convex', 'onedge',
                       'degenerate', 'huge', 'corner'])
    if mode == 'huge':
        pts = [(rng.choice([-1, 1]) * 10 ** rng.uniform(0, 30),
                rng.choice([-1, 1]) * 10 ** rng.uniform(0, 30)) for _ in range(n)]
    elif mode == 'convex' or mode == 'corner':
        if mode == 'corner':
            cx, cy = rng.choice([l, r]), rng.choice([t, b])
        else:
            cx, cy = rng.uniform(l - w, r + w), rng.uniform(t - h, b + h)
        rad = abs(w) * rng.uniform(0.05, 1.5) + 1
        angs = sorted(rng.uniform(0, 6.2832) for _ in range(n))
        import math
        pts = [(cx + rad * math.cos(a), cy + rad * math.sin(a)) for a in angs]
    else:
        lo_x, hi_x, lo_y, hi_y = l - w, r + w, t - h, b + h
        if mode == 'inside':
            lo_x, hi_x, lo_y, hi_y = l, r, t, b
        elif mode == 'outside':
            side = rng.randrange(4)
            lo_x, hi_x = [(l - w, l), (r, r + w), (l, r), (l, r)][side]
            lo_y, hi_y = [(t, b), (t, b), (t - h, t), (b, b + h)][side]
        pts = [(rng.uniform(lo_x, hi_x), rng.uniform(lo_y, hi_y)) for _ in range(n)]
        if mode == 'onedge':
            pts = [(rng.choice([x, l, r]), rng.choice([y, t, b])) for x, y in pts]
        elif mode == 'degenerate':
            k = rng.randrange(3)
            if k == 0:     # all the same point
                pts = [pts[0]] * n
            elif k == 1:   # collinear
                (x0, y0), (x1, y1) = pts[0], pts[1]
                pts = [(x0 + (x1 - x0) * s, y0 + (y1 - y0) * s)
                       for s in (rng.uniform(-1, 2) for _ in range(n))]
            else:          # repeated vertices
                pts = [pts[rng.randrange(2)] for _ in range(n)]
    verts = []
    for x, y in pts:
        sz = rng.choice([rng.uniform(0.001, 1.0), rng.uniform(1e-6, 1e-3), 1.0])
        if rng.random() < 0.01:
            sz = rng.choice([0.0, -rng.uniform(0, 1)])
        verts.append((x, y, sz))
    return verts, mode


def rand_bufs(rng, verts):
    bufs = [bytearray(rng.getrandbits(8) for _ in range(size)) for _, size in BUFS]
    for i, (x, y, z) in enumerate(verts):
        struct.pack_into('<fff', bufs[0], i * 0x20, x, y, z)
        struct.pack_into('<ff', bufs[0], i * 0x20 + 0x18, rng.uniform(-4, 4), rng.uniform(-4, 4))
        struct.pack_into('<6f', bufs[1], i * 0x20, *[rng.uniform(0, 1) for _ in range(3)],
                         *[rng.uniform(0, 255) for _ in range(3)])
    # Garbage in the temp buffers stays finite so a stray read shows as a value.
    for buf in (bufs[2], bufs[3], bufs[5]):
        for o in range(0, len(buf), 4):
            struct.pack_into('<f', buf, o, rng.uniform(-1e4, 1e4))
    sh = bufs[4]
    for o in range(0x20, SH_SIZE, 4):
        struct.pack_into('<f', sh, o, rng.uniform(-2, 2))
    struct.pack_into('<i', sh, 0, rng.randint(0, SH_SETS))   # every clipper sees a count
    return [bytes(b) for b in bufs]


# ---- comparison -----------------------------------------------------------

def compare(a, b):
    """(identical, worst difference) over two buffers read as floats: relative,
    absolute below 1; inf when only one side is finite."""
    if a == b:
        return True, 0.0
    worst = 0.0
    for (x,), (y,) in zip(struct.iter_unpack('<f', a), struct.iter_unpack('<f', b)):
        if x == y or (x != x and y != y):
            continue
        if x != x or y != y or abs(x) == float('inf') or abs(y) == float('inf'):
            return False, float('inf')
        worst = max(worst, abs(x - y) / max(abs(x), abs(y), 1.0))
    return worst == 0.0, worst


def check(emu, which, addr, rng):
    """(same, worst, detail, clipped, mode); worst is None when count or result differ."""
    rect, edges = rand_rect(rng)
    verts, mode = rand_poly(rng, edges)
    bufs = rand_bufs(rng, verts)
    want = emu.run(addr, rect, bufs, len(verts))
    got = port(which, rect, bufs, len(verts))
    clipped = want[1] != len(verts) or want[2] != list(bufs)
    if want[:2] != got[:2]:
        return False, None, (mode, rect.hex(), verts, want[:2], got[:2]), clipped, mode
    same, worst, where = True, 0.0, []
    for i in range(len(BUFS)):
        s, w = compare(want[2][i], got[2][i])
        if not s:
            where.append(NAMES[i])
        same, worst = same and s, max(worst, w)
    return same, worst, (mode, rect.hex(), verts, where), clipped, mode


def unreached(emu, start, end):
    """Branch targets and fall-throughs in [start, end) the inputs never ran."""
    asm = (ROOT / 'analysis-output' / 'mech3.asm').read_text().splitlines()
    leaders, prev_branch = set(), False
    pat = re.compile(r'^\s+([0-9a-f]+):\s+\t(\w+)\s*(?:0x([0-9a-f]+))?')
    nops = set()
    for line in asm:
        m = pat.match(line)
        if not m:
            continue
        a = int(m.group(1), 16)
        if not start <= a < end:
            continue
        if prev_branch:
            leaders.add(a)
        op = m.group(2)
        if op == 'nop':
            nops.add(a)
        prev_branch = op.startswith('j') or op == 'ret'
        if op.startswith('j') and m.group(3):
            leaders.add(int(m.group(3), 16))
    return sorted(a for a in leaders if a not in emu.blocks and a not in nops)


def main():
    cases = int(sys.argv[1]) if len(sys.argv) > 1 else 4000
    rng = random.Random(0x529ce0)
    emu = Emulated(0x007f)   # 24-bit precision, round to nearest, all masked
    bad = shown = 0
    for which, (name, addr, end) in enumerate(CLIPPERS):
        n_ok = n_clip = 0
        for _ in range(cases):
            same, worst, info, clipped, _ = check(emu, which, addr, rng)
            n_clip += clipped
            n_ok += same
            if not same and shown < 5:
                shown += 1
                print(f'  {name} differs ({worst}): {info}')
        missed = unreached(emu, addr, end)
        print(f'{name} {addr:#x}: {n_ok}/{cases} identical ({n_clip} clipped); '
              f'blocks never run: {[hex(a) for a in missed] or "none"}')
        bad += cases - n_ok
    # Drift at the x87's 64-bit precision, reported only (docs/drafts/clip2d.md).
    emu64 = Emulated(0x037f)
    for which, (name, addr, _) in enumerate(CLIPPERS):
        tally = {'identical': 0, 'count': 0, 'nonfinite': 0}
        worst_normal = worst_huge = 0.0
        for _ in range(max(cases // 4, 50)):
            same, w, _, _, mode = check(emu64, which, addr, rng)
            if same:
                tally['identical'] += 1
            elif w is None:
                tally['count'] += 1
            elif w == float('inf'):
                tally['nonfinite'] += 1
            elif mode == 'huge':
                worst_huge = max(worst_huge, w)
            else:
                worst_normal = max(worst_normal, w)
        print(f'  64-bit x87, {name}: {tally}, worst difference {worst_normal:.3g} '
              f'(huge inputs {worst_huge:.3g})')
    print('clip2d: all identical' if bad == 0 else f'clip2d: {bad} cases differ')
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
