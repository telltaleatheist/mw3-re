"""The vehicle editor's save (upgrades/editor/aiv_save, docs/editor.md, The vehicle editor's save): in every MW3
reader archive holding an aiv.zrd, every record written over itself through tests/probe_aiv_save.cpp,
- as read: each must come back byte for byte, and mech3ax (TerranMechworks/mech3ax, its unzbd) must read the
  saved archive exactly as the shipped one;
- with one group of its settings changed (a word, a flag, the chassis, the pilot, the voice, an order, mech3;
  several seeds): each must read back with the change (our reader; the yaw within 1e-5), and mech3ax must read
  the saved archive with every other member and every record's name the same.

    cmake --build build --target mw3probe && .venv/bin/python -P tests/differential/test_aiv_save_mech3ax.py [seeds]

Needs unzbd built in ../mech3ax (MECH3AX_UNZBD for another place); without it the mech3ax half is skipped.
Breaks, by hand in aiv_save.cpp, each must fail: p8's 5th item from +0x74; the vlist's region and valve swapped;
every group kept even when changed.
"""
import ctypes
import json
import os
import shutil
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
probe.probe_aiv_save.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint32, ctypes.c_int,
                                 ctypes.POINTER(ctypes.c_int32), ctypes.c_char_p, ctypes.c_int]


def extract(zbd, out):
    subprocess.run([str(UNZBD), 'mw', 'reader', str(zbd), str(out)], check=True, capture_output=True)
    with zipfile.ZipFile(out) as z:
        return {n: json.loads(z.read(n)) for n in z.namelist()}


def main():
    seeds = int(sys.argv[1]) if len(sys.argv) > 1 else 6
    mx = UNZBD.exists()
    tmp = Path(tempfile.mkdtemp(prefix='aiv_save_mx_', dir=ROOT / 'build'))
    records = archives = changed_ok = 0
    try:
        for zbd in sorted((ROOT / 'game' / 'mw3-1.2' / 'zbd').rglob('reader*.zbd')):
            for seed in range(seeds + 1):
                changed = seed > 0
                stats = (ctypes.c_int32 * 5)()
                bad = ctypes.create_string_buffer(128)
                saved = tmp / 'saved.zbd'
                r = probe.probe_aiv_save(str(zbd).encode(), str(saved).encode(), seed, int(changed), stats, bad, 128)
                if r == -2:
                    break  # no aiv.zrd
                if r < 0:
                    raise SystemExit(f'{zbd}: the probe refused it ({r})')
                n, kept, same, differ, refused = list(stats)
                if refused or differ:
                    raise SystemExit(f'{zbd} seed {seed}: {refused} refused, {differ} read back otherwise '
                                     f'(first {bad.value.decode()})')
                if not changed:
                    archives += 1
                    records += n
                    if kept != n:
                        raise SystemExit(f'{zbd}: {n - kept} of {n} records not kept byte for byte')
                else:
                    changed_ok += same
                if not mx:
                    continue
                orig = extract(zbd, tmp / 'orig.zip')
                got = extract(saved, tmp / 'saved.zip')
                if sorted(got) != sorted(orig):
                    raise SystemExit(f'{zbd} seed {seed}: members differ')
                for m in orig:
                    if m != 'aiv.zrd.json' and got[m] != orig[m]:
                        raise SystemExit(f'{zbd} seed {seed}: {m} reads differently in mech3ax')
                a, b = orig['aiv.zrd.json'][0], got['aiv.zrd.json'][0]
                if not changed and a != b:
                    raise SystemExit(f'{zbd}: aiv.zrd reads differently in mech3ax with nothing changed')
                if [a[i] for i in range(0, len(a), 2)] != [b[i] for i in range(0, len(b), 2)]:
                    raise SystemExit(f'{zbd} seed {seed}: the records\' names differ in mech3ax')
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    print(f'ok: {records} records in {archives} archives kept byte for byte, {changed_ok} changed ones read back '
          f'as changed over {seeds} seeds' + ('; mech3ax reads every save' if mx else '; mech3ax skipped (no unzbd)'))
    return 0


if __name__ == '__main__':
    rc = main()
    sys.stdout.flush()
    os._exit(rc)
