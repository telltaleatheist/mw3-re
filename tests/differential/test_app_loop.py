"""zGameApp's main loop and state stack against the original (docs/game-loop.md).

Runs start() 0x58fc70 and Run() 0x58fd70 with the message pump, the clock, the
network and every state method stubbed, the states queueing Push/Replace/Pop
through the original's own queue functions (0x590250, 0x590400, 0x5905a0) and
the pump delivering window activations (0x590c00) and messages (0x590740).
The call traces and the final stack must match the rebuild's.

    cmake --build build && .venv/bin/python tests/differential/test_app_loop.py
"""
import ctypes
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import (Emu, Trace, call_abs, compare, load_probe, mov_eax, mov_ecx,  # noqa: E402
                       push_imm, rnd)

PEEK, POST_QUIT, MSG_WAIT, SET_PRIORITY = 0x5bb6b8, 0x5bb6c4, 0x5bb6f0, 0x5bb0f8
REPLACE, PUSH, POP = 0x590250, 0x590400, 0x5905a0
ON_ACTIVATE, MESSAGE = 0x590c00, 0x590740


def make_script(seed, count, steps):
    """Answers that depend only on the key and its occurrence (calltrace.Trace)."""
    world = {'quit': False, 'depth': -1, 'pending': 0}
    gens = {}

    def g(key):
        if key not in gens:
            gens[key] = rnd(seed, key)
        return gens[key]

    def peek(n, a):
        if world['quit'] or n > steps:
            return 1
        return 1 if g('peek').random() < 0.3 else 0

    def pump(n, a):
        if world['quit'] or n > steps:
            return 0
        r = g('pump')
        x = r.random()
        if x < 0.06:  # window (de)activation
            return 1 | (1 << 8) | ((1 if r.random() < 0.6 else 0) << 12)
        if x < 0.10:  # a forwarded message
            return 1 | (2 << 8) | (r.randrange(4) << 12) | (r.randrange(8) << 20)
        return 1

    def post_quit(n, a):
        world['quit'] = True
        return 0

    def depth(n, a):
        world['depth'], world['pending'] = a[0] if a[0] < 0x80000000 else -1, a[1]
        return 0

    def update(name):
        def f(n, a):
            r = g(name + 'u')
            x = r.random()
            if x < 0.01:
                return 1  # quit
            target, arg = r.randrange(count), r.randrange(4096)
            if x < 0.13:
                return (2 << 8) | (target << 12) | (arg << 20)
            if x < 0.25:
                return (3 << 8) | (target << 12) | (arg << 20)
            if x < 0.37 and world['depth'] >= 1 and world['pending'] == 0:
                return (1 << 8) | (arg << 20)
            return 0
        return f

    def enter(name):
        return lambda n, a: 1 if g(name + 'e').random() < 0.85 else 0

    script = {'PeekMessageA': peek, 'app.vt+0x64': pump, 'PostQuitMessage': post_quit,
              '@depth': depth, 'app.vt+0xac': 1, 'app.vt+0x70': 0}
    for i in range(count):
        s = f'state{i}'
        script[s + '.vt+0xc'] = enter(s)
        script[s + '.vt+0x10'] = update(s)
        script[s + '.vt+0x24'] = (lambda nm: lambda n, a: g(nm + 'm').randrange(3))(s)
    return script


