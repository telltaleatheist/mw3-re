"""PM's second campaign as a player reaches it: `mw3pm` itself, from the main menu's STANDALONE through the real states.
Eleven missions in the exe's order, each won into the next; the file saved, and read back partway by a new run; a mission
already reached replayed, the best kept; the last win going to the main menu with no credits; titles, ranks and CONTINUE's
help read back from the screens.

What this does NOT show. The wins are Exit Mission with the objectives forced (as `--campaign-chain new:N`), or one mission
by shooting everything, and PM's AI is not running: these missions depend on it, so a chain win does not prove a pirate
mission plays right, and nothing here calls one played. What the exe does at each step rests on reading it and on
test_pm_standalone's comparison of the game object: the screens themselves are not run in the exe.

    cmake --build build --target mw3pm && .venv/bin/python -P tests/differential/test_pm_standalone_chain.py
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

ORDER = ['c1sa1', 'c2sa1', 'c1sa2', 'c1sa3', 'c3sa1', 'c3sa2', 'c3sa3', 'c2sa2', 'c4sa1', 'c4sa2', 'c1sa4']
NAME = [f'Mission {i}' for i in range(1, 11)] + ['Bonus Mission']


def rank(row):  # 0x47ebxx: by the mission's index when the debriefing is made
    return 1 if 2 <= row <= 4 else 4 if 5 <= row <= 10 else 0


def run(exe, write_dir, chain):
    env = dict(os.environ, SDL_AUDIODRIVER='dummy')
    r = subprocess.run([str(exe), '--campaign-chain', chain, '--hidden', '--nosound', '--write-dir', str(write_dir),
                        '--frame-ms', '33', '--seed', '1'], cwd=ROOT, env=env, capture_output=True, text=True, errors='replace',
                       timeout=280)
    return r.returncode, r.stdout


def check(name, got, want):
    if got == want:
        return 0
    print(f'{name}:\n  got  {got}\n  want {want}')
    return 1


def titles(out):
    return re.findall(r"^mw3: chain debriefing title: (.*); the game's name: (.*); rank (-?\d+)$", out, re.M)


def helps(out):
    return re.findall(r'^mw3: chain standalone debrief: rank (-?\d+), help 0, CONTINUE help "(.*)", won flag (\d), at (\d+), best (\d+)$',
                      out, re.M)


def launched(out):
    return re.findall(r'^mw3: chain LAUNCH row (\d+) (c\dsa\d)$', out, re.M)


def main():
    bad = 0
    tmp = Path(tempfile.mkdtemp(prefix='pm_sa_', dir=ROOT / 'build'))
    try:
        exe = tmp / 'mw3pm_sa'  # a fresh name: the same path can be refused
        shutil.copy2(ROOT / 'build' / 'mw3pm', exe)
        # All eleven at once.
        whole = tmp / 'whole'
        code, out = run(exe, whole, 'standalone:99')
        bad += check('the chain', code, 0)
        bad += check('the order', launched(out), [(str(i), ORDER[i]) for i in range(11)])
        bad += check('titles', titles(out), [(NAME[i], NAME[min(i + 1, 10)], str(rank(i))) for i in range(11)])
        want = [(str(rank(i)), 'Next Mission', '0', str(i + 1), str(i + 1)) for i in range(10)] + [('4', 'Finish Game', '1', '10', '10')]
        bad += check('helps, at, best', helps(out), want)
        bad += check('the last win', 'chain main menu after 11 wins without the credits (credits entered 0)' in out, True)
        bad += check('no credits state', 'chain state Credits' in out, False)
        bad += check('nothing refused at the briefing', 'chain stuck' in out, False)
        bad += check('briefing maps', sorted(set(re.findall(r'^mw3: chain briefing map width (\d+)$', out, re.M))), ['640'])
        bad += check('its buttons', set(re.findall(r'chain buttons: (.*)$', out, re.M)), {'lab 1 salvage 0 replay 1 abort 1'})
        # The lancemates' mechs: PM's 25, not clamped to MW3's 19 (0x4a19c0).
        mates = [int(m) for line in re.findall(r'slots \[ (.*?) \]', out) for m in re.findall(r'(-?\d+)\+cfg', line)[1:]]
        bad += check('a lancemate past MW3\'s roster', max(mates) > 18, True)
        whole_titles = len(titles(out))

        # Saved and read back partway, a reached mission replayed, the rest played from the file.
        d = tmp / 'part'
        code, out = run(exe, d, 'standalone:4')
        bad += check('four wins', (code, len(titles(out))), (0, 4))
        code, out = run(exe, d, 'standalone-replay:1')
        rows = re.findall(r'^mw3: chain mission list (\d+): (.*)$', out, re.M)
        bad += check('read back: the list', rows, [(str(i), NAME[i]) for i in range(5)])
        bad += check('replay: row 1', launched(out)[:1], [('1', ORDER[1])])
        bad += check('replay: the next, best kept', helps(out)[:1], [(str(rank(1)), 'Next Mission', '0', '2', '4')])
        code, out = run(exe, d, 'standalone-resume:99')
        rows = re.findall(r'^mw3: chain mission list (\d+): (.*)$', out, re.M)
        bad += check('read back again: still five', len(rows), 5)
        bad += check('resumed at row 2', launched(out), [(str(i), ORDER[i]) for i in range(2, 11)])
        bad += check('resumed to the end', 'chain main menu after 9 wins without the credits (credits entered 0)' in out, True)

        # One by shooting.
        code, out = run(exe, tmp / 'kills', 'standalone-kills:1')
        lines = re.findall(r'^mw3: chain debriefing line: (.*)$', out, re.M)
        bad += check('kills chain', code, 0)
        bad += check('won by kills: a total', 'Mission Total' in lines, True)
        won = re.search(r'^mw3: chain won row 0 c1sa1 by (\d+) kills', out, re.M)
        print('c1sa1 by shooting:', won.group(0)[5:] if won else 'no win line', '|', lines[:8])
        pm_floors.floors(missions=(whole_titles, 11), resumed=(len(launched(out)), 1), lines=(len(lines), 4))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    print(f'{whole_titles} missions chained, read back and replayed: {bad} differ. PM\'s AI is not running: nothing here is played.')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
