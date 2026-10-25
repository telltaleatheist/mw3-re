# Damage

Mech3.exe 1.2, 2026-10-19. How a mech takes damage, from a weapon's impact to
armor, internal structure, criticals, part loss and death. Addresses are the
1.2 exe. "Likely" marks a reading not confirmed by a second source.

The mech's side of it is **ported and emulator-checked** (section 4): every
function from `Mech::TakeDamage` down to `Mech::Die` runs as the original on
1,500 random mechs and 33,000 operations, state and calls out identical bit
for bit at the game's 24-bit x87 precision. Sections 1-3 were first read from
the disassembly; the port corrected them where noted.

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
   death message; from the shot's owner `[0x9a0edc]`, 0x40e920) and tells the
   controls/AI (+0x37c, vtable +0x94). Every hit overwrites it (null for a
   shot without a shooter node). For the player's mech it lasts one frame:
   the player tick (0x4a1160, the tick object's action; at 0x4a121a) runs 0x41aba0 on the pilot's
   mech after its update and the entities', and that clears +0x664
   unconditionally. Hits land in the weapon tick (priority 3, after the
   player's 2), so the field holds the shooter from the hit to the end of
   the next player tick. The readers that matter take it at once: the kill
   credit inside this call (Die, 0x41b420), the AI's or the controls' on_hit
   just below; an AI mech's field is cleared only by the next hit or the
   forget chain (fixes.md). Checked in play in the port:
   `mission_c1m1_truck_nodes_name_the_truck_headless` sees it set on the
   frame of the truck's hit and cleared on the next, and the only writes are
   that take_damage and 0x41aba0 (watched, 2026-10-25).
3. `HEAT` second value (WeaponType +0xc, x frame time for beams) is added to the
   target's heat at the end (0x419e40). It is 0 in every shipped weapon. A hit
   that does nothing (below) adds none.
4. NARC (WeaponType +0x4c): marks the mech (0x41ac10(1)) and returns. NARC
   hits do no damage. 0x41ac10 sets the NARC timer +0x570 to 30 s, **unless the
   mech carries ECM** (equipment count +0x3cc > 0), which keeps it at 0.
5. Knockback: `IMPACT_FORCE` (WeaponType +0x48; 0 when invincible) along the
   shot's horizontal direction is added to the vector at +0x540 and its length
   kept at +0x53c (only in movement states 1-9). Each frame 0x417a80 compares
   that length with `+0x6c4 + +0x6c8 x (tonnage - 10) / 90` (3.76 + 11.28 x ...,
   so 3.76 for 10 t up to 15.04 for 100 t, set at 0x414c71) and, when reached,
   staggers the mech (0x41b7f0, sound 1 for the player) and clears it.
   The knockback vector is also the impulse handed to the part (`ApplyImpact`,
   below). Its direction is `shot[0] - shot[1]` flattened (0x51e210); a zero
   length leaves it 0.
6. With a collision record +0x20, a roster entry +0x1c (0x414370) and decals
   (+0x3e8), 0x474d90 marks the hit on the mech's damage textures.
7. +0x411 = the weapon's military name (def +8) is message 0x305, `NUKE`; the
   torso's `Destroy(1)` sets it too. It picks the explosion death and, for the
   player, zeroes every armor value in `Die`.
8. If the hit node is a part (node +0x28 low byte, +0x40 the part): part
   `TakeHit`. Otherwise, if the node belongs to a mech (flags 0xf000, +0x40;
   any mech, not checked to be this one) and the attacker is not the mech
   itself: `d = damage x 0.143 x 0.0062` (x difficulty when this is the
   player, **also in multiplayer**), and `ApplyDamage` is called on all 7
   non-head locations: CT, LT, RT front with d, their rear sections with
   0.2 d, both legs and both arms with d. 0.143 is about 1/7. The head never
   takes this damage, and it never rolls criticals. A node that is neither
   does nothing at all.
9. Last, 0x41d170 notes the time and the torso at +0xe0/+0xe4.

### MechPart::TakeHit (0x460970, vtable +0x18)

Arguments: weapon def, collision, damage, impulse.

1. `d = damage x 0.0062` (0x5e1f68). Legs multiply by 1.0 first (0x463b40,
   0x5e1f74), so there are no per-location multipliers.
2. Difficulty: except in multiplayer (game type 3), damage to the player's mech
   (`[0x60861c] +0x14`, the mission's player), and to mechs that pass a
   team-mask test against the player's
   (`player +0xc4 & target +0xc0`, likely allies), is multiplied by
   0x608e5c. 0x40cc20 sets it from the difficulty table: low 0.7, medium 1.0,
   high 1.3 (the third float of each row; the others go to 0x608e54/58).
3. `ApplyImpact` (vtable +0x1c) with `IMPACT_FORCE`, when there was knockback.
4. The torso picks its section from the shot (0x460350, below).
5. Critical roll (section 3). If one is rolled, `Critical` runs and **the hit
   deals no armor or internal damage at all**.
6. Otherwise `ApplyDamage(d, section)`.
7. Head hits on the player trigger a HUD effect (0x508da0) when the pilot's
   +0x228 block, its [1] and +0x200 are set.

Calves, feet and hands pass the whole call to their thigh or arm (through the
vtable, so a foot hit is a thigh hit, limp roll included). The hip calls the
torso's plain `TakeHit` 0x4647b0 directly: no `ZAPS_HUD` static for hip hits.

### ApplyImpact: the torso's jolt (0x464af0)

Only the torso does anything; arms, head, hip and hands forward to it
(0x466c30), legs, calves and feet ignore the impulse (0x40baf0). With impulse
`i` (the knockback vector) and `IMPACT_FORCE` f, unless `i.x` and `i.z` are both
0 (then the spin +0xd8 is cleared and nothing else changes):

```
spin  (+0xd8)  = R^T (i.z, 0, -i.x) x 15 deg x f x -0.5    (0x5c2738 = 0.2618)
shove (+0x100) = R^T (k0 i.z x -0.5, k1 i.x x 0.5, k2 i.x x 0.5)
```

R is the torso node's world rotation (0x461cf0, after the node update
0x4ed530) and k0..k2 the torso's +0x14c..+0x154. The spin axis is horizontal and
across the shot, so the torso rocks away from the hit, harder for heavier
weapons; the shove moves it. Its per-frame decay (likely 0x464ce0) is not ported.

Torso, arm and head wrappers (0x464720, 0x466ba0, 0x467200) also play the
weapon's `ZAPS_HUD` static (WeaponType +0x14) on the player's HUD (0x43be70,
kind 4 torso and head, 3 arms).

### Front or rear: 0x460350 (emulator-checked)

The torso builds 8 edges of its footprint in its own space, transforms them to
the world, and tests them in a fixed order against the shot line in the
horizontal plane (2D segment test 0x460780). The box is +-w wide (+0x7c) and
+-d deep (+0x80), half the size of the node `torso_polys`' bounding box
(0x46439d); the centre strip is +-0.2 w (0x5e4ef0). The shot line is the
collision segment stretched 10 times both ways (0x5e4ef4), so it crosses the
whole torso. The mech faces -z in its own frame and +x is its right
(right-handed; the Mad Cat's cockpit and toes face -z in `mechlib.zbd`):

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

**The test is one-sided** (0x460780's first half): an edge counts only when
the stretched shot crosses it going in, from outside the box to inside. `shot`
is `[0x9a0ec4]`, two points set by 0x583480: [0] the projectile's position
(+0x48, likely where it was before the step that hit) and [1] the collision
point, so [0] is the shooter's side (likely; the reverse would put every frontal
shot on the rear armor). A convex box is entered through one face, so the order
only matters at corners. The first edge crossed wins; none crossed (the line
misses the box) means section 0, CT front. If the chosen side torso is
destroyed (internal 0), the hit moves to the centre torso, front or rear to
match (`(s <= 2) - 1 & 3`).

The side planes (edges 6, 7) give **front** sections: a hit through the side
of the torso always strikes the side torso's front armor. Emulator sweep
(`tests/differential/explore/damage_sections.py`: half sizes 2 x 1.5, shots
from 30 units at each bearing, aimed through the centre or offset sideways):

| Shot from | Through the centre | Aimed 0.8 off centre, toward the mech's left / right |
|---|---|---|
| ahead (0) | CT front | LT front / RT front |
| front-right (45) | RT front | CT front / RT front |
| right (90) | RT front | RT front (either way along the side) |
| rear-right (135) | RT rear | CT rear / RT front |
| behind (180) | CT rear | LT rear / RT rear |
| rear-left (225) | LT rear | LT front / CT rear |
| left (270) | LT front | LT front (either way along the side) |

So the rear armor is only hit from behind, within about 45 to 60 degrees of
straight back for a hit through the middle; anything from the side is front
armor.

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

`SetInternal` base (0x461510, section 0 only): a destroyed part takes 0; an
unchanged value only sets Mech +0x410. Otherwise it stores the value, sends
network event 0x15 with the part id (Mech vtable +0x168, "section damaged",
`network-messages.md`) and sets Mech +0x410. At 0 or below: internal = 0 and
`Destroy(v, 1)` with v = 0 when `rand/32767 > 0.4` (60 %) else 1 (40 %); v picks
one of two destruction effects from the pilot's table (+0x1a4 + 0xc x id + 4v);
0x586ce0 returning true forces v = 0. Torso `SetInternal` (0x464830): the
section's internal value (front and rear share it), event 0x15 with 1, 7 or 8
**after** the consequence: centre torso at 0 → `Destroy(0, 1)` on the torso;
left torso at 0 → left arm `SetInternal(0, 0)`; right torso → right arm. The
side torso itself keeps no destroyed flag: `SetArmor` (0x464950) just pins its
armor at 0. Feet, calves and hands forward `SetInternal` to their thigh or arm.

`Destroy` base (0x461a30, `(variant, fly)`) and 0x4617f0:
- every weapon mounted on the part is destroyed (0x4b1040 → 0x4b3020);
- arms and thighs fly off as a separate object (vtable +0x60, which returns the
  new node and fills a start point and a velocity), except on the Elemental
  (roster index 7); the pilot's destruction effect `variant` (+0x1a4 + 0xc x
  id + 4 x variant) plays on that node (0x508aa0), or on the part's own node at
  the mech's +0x30 velocity when it does not fly; a flying part with `fly` set
  also gets the trail effect (+0x1ac + 0xc x id). Both register an end callback
  (0x508f00 with 0x461c90) that clears the handle (+0x6c, +0x70);
- 0x4617f0: destroyed flag (+0x48), armor and internal = 0, and for each
  intact (+8 = 0) equipment item in that part that takes critical slots: heat
  sinks (types 9, 10) are lost (`HeatSinks::Lose(count)`), any other type
  decrements the mech's per-type counter at Mech +0x3ac + 4 x type by one (jump
  jets +0x3d8 = type 11, MASC +0x3dc = type 12). An item already wrecked by an
  equipment critical is not counted twice;
- 0x41b5f0: weapons on the part again; on the Elemental, any part loss calls
  torso `Destroy(1, 1)` (death).

**Flying off** (vtable +0x60; base 0x461d80, thigh 0x466fe0, arm 0x467100;
the `from` and `x` arguments are never read). The arm first loses its hand's
weapons (0x41b5f0 on +0x74); the thigh its calf's and foot's, and on the
**Blackhawk and the Owens** (rosters 2, 11) the arm on the same side is
destroyed with it (vt+0x20(0, 0), vt+0x3c(0, 0)). Then the base: the part's
node's world matrix (0x461cf0) and its local one (+0x14) are kept; with the
stub flag a copy of the pilot's stub for the node's id (+0x20c + 4 x id: 1
the never-set +0x210, 2..5 `larm/rarm/lleg/rleg_stub01.flt`; 0x4f9af0(stub,
0, 0)), else an empty object3d (0x4fb740); none of either and the error
"Failed to copy stub for %s partId %d" (0x4a4), the part staying. The new
node takes the old one's name (0x4eb300). The old node: flag 8 off
(0x4eb620), off its first parent (0x4ebb40), under the world ([0x608c84]
vt+0), +0x2c bits 5 and 6 cleared (0x4eb8a0, 0x4eb8e0), placed at its
world pose (0x461d30: the translation and 0x523bc0's angles). The new node
goes under the old parent (0x4eb9b0), becomes the part's (vt+0x0c,
0x460190), gets the old local matrix and the base node update (0x460180)
and the old id down its tree (0x460050); the old tree loses its ids, owners
and damage handlers (0x462000) and joins the mech's list +0x6d0 (0x413bb0;
in instant action with opacity 1). Each update (0x413cb0, Mech::Update's
purge) runs 0x413c30 on each: in instant action, while the mech's node uses
an opacity (0x4fbb10), the flown node takes it when above 0, so the parts
fade with the dead mech. 0x413c30 always answers 0 (`xor al, al`), so
nothing leaves the list; it holds at most one node per arm and thigh, and
the nodes go with the world at the mission's end.
The velocity: (1, 0, 0) (0x5eda60), x -1 for the left arm and thigh (ids
2, 4), turned by the world matrix (0x5226e0), its y + 0.1, times 20
(0x5e4f00), plus 0.75 x the mech's +0x30. Finally 0x41b5f0 on the part. The
old node is returned: the destruction effect plays on it with that
velocity, which carries it away.

