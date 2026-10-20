"""The fade state against the original (docs/game-loop.md, The fade state).

Each run starts a fade through one of its three entries (0x0048ee70 the Core
state's, 0x0048eea0 the push, 0x0048ef00 the replace) with random arguments,
then drives it as the app would: queued (0x0048ef60), entered (0x0048ef70),
updated frame by frame (0x0048ef90) with random frame times until it moves the
stack on, and left (0x0048f030). The emulator runs the original with every
call out stubbed (app_world.py), at the 24-bit x87 precision of play; the
rebuild's port (probe_fade.cpp) gets the original's answers replayed.
Compared: the call traces (the alpha each frame among them), the queued state
changes, the results and the fade's fields after every call.

    cmake --build build && .venv/bin/python tests/differential/test_fade.py [seeds]
    .venv/bin/python tests/differential/test_fade.py --break   # the test catches errors
"""
import ctypes
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from app_world import World, fbits  # noqa: E402
from calltrace import Replay, Trace, compare, load_probe, rnd  # noqa: E402

FADE = 0x609888
NEXT = (0x6099a0, 0x608bd0)
SECONDS = (2.0, 0.5, 0.1, 3.7, 1.0 / 3.0)
DTS = (0.0, 0.016, 0.033, 0.0333, 0.1, 0.25, 0.7)
FORMATS = ((5, 6, 5), (5, 5, 5))
# 0x0048ef00's callers pass these (the campaign's and the menus' fade outs).
STUBS = {0x579500: 'c', 0x55b1f0: 'c,d,s0,s1,s2', 0x517cc0: 'c'}


def set_format(e, r_bits, g_bits, b_bits):
    """0x005792c0's globals, as zvid::set_pixel_format sets them."""
    for a, v in ((0x802738, r_bits + g_bits + b_bits - 8), (0x80273c, g_bits + b_bits - 8),
                 (0x802740, 8 - b_bits), (0x802744, ((1 << r_bits) - 1) << (8 - r_bits)),
                 (0x802748, ((1 << g_bits) - 1) << (8 - g_bits)),
                 (0x80274c, ((1 << b_bits) - 1) << (8 - b_bits))):
        e.w32(a, v)


def make_world(seed, t):
    w = World(t, seed)
    e = w.emu
    del e.slots[0x48ee70]  # the Core state's call: here it runs
    for addr, spec in STUBS.items():
        e.stub(addr, spec=spec)
    # 0x00558540: the colour is CX only.
    e.stub(0x558540, spec='c,d,s0,s1',
           fn=lambda em, k, a, p: em._ret(em.t.handle(k, [a[0] & 0xffff] + a[1:]), p))
    # The object as its constructor 0x0048f1b0 leaves it (+0x2c and +0x34 untouched).
    e.w32(FADE, 0x5c6088)
    for off, v in ((0x8, 0), (0xc, 2), (0x10, 0), (0x1c, 0xffffffff), (0x24, fbits(2.0)),
                   (0x28, 0), (0x30, fbits(1.0))):
        e.w32(FADE + off, v)
    return w


def answers(seed, w):
    r = rnd(seed, 'answers')
    no_image = r.random() < 0.15

    def answer(key):
        def f(n, a):
            rr = rnd(seed, f'{key}#{n}')
            if key == '0x00579500':
                return 0 if no_image else w.emu.world_alloc(0x40)
            if key == '0x00579840':
                return rr.randrange(2)
            return rr.randrange(4)
        return f
    return answer


class Script(dict):
    def __init__(self, f):
        super().__init__()
        self.f = f

    def get(self, key, default=None):
        return self.f(key)

    def items(self):
        return []


def fields_emu(e):
    v = [e.r32(FADE + off) for off in range(0x8, 0x38, 4)]
    v[6] &= 0xffff  # +0x20 is a word
    return v + [0]


def fields_probe(probe):
    out = (ctypes.c_uint32 * 13)()
    probe.probe_fade_fields(out)
    return list(out)


def queue_probe(probe):
    q = (ctypes.c_uint32 * 48)()
    n = probe.probe_fade_queue(q, 16)
    return [(q[3 * i], q[3 * i + 1], q[3 * i + 2]) for i in range(n)]


