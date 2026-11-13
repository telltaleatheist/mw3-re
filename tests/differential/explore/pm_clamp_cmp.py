"""PM's limit clamp 0x004a71f0 against MW3's vt+0x04 (0x004a41b0) on vehicle-like
limits (gun pitch lo..hi, full yaw) and random directions. For the PM port audit (docs/port-audit-pm.md)."""
import math, random, struct, sys, collections
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from original import Original
from unicorn.x86_const import UC_X86_REG_EAX
def fb(x): return struct.unpack('<I', struct.pack('<f', x))[0]
pm = Original(exe='pm'); pm.tr = lambda a, size=1: a
mw = Original()
# MSVCRT's _hypot as x87 code (as test_ai_firing.py)
mw.import_code(0x5bb5b0, bytes.fromhex('dd442404d8c8dd44240cd8c8dec1d9fac3'))
po, pi_, pout = pm.alloc(0x100), pm.alloc(16), pm.alloc(16)
mo, mi, mout = mw.alloc(0x100), mw.alloc(16), mw.alloc(16)
r = random.Random(7)
diff = collections.Counter(); n = 0; worst = (0, None)
for _ in range(int(sys.argv[1]) if len(sys.argv) > 1 else 2000):
    if r.random() < 0.3: lo, hi = -0.259, 0.5   # gun_pitch's default (0xbe84816f, 0.5)
    else: lo, hi = sorted(r.uniform(-1.5, 1.5) for _ in range(2))
    yl, yh = -2 * math.pi, 2 * math.pi
    th = r.uniform(-math.pi, math.pi); ph = r.uniform(-1.5, 1.5); s = r.choice([1.0, r.uniform(0.3, 3)])
    d = (s * math.cos(ph) * math.sin(th), s * math.sin(ph), s * math.cos(ph) * math.cos(th))
    pm.write(po, b'\0' * 0x100)
    pm.call(0x004a7390, ecx=po, stack_args=[fb(lo), fb(hi), fb(yl), fb(yh)])
    pm.write(pi_, struct.pack('<3f', *d)); pm.write(pout, b'\0' * 12)
    pm.call(0x004a71f0, ecx=po, stack_args=[pi_, pout])
    pr = pm._uc.reg_read(UC_X86_REG_EAX) & 0xff; pv = struct.unpack('<3f', pm.read(pout, 12))
    mw.write(mo, b'\0' * 0x100)
    mw.call(0x004a4b30, ecx=mo, stack_args=[fb(lo), fb(hi), fb(yl), fb(yh)])
    lim = struct.unpack('<I', mw.read(mo, 4))[0]; vt = struct.unpack('<I', mw.read(lim, 4))[0]
    fn = struct.unpack('<I', mw.read(vt + 4, 4))[0]
    mw.write(mi, struct.pack('<3f', *d)); mw.write(mout, b'\0' * 12)
    mw.call(fn, ecx=lim, stack_args=[mi, mout])
    mr = mw._uc.reg_read(UC_X86_REG_EAX) & 0xff; mv = struct.unpack('<3f', mw.read(mout, 12))
    n += 1
    e = max(abs(a - b) for a, b in zip(pv, mv))
    key = ('within' if mr else 'past', 'answer same' if bool(pr) == bool(mr) else 'answer differs', 'out same' if e < 1e-4 else 'out differs')
    diff[key] += 1
    if e > worst[0]: worst = (e, (lo, hi, d, pv, mv, pr, mr))
print(n, dict(diff)); print('worst', worst)
