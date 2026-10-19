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
- **Worlds are loaded by a script.** Mission load runs a script whose
  `GameZReadZBDFile` command reads the world; the matching
  `GameZWriteZBDFile` survives inside a disabled `ifdef COMPILE` block, a
  leftover from the tool that built the files.

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
