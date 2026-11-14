"""The script engine's commands the port leaves null (zinterp.h; ported for preservation, not wired in) against
the original x86, part 1: the world's partition settings (0x4f1dc0, 0x4f1d90 with 0x4f1cd0, 0x4f2860, 0x4f2880),
the camera's (0x4f32a0, 0x4f3550, 0x4f39d0, 0x4f3b00 and the getters 0x4f3d00, 0x4f3f70) and the sequences'
(0x4fdc70, 0x4fdcd0, 0x4fde50, 0x4fde90, 0x4fded0, 0x4fdf10). Random data, null nodes and data, wrong classes;
the data, the globals, the outputs, the results and the reports (level, line) compared.

    cmake --build build && python3 tests/differential/test_script_cmds.py
"""
import collections
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import ROOT, Original  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
P, I32, F = ctypes.c_void_p, ctypes.c_int32, ctypes.c_float
probe.probe_world_cmd.argtypes = [I32, P, I32, I32, F, F, I32, P, P, P, I32]
probe.probe_world_cmd.restype = I32
probe.probe_camera_cmd.argtypes = [I32, P, I32, I32, I32, I32, F, I32, F, I32, P, P, I32]
probe.probe_camera_cmd.restype = I32
probe.probe_seq_cmd.argtypes = [I32, P, P, P, I32, I32, I32, I32, I32, F, P, P, I32]
probe.probe_seq_cmd.restype = I32
probe.probe_window_cmd.argtypes = [I32, P, I32, I32, I32, I32, P, P, I32]
probe.probe_window_cmd.restype = I32
probe.probe_display_create.argtypes = [P, P]
probe.probe_list_cmd.argtypes = [I32, I32, P, P, I32, P, P, I32, P, P, I32]
probe.probe_list_cmd.restype = I32
probe.probe_lighting.argtypes = [P, P, P, I32, I32]
probe.probe_lighting.restype = I32

SEEN = collections.Counter()
ZERR, GEO_REPORT, SPRINTF = 0x49bfd0, 0x50da70, 0x5bb658
CALLOC, REALLOC, PRINTF, STRNCMP = 0x5bb60c, 0x5bb610, 0x5bb5f0, 0x5bb654
LISTS = 0x5ea6d0
PENDING = 0x60bea0
NODE_ALLOC, PUSH_FRONT, NODE_LINK = 0x4eacd0, 0x4f8080, 0x4eba90
WALK_HSE, WALK_DYN, WALK_TARGET = 0x5ea518, 0x5ea51c, 0x5ea520


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


