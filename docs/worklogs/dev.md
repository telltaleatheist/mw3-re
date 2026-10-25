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
