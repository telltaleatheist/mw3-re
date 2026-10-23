# Vehicles and turrets (scoping, 2026-10-21)

What c1m1 and c1m2 need, and what porting it means. Being ported (the
steps: docs/worklogs/ai.md, item 4).

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

- 0x4ac320 (ECX the controls, EDX the type, the name on the stack): the
  node by name on list 6, else a copy of the type's model (0x4f9af0(model,
  0, 0)) named and put under the world ([0x608c84] vt+0); then a 0xfc-byte
  object, 0x4ac530(controls, node): the vehicle base 0x4aeb00 (AiTarget,
  ZEntity3d, ZEntity), the engine sound at +0xe8 (0x4adaf0, vtable
  0x5c8b90; the setup puts the definition's sounds/engine there from the
  vehicles' shared cache 0x60b110, step 1b) and the motion animation at
  +0xf0 (0x4aec00 zeroes it; the linker shares that body with IControls'
  hit record; the setup's motion_anim),
  vtable 0x5c88e8, the controls (+0xe4) either given (their SetOwner,
  IControls vt+0x08) or made: 0x4ac860(0xa70) and 0x4ae270, the vehicle's
  own basic controls over 0x4ac750 (vtable 0x5c8c50, step 2a; the AI's are
  the ones given), the vehicles' camera modes once (0x4ace00), the node
  attached (0x4d6c90) and zEntity's list (0x5a7f50).
- The destructor 0x4ac650 (0x4ac630 deletes): the engine sound dropped, the
  controls deleted (IControls vt+0x00), the node detached (0x583400), taken
  from the world (0x4ebb40), freed (0x4f9050) and unlisted; the motion
  animation stopped (0x4addb0 -> 0x4ade70 -> 0x506ca0); then the base 0x4aeb50 (the
  weapons 0x4b01d0, the network's record), AiTarget's 0x4d58a0 (the
  contacts, the shadow) and ZEntity3d's 0x5a7f90, which unlists the entity
  again whether it is listed or not, and ZEntity's 0x5a8370.
- The HUD markers 0x4ac3d0: three zTwoD images, `hud_veh_ally_sm`,
  `hud_veh_enemy_sm`, `hud_veh_other_sm`, at [0x60b120] / [0x60b124] /
  [0x60b128].
- Ported (steps 1a and 1b, 2026-10-23): all of the above and below to the
  motion animation, `mech3/veh_basic_object`
  (`BasicVehicle` over the shared `Vehicle` base, which `Mech` now derives
  from too), checked by `test_veh_basic.py`. 0x4ace00 stays a hook (the
  camera modes for viewing a vehicle); the engine sounds' clear runs at each
  mission's end. Not in play yet: the program makes no vehicles until their
  placement (step 1c).
- vt+0xdc 0x4a3630(pos, yaw), the placement (ported in step 1c1): the
  ground under the point from 10 above it (0x4edab0), the vehicle 0.1 over
  the first polygon found (not the highest, as the mechs' pick does), else
  where given; the vehicle's own vt+0x18 (zEntity3d's 0x5a8550: the node's
  translation and 0x5a85c0); the controls' y angle (+0x330) the yaw, their
  commands cleared (IControls vt+0x14 0x4ad640: throttle +0x08 and turn
  +0x0c to 0; the jump jets' setters are empty for vehicles), the owner
  refreshed, +0x70 zeroed, the +0x764 block's words at +0x8b0 (count
  +0x7e8) zeroed, +0x20 and +0x24 zeroed, and the points at +0x160 (count
  +0x214) set to the place, but only on controls with a serial: the
  placement runs before the setup's init gives them one, so on a vehicle's
  first placement they stay as they are (0).
- vt+0x158 0x49ec00(def, name, kind): the definition's `sounds` list's
  `engine` (0x4adb20 -> 0x4ad930, the engine-sound cache; not weapons, as
  first read) and its `motion_anim` (0x4ade40), then the controls'
  vt+0x0c (the AI's setup).
- The engine sounds (CZEngineSoundList, the global at 0x60b110, ported in
  step 1b): made at start-up (static initializer 0x4ada90 -> 0x4ad840, room
  for 64 entries of 0xc bytes, never grown; cleared at exit 0x4ade90) and
  emptied at each mission's end (0x491780 -> 0x4d71f0 -> 0x4add10, each
  entry's voice unlocked and stopped, 0x4adbf0). An entry (0x4adb90 /
  0x4adbb0) is the sound by name (0x56f6f0) and the voice playing it (+8,
  set by the frame, step 2): every vehicle with the same engine shares one
  entry. A lookup (0x4ad930) compares names (0x56f740); a sound zSound does
  not have gives none; a 65th distinct sound gives none and the report
  "CZEngineSoundList is too small" (0x49bfd0, empty in this build).
- The motion animation (0x4ade40): the running one (+0xf4) stopped, then
  the definition's by name (0x50b450) at +0xf0, or none.
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

## The vehicles' controls base (0x4ac750, ported in step 2a1)

The AI's vehicle controls (0x4e71c0 over 0x4e7090) and the vehicle's own
basic ones (0x4ae270) sit on one base, `mech3::VehicleControls`
(`mech3/veh_controls`; `ai::AiVehicleControls` derives from it). Its
constructor sets only the turret limits; its users fill it:

- The init (vt+0x10, 0x4ac8a0), with the owner and its node: a serial
  number (+0x14, from the counter [0x60b148]), the node's rotation (+0x32c,
  0x4fbe90; a copy at +0x320) turned into the owner's matrix (0x4ab990,
  zMath's euler rotation) and its second row kept (+0x308), the owner
  refreshed (0x5a85c0), the nodes `body`, `turret`, `shadow`, `gun` found
  in the tree (+0x700, +0x704, +0x70c, +0x708), the gun's world position
  (+0x6d8), then the gun's firepoints: `fpnt_c` (its marker model dropped),
  `fpnt_l`, `fpnt_r` (removed and freed once read); the health 100 (+0x740,
  +0x744).
- The setup (vt+0x0c, 0x4acbc0, the end of the vehicle's 0x49ec00): the init
  (through the vtable, so the basic controls' 0x4ae3c0 runs there), each
  `start_anims` animation started on the owner's node (0x508c10), the
  definition's `health`, the owner's 0x4af2a0 (step 2a1b) and its weapons
  (0x4b4660, the port's `build_vehicle_weapons`), the owner's node at +0x3b0,
  the block at +0x398 cleared (0x5869d0).
- vt+0x18 0x4accd0: +0x1c the gravity ([0x609a14]), +0x30, +0x3c, +0x350,
  +0x5e8 zero.

- The weapon slots (the owner's 0x4af2a0, ported in step 2a1b): ten of
  0x38 bytes from +0x3b8, emptied, then one per `weapons` item (name n n
  ammo ...; the same list `build_vehicle_weapons` reads): zWeapon's
  definition by the name (0x581ed0), bit 1 paired (the item's third value),
  bit 2 counted (its second value, when the definition exists) with the
  fourth value as a float at +0x18, the fifth to seventh values kept as
  they are, bit 0 from the eighth. Under the gun, its firepoint node is
  named after the weapon, or `<name>_L` and `<name>_R` when paired; each is
  hidden (0x4eb170), the first slot's shown again at the end (+0x3b4 points
  at it). A beam weapon gets its beam (0x584a70) with the muzzle +0x608 and
  direction +0x5f0, which the beam reads live, and 8 segments on the
  controls whose serial is 1, 1 on every other: the serial counts every
  vehicle controls set up since the program started ([0x60b148] is never
  reset). No shipped vehicle carries a beam (their weapons are AC2, MGUN,
  SRM2), so it never shows. FIX (bug): a weapon zWeapon lacks crashes the
  original; here the slot stays empty (fixes.md).

Open: a firepoint the gun has lands in one set of fields (fpnt_c +0x62c with
the inverse of its length at +0x724, fpnt_r +0x638, fpnt_l +0x644), the
starting value for one it lacks in another (+0x608 (0, 0, -1), +0x614
(1.51, 0.396, -2.46), +0x620 (-1.51, 0.396, -2.46)). +0x608 is the muzzle a
beam reads live, so the found positions are likely the local offsets the
aiming turns into it each frame; settled with the frame (step 2c).

## The motion block (the controls' +0x764, ported in step 1c2a)

`mech3::VehicleMotion`, 0x2a0 bytes to the base's end, read from
vehicle.zrd's mode by 0x49ef40 (0x49ec90 calls it on the controls' block
with the vehicle's node, the mode, the vehicle's name and type and the
controls' +0x214 for the collision count). Defaults in brackets:

- The model's nodes `right_morphs`, `left_morphs`, `chassis`, `rtracks`,
  `ltracks` (+0x288..+0x298).
- The supports: the model's `support00..03` (hidden), else the `platform`
  list's points ("Cannot find support points for %s!"); +0x84 their count,
  +0x8c the points (16 fit), +0x14c a word each.
- The collision points: the model's `collide00..11` (hidden, kept in the
  order 0-2, 6-8, 3-5, 9-11), else the `collision` list's, whose count
  goes to the controls' +0x214 ("Cannot find collision points for %s!").
- `rates` (10, 30), `friction` (10000, 10, 0; the second held under 0.9 of
  the first), `stopping` (8), `quicksand_slowdown` (0.9), `lava_slowdown`
  (0.8), `turns` (0.6, 2), `turn_damping` (30), `rate_damping` (30, 30),
  `a_damping` (8), `mass` (1; its inverse at +0x24c), `gun_pitch` (-15
  degrees, 0.5: the gun's limits rebuilt with them, 0x4a4b30 picking the
  full-turn class 0x4a4a70 for the -2 pi..2 pi it is given), `mode` by its
  first two letters (basic 0, fly 1, sub 2, track 3, hover 4, amphib 5;
  track without the key, an unknown name keeps the mode it had), the wave
  values (`amphib_wave`, then `hover_wave`, then `sub_wave`: the last given
  wins, whatever the mode), `mode_alt` (2), `chas_smooth`, `chas_pitch`,
  `chas_roll` (0; their last value as its absolute), `collision_d` (0.5,
  0.15), the destroy animation by the vehicle's name, else its type ("No
  destroy animation for (%s) or (%s)").
- The limiter classes 0x4a4b30 chooses from (by the yaw range's width):
  0x4a42f0 (none: a fixed yaw), 0x4a4580 (under pi), 0x4a4800 (under 2 pi),
  0x4a4a70 (a full turn); only the last is ported (the turrets need the
  others).
- Quirks: without `chas_roll` the exe zeroes `chas_pitch`'s values (a
  copy's slip, FIX (bug) behind zfix bugs); without `platform` or
  `collision` (and the model's points) it reads through null (FIX (bug)).
  No shipped vehicle (MW3 or Pirate's Moon) has any `chas_*` or `*_wave`
  key, and every one has both lists.
- Open: with the model's twelve collide points the count is not written,
  so it stays what the controls had (0 from the zeroed block). Whether any
  shipped vehicle model has them (which would leave it no collision points)
  is to check against the models.

## The speeds and turns (vt+0x114..+0x13c, ported in step 1c2b)

What the AI asks a vehicle (the same slots the mechs answer, mech.h), from
the motion block: the top speed is `rates`' second value, the slowing
`rate_damping`'s second, the turn rate `turns`' second.

| Slot | Function | Gives |
|---|---|---|
| +0x114 | 0x4acfa0 | the throttle (IControls GetThrottle, the controls' +0x08) x top speed |
| +0x118 | 0x4acf70 | share x top speed |
| +0x11c | 0x4acfe0 | speed / top speed (0 for a top speed of 0 or NaN) |
| +0x120 | 0x4ad020 | speed / slowing: the stopping distance |
| +0x124 | 0x4ad050 | the stopping distance at the throttle's speed |
| +0x128 | 0x4ad0b0 | the stopping distance at a share's speed |
| +0x12c | 0x4ad100 | slowing x d / top speed, clamped to 0..1 (NaN 0): the share that stops within d |
| +0x130 | 0x4ad180 | angle / turn rate (at least 0.0001; a second argument unread) |
| +0x134 | 0x4ad250 | the turn time of the controls' \|+0x78\| |
| +0x138 | 0x4ad290 | the turn time of a share's speed |
| +0x13c | 0x4ad1c0 | \|turn rate x b\| x a / top speed, clamped to 0..1 |

Without controls the exe reads through null in +0x120..+0x128 (no vehicle
is without them); the port gives 0. vt+0x108..+0x110 (0x4acd90, 0x4acdc0,
0x4acde0) are the vehicle cameras' (with 0x4ace00), still hooks.

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

## A mobile field base lost (0x004590b0, found 2026-10-21)

Run when a field base dies (it starts with the basic vehicle's death
0x004ad2d0). In a campaign (0x00479440) the campaign's +0xa8, the field
bases, goes down by one: +0xa8 is the base count (3 at the start, reset to 3
after Op 1 Mission 1 and Op 4 Mission 4), and each base is the 300 t and 2
mechs of room the briefing's split allows (campaign.md section 9). With
bases left, what the player carries (the game's store +0x08) is cut on the
spot to a fraction: 0.7 with two bases left, 0.5 with one (1.0 otherwise):
- mechs: the store's mechs counted (up to 64); if at least as many as the
  bases, random ones are removed until ceil(fraction x count), capped at 2
  per base, remain; a message 0xf05 for each, 0xf07 once;
- the rest by weight: random items taken out until the store's tons are
  fraction x tons, capped at 300 t per base, each moved to the campaign's
  +0x78 (vt+0) and removed from the store (vt+0x14); message 0xf06 with the
  tons lost ("%f").
  The +0x78 store (`CampaignGame::store78`) comes back only on a replay:
  0x00434cb0 adds it (and +0x50) back to the store and resets +0xa8 to
  +0xac on the game it is called on. When the mission fails or is aborted
  that is the game the player replays from, so the gear returns. At the next
  mission's game, though, 0x00434e00 first copies the old +0x08 store and the
  old +0xa8 into the new game (434e89..434e9a), then calls 0x00434cb0 on the
  old one: the new game keeps the cut store and the reduced base count. On a
  win, what a lost base cut is gone for good (campaign.md section 9).
With no base left nothing is cut here. Not ported (the field base waits on
the AI vehicle controls). No sign in 0x00458xxx..0x0045bxxx of repairs
looking at the mechs carried: the store is touched only here.
