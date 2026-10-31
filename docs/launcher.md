# The launcher and the first-start setup (ours; design, 2026-10-30)

Not original code: the rebuild's own front door, so a player installs,
starts and plays with no command lines (Owen, 2026-10-30). The design
sketch's screens: `assets/launcher/` holds the art (with its prompts);
the mockups were handed over in the outbox (launcher-sketch).

## The flow

`mw3` with no check flags (not `--hidden`, `--frames`, `--shot`,
`--campaign-chain`, `--mission`/`--fly`, `--movie`, `--dialog`, nor
`--no-launcher`) runs:

1. SDL, the write dir, mw3.log (as now).
2. The window, made once and kept for the game: fullscreen at the desktop
   size, the 640x480 frame fitted as the menus are (sdl_fit), so the
   launcher looks the same in a window, at 4:3 and at 16:9.
3. The install, quietly: mw3.cfg's `install=`, then the finder's places
   (`sdlfe::install_candidates`), each through `setup::check_install`.
   No message box any more: the setup screen replaces the Browse dialog.
4. **The launcher screen** (mockup 1). Entries:
   - **MechWarrior 3**: "Ready: version 1.2, <folder>", or "Not set up yet"
     (a click opens the setup screen), or for 1.0 data "Ready: version 1.0
     (Setup can apply the 1.2 patch)". A folder too long for the plate is
     shortened from the middle by its drawn width ("Ready: version 1.2, "
     kept, more of the folder's end; `fit_middle`, text_fit.cpp): a long
     path ran past both sides on Owen's PC (lead, 2026-10-31).
     `launcher_long_path_fits_the_button`.
   - **Pirate's Moon**: greyed, "Coming later".
   - **MechWarrior (1989)**: only when the MW1 port is found beside ours
     (below).
   - **Setup** and **Quit**.
5. MechWarrior 3 chosen: the launcher's textures are freed and the start
   goes on exactly as today in the same window (mounts, data version,
   movies helper or import, `run()`), so the intro movie is the next
   thing seen.

## The setup screen (mockups 2 to 4)

A status line, then four plates and Back:

| plate | shown | what it does |
| --- | --- | --- |
| Find my installed MechWarrior 3 | always | the finder again, then a folder/Mech3.exe picker; `setup::check_install`; saved by `sdlfe::save_install` |
| Install from my disc or disc image | always | the discs `setup::find_discs()` sees as plates of their own, plus "Choose a .iso or .cue file..."; `setup::probe_source` first ("MechWarrior 3 1.0 disc image (.cue), with 2 music tracks: about 711 MB to install." with Install / Back), then `setup::install_from_disc` into `<write dir>/install`, music into `<write dir>/music` for a .cue |
| Apply the 1.2 patch | when the data is 1.0 or a mix | a file picker (the ZIP) or a folder picker; `setup::apply_patch` into `<write dir>/patch12` |
| Add the music from my disc image | always (offered again after an install from an .iso, which has no CD audio) | a .cue picker; `setup::add_music` |

Each operation runs on a worker thread; its `Progress` (stage, fraction,
message) goes to the screen under a mutex: the bar, the stage and the
current file (mockup 3). Cancel makes the callback return false
(Mw3 dev's calls stop within a file and change nothing). The result's
`summary` is shown in a line at the top of the launcher, green or red
(mockup 4); `warnings` under it; `detail` only in mw3.log. The first
start's notes (release-0.1's box) move here: the 1.0 note is the
MechWarrior 3 entry's line and the Apply plate; the music note a line on
the setup screen.

File and folder pickers are SDL's (`SDL_ShowOpenFileDialog`,
`SDL_ShowOpenFolderDialog`, as `sdl_install.cpp` waits on them now),
filtered to .iso/.cue, .zip, or Mech3.exe.

## Drawing it

- A 640x480 16-bit (565) canvas each frame, as the game's 2D path: the
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

## MechWarrior (1989)

Elthial's MechWarrior SDL port (github.com/Elthial/Mechwarrior-sdl-port;
MIT for its own code, credited in the launcher's line and
THIRD-PARTY.txt; we ship none of it). Shown when `mechwarrior.exe`
(`mechwarrior` elsewhere) is beside `mw3` or in a `MechWarrior1989\`
folder beside it. It needs the player's MW1 folder (`BTECH.EXE`): found
once by a folder picker and kept as `mw1_game=` in mw3.cfg. Started with
`SDL_CreateProcess` as its own script does (`mechwarrior.exe <game dir>
<capture dir>`, the capture dir `<write dir>/mw1`); the launcher's window
hides until it exits, then comes back.

## Built (2026-10-30, mac5/launcher-ui)

`frontend/sdl/launcher.cpp` (`sdlfe::run_launcher`), called from
`mw3_main.cpp` for a player's plain start (not `--hidden`, `--frames`,
`--shot`, a chain, `--mission`/`--fly`, `--movie`, `--dialog`, `-nofmv`, a
named install or `--install-from`/`--patch`; `--no-launcher` skips it):
- Its own SDL_Renderer window (fullscreen unless `--windowed`/`--window`),
  a 640x480 RGB565 canvas letterboxed into it; the game's window opens
  after it closes (one window throughout is still to do).
- The install found quietly: mw3.cfg's, `<write dir>/install` (the disc
  install's), the finder's places, a development copy; each through
  `setup::check_install`. None: the setup screen first.
- The main screen: MechWarrior 3 (ready, its version and folder, or "not
  set up yet"), Pirate's Moon greyed, SETUP, QUIT. MechWarrior (1989) is
  not wired yet.
- The setup screen: FIND (the finder, else a folder picker), INSTALL (a
  disc `find_discs` sees, else a .iso/.cue picker; `probe_source`'s line and
  INSTALL/BACK, then `install_from_disc` into `<write dir>/install`, a
  .cue's music into `<write dir>/music`), the 1.2 PATCH (enabled for 1.0 or
  a mix; a .zip picker, `apply_patch`), the MUSIC (a .cue picker,
  `add_music`), BACK. Each operation on a worker thread with the bar, its
  stage and file, and CANCEL; its summary on the main screen's line.
- Art from `assets/launcher` (beside the exe in a package, the repo's in a
  development build: `MW3_ASSETS_DIR`); the font `assets/launcher/font.ttf`
  when there is one, else a condensed system face (Bahnschrift on
  Windows, DIN Condensed on the Mac, DejaVu Sans Condensed on Linux): a
  placeholder, as is the title art's "MW3 REBUILD".
- The hover ripple is `zrndr::ripple` on the canvas; no sounds yet.
- Mouse, arrows/Tab, Enter/Space, Escape (back, or quit on the main
  screen).
- `--launcher-shot FILE [--launcher-screen main|setup|work]` draws one
  screen into a BMP; `sdl_program_launcher_screens` checks the three.

## Open

- The working title ("MW3 REBUILD" on the title art) and the font: Owen's.
- The calls are Mw3 dev's `mech3::setup` (game_setup.h, mac4/setup-api).
- Once the GUI-subsystem build lands, mw3.log is the only console.
