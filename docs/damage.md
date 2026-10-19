# Damage (Mech3.exe 1.2 (researched by a subagent from the disassembly, spot-checked: the critical thresholds at 0x460afd-0x460b70 and the 0.15 armor gate, the +0x630 invincibility reads at 0x41ad48 and 0x461386))

Mech3.exe 1.2, 2026-10-19. Research draft: how a mech takes damage, from a
weapon's impact to armor, internal structure, criticals, part loss and death.
Addresses are the 1.2 exe. "Likely" marks a reading not confirmed by a second
source. Ghidra drops ECX/EDX arguments; the asm was checked for every value
quoted here.

## 1. Where armor and internal structure live

Every part (base class constructor 0x45fbe0, base vtable 0x5c2840) keeps its
state as fractions of the starting value, 1.0 = full:

| Part field | Meaning | Set by |
|---|---|---|
| +0x04 | the mech (looked up lazily, 0x460000) | |
| +0x08 | model node; node +0x28 low byte = part id, node +0x40 = owner object | |
| +0x38 | internal structure, fraction | `MechPart::Init` 0x45fe00 = 1.0 |
| +0x3c | internal scale = 100 / IS points (1.0 if IS is 0) | 0x45fe00 |
| +0x40 | armor, fraction | 0x45fe00 = 1.0 |
| +0x44 | armor scale = 100 / armor points (100 if armor is 0) | 0x45fe00 |
| +0x48 | destroyed (byte) | 0x4617f0 |
| +0x49 | foot-in-water flag (0x461730, vtable +0x10) | |

Base-vtable accessors (all take a section argument; the base ignores it):
+0x44 `GetArmor` 0x467340 (+0x40), +0x48 `GetInternal` 0x467350 (+0x38),
+0x4c `GetArmorScale` 0x467360 (+0x44), +0x50 `GetInternalScale` 0x467370
(+0x3c), +0x40 `SetArmor` 0x461640, +0x3c `SetInternal` 0x461510,
+0x38 `ApplyDamage` 0x461300, +0x18 `TakeHit` 0x460970, +0x1c `ApplyImpact`
(0x40baf0, empty in the base), +0x20 `Destroy` 0x461a30, +0x28 `Critical`
0x460d50.

### The torso has six sections

The torso (constructor 0x464000, vtable 0x5c29e0, size 0x1b4) overrides the
accessors so that a section number picks the field (0x4649c0, 0x464a20,
0x4649f0, 0x464a70, 0x464830, 0x464950):

| Section | Location | Armor | Armor scale | Internal | Internal scale |
|---|---|---|---|---|---|
| 0 | centre torso, front | +0x40 | +0x44 | +0x38 | +0x3c |
| 1 | left torso, front | +0x178 | +0x18c | +0x1a0 | +0x1a8 |
| 2 | right torso, front | +0x17c | +0x190 | +0x1a4 | +0x1ac |
| 3 | centre torso, rear | +0x180 | +0x194 | +0x38 | +0x3c |
| 4 | left torso, rear | +0x184 | +0x198 | +0x1a0 | +0x1a8 |
| 5 | right torso, rear | +0x188 | +0x19c | +0x1a4 | +0x1ac |

Front and rear share one internal value; each has its own armor. `ltorso` (7)
and `rtorso` (8) are sections of this object, not parts of their own.
`SetArmor` (0x464950) forces a section's armor to 0 once its internal value is 0.

### Where the numbers come from

**Armor** is read from the active configuration's `armor` block in
`dfn_<mech>.zrd` (`MechDfn` reader 0x46ba40, 0x46c4d0-0x46c6b5) into an
11-slot array at config +0x38:

| Slot | Key | Location |
|---|---|---|
| 0, 3 | `torso` (front, rear) | CT front, CT rear |
| 1, 2 / 4, 5 | `torsolr` (front, rear) | LT/RT front / LT/RT rear |
| 6 | `head` | head |
| 7, 8 | `arms` | left, right arm |
| 9, 10 | `legs` | left, right leg |

`MechPart::Init` (0x45fe00) maps part id to a slot (1 torso→0, 2 larm→7,
3 rarm→8, 4 lthigh→9, 5 rthigh→10, 6 head→6, 7 ltorso→1, 8 rtorso→2); the torso's
own Init (0x4644b0) fills slots 1-5. The reader checks the total against
`floor(16 x mass x f)` (0x424760: f = 1, or for ferro-fibrous 1.2 for Clan and
1.12 for Inner Sphere) and only logs "***ARMOR ALLOCATION ERROR***"
(0x5e5240) on a mismatch. Mad Cat: 12 t Clan ferro, 16 x 12 x 1.2 = 230 =
9 + 36 + 9 + 2x25 + 2x7 + 2x24 + 2x32.

