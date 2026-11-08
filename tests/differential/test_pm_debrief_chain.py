"""Pirate's Moon's debriefing and campaign reached as a player reaches them:
`mw3pm` itself through three chains, every line of the results read back.

`--action-chain c1`: the first world's five cases, two rounds each. Their titles
(0xdc0 is "Instant Action %1", the name only: MW3's number and name was a crash),
the ranks (0x47e5da a fifth of the waves in the wave modes, else hidden) and one
round's lines with its wave points.
`--campaign-chain new:10`: the first campaign's ten missions in PM's order (the second, the exe's standalone, is not ported) (0x435de0: 1, 2, 5 c1;
3, 4, 8 c2; 6, 7 c3; 9, 10 c4), the titles ("Mission %1!d!", 0x21d), mission 10
ending it (0x435db0) in the Win Movie State, then the credits.
`--campaign-chain kills:1`: mission 1 won by shooting: the objectives' rows, the
bonuses, the two totals and the rank by the score (0x47eb82).

The second campaign's title, press and rank are test_pm_standalone_chain's. The layout's positions are not read here; `test_debriefing` stays to
move, it needs the twod bench on PM's exe (campaign.md).
"""
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import pm_floors  # noqa: E402

KINDS = ['Battle', 'Battle Waves', 'Battle Progression', 'Attack', 'Defend']
WORLD = {1: 1, 2: 1, 3: 2, 4: 2, 5: 1, 6: 3, 7: 3, 8: 2, 9: 4, 10: 4}
RANK = {'Battle': -1, 'Battle Waves': 0, 'Battle Progression': 0, 'Attack': -1, 'Defend': -1}
BATTLE_LOST = ['Primary Objectives', 'Mission Total', '0', "'Mechs Defeated", 'Actions', '0']
# The round's two lancemates are a random draw of the three (their AI params' pilot, by rand): which two moves with
# the seed under either AI (seed 2: Epona and Alan, then Dominic and Alan with MW3's AI, Epona and Alan with PM's).
# PM's AI, the default since e40aa9b5, uses rand otherwise in the first round, so the second draws Epona and Alan;
# PM_AI=0 still draws Dominic and Epona. Re-taken at the flip, nobody dropped.
PROGRESSION_WON = ['Primary Objectives', "'Mechs Defeated", '2500', 'Mission Total', '4800', "'Mechs Defeated",
                   'Actions', '5', 'Epona Rhi', '0', 'Alan Mattila', '0', 'Mission Total', '4800']
if os.environ.get('PM_AI') == '0':
    PROGRESSION_WON[8:12] = ['Dominic Paine', '0', 'Epona Rhi', '0']
MISSION_1_WON = ['Primary Objectives', 'Protect FedSuns Base', '2500', 'Secondary Objectives',
                 'Protect Rim Collective Camp', '2500', "'Mechs Defeated", '1700', 'Turrets Destroyed', '1200',
                 'Campaign Total', '7900', 'Mission Total', '7900', "'Mechs Defeated", 'Chain', '4', 'Epona Rhi', '0',
                 'Mission Total', '7900']  # the 4 allied turf_ turrets no longer killed and scored


def run(exe, write_dir, *args):
    env = dict(os.environ, SDL_AUDIODRIVER='dummy')
    r = subprocess.run([str(exe), *args, '--hidden', '--nosound', '--write-dir', str(write_dir),
                        '--frame-ms', '33', '--seed', '1'],
                       cwd=ROOT, env=env, capture_output=True, text=True, errors='replace', timeout=240)
    return r.returncode, r.stdout


