# Game systems log (mw3 mac 2)

Newest last. One short entry per item: what, commits, what to remember.

## 2026-10-20: mech placement 0x004161c0 (branch mac2/placement)

- Commits 68c7be9 (merge main), 27e10bd (the work); reported to mw3 mac 1.
- Ported the placement, SetPlace, the point query family (cls_di
  `ground_point`, gmod_hit point tests), the pick (veh_basic.cpp), the
  spawn 0x004782f0; docs in mission-start.md section 11.
- Remember: `test_place.py` (about 33 s) builds a mech image in the emulator
  with a copied vtable and stubs; `scratch/breaks.py` holds the 30 break
  tests (run at 100 queries). In one mw3tests process the mission world is
  kept from the first test and later engines have no camera: give the
  engine the kept camera (`set_mission_view`). A crashed mw3tests sits in
  state UE for a long time (crash report); look at the log, not the exit.
- Open: the pick's 2y + 10 step (fixes.md candidate), the player's power-up,
  zEntity's marker nodes 0x005a8390.

## 2026-10-20: the player's mech drives (branch mac2/drive)

- The power-up is Core's resume(7) (0x0041ca30(0), 0x0041cc30(0)), wired
  as GameEngine hooks; the frame: player tick -> vehicle_update ->
  Mech::update, the spawn's step too; `wire_mech_frame` in app_mech.cpp
  sets the pose/skis/collide/moved hooks; the soil table 0x0047f400 ported.
- mw3tests `mission_c1m1_player_walks_and_turns_headless` feeds a fake
  DirectInput keyboard. Tests start "as a new process" (missions() and
  mech types reset, main's pattern); the kept-world camera hack is gone.
- c1m1's player starts on the seabed under 4.86 m of water.
- Open: HUD, weapons (0x0041aba0, +0xd4), foot effects and sounds, mechs
  and vehicles as collision targets, the AI mechs' own frames.

## 2026-10-20: the shot queries (branch mac2/shot-queries)

- Ported cls_di.c's third query family: segment_list 0x4eed90 (cells
  0x4eefe0, nodes 0x4ef570 / 0x4ef980 / 0x4efaa0 / 0x4efba0, box 0x4f1470
  with 0x4ec6b0), segment_nearest 0x4eece0, sphere_query 0x4f0ac0
  (0x4f0f90, 0x4f0f00); gmod_hit model_segment 0x54cad0, polygon_segment
  0x54d760, polygon_segment_uv 0x54dd30, box_segment 0x54d2f0;
  zmth::invert_single (0x523c50 at 24 bits).
- test_shotq.py; scratch/x87.py traces an asm range's x87 sums (Ghidra's
  printed orders are wrong for these). Ground probes in a test must run on
  both sides (the point query moves cached polygons).
- Next: the fire wiring on mac2/fire (RunHooks::segment_list/segment,
  HitHooks::query to these).

## 2026-10-20: the player's weapons fire (branch mac2/fire)

- Commits f1a9e50 (invert comments, tools/analysis/x87.py), a59570b (the
  wiring and the c1m1 fire test). app_weapons.cpp wires the manager's,
  zWeapon's RunHooks/HitHooks, DamageCalls' node_part/node_owner, the Heat
  option, the frame step copies, Mech::aim_at 0x41bc20, 0x41aba0; the load
  needed InitHooks::pool_objects and the light hooks; zwep::shutdown at exit.
- The aim comes from the HUD's reticle (0x41cd40, not ported): the test
  aims for it. Group selection needs the mech's command handler (not
  ported): the test selects a hot group on the manager. The Mad Cat's first
  group is machine guns (heat 0).
- test_weapons_fly takes ~25 min (normal).

## 2026-10-20: the mech's commands (branch mac2/commands)

- Fire review fixes first (c21e6ac). Then Mech::command 0x41be80
  (mech_command.cpp, CommandCalls), Mech::heat_override 0x419190 and
  flush_coolant 0x419e60 (mech.cpp; +0x56c is now `coolant`), wired on the
  pilot's mech through WiredBuild::view_target_command; MouseMode through
  program_set_mouse_mode.
- My first mapping from the decomp had wrong pieces (0x15 falls into 0x14;
  lance orders pass an order number, not the command; jettison sound 0x10;
  0x49 is vt+0xc0). Reading the asm and decoding both jump tables
  (0x41c7d0/0x41c8d4, 0x41c974/0x41c98c) found them. Do that first.
