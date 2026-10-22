"""The mech types' data against the original (docs/mech-build.md section 14):
the cache 0x430d10 with the constructor 0x4303d0 (0x430490) and the loader
0x4308a0 and its steps (the ranges 0x430840, the sounds 0x4306d0, the
effects 0x4320b0, the soils 0x432670 / 0x4326d0), the refresh 0x4319a0
(0x4319f0 with the canned effects 0x431750) and the clear 0x4318e0
(0x430580), the options' detail scale 0x430440, for all 19 mechs.

    cmake --build build && .venv/bin/python -P tests/differential/test_mech_data_load.py [cases]

The original runs first with every call out of mech_data.cpp stubbed and
answered from one script: mechlib's trees (0x484760), the definition
(0x4846a0, a fake block with its ranges, reader and skins), the skins
(0x475780), the cameras (0x430e70), the animations (0x431a70, tested by
test_mechdata.py; its stub may fail the step), the sounds and effects by
name (0x56f6f0, 0x50b450), the nodes by name (0x4f7fe0), the game type
(0x479480 / 0x479460), zReader (0x5569e0, 0x5570b0, 0x5569f0), the error
box and report, the destructors' calls; MFC's CString is emulated. Each
session gets, refreshes and clears the cache in a random order, the types
present or failing (no model, no low-detail model, no definition, a failed
step, missing sounds and effects). The port then runs on a calltrace.Replay
of the answers. Compared: the call traces, every result, the cache's
entries in order, and every live data and soils block the original
allocated with the port's object laid over it field by field (the bytes
the original never wrote stay the allocator's fill).
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import CALLBACK, Emu, Replay, Trace, compare, load_probe  # noqa: E402
from test_mission_load import c_format  # noqa: E402

FILL = 0xcd
U = ctypes.c_uint32
CACHE, COUNT, STUBS_LIST = 0x605704, 0x605708, 0x605714
GET, REFRESH, CLEAR, SCALE = 0x430d10, 0x4319a0, 0x4318e0, 0x430440
CALLS = {
    0x484760: 'c,d', 0x4eb6d0: 'c,d', 0x4ac150: 's0,s1,s2,zs3', 0x49bfd0: 's0,zs1,s2,zs3,zs4', 0x4846a0: 'c',
    0x475780: 'c,s0', 0x56f6f0: 'zc', 0x50b450: 'zc', 0x4f7fe0: 'c,zd', 0x479480: '', 0x479460: '',
    0x5569e0: 'c,zd', 0x43b860: 'c', 0x40abf0: 'c',
}


def setup_probe():
    probe = load_probe()
    probe.probe_app_set_callback.argtypes = [CALLBACK]
    probe.probe_md_dfn.argtypes = [U, U, U, ctypes.c_float, ctypes.c_float]
    probe.probe_md_get.argtypes = [ctypes.c_int, ctypes.c_int]
    probe.probe_md_get.restype = U
    probe.probe_md_cache.argtypes = [ctypes.c_void_p, ctypes.c_int]
    probe.probe_md_image.argtypes = [U, U, ctypes.c_void_p, ctypes.c_void_p]
    probe.probe_md_scale.argtypes = [ctypes.c_float]
    return probe


def make_case(i):
    r = random.Random(f'md{i}')
    roster = i % 19
    others = [r.randrange(19) for _ in range(2)]
    ops = []
    for _ in range(r.randrange(3, 9)):
        k = r.random()
        if k < 0.6:
            ops.append(('get', r.choice([roster, roster] + others), int(r.random() < 0.8)))
        elif k < 0.8:
            ops.append(('refresh',))
        else:
            ops.append(('clear',))
    ops.append(('clear',))
    # The options' mech detail scale (0x430440) between them: its own stream,
    # so the other draws stay as they were.
    r2 = random.Random(f'mds{i}')
    for _ in range(r2.randrange(0, 3)):
        ops.insert(r2.randrange(len(ops)), ('scale', r2.choice([0.5, 0.75, 1.0, 1.5, 2.0, r2.uniform(0.1, 3)])))
    # A type without a definition stays cached, and the refresh then reads
    # through null in the exe (0x431a20; the port's FIX (bug)): sessions that
    # refresh keep every definition.
    refresh = any(op[0] == 'refresh' for op in ops)
    return {
        'seed': i, 'rng': r, 'roster': roster, 'ops': ops,
        'no_model': {x: r.random() < 0.1 for x in range(19)},
        'no_lod': {x: r.random() < 0.15 for x in range(19)},
        'no_dfn': {x: r.random() < 0.1 and not refresh for x in range(19)},
        'no_skins': {x: r.random() < 0.2 for x in range(19)},
        'fail_anims': {x: r.random() < 0.1 for x in range(19)},
        'ranges': {x: (r.uniform(0, 200), r.uniform(0, 200)) for x in range(19)},
        'miss': r.choice([0.0, 0.1, 0.5]), 'mp': int(r.random() < 0.3), 'ia': int(r.random() < 0.3),
        'blocks': [],
    }


def setup(case):
    r = case['rng']
    t = Trace()
    e = Emu(t)
    # The types load with Direct3D up: the x87 at its in-play 24 bits (the
    # detail scale's products round to float as the port's do).
    e.cpu.fpu_control(0x007f)
    blocks = case['blocks']

    def alloc(n, a):
        size = max(a[0], 4)
        addr = e.cpu.alloc(size)
        e.cpu.write(addr, bytes([FILL]) * size)
        blocks.append((addr, a[0]))
        return addr

    # The definitions, trees, reader nodes and handles live in the fake world.
    dfn = {}
    for x in range(19):
        d = e.world_alloc(0x1a8)
        dfn[x] = (d, e.world_alloc(0x10), 0 if case['no_skins'][x] else e.world_alloc(0x14))
        e.w32(d + 8, dfn[x][1])
        e.w32(d + 0x1c, dfn[x][2])
        e.cpu.write(d + 0x14c, struct.pack('<ff', *case['ranges'][x]))
    case['dfn'] = dfn
    canned = {}
    for name in ('eject', 'jumpjets', 'narc'):
        node, lst = e.world_alloc(8), e.world_alloc(0x10)
        e.w32(node + 4, lst)
        e.w32(lst + 0xc, e.world_string(f'fx_{name}'))
        canned[name] = node
    anims_node, canned_node = e.world_alloc(8), e.world_alloc(8)
    miss = case['miss']

    def present(tag, name):
        return random.Random(f'{case["seed"]}:{tag}:{name}').random() >= miss

    def handle(tag, name):
        return 0x2b000000 | (hash((tag, name)) & 0xfffff0) if present(tag, name) else 0

    def model(n, a):
        roster, variant = a
        if variant == 0 and case['no_model'][roster] or variant == 1 and case['no_lod'][roster]:
            return 0
        return 0x2a000000 + roster * 0x100 + variant * 0x10

    def reader_find(n, a):
        node, key = a
        if key == 'animations':
            return anims_node
        if key == 'canned':
            return canned_node if node == anims_node else 0
        return canned.get(key, 0) if node == canned_node and present('canned', key) else 0

    script = {
        'new': alloc,
        '0x00484760': model,
        '0x004846a0': lambda n, a: 0 if case['no_dfn'][a[0]] else dfn[a[0]][0],
        '0x00430e70': lambda n, a: 0x29000000 + n * 0x10,
        '0x0056f6f0': lambda n, a: handle('snd', a[0]),
        '0x0050b450': lambda n, a: handle('anim', a[0]),
        '0x004f7fe0': lambda n, a: handle('node', a[1]),
        '0x00479480': case['mp'], '0x00479460': case['ia'],
        '0x00431a70': lambda n, a: 2 if case['fail_anims'].get(case.get('cur', -1)) else 0,
        '0x005569e0': reader_find,
        '0x005570b0': lambda n, a: int(present('ofs', n)),
        '@vec': lambda n, a: struct.unpack('<I', struct.pack('<f', r.uniform(-5, 5)))[0],
        '0x005569f0': lambda n, a: int(present('nodename', n)),
    }
    t.script = script
    t.silent |= {'new', 'delete', '@vec'}
    for addr, spec in CALLS.items():
        e.stub(addr, spec=spec)

    def new(em, key, a, pop):
        em._ret(t.handle('new', em.args('s0')), 0)
    e.stub(0x5abd26, 'new', '', 0, new)
    e.stub(0x5abd1a, 'delete', 's0', 0, lambda em, k, a, p: em._ret(0, 0))

    def cameras(em, key, a, pop):
        this = em.args('c')[0]
        em.w32(this + 0x24, t.handle(key, [this]))
        em._ret(0, pop)
    e.stub(0x430e70, '0x00430e70', 'c', fn=cameras)

    def anims(em, key, a, pop):
        this = em.args('c')[0]
        if t.handle(key, [this]) == 2:
            em.w8(this + 0x20, 0)
        em._ret(0, pop)
    e.stub(0x431a70, '0x00431a70', 'c', fn=anims)

    def vec3(em, key, a, pop):
        node, key_s, out = em.args('c,zd,s0')
        v = t.handle(key, [node, key_s])
        if v:
            for i in range(3):
                em.w32(out + 4 * i, t.handle('@vec', [i]))
        em._ret(v, pop)
    e.stub(0x5570b0, '0x005570b0', 'c,zd,s0', fn=vec3)

    def get_string(em, key, a, pop):
        node, key_s = em.args('c,zd')
        v = t.handle(key, [node, key_s])
        p = em.world_string(f'stub{v:x}') if v else 0
        t.override(key, p)
        em._ret(p, pop)
    e.stub(0x5569f0, '0x005569f0', 'c,zd', fn=get_string)

    # MFC's CString: a pointer to its characters.
    empty = e.world_string('')

    def cs_ctor(em, key, a, pop):
        em.w32(em.args('c')[0], empty)
        em._ret(em.args('c')[0], 0)

    def cs_ctor_str(em, key, a, pop):
        this, src = em.args('c,s0')
        em.w32(this, em.world_string(em.cstr(src)))
        em._ret(this, 4)

    def cs_set_at(em, key, a, pop):
        this, i, ch = em.args('c,s0,s1')
        s = list(em.cstr(em.r32(this)))
        s[i] = chr(ch & 0xff)
        em.w32(this, em.world_string(''.join(s)))
        em._ret(0, 8)

    def cs_plus(em, key, a, pop):
        ret, left, right = em.args("s0,s1,s2")
        em.w32(ret, em.world_string(em.cstr(left) + em.cstr(em.r32(right))))
        em._ret(ret, 0xc)  # AFXAPI: __stdcall

    def cs_assign(em, key, a, pop):
        this, src = em.args('c,s0')
        em.w32(this, em.r32(src))
        em._ret(this, 4)

    def cs_format(em, key, a, pop):
        this, fmt = em.args('s0,s1')
        f = em.cstr(fmt)
        vals = [em.cstr(v) for v in em.args(','.join(f's{2 + k}' for k in range(f.count('%s'))))] if '%s' in f else []
        em.w32(this, em.world_string(c_format(f, vals)))
        em._ret(0, 0)
    def sprintf(em, key, a, pop):
        buf, fmt = em.args('s0,s1')
        f = em.cstr(fmt)
        vals = [em.cstr(v) for v in em.args(','.join(f's{2 + k}' for k in range(f.count('%s'))))] if '%s' in f else []
        s = c_format(f, vals)
        em.cpu.write(buf, s.encode('latin1') + b'\0')
        em._ret(len(s), 0)
    e.import_stub(0x5bb658, 'sprintf', '', 0, sprintf)
    e.stub(0x5abdb0, 'cs', '', 0, cs_ctor)
    e.stub(0x5abda4, 'cs~', '', 0, lambda em, k, a, p: em._ret(0, 0))
    e.stub(0x5abe04, 'cs(s)', '', 4, cs_ctor_str)
    e.stub(0x5abe10, 'SetAt', '', 8, cs_set_at)
    e.stub(0x5abe0a, 'operator+', '', 0, cs_plus)
    e.stub(0x5abdf2, 'operator=', '', 4, cs_assign)
    e.stub(0x5abdaa, 'Format', '', 0, cs_format)

    # The two lists' sentinels (0x430250 at start-up, outside the trace).
    for lst, cnt in ((CACHE, COUNT), (STUBS_LIST, STUBS_LIST + 4)):
        s = e.alloc(0xc)
        e.w32(s, s), e.w32(s + 4, s)
        e.w32(lst, s), e.w32(cnt, 0)
    return t, e


def cache(e):
    s, out = e.r32(CACHE), []
    p = e.r32(s)
    while p != s:
        out.append(e.r32(p + 8))
        p = e.r32(p)
    return out


def compare_blocks(probe, name, case, e, live):
    bad = []
    for addr, size in case['blocks']:
        if addr not in live:
            continue
        mem = e.cpu.read(addr, size)
        img = (ctypes.c_uint8 * size)(*([FILL] * size))
        mask = (ctypes.c_uint8 * size)()
        k = probe.probe_md_image(addr, size, img, mask)
        if k < 0:
            bad.append(f'{addr:#x}: no live port object ({k})')
            continue
        for i in range(size):
            if mask[i]:
                if mem[i] != img[i] and not (mem[i] == FILL and img[i] == 0):
                    bad.append(f'{["data", "soils"][k]} +{i:#x}: original {mem[i]:#04x}, port {img[i]:#04x}')
            elif mem[i] != FILL:
                bad.append(f'{["data", "soils"][k]} +{i:#x}: written by the original, not modelled')
            if len(bad) > 6:
                break
    if bad:
        print(f'{name}: blocks differ:\n  ' + '\n  '.join(bad[:8]))
    return not bad


def run(probe, i, stats):
    case = make_case(i)
    t, e = setup(case)
    probe.probe_md_reset()
    for x, (d, reader, skins) in case['dfn'].items():
        probe.probe_md_dfn(d, reader, skins, *case['ranges'][x])
    want = []
    for op in case['ops']:
        try:
            if op[0] == 'get':
                case['cur'] = op[1]
                want.append(e.call(GET, ecx=op[1], edx=op[2]))
            elif op[0] == 'scale':
                case['cur'] = -1
                e.call(SCALE, stack=[struct.unpack('<I', struct.pack('<f', op[1]))[0]])
                want.append(0)
            else:
                e.call(REFRESH if op[0] == 'refresh' else CLEAR)
                want.append(0)
        except Exception:
            from unicorn.x86_const import UC_X86_REG_EIP, UC_X86_REG_ESP
            esp = e.uc.reg_read(UC_X86_REG_ESP)
            print(f'  {op} at {e.uc.reg_read(UC_X86_REG_EIP):#x} (return {e.r32(esp):#x}); the last calls: {t.log[-3:]}')
            raise
        want.append(tuple(cache(e)))
        if op[0] != 'clear':
            live = set(cache(e)) | {e.r32(p + 0x230) for p in cache(e)}
            case.setdefault('checks', []).append(live)
    snap = e.snapshot()
    rep = Replay(t, e, snap)
    rep.script = t.script
    probe.probe_app_set_callback(rep.callback())
    got = []
    buf = (U * 64)()
    for op in case['ops']:
        if op[0] == 'get':
            case['cur'] = op[1]
            got.append(probe.probe_md_get(op[1], op[2]))
        elif op[0] == 'refresh':
            probe.probe_md_refresh()
            got.append(0)
        elif op[0] == 'scale':
            case['cur'] = -1
            probe.probe_md_scale(op[1])
            got.append(0)
        else:
            probe.probe_md_clear()
            got.append(0)
        n = probe.probe_md_cache(buf, 64)
        got.append(tuple(buf[:n]))
    name = f'case {i} (mech {case["roster"]})'
    ok = compare(name, t.log, rep.log)
    if want != got:
        print(f'{name}: results differ: original {want}, port {got}')
        ok = False
    stats['calls'] += len(t.log)
    stats['gets'] += sum(op[0] == 'get' for op in case['ops'])
    return ok, case, e


def run_blocks(probe, i, stats):
    """The images: one session up to its last get, compared before the clear."""
    case = make_case(i)
    case['ops'] = [op for op in case['ops'] if op[0] != 'clear'] or [('get', case['roster'], 1)]
    t, e = setup(case)
    probe.probe_md_reset()
    for x, (d, reader, skins) in case['dfn'].items():
        probe.probe_md_dfn(d, reader, skins, *case['ranges'][x])
    for op in case['ops']:
        case['cur'] = op[1] if op[0] == 'get' else -1
        if op[0] == 'get':
            e.call(GET, ecx=op[1], edx=op[2])
        elif op[0] == 'scale':
            e.call(SCALE, stack=[struct.unpack('<I', struct.pack('<f', op[1]))[0]])
        else:
            e.call(REFRESH)
    rep = Replay(t, e, e.snapshot())
    rep.script = t.script
    probe.probe_app_set_callback(rep.callback())
    for op in case['ops']:
        case['cur'] = op[1] if op[0] == 'get' else -1
        if op[0] == 'get':
            probe.probe_md_get(op[1], op[2])
        elif op[0] == 'scale':
            probe.probe_md_scale(op[1])
        else:
            probe.probe_md_refresh()
    live = set(cache(e)) | {e.r32(p + 0x230) for p in cache(e)}
    stats['blocks'] += len(live)
    ok = compare(f'case {i} blocks', t.log, rep.log) and compare_blocks(probe, f'case {i}', case, e, live)
    return ok


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 152
    probe = setup_probe()
    stats = {'calls': 0, 'gets': 0, 'blocks': 0}
    failed = 0
    for i in range(n):
        ok, _, _ = run(probe, i, stats)
        ok = run_blocks(probe, i, stats) and ok
        if not ok:
            failed += 1
            if failed > 3:
                break
    if failed:
        raise SystemExit(f'{failed} of {n} cases differ')
    print(f'ok: {n} sessions on all 19 mechs, {stats["gets"]} lookups, {stats["calls"]} calls and '
          f'{stats["blocks"]} data and soils blocks identical')


if __name__ == '__main__':
    try:
        main()
    except SystemExit as ex:
        print(ex)
        sys.stdout.flush()
        os._exit(1 if ex.code else 0)
    sys.stdout.flush()
    os._exit(0)
