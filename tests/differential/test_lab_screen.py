"""The mech lab's screen (docs/mechlab.md section 2, step 2) against the original x86.

    cmake --build build && .venv/bin/python -P tests/differential/test_lab_screen.py [runs] [first] [--verbose]

The original builds the screen 0x0044b920 from dlgMechLabII.zrd's LABMENU
(or a synthetic one, odd runs: entries moved or missing) for a lance of 1..5
slots, with the Lab State's globals ([0x00605a7c] the pilot, [0x00605a28]
the slot shown, [0x00605a84] the flag) and the game's slots (vt+0x34) and
kind (vt+0x2c) from the run's setup. Then a plan: page switches 0x0044ca50
(each page shown or hidden), presses 0x0044d4d0 (pages, ACCEPT, the
slots, the pages' own ids), a chassis set for the shown slot 0x00449010,
CONFIG_EDIT moved, a rename marked under way, dumps, the destructor. The port
(tests/probe_lab.cpp) runs the same plan. Compared: every call out and a
dump of the dialog, the tabs, the mech buttons, the parts, the panels and
MESSAGE_AREA. The x87 runs at 24 bits.

Left to the steps that port them (mechlab.md section 5), stubbed here: the
pages' controls' reading, after-bind and showing (CHASSIS_LIST,
CONFIG_LIST, UNASSIGNED_LIST, ASSIGNED_LIST, the weapon lists, ENGINE_TABLE
and LOADOUT_LIST: their own vtables' slots, "lab.vt+<slot>" with the
control's offset), the panels made with new (CHASSIS_DATA 0x00457600,
MECH_DATA 0x00456cd0, ARMOR_TITLES made inline at 0x0044bd7e: a code hook
jumps over it), the part pick 0x004504d0 and the weapon groups' refresh
0x0044dcf0.
"""
import ctypes
import os
import random
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import Replay, compare, load_probe, Trace  # noqa: E402
from original import ROOT  # noqa: E402
from twod_world import IAT, READER, TwodWorld, c_format, entries, find, parse_node, write_node  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP  # noqa: E402
import test_twod  # noqa: E402
from test_twod import canonical  # noqa: E402
from test_twod_msgbox import archive  # noqa: E402
from test_menus import MECH3, mutate_buttons  # noqa: E402

probe = load_probe()
probe.probe_app_set_callback.argtypes = [ctypes.c_void_p]
probe.probe_lab_run.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]

SIZE = 0x1b468
CTOR = 0x44b920
FLOOR = bytes.fromhex('83ec04 d93c24 668b0424 6625fff3 660d0004 6689442402 d96c2402 dd442408 d9fc d92c24 83c404 c3')
IAT_FLOOR = 0x5bb66c
PILOT, SLOT, SLOTS, MECH, STORE, SCREEN, FLAG = 0x605a7c, 0x605a28, 0x605a2c, 0x605a30, 0x605a80, 0x605a58, 0x605a84
LAB_STORE = 0x7720
TABS = [0x117c4 + 0x17c * i for i in range(7)]
MECHS = [0x122e4 + 0x64c * i for i in range(12)]
PARTS = [0x18880, 0x189f0, 0x18b60, 0x18cd0, 0x18e40, 0x18fb0, 0x19120]
PANELS = [0x112ac, 0x11374, 0x1143c]
# The pages' controls' vtables and the slots left to their steps.
LATER = {v: (0x84, 0x88, 0x60, 0x64) + ((0xa8,) if v == 0x5c0cc0 else ()) for v in (0x5c0f18, 0x5c0cc0, 0x5c1588, 0x5c1d68, 0x5c1fe0, 0x5c1300,
                                               0x5c1250, 0x5c1130)}
POPS = {0x84: 8, 0x88: 0, 0x60: 4, 0x64: 0, 0xa8: 0}

