"""PM's valve builder 0x4f5f50 and its actions against the exe: every shipped valves.zrd and crafted
records built in order, each valve set through values and the timers ticked, with every call out, every
valve's value and the words written on the brains and targets compared. The world is fake: a name is an
object answering for its own vehicle, controls, brain and mech.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_ai_valves.py [archives]
"""
import ctypes
import struct
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools' / 'analysis'))
import pm_floors  # noqa: E402
import test_pm_ai_nodes as nodes  # noqa: E402
import test_pm_zar  # noqa: E402
import test_pm_zreader as zr  # noqa: E402
import zrd  # noqa: E402

PM = nodes.PM
PARSE, BUILD, SET, TICK = 0x4f38e0, 0x4f5f50, 0x4e40d0, 0x4e4200
HEAD, NOW = 0x62fbe4, 0x62da90
SOUND, PLAY, ANIM, RUN = 0x590a70, 0x58fc70, 0x524b50, 0x521870
TARGET, ENTITY, POWER, ABORT, COMPLETE, REPORT = 0x4e77d0, 0x5cae90, 0x41dab0, 0x492480, 0x492460, 0x406cb0
WORDS = (0x20, 0x28, 0x2c, 0x3c, 0x40, 0x44, 0x64, 0x68, 0x6c, 0x70, 0x98, 0x9c, 0xa0, 0xc0, 0xc4, 0xc8, 0xfc)


class World(zr.Emulated):
    def install(self):
        self.log = []
        self.ids, self.sounds, self.anims = {}, {}, {}
        self.w32 = lambda a, v: self.cpu._uc.mem_write(a, struct.pack('<I', v & 0xffffffff))
        head = self.alloc(0xc)
        self.w32(head, head)
        self.w32(head + 4, head)
        self.w32(HEAD, head)
        self.head = head
        self.stand(0x5cee12, lambda c: 0)
        self.stand(REPORT, lambda c: self.log.append(f'report {c.arg(2):x}') or 0)
        self.stand(SOUND, lambda c: self.named(self.sounds, c.arg(0), b'nosound'))
        self.stand(ANIM, lambda c: self.named(self.anims, c.arg(0), b'noanim'))
        self.stand(PLAY, lambda c: self.log.append(f'sound {c.arg(0):x}') or 0)
        self.stand(RUN, lambda c: self.log.append(f'anim {c.arg(0):x}') or 0)
        self.stand(TARGET, lambda c: self.entity(c.arg(0)))
        self.stand(ENTITY, lambda c: self.entity(c.arg(0)), 4)
        self.stand(POWER, lambda c: self.log.append(f'power {self.ord[c.ecx]:x} {c.arg(0) & 0xff:x}') or 0, 4)
        self.stand(ABORT, lambda c: self.log.append('abort') or 0)
        self.stand(COMPLETE, lambda c: self.log.append('complete') or 0)
        self.ord = {}
        self.vt = self.alloc(0x200)
        same = self.cpu.code(b'\x89\xc8\xc3')  # mov eax, ecx; ret
        for slot in (0x2c, 0x48, 0x14c, 0x58, 0x18):
            self.w32(self.vt + slot, same)
        self.w32(self.vt + 0x78, self.slot(lambda c: self.log.append(f'destroy {self.ord[c.ecx]:x}') or 0, 0))
        self.w32(self.vt + 0xe8, self.slot(self.place, 8))
        self.w32(self.vt + 4, self.slot(lambda c: self.log.append(f'net {self.ord[c.ecx]:x} {self.ord[c.arg(0)]:x}') or 0, 4))

    def stand(self, addr, fn, pop=0):
        if addr in self.cpu.stubs:
            self.cpu.stubs[addr] = (fn, pop)
        else:
            self.native_stub(addr, fn, pop)

    def slot(self, fn, pop):
        code = self.cpu.code(b'\xc3')
        self.cpu.stub_function(code, fn, pop)
        return code

    def place(self, c):
        p = c.arg(0)
        words = struct.unpack('<III', bytes(self.cpu._uc.mem_read(p, 12)))
        self.log.append(f'place {self.ord[c.ecx]:x} {words[0]:x} {words[1]:x} {words[2]:x} {c.arg(1):x}')
        return 0

    def named(self, table, a, none):
        s = self.cstr(a) if a else b''
        if not a or s.startswith(none):
            return 0
        return table.setdefault(s, len(table) + 1)

    def entity(self, a):
        s = self.cstr(a) if a else b''
        if not a or s.startswith(b'nobody'):
            return 0
        if s not in self.ids:
            o, order = self.alloc(0x400), self.alloc(0x100)
            self.w32(o, self.vt)
            self.w32(o + 0x13c, order)  # one order: its net is the brain itself
            self.w32(o + 0x144, order + 0x84)
            self.w32(order + 0xc, o)
            self.ids[s] = o
            self.ord[o] = len(self.ids)
        return self.ids[s]

    def valves(self):
        out, q = [], self.r32(self.head)
        while q != self.head:
            out.append(self.r32(q + 8))
            q = self.r32(q)
        return out

    def dump(self):
        o = 'values'
        for v in self.valves():
            cell = self.r32(v + 4)
            value = self.call(self.r32(self.r32(cell) + 4), ecx=cell)
            o += f" {self.cstr(self.r32(v)).decode('latin1')}={struct.unpack('<i', struct.pack('<I', value))[0]}"
        self.log.append(o)
        o = 'words'
        for name in sorted(self.ids):
            for off in WORDS:
                w = self.r32(self.ids[name] + off)
                if w:
                    o += f" {name.decode('latin1')}+{0xdc if off == 0xfc else off:x}={w:x}"
        self.log.append(o)

    def run(self, script):
        root = self.call(zr.LOAD, args=[self.string('valves.zrd'), 0, 0])
        lst = self.r32(root + 4)
        if self.r32(lst + 4) > 1 and self.r32(lst + 8) == 4:
            lst = self.r32(lst + 12)
        r = self.alloc(0x160)
        head = self.alloc(0x10)
        self.w32(head, head)
        self.w32(head + 4, head)
        self.w32(r + 0x140, head)
        for k in range(1, self.r32(lst + 4) - 1, 2):
            p = lst + 8 * k
            if self.r32(p) != 3 or self.r32(p + 8) != 4:
                continue
            self.call(PARSE, ecx=r, args=[p + 8])
            if not self.call(BUILD, args=[self.r32(p + 4), r]):
                self.log.append('no valve')
        self.dump()
        for op in script:
            if op[0] == 'set':
                for v in self.valves():
                    if self.cstr(self.r32(v)).decode('latin1') == op[1]:
                        self.call(SET, ecx=v, args=[op[2]])
            else:
                self.w32(NOW, struct.unpack('<I', struct.pack('<f', op[1]))[0])
                self.call(TICK)
            self.dump()
        return self.log


