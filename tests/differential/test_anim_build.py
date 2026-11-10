"""--build-anim (upgrades/mods/anim_defs, docs/modding.md, Animations a def at a time, Both ways) through the original
game's own reader: a mod's def files (from mech3ax's unzbd: one def replaced, its health changed, a scripted one where
the world has scripts, its script by file; and one added under a new anim name) built into each world's anim.zbd for
the original by `mw3 --build-anim`, then read and run by the original x86 (0x50a0e0, the post-load 0x50ae60 and
frames of random calls, test_anim_run's run_mission) against the port: identical, as on the shipped files.

    cmake --build build && .venv/bin/python -P tests/differential/test_anim_build.py [frames, 60] [worlds...]

Needs unzbd built in ../mech3ax (MECH3AX_UNZBD for another place). Breaks, by hand in anim_defs.cpp, each must
fail: the def count left as it was; a replaced def appended instead of put in place (the original's fixup then binds
its node run another way).
"""
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
import test_anim_run as R  # noqa: E402
from original import ROOT  # noqa: E402

UNZBD = Path(os.environ.get('MECH3AX_UNZBD') or
             (ROOT / 'game').resolve().parent.parent / 'mech3ax' / 'target' / 'release' / 'unzbd')
MW3 = ROOT / 'build' / 'mw3'
INSTALL = ROOT / 'game' / 'mw3-1.2'


def export(zbd, tmp):
    out = Path(tmp) / (zbd.parent.name + '.zip')
    subprocess.run([str(UNZBD), 'mw', 'anim', str(zbd), str(out)], check=True, capture_output=True)
    with zipfile.ZipFile(out) as z:
        return {n: z.read(n).decode('utf-8') for n in z.namelist()}


def main():
    args = sys.argv[1:]
    frames = int(args.pop(0)) if args and args[0].isdigit() else 60
    worlds = args or ['t1', 'c1']
    if not UNZBD.exists():
        print('skipped: no unzbd')
        return 0
    cmp = R.Compare()
    stats = {k: 0 for k in ('frames', 'start', 'start_at', 'start_node', 'reset', 'reset_node', 'reset_cockpit',
                            'stop', 'invalidate', 'ram', 'hit', 'player', 'zone', 'running_end')}
    with tempfile.TemporaryDirectory(prefix='anim-build-') as tmp:
        mod = Path(tmp) / 'mods' / 'both'
        (mod / 'zbd').mkdir(parents=True)
        (mod / 'mod.txt').write_text('name=Both ways\nanim=defs\n')
        shipped_counts = {}
        for w in worlds:
            files = export(INSTALL / 'zbd' / w / 'anim.zbd', tmp)
            meta = json.loads(files['metadata.json'])
            names, scripts = meta['anim_def_names'], meta['script_names']
            k = next((i for i, n in enumerate(names) if 'ObjectMotionSiScript' in files[n + '.json']), len(names) // 2)
            j = len(names) // 3
            folder = mod / 'zbd' / w / 'anim'
            folder.mkdir(parents=True)

            def by_file(d):  # an ObjectMotionSiScript's index as its script's own file
                if isinstance(d, dict):
                    if 'index' in d and 'name' in d and len(d) == 2 and scripts:
                        return {'name': d['name'], 'script': scripts[d['index']]}
                    return {key: by_file(v) for key, v in d.items()}
                if isinstance(d, list):
                    return [by_file(v) for v in d]
                return d
            rep = by_file(json.loads(files[names[k] + '.json']))
            rep['health'] = 123.5
            add = by_file(json.loads(files[names[j] + '.json']))
            add['anim_name'] = 'mod_added'
            (folder / (names[k] + '.json')).write_text(json.dumps(rep, indent=2))
            (folder / 'mod_added.json').write_text(json.dumps(add, indent=2))
            for s in scripts:
                if s + '.json' in files:
                    (folder / (s + '.json')).write_text(files[s + '.json'])
            shipped_counts[w] = len(names) + 1
        r = subprocess.run([str(MW3), '--build-anim', str(mod), '--install', str(INSTALL), '--write-dir', tmp,
                            '--hidden', '--nosound'], capture_output=True, text=True)
        print(r.stdout.strip())
        assert r.returncode == 0, r.stderr
        for i, w in enumerate(worlds):
            built = mod / 'original' / 'zbd' / w / 'anim.zbd'
            data = built.read_bytes()
            sources = struct.unpack_from('<I', data, 8)[0]
            count = struct.unpack_from('<h', data, 0x0c + sources * 0x54 + 0x0a)[0]
            assert count == shipped_counts[w] + 1, (w, count, shipped_counts[w])
            R.anim_of = lambda m, p=built: R.short_link(p, f'{m}-built.zbd')
            R.run_mission(w, frames, 2000 + i, cmp, stats)
            print(f'{w}: {count} defs (shipped {shipped_counts[w]}), the original reads and runs it as the port: ok '
                  f'({stats["frames"]} frames so far)', flush=True)
    print(f'ok: {len(worlds)} worlds built by --build-anim, read and run identically by the original and the port; '
          f'{cmp.words} words compared')
    return 0


if __name__ == '__main__':
    rc = main()
    sys.stdout.flush()
    os._exit(rc)
