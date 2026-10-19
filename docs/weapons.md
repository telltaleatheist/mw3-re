# Weapons at run time (draft)

Research notes, not yet checked in an emulator. Every claim gives the address it
comes from; "likely" marks an inference. Read with `docs/structs.md` (Weapons),
`docs/heat.md` and `docs/types.json` (ZWeaponDef, WeaponType).

## Three layers

| Layer | Made by | Size | What it is |
|---|---|---|---|
| ZWeaponDef | 0x583880 (zwep_init.c) | 0x1f4 | one per `BALLISTICS` entry in `weapons.zrd` |
| WeaponType | 0x4af5a0 | 0xc8 | the MW3 half of each def, at def +0x1f0 |
| MechWeaponTable | data at 0x5e64f0 | 37 x 0x18 | the weapons the mech lab knows: name, message id, family, index, availability, uses ammo |
| WeaponManager | ctor 0x4b00d0, dtor 0x4b01d0 | 0x94 | one per vehicle, at Vehicle +0xd4 |
| WeaponGroup | ctor 0x4b1d80 | 0x54 | five per manager |
| Weapon (projectile) | ctor 0x4b2530, vtable 0x5c9088 | 0x98 | one per mounted weapon |
| BeamWeapon | ctor 0x4b3c10, vtable 0x5c90c0 | 0xa4 | subclass for defs with BEAM (def flag bit 1) |
| AmmoBin | built inline in 0x4b2530 / 0x4b3c10 | 0x18 | ammo pool, or a beam's charge |

### FIRE_RATE is stored inverted

The zwep reader stores `def+0x14 = 1 / FIRE_RATE` (0x583e84: `fld 1.0; fdiv value`).
So def +0x14 is the **period, seconds per shot**. MGUN `FIRE_RATE 4` fires every
0.25 s, AC 0.5 every 2 s, Ultra AC 1.0 every 1 s, LRM/PPC 0.25 every 4 s, SRM 0.2
every 5 s, Gauss 0.4 every 2.5 s. Beams have `FIRE_RATE 0.0001`, so def +0x14 =
10000: for beams it is "charge units per second", not a period (see Beams).
`types.json` calls +0x14 `fire_rate`; `period` would be truer.

### The mech-lab weapon table (0x5e64f0)

37 entries, index 0..0x24, 6 dwords each: name, message id, family, index,
availability (1/2/3, likely clan/sphere/both), uses-ammo byte (+0x14).
`0x4848a0` maps a name to its index (stricmp), `0x484900` index to name ("noweapon"
if out of range), `0x484960` reads uses-ammo. The table has AC2..AC20, the ER, plain
and pulse lasers, ERPPC, Flamer, Gauss, LB2..20X, LRM5..20, MGUN, NARC, SRM2..6,
Streak SRM2..6, TAG (index 0x20) and the four Ultra ACs.

### WeaponType fields (0x4af5a0), adding to types.json

