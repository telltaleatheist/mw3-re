"""MakeShadows on Pirate's Moon's exe (0x563d20, with 0x563b80, 0x5639e0, 0x563b30 and PM's builder 0x563760;
MW3's test_make_shadows.py): each of PM's four worlds read by PM's loader (test_pm_gamez.py's bench) and by the port
(probe_pm_gamez_stack), then MakeShadows with the same stand-in segment query on both sides (PM's 0x501030 stubbed,
cdecl: world, from, to, out). Compared: every used model's polygon colours and a running checksum of the segments.

    cmake --build build --target pmprobe && python3 -P tests/differential/test_pm_make_shadows.py
"""
import ctypes
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_pm_gamez as tg  # noqa: E402
import test_pm_zar  # noqa: E402

MAKE_SHADOWS, SEGMENT_LIST = 0x563d20, 0x501030
BUFFERS = (0xa4e094, 0xa4e098)  # the model draw's morph and transform buffers (MW3's 0x74a288, 0x74a28c)


def mix(h, v):
    return ((h ^ v) * 0x01000193) & 0xffffffff


def main():
    p = test_pm_zar.load_pm_probe()
    p.probe_pm_make_shadows.argtypes = [ctypes.c_char_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p]
    p.probe_pm_make_shadows.restype = ctypes.c_int
    emu = tg.Emulated()
    state = {}

    def query(c):  # cdecl: world, from x, y, z, to x, y, z, out
        frm = [c.arg(1 + i) for i in range(3)]
        for v in frm + [c.arg(4 + i) for i in range(3)]:
            state['hash'] = mix(state['hash'], v)
        state['calls'] += 1
        hit = ((frm[0] ^ (frm[1] * 31) ^ (frm[2] * 131)) & 0xffffffff) % 3 == 0
        emu.cpu._uc.mem_write(c.arg(7), bytes([1 if hit else 0]))
        return 0 if hit else 1
    emu.native_stub(SEGMENT_LIST, query)
    buf = ctypes.create_string_buffer(1 << 26)
    total = 0
    for w in tg.WORLDS:
        path = tg.ZBD / w / 'gamez.zbd'
        emu.stack([path], 5, 6, 5, models=True, nodes=True)
        for a in BUFFERS:
            emu.w32(a, emu.alloc(12 * 0x10000))
        state.update(hash=0x811c9dc5, calls=0)
        emu.call(MAKE_SHADOWS)
        want = bytearray()
        r = emu.r32
        cap, blocks = emu.i32(tg.M_CAPACITY), r(tg.M_BLOCKS)
        for i in range(cap):
            s = r(blocks + 4 * (i // 750)) + 0x68 * (i % 750)
            if emu.signed(r(s + 0xc)) < 1 or emu.signed(r(s + 0x10)) <= 0:
                continue
            polys = r(s + 0x34)
            for k in range(emu.signed(r(s + 0x10))):
                n, col = r(polys + 0x28 * k) & 0x3ff, r(polys + 0x28 * k + 0x1c)
                if col:
                    want += bytes(emu.cpu._uc.mem_read(col, 12 * n))
        stats = (ctypes.c_uint32 * 2)()
        n = p.probe_pm_make_shadows(str(path).encode(), buf, len(buf), stats)
        got = buf.raw[:n]
        if (stats[0], stats[1]) != (state['calls'], state['hash']):
            raise SystemExit(f'{w}: segments {stats[0]} ({stats[1]:#x}) vs {state["calls"]} ({state["hash"]:#x})')
        if got != bytes(want):
            at = next((i for i in range(min(len(got), len(want))) if got[i] != want[i]), None)
            raise SystemExit(f'{w}: colours {len(got)} vs {len(want)} bytes, first difference at {at}')
        print(f'{w}: {state["calls"]} segments, {len(want) // 12} colours as PM\'s')
        total += state['calls']
    print(f'ok: MakeShadows on PM\'s {len(tg.WORLDS)} worlds as PM\'s exe ({total} segments)')


if __name__ == '__main__':
    main()
