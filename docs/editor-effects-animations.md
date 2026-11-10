# Effects and Animations: a design sketch for Owen (2026-11-10)

A modding dialog over the animation def files (modding.md, Animations a def
at a time, steps 1 to 4), in the form of editor-mech-variant.md. For Owen to
pick from; no code until he does.

What is there already, and what this would put a form on:
- A mod can replace or add one animation of a world with a def file:
  `zbd\<world>\anim\<node>-<anim>.json` (one world) or `zbd\anim\...`
  (every world), the file mech3ax's unzbd writes, merged into the world's
  set at load (replaced defs keep their place, added ones go after).
- A def can bring its own node tree (`<node>.flt.json`, a mechlib-style
  model with its materials), so an effect does not need six gamez.zbd edits.
- The MODS screen and `--check-mod` list the def files and their conflicts;
  `--build-anim` merges them into whole anim.zbd files for the original game.
- Only MechWarrior 3 reads def files; Pirate's Moon reads its anim.zbd whole.

## Where it opens

A row in the editor's menu, **EDIT EFFECTS**, from MODS → EDITOR only, as
EDIT WEAPONS. It edits the world of the mission the editor is in.

## The form (first sitting: copy an animation, change its timing and sounds)

```
 Effects and Animations                       world c1
 ---------------------------------------------------------------
 copy of        [ hgaus_trc-fire              v ]   the world's animations
 name           [ hgaus_trc-fire_b              ]   <node>-<anim>, the file's key
 for            (x) this world   ( ) every world
 ---------------------------------------------------------------
 events         time   kind          what
   [x]          0.00   SOUND         [ wep_gauss_fire       v ]
   [x]          0.00   PUFFER        [ muzzle_smoke         v ]
   [x]          0.25   LIGHT         [ muzzle_flash_light   v ]
   [ ]          ...
                [ add ]  [ remove ]  [ earlier ]  [ later ]
 time           [ 0.25 ] seconds    (the picked event's)
 ---------------------------------------------------------------
 [ Play here ]  [ OK ]  [ Cancel ]
 the reason OK was refused, if it was
```

The names in the mock (the animation, its sounds and effects, the event
kinds' labels) are illustrative, not read from a world.

- **copy of** lists the world's animations (their def keys, as the MODS
  screen shows them). Picking one fills the rest from it.
- **name**: the copy's key, or a new one (a new name adds a def; the copy's
  own name replaces it in this mod).
- **events**: the def's event list in order, each its time and kind. The
  first sitting edits an event's time and the name an event plays (a sound,
  a puffer, a light, another animation), each picked from the names the
  world and the game already have, as New Weapon's effect slots are. Other
  kinds show read-only.
- **Play here** starts the edited animation at the view's pick (as New
  Weapon's preview plays a slot), so the modder sees and hears it before
  writing anything.

## What OK writes

- `zbd\<world>\anim\<key>.json` (or `zbd\anim\<key>.json` for every
  world): the def as unzbd writes it (`zeff_anim_json`'s writer), the copy's
  other fields unchanged; an earlier file kept as .bak.
- A scripted motion the copy uses goes beside it as `si-script-<name>.zan.json`
  when the copy names one (the def file names it, step 1).
- Nothing in mod.txt: def files are found by their folder.

The next start (or the next TEST) loads it.

## What it cannot do yet

- **New event kinds or events from nothing**: only the copy's kinds; adding
  an event copies one of the def's own.
- **Motions and node trees**: an object's path (the motion events' keys)
  and the def's own node (`<node>.flt.json`) are the copy's. A new model
  needs mechlib-style art made outside.
- **Puffer, light and sound definitions themselves**: it picks among the
  names the game has; making a new smoke or a new light is another form.
- **Pirate's Moon**: it reads no def files, so the form would say so there
  (or write a whole anim.zbd through `--build-anim`'s merge; Owen's pick).
- **The original game**: def files are ours; `--build-anim` makes the whole
  files for it (modding.md).

## For Owen to pick

1. Whether this comes before or after the Campaign Missions form.
2. The first sitting: timing and names only (sketched), or also adding and
   removing events.
3. Play here: needed in the first sitting, or later.
4. Pirate's Moon: say it is not read there, or write a whole anim.zbd.
