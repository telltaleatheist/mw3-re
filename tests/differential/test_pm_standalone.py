"""PM's second campaign's game object against the exe (docs/pirates-moon.md, The second campaign): the list 0x40def0 and
its reader of sainfo.zrd 0x40d920 on the shipped readersa files, its order pinned; the mission 0x40e340, what the menu
lists 0x40e4d0, moving on 0x40e510 (its pilot's file written is test_pm_pilot's round and the chain's reload), the
mission's start 0x40e3f0 with the salvage block 0x473690 on every mission, the lineup 0x40e760, 0x40e830, 0x40e6c0.

Messages stand in on both sides: 0x596db0 answers the message's name, 0x596e50 "MSG_<id>", so names are compared and not
the text. A mech with a loadout from the salvage block is compared as the call out (0x4752c0, store_add_mech); the loadout's
parts going in the store are test_pm_config's and Store's: the loadout is empty here, the mech real on both sides.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_standalone.py
"""
import ctypes
import glob
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import pm_floors  # noqa: E402
import test_pm_config as tc  # noqa: E402
import test_pm_mechdef as md  # noqa: E402
import test_pm_zar  # noqa: E402
from original import ROOT  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn.x86_const import UC_X86_REG_ESP  # noqa: E402

LIST, MISSION, UNLOCKED, NEXT, START, SLOT, COUNT, MATE, CTOR = (
    0x40def0, 0x40e340, 0x40e4d0, 0x40e510, 0x40e3f0, 0x40e760, 0x40e830, 0x40e6c0, 0x40e100)
LIST_BEGIN, LIST_END = 0x61884c, 0x618850
# The order the exe sorts the shipped data into (`order` in each sainfo.zrd).
ORDER = [(1, 1), (2, 1), (1, 2), (1, 3), (3, 1), (3, 2), (3, 3), (2, 2), (4, 1), (4, 2), (1, 4)]
FIND_FIRST, FIND_NEXT, FIND_CLOSE, STRCHR, ATOI = 0x5de0b8, 0x5de0b4, 0x5de0b0, 0x5de63c, 0x5de66c


