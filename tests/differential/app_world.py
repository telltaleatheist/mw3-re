"""The fake world the app and state tests run the original in (docs/game-loop.md).

Every function the app and its states call outside the ported code is stubbed
here with the key the rebuild's hook reports (mech3/app_hooks.h) and the
arguments the original passes (calltrace.Emu.args specs). The modules not
ported are fake objects in the emulator's world region; the rebuild reaches
their fields through peek/poke and gets the original's stub answers replayed
(calltrace.Replay).
"""
import struct
import sys
from pathlib import Path

from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESP

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import Emu, rnd  # noqa: E402

APP = 0x608c88
STATES = {0: 0x608e90, 1: 0x608f58, 2: 0x608f00, 3: 0x608f48, 4: 0x608ef8, 5: 0x608ec0,
          6: 0x608f18, 7: 0x608f38, 8: 0x608ee8, 9: 0x608e70, 10: 0x608ea0}
VTABLES = {0x608e90: 0x5c4078, 0x608f58: 0x5c40c8, 0x608f00: 0x5c5030, 0x608f48: 0x5c40f0,
           0x608ef8: 0x5c4178, 0x608ec0: 0x5c4040, 0x608f18: 0x5c4148, 0x608f38: 0x5c4120,
           0x608ee8: 0x5c3ee8, 0x608e70: 0x5c3980, 0x608ea0: 0x5c40a0}
RENDERERS = (0x5c4068, 0x5c406c, 0x5c4070)

