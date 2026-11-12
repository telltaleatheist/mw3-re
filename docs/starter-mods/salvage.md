# Salvage: every kill's salvage offered

`mw3 --make-starter salvage` makes `mods\starter_salvage` with two files:

- `mod.txt` names the mod and holds the one line that does the work:

  ```
  salvage_chance=100
  ```

  The number is how many times in 100 a kill's salvage is offered. The
  original game tosses a coin (1 in 2) in two places: for a destroyed
  vehicle's weapons (all of them or none), and for a destroyed mech that lost
  a leg (the whole mech, or only its weapons and equipment). 100 always gives
  them; 0 never does; 50 is the original's odds.
- `about_this_mod.txt` says what it is.

Tick it for a campaign on the MODS screen and press PLAY: every mission of
that campaign uses it. The main menu's campaign is never changed, and
Instant Action and multiplayer have no salvage.

One mission can have its own number, over the line above:

```
mission_salvage=c1 2 50
```

That is the world's folder (`c1`), the mission's number on it (`2`) and the
number from 0 to 100. Put one such line for each mission. In the editor
(MODS, MISSIONS, a mission, the pause's EDIT MISSION) the row MISSION
SETTINGS writes this line for the mission that is open, into your own copy
of the campaign.

What salvage never brings stays as the original: a mech whose centre torso
was destroyed (unless by a head shot), and equipment in destroyed parts.

If two ticked mods set a number, the one higher in the list wins; the other's
row on MODS says it shares its salvage chance.
