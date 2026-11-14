"""The script engine's zClass commands (test_script_cmds.py's part 1) against Pirate's Moon's exe: PM's twins, each
cdecl, run on the same random cases as MW3's, through the same probes. PM's report lines (Camera.c's 0x37f, 0x412,
0x471, 0x4ec for MW3's 0x562, 0x5f0, 0x64f, 0x6ca; cls_world.c's 0x3a5, 0xe51 for 0x2db, 0xdff) and its World layout
(MW3's with PM's second area, 0x10 bytes, at +0x4c; a cell 0x58 bytes) are PM's. WorldSetPartitionTolerance's twin
is 0x505bf0 (world_set_margins); its second area is that function's own test's.

    cmake --build build --target pmprobe && python3 tests/differential/test_pm_script_cmds.py
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

os.environ['MW3_EXE'] = 'pm'
os.environ['MW3_PM_MOVED'] = '1'  # this test's addresses are PM's own
sys.path.insert(0, str(Path(__file__).parent))
from original import FTOL, ROOT, Original  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

LIB = next((ROOT / 'build').glob('libpmprobe.*'))
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
probe.probe_pm_node_lighting.argtypes = [P, P, P, I32, I32]
probe.probe_pm_node_lighting.restype = I32
probe.probe_pm_model_cmd.argtypes = [I32, P, P, P, P, P, P, I32, P, I32, I32, F, P]
MALLOC, FREE = 0x5de664, 0x5de65c
probe.probe_mark_paint.argtypes = [I32, I32, I32, I32, P, I32, I32, I32, P, P, P, I32, I32, I32]
probe.probe_mark_paint.restype = I32
probe.probe_pm_anim_camera.argtypes = [I32, P, I32, I32, P, I32, F, P, P, I32]
probe.probe_pm_anim_camera.restype = I32
probe.probe_pm_wireframe.argtypes = [I32, I32]
probe.probe_pm_wireframe.restype = I32
MODEL = {0: 0x559910, 1: 0x55b810, 2: 0x55be00, 3: 0x55b630, 4: 0x559940}

# PM's own: the report (0x49bfd0's), zGeometry's buffer report (0x50da70's), the C runtime.
ZERR, GEO_REPORT, SPRINTF, CALLOC, REALLOC, PRINTF, STRNCMP = 0x406cb0, 0x527250, 0x5de658, 0x5de60c, 0x596520, \
    0x5de5f4, 0x5de654
NODE_ALLOC, PUSH_FRONT, NODE_LINK, NODE_DELETE, SOFT_CLEAR = 0x4fc550, 0x50b9e0, 0x4fd6f0, 0x4fc750, 0x585cc0
PENDING, LISTS = 0x639330, 0x60db00
WALK_HSE, WALK_DYN, WALK_TARGET = 0x60d978, 0x60d97c, 0x60d980
WORLD = {'map_size': 0x504f80, 'virtual': 0x504f50, 'margins': 0x505bf0, 'max_dec': 0x505c40}
CAMERA = [0x506670, 0x506970, 0x506e50, 0x506f80, 0x507150, 0x507370]
SEQ = {'create': 0x515730, 'add': 0x515790, 'set': [0x515910, 0x515960, 0x5159b0, 0x515a00]}
WINDOW = [0x50ded0, 0x50df50, 0x50dff0, 0x50e110]
DISPLAY, LIST_PRINT, WALK_PREFIX, LIGHTING = 0x50e1f0, 0x50b8b0, 0x50c660, 0x50db40
SEEN = {}


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


def bits(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def s32(v):
    return v - (1 << 32) if v >= 1 << 31 else v


class Exe:
    def __init__(self):
        self.cpu = cpu = Original(exe='pm')
        cpu.fpu_control(0x027f)
        self.reports, self.pushed, self.linked, self.printed = [], [], [], ''
        cpu.stub_function(ZERR, lambda c: self.reports.extend((c.arg(0), c.arg(2))))
        cpu.stub_function(GEO_REPORT, lambda c: self.reports.extend((c.arg(0), 0x23)))
        cpu.stub_import(SPRINTF, lambda c: 0)
        cpu.stub_import(CALLOC, lambda c: self.zeroed(c.arg(0) * c.arg(1)))
        cpu.stub_function(REALLOC, self.realloc)
        cpu.stub_function(NODE_ALLOC, lambda c: self.zeroed(0xe0))
        cpu.stub_function(PUSH_FRONT, lambda c: self.pushed.append((c.arg(0), c.arg(1))) and 0)
        cpu.stub_function(NODE_LINK, lambda c: self.linked.append((c.arg(0), c.arg(1))) and 0)
        cpu.stub_function(NODE_DELETE, lambda c: 0)
        cpu.stub_function(SOFT_CLEAR, lambda c: 0)
        cpu.stub_import(PRINTF, self.printf)
        cpu.import_code(0x5de644, FTOL)  # PM's _ftol import
        cpu.stub_import(MALLOC, lambda c: c.alloc(max(1, c.arg(0))))
        cpu.stub_import(FREE, lambda c: 0)
        cpu.stub_import(STRNCMP, self.strncmp)

    def zeroed(self, n):
        a = self.cpu.alloc(max(1, n))
        self.cpu.write(a, bytes(max(1, n)))
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
        self.printed += fmt % (s32(c.arg(1)), self.cstr(c.arg(2)).decode('latin1'))
        return 0

    def strncmp(self, c):
        a, b, n = self.cstr(c.arg(0)), self.cstr(c.arg(1)), c.arg(2)
        return 0 if a[:n] == b[:n] else (1 if a[:n] > b[:n] else 0xffffffff)

    def call(self, addr, *args):
        self.cpu.uc.reg_write(UC_X86_REG_EAX, 0)
        self.cpu.call(addr, stack_args=tuple(a & 0xffffffff for a in args))
        return s32(self.cpu.uc.reg_read(UC_X86_REG_EAX))

    def take(self):
        r, self.reports = self.reports, []
        return r


def to_pm_world(raw):
    """MW3's layout to PM's: PM's second area, 0x10 bytes, at +0x4c."""
    return bytes(raw[:0x4c]) + bytes(0x10) + bytes(raw[0x4c:])