# Exe functions: address -> argument spec. The key is the address.
CALLS = {
    0x575a00: '', 0x40d4d0: '', 0x40cc10: '', 0x40d4c0: 'c', 0x40cbf0: 'c', 0x40d9d0: '',
    0x40d810: 'c', 0x40d480: '', 0x40d4b0: '', 0x40d6a0: '', 0x40d750: '', 0x40d7b0: '',
    0x40d790: '', 0x40d7a0: '', 0x40d7c0: '', 0x40d070: 'c', 0x40ce90: 'c', 0x40cb30: '',
    0x40c420: 'zc', 0x508fb0: 'c',
    0x5899b0: '', 0x480340: '', 0x4803a0: '', 0x575e30: '*c,*d', 0x589a50: 'c',
    0x597d80: 'c', 0x47faa0: 'c', 0x50da50: 'c,d,zs0', 0x4ac150: 's0,s1,s2', 0x4eaab0: 'c,d',
    0x4828d0: '', 0x575f30: 'zc',
    0x598210: 'zc,zd,zs0', 0x51c630: 'c,zd', 0x555cd0: 'c', 0x556170: 'zc', 0x557320: 'zc,d',
    0x5a8b30: '', 0x5090d0: 'c', 0x50d1e0: 'c', 0x50d1f0: 'c', 0x583740: 'c', 0x501f90: 'c',
    0x4f8f60: 'c', 0x516f50: 'zc', 0x50d270: '', 0x4f8f20: '', 0x526510: '', 0x5578f0: '',
    0x5700f0: 'c', 0x576c50: '', 0x583750: '', 0x518790: 'c', 0x56f4b0: 'c',
    0x526750: 'zc,d', 0x557ae0: 'zc,d', 0x542bb0: '', 0x516e90: 'zc,d', 0x5705e0: 'c,zd,s0,s1',
    0x51b0a0: 'c,d', 0x51dc10: '', 0x51ddb0: '', 0x4333f0: 'c', 0x4338a0: 'c,s0',
    0x40f410: 'c,d', 0x4583b0: 'zc', 0x5759a0: '', 0x575d00: 'c', 0x579ab0: 'c',
    0x579c60: 'c,d,s0,s1', 0x564370: 'c',
    0x4785f0: '', 0x414000: '', 0x4844b0: '', 0x4584e0: '', 0x40f7d0: '', 0x5646d0: '',
    0x586c20: '', 0x502020: '', 0x516f00: '', 0x583870: '', 0x50d2b0: '', 0x4f8f40: '',
    0x526780: '', 0x5715d0: 'c', 0x557b10: '', 0x570510: '', 0x576c60: '', 0x51b170: '',
    0x556210: '', 0x579ef0: '', 0x4d72a0: '', 0x555d90: '', 0x5a8bb0: '', 0x51bae0: 'c',
    0x576db0: '', 0x576c70: '', 0x579880: '', 0x576d80: '', 0x576dc0: '', 0x576da0: '',
    0x576d90: '', 0x576cd0: '', 0x576ca0: '', 0x576dd0: 'c', 0x576ce0: 'c', 0x576cf0: 'c',
    0x576d00: '', 0x576d60: '', 0x576de0: '', 0x576e70: '', 0x576ea0: 'c,d,s0,s1',
    0x579250: 'c', 0x579840: 'c', 0x57a3c0: 'c', 0x577cb0: '', 0x579af0: '', 0x579b10: '',
    0x557f90: 'c,d,s0,s1', 0x558290: 'c,d,s0,s1', 0x558070: 'c', 0x557b20: '', 0x558010: 'c',
    0x558610: '',
    0x59fb80: 'c', 0x59fa10: 'c', 0x59fa60: 'c', 0x59fa40: 'c', 0x48ee00: 'c',
    0x598d80: 'c,zs0,zs1', 0x5996d0: 'c', 0x599710: 'c', 0x599750: 'c,s0', 0x598d00: 'c',
    0x51b360: 'cl', 0x51a9b0: 'c', 0x51a300: '', 0x51b1c0: '', 0x51b310: '', 0x51b340: '',
    0x51b560: '', 0x51b4f0: 'c,d', 0x51b790: 's0,s1', 0x4ab6b0: 'c',
    0x572c90: '', 0x572d70: 'c', 0x572c30: '', 0x573040: '', 0x4ac290: 'c,d',
    0x56dfb0: 'c,d', 0x56f6f0: 'zc', 0x56e3d0: 'c,s0', 0x56eb10: '', 0x56f0a0: 'c',
    0x56f190: 'c', 0x5716a0: '', 0x56f520: 'zc', 0x56e9f0: 'c', 0x56f530: 'zc',
    0x4788b0: 'zc', 0x478250: 'c,s0', 0x40a0f0: 'c', 0x4783c0: 'c,s0', 0x4783a0: 'c,s0',
    0x478f10: 'c,s0', 0x479440: 'c', 0x479420: 'c', 0x477f50: 'zc', 0x5a2ba0: 'c,s0',
    0x414430: 'c,s0', 0x41ca30: 'c,s0', 0x41cc30: 'c,s0', 0x41ab90: 'c', 0x430aa0: 'c',
    0x43cd20: 'c,s0', 0x4d8610: 'c', 0x4d5ca0: '', 0x47ae10: 'cl,dl', 0x48f470: 'c,s0',
    0x490720: 'c', 0x490880: 'c', 0x4a8cd0: '', 0x40f8d0: '', 0x5a6790: 'c,d',
    0x5a67c0: 's0', 0x4f7d80: '', 0x40ad90: 'c', 0x4f73f0: 'c,d,s0', 0x4b51f0: 'c',
    0x4eb170: 'c,d', 0x4eb6d0: 'c,d', 0x4f3540: 'c', 0x4f3550: 'c', 0x4f7350: 'c,d',
    0x4f9fa0: 'c,d,s0', 0x4fa0b0: 'c,d,s0', 0x526e10: 'c', 0x526a80: '', 0x526a70: 'c',
    0x5674d0: '', 0x567570: '', 0x567580: 'c', 0x5675e0: 'c', 0x5674e0: 'c,s0',
    0x567530: 's0', 0x567dc0: '', 0x566f10: '', 0x4814e0: '', 0x40fac0: '', 0x40e4b0: '',
    0x4e6900: '', 0x4ea740: '', 0x5573d0: 'c', 0x50ace0: 'c', 0x40fef0: 'zc', 0x586ce0: '',
    0x48e2d0: 'c', 0x48ee70: 'c,d,s0,s1,s2,s3,s4', 0x40a7d0: '', 0x40a800: '',
    0x40a830: '', 0x40a820: '', 0x4a5bc0: 'c,s0', 0x40baf0: 'c,zs0,s1', 0x409320: 'c',
    0x47abe0: 'c', 0x409ec0: 'c', 0x409eb0: 'c', 0x408bd0: 'c', 0x404ae0: 'c,s0',
}
# Functions that return a string: the probe returns "<tag><answer>".
STRINGS = {0x576040: 'msg', 0x4af280: 'ver', 0x4af290: 'num', 0x484c50: 'dir'}
# Constructors: the answer is the object made (a fake object, by kind).
CTORS = {0x410700: '', 0x433090: '', 0x486260: 's0', 0x48a6c0: '', 0x488910: '', 0x48aef0: '',
         0x48b120: '', 0x438be0: '', 0x43a640: '', 0x42d9d0: '', 0x49c900: '', 0x4295e0: '',
         0x492750: 's0', 0x4289a0: 's0', 0x405230: 'zs0', 0x47b430: 's0,s1,s2,s3', 0x47d3b0: 's0',
         0x48dfe0: 'zs0,zs1', 0x598cc0: 's0,s1,s2', 0x590920: 'zs0'}
