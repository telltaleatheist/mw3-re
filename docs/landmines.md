# The landmines

Ported 2026-10-25 (item 13): `src/mech3/landmines.cpp`, the load 0x4135c0,
the free 0x413770, the check 0x413560 and the detonation 0x413470, beside
the mechs' set-up in the exe (0x413470..0x4137c0; the original file name is
unknown, between briefing.cpp and mech.cpp).

## The data

Each operation's `reader.zbd` may hold `landmines.zrd`:

```
("weapon" ("LANDMINE") "radius" (20.0) "m1" () "m2" ((x y z) (x y z) ...) ...)
```

c1 and c2 ship it, both with LANDMINE and 20 m: c1 mission 2 has 82 mines,
c1 mission 4 194, c2 mission 3 207; every other mission's block is empty
or missing. LANDMINE (weapons.zrd) is a missile weapon: DAMAGE 30,
IMPACT_PROXIMITY 40, a CRATER (radius 10..15, depth 0.5), the
"impact_missle_mech" animation and "large_pock.flt" surface mark.

## The load (0x4135c0, ECX 0x604e48)

Called by the mission load (0x490bf2) with "landmines.zrd" and the mission's
block name (`m<n>`). No file: success (1). "weapon" must be a one-string
list naming a zWeapon definition (0x581ed0), else it fails (0) and leaves
the tree unfreed (the exe's; only a broken file reaches it). "radius" as a
one-float list sets the radius (0x5e1f58) and its square (0x604e40); anything
else gives 20 and 400. Each item of the block that is a list of exactly three
floats becomes a mine (16 bytes: armed 1, x y z) at the end of the list
(0x4137c0); others are skipped. The load only appends: the mission's cleanup
empties the list (0x413770, from 0x4918b5).

## The check (0x413560)

Every mech's update (Mech::Update 0x419fc0, at 0x41a791) passes its place
(0x5a85e0, the node's origin) to the check, so the player, lancemates and
enemies all set mines off; there is no team test. Each armed mine whose
distance to the place, squared in 3D (0x51dfa0: dz² + dy² + dx²), is under
the radius squared goes off. The compare is the x87's: a NaN distance counts
as near, so a mech at an undefined place would set off every armed mine. The
answer (whether any went off) is not read.

## The detonation (0x413470)

The ground under the mine is asked from 50 m above it (0x4edab0 on the
world, the y given minus -50). When there is ground, the record's point is
the mine's x and z with the ground's height, and a stand-in shot (a static
Projectile at 0x604e60: its +0x24 and +0x48 that point, +0x5bc 1.0, its node
0x605438, memory nothing else touches) takes zWeapon's impact (0x5826b0,
no animation override): the weapon's callback, damage to what the ground
record names, crater, sound, animations. Then, if the weapon has an
IMPACT_PROXIMITY, its splash query (0x582920, flag 1) and splash (0x5829b0)
around the stand-in shot: damage falling off with the square of the distance
to everything within 40 m, the mech that stepped on it included. The mine
is disarmed and stays listed. With no ground under a mine the impact is
skipped and the splash comes from wherever the last mine went off (no
shipped mine is off the ground).

The port leaves the stand-in shot's node empty: the exe's 0x605438 is zero,
so the splash query's INTERSECT toggle reads the same.

## The radar

The HUD's radar draws each armed mine within 100 m as a red X (hud.cpp,
hud.md section 15); its `mine_count` / `mine` hooks read the list.

## Checked

`test_landmines.py` (probe `tests/probe_landmines.cpp`): 3000 loads (no
file; the weapon missing, an int, two strings, unknown; the radius missing,
an int, two values, zero, negative; the block missing, up to 40 items with
an int, two or four values or a non-list among them; other blocks; mines
already listed), with zReader's find and the list's insert run as they are;
3000 checks (up to 12 mines armed or not, near, at the edge, far, NaN; the
place NaN sometimes; IMPACT_PROXIMITY 0 or not; the ground found or not) with
the distance and detonation run as they are. Compared: the answers, the
globals, the list, the stand-in shot, every call with what it was given.
`mw3tests` `mission_c1m2_landmines_go_off_headless`: c1 mission 2 lists 82
armed mines, LANDMINE at 20 m; the player put on the mine nearest the start
sets it off (an impact, a splash) and an AI mech put on one 100 m from every
spent one sets that off; the list keeps 82, and the mission's end empties
it.
