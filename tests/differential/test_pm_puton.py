"""The lab's put-on against PM's exe (0x455a80, lab_put_on): each shipped loadout, a store of
weapons, ammo, armour of both kinds, sinks and jets, and random rows and parts, with the real
definition's firepoints, loadout, store and item code. The list's rows and rule, the part,
the refresh, the sound and the message text are stood in for and logged on both sides.
Compared after each put: the events, the armour, the loadout's items and the store's.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_puton.py [loadouts per mech]
"""
import ctypes
import random
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import pm_floors  # noqa: E402
import test_pm_config as tc  # noqa: E402
import test_pm_mechdef as md  # noqa: E402
import test_pm_zar  # noqa: E402
from original import ROOT  # noqa: E402

PUT_ON, DFN_OF, OBJECT_AT, PART, REFRESH, SOUND, MESSAGE_FMT, MESSAGE = (
    0x455a80, 0x4867f0, 0x4547e0, 0x457160, 0x44f200, 0x454000, 0x596de0, 0x596e50)
STORE_CTOR, STORE_VT, IAT_RAND = 0x473170, 0x5e5bac, 0x5de668
CS_CTOR, CS_FORMAT, CS_DTOR = 0x5ce8e4, 0x5ce8de, 0x5ce8d8
SCREEN, PILOT, SLOT, STORE, FLAG = 0x629748, 0x629768, 0x629718, 0x62976c, 0x629770
KINDS = {0x5dfc70: 3, 0x5dfc40: 2, 0x5e36b4: 1}


def s32(v):
    return struct.unpack('<i', struct.pack('<I', v & 0xffffffff))[0]