def entry(r):
    """(address, ECX, EDX, stack dwords) for one of the three entries."""
    direction = r.choice([-1 & 0xffffffff, 1, 1, 0, 2, -1 & 0xffffffff])
    buffer = r.choice([0, 1, 2])
    seconds = fbits(r.choice(SECONDS))
    rgb = [r.choice([0, 0, 255, r.randrange(256)]) for _ in range(3)]
    arg = r.choice([0, 7, 4, 1, r.randrange(16)])
    which = r.choice([0x48ee70, 0x48eea0, 0x48ef00])
    if which == 0x48ee70:
        return which, arg, direction, [buffer, seconds] + rgb
    nxt = r.choice((0,) + NEXT)
    return which, nxt, arg, [direction, buffer, seconds] + rgb


def run(probe, seed, verbose=False):
    t = Trace()
    w = make_world(seed, t)
    e = w.emu
    t.script = Script(answers(seed, w))
    r = rnd(seed, 'run')
    fmt = r.choice(FORMATS)
    set_format(e, *fmt)
    probe.probe_fade_reset(*fmt)
    e.w32(0x72f348, r.randrange(2))
    addr, ecx, edx, stack = entry(r)
    dts = []
    # Some runs at one frame time, so the alpha lands exactly on 0 or 1.
    steady = r.choice(DTS[1:]) if r.random() < 0.4 else None
    frames = 0
    snap = e.snapshot()
    rep = Replay(t, e, snap)
    probe.probe_app_set_callback(rep.callback())
    name = f'seed {seed} ({addr:#x}, next {ecx:#x}, args {[hex(x) for x in stack]}, format {fmt})'

    def one(a, c, d, s):
        e.cpu.fpu_control(0x007f)  # the game's 24-bit precision in play
        eret = w.call(a, ecx=c, edx=d, stack=tuple(s))
        if a in (0x48ee70, 0x48eea0, 0x48ef00):  # the app's on_queued, inside the rebuild's entry
            e.cpu.fpu_control(0x007f)
            w.call(0x48ef60, ecx=FADE)
        ss = (ctypes.c_uint32 * 8)(*(list(s) + [0] * (8 - len(s))))
        pret = ctypes.c_uint32(0)
        if probe.probe_fade_call(a, c, d, ss, ctypes.byref(pret)):
            raise SystemExit(f'{a:#x}: not in the probe')
        if a in (0x48ef70, 0x48ef90) and ((eret or 0) & 0xff != 0) != (pret.value != 0):
            print(f'{name}: {a:#x} result original {eret} rebuild {pret.value}')
            return False
        return True

    ok = one(addr, ecx, edx, stack)
    ok = ok and one(0x48ef70, FADE, 0, [])
    while ok and frames < 400:
        dt = steady or (r.choice(DTS) if r.random() < 0.9 else dts[-1] if dts else 0.033)
        dts.append(dt)
        e.world_w32(0x72c52c, fbits(dt))  # seen by the rebuild's peek too
        before = len(w.queue)
        ok = one(0x48ef90, FADE, 0, [])
        frames += 1
        if not ok:
            break
        fa, fb = fields_emu(e), fields_probe(probe)
        if fa != fb:
            print(f'{name}: frame {frames}: fields differ\n  original {[hex(x) for x in fa]}\n'
                  f'  rebuild  {[hex(x) for x in fb]}')
            ok = False
        if len(w.queue) > before:
            break
    ok = ok and one(0x48f030, FADE, 0, [])
    ok = ok and compare(name, t.log, rep.log)
    if ok and queue_probe(probe) != w.queue:
        print(f'{name}: queued changes differ\n  original {w.queue}\n  rebuild  {queue_probe(probe)}')
        ok = False
    if ok:
        fa, fb = fields_emu(e), fields_probe(probe)
        if fa != fb:
            print(f'{name}: fields differ after leave\n  original {[hex(x) for x in fa]}\n'
                  f'  rebuild  {[hex(x) for x in fb]}')
            ok = False
    if verbose:
        print(name, frames, 'frames', w.queue)
    return ok, len(t.log), frames


def main():
    probe = load_probe()
    probe.probe_fade_call.restype = ctypes.c_int32
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    seeds = int(args[0]) if args else 300
    if '--break' in sys.argv:
        broken = 0
        for seed in range(seeds):
            ok, _, _ = run(probe, seed)
            broken += not ok
        print(f'{broken} of {seeds} runs caught the break')
        sys.stdout.flush(); os._exit(0 if broken else 1)
    calls = frames = 0
    for seed in range(seeds):
        ok, n, f = run(probe, seed, verbose='-v' in sys.argv)
        if not ok:
            sys.stdout.flush(); os._exit(1)
        calls += n
        frames += f
    print(f'{seeds} runs, {frames} frames, {calls} calls: the rebuilt fade state matches the original')
    sys.stdout.flush(); os._exit(0)


if __name__ == '__main__':
    main()
