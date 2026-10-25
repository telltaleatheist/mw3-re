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

(Resumed 2026-10-24: the lead took steps 1a..3c meanwhile, below. Rule
from the lead: the related test set includes every test that imports a
changed test's harness.)

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

## 2026-10-23 Item 4, step 3c: teardown and kill credit (lead)

- The mission's end frees vehicles (0x4ac630(1)); a mech's kill of one
  counts in its +0x3f8/+0x404 and salvages (0x472ec0, add_vehicle_salvage
  over the weapon manager); a vehicle's kill credit 0x4a5bc0 is empty.
- c1m1 headless: the player's kill of apc_02 (+1 vehicle, +75), all four
  freed at the cleanup.
- Left: the mobile HQs (step 3), BASIC's steps (stealth only).

## 2026-10-24 Item 4, step 4a1: the mobile field base made and run (mac 4)

- mech3/mech_fieldbase: FieldBase over BasicVehicle (0x160, vtable
  0x5c23d8: dtor, frame, death, vt+0xf0/+0xf4 itself, setup, damage), the
  create 0x4589d0 / 0x458a70, the registry 0x605ae0's add 0x45a030 (three,
  the campaign's +0xa8 at most; past them out of play; FIX (bug): a fourth
  written over the first valve), alive 0x45a000, clear 0x459e50 and the
  registry part of the mission start 0x459e80 (eleven radio lines,
  mfbeffects, [0x60b4d0]); the setup 0x458d40 (nodes, the arm script
  "mobilehq" 0x458890, the AI record), the frame 0x459640 (the queued
  steps, the throttle held), the arms 0x4597d0 / 0x458900, the damage
  0x458f50, the death 0x4590b0 (the campaign's cut a hook, `base_lost`),
  the destructor 0x458bc0. The limiter classes 0x4a42f0 / 0x4a4580 /
  0x4a4800 are the gun's (motion block); the field base makes none.
- Program: WiredPlayerHooks' mobilehq_create, the class flag
  (`BasicVehicle::field_base`, ours) dispatching the death, damage, frame,
  setup and free; vt+0xf4 for the station keeping; motion.zbd opened at the
  mechs' set-up (0x413e60, as the exe; before, only with the first mech's
  data, after the bases' arms wanted it). The debug launch's bases left
  read 3 (it has no campaign game).
- test_veh_fieldbase.py: 300 runs each of create, setup, frame, arms,
  damage, destructor (and 20 deaths), 45 branches reached, 25 breaks
  caught. The basic vehicle's own pieces are seams (FieldBaseHooks
  base_*), stubbed on both sides.
- c1m1 headless: three bases registered, alive and held in place, 11 of
  the 12 dock names found (no shipped sound set has mcb_deploy), one's arms
  opened through its frames, one shot down by the player (no campaign
  behind the debug launch, so no cut), all freed at the cleanup.
- Other c1m1 tests updated: seven program vehicles and ten entities; the
  Owens, woken, now has a firing plan at frame 200 (phase 2) on a field
  base (team 2, 530..600 m, inside its 1000 m sensor range) where it had
  none: the bases are its nearest enemies, as in the exe.
- Found: the bases a campaign has lost are hidden and inert in later
  missions and left out of the fail condition; with none left there is no
  fail condition; bases take the difficulty's damage share like the
  player's mech; they stand still until the dock lets them drive; the
  death's campaign loss is not guarded by the death's once.
- Split of the rest (step 4b, the repair dock, about 1,700 decompiled
  lines): 4b1 the dock's states and the drive step (0x459b80, 0x45ab30..
  0x45b9e0, the step 0x5c2540: 0x4598a0..0x459af0, 0x45b9c0); 4b2 its
  frame 0x45a270 with 0x45a780 and the repair 0x45a180 / 0x45a140 (the
  game state 4, the effects 0x458cf0); 4b3 the death's store cut (0x4590b0
  after 0x4ad2d0, 221 lines); 4b4 the AI's deploy state 0x4d0ce0 (157,
  reads [0x60b4d0] / [0x60b4cc]).

## 2026-10-24 Item 4, step 4b1: the repair dock's states and drives (mac 4)

- mech3/mech_fieldbase_dock: DockStep (vtable 0x5c2600, the base of the
  states and drives; FieldBaseAction renamed), the drive 0x5c2540
  (0x4598a0..0x459af0, 0x459a80, 0x45b9c0), the seven states in the
  registry (idle 0x45a740.., contact 0x45acb0.., places 0x45ab50..,
  deploy 0x45af00.., repair 0x45b2f0.., close 0x45b460.., reset
  0x45b6c0..; 0x45b770, 0x45b7d0, the unwinds), the registry's
  constructor 0x459b80 and helpers 0x45a720, 0x45b740, 0x45a500, 0x45a540,
  0x45b290, 0x45b2c0, the effects 0x458cf0 / 0x458d20, and the rest of the
  mission start 0x459e80 (the state list, "mcbdeploy"): 0x459e80 is whole.
- Boundary: the idle state's message 0x45a780 (the request: the AI orders,
  the dock points 0x45a560, the six states queued) stays with the frame in
  4b2, as a hook (`dock_request`). 0x45b9e0 on is the objectives' code, not
  the dock's; 0x45b860 / 0x45b900 are 0x45a780's and the death cut's
  helpers.
- FIX (bug): a drive freed by its state while still on its base's steps is
  kept until the base drops it (the exe reads the freed drive; no change in
  play); a missing base (fewer than three) is passed by the drives and the
  places' check.
- test_veh_dock.py: 600 random state sequences and 300 drive sequences,
  89 branches (each slot of each state, both answers of each step and
  done), 30 breaks caught. c1m1 headless: the dock as the mission start
  leaves it (the idle state alone, to begin; mcbdeploy 0; the bases held).
- Found: the dock waits for the mech within 12 m of its point and powered
  down; powering up during the opening or the 12 s repair aborts it; the
  silent mcb_deploy line is the "bases opened" one.
- Next, 4b2: the dock's frame 0x45a270 (which runs the state list and asks
  the idle state), the request 0x45a780 with 0x45a560 and 0x45b860, the
  repair 0x45a180 / 0x45a140.

## 2026-10-24 Item 4, step 4b2: the dock's frame, the request, the stations (mac 4)

