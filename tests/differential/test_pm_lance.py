"""The lance's fit when a load ends, PM's 0x407c20 and 0x4079b0, against the port's lance_fit_game and lance_fit. The rule: in a
multiplayer game (3) no fit at all;
standalone (5) and in Instant Action (2) every slot keeps its mech and loadout whatever the store holds; in a campaign (1) a
slot's mech must be in the store, else the next of the 25 that is, and it comes out of the store. MW3's rule was only for
Op 4 Mission 6's lancemates, and MW3's test can't load here.

Stood in on the exe's side: the slot's set_config 0x434880 and the loadout's copy 0x425970, whose store side needs the weapons'
whole tables, so the loadouts' parts aren't compared: which mech, whether a loadout, and the mechs left in the store are.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_lance.py
"""
import ctypes
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

FIT, STORE_CTOR, ADD_MECH, MISSION = 0x407c20, 0x473170, 0x473e70, 0x62c40c


class Bench(tc.Emulated):
    def __init__(self, table):
        super().__init__(table)
        c = self.cpu
        self.lu = c._uc
        self.dfns = {}
        self.slots_ = []
        a = self.alloc
        self.mission_, self.game_, self.gvt = a(0x80), a(0x40), a(0x80)
        self.lw(MISSION, self.mission_), self.lw(self.mission_ + 0x6c, self.game_), self.lw(self.game_, self.gvt)
        self.kind = 1
        self.vstub(self.gvt + 0x28, lambda k: self.kind)
        self.vstub(self.gvt + 0x38, lambda k: len(self.slots_))
        self.vstub(self.gvt + 0x30, lambda k: self.slots_[k.arg(0)], 4)
        self.native_stub(0x407900, lambda k: self.store_)
        self.native_stub(0x407960, lambda k: 0)
        self.native_stub(0x4867f0, lambda k: self.dfns.get(k.arg(0), 0))
        self.native_stub(0x434880, self.set_config, 12)
        self.native_stub(0x5bfdd0, lambda k: self.lw(k.ecx + 4, k.arg(0)), 4)
        self.native_stub(0x425970, lambda k: k.arg(1))
        self.native_stub(0x425d50, lambda k: 0)

    def lw(self, a, v):
        self.lu.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def vstub(self, slot, fn, pop=0):
        addr = self.cpu.code(b'\xc3')
        self.lw(slot, addr)
        self.native_stub(addr, fn, pop)

    def set_config(self, k):
        self.lw(k.ecx + 8, k.arg(0))
        return 1

    def ls32(self, a):
        return struct.unpack('<i', bytes(self.lu.mem_read(a, 4)))[0]


