# The program: mw3

`mw3` is the rebuild as a program: the original's own start-up and main loop
(startup.md, game-loop.md) running on SDL3. Everything the start-up reaches
that is ported runs; everything that is not is a hook that does nothing,
logged once with the original's address. It is the place the remaining
modules plug into as they are ported. `mw3view` stays the development viewer.

## Building and starting

Needs SDL3 and SDL3_ttf (`brew install sdl3 sdl3_ttf`); SDL3_sound
(`brew install sdl3_sound`) adds .ogg, .flac and .mp3 music (without it,
.wav only). CMake builds `build/mw3` when SDL3_ttf is found.

```
build/mw3 [install-dir] [options] [-nofmv] [-seasick] [-cdN]
```

- **The install**: the folder holding `Mech3.exe` and `zbd/` (MW3 1.2), as a
  path argument, `--install DIR`, or `install=DIR` in `mw3.cfg` in the write
  dir; the build's default is the repo's `game/mw3-1.2`. It is mounted
  (assets-layer.md); `--assets DIR` (or `assets=DIR` lines) mounts folders of
  replacement files in front of it.
- **The write dir**: saves and settings, never the install. Default
  `SDL_GetPrefPath("MicroProse", "MechWarrior 3")` (on macOS
  `~/Library/Application Support/MicroProse/MechWarrior 3/`), or
  `--write-dir DIR`. It is searched before the mounts, so what the game wrote
  wins.
- **The original's switches** (`-nofmv`, `-seasick`, `-cdN`) go on to the
  ported command-line parser as the exe's command line.
