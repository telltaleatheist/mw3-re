"""PM's net node reader 0x4ec240 against the exe: every node_NN of every shipped net and region
file, in each mission's archive. New in PM: via ( name number radius ), terminus, static_mfb, jump
through the typed getters. The valves' and sounds' lookups stand in on both sides, compared by
name. The rest of the file's reading (the header, the lists, the map by name) isn't here.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_ai_nodes.py [archives]
"""
import ctypes
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools' / 'analysis'))
import pm_floors  # noqa: E402
import test_pm_zar  # noqa: E402
import test_pm_zreader as zr  # noqa: E402
import zrd  # noqa: E402
from original import ROOT  # noqa: E402

PM = ROOT / 'game' / 'pm'
READ, CTOR, VALVE, SOUND, LOCALIZE = 0x4ec240, 0x4d20c0, 0x4e4100, 0x590a70, 0x596db0
TARGET, TARGET_CTOR = 0x4f1c20, 0x4f1bd0


class Bench(zr.Emulated):
    def stand_ins(self):
        self.names = {}
        self.ids = {}
        self.native_stub(VALVE, self.named)
        self.native_stub(SOUND, self.named)
        self.stand(LOCALIZE, lambda c: c.arg(0))

    def stand(self, addr, fn, pop=0):
        if addr in self.cpu.stubs:
            self.cpu.stubs[addr] = (fn, pop)
        else:
            self.native_stub(addr, fn, pop)

    def named(self, c):
        s = self.cstr(c.arg(0)) if c.arg(0) else b''
        if s not in self.ids:
            a = self.alloc(4)
            self.ids[s] = a
            self.names[a] = s.decode('latin1')
        return self.ids[s]

    def r32s(self, a):
        return struct.unpack('<i', bytes(self.cpu._uc.mem_read(a, 4)))[0]

    def b(self, a):
        return bytes(self.cpu._uc.mem_read(a, 1))[0]

    def h(self, a):
        return f'{self.r32(a):x}'

    def nm(self, a):
        return self.names.get(self.r32(a), '-')

    def s(self, a):
        p = self.r32(a)
        return self.cstr(p).decode('latin1') if p else '-'

    def node(self, key, value):
        r = self.alloc(0x100)
        self.call(CTOR, ecx=r)
        self.call(READ, ecx=r, args=[value])
        i, b, h, nm = self.r32s, self.b, self.h, self.nm
        d = struct.unpack('<h', bytes(self.cpu._uc.mem_read(r + 0x74, 2)))[0]
        line = [key, i(r + 0x18), h(r + 0x1c), i(r + 0x20), h(r + 0x24), i(r + 0x28), h(r + 0x2c), i(r + 0x30), nm(r + 0x34),
                b(r + 0x38), i(r + 0x3c), i(r + 0x40), i(r + 0x44), i(r + 0x48), h(r + 0x4c), i(r + 0x50), i(r + 0x54),
                i(r + 0x58), i(r + 0x5c), i(r + 0x60), i(r + 0x64), b(r + 0x68), b(r + 0x69), b(r + 0x6a), b(r + 0x6b),
                b(r + 0x6c), b(r + 0x6d), b(r + 0x6e), b(r + 0x6f), b(r + 0x70), b(r + 0x71), b(r + 0x72), d, nm(r + 0x78),
                nm(r + 0x7c), b(r + 0x80), h(r + 0x84), b(r + 0x88), b(r + 0x89), h(r + 0x8c), nm(r + 0x90),
                self.names.get(self.r32(r + 0x94), '-'), b(r + 0x98), b(r + 0x99), h(r + 0x9c), i(r + 0xa0), b(r + 0xa4),
                b(r + 0xa5), b(r + 0xa6)]
        text = ' '.join(str(x) for x in line) + ' '
        text += f"{self.s(r + 0xa8)}|{self.s(r + 0xac)}|{self.s(r + 0xb8)}|{self.s(r + 0xb0)}|"
        u = self.r32(r + 0xbc)
        if u:
            for p in range(self.r32(u + 4), self.r32(u + 8), 8):
                text += f'{self.names.get(self.r32(p), "-")}:{self.r32s(p + 4)},'
        text += '|'
        a = self.r32(r + 0xc0)
        if a:
            text += f'{self.names.get(self.r32(a), "-")}:{self.r32s(a + 4)}'
        return text

    def target(self, key, value):
        r = self.alloc(0x60)
        self.call(TARGET_CTOR, ecx=r)
        self.call(TARGET, ecx=r, args=[value, self.string(key)])
        i, h = self.r32s, self.h
        line = [key, i(r), h(r + 8), h(r + 0x18), h(r + 0x1c), h(r + 0x20), h(r + 0x24), h(r + 0x28), h(r + 0x2c), h(r + 0x34), i(r + 0x38),
                f'{self.b(r + 0x3c):x}', h(r + 0x44)]
        text = ' '.join(str(x) for x in line) + ' ' + f"{self.s(r + 4)}|{self.s(r + 0xc)}|{self.s(r + 0x10)}|{self.s(r + 0x14)}|{self.s(r + 0x40)}|"
        v = self.r32(r + 0x30)
        if v:
            for p in range(self.r32(v + 4), self.r32(v + 8), 0xc):
                text += f'{self.names.get(self.r32(p), "-")}:{self.r32s(p + 4)}:{self.r32(p + 8)},'
        return text

    def targets(self, root):
        lst = self.r32(root + 4)
        if self.r32(lst + 4) > 1 and self.r32(lst + 8) == 4:
            lst = self.r32(lst + 12)
        out = []
        n = self.r32(lst + 4)
        for i in range(1, n - 1, 2):
            p = lst + 8 * i
            if self.r32(p) == 3:
                out.append(self.target(self.cstr(self.r32(p + 4)).decode('latin1'), p + 8))
        return out

    def walk(self, root):
        kind, lst = self.r32(root), self.r32(root + 4)
        out = []
        if kind != 4 or self.r32(lst + 4) < 2 or self.r32(lst + 8) != 4:
            return out
        lst = self.r32(lst + 12)  # ( ( "version" ... "node_00" ... ) )
        n = self.r32(lst + 4)
        for i in range(1, n - 1):
            p = lst + 8 * i
            if self.r32(p) == 3:
                k = self.cstr(self.r32(p + 4)).decode('latin1')
                if k.startswith('node_'):
                    out.append(self.node(k, p + 8))
        return out


