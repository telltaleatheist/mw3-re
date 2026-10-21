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

## 2026-10-20 — craters part 3a: Weiler's state and leaves (branch mac2/weiler1)

- Weiler is split 3a/3b/3c (craters.md section 5, "Weiler–Atherton, part
  3a"). 3a ported in `src/gamez/zgeometry/zgeo_weiler`: the state (0x28cc;
  region +0xc pool, polygon +0x28/+0x2c, edge/contour/crossing pools
  +0x34/+0x48/+0x5c, result pools +0x70/+0x84/+0x98/+0xac, side tables
  +0xc0/+0x14c0, offset +0x28c0..c8), weiler_new 0x50f960 / renew 0x50fa70 /
  free 0x50fab0 / region 0x50f950, drop_repeats 0x515360, swap_axes
  0x513680, recenter/uncenter 0x514c40/0x514dd0, rings and contours
  0x50fe70/0x5146a0/0x5129a0/0x514680/0x5136f0, reset 0x514d10, side
  tables 0x513750, the case 0x50ff70 + 0x510180 + point-in-polygon
  0x513cf0, results 0x5139e0 / 0x513a90 (+ bridge 0x515040), plane z
  0x514e40, leaves 0x514f80 / 0x515170 / 0x515410 (the snap from 0x50ea50;
  NOT the clip entry, which is 0x50faf0).
- Edge layout: +0 prev, +4 next, +8 flags, +0xc/+0x10 points, +0x14
  divided, box +0x18/+0x1c/+0x24/+0x28, +0x30 crossing, +0x34 mark, +0x38
  contour. Result (0x20): three {count, ResultContour*} lists + point
  count/points; ResultContour = {points, first float index}.
