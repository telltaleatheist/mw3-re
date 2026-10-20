# twod_vtables.py: the zTwoD classes' vtables, each up to the next one, with the
# slots that differ from a base marked (docs/twod.md). Run from the repo root.
import sys
sys.path.insert(0, 'tools/analysis')
import pe  # noqa: E402

# vtable: (name, base vtable or None)
VT = {
    0x5bc0e0: ('Object2D', None), 0x5cf8c8: ('Image', 0x5bc0e0), 0x5cf948: ('Control', 0x5cf8c8),
    0x5cfaa8: ('Button', 0x5cf948), 0x5cfb38: ('CheckBox', 0x5cf948), 0x5cfbc8: ('ListBox', 0x5cfb38),
    0x5cfc78: ('Dialog', 0x5cfed0), 0x5cfc88: ('Dialog@0x58 (cursor)', 0x5d0da0), 0x5cfd10: ('Layer', None),
    0x5cfd28: ('FontText', 0x5cfed8), 0x5cf9d8: ('Text', 0x5cfd28), 0x5cfe00: ('EditText', 0x5cf9d8),
    0x5cfed0: ('Window', 0x5cfd10), 0x5cfed8: ('TextBase', 0x5bc0e0), 0x5d0da0: ('Cursor', 0x5cf8c8),
    0x5d0e28: ('Movie2D', 0x5bc0e0),
}
ALL = sorted(set(VT) | {0x5cff58, 0x5cfff0, 0x5d0070, 0x5d00f0, 0x5d0170, 0x5d0178, 0x5d01a8, 0x5d0288,
                        0x5d0290, 0x5d0330, 0x5d0360, 0x5d03e0, 0x5d0478, 0x5d04a8, 0x5d0528, 0x5d05e0,
                        0x5d0660, 0x5d06e0, 0x5d0760, 0x5d07e0, 0x5d0860, 0x5d08f0, 0x5d0920, 0x5d09b0,
                        0x5d0a40, 0x5d0ad0, 0x5d0b60, 0x5d0bf0, 0x5d0c80, 0x5d0d10, 0x5d0e28, 0x5d0ea8,
                        0x5bc1b0, 0x5bc190, 0x5bd988})


def slots(va):
    nxt = min([a for a in ALL if a > va] + [va + 0x200])
    out = []
    for i in range((nxt - va) // 4):
        v = pe.u32(va + 4 * i)
        if v is None or not (0x401000 <= v < 0x5b0000):
            break
        out.append(v)
    return out


want = [int(a, 16) for a in sys.argv[1:]] or sorted(VT)
for va in want:
    name, base = VT.get(va, ('?', None))
    s = slots(va)
    b = slots(base) if base else []
    print(f'{va:#x} {name} [{len(s)} slots]' + (f' base {base:#x}' if base else ''))
    for i, v in enumerate(s):
        mark = '' if i < len(b) and b[i] == v else ' *'
        print(f'   +{i*4:#05x} {v:#x}{mark}')
