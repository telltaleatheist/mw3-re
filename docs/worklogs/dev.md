# Work log: mw3 dev

The single worker session from 2026-10-25 (Owen), formerly "mw3 mac 4".
Its earlier log, the AI items and everything up to HUD plan item 1, is
`ai.md`; read its last sections for the state at the switch. The protocol
is in `README.md`.

## Queue at the switch (2026-10-25)

- Item 19, the weather: rain (c3, c4, t1) and snow (c4b m4 / m6), the
  manager's rect and tick (0x5a6790 / 0x5a67c0, renamed weather_rect /
  weather_tick), app_mission's create_rain / create_snow / weather_add.
  Rain first, snow second if it needs another sitting.
- Then `hud-plan.md` items 6-9: 0x40fac0 as one call, the mission maps
  (maps.zrd), the radar's map mode (Shift+R), the radar's polylines.
- Later: the front-end screens (campaign, Instant Action, the mech lab,
  briefing and debriefing, salvage, movies), planned the same way.

## Item 19: the weather, rain first (2026-10-25)

- Assigned (the lead): the weather base 0x5a5470 with rain 0x5a60a0 and
  snow 0x5a5ad0 (vtables 0x5d0528 / 0x5d0660 / 0x5d05e0), the manager's
  rect and tick (0x5a6790 / 0x5a67c0 renamed weather_rect / weather_tick),
  app_mission's create_rain / create_snow / weather_add, the frame order
  and blend; split rain (c3, c4, t1) and snow (c4b m4 / m6) if large; a
  differential of the update, breaks, headless on a rain world, a
  screenshot pair. Branch mac4/weather off main (now 8181f8d).
- Traced: the weather is a zTwoD object (Object2D, 0x98 bytes) on the
  manager [0x736328] (a Layer with two rect slots +0x10 / +0x14, the text
  target +0x18 and timed texts to +0x1ec). Its own slots: +0x00 dtor,
  +0x04 draw 0x5a5360 (+0x7c once per manager rect), +0x24 update (rain
  0x5a6110, snow 0x5a5b40 with 0x5a5e10), +0x7c the particles' draw
  0x5a57d0 (a quad each through draw_textured_white, the "SnowFX" 16x8
  ramp texture, clip test 0x5a5a60). Particles live in screen space and
  are re-randomized every frame (0x5a56e0); the streak is one frame's
  motion (gravity + wind - the camera's move, in camera space).
- Queue after it (Owen, via the lead, 2026-10-25: this session is the one
  working session, the lead reviews): snow (19b if split), then HUD plan
  items 6-9 in order (0x40fac0 as one call; maps.zrd; the radar's map
  mode; the polylines), then the front-end screens.
- Done 19a (rain): `ztwod/twod_weather` (base, rain, the overlay's rect and
  tick), the program's `ProgramWeather` and `GameEngine::weather_rect` /
  `weather_tick` (the rect by its number: render, display, window). Found
  on the way: the overlay layer is active from static init (0x5a4590's
  vt+4(1)) and holds its own hidden object and five texts; the streak is
  scaled to 0.1 only when |d|^2 > 0.01 (a short move is its own streak).
  FIXes: the first frame's unset size (bug), the zeroed ramp (A), the
  slant per frame rate (B, per_second). Tests: test_weather.py 400 cases
  (2 ulps / a pixel for fsin / fcos, 9 frames in 2,581), 13/13 breaks,
  mission_c4m2_rain_made_and_drawn_headless, a screenshot pair in
  claude-files/outbox/2026-10-25 (weather_c4m2_rain_on/off.png). Docs:
  weather.md, fixes, CHANGES, discoveries, symbols, twod, world-params,
  hud-plan item 5. Next: 19b, snow.

## Item 19b: the snow (2026-10-25)

- Assigned (the lead, after reviewing 19a): the snow 0x5a5ad0 (vtable
  0x5d05e0), its update 0x5a5b40 with 0x5a5e10, create_snow in app_mission;
  stacked on mac4/weather (19a being merged).
- Done 19b: `ztwod::Snow` (0x5a5ad0, update 0x5a5b40; Ghidra's 0x5a5e10 is
  its tail, not a function), the two updates' shared start as
  `Weather::motion` / `project`, a clock each (rain [0x73653c]/[0x736528],
  snow [0x736538]/[0x736518]), create_snow through `program_weather`.
  Snow keeps its places (reset only off the view: |y| or |x| > |z|, z > 1,
  !(z >= 0.5)); flakes ftol(3.5 k) pixels thick. The per_second FIX covers
  the snow's fall too. Tests: test_weather.py 400 cases (200 snow, 5,227
  frames; 8 ulps / 1e-7 of a place / a pixel, the snow's places synced each
  frame; 69 frames tolerated), 20/20 breaks, the two headless runs sharing
  run_weather_mission, a snow pair (weather_c4bm4_snow_on/off.png).