- FIX (bug): pools are deques (the exe reallocs under live pointers), side
  tables sized to need (the exe's 1,280-float tables overrun). fixes.md +
  CHANGES.md (crashes heading).
- Test `test_weiler.py` (run_all), probe `tests/probe_weiler.cpp`: 2,400 runs
  identical; BREAK=1..5 fail. The original side replays the driver's prelude
  (0x5128e0 x4 for the result pools, the out struct) and calls the pieces;
  sizes must come from the state after drop_repeats (+0x14).
- Next: 3b, the crossings 0x510250 (321 lines) with 0x513860 / 0x513930.

## 2026-10-20 — craters part 3b: shared edges (branch mac2/weiler2)

- 0x510250 is WeedOut (shared/collinear edges), not the crossing search; the
  crossings are found in the walk 0x510da0 (3c, with 0x513860 divide).
  Ported weed_out 0x510250 and split_pair 0x513930 from the asm (the
  pseudocode swaps several branches): registers edi R, [esp+0x14] R2, esi P,
  [esp+0x10] P2, [esp+0x48]/[esp+0x4c] the cached R.a/R.b (updated by the
  cases), jump table 0x510d64 (codes 3..15; 4 and 8 nothing).
- Pool reworked: 3a's deque did not keep a ring contiguous across its
  blocks, but the exe walks rings by stepping a pointer (510250 does). Now
  each alloc(n) is one contiguous run in never-freed chunks, logical indices
  in `slots`; Pool::index_of for the probes.
- Test: test_weiler.py `weed` (800 grid pairs via weed_case()); every case
  branch hit (scratch/cov_weed.py hooks the branch addresses). Regions up to
  150 points in the build test. 3,200 runs identical; BREAK=1..6 fail.
- Next: 3c, the walk 0x510da0 (926 lines) and the rest, then the driver.

## 2026-10-20 — craters part 3c, first half: the walk (branch mac2/weiler3)

- Ported (zgeo_weiler): walk 0x510da0, intersect 0x513f20, crossing_type
  0x514280 (table 0x5ec348, nudge constants 0x5cdd60/64 = -1e-5 / -0.99999),
  divide 0x513860, corner_side 0x514730, corner_pair 0x514840. Edge +0x30 /
  +0x34 are `start` / `end` crossings (renamed from crossing/mark); Crossing
  = point[3], links[8], type. Edge points may point into crossing records
  (probe point_ref kind 4).
- The pseudocode of 0x510da0 matched the asm where checked (type 3, 4/5,
  sub 10); only the 514730/514840 call registers are missing from it:
  514730 types 13/16 (P.prev, P, R2), 19 (P, P.next, R), 22 (P, P.next,
  R2), 6/7 (R.prev, R, P/P2), 8/9 (R, R.next, P/P2); 514840 types 12
  (P.prev, P, R.prev, R), 15 (P.prev, P, R2.prev, R2), 14/17 (P.prev, P,
  R2, R2.next), 18/21 (P, P.next, R2.prev, R2), 20/23 (P, P.next, R2,
  R2.next). In 0x514840 the last two tightness tests use e4.b (pseudocode
  reuses the variable).
- FIX (bug): the original loops forever on a polygon corner on a region
  edge within float error (crossings re-made, links cycle). Port: step
  guard per region edge, zerr line 0, returns 1. fixes.md + CHANGES.md
  (crashes). The test spots the guard (GAVE_UP marker) and checks the
  original under a 20M-instruction budget instead (walk_hang).
- Test test_weiler.py `walk` (1,200 cases: crater rings vs crossing
  triangles/quads, grid pairs, corners on region points/edges, edges through
  region points); every type and type-3 sub-case reached
  (scratch/cov_walk.py). Corner results 4/6 rare or not reached.
- Next (3c second half): the contour merge 0x5129f0 + validate 0x5154d0,
  contour count 0x513390 (+0x514710), output 0x513480 / 0x5135a0, the
  driver 0x50faf0; then the whole clip end to end (also from zgeo_model's
  0x50f120 / 0x50f7b0 later in part 5).

## 2026-10-20 — craters part 3c, second half: contours and the driver (branch mac2/weiler4, from main 1c2bd1e)

- Ported (zgeo_weiler): validate_crossings 0x5154d0 (needed links per
  type as bit masks, from the asm; jump table 0x515864), merge_contours
  0x5129f0 (jump table 0x513330; the new_contour edges per type are only in
  the asm, listed in craters.md), next_edge 0x514710, count_contours
  0x513390 (+0x28c9 renamed `untouched`), output_contours 0x513480,
  output_contour 0x5135a0, free_result 0x50fe10, clip 0x50faf0 (ECX w, EDX
  want, stack points, n, Result).
- Driver asm checked against the pseudocode: matches. Quirk kept: code 1
  (apart) returns before uncenter/swap back (craters.md).
- The exe's fprintf(stderr) lines are std::fprintf in the port (not zerr);
  the test stubs IAT 0x5bb62c.
- Test test_weiler.py `clip`: 1,500 cases, each one or two polygons in turn
  through one Weiler (walk_case() / trivial_case()), want 1..7; compared:
  code, the Result lists and points (unless 0), the polygon buffer after,
  the contours (flags, count, has edge) and `untouched`. ONLY=<section>
  runs one section. BREAK=8 (polygon z), 9 (want).
- FIX (bug), second freeze: 0x513390 loops forever when a merged contour's
  links cycle without returning to its first edge (grid shapes: a shared
  edge plus a touching corner). Port: steps > edges.count (reset on the
  turn-round) → zerr line 0, count_contours returns 0, clip fails. And
  0x5135a0 writes on forever when the first edge's prev is off the ring;
  the port stops after one lap (unless the lap ends at `last`, which is
  legitimate when prev == first) and reports, the result kept (the whole
  contour). Both use the GAVE_UP marker; the test checks the original
  never returns (5M-instruction budget for clips).
- Coverage (scratch/cov_clip.py): merge types 4, 5, 10, 11, 12, 14, 15,
  17, 18, 20, 21, 23, 24, 25 and both 24/25 branches reached; 6..9, 13,
  16, 19, 22 never: the walk rewrites those types after judging corners
  and keeps them only for a "not a corner" result (transcribed from the
  asm only). Validation failures (clip 0) occur (~3%).
- Results: test_weiler.py seeds 1/2/3 identical (5,846 / 5,843 / 5,835 runs;
  seed 1: clip 0/1/2/3/4 = 45/351/705/163/192, 44 clip hangs, 10 walk
  hangs); BREAK=1..9 all fail; mw3tests 102/102, mw3sdltests 6/6.
- Next: part 4, convexify 0x5158c0..0x5166c0 (driver 0x5162d0: 3- and
  4-point polygons by hand, more through the recursive triangulator
  0x5166c0 / 0x516a50 (leftmost-vertex diagonal); 0x515be0 the crater
  ring's triangles between two rings, with the globals 0x60d458 (points),
  0x60d460 (triangles), 0x610460 (count), 0x610468 (plane, 0x515f10)).


## 2026-10-21 — craters part 4: convexify (branch mac2/convexify, from main 5d43014)

- Ported (new zgeo_convexify): convexify 0x5162d0 (quads by hand: the
  first corner p_k p_k+1 p_k+2 turning > 0 is reflex, (k+1)%4 odd cuts
  1-3, even 0-2), triangulate 0x5166c0 (index records of stride 3, or 2
  with flag 1; the 3-vertex top call returns identity indices), split
  0x516a50 (leftmost vertex; y from rec[1] for the search and range, from
  x+1 in the inside tests), ring_triangles 0x515be0 with 0x5158c0 /
  0x515990 / 0x515aa0 / 0x515ae0 / 0x515b30 (the "o1 + o2 + k != 3" skip),
  plane_of 0x515f10 (bit-trick sqrt), plane_z 0x5160e0, reverse 0x516120,
  face_up 0x516190. Globals (0x60d458 points, 0x60d460 triangles,
  0x610460 count, 0x610468 plane) are locals/arguments in the port.
- The normal's square sum is x² + y² + z² by the FPU stack (pseudocode
  shows z² first): changed the last bit in 54/400 planes until fixed.
