# Vehicles and turrets (scoping, 2026-10-21)

What c1m1 and c1m2 need, and what porting it means. Not ported yet.

## In the first missions

c1m1's `aiv.zrd` (readerm1.zbd) places, besides the player and the Owens:
three mobile field bases (`mobilehq_01..03`, kind 2, `MSG_AIV_MOBILE_FIELD_BASE`),
two trucks (`ttruck_01`, `_02`, kind 1, `MSG_VEHICLE_TRUCK`) and two APCs
(`apc_01` kind 3, `apc_02` kind 5, `MSG_VEHICLE_APC`), with regions for the
APCs' moves (reg_apcadvance, reg_apcattack, ...). The launch reaches the
mobile base's constructor 0x4589d0 three times and the basic vehicle's
0x4ac320 four times (`mission_start_reads_c1m1_vehicles_headless`); both are
logged, so these vehicles do not exist in the port's missions. c1's reader
also holds `bossturr.zrd` and `triturret.zrd` (turrets, aiTurrets.zrd).

## The basic vehicle (veh_basic.cpp region, 0x4ac320..)

- 0x4ac320: the node by name on list 6, else a copy of the type's model
  (0x4f9af0) named and put under the world ([0x608c84] vt+0); then a
  0xfc-byte object, 0x4ac530: the vehicle base 0x4aeb00 (AiTarget, ZEntity3d,
  ZEntity: the port's `vehicle_base` does it for mechs), sub-objects
  0x4adaf0 / 0x4aec00, vtable 0x5c88e8, controls (+0xe4) either given or
  made: 0x4ac860 (0xcd8 bytes zeroed) and 0x4ae270 (the AI's vehicle
  controls over 0x4ac750, the port's `ai::AiVehicleControls`), the
  vehicles' camera modes once (0x4ace00), the node attached (0x4d6c90) and
  zEntity's list (0x5a7f50).
- vt+0x158 0x49ec00: the definition's lists (weapons 0x4adb20 -> 0x4ad930,
  through the ported 0x4b4660; the animations 0x4ade40), then the controls'
  vt+0x0c (the AI's setup).
- vt+0x34 0x4a1040: the frame, from the controls (their throttle and turn
  through vt+0x1c and the IControls base): without the AI nothing moves.
- vt+0x38 0x5a80f0: the ground under it (0x4edab0) and the move.
- vt+0x160 0x4ad3f0: damage (its health, the controls told), vt+0x70
  0x4ad2d0: the death (effects 0x508c10, 0x581400, the weapons 0x4b48c0,
  the AI 0x402510).
- The killer's kill credit already reads a vehicle (on_kill's +0x40 branch,
  75 points, salvage 0x472ec0, not ported).
- About 40 functions in the vtable (most under 30 lines; 0x4d6de0 142 and
  0x4a2ef0 144 are AiTarget / AI), roughly 2,000 lines of pseudocode.

## The mobile field base (0x4589d0, mech_map / mech_objective region)

The repair stations; their own class. Their repair and the objective tie-in
are separate items.

## Why 2.1 waits for the AI (2026-10-21)

Even standing still, the basic vehicle lives through its controls (+0xe4):
when none is given, its constructor makes its own (0x4ac860 then 0x4ae270
over 0x4ac750, 0xa70 bytes: the basic vehicle controls, the same family as
`ai::AiVehicleControls`), else it hands itself to the given one (vt+0x08).
Its placement (vt+0xdc, 0x4a3630) writes the controls' +0x330 and +0x70 and
the +0x764 block's per-point table (+0x84 count, +0x14c), and calls their
IControls vt+0x14; 0x49f660 (after the setup) calls their vt+0x18 and fills
their +0x160 points from the +0x764 block's (+0x194); the setup (vt+0x158)
ends in their vt+0x0c; the frame and the damage go through vt+0x1c and the
IControls base. So the vehicle object cannot be ported apart from the AI's
vehicle controls (mac 4's `zai_brain` / `AiVehicleControls`, data side only so
far). Also open: the vehicle as a second kind of entity, since the program's
sensors, kill credit and teardown take every zEntity entry to be a Mech
(app_sensors `mech()`, `mech_free`, killer_notify): a kind tag or a common
base is needed first.

## A split

1. The basic vehicle as an object: construction, placement (vt+0xdc), the
   definition's weapons, its model node and touch handler, damage and
   death, the kill credit's salvage 0x472ec0. It stands where placed.
2. Its frame with the controls (the AI's; on hold).
3. The mobile field base, then the turrets (turret.cpp, 903 lines).

## Telling a mech from another entity (2026-10-21)

zEntity's list holds every entity, mechs and vehicles alike. The exe tells
them apart through the entity's own vtable: vt+0x2c, then vt+0x50 returns the
mech (a mech returns itself; the basic vehicle, vtable 0x5c88e8, returns 0).
The program keeps no vtables, so `app_mech.cpp` keeps the same answer as a
registry: `entity_register` adds each mech the program builds, its free
removes it, `missions_start` clears it, and `entity_is_mech` /
`entity_mech(handle)` ask it. Every place that treated a list entry as a mech
(sensors' `mech()`, `mech_free`, `killer_notify`, contacts, the player's
target, the update loop, debrief and salvage, `describe_mech`) asks first and
skips anything else, so a vehicle can join the list without being read as a
mech. The update loop logs a skipped entity as the unported "entity vt+0x34"
until vehicles have their own update. `test_program`'s c1m1 death run puts a
stranger entry in the list and checks it is skipped (treating every entry
as a mech crashes that run).