| Offset | Key | Notes |
|---|---|---|
| +0x00 | — | mech-lab index (0x4848a0 on the def name), -1 if not a mech weapon |
| +0x08, +0x0c | HEAT | first value is the firing heat |
| +0x10 | SHOW_ON_RADAR | |
| +0x14, +0x18 | ZAPS_HUD | two numbers (int or float) |
| +0x1c, +0x20, +0x24 | MASS `clan` (tons, slots), present flag | slots via ftol |
| +0x28, +0x2c, +0x30 | MASS `sphere` (tons, slots), present flag | |
| +0x34 | — | tons per round = 1 / +0x38 (0x484bc0 returns it to the mech lab) |
| +0x38 | MASS `ammo` | rounds per ton, **multiplied by SALVO truecount** when SALVO exists: LRM20 6 x 20 = 120, MGUN 50 x 4 = 200, matching the dfn files |
| +0x3c | SALVO count | 1 if SALVO exists without count; 0 without SALVO |
| +0x40 | SALVO delta | 0.1 default; not used in weapon.cpp (likely GameZ) |
| +0x44 | SALVO truecount | defaults to count |
| +0x48 | IMPACT_FORCE | |
| +0x4c | ISNARC | |
| +0x50, +0x54, +0x58 | BURST radius, count, `wep` (a ZWeaponDef*) | BURST also adds the burst weapon's range to +0x70 and sets def flag bit 25 (DETONATE_AT_RANGE) |
| +0x5c, +0x60 | U_LOOPED, U_RECHARGE_SND | sounds |
| +0x64..+0x6c | NAMESOUND | three sounds |
| +0x70..+0x7c | range, range², range/2, (range/2)² | the half range is used by beams under water (see below) |
| +0x80 | — | constructor argument (byte) |
| +0x8c, +0x90 | SALVO radius (squared), delay | when both > 0 a GameZ callback is registered (0x584de0) |
| +0x94 | RECOIL | applied to the firing part |
| +0x98 | U_RECHARGE_TIME | **stored converted**: `period * U_FIRE_TIME / U_RECHARGE_TIME` (0x4af9be) = charge units regained per second |
| +0x9c | U_FIRE_TIME | 0.5 default |
| +0xa0 | U_DESIGNATE_AIRSTRIKE | a ZWeaponDef* (TAG calls in `TAG_MISSILE`) |
| +0xc0 | — | LRM5..20 only (index 0x11..0x14): `range * 0.7 / clamp(velocity, 100, 5000)` = flight time to 70% of range; use not traced (likely arming or RANGE_MINIMUM) |

## The manager (Vehicle +0xd4)

