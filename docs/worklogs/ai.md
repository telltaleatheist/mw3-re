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