- FIX (A): 0x515be0's 32-int edge list on the stack → vector.
- Test test_convexify.py (in run_all): 1,500 lists of 1..4 star/grid
  polygons (2..14 points, both ways round) and 800 ground polygon + crater
  ring pairs (sloped, 4..40 ring points, each triangle face_up'd). Seeds
  1/2 identical (2,300 runs); BREAK=1..3 fail; every branch reached
  (scratch/cov_cvx.py: quad convex/odd/even, all four recursion paths,
  inside found / not, the sum-3 skip). Ring triangulation never came out
  short of np + nr in 3,000 random cases.
- Next: part 5, the crater (0x4ffaa0 / 0x4ffad0 / 0x4fff20 / 0x4fff80 /
  0x4ff940, history, set), zgeo_model's clip 0x50f120 / 0x50ec10 /
  0x50ea50 / 0x50f7b0, wired so a missed shot cuts craters in c1m1; then
  report which merge types (6–9, 13, 16, 19, 22) real craters hit.

## 2026-10-21 — craters part 5a: zModel's polygon add (branch mac2/crater, from mac2/convexify b3aa624)

- Ported into gmod_const (gmod_const.c in the exe): model_add_polygon
  0x53c6c0, drop_colinear 0x53a530, add_fan 0x53a930, add_pieces 0x53ac10,
  add_record 0x53c370, find_vertex / find_morph_vertex / find_normal
  0x539d80 / 0x539ec0 / 0x53a070, fill_record 0x53c500, flatten_uvs
  0x53bdc0, uv_gradient 0x53c270, snap_uvs 0x53af50, the merge precision
  0x538050 / 0x538060 (0x5ee3e0). Constants: planarity 0.005 (double
  0x5ee3d0), colinear 0.001 (0x5ee3d8), 48 verts (0x5ee3e4), 57.6 / 921.6
  limits. 0x50da70 = zerr level ECX (1) with the 0x73f780 text.
- Pseudocode traps: the colinear square sum is x²+y²+z² (pseudocode shows
  z first); the Material's "textured" is its byte +1 bit 0 (the exe reads
  it as a ushort & 0x100).
- Test test_gmod_add.py (in run_all): 150 models built by 1..14 adds each
  (flat, tilted, bent, grid, colinear, repeated, line, wall, 46..60-point,
  under-3), textured or not, normals, colours, morphs, both precisions;
  model dumped after each add. Seeds 1/2 identical (1,180 / 1,074);
  BREAK=1/2 fail; every branch reached (scratch/cov_gadd.py: fan, pieces,
  drop, discard, morph, normals, all three UV axes).
- Part-4 review follow-ups (mac 1): the ring's edge list is FIX (bug); the
  1,024-triangle array note; NaN compares as the exe's (zeroish() for
  `test ah,0x40`, !(x > 0) for `test ah,0x41`) in plane_of / face_up and
  in the new gmod code; test_convexify gained plane / face_up cases
  (degenerate, NaN, turn 0, 3..6 points) and rings with zero-length or
  collinear edges (3,100 runs identical; NaNs compared as one value:
  x87's default NaN is negative, ARM's positive).
