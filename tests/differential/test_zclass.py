"""The ported zClass node pool (docs/zclass-world.md, section 1) against the
original x86: random runs of node_alloc (0x4eacd0) and node_slot_free
(0x4f8de0) with small pool blocks and list-item chunks, so both grow often.

    cmake --build build && .venv/bin/python tests/differential/test_zclass.py

Each run compares what every call returns, every slot (all 0xd0 bytes and the
link word), the free list, and list 6 (every node) with each item's place in
the item pool.
"""
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
probe.probe_node_ops.argtypes = [ctypes.POINTER(ctypes.c_int32), ctypes.c_int, ctypes.c_int, ctypes.c_int,
                                 ctypes.POINTER(ctypes.c_int32), ctypes.c_void_p, ctypes.c_int]
ALLOC, SLOT_FREE = 0x4eacd0, 0x4f8de0
BLOCK_SIZE, FREE, BLOCKS, NBLOCKS, TOTAL, LIVE = 0x5ea830, 0x5ea834, 0x60bf38, 0x60bf30, 0x60bf34, 0x60bf3c
CHUNK_SIZE, ITEM_CHUNKS, ITEM_NCHUNKS, ITEM_FREE = 0x5ea750, 0x60bf18, 0x60bf14, 0x60bf0c
LIST6_HEAD, LIST6_TAIL = 0x60be4c, 0x60be50
IAT = {'malloc': 0x5bb664, 'calloc': 0x5bb60c, 'realloc': 0x5bb610, 'free': 0x5bb65c, 'sprintf': 0x5bb658}
BIG = 0x40000000


class Emulated:
    def __init__(self):
        cpu = self.cpu = Original()
        cpu.uc.mem_map(BIG, 0x2000000)
        self.sizes = {}
        cpu.stub_import(IAT['malloc'], lambda c: self.alloc(c.arg(0)))
        cpu.stub_import(IAT['calloc'], lambda c: self.alloc(c.arg(0) * c.arg(1), zero=True))
        cpu.stub_import(IAT['realloc'], self.realloc)
        cpu.stub_import(IAT['free'], lambda c: 0)
        cpu.stub_import(IAT['sprintf'], self.sprintf)

    def alloc(self, n, zero=False):
        a = self.next
        self.next += (max(n, 1) + 0xf) & ~0xf
        self.sizes[a] = n
        self.cpu.write(a, b'\xcd' * n if not zero else bytes(n))
        return a

    def realloc(self, c):
        old, n = c.arg(0), c.arg(1)
        a = self.alloc(n)
        if old:
            c.write(a, c.read(old, min(self.sizes.get(old, 0), n)))
        return a

    def sprintf(self, c):
        dst, fmt, s = c.arg(0), self.cstr(c.arg(1)), self.cstr(c.arg(2))
        assert fmt == b'%s'
        c.write(dst, s + b'\0')
        return len(s)

    def cstr(self, p):
        out = bytearray()
        while (b := self.cpu.read(p + len(out), 1)) != b'\0':
            out += b
        return bytes(out)

    def u(self, a):
        return struct.unpack('<I', self.cpu.read(a, 4))[0]

    def i(self, a):
        return struct.unpack('<i', self.cpu.read(a, 4))[0]

    def run(self, ops, block_size, chunk_size):
        cpu, u = self.cpu, self.u
        self.next = BIG
        for a in (BLOCKS, NBLOCKS, TOTAL, LIVE, ITEM_CHUNKS, ITEM_NCHUNKS, ITEM_FREE):
            cpu.u32(a, 0)
        cpu.write(0x60be4c, bytes(0x60bf0c - 0x60be4c))
        cpu.u32(FREE, 0xffffffff)
        cpu.u32(BLOCK_SIZE, block_size)
        cpu.u32(CHUNK_SIZE, chunk_size)
        results, plan = [], []
        rng = random.Random(len(ops))
        live = []
        for op in ops:
            if op == 'alloc' or not live:
                cpu.call(ALLOC)
                idx = self.index(cpu.uc.reg_read(UC_X86_REG_EAX))
                live.append(idx)
                plan.append(-1)
                results.append(idx)
            else:
                k = live.pop(rng.randrange(len(live)))
                cpu.call(SLOT_FREE, ecx=self.slot(k))
                plan.append(k)
                results.append(self.i(FREE))
        out = [struct.pack('<4i', self.i(TOTAL), self.i(LIVE), self.i(FREE), self.i(NBLOCKS))]
        for k in range(self.i(TOTAL)):
            out.append(cpu.read(self.slot(k), 0xd4))
        items, p = [], u(LIST6_HEAD)
        while p:
            items.append(p)
            p = u(p + 8)
        out.append(struct.pack('<i', len(items)))
        for p in items:
            out.append(struct.pack('<i', self.index(u(p))) + self.item(p))
        out.append(self.item(u(LIST6_TAIL)))
        free, p = 0, u(ITEM_FREE)
        while p:
            free += 1
            p = u(p + 8)
        out.append(struct.pack('<i', free) + self.item(u(ITEM_FREE)))
        return plan, results, b''.join(out)

    def blocks(self):
        return [self.u(self.u(BLOCKS) + 4 * b) for b in range(self.i(NBLOCKS))]

    def slot(self, k):
        bs = self.i(BLOCK_SIZE)
        return self.blocks()[k // bs] + (k % bs) * 0xd4

    def index(self, ptr):
        bs = self.i(BLOCK_SIZE)
        for b, base in enumerate(self.blocks()):
            if base <= ptr < base + bs * 0xd4:
                return b * bs + (ptr - base) // 0xd4
        return -1 if ptr == 0 else -2

    def item(self, p):
        cs = self.i(CHUNK_SIZE)
        for c in range(self.i(ITEM_NCHUNKS)):
            base = self.u(self.u(ITEM_CHUNKS) + 4 * c)
            if p and base <= p < base + cs * 0x10:
                return struct.pack('<ii', c, (p - base) // 0x10)
        return struct.pack('<ii', -1, -1)


def main():
    emu = Emulated()
    buf = ctypes.create_string_buffer(8 << 20)
    rng = random.Random(1)
    runs = 0
    for block_size, chunk_size in ((3, 3), (2, 5), (7, 2), (2500, 2500)):
        for length in (1, 5, 20, 100, 300):
            ops = [rng.choice(['alloc', 'alloc', 'free']) for _ in range(length)]
            plan, want_res, want = emu.run(ops, block_size, chunk_size)
            arr = (ctypes.c_int32 * len(plan))(*plan)
            res = (ctypes.c_int32 * len(plan))()
            n = probe.probe_node_ops(arr, len(plan), block_size, chunk_size, res, buf, len(buf))
            got = buf.raw[:max(n, 0)]
            if list(res) != want_res or got != want:
                at = next((i for i in range(min(n, len(want))) if got[i] != want[i]), None)
                raise SystemExit(f'blocks {block_size} chunks {chunk_size} ops {length}: results '
                                 f'{list(res) == want_res}, dump {len(want)} vs {n}, first difference at {at}')
            runs += 1
    print(f'ok: {runs} runs of node alloc/free identical (slots, free list, list 6, item pool)')


if __name__ == '__main__':
    main()
