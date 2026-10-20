"""The AI's reader files against the original x86 (docs/ai.md, Data files):
the reader-file base (0x4de9c0..0x4ded60), the net and region files
(ZRdrAiNet.cpp: the cache by name 0x4da570, the header 0x4da900, the node
records 0x4db850 with 0x4dae40 / 0x4db140 / 0x4daee0 / 0x4db1b0, the record
0x4c6bd0, the lookups 0x4de730 / 0x4de830) and the valves they name
(0x4d3c40).

    cmake --build build && .venv/bin/python tests/differential/test_ai_readers.py

Every net_NN.zrd and reg_*.zrd in every mission's reader archive, opened by
number and by name, then a few hundred made-up files that reach the paths the
shipped ones do not (old and new versions, the "mech3" tag, missing and
reserved names, netparams, list-first nodes, every property, dangling keys,
negative and float links, second references, reloads). Both sides read the
same bytes; their output streams (tests/probe_ai.cpp) and their logs of zerr
reports, localize and zSound calls must match exactly. Load-time code: the
x87 at its power-on precision.
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from ai_world import ROOT, AivRun, Emulated, archive, mission_archives, ser  # noqa: E402
import zrd  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
probe.probe_ainet_run.argtypes = [ctypes.c_char_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int,
                                  ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p,
                                  ctypes.c_int, ctypes.c_void_p]
probe.probe_aiv_run.argtypes = [ctypes.c_char_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p, ctypes.c_int,
                                ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p]
WORK = ROOT / 'build' / 'ai_readers'
# Files the original crashes on, which the port reads (docs/fixes.md).
CRASHES = {'not a list'}
# Places in the original the files must reach.
COVER = {0x4da95c: 'past version', 0x4da973: 'future version', 0x4da9d5: '"mech3" upgrade',
         0x4da9e9: 'wrong version tag', 0x4daa0c: 'no version', 0x4daa8a: 'reserved name',
         0x4dabda: 'name != net_NN', 0x4dac33: 'file != net_NN', 0x4dab68: '107 without a name',
         0x4dab47: '108 without a name', 0x4dab16: 'netparams pos', 0x4db8aa: 'list-first node',
         0x4db290: 'valve value', 0x4db38c: 'hidden', 0x4db3b0: 'fixed', 0x4db3d4: 'autonext', 0x4db3f8: 'visible',
         0x4db41e: 'deploy', 0x4db46d: 'mcbwait', 0x4db4ad: 'arrangenodes', 0x4db4d0: 'sound', 0x4db4f1: 'backup',
         0x4db508: 'gate', 0x4db52a: 'shunt', 0x4db556: 'valveunion', 0x4db5c9: 'valveunion entry',
         0x4db5de: 'valveunion count', 0x4db7ff: 'valve_assign', 0x4dea66: 'wrong suffix', 0x4dea86: '.zrd suffix',
         0x4decbc: 'missing file', 0x4deb70: 'second reference', 0x4debb9: 'last release unloads',
         0x4df7b6: 'team from the name', 0x4df9bc: 'p4 order', 0x4dfbfe: 'pilot name', 0x4dffec: 'epona voice',
         0x4e0027: 'alan voice', 0x4e0062: 'dom voice', 0x4e00b3: 'lancemate', 0x4e01e6: '17-item order',
         0x4e027f: 'order region', 0x4e0298: 'order valve', 0x4e0308: 'mech3 tag', 0x4e0351: 'p8 with old tags',
         0x4e03f7: 'p3 alone', 0x4e03d8: 'p3 and p7 differ', 0x4e0431: 'p5 jump', 0x4e043d: 'p5 both',
         0x4e045a: 'p5 none'}
CAP, LOGCAP = 1 << 22, 1 << 18


def port_events(buf, n):
    out, i = [], 0
    while i < n:
        k = buf[i]
        out.append(tuple(buf[i + 1:i + 1 + k]))
        i += 1 + k
    return out


def port_nets(files, steps, named):
    sets = files if isinstance(files, list) else [files]
    paths = []
    for k, fs in enumerate(sets):
        paths.append(WORK / f'nets{k}.zbd')
        paths[-1].write_bytes(archive(fs))
    path = paths[0]
    # op 3 switches to archive `net`: the probe takes its path as the name.
    steps = [(s[0], str(paths[s[0]]), 3) if s[2] == 3 else s for s in steps]
    n = len(steps)
    nets = (ctypes.c_int32 * n)(*[s[0] for s in steps])
    keep = [ctypes.c_char_p(s[1].encode() if s[1] is not None else None) for s in steps]
    names = (ctypes.c_char_p * n)(*keep)
    ops = (ctypes.c_int32 * n)(*[s[2] for s in steps])
    nk = [ctypes.c_char_p(k) for k in named]
    nnamed = (ctypes.c_char_p * max(len(named), 1))(*nk)
    out = (ctypes.c_uint8 * CAP)()
    log = (ctypes.c_uint32 * LOGCAP)()
    nlog = ctypes.c_int()
    r = probe.probe_ainet_run(str(path).encode(), nets, names, ops, n, nnamed, len(named), out, CAP, log, LOGCAP,
                              ctypes.byref(nlog))
    if r < 0:
        raise SystemExit(f'probe_ainet_run: {r}')
    return bytes(out[:r]), port_events(log, nlog.value)


def port_aiv(files, names):
    path = WORK / 'aiv.zbd'
    path.write_bytes(archive(files))
    keep = [ctypes.c_char_p(k) for k in names]
    arr = (ctypes.c_char_p * max(len(names), 1))(*keep)
    out = (ctypes.c_uint8 * CAP)()
    log = (ctypes.c_uint32 * LOGCAP)()
    nlog = ctypes.c_int()
    r = probe.probe_aiv_run(str(path).encode(), arr, len(names), out, CAP, log, LOGCAP, ctypes.byref(nlog))
    if r < 0:
        raise SystemExit(f'probe_aiv_run: {r}')
    return bytes(out[:r]), port_events(log, nlog.value)


def first_diff(a, b):
    for i in range(min(len(a), len(b))):
        if a[i] != b[i]:
            return i
    return min(len(a), len(b))


class Checker:
    def __init__(self):
        self.emu = Emulated()
        self.emu.cover(COVER)
        self.cases = self.fails = 0

    def nets(self, label, files, steps, named=()):
        self.cases += 1
        named = [k.encode() if isinstance(k, str) else k for k in named]
        try:
            mine, mlog = self.emu.run_nets(files, steps, named)
        except Exception as e:  # the original crashed
            if label in CRASHES:  # a FIX (bug) case: the port must read it without crashing
                port_nets(files, steps, named)
            else:
                self.fails += 1
                print(f'CRASH {label}: {e}')
            reached = self.emu.reached
            self.emu = Emulated()
            self.emu.cover(COVER)
            self.emu.reached = reached
            return None
        port, plog = port_nets(files, steps, named)
        if mine != port or mlog != plog:
            self.fails += 1
            if self.fails <= 10:
                i = first_diff(mine, port)
                print(f'MISMATCH {label}: stream {len(mine)} vs {len(port)} bytes, first difference at {i}; '
                      f'logs {len(mlog)} vs {len(plog)}')
                print('  original', mine[max(0, i - 16):i + 32].hex())
                print('  port    ', port[max(0, i - 16):i + 32].hex())
                if mlog != plog:
                    j = first_diff(mlog, plog)
                    print('  log original', mlog[j:j + 4], 'port', plog[j:j + 4])
        return mine

    def aiv(self, label, files, names):
        self.cases += 1
        names = [k.encode() if isinstance(k, str) else k for k in names]
        try:
            mine, mlog = AivRun(self.emu).run(files, names)
        except Exception as e:  # the original crashed
            self.fails += 1
            print(f'CRASH {label}: {e}')
            reached = self.emu.reached
            self.emu = Emulated()
            self.emu.cover(COVER)
            self.emu.reached = reached
            return
        port, plog = port_aiv(files, names)
        if mine != port or mlog != plog:
            self.fails += 1
            if self.fails <= 10:
                i = first_diff(mine, port)
                print(f'MISMATCH {label}: stream {len(mine)} vs {len(port)} bytes, first difference at {i}; '
                      f'logs {len(mlog)} vs {len(plog)}')
                print('  original', mine[max(0, i - 16):i + 32].hex())
                print('  port    ', port[max(0, i - 16):i + 32].hex())
                if mlog != plog:
                    j = first_diff(mlog, plog)
                    print('  log original', mlog[j:j + 4], 'port', plog[j:j + 4])


def record_names(data):
    root, _ = zrd.node(data, 0)
    if not (isinstance(root, list) and len(root) == 1 and isinstance(root[0], list)):
        return []
    top = root[0]
    return [top[i] for i in range(0, len(top) - 1, 2) if isinstance(top[i], str)]


def shipped_aiv(check):
    count = 0
    for path, files in mission_archives():
        if 'aiv.zrd' not in files:
            continue
        names = record_names(files['aiv.zrd']) + ['no_such_vehicle']
        check.aiv(f'{path.parent.name}/{path.name}/aiv.zrd', {'aiv.zrd': files['aiv.zrd']}, names)
        count += 1
    return count


# --- made-up files -----------------------------------------------------------
def node(pos=(1.0, 2.0, 3.0), links=(1,), props=()):
    return [12, list(pos), list(links)] + list(props)


def net_file(version=(106,), name='net_01', nodes=(), extra=()):
    top = []
    if version is not None:
        top += ['version', list(version)]
    if name is not None:
        top += ['name', [name]]
    top += list(extra)
    for k, v in nodes:
        top += [k, v]
    return ser([top])


PROPS = [
    ['speed', [1, 0.5]], ['speed', [0, 2]], ['canleave', [1, 0]], ['wait_interval', [1, 3.5]],
    ['wait_time', [1, 12.0]], ['valve', [1, 'v_alpha']], ['valve', [0, 'v_beta', 7]], ['valve', [1, 'V_ALPHA', 0]],
    ['name', ['MSG_NODE_X']], ['auxname', ['aux one']], ['name', ['plain']], ['jump', [1, 25.0]],
    ['scan_aim', [3, 4]], ['scan_time', [1, 0]], ['scan_hit', [2, 5]], ['hidden', [1]], ['hidden', [0]],
    ['fixed', [2]], ['autonext', [0]], ['visible', [0]], ['visible', [9]],
    ['deploy', ['d_one', 3, 'd_two', 258, 70000]], ['mcbwait', ['m_valve', 300, 4.25]], ['arrangenodes', [12.5]],
    ['sound', ['snd_x']], ['backup', []], ['gate', ['gate_a']], ['shunt', []],
    ['valveunion', [[1, 'u1', 2], [0, 'u2', 3], [1, 'u3'], [5, 'v_alpha', -4]]], ['valveunion', []],
    ['valveunion', [[1, 'u9', 1]]], ['valve_assign', ['assign_v', 9]], ['valve_assign', ['v_alpha', -1]],
    ['jump', [1, 3]], ['speed', [1.5, 1]], ['scan_aim', [1.25, -2]],
]


def variants(rng):
    out = []
    # The header's paths.
    for ver in [None, (104,), (105,), (105, 'mech3'), (105, 'mech2'), (106,), (106, 'mech3'), (107,), (108,),
                (109,), (110,), (99, 'x')]:
        for name in [None, 'net_01', 'net_02', 'mcbwaynet', 'other']:
            out.append((f'header {ver} {name}', {'net_01.zrd': net_file(ver, name, [('node_00', node())])},
                        [(1, None, 0)]))
    out.append(('file name', {'other.zrd': net_file((105,), 'net_05', [('node_00', node())])}, [(5, 'other', 0)]))
    out.append(('netparams pos', {'net_03.zrd': net_file((106,), 'net_03', [('node_00', node())],
                                                         ['netparams', ['pos', [5.5, -6.0, 7]]])}, [(3, None, 0)]))
    out.append(('netparams empty', {'net_03.zrd': net_file((106,), 'net_03', [('node_00', node())],
                                                           ['netparams', ['x', 1]])}, [(3, None, 0)]))
    out.append(('suffix', {'net_04.zrd': net_file((106,), 'net_04', [('node_00', node())])}, [(4, 'net_04.zrd', 0)]))
    out.append(('suffix wrong', {'net_04.zrd': net_file((106,), 'net_04', [('node_00', node())])},
                [(4, 'net_04.zrx', 0)]))
    out.append(('missing file', {}, [(7, None, 0), (7, None, 0), (7, None, 1)]))
    # Shapes the record walk takes.
    for label, top in [('empty', []), ('no nodes', ['version', [106], 'name', ['net_01']]),
                       # Every record value long enough for what is read of it: past the
                       # end the exe reads the heap, the port a zeroed slot (zreader.md).
                       ('node first', ['node_00', node(), 'version', [106, [1.0, 2.0, 3.0]]]),
                       ('key in the middle', ['version', [106], 'node_00', node(), 'junk', [12, [0.0, 0.0, 0.0]],
                                              'node_01', node()]),
                       ('two lists', None), ('not a list', None)]:
        if top is None:
            data = ser([['version', [106]], ['node_00', node()]]) if label == 'two lists' else ser(5)
        else:
            data = ser([top])
        out.append((label, {'net_01.zrd': data}, [(1, None, 0)]))
    # Node kinds and links.
    kinds = [('node_00', node()), ('node_01', node(links=())), ('node_02', [12, [1.0, 2.0, 3.0]]),
             ('node_03', [12, [4.0, 5.0, 6.0], [3, -1, 0, 2.5, -2.5, 7]]),
             ('node_04', [[1, 2, -3], 'canleave', [1, 1]]), ('node_05', [[]]), ('node_6', node()),
             ('node_123', node()), ('node_-2', node()), ('node_x', node()),
             ('node_07', [12, [1.0, 2.0, 3.0], [1], 'speed', [1, 0.25]]), ('node_08', [5, [9.0, 9.0, 9.0], [2]])]
    out.append(('kinds', {'net_05.zrd': net_file((106,), 'net_05', kinds)}, [(5, None, 0)]))
    # Every property, alone and in random sets.
    for i, p in enumerate(PROPS):
        out.append((f'prop {p[0]} {i}', {'net_06.zrd': net_file((106,), 'net_06', [
            ('node_00', node(props=p)), ('node_01', [[0, 1]] + p)])}, [(6, None, 0)]))
    for i in range(120):
        nodes = []
        for k in range(rng.randrange(1, 6)):
            props = []
            for p in rng.sample(PROPS, rng.randrange(0, 8)):
                props += p
            if rng.random() < 0.3:
                nodes.append((f'node_{k:02d}', [[rng.randrange(-2, 9) for _ in range(rng.randrange(0, 4))]] + props))
            else:
                nodes.append((f'node_{k:02d}', node(links=[rng.randrange(-2, 9) for _ in range(rng.randrange(0, 4))],
                                                    props=props)))
        out.append((f'random {i}', {'net_08.zrd': net_file((rng.choice([105, 106, 108]),), 'net_08', nodes)},
                    [(8, None, 0)]))
    # A file changed between loads: the header keeps what the new one lacks.
    a = {'net_10.zrd': net_file((105, 'mech3'), 'net_10', [('node_00', node())], ['netparams', ['pos', [1.0, 2.0, 3.0]]])}
    b = {'net_10.zrd': net_file(None, None, [('node_00', node()), ('node_01', node())], ['netparams', ['other', 1]])}
    c = {'net_10.zrd': net_file((107,), 'renamed', [('node_00', node(props=['valve', [1, 'late']]))])}
    out.append(('reloaded', [a, b, c], [(10, None, 0), (10, None, 1), (1, None, 3), (10, None, 0), (10, None, 1),
                                        (2, None, 3), (10, None, 0)]))
    # References: twice, released, reloaded, by number and by name, a region.
    two = {'net_09.zrd': net_file((106,), 'net_09', [('node_00', node(props=['valve', [1, 'r1']]))]),
           'reg_area.zrd': net_file((108,), 'reg_area', [('node_00', node()), ('node_01', node(links=[0]))])}
    out.append(('refs', two, [(9, None, 0), (9, None, 0), (9, None, 1), (9, None, 2), (9, None, 1),
                              (9, None, 0), (9, 'net_09', 0), (0, 'reg_area', 0), (4, 'reg_area', 0),
                              (0, None, 1), (0, None, 1)]))
    return out


ENTRY = [1, 0, 0.25, 0, 1, 0.999, 0, 0.8, 1, 'reg_a', 0, 0.0, 1, 60.0, 1, 'valve_a', 1]
P4 = [1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 1, 0, 0.5, 1, 0, 0.75, 1, 0.9, 1, 'reg_p4', 1, 12.0, 0, 40.0,
      'valve_p4']


def aiv_rec(kind=4, pos=(1.0, 2.0, 3.0), yaw=30.0, team=2, props=()):
    return [kind, list(pos), yaw] + ([team] if team is not None else []) + list(props)


def aiv_file(records):
    top = []
    for k, v in records:
        top += [k, v]
    return ser([top])


AIV_PROPS = [
    ['params', [1000.0, 0, 500.0, 130.0, 80.0, 60.0, 1000.0, 2.5, 2.0]], ['params', [1, 2, 3, 4, 5, 6, 7, 8, 9]],
    ['p2', [0, 1, 1, 0, 50.0, 50.0, 20.0, 0.1]], ['p2', [3, 0, 0, 1, 1.0, 2.0, 3.0, 4.0]],
    ['p6', [0, 8.0, 1, 0]], ['p8', [12.0, 14.0, 4.0, 6.0, 1, 40.0, 30.0, 50.0, 0.7, 1, 0, 0.4, 0.5, 0, 1, 0, 0]],
    ['p9', [4.0, 4.0, 12.0, 1, 0.05]], ['p9', [1.0, 2.0, 3.0, 0, 0.5]], ['p10', [2.2, 3]], ['p11', [7.5]],
    ['p12', ['owens', 0, 1]], ['p12', ['MADCAT', 1, 0]], ['p12', ['nosuchmech', 1, 1]], ['p13', [1]], ['p13', [0]],
    ['pilot', ['MSG_AIV_X']], ['pilot', ['MSG_AIV_Y', 'can_configure', ['true']]],
    ['pilot', ['MSG_AIV_Z', 'can_configure', ['false']]], ['pilot', [5, 'can_configure', [1]]],
    ['sound_tag', ['dom']], ['sound_tag', [0]], ['sound_tag', [1.5]], ['sound_tag', 'alan'], ['sound_tag', [7]],
    ['skin', [3]], ['lancemate', ['on']], ['lancemate', ['off']], ['lancemate', [2]], ['lancemate', [7]],
    ['lancemate', ['NONE']], ['lancemate', ['bogus']], ['lancemate', [1.5]], ['lancemate', ['default']],
    ['vlist', [ENTRY, ENTRY[:16], [0, 1, 1.5, 1, 0, 2.0, 1, 3.0, 0, 0, 1, 4.0, 0, 5.0, 0, 'valve_b', 0]]],
    ['vlist', []], ['vlist', [ENTRY[:9] + ['reg_b'] + ENTRY[10:15] + ['valve_a'] + [0]]],
    ['vlist', [ENTRY[:15] + [0, 1]]], ['mech3', [1]], ['mech3', [0]],
]
OLD_TAGS = {'p3': ['p3', [1.0, 2.0, 3.0, 4.0, 5.0, 6.0]], 'p3same': ['p3', [6.0, 5.0, 7.0, 8.0, 9.0, 10.0]],
            'p4': ['p4', P4], 'p5': ['p5', [1.0, 2.0, 3.0, 1, 1]], 'p7': ['p7', [1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0]],
            'p8': AIV_PROPS[5]}


def aiv_variants(rng):
    out = []
    names = ['player', 'stealth', 'Epona_Rhi', 'Alan_Mattila', 'Dominic_Paine', 'Alan_Matilla', 'truck']
    # Short records: the team from the name; one item more: the team given.
    recs = [(n, aiv_rec(team=None, props=())) for n in names] + [('given', aiv_rec(team=1))]
    recs += [('only', [3, [1.0, 2.0, 3.0], 45])]
    out.append(('short records', recs))
    for i, p in enumerate(AIV_PROPS):
        out.append((f'aiv prop {p[0]} {i}', [(names[i % len(names)], aiv_rec(props=p)), ('second', aiv_rec(props=p))]))
    # The old tags and their checks, every combination.
    keys = ['p3', 'p4', 'p5', 'p7', 'p8']
    for mask in range(32):
        props = []
        for b, k in enumerate(keys):
            if mask >> b & 1:
                props += OLD_TAGS[k]
        out.append((f'old tags {mask:05b}', [('v', aiv_rec(props=props))]))
    for a, b in [(0, 0), (0, 1), (1, 1), (2, 1), (3, 0), (0, 3), (5, 0)]:
        out.append((f'p5 {a} {b}', [('v', aiv_rec(props=['p5', [1.0, 2.0, 3.0, a, b]]))]))
    out.append(('p3 p7 same', [('v', aiv_rec(props=OLD_TAGS['p3same'] + OLD_TAGS['p7']))]))
    # Random records, random yaws (the degree/radian round trips).
    for i in range(60):
        recs = []
        for k in range(rng.randrange(1, 6)):
            props = []
            for p in rng.sample(AIV_PROPS, rng.randrange(0, 9)):
                props += p
            recs.append((rng.choice(names) + f'_{k}', aiv_rec(kind=rng.randrange(0, 27), yaw=rng.uniform(-400, 400),
                                                              team=rng.choice([None, 0, 1, 2]) if not props else
                                                              rng.choice([0, 1, 2]), props=props)))
        out.append((f'aiv random {i}', recs))
    return out


def shipped(check):
    count = 0
    for path, files in mission_archives():
        for name, data in files.items():
            if not (name.startswith('net_') or name.startswith('reg_')):
                continue
            base = name[:-4]
            if base.startswith('net_') and base[4:].isdigit():
                steps = [(int(base[4:]), None, 0), (int(base[4:]), base, 0)]
            else:
                steps = [(0, base, 0)]
            check.nets(f'{path.parent.name}/{path.name}/{name}', {name: data}, steps, ['node_01', 'node_00x'])
            count += 1
    return count


def main():
    WORK.mkdir(parents=True, exist_ok=True)
    rng = random.Random(1)
    check = Checker()
    n = shipped(check)
    print(f'shipped: {n} net and region files')
    n = shipped_aiv(check)
    print(f'shipped: {n} aiv.zrd files')
    vs = variants(rng)
    for label, files, steps in vs:
        check.nets(label, files, steps, ['node_01', 'node_04', 'missing'])
    print(f'variants: {len(vs)}')
    avs = aiv_variants(rng)
    for label, recs in avs:
        names = [k for k, v in recs] + ['missing']
        check.aiv(label, {'aiv.zrd': aiv_file(recs)}, names)
    print(f'aiv variants: {len(avs)}')
    missed = [v for a, v in COVER.items() if a not in check.emu.reached]
    if missed:
        raise SystemExit(f'not reached in the original: {missed}')
    if check.fails:
        raise SystemExit(f'ai readers: {check.fails} of {check.cases} cases differ')
    print(f'ai readers: {check.cases} cases identical')


if __name__ == '__main__':
    main()