Made in the mech constructor (0x414daf, `new 0x94`, `0x4b00d0(this)`) and, for other
vehicles, by 0x4b4660 (weapon.cpp, reads the vehicle's `weapons` list).

| Offset | Field |
|---|---|
| +0x00 | trigger down now (0x4b0c20 copies it to +0x01 first) |
| +0x01 | trigger down last frame |
| +0x04 | trigger state this frame: 1 pressed, 2 held, 3 released (0x4b0c30) |
| +0x08 | alpha-strike end time (game time), 0 when none |
| +0x0c..+0x14 | aim point (vec3) |
| +0x18 | aim point valid (0x4b0bc0) |
| +0x1c | fire mode: 0 chain, 1 single, 2 group (likely names; see Firing) |
| +0x24 | weapon count |
| +0x28 | selected group, -1 none |
| +0x2c..+0x3c | WeaponGroup*[5] |
| +0x40..+0x7c | Weapon*[16] |
| +0x80 | the owner as a mech/entity (owner vt+0x2c, then +0x50); compared with the player `[0x5f4c8c]+0x64`. +0x80 +0x3ec is the mech's configuration |
| +0x84 | the owner vehicle (constructor argument) |
| +0x88 | byte, gates 0x4b3330 (likely "has a target") |
| +0x89 | weapons online (1 at start; 0x4b0bf0(0) releases the trigger and clears the aim) |
| +0x8c | sum over bins of `ammo * damage` (AmmoBin::SetAmount keeps it) |
| +0x90 | 1 / the largest that sum has been (likely a HUD "ammo left" ratio) |

Main methods: 0x4b0400 add weapon from a config entry, 0x4b0460 add weapon by
index, 0x4b03c0 find weapon by mech-lab index, 0x4b0c30 per-frame update,
0x4b04f0 / 0x4b0610 next / previous group, 0x4b0730 select group, 0x4b0790 /
0x4b07e0 next / previous weapon in group, 0x4b0a70 alpha strike, 0x4b0ae0
jettison, 0x4b0de0 crit / cook-off, 0x4b0220 unlimited ammo, 0x4b2da0 per-frame
arm restriction.

### WeaponGroup (0x54)

+0x00 manager, +0x04 index 0..4, +0x08 count, +0x0c current index (-1),
+0x10 current Weapon*, +0x14 Weapon*[16]. Add 0x4b1eb0 (max 16), remove 0x4b1ef0,
find 0x4b1db0, select 0x4b1de0. Next / previous (0x4b1f40 / 0x4b2000) skip weapons
with no ammo (vt+0x14 == 0) and destroyed ones. A weapon's group is Weapon +0x60;
`Weapon::SetGroup` (0x4b1e10) moves it between groups.

### AmmoBin (0x18)

| Offset | Field |
|---|---|
| +0x00 | reference count |
| +0x04 | amount now |
| +0x08 | amount max (grows when ammo is added past it) |
| +0x0c | infinite: amount and max read back as -1 (0x4b4cf0, 0x4b4d00) |
| +0x0d | "out of ammo" sound already played |
| +0x10 | manager (set on first use, 0x4b4cc0) |
| +0x14 | the def's DAMAGE, for the manager's +0x8c sum |

`AmmoBin::SetAmount(n)` 0x4b4d10, `Add(n)` 0x4b4dc0.

**One bin per weapon type per mech.** The weapon constructor (0x4b2530, loop at
the end) looks for another weapon on the same manager with the same ZWeaponDef and
shares its bin (refcount + 1). So both LRM20 launchers on a Mad Cat draw from one
pool of 240 missiles, whatever torso the ammo was listed in.

Weapons that do not use ammo by the mech-lab table (lasers, PPCs, Flamer, TAG)
get an infinite bin (0x4b2230: uses-ammo false → bin +0x0c = 1). A beam gets its
own unshared bin holding its charge (0x4b3c10).

## From the dfn to weapon instances

`dfn_<mech>.zrd` `configurations` → each configuration's `weapons` list, read in
0x46ba40 at 0x46c6bc..0x46cb22:

```
weapons ( (NAME FIREPOINT "group" (G ...) ["ammo" (N "part" (PART) ["extra" (N2 [N3 [N4]])])]) ... )
```

- More than 16 entries: error "Too Many Weapons (%d>%d)" (0x5e51f4) and only 16
  are read (0x46c6e0).
- NAME → mech-lab index (0x4848a0); unknown names are skipped with "Unrecognized
  Weapon (%s)" (0x5e51d8).
- FIREPOINT is looked up in the mech's `firepoints` list (records of 0x20: +0 name,
  +4 `share`, +8 `radius`, +0xc part index, read at 0x46c065..0x46c0f2). An unknown
  name makes a new record with radius -1 and part -1 (0x46c828).
- `group`: the first number only (0x556ae0 reads one int), minus 1, clamped to
  0..4 (0x46c91f).
- Each weapon becomes a config item, vtable 0x5c0fc0, 0x1c bytes: +4 = 1 (count),
  +8 group, +0xc firepoint, +0x14 index; item kind (vt+0xc) is 1.
- `ammo (N "part" (P))` makes an ammo item, vtable 0x5bd150, 0x18 bytes: +4 = N,
  +0xc = part, +0x14 = weapon index, kind 2. Each `extra` number makes one more
  ammo item in the **same part** (0x46ca55..0x46cb08, up to three).
- `equipment` items are kind 3 with +0x18 = equipment kind (table at 0x5e6868, 5
  dwords each): 0 ams, 1 armor, 2 armorff, 3 fcs, 4 bap, 5 case, 6 c3, 7 c3slave,
  8 ecm, 9 sink, 10 dsink, 11 jjets, 12 masc, 13 tag, 14 tc.

When the mech takes a configuration (0x414501..0x414732, in the mech's
configuration setup):

1. The manager is reset (0x4b0290).
2. Every weapon item → `WeaponManager::AddWeapon` 0x4b0400 → 0x4b2230 picks the
   class from def flag bit 1 (BEAM): BeamWeapon (0x4b3c10) or Weapon (0x4b2530),
   copies SALVO count to Weapon +0x64, and the weapon joins its group.