- Next: 5b, zgeo_model's clip (0x50e950, 0x50ea50, 0x50ec10, 0x50ef70,
  0x50f120, 0x50f7b0, 0x4eb6a0) and the crater (zDEClient 0x4ff940 ..
  0x500850, 0x500600 / 0x500da0), wired defaults (model_add hook →
  model_add_polygon), end-to-end on the worlds (test_scenewalk loader).

## 2026-10-21 — craters part 5b: the clip and the crater (branch mac2/crater)

- Ported: zgeo_model crater_node 0x50e950, snap_outline 0x50ea50,
  clip_cell 0x50ec10, clip_node 0x50ef70, clip_model 0x50f120, clear_of
  0x50f7b0 (+0x4eb6a0 inline, 0x539cc0 inline); gmod_const expand_strips
  0x553160; new zdec_crater: crater_make 0x4ffaa0, crater_instance
  0x4ff940, crater_build 0x4ffad0, record_new/free 0x4ffe80/0x4ff8f0,
  crater_clip 0x4fff20, crater_tessellate 0x4fff80, history_push 0x500850,
  note_touched 0x500600 (std::set), world_node 0x5020b0, cell_at 0x502090.
  zgeo Hooks gained model_alloc and world_node; model_add defaults to
  zmod::model_add_polygon, weiler_free to zgeo::weiler_free.
- Finds: ZDEC_FEATURE is the crater node's own name (record at +0x40):
  the 5 m test is crater-vs-crater (discoveries corrected). The record is
  freed while the node points at it (FIX bug: features kept). Untextured
  order → null UV write crash (FIX bug; test confirms the crash). The
  normals argument 0x5eb910 is 4 up vectors. The floor centre is 2 depths
  below the hit. The changed-node list overrun (FIX bug, vector).
