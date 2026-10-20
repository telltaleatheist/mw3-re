"""zInput (docs/input.md) against the original: random plans of starts,
polls with scripted keyboard, mouse and joystick input, key and command
queries, bindings, callbacks, map pushes and pops, mouse moves and resets,
joystick reads, names, suspends and stops.

The input layer is stubbed identically on both sides: in the emulator fake
DirectInput COM objects (IDirectInput, three IDirectInputDevice2) and the
Win32 and message imports; in the rebuild a fake platform::Input and
zin::Host. Both call the same scripted World, which answers from what the
plan queued. Compared after every step: the step's result, every call out
(with its arguments), the callbacks fired, and a dump of zInput's globals.

    cmake --build build && .venv/bin/python tests/differential/test_zinput.py
"""
import ctypes
import math
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from unicorn.x86_const import UC_X86_REG_ESP  # noqa: E402
from original import FTOL, IAT_FTOL, ROOT, Original  # noqa: E402

probe = ctypes.CDLL(str(next((ROOT / 'build').glob('libmw3probe.*'))))
CB = ctypes.CFUNCTYPE(ctypes.c_int32, ctypes.c_int32, ctypes.c_int32, ctypes.c_int32, ctypes.c_void_p,
                      ctypes.POINTER(ctypes.c_uint32))
probe.zin_op.restype = ctypes.c_uint32
probe.zin_op.argtypes = [ctypes.c_int32] + [ctypes.c_uint32] * 5 + [ctypes.c_char_p, ctypes.c_char_p]

INPUTLOST, NOTACQ, GENERIC = 0x8007001e, 0x8007000c, 0x80004005
HR_NAMES = {0x80004005: 'DIERR_GENERIC', 0x8007000c: 'DIERR_NOTACQUIRED', 0x8007001e: 'DIERR_INPUTLOST',
            0x80070057: 'DIERR_INVALIDPARAM'}
KBD, MOUSE, JOY = 0, 1, 2
u32 = lambda v: v & 0xffffffff  # noqa: E731
f2u = lambda f: struct.unpack('<I', struct.pack('<f', f))[0]  # noqa: E731


