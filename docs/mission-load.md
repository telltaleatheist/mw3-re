# The mission load

Mech3.exe 1.2, `C:\zipper\Mech3\mission.cpp` (0x0048f400..0x00491de0),
ported as `mech3/mission` (orchestration) with the program's wiring in
`mech3/app_mission.cpp`. Checked against the exe by
`tests/differential/test_mission_load.py` (section 5).

## 1. Who starts it

The Load state (the briefing, game-loop.md) builds its screen once, then
calls 0x00490880 on the missions object at 0x006098d0. That makes an event
(+0x68, named "load"), starts a thread (+0x6c) on 0x00490900 at idle
priority and drops the game thread's priority to normal: the mission loads
**while the briefing plays**. The thread runs 0x00490ab0 and signals the
event; 0x00490930 polls it (WaitForSingleObject(.., 0)). The abort flag +0x70
is checked between the steps, so leaving the briefing can stop a load.

## 2. The world, 0x0048faf0 (ECX missions, the mission index)

A mission already up (+0x60 not -1) is kept when it is the same index, else
unloaded (0x00490480) and the load starts again. Then, in order (the
progress bar 0x004050d0 and its text 0x00405120 between steps):

1. The directory by index (0x00484c50: c1 c2 c3 c4 c4b t1, then lab, dev1,
   dev2, "none"). The interp script object gets the macro `USEZBD`;
   `zbd\mechlib.zbd` and its sounds (0x00484270); [0x005f4244] = 1 (scripts from
   the archive, interp.md); animation loading allowed (0x00509150).
2. **The texture budget** 0x004902a0: on a 3D card, its texture memory in MB
   ([0x0072f38c]; 1 if it cannot say), else 0; then capped by the
   TextureMemory option: TEXMEM_MAX no cap, `12MB`, `10MB` and `8MB` all 8,
   then 6, 4, 2.
3. **The texture sets.** "texture" at level 0 (2 under 4 MB, 3 under 8);
   the mech set "mechtex" on a 3D card, "mechtexS" in software, with flags
   0x11 under 4 MB or when InstallOptions has neither bit 2 nor 4. On a 3D
   card the macro `HARDWARE` and 0x005269e0(0). [0x006061cc] (HOLES) is set
   when the RGB choice or the app's +0x1b9 is on. The libraries open with
   0x005182a0.
4. "MechWarrior3 (PRODUCTION) (<version>)", the video driver and mode texts.
5. `support\<dir>size.gw` through the interpreter (node and model array
   sizes; the result is ignored), zClass's reset 0x004f8f70, zModel's pool
   0x00526800, 0x00535ed0(0.5).
6. **`<dir>.gs`** (it reads `gamez.zbd` with `GameZReadZBDFile`, sets the
   animation file, lights, fog: interp.md, gamez.md). If it cannot run:
   "Failed to Interpret: %s -- Mission Load Aborted" (zerr, empty in the
   release build), 0x00491780(1), and the load fails.
7. The HUD's image path, "Loading Textures (level/flags) RGB|PAL
   HOLES|NOHOLES", the textures loaded (0x005190d0), message 0x104.
8. The nodes `world1` (list 0xd), `camera1` (8), `window1` (0xe), `display`
   (0xf); three objects: [0x00608c84] (8 bytes, 0x0047f2b0 on the world),
   the view's window (0x3c, 0x004b52e0 on the window), **the player's view
   [0x005f4c8c]** (0x7c, 0x0040aca0 on the camera, the window and
   [0x00608c84]). The options get the window, display and camera; the view
   window's two rectangles both take the render rectangle (so its scale
   branch never runs), and the window node is sized and placed by it.
9. [0x009a17c0] = 0, 0x004e7b00, effects (0x0050d2c0, `effects.zrd`), the
   animations (0x005090e0 the world, 0x0050ae60 the post-load pass,
   anim.md, anim-postload.md), decals (0x00501ab0), weapons (0x00583880,
   `weapons.zrd`, weapons.md), the HUD (0x0040f7f0 with the display and
   window rectangles, 0x0040db60).
10. +0x60 = the index; "Creating Stealth": [0x00609a2c] from 0x004a4de0.

## 3. The rest, 0x00490ab0