test_twod.VT_KIND.update({0x5c0e98: 'I', 0x5c0e00: 'C', 0x5c0d70: 'C', 0x5c14f0: 'C', 0x5c1448: 'C',
                          0x5c1630: 'C', 0x5c13b0: 'K', 0x5c11c0: 'C', 0x5c1740: 'F'})


def i32(v):
    return struct.unpack('<i', struct.pack('<I', v & 0xffffffff))[0]


# ---- the files ----

def synthetic(seed, tree):
    r = random.Random(f'lab:{seed}')
    body = [list(x) if isinstance(x, list) else x for x in find(tree, 'LABMENU')]
    out = []
    for i in range(0, len(body) - 1, 2):
        k, v = body[i], body[i + 1]
        if k == 'BUTTONS':
            nb = []
            for j in range(0, len(v) - 1, 2):
                name, entry = v[j], v[j + 1]
                e = [list(x) if isinstance(x, list) else x for x in entry]
                if name == 'CURRENT_CHASSIS' or name.startswith('ICON_'):
                    # CURRENT_CHASSIS's first label and the tabs are used unchecked
                    # (the exe crashes without them): kept, maybe moved.
                    if 'POSITION' in e and r.random() < 0.3:
                        e[e.index('POSITION') + 1] = [r.randrange(20, 500), r.randrange(20, 400)]
                    nb += [name, e]
                else:
                    nb += mutate_buttons(r, [name, entry], None)
            v = nb
        out += [k, v]
    return [['LABMENU', out]]


def make_plan(seed, accept=True):
    r = random.Random(f'plan:{seed}')
    lines = ['S', 'E']
    for _ in range(r.randrange(2, 12)):
        if r.random() < 0.15:
            lines.append(f'M {r.randrange(0, 600):x} {r.randrange(0, 400):x}')
        if r.random() < 0.1:
            lines.append(f'X {r.choice([0, 0, 1]):x}')
        if accept and r.random() < 0.15:  # a chassis chosen for the shown slot (0x00449010; step 3 calls it)
            lines.append(f'C {r.choice([0, 3, 7, 18]):x}')
        if r.random() < 0.5:
            # ACCEPT reads the shown slot unchecked: only with one shown (as
            # in play: entering the lab shows slot 0 first).
            ids = [0, 1, 2, 3, 4, 5, 8, 9, 10, 11, 8, 9, -1, 7, 12, 13, 16, 19, 31] + ([6] if accept else [])
            lines.append(f'B {r.choice(ids) & 0xffffffff:x}')
            if r.random() < 0.4:
                lines.append('E')
            continue
        lines.append(f'G {r.randrange(-1, 8) & 0xffffffff:x} {r.choice([0, 1, 1]):x}')
        if r.random() < 0.4:
            lines.append('E')
    lines += ['E', 'U']
    return lines


# ---- the original ----

