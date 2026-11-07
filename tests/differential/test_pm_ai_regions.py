"""PM's AI regions against the exe: test_ai_regions moved. The lookup by name 0x4d0fc0 with its list, the
short-lived builder, the loop's edges 0x4d0d50 and the inside test 0x4d0bd0 with 0x4d0e50, at 53 bits to the
bit, where each test stops too. Every reg_*.zrd in every reader archive, each asked twice, net files and a
missing name asked as regions, then made-up files; points round each region's corners and sides.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_ai_regions.py [archives]
"""
import ctypes
import os
import random
import struct
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools' / 'analysis'))
os.environ['MW3_X87'] = '53'  # the reference's precision: PM's AI, docs/pm-tests.md
import pm_floors  # noqa: E402
import test_pm_ai_nets as nets  # noqa: E402
import test_pm_ai_nodes as nodes  # noqa: E402
import test_pm_zar  # noqa: E402
import test_pm_zreader as zr  # noqa: E402
import zrd  # noqa: E402

PM = nets.PM
FIND, INSIDE, REGIONS = 0x4d0fc0, 0x4d0bd0, 0x62faa8


def bits(v):
    return struct.unpack('<I', struct.pack('<f', v))[0]


class Bench(nets.NetBench):
    stub_regions = False

    def setup(self, world):
        super().setup(world)
        self.call(0x4d1140, ecx=REGIONS)
        self.slot = self.alloc(12)
        self.got = []

    def region(self, r):
        b, end = self.r32(r + 0xc), self.r32(r + 0x10)
        n = (end - b) // 4 if b else 0
        cur = (self.r32(r + 0x18) - b) // 4
        o = [str(self.r32s(r + 4)), self.cstr(self.r32(r + 0x1c)).decode('latin1'), str(n), str(cur)]
        words = []
        for p in range(b, end, 4):
            e = self.r32(p)
            words += [self.r32(e + off) for off in range(4, 0x1c, 4)]
        return ' '.join(o + [f'{w:x}' for w in words]), words

    def find(self, name):
        r = self.call(FIND, args=[self.string(name)])
        self.got.append(r)
        return self.region(r) if r else ('none', [])

    def inside(self, i, pt):
        r = self.got[i]
        self.cpu._uc.mem_write(self.slot, struct.pack('<3I', *pt))
        v = self.call(INSIDE, ecx=r, args=[self.slot]) & 0xff
        return f'{v} {(self.r32(r + 0x18) - self.r32(r + 0xc)) // 4}'


def points(words, rng, n=24):
    """Round a region with these edge words: corners, middles of sides, a little in and out, and the far corners of its box."""
    f = [struct.unpack('<f', struct.pack('<I', w))[0] for w in words]
    corners = [f[i:i + 3] for i in range(0, len(f), 6)]
    if not corners:
        return []
    out = []
    xs, zs = [c[0] for c in corners], [c[2] for c in corners]
    for _ in range(n):
        k = rng.randrange(6)
        a = rng.choice(corners)
        b = rng.choice(corners)
        if k == 0:
            p = list(a)
        elif k == 1:
            p = [(a[j] + b[j]) / 2 for j in range(3)]
        elif k == 2:
            p = [a[0] + rng.uniform(-3, 3), a[1], a[2] + rng.uniform(-3, 3)]
        elif k == 3:
            p = [sum(xs) / len(xs) + rng.uniform(-5, 5), a[1], sum(zs) / len(zs) + rng.uniform(-5, 5)]
        elif k == 4:
            p = [rng.uniform(min(xs) - 9, max(xs) + 9), a[1] + rng.uniform(-40, 40), rng.uniform(min(zs) - 9, max(zs) + 9)]
        else:
            t = rng.random()
            p = [a[j] + (b[j] - a[j]) * t for j in range(3)]
        out.append([bits(v) for v in p])
    return out


def reg_file(nodes_, version=0x6c, name='reg_a'):
    top = ['version', [version], 'name', [name]]
    for k, v in nodes_:
        top += [k, v]
    return nodes.encode([top])


