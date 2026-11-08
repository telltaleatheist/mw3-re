"""PM's nets compiled against the exe: 0x4022b0 for 1..99 on every shipped reader archive and on made-up
nets, through the builder 0x4c39e0, the slot counts 0x4c4490, 0x4c4300, the steps 0x4c4c20, 0x4c45c0,
0x4c4bb0, each step's class and attach, every compiled net's steps compared as text. New in PM: via and
terminus nodes, the net class for a net with a via, and water as ground under a node. The ground stands
in on both sides by one rule (the query 0x4ff8d0); the pick 0x4af770 runs on each. Then the waynets
(waynet_N in each archive, 0x4c55b0 with its points 0x47c5f0), moved from test_nav_waynet. What a step does
on a walk isn't here, nor the arrangenodes point (0x4e57e0 answers none).

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_ai_nets.py [archives]
"""
import ctypes
import os
import random
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools' / 'analysis'))
os.environ['MW3_X87'] = '53'  # the reference's precision: load-time code, and PM's AI
import pm_floors  # noqa: E402
import test_pm_ai_nodes as nodes  # noqa: E402
import test_pm_zar  # noqa: E402
import test_pm_zreader as zr  # noqa: E402
import zrd  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn.x86_const import UC_X86_REG_ESP  # noqa: E402

PM = nodes.PM
REGISTRY, NULL_STEP, DEFAULT, WORLD = 0x627c38, 0x627c08, 0x627cc8, 0x62ca90
BUILD, QUERY, WAYNET = 0x4022b0, 0x4ff8d0, 0x4e57e0
EDIT, EDIT_END, WAYNET_VT, POINT_VT = 0x4c39e0, 0x4c3e60, 0x5e5c00, 0x5e5c70
NETS = {0x5ecd14: 0, 0x5ed558: 1}
KINDS = {0x5edcf0: 'speed', 0x5eddd0: 'wait_interval', 0x5edeb0: 'wait_time', 0x5edf90: 'valve', 0x5ee1d0: 'scan',
         0x5ee2b0: 'canleave', 0x5ee36c: 'canleave_not', 0x5eed00: 'jump', 0x5ee5e8: 'sound', 0x5ee6d0: 'gate',
         0x5ee7b0: 'shunt', 0x5ee970: 'union', 0x5eea50: 'assign', 0x5eef64: 'node', 0x5ef020: 'backup',
         0x5ee508: 'mcbwait', 0x5ef11c: 'deploy', 0x5ef498: 'via', 0x5ef578: 'terminus'}
TESTS = {0x4e9ea0: 'aim1', 0x4e9eb0: 'aim0', 0x4e9ec0: 'hit1', 0x4e9ed0: 'hit0', 0x4e9ee0: 'time1', 0x4e9ef0: 'time0',
         0x4e9f00: 'leave1', 0x4e9f20: 'leave0'}
# MSVCRT's floor as x87 code (test_scenewalk.py).
FLOOR = bytes.fromhex('83ec04 d93c24 668b0424 6625fff3 660d0004 6689442402 d96c2402 dd442408 d9fc d92c24 83c404 c3')
OFFS = [-30.0, -12.0, -5.0, 0.0, 4.0, 9.5, 10.0, 25.0]


def f32(v):
    return struct.unpack('<f', struct.pack('<f', v))[0]


def stand_in_hits(x, y, z):
    """(height word, water) for each hit, as hits_at in tests/probe_pm_ai_nets.cpp; x, y, z as the words."""
    b = z
    k = ((x ^ (b >> 3) ^ ((b << 7) & 0xffffffff)) * 2654435761) & 0xffffffff
    y = struct.unpack('<f', struct.pack('<I', y))[0]
    return [(struct.pack('<f', f32(y + OFFS[(k >> (2 + 3 * i)) & 7])), (k >> (20 + i)) & 1) for i in range((k >> 29) & 3)]


