# The launcher and the first-start setup (ours; design, 2026-10-30)

Not original code: the rebuild's own front door, so a player installs,
starts and plays with no command lines (2026-10-30). The design
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
   size, its 854x480 frame (16:9 since 2026-11-08: it opened 4:3)
   fitted as the menus are (sdl_fit), letterboxed on a 4:3 screen.
3. The install, quietly: mw3.cfg's `install=`, then the finder's places
   (`sdlfe::install_candidates`), each through `setup::check_install`.
   No message box any more: the setup screen replaces the Browse dialog.
4. **The launcher screen** (mockup 1). Entries:
   - **MechWarrior 3**: "Ready, official: data 1.2, program v1.1.0, <folder>"
     (each game's data version and its program's, sitting 5 of Pirate's Moon
     below). The label (2026-11-10: shown, never blocking) is
     "official" when the data is a release's (MW3 1.0, 1.2, or 1.0 with the
     patch's files; PM 1.0) and the program a release build (a bare vX.Y.Z
     tag, PM's pm-vX.Y.Z), else "modified (data, program)" naming which
     (`official_words`; breaks: any v tag taken as a release, the data always
     official: each caught), as "Ready, official: data 1.2,
     program v1.1.0, <folder>" (`versions_words`). Mods are not spoken of
     here (until 2026-11-10 a note "2 mods on" was): a game started from the
     launcher is the original, and mods are played from its MODS screen,
     each campaign with its own (mods-campaigns-plan.md). Or "Not found yet:
     click to find your game" (a click opens the find screen; v1.0,
     2026-11-02: only then, the launcher always opens on the games). A
     folder too long for the plate is
     shortened from the middle by its drawn width ("Ready, official: data 1.2, ...: "
     kept, more of the folder's end; `fit_middle`, text_fit.cpp): a long
     path ran past both sides on the reference PC (lead, 2026-10-31).
     `launcher_long_path_fits_the_button`.
   - **Pirate's Moon**: greyed, "Coming later", while `mw3pm` is not beside
     the launcher; with it, "Ready: <folder>" or "Not found yet: click to find
     your game" (below, Pirate's Moon).
   - **MechWarrior 1** (MW1 fork): when `mw1` is beside ours (below,
     MechWarrior 1), in place of the MechWarrior (1989) entry.
   - **MechWarrior (1989)**: without `mw1`, only when the MW1 port is found
     beside ours (below).
   - **Game files** (2026-11-02, after the rework): the find screen
     at any time, to repick or refresh the game's files (below), and
     **Quit** (for fullscreen; Escape too).
5. MechWarrior 3 chosen: the launcher's textures are freed and the start
   goes on exactly as today in the same window (mounts, data version,
   movies helper or import, `run()`), so the intro movie is the next
   thing seen.

## The find screen and the notes (v1.0, 2026-11-02)

Since launcher split b6 (2026-11-26) this screen, its checks and its notes are
the games' own: built into mw3 (and mw3pm) only, behind `MW3_GAME_FIND_SCREEN`,
until d1 replaces it with the kit's setup screen. mw3launcher reaches a game's
files only through its served `--setup`; for an older program (no `setup: 1`)
it takes mw3.cfg's `install=` / `pm_install=` unchecked, and its GAME FILES and
PLAY with no copy say to update the game (launcher-split.md, b6 piece 1). The
checks below run through `mw3 --launcher-shot F --launcher-screen S`.

**Anything can be picked (2026-11-24).** The pickers had a filter that showed
.bin and .iso first and hid the rest; they now show every file, and PICK A
FOLDER is beside the picker on the find screen, in the list of copies and in
the no-music dialog (the systems' pickers take files or folders, never both).
A file or a folder dropped on the window is taken as a pick on every screen
that asks for one (`dropped`). ADD FROM MY DISC takes a download too (.zip,
.7z, .rar, .tar): its disc image is unpacked into `<media>/setup-staging`,
read, and that folder removed (`disc_pick`).

**NO MUSIC ADDED (2026-11-24, confirm 8).** A pick with no CD audio in it was
shown as an error. It is a question now: PLAY WITHOUT MUSIC (first), PICK
A FILE, PICK A FOLDER, BACK; what else the disc gave (sounds, movies) is kept
and said. Check: `sdl_program_launcher_game_files_pirates_moon`'s end.

**BEFORE YOU PLAY is laid out by measure (2026-11-11, the playtest W3).**
The status line ("No disc in a drive. Pick its image ...") was drawn at a
fixed row and landed on the last lines of a long note (a long settings
folder in the music note; several notes at once). `Launcher::notes_layout`
measures the notes wrapped and takes the first of six forms at which they
and the status line end above the first button: 14 px at 470, 600 or 700
wide (the panel is the frame's whole width there), then 13, 12 and 11 px at
700. The status line goes under the notes; a line's room is kept for it
whenever ADD FROM MY DISC is there, so the notes do not change form when it
speaks. The buttons keep their places (the frame's bottom edge is just
under OK). Nothing is cut: if even the smallest form ran into the button
The log would say "into the buttons". A plate's second line is drawn 8
below its middle and its name 11 above (the art's rule along the bottom ran
through the letters of a 46-high plate; ADD FROM MY DISC is 52 high now),
and ADD FROM MY DISC's own is shorter ("A disc in a drive, or its image or
any file of the disc": the longer one was cut in the middle). The log has
"mw3: launcher: notes: N at size S and width W, the status line at y Y,
...". Test: `sdl_program_launcher_notes_fit_above_the_buttons`, three
cases under a long folder: no movies, no music and an empty soundsH.zbd
(two notes, 14 px); the same with two operations' missions gone (three
notes, 12 px); Pirate's Moon's own notes, its empty soundsH.zbd and no
music (13 px). `--launcher-screen pmnotes` and `pmdisc` (ours, for shots)
draw Pirate's Moon's notes as its start shows them. Break: the first form
alone with the status line at its old row says "into the buttons".

Reached by choosing a game without its files (none found at the start, or
mw3.cfg's gone), or alone on a plain mw3 start with none. At the top, in
the launcher's text, `kFindWords`: "MechWarrior 3 needs your own copy of the
game. Show us your disc, a disc image or download (.iso, .cue, .zip), or a
folder where it's installed. We copy what the game needs into your user
folder; your disc and any installed copy are left untouched."

| plate | shown | what it does |
| --- | --- | --- |
| Find the game | always (one option, 2026-11-02) | the finder (mw3.cfg's, the installer's places, a folder dropped beside the program) and a disc in a drive (`find_discs`) first. Only `<write dir>/install` (SETUP's own copy) or mw3.cfg's is used at once; anything else found is offered, never taken (2026-11-21: the player always chooses): "USE THESE GAME FILES?", one plate naming the find (clicking it opens the picker for other files), its summary, INSTALL / BACK. From GAME FILES the copy in use is no answer: the picker opens, "In use: ...". Choosing again without the launcher: Shift held while the game starts (`held_keys.h`: Windows' key state, the Mac's event flags, X11's keymap; a Wayland-only session has no answer, so `mw3 --setup` there) opens this screen with "In use: ... FIND THE GAME to choose other files, or PLAY to go on", PLAY in QUIT's place. Pirate's Moon the same (2026-11-21): a copy the search finds (not PM's own `<its settings>/install`) is offered on PIRATE'S MOON's click and copied into its settings folder's install; LauncherOptions::pm is mw3pm's own start (on the `pm` branch), the screen as Pirate's Moon's, its choice saved as its own mw3.cfg's install=. With neither, "Not found on this computer or in a disc drive. Pick it: a disc image, a download, or Mech3.exe." and one file picker opens (.iso .cue .bin .zip .7z .rar .., Mech3.exe, any file). `pick_source`: an image or archive is the source; any other file stands for its folder (Mech3.exe or any file of an installed copy, zbd\\ and video\\ up one; a disc's folder), a .bin for its .cue. Then `source::map_source` on the worker ("LOOKING AT YOUR GAME", its bar, CANCEL): anything found (an installed copy on disk too, since 2026-11-21: copied in, so the original may be deleted; only `<write dir>/install` itself is used where it is) INSTALL / BACK under its summary and `setup::install_from_source` into `<write dir>/install`, a cue's music into `<write dir>/music`. SDL3 has no file-or-folder dialog on the three systems (none on the Mac's panel either through SDL 3.4), so a folder is picked by one of its files |
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
are converted in the launcher (a first start, 2026-11-02, sat 44 s on
"Starting MechWarrior 3..." while mw3 did it with only a status line): before
the game starts, and right after ADD FROM MY DISC, any AVI of the install or
the write dir without its .fmv goes through mw3import on the work screen
("CONVERTING THE MOVIES", "Converting the movies (K of N)", the movie's name,
CANCEL; movie_import.h, its "progress K N name" lines). A cancel keeps the
ones done; mw3's own first-run import (`first_run_import`, its small progress
window) stays the fallback for a start without the launcher. None on Windows,
nor with --no-import (passed on to the game) or MW3_NO_IMPORT; MW3_IMPORTER
names a stand-in for the checks. `sdl_program_launcher_converts_the_movies_before_the_game`.

The file picker (very slow to open on macOS 26): SDL shows NSOpenPanel
as a sheet on the launcher's window, and the panel is another process that
answers through the main run loop, which the launcher's vsync frame loop
served once a frame. While a picker is up the launcher draws nothing and
waits in SDL_WaitEventTimeout (the run loop serving the panel at once). The
log times it: "the file picker asked for", "the picker took the focus N ms
after", "the picker answered (...) N ms after".

The picker's wait (measured 2026-11-10, playtest-1.1.0 item 2). The call that
opens the panel (`SDL_ShowOpenFileDialog`) blocks the main thread until the
system's panel service answers; the sheet is on screen 3 to 20 ms after it
returns. On the Mac, a fresh process each row, the panel cancelled by code:

| Start folder | First picker | Second, same process |
|---|---|---|
| none (the system's last one) | 967 ms | 528 ms |
| Downloads (26 items, 9.7 GB, two disc zips) | 945 ms | 530 ms |
| an empty folder | 967 ms | 520 ms |
| none, fullscreen window | 948 ms | 249 ms |

A plain NSOpenPanel without SDL gave the same (905 to 1009 ms, then 497 to
560 ms; the same for the Desktop and /Volumes). So the start folder and what
is in it make no difference, and the picker is opened without one, as before:
the system reopens the player's last folder. The 9.5 s once seen ("took the
focus 9531 ms after") was the system's panel service starting cold for the
app: in the system log the launcher's main thread waits 8.4 s on it while the
service sets up iCloud Drive, Finder extensions and the sidebar's volumes,
then the panel is blank for 5 s more while the service fills it. None of it
is ours to shorten, and it did not happen again once the service was warm.

What was ours: the ask and the blocking call were made in the click's own
step, before the screen with its "Pick it" line was drawn, so the launcher sat
frozen on the old screen. Now the ask waits one frame (`ask_pending_`): the
launcher draws the screen with "Opening the file picker..." in a box at the
bottom, presents it, then calls (`open_picker`). The log adds "the picker's
notice shown" and "the picker's call took N ms". For the checks
`MW3_LAUNCHER_PICKER=cancel` answers the ask as cancelled with no window and
`MW3_LAUNCHER_PICKER_SHOT` saves the frame on screen at the ask
(`sdl_program_launcher_says_the_picker_is_opening_before_it_asks`).

GAME FILES (on the game's page since 2026-11-24) opens the same screen with the install in use ("Using
MechWarrior 3 1.2 at <folder>. No music.") and, while it lacks movies or
music, ADD FROM MY DISC (back to this screen after). A new pick goes back
to the games, not into the game, its folder saved to mw3.cfg. A disc or
archive install into `<write dir>/install` is built in `install.new` and
swapped in only once complete (`swap_in_folder`: the old copy set aside as
`install.old`, removed after the rename, put back if it fails), so a cancel
or a failure leaves the old one working and no second copy stays. An installed
copy picked is copied in over the launcher's own earlier copy (
2026-11-21: once found, the original may be deleted). An older mw3.cfg
pointing at a copy elsewhere still works; re-picking moves it in.
`sdl_program_launcher_game_files_repicks`.

Keys and a pad (`nav`): the arrows, Tab or the d-pad step through the
plates (QUIT included), Enter, Space, A or Start press,
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

## How it is drawn (RmlUi, 2026-11-24)

Until 1.4.1 the launcher painted an 854x480 16-bit canvas itself and the window
showed it enlarged. Now its screens are a document (RmlUi 6.3, `third_party/rmlui`,
MIT, vendored) drawn at the window's own pixels, so text and edges are sharp at any
size and a larger window shows the same layout larger.

- The layout is `assets/launcher/ui/launcher.rml` and `launcher.rcss` (HTML and
  CSS's relatives): every screen of the launcher in one document, shown by its
  name. A release needs both files beside the pictures and fonts.
- The launcher (`launcher.cpp`) keeps its logic, its button names and its log. Each
  frame it fills a `UiModel` (`launcher_ui.h`: the words, the status line, the
  panels and rows, the buttons by group) and the document shows it;
  `<x buttons="group">` is filled with that group's buttons. `LauncherUi`
  (`launcher_ui.cpp`) is the only code that knows RmlUi.
- Drawn through SDL_Renderer (its own render interface; no GPU API of its own),
  with FreeType, the one SDL3_ttf is built with, linked into the programs.
  `mw3` holds the launcher's setup screens, so it links both too.
- Lengths are dp: pixels on the 854x480 canvas, scaled by the window.
- Text that must fit (`<fit>`) goes smaller down to its least size, then is cut in
  the middle; the notes before playing measure themselves (`notes_*`).
- The stars scroll to the left; a hovered button lights its edge; no ripple.
  Every button is the same flat kind (the steel plate is gone).
- Drawn while the window's edge is dragged; the window has no least size.
- Sounds: with an install, the main menu's rollover and select sounds through zSnd
  from the install; without one, silent (no sounds of ours).
- Mouse, keyboard (arrows, Enter, Escape) and a pad's d-pad and A/B.
- Checks: `--launcher-shot` draws without a window (a software renderer);
  `MW3_LAUNCHER_SHOT_SIZE=1708x960` at that size; `MW3_LAUNCHER_MOUSE` plays mouse
  steps ("move 712,130;click 712,130;wheel -2;down 800,120;up 800,300").

## Pirate's Moon (2026-11-06)

`mw3pm` is found in the launcher's own folder (`own_dir`, as `mw3`: a bundle's
Contents/MacOS, the AppImage's usr/bin; `MW3_PM_BIN` for checks), the final
package's layout. It is a separate game: it gets `--install <folder>
--no-launcher`, `--write-dir` with its own folder as the launcher knows it
(`MW3_PM_WRITE_DIR`, a MY DATA FOLDER move, else the usual one or where its
pointer leads; until 2026-11-11 it got none and fell back to the usual folder,
a named one ignored), the window and sound choices, and `--ready-file`, never
MW3's install or write dir; its settings and saves are its own. Its folder is
`pm_install=` in the launcher's mw3.cfg, checked at start with `map_source`
(a Pirate's Moon install, in a plain folder). A click on the entry starts it,
or, with no folder yet, asks for one. GAME FILES acts on its page's game (until 2026-11-24 the game last clicked):
for Pirate's Moon the same folder picker, which also keeps MY DATA FOLDER, ADD
FROM MY DISC and SETUP out of it (later items). Installing from a disc and its
music (the image's track 2) are later too.

### Pirate's Moon as MechWarrior 3 (plan, 2026-11-09; toward the combined 1.1.0)

The launcher does for Pirate's Moon everything it does for
MechWarrior 3. the rules for all of it: open and easy (players may replace
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
   too; an installed PM copy on disk copied in (2026-11-21). The folder it goes to
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
   uses PM's own settings folder: "BattleTech/Pirate's Moon" beside MW3's
   usual one, as SDL_GetPrefPath makes it for mw3pm (a player's earlier
   "MicroProse/Pirate's Moon" moved across once, program.md, The family
   folder), or where its pointer leads.
   - FIND THE GAME searches as at the start (sitting 1, plus its own
     `install\`), then a disc in a drive, then the picker. A pick is
     installed into `<PM settings>\install`, its music into `music\`. An
     installed copy is copied in too (2026-11-21). MechWarrior 3 picked there is said
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

## The panels (2026-11-24)

The main screen as a list, the download page's look (owenmorgan.com/battletech;
the mockup "BattleTech launcher: scrolling list", left board, picked on
2026-11-23): one panel a game, top to bottom MECHWARRIOR 3, PIRATE'S MOON,
MECHWARRIOR 1: REVERSE ENGINEERED, MECHWARRIOR 1 REMAKE, THE CRESCENT HAWK'S
INCEPTION. Inception is one panel until its remake has a release (or is
installed); then INCEPTION REMAKE gets its own and the first panel is the
original. MW1's first panel names MechWarrior 1 Rebuild (mech1-re) by its full
name, its blurb without the port it began from (2026-11-25: the game is no longer
that port; mech1-re credits it). The cards' tabs are gone (an old `tab_<card>=` in mw3.cfg is ignored).

- A panel (752x100 on the 854x480 canvas): the badge (76) at the left, dimmed
  while the game is not out; the name (the lettering, 20); then three small
  lines in the second font (`font_body.ttf`, Saira Condensed Medium, 12): the
  status's first line in amber (its second joined with " · " when both fit),
  the rest of the status, then a short line about the game only when it fits
  whole (2026-11-24: "there's no way to read the full description"; a status
  of three lines leaves no room, and it is left out). Its button at the right,
  upright in the middle: PLAY (split with its UPDATE when one is ready),
  DOWNLOAD, COMING SOON, with its gear to the left (A game's page). The panel whose button is
  hot (mouse, keys, pad) gets an amber, doubled edge.
- Inception has a badge (`badge_inception.png`, the hawk; README there).
- The list scrolls between the header and the bottom row (y 80 to 418): about
  three panels show. The mouse wheel scrolls (80 px a notch, smoothly; a
  trackpad's parts of one too); the scrollbar at the list's right (an amber thumb on a dark track)
  drags, and a click on its track pages. The keys and the pad scroll a focused
  panel's button into the list. A shade at the list's top and bottom shows more
  is there. Buttons are hit only inside the list.
- The log names each panel's button by its game, then the bottom row, then
  UPDATE AVAILABLE: "screen main: MECHWARRIOR 3, MECHWARRIOR 3 SETTINGS,
  PIRATE'S MOON, PIRATE'S MOON SETTINGS, ..., CHECK FOR UPDATES, QUIT" (a gear is "<GAME> SETTINGS"). `MW3_LAUNCHER_LIST_Y=<px>` starts the list scrolled (shots).
- Sections (2026-11-27): a game's optional `"section"` in the catalog is a
  heading over the list (the title font, amber, a rule under it), drawn above
  a panel whose section differs from the panel above it, so a hidden first
  game never takes its section's heading with it. The words are data: renamed
  or moved by a catalog revision, no launcher release. Launchers before 1.9.1
  ignore the field (a plain list). The log says "section <TITLE>".
- Sounds (2026-11-27): hover.wav and click.wav play through one logical
  device on the default output, each its own stream bound to it. Before, each
  opened its own (`SDL_OpenAudioDeviceStream` twice), and SDL 3.4.12 broke its
  list of logical devices when the default output changed with more than one
  on it (fixed upstream in 3.4.14, 5e53f67155): SDL_Quit then crashed at the
  launcher's exit (a 1.8.0 report, after a two-hour session). The games open
  two as well (music, effects); the SDL 3.4.20 move covers them.
- Its log (run_log.cpp) sets stdout and stderr unbuffered once they are the
  pipe: buffered, a crash lost the lines still waiting (the report above kept
  only the crash handler's).

The two sections below are the layouts before it; the game states, the COMING
SOON rule and the kit's contract are unchanged.

## The games catalog (2026-11-26, launcher-split.md b0)

The families and the games are data: `src/upgrades/update/games.json`,
built into the launcher as text (CMake writes `games_catalog.cpp` from it)
and read once by `parse_catalog` (games.h). It replaced games.cpp's table
(key, name, repo, tag prefix, program), the release files' names
(`assets`, `{tag}` for the tag) and launcher.cpp's panel rows, badges,
blurbs, buttons' names and screen headers. Each game has four names:
`name` for the words ("Pirate's Moon"), `title` on its panel, `heading`
over its screens ("MECHWARRIOR 3: PIRATE'S MOON") and `button`, its panel
button's name in the log and for a pad. `heading_badge` is the badge a
game's own setup shows when it differs from its panel's (none does: the MW1
Rebuild's setup showed the Remake's `badge_mw1.png` until 2026-11-26, now its
panel's `badge_mw1o.png`). The panels go
by `order`; `panel_before_release: false` keeps a panel away until a
release is found or the game is installed (Inception Remake). `state`,
`needs` and `min_launcher` are read and checked, and acted on since the
fetched catalog (b7, below; `needs` by DOWNLOAD since 2.0.1, Games on demand, A game's needs). The code still knows each game by its
`Game` (games.h), linked to its entry by key; a built-in catalog without
one of them stops the program at its first read (the test stops it
first).

Checked: `games_catalog_built_in_as_the_tables_were` (every value as the
tables had it, the files' names on every platform) and
`games_catalog_refuses_bad_entries` (mw3tests); every `--launcher-shot`
screen, games on demand and an empty folder, byte for byte against the
build before (66 shots, the version text pinned).

### Fetched (2026-11-26, launcher-split.md b7)

A newer catalog comes from the addresses sources.json lists (Where the
launcher looks, below: reverse-engineered.com, then the hub's main on
GitHub and the mirror), each asked and checked against its signature, the
higher `revision` winning, with no word about the player sent. It is
fetched on a thread while the launcher is open and cached in the write
dir, `catalog/games.json`, with the art it names that the package does not
carry (`catalog/art/<name>` beside it on the hub; kept only when it is a
PNG). A start reads the cache and merges it over the built-in catalog:
an entry by its key replaces the built-in's, a new key adds a game, and
anything it does not list stays as built in. Offline, the cache serves;
with no cache, or one whose `revision` is not above the built-in's, the
built-in catalog is used. A check run (hidden, a shot) fetches first, and
only when `MW3_CATALOG_URL` (one address, or `A|B`; `file://` there) or
`MW3_SOURCES_FILE` is set, and never from an address that is not file://.

**A catalog fetched while open** (2026-11-27): every fetch is put in use as
soon as it lands and checks (signed, newer than the one in use), never one
start later. The window never waits on the network: the cached or built-in
list is on screen at once and the new one is swapped in when it arrives.
The fetches: the start's (on its thread), CHECK FOR UPDATES (the list
fresh too), the end of a game's install or update, every 4 hours while
open, and the start after the launcher's own update (an ordinary start, so
the first of these). A newer one waits while an install, an update or a
setup is running, while the games' releases are being asked, or while
another screen is open (`catalog: a newer one waits: ...`), then is merged
over the built-in as at a start (`catalog::apply_newer`,
`u::replace_catalog`): every game keeps its place in the launcher's tables
(a thread may hold one), a game the new catalog lacks keeps its last entry
(catalogs never drop one), new games follow; the catalog replaced is never
freed. The new games are looked up, every game's releases asked again, the
cards laid out again, their art read from the cache the fetch filled. Said
as `catalog: revision N in use (fetched while open, G games)`. Checks:
`MW3_CATALOG_START=late` leaves a check run's start fetch to the
launcher's thread (as a player's), `none` makes none;
`MW3_CATALOG_EVERY_S` sets the interval; the test
`sdl_program_launcher_takes_a_catalog_while_open` covers the start, an
install, CHECK FOR UPDATES and a real self-update's relaunch.

