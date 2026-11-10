# Mods by campaign: the plan (Owen, 2026-11-10)

Owen's model for how players use mods, replacing "mods turned on apply to the
whole game". Both games; Pirate's Moon has two official campaigns (Campaign and
Pirate Missions), MechWarrior 3 one.

## The rule

- **The main menu is the original.** Its CAMPAIGN (and Pirate's Moon's Pirate
  Missions) always plays the game's own files: no button anywhere applies a mod
  to it. Nobody can lose their campaign or need to restore game files.
- **Game mods are played through MODS.** Game mods are worlds and campaigns
  (built in zStudio and imported whole) and assets (new or changed mechs,
  weapons and their values).
- **Instant Action takes mods, from its own screen** (Owen, 2026-11-10,
  replacing "an entry in the CAMPAIGNS list"): see Instant Action below.
- **Multiplayer takes no mods for now**; perhaps later, if players ask.
- **Translations and purely visual mods are not managed.** Players do them the
  old way, with an installer that replaces game files; nothing stops that, and
  modders may still edit any file directly. The MODS screen is for game mods.

## The MODS screen

- **CAMPAIGNS (left):** the official campaign(s), there from the first start;
  every campaign imported from a mod; every copy the player saved.
- **The chosen campaign's mods (right):** every imported asset mod, with a
  checkbox. Each campaign keeps its own ticks: Freako's weapons can be on for
  one campaign and off for another. A campaign that ships its own assets, or
  names them with `needs=`, shows those ticked and locked. Where two ticked
  mods change the same thing, the list shows which one wins, and the player
  can reorder them (as the MODS list does today).
- **PLAY:** the chosen campaign with its ticked mods.
- **EDIT:** the chosen campaign in the Battlesport editor. Saving is always
  **Save as**: a new campaign entry, a copy that holds only the files changed,
  on top of the campaign it came from. The official entry is never written; a
  copy can be deleted (only folders we made and marked), and its original is
  still there.
- **IMPORT / EXPORT, REPORT, TEST:** as now. IMPORT puts an asset mod in the
  right-hand list and a campaign mod in CAMPAIGNS. A mod made for the original
  game that replaces its campaign's files (Freako's campaign rewrite) becomes a
  campaign entry of its own, on top of the official one. REPORT on a copy says
  what it changed from the campaign it was saved from. TEST plays one mission
  of the chosen campaign with its ticks.
- **Pilots and saves:** each campaign entry keeps its own, so modded progress
  never mixes with the main menu's campaign. Starting a campaign entry can
  copy a pilot over from the main menu's campaign; the copy goes on alone and
  the original pilot is never written.

Freako's weapons, start to end: MODS, IMPORT his files, choose any campaign
(the official one or a copy), tick his mod, PLAY.

## What changes in the code

- The one list of mods turned on (`mod=` lines in mw3.cfg, mounted at start for
  the whole run) gives way to one set per campaign entry, kept beside that
  entry's pilots and saves.
- A start mounts no mods: the main menu, its campaign and multiplayer play the
  original. Mods are mounted only for a campaign (or Instant Action) entered
  from MODS.
