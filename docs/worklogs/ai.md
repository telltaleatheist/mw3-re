# AI log (mw3 mac 4)

Newest last. One short entry per item: what, commits, what to remember.

## 2026-10-20 Item 1: the brain map, the HUD's planner getters (mac4/brain-map)

- Map: ai.md section 17 (frame, order stack, behaviours and their states,
  the engage gate, steering, vlist triggers, radio and player orders,
  firing, switches, a 15-item porting order). The decompiler is unreliable
  in this range (lost `this`, broken switch fall-through, dropped returns):
  read mech3.asm. Helpers: scratch/asm.py (disassembly by address),
  scratch/pe.py (vtables, floats), scratch/calltree.py.
- Ported 0x4b1bd0 / 0x4b1c70 / 0x4b1cd0 (`mech3/weapon_planner`), test
  `test_weapon_planner.py` rides on test_weapons_run.py's managers; probe
  accessor `probe_wm_planner` added to mac 2's probe_weapons_run.cpp.
  FIX (bug): 0x4b1cd0's saved-ECX answer.
- Next in the order: the order stack (item 2). Order must become
  polymorphic (net part, state), the existing test_ai_brain compares the
  current Order layout: update it together.
- Noticed: 0x4e8380 popping the last order resumes through a null block
  (not reached by any caller seen).

## 2026-10-20 Item 2: the order stack (mac4/orders)

- Ported ai.md 17.2's table but 0x4d88a0 / 0x4d88d0 (need the net
  0x603fb8). Order owns NetPart (AiNet::make_part, vt+0x5c) and
  BehaviourState (Behaviour::make_state, vt+0x8c); BrainHooks gained
  default_behaviour(). Every behaviour but 0x604068 / 0x60b460 gets the base
  state until its own is ported: replace in Behaviour::make_state or per
  class override.
- Test test_ai_orders.py: fakes numbered on both sides, plain Trace (no
  Replay), the exe's stack walked from the deque iterators. Ops the exe
  crashes on are excluded (legal(), and the 0x4e9100 full-block case).
- FIX (bug) 0x4e9100: forward search past a full last block (the push
  allocates its next block lazily).
- Remember: `cp` in zsh is interactive and hangs: `command cp -f`.

## 2026-10-20 Item 3: the gates (mac4/gates)

- Ported 0x4e8f10, 0x4e8dd0, 0x4e9640, 0x4e9650, 0x4e96b0. The brain takes
  the vehicle as a Handle; BrainHooks has now/health/position/targetable/
  manoeuvre_behaviour (program wiring comes with item 4). xe4 is a pointer
  (the home point), x230 a float. My map had 0x4e96b0's two manoeuvres
  swapped: fixed in 17.1.
- Test test_ai_gates.py over test_ai_orders' Original; an x87-returning
  fake via a `fld [val]; ret` thunk with a code hook that answers the
  value; the brain's vbase (+0xe0) table for the owner lookup.
- Quirk: the health floor bit clears the first time the pilot is found
  above it (fixes.md candidate).

## 2026-10-20 Item 4: the brain's frame (mac4/brain-frame)

