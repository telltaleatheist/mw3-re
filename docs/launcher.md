# The launcher and the first-start setup (ours; design, 2026-10-30)

Not original code: the rebuild's own front door, so a player installs,
starts and plays with no command lines (Owen, 2026-10-30). The design
sketch's screens: `assets/launcher/` holds the art (with its prompts);
the mockups were handed over in the outbox (launcher-sketch).

## The flow

Since SETUP from any source (2026-10-31, docs/setup-sources.md and
games-layout.md): a plain `mw3` start goes straight into the game when an
install is found (mw3.cfg's, `<write dir>/install`, the finder's places),
with no launcher; with none, it opens the **find screen alone** (BACK is
QUIT there) and goes on into the game in the same window once one is
found or installed. The launcher below is its own program, `mw3launcher`
(the launcher split, 2026-11-01; The two programs, below):

1. SDL, the write dir, mw3.log (as now).
2. The window, made once and kept for the game: fullscreen at the desktop
   size, its 854x480 frame (16:9 since 2026-11-08, Owen: it opened 4:3)
   fitted as the menus are (sdl_fit), letterboxed on a 4:3 screen.
3. The install, quietly: mw3.cfg's `install=`, then the finder's places
   (`sdlfe::install_candidates`), each through `setup::check_install`.
   No message box any more: the setup screen replaces the Browse dialog.