def names(data):
    out = []
    for n, start, _ in zrd.entries(data):
        if n.lower() == 'valves.zrd':
            v = zrd.node(data, start)[0]
            top = v[0] if len(v) == 1 and isinstance(v[0], list) else v
            for k in range(0, len(top) - 1, 2):
                if isinstance(top[k], str) and isinstance(top[k + 1], list):
                    out.append(top[k])
    return out


def script_of(valves, values):
    ops, t = [], 0.0
    for i, n in enumerate(valves):
        for v in values:
            ops.append(('set', n, v))
        if i % 8 == 7:
            t += 7.0
            ops.append(('tick', t))
    ops.append(('tick', t + 1000.0))
    return ops


def text(ops):
    return ' '.join(f'set {o[1]} {o[2]}' if o[0] == 'set' else f'tick {o[1]}' for o in ops)


def crafted():
    tags = [('sound', ['snd', 1]), ('sound_once', ['snd1', 1]), ('animation', ['an', 1]), ('animation_once', ['an1', 1]),
            ('scan_aim', ['u1', 1, 1]), ('scan_hit', ['u2', 0, 1]), ('scan_time', ['u3', 1, 1]), ('canleave', ['u4', 1, 1]),
            ('retreat_dist', ['u5', 0, 1]), ('retreat_time', ['u6', 1, 1]), ('retreat_health', ['u7', 0, 1]),
            ('shutdown', ['u8', 1, 1]), ('destroy', ['u9', 1]), ('setteam', ['u10', 2 | 3 << 5, 1]),
            ('attack_strategy', ['u11', 4, 1]), ('teleport', ['u12', [1.5, 2.5, 3.5], 45.0, 1]),
            ('leaveNetRange', ['u13', 3.0, 1]), ('attack_radius', ['u14', 4.0, 1]), ('approachRange', ['u15', 5.0, 1]),
            ('minRange', ['u16', 6.0, 1]), ('scan_delay_min', ['u17', 7.0, 1]), ('scan_delay_max', ['u18', 8.0, 1]),
            ('scan_duration_min', ['u19', 9.0, 1]), ('scan_duration_max', ['u20', 10.0, 1]), ('missionreset', [1]),
            ('missionabort', [1]), ('missioncomplete', [1]), ('update', ['other']), ('delayupdate', ['later', 2.5])]
    rec = ['all', [x for t in tags for x in t]]
    for i, (k, v) in enumerate(tags):  # each alone on values of their own, with a name nobody answers to
        blank = k in ('shutdown', 'destroy', 'setteam', 'attack_strategy', 'teleport')
        w = [(f'nobody{i}' if blank else f'e{i}') if isinstance(x, str) else x for x in v]
        if isinstance(w[-1], int):
            w[-1] = 2 + i
        rec += [f'one_{i}', [k, w]]
    rec += ['value_set', ['value', [3]], 'nosound_v', ['sound', ['nosound1', 1]], 'noanim_v', ['animation', ['noanim1', 1]]]
    for i, k in enumerate(('any_nonzero', 'any_zero', 'all_nonzero', 'all_zero')):
        rec += [f'cond_{i}', [k, [], 'namelist', ['m1', 'm2'], 'sound', ['c', 1]]]
    rec += ['bad_cond', ['any_zero', []], 'bad_list', ['namelist', ['m1']], 'conflict', ['value', [1]],
            'conflict', ['any_zero', [], 'namelist', ['m1']]]
    return nodes.encode(rec)


