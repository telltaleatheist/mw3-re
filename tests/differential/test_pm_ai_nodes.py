"""PM's AI readers against the exe: valves.zrd 0x4f38e0 (shipped and crafted), aiv.zrd 0x4f0830,
targets.zrd 0x4f1c20, and the net node reader 0x4ec240 on every node_NN of every shipped net and region
file, in each mission's archive. New in PM: via ( name number radius ), terminus, static_mfb, jump
through the typed getters. The valves' and sounds' lookups stand in on both sides, compared by
name. The rest of the file's reading (the header, the lists, the map by name) isn't here.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_ai_nodes.py [archives]
"""
import ctypes
import struct
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools' / 'analysis'))
import pm_floors  # noqa: E402
import test_pm_mechdef as md  # noqa: E402
import test_pm_zar  # noqa: E402
import test_pm_zreader as zr  # noqa: E402
import zrd  # noqa: E402
from original import ROOT  # noqa: E402

PM = ROOT / 'game' / 'pm'
MECHS = {m: i for i, m in enumerate(md.MECHS)}
READ, CTOR, VALVE, SOUND, LOCALIZE = 0x4ec240, 0x4d20c0, 0x4e4100, 0x590a70, 0x596db0
TARGET, TARGET_CTOR = 0x4f1c20, 0x4f1bd0
AIV, AIV_CTOR, AIV_DEFAULTS = 0x4f0830, 0x4e9b60, 0x60bdd0
AIV_RESET = 0x4e9d60  # the defaults' 0x3b words copied again, the team, no orders or names
VALVE_READ = 0x4f38e0
HEADER, REPORT = 0x4eb960, 0x406cb0


