import struct, sys
f = open(sys.argv[1], 'rb')
def sector(n, count=1):
    f.seek(n * 2048); return f.read(2048 * count)
pvd = sector(16)
print('volume', pvd[40:72].decode('ascii', 'replace').strip(), 'blocks', struct.unpack('<I', pvd[80:84])[0])
root = pvd[156:190]
def walk(extent, size, path, depth):
    data = sector(extent, (size + 2047) // 2048)
    i = 0
    while i < size:
        n = data[i]
        if n == 0:
            i = (i // 2048 + 1) * 2048; continue
        rec = data[i:i + n]
        ext, ln, flags, nl = struct.unpack('<I', rec[2:6])[0], struct.unpack('<I', rec[10:14])[0], rec[25], rec[32]
        name = rec[33:33 + nl]
        i += n
        if name in (b'\0', b'\1'): continue
        name = name.decode('ascii', 'replace').split(';')[0]
        full = path + '/' + name
        print(('D ' if flags & 2 else 'F ') + f'{ln:>11} {ext:>8} {full}')
        if flags & 2 and depth < 6: walk(ext, ln, full, depth + 1)
walk(struct.unpack('<I', root[2:6])[0], struct.unpack('<I', root[10:14])[0], '', 0)
