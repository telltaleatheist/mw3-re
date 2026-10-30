"""The damage decals against the original x86 (docs/damage-decals.md):

- the cookie 0x542730 (zModel gmod_cookie.cpp): a bullet hole's alpha
  stamps the damage skin's texels into a texture at the captured UV;
- the damage decal 0x474d90 with its texture lookup 0x474cd0: which skin
  entry, which hole, the cookie's arguments and the device reload.

    cmake --build build && .venv/bin/python -P tests/differential/test_decal.py [runs]

The cookie runs on random images (8 to 64 texels a side), holes (odd and even
sizes), masks (every value, and the thresholds 3, 4, 7, 8, 0xfb, 0xfc), UVs
from -3 to 3 (wrapped), both formats (565, 555), and now and then mismatched
sizes or a palette (refused). Compared: the result, every target texel and
the UV after, at 24-bit precision.

The decal runs on skins of up to 8 entries (texture or none, cached or not,
a damage link or none, the damage's texture or none), a hit material whose
texture is one of them or none, 0 to 8 holes, the damage skin linked or not,
and a seed. 0x475520, rand, the cookie and the reload are stubbed; the
cookie answers 1. Compared: the calls (the cookie's target entry, damage
image and hole; the reload's record).
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import load_probe  # noqa: E402
from original import FTOL, IAT_FTOL, Original  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

probe = load_probe()
F, I16, I32, U32 = ctypes.c_float, ctypes.c_int16, ctypes.c_int32, ctypes.c_uint32
probe.probe_cookie.argtypes = [I16, I16, ctypes.POINTER(ctypes.c_uint16), I16, I16, ctypes.POINTER(ctypes.c_uint16),
                               I32, I16, I16, ctypes.POINTER(ctypes.c_uint8), ctypes.POINTER(F), I32]
probe.probe_decal.argtypes = [I32] + [ctypes.POINTER(I32)] * 4 + [I32, I32, I32, U32, ctypes.POINTER(U32)]
UV, FORMAT, HOLES, D3D_RELOAD = 0x74a2c0, 0x74a738, 0x6061ac, 0x72f374
IAT_RAND = 0x5bb668


class MsvcRand:
    def __init__(self, seed):
        self.s = seed

    def __call__(self, c):
        self.s = (self.s * 214013 + 2531011) & 0xffffffff
        return (self.s >> 16) & 0x7fff


def f2u(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def u2f(x):
    return struct.unpack('<f', struct.pack('<I', x))[0]


class Emulated:
    def __init__(self):
        self.cpu = cpu = Original()
        cpu.fpu_control(0x007f)  # play precision
        cpu.import_code(IAT_FTOL, FTOL)
        cpu.stub_function(0x49bfd0, lambda c: 0)  # the report
        self.calls = []

    def image(self, w, h, pixels=None, alpha=None, palette=0):
        cpu = self.cpu
        t = cpu.alloc(0x38)
        cpu.write(t, bytes(0x38))
        cpu.write(t + 4, struct.pack('<hh', w, h))
        if pixels is not None:
            p = cpu.alloc(2 * len(pixels) + 4)
            cpu.write(p, struct.pack('<%dH' % len(pixels), *pixels))
            cpu.u32(t + 0x10, p)
        if alpha is not None:
            a = cpu.alloc(len(alpha) + 4)
            cpu.write(a, bytes(alpha))
            cpu.u32(t + 0x14, a)
        cpu.u32(t + 0x18, palette)
        return t

    def cookie(self, tw, th, tpix, fw, fh, fpix, fpal, hw, hh, halpha, uv, fmt):
        cpu = self.cpu
        target = self.image(tw, th, tpix)
        from_ = self.image(fw, fh, fpix, palette=0x1000 if fpal else 0)
        hole = self.image(hw, hh, alpha=halpha)
        entry = cpu.alloc(0x28)
        cpu.write(entry, bytes(0x28))
        cpu.u32(entry, target)
        cpu.write(UV, struct.pack('<2f', *uv))
        cpu.u32(FORMAT, fmt)
        cpu.call(0x542730, ecx=entry, edx=from_, stack_args=(hole, 0))
        r = cpu.uc.reg_read(UC_X86_REG_EAX) & 0xff
        p = struct.unpack('<I', cpu.read(target + 0x10, 4))[0]
        out = list(struct.unpack('<%dH' % len(tpix), cpu.read(p, 2 * len(tpix))))
        return r, out, list(struct.unpack('<2f', cpu.read(UV, 8)))


def run_cookies(runs, r):
    emu = Emulated()
    seen = {'ok': 0, 'refused': 0, 565: 0, 555: 0}
    for k in range(runs):
        w, h = r.choice([8, 16, 32, 64, 12, 20]), r.choice([8, 16, 32, 64, 12, 20])
        hw, hh = r.choice([8, 16, 7, 9, 5, 4]), r.choice([8, 16, 7, 9, 5, 4])
        hw, hh = min(hw, w), min(hh, h)
        fw, fh = (w, h) if r.random() < 0.92 else (w + r.choice([-2, 2]), h)
        fpal = r.random() < 0.04
        tpix = [r.randrange(1 << 16) for _ in range(w * h)]
        fpix = [r.randrange(1 << 16) for _ in range(max(1, fw) * max(1, fh))]
        edge = [0, 3, 4, 7, 8, 0xfb, 0xfc, 0xff, 0x80, 1, 0xfe]
        halpha = [r.choice(edge) if r.random() < 0.4 else r.randrange(256) for _ in range(hw * hh)]
        uv = [r.choice([r.uniform(-3, 3), r.uniform(0, 1), 0.0, 1.0, 1.01, -0.01, 1.005]) for _ in range(2)]
        uv = [u2f(f2u(x)) for x in uv]
        fmt = r.choice([5, 6])
        want = emu.cookie(w, h, tpix, fw, fh, fpix, fpal, hw, hh, halpha, uv, fmt)
        tp = (ctypes.c_uint16 * len(tpix))(*tpix)
        fp = (ctypes.c_uint16 * len(fpix))(*fpix)
        ha = (ctypes.c_uint8 * len(halpha))(*halpha)
        uvp = (F * 2)(*uv)
        got_r = probe.probe_cookie(w, h, tp, fw, fh, fp, 1 if fpal else 0, hw, hh, ha, uvp, fmt)
        got = (got_r, list(tp), [uvp[0], uvp[1]])
        if want != got:
            diff = [i for i in range(len(tpix)) if want[1][i] != got[1][i]][:4]
            raise SystemExit(f'cookie {k}: {w}x{h} hole {hw}x{hh} uv {uv} format {fmt}: result {want[0]}/{got[0]}, '
                             f'uv {want[2]}/{got[2]}, texels differ at {diff} '
                             f'{[(hex(want[1][i]), hex(got[1][i])) for i in diff]}')
        seen['ok' if want[0] else 'refused'] += 1
        if want[0]:
            seen[565 if fmt == 6 else 555] += 1
    return seen


def run_decals(runs, r):
    emu = Emulated()
    cpu = emu.cpu
    seen = {'painted': 0, 'reloaded': 0, 'nothing': 0}
    # The stubs once (each stub_function adds a hook), their state per case.
    case = {}
    cpu.stub_function(0x475520, lambda c: 1 if case['linked'] else 0)
    cpu.stub_import(IAT_RAND, lambda c: case['rand'](c))

    def cookie(c):
        texes, images, hole_imgs = case['texes'], case['images'], case['holes']
        ti = texes.index(c.ecx) if c.ecx in texes else -1
        fi = images.index(c.edx) if c.edx in images else -1
        h = c.arg(0)
        hi = hole_imgs.index(h) if h in hole_imgs else -1
        emu.calls.extend([1, ti & 0xffffffff, fi & 0xffffffff, hi & 0xffffffff])
        return 1
    cpu.stub_function(0x542730, cookie, 8)

    def reload(c):
        texes = case['texes']
        emu.calls.extend([2, texes.index(c.ecx) & 0xffffffff if c.ecx in texes else 0xffffffff])
        return 0
    cpu.stub_function(0x57baa0, reload, 4)  # [0x72f374]'s
    cpu.u32(D3D_RELOAD, 0x57baa0)
    for k in range(runs):
        n = r.randrange(1, 9)
        has_tex = [int(r.random() < 0.85) for _ in range(n)]
        cached = [int(r.random() < 0.15) for _ in range(n)]
        damage = [r.randrange(n) if r.random() < 0.8 else -1 for _ in range(n)]
        damage_tex = [int(r.random() < 0.85) for _ in range(n)]
        hit = r.randrange(n) if r.random() < 0.85 else -1
        if hit >= 0 and r.random() < 0.15:
            hit += 100  # the texture pointer an image's: only a cached entry holds one
        holes = r.choice([0, 1, 2, 8, 8, 8])
        linked = int(r.random() < 0.9)
        seed = r.randrange(1 << 31)
        # The original's: the skin (+0x08..+0x0c the entries), each entry
        # (+0 texture, +4 cached, +0x10 damage), the texture entries (+0 image,
        # +4 the device record on odd ones), the images, the holes.
        texes = [cpu.alloc(0x28) for _ in range(n)]
        images = [cpu.alloc(0x38) for _ in range(n)]
        entries = [cpu.alloc(0x1c) for _ in range(n)]
        for i in range(n):
            cpu.write(texes[i], bytes(0x28))
            cpu.u32(texes[i], images[i])
            cpu.u32(texes[i] + 4, texes[i] if i & 1 else 0)
            cpu.write(entries[i], bytes(0x1c))
        tex_of = {}
        for i in range(n):
            t = (images[i] if cached[i] else texes[i]) if has_tex[i] else 0
            tex_of[i] = t
        for i in range(n):
            if damage[i] >= 0 and not damage_tex[i]:
                tex_of[damage[i]] = 0
        for i in range(n):
            cpu.u32(entries[i], tex_of[i])
            cpu.write(entries[i] + 4, bytes([cached[i]]))
            cpu.u32(entries[i] + 0x10, entries[damage[i]] if damage[i] >= 0 else 0)
        vec = cpu.alloc(4 * n)
        cpu.write(vec, struct.pack('<%dI' % n, *entries))
        skin = cpu.alloc(0x18)
        cpu.write(skin, bytes(0x18))
        cpu.u32(skin + 8, vec)
        cpu.u32(skin + 0xc, vec + 4 * n)
        hole_imgs = [cpu.alloc(0x38) for _ in range(8)]
        hv = cpu.alloc(32)
        cpu.write(hv, struct.pack('<8I', *hole_imgs))
        cpu.u32(HOLES, hv if holes else 0)
        cpu.u32(HOLES + 4, hv + 4 * holes if holes else 0)
        stray = cpu.alloc(0x28)  # a texture no entry has
        mat = cpu.alloc(0x2c)
        cpu.write(mat, bytes(0x2c))
        cpu.u32(mat + 0x10, images[hit - 100] if hit >= 100 else texes[hit] if hit >= 0 else stray)
        coll = cpu.alloc(0x30)
        cpu.write(coll, bytes(0x30))
        cpu.u32(coll + 0x20, mat)
        emu.calls = []
        case.update(linked=linked, rand=MsvcRand(seed), texes=texes, images=images, holes=hole_imgs)
        setobj = cpu.alloc(0x14)
        cpu.call(0x474d90, ecx=setobj, stack_args=(skin, coll, 0))
        want = list(emu.calls)
        a = lambda v: (I32 * len(v))(*v)  # noqa: E731
        out = (U32 * 64)()
        probe.probe_decal(n, a(has_tex), a(cached), a(damage), a(damage_tex), hit, holes, linked, seed, out)
        got = list(out[1:1 + out[0]])
        if want != got:
            raise SystemExit(f'decal {k}: n {n} tex {has_tex} cached {cached} damage {damage} dtex {damage_tex} '
                             f'hit {hit} holes {holes} linked {linked}: original {want}, port {got}')
        if not want:
            seen['nothing'] += 1
        else:
            seen['painted'] += 1
            seen['reloaded'] += 2 in want[4:5]
    return seen


def main():
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    r = random.Random(474)
    c = run_cookies(runs, r)
    print(f"cookie: {runs} stamps identical ({c['ok']} made: {c[565]} in 565, {c[555]} in 555; {c['refused']} refused)")
    d = run_decals(runs, r)
    print(f"decal: {runs} hits identical ({d['painted']} painted, {d['reloaded']} reloaded, {d['nothing']} with nothing to do)")


if __name__ == '__main__':
    main()
