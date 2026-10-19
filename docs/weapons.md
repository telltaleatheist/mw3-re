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

Left: see the last section.

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
(not ported, reads the vehicle's `weapons` list).

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
When the mech takes a configuration (0x414501..0x414732, mech code, not
ported here):

1. The manager is reset (0x4b0290: weapons and groups remade; group and
   destroyed count stay).
2. Every weapon item → 0x4b0400 → 0x4b2230: the group clamped to 0..4, a
   BeamWeapon (0x4b3c10) or Weapon (0x4b2530); a weapon that uses no ammo by
   the mech-lab table gets an infinite bin; SALVO count to +0x64.
3. Every ammo item → the first weapon of its index → its bin `Add(N)`.
4. Equipment counts; TAG mounts weapon 0x20 into group 5 through 0x4b0460
   (no 16-weapon check); AMS at 0x4146f9.

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

### Projectile (0x5d8)

| Offset | Field |
|---|---|
| +0x00 | next (free list, then the def's active list at def +0x7c) |
| +0x04 | the shooter's node |
| +0x08 | the vehicle's +0x64 (likely the collision mask) |
| +0x0c | scene node |
| +0x10..+0x1c | flyout effects, pooled model |
| +0x20 | state: 1 flying, 0 remove, 2 dormant mine |
| +0x24 | spawn position |
| +0x30 | base velocity (the shooter's) |
| +0x3c, +0x48, +0x54, +0x60 | direction, position, next position, velocity |
| +0x6c..+0x74 | lock: entity, the target's position and velocity pointers |
| +0x78..+0x584 | precomputed static hits (0x28-byte records sorted by distance) |
| +0x588 | homing time |
| +0x58c, +0x590 | speed, maximum speed |
| +0x598 | RANDOM_DEVIATION's drift velocity |
| +0x5a4 | the drift so far: each frame += drift x dt, and held within +-1 (y +-0.75) by pushing the drift back at 10 a second (0x585f07) |
| +0x5b0, +0x5b4 | distance travelled, age |
| +0x5b8 | flyout health, -1 none |
| +0x5bc | damage scale ([0x9a17d0]) |
| +0x5c0, +0x5c4, +0x5c8 | copies of [0x9a17d8], [0x9a17dc] (unread), [0x9a17e0] (terrain following) |
| +0x5cc | spawn's p5 (likely a network id) |
| +0x5d0, +0x5d4 | per-tick callback (radar), its argument |

### Spawn (0x585010)

ECX def, EDX the shooter's node, then x64 pointer, muzzle, direction,
velocity, p5, p6 (`ret 0x18`; the friend flag stays in 0x4b4a40). A network
gate ([0x9a0ec0] and [0x72fcac]); a pool slot or nothing; positions and
direction; the fire globals copied and reset to 1, 1, 1, 0 (so the doubled
FCS tracking a lock writes is always reset before any homing reads it).
Without ACCELERATION and not INSTANT the speed is the def's velocity and the
velocity = shooter's + direction x speed (RELATIVE_SPEED is never tested);
without gravity or lock the static hits along the whole range are cast once.
With ACCELERATION the shot starts at 0.0001 plus the shooter's speed. A lock
copies the weapon's three lock dwords.

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

### Homing (0x585900, not ported)

Steers toward the target's position (LOCK_ON_LEAD blends an intercept,
0x524247, between lead min and max of age). The turn per frame is TURN_RATE x
[0x9a17d8] x factor x dt, factor 0 until TURN_SUSPEND_TIME, then
(t - suspend) / LOCK_ON, held at 1 after LOCK_ON. Tethered shots and FCS
shots (+0x5c8) follow the terrain (a ground query, 0x4eda10): under 10 m
they climb; with no ground under them they expire. Each turn slows the shot:
speed x= 0.8 + 0.2 cos(turn). Until LOCK_ON the shooter's velocity fades out.

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

### Beams (0x581230 start, 0x5814b0 update; not ported)

The beam object (0x150): +0x20 / +0x24 origin and direction pointers, +0x28
length, +0x2c designate timer, +0x30 age, +0x34 damage scale, +0x38 range
scale, +0x3c segments (up to 8, 1 for DESIGNATE), +0x14c flags. The update
grows the length toward RANGE x scale at 1 / TIME_TO_MAX_RANGE, casts a ray
(0x583130 picks the hit, 0x582b10 classifies it: handler, bounce, surface),
and applies 0x583020 to the first object. TAG (DESIGNATE) held on an object
calls the airstrike once a second (the sixth DESIGNATE value, 1.0).

### Specific weapons

- LB-X: DAMAGE 0, RANGE 30, BURST sets DETONATE_AT_RANGE; each shell bursts at
  30 m (or on impact) into 3 pellets (0x4b44d0): the shell's velocity plus up
  to `radius` sideways at a random angle (a cone of about 1 degree); spawn
  then adds the pellet's own 400 (likely about 800 m/s).
- NARC: a homing missile of DAMAGE 0; TakeDamage marks with ISNARC.
- TAG: the strike (0x4afed0) is 4 TAG_MISSILEs from 1500 back along the beam
  and 400 up, 50 apart, homing on the point.
- AMS is not in zWeapon (mech code at 0x4146f9).

## Discoveries

See `docs/discoveries.md`, Weapons.

## Frame-rate dependence

- Salvos leave one shot a frame.
- Beam recharge adds ftol(rate x dt) a frame: at 420 fps and above a plain
  laser never recharges. A port at a high frame rate must carry the fraction.
- Homing slows a turning shot per frame (0.8 + 0.2 cos(turn)), so the loss
  depends on the frame rate (likely less at a high one).

## Left to port

- zWeapon: spawn 0x585010 (with the pool 0x584df0 / 0x584e60 / 0x584f40 and
  the static-hit cast), the tick 0x5823c0 with update 0x585710 and commit
  0x5858b0 / 0x5864c0 / 0x586600, homing 0x585900 (needs the intercept
  0x524247, 0x51e460, acos / asin and the ground query), collision 0x5861f0
  and hit pick 0x583130 / 0x582b10, mines 0x586670 / 0x586740, beams
  (0x584a70 make, 0x584b40 free, 0x581230 start, 0x581400 stop, 0x5814b0
  update, MULTI_TARGET 0x582c10 / 0x582de0), craters 0x5824b0 / 0x5825b0,
  sounds 0x582f60 / 0x582fc0, the node handler slots 0x583220..0x583440, the
  second hand-off 0x5836b0, the light pool 0x586970.., the aim helper
  0x581ff0, the startup 0x583750's network registration.
- weapon.cpp: the vehicle builder 0x4b4660 (a vehicle's `weapons` list), the
  AI's weapon planner 0x4b1170..0x4b1d7f, the TAG strike tracker
  0x4b3ab0..0x4b3c05, the HUD ammo gauge 0x4b0d80, the radar list
  0x4b49c0..0x4b4b96.
- The mech's configuration step (0x414501..0x414732), in mech code.