class Original:
    def __init__(self, seed, data, start, cfg):
        self.t = Trace(seed=seed)
        w = self.w = TwodWorld(self.t, seed)
        e = self.e = w.emu
        t = self.t
        self.cfg = cfg
        tree = w.read_tree(data, start)
        e.stub(0x556740, '@zrdr_load', 'c,d', None, lambda emu, key, a, pop: emu._ret(tree, pop))
        e.stub(0x556820, '@zrdr_free', 'c')
        for addr, spec in MECH3.items():
            e.stub(addr, f'{addr:#010x}', spec)
        e.cpu.import_code(IAT_FLOOR, FLOOR)

        # The Lab State's globals and the game: its slots and kind.
        pilot, game, vt = e.world_alloc(0x84), e.world_alloc(0xd0), e.world_alloc(0x60)
        e.w32(game, vt)
        e.w32(pilot + 0x6c, game)
        e.w32(PILOT, pilot)
        e.w32(SLOT, cfg['shown'] & 0xffffffff)
        e.w32(SLOTS, cfg['slots'])
        e.w32(MECH, 0x13)
        e.w32(STORE, LAB_STORE)
        e.cpu.write(FLAG, bytes([cfg['flag']]))
        slots = self.slots = []
        for m in cfg['mechs']:
            s = e.world_alloc(0x18)
            e.w32(s + 4, m & 0xffffffff)
            e.w32(s + 0xc, 7)
            slots.append(s)

        def slot(emu, key, a, pop):
            i = i32(a[1])
            v = slots[i] if 0 <= i < len(slots) else 0
            t.handle(key, a)
            t.override(key, v)
            emu._ret(v, pop)
        e.w32(vt + 0x34, e.new_stub('vt+0x34', 'c,s0', 4, slot))
        def kind(emu, key, a, pop):
            t.handle(key, a)
            t.override(key, cfg['kind'])
            emu._ret(cfg['kind'], pop)
        e.w32(vt + 0x2c, e.new_stub('vt+0x2c', 'c', 0, kind))

        # The panels made with new: fakes whose slots are recorded.
        def fake(emu, key, a, pop):
            t.handle(key, a)
            o = self.fake_panel()
            t.override(key, o)
            emu._ret(o, pop)
        e.stub(0x457600, '0x00457600', '', 0, fake)
        e.stub(0x456cd0, '0x00456cd0', '', 0, fake)

        def armor(uc, address, size, user):  # new(0x31e0) and its inline making
            t.handle('0x0044fad0', ())
            o = self.fake_panel()
            t.override('0x0044fad0', o)
            uc.reg_write(UC_X86_REG_ESP, uc.reg_read(UC_X86_REG_ESP) + 4)  # the size pushed
            uc.reg_write(UC_X86_REG_EAX, o)
            uc.reg_write(UC_X86_REG_EIP, 0x44be16)
        e.uc.hook_add(UC_HOOK_CODE, armor, begin=0x44bd8e, end=0x44bd8e)

        # The pages' controls' slots of later steps.
        for vtab, slots_ in LATER.items():
            for sl in slots_:
                spec = 'c,s0' if sl == 0x60 else 'c'
                e.w32(vtab + sl, e.new_stub(f'lab.vt+{sl:#x}', spec, POPS[sl], self.later))
        e.stub(0x4504d0, '0x004504d0', 's0', 4)
        e.stub(0x44dcf0, '0x0044dcf0', '', 0)
        e.stub(0x44cf50, '0x0044cf50', '', 0)
        e.stub(0x449b00, '0x00449b00', 'c,s0', 4, self.later)
        e.stub(0x472a30, '0x00472a30', 'c,s0', 4)
        e.stub(0x471c10, '0x00471c10', 'c,s0', 4)
        e.stub(0x433df0, '0x00433df0', 'c,s0,s1,s2', 12)
        e.stub(0x44a3a0, '0x0044a3a0', 's0', 4)

        def sprintf(emu, key, a, pop):
            esp = emu.uc.reg_read(UC_X86_REG_ESP)
            text = c_format(emu, emu.cstr(a[1]), esp + 12)
            emu.cpu.write(a[0], text.encode('latin1') + b'\0')
            emu._ret(len(text), 0)
        e.import_stub(0x5bb658, '@sprintf', 's0,s1', 0, sprintf)

        def press(uc, address, size, user):  # the pages' own presses: their steps
            esp = uc.reg_read(UC_X86_REG_ESP)
            t.handle('lab.press', (e.r32(esp + 0x24),))
            ret = e.r32(esp + 0x20)
            uc.reg_write(UC_X86_REG_ESP, esp + 0x24 + 0xc)
            uc.reg_write(UC_X86_REG_EIP, ret)
        e.uc.hook_add(UC_HOOK_CODE, press, begin=0x44d54a, end=0x44d54a)

        def mech_name(emu, key, a, pop):
            v = t.handle(key, (i32(a[0]),))
            emu._ret(e.world_string(f'mech{i32(a[0])}') if v else 0, pop)
        e.stub(0x484650, '0x00484650', 'c', None, mech_name)
        empty = e.world_string('')
        e.stub(0x5abdb0, '@cs', 'c', 0, lambda emu, k, a, p: (emu.w32(a[0], empty), emu._ret(a[0], 0)))
        e.stub(0x5abda4, '@cs~', '', 0, lambda emu, k, a, p: emu._ret(0, 0))
        e.stub(0x5abe5e, '@cs+=', 'c,s0', 4, lambda emu, k, a, p: (
            emu.w32(a[0], emu.world_string(emu.cstr(emu.r32(a[0])) + (emu.cstr(a[1]) if a[1] else ''))),
            emu._ret(a[0], 4)))
        e.stub(0x5abdbc, '@cs=', 'c,s0', 4, lambda emu, k, a, p: (
            emu.w32(a[0], emu.world_string(emu.cstr(a[1]) if a[1] else '')), emu._ret(a[0], 4)))

        def cs_format(emu, key, a, pop):
            va = emu.uc.reg_read(UC_X86_REG_ESP) + 12
            emu.w32(a[0], emu.world_string(c_format(emu, emu.cstr(a[1]), va)))
            emu._ret(a[0], 0)
        e.stub(0x5abdaa, '@cs.format', 's0,s1', 0, cs_format)

        def strdup(emu, key, a, pop):
            s = emu.cpu.read(a[0], 256).split(b'\0')[0] if a[0] else b''
            p = emu.alloc(len(s) + 1)
            emu.cpu.write(p, s + b'\0')
            emu._ret(p if a[0] else 0, 0)
        e.import_stub(IAT['_strdup'], '@_strdup', 's0', 0, strdup)

        # FIX (bug), as the port's FileList: an empty list's rectangle ends at its top.
        def empty_rect(uc, address, size, user):
            esp = uc.reg_read(UC_X86_REG_ESP)
            if i32(e.r32(esp + 0x10)) <= 0:
                e.w32(esp + 0x30, e.r32(esp + 0x1c))
        e.uc.hook_add(UC_HOOK_CODE, empty_rect, begin=0x48d06d, end=0x48d06d)

        def copy_rect(emu, key, a, pop):
            emu.cpu.write(a[0], emu.cpu.read(a[1], 16))
            emu._ret(1, 8)
        e.import_stub(IAT['CopyRect'], '@CopyRect', 's0,s1', 8, copy_rect)
        w.answers.update({
            '0x0040cec0': cfg['fast'],
            '0x0040d760': 640, '0x0040d770': 480,
            '0x00576040': lambda rr, k, a: e.world_string(f'M{a[0]:x}'),
            '0x00484650': lambda rr, k, a: rr.choice([1, 1, 1, 0]),
            '0x0044a3a0': lambda rr, k, a: rr.choice([0, 1]),
        })
        self.m = 0
        self.snapshot = None

    def fake_panel(self):
        e, w = self.e, self.w
        o = w.emu.world_alloc(0x200)
        vt = e.world_alloc(0x100)
        for sl, spec, pop in ((0x0, 'c,s0', 4), (0x60, 'c,s0', 4), (0x64, 'c', 0), (0x84, 'c', 8), (0x88, 'c', 0)):
            e.w32(vt + sl, e.new_stub(f'vt+{sl:#x}', spec, pop))
        e.w32(o, vt)
        sub = e.world_alloc(0x100)
        e.w32(sub + 0x98, e.new_stub('vt+0x98', 'c,s0,s1', 8))
        e.w32(o + 0x188, sub)
        return o

    def later(self, emu, key, a, pop):
        args = (a[0] - self.m,) + tuple(a[1:])
        self.t.handle(key, args)
        emu._ret(0, pop)

    def vcall(self, obj, slot, stack=()):
        return self.e.call(self.e.r32(self.e.r32(obj) + slot), ecx=obj, stack=stack)

    def run(self, plan):
        e = self.e
        dump = ''
        for line in plan:
            p = line.split()
            op = p[0]
            pv = [int(x, 16) for x in p[1:]]
            m = self.m
            if op == 'S':
                self.snapshot = e.snapshot()
                self.m = self.d = m = e.alloc(SIZE)
                e.w32(SCREEN, m)
                e.call(CTOR, ecx=m, stack=(self.cfg['slots'],))
            elif not m:
                continue
            elif op == 'G':
                e.call(0x44ca50, ecx=m, stack=(pv[0], pv[1]))
            elif op == 'B':
                self.vcall(m, 0x14, (pv[0], 0, 0))
            elif op == 'C':
                e.call(0x449010, ecx=0x6059f8, stack=(pv[0],))
            elif op == 'X':  # a rename under way (CONFIG_EDIT's +0x38c)
                e.w32(m + 0x18034, pv[0])
            elif op == 'M':  # CONFIG_EDIT moved (its set_pos, 0x0044a710)
                self.vcall(m + 0x17ca8, 0x0c, (pv[0], pv[1]))
            elif op == 'E':
                dump += test_twod.Original.dump(self) + self.dump_screen()
            elif op == 'U':
                self.vcall(m, 0x08, (1,))
                self.m = 0
                dump += 'gone\n'
        return dump

    # The dump probe_lab.cpp writes, from the emulator's memory.
    def dump_screen(self):
        e, m = self.e, self.m
        objs = []
        p = e.r32(m + 8)
        while p:
            objs.append(p)
            p = e.r32(p + 4)

        def ref(a):
            return f'o{objs.index(a) if a in objs else -1}'
        out = [f'screen page={i32(e.r32(m + 0x111ec))}\n']
        for i, off in enumerate(TABS):
            b = m + off
            first, last = e.r32(b + 0x170), e.r32(b + 0x174)
            hl = [e.r32(first + 4 * k) for k in range((last - first) // 4)] if first else []
            out.append(f'tab{i} id={i32(e.r32(b + 0x150))} en={i32(e.r32(b + 0xc4))} sel={e.r8(b + 0x158)} '
                       f'blink={e.r8(b + 0x159)} help=' + ','.join(ref(x) for x in hl) + '\n')
        for i, off in enumerate(MECHS):
            b = m + off
            out.append(f'mech{i} id={i32(e.r32(b + 0x150))} mech={i32(e.r32(b + 0x17c))} name={ref(b + 0x180)}\n')
        head = m + 0x18710
        out.append(f'parts head={i32(e.r32(head + 0x16c))},{i32(e.r32(head + 0x164))}' +
                   ''.join(f' {i32(e.r32(m + o + 0x16c))},{i32(e.r32(m + o + 0x164))}' for o in PARTS) + '\n')
        for off in PANELS:
            b = m + off
            out.append(f'panel {e.r32(b + 0xbc):x} {e.r32(b + 0xc0):x} {e.r32(b + 0xc4):x}\n')
        msg = m + 0x1afb0
        out.append(f'message sound={e.r32(msg + 0x4b0):x} vol={e.r32(msg + 0x4b4):x}\n')
        ch = m + 0x16e74
        out.append(f'lab slot={i32(e.r32(SLOT))} mech={i32(e.r32(MECH))} chassis sel={i32(e.r32(ch + 0x17c))} '
                   f'x218={e.r32(ch + 0x218):x} store={e.r32(ch + 0x214):x} slots=' +
                   ','.join(f'{i32(e.r32(s + 4))}/{e.r32(s + 0xc):x}' for s in self.slots) + '\n')
        return ''.join(out)


def run_case(seed, path, data, start, cfg, plan, verbose):
    orig = Original(seed, data, start, cfg)
    dump_o = orig.run(plan)
    rep = Replay(orig.t, orig.e, orig.snapshot)
    probe.probe_app_set_callback(ctypes.cast(rep.callback(), ctypes.c_void_p))
    buf = ctypes.create_string_buffer(1 << 23)
    setup = ' '.join(f'{v & 0xffffffff:x}' for v in [cfg['slots'], orig.e.r32(PILOT), LAB_STORE, cfg['shown'],
                                                       cfg['flag'], len(orig.slots)] + orig.slots)
    n = probe.probe_lab_run(str(path).encode(), setup.encode(), '\n'.join(plan).encode(), buf, len(buf))
    assert n >= 0, 'probe failed'
    dump_r = buf.value.decode('latin1')
    stubbed = orig.w.stub_textures
    to, do = canonical(orig.t.log, dump_o, stubbed)
    tr, dr = canonical(rep.log, dump_r, stubbed)
    do, dr = (re.sub(r'face=".*?" height=', 'height=', x, flags=re.S) for x in (do, dr))
    ok = compare(f'lab seed {seed}', to, tr)
    if do != dr:
        ok = False
        for a, b in zip(do.split('\n'), dr.split('\n')):
            if a != b:
                print(f'lab seed {seed}: dumps differ')
                ta, tb = a.split(' '), b.split(' ')
                print('  original:', ' '.join(x for x in ta if x not in tb)[:400])
                print('  rebuild: ', ' '.join(x for x in tb if x not in ta)[:400])
                print('  in:', a[:200])
                break
        else:
            print(f'lab seed {seed}: dumps differ in length')
    if verbose and ok:
        print(f'lab seed {seed}: {len(to)} calls')
    return ok, to


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    runs = int(args[0]) if args else 40
    first = int(args[1]) if len(args) > 1 else 0
    verbose = '--verbose' in sys.argv
    data = READER.read_bytes()
    start = next(s for n, s, _ in entries(data) if n.lower() == 'dlgmechlabii.zrd')
    tree, _ = parse_node(data, start)
    synth_path = ROOT / 'build' / 'lab_synth.zbd'
    passed = calls = 0
    stats = {}
    for seed in range(first, first + runs):
        if verbose:
            print('seed', seed, flush=True)
        r = random.Random(f'cfg:{seed}')
        n = r.choice([1, 2, 3, 4, 4, 5])
        cfg = {'slots': n, 'mechs': [r.choice([0, 3, 7, 18, -1]) for _ in range(n)],
               'shown': r.choice([-1, 0, 0, 1, 3, 7]), 'flag': r.choice([0, 0, 1]),
               'kind': r.choice([1, 1, 2, 3, 0]), 'fast': r.choice([0, 0, 1])}
        if seed % 2:
            t = synthetic(seed, tree)
            blob = write_node(t)
            synth_path.write_bytes(archive([('dlgMechLabII.zrd', blob)]))
            path, fdata, fstart = synth_path, blob, 0
        else:
            path, fdata, fstart = READER, data, start
        plan = make_plan(seed, 0 <= cfg['shown'] < n and cfg['mechs'][cfg['shown']] is not None)
        ok, trace = run_case(seed, path, fdata, fstart, cfg, plan, verbose)
        passed += ok
        calls += len(trace)
        for k, _ in trace:
            if k.startswith('lab.') or k.startswith('vt+') or k in ('0x004504d0', '0x0044dcf0', '0x00472a30',
                                                                      '0x00471c10', '0x00433df0', '0x0044cf50',
                                                                      '0x005905a0', '0x0044a3a0', '0x00449b00'):
                stats[k] = stats.get(k, 0) + 1
    print(f'lab screen: {passed}/{runs} runs identical ({calls} calls; ' +
          ', '.join(f'{k} x{v}' for k, v in sorted(stats.items())) + ')')
    return 0 if passed == runs else 1


if __name__ == '__main__':
    code = 1
    try:
        code = main()
    except BaseException:
        import traceback
        traceback.print_exc()
    sys.stdout.flush()
    sys.stderr.flush()
    os._exit(code)
