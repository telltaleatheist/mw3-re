"""zClass's action lists and frame update (docs/action-lists.md) against the
original x86 in Unicorn, the FPU at 24 bits as in play. Each mission world is
read by the original's 0x4ff520 and by the port's loaders, then:

1. The frame update 0x4f7f70 (0x4f7ec0, 0x4f7f10, 0x4f7e50 -> 0x4f7e00 ->
   0x4f7db0 -> gwNodeUpdate 0x4eca00 with the model box 0x4ed220 / 0x53b4e0,
   the children's box 0x4ecf70, the bounds and cells 0x4ecc00, the world's
   fog and area update 0x4f1df0 / 0x4f18a0) on what the load left changed,
   then over rounds of random moves: object3d translation, rotation, scale,
   added translation and the camera, a few nodes or many.
2. The action lists 0x4f7d80 (0x4f7d20 per list, the frame update after
   each): action nodes made by 0x4fb740 on priorities -1..7 (pushed front
   and back), active or not, each running a script that logs it and then
   sets or clears actions (its own, others', front or back), changes
   priorities (its own list included, mid-run), activity, removes nodes
   from lists and moves world objects. Scripts change every few frames.
3. Sequences (list 11, 0x4fdf50) and animates (list 12, 0x4fd950 with
   0x4fda20 / 0x4fdab0, then gwNodeUpdate's class 8) made by hand, as no
   shipped file has them: random headers and keys, frames of random steps.

After every call both sides' state is compared: every node (list 6; flags,
+0x2c, class, priority, cell, action set, parents and children, the boxes
+0x64..+0xbb, +0xc0/+0xc4, its list-7 item), its class data (camera,
object3d, sequence, animate; a world's header and every cell with its
nodes), the model boxes (+0x48..+0x57), the 16 lists with their items and
purge flags, and the trace of actions run and zerr reports, in order.

    cmake --build build && .venv/bin/python -P tests/differential/test_action_lists.py [rounds] [worlds...]
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

os.environ.setdefault('MW3_X87', '24')
sys.path.insert(0, str(Path(__file__).parent))
from original import ROOT  # noqa: E402
from test_gamez import (BIG, COUNT, COUNT_M, GROUP, HANDLE, IAT_FCLOSE, IAT_FOPEN, IAT_FSEEK, POOL,  # noqa: E402
                        POOL_COUNT, POOL_SIZE, READ_ZBD, SET_FORMAT, ZBD, ZERR, Emulated)
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
probe.al_load.argtypes = [ctypes.c_char_p]
probe.al_globals.argtypes = [ctypes.c_uint32, ctypes.c_float]
probe.al_set.argtypes = [ctypes.c_int32, ctypes.c_int32] + [ctypes.c_float] * 3
probe.al_set_action.argtypes = [ctypes.c_int32] * 3
probe.al_set_priority.argtypes = [ctypes.c_int32] * 2
probe.al_set_flags.argtypes = [ctypes.c_int32, ctypes.c_uint32]
probe.al_or.argtypes = [ctypes.c_int32, ctypes.c_int32, ctypes.c_uint32]
probe.al_set_matrix.argtypes = [ctypes.c_int32, ctypes.c_void_p]
probe.al_script.argtypes = [ctypes.c_int32, ctypes.c_void_p, ctypes.c_int32]
probe.al_seq.argtypes = [ctypes.c_char_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int32]
probe.al_animate.argtypes = [ctypes.c_char_p, ctypes.c_void_p, ctypes.c_int32, ctypes.c_uint32]
probe.al_dump.argtypes = [ctypes.c_void_p, ctypes.c_int32]
probe.al_trace.argtypes = [ctypes.c_void_p, ctypes.c_int32]

WORLDS = ['c1', 'c2', 'c3', 'c4', 'c4b', 't1']
FRAME_UPDATE, RUN_LISTS, OBJECT3D_CREATE, NODE_ALLOC, PUSH_FRONT = 0x4f7f70, 0x4f7d80, 0x4fb740, 0x4eacd0, 0x4f8080
SET_ACTION, SET_ACTION_BACK, SET_PRIORITY, SET_ACTIVE, LIST_REMOVE = 0x4eb450, 0x4eb500, 0x4eb5b0, 0x4eb170, 0x4f8220
SETTERS = {1: 0x4fc0b0, 2: 0x4fbd80, 3: 0x4fbc10, 4: 0x4f3870, 5: 0x4f36b0, 6: 0x4fc1b0}
NODE_BLOCKS, NODE_NBLOCKS, NODE_BLOCK_SIZE = 0x60bf38, 0x60bf30, 0x5ea830
LIST_HEADS, LISTS_ENABLED = 0x5ea6d0, 0x5ea6b0
FRAME, EXPIRY, DT = 0x72f338, 0x73f3a8, 0x72c52c
IAT_SPRINTF = 0x5bb658
WORLD_POSITION, SET_MATRIX = 0x4ed530, 0x4fc300
ACT = 0x2e200000          # the actions' entries, slots, log stubs, then their scripts' code
KACTIONS = 8
FLAG_LOG, POS_OUT, MATRIX_IN = ACT + 0xa00, ACT + 0xb00, ACT + 0xb40
END = 0xffffffff
COVERAGE = {0x4eca00: 'gwNodeUpdate', 0x4ed220: 'model box', 0x53b680: 'model extent', 0x4ecf70: 'children box',
            0x4ecc00: 'bounds', 0x4f2c30: 'cell left', 0x4f28c0: 'cell joined', 0x4f2360: 'cell touched',
            0x4f18a0: 'area update', 0x4f1df0: 'world fog/area', 0x4fd680: 'sound node', 0x4fdf50: 'sequence',
            0x4fd950: 'animate', 0x4fdab0: 'animate sample', 0x4f7d20: 'action list run'}


def f2u(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


class Emu(Emulated):
    def __init__(self):
        super().__init__()
        cpu = self.cpu
        cpu.fpu_control(0x007f)   # play precision (CLAUDE.md)
        cpu.stub_import(IAT_FOPEN, lambda c: HANDLE)
        cpu.stub_import(IAT_FSEEK, self.fseek)
        cpu.stub_import(IAT_FCLOSE, lambda c: 0)
        self.files_stubbed = True
        self.trace = []
        self.actions = 0
        cpu.stub_function(ZERR, lambda c: self.trace.extend([100, c.arg(0), c.arg(2)]))
        self.zerr_stubbed = True
        cpu.stub_import(IAT_SPRINTF, self.sprintf)
        # Each action is a fixed entry jumping through a slot to its script's
        # code; a new script is new code (Unicorn keeps translated code, so
        # code is never rewritten in place).
        cpu.uc.mem_map(ACT, 0x100000)
        cpu.write(ACT, b'\xc3' * 0x100000)
        self.generation = 0
        for k in range(KACTIONS):
            cpu.write(self.action_addr(k), b'\xff\x25' + struct.pack('<I', ACT + 0x100 + 4 * k))
            cpu.u32(ACT + 0x100 + 4 * k, ACT + 0xc00)  # a ret until scripted
            cpu.stub_function(self.log_addr(k), lambda c, k=k: self.log_action(k, c.ecx))
        # Each gwNodeUpdate in order, as the port's observer gives it.
        from unicorn import UC_HOOK_CODE
        cpu.uc.hook_add(UC_HOOK_CODE, lambda uc, a, n, u: self.trace.extend([3, self.index(self.cpu.ecx)]),
                        begin=0x4eca00, end=0x4eca00)
        cpu.stub_function(FLAG_LOG, self.flag_log)
        # What the runs reached in the original (printed at the end).
        from unicorn import UC_HOOK_CODE
        self.reached = dict.fromkeys(COVERAGE, 0)
        for a in COVERAGE:
            cpu.uc.hook_add(UC_HOOK_CODE, lambda uc, addr, n, u: self.reached.__setitem__(addr, self.reached[addr] + 1),
                            begin=a, end=a)

    def log_action(self, k, node):
        self.trace.extend([1, k, self.index(node)])
        self.actions += 1

    def flag_log(self, c):  # an action's op 9: what a node's flags say now
        n = c.ecx
        self.trace.extend([2, self.index(n)] + list(struct.unpack('<ii', c.read(n + 0x24, 4) + c.read(n + 0x2c, 4))))
        return 0

    def cstr(self, a):
        s = b''
        while True:
            ch = self.cpu.read(a + len(s), 1)
            if ch == b'\0':
                return s
            s += ch

    def sprintf(self, c):  # node_alloc's sprintf(name, "%s", "Default_node_name")
        assert self.cstr(c.arg(1)) == b'%s'
        s = self.cstr(c.arg(2))
        c.write(c.arg(0), s + b'\0')
        return len(s)

    @staticmethod
    def action_addr(k):
        return ACT + 0x10 * k if 0 <= k < KACTIONS else 0

    @staticmethod
    def log_addr(k):
        return ACT + 0x800 + 0x10 * k

    def u32(self, a):
        return struct.unpack('<I', self.cpu.read(a, 4))[0]

    def load(self, path):
        cpu = self.cpu
        data = path.read_bytes()
        self.next, self.data, self.pos = BIG, data, 0
        for a in (0x63fbc8, 0x63fbc0, 0x63fbc4, COUNT_M, POOL, POOL_SIZE, POOL_COUNT, COUNT):
            cpu.u32(a, 0)
        cpu.u32(GROUP, 3)
        cpu.call(SET_FORMAT, ecx=5, edx=6, stack_args=(5, 0, 0, 0))
        self.reset_world(-1)
        p = self.alloc(len(str(path)) + 1)
        cpu.write(p, str(path).encode() + b'\0')
        cpu.call(READ_ZBD, ecx=p)
        cpu.u32(0x74a288, self.calloc(12 * 0x4000))  # the model renderer's morph scratch (its init, 0x526510)
        self.trace = []
        return struct.unpack('<i', struct.pack('<I', cpu.uc.reg_read(UC_X86_REG_EAX)))[0]

    def eax(self):
        return struct.unpack('<i', struct.pack('<I', self.cpu.uc.reg_read(UC_X86_REG_EAX)))[0]

    # --- nodes ---
    def blocks(self):
        nb = self.u32(NODE_NBLOCKS)
        return [self.u32(self.u32(NODE_BLOCKS) + 4 * b) for b in range(nb)]

    def index(self, ptr):
        if ptr == 0:
            return -1
        bs = self.u32(NODE_BLOCK_SIZE)
        for b, base in enumerate(self.blocks()):
            if base <= ptr < base + bs * 0xd4:
                return b * bs + (ptr - base) // 0xd4
        return -2

    def ptr(self, i):
        if i < 0:
            return 0
        bs = self.u32(NODE_BLOCK_SIZE)
        return self.blocks()[i // bs] + (i % bs) * 0xd4

    def call(self, addr, ecx=0, edx=0, stack_args=()):
        try:
            self.cpu.call(addr, ecx=ecx, edx=edx, stack_args=stack_args, count=2_000_000_000)
        except Exception as e:
            from unicorn.x86_const import UC_X86_REG_EIP, UC_X86_REG_ESP
            esp = self.cpu.uc.reg_read(UC_X86_REG_ESP)
            stack = struct.unpack('<8I', self.cpu.read(esp, 32))
            raise SystemExit(f'{addr:#x} faulted at {self.cpu.uc.reg_read(UC_X86_REG_EIP):#x}: {e}; '
                             f'stack {[hex(x) for x in stack]}')
        return self.eax()

    # --- the actions' code ---
    def script(self, k, ops):
        code = bytearray()
        self.generation += 1
        base = ACT + 0x1000 * self.generation
        assert base + 0x1000 <= ACT + 0x100000

        def call(target):
            at = base + len(code)
            code.extend(b'\xe8' + struct.pack('<i', target - (at + 5)))

        def imm(op, v):
            code.extend(bytes([op]) + struct.pack('<I', v & 0xffffffff))
        code.extend(b'\x51')                     # push ecx
        call(self.log_addr(k))
        code.extend(b'\x59')                     # pop ecx
        for op, a, b, c, d, _ in ops:
            if op in (1, 2):
                imm(0xb9, self.ptr(a)), imm(0xba, self.action_addr(b))
                call(SET_ACTION if op == 1 else SET_ACTION_BACK)
            elif op == 3:
                imm(0xb9, self.ptr(a)), imm(0xba, b), call(SET_PRIORITY)
            elif op == 4:
                imm(0xb9, self.ptr(a)), imm(0xba, b), call(SET_ACTIVE)
            elif op in (5, 7):
                imm(0x68, d), imm(0x68, c), imm(0x68, b), imm(0xb9, self.ptr(a))
                call(SETTERS[1] if op == 5 else SETTERS[2])
            elif op == 6:
                imm(0xb9, a), imm(0xba, self.ptr(b)), call(LIST_REMOVE)
            elif op == 8:
                imm(0xb9, self.ptr(a)), imm(0xba, POS_OUT), call(WORLD_POSITION)
            elif op == 9:
                imm(0xb9, self.ptr(a)), call(FLAG_LOG)
        code.extend(b'\xc3')
        assert len(code) <= 0x1000
        self.cpu.write(base, bytes(code))
        self.cpu.u32(ACT + 0x100 + 4 * k, base)

    # --- the state ---
    def dump(self):
        cpu, u32 = self.cpu, self.u32
        out = bytearray()

        def w(v):
            out.extend(struct.pack('<I', v & 0xffffffff))
        item = u32(u32(LIST_HEADS + 4 * 6))  # every node is on list 6
        while item:
            n = u32(item)
            item = u32(item + 8)
            if not n:
                continue
            raw = cpu.read(n, 0xd4)
            g = lambda o: struct.unpack_from('<I', raw, o)[0]  # noqa: E731
            w(self.index(n)), w(g(0x24)), w(g(0x2c)), w(g(0x34)), w(g(0x44)), w(g(0x4c)), w(g(0x50)), w(1 if g(0x48) else 0)
            w(g(0x54))
            for k in range(g(0x54)):
                w(self.index(u32(g(0x58) + 4 * k)))
            w(g(0x5c))
            for k in range(g(0x5c)):
                w(self.index(u32(g(0x60) + 4 * k)))
            out.extend(raw[0x64:0xbc])
            w(g(0xc0)), w(g(0xc4))
            w(u32(g(0xcc) + 0xc) if g(0xcc) else END)
            if g(0x3c):
                out.extend(cpu.read(g(0x3c) + 0x48, 0x10))
            d, cls = g(0x38), g(0x34)
            if not d:
                continue
            if cls == 1:
                out.extend(cpu.read(d + 0x10, 0x20) + cpu.read(d + 0x80, 0x30))
            elif cls == 5:
                out.extend(cpu.read(d, 0x90))
            elif cls == 7:
                out.extend(cpu.read(d, 0x20))
            elif cls == 8:
                out.extend(cpu.read(d, 0x6c) + cpu.read(d + 0x70, 0xd4 - 0x70))
            elif cls == 2:
                out.extend(cpu.read(d, 0xc) + cpu.read(d + 0x10, 0x80))
                nx, nz, rows = u32(d + 0x88), u32(d + 0x8c), u32(d + 0x90)
                for z in range(nz):
                    row = u32(rows + 4 * z)
                    for x in range(nx):
                        c = row + 0x48 * x
                        out.extend(cpu.read(c, 0x3c))
                        cnt = struct.unpack('<h', cpu.read(c + 0x3a, 2))[0]
                        for k in range(cnt):
                            w(self.index(u32(u32(c + 0x3c) + 4 * k)))
        w(END)
        for l in range(16):
            head = u32(LIST_HEADS + 4 * l)
            it = u32(head)
            while it:
                w(self.index(u32(it))), w(u32(it + 0xc))
                it = u32(it + 8)
            w(0xfffffffe), w(u32(head + 8))
        w(u32(LISTS_ENABLED))
        return bytes(out)


def port_dump():
    buf = ctypes.create_string_buffer(1 << 24)
    n = probe.al_dump(buf, len(buf))
    assert n <= len(buf)
    return buf.raw[:n]


def port_trace():
    buf = (ctypes.c_int32 * 200000)()
    n = probe.al_trace(buf, len(buf))
    assert n <= len(buf)
    return [buf[i] for i in range(n)]


MAX_REL = [0.0]


def close(a, b):
    """Two dump words that differ: alike when both read as ordinary floats
    within 1e-5 of each other (the x87 and the port in the last bits)."""
    fa, fb = (struct.unpack('<f', struct.pack('<I', x))[0] for x in (a, b))
    ea, eb = (a >> 23) & 0xff, (b >> 23) & 0xff
    if not (0x40 <= ea <= 0xbf and 0x40 <= eb <= 0xbf):
        return False
    rel = abs(fa - fb) / max(abs(fa), abs(fb), 1.0)
    MAX_REL[0] = max(MAX_REL[0], rel)
    return rel <= 1e-5


class Run:
    def __init__(self, emu):
        self.emu = emu
        self.checks = 0
        self.inexact = 0

    def compare(self, what):
        e, p = self.emu.dump(), port_dump()
        te, tp = self.emu.trace, port_trace()
        self.emu.trace = []
        if te != tp:
            i = next((k for k in range(min(len(te), len(tp))) if te[k] != tp[k]), min(len(te), len(tp)))
            raise SystemExit(f'{what}: traces differ at {i}: original {te[max(0, i - 6):i + 9]} '
                             f'port {tp[max(0, i - 6):i + 9]} (lengths {len(te)} / {len(tp)})')
        if len(e) != len(p):
            raise SystemExit(f'{what}: dump sizes differ: original {len(e)} port {len(p)}')
        if e != p:
            we = struct.unpack(f'<{len(e) // 4}I', e)
            wp = struct.unpack(f'<{len(p) // 4}I', p)
            for k, (a, b) in enumerate(zip(we, wp)):
                if a != b:
                    if not close(a, b):
                        raise SystemExit(f'{what}: dumps differ at word {k}: original {a:#x} port {b:#x}; '
                                         f'around: {[hex(x) for x in we[max(0, k - 8):k + 4]]} / '
                                         f'{[hex(x) for x in wp[max(0, k - 8):k + 4]]}')
                    self.inexact += 1
        self.checks += 1
        return len(te)


def object3d_nodes(emu):
    out = []
    bs = emu.u32(NODE_BLOCK_SIZE)
    blocks = emu.blocks()
    for i in range(len(blocks) * bs):
        n = blocks[i // bs] + (i % bs) * 0xd4
        if emu.u32(n + 0xd0) & 0x1000000 and emu.u32(n + 0x34) == 5 and emu.u32(n + 0x38):
            out.append(i)
    return out


def rand_vec(rng, kind, base):
    if kind == 2:
        return [rng.uniform(-3.2, 3.2) for _ in range(3)]
    if kind == 3:
        return [1.0, 1.0, 1.0] if rng.random() < 0.3 else [rng.uniform(0.2, 3.0) for _ in range(3)]
    if rng.random() < 0.1:
        return [0.0, 0.0, 0.0]
    spread = rng.choice((1.0, 20.0, 200.0, 2000.0))
    return [b + rng.uniform(-spread, spread) for b in base]


def both_set(emu, i, what, v):
    emu.call(SETTERS[what], ecx=emu.ptr(i), stack_args=tuple(f2u(x) for x in v))
    re = emu.eax()
    rp = probe.al_set(i, what, *v)
    if re != rp:
        raise SystemExit(f'setter {what} on {i}: original {re} port {rp}')


def both_globals(emu, frame, dt):
    emu.cpu.u32(FRAME, frame)
    emu.cpu.u32(DT, f2u(dt))
    probe.al_globals(frame, dt)


def special_models(emu, objs):
    """The objects whose models are rare kinds: facades (type 1, both modes),
    morphs, models of lights only."""
    out = []
    for i in objs:
        m = emu.u32(emu.ptr(i) + 0x3c)
        if m and (emu.u32(m) == 1 or emu.u32(m + 0x1c) or not emu.u32(m + 0x14)):
            out.append(i)
    return out


def part_frames(run, rng, objs, camera, rounds):
    emu = run.emu
    # First every rare model's box rebuilt, the objects left where they are.
    for i in special_models(emu, objs):
        pos = struct.unpack('<3f', emu.cpu.read(emu.u32(emu.ptr(i) + 0x38) + 0x54, 12))
        both_set(emu, i, 1, list(pos))
        emu.cpu.u32(emu.ptr(i) + 0x2c, emu.u32(emu.ptr(i) + 0x2c) | 4)
        probe.al_or(i, 0x2c, 4)
    run.compare('the rare models marked')
    emu.call(FRAME_UPDATE)
    probe.al_frame_update()
    run.compare('the rare models updated')
    for r in range(rounds):
        both_globals(emu, 10 + r, rng.choice((0.0, 1 / 30, 1 / 60, 0.25)))
        n = rng.choice((1, 2, 5, 20, 100))
        for _ in range(n):
            if camera is not None and rng.random() < 0.08:
                what = rng.choice((4, 5))
                v = rand_vec(rng, 2 if what == 5 else 1, [0.0, 50.0, 0.0])
                both_set(emu, camera, what, v)
                continue
            i = rng.choice(objs)
            what = rng.choice((1, 1, 2, 3, 6))
            pos = struct.unpack('<3f', emu.cpu.read(emu.u32(emu.ptr(i) + 0x38) + 0x54, 12))
            v = rand_vec(rng, what if what in (2, 3) else 1, pos if what == 1 else [0.0, 0.0, 0.0])
            if what == 6:
                v = [rng.uniform(-50, 50) for _ in range(3)]
            both_set(emu, i, what, v)
            if rng.random() < 0.05:  # its own matrix from now on (USE_MATRIX)
                m = [rng.uniform(-1, 1) for _ in range(9)] + [p + rng.uniform(-30, 30) for p in pos]
                emu.cpu.write(MATRIX_IN, struct.pack('<12f', *m))
                re = emu.call(SET_MATRIX, ecx=emu.ptr(i), edx=MATRIX_IN)
                rp = probe.al_set_matrix(i, (ctypes.c_float * 12)(*m))
                if re != rp:
                    raise SystemExit(f'set_matrix on {i}: original {re} port {rp}')
            if rng.random() < 0.3:  # its model box (and children's box) due again, as a model change marks it
                bits = rng.choice((4, 2, 6))
                emu.cpu.u32(emu.ptr(i) + 0x2c, emu.u32(emu.ptr(i) + 0x2c) | bits)
                probe.al_or(i, 0x2c, bits)
        run.compare(f'round {r}: {n} moves')
        emu.call(FRAME_UPDATE)
        probe.al_frame_update()
        run.compare(f'round {r}: frame update')


def random_script(rng, actors, objs):
    ops = []
    for _ in range(rng.randint(0, 4)):
        op = rng.choice((1, 1, 2, 3, 3, 4, 5, 6, 7, 8, 9, 9))
        a = rng.choice(actors) if op != 6 else rng.choice((0, 1, 2, 3, 4, 5, 7))
        if op in (8, 9):
            ops.append((op, rng.choice(objs) if rng.random() < 0.7 else a, 0, 0, 0, 0))
            continue
        if op in (1, 2):
            ops.append((op, a, rng.choice(list(range(KACTIONS)) + [-1, -1]), 0, 0, 0))
        elif op == 3:
            ops.append((op, a, rng.choice((-1, 0, 1, 2, 3, 4, 5, 6)), 0, 0, 0))
        elif op == 4:
            ops.append((op, a, rng.choice((0, 1)), 0, 0, 0))
        elif op in (5, 7):
            target = rng.choice(objs) if rng.random() < 0.8 else a
            v = [rng.uniform(-500, 500), rng.uniform(-10, 60), rng.uniform(-500, 500)] if op == 5 else \
                [rng.uniform(-3, 3) for _ in range(3)]
            ops.append((op, target, *[struct.unpack('<i', struct.pack('<f', x))[0] for x in v], 0))
        else:
            ops.append((6, a, rng.choice(actors), 0, 0, 0))
    return ops


def part_actions(run, rng, objs, frames):
    emu = run.emu
    actors = []
    for _ in range(12):
        ie = emu.index(emu.call(OBJECT3D_CREATE) & 0xffffffff)
        ip = probe.al_object3d()
        if ie != ip:
            raise SystemExit(f'object3d_create: original node {ie}, port {ip}')
        actors.append(ie)
    run.compare('the action nodes')
    scripts = {}
    for f in range(frames):
        if f % 4 == 0:
            for k in range(KACTIONS):
                scripts[k] = random_script(rng, actors, objs)
                emu.script(k, scripts[k])
                flat = (ctypes.c_int32 * (6 * max(1, len(scripts[k]))))(*[x for o in scripts[k] for x in o])
                probe.al_script(k, flat, len(scripts[k]))
            for a in actors:
                if f == 0 or rng.random() < 0.4:
                    p = rng.choice((0, 1, 2, 3, 4, 5)) if f == 0 else rng.choice((-1, 0, 1, 2, 3, 4, 5, 6, 7))
                    re = emu.call(SET_PRIORITY, ecx=emu.ptr(a), edx=p)
                    rp = probe.al_set_priority(a, p)
                    assert re == rp, ('priority', a, p, re, rp)
                if f == 0 or rng.random() < 0.5:
                    k = rng.randrange(KACTIONS) if f == 0 else rng.choice(list(range(KACTIONS)) + [-1])
                    back = rng.random() < 0.5
                    re = emu.call(SET_ACTION_BACK if back else SET_ACTION, ecx=emu.ptr(a), edx=emu.action_addr(k))
                    rp = probe.al_set_action(a, k, int(back))
                    assert re == rp, ('action', a, k, re, rp)
                if rng.random() < 0.2:
                    flags = emu.u32(emu.ptr(a) + 0x24) ^ 4
                    emu.cpu.u32(emu.ptr(a) + 0x24, flags)
                    probe.al_set_flags(a, flags)
            run.compare(f'frame {f}: the actions set')
        both_globals(emu, 100 + f, rng.choice((1 / 30, 1 / 60)))
        if os.environ.get('MW3_DUMP') == '1':
            for l in range(7):
                it, items = emu.u32(emu.u32(LIST_HEADS + 4 * l)), []
                while it:
                    items.append((emu.index(emu.u32(it)), emu.u32(it + 0xc)))
                    it = emu.u32(it + 8)
                print('list', l, items)
            for a in actors:
                pa = emu.ptr(a)
                print('actor', a, 'prio', emu.u32(pa + 0x44), 'act', (emu.u32(pa + 0x48) - ACT) // 0x200 if emu.u32(pa + 0x48) else None, 'flags', hex(emu.u32(pa + 0x24)))
        emu.call(RUN_LISTS)
        probe.al_run_lists()
        try:
            run.compare(f'frame {f}: the action lists')
        except SystemExit:
            if os.environ.get('MW3_DUMP') == '1':
                for k, ops in scripts.items():
                    print('script', k, ops)
            raise


def part_timed(run, rng, frames):
    emu = run.emu
    cpu = emu.cpu
    for _ in range(6):
        count = rng.randint(1, 5)
        times = [rng.choice((0.0, rng.uniform(0.02, 0.3), rng.uniform(0.02, 0.3))) for _ in range(count)]
        if all(t == 0.0 for t in times):
            times[0] = 0.1
        header = struct.pack('<iiiiiifi', rng.random() < 0.8, rng.random() < 0.5, rng.random() < 0.5,
                             rng.random() < 0.15, rng.choice((1, 1, -1)), rng.randrange(count),
                             rng.uniform(0, 0.2), count)
        # the original's node and data
        n = emu.call(NODE_ALLOC) & 0xffffffff
        cpu.u32(n + 0x34, 7)
        d = emu.calloc(0x20 + 8 * count)
        cpu.write(d, header)
        for k, t in enumerate(times):
            cpu.write(d + 0x20 + 8 * k, struct.pack('<If', 0, t))
        cpu.u32(n + 0x38, d)
        emu.call(PUSH_FRONT, ecx=11, edx=n)
        nodes = (ctypes.c_int32 * count)(*([-1] * count))
        ts = (ctypes.c_float * count)(*times)
        ip = probe.al_seq(header, nodes, ts, count)
        assert ip == emu.index(n), ('seq node', ip, emu.index(n))
    for _ in range(6):
        nk = rng.randint(2, 6)
        keys = [rng.uniform(-3, 3) for _ in range(9 * (nk + 1))]
        dur = rng.uniform(0.2, 2.0)
        raw = bytearray(rng.getrandbits(8) for _ in range(0xd4))
        struct.pack_into('<II', raw, 0, rng.choice((0, 1, 2, 3)), rng.choice((0, 4, 4, 4, 5)))
        struct.pack_into('<9f', raw, 0x94, *[rng.choice((1.0, rng.uniform(-2, 2))) for _ in range(9)])
        loop = rng.choice((-1, 0))
        struct.pack_into('<fIfff', raw, 0xb8, dur, 0, rng.uniform(0, dur * 0.8), rng.uniform(0, 0.2),
                         rng.uniform(dur * 0.5, dur))
        struct.pack_into('<hhhh', raw, 0xcc, rng.choice((0, 1, 1, 2)), 0, nk, loop)
        flags = rng.choice((0, 4, 4, 4))
        n = emu.call(NODE_ALLOC) & 0xffffffff
        cpu.u32(n + 0x34, 8)
        cpu.u32(n + 0x24, flags)
        d = emu.calloc(0xd4)
        kp = emu.calloc(4 * len(keys))
        cpu.write(kp, struct.pack(f'<{len(keys)}f', *keys))
        struct.pack_into('<I', raw, 0x6c, kp)
        cpu.write(d, bytes(raw))
        cpu.u32(n + 0x38, d)
        emu.call(PUSH_FRONT, ecx=12, edx=n)
        ka = (ctypes.c_float * len(keys))(*keys)
        ip = probe.al_animate(bytes(raw), ka, nk + 1, flags)
        assert ip == emu.index(n), ('animate node', ip, emu.index(n))
    run.compare('the sequences and animates')
    for f in range(3 * frames):
        both_globals(emu, 200 + f, rng.choice((0.0, 1 / 30, 0.1, 0.25)))
        emu.call(FRAME_UPDATE)
        probe.al_frame_update()
        run.compare(f'timed frame {f}')


def main():
    args = sys.argv[1:]
    rounds = int(args.pop(0)) if args and args[0].isdigit() else 12
    worlds = args or WORLDS
    rng = random.Random(1234)
    total_checks = total_inexact = total_actions = 0
    for w in worlds:
        path = ZBD / w / 'gamez.zbd'
        emu = Emu()
        re = emu.load(path)
        rp = probe.al_load(str(path).encode())
        if re != rp:
            raise SystemExit(f'{w}: read_zbd original {re} port {rp}')
        port_trace()
        both_globals(emu, 1, 1 / 30)
        run = Run(emu)
        run.compare(f'{w}: loaded')
        emu.call(FRAME_UPDATE)
        probe.al_frame_update()
        run.compare(f'{w}: the first frame update')
        objs = object3d_nodes(emu)
        cams = [i for i in range(len(emu.blocks()) * emu.u32(NODE_BLOCK_SIZE))
                if emu.u32(emu.ptr(i) + 0xd0) & 0x1000000 and emu.u32(emu.ptr(i) + 0x34) == 1]
        part_frames(run, rng, objs, cams[0] if cams else None, rounds)
        part_timed(run, rng, rounds)  # its sequences and animates stay for the action lists' frames
        part_actions(run, rng, objs, rounds)
        print(f'{w}: {len(objs)} objects, {run.checks} states identical ({run.inexact} words within 1e-5), '
              f'{emu.actions} actions run')
        print('   reached: ' + ', '.join(f'{COVERAGE[a]} {n}' for a, n in emu.reached.items()))
        total_checks += run.checks
        total_inexact += run.inexact
        total_actions += emu.actions
    print(f'all identical: {total_checks} states over {len(worlds)} worlds, {total_actions} actions run, '
          f'{total_inexact} float words within 1e-5 (largest relative difference {MAX_REL[0]:.2g})')


if __name__ == '__main__':
    code = 0
    try:
        main()
    except SystemExit as e:
        if e.code not in (None, 0):
            print(e.code if isinstance(e.code, str) else f'failed: {e.code}')
            code = 1
    sys.stdout.flush()
    os._exit(code)  # Unicorn can hang at interpreter exit on this Mac