class NetBench(nodes.Bench):
    def setup(self, world):
        self.stand_ins()
        self.reports = []
        self.grounded = self.water_hits = 0
        self.stand(nodes.REPORT, lambda c: self.reports.append(f'{c.arg(0):x},{c.arg(2):x}') or 0)
        self.stand(WAYNET, lambda c: 0)
        self.stand(zr.OP_NEW, self.guarded)
        self.cpu.stub_import(0x5bb4f8, lambda c: c.ecx)  # std::_Lockit, by MW3's slots
        self.cpu.stub_import(0x5bb4f4, lambda c: 0)
        self.cpu.stub_import(0x5bb5a8, self.strrchr)
        self.cpu.stub_import(0x5bb658, self.sprintf)
        self.cpu.stub_import(0x5bb5b4, self.strtol)
        self.cpu.import_code(0x5bb66c, FLOOR)
        self.call(0x4d2fe0, ecx=REGISTRY)
        self.call(0x4ed2d0)
        self.call(0x410ac0, ecx=NULL_STEP)
        self.call(0x4d3b60, ecx=DEFAULT)
        self.mats = [self.alloc(0x2c), self.alloc(0x2c)]
        self.cpu._uc.mem_write(self.mats[1] + 0x20, struct.pack('<I', 1))
        if world:
            w = self.alloc(0x40)
            self.cpu._uc.mem_write(WORLD, struct.pack('<I', w))
            self.native_stub(QUERY, self.query)

    def guarded(self, c):
        """A link past the nodes reads just past the slot map: -1 there, the "no node" of the port's FIX (bug)."""
        p = self.alloc(c.arg(0) + 16)
        self.cpu._uc.mem_write(p + c.arg(0), b'\xff' * 16)
        return p

    def strrchr(self, c):
        i = self.cstr(c.arg(0)).rfind(bytes([c.arg(1) & 0xff]))
        return c.arg(0) + i if i >= 0 else 0

    def strtol(self, c):
        s = self.cstr(c.arg(0))
        m = re.match(rb'\s*[-+]?\d*', s)
        if c.arg(1):
            self.cpu._uc.mem_write(c.arg(1), struct.pack('<I', c.arg(0) + m.end()))
        return int(m[0] or b'0') & 0xffffffff if m[0].strip(b' +-') else 0

    def sprintf(self, c):
        k = [2]

        def one(m):
            v = c.arg(k[0])
            k[0] += 1
            return self.cstr(v).decode('latin1') if m[0][-1] == 's' else m[0] % v
        out = re.sub(r'%[-0-9]*[sdxu]', one, self.cstr(c.arg(1)).decode('latin1'))
        self.cpu._uc.mem_write(c.arg(0), out.encode('latin1') + b'\0')
        return len(out)

    def query(self, c):
        hits = stand_in_hits(c.arg(1), c.arg(2), c.arg(3))
        rec = c.arg(4)
        self.cpu._uc.mem_write(rec, bytes([len(hits)]))
        for i, (h, water) in enumerate(hits):
            at = rec + 4 + i * 0x2c
            self.cpu._uc.mem_write(at + 0x10, h)
            self.cpu._uc.mem_write(at + 0x20, struct.pack('<I', self.mats[water]))
            self.water_hits += water
        self.grounded += bool(hits)
        return 0

    def list_of(self, head):
        out, p = [], self.r32(head)
        while p != head:
            out.append(self.r32(p + 8))
            p = self.r32(p)
        return out

    def token(self, els, p):
        if p == 0:
            return -5
        if p == NULL_STEP:
            return -2
        if p == DEFAULT:
            return -3
        return els.index(p) if p in els else -4

    def vn(self, p):
        return self.names.get(p, '-') if p else '-'

    def step(self, els, p):
        u, vt = self.r32, self.r32(p)
        if p == NULL_STEP:
            return 'null'
        k = KINDS.get(vt, f'?{vt:x}')
        o = [k, str(self.b(p + 4) & 0xff), str(self.r32s(p + 8))]
        if k in ('node', 'backup', 'mcbwait', 'deploy', 'via', 'terminus'):
            o.append(self.cstr(u(p + 0xc)).decode('latin1') if u(p + 0xc) else '-')
            o += [f'{u(p + off):x}' for off in (0x10, 0x14, 0x18)]
            b, end = u(p + 0x20), u(p + 0x24)
            o.append('[' + ''.join(f'{self.token(els, u(q))},' for q in range(b, end, 4)) + ']' if b else '[]')
            if k == 'mcbwait':
                o += [self.vn(u(p + 0x2c)), str(u(p + 0x30) & 0xff), f'{u(p + 0x34):x}']
            elif k == 'deploy':
                o += [self.vn(u(p + 0x2c)), str(u(p + 0x30) & 0xff), self.vn(u(p + 0x34)), str(u(p + 0x38) & 0xff),
                      str(self.token(els, u(p + 0x3c)))]
            elif k == 'via':
                o += [f'{u(p + 0x2c):x}', str(self.token(els, u(p + 0x30))), self.cstr(u(p + 0x34)).decode('latin1')]
            return ' '.join(o)
        o.append(str(self.token(els, u(p + 0xc))))
        if k in ('speed', 'wait_interval', 'wait_time', 'jump'):
            o.append(f'{u(p + 0x10):x}')
        elif k == 'sound':
            o.append(self.names[u(p + 0x10)])
        elif k in ('valve', 'assign'):
            o += [self.vn(u(p + 0x10)), str(self.r32s(p + 0x14))]
        elif k in ('scan', 'canleave', 'canleave_not'):
            b, end = u(p + 0x14), u(p + 0x18)
            o.append(str(self.b(p + 0x10)))
            o += [f'{TESTS.get(u(q), hex(u(q)))}:{u(q + 4)}' for q in range(b, end, 8)] if b else []
        elif k == 'gate':
            o.append(self.cstr(u(p + 0x10)).decode('latin1') if u(p + 0x10) else '-')
        elif k == 'union':
            b, end = u(p + 0x14), u(p + 0x18)
            o += [f'{self.vn(u(q))}:{self.r32s(q + 4)}' for q in range(b, end, 8)] if b else []
        return ' '.join(o)

    def build(self):
        for n in range(1, 100):
            self.call(BUILD, args=[n, 0])
        o = []
        for x in self.list_of(self.r32(REGISTRY + 4)):
            vt = self.r32(x)
            if vt not in NETS:
                continue
            b, end = self.r32(x + 0xc), self.r32(x + 0x10)
            els = [self.r32(q) for q in range(b, end, 4)] if b else []
            o.append(f'net {self.r32s(x + 4)} {self.b(x + 0x18)} {len(els)} {NETS[vt]}')
            o += [self.step(els, p) for p in els]
        return o + [f'report {r}' for r in self.reports]


    def vias(self):
        """0x4c7d60 over the registry on an area of two polygons: each via node handed to 0x4cf350 (stubbed: what it
        gets, by net and slot, polygon by polygon; none taken, so each is also logged, 0x406cb0 stubbed)."""
        area, polys = self.alloc(0x20), self.alloc(8)
        self.cpu.u32(polys, 0x7100), self.cpu.u32(polys + 4, 0x7200)
        self.cpu.u32(area + 8, polys), self.cpu.u32(area + 0xc, polys + 8)
        got, cur = [], {}

        def net_in(uc, ad, sz, u):
            cur['net'] = self.r32(uc.reg_read(UC_X86_REG_ESP) + 4)
        h = self.cpu._uc.hook_add(UC_HOOK_CODE, net_in, begin=0x4c7c00, end=0x4c7c00)

        def handed(c):
            node = self.r32(self.r32(c.ecx + 4) + 4)
            poly = self.r32(c._uc.reg_read(UC_X86_REG_ESP) + 4)
            net = cur['net']
            b, end = self.r32(net + 0xc), self.r32(net + 0x10)
            els = [self.r32(q) for q in range(b, end, 4)]
            got.append(f'via {self.r32s(net + 4)} {els.index(node) if node in els else -1} {(poly >> 8) - 0x71}')
            return 0
        self.native_stub(0x4cf350, handed, pop=4)
        self.native_stub(0x406cb0, lambda c: 0)
        self.call(0x4c7d60, ecx=area)
        self.cpu._uc.hook_del(h)
        return got

    def waynets(self, names):
        self.reports = []
        o = []
        for i, name in enumerate(names):
            edit = self.alloc(0x44)
            self.call(EDIT, ecx=edit, args=[10000 + i, self.string(name), 1])
            w = self.r32(edit + 0x34)
            self.call(EDIT_END, ecx=edit)
            if not w or self.r32(w) != WAYNET_VT:
                o.append(f'{name} none')
                continue
            b, end = self.r32(w + 0xc), self.r32(w + 0x10)
            els = [self.r32(q) for q in range(b, end, 4)] if b else []
            lab = self.r32(w + 0x1c)
            o.append(f'{name} {self.r32s(w + 4)} {self.cstr(lab).decode("latin1") if lab else ""} {len(els)}')
            for q in els:
                if self.r32(q) != POINT_VT:
                    o.append('null' if q == NULL_STEP else 'other')
                    continue
                s = [self.r32(q + off) for off in (0x28, 0x2c)]
                t = [self.cstr(x).decode('latin1') if x else '-' for x in s]
                o.append('point ' + ' '.join(str(self.b(q + k)) for k in range(0xc, 0x12)) +
                         f' {self.names.get(self.r32(q + 0x14), "-")} {self.r32s(q + 0x30)} {self.b(q + 0x34)} '
                         + ' '.join(f'{self.r32(q + 0x1c + 4 * k):x}' for k in range(3)) + f' {t[0]}|{t[1]}')
        return o + [f'report {r}' for r in self.reports]