class Bench(tc.Emulated):
    def stand_in(self, addr, fn, pop=0):
        if addr in self.cpu.stubs:
            self.cpu.stubs[addr] = (fn, pop)
        else:
            self.native_stub(addr, fn, pop)

    def ev(self, name, value=0):
        def f(c):
            self.events.append(name)
            return value() if callable(value) else value
        return f

    def stand_ins(self):
        self.events, self.state, self.rows = [], 0, []
        self.answer_rule, self.answer_part = 1, 0
        slot = self.cpu.code(b'\xc3')
        self.native_stub(slot, self.rand)
        self.w32(IAT_RAND, slot)
        self.stand_in(PART, self.ev('part', lambda: self.answer_part))
        self.stand_in(REFRESH, self.ev('refresh True'))
        self.stand_in(SOUND, lambda c: self.events.append(f'sound {c.arg(0)} {c.arg(1):x}') or 0, 8)
        self.stand_in(OBJECT_AT, lambda c: self.rows[s32(c.arg(0))] if 0 <= s32(c.arg(0)) < len(self.rows) else 0, 4)
        self.stand_in(MESSAGE, lambda c: self.string(f'MSG_{c.arg(0)}'))
        self.stand_in(MESSAGE_FMT, self.fmt)
        self.stand_in(CS_CTOR, self.cs_ctor)
        self.stand_in(CS_FORMAT, self.cs_format)
        self.stand_in(CS_DTOR, lambda c: 0)
        self.stand_in(DFN_OF, lambda c: self.dfns.get(c.arg(0), 0))
        # 0x4b8a60's answer with no weapon definition, the port's too (lab_rounds); the rounds aren't compared
        self.stand_in(0x4b8a60, lambda c: 100 if s32(c.arg(1)) > 150 else 20 if s32(c.arg(1)) > 20 else 1)
        self.dfns = {}
        self.list_vt = self.alloc(0x200)
        for off, name in ((0x3c, 'enter'), (0x40, 'leave')):
            self.w32(self.list_vt + off, self.thunk(self.ev(name), 0))
        self.w32(self.list_vt + 0xa8, self.thunk(self.rule, 4))
        self.lst = self.alloc(0x400)
        self.w32(self.lst, self.list_vt)
        screen, mvt = self.alloc(0x1c000), self.alloc(0x100)
        self.w32(SCREEN, screen)
        self.w32(screen + 0x1b4c4, mvt)
        self.w32(mvt + 0x7c, self.thunk(self.message, 0))
        self.slot = self.alloc(0x18)
        game, gvt, pilot = self.alloc(0xd0), self.alloc(0x60), self.alloc(0x84)
        self.w32(game, gvt)
        self.w32(gvt + 0x30, self.thunk(lambda c: self.slot, 4))
        self.w32(pilot + 0x6c, game)
        self.w32(PILOT, pilot), self.w32(SLOT, 0)

    def thunk(self, fn, pop):
        addr = self.cpu.code(b'\xc3')
        self.native_stub(addr, fn, pop)
        return addr

    def rand(self, c):
        self.state = (self.state * 214013 + 2531011) & 0xffffffff
        return (self.state >> 16) & 0x7fff

    def rule(self, c):
        self.events.append(f'rule {s32(c.arg(0))}')
        return self.answer_rule

    def fmt(self, c):
        text = f'F{c.arg(2):x}:,{s32(c.arg(4))}'[:c.arg(1) - 1]
        self.cpu._uc.mem_write(c.arg(0), text.encode('latin1') + b'\0')
        return len(text)

    def cs_ctor(self, c):
        self.w32(c.ecx, self.alloc(0x100))
        return c.ecx

    def cs_format(self, c):  # cdecl (CString*, format): the text as it is
        self.cpu._uc.mem_write(self.r32(c.arg(0)), self.cstr(c.arg(1)) + b'\0')
        return 0

    def message(self, c):
        arg = c.arg(1)
        self.events.append('message ' + self.cstr(arg).decode('latin1'))
        return 0

    def begin(self, config, flag, full, mech):
        self.w32(self.slot + 4, mech), self.w32(self.slot + 8, config)
        self.wb(FLAG, flag)
        self.store = self.alloc(0x14)
        self.call(STORE_CTOR, ecx=self.store, args=[0])
        self.wb(self.store + 0x10, full)
        self.w32(self.lst + 0x220, self.store)
        self.w32(STORE, self.store)

    def store_add(self, kind, a, b, c):
        vt = self.r32(self.store)
        if kind == 1:
            self.call(self.r32(vt + 0xc), ecx=self.store, args=[a, b])
        elif kind == 2:
            self.call(self.r32(vt + 4), ecx=self.store, args=[a, b])
        else:
            self.call(self.r32(vt + 8), ecx=self.store, args=[a, b, c])

    def item_text(self, it):
        k = KINDS.get(self.r32(it), 0)
        head = f'[k{k} n{s32(self.r32(it + 4))} g{s32(self.r32(it + 8))} p{s32(self.r32(it + 0xc))} b{self.raw(it + 0x10, 1)[0]}'
        if k == 3:
            return head + f' id{s32(self.r32(it + 0x18))} t{s32(self.r32(it + 0x1c))}] '
        if k == 1:
            return head + f' id{s32(self.r32(it + 0x14))} t{s32(self.r32(it + 0x18))}] '
        return head + f' id{s32(self.r32(it + 0x14))}] '

    def put(self, config, row, rule, part, seed):
        self.state = seed
        self.answer_rule, self.answer_part = rule, part & 0xffffffff
        self.rows = self.items(self.store)
        self.events = []
        self.call(PUT_ON, ecx=self.lst, args=[row & 0xffffffff])
        tons = struct.unpack('<f', self.raw(config + 0x2c, 4))[0]
        tech, ferro = s32(self.r32(config + 0x30)), self.raw(config + 0x34, 1)[0]
        armour = ','.join(str(s32(self.r32(config + 0x38 + 4 * i))) for i in range(11))
        return (''.join(x + ' ' for x in self.events) + f'\narmour {tons:g} t{tech} f{ferro} {armour} m{self.raw(config + 0x1f, 1)[0]}\n'
                + ''.join(self.item_text(i) for i in self.items(config)) + '\n'
                + ''.join(self.item_text(i) for i in self.items(self.store)) + '\n')


