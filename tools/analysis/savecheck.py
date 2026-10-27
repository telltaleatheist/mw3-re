"""The saved campaign end to end in the program (docs/save.md section 3):
build/mw3 run four times on a scratch write dir with --nosound --hidden.

    .venv/bin/python tools/analysis/savecheck.py [--keep DIR]

1. `--briefing c1m1`, LAUNCH, `--win 60` (the mission won by its Exit
   Mission: the end screen, the fade, the debriefing's Win State, whose
   save writes Op 1 Mission 2's game), then the checkmark: straight on to
   c1m2's briefing (the reference: nothing read back).
2. The same win in a new dir, the app quit at the debriefing; a second run
   from the plain start: CAMPAIGN, the pilot, ACCEPT (the newest save,
   Op 1 Mission 2): its briefing must say "Saved Game Loaded" and hold
   what the straight-on briefing held (store and salvage by item, the
   mech and loadout, the lancemates).
3. From that save, Op 1 Mission 1 chosen on the pilot screen, won again:
   "Saved game already exists. Replace existing game?" answered No (the
   game's file untouched) and, in a copy, OK (rewritten). The rebuild's
   order (fixes.md: the world written after the answer); the original's,
   which writes it before, is test_pilot.py's and the campaign test's.

The game pixels of the clicks are the 640 x 480 screen's.
"""
import argparse
import hashlib
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MW3 = ROOT / 'build' / 'mw3'
PILOT_CLICKS = ['70,60@40', '150,135@80']   # CAMPAIGN, the pilot (the list's first row)
ACCEPT, LAUNCH, CHECKMARK = '470,320@{}', '556,435@{}', '595,445@{}'
OP1M1_ROW = '467,46@120'                    # the pilot screen's first saved game
OK, NO = '246,290@5600', '392,290@5600'     # the Replace box's buttons


def run(write_dir, args, frames):
    cmd = [str(MW3), '--nosound', '--hidden', '--verbose', '--write-dir', str(write_dir), '--frames', str(frames)]
    for a in args:
        cmd += a if isinstance(a, list) else ['--click-at', a]
    out = subprocess.run(cmd, capture_output=True, text=True, timeout=900).stdout
    lines = lambda key: [l[len(f'mw3: {key}: '):] for l in out.splitlines() if l.startswith(f'mw3: {key}: ')]
    return out, lines('at the briefing'), lines('saved at the win')


def holds(line):
    return re.sub(r'; anim log \d+$', '', line)


def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest() if p.exists() else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--keep', type=Path)
    a = ap.parse_args()
    with tempfile.TemporaryDirectory() as tmp:
        root = a.keep or Path(tmp)
        root.mkdir(parents=True, exist_ok=True)
        fails = checks(root)
    sys.exit(1 if fails else 0)


def checks(root):
    fails = []

    def check(ok, what):
        print(('ok   ' if ok else 'FAIL ') + what)
        if not ok:
            fails.append(what)

    win = [['--briefing', 'c1m1'], LAUNCH.format(1500), ['--win', '60']]
    # 1. Straight on.
    d1 = root / 'straight'
    shutil.rmtree(d1, ignore_errors=True)
    out1, briefs, saves = run(d1, win + [CHECKMARK.format(5000)], 9000)
    check(len(saves) == 1 and saves[0].startswith('Op 1 Mission 2'), f'the win saved Op 1 Mission 2: {saves}')
    check(len(briefs) == 2 and briefs[1].startswith('Op 1 Mission 2'), 'straight on to c1m2\'s briefing')
    straight = briefs[-1] if briefs else ''
    # 2. Quit at the debriefing, then the saved game from the pilot screen.
    d2 = root / 'saved'
    shutil.rmtree(d2, ignore_errors=True)
    run(d2, win, 6000)
    game = d2 / 'pilots' / 'games' / 'Briefing0102.mw3'
    check(game.exists() and game.stat().st_size > 1000, f'the saved game written ({game.name})')
    out, briefs, _ = run(d2, PILOT_CLICKS + [ACCEPT.format(100)], 2600)
    check('mw3: load: Saved Game Loaded' in out, '"Saved Game Loaded" at c1m2\'s load')
    check(len(briefs) == 1 and holds(briefs[0]) == holds(straight),
          f'the reloaded briefing holds what the straight-on one did\n     {briefs}\n     {straight}')
    check('Saved Game Loaded' not in out1, 'straight on reads nothing back')
    # 3. Op 1 Mission 1 won again: Replace existing game?
    before = digest(game)
    for name, button, same in (('no', NO, True), ('ok', OK, False)):
        d = root / f'replace_{name}'
        shutil.rmtree(d, ignore_errors=True)
        shutil.copytree(d2, d)
        run(d, PILOT_CLICKS + [OP1M1_ROW, ACCEPT.format(160), LAUNCH.format(1560), ['--win', '60'], button], 7600)
        after = digest(d / 'pilots' / 'games' / 'Briefing0102.mw3')
        check((after == before) == same, f'Replace existing game? {name.upper()}: the file {"kept" if same else "rewritten"}')
    print(f'{len(fails)} failed')
    return fails


if __name__ == '__main__':
    main()