def net_file(nodes_, version=0x6a):
    top = ['version', [version], 'name', ['n']]
    for k, v in nodes_:
        top += [k, v]
    return nodes.encode([top])


PROPS = [['speed', [1, 0.75]], ['wait_interval', [1, 2.5]], ['wait_time', [1, 3.5]], ['valve', [1, 'v_a']],
         ['valve', [1, 'v_b', 7]], ['canleave', [1, 1]], ['canleave', [1, 0]], ['scan_aim', [1, 1]], ['scan_aim', [1, 0]],
         ['scan_hit', [1, 1]], ['scan_hit', [1, 0]], ['scan_time', [1, 1]], ['scan_time', [1, 0]], ['jump', [1, 22.5]],
         ['sound', ['s_one']], ['gate', ['g_one']], ['shunt', []], ['valveunion', [[1, 'v_a', 2], [0, 'v_c', 1], [1, 'v_b']]],
         ['valve_assign', ['v_a', 4]], ['auxname', ['aux1']], ['name', ['MSG_X']]]
SPECIAL = [['deploy', ['d_one', 3, 'd_two', 258, 2]], ['mcbwait', ['m_valve', 300, 4.25]], ['backup', []], ['terminus', []],
           ['via', ['the_via', 1, 7.0]], ['arrangenodes', [5.0]], ['static_mfb', [1]]]


