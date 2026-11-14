"""Pirate's Moon's texture fetch 0x5327d0 never runs its clear of transparent texels (0x532740, MW3's
clear_transparent): its flag [0x66a27c] is set before the lookup and cleared only when both lists miss, when there is
nothing to clear. Every lookup outcome (found in the current list, in the other after the switch, in neither), with
and without the second argument (skip the other list), on alpha and other textures: the clear is never called, the
answer is the found texture, the list switches on a first miss. The port's fetch_image leaves the clear out
(pm_fetch_keeps_transparent_colours checks it on PM's data).

    .venv/bin/python -P tests/differential/test_pm_fetch_clear.py
"""
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_pm_zar  # noqa: E402

FETCH, FIND, CLEAR, FLAG, LIST = 0x5327d0, 0x532570, 0x532740, 0x66a27c, 0x66a278


def main():
    z = test_pm_zar.Emulated()
    u = z.cpu._uc
    st = {}
    tex = z.alloc(0x40)

    def find(c):
        r = st['answers'][st['calls']] if st['calls'] < len(st['answers']) else 0
        st['calls'] += 1
        return r
    z.native_stub(FIND, find, 4)  # thiscall on the list, the name popped
    z.native_stub(CLEAR, lambda c: st.update(cleared=st['cleared'] + 1) or 0, 0)
    bad = runs = 0
    for answers in ([tex], [0, tex], [0, 0]):
        for skip in (0, 1):
            for flags in (0x02, 0x00, 0x0b):
                for lst in (0, 1):
                    u.mem_write(tex + 9, bytes([flags]))
                    u.mem_write(tex + 0x14, struct.pack('<I', 0x1000))
                    u.mem_write(LIST, struct.pack('<I', lst))
                    st.update(answers=answers, calls=0, cleared=0)
                    r = z.call(FETCH, args=(z.alloc(8), skip))
                    lst_after = struct.unpack('<I', bytes(u.mem_read(LIST, 4)))[0]
                    want = tex if answers[0] or (not skip and answers[1:] and answers[1]) else 0
                    switched = (lst + 1) % 2 if (not answers[0] and not skip) else lst
                    runs += 1
                    if st['cleared'] or r != want or lst_after != switched:
                        bad += 1
                        print(f'FAIL answers {answers} skip {skip} flags {flags:#x} list {lst}: cleared {st["cleared"]}, '
                              f'answer {r:#x} (want {want:#x}), list {lst_after} (want {switched})')
    print(f'{runs} fetches, the clear never called; {bad} differ')
    sys.stdout.flush()
    os._exit(1 if bad else 0)


if __name__ == '__main__':
    main()
