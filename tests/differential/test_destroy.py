"""zClass taking nodes down (docs/zclass-world.md, Destroy; docs/action-lists.md)
against the original x86, on the six mission worlds.

    cmake --build build && .venv/bin/python -P tests/differential/test_destroy.py

Each world's gamez.zbd is read whole by both sides (0x4ff520), then run-time
nodes join it as the game makes them: object3d nodes under the world (in its
cells or among its own children) and under other nodes, some with actions
on lists 0..5 (front and back), lights and sounds the world lists. Then
random operations on random nodes of list 6: the list delete 0x4f84b0, the
class delete 0x4eaea0, the release 0x4eb050, the tree delete 0x4f9050, a
child's removal 0x4ebb40, the slot free 0x4eaf90, a model dropped
(0x4eb370), immediate deletes off ([0x5e9e18] 0, the shutdown's setting)
and the lists held off ([0x5ea6b0] 0, a running action list: the deferred
list) then purged (0x4f7bd0). Last the shutdown pass 0x4f8ab0 and the reset
0x4f8fb0 (0x4f8f40). After every call: its result, the whole node pool
(every slot, links, class data with each world's cells), the 16 lists and
their items, the item pool, the deferred list, the reports, the model
pool's count and free head; every 16 calls and at the end every model
slot's use count, polygon count and link.

DESTROY_TEST_BREAK=<n> runs with break n compiled into nothing: the breaks
are made by editing the port (docs/action-lists.md lists them).
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_gamez as G  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

probe = G.probe
probe.probe_destroy_load.argtypes = [ctypes.c_char_p]
probe.probe_destroy_op.argtypes = [ctypes.c_int] * 4
probe.probe_destroy_dump.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_int]

IMMEDIATE, ENABLED, DEFERRED = 0x5e9e18, 0x5ea6b0, 0x60bf10
FRAME, EXPIRY, UPDATE_FRAME, FROZEN = 0x72f338, 0x73f3a8, 0x5e9e2c, 0x60b94c
IAT_SPRINTF = 0x5bb658
FAKE_ACTION = 0x401000  # never run: no action list runs here
OPS = {
    0: 0x4f84b0, 1: 0x4eaea0, 2: 0x4eb050, 3: 0x4f9050, 11: 0x4eaf90,
}
HISTORY = Path(os.environ['DESTROY_HISTORY']) if os.environ.get('DESTROY_HISTORY') else None  # debugging
NAMES = {0: 'list delete', 1: 'class delete', 2: 'release', 3: 'tree delete', 4: 'remove child',
         5: 'immediate', 6: 'lists held', 7: 'purge', 8: 'object3d', 9: 'light', 10: 'sound',
         11: 'free', 12: 'shutdown pass', 13: 'reset', 14: 'drop model'}


def s32(v):
    return struct.unpack('<i', struct.pack('<I', v & 0xffffffff))[0]


class Emulated(G.Emulated):
    def __init__(self):
        super().__init__()
        self.cpu.stub_import(IAT_SPRINTF, self.sprintf)

    def cstr(self, p):
        out = bytearray()
        while (b := self.cpu.read(p + len(out), 1)) != b'\0':
            out += b
        return bytes(out)

    def sprintf(self, c):
        """Only the node name's "%s" (0x4eacd0) is kept; the reports' text is not compared."""
        s = self.cstr(c.arg(2)) if self.cstr(c.arg(1)) == b'%s' else b''
        self.cpu.write(c.arg(0), s + b'\0')
        return len(s)

    def load(self, path):
        cpu = self.cpu
        for a, v in ((IMMEDIATE, 1), (ENABLED, 1), (FRAME, 0), (EXPIRY, 0), (UPDATE_FRAME, 1), (FROZEN, 0)):
            cpu.u32(a, v)
        out = self.read_zbd(path.read_bytes(), str(path).encode())
        return struct.unpack_from('<i', out)[0]

    def blocks(self):
        i32 = self.i32
        return [i32(i32(G.NODE_BLOCKS) + 4 * b) & 0xffffffff for b in range(i32(G.NODE_NBLOCKS))]

    def node(self, index):
        if index < 0:
            return 0
        bs = self.i32(0x5ea830)
        return self.blocks()[index // bs] + (index % bs) * 0xd4

    def index(self, ptr):
        bs = self.i32(0x5ea830)
        for b, base in enumerate(self.blocks()):
            if base <= ptr < base + bs * 0xd4:
                return b * bs + (ptr - base) // 0xd4
        return -1 if ptr == 0 else -2

    def ret(self):
        return s32(self.cpu.uc.reg_read(UC_X86_REG_EAX))

    def op(self, op, a, b, c):
        cpu, n = self.cpu, self.node(a)
        if op in OPS:
            cpu.call(OPS[op], ecx=n)
            return self.ret()
        if op == 4:
            cpu.call(0x4ebb40, ecx=self.node(b), edx=n)
            return self.ret()
        if op in (5, 6):
            cpu.u32(IMMEDIATE if op == 5 else ENABLED, a)
            return 0
        if op == 7:
            cpu.call(0x4f7bd0)
            return self.ret()
        if op == 8:
            cpu.call(0x4fb740)
            o = cpu.uc.reg_read(UC_X86_REG_EAX)
            if n:
                cpu.call(0x4f2a40 if self.i32(n + 0x34) == 2 else 0x4eb9b0, ecx=n, edx=o)
            if b >= 0:
                cpu.call(0x4eb5b0, ecx=o, edx=b)
                cpu.call(0x4eb500 if c else 0x4eb450, ecx=o, edx=FAKE_ACTION)
            return self.index(o)
        if op in (9, 10):
            cpu.call(0x4fa720 if op == 9 else 0x4fd280)
            o = cpu.uc.reg_read(UC_X86_REG_EAX)
            if n:
                cpu.call(0x4f2d50 if op == 9 else 0x4f2f90, ecx=n, edx=o)
            return self.index(o)
        if op in (12, 13):
            cpu.call(0x4f8ab0 if op == 12 else 0x4f8f40)
            return self.ret()
        if op == 14:
            cpu.call(0x4eb370, ecx=n, edx=(self.i32(G.POOL) + 0x60 * b) if b >= 0 else 0)
            return self.ret()
        raise ValueError(op)

    def dump(self, pool):
        i32 = self.i32
        out = [self.dump_world(i32(G.NODE_CAP), items=True)]
        out.append(struct.pack('<ii', i32(IMMEDIATE), i32(ENABLED)) + self.cpu.read(0x60bf50, 4))
        items, p = [], i32(DEFERRED) & 0xffffffff
        while p:
            items.append(p)
            p = i32(p + 8) & 0xffffffff
        chunks = [i32((i32(G.ITEM_CHUNKS) & 0xffffffff) + 4 * c) & 0xffffffff for c in range(i32(G.ITEM_NCHUNKS))]
        cs = i32(0x5ea750)

        def item(ptr):
            for c, base in enumerate(chunks):
                if ptr and base <= ptr < base + cs * 0x10:
                    return struct.pack('<ii', c, (ptr - base) // 0x10)
            return struct.pack('<ii', -1, -1)
        out.append(struct.pack('<i', len(items)))
        for p in items:
            out.append(struct.pack('<i', self.index(i32(p) & 0xffffffff)) + item(p))
        out.append(struct.pack('<ii', i32(G.POOL_COUNT), i32(G.POOL_FREE)))
        if pool:
            size, base = i32(G.POOL_SIZE), i32(G.POOL) & 0xffffffff
            raw = self.cpu.read(base, size * 0x60) if size > 0 else b''
            out.append(b''.join(raw[k * 0x60 + 0xc:k * 0x60 + 0x14] + raw[k * 0x60 + 0x5c:k * 0x60 + 0x60]
                                for k in range(size)))
        return b''.join(out)

    def listed(self):
        """The nodes on list 6, by pool index (removed items skipped)."""
        out, p = [], self.i32(G.LIST_HEADS + 4 * 6) & 0xffffffff
        p = self.i32(p) & 0xffffffff
        while p:
            node = self.i32(p) & 0xffffffff
            if node:
                out.append(self.index(node))
            p = self.i32(p + 8) & 0xffffffff
        return out

    def info(self, index):
        n = self.node(index)
        cls, data = self.i32(n + 0x34), self.i32(n + 0x38)
        return cls, self.i32(n + 0x54), self.i32(n + 0x5c), self.i32(n + 0x3c), data


def compare(what, want, buf, n):
    got = buf.raw[:max(n, 0)]
    if got != want:
        if HISTORY:
            HISTORY.with_suffix('.want').write_bytes(want)
            HISTORY.with_suffix('.got').write_bytes(got)
        at = next((i for i in range(min(len(got), len(want))) if got[i] != want[i]), min(len(got), len(want)))
        raise SystemExit(f'{what}: original {len(want)} bytes, port {n}, first difference at {at}')


def pick_op(rng, emu, worlds):
    """One random operation as (op, a, b, c) on the original's current nodes."""
    nodes = emu.listed()
    if not nodes:
        return 12, -1, -1, 0
    r = rng.random()
    n = rng.choice(nodes)
    cls, parents, children, model, data = emu.info(n)
    if r < 0.18:
        parent = -1
        for _ in range(8):
            cand = rng.choice(nodes + worlds)
            if emu.info(cand)[0] in (2, 5):
                parent = cand
                break
        prio = rng.choice([-1, -1, 0, 1, 2, 3, 4, 5])
        return 8, parent, prio, rng.randrange(2)
    if r < 0.22 and worlds:
        return rng.choice((9, 10)), rng.choice(worlds + [-1]), -1, 0
    if r < 0.27:
        return 5, rng.choice((0, 1, 1)), 0, 0
    if r < 0.30:
        return 6, 0, 0, 0
    if r < 0.33:
        return 7, -1, 0, 0
    if r < 0.38 and parents > 0:
        parent = emu.index(emu.i32(emu.i32(emu.node(n) + 0x58) & 0xffffffff) & 0xffffffff)
        if emu.info(parent)[0] in (1, 2, 3, 4, 5, 6, 8, 9, 10):
            return 4, n, parent, 0
    if r < 0.41 and model:
        return 14, n, -1, 0
    if cls == 0:
        return 0, n, 0, 0  # an unknown class: 3, reported when deletes are immediate
    # The bare release and free skip the class's own delete: a light or sound
    # freed so would stay in its world's list (both sides then crash), so
    # they are only used on object3d nodes, as the game does.
    return rng.choice((0, 0, 0, 1, 3, 3) + ((2, 11) if cls == 5 else ())), n, 0, 0


