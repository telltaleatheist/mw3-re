"""Craters and quicksand end to end (parts 5b, 6) against the original x86 (docs/craters.md,
sections 2 and 5): 0x4ff940 with the build 0x4ffad0 / 0x4ffe80, the clip
0x4fff20 -> zgeo_model's 0x50ec10 (the outline snapped 0x50ea50, each node
0x50ef70: kept clear 0x50f7b0 or clipped 0x50f120 with Weiler, convexify
and the polygon add), the tessellation 0x4fff80 (the crater node 0x50e950)
and the commit 0x50e850, on every mission world read by the original's
0x4ff520 and by the port's loaders (test_scenewalk).

zDEClient points at the world with one texture entry (a textured world
material for the crater, a stand-in dust animation). Craters land one after
another at random spots (some on top of earlier ones, so the cell limit and
the overlap refusal come up), 5..16 ring points, radius 3..25, depth 1..5,
textured or not. After each: the code, the reports, the dust, the history
entry (0x500850 stands in on the original's side), every cell's crater
count, each node whose model changed with its new model (header, vertices,
normals, polygons; materials and models as indices), and each new node
(name, flags, zone, model).

    cmake --build build && .venv/bin/python tests/differential/test_crater.py [craters per world]

BREAK=<n> breaks the port's inputs in a known way (see main); each must fail.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_di import World, probe  # noqa: E402
from test_gamez import POOL, POOL_SIZE, WORLDS, ZBD  # noqa: E402

probe.crater_setup.argtypes = [ctypes.c_int, ctypes.c_uint32]
probe.crater_run.argtypes = [ctypes.c_uint32, ctypes.c_int, ctypes.c_int] + [ctypes.c_float] * 5 + \
                            [ctypes.c_int, ctypes.c_void_p, ctypes.c_int]
probe.crater_save.argtypes = [ctypes.c_void_p, ctypes.c_int]
probe.crater_restore.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_int]
CAP = 1 << 24
GAVE_UP = struct.pack('<III', 0x70, 0x100, 0)  # the port's Weiler giving up where the original loops (fixes.md)
HANG = 300_000_000
BUF = ctypes.create_string_buffer(CAP)
DUST = 0x1234
FRAME = 100


class Craters(World):
    def __init__(self, name):
        super().__init__(name)
        cpu = self.cpu
        material = textured_material(self)
        self.dust = []
        cpu.stub_import(0x5bb584, self.memmove)
        cpu.stub_import(0x5bb658, self.sprintf)
        cpu.stub_import(0x5bb62c, lambda c: 0)  # fprintf (stderr)
        cpu.stub_function(0x508aa0, self.play, 0x24)
        # The history vector and the touched set run as the original's: their
        # static constructor 0x500670 (atexit registration skipped) with
        # operator new / delete and the locks stubbed.
        cpu.stub_import(0x5bb4bc, lambda c: self.alloc(c.arg(0)))  # operator new
        cpu.stub_import(0x5bb4c8, lambda c: 0)                     # operator delete
        cpu.stub_import(0x5bb4f8, lambda c: c.ecx)                 # std::_Lockit
        cpu.stub_import(0x5bb4f4, lambda c: 0)                     # ~_Lockit
        cpu.stub_function(0x5ac140, lambda c: 0)                   # atexit
        cpu.stub_function(0x5a8a50, self.write_record, 8)          # zEntity's record write
        cpu.call(0x500670)
        self.records = []
        wd = self.wd
        tex = self.alloc(12)
        cpu.write(tex, struct.pack('<III', 0, self.material_ptr(material), DUST))
        for a, v in ((0x60c0c0, self.world), (0x60c0c4, wd), (0x60bfbc, tex), (0x60bfb8, 1), (0x72f338, FRAME),
                     (0x60c0c8, 0), (0x60bfe8, 1), (0x60bffc, self.material_ptr(material)),
                     (0x60c000, self.material_ptr(material + 1)),
                     (0x5ee3e0, struct.unpack('<I', struct.pack('<f', 0.001))[0])):
            cpu.u32(a, v)
        cpu.write(0x60cb38, bytes(0x24 * 63 + 0x18))
        self.material = material

    def memmove(self, c):
        self.cpu.write(c.arg(0), self.cpu.read(c.arg(1), c.arg(2)))
        return c.arg(0)

    def cstr(self, a):
        out = bytearray()
        while True:
            b = self.cpu.read(a + len(out), 1)[0]
            if not b:
                return bytes(out)
            out.append(b)

    def sprintf(self, c):
        """The node namer's "%s" and the save's "Crater%d" / "QSand%d"."""
        fmt = self.cstr(c.arg(1))
        if fmt == b'%s':
            s = self.cstr(c.arg(2))
        else:
            assert fmt.endswith(b'%d') and fmt.count(b'%') == 1, fmt
            s = fmt[:-2] + str(struct.unpack('<i', struct.pack('<I', c.arg(2)))[0]).encode()
        self.cpu.write(c.arg(0), s + b'\0')
        return len(s)

    def material_ptr(self, i):
        return self.mat_blocks[i // 1000] + (i % 1000) * 0x2c

    def history(self):
        b, e = self.u32(0x60bf94), self.u32(0x60bf98)
        return [struct.unpack('<13I', self.cpu.read(a, 0x34)) for a in range(b, e, 0x34)] if b else []

    def history_out(self, before):
        h = self.history()
        if len(h) <= before:
            return b''
        e = list(h[-1])
        n = 11 if e[0] == 3 else 10
        words = e[:1 + n]
        for k in ((3,) if words[0] == 1 else (3, 4)):  # materials as indices (the port's records)
            words[k] = self.material_index(words[k]) & 0xffffffff
        words[-1] = 0  # the owner
        return struct.pack(f'<{len(words)}I', *words)

    def write_record(self, c):
        name = self.cstr(c.edx)
        rec = list(struct.unpack('<13I', c.read(c.arg(0), 0x34)))
        self.records.append(bytes(c.read(c.arg(0), 0x34)))
        out = name + b'\0' + struct.pack('<I', c.arg(1))
        if rec[12]:
            out += struct.pack('<I', rec[12])
        else:
            for k in ((3,) if rec[0] == 1 else (3, 4)):
                rec[k] = self.material_index(rec[k]) & 0xffffffff
            rec[11 if rec[0] == 3 else 10] = 0
            if rec[0] == 1:
                rec[11] = 0  # left as the exe's stack had it (the port's 0)
            out += struct.pack('<13I', *rec)
        self.saved += out
        return 0

    def save(self):
        self.saved, self.records = bytearray(), []
        self.cpu.call(0x500b50, ecx=0)
        return bytes(self.saved)

    def restore(self, i, budget=2_000_000_000):
        before, _ = self.live_nodes()
        hist = len(self.history())
        self.reports = []
        rec = self.alloc(0x34)
        self.cpu.write(rec, self.records[i])
        self.cpu.call(0x500c20, stack_args=(rec, 0, 0), count=budget)
        out = bytearray()
        for level, line in self.reports:
            out += struct.pack('<III', 0x70, level, line)
        out += struct.pack('<I', 0xaaaa) + self.history_out(hist) + struct.pack('<I', 0xbbbb)
        out += struct.pack('<II', len(self.history()), self.set_size())
        self.changes(out, before)
        return bytes(out)

    def set_size(self):
        return self.u32(0x60bfac)

    def play(self, c):
        self.dust.append((c.ecx, struct.pack('<3I', c.arg(0), c.arg(1), c.arg(2))))
        return 0

    def live_nodes(self):
        bs = self.u32(0x5ea830)
        blocks = [self.u32(self.u32(0x60bf38) + 4 * b) for b in range(self.u32(0x60bf30))]
        out = []
        for b, base in enumerate(blocks):
            for i in range(bs):
                a = base + 0xd4 * i
                out.append((a, (self.u32(a + 0xd0) & 0x1000000) != 0, self.u32(a + 0x3c)))
        return out, blocks

    def model_index(self, p):
        if not p:
            return 0xffffffff
        pool = self.u32(POOL)
        return (p - pool) // 0x60 if pool <= p < pool + 0x60 * self.u32(POOL_SIZE) else 0xfffffffe

    def dump_model(self, out, p):
        out += struct.pack('<I', self.model_index(p))
        if not p:
            return
        out += self.cpu.read(p, 0x34) + self.cpu.read(p + 0x48, 0x14)
        npoly, nv, nn = (self.u32(p + o) for o in (0x10, 0x14, 0x18))
        out += self.cpu.read(self.u32(p + 0x38), 12 * nv) if nv else b''
        out += self.cpu.read(self.u32(p + 0x3c), 12 * nn) if nn else b''
        polys = self.u32(p + 0x34)
        for i in range(npoly):
            q = polys + 0x24 * i
            flags = self.u32(q)
            k = flags & 0xff
            out += struct.pack('<II', flags, self.u32(q + 4))
            out += self.cpu.read(self.u32(q + 8), 4 * k)
            if flags & 0x200:
                out += self.cpu.read(self.u32(q + 0xc), 4 * k)
            uv = self.u32(q + 0x10)
            out += struct.pack('<I', 1 if uv else 0)
            if uv:
                out += self.cpu.read(uv, 8 * k)
            out += self.cpu.read(self.u32(q + 0x14), 12 * k)
            out += struct.pack('<iI', self.material_index(self.u32(q + 0x1c)), self.u32(q + 0x20))

    def crater(self, flags, points, ground, depth, radius, x, y, z, dust, budget=2_000_000_000):
        cpu = self.cpu
        before, _ = self.live_nodes()
        hist = len(self.history())
        self.reports, self.dust = [], []
        order = self.alloc(44)
        if dust < 2:
            cpu.write(order, struct.pack('<IiIIff3fI', flags, points, self.material_ptr(ground) if ground >= 0 else 0,
                                         0, depth, radius, x, y, z, 0))
            cpu.call(0x4ff940, ecx=order, edx=dust, count=budget)
        else:  # a quicksand pit
            cpu.write(order, struct.pack('<Ii3Iff3fI', flags, points, 0, 0, 0, depth, radius, x, y, z, 0))
            cpu.call(0x502110, ecx=order, count=budget)
        code = cpu.uc.reg_read(19)
        out = bytearray()
        for level, line in self.reports:
            out += struct.pack('<III', 0x70, level, line)
        for anim, at in self.dust:
            out += struct.pack('<II', 0x71, 1 if anim == DUST else 0) + at
        out += struct.pack('<II', code, 0xaaaa)
        out += self.history_out(hist)
        out += struct.pack('<I', 0xbbbb)
        self.changes(out, before)
        return code, bytes(out)

    def changes(self, out, before):
        cpu = self.cpu
        wd = self.wd
        nx, nz = self.u32(wd + 0x88), self.u32(wd + 0x8c)
        rows = self.u32(wd + 0x90)
        for j in range(nz):
            row = self.u32(rows + 4 * j)
            out += bytes(cpu.read(row + 0x48 * i + 0x39, 1)[0] for i in range(nx))
        after, _ = self.live_nodes()
        for i, (a, live, model) in enumerate(after):
            if i < len(before) and before[i][1]:
                if not live:
                    out += struct.pack('<II', 3, i)
                elif model != before[i][2]:
                    out += struct.pack('<II', 1, i)
                    self.dump_model(out, model)
            elif live:
                out += struct.pack('<II', 2, i) + cpu.read(a, 0x24)
                out += struct.pack('<II', self.u32(a + 0x24), cpu.read(a + 0x30, 1)[0])
                self.dump_model(out, model)


def textured_material(emu):
    """The first textured material (flag byte bit 0)."""
    for i in range(4000):
        b = emu.mat_blocks[i // 1000]
        if not b:
            break
        if emu.cpu.read(b + (i % 1000) * 0x2c + 1, 1)[0] & 1:
            return i
    raise SystemExit('no textured material')


def main():
    per_world = int(sys.argv[1]) if len(sys.argv) > 1 else 40
    rng = random.Random(int(os.environ.get('SEED', '1')))
    brk = int(os.environ.get('BREAK', '0'))
    runs = fails = made = 0
    codes = {}
    for name in WORLDS:
        emu = Craters(name)
        material = emu.material
        path = str(ZBD / name / 'gamez.zbd').encode()
        if probe.sw_load(path) != 0:
            raise SystemExit(f'{name}: the port could not read the world')
        probe.crater_setup(material, FRAME)
        x0, z0, w, d = emu.area
        spots = []
        broken = False
        for k in range(per_world):
            if spots and rng.random() < 0.3:
                x, z = rng.choice(spots)
                x += rng.uniform(-15, 15)
                z += rng.uniform(-15, 15)
            else:
                x, z = x0 + rng.uniform(0.02, 0.98) * w, z0 + rng.uniform(0.02, 0.98) * d
            spots.append((x, z))
            flags = 0x100c if rng.random() < 0.85 else 0x1008
            points = rng.randint(5, 16)
            depth, radius = rng.uniform(1, 5), rng.uniform(3, 25)
            y = rng.uniform(-5, 40)
            dust = rng.choice((0, 1, 1, 2))  # 2: a quicksand pit
            mx = x + (0.5 if brk == 1 else 0.0)
            mpoints = points + (1 if brk == 2 else 0)
            n = probe.crater_run(flags, mpoints, material, depth, radius, mx, y, z, dust, BUF, CAP)
            got = BUF.raw[:n]
            if GAVE_UP in got:  # the original must loop; its state is then lost, so on to the next world
                try:
                    emu.crater(flags, points, material, depth, radius, x, y, z, dust, budget=HANG)
                    fails += 1
                    print(f'{name} crater {k}: the port gave up but the original returned')
                except Exception:
                    codes['hang'] = codes.get('hang', 0) + 1
                broken = f'crater {k}'
                break
            code, want = emu.crater(flags, points, material, depth, radius, x, y, z, dust)
            runs += 1
            kind = ('quicksand' if dust == 2 else 'crater') + ('_made' if code == 0 else '_refused')
            codes[kind] = codes.get(kind, 0) + 1
            made += code == 0
            if got != want:
                fails += 1
                if fails <= 3:
                    i = next((j for j in range(min(len(got), len(want))) if got[j] != want[j]), min(len(got), len(want)))
                    print(f'FAIL {name} crater {k} (code {code}, {points} points, r {radius:.1f}): first difference at '
                          f'byte {i} (port {len(got)}, original {len(want)})')
                    print('  port    ', got[max(0, i - 16):i + 32].hex())
                    print('  original', want[max(0, i - 16):i + 32].hex())
        # zEntity's save (the history as records) and restore: the Dummy record
        # resets the world (every touched node re-read from gamez.zbd, crater
        # nodes freed, the cells' counts 0, the history and set emptied),
        # then each record remakes its crater or pit (no dust).
        if broken:
            print(f'{name}: stopped at a hang ({broken})', flush=True)
            continue
        want = emu.save()
        n = probe.crater_save(BUF, CAP)
        runs += 1
        if BUF.raw[:n] != want:
            fails += 1
            print(f'{name}: the saved records differ (port {n}, original {len(want)} bytes)')
        for i in range(len(emu.records)):
            n = probe.crater_restore(i if not (brk == 3 and i == 0) else 1, BUF, CAP)  # BREAK=3: no reset
            got = BUF.raw[:n]
            if GAVE_UP in got:
                try:
                    emu.restore(i, budget=HANG)
                    fails += 1
                    print(f'{name} restore {i}: the port gave up but the original returned')
                except Exception:
                    codes['hang'] = codes.get('hang', 0) + 1
                broken = f'restore {i}'
                break
            want = emu.restore(i)
            runs += 1
            codes['restored'] = codes.get('restored', 0) + 1
            if got != want:
                fails += 1
                if fails <= 3:
                    j = next((k for k in range(min(len(got), len(want))) if got[k] != want[k]), min(len(got), len(want)))
                    print(f'FAIL {name} restore {i}: first difference at byte {j} (port {len(got)}, original {len(want)})')
                    print('  port    ', got[max(0, j - 16):j + 32].hex())
                    print('  original', want[max(0, j - 16):j + 32].hex())
        if broken:
            print(f'{name}: stopped at a hang ({broken})', flush=True)
            continue
        # Last, an untextured crater: the original writes the floor centre's
        # UV through a null pointer (0x4fffe5..); the port makes it.
        # (when it gets that far: a crater refused earlier returns as usual).
        x, z = spots[0]
        n = probe.crater_run(0x4, 8, material, 2.0, 8.0, x + 40, 10.0, z + 40, 0, BUF, CAP)
        got = BUF.raw[:n]
        port_code = struct.unpack_from('<i', got, got.index(struct.pack('<I', 0xaaaa)) - 4)[0]
        try:
            code, want = emu.crater(0x4, 8, material, 2.0, 8.0, x + 40, 10.0, z + 40, 0)
            if port_code == 0 or got != want:
                print(f'{name}: untextured: the original returned {code}, the port {port_code}')
                fails += 1
        except Exception:
            if port_code != 0:
                print(f'{name}: untextured: the original crashed, the port returned {port_code}')
                fails += 1
            codes['untextured_crash'] = codes.get('untextured_crash', 0) + 1
        runs += 1
        print(f'{name}: {per_world} craters', flush=True)
    if fails:
        print(f'{fails} of {runs} craters differ')
        sys.stdout.flush(); os._exit(1)
    print(f'ok: {runs} craters identical ({made} made; codes {sorted(codes.items(), key=str)})')
    sys.stdout.flush(); os._exit(0)


if __name__ == '__main__':
    main()