def node(pos, links, props=()):
    return [12, list(pos), list(links)] + [x for p in props for x in p]


def made_up(rng):
    out = []
    square = [('node_00', node((0.0, 0.0, 0.0), [1, 3])), ('node_01', node((10.0, 1.0, 0.0), [2, 0])),
              ('node_02', node((10.0, 2.0, 10.0), [3, 1])), ('node_03', node((0.0, 3.0, 10.0), [0, 2]))]
    out.append(('square', {'net_01.zrd': net_file(square)}))
    for i, p in enumerate(PROPS + SPECIAL):
        ns = [('node_00', node((0.0, 0.0, 0.0), [1, 2], [p])), ('node_01', node((5.0, 0.0, 0.0), [2])),
              ('node_02', node((5.0, 0.0, 5.0), [0], [p]))]
        out.append((f'prop {p[0]} {i}', {'net_02.zrd': net_file(ns)}))
    for i, p in enumerate(PROPS[::3]):  # edge nodes: (from to ...) with properties for those links
        ns = square + [('node_04', [[0, 1, 3]] + p), ('node_05', [[2, 3]] + p), ('node_06', [[1]] + p)]
        out.append((f'edge {p[0]} {i}', {'net_03.zrd': net_file(ns)}))
    out.append(('via on an edge', {'net_03.zrd': net_file(square + [('node_04', [[0, 1]] + SPECIAL[4])])}))
    out.append(('several', {f'net_{k:02d}.zrd': net_file(square) for k in (1, 2, 7, 42, 99)}))
    out.append(('duplicate', {'net_04.zrd': net_file(square + [('node_02', node((9.0, 9.0, 9.0), [0]))])}))
    for i, a in enumerate(SPECIAL[:5]):  # which class wins with two on one node
        for j, b in enumerate(SPECIAL[:5]):
            if i != j:
                ns = [('node_00', node((0.0, 0.0, 0.0), [1, 2], [a, b])), ('node_01', node((5.0, 0.0, 0.0), [2])),
                      ('node_02', node((5.0, 0.0, 5.0), [0]))]
                out.append((f'both {a[0]} {b[0]}', {'net_02.zrd': net_file(ns)}))
    for i in range(60):
        n = rng.randrange(2, 9)
        ns = []
        for k in range(n):
            props = rng.sample(PROPS, rng.randrange(0, 5))
            if rng.random() < 0.3:
                props.append(rng.choice(SPECIAL[:5]))
            links = [rng.randrange(n) for _ in range(rng.randrange(1, 4))]
            pos = (rng.uniform(-500, 500), rng.uniform(-20, 20), rng.uniform(-500, 500))
            ns.append((f'node_{k:02d}', node(pos, links, props)))
        out.append((f'random {i}', {'net_08.zrd': net_file(ns)}))
    flags = [['hidden', [1]], ['fixed', [1]], ['autonext', [0]], ['visible', [0]], ['auxname', ['Able']], ['auxname', ['Baker']],
             ['name', ['MSG_WPT_ABLE']], ['speed', [1, 0.5]], ['gate', ['g1']], ['static_mfb', [1]]]
    for i in range(30):
        n = rng.randrange(1, 7)
        ns = []
        for k in range(n):
            links = [rng.randrange(n) for _ in range(rng.randrange(0, 4))]
            pos = (rng.uniform(-500, 500), rng.uniform(0, 30), rng.uniform(-500, 500))
            if rng.random() < 0.15:
                ns.append((f'node_{k:02d}', [links] + rng.choice(flags)))
            else:
                ns.append((f'node_{k:02d}', node(pos, links, rng.sample(flags, rng.randrange(0, 3)))))
        out.append((f'waynet {i}', {'waynet_1.zrd': net_file(ns, rng.choice([0x6b, 0x6b, 0x6b, 0x6a]))}))
    return out


