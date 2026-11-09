"""Pirate's Moon's network loadout (pm-tests item 34): every dfn's built-in loadouts packed by PM's 0x4274d0 and by the
port (Config::packed), then the packed words unpacked by PM's 0x427830 and by the port (Config::unpacked) and each
saved by its own Save (0x425e40). Compared: the packed words, and the saved bytes. PM's header word sits a bit
higher than MW3's (bit 0 clear; endo steel 2, the engine 3..9, XL 10, ferro 11, the armour's half tons from 12, its
tech 30..31): the port's unpack read MW3's, so a remote mech came with the wrong engine (a Mad Cat's 73 read as 19,
6.3 m/s against 22.5), walked behind its owner's own place and was pulled on each state (the soak's 5.5 m).
Each loadout's header is also changed (the engine across the table, the flags flipped) before both unpack it.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_config_unpack.py

BREAK=1 unpacks the port's words with the engine one bit lower (MW3's place): it must fail.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_pm_mechdef as md  # noqa: E402
import test_pm_mechsave as ms  # noqa: E402
import test_pm_zar  # noqa: E402
from original import ROOT  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

PACK, UNPACK = 0x4274d0, 0x427830
TMP = str(ROOT / 'build' / 'pm_config_unpack.tmp')


def main():
    brk = int(os.environ.get('BREAK', '0'))
    rel = 'zbd/reader.zbd'
    data = (ROOT / 'game' / 'pm' / rel).read_bytes()
    table = ms.tc.labs(data)
    emu = ms.Bench(table)
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
    p.probe_pm_cfg_pack.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_int]
    p.probe_pm_cfg_unpack_save.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p, ctypes.c_int, ctypes.c_char_p]
    for name, lab in table.items():
        p.probe_pm_cfg_lab(name.encode(), *lab)
    rng = random.Random(0x427830)
    words_buf = (ctypes.c_uint32 * 4096)()
    out = (ctypes.c_uint8 * (1 << 16))()
    packs = unpacks = 0

    def unpack_both(what, words):
        nonlocal unpacks
        buf = emu.alloc(4 * len(words) + 16)
        emu.cpu._uc.mem_write(buf, struct.pack(f'<{len(words)}I', *words) + bytes(16))
        emu.call(UNPACK, args=[buf])  # PM's takes the packed form on the stack (MW3's 0x4259c0 in ECX)
        want = emu.save(emu.cpu._uc.reg_read(UC_X86_REG_EAX))
        mine = list(words)
        if brk == 1:  # the engine read from MW3's place, one bit lower
            mine[1] = (mine[1] & ~(0x7f << 3)) | (((mine[1] >> 2) & 0x7f) << 3)
        arr = (ctypes.c_uint32 * len(mine))(*mine)
        n = p.probe_pm_cfg_unpack_save(arr, len(mine), out, len(out), TMP.encode())
        got = bytes(out[:n])
        if got != want:
            at = next((i for i in range(min(len(got), len(want))) if got[i] != want[i]), None)
            raise SystemExit(f'{what} unpacked and saved: exe {len(want)} bytes, port {n}, first difference at {at}')
        unpacks += 1

    for index, mech in enumerate(md.MECHS):
        fname = f'dfn_{mech}.zrd'
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
            emu.call(PACK, ecx=c)
            at = emu.cpu._uc.reg_read(UC_X86_REG_EAX)
            size = emu.r32(at) & 0xffff
            want = list(struct.unpack(f'<{size // 4}I', emu.raw(at, size // 4 * 4)))
            n = p.probe_pm_cfg_pack(k, words_buf, len(words_buf))
            got = list(words_buf[:n])[:len(want)]
            if got != want:
                diff = next((i for i in range(min(len(got), len(want))) if got[i] != want[i]), None)
                raise SystemExit(f'{mech} {k} packed: word {diff}: exe {want[diff] if diff is not None else "-":#x}, '
                                 f'port {got[diff] if diff is not None else "-"}')
            packs += 1
            unpack_both(f'{mech} {k}', want)
            for _ in range(4):  # the header changed: the engine across the table, the flags flipped
                w = list(want)
                if rng.random() < 0.5:
                    w[1] = (w[1] & ~(0x7f << 3)) | (rng.randrange(0, 80) << 3)
                else:
                    w[1] ^= 1 << rng.choice([1, 2, 10, 11, 12, 20, 30, 31])
                unpack_both(f'{mech} {k} changed', w)
    Path(TMP).unlink(missing_ok=True)
    print(f'ok: {packs} loadouts packed as PM packs them; {unpacks} unpacked and saved as PM does')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()
