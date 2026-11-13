"""PM's turret limits (promote1.cpp's CZValidateDir: set 0x004a7390, test 0x004a7000)
against a float32 copy of the port's TurnLimits (src/mech3/turret.cpp make/within).
For the PM port audit (docs/port-audit-pm.md): prints how often they disagree, by limit kind."""
import math, random, struct, sys, collections
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from original import Original

def f32(x): return struct.unpack('<f', struct.pack('<f', float(x)))[0]
def mul(a, b): return f32(a * b)
def dot(a, b, c, d): return f32(mul(a, b) + mul(c, d))
def fb(x): return struct.unpack('<I', struct.pack('<f', x))[0]

KPI = struct.unpack('<d', struct.pack('<Q', 0x400921fb54442eea))[0]
K2PI = struct.unpack('<d', struct.pack('<Q', 0x401921fb54442eea))[0]
EPS = struct.unpack('<f', struct.pack('<I', 0x3a83126f))[0]
TINY = struct.unpack('<f', struct.pack('<I', 0x3727c5ac))[0]

def fs(a): return f32(math.sin(float(a)))
def fc(a): return f32(math.cos(float(a)))

def port(lo, hi, ylo, yhi, d):
    lo, hi, ylo, yhi = map(f32, (lo, hi, ylo, yhi))
    d = [f32(x) for x in d]
    rng = f32(yhi - ylo)
    lo_s, hi_s = fs(lo), fs(hi)
    if not (rng > f32(EPS)):
        kind = 'fixed'; e = (fc(ylo), 0.0, -fs(ylo))
    elif float(rng) >= K2PI:
        kind = 'full'
    else:
        kind = 'wide' if float(rng) >= KPI else 'narrow'
        a_e = (-fc(ylo), 0.0, fs(ylo)); b_e = (fc(yhi), 0.0, -fs(yhi))
    if lo_s > d[1] or d[1] > hi_s:
        return kind, False
    if kind == 'full':
        return kind, True
    if kind == 'fixed':
        across = dot(d[0], d[0], d[2], d[2]); off = dot(d[0], e[0], d[2], e[2])
        return kind, mul(across, f32(TINY)) > mul(off, off)
    a = dot(d[0], a_e[0], d[2], a_e[2]); b = dot(d[0], b_e[0], d[2], b_e[2])
    return kind, ((a >= 0) and (b >= 0)) if kind == 'narrow' else ((a >= 0) or (b >= 0))

cpu = Original(exe='pm')
cpu.tr = lambda a, size=1: a
obj, dv, out = cpu.alloc(0x100), cpu.alloc(16), cpu.alloc(0x40)

def pm(lo, hi, ylo, yhi, d):
    cpu.write(obj, b'\0' * 0x100)
    cpu.call(0x004a7390, ecx=obj, stack_args=[fb(lo), fb(hi), fb(ylo), fb(yhi)])
    cpu.write(dv, struct.pack('<3f', *d))
    cpu.call(0x004a7000, ecx=obj, stack_args=[dv, out])
    return bool(cpu._uc.reg_read(__import__('unicorn').x86_const.UC_X86_REG_EAX) & 0xff)

r = random.Random(1113)
PITCH = struct.unpack('<f', struct.pack('<I', 0x3fc90fd8))[0]
cases = []
# turret-like: pitch +-1.570796, yaw from the data's degree ranges; vehicle-like: full yaw
N = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
for _ in range(N):
    k = r.random()
    if k < 0.25:
        a, b = sorted(r.uniform(-180, 180) for _ in range(2)); ylo, yhi = math.radians(a), math.radians(b)
    elif k < 0.4:
        c = r.uniform(-180, 180); w = r.uniform(180, 359); ylo, yhi = math.radians(c - w / 2), math.radians(c + w / 2)
    elif k < 0.5:
        ylo, yhi = -2 * math.pi, 2 * math.pi
    elif k < 0.6:
        y = math.radians(r.uniform(-180, 180)); ylo, yhi = y, y
    else:
        a, b = sorted(r.uniform(-360, 360) for _ in range(2)); ylo, yhi = math.radians(a), math.radians(b)
    lo, hi = (-PITCH, PITCH) if r.random() < 0.6 else sorted(r.uniform(-1.5, 1.5) for _ in range(2))
    th = r.uniform(-math.pi, math.pi); ph = r.uniform(-1.4, 1.4); s = r.choice([1.0, 1.0, r.uniform(0.2, 3)])
    d = (s * math.cos(ph) * math.sin(th), s * math.sin(ph), s * math.cos(ph) * math.cos(th))
    cases.append((lo, hi, ylo, yhi, d))

tally = collections.Counter(); bad = collections.Counter(); ex = {}
for c in cases:
    kind, want = port(*c)
    got = pm(*c)
    tally[kind] += 1
    if got != want:
        bad[kind, want, got] += 1
        ex.setdefault((kind, want, got), c)
print('cases', dict(tally))
for k, n in sorted(bad.items()):
    print('differ', k, n, 'e.g.', tuple(round(x, 4) if isinstance(x, float) else tuple(round(y, 4) for y in x) for x in ex[k]))
print('total differ', sum(bad.values()), 'of', len(cases))

# The model checked against MW3's own limits (make 0x004a4b30, within vt+0x08).
mw = Original()
mo, md = mw.alloc(0x100), mw.alloc(16)
from unicorn.x86_const import UC_X86_REG_EAX
mbad = collections.Counter()
for c in cases:
    lo, hi, ylo, yhi, d = c
    mw.write(mo, b'\0' * 0x100)
    mw.call(0x004a4b30, ecx=mo, stack_args=[fb(lo), fb(hi), fb(ylo), fb(yhi)])
    lim = struct.unpack('<I', mw.read(mo, 4))[0]
    vt = struct.unpack('<I', mw.read(lim, 4))[0]
    fn = struct.unpack('<I', mw.read(vt + 8, 4))[0]
    mw.write(md, struct.pack('<3f', *d))
    mw.call(fn, ecx=lim, stack_args=[md])
    got = bool(mw._uc.reg_read(UC_X86_REG_EAX) & 0xff)
    kind, want = port(*c)
    if got != want: mbad[kind, want, got] += 1
print('model vs MW3: differ', sum(mbad.values()), dict(mbad))

hyp = {'mirrored x': lambda lo, hi, ylo, yhi, d: port(lo, hi, ylo, yhi, (-d[0], d[1], d[2])),
       'yaw negated': lambda lo, hi, ylo, yhi, d: port(lo, hi, -yhi, -ylo, d)}
for name, f in hyp.items():
    n = collections.Counter()
    for c in cases:
        kind, want = f(*c)
        if pm(*c) != want: n[kind] += 1
    print('PM vs port', name, 'differ', sum(n.values()), dict(n))