- 5a review follow-ups (mac 1): colinear drop FIX (bug) behind zfix bugs
  (other arrays compacted; real craters: 37 drops with UVs in 8,166 adds);
  test_gmod_add's fix_colinear section compares the port with the fix on
  against the original fed cleaned arrays (233 identical); kPlanar
  comment (the debug console's 0x538070 / 0x538080); add_record's
  new[] note; the planarity sum already in x87 order.
- Test test_crater.py (run_all): 40 craters per world on the six worlds
  (test_di.World loader; stubs memmove, sprintf "%s", 0x500850 history,
  0x500600 set, 0x508aa0 dust), plus an untextured crater per world that
  must crash the original. 246 identical (158 made). BREAK=1/2 fail.
  Real craters hit Weiler merge types 4, 5, 10 only (scratch/cov_crater.py).
- Next: 5c, wiring crater_make into zwep (app_weapons r.crater_make), the
  veto, the dust anim, and a c1m1 run.

## 2026-10-21 — craters part 5c: wiring (branch mac2/crater)

- zwep's crater hook (app_weapons r.crater_make) is zdec::crater_make;
  the mission's decals_load (app_mission) sets zgeo's world_node to
  zdec::world_node and the dust to zeff::anim_start_at (0x508aa0: no
  node, zero rotation). The veto (multiplayer) stays null. clip_cell
  guards a missing world node.
- Not run in the game: mw3 needs the GPU (SDL GPU even --hidden), and
  Owen's standing rule is no GPU use while he streams without asking.
  Asked mac 1 to clear a c1m1 run (`mw3 --mission c1m1 --fire N --frames
  N --shot`); test_crater covers the pipeline on the real worlds.
- Next: part 6, quicksand (0x502110 ..), with the movement question
  (does the soil slow or sink a mech).

## 2026-10-21 — craters part 6: quicksand (branch mac2/quicksand, from mac2/crater 5cfba30)

- Suggested c1m1 check for Owen (GPU): `build/mw3 --mission c1m1 --fire 60
  --frames 300 --shot crater.bmp --hidden --nosound` with a --pos/--look
  aimed at the ground; a missed shot should leave a crater.
- Ported (new zdec_qsand): quicksand_make 0x502110, record 0x5025e0 /
  0x5020c0, build 0x502230 (the crater's placement, now shared as
  zdec::place_ring), clip 0x502660, tessellate 0x5026c0 (pit node in the
  still frame, lid node at the hit's height in the animated material,
  UVs 4.5/(2r), the lid on the inner UVs); zwep's quicksand hook wired.
  The order's own material words (x08[0..1]) are 32-bit in the port's
  QuicksandOrder: only the zDEClient defaults (0) are supported (every
  caller).
- Movement answer (Owen): no slowing or sinking; zModel soil 3
  "quicksand" maps to game soil 0 (the game's table has no quicksand);
  the lid is ground-level and walkable. craters.md section 3,
  discoveries.
- 5b review items (mac 1): dust null guard marked FIX (bug); clip_model's
  code-0 path notes the exe leaks the Result too; the Feature on a
  model-less node noted.
- Test: test_crater.py mixes quicksand pits (a quarter) with the craters
  (veto 0, switch on, cycle/still materials set on both sides).

## 2026-10-21 — craters part 7: reset and restore (branch mac2/crater-reset, from mac2/quicksand 6d6f00e)

- The gamez.zbd re-read path was already ported (zcls::reread /
  reread_node / zmod::reread_model, 2026-10-20).
- Ported (zdec_crater): craters_reset 0x500760 (reread with children, the
  cell count through parents[0] / 0x4f22f0, ZDEC_FEATURE nodes on list 6
  detached, model released, node_delete), history_clear 0x500af0,
  decals_free 0x502020 (wired to the mission's decals_free and the app's
  decals_shutdown), history_each 0x500c60, zEntity's history_save 0x500b50
  ("Dummy" with the reset flag, "Crater%d"/"QSand%d", write_record hook for
  0x5a8a50) and history_restore 0x500c20. The touched set orders by pool
  index (the exe's by address = pool order).
- FIX (A): materials in history entries and records are table indices
  (the exe's 32-bit pointers), the shooter 0; restore maps them back.
  QuicksandOrder's x08[3] became cycle / still handles and x10 (mac 1's
  part-6 review item 1); probes zdec / weapons_fly adapted.
- Part-6 review items: the Feature-copy note at the pit's free, the
  buffers-free-themselves note.
- Test: test_crater.py runs the original's history/set code for real
  (static ctor 0x500670; new/delete/_Lockit/atexit stubbed), reads the
  history from 0x60bf94; per world after the craters: zEntity save
  (0x5a8a50 stubbed collecting), restore of the Dummy (the reset: re-read
  models in full, crater nodes gone, cells 0) and of every record. The
  crater entry's 12th word (exe stack garbage) is masked. Seed 1: 422
  identical (170 restores).
- Craters (parts 2..7) are complete. Open: the in-game c1m1 check (GPU,
  waits for Owen).

## 2026-10-21 — c1m1 toward playable: the order (branch mac2/c1m1-survey, from main a5a0f35)

Order (by what a player notices first; from a headless survey,
`MW3_SURVEY=1 build/mw3tests mission_c1m1_unported_survey_headless`: 600
Core frames walking and firing, 155 unported calls; the front end's, the
HUD's, the renderer's and the network's left to their owners, the AI on
hold):
1. The mech parts' outside calls (`PartCalls`, mech_parts.h) are not wired
   in the game at all (null calls are skipped silently, so the survey does
   not list them): footstep sounds, footprints and splashes, what a foot
   steps on, the arms' aim at the target, the cockpit's look limits.
2. Surface marks: the impact type 0x5426a0 and the marks 0x542060
   (gmod_cookie) where shots hit (632 calls in the survey).
3. The mechs' level of detail 0x41e870 (detail bits by camera distance,
   1,536 calls) or the pilot's update 0x4783a0, whichever shows more.
Later: 0x5a85c0 zEntity3d's refresh (1,536), 0x41e3f0, 0x4edab0 on the
view's ground, the anim lookups 0x50b450 (92).

- Side find: player.zrd's qsd_gravity (and wat_gravity) are written at
  0x49e8be..0x49e901 and never read: no gravity change in quicksand or
  water (craters.md section 3).

## 2026-10-21 — c1m1 item 1: the mechs' outside calls (branch mac2/c1m1-survey)

- Found: the mech code's call tables were mostly unwired in the game
  (null calls skip silently, so the survey never showed them): PartCalls
  entirely, MoveCalls, most of HeatCalls (the heat model's throttle was 0
  in the game!), DamageCalls' effects, CollideCalls' touch.
- Ported: the foot's wake (mech_wake: 0x0045f980 / 0x0045fa00 /
  0x0045fa70 / 0x0045faf0 / 0x0045fb50 / 0x0045fb80 / 0x0045fab0; three
  mech_leg_wake anims by |throttle| 0.2 / 0.5) and the sounds' EXTRA
  records (sound_extra: 0x004e6b00, 0x004e6a60/0x004e6aa0, 0x004e6be0,
  0x004e69a0, 0x004e6cd0, 0x004e6d80, 0x00432990), now zSound's load and
  free callbacks (GameEngine::sound_load / sound_shutdown_groups).
- Wired (app_mech wire_mech_parts): PartCalls (register_part = the part
  list, wake, step sounds, prints, step_on = zwep::touch on a touchable
  contact, look limits from the cockpit camera +0x68/+0x74/+0x70 (its
  vt+0x1c 0x004d2a70 returns itself), aim from the weapons' +0x0c/+0x18,
  ground_at = zcls::ground_point, sound); DamageCalls spawn_effect(_at),
  node_set_active/name, stop_foot_effect; MoveCalls pilot_sound (at the
  mech), fade_sound, stop_effect, weapons_enable; HeatCalls throttle,
  play_sound(_at), stop_sound; CollideCalls node_touchable (handler > 1
  with slot 1) and touch. zwep::TouchFn now returns the handler's float
  (the health left: 0x004d26d0, 0x00502df0), as the collider reads it.
- Find: the subtitle switch [0x0060b878] is never set: radio subtitles
  never show (discoveries, fixes.md candidate). player.zrd's quicksand and
  water gravities are never read.
- Tests: the walk test now checks step sounds (>20), prints or wake, and
  pilot sounds (engine loops); mw3tests 105/105. Not differential: the
  wake and the records are small; the records' parsing runs on the real
  sounds.zrd in every headless mission test.
- Left from this table: the HUD's zoom flags (screen_flags, mac 3's HUD),
  the jump jets' flames 0x00432830, force feedback, the death/damage
  DamageCalls (death_prep, death_anim, ...), the fall cameras.
