# Weapons at run time

Mech3.exe 1.2, 2026-10-21. Two halves, like the source tree: zWeapon in the
engine (`gamez/zweapon`, 0x581230..0x586b70 and `zwep_init.c` 0x583880) and
the game's `mech3\weapon.cpp` (0x4af5a0..0x4b5000). Read with
`docs/structs.md` (Weapons), `docs/heat.md` and `docs/damage.md`
(TakeDamage, the receiving end). Every claim gives its address; "likely"
marks an inference.

## Status

Ported and emulator-checked (24-bit x87, floats bit for bit, every call out
logged on both sides):

| Piece | Port | Test |
|---|---|---|
| weapons.zrd into the definitions: 0x583880, effect blocks 0x584bb0, find 0x581ed0, shutdown 0x584930; the MW3 half WeaponType 0x4af5a0 / 0x4b0020 / 0x4afb10; the mech lab's weights 0x484b70..0x484c00 | `gamez/zweapon/zwep_init`, `mech3/weapon` | `test_weapons_zrd.py`: the real file (49 defs) and 300 variants, every def, impact block and WeaponType byte for byte, 41 paths |
| The manager, groups, bins, projectile and beam weapons (0x4b00d0..0x4b4f00 but the AI planner), the def callbacks (salvo marks 0x4afc00, airstrike 0x4afed0, LB-X burst 0x4b44d0), the AI's LRM wait, the mech lab's rounds | `mech3/weapon` | `test_weapons_run.py`: 600 managers, 44,000 operations, 125,000 calls, 60 paths |
| zWeapon's fire state setters (0x581f60..0x581fd0), the damage path (beam damage 0x583020, hand-off 0x583480, impact 0x5826b0, splash 0x582920 / 0x5829b0, hit 0x586350, air burst 0x582320) and the flight step 0x585cc0 | `gamez/zweapon/zweapon` | `test_weapons_hit.py`: 20,000 operations on the real defs, 48 paths |
| Vector and quaternion helpers 0x51dfa0, 0x51e3d0, 0x51e740, 0x525490, 0x525840 | `gamez/zmath/zmth_vector` | through the two tests above |
| zWeapon's projectiles: the pool 0x584df0 / 0x584e60 / 0x584f40 / 0x584fb0 / 0x584fe0 and its nodes 0x5847c0, the FLYOUT model pool 0x5830b0 / 0x5830e0 / 0x583120, spawn 0x585010 with the static cast, the tick 0x5823c0, update 0x585710, homing 0x585900, collision 0x5861f0 (0x586160 / 0x5861c0), the pick 0x583130, classify 0x582b10, commit 0x5858b0 / 0x5864c0 / 0x586600, mines 0x586670 / 0x586740, bursts 0x5821c0 / 0x5820c0 / 0x5822c0, the aim slope 0x581ff0, the counts 0x581e50, sounds 0x582f60 / 0x582fc0, craters 0x5825b0 / 0x5824b0, the node handler slots 0x583220..0x583440 and touch 0x5836b0, the light pool 0x586970 / 0x5869f0 / 0x586b20 / 0x586b70; the maths 0x51e0e0, 0x51e000, 0x51e280, 0x51e460, 0x524247 | `gamez/zweapon/zwep_shot`, `zweapon`, `zmth_vector` | `test_weapons_fly.py`: 200 sessions (more until every path is reached) of 40 operations (spawns, frames, bursts, beams, handler tables, the AMS) on the real defs with unshipped features poked in, every call out, every pool slot, beam and scene node compared; trig-fed floats within 3e-5, 155 paths (287 sessions, 318,000 calls on the last run) |
| The beams: make 0x584a70 (0x584db0), free 0x584b40, start 0x581230, stop 0x581400, the frame 0x5814b0 with DESIGNATE and BOUNCES, MULTI_TARGET 0x582c10 / 0x582de0, draw 0x582f00 | `gamez/zweapon/zwep_beam` | `test_weapons_fly.py` |
| weapon.cpp: the radar list 0x4b49c0..0x4b4b96, the AMS 0x4b3ab0..0x4b3c05, the HUD ammo gauge 0x4b0d80, the vehicle builder 0x4b4660 with the named add 0x4b0460, the mech's configuration step 0x414501..0x414732 | `mech3/weapon` | radar and AMS in `test_weapons_fly.py`; the rest in `test_weapons_run.py` (600 managers, 73 paths) |
| The engine's soil names and LoadSoils 0x535f20 / 0x537cb0 / 0x537270, read by the impact table | `gamez/zmodel/gmod_matl` | `test_weapons_zrd.py`: LoadSoils on soils.zrd and 40 variants; weapons.zrd read with the mission's 14 soils |