class World:
    """The script both sides ask: devices, their queued input, Windows."""

    def __init__(self, seed, scen):
        self.rng = random.Random(seed)
        self.s = scen
        self.trace = []
        self.kbd = []      # queued (dik, data)
        self.kbd_hr = []   # queued results for the next reads
        self.mouse = []    # queued (x, y, z, buttons4)
        self.mouse_hr = []
        self.joy = []      # queued JoyState byte strings
        self.joy_hr = []
        self.acq_fail = scen['acq_fail']

    def log(self, *e):
        self.trace.append(e)

    def open(self, d):
        self.log('open', d)
        return 1 if self.s['present'][d] else 0

    def close(self, d):
        self.log('release', d)

    def acquire(self, d):
        self.log('acquire', d)
        return NOTACQ if self.rng.random() < self.acq_fail else 0

    def unacquire(self, d):
        self.log('unacquire', d)
        return NOTACQ if self.rng.random() < self.acq_fail / 2 else 0

    def coop(self, d, f):
        self.log('coop', d, f)

    def kbd_data(self, n):
        hr = self.kbd_hr.pop(0) if self.kbd_hr else 0
        take, self.kbd = self.kbd[:n], self.kbd[n:]
        self.log('kbd', n, hr, len(take))
        return hr, take

    def mouse_state(self):
        hr = self.mouse_hr.pop(0) if self.mouse_hr else 0
        st = self.mouse.pop(0) if self.mouse else (0, 0, 0, b'\0\0\0\0')
        self.log('mouse', hr)
        return hr, st

    def joy_state(self):
        hr = self.joy_hr.pop(0) if self.joy_hr else 0
        st = self.joy.pop(0) if self.joy else joy_bytes([32768] * 8, [0xffffffff] * 4, b'')
        self.log('joy', hr)
        return hr, st

    def caps(self):
        self.log('caps')
        return self.s['caps']

    def objname(self, ofs):
        self.log('objname', ofs)
        n = self.s['axes'][ofs // 4]
        return (0, n) if n else (0x80070002, '')

    def setrange(self, ofs, mn, mx):
        self.log('setrange', ofs, u32(mn), u32(mx))
        return u32(0x80070057) if self.s['range_fail'][ofs // 4] else 0

    def getrange(self, ofs):
        self.log('getrange', ofs)
        return 0, self.s['range']

    def deadzone(self, ofs, dz):
        self.log('deadzone', ofs, dz)
        return 0

    def c2s(self, x, y):
        self.log('c2s', u32(x), u32(y))
        return x + 100, y + 50

    def s2c(self, x, y):
        self.log('s2c', u32(x), u32(y))
        return x - 100, y - 50

    def setcursor(self, x, y):
        self.log('setcursor', u32(x), u32(y))

    def clientsize(self):
        self.log('clientsize')
        return self.s['client']

    def mapvk(self, scan):
        self.log('mapvk', scan)
        return 0 if scan in (0x9c, 0xb5, 0x37) or scan > 0xd3 else scan + 0x30

    def kbstate(self):
        self.log('kbstate')
        b = bytearray(256)
        b[0x90] = self.s['numlock']
        return self.s['kbstate_ok'], bytes(b)

    def toascii(self, vk, scan, numlock):
        self.log('toascii', vk, scan, numlock)
        if 0x10 <= scan <= 0x32 and scan not in (0x1c, 0x1d, 0x2a):
            return 1, 0x40 + scan
        return (1, 0) if scan == 0x39 else (0, 0)

    def msgid(self, name):
        self.log('msgid', name)
        h = sum(name.encode()) % 7
        return 0 if h < 3 else 1000 + sum(name.encode())

    def msgtext(self, i):
        self.log('msgtext', i)
        return '' if i % 5 == 0 else f'T{i}'

    def cmd_cb(self, n, cmd):
        self.log('cmdcb', n, cmd)

    def char_cb(self, ch, key, ctx):
        self.log('charcb', ch, key, ctx)

    def report(self, name, line):
        self.log('report', name, line)


def joy_bytes(axes, povs, buttons):
    b = struct.pack('<8i', *axes) + struct.pack('<4I', *povs) + bytes(buttons).ljust(128, b'\0')
    return b.ljust(0x110, b'\0')


def kevent(dik, data, i):
    return struct.pack('<4I', dik, data, i * 7, i)


# --- the rebuild -----------------------------------------------------------

class Rebuilt:
    def __init__(self, world, with_di):
        self.w = world
        self.cb = CB(self._cb)
        probe.zin_setup(self.cb, with_di)

    def _cb(self, op, a, b, buf, io):
        w = self.w
        if op == 1:
            return w.open(a)
        if op == 2:
            return w.close(a) or 0
        if op == 3:
            return s32(w.acquire(a))
        if op == 4:
            return s32(w.unacquire(a))
        if op == 5:
            return w.coop(a, u32(b)) or 0
        if op == 6:
            hr, ev = w.kbd_data(io[0])
            for i, (d, data) in enumerate(ev):
                ctypes.memmove(buf + 16 * i, kevent(d, data, i), 16)
            io[0] = len(ev)
            return s32(hr)
        if op == 7:
            hr, (x, y, z, bt) = w.mouse_state()
            if hr == 0:
                ctypes.memmove(buf, struct.pack('<3i', x, y, z) + bt, 16)
            return s32(hr)
        if op == 8:
            hr, st = w.joy_state()
            if hr != INPUTLOST or True:
                ctypes.memmove(buf, st, 0x110)
            return s32(hr)
        if op == 9:
            io[0], io[1] = w.caps()
            return 0
        if op == 10:
            hr, n = w.objname(a)
            if hr == 0:
                ctypes.memmove(buf, n.encode() + b'\0', len(n) + 1)
            return s32(hr)
        if op == 11:
            return s32(w.setrange(a, s32(io[0]), s32(io[1])))
        if op == 12:
            hr, (mn, mx) = w.getrange(a)
            io[0], io[1] = u32(mn), u32(mx)
            return s32(hr)
        if op == 13:
            return w.deadzone(a, u32(b))
        if op == 20 or op == 21:
            x, y = (w.c2s if op == 20 else w.s2c)(s32(io[0]), s32(io[1]))
            io[0], io[1] = u32(x), u32(y)
            return 0
        if op == 22:
            return w.setcursor(a, b) or 0
        if op == 23:
            io[0], io[1] = w.clientsize()
            return 0
        if op == 24:
            return w.mapvk(a)
        if op == 25:
            ok, st = w.kbstate()
            if ok:
                ctypes.memmove(buf, st, 256)
            return ok
        if op == 26:
            numlock = ctypes.string_at(buf, 256)[0x90]
            r, out = w.toascii(u32(a), u32(b), numlock)
            io[0] = out
            return r
        if op == 30:
            return w.msgid(ctypes.string_at(buf).decode('latin-1'))
        if op == 31:
            t = w.msgtext(a)
            if t is None:
                return 0
            ctypes.memmove(buf, t.encode() + b'\0', len(t) + 1)
            return 1
        if op == 40:
            return w.cmd_cb(a, b) or 0
        if op == 41:
            return w.char_cb(a, b, io[0]) or 0
        if op == 42:
            return w.report(ctypes.string_at(buf).decode(), b) or 0
        raise SystemExit(f'probe op {op}')

    def op(self, n, args=(), s=None):
        a = list(args) + [0] * (5 - len(args))
        out = ctypes.create_string_buffer(0x200)
        r = probe.zin_op(n, *[u32(x) for x in a[:5]], s.encode() if s is not None else None, out)
        return r, out.raw

    def dump(self):
        buf = (ctypes.c_uint32 * 20000)()
        n = probe.zin_dump(buf)
        return list(buf[:n])

    def name(self, which, i):
        out = ctypes.create_string_buffer(0x100)
        probe.zin_name(which, i, out)
        return out.value.decode('latin-1')


def s32(v):
    v &= 0xffffffff
    return v - (1 << 32) if v & 0x80000000 else v


# --- the original ----------------------------------------------------------

COM = 0x2d000000
GUID_KBD, GUID_MOUSE = 0x5cdf20, 0x5cdf10


class Emulated:
    def __init__(self, world, with_di):
        self.w = world
        self.with_di = with_di
        cpu = self.cpu = Original()
        cpu.import_code(IAT_FTOL, FTOL)
        cpu.fpu_control(0x007f)  # in play the x87 runs at 24 bits (CLAUDE.md)
        cpu.uc.mem_map(COM, 0x10000)
        self.next_stub = COM + 0x8000
        self.objs = {}
        di = self._obj('di', {2: (self._release_di, 4), 3: (self._create_device, 16), 4: (self._enum, 20)})
        self.di = di
        self.dev = {}
        for d in (KBD, MOUSE, JOY):
            self.dev[d] = self._obj(d, {
                0: (lambda c, d=d: self._qi(c), 12), 2: (lambda c, d=d: self._release(c, d), 4),
                3: (lambda c, d=d: self._caps(c), 8), 5: (lambda c, d=d: self._getprop(c), 12),
                6: (lambda c, d=d: self._setprop(c, d), 12), 7: (lambda c, d=d: s32u(self.w.acquire(d)), 4),
                8: (lambda c, d=d: s32u(self.w.unacquire(d)), 4), 9: (lambda c, d=d: self._state(c, d), 12),
                10: (lambda c, d=d: self._data(c), 20), 11: (lambda c, d=d: 0, 8),
                13: (lambda c, d=d: self._coop(c, d), 12), 14: (lambda c, d=d: self._objinfo(c), 16),
                25: (lambda c, d=d: 0, 4)})
        self.release_count = {}
        cpu.stub_function(0x5ad678, self._di_create, 16)
        heap = {'n': 0x10000000 + 0x400000}
        self.sizes = {}

        def alloc(n, zero=True):
            a = heap['n']
            heap['n'] += (n + 0x1f) & ~0xf
            cpu.write(a, b'\0' * max(n, 1))
            self.sizes[a] = n
            return a
        self.alloc = alloc

        def cstr(a):
            out = b''
            while True:
                c = cpu.read(a + len(out), 1)
                if c == b'\0':
                    return out
                out += c

        def put(a, b):
            cpu.write(a, b)
        imp = cpu.stub_import
        imp(0x5bb60c, lambda c: alloc(c.arg(0) * c.arg(1)))  # calloc

        def realloc(c):
            old, n = c.arg(0), c.arg(1)
            a = alloc(n)
            if old:
                cpu.write(a, cpu.read(old, min(self.sizes.get(old, 0), n)))
            return a
        imp(0x5bb610, realloc)
        imp(0x5bb65c, lambda c: 0)  # free
        imp(0x5bb670, lambda c: self._strdup(c.arg(0), cstr))

        def strncpy(c):
            d, s, n = c.arg(0), c.arg(1), c.arg(2)
            b = cstr(s)[:n]
            put(d, b + b'\0' * (n - len(b)))
            return d
        imp(0x5bb648, strncpy)

        def strncat(c):
            d, s, n = c.arg(0), c.arg(1), c.arg(2)
            put(d + len(cstr(d)), cstr(s)[:n] + b'\0')
            return d
        imp(0x5bb600, strncat)

        def sprintf(c):
            fmt = cstr(c.arg(1)).decode('latin-1')
            out = fmt.replace('%d', str(s32(c.arg(2)))) if '%d' in fmt else fmt
            put(c.arg(0), out.encode('latin-1') + b'\0')
            return len(out)
        imp(0x5bb658, sprintf)
        cpu.stub_function(0x5abd26, lambda c: alloc(c.arg(0)))  # operator new
        cpu.stub_function(0x5abd1a, lambda c: 0)  # operator delete
        # Win32.
        imp(0x5bb6b0, self._c2s, 8)
        imp(0x5bb69c, lambda c: self.w.setcursor(s32(c.arg(0)), s32(c.arg(1))) or 1, 8)
        imp(0x5bb6ac, self._clientsize, 8)
        imp(0x5bb6b4, self._s2c, 8)
        imp(0x5bb6a0, lambda c: self.w.mapvk(c.arg(0)), 8)
        imp(0x5bb6a4, self._kbstate, 4)
        imp(0x5bb6a8, self._toascii, 20)
        cpu.stub_function(0x575f90, lambda c: self.w.msgid(cstr(c.ecx).decode('latin-1')) if c.ecx else 0)
        self.text = alloc(0x200)

        def msgtext(c):
            t = self.w.msgtext(c.ecx)
            if t is None:
                return 0
            put(self.text, t.encode() + b'\0')
            return self.text
        cpu.stub_function(0x576040, msgtext)
        cpu.stub_function(0x51ddc0, self._report, 4)

        def settings_entry(c):  # 0x00597d90: a new entry, its data zeroed (the store is not ported)
            e = alloc(0x18)
            cpu.u32(e, alloc(c.arg(0)))
            return e
        cpu.stub_function(0x597d90, settings_entry, 8)
        self.cstr = cstr
        # Callbacks: eight command callbacks, one char callback.
        self.cmd_cbs = []
        for i in range(8):
            a = self._stub(lambda c, i=i: self.w.cmd_cb(i, s32(c.ecx)) or 0, 0)
            self.cmd_cbs.append(a)
        self.char_cb = self._stub(lambda c: self.w.char_cb(s32(c.ecx), s32(c.edx), c.arg(0)) or 0, 4)
        self.strs = {}

    def _stub(self, fn, pop):
        a = self.next_stub
        self.next_stub += 16
        self.cpu.write(a, b'\xc3')
        self.cpu.stub_function(a, fn, pop)
        return a

    def _obj(self, tag, slots):
        obj = COM + 0x1000 * (len(self.objs) + 1)
        vt = obj + 0x100
        self.cpu.u32(obj, vt)
        for i in range(32):
            fn, pop = slots.get(i, (lambda c: 0, 4))
            self.cpu.u32(vt + 4 * i, self._stub(fn, pop))
        self.objs[obj] = tag
        return obj

    def _strdup(self, s, cstr):
        b = cstr(s)
        a = self.alloc(len(b) + 1)
        self.cpu.write(a, b + b'\0')
        return a

    def _di_create(self, c):
        if not self.with_di:
            return GENERIC
        self.cpu.u32(c.arg(2), self.di)
        return 0

    def _release_di(self, c):
        return 0

    def _release(self, c, d):
        if struct.unpack('<I', self.cpu.read(c.uc.reg_read(UC_X86_REG_ESP), 4))[0] != 0x51b629:
            self.w.close(d)  # not the mouse's first interface, let go after the query
        return 0

    def _create_device(self, c):
        guid, out = c.arg(1), c.arg(2)
        d = KBD if guid == GUID_KBD else MOUSE
        if not self.w.open(d):
            return GENERIC
        self.cpu.u32(out, self.dev[d])
        return 0

    def _qi(self, c):
        self.cpu.u32(c.arg(2), c.arg(0))
        return 0

    def _enum(self, c):
        if self.w.open(JOY):
            self.cpu.u32(0x63c4a8, self.dev[JOY])
        return 0

    def _caps(self, c):
        f, n = self.w.caps()
        self.cpu.u32(c.arg(1) + 4, f)
        self.cpu.u32(c.arg(1) + 0xc, n)
        return 0

    def _getprop(self, c):
        p = c.arg(2)
        ofs = struct.unpack('<I', self.cpu.read(p + 8, 4))[0]
        hr, (mn, mx) = self.w.getrange(ofs)
        self.cpu.write(p + 0x10, struct.pack('<ii', mn, mx))
        return hr

    def _setprop(self, c, d):
        if d != JOY:
            return 0  # the buffer sizes: part of open()
        prop, p = c.arg(1), c.arg(2)
        ofs, = struct.unpack('<I', self.cpu.read(p + 8, 4))
        if prop == 4:
            mn, mx = struct.unpack('<ii', self.cpu.read(p + 0x10, 8))
            return self.w.setrange(ofs, mn, mx)
        return self.w.deadzone(ofs, struct.unpack('<I', self.cpu.read(p + 0x10, 4))[0])

    def _state(self, c, d):
        buf = c.arg(2)
        if d == MOUSE:
            hr, (x, y, z, bt) = self.w.mouse_state()
            if hr == 0:
                self.cpu.write(buf, struct.pack('<3i', x, y, z) + bt)
            return hr
        hr, st = self.w.joy_state()
        self.cpu.write(buf, st)
        return hr

    def _data(self, c):
        buf, pn = c.arg(2), c.arg(3)
        n = struct.unpack('<I', self.cpu.read(pn, 4))[0]
        hr, ev = self.w.kbd_data(n)
        for i, (d, data) in enumerate(ev):
            self.cpu.write(buf + 16 * i, kevent(d, data, i))
        self.cpu.u32(pn, len(ev))
        return hr

    def _coop(self, c, d):
        self.w.coop(d, c.arg(2))
        return 0

    def _objinfo(self, c):
        p, ofs = c.arg(1), c.arg(2)
        hr, n = self.w.objname(ofs)
        if hr == 0:
            self.cpu.write(p + 0x20, n.encode() + b'\0')
        return hr

    def _c2s(self, c):
        p = c.arg(1)
        x, y = struct.unpack('<ii', self.cpu.read(p, 8))
        x, y = self.w.c2s(x, y)
        self.cpu.write(p, struct.pack('<ii', x, y))
        return 1

    def _s2c(self, c):
        p = c.arg(1)
        x, y = struct.unpack('<ii', self.cpu.read(p, 8))
        x, y = self.w.s2c(x, y)
        self.cpu.write(p, struct.pack('<ii', x, y))
        return 1

    def _clientsize(self, c):
        w, h = self.w.clientsize()
        self.cpu.write(c.arg(1), struct.pack('<4i', 0, 0, w, h))
        return 1

    def _kbstate(self, c):
        ok, st = self.w.kbstate()
        if ok:
            self.cpu.write(c.arg(0), st)
        return ok

    def _toascii(self, c):
        numlock = self.cpu.read(c.arg(2) + 0x90, 1)[0]
        r, out = self.w.toascii(c.arg(0), c.arg(1), numlock)
        self.cpu.write(c.arg(3), struct.pack('<H', out))
        return r

    def _report(self, c):
        self.w.report(HR_NAMES.get(c.ecx, 'Unknown Error'), c.arg(0))
        return 0

    def str(self, s):
        if s is None:
            return 0
        if s not in self.strs:
            a = self.alloc(len(s) + 1)
            self.cpu.write(a, s.encode('latin-1') + b'\0')
            self.strs[s] = a
        return self.strs[s]

    def op(self, n, args=(), s=None):
        a = [u32(x) for x in list(args) + [0] * (5 - len(args))]
        cpu, call = self.cpu, self.cpu.call
        out = self.alloc(0x200)
        res = 0
        eax = lambda: cpu.uc.reg_read(19)  # UC_X86_REG_EAX  # noqa: E731
        simple = {0: (0x51b0a0, a[0], a[1]), 1: (0x51b170,), 2: (0x51b1c0,), 3: (0x51b050,), 4: (0x51b080,),
                  5: (0x51b290,), 6: (0x51b2c0,), 7: (0x51b2f0,), 8: (0x51b310,), 9: (0x51b340,),
                  10: (0x51b360, a[0]), 11: (0x51a9b0, a[0]), 13: (0x51aa00, 0), 16: (0x51b4f0, a[0], a[1]),
                  17: (0x51b560,), 18: (0x51b590, a[0], a[1]), 21: (0x51bae0, a[0]), 22: (0x51baf0, 0),
                  23: (0x51ba80,), 26: (0x51d2f0, a[0], self.cmd_cbs[a[1] & 7] if a[1] else 0),
                  27: (0x51d300, a[0]), 28: (0x51c440, a[0]), 33: (0x51d1c0,), 34: (0x51d1d0, a[0]),
                  35: (0x51d3a0, 0), 36: (0x51d490,), 37: (0x51d0c0,), 38: (0x51d1f0, a[0]),
                  39: (0x51d240, a[0]), 40: (0x51dcf0, a[0]), 41: (0x51dd20, 0), 42: (0x51dd60,),
                  43: (0x51ddb0,), 44: (0x51b6e0, a[0]), 45: (0x51dc10,), 46: (0x51ab90, a[0]),
                  47: (0x51d250, a[0]), 48: (0x51d260, a[0]), 49: (0x51d270, a[0])}
        if n in simple:
            t = simple[n]
            call(t[0], *(list(t[1:]) + [0, 0])[:2])
            res = 0 if n in (2, 3, 4, 10, 16, 17, 18, 23, 33, 34, 35, 36, 37, 42) else eax()
            if n in (45, 40, 44, 46):
                res &= 0xff if n in (40, 44, 45) else 0xffffffff
            if n in (5, 6, 7, 8, 9):
                res &= 0xffff
        elif n == 12:
            call(0x51a9a0, self.char_cb if a[0] else 0, a[1])
        elif n == 14:
            call(0x51aee0, self.str(s))
            res = eax()
        elif n in (15, 19):
            call(0x51b440 if n == 15 else 0x51b790, stack_args=(a[0], a[1]))
        elif n == 20:
            call(0x51ba60, out)
            res = eax()
        elif n == 24:
            call(0x51c630, a[0], self.str(s))
        elif n == 25:
            call(0x51d2c0, a[0], self.str(s), stack_args=(a[1], a[2], a[3], a[4]))
            res = eax()
        elif n in (29, 30, 31, 32):
            fn = {29: 0x51d310, 30: 0x51d330, 31: 0x51d350, 32: 0x51d370}[n]
            call(fn, a[0], out, stack_args=(a[1],))
            r = eax()
            raw = bytearray(cpu.read(out, 0x200))
            ret = self.cstr(r) if r else b''
            raw[0x100:0x100 + len(ret) + 1] = ret + b'\0'
            return 0, bytes(raw) if n != 29 else bytes(raw[:0x100]) + bytes(0x100)
        else:
            raise SystemExit(f'op {n}')
        return res & 0xffffffff, bytes(cpu.read(out, 0x200))

    def rd(self, a, n=4):
        return struct.unpack('<I', self.cpu.read(a, 4))[0] if n == 4 else self.cpu.read(a, n)

    def dump(self):
        r = self.rd
        b = lambda a: self.cpu.read(a, 1)[0]  # noqa: E731
        h = lambda a: struct.unpack('<H', self.cpu.read(a, 2))[0]  # noqa: E731
        o = [r(0x63857c), r(0x638580), b(0x638584) | b(0x638585) << 8 | b(0x638586) << 16,
             h(0x638588) | h(0x63858a) << 16, h(0x63858c), r(0x638590), r(0x63859c), r(0x6385a4), r(0x6385a8),
             1 if r(0x63c49c) else 0, r(0x63c4a0)]
        keys = struct.unpack('<%dI' % (0x7de * 2), self.cpu.read(0x6385ac, 0x7de * 8))
        for i in range(0x7de):
            o += [keys[2 * i], 1 if keys[2 * i + 1] else 0]
        ev = r(0x638598)
        if ev and r(0x638590):
            for i in range(r(0x63859c)):
                o += [r(ev + 16 * i), r(ev + 16 * i + 4)]
        o.append(r(0x63c4a4))
        o += list(struct.unpack('<68I', self.cpu.read(0x63c4ac, 0x110)))
        o += list(struct.unpack('<68I', self.cpu.read(0x63c5bc, 0x110)))
        o += [r(0x63c6ec), r(0x63c6f0), r(0x63c6f4), r(0x63c6f8)]
        o += list(struct.unpack('<420I', self.cpu.read(0x63c6fc, 0x690)))
        o.append(r(0x63cd8c))
        o += list(struct.unpack('<8I', self.cpu.read(0x63cd94, 0x20)))
        o += [r(0x63d1a0), r(0x63d1b8), r(0x63d1bc), r(0x63d1c0), r(0x63d1c4), r(0x63d1c8), r(0x63d1cc),
              r(0x63d1d0), r(0x63d1d4)]
        o += list(struct.unpack('<15I', self.cpu.read(0x63d1d8, 0x3c)))
        o += [r(0x5ece48), r(0x5ece4c), r(0x5ece50), r(0x5ece54), r(0x63cdd0)]
        m = r(0x63cdb8)
        if m:
            cnt = r(m + 4)
            o += [1, b(m), cnt, b(m + 0x3f58)]
            bind, cbs = r(m + 8), r(m + 0x10)
            for i in range(cnt):
                cb = r(cbs + 4 * i)
                o += [r(bind + 4 * i), 0x100 + self.cmd_cbs.index(cb) if cb in self.cmd_cbs else (0xdead if cb else 0)]
            o += list(struct.unpack('<%dI' % (0x7de * 2 + 20), self.cpu.read(m + 0x18, (0x7de * 2 + 20) * 4)))
        else:
            o.append(0)
        return o

    def name(self, which, i):
        r = self.rd
        if which == 0:
            m = r(0x63cdb8)
            if not m or i >= r(m + 4):
                return ''
            return self.cstr(r(r(m + 0x14) + 4 * i)).decode('latin-1')
        base = {1: 0x63cdd4, 2: 0x63d14c, 3: 0x63d18c}[which]
        p = r(base + 4 * i)
        if not p:
            return '<none>' if which == 1 else ''
        return self.cstr(p).decode('latin-1')


def nan_free(words):
    # x87's NaN is the negative 'indefinite' (0/0, inf * 0: a screen of size 0).
    return [0x7fc00000 if (w & 0x7f800000) == 0x7f800000 and w & 0x7fffff else w for w in words]


def s32u(v):
    return v & 0xffffffff


# --- plans -----------------------------------------------------------------

DIKS = [0x01, 0x02, 0x10, 0x11, 0x1e, 0x1f, 0x20, 0x1c, 0x39, 0x3b, 0x3e, 0x47, 0x4b, 0x4d, 0x50, 0x53, 0x9c,
        0xb5, 0xc7, 0xc8, 0xcb, 0xcd, 0xcf, 0xd0, 0xd3, 0x37, 0x1d, 0x2a, 0x38, 0x36, 0x9d, 0xb8, 0xdd]
MODS = [0x1d, 0x2a, 0x38]


def scenario(rng):
    # Without a mouse the original crashes at the start (0x0051b5f0 uses the
    # device it did not get); the port carries on (input.md).
    present = [rng.random() < 0.95, True, rng.random() < 0.6]
    names = ['X Axis', 'Y Axis', 'Z Axis', 'X Rotation', 'Y Rotation', 'Z Rotation']
    axes = [n if rng.random() < 0.7 else '' for n in names]
    return {'present': present, 'acq_fail': rng.choice([0, 0, 0.05, 0.3]),
            'caps': (rng.choice([0, 0x100, 0x700, 0x300]), rng.randint(2, 6)), 'axes': axes,
            'range_fail': [rng.random() < 0.3 for _ in range(6)], 'range': (rng.randint(-5000, 0), rng.randint(1, 70000)),
            'client': rng.choice([(640, 480), (800, 600), (0, 0)]), 'numlock': rng.choice([0, 1]),
            'kbstate_ok': 1}  # failing, the original reads its stack buffer as it was


def plan(rng):
    steps = [('queue', None), ('op', 0, (0x1234, 0x400000))] if rng.random() < 0.95 else []
    if rng.random() < 0.9:
        steps.insert(0, ('op', 24, (0xa7,), rng.choice(['DefaultCmdMap', None])))
    held = set()
    bound = []
    for _ in range(rng.randint(40, 120)):
        k = rng.random()
        if k < 0.3:
            ev = []
            for _ in range(rng.choice([0, 1, 2, 3, 8])):
                d = rng.choice(DIKS + MODS * 2)
                down = d not in held if rng.random() < 0.8 else rng.random() < 0.5
                (held.add if down else held.discard)(d)
                ev.append((d, 0x80 if down else 0))
            if rng.random() < 0.15:  # Alt-F4 in pieces (its command runs at once)
                ev = rng.choice([[(0x38, 0x80), (0x3e, 0x80)], [(0x3e, 0)], [(0x38, 0)], [(0x3e, 0x80)]])
            mouse = (rng.randint(-40, 40), rng.randint(-40, 40), rng.choice([0, 0, 120, -120]),
                     bytes(rng.choice([0, 0x80]) for _ in range(4)))
            joy = joy_bytes([rng.randint(0, 65535) for _ in range(8)],
                            [rng.choice([0xffffffff, 0, 9000, 0xffff]) for _ in range(4)],
                            [rng.choice([0, 0, 0x80]) for _ in range(12)])
            hrs = (rng.choice([0] * 8 + [INPUTLOST, 1, NOTACQ]), rng.choice([0] * 8 + [INPUTLOST, NOTACQ]),
                   rng.choice([0] * 8 + [INPUTLOST, NOTACQ]))
            steps.append(('input', ev, mouse, joy, hrs))
            steps.append(('op', 10, (rng.choice([0, 1]),)))
        elif k < 0.45:
            steps.append(('op', 11, (rng.choice(DIKS + [d | m for d in DIKS[:8] for m in (0x100, 0x200, 0x400)]),)))
        elif k < 0.55:
            cmd = rng.choice(bound) if bound and rng.random() < 0.3 else rng.randint(0, 0xa6)
            bound.append(cmd)
            keys = [0] + DIKS + [d | 0x200 for d in DIKS[:6]] + [0x13e]
            steps.append(('op', 25, (cmd, 0x13e if rng.random() < 0.08 else rng.choice(keys), rng.choice(keys), rng.randint(0, 11), rng.randint(0, 3)),
                          rng.choice(['', 'CMD_X%d' % cmd, None])))
        elif k < 0.62:
            cmd = rng.choice(bound) if bound and rng.random() < 0.85 else rng.randint(0, 0xa6)
            steps.append(('op', 26, (cmd, rng.randint(0, 8))))
        elif k < 0.70:
            n = rng.choice([27, 27, 28, 38, 39, 47])
            if n in (27, 28, 38):
                steps.append(('op', n, (rng.choice(bound) if bound and rng.random() < 0.8 else rng.randint(0, 0xa6),)))
            else:
                steps.append(('op', n, (rng.choice(DIKS),)))
        elif k < 0.74:
            steps.append(('op', rng.choice([48, 49, 40, 44]), (rng.randint(1, 3),)))
        elif k < 0.80:
            n = rng.choice([29, 30, 31, 32])
            i = {29: rng.randint(0, 0xa6), 30: rng.choice([rng.randint(0, 0xdd), rng.choice(DIKS)]) |
                 rng.choice([0, 0x100, 0x200, 0x700]), 31: rng.randint(0, 10), 32: rng.randint(0, 3)}[n]
            steps.append(('op', n, (i, rng.choice([0x50, 8]))))
        elif k < 0.84:
            n = rng.choice([15, 16, 18, 19])
            args = {15: lambda: (f2u(rng.uniform(-1.5, 1.5)), f2u(rng.uniform(-1.5, 1.5))),
                    16: lambda: (rng.randint(-50, 900), rng.randint(-50, 700)),
                    18: lambda: rng.choice([(640, 480), (800, 600), (1, 1)]),
                    19: lambda: (f2u(rng.choice([1.0, 1.3, 2.5])), f2u(rng.choice([1.0, 1.3, 0.7])))}[n]()
            steps.append(('op', n, args))
        elif k < 0.88:
            steps.append(('op', rng.choice([17, 20, 21, 22, 23, 41, 42, 43, 45, 2]), (rng.choice([5, 8, 0xd]),)))
        elif k < 0.92:
            steps.append(('op', rng.choice([35, 35, 36, 33, 34]), (rng.randint(0, 1),)))
        elif k < 0.95:
            steps.append(('op', rng.choice([3, 4, 4, 5, 6, 7, 8, 9]), ()))
        elif k < 0.97:
            steps.append(('op', 12, (rng.choice([0, 0, 0, 1]), rng.randint(0, 99))))
        elif k < 0.985:
            steps.append(('op', rng.choice([13, 46]), (rng.choice(DIKS),)))
        else:
            steps.append(('op', 14, (), rng.choice(['T', 'T25', 'ESCAPE', 'nothing'])))
    if rng.random() < 0.4:  # Alt-F4's command: pressed, held, let go (0x0051a5d0's special case)
        cmd = rng.randint(1, 0xa6)
        steps += [('op', 25, (cmd, 0x13e, 0, 0, 0), None), ('op', 26, (cmd, 3)), ('op', 12, (0, 0))]
        for ev in ([(0x38, 0x80), (0x3e, 0x80)], [], [(0x3e, 0)], [(0x3e, 0x80), (0x3e, 0)], [(0x38, 0)]):
            steps += [('input', ev, (0, 0, 0, bytes(4)), joy_bytes([0] * 8, [0] * 4, b''), (0, 0, 0)),
                      ('op', 10, (rng.choice([0, 1]),))]
    if rng.random() < 0.5:
        steps += [('op', 37, ()), ('op', 1, ())]
    return steps


def run(seed):
    rng = random.Random(seed)
    scen = scenario(rng)
    with_di = 1 if rng.random() < 0.95 else 0
    steps = plan(rng)
    wa, wb = World(seed, scen), World(seed, scen)
    a, b = Emulated(wa, with_di), Rebuilt(wb, with_di)
    has_map = False
    started = False
    for i, st in enumerate(steps):
        if st[0] == 'queue':
            continue
        if st[0] == 'input':
            for w in (wa, wb):
                w.kbd += st[1]
                w.mouse.append(st[2])
                w.joy.append(st[3])
                w.kbd_hr.append(st[4][0])
                w.mouse_hr.append(st[4][1])
                w.joy_hr.append(st[4][2])
            continue
        n, args = st[1], st[2]
        s = st[3] if len(st) > 3 else None
        if n == 24:
            has_map = True
        # Steps the original cannot take in this state (null pointers): skipped on both sides.
        needs_map = n in (25, 26, 27, 28, 29, 33, 34, 35, 36, 38, 39, 47, 48, 49)
        if needs_map and (not has_map or (a.rd(0x63cdb8) == 0)):
            continue
        if n in (25, 26, 27, 28, 29, 38, 39, 47) and args and args[0] >= a.rd(a.rd(0x63cdb8) + 4):
            if n not in (39, 47):
                continue
        if n == 36 and a.rd(0x63cdd0) == 0:
            continue  # popping the last map leaves none
        if n == 10 and (not started or not has_map):
            continue  # the polls' callbacks read the map
        if n in (11, 13, 2, 23, 17, 22) and not started:
            continue
        if n == 13 and not (a.rd(0x638590)):
            continue
        if n == 0:
            started = True
        if n == 1:
            started = False
        if n == 37:
            has_map = False
        ra, oa = a.op(n, args, s)
        rb, ob = b.op(n, args, s)
        if n in (29, 30, 31, 32):
            oa, ob = oa[:0x100].split(b'\0')[0] + b'|' + oa[0x100:].split(b'\0')[0], \
                ob[:0x100].split(b'\0')[0] + b'|' + ob[0x100:].split(b'\0')[0]
        elif n != 20:
            oa = ob = b''
        else:
            oa = struct.pack('<15I', *nan_free(struct.unpack('<15I', oa[:0x3c])))
            ob = struct.pack('<15I', *nan_free(struct.unpack('<15I', ob[:0x3c])))
        if (ra, oa) != (rb, ob):
            raise SystemExit(f'seed {seed} step {i} op {st}: result {ra:#x} {oa!r} vs {rb:#x} {ob!r}')
        if wa.trace != wb.trace:
            k = next(j for j in range(min(len(wa.trace), len(wb.trace))) if wa.trace[j] != wb.trace[j]) \
                if any(x != y for x, y in zip(wa.trace, wb.trace)) else min(len(wa.trace), len(wb.trace))
            raise SystemExit(f'seed {seed} step {i} op {st}: calls differ at {k}:\n'
                             f"{wa.trace[k - 3:k + 6]}\n{wb.trace[k - 3:k + 6]}\n{len(wa.trace)} {len(wb.trace)}")
        da, db = nan_free(a.dump()), nan_free(b.dump())
        if da != db:
            j = next((j for j in range(min(len(da), len(db))) if da[j] != db[j]), min(len(da), len(db)))
            raise SystemExit(f'seed {seed} step {i} op {st}: state differs at word {j} '
                             f'({len(da)} vs {len(db)}): {da[j:j + 4]} vs {db[j:j + 4]}')
        if n == 24 or (n == 25 and i % 5 == 0):
            for which, cnt in ((0, 0xa7), (1, 0xde), (2, 11), (3, 4)):
                for k in range(cnt):
                    if a.name(which, k) != b.name(which, k):
                        raise SystemExit(f'seed {seed} step {i}: name {which} {k}: '
                                         f'{a.name(which, k)!r} vs {b.name(which, k)!r}')
    return len(wa.trace)


def main():
    seeds = range(int(sys.argv[1]) if len(sys.argv) > 1 else 200)
    calls = 0
    for seed in seeds:
        calls += run(seed)
    print(f'{len(seeds)} plans: zInput matches the original ({calls} calls out)')


if __name__ == '__main__':
    main()