3. Every ammo item whose weapon uses ammo → the first weapon of that index
   (0x4b03c0) → its bin `Add(N)` (0x41461b).
4. Equipment: kind 11 (jump jets) calls 0x417300(1); every other kind adds 1 to
   the count at mech +0x3ac + 4·kind (+0x3b8 fcs, +0x3c0 case, +0x3cc ecm, +0x3d8
   jump jets, +0x3dc masc, ...). Kind 13 (TAG) also mounts weapon 0x20 (TAG) through
   0x4b0460 into group 5 (0x4146b9). Kind 0 (AMS) is handled at 0x4146f9.

The mech is at +0x3ec of the configuration list afterwards (0x414594).

Vehicles that are not mechs (0x4b4660) list `(NAME [ammo] ["firepoint" NAME])`;
every weapon goes to group 1, and an ammo weapon given no ammo gets an infinite
bin (0x4b4787..0x4b47cd).

### Weapon fields (0x98, projectile; BeamWeapon adds +0x98..+0xa0)

| Offset | Field |
|---|---|
| +0x08..+0x10 | lock target, time, its id (0x4b3370) |
| +0x14 | locked target for the projectile |
| +0x18 | rating: damage per second, `damage / period` (beams `damage / (fire + recharge time)`); BURST weapons use `burst count * burst damage` |
| +0x1c | from the config item +0x24 |
| +0x20, +0x24 | the firing part's node, the part (0x41d510 on the firepoint's part) |
| +0x28 | mech-lab index |
| +0x2c | ZWeaponDef* |
| +0x30 | reload timer (s); `CanFire` needs <= 0 |
| +0x34..+0x54 | aim: target point, muzzle position and direction (0x4b36e0) |
| +0x58 | AmmoBin* |
| +0x5c | locked (result of 0x4b3370) |
| +0x5d | the firing part carries FCS |
| +0x60 | group |
| +0x64 | SALVO count (0 = not a salvo weapon) |
| +0x68 | salvo shots still to fire |
| +0x70 | damage 0..1; 1 = destroyed (0x4b2d50 adds, 0x4b3020 handles destruction) |
| +0x74 | enabled (arm restriction and water, 0x4b2f90) |
| +0x78, +0x7c | looped fire sound, recharge sound |
| +0x80 | firepoint record |
| +0x84..+0x8c | random offset inside the firepoint radius |
| +0x90 | config item |
| +0x94 | manager |
| beam +0x98 | firing now |
| beam +0x9c | target point |
| beam +0xa0 | the GameZ beam object (0x584a70) |

Firepoint sharing: a weapon without SALVO truecount that shares a firepoint with a
weapon already on the manager gets a random muzzle offset of up to `radius` in x
and y (0x4b2530, `rand()` scaled by 1/16383.5 - 1).

### Vtable (projectile 0x5c9088 / beam 0x5c90c0)

| Slot | Projectile | Beam | Meaning |
|---|---|---|---|
| +0x00 | 0x4b28b0 | 0x4b3d50 | destructor |
| +0x04 | 0x4b3450 | same | Fire one shot |
| +0x08 | 0x4e42d0 (false) | 0x4b3d30 (true) | IsBeam |
| +0x0c | 0x4e42d0 (false) | 0x4b3d40 | IsFiring (beam +0x98) |
| +0x10 | 0x4b4de0 | 0x4b4e50 | damage per second, with salvo (AI rating, likely) |
| +0x14 | 0x4b2830 | 0x4a5260 (-1) | ammo left (-1 infinite) |
| +0x18 | 0x4b2c20 | 0x4b4230 | Update(dt) |
| +0x1c | 0x4b3370 | same | lock on |
| +0x24 | 0x4b3660 | 0x4b3f40 | aim and fire if CanFire |
| +0x28 | 0x4b3190 | 0x4b3fc0 | trigger pressed |
| +0x2c | 0x4b3260 | 0x4b4180 | trigger released |
| +0x30 | 0x4b3280 | 0x4b41b0 | trigger held |
| +0x34 | 0x4b2840 | 0x4b4470 | ready fraction for the HUD, 0..1 |

