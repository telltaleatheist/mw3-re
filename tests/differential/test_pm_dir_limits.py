"""Pirate's Moon's direction limits (src/mech3/dir_limits_pm: the exe's CZValidateDir) against PM's exe: the
record each setter makes (0x4a6ec0 unset, then 0x4a6ee0, 0x4a7390 or 0x4a7420), the within test (0x4a70c0 over
0x4a7000) and the clamp (0x4a71f0 with 0x4a7170) on random limits and directions. The limits: yaw ranges on either
side of 0, across it, tiny, reversed, a turn or more; pitches inside, outside and reversed (the setter's report
stubbed). The directions: any way, all but straight up or down around the 0.999 cut, along the axes, zero and NaN.
The answers must match exactly; the record and the clamp's direction to 1e-6 (the x87's fsin and fcos against
the host's, an ulp apart at times).

    cmake --build build --target pmprobe && .venv/bin/python tests/differential/test_pm_dir_limits.py [cases]
"""
import ctypes
import math
import os
import random
import struct
import sys
from pathlib import Path

os.environ.setdefault('MW3_X87', '53')
sys.path.insert(0, str(Path(__file__).parent))
import pm_floors  # noqa: E402
from original import Original, ROOT  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

probe = ctypes.CDLL(str(next((ROOT / 'build').glob('libpmprobe.*'))))
F = ctypes.c_float
probe.probe_pm_dir.argtypes = [ctypes.c_int, F * 4, F * 3, F * 18, ctypes.POINTER(ctypes.c_int), F * 3]
NAN = float('nan')


def fb(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def f32(*v):
    return struct.unpack(f'<{len(v)}f', struct.pack(f'<{len(v)}f', *v))


def angle(r):
    return r.choice([r.uniform(-1.6, 1.6), r.uniform(-7.0, 7.0), 0.0, r.uniform(-0.0005, 0.0005),
                     math.pi, -math.pi, 2 * math.pi, -2 * math.pi, math.pi / 2, -math.pi / 2])


def limits(r):
    how = r.choice([0, 0, 1, 1, 2])
    if how == 2:
        return how, f32(r.choice([r.uniform(0, 1.6), 1.5707963, 2.0]), r.choice([r.uniform(0, 7), 2 * math.pi, 7.0]), 0, 0)
    plo, phi = sorted(r.uniform(-1.6, 1.6) for _ in range(2))
    if r.random() < 0.1:
        plo, phi = phi, plo
    if r.random() < 0.1:
        plo, phi = r.uniform(-2.5, -1.4), r.uniform(1.4, 2.5)
    ylo, yhi = sorted((angle(r), angle(r)))
    if r.random() < 0.1:
        ylo, yhi = yhi, ylo
    if r.random() < 0.1:
        yhi = ylo + r.uniform(0, 0.002)
    return how, f32(plo, phi, ylo, yhi)


def direction(r):
    k = r.random()
    if k < 0.06:
        y = r.choice([1, -1]) * r.uniform(0.998, 1.0)
        a = r.uniform(-math.pi, math.pi)
        s = math.sqrt(max(0.0, 1 - y * y))
        return f32(s * math.sin(a), y, s * math.cos(a))
    if k < 0.1:
        return f32(*r.choice([(0, 1, 0), (0, -1, 0), (1, 0, 0), (0, 0, -1), (0, 0, 0), (NAN, 0, 0), (0, NAN, 1)]))
    th, ph = r.uniform(-math.pi, math.pi), r.uniform(-1.57, 1.57)
    s = r.choice([1.0, r.uniform(0.2, 3)])
    return f32(s * math.cos(ph) * math.sin(th), s * math.sin(ph), s * math.cos(ph) * math.cos(th))


def same(a, b, tol):
    if math.isnan(a) or math.isnan(b):
        return math.isnan(a) and math.isnan(b)
    return abs(a - b) <= tol * max(1.0, abs(a))


def main():
    cases = int(sys.argv[1]) if len(sys.argv) > 1 else 6000
    pm = Original(exe='pm')
    pm.tr = lambda a, size=1: a  # PM's own addresses
    pm.stub_function(0x406cb0, lambda c: 0)  # the setter's report (cdecl): nothing
    rec, pd, pout = pm.alloc(0x48), pm.alloc(16), pm.alloc(16)
    r = random.Random(0x4a71f0)
    bad = 0
    count = {'within': 0, 'clamped': 0, 'vertical': 0, 'edges': 0}
    for n in range(cases):
        how, a = limits(r)
        d = direction(r)
        pm.write(rec, b'\0' * 0x48)
        pm.call(0x004a6ec0, ecx=rec)
        if how == 0:
            pm.call(0x004a6ee0, ecx=rec, stack_args=[fb(x) for x in a])
        elif how == 1:
            pm.call(0x004a7390, ecx=rec, stack_args=[fb(x) for x in a])
        elif how == 2:
            pm.call(0x004a7420, ecx=rec, stack_args=[fb(a[0]), fb(a[1])])
        erec = pm.read(rec, 0x48)
        pm.write(pd, struct.pack('<3f', *d))
        pm.call(0x004a70c0, ecx=rec, stack_args=[pd])
        ewithin = pm._uc.reg_read(UC_X86_REG_EAX) & 0xff
        pm.write(pout, b'\0' * 12)
        pm.call(0x004a71f0, ecx=rec, stack_args=[pd, pout])
        eclamp = pm._uc.reg_read(UC_X86_REG_EAX) & 0xff
        eout = struct.unpack('<3f', pm.read(pout, 12))
        prec, pw, po = (F * 18)(), ctypes.c_int(), (F * 3)()
        pc = probe.probe_pm_dir(how, (F * 4)(*a), (F * 3)(*d), prec, ctypes.byref(pw), po)
        erec_f = struct.unpack('<18f', erec)
        ok = all(same(x, y, 1e-6) for x, y in zip(erec_f, prec)) and bool(ewithin) == bool(pw.value) and bool(eclamp) == bool(pc) \
            and all(same(x, y, 1e-6) for x, y in zip(eout, po))
        count['within'] += bool(ewithin)
        count['clamped'] += not eclamp
        count['vertical'] += abs(d[1]) >= 0.999 and not eclamp
        count['edges'] += not eclamp and abs(d[1]) < 0.999 and eout[1] == 0.0
        if not ok:
            bad += 1
            if bad <= 5:
                print(f'FAIL {n}: how {how} a {a} d {d}')
                print(f'  exe  rec {struct.unpack("<18f", erec)} within {ewithin} clamp {eclamp} {eout}')
                print(f'  port rec {tuple(prec)} within {pw.value} clamp {pc} {tuple(po)}')
    print(f'{cases} cases, {bad} differ; {count}')
    sys.stdout.flush()
    if bad:
        os._exit(1)
    pm_floors.floors(within=(count['within'], cases // 5), clamped=(count['clamped'], cases // 5),
                     vertical=(count['vertical'], 20))
    os._exit(0)


if __name__ == '__main__':
    main()
