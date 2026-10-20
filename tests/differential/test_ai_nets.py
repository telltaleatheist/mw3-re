"""The AI's nets compiled, against the original x86 (docs/ai.md section 9):
0x402240 for every net number (0x402170 asks 1..99), through the builder
(CZAiNetEdit 0x4bcba0) into the compiled net (0x4be670 -> 0x4be300: the
slot counts 0x4bd440 / 0x4bd740 / 0x4bd670 / 0x4bd4e0 / 0x4bd5b0, the net
0x4bf910, the steps 0x4be230 / 0x4bde20 / 0x4bd790 / 0x4bdd90 / 0x4bdd70,
each step's class 0x4c9eb0..0x4cff10 and attach 0x4c9e60 / 0x4c94a0 /
0x4cffd0).

    cmake --build build && .venv/bin/python tests/differential/test_ai_nets.py

Every mission's reader archive, then made-up nets: every node property on
its own and in random sets, edge nodes (no position: from, to...),
duplicates, links to missing and unpositioned nodes, deploy, mcbwait and
backup nodes, nets built twice. Both sides' output streams
(tests/probe_ai_net.cpp: the registry, every compiled net's steps with
their data and links as slots) and their logs of zerr, localize and zSound
calls must match. Load-time code: the x87 at 53 bits.
"""
import ctypes
import random

from unicorn.x86_const import UC_X86_REG_EIP
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from ai_world import ROOT, CompiledRun, Emulated, archive, mission_archives, ser  # noqa: E402
import test_ai_readers  # noqa: E402

# deploy's last item names a node by number (read as a short): one far past
# the net reads unknown heap in the exe, so the made-up nets name real ones.
PROPS = [p for p in test_ai_readers.PROPS if p[0] != 'deploy'] + [['deploy', ['d_one', 3, 'd_two', 258, 2]]]

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
probe.probe_nets_run.argtypes = [ctypes.c_char_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p, ctypes.c_int,
                                 ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p]
WORK = ROOT / 'build' / 'ai_nets'
CAP, LOGCAP = 1 << 24, 1 << 20
# Where the original crashes on malformed nets, which the port reads
# (docs/fixes.md): arrangenodes linking past the nodes (0x4c7340), and a
# node's properties put on a link whose node it cannot have (0x4bd790: the
# way into the node's links is past its end, null for a node left with none).
CRASHES = [(0x4c7340, 0x4c74a0), (0x4bd790, 0x4bdd70)]
# Places in the original the nets must reach.
COVER = {0x4bdf5d: 'deploy node', 0x4bdfd8: 'mcbwait node', 0x4be04a: 'backup node', 0x4be09b: 'plain node',
         0x4bdd70: 'edge node on a link', 0x4bd790: 'properties', 0x4bce63: 'duplicate node',
         0x4be0f8: 'arrangenodes waynet', 0x4cbcf0: 'canleave', 0x4cbd40: 'canleave not', 0x4cb950: 'scan',
         0x4cd4a0: 'gate', 0x4cd6e0: 'shunt', 0x4cda50: 'valve union', 0x4cdc20: 'valve assign',
         0x4cce30: 'sound', 0x4cb2a0: 'jump', 0x4ca1c0: 'valve'}


def port_events(buf, n):
    out, i = [], 0
    while i < n:
        k = buf[i]
        out.append(tuple(buf[i + 1:i + 1 + k]))
        i += 1 + k
    return out


def port_nets(files, nets):
    path = WORK / 'nets.zbd'
    path.write_bytes(archive(files))
    arr = (ctypes.c_int32 * max(len(nets), 1))(*nets)
    out = (ctypes.c_uint8 * CAP)()
    log = (ctypes.c_uint32 * LOGCAP)()
    nlog = ctypes.c_int()
    r = probe.probe_nets_run(str(path).encode(), arr, len(nets), out, CAP, log, LOGCAP, ctypes.byref(nlog))
    if r < 0:
        raise SystemExit(f'probe_nets_run: {r}')
    return bytes(out[:r]), port_events(log, nlog.value)


def first_diff(a, b):
    for i in range(min(len(a), len(b))):
        if a[i] != b[i]:
            return i
    return min(len(a), len(b))


class Checker:
    def __init__(self):
        self.emu = self.fresh()
        self.cases = self.fails = self.crashes = 0
        self.size = 0

    @staticmethod
    def fresh():
        e = Emulated()
        e.cover(COVER)
        # A link to a node past the last reads just past the slot map: -1
        # there, the "no node" the port's FIX (bug) takes.
        e.guard = 0xff
        return e

    def nets(self, label, files, nets=tuple(range(1, 100))):
        self.cases += 1
        try:
            mine, mlog = CompiledRun(self.emu).run(files, nets)
        except Exception as e:  # the original crashed
            eip = self.emu.cpu.uc.reg_read(UC_X86_REG_EIP)
            if any(lo <= eip < hi for lo, hi in CRASHES):  # a FIX (bug) case: the port must not crash
                port_nets(files, list(nets))
                self.crashes += 1
            else:
                self.fails += 1
                print(f'CRASH {label}: {e} at {eip:#x}')
            reached = self.emu.reached
            self.emu = self.fresh()
            self.emu.reached = reached
            return
        port, plog = port_nets(files, list(nets))
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
        self.size += len(mine)