- Ported 0x4e9710 (Brain::frame), 0x4e96f0 (AiControls::update with
  BrainControls), 0x428490 (MechControls::stop), 0x41ba40 (Mech::health in
  mac 2's mech_damage.cpp), the AI switch 0x4ea740 / 0x4ea750. The
  front end's Engine hook `classes_reset` was 0x4ea740: renamed ai_on.
- Program: WiredBrainHooks (app_mission) over void* helpers in app_mech
  (mech_health, mech_powered_down, mech_ai_target, mech_ammo_value,
  mech_world_position): app_mission must not include mech.h (Handle
  types differ).
- The later items' calls are BrainHooks recorded by the engine; the
  headless test counts them to show the frame runs each tick.
- Quirks: the attack ignores the picked enemy (candidate); a NaN attack
  range without a vehicle crashes the exe (FIX (bug)).
- Tests: test_ai_frame.py (on test_ai_gates.Original), test_mech_health.py
  (on test_damage's mechs, probe_damage op 6).

## 2026-10-20 Item 5: the vlist triggers (mac4/vlist)

- Ported 0x4d8e70, 0x4d9290, 0x4e9550, 0x4e94f0; the frame runs the real
  vlist (the frame test no longer stubs 0x4e9550). Hooks: heat (vt+0x9c),
  clear_line (0x4d5980), region_inside (default Region::inside).
- The player's copy 0x4d8990 (PlayerControls +0x12c, base 0x4d9840) uses
  the same vlist_run once the player's AI settings base is ported.
- Test test_ai_vlist.py (on test_ai_frame.Original): the exe's std::list in
  memory, fake valves beside the port's real Valves. Careful: the probe
  must clear the brain's borrowed radio and vlist before destroying it.
- I misread the heat jump at first: re-check `je` targets against the
  set label, not only the flags.

## 2026-10-20 Item 6: steering (mac4/steering)

- Ported 0x4ce530..0x4cec80 as Steer (set/update/measure/near/carrot/
  project/advance/shift, the contacts' push). Steering never touches the
  controls: the walking states (node state 0x4cf140, via 0x4cecf0) do. The
  Owens can't walk until items 7-8.
- Emulator needs the start-up's cosines ([0x60b494] cos 135 etc.) and
  MSVCRT _copysign (0x5bb5ac): set in test_ai_frame.Original.
- x87 compares: check each branch's NaN side (C0/C3 with test ah,1/0x41/0x40).
- Queued from Owen (via the lead): the allied attack pick (zfix bugs), the
  valve re-entry guard, c4b m6's missing boundary regions.

## 2026-10-20 Item 7: the plain states (mac4/states)

- Ported the base, null step, 0x604058 (stand), 0x604028 (hold), default and
  shared states; Brain::controls (set by the frame and AiControls) gives
  states the controls; BrainControls::fire added. The base state's vt+0x18
  answers null: every behaviour's own class answers its own.
- The exe's default run reads its order back through the NEW state's +0x04:
  the fake states in test_ai_orders now keep the order at +0x04 (serial at
  +0x0c).
- 0x4c8a30 (a base with three cleared words) waits for items 9-10.

## 2026-10-20 The c1m1/c1m2/c2m1 check, then on hold (mac4/states)

- After merging main, the lead's full run failed
  `missions_c1m1_c1m2_then_c2m1_in_one_process_headless` at
  `legs[2].anims != legs[0].anims`. It compared heap addresses: c2's
  animation table is a fresh read (zbd\c2\anim.zbd, 1422 defs against c1's
  1770) and may land in c1's freed block, which any change in allocation
  order makes likely. Nothing carried over from item 7. The check now
  compares the file read and the def count; 3 full runs 93/93 here.
- On hold (Owen pacing usage until Friday). Next when back: item 8, the node
  state 0x4ced40 (run 0x4cf140, resume 0x4c9700, with backup 0x4cfa80 and
  deploy 0x4d0040; the context 0x4cecf0; steering's set 0x4cea30), ending
  with the headless walking check (the Owens turning and walking toward its
  node under its own controls). Then Owen's three: the allied attack pick
  (zfix bugs, lancemate test), the valve re-entry guard, c4b m6's missing
  boundary regions.

## 2026-10-22 Item 8a: the node state (mac4/node)

- Ported 0x4ced40 / 0x4cf140 / 0x4c9700 (NodeState), the walk 0x4cee80,
  the turns 0x4cdd40 / 0x4cdee0 (settings 0x4c7900), the link pick
  0x4ccf20 and the weights (0x4cd430, 0x4cd470, 0x4ca210, 0x4cd580,
  0x4cdb80, 0x4c79f0 / 0x4c7960), Mech vt+0x118 / +0x120. In `zai_node`.
- Also the speed and valve steps' states (0x4c9f40, 0x4ca2e0, item 9's):
  c1m1's first link is speed -> valve -> node, so the walk needs them.
  Shunt and valve union states share 0x4ca2e0 (item 9).
- Wired the mission cleanup's 0x58b290 (`znet::clear`, already ported):
  with links followed, a second mission in one process crashed on the
  first mission's nets pointing at freed valves.
- The exe's x87 rounds float x double once: use `crt::x87_mul_f24` /
  `x87_add_f24` (and zmath's normalisers, crt's `msvc_ftol`), not casts.
- Emulator: the world_alloc region does not execute: put thunks at OWNER+.
  A stub with spec 'c' gets only [ecx] in `a`: read stack args from ESP.
- c1m1: the Owens walks 237 m to its second node, goes on at its stopping
  distance (16.8 m) to a scan step (item 9) and stops there.
  owens_runs_its_frames now holds the Owens still (brain +0xec 0) from
  frame 150 for its collision checks.
- Next: 8b backup 0x4cfad0, 8c deploy 0x4d00a0; then the scan / canleave
  states (where the Owens stops) toward "enemies walk and fire in c1m1".

## 2026-10-22 Items 8b, 8c: backup and deploy (mac4/backup)

- BackupState 0x4cfad0 (no shipped use): its aim is an unset stack vector
  (FIX: the heading -dir; the test writes it into the exe's slot at entry,
  E-0x2c); on the frame phase 0 ends it walks with its front (quirk kept).
- DeployState 0x4d00a0 (288 shipped): walk, within 1 m, settle (+0x40),
  face away from the target (1 degree params [0x60b4d8]), slide 0.5 m/s
  (Mech set_place), hold while "mcbdeploy" (0x4cfe40 at mission load via
  0x4e7b40, wired) is 1. aiv +0x80 0 while deploying.
- NodeState::go_on shares the three runs' pick/wait loop.
- Emulator: the deploy face (+0x2c) is the previous state's bytes until
  phase 4. Coverage of backup's wait needed backup dead ends.
- Next: the scan / canleave state 0x4cbc50 (run 0x4cbca0, the tests
  0x4d9c20..0x4d9ca0), where the c1m1 Owens stops.

## 2026-10-22 Item 9a: scan / canleave (mac4/scan)

- ScanState 0x4cbca0 and the tests 0x4d9c20..0x4d9ca0 (Brain::scan_test:
  aiv +0x98/+0x9c/+0xa0, +0x3c with +0xdc FLT_MAX). Real scan steps in
  test_ai_node.py's nets.
- c1m1: the Owens passes the scan step and walks 119 m to its post, then
  waits there on the valve firefly_retreat (by design). The walking check
  now covers that third leg.
- Next toward walk-and-fire: the firing (item 12, 0x4b5b80 with its plans
  and aim), the planner's volley (item 13, with mac 2), the attack (item 10).

## 2026-10-22 Item 12a: the firing state machine (mac4/firing)

- zai_firing: Firing::run 0x4b5b80 (phases 8,1/6,2,3,4,7,5), drop 0x4b6690,
  stand_down 0x4b6700, LookAround 0x4b63d0/0x4b63f0, the line cone
  0x4b5860/0x4b5940; Brain::scan_aim 0x4d8930 (IControls vt+0x80).
  FirePlan / FireSource abstract (their slots answer bytes).
- Hooks: weapon manager and weapons (wired to WeaponManager/Weapon via
  app_mech helpers, null-safe FIXes), torso_centre, fire_plan_pick,
  aim_track, aim_lead (12b).
- c1m1: the Owens rests in phase 5, looking around; the pick (12b) is
  asked each tick and answers no plan.
- Notes for 12b: scratch/firing_notes.md (my decoding).
- Next: 12b1 the plan pick 0x4b6790 and the plan class (vtable 0x5c9460),
  12b2 the aim 0x4b72a0 / lead 0x4b77e0, then 13 the volley 0x4b1860.

## 2026-10-22 Item 12b1: the volley plan pick (mac4/plan)

- volley_plan_pick 0x4b6790 with the scorer 0x4b6b30 (Scorer), the swap
  pass for the partial sort 0x4b79e0 (middle first+1), 0x4b5a80, 0x4b5a20;
  VolleyPlan (0x5c9460: take, in_reach, ready; track a hook for 12b2).
- Emulator: run the start-up 0x4b5b00 (0.25) and 0x4b5b30 (the marker)
  first. A stub select must set the vehicle's +0x6c as 0x4d5e40 does.
- Program: contact_entity / entity_active wired (mechs); the Owens' pick
  finds nothing past its range and widens it to aiv +0x30.
- Logged, unported vehicle hooks: vt+0x8c can_face and vt+0x150 in reach
  (the torso's 0x465c70 / 0x465bb0, 12b2), 0x4b6e30 track (12b2), 0x4b1860
  volley (13), 0x4d5980 clear line (world line query, not AI).
- Next: 12b2 (track 0x4b6e30, lead point 0x4d6de0, torso aims, aim
  0x4b72a0, lead 0x4b77e0), then 13.

## 2026-10-22 Item 12b2a: the plan's track and the lead point (mac4/aim)

- VolleyPlan::track 0x4b6e30 and lead_point 0x4d6de0 (zai_firing); the
  torso's face_angles 0x465c70 / update_facing 0x465ad0 (+0x98 read inline
  for 0x465bb0) and the Mech's can_face 0x41dc70, clamp_aim 0x41dca0,
  faces_within 0x41dba0, set_torso_aim 0x41dbd0, lead_velocity 0x41d400
  (mech_part_update, only new functions in mac 2's files).
- Tests: test_parts ops 24-28, test_ai_plan op 4 (track) / op 5; the two
  uncaught lead breaks are exact ties (double root, t == 0).
- FIX (bug): no selected group / no manager / no contact in the track.
- Split: 12b2b is the aim 0x4b72a0 (0x4b70b0, 0x4b71a0) and the lead
  0x4b77e0 (0x4b74f0).
- Logged, unported hooks now: aim 0x4b72a0 / lead 0x4b77e0 (12b2b), volley
  0x4b1860 (13), clear line 0x4d5980 (mac 2).

## 2026-10-22 Item 12b2b: the aim error (mac4/aim2)

- Aim (firing +0x54): frame 0x4b70b0, pick_drift 0x4b71a0, track 0x4b72a0,
  repick 0x4b74f0, lead 0x4b77e0; the start-up's [0x60b2ec] = 2/100 as a
  constant. BrainHooks aim_track / aim_lead now default to it; new hooks
  aim_intensity ([0x608e58], options().intensity[1]), targeting_aid (mech
  +0x3e4 tc / +0x3b8 fcs), wm_aim_at (+0x88, then set_aim).
- Test: test_ai_aim.py (over test_ai_plan's world; probe_ao_aim_set /
  probe_ao_aim). Not reached: the re-pick's exactly-zero offset; the
  along == range skip is not hookable by address (uncovered).
- Headless: the c1m1 Owens still finds no plan (the player is out of its
  range), so the aim does not run there yet.
- Logged, unported: volley 0x4b1860 (13), clear line 0x4d5980 (mac 2).

## 2026-10-22 Item 13: the volley (mac4/volley)

- weapon_planner: volley_pick 0x4b1860 with shot_of 0x4b1370, add_weapon
  0x4b1470, group_out 0x4b1440, the kept groups 0x4b17d0; 0x4b1170's static
  (0x60b240) is read by nothing: not kept. weapon.cpp untouched.
- Quirks kept, candidates for Owen (fixes.md): the sums double (alpha never
  picked), the group roll's threshold (group fire nearly always).
- FIX (bug): no pilot chain; a candidate weapon in no group.
- Program: WiredBrainHooks::volley_pick -> ai_volley_pick (app_mech: the
  mech's AiControls brain +0x34 / +0x90, its heat).
- Test: test_weapon_planner.py checks a volley after every operation on the
  real managers (manager state and calls compared). Breaks: 17 of 20; the
  three missed are equivalent or need data the shipped weapons lack (heat
  cap 1.4: both budgets negative; instant speed: no instant weapon has
  speed 0; roll scale 26214: only within 1 of the threshold).
- Headless c1m1: 30 m off the Owens picks a group volley and tracks, but the
  plan's ready (clear line 0x4d5980, mac 2) is 0, so it never fires; with
  the clear line answered yes (an experiment, not kept) it fires (phase 4
  157 of 300 frames).

## 2026-10-22 Item 14: the clear line (mac4/clearline)

- ai::clear_line 0x4d5980 (zai_target) with zmod::zones_overlap 0x53cc90
  (gmod_cull); BrainHooks clear_line / clear_line_at default to it, new
  hooks zone_list, entity_node, entity_centroid, node_world_cached,
  node_intersect, node_set_intersect, segment_first. Program: cls_di's
  segment_list with segment_options (first_hit 1, exclude 0x40000); cls_di
  untouched. The vlist triggers' bit 6 uses the same hook.
- Zone lists: zEntity3d's keeper 0x5a80f0 not ported, so "all" here.
- Test: test_ai_clearline.py (standalone emulator; 12 of 12 breaks).
- Headless: the c1m1 Owens fires its two lasers at the player (beams use
  charge, not reload: watch is_firing / the bin, not +0x30).

## 2026-10-22 Item 10a: the vehicle's own net (mac4/vehnet)

- VehicleNet 0x4c0090 (seven behaviours by StepKind; their states the
  base's), vehicle_net_of 0x4d70f0 (AiTarget +0xd0 now a VehicleNet*),
  the getters 0x4c0de0 / 0x4c0e10 / 0x4c0e40 (type tests +0x90/+0x94/+0x98),
  VehiclePart 0x4c0ba0 / 0x4c0d60 / 0x4c0dc0 (the leash's home point on
  the brain's +0xe4; the mode's behaviour +0x1c, reader not found).
- 0x4c0810 is the attack's own one-behaviour net: moved to 10b.
- FIX (bug): the part's run without a vehicle. Candidate: mode 3 gets none.
- Test: test_ai_vehnet.py (emulator runs the real ctors; znet registry
  via 0x4c7b80; 12 of 12 breaks).

## 2026-10-22 Item 10b: the attack and the way home (mac4/attack)

- zai_attack: attack 0x4e9ca0, AttackNet 0x4c0810 / AttackPart 0x4c0a90,
  the end / rearm / return states (0x4cac20, 0x4cace0, 0x4cadb0 with
  0x4cac80, 0x4cad30, 0x4cae10, 0x4cae90), turn_past 0x4c8d10, turn_hold
  0x4c8df0, flat_way 0x4ce250; Mech::stop_share 0x41d240 (vt+0x12c).
  BrainControls::throttle_now (vt+0x1c). BrainHooks::attack defaults to it.
- 0x4cae90 is the walk back to the old net, not the approach; the chase is
  the fight state 0x4d1d10 (10b2).
- FIX (bug): no nearest step at the resume.
- Tests: test_ai_attack.py (on test_ai_orders; the turn settings and the
  stand behaviour's vtable written in; the uninitialised waypoint zeroed),
  test_parts op 29; c1m1: engaging on, attack, fight popped, Owens moved
  250 m off, walks back to 79.8 m (its +0x28 80).
- Next: 10b2 (the fight 0x4d1ca0 / 0x4d1d10, 0x4c8a70), then 10c.

## 2026-10-22 Item 10b2: the fight (mac4/fight)

- FightState 0x4d1cc0 / run 0x4d1d10 and drive_to 0x4c8a70 (zai_attack);
  make_state for VehAttack. The vehicle net's other states (follow, modes,
  own order) go through BrainHooks::vehicle_step_state (the base state).
- Test: test_ai_attack.py pushes a real fight (0x4d70f0 on a target
  vehicle) with the unported makers stubbed to fakes; 14 of 14 breaks.
- Headless: engaging on, mode 1, player 160 m off: the Owens closes to
  79.5 m and takes the mode's behaviour; then the way home as in 10b. The
  Owens' own mode is 0; a 300 m chase was given up after ~130 m (leash or
  time, not checked).
- Next: 10c, the brain's own order 0x4e81a0.

## 2026-10-22 Item 10c: the brain's own order (mac4/own)

- Brain::run_own_order 0x4e81a0 (BrainHooks::own_order_run defaults to it).
  Its state (0x4cc150, station keeping) is a hook answering none (logged)
  until 10d: the base state would stop the pilot every frame.
- Test: test_ai_attack.py (own order after the unwind: its home point and
  the stack's come off apart, a stale pointer in the exe); 7 of 8 breaks.
- c1m1: the Owens' +0x80 1 / +0x74 40 m: its own order asks for the state
  once the player is near. The 300 m give-up is the leash (+0xcc 0xeb,
  +0x24 16900 = 130 m; the time +0xe8 10 s would follow).
- Next proposed: 10d, the station-keeping states 0x4cc290 / 0x4cc240 with
  their runs 0x4cc3e0 / 0x4cc020.

## 2026-10-22 Item 10d (1) and 10d2: the home-point FIX, the station keeping's maker and states (mac4/station)

- FIX (bug): Brain::homes (merged separately, 143b5e7).
- make_station_state 0x4cc150, StationState 0x4cc290 (resume 0x4cc380),
  StationHoldState 0x4cc240 (run 0x4cc020); the run 0x4cc3e0 is a hook
  (station_run) for 10d3. BrainControls::reversing (vt+0x18),
  BrainHooks::vehicle_xf4 (vt+0xf4, mechs 0). Order::own: the own order's
  pop 0x4e8270 empties it instead of popping the stack.
- FIX (bug): the maker's lancemate test on a missing other.
- Test: test_ai_attack.py's own order on the real maker (the emulator's
  own order given its vtable 0x5cd8e8); 10 of 11 breaks.
- c1m1: the Owens keeps station (StationState) on the player within 40 m.
- Next: 10d3, the run 0x4cc3e0.

## 2026-10-22 Item 10d3: the station keeping's run (mac4/station2)

- StationState::run 0x4cc3e0 (giving way: swerve, back off, drive off,
  alongside; let go past +0x28); Mech::throttle_speed 0x41d1d0
  (vt+0x114) and BrainHooks::move_speed; station_run hook gone.
- FIX (bug): the exe's endless phases when +0x78 < the state's +0x24
  (found by the test when +0x78 changed under a live state; shipped values
  5..50 cannot); port-only test hangs without the guard.
- Test: test_ai_attack.py runs the real run (velocities and speeds on both
  sides, +0x78 fixed per case); 18 of 18 breaks.
- c1m1: the own order is off (+0x80 0) for the bump window (it gives way
  otherwise); after the way home the player walks at the Owens: it swerves,
  backs off facing the player, lets go at 44.2 m.

## 2026-10-22 Owen's three: the split

- 1 the allied attack pick (mac4/allied, held: no shipped ally has engaging
  +0x3c on, so the frame never weighs an attack for them; Owen to choose a
  narrow fix or a look at what a lancemate does after its told target);
  2 the valve re-entry guard; 3a c4b m6's boundary pair (analysis); 3b its
  fix behind zfix bugs (a candidate instead if 3a cannot show the pair).

## 2026-10-22 Item 2: the valve re-entry guard (mac4/valveguard)

- Valve::fire 0x4d3bf0 skips a valve firing inside its own reactions
  (FIX (bug), unconditional: the update 0x4e3d90 is the only reaction that
  re-fires at once, so any re-entry is an endless loop in the exe).
  valve_reentries() counts the skips.
- test_ai_valves.py: 352 cases identical, skips counted and asserted 0;
  test_fixes: a self update and a ring of three, each reaction once per set.

## 2026-10-22 Item 3a: c4b m6's boundary pair (mac4/c4bm6, docs only)

- The pair is c4bm7 (hulls vs aiv positions: 9/9 inside; c4bm6 is two
  small disjoint areas). Also: c4b m5's valve name slip (reg_outside_c4m5),
  c4b m4's inverted entries (left). The player's vlist (0x4d8990 on
  0x4d9840) is not wired, so 3b needs it first (3b1), then the names (3b2).

## 2026-10-22 Item 1-trace: how the allies fight (mac4/allytrace)

- ai.md 17.8b: the firing (0x4b6790, scorer 0x4b79e0) picks any non-ally
  in +0x30 as the target whatever the orders; allies never close on their
  own (+0x3c off in every shipped ally record, no lance order writes it);
  hits are radio only (0x4ea520 over 0x4aec90/0x4aec10), except the
  player's 3-s run of hits on an ally (0x4d5dc0: everyone's enemy);
  "attack my target" pushes the target net's follow (0x4d1870/0x4d1980,
  pops when the target is gone), sets no target.
- Headless c1m4 (Dominic woken, an enemy 60 m ahead): targets and fires,
  no move; at 150 m aims only. Owen's rule listed as a candidate, not coded.

## 2026-10-22 Item 3b1: the player's vlist (mac4/playervlist)

- PlayerControls::settings (+0x5c, 0x4d9840), set_record (vt+0x88
  0x428440, the program's launch hook), pilot_tick (vt+0xa4 0x4d8990:
  vlist_run on the settings, nothing picked); the update runs it unless
  ControlsCalls::pilot_tick is set (probe_controls').
- FIX (bug): the controls live for the game; the next launch's placement
  update ran the last record's vlist over freed valves (a crash in the
  suite's second mission). Cleared with the valves.
- test_ai_vlist.py: each list also run through the exe's 0x4d8990 on a
  fake player controls block; won/lost test: walking out of c1m1 fails it.

## 2026-10-22 Item 3b2: c4b m5/m6 boundaries (mac4/c4bnames)

- zrdraiv's vlist entries, behind bugs: m6's reg_*_c4m7 read as
  reg_*_c4bm7, m5's valve reg_outside_c4m5 as outside_c4m5 (the names occur
  nowhere else). Headless: walking out of m5 and m6 fails each a frame later.
- A bugs-off launch after another crashes on the HUD's unset block (the
  exe's uninitialised member, hud.cpp FIX); the headless test runs bugs on,
  the exe's reading is test_ai_readers'.

## 2026-10-22 Note: bugs-off launches after another mission

- The Hud ctor (0x43b190) leaves +0x22e3c (block) unset, as the exe does
  with bugs off (hud.cpp clears it only with bugs on): a second bugs-off
  launch in one process may read heap garbage there and crash in
  Hud::setup. Headless tests that need bugs off launch one mission, or run
  with bugs on and leave the exe's path to the differential tests
  (fixes.md, the HUD's mode block).

## 2026-10-22 The lance port: the split

- L1 hits (0x4aec10/40/70/90, 0x4aed40, 0x4ea500/520, wiring 0x4d5dc0);
  L2 the radio's lines (0x4e6d50/0x4e6c60, target lost 0x4ea630, death
  0x4ea6b0, low/out of ammo); L3 the follow state 0x4d1870/0x4d1980; L4 the
  lance orders (0x469940, 0x4ea000..0x4ea230, 0x4e9fd0, 0x4ea260/390/3f0/
  450/4b0); L5 headless c1m4 (order Dominic to attack; friendly fire 3 s).

## 2026-10-22 Item L1: the hits (mac4/hits)

- ai::HitRecord (IControls +0x04: last, run start, attacker), note
  0x4aec10, within 0x4aec40, run 0x4aec70; controls_hit 0x4aec90;
  MechControls::on_hit / hit_an_ally / hit_lately (vt+0x94/+0x98/+0xa0),
  AiControls' 0x4ea520 / 0x4ea500 via Brain::on_hit / hit_an_ally.
  take_damage calls on_hit when the controls_hit hook is unset (probe_damage
  sets it). BrainHooks::hit_lately gone (the record answers).
- The trace's 17.8b corrected: call_help is for the victim far (22.4 m)
  from the player; the turn covers the allies after the player in the list.
- test_ai_hits.py: 10,000 hits identical, 8 of 8 breaks; test_ai_vlist's
  player case now runs the exe's 0x4aed40 on the same record.

## 2026-10-22 Item L2: the radio's lines (mac4/radio)

- sound_extra_say 0x4e6c60 / sound_say 0x4e6d50 (zSound's 2D play, the
  EXTRA record's delay and subtitle); BrainHooks::radio_line plays the
  radio's slot. The kill line 0x4ea630 (vt+0xb4, from the kill credit, not
  "target lost" as the trace first said: corrected) and the death's
  0x4d8a00 / 0x4ea6b0 / 0x4aed50 (vt+0xa8: lance_lost hook, the tick, ejecting).
  Mech::die and on_kill call the controls when their hooks are unset.
- test_sound_extra.py: say 600 cases (break 4 caught); test_ai_hits: the
  kill and death lines each case; 3 of 3 breaks.

## 2026-10-22 Item L3: the follow state (mac4/follow)

- FollowState 0x4d1870 / run 0x4d1980 (zai_attack), VehFollow's state in
  make_state (was the vehicle_step_state stand-in). walk_drive takes
  0x4cee80's two slack arguments (default the nodes' 0.04).
- test_ai_attack.py: the follow in half the pushes; the exe's 0x4c89f0
  real (no longer stubbed); the exe brain's steering given its ctor values
  and the start-up cosines; _copysign; the vehicle's vt+0x118 0x41d1c0 at
  12 m/s both sides; the controls' vt+0x30; the boundary stub halves EDX's
  point for the follow's and the walk's calls; random carrot offsets
  between runs; the whole steering compared. 9 of 10 breaks (one same).

## 2026-10-22 Item L4a: the brain's lance orders (mac4/lance)

- Brain::attack_target (0x4ea450/4a0), form_on_me (0x4ea4b0, FormationNet
  0x603fb8 = 0x4c0570, part 0x4c07d0), enemy_only / ally_only (0x4ea390 /
  0x4ea3f0, FIX for a contact that sees nothing), lance_order (0x4ea000..
  0x4ea230 + the answer 0x4e9fd0); the refit 0x4ea260 a hook (repair).
- test_ai_lance.py: 3,608 orders identical, 6 branches, 9 of 9 breaks. The
  probe's g_real_formation switches it to the real formation net and the
  vehicle nets' own follow; its contact_entity answers the three contacts.
- L4 split: L4b the dispatchers 0x469400 / 0x469650 and the lance list;
  L5 headless; L4c the command mode (order 2) last.

## 2026-10-22 Item L4b: the dispatcher (mac4/lance2)

- mech3/lance_orders: lance_dispatch 0x469940 with 0x469400 / 0x469650's
  own paths and the made contact 0x4e6ee0 (LanceCalls for the tests);
  mech_command's lance keys use it when unset. Command mode (order 2 and
  both paths' 0x468f00 / 0x468fa0) and 0x468ca0's [0x605f58] are L4c.
- player_takes_triggers (0x49e540 / 0x49e4e0) replaces L2's lance_lost:
  a dead pilot's region triggers go to the player's settings.
- test_lance_dispatch.py (probe_lance.cpp): 3,000 cases identical, 7 of 7
  breaks (one at the full count); test_lance.cpp: the triggers.

## 2026-10-22 Item L5: the lance end to end (mac4/lancecheck)

- test_program mission_c1m4_dominic_takes_the_order_and_the_lance_turns:
  the player selects Dominic, targets the enemy (400 m on a clear line),
  player_command(0x54): the follow on the enemy's net, closes to ~60 m
  firing; 6 s of the player's hits turn him (enemies + team, allies - team).
- Found: the ground blocked the survey's 150 m line (not the range: he
  fires from 600 m on a clear line); corrected in 17.8b and discoveries.
- Found: headless walks are not bit-repeatable (Dominic's z differs by
  1e-4 m from ~2.5 s in; the player's handle varies by one): reported to
  the lead, the test asserts only what does not depend on it.

## 2026-10-22 Item L4c1: the pocket commander's controls (mac4/command)

- mech3/mech_pocket: PocketControls (0x4690d0, dtor 0x469310, set_owner
  0x46a440, update 0x46a360, keys 0x46a860, unwrap 0x469cf0, release
  0x46ad80 with a FIX for the exe's double free, death 0x46abb0),
  pocket_enter; the dispatcher's order 2 and its in-mode paths (L4c3's
  orders through pocket_order, L4c3). MechControls: set_owner,
  unwrap, pocket, release, command as virtuals; the mech's update unwraps.
- The program: the camera (program_view_camera_id / set), the viewed HUD's
  reticle flag and image, the weapons' and equipment's hold; the view's
  pocket 0x4b5290 and the panel (L4c2) not ported.
- test_pocket.py: 400 cases identical, 8 of 8 breaks.

## 2026-10-22 Item L4c3: the orders to the lancemates picked (mac4/command2)

- mech3/lance_orders: pocket_order (0x468f00 / 0x468fa0 with the records
  0x468bd0 / 0x468df0 and the unit steps 0x468c00 / 0x468e20),
  retarget_team (0x468ca0: orders 10 / 11 on the command target marker
  [0x605f58], an AiTarget on cmd_target.flt the panel moves to the ground
  under the cursor, make it the lancemate's enemy or ally); in_pocket uses them. The units come from
  PocketCalls::panel_units (the panel, L4c2; unset: none).
- test_pocket_orders.py: 3,000 cases identical, 7 of 7 breaks.

## 2026-10-22 Item L4c2a: the pocket commander's panel (mac4/panel)

- mech3/pocket_panel: PocketPanel (0x468250, 0x468720, pick 0x469060,
  0x4687e0), its lines (PocketItem 0x4674f0, PocketMateItem 0x467620,
  PocketOrderItem 0x467960 / 0x4679a0 with the five lit tests), the
  colours, the orders' table 0x467d80, the mission start / end (0x46aaa0,
  0x46ab10, sounds 0x46a930). The mode makes, picks and frees the panel by
  default; pocket_order reads its units (pocket_panel_units).
- test_pocket_panel.py: 200 runs identical (calls and dump), 200 tables;
  16 of 16 breaks (one found a test gap: replayed names hid an extra line,
  so the names now go in the plan).
- Left for L4c2b: the frame 0x469d00, the program's keys, names, cursor,
  marker and mission start wiring; L4c2c: 0x468900, 0x467760, 0x46b0a0,
  0x46af30.

## 2026-10-22 Item L4c2b: the pocket commander's frame (mac4/frame)

- mech3/pocket_panel: pocket_frame (0x469d00) with PocketPanel::route
  (0x468820), FrameCalls for its world; the mode's update runs it by
  default. FIX (bug): the exe reads the HUD's +0x70 through null when the
  player is not the view's mech (crashes the emulator; the differential
  keeps him viewed, test_fixes covers it).
- Found in my L4c1 port: 0x469cf0 is the dispatcher on +0x160 (mov edx,
  [ecx+0x160]; jmp 0x469930: push 1; call 0x469940), not only order 2: unwrap now dispatches the
  order a line put there.
- Program: the mission start / end in the load's listeners, the orders'
  keys from zInput. The HUD / map picks, names, places, marker making and
  net elements wait for L4c2c.
- test_pocket_frame.py: 200 runs identical, 11 branches; 16 of 16 breaks
  (three needed the full count or a wider world: the map's edges, a net
  element on the marker).

## 2026-10-22 Item L4c2c: the pocket commander's own keys (mac4/keys)

- PocketControls::keys (0x46b0a0) and mouse_look (0x46af30): 0x427d60's
  driving without the axis table, with the mode's mouse look, twist keys
  at 1, pitch / twist eased at 10/s; FIX (B) for the throttle step as the
  player's. The update runs it after the frame. Program: the viewed HUD
  for the zoom box (PocketCalls::view_screen; the zoom hooks were there).
- The lead's note checked: 0x469cf0 jumps to 0x469930 (push 1; call
  0x469940); that argument is read only in case 2, as the mode for
  0x4690d0; both callers pass 1, as lance_dispatch's pocket_enter(c, 1).
