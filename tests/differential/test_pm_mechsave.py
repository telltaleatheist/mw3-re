"""PM's saved loadouts against the exe: Config::Save 0x425e40 and Load 0x426000, its magic and
version, and a file from MW3 (no header) renumbered through 0x487740 and 0x487810. As in the port,
Load's Validate is asked without the checksum trap (FIX (trap)).

Not here: MW3's test_mechsave.py's whole archives. The version check is the exe's own read:
0x425660 returns 0x10, written at 0x426bd0, compared at 0x4267f0.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_mechsave.py [cases per loadout]
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

SAVE, LOAD, CTOR = 0x425e40, 0x426000, 0x425670
VALIDATE_PUSH = 0x426286
TMP = str(ROOT / 'build' / 'pm_mechsave.tmp')
ID_AT = 8  # kind, count, then the id


class Bench(tc.Emulated):
    def __init__(self, table):
        super().__init__(table)
        self.cpu._uc.mem_write(VALIDATE_PUSH, b'\x6a\x00')  # push 0: the port drops the trap
        self.files['mem'] = bytearray()

    def handle(self, data):
        self.files['mem'] = bytearray(data)
        self.handles[0x7000] = ['mem', 0]
        return 0x7000

    def save(self, c):
        self.wb(c + 0x1c, 1)
        self.call(SAVE, ecx=c, args=[self.handle(b'')])
        return bytes(self.files['mem'])

    def load(self, name, blob):
        c = self.alloc(0xd8)
        self.cpu._uc.mem_write(c, bytes(0xd8))
        self.call(CTOR, ecx=c, args=[0xffffffff, self.string(name.decode('latin1'))])
        ok = self.call(LOAD, ecx=c, args=[self.handle(blob)]) & 0xff
        tons = struct.unpack('<i', self.raw(c + 0x2c, 4))[0]
        out = [ok, self.s32(c + 0x14), self.s32(c + 0x88), self.s32(c + 0x68), self.s32(c + 0x24), tons]
        for it in self.items(c):
            kind = self.call(self.r32(self.r32(it) + 0xc), ecx=it)
            off = 0x18 if kind == 3 else 0x14
            out += [kind, self.s32(it + off), self.s32(it + 4), self.s32(it + 0xc)]
        return out

    def s32(self, a):
        return struct.unpack('<i', self.raw(a, 4))[0]


def to_mw3(blob):
    """A PM file as MW3's: no header. Its ids are read as MW3's, so any in range will do."""
    return blob[8:]


def main():
    per = int(sys.argv[1]) if len(sys.argv) > 1 else 6
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
    p.probe_pm_cfg_lab.argtypes = [ctypes.c_char_p, ctypes.c_float, ctypes.c_int, ctypes.c_float, ctypes.c_int,
                                   ctypes.c_float]
    p.probe_pm_cfg_load.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
    p.probe_pm_cfg_save.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_int, ctypes.c_char_p]
    p.probe_pm_cfg_blob.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_void_p]
    for name, lab in table.items():
        p.probe_pm_cfg_lab(name.encode(), *lab)
    rng = random.Random(0x426000)
    buf = (ctypes.c_uint8 * (1 << 16))()
    out = (ctypes.c_int32 * 4096)()
    saves = loads = accepted = mw3 = 0

    def check(what, name, blob):
        nonlocal loads, accepted
        want = emu.load(name, blob)
        Path(TMP).write_bytes(blob)
        k = p.probe_pm_cfg_blob(TMP.encode(), name, out)
        got = list(out[:k])
        if want != got:
            raise SystemExit(f'{what}: exe {want[:24]}\n  port {got[:24]}')
        loads += 1
        accepted += want[0]

    for index, mech in enumerate(md.MECHS):
        fname = f'dfn_{mech}.zrd'
        emu.reports = []
        root = emu.call(md.zr.LOAD, args=[emu.string(fname), 0, 0])
        dfn = emu.alloc(md.SIZE)
        emu.call(md.CTOR, ecx=dfn, args=[index, root])
        emu.call(md.READ, ecx=dfn)
        cl = emu.r32(dfn + 0x20)
        first, last = emu.r32(cl + 0xc), emu.r32(cl + 0x10)
        count = (last - first) // 4
        if p.probe_pm_cfg_load(str(ROOT / 'game' / 'pm').encode(), fname.encode(), index) != count:
            raise SystemExit(f'{mech}: the loadouts differ')
        for k in range(count):
            c = emu.r32(first + 4 * k)
            blob = emu.save(c)
            n = p.probe_pm_cfg_save(k, buf, len(buf), TMP.encode())
            got = bytes(buf[:n])
            if got != blob:
                at = next((i for i in range(min(len(got), len(blob))) if got[i] != blob[i]), None)
                raise SystemExit(f'{mech} {k} saved: exe {len(blob)} bytes, port {n}, first difference at {at}')
            saves += 1
            name = emu.cstr(emu.r32(c + 0x18))
            check(f'{mech} {k} as saved', name, blob)
            check(f'{mech} {k} from MW3', name, to_mw3(blob))
            mw3 += 1
            bad = bytearray(blob)
            bad[4] = 15
            check(f'{mech} {k} version 15', name, bytes(bad))
            for _ in range(per):
                # MW3's numbers across their range and past it
                b = bytearray(to_mw3(blob))
                name_len = struct.unpack_from('<I', b)[0]
                count_at = 4 + name_len
                n_items = struct.unpack_from('<I', b, count_at)[0]
                at, ok = count_at + 4, True
                for _ in range(n_items):
                    kind = struct.unpack_from('<I', b, at)[0]
                    if kind in (1, 2):
                        struct.pack_into('<i', b, at + ID_AT, rng.randint(-1, 40))
                    at += 4 + 16 + (4 if kind in (1, 3) else 1 if kind == 4 else 0)
                    if at >= len(b):
                        ok = False
                        break
                if ok:
                    struct.pack_into('<i', b, at, rng.randint(-1, 21))
                check(f'{mech} {k} MW3 numbers', name, bytes(b))
    pm_floors.floors(loadouts=(saves, 113), loads=(loads, 900), accepted=(accepted, 200), from_mw3=(mw3, 113))
    print(f'ok: {saves} loadouts saved identically; {loads} loads agree ({accepted} accepted), {mw3} from MW3\'s format')
    os.unlink(TMP)


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
