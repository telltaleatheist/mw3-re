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
