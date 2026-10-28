# What a player meets inside a mission: census and plan

The in-mission counterpart of `frontend-plan.md` (item 14, 2026-10-27). For
each system in play: the original's range and what is ported, what the
program runs now (checked live), what is missing with a rough size, and a
porting order in small items, each with its test. The details stay in the
system docs; this file points to them.

## 0. How a mission is checked live

- `build/mw3 -nofmv --mission c1m1 --write-dir DIR` (a scratch write dir:
  the default holds Owen's settings) starts a mission through the
  original's states (program.md). The c4b world's missions are `c4bm4`..
  `c4bm6` (`c4m4` loads world c4, which has no such mission: the launch
  makes no mech); Operation 4 has six missions (no `c4bm7`).
- `--verbose` prints every original function the run reached that is not
  ported, with its count; `--shot F.bmp --shot-frame N`, `--click N:M`
  (fire), `--fire-group G`, `--walk`, `--objectives N`, `--win N`.
- `census_c1m1_unported_headless` (mw3tests, `MW3_CENSUS=1`,
  `MW3_CENSUS_SECONDS`, `MW3_CENSUS_OUT`) plays c1m1 with input (walks,
  turns, targets, fires each group, jumps, zooms, looks around, presses the
  HUD keys, quits), and `tools/analysis/census.py` ranks what it logged.
- About 90 headless mission tests in `tests/test_program.cpp` cover the
  systems below one by one (named in each section).

## 1. The census today

**c1m1 with input, 30 s** (`MW3_CENSUS_SECONDS=30`): 84 distinct unported
calls (163 on 2026-10-21). By what they are:

| Kind | Calls | Count |
|---|---|---|
| Plumbing the program does its own way: zVideo's devices and buffers, zClass's display, window and camera setup, the software render target 0x00557f90, fonts, zInput's init, zReader's paths | 0x00576c50..0x00576dc0, 0x00579040, 0x005799f0, 0x00579ea0, 0x0057a3c0, 0x004f3430..0x004f3ff0, 0x004f9fa0, 0x004fa0b0, 0x004fa580, 0x004fa600, 0x00557f90, 0x00558070, 0x00558290, 0x005578f0, 0x00557b30, 0x00518790, 0x00518f50, 0x00519e50, 0x0051a300, 0x00526510, 0x005269e0, 0x005561a0, 0x005561e0, ... | ~45 |
| Multiplayer only (DirectPlay's poll, the chat panel, the network weapons) | 0x00588930 (every frame), 0x0041ab90, 0x00586c20 | 3 |
| Developer only (the HUD's debug lines and their clears, hud-plan.md item 2) | 0x0040e4b0, `hud_line_clear` / `hud_text_clear` (vt+0x7c) | 3 |
| Ported, but the program still reaches the address as a hook | 0x00491d20 (Missions::check_cheats), 0x004d9840 (AiSettings), 0x0040aca0 (PlayerView), 0x004f7fe0 (list find), 0x00430aa0 (the HUD of a mech), 0x00479460 / 0x00479480 (the pilot's game by type), 0x004d72a0 | ~8 |
| Real, small | 0x004776d0 (a polygon each mech build adds, 19 lines), 0x0053bb20 (model priority, 21), 0x004a4de0 (the stealth object: back burner, vehicles.md), 0x0043b860 (the HUD's free at the end, 86), 0x00478f10 / 0x004782c0 (the launch's pilot calls), 0x00490720 (mission.cpp, 64), 0x0047f2b0, 0x004ab6b0 / 0x004ac860 (vehicles' setup), 0x004b4ab0 / 0x004b52e0 (weapon.cpp), 0x00413e60 (the globals 0x005e1f68..), 0x0040a7d0..0x0040baf0 (the app's sound set and the view's helpers) | ~20 |

**Every campaign mission, no input, 1200 frames** (c1m1..c3m6, c4m1..
c4m3, c4bm4..c4bm6, and t1): each launches the player's mech, runs and
draws; each reaches 83..90 unported calls, all in c1m1's set. Only the
c4b missions add three mech hooks: 0x0041aba0 (the attacker cleared,
damage.md), 0x0041ca30 (the HUD's fall flag) and 0x0041cc30 (Mech::Power,
ported): something in c4b's start (likely the player starting powered down
or falling) reaches them through an unwired hook.

**Seen while doing it:**

- c4m2 runs about 8 times slower headless than the others (136 s for 1200
  frames against 17): to profile (rain, the sun's flare, or the lights).
- One c1m1 run (with `--verbose --shot`) ended with exit 133 (SIGTRAP)
  after the census print; three plain reruns ended cleanly. Intermittent,
  at the quit.
- The 180 s census run did not finish within 25 minutes (the 30 s one
  takes 30 s): something late in the run, or its quit, hangs.

Shots of a live c1m1 (the start, firing, the objectives box open, walking)
and of c4m2 (the sun's flare and the rain): outbox 2026-10-27/mission.

## 2. The systems

### Mech movement and collision

- **Original**: mech.cpp, mech_anim.cpp, mech_control.cpp and the parts
  (0x00419fc0 Mech::Update, the gait, turns, jump jets, falls, the skis'
  ground probes); collision in cls_di.c and mech_collide (movement*.md).
- **Ported**: all of movement.md's list, the parts' setup 0x00464290 and
  its calls out, the collider and its box test, falls and getting up
  (movement.md Left, items 1-2).
- **Runs**: walking, turning, jumping (the Thor's flames), the ceiling
  crush, the heat shutdown (`mission_c1m1_player_walks_and_turns`,
  `_player_commands`, `_ceiling_crush_uses_the_box`,
  `_heat_shutdown_stops_the_mech`, `placements_on_every_campaign_mission`).
- **Missing**: the HUD's zoom flags (+0x1514, the torso's slow turn) and
  force feedback (movement.md Left 1): small, each under 50 lines.

### Weapons

- **Original**: weapon.cpp (the manager, groups, bins), zWeapon (spawn,
  the tick, homing 0x00585900, beams, impact 0x005826b0), landmines.
- **Ported**: the manager and its firing, zWeapon's spawn, tick, homing,
  beams and impact, the glow objects, the AMS, recoil, the mines, the AI's
  volley pick (weapons.md Left to port).
- **Runs**: firing every group, LRMs homing on a lock, lasers on the
  reticle, recoil, the mines in c1m2, turrets destroyed
  (`mission_c1m1_player_fires_on_the_owens`, `_lrm_homes_on_the_lock`,
  `_laser_*_on_the_reticle`, `_arm_weapon_recoils`,
  `mission_c1m2_landmines_go_off`, `_shots_destroy_the_srm_turrets`).
- **Missing**: 0x004b52e0 (weapon.cpp, 56 lines, reached once at the
  start; 0x004b4ab0 wired in item 2); the network registration (multiplayer).
  Open in play: playtest #23 / #24 (a beam fires only from a full charge,
  so a click while it recharges is dropped and a held click fires late, as
  the original): a candidate for Owen (fixes.md), not a port.

### AI

- **Original**: zainet*, zaitarget, zai_brain and the behaviours, the
  valves, the sensors (0x004a8cd0), the in-out boundary
  (ZAiNetMechInOut, 0x004c11f0..0x004c3ed0), the lancemates (0x0049e5e0).
- **Ported**: everything in ai.md section 6 and 17.9, the sensors and the
  boundary since (`zainet_inout`; ai.md section 6 still says not ported:
  to correct), the lancemates and their orders.
- **Runs**: AI mechs walk, pick targets, fire, die and are forgotten;
  Dominic takes orders, stops at a no-go patch and fires unordered
  (`mission_c1m1_owens_*`, `mission_c1m4_dominic_*`,
  `mission_c2m1_ai_pick_goes_to_the_hotter_mech`,
  `mission_c1m1_sensors_fill_the_contacts`).
- **Missing**: 0x004d72a0 (AiTarget::Name, ported) still reached as a hook;
  0x004d9840 likewise. Nothing else in play.

### Damage and death

- **Original**: mech_damage, mech_parts, mech_dead (0x0041e140 and the
  death animations), the cockpit's cracks, the kill credit 0x0041b420.
- **Ported**: damage.md sections 1-4, the deaths, the wreck's removal at
  180 s (with the Instant Action tally), the kill credit.
- **Runs**: `mission_c1m1_owens_dies`, `_cockpit_cracks_on_a_head_hit`,
  `_hud_static_when_hurt`, `mission_c1m4_a_wreck_is_deleted_and_forgotten`,
  `_won_and_lost_by_its_objectives` (the player's death loses).
- **Missing**: 0x0041aba0 (reached as a hook in c4b, 8 lines: the
  attacker cleared after a hit record); the torso jolt's per-frame decay
  (damage.md: "likely 0x00464ce0", but that is the rate-mode steer, ported:
  to find).

### The HUD and cockpit

- **Original**: mech_data.cpp's HUD (0x00430aa0 and the panels), the
  reticle, the zoom, the MFD and TARGET views, the message panel, the
  objectives box, the static (hud.md).
- **Ported**: hud-plan.md items 1-9 (all done), the objectives box's text
  with Instant Action's and training's headers.
- **Runs**: every panel in the shots; `playtest_hud_keys_zoom_radar_mfd`,
  `_hud_weapon_bars_recharge`, `mission_c1m1_hud_*`,
  `_mfd_shows_the_mech_and_target_its_target`,
  `_objectives_box_text`.
- **Missing**: the HUD's free 0x0043b860 at the end (86 lines: today the
  program frees its own HUD; port for the order of what goes); the
  developer lines (behind the stealth object's debug key, CLAUDE.md); the
  chat panel (multiplayer).

### The radar and map

- **Original**: the radar (0x0043d920 and family), the mission maps
  (0x004583b0 family), the markers from world.zrd (0x00459e80).
- **Ported**: all (hud.md sections 15, 19, 25; hud-plan items 7-9).
- **Runs**: `mission_c1m1_radar_shows_the_boundary`, `_radar_shows_turrets_
  and_vehicles`, `mission_t1m2_radar_shows_its_targets`,
  `_maps_and_the_radar_map_mode`.
- **Missing**: nothing seen.

### Sound and music

- **Original**: zSound (sounds.zrd, sets, 3D voices), the mission's voice
  lines and their priorities, the CD music (sound.md).
- **Ported**: zSound, the sounds' EXTRA records, the voice lines, the
  music tracks (`music/trackNN.*`), the options' SoundLOD and volume.
- **Runs**: the mission's track, step and weapon sounds, the voice lines
  (sound.md In a mission).
- **Missing**: the app's sound set 0x0040a800 / 0x0040baf0 (10 and 8
  lines, reached at the start); the CD's mixer volume (the music output's
  own stands in); the subtitles, which the exe never shows (a candidate in
  fixes.md).

### The mission script's engine calls

- **Original**: zInterp's 163 engine functions (interp.md, Engine calls).
- **Ported**: the interpreter whole; of the 163 engine entries, about 56
  are set by its defaults (by a search of the sources, not a run: lists,
  nodes, the world read, cameras' clip, lights, fog, textures, colours,
  clouds, animation paths).
- **Runs**: every mission's `.gs` with `GameZReadZBDFile`: the worlds come
  from the .zbd, so the `.gw` world-building commands do not run.
- **Missing**: about 107 entries null (camera, display and window setup,
  light setters, model and material creation, polygon building, world
  partitioning, `write_zbd`...). Most serve the loose `.gw` path, which
  the release never takes; which ones a shipped mission's live path calls
  is not yet measured (the only unknown words seen are `SetModelDirectory`
  and c1's `Object3DSetScrllAlways`).

### Render pieces

- **Original**: zRender and zModel's Direct3D path (render.md).
- **Ported**: the model draw, the scene walk, lighting, fog, the
  transparent sort, puffers, lens flares (`zrndr_flare`), the cockpit
  passes, projected shadows and lights, the casters' images and the
  per-mech upkeep, the weather (rain, snow).
- **Runs**: `mission_c1m1_mechs_cast_shadows`, `mission_c4m2_sun_flare_
  drawn_and_blocked`, `_rain_made_and_drawn`, `mission_c4b_m4_snow_made_
  and_drawn`, `_models_are_lit`, `_fog_mix_reaches_the_device`,
  `_horizon_scrolls`.
- **Missing**: the vehicles' shadow casters (the upkeep skips them "until
  vehicles are ported", and they are: render-shadows.md Open); model
  priority 0x0053bb20 (21 lines; draw order); the polygon 0x004776d0 each
  mech build adds (19 lines, to trace); the software renderer and its
  lighting, not ported by decision (CLAUDE.md, Rendering).

### Vehicles and the world's objects

- **Ported**: the basic vehicles, turrets, the field bases, the repair
  dock, the mcbwaynet and nav waynet, the trucks, the stealth object
  waiting (vehicles.md).
- **Runs**: `mission_c1m1_vehicles_run_their_frames`, `_turrets`,
  `_field_bases_repair_the_player`, `_nav_waynet`,
  `_truck_nodes_name_the_truck`.
- **Missing**: 0x004ab6b0 (85 lines) and 0x004ac860 (23) reached once at
  the start (turret.cpp / veh_basic.cpp, to identify); the stealth object
  0x004a4de0 and BASIC mode (back burner, Owen).

### Salvage

- **Ported**: the mission's salvage store, the objectives' rewards, the
  debriefing's list, the salvage screen, a field base lost (campaign.md
  sections 9-10).
- **Runs**: `campaign_operation_end_and_campaign_end`,
  `mission_c1m1_field_base_lost_cuts_the_campaign_store`.
- **Missing**: nothing seen.

### The end of the mission

- **Ported**: the objectives' win and loss, the end screen and its fades,
  the Win State and debriefing, the IA and TR Dummy States, the cleanup
  (mission-frame.md, campaign.md 11-12).
- **Runs**: `mission_c1m1_won_and_lost_by_its_objectives`,
  `campaign_menus_to_c1m1_through_the_briefing`, `instant_action_menus_
  to_wave1_tallies`, `training_menus_to_lesson3`,
  `missions_c1m1_c1m2_then_c2m1_in_one_process`.
- **Missing**: the launch's pilot calls 0x00478f10 / 0x004782c0 reached as
  hooks (ported in pilot.cpp: to wire); the quit's intermittent trap and
  the long census's hang (section 1).

## 3. Porting order

Small items, each one sitting, in the order a player would notice them.

1. **Wire the ported functions still reached as hooks**: 0x00491d20,
   0x004d9840, 0x0040aca0, 0x004f7fe0, 0x00430aa0, 0x00479460 / 0x00479480,
   0x004d72a0, 0x00478f10 / 0x004782c0, and c4b's 0x0041aba0 / 0x0041ca30
   / 0x0041cc30. Test: the census and a c4bm4 `--verbose` run without
   them; mw3tests. Done (2026-10-27): the 30 s census is at 73, c4bm4
   reaches 79, none of these listed. c4b's three were the `c4m4` runs
   (world c4, no mech); c4bm4 never reaches them. 0x00478f10 was wired
   already (its log was the debug start's stand-in pilot).
2. **The quit**: the 180 s census's hang and the intermittent SIGTRAP at
   c1m1's exit. Test: the 180 s census finishes; 20 c1m1 runs with
   `--verbose --shot` exit 0 (with AddressSanitizer once). Done
   (2026-10-27), two bugs of ours. The trap: mw3's main ran `stop_game`
   after `App::run` had returned through ExitInstance, so the world's
   sound nodes unlocked voices zSound had freed (AddressSanitizer: a
   use after free in `zsnd::unlock`); malloc caught the damage at random
   later (Metal's shader release, CoreAudio's close). The hang: 0x004b4ab0
   (the radar list's object, whose action empties the list each frame)
   was never wired, so every shot fired stayed listed and the radar's
   walk grew without end (2,500 entries by 30 s); the AMS read the same
   stale list. The 180 s census runs in 91 s now.
3. **c4m2's slow frames**: profile the 8 times slower run. Test: c4m2's
   1200 frames within twice c1m1's time. Met by item 2's radar list:
   c4m2 25 s, c1m1 17 s (136 s before); nothing left to profile.
4. **The vehicles' shadows**: the upkeep (0x004814e0) over the ported
   vehicles too. Test: `mission_c1m1_mechs_cast_shadows` extended to a
   vehicle within 100 m. Done (2026-10-27): every entry the exe's casts
   answer (vehicles, field bases, turrets, the pocket marker); a mobile HQ
   moved 40 m from the player casts a drawn 32x32 caster, the far ones
   none. `mw3 --mission c1m3 --vehicle-ahead 40 --shot` shows it.
5. **The model priority 0x0053bb20 and the mech build's polygon
   0x004776d0** (done 2026-10-27: both ported with 0x00477730, wired in the
   program and the script engine; test_ski_polygon.py; movement-ground.md,
   The skis' debug quads): trace, port, differential (test_modeldraw.py's harness
   for the first; mech-build's for the second).
6. **The start-up's leftovers that a player could see**: 0x004ab6b0 /
   0x004ac860 (vehicles), 0x004b52e0 (weapons), 0x00490720,
   0x0047f2b0, 0x00413e60: identify each, port the ones with an effect.
   Test: a differential each (the families' existing tests), the census.
   Done (2026-10-27). One had an effect: 0x00413e60 sets the hit factors
   by the network flag, and alone the limp structure is 0.4 where the port
   had the data's 0.3 (damage.md, The hit factors; test_hit_globals.py).
   0x00490720 was the program's already (the base hook's log dropped). No
   effect: 0x004b52e0 (the view's window object, its two rects the same:
   scale 1), 0x0047f2b0 (defaults the world reader overwrites, the soil
   table the program builds, the console's vars), 0x004ac860 (the
   stealth object's block: back burner). 0x004ab6b0, the mode switch, is
   resolution.md's step 3 (the port has one mode).
7. **The script engine's live calls**: measure which null entries the
   shipped missions' scripts reach (an observer count in a headless run
   over every mission); port those. Test: test_interp.py's mission runs
   with the entries real.
8. **AI hit rate at range**: in `mission_c1m4_dominic_fires_on_a_target`
   Dominic holds the trigger most of 40 s from about 80 m and lands two
   hits (the hut 30 -> 16 -> 9; item 5's rand()s moved the run). Find
   where the shots go (aim, line of sight, the lasers' recharge) against
   the exe; it may be playtest #23's firing delay. Test bed: that run.
9. **The HUD's free 0x0043b860** at the mission's end. Test:
   test_hud.py's free op; `mission_end_frees_what_the_next_does_not_need`.
10. **The app's sound set 0x0040a800 / 0x0040baf0**. Test: test_app_states.
11. **The plumbing**: mark the ~45 calls the program does its own way as
    such (silent, with a note in program.md) so the census lists only
    what is left. Test: the census's count.
12. **Docs**: ai.md section 6 (the boundary and the sensors are ported),
    damage.md's jolt decay (find its address), movement.md Left. Test:
    none (docs).

Decisions for Owen rather than ports: the beam's charge gate (playtest #23
/ #24, fixes.md Candidates), the subtitles, the other Candidates in
fixes.md. Back burner by decision: the stealth object and BASIC mode, the
software look, multiplayer (with the network work).
