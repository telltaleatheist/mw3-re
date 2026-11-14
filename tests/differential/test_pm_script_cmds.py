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
from original import ROOT, Original  # noqa: E402
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
        fl = (ctypes.c_uint32 * n)(*flags)
        probe.probe_lighting((ctypes.c_int32 * n)(*parent), (ctypes.c_int32 * n)(*has_model), fl, n, s32(on))
        if list(fl) != want:
            raise SystemExit(f'lighting case {done}: {list(fl)} != {want}')
        done += 1
    return done


def main():
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 400
    exe = Exe()
    rng = random.Random(17)
    w, c, s = world_cases(exe, rng, count), camera_cases(exe, rng, count), seq_cases(exe, rng, count)
    wn, li, lt = window_cases(exe, rng, count), list_cases(exe, rng, count), lighting_cases(exe, rng, count)
    print(f'ok: on PM\'s exe {w} world, {c} camera, {s} sequence, {wn} window and display, {li} list and {lt} '
          f'lighting commands as the port (data, globals, outputs, results, report lines)')
    print(dict(sorted(SEEN.items())))


if __name__ == '__main__':
    main()
