# How MechWarrior 3 really works

Findings from taking `Mech3.exe` 1.2 apart, written for players. Each one comes
from the game's own code; the address and the technical write-up are given so
anyone can check. "Likely" marks a guess.

## Heat

From the heat update at 0x4192f0 (details: `heat.md`).

- **Heat runs from 0 to 2.0, and the warnings sit at fixed points.** The beep
  starts above 0.7. "Shutdown imminent" plays above 0.95. Dropping back to 0.9
  cancels the shutdown. At 2.0, critical, the mech dies (except in
  multiplayer, below).
- **A mech standing still with ten standard heat sinks sheds 0.0625 heat a
  second**, about 30 seconds from the 2.0 maximum down to zero. Double heat sinks
  cool exactly twice as fast. Each extra sink adds 10%.
- **Wading is a flat x8 cooling, however deep the water.** The code divides the
  water depth by the mech's height and squares the result, which looks like it
  was meant to cool more the deeper you stand. But it takes the larger of that
  ratio and 1, not the smaller, so the result is always at least 16 and gets
  capped at 8. Ankle-deep water cools as well as being neck-deep.
- **Only the left foot counts.** The water check reads the left foot alone. Wade
  in with just your right foot and you get no cooling.
- **Powering down triples your cooling.** While shut down (heat shutdown or the
  power button), dissipation is x3.
- **Lava ignores the frame rate.** Standing on lava (either foot) adds 0.05 heat
  every frame, not every second. Everything else in the heat code scales with
  time. So the faster the game ran, the faster lava cooked you: about 1.5 heat a
  second at 30 fps, 3 at 60 fps. The game waits for the monitor's refresh, so
  in 1999 that topped out around 60-85 fps; at 144 fps today it would be over
  7 a second.
- **Critical hits can destroy heat sinks, but your pilot never says so.** One
  kind of critical hit on a part (0x460d50, type 0) knocks out a heat sink, and
  cooling drops 10% for each one lost. It is meant to play a voice callout, but
  only when the heat sink routine (0x45fbc0) reports success, and that routine
  always reports failure. The sink is gone, silently. Losing a whole part takes
  its sinks with it (0x4617f0).
- **Jump jets and MASC each add 0.05 heat a second** while running, on top of
  weapons. Running hard also cools you slower: past 60% throttle (either
  direction), cooling drops, to 60% of normal at full throttle.
- **Shutdown has stages.** Above 0.95 you get the "shutdown imminent" warning,
  and about 4 seconds later the mech powers down unless you've dropped to 0.9.
  Hitting 1.5 skips the warning and shuts you down at once. Once shut down, the
  mech restarts when heat falls to 0.4.
- **Overriding a shutdown is a dice roll.** If you override the automatic
  shutdown and stay hot (above 0.9), the alarm starts and, every few seconds
  (a random 0.1 to 3, then 3 more), the game rolls. 80% of the time you get
  away with it, but each of those rolls may also cook off an ammo bin, the one
  with the least ammo x damage left, with an explosion sound. The other 20%,
  the torso's damage routine runs and heat resets to zero (exactly what that
  does to the mech isn't traced yet). At 2.0 there is no roll: the torso is
  destroyed. A mech that does shut down skips all of this and just waits to
  cool to 0.4.
- **AI mechs play by the same heat rules as you.** There is a separate, simpler
  rule in the code ("non-player mechs explode at 2.0") behind a switch that is
  always on, so it never runs. AI mechs warn, shut down, cook off ammo and die
  exactly like the player; they just don't hear the beeps.
- **"Heat Death disabled" is the invincibility cheat talking.** The debug
  menu's "Invincible" item (or an option bit) sets a flag on your mech (+0x630)
  that blocks damage and criticals; when heat would have killed you, the game
  prints "Heat Death disabled" instead (damage.md).
- **The shutdown override only works while the alarm is beeping.** "Override
  Thermal Shutdown" (0x419190) does nothing unless the heat warning is already
  sounding. Once on, it cancels the automatic power-down, but not at 1.5 heat,
  which still shuts you down, and it switches itself off when you cool below
  0.4.
