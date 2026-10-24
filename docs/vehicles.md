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
  from too), checked by `test_veh_basic.py`. The camera modes (0x4ace00)
  followed with the cameras (below); the engine sounds' clear runs at each
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
is without them); the port gives 0.

## The cameras (0x4ace00, vt+0x108..+0x110, ported 2026-10-23)

Every vehicle shares one camera list, the static at 0x60b130 (freed at exit
by 0x4ac300), filled once by 0x4ace00 from the first vehicle's constructor
(the flag [0x5e8578] starts at 1). Its three modes are the plain base
(0x40a950, 0x44 bytes, base vtable 0x5bc2c0), none of the mechs' classes, and
no definition block is read:

| id | dist | rotation | desired_pos | dist_vary_factor | otherwise |
|---|---|---|---|---|---|
| 0 behind | 15 (base) | pitch -0.06 | (0, 0.43, 1), not normalised | 1 | the base's |
| 1 on it | 1 | identity | (0, 1, 0) | 0 | the base's |
| 3 above | 150 | pitch -pi/2 | (0, 1, 0) | 0 | dist_vary 0, dist_factor 0 |

There is no mode 2. The list's current id (+0x10, [0x60b140]) is written only
by its static constructor, so vt+0x110 (0x4acdc0) and the view attach vt+0x108
(0x4acd90: 0x40adf0 on the view [0x5f4c8c] with the vehicle and that mode)
always give mode 0; vt+0x10c (0x4acde0) finds a mode by id. vt+0x108 is the
slot the mission launch (0x491150) calls on the player's entity; MW3's player
is always a mech, so a vehicle's view would come only from elsewhere.
`mech3/veh_basic_object` ports all four (`vehicle_cameras_make`,
`vehicle_camera`, `vehicle_camera_current`, `vehicle_view_attach`, the last
through the hooks: the program's view takes a mech), checked by
`test_veh_basic.py` at both x87 precisions (9 breaks; 8 caught, the ninth an
ulp inside the tolerance).

## Damage and death (step 1d, ported 2026-10-23)

zWeapon reaches a vehicle through the hit callback 0x4af550, registered for
every vehicle the mission start makes (0x49f8a0: 0x5833b0(node, 1,
0x4af550)): the amount doubled at difficulty 0 (low), halved at 2 (high),
then vt+0x160 with (weapon, the callback's first argument, amount). The
basic vehicle's slots (vtable 0x5c88e8; the derived class at 0x5c23d8 has
its own death and damage):

- **vt+0x160 0x4ad3f0, the damage.** Controls whose vt+0x1c says destroyed
  (the AI's 0x4e7470: the owner's +0x08 clear; the basic 0xa70 controls'
  0x4ae6a0: state +0x38 is 4; the base's 0x4a5270 always) only clear the
  vehicle's +0x08 and its engine sound. A weapon with def flag bit 22
  (`FREEZE` or `IMPACT HEAT` present, structs.md; every laser has impact
  heat) goes to the glow block (+0x398, zWeapon's 0x586a30): a freezing
  one first with amount x 0.01, and when that answers 2 (the block's +0x08
  below -0.5) the controls' +0x28 is set and nothing more happens; a heat
  one takes the amount off the health (+0x744). Both then pass 1 - health /
  max (+0x740) to the glow. Any other hit takes the amount, or, once +0x28
  is set, amount + +0x08 x health (+0x08 is negative while frozen, so such
  hits add health as written; no shipped weapon freezes, so this never
  happens). Health not above 0 (a NaN counts) kills: with a weapon, the
  attacker's object (0x581e40, 0x40eee0 asked twice) gets vt+0x80(vehicle)
  as the kill credit, 0x583710(1, 0), vt+0x70, answer 0; without one,
  vt+0x70 and answer 1. Alive, 0x583730(health / max), then a hit over 5 by
  a weapon without bit 22 jolts it: k = inv_mass (+0x764+0x24c) x amount,
  0x4accf0(normalised from - to of the shot line 0x581e30, k x 0.025,
  k x 1.667).
- **0x4accf0, the jolt.** The direction into the node's frame (the node's
  local 3x4 through the matrix stack, 0x522640 by the inverse rotation);
  angles.x (+0x32c) -= z x turn, angles.z (+0x334) += x x turn, and +0x70 x
  and z -= x and z x push.
- **vt+0x70 0x4ad2d0, the death,** once (+0x08 set): +0x08 and the engine
  sound cleared, health 0, state 4, IControls vt+0xa8 (the AI's
  0x4ea6b0), 0x402510 (the basic controls' path: entries with a negative
  id dropped; vt+0x00 gives the path controls, null for the others), the
  destroy animation (+0x764+0x29c) on the node, a beam that is on (+0x394)
  stopped (the first slot's +0x34, 0x581400), the glow ended (0x586af0),
  the weapons freed (0x4b48c0: 0x4b1110, then deleted) and 0x583710(1, 0).
- **vt+0x68 0x4ad390, TARGET's tint:** on, the node's object3d colour
  blend (0x4fb8c0) red (0x5e8580) at (1 - health / max) x 0.6; then the
  blend's switch (0x4fb840) on or off.

The controls' health fields were named the wrong way round until this
step (+0x740 is the maximum, +0x744 the health); the setup writes both
alike, so nothing changed in play. `mech3/veh_basic_object` ports all six
(`vehicle_damage`, `vehicle_jolt`, `vehicle_die`, `vehicle_weapons_free`,
`vehicle_hit`, `vehicle_view_tint`); the controls' answers, the glow and
zWeapon's hit state are hooks. `test_veh_damage.py`: 600 runs at 24 bits
against the real vtable and matrix stack, 17 breaks caught. Salvage
(0x472ec0) was already ported (`mech_salvage`).

## The debug readout (vt+0xe0 0x4a2ef0, step 1e, ported 2026-10-23)

The slot the mechs use for the HUD's frame (0x41cd40, the view target's)
is a developer readout on a vehicle: the HUD's text lines 1 to 4
([0x5fc848] + 0x4b0 n, each set through its vt+0x7c, a printf with the
text as the format):

1. With the controls' vt+0x20 at 1, "%s using %s dynamics" (" - A" when the
   controls' +0x20 is set) with the node's name and the motion mode's name
   (BASIC, FLY, SUB, TRACK, HOVER, AMPHIB, else UNKNOWN); else, on a path
   (IControls vt+0x70 at 1), "%s is in mode %d and had goal node %d"
   (vt+0x04, vt+0x08); else, destroyed (vt+0x1c at 1), "%s is DEAD!"; else
   empty. Every MW3 controls answers 0 to vt+0x20 (0x4d72a0), so the first
   never shows.
2. "POS %d %d %d YAW %d": zEntity's position (0x5a85e0, asked three times,
   z first) and the angles' y in degrees, through _ftol (truncated through
   64 bits, the low word).