class Bench(zr.Emulated):
    stub_regions = True

    def stand_ins(self):
        self.names = {}
        self.ids = {}
        self.native_stub(VALVE, self.named)
        self.native_stub(SOUND, self.named)
        self.stand(LOCALIZE, lambda c: c.arg(0))
        self.native_stub(0x486730, lambda c: MECHS.get(self.cstr(c.arg(0)).decode('latin1').lower(), 0xffffffff))  # the mech by name
        if self.stub_regions:
            self.native_stub(0x4d0fc0, lambda c: 0)  # a region by name: not compared
        self.stand(0x5cee12, lambda c: 0)  # atexit, for 0x4f38e0's static defaults
        self.cpu.stub_import(0x5bb654, self.strncmp, 0)  # strncmp, by MW3's slot (the map moves it)
        self.stand(0x4e5640, lambda c: self.string(f'net_{c.arg(0):02d}'))  # sprintf

    def strncmp(self, c):
        a, b = self.cstr(c.arg(0))[:c.arg(2)], self.cstr(c.arg(1))[:c.arg(2)]
        return (a > b) - (a < b)

    def stand(self, addr, fn, pop=0):
        if addr in self.cpu.stubs:
            self.cpu.stubs[addr] = (fn, pop)
        else:
            self.native_stub(addr, fn, pop)

    def named(self, c):
        s = self.cstr(c.arg(0)) if c.arg(0) else b''
        if s not in self.ids:
            a = self.alloc(4)
            self.ids[s] = a
            self.names[a] = s.decode('latin1')
        return self.ids[s]

    def r32s(self, a):
        return struct.unpack('<i', bytes(self.cpu._uc.mem_read(a, 4)))[0]

    def b(self, a):
        return bytes(self.cpu._uc.mem_read(a, 1))[0]

    def h(self, a):
        return f'{self.r32(a):x}'

    def nm(self, a):
        return self.names.get(self.r32(a), '-')

    def s(self, a):
        p = self.r32(a)
        return self.cstr(p).decode('latin1') if p else '-'

    def node(self, key, value):
        r = self.alloc(0x100)
        self.call(CTOR, ecx=r)
        self.call(READ, ecx=r, args=[value])
        i, b, h, nm = self.r32s, self.b, self.h, self.nm
        d = struct.unpack('<h', bytes(self.cpu._uc.mem_read(r + 0x74, 2)))[0]
        line = [key, i(r + 0x18), h(r + 0x1c), i(r + 0x20), h(r + 0x24), i(r + 0x28), h(r + 0x2c), i(r + 0x30), nm(r + 0x34),
                b(r + 0x38), i(r + 0x3c), i(r + 0x40), i(r + 0x44), i(r + 0x48), h(r + 0x4c), i(r + 0x50), i(r + 0x54),
                i(r + 0x58), i(r + 0x5c), i(r + 0x60), i(r + 0x64), b(r + 0x68), b(r + 0x69), b(r + 0x6a), b(r + 0x6b),
                b(r + 0x6c), b(r + 0x6d), b(r + 0x6e), b(r + 0x6f), b(r + 0x70), b(r + 0x71), b(r + 0x72), d, nm(r + 0x78),
                nm(r + 0x7c), b(r + 0x80), h(r + 0x84), b(r + 0x88), b(r + 0x89), h(r + 0x8c), nm(r + 0x90),
                self.names.get(self.r32(r + 0x94), '-'), b(r + 0x98), b(r + 0x99), h(r + 0x9c), i(r + 0xa0), b(r + 0xa4),
                b(r + 0xa5), b(r + 0xa6)]
        text = ' '.join(str(x) for x in line) + ' '
        text += f"{self.s(r + 0xa8)}|{self.s(r + 0xac)}|{self.s(r + 0xb8)}|{self.s(r + 0xb0)}|"
        u = self.r32(r + 0xbc)
        if u:
            for p in range(self.r32(u + 4), self.r32(u + 8), 8):
                text += f'{self.names.get(self.r32(p), "-")}:{self.r32s(p + 4)},'
        text += '|'
        a = self.r32(r + 0xc0)
        if a:
            text += f'{self.names.get(self.r32(a), "-")}:{self.r32s(a + 4)}'
        return text

    def target(self, key, value):
        r = self.alloc(0x60)
        self.call(TARGET_CTOR, ecx=r)
        self.call(TARGET, ecx=r, args=[value, self.string(key)])
        i, h = self.r32s, self.h
        line = [key, i(r), h(r + 8), h(r + 0x18), h(r + 0x1c), h(r + 0x20), h(r + 0x24), h(r + 0x28), h(r + 0x2c), h(r + 0x34), i(r + 0x38),
                f'{self.b(r + 0x3c):x}', h(r + 0x44)]
        text = ' '.join(str(x) for x in line) + ' ' + f"{self.s(r + 4)}|{self.s(r + 0xc)}|{self.s(r + 0x10)}|{self.s(r + 0x14)}|{self.s(r + 0x40)}|"
        v = self.r32(r + 0x30)
        if v:
            for p in range(self.r32(v + 4), self.r32(v + 8), 0xc):
                text += f'{self.names.get(self.r32(p), "-")}:{self.r32s(p + 4)}:{self.r32(p + 8)},'
        return text

    def aiv(self, root):
        lst = self.r32(root + 4)
        if self.r32(lst + 4) > 1 and self.r32(lst + 8) == 4:
            lst = self.r32(lst + 12)
        out = []
        n = self.r32(lst + 4)
        for i in range(1, n - 1, 2):
            p = lst + 8 * i
            if self.r32(p) != 3 or self.r32(p + 8) != 4:  # not a name and its record
                continue
            key = self.cstr(self.r32(p + 4)).decode('latin1')
            r = self.alloc(0x110)
            self.call(AIV_CTOR, ecx=r, args=[AIV_DEFAULTS])
            self.call(AIV, ecx=r, args=[p + 8, self.string(key)])
            text = key + ''.join(f' {self.r32(r + o):x}' for o in range(4, 0xcc, 4))
            text += ''.join(f' {self.r32(r + o):x}' for o in range(0xcc, 0xec, 4))
            text += f' {self.b(r + 0xec):x} {self.b(r + 0xed):x} {self.b(r + 0xee):x} '
            text += f'{self.pstring(r, 0xf4)}|{self.pstring(r, 0xf8)}|'
            v = self.r32(r + 0xf0)
            if v:
                head = self.r32(v + 4)
                node = self.r32(head)
                while node != head:
                    e = node + 8
                    text += f'{self.r32(e):x},{self.r32(e + 4):x},{self.r32(e + 8):x},{self.r32(e + 0xc):x},{self.r32(e + 0x10):x},'
                    text += f'{self.b(e + 0x18):x},{self.b(e + 0x19):x},{self.names.get(self.r32(e + 0x1c), "-")};'
                    node = self.r32(node)
            out.append(text)
        return out

    def pstring(self, r, off):
        p = self.r32(r + off)
        return self.cstr(p).decode('latin1') if p else '-'

    def valves(self, root):
        lst = self.r32(root + 4)
        if self.r32(lst + 4) > 1 and self.r32(lst + 8) == 4:
            lst = self.r32(lst + 12)
        r = self.alloc(0x160)  # one record read over again, as the file's is; its ctor opens the file
        head = self.alloc(0x10)
        self.cpu._uc.mem_write(head, struct.pack('<II', head, head))
        self.cpu._uc.mem_write(r + 0x140, struct.pack('<II', head, 0))
        i, h, b, s = self.r32s, self.h, self.b, self.s
        named = lambda o: f'{s(r + o)} {i(r + o + 4)} {b(r + o + 8):x}'
        flag = lambda o: f'{b(r + o):x} {i(r + o + 4)}'
        three = lambda o, v, w: f'{s(r + o)} {i(r + v)} {h(r + w)}'
        out = []
        n = self.r32(lst + 4)
        for k in range(1, n - 1, 2):
            p = lst + 8 * k
            if self.r32(p) != 3 or self.r32(p + 8) != 4:
                continue
            key = self.cstr(self.r32(p + 4)).decode('latin1')
            self.call(VALVE_READ, ecx=r, args=[p + 8])
            f = [key, flag(0), named(8), named(0x14), named(0x20), named(0x2c), named(0x38), named(0x44), named(0x50),
                 flag(0x5c), flag(0x64), flag(0x6c), f'{s(r + 0x74)} {i(r + 0x78)} 0', f'{s(r + 0x7c)} 0 0',
                 f'{s(r + 0x80)} 0 {h(r + 0x84)}', three(0x88, 0x8c, 0x90), three(0x94, 0x98, 0x9c),
                 three(0xa0, 0xa4, 0xb4), f'{h(r + 0xa8)} {h(r + 0xac)} {h(r + 0xb0)}']
            f += [three(o, o + 4, o + 8) for o in (0xb8, 0xc4, 0xd0, 0xdc, 0xe8, 0xf4, 0x100, 0x10c)]
            f += [named(0x118), named(0x124), named(0x130), f'{b(r + 0x148) & 0x1f:x}']
            names, q = '', self.r32(head)
            while q != head:
                names += s(q + 8) + ','
                q = self.r32(q)
            out.append('|'.join(f) + '|' + names)
        return out

    def targets(self, root):
        lst = self.r32(root + 4)
        if self.r32(lst + 4) > 1 and self.r32(lst + 8) == 4:
            lst = self.r32(lst + 12)
        out = []
        n = self.r32(lst + 4)
        for i in range(1, n - 1, 2):
            p = lst + 8 * i
            if self.r32(p) == 3:
                out.append(self.target(self.cstr(self.r32(p + 4)).decode('latin1'), p + 8))
        return out

    def header(self, root, base, net):
        lst = self.r32(root + 4)
        node = lst + 8 if self.r32(lst + 4) > 1 and self.r32(lst + 8) == 4 else root
        self.reports = []
        self.stand(REPORT, lambda c: self.reports.append(f'{c.arg(2):x},') or 0)
        r = self.alloc(0x20)
        self.cpu._uc.mem_write(r, struct.pack('<II', net, 0x6a))
        self.call(HEADER, ecx=r, args=[node, self.string(base)])
        i = self.r32s
        name = self.s(r + 0x18)
        self.stand(REPORT, lambda c: 0)
        return (f'{i(r + 4)} {i(r + 8)} {self.h(r + 0xc)} {self.h(r + 0x10)} {self.h(r + 0x14)} {i(r + 0x1c)} {name} '
                + ''.join(self.reports))

    def walk(self, root):
        kind, lst = self.r32(root), self.r32(root + 4)
        out = []
        if kind != 4 or self.r32(lst + 4) < 2 or self.r32(lst + 8) != 4:
            return out
        lst = self.r32(lst + 12)  # ( ( "version" ... "node_00" ... ) )
        n = self.r32(lst + 4)
        for i in range(1, n - 1):
            p = lst + 8 * i
            if self.r32(p) == 3:
                k = self.cstr(self.r32(p + 4)).decode('latin1')
                if k.startswith('node_'):
                    out.append(self.node(k, p + 8))
        return out


