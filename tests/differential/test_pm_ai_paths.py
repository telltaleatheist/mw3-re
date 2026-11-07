"""How PM's AI finds its way, against the exe: test_ai_paths moved, on test_pm_ai_nets' nets. The nearest
step 0x4e5580; the finder 0x4d3080, 0x4d3120, with the search 0x4d33b0, 0x4d32c0, 0x4d33e0, 0x4d3580 and
the way kept 0x4d37b0, 0x4d3750, 0x4d3700, 0x4d3670; the steps' tests of the pilot 0x4d4c90, 0x4dade0,
0x4d6790, 0x4d6e60. Compared at 53 bits to the bit, each way's length too. Every net for the nearest step;
ways in nets of 30 nodes or fewer, as the search is depth first, and on made-up nets with a known answer.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_ai_paths.py [archives]
"""
import ctypes
import os
import random
import struct
import sys
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
NEAREST, FIND, NEXT, PLACE = 0x4e5580, 0x4d3080, 0x4d3120, 0x5cb260
MAX_NODES = 30
NEAR_X = 3000.0 - 2.0 ** -12  # the float below 3000
NODE_KINDS = {k for k, v in nets.KINDS.items() if v in ('node', 'backup', 'mcbwait', 'deploy', 'via', 'terminus')}


def bits(v):
    return struct.unpack('<I', struct.pack('<f', v))[0]


class Bench(nets.NetBench):
    def setup(self, world):
        super().setup(world)
        self.slots = {}
        self.place = self.alloc(12)
        self.stand(PLACE, lambda c: self.place)

    def named(self, c):
        """A valve: +0x04 an object whose vt+4 reads its value. Sounds are looked up here too, harmlessly."""
        s = self.cstr(c.arg(0)) if c.arg(0) else b''
        if s not in self.ids:
            v, obj, vt, slot = self.alloc(0xc), self.alloc(8), self.alloc(8), self.alloc(4)
            code = self.cpu.code(b'\xa1' + struct.pack('<I', slot) + b'\xc3')  # mov eax, [slot]; ret
            self.cpu._uc.mem_write(v + 4, struct.pack('<I', obj))
            self.cpu._uc.mem_write(obj, struct.pack('<I', vt))
            self.cpu._uc.mem_write(vt + 4, struct.pack('<I', code))
            self.ids[s] = v
            self.names[v] = s.decode('latin1')
            self.slots[s.decode('latin1')] = slot
        return self.ids[s]

    def compiled(self, nid):
        for x in self.list_of(self.r32(nets.REGISTRY + 4)):
            if self.r32(x) in nets.NETS and self.r32s(x + 4) == nid:
                b, end = self.r32(x + 0xc), self.r32(x + 0x10)
                return x, [self.r32(q) for q in range(b, end, 4)]
        return 0, []

    def node_at(self, els, k):
        for p in els:
            if self.r32(p) in NODE_KINDS:
                if k == 0:
                    return p
                k -= 1
        return 0

    def brain(self, jets, name):
        """The pilot as the steps ask: +0x100's +4 the offset of +0x11c, the vehicle; its +4 the name, slot +0x58 the mech."""
        b, ctl, veh, vt, mech, nm = (self.alloc(0x200), self.alloc(8), self.alloc(0x10), self.alloc(0x60), self.alloc(0x400),
                                     self.string(name))
        w = lambda a, v: self.cpu._uc.mem_write(a, struct.pack('<I', v))  # noqa: E731
        w(b + 0x100, ctl), w(b + 0x11c, veh), w(veh, vt), w(veh + 4, nm), w(mech + 1000, jets)
        w(vt + 0x58, self.cpu.code(b'\xb8' + struct.pack('<I', mech) + b'\xc3'))
        return b

    def way(self, els, holder):
        f = self.r32(holder)
        if not f:
            return ' none'
        head, out = self.r32(f + 8), []
        p = self.r32(head)
        while p != head:
            out.append(self.token(els, self.r32(p + 8)))
            p = self.r32(p)
        return f' {self.b(f + 0x14)} {self.r32(f + 0x10):x} {len(out)}' + ''.join(f' {t}' for t in out)

    def query(self, q):
        op = q[0]
        if op == 'V':
            if q[1] in self.slots:
                self.cpu._uc.mem_write(self.slots[q[1]], struct.pack('<i', q[2]))
            return []
        x, els = self.compiled(q[1])
        if op == 'N':
            pt = self.alloc(12)
            self.cpu._uc.mem_write(pt, struct.pack('<3I', *q[2:5]))
            return [f'near {self.token(els, self.call(NEAREST, ecx=x, args=[pt]))}']
        _, n, a, b, jets, name, px, py, pz, follows, flip, value, *rest = q
        self.cpu._uc.mem_write(self.place, struct.pack('<3I', px, py, pz))
        start, target = self.node_at(els, a), self.node_at(els, b)
        holder = self.alloc(4)
        r = self.call(FIND, ecx=holder, args=[self.brain(jets, name), x, start, target]) & 0xff
        out = [f'way {r}{self.way(els, holder)}']
        if flip != '-':
            self.query(('V', flip, value))
        at = self.node_at(els, rest[0]) if rest else start  # asked from further on: the way before it dropped
        for _ in range(follows):
            if not at:
                break
            at = self.call(NEXT, ecx=holder, args=[at])
            out.append(f'next {self.token(els, at) if at else -1}{self.way(els, holder)}')
        return out