- `--window WxH` (default 1280x960: the game's 640x480 doubled),
  `--nosound` (a silent DirectSound stand-in), `--hidden`, `--verbose` (lists
  every unported function reached, with call counts, at exit; from the
  menus, also the player's mech 30 frames into the mission: its weapons,
  sinks, engine, speed and centre torso armour),
  `--frames N` (quit after N frames).

Plain `mw3` runs the original start: the constructor, InitInstance, the
engine start (the `zXxxInit: PASSED` lines), the Intro state (movies hooked,
so it moves on at once), then the Main state and its main menu (ported,
mainmenu.md: the background, the buttons and their ripples' state, the
logo, the hotkeys; the ripple's zRender drawing 0x00558dd0 is a hook, so no
ring shows). Its buttons reach the menus they open.

**A campaign from the menus** (campaign.md section 7, item 5): NEW CAMPAIGN
opens the pilot screen (type a name, Enter, ACCEPT); the Load state then
builds the ported briefing (brief_c11: the voice, its map, the buttons) and
loads the mission (at once: FIX (A), mission-load.md); LAUNCH (or L) builds
the pilot's mech with its own loadout and goes on to the Core state, c1
mission 1. The Load state's game is the chosen pilot's campaign (or training)
game, through the launch's stand-in (`GameEngine::load_state_enter`); its
briefing's other buttons (Mech Lab, Salvage, the reallocation list) and the
map's background picture's mission map object are hooks. `build/mw3 -nofmv`
from the repo root after a build; `--window 1280x960` is the default.
Once a mission is won, Exit Mission (Ctrl+Q) fades to the debriefing (the
ported screen: CONTINUE, REPLAY, ABORT; its results, lance cards and
salvage rows are hooks, campaign.md section 3), and CONTINUE goes on to the
next mission's briefing.

## Debug starts (our addition)

While the menus are not ported, the debug starts take the place of the state
after the intro (the app's +0x1dc, where a lobby launch puts the Load state).
They run inside the original main loop, after the original start-up.

- `mw3 --mission c1m1` (`c1` is mission 1; `c2`, `c3`, `c4`, `c4b`, `t1`;
  `--pos X Y Z`, `--look YAW PITCH` in degrees place the camera):
  **through the original's states** (mission-load.md): the pilot's game
  answers a campaign mission (type 1, the number, the directory's index), the
  Load state runs, its mission load (0x00490880 -> 0x00490ab0 -> 0x0048faf0,
  ported: the interp scripts read gamez.zbd, the textures, the view,
  animations, world.zrd) prints its progress lines, then the Core state
  (queued in place of the briefing's Launch button) runs the mission frame
  and draws the world through its Direct3D path from `camera1`. Mechs, AI,
  weapons, the HUD and the briefing screen are hooks logged once.
  `--shot`/`--frames` count the Core state's frames.
- `mw3 --fly c1` (also `c1m2`: world block m2, mission number 2; `c4b`,
  `t1`; `--variant BLOCK`, `--pos X Y Z`, `--look YAW PITCH` in degrees): a
  state of our own, the world loaded as `mw3view --real` loads it (`src/frontend/debug_world`: the
  ported gamez.zbd reader, texture groups, world.zrd parameters), drawn every
  frame by the ported render path on the SDL GPU. A free camera: W/S/A/D, Q/E
  down and up, arrows turn, Shift fast, right mouse button looks, P prints the
  camera as arguments, Escape quits. Sound: zSound's frame each frame as the
  Core frame calls it (0x0056dfb0(0, 1)), the `common` and `mech` sets,
  `mech_powerup` at the start (as the Core state's enter plays it), the
  world's sound nodes and the listener wired to the scene walk; the music
  track picked as the mission start picks it (0x00480df0: `-cdN`, else by
  mission number on a 2..5-track disc). No mission load, animations, mechs,
  AI, weapons, HUD or weather.
- `mw3 --dialog dialog.zrd MAINMENU` (any dialog of twod.md section 3): the
  dialog drawn by the ported zTwoD on the SDL host, every BUTTONS and
  PRIMITIVES entry bound by name (check buttons when they have CHECKED, texts
  when they have TEXT), the window's mouse given to Window::update (the
  buttons do nothing: their screens are not ported).
- `--shot FILE.bmp [--shot-frame N] [--shot-any]` saves frame N (default 30;
  with `--shot-any` every frame shown counts, the fade's too) of either
  and quits, with the window hidden; `--shot-series K` also the K frames
  before it.
- For checking (ours, from 2026-10-25): `--tap DIK@N[:M]` presses a key as
  an SDL event; `--click-at X,Y@N` (the plain start) clicks at a game pixel;
  `--click N:M`, `--walk`, `--fire-group G`, `--objectives N` in a mission;
  `--briefing c1m2` (the plain start) saves a pilot "Briefing" whose newest
  campaign stands at that mission and clicks CAMPAIGN, the pilot and ACCEPT,
  so the Load state shows the mission's own briefing (frontend-plan.md item
  0). Its campaign is the pilot's new one stepped on mission by mission
  (0x00434e00), so the store, the slots and the field bases are a real
  campaign's (without the earlier missions' salvage). `--win N` wins the
  mission N frames in as a player does, by Exit Mission once won (the end
  screen, its fade, the debriefing's Win State and its save); with
  `--verbose` the program prints the pilot's game as the Win State saved it
  and as each briefing finds it once its mission has loaded
  (`tools/analysis/savecheck.py`, save.md), and each state the app moves to
  by name (`tools/analysis/opcheck.py`: an operation's end and the
  campaign's). `--lab-mech N` hands the lab's 3D view chassis N (0..18) once
  the lab is up, the slot keeping its mech (mechlab.md, the 3D view).

## What runs ported

The app's start-up and loop as written, checked against the exe by the app
tests (game-loop.md, Testing): the constructor 0x0047f7a0, InitInstance
0x00480000, the engine start 0x00480570, the video open 0x00480970, the
main window 0x00482d10 and its command line, `zGameApp::Run` 0x0058fd70 with
the state commands, every state (Intro, Main, Attract, Pause, Exit, Core,
Load, Pre Lab, Salvage, Win, the key list), the stop 0x004808a0 and
ExitInstance 0x0047fa10.

The quit: the Exit state runs the stop (0x00482060), then the loop's
WM_QUIT returns ExitInstance. A quit outside it (the window closed, a
debug run's `--frames`) returns through ExitInstance alone, as the exe
does, and the world stays until the process ends. mw3 once ran the stop
after that; zSound had freed its sounds by then, and the world's sound
nodes wrote into them (mission-plan.md item 2).

Hooks wired to ports by `mech3::GameEngine` (`src/mech3/app_engine`):

| Hook | Port |
|---|---|
| the interp script object 0x00410700 | `mech3::interp_create` (interp.zbd, DamageScale) |
| zReader init / archives 0x00556170, 0x00557320, 0x005573d0 | `zrdr` |
| zSound init, load, sets, play, frame, pause snapshot, reset, shutdown (0x005700f0, 0x005705e0, 0x0056f4b0, 0x0056f520, 0x0056f530, 0x0056f6f0, 0x0056e3d0, 0x0056e9f0, 0x0056dfb0, 0x0056eb10, 0x0056f0a0, 0x0056f190, 0x005716a0, 0x005715d0, 0x00570510) | `zsnd`; its engine variables SoundLOD and SoundVolume are the options' |
| the CD (CD_TRACKS from sounds.zrd, 0x00572450 / 0x005728f0) and the music calls | `zsnd_cd`, `mech3_music` |
| the clock 0x00575a00, its start 0x005759a0 | `ztimer::update`, `ztimer::init` (ported here) |
| global light 0x00526a70 / 0x00526a80, lights 0x00542bb0 | `zmod` |
| the options and their hooks (sound volume, music volume, global light, video mode) | `mech3/options`, `zsnd`, `mech3_music`, `zmod` |
| messages 0x00576040, 0x00575fb0, 0x00575fd0 | `zutl_msgdll` over Mech3Msg.dll |
| zTwoD's outside calls (`ztwod::Host`) | `SdlTwodHost` (below) |
| the settings store 0x00598210, 0x00597d80, the loader 0x0040c420, the save 0x0040cb20 and the store's free 0x00598270 (the app's destructor 0x0047fab0, `Mech3App::destroy`) | `zreg`, `mech3::load_options` (settings.md); the registry is the write dir's `settings.reg` (`platform::RegFile`) |
| the mission load 0x00490880, 0x00490ab0, 0x0048faf0 (and 0x004902a0, 0x00490630, 0x00484c50) | `mech3::Missions` (mission-load.md); its hooks in `app_mission.cpp`: zInterp, the options, zImage, zClass's and zModel's resets, node lookups, animations, world.zrd, the readers, sounds; the view's camera run is the scene walk |
| the briefing screen 0x00405230 (the Load state's) and its calls: the load's fraction and lines 0x004050d0 / 0x00405120, the script's start, its cursor 0x00409ec0 / 0x00409eb0, the voice's play, state and cue markers, LAUNCH's 0x00491150, the cleanup 0x00491780, Replace, the nav points [0x00609980]; 0x00405160's MAP image stood in for (the image only) | `mech3::BriefingScreen` (campaign.md section 2) in `app_menus.cpp`; the pilot's game into the launch's stand-in (`load_state_enter`), its loadout the slot's |
| the save after a mission 0x00478f10 (the Win State's, the launch's) | `mech3::Pilot::record_mission` on the pilot the pilot screen chose (`program_record_mission`; a debug start's stand-in is logged); the pilot module's message box is the engine's (`wire_pilot_hooks`), zSave's world save 0x005a8a70 logged (campaign.md) |
| the sensors 0x004a8cd0 (each Core frame) and the entity sort 0x004d5ca0 (the mission start's resume 7) | `mech3/sensors` (ai.md section 15) over `app_sensors.cpp`: zEntity's list is the port's, the mechs put on it by the build's 0x005a7f50; the Core state's mission-start fade (0x0048ee70) runs the ported fade state, whose end gives resume(7) |
| the fade state's screen calls: the grab 0x00579500, the draw 0x0055b1f0, the free 0x00517cc0; the colour 0x00558540 | `GameEngine` (`app_engine.cpp`): the still is a copy of the 16-bit buffer, drawn back whole (FIX (A)); the colour is the port (`zrndr::tint`) |
| the Direct3D frame: the lock 0x00576d00 / 0x00576d60, the scene nesting 0x00579af0 / 0x00579b10, the quad [0x0072f3ec], the software finish 0x00558610 | `zvid_dd`, `zvid_ddd3d`, `zrndr_tint` (render-frame.md); the surface Lock / Unlock on the 16-bit layer and `Screen2D::overlay_16` (FIX (A)) |
| the player's view 0x0040aca0 (its update from the player's tick, the attach 0x0041ce30, the camera switch 0x0040ae70) and the mech types' cameras 0x00430e70 | `mech3/player_view` over `app_view.cpp` (view.md): camera1 follows the target's camera; the app code reads the view's mode and target from its block, copied after each change |
| zInput: the command map 0x0051c630, start and stop 0x0051b0a0 / 0x0051b170, the activate and deactivate, polls 0x0051b360, key states 0x0051a9b0, resets, the mouse's use, centre, position, acceleration and screen, joystick and force feedback 0x0051dc10 / 0x0051ddb0 | `zin` (input.md), on `SdlInput` and `SdlZinHost` |

Memory of modules not ported (the `peek`/`poke` the app code uses for
their fields) is a sparse byte map: what is poked reads back, the rest is 0.
Objects the ports hand out (zSound objects, voices, snapshots, the interp)
get 32-bit handles from a table.

Still hooked (logged once each, return 0), the main ones: the movies (zFMV
0x00598cc0..), the main menu's menu objects (0x0048a6c0 ..) and App::Push
of them (0x00590400), the pilots, mechs, maps and missions (0x004788b0,
0x0040f410, 0x004583b0 ...), the mission's modules (mission-load.md, Open), zEffect, zClass's and zModel's
module starts, mech3's controls (0x00411150, 0x004120a0), the network. `--verbose` prints the full list of a run.

## The platform underneath (FIX (A), fixes.md)

`src/platform/platform.h` names what the game needs; `src/frontend/sdl`
implements it:

- **Window** (`SdlWindow`): the `Mech3Class` frame. Made hidden before the
  engine starts so the GPU device and Render3D exist; the game's
  `CreateEx` sizes and titles it ("MechWarrior 3 1.2", message 7 and the
  version), `UpdateWindow` shows it. The game's 640x480 is stretched over the
  window.
- **Message pump** (`SdlPump`): SDL's event queue for `zGameApp::Run`. Focus
  gained and lost are the frame's OnActivate (0x00590c00), so the loop idles
  while the window is in the background, as the original's did. The window's
  close posts the quit. A music file's end is MCI's notify (0x00590740).
- **Clock** (`SdlClock`): `GetTickCount` counted from the start, `Sleep`.
- **Input** (`SdlInput`): DirectInput as zInput opens it (0x0051b0a0): the
  keyboard buffered as `GetDeviceData` gives it (0x80 events of
  DIDEVICEOBJECTDATA with DIK codes, overflow reported as
  DI_BUFFEROVERFLOW, auto-repeat not reported, nothing while unacquired), the
  mouse as DIMOUSESTATE (relative counts since the last read, the wheel in
  120s, buttons left, right, middle, X1), the first joystick as DIJOYSTATE2
  (axes 0..65535, DirectInput's default range; hats as POV hundredths of a
  degree, -1 centred; 128 buttons). SDL scancodes are USB usages; the DIK
  codes are the PC keyboard's set-1 scan codes, mapped key for key by
  position (`dik_from_scancode`). zInput (input.md) runs on it: the
  cooperative level (the mouse exclusive and foreground: SDL's relative mouse
  mode while acquired, so the cursor is hidden and held as DirectInput held
  it; taken away with the focus, reads then give DIERR_INPUTLOST), motion
  scaled to the game's 640 pixels across the window, the joystick's
  capabilities, axis names, ranges and dead zones. `SdlZinHost` gives zInput
  its Win32 calls (the client area is the 640x480 frame, characters from
  SDL's keymap). The debug states read DIK codes directly; the dialog one
  polls zInput, so its dialog gets zInput's mouse.
- **Render3D** (`SdlRender3D`, render.md section 4) for the 3D path, and
  **Screen2D** (`SdlScreen2D`) for the menus: the 16-bit back buffer the
  2D blits draw into (locked by 0x00576de0, flipped by 0x00576ea0) is
  converted to a texture and drawn over the window by Render3D. Render3D
  keeps one back-buffer texture a frame (the window's pixels): every scene,
  2D layer and colour quad of the frame draws into it in order, the frame
  clear (0x005777f0) clears it once, and the flip shows it (or saves it for
  `--shot`). In Direct3D the back buffer's lock (0x00576d00, render-frame.md)
  hands the 2D code the 16-bit layer filled with a key colour (0xf81f), and
  its unlock lays the changed pixels over the 3D frame
  (`Screen2D::overlay_16`), so the HUD and the fade's still go on top of
  the world as on the locked DirectDraw surface.
- **2D host** (`SdlTwodHost`, twod.md section 6): GDI's text on SDL3_ttf
  with the game's own `arial.ttf`, `impact.ttf` and `lucon.ttf` ("Lucida
  Console"), found through the mounts. GDI's rules where the game can see
  them: a negative height is the character height, a positive one the cell
  height, sized as GDI does (VDMX, hinted advances, no kerning; twod.md
  section 6, Text widths); text is drawn without antialiasing (GDI on a 16-bit DIB), so the
  key-colour blit keeps clean edges; `DT_CALCRECT`, line breaks,
  `DT_WORDBREAK` in the box, `DT_CENTER` / `DT_RIGHT`, `&` prefixes; opaque
  and transparent background modes; `GetTextMetrics` (internal leading the
  cell height less the em). Text is Windows-1252, converted to UTF-8 for
  SDL_ttf. fonts.zrd's WINDOWS_FONTS are read by name for zImage's named
  fonts (face, height, colour or two, shadow, align, weight). Images come
  from the image library (`zimg::fetch_image`, refcounted by name); screen
  grabs copy the 16-bit buffer; the mouse is zInput's (0x0051b7b0).
- **Sound** (`SdlSoundOutput`, sound.md section 7), or
  `platform::SilentSoundOutput` with `--nosound` and in tests.
- **Music** (`SdlMusicOutput`): `zsnd_cd` asks for `music/trackNN.(ogg|flac|
  wav|mp3)`; `mech3::MountFiles` looks it up in the write dir, then every
  mount, so the tracks can sit in the write dir's `music/` or the install's.
  The file is decoded as it plays (SDL3_sound), gain from the CD volume.
- **Settings** (`platform::RegFile`, settings.md): the registry's two keys
  as regedit's text in the write dir's `settings.reg`, written when the app
  ends; zInput's bindings are a store entry (`SdlZinHost::settings_block`).
- **Alert** (`SdlAlert`): `MessageBoxA` and the game's own box 0x004ac150
  (its dlgMessage.zrd box is the 2D front end's, not wired yet) as SDL's
  message box with the same texts and buttons.
- **Mech3Msg.dll** (`zutl_msgdll`): read as data. The message table is the
  DLL's RT_MESSAGETABLE resource; `ZLocGetID`'s `{name, id}` table is found
  by following the export into its binary search (`mov ebx, table` ...
  `cmp ebx, end`; 1,577 entries in 1.2's DLL). Missing, the program says so
  and exits, as the original does.

## The engine's reports (our addition)

The exe's error report (`zerr_report`, 0x0049bfd0) is empty in this build,
so the original never shows its asserts and warnings. The rebuild prints
them to stderr as `mw3: report <level> <file>:<line> <text>` (not when
quiet), with the text filled the way the exe's calls pass their arguments
(`zerr::reportf`; the modules' own printers in `app_mission.cpp` the same).
Where the exe passes nothing for a `%s` (weilerIntersect's error, zWeapon's
"Failed to read %s") the format prints as written; ZAiTarget's animation
name comes from 0x0050b620, empty in this build (the exe passes what EAX
holds), so the rebuild names the animation. `reports_fill_their_formats`
checks it. A mission start prints five: the quit box's dialog lacks the
YES, NO, ABORT, RETRY and IGNORE buttons the message box binds.

`MW3_TRACE_UNPORTED=<key>` (ours, not on Windows) prints a backtrace for that unported
key's first 8 reaches, to find which hook a census entry comes through.

## Tests

- `build/mw3tests` (`tests/test_program.cpp`): Mech3Msg.dll read (message 7,
  0x14d, the skin keys through `zutl::localize`), the mounts and write dir
  order, the music tracks found through the mounts and the disc's track
  count, and the whole start-up with fake platform parts: InitInstance,
  `zGameApp::Run` through Intro to the Main state, the title, Direct3D
  chosen, the movies reaching their hooks, the main menu built from
  MAINMENU (its buttons bound, the version line), frames flipped.
- `build/mw3sdltests` (`tests/test_sdl_frontend.cpp`): the DIK table (known
  codes, no two keys sharing one), the keyboard buffer (order, auto-repeat,
  overflow, unacquired), the mouse counts, the GDI text on the game's fonts
  (extents, metrics, two lines, colour only where the glyphs are, opaque mode,
  GDI's cell height, word breaks, `&` prefixes).
- `tests/differential/test_zinput.py`: zInput against the exe (input.md,
  Testing).
- `tests/differential/test_mission_load.py`: the mission load against the
  exe (mission-load.md, Testing).
- `tests/differential/test_settings.py`: the settings loader and the store
  against the exe (settings.md, Testing); `mw3tests` reads a regedit export
  and saves through `platform::RegFile`.
- `tests/differential/test_ztimer.py` now also runs the clock start
  0x005759a0 on a random frame against the exe (break tests: the total not
  zeroed, the tick product rounded differently: both caught).
- The app tests (`test_app_loop.py`, `test_app_options.py`,
  `test_app_states.py`) are unchanged and pass: `GameEngine` only overrides
  hooks; the base `Engine` they test is as it was.

Checked by hand (2026-10-20): `mw3 --mission c1 --shot` draws the world
(c1's river and canyon under the hormain sky), `--dialog dialog.zrd MAINMENU`
draws the main menu with its TrueType labels, a visible window runs 150
frames of c1m2, a test track in the write dir's `music/` plays and loops
through the MCI notify path.

## Open

- mech3's controls setup (the default keys of the 0xa7 commands, 0x00411150,
  and the controls file, input.md Open).
- The screens after the main menu (mainmenu.md, Left) on zTwoD and the
  SDL host; the game's message box 0x004ac150 on dlgMessage.zrd; the
  ripple's drawing 0x00558dd0.
- The mission's hooked modules (mission-load.md, Open): the player's mech,
  AI, weapons, effects, HUD; the briefing screen and its Launch button.
- Movies (zFMV, Indeo 5).
- The cursor (0x00579840) is not shown or hidden; the menus draw their own.
- Shaders are MSL only (Metal); SPIR-V and DXIL come with the Windows build.
