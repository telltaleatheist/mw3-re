"""The editor's net save read by mech3ax (TerranMechworks/mech3ax, its unzbd), as a cross-check of the archive
and zrd writing in mech3/net_save (docs/editor.md). Every MW3 reader archive holding nets is saved three ways by
tests/probe_ai_netwrite.cpp's probe_net_resave and extracted by unzbd beside the shipped one:

  0 unedited: every member the same JSON;
  1 every net, region and waynet node re-encoded from its record: the other members the same JSON, each net
    member's keys in the same order and every node's position and links the same 32-bit values;
  2 c1's readerm1.zbd with net_01.zrd edited (node_00 moved, node_01's speed, node_99 added): those as saved,
    every other node and member the same JSON.

    cmake --build build --target mw3probe && .venv/bin/python -P tests/differential/test_net_save_mech3ax.py

Needs unzbd built in ../mech3ax (cargo build --release; MECH3AX_UNZBD for another place); skipped without it. BREAK=1 has the probe's mode 0 save
c1 readerm1.zbd re-encoded (mode 1): it must fail.
"""
import ctypes
import json
import shutil
import os
import struct
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import load_probe  # noqa: E402
from original import ROOT  # noqa: E402

# Beside the checkout (a worktree's game/ links to the main one's); MECH3AX_UNZBD overrides.
UNZBD = Path(os.environ.get('MECH3AX_UNZBD') or
             (ROOT / 'game').resolve().parent.parent / 'mech3ax' / 'target' / 'release' / 'unzbd')
probe = load_probe()
probe.probe_net_resave.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]


def extract(zbd, out):
    subprocess.run([str(UNZBD), 'mw', 'reader', str(zbd), str(out)], check=True, capture_output=True)
    with zipfile.ZipFile(out) as z:
        return {n: json.loads(z.read(n)) for n in z.namelist()}


def is_net(member):
    n = member[:-5] if member.endswith('.json') else member
    return n.endswith('.zrd') and (n.startswith('net_') or n.startswith('reg_') or 'waynet' in n)


def bits(v):
    """A number's 32-bit pattern, as the net reader takes it (an int's or a float's)."""
    return struct.pack('<i', v) if isinstance(v, int) else struct.pack('<f', v)


def pairs(tree):
    kv = tree[0]
    return [(kv[i], kv[i + 1] if i + 1 < len(kv) else None) for i in range(0, len(kv), 2)]


def pos_links(node):
    if isinstance(node, list) and node and isinstance(node[0], int):
        pos = node[1] if len(node) > 1 and isinstance(node[1], list) else []
        links = node[2] if len(node) > 2 and isinstance(node[2], list) else []
    elif isinstance(node, list) and node and isinstance(node[0], list):
        pos, links = [], node[0]
    else:
        return None
    return [bits(p) for p in pos], [bits(link) for link in links if bits(link) >= b'\0\0\0\0' and
                                    struct.unpack('<i', bits(link))[0] >= 0]


def main():
    if not UNZBD.exists():
        print(f'skipped: no {UNZBD}')
        return
    brk = int(os.environ.get('BREAK', '0'))
    archives = sorted(p for p in (ROOT / 'game' / 'mw3-1.2' / 'zbd').rglob('reader*.zbd'))
    tmp = Path(tempfile.mkdtemp(prefix='net_save_mx_', dir=ROOT / 'build'))
    try:
        check(archives, tmp, brk)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def check(archives, tmp, brk):
    counts = {'archives': 0, 'members': 0, 'nets': 0, 'nodes': 0}
    for zbd in archives:
        orig = extract(zbd, tmp / 'orig.zip')
        if not any(is_net(m) for m in orig):
            continue
        counts['archives'] += 1
        modes = (0, 1, 2) if zbd.parent.name == 'c1' and zbd.name == 'readerm1.zbd' else (0, 1)
        for mode in modes:
            saved = tmp / f'saved{mode}.zbd'
            probe_mode = 1 if brk == 1 and mode == 0 and zbd.parent.name == 'c1' and zbd.name == 'readerm1.zbd' \
                else mode
            if probe.probe_net_resave(str(zbd).encode(), str(saved).encode(), probe_mode) < 0:
                raise SystemExit(f'{zbd} mode {mode}: the probe refused it')
            got = extract(saved, tmp / f'saved{mode}.zip')
            if sorted(got) != sorted(orig):
                raise SystemExit(f'{zbd} mode {mode}: members differ')
            for m in orig:
                counts['members'] += mode == 0
                if mode == 0 or not is_net(m) or (mode == 2 and m != 'net_01.zrd.json'):
                    if got[m] != orig[m]:
                        raise SystemExit(f'{zbd} mode {mode}: {m} reads differently in mech3ax')
                    continue
                a, b = pairs(orig[m]), pairs(got[m])
                if mode == 1:
                    counts['nets'] += 1
                    if [k for k, _ in a] != [k for k, _ in b]:
                        raise SystemExit(f'{zbd} {m}: the keys differ')
                    for (k, va), (_, vb) in zip(a, b):
                        if isinstance(k, str) and k.startswith('node_'):
                            counts['nodes'] += 1
                            if pos_links(va) != pos_links(vb):
                                raise SystemExit(f'{zbd} {m} {k}: position or links differ\n  {va}\n  {vb}')
                    continue
                db = dict(b)
                for k, va in a:
                    if k in ('node_00', 'node_01'):
                        continue
                    if db.get(k) != va:
                        raise SystemExit(f'{zbd} {m} {k}: changed by the edit')
                n0, n1, n99 = db['node_00'], db['node_01'], db.get('node_99')
                f32 = lambda x: struct.unpack('<f', struct.pack('<f', x))[0]  # noqa: E731
                ok = (n0[1][0] == f32(1234.5678) and n0[1][2] == f32(-0.04999) and
                      n1[n1.index('speed') + 1] == [1, f32(0.3333333)] and n99 is not None and
                      n99[0] == [0, 1, 3] and n99[n99.index('valve') + 1] == [1, 'door_a', 7] and
                      n99[n99.index('valveunion') + 1] == [[1, 'u1', 2]] and n99[n99.index('auxname') + 1] == ['Added'])
                if not ok:
                    raise SystemExit(f'{zbd} {m}: the edits read differently in mech3ax\n  {n0}\n  {n1}\n  {n99}')
    print(f"ok: {counts['archives']} archives saved unedited read the same in mech3ax ({counts['members']} members); "
          f"{counts['nets']} nets re-encoded, {counts['nodes']} nodes' places and links the same; c1m1's edits as saved")


if __name__ == '__main__':
    main()