**A live check** (`MW3_LIVE_CHECK=1`, 2026-11-27) lifts that one rule, so
a release is checked without a window: a check run then asks the real
addresses, as a player's start does: the catalog's (https only, its `.sig`
checked; "catalog: a live check: the real addresses asked"), each game's
releases and the launcher's own, on every home. Every other check-run rule
holds: only folders it is given (its own games folder), no picker, no
trash, `MW3_SOURCES_FILE` still read. Run it with a scratch write dir:

    MW3_LIVE_CHECK=1 MW3_LAUNCHER_FRAMES=600 mw3launcher --hidden --nosound --write-dir <scratch>

and read the log: each catalog address "answered, signed", the revision in
use, each game's "at <home>: answered", and the update check's verdict.
Test: `sdl_program_launcher_live_check_asks_the_real_addresses` (every
real address there is `https://127.0.0.1/`, refused at once: no network).

What an entry does:

- `released`: its repo is asked and its panel shown, as before.
- `coming_soon`: its repo is still asked, and a release found turns it to
  DOWNLOAD; with `panel_before_release: false` it has no panel until then.
  Flipped to `released`, its panel shows at once (COMING SOON until its
  repo has a release).
- `removed`: no panel and its repo not asked, unless it is on this
  computer: then it stays, plays and updates as before. No catalog change
  touches an installed game.
- A new game (up to `kMaxGames`, 10 in all) is a game on the setup kit's
  contract: DOWNLOAD from its repo (still checked against SHA256SUMS),
  then PLAY, its page and its links as the kit's games have them.
- Deleted by mistake (2026-11-26): a game only the fetched catalog has
  (not built in) that is installed or linked here keeps its last-known
  entry. At each start, and at its install or link (`catalog::keep`), the
  fetched catalog in use is copied to `<write dir>/catalog/kept/<key>.json`
  for each such game; a catalog in use without that key (a later fetch, or
  a newer built-in one) gets the entry back, its family too when missing,
  logged `catalog: <key> kept: installed here` (or `linked here`). It then
  plays and updates as before. Once the game is neither installed (its
  folder's `rebuild-game.txt`, in the games folder or the folder its
  INSTALL picked) nor linked (`link_<key>=` to a program still there), the
  kept file is removed at the next start (`catalog: <key> forgotten`). An
  entry set to `removed` is in the catalog, so it is not replaced.

An entry's `"original"` (2026-11-26) lists where the original game's
files can be fetched from, the same file at each address, tried in
order: up to `kMaxOriginals` (4) of `{"url", "sha256", "size", "name",
"note"}`, `url` https, `sha256` 64 hex digits, `size` in bytes, `name`
a plain file name. It is read and checked (a bad one is left out, said
in the log, and its game kept). The start's log line,
`catalog: <key> names an original download`, only means it is
listed: what fetches it is DOWNLOAD (Games on demand, The original game's
files). file:/// is taken only in checks (`MW3_GAMES_API` set). No
catalog lists an original source today, and without one nothing is ever
fetched. Launchers 1.6.0 to 1.7.1 ignore the field. Checked:
`games_catalog_original_read_and_unused`.

An entry's `"compact"` (2026-11-27) lists the same files made small
(mech3-re's docs/compact.md: a .tar.xz the game's own setup takes and
checks against the manifest inside it), with the same fields and checks
plus an optional `game_min`, a dotted version ("1.7.0"). An entry may have
`"compact"` without `"original"`. DOWNLOAD tries every original first,
then each compact whose `game_min` the game's own version (its status's,
"v1.7.0", "pm-v1.2.12", a build's tail read past) reaches; one with a
`game_min` is skipped when the version is unknown or older
(`upgrades::update::original_sources`). Launchers before 1.9.0 never read
the key. Checked: `games_catalog_compact_and_game_min`.

Trust: an entry whose `repo` names another owner (`owner/repo`) is left
out, so is one whose `min_launcher` is newer than this launcher (a build's
"unknown" version takes every entry), and one whose art is not a plain
`.png` name. A bad entry is left out and the rest read; a catalog that is
not JSON, or in a newer format (`"catalog"` above 1), is not used at all.
Each is said in the log (`catalog: ... left out: ...`). A catalog is taken
only when signed (Where the launcher looks, below).

Checked: `games_catalog_fetched_leaves_out_bad_entries`,
`games_catalog_fetched_merges_over_the_built_in` (mw3tests) and
`sdl_program_launcher_takes_a_fetched_catalog` (a stand-in adds a game with
its badge, flips Inception's remake to released, removes MechWarrior 1
Rebuild and an installed Inception, which still plays; offline with the
cache; offline with none), `sdl_program_launcher_keeps_an_installed_games_entry`
(revision 2 adds a game, installed; revision 3 leaves it out: kept, and it
plays; uninstalled: forgotten). `tools/launcher/shots.sh` takes every screen for
the byte-for-byte check: the built-in catalog's 66 shots are as before.

## One bar for a press (2026-11-27)

Everything one press starts shows one progress line and one bar, with a
total: a card's DOWNLOAD (the game's package, then its original files when
the catalog lists them and the game was not installed, then their setup),
an UPDATE, UPDATE ALL (each game's update, the launcher's last) and the
launcher's own update. `launcher_job.h` holds it (`job::Job`, time passed
in, so the checks run it on a fake clock); the launcher feeds it each frame
from the work under way (`job_tick`): the download's bytes (curl's `.part`,
through `DownloadFn` and the games' `Meter`), the words past them, and the
setup's own fractions.

