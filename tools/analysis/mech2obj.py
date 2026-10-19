"""Exports a mech from mechlib.zbd as OBJ + MTL + PNG textures, and a preview.

  mech2obj.py <mech> <outdir>        e.g. mech2obj.py madcat out/

Reads the format in docs/mechlib.md directly (no game code): the node tree,
each object3d's transform (3x3 rotation + translation at +0x30), the models
and the shared materials; textures come from rmechtex.zbd. Also writes
<mech>_preview.png, a flat-shaded front view rendered on the CPU.
"""
import math
import struct
import sys
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import tex2png  # noqa: E402

ZBD = Path(__file__).resolve().parents[2] / 'game' / 'mw3-1.2' / 'zbd'


def entries(data):
    _, count = struct.unpack_from('<II', data, len(data) - 8)
    table = len(data) - 8 - count * 0x94
    for i in range(count):
        start, length = struct.unpack_from('<II', data, table + i * 0x94)
        name = data[table + i * 0x94 + 8:table + i * 0x94 + 72].split(b'\0')[0].decode()
        yield name, start, length


def read_materials(data, pos):
    count, = struct.unpack_from('<I', data, pos)
    pos += 4
    out = []
    for _ in range(count):
        raw = data[pos:pos + 0x28]
        pos += 0x28
        tex = None
        if struct.unpack_from('<I', raw, 0x10)[0]:
            n, = struct.unpack_from('<I', data, pos)
            tex = data[pos + 4:pos + 4 + n].decode('latin-1')
            pos += 4 + n
        r, g, b = struct.unpack_from('<3f', raw, 4)
        out.append((tex, (r / 255, g / 255, b / 255)))
    return out


def mat_mul(a, b):
    """3x4 affine (rotation rows + translation) composition: apply b, then a."""
    ra, ta = a
    rb, tb = b
    r = [[sum(rb[i][k] * ra[k][j] for k in range(3)) for j in range(3)] for i in range(3)]
    t = [sum(tb[k] * ra[k][j] for k in range(3)) + ta[j] for j in range(3)]
    return r, t


def apply(m, v):
    r, t = m
    return [sum(v[k] * r[k][j] for k in range(3)) + t[j] for j in range(3)]


def walk(data, pos, parent, meshes):
    node = data[pos:pos + 0xd0]
    name = node[:36].split(b'\0')[0].decode('latin-1')
    ntype, = struct.unpack_from('<I', node, 0x34)
    has_model, = struct.unpack_from('<I', node, 0x3c)
    children, = struct.unpack_from('<I', node, 0x5c)
    pos += 0xd0
    world = parent
    if ntype == 5:
        o = data[pos:pos + 0x90]
        m = struct.unpack_from('<12f', o, 0x30)
        local = ([list(m[0:3]), list(m[3:6]), list(m[6:9])], list(m[9:12]))
        world = mat_mul(parent, local)
        pos += 0x90
    if has_model:
        polys, verts, norms, morphs = struct.unpack_from('<4I', data, pos + 0x10)
        p = pos + 0x5c
        vs = [struct.unpack_from('<3f', data, p + 12 * i) for i in range(verts)]
        p += 12 * (verts + norms + morphs)
        table, p = p, p + 0x24 * polys
        faces = []
        for i in range(polys):
            flags, _, _, nptr, uptr, cptr, _, mat, _ = struct.unpack_from('<IiIIIIIII', data, table + 0x24 * i)
            k = flags & 0xff
            idx = struct.unpack_from(f'<{k}I', data, p)
            p += 4 * k
            if flags & 0x200 and nptr:
                p += 4 * k
            uvs = None
            if uptr:
                uvs = [struct.unpack_from('<2f', data, p + 8 * j) for j in range(k)]
                p += 8 * k
            if cptr:
                p += 12 * k
            faces.append((idx, uvs, mat, bool(flags & 0x400)))
        meshes.append((name, [apply(world, v) for v in vs], faces))
        pos = p
    for _ in range(children):
        pos = walk(data, pos, world, meshes)
    return pos


def triangles(idx, strip):
    if strip:
        return [(idx[i], idx[i + 1], idx[i + 2]) if i % 2 == 0 else (idx[i + 1], idx[i], idx[i + 2])
                for i in range(len(idx) - 2)]
    return [(idx[0], idx[i], idx[i + 1]) for i in range(1, len(idx) - 1)]