**Internal structure** is not in the data files. It comes from the built-in
table at 0x5e6998: 19 rows of five ints, `tonnage, CT, side torso, arm, leg`,
for 10 to 100 t (row = tonnage / 5 - 2). The values are the BattleTech ones
(100 t: 31/21/17/21; 10 t: 4/3/1/2). Lookups 0x485400 (CT), 0x485430 (side
torso), 0x485460 (arm), 0x485490 (leg) take the roster index in ECX
(Mech +0x684 +0xc) and read the tonnage from the roster (0x4846e0, roster
0x5e6078, 0x3c bytes per mech). The head is hard-coded to 3 (0x45fe9c).
Correction to `startup.md`: the table's first column is tonnage, not head.

Calves, feet, hands and the hip have no armor or IS of their own (the slot
lookup falls to the default, IS 0, armor 0): their `TakeHit` forwards to the
parent part. Calf 0x466f50 and foot 0x462980 go to the thigh, hand 0x467440
to the arm, hip 0x462560 to the torso (0x4647b0, the plain base `TakeHit`).

## 2. The hit path

```
beam:        0x5814b0 / 0x582de0 -> Weapon_Damage 0x583020 -> Damage_Dispatch 0x583480
projectile:  0x5826b0 (damage = def+0x48 x proj+0x5bc)       -> 0x583480
splash:      0x5829b0 (linear falloff, see below)             -> 0x583480
0x583480: node +0xbc = {entity, fn}; fn(ecx=entity, edx=def, collision, damage)
  mech: fn = 0x4161a0 -> Mech vtable +0x164 = Mech::TakeDamage 0x41ac60
    part found (node tagged as part) -> part vtable +0x18 TakeHit(def, coll, dmg, impulse)
      0x460970: scale, maybe roll a critical, else vtable +0x38 ApplyDamage(dmg, section)
        0x461300: armor and internal, then SetArmor / SetInternal
          SetInternal <= 0 -> vtable +0x20 Destroy
```

### Weapon damage before the mech sees it

- Beams, `Weapon_Damage` 0x583020 (ECX def, EDX beam): `DAMAGE` (def +0x48)
  in full if `DAMAGE_AT_MAX_RANGE` (def +0x68) >= 1, else
  `DAMAGE x max(+0x68, (cos(pi x len / (k x RANGE)) + 1) / 2)`, where len is
  the beam's current length (+0x28) and k its range factor (+0x38): a cosine
  curve from 100 % at the muzzle to `DAMAGE_AT_MAX_RANGE` at full range (0.75
  for every shipped laser, 0.25 when the key is missing, 0x583b80). Timed
  beams (flag 0x4) deal it x frame time every frame; others deal it once per
  beam (bit 1 of +0x14c), then 0 until the next shot.
- Projectiles, 0x5826b0: `DAMAGE x proj +0x5bc` (a per-projectile multiplier,
  likely 1). No range falloff.
- Splash, 0x5829b0: for each other object in the blast,
  `DAMAGE x (1 - dist / def+0x40)` (linear to 0 at `IMPACT_PROXIMITY`), or full
  damage when flag 0x4000 (MINE) is set; x proj +0x5bc. The directly hit
  entity is skipped.
- 0x583480 with node +0xbc == 1 (not a handler) subtracts the raw damage from
  owner +0x5b8: buildings and props use unscaled points, likely.
- The global pre-hit hook 0x9a17a4 is only ever cleared (0x5837c1); never set.

### Mech::TakeDamage (0x41ac60, Mech vtable +0x164)

1. Ignored if the mech is dead (state +0x578 == 10) or inactive (+0x08 == 0).
2. Records the attacker at +0x664 (killer for the kill credit and the network
   death message) and tells the controls/AI (+0x37c, vtable +0x94).
3. `HEAT` second value (WeaponType +0xc, x frame time for beams) is added to the
   target's heat at the end (0x419e40). It is 0 in every shipped weapon.
4. NARC (WeaponType +0x4c): marks the mech (0x41ac10(1)) and returns. NARC
   hits do no damage.