class Bench(tc.Emulated):
    def __init__(self, table):
        super().__init__(table)
        c = self.cpu
        self.sa_uc = c._uc
        empty = self.string('')
        self.sa_empty = empty
        def n(addr, fn, pop=0):  # a stub the bench has is replaced, not hooked twice
            if addr in c.stubs:
                c.stubs[addr] = (fn, pop)
            else:
                self.native_stub(addr, fn, pop)
        n(0x5ce8c6, lambda k: self.alloc(k.arg(0)))
        n(0x5ce8c0, lambda k: 0)
        n(0x5ce8e4, lambda k: (self.sa_w(k.ecx, empty), k.ecx)[1])  # CString()
        n(0x5ce8d8, lambda k: 0)                                 # ~CString
        n(0x5ce8de, self.sa_fmt)                                    # Format
        n(0x5cea70, lambda k: (self.sa_w(k.ecx, self.r32(k.arg(0))), k.ecx)[1], 4)  # CString(CString)
        n(0x5cea10, lambda k: (self.sa_w(k.ecx, self.string(self.cstr(k.arg(0)).decode('latin1'))), k.ecx)[1], 4)
        n(0x596db0, lambda k: k.arg(0))                          # the message of that name: the name
        n(0x596e50, lambda k: self.string('MSG_%d' % k.arg(0)))
        n(0x47af50, lambda k: 0)                                 # the pilot's file opened
        # A salvage mech with a loadout: the mech goes in (0x4752c0, 0x475310 real), its loadout empty here.
        empty_cfg = self.alloc(0x40)
        head = self.alloc(12)
        self.sa_w(head, head), self.sa_w(head + 4, head), self.sa_w(empty_cfg + 8, head)
        n(0x4867f0, lambda k: empty_cfg)
        n(0x426af0, lambda k: empty_cfg, 4)
        c._uc.hook_add(UC_HOOK_CODE, self.sa_add_config, begin=0x4752c0, end=0x4752c0)
        def imp(slot, fn, pop=0):  # PM's own import slots, as they are
            addr = c.code(b'\xc3')
            self.sa_w(slot, addr)
            n(addr, fn, pop)
        imp(FIND_FIRST, self.sa_first, 8)
        imp(FIND_NEXT, self.sa_next, 8)
        imp(FIND_CLOSE, lambda k: 1, 4)
        imp(STRCHR, self.sa_strchr)
        imp(ATOI, lambda k: int(self.cstr(k.arg(0)) or 0))
        self.sa_names, self.sa_at, self.sa_log = [], 0, []

    def sa_w(self, a, v):
        self.sa_uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def sa_fmt(self, k):
        f, out, n, i = self.cstr(k.arg(1)).decode('latin1'), '', 2, 0
        while i < len(f):
            if f[i] != '%':
                out += f[i]
                i += 1
                continue
            out += str(struct.unpack('<i', struct.pack('<I', k.arg(n)))[0])
            n, i = n + 1, i + 2
        self.sa_w(k.arg(0), self.string(out))
        self.sa_last_format = out
        return k.arg(0)

    def sa_strchr(self, k):
        s = self.cstr(k.arg(0))
        i = s.find(bytes([k.arg(1) & 0xff]))
        return k.arg(0) + i if i >= 0 else 0

    def sa_found(self, out):
        self.sa_uc.mem_write(out, bytes(0x140))
        self.sa_uc.mem_write(out + 0x2c, self.sa_names[self.sa_at].encode() + b'\0')
        self.sa_at += 1

    def sa_first(self, k):
        pat = self.cstr(k.arg(0)).decode('latin1').replace('\\', '/').lower()
        d, stem = os.path.split(pat)
        self.sa_names = sorted(os.path.basename(f) for f in glob.glob(str(ROOT / 'game' / 'pm' / d / 'readersa*.zbd')))
        self.sa_at = 0
        if not self.sa_names:
            return 0xffffffff
        self.sa_found(k.arg(1))
        return 0x7777

    def sa_next(self, k):
        if self.sa_at >= len(self.sa_names):
            return 0
        self.sa_found(k.arg(1))
        return 1

    def sa_add_config(self, uc, address, size, user):
        sp = uc.reg_read(UC_X86_REG_ESP)
        mech, name = struct.unpack('<II', bytes(uc.mem_read(sp + 4, 8)))
        self.sa_log += [1, mech] + list(self.cstr(name)) + [0]

    def sa_missions(self):
        b, e = self.r32(LIST_BEGIN), self.r32(LIST_END)
        out = []
        for i in range((e - b) // 4):
            m = self.r32(b + 4 * i)
            out += [self.r32(m + 0x18), self.r32(m + 0x1c), self.r32(m + 0x14)]
        return out

    def sa_store(self, g):
        out = []
        for it in self.items(g + 8 - 8 + 8):
            kind = self.call(self.r32(self.r32(it) + 0xc), ecx=it)
            off = 0x18 if kind == 3 else 0x14
            tech = self.sa_s32(it + 0x18) if kind == 1 else self.sa_s32(it + 0x1c) if kind == 3 else 0
            out += [kind, self.sa_s32(it + off), self.sa_s32(it + 4), self.sa_s32(it + 0xc), tech]
        return out

    def sa_s32(self, a):
        return struct.unpack('<i', bytes(self.sa_uc.mem_read(a, 4)))[0]


def main():
    rel = 'zbd/reader.zbd'
    data = (ROOT / 'game' / 'pm' / rel).read_bytes()
    table = tc.labs(data)
    emu = Bench(table)
    emu.files[test_pm_zar.key(rel)] = bytearray(data)
    for f in glob.glob(str(ROOT / 'game' / 'pm' / 'zbd' / 'c*' / 'readersa*.zbd')):
        emu.files[test_pm_zar.key(os.path.relpath(f, ROOT / 'game' / 'pm'))] = bytearray(Path(f).read_bytes())
    emu.cpu.import_code(md.IAT_FLOOR, md.FLOOR)
    emu.cpu.import_code(0x5bb61c, md.FLOOR.replace(bytes.fromhex('660d0004'), bytes.fromhex('660d0008')))
    if not emu.call(md.zr.OPEN_ARCHIVE, args=[emu.string(rel.replace('/', '\\')), 1]):
        raise SystemExit('PM does not open reader.zbd')
    emu.call(0x475860)
    p = test_pm_zar.load_pm_probe()
    p.probe_pm_cfg_lab.argtypes = [ctypes.c_char_p, ctypes.c_float, ctypes.c_int, ctypes.c_float, ctypes.c_int, ctypes.c_float]
    p.probe_pm_cfg_load.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
    for name, lab in table.items():
        p.probe_pm_cfg_lab(name.encode(), *lab)
    p.probe_pm_cfg_load(str(ROOT / 'game' / 'pm').encode(), b'dfn_atlas.zrd', md.MECHS.index('atlas'))
    buf = (ctypes.c_int32 * 8192)()

    # the list
    want_n = emu.call(LIST)
    want = emu.sa_missions()
    n = p.probe_pm_sa_list(buf)
    got = list(buf[:3 * n])
    if want != got or want_n != n:
        raise SystemExit(f'the list: exe {want_n} {want}\n  port {n} {got}')
    if [(want[3 * i], want[3 * i + 1]) for i in range(n)] != ORDER or [want[3 * i + 2] for i in range(n)] != list(range(1, 12)):
        raise SystemExit(f'the order is not the one pinned: {want}')
    g = emu.alloc(0x5c)
    emu.sa_uc.mem_write(g, bytes(0x5c))
    pilot = emu.alloc(0x90)
    emu.sa_uc.mem_write(pilot, bytes(0x90))
    emu.sa_w(pilot + 0x10, 0xffffffff)
    emu.call(CTOR, ecx=g, args=[pilot])

    def check(what, a, b):
        if a != b:
            if isinstance(a, list) and isinstance(b, list):
                i = next((j for j, (x, y) in enumerate(zip(a, b)) if x != y), min(len(a), len(b)))
                raise SystemExit(f'{what}: lengths {len(a)} {len(b)}, at {i}: exe {a[max(0, i - 5):i + 10]} port {b[max(0, i - 5):i + 10]}')
            raise SystemExit(f'{what}: exe {a} port {b}')

    rng = random.Random(0x40e340)
    moved = trials = 0
    for _ in range(600):  # the mission, moved within the list; what is listed
        i = rng.randint(-3, 14)
        check(f'mission {i}', emu.call(MISSION, ecx=g, args=[i & 0xffffffff]) & 0xff, p.probe_pm_sa_op(0, i))
        check('at', emu.sa_s32(g + 0x38), p.probe_pm_sa_op(6, 0))
        check('its number', emu.r32(emu.r32(g + 0x40) + 0x1c), p.probe_pm_sa_op(9, 0))
        check('its world', emu.r32(emu.r32(g + 0x40) + 0x18) - 1, p.probe_pm_sa_op(10, 0))
        b = rng.randint(0, 13)
        emu.sa_w(g + 0x3c, b)
        p.probe_pm_sa_op(2, b)
        check(f'listed with best {b}', emu.call(UNLOCKED, ecx=g), p.probe_pm_sa_op(1, 0))
        moved += not 0 <= i <= 10
        trials += 1

    starts = with_block = 0
    # Every mission's start, the salvage compared, twice: the second time with the lance's slots in the store.
    for row in list(range(n)) * 2:
        emu.call(MISSION, ecx=g, args=[row])
        p.probe_pm_sa_op(0, row)
        emu.sa_log = []
        emu.call(START, ecx=g)
        p.probe_pm_sa_op(3, 0)
        k = p.probe_pm_sa_log(buf)
        check(f'row {row}: the calls out', emu.sa_log, list(buf[:k]))
        k = p.probe_pm_sa_store(buf)
        check(f'row {row}: the store', emu.sa_store(g), list(buf[:k]))
        check(f'row {row}: slots', emu.call(COUNT, ecx=g), p.probe_pm_sa_op(5, 0))
        starts += 1
        with_block += len(emu.sa_store(g)) > 0

    # The lineup: lancemates of a mission, new and then again (the slot there keeps its own, 0x40e6c0), and the player's.
    index = md.MECHS.index('atlas')
    root = emu.call(md.zr.LOAD, args=[emu.string('dfn_atlas.zrd'), 0, 0])
    dfn = emu.alloc(md.SIZE)
    emu.call(md.CTOR, ecx=dfn, args=[index, root])
    emu.call(md.READ, ecx=dfn)
    cl = emu.r32(dfn + 0x20)
    first = emu.r32(cl + 0xc)
    cfgs = (emu.r32(cl + 0x10) - first) // 4
    p.probe_pm_cfg_at.restype = ctypes.c_void_p
    p.probe_pm_sa_lancemate.argtypes = [ctypes.c_int32, ctypes.c_int32, ctypes.c_int32, ctypes.c_void_p]
    p.probe_pm_sa_player.argtypes = [ctypes.c_int32, ctypes.c_void_p]
    p.probe_pm_sa_lancemate_notake.argtypes = [ctypes.c_int32, ctypes.c_int32, ctypes.c_int32, ctypes.c_void_p]
    emu.call(MISSION, ecx=g, args=[3]), p.probe_pm_sa_op(0, 3)
    emu.call(START, ecx=g), p.probe_pm_sa_op(3, 0), p.probe_pm_sa_log(buf)
    slots = replays = 0

    # 0x434880 (the slot's set_config) and 0x474b40 stand in: the store's side of taking a loadout needs the weapons' whole
    # tables in the emulator; GameSlot::set_config is compared by the campaign's tests. So the store isn't compared here.
    takes = []

    def take(k):
        takes.append((k.arg(0) != 0, k.arg(1) != 0, k.arg(2) & 0xff))
        emu.sa_w(k.ecx + 8, k.arg(0))
        return 1

    emu.native_stub(0x434880, take, 12)
    real_remove = [False]

    def compare_slots(what):
        nonlocal slots
        check(f'{what}: count', emu.call(COUNT, ecx=g), p.probe_pm_sa_op(5, 0))
        for i in range(5):
            s = emu.call(SLOT, ecx=g, args=[i])
            r = p.probe_pm_sa_slot(i, buf)
            if not s:
                check(f'{what}: slot {i} none', -9, r)
                continue
            cfg = emu.r32(s + 8)
            check(f'{what}: slot {i}', [emu.sa_s32(s + 4), emu.sa_s32(cfg + 0x14) if cfg else -2, emu.sa_s32(s + 0x14)], list(buf[:3]))
            slots += 1

    compare_slots('the lineup at first')
    for pass_ in range(2):
        for i in range(3):
            s = emu.alloc(0x18)
            emu.sa_uc.mem_write(s, bytes(0x18))
            emu.call(0x434780, ecx=s, args=[index, 0, 0])
            emu.sa_w(s + 0x14, 100 + 10 * pass_ + i)
            k = (i + pass_) % cfgs
            emu.call(MATE, ecx=g, args=[i, s, emu.r32(first + 4 * k)])
            p.probe_pm_sa_lancemate(i, index, 100 + 10 * pass_ + i, p.probe_pm_cfg_at(k))
        emu.sa_w(g + 0x48, 3 - pass_), p.probe_pm_sa_op(8, 3 - pass_)
        compare_slots(f'lancemates, pass {pass_}')
        takes.clear()
    compare_slots('the lineup stays')
    # A replay's start: the salvage again, less what the lance has (0x474dd0, 0x474b40, real on both sides). The lineup's
    # mechs and loadouts are in the mission's block or not: this mission's has Atlases' parts in part.
    for row in (3, 8, 10):
        emu.call(MISSION, ecx=g, args=[row]), p.probe_pm_sa_op(0, row)
        emu.call(START, ecx=g), p.probe_pm_sa_op(3, 0), p.probe_pm_sa_log(buf)
        emu.call(SLOT, ecx=g, args=[0]), p.probe_pm_sa_slot(0, buf)  # the player's, made on asking
        for i in range(3):  # this mission's lancemates, as above
            s_ = emu.alloc(0x18)
            emu.sa_uc.mem_write(s_, bytes(0x18))
            emu.call(0x434780, ecx=s_, args=[index, 0, 0])
            emu.call(MATE, ecx=g, args=[i, s_, emu.r32(first + 4 * (i % cfgs))])
            p.probe_pm_sa_lancemate_notake(i, index, 0, p.probe_pm_cfg_at(i % cfgs))
        emu.sa_w(g + 0x48, 3), p.probe_pm_sa_op(8, 3)
        k = p.probe_pm_sa_store(buf)
        before = list(buf[:k])
        emu.sa_log = []
        emu.call(START, ecx=g), p.probe_pm_sa_op(3, 0), p.probe_pm_sa_log(buf)
        k = p.probe_pm_sa_store(buf)
        check(f'row {row}: the store less the lance', emu.sa_store(g), list(buf[:k]))
        replays += list(buf[:k]) != []

    # Moving on: each win up to past the last; the best kept across a replay's earlier row.
    emu.call(MISSION, ecx=g, args=[0])
    p.probe_pm_sa_op(0, 0)
    emu.sa_w(g + 0x3c, 0), p.probe_pm_sa_op(2, 0)
    past = 0
    for step in range(20):
        emu.call(START, ecx=g), p.probe_pm_sa_op(3, 0), p.probe_pm_sa_log(buf)
        if step == 5:  # a replay of an earlier row: the best stays
            emu.call(MISSION, ecx=g, args=[1]), p.probe_pm_sa_op(0, 1)
            emu.call(START, ecx=g), p.probe_pm_sa_op(3, 0), p.probe_pm_sa_log(buf)
        a = emu.call(NEXT, ecx=g) & 0xff
        b = p.probe_pm_sa_op(4, 0)
        check(f'step {step}: past the end', a, b)
        check(f'step {step}: at', emu.sa_s32(g + 0x38), p.probe_pm_sa_op(6, 0))
        check(f'step {step}: best', emu.sa_s32(g + 0x3c), p.probe_pm_sa_op(7, 0))
        past += a
    pm_floors.floors(trials=(trials, 600), moved=(moved, 100), starts=(starts, 22), with_block=(with_block, 22), past=(past, 2), slots=(slots, 10), replays=(replays, 3))
    print(f'ok: the list of {n} in the pinned order; {trials} missions set ({moved} moved); {starts} starts, the stores and '
          f'calls identical; moved on past the end {past} times')


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
    os._exit(code)