def lit(path):
    """The share of a 32-bit BMP's middle (no HUD) that isn't black, in a mission's shot."""
    d = Path(path).read_bytes()
    off, w, h = struct.unpack_from('<I', d, 10)[0], *struct.unpack_from('<ii', d, 18)
    bh, n, on = abs(h), 0, 0
    for y in range(bh * 25 // 100, bh * 70 // 100, 4):
        at = off + y * w * 4
        for x in range(w * 20 // 100, w * 80 // 100, 4):
            p = at + x * 4
            n += 1
            on += d[p] + d[p + 1] + d[p + 2] > 9
    return on / n


def check(name, got, want):
    if got == want:
        return 0
    print(f'{name}:\n  got  {got}\n  want {want}')
    return 1


def main():
    bad = 0
    tmp = Path(tempfile.mkdtemp(prefix='pm_debrief_', dir=ROOT / 'build'))
    try:
        exe = tmp / 'mw3pm_debrief'  # a fresh name: the same path can be refused
        shutil.copy2(ROOT / 'build' / 'mw3pm', exe)

        code, out = run(exe, tmp / 'action', '--action-chain', 'c1')
        titles = re.findall(r'^mw3: modes c1 (.+) debriefing title: (.*); rank (-?\d+)$', out, re.M)
        done = re.search(r'modes done: (\d+) cases, (\d+) tallied as expected, (\d+) not', out)
        bad += check('action chain', (code, done and done.groups()), (0, ('5', '5', '0')))
        bad += check('action titles', titles,
                     [(k, f'Instant Action {k}', str(RANK[k])) for k in KINDS for _ in range(2)])
        lost = re.split(r'^mw3: modes c1 Battle debriefing title:.*$', out, flags=re.M)
        lines = re.findall(r'^mw3: modes c1 Battle debriefing line: (.*)$', lost[1] if len(lost) > 1 else '', re.M)
        bad += check('lost, nothing scored', lines, BATTLE_LOST)  # no heading above: its total is 0
        rounds = re.split(r'^mw3: modes c1 Battle Progression debriefing title:.*$', out, flags=re.M)
        lines = re.findall(r'^mw3: modes c1 Battle Progression debriefing line: (.*)$', rounds[2] if len(rounds) > 2 else '',
                           re.M)
        bad += check('won by waves', lines, PROGRESSION_WON)

        code, out = run(exe, tmp / 'campaign', '--campaign-chain', 'new:10')
        launched = [(int(a), int(b)) for a, b in re.findall(r'^mw3: chain LAUNCH op (\d+) mission (\d+)$', out, re.M)]
        seen = re.findall(r"^mw3: chain debriefing title: (.*); the game's name: (.*); rank (-?\d+)$", out, re.M)
        bad += check('campaign chain', code, 0)
        # Through the lab (its purge freed the world's materials), the world is there, by the picture.
        os.environ['MW3_CHAIN_WORLD'] = '1'
        code, world = run(exe, tmp / 'world', '--campaign-chain', 'new:3')
        del os.environ['MW3_CHAIN_WORLD']
        shots = sorted((tmp / 'world').glob('chain_*_world.bmp'))
        shares = [lit(p) for p in shots]
        print('the world after the lab, share not black:', ' '.join(f'{x:.2f}' for x in shares))
        bad += check('lab before each', len(re.findall(r'^mw3: chain MECHLAB$', world, re.M)) >= 3, True)
        bad += check('world shots', (code, len(shots)), (0, 3))
        bad += check('worlds drawn', [x > 0.3 for x in shares], [True] * 3)
        # The prep screen's map: briefing.zrd's brief_c1m1, not the default section's nothing.
        bad += check('briefing maps', sorted(set(re.findall(r'^mw3: chain briefing map width (\d+)$', out, re.M))), ['640'])
        bad += check('the worlds', launched[:10], [(WORLD[m], m) for m in range(1, 11)])
        # The name is read once the results moved the game on; the last has no next.
        bad += check('campaign titles', seen, [(f'Mission {m}', f'Mission {min(m + 1, 10)}', '2') for m in range(1, 11)])
        end = re.search(r'^mw3: chain state Win Movie State', out, re.M)
        bad += check('mission 10 ends it', bool(end) and 'chain credits after 10 wins' in out, True)

        code, out = run(exe, tmp / 'kills', '--campaign-chain', 'kills:1')
        lines = re.findall(r'^mw3: chain debriefing line: (.*)$', out, re.M)
        bad += check('kills chain', code, 0)
        bad += check('won by kills', lines, MISSION_1_WON)
        pm_floors.floors(rounds=(len(titles), 10), missions=(len(seen), 10), lines=(len(lines), 21))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    print(f'{len(titles)} instant action rounds, {len(seen)} campaign missions, {len(lines)} lines read: {bad} bad')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