5. Knockback: `IMPACT_FORCE` (WeaponType +0x48; 0 when invincible) along the
   shot's horizontal direction is added to the vector at +0x540 and its length
   kept at +0x53c (only in movement states 1-9). Each frame 0x417a80 compares
   that length with `+0x6c4 + +0x6c8 x (tonnage - 10) / 90` (3.76 + 11.28 x ...,
   so 3.76 for 10 t up to 15.04 for 100 t, set at 0x414c71) and, when reached,
   staggers the mech (0x41b7f0, sound 1 for the player) and clears it.
6. +0x411 = "hit by a weapon whose name is message 0x305, NUKE".
7. If the hit node is a part: part `TakeHit`. Otherwise (a hit on the mech that
   is not on a part node, likely area damage) and the attacker is not the mech
   itself: `d = damage x 0.143 x 0.0062` (x difficulty for the player), and
   `ApplyDamage` is called on all 7 non-head locations: CT, LT, RT front with
   d, their rear sections with 0.2 d, both legs and both arms with d. 0.143 is
   about 1/7. The head never takes this damage, and it never rolls criticals.

### MechPart::TakeHit (0x460970, vtable +0x18)

Arguments: weapon def, collision, damage, impulse.

1. `d = damage x 0.0062` (0x5e1f68). Legs multiply by 1.0 first (0x463b40,
   0x5e1f74), so there are no per-location multipliers.
2. Difficulty: except in multiplayer (game type 3), damage to the player's mech,
   and to mechs that pass a team-mask test against the player's
   (`player +0xc4 & target +0xc0`, likely allies), is multiplied by
   0x608e5c. 0x40cc20 sets it from the difficulty table: low 0.7, medium 1.0,
   high 1.3 (the third float of each row; the others go to 0x608e54/58).
3. `ApplyImpact` (vtable +0x1c) with `IMPACT_FORCE`.
4. The torso picks its section from the shot (0x460350, below).
5. Critical roll (section 3). If one is rolled, `Critical` runs and **the hit
   deals no armor or internal damage at all**.
6. Otherwise `ApplyDamage(d, section)`.
7. Head hits on the player trigger a HUD effect (0x508da0).

Torso, arm and head wrappers (0x464720, 0x466ba0, 0x467200) also play the
weapon's `ZAPS_HUD` static (WeaponType +0x14) on the player's HUD (0x43be70,
kind 4 torso and head, 3 arms).

### Front or rear: 0x460350

The torso builds 8 edges of its footprint in its own space, transforms them to
the world, and tests them in a fixed order against the shot line in the
horizontal plane (2D segment test 0x460780). The box is +-w wide (+0x7c) and
+-d deep (+0x80); the centre strip is +-0.2 w (0x5e4ef0). The shot line is the
collision segment stretched 10 times both ways (0x5e4ef4), so it crosses the
whole torso:

| Order | Edge | Section |
|---|---|---|
| 0 | centre, z = -d | 0 CT front |
| 1 | centre, z = +d | 3 CT rear |
| 2 | left, z = -d | 1 LT front |
| 3 | left, z = +d | 4 LT rear |
| 4 | right, z = -d | 2 RT front |
| 5 | right, z = +d | 5 RT rear |
| 6 | left side, x = -w | 1 LT front |
| 7 | right side, x = +w | 2 RT front |

The first edge crossed wins; none crossed means section 0. If the chosen side
torso is destroyed (internal 0), the hit moves to the centre torso, front or
rear to match (`(s < 3) - 1 & 3`).

### ApplyDamage (0x461300, vtable +0x38)

With `a` = armor, `h` = internal, `A`/`S` their scales (100 / points):

```
a' = a - 0.62 x A x d                  (0x5e4ef8)
h' = h - (1.2 - a) x S x d             (0x5e4efc; uses the old armor)
a' clamped to [0, 1]
h' = min(h', 1), and at least m:
     m = a'                             (limbs, head)
     m = min(a', armor of the other face)   (torso: s +- 3)
```

then `SetInternal(h')` and `SetArmor(a')`. With invincibility (Mech +0x630)
`h'` is clamped to [0.01, 1] instead and the floor is skipped.

In points, a hit of weapon damage D (difficulty 1.0) removes `0.3844 x D`
armor points and `0.62 x (1.2 - a) x D` internal points: 0.124 D with full
armor, 0.744 D with none. Internal damage starts with the first hit, but the
internal fraction can never fall below the armor fraction (for the torso,
below the weaker of front and rear armor). See Discoveries.