The engine calls (scene graph, object3d, animations, sounds, the crater
module, cls_di.c's segment, ground and sphere queries) go through
`zwep::RunHooks` / `HitHooks`; the queries are ported separately in cls_di.c
by another piece of work and are hooked here. Left: see the last section.

## Three layers

| Layer | Made by | Size | What it is |
|---|---|---|---|
| WeaponDef | 0x583880 (zwep_init.c) | 0x1f4 | one per `BALLISTICS` entry in `weapons.zrd`, a table at 0x9a0eac (count 0x9a0ea8) |
| WeaponType | 0x4af5a0 | 0xc8 | the MW3 half of each def, at def +0x1f0 |
| MechWeaponTable | data at 0x5e64f0 | 37 x 0x18 | the weapons the mech lab knows: name, message id, family, index, availability, uses ammo |
| WeaponManager | ctor 0x4b00d0, dtor 0x4b01d0 | 0x94 | one per vehicle, at Vehicle +0xd4 |
| WeaponGroup | ctor 0x4b1d80 | 0x54 | five per manager |
| Weapon (projectile) | ctor 0x4b2530, vtable 0x5c9088 | 0x98 | one per mounted weapon |
| BeamWeapon | ctor 0x4b3c10, vtable 0x5c90c0 | 0xa4 | for defs with BEAM (def flag bit 1) |
| AmmoBin | in the weapon ctors | 0x18 | ammo pool, or a beam's charge |
| Projectile | pool at 0x9a0eb4 | 0x5d8 | a shot in flight (zWeapon) |
| Beam | 0x584a70 | 0x150 | a beam in the air (zWeapon) |

## Definitions: weapons.zrd (0x583880)

Called at mission load (0x490173) with ECX the world, EDX `weapons.zrd`, a
zero argument kept at 0x9a0ec0 and the callback 0x4b0020 that makes each
WeaponType. Top level: `VERSION` must be 2 (else error 0x400 and -1);
`LOCK_ON_WARNING`, `NO_AMMO_WARNING`, `TRIGGER_INACTIVE`, `WEAPON_INACTIVE`
are sounds (0x9a177c, 0x9a1794, 0x9a1798, 0x9a179c, none in the file);
`MAX_CRATER_RADIUS` (0x9a1784, 30 by the startup 0x583750); `BALLISTICS` is
`name (keys...)` pairs.

Each def starts as: ammo limit 50, range 500, velocity 120, damage 0.1, turn
and pitch rate 0.16, flag DETONATE_ON_WATER, index = (short) its position,
six zeroed impact blocks. Then, in this order (field offsets in
`zwep_init.h`):

- `NAME` (+4, else the key), `DESC` and `MILITARY_NAME` through the message
  table, strdup'd (+8, +0xc, else the key). Damage's "killed by a nuke" test
  compares +8 (`docs/damage.md`).
- `ACCELERATION` +0x38, `AMMO_LIMIT` +0x18.
- `BEAM`: flags BEAM and TIMED_DAMAGE, velocity 10000, energy 1, damage at max
  range 0.25, pulse (1, 0.5, 0.75); inside, `BOUNCES`, `TIMED_DAMAGE`,
  `ENERGY`, `TIME_TO_MAX_RANGE` (velocity = 1 / it, 10000 when <= 0),
  `DAMAGE_AT_MAX_RANGE`, `PULSE` (three values: 1 / the first, 0.125 when
  <= 0, then the other two). The file writes `BOUNCE`: never read.
- `CATCHES_FIRE`; `CRATER` (flag, the crater module's defaults twice from
  0x60bfc0, then `points`, `radius`, `depth` as (min max): minimums at
  +0x174.., spans at +0x19c..); `DAMAGE` +0x48; `DETONATION_DISTANCE`
  (squared, +0x44); `DETONATE_AT_RANGE`, `DETONATE_ON_WATER`;
  `DESTROY_ANIMATION` +0x168; `RANDOM_DEVIATION`; `EXPIRES`; `FIRE_RATE`
  (+0x14 = 1 / it: the period, seconds per shot); `FIXED_ROTATE`; `GRAVITY`
  +0x4c (behind a LOCK_ON test that is dead: LOCK_ON is read later);
  `HIT_OWNER`; `IMPACT_PROXIMITY` (+0x3c and squared +0x40); `IMPACT_TYPE`;
  `INSTANT`; `LOCK_ON` (+0x50 and the flag); `LOCK_ON_LEAD` (+0x70..+0x78);
  `MINE`; `MULTI_TARGET`; `QUICKSAND` (flag; a crater block if CRATER did not
  give one); `RANGE` (+0x1c, squared +0x20); `RANGE_MINIMUM` (flag, +0x24,
  +0x28); `RELATIVE_SPEED`, `REMOTE_DETONATE`, `TETHER_GUIDED`; `TURN_RATE`,
  `TURN_SUSPEND_TIME`, `PITCH_RATE`; `VELOCITY` (not for beams); `RELOAD`.
- The WeaponType callback, then the `FIRE` (+0x84) and `FLYOUT` (+0xe0)
  effect blocks, `FLYOUT_HEALTH` (+0x80 as a float, else -1),
  `SHOW_ON_RADAR`.
- `IMPACT`: `default`, then one block per soil kind in 0x5edbc0's order:
  water, seafloor, quicksand, lava, fire. A kind not listed copies default; a
  listed one without `ANIMATION_MIN_RANGE` takes default's. Then
  `ANIMATION_ALWAYS`, `FREEZE` / `HEAT` / `DESIGNATE` (six values into
  +0x1c8; DESIGNATE also sets +0x44 from its sixth, drops the heat flags and
  sets bit 12), `KILL_ANIMATION` +0x16c, `DAMAGE_ANIMATION` (count 1, health
  1.0), and on Direct3D only (0x72f348) `DAMAGE_ANIM_ON_HEALTH`, (health anim)
  pairs from +0x148.
- After each def: with gravity, +0x1e0 = v^2 / 2g and the velocity is replaced
  by sqrt(2 g range) through the bit trick (none shipped: every GRAVITY is 0);
  a FLYOUT model is set active and bit 9 records whether no node in its tree
  is unlit-data (0x4f9e00 with 0x4f9de0); a projectile def adds
  1 + ftol(range / velocity / period) slots to the pool.

The effect block (0x5c, 0x584bb0): +0 flags (bit 0 `RANDOM_ROTATE`, bit 1
`ANIMATION`'s second value: the fire animation follows the firepoint),
+4 `ANIMATION`, +0xc `ANIMATION_ATTACHED`, +0x10 `MODEL_ANIMATION`, +0x14
`SURFACE_ANIMATION`, +0x18 `ANIMATION_MIN_RANGE`, +0x1c `EFFECT` (else
`MODEL` +0x20, a node of list 6), +0x24 `SOUND` count and four sounds,
+0x38 `BOUNCE_SOUND` count and four, +0x4c / +0x50 the next shot's sound and
animation choice (0x581f70 / 0x581f90), +0x54 / +0x58 the node and offset it
is attached to (0x581fd0), cleared by the reader.

The pool: 164 projectiles on the shipped file for the whole mission, a free
list through +0 (0x9a0eb8). An empty pool drops the shot (0x585053) after the
weapon has spent its ammo and heat.

The IMPACT table has one block per soil name the engine knows when the file is
read (0x5edbb8, names 0x5edbc0): six built in (default, water, seafloor,
quicksand, lava, fire), then whatever the mission script's `LoadSoils
soils.zrd` (0x537cb0) added before the weapons are read at 0x490173: dirt,
mud, grass, concrete, snow, mech, silt, noslip (6..13). A material's +0x20 is
an index in that list, so a hit on dirt plays the `dirt` block: the pock
marks of the autocannons, Gauss, missiles and MGUN show (28 weapons have a
dirt, mud or snow SURFACE_ANIMATION). The game's own soil table 0x608c00
(grass 2, dirt 3, ...) maps those engine indices for the feet.

### WeaponType (0x4af5a0)

| Offset | Key | Notes |
|---|---|---|
| +0x00 | — | mech-lab index (0x4848a0 on the def's NAME), -1 if none |
| +0x08, +0x0c | HEAT | firing heat, then heat added to the target (0 in every weapon) |
| +0x10 | SHOW_ON_RADAR | an int list item only |
| +0x14, +0x18 | ZAPS_HUD | int or float; the second slot reads the **first** value again (0x4af71b): ERPPC's `(0.5 2.0)` gives 0.5, 0.5 |
| +0x1c..+0x30 | MASS `clan`, `sphere` | tons, slots (ftol), present |
| +0x34 | — | tons per round, 1 / +0x38 |
| +0x38 | MASS `ammo` | rounds per ton x SALVO truecount when SALVO exists |
| +0x3c, +0x40, +0x44 | SALVO count, delta, truecount | count 1, delta 0.1, truecount = count by default |
| +0x48 | IMPACT_FORCE | |
| +0x4c | ISNARC | |
| +0x50, +0x54, +0x58 | BURST radius, count, `wep` | with a known `wep`: +0x70 += its range, def flag DETONATE_AT_RANGE, callback 0x4b44d0 |
| +0x5c, +0x60 | U_LOOPED, U_RECHARGE_SND | MGUN alone has a loop |
| +0x64..+0x6c | NAMESOUND | name, `mech_part_damage`, `mech_part_destroy` |
| +0x70..+0x7c | — | range, range^2, range / 2, (range / 2)^2 (the halves not raised by BURST) |
| +0x80 | — | the low byte of the ctor's first argument, the reader node's address: meaningless |
| +0x84, +0x88 | — | the salvo marks list (0x4afc00) |
| +0x8c, +0x90 | SALVO radius (squared), delay | both > 0: callback 0x4afe70 |
| +0x94 | RECOIL | |
| +0x98 | U_RECHARGE_TIME | stored as period x fire time / recharge time |
| +0x9c | U_FIRE_TIME | 0.5 default |
| +0xa0 | U_DESIGNATE_AIRSTRIKE | the strike's def (TAG: TAG_MISSILE); callback 0x4afea0 |
| +0xa4..+0xbc | — | the airstrike's lock: point, flag 1, no entity, a pointer to the point, no velocity |
| +0xc0 | — | LRMs (index 0x11..0x14): range x 0.7 / clamp(velocity, 100, 5000) |
| +0xc4 | — | the AI fires this type again from this game time (0x4b4c70, set by the AI through 0x4b4c90 to now + +0xc0; 0x4b4bd0 resets them) |

The mech lab reads the weights and slots through 0x581ed0 then +0x1f0
(`mech3::weapon_weight`, `ammo_weight`, `weapon_slots`); 0x4b4900 gives the
lab's rounds for a weapon: min(rounds per ton, n), or 100 / 20 / 1 without a
def.

## The manager (Vehicle +0xd4)

Made in the mech constructor (0x414daf) and, for other vehicles, by 0x4b4660
(`build_vehicle_weapons`, from the vehicle's `weapons` list: "Vehicles" below).

| Offset | Field |
|---|---|
| +0x00 | trigger down now (0x4b0c20 copies it to +0x01 first) |
| +0x01 | trigger down last frame |
| +0x04 | 1 pressed, 2 held, 3 released, 0 neither (0x4b0c30) |
| +0x08 | alpha-strike end, clock time ([0x72c530]); 0 none |
| +0x0c..+0x18 | aim point, valid (0x4b0bc0) |
| +0x1c | fire mode: 0 chain, 1 single, 2 group |
| +0x20 | weapons destroyed (0x4b1040, 0x4b1110) |
| +0x24 | weapon count |
| +0x28 | selected group: **0 at start** (the ctor writes 0, not -1) |
| +0x2c..+0x3c | WeaponGroup*[5] |
| +0x40..+0x7c | Weapon*[16] |
| +0x80 | the mech (vehicle vt+0x2c, vt+0x50), 0 for other vehicles |
| +0x84 | the vehicle |
| +0x88 | has a target: lock-on is tried only with it (0x4b3330) |
| +0x89 | weapons online |
| +0x8c | sum over bins of ammo x damage |
| +0x90 | 1 / the largest that sum has been (0x4b4d10) |

Methods: 0x4b0400 add a loadout item, 0x4b0460 add by index on a node,
0x4b03c0 find by index, 0x4b0c30 update, 0x4b04f0 / 0x4b0610 next / previous
group, 0x4b0730 select, 0x4b0790 / 0x4b07e0 next / previous weapon, 0x4b0830
mode sound (0x16 chain, 0x14 single, 0x15 group), 0x4b0880 release, 0x4b08c0
network fire, 0x4b0a70 alpha strike, 0x4b0ae0 jettison, 0x4b0bf0 online,
0x4b0de0 critical, 0x4b1040 part lost, 0x4b1110 destroy all, 0x4b0220
unlimited ammo, 0x4b0370 forget a target, 0x4b2da0 arm restriction.

### WeaponGroup (0x54)

+0 manager, +4 index, +8 count, +0xc current (-1), +0x10 current weapon,
+0x14 Weapon*[16]. Add 0x4b1eb0 (up to 16, then selects weapons[0]), remove
0x4b1ef0 (shifts down, selects weapons[0]: after the last one goes, current
keeps pointing at it), find 0x4b1db0, select 0x4b1de0. Next / previous
(0x4b1f40 / 0x4b2000) skip weapons with no ammo and destroyed ones; `current`
is compared unsigned, and a group of more than one weapon with current -1
would loop four billion times (never happens: add selects). Trigger 0x4b2100,
held 0x4b21f0, release 0x4b20b0: mode 2 the whole group, else the current
weapon.

### AmmoBin (0x18)

+0 references, +4 amount, +8 max (grows when more is put in), +0xc
infinite (amount and max read -1), +0xd "out of ammo" played, +0x10 manager
and +0x14 damage (from the first weapon, 0x4b4cc0). SetAmount 0x4b4d10 keeps
the manager's +0x8c / +0x90; Add 0x4b4dc0. A weapon of a type already on
the manager takes that bin, adding a reference **for every** weapon of the
type it finds, so three LRM20s count 1 + 1 + 2. A beam's bin is its own and
has no manager.

## From the loadout to weapons

`dfn_<mech>.zrd` → configurations → `weapons` (`docs/mechdef-configs.md`).
When the mech takes a configuration (0x414501..0x414732, mech code, ported as
`mech3::configure_weapons`):

1. The manager is reset (0x4b0290: weapons and groups remade; group and
   destroyed count stay).
2. Every weapon item → 0x4b0400 → 0x4b2230: the group clamped to 0..4, a
   BeamWeapon (0x4b3c10) or Weapon (0x4b2530); a weapon that uses no ammo by
   the mech-lab table gets an infinite bin; SALVO count to +0x64.
3. Every ammo item → the first weapon of its index → its bin `Add(N)`.
4. Equipment counts; TAG mounts weapon 0x20 into group 5 through 0x4b0460
   (no 16-weapon check); the AMS gets 100 rounds per AMS (0x4146f9).

### Weapon (0x98; BeamWeapon adds +0x98..+0xa0)

| Offset | Field |
|---|---|
| +0x04..+0x10 | lock: flag, target (target vt+0x24), its id (0x5a8630), target vt+0xc |
| +0x14 | the target handed to the missile (with [0x605fb4] set) |
| +0x18 | rating: shots a second x damage (BURST: pellets x pellet damage); salvos not counted (set before +0x64) |
| +0x1c | its slot on the manager |
| +0x20, +0x24 | the firing part's node, the part (0x41d510 on the firepoint's model part, every update) |
| +0x28 | mech-lab index |
| +0x2c | WeaponDef* |
| +0x30 | reload timer |
| +0x34..+0x54 | aim point, muzzle, direction (0x4b36e0) |
| +0x58 | AmmoBin* |
| +0x5c | locked this shot |
| +0x5d | FCS in the firing part |
| +0x60 | group |
| +0x64, +0x68 | SALVO count, shots still to fire |
| +0x70 | damage 0..1, 1 destroyed |
| +0x74 | enabled (arms and water, 0x4b2f90) |
| +0x78, +0x7c | loop sound, recharge sound |
| +0x80 | firepoint |
| +0x84..+0x8c | offset from a weapon sharing the firepoint (share x (rand - 16383.5) / 16383.5 in x and y, unless the weapon has a SALVO truecount) |
| +0x90 | loadout item |
| +0x94 | manager |
| beam +0x98, +0x9c, +0xa0 | firing, the aim point's address, the zWeapon beam |

Vtables: projectile 0x5c9088 / beam 0x5c90c0: +0 dtor, +4 fire, +8 is beam,
+0xc firing, +0x10 damage a second with salvo (beam: per fire-and-recharge
cycle), +0x14 ammo (beam -1), +0x18 update, +0x1c lock on, +0x20 try lock
(on the entity's target record, only with manager +0x88), +0x24 aim and fire,
+0x28 trigger, +0x2c release, +0x30 held, +0x34 ready 0..1.

## Firing

### Trigger and modes

Commands (handler 0x41be80): CMD_TRIGGER1 0x3d sets manager +0;
CMD_WEP_NEXT / PREV; CMD_FIREMODE (mode + 1) % 3 with its sound;
CMD_WEPGRP_NEXT / PREV; CMD_WEPGRP_1..5; CMD_MOVETOWG_1..5;
CMD_ALPHASTRIKE 0x4b0a70(0.2); CMD_JETTISON_AMMO.

`Update(dt)` (0x4b0c30, from the mech update at 0x41a814): every weapon's
update, then without an alpha strike: pressed → the group's trigger (if
nothing fired the press is dropped); held → held; released → release, and in
chain mode the group steps to its next weapon. During an alpha strike the
state is 2 until the clock passes its end, then everything is released.

In group mode with more than one beam, every beam but the best rated clears
[0x9a17cc] before firing: zWeapon then plays no ground impact for it (only
the animation, not damage).

### CanFire (0x4b3a20)

Def present, enabled, weapons online, reload <= 0, ammo > 0 or infinite,
damage < 1.

### Projectile weapons

- Trigger (0x4b3190): aim and fire; on a shot, a SALVO weapon queues
  count - 1 more and a looped sound (MGUN) starts at the muzzle. A refused
  trigger on an empty bin plays "out of ammo" (mech sound 0xe) once for the
  player.
- Held (0x4b3280): a SALVO weapon that has reloaded triggers again; others
  try to fire. The out-of-ammo note here keys on "triggered again", not on a
  shot.
- Update (0x4b2c20): reload -= dt; the recharge sound for the mission's
  player while reloading; one more salvo projectile a frame, with the fire
  sound and animation muted (0x581f70 / 0x581f90 with -1), re-aimed.
- Fire (0x4b3450): FCS in the firing part; the lock (if the owner's vt+0x58
  is 0); the fire effect attached to the firepoint when its flag says so;
  spawn (0x4b4a40 → 0x585010; the friend flag decides the radar colour only);
  stats, the network event 0x16; reload = period; ammo on the salvo's first
  shot only (truecount, capped at what is left); the HUD; the firing heat and
  recoil (0x461670).
- Aim (0x4b36e0): the muzzle is the firepoint (salvo shots scattered by its
  `radius`, others moved by their offset) in the part node's frame
  (0x4ed650). Off the centre torso (node id 1), the direction keeps within
  acos(0.89), about 27 degrees, of the part's facing; centre-torso weapons
  aim anywhere.

### Beams

A beam's bin is its charge: full = ftol(U_FIRE_TIME x 10000).

- Trigger (0x4b3fc0): with a recharge time only from a full charge; aims,
  makes the zWeapon beam once (0x584a70), takes one unit, starts it
  (0x581230), half range under water ([0x9a17d4] = 0.5), heat once.
- Update (0x4b4230): n = ftol(max(10000 dt, 1)); firing drains n (timed
  beams heat in proportion) and stops at 0; not firing refills
  ftol(+0x98 x f x dt), f = 1 to heat 0.5, then 0.5 / heat, floored at 0.5.
- Release: only beams with a fire time over 0.5 s (pulse lasers) stop; held:
  fire again when ready, or keep steering.

### Heat (0x461670)

Projectiles: heat, then recoil on the part. Beams: heat once, or heat x dt
for TIMED_DAMAGE. AddHeat 0x419e40 (`docs/heat.md`).

### Arm restriction (0x4b2da0) and water (0x4b2f90)

For the player: the left and right side views keep only the matching arm
(firepoint part 2 / 3). In free look (controls +0xc == 1) with the torso's
two frames (0x465940 and the torso's +0xa4) more than 35 degrees apart, the
side comes from the sign of their cross product's y. Separately, a weapon
that is not a beam in a part with +0x49 (under water) is disabled.

## Crits, cook-off and jettison (0x4b0de0)

The candidate with the smallest value wins (from +inf). Weapon crit: weapons
on the part not destroyed, value the rating for plain beams, |ammo x rating|
otherwise; the winner is destroyed (0x4b3020: its sounds, HUD reselection).
Ammo: the configuration's ammo items in the part (any part for -1), value
|ammo x rating| of the first weapon of the index; the winner's bin is
emptied. Nothing here applies damage; CASE is checked by the caller
(`docs/damage.md`). Jettison empties the current weapon's bin, or the whole
group's in group mode (unlimited bins too).

## zWeapon run time

The rest of 0x581230..0x586b70 (0x580550 is DirectDraw's error strings; from
0x586ba0 it is the network session).

### Projectile (0x5d8, `zwep::Projectile`)

| Offset | Field |
|---|---|
| +0x00 | next (free list, then the def's active list at def +0x7c) |
| +0x04 | the shooter's node |
| +0x08 | the shooter's zone set ([vehicle +0x64]); with no zone only the low byte is set, to 4: "use the tick's" |
| +0x0c | scene node ("Projectile_%d", made by 0x5847c0) |
| +0x10, +0x14, +0x18, +0x1c | FLYOUT animation (following the node), attached animation, the model animation, the pooled copy of the FLYOUT model |
| +0x20 | state: 1 flying, 0 remove, 2 a mine lying in wait |
| +0x24 | spawn position |
| +0x30 | base velocity: the shooter's, for a locked shot only |
| +0x3c, +0x48, +0x54, +0x60 | direction, position, the move's end this frame, velocity |
| +0x6c..+0x74 | lock: entity, the target's position and velocity pointers (a `LockRef`) |
| +0x78, +0x7c | static hits cast at the spawn, the frame of the cast ([0x72f338]) |
| +0x80 | the static hits (a 0x504-byte list: count, then 0x28-byte records) |
| +0x584 | the next static record to reach |
| +0x588 | homing time; for a mine in a network game, its expiry |
| +0x58c, +0x590 | speed, maximum speed |
| +0x598 | RANDOM_DEVIATION's drift velocity |
| +0x5a4 | the drift so far, held within +-1 (y +-0.75) by pushing back at 10 a second (0x585f07) |
| +0x5b0, +0x5b4 | distance travelled, age |
| +0x5b8 | flyout health, -1 none; 0 means shot down |
| +0x5bc | damage scale ([0x9a17d0]) |
| +0x5c0, +0x5c4, +0x5c8 | copies of [0x9a17d8], [0x9a17dc] (both unread) and [0x9a17e0] (terrain following) |
| +0x5cc | spawn's fifth argument (likely a network id; the network gate may change it) |
| +0x5d0, +0x5d4 | per-frame callback (the radar's 0x4b4a30 / 0x4b4a20), its argument (the shot) |

Query answers (`HitList`, 0x504 bytes): the segment query 0x4eed90 and the
sphere query 0x4f0ac0 write the count in byte 0; the nearest-hit query
0x4eece0 moves it to byte 1 and puts the nearest index in byte 0, which is
what the pick reads.

### Spawn (0x585010)

ECX def, EDX the shooter's node, then the zone pointer, muzzle, direction,
velocity, p5, a slot to reuse (`ret 0x18`). With the network flag ([0x9a0ec0])
the gate [0x72fcac] can refuse it; a pool slot (0x584df0: the FLYOUT model
put under the node, a pooled copy when the model is animated) or nothing.
Positions and direction; the fire globals copied and reset to 1, 1, 1, 0.
Without ACCELERATION and not INSTANT: speed = VELOCITY, velocity = shooter's +
direction x speed (RELATIVE_SPEED is never tested), and, without gravity or
lock, the **static cast**: the segment query over the whole RANGE, with the
shooter (and a shootable shot's own node) out of it and only nodes that have
not moved since the frame (0x4ed9d0); the records' distances, then a sort
that compares the shifted record with the one before it instead of the one
being placed, so it is one bubble pass: only the farthest is sure to be last.
With ACCELERATION (or INSTANT) the shot starts at 0.0001 plus the shooter's
speed, the maximum raised by it, and the velocity the shooter's less its
speed along the direction. Then the FIRE sound at the muzzle, the FIRE effect,
or the FIRE animation (the choice from 0x581f90; RANDOM_ROTATE rolls it; on the
firepoint when attached); the node turned to the direction for animated
flyouts; the FLYOUT animations started; a lock's three dwords copied
(LOCK_ON, with the lock flag above 0) and the shooter's velocity kept as the
base; FLYOUT_HEALTH makes the node shootable (+0x24 bit 27, +0xbc = 1, +0x40 =
the shot). The fire and flyout choices are set back to 0.

### The frame (tick 0x5823c0)

The tick saves the zone ([0x74a328]), runs every def's update (beams:
0x5814b0), then every projectile def's commit, and puts the zone back.

Update (0x585710), per shot: state 1; its callback; the zone set to the
shot's (or the tick's); then **shot down** (health exactly 0: the
DESTROY_ANIMATION, or an air burst without one), a **mine** at rest
(0x586670), or flying: homing if locked, then the step (INSTANT: the whole
RANGE at once). A flying shot's move is cast (0x5861f0) and what it meets is
hit (0x586350) or, for a mine, landed on (0x586740). A stopped shot that is
not a mine would move by the last shot's move (the original's local; no
shipped weapon stops). INSTANT shots end after one frame.

Collision (0x5861f0): without a static cast, the segment query from the
position to the move's end and the pick. With one, the next static record
whose node has not moved since the cast, once the distance travelled reaches
it (one record a frame), against a query of only what moved since (0x4ed9e0);
the nearer wins.

The pick (0x583130): the query's nearest record, but water stops only
DETONATE_ON_WATER shots: the others play the water animation and take the
nearest record that is not water (none when the list has one record; the
static list's byte 1 is never written, so a static water hit is dropped).

Commit (0x5858b0): dead shots leave the list (0x586600: animations stopped,
the node out of the world, back to the pool; a TETHER_GUIDED shot is not
returned to the pool, its slot is lost), the others move (0x5864c0): gravity
shots face their velocity (not FIXED_ROTATE), a mine spins 200 degrees a
second and grows from 1 to 5 over its first second, the node is put where the
shot was at the frame's start ([0x5cec0c] is the constant 1 that picks it),
then the position takes the move's end.

### Flight (0x585cc0, ported)

Each frame: speed += ACCELERATION x dt up to the maximum (rebuilding the
velocity from base + direction x speed unless locked and not tethered);
gravity; the move = velocity x dt; RANDOM_DEVIATION adds a random drift of up
to 5 dt per axis, turned to agree with the drift so far, except within 20 m
(or 2 x DETONATION_DISTANCE) of the target; a tethered shot is held under
[0x9a17a0] (30) above the ground height; the distance travelled grows by the
move's length (mines count half). At RANGE: a homing shot without EXPIRES, or
DETONATE_AT_RANGE, bursts in the air (0x582320); every shot ends. Short of
it, a locked shot inside DETONATION_DISTANCE bursts.

### Homing (0x585900)

The wanted direction is to the target; LOCK_ON_LEAD (no shipped weapon)
blends in the intercept (0x524247) from lead min to lead max of age (slerp
0x51e460), then uses it. The turn allowed this frame is TURN_RATE x
[0x9a17d8] (the global, which spawn has reset to 1: the FCS doubling never
applies) x factor x dt, the factor 0 until TURN_SUSPEND_TIME, then
(t - suspend) / LOCK_ON, or 1 when there is no suspend time; t is the homing
time, which stops one frame past LOCK_ON, so with a suspend time the factor
never reaches 1 (LRMs about 0.93 at 30 fps). Tethered shots and FCS shots
(+0x5c8) probe the ground (0x4eda10): a tethered shot within 5 m of its
ceiling stops climbing; an FCS shot under 10 m climbs (pitch rate x 0.64)
when the line to the target is blocked by something without a damage
handler; with no ground under it the shot expires. The angle is acos of the
dot clamped to 0..1 (beyond 90 degrees counts as 90); the direction turns by
at most the allowed turn (slerp, normalised), and the speed is multiplied by
0.8 + 0.2 cos(turn). Until LOCK_ON the shooter's velocity fades out of the
velocity; the node faces the direction.

### Mines (0x586670, 0x586740; no shipped weapon)

A mine lands on the first thing it meets: something with a damage handler
sets it off (the impact); BOUNCES reflects it (0x582b10) at a quarter of its
speed with the bounce sound, over half the bounce's length, until it is under
0.2: it lies down (state 2, scale 5, in a network game expiring after 120 s).
Lying, it goes off when the splash query finds anyone but its layer within
IMPACT_PROXIMITY (REMOTE_DETONATE ones only by 0x5820c0), or when its time is
up: the splash, then a ground burst without splash. A pass without a bounce
reuses the last bounce length (the original's stack; 0 at first here).

### Bursts (0x5821c0, 0x5820c0, 0x5822c0)

A ground burst casts from 1 m above the shot down to a tenth of
IMPACT_PROXIMITY below it, impacts what it finds, then splashes from 1 m above
(the shot keeps the raised position). REMOTE_DETONATE (0x5820c0) bursts every
stopped shot of the weapon (of one owner, or all) or one at a point, and
tells the network ([0x72fcb4]); 0x5822c0 bursts at a point for no one.

### Craters and quicksand (0x5825b0, 0x5824b0)

On a node with +0x24 bit 0x10000: the crater module's defaults, then points,
radius and depth each the minimum plus rand / 32767 x the span, the material,
the point and the shooter; 0x4ffaa0 makes it (1 when made). Quicksand only on
the default soil, through 0x5020f0 / 0x502110 the same way.

### Node handler slots (0x583220..0x583440, 0x5836b0)

Node +0xbc is a 16-byte table of two {object, function} slots, 0 damage, 1
touch, shared by a subtree: 0x5832f0 sets a slot of the node's own table, or
makes a table (a new node, or splitting from the parent's, the other slot left
empty) and hands it down to every child that had the old one (0x5832a0).
0x5833b0 (flag, then fn) sets the damage slot and with the flag sets a node
flag (0x4eb790); 0x5833f0 the touch slot; 0x583400 frees the node's own table
off its subtree. The touch (0x5836b0): slot 1 with the damage, the shot line
set to the contact (collision's 20 per frame against buildings).

### The impact light pool (0x586970..)

Eight "Thermal glow" lights at load, range 0.1..0.2, a list through node
+0x40; DESIGNATE beams take one (coloured with DESIGNATE's 4th..6th values:
TAG's is green 0.51, 1.0, 0.03), put it on what they touch and give it back.

### Hits (ported)

- Hit 0x586350: the shot line to 0x9a0ec4..0x9a0ed8 (what TakeDamage reads);
  inside RANGE_MINIMUM with a zero second value the shot is a dud, else the
  damage scale x= that value (LRMs 90 m, 0.25: a quarter of the damage and of
  the splash radius) with the min-range animation; then the impact and, with
  IMPACT_PROXIMITY, splash on everyone but the direct target.
- Impact 0x5826b0: the def callback (salvo marks 0x4afe70: a later shot of a
  salvo within its radius of an earlier one, before its delay, skips sound,
  craters and animation; LB-X burst 0x4b44d0; airstrike 0x4afea0), the damage
  DAMAGE x scale through 0x583480, craters / quicksand (also where a kill
  fell), the impact sound, animation, surface animation (oriented to the
  hit's normal), and effect.
- Hand-off 0x583480: the hit node's +0xbc: 0 nothing; 1 a projectile with
  FLYOUT_HEALTH (any damage wears it down, LRM_BG only); else {object,
  function}, called as function(object, def, record, damage). For a mech that
  is 0x4161a0 → **Mech::TakeDamage(def, record, damage)**. The attacker goes
  to 0x9a0edc; the handler reports a kill (0x583710) and the health left
  (0x583730), which pick the kill animation, the damage animation by health,
  or a beam's ground animation.
- Splash 0x5829b0: damage x (1 - d^2 / IMPACT_PROXIMITY^2) x scale (MINE:
  full) to each record of the query (0x4f0ac0, radius IMPACT_PROXIMITY x
  scale), skipping the excluded handler. A queue for more than 31 hits
  (0x9a0ef4) is dead: nothing puts the count there.
- Air burst 0x582320: an impact on the shot's own node at its position, then
  splash on everyone (the shooter included, likely).
- Beam damage 0x583020: factor = max(0.5 (1 + cos(pi d / (RANGE x range
  scale))), DAMAGE_AT_MAX_RANGE), d the beam's length; DAMAGE x factor x the
  beam's scale, once a shot (the beam's flag 2), or x dt for TIMED_DAMAGE.
  Under water the range scale is 0.5: the falloff runs over half the range.

### Beams (`zwep::Beam`, 0x150)

+0 next, +4 prev on the def's list (+0x7c), +8 def, +0xc the owner's node,
+0x10 zone, +0x14 the looping FLYOUT sound, +0x1c light, +0x20 / +0x24 the
weapon's muzzle and direction (followed every frame), +0x28 length, +0x2c the
designate timer, +0x30 age, +0x34 damage scale, +0x38 range scale, +0x3c
segments made (up to 8, 1 for DESIGNATE; the game asks for 1), +0x40 shown,
+0x44 segments (node, point, direction, length), +0x144 / +0x148 the lock
(MULTI_TARGET: a count and an array), +0x14c flags (bit 0 ground effect, bit
1 damage dealt).

- Make 0x584a70: "BeamReflect_%d" nodes with the FLYOUT model, in the world,
  hidden. Free 0x584b40.
- Start 0x581230: the FIRE sound, the FLYOUT sound looping (silent for
  DESIGNATE), the FIRE effect or animation (kept in the fire block's +0x08
  while it fires), the fire state taken (damage and range scale, the ground
  flag, the lock), DESIGNATE's light; onto the list. Stop 0x581400: the loop
  stopped, the FIRE sound again, the animation freed, off the list, the light
  back, the segments hidden.
- The frame 0x5814b0: the length grows toward RANGE x range scale by
  (reach - length) x VELOCITY (1 / TIME_TO_MAX_RANGE) x dt, snapping within
  0.1; each segment is placed, scaled by the PULSE wave ((sin(age x rate x
  2 pi) + 1) / 2 between PULSE's two values) and cast (the owner out of the
  first). What it meets plays the impact sound (every frame) and: with a
  damage handler, takes the damage (0x583020, the beam stopping there), or
  for DESIGNATE adds to the timer and, once past the sixth DESIGNATE value
  (TAG: 1 s), impacts it with a stand-in shot (the airstrike); without one,
  the ground animation (if the beam's ground flag) and, for DESIGNATE, the
  timer counts down and sets the loop's volume. BOUNCES reflects into the next
  segment. Nothing met: DESIGNATE goes silent, its timer and light off.
- MULTI_TARGET (no shipped weapon): with 2..4 locks the targets are sorted
  along the direction to the farthest (0x582c10, on the weapon's own array),
  the chain kinked at random between points 40% of the way to each, with a
  branch to each target (0x582de0 casts each segment, damaging what has a
  handler); one lock kinks once at half way; none is a straight full-reach
  ray.

### Specific weapons

- LB-X: DAMAGE 0, RANGE 30, BURST sets DETONATE_AT_RANGE; each shell bursts at
  30 m (or on impact) into 3 pellets (0x4b44d0): the shell's velocity plus up
  to `radius` sideways at a random angle (a cone of about 1 degree); spawn
  then adds the pellet's own 400 (likely about 800 m/s).
- NARC: a homing missile of DAMAGE 0; TakeDamage marks with ISNARC.
- TAG: the strike (0x4afed0) is 4 TAG_MISSILEs from 1500 back along the beam
  and 400 up, 50 apart, homing on the point.
- AMS (0x4b3ab0..0x4b3c05, weapon.cpp; the mech makes it with range 150,
  period 0.6, 100 rounds, and the configuration step sets 100 rounds per
  AMS): it takes the nearest shot on the radar list within 150 by |dx| + |dz|
  (the player's foes' shots; an enemy's AMS the player side's) and shoots it
  down (health 0) once the period, shared over the AMS count, has passed.
  Only shots on the radar (SHOW_ON_RADAR: the missiles) can be shot down.
- The radar list (0x4b49c0..0x4b4b96): a std::list of {shot, friendly}; every
  frame each radar shot's tick puts itself on it, and the radar object's
  action (priority 2) empties it.

## Discoveries

See `docs/discoveries.md`, Weapons.

## Frame-rate dependence

- Salvos leave one shot a frame.
- A beam grows by (reach - length) x VELOCITY x dt toward its reach: with the
  shipped 10000 it reaches it in one frame at any rate.
- The homing clock stops one frame past LOCK_ON, so the top turn factor of a
  missile with a suspend time depends on that frame's length.
- Beam recharge adds ftol(rate x dt) a frame: at 420 fps and above a plain
  laser never recharges. A port at a high frame rate must carry the fraction.
- Homing slows a turning shot per frame (0.8 + 0.2 cos(turn)), so the loss
  depends on the frame rate (likely less at a high one).

## Vehicles and the configuration step

- The vehicle builder 0x4b4660: a vehicle's `weapons` items are (name 1 1
  ammo aim... "firepoint" (names)). The firepoint name is looked up in the
  whole list (0x5569f0 searches nested lists), not the item, so every weapon
  takes the first item's first firepoint name; the node is found by name
  (0x4eb0d0, then again in 0x4b0460 by 0x4abb60). Ammo 0 is an endless bin
  (beams keep theirs); weapons go in group 4, then 0. Errors at lines
  0xd94 / 0xd97 / 0xd9a.
- The mech's configuration step 0x414501..0x414732: the manager reset, each
  weapon item mounted (an error at 0x1b1 when refused), the configuration kept
  at +0x3ec, then each ammo item into the first weapon of its index (weapons
  that use ammo), equipment counted at +0x3ac (jump jets, kind 11, through
  0x417300), the AMS given 100 rounds per AMS, and TAG (kind 13) mounted as
  weapon 0x20 on the first firepoint's node by name, in group 5 (index 4).
  A TAG mounted this way has no firepoint (the vehicle constructor), and the
  arm check 0x4b2f90 reads through it for a side view (likely a crash the
  game never reaches; the test keeps clear of it).

## Left to port

- The AI's weapon planner 0x4b1170..0x4b1d7f (with 0x4b1bd0 / 0x4b1c70, which
  the HUD gauge's 0x443d20 also calls).
- The glow objects 0x5869d0 / 0x586a30 / 0x586af0 (the game's freeze and heat
  effects, called from 0x4accb7..0x4ad4b5), and the startup 0x583750's network
  registration.
- The engine pieces zWeapon calls, hooked here: cls_di.c's queries (ported
  separately), the crater module (0x4ff920 / 0x4ffaa0 / 0x5020f0 / 0x502110),
  object3d and the animations.
- Wiring: the mech's own setup still has to call `configure_weapons`, build
  the AMS and run the tick; `zwep::tick` is the weapon_tick action.
