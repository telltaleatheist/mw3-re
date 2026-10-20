# Front end log (mw3 mac 3)

Newest last. One short entry per item: what, commits, what to remember.

## 2026-10-20 The fade state 0x0048eea0 (branch mac3/fade)

Commits 0c9787f, a0c690b. FadeState in app_states (fade_push / fade_replace,
0x0048ee70..0x0048f1b0); GameEngine::fade runs it; new Engine hooks
video_grab / image_draw / screen_tint / image_free, done on the program's
16-bit buffer. test_fade.py + probe_fade.cpp (17/17 breaks; the break script
pattern: mutate app_states.cpp, rebuild mw3probe, expect failure, restore).
Remember: the emulator's queue stubs don't call on_queued, the rebuild's push
does, so the test runs 0x48ef60 right after the entry; app_world stubs
0x48ee70 (deleted from e.slots in the test); print + flush before os._exit.
Left: the fade-out callers (their next states unported).

## 2026-10-20 Fade follow-up (branch mac3/fade-followup)

Commit bef71c4: FIX (A) marks, stills' handles reused, tint unit test (since
replaced by the ported tint in mac3/d3d-frame).

## 2026-10-20 The Direct3D frame (branch mac3/d3d-frame)

Commits 46ec4fd and the symbols/worklog one. zvid_dd (surfaces, locks, scene
nesting, 3D lock), zvid_ddd3d (colour quad), zrndr_tint (tint, software
finish); GameEngine calls them; SdlRender3D keeps one back-buffer texture a
frame, Screen2D::overlay_16 lays the keyed 16-bit layer (0xf81f) over it.
Remember: `drew_scene()` counts only 3D-scene frames (--shot counts Core
frames; fade frames lay only a layer). test_d3d_frame.py + probe_frame.cpp;
test_drawtable test_tint_quad (check new fn numbers get their own branch: an
`else` swallowed fn 15 once). Break script: scratch/breaks2.py pattern.
The blue lower half Owen saw: camera1 at the origin under the ground; the
view 0x0040aca0 and placement 0x004161c0 own it (mac 2).
Left: lock readback of the 3D picture (2D code that reads the screen sees
the key), 0x00576de0's software lock as a port, the 2D front end's tint callers.

## 2026-10-20 The player's view (branch mac3/view)

The view 0x0040aca0 family, the four camera classes and the cameras' set-up
0x00430e70 in mech3/player_view (docs/view.md); the program's glue in
app_view.cpp (the view made with the mission's, attached at the launch,
updated by the player's tick through PlayerHooks::view_tick, mode and target
copied into the view's block by GameEngine::sync_view). app_engine.h and
mech.h cannot share a file (two Handle types): app_view.cpp talks to the
engine through ViewProgram callbacks.
Remember: emulator tests run in the background into scratch/ files with
`-P -u -X faulthandler`, print per seed, end with flush + os._exit (a native
crash goes into UE instead of dying); probes must Mute set-up calls that run
before the seed's callback is set (a freed ctypes callback segfaults); the
break script leaves its last mutant built: rebuild after. Commits carry no
Co-Authored-By / Claude-Session lines (noreply email set in the worktree).
Left: 0x0040b310 pull-in, the world queries of the ground/water/weather
checks, the torso's cockpit switch 0x0041ce10 and the HUD's camera calls.

## 2026-10-20 The HUD's construction, item 1b (branch mac3/hud-map)

docs/hud.md maps the HUD (porting order section 7, as ported section 9).
mech3/hud builds the 0x22e40-byte HUD and all ten pieces on the ported
zTwoD, members in the original's order so the recorded calls match;
test_hud.py + probe_hud.cpp compare traces and every constructor-set field
(100 runs). New in zTwoD: FontText's text by message id (0x00409bd0, the
FormatMessage behind Host::format_message; the RecordingHost answers
"msg<id>"), and partial Frame, FilledBox, Line2D, ClipLine2D, Polygon and
Shape in twod_shapes.cpp (built and set, not drawn). zRender's nodes, the nav
list 0x0047e840, messages and the options' rects are HudHooks (lead's call:
the zoom views port with the render work).
Remember: emulator memory made after the snapshot is invisible to the
replay's peeks (make strings before `e.snapshot()`); pointers are 64-bit
here (the original's calloc(4, n) of pointers needs n * 8); an import or
engine function the original runs natively (0x004f34c0) must be stubbed to
show in the trace. Break script: scratch/breaks_hud.py (33 of 41, misses
equivalent at construction).
Next: 1a, the set-up 0x0043bb80 and the hud.zrd readers, with the +0x22e3c
FIX (bug) checked in tests/test_fixes.cpp.

## 2026-10-20 The HUD's set-up, item 1a (branch mac3/hud-map)

Hud::setup 0x0043bb80 runs from the constructor: hud.zrd's block by video
mode, MFD/TARGET/TIMER, the pieces' attach calls (reticle, MFD, TARGET,
radar with the display-sized zoom), and the readers EQUIPMENT, WEAPONS,
COMPASS, GAUGES (`read_layout`). test_hud.py now loads hud.zrd for real on
both sides (the original's zrdr load stubbed to a tree read by
`w.read_tree`, the probe opening the archive): shipped, crafted
(`synthetic`, resized MFDs reach the panel's realloc: stubbed) or none, with
a fake mech (vtable stub for vt+0xa0, fields the set-up peeks). The probe's
`id()` maps the HUD's members to the original's offsets for hook arguments.
test_fixes fix_hud_block_cleared checks +0x22e3c.
Remember: the equipment count skips non-positive parts (Ghidra was right,
my asm reading wasn't); a fake mech's +0x6c must stay 0 (a set slot takes
TARGET's unported path).
Next (lead, priority): the reticle's aim point and the hand-off 0x0041cd40
into mech +0x1d48, so `mw3 --mission c1m1` aims where you look; then the
porting order.

## 2026-10-20 The aim from the reticle (lead's priority)

hud.md section 10. HUD side in mech3/hud (0x004445f0, 0x00444350,
0x00444710, 0x004446a0, 0x00443660), the mech side in mech3/mech_hud
(0x0041ce60, 0x0041cd40, 0x0041bd80, 0x00430aa0), options' 0x0040d9e0,
zMath's 0x00523930; the program in mech3/app_hud (wired with the mech build;
the HUD set up at the view attach; the Core frame's hud() runs the hand-off).
MechControls got vtable +0x14 `look_position` (PlayerControls +0x140).
test_hud.py's aim phase: answers made before the snapshot (the replay reads
the snapshot), the segment code answered through the script so the replay
returns it. mw3tests mission_c1m1_player_aims_with_the_reticle_headless.
Remember: in view 0 the controls zero the look every frame (the reticle stays
centred); the cockpit ray at 11.9 m passes over an Owens' middle, so the test
pitches the torso (CMD_PITCH_DOWN 5) until the ray ends on it.
Next: back to the porting order (item 2, the HUD primitives' drawing).

## 2026-10-20 The mission-start fade (branch mac3/fade-grab)

FIX (A): `Render3D::read_back` (SDL GPU download on a fence, also used by the
capture now) and `GameEngine::video_grab` scaling it to the 16-bit size; the
menus' software grabs keep the layer. The Core state queues the fade before
the mission's first frame, so the still is the last 3D picture. `mw3
--shot-any` counts the fade's frames (--shot counts only scene frames);
`tools/analysis/fadecheck.py` checks frames 60/200/400 (GPU: ask Owen first,
he said yes on 2026-10-20). Locks stay without readback (a stall per frame).
Next: item 2, the drawing primitives (the reticle and the HUD on screen).