def archive_of(files, like):
    version, _, last = struct.unpack_from('<III', like, len(like) - 12)
    names = dict(files)
    names.setdefault('other.zrd', next(iter(files.values())))  # with one entry zrd.py takes it for MW3
    data, entries = b'', b''
    for n, body in names.items():
        entries += struct.pack('<II', len(data), len(body)) + n.encode().ljust(0x8c, b'\0')
        data += body
    return data + entries + struct.pack('<III', version, len(names), last)


class Tally:
    def __init__(self, p):
        self.p, self.buf = p, ctypes.create_string_buffer(1 << 24)
        self.cases = self.bad = self.steps = self.grounded = self.water = 0
        self.seen = {}
        self.waynets = 0

    def run(self, label, root, rel, data, world=1):
        emu = NetBench()
        emu.setup(world)
        emu.files[test_pm_zar.key(rel)] = bytearray(data)
        if not emu.call(zr.OPEN_ARCHIVE, args=[emu.string(rel.replace('/', '\\')), 1]):
            raise SystemExit(f'{label}: PM does not open it')
        self.p.probe_pm_zrdr_load(str(root).encode(), rel.encode(), b'')
        theirs = emu.build()
        n = self.p.probe_pm_ai_nets(world, self.buf, len(self.buf))
        ours = self.buf.value.decode('latin1').splitlines() if n >= 0 else [f'probe failed {n}']
        theirs += emu.vias()
        n = self.p.probe_pm_ai_vias(self.buf, len(self.buf))
        ours += self.buf.value.decode('latin1').splitlines() if n >= 0 else [f'probe failed {n}']
        self.cases += 1
        names = [n[:-4] for n, _, _ in zrd.entries(data) if n.lower().startswith('waynet')]
        if names:
            more = emu.waynets(names)
            n = self.p.probe_pm_ai_waynets('\n'.join(names).encode(), world, self.buf, len(self.buf))
            ours += self.buf.value.decode('latin1').splitlines() if n >= 0 else [f'probe failed {n}']
            theirs += more
            self.waynets += len(names)
        self.steps += len(theirs)
        self.grounded += emu.grounded
        self.water += emu.water_hits
        for line in theirs:
            k = line.split()[0]
            self.seen[k] = self.seen.get(k, 0) + 1
        if theirs != ours:
            self.bad += 1
            if self.bad <= 6:
                print(f'{label}: {len(theirs)} and {len(ours)} lines')
                for a, b in zip(theirs, ours):
                    if a != b:
                        print(f'  exe  {a[:200]}\n  port {b[:200]}')
                        break