Per part:

| Part | Destroy | Effect |
|---|---|---|
| Arm 0x466c50 | internal 0, base, then the hand (+0x74) goes: 0x41b5f0 on it, removed from the mech's part list (+0x384) and deleted | weapons and equipment in the arm lost |
| Side torso | (section of the torso) | its armor locks at 0, its arm is destroyed, later hits go to the CT |
| Thigh 0x463f70 | the foot's effect stops (0x45fb50) and its node is hidden (0x4eb170), base, then calf (+0x74) and foot (+0x78) lose their equipment and their nodes (vtable +0xc with 0) | the mech falls (0x41b640) and **dies** (Mech vtable +0x70) |
| Head 0x4671e0 | marks itself destroyed, then torso `Destroy(0, 1)`; its own equipment is not counted off | **death** |
| Hip 0x462490 | torso `Destroy(0, 0)` (only reachable through `Mech::DestroyPart(15)`) | **death** |
| Centre torso 0x465f40 | death animation and effect, the mech node renamed `deadmech` (0x4eb300), 0x4617f0, Mech vtable +0x70 | **death** |

`Mech::Die` (0x41b0d0, Mech vtable +0x70): killer (+0x664) told through its
vtable +0x80 (a mech's is 0x41b420, the kill credit); for the mission's player
the statistics (0x433530, 0x433900(1), 0x4338d0(1)) and, when networked
(0x479480), death message 0x13 with the killer; state +0x578 = 10 (previous kept
at +0x57c), +0x410 set, the three vectors +0x30, +0x3c, +0x58 zeroed, the
weapons stopped (0x4b1110), mission-failure hook (0x40ae70(2)) for the player
in view mode 1, all 25 sounds at +0x5b8 stopped, controls stopped (+0x37c
vtable +0xa8), both feet's effects stopped (0x463100), the player's cockpit HUD
shut off (0x41b263..0x41b30c), an action of kind 5 or 6 at the head of the list
+0x480 stopped (0x421060 on +0x458 + 4 x kind), then for the mission's player
0x48f9e0 and, after a NUKE kill, every armor value zeroed for the display.

Centre-torso death animation (0x465f40): a NUKE kill or an Elemental explodes in
place (effect 1); otherwise, if airborne (Mech vtable +0x144) or moving
forwards faster than 13.89 m/s (50 km/h; forward is -z, `movement-jump.md`),
animation 1 (fallf) / effect 2; below 50 km/h either way, animation 7 or 8
(collapsel/collapser), alternating through the global 0x605cc0; backwards
faster than that, animation 2 (fallb) / effect 3.

Before the fall, 0x41d070 (the death's preparation): +0x634 bit 0 cleared
(0x41ca60(0)); unless the mech lies on its back or front (states 12, 13), the
hip's world position (0x4ed530) and the pilot's canned effect 0, "eject"
(0x432830, below) at it: when that starts, the hip sinks 0.75 of its crouch
depth at once (0x4622a0) and, with detail bit 0, the eject sound (slot 10,
`mech_eject`) plays into +0x5e0; for the mission's player, 0x48f9e0 (the
loss; Die calls it again, a no-op the second time). Die then stops every
sound at +0x5b8, +0x5e0 among them, in the same call: the eject is never
heard.

The canned effects (0x432830 on the pilot, `(slot, node, at, vel)`): slot 0
eject, 1 jump jets, 2 narc (`mechdef`'s `canned`: an effect, `ofs` and a
`node` by name on list 6). The position is the slot's offset plus `at`, in
the world's axes (the offset is never turned with the mech). With no node
given, a copy of the slot's node (0x4f9af0(node, 0, 0)) is made and kept on
a list (0x605714, count 0x605718); none and no effect when the slot has no
node. Unless `at` is the zero vector's address 0x63d870, the node is moved
there (0x4fc0b0); the effect starts at the node with the position and `vel`
(0x508aa0). The jump jets pass their jet node and the zero vector for both
(the flames sit at the offset on the node). The copies go at the mission's
end (0x431800, from the cleanup and from 0x4318e0): each off all its
parents, every animation using it stopped and forgotten (0x506bc0(node, 1,
1): 0x506ae0 checks the def's node, root, objects and nodes; 0x506b60 clears
the objects' and nodes' pointers), then destroyed (0x4f9050).

The end callback the parts' effects get (0x508f00 with 0x461c90, ECX the
effect, EDX the part): when the effect stops (value 0) the handle holding it
is cleared, the trail (+0x70), the debris (+0x6c), or on the torso (its
node's id word +0x28 is 1) the second and first death effects (+0x78,
+0x74).