- **You can't restart early from a heat shutdown.** The power on/off routine
  (0x41cc30) does nothing while a heat shutdown is in progress; the game powers
  you back up itself once heat is down to 0.4. (Likely what the power button
  calls; not traced from the key yet. The in-game help says the startup key
  "can also restart your 'Mech after an automatic shutdown", which this code
  does not allow.)
- **Powering down waits for your legs.** Pressing power-down only asks; the
  mech actually shuts off once its animation reaches a stop pose (states 1, 12
  or 13), and a power-down asked for while a flag at +0x584 is set (likely
  mid-air) is ignored.
- **Heat can't kill you in multiplayer.** In multiplayer, heat is always on,
  but reaching critical heat never destroys your torso: it only shuts you down
  and resets your heat. In campaign, instant action and training, critical heat
  is fatal (gametypes.md).

## Damage

From the damage code (damage.md).

- **Criticals only happen through thin armor.** A hit can roll a critical only
  when the part's armor is at 15% or less, and then one roll decides: 0.8%
  heat sink, 0.5% ammo, 0.3% equipment, 0.3% weapon, 0.1% "location special"
  (engine hit in the torso, instant death in the head, a limp in a leg).
- **Lasers never cause criticals.** Every laser is a beam weapon, and beams are
  excluded from the critical roll; PPCs, autocannons, missiles and Gauss can.
- **A critical replaces the hit's damage.** When a critical is rolled, that
  hit's armor and structure damage is skipped, even if the critical has nothing
  to destroy.
- **Ammo criticals don't explode.** An ammo critical empties the smallest bin;
  no damage follows. CASE in the part blocks it.
- **Losing one leg kills the mech**; losing an arm or a side torso never does
  (a lost side torso takes its arm with it).
- **A part can't be destroyed while it has armor.** Structure damage can never
  take the internal value below the remaining armor fraction.
- **Difficulty only scales damage you take**: x0.7, x1.0 or x1.3, and not in
  multiplayer.

## Weapons

From the weapon code (weapons.md).

- **The plain PPC can't be mounted.** `weapons.zrd` defines a `PPC`, but the
  MechLab's weapon table (0x5e64f0) only lists the ER PPC, so no mech can
  carry the plain one.
- **Hot lasers recharge slower.** Above heat 0.5, a beam weapon's recharge is
  multiplied by 0.5 / heat (0x4b43bf): at heat 1.0 lasers refill at half speed.
- **Small lasers are the slowest to recharge**: 4 seconds, against 3 for medium
  and large (`weapons.zrd`).
- **Only a weapon's first group counts.** A loadout can list a weapon in two
  groups, like `group (1 2)`, and 59 entries across 11 stock mechs do, but the
  game reads only the first number.
- **One ammo bin per weapon type.** All launchers of the same weapon share one
  bin, whatever part the ammo is listed in; the part only decides which
  critical hit can reach it.
- **CASE stops ammo crits but not heat cook-offs.** CASE in a hit part cancels
  an ammo critical there (0x460e30), but the overheating cook-off roll never
  checks for it.
- **"Fire rate" in the data is really a period.** The weapon reader stores
  1 / `FIRE_RATE` (0x583e87), the time between shots.

## Sound

- **The "medium" sound playback format is never read.** `sounds.zrd` lists
  playback formats as `HIGH`, `MEDIUM` and `LOW`, but the sound setup asks for
  `"MED"` (0x570ccf), an exact match that never hits, so the medium format slot
  (0x72c2e0) stays empty. What the medium sound quality setting falls back to
  isn't traced yet.
- **Sounds are plain WAV files** packed in `soundsL.zbd` (11 kHz, 8-bit) and
  `soundsH.zbd` (22 kHz), 999 of them, named in `sounds.zrd` by set: menus,
  briefings, mech voices, weapons.

## Startup