def from_pm_world(b):
    return bytes(b[:0x4c]) + bytes(b[0x5c:0x5c + 0xbc - 0x4c])


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
        if which == 1:
            a = rng.choice([0, 1, 2])
        b = rng.randrange(-5, 10)
        fa, fb = f32(rng.uniform(-50, 50)), f32(rng.uniform(-50, 50))
        node, data = exe.zeroed(0xe0), exe.zeroed(0xcc)
        cpu.u32(node + 0x38, data)
        cpu.write(data, to_pm_world(raw))
        cpu.u32(data + 0xa0, 0)  # the grid's rows (MW3's +0x90)
        nodes = []
        if grid:
            table = cpu.alloc(4 * rows)
            k = 0
            for z in range(rows):
                row = exe.zeroed(0x58 * cols)
                cpu.u32(table + 4 * z, row)
                for x in range(cols):
                    p = row + 0x58 * x
                    n = counts[z * cols + x]
                    cpu.write(p + 0x3a, struct.pack('<h', 3 if n < 0 else n))
                    if n < 0:
                        continue
                    lst = exe.zeroed(0xc * max(n, 1))  # PM's 12-byte entries: the node, its y range
                    cpu.u32(p + 0x3c, lst)
                    for i in range(n):
                        nd = exe.zeroed(0xe0)
                        cpu.u32(nd + 0x24, flags[k])
                        cpu.u32(lst + 0xc * i, nd)
                        nodes.append(nd)
                        k += 1
            cpu.u32(data + 0xa0, table)
        cpu.u32(PENDING, 0)
        if which == 0:
            r = exe.call(WORLD['map_size'], node, a, b)
        elif which == 1:
            r = exe.call(WORLD['virtual'], node, a)
        elif which == 2:
            r = exe.call(WORLD['margins'], node, bits(fa), bits(fb))
        else:
            r = exe.call(WORLD['max_dec'], node, a)
        want_raw = bytearray(from_pm_world(cpu.read(data, 0xcc)))
        struct.pack_into('<I', want_raw, 0x90, 0)
        want = (r, [struct.unpack('<I', cpu.read(nd + 0x24, 4))[0] for nd in nodes], exe.take())
        got_raw = bytearray(raw)
        struct.pack_into('<I', got_raw, 0x90, 0)
        flag_buf = (ctypes.c_uint32 * max(1, len(flags)))(*flags)
        count_buf = (ctypes.c_int16 * len(counts))(*counts)
        rep = (ctypes.c_int32 * 32)()
        raw_buf = ctypes.create_string_buffer(bytes(got_raw), 0xbc)
        n = probe.probe_world_cmd(which, raw_buf, a, b, fa, fb, int(grid), count_buf, flag_buf, rep, 32)
        got_raw = bytearray(raw_buf.raw)
        struct.pack_into('<I', got_raw, 0x90, 0)
        got = (rep[0], list(flag_buf)[:len(flags)], [rep[1 + i] for i in range(n)])
        if got_raw != want_raw or got != want:
            diff = [hex(i) for i in range(0xbc) if got_raw[i] != want_raw[i]]
            raise SystemExit(f'world case {done} (command {which}, grid {grid}, a {a}): {diff} {got} {want}')
        SEEN[f'world {which}'] = SEEN.get(f'world {which}', 0) + 1
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
        node, data, outs = exe.zeroed(0xe0), cpu.alloc(0x1e8), exe.zeroed(12)
        cpu.u32(node + 0x34, cls), cpu.u32(node + 0x38, 0 if null_data else data)
        cpu.write(data, raw)
        cpu.u32(WALK_DYN, dyn), cpu.u32(WALK_TARGET, bits(target)), cpu.u32(WALK_HSE, hse)
        n = 0 if null_node else node
        if which == 0:
            exe.call(CAMERA[0], on, bits(f0))
            r = 0
        elif which == 1:
            r = exe.call(CAMERA[1], on)
        elif which == 2:
            r = exe.call(CAMERA[2], n, outs, outs + 4, outs + 8)
        elif which in (3, 4):
            r = exe.call(CAMERA[which], n, outs, outs + 4)
        else:
            r = exe.call(CAMERA[5], n, outs)
        want = [r & 0xffffffff] + list(struct.unpack('<3I', cpu.read(outs, 12))) + \
            [struct.unpack('<I', cpu.read(a, 4))[0] for a in (WALK_DYN, WALK_TARGET, WALK_HSE)] + exe.take()
        out = (ctypes.c_uint32 * 7)()
        rep = (ctypes.c_int32 * 16)()
        k = probe.probe_camera_cmd(which, bytes(raw), cls, int(null_node), int(null_data), on, f0, dyn, target, hse,
                                   out, rep, 16)
        got = list(out) + [rep[i] for i in range(k)]
        for i in (4, 6):
            got[i] &= 0xffffffff
            want[i] &= 0xffffffff
        if got != want:
            raise SystemExit(f'camera case {done} (command {which}, class {cls}, null {null_node}/{null_data}): '
                             f'{got} != {want}')
        SEEN[f'camera {which} result {r}'] = SEEN.get(f'camera {which} result {r}', 0) + 1
        done += 1
    return done