`Mech::NoteHit` (0x41d170) stamps the time and names the torso as the part
last hit; 0x41d130 then makes that part's node zEntity's target node
(+0x10, 0x5a8680, which refreshes the target and centroid nodes' world
matrices 0x5a8500, then 0x5a85c0), or the marker 0.65 up (+0xe8) when no
part is named. The hit timeout (0x41d190, every frame) calls NoteHit again
once a second has passed since the last, so from a second into a mission
every mech's target node is its torso's. 0x5a8630 reads that node's world
position.

**The kill credit** (`Mech::OnKill` 0x41b420, Mech vtable +0x80, from the
victim's Die on its last attacker +0x664): the victim's vt+0x2c gives an
object whose +0x50, +0x40 and +0x48 say what it was (a mech's are 0x4d2a70,
itself, and 0x40bae0, none: a mech is a mech). The credit goes to the
salvage when the game is the campaign (game vt+0x2c 1) and the killer is the
player ([0x60861c] +0x14) or counts the player's team (its vt+0x28, itself,
+0xc0) among its allies (+0xc4). The killer's controls hear of it (+0x37c's
second base, vt+0xb4). Then:

| Killed | Salvage (credit only) | Count | Points |
|---|---|---|---|
| a mech (+0x50) | 0x472f00 on [0x609990] | +0x3f4 | +0x400 += 10 x (30, 40, 50, 70 by its weight class, 0x484700; 10 for an Elemental killer) |
| a vehicle (+0x40) | 0x472ec0 | +0x3f8 | +0x404 += 75 |
| a building (+0x48) | | +0x3fc | +0x408 += 200 |

