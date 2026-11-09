"""Dump the dialog templates (RT_DIALOG) of a PE file: each control's class, id, text, place and style.

    python3 tools/analysis/dlgdump.py game/mw3-1.2/Mech3.exe [id ...]

No dependencies. Reads DLGTEMPLATE and DLGTEMPLATEEX; the built-in classes by ordinal
(0x80 button, 0x81 edit, 0x82 static, 0x83 list box, 0x84 scroll bar, 0x85 combo box).
"""
import struct
import sys

CLASSES = {0x80: 'button', 0x81: 'edit', 0x82: 'static', 0x83: 'listbox', 0x84: 'scrollbar', 0x85: 'combobox'}


def sections(b):
    pe = struct.unpack_from('<I', b, 0x3c)[0]
    n = struct.unpack_from('<H', b, pe + 6)[0]
    opt = struct.unpack_from('<H', b, pe + 20)[0]
    rsrc_rva = struct.unpack_from('<I', b, pe + 24 + 96 + 2 * 8)[0]
    out = []
    at = pe + 24 + opt
    for i in range(n):
        name, vsize, va, rsize, raw = struct.unpack_from('<8sIIII', b, at + 40 * i)
        out.append((va, max(vsize, rsize), raw))
    return out, rsrc_rva


def rva_to_off(secs, rva):
    for va, size, raw in secs:
        if va <= rva < va + size:
            return rva - va + raw
    raise ValueError(hex(rva))


def entries(b, base, off):
    named, ids = struct.unpack_from('<HH', b, off + 12)
    for i in range(named + ids):
        name, data = struct.unpack_from('<II', b, off + 16 + 8 * i)
        yield name, data


def dialogs(b):
    secs, rsrc = sections(b)
    base = rva_to_off(secs, rsrc)
    for t, d in entries(b, base, base):
        if t != 5:
            continue
        for name, d2 in entries(b, base, base + (d & 0x7fffffff)):
            for lang, d3 in entries(b, base, base + (d2 & 0x7fffffff)):
                rva, size = struct.unpack_from('<II', b, base + d3)
                off = rva_to_off(secs, rva)
                yield name, b[off:off + size]


def sz_or_ord(b, at):
    if struct.unpack_from('<H', b, at)[0] == 0xffff:
        return struct.unpack_from('<H', b, at + 2)[0], at + 4
    end = at
    while struct.unpack_from('<H', b, end)[0]:
        end += 2
    return b[at:end].decode('utf-16le'), end + 2


def align4(x):
    return (x + 3) & ~3


def parse(t):
    ex = struct.unpack_from('<HH', t, 0) == (1, 0xffff)
    if ex:
        _, _, help_id, exstyle, style, n, x, y, cx, cy = struct.unpack_from('<HHIIIHhhhh', t, 0)
        at = 26
    else:
        style, exstyle, n, x, y, cx, cy = struct.unpack_from('<IIHhhhh', t, 0)
        at = 18
    menu, at = sz_or_ord(t, at)
    cls, at = sz_or_ord(t, at)
    title, at = sz_or_ord(t, at)
    font = None
    if style & 0x40:  # DS_SETFONT
        pt = struct.unpack_from('<H', t, at)[0]
        at += 6 if ex else 2
        face, at = sz_or_ord(t, at)
        font = (face, pt)
    head = dict(title=title, style=style, rect=(x, y, cx, cy), font=font, menu=menu, cls=cls)
    ctrls = []
    for _ in range(n):
        at = align4(at)
        if ex:
            _, cexstyle, cstyle, cx_, cy_, w, h, cid = struct.unpack_from('<IIIhhhhI', t, at)
            at += 24
        else:
            cstyle, cexstyle, cx_, cy_, w, h, cid = struct.unpack_from('<IIhhhhH', t, at)
            at += 18
        ccls, at = sz_or_ord(t, at)
        ctext, at = sz_or_ord(t, at)
        extra = struct.unpack_from('<H', t, at)[0]
        at += 2 + extra
        ctrls.append(dict(cls=CLASSES.get(ccls, ccls), id=cid, text=ctext, rect=(cx_, cy_, w, h), style=cstyle))
    return head, ctrls


def main():
    b = open(sys.argv[1], 'rb').read()
    want = {int(a, 0) for a in sys.argv[2:]}
    for name, t in sorted(dialogs(b), key=lambda p: p[0]):
        if want and name not in want:
            continue
        head, ctrls = parse(t)
        print('%d "%s" %s style %#x' % (name, head['title'], head['rect'], head['style']))
        for c in ctrls:
            print('  %-9s %5d %-28r %s %#x' % (c['cls'], c['id'], c['text'], c['rect'], c['style']))


if __name__ == '__main__':
    main()
