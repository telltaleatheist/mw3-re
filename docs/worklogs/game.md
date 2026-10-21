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

## 2026-10-20: the intermittent crash after the multi-mission test (branch mac2/handle-fix)

- Cause: GameEngine::handle_of keys handles by address and keeps them;
  WiredPlayerHooks (a global) kept the last mission's and the last
  engine's controls / params / mechs handle sets. A new AI mech allocated
  at a dead AiParams' address got its handle, set_team_masks wrote the
  masks as AiParams offsets into it (centroid = 0x400000004), and
  Mech::entity_moved crashed in node_to_world. Found with a pool check in
  entity_moved (ASan cannot see it: no freed memory is read).
- Fix: the sets cleared per engine (wiring) and per mission (player_setup).
  15 + 20 runs of the pair clean (before: about 1 crash in 8).
- Open: an optimized (RelWithDebInfo) ASan build of mw3tests stops in
  Mech3App::init_instance on a stack address cut to 32 bits; a Debug ASan
  build runs both tests clean. Not chased yet.
## 2026-10-20: TAG, hard but consistent (branch mac2/tag-fix)

- Owen's decision, zfix bugs: the hold in whole ms (zwep::designate_held),
  the beam's time its 30 fps one (1.067 s; BeamWeapon::fired_time), the
  strike locked on the marked mech (WeaponCalls::strike_target via
  zwep::damage_object; wired in app_weapons for mechs).
- fix_tag_hold_and_beam_in_milliseconds, fix_tag_strike_homes_on_the_unit;
  TAG_UNIT=1 test_tag.py flies the unit lock with the original's homing:
  4/4 hits in every case. test_tag.py (the exe's path) unchanged.
- Next: craters part 2 (zgeo_model).

## 2026-10-20: the "32-bit truncation" at start-up (branch mac2/trunc-fix)

- Not a truncation: zsnd::State held `float* volume = &volume_default`, a
  pointer into itself. `zsnd::state() = zsnd::State{}` (mw3tests'
  reset_process_state, probe_sound) copied a temporary's self-pointer, so
  state().volume pointed at a dead stack slot and the options load's
  set_volume(0.5f) wrote 0x3f000000 there: in the optimized ASan build that
  was load_options' saved x22 (init_instance's `e`), giving 0x3f000000_xxxxxxxx.
  Found by watching the saved register's slot in lldb.
- Fix: volume defaults to null; State::master() is *volume or
  volume_default (the exe's static start value). The optimized ASan build
  runs the multi-mission pair clean; test_sound.py, mw3tests 100/100,
  mw3sdltests 6/6.

## 2026-10-20: where things stand (before Owen's /compact)

- Merged into main today: the unload (0x00490480), craters part 1
  (declient.zrd), the placement step fix, the handle-set fix, TAG (ms
  timing, homing on the mech), the zSound State self-pointer fix.
- Next item: **craters part 2**, on branch `mac2/craters2` (cut from main
  9b0474e, nothing on it yet). The plan is docs/craters.md section 6:
  2 zgeo_model's buffers and area partitions (0x0050da90..0x0050f950, 31
  functions, ~1,425 lines), 3 Weiler–Atherton (0x0050f950..0x005158c0, two
  or three parts, the core 0x00510da0 is 926 lines), 4 convexify, 5 the
  crater 0x004ffad0/0x004fff20/0x004fff80/0x004ff940 with the history
  vector and the touched-node std::set, 6 quicksand 0x00502230..0x005026c0,
  7 reset/restore 0x00500760/0x00500c20 (needs gamez.zbd's re-read path)
  and the wiring (zWeapon's crater_make / quicksand_make hooks are still
  unported stubs in app_weapons.cpp). Report to mac 1 after each part.
- The crater's entry: 0x004ffaa0 (gate [0x0060bf88], multiplayer's
  0x0046fd30) -> 0x004ff940; the order is zwep::CraterOrder (+0x08 the
  ground material, +0x0c slope, +0x10 depth, +0x14 radius, +0x18 point).
  Clip driver 0x0050ec10 picks the cell's polygons with flag 4 and
  0x20000 / 0x10000, 0x0050ea50 clips each through 0x00515410 (Weiler).
