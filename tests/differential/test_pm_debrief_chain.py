"""Pirate's Moon's debriefing reached as a player reaches it: `mw3pm` itself,
through `--action-chain c1` (every round of the first world's five cases) and
`--campaign-chain` (the first missions). Its message table changed under the
titles: 0xdc0 is "Instant Action %1" (the name only; MW3's took a number and
the name, whose number was read as the pointer: a crash), 0x21d is "Mission
%1!d!" (MW3's took the operation first). The exe: 0x47dd4e, 0x47dec6, 0x435640.

The results' body stays MW3's until 0x47d890 is ported whole; type 5's title
is not reached here, the standalone game object being the front end's.
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import pm_floors  # noqa: E402

KINDS = ['Battle', 'Battle Waves', 'Battle Progression', 'Attack', 'Defend']


def run(exe, write_dir, *args):
    env = dict(os.environ, SDL_AUDIODRIVER='dummy')
    r = subprocess.run([str(exe), *args, '--hidden', '--nosound', '--write-dir', str(write_dir),
                        '--frame-ms', '33', '--seed', '1'],
                       cwd=ROOT, env=env, capture_output=True, text=True, errors='replace', timeout=240)
    return r.returncode, r.stdout


def main():
    bad = 0
    tmp = Path(tempfile.mkdtemp(prefix='pm_debrief_', dir=ROOT / 'build'))
    try:
        exe = tmp / 'mw3pm_debrief'  # a fresh name: the same path can be refused
        shutil.copy2(ROOT / 'build' / 'mw3pm', exe)

        code, out = run(exe, tmp / 'action', '--action-chain', 'c1')
        titles = re.findall(r'^mw3: modes (.+) debriefing title: (.*)$', out, re.M)
        done = re.search(r'modes done: (\d+) cases, (\d+) tallied as expected, (\d+) not', out)
        if code != 0 or not done or done.group(1) != '5' or done.group(2) != '5' or done.group(3) != '0':
            print('action chain: exit', code, 'done:', done and done.group(0))
            print('\n'.join(out.splitlines()[-6:]))
            bad += 1
        want = [(f'c1 {k}', f'Instant Action {k}') for k in KINDS for _ in range(2)]
        if titles != want:
            print('action titles', titles)
            bad += 1

        code, out = run(exe, tmp / 'campaign', '--campaign-chain', '1')
        seen = re.findall(r'^mw3: chain debriefing title: (.*); the game\'s name: (.*)$', out, re.M)
        want = [('Mission 1', 'Mission 2'), ('Mission 2', 'Mission 3')]  # the name asked after the result moved on
        if code != 0 or seen != want:
            print('campaign chain: exit', code, seen)
            bad += 1
        pm_floors.floors(rounds=(len(titles), 10), campaign=(len(seen), 2))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    print(f'{len(titles)} instant action and {len(seen)} campaign debriefings: {bad} bad')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
