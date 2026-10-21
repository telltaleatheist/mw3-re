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

## 2026-10-20 Item 2: the HUD draws (branch mac3/hud-draw)

zRender's 2D primitives (`zrender/zrndr_prim2d`: dashed, clipped and noisy
lines, the line list, circles) and zTwoD's HUD draws (Frame, FilledBox, the
HUD lines, Polygon, PolyLine's clip paths, FlashImage 0x005a6d10) behind
`ztwod::Host`; test_prim2d.py and test_twod_prims.py 3000/3000, breaks 15/17.
Then the HUD's frame 0x0043c820 (`Hud::update`, called by hud_handoff
directly, the hud_frame hook gone) and every piece's +0x24 update (hud.md
section 11). Hooked: the debug overlays, the radar's update 0x0043dd00
(item 7), TARGET's hit test 0x00440a30 and the aim state 0x00443d20 (item
5), the weapon lines' looks 0x00446540, the nav list, the network. Not
ported: the Direct3D pass 0x0043c6b0 (see-through panels, MFD/TARGET 3D
views), so in Direct3D the panels are missing.
test_hud.py gained a frames phase: ops between frames (OPS: active, zoom
shown/round, reticle state, lights, flashes, modes, TARGET's slot, any
child's flags, dirty-all, show-all, texts, images, weapon line counts),
zRender's 2D calls logged on both sides (`HudHost` in probe_hud.cpp).
Remember: the aim-state stub must set a state 0..3 (with 5 left the zoom
light reads past its 4 images, as the original would; never in play);
0x004e6fe0 takes what TARGET's slot holds, not the slot; most HUD children
are managed (drawn only when dirty) and empty until their unported setters
run, so the test sets texts/images/flags itself or breaks go unseen.

## 2026-10-20 (a) The HUD's see-through images in Direct3D (branch mac3/hud-key)

Not a colour key after all: 0x0055b1f0's DirectDraw path (0x00576f90, keyed
on black: the surface 0x005778d0 is made with DDSD_CKSRCBLT and a zero key)
only serves images given a surface by [0x0072f360] = 0x005773e0 (a few
screens), never Image. The HUD's images take blit16's alpha blend onto the
locked back buffer, which holds the frame in the original but the magenta
key in our layer. FIX (A) `zrndr::LayerCover` (coverage and colour per
pixel while GameEngine's layer is locked) and `Screen2D::overlay_16` taking
it. mw3tests fix_layer_blend_keeps_its_alpha; GPU shot of c1m1 clean.
Remember: the cover is set only between lock_back and unlock_back; with it
unset blit16 is the original exactly (test_twod_blit.py).

## 2026-10-20 (b) The HUD's Direct3D pass 0x0043c6b0: where it stands (before a reboot)

Not started in code; (a) is committed (15d64b0). Read so far:
- 0x0041cd90 (the mech, for the view's target with a HUD from 0x00430aa0)
  calls 0x0043c6b0(dt) on the HUD. With a mech, in Direct3D ([0x0072f348]):
  the MFD's and TARGET's modes (vt+0x80); when the HUD is active, TARGET in
  mode 1: 0x004425d0 then 0x00576ce0; MFD in mode 1: 0x00576ce0; then
  0x00579af0; the see-through panels by 0x0043ad70 (TARGET's and the MFD's
  in mode 1, the weapons' while shown, the equipment's while it and the
  weapons are shown); 0x0040fb30; the objectives' panel while shown;
  [0x0072f3e4](); 0x00579b10. Then, active, TARGET's and the MFD's +0x28
  (0x00441410: mode 1 the target's object vt+0x68(1), 0x004428b0 on its +4,
  vt+0x68(0); mode 2 the damage view's +0x28 / 0x00442500: mode 1 the view's
  +0x28 0x00442870 = its zView's +4, mode 2 the damage view's +0x28).
- The panel's +0x28 0x0043ad20 is the same draw for one panel, guarded by
  shown, Direct3D and alpha != 0xff: 0x00576ce0, 0x00579af0, 0x0043ad70,
  [0x0072f3e4](), 0x00579b10.
Next: read 0x0043ad70 (the quad from the panel's corners +0xcc, colour
+0xfc, alpha +0xfe), 0x00576ce0 / 0x00579af0 / 0x00579b10 / [0x0072f3e4]
(scene and state calls; check render-frame.md / zvid ports for names),
0x0040fb30, 0x004425d0, 0x004428b0; port the pass and the panel's +0x28 with
the zView parts hooked (render work), a differential test in test_hud.py
(the pass after frames, render calls stubbed and logged), then wire it at
the program's 0x0041cd90 point in the Core frame (where the 3D scene is open).

## 2026-10-20 (b) The HUD's Direct3D pass (branch mac3/hud-key, main merged in)

hud.md section 11. Ported: Hud::draw3d 0x0043c6b0, HudPanel::draw3d
0x0043ad20 and draw_quad 0x0043ad70, HudMfd::shown_rect 0x004425d0 (asks the
mode again: a target can drop between the two asks), the MFD's and TARGET's
+0x28 (0x00442500, 0x00441410), the MFD view's 0x00442870 (hooked zView
render). Object2D +0x28 is `draw3d(float)` now (was `frame()`, unused; the
original's takes dt). The program runs it at the view target's vt+0xe4
(GameEngine::vcall → program_hud_draw3d, 0x0041cd90) with scene, quad
(TL buffer + zvid::draw_flat) and flush on the zVideo ports. Hooked: dirty
rects 0x00576ce0, the message panel 0x0040fb30, the 3D views' rendering
0x004428b0 / 0x00442870 (render work), the nav list's rect.
test_hud.py op 17 runs the pass; the new'd views are named 1/2 on both
sides (view_call / view_name).

## 2026-10-20 Item 5a: the missile lock 0x00443d20 (branch mac3/aim-state)

hud.md section 12. HudReticle::aim_state (x950/x954 are lock/lock_timer).
Outside through HudHooks, read-only (lead's approval): the group calls
0x004b1bd0/0x004b1c70/0x004b1cd0 stay hooks (mac 4, the planner), the
contact's mech/distance (zai_target), the aimed node's object 0x0040e920,
HUD sounds 0x0041e800/0x0041e770; the manager's +0x88 through a hook.
test_hud.py: the original runs; float answers on the x87 by a thunk; the
hits' nodes are real blocks; TARGET's slot is never null (the original
crashes on one); 0x0040e920 answers by node (asked twice). 100/100, 12/12.
Next: 5b the weapon lines (0x00446240 layout, 0x00446540 looks, 0x00445dc0
refresh, 0x00445d10 with a weapon).

## 2026-10-20 Item 5b: the weapon lines (branch mac3/aim-state)

hud.md section 13. HudWeapons::refresh_all 0x00446520 / layout 0x00446240 /
looks 0x00446540, HudWeaponLine::set_weapon 0x00445d10 (with a weapon; the
mark image is its second argument) and refresh_line 0x00445dc0. `shown` is
"refreshed each frame while any line recycles"; x1143c is next_weapon.
The manager and weapons are read through HudHooks (offsets of weapon.h).
Not wired in the program: the hooks carry 32-bit handles, the program's
weapons are pointers; needs a handle registry or typed hooks (proposed).
test_hud.py's fake mech has a manager with groups of fake weapons (vtables,
float returns by thunk), the original's refresh runs.

## 2026-10-20 The HUD on the real mech (branch mac3/hud-wire)

HudHooks' handles are void* now (lead agreed): the program passes real
pointers, the probe the emulator's 32-bit addresses (id() maps back); the
weapon list's +0x38 and next_weapon are pointers, dumped by id(). No
registry. ProgramHudHooks: mech_weapons = m->ammo, the planner getters,
has_target (+0x88), TARGET's slot = &m->target, contact_mech through
collide_calls(), Pilot roster (7 Elemental), equipment[8] (ECM), the groups
and weapons (DESC +8 as the name, ready(), ammo(), damage, enabled).
mw3tests can't include mech.h (app_hooks' Handle is 32-bit, mech.h's
void*): the test reads through app_hud's program_hud_report /
program_hud_lock_on / program_hud_force_over. The fake clock steps 1/60 s.
Next (lead): TARGET's tracking 0x00440b00, set_target 0x00441800,
0x00440a30 and the hit tests 0x005a3440 / 0x005a3270 / 0x005a3b30, ending
with the real c1m1 lock (no force).

## 2026-10-20 TARGET's tracking, parts 1-2 (branch mac3/target-track)

Part 1: zmod::project_point 0x0053cf10 (zmodel/gmod_screen; FIX (bug) the
behind-the-camera outcode's address bits) and options' render_to_screen
0x0040da30; test_project_point.py. Part 2: HudTarget::track 0x00440b00,
triangle 0x00440510 with clip_round_f 0x00440290, lead_point 0x0041d940 in
mech_hud (FIX (bug): no group). Shape's 0x005a3530 is a float scale
(set_scale). test_hud ops 18 (tracking) and 19 (lead). Remember: the
projections are answered from a table by call index ('0x0053cf10.out');
rand is an LCG from 1 on both sides (crt::msvc_srand(1) in the probe);
0x004e6fe0 must answer the live contact's vehicle (the triangle reads it
unchecked); the selected group must exist (the lead point reads it
unchecked); TARGET's triangle rect is the original's global 0x005e39d8.
Next: part 3 (set_target 0x00441800 / 0x00441780 / 0x00441660 with a
target, 0x00440a30, the hit tests), then give the fake mech a target in
+0x6c and rerun part 2's six missed breaks.

## 2026-10-20 TARGET's tracking, parts 3-4 (branch mac3/target-track)

Part 3: set_target 0x00441800 / set_shown 0x00441780 / texts 0x00441660 /
show_for 0x00441560 with a target, over 0x00440a30 (NARC: always), zTwoD's
Shape transform/update/contains 0x005a3270/0x005a3550/0x005a3440, Frame's
contains 0x005a3b30. Part 4: Hud::data_update 0x0043c390 (FIX (bug): the
timer past an hour), GameEngine::camera_update runs it; ProgramHudHooks for
the tracking and lead point on the mechs; program_hud_target_set for the
HUD part of 0x0041d890. Asked the lead: the sensors' set_target in
app_sensors.cpp (mac 4's) should call program_hud_target_set where it logs
0x0041d890. Remember: 0x004e6ff0 must answer the same per contact (the
original asks twice in a row); the fake mech's vtable needs +0x148; the
jump-jets bool is pushed with stack garbage above it (masked in the stub);
the torso pitch springs back when the key is let go (the c1m1 test holds
it around the triangle's middle).

## 2026-10-20 The radar, parts 1-2 (branch mac3/radar)

Part 1: HudRadar::update 0x0043dd00 up to the contacts (the map mode, the
size and place steps 0x0043dce0, the range text, the light, the polylines).
Part 2: update_contacts (from 0x0043e445): the mech's contacts with their
blips (vt+0x3c; the Mech's ported, mech_hud mech_radar_blip 0x0041ec60 and
the blips' create/destroy from 0x00413e60 / 0x00414000), TARGET's triangle,
the network marker (hook, 0x0058b060), the shots (Pixel2D 0x005a6b80, new),
the landmines. Remember: the exe's data starts the radar's and TARGET's
clip rects at (1, 1, 639, 479) (the probe resets them per run); the
original clears +0x5d only at the end; map mode without a map or a map
without a rect crashes the original; normalize 0x004020d0 is stubbed (the
vector kept, its true length answered); ceil comes in as FLOOR's code with
the rounding bits 0x0800; the marker's message arguments are logged on
both sides. Never edit src while breaks.py runs (it restores its copy).
Part 3: the program hooks (app_hud), the cone from the torso's flat front
(0x0041cdf0), the c1m1 check (the Owens is powered down, so not in sight,
until the bramp valve; mw3tests has no filter, it runs all). Next: inverted
mouse Y (Owen's decided fix) and item 6's remainder.

## 2026-10-20 Inverted mouse Y (branch mac3/invert-mouse)

Owen's decided fix: the game options' ACCEPT (0x00438100) wrote -1.3 / 1.3
over MouseAccelY; now `mouse_y_accel` keeps the size and sets the sign,
behind zfix bugs. test_menus.py options 30/30 (the exe's path);
fix_inverted_mouse_keeps_sensitivity. Remember: test_fixes.cpp cannot
include options_menu.h (its Handle clashes with mech.h's); declare instead.
Next: the credits' "Other Voices" offset and other data slips.

## 2026-10-20 Data slips: the credits' "Other Voices" (branch mac3/data-slips)

dlgCredits.zrd's "Other Voices" line has `"offset" (20)`; the exe keeps the
line above's (30 0). `credit_line_offset` gives a one-number offset the
headings' (20 20) behind zfix bugs (all 46 headings are (20 20) in
lab_large_grey; this one is in the names' ia_dark_grey, left as is).
test_menus.py credits 30/30 (the exe's path); fix_credits_other_voices_offset.
Dump a reader file: tools/analysis/zrd.py game/mw3-1.2/zbd/reader.zbd
dlgCredits.zrd. Not done: a spelling pass over Mech3Msg.dll's messages.