3. "VEH ZONES(%d): " and the vehicle's zones: +0x64 is a 4-byte zone record
   (a count, then up to three ids, 0x4f6c80's), each as "%d ".
4. "CAM ZONES(%d): " likewise from the view's camera (its data +0x1e4).

Only a vehicle the view is on reaches it, which MW3's player never is.
`vehicle_debug_lines` in `mech3/veh_basic_object` (the lines through the
hooks); `test_veh_damage.py` runs it 200 times with MFC's CString modelled
(8 breaks caught). The AI slot 0x4d6de0 (vt+0x84, the lead point) was
ported earlier (`zai_firing`).

## The gun's aim (step 2b1, ported 2026-10-23)

Each vehicle controls has pitch limits at +0x790 (the pointer at +0x78c;
vtable 0x5c8340 over the base 0x5c82f0, 0x4a4000/0x4a4a70): the sines and
cosines of `gun_pitch`'s two values (defaults -0.2588 and 0.5 radians,
which every shipped vehicle keeps: none sets the key).

- **within** (vt+0x08, 0x4a4180): the direction's y not past either sine.
  Both compares are "not greater", so a NaN is within.
- **clamp** (vt+0x04, 0x4a41b0): within, the direction itself and true;
  else the limit passed (the low one first; a NaN counts as below) and
  false, with out = (x f, sin, z f), f = cos / hypot(x, y), or (0, sin,
  -cos) when that length is 0. The heading's length is hypot(x, z): the
  exe's y is a typo (FIX (bug) behind `bugs`; fixes.md). With it the
  clamped direction misses the limit (for one dead ahead: 24.6 degrees for
  a target 40 up, 43 for 60 up; off to the side it differs, and straight up
  gives (0, sin, 0)). It shows nowhere: the only vehicle caller, the AI's
  track 0x4b6e30, keeps the clamp's answer only for a dot product it never
  reads and aims (vt+0x154) the unclamped direction (zai_firing).
- The vehicle's slots: vt+0x88 / vt+0x90 (0x4ad740, 0x4ad7c0) ask a
  direction in the vehicle's frame, vt+0x8c / vt+0x94 (0x4ad780 / 0x4a4060,
  0x4ad7f0 / 0x4a40f0) a world one turned into it by the rows of the
  vehicle's matrix (+0x0c); vt+0x98 (0x4ad820) is health / max; vt+0x150
  and vt+0x154 (0x4ad6e0, 0x4ad700) pass to the controls' vt+0x24 and
  vt+0x28 (no controls: false, nothing). The base controls answer false and
  do nothing (0x4d6dc0, 0x48b940).

The AI controls' turret (their +0xcb0 aim and +0xcbc flag):

- **aim at** (vt+0x28, 0x4e7570): a direction in the vehicle's frame (the
  track turns it so by rows, 0x4b6ef3) back into the world by the
  matrix's columns, flag set; the second argument unread.
- **can aim** (vt+0x24, 0x4e7490): no turret node (+0x704), true; else the
  aim in the turret's frame (by the rows of its cached world matrix,
  object3d data +0x60; render.md), flattened
  and normalised (0x4020d0); a zero or NaN length false; else -z > the
  argument (strictly).
- **turn** (0x4e72f0, the +0xa04 base's vt+0x04 and IControls vt+0xbc,
  answering 1): the aim (else the owner's +0x18, vt+0x10) in the turret's
  frame as above; its heading atan2(-x, -z) past 0.001 turns the turret
  node about y (0x4fbef0) by (1 - f) of it, f = e^(-5.545 dt): the rate is
  -ln(0.5) / 0.125 (half the way every 0.125 s, made once at [0x60b880]),
  and the exponential is the float-bits trick ftol(-k dt x 12102200) +
  0x3f800000.

`TurretLimits` and `turret_*` in `mech3/veh_controls`, the slots in
`mech3/veh_basic_object`, `AiVehicleControls` (zai_brain) holding the aim;
`test_veh_aim.py`: 600 runs against the real vtables, 14 breaks, 13 caught
(the fourteenth is equivalent: a NaN never reaches the high limit's test).

## The AI vehicle controls' slots (step 2b2, ported 2026-10-23)

The AI vehicle controls (0x4e71c0, 0xcd8 bytes) answer the brain through
the same IControls slots as the mechs' AI controls, most by thunks that
subtract the vtordisp and 0x2bc to reach the vehicle controls base at
+0xa08 (so `ecx - 0xa00` is +0x08), the rest by `ecx - 0x10` into the
brain at +0xa04:

| IControls | function | does |
|---|---|---|
| +0x0c | 0x4e71a0 -> 0x4e9710 | the brain's frame |
| +0x10 | 0x4ad720 | the fire flag (+0x390) |
| +0x14 | 0x4ad640 | the commands cleared |
| +0x18 / +0x1c / +0x24 / +0x30 | 0x4a5b20 / 0x4a5b50 / 0x4a5b90 / 0x4aed90 | throttle < 0, throttle, turn, turn (the goal is the turn itself) |
| +0x20 / +0x28 | 0x4a5b70 / 0x4a5bb0 | throttle, turn set |
| +0x34 / +0x3c / +0x80 | 0x4a5bc0 / 0x4a5bc0 / 0x40bae0 | nothing (no jets; no scan aim) |
| +0x70 | 0x4e7790 -> 0x4a5270 | on a path: always |
| +0x94 / +0x98 / +0xa4 / +0xb4 | 0x4ea520 / 0x4ea500 / 0x4e94f0 / 0x4ea630 | the brain's hit, ally hit, tick, kill (the mechs' too) |
| +0xa0 | 0x4aed40 | hit within a second (the hit record +0xcc8) |
| +0xa8 | 0x4e7800 -> 0x4ea6b0 | the death: the tick, then the radio's ejecting line (no triggers handed on, unlike the mechs' 0x4d8a00) |

