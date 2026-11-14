"""Pirate's Moon's FMV script tick 0x5ba960 (MW3's 0x599620), against PM's exe: Space (or Escape,
Enter) while the current action is skippable (vtable +0x10; not the pictures nor STOPCD) clears
the input, ends it, passes over every skippable action after it and starts the next that is not,
or with none left stops the sounds and ends; MW3 ended the whole script on the key. Otherwise the
current steps, and the next starts when it is done. Scripts of 0..7 recording actions (each
skippable or not, each step's answers from a plan), polls at quarter seconds with the key down or
not. Compared: every call to the actions (which, with what time), the input cleared, the sounds
stopped, each poll's answer and the current action.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_fmv_skip.py [runs]
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_pm_zar  # noqa: E402

probe = test_pm_zar.load_pm_probe()
U8 = ctypes.c_char_p
probe.probe_pm_fmv_skip.argtypes = [ctypes.c_int32, U8, U8, ctypes.c_int32, ctypes.c_int32,
                                    ctypes.POINTER(ctypes.c_double), U8, ctypes.POINTER(ctypes.c_int32), ctypes.c_int32]
probe.probe_pm_fmv_skip.restype = ctypes.c_int32
TICK = 0x5ba960


def fbits(d):
    return struct.unpack('<i', struct.pack('<f', d))[0]


class Exe:
    def __init__(self):
        self.e = e = test_pm_zar.Emulated()
        cpu = e.cpu
        self.log, self.key, self.acts = [], 0, {}
        put = self.log.extend
        e.native_stub(0x534320, lambda c: 0x80 if c.arg(0) == 0x39 and self.key else 0)
        e.native_stub(0x533b90, lambda c: put((4, 0, 0)))
        e.native_stub(0x58ff90, lambda c: 0x77)
        e.native_stub(0x5904b0, lambda c: put((5, c.arg(0), 0)))

        def dbl(c):
            return struct.unpack('<d', struct.pack('<2I', c.arg(0), c.arg(1)))[0]

        def step(c):
            a = self.acts[c.ecx]
            put((1, a['i'], fbits(dbl(c))))
            r = a['plan'][a['calls'] & 7]
            a['calls'] += 1
            return r
        slots = {4: (step, 8), 8: (lambda c: put((2, self.acts[c.ecx]['i'], fbits(dbl(c)))), 8),
                 0xc: (lambda c: put((3, self.acts[c.ecx]['i'], 0)), 0),
                 0x10: (lambda c: self.acts[c.ecx]['skip'], 0)}
        self.vt = e.alloc(0x20)
        for off, (fn, pop) in slots.items():
            code = cpu.code(b'\xc2' + struct.pack('<H', pop) if pop else b'\xc3')
            e.native_stub(code, fn, pop)
            cpu._uc.mem_write(self.vt + off, struct.pack('<I', code))
        self.script = e.alloc(0x20)
        self.objs = [e.alloc(0x10) for _ in range(8)]

    def run(self, n, skips, plans, at, ts, keys):
        u = self.e.cpu._uc
        self.log.clear()
        self.acts.clear()
        for i in range(n):
            o = self.objs[i]
            u.mem_write(o, struct.pack('<II', self.vt, self.objs[i + 1] if i + 1 < n else 0))
            self.acts[o] = {'i': i, 'skip': skips[i], 'plan': plans[8 * i:8 * i + 8], 'calls': 0}
        u.mem_write(self.script + 8, struct.pack('<d', 0.0))
        u.mem_write(self.script + 0x18, struct.pack('<I', self.objs[at] if at < n else 0))
        idx = {o: i for i, o in enumerate(self.objs[:n])}
        for t, k in zip(ts, keys):
            self.key = k
            r = self.e.call(TICK, ecx=self.script, args=list(struct.unpack('<2I', struct.pack('<d', t))))
            cur = struct.unpack('<I', bytes(u.mem_read(self.script + 0x18, 4)))[0]
            self.log.extend((6, r & 0xff, idx.get(cur, -1)))
        return [v if v < 0x80000000 else v - (1 << 32) for v in self.log]


def main():
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 400
    rnd = random.Random(3)
    exe = Exe()
    bad = 0
    for r in range(runs):
        n = rnd.randint(0, 7)
        skips = [rnd.choice((0, 1, 1)) for _ in range(n)]
        plans = [rnd.choice((0, 1, 1)) for _ in range(8 * n)]
        at = rnd.randint(0, n)
        polls = rnd.randint(1, 12)
        ts = [0.25 * (p + 1) for p in range(polls)]
        keys = [rnd.choice((0, 0, 1)) for _ in range(polls)]
        theirs = exe.run(n, skips, plans, at, ts, keys)
        buf = (ctypes.c_int32 * 2048)()
        m = probe.probe_pm_fmv_skip(n, bytes(skips) or b'\0', bytes(plans) or b'\0', at, polls,
                                    (ctypes.c_double * polls)(*ts), bytes(keys), buf, 2048)
        ours = list(buf[:m])
        if theirs != ours:
            bad += 1
            if bad <= 4:
                print(f'run {r}: n {n} skips {skips} at {at} keys {keys}\n  exe  {theirs}\n  port {ours}')
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