# Imports by IAT slot: (key, spec, pop).
IMPORTS = {
    0x5bb0ec: ('Sleep', 's0', 4), 0x5bb6dc: ('SystemParametersInfoA', 's0,s1', 16),
    0x5bb668: ('rand', '', 0), 0x5bb710: ('MessageBeep', 's0', 4),
    0x5bb728: ('MessageBoxA', 's0,zs1,zs2,s3', 16), 0x5bb72c: ('GetFocus', '', 0),
    0x5bb720: ('UpdateWindow', 's0', 4), 0x5bb724: ('UnregisterClassA', 'zs0', 8),
    0x5bb5ec: ('fflush', '', 0), 0x5bb6e4: ('SetMenu', 's0,s1', 8),
}
# Calls the rebuild leaves out or does without a call (FIX (A), set-up).
SILENT_IMPORTS = {0x5bb6e0: 4, 0x5bb718: 8, 0x5bb71c: 8, 0x5bb070: 4, 0x5bb6d0: 16,
                  0x5bb6d4: 12, 0x5bb6d8: 8}
SILENT_THUNKS = {0x5abdb0: 0, 0x5abdbc: 4, 0x5abee2: 8, 0x5abed6: 0,
                 0x5abeca: 0, 0x5abefa: 4, 0x5abef4: 0, 0x5abda4: 0, 0x5abfa8: 4,
                 0x5abfa2: 0, 0x5ac0b0: 4}