- original.py: a stub address deleted from `stubs` is skipped (the keys
  test runs what test_pocket.py stubs).
- test_pocket_keys.py: 2,000 cases identical, 9 branches; 12 of 13
  breaks (the 13th equivalent).

## 2026-10-22 Repeatability (mac4/repeat)

- Cause: rand seeded from the clock at the mission's start (0x501ab0,
  0x508fc0; the exe also 0x49aca0): the volley pick differs, so Dominic's
  heat and speed cap. Traced in fresh processes with an in-update dump.
- crt::rand_fixed_seed() for the tests (reset_process_state sets 1); the
  game keeps time(). The ordered c1m4 test read the player's AiTarget
  after the run (a use-after-free, found under MallocScribble): its team
  is copied now. Both c1m4 tests assert their exact outcomes.
- Second cause, a port-only gap (the exe frees them): the cleanup's
  0x4c5fa0 (the regions freed) was not wired, so in the port every
  mission after the first in a session found the earlier mission's
  regions by name (region_find); in c1m4 after c1m1 the player's
  `mission_start` trigger tested c1m1's region and never fired. Found by
  bisecting the full mw3tests (the campaign test before c1m4). Wired with
  the cleanup's other ported calls (mission-frame.md section 5);
  mission_after_another_gets_its_own_regions_headless (fails unwired).