def bits(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


class Exe:
    def __init__(self):
        self.cpu = cpu = Original()
        self.reports = []
        cpu.stub_function(ZERR, lambda c: self.reports.extend((c.arg(0), c.arg(2))))
        cpu.stub_function(GEO_REPORT, lambda c: self.reports.extend((c.ecx, 0x23)))
        cpu.stub_import(SPRINTF, lambda c: 0)
        cpu.stub_import(CALLOC, self.calloc)
        cpu.stub_import(REALLOC, self.realloc)
        cpu.stub_function(NODE_ALLOC, self.node_alloc)
        cpu.stub_function(PUSH_FRONT, lambda c: self.pushed.append((c.ecx, c.edx)) and 0)
        cpu.stub_function(NODE_LINK, lambda c: self.linked.append((c.ecx, c.edx)) and 0)
        cpu.stub_import(PRINTF, self.printf)
        cpu.stub_import(STRNCMP, self.strncmp)
        self.printed = ''
        self.pushed, self.linked = [], []
        self.no_node = False

    def calloc(self, c):
        n = c.arg(0) * c.arg(1)
        a = c.alloc(n)
        c.write(a, bytes(n))
        return a

    def realloc(self, c):
        old, n = c.arg(0), c.arg(1)
        a = c.alloc(n)
        c.write(a, c.read(old, n) if old else bytes(n))
        return a

    def cstr(self, a, limit=0x100):
        b = self.cpu.read(a, limit)
        return b[:b.index(0)] if 0 in b else b

    def printf(self, c):
        fmt = self.cstr(c.arg(0)).decode('latin1')
        assert fmt == 'Node %d desc: %s\n', fmt
        self.printed += fmt % (struct.unpack('<i', struct.pack('<I', c.arg(1)))[0],
                               self.cstr(c.arg(2)).decode('latin1'))
        return 0

    def strncmp(self, c):
        a, b, n = self.cstr(c.arg(0)), self.cstr(c.arg(1)), c.arg(2)
        a, b = a[:n], b[:n]
        return 0 if a == b else (1 if a > b else 0xffffffff)

    def node_alloc(self, c):
        if self.no_node:
            return 0
        a = c.alloc(0xd4)
        c.write(a, bytes(0xd4))
        return a

    def eax(self):
        return self.cpu.uc.reg_read(UC_X86_REG_EAX)

    def take(self):
        r, self.reports = self.reports, []
        return r


def reports_of(buf, n):
    return [buf[i] for i in range(n)]


def world_cases(exe, rng, count):
    cpu, done = exe.cpu, 0
    for _ in range(count):
        which = rng.randrange(4)
        raw = bytearray(rng.randbytes(0xbc))
        cols, rows = rng.randrange(1, 4), rng.randrange(1, 4)
        struct.pack_into('<ii', raw, 0x88, cols, rows)
        grid = which == 1 and rng.random() < 0.85
        counts = [rng.choice([-1, 0, 1, 2, 3]) for _ in range(cols * rows)]
        flags = [rng.getrandbits(32) for _ in range(sum(max(0, k) for k in counts))] if grid else []
        a = rng.choice([0, 1, rng.randrange(-5, 300), rng.randrange(250, 260)])
        b = rng.randrange(-5, 10)
        fa, fb = f32(rng.uniform(-50, 50)), f32(rng.uniform(-50, 50))
        if which == 1:
            a = rng.choice([0, 1, 2])
        # The exe's: the grid of 0x48-byte partitions, row pointers at +0x90.
        node, data = cpu.alloc(0xd4), cpu.alloc(0xbc)
        cpu.write(node, bytes(0xd4))
        cpu.u32(node + 0x38, data)
        cpu.write(data, raw)
        cpu.u32(data + 0x90, 0)
        nodes = []
        if grid:
            table = cpu.alloc(4 * rows)
            k = 0
            for z in range(rows):
                row = cpu.alloc(0x48 * cols)
                cpu.write(row, bytes(0x48 * cols))
                cpu.u32(table + 4 * z, row)
                for x in range(cols):
                    p = row + 0x48 * x
                    n = counts[z * cols + x]
                    cpu.write(p + 0x3a, struct.pack('<h', 3 if n < 0 else n))
                    if n < 0:
                        continue
                    lst = cpu.alloc(4 * max(n, 1))
                    cpu.u32(p + 0x3c, lst)
                    for i in range(n):
                        nd = cpu.alloc(0xd4)
                        cpu.write(nd, bytes(0xd4))
                        cpu.u32(nd + 0x24, flags[k])
                        cpu.u32(lst + 4 * i, nd)
                        nodes.append(nd)
                        k += 1
            cpu.u32(data + 0x90, table)
        cpu.u32(PENDING, 0)
        if which == 0:
            cpu.call(0x4f1dc0, ecx=node, edx=a, stack_args=(b,))
        elif which == 1:
            cpu.call(0x4f1d90, ecx=node, edx=a)
        elif which == 2:
            cpu.call(0x4f2860, ecx=node, stack_args=(bits(fa), bits(fb)))
        else:
            cpu.call(0x4f2880, ecx=node, edx=a)
        want_raw = bytearray(cpu.read(data, 0xbc))
        want = (exe.eax(), [struct.unpack('<I', cpu.read(nd + 0x24, 4))[0] for nd in nodes], exe.take())
        got_raw = bytearray(raw)
        struct.pack_into('<I', got_raw, 0x90, 0)
        flag_buf = (ctypes.c_uint32 * max(1, len(flags)))(*flags)
        count_buf = (ctypes.c_int16 * len(counts))(*counts)
        rep = (ctypes.c_int32 * 32)()
        raw_buf = ctypes.create_string_buffer(bytes(got_raw), 0xbc)
        n = probe.probe_world_cmd(which, raw_buf, a, b, fa, fb, int(grid), count_buf, flag_buf, rep, 32)
        got_raw = bytearray(raw_buf.raw)
        struct.pack_into('<I', want_raw, 0x90, 0)  # the grid's pointer, the port's own
        got = (rep[0] & 0xffffffff, list(flag_buf)[:len(flags)], reports_of(rep[1:], n))
        if got_raw != want_raw or got != want:
            diff = [hex(i) for i in range(0xbc) if got_raw[i] != want_raw[i]]
            raise SystemExit(f'world case {done} (command {which}, grid {grid}, a {a}): differs {diff} '
                             f'{got if got != want else ""} {want if got != want else ""}')
        SEEN[f'world {which}' + (' marked' if nodes else '') + (' reported' if want[2] else '')] += 1
        done += 1
    return done


def camera_cases(exe, rng, count):
    cpu, done = exe.cpu, 0
    for _ in range(count):
        which = rng.randrange(6)
        raw = bytearray(rng.randbytes(0x1e8))
        cls = 1 if rng.random() < 0.85 else rng.randrange(0, 12)
        null_node, null_data = rng.random() < 0.06, rng.random() < 0.06
        on = rng.choice([0, 1, rng.getrandbits(32) - (1 << 31)])
        f0 = 0.0 if rng.random() < 0.2 else f32(rng.uniform(-100, 100))
        dyn, target, hse = rng.getrandbits(31), f32(rng.uniform(0, 1)), rng.getrandbits(31)
        node, data, outs = cpu.alloc(0xd4), cpu.alloc(0x1e8), cpu.alloc(12)
        cpu.write(node, bytes(0xd4))
        cpu.u32(node + 0x34, cls)
        cpu.u32(node + 0x38, 0 if null_data else data)
        cpu.write(data, raw)
        cpu.write(outs, bytes(12))
        cpu.u32(WALK_DYN, dyn), cpu.u32(WALK_TARGET, bits(target)), cpu.u32(WALK_HSE, hse)
        ecx = 0 if null_node else node
        cpu.uc.reg_write(UC_X86_REG_EAX, 0)
        if which == 0:
            cpu.call(0x4f32a0, ecx=on, stack_args=(bits(f0),))
        elif which == 1:
            cpu.call(0x4f3550, ecx=on)
        elif which == 2:
            cpu.call(0x4f39d0, ecx=ecx, edx=outs, stack_args=(outs + 4, outs + 8))
        elif which == 3:
            cpu.call(0x4f3b00, ecx=ecx, edx=outs, stack_args=(outs + 4,))
        elif which == 4:
            cpu.call(0x4f3d00, ecx=ecx, edx=outs, stack_args=(outs + 4,))
        else:
            cpu.call(0x4f3f70, ecx=ecx, edx=outs)
        result = 0 if which == 0 else exe.eax()
        want = [result] + list(struct.unpack('<3I', cpu.read(outs, 12))) + \
            [struct.unpack('<I', cpu.read(a, 4))[0] for a in (WALK_DYN, WALK_TARGET, WALK_HSE)] + exe.take()
        out = (ctypes.c_uint32 * 7)()
        rep = (ctypes.c_int32 * 16)()
        n = probe.probe_camera_cmd(which, bytes(raw), cls, int(null_node), int(null_data), on, f0, dyn, target, hse,
                                   out, rep, 16)
        got = list(out) + reports_of(rep, n)
        got[0] &= 0xffffffff
        want[0] &= 0xffffffff
        for i in (4, 6):
            got[i] &= 0xffffffff
            want[i] &= 0xffffffff
        if got != want:
            raise SystemExit(f'camera case {done} (command {which}, class {cls}, null {null_node}/{null_data}): '
                             f'{got} != {want}')
        SEEN[f'camera {which} result {want[0]}'] += 1
        done += 1
    return done


def seq_cases(exe, rng, count):
    cpu, done = exe.cpu, 0
    for _ in range(count):
        which = rng.randrange(6)
        kids = rng.randrange(1, 5)
        n = rng.randrange(0, 5)
        header = bytearray(rng.randbytes(0x20))
        struct.pack_into('<i', header, 0x1c, n)
        nodes = [rng.randrange(1, kids + 1) for _ in range(n)]
        times = [f32(rng.uniform(0, 5)) for _ in range(n)]
        null_node, null_data = rng.random() < 0.06, rng.random() < 0.06
        a = rng.randrange(0, kids + 1) if which == 1 else rng.choice([0, 1, rng.getrandbits(31)])
        if which == 1 and rng.random() < 0.9:
            a = rng.randrange(1, kids + 1)
        at = rng.randrange(0, n + 1)
        t = f32(rng.uniform(0, 5))
        exe.pushed, exe.linked = [], []
        exe.no_node = False
        # The exe's: nodes 0.. (0 the sequence), its data the header then the entries.
        if which == 0:
            cpu.call(0x4fdc70)
            node = exe.eax()
            d = struct.unpack('<I', cpu.read(node + 0x38, 4))[0]
            want = (cpu.read(d, 0x20), [struct.unpack('<i', cpu.read(node + 0x34, 4))[0],
                                         int(exe.pushed == [(11, node)]), 1], exe.take())
            hb = ctypes.create_string_buffer(0x20)
            out = (ctypes.c_int32 * 3)()
            rep = (ctypes.c_int32 * 16)()
            k = probe.probe_seq_cmd(0, hb, None, None, 0, 0, 0, 0, 0, 0.0, out, rep, 16)
            # The exe's block holds room for one entry from the start; the port's entries come with the first add.
            got = (hb.raw, [out[0], out[1], 1], reports_of(rep, k))
            if got != want:
                raise SystemExit(f'seq create {done}: {got} != {want}')
            SEEN['seq 0 created'] += 1
            done += 1
            continue
        all_nodes = [cpu.alloc(0xd4) for _ in range(kids + 1)]
        for nd in all_nodes:
            cpu.write(nd, bytes(0xd4))
        data = cpu.alloc(0x20 + 8 * max(n, 1))
        cpu.write(data, header)
        for i in range(n):
            cpu.write(data + 0x20 + 8 * i, struct.pack('<If', all_nodes[nodes[i]], times[i]))
        cpu.u32(all_nodes[0] + 0x34, 7)
        cpu.u32(all_nodes[0] + 0x38, 0 if null_data else data)
        ecx = 0 if null_node else all_nodes[0]
        if which == 1:
            cpu.call(0x4fdcd0, ecx=ecx, edx=all_nodes[a] if a else 0, stack_args=(at, bits(t)))
        else:
            cpu.call((0x4fde50, 0x4fde90, 0x4fded0, 0x4fdf10)[which - 2], ecx=ecx, edx=a)
        d = struct.unpack('<I', cpu.read(all_nodes[0] + 0x38, 4))[0] if not null_data else data
        hdr = cpu.read(d, 0x20)
        after = struct.unpack_from('<i', hdr, 0x1c)[0]
        ents = []
        for i in range(after):
            nd, tm = struct.unpack('<If', cpu.read(d + 0x20 + 8 * i, 8))
            ents.append((all_nodes.index(nd) if nd in all_nodes else -1, tm))
        linked = int(exe.linked == [(all_nodes[0], all_nodes[a])]) if which == 1 and a else 0
        want = (hdr, ents, [exe.eax() & 0xffffffff, linked], exe.take())
        hb = ctypes.create_string_buffer(bytes(header), 0x20)
        en = (ctypes.c_int32 * 8)(*nodes)
        et = (ctypes.c_float * 8)(*times)
        out = (ctypes.c_int32 * 3)()
        rep = (ctypes.c_int32 * 16)()
        k = probe.probe_seq_cmd(which, hb, en, et, kids, int(null_node), int(null_data), a, at, t, out, rep, 16)
        g_after = struct.unpack_from('<i', hb.raw, 0x1c)[0]
        got = (hb.raw, [(en[i], f32(et[i])) for i in range(g_after)], [out[0] & 0xffffffff, out[1]],
               reports_of(rep, k))
        if got != want:
            raise SystemExit(f'seq case {done} (command {which}, null {null_node}/{null_data}, a {a}, at {at}): '
                             f'{got} != {want}')
        SEEN[f'seq {which} result {want[2][0]}' + (' inserted' if which == 1 and at < n and want[2][1] else '')] += 1
        done += 1
    return done


def window_cases(exe, rng, count):
    cpu, done = exe.cpu, 0
    # The close hands the polygon to the software renderer's list (0x564660), not ported: stubbed.
    cpu.stub_function(0x564660, lambda c: 0)
    for _ in range(count):
        which = rng.randrange(4)
        raw = bytearray(rng.randbytes(0xf8))
        # The polygon being added and its vertex count, mostly in range; the bit 31s random.
        struct.pack_into('<I', raw, 0xe0, rng.choice([0, 1, 2, 3, 3, 4]) | rng.choice([0, 0x80000000]))
        for k in range(4):
            struct.pack_into('<I', raw, 0x40 + 0x34 * k, rng.choice([0, 1, 2, 3, 4]) | rng.choice([0, 0x80000000]))
        cls = 3 if rng.random() < 0.85 else rng.randrange(0, 12)
        null_node, null_data = rng.random() < 0.05, rng.random() < 0.05
        a = rng.choice([0, 1, 2, -1, rng.getrandbits(31)])
        xyz = [f32(rng.uniform(-500, 500)) for _ in range(3)]
        node, data, pt = cpu.alloc(0xd4), cpu.alloc(0xf8), cpu.alloc(12)
        cpu.write(node, bytes(0xd4))
        cpu.u32(node + 0x34, cls)
        cpu.u32(node + 0x38, 0 if null_data else data)
        cpu.write(data, raw)
        cpu.write(pt, struct.pack('<3f', *xyz))
        ecx = 0 if null_node else node
        if which == 0:
            cpu.call(0x4fa1c0, ecx=ecx, edx=a)
        elif which == 1:
            cpu.call(0x4fa230, ecx=ecx, edx=a)
        elif which == 2:
            cpu.call(0x4fa2c0, ecx=ecx, edx=pt)
        else:
            cpu.call(0x4fa3e0, ecx=ecx)
        want = (cpu.read(data, 0xf8), exe.eax() & 0xffffffff, exe.take())
        buf = ctypes.create_string_buffer(bytes(raw), 0xf8)
        rep = (ctypes.c_int32 * 16)()
        n = probe.probe_window_cmd(which, buf, cls, int(null_node), int(null_data), a, (ctypes.c_float * 3)(*xyz),
                                   rep, 16)
        got = (buf.raw, rep[0] & 0xffffffff, reports_of(rep[1:], n))
        if got != want:
            diff = [hex(i) for i in range(0xf8) if got[0][i] != want[0][i]]
            raise SystemExit(f'window case {done} (command {which}, class {cls}, null {null_node}/{null_data}): '
                             f'{diff} {got[1:]} != {want[1:]}')
        SEEN[f'window {which} result {want[1]}'] += 1
        done += 1
    exe.pushed = []
    cpu.call(0x4fa4c0)
    node = exe.eax()
    d = struct.unpack('<I', cpu.read(node + 0x38, 4))[0]
    want = (cpu.read(d, 0x1c), [struct.unpack('<i', cpu.read(node + 0x34, 4))[0], int(exe.pushed == [(15, node)])])
    raw, out = ctypes.create_string_buffer(0x1c), (ctypes.c_int32 * 2)()
    probe.probe_display_create(raw, out)
    if (raw.raw, list(out)) != want:
        raise SystemExit(f'display create: {(raw.raw, list(out))} != {want}')
    return done + 1


NAMES = ['tree', 'tree01', 'treeline', 'rock', 'rock2', 'trea', 'tr', 'house', '']


def list_cases(exe, rng, count):
    cpu, done = exe.cpu, 0
    for _ in range(count):
        which = rng.randrange(2)
        lst = rng.randrange(8, 16)  # node_alloc puts every node on list 6 in the port
        n = rng.randrange(0, 7)
        names = [rng.choice(NAMES) for _ in range(n)]
        present = [int(rng.random() < 0.85) for _ in range(n)]
        calls = rng.randrange(1, 8)
        keys = [rng.choice(['tree', 'tr', 'rock', 't', 'x', '', 'house']) for _ in range(calls)]
        has_key = [1] + [int(rng.random() < 0.3) for _ in range(calls - 1)]
        # The exe's list: items {node, prev, next, removable} from the list's head.
        nodes = []
        for nm in names:
            nd = cpu.alloc(0xd4)
            cpu.write(nd, bytes(0xd4))
            cpu.write(nd, nm.encode() + b'\0')
            nodes.append(nd)
        items = [cpu.alloc(0x10) for _ in range(n)]
        for i in range(n):
            cpu.write(items[i], struct.pack('<IIIi', nodes[i] if present[i] else 0, items[i - 1] if i else 0,
                                            items[i + 1] if i + 1 < n else 0, 0))
        lp = struct.unpack('<I', cpu.read(LISTS + 4 * lst, 4))[0]
        saved = cpu.read(lp, 12)
        cpu.write(lp, struct.pack('<III', items[0] if n else 0, items[-1] if n else 0, 0))
        exe.printed = ''
        found_w = []
        if which == 0:
            cpu.call(0x4f7fa0, ecx=lst)
        else:
            cpu.u32(0x60be40, 0), cpu.u32(0x60be44, 0), cpu.u32(0x60be48, 0)
            key_mem = [cpu.alloc(0x24) for _ in range(calls)]
            for k in range(calls):
                cpu.write(key_mem[k], keys[k].encode() + b'\0')
                cpu.call(0x4f8b80, ecx=key_mem[k] if has_key[k] else 0, edx=lst)
                hit = exe.eax()
                found_w.append(nodes.index(hit) if hit in nodes else -1)
        cpu.write(lp, saved)
        nb = b''.join(nm.encode().ljust(0x24, b'\0') for nm in names) or b'\0'
        kb = b''.join(k.encode().ljust(0x24, b'\0') for k in keys)
        found = (ctypes.c_int32 * calls)()
        text = ctypes.create_string_buffer(0x1000)
        probe.probe_list_cmd(which, lst, nb, (ctypes.c_int32 * max(1, n))(*present), n, kb,
                             (ctypes.c_int32 * calls)(*has_key), calls, found, text, 0x1000)
        got = text.value.decode() if which == 0 else list(found)
        want = exe.printed if which == 0 else found_w
        if got != want:
            raise SystemExit(f'list case {done} (command {which}, names {names}, present {present}, keys {keys} '
                             f'{has_key}): {got!r} != {want!r}')
        SEEN[f'list {which}' + (' found' if which and any(f >= 0 for f in want) else '')] += 1
        done += 1
    return done


def lighting_cases(exe, rng, count):
    cpu, done = exe.cpu, 0
    for _ in range(count):
        n = rng.randrange(1, 9)
        parent = [-1] + [rng.randrange(0, i) for i in range(1, n)]
        has_model = [int(rng.random() < 0.7) for _ in range(n)]
        flags = [rng.getrandbits(32) for _ in range(n)]
        on = rng.choice([0, 1, 2, 3, rng.getrandbits(32)])
        nodes = [cpu.alloc(0xd4) for _ in range(n)]
        models = [cpu.alloc(0x60) for _ in range(n)]
        kids = [[j for j in range(n) if parent[j] == i] for i in range(n)]
        for i in range(n):
            cpu.write(nodes[i], bytes(0xd4))
            cpu.write(models[i], bytes(0x60))
            cpu.u32(models[i] + 8, flags[i])
            cpu.u32(nodes[i] + 0x3c, models[i] if has_model[i] else 0)
            arr = cpu.alloc(4 * max(1, len(kids[i])))
            for k, j in enumerate(kids[i]):
                cpu.u32(arr + 4 * k, nodes[j])
            cpu.u32(nodes[i] + 0x5c, len(kids[i]))
            cpu.u32(nodes[i] + 0x60, arr)
        cpu.call(0x4f9e50, ecx=nodes[0], edx=on)
        want = [struct.unpack('<I', cpu.read(m + 8, 4))[0] for m in models]
        fl = (ctypes.c_uint32 * n)(*flags)
        probe.probe_lighting((ctypes.c_int32 * n)(*parent), (ctypes.c_int32 * n)(*has_model), fl, n,
                             on - (1 << 32) if on >= 1 << 31 else on)
        if list(fl) != want:
            raise SystemExit(f'lighting case {done}: {list(fl)} != {want}')
        done += 1
    return done


def main():
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 400
    exe = Exe()
    rng = random.Random(17)
    w = world_cases(exe, rng, count)
    c = camera_cases(exe, rng, count)
    s = seq_cases(exe, rng, count)
    wn = window_cases(exe, rng, count)
    li = list_cases(exe, rng, count)
    lt = lighting_cases(exe, rng, count)
    print(f'ok: {w} world, {c} camera, {s} sequence, {wn} window and display, {li} list and {lt} lighting commands '
          f'as the original (data, globals, outputs, results, reports)')
    print(dict(sorted(SEEN.items())))


if __name__ == '__main__':
    main()
