"""The saved archives, configs\\<mech>.mw3, against PM's exe: the writer 0x426bd0 and the loader
0x4267f0 (MW3's test_mechsave.py's whole archives, on PM's format). Each mech's loadouts, a random
half marked the player's own, written by both, the bytes compared with each entry's flags,
comment and time masked (the exe fills them from its stack); then the exe's file read by both,
as it is, with another version, with no version entry, cut short, and with no file.
The dialogs on the developer keys are left out (pm-map.md); 0x427e50 and 0x427e80 are read only.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_archives.py
"""
import ctypes
import os
import random
import shutil
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import pm_floors  # noqa: E402
import test_pm_config as tc  # noqa: E402
import test_pm_mechdef as md  # noqa: E402
import test_pm_mechsave as ms  # noqa: E402
import test_pm_zar  # noqa: E402
from original import ROOT  # noqa: E402

SAVE_ALL, LOAD_SAVED, LIST_CTOR, DIR_EXISTS = 0x426bd0, 0x4267f0, 0x4266d0, 0x596ea0
ENTRY = 0x94


def mask(data):
    if len(data) < 12:
        return data
    count = struct.unpack_from('<I', data, len(data) - 8)[0]  # version 2's trailer: version, count, checksum
    out = bytearray(data)
    table = len(data) - 12 - ENTRY * count
    for i in range(count):
        e = table + ENTRY * i
        out[e + 0x48:e + ENTRY] = bytes(ENTRY - 0x48)
    return bytes(out)


class Bench(ms.Bench):
    def stand_ins(self):
        slot = self.cpu.code(b'\xc3')  # sprintf: only configs\\%s.mw3 here
        self.native_stub(slot, self.sprintf)
        self.w32(0x5de658, slot)
        self.native_stub(DIR_EXISTS, lambda c: 1)
        self.cpu.stubs.pop(LOAD_SAVED, None)  # the definitions' reader stands it in; the real one here

    def sprintf(self, c):
        text = self.cstr(c.arg(1)).replace(b'%s', self.cstr(c.arg(2)))
        self.cpu._uc.mem_write(c.arg(0), text + b'\0')
        return len(text)

    def load_list(self, mech):
        lst = self.alloc(0x18)
        self.call(LIST_CTOR, ecx=lst, args=[mech, 0])
        ok = self.call(LOAD_SAVED, ecx=lst) & 0xff
        first, last = self.r32(lst + 0xc), self.r32(lst + 0x10)
        out = [ok, (last - first) // 4]
        for a in range(first, last, 4):
            c = self.r32(a)
            total = 0
            for ch in self.cstr(self.r32(c + 0x18)):
                total = (total * 31 + ch) & 0xffffffff
            tons = self.s32(c + 0x2c)
            out += [struct.unpack('<i', struct.pack('<I', total))[0], self.s32(c + 0x14), self.s32(c + 0x88),
                    self.s32(c + 0x68), self.s32(c + 0x24), tons, self.raw(c + 0x1c, 1)[0]]
            for it in self.items(c):
                kind = self.call(self.r32(self.r32(it) + 0xc), ecx=it)
                off = 0x18 if kind == 3 else 0x14
                out += [kind, self.s32(it + off), self.s32(it + 4), self.s32(it + 0xc)]
            out.append(-999)
        return out


def main():
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
    p.probe_pm_archive_save.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
    p.probe_pm_archive_load.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.POINTER(ctypes.c_int32)]
    for name, lab in table.items():
        p.probe_pm_cfg_lab(name.encode(), *lab)
    rng = random.Random(0x426bd0)
    tmp = Path(os.environ.get('TMPDIR', '/tmp')) / f'pm_archives_{os.getpid()}'
    out = (ctypes.c_int32 * 200000)()
    writes = reads = kept = bad = 0
    try:
        for index, mech in enumerate(md.MECHS):
            name = f'dfn_{mech}.zrd'
            emu.reports = []
            root = emu.call(md.zr.LOAD, args=[emu.string(name), 0, 0])
            dfn = emu.alloc(md.SIZE)
            emu.call(md.CTOR, ecx=dfn, args=[index, root])
            emu.call(md.READ, ecx=dfn)
            cl = emu.r32(dfn + 0x20)
            first, last = emu.r32(cl + 0xc), emu.r32(cl + 0x10)
            count = (last - first) // 4
            if p.probe_pm_cfg_load(str(ROOT / 'game' / 'pm').encode(), name.encode(), index) != count:
                raise SystemExit(f'{mech}: loadouts differ')
            mine = bytes(rng.randrange(2) for _ in range(count))
            for k in range(count):
                emu.wb(emu.r32(first + 4 * k) + 0x1c, mine[k])
            ek = test_pm_zar.key(f'configs\\{mech}.mw3')
            emu.call(SAVE_ALL, ecx=cl)
            theirs = bytes(emu.files[ek])
            shutil.rmtree(tmp, ignore_errors=True)
            tmp.mkdir(parents=True)
            p.probe_pm_archive_save(str(tmp).encode(), mine, count)
            ours = (tmp / 'configs' / f'{mech}.mw3').read_bytes()
            writes += 1
            if mask(theirs) != mask(ours):
                bad += 1
                at = next((i for i, (a, b) in enumerate(zip(mask(theirs), mask(ours))) if a != b), -1)
                print(f'{mech}: written files differ, {len(theirs)} and {len(ours)} bytes, first at {at:#x}')
                continue
            # Another version's bytes: the first entry's data is the version.
            versions = bytearray(theirs)
            struct.pack_into('<I', versions, 0, 15)
            cases = [('as written', theirs), ('other version', bytes(versions)), ('cut', theirs[:len(theirs) // 2]), ('none', None)]
            for what, blob in cases:
                if blob is None:
                    emu.files.pop(ek, None)
                    shutil.rmtree(tmp / 'configs', ignore_errors=True)
                else:
                    emu.files[ek] = bytearray(blob)
                    (tmp / 'configs').mkdir(exist_ok=True)
                    (tmp / 'configs' / f'{mech}.mw3').write_bytes(blob)
                want = emu.load_list(index)
                n = p.probe_pm_archive_load(str(tmp).encode(), index, out)
                got = list(out[:n])
                reads += 1
                kept += want[1]
                if want != got:
                    bad += 1
                    print(f'{mech} {what}: exe {want[:24]}\n  port {got[:24]}')
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    pm_floors.floors(writes=(writes, 25), reads=(reads, 100), kept=(kept, 40))
    print(f'{writes} archives written alike, {reads} read, {kept} loadouts kept: {bad} differ')
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
    os._exit(code)
