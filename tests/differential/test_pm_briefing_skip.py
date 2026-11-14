"""The briefing script's skip, against PM's exe: the frame 0x405f20 with Space or S held runs
every step left (the one playing first) once with 60 s, then stops the voice and the script;
MECHLAB and SALVAGE (0x404780, ids 2 and 4) do the same before leaving. MW3's (0x406440,
0x404c30) only stop. A script of 0..6 recording steps, the one playing anywhere, playing or
not, each step done or not, the voice set or not; compared: every step called (which, with what
time), the input cleared, the voice stopped, the button left, then the script's state (playing,
the step at), the step reached and the voice. The screen around them is stand-ins.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_briefing_skip.py [runs]
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_pm_zar  # noqa: E402

probe = test_pm_zar.load_pm_probe()
probe.probe_pm_briefing_skip.argtypes = [ctypes.c_int32, ctypes.c_char_p, ctypes.c_int32, ctypes.c_int32,
                                         ctypes.c_int32, ctypes.c_uint32, ctypes.c_int32, ctypes.c_float,
                                         ctypes.c_int32, ctypes.POINTER(ctypes.c_int32), ctypes.c_int32]
probe.probe_pm_briefing_skip.restype = ctypes.c_int32
FRAME, BUTTON = 0x405f20, 0x404780
REACHED, VOICE = 0x602488, 0x618748
HEAD, AT, PLAYING, LOADED, TARGET, BAR_VALUE, BAR = 0x113bc, 0x113c4, 0x113c8, 0x113b4, 0x113b0, 0x11518, 0x113cc
LAB, SALVAGE = 0x11d1c, 0x12014
SPACE, S = 0x39, 0x1f


class Exe:
    def __init__(self):
        self.e = e = test_pm_zar.Emulated()
        self.log = []
        self.keys = set()
        self.steps = {}
        put = self.log.extend
        e.native_stub(0x493c80, lambda c: 0)                    # the loader's poll
        e.native_stub(0x407cc0, lambda c: 0, 4)                 # set_loaded
        e.native_stub(0x5bfde0, lambda c: 0, 4)                 # Window::update
        e.native_stub(0x534320, lambda c: 3 if c.arg(0) in self.keys else 0)
        e.native_stub(0x533b90, lambda c: put((2, 0, 0)))       # the input cleared
        e.native_stub(0x58fe70, lambda c: put((3, c.arg(0), 0)))  # the voice stopped
        e.native_stub(0x407900, lambda c: 0)
        e.native_stub(0x449dd0, lambda c: 0)                    # the mech lab opened
        e.native_stub(0x47f790, lambda c: 0)                    # the salvage opened
        self.step_code = e.cpu.code(b'\xc2\x04\x00')
        e.native_stub(self.step_code, lambda c: put((1, self.steps[c.ecx][0], c.arg(0))) or self.steps[c.ecx][1], 4)
        self.leave = {}
        for off, id_ in ((LAB, 2), (SALVAGE, 4)):
            code = e.cpu.code(b'\xc3')
            e.native_stub(code, lambda c, i=id_: put((4, i, 0)))
            self.leave[off] = code
        self.bar_code = e.cpu.code(b'\xc2\x04\x00')
        e.native_stub(self.bar_code, lambda c: 0, 4)
        self.screen = e.alloc(0x13000)  # one screen, its fields set each run (the heap is small)
        self.step_vt = e.alloc(16)
        self.w(self.step_vt, self.step_code)
        bar_vt = e.alloc(0x100)
        self.w(bar_vt + 0x8c, self.bar_code)
        self.w(self.screen + BAR, bar_vt)
        for off, code in self.leave.items():
            vt = e.alloc(0x100)
            self.w(vt + 0x40, code)
            self.w(self.screen + off, vt)
        self.head = e.alloc(12)
        self.nodes = [e.alloc(12) for _ in range(6)]
        self.objs = [e.alloc(8) for _ in range(6)]

    def w(self, a, v):
        self.e.cpu._uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def run(self, n, dones, at, playing, key, voice, reached, dt, mode):
        e, w = self.e, self.w
        self.log.clear()
        self.steps.clear()
        s, head, nodes = self.screen, self.head, self.nodes[:n]
        ring = [head] + nodes
        for i, node in enumerate(nodes):
            obj = self.objs[i]
            w(obj, self.step_vt)
            self.steps[obj] = (i, dones[i])
            w(node + 8, obj)
        for i, node in enumerate(ring):
            w(node, ring[(i + 1) % len(ring)])
            w(node + 4, ring[i - 1])
        w(s + HEAD, head)
        w(s + AT, nodes[at] if at < n else head)
        e.cpu._uc.mem_write(s + PLAYING, bytes([playing]))
        w(VOICE, voice)
        w(REACHED, reached)
        self.keys = {SPACE} if key == 1 else {S} if key == 2 else set()
        if mode == 0:
            e.call(FRAME, ecx=s, args=[struct.unpack('<I', struct.pack('<f', dt))[0]])
        else:
            e.call(BUTTON, ecx=s, args=[mode, 0, 0])
        at_node = e.r32(s + AT)
        out = list(self.log)
        out += [9, e.cpu.read(s + PLAYING, 1)[0], ring.index(at_node) - 1 if at_node != head else n]
        out += [8, e.r32(REACHED), e.r32(VOICE)]
        return [v & 0xffffffff for v in out]


def port(n, dones, at, playing, key, voice, reached, dt, mode):
    buf = (ctypes.c_int32 * 256)()
    m = probe.probe_pm_briefing_skip(n, bytes(dones) or b'\0', at, playing, 1 if key else 0, voice, reached, dt,
                                     mode, buf, 256)
    return [v & 0xffffffff for v in buf[:m]]


def main():
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 400
    rnd = random.Random(7)
    exe = Exe()
    bad = 0
    for r in range(runs):
        n = rnd.randint(0, 6)
        at = rnd.randint(0, n)
        dones = [rnd.randint(0, 1) for _ in range(n)]
        playing = rnd.randint(0, 1)
        key = rnd.choice((0, 1, 2))
        mode = rnd.choice((0, 0, 2, 4))
        voice = rnd.choice((0, 0x5150 + r))
        reached = rnd.choice((-1, 0, 3, 999))
        dt = rnd.choice((0.0, 1 / 30, 0.25))
        dt = struct.unpack('<f', struct.pack('<f', dt))[0]
        theirs = exe.run(n, dones, at, playing, key, voice, reached, dt, mode)
        ours = port(n, dones, at, playing, key, voice, reached, dt, mode)
        if theirs != ours:
            bad += 1
            if bad <= 5:
                print(f'run {r}: n {n} at {at} dones {dones} playing {playing} key {key} mode {mode} voice {voice:#x}')
                print(f'  exe  {[hex(v) for v in theirs]}\n  port {[hex(v) for v in ours]}')
    print(f'{runs} runs: {bad} differ')
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    code = 0
    try:
        main()
    except SystemExit as ex:
        code = 0 if ex.code in (None, 0) else 1
    except BaseException:
        import traceback
        traceback.print_exc()
        code = 1
    sys.stdout.flush()
    import os
    os._exit(code)
