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