def seq_cases(exe, rng, count):
    cpu, done = exe.cpu, 0
    for _ in range(count):
        which = rng.randrange(6)
        kids, n = rng.randrange(1, 5), rng.randrange(0, 5)
        header = bytearray(rng.randbytes(0x20))
        struct.pack_into('<i', header, 0x1c, n)
        nodes = [rng.randrange(1, kids + 1) for _ in range(n)]
        times = [f32(rng.uniform(0, 5)) for _ in range(n)]
        null_node, null_data = rng.random() < 0.06, rng.random() < 0.06
        a = rng.randrange(1, kids + 1) if which == 1 and rng.random() < 0.9 else \
            (0 if which == 1 else rng.choice([0, 1, rng.getrandbits(31)]))
        at, t = rng.randrange(0, n + 1), f32(rng.uniform(0, 5))
        exe.pushed, exe.linked = [], []
        if which == 0:
            node = exe.call(SEQ['create'])
            d = struct.unpack('<I', cpu.read(node + 0x38, 4))[0]
            want = (cpu.read(d, 0x20), [struct.unpack('<i', cpu.read(node + 0x34, 4))[0],
                                         int(exe.pushed == [(11, node)]), 1], exe.take())
            hb, out, rep = ctypes.create_string_buffer(0x20), (ctypes.c_int32 * 3)(), (ctypes.c_int32 * 16)()
            k = probe.probe_seq_cmd(0, hb, None, None, 0, 0, 0, 0, 0, 0.0, out, rep, 16)
            got = (hb.raw, [out[0], out[1], 1], [rep[i] for i in range(k)])
            if got != want:
                raise SystemExit(f'seq create {done}: {got} != {want}')
            done += 1
            continue
        all_nodes = [exe.zeroed(0xe0) for _ in range(kids + 1)]
        data = exe.zeroed(0x20 + 8 * max(n, 1))
        cpu.write(data, header)
        for i in range(n):
            cpu.write(data + 0x20 + 8 * i, struct.pack('<If', all_nodes[nodes[i]], times[i]))
        cpu.u32(all_nodes[0] + 0x34, 7), cpu.u32(all_nodes[0] + 0x38, 0 if null_data else data)
        first = 0 if null_node else all_nodes[0]
        if which == 1:
            r = exe.call(SEQ['add'], first, all_nodes[a] if a else 0, at, bits(t))
        else:
            r = exe.call(SEQ['set'][which - 2], first, a)
        d = struct.unpack('<I', cpu.read(all_nodes[0] + 0x38, 4))[0] if not null_data else data
        hdr = cpu.read(d, 0x20)
        ents = []
        for i in range(struct.unpack_from('<i', hdr, 0x1c)[0]):
            nd, tm = struct.unpack('<If', cpu.read(d + 0x20 + 8 * i, 8))
            ents.append((all_nodes.index(nd) if nd in all_nodes else -1, tm))
        linked = int(exe.linked == [(all_nodes[0], all_nodes[a])]) if which == 1 and a else 0
        want = (hdr, ents, [r & 0xffffffff, linked], exe.take())
        hb = ctypes.create_string_buffer(bytes(header), 0x20)
        en, et = (ctypes.c_int32 * 8)(*nodes), (ctypes.c_float * 8)(*times)
        out, rep = (ctypes.c_int32 * 3)(), (ctypes.c_int32 * 16)()
        k = probe.probe_seq_cmd(which, hb, en, et, kids, int(null_node), int(null_data), a, at, t, out, rep, 16)
        after = struct.unpack_from('<i', hb.raw, 0x1c)[0]
        got = (hb.raw, [(en[i], f32(et[i])) for i in range(after)], [out[0] & 0xffffffff, out[1]],
               [rep[i] for i in range(k)])
        if got != want:
            raise SystemExit(f'seq case {done} (command {which}, null {null_node}/{null_data}): {got} != {want}')
        done += 1
    return done