- test_sound_extra.py (in run_all): the EXTRA load 0x4e6b00 on made-up
  lists (reader nodes in emulator memory), the gated play 0x4e6cd0 (NaN
  times and delays included, 0x571cc0 / 0x40fe50 stood in), the wake
  0x45faf0 / 0x45fb50 (0x508da0 / 0x506ca0 / 0x4eb170 stood in): 1,200
  identical, seeds 1/2; BREAK=1..3 fail. test_weapons_fly agrees after
  TouchFn's change (287 sessions).
- Part-7 review notes (mac 1): quicksand's shooter word 0; decals_free's
  frames note; the touched set only ever holds ground nodes (crater nodes
  are 0x0108001c | 0x8000 | 0x20000: kept clear, never cut), so a second
  reset reads live nodes only: no fix.
- Order revised: item 2 is the mechs' deaths and damage reactions (the
  death fall Mech::death_fall 0x41b760, stop_action, blend_anim are ported
  but unwired; death_prep 0x41d070, last_hit 0x41d130 not), more visible
  than the surface marks, which become item 3 (they paint the hit
  texture's pixels at the shot's UV: the renderer has to re-upload).


## 2026-10-21: item 1's review fixes, then item 2 (deaths)

- Review (mac 1, Sonnet): UpdateCalls set_volume (0x0056f370) and
  set_position (0x00571f30, plain voices only, as there) wired: the walk,
  wade and limp loops follow the throttle and the mech. The Foot's
  destructor 0x004628c0 ported (the wake stopped and freed through
  PartCalls effect_free 0x0045fab0, the toe arrays); nothing deletes feet
  yet, as the mech's destructor is not ported (backlog: Mech's dtor and
  its parts'; the port's mechs live until the process ends).
- The touch handlers are registered: targets and animations go through
  zwep::register_collide (zwep_damage.cpp, 0x005833f0's second port, the
  same node->damage table zwep::touch reads), not set_touch_handler.
  Duplicate ports of 0x005833b0/0x005833f0 noted (zweapon.cpp and
  zwep_damage.cpp), same storage.