def fbits(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


class World:
    """One test's emulator, its stubs and its fake objects."""

    def __init__(self, trace, seed):
        self.t = trace
        self.seed = seed
        self.r = rnd(seed, 'world')
        e = self.emu = Emu(trace)
        self.queue = []
        self.exited = False
        self.strings = {}
        e.heap_stubs()
        for addr, spec in CALLS.items():
            e.stub(addr, spec=spec)
        for addr, tag in STRINGS.items():
            e.stub(addr, spec='c' if addr in (0x576040, 0x484c50) else '',
                   fn=(lambda tg: lambda em, k, a, p: self._string(em, k, a, p, tg))(tag))
        for addr, spec in CTORS.items():
            e.stub(addr, spec=spec, fn=self._ctor)
        for iat, (key, spec, pop) in IMPORTS.items():
            e.import_stub(iat, key, spec, pop)
        for iat, pop in SILENT_IMPORTS.items():
            e.import_stub(iat, f'silent{iat:x}', '', pop)
            trace.silent.add(f'silent{iat:x}')
        for addr, pop in SILENT_THUNKS.items():
            e.stub(addr, key=f'silent{addr:x}', pop=pop)
            trace.silent.add(f'silent{addr:x}')
        module_state = e.world_alloc(0x40)
        e.stub(0x5abed0, key='silent5abed0', pop=0, fn=lambda em, k, a, p: em._ret(module_state, 0))
        trace.silent.add('silent5abed0')
        e.import_stub(0x5bb0e0, 'ExitProcess', 's0', 4, self._exit)
        e.import_stub(0x5bb73c, 'GetCursorPos', '', 4, self._cursor)
        e.import_stub(0x5bb5f0, 'printf', '', 0, self._printf)
        e.import_stub(0x5bb0f4, 'GetCommandLineA', '', 0, self._command_line)
        for iat, name in ((0x5bb664, 'malloc'), (0x5bb608, 'strtok'), (0x5bb654, 'strncmp'),
                          (0x5bb5e4, 'atol'), (0x5bb65c, 'free'), (0x5bb670, '_strdup')):
            e.import_stub(iat, name, '', 0, getattr(self, '_' + name.strip('_')))
            trace.silent.add(name)
        e.stub(0x5abdaa, key='CString::Format', fn=self._format, pop=0)
        e.import_stub(0x5bb658, 'sprintf', '', 0, self._sprintf)
        e.stub(0x575fd0, fn=self._message_format)
        e.stub(0x5abfae, key='CWnd::CreateEx', spec='c', fn=self._create_ex, pop=44)
        e.stub(0x5abf9c, key='CWnd::SetWindowTextA', spec='c,zs0', pop=4)
        e.stub(0x5abedc, key='AfxRegisterClass', pop=4)
        e.stub(0x590a10, key='title', fn=self._title)
        e.stub(0x575d40, key='disc', fn=lambda em, k, a, p: em._ret(1, 4))
        # FIX (trap): the exe self-check is left out of the rebuild; here it passes.
        e.stub(0x4854c0, key='tamper', fn=lambda em, k, a, p: em._ret(1, 0))
        e.stub(0x49ccb0, spec='c', fn=self._list_head)
        e.stub(0x57a3d0, fn=self._directx)
        e.stub(0x599f70, spec='s0,s1', fn=self._wait_ctor)
        # 0x0049bfd0 is the engine's error report, empty in the release build and
        # shared by every empty virtual: left to run.
        trace.silent |= {'title', 'disc', 'tamper', 'new', 'delete', '@depth'}
        # The queue functions: recorded apart (the rebuild's queue is compared after).
        for addr, typ in ((0x590250, 3), (0x590400, 2), (0x5905a0, 1)):
            e.stub(addr, key=f'queue{typ}', fn=(lambda ty: lambda em, k, a, p: self._queue(em, ty))(typ))
            trace.silent.add(f'queue{typ}')
        # Indirect calls through zVideo's pointers.
        for slot, key, spec in ((0x72f390, '[0x0072f390]', 'c'), (0x72f3ec, '[0x0072f3ec]', ''),
                                (0x72f358, '[0x0072f358]', 'c,d'), (0x72f3f0, '[0x0072f3f0]', '')):
            e.w32(slot, e.new_stub(key, spec, 0))
        e.w32(0x72f38c, e.new_stub('[0x0072f38c]', 'c', 4, self._video_memory))
        # The wait object's methods (on the stack in the original).
        self.wait_vt = e.world_alloc(0x40)
        for off, spec, pop in ((0x8, 's0,s1', 8), (0x4, 's0,s1', 8), (0xc, '', 0)):
            e.w32(self.wait_vt + off, e.new_stub(f'wait.vt+{off:#x}', spec, pop))
        self.vt_cache = {}

    # --- fake objects -------------------------------------------------------
    def vtable(self, slots):
        """A vtable whose slots are 'vt+<slot>' stubs: slots maps slot -> stack args."""
        key = tuple(sorted(slots.items()))
        if key in self.vt_cache:
            return self.vt_cache[key]
        e = self.emu
        vt = e.world_alloc(0x200)
        for slot, nargs in slots.items():
            spec = ','.join(['c'] + [f's{i}' for i in range(nargs)])
            e.w32(vt + slot, e.new_stub(f'vt+{slot:#x}', spec, 4 * nargs))
        self.vt_cache[key] = vt
        return vt

    def obj(self, slots=None, size=0x200, fields=None):
        e = self.emu
        o = e.world_alloc(size)
        if slots:
            e.w32(o, self.vtable(slots))
        for off, v in (fields or {}).items():
            e.w32(o + off, v)
        return o

    # --- stub bodies --------------------------------------------------------
    def _string(self, e, key, a, pop, tag):
        v = e.t.handle(key, a)
        s = f'{tag}{v:08x}'
        if s not in self.strings:
            self.strings[s] = e.world_string(s)
        e._ret(self.strings[s], pop)

    def _ctor(self, e, key, a, pop):
        e.t.handle(key, a)
        o = self.make(key)
        e.t.override(key, o)
        e._ret(o, pop)

    def make(self, key):
        """The object a constructor makes; by default a plain fake with the
        usual slots of the shell screens and menu objects."""
        if key == '0x00590920':  # the frame: the original's own `this`
            return self.emu.uc.reg_read(UC_X86_REG_ECX)
        o = self.obj({0x0: 1, 0x4: 1, 0x8: 1, 0x10: 1, 0x18: 1}, size=0x14000)
        for off in (0x111ec, 0x11494, 0x1173c, 0x119e4, 0x11c8c, 0x11f34, 0x121dc):
            self.emu.w32(o + off, self.vtable({0x30: 0}))
        return o

    def _exit(self, e, key, a, pop):
        e.t.handle(key, a)
        self.exited = True
        e.uc.emu_stop()

    def _cursor(self, e, key, a, pop):
        e.t.handle(key, a)
        esp = e.uc.reg_read(UC_X86_REG_ESP)
        p = e.r32(esp + 4)
        e.w32(p, e.t.handle('@cursor_x', []))
        e.w32(p + 4, e.t.handle('@cursor_y', []))
        e._ret(1, pop)

    def _printf(self, e, key, a, pop):
        esp = e.uc.reg_read(UC_X86_REG_ESP)
        fmt = e.cstr(e.r32(esp + 4))
        args = [fmt] + ([e.cstr(e.r32(esp + 8))] if '%s' in fmt else [])
        e.t.handle('printf', args)
        e._ret(0, 0)

    def _command_line(self, e, key, a, pop):
        e.t.handle(key, a)
        e._ret(e.world_string(self.command_line), 0)

    def _malloc(self, e, key, a, pop):
        esp = e.uc.reg_read(UC_X86_REG_ESP)
        e._ret(e.alloc(e.r32(esp + 4)), 0)

    def _free(self, e, key, a, pop):
        e._ret(0, 0)

    def _strdup(self, e, key, a, pop):
        esp = e.uc.reg_read(UC_X86_REG_ESP)
        e._ret(e.world_string(e.cstr(e.r32(esp + 4))), 0)

    def _strtok(self, e, key, a, pop):
        esp = e.uc.reg_read(UC_X86_REG_ESP)
        s, delim = e.r32(esp + 4), e.cstr(e.r32(esp + 8))
        p = s if s else self.tok_next
        if not p:
            e._ret(0, 0)
            return
        while e.r8(p) and chr(e.r8(p)) in delim:
            p += 1
        if not e.r8(p):
            self.tok_next = 0
            e._ret(0, 0)
            return
        start = p
        while e.r8(p) and chr(e.r8(p)) not in delim:
            p += 1
        if e.r8(p):
            e.w8(p, 0)
            self.tok_next = p + 1
        else:
            self.tok_next = p
        e._ret(start, 0)

    def _strncmp(self, e, key, a, pop):
        esp = e.uc.reg_read(UC_X86_REG_ESP)
        x, y, n = e.r32(esp + 4), e.r32(esp + 8), e.r32(esp + 12)
        for i in range(n):
            cx, cy = e.r8(x + i), e.r8(y + i)
            if cx != cy:
                e._ret(1 if cx > cy else 0xffffffff, 0)
                return
            if cx == 0:
                break
        e._ret(0, 0)

    def _atol(self, e, key, a, pop):
        esp = e.uc.reg_read(UC_X86_REG_ESP)
        s = e.cstr(e.r32(esp + 4)).lstrip()
        i, sign, v = 0, 1, 0
        if s[:1] in '+-' and s:
            sign = -1 if s[0] == '-' else 1
            i = 1
        while i < len(s) and s[i].isdigit():
            v = v * 10 + int(s[i])
            i += 1
        e._ret((sign * v) & 0xffffffff, 0)

    def _format(self, e, key, a, pop):
        esp = e.uc.reg_read(UC_X86_REG_ESP)
        cs, fmt = e.r32(esp + 4), e.cstr(e.r32(esp + 8))
        out, k, i = '', 12, 0
        while i < len(fmt):
            if fmt[i] == '%' and i + 1 < len(fmt) and fmt[i + 1] in 'sd':
                v = e.r32(esp + k)
                k += 4
                out += e.cstr(v) if fmt[i + 1] == 's' else str(v - (1 << 32) if v >> 31 else v)
                i += 2
            else:
                out += fmt[i]
                i += 1
        e.t.handle('CString::Format', [out])
        e.w32(cs, e.world_string(out))
        e._ret(cs, 0)

    def _sprintf(self, e, key, a, pop):
        esp = e.uc.reg_read(UC_X86_REG_ESP)
        out, fmt = e.r32(esp + 4), e.cstr(e.r32(esp + 8))
        s = fmt.replace('%d', '{}').format(*[e.r32(esp + 12 + 4 * i) for i in range(fmt.count('%d'))])
        e.cpu.write(out, s.encode() + b'\0')
        e.t.handle('sprintf', [s])
        e._ret(len(s), 0)

    def _message_format(self, e, key, a, pop):
        esp = e.uc.reg_read(UC_X86_REG_ESP)
        out, mid, arg = e.r32(esp + 4), e.r32(esp + 12), e.r32(esp + 16)
        v = e.t.handle(key, [mid, e.cstr(arg)])
        e.cpu.write(out, f'fmt{v:08x}'.encode() + b'\0')
        e._ret(0, 0)

    def _create_ex(self, e, key, a, pop):
        hwnd = e.t.handle(key, a)
        e.world_w32(a[0] + 0x20, hwnd)
        e._ret(1, pop)
        e.t.override(key, hwnd)

    def _title(self, e, key, a, pop):
        esp = e.uc.reg_read(UC_X86_REG_ESP)
        out = e.r32(esp + 4)
        e.w32(out, e.world_string(''))
        e._ret(out, 4)

    def _list_head(self, e, key, a, pop):
        v = e.t.handle(key, a)
        esp = e.uc.reg_read(UC_X86_REG_ESP)
        out = e.r32(esp + 4)
        e.w32(out, v)
        e._ret(out, 4)

    def _directx(self, e, key, a, pop):
        """FIX (A) in the rebuild: DirectX 6 (0x600) on Windows NT (2)."""
        e.t.handle(key, a)
        e.w32(e.uc.reg_read(UC_X86_REG_ECX), 0x600)
        e.w32(e.uc.reg_read(UC_X86_REG_EDX), 2)
        e._ret(0, pop)

    def _wait_ctor(self, e, key, a, pop):
        e.t.handle(key, a)
        e.w32(e.uc.reg_read(UC_X86_REG_ECX), self.wait_vt)
        e._ret(e.uc.reg_read(UC_X86_REG_ECX), 8)

    def _video_memory(self, e, key, a, pop):
        r = e.t.handle(key, a)
        e.w32(e.uc.reg_read(UC_X86_REG_EDX), e.t.handle('@video_total', []))
        e._ret(r, 4)

    def _queue(self, e, typ):
        esp = e.uc.reg_read(UC_X86_REG_ESP)
        if typ == 1:
            self.queue.append((1, 0, e.r32(esp + 4)))
            e._ret(0, 4)
        else:
            self.queue.append((typ, e.r32(esp + 4), e.r32(esp + 8)))
            e._ret(0, 8)

    # --- running ------------------------------------------------------------
    def call(self, addr, ecx=0, edx=0, stack=()):
        from unicorn import UcError
        from unicorn.x86_const import UC_X86_REG_EIP
        try:
            return self.emu.call(addr, ecx=ecx, edx=edx, stack=stack)
        except RuntimeError:
            if self.exited:
                return None
            raise
        except UcError as err:
            eip = self.emu.uc.reg_read(UC_X86_REG_EIP)
            last = self.t.log[-3:] if self.t.log else []
            raise RuntimeError(f'{addr:#x}: {err} at eip {eip:#x}, after {last}') from err