def line(q):
    if q[0] == 'V':
        return f'V {q[1]} {q[2]}'
    if q[0] == 'N':
        return f'N {q[1]} {q[2]:x} {q[3]:x} {q[4]:x}'
    _, n, a, b, jets, name, px, py, pz, follows, flip, value, *rest = q
    return f'P {n} {a} {b} {jets} {name} {px:x} {py:x} {pz:x} {follows} {flip} {value} {rest[0] if rest else -1}'


def net_info(root_files):
    """Each net: number -> (positions, gates, valves), from the file's text of names."""
    out = {}
    for name, data in root_files.items():
        low = name.lower()
        if not (low.startswith('net_') and low.endswith('.zrd')):
            continue
        n = int(low[4:6]) if low[4:6].isdigit() else None
        if n is None:
            continue
        try:
            top = zrd.node(data, 0)[0][0]
        except Exception:
            continue
        pos, gates, valves = {}, set(), set()

        def walk(v, o):
            if isinstance(v, list):
                for x in v:
                    walk(x, o)
            else:
                o.append(v)
        for i in range(0, len(top) - 1, 2):
            k, v = top[i], top[i + 1]
            if not (isinstance(k, str) and k.startswith('node_') and isinstance(v, list) and v):
                continue
            if isinstance(v[0], int) and len(v) > 1 and isinstance(v[1], list) and len(v[1]) == 3:
                pos[k] = tuple(float(x) for x in v[1])
            rest = v[3:] if isinstance(v[0], int) else v[1:]
            for j in range(0, len(rest) - 1, 2):
                flat = []
                walk(rest[j + 1], flat)
                names = [x for x in flat if isinstance(x, str) and ' ' not in x]
                if rest[j] == 'gate':
                    gates.update(names)
                elif rest[j] in ('valve', 'valveunion', 'valve_assign', 'deploy', 'mcbwait'):
                    valves.update(names)
        out[n] = ([pos[k] for k in sorted(pos)], sorted(gates), sorted(valves))
    return out