def shipped(check):
    n = 0
    for path, files in mission_archives():
        nets = sorted(k for k in files if k.lower().startswith('net_') and k.lower().endswith('.zrd'))
        if not nets:
            continue
        check.nets(f'{path.parent.name}/{path.name}', files)
        n += len(nets)
    return n


# --- made-up nets ----------------------------------------------------------------
def node(pos=(1.0, 2.0, 3.0), links=(1,), props=()):
    return [12, list(pos), list(links)] + list(props)


def net_file(name='net_01', nodes=(), version=(106,)):
    top = ['version', list(version), 'name', [name]]
    for k, v in nodes:
        top += [k, v]
    return ser([top])


def variants(rng):
    out = []
    square = [('node_00', node((0.0, 0.0, 0.0), [1, 3])), ('node_01', node((10.0, 1.0, 0.0), [2, 0])),
              ('node_02', node((10.0, 2.0, 10.0), [3, 1])), ('node_03', node((0.0, 3.0, 10.0), [0, 2]))]
    out.append(('square', {'net_01.zrd': net_file(nodes=square)}))
    for i, p in enumerate(PROPS):
        nodes = [('node_00', node((0.0, 0.0, 0.0), [1, 2], p)), ('node_01', node((5.0, 0.0, 0.0), [2])),
                 ('node_02', node((5.0, 0.0, 5.0), [0], p))]
        out.append((f'prop {p[0]} {i}', {'net_02.zrd': net_file('net_02', nodes)}))
    # Edge nodes: (from to ...) with properties for those links.
    for i, p in enumerate(PROPS[::3]):
        nodes = square + [('node_04', [[0, 1, 3, 9]] + p), ('node_05', [[2, 3]] + p), ('node_06', [[1]] + p),
                          ('node_07', [[3, 0, 0]] + p)]
        out.append((f'edge {p[0]} {i}', {'net_03.zrd': net_file('net_03', nodes)}))
    out.append(('duplicate', {'net_04.zrd': net_file('net_04', square + [('node_02', node((9.0, 9.0, 9.0), [0]))])}))
    out.append(('to nothing', {'net_05.zrd': net_file('net_05', [
        ('node_00', node((0.0, 0.0, 0.0), [1, 7, 2])), ('node_01', node((5.0, 0.0, 0.0), [0, 2])),
        ('node_02', [[0, 1], 'speed', [1, 0.5]]), ('node_03', node((1.0, 1.0, 1.0), [2, 2, 2]))])}))
    out.append(('to nothing last', {'net_05.zrd': net_file('net_05', [
        ('node_00', node((0.0, 0.0, 0.0), [1, 7])), ('node_01', node((5.0, 0.0, 0.0), [0, 9, 8]))])}))
    out.append(('special nodes', {'net_06.zrd': net_file('net_06', [
        ('node_00', node((0.0, 0.0, 0.0), [1, 2, 3], ['deploy', ['d_one', 3, 'd_two', 258, 2]])),
        ('node_01', node((5.0, 0.0, 0.0), [2], ['mcbwait', ['m_valve', 300, 4.25], 'auxname', ['aux1']])),
        ('node_02', node((5.0, 0.0, 5.0), [3], ['backup', [], 'name', ['MSG_X']])),
        ('node_03', node((0.0, 0.0, 5.0), [0], ['deploy', ['d_three', 1, 'd_one', 0, 9]])),
        ('node_04', node((1.0, 1.0, 1.0), [], ['arrangenodes', [5.0]]))])}))
    out.append(('several nets', {f'net_{k:02d}.zrd': net_file(f'net_{k:02d}', square) for k in (1, 2, 7, 42, 99)}))
    out.append(('built twice', {'net_01.zrd': net_file(nodes=square)}, (1, 1, 2, 1)))
    out.append(('out of range', {'net_01.zrd': net_file(nodes=square)}, (0, -1, 100, 1, 400, 10000)))
    out.append(('region version', {'net_01.zrd': net_file(nodes=square, version=(108,))}))
    for i in range(80):
        nodes = []
        n = rng.randrange(1, 9)
        for k in range(n):
            props = []
            for p in rng.sample(PROPS, rng.randrange(0, 5)):
                props += p
            links = [rng.randrange(-1, n + 2) for _ in range(rng.randrange(0, 4))]
            if rng.random() < 0.25:
                nodes.append((f'node_{k:02d}', [links] + props))
            else:
                pos = (rng.uniform(-500, 500), rng.uniform(-20, 20), rng.uniform(-500, 500))
                nodes.append((f'node_{k:02d}', node(pos, links, props)))
        out.append((f'random {i}', {'net_08.zrd': net_file('net_08', nodes)}))
    return out


def main():
    rng = random.Random(3025)
    WORK.mkdir(parents=True, exist_ok=True)
    check = Checker()
    n = shipped(check)
    print(f'shipped: {n} net files in {check.cases} archives')
    vs = variants(rng)
    for v in vs:
        check.nets(*v)
    print(f'variants: {len(vs)} ({check.crashes} crash the original, which the port reads)')
    missed = sorted(set(COVER) - check.emu.reached)
    for a in missed:
        print(f'NOT REACHED {a:#x}: {COVER[a]}')
    if check.fails or missed:
        sys.exit(f'ai nets: {check.fails} of {check.cases} cases differ')
    print(f'ai nets: {check.cases} cases identical')


if __name__ == '__main__':
    main()