def encode(v):
    if isinstance(v, bool) or isinstance(v, int):
        return struct.pack('<ii', 1, v)
    if isinstance(v, float):
        return struct.pack('<if', 2, v)
    if isinstance(v, str):
        return struct.pack('<iI', 3, len(v)) + v.encode('latin1')
    return struct.pack('<iI', 4, len(v) + 1) + b''.join(encode(x) for x in v)


def crafted_valves():
    """The keys the shipped files barely use or never do, each alone and all at once, and an empty
    record after them: what one read leaves is what the next starts from."""
    tags = [('value', [7]), ('sound', ['snd', 2]), ('sound_once', ['snd1', 3]), ('animation', ['an', 4]),
            ('animation_once', ['an1', 5]), ('scan_aim', ['aim', 1, 6]), ('scan_hit', ['hit', 0, 7]),
            ('scan_time', ['tim', 2, 8]), ('canleave', ['lv', 1, 9]), ('shutdown', ['veh', 1, 10]),
            ('missionreset', [11]), ('missionabort', [12]), ('missioncomplete', [13]), ('destroy', ['ent', 14]),
            ('setteam', ['tm', 3, 15]), ('attack_strategy', ['at', 4, 16]), ('update', ['up']),
            ('delayupdate', ['dl', 2.5]), ('all_nonzero', []), ('all_zero', []), ('any_nonzero', []),
            ('any_zero', []), ('namelist', ['a', 'b', 'c']), ('teleport', ['tp', [1.5, 2.5, 3.5], 45.0, 17])]
    for i, k in enumerate(('retreat_dist', 'retreat_time', 'retreat_health')):
        tags.append((k, [k[:9], i % 2, 20 + i]))
    for i, k in enumerate(('leaveNetRange', 'attack_radius', 'approachRange', 'minRange', 'scan_delay_min',
                           'scan_delay_max', 'scan_duration_min', 'scan_duration_max')):
        tags.append((k, [k[:9], 3.0 + i, 30 + i]))
    records = []
    for i, (k, v) in enumerate(tags):
        records += [f'one_{i}', [k, v]]
    records += ['all', [x for t in tags for x in t], 'none', ['value', [1]]]
    # An integer where these keys hold a number: the leave range squares its bits as a float.
    records += ['ints', ['leaveNetRange', ['n', 3, 1], 'attack_radius', ['n', 4, 2]]]
    return encode(records)