The primary vtable (0x5cd880): +0x1c destroyed 0x4e7470 (the owner's +0x08
clear), +0x20 0 (0x4d72a0), +0x04 / +0x08 the path's mode and goal (0, -1:
0x40bae0, 0x4a5260), +0x24 / +0x28 the turret (2b1). The brain's smoothing
(+0xa04 vt+0x04) is the turret's turn 0x4e72f0. The base controls' answers
are the defaults of `VehicleControls`' virtuals (destroyed: yes, 0x4a5270).
`AiVehicleControls` (zai_brain) implements the brain's `BrainControls`;
`test_ai_brain.py` calls the original's slots on its 690 vehicle cases and
compares. 0x4e95b0 (+0x84), 0x4d99e0 (+0x88), 0x4ea700 (+0x60) and 0x4d1e60
(+0x54) are shared with the mechs and answered where they are called.

## The setup's points and zone (0x49f660, 0x49f710, ported 2026-10-23)

After the motion block, the mission start (0x49f8a0) calls 0x49f660 on the
vehicle: the controls' vt+0x18 (0x4accd0, the gravity word), then each of
the controls' points (+0x160, count +0x214) is the motion's collision point
(+0x764+0x194) off the entity's position (0x5a85e0, asked each time). Then
0x49f710 finds its zone: the node's flag 8 saved and cleared (so the query
misses the vehicle itself), the ground under its x and z from 500 up
(0x4edab0), the flag put back, the pick 0x4abd90 (step 4, water not
ground); when the query found ground, the picked polygon's 4-byte zone
record becomes the vehicle's +0x64 (zEntity's 0x5a80d0) and the node's zone
byte (0x5a80c0 -> 0x4eb810) the first id the record shares with the view's
camera (0x53cd20: 0xff with zones off, either list empty or an 0xff met),
else the record's first id. `vehicle_points_place` and
`vehicle_zone_from_ground` (veh_basic_object), `zmod::zone_common`
(gmod_cull); `test_veh_setup.py`, 600 runs, 12 breaks caught. zEntity's
per-frame zone update 0x5a80f0 (vt+0x38) is left for the frame.

## The movement (step 2c, in progress)

The vehicle's frame (vt+0x34 0x4a1040, from the entity manager's walk) runs
the controls' IControls vt+0x0c (the AI's: the brain's frame); destroyed
controls stop the engine sound and the motion animation; on a path, the
movement 0x4a1240: by the motion's mode, BASIC 0x4a1d90 or TRACK 0x4a12c0,
nothing for FLY, SUB, HOVER and AMPHIB (every shipped MW3 vehicle but the
stealth object is TRACK; no vehicle file uses the others), then the
orientation normalised and the controls' world and local velocities copied
to the entity (+0x30, +0x3c). The pieces ported so far (2c1, `mech3/veh_move`):

- **The turn rate** (the controls' vt+0x14): the base's 0x4a2a00 adds the
  turn x turns[0] x dt (none: damped by turn_damping; against the turn:
  first stopped) held to +-top_turn (+0x8c); the AI's 0x4a2ae0 sets it to
  the turn x top_turn outright.
- **Turning to a heading** (0x4a28c0, while +0x760): a turn command cancels
  it; a full turns[1] x dt a frame while the heading is further than that,
  then the yaw (+0x330) put on it.
- **The speed** (0x4a2b10): the local z speed (+0x78) less throttle x
  rates[0] x dt, damped by rate_damping[1] with no throttle or against it,
  held to |throttle| x top_speed (+0x88); the side slip (+0x70) damped by
  rate_damping[0] and dropped under 0.01.
- **Sliding** (+0x34; 0x4a2dd0 instead): the world velocity (+0x64) turned
  into the local one, the speed as above but held to top_speed itself, and
  the side force (0x4a2c90) applied: the slope's (the matrix's [1] x -60)
  plus the turn's (-z speed x (forward x +0x314) / dt); standing, a force
  past friction[0] starts a slide less friction[0]; sliding, less
  friction[1] against the slip; the slide ends when the slip turns over.
- The x87 compares: "below" tests pass NaNs and "above" ones do not, so a
  NaN speed clamps to the low bound and a NaN slip counts as none.

`test_veh_move.py`: 1200 runs through the AI controls' real thunks, 14
breaks, 13 caught (the heading's exact-equality case is never drawn).

## The suspension (TRACK, 0x4a1760, step 2c2, ported 2026-10-23)

A track vehicle rides on its four supports (`platform`, the motion's
support[0..3]; their world places at support[8..11]); support_word[0..7]
are the supports' vertical speeds (floats), [8..15] their in-air flags.

- **The ground** (0x4a23f0): the supports' world places, one query for the
  four from 500 up (0x4ee2f0, the vehicle's node left out by its flag 8),
  each support's ground the pick (0x4abd90, step 4, water not ground) and
  its slope the world velocity against the polygon's normal (-(vx nx + vz
  nz)); support 0's polygon gives the zone (as the setup's). A support with
  no ground at all: nothing more this frame (answer 0). Each support's sum
  of downhill pushes on polygons steeper than [0x5e812c] (0x4a2770) goes
  into a local the exe never reads.