def window_cases(exe, rng, count):
    cpu, done = exe.cpu, 0
    for _ in range(count):
        which = rng.randrange(4)
        raw = bytearray(rng.randbytes(0xf8))
        struct.pack_into('<I', raw, 0xe0, rng.choice([0, 1, 2, 3, 3, 4]) | rng.choice([0, 0x80000000]))
        for k in range(4):
            struct.pack_into('<I', raw, 0x40 + 0x34 * k, rng.choice([0, 1, 2, 3, 4]) | rng.choice([0, 0x80000000]))
        cls = 3 if rng.random() < 0.85 else rng.randrange(0, 12)
        null_node, null_data = rng.random() < 0.05, rng.random() < 0.05
        a = rng.choice([0, 1, 2, -1, rng.getrandbits(31)])
        xyz = [f32(rng.uniform(-500, 500)) for _ in range(3)]
        node, data, pt = exe.zeroed(0xe0), cpu.alloc(0xf8), cpu.alloc(12)
        cpu.u32(node + 0x34, cls), cpu.u32(node + 0x38, 0 if null_data else data)
        cpu.write(data, raw), cpu.write(pt, struct.pack('<3f', *xyz))
        n = 0 if null_node else node
        r = exe.call(WINDOW[which], *((n, a) if which < 2 else (n, pt) if which == 2 else (n,)))
        want = (cpu.read(data, 0xf8), r & 0xffffffff, exe.take())
        buf, rep = ctypes.create_string_buffer(bytes(raw), 0xf8), (ctypes.c_int32 * 16)()
        k = probe.probe_window_cmd(which, buf, cls, int(null_node), int(null_data), a, (ctypes.c_float * 3)(*xyz),
                                   rep, 16)
        got = (buf.raw, rep[0] & 0xffffffff, [rep[1 + i] for i in range(k)])
        if got != want:
            raise SystemExit(f'window case {done} (command {which}): {got[1:]} != {want[1:]}')
        done += 1
    exe.pushed = []
    node = exe.call(DISPLAY)
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
        which, lst = rng.randrange(2), rng.randrange(8, 16)
        n = rng.randrange(0, 7)
        names = [rng.choice(NAMES) for _ in range(n)]
        present = [int(rng.random() < 0.85) for _ in range(n)]
        calls = rng.randrange(1, 8)
        keys = [rng.choice(['tree', 'tr', 'rock', 't', 'x', '', 'house']) for _ in range(calls)]
        has_key = [1] + [int(rng.random() < 0.3) for _ in range(calls - 1)]
        nodes = []
        for nm in names:
            nd = exe.zeroed(0xe0)
            cpu.write(nd, nm.encode() + b'\0')
            nodes.append(nd)
        items = [cpu.alloc(0x10) for _ in range(n)]
        for i in range(n):
            cpu.write(items[i], struct.pack('<IIIi', nodes[i] if present[i] else 0, items[i - 1] if i else 0,
                                            items[i + 1] if i + 1 < n else 0, 0))
        lp = struct.unpack('<I', cpu.read(LISTS + 4 * lst, 4))[0]
        saved = cpu.read(lp, 12)
        cpu.write(lp, struct.pack('<III', items[0] if n else 0, items[-1] if n else 0, 0))
        exe.printed, found_w = '', []
        if which == 0:
            exe.call(LIST_PRINT, lst)
        else:
            cpu.write(0x6392d0, bytes(12))
            for k in range(calls):
                km = cpu.alloc(0x24)
                cpu.write(km, keys[k].encode() + b'\0')
                hit = exe.call(WALK_PREFIX, km if has_key[k] else 0, lst) & 0xffffffff
                found_w.append(nodes.index(hit) if hit in nodes else -1)
        cpu.write(lp, saved)
        nb = b''.join(nm.encode().ljust(0x24, b'\0') for nm in names) or b'\0'
        kb = b''.join(k.encode().ljust(0x24, b'\0') for k in keys)
        found, text = (ctypes.c_int32 * calls)(), ctypes.create_string_buffer(0x1000)
        probe.probe_list_cmd(which, lst, nb, (ctypes.c_int32 * max(1, n))(*present), n, kb,
                             (ctypes.c_int32 * calls)(*has_key), calls, found, text, 0x1000)
        got = text.value.decode() if which == 0 else list(found)
        want = exe.printed if which == 0 else found_w
        if got != want:
            raise SystemExit(f'list case {done} (command {which}): {got!r} != {want!r}')
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
        nodes = [exe.zeroed(0xe0) for _ in range(n)]
        models = [exe.zeroed(0x68) for _ in range(n)]
        kids = [[j for j in range(n) if parent[j] == i] for i in range(n)]
        for i in range(n):  # PM's node: the model +0x3c, the child count a short at +0x56, the children +0x5c
            cpu.u32(models[i] + 8, flags[i])
            cpu.u32(nodes[i] + 0x3c, models[i] if has_model[i] else 0)
            arr = cpu.alloc(4 * max(1, len(kids[i])))
            for k, j in enumerate(kids[i]):
                cpu.u32(arr + 4 * k, nodes[j])
            cpu.write(nodes[i] + 0x56, struct.pack('<h', len(kids[i])))
            cpu.u32(nodes[i] + 0x5c, arr)
        exe.call(LIGHTING, nodes[0], on)
        want = [struct.unpack('<I', cpu.read(m + 8, 4))[0] for m in models]
        for fn in (probe.probe_lighting, probe.probe_pm_node_lighting):  # the node's MW3 model, then its PM one
            fl = (ctypes.c_uint32 * n)(*flags)
            fn((ctypes.c_int32 * n)(*parent), (ctypes.c_int32 * n)(*has_model), fl, n, s32(on))
            if list(fl) != want:
                raise SystemExit(f'lighting case {done} ({fn.__name__}): {list(fl)} != {want}')
        done += 1
    return done