### Part destruction

`SetInternal` base (0x461510, section 0 only): stores the value, sends network
event 0x15 (Mech vtable +0x168, "section damaged", `network-messages.md`) and
sets Mech +0x410. At 0 or below: internal = 0 and `Destroy(v, 1)` with v = 0
when `rand/32767 > 0.4` (60 %) else 1 (40 %); v picks one of two destruction
effects from the owner's table (+0x1a4 + 0xc x id + 4v); 0x586ce0 returning true
forces v = 0. Torso `SetInternal` (0x464830): centre torso at 0 →
`Destroy(0, 1)` on the torso; left torso at 0 → left arm `SetInternal(0, 0)`;
right torso → right arm.

`Destroy` base (0x461a30) and 0x4617f0:
- every weapon mounted on the part is destroyed (0x4b1040 → 0x4b3020);
- arms and thighs fly off as a separate object with an effect (vtable +0x60),
  except on the Elemental (roster index 7), where the effect plays at the mech;
- 0x4617f0: destroyed flag (+0x48), armor and internal = 0, and for each
  equipment item in that part with a count: heat sinks (types 9, 10) are lost
  (`HeatSinks::Lose(count)`), any other type decrements the mech's per-type
  counter at Mech +0x3ac + 4 x type by one (jump jets +0x3d8 = type 11, MASC
  +0x3dc = type 12);
- 0x41b5f0: weapons on the part again; on the Elemental, any part loss calls
  torso `Destroy(1, 1)` (death).

Per part:

| Part | Destroy | Effect |
|---|---|---|
| Arm 0x466c50 | base, then the hand (+0x74) is removed from the mech's list (+0x384) and deleted | weapons and equipment in the arm lost |
| Side torso | (section of the torso) | its armor locks at 0, its arm is destroyed, later hits go to the CT |
| Thigh 0x463f70 | base, then calf (+0x74) and foot (+0x78) lose their equipment and hide | the mech falls (0x41b640) and **dies** (Mech vtable +0x70) |
| Head 0x4671e0 | marks itself destroyed, then torso `Destroy(0, 1)` | **death** |
| Centre torso 0x465f40 | death animation and effect, 0x4617f0, Mech vtable +0x70 | **death** |

`Mech::Die` (0x41b0d0, Mech vtable +0x70): state +0x578 = 10 (previous kept at
+0x57c), movement zeroed, killer (+0x664) told through its vtable +0x80, network
death message 0x13 with the killer for the player, mission-failure hook
(0x40ae70(2)) for the player, looping sounds stopped, controls stopped
(+0x37c vtable +0xa8). If the player was killed by the NUKE every armor value is
zeroed for the display.

Centre-torso death animation (0x465f40): a NUKE kill or an Elemental explodes in
place (effect 1); otherwise, if airborne (Mech vtable +0x144) or moving
backwards faster than 13.89 m/s (50 km/h), animation 1 / effect 2; at up to
50 km/h forwards, animation 7 or 8, alternating through the global 0x605cc0;
faster, animation 2 / effect 3 (likely a forward crash).

Death conditions found: centre torso internal 0; head internal 0; either thigh
destroyed; head critical (below); any part on an Elemental; heat (`heat.md`);
a mission script's `Mech::DestroyPart` (0x41cfa0, called from zrdrvalve.cpp
0x4e34e1 and the heat code 0x419bde). Arm or side-torso loss is never fatal,
and no XL-engine rule was found in this path (the `xl` key is stored at config
+0x28, 0x46c4eb; its readers are not traced).

## 3. Critical hits

### What rolls one (0x460970)

All of: the mech is not invincible (+0x630), the hit part's +0x40 armor is at
most 0.15, there is a weapon def, and the weapon is not a BEAM (flag 0x2). Then
one `rand()` (r = rand x 3.051851e-05):

| r | Chance per eligible hit | Critical |
|---|---|---|
| < 0.008 | 0.8 % | 0 heat sink |
| 0.008-0.013 | 0.5 % | 2 ammo |
| 0.013-0.016 | 0.3 % | 3 equipment |
| 0.016-0.019 | 0.3 % | 1 weapon |
| 0.019-0.02 | 0.1 % | 4 location special |
| >= 0.02 | 98 % | none, normal damage |