## Firing

### Trigger and modes

The pilot's commands (handler 0x41be80, ids from the binding table 0x411150):

| Command | id | Code |
|---|---|---|
| CMD_TRIGGER1 | 0x3d | sets manager +0 (0x4b0c20) |
| CMD_WEP_NEXT / PREV | 0x25 / 0x26 | 0x4b0790 / 0x4b07e0 |
| CMD_NEXT_WEAPON | 0x27 | selects the HUD's highlighted weapon and its group (0x41c370) |
| CMD_FIREMODE | 0x28 | mode = (mode + 1) % 3, then a mode sound (0x4b0830) |
| CMD_WEPGRP_NEXT / PREV | 0x2a / 0x2b | 0x4b04f0 / 0x4b0610: skip groups with no beam and no ammo |
| CMD_WEPGRP_1..5 | 0x2c..0x30 | select group (0x4b0730; refused if empty) |
| CMD_MOVETOWG_1..5 | 0x31..0x35 | move the current weapon into group n and select it (0x41c481) |
| CMD_ALPHASTRIKE | 0x39 | 0x4b0a70(0.2), refused while powering down or down (mech vt+0xa0 = 0x41cc10) |
| CMD_JETTISON_AMMO | 0x3b | 0x4b0ae0, then mech sound 0x10 |

Per frame, `WeaponManager::Update(dt)` (0x4b0c30, called from the mech update at
0x41a814) runs every weapon's Update, then:

- **Pressed** (down now, not last frame): state 1, `WeaponGroup::Trigger`
  (0x4b2100). Mode 2 fires every weapon in the selected group (vt+0x28); modes 0
  and 1 fire only the group's current weapon. If nothing fired, the press is
  dropped (+0 cleared).
- **Held**: state 2, `WeaponGroup::Held` (0x4b21f0, vt+0x30) on the same weapons.
- **Released**: state 3, vt+0x2c on them; in mode 0 the group then steps to its
  next weapon with ammo (0x4b1f40). So mode 0 is chain fire, 1 stays on one
  weapon, 2 fires the group (names likely). The manager starts in mode 0
  (0x4b00d0).
- Alpha strike: every weapon on the mech gets vt+0x28 at once, +8 = now + 0.2;
  after 0.2 s every weapon is released (0x4b0ab0).

In group mode, when more than one beam is ready, every ready beam except the one
with the highest +0x18 clears `[0x9a17cc]` before firing (0x4b2100) — likely so
only one beam plays its effect or sound.

### CanFire (0x4b3a20)

All of: def present, weapon enabled (+0x74), weapons online (manager +0x89), reload
timer <= 0, ammo >= 1 or infinite, damage < 1.0.

### Projectile weapons

- Trigger (0x4b3190): aim at the manager's aim point and fire if CanFire
  (vt+0x24 with force 0). If it fired and the weapon has SALVO, +0x68 = count - 1.
  If it did not fire, the bin is empty and the owner is the player, play the
  "out of ammo" sound once (bin +0x0d).
- Held (0x4b3280): a SALVO weapon whose reload is done triggers again (a new
  salvo); others try vt+0x24. So holding the trigger refires each weapon as soon
  as it reloads.
- Update (0x4b2c20): reload timer -= dt; the recharge sound plays for the player's
  own mech while reloading (WeaponType +0x60). While +0x68 > 0, **one more salvo
  projectile is fired per frame**, re-aimed at the aim point, and +0x68 drops.