A mech kill also clears the killer's targeted part (0x41d810(-1): +0xf8 =
-1). The weight classes (the roster's +0x2c) are not the tonnage's: 0 the
Elemental and Firefly; 1 Blackhawk, Bushwacker, Mad Cat, Owens, Puma,
Shadowcat, Strider; 2 Annihilator, Avatar, Cauldron, Champion, Orion, Thor,
Vulture; 3 Daishi, Sunder, Supernova. A weapon kill's attacker is the
shooter's node's object (0x40eee0 → 0x40e920: the node's +0x28 bits 16, 18,
a vehicle's (0xf000) or part's (low byte) owner, 19, 17, 20, 21, 22, the
owner at +0x40).

**After death** (`mech3/mech_dead`). Once a dead mech is still (its fall
over, +0x08 off), its update adds it to the dead mechs' list (0x41e350 on
0x605510; +0x6b4 the time) and returns early from then on. The player's
tick runs the list each frame (0x41e3f0): each gets 0x419ef0 (no impulse,
its forward and sideways speeds +0x3c / +0x44 run down by x(1 - 3 dt) and
stop below 0.05, no turn, the skis' ground step 0x4162c0, zEntity3d's move
0x5a8000, the flown parts' fade), then 0x41e220 decides: in the campaign it
stays; in instant action it fades from 170 s ((180 - t) / 10, 0x41e060) and
at 180 s is hidden (0x41e140), its mech kills (+0x3f4) added to the match
(the player's +0x4c, an ally's +0x50, the others' +0x54 of the game), the
sensors forget it (0x4aa6b0) and it is deleted; in multiplayer the
network's update (vt+0x158), the fade over 7.1 s when the game's +0x8c is
set, and for the dead player at 7.1 s the respawn's answer (0x46dde0). The
cleanup's 0x414000 empties the list. Checked: `test_deaths.py`
(dead_update 300, dead_step 600 over the three game kinds; BREAK=14, 15);
the c1m1 Owens is on the list after its fall, not before. The sensors' forget
0x4aa6b0 is ported (ai.md section 15, The forget): the wreck is deleted then,
as the exe does, with a FIX (bug) for the attackers it leaves (+0x664). Not
ported: the instant action game's tallies, the network's update and
respawn.

Death conditions found: centre torso internal 0; head internal 0; either thigh
destroyed; head critical (below); any part on an Elemental; heat (`heat.md`);
a mission script's `Mech::DestroyPart` (0x41cfa0, called from zrdrvalve.cpp
0x4e34e1 and the heat code 0x419bde). Arm or side-torso loss is never fatal.
**No XL-engine rule exists**: the `xl` key (config +0x28, 0x46c4eb) is read
only by the engine weight and rating (0x432f40, 0x432fa0), whose callers are
the loadout weight (0x425xxx), the mech lab (0x44bxxx, 0x44dxxx) and the items
(0x4731xx); nothing in the mech's run-time code or the parts reads it.

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
- **3 Equipment.** Among the part's equipment (item kind 3) that is intact
  (+0x08 = 0) and not a heat sink (types 9, 10), one is picked at random
  **weighted by its critical slots** (item vtable +0x18, 0x4731c0; items with
  none never are), marked destroyed (+0x08 = 1) and the mech's counter for its
  type (+0x3ac + 4 x type) decremented. Voice for the type (0x485390 →
  0x5e6870 + 0x14 x type → 0x608fc0). With one candidate no `rand()` is
  drawn. The candidates live in two 12-entry stack arrays with no bound check
  (a part with more than 12 such items would overrun them; likely never in
  the shipped loadouts).
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
- **Rear armor is only hit from behind; side hits strike front armor**
  (emulator). Each edge of the torso's footprint counts only for a shot
  entering through it, and the two side faces give the side torsos' front
  sections. A shot through the middle from more than about 45 to 60 degrees
  off straight behind lands on front armor. (Corrects an earlier reading here
  that a fixed edge order sent straight rear shots to the front.)
- **A shot that only clips the torso's outline counts as centre-front.** When
  the stretched shot line misses the torso's footprint box, the section stays
  0.
- **Side torsos are hollow** (from the code). Losing a side torso takes its
  arm and pins the side's armor at 0, but nothing calls the part-loss code for
  the side itself: weapons mounted there belong to the torso object (0x41d510
  maps ids 7 and 8 to it) and keep firing, and heat sinks, jump jets and other
  equipment listed in `ltorso`/`rtorso` stay counted. For the same reason
  criticals on any torso hit only look at items listed in the centre `torso`:
  side-torso ammo and equipment can never be hit by a critical, and **CASE in
  a side torso never does anything** (the Mad Cat's two CASE are in its side
  torsos). Weapon criticals pick among all torso-mounted weapons, sides
  included.
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
- **Equipment criticals favour bulky equipment**: the pick is weighted by
  critical slots, not item count.
- **ECM makes you immune to NARC** (0x41ac10: the NARC timer stays 0 while the
  mech carries ECM).
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
  cumulative weight (r is 1.0 when `rand()` returns 32767, and the summed
  weights can round below 1), the code uses the random float's bits as the item
  pointer (0x461211). Very rare; a crash. The port takes the last candidate
  (`FIX (bug)`, `fixes.md`).
- **Hip hits never fizz the HUD**: the hip goes straight to the torso's plain
  hit (0x462560), skipping the `ZAPS_HUD` static the torso, arms and head play.

## Names

The ported functions are in `symbols.csv`. Others proposed:

| Address | Name | Note |
|---|---|---|
| 0x4161a0 | mech_damage_callback | node +0xbc handler → +0x164 |
| 0x45fe00 | MechPart::Init | armor and IS from config and table (ported, mech-build.md section 4) |
| 0x4644b0 | Torso::Init | the sections 1-5 and the side structure (ported) |
| 0x583020 | zWeapon_BeamDamage | |
| 0x583480 | zWeapon_DealDamage | sets the shot `[0x9a0ec4]` |
| 0x5826b0 | zWeapon_ProjectileHit | |
| 0x5829b0 | zWeapon_Splash | |
| 0x4b4d10 | AmmoBin::SetRounds | |
| 0x4b2d50 | Weapon::AddDamage | +0x70, 1.0 = destroyed |
| 0x485400/30/60/90 | is_ct / is_side / is_arm / is_leg | IS table lookups |
| 0x41b420 | Mech::OnKill | Mech vtable +0x80: the killer's score |
| 0x464ce0 | Torso::UpdateJolt | likely; the spin and shove decay |

## 4. The port

`src/mech3/mech_parts.{h,cpp}` (mech_parts.cpp: the base part and the torso,
leg, arm, head, hip, foot, calf and hand classes with the vtable methods
above) and `src/mech3/mech_damage.cpp` (the Mech's side: `take_damage`,
`die`, `on_part_destroyed`, `destroy_part`, `start_limp`, `fall`,
`knocked_down`, `set_max_speed`, `scale_knockdown`, `mark_narc`,
`note_hit`, `aim_at_last_hit`, `death_prep`). The Mech struct grew the fields these use (`mech.h`); the
pilot side (+0x684) is `MechData`. Calls into unported code (nodes, effects,
weapons, HUD, sounds, statistics, network, animation) go through
`DamageCalls`; the heat code's torso destruction now calls the port when no
hook is set.

Checked by `tests/differential/test_damage.py` (probe `tests/probe_damage.cpp`):
a whole mech in emulator memory with the real Mech vtable (two entries
stubbed), the 13 parts with their real vtables and nodes, the pilot, the
loadout's item list and the part list. 1,500 random mechs (armor and
structure fractions, sections, stripped armor, Elemental, invincible, ECM,
items in every part) take random hits on every part node and on the body
(knockback, NARC, nuke, beams, zero damage), plus direct criticals of every
kind on every part, `DestroyPart` for every id, limps, falls and the torso's
section choice; after each of 33,000 operations the mech, its parts, the
items' destroyed flags, the part list and the 41 kinds of call out must match
exactly. Code hooks on the original confirm the run reaches every critical
roll, CASE, difficulty and multiplayer, the armor floors, structure at 0,
area damage, limps and each kind of death. Breaking the port (old armor in
the structure formula, `<` for the critical gate, front and rear swapped,
CASE ignored, one sink lost per part, difficulty for every mech, the AI limp
chance, the torso floor, area damage to the rear, knockback when dead) fails
each time.