def crafted_archive(body, like):
    version, count, last = struct.unpack_from('<III', like, len(like) - 12)
    entry = struct.pack('<II', 0, len(body)) + b'valves.zrd'.ljust(0x8c, b'\0')
    other = struct.pack('<II', 0, len(body)) + b'other.zrd'.ljust(0x8c, b'\0')  # with one entry zrd.py takes it for MW3's
    return body + entry + other + struct.pack('<III', version, 2, last)


NET_HEADERS = [  # net 3, file net_03: each of the header's complaints
    [], ['version', [0x60]], ['version', [0x70]], ['version', [0x69, 'mech3']], ['version', [0x69, 'other']],
    ['version', [0x69, 'mech3'], 'name', ['net_03']], ['version', [0x69], 'name', ['net_03']],
    ['version', [0x69], 'name', ['wrong']], ['version', [0x6a], 'name', ['mcbwaynet']], ['version', [0x6b]],
    ['version', [0x6c]], ['version', [0x6b], 'name', ['x'], 'netparams', ['pos', [1.5, 2.5, 3.5]]],
    ['version', [0x6a], 'netparams', []], ['version', [0x6a], 'node_00', [], 'node_01', []],
]


def check_headers(p, buf, like):
    bad = 0
    for k, v in enumerate(NET_HEADERS):
        body = encode([v])
        version, _, last = struct.unpack_from('<III', like, len(like) - 12)
        data = body + b''.join(struct.pack('<II', 0, len(body)) + n.ljust(0x8c, b'\0') for n in (b'net_03.zrd', b'other.zrd'))
        data += struct.pack('<III', version, 2, last)
        with tempfile.TemporaryDirectory() as d:
            rel = 'crafted.zbd'
            (Path(d) / rel).write_bytes(data)
            emu = Bench()
            emu.stand_ins()
            emu.files[test_pm_zar.key(rel)] = bytearray(data)
            emu.call(zr.OPEN_ARCHIVE, args=[emu.string(rel), 1])
            p.probe_pm_zrdr_load(d.encode(), rel.encode(), b'net_03.zrd')
            for base in ('net_03', 'net_04'):  # the file's own name, and one that isn't the net's
                theirs = emu.header(emu.call(zr.LOAD, args=[emu.string('net_03.zrd'), 0, 0]), base, 3)
                n = p.probe_pm_ai_header(b'net_03.zrd', base.encode(), 3, buf, len(buf))
                ours = buf.value.decode('latin1') if n >= 0 else 'probe failed'
                if theirs != ours:
                    bad += 1
                    print(f'crafted header {k} {base} {v}\n  exe  {theirs}\n  port {ours}')
    return len(NET_HEADERS), bad


