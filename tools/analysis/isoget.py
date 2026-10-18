# isoget.py <iso> <outdir>: copy every file out of an ISO 9660 image
import os, struct, sys
iso, out = sys.argv[1], sys.argv[2]
f = open(iso, 'rb')
def sector(n, count=1):
    f.seek(n * 2048); return f.read(2048 * count)
def walk(extent, size, path):
    data = sector(extent, (size + 2047) // 2048)
    i = 0
    while i < size:
        n = data[i]
        if n == 0:
            i = (i // 2048 + 1) * 2048; continue
        rec = data[i:i + n]; i += n
        ext, ln, flags, nl = struct.unpack('<I', rec[2:6])[0], struct.unpack('<I', rec[10:14])[0], rec[25], rec[32]
        name = rec[33:33 + nl]
        if name in (b'\0', b'\1'): continue
        target = os.path.join(path, name.decode('ascii').split(';')[0])
        if flags & 2:
            os.makedirs(target, exist_ok=True); walk(ext, ln, target)
        else:
            f.seek(ext * 2048)
            with open(target, 'wb') as o:
                left = ln
                while left:
                    chunk = f.read(min(left, 1 << 20)); o.write(chunk); left -= len(chunk)
pvd = sector(16)
os.makedirs(out, exist_ok=True)
walk(struct.unpack('<I', pvd[158:162])[0], struct.unpack('<I', pvd[166:170])[0], out)
