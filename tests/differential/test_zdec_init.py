"""zDEClient's set-up 0x501ab0 against the original x86 (docs/craters.md,
section 1): declient.zrd into the crater and quicksand defaults, the crater
texture table and the quicksand's animated material.

    cmake --build build && .venv/bin/python tests/differential/test_zdec_init.py

Every world's declient.zrd, with no texture loaded yet, all of them loaded
and a random half, the animations found or not; then made-up files (none,
no CRATER, no QUICK_SAND, empty lists, one-item and three-item TEXTURE_ANIM
values, frame counts 0 and 2..9, missing keys, ints for floats, the restore
registration). The original's reader, zreader lookups, material code
(0x536a70, 0x536ad0, 0x537830, 0x537be0, 0x5375c0..0x5377e0) and texture
default 0x518210 run; the texture lookups (0x518220, 0x518c60), their load
0x5190d0, the animation lookup 0x50b450 and the restore's registration
0x5a8a20 are stubbed on both sides with fake handles. Compared: the calls
in order, zerr reports, the result, the defaults, the table, the quicksand
state and the whole material table. Two kinds of file crash or misread in
the original (a QUICK_SAND with one frame or no DEFAULT_TEXTURE; FIX (bug),
docs/fixes.md): the port alone runs them.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from ai_world import IAT, ROOT, ZBD, ser  # noqa: E402
from original import FTOL, IAT_FTOL, Original  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402
import zrd  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
probe.probe_zdec_init.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int,
                                  ctypes.c_void_p, ctypes.c_int]
INIT, READ_NODE, LOAD, ZERR = 0x501ab0, 0x557180, 0x556740, 0x49bfd0
FIND_TEX, ADD_TEX, LOAD_TEX, ANIM_FIND, REGISTER, MAT_INIT = 0x518220, 0x518c60, 0x5190d0, 0x50b450, 0x5a8a20, 0x536a70
IAT_TIME, IAT_SRAND = 0x5bb5d8, 0x5bb5dc
DEFAULT_TEX = 0x5ecbf8
TEX_BASE, ANIM_BASE = 0x6f000000, 0x1000
MAT = dict(blocks=0x63fbc8, nblocks=0x63fbc0, cap=0x63fbc4, count=0x63fbcc, free=0x5edd54, used=0x5edd58,
           last=0x63fbd0)
HANDLE = 0x1234
CAP = 1 << 20


class Emulated:
    def __init__(self):
        cpu = self.cpu = Original()
        cpu.import_code(IAT_FTOL, FTOL)
        cpu.fpu_control(0x027f)
        self.sizes = {}
        cpu.stub_import(IAT['readfile'], self.read_file, pop_bytes=20)
        cpu.stub_import(IAT['malloc'], lambda c: self.alloc(c.arg(0)))
        cpu.stub_import(IAT['calloc'], lambda c: self.alloc(c.arg(0) * c.arg(1)))
        cpu.stub_import(IAT['realloc'], self.realloc)
        cpu.stub_import(IAT['free'], lambda c: 0)
        cpu.stub_import(IAT_TIME, lambda c: 0)
        cpu.stub_import(IAT_SRAND, lambda c: 0)
        cpu.stub_function(LOAD, lambda c: self.root, 4)
        cpu.stub_function(ZERR, self.zerr, 0)
        cpu.stub_function(FIND_TEX, self.find_tex, 0)
        cpu.stub_function(ADD_TEX, self.add_tex, 0)
        cpu.stub_function(LOAD_TEX, lambda c: self.log(0x74), 0)
        cpu.stub_function(ANIM_FIND, self.anim_find, 0)
        cpu.stub_function(REGISTER, lambda c: self.log(0x75), 12)
        # 0x536a70 keeps flag bits 3..7 of the uninitialised stack material:
        # zero it first, as the port's value-initialised one.
        from unicorn import UC_HOOK_CODE
        cpu.uc.hook_add(UC_HOOK_CODE, lambda uc, a, s, u: cpu.write(cpu.ecx, bytes(0x28)), begin=MAT_INIT,
                        end=MAT_INIT)
        self.base_heap = cpu.heap_next

    # --- stubs ---------------------------------------------------------------
    def alloc(self, n):
        p = self.cpu.alloc(max(n, 1))
        self.cpu.write(p, bytes(max(n, 1)))
        self.sizes[p] = n
        return p

    def realloc(self, c):
        old, n = c.arg(0), c.arg(1)
        p = self.alloc(n)
        if old:
            self.cpu.write(p, self.cpu.read(old, min(n, self.sizes.get(old, 0))))
        return p

    def read_file(self, c):
        buf, n, out = c.arg(1), c.arg(2), c.arg(3)
        chunk = self.data[self.pos:self.pos + n]
        self.pos += len(chunk)
        c.write(buf, chunk)
        c.u32(out, len(chunk))
        return 1

    def cstr(self, p):
        if not p:
            return None
        out = bytearray()
        while True:
            b = self.cpu.read(p + len(out), 64)
            i = b.find(b'\0')
            if i >= 0:
                return bytes(out + b[:i])
            out += b

    def log(self, *v):
        for x in v:
            if isinstance(x, (bytes, type(None))):
                self.out += struct.pack('<I', 0xffffffff if x is None else len(x)) + (x or b'')
            else:
                self.out += struct.pack('<I', x & 0xffffffff)
        return 0

    def zerr(self, c):
        return self.log(0x70, c.arg(0), c.arg(2))

    def find_tex(self, c):
        name = self.cstr(c.ecx)
        self.log(0x71, name)
        if name in self.known:
            return TEX_BASE + 0x40 * self.known.index(name)
        if name in self.added:
            return TEX_BASE + 0x40 * (256 + self.added.index(name))
        return 0

    def add_tex(self, c):
        name = self.cstr(c.ecx)
        self.log(0x72, name)
        self.added.append(name)
        return TEX_BASE + 0x40 * (256 + len(self.added) - 1)

    def anim_find(self, c):
        name = self.cstr(c.ecx)
        self.log(0x73, name)
        if name in self.anims:
            return ANIM_BASE + 16 * self.anims.index(name)
        return 0

    # --- a run ---------------------------------------------------------------
    def u32(self, a):
        return struct.unpack('<I', self.cpu.read(a, 4))[0]

    def run(self, data, known, anims, restore):
        cpu = self.cpu
        cpu.heap_next, self.sizes, self.out = self.base_heap, {}, bytearray()
        self.known, self.added, self.anims = known, [], anims
        cpu.write(0x60bf80, bytes(0x60c0d0 - 0x60bf80))
        for k, a in MAT.items():
            cpu.u32(a, 0xffffffff if k in ('free', 'used') else 0)
        cpu.u32(0x5eba1c, restore)
        self.root = 0
        if data:
            self.data, self.pos = data, 0
            self.root = self.alloc(8)
            cpu.call(READ_NODE, ecx=HANDLE, edx=self.root)
        world = self.alloc(0x40)
        cpu.u32(world + 0x38, 0x5000)
        cpu.call(INIT, ecx=world)
        r = cpu.uc.reg_read(UC_X86_REG_EAX)
        self.log(0x7f, r)
        self.out += cpu.read(0x60bfc0, 8) + bytes(4) + cpu.read(0x60bfcc, 12)
        self.out += cpu.read(0x60c00c, 28)
        n = self.u32(0x60bfb8)
        self.log(n)
        tab = self.u32(0x60bfbc)
        for i in range(n):
            g, cr, an = struct.unpack('<3I', cpu.read(tab + 12 * i, 12))
            self.log(self.mat_id(g), self.mat_id(cr), (an - ANIM_BASE) // 16 if an else 0xffffffff)
        on = self.u32(0x60bfe8)
        self.log(on)
        if on:
            self.log(self.u32(0x60bfec), self.u32(0x60bff0), self.mat_id(self.u32(0x60bffc)),
                     self.mat_id(self.u32(0x60c000)))
        self.log(self.u32(MAT['count']))
        i = struct.unpack('<i', cpu.read(MAT['used'], 4))[0]
        while i >= 0:
            m = self.material(i)
            raw = cpu.read(m, 0x2c)
            alpha, flags, rgb = raw[0], raw[1], struct.unpack_from('<H', raw, 2)[0]
            self.log(i, alpha, flags, rgb)
            self.out += raw[4:0x10]
            self.log(self.tex_id(struct.unpack_from('<I', raw, 0x10)[0]))
            self.out += raw[0x14:0x24]
            if flags & 4:
                cy = struct.unpack_from('<I', raw, 0x24)[0]
                c = cpu.read(cy, 0x1c)
                looping, _, index, speed, count, added, texs = struct.unpack('<IIIIIII', c)
                self.log(looping, index, speed, count, added)
                for k in range(count):
                    self.log(self.tex_id(self.u32(texs + 4 * k)))
            i = struct.unpack_from('<h', raw, 0x2a)[0]
        return bytes(self.out)

    def material(self, i):
        bs = self.u32(0x5edd50)
        return self.u32(self.u32(MAT['blocks']) + 4 * (i // bs)) + (i % bs) * 0x2c

    def mat_id(self, p):
        if not p:
            return 0xfffffffe
        bs = self.u32(0x5edd50)
        arr = self.u32(MAT['blocks'])
        for b in range(self.u32(MAT['nblocks'])):
            base = self.u32(arr + 4 * b)
            if base <= p < base + bs * 0x2c:
                return b * bs + (p - base) // 0x2c
        return 0xfffffffd

    def tex_id(self, p):
        if p == 0:
            return 0
        if p == DEFAULT_TEX:
            return 0xffffffff
        return (p - TEX_BASE) // 0x40


def port(data, known, anims, restore):
    buf = ctypes.create_string_buffer(CAP)
    n = probe.probe_zdec_init(data or b'', len(data or b''), b'\n'.join(known), b'\n'.join(anims), restore, buf,
                              CAP)
    assert n >= 0
    return buf.raw[:n]


def world_files():
    out = []
    for p in sorted(ZBD.glob('*/reader.zbd')):
        data = p.read_bytes()
        files = {n: data[s:s + k] for n, s, k in zrd.entries(data)}
        if 'declient.zrd' in files:
            out.append((p.parent.name, files['declient.zrd']))
    return out


def made_up(rng):
    """(name, tree, crashes in the original)."""
    def tex():
        return rng.choice(['ta', 'tb', 'tc', 'td', 'gx', 'gy'])

    def anim():
        return rng.choice(['dust', 'boom', 'none.flt'])

    cases = []
    for k in range(400):
        crater = []
        for key in rng.sample(['DEFAULT_TEXTURE', 'DEFAULT_ANIM', 'TEXTURE_PARMS', 'SLOPE', 'RADIUS', 'DEPTH',
                               'POINTS', 'TEXTURE_ANIM'], rng.randint(0, 8)):
            if key == 'DEFAULT_TEXTURE':
                crater += [key, [tex()]]
            elif key == 'DEFAULT_ANIM':
                crater += [key, [anim()]]
            elif key == 'TEXTURE_PARMS':
                crater += [key, [1.0, 1.0]]
            elif key == 'POINTS':
                crater += [key, [rng.choice([3, 7, 12])]]
            elif key == 'TEXTURE_ANIM':
                pairs = []
                for _ in range(rng.randint(0, 5)):
                    v = rng.choice([[tex()], [tex(), anim()], [tex(), anim(), 'extra']])
                    pairs += [rng.choice(['g1', 'g2', 'g3']), v]
                crater += [key, pairs]
            else:
                crater += [key, [rng.choice([rng.uniform(0.5, 50.0), rng.randint(1, 40)])]]
        qs, crash = [], False
        keys = rng.sample(['TEXTURE_PARMS', 'SLOPE', 'RADIUS', 'DEPTH', 'POINTS'], rng.randint(0, 5))
        if rng.random() < 0.85:
            keys.insert(rng.randint(0, len(keys)), 'DEFAULT_TEXTURE')
        for key in keys:
            if key == 'DEFAULT_TEXTURE':
                frames = rng.choice([0, 1, 2, 3, 7, 9])
                crash |= frames == 1
                qs += [key, [rng.choice([10.0, 4.5, 15])] + [f'q{i}' for i in range(frames)]]
            elif key == 'POINTS':
                qs += [key, [rng.choice([5, 7])]]
            elif key == 'TEXTURE_PARMS':
                qs += [key, [1.0, 1.0]]
            else:
                qs += [key, [rng.choice([rng.uniform(0.5, 50.0), rng.randint(1, 40)])]]
        top = ['VERSION', [1.0]]
        if rng.random() < 0.9:
            top += ['CRATER', crater]
        if rng.random() < 0.8:
            top += ['QUICK_SAND', qs]
            crash |= 'DEFAULT_TEXTURE' not in qs
        cases.append((f'made-up {k}', [top], crash))
    return cases


def main():
    rng = random.Random(0x2dec)
    em = Emulated()
    runs, fails, port_only = 0, 0, 0
    work = []
    for world, data in world_files():
        for mode in ('none', 'all', 'half'):
            for anims in ([], [b'dust_fly01.flt']):
                work.append((f'{world} {mode} anims={len(anims)}', data, mode, anims, rng.randint(0, 1), False))
    for name, tree, crash in made_up(rng):
        work.append((name, ser(tree), 'half', [b'dust', b'boom'], rng.randint(0, 1), crash))
    work.append(('no file', None, 'none', [], 0, False))
    for name, data, mode, anims, restore, crash in work:
        names = sorted({s for s in zrd_strings(data)}) if data else []
        known = [] if mode == 'none' else names if mode == 'all' else [n for n in names if rng.random() < 0.5]
        mine = port(data, known, anims, restore)
        if crash:
            port_only += 1
            continue
        theirs = em.run(data, known, anims, restore)
        runs += 1
        if mine != theirs:
            fails += 1
            if fails <= 3:
                i = next(k for k in range(min(len(mine), len(theirs))) if mine[k] != theirs[k]) \
                    if any(a != b for a, b in zip(mine, theirs)) else min(len(mine), len(theirs))
                print(f'FAIL {name}: first difference at byte {i} (port {len(mine)}, original {len(theirs)})')
                print('  port    ', mine[max(0, i - 16):i + 32].hex())
                print('  original', theirs[max(0, i - 16):i + 32].hex())
    if fails:
        print(f'{fails} of {runs} runs differ')
        sys.stdout.flush(); os._exit(1)
    print(f'ok: {runs} runs identical (6 worlds x 6, {runs - 37} made-up, no file); {port_only} the original '
          f'crashes on read by the port alone')
    sys.stdout.flush(); os._exit(0)


def zrd_strings(data):
    """The strings in a binary reader file (type 3 nodes)."""
    out, i = [], 0
    while i + 8 <= len(data):
        t, n = struct.unpack_from('<iI', data, i)
        if t == 3 and n < 64 and i + 8 + n <= len(data):
            out.append(data[i + 8:i + 8 + n])
            i += 8 + n
        else:
            i += 8 if t in (1, 2, 4) else 4
    return out


if __name__ == '__main__':
    main()