## HUD plan item 6: 0x40fac0 as one call (2026-10-25)

- Assigned (the lead): hud-plan.md items 6-9, one at a time, each branched
  off main. Item 6 on mac4/hud-frame off cdfe0f4.
- Done: `GameEngine::hud_update` is 0x40fac0 (the view target's frame, the
  message panel's update, the pocket commander; the developer lines,
  frame-rate text and console layers left out), app_states' hud() calls it
  alone. Found: the message panel's text had never been drawn (its update,
  vt+0x24, is 0x40fac0's); the c1m1 HUD test now checks it (break caught).
  WiredLaunch's mech_screen gives `pilot_hud`; hud_reset / hud_zoom hide or
  show the objectives. Noted for later (hud-plan item 6): the radar reset
  0x43d920, and app_states' `radar` / `ai_tick` are the HUD's 0x43cd20 and
  reticle update (`Engine::mech_ai` is 0x430aa0), still logged.

## HUD plan item 6b: the HUD's static and the zoom pass (2026-10-25)

- Assigned (the lead, from item 6's finding): app_states' radar() /
  ai_tick() are the HUD's: 0x43cd20 on the pilot's HUD and the reticle's
  (+0x144c) vt+0x28. Branch mac4/hud-static off main.
- Traced: 0x43cd20 is the HUD's screen static. Four channels (+0x22e08..
  +0x22e34: three static levels with a floor and a hold, and the radar's
  fade); with the lowest internal structure of the torso's three sections
  and the head under 0.4 (and GameIntensity's flag clear) it adds to a
  random one (0x43bfe0 kind 5: 0.1 a frame up to k / 4); each shown
  channel draws static (0x43cca0 -> 0x558c10: noise-table pixels under the
  level replaced by three greys); the levels decay 0.3 a second after
  their hold. The reticle's +0x28 is 0x444230 -> the zoom window's +0x28
  0x442f50 (its 3D pass, 198 lines): split off as 6c.
- Done 6b: `Hud::static_tick` / `add_static` / the draw (0x43cd20,
  0x43bfe0, 0x43cca0), `zrndr::finish_init` / `static_fill` (0x5581c0,
  0x558c10) and `GameEngine::render_reset` (0x557b20: the noise filled at
  start-up as the exe, 0x557b30 logged). The draw paths' calls renamed
  (`pilot_hud`, `hud_static`, `hud_zoom_pass`; the exe's call shapes kept as
  defaults), the mech's +0x684 written by WiredLaunch. FIXes: the 0.1 a
  frame (B, per_second), TARGET's null rect (bug), the table's spare byte
  (A). Tests: test_hud ops 39-41 (100 HUDs, 262 fills; the exe's null read
  hooked to the port's FIX), test_static.py (pixels), breaks. Next: 6c.

## HUD plan item 6d: c1m1's emplacements (2026-10-25)

- Assigned (the lead, before 6c): c1m1's "Completed: Destroy SRM
  Emplacement" lines at 00:00:01. Branch mac4/emplacements off main 0f23363.
- Found: the turrets are built (the ported loader, 14 in c1); the
  objectives' entity_find (0x4d7330) searched only the AI mechs and the
  targets, so every turret's and vehicle's destroy objective counted as
  destroyed at once. Fixed: turrets and vehicles by node name, their +0x08
  as the flag, labels through entity_name (vt+0xa4; a turret's is none).
  The c1m1 won/lost test checks the four emplacements alive and their
  objectives open, and kills them to win (break caught). Survey of every
  mission: all found but c2m3's firefly_01 / firefly_02 (the mission makes
  firefly_03 / 04): a data typo in the original, a candidate for Owen.

## c2m3's Firefly objectives (2026-10-25)

- Assigned (the lead, Owen approved): the data typo found in 6d as a FIX
  (bug). Done: `fix_objective_data` (mech_objective) after the read, keyed
  on c2 / m3, renames the two secondary destroy objectives to firefly_03 /
  04 and finds them again; objectives_load takes the mission's directory
  (null in the probes). fixes.md's Data typos, CHANGES, objectives.md,
  discoveries; mission_c2m3_firefly_objectives_open_headless (break
  caught). Next: 6c1.

## HUD plan item 6c1: the HUD's 3D views (2026-10-25)

- Assigned (the lead, the split): the views' nodes and render first. Branch
  mac4/hud-views off main 05a3b01.
- Done: zClass window_create / set_size / set_origin (0x4f9e90, 0x4f9fa0,
  0x4fa0b0) and camera_set_flag8 (0x4f34c0); Render3D::clear_depth_rect
  (a default) and zvid::depth_fill (0x576ce0 -> 0x577770's DEPTHFILL);
  ProgramHudHooks' node table and the camera / window hooks, zview_render
  through render_world (hardware only, as 0x4b52c0), dirty_rect as the
  depth fill. Test mission_c1m1_hud_views_drawn_headless (break caught).
  Open [Owen]: the MFD's mode 1 camera (0x442900 has no caller found).

## HUD plan item 6c2: the zoom window's 3D pass (2026-10-25)

- Assigned (the lead, the split): 0x442f50 with 0x4436c0, stacked on
  mac4/hud-views. Found: the zoom's zView is its own class (vtable
  0x5c0600): its render 0x443320 pushes the fog back for the pass; the
  zoom hides the player's own mech; the round window's corners are masked
  with four clear-texture fans at depth 0.99; the program never formatted
  zTwoD message texts (FormatMessage), so the zoom's range (and any other
  set_text_id text) was empty: SdlTwodHost::format_message.
- Tests: test_hud op 42 (100 HUDs, 75 passes, 120 fans), breaks,
  mission_c1m1_hud_zoom_drawn_headless.

## Playtest #23 / #24 / #28: firing, aim, recoil (2026-10-25)

- Assigned (the lead, Owen's second playtest): some clicks fire nothing
  until clicked again (#23), a laser does not land where the reticle was at
  the click (#24), no recoil (#28: WeaponCalls::recoil never set). Each with
  a headless check that reproduces it first. Branch mac4/playtest-firing
  off main.
- #28 fixed: `c.recoil` wired in app_weapons.cpp (an Arm kicks, a Hand
  passes it to its arm, other parts none). The Bushwhacker's LRM/AC group
  went from recoil 0 to 0.43 (mission_c1m1_arm_weapon_recoils_headless).
- #23 as the original: five clicks pressed and released inside one frame
  (mouse and key) all fire once the bar is full
  (mission_c1m1_short_clicks_fire_headless); a beam fires only from a full
  charge (0x4b3fc0), so clicks while it recharges are dropped. Buffering
  them is in fixes.md's candidates.
- #24 as the original: the beam fires at exactly the aim point the click
  saw, turning or not, and a held beam follows the reticle
  (mission_c1m1_laser_{lands,turning}_on_the_reticle_headless; a sideways
  1 m aim error fails both). A trap on the way: at 80 m the reticle's ray
  passes 0.9 m beside the Owens' torso (its AI turns it about 12°) and
  hits the building behind, while the arm's beam still crosses the Owens.
  Test helpers mech_group_ammo / _ready / _muzzle, mech_arm_recoil.
## Playtest #25: LRMs lock but don't home (2026-10-25)

- Assigned (the lead): in the original, lock, aim at the sky, fire, and the
  missiles climb and come down on the target; here they fly straight.
  Branch mac4/playtest-lrm off main. Lead: the program never wires
  WeaponCalls' lock hooks (entity_target, record_target, target_position,
  target_velocity, target_mech), so Weapon::try_lock (0x4b3330) finds no
  target and the shot spawns without one.
- Fixed: app_weapons wires entity_target (+0x6c), record_target (the
  contact's object), target_mech, target_position (app_sensors
  weapon_lock_point: 0x5a8630's live pointer into the target node),
  target_velocity (weapon_lock_velocity: a mech's 0x41d480 into the new
  Mech::lock_velocity +0x64c, a vehicle's live +0x30), ecm_count, fcs_count,
  equipment_in_part, lock_notice (mech_lock_warning, 0x41abd0). The
  missiles now spawn with the lock and home (traced: target_at follows the
  Owens). LRM5's TURN_RATE 0.25 (rad/s, as the exe reads it) means 10
  degrees off at 150 m hits and 22 does not; the test fires 10 degrees off,
  and fails without the wiring (record_target null: no hit).
- Open: what drops an in-flight shot's lock when its target goes.
- Test isolation: in the full suite the LRM test's E did nothing after
  campaign_menus_to_c1m1_through_the_briefing_headless (bisected). The
  player controls ([0x6056ec]) survived that test, so 0x427280 did not
  register the command callbacks again on the new zInput map each test's
  init makes (E is a callback command; throttle and fire are polled).
  reset_process_state now clears the instance, as a new process has none.
  The program makes zInput once, so play is not affected.

## Playtest #27: shots don't destroy c1m1's missile turrets (2026-10-25)

- Assigned (lead): shots at the c1m1 missile turrets don't destroy them.
  Branch mac4/playtest-turrets, off mac4/playtest-lrm (the #25 hooks).
- Plan: a headless repro firing at a tur_* node; follow the hit from the
  segment query to Turret::hit and the deactivation at health 0.
- Not reproduced headless. Projectiles from 60 m at the centroid kill all
  four SRM emplacements in two PPC hits each (healthy off, destroyed on);
  the Mad Cat with its reticle pitched onto tur_srm04 kills it with its
  lasers in three volleys. Probes found: the node origin is the turret's
  base (shots at it hit the ground); tur_srm02 is inside rktun01, whose
  walls take shots along x; teleporting the player outside c1m1's area
  sets outside_c1m1 (lose(), the debriefing 8 s later), so probes stay in
  bounds; CountingPump's limit counts passes, not frames.
- Tests: mission_c1m1_shots_destroy_the_srm_turrets_headless,
  mission_c1m1_player_destroys_an_srm_turret_headless; break (no hit
  callback set): health stays 1.000, both fail.
- Asked the lead for Owen's case (turret, weapon, what he saw).

## Playtest #26: F12's objectives box has no text (2026-10-25)

- Assigned (lead): F12 opens the objectives box with no text; its filler
  0x446ad0 (HudObjectives, from show_for 0x446820) is unported. Branch
  mac4/playtest-objbox off main 6ba7b85.
- Ported 0x446ad0 (HudObjectives::fill) with 0x446930 (its vt+0x7c, a blank
  score per line: without it the "In Progress" column sat beside the
  header), 0x446970, 0x446a20 and 0x45e080 (the "  OP: <nav>" suffix).
  ProgramHudHooks::objectives_source gives mission_objectives(), +0x56,
  the campaign's operation and mission; nav names and keys' names too. The
  objective hooks now give the cockpit (program_mech_hud) and refill the
  box on a completion (0x45e110). Screenshot via the new dev option
  `build/mw3 --objectives N`.
- Test mission_c1m1_objectives_box_text_headless; breaks: no fill (no
  lines), no blank score (columns differ). FIX (bug): the texts go in as
  "%s" arguments (fixes.md; no shipped text has a '%').

## Playtest #23/#24 reopened: the click and the beam's start (2026-10-25)

- Assigned (lead, Owen's words): a charged, ready weapon does not fire on a
  click, only when the mouse button is held; and when the laser leaves the
  arm the mech has moved on but the beam starts where the mech was at the
  click. Check the real SDL mouse path (buffered vs state, which frame the
  edge is read, a press seen only while still down), time press -> fire in
  frames, and whether the beam's origin (+0x40 muzzle) is taken at the
  press and not refreshed at the spawn. Branch mac4/playtest-click.
- Repro tools (ours): `build/mw3 --mission c1m1 --walk --fire-group G
  --click N:M` pushes real SDL mouse events (SDL_PushEvent) at frames N / M
  and logs each frame's dt, the manager's trigger and state, the beam and
  how far its origin is from the firepoint now; `--walk` presses
  THROTTLE0. Test sdl_program_mouse_click_fires_the_laser feeds SdlInput
  the same events.
- Found: no press delay. In the live program (~110 fps) every group (MGUN,
  small, medium, large lasers, LRM20) fires on the frame after the press
  event (the press frame's poll), ready or not shown at 1.00 before.
- Found: the stale beam. A short laser keeps firing after the button is let
  go until its charge drains (release 0x4b4180 stops only fire_time > 0.5),
  and its muzzle is re-aimed only on the trigger (0x4b3f40) and the held
  frames (0x4b41b0); BeamWeapon's update (0x4b4230) does not aim. Walking
  at full speed the beam's origin trails the firepoint by 0.17 m a frame
  after the release, 1.4 m by the beam's end (ERLaser_Medium, ~140 ms).
  The exe's only aim callers are 0x4b2d2d (projectile update), 0x4b36b0,
  0x4b3f6b, 0x4b41d9: the same in the original.
- Fixed (bugs): BeamWeapon::update re-aims a firing beam each frame (the
  held path's aim), so the beam leaves from the arm after a click. The SDL
  test runs both paths: fixed 0.00 m, the original 1.28 m behind; the live
  program likewise (0.00 m over frames 404..415).
- "Has to hold it to fire" not reproduced: every press fires on its frame.
  Fits the original's rules: a click before the charge is completely full
  does nothing (0x4b3fc0) and a hold fires the moment it is; in chain fire
  a click fires only the current weapon. Asked for Owen's case.

## Playtest #22: ground textures blink (2026-10-25)

- Assigned (lead): the ground's textures blink in play. Branch
  mac4/playtest-blink off main 0e5ee80.
- Tools (ours): `build/mw3 --shot F.bmp --shot-frame N --shot-series K`
  writes the K frames before N as F_<frame>.bmp; `--tap DIK@N[:M]` presses
  a key as an SDL event (X 0x2d for the outside view, Left 0xcb to turn);
  tools/analysis/blinks.py marks pixels that differ from both neighbouring
  frames while those agree.
- Not reproduced: c1m1 still (water and shore animate, a mast, birds),
  walking in the cockpit (the frame's edges bob, birds), outside still (0
  px) and turning; c1m2 still (a parked vehicle's edges flicker: its
  model's faces, not the ground); c2m1 still (0-1 px). Code: D32 float
  reversed depth from the game's sz, cycled vertex uploads, no texture
  eviction in zRender or Render3D. Questions to Owen through the lead.

## Playtest #29: the MFD's three modes, the mode-1 camera with 6c3 (2026-10-25)

- Assigned (lead): the MFD's mode-1 camera caller (MFD vt+0x8c, 0x442900)
  with hud-plan 6c3 (TARGET's object view 0x4428b0); check all three D
  modes against Owen's description (1 the own mech in 3D from the front,
  2 the parts damage diagram, 3 the armour bar graph, green, red where
  damaged), a screenshot each. Branch mac4/playtest-mfd off
  mac4/playtest-blink (its --tap).
- Found the "caller": 0x442900 is no MFD slot but the MFD view's zView's
  render: 0x442620 sets the view's +0x34 vtable to 0x5c04f8, whose +0x04 is
  0x442900 (the plain zView 0x5c9118 has 0x4b52c0). It puts the camera 13 m
  before the mech (local -z) at 1.25 x the centroid's height and draws the
  mech alone (0x4428b0). Ported in ProgramHudHooks (zview_render dispatches
  on HudMfdView; mfd_view; view_draw_node for 0x4428b0, also TARGET's 6c3).
- Screenshots: mode 1 the Mad Cat from the front, 2 the parts diagram,
  3 the armour bars; Owen's description. Pitfall: a reused --write-dir keeps
  the view option a --tap of X set (the next runs start outside).
- Found and fixed a crash: zutl::format_message_v read every %N as a string,
  so the radar's nav line 0x229 "OP: %1 [%2!d!m]" crashed with the nav point
  under 1 km. It now reads each by its !type! (msgdll_formats_each_argument_by_its_type).
- Test mission_c1m1_mfd_shows_the_mech_and_target_its_target_headless;
  break: the MFD view as a plain zView (camera at the origin, no node).

## Playtest #30 / HUD plan item 9: the radar's boundary lines (2026-10-25)

- Assigned (lead): no mission boundaries (yellow and red lines) on the
  radar; the radar's polylines (hud-plan item 9, needs zTwoD's PolyLine).
  Branch mac4/playtest-radar off main f5e2ecf.
- The polylines are the mission boundary, not the nav route: 0x43d920 (the
  mission start's, 0x491696) takes the player's settings' vlist regions
  reg_inside_ (0x4d92f0) and reg_outside_ (0x4d9360), counts their corners,
  colours them yellow (c3,ad,03) and red. The draw (0x43dd00 step 5) was
  already ported. Ported the reset (HudRadar::reset_boundary) and the
  program's hooks (radar_boundary, radar_point_count, radar_point); the
  launch's hud_43d920 finds the player's HUD (its handle is truncated).
- Survey: shipped boundaries have 6..12 corners (a polyline holds 21, the
  radar writes count + 1): no overflow. Screenshot: c1m1's radar shows both
  lines just south of the start.
- Test mission_c1m1_radar_shows_the_boundary_headless; break: no region.

## HUD plan items 7-8: the mission maps and the radar's map mode (2026-10-25)

- Assigned (lead): item 7, the mission maps (0x4583b0 and family: maps.zrd
  read at the start, freed at the end); item 8, the radar's map mode (the
  map drawn under the contacts, Shift+R). Branch mac4/hud-maps off
  mac4/playtest-radar.
- Ported the map family into mech3/mission_map (mech_map.cpp's): the map
  class 0x4580c0 (an Image), the read 0x4583b0, load/unload, find 0x458590,
  pick 0x4582a0 ("large", else the first loaded), point 0x4586e0. Wired the
  startup read and exit free (GameEngine maps_init / mech_data_shutdown),
  the cleanup's 0x4582c0, the briefing's pick into missions +0xbc (and the
  debug start's, ours), the HUD's map hooks and 0x491c80.
- Test mission_c1m1_maps_and_the_radar_map_mode_headless (image stand-ins:
  the headless zTwoD host loads none). Live: Shift+R draws c1m1_small in the
  radar's box with the mech's circle at its pixel.
- Break: the briefing's pick not stored: no map at the start (c1m1 expected).
  With Shift+R on too, that run crashes at the quit's cleanup instead:
  zsnd::snapshot calls out()->get_status for a dup voice while the headless
  test's zSound has no output (hooks().output null). Open, test-only: the
  headless runs give zSound no output; a dup appears once Shift+R plays
  over an empty mission map.
- The full suite first crashed in the new test: its image stand-ins
  answered every name while the maps' path was set (a world texture with
  no fields scrolled); now only the maps' bitmaps. And the maps' read flag
  outlived a test (0x4584e0 keeps it, as the exe); reset_process_state
  clears the maps as a new process has none.

## Front-end census and plan, docs/frontend-plan.md (2026-10-25)

- Assigned (lead): a census and plan like hud-plan.md. The campaign loop as
  a player sees it (main menu, pilot, campaign / Operation screen, briefing,
  mission, debriefing, salvage, mech lab, next mission, save and load), then
  Instant Action setup, training and the movies; per screen its object and
  state, what is ported, what runs now (live), what is missing, the size;
  a porting order in small items, the campaign loop first (c1m1 -> c1m2
  with salvage and the lab), each with a test plan. For review before item
  1. Also: the c1m2 parked vehicle's edge flicker (#22) as an open render
  note. Branch mac4/frontend-plan off main 78b1f9b.
- Wrote docs/frontend-plan.md: the loop's screens with their ports and
  sizes, IA, training, the movies, a porting order of 13 items (salvage
  first). Live through the menus with a new driver in mw3_main (ours):
  `--click-at X,Y@N` (game pixels, zInput's 0x51b440), `--tap`, `--shot`
  from the plain start (the stale "main menu not ported" line removed).
  Seen: main menu, pilot screen, c1m1's briefing (MECHLAB and SALVAGE grey
  on Op 1 Mission 1, as 0x408ef0 has it). render.md 6(c): c1m2's parked
  vehicle edge flicker, open.

## Front-end item 0: --briefing (2026-10-25)

- Branch mac4/fe-briefing off main 39e1251. `--briefing c1m2` (ours): a
  pilot "Briefing" saved with its newest campaign at the mission, then the
  driver clicks CAMPAIGN, the pilot, ACCEPT; the Campaign state goes on to
  the Load state. Two dead ends first: post_intro = Load (the briefing came
  up but the load stalled at "Loading Textures (0/17)" and the quit hung
  waiting for it: the Main state's set-up was skipped), and app.replace from
  the frame hook (queued but never run there).
- Seen: c1m2's briefing (map, route, MECHLAB and SALVAGE lit); MECHLAB opens
  the lab with an empty stockpile (a new campaign's store fill 0x470e30 /
  0x4733b0 not ported; plan item 1a added).

## Front-end item 1a: a new campaign's store (2026-10-25)

- Branch mac4/fe-store off main 724b11c. Finding: already ported
  (0x4345a0 with 0x4733b0, carried by 0x434e00); `--briefing` built its game
  at the mission (no stock, no field bases: the salvage capacity would be
  0). It now steps the pilot's new campaign with next_mission; the lab shows
  the Bushwhacker. 0x470e30 is the lab / IA / MP / training store (item 12).
- campaign_menus_to_c1m1 now checks c1m2's store: 0 mechs, 4 kind 1, 3
  ammo, 3 kind 3. Break (no new-campaign stock): a spare mech appears and
  the lab's ACCEPT checks fail. The weapons and ammo counts are the same
  either way, so they are not (only) the stock's: the player's loadout
  moves between the slot and the store too; not traced further.