- Mods are read when the game starts, and caches (mech definitions, the
  mechlib, textures, sounds, the menus' reader entries) are filled then. So
  the first version enters a campaign by **starting the game again into it**
  with its set (the program's own start path, behind the loading screen), and
  goes back to the original the same way. Mounting in place, without the
  restart, can come later, once every cache it would leave stale is known.
- "Save as" makes a new campaign mod: a folder of ours (with our marker) whose
  `mod.txt` names the campaign it is based on and holds the edited files.
- Multiplayer's lobby check for matching mods stays, finding none on both
  sides until multiplayer takes mods.
- Nothing is lost on the way: the `mod=` lines a player has today become the
  official campaign entry's ticks the first time this build starts.

## Instant Action (Owen, 2026-11-10)

- **A MODS button on Instant Action's own screen** opens the tick list a
  campaign has: every imported mod with a checkbox, order and conflicts shown.
- **ACCEPT starts the game again into Instant Action with those mods** and
  comes back to the same screen. With nothing ticked, Instant Action is the
  original and nothing starts again.
- **The ticks are remembered.** With mods ticked, INSTANT ACTION on the main
  menu goes straight into the modded one. The screen always says what is on
  ("MODS: 2 on"), and one button turns them all off (back to the original).
- **Custom maps:** a ticked mod's Instant Action maps (`ia_world=`) show in
  the map list beside the game's own, so a map a player built and imported as
  a mod is played here.
- **Saved mech loadouts with mods on are kept apart** from the main game's (a
  loadout with a modded weapon would not load in the original); the copy-over
  offer brings the player's existing ones across.
- **MODS, CAMPAIGNS lists campaigns only**: Instant Action has no row there.
  Underneath it is still the entry `ia` (its folder, ticks and loadouts).

## Settled in the lead's review (2026-11-10)

- **Inside a campaign entry** the player gets that entry's own screen only:
  its pilot screen, Instant Action's screen, the mission (TEST) or the editor.
  Multiplayer is not offered there. Leaving goes back to MODS on a plain run.
- **Entering and leaving restart the program** (the first version). On Windows
  in fullscreen the desktop shows for about a second between the two.
- **One set of Instant Action ticks per game**, on both games (Instant Action below).
- **The Upgrades settings are the player's, not the entry's**: one set for the
  whole game, whichever campaign is played.
- **A ticked mod that has gone missing** (removed from the mods folder) shows
  as missing, and the entry does not start until it is unticked or imported
  again. The entry's pilots and saves are kept.
- **Deleting a copy** removes its pilots and saves with it, and the confirm
  says so. Official entries and imported mods' files are never deleted this way.
- **A saved game belongs to its entry**: it lives in the entry's own pilots
  folder and records the entry and its ticks, so loading it after the ticks
  changed says so.
- **No release ever read `mod=` lines** (mods came after 1.0.5), so the move of
  those lines into the official entry only meets test folders.

## Open

- 1.1.0 waits for this (Owen, 2026-11-10), so mods change behaviour once.
- The starter mods' translation kind: kept as it is (it costs nothing), but not
  featured, since translations are not managed.

## Built: the pilot's copy (2026-11-10)

`mech3::entry_copy_pilot` (`upgrades/mods/entry_pilot_copy`), the function
behind "starting a campaign entry can copy a pilot over"; the MODS screen's
offer calls it. It copies the main menu's `pilots/<name>.mw3`, that pilot's
saved games (`pilots/games/<name>CCMM.mw3`: the name, four digits, nothing
else, so Bob's copy leaves Bob2's saves) and each one's `.rows`, into the
entry's `pilots/`, and the saved loadouts `configs/*.mw3` into the entry's
loadouts folder (given by the caller). The originals are only read. Nothing
in the entry is replaced: a pilot or a saved game of that name already there
refuses the whole copy. The loadouts are not so strict: a loadout file the
entry already has is kept (counted in `configs_kept`), and one whose place is
taken by a stray file (its `.rows` without it, a folder of its name) is
skipped (`configs_skipped`), so one stray file among the saved mechs never
costs the pilot's copy. The entry's folder must carry our marker. A
copy that fails part-way removes the files it made and the folders it made
if they are empty again, nothing else. `main_menu_pilots` lists the names
to offer. Tests: `entry_pilot_copy_*` (tests/test_entry_pilot_copy.cpp).

## Built: REPORT on a copy (2026-11-10)

`mech3::mod_copy_changes` and `mod_copy_report` (`upgrades/mods/mod_report`),
the lines REPORT adds for a copy (a mod whose mod.txt has `base=`); the MODS
screen calls `mod_copy_report(write_dir, install, folder)` after the mod's
own report. The chain is walked through the entries (`find_entry`): the
base's mod, that mod's base, down to the game's own entry. Each of the
copy's files (not its mod.txt or our markers; names compared in any letter
case) is set against the nearest mod under it that has the file: other
bytes, "replaces N files of that campaign"; the same bytes, "the same, so it
changes nothing"; in no mod under it but in the game's install, "replaces N
of the game's own files"; nowhere, "adds N new files". Its mod.txt rows
(`mech=`, `weapon=`, `world=`, `ia_world=`, `campaign=`, by name: the words
before the first key=value) are changed or added against the rows under it,
the nearest mod's winning. Up to six names are listed, then "and N more".
When part of the chain is not in the mods folder that is said, and only
what is certain is: what the copy replaces in the mods still there (a file
nothing above the gap has may have been the missing campaign's, so neither
"adds" nor the game's files are claimed). A chain that comes round to
itself ends. Tests: `mod_copy_report_*` (tests/test_mod_copy_report.cpp).

## Built: the save's entry (2026-11-10)

"A saved game belongs to its entry", in `upgrades/mods/save_mods`. The
program makes one call once an entry's mods are mounted,
`mech3::save_entry_set(write_dir, entry)` (a plain start leaves it empty);
it fills `save_entry()`: the entry's id and name, the entries it is built on
(by `base=`, nearest first), the gameplay hash of its ticks in their order
(`mods_gameplay_hash`, so a looks-only tick changes nothing) with their
names, and the same hash of everything it mounts (`entry_mount_list`: its
own mod and its bases' count there). A game saved in an entry that mounts
something of gameplay holds one more zSave entry, `Mods/entry` (text:
`entry`, `name`, `base`, `ticks`, `tick`, `mounted` lines), in place of the
mod list `Mods/identity`; an entry that mounts nothing of gameplay (the
game's own with no gameplay tick) writes nothing, the original's save. The
load's note, on the loading screen and in the log, once:

- saved in another entry: "Saved in another campaign (X); this is Y";
- the ticks changed: "This campaign's mods changed since the save: then A,
  B; now A" (or, the same ticks with other files, "A mod of this campaign
  changed since the save (A)");
- the same ticks, the entry's own or its bases' files changed: "This
  campaign's own files changed since the save";
- saved outside an entry (a pilot copied over from the main menu):
  "Saved without mods; the campaign Y now has: A" (or "its own files");
- saved in an entry, loaded where nothing is mounted: "Saved in the
  campaign X with its mods; none now".

Outside an entry the mod list is written and compared as before (`--mod`, a
developer's start). Tests: `save_entry_*` (tests/test_save_entry.cpp).

## Built: the restart on Windows (2026-11-10)

The `_WIN32` side of `sdlfe::restart_into` (`frontend/sdl/sdl_restart.cpp`).
Windows has no exec, so the program is started as a new process and this
one ends:

- its own file by `GetModuleFileNameW`, the buffer grown until the name fits
  (long paths);
- the command line by `win_command_line` (`frontend/sdl/win_cmdline`, plain
  string code built on every system): the program's path in plain quotes,
  each argument quoted by the rules `CommandLineToArgvW` splits by (as it is
  without a space, tab, newline or quote; else in quotes, a quote written
  `\"`, the backslashes before a quote or before the closing one doubled, an
  empty argument `""`);
- `CreateProcessW` with no inherited handles, the same working directory and
  environment, made suspended;
- the log: with no console stdout and stderr are `<write dir>\mw3.log`
  (`log_to_file`), and the next start renames that file and opens a new one.
  So this process writes its last line ("starting again as process N: ..."),
  closes stdout and stderr, and only then lets the new process run
  (`ResumeThread`); the new one cannot reach the log while the old one holds
  it. `AllowSetForegroundWindow` lets its window come to the front. Then
  `ExitProcess(0)`;
- the new process's own standard handles: without `STARTF_USESTDHANDLES`
  Windows gives a console program copies of its parent's standard handles
  even when no handles are inherited, so the new process started out holding
  the old log as its stdout and its rename was refused (error 32, seen on
  the PC with `restart_check`, 2026-11-10). When this run's stdout or stderr
  is a file on disk (asked of Windows' standard handles and of the C
  runtime's, which alone knows a windowed program's `freopen`), the new
  process is started with `STARTF_USESTDHANDLES` and no handles: a fresh
  program with nothing open. When they are a console or a pipe (a
  developer's run, a test reading the output) they are left, so the output
  still shows; neither holds a log;
- the new start's side (`log_to_file`, `frontend/sdl/log_start`): with that
  fix the move still went through only at the second try on the PC (50 ms
  later), so something holds the old log for a moment after the new process
  runs. The new start is therefore safe against it, whatever it is: it is
  told the old process's id (`MW3_RESTARTED_FROM` in its environment, set by
  `restart_into`; not an argument, so the start's arguments are untouched),
  waits for that process to end (2 s at most), then tries the move up to 20
  times 50 ms apart (a first start 5 times: a quick quit and start meets the
  same). The PC's finding (`restart_check`, 2026-11-10, three runs alike):
  with the first process already ended the move still went through only at
  the second try, so the holder for those milliseconds is something outside
  our two programs, and the tries, not the wait, are what protects the log. The last start's log is never emptied, restarted or
  not: when it cannot be moved the log is opened for append and the first
  line says so ("the last start's log could not be moved to mw3.old.log
  (error N after T tries): it is kept above, and this start's lines follow
  it"). Until now the move's result was not looked at and the log opened
  with "w", so a held log was overwritten. A restarted run's log says "mw3:
  started again from process N (waited for its end, M ms); the last log
  moved at try T".

Nothing waits on the old process: the launcher ends at the game's first
frame on Windows (`Launcher::watch_game`; it stays, waiting on the process,
only for an AppImage). One case is left as it is: a game whose first frame
took longer than the launcher's wait leaves the launcher up and watching,
and it would then show itself when the first process ends at a restart.

Tests: `win_cmdline_quotes_by_the_documented_rules` (a table, and a whole
line) and `win_cmdline_round_trips_through_the_splitting_rules` (3,906
arguments of the characters that matter split again by those rules), in
tests/test_win_cmdline.cpp, run on every system. `tests/win/restart_check.cpp`
is the Windows check without the game: `restart_check.exe DIR --watch` runs
the check twice (in `DIR\plain` and `DIR\wait`), waits for each and prints
the logs (0 when both passed). The first run takes `check.log` as its output
and starts itself again with arguments that are hard to quote; the second
moves that log aside before it opens anything (`log_start`, 20 tries) and
writes a new `check.log` ending "restart check: OK" when the old log is
whole in `check.old.log`, the new one fresh, and the arguments and working
directory the first run's. Plain: the move is tried at once. Wait: the
second run first waits for the first run's process to end, as the game
does. In both the move must go through within the tries, and the try is
printed. `log_start_*`
(tests/test_log_start.cpp) checks the decision on every system with moves
made to fail; it cannot show Windows refusing to move a held file. Build (the mingw
cross compiler, or any Windows one):
`x86_64-w64-mingw32-g++ -std=c++17 -Isrc tests/win/restart_check.cpp src/frontend/sdl/sdl_restart.cpp src/frontend/sdl/win_cmdline.cpp src/frontend/sdl/log_start.cpp -o restart_check.exe -static -luser32`.
