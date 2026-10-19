# Exploration script (render map, 2026-10-19): polygon facts for docs/drafts/render.md.
# Per world: winding of terrain/sky polygons (right-hand normal in file order),
# priorities, vertex colours on textured vs untextured polygons, and which nodes
# use given untextured material colours. Usage: render_poly_survey.py c1 [r,g,b ...]
import struct, sys, collections, math
G = 'game/mw3-1.2/zbd/'
U = lambda b, o, f='<i': struct.unpack_from(f, b, o)[0]

def load(m):
    b = open(G + m + '/gamez.zbd', 'rb').read()
    sig, ver, tc, to, mo, mdo, nas, nlf, no = struct.unpack_from('<9i', b, 0)
    asz, cnt, _, _ = struct.unpack_from('<4i', b, mo); mat0 = mo + 16
    mats = []
    for i in range(asz):
        o = mat0 + i * 0x2c
        mats.append(dict(alpha=b[o], flags=b[o + 1], rgb=tuple(round(x) for x in struct.unpack_from('<3f', b, o + 4)),
                         tex=U(b, o + 0x10)))
    o = mdo; masz, mcnt, mfree = struct.unpack_from('<3i', b, o); o += 12
    hdrs = [b[o + i * 0x60:o + i * 0x60 + 0x60] for i in range(masz)]; o += masz * 0x60
    models = {}
    for i, h in enumerate(hdrs[:mcnt]):
        mtype, facade, mflags = struct.unpack_from('<3I', h, 0)
        pc, poly, vert, nrm, morph, light = struct.unpack_from('<6i', h, 0xc)
        V = [struct.unpack_from('<3f', b, o + 12 * j) for j in range(vert)]; o += 12 * vert
        o += 12 * nrm + 12 * morph
        if light > 0:
            L = [U(b, o + j * 0x4c + 0xc) for j in range(light)]; o += 0x4c * light
            o += 12 * sum(L)
        P = []
        if poly > 0:
            H = [b[o + j * 0x24:o + j * 0x24 + 0x24] for j in range(poly)]; o += 0x24 * poly
            for p in H:
                fl = U(p, 0, '<I'); k = fl & 0xff; mi = U(p, 0x1c); pr = U(p, 4)
                idx = list(struct.unpack_from(f'<{k}I', b, o)); o += 4 * k
                if fl & 0x200: o += 4 * k
                uv = None
                if mats[mi]['flags'] & 1:
                    uv = struct.unpack_from(f'<{2*k}f', b, o); o += 8 * k
                col = struct.unpack_from(f'<{3*k}f', b, o); o += 12 * k
                P.append(dict(flags=fl, n=k, mat=mi, pri=pr, idx=idx, uv=uv, col=col))
        models[i] = dict(type=mtype, flags=mflags, V=V, P=P)
    # nodes
    o = no
    N = [b[o + i * 0xd4:o + i * 0xd4 + 0xd4] for i in range(nas)]; o += nas * 0xd4
    nodes = []
    for i, n in enumerate(N):
        t = U(n, 0x34)
        nodes.append(dict(name=n[:36].split(b'\0')[0].decode('latin-1'), flags=U(n, 0x24, '<I'), cls=t,
                          model=U(n, 0x3c), pc=U(n, 0x54), cc=U(n, 0x5c)))
    # parent/child arrays come after the class data; reuse light_survey's walk order
    sizes = {1: 0x1e8, 2: 0xbc, 3: 0xf8, 4: 0x1c, 5: 0x90, 6: 0x50, 9: 0xd0, 10: 0x94}
    for i, n in enumerate(N):
        t = nodes[i]['cls']
        if t:
            d = b[o:o + sizes[t]]; o += sizes[t]
            if t == 2:
                lc = U(d, 0xa0); sc = U(d, 0xac); xc = U(d, 0x88); zc = U(d, 0x8c)
                o += 4 * max(lc, 0) + 4 * max(sc, 0)
                for _ in range(zc * xc):
                    o += 0x48; nc = struct.unpack_from('<h', b, o - 0x48 + 0x3a)[0]
                    if nc > 0: o += 4 * nc
            if t == 9 and U(d, 200) > 0: o += 4 * U(d, 200)
            if t == 10 and U(d, 0x8c) > 0: o += 4 * U(d, 0x8c)
        pc = nodes[i]['pc']; cc = nodes[i]['cc']
        nodes[i]['parents'] = list(struct.unpack_from(f'<{max(pc,0)}i', b, o)) if pc > 0 else []
        if pc > 0: o += 4 * pc
        nodes[i]['children'] = list(struct.unpack_from(f'<{max(cc,0)}i', b, o)) if cc > 0 else []
        if cc > 0: o += 4 * cc
    return mats, models, nodes

def normal(V, p):
    f = [V[i] for i in p['idx']]
    a, bb, c = f[0], f[1], f[2]
    u = [bb[k] - a[k] for k in range(3)]; v = [c[k] - a[k] for k in range(3)]
    n = (u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0])
    l = math.sqrt(sum(x*x for x in n)) or 1
    return tuple(x / l for x in n), tuple(sum(q[k] for q in f) / len(f) for k in range(3))

def path(nodes, i):
    s = []
    while i >= 0 and len(s) < 12:
        s.append(nodes[i]['name']); ps = nodes[i]['parents']; i = ps[0] if ps else -1
    return '/'.join(reversed(s))