- fix_pocket_frame_without_a_hud stubs the commands: since L4c2c the
  mode's keys read zin::command, which crashed with no zInput map when
  the test ran alone.
- Left: GameEngine::handle_of gives a freed address's handle to a new
  object, so handles can still differ between processes when tests share
  one; nothing depends on their values here.

## 2026-10-22 Item L4c2d: the pocket commander's draws (mac4/draws)

- PocketMateItem::draw (0x467760: the lines from the names to the units),
  PocketPanel::update (0x468900: the clip, the lines, the cursor's
  triangle), pocket_draw (0x46a410 / 0x46a3f0) in the program's HUD
  step; DrawCalls wired to the HUD's projection, window rect and view.
- The line's ends were not readable from the stack offsets: the emulator
  with marked inputs settled them (the text's edge and middle to the
  unit's point).
- test_pocket_frame.py draws among the frames: 200 runs identical, 18
  branches; 13 of 15 breaks (two the same behaviour).

## 2026-10-22 The pocket commander in the program (mac4/pocketwire)

- app_pocket.cpp: the marker made at the mission's start (0x468100 on the
  ported zClass pieces, a PocketMarker in zEntity's list), its show and
  move; the frame's picks on the HUD's reticle; the names; the node mark.
  The brain hooks take the marker as a target (WiredBrainHooks).
- Unwired, not ported: the mission map and its picks, the network
  elements, the repair bays, the view's pocket layer.
- mission_c1m4_pocket_commander_sends_dominic_to_a_spot_headless: exact.
  The c1m1 sensors test counts the marker (3 entities).

## 2026-10-22 Targeting the targets.zrd objects (mac4/targeting)

- The entity sort (0x4d5ca0, 0x4d7360, 0x4d7a40, 0x4d7bc0) was already
  ported (sensors.cpp); my map counted it as left by its tags.
- The brain hooks: contact_entity (0x4e6fe0) for every entity,
  contact_vehicle (0x4e6ff0) mechs only; a target's AiTarget, place, lock
  and centroid nodes, active, health fraction; the aim-at-last-hit on
  mechs only. zEntity3d's node bind for targets (0x40eaa0 / 0x40ed00 over
  0x40ea30 / 0x40ec20).