def check_crafted(p, buf, like):
    data = crafted_archive(crafted_valves(), like)
    with tempfile.TemporaryDirectory() as d:
        rel = 'crafted.zbd'
        (Path(d) / rel).write_bytes(data)
        emu = Bench()
        emu.stand_ins()
        emu.files[test_pm_zar.key(rel)] = bytearray(data)
        if not emu.call(zr.OPEN_ARCHIVE, args=[emu.string(rel), 1]):
            raise SystemExit('PM does not open the crafted archive')
        theirs = emu.valves(emu.call(zr.LOAD, args=[emu.string('valves.zrd'), 0, 0]))
        if p.probe_pm_zrdr_load(d.encode(), rel.encode(), b'valves.zrd') != 1:
            raise SystemExit('the port does not open the crafted archive')
        n = p.probe_pm_ai_valves(b'valves.zrd', buf, len(buf))
        ours = buf.value.decode('latin1').splitlines() if n >= 0 else []
    if theirs != ours:
        print(f'crafted valves: {len(theirs)} and {len(ours)} records')
        for a, b in zip(theirs, ours):
            if a != b:
                print('  exe ', a[:900])
                print('  port', b[:900])
                break
    return len(theirs), theirs != ours


def check_reset(p):
    """A record's reset (0x4e9d60) against the port's, word for word over +0x04..+0xef, from every byte filled."""
    out = (ctypes.c_uint32 * 0x3b)()
    for fill in (0x00, 0xab, 0xff):
        emu = Bench()
        emu.stand_ins()
        r = emu.alloc(0x110)
        emu.call(AIV_CTOR, ecx=r, args=[AIV_DEFAULTS])
        emu.cpu._uc.mem_write(r + 4, bytes([fill]) * 0xec)
        emu.call(AIV_RESET, ecx=r)
        theirs = [emu.r32(r + 4 + 4 * i) for i in range(0x3b)]
        p.probe_pm_ai_reset(fill, out)
        ours = list(out)
        if theirs != ours:
            bad = [(4 + 4 * i, hex(a), hex(b)) for i, (a, b) in enumerate(zip(theirs, ours)) if a != b]
            raise SystemExit(f'reset from {fill:#x}: (offset, exe, port) {bad}')
    return len(theirs)