def model_cases(exe, rng, count):
    """PM's model commands (pm::zmod): 0x68-byte slots, 0x28-byte polygons (+0x14 the materials, +0x18 the UVs)."""
    cpu, done = exe.cpu, 0
    for _ in range(count):
        which = rng.randrange(5)
        npoly, verts, nmat = rng.randrange(0, 6), rng.randrange(0, 12), rng.randrange(1, 4)
        header = bytearray(rng.randbytes(0x64))
        struct.pack_into('<ii', header, 0x10, npoly, verts)
        poly_flags = [(rng.getrandbits(32) & ~0x3ff) | (rng.randrange(0, 5) if verts else 0) for _ in range(npoly)]
        indices = [rng.randrange(0, verts) for f in poly_flags for _ in range(f & 0x3ff)]
        poly_mat = [rng.randrange(nmat) for _ in range(npoly)]
        uvs = [int(rng.random() < 0.5) for _ in range(npoly)]
        mats = [bytearray(rng.randbytes(0x20)) for _ in range(nmat)]
        fixed = [rng.randrange(0, verts) for _ in range(rng.randrange(0, 4))] if verts else []
        a = len(fixed) if which == 3 else rng.choice([0, 1, 2, 3, 255, rng.getrandbits(31)])
        b = rng.choice([0, -1, 1, 2, 3, 6])
        v = f32(rng.uniform(-5, 5))
        mdl = exe.zeroed(0x68)
        cpu.write(mdl, header)
        polys = exe.zeroed(0x28 * max(1, npoly))
        mat_at = [exe.zeroed(0x2c) for _ in range(nmat)]
        for i, m in enumerate(mats):
            cpu.write(mat_at[i], bytes(m))
        k = 0
        for i in range(npoly):
            n = poly_flags[i] & 0x3ff
            ia = cpu.alloc(4 * max(1, n))
            cpu.write(ia, struct.pack(f'<{n}I', *indices[k:k + n]))
            k += n
            first = cpu.alloc(4)
            cpu.u32(first, mat_at[poly_mat[i]])
            cpu.u32(polys + 0x28 * i, poly_flags[i])
            cpu.u32(polys + 0x28 * i + 8, ia)
            cpu.u32(polys + 0x28 * i + 0x10, 1)
            cpu.u32(polys + 0x28 * i + 0x14, first)
            cpu.u32(polys + 0x28 * i + 0x18, cpu.alloc(4) if uvs[i] else 0)
        cpu.u32(mdl + 0x34, polys), cpu.u32(mdl + 0x44, 0)
        fa = cpu.alloc(4 * max(1, len(fixed)))
        cpu.write(fa, struct.pack(f'<{len(fixed)}i', *fixed))
        if which == 3:
            exe.call(MODEL[3], mdl, bits(v), fa, a, b)
        else:
            exe.call(MODEL[which], mdl, a)
        wh = bytearray(cpu.read(mdl, 0x64))
        for off in (0x34, 0x44):
            struct.pack_into('<I', wh, off, 0)
        morph = cpu.read(struct.unpack('<I', cpu.read(mdl + 0x44, 4))[0], 12 * verts) if which == 3 and verts else b''
        want = (bytes(wh), [struct.unpack('<I', cpu.read(polys + 0x28 * i, 4))[0] for i in range(npoly)],
                [cpu.read(m, 0x20) for m in mat_at], morph)
        hb = ctypes.create_string_buffer(bytes(header), 0x64)
        pf = (ctypes.c_uint32 * max(1, npoly))(*poly_flags)
        mb = ctypes.create_string_buffer(b''.join(bytes(m) for m in mats), 0x20 * nmat)
        mo = (ctypes.c_float * max(1, 3 * verts))()
        probe.probe_pm_model_cmd(which, hb, pf, (ctypes.c_uint32 * max(1, len(indices)))(*indices),
                                 (ctypes.c_int32 * max(1, npoly))(*poly_mat), (ctypes.c_int32 * max(1, npoly))(*uvs),
                                 mb, nmat, (ctypes.c_int32 * max(1, len(fixed)))(*fixed), a, b, v, mo)
        gh = bytearray(hb.raw)
        for off in (0x34, 0x44):
            struct.pack_into('<I', gh, off, 0)
        got = (bytes(gh), list(pf)[:npoly], [mb.raw[0x20 * i:0x20 * i + 0x20] for i in range(nmat)],
               bytes(mo)[:12 * verts] if which == 3 and verts else b'')
        strip = [lambda m: m[:0x10] + m[0x14:]]
        if got[:2] != want[:2] or got[3] != want[3] or \
                [strip[0](m) for m in got[2]] != [strip[0](m) for m in want[2]]:
            raise SystemExit(f'model case {done} (command {which}, polygons {npoly}, vertices {verts}): differs')
        SEEN[f'model {which}'] = SEEN.get(f'model {which}', 0) + 1
        done += 1
    # The texture world setters and AnimDebugFrame, as MW3's at PM's addresses.
    for addr, glob, args in ((0x540430, 0xa4e0a8, (bits(1.5), bits(-2.0))), (0x540410, 0xa4e0b0, (bits(0.25), bits(4.0)))):
        exe.call(addr, *args)
        if cpu.read(glob, 8) != struct.pack('<2I', *args):
            raise SystemExit(f'{addr:#x}: {cpu.read(glob, 8).hex()}')
        done += 1
    # VideoSetWireFrame 0x5981f0: the device (0xb09f08)'s SetRenderState through its vtable kept at 0xb09f0c (+0x58).
    sent = []
    set_state, vtable, device = cpu.code(b'\xc3'), exe.zeroed(0x60), exe.zeroed(8)
    cpu.stub_function(set_state, lambda c: sent.append((c.arg(1), c.arg(2))) and 0, pop_bytes=0xc)
    cpu.u32(vtable + 0x58, set_state), cpu.u32(device, vtable)
    cpu.u32(0xb09f08, device), cpu.u32(0xb09f0c, vtable)
    for on in (0, 1, 2, -1, 0x7fffffff):
        for before in (0, 1):
            sent.clear()
            exe.call(0x5981f0, on)
            if len(sent) != 1 or sent[0][0] != 8 or int(sent[0][1] == 2) != probe.probe_pm_wireframe(on, before):
                raise SystemExit(f'VideoSetWireFrame {on}: {sent}')
            done += 1
    cpu.u32(0x8c4b60, 41)
    exe.call(0x520960)
    if struct.unpack('<I', cpu.read(0x60f2c8, 4))[0] != 42:
        raise SystemExit('0x520960: not the next frame')
    return done + 1


