"""The vehicle editor's save on Pirate's Moon's own aiv.zrd files, the twin of test_aiv_save_mech3ax.py: in every PM
reader archive holding an aiv.zrd, every record written over itself through tests/probe_aiv_save.cpp (in pmprobe),
- as read: each must come back byte for byte, and mech3ax (its unzbd, as PM's) must read the saved archive exactly as
  the shipped one;
- with one group of its settings changed (several seeds): each must read back with the change (our reader, PM's
  compare 0x004f02b0 with its "target" block), and mech3ax must read the saved archive with every other member and
  every record's name the same.
PM's "target" tag (eight numbers and seven switches, read by 0x004f1120) is not a group the save writes: it is kept
as the file has it, an unknown tag. Every record holding one must hold it unchanged after every save, by mech3ax.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_aiv_save_mech3ax.py [seeds]

Needs unzbd built in ../mech3ax (MECH3AX_UNZBD for another place); without it the mech3ax half is skipped.
Breaks, by hand in aiv_save.cpp, each must fail: "target" taken as an old form (dropped); the target tag's value
written as the p13 group's.
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

os.environ['MW3_EXE'] = 'pm'
sys.path.insert(0, str(Path(__file__).parent))
from calltrace import load_probe  # noqa: E402
from original import ROOT  # noqa: E402

UNZBD = Path(os.environ.get('MECH3AX_UNZBD') or
             (ROOT / 'game').resolve().parent.parent / 'mech3ax' / 'target' / 'release' / 'unzbd')
probe = load_probe()
probe.probe_aiv_save.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint32, ctypes.c_int,
                                 ctypes.POINTER(ctypes.c_int32), ctypes.c_char_p, ctypes.c_int]


def extract(zbd, out):
    subprocess.run([str(UNZBD), 'pm', 'reader', str(zbd), str(out)], check=True, capture_output=True)
    with zipfile.ZipFile(out) as z:
        return {n: json.loads(z.read(n)) for n in z.namelist()}


def records(aiv):
    """The file's records, a name and its list, in order; loose words between them (PM's readermp files) skipped,
    as the game's lookup by name does."""
    out, i = [], 0
    while i < len(aiv):
        if isinstance(aiv[i], str) and i + 1 < len(aiv) and isinstance(aiv[i + 1], list):
            out.append((aiv[i], aiv[i + 1]))
            i += 2
        else:
            i += 1
    return out


def targets(aiv):
    """Each record's "target" value, by the record's name (a record lists its tags as key, value)."""
    out = {}
    for name, rec in records(aiv):
        for k in range(len(rec) - 1):
            if rec[k] == 'target':
                out[name] = rec[k + 1]
    return out


def main():
    seeds = int(sys.argv[1]) if len(sys.argv) > 1 else 6
    mx = UNZBD.exists()
    tmp = Path(tempfile.mkdtemp(prefix='pm_aiv_save_mx_', dir=ROOT / 'build'))
    records = archives = changed_ok = with_target = 0
    try:
        for zbd in sorted((ROOT / 'game' / 'pm' / 'zbd').rglob('reader*.zbd')):
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
                if [x for x in a if not isinstance(x, list)] != [x for x in b if not isinstance(x, list)]:
                    raise SystemExit(f'{zbd} seed {seed}: the records\' names (or the loose words) differ in mech3ax')
                ta, tb = targets(a), targets(b)
                if ta != tb:
                    first = next(k for k in sorted(set(ta) | set(tb)) if ta.get(k) != tb.get(k))
                    raise SystemExit(f'{zbd} seed {seed}: {first}\'s target tag not kept '
                                     f'({ta.get(first)} -> {tb.get(first)})')
                if not changed:
                    with_target += len(ta)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    print(f'ok: {records} records in {archives} archives kept byte for byte, {changed_ok} changed ones read back '
          f'as changed over {seeds} seeds' +
          (f'; mech3ax reads every save, {with_target} records\' target tags kept through each' if mx
           else '; mech3ax skipped (no unzbd)'))
    if mx and not with_target:
        raise SystemExit('no record with a target tag was seen')
    return 0


if __name__ == '__main__':
    rc = main()
    sys.stdout.flush()
    os._exit(rc)