def main():
    limit = int(sys.argv[1]) if len(sys.argv) > 1 else 10 ** 6
    p = test_pm_zar.load_pm_probe()
    p.probe_pm_ai_nodes.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
    p.probe_pm_ai_header.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_int]
    p.probe_pm_ai_aiv.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
    p.probe_pm_ai_valves.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
    p.probe_pm_ai_targets.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
    p.probe_pm_zrdr_load.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
    p.probe_pm_ai_reset.argtypes = [ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint32)]
    print(f'reset: {check_reset(p)} words identical to 0x4e9d60\'s from three fills')
    buf = ctypes.create_string_buffer(1 << 22)
    files = nodes = bad = vias = tfiles = trecs = special = afiles = arecs = vfiles = vrecs = 0
    for path in sorted(PM.glob('zbd/*/reader*.zbd'))[:limit]:
        rel = path.relative_to(PM).as_posix()
        data = path.read_bytes()
        names = [n for n, _, _ in zrd.entries(data) if n.lower().startswith(('net_', 'reg_'))]
        has_targets = any(n.lower() == 'targets.zrd' for n, _, _ in zrd.entries(data))
        has_aiv = any(n.lower() == 'aiv.zrd' for n, _, _ in zrd.entries(data))
        has_valves = any(n.lower() == 'valves.zrd' for n, _, _ in zrd.entries(data))
        if not names and not has_targets and not has_aiv and not has_valves:
            continue
        emu = Bench()
        emu.stand_ins()
        emu.files[test_pm_zar.key(rel)] = bytearray(data)
        if not emu.call(zr.OPEN_ARCHIVE, args=[emu.string(rel.replace('/', '\\')), 1]):
            raise SystemExit(f'{rel}: PM does not open it')
        p.probe_pm_zrdr_load(str(PM).encode(), rel.encode(), (names[0] if names else 'targets.zrd').encode())
        if has_valves:
            root = emu.call(zr.LOAD, args=[emu.string('valves.zrd'), 0, 0])
            theirs = emu.valves(root) if root else []
            n = p.probe_pm_ai_valves(b'valves.zrd', buf, len(buf))
            ours = buf.value.decode('latin1').splitlines() if n >= 0 else []
            vfiles += 1
            vrecs += len(theirs)
            if theirs != ours:
                bad += 1
                if bad <= 5:
                    print(f'{rel} valves: {len(theirs)} and {len(ours)} records')
                    for a, b in zip(theirs, ours):
                        if a != b:
                            print('  exe ', a[:500])
                            print('  port', b[:500])
                            break
        if has_aiv:
            root = emu.call(zr.LOAD, args=[emu.string('aiv.zrd'), 0, 0])
            theirs = emu.aiv(root) if root else []
            n = p.probe_pm_ai_aiv(b'aiv.zrd', buf, len(buf))
            ours = buf.value.decode('latin1').splitlines() if n >= 0 else []
            afiles += 1
            arecs += len(theirs)
            if theirs != ours:
                bad += 1
                if bad <= 5:
                    print(f'{rel} aiv: {len(theirs)} and {len(ours)} records')
                    for a, b in zip(theirs, ours):
                        if a != b:
                            ta, tb = a.split(' '), b.split(' ')
                            print('  ', ta[0], [(hex(4 + 4 * (i - 1)), x, y) for i, (x, y) in enumerate(zip(ta, tb)) if x != y][:8])
                            break
        if has_targets:
            root = emu.call(zr.LOAD, args=[emu.string('targets.zrd'), 0, 0])
            theirs = emu.targets(root) if root else []
            n = p.probe_pm_ai_targets(b'targets.zrd', buf, len(buf))
            ours = buf.value.decode('latin1').splitlines() if n >= 0 else []
            tfiles += 1
            trecs += len(theirs)
            special += sum('|' in x and (x.split('|')[4] != '-' or x.split()[10] != '-1') for x in theirs)
            if theirs != ours:
                bad += 1
                if bad <= 5:
                    print(f'{rel} targets: {len(theirs)} and {len(ours)} records')
                    for a, b in zip(theirs, ours):
                        if a != b:
                            print('  exe ', a[:300])
                            print('  port', b[:300])
                            break
        for name in names:
            root = emu.call(zr.LOAD, args=[emu.string(name), 0, 0])
            theirs = emu.walk(root) if root else []
            if root:
                base = name[:-4] if name.lower().endswith('.zrd') else name
                head = emu.header(root, base, 0)
                n = p.probe_pm_ai_header(name.encode(), base.encode(), 0, buf, len(buf))
                if head != (buf.value.decode('latin1') if n >= 0 else 'probe failed'):
                    bad += 1
                    print(f'{rel} {name} header\n  exe  {head}\n  port {buf.value.decode("latin1")}')
            n = p.probe_pm_ai_nodes(name.encode(), buf, len(buf))
            ours = buf.value.decode('latin1').splitlines() if n >= 0 else []
            files += 1
            nodes += len(theirs)
            vias += sum(' ' in x and '|' in x and x.split('|')[3] != '-' for x in theirs)
            if theirs != ours:
                bad += 1
                if bad <= 5:
                    print(f'{rel} {name}: {len(theirs)} and {len(ours)} nodes')
                    for a, b in zip(theirs, ours):
                        if a != b:
                            print('  exe ', a[:400])
                            print('  port', b[:400])
                            break
    if limit >= 10 ** 6:
        pm_floors.floors(files=(files, 400), nodes=(nodes, 5000), vias=(vias, 500), target_files=(tfiles, 30), specials=(special, 10))
    made_headers, wrong = check_headers(p, buf, next(iter(sorted(PM.glob('zbd/*/reader*.zbd')))).read_bytes())
    bad += wrong
    crafted, wrong = check_crafted(p, buf, next(iter(sorted(PM.glob('zbd/*/reader*.zbd')))).read_bytes())
    bad += wrong
    print(f'{files} files, headers and {made_headers} crafted headers, {nodes} nodes ({vias} with via); {tfiles} targets.zrd, {trecs} records ({special} with PM\'s new keys); {vfiles} valves.zrd, {vrecs} records and {crafted} crafted; {afiles} aiv.zrd, {arecs} records: {bad} differ')
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    code = 0
    try:
        main()
    except SystemExit as ex:
        if isinstance(ex.code, str):
            print(ex.code)
        code = 0 if ex.code in (None, 0) else 1
    except BaseException:
        import traceback
        traceback.print_exc()
        code = 1
    sys.stdout.flush()
    import os
    os._exit(code)