- Tools and tricks from today:
  - `build-asan/` in this worktree: RelWithDebInfo + address sanitizer
    (`cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=RelWithDebInfo
    -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer"`);
    mw3tests skips its crash handler there. lldb works when launched
    (`lldb -b -s cmds.txt`, `-k` for on-crash commands); address
    breakpoints do not hit, name breakpoints do; watch a callee-saved
    register's slot at `$fp - k` to find who clobbers it.
  - Port-side heap is not where every stale pointer lives: the node pool
    and GameEngine's address-keyed handles hide reuse from ASan; a pool
    check in the suspect function found the handle bug.
  - `MW3_TEST=a,b`, `MW3_TEST_TRACE=1` (each test's start), the crash
    handler's backtrace; `MW3_PLACE_POINTS` / `MW3_PLACE_REPORT` for the
    placement survey (tools/analysis/aiv_points.py); `TAG_UNIT=1
    test_tag.py` flies the unit-locked strike with the original's homing.
  - Differential tests that read a .zrd in the emulator: test_zdec_init.py
    builds the original's reader tree with 0x557180 and stubs 0x556740;
    stub material init 0x536a70's stack material to zero (0x28 bytes).
- Rules to keep: no subagents; kill only own pids; os._exit at the end of
  emulator tests (flush stdout first); CHANGES.md lines under their
  heading; no Co-Authored-By lines; noreply author email.

## 2026-10-20 — craters part 2: zgeo_model (branch mac2/zgeo-model)

- Ported 24 of zgeo_model's 32 functions into
  `src/gamez/zgeometry/zgeo_model` (+ `zgeo_weiler` with its first leaf,
  0x00515130 points_near): the colour cache 0x50da90/0x50dad0/0x50dbc0, the
  plane turns 0x50dc90/0x50dc70 ((x,y,z) -> (x,-z,y)), boxes
  0x50dcb0/0x50e3d0, coloured materials 0x50dd20/0x50de00, the gradient
  0x50e020 and UVs/colours/normals at new points 0x50df30/0x50e100/0x50e240,
  the vertex buffer 0x50f0a0, the clip polygon 0x50e450/0x50e4c0/0x50e520
  with point/edge search and inserts 0x50e550/0x50e690/0x50e5a0, the clip
  set 0x50e910/0x50e930 and commit 0x50e850, the polygon adds
  0x50dea0/0x50f4e0/0x50f640. Map: docs/craters.md section 5.
- Hooks (zgeo::hooks()): zModel's add-a-polygon 0x53c6c0 (unported, null by
  default; signature ECX model, EDX n, then points, normals, uvs, 0, 0, 0,
  material, priority, flag bit 8, zone*, colours), material_add, rand,
  node get/set model, model_release, Weiler's free 0x50fab0 (null until
  part 3). Buffers are std::vectors for the exe's malloc/realloc.
- Left in zgeo_model, for parts 3/5: 0x50e950 (the crater's node),
  0x50ea50, 0x50ec10 (driver), 0x50ef70, 0x50f120, 0x50f7b0 (need Weiler).
- Test: `test_zgeo_model.py` (in run_all.sh), probe `tests/probe_zgeo.cpp`;
  2,000 made-up runs identical at 24-bit x87; BREAK=1..9 each fail. The
  original's _ftol at 0x5ac340 jumps through the IAT: the test needs
  `cpu.import_code(IAT_FTOL, FTOL)`. Original.call does not return EAX: read
  it from the register.
- Finds: crater vertices take the cut ground's vertex colours (discoveries);
  untextured cut ground or a crater order without flags 0x1008 gets random
  colours (packed from the wrong bits); no shipped world has untextured
  ground on flag-0x10000 nodes (scratch survey over the six gamez.zbd).
  The crater order's flags are always the defaults' 0x100c (0x5825b0 sets
  only points, depth, radius), so 0x4fff80's random path never runs either.
- Next: craters part 3, Weiler–Atherton (0x0050f950..0x005158c0).
