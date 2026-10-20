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