- test_command.py: callees stubbed and logged on both sides
  (CommandCalls::own for the mech's and manager's own calls). The radar
  needs the directed sweep; random trials miss its edge.
- mw3tests: player controls are a per-process singleton (0x6056ec) that
  registers the keys once; a later test in the same process must null
  player_controls_instance() or its keys never reach the handler.
- The c1m1 Mad Cat has no jump jets; the commands test launches the Thor
  (roster 17).
- Left as hooks: the HUD keys, camera look keys, torso centre, 0x426780,
  nav points, lance orders, part targeting 0x41d810, MCB, chat, 0x3e, 0xa6.

## 2026-10-20: the AI mechs' frames (branch mac2/ai-frames)

- a9265ac: option_5f4d44 reads options().mouse_mode (program_mouse_mode).
- 1fb52df: zent::Budget::update_all (0x5a8850; per_frame 0x40 from
  0x5a8750) from player_tick's 0x4a1140 (hook renamed entities_update);
  node_mech / node_entity / the bump's contacts wired, Contact::distance
  0x4e7030 (Contact +0x1c dist, +0x20 dir); controls_calls().ai_brain
  records 0x4e9710 and smooths. node_owner 0x40e9d0 is one function now.
- The walk leaves `current` on the start slot (the second loop's end): the
  diff test found it.
- Merged main (mac 3's reticle aim); the fire test's mech_aim_at stand-in
  dropped. main's and my test_program.cpp additions conflicted at the same
  place: rebuilt the file from main plus my test (checkout --ours lost
  main's other hunks; don't).
- The probes rarely reach the Owens head-on: the bump does most of the
  blocking; node_mech's break shows as the attacker check.

## 2026-10-20: weapon effects (branch mac2/weapon-fx)

- What was missing: every RunHooks/HitHooks/InitHooks effect entry was
  unset (anims, sounds, lights, the anim lookup 0x50b450). Lasers and
  LRMs already drew (their FLYOUT models); the machine gun's tracer is a
  model light (no polygons), drawn by 0x541df0 as a D3D point (0x57dba0),
  neither ported: now `gmod_draw` model_light and `zvid` draw_point.
- The weapons' looped sounds need zSound's "common" set, which 0x490720
  (the Load state's mission reset) loads: ported into GameEngine::
  missions_reset with "mech", the dir's and the voice set.
- Ported small links: zeff anim_find 0x50b450, anim_stop_all 0x50ace0, the
  effect table's header and lookup (effects.zrd is empty in MW3 and PM),
  cls_di ground_below 0x4eda10 (NaN-exact choice), zmth quat_angles
  0x525f80, the anim copy_tree (0x4f9b50(n, 1, 1)).
- Trap: mission N+1 in one process keeps mission N's anim defs (the unload
  0x490480 with 0x50b380 is not ported; anim_load returns early on the old
  block): "Multiple parents" and freed nodes in the world. mw3tests clears
  zEffect's state in `reset_effects` after each reset_states.
- `mw3 --fire N --fire-group G` (ours) and `weapon_fx_log()` (ours) for the
  checks. MW3_TEST takes a comma list now.
- Debugging: lldb --batch hung; a temporary backtrace_symbols_fd in the
  reporting function found the caller fast. Don't `pkill -f mw3tests`: it
  hits other sessions' runs (told the lead).

### Where weapon-fx stands (saved before the reboot, 2026-10-20 evening)

- Done (in the WIP commit): every effect hook wired (anims, sounds, impact
  lights, the anim lookup and copy_tree), model lights as points (0x541df0,
  0x57dba0), the mission sound sets (0x490720), anim_stop_all 0x50ace0,
  ground_below/keep_below 0x4eda10, quat_angles 0x525f80, the empty effect
  table, mw3 --fire/--fire-group, weapon_fx_log, reset_effects in mw3tests,
  docs (weapon-effects.md, weapons.md link, discoveries, this log).
- Verified before the reboot: the c1m1 set 11/11 (with reset_effects),
  test_modeldraw (lights varied) 24,792 calls identical, test_place's
  4,000 made-up lists identical, test_matrix's surface angles; breaks
  caught: light priority bias, light fade, keep_below tie and NaN, surface
  angles, impact anims unset, common set not loaded. The fire test asserts
  the effects and passed.
- Not yet run after the last changes: the full mw3tests and mw3sdltests,
  test_weapons_run/hit, test_anim_run, test_command, test_entity_walk,
  test_collide, test_update. Run them first after the reboot (in the
  background), then commit (non-WIP message), then report to mw3 mac 1.
- Report items: tracers are 1-pixel points as in the original; the unload
  0x490480 (with anim free 0x50b380) not ported, so a 2nd mission in one
  run keeps stale anims; left as hooks: craters (zdeclient), surface marks
  (0x542060), lens flares (0x567420..), cockpit sounds 0x41e770, Betty.
- Note: hung mw3tests processes in state UE pile up after crashes; avoid
  aborting runs and never pkill broadly.

### After the reboot

- Merged main; my emulator tests (modeldraw, matrix, place, entity_walk,
  command) end with os._exit like main's.
- The first full run crashed (exit 139, no hang): with the sound sets now
  loaded, a later test's Core state played a sound into zSound's null queue.
  zSound's shutdown leaves `initialized` set (the exe never needs more), so
  a second engine in one process was refused its init ("zSndInit: FAILED"
  in every mission test before). `reset_effects` resets zSound's state too;
  now "zSndInit: PASSED" in all 13 mission tests.
- Ran: mw3tests 89/89, mw3sdltests 6/6, test_matrix, test_place,
  test_modeldraw, test_entity_walk, test_command, test_weapons_run,
  test_weapons_hit, test_anim_run, test_collide, test_update: all pass.

## 2026-10-20: the mission unload (branch mac2/unload)

- Ported 0x00490480 (`Missions::unload`, hooks in `MissionHooks`, wired in
  app_mission.cpp), zEffect's start/end (0x0050d270, 0x0050d570,
  0x0050d2b0, the anim start 0x00508fc0, free 0x0050b0e0 / 0x0050b380 /
  0x0050b430), the textures' free 0x00518b70 with 0x00518b30, the library
  close-all 0x005186b0. Docs: mission-load.md section 6.
- zSound's `initialized`: the exe's (never cleared). mw3tests'
  `reset_process_state` resets it and the texture libraries per app;
  `reset_effects` is gone (the app's start/end free zEffect now).
- Fixed on the way: the debug launch's cached slot held a freed config on a
  second launch (`slot_ = 0` in set_launch); the texture stand-in's mounts
  piled up across missions (each load now replaces the last one's); the
  stand-in no longer clears the library lists (0x005184e0 does not).
- mw3tests: a crash handler printing a backtrace, MW3_TEST takes a list,
  MW3_TEST_TRACE prints each test's start. Mac 3's left_button test lacked
  the zSound reset (aborted the full run after the fire test); fixed here.
- Test: c1m1, c1m2, c2m1 in one process. Breaks caught: no unload, no anim
  free, no texture free. No differential test (the unload has no logic of
  its own beyond the call order).
- Next: craters and quicksand (zdeclient).

## 2026-10-20: craters and quicksand, part 1 (branch mac2/craters)

- Mapped zDEClient (docs/craters.md): 40 functions (~2,700 lines) plus the
  zGeometry clipper it needs (zgeo_model, Weiler–Atherton, convexify:
  ~5,900 lines), so ~8,600 lines, not 1,700. Porting order in seven parts.
- Part 1 done: declient.zrd (0x00501ab0 with 0x00501fa0, 0x00502070,
  0x00501f90, the defaults 0x004ff920 / 0x005020f0) and seven gmod_matl
  helpers; wired (decals_load, decals_set_mode, zWeapon's defaults).
  test_zdec_init.py: 322 runs identical, 7 of 8 breaks caught (the 8th is
  unobservable on comparable files). FIX (bug): one-frame / missing
  quicksand texture.
- Next: part 2, zgeo_model's buffers and partitions.

## 2026-10-20: the placement step fix (branch mac2/place-fix)

- Owen's decision: 0x4161c0's pick gets a step of 10 (zfix bugs). His
  condition checked: placements_on_every_campaign_mission_headless lists
  every place both ways: 195 live placements in the 24 campaign missions
  and, with MW3_PLACE_POINTS (tools/analysis/aiv_points.py), all 768
  aiv.zrd records of every reader archive on their worlds. None moves; 55
  stand under other ground and land on the same surface both ways. The
  fix only matters for the mech swap 0x4783c0 (re-placed where it stands).
- fix_place_steps_ten_above checks Mech::place both ways; test_place.py
  (fixes off) unchanged.
- Next: TAG (ms timing, homing on the marked unit), then craters part 2.
