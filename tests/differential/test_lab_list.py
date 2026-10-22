"""The mech lab's item list base (0x00451b40, docs/mechlab.md, The item lists)
against the original x86.

    cmake --build build && .venv/bin/python -P tests/differential/test_lab_list.py [runs] [--verbose]

The original makes a menu screen (0x0040a110, its vtable copied with
+0x14, the screen's on_button, a logging stub), loads a synthetic dialog file holding LABMENU's CURSOR and one of its
two item lists (UNASSIGNED_LIST or ASSIGNED_LIST, as shipped or mutated:
keys dropped, values changed), makes the base list (0x00451b40, vtable
0x005c1b70) and binds it (0x0059f8b0). Then a plan: items added by text
(+0x90, null names among them), by store item (+0x94, fake items whose
vt+0x0c, +0x18 and +0x20 answer kind, slots and name; ammo's name through
message 0xf59) and by part (+0x98, 0x00484d70); a store to refill from
(+0xb8, through +0x9c's scroll); clears (0x00453420), selects (+0xac),
layouts (+0xb4), the list shown and hidden, the dialog on and off, and frames
of the mouse wandering over the rows and arrows, pressing, holding and
releasing (Window::update). Then a dump of the dialog, the list, its items,
rows, arrows and images, and the list's destructor. The port
(tests/probe_lab_list.cpp) runs the same plan. Compared: the call traces and
the dumps.
"""
import ctypes
import random
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import Replay, Trace, compare, load_probe  # noqa: E402
from original import ROOT  # noqa: E402
from twod_world import IAT, READER, TwodWorld, entries, find, parse_node, write_node  # noqa: E402
import test_twod  # noqa: E402
from test_twod import canonical, fbits  # noqa: E402
from test_twod_msgbox import archive  # noqa: E402

probe = load_probe()
probe.probe_app_set_callback.argtypes = [ctypes.c_void_p]
probe.probe_lab_list.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p,
                                 ctypes.c_char_p, ctypes.c_int]
probe.probe_lab_list.restype = ctypes.c_int

SCREEN_CTOR, LOAD_FROM, BIND_BUTTON = 0x40a110, 0x59f050, 0x59f8b0
LIST_CTOR, LIST_SIZE, LIST_VT = 0x451b40, 0x22c, 0x5c1b70
SCREEN_SIZE = 0x11b20
# floor (the panels' opacity), x87 code in the import's place (test_lab_screen.py's)
FLOOR = bytes.fromhex('83ec04 d93c24 668b0424 6625fff3 660d0004 6689442402 d96c2402 dd442408 d9fc d92c24 83c404 c3')
IAT_FLOOR = 0x5bb66c
test_twod.VT_KIND.update({LIST_VT: 'C'})
NAMES = ['ER Laser (L)', 'LRM 20', 'Gauss Rifle', 'Heat Sink', 'a much longer item name for the row', '50%', '']
N_ITEMS = 6


def lab_entries():
    data = READER.read_bytes()
    start = next(s for n, s, _ in entries(data) if n == 'dlgMechLabII.zrd')
    tree, _ = parse_node(data, start)
    lab = find(tree, 'LABMENU')
    return find(lab, 'CURSOR'), find(find(lab, 'BUTTONS'), 'UNASSIGNED_LIST'), find(find(lab, 'BUTTONS'),
                                                                                       'ASSIGNED_LIST')


def mutate(r, entry):
    e = [list(x) if isinstance(x, list) else x for x in entry]
    out = []
    for i in range(0, len(e) - 1, 2):
        k, v = e[i], e[i + 1]
        if k in ('UP', 'DOWN', 'BAR', 'ITEMROLLOVER', 'ROLLOVER_SOUND') and r.random() < 0.2:
            continue
        if k == 'SELECTED_FONT' and r.random() < 0.4:  # NORMAL_FONT stands in
            continue
        if k == 'SIZE' and r.random() < 0.4:  # (16k - 3: a row 3 pixels over the height, still in)
            v = [r.choice([60, 120, 200]), r.choice([0, 30, 100, 200, 322, 500, 29, 61, 125, 157])]
        if k == 'SPACING' and r.random() < 0.4:
            v = [r.randrange(-5, 20), r.choice([-13, 0, 3, 10])]
        if k == 'OFFSET' and r.random() < 0.3:
            v = [r.randrange(-20, 20), r.randrange(-20, 20)]
        out += [k, v]
    if r.random() < 0.3:
        out += ['HEIGHT', [r.choice([6, 12, 20, 40])]]
    if r.random() < 0.15:
        out += ['FONT', ['lab_small_red']]  # the list's own (the shipped one sits in ITEMROLLOVER)
    if r.random() < 0.2:
        out += ['BAR', ['BITMAP', ['lab_text_glow'], 'OFFSET', [r.randrange(-20, 5), r.randrange(-20, 5)]]]
    return out


