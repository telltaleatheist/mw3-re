# The HUD and cockpit: what is left (plan, 2026-10-25)

A plan for whoever takes the HUD, the cockpit instruments and zTwoD's layer
ticks. It builds on `hud.md` (the HUD object, sections 1-19), `twod.md`
(zTwoD) and the program's `app_hud.cpp`. No code here; each item below is one
sitting with its own test.

## 1. What is already ported

The HUD object and almost all of its pieces are ported in `mech3/hud`
(2,856 lines; hud.md section 9) and wired in the program (`app_hud.cpp`):

- The object (0x43b190), every piece's constructor, the set-up 0x43bb80
  with hud.zrd at every video mode (test_hud.py, 100 runs).
- The frame: the data update 0x43c390 (from 0x40f8d0; `program_hud_data`),
  the HUD's own frame 0x43c820 through the view target's vt+0xe0 0x41cd40
  (`program_hud_update` -> `hud_handoff`), the Direct3D pass vt+0xe4
  0x41cd90 (`program_hud_draw3d`).
- The pieces' updates and draws: the compass, timer, gauges, weapon lines
  and the lock (sections 12, 13), TARGET with its tracking (14), the radar's
  update and contacts of every class (15), the damage displays and TARGET's
  text (16), objectives, equipment (17), the message panel (18), the HUD
  keys (19), the landmines' red marks (item 13).
- The pilot's HUD and cockpit model (0x430ad0, `pilot_hud`,
  `mech3::make_cockpit`), the cockpit's 3D passes (render_subtree) and,
  since item 18, the camera refresh after them (0x4b51f0).

The playtest screenshots show the result: compass, radar, weapon list,
gauges, reticle and the lance panel draw as in the exe.

## 2. What is still a stub (the surveys' HUD-side calls)

| Call | Size | What it is | Status in the program |
|---|---|---|---|
| 0x40fac0 (`hud_update`) | 19 | the Core frame's HUD update: view target vt+0xe0, 0x40fb50, three zTwoD layer ticks (0x59fd20), 0x40f1f0, the message panel's vt+0x24, the pocket commander 0x46a410 | ported as one call (item 6); the developer pieces left out |
| 0x40fb50 | 101 | the 25 text lines at 0x5fc848 (0x4b0 each): mode 0 clears them all (vt+0x7c, vt+0x64), 1 arms, 2 a picked line by the cursor | not ported; developer-only (hud.md section 20) |
| 0x40e4b0 (`hud_draw`) | 81 | those 25 lines drawn and hit-tested by the mouse (rows y = 0x5f + 12 i; hover colour 0xff00) | not ported; developer-only (section 20) |
| 0x40f1f0 | 36 | a frame-rate text (frames over a second) on the debug text object [0x5fc378] | not ported; shown only by a dead key command (section 20) |
| 0x5a6790 / 0x5a67c0 | 12 + 9 | **not the HUD's**: the weather's manager [0x736328], its rect and its objects' tick: the rain and snow (hud.md section 20) | logged; the program makes no weather (below) |
| 0x430aa0 | 16 | the pilot's HUD (MechData +0x224), made on first use | ported as `pilot_hud`; the launch's call wired (item 6); the draw paths' (`Engine::mech_ai`) still logs |
| 0x41ab90 | 14 | the mech's +0x378 zTwoD panel drawn (0x5a4410) | multiplayer only (made by commands 0x76 / 0x77 when networked; mech-build.md): nothing in single player |
| 0x4583b0 | 72 | maps.zrd: the mission maps (0x4580c0, 54 each; list [0x605acc]); free 0x4582c0 (17); find 0x458590 (42) | not ported: the radar's map mode (Shift+R, 0x43dc10) has no map |
| radar polylines, net marker | - | the nav route's lines (+0xbd4.. lists), the network element 0x58b060 | answer none (hud.md section 15) |

What a player sees missing today, on the HUD's side: the radar's map mode
and the nav route's lines on the radar. The 25 text lines and the
frame-rate text are developer-only: every writer is reached only through
the app key handler's dead commands, a vehicle or the stealth object as the
view's target (hud.md section 20). Nothing in single player depends on
0x41ab90. **Outside the HUD**, item 1 found the weather missing: rain in
c3, c4 and t1 and snow in c4b missions 4 and 6 are never made by the
program (`create_rain` / `create_snow` empty), and 0x5a67c0 is their tick.
That is its own item for the render or effects side (the rain and snow
classes, vtables 0x5d0660 and 0x5d05e0 over the base 0x5d0528, built by
0x5a60a0 / 0x5a5ad0; about 400 to 600 decomp lines with their draws),
not the HUD's.

## 3. Function families left, with sizes

- **The text lines** (0x40fb50 101, 0x40e4b0 81, 0x40f1f0 36, the line
  objects' vt+0x7c printf and vt+0x64; their class: 0x4b0 bytes, a zTwoD
  text object, to be found from the constructor that fills 0x5fc848).
  About 250 lines.
