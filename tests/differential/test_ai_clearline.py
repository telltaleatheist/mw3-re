"""The AI's clear line (docs/ai.md 17.7) against the original x86: 0x4d5980
with the zone test 0x53cc90, the node flag 0x4eb710 and the query options
0x4ed9c0 / 0x4eda00 run as they are.

    cmake --build build && .venv/bin/python tests/differential/test_ai_clearline.py

Two entities with a node (+0x04: its flags +0x24), a centroid (+0x14: its
data +0x38, stale flag 0x20 and position +0x84) and a zone list (+0x64);
the zone mode [0x5ea1d0]; the point given or not. Traced: the stale
centroids' refresh (0x4ed280 with the matrix calls 0x51e9d0, 0x51fde0,
0x51ea00 silent), the flags set (0x4eb6d0) and the world's segment query
(0x4eed90 on [0x608c84] +4: the two points, the first-hit mode and the
exclude flags it sees; its answer and the crossings it writes scripted).
The answer, the calls and the first-hit mode left 0 must match.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import Emu, Trace, load_probe  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402

U = ctypes.c_uint32
LINE = 0x4d5980
f2i = lambda x: struct.unpack('<I', struct.pack('<f', x))[0]  # noqa: E731
COVER = {0x4d5ad9: 'zones apart or done', 0x4d59e9: 'zones meet', 0x4d59ab: 'the other refreshed',
         0x4d5a42: 'own refreshed', 0x4d5ac9: 'blocked', 0x4d5ad0: 'clear', 0x4d59d6: 'a point given'}
REACHED = set()


def script(seed):
    def ans(salt, choices):
        return lambda n, args: random.Random(f'{seed}:{salt}:{n}').choice(choices)
    return {'0x004eed90': ans('seg', [0, 0, 1]), 'segment.count': ans('count', [0, 1, 2, 0])}


class Original:
    def __init__(self, seed):
        self.t = Trace(script(seed))
        self.e = e = Emu(self.t)
        for a in (0x51e9d0, 0x51fde0, 0x51ea00):
            e.stub(a, f'@{a:#010x}', '')
        e.stub(0x4ed280, '0x004ed280', 'c,d')
        e.stub(0x4eb6d0, '0x004eb6d0', 'c,d')
        e.stub(0x4eed90, '0x004eed90', 'c,d,s0,s1,s2,s3,s4,s5', 0x18, self.segment)
        self.world = e.world_alloc(0x10)
        self.wnode = e.world_alloc(0x10)
        e.w32(self.world + 4, self.wnode)
        e.w32(0x608c84, self.world)
        self.ents, self.nodes, self.cents, self.datas = [], [], [], []
        for _ in range(2):
            en, nd, ce, da = (e.world_alloc(0x100) for _ in range(4))
            e.w32(en + 4, nd)
            e.w32(en + 0x14, ce)
            e.w32(ce + 0x38, da)
            self.ents.append(en), self.nodes.append(nd), self.cents.append(ce), self.datas.append(da)
        self.pt = e.world_alloc(0x10)
        self.seen_first = []
        for a in COVER:
            e.uc.hook_add(UC_HOOK_CODE, lambda uc, ad, sz, u: REACHED.add(ad), begin=a, end=a)

    def segment(self, e, key, a, pop):
        ecx, edx = a[0], a[1]
        first, excl = e.r32(0x60b968), e.r32(0x60b974)
        r = self.t.handle(key, a[2:8] + [first, excl])
        e.w8(edx, self.t.handle('segment.count', []))
        e.w32(0x60b974, 0)  # the query clears the exclude flags
        self.seen_first.append(ecx == self.wnode)
        e._ret(r, pop)

    def run(self, w, at):
        e = self.e
        for k in range(2):
            x = w[9 * k:9 * k + 9]
            e.cpu.write(self.ents[k] + 0x64, struct.pack('<I', x[3]))
            e.w32(self.nodes[k] + 0x24, 0x10 if x[4] else 0x2)
            e.w8(self.datas[k], 0x20 if x[5] else 0)
            e.cpu.write(self.datas[k] + 0x84, struct.pack('<3I', *x[6:9]))
        e.w32(0x5ea1d0, w[18])
        if at is not None:
            e.cpu.write(self.pt, struct.pack('<3f', *at))
        e.w32(0x60b968, 0)
        r = e.call(LINE, ecx=self.ents[0], stack=(self.ents[1], self.pt if at is not None else 0)) & 0xff
        return r, e.r32(0x60b968)


def world(rng, o):
    def zones():
        n = rng.choice([0, 1, 1, 2, 3])
        ids = [rng.choice([1, 2, 3, 4, 0xff, 2]) for _ in range(3)]
        return n | ids[0] << 8 | ids[1] << 16 | ids[2] << 24
    w = []
    for k in range(2):
        w += [o.ents[k], o.nodes[k], o.cents[k], zones(), int(rng.random() < 0.7), int(rng.random() < 0.4)]
        w += [f2i(rng.uniform(-500, 500)), f2i(rng.uniform(0, 40)), f2i(rng.uniform(-500, 500))]
    w.append(rng.choice([1, 1, 1, 0]))
    at = [rng.uniform(-500, 500), rng.uniform(0, 40), rng.uniform(-500, 500)] if rng.random() < 0.5 else None
    return w, at


def main():
    probe = load_probe()
    probe.probe_ao_line.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]
    probe.probe_ao_new()
    cases = int(os.environ.get('LINE_CASES', '300'))
    stats = {'clear': 0, 'blocked': 0}
    for seed in range(cases):
        orig = Original(seed)
        port = Trace(script(seed))
        probe.probe_app_set_callback(port.callback())
        rng = random.Random(seed)
        for k in range(20):
            w, at = world(rng, orig)
            start = len(orig.t.log)
            a, first = orig.run(w, at)
            out = (ctypes.c_int32 * 1)()
            probe.probe_ao_line((U * len(w))(*w), (ctypes.c_float * 3)(*at) if at else None, out)
            if orig.t.log != port.log:
                i = next((j for j, (x, y) in enumerate(zip(orig.t.log, port.log)) if x != y),
                         min(len(orig.t.log), len(port.log)))
                raise SystemExit(f'seed {seed} case {k}: calls differ at {i}:\n  before {orig.t.log[start:i][-4:]}'
                                 f'\n  exe  {orig.t.log[i:i + 3]}\n  port {port.log[i:i + 3]}')
            if a != (out[0] & 0xff) or first != 0:
                raise SystemExit(f'seed {seed} case {k}: exe {a} (first-hit left {first}), port {out[0]}')
            stats['clear' if a else 'blocked'] += 1
        if not all(orig.seen_first):
            raise SystemExit(f'seed {seed}: the query not on the world node')
    probe.probe_ao_done()
    never = [COVER[a] for a in COVER if a not in REACHED]
    if never:
        raise SystemExit(f'branches never reached: {never}')
    print(f'ok: {cases} cases, {stats["clear"]} clear and {stats["blocked"]} blocked lines identical; '
          f'{len(COVER)} branches reached')


if __name__ == '__main__':
    code = 0
    try:
        main()
    except SystemExit as e:
        print(e)
        code = 1
    sys.stdout.flush()
    os._exit(code)