- The total: the release asset's `size` from the releases list, plus the
  catalog's `size` of the original's first source. A part whose game turns
  out to have its files (its `--status` after the install) is left out,
  the reason on the smaller line, the total smaller; the bar may jump
  forward. A setup has the last 15% of the bar, by its own fractions.
- The line: `MechWarrior 3: 287 of 668 MB · 4.1 MB/s · about 2 min left`;
  `starting...` until the bytes have come for 2 s (the speed is taken over
  the last 5 s, afresh at each part); `waiting for the server...` after
  10 s without a byte, before curl's minute ends it; MB only with no size
  known; past the bytes, the step's words (`Checking the download`, the
  setup's stages).
- The smaller line: the part, `the game (43 MB)`, then `the original game
  files from the Internet Archive (625 MB)` (the source's `note`, else its
  host), then `setting up`.
- CANCEL stops the whole press: parts installed stay, the partial file is
  removed, UPDATE ALL's queue is dropped.
- The log: `job: <name>: <parts> (<total> in all)` at the press, a line as
  the part changes and once a second, `job: <key> left out: <why>`, and
  `job: ended at <in> of <total>: ...`. The SHA256SUMS fetched first (a few
  hundred bytes) is not counted.
- Checks: `MW3_LAUNCHER_CANCEL_AT=<part key>` presses CANCEL once that
  part's bytes come; the test serves its files through FIFOs at a set pace
  (curl's `--limit-rate` does not slow `file://`).
  `launcher_job_one_bar_for_a_press` (the model) and
  `sdl_program_launcher_shows_one_bar_for_a_download` (two files one
  rising total, a status with files, a CANCEL in part 2).

## Where the launcher looks, and signed data (2026-11-26)

Nothing the launcher fetches from is fixed in its code. Three files say
where to look, each trusted for what it holds:

- **sources.json** (`assets/launcher/sources.json`, shipped beside the
  badges): `{"sources": 1, "catalog": [...], "keys": [...],
  "unsigned_sums": [...]}`. `catalog` is the catalog's addresses in order
  (reverse-engineered.com's `/launcher/catalog.json`, the hub's main on
  GitHub, the mirror's copy; up to 6, https and a plain host), `keys` the
  public Ed25519 keys (64 hex digits, up to 8) that sign the catalog and
  the releases' sums, and `unsigned_sums` the releases lists (address
  prefixes) whose unsigned `SHA256SUMS` are still taken: GitHub's. It is
  the source of truth: the build embeds the same file
  (`sources_builtin.cpp`, from CMake), used only when the shipped one is
  missing or does not read (`sources: the built-in copy (<path>: <why>)`
  in the log; a build whose embedded copy does not read stops at the
  start). CMake warns while `keys` is empty: such a build takes no fetched
  catalog (`catalog: sources.json lists no key`), and its built-in catalog
  is used. A check run reads `MW3_SOURCES_FILE` instead (file:// taken
  there). `upgrades/update/sources.h`.
- **The catalog** names everything else: `homes` (below, Two homes) for
  every repo's releases, the games and their `original`, and
  `catalog_sources`, its own addresses. The launcher keeps the last good
  catalog (the cache); the next start asks the cached catalog's
  `catalog_sources` first, then sources.json's, each once
  (`sdlfe::catalog::addresses`), so the catalog can move without a
  launcher release. Each is asked; the newest signed one is cached with
  its `.sig` (the first on a tie). The start says them:
  `catalog: asked at ...`.
- **Signatures.** Ed25519 (Monocypher 4.0.2, in re-setup-kit: `rekit/sign.h`;
  the launcher uses the kit's copy). A signature is a detached `<file>.sig`,
  one line of 128 hex digits per key, each over the whole file; several
  lines let the old key and the next both sign while keys move. A fetched
  catalog is taken only if a line checks against a key the launcher knows;
  otherwise it is logged (`catalog at <address>: not used: no signature`, or
  `its signature does not check against the launcher's keys`) and the next
  address asked. A signed catalog may name `next_keys` (up to 4): they are
  learned (kept in `<write dir>/catalog/keys.txt`, read at each start), so
  a later catalog signed only by the next key is taken (key rotation; a
  launcher that never saw the vouching catalog needs its sources.json to
  list the new key). A signed catalog may also name `revoked_keys` (up to 8,
  shipped or learned): once it is taken they never verify again, are
  dropped from the learned keys and never learned again, and are kept in
  keys.txt as `revoked <key>`, so a leaked key cannot mint itself
  successors through `next_keys`. Sign a revoking catalog with a key that
  stays. The cache counts only while its stored `games.json.sig` checks
  against the keys in use: a cache an older launcher fetched unsigned
  (1.7.1 and before, same write dir), or one signed by a key since
  revoked, is not used at the start (`catalog: the cache is not signed: not
  used`, the built-in catalog then), its `catalog_sources` are not asked,
  and a signed catalog of any revision replaces it.
- **Release sums.** A release may carry `SHA256SUMS.sig` beside
  `SHA256SUMS`. Signed sums are taken from any home; unsigned ones only
  from a list sources.json names in `unsigned_sums` (GitHub's), so a
  mirror or another home can serve a file only when its sums are signed,
  or when GitHub's line vouches for it (`sdlfe::download_checked`). A
  refusal is said: `The release's SHA256SUMS could not be trusted (its
  SHA256SUMS is not signed), so nothing was downloaded.`
- **Art.** The badges are fetched beside the catalog (`art/<name>`), so
  the signed catalog vouches for them: each game's `badge_sha256` (and
  `heading_badge_sha256`, `card_sha256`; 64 hex digits, a bad one leaving
  the entry out) is the art's SHA-256. A fetched PNG is kept only when it
  matches (`catalog: art <name> not kept: not the catalog's sha256`), and
  art with no sum is never fetched (the carried file, or none). At each
  start the cached art the catalog in use names is checked the same way
  and removed when it is not its sum, or has none (art an older launcher
  fetched unchecked into the same write dir). Families carry no art.
  Launchers 1.7.1 and older ignore the fields. `publish_catalog.py` writes
  the sums from `catalog/art` before signing and refuses a named art that
  is not there.
- **Fresh copies.** The catalog and its `.sig` are asked with
  `Cache-Control: no-cache` and `Pragma: no-cache` (`curl_get`'s `fresh`;
  the kit's `fetch_url` the same), so a proxy between serves no stale copy:
  the site's server marks its static files good for years.
- **Signing** (`tools/package/sign.py`, Python's `cryptography`): the
  private key never goes in a repo; `MW3_SIGNING_KEY` names its file
  (several, `:`-separated, sign with each). `sign.py <file>` writes
  `<file>.sig`, `--public` prints the public keys for sources.json,
  `--new-key <path>` makes one, `--check <file> <key>` checks.
  `sums.py` signs `SHA256SUMS` when the variable is set;
  `publish_catalog.py <folder>` signs `catalog/games.json` (its `.sig`
  committed beside it on the hub's main) and stages `catalog.json`,
  `catalog.json.sig` and `art/` for the site's `/launcher/` path, served
  as plain files (DEPLOY.md).

Checked: `sources_read_shipped_and_built_in`,
`games_catalog_sources_and_next_keys` (mw3tests),
`sdl_homes_lists_and_catalog_addresses`, `sdl_update_download_takes_signed_sums`
(signed sums from an untrusted home taken, unsigned and another key's
refused there, GitHub's unsigned taken, another home's signed line vouching
for GitHub's file), `sdl_program_launcher_takes_only_a_signed_catalog` (the
order with each address failing in turn, the newest winning, unsigned,
another key's, a changed text, a bad .sig, a key learned and used, refused
in a fresh write dir, signed by both; `catalog_sources` asked first on the
next start; 1.7.1's unsigned cache at revision 3 and 5 not used, then
replaced by a signed revision 3; the shipped key revoked, a catalog by it
refused and its successor not learned, a learned key revoked, its successor
taken; sources.json missing: the built-in copy, nothing taken, its real
addresses not asked) and `sdl_program_launcher_checks_for_updates` (an
unsigned newest on the backup: shown, not downloaded). Tests not about
signing have their catalogs signed by test key 1 (`spawn_launcher`); test
keys come from the kit's `tests/test_keys.h`.

## The launcher's own small copies (2026-11-26, launcher-split.md b1)

What the launcher took from the game's code for its own use is its own now, in `launcher_own.h` (`sdlfe::own`):
the Work types (`Progress`, `ProgressFn`, `Result`, were `mech3::setup`'s), mw3.cfg's line editor
(`save_config_value`, `save_install`), `crc32` (the notes' seen-once check), `any_case` and `path_from_config` (with
`narrow_paths_utf8`). Each is a copy that does what the game's does. MW3's setup calls the launcher still makes
(`check_install`, `start_check`, `install_from_source`, `add_from_disc`, `add_pm_from_disc`, `apply_patch`,
`find_discs`) go through `launcher_mw3_setup.h`, which turns their types into the launcher's; it is the one place
that names `mech3::setup`, and it goes when b2 and b3 put those calls behind the games' served verbs.

Checked: `launcher_own_copies_do_as_the_games` (mw3sdltests: mw3.cfg byte for byte against sdl_install's on the
same files, crc32 against zutl_zar's, any_case against zutl's on one tree, path_from_config); every
`--launcher-shot` screen byte for byte as before (`tools/launcher/shots.sh`, 66 shots).

## The copies on this computer (2026-11-24)

"The user should be able to pick what's used. Some copies have music and some
don't ... Show if anything is missing." GAME FILES, then FIND THE GAME, with a
copy in use (or Shift held at a game's start) lists every copy of the game the
launcher finds (`find_copies`): the game's own folder's `install`, mw3.cfg's
(`install=`, `pm_install=`), the user folder's `install`, the installer's
places and a drop-in, each once, the one in use first. A row a copy: its
folder; under it IN USE, the game and its data's version, and what the game
would lack with it, or "nothing missing":

- MechWarrior 3: music, movies, the 1.2 patch (data 1.0).
- Pirate's Moon: music, full-quality sounds (a soundsH.zbd without them).

Music and movies the game already has outside the copy (the game folder's,
the user folder's) count for every copy. A row pressed: "Use these game
files?" as any find, then copied into the game's folder whole, its music
and movies with it. The row in use is greyed. PICK A FILE opens the one
picker (a disc image, a download, Mech3.exe); BACK returns. Up to four rows.
The log: "copy <folder>: <version>[, in use], missing <a> and <b>" (or
"nothing missing") and "screen copies: IN USE, COPY 2, PICK A FILE, BACK".
`--launcher-screen copies` / `pmcopies` draws it (checks:
`sdl_program_launcher_lists_the_copies`). MechWarrior (1989) keeps its own
picker: nothing of it can be missing that way.

## Copies installed without the launcher (2026-11-24)

"If somebody downloads mech3-re and installs independently of the launcher
... the launcher should automatically pick it up if it's one of ours." A
game's program records where it is: each start of a released build writes
`rebuild-program.txt` (game=, program=, version=) into its user folder
(`sdlfe::games::record_self`; mw3 into BattleTech/MechWarrior 3, mw3pm into
BattleTech/Pirate's Moon, MechWarrior 1 Remake from its 1.1.0 into
BattleTech/MechWarrior 1 Remake, read by path only; the kit's games when their
builds do it). The
program is the file a player starts: `$APPIMAGE` for an AppImage, never a
macOS App Translocation path. A development build (v1.3.1-29-g...) writes
nothing. Nor does a copy the launcher installed (`launchers_own`: its
rebuild-game.txt above it), so it is never linked as one installed without it.

At the start, a game with nothing beside the launcher looks for such a copy
(`find_link`, `sdlfe::games::found_elsewhere`): its record, Windows' uninstall
entry for its Setup (the AppId's InstallLocation; MW3, Pirate's Moon, MW1
Remake, MW1 Rebuild and Inception), and
the Mac's /Applications and ~/Applications. Each is checked before it is
trusted (`identify`): the file is there and its stamp
(MW3-PROGRAM-VERSION) is this game's release, or, for an AppImage (its stamp
compressed) and the kit's games, its `--status` names the game. (MW3 alone's
AppImage starts mw3launcher: `--status`, `--setup` and `--ready-file` go to the
mw3 beside it.) The newest is
linked (mw3.cfg `link_<key>=`) and logged "games: pm: found <program> (its
record), linked". A check run (hidden, a shot) reads the record in its own
folder only, never the computer's Applications or registry.

PLAY starts the launcher's own copy or the linked one, whichever is newer
(the launcher's on a tie). UPDATE on a linked copy installs the new version
into the launcher's folder, which then plays; the player's own install is
never written to (the launcher only writes folders it made). The log:
"games: pm: linked pm-v1.2.6 at <program>".

## A game's page (2026-11-24)

"There should be a gear that pops up an option to link an existing install
... they should have an uninstall option for every game." Every panel with a
game (not COMING SOON) has a gear, a square left of its button
(`Button::gear`, `kGear + game`), after PLAY and UPDATE in the focus order so
a pad's first press is still PLAY; the log calls it "<GAME> SETTINGS". It
opens the game's page (`kGamePage`, headed with the game's name): a row for
each thing, what it is and where, its buttons at its right, then two small
buttons and BACK. (Until the evening of 2026-11-24 the page was a column of
plates, and GAME FILES and ADD FROM FILE were on the games' bottom row.)

- GAME FILES (`files_row`): the player's own copy in use and what it lacks;
  CHANGE (logged GAME FILES) opens its find screen, whose BACK comes back
  here (`open_files`, `files_page_`). MechWarrior 3's is there whether or not
  the game is installed (Pirate's Moon's and MechWarrior 1 Remake's need
  theirs); MechWarrior (1989)'s opens the picker. A kit game that does not
  serve its setup shows the row alone: PLAY asks. One that does: with files
  in use it has PICK A FILE and PICK A FOLDER and no FIND THE GAME, as the
  search finds the copy in use, which is no answer.
- INSTALLED GAME: the copy PLAY starts (the launcher's or the linked one,
  the newer; the launcher's on a tie), its version and folder, or Not
  installed with DOWNLOAD. OTHER COPY: the other, when both are here.
- UNINSTALL, for the launcher's copy: a confirm with UNINSTALL (the game
  only) and, when allowed, UNINSTALL AND REMOVE GAME FILES; KEEP IT (and
  Escape) goes back. The folder goes only when it carries the launcher's
  record (`sdlfe::games::uninstall`, moved aside first so a running game is
  refused whole). The game files go only for MechWarrior 3 and Pirate's
  Moon, only when the copy going is the last one (none unlinked but still
  there, none beside, no MechWarrior 1 Remake, which plays them), and only a
  folder our copying made (its mw3-install.txt) in the user folder's or the
  game's folder's `install` (`own_files`): never a copy of the original the
  player pointed at. Pilots, saves, settings and mods always stay.
- A linked copy has UNLINK (forgotten, `unlinked_<key>=`, so it is not linked
  again by itself; nothing is deleted) and UNINSTALL (logged UNINSTALL
  LINKED), with the same confirm (`removal_of`, `remove_linked`): a Mac .app
  or a Linux AppImage is moved to the trash whole; on Windows its own Setup's
  uninstaller (unins*.exe with its .dat beside the program) runs silently,
  Windows asking for an administrator where its Setup did. None for a
  program loose in a folder (what else is in there is not ours to say), one
  in a package with another game of ours or with this launcher; the row says
  so. Asked again at the removal; the answer is whether the program is gone.
- LINK A COPY ALREADY INSTALLED (LINK ANOTHER COPY): the picker; the game's
  program in what was picked (the program, its folder, a .app, an AppImage;
  `program_of_pick`), checked as above, a development build allowed. Not the
  game: for one that serves its setup the pick is looked at as its game files
  instead (a .zip of the game was picked here), else said.
- INSTALL FROM A DOWNLOADED FILE: a game's own download (any game's: the file
  says which), as ADD FROM FILE did.

`--launcher-screen game:<key>` and `game:<key>:uninstall` draw it. Checks:
`sdl_program_launcher_links_and_uninstalls` (found by its record, a
development build not linked, a wrong pick, LINK, UNLINK, every uninstall and
`removal_of`'s answers; the game files there are links to game/pm, which
stays whole). `MW3_LAUNCHER_TRASH=<dir>` is the checks' trash.

## The family cards (2026-11-22, replaced by the panels 2026-11-24)

2026-11-22, from four mockups (the canvas "Launcher Families"): option
A, "everything up front and center". One card a family, a tab a game in it:

- **MECHWARRIOR 3**: MECHWARRIOR 3 | PIRATE'S MOON.
- **MECHWARRIOR 1**: ORIGINAL (MechWarrior 1 Rebuild, mech1-re; Elthial's
  port instead when a package carries it) | REMAKE (MechWarrior 1 Remake).
- **INCEPTION**: ORIGINAL (Inception Rebuild, inception-re) | REMAKE
  (Inception Remake, inception-remake-re).

Originals on the left. The first tabs are MW3, MW1's REMAKE and Inception's
ORIGINAL; the last pick is kept (`mw3.cfg` `tab_mw3=`, `tab_mw1=`,
`tab_inception=`, the game's key). The badge follows the tab: MW3's, Pirate's
Moon's, the remake's, and `badge_mw1o.png` for MW1's original (a pixel-art
'Mech on green phosphor, `assets/launcher/README.md`); Inception has none
yet (our own art only): a plain disc. Under the tabs, the selected game's
status lines, then its button: PLAY (split with its UPDATE when one is
ready), DOWNLOAD, or COMING SOON. A tab whose game has an update carries a
small mark.

**COMING SOON by itself** ("as soon as the releases are on the github,
it switches to play"): every game is asked of its repo's releases. A repo with
no release (`GameCheck::kNoRelease`) greys that game's button as COMING SOON;
once a release is out the same launcher shows DOWNLOAD, then PLAY. The repos
are public with no release until it is approved (MW1 Rebuild and Inception
Rebuild; Inception Remake's repo stays untouched until Inception is done). A
card whose games are all COMING SOON or COMING LATER is greyed.

MW1 Rebuild and Inception's two are on the setup kit's contract (below): the
launcher reads their `--status` at the start and after each run or install
(its state and note on the card) and PLAY starts them plain; their own setup
screen opens when their game files are missing. One that serves its setup
(`"setup": 1`) goes through the launcher's screens first while its files are
not ready, or were only found by its own search and not set up (2026-11-25:
ready with the note "found by the search, not set up yet", both games' words;
`kit_set_up`). Their names: `mw1r`
(`mw1r-v`, `MW1-Rebuild-*`), `chi` (`chi-v`, `CHI-Rebuild-*`), `chirm`
(`chirm-v`, `CHI-Remake-*`); `MW3_MW1R_BIN`, `MW3_CHI_BIN`, `MW3_CHIRM_BIN`
for the checks.

The log names each card's button by its game, the tabs after the bottom row
(so the pad and the keys reach PLAY first): "screen main: MECHWARRIOR 3,
MECHWARRIOR 1, INCEPTION ORIGINAL, GAME FILES, UPDATES, ADD FROM FILE,
QUIT, MECHWARRIOR 3 TAB, PIRATE'S MOON TAB, MECHWARRIOR 1 ORIGINAL TAB,
MECHWARRIOR 1 REMAKE TAB, INCEPTION ORIGINAL TAB, INCEPTION REMAKE TAB". Every
game's entry line is logged once whichever tab shows, plus "entry INCEPTION
ORIGINAL:" and "entry INCEPTION REMAKE:".

The header says BATTLETECH (2026-11-22: the family's launcher), on the cards
and on the launcher's own screens, the window is "BattleTech", and the corner's
version reads "BattleTech launcher <v>" (was "MechWarrior 3 Rebuild <v>"); title.png's
"MW3 REBUILD" is no longer drawn. A game's setup screen keeps its game's name. Since 2026-11-24 ("launcher yes, game
no"): inside the launcher every screen is headed with the emblem and BATTLETECH,
the game a setup screen is for named under it in amber; a game started on its
own (mw3's or mw3pm's own setup, Shift at the start) keeps its badge and name.
The log's "header <game>" line names the screen's game either way. Every
button's words fit inside it: drawn smaller, down to a floor (a plate's name
13, its line 10; a flat button's 11 and 9), until they fit, and only what
still does not fit at the floor (a long folder's path) is cut in the middle,
never a button's own name ("it should never say CHANG...LDER"). A plate keeps
16 px clear at each end for its rivets, a flat button 8.

The section below is the card layout before the families (one card a game);
its panel, badges, square buttons and bottom row are unchanged.

## The cards (layout C, 2026-11-20)

The main screen as the mockup picked on 2026-11-19 (layout C, "Cards";
its source in claude-files/keep/mw3-rebuild/launcher-mockup-2026-11-19):
the 1280x720 mockup at two thirds on the 854x480 canvas.

- The starfield darkened (its colour at 45%), no metal bands; the emblem and
  "MW3 REBUILD" top left; the status message in the header's right half.
- A card a game, left to right: MECHWARRIOR 3, PIRATE'S MOON, MECHWARRIOR 1
  (only when its remake or Elthial's port is there; two cards otherwise).
  Each card: the mockup's panel (a dark gradient, a steel edge, an amber
  line along its bottom), its badge on the panel with no box, its name, up
  to three status lines (the verdict, the versions, the folder; the folder
  shortened in the middle), and its buttons along the bottom.
- MW3 and Pirate's Moon: "REBUILT ON ITS OWN ENGINE" above one amber PLAY,
  which does what the round entry's click did (finds the game first).
- MW1: a grey PLAY ORIGINAL (Elthial's port of the 1989 game) above the
  amber PLAY REMAKE (MechWarrior 1 Remake). Both use the same MechWarrior
  (1989) folder (MechWarrior 1's files, below). The remake needs MW3 and
  Pirate's Moon first; the original needs only its port. The card says what
  is missing short ("Remake needs MW3 and Pirate's Moon"; the whole sentence,
  with GAME FILES, cut in the middle on a fresh install: MW1 dev 2's 1.2.0
  pass); the status line and the log keep the whole sentence.
- The original is cut for now (2026-11-20: Elthial's port does not
  run well enough yet; MW1 original may come out with Crescent Hawk's
  Inception). PLAY ORIGINAL is greyed, "COMING LATER", with "Original: coming
  later" on the card. The port is looked for only at `MW3_MW1_PORT` (the
  checks), so its start below stays tested; the packaging is opt-in
  (`combined.sh --mw1-port`, `MW3_PKG_WITH_PORT=1` for combined.cmd).
- The original's combat plays one BTECH.EXE revision (the port's
  CombatInstallation.c pins the whole file's CRC-32, 0x36A13E1A, 174,267
  bytes): `sdlfe::mw1::original_supported`. Another revision greys PLAY
  ORIGINAL ("BTECH.EXE NOT SUPPORTED", and "Your BTECH.EXE isn't a revision
  Elthial's port supports" on the card) instead of failing at the first
  battle; a pick made from PLAY ORIGINAL is kept for the remake and said.
  The checks use a stand-in of zeros with its last four bytes set to the
  CRC (`tests/mw1_btech_stand_in.h`): no game data.
- b5 (2026-11-26): the port's path is out of the launcher (MechWarrior 1's
  files and Elthial's port, below).
- The bottom row: CHECK FOR UPDATES, QUIT (flat buttons; GAME FILES and UPDATES until 2026-11-24), the package's
  version and the trademark line at the right.
- Square buttons only: a hovered one lights its edge in amber (an amber
  button also brightens); no ripple. The click sound stays.
- The badges are cut out (`tools/badge_cutout.py`: the black surround
  flooded to transparent from the edges) and drawn with their own alpha;
  `assets/launcher/README.md` has their prompts and seeds.
- The log and the checks name each card's PLAY by its game ("screen main:
  MECHWARRIOR 3, PIRATE'S MOON, MECHWARRIOR 1 ORIGINAL, MECHWARRIOR 1, GAME
  FILES, UPDATES, QUIT"), and the entry lines are as before, plus
  "entry MECHWARRIOR 1 ORIGINAL: <port> | needs Elthial's port".
- `--launcher-mw1` (checks) presses PLAY REMAKE, or PLAY ORIGINAL when
  there is no remake. Checks: `sdl_program_launcher_screens` (the hover's
  edge), `sdl_program_launcher_starts_mechwarrior_1` (both programs: the
  original's button starts the port, not the remake).
- The other screens (find, notes, data, updates, working) keep their
  metal-band look for now.

## MechWarrior 1 (MechWarrior 1 Remake, through the contract since b5, 2026-11-26)

The panel MECHWARRIOR 1 starts MechWarrior 1 Remake (mech1-remake-re). It is
shown when `mw1` (`mw1.exe`) is beside the launcher or installed by it
(`MW3_MW1_BIN` names it, for checks). It needs MechWarrior 3 and Pirate's Moon
(the catalog's `needs`), and its own files are the player's MechWarrior (1989),
`BTECH.EXE`'s folder.

- **Its needs** are the launcher's own view: MW3's install and Pirate's
  Moon's as their panels have them. Without one the panel says "Needs ..."
  and its line "Not ready: needs MechWarrior 3 and/or Pirate's Moon", PLAY
  starts nothing. Never the Remake's `missing`, which holds only its own files'
  gaps (launcher split b5: missing means not ready).
- **A Remake that serves its setup** (its `--status` says `"setup": 1`; the
  Remake's mw1/src/mw1_start.cpp) is asked with the needs' copies,
  `--status --with mech3=<MW3> --with mech3-pm=<PM>`. Its status gives the 1989
  game's state and `where`. Without them its line says "Not found yet: click
  to find your MechWarrior (1989)", and PLAY or GAME FILES opens its setup on
  the launcher's screens as a kit game's (find, the picker, look, install:
  the Remake's own finder and copy-in, mw1/src/mw1_game_files). A game-files
  pick given to LINK A COPY goes there too. Set up, PLAY starts it with the
  same `--with` pair, `--no-launcher` and this start's choices (`--windowed`,
  `--window`, `--nosound`, `--hidden`, `--no-import`); `play: 1`.
- **An older Remake** (no status, or one without `"setup": 1`) is started as
  before: `--install <PM> --mw3-install <MW3> --mw1-game <MW1> --no-launcher`
  and the choices, with mw3.cfg's `mw1_game=` read as it is (unchecked). With
  no `mw1_game=` the launcher can no longer give it one (its MW1 finder went
  with the port in b5): its line says "Not set up: update MechWarrior 1 Remake
  to find your MechWarrior (1989)", its page's GAME FILES row says the same
  with no CHANGE, and PLAY starts its UPDATE when a newer release is known, or
  says so on the status line. This is why the new launcher and the new Remake
  ship together (docs/launcher-split.md, c4).
- Its line is "Ready: <MW1 folder>" ("Ready, mw1-v1.0.0: <MW1 folder>" when its
  file holds a version, read as mw3pm's is), also on the log's programs line;
  the launcher stays open while it runs (While a game runs), then its status is asked again.
- Checks: `sdl_program_launcher_starts_mechwarrior_1` (a stand-in `mw1`: an
  older one's arguments, the needs, an older one without `mw1_game=`, a served
  one's status and start, its setup through the launcher's screens, GAME
  FILES), `sdl_program_launcher_offers_an_older_remake_its_update` (PLAY on an
  older one with a newer release known: its UPDATE). `MW3_LAUNCHER_FILES=mw1`
  opens its GAME FILES; `MW3_LAUNCHER_PICK` with `--launcher-mw1` answers its
  picker.

### MechWarrior 1's files and Elthial's port (removed from the launcher in b5)

From 2026-11-20 to b5 the launcher found, offered and copied in MechWarrior
(1989) itself (`frontend/sdl/mw1_files`, `test_mw1_files.cpp`; the find plate
shots `mw1found`, `mw1origfound`). That finder is the Remake's own now
(mw1/src/mw1_game_files, mw1tests' `mw1_game_files_*`), served through
`--setup`, and its rules (ours first, the named folders, the 512 MB refusal,
the marker, the zip picks) are documented there.

Elthial's MechWarrior SDL port (github.com/Elthial/Mechwarrior-sdl-port) was
the MECHWARRIOR (1989) entry from 2026-10-31, then MW1's PLAY ORIGINAL, cut to
COMING LATER on 2026-11-20 and looked for only at `MW3_MW1_PORT`. b5 (Decided,
6 in launcher-split.md) took its path out: the port's start, PLAY ORIGINAL,
the BTECH.EXE revision check (`original_supported`, CRC-32 0x36A13E1A) and
its words, and `sdl_program_launcher_starts_mechwarrior_1989`. MECHWARRIOR 1
ORIGINAL is MechWarrior 1 Rebuild's (mech1-re) alone. The port's opt-in
packaging went right after: `tools/package/mw1port/` (build_port.sh/.cmd,
its pins and patches, THIRD-PARTY-MW1989.txt), `MW3_PKG_MW1_PORT_DIR` in
make_app.sh, make-appimage.sh and package.cmd, and `check_dist.ps1 -Mw1Port`
(combined.sh's `--mw1-port` and combined.cmd's `MW3_PKG_WITH_PORT` had gone
already). The Windows installer still removes `{app}\MechWarrior1989` and
`THIRD-PARTY-MW1989.txt` from an older install it is put over.

## Mods (2026-11-08, removed 2026-11-10)

The launcher had a MODS button and screen (the list, on and off, UP and
DOWN, IMPORT, EXPORT, ADD FOLDER, MODS FOLDER, CHECK and a mod's report,
on the game last clicked). Removed (2026-11-10: "we don't need it
there anymore... if we're doing mods directly in the games"): each game's
own MODS screen does all of it, and more (REPORT, TEST, custom campaigns,
the editor; docs/modding.md). The shared code those screens use
(`mods_list`, `mod_report`, `mod_needs`, `mod_install`, `mod_export`) and
its tests stay. The note "N mods on" that stayed on each game's line went
too (2026-11-10, mods by campaign): a plain start mounts no mods, so the
count said nothing true.

## Built (2026-10-30, mac5/launcher-ui)

`frontend/sdl/launcher.cpp` (`sdlfe::run_launcher`), called from
`mw3_main.cpp` for a player's plain start (not `--hidden`, `--frames`,
`--shot`, a chain, `--mission`/`--fly`, `--movie`, `--dialog`, `-nofmv`, a
named install or `--install-from`/`--patch`; `--no-launcher` skips it):
- Its own SDL_Renderer window, always windowed since 1.2.0 (2026-11-20: on the Mac a fullscreen launcher is a
  fullscreen game to macOS, its bundle in the games category, so Game Mode came on and slowed every other process,
  the file picker's among them: 7-12 s to open, about 1 s windowed; gamepolicyd's log showed Game Mode on exactly
  while the picker loaded). The games still go fullscreen.
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
- The games are round badges (2026-10-31: the ripple is a circle's):
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
- Only the round badges ripple (2026-11-02, after the PC's fresh
  install: the ripple belongs to a circle, as in the original's menus). A
  square plate hovered is brightened and lights its edge in the frame's
  amber (a line, a softer one inside); the rollover sound as before. The
  shot logs what the hovered entry drew (`hover <label>: ripple|edge`),
  and `sdl_program_launcher_screens` checks it on each screen with the
  edge's pixel.
- The ripple is the main menu's own ("much slower than the original
  ripple"): `mech3::menu_ripple_step` (menu_ripple.h), the one SlideButton
  runs (0x00485650): from 20 pixels, 120 a second, the amplitude 7 less 0.3
  a 30 fps frame; drawn by 0x00558dd0. `launcher_ripple_matches_the_main_menus`.
- Pacing: vsync never alone (2026-11-25). macOS does not hold a present back for a window not on screen (behind a
  fullscreen game): 1.5.0 drew some 950 frames a second there, every one with new drawables, and took 25 to 55 GB.
  A frame now takes at least 0.9 of the display's refresh, and a window the system calls covered or minimised waits
  100 ms between frames, any event at once. `MW3_LAUNCHER_UNPACED` (checks) is a present that does not wait.
- Paced by vsync (else a precise sleep to 60 a second). Timed on the reference PC
  (2026-10-31): the first frame drawn 306 ms after the start (art 86 ms,
  the install search 197 ms), shown in a window at 339 ms; 55.9 frames a
  second with vsync, 54.8 with the sleep, the draw 2.6 ms (direct3d11). The
  start was never slow: the old ripple was (80 pixels a second, 2 s long).
  The log gives these at every start and quit.
- The hover ripple went with the cards (2026-11-24). The sounds are the
  launcher's own since b6 (2026-11-26; assets/launcher/README.md, how they
  were made): `hover.wav` at 0.7 when the hovered or focused button changes
  and `click.wav` on a press, the gains of the main menu's (sounds.zrd's
  MAINMENU set), through an SDL audio stream, from the first screen with no
  install needed. Before, they were the game's own sfx_button2.wav and
  sfx_button6.wav, read from the install's soundsL.zbd or soundsH.zbd (none
  without an install). None with `--nosound`; `MW3_LAUNCHER_MUTE` plays them
  at no volume (checks). `sdl_program_launcher_plays_the_menus_sounds`.
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

## While a game runs (2026-11-26)

The launcher stays open beside the games it starts (the owner, 2026-11-26: "just keep the launcher open. if they
want it closed they can close it."; no setting: a `game_start=` line an earlier build wrote to mw3.cfg is ignored).
A game's own setup (mw3 or mw3pm started alone, `setup_only`) is not the launcher and still closes once its game is
up.

- Each game started is a run of its own (`Launcher::Run`, one a game): its panel's PLAY says RUNNING, grayed, until
  its process exits; the other games start as usual. MechWarrior 3 and Pirate's Moon count as running from their
  first frame (the ready file, as below; input is ignored before it, as it always was); a kit game and MechWarrior 1
  Remake, which have no first-frame signal, from their start (until 2026-11-26 the launcher hid for them and blocked
  until they exited).
- At the first frame the status line says "<game> is running."; the game is let to the front on Windows
  (`allow_foreground`, FIX (A) W4; now for the kit games too). A game's end: "<game> closed." (or its error), its
  PLAY back, a kit game's status asked again, and the launcher raised once no game runs.
- UPDATE (the split's arrow, or UPDATE ALL) for a running game waits for its end: "<game> updates when it closes",
  begun by the run loop once the game exits and nothing else is being done; UPDATE ALL's own launcher update comes
  after it (the launcher's restart would lose it). The game's page keeps its buttons grayed while it runs (no
  CHANGE, UNINSTALL or UNLINK under a running game).
- Pacing while a full-screen game covers the launcher (1.5.1's rule, Built below): a window the system calls covered,
  minimized or hidden waits 100 ms between frames; with a game running, so does a window without the input focus,
  so the rule holds where the system does not report the cover (Windows under an exclusive full-screen game).
- A Windows game that starts itself again (a MODS campaign's restart, `kStartedAgainExit`): the game goes on as a
  process the launcher did not start. The launcher stays and says "<game> started itself again: it runs on, but its
  panel can't follow it." (PLAY is then live again: the restarted process does not tell the launcher its id; a later
  game build could, beside the ready file.) A game's own setup closes, as before.
- Quitting the launcher while games run leaves them running. An AppImage-mounted game (`in_own_mount`, a game
  beside the launcher in its AppImage): the launcher never ends from under it; a quit waits for it hidden
  (`mount_wait`, FIX (A)), and its crash box is the launcher's.
- Tests: `sdl_program_launcher_starts_mw3_as_its_own_program` (stays open, quits at the game's end with
  --launcher-play), `sdl_program_launcher_stays_when_the_game_fails_or_is_slow` (the restart before and after the
  first frame), `sdl_program_launcher_waits_for_the_game_in_an_appimage` (a quit leaves the game; in an AppImage it
  waits).

## The two programs (2026-11-01, the launcher split; games-layout.md)

`mw3launcher --version` prints the build's version on one line and ends with 0, as `mw3 --version` does, before
anything is opened or made (2026-11-11: a package check wants all three programs' versions without a window;
`sdl_program_launcher_says_its_version`).

- `mw3launcher` (`launcher_main.cpp` over the same `launcher.cpp`): the
  main icon. Windows: mw3launcher.exe, "MW3 Rebuild" in its version
  resource, no console in any build (its output in the write dir's
  mw3launcher.log; the game's stays mw3.log); the installer's main and
  desktop shortcuts, "MechWarrior 3", start it; "MechWarrior 3 (no
  launcher)" in the Start menu starts mw3.exe. Mac: "MechWarrior 3.app"'s
  own executable (CFBundleExecutable), mw3 beside it in Contents/MacOS.
  Linux: the AppImage's entry (mw3.desktop's Exec, its Name "MechWarrior
  3"). The installed names are the game's (2026-11-01); the release
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
  moment, then stays open beside it (While a game runs, above; from
  2026-11-01 to 2026-11-26 it **closed** there, the game running on alone). A failed start
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
  status line in place of the 20 s "not shown a picture yet" (
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

## The setup kit's contract (2026-11-21, branch setup-kit)

The shared setup kit (github.com/telltaleatheist/re-setup-kit, `extern/re-setup-kit`)
lets each game answer the launcher on its own, so the launcher need not carry
each game's recipe (DESIGN.md there):
- `mw3 --status`: one JSON line on stdout, exit 0, no window and no log; the
  start's own search (mw3.cfg's `install=`, `<write dir>/install`, the
  installer's places) and `check_install`. `files.state` is `ready`,
  `missing` or `incomplete`; `first_run.pending` lists `movies` while the
  importer has movies to convert.
- `mw3 --setup`: the find screen alone, whatever is found; exit 0 once a pick
  is installed and saved to mw3.cfg, 1 when the player quits it, 2 when SDL
  cannot start. `sdl_program_status_and_setup_answer_the_launcher`.
- `mw3 --setup --describe | --search | --look <pick> | --install <pick>
  [--folder <dir>]` (2026-11-25, docs/launcher-split.md a2): the same asks a
  kit game answers, through MechWarrior 3's recipe (`frontend/sdl/mw3_recipe`:
  map_source's rules named `mech3` and `mech3-pm`, the start's search then a
  disc in a drive, install_from_source). No window; the copy a look finds in
  place, or an install puts in `<write dir>/install`, is saved as `install=`.
  A Pirate's Moon pick is refused in its words. The status says `"setup": 1`;
  the launcher asks it only once MW3 is one of its kit games (a later step).
  The data folder and `rebuild-program.txt` are the kit's too
  (`rekit/data_folder.h`, `rekit/program_record.h`), with the same files.
- Its extras (a3): `--setup --copies` (GAME FILES' list, as find_copies:
  each copy once, the one in use first, what it lacks: music, movies, the 1.2
  patch), `--add <pick>` (ADD FROM MY DISC: what the copy in use lacks, from a
  disc, its image or download; `""` takes the disc in a drive) and
  `--first-run movies` (the importer, as convert_movies; on Windows nothing
  to do). The status also carries `notes` (launcher_notes' and the start
  check's), `user` (the write dir) and `"play": 1`.
  `sdl_program_serves_the_launchers_extras`.
- What the panels need (b2, 2026-11-26): the status looks where the launcher
  looked (`mw3_status_find`): the card INSTALL's folder's `install/` and
  this program's own marked game folder's (`own_game_files`: the mark must
  name `mw3`), mw3.cfg's `install=`, the user folder's `install/`, the
  installer's places, the development copy. A copy from the installer's
  places, or any other while the game's own folder is set, gets the note
  `found by the search, not set up yet` (the kit games' words: offered to be
  copied in, never taken). `--copies`, `--add` and `--first-run` use the same
  copy in use. The start check's warnings come first in `notes`, counted in
  `notes_once` (the launcher shows them once, by `mw3_notes=`'s hash), then
  launcher_notes' every time. `files.lacks` says what the copy lacks
  (`music`, `movies`, `the 1.2 patch`) from the code its `--copies` row
  uses. `--status` wins over `--setup` and its asks, in any order: a
  launcher's child never reaches a screen. `sdl_program_status_finds_as_the_launcher`.
**MechWarrior 3's and Pirate's Moon's panels from their status (b2,
2026-11-26).** The launcher asks every game's `--status`: MechWarrior 3's
with its own user folder (they share it), Pirate's Moon's with `pm_wd()`,
the kit games' as before. MechWarrior 3's and Pirate's Moon's programs are
asked only when their file carries our program version (the tag the panels'
"program" words come from): a file without one, a script or not ours, is
never run, as before. At the start all are asked at once, each on its
own thread, beside the fonts and the Vulkan check, and collected where the
checks of the files were (`status_collect`), so the first frame has every
panel's lines and nothing flickers. Measured on the Mac (a shot, five runs):
the first frame at about 1.16 s with both programs answering, 3.2 s with
neither (the older path's own checks, Pirate's Moon's search the slow part);
Pirate's Moon's ask adds nothing. After the start a status is asked again
when a panel's lines are made, at most once a frame (`status_fresh_`), so a
change the launcher made (an install, a link, ADD FROM MY DISC) shows. From
the status: the copy in use and its data (`find_install`, `find_pm`; the
note `found by the search, not set up yet` makes it an offer), what it
lacks (`files.lacks`: the panel's "no music" and the like), the notes (the
start check's `notes_once` hashed under `mw3_notes=` / `pm_notes=` as
before, so saved hashes stay valid; the music's folder shortened to fit).
A copy that only `pm_install=` names is still looked at by the launcher.
MechWarrior 3's start check is the status's too: its notes come as
`notes_once`; a copy it refuses (no Mech3Msg.dll) is started and mw3 says so
in its own box, as before, but the launcher's log no longer has the check's
"cannot start" line on that path.
**The older programs' path:** a status without `"setup": 1` (a game
released before), no answer, a program without our version, a copy in use the status does not name, and
mw3's own find screen and mw3pm's own start (no program of their own to
ask) keep the launcher's own checks (`find_install_older` and the like),
unchanged; they end at b6 (docs/launcher-split.md). Panels and PLAY are
otherwise the same code; MechWarrior 3 and Pirate's Moon become kit games
(their GAME FILES and PLAY through the game) at b3 and b4. Tests:
`sdl_program_launcher_panels_from_the_status` (stand-ins answering as the
games do; the older path's hash equal to the status's), the launcher tests
through the real mw3's status and setup (`kAsksAsMw3` and `kVersionLine` in the
stand-ins; Pirate's Moon's stand-ins carry no version, so they test the
older path).

**A status unknown is not an older program (1.7.1, mech1-remake-re #2,
2026-11-26).** Launcher 1.7.0 told players with Remake 1.1.3 that it was
"older than the launcher": its status had not come, and no status was taken
for no served setup. Three ways it had none: a Linux AppImage the player
downloaded and the launcher linked carries its version only inside its
packed files, so it was never asked; an ask past 10 s (a first start under a
virus scan, a slow disk) counted as none; any other failure (it did not
start, it exited with an error, it printed no status) was folded in, the log
saying only "no answer". Now:
- An AppImage linked is asked when its own `--status` said it is the game
  (`find_link`'s `identify`, which runs AppImages): `may_ask`. Its version
  in the panels comes from that answer too (`program_tag`).
- `kit_status` says why it had none (`it did not start`, `no answer in 10 s:
  stopped`, `it exited 1: ...`, `no status in its answer: ...`, its status
  unreadable, `not asked: its program file carries no version of ours`), in
  the log line `games: <key>: status no answer (<why>)`.
- The Remake's start ask runs on its own thread (`ask_later`, 30 s), its
  panel "Checking the Remake..." meanwhile; the first frame no longer waits
  for it. MechWarrior 3's and Pirate's Moon's start asks still run beside the
  fonts (10 s); one that gave none is asked once more on its own thread.
- PLAY (or GAME FILES, ADD FROM MY DISC and the other `older_game` callers)
  on a game whose status is unknown asks it again on its own thread, with
  "Checking <game>..."; once it answers, the PLAY is pressed again. One that
  still gives none says "<game> did not answer the launcher (see
  mw3launcher.log)" and nothing about updating. Only a status that came
  without `"setup": 1` is older (`older_game`, `mw1_too_old`). The panels
  say "Checking ..." or "did not answer: PLAY asks again" instead of
  "Update it".
- A panel's lines do not ask again while an ask is on its thread, nor after
  one gave none (a program that hangs would hold every frame); an event
  (PLAY, a run's end, an install, a link) asks again.
- A quit does not wait for an ask on its thread: its answer is its own
  (`LaterAsk`), and its program is stopped with the curls (`stop_asking`,
  `run_program`'s `quits`), the launcher waiting at most a second for that.
- A link to a copy older than one found elsewhere (the program's own record,
  a Setup's, Applications) moves to the newer one (`find_link`); before, a
  link kept the older copy asked after the player installed a new one.

`MW3_STATUS_SECONDS` (checks) sets every ask's limit. Test:
`sdl_program_launcher_says_an_unknown_status_is_not_older` (stand-ins: a
Remake without our version, one too slow, one slow only the first time, a
linked AppImage, one without a served setup; MechWarrior 3 slow at the
start, and one without our version).

**GAME FILES through the game (b3, 2026-11-26).** MechWarrior 3's and
Pirate's Moon's GAME FILES keep their own screens (the find screen's words
and buttons, the copies list, USE THESE GAME FILES?, the notes, NO MUSIC
ADDED), and what the screens do underneath is asked of the game's program
when it answers for its files (`files_served`, the status's `"setup": 1`),
each ask with its user folder (`--write-dir`): the copies list from
`--setup --copies`, a disc in a drive from `--search` (its copy is the
status's already), a pick looked at by `--look` (the user folder's own copy
taken in place, anything else on the plate with the game's words), GO's copy
by `--install <pick> [--folder]` (the launcher claims the folder first, as
before, so a folder of someone else's is refused before anything starts; the
game claims it too), ADD FROM MY DISC by `--add`. The game saves `install=`;
the launcher asks its status again after each. `--add` says only whether it
worked, so NO MUSIC ADDED comes from the status: the music wanted before and
still wanted after (never for a stopped add); the movies an add brought are
converted when the status has `pending: ["movies"]`. The launcher still
removes its own earlier copy when GAME FILES moves to another (marked
folders only); that moves into the game's `--install` at b6. The older path
(no program to ask: mw3's own find screen, mw3pm's own start; an older
program) is unchanged. Test: `sdl_program_launcher_game_files_through_the_game`
(the real mw3's `--add` with a cue sheet's music, a data-only image and a
stopped add; a scripted mw3pm's look, install, copies and adds); the stand-ins
pass `--setup` to the real mw3 (`kAsksAsMw3`).

**PLAY through the game (b4, 2026-11-26).** Before MechWarrior 3 starts,
the movies its status lists as pending (`first_run.pending: ["movies"]`)
are converted by the game, `--setup --first-run movies`, on the same work
screen (CONVERTING THE MOVIES, its stages "Converting the movies (K of N)"
as before; every step the game sends is shown, a quick one's too), unless
this launcher was told not to (`--no-import`, `MW3_NO_IMPORT`). The same
after ADD FROM MY DISC brought movies. mw3's status counts them as the
launcher did: the copy's own and those in the media folder's `video\`, and
only with the importer beside the program. The start options `play: 1`
names were already passed (window, sound, hidden, the user folder, the
ready file, and `--no-import`); the copy in use is passed too, until b6
settles how the game's own find and the status's agree
(docs/launcher-split.md, b6). Each work screen's stage is logged once
(`work stage:`) for the checks.

**A kit game's files, on the launcher's screens (2026-11-24).** "I thought we
manage the setup kits from the launcher": PLAY on Inception opened Inception's
own setup window. A game whose status says `"setup": 1` answers `--setup
--describe | --search | --look <pick> | --install <pick> [--folder <dir>]`
with JSON lines (the kit's `rekit/sdl/setup_serve.h`; `sdlfe::games::kit_setup`
runs it, a "cancel" line on its stdin for CANCEL). PLAY with its files missing
opens the find screen with the game's words; FIND THE GAME is its search, then
the picker; any pick goes to the game as it is (it knows what a pick holds);
the find's plate, INSTALL and CHANGE FOLDER as MW3's, the default being the
folder its DOWNLOAD picked, else the game's own. Installed, its status is read
again and the game starts. GAME FILES after its PLAY, and its page's GAME
FILES CHANGE, open the same screens without starting it (`open_kit_files`,
`kit_look`, `kit_looked`, `kit_install`, `kit_ready`; `kit_files_` says whose
the screens are). A game without it is started plain, its own setup opening as
before. The recipe stays the game's. `--launcher-screen chifiles` draws it.
Check: `sdl_program_launcher_sets_a_kit_game_up` (a stand-in).

## Open

- The working title ("MW3 REBUILD" on the title art) and the font: ours.
- The calls are Mw3 dev's `mech3::setup` (game_setup.h, mac4/setup-api).
- Once the GUI-subsystem build lands, mw3.log is the only console.

## Updates (planned 2026-11-10; 2026-11-09, no signing)

CHECK FOR UPDATES in the launcher, the same flow on all three platforms, never
the settings folders (MW3's, PM's, or where their pointers lead): only the
program files are replaced.

1. **Check**: GitHub's releases lists of both homes of the package
   (`api.github.com/repos/telltaleatheist/mw3-re/releases`, the hub every
   copy up to 1.2.1 asks, and `.../mech3-re/releases`, MechWarrior 3's own
   repo after the split; 2026-11-21), merged (one not answering is
   fine; the check fails only when neither does), the highest
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
   taken. Since 2026-11-21 (each game released on its own repo, its
   builds bundled into the launcher's package on mw3-re) a release counts
   only when it holds this platform's bundle (`MW3-Rebuild-...`), so a
   game's own release on mech3-re (MW3-alone downloads) is never the update;
   a game repo's release and the hub's bundle carry the same version, so the
   1.2.2 copies (which still take the highest tag of either) find the
   bundle first, on mw3-re, read first. `MW3_UPDATE_API` names another URL for
   checks (a `file://` JSON). Tests: newer, same, older, a dev build, no
   network, a broken answer: each said in plain words.
   **Done (2026-11-10):** `upgrades::update` (`parse_releases`, a small JSON
   reader; `package_tag`, `newest_package`, `compare_versions`, `judge`) and
   `sdlfe::curl_get` / `check_for_updates` (`frontend/sdl/sdl_update`; curl
   found at Windows' System32\curl.exe or /usr/bin/curl, never through
   PATH; `--proto =https`, `file://` only for `MW3_UPDATE_API`). UPDATES on
   the main screen (since 2026-11-24 CHECK FOR UPDATES, below) asked on the worker and
   said the verdict on the status line. Checked once against GitHub's real
   list: v1.0.5 the newest package, this build up to date. Tests:
   `update_reads_the_releases_list` (escapes, \u, a surrogate pair, four broken
   answers), `update_picks_the_newest_package_release` (drafts, prereleases,
   pm-v*, rc tags, 1.10 above 1.9), `update_compares_versions` (a dev build
   counts as its release), `update_says_what_it_found`,
   `sdl_update_fetch_by_curl`, `sdl_program_launcher_checks_for_updates`.
   Breaks: drafts kept, rc tags taken: each caught.
   **At every start (2026-11-23: so new games, features or a reworked
   look reach players):** the same check runs quietly on a thread of its own
   (`start_self_check`, collected each frame by `take_self_check`), both
   repos in both homes as CHECK FOR UPDATES asks them. The main screen never waits on
   it and it never prompts. A newer release shows an amber button, UPDATE
   AVAILABLE <v> (`kSelfUpdate`), in the header's top right beside the mark
   (2026-11-23, as Discord's and Chrome's; it was the footer's version
   turned into a button, too small to see), after QUIT in the keys' and the
   pad's order; the footer keeps the version. Pressing it opens the UPDATE screen (notes,
   UPDATE, BACK). Nothing is downloaded or installed until
   the player presses UPDATE there. The same version, an older
   one or a failed check show no button; the verdict goes to the
   log only ("update check at the start: ..."). A check run (`--hidden`, or
   a `--launcher-shot`) without `MW3_UPDATE_API` asks nothing, so tests never
   reach GitHub or git.owenmorgan.com (unless it is a live check,
   `MW3_LIVE_CHECK=1`); a shot with it asks before drawing.
   `MW3_LAUNCHER_NAV_LATE=1` (checks) runs `MW3_LAUNCHER_NAV`'s steps once
   that check has answered. Test:
   `sdl_program_launcher_offers_its_own_update_at_start` (newer: the footer
   line, the button in the main screen's list after QUIT, reached by the keys,
   never opening anything by itself, pressed: the UPDATE screen and its notes,
   nothing downloaded; the same version and a failed check: the footer kept,
   the verdict in the log; a hidden run: nothing asked).
   **CHECK FOR UPDATES (2026-11-24: for a launcher left open for days; was UPDATES):** asks again for the launcher
   and for every game, in place (`recheck`, collected by `recheck_done`; the main screen stays). Games with a newer
   release get their UPDATE beside PLAY and are named on the status line ("Ready to update: Pirate's Moon 1.2.5.");
   else "Everything is up to date." or what could not be reached. When the launcher has one, a box over the games
   says what is ready: UPDATE LAUNCHER, UPDATE ALL (only when games have one too: the games first, the launcher last,
   as it restarts; it stops at the first that fails), NOT NOW (Escape). The buttons beneath are off while it is up.
   `--launcher-screen updatebox` draws it; `MW3_LAUNCHER_UPDATE_ALL` (checks) presses UPDATE ALL. Test:
   `sdl_program_launcher_rechecks_while_open`. Two checks can run at once, so a temporary file's number is counted
   atomically (they once shared one and an update could be missed).
2. **The notes and the question**: an UPDATE screen with the release's notes
   (its body, wrapped and paged, the markdown's `#` and `-` shown plainly),
   "Update the launcher to 1.2.0?" (was "Download and install MechWarrior 3 Rebuild 1.2.0?"), UPDATE and BACK
   (2026-11-23: two buttons, no paging arrows; long notes scroll with the wheel). Tests: the screen's text from a fixture release.
   **Done (2026-11-10):** a newer release opens the UPDATE screen
   (`Launcher::show_update`): "BATTLETECH LAUNCHER 1.2.0", its notes as
   plain lines (`upgrades::update::notes_lines`: headings without #, items as
   bullets, **, * and ` marks dropped, links as their text) wrapped to 500 px
   and paged with < and >, the question with "Your mods, pilots, saves and
   settings are kept.", UPDATE (sitting 3) and BACK. Same or
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
   `mech3::sha256_file_hex` a megabyte at a time). UPDATE runs it
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
   compile-checked with the mingw build and played on the reference PC.
   **Done (2026-11-10):** `upgrades::update::swap_in` (copied beside the old as
   .new, the old moved aside as .old, the new renamed in, the old removed last;
   any failure puts the old back; an earlier try's .new/.old cleared); since 2026-11-25 a
   Mac app keeps its own folder and exchanges its `Contents` with the new one's
   in one step (renamex_np RENAME_SWAP; the whole-folder move only where that
   fails), because the Dock follows the folder: renamed to X.app.old it was
   seen as a plain folder, a click listing "Contents". Found on 1.5.1, the disk
   and Launch Services right (an application bundle, one path), so the Dock's
   own memory; the rename as its cause is inferred, not seen happening) and
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
   Windows Setup run waits for the reference PC.
   **Windows, run for real (owens-pc, 2026-11-23):** two launcher-only
   packages from package.cmd: the old one from main 09da080e, the new one
   from pc/launcher-self-update tagged v1.3.99 for the run (a local tag,
   deleted after). Both Setups were built from a test-only variant of
   installer.iss: another AppId and name ("BattleTech upgrade test"),
   `PrivilegesRequired=lowest`, a scratch install folder, the
   `MechWarrior 3.lnk` deletes and the desktop icon left out. The PC's own
   per-machine 1.2.0 install, under the real AppId, would otherwise have been
   the one upgraded. User folders: fake BattleTech ones, the real MicroProse
   folder renamed aside and back. The run:
   - the old Setup installed;
   - the old launcher started with `MW3_UPDATE_API` at a `file://` list
     naming the new Setup and its SHA256SUMS;
   - UPDATES, then the UPDATE screen with the notes;
   - UPDATE: "Downloaded and checked", the new Setup started
     and the old launcher quit;
   - the normal wizard (licence, the folder page offering the installed
     folder, tasks, Ready, Install);
   - Finish with the Launch box ticked by default.

   Result:
   - the new launcher opened by itself from the same folder, its corner
     "BattleTech launcher 1.3.99-dirty", its start check saying it is the
     newest;
   - one uninstall entry, now v1.3.99, with the same `unins000`: upgraded in
     place;
   - every user file's SHA-256 the same before UPDATE and
     after the new launcher was up (8 files: mw3.cfg, a pilot, a save, PM's
     mw3.cfg and pilot, rebuild-games.txt, names-utf8.done; the log is
     rewritten by each start);
   - The real install untouched (still v1.2.0).

   Not covered: the admin (UAC) path with the real AppId over an existing
   per-machine install; that needs someone at the PC. Found on the way and
   fixed: since the two homes, every Windows package built on this PC failed
   `check_dist.ps1`, because the user name was inside git.owenmorgan.com's
   API address. It and `tools/deps/check_paths.py` now allow the backup's
   addresses, as they allow GitHub's.
5. **the hard rule, checked**: the upgrade test (`sdl_program_package_upgrade_keeps_the_players_files`)
   run through the updater's own swap, the settings folders unchanged by
   hash.
   **Done (2026-11-10):** the test's new program folder is built beside and
   put in by `upgrades::update::swap_in`; the 12 player files (MW3's data
   folder through its pointer, PM's settings folder) kept byte for byte,
   every mw3.cfg line kept, both games started from the new programs. Break
   (the new copy never put in: the old programs started again): caught. The
   Mac's own install (`sdl_update_installs_the_mac_app`) checks the settings
   folder beside the app the same way. Left for the release: the Windows
   Setup's upgrade over 1.0.x on the reference PC (release.md, Windows).

## Two homes for every release (2026-11-22)

Every family repo and its releases (with `SHA256SUMS`) are copied from GitHub
to a read-only Forgejo at `git.owenmorgan.com`, under the same paths
(`git.owenmorgan.com/telltaleatheist/<repo>`). The launcher reads both, so it keeps working if one is forgotten
or GitHub goes away.

- **The list of homes (2026-11-26; GitHub an entry since the signing).**
  The copies are data: the catalog's `"homes"`, each `{"key",
  "releases", "order"}`, `releases` a releases list's address with
  `{repo}` in it once (and `{count}` where the list takes a page size),
  https and a plain host (file:/// in checks), asked by `order`; the
  first is the one downloads come from first. The built-in list is
  GitHub's (`gh`, `api.github.com/repos/telltaleatheist/{repo}/releases?per_page={count}`)
  then the mirror's (`om`, Forgejo, `limit=40`); a fetched catalog with a
  higher revision replaces it the way it replaces games
  (`merged_catalog`), and one without `"homes"` keeps the built-in list.
  A bad list (not a list, empty, more than `kMaxHomes` (4), an address
  that is not https and a plain host or lacks `{repo}`, a key or address
  twice, a field missing, the older `url` shape) is left out whole, said
  in the log (`catalog: ... left out: homes[0]: ... (the built-in homes
  kept)`), and the built-in list stays; the games read as before. The
  start says the list in use (`catalog: homes: gh, om`). The releases
  lists (`release_lists`, through `home_lists`) and so the download's
  fallback (`other_homes`, in the homes' order) take the catalog in
  use's list (`upgrades::update::active_homes`); the catalog's own
  addresses are sources.json's and `catalog_sources` (Where the launcher
  looks). Unsigned `SHA256SUMS` are taken only from a list sources.json
  names (GitHub's). A move of the mirror is a catalog edit with its
  revision raised. Launchers 1.6.0 to 1.7.1 ignore `"homes"`,
  `"catalog_sources"` and `"next_keys"` (1.7.1's `read_catalog` looks
  each field up by name, so an unknown one is never read) and keep
  asking git.owenmorgan.com and GitHub, built into them, for releases and
  for `catalog/games.json` on the hub's main, unsigned: so that name and
  that file stay.

- **The lists.** Each repo is asked in both homes, git.owenmorgan.com first
  (`https://git.owenmorgan.com/api/v1/repos/telltaleatheist/<repo>/releases?limit=40`:
  Forgejo takes `limit`, not `per_page`), then GitHub. Forgejo's JSON has
  the same fields `parse_releases` reads (`tag_name`, `body`, `draft`,
  `prerelease`, `assets[name, browser_download_url, size]`). The update
  check asks mw3-re and mech3-re in both homes, four lists in all
  (`check_for_updates`); a game's card asks its repo in both
  (`games::check`). `sdlfe::release_lists` builds them; `ask_lists` asks
  them in turn.
- **The judging.** Every list that reads counts, and the newest release on
  any of them wins (`upgrades::update::read_homes`, then `judge` or
  `judge_game` over all of them). One home down or unreadable is fine; the
  check fails only when no list reads, saying the first one's reason.
- **The download.** A release in both homes is downloaded from GitHub
  first; the other home carries only what GitHub can't. `with_homes` builds the chosen release from the
  GitHub copy (an `Answer` marked `download_first`) and keeps the other
  home's assets as `Release::other_homes`. `sdlfe::download_checked`
  (used by the updater's `download_update` and the games'
  `install_release`) tries each home in turn when the one before fails:
  its file gone or cut short, or not its checksum. A newest release on only
  one home downloads from that home. Each home's file is checked against
  that home's `SHA256SUMS` line. A home without one (no `SHA256SUMS`, it
  could not be fetched, or no line for the file) uses the other homes'
  line when they agree, and nothing when they disagree. A cancel never
  tries the other home. The log names the home that failed and the one
  that served the file.
- **Checks.** `MW3_UPDATE_API` and `MW3_GAMES_API` name one list (taken as
  a trusted first home, as GitHub's) or two as `UNTRUSTED|TRUSTED`, the
  second downloaded from first (`{repo}` in each for the games).
  The list of homes: `games_catalog_homes_built_in_and_fetched`,
  `games_catalog_bad_homes_leave_the_built_in`,
  `sdl_homes_lists_and_catalog_addresses`,
  `sdl_update_download_falls_back_to_the_second_home` (two homes and
  GitHub: GitHub's file and the first home's gone, the second's served)
  and `sdl_program_launcher_uses_the_catalogs_homes` (built in, fetched,
  a bad list).
  Tests: `update_reads_both_homes` and `games_read_both_homes` (the same
  release in both: GitHub's file with the backup's behind it, in either
  order; newest on GitHub only; newest on the backup only; either home
  down; both down; a home without the platform's file; a draft is no
  copy), `sdl_release_lists_both_homes` (the URLs, and both forms of the
  variables), `sdl_update_download_falls_back_to_the_backup` (GitHub's
  file gone, GitHub's file damaged, GitHub without `SHA256SUMS` checked
  against the backup's line, no line anywhere, both gone, a cancel), and
  `sdl_program_launcher_checks_for_updates` (the launcher with two lists:
  newest on GitHub only, on the backup only, GitHub down, the backup
  down, both failing).

## Mac signing (2026-11-26, from 1.7.0)

The Mac launcher is Developer ID signed and notarized from 1.7.0 (every release before it was signed ad hoc, and
macOS asked once before the first open). `tools/macos/make_app.sh` does it through the setup kit's
`tools/macos/sign_notarize.sh` (the same script every game's build uses: the kit's README, Mac signing), which
takes both from the environment, so no name or team ID is ever in the repo: `RE_SIGN_IDENTITY` (the keychain's
Developer ID Application identity, its name or SHA-1) and `RE_NOTARY_PROFILE` (a `xcrun notarytool
store-credentials` profile). The `MW3_SIGN_IDENTITY`, `MW3_NOTARY_PROFILE` and `MW3_ALLOW_UNSIGNED` names are read
too.

- Signed: every Mach-O inside the app (the libraries in Frameworks), then the app, with the hardened runtime and a
  secure timestamp; `mw3.entitlements` grants nothing (no JIT, nothing loaded at run time, no sandbox). Then the
  DMG.
- Notarized: the DMG, `notarytool submit --wait`; Accepted is stapled to the DMG and to the app beside it,
  `stapler validate`, and `spctl` must say "Notarized Developer ID" for both. A rejection prints Apple's log and
  fails the build.
- A build that would not be notarized (no identity, no profile, or the profile not in the keychain) is refused before
  anything is built unless `RE_ALLOW_UNSIGNED=1` (development and check builds), so an unnotarized DMG never
  reaches a release. Its README.txt keeps the right-click > Open paragraph; a notarized one says it opens like any
  other app (`[notarized]`/`[unsigned]` in tools/macos/README.txt, picked by make_app.sh).
- Checked 2026-11-26: a dev DMG notarized at the first submission; the released 1.6.0 (ad hoc) updated itself to it
  through a file:// release, and the swapped app passed `codesign --verify --deep --strict` and `spctl` (Notarized
  Developer ID), its bundle ID and the player's files kept.
- Gatekeeper checks only what carries the quarantine flag (a browser's download). The games the launcher downloads
  and installs itself carry none, so the launcher's own DMG is the one players meet first.
- No Windows signing for now.

## Games on demand (2026-11-22)

"Make the launcher download the games when the player wants to play
them, with an update button", like StarCraft 2's launcher; players choose to
update; each game can still be downloaded and played on its own. So since
1.3.0 the combined package (`MW3-Rebuild-...`, the one the updater takes) is
**the launcher alone**, and each card installs its game from that game's own
release.

- **The check.** At the start each card's repo is asked for its releases
  (`mech3-re` for MechWarrior 3, `mech3-pm-re` for Pirate's Moon,
  `mech1-remake-re` for MechWarrior 1 Remake), on a thread, the cards
  saying "Asking GitHub..." meanwhile. The newest release whose tag is the
  game's own (`vX.Y.Z`, `pm-vX.Y.Z`, `mw1-vX.Y.Z`), not a draft or a
  prerelease, with this platform's file, is the answer
  (`upgrades::update::newest_game`). A check run (`--hidden`, or a
  `--launcher-shot` since 2026-11-23) asks only `MW3_GAMES_API` (a URL with
  `{repo}`, `file://` allowed), never GitHub or git.owenmorgan.com (a live
  check, `MW3_LIVE_CHECK=1`, asks the real homes); without
  it the log says "games: not asked in a check run" and the cards show what
  they show with no answer ("Can't reach the server").
  **A quit never waits on the network (lead, 2026-11-23).** Both start checks
  run on threads that the launcher joins as it closes. A home that hangs
  would have held the quit for the curl limits: up to 4 lists × 20 s for its
  own update, 6 games × 2 homes × 15 s for the games. Now `~Launcher` calls
  `sdlfe::stop_asking()` first. Any curl running is killed at once, a
  download too, and any later `curl_get` fails at once ("the launcher is
  closing"), so the threads end and join in moments. Nothing is detached,
  so no thread outlives the process's own teardown. Test:
  `sdl_program_launcher_quits_without_waiting_on_the_network` (both checks
  stuck on a FIFO list nobody writes: the quit took 1.1 s; with
  `stop_asking` a no-op it took over 60 s and failed).
  **The launcher-only package carries no FFmpeg.** The launcher loads none of
  it: the movies' import runs an installed MechWarrior 3's own `mw3import`,
  from that game's folder with its own DLLs. Until 2026-11-23 `package.cmd`'s
  `for` loop matched `av*.dll`/`sw*.dll` in the current folder, not the
  package's, so they shipped anyway. It now deletes them from the package,
  and `check_dist.ps1 -LauncherOnly` fails a package that has them. The
  installer's LauncherOnly delete of `{app}\av*.dll` and `sw*.dll` stays: it
  clears an earlier package's copies on an upgrade.
- **The files** (`game_asset_name`): Windows takes the zip, never the Setup
  (`MW3-alone-<v>-win64.zip`, `mw3pm-<pm v>-win64.zip`,
  `MW1-Remake-<mw1 v>-win64.zip`); the Mac the disk image (`MW3-alone-`,
  `PM-Rebuild-`, `MW1-Remake-` `...-mac.dmg`: attached read-only, its app
  copied out); Linux the AppImage (unpacked with its own
  `--appimage-extract`, no FUSE needed). Each is checked against its
  release's `SHA256SUMS` line; nothing is installed without one.
- **The card.** A game not installed shows DOWNLOAD (2026-11-23: no "install" on the launcher's own buttons; was INSTALL) (with the version and the
  download's size) where PLAY goes; offline it says it can't reach the
  server and points to its page's INSTALL FROM A DOWNLOADED FILE. An installed game with a newer
  release splits its PLAY button (2026-11-22: "the main button can
  be play, and there can be a divider on the end"): PLAY (its word centred
  in its own part, 2026-11-24), a short divider,
  and at its end a download arrow over the version, which is that game's
  UPDATE (its own button for clicks, focus and the checks, logged as
  "<GAME> UPDATE"). Since the family cards each tab's game splits its own
  button the same way. Nothing installs until the player presses one.
- **Where.** The launcher's own games folder, per user and writable without
  rights: `%LOCALAPPDATA%\BattleTech\games`,
  `~/Library/Application Support/BattleTech/games`,
  `~/.local/share/BattleTech/games` (`$XDG_DATA_HOME`); up to 1.3.1 under
  `MicroProse/BattleTech`, moved across once (program.md, The family
  folder). One
  folder a game (`mw3`, `pm`, `mw1`), each with `rebuild-game.txt` (game,
  tag, file, its SHA-256, github or file). The games folder carries
  `rebuild-games.txt`; a folder is unpacked into `.incoming` (marked) and
  swapped in, the old one moved aside and removed last. The launcher removes
  or replaces only folders that carry its marks; anything else in the way
  stops the install with a message.
- **The player's files are never touched.** Each game copies what it needs
  from the player's original into its own data folder (since 1.2.3), apart
  from the program, so an update replaces only the program: the original is
  never asked for again.
- **A game beside the launcher wins.** A package or development build that
  carries a game (MW3 alone, `build/`) starts that one, as before; its card
  has no UPDATE (it updates with its package). The Windows Setup of the
  launcher alone removes an earlier package's game programs from its folder
  (installer.iss, `LauncherOnly`), so an update from 1.2.4 does not keep
  starting old copies; the Mac app and the AppImage are replaced whole.
- **INSTALL FROM A DOWNLOADED FILE** (a game's page; ADD FROM FILE on the bottom row until 2026-11-24): a game's download from
  anywhere (a modder's build, no internet). A `SHA256SUMS` beside it is
  checked: a match installs, a mismatch is refused; without one the player
  is told it can't be checked and asked (ADD ANYWAY). Which game it is
  comes from the program inside it; the full package (more than one game) is
  refused.
- **Started from there.** An installed game runs from its own folder, so the
  AppImage launcher no longer waits for it to end (that wait was for a game
  inside its own mount, `in_own_mount`). The movies' converter of an
  installed MechWarrior 3 is used when the launcher has none beside it.
- **Code.** `src/upgrades/update/games.{h,cpp}` (the logic: tags, files,
  the record, a card's verdict; tests `games_*` in `test_update.cpp`),
  `src/frontend/sdl/sdl_games.{h,cpp}` (the folder, check, download,
  unpacking, swap). Checks: `MW3_GAMES_DIR`, `MW3_GAMES_API`,
  `MW3_LAUNCHER_GAME_INSTALL=<mw3|pm|mw1>` (DOWNLOAD or UPDATE pressed once
  the answer is in), `MW3_LAUNCHER_INSTALL_FILE=1` with
  `MW3_LAUNCHER_PICKER=path:<file>`, `MW3_LAUNCHER_INSTALL_ANYWAY=1`.

### The original game's files (2026-11-26)

No catalog lists a source today, so nothing below ever runs yet. A game
whose catalog entry lists `"original"` sources (The games catalog) gets
them on DOWNLOAD, right after its own release is installed.
DOWNLOAD is the player's consent, so no question and no confirm screen
follows (decided 2026-11-26). Only the progress and the usual CANCEL show.

- **When.** Once a card's DOWNLOAD is installed (not an UPDATE, not INSTALL
  FROM A DOWNLOADED FILE), the launcher asks
  the game's `--status` (its write folder passed, as for every status). It
  fetches only when the game has no files yet (`missing`; `incomplete` is
  some of them, left to its GAME FILES) and answers a served setup
  (`setup` >= 1). Otherwise the log says why
  (`original: <key>: not fetched: it has its game files`, `...: no answer`,
  `...: its setup is not one the launcher drives`).
- **The fetch** (`sdlfe::games::get_original`). The work is "GETTING THE
  ORIGINAL GAME FILES". Each source is fetched in turn with the release
  downloads' curl (`curl_download`: HTTPS only, redirects too), into
  `<temp>/MW3-Rebuild-originals/<name>`. That folder carries the mark
  `rebuild-original-download.txt`. A folder of that name without the mark
  that holds anything is never written to, and the fetch stops there. Each
  file's size and then its SHA-256 are checked. A bad one is logged
  (`original: <url>: <why>`), deleted and the next source tried.
- **The hand-over.** The good file goes to the game's own served setup
  exactly as PICK A FILE sends a pick: `--setup --look <file>`, then
  `--install <file>` with the game's folder. The game imports it by its own
  recipe. The launcher copies nothing into a game's folder.
- **Afterwards** the file is deleted, success or not, and the folder goes
  with its mark once nothing else is in it.
  - Success: `original: <key>: set up at <where>`, and the game's panel
    reads its status again.
  - Failure of any kind (every source bad, the setup refusing the file, a
    cancel, the folder not ours): `original: <key>: not set up: <why>`. The
    status line then says "<name> is installed. Its original game files
    could not be fetched (<why>): add them from GAME FILES on its gear.",
    which is today's flow.
- **A game run on its own** offers the same download from its find screen
  when its recipe names a catalog (re-setup-kit DESIGN.md, Original files).
- Checked: `sdl_program_launcher_gets_a_games_original_files`. A stand-in
  game's release is served by file://. Its status lacks files until its
  setup is handed a file. The cases are a good source, a wrong sum, a cut
  file, a missing source then a good one, a setup that refuses the file, no
  source, the temp folder's name taken by someone's own folder, and a game
  that has its files already. Each checks that the temp file is gone.

### Offline (2026-11-27)

The launcher counts as offline when every game's repo was asked and none answered (each game's check `kFailed`).
Then only the installed games are listed, so nothing on the list needs the network to work. When nothing is installed
either, the list's place says "Can't connect to the internet, and no game is installed yet. Check your connection or
firewall, then press CHECK FOR UPDATES to try again." CHECK FOR UPDATES asks the catalog and every repo again, and the
first answer brings the whole list back. There is no sign-in of any kind. A check run never asks the network and so
never counts as offline; `MW3_LAUNCHER_OFFLINE` makes one, for shots and tests.

### A dropped download (2026-11-27)

`curl_download` (and the kit's `fetch_url`, which the games' own setup uses) tries again after 5 s when the connection
drops: curl's codes 6, 7, 18, 28, 35, 52, 55 and 56, or a file of known size that came short. Each try resumes from the
`.part` file's end (`--continue-at -`), up to 5 tries, as long as each try gets further. A server that will not resume
(curl 33, HTTP 416) starts it over once. The finished file is still checked whole (size and SHA-256) by the caller,
so a resume can never install a bad file. Test: `launcher_download_resumes_after_a_dropped_connection`, where a
stand-in server on this computer (plain http, allowed only for 127.0.0.1 under `MW3_DOWNLOAD_LOCAL_HTTP`) drops a 3 MB
file halfway.

### A game's needs (2.0.1, 2026-11-27)

DOWNLOAD on a game whose catalog `needs` names a game that is not installed
(the MechWarrior 1 Remake: MechWarrior 3 and Pirate's Moon) asks once, in
the launcher's usual box: "MechWarrior 1 Remake needs MechWarrior 3 and
Pirate's Moon. Install all three? Up to N MB.", INSTALL ALL or CANCEL
(confirm 9, `ask_needs`). Only DOWNLOAD asks; the panels and PLAY say what
is missing as before (MechWarrior 1, above).

- **Which needs.** Each key in `needs` whose game has no program here (not
  installed, not beside the launcher) and has a release for this computer.
  One without a release (offline, none for this platform) is left out and
  logged, and the game installs as before.
- **N** is the sum of the job's parts (`needs_parts`): each missing need's
  package, its original files (the catalog's first `original`, else
  `compact`) unless its game files are already known (`need_where`, the
  same folder `--with` would give), then the game's package and, on a first
  install, its own original files. "Up to": a need's setup that finds a
  copy (a disc in a drive, an install) skips their download.
- **INSTALL ALL** starts one job (one bar, `launcher_job.h`) and queues
  the needs, then the game, on `update_queue_`; each is installed by
  `install_game_now` exactly as its own DOWNLOAD would (its package, then
  `get_original` when its status says its files are missing), into its
  usual folder (the place screen is not asked). `got_original` goes on to
  the next in the queue after MechWarrior 3's or Pirate's Moon's files are
  put in use; MechWarrior 3's notes screen waits for PLAY (`mw3_ready`,
  still unseen) while the queue runs.
- **CANCEL** during the job, or a failure, stops the queue there: what is
  installed stays, the rest is not started (`r.cancelled` clears the queue).
- **The Remake's status** is then asked with the new copies (`--with`),
  as `find_mw1` does after any install.
- Checked by `sdl_program_launcher_installs_what_a_game_needs` (a stand-in
  game needing another: the box's words and total, the need installed and
  set up first in the same job, not pressed, CANCEL during the need's
  original files). The MechWarrior 3 and Pirate's Moon branches of
  `got_original` (use_install, use_pm) are not run by a check.
- `MW3_LAUNCHER_NEEDS_GO` (checks): INSTALL ALL pressed.

### Update counts (2.1.0, 2026-11-27)

GitHub's download counts hold every update as well as every new install, so
the launcher reports each finished update once, and new installs are
GitHub's total less the updates fetched from GitHub.

- **What is sent.** One request, after a game's UPDATE (UPDATE ALL's too)
  has installed and passed its checks, and after the launcher's own update:
  `GET <counts>?kind=update|self&game=<catalog key, or launcher>&from=<old
  version>&to=<new version>&launcher=<this launcher's version>&src=github|mirror`.
  `src` is the home the package actually came from (`Fetched::github`:
  GitHub's list, `Release::first_home`, served it), since mirror downloads
  are not in GitHub's totals. Each value is cut to the site's
  `[A-Za-z0-9._-]{1,40}` (another character becomes `_`). Nothing else from
  the launcher: no name, no account, no machine id, no file paths. The site
  keeps counts only.
- **When.** A first install (DOWNLOAD) sends nothing; neither does INSTALL
  FROM A DOWNLOADED FILE. A game's update is sent from `finish_work` on
  its own thread (`count_update`), 3 s at most, never retried, its answer
  ignored; a failure is one log line. The launcher's own update is sent by
  the new launcher: the old one writes `launcher-updated.txt` in its data
  folder before the restart (on Windows the new Setup runs after it quits,
  so "installed" is only known there), and the new one takes it at its
  first start (read and removed, so never twice) and sends it once a
  catalog naming counts is in use, or drops it after the start's catalog
  fetch when none does. A note whose `to` is not this launcher's version
  (a Setup that did not finish) is dropped.
- **Where.** The catalog's top-level `"counts"`
  (`https://reverse-engineered.com/launcher/hit`), https only. A newer
  fetched catalog's value wins over the built-in's, none included, so the
  site can stop it without a release. Older launchers ignore the field.
- **Off.** `"send_update_counts": false` (or 0) in the player's
  `launcher-settings.json` (docs/launcher-config.md) sends nothing.
- **Launcher starts.** Every `curl_get` (the catalog fetch, its `.sig`, the
  releases lists) sends `User-Agent: MW3-Rebuild-launcher/<version>`, so the
  site counts the catalog fetches by launcher version.
- Checked by `sdl_program_launcher_counts_updates` (a stand-in server:
  update from GitHub and from the mirror, install, switch off, no field,
  the new launcher's note and one for another version, the User-Agent) and
  `sdl_program_launcher_takes_a_catalog_while_open` (a real self-update's
  note taken by the relaunched launcher). `MW3_COUNTS_TO` (checks) sends to
  a local server in place of the catalog's address.

### The Mac's disk image (2.0.1, 2026-11-27)

A game's package on the Mac is a disk image: attached read-only out of
sight (`MW3-Rebuild-game-mount-*` in the temp folder), its app copied out,
detached. The first detach after macOS sees a new app took 10.6 s (its
first look holds the volume), so the bar sat at 100%. Since 2.0.1 the
install ends once the app is copied: `detach_soon` starts the detach in a
shell of its own session (`POSIX_SPAWN_SETSID`, so nothing that ends the
launcher's process group cuts it off), tried 3 times 5 s apart, which then
removes the empty mount folder. The launcher's own update (its
`-update-mount-*`) does the same, so the restart does not wait.

At each start `clear_old_mounts` looks at the temp folder for either name:
one from over 10 minutes ago (its name's number is the system clock since
boot, so also any from before a restart) is detached when still attached
and removed when empty; one made just now (another launcher's install), one
with files in it, or any other name is left alone. Checked by
`sdl_games_detaches_disk_images_behind_the_install` and
`sdl_update_installs_the_mac_app`.

## Where a game goes (2026-11-22)

"Everything can be installed in the same location the user picks", and
the saves "should probably stay in the normal user folder, for modding
purposes". So each game has a folder of its own that the player picks, holding
its program and its game files; pilots, saves, settings and mods stay in the
user folder (the write dir). Its music and converted movies go there too
("we can have it look inside the game folder for movies and music"):
`sdlfe::media_dir(install, write_dir)` is the folder above the install when it
carries the game's mark, else the user folder as before; the game mounts it
after the install (`music/`, `video/*.fmv`), and the conversions (the
launcher's and the game's first-run one), ADD FROM MY DISC and a disc
install's music write there. A rip the player put in the user folder's
`music` still counts. Moving an install into a game's folder later converts
the movies again, once, there.

- **A card's first DOWNLOAD** asks where (confirm 7: CHANGE FOLDER, then
  DOWNLOAD, large, amber and focused in the middle, then BACK; INSTALL of the
  game files is laid out the same, `add_go`, 2026-11-24), the default being the games folder above. The program goes under
  the picked folder as under the games folder (`<folder>/<key>`), and the
  folder is kept in mw3.cfg as `games_<key>=` (mw3, pm, mw1), read at every
  start (in a game's own setup too). UPDATE goes where the game is, unasked.
  INSTALL FROM A DOWNLOADED FILE still installs into the games folder and forgets a picked
  one (its unpacking is staged inside the games folder, and a rename does not
  cross drives).
- **"Use these game files?"** has CHANGE FOLDER beside INSTALL, in the
  launcher and in each game's own setup (`mw3 --setup`, `mw3pm`). The game
  files go into `<folder>/install` (mw3.cfg `install=`, `pm_install=` as
  before), the default being the folder its card's DOWNLOAD picked, else the
  user folder's `install`. The line under the find says where, fitted to one
  line.
- **The game folder's own files first** (issue #10, 2026-11-23). A player put 1.3 on E: while an older
  rebuild's copy of the game files stayed on C:, named by mw3.cfg's `install=`; the launcher used C:. The
  lookup now tries `<the card's folder>/install` (`games_mw3=`) before `install=`; when the files are only
  elsewhere they are offered as a find is ("Use these game files?", INSTALL copies them into the game's
  folder), never used in place, and the old copy is never touched. mw3 started on its own does the same
  (`sdlfe::own_game_files`: the nearest folder above the program carrying the game's mark). Test:
  `sdl_program_launcher_takes_the_game_folders_own_files_first`.
  Pirate's Moon and MechWarrior (1989) the same (2026-11-24: "they should get the latest copy"; found
  by MW1's lead in the shared lookups): Pirate's Moon's `<its folder>/install` (`games_pm=`), then its
  user folder's `install`, before `pm_install=`; MW1's `mw1/game` in the write dir before `mw1_game=`.
  Ours found, mw3.cfg follows it ("<game> in our folder X, not Y: ours in use"); with none of ours, the
  setting is used as before. Tests: `sdl_program_launcher_starts_pirates_moon`,
  `sdl_program_launcher_starts_mechwarrior_1` (their own-copy-first parts).
- **Whose folder.** A picked folder is the game's when it is empty or carries
  this game's mark (`rebuild-game-folder.txt`, `game=<key>`), else a folder
  named for the game inside it (`MechWarrior 3`, `Pirate's Moon`,
  `MechWarrior 1 Remake`): a drive's top or someone's folder is never filled
  loose. Before anything goes in, the folder is claimed
  (`sdlfe::games::claim_game_folder`): made when missing, given the games
  folder's mark and the game's; one holding anything else is refused and
  nothing is written. `game_folder_in` and `claim_game_folder` are in
  `sdl_games`; they move to the setup kit with the setup screen.
- **The headers.** Each setup screen names the game it sets up, with its badge
  (`kHeaderNames`: MECHWARRIOR 3, MECHWARRIOR 3: PIRATE'S MOON, MECHWARRIOR 1
  REMAKE, MECHWARRIOR 1 for the 1989 game's PLAY ORIGINAL), never the
  launcher's title (the kit is game-agnostic); the metal bands stay. The
  cards, the launcher's own update and a checked download keep the launcher's.
  Logged as `header <title>`.

Checks: `MW3_LAUNCHER_INSTALL_TO=<folder>` (the card INSTALL's CHANGE FOLDER
pick, with `MW3_LAUNCHER_GAME_INSTALL`), `MW3_LAUNCHER_FILES_TO=<folder>` (the
game files' pick); shots `--launcher-screen place`, `found` (`mw1found` and
`mw1origfound` until b5). Test: `sdl_program_launcher_finds_the_game_then_plays` (CHANGE
FOLDER into a folder holding someone's file; someone's own `MechWarrior 3`
folder refused), the screens test and PM's GAME FILES test (the headers).