def main():
    limit = int(sys.argv[1]) if len(sys.argv) > 1 else 10 ** 6
    p = test_pm_zar.load_pm_probe()
    p.probe_pm_ai_nodes.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
    p.probe_pm_ai_targets.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
    p.probe_pm_zrdr_load.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
    buf = ctypes.create_string_buffer(1 << 22)
    files = nodes = bad = vias = tfiles = trecs = special = 0
    for path in sorted(PM.glob('zbd/*/reader*.zbd'))[:limit]:
        rel = path.relative_to(PM).as_posix()
        data = path.read_bytes()
        names = [n for n, _, _ in zrd.entries(data) if n.lower().startswith(('net_', 'reg_'))]
        has_targets = any(n.lower() == 'targets.zrd' for n, _, _ in zrd.entries(data))
        if not names and not has_targets:
            continue
        emu = Bench()
        emu.stand_ins()
        emu.files[test_pm_zar.key(rel)] = bytearray(data)
        if not emu.call(zr.OPEN_ARCHIVE, args=[emu.string(rel.replace('/', '\\')), 1]):
            raise SystemExit(f'{rel}: PM does not open it')
        p.probe_pm_zrdr_load(str(PM).encode(), rel.encode(), (names[0] if names else 'targets.zrd').encode())
        if has_targets:
            root = emu.call(zr.LOAD, args=[emu.string('targets.zrd'), 0, 0])
            theirs = emu.targets(root) if root else []
            n = p.probe_pm_ai_targets(b'targets.zrd', buf, len(buf))
            ours = buf.value.decode('latin1').splitlines() if n >= 0 else []
            tfiles += 1
            trecs += len(theirs)
            special += sum('|' in x and (x.split('|')[4] != '-' or x.split()[10] != '-1') for x in theirs)
            if theirs != ours:
                bad += 1
                if bad <= 5:
                    print(f'{rel} targets: {len(theirs)} and {len(ours)} records')
                    for a, b in zip(theirs, ours):
                        if a != b:
                            print('  exe ', a[:300])
                            print('  port', b[:300])
                            break
        for name in names:
            root = emu.call(zr.LOAD, args=[emu.string(name), 0, 0])
            theirs = emu.walk(root) if root else []
            n = p.probe_pm_ai_nodes(name.encode(), buf, len(buf))
            ours = buf.value.decode('latin1').splitlines() if n >= 0 else []
            files += 1
            nodes += len(theirs)
            vias += sum(' ' in x and '|' in x and x.split('|')[3] != '-' for x in theirs)
            if theirs != ours:
                bad += 1
                if bad <= 5:
                    print(f'{rel} {name}: {len(theirs)} and {len(ours)} nodes')
                    for a, b in zip(theirs, ours):
                        if a != b:
                            print('  exe ', a[:400])
                            print('  port', b[:400])
                            break
    if limit >= 10 ** 6:
        pm_floors.floors(files=(files, 400), nodes=(nodes, 5000), vias=(vias, 500), target_files=(tfiles, 30), specials=(special, 10))
    print(f'{files} files, {nodes} nodes ({vias} with via); {tfiles} targets.zrd, {trecs} records ({special} with PM\'s new keys): {bad} differ')
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    code = 0
    try:
        main()
    except SystemExit as ex:
        if isinstance(ex.code, str):
            print(ex.code)
        code = 0 if ex.code in (None, 0) else 1
    except BaseException:
        import traceback
        traceback.print_exc()
        code = 1
    sys.stdout.flush()
    import os
    os._exit(code)
