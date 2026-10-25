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
| 0x40fac0 (`hud_update`) | 19 | the Core frame's HUD update: view target vt+0xe0, 0x40fb50, three zTwoD layer ticks (0x59fd20), 0x40f1f0, the message panel's vt+0x24, the pocket commander 0x46a410 | its parts run on their own (`program_hud_update`, `pocket_draw`, the message panel); the entry itself logs |
| 0x40fb50 | 101 | the 25 text lines at 0x5fc848 (0x4b0 each): mode 0 clears them all (vt+0x7c, vt+0x64), 1 arms, 2 a picked line by the cursor | not ported; the lines exist only as hooks (`hud_line`, `hud_line_clear`) |
| 0x40e4b0 (`hud_draw`) | 81 | those 25 lines drawn and hit-tested by the mouse (rows y = 0x5f + 12 i; hover colour 0xff00) | not ported; logged every frame |
| 0x40f1f0 | 36 | a frame-rate text (frames over a second, CString::Format) into one of the lines | not ported |
| 0x5a6790 / 0x5a67c0 | 12 + 9 | zTwoD's layer: its two rects (0x5a66b0), then the layer's objects' vt+0x24 (0x59fd20) and +0x1ec cleared (0x5a6600) | logged (misnamed `effects_draw` / `particles_draw` in app_hooks.h) |
| 0x430aa0 | 16 | the pilot's HUD (MechData +0x224), made on first use | ported as `pilot_hud`; the mission frame's own call (`LaunchHooks::mech_screen`) still logs |
| 0x41ab90 | 14 | the mech's +0x378 zTwoD panel drawn (0x5a4410) | multiplayer only (made by commands 0x76 / 0x77 when networked; mech-build.md): nothing in single player |
| 0x4583b0 | 72 | maps.zrd: the mission maps (0x4580c0, 54 each; list [0x605acc]); free 0x4582c0 (17); find 0x458590 (42) | not ported: the radar's map mode (Shift+R, 0x43dc10) has no map |
| radar polylines, net marker | - | the nav route's lines (+0xbd4.. lists), the network element 0x58b060 | answer none (hud.md section 15) |

What a player sees missing today: the radar's map mode, the nav route's
lines on the radar, any text the game writes to the 25 lines (the
vehicles' developer readout uses lines 1-4; what else writes them is open,
item 1), and the frame-rate text. Nothing in single player depends on
0x41ab90.

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

1. **Trace the 25 lines.** Find their class and constructor, every writer
   of vt+0x7c in the exe (a grep of the call sites through 0x5fc848), and
   when 0x5fc314 (the hit-test's switch) and 0x603f8c are set. Output: a
   hud.md section; no code. Decides whether the lines are developer-only.
2. **The lines' object and 0x40fb50** (the clear, arm and pick modes), on
   the ported zTwoD text. Shows: nothing yet (the lines are drawn in 3).
3. **0x40e4b0, the lines' draw and hover.** Shows: whatever item 1 found
   writing them (the vehicles' readout at least, when it is on).
4. **0x40f1f0, the frame-rate text**, if item 1 shows it is reachable in
   the release (else document and skip).
5. **zTwoD's layer ticks** 0x5a6790 / 0x5a67c0 (renamed in app_hooks.h),
   once the layer object is named. Shows: the layer's objects updated by
   the exe's call rather than the program's own pass (no visible change
   expected; the test shows the same calls).
6. **0x40fac0 as the exe's one call**, its parts in their order, replacing
   the program's separate calls; `mech_screen` to `pilot_hud`. Shows:
   nothing new; the order of the HUD's updates becomes the exe's.
7. **The mission maps** (0x4583b0 and family): maps.zrd read at the
   start, freed at the end (the cleanup's 0x4582c0 is in mission-frame.md).
   Shows: nothing yet.
8. **The radar's map mode**: the map object's draw under the contacts
   (0x43dc10, hud.md sections 15 and 19). Shows: Shift+R swaps the radar
   for the mission map with the contacts over it.
9. **The radar's polylines** (needs zTwoD's PolyLine first, twod.md Left;
   that may be its own sitting). Shows: the nav route's lines on the radar.

Items 1-6 and 7-9 are independent of each other.

## 5. How each is tested

- **test_hud.py** (`tests/probe_hud.cpp`) builds the HUD in the emulator and
  the port and compares the call traces and the fields; it already has ops
  for the radar's map mode and polylines (ops 21-24: the counts, lists and
  the changed flag). Items 2, 3, 5, 8 and 9 add ops there: the lines' modes
  and draws with random text and cursor rows; the layer ticks over a fake
  layer; the map's draw with a rect and scale.
- **New differentials** for the maps' reader (item 7: every shipped
  maps.zrd and crafted ones, as test_landmines.py does for its zrd) and for
  0x40f1f0 (item 4: the counter's sums over random frame times).
- **Headless** (`tests/test_program.cpp`): item 3, a vehicle's readout
  switched on shows its lines; item 6, the HUD frame's call order recorded
  through the hooks; item 8, Shift+R in c1m1 turns the radar's map mode on
  with a map loaded.
- **Screenshots** (GPU allowed): items 3, 8 and 9 each with a pair, before
  and after, through `mw3 --mission ... --shot`.
- **Breaks** for each differential, as usual.

## 6. Open

- What writes the 25 lines in a release build besides the vehicles'
  readout, and whether 0x5fc314 (their mouse hit-test) is ever on outside
  the developers' modes.
- Which zTwoD layer 0x5a67c0 ticks, and whether the program's own 2D pass
  already covers its objects.
- 0x41ab90 and the chat panel wait for multiplayer.
