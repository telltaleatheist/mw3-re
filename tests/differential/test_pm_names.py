"""PM's rosters: the mech table 0x608238 (25) and the weapon table 0x608818 (56) through their functions, against
mech3/mech_names, every index and out of range, every name in either case and unknown names (docs/pm-map.md).

    cmake --build build --target pmprobe
    .venv/bin/python -P tests/differential/test_pm_names.py
"""
import ctypes
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_pm_zar  # noqa: E402

M_NAME, M_INDEX, M_MSG, M_MASS, M_CLASS, M_TECH = 0x486790, 0x486730, 0x4867c0, 0x486830, 0x486860, 0x486880
W_INDEX, W_NAME, W_MSG, W_TECH, W_AMMO, MESSAGE = 0x4869e0, 0x486a30, 0x486a50, 0x486a80, 0x486ab0, 0x596e50
STICK = 0x7700


class Emulated(test_pm_zar.Emulated):
    def __init__(self):
        super().__init__()
        self.native_stub(MESSAGE, lambda c: c.arg(0) | 0x10000000, 0)  # 'its id' for a shown name
        self.stack_str = self.alloc(0x40)
        stub = self.cpu.code(b'\xc3')  # the import [0x5de674]: _stricmp, the CRT's
        self.cpu._uc.mem_write(0x5de674, struct.pack('<I', stub))
        self.native_stub(stub, self.stricmp, 0)

    def stricmp(self, c):
        x, y = self.cstr(c.arg(0)).lower(), self.cstr(c.arg(1)).lower()
        return 0 if x == y else (1 if x > y else -1)

    def str(self, s):
        self.cpu._uc.mem_write(self.stack_str, s.encode() + b'\0')
        return self.stack_str

    def fret(self, addr, a):
        """A float return: store it through a small thunk."""
        out = self.alloc(4)
        code = b'\xff\x74\x24\x04\xb8' + struct.pack('<I', addr) + b'\xff\xd0\x83\xc4\x04\xd9\x1d' + struct.pack('<I', out) + b'\xc3'
        self.cpu.call(self.cpu.code(code), stack_args=(a,), count=2_000_000_000)
        return self.r32(out)

    def i(self, addr, *args):
        return self.call(addr, args=list(args)) & 0xffffffff


def main():
    p = test_pm_zar.load_pm_probe()
    cc = ctypes.c_char_p
    p.probe_pm_names.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.POINTER(cc)]
    p.probe_pm_name_index.argtypes = [ctypes.c_int, cc]
    e = Emulated()
    bad = 0
    n = 0

    def port(fn, a, b=0):
        t = cc()
        r = p.probe_pm_names(fn, a, b, ctypes.byref(t))
        return t.value.decode() if fn in (0, 5) else r & 0xffffffff

    def check(what, want, got):
        nonlocal bad, n
        n += 1
        if want != got:
            bad += 1
            if bad <= 10:
                print(f'{what}: exe {want!r} port {got!r}')

    names = {0: [], 1: []}
    for m in range(-2, 28):
        a = m & 0xffffffff
        nm = e.cstr(e.i(M_NAME, a)).decode()
        names[0].append(nm)
        check(f'mech name {m}', nm, port(0, m))
        check(f'mech tech {m}', e.i(M_TECH, a), port(1, m))
        msg = e.i(M_MSG, a)
        check(f'mech message {m}', msg & 0xffff if msg >> 28 == 1 else 0xffffffff, port(2, m))
        check(f'mech mass {m}', e.fret(M_MASS, a), port(3, m))
        check(f'mech class {m}', e.i(M_CLASS, a), port(4, m))
    for w in range(-2, 59):
        a = w & 0xffffffff
        nm = e.cstr(e.i(W_NAME, a)).decode()
        names[1].append(nm)
        check(f'weapon name {w}', nm, port(5, w))
        msg = e.i(W_MSG, a)
        check(f'weapon message {w}', msg & 0xffff if msg >> 28 == 1 else 0xffffffff, port(6, w))
        for t in (1, 2, 3):
            check(f'weapon tech {w} {t}', e.i(W_TECH, a, t) & 0xff, port(7, w, t))
        check(f'weapon ammo {w}', e.i(W_AMMO, a) & 0xff, port(8, w))
    check('the Elemental', 'elemental', port(0, port(9, 0)))
    check('the Mad Cat', 'madcat', port(0, port(10, 0)))
    check('mech count', sum(x != 'notmech' for x in names[0]), port(11, 0))
    check('weapon count', sum(x != 'noweapon' for x in names[1]), port(12, 0))
    for kind, addr in ((0, M_INDEX), (1, W_INDEX)):
        for nm in sorted(set(names[kind])) + ['nosuch', 'Ppc', 'ELEMENTAL']:
            for v in (nm, nm.upper(), nm.lower()):
                check(f'index {kind} {v}', e.i(addr, e.str(v)), p.probe_pm_name_index(kind, v.encode()) & 0xffffffff)
    print(f'{n - bad}/{n} identical')
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    import os
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
    os._exit(code)
