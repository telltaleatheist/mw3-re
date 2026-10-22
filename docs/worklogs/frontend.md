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

## 2026-10-20 Item 6's remainder: damage displays and TARGET's text (branch mac3/hud-damage)

Part 1: HudDamagePart::set_level 0x004475c0, HudDamageView::update_levels
0x00447a50, HudMfd::data_update 0x004424b0. Part 2: HudTarget::data_text
0x00441850 (the texts, colours, background). Remember: a float-returning
thiscall stub with a stack argument needs its thunk to `ret 4` (`_ret`
with then= leaves the return to the thunk); MFC CString stubs at
0x5abdb0/da4/daa/dbc/e4c are import thunks (give pop); answer strings must
be made before the snapshot for the probe's peeks; `names` is reused later
in test_hud's world (mine are cnames). Next: part 3 (program, c1m1 damage
check, GPU shot).
Part 3: the program (app_hud: part_internal on the parts, the names from
Mech3Msg.dll, loadout, aimed part, saved_or_ally, format_message), the km
text fixed (" (1.23km)", zfix bugs; fix_target_distance_km_spaced), the c1m1
aim test with TARGET in mode 2 (a torso part's image changes at frame 89).
Remember: the silhouette set loads once a process (0x0043afc0's flag);
reset_process_state clears it; headless runs load no 2D images (stand-ins in
the test). The name tables came from running 0x00484670 / 0x00484d70 in
Unicorn (objdump mis-decodes that stretch; no capstone in the venv).
No GPU shot: the MFD and TARGET modes cannot be set from the command line,
and headless covers it.

## 2026-10-20 Gauges, objectives, equipment (branch mac3/hud-gauges)

HudGauges::set_speed/throttle/heat/coolant/jump (0x004453a0..0x004456f0),
HudObjectives::data_update/place_above (0x00446900/0x004468b0),
HudEquipment::data_update and HudEquipLine::set_item (0x00448320/0x00447e40).
The heat bar's source is one Rect at +0x1440 now. The coolant's eased level
is a process global ([0x005e3a14]; the probe resets it). Remember: never
`open(p,'w').write(open(p).read())` (it truncates first: test_hud.py was
restored from git); the item names came from 0x00484980 run in Unicorn;
op 26 pokes the jets' count through the replay's memory model.
Merged into main (lead, 2026-10-20). The coolant easing: Owen's standing
rule applies (per-frame steps go per second at the 30 fps reference), so it
gets zfix per_second in the next item and leaves fixes.md's Candidates.

## Where things stand (2026-10-20, before a compaction)

All of the above is merged. The HUD porting order (hud.md section 7): items
1-7 and 9 done, with 4 (gauges) and 6 (MFD/TARGET texts, damage) complete;
item 8 (reticle and zoom window: 0x004441a0, 0x00444230, 0x00442ac0..,
0x004436c0, 0x00442df0) partly; item 10's frame entries ported. Open:
the zoom window's show_for 0x004438d0, the debug
overlays (0x0043c820's first part), a mech's own name (0x0041df30 /
0x004aee70), the network's HUD pieces (0x00479ed0, 0x00479ef0, the marker
0x0058b060), the nav list 0x0047e840.

Since then the coolant easing and the message panel are done and merged
(next entry); the objectives' stand-in is gone. No current item: waiting
for the lead's next one.

How I work here:
- test_hud.py N (ops, worlds, dumps) against tests/probe_hud.cpp; run
  `.venv/bin/python -P -u -X faulthandler tests/differential/test_hud.py 100
  > scratch/hud/x.txt 2>&1` in the background.
- Breaks: scratch/hud/breaks.py SEEDS "name,name" (entries in its B0 lists,
  prefixed by item: r radar, c/m/s/x contacts, d damage, t TARGET text,
  g/o/e gauges); never edit src while it runs (it restores its copy).
- Coverage counters: scratch/hud/count_*.py (Original(seed).run(), t.log).
- Running an exe function in Unicorn for a table: tests/differential
  original.Original, stub_function(0x576040, ...), call(addr, ecx=...),
  EAX via uc.reg_read.
- mw3tests has no filter: it runs all (about 5 minutes); the c1m1 HUD tests
  are mission_c1m1_hud_weapon_lines_and_lock_headless and
  mission_c1m1_player_aims_with_the_reticle_headless.
- Program wiring: src/mech3/app_hud.cpp (ProgramHudHooks), tests helpers
  in app_hud.h (program_hud_report, program_hud_target, program_hud_lock_on).

## 2026-10-20 Coolant per second; the message panel (branch mac3/message-panel)

dc046fe: the coolant easing per second (FIX (B), Owen's rule; the candidate
dropped). 3e8cf3b + this commit: mech3/message_panel (0x0040f410
create, 0x0040f7d0 free, the class 0x005bc7a0: 0x0040f6b0, 0x0040f680,
0x0040f5d0, 0x0040fa90; the slide from 0x0040f8d0; 0x0040f040/070/0a0/0b0;
0x0040fe20/0x0040fe50; 0x0040fb30; 0x0040fed0). FIX (B): the slide's
truncated step (0 above 60 fps) carries its fraction. The objectives now
use the real point (stand-in gone). Hooks renamed message_panel_init /
message_panel_free; WiredMissionHooks fn_40f040 / fn_40f0b0 and
command_calls().other (0xa6) wired. Remember: the panel is a process global
(the probe destroys it under probe_app::Mute each run); within a frame the
test's ops run slide, switches, then messages, so a switch is followed by
a message before the next slide; headless text has no height.
Merged into main (lead, 2026-10-20, cb750d0). The slide speed stays 60 px/s
(the lead: the panel's speed field is per second by design; 30 px/s was the
accident of a 30 fps frame). Next: waiting for the lead's item; the HUD's
open pieces are listed under Where things stand.

## 2026-10-20 The campaign flow (branch mac3/campaign-map)

New area from the lead: the campaign from the pilot screen to a mission
and back. 5c278dd: docs/campaign.md, the map and a 9-item porting order
(lead agreed; the briefing's progress bar goes per_second in item 2, no
candidate). Item 1, the campaign's progress: Pilot::record_mission
0x00478f10, CampaignGame::next_mission 0x00434e00 (+ slot_at, slot_count,
reset_lancemates, copy_lancemates), campaign_advance 0x00484870 with the
counts 0x00484840 now ported as the hook's default, GameSlot::copy_from /
set_config, Store::assign / add_mech / add_config; and the loadout copy
(Config::copy_of 0x00424010/0x00423e00, add_item 0x004723c0 for every kind,
recount 0x00425bd0 into new fields free_slots/free_in, WeaponItem::location
0x00473220 via mech_dfn). New PilotHooks: config_copy, config_recount,
mission_salvage, save_world, message_box. Tests: test_pilot.py ops P (record,
with the question's answer), l, s (salvage), f (modified); campaign_steps in
the plan; fake loadouts now carry a weapon and its ammo on both sides.
test_mechsave.py copies every undamaged loadout (stand-in dfns seeded inside
each copy: the test resets the emulator heap between loads; mech_dfn_slot
added for the probe). Breaks: scratch/campaign/breaks.py SEEDS [names].
Found: No to "replace saved game" still overwrites the file (zSave writes
first): fixes.md "still to fix", for when zSave is wired.
Merged into main (06a6f86). Next: campaign item 2 on mac3/briefing, the
briefing screen's shell (0x00405230 without the map; buttons and their
enabling 0x00408ef0/0x00404ae0; the frame 0x00406440; ABORT 0x00405050;
LAUNCH 0x00404f70), the progress bar's +0.005 a frame per_second at the
30 fps reference (lead: no candidate). Start by reading those five in
analysis-output/decomp and the Load state's hooks in app_states.

## 2026-10-20 Campaign item 2: the briefing shell (branch mac3/briefing, off main 06a6f86)

`mech3/briefing.{h,cpp}`: BriefingScreen 0x00405230 (dtor 0x00406170,
frame 0x00406440 with the bar per_second, set_loaded 0x00408ef0, presses
0x00404c30, abort 0x00405050, launch 0x00404f70, progress 0x004050d0, log
0x00405120 FIX (bug) "%s"), BriefingScript (list + stop 0x004063e0; entries
are item 3), globals briefing_reached [0x5e043c], briefing_voice [0x5f4c84],
briefing_screen [0x5f4c80]. In menu_widgets: HelpButton (0x0044aaf0, the
mech lab's blinking button with HELP labels), ShellSlider (vtable 0x5bbc90,
was controls' AxisSlider, now an alias), StoreList (0x0048d880 ctor only,
REALLOC_LIST unbound). Control::enable (0x00409e60) in twod.h. Engine hooks
in app_hooks.h: game_kind/game_campaign/game_salvage_left/
game_multiplayer_kind (defaults on the ported pilot, briefing.cpp),
app_flag_1ba, briefing_map_image, briefing_realloc, briefing_lance_loadouts,
briefing_lab_store, mech_lab_open, salvage_open, multiplayer_launch_prep,
missions_loaded, mission_launch, mission_cleanup, replace_state,
campaign_merge_salvage. Not wired yet: LoadState still constructs 0x405230
by address (item 5). Test: test_briefing.py + probe_briefing.cpp (40/40;
skips SOUND block 0x40583d->0x405bc8 and REALLOC_LIST bind in the original;
the original's never-set script iterator counts as the end). Unit:
test_mainmenu.cpp fix_briefing_bar_per_second. Breaks: scratch/briefing/
breaks.py (32). Debug helpers scratch/briefing/dbg2.py (first differing dump
after each op), dbg3.py (first differing canonical call with its frame).
Lead (night): go on through items 3-7 without waiting, branch from main if
merged else from the previous branch; then stop.

## 2026-10-20 Campaign item 3: the briefing script (branch mac3/briefing-script, off mac3/briefing cc1b6b0)

BriefingVoice 0x00405e10 (vtable 0x5bbc50), BriefingMarker 0x00405db0
(0x5bbc40), BriefingShow 0x00405d50/0x00405d90 (0x5bbc30), briefing_cue
0x00405ed0, BriefingScreen::read_script (the SOUND block, FIX (bug) for a
missing MARKERS). Engine hooks voice_play, voice_status, sound_cue_briefing,
briefing_map_object (0x00406650, null until item 4: BriefingShow untested
until then). Correction: the destructor's virtual list destroy (0x00409910)
is the +0x12e60 list's; the script's entries are plain deletes. Test:
test_briefing.py now runs the SOUND block, ops Y (start) and C (cue).
Breaks: scratch/briefing/breaks3.py, 12/12 (the marker compare needed a scripted cue run in the plan).

## 2026-10-20 Campaign item 4: the briefing map (branch mac3/briefing-map, off mac3/briefing-script 9a84a1c)

BriefingScreen::read_object 0x00406650 (every kind), MapText 0x5bc030
(set_pos 0x00407cf0), MapCircle 0x5bbfb0 (set_pos 0x004080a0, set_radius
0x00408050, show 0x00407e40, update/shrink 0x00407ea0 FIX (B) per_second),
MapWaypoint 0x5bbeb0 (update 0x004081a0), MapTriangle 0x5bbe30 (ctor
0x005a31e0 inline, erase 0x00408470, show 0x004084b0, update 0x004084f0),
MapLine 0x5bbdb0 (update 0x00408600); objects list +0x12e60 deleted first in
the destructor. Engine hooks: game_text, mission_navs, nav_find, mission_map,
nav_position, map_project, map_image, world_up, report (silent: 0x0049bfd0 is
empty). zTwoD gained Frame::hit (0x00409db0) and Shape::hit/get_x/get_y
(0x00409e40, 0x00409e20/30), slots the port had missed. Test: real map
objects; stubs for nav points, map, projection (0x004586e0 answers x, y and
the result packed), game texts, 0x00575ff0 (message 0x200's argument
logged), 0x00579880, the 2D circle 0x005630c0 and line 0x00562940; dump kind
'B' (base only) added to test_twod.py and probe_twod.h; synthetic objects
of every kind, a synthetic brief_zz with fixed cases (a skipped point mid-line, a label with no message, a circle under the mouse, a text with no AT). 40/40. Breaks: scratch/briefing/
breaks4.py, 25/27: the triangle's erase (no background is ever set) and the line's per-frame dirty (redrawn anyway) cannot show.

## 2026-10-21 Campaign item 5: the program to c1m1 (branch mac3/campaign-program, off mac3/briefing-map a80d985)

GameEngine (app_menus.cpp): construct(0x405230) makes the ported
BriefingScreen (briefing_screen() set); load_state_enter (our hook, the Load
state's enter) fills the launch stand-in from current_pilot()'s game
(campaign: number, dir 0x00435350, slot mech + its Config handle; training:
t1); Launch gained auto_launch (debug starts only) and config (vcall 0x34's
+8). briefing_script_start replaces the Load state's pokes (Engine default
keeps them); briefing_init/start set the cursor; 0x00405160 stood in (MAP
image only); missions_loaded/mission_launch/mission_cleanup/replace_state/
voice_play/voice_status/sound_cue_briefing/app_flag_1ba/mission_navs wired;
the load's progress and lines go to the briefing (program_briefing_progress/
line, free functions: app_mission.cpp cannot include briefing.h, mech.h's
Pilot clashes). Program check: test_program campaign_menus_to_c1m1_through_
the_briefing_headless (NEW CAMPAIGN, pilot "Recruit": the pilots list stays
between tests, so "Ace" from the start test had c1m2 newest; ACCEPT, the
briefing loaded with its script and map, LAUNCH enabled and MECHLAB grey,
LAUNCH, 30 Core frames, the mech built). Breaks: scratch/briefing/breaks5.py 4/4 (the script's start needed its own check). Owen's try: build/mw3 -nofmv.

## 2026-10-21 Campaign item 6: the debriefing (branch mac3/debriefing, off mac3/campaign-program d190607) — in progress

Reading 0x0047b430 (WINMENU of dlgWin.zrd; new(0x13bf4); args state, campaign,
mission, state+0x10): base MenuScreen-like (0x0059ec40 Dialog, 0x00438cc0
the screen base, vtable 0x5bc190 then 0x5c3b00); parts: ICON_CONTINUE/REPLAY/
ABORT 3 HelpButtons (0x17c, ids 0, 1? 2 by +0x150 writes) at +0x112a8, +0x11404,
+0x115a0; 4 player panels 0x6a0 (ctor 0x0047af70, dtor 0x0047b180) at
+0x1171c; ICON_RANK HelpButton subclass (vtable 0x5c3bd0) +0x1319c; two
Line2D; WIN_TITLE Control (vtable 0x5c11c0) +0x133b0; SALVAGE_LIST StoreList
(0x0048d880(0,5), vtable 0x5c3b20) +0x134fc; +0x13bcc byte (game type check),
+0x13bdc = arg 4. Reads the zEntity list (0x0073655c..), each vehicle's
vt+0x2c/+0x50 (its mech), team masks, damage vt+0x98, kills +0x3f4..; mission
time [0x0060991c]; [0x00609926] failed flag; salvage [0x00609990]. Press
0x0047d030. Plan: the shell first (layout, buttons, press), the lance panels
and stats through hooks the test answers.
Decided split (6a now): the shell 0x0047b430 up to 0x0047baf0 + its tail
(texts vector into the dialog, rank 0x0047b2f0 as a hook in 6a), the press
0x0047d030, the icons (plain HelpButtons via 0x0044c310), WIN_TITLE (Control),
the cards' shell (0x0047af70 base 0x0044a770: HelpButton + mech 0x13 at
+0x17c + Text at +0x180; read_control 0x0047b070, update 0x0047b2b0), the
StoreList's read_control 0x0048db30 / after_bind 0x0048db70; hooks: the
results block 0x0047baf0..0x0047c84b (6b), the cards' mech display 0x0047b210
(6c), the StoreList refill 0x0048dbe0 (6c/8). Then item 7 before 6b/6c.
Mission time 0x0047b750: seconds = t - minutes*60 (the HUD timer's slip):
FIX (bug) as fix_hud_timer_past_an_hour. Labels of WIN_TITLE: [1] result
(0x180 failed / 0x181), [2] "Op %1 Mission %2" 0x177 (IA: 0xdc0 formatted,
else the IA game's vt+0xc name), [3] the time; CONTINUE help 0x174 (won,
no cheat [0x00609994]) else 0x173, 0xfb8 when the campaign is won
(state+0x10); IA: ABORT help 0xdbf, label[1] 0x176, CONTINUE help 0x173.
FontText +0xa8 (0x00409c40: plain text of a message id) needs porting.
6a done (code): mech3/debriefing.{h,cpp} (DebriefScreen, MechButton, LanceCard,
RankIcon, WinTitle), StoreList's read_control/after_bind/update/layout/
can_light/refill (menu_widgets), FontText::set_message (0x00409c40) and
Host::message_text (0x00576040; SdlTwodHost answers it), Engine hooks
(mission_failed/cheated/time/salvage_store, exchange_fast_menu,
debrief_results, lance_display_*, store_list_refill, mech_name_of,
message_format_is, fade_to_state (IA abort: ECX the IA Dummy State), pilot_save_now,
campaign_reset_lancemates, anim_reset, replace_state_at, campaign_set_b0,
action_next_round). +0x13bcc is the texts vector's allocator byte (a stray
stack byte), not a flag. Cards, the rank and the title have no rollover; the
cards no press. test_debriefing.py + probe_debriefing.cpp 40/40 (the results
block skipped by a code hook; the synthetic file keeps the icons and the
title: the exe reads their labels unchecked). Breaks: scratch/briefing/
breaks6.py 19/24 (unobservable here: the salvage list already shown, a card's update, the title's rollover, the store list's title flag and row fonts with no rows). Not yet wired into the program (the Win State constructs
0x47b430 by address): with item 7.

## 2026-10-21 Campaign item 7: the loop c1m1 -> debriefing -> c1m2 (branch mac3/campaign-loop, off mac3/debriefing 7a3a7b6)

WinState takes its address; States::end_win at 0x006099a0 (the missions
object's +0xd0), the end screen's fade's next. WiredMissionHooks::fade_out
(0x006099a0) is fade_replace(&end_win, 0, 1, 2, 2 s, black). GameEngine:
construct(0x47b430) makes DebriefScreen (won = the matching WinState's
campaign_won_), pilot_campaign = the launch's game, vcall game 0x48 (Launch.op)
and 0x38 (program_slot_count), mission_results 0x0047ae10 (failed: merge;
won: the lance's salvage logged, item 8). Host::message_text's default is
zutl's (ported). 6a review follow-up: fade_to_state carries 0x0048ef00's
stack arguments (1, 2, [0x5e7274] = 2 s, 0, 0, 0). Program check: the
campaign test goes on: Exit Mission after a win, the end Win State's
ported debriefing, CONTINUE, c1m2's briefing loaded with its script; no
0x00479440 or 0x0047b430 unported. Breaks: scratch/briefing/breaks7.py 4/5
(the operation only matters at Op 4). Left: 6b (results, scores into the
ported campaign's +0xb4/+0xc4), 6c (lance cards), item 8 (salvage), 9.

## 2026-10-21 Item 7 review fix (branch mac3/debrief-results, off main f8cfa9e)

shell_clear and state_screen now know the end Win State (shell_state_at:
main, pause, load, win, end_win, salvage); before, WinState::leave on
0x006099a0 fell to the unported stub and the debriefing leaked each mission
end. The campaign test checks the screen is freed after CONTINUE (caught
with end_win removed from the list).

## 2026-10-21 Campaign item 6b: the debriefing's results (branch mac3/debrief-results)

Branch off main f8cfa9e; its first commit ded0581 is item 7's review fix
(end_win in shell_clear/state_screen). Ported 0x0047baf0..0x0047c84b as
`DebriefScreen::results` with add_text 0x0047cab0 / add_number 0x0047cce0 /
add_points 0x0047cd70 and `RankIcon::set_rank` 0x0047b2f0 (campaign.md
section 3, The results). The lance filter is in the port (the hook gives
every entity with a mech: player flag, team, type, +0x3f4..+0x408, name;
`player_allies` the player's +0xc4). +0x3f4/+0x3f8/+0x3fc are the kill
credit's mechs/vehicles/turrets counts, +0x400.. their points; +0xb4 the
pilots' career mech kills (header "'Mechs Defeated"). An early misread
(the top line double-counting) was wrong: the top is the mission total.
Hooks added: debrief_lance, player_allies, debrief_objectives,
objective_text (0x0045e080), campaign_score/record, mission_ended,
action_rounds, win_keep_scores (the Win State's copy; the default keeps
the field32 calls test_app_states replays). Program: `mech_debrief_lance`
(app_mech.cpp) from the ported mechs, the score and records from the
pilot's ported campaign, `GameEngine::win_keep_scores` into it. Not yet:
a lancemate's name (0x004aee70, AI) is "", objectives.zrd's lists, IA's
+0x44/+0x6c/+0x70 on ActionGame.
Test: test_debriefing.py 40/40 (fake zEntity list and objective lists,
the results' dump; CString operator= 0x5abdbc stubbed; ICON_RANK's labels
kept in the synthetic file as the exe reads them unchecked); Line2D got a
dump kind `N` (test_twod.py, probe_twod.h). test_program's loop checks the
score through to the next game. Breaks: scratch/briefing/breaks8.py.

## 2026-10-21 Campaign item 8a: a won mission's salvage (branch mac3/salvage)

Branch off main 3182545. First commit bf546c5: the 6b review fix (the
program's Win State copy skips cards without a record line, since
lancemates' names are not ported; loop test pins b4[1]). Then 8a: new
`mech3/mech_salvage` (the original's mech_salvage.cpp): add_salvage
0x00472f00, add_weapons 0x00472da0, add_vehicle_salvage 0x00472ec0,
lance_salvage (0x0047ae10's CL 0 path) over a `SalvageMech` view;
`Store::add_weapon` 0x00471840 and `add_equipment` 0x00471a20 (pilot
module); `weapon_has_tech` 0x00484940 (mech_names, the 0x005e6500 masks).
Found in the asm (the decomp hid it): 0x0047ae10 sends the player's own
mech into the campaign's +0x64 store (ecx = game + 0x64), allies' into
[0x00609990]. Program: `mech_lance_salvage` builds the view from ported
mechs (a local copy of 0x0041d510, app_weapons.cpp keeps its own in an
unnamed namespace), `mission_salvage()` is the missions +0xc0 store, the
launch empties it (GameEngine::mission_launch), the pilot hooks and
mission_salvage_store() hand it out. FIX (bug): a weapon without a def
skipped (exe null read). Test: test_salvage.py 200/200 on test_pilot's
emulator; breaks 19/21 (2 unshowable). Left for 8b: the store list's rows
0x0048dbe0 (debriefing salvage list, REALLOC_LIST, 0x00404ae0,
0x00408bd0/0x00408b40); 8c: 0x00408c30.

## 2026-10-21 Campaign item 8b1: the store list's rows (branch mac3/salvage)

8b split: 8b1 the rows (StoreList::refill 0x0048dbe0, add_row 0x0048d9a0,
the items' names and labels 0x00473700..0x004739c0 in mech_salvage.cpp,
the shown-name message tables in mech_names); 8b2 the briefing's
reallocation (REALLOC_LIST bound, 0x00404ae0, 0x00408b40, 0x00408bd0 with
0x00408850 and 0x00408690: 300 t and 2 mechs per +0xa8 unit). Order
found: the armor and ammo labels ask for "tons" / "rounds" before the name.
test_debriefing's salvage list now refills from a real store; an 'R' plan
step varies the list's settings. Breaks 16/18 (2 unshowable). Also the
8a review nit: app_mech.cpp's tech lookup noted (MechDfn +0 is the roster
index).

## 2026-10-21 Campaign item 8b2: the briefing's reallocation (branch mac3/salvage)

Ported in briefing.cpp: store_tons 0x00409fd0, store_mechs 0x00472970,
store_count_like 0x00473510, store_take 0x00408690, store_reallocate
0x00408850, lab_store_build (0x00408b40 / 0x00408660), refresh 0x00408bd0,
show_realloc 0x00404ae0; REALLOC_LIST bound. Room: +0xa8 x (300 t, 2 mechs).
Engine hooks game_store, campaign_salvage, campaign_units, store_handle;
the defaults give stores and items handles from a table (menu_widgets.cpp),
GameEngine's own store_of/item_handle dropped for it; the Load state's
briefing_refresh/briefing_show reach the ported screen. Removed hooks:
briefing_realloc, briefing_lab_store. Tests: test_salvage.py's realloc
part (200/200, weapon labs from weapons.zrd), test_briefing.py with the
bind unskipped, real stores in the fake game, an 'X' plan step (40/40).
Breaks 16/18 (2 unshowable). Quirk kept: +0xa8 = 0 lets the split mint
mechs. Next: 8c, the lancemates' loadouts 0x00408c30.

## 2026-10-21 Campaign item 8c: the lance's loadouts (branch mac3/salvage)

0x00408c30 ported as lance_slot / lance_fit / lance_keeps and
BriefingScreen::lance_loadouts (set_loaded calls it); with it the mech
lab's store-aware loadout pieces: config_copy_store (0x00424010 whole),
GameSlot::set_config(c, store, take) (0x00433df0 whole), Store::has_config
0x004735f0, take_config 0x00473440, give_config 0x00473400, remove_config
0x00472d40, has_mech 0x00409f00, count_like 0x00473510. Found: 0x004735f0
counts the second store (decomp hid the ECX). Test: test_salvage.py lance
part (200/200) with stand-in dfns and real loadouts built by the exe;
test_pilot's emulator stubs 0x00424010/0x00425bd0/0x00424350, so the
lance test keeps them native. Breaks 17/19 (2 unshowable; the "clamp"
break hangs the probe's cut loop and was killed, counted caught).
Next: 6c (the lance cards' mech display 0x0047e840, 0x0047b210).

## 2026-10-21 Campaign item 6c: the lance cards' armor display (branch mac3/salvage)

LanceDisplay (0x0047e840..0x0047f140) ported in debriefing.cpp: eleven
bars of two Polygons each, H/T/A/L labels (hud_htal), a line; layout
0x0047eba0 (scale = height / the most armor seen on that card), fill
0x0047ef10 (part vt+0x44 by section), update 0x0047f0e0; set_vehicle
0x0047b210, set_mech_image 0x0047b260, mech_icon 0x0044a9b0. No mech-lab
boundary hit: 0x0044a9b0 is a small icon lookup. Single-precision maths
(the x87 at 24 bits; at 53 bits exact multiples come out a pixel short).
Engine hook vehicle_armor (GameEngine: mech_lance_armor in app_mech.cpp),
GameEngine::mech_name_of. Test: test_debriefing.py natively (polygon fills
recorded as test_hud does; CString += stubbed); breaks 20/23 (3
unshowable). With it, the 8c review items: has_config's compare is the
exe's (other vs the loadout, never equal), give_config returns
add_config's result, the dropped items' delete has its fixes.md row.

## 2026-10-21 Save-before-question fix (branch mac3/save-fix)

Confirmed in the asm: 0x00478f10 calls next_mission (vt+0x24, 0x00478f3c),
which runs zSave 0x005a8a70 (0x00434f42) before the question at
0x00478fc8. FIX (bug) behind `bugs`: next_mission(next, save=false), and
record_mission saves the world after filing (new or Yes), not on No.
test_pilot.py (bugs off) still matches the exe; new C++ test
fix_campaign_save_after_the_question covers both answers, fix on and off
(two quick breaks caught). fixes.md row moved from "still to fix" into the
table; CHANGES.md and discoveries updated. Also the 6c review comments
(most 0 → height 0 via inf/NaN; mech_lance_armor's guards as departures).

## 2026-10-21 HUD: the aimed node's object (branch mac3/save-fix)

app_hud's node_object now calls mac 2's node_entity (0x0040e920); the
stub that answered none is gone. object_byte8 (+8, ZEntity's active) reads
a mech's `active` (entity_is_mech in app_mech.cpp), any other object is
logged unported; node_bc (+0xbc) is the node's damage callbacks' record
(a typed field in the port, the raw bytes there are stale): has one = 2.
test_hud 20/20 (its probe keeps the hooks), mw3tests 107/0.

## 2026-10-21 HUD leftovers (branch mac3/save-fix)

- The MFD's/TARGET's "nav list" (0x0047e840) is the armor display: moved
  to armor_display.h + lance_armor.h, 0x0047eb60 place ported, the
  program's HUD hooks run it (c1m1 HUD test checks it is laid out).
- The zoom window's show_for 0x004438d0 (view's, then text hidden);
  test_hud.py op 33 (break caught).
- A mech's own name 0x0041df30 / 0x004aee70: player's pilot name
  (program_pilot_name, app_menus.cpp) or the brain's AiParams pilot
  (mech_brain_name, app_mech.cpp); HUD TARGET's third text and the
  debriefing's lancemate names use it. Open question for the lead: the
  6b guard (keep a card's +0xb4 without a record line) could go now.
Network HUD pieces skipped as ordered.

## 2026-10-21 b4 FIX, the mech lab map, lab item 1 (branch mac3/mechlab)

- b4 guard behind `bugs` (e26e40d): off follows the exe (a card without a
  record line writes 0), on keeps it; the campaign loop test runs both
  (b4[1] 7 / 0), with a GameSlot(-1) lancemate and x9c = 2.
- docs/mechlab.md (c6d67f7): way in and out, LABMENU's controls and ids,
  what the briefing and debriefing need, what is ported, a 10-item order.
- Item 1: `LabState` (app_states.h, mech3/mech_lab.cpp): 0x00448830,
  enter/update/leave, ACCEPT 0x00448870, the screen's make/delete, the
  frame 0x00448f10, the part lookup 0x004507c0/0x00448e20. Hooks
  (`Engine::lab_*`, config_export/import, model_purge, node_frame_update)
  for the 3D world, the screen's pages and slots, the loadout files.
  test_app_states.py `lab` cases (11 functions); the Ctrl+I add's tail is
  a hook the test keeps out of reach. GameEngine opens the ported lab from
  MECHLAB, its slots from the ported campaign, the world load passing.
  Found: the lab's +0x64 is the frame clear's flag ([0x008026f0] through
  0x0057a3c0; its hook renamed video_set_clear from video_set_gamma); update's result is never
  set; Ctrl+X / Ctrl+I export/import loadouts (discoveries).

## 2026-10-21 Lab step 4 (branch mac3/lab-configs)

- CONFIG_LIST real (read, show/hide, select 0x00449c40, fill 0x00449fb0,
  0x00449e10, 0x00449d40, 0x0044a1f0), CONFIG_EDIT's rename start/finish
  0x0044a2f0 / 0x0044a3a0 with Enter/Escape, presses 14/15/16, the import's
  add (0x00448c1d). Hooks for the lists and loadouts on handles (the probe
  models the lists: the exe edits them in its memory); GameEngine on the
  ported ConfigList (remove 0x00425090 ported) and Config.
  Store::has_config gained a loadout-as-other overload (0x004735f0 as the
  lab calls it).
- FIX (A) Ctrl+X / Ctrl+I: export\ and import\ (lab_export_config,
  lab_import_config; the read name is the new loadout's, 0x00423d00);
  FIX (trap) no checksum on import. Config's dup is null-safe as _strdup.
- The lead's FIX (bug) items: the tonnage text (printed as text) and the
  name sort (by name), behind bugs; fix_lab_sort_by_name. The skin item is
  not a bug: the mission start (0x0049e5e0) paints the whole lance with
  slot 0's scheme, so the lab's one list is the lance's.
- test_lab_screen: loadout sessions in the plan (rows, SAVE, RENAME,
  DELETE, typed names, Enter/Escape).
- Breaks 28/29 (the tonnage FIX's: same text with numbers). The test's
  0x00433df0 stub now gives the slot its loadout (both sides), the
  synthetic dialogs add CONFIG_ADD, a quarter of the stores are full.
- Confirmed the speedrun item duplication (community-issues.md): 0x004735f0
  checks each item alone against the store, never the loadout's sum.

## 2026-10-21 Lab step 3 (branch mac3/lab-chassis)

- CHASSIS_LIST real: refill 0x00449250 (VC6 std::sort written out:
  vc6_sort in lab_screen.cpp, the comparator's message calls are traced),
  layout 0x00449760, select 0x00449b30, find/select by mech, sorts 31/32;
  SKIN_LIST select 0x0044b1c0; the page refresh 0x0044cf70/0x0044cf50 with
  the later steps' parts hooked. Program overrides on the ported slots,
  loadouts, dfns and skins.
- Port gap found for the lead's question: 0x004733b0 (a new campaign's
  starting mech) was an unported hook, so the store started empty and the
  briefing's fit cleared slot 0. Ported (PilotHooks default,
  ConfigList::find 0x00424e90); the campaign test no longer sets the mech.
- Review items from step 2: the ids the exe ignores are quiet; the
  set_mech guard noted; 0x00433df0's (0, 1) are in the hook's call.
- test_lab_screen 60/60 (stores over 16 mechs, the question box, skins).
- Breaks 27/33 caught (the median one hangs the probe: counted). Not
  observable: the sort's threshold 16 vs 15 (the partition loop keeps 16),
  the recursion side (equal halves only), sort modes > 1 (never set), the
  found index (an equivalent rewrite), the chosen tonnage's font/show order,
  the skin arrows during a rename (they start on and nothing turns them off
  yet). Synthetic dialogs add the three tabs the shipped one leaves out.

## 2026-10-21 Lab step 2 (branch mac3/lab-shell)

- 2a: LabScreen (mech3/lab_screen.{h,cpp}) 0x0044b920 / 0x0044c600: every
  control made in the original's order (tabs and the 12 mech buttons in
  place, InPlace<>), the screen's own controls bound and checked; the pages'
  controls (CHASSIS/CONFIG/UNASSIGNED/ASSIGNED/weapon lists, ENGINE_TABLE,
  LOADOUT_LIST) made but their read/after-bind/show/hide/refill hooked
  ("lab.vt+<slot>" with their offset; the test patches those slots in their
  own vtables), the three new'd panels faked (construct 0x00457600,
  0x00456cd0, ARMOR_TITLES jumped over at 0x0044bd8e). Page switch 0x0044ca50.
- 2b: the press 0x0044d4d0's shell (rename guard, pages, slots, "lab.press"
  for 14..32), 0x0044ce30 (ACCEPT), the Lab State's 0x00448f80 / 0x00449010,
  0x0044ce90, MechButton::set_mech 0x0044a890. Program: construct(0x44b920)
  makes the screen; MECHLAB -> real screen -> ACCEPT back (campaign test).
- test_lab_screen.py 60/60 (synthetic dialogs on odd seeds). Item 1 fixed:
  +0x38 is the mech to show (mech_, kNoMech), +0x20 the per-mech entries.
- Found: PART_HEAD is the parts' class (vtable overwritten); "double2" panel
  image never released; MISSION DATA tab always disabled (discoveries);
  CONFIG_ADD (13) has no press; 0x005a2ba0 is a shared +4 setter (not
  stubbable); MESSAGE_AREA plays its SOUND with every text.

## 2026-10-21 Playtest #2, the game's message box (branch mac3/msgbox, merged)

- GameEngine::game_message_box ports 0x004ac150 on ztwod::MessageBox
  (dlgMessage.zrd's MESSAGEBOX) over the current screen; message_box and
  message_box3 go through it. Its loop pumps the app's MessagePump
  (pump_events: a quit ends the box and is posted again). FIX (A) row in
  fixes.md. Test: playtest_delete_pilot_through_the_games_message_box_headless
  (yes and no).

## 2026-10-21 Playtest #3, the pause screen (branch mac3/pause)

- PauseScreen (mech3/pause_menu.{h,cpp}): ctor 0x00492750, dtor 0x00492ae0,
  press 0x00492d70 (RESUME, QUIT by game kind, OPTIONS, REPLAY), menus.md
  section 11. Engine hooks: zSound's resume and group stash (0x0056f130,
  0x005732f0, 0x00573300, 0x00573350), mission_abort (0x0048fa80, ported as
  Missions::abort_now), set_reopen_training (0x0047fe00); GameEngine's
  fade_to_state handles 0x006099a0; menu_vcall routes a shell state's +0x28.
- The purple: the program's 2D buffer after a 3D frame is the layer over
  its key colour. FIX (A): the zTwoD host's screen_grab reads the shown frame
  back (GameEngine::shown_frame) after a 3D frame.
- test_menus.py pause 60/60 (all four screens 60/60); breaks 23/23 caught
  (sound order, grab, backdrop, REPLAY's kinds, bind order, the OPTIONS
  flag, flags, the release, every press's calls);
  playtest_escape_pauses_the_mission_and_resume_returns_headless.
- Found: Escape on the pause screen skips RESUME's sound calls (sounds stay
  stopped, groups lost); REPLAY is an instant silent loss; [0x006099c4]
  (hide OPTIONS) is never written; no mode switch on pause.
- Left: the SDL host's image_release does not free an uncached image
  (0x00517b10 frees it through 0x005185f0): grabs (bake, pause backdrop)
  leak until it does.
## 2026-10-21 Playtest #10, the HUD's missing images (branch mac3/hud-images)

- Not a port bug: dmg_<part>_blk (8) and hud_othererrow_* (4) are named only
  by the exe (MW3 1.2 and Pirate's Moon); no shipped library has them.
  0x005179d0 returns 0 silently, as the port does. The suspected zimg reset
  in app_mission.cpp is not involved (the other 40 HUD images load).
- The SDL host reports a missing image once per name. Test:
  playtest_hud_images_the_data_lacks (40 found, 12 missing). hud.md section
  16, discoveries (Playing a mission), fixes.md candidate (fill-in art).

## 2026-10-21 The image_release leak (branch mac3/image-release)

- The SDL host's image_release kept every image ("the library keeps it"),
  but zImage's library reads a new copy for every first get: named images
  leaked at a count of 0, and grabs (bake, the pause backdrop) were never
  freed. Now as 0x00517b10: freed at 0 (0x00517760) and at once when not in
  the list (0x005185f0). FIX (bug) guard: a release after the free is
  reported, not freed twice (fixes.md, CHANGES.md).
- Test: sdl_host_image_release_counts_and_frees (shared count, fresh copy
  after 0, a grab released twice); it crashes with the guard taken out.
- Stopped here at Owen's pause (lead, 2026-10-21). Next when work resumes,
  in order: (1) rerun the mech lab step 4 breaks (scratchpad
  lab/breaks4.py against test_lab_screen.py) on main, keeping the output;
  report the real count and fix the test for any break still missed (the
  only saved run, 09:30 before the commit, caught 20/28; the commit's
  28/29 was not kept); (2) #8 (weapon recharge on the HUD); (3) #13 with
  #7 folded in (the HUD keys reaching the unwired HUD-command hook; keypad
  0 the zoom).

## 2026-10-21 Resumed: test_debriefing's hang, lab step 4 breaks on main

- test_debriefing.py (Owen killed it after 11 h at 193 GB, after its
  "24/24 runs identical (326147 calls" line): not reproduced at a95c56b.
  Six runs under the cap (system and .venv Python) all exit 0 in 21 s at
  0.27-0.28 GB peak, with the same 326147 calls. No threads in the probe;
  the script ends in os._exit right after that print, so no Python runs
  after it: the growth was in the exit itself (the Mac's stuck-at-exit
  state, project-ue-hangs) or in another process. New:
  tests/differential/run_capped.py (memory cap, time limit, RSS log), now
  the rule for every differential and break run (worklogs/README.md).
- Lab step 4 breaks (lab/breaks4b.py: breaks4.py with each test run
  capped) against main a95c56b, test_lab_screen.py 40: 27/28 caught. The
  only miss, "fix tons", is an equivalent mutant: the chosen row's tonnage
  ("%.1f") never holds a '%', so the exe's set_text(b) and the FIX's
  set_text("%s", b) show the same text. The FIX could never change
  anything: removed (the exe's call kept), with its fixes.md row and
  CHANGES.md line. The 09:30 run's other misses are all caught now.
  Output kept:

```
caught fill training / fill unavailable / fill full / fill other /
  default first / default check / select modified / select name
  (traces differ)
caught add enabled / rename own / save flag (dumps differ)
caught row question / row config / start rename flag (traces differ)
caught start pos (dumps differ)
caught taken same / stock refuse / overwrite q / overwrite delete /
  rename modified / cancel copy / save existing / delete own /
  delete save (traces differ)
caught edit arrows (dumps differ)
caught message sound / fix sort (traces differ)
MISSED fix tons: 40/40 runs identical (equivalent; the fix removed)
27 caught
```

## 2026-10-21 Playtest #20 with #13 and #7: the HUD keys (branch mac3/hud-keys)

- #20's cause: the controls' update (0x00427d60) clicks the zoom whenever
  the zoom window's hidden bit (+0x1514 bit 4) disagrees with the held key,
  then shows or hides it every frame. None of ControlsCalls::screen_busy /
  zoom or PartCalls::screen_flags was wired in the program: the flags read
  0 ("shown") with the key up, so the click played every frame and the
  window never opened. Wired to the zoom window (HUD +0x1508); one click
  per change now. screen_flags also turns on the torso's slow turn while
  zoomed.
- #13: the HUD keys through CommandCalls::hud, answered by app_hud.cpp's
  hud_command (keypad 0 0x00442e00, F12, T's TARGET part, R 0x0043da40, H,
  D / Ctrl+D 0x00442440, Shift+R 0x0043dc10) and Alt+R's light 0x0041ded0;
  Z and Q's pick (0x0041c1ab.., 0x004a7b40) ported into Mech::command with
  hooks for the aimed node, its object, team and refusals; 0x0041d890's
  HUD part now plays the target's own sound (slot 24, mech_<name>: none
  ships). R's radar_up / radar_down wired. 0x0043da00 was misnamed
  select_sound: it picks the mission map ("small"/"large", 0x00458590,
  not ported: the program answers none, so Shift+R turns itself off).
- FIX (bug): keypad 0 with no HUD ran 0x00442e00 on 0x1508 (fixes.md,
  CHANGES.md).
- #7's watch: nothing exploitable; Q targets allies, Z/Q over an unlisted
  object drop the target (community-issues.md, discoveries).
- Tests: test_hud.py ops 34..38 (100 seeds), test_command.py with the pick
  run for real (6096), mw3tests playtest_hud_keys_zoom_radar_mfd_headless.
  Breaks (scratchpad hudkeys/breaks.py, every test capped): 27/28 caught;
  the miss (the MFD's save when 0) cannot show. The first run's Alt+R check
  missed an inverted light (it pressed until amber); now two presses with
  the range checked.
- Left: F12's text (0x00446ad0, the objectives' lines), the mission maps'
  list (0x00458590), targets.zrd objects' node binding (0x0040eaa0) so Z/Q
  find buildings.

## 2026-10-21 Playtest #8: the weapon bars (branch mac3/recharge)

- The timers, the refresh and the bar's draw were all right: c1m1's ER
  Laser (S) (a beam: the bar is its 1250 charge) drains on a shot and
  refills in about 3.2 s, the machine guns (period 0.25 s) in 16 frames, and
  the line's value follows. The cause was the per-frame refresh: the list's
  update runs the looks only after a shot marks it (0x004464c0, +0x11438),
  through WeaponCalls::hud_fire, which the program wired in 258364d at
  10:29, after Owen's run (row #8 written at 09:23). With it unwired the
  bars stay at 1: checked by taking it out.
- Found on the way: HudWeaponLine had no destructor. The original's
  0x00445970 sets the bar's fill and empty (the texts' own images) to none
  before the Slider's 0x0059e170 releases them; without it the SDL host
  frees them twice when a HUD goes (malloc abort in the new SDL test).
  Headless tests never saw it: the default host renders no text images.
- Tests: mw3tests playtest_hud_weapon_bars_recharge_headless (F2's laser:
  the bar follows the charge down and back over seconds);
  mw3sdltests sdl_host_hud_weapon_line_shows_the_recharge (a real line with
  the game's fonts half recharged: green left of the cut, red right) and
  sdl_program_weapon_list_shows_the_recharge (c1m1 through the SDL host's
  2D, software and Direct3D layer paths: green before, red after firing,
  green again). The SDL tests reset weapon_set(): its fonts are one host's
  handles. Breaks (scratchpad hudkeys/breaks8.py): 6/6.

## 2026-10-21 Playtest #21: the mission's quit box (branch mac3/msgbox21)

- 0x0048dfe0's one caller is the Core state's start (0x00480c70): every
  mission start made it (new 0x12688) and the program reported it unported,
  which is what Owen saw late in a run. It is zTwoD's MessageBox with a
  spare SlideButton (+0x12494, id -1, unused), OK and CANCEL shown, message
  0xde1 and title 7; the frame updates it, and only a multiplayer stop
  shows it (0x0048e2d0: the command on mouse button 1 unbound meanwhile),
  0x0048e320 closes it, OK leaves the network game (0x0046d100,
  0x0047fc40(0), the fade to 0x00609848, not ported), Cancel closes.
- Ported: mech3/quit_box (QuitBox); GameEngine::message_box_new and
  message_box_quit_prompt, its vt+0/+4/+8 through menu_vcall; the hook
  Engine::network_leave (0x0046d100).
- Test: test_menus.py quit (kind 4: the probe now holds any Dialog; new
  ops M the prompt, L the close; the buttons pressed as 'O'; the message
  box's button vtables added to VT_KIND; zVideo's screen rect, the cursor
  show and the message formatter stubbed): 60/60, the other kinds still
  60/60. Breaks (scratchpad hudkeys/breaks21.py): 19/19 (the backdrop's
  hide caught once its flags joined the dump). mw3tests' HUD keys test checks c1m1 no longer reports 0x0048dfe0.
- Seen on the way: the five BindButton reports at each mission start (the
  shipped MESSAGEBOX has no YES/NO/ABORT/RETRY/IGNORE; the original reports
  them too) print their format unfilled ("BindButton (%s) failed"): the
  report printer's, not looked at.