def mark_cases(exe, rng, count):
    """PM's texture-mark painter 0x548220: MW3's 0x542060 with PM's globals (on 0xa4e0b8, type 0xa4e0bc, maps
    0xa4e0c0, uv 0xa4e0cc, format 0xa4e59c), its entry's surface at +8, and its lock 0x8c4b90 (surface, &pixels,
    &pitch), unlock 0x8c4b94 and reload 0x8c4b9c (the entry, a rectangle)."""
    cpu, done = exe.cpu, 0
    reloads, state = [], {}
    lock_at, unlock_at, reload_at = cpu.code(b'\xc3'), cpu.code(b'\xc3'), cpu.code(b'\xc3')

    def lock(c):
        cpu.u32(c.arg(1), state['pixels'])
        cpu.u32(c.arg(2), state['pitch'])
        return 1
    cpu.stub_function(lock_at, lock)
    cpu.stub_function(unlock_at, lambda c: 0)
    cpu.stub_function(reload_at, lambda c: reloads.append(c.arg(0)) and 0)
    cpu.u32(0x8c4b90, lock_at), cpu.u32(0x8c4b94, unlock_at), cpu.u32(0x8c4b9c, reload_at)
    for _ in range(count):
        tw, th, mw, mh = rng.randrange(1, 13), rng.randrange(1, 13), rng.randrange(1, 15), rng.randrange(1, 15)
        tpal, mpal = (0 if rng.random() < 0.9 else 4), (0 if rng.random() < 0.9 else 4)
        mflags = rng.choice([3, 3, 3, 3, 1, 2, 7, 0x13])
        on, has_alpha, fmt, surface = int(rng.random() < 0.92), rng.random() < 0.6, rng.choice([5, 6]), \
            int(rng.random() < 0.3)
        uv = [f32(rng.uniform(-2.6, 2.6)), f32(rng.uniform(-2.6, 2.6))]
        tpix = [rng.getrandbits(16) for _ in range(tw * th)]
        mpix = [rng.choice([0, rng.getrandbits(16)]) for _ in range(mw * mh)]
        malpha = bytes(rng.choice([0, 2, 5, 9, 100, 200, 0xfb, 0xfc, 0xff]) for _ in range(mw * mh))
        tex, mark = exe.zeroed(0x38), exe.zeroed(0x38)
        tp, mp, ma = cpu.alloc(2 * tw * th), cpu.alloc(2 * mw * mh), cpu.alloc(mw * mh)
        cpu.write(tp, struct.pack(f'<{tw * th}H', *tpix)), cpu.write(mp, struct.pack(f'<{mw * mh}H', *mpix))
        cpu.write(ma, malpha)
        for t, w, h, pal, px, al in ((tex, tw, th, tpal, tp, 0), (mark, mw, mh, mpal, mp, ma if has_alpha else 0)):
            cpu.write(t + 4, struct.pack('<hh', w, h)), cpu.write(t + 0xe, struct.pack('<h', pal))
            cpu.u32(t + 0x10, px), cpu.u32(t + 0x14, al)
        entry, mentry, mat, hit = exe.zeroed(0x2c), exe.zeroed(0x2c), exe.zeroed(0x2c), exe.zeroed(0x28)
        cpu.u32(entry, tex), cpu.u32(entry + 8, 0x5000 if surface else 0)
        cpu.u32(mentry, mark)
        cpu.write(mat + 1, bytes([mflags])), cpu.u32(mat + 0x10, entry), cpu.u32(hit + 0x20, mat)
        cpu.u32(0xa4e0b8, on), cpu.u32(0xa4e0bc, 1), cpu.u32(0xa4e0c4, mentry)
        cpu.write(0xa4e0cc, struct.pack('<2f', *uv))
        cpu.u32(0xa4e59c, fmt)
        state.update(pixels=tp, pitch=2 * tw)
        reloads.clear()
        exe.call(0x548220, hit)
        want = (cpu.read(tp, 2 * tw * th), cpu.read(0xa4e0cc, 8), len(reloads))
        tb, ub = (ctypes.c_uint16 * (tw * th))(*tpix), (ctypes.c_float * 2)(*uv)
        n = probe.probe_mark_paint(mflags, tw, th, tpal, tb, mw, mh, mpal, (ctypes.c_uint16 * (mw * mh))(*mpix),
                                   ctypes.create_string_buffer(malpha, mw * mh) if has_alpha else None, ub, on, fmt,
                                   surface)
        got = (bytes(tb), bytes(ub), n)
        if got != want:
            raise SystemExit(f'mark case {done} (target {tw}x{th}, mark {mw}x{mh}, uv {uv}, flags {mflags:#x}, on {on},'
                             f' format {fmt}, surface {surface}): {got[1:]} != {want[1:]}')
        SEEN['mark'] = SEEN.get('mark', 0) + 1
        done += 1
    return done