def queries_for(info, rng, paths=8, near=8):
    qs = []
    for n, (pos, gates, valves) in sorted(info.items()):
        if not pos:
            continue
        for _ in range(near):
            p = rng.choice(pos)
            qs.append(('N', n, bits(p[0] + rng.uniform(-60, 60)), bits(p[1] + rng.uniform(-60, 60)), bits(p[2] + rng.uniform(-60, 60))))
        for _ in range(paths if len(pos) <= MAX_NODES else 0):
            for v in valves:
                if rng.random() < 0.5:
                    qs.append(('V', v, rng.choice([0, 0, 1, 1, 2])))
            a, b = rng.randrange(len(pos)), rng.randrange(len(pos))
            p = pos[a] if rng.random() < 0.8 else pos[b]
            stand = [bits(p[k] + rng.uniform(-8, 8)) for k in range(3)]
            name = rng.choice(gates) if gates and rng.random() < 0.5 else 'nobody'
            flip, value = '-', 0
            if valves and rng.random() < 0.5:
                flip, value = rng.choice(valves), rng.choice([0, 1, 2])
            qs.append(('P', n, a, b, rng.choice([0, 1]), name, *stand, rng.randrange(1, 8), flip, value))
    return qs


def made_up():
    ladder = [('node_00', nets.node((0.0, 0.0, 0.0), [1, 2])),
              ('node_01', nets.node((50.0, 0.0, 0.0), [3], [['jump', [1, 30.0]]])),
              ('node_02', nets.node((0.0, 0.0, 80.0), [3], [['gate', ['tank_7']]])),
              ('node_03', nets.node((50.0, 0.0, 80.0), [4], [['valve', [1, 'v_ladder']]])),
              ('node_04', nets.node((100.0, 0.0, 80.0), [5, 0])),
              ('node_05', nets.node((150.0, 0.0, 80.0), [0])),
              ('node_06', [[0, 2], 'valveunion', [[1, 'u_a', 1], [1, 'u_b', 2]]])]
    ring = [(f'node_{i:02d}', nets.node((float(30 * i), 0.0, float(7 * (i % 3))), [(i + 1) % 9, (i + 4) % 9])) for i in range(9)]
    # Two ways of one length: the first found is kept.
    square = [('node_00', nets.node((0.0, 0.0, 0.0), [1, 2])), ('node_01', nets.node((30.0, 0.0, 0.0), [3])),
              ('node_02', nets.node((0.0, 0.0, 30.0), [3])), ('node_03', nets.node((30.0, 0.0, 30.0), []))]
    # The short way behind a valve, the long one behind a union: shut, the search runs again from where the pilot is.
    fork = [('node_00', nets.node((0.0, 0.0, 0.0), [1, 2])), ('node_01', nets.node((50.0, 0.0, 0.0), [3], [['valve', [1, 'v_f', 1]]])),
            ('node_02', nets.node((0.0, 0.0, 300.0), [3], [['valveunion', [[1, 'u_a', 1], [1, 'u_b', 2]]]])),
            ('node_03', nets.node((60.0, 0.0, 300.0), [4])), ('node_04', nets.node((120.0, 0.0, 300.0), [5])),
            ('node_05', nets.node((180.0, 0.0, 300.0), []))]
    # Seen from 0: 0 and 1 at the same stored distance though the register's sum differs, 2 and 3 exactly equal.
    near = [('node_00', nets.node((3000.0, 0.0, 0.0), [1])), ('node_01', nets.node((NEAR_X, 1.1, 0.0), [2])),
            ('node_02', nets.node((0.0, 0.0, 7.0), [3])), ('node_03', nets.node((0.0, 0.0, -7.0), [0]))]
    z = bits(0.0)
    s0 = (z, z, z)
    return [('ladder', {'net_05.zrd': nets.net_file(ladder)}, []), ('ring', {'net_07.zrd': nets.net_file(ring)}, []),
            ('square', {'net_02.zrd': nets.net_file(square)}, [('P', 2, 0, 3, 0, 'nobody', *s0, 4, '-', 0)]),
            ('fork', {'net_03.zrd': nets.net_file(fork)}, [
                ('V', 'v_f', 1), ('V', 'u_a', 1), ('P', 3, 0, 5, 0, 'nobody', *s0, 8, '-', 0),
                ('P', 3, 0, 5, 0, 'nobody', *s0, 8, 'v_f', 0), ('P', 3, 0, 5, 0, 'nobody', *s0, 8, 'u_a', 0),
                ('V', 'u_a', 0), ('V', 'u_b', 2), ('P', 3, 0, 5, 0, 'nobody', *s0, 8, '-', 0),
                ('V', 'u_b', 1), ('V', 'v_f', 0), ('P', 3, 0, 5, 0, 'nobody', *s0, 4, '-', 0),
                ('V', 'v_f', 1), ('P', 3, 0, 5, 0, 'nobody', *s0, 8, '-', 0, 3), ('P', 3, 0, 5, 0, 'nobody', *s0, 8, 'v_f', 0, 1),
                ('V', 'v_f', 1), ('P', 3, 3, 5, 0, 'nobody', *s0, 4, '-', 0),
                ('P', 3, 0, 5, 0, 'nobody', bits(180.0), z, bits(300.0), 2, '-', 0, 3)]),  # at the target, asked from off the way
            ('near', {'net_04.zrd': nets.net_file(near)}, [('N', 4, z, z, z), ('N', 4, bits(0.0), bits(0.0), bits(0.5)),
                                                          ('N', 4, bits(1.0), z, z)])]