`tests/differential/test_deaths.py` (probe `tests/probe_deaths.cpp`): the
effect-end callback 0x461c90 on random handles, ids and values; the canned
effects 0x432830 on random slots (with and without nodes, `at` and `vel`
real or the zero vector, the copy failing) and their free 0x431800 (copies
with up to two parents); the death's preparation 0x41d070 over the states,
detail bits, the player or not; zEffect's 0x506ae0 and 0x506b60. 1,553 runs
identical (2,653 with the kill credit and the fly-off; seeds 1 and 2); BREAK=1..8 fail. In the game,
`mission_c1m1_owens_dies_headless` destroys the woken Owens' centre torso:
dead, renamed `deadmech`, a slow fall queued (7 or 8) that plays out over
about 4.4 s, its eject seat copied, the player untouched, the copy freed at
the cleanup.

Fixes: the heat-sink critical's callout plays when a sink is lost, and the
equipment critical's out-of-range pick takes the last candidate (`fixes.md`).
The test runs the original with 0x45fbc0 reporting a lost sink, the fix's
behaviour.

## Open

- Whether `+0x38`/`+0x40` names should be "internal"/"armor" in the HUD's terms
  (the network sends three 0-255 values per section).
- Which end of `[0x9a0ec4]` is the shooter's for beams and splash (0x582e89,
  0x5829b0); for projectiles it is [0] (likely, above).
- The torso jolt's decay (0x464ce0) and the knockback stagger (0x417a80).
- Unported here: 0x4e66a0 (a part's node off the mech), the statistics, the
  network events, the controls' hit and stop, the cockpit HUD's death and the
  damage decals (0x474d90). In the game (c1m1 item 2, `app_mech.cpp`
  wire_mech_parts) the death's preparation and fall, the limp's blend, the
  action stop, the weapons' side (0x4b1110, 0x4b1040, 0x4b0de0), the effects'
  end callback, the part's world matrix, the view's camera and the player's
  loss are wired.

Resolved by the port: the front/rear choice (emulator, above), what
`ApplyImpact` does (the torso's jolt), and the `xl` flag (no damage rule).
