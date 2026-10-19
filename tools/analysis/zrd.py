"""Lists a .zbd archive or prints one of its reader files as text.

  zrd.py <archive.zbd>              list the entries
  zrd.py <archive.zbd> <name.zrd>   print that entry as nested lists

Format: see docs/zreader.md (nodes are int32 type + value; a list stores its
count, including itself, then count-1 nodes).
"""
import struct
import sys


def entries(data):
    version, count = struct.unpack_from('<II', data, len(data) - 8)
    assert version == 1, version
    table = len(data) - 8 - count * 0x94
    for i in range(count):
        start, length = struct.unpack_from('<II', data, table + i * 0x94)
        name = data[table + i * 0x94 + 8:table + i * 0x94 + 72].split(b'\0')[0].decode('latin-1')
        yield name, start, length


def node(data, pos):
    kind, = struct.unpack_from('<i', data, pos)
    pos += 4
    if kind == 1:
        return struct.unpack_from('<i', data, pos)[0], pos + 4
    if kind == 2:
        return struct.unpack_from('<f', data, pos)[0], pos + 4
    if kind == 3:
        n, = struct.unpack_from('<I', data, pos)
        return data[pos + 4:pos + 4 + n].decode('latin-1'), pos + 4 + n
    if kind == 4:
        n, = struct.unpack_from('<I', data, pos)
        pos += 4
        items = []
        for _ in range(n - 1):
            item, pos = node(data, pos)
            items.append(item)
        return items, pos
    raise ValueError(f'bad node type {kind} at {pos:#x}')


def text(value, indent=0):
    pad = '  ' * indent
    if isinstance(value, list):
        if all(not isinstance(v, list) for v in value) and len(value) <= 8:
            return pad + '(' + ' '.join(text(v).strip() for v in value) + ')'
        lines = [pad + '(']
        lines += [text(v, indent + 1) for v in value]
        return '\n'.join(lines) + '\n' + pad + ')' if False else '\n'.join(lines + [pad + ')'])
    if isinstance(value, str):
        return pad + '"' + value + '"'
    if isinstance(value, float):
        return pad + repr(round(value, 6))
    return pad + str(value)


def main():
    data = open(sys.argv[1], 'rb').read()
    if len(sys.argv) == 2:
        for name, start, length in entries(data):
            print(f'{name}\t{length}')
        return
    for name, start, length in entries(data):
        if name.lower() == sys.argv[2].lower():
            value, _ = node(data[start:start + length], 0)
            print(text(value))
            return
    sys.exit(f'{sys.argv[2]}: not in the archive')


if __name__ == '__main__':
    main()