- mission_c1m4_dominic_fires_on_a_target_headless: exact.

## 2026-10-22 The mission boundary, B1: the reader (mac4/boundary)

- Split sent to the lead: B1 the reader (this), B2 the query 0x4c3000
  with its crossings and 0x4c3bd0, B3 the program's inout_clamp and the
  headless edge stop.
- zainet_inout: the variant, the match, the walk through the matrix
  stack, the models (corners on the ground), the polygons and their
  sides, the unlink and delete, the selection, the free. Wired on the
  mission's listeners (app_mission.cpp) with the launch's game.
- test_inout.py runs the original's listener load on each world read by
  its own 0x4ff520 (test_di's World): 84 loads identical at 24 bits.
  Needed stand-ins: std::numeric_limits<float>::max (0x5bb4fc) and
  _CIsqrt (0x5bb5b8) as x87 code; the start-up's 0x4c19f0. Campaign
  cases keep to each world's operation (c3 under op 1 writes m5/m6 past
  the vector: the FIX).
- Three FIX (bug): out-of-range children and mission numbers, the
  current area cleared at the end (the exe leaves it dangling).
- The shipped `inout` nodes are all at the identity (data flag 8), so the
  test also moves `inout` and its children (some off the tree-valid path)
  to run the vertices' transform and the walk's matrices.
- Breaks: 14 of 19 caught; the other five are the same results (`<=` for
  `<` in the box, a y the ground replaces, a sum reordered past a zero,
  push for push_copy of an equal matrix) or unseen (the stale flag on a
  subtree that is then deleted).

## 2026-10-22 The mission boundary, B2: the query (mac4/boundary-query)

- zainet_inout: the query 0x4c2b00 with each polygon's pass, the crossing
  count, the nearest outline point, the way round, the clear line; the
  pull 0x4c3000 (its vehicle record, read by nothing, left out); the
  messages 0x4c3bd0.
- BrainHooks::inout_clamp now takes the exe's `in` and `out`: the attack's
  return and the drive passed a zeroed point before (unseen while the hook
  answered false). The probe's and test_ai_attack's stand-ins follow.
- test_inout_query.py: 10,000 segments on every area identical at 24 bits,
  every kind of answer seen, 655 pulls (one buffer and two), 294 messages.
  The areas' last 0x10 bytes are the stack's leftovers in the exe; the
  test zeroes them after the load.
- The red shapes are no-go patches, not a fence (B1's discovery entry was
  wrong, corrected); blue is the fence where there is one. Bridges and
  gates have red and yellow sets under one name for the messages to swap.

## 2026-10-22 The mission boundary, B3: in the program (mac4/boundary-wire)

- WiredBrainHooks::inout_clamp: ai::inout_pull from position() (0x5a85e0).
- New headless check: mission_c1m4_dominic_stops_at_a_no_go_patch_headless
  (exact; the 17 m overshoot and the walk back out are the exe's pieces).
- Five headless runs changed with the boundary acting: the order and the
  unordered shot (pulled round a patch corner: new exact values), the target
  (Dominic's spot is inside a patch: he walks out, the hut is at 9 of 30
  after 30 s), the Owens' way home (pulled from the east: it now starts on
  the first side whose way back is clear, south, and the give-way puts the
  player ahead of it by its forward).
- An order dependence turned up (the target run differed after another
  mission): the animations' random cursor 0x60c5f4 runs on across missions
  (the exe's start 0x508fc0 does not reset it; object motion's random
  direction moved ghsD3's hidden debris, which the feet's ground probe hits).
  The exe's own behaviour; reset_process_state now resets it and the
  stagger as a new process would.

## 2026-10-22 Item 3a: the wait states (mac4/behaviours)

- The lead's batches: 1+2, 3+4+5, 6+7+8, then one class per item; 15 (the
  refit order's state) goes with refit/repair.
- WaitState (0x4c9fc0 / 0x4ca0e0, runs 0x4ca070 / 0x4ca190) in zai_brain,
  declared in zai_node.h for the probe.
- test_ai_node.py: waits in the chains, their start in out[45]. The
  compares' edges (equal times, NaN) needed values and a clock on the
  clock's own 0.5 s steps; 9 of 9 breaks then.

## 2026-10-22 Item 3b: mcbwait, sound, gate (mac4/behaviours)

- McbWaitState, SoundState, GateState (zai_node.h, zai_brain.cpp);
  BrainHooks::sound_at (0x56e7e0), the program's zsnd::play_at.
- test_ai_node.py: the gate's state real (the jump's still a fake), sound
  steps (kind 10) and mcbwait nodes (11); 83 branches, 11 of 11 breaks.