4. **The launcher screen** (mockup 1). Entries:
   - **MechWarrior 3**: "Ready, official: data 1.2, program v1.1.0, <folder>"
     (each game's data version and its program's, sitting 5 of Pirate's Moon
     below). The label (Owen, 2026-11-10: shown, never blocking) is
     "official" when the data is a release's (MW3 1.0, 1.2, or 1.0 with the
     patch's files; PM 1.0) and the program a release build (a bare vX.Y.Z
     tag, PM's pm-vX.Y.Z), else "modified (data, program)" naming which
     (`official_words`; breaks: any v tag taken as a release, the data always
     official: each caught). Mods are a feature, not a change to the game
     (lead): a neutral note of their own, "Ready, official: data 1.2,
     program v1.1.0, 2 mods on, <folder>" (`versions_words`). Or "Not found yet:
     click to find your game" (a click opens the find screen; v1.0, Owen
     2026-11-02: only then, the launcher always opens on the games). A
     folder too long for the plate is
     shortened from the middle by its drawn width ("Ready, official: data 1.2, ...: "
     kept, more of the folder's end; `fit_middle`, text_fit.cpp): a long
     path ran past both sides on Owen's PC (lead, 2026-10-31).
     `launcher_long_path_fits_the_button`.
   - **Pirate's Moon**: greyed, "Coming later", while `mw3pm` is not beside
     the launcher; with it, "Ready: <folder>" or "Not found yet: click to find
     your game" (below, Pirate's Moon).
   - **MechWarrior (1989)**: only when the MW1 port is found beside ours
     (below).
   - **Game files** (Owen, 2026-11-02, after the rework): the find screen
     at any time, to repick or refresh the game's files (below), and
     **Quit** (for fullscreen; Escape too).
5. MechWarrior 3 chosen: the launcher's textures are freed and the start
   goes on exactly as today in the same window (mounts, data version,
   movies helper or import, `run()`), so the intro movie is the next
   thing seen.

## The find screen and the notes (v1.0, Owen and the lead, 2026-11-02)

Reached by choosing a game without its files (none found at the start, or
mw3.cfg's gone), or alone on a plain mw3 start with none. At the top, in
the launcher's text, `kFindWords`: "MechWarrior 3 needs your own copy of the
game. Show us your disc, a disc image or download (.iso, .cue, .zip), or a
folder where it's installed. We copy what the game needs into your user
folder; your disc and any installed copy are left untouched."

| plate | shown | what it does |
| --- | --- | --- |
| Find the game | always (one option, Owen 2026-11-02) | the finder (mw3.cfg's, the installer's places) and a disc in a drive (`find_discs`) first: either is used at once, no picker. With neither, "Not found on this computer or in a disc drive. Pick it: a disc image, a download, or Mech3.exe." and one file picker opens (.iso .cue .bin .zip .7z .rar .., Mech3.exe, any file). `pick_source`: an image or archive is the source; any other file stands for its folder (Mech3.exe or any file of an installed copy, zbd\\ and video\\ up one; a disc's folder), a .bin for its .cue. Then `source::map_source` on the worker ("LOOKING AT YOUR GAME", its bar, CANCEL): an installed copy on disk is used where it is, anything else INSTALL / BACK under its summary and `setup::install_from_source` into `<write dir>/install`, a cue's music into `<write dir>/music`. SDL3 has no file-or-folder dialog on the three systems (none on the Mac's panel either through SDL 3.4), so a folder is picked by one of its files |
| Back | always | the games (QUIT on a plain mw3 start) |

Once found or installed the game starts at once (`use_install`, `play`).
When the install lacks the movies (a CD-type install leaves them on the
disc: an empty video folder; dev 2's PC check) or the music, or its data is
of an unknown version, the notes come first ("BEFORE YOU PLAY", one OK that
starts the game; Escape as OK). They say what is missing (launcher_notes:
"Your install has no movies (they stayed on the CD) and no music ...").

| plate | shown | what it does |
| --- | --- | --- |
| Add from my disc | the notes (and GAME FILES' screen), when the movies or the music are missing | a disc in a drive first; else "No disc in a drive ..." and one picker (its image, or any file of the disc for its folder: `pick_source`); `setup::add_from_disc` on the worker ("ADDING FROM YOUR DISC"): the disc's VIDEO\*.AVI into `<write dir>/video` (`add_movies`, from a drive, the disc's folder, an .iso or a .cue's data track), the music from a .cue only (`add_music`; from a drive, a folder or an .iso it says the music needs a .cue). Back to the notes with what was added |

The game finds movies in the write dir before the install's (zutl::find),
so the Indeo helper plays the added AVIs on Windows. Elsewhere the movies
are converted in the launcher (Owen's first start, 2026-11-02, sat 44 s on
"Starting MechWarrior 3..." while mw3 did it with only a status line): before
the game starts, and right after ADD FROM MY DISC, any AVI of the install or
the write dir without its .fmv goes through mw3import on the work screen
("CONVERTING THE MOVIES", "Converting the movies (K of N)", the movie's name,
CANCEL; movie_import.h, its "progress K N name" lines). A cancel keeps the
ones done; mw3's own first-run import (`first_run_import`, its small progress
window) stays the fallback for a start without the launcher. None on Windows,
nor with --no-import (passed on to the game) or MW3_NO_IMPORT; MW3_IMPORTER
names a stand-in for the checks. `sdl_program_launcher_converts_the_movies_before_the_game`.

The file picker (Owen: very slow to open on macOS 26): SDL shows NSOpenPanel
as a sheet on the launcher's window, and the panel is another process that
answers through the main run loop, which the launcher's vsync frame loop
served once a frame. While a picker is up the launcher draws nothing and
waits in SDL_WaitEventTimeout (the run loop serving the panel at once). The
log times it: "the file picker asked for", "the picker took the focus N ms
after", "the picker answered (...) N ms after".

GAME FILES opens the same screen with the install in use ("Using
MechWarrior 3 1.2 at <folder>. No music.") and, while it lacks movies or
music, ADD FROM MY DISC (back to this screen after). A new pick goes back
to the games, not into the game, its folder saved to mw3.cfg. A disc or
archive install into `<write dir>/install` is built in `install.new` and
swapped in only once complete (`swap_in_folder`: the old copy set aside as
`install.old`, removed after the rename, put back if it fails), so a cancel
or a failure leaves the old one working and no second copy stays. A copy
picked to be used where it is leaves the launcher's own earlier copy
unpointed-at (mw3.cfg holds one folder), so that is removed.
`sdl_program_launcher_game_files_repicks`.

Keys and a pad (`nav`): the arrows, Tab or the d-pad step through the
plates (GAME FILES and QUIT included), Enter, Space, A or Start press,
Escape, B or Back go back. MW3_LAUNCHER_NAV (checks) feeds them as a list.

Gone in v1.0: the old Setup screen, "A folder instead", "Search
again", "Apply the 1.2 patch" (the patch is built in, patch12.h, so a 1.0
install needs nothing; `setup::apply_patch` and the launcher's kPatch pick
are kept but unreachable). The installer has no "MechWarrior 3 (no
launcher)" shortcut any more; mw3.exe is still installed.

Each screen logs its plates ("mw3: launcher: screen find: FIND THE GAME,
BACK"), which the checks follow.

Each operation runs on a worker thread; its `Progress` (stage, fraction,
message) goes to the screen under a mutex: the bar, the stage and the
current file (mockup 3). Cancel makes the callback return false
(Mw3 dev's calls stop within a file and change nothing). The result's
`summary` is shown in a line at the top of the launcher, green or red
(mockup 4); `warnings` under it; `detail` only in mw3.log. The first
start's notes are the notes screen above.

File and folder pickers are SDL's (`SDL_ShowOpenFileDialog`,
`SDL_ShowOpenFolderDialog`, as `sdl_install.cpp` waits on them now),
filtered to .iso/.cue, .zip, or Mech3.exe.

## Drawing it

- An 854x480 16-bit (565) canvas each frame (16:9; 640x480 until
  2026-11-08), as the game's 2D path. The art spans the width; the plates,
  boxes and bar stay the 640-wide block they were laid out as, 107 in
  (`kX`), the game rows widened to 500 for their longer lines: the
  starfield scrolling slowly to the left (a few pixels a second, its ends
  cross-faded), the metal bands top and bottom scrolling at a third of
  that, the title and emblem, the plates.
- Text: SDL3_ttf (already linked) with an OFL font shipped beside the art
  (a condensed technical face, e.g. Saira Condensed or Rajdhani; OFL.txt in
  licenses\ and THIRD-PARTY.txt). The mockups stand in DIN Condensed.
- PNGs through SDL 3.4's `SDL_LoadPNG` (the PC's deps are 3.4.12): no new
  library.
- The hover ripple is the main menu's: `zrndr::ripple` (0x00558dd0,
  ported) on the canvas around the hovered plate, its ring spreading and
  fading as `PushButtonRipple` does (twod.md, Ripple; its radius, max
  radius and speeds from the main menu's dialog values).
- Sounds: with an install, the main menu dialog's rollover and select
  sounds through zSnd from the install; without one, silent (no sounds
  of ours).
- Mouse, keyboard (arrows, Enter, Escape) and a pad's d-pad and A/B.

## Pirate's Moon (2026-11-06, Owen and the lead)

`mw3pm` is found in the launcher's own folder (`own_dir`, as `mw3`: a bundle's
Contents/MacOS, the AppImage's usr/bin; `MW3_PM_BIN` for checks), the final
package's layout. It is a separate game: it gets `--install <folder>
--no-launcher`, the window and sound choices, and `--ready-file`, never MW3's
install or write dir; its settings and saves are its own. Its folder is
`pm_install=` in the launcher's mw3.cfg, checked at start with `map_source`
(a Pirate's Moon install, in a plain folder). A click on the entry starts it,
or, with no folder yet, asks for one. GAME FILES acts on the game last clicked:
for Pirate's Moon the same folder picker, which also keeps MY DATA FOLDER, ADD
FROM MY DISC and SETUP out of it (later items). Installing from a disc and its
music (the image's track 2) are later too.

### Pirate's Moon as MechWarrior 3 (plan, 2026-11-09; toward the combined 1.1.0)

The lead, from Owen: the launcher does for Pirate's Moon everything it does for
MechWarrior 3. Owen's rules for all of it: open and easy (players may replace
the games' programs or data; no integrity or hash refusals, no automatic
repair: the launcher re-detects what is there, shows it, and explains plainly
only what truly cannot run); nothing it does touches a player's mods, pilots,
saves, settings or imported data; each game still starts on its own, and the
single-game packages stay. One sitting each, each with its tests:

1. **Found by itself.** `sdlfe::install_candidates` takes the game: Pirate's
   Moon's registry key (HKLM `SOFTWARE\MicroProse\MechWarrior 3 EP1\1.0`
   "Program", both views and the VirtualStore copies), its default and usual
   folders, and the same Wine, Proton and Lutris prefixes as MechWarrior 3's,
   plus the Mac's (CrossOver and Whisky bottles, both games). Each candidate
   through `map_source`, so neither game's folder is taken for the other's.
   The launcher looks when mw3.cfg's `pm_install=` is empty or no longer
   holds the game, and saves what it finds. Tests: a fake HOME with prefixes
   and bottles (each game found in its own, an MW3 folder never taken as
   PM's, the saved one first); Windows' registry part by the mingw build.
   **Done (2026-11-09):** `install_candidates(pirates_moon)`, `Launcher::find_pm`;
   `sdl_program_launcher_finds_pirates_moon` (MW3's data under PM's folder
   name in the prefix searched first, never taken; PM in a CrossOver bottle
   found, saved as pm_install= and started there). Breaks: the game check
   dropped, the bottles dropped, PM's folder name changed: each caught.
   MechWarrior 3's search is a superset of what it was (the bottles added).
2. **Installed from the disc or an image** (.iso, .cue/.bin, a zip or 7z of
   either) into the rebuild's own folder, as MW3's SETUP does, music from
   the image's track 2. `setup::install_from_source` taking a Pirate's Moon
   source (today refused: "Its support comes later"). Tests on game/pm-disc
   (the image), the result read by `map_source` as PM and played to its
   first frame by mw3pm.
   **Done (2026-11-09):** `install_from_disc(..., pirates_moon)` and
   `setup::install_from_source` taking a PM source: from the disc, its image,
   its cue (track 2 as `music/track02.wav`) or an archive of any of them; no
   1.0 checks and no patch (PM had one release, "1.0"), `zbd\reader.zbd` and
   `zbd\c1\readersa1.zbd` required, the disc's loose `MECH3.EXE` (beside the
   cabinet, not in it) copied in as `Mech3.exe` and staged from an archive
   too; an installed PM copy on disk used where it is. The folder it goes to
   is the launcher's choice (sitting 4: PM's own settings folder).
   `setup_install_pirates_moon_from_its_disc`: the cue and a zip of the image
   each give game/pm's files at their sizes, the exe and reader.zbd byte for
   byte, read back by `map_source` as PM; with `MW3_PM_BIN`, mw3pm plays the
   result to a frame. Breaks: the loose exe dropped, the game not passed,
   the exe not staged: each caught.
3. **Checks before the first game**, in plain words: missing files; an
   empty `zbd\soundsH.zbd` (the disc's copy put in Pirate's Moon's settings
   folder, never into the install); anything else that blocks a start.
   Tests: each case made in a copy, the words checked, the install untouched.
   **Done (2026-11-09):** `setup::check_install(dir, write_dir, true)` and
   `Launcher::start_pm`. What blocks was measured on copies of game/pm with
   one file gone, mw3pm run to its menu and into c1m1: only an empty or
   missing `zbd\reader.zbd` and a missing `Mech3Msg.dll` stop it starting
   (refused, in words, nothing started). Missing `interp.zbd`, `mechlib.zbd`
   or `motion.zbd` stop the missions ("Failed to Interpret: c1.gs -- Mission
   Load Aborted"), and an operation without its `gamez.zbd` or `reader.zbd`
   crashes on its load (MW3's mw3 too): notes naming them. An empty or
   missing `soundsH.zbd` only drops to soundsL's 11 kHz sounds: a note.
   A file in PM's settings folder counts (searched before the install). The
   notes show once on BEFORE YOU PLAY (their CRC kept as `pm_notes=` in
   mw3.cfg; new ones show again), then only in the log. `setup::add_pm_sounds`
   takes the disc's `zbd\soundsH.zbd` from its cabinet into PM's settings
   folder's `zbd\` (one already there kept), for sitting 4's ADD FROM MY
   DISC. The launcher finds PM's settings folder as mw3pm does
   (`data_folder_at_start(..., "Pirate's Moon")`; `MW3_PM_WRITE_DIR` for
   checks) and writes nothing there. Tests: `setup_pirates_moon_checks_before_a_start`
   (each case by links, the words, the disc's sounds added and the install's
   listing unchanged, a cancel), `sdl_program_launcher_checks_pirates_moon`
   (refused, notes once, then logged, a new note shown, the settings
   folder's sounds counted). Breaks: the Mech3Msg.dll check dropped, the
   settings folder not read, the notes always shown, operation 4 missed:
   each caught.
4. **GAME FILES for Pirate's Moon**: its folder, ADD FROM MY DISC (the
   music), SETUP, and MY DATA FOLDER where it applies, the same screens as
   MW3's acting on the game last clicked. Tests: each button through the
   screens on PM, MW3's files untouched.
   **Done (2026-11-09):** GAME FILES opens on the game last clicked
   (`files_pm_`), and PIRATE'S MOON with none found opens its find screen
   as MECHWARRIOR 3 does (it asked for a folder before). On PM every screen
   uses PM's own settings folder: "MicroProse/Pirate's Moon" beside MW3's
   usual one, as SDL_GetPrefPath makes it for mw3pm (worked out here, not
   created), or where its pointer leads.
   - FIND THE GAME searches as at the start (sitting 1, plus its own
     `install\`), then a disc in a drive, then the picker. A pick is
     installed into `<PM settings>\install`, its music into `music\`. An
     installed copy is used where it is. MechWarrior 3 picked there is said
     ("This is MechWarrior 3, not Pirate's Moon: give it under MECHWARRIOR
     3's GAME FILES"), and PM picked in MW3's is said the other way
     (`game_words`).
   - ADD FROM MY DISC (`setup::add_pm_from_disc`) adds the music from a
     .cue and the soundsH.zbd (sitting 3), each only when missing.
   - MY DATA FOLDER moves PM's folder (its pointer in PM's usual one, a
     "Pirate's Moon Data" folder inside a drive's root), its install with
     it (`pm_install=` follows). MW3's folder and settings are untouched.
   - PM's notes before a start add MW3's no-music note.
   Checks: `MW3_LAUNCHER_FILES=pm` (GAME FILES on PM), `MW3_LAUNCHER_ADD`
   (ADD FROM MY DISC's pick), and `--launcher-pm` with `MW3_LAUNCHER_PICK`
   and none found (its find screen, the pick installed, then started).
   `sdl_program_launcher_game_files_pirates_moon` runs install, MW3 picked,
   add, move, and the start from the moved folder. Breaks: the wanted game
   ignored, the install into MW3's folder, PM's add dropped, PM's move not
   followed: each caught.
   **MechWarrior 3 too (2026-11-10, lead):** the check is `setup::start_check(dir, write_dir, game)`, both games'.
   MW3's folders are c1-c4, c4b ("operation 4's second part") and t1 ("training"); measured on copies of
   game/mw3-1.2 the same two files stop it starting. MW3's PLAY (`Launcher::mw3_ready`) shows its new notes once
   (`mw3_notes=`), on the same screen as a first start's notes when there is one (`mw3_notes_seen`), and leaves a
   blocker to mw3, which says it in its own box as the original does ("Cannot read mech3Msg.dll from the game
   folder.", "No MechWarrior 3 install at ..."): a second refusal in the launcher added nothing. PM's launcher
   refuses (mw3pm's own line is less plain). `setup_mechwarrior3_checks_before_a_start`,
   `sdl_program_launcher_checks_mechwarrior3` (breaks: training left out, the notes always shown: caught).
5. **Versions**: each entry shows its game's data version and its program's
   version. Test: the launcher's text for both games.
   **Done (2026-11-09):** "Ready: data 1.2, program v1.1.0, <folder>", PM's
   the same. PM's data version is measured as mw3pm measures it, its 14 main
   files against the 1.0 disc's sizes and CRCs (branch pm's kPmFiles, copied
   into `check_pm`), a settings folder's file first: "1.0", "unknown" (shown
   "data unknown (a mod?)") or "incomplete". Each program's version comes
   from its file, never by running it: `mw3_version_tag` in mw3_main.cpp
   ("MW3-PROGRAM-VERSION:" and MW3_VERSION, kept by `used`, printed by
   --version), found by `program_version` reading the file (a tag across its
   1 MB chunks too, the launcher's own half-needle skipped). Running mw3.exe
   to ask would load its DLLs, and Windows shows a box for a missing one. A
   file without the tag (a script, an older build) shows no program
   version. mw3pm gets the tag once pm merges main (its --version line
   should print `mw3_version_tag + 20` as mw3's does). Tests:
   `sdl_program_launcher_shows_each_games_versions` (both lines, a tag
   across the chunk edge, an empty match skipped, a file without one never
   run, the real mw3's equal to its --version) and the data cases in
   `setup_pirates_moon_checks_before_a_start` (a changed file of the same
   size, a missing one). Breaks: the program left out, the data CRC not
   checked, the chunk edge lost, the empty tag taken: each caught.

## Installed anywhere (2026-11-06, a player's request)

Nothing keeps where the programs are. They find each other beside themselves;
mw3.cfg keeps only the games' folders (`install=`, `pm_install=`), in the write
dir, so a moved package carries on. `sdl_program_launcher_follows_a_moved_folder`;
a bundle's start from another folder and volume was run on the Mac. Windows has
the plain-letters limit below. macOS asks the first time a bundle reads a
removable or network volume (a game on an external disk): the launcher waits in
its read until the player answers, which looks like a slow start.

Windows: the folders a player picks may be on any drive, with spaces and
apostrophes. Letters outside ASCII are the limit (`program.md`, The data
folder): the code is UTF-8 throughout, but on Windows `fs::path` and `fopen` read
narrow strings in the ANSI code page and `fs::path::string()` writes them in it.
MY DATA FOLDER refuses such a name; the install has no such guard.

## MechWarrior (1989)

Elthial's MechWarrior SDL port (github.com/Elthial/Mechwarrior-sdl-port;
MIT for its own code, credited on its entry's line, "Elthial's SDL port";
we ship none of it, so no licence file goes with ours). Shown when
`mechwarrior.exe` (`mechwarrior` elsewhere) is beside `mw3` or in a
`MechWarrior1989\` folder beside it (`MW3_MW1_PORT` names it, for checks).
It needs the player's MW1 folder (`BTECH.EXE`, any case): picked once with
a folder picker and kept as `mw1_game=` in mw3.cfg (`save_config_value`).
Started with `SDL_CreateProcess` as its own script (Run-MechWarrior.ps1)
does: `mechwarrior <game dir> <capture dir>`, the capture dir
`<write dir>/mw1/captures` (made first: the port wants it existing) and
MECHWARRIOR_SAVE_DIRECTORY `<write dir>/mw1/saves`; its MechWarrior.ini
is its own, beside it. The launcher's window hides until it exits, then
comes back with a line saying how it closed. Built 2026-10-31;
`--launcher-mw1` presses it at once (checks);
`sdl_program_launcher_starts_mechwarrior_1989` (a stand-in script).

## Mods (2026-11-08, modding item 5)

**On the game last clicked (2026-11-10, the combined package):** after a click
on PIRATE'S MOON, MODS lists and writes Pirate's Moon's own settings folder
(its `mods\` and mw3.cfg; `mods_target(o, true)`), takes `for=mw3pm` mods and
refuses `for=mw3` ones in words (`mech3::mods_game`, "mw3" here, set to "mw3pm"
around each operation; branch `pm`'s own default is "mw3pm"), adds and exports
there, and checks against PM's install. CHECK stays MechWarrior 3's: this
program's readers cannot read PM's version-2 archives, so for PM it points to
`mw3pm --check-mod`. MW3's folder and mw3.cfg are untouched.
`sdl_program_launcher_mods_on_pirates_moon` (breaks: MODS always MW3's, the
for= left "mw3": each caught); `MW3_LAUNCHER_MODS=pm` opens it for checks.

MODS on the main screen (beside GAME FILES and QUIT) opens the list of the
settings folder's `mods\` (`mech3::mods_list`, docs/modding.md, The
launcher's MODS list): six rows a page, the enabled mods first in the order
they win, the rest by name. A row's second line says ON or OFF and the
folder, why a folder is no usable mod (the row greyed), what it shares with
a mod above it, and after CHECK what the game would refuse in it. Clicking a
row turns it on (last, below the others) or off; UP and DOWN move the
selected enabled mod; each change rewrites mw3.cfg's `mod=` lines at once
(written beside it and renamed over it) and counts from the game's next
start. The game is chosen in one place, `mods_target` (its settings folder
and the game `for=` must name): MechWarrior 3's here, Pirate's Moon's own
folder when the modding work crosses to its branch. Below the banner, as
the other screens: the rows, the status under them, and the mods folder's
path in the footer.

## Built (2026-10-30, mac5/launcher-ui)

`frontend/sdl/launcher.cpp` (`sdlfe::run_launcher`), called from
`mw3_main.cpp` for a player's plain start (not `--hidden`, `--frames`,
`--shot`, a chain, `--mission`/`--fly`, `--movie`, `--dialog`, `-nofmv`, a
named install or `--install-from`/`--patch`; `--no-launcher` skips it):
- Its own SDL_Renderer window (fullscreen unless `--windowed`/`--window`),
  an 854x480 (16:9) RGB565 canvas letterboxed into it; a window without
  `--window` is 1280x720. PLAY hands the window to the
  game (2026-10-31): the renderer is freed and `SdlWindow::init` claims the
  same window for its GPU device instead of opening a second one. The
  window is not high pixel density, as the game's never was (its sizes are
  the window's points). `--launcher-play` presses MECHWARRIOR 3 at once
  (for checks); `sdl_program_launcher_window_goes_on_to_the_game`. The
  window's events are flushed at the hand-over, so the launcher's lost
  focus cannot reach the game as a deactivation (program.md, Message pump).
- The install found quietly: mw3.cfg's, `<write dir>/install` (the disc
  install's), the finder's places, a development copy; each through
  `setup::check_install`. None: the setup screen first.
- The main screen: MechWarrior 3 (ready, its version and folder, or "not
  set up yet"), Pirate's Moon greyed, MechWarrior (1989) when its port is
  there (below), SETUP, QUIT.
- The setup screen: FIND (the finder, else a folder picker), INSTALL (a
  disc `find_discs` sees, else a .iso/.cue picker; `probe_source`'s line and
  INSTALL/BACK, then `install_from_disc` into `<write dir>/install`, a
  .cue's music into `<write dir>/music`), the 1.2 PATCH (enabled for 1.0 or
  a mix; a .zip picker, `apply_patch`), the MUSIC (a .cue picker,
  `add_music`), BACK. Each operation on a worker thread with the bar, its
  stage and file, and CANCEL; its summary on the main screen's line.
- Art from `assets/launcher` (beside the exe in a package, the repo's in a
  development build: `MW3_ASSETS_DIR`); the font `assets/launcher/font.ttf`,
  Saira Condensed Bold (SIL OFL, `font-OFL.txt` beside it; from 1.0.4, after
  an Arch player with none of the fixed paths got empty buttons), else a
  condensed system face (Bahnschrift on Windows, DIN Condensed on the Mac,
  docs/linux.md on Linux). A placeholder, as is the title art's "MW3 REBUILD".
- The games are round badges (2026-10-31, Owen: the ripple is a circle's):
  `badge_mw3.png`, `badge_pm.png`, `badge_mw1.png` (assets/launcher, ours,
  their prompts in its README), each with its name and line beside it,
  the ripple from the badge's centre.
- Spaces (2026-11-02, the lead from setup.png: "No MechWarrior 3was
  found"): DIN Condensed's space is 2 pixels at 13 and its digits draw a
  pixel past their advance, so a space after a digit vanished. A font
  whose space is under a fifth of its size has each space doubled, drawn
  and measured alike, in the launcher's one text path (`Text::shown`,
  `widen_spaces` in text_fit.h); the gap after "3" goes from 2 pixels to 4
  (the font's own "a g" is 3). `launcher_narrow_spaces_widen`.
- Only the round badges ripple (2026-11-02, Owen after the PC's fresh
  install: the ripple belongs to a circle, as in the original's menus). A
  square plate hovered is brightened and lights its edge in the frame's
  amber (a line, a softer one inside); the rollover sound as before. The
  shot logs what the hovered entry drew (`hover <label>: ripple|edge`),
  and `sdl_program_launcher_screens` checks it on each screen with the
  edge's pixel.
- The ripple is the main menu's own (Owen: "much slower than the original
  ripple"): `mech3::menu_ripple_step` (menu_ripple.h), the one SlideButton
  runs (0x00485650): from 20 pixels, 120 a second, the amplitude 7 less 0.3
  a 30 fps frame; drawn by 0x00558dd0. `launcher_ripple_matches_the_main_menus`.
- Paced by vsync (else a precise sleep to 60 a second). Timed on Owen's PC
  (2026-10-31): the first frame drawn 306 ms after the start (art 86 ms,
  the install search 197 ms), shown in a window at 339 ms; 55.9 frames a
  second with vsync, 54.8 with the sleep, the draw 2.6 ms (direct3d11). The
  start was never slow: the old ripple was (80 pixels a second, 2 s long).
  The log gives these at every start and quit.
- The hover ripple is `zrndr::ripple` on the canvas. The sounds are the
  main menu's own (2026-10-31): sounds.zrd's MAINMENU set, menu_rollover
  (sfx_button2.wav at 0.7) when the hovered or focused button changes and
  menu_select (sfx_button6.wav) on a press, read from the install's
  soundsL.zbd (else soundsH.zbd) as zSnd reads them, through an SDL audio
  stream. None without an install (loaded once one is known, after SETUP
  too) or with `--nosound`; `MW3_LAUNCHER_MUTE` plays them at no volume
  (checks). `sdl_program_launcher_plays_the_menus_sounds`.
- Mouse, arrows/Tab, Enter/Space, Escape (back, or quit on the main
  screen).
- The first start's notes (2026-10-31): with no `install=` in mw3.cfg yet
  (PLAY saves one) and something to say, the launcher opens on BEFORE YOU
  PLAY, then OK to the main screen. The notes (`launcher_notes`,
  launcher_notes.cpp) are the start's in the launcher's words: 1.0 data or
  a mix (SETUP can apply the 1.2 patch), files of no known version, and no
  music (SETUP's ADD THE MUSIC, or a rip as track02.ogg ... in the write
  dir's music; the folder shortened to fit). Logged on every start. Before
  this, a player who came through the launcher never saw them: the install
  it hands over is not a searched one, so the message box after it
  (mw3_main, still there for `--no-launcher`) did not show.
  `launcher_first_start_notes`.
- `--launcher-shot FILE [--launcher-screen main|setup|work|notes]` draws one
  screen into a BMP; `sdl_program_launcher_screens` checks the four.

## The two programs (2026-11-01, the launcher split; games-layout.md)

- `mw3launcher` (`launcher_main.cpp` over the same `launcher.cpp`): the
  main icon. Windows: mw3launcher.exe, "MW3 Rebuild" in its version
  resource, no console in any build (its output in the write dir's
  mw3launcher.log; the game's stays mw3.log); the installer's main and
  desktop shortcuts, "MechWarrior 3", start it; "MechWarrior 3 (no
  launcher)" in the Start menu starts mw3.exe. Mac: "MechWarrior 3.app"'s
  own executable (CFBundleExecutable), mw3 beside it in Contents/MacOS.
  Linux: the AppImage's entry (mw3.desktop's Exec, its Name "MechWarrior
  3"). The installed names are the game's (Owen, 2026-11-01); the release
  files keep MW3-Rebuild-... (release.md).
  Each package script checks both programs are in it.
- MECHWARRIOR 3 starts `mw3` beside it (its own folder: `_NSGetExecutablePath`,
  `/proc/self/exe`, `GetModuleFileNameW`; `MW3_GAME_BIN` for checks) as
  `mw3 <install> [--write-dir D] [--windowed | --window WxH] [--nosound]
  [--hidden] --ready-file F`, saving the install to mw3.cfg first, with its
  own stdio (none: the game writes mw3.log itself; the launcher's log pipe
  in its stdout would hold the launcher's exit until the game's). The
  launcher stays on screen ("Starting MechWarrior 3...", input ignored)
  until the game writes F at its first frame with a swapchain image
  (`sdlfe::mark_ready`, sdl_render3d.cpp), so the switch shows no empty
  moment, then **closes**, the game running on alone (Owen, 2026-11-01;
  until then it hid and came back when the game ended). A failed start
  leaves it up: a game that exits first has its exit code on the status
  line; no first frame in 20 s says so on the status line ("has not shown
  a picture yet") and the launcher keeps watching, closing at the frame if
  it comes. `--hidden` keeps its window unshown and goes on to the game
  (checks); `MW3_LAUNCHER_READY_MS` shortens the 20 s for them.
  While the game works before its first frame it says what it is doing in
  F.status (`sdlfe::set_ready_status`: its first start's movie import,
  "Importing your movies (first start only)..." with the count when the
  importer gives one, then "Starting MechWarrior 3..."; removed at the first
  frame). The launcher reads it four times a second and shows it on its
  status line in place of the 20 s "not shown a picture yet" (Owen,
  2026-11-02: a first start sat a minute on "Starting MechWarrior 3...").
  Test: `sdl_program_launcher_shows_the_games_status`.
  Tests: `sdl_program_launcher_starts_mw3_as_its_own_program` (the
  launcher's process ends at the ready file, the stand-in game still
  running) and `sdl_program_launcher_stays_when_the_game_fails_or_is_slow`.
  MechWarrior (1989) is unchanged: it has no ready signal, so the launcher
  hides until it exits, as below.
- The gap, measured (the launcher's log: "the game's first frame N ms after
  MECHWARRIOR 3 was pressed"; the game's: "first frame shown N ms after the
  start"): Mac, the app, 1.94 s, of which the game's own start 1.76 s (a
  development build: SDL and mounts 0.20 s, the data version 0.08 s, the
  window and GPU device 0.30 s, the original's start-up to its first frame
  1.1 s) and the process's start about 0.2 to 0.45 s. The old one-process
  hand-over saved the window and SDL's start (about 0.5 s) but drew nothing
  either until the game's first frame; now the launcher is that picture.
  Windows: over ssh the game gets no swapchain (windows.md), so the gap is
  to be measured in a desktop session.
- `mw3` started alone: straight into the game when an install is found;
  the setup screen alone otherwise (`setup_only`, in the same window into
  the game). `--launcher`, `--launcher-shot`, `--launcher-play` and
  `--launcher-mw1` are mw3launcher's now.
- Tests: `sdl_program_launcher_starts_mw3_as_its_own_program` (a stand-in
  game keeps its arguments and writes the ready file: the install, the
  write dir, the window, --nosound, --ready-file; the launcher's first-frame
  and exit lines), `sdl_program_launcher_window_goes_on_to_the_game` (now the
  setup screen's hand-over in mw3: an installed copy picked, the window
  claimed, a frame shown), `sdl_program_plain_start_goes_straight_in_or_to_setup`
  (mw3 refuses --launcher), the screens, sounds and MechWarrior (1989)
  checks on mw3launcher.

## Open

- The working title ("MW3 REBUILD" on the title art) and the font: Owen's.
- The calls are Mw3 dev's `mech3::setup` (game_setup.h, mac4/setup-api).
- Once the GUI-subsystem build lands, mw3.log is the only console.

## Updates (planned 2026-11-10; Owen 2026-11-09, no signing)

CHECK FOR UPDATES in the launcher, the same flow on all three platforms, never
the settings folders (MW3's, PM's, or where their pointers lead): only the
program files are replaced.

1. **Check**: GitHub's releases list
   (`api.github.com/repos/telltaleatheist/mw3-re/releases`), the highest
   bare `vX.Y.Z` tag of the releases that are neither drafts nor
   prereleases (`pm-v*` game releases and anything else left out; lead
   2026-11-10: not the Latest flag, which anyone may set otherwise), compared
   with the launcher's own tag (`MW3_VERSION`, numbers part by part; a build
   between releases counts as its last release). Fetched with the
   system's `curl` (macOS, Linux, and Windows 10 1803+ ship it: no new
   library, and its HTTPS is the system's), run as a child with no shell, a
   time limit and its arguments an array; a small JSON reader for the fields
   needed (`tag_name`, `draft`, `prerelease`, `body`, `assets[].name`,
   `assets[].browser_download_url`). The individual games' downloads may sit
   on the same release as extra assets: only the platform's package asset is
   taken. `MW3_UPDATE_API` names another URL for
   checks (a `file://` JSON). Tests: newer, same, older, a dev build, no
   network, a broken answer: each said in plain words.
   **Done (2026-11-10):** `upgrades::update` (`parse_releases`, a small JSON
   reader; `package_tag`, `newest_package`, `compare_versions`, `judge`) and
   `sdlfe::curl_get` / `check_for_updates` (`frontend/sdl/sdl_update`; curl
   found at Windows' System32\curl.exe or /usr/bin/curl, never through
   PATH; `--proto =https`, `file://` only for `MW3_UPDATE_API`). UPDATES on
   the main screen (GAME FILES, MODS, UPDATES, QUIT) asks on the worker and
   says the verdict on the status line. Checked once against GitHub's real
   list: v1.0.5 the newest package, this build up to date. Tests:
   `update_reads_the_releases_list` (escapes, \u, a surrogate pair, four broken
   answers), `update_picks_the_newest_package_release` (drafts, prereleases,
   pm-v*, rc tags, 1.10 above 1.9), `update_compares_versions` (a dev build
   counts as its release), `update_says_what_it_found`,
   `sdl_update_fetch_by_curl`, `sdl_program_launcher_checks_for_updates`.
   Breaks: drafts kept, rc tags taken: each caught.
2. **The notes and the question**: an UPDATE screen with the release's notes
   (its body, wrapped and paged, the markdown's `#` and `-` shown plainly),
   "Download and install MechWarrior 3 Rebuild 1.2.0?", DOWNLOAD AND
   INSTALL and NOT NOW. Tests: the screen's text from a fixture release.
   **Done (2026-11-10):** a newer release opens the UPDATE screen
   (`Launcher::show_update`): "MECHWARRIOR 3 REBUILD 1.2.0", its notes as
   plain lines (`upgrades::update::notes_lines`: headings without #, items as
   bullets, **, * and ` marks dropped, links as their text) wrapped to 500 px
   and paged with < and >, the question with "Your mods, pilots, saves and
   settings are kept.", DOWNLOAD AND INSTALL (sitting 3) and NOT NOW. Same or
   older: the main screen's status line, as sitting 1. `--launcher-screen
   update` shoots it (with `MW3_UPDATE_API`). Tests:
   `update_notes_as_plain_lines`, `sdl_program_launcher_checks_for_updates`
   (the screen's plates and notes; none for same, older or broken). Break:
   no screen for a newer release: caught.
3. **The download and its check**: the platform's asset
   (`MW3-Rebuild-Setup-<v>.exe`, `MW3-Rebuild-<v>-mac.dmg`,
   `MW3-Rebuild-<v>-x86_64.AppImage`, the packaging's names) and the
   release's `SHA256SUMS`, over HTTPS by curl into the system's temp folder,
   with the bar and CANCEL; the asset's SHA-256 (patch_import's) against its
   line. A mismatch, a missing line or a cut download installs nothing and
   says so. Tests: good, wrong sum, no sums file, cancelled, cut short.
   **Done (2026-11-10):** `upgrades::update::platform_asset` (the games' own
   downloads on the release are other names, never taken), `sums_line` (text
   and binary forms, any case), `sdlfe::curl_download` (to `.part`, renamed when
   complete; no total limit, a minute without data ends it; the bar from the
   file's size against the asset's; CANCEL kills curl and removes the part),
   `download_update` (SHA256SUMS first, then the package, hashed by
   `mech3::sha256_file_hex` a megabyte at a time). DOWNLOAD AND INSTALL runs it
   on the worker into `<temp>/MW3-Rebuild-update` (never a settings folder);
   every failure removes the file and says why ("did not match its checksum,
   so nothing was installed", "has no SHA256SUMS", "has no line for ...",
   curl's reason). Tests: `update_picks_the_platforms_package_and_its_sum`,
   `sdl_update_download_and_its_check` (good, wrong sum, no line, no sums, no
   package, cancelled, gone), `sdl_program_launcher_checks_for_updates` (the
   newer release downloaded and checked through the screen, into the test's
   own TMPDIR). Breaks: the sum not compared: caught; a cancel keeping the
   part file: not caught (a local copy ends before the cancel can meet a part
   file; the cancel's words and nothing kept are tested).
4. **The install, each platform's normal way**: Windows runs the new Setup
   (Inno Setup, the same AppId, upgrading in place; the launcher quits
   first); the Mac replaces the whole app (the downloaded DMG attached with
   `hdiutil`, the new app copied beside the old one, swapped in, the old one
   removed only once the new one is in place, the DMG detached); Linux swaps
   the AppImage (`$APPIMAGE`: the new one written beside it, made executable,
   renamed over it). Then the new launcher is started and this one quits.
   Tests: the Mac and Linux swaps on scratch copies (a bundle, an AppImage
   file), a failure part way leaving the old program working; Windows
   compile-checked with the mingw build and played on Owen's PC.
   **Done (2026-11-10):** `upgrades::update::swap_in` (copied beside the old as
   .new, the old moved aside as .old, the new renamed in, the old removed last;
   any failure puts the old back; an earlier try's .new/.old cleared) and
   `sdlfe::install_update`: Windows starts the new Setup and the launcher quits;
   the Mac attaches the DMG (`hdiutil attach -nobrowse -readonly -noautoopen`
   on a mount point of our own), swaps its app in for the running one (found
   from the executable's path; only an .app with Contents/MacOS/mw3launcher,
   the new one too), detaches; Linux swaps the AppImage in for `$APPIMAGE`
   (only a file *.AppImage). A copy not run from its package (the build, a
   dev run) refuses in words ("cannot replace itself: ...use the downloaded
   file"). After an install the new launcher starts on its own
   (`start_detached`) and this one quits; `MW3_UPDATE_TARGET` and
   `MW3_UPDATE_NO_RESTART` for checks. Tests:
   `update_swaps_the_program_in_or_keeps_the_old` (a folder, a file with its
   execute bit, a missing new copy, a missing target, a read-only folder: the
   old kept), `sdl_update_installs_the_mac_app` (a real DMG made by hdiutil:
   swapped, detached, the DMG removed, the settings folder beside unchanged;
   not our app and a DMG without one refused), the launcher test (the build's
   launcher refuses to replace itself). Breaks: any target taken, not
   detached: each caught. Windows' branch compile-checked with mingw; Linux's
   is not compiled on this Mac (its package build is the first), and the
   Windows Setup run waits for Owen's PC.
5. **Owen's hard rule, checked**: the upgrade test (`sdl_program_package_upgrade_keeps_the_players_files`)
   run through the updater's own swap, the settings folders unchanged by
   hash.
   **Done (2026-11-10):** the test's new program folder is built beside and
   put in by `upgrades::update::swap_in`; the 12 player files (MW3's data
   folder through its pointer, PM's settings folder) kept byte for byte,
   every mw3.cfg line kept, both games started from the new programs. Break
   (the new copy never put in: the old programs started again): caught. The
   Mac's own install (`sdl_update_installs_the_mac_app`) checks the settings
   folder beside the app the same way. Left for the release: the Windows
   Setup's upgrade over 1.0.x on Owen's PC (release.md, Windows).
