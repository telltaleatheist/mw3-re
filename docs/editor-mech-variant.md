# Mech Variant: a design sketch for Owen (2026-11-10)

The next modding dialog after New Weapon (editor.md, 1a), for Owen to pick
from. No code until he does. It makes a new mech from one the game (or an
enabled mod) has, the way `--make-starter mech` makes Thor B
(modding.md, Starter mods), but with the numbers on a form.

## Where it opens

A new row in the editor's main menu, **EDIT MECHS**, in its last blank row
(between HELP and TELEPORT), opening only in a mission the MODS screen's
EDITOR started, as EDIT WEAPONS does. Anywhere else the line under the menu
says "EDIT MECHS writes into a mod: open one from MODS, EDITOR".

Alternative: a second page of New Weapon's form. The menu row is simpler
for players to find.

## The form (first sitting: the row and the handling)

```
 Mech Variant
 ---------------------------------------------------------------
 copy of        [ Thor                          v ]
 name           [ thor_b                          ]  letters, digits, _ -
 title          [ Thor B                          ]  shown in the lab
 tech           ( ) Clan  ( ) Inner Sphere  (x) as the copy
 mass           [ 70 ] t      class [ heavy     v ]
 ---------------------------------------------------------------
 turn rate      [ 2.91 ]                    (max_turn_rate)
 speed-up       [ 0.50 ]                    (accel_rate)
 slow-down      [ 1.30 ]                    (decel_rate)
 engine         [ 250 ] to [ 380 ]          (engine_minmax)
 torso turn     [ 110 ] degrees             (torso_limits: yaw)
 torso pitch    [ -18 ] to [ 24 ] degrees   (torso_limits: pitch)
 ---------------------------------------------------------------
 [ OK ]  [ Cancel ]
 the reason OK was refused, if it was
```

- **copy of** lists every mech the lab knows: the game's (19 on MW3, 26 on
  Pirate's Moon) and the enabled mods' rows. Picking one fills every field
  from its definition (its `dfn_<mech>.zrd`, read as the game reads it).
- **name** starts as the first free of `<copy>_b`, `_c` ... (free of every
  mech and every mod row), **title** the same in words.
- **tech, mass, class** are the copy's. Mass moves in steps of 5 (10 to 100,
  the internal structure's table); class is the lab's list (light, medium,
  heavy, assault).
- The **numbers** are the copy's, as the file holds them. The turn rate is
  in radians (the game keeps half of it as the loadouts' turn rate, 72
  degrees when a file has none); showing it in degrees a second needs a
  measured turn first (a test), so the sketch shows the file's number. The
  rest of the definition
  (armour, internals, firepoints, the loadouts, skins, the HUD, the
  animations) is the copy's, unchanged.
- Each field has a tip, as New Weapon's (help file `help_mechs.cpp`).

OK checks the row as mod.txt's reader does (`mech_row_parse`) and as a mount
would (a `mech_row_taken` to add, as New Weapon's `weapon_row_taken`: a
name already a mech or a row), and the numbers'
ranges. A refusal shows on the last line; nothing is written.

## What OK writes

Into the mod the mission was opened from, as New Weapon writes:

- **mod.txt**: `mech=<name> tech=.. mass=.. class=.. model=<copy> title=..`,
  in place of a mech= line naming the same mech, else after the last line;
  the old file kept as mod.txt.bak. `model=<copy>` borrows the copy's
  models, walking, step sounds and voice, so no art is needed.
- **zbd\dfn_<name>.zrd**: the copy's definition as text with the changed
  numbers, as the starter mod writes it (the loose file beside reader.zbd,
  which the reader now reads; modding.md, Starter mods). An earlier one is
  kept as .bak. The original game never opens it (its mech table has only
  its own 19 names), so a mod keeps working in the original.

The lab offers it from the next start (rows are read at the mount), or the
next TEST (Quick testing: TEST re-reads the definitions).

## What it cannot do yet (later sittings, or not at all)

- **Loadouts**: the variant's stock loadouts are the copy's. Changing them
  belongs in the mech lab (save a loadout there; a later sitting could write
  the lab's loadout into the definition's `configurations`).
- **Armour and internals per part**: in the definition's loadouts and the
  internal structure table; a later sitting.
- **Where weapons sit** (firepoints) and **which parts** a mech has: the
  copy's. A new model needs mechlib work (a mod's own mechlib.zbd), not this
  form.
- **Skins**: the copy's list; picking one of the copy's skins as the stock
  could come with the loadouts.
- **The Mad Cat's half ton**: the original weighs the Mad Cat's loadouts
  (mech 9) half a ton light (modding.md). A copy of the Mad Cat at 75 t has
  its loadouts half a ton over and refused by the lab; the form would say so
  when the Mad Cat is picked and suggest 80 t or lighter loadouts.

## For Owen to pick

1. The row (EDIT MECHS in the blank row) or a page of New Weapon.
2. The first sitting's fields: the row and the handling above, or fewer
   (only copy, name, title, mass) to start.
3. The turn rate as the file's number (sketched) or in degrees a second
   (after a test measures what the number means in play).
4. Whether mass may change in the first sitting (the loadouts were made for
   the copy's tonnage: a lighter variant can make them too heavy, which the
   lab then refuses).

Both games: MW3 first, then Pirate's Moon by merge (its definitions have the
same keys; its own mechs, 26, in the list).
