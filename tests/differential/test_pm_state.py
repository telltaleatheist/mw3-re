"""PM's multiplayer menu state made and deleted, against the port (mech3/pilot_menu GameMenuState): the
constructor 0x4a0120 (the shell's 0x48cef0, then three screens, each `new` (0x5ce8c6) and its constructor
0x496660 / 0x498b70 / 0x49f5c0 with the state passed, a failed `new` leaving 0) and the destructor 0x4a0230
(each screen still held deleted through its vtable's +0x0c with 1, in order; +0x1c, +0x20, +0x24 and +0x0c
cleared; then the shell's 0x48cf60). MW3's twin runs inside test_pilotscreen.py.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_state.py [runs]

The screens are not built on either side: their constructors are stubs that give the object a vtable whose
+0x0c is recorded, and the port's engine records what it is asked to make and to call. PM's differences from
MW3's, which the comparison maps: the screens' deleting destructor is one slot on (+0x0c, MW3's +0x08; the port
names it by MW3's slot), and the constructors' addresses (the port names them by MW3's: 0x493260, 0x495f90,
0x49bd80). Each run: which `new` fails, which screens were already let go before the delete, the screen shown.
"""
import ctypes
import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_pm_zar  # noqa: E402

CTOR, DTOR, SHELL_CTOR, SHELL_DTOR, NEW = 0x4a0120, 0x4a0230, 0x48cef0, 0x48cf60, 0x5ce8c6
SCREENS = [(0x496660, 0x1443c, 0x493260), (0x498b70, 0x167b8, 0x495f90), (0x49f5c0, 0x123d8, 0x49bd80)]  # PM, size, MW3
VTABLE = 0x5ea54c
PORT_DELETE_SLOT, PM_DELETE_SLOT = 0x08, 0x0c

probe = test_pm_zar.load_pm_probe()
probe.probe_pm_state_run.argtypes = [ctypes.c_uint32, ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32,
                                     ctypes.c_void_p, ctypes.c_int]


class Emu:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.log, self.fail, self.news = [], 0, []
        cpu = z.cpu
        w32 = lambda a, v: cpu._uc.mem_write(a, int(v & 0xffffffff).to_bytes(4, 'little'))  # noqa: E731
        self.w32 = w32
        # The screens' vtable: every slot a recorded stub, so a call through any other slot would show.
        self.vt = z.alloc(0x40)
        for slot in range(0, 0x40, 4):
            fn = cpu.code(b'\xc3')
            w32(self.vt + slot, fn)
            z.native_stub(fn, lambda c, slot=slot: self.log.append((2, c.ecx, slot, c.arg(0))), 4)

        def op_new(c):
            size = c.arg(0)
            i = len(self.news)
            self.news.append(size)
            return 0 if self.fail >> i & 1 else z.alloc(size if size < 0x100 else 0x100)

        def shell_ctor(c):
            return c.ecx
        z.native_stub(NEW, op_new)
        z.native_stub(SHELL_CTOR, shell_ctor, 12)  # the name and two zeros
        z.native_stub(SHELL_DTOR, lambda c: 0)
        for pm, size, mw3 in SCREENS:
            def ctor(c, mw3=mw3):
                obj = c.ecx
                w32(obj, self.vt)
                self.log.append((1, mw3, c.arg(0), obj))
                return obj
            z.native_stub(pm, ctor, 4)

    def run(self, fail, gone, shown):
        z = self.z
        self.log, self.fail, self.news = [], fail, []
        g = z.alloc(0x40)
        assert z.call(CTOR, ecx=g) == g and z.r32(g) == VTABLE
        assert self.news == [s for _, s, _ in SCREENS], [hex(x) for x in self.news]
        held = [z.r32(g + 0x1c + 4 * i) for i in range(3)]
        # A failed `new` makes no screen: the port's engine says so with a record of handle 0.
        made, k = [], 0
        for i, (_, _, mw3) in enumerate(SCREENS):
            if fail >> i & 1:
                assert held[i] == 0
                made.append((1, mw3, g, 0))
            else:
                assert self.log[k][:3] == (1, mw3, g) and self.log[k][3] == held[i]
                made.append(self.log[k])
                k += 1
        assert k == len(self.log)
        for i in range(3):
            if gone >> i & 1:
                self.w32(g + 0x1c + 4 * i, 0)
        self.w32(g + 0x0c, held[shown] if shown < 3 else 0)
        self.log = []
        z.call(DTOR, ecx=g)
        assert [z.r32(g + o) for o in (0x1c, 0x20, 0x24, 0x0c)] == [0, 0, 0, 0]
        deletes = [(2, obj, PORT_DELETE_SLOT if slot == PM_DELETE_SLOT else 0x1000 + slot, a)
                   for _, obj, slot, a in self.log]
        return g, held, made, deletes


def port(g, held, fail, gone):
    handles = (ctypes.c_uint32 * 3)(*[h or 0 for h in held])
    out = (ctypes.c_uint32 * 256)()
    n = probe.probe_pm_state_run(g, handles, fail, gone, out, 256)
    v, recs, i = list(out[:n]), [], 0
    made_marker = None
    while i < n:
        if v[i] == 3:
            made_marker = tuple(v[i + 1:i + 4])
            i += 4
        else:
            recs.append(tuple(v[i:i + 4]))
            i += 4
    return recs, made_marker


def main():
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 200
    emu = Emu()
    bad, deletes_seen = 0, 0
    for seed in range(runs):
        r = random.Random(f'pm-state:{seed}')
        fail = r.choice([0, 0, 0, 0, 1, 2, 4, 3, 5, 6, 7])
        gone = r.choice([0, 0, 0, 1, 2, 4, 3, 5, 6, 7])
        shown = r.randrange(4)
        g, held, made, deletes = emu.run(fail, gone, shown)
        want = made + deletes
        got, marker = port(g, held, fail, gone)
        deletes_seen += len(deletes)
        if got != want or marker != tuple(held):
            bad += 1
            if bad <= 5:
                print(f'seed {seed} (fail {fail}, gone {gone}, shown {shown}): differs')
                print('  original', [tuple(hex(x) for x in t) for t in want], [hex(h) for h in held])
                print('  port    ', [tuple(hex(x) for x in t) for t in got], marker and [hex(h) for h in marker])
    if bad:
        print(f'FAILED: {bad} of {runs} runs differ')
        sys.exit(1)
    print(f'ok: {runs} runs identical ({deletes_seen} deletes; a failed new, screens already gone and the shown screen varied)')


if __name__ == '__main__':
    main()