def main():
    limit = int(sys.argv[1]) if len(sys.argv) > 1 else 10 ** 6
    p = test_pm_zar.load_pm_probe()
    p.probe_pm_ai_nets.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_int]
    p.probe_pm_ai_paths.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
    p.probe_pm_zrdr_load.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
    buf = ctypes.create_string_buffer(1 << 24)
    rng = random.Random(1999)
    bad = cases = total = ways = found = 0

    def run(label, root, rel, data, files, extra=()):
        nonlocal bad, cases, total, ways, found
        qs = queries_for(net_info(files), rng) + list(extra)
        if not qs:
            return
        emu = Bench()
        emu.setup(0)
        emu.files[test_pm_zar.key(rel)] = bytearray(data)
        emu.call(zr.OPEN_ARCHIVE, args=[emu.string(rel.replace('/', '\\')), 1])
        p.probe_pm_zrdr_load(str(root).encode(), rel.encode(), b'')
        emu.build()
        assert p.probe_pm_ai_nets(0, buf, len(buf)) >= 0
        theirs = [ln for q in qs for ln in emu.query(q)]
        n = p.probe_pm_ai_paths(('\n'.join(line(q) for q in qs) + '\n').encode(), buf, len(buf))
        ours = buf.value.decode('latin1').splitlines() if n >= 0 else [f'probe failed {n}']
        cases += 1
        total += len(theirs)
        ways += sum(ln.startswith('way') for ln in theirs)
        found += sum(ln.startswith('way 1') for ln in theirs)
        if theirs != ours:
            bad += 1
            if bad <= 6:
                print(f'{label}: {len(theirs)} and {len(ours)} lines')
                for a, b in zip(theirs, ours):
                    if a != b:
                        print(f'  exe  {a[:160]}\n  port {b[:160]}')
                        break

    like = None
    for path in sorted(PM.glob('zbd/*/reader*.zbd'))[:limit]:
        data = path.read_bytes()
        like = like or data
        files = {n: data[o:o + s] for n, o, s in zrd.entries(data)}
        run(path.relative_to(PM).as_posix(), PM, path.relative_to(PM).as_posix(), data, files)
    import tempfile
    with tempfile.TemporaryDirectory() as d:
        for label, files, extra in made_up():
            data = nets.archive_of(files, like)
            (Path(d) / 'crafted.zbd').write_bytes(data)
            run(label, d, 'crafted.zbd', data, files, extra)
    print(f'{cases} archives, {total} answers, {ways} ways, {found} found: {bad} differ')
    if not bad and limit > 100:
        pm_floors.floors(cases=(cases, 40), answers=(total, 6000), ways=(ways, 1300), found=(found, 900))
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