def dialog_file(seed):
    r = random.Random(f'lablist:{seed}')
    cursor, un, asg = lab_entries()
    name = r.choice(['UNASSIGNED_LIST', 'ASSIGNED_LIST'])
    entry = un if name == 'UNASSIGNED_LIST' else asg
    if seed % 2:
        entry = mutate(r, entry)
    body = ['CURSOR', cursor, 'BUTTONS', [name, entry]]
    return [['SHARED_IMAGE_PATH', ['common'], 'LISTDLG', body]], name, entry


def arrows_of(entry):
    p = find(entry, 'POSITION') or [0, 0]
    out = []
    for key in ('UP', 'DOWN'):
        a = find(entry, key)
        ap = find(a, 'POSITION') if a else None
        if ap:
            out.append((p[0] + ap[0] + 4, p[1] + ap[1] + 4))
    return out


def spots_of(entry):
    p = find(entry, 'POSITION') or [0, 0]
    out = [(p[0] + 20, p[1] + 10 + 16 * k) for k in range(12)]
    out += [(p[0] + 0xa6 + k % 8, p[1] + 10 + 16 * k) for k in range(8)]  # a row's right edge (0xac)
    for key in ('UP', 'DOWN'):
        a = find(entry, key)
        ap = find(a, 'POSITION') if a else None
        if ap:
            out += [(p[0] + ap[0] + 4, p[1] + ap[1] + 4)] * 3
    return out


def frame(dt, x, y, left):
    m = [x & 0xffffffff, y & 0xffffffff, 0, fbits(0.0), fbits(0.0), fbits(0.0), 0, 0, 0,
         fbits(0.0), fbits(0.0), fbits(0.0), left, 0, 0]
    return 'F ' + ' '.join(f'{v:x}' for v in [fbits(dt)] + m)


def make_plan(seed, spots, arrows=()):
    r = random.Random(f'plan:{seed}')
    lines = ['N', 'D 1']  # the dialog on: its update reads the mouse
    if r.random() < 0.7:
        lines.append('A 1')
    down = 0
    for _ in range(r.randrange(30, 90)):
        roll = r.random()
        if roll < 0.18:
            lines.append(f'T {r.randrange(len(NAMES) + 1)} {r.choice([0, 1, 3, 12, 99, -1])}')
        elif roll < 0.30:
            lines.append(f'O {r.randrange(N_ITEMS)} {r.choice([-1, -1, 0, 4])}')
        elif roll < 0.33:
            lines.append(f'P {r.choice([0, 1, 4, 8, 9, 15, 16])}')
        elif roll < 0.36:
            lines.append('C')
        elif roll < 0.42:
            lines.append(f'S {r.randrange(-1, 12)}')
        elif roll < 0.46:
            lines.append(f'K {r.choice([-1, 1, 1, 3, -5])}')
        elif roll < 0.48:
            lines.append(f'G {r.randrange(N_ITEMS + 1)}')  # a store of n items, then refill
        elif roll < 0.50:
            lines.append('L')
        elif roll < 0.52:
            lines.append(f'Q {r.randrange(8)}')  # an item's font (the subclasses set them)
        elif roll < 0.53:
            lines.append(f'A {r.randrange(2)}')
        elif roll < 0.535:
            lines.append(f'D {r.choice([0, 1, 1])}')
        elif roll < 0.58 and arrows:  # held on an arrow: the repeat's first wait to exactly 0
            ax, ay = r.choice(arrows)
            lines.append(frame(1 / 30, ax, ay, 1))
            lines += [frame(r.choice([0.5, 0.25, 0.15, 0.5]), ax, ay, 2) for _ in range(r.randrange(2, 6))]
            lines.append(frame(1 / 30, ax, ay, 4))
            down = 0
            continue
        sx, sy = r.choice(spots)
        if r.random() < 0.85:
            x, y = sx + r.randrange(-6, 30), sy + r.randrange(-6, 12)
        else:
            x, y = r.randrange(-20, 660), r.randrange(-20, 500)
        if down == 0:
            left = 1 if r.random() < 0.25 else 0
        elif down in (1, 2):
            left = 2 if r.random() < 0.6 else 4
        else:
            left = 0
        down = left
        dt = r.choice([0.0, 1 / 60, 1 / 30, 0.05, 0.2, 0.7, 0.25, 0.5])
        m = [x & 0xffffffff, y & 0xffffffff, 0, fbits(0.0), fbits(0.0), fbits(0.0), 0, 0, 0,
             fbits(0.0), fbits(0.0), fbits(0.0), left, 0, 0]
        lines.append('F ' + ' '.join(f'{v:x}' for v in [fbits(dt)] + m))
    lines.append('X')
    return lines


