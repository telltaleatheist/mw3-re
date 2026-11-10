# A weapon: ER PPC B

`mw3 --make-starter weapon` makes `mods\starter_weapon` with three files:

- `mod.txt` names the mod and adds the weapon:

  ```
  weapon=ERPPC_B ammo=no tech=any title=ER PPC B
  ```

  `ERPPC_B` is the weapon's name in `weapons.zrd`. `ammo=no` because it is
  an energy weapon, `tech=any` so both Clan and Inner Sphere mechs can carry
  it, `title` the name the mech lab shows.
- `zbd\weapons.zrd` is your game's `weapons.zrd` as text with one weapon
  added at the end: a copy of the ER PPC named `ERPPC_B`, its `DAMAGE` a
  quarter higher and its `HEAT` a fifth higher. Its look and sounds are the
  ER PPC's. New weapons go at the end of the list: the game numbers them by
  their place.
- `about_this_mod.txt` says what it is.

To make your own, change `ERPPC_B`'s numbers (`DAMAGE`, `HEAT`, `RANGE`,
`FIRE_RATE`, `VELOCITY`, `MASS`), or use the editor's New Weapon, which
writes both lines for you (MODS, EDITOR, a mission, EDIT WEAPONS). After an
edit, QUIT to MODS and TEST again: the new numbers are in, no restart.

The original game reads this `weapons.zrd` too and simply never uses the
extra weapon.