- FIX (bug) extended: an mcbwait record without a valve name.

## 2026-10-22 Item 3c: shunt, valve union, valve_assign (mac4/behaviours)

- Shunt and valve union make the valve step's PassState; AssignState
  (0x4cdcf0) sets its valve first. FIX (bug) extended to the assign.
- test_ai_node.py: kinds 12-14 in the chains; 88 branches, 6 of 6 breaks.
- The property steps are all done; the jump (item 4 on my list) is next.

## 2026-10-22 Item 3d: the jump (mac4/behaviours)

- JumpState and jump_plan (0x4cb330, 0x4cb120 / 0x4cb250, run 0x4cb4b0).
  New hooks: forward_speed, set_local_velocity, airborne, gravity;
  BrainControls set_jump (vt+0x34), set_jet_forward (vt+0x3c).
- test_ai_node.py: the jump's state real; _hypot as x87 code (sqrt of the
  squares at 24 bits, the port's too); world words 25-28 (velocity x/z,
  airborne, gravity), out to 64 words; jumps land on nodes; NaNs of either
  sign match (0/0 from a zero share); 800 cases for every phase; 115
  branches; 16 of 17 breaks (the miss: a 1% band at the speed cap).
- FIX (bug): a jump whose next step is no node does not turn.

## 2026-10-22 Item 3e: the brain's step is dead (not ported)

- 0x4cd7b0 / 0x4cd810: the brain's +0x240 step is made (0x4cd770, only from
  0x4cf44a) and its maker 0x4cd790 sits only in its vtable 0x5cc290, which
  only the constructor names; no order is ever given it. Its run is
  0x4cdd40 inline (turn on the spot, then on). Documented in ai.md, skipped.

## 2026-10-22 Item 3f: mode 1, circling (mac4/behaviours)

- CircleState (0x4c9750, run 0x4c9880) in zai_attack, made for VehMode1.
- test_ai_attack.py: mode 1 real (vt+0x128 / +0x120 real on the vehicle,
  the brain's +0x44 / +0x48 / +0x94 and +0x534 in the world, negative
  throttles, the target moving alone between runs, rows of 28 words);
  the boundary's trace carries its point (EDX) in both tests now; 59
  branches, 17 of 17 breaks.

## 2026-10-22 Item 3g: mode 2, orbiting (mac4/behaviours)

- OrbitState (0x4d1320, run 0x4d13c0, resume 0x4d13b0) in zai_attack.
- test_ai_attack.py: mode 2 real; a fake order pushed and popped over the
  top (the resume); mode 2 drawn more often, the stopping rate from 0.2;
  68 branches, 11 of 11 breaks.

## 2026-10-22 Item 3h: mode 4 (mac4/behaviours)

- VeerState (0x4d1650, run 0x4d16f0, resume 0x4d16e0) in zai_attack:
  mode 2 without the boundary. BrainHooks::vehicle_step_state removed.