# ---- the original ----

class Original:
    def __init__(self, seed, data, plan):
        self.t = Trace(seed=seed)
        w = self.w = TwodWorld(self.t, seed)
        e = self.e = w.emu
        self.data, self.plan = data, plan
        r = random.Random(f'items:{seed}')
        # The fake store items: vt+0x0c kind, vt+0x18 slots, vt+0x20 name.
        self.items, kinds, names = [], {}, {}
        for i in range(N_ITEMS):
            it = e.object('item', size=0x20, slots={0x0c: ('c', 0), 0x18: ('c', 0), 0x20: ('c', 0)})
            kinds[it] = r.choice([0, 1, 1, 2, 3])
            names[it] = e.world_string(r.choice(NAMES[:-1]))
            self.items.append(it)
        w.answers['item.vt+0xc'] = lambda rr, n, a: kinds[a[0]]
        w.answers['item.vt+0x20'] = lambda rr, n, a: names[a[0]]
        w.answers['item.vt+0x18'] = lambda rr, n, a: rr.choice([0, 1, 2, 3, 7])
        # The ammo's message 0xf59 and the parts' names.

        def fmt(emu, key, a, pop):
            buf, size, mid, arg = a
            s = f'F{mid:x}:{emu.cstr(arg)}'
            emu.cpu.write(buf, s.encode('latin1')[:size - 1] + b'\0')
            emu._ret(self.t.handle(key, (mid, emu.cstr(arg))), pop)
        e.stub(0x575fd0, '0x00575fd0', 's0,s1,s2,s3', None, fmt)
        w.answers['0x00575fd0'] = 1
        parts = {i: e.world_string(f'Part{i}') for i in range(17)}
        e.stub(0x484d70, '0x00484d70', 'c')
        w.answers['0x00484d70'] = lambda rr, n, a: parts.get(a[0], 0)
        def copy_rect(emu, key, a, pop):  # the layout's rectangle
            emu.cpu.write(a[0], emu.cpu.read(a[1], 16))
            emu._ret(1, 8)
        e.import_stub(IAT['CopyRect'], '@CopyRect', 's0,s1', 8, copy_rect)

        def strdup(emu, key, a, pop):  # the items' names
            s_ = emu.cpu.read(a[0], 256).split(b'\0')[0] if a[0] else b''
            q = emu.alloc(len(s_) + 1)
            emu.cpu.write(q, s_ + b'\0')
            emu._ret(q if a[0] else 0, 0)
        e.import_stub(IAT['_strdup'], '@_strdup', 's0', 0, strdup)
        e.cpu.import_code(IAT_FLOOR, FLOOR)
        find_font = w.answers['0x00517680']
        w.answers['0x00517680'] = lambda rr, n, a: 0 if a[0] in ('', None) else find_font(rr, n, a)  # no name
        for a in (0x40d760, 0x40d770):  # the display's size (the screen's background)
            e.stub(a, f'{a:#010x}', '')
        w.answers['0x0040d760'], w.answers['0x0040d770'] = 640, 480
        e.stub(0x40cec0, '0x0040cec0', '')  # FastMenu, asked by the arrows' reading
        w.answers['0x0040cec0'] = lambda rr, n, a: rr.randrange(2)
        # The screen's on_button, and the store's list (built by G).
        self.store = e.world_alloc(0x20)
        self.nodes = [e.world_alloc(0xc) for _ in range(N_ITEMS + 1)]

    def vcall(self, obj, slot, stack=()):
        vt = self.e.r32(obj)
        return self.e.call(self.e.r32(vt + slot), ecx=obj, stack=stack)

    def set_store(self, n):
        """The store (+0x08: a list head whose nodes are next, prev, item) with the first n items."""
        e = self.e
        head = self.nodes[0]
        chain = [head] + self.nodes[1:n + 1]
        for i, nd in enumerate(chain):
            e.w32(nd, chain[(i + 1) % len(chain)])
            e.w32(nd + 4, chain[i - 1])
            e.w32(nd + 8, self.items[i - 1] if i else 0)
        e.w32(self.store + 8, head)

    def run(self):
        e, w, t = self.e, self.w, self.t
        root = w.read_tree(self.data, 0)
        self.snapshot = e.snapshot()
        d = self.d = e.alloc(SCREEN_SIZE)
        e.call(SCREEN_CTOR, ecx=d, stack=(0,))
        vt = e.alloc(0x100)
        e.cpu.write(vt, bytes(e.cpu.read(e.r32(d), 0x100)))
        e.w32(vt + 0x14, e.new_stub('screen.vt+0x14', 's0,s1,s2', 0xc))
        e.w32(d, vt)
        e.call(LOAD_FROM, ecx=d, stack=(root, e.world_string('LISTDLG'), 0))
        m = self.m = None
        dump = ''
        for line in self.plan:
            p = line.split()
            op = p[0]
            if op == 'N':
                m = self.m = e.alloc(LIST_SIZE)
                e.call(LIST_CTOR, ecx=m, stack=(0xffffffff,))
                t.handle('bind', ())
                e.call(BIND_BUTTON, ecx=d, stack=(m, e.world_string(self.name)))
            elif op == 'T':
                i = int(p[1])
                s = e.world_string(NAMES[i]) if i < len(NAMES) else 0
                self.vcall(m, 0x90, (s, int(p[2]) & 0xffffffff))
            elif op == 'O':
                self.vcall(m, 0x94, (self.items[int(p[1])], int(p[2]) & 0xffffffff))
            elif op == 'P':
                self.vcall(m, 0x98, (int(p[1]),))
            elif op == 'C':
                e.call(0x453420, ecx=m)
            elif op == 'S':
                self.vcall(m, 0xac, (int(p[1]) & 0xffffffff,))
            elif op == 'K':
                self.vcall(m, 0x9c, (int(p[1]) & 0xffffffff,))
            elif op == 'G':
                self.set_store(int(p[1]))
                e.w32(m + 0x220, self.store)
                self.vcall(m, 0xb8)
            elif op == 'L':
                self.vcall(m, 0xb4)
            elif op == 'Q':
                first, last = e.r32(m + 0x168), e.r32(m + 0x16c)
                i = int(p[1])
                if first and i < (last - first) // 4:
                    e.w32(e.r32(first + 4 * i) + 0xc, e.r32(m + 0x1c0))
            elif op == 'A':
                if int(p[1]):
                    self.vcall(m, 0x60, (fbits(-1.0),))
                else:
                    self.vcall(m, 0x64)
            elif op == 'D':
                self.vcall(d, 0x04, (int(p[1]),))
            elif op == 'F':
                vals = [int(v, 16) for v in p[1:]]
                w.mouse_frames.append(vals[1:])
                self.vcall(d, 0x00, (vals[0],))
                w.frame += 1
            elif op == 'X':
                dump = test_twod.Original.dump(self) + self.dump_list()
                e.call(0x59fc80, ecx=d, stack=(m,))  # out of the dialog, then deleted, then the screen
                self.vcall(m, 0x00, (1,))
                self.vcall(d, 0x08, (1,))
        return dump

    def dump_list(self):
        """probe_lab_list.cpp's dump_list."""
        e, m = self.e, self.m
        out, line = [], []

        def kv(k, v):
            line.append(f'{k}={v & 0xffffffff:x}')

        def tx(k, v):
            line.append(f'{k}=T{v:x}')

        def rc(k, a):
            vals = [struct.unpack('<i', struct.pack('<I', e.r32(a + 4 * i)))[0] for i in range(4)]
            line.append(f'{k}=' + ','.join(str(v) for v in vals))

        def s(k, a):
            line.append(f'{k}="' + (e.cstr(a).replace(' ', '_') if a else '(null)') + '"')

        def flush(head):
            out.append(head + ' ' + ' '.join(line) + ' ')
            line.clear()

        def base(o):
            kv('flags', e.r32(o + 0xc))
            kv('x', e.r32(o + 0x14))
            kv('y', e.r32(o + 0x18))
            kv('mouse', e.r32(o + 0x30) & 0xffff)

        def image(o):
            base(o)
            tx('img', e.r32(o + 0x3c))

        def text(o):
            base(o)
            s('text', o + 0x34)
            kv('align', e.r32(o + 0x244))
            for k, off in (('top', 0x24c), ('hfont', 0x258), ('w', 0x460), ('h', 0x464), ('nr', 0x474),
                           ('named', 0x4ac), ('ft', 0x4b0), ('frate', 0x4b4), ('fon', 0x4c0), ('fmode', 0x4c4)):
                kv(k, e.r32(o + off) & (0xff if k == 'fon' else 0xffffffff))
            rc('tr', o + 0x490)

        def control(o):
            image(o)
            kv('en', e.r32(o + 0xc4))
            rc('bounds', o + 0xcc)

        for k, off in (('top', 0x158), ('end', 0x15c), ('sel', 0x160), ('rowh', 0x188), ('sx', 0x18c), ('sy', 0x190),
                       ('spx', 0x1a4), ('spy', 0x1a8), ('ox', 0x1ac), ('oy', 0x1b0)):
            kv(k, e.r32(m + off))
        rc('rect', m + 0x194)
        for k, off in (('fn', 0x1b4), ('fs', 0x1b8), ('fr', 0x1bc), ('fl', 0x1c0), ('fst', 0x1c4), ('fc', 0x1c8),
                       ('fm', 0x1cc), ('fg', 0x1d0), ('bdx', 0x1d4), ('bdy', 0x1d8), ('rdx', 0x1dc), ('rdy', 0x1e0),
                       ('inside', 0x1f4), ('snd0', 0x1fc), ('snd1', 0x200), ('play0', 0x204), ('play1', 0x208),
                       ('alit', 0x20c), ('rt', 0x210), ('rd', 0x214), ('rr', 0x218), ('rep', 0x21c)):
            kv(k, e.r32(m + off) & (0xff if k in ('inside', 'rep') else 0xffffffff))
        rows = [e.r32(e.r32(m + 0x178) + 4 * i) for i in range((e.r32(m + 0x17c) - e.r32(m + 0x178)) // 4)] \
            if e.r32(m + 0x178) else []
        lit = e.r32(m + 0x1f8)
        sel = e.r32(m + 0x184)
        kv('lit', rows.index(lit) if lit in rows else (0x99 if lit and lit == sel else 0xff if lit else 0xfe))
        control(m)
        flush('list')
        first, last = e.r32(m + 0x168), e.r32(m + 0x16c)
        items = [e.r32(first + 4 * i) for i in range((last - first) // 4)] if first else []
        for i, it in enumerate(items):
            s('text', e.r32(it))
            ob = e.r32(it + 4)
            kv('obj', self.items.index(ob) + 1 if ob in self.items else ob)
            kv('count', e.r32(it + 8))
            kv('font', e.r32(it + 0xc))
            flush(f'item{i}')

        def row(o):
            text(o)
            kv('index', e.r32(o + 0x4d0))
            kv('height', e.r32(o + 0x148c))
            p = o + 0x4d4
            image(p)
            kv('pcolor', e.r32(p + 0xfc) & 0xffff)
            line.append('\n   count')
            text(o + 0x5dc)
            line.append('\n   mass')
            base(o + 0xaa8)
            line.append('\n   crit')
            base(o + 0xf74)
            line.append('\n   line')
            base(o + 0x1440)
            kv('end', e.r32(o + 0x1440 + 0x34))
            kv('end_y', e.r32(o + 0x1440 + 0x38))
            kv('color', e.r32(o + 0x1440 + 0x3c))

        for i, o in enumerate(rows):
            row(o)
            flush(f'row{i}')
        if sel:
            row(sel)
            line[:] = [x for x in line if not x.startswith('index=')]  # (not set by the constructor)
            flush('selrow')
        for k, off in (('up', 0x1ec), ('down', 0x1f0), ('bar', 0x1e8), ('roll', 0x1e4)):
            o = e.r32(m + off)
            if o:
                (control if k in ('up', 'down') else image)(o)
            flush(k if o else k + '=none')
        return '\n'.join(out) + '\n'


def run_case(seed, verbose):
    tree, name, entry = dialog_file(seed)
    blob = write_node(tree)
    path = ROOT / 'build' / 'lab_list_synth.zbd'
    path.write_bytes(archive([('listdlg.zrd', blob)]))
    plan = make_plan(seed, spots_of(entry), arrows_of(entry))
    orig = Original(seed, blob, plan)
    orig.name = name
    try:
        dump_o = orig.run()
    except Exception:
        from unicorn.x86_const import UC_X86_REG_EIP
        print(f'seed {seed}: the original failed at {orig.e.uc.reg_read(UC_X86_REG_EIP):#x}', orig.t.log[-5:])
        raise
    rep = Replay(orig.t, orig.e, orig.snapshot)
    probe.probe_app_set_callback(ctypes.cast(rep.callback(), ctypes.c_void_p))
    buf = ctypes.create_string_buffer(1 << 22)
    tokens = ' '.join(f'{x:x}' for x in orig.items + [orig.store])
    n = probe.probe_lab_list(str(path).encode(), name.encode(), tokens.encode(), '\n'.join(plan).encode(), buf,
                             len(buf))
    assert n >= 0, 'probe failed'
    dump_r = buf.value.decode('latin1')
    stubbed = orig.w.stub_textures
    to, do = canonical(orig.t.log, dump_o, stubbed)
    tr, dr = canonical(rep.log, dump_r, stubbed)
    do, dr = (re.sub(r'face=".*?" height=', 'height=', x, flags=re.S) for x in (do, dr))
    ok = compare(f'lab list seed {seed}', to, tr)
    if do != dr:
        ok = False
        for a, b in zip(do.split('\n'), dr.split('\n')):
            if a != b:
                print(f'lab list seed {seed}: dumps differ')
                ta, tb = a.split(' '), b.split(' ')
                print('  original:', ' '.join(x for x in ta if x not in tb)[:400])
                print('  rebuild: ', ' '.join(x for x in tb if x not in ta)[:400])
                print('  in:', a[:160])
                break
        else:
            print(f'lab list seed {seed}: dumps differ in length')
    if verbose and ok:
        print(f'lab list seed {seed}: {len(to)} calls')
    return ok, to


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    runs = int(args[0]) if args else 40
    verbose = '--verbose' in sys.argv
    passed = calls = 0
    stats = {}
    for seed in range(runs):
        ok, trace = run_case(seed, verbose)
        passed += ok
        calls += len(trace)
        for k, _ in trace:
            if k in ('screen.vt+0x14', '0x00575fd0', '0x00484d70', 'item.vt+0x18', '0x0056e3d0', '0x0056e9f0'):
                stats[k] = stats.get(k, 0) + 1
    print(f'lab list: {passed}/{runs} runs identical ({calls} calls; ' +
          ', '.join(f'{k} x{v}' for k, v in sorted(stats.items())) + ')')
    return 0 if passed == runs else 1


if __name__ == '__main__':
    import os
    code = 1
    try:
        code = main()
    except BaseException:
        import traceback
        traceback.print_exc()
    sys.stdout.flush()
    sys.stderr.flush()
    os._exit(code)