- **The exe checks itself for stat hacks.** The very first thing the game does
  is a CRC-32 over two tables built into `Mech3.exe`: the mech roster (each
  mech's name, tonnage and class) and the BattleTech internal-structure table
  (structure points by tonnage for head, torsos, arms and legs). If the result
  isn't 0x00efd77d, someone has hex-edited the stats: the intro movies still
  play, but instead of the main menu the game goes to its "Exit State" and
  closes, with no error. Likely aimed at multiplayer cheats (startup.md).
  (Removed in the rebuild, with the other traps: protection.md.)
- **The CD check only looks for the first intro movie**, `video\c1.avi`, on
  any drive Windows calls a CD-ROM. Any disc or mounted image with that file
  passes; no label or key is checked. Without it: "Please insert the
  MechWarrior 3 CD!", OK looks again, Cancel quits.
- **The well-known 1.2 NoCD exe is also a bug fix.** Besides four jumps in
  the disc check, it stops a startup crash on fast PCs (a CPU speed test that
  divides by zero) and keeps a software-mode line from drawing outside the
  screen. Likely why some found it "more stable" than the original.
- **Hidden launch switches**: `-nofmv` skips the movies, `-cdN` forces CD
  music track N in missions, `-seasick` sets an otherwise menu-controlled
  option (meaning not yet known).
- **Turn a cheat on once and the campaign mission doesn't count.**
  Invincibility, unlimited ammo or heat off, even switched off again, mean
  "Replay Mission" instead of "Next Mission" and no score added, until you
  restart the game.
- **In multiplayer the host re-checks the locked rules every 51 seconds**;
  changing them in memory (say, unlimited ammo after the lock) ends the game
  a minute or two later as if the clock ran out.
- **Only one copy runs at a time.** Started a second time, the game finds the
  first window and brings it to the front instead. Launched from a multiplayer
  lobby, it closes the old copy instead.
- **The default mech is the Bushwacker**, set in the app defaults at startup.

## Multiplayer

From the network code (network-messages.md).

- **Your own PC decides whether your mech dies.** Each player's machine runs its
  own mech's damage and death and tells the others. A hacked client could likely
  shrug off hits.
- **The host re-checks the rules every 51 seconds** with a CRC-32 of the game
  settings block, and ends the game if it changed. Right after the check it may
  set bits in that same block, which looks like a deliberate way to end the game
  at the next check (likely).
- **If the host leaves, the game carries on**: a new host is picked instead of
  ending the match.
- **Fewer players, smoother play.** Each mech's position goes out about 7 times
  a second with 2-4 players, down to 3 with 8; the tables stop at 8 players.
- **Craters are shared.** The host checks each crater and sends it to everyone,
  so the terrain damage matches on every screen.

## Graphics

- **The software renderer draws with the stack pointer** (`blit-trick.md`). To
  fill a row of pixels fast, it points the CPU's stack at the screen and
  "pushes" colours onto it, two bytes at a time, backwards. It is quick on a
  Pentium, but anything else that uses the stack meanwhile writes straight into
  the picture. It is also why some modern wrappers (dgVoodoo) choke on the game.
- **The game picks the best texture set it can find.** For each texture
  library it looks for a numbered file first (`rmechtex16.zbd`, then 15, 14,
  ... down to the plain `rmechtex.zbd`) and uses the first one present. On a
  15-bit display every texture's colours are converted as they load, and green
  loses a bit of precision.
- **`lab.flt` in `mechlib.zbd` is likely never loaded.** Every model name the
  game builds starts with `mech_` (0x484760, 0x4847e0), so the one model
  without that prefix sits unused (mechlib.md).
- **All mechs share one set of 442 materials**, read once from `mechlib.zbd`.
- **In GameZ, an animation is a "script".** Every mech animation in
  `motion.zbd` is loaded as a `CZSIScript`, with keyframes for each body part.
  A loose `<name>.zan` file would replace one, a developer hook like the loose
  data files.

## Timing

- **The game clock went bad on machines left on for days.** It reads Windows'
  "milliseconds since the PC booted" into a 32-bit float. After about 4.7 hours
  of uptime the clock can only step in whole milliseconds, and after 10 days in
  64 ms steps, so frames start to judder. A PC that sleeps instead of shutting
  down got there easily. (The rebuild counts from game start: `fixes.md`.)
- **Game speed does not depend on CPU speed.** Everything is scaled by the real
  time each frame took, capped at 1/8 second a frame. The exceptions found so
  far are per-frame, like the lava heat above.

## Mission worlds

From the world files (gamez.md).

- **Each mission world is a memory dump.** `gamez.zbd` is the engine's
  in-memory pools of textures, materials, models and scene nodes written out as
  they were, free slots and stale pointers included.
- **Deleted objects are still in the files.** Free node slots keep the names of
  objects the level designers deleted without clearing; mission 1's world holds
  1,393 of them.
- **The engine's error messages go nowhere.** The GameZ error reporter
  (0x49bfd0) is an empty stub in the shipped game, so every engine error is
  silent; a broken node is just marked dead and loading carries on.
- **Field of view is capped at 80 degrees.** The camera setter clamps each
  view angle to 1.396 radians (0x4f3b90), whatever the data asks for.
- **Sound in the world files was never finished.** Reading a sound node logs
  `Reading sound node data: Must complete software.`, and none of the six
  worlds has one; the animation loader creates the sound nodes at mission
  load instead (anim.md).
- **Fog colour changes under 1% are ignored.** The renderer keeps its fog
  colour unless some channel moves by at least 0.01 (0x567eb0), and clamps
  each to 0..1 first, writing the clamp back into the world.
- **Each world file has exactly one light node.** The other lights are made
  by the animation loader at mission load (anim.md).
- **Worlds are loaded by a script.** Mission load runs a script whose
  `GameZReadZBDFile` command reads the world; the matching
  `GameZWriteZBDFile` survives inside a disabled `ifdef COMPILE` block, a
  leftover from the tool that built the files.

- **A typo in mission 1's world script.** The c1 build script says
  `Object3DSetScrllAlways` after finding the node `ocean52`; the command is
  `Object3DSetScrollAlways`, so that water surface never scrolls (interp.md).
- **The world compiler's commands are still in the scripts.** 385 lines call
  `LoadGameGen`, a command the shipped exe doesn't have; they sit in a branch
  that only runs if `gamez.zbd` fails to load, so the fallback could never
  have worked. `SetModelDirectory` runs every mission and is silently ignored.
- **There is a hidden "GameZ Interpreter" console** (0x5960a0) where you could
  type commands like `var DamageScale = 0` or `PrintTree`, but in 1.2 it is
  likely unreachable (interp.md).
- **Modding catch:** if any loose script next to the exe has a different file
  time from the one stored in `interp.zbd`, the game rejects the whole archive,
  so a modder has to extract all 44 scripts.

## Drawing the world

From the render path (render.md).

- **Every map has invisible mission boundaries.** Flat red and yellow models
  under `inout` mark each mission variant's in/out-of-bounds area. At
  mission load the AI reads the one it needs, then the whole set is deleted
  before the first frame, so nobody ever sees them.
- **Trees, docks and bridges carry invisible helpers**: loudly coloured
  (magenta, pink, white) shapes with zero opacity, at the trunk, canopy height
  or deck. Likely collision or targeting helpers the artists coloured to see
  them in the editor.
- **The fake shadows are see-through black polygons** under crates, tanks and
  buildings, not computed shadows.
- **Fog starts at the cockpit in hardware mode.** The missions ask for fog from
  500 to 750 m in the software renderer but from 32 m in hardware, and it is
  ground fog: thick below 80 m altitude, gone above 100.
- **The fog is always the colour of the sky band**, in every world.
- **The sun only darkens.** Lighting can make a surface darker than its
  texture, never brighter.
- **The sky is a dome that follows you** (position only), drawn past the far
  plane so it never gets cut off.

## Mechs: camos

From the mech definitions (mechdef-skins.md). The camo names are GUNMETAL,
SUNFIRE, DESERT, WASP, SLATE, JUNGLE, ARCTIC and SPECTRE.

- **The Firefly has two WASP camos and no SLATE.** Its fifth skin is
  labelled `MSG_SKIN_VARIANTD` instead of `VARIANTE`, so both the fourth and
  fifth skins are named WASP; the second one uses the `_e` (Slate) textures.
  The data is certain; that the skin picker shows the name twice is not checked
  in game.
- **The Mad Cat's damage list is shifted by two.** Damage textures pair with
  skin textures by position, and the Mad Cat's damage list has two extra zeros
  before its six damage masks. So `mcat_d01`/`mcat_d02` get none, and
  `mcat_d03`/`mcat_d04` (64x64) get a 128x128 mask, which the size check
  0x474740 rejects (sizes from `rmechtex.zbd`, by the primary name). The
  Supernova has one extra damage item too; `supern_d03` (128x128) is paired
  with a 128x64 mask, which fits only its alternate `supernova18`. Whether the game logs and drops these at run time is
  unverified (it depends on which name the entry loads, +8 or +0xc).
- **Variant camos are only suffixes.** Every camo after the first is the
  primary list with `_b`, `_c` ... appended to each texture name; the files
  never list variant textures one by one.
- **Elementals never show bullet holes**: their damage list is all zeros.
- **A dead special case**: the texture loader treats an entry named
  `vult_d03_c` specially, but no file makes that name (the Vulture's is
  `vultur_d03_c`), and no texture library has it.
- **Five primary textures are missing from every library**: `bushwacker14`,
  `blkhwk16`, `cauldron01`, `cauldron08`, `owens13` (not in `rmechtex*.zbd` or
  `rimage.zbd`). Probably unused by the models; not checked.

## Mechs: firepoints

From the mech definitions (mechdef-parts.md).

- **Firepoint `radius` and `share` do different jobs.** `radius` is how far
  each missile of a salvo scatters from its firepoint; `share` is the
  sideways offset, random but chosen once when it is mounted, given to a
  second weapon on the same firepoint, so
  the two do not fire from exactly the same spot. Almost every mech has
  0.5 for both, so it rarely shows.
- **Elemental weapons do not spread.** The Elemental's arm firepoints have no
  radius or share at all (0), and its torso ones only a radius of 0.2.
- **TAG always fires from the first firepoint** in the mech's list
  (`firepoint01`, a right-arm point on every shipped mech), whatever the
  loadout.
- **A leftover `parts` lookup.** The reader asks every mech file for a
  `parts` entry that no file has, then ignores whatever it would have found:
  the same 13 part names are set for every mech. The left and right side
  torsos are not among them (their slots stay unnamed); whether anything
  reads these slots is not known yet.
- **Firepoints in the dfn name a part, but the game aims from the model.**
  The weapons find their firing part from the 3D model node with the
  firepoint's name (0x46cb80), not from the `part` written in the file; the
  file's part is only reached through the config item (0x473220).

## Mechs: loadouts

From the mech definitions (mechdef-configs.md).

- **The Bushwacker has four hidden training loadouts**, `training1`..`training4`
  (one with no weapons and three jump jets, one with only two machine guns,
  one with 2 tons of armor). Their names start with `train`, which sets a
  flag on the loadout (+0x1e). What the flag does (likely: the training
  missions use them and the mech lab hides them) is not traced.
- **The Sunder's Alternate B loses 240 LRM missiles to a typo**: its file puts
  `extra (120 120)` beside the LRM20's `ammo` instead of inside it, so only
  the first 120 are loaded (checked in the emulator).
- **The Elemental's armor does not add up**: 1 ton of standard armor is worth
  16 points and the file hands out 11. The game checks and writes an
  "ARMOR ALLOCATION ERROR", but error output is compiled out, so nobody ever
  saw it. Every other loadout adds up exactly.
- **Armor points per ton**: 16 standard, 19.2 Clan ferro-fibrous, 17.92 Inner
  Sphere ferro-fibrous; armor tonnage is rounded to the nearest half ton.
- **Heat sinks**: every loadout gets 10 free, plus one per listed sink. Single
  or double is decided by the last sink in the list, so a mix would count as
  all one kind (no shipped loadout mixes them).
- **The `max_speed` in every loadout's engine block is ignored**: nothing in
  the game reads it from a mech file.
- **Turning comes from the mech, not the loadout**: every loadout gets half of
  the file's `max_turn_rate` and the mech's acceleration values; the per-engine
  override the code allows is never used.

## Mechs: the definition file and saved loadouts

From the mech definitions (mechdef-head.md).

- **The Strider has no recoil slide because of a typo**: its file says
  `recoil_slide ("1.5.0")`, which is text, not a number, so the game keeps 0.
  Its neighbours (Cauldron, Bushwacker, Champion) have 1.5.
- **Every mech's maximum engine is set to 400** right after it is read; the
  per-mech maximums in the files (315 for the Bushwacker, 290 for the light
  mechs) are thrown away. Whether the mech lab uses this limit is not traced.
- **The Elemental has no crouch height or engine range** in its file, so it
  gets the defaults (1.0, 10..400).
- **Saved loadouts are tamper-checked.** The mech lab's loadouts go to
  `configs\<mech>.mw3`, with a CRC-32 checksum. Edit one by hand and the game
  silently drops it at load. The game also re-checks weight (0.24 t of
  slack), armor points and jump jets (at most 2 for mechs up to 40 t, 3 up to
  65 t, 4 above). **Elemental loadouts are never checked.** (The rebuild
  drops the checksum: hand-edited loadouts load if they are legal.)
- **The Mad Cat gets half a ton for free.** The loadout weight check takes
  0.5 t off for the Mad Cat alone (0x425387), on top of the 0.24 t of slack
  every mech gets. Whether the mech lab's own weight display does the same is
  not checked.
- **Every mech's weight includes a fixed 3 t cockpit** plus engine shielding
  of 1 t per 100 rating, rounded up (a 300 engine adds 3 t).
- **A damaged loadout file can hang or crash the game** at load: a broken
  item count makes it read 4 billion times past the end, a broken name length
  makes it ask for 4 GB of memory. (Both fixed in the rebuild.)
- Your loadouts are written when the game frees the mech data (at exit),
  not when you leave the mech lab (as far as traced).
- **A mod with a wrong `version` in a mech file crashes the game** instead of
  skipping the mech. (Fixed in the rebuild: the mech is skipped.)
- Seventeen of the mech files carry a developer comment that the build tool
  kept as data: "torso limits: maxYaw minPitch maxPitch (degrees)", which is
  how we know the torso fields' names.

## Mission animations

From the animation files (anim.md).

- **Copies of the same effect find their own object in order.** When several
  animations in a row are meant for objects with the same name (up to 58 in
  a row on c1), the game hooks the first to the first such object, the
  second to the second, and so on. If there are more animations than
  objects, it wraps round to the first.
- **The last animation in a mission cannot be a prerequisite.** When one
  animation waits for another, the search stops one short of the end of the
  list, so the very last animation is never found (0x50aaa3). No shipped
  mission depends on this. (Fixed in the rebuild.)
- **One missing object wipes all of an animation's prerequisites.** If an
  animation's "wait until this object is active" condition names an object
  that is not in the level, the game drops every condition of that
  animation, not just the broken one, and the animation can then fire
  without waiting.
- **Every animation's reset script runs once while the mission loads**, and
  what it leaves is the starting state of the level (anim-postload.md).
- **Lights, sounds and smoke tied to animations start switched off**: in the
  first training mission that is 63 lights, 17 sounds and 58 smoke puffers,
  waiting for their animation.
- **94 "when shot" animations have 0 health**, so any hit sets them off, even
  one doing no damage.
- **The file remembers where the developers' PC kept things in memory.**
  Every anim.zbd still holds the memory addresses from the machine that
  built it (mech3ax uses them to tell the versions apart). The game throws
  them away and looks every object up again by name.
- **The list of source files is read and ignored.** Each anim.zbd starts with
  the names and dates of the text files it was built from; the game reads
  past them without looking.

## Data files

- **Every setting lives in `reader.zbd`**, 178 small files of nested lists
  (mechs, weapons, sounds, HUD, briefings) (`zreader.md`).
- **The data lookup can't tell names from values.** Asking for a key finds the
  first matching string anywhere in a file, even if that string is someone's
  value. And when the match is the last item of a list, the game reads whatever
  happens to be next in memory.
- **The code knows two cameras no mech uses.** Each mech's data can set up
  six camera kinds (0x430e70): `tether`, `attach` (the cockpit), `area`,
  `mcbtether` (likely the mobile field base), `zippercam`, named after Zipper
  Interactive, and `godseye`. No data file in the game defines the last two.
- **Some keys never match the data.** Comparing every key the exe looks up
  with every string in the 55 reader files finds 118 keys the data never
  sets. Most are optional settings, but some are spelling mismatches: the
  weapon code reads `BOUNCES` while `weapons.zrd` says `BOUNCE` (always 0, so
  harmless), and the sound code's `MED` (above).
- **Loose text files override the archive.** The game first looks for a
  developer text version of each file in `data\common\zrdr` and only falls back
  to `reader.zbd` when there is none. None ship with the game, but the hook is
  still there.

## Game states

- **The menu/state stack has no real guard rails** (`zgameapp.cpp`). Too many
  "pops" leave it resuming the bottom screen, and too many "pushes" overwrite
  the top one. Pushing onto an empty stack would crash; the game always starts
  with a "replace", so it never happens.