- Fire (0x4b3450), once per projectile:
  - lock-on (0x4b3370) when def flag bit 15 is set (likely LOCK_ON). When the
    mech has FCS (mech +0x3b8 >= 1) **in the firing part** (0x460c10(3, part)) and
    the target has no ECM (target +0x3cc, ECM count, is 0), it writes 2.0, 2.0 and
    1 to `[0x9a17d8]`, `[0x9a17dc]`, `[0x9a17e0]` — likely doubled missile
    tracking. This is the only FCS effect found.
  - spawns the projectile (0x4b4a40) with a friendly-fire mask (owner +200 &
    player side +0xc0).
  - reload timer = period (def +0x14), so the reload starts after the last salvo
    projectile.
  - ammo: only on the first projectile of a salvo (+0x68 == 0). It takes 1, or
    SALVO truecount when the weapon has SALVO, capped at what is left, and lowers
    manager +0x8c.
  - heat and recoil: `MechPart::WeaponFired` (0x461670) on the firing part.
  - for the player and when `[0x608620]` is set, 0x433630(index) (likely shot
    statistics).

So a SALVO weapon fires `count` projectiles (one per frame), each doing DAMAGE and
each adding HEAT and RECOIL, for `truecount` rounds of ammo. LRM20: 5 projectiles of
5.0, 20 missiles of ammo, 5 x 0.06 = 0.30 heat a volley. AC20: 5 shells of 6.5 for
one round, 0.238 heat. (That the damage is per projectile is likely; the damage
itself is applied by GameZ, not traced.)

### Beams

A beam's bin is its charge, in units where def +0x14 = 1 / FIRE_RATE = 10000 a
second for every shipped beam:

- full charge = ftol(U_FIRE_TIME x 10000) (0x4b3ce7): 1250 for the plain and ER
  lasers (0.125 s), 20000 for pulse lasers (2.0 s), 10000 for TAG (1.0 s).
- Trigger (0x4b3fc0): when U_RECHARGE_TIME is set, **only from a full charge**.
  Takes one unit, starts the GameZ beam (0x581230), sets firing, adds the heat
  once (0x461670), updates the HUD. Under water (part +0x49, likely) it writes 0.5
  to `[0x9a17d4]` (likely half beam damage or width).
- Update (0x4b4230), n = ftol(max(10000 x dt, 1)):
  - firing: charge -= n; at 0 the beam stops (0x4b4140). TIMED_DAMAGE beams add
    `heat x n / full` to the mech every frame. So a beam burns for U_FIRE_TIME.
  - not firing and not full: charge += ftol(WeaponType +0x98 x f x dt), capped at
    full, where +0x98 = 10000 x fire / recharge time, so empty to full takes
    U_RECHARGE_TIME. **f is the heat factor**: 1 up to heat 0.5, then 0.5 / heat,
    never below 0.5 (0x4b43bf: mech vt+0x9c = 0x41ec90 returns heat +0x554). At
    heat 1.0 and above lasers recharge at half speed.
- Released (0x4b4180): a beam with U_FIRE_TIME over 0.5 s (the pulse lasers)
  stops at once; short beams always finish.
- Held (0x4b41b0): not firing → trigger again (fires as soon as full); firing →
  keep steering the beam at the aim point.

Ready fraction for the HUD: projectile `(period - timer) / period` (0x4b2840),
beam `charge / full` (0x4b4470).

### Heat (0x461670, `MechPart::WeaponFired`)

ECX is the firing part (Weapon +0x24); the mech comes from the part (0x460000).
Non-beam: `AddHeat(WeaponType.heat)` then part vt+0x24(WeaponType.recoil). Beam:
`AddHeat(heat)`, or `heat x [0x72c52c]` (likely the frame dt) for TIMED_DAMAGE;
beams have no recoil. `AddHeat` (0x419e40) adds to mech +0x558, applied by the heat
model at the end of the frame (`docs/heat.md`).

Correction for `docs/heat.md` / `symbols.csv`: 0x4b4230 is `BeamWeapon::Update`;
"scaled by the share of ammo used" is the beam's charge drained this frame.

### What limits the rate of fire

- Projectiles: the reload timer = 1 / FIRE_RATE, restarted by every projectile.
- Salvo spacing: one frame (no timer; SALVO `delta` is not read by weapon.cpp).
- Beams: full charge needed; recharge time = U_RECHARGE_TIME, slowed by heat.
- Everyone: CanFire's ammo, damage, weapons-online and arm/water checks.