def main(m, wanted):
    mats, models, nodes = load(m)
    users = collections.defaultdict(list)
    for i, n in enumerate(nodes):
        if n['cls'] == 5 and n['model'] >= 0: users[n['model']].append(i)
    st = collections.Counter(); pri = collections.Counter(); vc = collections.Counter()
    for mi, md in models.items():
        for p in md['P']:
            tex = mats[p['mat']]['flags'] & 1
            pri[p['pri']] += 1
            cols = set(round(x) for x in p['col'])
            vc[('tex' if tex else 'flat', 'all255' if cols == {255} else 'other')] += 1
            st[hex(p['flags'] & ~0xff)] += 1
    print(m, 'poly flags', dict(st)); print('  priorities', dict(pri)); print('  vertex colours', dict(vc))
    # winding: terrain (node flag 0x8000) normals' y sign; sky normals vs centre
    up = collections.Counter()
    for i, n in enumerate(nodes):
        if n['cls'] == 5 and n['model'] >= 0 and n['flags'] & 0x8000:
            md = models[n['model']]
            for p in md['P']:
                if p['n'] >= 3:
                    nn, _ = normal(md['V'], p); up['+y' if nn[1] > 0.1 else ('-y' if nn[1] < -0.1 else 'side')] += 1
    print('  terrain right-hand normals', dict(up))
    for i, n in enumerate(nodes):
        if n['name'].startswith('hor') or n['name'] in ('horizon',):
            print('  sky node', n['name'], 'cls', n['cls'], 'flags', hex(n['flags']), 'model', n['model'], 'path', path(nodes, i))
            if n['cls'] == 5 and n['model'] >= 0:
                md = models[n['model']]; inw = collections.Counter(); cl = collections.Counter()
                for p in md['P']:
                    nn, c = normal(md['V'], p)
                    d = sum(nn[k] * c[k] for k in range(3))
                    inw['towards-origin' if d < 0 else 'away'] += 1
                    mt = mats[p['mat']]; cl[(mt['flags'] & 1, mt['rgb'] if not mt['flags'] & 1 else mt['tex'], tuple(sorted(set(round(x) for x in p['col']))))] += 1
                print('    model flags', hex(md['flags']), 'polys', len(md['P']), 'normal (local, origin=node)', dict(inw))
                print('    (textured, rgb|texture, vertex colours):', dict(cl))
    for w in wanted:
        hits = collections.Counter(); ex = {}
        for mi, md in models.items():
            for p in md['P']:
                mt = mats[p['mat']]
                if not mt['flags'] & 1 and mt['rgb'] == w:
                    for u in users.get(mi, [-1]):
                        key = (nodes[u]['name'] if u >= 0 else '(no node)', hex(p['flags']), p['pri'])
                        hits[key] += 1
                        if key not in ex:
                            nn, c = normal(md['V'], p)
                            ex[key] = (path(nodes, u) if u >= 0 else '', [tuple(round(x, 2) for x in md['V'][j]) for j in p['idx']][:4], tuple(round(x, 2) for x in nn),
                                       len(md['P']), sorted(set((mats[q['mat']]['flags'] & 1, mats[q['mat']]['rgb']) for q in md['P'])), hex(nodes[u]['flags']) if u >= 0 else '', hex(md['flags']), mt['alpha'])
        print('  colour', w, 'total', sum(hits.values()))
        for k, v in hits.most_common(8):
            print('   ', v, k, ex[k])

if __name__ == '__main__':
    wanted = [tuple(int(x) for x in a.split(',')) for a in sys.argv[2:]]
    main(sys.argv[1], wanted)

def alpha_survey(m):
    """Untextured polygons by material (alpha, rgb): polygon count and a few node paths."""
    mats, models, nodes = load(m)
    users = collections.defaultdict(list)
    for i, n in enumerate(nodes):
        if n['cls'] == 5 and n['model'] >= 0: users[n['model']].append(i)
    c = collections.Counter(); ex = collections.defaultdict(set); ta = collections.Counter()
    for mi, md in models.items():
        for p in md['P']:
            mt = mats[p['mat']]
            if mt['flags'] & 1: ta[mt['alpha']] += 1; continue
            k = (mt['alpha'], mt['rgb']); c[k] += 1
            for u in users.get(mi, [])[:2]:
                if len(ex[k]) < 3: ex[k].add(path(nodes, u))
    print(m, 'textured polygons by material alpha', dict(ta))
    for k, v in sorted(c.items(), key=lambda kv: -kv[1]):
        print('  untextured', v, 'alpha', k[0], 'rgb', k[1], sorted(ex[k]))

def tree(m, root):
    mats, models, nodes = load(m)
    for i, n in enumerate(nodes):
        if n['name'] == root:
            def rec(j, d):
                n = nodes[j]; md = models.get(n['model']) if n['model'] >= 0 else None
                info = ''
                if md:
                    cl = collections.Counter((mats[p['mat']]['flags'] & 1, mats[p['mat']]['alpha'], mats[p['mat']]['rgb'], tuple(sorted(set(round(x) for x in p['col'])))[:3]) for p in md['P'])
                    info = f" model flags {md['flags']:#x} type {md['type']} polys {len(md['P'])} {dict(cl)}"
                print('  ' * d, n['name'], 'cls', n['cls'], hex(n['flags']), info)
                for ch in n['children']: rec(ch, d + 1)
            rec(i, 1)