def compare(p, buf, rel, data, ops):
    with tempfile.TemporaryDirectory() as d:
        (Path(d) / rel).parent.mkdir(parents=True, exist_ok=True)
        (Path(d) / rel).write_bytes(data)
        emu = World()
        emu.install()
        emu.files[test_pm_zar.key(rel)] = bytearray(data)
        if not emu.call(zr.OPEN_ARCHIVE, args=[emu.string(rel.replace('/', '\\')), 1]):
            raise SystemExit(f'{rel}: PM does not open it')
        theirs = emu.run(ops)
        if p.probe_pm_zrdr_load(d.encode(), rel.encode(), b'valves.zrd') != 1:
            raise SystemExit(f'{rel}: the port does not open it')
        n = p.probe_pm_ai_valve_run(text(ops).encode(), buf, len(buf))
        ours = buf.value.decode('latin1').splitlines() if n >= 0 else ['probe failed']
    if theirs == ours:
        return len(theirs), False
    print(f'{rel}: {len(theirs)} and {len(ours)} lines')
    for k, (a, b) in enumerate(zip(theirs, ours)):
        if a != b:
            print(f'  line {k}\n  exe  {a[:700]}\n  port {b[:700]}')
            break
    return len(theirs), True


def main():
    limit = int(sys.argv[1]) if len(sys.argv) > 1 else 10 ** 6
    p = test_pm_zar.load_pm_probe()
    p.probe_pm_zrdr_load.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
    p.probe_pm_ai_valve_run.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
    buf = ctypes.create_string_buffer(1 << 25)
    files = lines = bad = 0
    archives = [a for a in sorted(PM.glob('zbd/*/reader*.zbd')) if any(
        n.lower() == 'valves.zrd' for n, _, _ in zrd.entries(a.read_bytes()))]
    for path in archives[:limit]:
        data = path.read_bytes()
        rel = path.relative_to(PM).as_posix()
        n, wrong = compare(p, buf, rel, data, script_of(names(data), (1, 0, 2, 0, 1)))
        files += 1
        lines += n
        bad += wrong
    like = archives[0].read_bytes()
    data = nodes.crafted_archive(crafted(), like)
    made = names(data)
    n, wrong = compare(p, buf, 'crafted.zbd', data, script_of(list(dict.fromkeys(made + ['m1', 'm2', 'other', 'later'])), list(range(40)) + [1, 0, 1]))
    bad += wrong
    pm_floors.floors(valve_files=(files, min(limit, 37)), crafted_records=(len(made), 41))
    print(f'{files} valves.zrd and a crafted one, {len(made)} crafted records; {lines + n} lines of log: {bad} files differ')
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