### Arm restriction and water (0x4b2da0 → 0x4b2f90, every frame)

For the player: when the camera's view (`[0x5f4c8c]+0x58` vt+0x1c, +0x40) is 0 or
1, or in the control mode `controls+0xc == 1` with the torso turned more than 35°
(`cos([0x5e8690])`) from the view, only weapons whose firepoint is on part 2 (left
arm) or 3 (right arm), the side chosen by the cross product, stay enabled. Likely
the look-left / look-right views and free look. Separately, a **non-beam weapon in
a part with +0x49 set is disabled**, and a beam there uses half range (0x4b39a0,
WeaponType +0x78). Part +0x49 is likely "under water".

Part indices (0x484e40): 1 torso, 2 larm, 3 rarm, 4 lleg/lthigh, 5 rleg/rthigh,
6 head, 7 ltorso, 8 rtorso, 9 lhand, 10 rhand, 11 lcalf, 12 rcalf, 13 lfoot, 14
rfoot, 15 hip.

## Crits, cook-off and jettison: 0x4b0de0(part, flag)

ECX is the manager. It picks the candidate with the **smallest** value, starting
from HUGE_VAL (0x4b0deb).

**flag 0 — weapon destroyed** (crit type 1, 0x460fc2). Candidates: weapons on the
manager whose part id (Weapon +0x24 +8 +0x28) is `part`, not already destroyed.
Value: `|ammo x rating|` (+0x18); for pure beams (flag bit 1 and not bit 2) just the
rating. The winner takes 0x4b2d50(1.0) → destroyed (0x4b3020: stops it, the
player's HUD and "weapon destroyed" voice 0x4853d0, reselection), then 0x4b0880.

**flag 1 — ammo explosion** (crit type 2 at 0x460e30, and the heat cook-off at
0x419b5a with part -1). Candidates: the ammo items in the mech's configuration
(manager +0x80 → +0x3ec) in `part` (any part for -1). For each, the first weapon of
that index (0x4b03c0, not checked for destruction) and its bin; skipped when the
bin is empty. Value: `|bin amount x rating|`. The winner's bin is set to 0
(`AmmoBin::SetAmount(0)`, 0x4b0f2c), then 0x4b0880.

Then the callers only play sounds: mech sound 15 (0x41e770, ECX 0xf) at
0x419b78 and 0x460e97, and the player's HUD note 0x446520. **Nothing in
0x4b0de0, SetAmount or these callers applies damage**: in this code an ammo
explosion empties the ammo and plays a sound. (The crit dispatcher's own caller
is in damage territory and was not checked for a follow-up.)

CASE: crit type 2 first asks 0x460cc0(5) whether the hit part carries CASE
(equipment kind 5, via 0x460c10 on the configuration). With CASE the crit does
nothing at all (0x460e3b jumps out). The heat cook-off does not check CASE.
Inside 0x4b0de0 a second CASE test (kind 3 item with +0x18 == 5) calls vt+8 and
ignores the result (0x4b0ee9): dead code.

**Jettison** (0x4b0ae0): every non-beam weapon in the current group (mode 2) or
the current weapon gets `SetAmount(0)` on its bin.

## Discoveries

- **Ammo is pooled per weapon type.** All launchers of one type share one bin
  (0x4b2530). Ammo listed in the left torso feeds the right-torso launcher, and
  where ammo sits only decides which crit can reach it.
- **An ammo explosion costs ammo, not armour**: the bin is emptied and a sound
  plays (0x4b0de0, 0x419b5a, 0x460e30); no damage call was found. And because the
  bin is shared, one crit empties that weapon type's ammo across the whole mech.
- **CASE stops ammo crits completely** in its part (0x460e30), but **not heat
  cook-offs**, which pick any part (0x419b56 passes -1).
- **The cook-off and the weapon crit choose the least valuable target**: the bin
  or weapon with the smallest ammo x damage per second. An empty weapon (value 0)
  is the first one a weapon crit destroys.
- **Lasers recharge at half speed when hot**: from heat 0.5 the recharge is scaled
  by 0.5 / heat, floored at 0.5 (0x4b43bf).
- **Small laser recharges slower than medium and large**: U_RECHARGE_TIME 4.0 s
  against 3.0 (weapons.zrd).
- **Ultra ACs never jam**: they just have FIRE_RATE 1.0 against 0.5 for the AC;
  nothing random in Fire (0x4b3450).
- **Salvo weapons heat per projectile**: an LRM20 volley adds 5 x HEAT; ammo is
  taken once for the whole salvo (`truecount` rounds).
- **Only the first group number counts**: `group (2 5)` puts the weapon in group 2
  only (0x556ae0 reads one int). 59 weapon entries in 11 shipped dfns list a
  second group (Vulture 10, Cauldron 9, Mad Cat 9, Owens 6, Daishi 5, ...),
  presumably meant as an "all weapons" group, and it is ignored.
- **Weapons can be regrouped in the cockpit** (CMD_MOVETOWG_1..5).
- **Pulse lasers stop when you let go**; other lasers always finish their 0.125 s.
- **FCS only matters in the firing part** and is cancelled by target ECM
  (0x4b3450, 0x4b3370).
- **Under water** (likely part +0x49): ballistic and missile weapons cannot fire;
  beams get half range.
- **Unlimited ammo** is bit 2 of the options word `[0x5f4d04]` (0x40cf30), which
  also sets `[0x609994]` = 1 (likely a "cheated" flag); missions can give single
  mechs unlimited ammo (0x46dc11, from mission data +0x8d).
- **Weapons on the same firepoint spread out** by a random offset within the
  firepoint radius (0x4b2530).
- **`PPC` is defined in weapons.zrd but missing from the mech-lab table**
  (0x5e64f0 has ERPPC only), so no mech can mount it.
- **Data typo, harmless**: weapons.zrd writes `BOUNCE (0)`, the reader asks for
  `BOUNCES` (0x583b9c).
- **Rating quirk**: the rating at Weapon +0x18 is computed before the salvo count
  is set (0x4b2530 reads +0x64 while it is still 0), so it ignores salvos; the
  vt+0x10 version (0x4b4de0) includes them. The TIMED_DAMAGE beam rating squares
  the damage (`damage x fire time x damage`, 0x4b3c10); no shipped beam is timed.
- **Latent overflow**: the weapon array holds 16 and only the dfn reader limits it.
  The TAG equipment adds a weapon through 0x4b0460 without a check, so a 16-weapon
  loadout plus TAG would write the 17th into manager +0x80 (likely never happens
  in shipped data).

## Frame-rate dependence (FIX (B) candidates)

- Salvo projectiles leave one per frame: an LRM20 volley spans 5 frames, 0.17 s at
  30 fps, 0.08 s at 60.
- Beam recharge adds ftol(416.7 x dt) units a frame for a plain laser: 13 at 30 fps
  (3.2 s to full), 6 at 60 fps (3.5 s), 0 at 420 fps and above: **lasers would
  never recharge**. The drain uses max(1, ...), so firing still ends. A port at
  a high frame rate must accumulate the fraction.

## Open

- Damage application, DAMAGE_AT_MAX_RANGE falloff, IMPACT_FORCE, burst pellets and
  lock-on tracking live in GameZ (zweapon, 0x581xxx..0x585xxx), not traced.
- `share` in firepoints, SALVO `delta`, WeaponType +0xc0, `[0x9a17cc]`,
  `[0x9a17d4]`, `[0x9a17d8..e0]` are unexplained.
- The network path (0x4b08c0 from 0x46e887) fires a weapon by index with an action
  0..3 (release, trigger, held, stop) and a target id.
