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