- **zTwoD's layer ticks** (0x5a6790, 0x5a67c0 over 0x5a66b0, 0x5a6600,
  0x59fd20 ported): about 40 lines, plus finding which layer object ECX is
  (the app's [0x5f4c8c]-side 2D layer).
- **The mission maps** (0x4583b0, 0x4580c0, 0x4582c0, 0x458590 and the map
  object's draw on the radar, 0x43dc10's other half): about 250 lines.
- **The radar's polylines and the net marker** (hud.md section 15, items
  5 and the marker): the polylines' lists are read; drawing them needs
  zTwoD's PolyLine (twod.md, Left). About 150 lines.
- **The entry points** (0x40fac0 as one call, `mech_screen` to
  `pilot_hud`): wiring, about 30 lines.
- Multiplayer's chat panel (0x41ab90, 0x5a4410, 0x41e5a0): left for the
  network work.

## 4. Porting order (one sitting each)

1. (Done 2026-10-25) **Trace the 25 lines**: developer-only, and
   0x5a67c0 is the weather's (hud.md section 20).
2. (Skipped) The lines' object and 0x40fb50, 3. their draw 0x40e4b0, 4. the
   frame-rate text 0x40f1f0: nothing a player can reach. They can wait for
   the stealth object and BASIC mode at the end (CLAUDE.md), whose debug
   key would bring the developer lines back with it.
5. (Done 2026-10-25, with the rain) The ticks 0x5a6790 / 0x5a67c0, now
   `weather_rect` / `weather_tick` (weather.md).
6. (Done 2026-10-25) **0x40fac0 as the exe's one call**
   (`GameEngine::hud_update`); the launch's `mech_screen` to `pilot_hud`
   with its objectives calls (hidden at the start, vt+0x64; shown for a
   time, vt+0x60). Showed: the message panel's text had never been drawn
   (its update, vt+0x24, is 0x40fac0's), now it is. Found, not done: the
   launch's radar reset 0x0043d920 on HUD + 0x14 (with items 8-9); and the
   draw paths' `radar` / `ai_tick` (app_states.cpp) are the HUD's, not the
   AI's: `Engine::mech_ai` is 0x00430aa0 (the HUD), 0x0043cd20 on it and the
   reticle's (+0x144c) vt+0x28 update, both still logged.
6b. (Done 2026-10-25) **The static** (0x43cd20 with 0x43bfe0, 0x43cca0,
   zRender's 0x558c10 and its noise 0x5581c0; hud.md section 21): the
   draw paths' `hud_static`. Shows: green static over the panels when the
   torso or head is badly hurt.
6c1. (Done 2026-10-25) **The views' nodes and render** (hud.md section 22):
   the HUD's cameras and windows made in zClass, the depth fill, the zView
   render. Open: what the MFD's mode 1 shows (Owen).
6c2. (Done 2026-10-25) **The zoom window's 3D pass** (the reticle's vt+0x28
   0x444230 -> 0x442f50 with 0x4436c0 and the zoom's own render 0x443320;
   hud.md section 23): the zoom camera's view in its box or circle.
6c3. (Done 2026-10-25) **TARGET's view of its target** (0x4428b0: the
   target drawn alone, its vt+0x68 around it; hud.md section 22), with the
   MFD's mode 1 (0x442900, its zView's own render: the player's mech from
   the front; playtest #29).
7. (Done 2026-10-25) **The mission maps** (0x4583b0 and family; hud.md
   section 25): maps.zrd read at the start, the mission's picked by the
   briefing, its image released at the mission's end.
8. (Done 2026-10-25) **The radar's map mode**: the map object's draw under the contacts
   (0x43dc10, hud.md sections 15 and 19). Shows: Shift+R swaps the radar
   for the mission map with the contacts over it.
9. (Done 2026-10-25) **The radar's polylines**: the mission boundary's
   two outlines, yellow and red (0x43d920; hud.md section 15, playtest
   #30). Shows: the boundary lines on the radar.

Item 6 and items 7-9 are independent of each other.

## 5. How each is tested

- **test_hud.py** (`tests/probe_hud.cpp`) builds the HUD in the emulator and
  the port and compares the call traces and the fields; it already has ops
  for the radar's map mode and polylines (ops 21-24: the counts, lists and
  the changed flag). Items 8 and 9 add ops there: the map's draw with a
  rect and scale, the polylines' draw.
- **A new differential** for the maps' reader (item 7: every shipped
  maps.zrd and crafted ones, as test_landmines.py does for its zrd).
- **Headless** (`tests/test_program.cpp`): item 6, the HUD frame's call
  order recorded through the hooks; item 8, Shift+R in c1m1 turns the
  radar's map mode on with a map loaded.
- **Screenshots** (GPU allowed): items 8 and 9 each with a pair, before and
  after, through `mw3 --mission ... --shot`.
- **Breaks** for each differential, as usual.

## 6. Open

- (Answered 2026-10-25, hud.md section 20) The 25 lines are developer-only;
  0x5a67c0 ticks the weather's manager [0x736328].
- Whether the release's camera cycle (X) can ever make a vehicle the view's
  target (which would show the vehicles' readout lines): not traced (the
  exe's camera list, 0x480b20 on [0x608c88]).
- 0x41ab90 and the chat panel wait for multiplayer.