def original(trace, count, first):
    emu = Emu(trace)
    emu.heap_stubs()
    app = emu.alloc(0x200)
    frame = emu.alloc(0x100)

    def pump(e, key, a, pop):
        r = e.t.handle(key, a)
        ev, value = (r >> 8) & 0xf, r >> 12
        code = b''
        here = e.alloc(64)
        if ev == 1:
            code = push_imm(0) + push_imm(0) + push_imm(value) + mov_ecx(frame)
            code += call_abs(here + len(code), ON_ACTIVATE)
        elif ev == 2:
            code = push_imm(value >> 8) + push_imm(value & 0xff) + mov_ecx(app)
            code += call_abs(here + len(code), MESSAGE)
        if not code:
            e._ret(r & 0xff, 0)
            return
        code += mov_eax(r & 0xff) + b'\xc3'
        e.cpu.write(here, code)
        e._ret(0, 0, then=here)

    vt = emu.alloc(0x100)
    emu.w32(app, vt)
    emu.w32(vt + 0x64, emu.new_stub('app.vt+0x64', '', 0, pump))
    emu.w32(vt + 0x70, emu.new_stub('app.vt+0x70', '', 0))
    emu.w32(vt + 0xa4, 0x58faa0)
    emu.w32(vt + 0xa8, 0x58fa80)
    emu.w32(vt + 0xac, emu.new_stub('app.vt+0xac', 's0', 4))
    emu.w32(vt + 0xb0, emu.new_stub('app.vt+0xb0', '', 0))
    emu.w32(vt + 0xb8, emu.new_stub('app.vt+0xb8', 's0', 4))
    emu.w32(app + 0xc8, 0xffffffff)
    emu.w32(app + 0x118, 100)
    emu.w32(app + 0x2c, 0x77)  # the thread handle
    emu.w32(frame + 0x20, 0x1234)
    emu.w32(frame + 0xc0, app)

    states = []

    def update(e, key, a, pop):
        e.t.handle('@depth', [e.r32(app + 0xc8), e.r32(app + 0x148)])
        r = e.t.handle(key, a)
        typ, target, arg = (r >> 8) & 0xf, (r >> 12) & 0xff, r >> 20
        if typ == 0:
            e._ret(r & 0xff, 0)
            return
        here = e.alloc(64)
        if typ == 1:
            code = push_imm(arg) + mov_ecx(app)
            code += call_abs(here + len(code), POP)
        else:
            code = push_imm(arg) + push_imm(states[target]) + mov_ecx(app)
            code += call_abs(here + len(code), PUSH if typ == 2 else REPLACE)
        code += mov_eax(r & 0xff) + b'\xc3'
        e.cpu.write(here, code)
        e._ret(0, 0, then=here)

    for i in range(count):
        states.append(emu.object(f'state{i}', slots={
            0x04: ('s0', 4), 0x08: ('', 0), 0x0c: ('', 0), 0x10: ('', 0, update),
            0x14: ('', 0), 0x18: ('', 0), 0x1c: ('s0', 4), 0x20: ('s0', 4),
            0x24: ('s0,s1', 8)}))
    emu.w32(app + 0xc4, states[first])

    emu.import_stub(PEEK, 'PeekMessageA', '', 20)
    emu.import_stub(POST_QUIT, 'PostQuitMessage', 's0', 4)
    emu.import_stub(MSG_WAIT, 'MsgWaitForMultipleObjects', 's3', 20)
    emu.import_stub(SET_PRIORITY, 'SetThreadPriority', 's1', 8)
    emu.stub(0x575a00)
    emu.stub(0x588930, spec='c,dl')
    emu.stub(0x572bf0, spec='c,d')
    emu.stub(0x51b050)
    emu.stub(0x51b080)
    emu.stub(0x579f20)
    emu.stub(0x49bfd0, spec='s0,s2')
    emu.stub(0x47fff0, key='frame', fn=lambda e, k, a, p: e._ret(frame, 0))
    emu.stub(0x5ac0f2, key='CFrameWnd::OnActivate', pop=12)
    trace.silent |= {'frame', 'CFrameWnd::OnActivate', '@depth'}

    ret = -1
    if emu.call(0x58fc70, ecx=app):
        ret = emu.call(0x58fd70, ecx=app)
    depth = emu.r32(app + 0xc8)
    out = [depth - (1 << 32) if depth >= 1 << 31 else depth, 1 if emu.r32(app + 0xd0) else 0,
           emu.r32(app + 0x148)]
    for i in range(16):
        p = emu.r32(app + 0xd8 + 4 * i)
        out.append(states.index(p) if p in states else -1)
    return ret, out


def rebuilt(probe, trace, count, first):
    trace.silent |= {'@depth'}
    probe.probe_app_set_callback(trace.callback())
    out = (ctypes.c_int32 * 19)()
    ret = probe.probe_app_loop(count, first, out)
    return ret, list(out)


def main():
    probe = load_probe()
    runs = calls = 0
    for seed in range(60):
        count = 2 + seed % 6
        first = seed % count
        steps = 40 + (seed * 37) % 260
        ta = Trace(make_script(seed, count, steps))
        a = original(ta, count, first)
        tb = Trace(make_script(seed, count, steps))
        b = rebuilt(probe, tb, count, first)
        if not compare(f'seed {seed}', ta.log, tb.log) or a != b:
            if a != b:
                print(f'seed {seed}: end state differs\n  original {a}\n  rebuild  {b}')
            raise SystemExit(1)
        runs += 1
        calls += len(ta.log)
    print(f'{runs} runs, {calls} calls: the rebuilt main loop and state stack match the original')


if __name__ == '__main__':
    main()
