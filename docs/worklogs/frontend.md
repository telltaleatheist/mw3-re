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