- mech_fieldbase_dock: the frame 0x45a270 (the states off multiplayer;
  the station repair in it), the request 0x45a780 (the idle state's
  message, with 0x45b860's back order and the net's element by name
  0x58b0a0), the rings 0x45a560, the station repair 0x45a180, its message
  0x45a140 and end 0x45a0f0. zai_brain: Brain::go_to 0x4c84b0 (the
  finder's pilot kept on the brain: the exe passes the brain itself).
- Program: the mission frame's 0x45a270 hook; the dock's clock, places,
  powered down, view, rest, effects wired; a stand-in dock point for the
  tests (the mcbwaynet is not ported).
- The lead's questions. 1: a step answering finished after its abort
  unwound the list marks the dropped state (the repair), not the close it
  unwound to: the close (begun by the unwind) runs from the next frame and
  the bases close; nothing a player sees. My 4b1 note that the repair's
  abort is silent was wrong: its end, called by the unwind, says
  mcb_abort. 2: "mcbdeploy" is made by the mission start (valve_get makes
  a missing one), so every mission has it, modded or not; no guard.
- test_veh_dock.py: 600 frame, request, ring and station runs on top of
  4b1's (100 branches), 26 more breaks caught. c1m1 headless
  (mission_c1m1_field_bases_repair_the_player_headless): with the dock
  point stood in at net_02's "MFB Baker", the player's request, the bases
  reach their places by their nets, the player onto the point powered
  down, the deploy, 12.1 s of repair and 0x4154e0 once, close, reset,
  idle.
- Left: the mcbwaynet (mech_waynet.cpp, ~1,100 lines) and the MFB command
  0x47a7a0 / 0x47a730 (4b3), so the player can ask in play; Mech::repair
  0x4154e0 (the mech's); the death's store cut 0x4590b0; the AI's deploy
  state 0x4d0ce0.

## 2026-10-24 Item 4, step 4b3: the mcbwaynet and the MFB command (mac 4)

- mech3/mech_waynet: Waynet (0x479640, over the compiled net: element 0
  the default behaviour), its selection and marker (0x479a50..0x47a270),
  WaynetPoint (0x47a320, 0x47a480..0x47a530), McbWaynet (0x47a550..
  0x47a6b0), the MFB command 0x47a7a0 / 0x47a910, 0x47a730, the make and
  free 0x4d5460 / 0x4d54f0, and the net compile's arrangenodes point
  (0x4bde20's, through NetHooks::waynet_point). FieldBases::set_requester
  answers now (0x45b740 returns whether it took it).
- Program: the dock's point is the mcbwaynet's selected one (the stand-in
  gone); the dock's marker clear is the mcbwaynet's; the nets' begin, the
  mission's end (0x47a720, 0x4d54f0), the mission's waynet (the missions'
  +0xb0 as a handle), MCB_COMMAND (0x73) and N / Shift+N / Ctrl+N on the
  mcbwaynet (vt+0x60..+0x68); the mcbwaynet's frame (vt+0x6c 0x4cd490)
  does nothing.
- test_veh_waynet.py: 600 runs, 300 points, 25 branches, 25 of 26 breaks
  (nearest's fallback is not reachable). c1m1 headless: the MFB command
  picks "MFB Baker" (the mission's one dock point) and asks; the repair
  runs as before; the mission's waynet back to none after.
- Split: 4b3b, the multiplayer nav waynet (0x4797c0, net files 105 / 107:
  0x4be690, 0x4bed90; vt+0x6c 0x479c50, 25 m) and the HUD's waypoint icons
  and arrows (0x4798f0's images, 0x479dd0..0x479ef0; the HUD's 0x479ed0
  call): single player has no nav waynet, but the HUD would show the MFB
  waypoint while picking.
- Left after: Mech::repair 0x4154e0, the store cut 0x4590b0, the AI deploy
  0x4d0ce0 (and the AI's MFB ask 0x4ea2b7 -> 0x47a780).


## 2026-10-24 Item 4, step 4b4: Mech::repair (mac 4)

- mech3/mech_repair: Mech::repair 0x4154e0 (internals, ammo from the
  store by need, the armour's cost and restore, coolant, jump fuel, the
  AMS, the player's HUD, +0x680) and the limp's end 0x415a90;
  RepairHooks (the store 0x479430, the view's target, the HUD's 0x430aa0 /
  0x446520 / 0x43be10). The store's two calls sit in
  mech_repair_store.cpp: pilot.h's `using zgame::Handle` clashes with
  mech.h's. The armour cost in float (play's 24-bit x87).
- Program: the dock's mech_repair calls it for a mech (a non-mech
  requester keeps the hook's unported default). No pilot in the debug
  launch: no store, so no ammo or armour then (a port gap, no FIX).
- test_mech_repair.py: 800 repairs, 19 branches, 29 of 29 breaks (the
  `count <= need` edge and the count's clamp are not observable). c1m1
  headless: a pilot whose game store holds every ammo and armour, the
  player's Mad Cat worn down before the request, after the 12 s all back
  (440 rounds from the store, 18 half tons of armour; jump fuel stays 0,
  no jets). program_mech_wear / _wear_down (app_vehicle.h) for it.
- Left: the store cut 0x4590b0, 4b3b (nav waynet, HUD waypoints), the AI
  deploy 0x4d0ce0.

## 2026-10-24 Item 4, step 4b5: a field base lost (mac 4)

- mech3/mech_fieldbase_cut: field_base_cut, 0x4590b0's part after the
  basic death, on the current campaign game: +0xa8 down one; with bases
  left, f 0.7 / 0.5; message 0xf07; the mechs listed (to 64), keep =
  min(ceil(f n), 2 a base) drawn off at random, the rest "Chassis Lost"
  (0xf05) into +0x78; the tons (0x45b900) cut to min(f tons, 300 t a base)
  by random items (ammo by 0x4b4900's lot) into +0x78, ten (six) idle
  draws ending it; 0xf06 with "%.2f" tons. FieldCutHooks: rand, the mech's
  name, lab_rounds, the message (the program's: the mission's panel).
- Found: the fallback after the draws (0x45951d..) never moves an item
  (its search always ends at the list's head): kept, a fixes.md
  candidate. The 64-entry mech list overruns past 64 (and a draw of 64
  zeroes the saved exception frame): FIX (bug), a vector (fixes.md,
  CHANGES.md). An unlimited item drawn is copied into +0x78 but stays.
  [0x605c3c] is written, never read.
- test_field_cut.py: 600 runs, 11 branches, 26 of 27 breaks (limit > 0
  against >= 0 is not observable: a 0 limit means 0 tons). c1m1 headless
  (mission_c1m1_field_base_lost_cuts_the_campaign_store_headless): a
  campaign pilot with 5 spare mechs and 563 t, mobilehq_03 shot down:
  bases 2, 4 mechs kept, 394 t (70 %), the rest in +0x78, the panel shown.
- Left: 4b3b (nav waynet, HUD waypoints), the AI deploy 0x4d0ce0.

## 2026-10-24 Item 4, step 4b6 (assigned by mw3 mac 1, in progress)

- Branch mac4/ai-deploy from main e731bab. The AI's side of the field
  base: the deploy state 0x4d0ce0 and the AI's MFB ask 0x4ea2b7 ->
  0x47a780 (left from 4b3). Port with address comments, differential with
  breaks, docs (vehicles.md, ai.md, symbols, discoveries), FIX rows where
  needed. If the deploy state pulls in much unported brain code, stop at a
  clean hooks boundary and report what is left.
- Also check from the 4b2 review: (a) a repair or deploy abort returns true
  after cancel() made the close state the front: does 0x45a270 end the close
  at once in the exe, and the port the same? (b) DockReset::begin writes the
  mcbdeploy valve with no null check: can it be null in play?
- Fix CHANGES.md's wording: exactly 64 mechs is affected too (the draw at
  slot 64), not only more than 64.
- After: 4b3b (the nav waynet, the HUD waypoint icons).
- Done (mac 4): mech3/zai_repair: refit_order 0x4ea260 (the BrainHooks
  refit's default now), RepairBehaviour 0x4d08d0 / make_state 0x4d0990,
  repair_possible 0x4d0920, RepairState 0x4d0a10 / 0x4d0ba0 / 0x4d0c60 /
  0x4d0ce0; BrainHooks bases_alive, mfb_ask (0x47a780), has_mcb,
  dock_point, dock_drive, dock_contact, mech_power, wired in the program
  (app_mission's brain hooks through app_vehicle's program_* calls).
  No unported brain code was needed: the steering, walk and turn were ported.
- test_ai_repair.py on test_ai_lance's world: 300 cases, 20 branches, 28 of
  31 breaks (3 not observable). The state is built in place in the exe's
  order, so its unwritten fields hold old bytes: the test zeroes them.
- (a) The abort's true: 0x45a270 marks +0x04 on the state it read before
  the step (the dropped one), not on the close; the port's `f` is the
  same pointer, and every state's begin clears its flag: the close is not
  ended early. Nothing changed. (b) mcbdeploy: 0x4d3c40 makes the valve
  when missing, and the reset runs only in a mission after the start set
  it: not null in play. Nothing changed.
- CHANGES.md / fixes.md: 64 or more mechs (the draw at slot 64).

## 2026-10-24 Item 4, step 4b3b (assigned by mw3 mac 1, in progress)

- Branch mac4/nav-waynet from main e8c8644. The multiplayer nav waynet
  (0x4797c0, net files 105 / 107: 0x4be690, 0x4bed90; vt+0x6c 0x479c50) and
  the HUD's waypoint icons and arrows (0x4798f0's images, 0x479dd0..
  0x479ef0; the HUD's 0x479ed0 call). Port with address comments,
  differential with breaks, docs (vehicles.md / hud, ai.md if touched,
  symbols, discoveries), FIX rows where needed. If the icons pull in
  unported HUD drawing: the logic up to a hooks boundary (what is drawn
  where), and report what is left.
- Fold in: the 4b6 discovery's "repaired from your carried gear" and "does
  not fire": keep if traced (0x4154e0 on the lancemate's mech from the
  campaign store; the fire gate during the state), else soften to the
  fight-picking (+0x80) being off.
- Done (mac 4): every campaign and training mission has a nav waynet
  (waynet_N.zrd, version 107); 4b3's "multiplayer only" was wrong (ai.md
  had it right). Ported: the build 0x4be690 (zainetedit build_waynet:
  no per-link slots, the counter's vtable 0x5ca48c), the make 0x4797c0
  (nav_waynet_make), the frame 0x479c50 (Waynet::frame), the edge arrow
  0x479ef0 / 0x479dd0 / 0x479ed0, the icons 0x4798f0 / 0x479a30 /
  0x479850. The +0x28 list holds points (the objectives' watched nav
  points, 0x45cf9d), not ints: `watched`.
- Program: the mission reset opens the readers (0x490630, which the port
  had left out) and makes the nav waynet into +0xb0 / +0xb4; the cleanup
  deletes it; its frame runs; N / Shift+N / Ctrl+N move it; the
  objectives' nav_find / nav_show / nav_position use it; the HUD's radar
  marker ("OP: <name> [km]", not a network player's as hud.md had it) and
  the edge arrow. HudHooks::marker_name answers a string now.
- FIX (bug): the frame's skip after the selected point (behind bugs); a
  selected element with no place (frame, arrow, radar: the MFB picking can
  wrap onto the mcbwaynet's default behaviour, likely a crash in the
  original, not seen in play); the 107 build's unchecked links, buffer and
  elements; 0x479850 releasing four of the eight arrow images (behind bugs).
- Tests: test_nav_waynet.py (47 shipped files, 300 made-up), test_nav_hud.py
  (600 runs, 20 branches), test_veh_waynet.py updated for the points list,
  c1m1 headless mission_c1m1_nav_waynet_headless; the dock test's end now
  the nav waynet. 4b6's discovery: "repaired from your carried gear" kept
  (traced: the dock's repair on its requester, the current game's store);
  "does not fire" softened to the engage and manoeuvre gates (may_fire
  gates the hit reaction and the timed manoeuvres, not the guns).

## 2026-10-24 Item 4c: the turrets (assigned by mw3 mac 1, after 4b3b merged as 3f407a4)

- From mw3 mac 1: rebase on main. The turret vehicles' remaining code:
  whatever of their class, frame, aim/fire and death is not ported yet
  (check vehicles.md and ai.md for what is left). Same bar as before
  (port with addresses, Unicorn differential and break tests, docs,
  FIX rows, related tests plus mw3tests / mw3sdltests). If the scope is
  big, split it, do the first part and report the split.
- Wait for Owen's compaction before starting.
- Scope (mac 4): nothing of turret.cpp is ported. The class: 0x284 bytes,
  vtable 0x005c8770 (55 slots, over the AiTarget / zEntity3d base the basic
  vehicle shares), ctor 0x4a5c60, setup 0x4a6270 (the aiTurrets.zrd entry),
  dtor 0x4a5ff0 / 0x4a5fd0, the loader 0x4a76b0 / cleanup 0x4a7970 (three
  HUD marker images, the TURRET blocks over the world's nodes), the frame
  0x4a6be0, the pick 0x4a70d0 (+ 0x4d72b0 in ai), the aim 0x4a71c0 and the
  fire. Shipped: aiTurrets.zrd and turret.zrd (zbd/reader.zbd), bossturr /
  triturret (c1), dometurr (c4b). Split:
  - 4c1: the object: loader, ctor, setup, dtor, cleanup, the small slots
    (markers, tint, messages 0x32..0x34, deactivate 0x4a7620, damage
    0x4a7530, aux anims 0x4a7af0); it stands placed, takes hits and dies.
  - 4c2: the frame 0x4a6be0, the pick 0x4a70d0 / 0x4d72b0, the aim
    0x4a71c0 and the fire.
  - 4c3: the program: the mission load makes them into the entity list,
    their frames, HUD markers, kill credit (+0x3fc), a headless c1 run.
- Lead (approved as drawn; report each part separately): in 4c2 port the
  aim's bit-trick square root as written (std::memcpy / bit_cast, not a
  union) so its error matches; portable, no (A) FIX; the docs say how far
  it is from a true sqrt.
- 4c1 done (mac 4): `mech3/turret` (`mech3::tur`): the loader 0x4a76b0 and
  end 0x4a7970, the constructor 0x4a5c60, the setup 0x4a6270, the
  destructor 0x4a5ff0, set_node 0x4a6230, aux_reset 0x4a7af0, the slots
  (message 0x4a74f0, hit 0x4a7530 via the callback 0x4a7940, kill 0x4a7600,
  deactivate 0x4a7620, tint 0x4a75b0, 0x4a7a60 / 0x4a7a70 / 0x4a7ab0,
  0x46b780, the markers 0x4a5fc0), the turn limits 0x4a4b30 (built in
  place at +0x210: four kinds, their within / clamp left for the aim). A
  CZAiTarget-style object (zgame handles, TurretHooks), not a Vehicle.
  The C++17 build has no std::bit_cast: memcpy (zgame::from_bits) for 4c2's
  square root too.
- Found: the turret's +0xd4 is the last attacker (kill credit), +0xd8 its
  id; the outright kill (vt+0x70, the valves' destroy) credits the player;
  without a destroy animation no weapons callback (weapons cannot hurt it);
  deactivate puts the effect out only when it is not showing (as written;
  4c2 sees whether the frame covers it); the helicopters are turrets;
  idle turrets skip the whole frame.
- FIX (bug): a missing weapon without DETECTION_RANGE (null read at load);
  list items past a list's end or a non-list value read as none.
- Tests: test_turret.py (the shipped file x12 worlds, 300 made-up, 30
  port-only crash files; 40 branches; 40 of 43 breaks; NaNs compared as one: the x87's
  negative default NaN against the hardware's positive).
- 4c1 merged as main 923cd1a. 4c2 assigned (mw3 mac 1): the frame
  0x4a6be0, the pick 0x4a70d0 / 0x4d72b0, the aim 0x4a71c0 (the bit-trick
  square root through zgame::from_bits) and the fire. Settle: (1)
  0x4a7620's effect test (put out only when not showing) against how the
  frame lights the effect: bug or intended (a candidate if unclear); (2)
  aim_from's NaN height on a turret without firepoints: the frame must not
  use it, or say what happens; (3) the helicopters: do their animations
  move them, and does the frame's aim read the node or the setup's copy.
- 4c2 done (mac 4): Turret::frame 0x4a6be0, pick 0x4a70d0, pick_enemy
  0x4d72b0, target_player 0x4a7b40 / 0x4a7ba0, aim 0x4a71c0 (the bit-trick
  flat length through zgame::from_bits: 0 to 6.1 % over, exact at powers
  of 4), TurnLimits::within for the four kinds. The lead's three: (1) the
  deactivate's effect test is the wrong way round (a lit charge stays lit):
  FIX (bug) behind bugs; no world has tur_ppc's `lightning` node, so never
  seen; (2) +0xec is never read after the setup; (3) the helicopters fly by
  animation (chopper_path*), the frame re-reads the node's world position
  (0x4ed530) and the aim the node's world matrix, so both follow.
- Found: the turrets take turns (frame % turrets made == id): each acts one
  frame in N; the count is never reset across missions (FIX (bug) behind
  bugs: reset at the load); the turn gating ties their pace to the frame
  rate (candidate for Owen); after a pick the target becomes the player
  (aims at the player's place without a line of sight on the turns the
  search is skipped or empty); the mine turret goes off in place on a pick.
- Also fixed in 4c1's docs: 0x4eb0d0 / 0x4f9d60 both search the node's
  whole tree (not a direct child).
- FIX (bug): no healthy node in the frame, no gun in the aim (null reads).
- Tests: test_turret_frame.py (120 runs, ~8600 frames, 44 branches),
  test_turret.py gained the limits' within on every kind (48 branches); 47 of 47 breaks.
- 4c2 merged as main 6e29636 (the lead checked the count reset: 0x4a76b0
  runs once per mission, from two cases of one switch, 0x490c8e /
  0x490d25). 4c3 assigned (mw3 mac 1): the mission load calls the loader
  there (gated on [0x608e3a]); the frames through the entity list; the HUD
  markers and the kill count +0x3fc; turrets_end and the deletes at the
  mission's end; a headless campaign run (c1m1: tur_srm01..08) with
  turrets that pick, fire and can be killed. Turn-by-time stays a
  candidate (the lead takes it to Owen with per_second).
- Owen decided (via mw3 mac 1), added to 4c3: the turret turn goes to
  time, a 30 fps frame per turn, behind zfix per_second: a turret's turn
  once every count x 1/30 s on game time, the ids spreading the turns as
  the frame modulo does, 0x609a4c advancing per turn as before; fixes.md's
  candidate becomes a FIX (B) row, a CHANGES.md line; a unit or SDL test
  showing the same turn rate at 30 and 60 fps.
- 4c3 done (mac 4): app_turret.cpp (the hooks on the ports; zEntity3d's
  0x5a80f0 ported there), the mission load / cleanup / delete, the entity
  walk (frame while +0x09), the sensors' vtable answers, the brain hooks,
  the kill credit (a "building"), the HUD's vt+0x3c (the radar still mechs
  only). Port gap closed: the game clock [0x72c530] was never copied into
  weapon_world in the program (AMS and turrets stalled). Owen's FIX (B):
  turn_due on game time, fix_turret_turns_by_time. Found and FIX (bug)
  behind bugs: the turret's radar "friendly" and AMS side are the wrong way
  round (player's AMS ignores hostile turret missiles; hostile AMS turrets
  shoot their own side's). 0x4abea0 ported (lead_dir) and run for real in
  test_turret_frame. Headless: mission_c1m1_turrets_headless (14 turrets;
  tur_srm08 sensed, picks, targets the player, fires; killed: +1 building,
  200 points). 10 of 10 breaks over the wiring and the turn by time.
- Correction (lead's review of 4c3): the "sides the wrong way round" were
  not bugs. weapon.h's entity_allies hook answered the mech's +0xc8, its
  enemies: so 0x4b4a40's flag is "hostile to the player" for mechs
  (0x4b3550) and turrets (0x4a6fa9) alike, and the AMS's side 0 takes the
  shots not hostile to the player (an enemy AMS's; a mech asks by
  player.enemies & its team, 0x41927e). Both FIXes, their rows, the CHANGES
  line and the discovery dropped, the original calls back; entity_allies
  renamed entity_enemies, the flag "friendly" renamed "hostile"
  (radar_hostile 0x4b4a30, radar_not_hostile 0x4b4a20) in weapon, hud,
  turret and the docs. I read the hook's name, not what it returned.

## 2026-10-24 Item AI-last: the AI's last pieces (assigned by mw3 mac 1, after 4c merged as f734981)

- From mw3 mac 1 (approved order: this, then the animation-owner registry,
  then the radar contacts beyond mechs, mac 3's sign-off replaced by the
  lead's review of app_hud.cpp, hooks and markers only): the order stack on
  the formation net 0x4d8820 / 0x4d88a0 / 0x4d88d0 / 0x4d88f0, the IControls
  events 0x4e95b0 / 0x4ea700, the switches 0x4ea740 / 0x4ea750; refresh
  ai.md section 6; log the dead brain step 0x4cd7b0 as unported on purpose
  with why. Same bar.
- Done (docs only, the lead approved): the code part is unreachable or
  empty (ai.md 17.9, Not ported on purpose); section 6 refreshed; the dead
  brain step logged with why. Next on the same branch (the lead): the
  entity-forget chain 0x4aa6b0, 0x4aa4d0, 0x4aeec0 and 0x4b0370 (ported in
  weapon.cpp, the lead reviews it for mac 2); mech_dead's interim FIX (A)
  removed with its fixes.md / CHANGES.md rows; a headless test that a
  wreck is freed when the exe frees it with nothing left holding it
  (contacts, targets, a lancemate's target, a weapon's lock); any port-side
  holder still pointing at it reported, not papered over. Related set with
  test_weapons_run, test_weapons_fly (background, ~26 min), test_tag.
- Forget chain done (mac 4): sensors.cpp entities_forget 0x4aa6b0 and
  forget_entity 0x4aa4d0; the program's vt+0xd4 by class (app_sensors;
  program_mech_forget / program_vehicle_forget = 0x4aeec0, with the
  weapons' already-ported 0x4b0370 forget_target, unchanged in weapon.cpp).
  The program's weapons had no target_key (vt+0x24): wired (itself), so a
  lock remembers its entity as in the exe. mech_dead's interim FIX (A)
  gone (no rows of its own in fixes.md / CHANGES.md: its note said so); the
  wreck freed through mech_free. FIX (bug): the attackers (+0x664) naming
  the wreck (was fixes.md's "still to fix" entry), the forget's unfreed
  contacts. The targets.zrd objects are not told (not on zEntity's list in
  the exe either). Port-side holders still pointing at a freed wreck: the
  mission's AI mech list g_ai_mechs (the valves' target_find by name) and
  the engine's handle table (handles_ keeps the pointer; a new object at
  that address would get the old handle). Reported, not changed. 14 of 15
  breaks (the null contact case not observable).
- Lead's review (held): the two port-side holders fixed on the branch.
  mech_free drops the mech from g_ai_mechs / g_ai_names and the player
  hooks' mech set (mission_ai_mech_forget); GameEngine::forget_object on a
  mech, vehicle or turret freed (FIX (A): the exe has no handle table, it
  passes raw pointers). The other registries checked: app_vehicle's
  g_vehicles / g_order and app_turret's g_turrets / g_locks already drop
  theirs; app_mech's mech_objects too; app_turret's g_places is keyed by
  handle and holds copies (no pointer). The c1m4 test shows both (the
  name finds nothing, the old handle answers null).
- Lead merged AI-last and the forget chain (c419ba7).

## Item 2: the animation-owner registry (2026-10-24, mac 4)

- Assigned (the lead): the animation-owner registry 0x4d4f20, 0x4d5100
  → 0x4d47e0, 0x4d46f0, 0x45b800, 0x508f00 / 0x508f20 / 0x508f30, with the
  callers 0x4d34a2, 0x4d33a5, 0x4a66af, 0x4aa6b8 (the last is the forget's
  release, logged as a stub until now). Branch mac4/anim-owner from main.
  Same bar: port with addresses, differential and break tests, docs
  (ai.md, symbols.csv, discoveries.md, fixes.md / CHANGES.md if a fix),
  the related set plus both suites, a brief report.
- Scope found wider (mac 4): the record's valve and most owners come from
  the mission load's binder 0x4d4b30, and the callback's handler 0x4d4a20
  is the point of it all. Split approved by the lead: 2a the registry
  (records, claim, release, the callback and handler with 0x4d4980, the
  listeners), 2b the binder. The lead's notes: the inout case calls the
  ported 0x4c3bd0; vt+0x74 / vt+0xdc by class; the differential sends every
  value 0..0x3f with owners none / mech / turret; breaks for the valve bit,
  a missed owner call, the 0x2a..0x31 range; the leak decided either way.
- 2a done (mac 4): `mech3/zai_anim_owner` (0x4d4f20 with 0x4d46f0 /
  0x45b800 / 0x508f20 / 0x508f30, 0x4d5100 -> 0x4d47e0, 0x4d4b10 ->
  0x4d4a20, 0x4d4980, 0x4d4700, 0x4d4740); CZAiTarget::message (0x4d7270
  with 0x4d3340 and the kill). The registry is a mission listener: its
  slot +0x10 empties the records at the mission's end (before the
  animations stop) and +0x0c frees them with the world (before the
  animations go): bounded, no leak, kept as the original (ai.md 18). The
  owners' vt+0x74 by vtable: the turret's 0x4a74f0, the targets' 0x4d7270
  (0 / 0x38), everyone else's 0x4d7270 on empty slots; vt+0xdc only for
  the player's vehicle: the Mech's and the basic vehicle's, the rest empty
  or value slots (none unported). FIX (bug) viaNode missing or no vehicle
  (unreachable with shipped data: no viaNode, no 0x11 sent). Program:
  ProgramAnimOwner in app_sensors.cpp; the targets' and turrets' claims,
  the forget's release and the listeners wired. test_anim_owner.py 400
  cases, 16 of 16 breaks; headless c1m1: tur_boss01 woken by
  raise_bturret1, the end's records emptied, the world's unload frees them.

## Item 2b: the animations' binder (2026-10-25, mac 4)

- Assigned (the lead, 2a reviewed): the mission load's binder 0x4d4b30
  (0x4d4c80, 0x4d4e50, 0x508f40, 0x50b570, 0x4d5110, 0x58f2a0). Branch
  mac4/anim-binder off mac4/anim-owner, rebased on main once 2a is merged.
  (1) every shipped world's bound animations checked against the exe's
  (owner and valve), a headless test with a c1 destroy animation's 0x2b
  turning its valve on in play; (2) the valve's parent walk tested past the
  root; (3) unported zEffect pieces hooked and listed.
- 2b done (mac 4): `zai_anim_owner` anim_owner_bind_all 0x4d4b30,
  bind_event 0x4d4c80, bind_commit 0x4d4e50 (the claim's record choice
  shared, record_for); zEffect's 0x508f40 (anim_each_callback), 0x50b570
  (anim_next_with_callbacks), 0x50b5e0 (anim_node) ported, none hooked;
  mission.cpp's fn_4d4b30 wired. FIX (bug): a valve walk on a def without
  a node (unreachable). test_anim_binder.py: all 6 worlds identical (121
  bound, 89 valves, 27 owners), again after the end, and a made-up round;
  test_anim_owner.py has the binder (past the root both ways); breaks 12 +
  4 (two HAS_CALLBACKS breaks each covered by the other test, replaced).
  Headless: citybuild01 rammed turns its valve on. Found in play: c1m1's
  opening call_ramp turns "bramp" on, which powers the Owens up (it stayed
  down before); mission_c1m1_player_fires_on_the_owens_headless moved to
  40 m (from 80 m the machine guns miss the moving Owens). Test-process
  reset frees the records (reset_process_state), as the tests never unload
  the world.
- Lead's review of 2b: matches the exe; the Owens test at 40 m kept. The
  node's entity is 0x40eee0 (the binder's call at 0x4d4dc1; 0x40e920 is the
  program's port behind it): symbols.csv rows added.

## Item 3: radar contacts beyond mechs (2026-10-25, mac 4)

- Assigned (the lead): branch mac4/radar-contacts off mac4/anim-binder,
  rebased on main once 2b is merged. app_hud's contact hooks for vehicles,
  field bases, turrets and targets.zrd objects, so the radar and the target
  list see them as the exe; their markers 0x4ac3d0 (vehicle), 0x4a5fc0
  (turret), 0x4d2d70 (target). Hooks and markers only, no HUD drawing
  rework (app_hud is mac 3's; the lead reviews it). Differential: the
  radar's contact list and each class's marker over the shipped worlds'
  entities; headless: c1m1's turrets or a target on the radar.
- Item 3 done (mac 4), test shape approved by the lead: `mech3/entity_classes`
  (the classes' slots by vtable: blip vt+0x3c, team, mech vt+0x50, hidden
  vt+0xa8, name vt+0xa4, centre vt+0xbc, 0x4e6fe0, 0x4e7070), the program's
  facts and hud_object_* in app_sensors.cpp, app_hud's contact hooks only
  (the contact's vehicle/entity/seen, team, place, blip, hidden, mech, name,
  centre, size, the lead point's place and velocity; the report counts
  blips by class). Markers: the vehicles' 0x4ac3d0 at the stealth step
  (0x4ac4e0 ported, the old ones deleted first: no stealth object in the
  port), the targets' made by app_mission's hooks. test_entity_classes.py
  300 cases, 9/9 breaks; test_hud.py with classed radar and TARGET objects
  (every class slot reached), 6/6 of the breaks; headless c1m1 (turrets,
  vehicles; the satdish has no `radar`: never shown, as in the exe) and
  t1m2 (four targets). Found: only training targets have `radar`; the
  vehicles' markers are the stealth object's. mission_c1m1_hud_weapon_lines
  test: its blip counts are the mechs' now (turrets and trucks show too).
- Lead merged item 3 (f0e2f22).

## Item 4: the models' effects, render leftovers (2026-10-25, mac 4)

- Assigned (the lead): branch mac4/model-fx off main. The model texture
  scroll 0x526ae0 (flag 0x10 at +8, u / v at +0x28 / +0x2c; callers the
  turret EFFECT 0x526ae0(1, 10, 0) and the interp command, interp.md ~487)
  and where the draw applies it (render.md section 3: unverified; port it if
  small); the model animate 0x541ad0 (the turret frame's step 5, the interp
  list's Clear(1)); the node blend tint 0x4fb8c0 and its switch 0x4fb840
  (the vehicles' damage reddening, vehicles.md 280 / 1237), whether the
  draw reads it (render.md's tint bit 4 [0x74a270]; a headless frame if
  wired). Per-frame steps to time behind per_second. Differential, breaks,
  docs, discoveries.
- Split by the lead: 4a (the scroll, its step, the blend, the wiring) now,
  4b next on its own branch (the mix's output stage 0x5680c0 / 0x579950 /
  0x579980 / 0x5798a0 with light_hooks set in the program, and app_hud's
  TARGET view vt+0x68; check whether lit or fogged models lose their mix
  too, a headless frame showing a damaged vehicle's red and a fogged
  model's mix).
- 4a done (mac 4): zmod model_set_scroll 0x526ae0 (gmod_init),
  model_scroll_tick 0x541ad0 (gmod_const: the scroll is the UVs moved, not
  the draw's; dt-scaled already), zcls object3d_set_blend 0x4fb8c0 /
  use_blend 0x4fb840 (NaN to 0 as the asm). Wired: the turrets' tint,
  EFFECT scroll and step; the vehicles' tint and treads; the interp's
  Engine (Object3DSetScroll*). test_model_fx.py 3000 cases, 18/18 breaks;
  headless c1m1 horscroll 0.015/s. The tests' process reset now resets the
  interp's ScrollAlways list (0x5962a0): a stale holder from the last
  test's freed world kept the next mission's from being made.

## Item 4b: the lighting's fog mix to the device (2026-10-25, mac 4)

- Assigned (the lead): branch mac4/mix-fog off mac4/model-fx, rebased on
  main once 4a is merged. mix_fog 0x5680c0 with 0x579950, send_mix_fog
  0x579980, point_color 0x5798a0, light_hooks set in the program's render
  wiring (mirroring zrndr_fog; the software ramp 0x56c170 may stay a hook);
  app_hud's TARGET view vt+0x68 (object_view_flag). Headless frame: a
  damaged vehicle's red and a point-lit model's mix.
- 4b done (mac 4): zrndr set_mix_fog_color 0x5680c0, zvid set_mix_fog_device
  0x579950 / send_mix_fog 0x579980 (NaN counts as the same, x87 C3),
  zmod::use_device_light_output() (point_color 0x5798a0, mix_fog,
  send_mix_fog) in the program's two render set-ups. Found: the port's
  0x579f50 left out [0x72f3f0] = 0x57bcf0, so no fog colour ever reached
  the device (the world's included): installed. The mech's TARGET tint
  0x41b9d0 with the parts' 0x462040, the thigh's 0x463aa0 (its foot too),
  the torso's 0x4643d0 (the hip too); app_hud's object_view_flag by class.
  test_mix_fog.py 2000 cases, 17/17 breaks (one replaced: the NaN
  threshold cannot be reached, both colours clamped first); headless c1m1:
  208/208/208 at the load, the damaged vehicle's 255/0/0 while tinted.
  Not reached: the mix itself, as no model is lit in the program (the
  GfxFlags Lighting bit; the lead traced it: item 4c).

## Item 4c: the GfxFlags (the models lit) (2026-10-25, mac 4)

- Assigned (the lead, who traced it): [0x74a2cc] points at the GfxFlags
  settings entry (zModel's start 0x5266f0: [0x74a2c8] = -1, then
  settings_find("GfxFlags_HW" / "_SW"); 0x526750(ECX name, EDX flags)
  re-points it, from 0x48074e (app.cpp:268's models_load) and the options
  menu 0x439c95 / 0x43a2e5); bit 0 Lighting (GfxFlags_HW 0x1b). Port the
  start's mask part, 0x526750 and render_load 0x557ae0 (zRender's copy:
  Transparency 0x02, Perspective 0x08, GlobalLight 0x10) with a
  differential; wire them in GameEngine to the settings entry so the
  options' LIGHTING works live. Headless c1m1: models lit (lights_reaching
  runs, the sun on a mech), mix_fog for a point-lit model; a before/after
  screenshot pair (PNG in scratch). A port gap: render-lighting.md only.
- 4c done (mac 4): zmod gfx_flags_start (0x5266f0's part) / use_gfx_flags
  0x526750, zrndr gfx_flags_start (0x557a95's) / use_gfx_flags 0x557ae0;
  GameEngine models_init / render_init (the parts, the rest still logged),
  models_load / render_load / model_use_gfx_flags. The lighting reads the
  option's value through the entry, so LIGHTING works live. zRender's copy
  only matters to the software path. test_gfx_flags.py 500 cases, 8/8
  breaks; headless c1m1: lit mean 376 (211 colours) against unlit 877
  (22), and with an explosion's light at a vehicle (started at its place,
  as a shot's impact does) the mix runs. The screenshot pair waits for
  Owen's yes to the GPU (the no-GPU-while-streaming rule).

## Item 5: an animation's light at its start node (2026-10-25, mac 4)

- Assigned (the lead): branch mac4/anim-light-at off main. light_flash
  started with plain anim_start at a vehicle's node left its light at
  (0, 0.5, 0); find whether the exe does the same with those inputs (zeff's
  ref_or_at with the event's node index, in the emulator against the port);
  same: document which caller play uses and close; different: fix with a
  differential and breaks. Say which game paths start light anims each
  way (shots' impacts, deaths, scripts). Small. Then item 6: the version-105
  waynet 0x4bed90.
- Item 5 settled (mac 4), docs only: the exe does the same. A free effect
  def (flag 0x100) started plainly at a node is rebound to it (0x509eb0:
  its node refs looked up again under it, `dummy02.flt` not found) and put
  at the zero pose, so light_flash's light stays at its offset (0, 0.5, 0).
  Checked with test_anim_run's harness on c1 (the start, three frames, the
  whole dump identical). In play light_flash comes from CALL_ANIMATION
  AT_NODE (0x506640 flag 1, anim_start_at at the node's world point);
  anim-run.md lists the start paths. Item 4c's headless test comment
  corrected (not a shot's impact, the AT_NODE call).

## Item 6: the version-105 waynet 0x4bed90 (2026-10-25, mac 4)

- Assigned (the lead, after item 5): the version-105 waynet 0x4bed90.
  Branch mac4/waynet-105 off main. Same bar: port, differential, breaks,
  docs, the related set and both suites.
- Item 6 settled (mac 4), docs only (the lead's (a)): 0x4bed90 and the
  older net class it builds through 0x402590 / 0x4025b0 (0x4021c0, vtable
  0x5bb8d0) are the editor's reload of a version-105 net; no shipped net
  file is 105 (the header reads a "mech3" 105 as 106), so they never run.
  ai.md 9 and 17.9's Not ported on purpose, 7 symbols rows.

## Item 7: the campaign's lancemates at launch (2026-10-25, mac 4)

- Assigned (the lead): branch mac4/lancemates off main. Port 0x49e5e0
  with 0x49e540 and the launch's sensor range 0x4d6d20, wired into
  mission_launch for game type 1; fold in the command marker's vcall
  answers (the plain AiTarget 0x5ccfe0, as the exe) with its own small
  check. Allies as the original (no engage change). Tests: a differential
  of the three over pilot cards with 0..3 lancemates, missing mechs and an
  IA/MP player (50000); breaks; a headless campaign mission with a
  lancemate slot filled (the mech made, its AI controls on, in the pocket
  panel, taking a lance order). Discoveries for players (the 1000 m cap).
- Found before starting: player_setup's slot hooks (0x433cc0, 0x4351c0,
  0x435230, the slot's +0x14) are unwired in the program too, so no
  campaign has lancemate slots there; 0x4351c0 / 0x435230 are not ported.
  0x4d6d20 is ported (AiTarget::set_sensor_range) but caps a NaN range,
  which the exe keeps (C0|C3).
- Item 7 done (mac 4): `launch_lancemates` 0x49e5e0 and `assume_duties`
  0x49e540 (player.cpp), `take_triggers` 0x49e4e0 split out (mech_control),
  `CampaignGame::set_lancemate` 0x4351c0 / `set_lancemate_count` 0x435230;
  0x4d6d20 keeps a NaN now. The program wires player_setup's slots to the
  campaign's GameSlots (the menus' pilot's, else a debug start's own), the
  launch's lancemates and sensor range, the player's settings (vt+0x90),
  TargetHooks vt+0x50 / +0x58, and the command marker's vcall answers.
  Tests: test_mission_start's third entry (the launch), test_pilot's ops h
  and k, test_ai_targets with NaN, mw3tests c2m1 lancemate (made, brain,
  panel, order 0 followed; the empty slot's 9 triggers to the player) and
  c1m1 marker; 21 of 21 breaks. Docs: mission-start.md 12, ai.md, mech-build,
  fixes (2 rows), CHANGES (1 line), symbols, 3 discoveries.
- Found by the full suite (placements on every campaign mission, crash at
  c2 mission 4, found with an ASan build): the program's IControls vt+0x54
  stand-in (the controls' brain) looked the mech up in zEntity's list, which
  the entity sort asks it about while the list is mid-sort. With two
  brained lancemates (Dominic from his slot, Epona a plain ally) the
  comparator's answer changed between calls and the exe's unguarded
  insertion read before the list. Fixed in app_sensors.cpp: by the AI mechs
  made.

## Item 8: Instant Action's line-up 0x49dc80 (2026-10-25, mac 4)

- Assigned (the lead): branch mac4/ia-lineup off main. 0x49dc80 driven
  from a record: a differential over the record's enemy and ally counts,
  mech choices, waves, the endless and wave modes (the win's re-run
  0x49dc80(0)); a headless IA mission from a made record, the enemies and
  allies made with their AI on, wave 2 after wave 1 falls. IA-screen state
  not ported: from the record, the rest hooked. Then item 3's two
  leftovers, 0x4d8610 and 0x4ac800.
- Found: ActionGame's line-up methods (0x4368b0, 0x436a90, 0x436af0,
  0x436ac0, 0x436840) were not ported, nor its +0x44 mode, +0x48 field
  bases, +0x70; the launch's objectives start (0x45c4a0) was unwired, and
  the objective hooks it needs (zEntity's list, active, hostile, vt+0x48,
  the game's mode and wave) were unanswered, so no IA mission could be won.
- Item 8 done (mac 4): `action_lineup` 0x49dc80 (player.cpp, the wave
  table 0x5e33a8 in it), ActionGame's slot_at / ally / remove /
  drop_enemies / add and its +0x44, +0x48, +0x70, count, side bytes. The
  program: the Launch's `action` game (a debug start's) or the pilot's;
  the IA hooks in WiredPlayerHooks, the launch's line-up and objectives
  start, the win's next wave, the objective hooks for "destroy the
  enemies". Tests: test_mission_start's ia1 / ia0 entries (400 cases),
  test_pilot's a / e ops, mw3tests mission_ia1_waves (wave 1 made with
  brains, destroyed, wave 2 made, the ally kept); 22 of 22 breaks. Docs:
  mission-start.md 13, fixes (1 row), CHANGES (1 line), symbols, 2
  discoveries (the twenty waves, the matching by weight class).

## Item 9: item 3's leftovers, 0x4d8610 and 0x4ac800 (2026-10-25, mac 4)

- Assigned (the lead): branch mac4/ai-cleanup off main. Port 0x4d8610 (the
  AI list reset) and 0x4ac800 (the vehicle controls' destructor, linking
  its FIX row), wired where the exe calls them; a headless restart twice
  with the AI lists and vehicle controls not carried over, also under
  ASan; a differential and breaks.
- Done (mac 4): `ai::ResetList` / `reset_list` (zai_list; the four lists
  are never filled in the exe: only their ctors, dtors and resets name
  them), wired to the Core frame's and the mission load's calls;
  `ai::vehicle_controls_gone` 0x4ac800 with its two calls as hooks (the
  program: the vehicle itself, then the basic path's 0x402510). Tests:
  test_ai_lists (3000), test_ai_brain (its 0x4ac800), mw3tests c1m1
  restarted twice (also ASan); 6 of 6 breaks. ai.md 19, symbols. No
  CHANGES line (port gaps); the fix row was there.

## Item 10: the AI's heat (2026-10-25, mac 4)

- Assigned (the lead, after the survey): branch mac4/ai-heat off main.
  BrainHooks::heat by class (vt+0x9c across the vtables), player.cpp's
  mech_update (mech.vt+0x34); whether any shipped vlist uses flag 0x20
  (ai.md); the survey's env patch folded in; headless: an AI pick changing
  with a target's heat and the powered-down AI mech's one 0.1 s update.
- Done (mac 4): vt+0x9c is 0x41ec90 (the Mech's +0x554) on both mech
  vtables and 0x45b950 (the constant 0.0 at 0x5c23a0) on every other
  entity class; the program answers mech_heat. WiredPlayerHooks
  mech_update runs the AI mech's update. 155 of 2,573 shipped vlist entries
  have bit 5 (107 IA, 48 campaign, no player's). Tests: mw3tests c2m1 heat
  pick, c1m1 powered-down settle (the Owens on the ground, 1 cm up
  without it); the survey test takes MW3_SURVEY_MISSION. ai.md 17.5 and
  17.7, 2 discoveries, symbols.
- Found on the way: the game clock (world +time) runs on from one mission
  into the next one's load, so an AI mech's load-time update (the
  powered-down start's, new here) stamped its brain with the last
  mission's time and the c1m4 lance tests' exact counts depended on the
  test before them. The lead traced it (2026-10-25): the exe zeroes the one
  clock [0x60991c] in the mission start 0x48f400, called from the setup
  0x490ab0 at 0x490b14 long before the launch, so every load runs at t = 0.
  A port bug, fixed: the missions' writes of +0x4c (the start, the frame,
  the cleanup) set the ports' copies at once (MissionHooks::game_time_set,
  the program's mech_set_game_time); the program's two late syncs are gone
  and the tests no longer reset the clock themselves.
  missions_game_time_starts_at_zero_each_mission_headless: c1m1 for 300
  frames (18 s), then c1m4 in the process: the mechs' clock under a second
  through c4's load and agreeing with the missions' at its first frame.
- 3 of 3 breaks (heat unanswered, heat from the frame's added heat, the
  update skipped). mission_start_reads_c1m1_vehicles now expects the Owens
  on the ground (settled by the update) rather than 0.01 above.
## Item 11: the vehicles' heat and freeze glow (2026-10-25, mac 4)

- Assigned (the lead): the small version. 0x586a30 / 0x586af0 with a
  differential covering the pool running out; what 0.1 / 0.2 means for a
  zClass light; whether every path returns the light.
- Done (mac 4): `zwep::Glow` with clear 0x5869d0, glow_hit 0x586a30,
  glow_end 0x586af0 (zwep_shot.cpp); the vehicles' Block398 is it; the
  hooks take the node (+0x3b0). 0x586ba0 / 0x586c20 are the network's,
  not a step: nothing writes +0x08 but the clear and nothing reads the
  level. 0.1 / 0.2 are near / far in metres (zClass compares squared
  distances): the light reaches the vehicle's model but lights only
  what is within 0.2 m of its origin. The pool is 16 (made twice per
  mission), shared with the designator beams. Only the death returns the
  light in the exe; a vehicle alive at the end leaves it, and zClass
  refuses to free it ("ERROR deleting light", 0x4fa850): FIX (bug) in the
  program's vehicle free (program_vehicle_free). Tests: test_vehicle_glow
  (2000 cases, the pool empty at 3,272 hits), mw3tests truck glow (also
  ASan; no light error at the end); 10 of 10 breaks. Docs: vehicles.md The
  glow, fixes (a row, a range candidate), CHANGES, discoveries, symbols.

## Item 12: the camera zone (2026-10-25, mac 4)

- Assigned (the lead): first check which shipped worlds have more than
  one zone; none: document and skip.
- They do (c1, c2, c3, c4b: interiors; c4, t1 only zone 1), so ported:
  `zcls::camera_set_zone` 0x4f6c80, the program's camera_zone doing
  0x40b080's steps, the render's zone probe (ground_below). Before, no
  zone was ever culled in the program. Tests: test_camera_zone (5000),
  mw3tests c4b m6 tunnel ({1} outside, {1, 44} inside; the target, the
  anims, the probe); 7 of 8 breaks (the render list's copy is redundant
  with the frame's). Open: zWeapon's own copy of [0x74a328]. view.md 7,
  a discovery, symbols.
- Found with the full suite (missions_c4b_m5_m6 hung): c4b m5 starts over
  zone 22 alone, so the zone-filtered queries skip zone-1 nodes; the
  tests' ground_under then gave NaN, the player was put at NaN, and the
  AI's clear line toward it hung the cell walk (NaN never ends it, in the
  exe too). ground_under now asks with the zone mode off; FIX (bug): the
  walk stops at NaN (fixes.md, CHANGES); the zone test checks a NaN query
  ends. 8 of 9 breaks.
- Review (the lead): join zWeapon's copy of [0x74a328] before the merge.
  Done: `run_state().zone` is gone; `zwep::query_zone` / `set_query_zone`
  read and write `zmod::zones().list` (one copy). Readers: the tick
  (tick_zone), each shot's update and each beam (set, then the casts and
  the ground), the spawn's straight cast (reads it). Before, the
  per-shot zone reached no query. mw3tests: a PPC shot in the c4b m6
  tunnel stops on g56 (zone 44, 16 m) with the player's zones, flies
  through with {1}; 9 of 10 breaks (the private copy: both hit).


## Item 13: the landmines (2026-10-25, mac 4)

- Assigned (the lead): the load 0x4135c0, free 0x413770, check 0x413560
  wired into Mech::Update's call; whether AI mechs set them off (the
  call's ECX) and whether the radius is 2D or 3D; headless as planned;
  discoveries (where, what weapon). Branch mac4/landmines off item 12's
  branch (12 still in the lead's tests).
- Done (mac 4): `mech3/landmines` with the detonation 0x413470. Every
  mech's update checks (0x41a791, no team test): AI mechs set them off.
  3D (0x51dfa0), a NaN distance counts as near (the x87). c1 m2 82, c1 m4
  194, c2 m3 207 mines, LANDMINE (30, IMPACT_PROXIMITY 40, crater) at 20 m.
  Wired: the mission's load and free, update_calls().landmines, the ground
  0x4edab0, the HUD's mine list (its red X's). Tests: test_landmines.py
  (3000 loads and checks), mw3tests c1m2 (the player and an AI mech each
  set one off; 82 still listed; empty after the cleanup); 14 of 14 breaks.
  Docs: landmines.md, discoveries, symbols, movement-collision,
  mission-frame. The c4bm5 survey tag: the parser reads "c%dm%d", so c4b
  is "c5m5" (item 15 can take "c4b").
