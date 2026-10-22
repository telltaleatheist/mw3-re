"""The sounds' EXTRA records and the foot's wake (c1m1 item 1) against the
original x86 (docs/sound.md, The sounds' EXTRA records; movement.md):
zSound's load callback 0x4e6b00 (with 0x4e6aa0 / 0x4e6a60 / 0x49d1c0 and
the reader's 0x5569e0) on made-up EXTRA lists (MINDELAY and SUBTITLE
present or not, nested, among other keys); the play 0x4e6cd0 over random
game times, delays and the subtitle switch, zSound's 0x571cc0 and the
message panel's 0x40fe50 stood in on both sides; the wake 0x45faf0 /
0x45fb50 (0x45fa70's levels) over runs of throttles and stops, zEffect's
0x508da0 / 0x506ca0 and zClass's 0x4eb170 stood in. The x87 at 24 bits.

    cmake --build build && .venv/bin/python tests/differential/test_sound_extra.py

BREAK=<n> breaks the port's inputs in a known way (see main); each must fail.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import FTOL, IAT_FTOL, ROOT, Original  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
P = ctypes.c_void_p
probe.probe_sx_load.argtypes = [P, P, P, ctypes.c_int]
probe.probe_sx_play.argtypes = [ctypes.c_float, ctypes.c_float, ctypes.c_int, ctypes.c_int, ctypes.c_float,
                                ctypes.c_float, ctypes.c_int, ctypes.c_float, P, ctypes.c_int]
probe.probe_sx_say.argtypes = [ctypes.c_float, ctypes.c_float, ctypes.c_int, ctypes.c_int, ctypes.c_float,
                               ctypes.c_float, P, ctypes.c_int]
probe.probe_wake.argtypes = [P, ctypes.c_int, P, ctypes.c_int]
CAP = 1 << 16
BUF = ctypes.create_string_buffer(CAP)
IAT_NEW, IAT_STRDUP = 0x5bb4bc, 0x5bb670
TIME, SUBTITLES, LIST, LIST_COUNT = 0x60991c, 0x60b878, 0x60b814, 0x60b818
STOP = 0x7fffffff


def fb(v):
    return struct.unpack('<I', struct.pack('<f', v))[0]


class Emulated:
    def __init__(self):
        cpu = self.cpu = Original()
        cpu.import_code(IAT_FTOL, FTOL)
        cpu.fpu_control(0x007f)
        cpu.stub_import(IAT_NEW, lambda c: self.zalloc(c.arg(0)))
        cpu.stub_import(IAT_STRDUP, self.strdup)
        cpu.stub_function(0x575fb0, lambda c: c.ecx)  # zLoc: the name itself, as the port's with no table
        cpu.stub_function(0x571cc0, self.play, 0x10)
        cpu.stub_function(0x40fe50, self.subtitle)
        cpu.stub_function(0x56e3d0, self.play2d, 0x4)
        cpu.stub_function(0x508da0, self.start, 0xc)
        cpu.stub_function(0x506ca0, self.stop)
        cpu.stub_function(0x4eb170, self.set_active)
        head = self.zalloc(12)
        cpu.write(head, struct.pack('<II', head, head))  # the record list's head: an empty ring
        cpu.u32(LIST, head)
        cpu.u32(LIST_COUNT, 0)
        self.out = bytearray()

    def zalloc(self, n):
        p = self.cpu.alloc(max(n, 4))
        self.cpu.write(p, bytes(max(n, 4)))
        return p

    def cstr(self, a):
        out = bytearray()
        while True:
            b = self.cpu.read(a + len(out), 1)[0]
            if not b:
                return bytes(out)
            out.append(b)

    def strdup(self, c):
        s = self.cstr(c.arg(0))
        p = self.zalloc(len(s) + 1)
        self.cpu.write(p, s + b'\0')
        return p

    def log(self, *v):
        for x in v:
            self.out += struct.pack('<I', x & 0xffffffff)

    def logf(self, a):
        self.out += self.cpu.read(a, 4)

    def logs(self, a):
        if not a:
            return self.log(0xffffffff)
        s = self.cstr(a)
        self.log(len(s))
        self.out += s

    def play(self, c):
        pos = c.edx
        self.log(0x51, 1 if c.ecx else 0, 1 if pos else 0)
        if pos:
            self.out += self.cpu.read(pos, 12)
        self.log(c.arg(0), 1 if c.arg(1) else 0, c.arg(2), c.arg(3))
        return 0x7777

    def play2d(self, c):
        self.log(0x53, 1 if c.ecx else 0, c.arg(0))
        return 0x7777

    def subtitle(self, c):
        self.log(0x52)
        self.logs(c.arg(0))
        return 0

    def start(self, c):
        self.n += 1
        self.log(0x61, c.ecx, 1 if c.arg(0) else 0)
        return 0x100 + self.n

    def stop(self, c):
        self.log(0x62, c.ecx)
        return 0

    def set_active(self, c):
        self.log(0x63, c.edx)
        return 0

    # --- the cases ---
    def build(self, item, strings):
        kind = item[0]
        if kind == 1:
            return 1, item[1]
        if kind == 2:
            return 2, fb(item[1])
        if kind == 3:
            p = self.zalloc(len(strings[item[1]]) + 1)
            self.cpu.write(p, strings[item[1]] + b'\0')
            return 3, p
        items = item[1]
        base = self.zalloc(8 * (len(items) + 1))
        self.cpu.write(base, struct.pack('<Ii', 1, len(items) + 1))
        for i, it in enumerate(items):
            self.cpu.write(base + 8 * (i + 1), struct.pack('<II', *self.build(it, strings)))
        return 4, base

    def load(self, tree, strings):
        self.out = bytearray()
        node = self.zalloc(8)
        self.cpu.write(node, struct.pack('<II', *self.build(tree, strings)))
        self.cpu.call(0x4e6b00, ecx=0x1234, edx=node)
        r = self.cpu.uc.reg_read(UC_X86_REG_EAX)
        self.log(1 if r else 0)
        if r:
            sub = struct.unpack('<I', self.cpu.read(r, 4))[0]
            self.logs(sub)
            self.logf(r + 4), self.logf(r + 8)
        return bytes(self.out)

    def play_case(self, nxt, delay, subtitle, on, now, volume, placed, fade):
        self.out = bytearray()
        rec = self.zalloc(12)
        sub = 0
        if subtitle:
            sub = self.zalloc(4)
            self.cpu.write(sub, b'SUB\0')
        self.cpu.write(rec, struct.pack('<Iff', sub, delay, nxt))
        self.cpu.write(SUBTITLES, bytes([1 if on else 0]))
        self.cpu.write(TIME, struct.pack('<f', now))
        pos = 0
        if placed:
            pos = self.zalloc(12)
            self.cpu.write(pos, struct.pack('<3f', 1.5, -2.0, 30.25))
        self.cpu.call(0x4e6cd0, ecx=rec, stack_args=(0x9999, fb(volume), pos, 0, fb(fade)))
        r = self.cpu.uc.reg_read(UC_X86_REG_EAX)
        self.log(1 if r else 0)
        self.logf(rec + 8)
        return bytes(self.out)

    def say_case(self, nxt, delay, subtitle, on, now, volume):
        self.out = bytearray()
        rec = self.zalloc(12)
        sub = 0
        if subtitle:
            sub = self.zalloc(4)
            self.cpu.write(sub, b'SUB\0')
        self.cpu.write(rec, struct.pack('<Iff', sub, delay, nxt))
        self.cpu.write(SUBTITLES, bytes([1 if on else 0]))
        self.cpu.write(TIME, struct.pack('<f', now))
        self.cpu.call(0x4e6c60, ecx=rec, stack_args=(0x9999, fb(volume)))
        r = self.cpu.uc.reg_read(UC_X86_REG_EAX)
        self.log(1 if r else 0)
        self.logf(rec + 8)
        return bytes(self.out)

    def wake(self, ops):
        self.out = bytearray()
        self.n = 0
        w = self.zalloc(0x1c)
        self.cpu.write(w, struct.pack('<7I', 0x5c2838, 0x4444, 0x10, 0x11, 0x12, 0, 0xffffffff))
        for op in ops:
            if op == STOP:
                self.cpu.call(0x45fb50, ecx=w)
            else:
                self.cpu.call(0x45faf0, ecx=w, stack_args=(op,))
            level, playing = struct.unpack('<iI', self.cpu.read(w + 0x18, 4) + self.cpu.read(w + 0x14, 4))
            self.log(level, 1 if playing else 0)
        return bytes(self.out)


def tokens(tree):
    out = []

    def walk(it):
        out.append(it[0])
        if it[0] == 4:
            out.append(len(it[1]))
            for x in it[1]:
                walk(x)
        elif it[0] == 2:
            out.append(fb(it[1]))
        else:
            out.append(it[1])
    walk(tree)
    return out


def extra_tree(rng):
    """An EXTRA-like list: other keys, MINDELAY (x) and SUBTITLE ("name") each
    maybe, sometimes inside a nested list."""
    names = [b'VOICE', b'QUEUE', b'DYNAMIC_WEIGHTS', b'MSG_CHIT_MAIN', b'MSG_MCB_POS_WAIT', b'MINDELAY', b'SUBTITLE',
             b'0020.wav']
    items = []
    if rng.random() < 0.6:
        items += [(3, 2), (2, rng.choice((0.5, 1.0)))]
    if rng.random() < 0.5:
        items += [(3, 7), (3, 0), (3, 1), (4, [])]
    if rng.random() < 0.5:
        items.append((3, 5))
        items.append((4, [(2, rng.choice((0.0, 2.5, 30.0, -1.0)))]))
    if rng.random() < 0.5:
        items.append((3, 6))
        items.append((4, [(3, rng.choice((3, 4)))]))
    if rng.random() < 0.2:
        items = [(4, items)]
    return (4, items), names


def main():
    rng = random.Random(int(os.environ.get('SEED', '1')))
    brk = int(os.environ.get('BREAK', '0'))
    emu = Emulated()
    runs = fails = 0
    kinds = {}

    def check(kind, mine, theirs):
        nonlocal runs, fails
        runs += 1
        kinds[kind] = kinds.get(kind, 0) + 1
        if mine != theirs:
            fails += 1
            if fails <= 3:
                print(f'FAIL {kind}: port {mine.hex()}\n     original {theirs.hex()}')

    for _ in range(300):
        tree, names = extra_tree(rng)
        mine_tree = tree
        if brk == 1 and tree[1] and tree[1][-1][0] == 4 and tree[1][-1][1] and tree[1][-1][1][0][0] == 2:
            mine_tree = (4, tree[1][:-1] + [(4, [(2, tree[1][-1][1][0][1] + 1.0)])])
        tok = tokens(mine_tree)
        tb = (ctypes.c_int32 * len(tok))(*tok)
        sb = (ctypes.c_char_p * len(names))(*names)
        n = probe.probe_sx_load(ctypes.cast(tb, P), ctypes.cast(sb, P), BUF, CAP)
        check('load', BUF.raw[:n], emu.load(tree, names))

    for _ in range(600):
        now = rng.choice((0.0, rng.uniform(0, 100), 10.0))
        nxt = rng.choice((0.0, now, now + rng.uniform(-5, 5), float('nan')))
        delay = rng.choice((0.0, rng.uniform(0, 30), -1.0, float('nan')))
        sub, on, placed = rng.randint(0, 1), rng.randint(0, 1), rng.randint(0, 1)
        volume, fade = rng.choice((1.0, 0.3, 0.55)), rng.choice((-1.0, 0.0))
        mine_now = now + (1.0 if brk == 2 else 0.0)
        n = probe.probe_sx_play(nxt, delay, sub, on, mine_now, volume, placed, fade, BUF, CAP)
        check('play', BUF.raw[:n], emu.play_case(nxt, delay, sub, on, now, volume, placed, fade))

    for _ in range(600):  # the radio's lines (0x4e6c60)
        now = rng.choice((0.0, rng.uniform(0, 100), 10.0))
        nxt = rng.choice((0.0, now, now + rng.uniform(-5, 5), float('nan')))
        delay = rng.choice((0.0, rng.uniform(0, 30), -1.0, float('nan')))
        sub, on = rng.randint(0, 1), rng.randint(0, 1)
        volume = rng.choice((1.0, 0.3))
        n = probe.probe_sx_say(nxt, delay, sub, on, now + (1.0 if brk == 4 else 0.0), volume, BUF, CAP)
        check('say', BUF.raw[:n], emu.say_case(nxt, delay, sub, on, now, volume))

    for _ in range(300):
        ops = []
        for _ in range(rng.randint(1, 12)):
            if rng.random() < 0.25:
                ops.append(STOP)
            else:
                ops.append(fb(rng.choice((0.0, 0.1, 0.2, 0.19999, 0.35, 0.5, 0.49999, 0.8, 1.0, -0.6, -0.3,
                                          float('nan'), rng.uniform(-1, 1)))))
        mine_ops = [o if not (brk == 3 and o == fb(0.5)) else fb(0.49) for o in ops]
        ob = (ctypes.c_int32 * len(ops))(*[struct.unpack('<i', struct.pack('<I', o))[0] for o in mine_ops])
        n = probe.probe_wake(ctypes.cast(ob, P), len(ops), BUF, CAP)
        check('wake', BUF.raw[:n], emu.wake(ops))

    if fails:
        print(f'{fails} of {runs} runs differ')
        sys.stdout.flush(); os._exit(1)
    print(f'ok: {runs} runs identical ({", ".join(f"{k} {v}" for k, v in sorted(kinds.items()))})')
    sys.stdout.flush(); os._exit(0)


if __name__ == '__main__':
    main()