BEAM weapons are all nine lasers, `mine_turr` and TAG (weapons.zrd), so lasers
never cause criticals. PPCs, autocannons, missiles and gauss can.

### MechPart::Critical (0x460d50, vtable +0x28; torso 0x4647d0, thigh 0x463ef0, calf 0x466fc0 override)

Returns early if the mech is inactive. `player` below = the mech is the local
player and has a HUD (0x430aa0).

- **0 Heat sink.** `HeatSinks::Lose(1)` (0x45fbc0) on Mech +0x658: one sink off
  the total, whether or not this part holds one. Lose always returns false, so
  the "sink" voice (0x485390, type 9 → voice 10 `snd_equip_sink`) and the
  crit sound never play (`heat.md`).
- **1 Weapon.** `Weapons +0xd4 → 0x4b0de0(part, 0)`: among the part's weapons
  not yet destroyed (+0x70 < 1), the one with the lowest score is destroyed
  (0x4b2d50(1.0)); score = |ammo x weapon +0x18| for ammo weapons, +0x18 alone
  for beams. +0x18 is damage x salvo / fire rate (0x4b2530). So an ammo weapon
  that is out of ammo scores 0 and goes first (likely). Weapon voice 0x4853d0,
  HUD update 0x446520.
- **2 Ammo.** Skipped if the part has CASE (equipment type 5, 0x460cc0 →
  0x460c10). Else `0x4b0de0(part, 1)`: the part's ammo bin (item kind 2) with
  the lowest non-zero |rounds x damage| is set to 0 rounds (0x4b4d10(0)).
  There is no explosion damage: the bin is just emptied. Mech sound 15. The
  heat cook-off uses the same call with part -1 (any bin; 0x419b5a).
- **3 Equipment.** Among the part's equipment (item kind 3) that is intact,
  is not a heat sink (types 9, 10) and has a count, one is picked at random
  weighted by count, marked destroyed (+0x08 = 1) and the mech's counter for
  its type (+0x3ac + 4 x type) decremented. Voice for the type (0x485390 →
  0x5e6870 + 0x14 x type → 0x608fc0).
- **4 Location special**, by part id:
  - torso (1): voice 6 `snd_equip_engine`, max speed x 0.85 (0x4143c0 sets
    +0x518 = v and +0x510 = 0.6 v and rescales the gait animations): an engine
    hit;
  - thighs (4, 5): never reached. The base code here would scale the knockback
    threshold (0x417a10(0.85)) and fall with chance `(100 - tonnage) %`
    (0x416770 = roster tonnage, 0x41b640), but thighs are `Leg` objects whose
    vtable +0x28 is 0x463ef0, which handles type 4 itself (calves forward to
    it): if the mech is not limping yet, `Mech::StartLimp(leg, rand & 1)`
    (0x41d5f0, below; 50 % also falls) and success, else nothing and failure.
    Other types go to the base;
  - head (6): head `Destroy(0, 1)`, i.e. **death**, unless invincible;
  - arms (2, 3): nothing.
- Every critical that reports success on the player also plays mech sound 3
  (`Mech::PlayTableSound`, 0x4612be).

Equipment type table (0x5e6868 + 0x14 x type: type, name, voice, message id,
kind): 1 armor, 2 armorff, 3 fcs (voice 7), 4 bap (2), 5 case (3), 6 c3 (4),
7 c3slave, 8 ecm (5), 9 sink (10), 10 dsink (11), 11 jjets (12), 12 masc (9),
13 tag (14), 14 tc (15). Voices at 0x608fc0 are loaded at 0x4842d7:
1 ams, 2 bap, 3 case, 4 c3, 5 ecm, 6 engine, 7 fcs, 8 gyro, 9 masc, 10 sink,
11 dsink, 12 jjets, 13 sensor, 14 tag, 15 tc (`snd_equip_*`).

### Leg hits: limp and fall (0x463b40)

After the normal leg hit, if the mech is not already limping (+0x5ac is -1,
0x415ac7; 0/1 = limping leg):
`p = 0.35` for the player, `1.0` for anyone else, h = leg internal;
if `rand/32767 < h^2 - (p + 1) h + p` (= (1 - h)(p - h)) or `h < 0.3`, call
0x41d5f0(leg, random bit): not when invincible; with the random bit (50 %) the
mech falls (0x41b660(3 or 4) by throttle sign), then +0x5ac = that leg if the
mech has a limp animation for it (+0x424 table). AI mechs start limping with
chance (1 - h)^2 per leg hit; the player only once the leg is below 35 %
internal, and always below 30 %.

