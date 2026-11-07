#!/usr/bin/env python3
"""The first run's score: which of PM's AI functions the port entered, against the exe's trace.

    tools/pm-hits/score.py BINARY HITS SUMMARY.tsv [--run c1m1-idle] [--list]

BINARY is the MW3_PM_HITS=ON mw3pmtests, HITS what PM_AI_HITS=<file> wrote, SUMMARY.tsv the PC trace's
(pm_addr and a column per run, holding "mission" among its phases when it ran in a mission). A port function is known by the
`// Pirate's Moon 1.0 @ 0x...` comment over its definition in src/mech3/*_pm.cpp. Run from the repo's root.
"""
import argparse
import csv
import glob
import re
import subprocess

DEF = re.compile(r'^[\w:<>,*& ]*?([\w:~]+)\s*\(')


def port_names():
    """{qualified-ish name: pm address} from the comments over definitions."""
    out = {}
    for f in glob.glob('src/mech3/*_pm.cpp'):
        lines = open(f, errors='replace').read().split('\n')
        for i, l in enumerate(lines):
            m = re.search(r"Pirate's Moon 1\.0 @ (0x[0-9a-f]+)", l)
            if not m:
                continue
            for k in range(i + 1, min(i + 6, len(lines))):
                d = DEF.match(lines[k])
                if d and not lines[k].startswith('//'):
                    out.setdefault(d.group(1), int(m.group(1), 16))
                    break
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('binary')
    ap.add_argument('hits')
    ap.add_argument('summary')
    ap.add_argument('--run', default='c1m1-idle')
    ap.add_argument('--list', action='store_true')
    a = ap.parse_args()
    syms = {}
    for line in subprocess.run(['nm', '-C', '-n', a.binary], capture_output=True, text=True).stdout.split('\n'):
        p = line.split(None, 2)
        if len(p) == 3 and p[1] in ('t', 'T'):
            syms[int(p[0], 16) - 0x100000000] = p[2]
    hit_names = [syms.get(int(x, 16), '?') for x in open(a.hits).read().split()]
    names = port_names()
    hit_pm = set()
    for h in hit_names:
        q = h.split('(')[0]
        for n, addr in names.items():
            if q == n or q.endswith('::' + n):
                hit_pm.add(addr)
    rows = list(csv.DictReader(open(a.summary, encoding='utf-8-sig'), delimiter='\t'))
    ran = {int(r['pm_addr'], 16) for r in rows if 'mission' in (r.get(a.run) or '')}
    listed = {int(r['pm_addr'], 16) for r in rows}  # the trace only watched these
    have = set(names.values())
    both, missed = ran & hit_pm, (ran & have) - hit_pm
    extra, unwatched = (hit_pm & listed) - ran, hit_pm - listed
    print(f'{len(hit_names)} instrumented functions entered; {len(hit_pm)} named PM functions')
    print(f'the trace watched {len(listed)} and ran {len(ran)} in {a.run}; the port names {len(ran & have)} of those')
    print(f'  hit: {len(both)}   ported but never entered: {len(missed)}   entered, watched, never run there: {len(extra)}'
          f'   entered, not watched (no answer): {len(unwatched)}')
    if a.list:
        print('never entered:', ' '.join(hex(x) for x in sorted(missed)))
        print('entered, watched, never run there:', ' '.join(hex(x) for x in sorted(extra)))
        print('entered, not watched:', ' '.join(hex(x) for x in sorted(unwatched)))


if __name__ == '__main__':
    main()