The game's mission index (pilot's game vt+0x54), the mission's start
0x0048f400 (the game time from 0, Exit Mission bound, the cheats checked:
mission-frame.md section 2), type (vt+0x2c) and number (vt+0x4c). The reader archives 0x00490630: `zbd\<dir>\reader.zbd`, the
loose paths (`..\data\common\zrdr`, `..\mechs`, `..\data\<dir>\zrdr`,
`..\aipath`), the block's aipath and `zbd\<dir>\reader<block>.zbd`, where
the block is `m<n>`, `ia<n>` (type 2) or `mp<n>` (type 3) (0x00490590).
Then the world (above; failing, the setup fails), the archives again, the
mechs' HUD markers 0x00413e60, 0x004319a0, 0x00459e80, **world.zrd's block**
(0x00491de0, world-params.md), 0x004e7b40, the stealth entity, the InOut AI
data (0x004c3b80), `landmines.zrd`, 0x004b4ab0, **the mission's vehicles**
(0x0049e810, player.cpp, ported: mission-start.md; not the player's mech,
which the briefing's Launch makes, 0x00491150), "Saved Game Loaded" when the
game was restored.

By game type: the turrets (`aiTurrets.zrd`, when the app's +0x1b2) for
single player and training; the objectives (0x0045c2d0, `objectives.zrd`
block `m<n>` / `ia<n>` / `mp<n>`; multiplayer missions above 2 read
`mpinfo.zrd`'s `objectives`; training goes through the game's vt+0x40);
then **the start animation**: `start_single_player_m<n>`, `switch_ia_m<n>`,
`start_multiplayer_m<n>` or `start_training_m<n>` (0x0050b450), falling back
to the name without the number. Multiplayer with the pilot's +0xa8 sets up
the MFB stations (0x00490950: `mfb_station01`, renamed `mfb_station_%02d`).

Then the four voice sounds (`com_success`, `com_failed`, `obj_primary`,
`obj_secondary`), "Post Vehicle Fixup" (0x004d2e20, 0x004d31d0, 0x004e2120),
the AI lists reset, the keys reset, the HUD's 25 text lines cleared,
0x0040f040, 0x0040f890, +0x5c = 1 (up), the last AI list, 0x004d4b30,
"Ready To Launch", the start animation run (0x00508c10), the interp's
scroll list (0x005964c0(0)).

## 4. The port

- `mech3::Missions` holds the fields mission.cpp uses; every call out is a
  `MissionHooks` method named by address. `test_mission_load.py` drives the
  port through recording hooks.
- **FIX (A)**: no thread. The load runs inside 0x00490880 and the briefing
  finds it done: the world's textures go to the GPU, which wants the main
  thread. (fixes.md)
- `GameEngine` wires the hooks (`app_mission.cpp`): zInterp's `source` and
  macros on the script object, the options, zImage's libraries and loads,
  zClass's and zModel's resets (with the draw table and scene walk the
  engine start would set), node lookups, the window node's size, the
  animation hooks (puffers, sounds; zEffect's effect table is not ported, so
  a def naming an effect drops out), world.zrd's reader (wind, clouds and
  sun; weather does nothing), the reader archives, the sounds, the weapons
  (`zwep::init` with the weapon_tick object), the mission's vehicles
  (`mech3::player_setup`, mission-start.md: the AI mechs built on their
  controllers, ai.md section 12), targets.zrd and traps.zrd (0x004d2e20, 0x004d31d0, ai.md section 14), valves.zrd (0x004e2120, ai.md section
  13) and, for a debug launch, the
  launch 0x00491150 that builds the player's mech (mech-build.md). The
  frame and the cleanup that follow the load: mission-frame.md. The rest (effects, decals, the
  HUD, the stealth entity, the mechs' and AI's other set-up, objectives, the
  start animation) are logged once by address.
- The view's +0x04 (its camera run) is the scene walk on `camera1`
  (`zcls::render_world`), so the Core state's Direct3D path draws the world;
  the flip shows the 3D frame.
- **Debug launch** (our addition): `mw3 --mission c1m1` sets a pilot whose
  game answers type 1, the mission number and index, with a placeholder mech
  handle at +0x14 (the Core state leaves without one), makes the Load state
  the state after the intro, and after the load queues the Core state in
  place of the briefing's Launch button, after the launch 0x00491150 has
  built the player's mech (the Mad Cat in its first loadout, mech-build.md
  section 9). `--pos X Y Z --look YAW PITCH`
  places the camera (the view does not follow the mech yet). `mw3 --fly c1` keeps the
  old free-camera viewer.

Checked by hand (2026-10-20): `mw3 --mission c1m1 --shot` goes through the
Intro, Load and Core states and draws c1's world from `camera1`; c2, c3,
c4, c4b and t1 load to "Ready To Launch" and draw. The scripts' three
"no GFX data for cycled texture" reports on c1 are the original's too (its
cycled textures are made before the textures load; zerr is empty in the
release build).

## 5. Testing

`test_mission_load.py`: 240 cases, each through the world 0x0048faf0 and
the setup 0x00490ab0 in the emulator (x87 at 53 bits) and the port, every
call out stubbed and answered from one script: every mission index
(the six shipped and lab, dev1, dev2, none), game types -1..5, software and
hardware, card memory from none to 64 MB and odd sizes, the memory query
failing, every TextureMemory setting, InstallOptions with and without bits
2 and 4, the size and mission scripts failing, allocations failing, the
load aborted after any call, a mission already up (the same and another),
the start animation found with and without its number, the stations flag.
MFC's CString and sprintf are emulated in Python. Compared: the call
traces (strings as text, the progress texts with their numbers), the
results, the globals written ([0x006061cc], [0x005f4244], [0x009a17c0],
the view, [0x00608c84], [0x00609a2c]), the view window's rectangles and
flag, and the missions object's fields. 45,303 calls identical. Breaking
the port showed the test catches it, 14 of 14: the mech set by the RGB
choice, 12 MB capping at 12, the 4 MB boundary inclusive, HOLES without the
app's flag, no cleanup after a failed script, the abort not checked after
the weapons, the HUD's rectangles swapped, +0x78 cleared, mpinfo.zrd from
mission 2, no fallback animation, the sounds swapped, the loaded flag after
the abort check, the reader block ignoring instant action, the budget
keeping the larger.

## Open

- What each hooked step does: effects 0x0050d2c0, decals 0x00501ab0, the
  HUD 0x0040f7f0, stealth 0x004a4de0, the launch's line-ups (mech-build.md
  section 13),
  the AI (0x004c3b80, 0x004d2e20.., ai.md), objectives 0x0045c2d0, the start
  animation 0x0050b450, the unload 0x00490480 (the cleanup 0x00491780 is
  ported: mission-frame.md).
- InstallOptions bits 2 and 4 (the 16-bit mech textures installed, likely).
- The window's clear colour (the port clears to the debug viewer's sky).
