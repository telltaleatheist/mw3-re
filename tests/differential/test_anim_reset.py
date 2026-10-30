"""zEffect's animations reset, 0x50ad60 (zeff::anim_reset_all), against the
original x86 in Unicorn, on the missions' real animation state.

    cmake --build build && .venv/bin/python -P tests/differential/test_anim_reset.py [frames, 30] [after, 10] [missions...]

The debriefing's retry of a failed campaign mission and Instant Action's
next round call it with ECX 1 (0x47d1eb, 0x47d271): every def and instance
stopped (0x50ace0 with 1), the executed and invalid ones restarted
(0x508460), then each not running reset on its 0x20 flag (0x508310) and
started when it starts with the mission (+0x99 4, 0x508ae0). For each
mission both sides read the world and run test_anim_run's frames of random
calls (starts, resets, stops, hits, ...), then the reset with ECX 0 or 1
(one world each); the state is compared, then more random frames show the
reset world plays on alike. Until 2026-10-30 the port left the call to the
census, and a retried mission kept its first try's buildings
(campaign.md section 14, The retry).
"""
import ctypes
import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_anim_run import Compare, prepare, play, probe  # noqa: E402
from test_gamez import WORLDS  # noqa: E402

probe.ar_reset_all.argtypes = [ctypes.c_int]
RESET_ALL = 0x50ad60


def run_mission(mission, frames, after, seed, cmp, stats):
    for restart in (0, 1):
        emu, port, rng, nrng = prepare(mission, seed ^ restart, cmp, stats)
        play(emu, port, mission, frames, rng, nrng, cmp, stats)
        emu.guard(lambda: emu.cpu.call(RESET_ALL, ecx=restart, count=2_000_000_000))
        emu.build_ids()
        probe.ar_reset_all(restart)
        cmp(f'{mission} reset ({restart})', emu.dump(True), port.dump(True))
        stats['resets'] += 1
        play(emu, port, mission, after, rng, nrng, cmp, stats, f'after the reset ({restart})')
        del emu


def main():
    args = sys.argv[1:]
    # (30 and 10 keep the six worlds within the related set's 15 minutes)
    frames = int(args.pop(0)) if args and args[0].isdigit() else 30
    after = int(args.pop(0)) if args and args[0].isdigit() else 10
    missions = args or WORLDS
    cmp = Compare()
    stats = {k: 0 for k in ('frames', 'start', 'start_at', 'start_node', 'reset', 'reset_node', 'reset_cockpit',
                            'stop', 'invalidate', 'ram', 'hit', 'player', 'zone', 'running_end', 'resets')}
    for i, m in enumerate(missions):
        run_mission(m, frames, after, 2000 + i, cmp, stats)
        print(f'{m}: ok ({stats["frames"]} frames so far)', flush=True)
    print(f'ok: {len(missions)} missions, {stats["resets"]} resets identical with {stats["frames"]} frames around them; '
          f'{cmp.words} words compared, {cmp.approx} within tolerance (worst {cmp.worst:.2e})')


if __name__ == '__main__':
    main()