- **The top speeds**: rates[1] and turns[1], times quicksand_slowdown with
  two supports counted in quicksand, else lava_slowdown with two in lava.
  The count is by the pick's water flag, which is only 0 or 1, so the
  quicksand (3) and lava (4) counts never rise: vehicles are never slowed
  (and the 0.3 sink of such supports never happens). A candidate for Owen
  (fixes.md).
- **The supports**: the lowest-ground support is left as it is; each other
  one steps (0x4a2820: gravity x1c, falling while above its ground, landing
  with the slope for its speed, else following the ground); in the air when
  all three are (+0x20).
- **Pitch and roll rates** (+0x50, +0x58): from the other support with the
  least vertical speed and the next two round the four (skipping the
  lowest), solved as the plane through their speeds; all in the air, back
  toward level instead ((pitch + 0.5236) x -0.7, no lower than -0.7; roll x
  -0.7); smoothed by a_damping (e^(-a_damping dt)); the three supports'
  speeds then set from the rates. The chosen support's speed is the
  vehicle's vertical speed (+0x68, +0x74).
- **The attitude**: up the plane through the three non-lowest supports
  (0x524020), the position put on it (0x4abf70) and set (vt+0x18), the
  forward tilted onto the up (0x4a3e40), the matrix rebuilt (0x4a3ed0:
  forward x up, up, -forward, the position), pitch and roll from the up
  turned by -yaw (0x523e70): asin(z), asin(-x). All in the air, each held
  to +-0.5236 (30 degrees), its rate and any rising support speed zeroed.

The exe leaves two picks unset in impossible cases (no support under 5000,
every speed past 10000); the port starts them at the first. `vehicle_suspension`
and its pieces in `mech3/veh_move`; `test_veh_suspension.py`, 600 runs with
the original's own helpers (the pick, exp_neg, asin through a stand-in for
the CRT's, the normal, the turn), 23 breaks, 22 caught (the water surface
feeds only the dead quicksand branch).

## The frame (vt+0x34 0x4a1040, step 2c3, ported 2026-10-23)