def anim_cases(exe, rng, count):
    """Anim events 20 (0x51c370) and 21 (0x51c4d0) on PM's exe: the def's node refs at +0xe8 (0x2c bytes each, the
    node at +0x28), PM's time left 0x8d3f68, PM's camera setters with PM's report lines."""
    cpu, done = exe.cpu, 0
    for _ in range(count):
        which = rng.choice([20, 21])
        raw = bytearray(rng.randbytes(0x1e8))
        for off in (0xb0, 0xb4, 0xd0, 0xd8, 0xdc, 0xe0, 0xe4):
            struct.pack_into('<f', raw, off, f32(rng.uniform(0.2, 3.0)))
        cls, null_node = (1 if rng.random() < 0.9 else 3), rng.random() < 0.08
        ev = bytearray(0x6c)
        struct.pack_into('<I', ev, 0xc, rng.getrandbits(7))
        struct.pack_into('<i', ev, 0x10, rng.choice([1, 1, 1, 1, 0, -1]))
        for off in range(0x14, 0x68, 4):
            struct.pack_into('<f', ev, off, f32(rng.uniform(0.3, 2.5)))
        end = f32(rng.uniform(0.5, 4))
        struct.pack_into('<f', ev, 0x68, end)
        state = rng.choice([0, 0, 1])
        time = f32(rng.choice([end * 0.5, end, end * 1.3, rng.uniform(0, 5)]))
        left = f32(rng.uniform(0.01, 0.2))
        node, data, table, seq, evp = exe.zeroed(0xe0), cpu.alloc(0x1e8), exe.zeroed(0x2c * 3), exe.zeroed(0x40), \
            cpu.alloc(0x6c)
        cpu.u32(node + 0x34, cls), cpu.u32(node + 0x38, data)
        cpu.write(data, raw)
        cpu.u32(table + 0x2c + 0x28, 0 if null_node else node)
        defp = exe.zeroed(0x140)
        cpu.u32(defp + 0xe8, table)
        cpu.write(seq + 0x20, bytes([state])), cpu.write(seq + 0x28, struct.pack('<f', time))
        cpu.write(evp, ev)
        cpu.write(0x8d3f68, struct.pack('<f', left))
        r = exe.call(0x51c370 if which == 20 else 0x51c4d0, defp, seq, evp)
        want = (r & 0xffffffff, cpu.read(data, 0x1e8), cpu.read(0x8d3f68, 4), exe.take())
        rb, lb = ctypes.create_string_buffer(bytes(raw), 0x1e8), (ctypes.c_float * 1)(left)
        rep = (ctypes.c_int32 * 512)()
        k = probe.probe_pm_anim_camera(which, rb, cls, int(null_node), ctypes.create_string_buffer(bytes(ev), 0x6c),
                                       state, time, lb, rep, 512)
        g, w = bytearray(rb.raw), bytearray(want[1])
        for off in (0xd4, 0x1d4, 0x1d8):  # the setters' derived words, within 2 ulps (test_script_cmds.py's)
            if abs(struct.unpack_from('<i', g, off)[0] - struct.unpack_from('<i', w, off)[0]) <= 2:
                g[off:off + 4] = w[off:off + 4]
        got = (k & 0xffffffff, bytes(g), bytes(lb), [rep[1 + i] for i in range(rep[0])])
        if got != want:
            diff = [hex(i) for i in range(0x1e8) if got[1][i] != want[1][i]]
            raise SystemExit(f'anim event {which} case {done} (flags {struct.unpack_from("<I", ev, 0xc)[0]:#x}, state '
                             f'{state}, class {cls}, null {null_node}): {k} vs {want[0]}, {diff}, reports {got[3]} '
                             f'{want[3]}')
        SEEN[f'anim {which} result {k}'] = SEEN.get(f'anim {which} result {k}', 0) + 1
        done += 1
    return done


def main():
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 400
    exe = Exe()
    rng = random.Random(17)
    w, c, s = world_cases(exe, rng, count), camera_cases(exe, rng, count), seq_cases(exe, rng, count)
    wn, li, lt = window_cases(exe, rng, count), list_cases(exe, rng, count), lighting_cases(exe, rng, count)
    mo = model_cases(exe, rng, count)
    mk = mark_cases(exe, rng, count)
    an = anim_cases(exe, rng, count)
    print(f'ok: on PM\'s exe {w} world, {c} camera, {s} sequence, {wn} window and display, {li} list and {lt} '
          f'lighting and {mo} model commands {mk} texture marks and {an} anim camera events as the port (data, globals, outputs, results, report lines)')
    print(dict(sorted(SEEN.items())))


if __name__ == '__main__':
    main()
