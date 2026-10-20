"""The mech's animation objects and the zSIScript samplers against the
original x86 (docs/motion.md, Playback; docs/movement.md, The animation
objects).

    cmake --build build && .venv/bin/python tests/differential/test_mechanim.py

Every shipped mech's gait motions (stand, step, walk, trot, run, limp and
its mirror, limpb) and leg one-shots (fall, land, get up, collapse) are
loaded on both sides from motion.zbd: the original's script reader runs in
the emulator and its factories 0x41f000 / 0x420d00 do the rest (parts by
name, stride, hip velocity keys, the start at the left foot forward, the
resample to 60 frames; 0x41f680 the mirror), the port through load_anim /
load_action. The whole state, every key array included, must match. Then
the blends (0x41ff30) of neighbouring gaits, copies (0x41f180), and random
calls of every sampler and setter (vtables 0x5bd1f0, 0x5bd240, 0x5bd290;
slerp 0x525ce0, the phase 0x5abae0 / 0x432a80, translation, rotation and
velocity samples with and without the ski frames, the footfalls 0x41fd30),
each output and the state after it identical bit for bit at 24-bit x87.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

os.environ.setdefault('MW3_X87', '24')
sys.path.insert(0, str(Path(__file__).parent))
from original import FTOL, IAT_FTOL, ROOT, Original  # noqa: E402
from test_motion import MOTION, entries  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
probe.probe_ma_open.argtypes = [ctypes.c_char_p]
probe.probe_ma_load.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_float, ctypes.c_uint32, ctypes.c_int]
probe.probe_ma_blend.argtypes = [ctypes.c_int, ctypes.c_int]
probe.probe_ma_copy.argtypes = [ctypes.c_int]
probe.probe_ma_state.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_void_p, ctypes.c_int]
probe.probe_ma_op.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p]
probe.probe_ma_set_rotations.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p]

FLOOR = bytes.fromhex('83ec04 d93c24 668b0424 6625fff3 660d0004 6689442402 d96c2402 dd442408 d9fc d92c24 83c404 c3')
CEIL = bytes.fromhex('83ec04 d93c24 668b0424 6625fff3 660d0008 6689442402 d96c2402 dd442408 d9fc d92c24 83c404 c3')
IAT_FLOOR, IAT_CEIL = 0x5bb66c, 0x5bb61c
# _CIasin: st0 = atan2(x, sqrt(1 - x*x)); _copysign(double x, double y) -> st0 (test_matrix.py's).
IAT_ASIN, IAT_COPYSIGN = 0x5bb5f4, 0x5bb5ac
ASIN = bytes.fromhex('d9c0 d8c8 d9e8 dee1 d9fa d9f3 c3')
COPYSIGN = bytes.fromhex('8b442410 2500000080 8b4c2408 81e1ffffff7f 0bc8 894c2408 dd442404 c3')
IAT_READFILE, IAT_MALLOC, IAT_CALLOC, IAT_NEW, IAT_FREE = 0x5bb0c4, 0x5bb664, 0x5bb60c, 0x5bb4bc, 0x5bb65c
IAT_STRICMP, IAT_STRNICMP, IAT_SPRINTF = 0x5bb580, 0x5bb604, 0x5bb658
DELETE, REPORT, BASE_CTOR = 0x5abd1a, 0x49bfd0, 0x41ed70
SCRIPT_CTOR, SCRIPT_READ, SCRIPT_VT, SET_FPS = 0x5a9920, 0x5a9c70, 0x5d0f20, 0x5aa450
LOAD_ANIM, LOAD_ACTION, MAKE_BLEND, COPY = 0x41f000, 0x420d00, 0x41ff30, 0x41f180
COPY_PHASE, STRIDE_SPEED, FOOTFALLS, STOP, RESET, STARTED = 0x41ef00, 0x41ef90, 0x41fd30, 0x421060, 0x420f40, 0x421000
HANDLE, BIG = 0x55, 0x40000000
VT = {0: 0x08, 1: 0x0c, 2: 0x10, 3: 0x14, 4: 0x18, 5: 0x1c, 6: 0x20, 7: 0x24, 8: 0x28, 9: 0x2c, 10: 0x30,
      11: 0x34, 12: 0x38, 13: 0x3c, 14: 0x44}
NAMES = ['zero', 'speed', 'time', 'advance', 'blend', 'hip frame', 'ski frame', 'velocity', 'foot', 'arm', 'hand',
         'hip', 'torso', 'torso rot', 'toe', 'seek', 'copy phase', 'stride speed', 'footfalls', 'stop', 'reset',
         'started']
GAITS = ['stand', 'step', 'walk', 'trot', 'run', 'limp', 'limpb']
SHOTS = ['fallf', 'fallb', 'landf', 'landb', 'getupf', 'getupb', 'collapser']


def f2u(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


class Emulated:
    def __init__(self, data):
        cpu = self.cpu = Original()
        cpu.uc.mem_map(BIG, 0x8000000)
        self.next = BIG
        self.data = data
        self.pending = 0
        self.reports = 0
        cpu.import_code(IAT_FTOL, FTOL)
        cpu.import_code(IAT_FLOOR, FLOOR)
        cpu.import_code(IAT_CEIL, CEIL)
        cpu.import_code(IAT_ASIN, ASIN)
        cpu.import_code(IAT_COPYSIGN, COPYSIGN)
        cpu.stub_import(IAT_READFILE, self.read_file, pop_bytes=20)
        cpu.stub_import(IAT_MALLOC, lambda c: self.alloc(c.arg(0)))
        cpu.stub_import(IAT_NEW, self.new)
        cpu.stub_import(IAT_CALLOC, self.calloc)
        cpu.stub_import(IAT_FREE, lambda c: 0)
        cpu.stub_import(IAT_STRICMP, lambda c: self.cmp(c, None))
        cpu.stub_import(IAT_STRNICMP, lambda c: self.cmp(c, c.arg(2)))
        cpu.stub_import(IAT_SPRINTF, self.sprintf)
        cpu.stub_function(DELETE, lambda c: 0)
        cpu.stub_function(REPORT, self.report)
        cpu.stub_function(BASE_CTOR, self.base_ctor, 4)
        self.scratch = self.alloc(0x400)
        self.ret_slot = self.alloc(16)
        # push 0; call eax; fstp dword [slot]; ret: the footfalls (one unused argument) return in ST0
        self.thunk = cpu.code(b'\x6a\x00\xff\xd0\xd9\x1d' + struct.pack('<I', self.ret_slot) + b'\xc3')

    def alloc(self, n):
        a = self.next
        self.next += (max(n, 1) + 0xf) & ~0xf
        self.cpu.write(a, bytes(max(n, 1)))
        return a

    def new(self, c):
        if self.pending:
            a, self.pending = self.pending, 0
            return a
        return self.alloc(c.arg(0))

    def calloc(self, c):
        return self.alloc(c.arg(0) * c.arg(1))

    def report(self, c):
        self.reports += 1
        return 0

    def base_ctor(self, c):
        # 0x41ed70 after the script is loaded here: its vtable, t and stride speed.
        c.u32(c.ecx, 0x5bd1b0)
        c.u32(c.ecx + 0x88, 0)
        c.u32(c.ecx + 0x8c, 0)
        return c.ecx

    def cstr(self, p):
        out = bytearray()
        while (b := self.cpu.read(p + len(out), 1)) != b'\0':
            out += b
        return bytes(out)

    def cmp(self, c, n):
        a, b = self.cstr(c.arg(0)).lower(), self.cstr(c.arg(1)).lower()
        if n is not None:
            a, b = a[:n], b[:n]
        return 0 if a == b else (-1 if a < b else 1)

    def sprintf(self, c):
        fmt = self.cstr(c.arg(1))
        assert fmt in (b'ltoe%02d', b'rtoe%02d'), fmt
        s = fmt.replace(b'%02d', b'%02d' % c.arg(2))
        c.write(c.arg(0), s + b'\0')
        return len(s)

    def read_file(self, c):
        h, buf, n, out = c.arg(0), c.arg(1), c.arg(2), c.arg(3)
        assert h == HANDLE
        chunk = self.data[self.pos:self.pos + n]
        self.pos += len(chunk)
        c.write(buf, chunk)
        if out:
            c.u32(out, len(chunk))
        return 1

    def u32(self, a):
        return struct.unpack('<I', self.cpu.read(a, 4))[0]

    def load(self, kind, name, start, speed, frames, mirror):
        cpu = self.cpu
        obj = self.alloc(0xf0)
        cpu.u32(obj, SCRIPT_VT)
        cpu.call(SCRIPT_CTOR, ecx=obj)
        cpu.write(obj + 0x10, b'\x01')  # 0x5a9970 / 0x5a9990: active
        self.pos = start
        cpu.call(SCRIPT_READ, ecx=obj, stack_args=(HANDLE,))
        cpu.call(SET_FPS, ecx=obj, stack_args=(f2u(30.0),))
        sname = self.alloc(len(name) + 1)
        cpu.write(sname, name.encode() + b'\0')
        self.pending = obj
        if kind == 0:
            cpu.call(LOAD_ANIM, ecx=sname, edx=frames, stack_args=(f2u(speed),))
        else:
            cpu.call(LOAD_ACTION, ecx=sname, edx=frames)
        if self.pending:
            raise SystemExit('the factory did not allocate')
        got = cpu.uc.reg_read(19)  # EAX
        if got != obj:
            return 0
        if mirror:
            cpu.call(self.u32(self.u32(obj) + 0x40), ecx=obj, stack_args=(f2u(speed), frames))
        return obj

    def blend(self, a, b):
        self.cpu.call(MAKE_BLEND, ecx=a, edx=b)
        return self.cpu.uc.reg_read(19)

    def copy(self, src):
        obj = self.alloc(0xdc)
        self.cpu.call(COPY, ecx=obj, stack_args=(src,))
        return obj

    def state(self, obj, kind, keys):
        r = self.cpu.read
        b = bytearray()
        b += r(obj + 4, 12) + struct.pack('<I', r(obj + 0x10, 1)[0])
        b += r(obj + 0x54, 20) + struct.pack('<I', r(obj + 0x70, 1)[0]) + r(obj + 0x74, 20)
        b += r(obj + 0x88, 8) + r(obj + 0xd0, 12)
        count, table = self.u32(obj + 0x68), self.u32(obj + 0x6c)
        b += struct.pack('<I', sum(1 for i in range(count) if table and self.u32(table + 4 * i)))
        n = self.u32(obj + 0x64)
        slots = [obj + 0x90 + 4 * i for i in range(16)]
        if kind == 1:
            slots += [obj + 0xdc + 4 * i for i in range(4)]
        for s in slots:
            p = self.u32(s)
            b += struct.pack('<I', 1 if p else 0)
            if not p:
                continue
            b += self.cstr(self.u32(p + 0x44)) + b'\0'
            b += struct.pack('<I', r(p + 0xc, 1)[0]) + r(p + 0x10, 48)
            vel = self.u32(p + 4)
            b += struct.pack('<I', 1 if vel else 0)
            if keys:
                b += r(self.u32(p), (n + 1) * 12) + r(self.u32(p + 8), (n + 1) * 16)
                if vel:
                    b += r(vel, (n + 1) * 12)
        if kind == 1:
            b += struct.pack('<II', r(obj + 0xec, 1)[0], r(obj + 0xed, 1)[0])
        if kind == 2:
            b += r(obj + 0xdc, 4)
        return bytes(b)

    def vcall(self, obj, off, args):
        self.cpu.call(self.u32(self.u32(obj) + off), ecx=obj, stack_args=args)
        return self.cpu.uc.reg_read(19) & 0xff  # AL

    def op(self, obj, op, a, objs):
        """The op's outputs as float bits, the probe's layout."""
        cpu, s = self.cpu, self.scratch
        rd = lambda p, n: list(struct.unpack(f'<{n}I', cpu.read(p, 4 * n)))  # noqa: E731
        fl = [f2u(x) for x in a]
        if op in (0, 1, 2, 3, 16):
            if op == 16:
                cpu.call(STRIDE_SPEED, ecx=obj, stack_args=(fl[0],))
            else:
                self.vcall(obj, VT[op], (fl[0],))
            return []
        if op == 4:
            if a[0]:
                cpu.write(s, struct.pack('<12f', *a[1:13]))
            self.vcall(obj, VT[4], (s if a[0] else 0,))
            return []
        if op == 5:
            if a[1]:
                cpu.write(s, struct.pack('<12f', *a[2:14]))
            self.vcall(obj, VT[5], (int(a[0]), s if a[1] else 0))
            return []
        if op == 6:
            cpu.write(s, struct.pack('<3f', *a[:3]))
            self.vcall(obj, VT[6], (s,))
            return rd(s, 3)
        if op == 7:
            self.vcall(obj, VT[7], (int(a[0]), s, s + 12))
            return rd(s, 7)
        if op in (8, 9):
            cpu.write(s, struct.pack('<7f', *a[3:10]))
            ok = self.vcall(obj, VT[op], (int(a[0]), s if a[1] else 0, s + 12 if a[2] else 0))
            return [f2u(1.0 if ok else 0.0)] + rd(s, 7)
        if op in (10, 11):
            self.vcall(obj, VT[op], (s, s + 12))
            return rd(s, 7)
        if op == 12:
            self.vcall(obj, VT[12], (s,))
            return rd(s, 4)
        if op == 13:
            cpu.write(s, struct.pack('<4f', *a[2:6]))
            ok = self.vcall(obj, VT[13], (int(a[0]), int(a[1]), s))
            return [f2u(1.0 if ok else 0.0)] + rd(s, 4)
        if op == 14:
            self.vcall(obj, VT[14], (int(a[0]) & 0xffffffff,))
            return []
        if op == 15:
            cpu.call(COPY_PHASE, ecx=obj, stack_args=(objs[int(a[0])],))
            return []
        if op == 17:
            cpu.uc.reg_write(19, FOOTFALLS)
            cpu.call(self.thunk, ecx=obj)
            return rd(self.ret_slot, 1)
        if op == 18:
            cpu.call(STOP, ecx=obj)
            return []
        if op == 19:
            cpu.call(RESET, ecx=obj)
            return []
        if op == 20:
            cpu.call(STARTED, ecx=obj)
            return [f2u(1.0 if cpu.uc.reg_read(19) & 0xff else 0.0)]
        raise ValueError(op)


def port_state(h, keys):
    buf = ctypes.create_string_buffer(1 << 20)
    n = probe.probe_ma_state(h, keys, buf, len(buf))
    assert n >= 0
    return buf.raw[:n]


def port_op(h, op, a):
    arr = (ctypes.c_float * 16)(*a)
    out = (ctypes.c_float * 16)()
    n = probe.probe_ma_op(h, op, arr, out)
    return [f2u(out[i]) for i in range(n)]


def first_diff(w, g):
    for i in range(min(len(w), len(g))):
        if w[i] != g[i]:
            return f'byte {i} of {len(w)} / {len(g)}'
    return f'lengths {len(w)} / {len(g)}'


def sync_mirror(emu, obj, h, kind):
    """The mirror's rotations (an Euler round trip through the CRT's asin,
    0x5aa940) against the port's within 2e-6, then the original's copied
    over so the samples after it can be checked bit for bit. Returns the
    largest difference."""
    n = emu.u32(obj + 0x64) + 1
    worst = 0.0
    stale = 0
    slots = [obj + 0x90 + 4 * i for i in range(16)] + ([obj + 0xdc + 4 * i for i in range(4)] if kind == 1 else [])
    for k, s in enumerate(slots):
        p = emu.u32(s)
        if not p:
            continue
        raw = emu.cpu.read(emu.u32(p + 8), n * 16)
        want = struct.unpack(f'<{4 * n}f', raw)
        old = (ctypes.c_float * (4 * n))()
        if not probe.probe_ma_set_rotations(h, k, ctypes.create_string_buffer(raw, len(raw)), old):
            raise SystemExit(f'mirror: part slot {k} missing in the port')
        worst = max(worst, max(abs(a - b) for a, b in zip(want, old)))
    if worst > 2e-6:
        raise SystemExit(f'mirror rotations {worst} apart')
    return worst


def rand_matrix(rng):
    if rng.random() < 0.3:
        m = [1, 0, 0, 0, 1, 0, 0, 0, 1]
    else:
        m = [rng.uniform(-1, 1) for _ in range(9)]
    return [f32(v) for v in m + [rng.uniform(-3, 3) for _ in range(3)]]


def rand_args(rng, op, kind, objs_kinds, loop):
    u = lambda lo, hi: f32(rng.uniform(lo, hi))  # noqa: E731
    if op == 0:
        return [rng.choice([0.0, 1.0, u(0, 3), u(-1, 0), u(5, 40)])]
    if op == 1:
        return [rng.choice([0.0, f32(loop), f32(loop * 0.5), u(-1, 3 * loop), u(0, loop)])]
    if op == 2:
        return [rng.choice([f32(1 / 30), f32(1 / 60), u(0, 0.2), u(-0.05, 0)])]
    if op == 3:
        return [rng.choice([0.0, 1.0, 0.5, u(0, 1)])]
    if op == 4:
        on = rng.random() < 0.7
        return [1.0 if on else 0.0] + rand_matrix(rng)
    if op == 5:
        on = rng.random() < 0.8
        return [float(rng.randrange(2)), 1.0 if on else 0.0] + rand_matrix(rng)
    if op == 6:
        return [u(-5, 5), u(-5, 5), u(-5, 5)]
    if op == 7:
        return [float(rng.randrange(2))]
    if op in (8, 9):
        return [float(rng.randrange(2)), float(rng.random() < 0.8), float(rng.random() < 0.8)] + [u(-9, 9) for _ in range(7)]
    if op == 13:
        return [float(rng.randrange(2)), float(rng.randrange(4))] + [u(-9, 9) for _ in range(4)]
    if op == 14:
        return [float(rng.choice([-1, 0, 1, 2]))]
    if op == 15:
        cands = [i for i, k in enumerate(objs_kinds) if k != 2 or True]
        return [float(rng.choice(cands))]
    if op == 16:
        # A blend with a stride and no stride speed gets a zero multiplier, and its
        # next set_blend divides by it: NaN, whose sign differs between the x87 and
        # this machine. The game never pairs those (stand and step have no stride).
        return [u(1, 40) if kind == 2 else rng.choice([0.0, u(1, 40)])]
    return []


def main():
    data = MOTION.read_bytes()
    table = {name: start for name, start, _ in entries(data)}
    lower = {name.lower(): name for name in table}
    emu = Emulated(data)
    if not probe.probe_ma_open(str(MOTION).encode()):
        raise SystemExit('the port could not open motion.zbd')
    rng = random.Random(1999)
    ops_per = int(sys.argv[1]) if len(sys.argv) > 1 else 400
    mechs = sorted({n.split('_')[0] for n in table if '_' in n})
    objs, kinds, handles, loops = [], [], [], []
    loaded = calls = 0
    worst = 0.0
    stale = 0
    seen_ops = set()

    def check_state(i, what, keys):
        w = emu.state(objs[i], kinds[i], keys)
        g = port_state(handles[i], keys)
        if w != g:
            head = '<3fI4fII3f2i5fI'
            raise SystemExit(f'{what}: state differs, {first_diff(w, g)}\n  original {struct.unpack(head, w[:84])}'
                             f'\n  port     {struct.unpack(head, g[:84])}')

    for mech in mechs:
        base = len(objs)
        mine = {}
        for kind, names in ((0, GAITS), (1, SHOTS)):
            for g in names:
                full = lower.get(f'{mech}_{g}'.lower())
                if not full:
                    continue
                for mirror in ([0, 1] if g == 'limp' else [0]):
                    speed = 0.0 if g in ('stand', 'step') or kind else f32(rng.uniform(5, 40))
                    frames = 60
                    o = emu.load(kind, full, table[full], speed, frames, mirror)
                    h = probe.probe_ma_load(kind, full.encode(), speed, frames, mirror)
                    if (o == 0) != (h < 0):
                        raise SystemExit(f'{full}: loaded {o != 0} in the original, {h >= 0} in the port')
                    if not o:
                        continue
                    objs.append(o), kinds.append(kind), handles.append(h)
                    loops.append(struct.unpack('<f', emu.cpu.read(o + 0x54, 4))[0])
                    if mirror:
                        worst = max(worst, sync_mirror(emu, o, h, kind))
                    check_state(len(objs) - 1, f'{full} (mirror {mirror})', 1)
                    if emu.u32(o + 0x84) > emu.u32(o + 0x64):
                        # The resample left cached keys past the new last key, under a
                        # cached t of 0: a sample at t = 0 reads beyond the arrays in the
                        # exe (the port recomputes, FIX (bug)). A sample elsewhere first.
                        for op, a in ((1, [f32(loops[-1] * 0.25)]), (10, []), (1, [0.0])):
                            emu.op(o, op, a, objs), port_op(h, op, a)
                        stale += 1
                    mine[g + ('r' if mirror else '')] = len(objs) - 1
                    loaded += 1
        if 'walk' in mine:
            i = mine['walk']
            w = emu.op(objs[i], 17, [], objs)
            g = port_op(handles[i], 17, [])
            if w != g:
                raise SystemExit(f'{mech}: footfalls differ')
            check_state(i, f'{mech} walk footfalls', 0)
            o = emu.copy(objs[i])
            h = probe.probe_ma_copy(handles[i])
            objs.append(o), kinds.append(0), handles.append(h), loops.append(loops[i])
            check_state(len(objs) - 1, f'{mech} walk copy', 1)
        for pa, pb in (('stand', 'step'), ('step', 'walk'), ('walk', 'trot'), ('trot', 'run'), ('walk', 'limp')):
            if pa in mine and pb in mine:
                o = emu.blend(objs[mine[pa]], objs[mine[pb]])
                h = probe.probe_ma_blend(handles[mine[pa]], handles[mine[pb]])
                objs.append(o), kinds.append(2), handles.append(h), loops.append(loops[mine[pa]])
                check_state(len(objs) - 1, f'{mech} {pa}/{pb} blend', 0)
        mine_all = list(range(base, len(objs)))
        for n in range(ops_per):
            i = rng.choice(mine_all)
            kind = kinds[i]
            allowed = list(range(17)) + ([17] if kind != 2 else []) + ([18, 19, 20] if kind == 1 else [])
            if kind == 2:
                allowed.remove(11)  # a blend has no torso part of its own: the exe would crash
            op = rng.choice(allowed)
            a = rand_args(rng, op, kind, kinds, loops[i])
            if op == 15:
                a = [float(rng.choice(mine_all))]
                w = emu.op(objs[i], 15, a, objs)
                g = port_op(handles[i], 15, [float(handles[int(a[0])])])
            else:
                w = emu.op(objs[i], op, a, objs)
                g = port_op(handles[i], op, a)
            what = f'{mech} object {i - base} (kind {kind}) op {n} {NAMES[op + 1]} {a[:3]}'
            if w != g:
                head = struct.unpack('<3fI4fII3f2i5fI', emu.state(objs[i], kind, 0)[:84])
                raise SystemExit(f'{what}: outputs differ\n  original {[hex(x) for x in w]}\n  port     {[hex(x) for x in g]}\n  state {head}')
            check_state(i, what, 0)
            seen_ops.add((kind, op))
            calls += 1
        for i in mine_all:
            check_state(i, f'{mech} object {i - base} at the end', 1)
    print(f'ok: {loaded} motions loaded identically ({len(mechs)} mechs), {calls} sampler and setter calls '
          f'identical bit for bit; {len(seen_ops)} (kind, op) pairs; {stale} stale caches primed, mirror rotations within {worst:.1e}')


if __name__ == '__main__':
    main()