- **The frame** (0x4a1040, the entity manager's per-entity vt+0x34): the
  controls' frame (IControls vt+0x0c; the AI's is the brain's); destroyed
  controls make the vehicle inactive, its engine sound and motion animation
  off. When the frame answered 1 and the controls are on a path (the AI's
  always): the movement, the motion animation, the player's mech's lock
  position kept at +0x6f0, the weapons (0x4b4880: the trigger is fire ==
  1, then the manager's update), zEntity's refresh.
- **The motion animation** (0x4addc0, `motion_anim` at +0xf0): it runs
  while the throttle is past 0.9 either way, and stops the first frame it
  is not (its hold time, [0x60b15c], is never set).
- **The movement** (0x4a1240): nothing for a frozen vehicle (+0x28); BASIC
  0x4a1d90 or TRACK 0x4a12c0, then the orientation (+0x48) the turn from
  (0, 0, -1) onto the forward (0x525a50) and the controls' velocities
  copied to the entity.
- **TRACK's step** (0x4a12c0): the heading's turn, the turn rate (vt+0x14;
  all in the air: damped by e^-dt), the yaw kept within +-2 pi, angles0
  kept, the matrix from the angles (0x4ab990), the up, the speed (in the
  air: both damped by e^(-dt/2)), the world velocity (-forward x z + its
  side x x) and the position moved by it (vt+0x18), the collision points
  placed (+0xac), the collisions 0x49ff80 when the controls' vt+0x20 is 0
  (every MW3 controls), BASIC's ground 0x4a1f00 or the suspension, the
  shadow shown or hidden once two frames agree (on the ground, in the
  air), the node's rotation and matrix, the points again (and their copies
  at +0x160), the matrix and forward kept (+0x278, +0x314), and the tracks'
  textures scrolled by (turn rate x 2.25 -+ speed) x 1.72.

`vehicle_frame`, `vehicle_move`, `vehicle_track_step`,
`vehicle_motion_anim_step`, `vehicle_weapons_step` in `mech3/veh_move`;
`test_veh_frame.py`: 600 track steps and moves on test_veh_suspension.py's
vehicles with the base controls' real slots, and 300 frames, 24 breaks
caught. The collisions (0x49ff80) followed; BASIC's two steps (0x4a1d90,
0x4a1f00, the stealth object's) wait for the end (The stealth object).

## The collisions (0x49ff80, step 2c4, ported 2026-10-23)

Each TRACK step, unless the controls' vt+0x20 says otherwise (none in MW3),
sweeps collision points from last frame's places (+0x160) to this frame's
(+0xac):

- **Which** (0x4a0180): turning more than moving (|speed| below |turn rate
  x 3.3|) the corners 0, 2, 3, 4, 5; moving (speed squared past 0.01) the
  front 0, 1, 2 or, going back, 3 and 5, with 4; sliding more than 1 a side
  the right's 6..8 or the left's 9..11 (a NaN slip counts as left), those
  placed afresh by this and last frame's matrices.
- **The query** (0x4a0910): one world segment query for all, the vehicle's
  node left out (its INTERSECT flag off, then on); each crossing sorted
  (0x4a09e0): one on an object (its node's object, 0x40eee0) to objects;
  else a normal y below -0.9 a ceiling, below 0.71 a wall; a floor of soil
  5 (fire) sets +0x2c. The exe then adds that floor to a list that is the
  segment's own start on its first crossing (a pointer written into its x)
  and null after (a crash): FIX (bug), the flag only. +0x36c is 1 when
  nothing was sorted.
- **The responses** (0x49ff80), each by its list's head (the last added):
  objects and walls 0x4a0b40, ceilings 0x4a0f50 (rising speeds turned
  down, the height put back to last frame's, the points with it); the lists
  freed. With no object or wall response, a remote side's collision record
  (+0xd8, multiplayer: on and active, of this frame, sent within the last
  four) answers as a wall with its normal and point.
- **A wall** (0x4a0b40): the hit node's touch with damage 1 when it has its
  own handlers (slot 1); nothing more for a normal flat in x and z (or NaN);
  else, in the ground plane: the vehicle moved out by the segment's end
  reflected about the normal from the crossing (zmth::reflect), the points
  with it; the velocity the slide along the wall plus collision_d[0] of the
  part into it, its height kept, and the local velocity turned from it; the
  turn rate kicked by collision_d[1] x the speed toward the side hit.

`vehicle_collisions` and its pieces in `mech3/veh_move` (CollisionHit now in
`mech3/collision_hit.h`, shared with the mechs'); `test_veh_collide.py`, 600
runs with the original's own sweep, sorting and responses, 24 breaks
caught; `test_fixes` fix_vehicle_collision_fire_floor.

## In the program (step 3a, 2026-10-23)

`mech3/app_vehicle` wires the basic vehicle's calls out to the ports (the
node lookups, copies and names, the world's node list, zEntity's list,
zEffect's animations, zWeapon's hit state and handlers, zEntity3d's
refresh: `Vehicle::entity_moved`, now on the shared base) and gives the
mission start (player.cpp, app_mission's `WiredPlayerHooks`) its vehicle:
made over the AI's controls, named, on its team, placed, set up (the
controls' setup through vt+0x0c), the motion block, the points and zone,
zWeapon's hit handler (0x4af550's port) on its node. c1m1's four (ttruck_01,
ttruck_02, apc_01, apc_02) are made, in the world, with their health and
weapons (`mission_start_reads_c1m1_vehicles_headless`), and on zEntity's
list with the player, the Owens and the command marker. Their frame, brain,
sensing and engine sound since step 3b (below); their teardown and kill
credit since 3c. The mobile field bases since 4a1 (below).

**The teardown and the kill credit (3c).** The mission's end deletes every
zEntity entry through vt+0x00; a vehicle's is 0x4ac630(1) (`vehicle_delete`,
`program_vehicle_free`). A vehicle killed by a weapon credits the shooter's
entity (obj.vt+0x80): a mech's 0x41b420 counts one vehicle and 75 points
and, in the campaign for the player or an ally, salvages it (0x472ec0: on a
coin, its weapons); the victim answers itself to vt+0x2c and vt+0x40, 0 to
vt+0x48 and vt+0x50. A vehicle's own vt+0x80 (0x4a5bc0) is an empty
`ret 4`: a vehicle that kills scores nothing.

**The collision count.** Every shipped vehicle's model has the twelve
collide00..11 nodes, so the motion reader takes the points from the model,
and that path never writes the count (+0x214): it stays 0 (nothing else
in the exe writes it). The TRACK step then places no collision points, and
the sweeps' segments run between unset ones, except a slide's side points
(6..11), placed afresh. So vehicles cross walls and buildings unless
sliding. The list path writes 12; a candidate for Owen (fixes.md): giving
the model path the count would make vehicles collide as their data says,
which may change how the AI's vehicles get round obstacles.

## Brain, sensors and the engine sound (step 3b, 2026-10-23)

The entity walk (0x4a1140) runs a vehicle's vt+0x34, the frame 0x4a1040,
when its +0x09 is set, as a mech's. What the AI's brain, the sensors and
valves.zrd ask of an entity goes through its vtable in the exe; the
program answers by kind (`is_program_vehicle`), from the basic vehicle's
vtable 0x5c88e8:

| slot | function | answer |
|---|---|---|
| +0x0c | 0x5a82d0 | the velocity, +0x30 |
| +0x1c | 0x5a85a0 | the node takes the matrix (`entity_moved`) |
| +0x24, +0x28, +0x2c, +0x40, +0xec | 0x4d2a70 | itself |
| +0x50, +0xa0 | 0x40bae0 | 0: no mech, never powered down |
| +0xb4, +0x144 | 0x4d7290 | 0: not hidden from radar, never airborne |
| +0x148 | 0x45b960 | [0x5c23a0], 0.0: no jump fuel |
| +0x70 | 0x4ad2d0 | the death (valves' destroy) |
| +0x98 | 0x4ad820 | the health share |
| +0x8c, +0x90, +0x150, +0x154 | 0x4ad780, 0x4ad7c0, 0x4ad6e0, 0x4ad700 | the gun's aim (above) |
| +0x114..+0x12c | 0x4acfa0.. | the speeds (above) |
| +0x140 | 0x4020b0 | its controls (+0xe4) as IControls |
| +0xc8 | 0x4add90 | the brain's empty vt+0x08, then the engine sound released |
| +0xcc | 0x4aee00 | active, and then the brain's vt+0x0c (+0x54 clear); true without a brain |
| +0xd0 | 0x4aee40 | the brain's vt+0x10 (sees its target); true without one |
| +0xdc | 0x4a3630 | the placement |

The sensors' +0xd0 false clears +0x09, so a vehicle whose brain does not
see its target (the player or the player's lance within sight range)
skips its frame: it stands frozen until someone comes near. The brain's
+0x88 (aiv.zrd's p8 item 10; 1 on c1m1's trucks) makes it always see.
This is the mechs' level of detail in another form.

**The engine sound.** The vehicles of one engine type share one entry of
the sound cache (0x60b110), with one voice. Each frame the sensors release
every vehicle's entry (vt+0xc8: 0x4adcd0, 0x4adc30): the claim is dropped and the
voice moved to a far point (0x4adb60: about 5e18 out on x, -y and z, too
far to hear). The radar ping (0x4aa400) then walks the view's contacts
nearest first; the first whose vt+0x40 has a vt+0xec (a vehicle answers
itself; a mech 0) claims its entry at its place (0x4adce0, 0x4adc60): the
voice moved there (0x571f30, no velocity, no doppler), or made there at
full volume (0x56e7e0) and locked (0x56f3f0) on the first claim. So only
the nearest vehicle in sensor range is heard, and it is heard at its place.
Ported (`EngineSoundEntry::claim` / `release`, `EngineSound::play` /
`release`), checked by `test_veh_engine.py`.

c1m1 (`mission_c1m1_vehicles_run_their_frames_headless`): after bramp the
trucks drive their net_01 (node_00/01 to node_08/09, where they wait for
truck_go); truck_go sends them on to node_15/16, whose valve_assign sets
destroy_truck_01/02, and the valves' destroy kills them (vt+0x70). The APCs
drive their own nets, more than 100 m.

## The mobile field base (0x4589d0, mech_map / mech_objective region; step 4a1)

`mech3/mech_fieldbase`, ported 2026-10-24. The mission start makes every
vehicle.zrd type `mobilehq` (matched whole) with 0x4589d0, the rest with
0x4ac320. c1m1's mobilehq_01..03 (aiv kind 2) are AI-driven over their net,
health 75.

**The class** (0x160 bytes, vtable 0x5c23d8): the basic vehicle's slots
but vt+0x00 (the destructor 0x458b30 / 0x458bc0), vt+0x34 (the frame
0x459640), vt+0x70 (the death 0x4590b0), vt+0xf0 and vt+0xf4 (0x4d2a70:
itself; the basic vehicle's and the mechs' 0x40bae0 give 0: so the AI's
station keeping, 0x4cc150, takes a field base as one), vt+0x158 (the setup
0x458d40) and vt+0x160 (the damage 0x458f50). Its own fields: a std::list of
queued steps at +0xfc..+0x107 and a "begin the front one" flag at +0x108,
the seven nodes `arm01..04`, `cover_left`, `cover_right`, `cover_middle` at
+0x120 (found in its tree, 0x4f9d60), the arm script at +0x13c, its parts
by the same names at +0x140, and an animation the dock starts at +0x15c
(0x458cf0 / 0x458d20, the registry's [0x605c44]). The limiter classes
0x4a42f0 / 0x4a4580 / 0x4a4800 are the gun's (the motion block); the field
base makes none.

**Made** (0x4589d0, 0x458a70): its node always a copy of the world's
`type` node, renamed and put in the world (the basic vehicle first looks
for a node of its own name); the basic vehicle made over it without a node
(0x4ac530(ctl, 0)), then given it (0x4d6c90, onto zEntity's list), 16 high
for the AI (+0x68, the basic 12); then the registry's add, and the node's
flag 8 cleared (0x4eb620).

**The registry** (the static at 0x605ae0, constructed by 0x459b80, emptied
at each mission start by 0x459e80 -> 0x459e50): up to three bases (+0x14)
with a valve each (+0x20), named after the base's node and set to 0
(objectives.md: the field-base objective reads them), +0x10 the count. In a
campaign no more than the campaign's +0xa8 (the bases left: 3 at the start)
are taken: a base past them is put out of play at once (+0x08 cleared, its
engine sound dropped, its node hidden 0x4eb170) but still counted. So after
losing a base in one mission, the next missions show one fewer: the third
is there but hidden and inert. A fourth base would be written past the three
over the first valve (FIX (bug); none of the shipped missions has one).
The alive count 0x45a000 counts the registered bases still active. The
mission start (0x459e80) also finds the dock's eleven radio lines
(+0x8c..+0xb4: mcb_friendly_fire, mcb_under_attack, mcb_lost_vehicle,
mcb_occupied, mcb_pos_wait, mcb_pos_cancel, mcb_contact, mcb_waiting,
mcb_deploy, mcb_abort, mcb_complete) and its effects animation
`mfbeffects` ([0x605c44]); no shipped sound set has `mcb_deploy`, so that
line (the deploy state's, once the bases have opened) is silent. The rest
of the registry is the repair dock (below).

**The setup** (vt+0x158 0x458d40): the basic one (0x49ec00), the seven
nodes, the scripts' loose-file path `..\data\common\mechs\animations`
added to [0x736584] (0x576140; the mechs' set-up 0x413e60 made it, and it is
never trimmed, so it grows by one entry per base per mission; the port's
motions come from motion.zbd only), the arm script (0x458890: the script
`mobilehq` from motion.zbd at 30 frames a second, its loop 1 s, played once:
+0x70 clear; its target +0x88 and time +0x8c 0, shut), its seven parts, +0x09
set (the entity walk runs it), and the AI record: never asleep (+0x88, p8
item 10), +0x5c 0, +0x60 1, +0x80 1, can_leave (+0x3c) and the three scan
switches (+0x98..+0xa0) off.

**The frame** (vt+0x34 0x459640), when +0x09 is set: the front queued step
is begun (when +0x108 says so), else ended and dropped when done (the next
then begun), else stepped (vt+0x04; true marks it running, +0x04). Then the
basic frame's controls update and destroyed test (as 0x4a1040), but when
the update ran: the throttle set to 0 unless [0x60b4d0] (the dock lets the
bases drive; 0 at the mission start, 1 from 0x45a780), the move 0x4a1240,
the motion animation 0x4addc0 and the refresh, without the basic frame's
path test, player aim and weapons step. A destroyed base still runs the
front step's after (vt+0x08) and the arms. So a field base on its net
stands where it was put until the dock moves it.

**The arms** (0x4597d0 over 0x458900): the script's time moves toward its
target (0 shut, 1 open) at 0.2 a second: five seconds to open; each frame it
moves, every part's pose at that time (translation, rotation as a
quaternion, 0x525740) is set on its node (0x4fc3f0, 0x4fc300). A NaN time
goes to 0; a NaN difference stops.

**The damage** (vt+0x160 0x458f50): nothing to an inactive base. Off
multiplayer, a base on the player's side (the player's mech's allies
+0xc4 against its team) takes the difficulty's share as the player's mech
does ([0x608e5c]: 0.7, 1.0, 1.3). Then the basic damage (0x4ad3f0); a base
it kills: the radio's "base lost" line (+0x94, 0x4e6d50) while any base is
still active, else 0x47a720 on [0x60b544] (the last base lost); a base hit
and alive: the attacker's entity (0x581e40, 0x40eee0) on its side gets the
friendly-fire line (+0x8c), any other the under-attack line (+0x90), and the
base's controls are told (IControls vt+0x94: the AI's 0x4ea520). The lines
are mcb_lost_vehicle, mcb_friendly_fire and mcb_under_attack.

**The death** (vt+0x70 0x4590b0): the basic death (0x4ad2d0, which acts
once), then in a campaign the loss below (A mobile field base lost). The
loss is not guarded by the death's once: each call takes a base off the
campaign's count. The valves' destroy and the damage both call vt+0x70;
the damage only for an active base. Unported (`base_lost`): the cut of
what the player carries (step 4b or later).

**The destructor** (0x458bc0): the dock's animation stopped, the node
taken from its first parent and freed (with zEffect's animations on it,
0x506bc0(node, 1, 1)) when it has one, off zEntity's list, the arm script
deleted, the steps' list emptied (the steps are the dock's), then the basic
vehicle's (0x4ac650, with no node left). The registry keeps the base's
pointer until the next mission start empties it.

In the program: made by the mission start (WiredPlayerHooks'
mobilehq_create), its slots by the class (`BasicVehicle::field_base`, ours:
the death and damage hooks, the frame, setup and free in app_vehicle),
vt+0xf4 for the AI's station keeping, the registry emptied by the mission
load's 0x459e80 hook. The debug launch has no campaign behind its game, so
the bases left read 3 there (a new campaign's). Checked:
`test_veh_fieldbase.py` (300 runs of each piece, 45 branches, 25 breaks)
and c1m1's headless test (three registered and alive, held in place, the
arms of one opened through its frames, one shot down, all freed at the
end).

## The repair dock (0x459b80..0x45b9c0; states and drives 4b1, frame and request 4b2)

`mech3/mech_fieldbase_dock`, ported 2026-10-24. The registry (0x605ae0)
holds the dock: a list of states (+0x00..+0x0b, a "begin the front one"
flag at +0x0c) run by the dock's frame (0x45a270, from the mission's frame
0x48f4ad, off multiplayer) as a field base runs its steps: the front one
begun (that frame does nothing more), else stepped until it says it is
finished, then ended, dropped and the next begun. A step that answers
finished after its own abort has unwound the list (the repair's) marks
the dropped state, not the new front: the close it unwound to was begun by
the unwind and runs from the next frame, so the bases close as they
should. The seven states are members of the
registry, each with the registry at +0x08 and the radio line it says at
+0x0c (vtable 0x5c2600 their base and the drives': begin clears +0x04, step
and done return it, the message aborts, the abort clears the dock's marker
0x47a720 on [0x60b544] and sets +0x04):

| state | vtable | what it does |
|---|---|---|
| idle +0xbc | 0x5c25e0 | waits (+0xb8 set); its message 0x45a780 is a mech's request (below), which queues the six others in the order below |
| places +0xe0 | 0x5c2620 | five frames on, says mcb_contact ("move into position"?); done when every active base with a valve has its valve set (each base's net sets it at its place) |
| contact +0xd0 | 0x5c25c0 | says mcb_waiting once; done when the requester is within 12 m (squared 144, [0x5e4a18]) of the dock point and powered down (vt+0xa0); [0x60b4cc] set |
| deploy +0xf4 | 0x5c2640 | busy (+0x78); each active base's drive out queued at the front of its steps; the player's view onto camera mode 4 (its own kept at +0x80); the mech at rest at the dock point (0x418e90); done with the three drives, saying mcb_deploy (no shipped sound); the mech powering up aborts |
| repair +0x110 | 0x5c25a0 | busy; the effects `mfbeffects` on each active base (0x458cf0); twelve seconds, then the mech repaired (0x4154e0 with that frame's time); powering up aborts; its end says mcb_complete and stops the effects |
| close +0x124 | 0x5c2580 | each base's drive in; the view back to its camera if still on mode 4; done with the drives; [0x60b4cc] cleared |
| reset +0x140 | 0x5c2560 | the bases held again ([0x60b4d0] 0), "mcbdeploy" 0; its end queues the idle state again |

The aborts: the places' and contact's (0x45ade0) say mcb_pos_cancel and
unwind to the reset; the deploy's (0x45b1b0) says mcb_abort, drops its
drives (0x459af0) and unwinds to the close; the repair's (0x45b3b0)
unwinds to the close, its line said by its own end (the unwind ends it
after it set mcb_abort). The
unwind drops the list's front states until the target is the front, ends
the old front and begins the target. While the dock is busy, another mech's
request (the message 0x45b770 of deploy, repair and close) hears
mcb_occupied. A line is said only once [0x605c4c] (the quiet time) has
passed; the places state holds the next one five seconds.

**The drives** (0x18 bytes, vtable 0x5c2540, 0x4598a0; their slots
0x4598d0..0x459af0, 0x45b9a0, 0x45b9c0): out, the base's arms open and it
backs at a quarter throttle until its travel (the stand-off 15.4 less its
distance from the dock point at the start, then plus its local z speed
each frame) passes -0.2; in, the arms shut and it drives forward from its
distance to its place in (+0x110) until the travel is under 0.2. The
throttle drops to 0 as soon as the stopping distance (vt+0x124) would
carry it there. Done when it reached it with its arms there (open: the
arm time 1, shut: 0; NaN counts as there), or with no base or an inactive
one. Its abort drops its base's front step and begins the next.

The drives are freed by the state's end, but a drive can still be on its
base's steps then (the base drops it a frame later). The exe goes on
reading the freed drive until the base drops it; the port keeps it until
then (FIX (bug), no change in play). A mission with fewer than three bases
(none shipped) would have the drives and the places' check follow a
missing base (FIX (bug): passed).

**The request** (0x45a780, the idle state's message; the registry's
0x45a500 passes a mech's ask to the front state, which the player's MFB
command 0x47a7a0 sends on its second press): the requester kept (+0x74),
the mcbwaynet's dock point ([0x60b544] the waynet "mcbwaynet" (0x4d5460),
its element by its +0x20, 0x58b060) and its name (vt+0x24); with both the
bases may drive ([0x60b4d0] 1), "mcbdeploy" 1, the point's own valve
(found by its name, 0x4d3d20, +0x88) 1 and the bases' valves 0. Each base's
brain (vt+0x140, IControls vt+0x54), on its current order's net, finds the
net's node of the point's name (0x58b0a0: an element by name) and goes
(0x4c84b0) to that node's link i (base i's place; c1m1's net_02 has the
node "MFB Baker" with three links), its way finder dropped first. Then the
rings around the dock point (0x45a560: three points on the near ring,
15.4 m, the first in +z and the others 120 degrees round, and three on
the far ring, 45.4 m; each base given its two and the point, +0x10c, +0x110,
+0x114) and the six states queued: places, contact, deploy, repair, close,
reset. 0x4c84b0 (the brain's go-to, `Brain::go_to`): on the current
order's net, the way finder (+0x238, 0x4c7c10) from the current state's
behaviour to the target; when that behaviour is +0x23c's (none), the
current order is dropped first and the new one's behaviour asked, its
answer unused.

Not ported: the mcbwaynet (mech_waynet.cpp, about 1,100 lines: arrangenodes
nodes made its points, the stations of multiplayer, the HUD's marker) and
the MFB command 0x47a7a0 / 0x47a730 (pick a point, ask, cancel). Until
then no request comes in play; the tests stand a dock point in.

**Multiplayer** (game type 3, the same frame): no states; with the game's
repairs on (0x479480, +0xa8), the player's mech active and powered down,
and the nearest station ([0x609980], 0x4d51d0) within 20 m (squared 400): an
enemy (the nearest contact, 0x4d6b90(3), with a mech) within 100 m gives
"%s is too close for MFB deployment" (its name); else within 6.5 m (squared
42.5) the repair at the station (0x45a180), past it "too far from MFB
depolyment point" (sic). The repair: "mfb_station_01" on the station's node
(by its name, +0x2c); a local mech shut down (+0xdd), the view on camera 4,
at rest at the station, the animation's end (0x45a0f0) repairs it
(0x4154e0), clears +0xdd, powers it up and puts the view's camera back
(+0x5c); the other players told (the stations' vt+0x14, 0x46ff90). A remote
mech only shows the animation; the network's message (0x46ffd0 ->
0x45a140) finds the station by id.

The repair itself (0x4154e0, the mech's: its parts mended and ammunition
refilled) is the mech's code, not ported here.

[0x60b4cc] and [0x60b4d0] are read by the AI's deploy state (0x4d0ce0,
step 4b4). The mission start (0x459e80) ends any dock left running: the
list back to the idle state alone, to begin, and "mcbdeploy" 0 (made
there when missing: every mission has it). Checked: `test_veh_dock.py`
(600 state runs, 300 drive runs, 600 frame, request, ring and station
runs; 100 branches; 30 + 26 breaks) and c1m1's headless tests (the dock as
the mission starts; with the dock point stood in at "MFB Baker", the
player's request, the bases to their places, the player onto the point
powered down, the deploy, the repair 12 s on and once, the close, the
reset and idle again).

## The stealth object and BASIC mode (2026-10-23; back burner)

The mission load makes it in every mission ("Creating Stealth", 0x4a4de0,
[0x609a2c], vehicle.zrd's `stealth`: no model, sounds or weapons, health
10000, `mode` basic, rates 40 / 65, turns 2.5 / 3.5); the unload deletes
it (0x490480). It is on zEntity's list, sorted last; the sensors clear its
state and +0x09 each frame, so the entity walk never runs it. Its frame
runs from the player's tick only while the view is on it.

The view goes onto it in two places: the debug key setup 0x40e430 (its own
key map, keys 0x39 and 0x439; the view attached, vt+0x108), the handler of
command 0x9a in 0x40dba0's switch, which nothing in the release registers
(0x40db60 registers 0x61, 0x6c, 0x70 to it; also unregistered, 0x84 calls
the mech swap); and the mech swap 0x4783c0 (the mech lab's resume(4) in
0x481170 with another mech, the multiplayer respawn 0x46dde0), which parks
the view on it while the mech is rebuilt and returns it in the same call.
So in the release it is only a placeholder for the camera.

**BASIC** (0x4a1d90, its ground 0x4a1f00), the stealth object's movement:
by camera mode, a car's steering (0x4a20d0: the turn into the heading
+0x330, wrapped at 2 pi) or, in mode 3 (straight down), north-up panning
(0x4a2000: heading 0, the input on the sideways speed, stopped dead before
reversing); the speed step 0x4a2b10; the move on the horizontal plane;
then the four platform points' ground (0x4a2190), the highest taken plus
an offset (+0x88 of the motion block, likely `mode_alt`), pitch and roll 0:
a level sled glued above the ground. A developer's fly camera.

Not ported (Owen 2026-10-23): both go in at the end, behind a debug key,
as our addition (CHANGES.md then).

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
mech. The update loop runs a basic vehicle's frame (step 3b) and logs any
other entity as the unported "entity vt+0x34". `test_program`'s c1m1 death run puts a
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
With no base left nothing is cut here. Not ported (step 4b or later: the
field base's death reaches it as `FieldBaseHooks::base_lost`). No sign in 0x00458xxx..0x0045bxxx of repairs
looking at the mechs carried: the store is touched only here.