def main():
    rel = 'zbd/reader.zbd'
    data = (ROOT / 'game' / 'pm' / rel).read_bytes()
    table = tc.labs(data)
    emu = Bench(table)
    emu.files[test_pm_zar.key(rel)] = bytearray(data)
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
    configs = {}
    for index, mech in enumerate(md.MECHS):
        fname = f'dfn_{mech}.zrd'
        root = emu.call(md.zr.LOAD, args=[emu.string(fname), 0, 0])
        dfn = emu.alloc(md.SIZE)
        emu.call(md.CTOR, ecx=dfn, args=[index, root])
        emu.call(md.READ, ecx=dfn)
        emu.dfns[index] = dfn
        cl = emu.r32(dfn + 0x20)
        first = emu.r32(cl + 0xc)
        configs[index] = [emu.r32(first + 4 * k) for k in range((emu.r32(cl + 0x10) - first) // 4)]
        if p.probe_pm_cfg_load(str(ROOT / 'game' / 'pm').encode(), fname.encode(), index) != len(configs[index]):
            raise SystemExit(f'{mech}: the loadouts differ')
        p.probe_pm_cfg_cache(index)
    rng = random.Random(0x407c20)
    trials = moved = none = kept = 0
    n_mechs = len(md.MECHS)
    out = (ctypes.c_int32 * 256)()
    this = emu.alloc(0x100)  # 0x407900 and 0x407960, its only uses of it, stand in
    for t in range(3000):
        kind = rng.choice([1, 2, 3, 5])  # 3, multiplayer: no fit at all
        emu.kind = kind
        n = rng.randint(1, 4)
        slots = []
        emu.slots_ = []
        for _ in range(n):
            m = rng.randrange(n_mechs) if rng.random() < 0.95 else -1
            k = rng.randrange(len(configs[m])) if m >= 0 and configs[m] and rng.random() < 0.5 else -1
            s = emu.alloc(0x18)
            emu.lu.mem_write(s, bytes(0x18))
            emu.lw(s + 4, m), emu.lw(s + 8, configs[m][k] if k >= 0 else 0)
            emu.slots_.append(s)
            slots += [m, k]
        store_mechs = [rng.randrange(n_mechs) for _ in range(rng.randint(0, 5))]
        emu.store_ = emu.alloc(0x14)
        emu.call(STORE_CTOR, ecx=emu.store_, args=[0])
        for m in store_mechs:
            emu.call(ADD_MECH, ecx=emu.store_, args=[m])
        emu.call(FIT, ecx=this)
        want = []
        for s in emu.slots_:
            cfg = emu.r32(s + 8)
            want += [emu.ls32(s + 4), emu.ls32(cfg + 0x14) if cfg else -2]
        for it in emu.items(emu.store_):
            if emu.call(emu.r32(emu.r32(it) + 0xc), ecx=it) == 0:
                want += [emu.ls32(it + 0x14), emu.ls32(it + 4)]
        a = (ctypes.c_int32 * (2 * n))(*slots)
        b = (ctypes.c_int32 * max(1, len(store_mechs)))(*store_mechs)
        k = p.probe_pm_lance(kind, a, n, b, len(store_mechs), out)
        got = list(out[:k])
        if want != got:
            raise SystemExit(f'{t}: kind {kind} slots {slots} store {store_mechs}\n  exe  {want}\n  port {got}')
        trials += 1
        for i in range(n):
            if slots[2 * i] < 0:
                continue
            if want[2 * i] == -1:
                none += 1
            elif want[2 * i] != slots[2 * i]:
                moved += 1
            elif kind != 1:
                kept += 1
    pm_floors.floors(trials=(trials, 3000), moved=(moved, 200), none=(none, 100), kept=(kept, 1000))
    print(f'ok: {trials} lances fitted alike; {moved} slots moved to another mech, {none} left with none, {kept} kept by the rule')
    # 0x4079b0 itself with the loadout given (its fourth argument; the lobby's return from salvage, 0x49ee20, passes
    # the one it kept): which mech the slot starts from, and the given loadout fitted in place of the slot's.
    p.probe_pm_lance_given.argtypes = [ctypes.c_int32, ctypes.POINTER(ctypes.c_int32), ctypes.POINTER(ctypes.c_int32),
                                       ctypes.c_int32, ctypes.POINTER(ctypes.c_int32)]
    given = 0
    for t in range(1500):
        keep = rng.randrange(2)
        m = rng.randrange(n_mechs) if rng.random() < 0.9 else -1
        k = rng.randrange(len(configs[m])) if m >= 0 and configs[m] and rng.random() < 0.4 else -1
        gm = rng.randrange(n_mechs) if rng.random() < 0.8 else -1
        gk = rng.randrange(len(configs[gm])) if gm >= 0 and configs[gm] else -1
        if gk >= 0:  # its callers' rule: the slot's mech is the given loadout's (the cycle stops only there)
            m = gm
            k = rng.randrange(len(configs[m])) if rng.random() < 0.3 else -1
        s = emu.alloc(0x18)
        emu.lu.mem_write(s, bytes(0x18))
        emu.lw(s + 4, m), emu.lw(s + 8, configs[m][k] if k >= 0 else 0)
        store_mechs = [rng.randrange(n_mechs) for _ in range(rng.randint(0, 5))]
        if gm >= 0 and rng.random() < 0.5:
            store_mechs.append(gm)
        emu.store_ = emu.alloc(0x14)
        emu.call(STORE_CTOR, ecx=emu.store_, args=[0])
        for sm in store_mechs:
            emu.call(ADD_MECH, ecx=emu.store_, args=[sm])
        gcfg = configs[gm][gk] if gk >= 0 else 0
        emu.call(0x4079b0, ecx=s, args=[emu.store_, keep, gcfg])
        cfg = emu.r32(s + 8)
        want = [emu.ls32(s + 4), emu.ls32(cfg + 0x14) if cfg else -2]
        cm = emu.ls32(cfg + 0x14) if cfg else -1  # which of its mech's loadouts (the copy stands in as itself)
        want.append(configs[cm].index(cfg) if cfg and cm in configs and cfg in configs[cm] else -1)
        for it in emu.items(emu.store_):
            if emu.call(emu.r32(emu.r32(it) + 0xc), ecx=it) == 0:
                want += [emu.ls32(it + 0x14), emu.ls32(it + 4)]
        b = (ctypes.c_int32 * max(1, len(store_mechs)))(*store_mechs)
        n_out = p.probe_pm_lance_given(keep, (ctypes.c_int32 * 4)(m, k, gm, gk), b, len(store_mechs), out)
        got = list(out[:n_out])
        if want != got:
            raise SystemExit(f'given {t}: keep {keep} slot {m},{k} given {gm},{gk} store {store_mechs}\n  exe  {want}\n  port {got}')
        given += gcfg != 0
    print(f'ok: 1500 slots fitted alike with a loadout given ({given} given one)')


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
