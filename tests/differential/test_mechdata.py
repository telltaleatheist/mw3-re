"""A mech type's animation load (0x431a70) against the original x86, for all
19 mechs (docs/movement.md, The gait table).

    cmake --build build && .venv/bin/python tests/differential/test_mechdata.py

The original runs whole in the emulator on the dfn's reader tree (read by
the original zReader) and the real motion archive: each gait (0x431350:
the "anim" and "trans" keys, the stride speeds from the engine, the right
limp mirrored), the forced bounds and their reports, the footfalls of step
and walk and the step's turn onto the walk's footfall, the one-shots
(0x4315d0: 20 frames a second, the collapses at 15 and mirrored, fallb when
missing). The load by name (0x5a9ad0) is redirected to the original script
reader at the archive entry; the canned effects (0x431750) are logged.
Every animation's whole state, the bounds and the log must match; then the
copies each mech gets (0x431fb0, 0x432030), the gait table for a top speed
(0x4329c0), and MASC (0x41df50 with set_max_speed 0x4143c0) on a mech of
the type.
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_mechanim as M  # noqa: E402
from test_zreader import READER, entries as reader_entries  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP  # noqa: E402

probe = M.probe
probe.probe_mdata_load.argtypes = [ctypes.c_char_p, ctypes.c_long, ctypes.c_int, ctypes.c_char_p, ctypes.c_float,
                                   ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int]
probe.probe_mdata_copy.argtypes = [ctypes.c_int, ctypes.c_int]
probe.probe_mdata_scale.argtypes = [ctypes.c_float, ctypes.c_void_p]
probe.probe_mdata_masc.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]

MECHS = ['annihilator', 'avatar', 'blackhawk', 'bushwacker', 'cauldron', 'champion', 'daishi', 'elemental',
         'firefly', 'madcat', 'orion', 'owens', 'puma', 'shadowcat', 'strider', 'sunder', 'supernova', 'thor',
         'vulture']
READ_NODE, LOAD_ALL, LOAD_BY_NAME, DFN, CANNED, SOUND = 0x557180, 0x431a70, 0x5a9ad0, 0x4846a0, 0x431750, 0x41e770
GAIT_COPY, ACTION_COPY, SCALE, MASC = 0x431fb0, 0x432030, 0x4329c0, 0x41df50
CSTR_CTOR, CSTR_DTOR, CSTR_FORMAT, CSTR_ASSIGN = 0x5bb454, 0x5bb468, 0x5bb464, 0x5bb458
MECH_VT, CTL_IFACE_VT, PLAYER = 0x5bcef8, 0x5bd428, 0x60861c
RHANDLE = 0x1234


def f2u(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


class Emulated(M.Emulated):
    def __init__(self, motions, reader):
        super().__init__(motions)
        self.reader, self.motions = reader, motions
        self.table = {name.lower(): start for name, start, _ in M.entries(motions)}
        cpu = self.cpu
        cpu.stub_import(CSTR_CTOR, self.cstr_ctor)
        cpu.stub_import(CSTR_DTOR, lambda c: 0)
        cpu.stub_import(CSTR_FORMAT, self.cstr_format)
        cpu.stub_import(CSTR_ASSIGN, self.cstr_assign, 4)
        cpu.stub_function(DFN, lambda c: self.dfn)
        cpu.stub_function(CANNED, self.canned, 8)
        cpu.stub_function(SOUND, self.sound, 8)
        cpu.uc.hook_add(UC_HOOK_CODE, self.load_by_name, begin=LOAD_BY_NAME, end=LOAD_BY_NAME)
        # push 0x55; mov eax, READ; call eax; ret 8: the script reader in the loader's place
        self.thunk_read = cpu.code(b'\x6a' + bytes([M.HANDLE]) + b'\xb8' + struct.pack('<I', M.SCRIPT_READ) +
                                   b'\xff\xd0\xc2\x08\x00')
        self.events = []

    def read_file(self, c):
        h = c.arg(0)
        if h == M.HANDLE:
            return super().read_file(c)
        assert h == RHANDLE
        buf, n, out = c.arg(1), c.arg(2), c.arg(3)
        chunk = self.reader[self.rpos:self.rpos + n]
        self.rpos += len(chunk)
        c.write(buf, chunk)
        if out:
            c.u32(out, len(chunk))
        return 1

    def report(self, c):
        self.events += [2, c.arg(2)]
        return 0

    def canned(self, c):
        self.events += [1, c.arg(1)]
        return 1

    def sound(self, c):
        self.sounds.append(c.ecx)
        return 0

    def cstr_ctor(self, c):
        p = self.alloc(4)
        c.write(p, b'\0')
        c.u32(c.ecx, p)
        return c.ecx

    def cstr_assign(self, c):
        s = self.cstr(c.arg(0))
        p = self.alloc(len(s) + 1)
        c.write(p, s + b'\0')
        c.u32(c.ecx, p)
        return c.ecx

    def cstr_format(self, c):
        fmt = self.cstr(c.arg(1))
        n = fmt.count(b'%s')
        assert fmt.replace(b'%s', b'') in (b'_', b'_fallb'), fmt
        args = tuple(self.cstr(c.arg(2 + k)) for k in range(n))
        s = fmt % args
        p = self.alloc(len(s) + 1)
        c.write(p, s + b'\0')
        c.u32(c.arg(0), p)
        return 0

    def load_by_name(self, uc, address, size, user):
        esp = uc.reg_read(UC_X86_REG_ESP)
        ret, name = struct.unpack('<II', self.cpu.read(esp, 8))
        key = self.cstr(name)
        script = self.cpu.ecx
        base = key.replace(b'\\', b'/').split(b'/')[-1].split(b'.')[0]
        self.cpu.write(script + 0x11, base + b'\0')
        start = self.table.get(key.decode().lower())
        if start is None:
            uc.reg_write(UC_X86_REG_EAX, 0)
            uc.reg_write(UC_X86_REG_ESP, esp + 12)
            uc.reg_write(UC_X86_REG_EIP, ret)
            return
        self.pos = start
        uc.reg_write(UC_X86_REG_EIP, self.thunk_read)

    def load_type(self, roster, name, start, limp, engine, mech, xl):
        cpu = self.cpu
        self.rpos = start
        root = self.alloc(8)
        cpu.call(READ_NODE, ecx=RHANDLE, edx=root)
        cfg = self.alloc(0xd8)
        cpu.u32(cfg + 0x14, mech), cpu.u32(cfg + 0x24, engine), cpu.write(cfg + 0x28, bytes([xl]))
        arr = self.alloc(4)
        cpu.u32(arr, cfg)
        lst = self.alloc(0x18)
        cpu.u32(lst + 0xc, arr), cpu.u32(lst + 0x10, arr + 4)
        self.dfn = self.alloc(0x30)
        cpu.u32(self.dfn + 8, root), cpu.u32(self.dfn + 0x20, lst)
        self.config = cfg
        md = self.md = self.alloc(0x240)
        sname = self.alloc(len(name) + 1)
        cpu.write(sname, name.encode() + b'\0')
        cpu.u32(md + 0xc, roster), cpu.u32(md + 0x10, sname), cpu.write(md + 0x22c, struct.pack('<f', limp))
        self.events = []
        cpu.call(LOAD_ALL, ecx=md)
        anims = [self.u32(md + 0x8c + 4 * i) for i in range(13)] + [self.u32(md + 0x128 + 4 * i) for i in range(9)]
        trans = list(struct.unpack('<26I', cpu.read(md + 0xc0, 104)))
        return anims, trans, self.events

    def copy(self, kind, i):
        self.cpu.call(GAIT_COPY if kind == 0 else ACTION_COPY, ecx=self.md, stack_args=(i,))
        return self.cpu.uc.reg_read(UC_X86_REG_EAX)

    def scale(self, max_speed):
        buf = self.alloc(104)
        self.cpu.call(SCALE, ecx=self.md, stack_args=(buf, f2u(max_speed)))
        return list(struct.unpack('<26I', self.cpu.read(buf, 104)))

    def masc(self, on, anims, args):
        """args: equipment, masc_time, throttle, max_speed, player."""
        cpu = self.cpu
        m, ctl, vbt, player = self.alloc(0x6e0), self.alloc(0x74), self.alloc(8), self.alloc(0x40)
        cpu.u32(m, MECH_VT)
        cpu.u32(m + 0x37c, ctl), cpu.u32(m + 0x3ec, self.config), cpu.u32(m + 0x684, self.md)
        for i in range(13):
            cpu.u32(m + 0x424 + 4 * i, anims[i])
        cpu.u32(m + 0x3dc, int(args[0]))
        cpu.write(m + 0x574, struct.pack('<f', args[1]))
        cpu.write(m + 0x518, struct.pack('<f', args[3]))
        cpu.u32(ctl + 4, vbt), cpu.u32(vbt + 4, 0x5c), cpu.u32(ctl + 0x5c, 0), cpu.u32(ctl + 0x60, CTL_IFACE_VT)
        cpu.u32(ctl + 0x70, m)
        cpu.write(ctl + 0x10, struct.pack('<f', args[2]))
        cpu.u32(player + 0x14, m if args[4] else 0)
        cpu.u32(PLAYER, player)
        self.sounds = []
        cpu.call(MASC, ecx=m, stack_args=(on,))
        r = cpu.read
        out = list(struct.unpack('<2I', r(m + 0x518, 4) + r(m + 0x510, 4)))
        out += list(struct.unpack('<I', r(ctl + 0x10, 4))) + list(struct.unpack('<I', r(m + 0x574, 4)))
        out += list(struct.unpack('<26I', r(m + 0x4a0, 104)))
        for a in anims:
            out += list(struct.unpack('<2I', r(a + 8, 4) + r(a + 0x8c, 4))) if a else [0, 0]
        return out, self.sounds[0] if self.sounds else -1


def main():
    motions = M.MOTION.read_bytes()
    reader = READER.read_bytes()
    dfns = {name: start for name, start, _ in reader_entries(reader)}
    emu = Emulated(motions, reader)
    if not probe.probe_ma_open(str(M.MOTION).encode()):
        raise SystemExit('the port could not open motion.zbd')
    rng = random.Random(1999)
    loaded = copies = masc_runs = worst = 0
    reports = 0
    for roster, name in enumerate(MECHS):
        start = dfns[f'dfn_{name}.zrd']
        limp = M.f32(rng.uniform(4, 15))
        handles = (ctypes.c_int32 * 22)()
        trans = (ctypes.c_float * 26)()
        config = (ctypes.c_int32 * 3)()
        log = (ctypes.c_int32 * 256)()
        n = probe.probe_mdata_load(str(READER).encode(), start, roster, name.encode(), limp, handles, trans, config,
                                   log, 256)
        if n < 0:
            raise SystemExit(f'{name}: the port could not load')
        objs, wtrans, wlog = emu.load_type(roster, name, start, limp, config[0], config[1], config[2])
        if wtrans != [f2u(trans[i]) for i in range(26)]:
            raise SystemExit(f'{name}: gait bounds differ\n  original {wtrans}\n  port {[f2u(x) for x in trans]}')
        if wlog != list(log[:n]):
            raise SystemExit(f'{name}: calls and reports differ\n  original {wlog}\n  port     {list(log[:n])}')
        reports += sum(1 for k in range(0, len(wlog), 2) if wlog[k] == 2)
        for i in range(22):
            o, h = objs[i], handles[i]
            what = f'{name} {"gait" if i < 13 else "one-shot"} {i if i < 13 else i - 13}'
            if (o == 0) != (h < 0):
                raise SystemExit(f'{what}: present {o != 0} in the original, {h >= 0} in the port')
            if not o:
                continue
            kind = 0 if i < 13 else 1
            if i in (1, 13 + 7):  # the right limp, the left collapse: mirrored
                worst = max(worst, M.sync_mirror(emu, o, h, kind))
            w, g = emu.state(o, kind, 1), M.port_state(h, 1)
            if w != g:
                raise SystemExit(f'{what}: state differs, {M.first_diff(w, g)}')
            loaded += 1
        cop = []
        for i in range(22):
            kind, k = (0, i) if i < 13 else (1, i - 13)
            o = emu.copy(kind, k)
            h = probe.probe_mdata_copy(kind, k)
            if (o == 0) != (h < 0):
                raise SystemExit(f'{name} copy {kind} {k}: present {o != 0} / {h >= 0}')
            cop.append((o, h))
            if o:
                w, g = emu.state(o, kind, 1), M.port_state(h, 1)
                if w != g:
                    raise SystemExit(f'{name} copy {kind} {k}: state differs, {M.first_diff(w, g)}')
                copies += 1
        for _ in range(5):
            top = M.f32(rng.uniform(5, 40))
            out = (ctypes.c_float * 26)()
            probe.probe_mdata_scale(top, out)
            if emu.scale(top) != [f2u(out[i]) for i in range(26)]:
                raise SystemExit(f'{name}: the gait table for {top} differs')
        wanims = [cop[i][0] for i in range(13)]
        panims = (ctypes.c_int32 * 13)(*[cop[i][1] for i in range(13)])
        for _ in range(6):
            on = rng.random() < 0.6
            args = [float(rng.choice([0, 1, 1, 2])), rng.choice([0.0, 0.0, 0.001, 3.5]),
                    M.f32(rng.uniform(-1, 1)), M.f32(rng.uniform(5, 40)), float(rng.random() < 0.5)]
            w, ws = emu.masc(int(on), wanims, args)
            out = (ctypes.c_float * 56)()
            ps = probe.probe_mdata_masc(int(on), panims, (ctypes.c_float * 5)(*args), out)
            g = [f2u(out[i]) for i in range(56)]
            if w != g or ws != ps:
                diff = [k for k in range(56) if w[k] != g[k]]
                raise SystemExit(f'{name}: MASC {on} {args} differs at {diff[:6]}, sound {ws} / {ps}')
            masc_runs += 1
    print(f'ok: {len(MECHS)} mech types, {loaded} animations loaded identically ({reports} transition reports), '
          f'{copies} copies, the gait tables and {masc_runs} MASC switches bit for bit; mirrors within {worst:.1e}')


if __name__ == '__main__':
    main()