def preview(meshes, materials, path, size=512):
    tris = []
    for _, vs, faces in meshes:
        for idx, _, mat, strip in faces:
            for a, b, c in triangles(list(range(len(idx))), strip):
                tris.append(([vs[idx[a]], vs[idx[b]], vs[idx[c]]], mat))
    pts = [v for t, _ in tris for v in t]
    if not pts:
        return
    xs, ys = [p[0] for p in pts], [p[1] for p in pts]
    cx, cy = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
    scale = 0.9 * size / max(max(xs) - min(xs), max(ys) - min(ys), 1e-6)
    zbuf = [[-1e30] * size for _ in range(size)]
    img = [[(40, 44, 52)] * size for _ in range(size)]
    light = (0.4, 0.6, -0.7)
    for (a, b, c), mat in tris:
        u = [b[i] - a[i] for i in range(3)]
        w = [c[i] - a[i] for i in range(3)]
        n = [u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2], u[0] * w[1] - u[1] * w[0]]
        ln = math.sqrt(sum(x * x for x in n)) or 1
        shade = 0.35 + 0.65 * abs(sum(n[i] * light[i] for i in range(3)) / ln)
        base = materials[mat][1] if mat < len(materials) else (0.7, 0.7, 0.7)
        if materials[mat][0] if mat < len(materials) else False:
            base = (0.65, 0.65, 0.6)
        col = tuple(min(255, int(255 * base[i] * shade)) for i in range(3))
        P = [((p[0] - cx) * scale + size / 2, size / 2 - (p[1] - cy) * scale, -p[2]) for p in (a, b, c)]
        minx, maxx = max(int(min(p[0] for p in P)), 0), min(int(max(p[0] for p in P)) + 1, size)
        miny, maxy = max(int(min(p[1] for p in P)), 0), min(int(max(p[1] for p in P)) + 1, size)
        (x0, y0, z0), (x1, y1, z1), (x2, y2, z2) = P
        d = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2)
        if abs(d) < 1e-9:
            continue
        for y in range(miny, maxy):
            for x in range(minx, maxx):
                l0 = ((y1 - y2) * (x - x2) + (x2 - x1) * (y - y2)) / d
                l1 = ((y2 - y0) * (x - x2) + (x0 - x2) * (y - y2)) / d
                l2 = 1 - l0 - l1
                if l0 < 0 or l1 < 0 or l2 < 0:
                    continue
                z = l0 * z0 + l1 * z1 + l2 * z2
                if z > zbuf[y][x]:
                    zbuf[y][x] = z
                    img[y][x] = col
    rows = [bytes(v for px in row for v in px) for row in img]
    tex2png.png(path, size, size, rows, False)


def main():
    mech, out = sys.argv[1], Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    data = (ZBD / 'mechlib.zbd').read_bytes()
    ents = {n: (s, l) for n, s, l in entries(data)}
    materials = read_materials(data, ents['materials'][0])
    start, length = ents[f'mech_{mech}.flt']
    meshes = []
    identity = ([[1, 0, 0], [0, 1, 0], [0, 0, 1]], [0, 0, 0])
    end = walk(data, start, identity, meshes)
    assert end == start + length, (end - start, length)
    used = set()
    with open(out / f'{mech}.obj', 'w') as obj:
        obj.write(f'mtllib {mech}.mtl\n')
        vbase = tbase = 1
        for name, vs, faces in meshes:
            obj.write(f'o {name}\n')
            for v in vs:
                obj.write('v %f %f %f\n' % tuple(v))
            for idx, uvs, mat, strip in faces:
                used.add(mat)
                obj.write(f'usemtl m{mat}\n')
                tri = triangles(list(range(len(idx))), strip)
                for a, b, c in tri:
                    if uvs:
                        for j in (a, b, c):
                            obj.write('vt %f %f\n' % (uvs[j][0], 1 - uvs[j][1]))
                        obj.write(f'f {vbase + idx[a]}/{tbase} {vbase + idx[b]}/{tbase + 1} {vbase + idx[c]}/{tbase + 2}\n')
                        tbase += 3
                    else:
                        obj.write(f'f {vbase + idx[a]} {vbase + idx[b]} {vbase + idx[c]}\n')
            vbase += len(vs)
    textures = set()
    with open(out / f'{mech}.mtl', 'w') as mtl:
        for m in sorted(used):
            tex, rgb = materials[m]
            mtl.write(f'newmtl m{m}\nKd %f %f %f\n' % rgb)
            if tex:
                base = tex.rsplit('.', 1)[0]
                mtl.write(f'map_Kd {base}.png\n')
                textures.add(base.lower())
    tex2png.export((ZBD / 'rmechtex.zbd').read_bytes(), out, exact=textures)
    preview(meshes, materials, out / f'{mech}_preview.png')
    print(f'{mech}: {len(meshes)} meshes, {sum(len(f) for _, _, f in meshes)} polygons, '
          f'{len(used)} materials, {len(textures)} textures -> {out}')


if __name__ == '__main__':
    main()