def main():
    emu = Emulated()
    buf = ctypes.create_string_buffer(64 << 20)
    seed = int(sys.argv[1]) if len(sys.argv) > 1 else 1
    total_ops = calls = 0
    worlds = os.environ.get('DESTROY_WORLDS', ','.join(G.WORLDS)).split(',')  # a break test runs one
    for wi, w in enumerate(worlds):
        path = G.ZBD / w / 'gamez.zbd'
        want = emu.load(path)
        got = probe.probe_destroy_load(str(path).encode())
        if want != got:
            raise SystemExit(f'{w}: read_zbd {want} vs {got}')
        compare(f'{w} after the read', emu.dump(1), buf, probe.probe_destroy_dump(1, buf, len(buf)))
        rng = random.Random(seed * 100 + wi)
        worlds = [i for i in emu.listed() if emu.info(i)[0] == 2]
        ops = []
        # The run-time nodes first, then the random takedown.
        for _ in range(120):
            k = rng.random()
            if k < 0.75:
                parent = rng.choice(worlds) if rng.random() < 0.6 else rng.choice(
                    [i for i in emu.listed() if emu.info(i)[0] == 5] or worlds)
                ops.append((8, parent, rng.choice([-1, 0, 1, 2, 3, 4, 5]), rng.randrange(2)))
            else:
                ops.append((rng.choice((9, 10)), rng.choice(worlds), -1, 0))
        steps = 0
        history = []
        for step in range(520):
            op = ops[step] if step < len(ops) else pick_op(rng, emu, worlds)
            if step == 500:
                op = (12, -1, -1, 0)  # the shutdown pass on what is left
            elif step == 510:
                op = (13, -1, -1, 0)
            elif step > 510:
                break
            history.append(op)
            if HISTORY:
                HISTORY.write_text(repr((w, history)))
            want = emu.op(*op)
            got = probe.probe_destroy_op(*op)
            if want != got:
                raise SystemExit(f'{w} step {step} {NAMES[op[0]]}{op[1:]}: original {want}, port {got}')
            pool = step % 16 == 0 or op[0] in (12, 13)
            compare(f'{w} step {step} {NAMES[op[0]]}{op[1:]}', emu.dump(pool), buf,
                    probe.probe_destroy_dump(int(pool), buf, len(buf)))
            steps += 1
        live = struct.unpack_from('<i', emu.dump(0), 4)[0]
        print(f'{w}: {steps} calls identical (the node pool, the lists, the deferred deletes, the models); '
              f'{live} nodes left after the reset')
        total_ops += steps
        calls += 1
    print(f'ok: {total_ops} calls on {calls} worlds identical')


if __name__ == '__main__':
    try:
        main()
    except SystemExit as e:
        print(e)
        sys.stdout.flush()
        os._exit(1 if e.code else 0)
    sys.stdout.flush()
    os._exit(0)