- test_ai_attack.py: mode 4 real, drawn more often, 300 cases; 72 branches.
  10 of 12 breaks; missed: the inactive target (dropped earlier by the
  brain's frame) and the facing threshold raised to 0.6.
- Follow-up (lead): test_ai_attack.py runs mode 2's or 4's state once with
  its target inactive after some steps (a separate generator, so the main
  draws stay); both inactive-gate breaks caught. Mode 1 left out: its
  flips' coverage depends on the main course.

## 2026-10-22 Item 3i: the combat manoeuvre (mac4/behaviours)

- ManoeuvreState (0x4caae0 / 0x4cabe0 over 0x4ca440, run 0x4ca600, suspend
  0x4ca5f0, dtor 0x4ca570) and AfterManoeuvreState (0x4ca3b0, 0x60b450's) in
  zai_attack; hooks jump_fuel (vt+0x148), lead_point (0x41d940 when the
  vehicle's target sees it), BrainControls::attacker (the hit record's by);
  wired in app_mission / app_mech.
- New test_ai_manoeuvre.py (over test_ai_node's harness): 300 cases, 43
  branches, 29 of 30 breaks (phase 1's turn store is dead).
- test_program c1m4 Dominic: a manoeuvre at the start (hit lately in the
  first second, the record at time 0): the hut falls within 40 s now; the
  test runs 1200 frames with the new counts.
- test_ai_lance.py's stack rows sized by test_ai_attack's ROW (broken since
  3f's 28-word rows).

## 2026-10-22 Item 4 (vehicles): the split (sent to the lead, not started)

Sizes from Ghidra's pseudocode: S under 150 lines, M 150-350, L over 350.
The exe has two vehicle files, veh_basic.cpp and turret.cpp. 0x49f8a0 makes
a "mobilehq*" name a field base (0x4589d0) and anything else a basic
vehicle (0x4ac320). The basic vehicle's vtable 0x5c88e8 has 89 slots, about
1,400 unique lines, many of them shared with mechs (zEntity 0x5a8xxx,
AiTarget 0x4d6de0 / 0x4d5e40, the vehicle net 0x4d70f0) or trivial.

Aircraft are not a class of their own. The helicopters are world nodes
("heli", "m5heli") moved by the mission animations (anim.zbd's heli,
heli_fall, heli_bounce, helipuffer, the copter*.zan paths), which already run
on the ported anim runtime. Small check: whether targets.zrd makes them
shootable. Boats ("fish_boat", "boat0") look the same (models and anims).
One more subclass found: 0x4a4de0 (vtable 0x5c8460, "stealth"), a basic
vehicle made at mission load from 0x490200. Its use is unknown; map it
before step 1 ends.

Step 1, the basic vehicle as an object:
- 1a (S-M): the vehicle struct (0xfc bytes) and its kind in the program's
  entity registry (entity_is_mech's answer: not a mech), the constructors
  0x4ac320 / 0x4ac530 / 0x4ac3d0 (about 160 lines), the dtor 0x4ac630.
- 1b (M): the sub-objects 0x4adaf0 / 0x4aec00, the definition's lists
  (weapons 0x4adb20 -> 0x4ad930, 90 lines, over the ported 0x4b4660; the
  animations 0x4ade40), the setup vt+0x158 0x49ec00 with 0x49f660 (about
  190 lines).
- 1c (M): the placement vt+0xdc 0x4a3630 (65 lines) and the small accessors
  0x4acd90..0x4ad290 (about 16 functions, 330 lines).
- 1d (S-M): the damage vt+0x160 0x4ad3f0 (85), the death vt+0x70 0x4ad2d0
  (28) and the kill credit's salvage 0x472ec0 (19).
- 1e (M): what is left of the AI slots, 0x4a2ef0 (144) and 0x4d6de0 (142):
  check first what is ported already.
Test: c1m1 headless, the trucks and APCs standing where placed, hit and
killed; plus differentials of the accessors.

Step 2, the frame with its controls:
- 2a (M): the basic vehicle controls 0x4ac750 / 0x4ac860 / 0x4ae270, vtable
  0x5c8c50 (11 slots, about 250 lines: 0x4ae3c0 111, 0x4ae6b0 50,
  0x4a2a00 36), joined to AiVehicleControls' data side.
- 2b (S): the AI vehicle controls' vtable 0x5cd830 (0x4e72f0 55; the rest
  are the ported gates or nothing).
- 2c (S-M): the frame vt+0x34 0x4a1040 (44) and the ground and move vt+0x38
  0x5a80f0 (54). Test: c1m1's APCs driving their regions.

Step 3, the field base, then the turrets:
- 3a (M): the field base object: ctor 0x458a70, dtor 0x458b30, setup
  0x458d40 (83), damage 0x458f50 (48), frame 0x459640 (70); the rest of its
  vtable 0x5c23d8 is the basic vehicle's.
- 3b (M-L): a base lost, 0x4590b0 (221, already in vehicles.md).
- 3c (unknown size): its repair, which belongs with item 5 (refit and
  repair). Map it first.
- 3d: the turrets. Map them (S): turret.cpp's 9 functions, 903 lines
  (0x4a6270..0x4a76b0), aiTurrets.zrd, the turrets' pick 0x4d72b0. Then two
  or three items of about 300 lines each.

Resume here (paused 2026-10-22 for the usage limit): item 3 is merged at
b0f6296; nothing is in progress. Wait for the lead's answer on this split,
then start 1a. Rule from the lead: the related test set includes every test
that imports a changed test's harness.

## 2026-10-23 Item 4, step 1a: the basic vehicle as an object (lead, main window)

From here the lead does the items in its own window (Owen, 2026-10-23); mac 4
is paused.

- `mech3/veh_basic_object`: `BasicVehicle` (0xfc) over `Vehicle`, the base
  split out of `Mech` (+0x04..+0xd8, `vehicle_base` 0x4aeb00 now takes it;
  `vehicle_set_node` is 0x4d6c90's stand-in for both). The entry 0x4ac320,
  the ctor 0x4ac530, the dtor 0x4ac650 / 0x4ac630 with the bases' 0x4aeb50,
  0x4d58a0, 0x5a7f90, 0x5a8370, the markers 0x4ac3d0.
- Corrections to the split: the ctor's arguments are (controls, node), not
  what Ghidra shows; the controls it makes are the vehicle's own basic ones
  (0x4ac860(0xa70), 0x4ae270), not the AI's (those come given); +0xe8 is the engine sound (the setup's
  sounds/engine, from a shared cache at 0x60b110) and +0xf0 the motion
  animation (its dtor stops it), not a hit record, though 0x4aec00 zeroes
  both (one shared body).
- test_veh_basic.py: 200 runs (2 s) over the name or type found, the copy
  failing, given or made controls (the block failing), and the dtor with
  each optional field set (weapons, network record, motion, engine); traces, the 0xfc bytes and the markers compared.
  8 breaks caught (16 s for all).
- Left from 1a: 0x4ace00 is a hook (the camera modes for viewing a
  vehicle); the program does not make vehicles yet (needs 1b, 1c). The
  stealth subclass 0x4a4de0 is still to map before step 1 ends.
- Next: 1b, the sub-objects' contents and the setup vt+0x158 0x49ec00.

## 2026-10-23 Item 4, step 1b: the setup, engine sounds, motion animation (lead)

- The setup 0x49ec00 (vt+0x158): the definition's `sounds` list's `engine`
  and its `motion_anim` (not weapons, as the split first read), then the
  controls' vt+0x0c. The engine sounds' list at 0x60b110 (64 entries, never
  grown; the full list's report is empty in this build), its lookup 0x4ad930,
  its clear 0x4add10 (wired to the mission cleanup's 0x4d71f0 in the
  program), the motion animation 0x4ade40.
- 0x49f660 moved to step 2: it offsets the controls' points by the
  vehicle's position, the controls' business.
- test_veh_basic.py gained the lookups (up to 200, 10 of 100 runs fill the
  list) with the clear, and the setup over random definitions; 11 breaks
  caught (the list's two at the full count).
- Next: 1c, the placement vt+0xdc 0x4a3630 and the accessors.

## 2026-10-23 Item 4: the order after 1b (lead)

- c1m1's four vehicles have nets, so they get the AI's vehicle controls
  (0x4e71c0, given); the vehicle's own 0xa70 controller (0x4ae270, its init
  0x4ae3c0 reading a record through 0x4022c0, ranges defaulted 102400, 1500,
  10, 4624, 4900, 3, 62500, ...) is for a vehicle whose kind has no net
  (create_vehicle 0x49fce0). Both sit on the base 0x4ac750: vtable +0x00,
  vbptr +0x04, the IControls virtual base after +0xa04 (+0xa08 when most
  derived), the turret limits in the +0x764 block (0x4ac740 -> 0x4a4af0);
  the base's ctor sets nothing else, its users fill it.
- The placement 0x4a3630 (1c) writes that base (+0x14, +0x20, +0x24,
  +0x70, +0x160 points with the +0x214 count, +0x330, the +0x764 block's
  +0x84 / +0x14c), so the order is now: 2a1 the base's layout from its
  users and `ai::AiVehicleControls` over it; 1c the placement and the
  accessors; 2b the AI vehicle controls' vtable 0x5cd830; 2c the frame;
  then 1d, 1e; the 0xa70 controller (0x4ae270, 0x4ae3c0, 0x4ae6b0,
  0x4a2a00, ...) after, as 2a2.

## 2026-10-23 Item 4, step 2a1: the vehicles' controls base (lead)

- `mech3/veh_controls`: `VehicleControls`, the base 0x4ac750 of the AI's
  vehicle controls and the basic ones; `ai::AiVehicleControls` derives from
  it (its turret limits and owner moved there; `TurretLimits` too). The init
  0x4ac8a0, the setup 0x4acbc0, 0x4accd0, 0x5869d0, the rotation 0x4ab990.
  The base's primary vtable 0x5c8b58 against the AI's 0x5cd880: the AI keeps
  the setup and init and overrides vt+0x14, +0x1c, +0x24, +0x28.
- test_veh_controls.py: 400 runs (2 s), the original's base made by its own
  ctor; the definition's reader nodes in the emulator's memory, mirrored by
  the probe; the rotation compared within 4 ulps or 1e-6. 10 breaks caught,
  among them the "fix" of the firepoints' fields (the test pins the
  original's choice).
- Open (vehicles.md): which firepoint fields the firing reads.
- Next: 2a1b, the vehicle's 0x4af2a0 (131 lines, the controls' +0x3bc
  table), which may settle the firepoints; then 1c.

## 2026-10-23 Item 4, step 2a1b: the vehicle's weapon slots (lead)

- 0x4af2a0 (the vehicle's method, on its controls): `WeaponSlot` x10 at
  +0x3b8, `vehicle_weapon_slots`, called from the controls' setup.
- Found: +0x608 and +0x5f0 are the muzzle and direction a beam reads live
  (0x584a70 keeps the pointers), so the firepoint question narrows to the
  aiming (2c). A beam's segments are 8 only on the controls whose serial
  is 1 (no shipped vehicle has a beam). FIX (bug) for a weapon zWeapon lacks.
- test_veh_controls.py gained 400 slot fills (presets at random, 0..10
  items, the gun, the firepoints, serial 1 or not): 4 s for the whole test;
  8 breaks caught.
- Next: 1c, the placement 0x4a3630, and the accessors.

## 2026-10-23 Item 4, step 1c1: the placement (lead)

- `vehicle_place` 0x4a3630 (mech3/veh_basic_object); `VehicleControls`
  gained throttle +0x08 / turn +0x0c (IControls SetThrottle / SetTurn
  through the thunks 0x4e76c0 / 0x4e76e0), `clear_commands` 0x4ad640, +0x20,
  +0x24, +0x70, the points +0x160 (count +0x214), the words +0x8b0 (count
  +0x7e8, 32 kept here); the vehicle's `controls` is a `VehicleControls*`.
- The AI vehicle controls' IControls vtable is 0x5cd778 (most slots thunks
  into the base by -0x2bc - vtordisp).
- test_veh_controls.py gained 400 placements on a stand-in vehicle over
  the original's base (its own ClearCommands runs): 6 s for the whole test;
  9 breaks caught.
- Next: 1c2, the accessors 0x4acd90..0x4ad290.

## 2026-10-23 Item 4, step 1c2a: the motion block (lead)

- `VehicleMotion` (the controls' +0x764..+0xa04) and its reader 0x49ef40
  with the model's support and collide points (0x49eed0, 0x49ecd0), the
  gun's limits rebuilt (0x4a4b30 -> 0x4a4a70; the other three limiter
  classes wait for the turrets). The 1c1 names +0x7e8 / +0x8b0 were the
  support count and words (16 fit, not 32).
- Two FIX (bug)s: the chas_roll slip (zfix bugs) and the missing lists.
  Neither game's data has any chas_* or *_wave key.
- Open: with the model's collide points the collision count is not
  written (stays 0): check the vehicle models.
- test_veh_controls.py gained 400 motion blocks (10 s for the whole test):
  10 breaks caught; test_fixes has the fix's own check.
- Next: 1c2b, the accessors vt+0x108..+0x13c (they read this block), and
  0x4ace00 (the vehicle cameras).

## 2026-10-23 Item 4, step 1c2b: the speeds and turns (lead)

- vt+0x114..+0x13c (11 functions) in veh_basic_object; vt+0x130's second
  argument and vt+0x120's argument are real (Ghidra showed neither), the
  clamps' constants doubles (1.0, 0.0), NaN handling from the asm's flags.
- test_veh_controls.py gained the 11 on a vehicle with the real vtable over
  the original's base (each through a fresh thunk: Unicorn keeps translated
  code), exact: 9 breaks caught (one equivalent break replaced).
- Next: the vehicle cameras (0x4ace00, vt+0x108..+0x110), then 1d (damage,
  death, salvage), 1e, and putting vehicles in the program.

## 2026-10-23 Item 4, the vehicle cameras (lead)

- 0x4ace00 and vt+0x108..+0x110 in veh_basic_object: one static list
  (0x60b130), three plain modes (0, 1, 3) made once; its current id is never
  set, so a vehicle's view is mode 0. The view attach goes through
  VehBasicHooks::view_attach (PlayerView takes a Mech).
- test_veh_basic.py gained the cameras at 24- and 64-bit x87: the modes'
  bytes, the lookups -1..5, the current, the view call. 9 breaks, 8 caught
  (a one-ulp height change is inside the tolerance).
- Next: 1d (damage vt+0x160 0x4ad3f0, death vt+0x70 0x4ad2d0, salvage
  0x472ec0), 1e, and putting vehicles in the program.

## 2026-10-23 Item 4, step 1d: damage and death (lead)

- vt+0x160 0x4ad3f0, vt+0x70 0x4ad2d0, the jolt 0x4accf0, the weapons'
  free 0x4b48c0, zWeapon's hit callback 0x4af550 (difficulty x2 / x0.5)
  and TARGET's tint vt+0x68 0x4ad390 in veh_basic_object; the controls'
  health names fixed (+0x744 is the health); frozen, state, beam_on added.
  Ghidra had the jolt reading its direction before the matrix stack turns
  it (0x522640 works in place).
- test_veh_damage.py: 600 runs, real vtable and matrix stack, 16 breaks
  caught. Salvage 0x472ec0 was already ported. A review subagent checked
  the asm (risky: NaN branches, register arguments).
- Next: 1e (the AI slots 0x4a2ef0, 0x4d6de0), then putting vehicles in
  the program.

## 2026-10-23 Item 4, step 1e: the debug readout (lead)

- 0x4d6de0 was already ported (zai_firing, the lead point). 0x4a2ef0 is the
  vehicle's vt+0xe0, a debug readout into the HUD's text lines 1-4
  (vehicle_debug_lines); Vehicle +0x64 is a zone record (count + 3 ids).
- test_veh_damage.py gained 200 readouts (MFC CString modelled in Python),
  8 breaks caught. _ftol's 64-bit truncation through crt::msvc_ftol.
- Next: 2a2, the 0xa70 basic controller (0x4ae270, 0x4ae3c0, 0x4ae6b0,
  0x4a2a00), then 2b and 2c, then putting vehicles in the program.

## 2026-10-23 Item 4, 2a2 deferred; step 2b1: the gun's aim (lead)

- 2a2 (the 0xa70 basic controller) deferred: every shipped non-multiplayer
  aiv record names a compiled net (513 MW3, 631 PM; the remote_N slots
  are multiplayer), so every shipped vehicle gets the AI controls; the
  0xa70 controller is made only for a vehicle without one (unshipped data,
  the developer's place-a-vehicle dialog 0x403820, the port's FIX for a
  `mech` suffix without a net). Its path list ([0x5f4c4c], 0x402xxx) is
  built from version-105 nets, none shipped.
- 2b1: the pitch limits (0x4a4180, 0x4a41b0 with a FIX (bug): hypot(x, y)
  for hypot(x, z); no effect: the track 0x4b6e30 drops the answer; the
  review caught my claim otherwise and two frames I had backwards), the vehicle's vt+0x88..+0x98, +0x150, +0x154, the AI
  controls' 0x4e7490 / 0x4e7570 / 0x4e72f0 (TurretAim). AiVehicleControls'
  own x38 dropped for the base's state (+0x38).
- test_veh_aim.py: 600 runs, 13 of 14 breaks (one equivalent); test_fixes
  fix_vehicle_gun_clamp.
- 2b2: AiVehicleControls as the brain's BrainControls (the slots table in
  vehicles.md), the controls' answers as VehicleControls virtuals (the
  VehBasicHooks defaults now call them); test_ai_brain.py checks the
  original's slots on its 690 vehicle cases.
- Next: 0x49f660 (the collision points at setup) and 0x4a2ae0, then 2c,
  the frame 0x4a1040 (0x4a1240 the movement).

## 2026-10-23 Item 4, step 2b3: the setup's points and zone (lead)

- 0x49f660 and 0x49f710 (veh_basic_object), the zone match 0x53cd20
  (gmod_cull: zone_common). Vehicle +0x64 is the zone record.
- test_veh_setup.py: 600 runs, 12 breaks caught.
- Next: 2c, the frame 0x4a1040 (with 0x4a2ae0, 0x4a1240 the movement,
  0x4addc0 the engine sound, 0x4b4880 the weapons), and zEntity's zone
  update 0x5a80f0.

## 2026-10-23 Item 4, step 2c1: the movement helpers (lead)

- mech3/veh_move: 0x4a28c0, 0x4a2890, 0x4a2b10, 0x4a2dd0, 0x4a2c90 and the
  turn steps 0x4a2a00 / 0x4a2ae0 (VehicleControls::turn_step). New fields:
  sliding +0x34, velocity +0x64, top_speed +0x88, top_turn +0x8c, x314,
  heading +0x748. The NaN rules of the x87 compares matter here.
- test_veh_move.py: 1200 runs, 13 of 14 breaks.
- Next: 2c2, 0x4a1760 (285 lines: the track's ground following and the
  top speeds), then 2c3: 0x4a12c0, 0x4a1240, the frame 0x4a1040, the
  collisions 0x49ff80, the engine sound 0x4addc0, 0x4b4880, BASIC 0x4a1d90.

## 2026-10-23 Item 4, step 2c2: the suspension (lead)

- 0x4a1760, 0x4a23f0, 0x4a2820, 0x4abf70, 0x4a3e40, 0x4a3ed0 in veh_move;
  0x523e70 (zmth::rotate_y); the hit reader in the probe shared.
- Found: the quicksand/lava slowdowns never apply (the pick's flag counts
  water only): a fixes.md candidate for Owen and a discovery.
- test_veh_suspension.py: 600 runs, 22 of 23 breaks (one equivalent).
- Next: 2c3: 0x4a12c0 (the TRACK step), 0x4a1240, the frame 0x4a1040,
  the collisions 0x49ff80, the engine sound 0x4addc0, 0x4b4880, BASIC 0x4a1d90.

## 2026-10-23 Item 4, step 2c3: the frame (lead)

- 0x4a1040, 0x4a1240, 0x4a12c0, 0x4addc0, 0x4b4880 in veh_move; new fields
  x7c, x_ac, last_matrix, player_at; VehicleControls::update virtual.
- test_veh_frame.py (over test_veh_suspension.py's builder): 600 track
  steps and moves, 300 frames, 24 breaks caught. A support's speed is
  (ground - y) / dt, so an ulp of the matrix's sin or cos is 1e-6 of it:
  the frame's compares allow that.
- Next: 2c4, the collisions 0x49ff80 (0x4a0180, 0x4a1020, 0x4a0b20,
  0x4a0b00, 0x4a0100, 0x4a0b40); BASIC's 0x4a1d90 / 0x4a1f00; then the
  vehicles into the program (the entity manager's walk, the sensors).

## 2026-10-23 Item 4, step 2c4: the collisions (lead)

- 0x49ff80, 0x4a0180, 0x4a0910, 0x4a09e0, 0x4a0f50, 0x4a0b40 in veh_move;
  CollisionHit moved to collision_hit.h and Vec3 to vec3.h (the Handle
  clash). FIX (bug): the fire-floor crossing (a crash).
- test_veh_collide.py: 600 runs with the original's own sweep, sorting and
  responses (zWeapon's touch and the query stubbed), 24 breaks.
- Next: BASIC's 0x4a1d90 / 0x4a1f00 (the stealth object), then the
  vehicles into the program.

## 2026-10-23 Item 4, step 3a: vehicles made in the program (lead)

- app_vehicle.cpp: the basic vehicle's and controls' hooks wired, the
  mission start's vehicle path (WiredPlayerHooks), zWeapon's handler;
  Vehicle::entity_moved / refresh_markers moved from Mech to the base.
- c1m1: the four vehicles made, placed, set up, on zEntity's list
  (headless test updated; the sensors' count now 7).
- Found: the collision count stays 0 on the model path (every shipped
  vehicle): vehicles cross walls unless sliding; a fixes.md candidate.
- Left for 3b: the vehicles' frame in the entity walk and the brain's hooks
  for vehicles (positions, velocity, matrix, health, speeds), the teardown,
  BASIC's steps (stealth only, never made), the mobile HQs.


## 2026-10-23 Item 4, step 3b: vehicles run in the program (lead)

- The entity walk runs a vehicle's frame (vt+0x34 0x4a1040) when +0x09 is
  set. The brain's hooks (WiredBrainHooks), the sensors' slots
  (ProgramSensors) and valves.zrd's (WiredValveHooks: found by name,
  destroy through vt+0x70, controls and brain) answer for vehicles from the
  vtable 0x5c88e8 (vehicles.md, Brain, sensors and the engine sound). The
  mech helpers that read only zEntity's fields take any vehicle.
- The engine sound's claim and release (0x4adc30, 0x4adc60, 0x4adcd0,
  0x4adce0, the far point 0x4adb60): test_veh_engine.py, 400 runs, 6
  breaks caught.
- c1m1 headless: the trucks drive to node_08/09 and wait for truck_go,
  then die (the destroy valves, or the Owens on the way); the APCs drive off.
- Left: the teardown at the mission's end, the kill credit (0x41b420), the
  mobile HQs (step 3), BASIC's steps.
