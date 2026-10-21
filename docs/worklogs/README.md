# Work logs and the specialists' protocol

The lead session ("mw3 mac 1") hands out one small item at a time to the
specialist sessions over SendMessage, merges their branches and runs the
reviews. Each specialist keeps its log here, so it can pick up after a
compaction by reading it.

| Session | Area | Log |
|---|---|---|
| mw3 mac 2 | Game systems: mechs, movement, weapons, effects, the world's queries | `game.md` |
| mw3 mac 3 | Front end and presentation: fades, the HUD, screens, render glue | `frontend.md` |
| mw3 mac 4 | The AI: the brain, orders, behaviours, AI firing, lancemates | `ai.md` |

## Rules for a specialist

- Read `CLAUDE.md` first; it holds the project's conventions.
- Run on Opus 5.5 (`claude-opus-5-5`). Porting and analysis need it.
- Start no agents or subagents. The lead runs the reviews.
- Work in your own worktree, never in the main checkout. Set it up once:
  `git worktree add .claude/worktrees/<session> -b <session>/<item>
  main`, with symlinks to the main checkout's `game/`, `analysis-output/`
  and `.venv/`, and your own `build/`. Then, for each item, run
  `git checkout -b <session>/<item> main` in that worktree.
- Keep scratch files in the worktree's `scratch/` folder, which is not
  tracked.
- Keep your context small:
  - Grep `analysis-output/decomp` and `mech3.asm` instead of reading whole
    files, and keep tool output short.
  - While iterating, run only the tests you touch.
  - Before reporting, run the related set, plus `mw3tests` and
    `mw3sdltests`.
  - End emulator tests with `sys.stdout.flush(); os._exit(code)`: Unicorn
    processes can hang at exit on this Mac, in a state nothing can kill.
    Run long tests in the background with their output going to a file in
    `scratch/`, then read the file, so a hang never blocks the session.
- Never kill processes by name (`pkill -f mw3tests` and the like): the
  other sessions run the same programs. Kill only a pid you started.
- A `CHANGES.md` line goes under its heading (bug fixes that change play,
  crashes, protection, modern machines), never at the end of the file: the
  last section lists what the rebuild keeps on purpose.
- Every item comes with a differential test with break tests, its docs,
  `fixes.md` and `CHANGES.md` lines for any change from the original, and
  player-facing finds in `discoveries.md`.
- Commit messages carry no `Co-Authored-By` or `Claude-Session` lines, and
  the author email is the noreply address set in the repo's git config.
- Commit on your branch and never push or merge to main. Report to "mw3 mac
  1" with SendMessage: the commits, what is ported (by address), the test
  results, the findings, the fixes and what is left. Keep the report short.
- Owen compacts each specialist after every major feature (`/compact`), so
  keep the log current enough to pick up from it alone.
- After reporting, append a few lines to your log (the item, its commits,
  and what you would need to remember), then wait for the next item.