def made_up():
    n = nets.node
    sq = [('node_00', n((0.0, 0.0, 0.0), [1])), ('node_01', n((0.0, 0.0, 100.0), [2])), ('node_02', n((100.0, 0.0, 100.0), [3])),
          ('node_03', n((100.0, 0.0, 0.0), [0]))]
    open_ = [('node_00', n((0.0, 0.0, 0.0), [1])), ('node_01', n((0.0, 0.0, 100.0), [2])), ('node_02', n((100.0, 0.0, 100.0), []))]
    same = [('node_00', n((5.0, 0.0, 5.0), [1])), ('node_01', n((5.0, 0.0, 5.0), [2])), ('node_02', n((9.0, 0.0, 5.0), [0]))]
    gap = [('node_02', n((0.0, 0.0, 0.0), [3])), ('node_03', n((0.0, 0.0, 50.0), [4])), ('node_04', n((50.0, 0.0, 0.0), [2]))]
    return [('square', {'reg_a.zrd': reg_file(sq)}), ('open', {'reg_a.zrd': reg_file(open_)}), ('same point', {'reg_a.zrd': reg_file(same)}),
            ('gap', {'reg_a.zrd': reg_file(gap)}), ('wrong version', {'reg_a.zrd': reg_file(sq, 0x6a)}),
            ('name mismatch', {'reg_a.zrd': reg_file(sq, name='reg_other')}),
            ('reversed', {'reg_a.zrd': reg_file([('node_00', n((0.0, 0.0, 0.0), [3])), ('node_01', n((0.0, 0.0, 100.0), [0])),
                                                 ('node_02', n((100.0, 0.0, 100.0), [1])), ('node_03', n((100.0, 0.0, 0.0), [2]))])})]


def main():
    limit = int(sys.argv[1]) if len(sys.argv) > 1 else 10 ** 6
    p = test_pm_zar.load_pm_probe()
    p.probe_pm_ai_nets.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_int]
    p.probe_pm_ai_regions.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
    p.probe_pm_zrdr_load.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
    buf = ctypes.create_string_buffer(1 << 24)
    rng = random.Random(1999)
    bad = cases = regions = pts = inside = 0

    def run(label, root, rel, data, names):
        nonlocal bad, cases, regions, pts, inside
        emu = Bench()
        emu.setup(0)
        emu.reports = []
        emu.files[test_pm_zar.key(rel)] = bytearray(data)
        emu.call(zr.OPEN_ARCHIVE, args=[emu.string(rel.replace('/', '\\')), 1])
        p.probe_pm_zrdr_load(str(root).encode(), rel.encode(), b'')
        assert p.probe_pm_ai_nets(0, buf, len(buf)) >= 0  # a clean start
        script, theirs = [], []
        for name in names:
            line, words = emu.find(name)
            script.append(f'F {name}')
            theirs.append(line)
            regions += line != 'none'
            i = len(emu.got) - 1
            for pt in points(words, rng):
                script.append(f'T {i} {pt[0]:x} {pt[1]:x} {pt[2]:x}')
                r = emu.inside(i, pt)
                theirs.append(r)
                pts += 1
                inside += r[0] == '1'
        head = emu.r32(0x62faac)
        script.append('L')
        theirs += [f'listed {emu.cstr(emu.r32(q + 0x1c)).decode("latin1")}' for q in emu.list_of(head)]
        theirs += [f'report {r}' for r in emu.reports]
        n = p.probe_pm_ai_regions(('\n'.join(script) + '\n').encode(), buf, len(buf))
        ours = buf.value.decode('latin1').splitlines() if n >= 0 else [f'probe failed {n}']
        cases += 1
        if theirs != ours:
            bad += 1
            if bad <= 6:
                print(f'{label}: {len(theirs)} and {len(ours)} lines')
                for j, (a, b) in enumerate(zip(theirs, ours)):
                    if a != b:
                        print(f'  {script[j][:60] if j < len(script) else ""}\n  exe  {a[:150]}\n  port {b[:150]}')
                        break

    like = None
    for path in sorted(PM.glob('zbd/*/reader*.zbd'))[:limit]:
        data = path.read_bytes()
        like = like or data
        names = [n[:-4] for n, _, _ in zrd.entries(data)]
        regs = sorted(n for n in names if n.lower().startswith('reg_'))
        ns = sorted(n for n in names if n.lower().startswith('net_'))[:2]
        if regs:
            rel = path.relative_to(PM).as_posix()
            run(rel, PM, rel, data, regs + regs + ns + ['reg_no_such'])
    with tempfile.TemporaryDirectory() as d:
        for label, files in made_up():
            data = nets.archive_of(files, like)
            (Path(d) / 'crafted.zbd').write_bytes(data)
            run(label, d, 'crafted.zbd', data, ['reg_a', 'reg_a'])
    print(f'{cases} archives, {regions} regions asked, {pts} points, {inside} inside: {bad} differ')
    if not bad and limit > 100:
        pm_floors.floors(cases=(cases, 10), regions=(regions, 20), points=(pts, 400), inside=(inside, 40))
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