## Discoveries

- **A critical hit replaces the damage.** When a hit rolls a critical, the
  armor and internal damage of that hit is skipped, even if the critical does
  nothing (no heat sink voice, no weapons in a leg, CASE blocking an ammo
  crit, a hit on an arm's "special"). About 2 % of eligible hits are free.
- **Lasers never cause criticals.** The roll requires the weapon not to be a
  BEAM, and every laser is one.
- **A part cannot be destroyed while it has armor.** Internal structure is
  floored at the armor fraction (torso: at the weaker of its front and rear
  armor). Internal structure is damaged from the first hit (0.124 x D points
  even with full armor), but on a well-armored part it ends up pinned to the
  armor value and both reach 0 on the same hit. Simulated Mad Cat centre torso
  (36 armor, 23 IS) vs a Large Laser (8): 12 hits to destroy, where tabletop
  BattleTech would need 8. The rear armor matters for the front: stripping the
  rear also removes the front's floor.
- **Armor is worth 0.3844 points per point of damage.** A weapon's `DAMAGE` is
  scaled by 0.0062 and then by 0.62 against armor x 100 / armor points.
- **Front or rear is decided by a fixed edge order, not by where the shot came
  from (likely).** The shot is stretched into a line through the whole torso,
  and the centre-front edge is tested first, so a straight shot into the back
  of the centre torso counts as front armor. Rear sections are hit when the
  line misses the corresponding front edge, which needs an oblique angle; such
  angled shots from the front can also land on the rear. Not yet checked in the
  emulator or in game.
- **Criticals check only the centre-front armor for the whole torso.** The
  roll reads the part's +0x40, which for the torso is the CT front. Side and
  rear torso hits can crit once the CT front is stripped, and never before.
- **Head criticals are instant death** (1 in 1000 eligible non-laser hits on a
  head with 15 % armor or less).
- **Losing one leg kills you.** Destroying either thigh calls `Mech::Die`.
- **The ammo critical does not explode.** It empties the smallest bin in the
  part (no damage). CASE in that part prevents it, and a CASE destroyed by an
  equipment critical still protects (0x460c10 does not check the destroyed
  flag). The heat cook-off bypasses CASE.
- **Weapon criticals take your least useful weapon first** (likely): lowest
  remaining ammo x damage, so an empty launcher is lost before a loaded one.
- **The heat sink critical ignores location.** It removes one sink from the
  total even if the hit part holds none; destroying a part later removes that
  part's own sinks too.
- **A leg critical makes you limp, once.** The Leg class's own critical
  (0x463ef0) starts the limp (and a fall half the time); on a mech that is
  already limping the leg critical does nothing, and the hit's damage is still
  skipped.
- **Dead code: a leg-actuator critical.** The base critical has a thigh case
  that would cut the knockdown threshold and trip light mechs more
  (`(100 - tonnage) %`), but the Leg override means it never runs. It also
  carries a bug: the constructor builds the threshold from 0.376 x 10 and
  0.376 x 40 (3.76 + 11.28 x (t - 10) / 90, 0x414c71), while 0x417a10 rebuilds
  it with 5 and 80, which would have made light mechs easier (10 t: 3.76 to
  1.60) and heavy mechs harder (100 t: 15.04 to 25.57) to knock down.
  0x417a10 has no other caller.
- **AI mechs limp far sooner than you** (p = 1.0 vs 0.35 in the limp chance).
- **Difficulty only scales damage taken by you and your side.** Low 0.7,
  medium 1.0, high 1.3; enemy mechs always take full damage. Multiplayer skips
  it for part hits but not for the area-damage path.
- **Area damage skips the head** and spreads 1/7 of the damage to each of the
  other seven locations, 20 % of that to each rear torso.
- **The Elemental dies from any part loss** (roster index 7, 0x41b5f0).
- **The "Heat Death disabled" switch is full invincibility.** Mech +0x630 also
  floors internal at 1 %, blocks criticals, knockback, limping and the head
  critical. It is the debug-menu item "Invincible" (MFC command 0x9c86 →
  0x4835a0 toggles 0x608e40, applied at spawn 0x478324) or game-option bit 1
  (0x40ced0, 0x40cf20). Suggest renaming the field `invincible`.
- **Unused callouts:** voices for AMS, gyro and sensor are loaded but nothing
  plays them; the engine callout is played by index (0x460eb6). Leg criticals
  have no voice.
- **NARC hits do no damage**, and the target-heat value (`HEAT`'s second
  number) is 0 for every weapon, so hits never heat the target through that
  path.
- **Death animations alternate**: slow-moving mechs killed by a CT loss fall
  with animation 7 or 8 in turn, through one global flag shared by all mechs.
- Torso `Destroy` contains `if (+0x411 == 0) +0x411 = 0` (0x465f40), a no-op.
- 0x4b0de0's ammo mode walks CASE items in the part and discards the result
  (dead code, possibly meant to let CASE stop heat cook-offs).
- Edge case: in the equipment critical, if `rand()` lands above the last
  cumulative weight (float rounding when r ~ 1.0), the code uses the random
  float's bits as the item pointer (0x461211). Very rare; likely a crash.

## Proposed names (for `symbols.csv`, not applied)

| Address | Name | Note |
|---|---|---|
| 0x41ac60 | Mech::TakeDamage | Mech vtable +0x164 |
| 0x4161a0 | mech_damage_callback | node +0xbc handler → +0x164 |
| 0x41b0d0 | Mech::Die | Mech vtable +0x70 |
| 0x41cfa0 | Mech::DestroyPart | (part id) part vtable +0x20 (0, 1); replaces `Mech::HitPart` |
| 0x41b5f0 | Mech::OnPartDestroyed | |
| 0x416770 | Mech::Tonnage | roster tonnage |
| 0x4143c0 | Mech::SetMaxSpeed | +0x518, +0x510 = 0.6 x |
| 0x417a10 | Mech::ScaleKnockdownThreshold | +0x6b8..+0x6c8 |
| 0x41d5f0 | Mech::StartLimp | (leg, fall) |
| 0x41b640 | Mech::Fall | 0x41b660(3 or 4) |
| 0x45fe00 | MechPart::Init | armor and IS from config and table |
| 0x460970 | MechPart::TakeHit | vtable +0x18 |
| 0x461300 | MechPart::ApplyDamage | vtable +0x38 |
| 0x461510 | MechPart::SetInternal | vtable +0x3c |
| 0x461640 | MechPart::SetArmor | vtable +0x40 |
| 0x461a30 | MechPart::Destroy | vtable +0x20 |
| 0x4617f0 | MechPart::LoseContents | |
| 0x460350 | Torso::HitSection | |
| 0x460780 | segments_cross_xz | |
| 0x460cc0 | MechPart::HasEquipment | (type) |
| 0x4644b0 | Torso::Init | |
| 0x464830 | Torso::SetInternal | |
| 0x464950 | Torso::SetArmor | |
| 0x465f40 | Torso::Destroy | |
| 0x463b40 | Leg::TakeHit | limp roll |
| 0x463f70 | Leg::Destroy | |
| 0x463ef0 | Leg::Critical | type 4 = start limp |
| 0x4671e0 | Head::Destroy | |
| 0x466c50 | Arm::Destroy | |
| 0x583020 | zWeapon_BeamDamage | |
| 0x583480 | zWeapon_DealDamage | |
| 0x5826b0 | zWeapon_ProjectileHit | |
| 0x5829b0 | zWeapon_Splash | |
| 0x4b0de0 | Weapons::CriticalLoss | (part, 0 weapon / 1 ammo); fix the `AmmoExplosion` note |
| 0x4b4d10 | AmmoBin::SetRounds | |
| 0x4b2d50 | Weapon::AddDamage | +0x70, 1.0 = destroyed |
| 0x485400/30/60/90 | is_ct / is_side / is_arm / is_leg | IS table lookups |
| 0x424760 | max_armor_points | |

Type changes: Mech +0x630 → `invincible`; +0x578 state (10 dead); +0x5ac limp
leg (-1 none); +0x664 last attacker; +0x411 killed by nuke; +0x518 max speed;
+0x3ac int[15] equipment counts by type; +0x3ec equipment list; Mech vtable
+0x168 network event.

## Open

- Confirm the front/rear edge order in the emulator (0x460350 with a
  synthetic torso and shots from behind).
- What the torso `ApplyImpact` (0x464af0) and arm 0x466c30 do with the impulse.
- Whether `+0x38`/`+0x40` names should be "internal"/"armor" in the HUD's terms
  (the network sends three 0-255 values per section).
- Who reads the `xl` engine flag.
