"""Add vehicle's aiv.zrd record (upgrades/editor/add_vehicle, docs/editor.md, Add veh to node) read by mech3ax
(TerranMechworks/mech3ax, its unzbd): in every MW3 reader archive holding an aiv.zrd, for each vehicle type its
records have, a record added as Add vehicle adds it (the type's first record's tags, our net, place, yaw and
team, the next free name) through tests/probe_aiv_add.cpp, the archive saved and extracted beside the shipped
one. The new record must read as (net (x y z) yaw team tags...) with the tags exactly its template's, after
every shipped record, and every other member must read the same.

    cmake --build build --target mw3probe && .venv/bin/python -P tests/differential/test_aiv_add_mech3ax.py

Needs unzbd built in ../mech3ax (MECH3AX_UNZBD for another place); skipped without it. BREAK=1 gives the probe a
team of 7 and expects 2: it must fail.
"""
import ctypes
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import load_probe  # noqa: E402
from original import ROOT  # noqa: E402

UNZBD = Path(os.environ.get('MECH3AX_UNZBD') or
             (ROOT / 'game').resolve().parent.parent / 'mech3ax' / 'target' / 'release' / 'unzbd')
probe = load_probe()
probe.probe_aiv_add.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_float,
                                ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_int, ctypes.c_char_p,
                                ctypes.c_int]


def extract(zbd, out):
    subprocess.run([str(UNZBD), 'mw', 'reader', str(zbd), str(out)], check=True, capture_output=True)
    with zipfile.ZipFile(out) as z:
        return {n: json.loads(z.read(n)) for n in z.namelist()}


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


def main():
    if not UNZBD.exists():
        print(f'skipped: no {UNZBD}')
        return 0
    brk = int(os.environ.get('BREAK', '0'))
    tmp = Path(tempfile.mkdtemp(prefix='aiv_add_mx_', dir=ROOT / 'build'))
    added = archives = 0
    try:
        for zbd in sorted((ROOT / 'game' / 'mw3-1.2' / 'zbd').rglob('reader*.zbd')):
            orig = extract(zbd, tmp / 'orig.zip')
            if 'aiv.zrd.json' not in orig:
                continue
            archives += 1
            kv = orig['aiv.zrd.json'][0]
            types = []
            for i in range(0, len(kv) - 1, 2):
                t = kv[i].split('_')[0]
                if '_' in kv[i] and t not in types:
                    types.append(t)
            for n, t in enumerate(types):
                pos = (f32(100.5 + n), f32(-30.25), f32(-1200.125 - n))
                yaw, net, team = f32(-20.0 + n), 7 + n, 2
                name = ctypes.create_string_buffer(64)
                saved = tmp / 'saved.zbd'
                items = probe.probe_aiv_add(str(zbd).encode(), str(saved).encode(), t.encode(), net, *pos, yaw,
                                            7 if brk == 1 else team, name, 64)
                if items < 0:
                    raise SystemExit(f'{zbd.name} {t}: the probe refused it ({items})')
                got = extract(saved, tmp / 'saved.zip')
                if sorted(got) != sorted(orig):
                    raise SystemExit(f'{zbd} {t}: members differ')
                for m in orig:
                    if m != 'aiv.zrd.json' and got[m] != orig[m]:
                        raise SystemExit(f'{zbd} {t}: {m} reads differently in mech3ax')
                a, b = orig['aiv.zrd.json'][0], got['aiv.zrd.json'][0]
                if b[:len(a)] != a or len(b) != len(a) + 2:
                    raise SystemExit(f'{zbd} {t}: the shipped records are not kept as they were')
                key, rec = b[-2], b[-1]
                template = next(a[i + 1] for i in range(0, len(a) - 1, 2) if a[i].split('_')[0] == t and '_' in a[i])
                want = [net, list(pos), yaw, team] + template[4:]
                if key != name.value.decode() or rec != want:
                    raise SystemExit(f'{zbd} {t}: added {key} reads as {rec[:5]}..., wanted {want[:5]}...')
                if len(template) - 4 != items:
                    raise SystemExit(f'{zbd} {t}: {items} tag items, the template has {len(template) - 4}')
                added += 1
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    print(f'ok: {added} records added in {archives} archives read in mech3ax as written, the rest unchanged')
    return 0


if __name__ == '__main__':
    rc = main()
    sys.stdout.flush()
    os._exit(rc)