def chassis_rows(emu, probe):
    """The chassis list 0x44a660 with an empty store: the empty row, by type and slot."""
    mission, game, gvt = emu.alloc(0x80), emu.alloc(0x40), emu.alloc(0x60)
    emu.w32(game, gvt), emu.w32(mission + 0x6c, game), emu.w32(0x62c40c, mission)
    typ = [0]
    emu.w32(gvt + 0x28, emu.thunk(lambda c: typ[0], 0))
    pilot = emu.r32(PILOT)
    emu.w32(gvt + 0x30, emu.thunk(lambda c: emu.slot, 4))
    emu.w32(pilot + 0x6c, game)
    added = []
    emu.stand_in(0x42fd80, lambda c: added.append(c.arg(0)) or 0, 8)
    emu.stand_in(0x450a20, lambda c: 0)
    emu.stand_in(0x48be00, lambda c: 0, 4)
    lst, vt = emu.alloc(0x800), emu.alloc(0x200)
    emu.w32(lst, vt)
    emu.w32(vt + 0xac, emu.thunk(lambda c: 0, 0))
    bad = 0
    for t in range(6):
        for slot in range(4):
            typ[0] = t
            emu.w32(SLOT, slot)
            added.clear()
            emu.call(0x44a660, ecx=lst)
            theirs, ours = len(added), probe.probe_pm_chassis_rows(t, slot)
            if theirs != ours:
                bad += 1
                print(f'chassis rows, type {t} slot {slot}: exe {theirs}, port {ours}')
    emu.w32(SLOT, 0)
    return bad


def norm(text):
    # the port's sound is the list's sounds[1], the test's 8; the exe's the index, 1
    return re.sub(r'F([0-9a-f]+):[^,\n]*,', r'F\1:,', text).replace('sound 8 ', 'sound 1 ')


