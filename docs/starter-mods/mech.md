# A mech variant: Thor B

`mw3 --make-starter mech` makes `mods\starter_mech` with three files:

- `mod.txt` names the mod and adds the mech:

  ```
  mech=thor_b tech=clan mass=70 class=heavy model=thor title=Thor B
  ```

  `thor_b` is the mech's own name (letters, digits, `_` and `-`). `tech`,
  `mass` and `class` say what the mech lab offers it as. `model=thor` borrows
  the Thor's models, walking and its sounds, so you do not need any art.
  `title` is the name shown.
- `zbd\dfn_thor_b.zrd` is the mech's definition: the Thor's own
  `dfn_thor.zrd` from your game, as text, with two numbers changed:
  `max_turn_rate` a quarter higher and `accel_rate` half again as high. The
  rest (armour, loadouts, where the weapons sit) is the Thor's.
- `about_this_mod.txt` says what it is.

To make your own variant, change the numbers in `dfn_thor_b.zrd` (any text
editor), or rename the mech: change `thor_b` in both `mod.txt` and the
file's name. A different base mech means its own `dfn_<mech>.zrd` (copy it
out of your game's `zbd\reader.zbd` with mech3ax's `unzbd`) and its `model=`.

In the game: the mech lab lists Thor B with the other Clan mechs. TEST
starts you in the Mad Cat; pick Thor B in the mech lab of a campaign or
Instant Action to drive it.
