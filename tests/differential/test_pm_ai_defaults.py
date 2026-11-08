"""PM's AI structs as their constructors and the start-up leave them, every word against the port's at 53 bits: the
firing's defaults 0x60b688 (the start-up's 0x4ba660, which 0x4ba750 copies), the aim 0x4bad70, the list of who hit a
pilot 0x4b2950 (0x4b2580), the fighting state 0x4d9b10 and the steering 0x4dc0f0. The tests of their code set most of
these as inputs; this is where their starting values are compared (docs/pm-ai-fields.md).

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_ai_defaults.py
"""
import ctypes
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
os.environ['MW3_X87'] = '53'  # the reference's precision: PM's AI, docs/pm-tests.md
import test_pm_zar  # noqa: E402

FIRING, AIM, ATTACKERS, FIGHT, STEER = 0x4ba660, 0x4bad70, 0x4b2950, 0x4d9b10, 0x4dc0f0
DEFAULTS, CLOCK, NEW = 0x60b688, 0x8c1b50, 0x5ce8c6
STARTUP = (0x4dbf80, 0x4dbfa0, 0x4dbfc0, 0x4dc020, 0x4dc070, 0x4dc0c0, 0x4dc0e0)  # the steering's
# The words a constructor doesn't write: the exe's are whatever the memory held (new's, here 0xcd), the port's 0.
UNSET = {'aim': {6}, 'fight': {0, 1}, 'steer': {0}}


def bits(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


def words(z, a, n):
    return list(struct.unpack(f'<{n}I', bytes(z.cpu._uc.mem_read(a, 4 * n))))


def compare(name, exe, port):
    bad = [i for i, (a, b) in enumerate(zip(exe, port)) if i not in UNSET.get(name, ()) and (a & m(name, i)) != (b & m(name, i))]
    for i in bad:
        print(f'  {name} +{4 * i:#04x}: exe {exe[i]:08x} port {port[i]:08x}')
    return not bad


def m(name, i):
    """Bytes the struct holds in a word: the rest is padding."""
    return {('aim', 6): 0xff, ('fight', 2): 0xffffffff, ('fight', 11): 0xffff, ('fight', 13): 0xff,
            ('steer', 18): 0xff, ('steer', 22): 0xff, ('firing', 13): 0xff}.get((name, i), 0xffffffff)


def main():
    lib = test_pm_zar.load_pm_probe()
    P = ctypes.c_void_p
    for n in ('firing', 'aim', 'fight', 'steer'):
        getattr(lib, f'probe_pm_{n}_defaults').argtypes = [P]
    lib.probe_pm_attackers_defaults.argtypes = [ctypes.c_float, P]
    z = test_pm_zar.Emulated()
    w = lambda a, v: z.cpu._uc.mem_write(a, struct.pack('<I', v & 0xffffffff))  # noqa: E731
    z.native_stub(NEW, lambda c: z.alloc(0x40))
    z.native_stub(0x4d3a40, lambda c: 0, 4)  # the fighting state's base: its +0x04
    out = (ctypes.c_uint32 * 64)()
    ok = True

    for f in STARTUP:  # the start-up's initializers of the statics the constructors copy
        z.call(f)
    z.call(FIRING)
    lib.probe_pm_firing_defaults(out)
    ok &= compare('firing', words(z, DEFAULTS, 27), list(out[:27]))

    for name, ctor, size, args in (('aim', AIM, 9, ()), ('fight', FIGHT, 14, (0,)), ('steer', STEER, 40, ())):
        o = z.alloc(4 * size + 0x40)
        z.cpu._uc.mem_write(o, b'\xcd' * 4 * size)
        z.call(ctor, ecx=o, args=args)
        getattr(lib, f'probe_pm_{name}_defaults')(out)
        ok &= compare(name, words(z, o, size), list(out[:size]))

    for t in (0.0, 12.5, 1234.0):
        w(CLOCK, bits(t))
        o = z.alloc(0x40)
        z.call(ATTACKERS, ecx=o, args=(bits(t),))
        lib.probe_pm_attackers_defaults(t, out)
        e = words(z, o, 7)  # the pilot's list: its vtable, then the list at +0x04 (rate +0x0c of that, ...)
        ok &= compare('attackers', [e[4], e[5], e[6], int(z.r32(e[2]) != e[2])], list(out[:4]))
    print('ok: the firing, aim, fighting state, steering and attackers start as the exe\'s' if ok else 'differ')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