def main():
    limit = int(sys.argv[1]) if len(sys.argv) > 1 else 10 ** 6
    p = test_pm_zar.load_pm_probe()
    p.probe_pm_ai_nets.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_int]
    p.probe_pm_ai_vias.argtypes = [ctypes.c_char_p, ctypes.c_int]
    p.probe_pm_ai_waynets.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_int]
    p.probe_pm_zrdr_load.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
    t = Tally(p)
    like = None
    shipped = 0
    for path in sorted(PM.glob('zbd/*/reader*.zbd'))[:limit]:
        data = path.read_bytes()
        names = [n for n, _, _ in zrd.entries(data) if n.lower().startswith(('net_', 'waynet'))]
        like = like or data
        if names:
            shipped += len(names)
            t.run(path.relative_to(PM).as_posix(), PM, path.relative_to(PM).as_posix(), data)
    import tempfile
    with tempfile.TemporaryDirectory() as d:
        for label, files in made_up(random.Random(3025)):
            data = archive_of(files, like)
            (Path(d) / 'crafted.zbd').write_bytes(data)
            t.run(label, d, 'crafted.zbd', data, world=int(label != 'square'))
    print(f'{t.cases} archives ({shipped} shipped net and waynet files, {t.waynets} waynets), {t.steps} lines, {t.grounded} nodes grounded, '
          f'{t.water} water hits: {t.bad} differ; {dict(sorted(t.seen.items()))}')
    if not t.bad and limit > 100:
        pm_floors.floors(cases=(t.cases, 100), grounded=(t.grounded, 50), water=(t.water, 50), reports=(t.seen.get('report', 0), 1),
                         waynets=(t.waynets, 40), points=(t.seen.get('point', 0), 100), vias=(t.seen.get('via', 0), 4),
                         **{k: (t.seen.get(k, 0), 3) for k in sorted(set(KINDS.values()))})
    sys.exit(1 if t.bad else 0)


if __name__ == '__main__':
    main()