- Mission unload stops the loops: Core's leave (0x00481fc0) snapshots and
  stops every playing voice (zsnd::snapshot walks every set's voices).
- The parts log is capped at 4,096 lines (as weapon_fx_log); the walk
  test clears it first. Its old "> 20 steps" counted earlier tests' steps
  (mw3tests runs every test whatever the argument): the walk alone makes 13.
- Found while porting the eject: the mech data's anim_find (0x0050b450)
  was unported in the game (WiredData), so every mech effect looked up by
  name was null: footprints, the death and disable effects, the canned
  eject/jump-jet/narc effects, the splashes. Wired to zeff::anim_find; the
  walk test now sees prints and wake both. The mission's start animation
  (start_single_player_m<n>, MissionHooks anim_start / anim_run) was
  unwired too: wired, and the walk test checks c1m1's ran to its end
  (status 3). Still unported: zrdrvalve's anim_find (the AI's, on hold).
- Item 2 (deaths): ported the death's preparation 0x0041d070 (the eject),
  the canned effects 0x00432830 and their node copies' free 0x00431800,
  last_hit 0x0041d130 (zEntity's target node, 0x005a8680 / 0x005a8500 split
  out of entity_moved), the effects' end callback 0x00461c90, zEffect's
  0x00506ae0 / 0x00506b60 / 0x00506bc0. Wired in the game: death_prep,
  death_anim (death_fall), stop_action, blend_anim (the limp), on_effect_end,
  weapons stop / part lost / critical, the player's loss (Missions::lose),
  view_mode, view_camera (was misnamed mission_failed: 0x0040ae70 is the
  view's camera mode), position, the part's node update and world matrix
  (the torso jolt read the identity before), MoveCalls pilot_effect (the
  jump jets' flames), MissionHooks fn_431800.
- test_deaths.py (in run_all): 1,553 identical, seeds 1/2, BREAK=1..4 fail.
  mission_c1m1_owens_dies_headless: the woken Owens killed: dead, deadmech,
  a slow fall (7/8) that plays ~4.4 s, one eject seat copy, freed at cleanup.
- Finds: the eject sound is cut by Die in the same call (never heard;
  fixes.md candidate); the eject seat's offset is in world axes; the slow
  death's left/right alternates for all mechs together.
- Left from DamageCalls: OnKill 0x0041b420 (kill credit), fly_off (part
  vtable +0x60), node_unlink 0x004e66a0, stats, net, controls_hit/stop,
  player_died_hud, damage decals, equip voices.
- Test harness: with the start animation now running, the tests that called
  missions().cleanup after app.run hit freed zSound sets (run's
  ExitInstance shuts zSound down first; the cleanup's anim_stop_all turns a
  running animation's sounds off): heap corruption, a SIGTRAP two tests
  later (ASan: zsnd::unlock after close_device). CountingPump now cleans the
  mission up at the quit, before ExitInstance (the bramps test, which checks
  the cleanup itself, opts out). The game never cleans up after
  ExitInstance, so no port change.

## 2026-10-21: items 3-5 (mac 1: kill credit, fly_off, surface marks)

- Item 2 review (mac 1): node_matrix's cls()==5 guard dropped (a part's
  node is an object3d, as the exe assumes); world_clear (0x00491d80) only
  frees the weather, the sun and the fluctuation, never the tree, so the
  canned copies' free after it touches live nodes: no fix.
- Item 3, the kill credit: Mech::on_kill 0x0041b420 ported (mech_damage),
  weight_class 0x00484700 (mech_names, the roster's +0x2c). Wired: the
  attacker (0x0040eee0 → node_entity 0x0040e920 on the hit's shooter node:
  weapon kills had no attacker before), killer_notify (a mech killer's
  on_kill; other killers logged), salvage_mech (0x00472f00 into the
  mission's store through mac 3's add_salvage), heat_calls().game_type
  (unwired before; its other readers only test for multiplayer). Left:
  the vehicles' salvage 0x00472ec0 (no vehicles ported), the controls'
  notice (AI), the HUD's node_object could use node_entity (mac 3).
- test_deaths: on_kill added (600 runs; 0x0041d810, 0x00484700, ftol run
  real): 2,153 identical seeds 1/2, BREAK=5,6 fail. The Owens death test
  now has the player as killer: 1 mech, 400 points, salvage grows.
- Finds: the kill points' weight class ignores tonnage; only the lance's
  kills go to the salvage.
- Item 3 review (mac 1): the victim_view fallback's note; the player and
  the Owens are both in zEntity's list in c1m1 (the death test checks);
  the attacker discovery (only the player's tick clears +0x664, 0x0041aba0).
- Item 4, fly_off: MechPart::fly_away 0x00461d80 (virtual), Leg 0x00466fe0
  (calf, foot; the Blackhawk's and Owens' same-side arm), Arm 0x00467100
  (hand); FlyCalls wired to zClass in app_mech (copy, create, rename,
  parent, flag 8, remove, attach, +0x2c bits, the pose, add, clear ids,
  set node, mark, the IA fade). The flying nodes go to Mech::x6d0. Left:
  the IA fade's per-frame 0x00413cb0 (purge hook).
- test_deaths: fly added (500 cases: kinds, sides, rosters 2/11/9/7/0, ids,
  stub flag, stub models, failing copy/create, parent or not): 2,653
  identical seeds 1/2, BREAK=7 (side), 8 (world matrix) fail. The Owens
  death test blows its left arm off first: one flying node, in the world,
  the stub in its place.
- Item 4 review (mac 1): fly_away now reads the old id before set_node and
  the side off the new node's id after marking (as 0x00461ece / 0x00461f07);
  the mech lookups' node fallback noted as unneeded (every port part has
  its mech). The purge (0x00413cb0 / 0x00413c30) ported as
  Mech::fade_debris and wired: the IA fade follows the mech's opacity;
  0x00413c30 returns 0 by `xor al, al`, so the list never shrinks by
  design: at most four nodes per mech, not a per-frame leak. object3d
  0x004fbb10 ported (object3d_opacity_used).
- Item 5, surface marks: dead in the shipped game. Only WriteTextureSetMap
  (0x005426b0, interp) turns [0x0074a2ac] on, and no MW3 or PM script uses
  it (grep of interp.zbd); init 0x00526515 clears it. Ported the state:
  mark_set_type/set_map/on/set_uv/enable/is_map (gmod_hit.cpp,
  0x005426a0..0x00542710), wired to the interp commands, the weapons'
  impact type and the texture loader's projected hook; 0x00542060 logs
  unported only if a modded script turns marks on. FIX (bug): a type past 2
  is ignored in set_map.
- test_deaths: fade (200) and marks (300) added: 3,153 identical; BREAK=9
  (map), 10 (IA) fail.

## 2026-10-21: mac 1's next order (one at a time)

1. Merge the two ports of the 0x005833f0 handler table, callers on one.
2. The Mech destructor and its parts', mechs freed at mission cleanup; the
   campaign loop test checks no leak.
3. Re-run MW3_SURVEY on c1m1; the top non-AI, non-GPU item.

- Item 1: zwep_damage.{h,cpp} deleted. zweapon.cpp's tables (0x00583220..
  0x00583440) are the one port: with no run hooks they work on zClass
  nodes (+0xbc damage, parents, children; the proximity flag through
  node_set_proximity), so zEffect's post-load and the AI targets register
  through set_damage_handler / set_touch_handler. zeff's callbacks now have
  zWeapon's DamageFn / TouchFn signatures; anim_on_weapon_hit read the
  weapon's flags as raw bytes at +0x6c of the port's WeaponDef (a wrong
  read on the port's struct): now weapon->flags & kFreeze. The AI targets'
  hit_killed / hit_health / remove_handlers hooks wired (0x00583710,
  0x00583730, 0x00583400; they were logged as unported).
- Item 2: the Mech's and the parts' destructors ported (mech-build.md
  section 15): Mech::release (0x004152d0 / 0x00414f40), mech_delete,
  MechPart::release (0x0045fd40, torso 0x00464250, thigh 0x00463740, arm
  0x00466380, foot 0x004628c0 replaces ~Foot), part_delete; FreeCalls wired
  in app_mech; the cleanup's delete_object and 0x0049dc40 delete every mech.
  Two tests read the player's node after the run (now freed): captured
  during the frames. The three-mission test checks mechs_alive() == 0
  after each cleanup (BREAK: the zEntity deletes off fails it); ASan clean.
- Next (mac 1): mech.h's Pilot vs pilot.h's Pilot (ODR): rename after item 2.
- mech.h's `struct Pilot` (the mech type's data, +0x684) renamed MechData:
  pilot.h's career `class Pilot` shared the name in mech3 (an ODR clash;
  mac 1). The exe names only the file (mech_data.cpp). zai_steps' is
  mech3::ai::Pilot, untouched; mech_hud.h's forward declaration was the
  mech type's and is MechData now.