def main():
    per = int(sys.argv[1]) if len(sys.argv) > 1 else 9
    rel = 'zbd/reader.zbd'
    data = (ROOT / 'game' / 'pm' / rel).read_bytes()
    table = tc.labs(data)
    emu = Bench(table)
    emu.stand_ins()
    emu.files[test_pm_zar.key(rel)] = bytearray(data)
    emu.cpu.import_code(md.IAT_FLOOR, md.FLOOR)
    emu.cpu.import_code(0x5bb61c, md.FLOOR.replace(bytes.fromhex('660d0004'), bytes.fromhex('660d0008')))
    if not emu.call(md.zr.OPEN_ARCHIVE, args=[emu.string(rel.replace('/', '\\')), 1]):
        raise SystemExit('PM does not open reader.zbd')
    emu.call(0x475860)
    p = test_pm_zar.load_pm_probe()
    p.probe_pm_cfg_lab.argtypes = [ctypes.c_char_p, ctypes.c_float, ctypes.c_int, ctypes.c_float, ctypes.c_int, ctypes.c_float]
    p.probe_pm_cfg_load.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
    p.probe_pm_cfg_load.restype = ctypes.c_int
    p.probe_pm_cfg_ptr.restype = ctypes.c_void_p
    p.probe_pm_puton_begin.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_int32), ctypes.POINTER(ctypes.c_float)]
    p.probe_pm_puton_put.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_uint32, ctypes.c_char_p, ctypes.c_int]
    for name, lab in table.items():
        p.probe_pm_cfg_lab(name.encode(), *lab)
    rng = random.Random(0x455a80)
    buf = ctypes.create_string_buffer(1 << 16)
    n = bad = racks = 0
    seen = {'weapon': 0, 'armour': 0, 'swap': 0, 'sink': 0, 'jet': 0, 'ammo': 0, 'message': 0}
    weapons = sorted(table) and list(range(0, 0x31))
    for index, mech in enumerate(md.MECHS):
        name = f'dfn_{mech}.zrd'
        emu.reports = []
        root = emu.call(md.zr.LOAD, args=[emu.string(name), 0, 0])
        dfn = emu.alloc(md.SIZE)
        emu.call(md.CTOR, ecx=dfn, args=[index, root])
        emu.call(md.READ, ecx=dfn)
        emu.dfns[index] = dfn
        cl = emu.r32(dfn + 0x20)
        first, last = emu.r32(cl + 0xc), emu.r32(cl + 0x10)
        count = (last - first) // 4
        if p.probe_pm_cfg_load(str(ROOT / 'game' / 'pm').encode(), name.encode(), index) != count:
            raise SystemExit(f'{mech}: loadouts differ')
        vec = emu.r32(dfn + 0x24)
        fps = [emu.r32(a) for a in range(emu.r32(vec + 4), emu.r32(vec + 8), 4)]
        parts = [s32(emu.r32(f + 0x68)) for f in fps]
        radii = [struct.unpack('<f', emu.raw(f + 0x60, 4))[0] for f in fps]
        cparts = (ctypes.c_int32 * max(1, len(parts)))(*parts)
        cradii = (ctypes.c_float * max(1, len(radii)))(*radii)
        for k in range(min(count, per)):
            c, mine = emu.r32(first + 4 * k), p.probe_pm_cfg_ptr(k)
            flag = 1 if rng.random() < 0.08 else 0
            full = 1 if rng.random() < 0.15 else 0
            emu.begin(c, flag, full, index)
            p.probe_pm_puton_begin(mine, flag, full, len(parts), cparts, cradii)
            for _ in range(rng.randrange(5, 14)):
                kind = rng.choice((1, 1, 1, 2, 3, 3, 3, 3))
                if kind == 1:
                    op = (1, rng.choice(weapons), rng.choice((1, 2)), 0)
                elif kind == 2:
                    op = (2, rng.choice(weapons), rng.choice((5, 20, 200)), 0)
                else:
                    op = (3, rng.choice((1, 2, 9, 10, 11, 11, 5, 3)), rng.choice((1, 2)), rng.choice((1, 2, 10, 40)))
                emu.store_add(*op)
                p.probe_pm_puton_store(*op)
            for _ in range(rng.randrange(4, 14)):
                rows = len(emu.items(emu.store))
                row = rng.randrange(-1, rows + 1)
                rule = 0 if rng.random() < 0.08 else 1
                part = rng.choice(parts) if parts and rng.random() < 0.85 else rng.randrange(-1, 16)
                seed = rng.randrange(1 << 31)
                theirs = norm(emu.put(c, row, rule, part, seed))
                got = p.probe_pm_puton_put(row, rule, part, seed, buf, len(buf))
                assert got >= 0
                ours = norm(buf.value.decode('latin1'))
                n += 1
                seen['weapon'] += 'sound' in theirs
                seen['message'] += 'message' in theirs
                if theirs != ours:
                    bad += 1
                    if bad <= 5:
                        print(f'{mech} {k} row {row} rule {rule} part {part}')
                        for a, b in zip(theirs.split('\n'), ours.split('\n')):
                            if a != b:
                                ta, tb = a.split('] '), b.split('] ')
                                print('  exe only ', [x for x in ta if x not in tb][:8], len(ta))
                                print('  port only', [x for x in tb if x not in ta][:8], len(tb))
                        print('   diverged: the rest of this loadout is off too')
                    break
            if k == 0 and parts:  # a rack past 16 weapons, a store that never runs out
                part = parts[0]
                for _ in range(20):
                    emu.store_add(1, 3, 1, 0)
                    p.probe_pm_puton_store(1, 3, 1, 0)
                    rows = emu.items(emu.store)
                    row = next((i for i, it in enumerate(rows) if KINDS.get(emu.r32(it)) == 1), 0)
                    seed = rng.randrange(1 << 31)
                    theirs = norm(emu.put(c, row, 1, part, seed))
                    assert p.probe_pm_puton_put(row, 1, part, seed, buf, len(buf)) >= 0
                    ours = norm(buf.value.decode('latin1'))
                    n += 1
                    seen['message'] += 'message' in theirs
                    racks += 'Only 16' in theirs
                    if theirs != ours:
                        bad += 1
                        print(f'{mech} {k} rack: differs')
                        break
            p.probe_pm_puton_end()
    chassis_bad = chassis_rows(emu, p)
    bad += chassis_bad
    pm_floors.floors(puts=(n, 300), placed=(seen['weapon'], 30), messages=(seen['message'], 5), racks=(racks, 5))
    print(f'{n} puts ({seen["weapon"]} placed, {seen["message"]} with a message, {racks} refused past 16 weapons): {bad} loadouts differ')
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
