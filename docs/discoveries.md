# How MechWarrior 3 really works

Findings from taking `Mech3.exe` 1.2 apart, written for players. Each one comes
from the game's own code; the address and the technical write-up are given so
anyone can check. "Likely" marks a guess.

## Heat

From the heat update at 0x4192f0 (details: `heat.md`).

- **Heat runs from 0 to 2.0, and the warnings sit at fixed points.** The beep
  starts above 0.7. "Shutdown imminent" plays above 0.95. Dropping back to 0.9
  cancels the shutdown. At 2.0, critical, the mech dies.
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
  second at 30 fps, 3 at 60 fps.
- **Jump jets and MASC each add 0.05 heat a second** while running, on top of
  weapons. Running flat out also cools you slower: the cooling rate drops once
  the controls value passes 0.6 (likely the throttle).
- **Shutdown has stages.** Above 0.95 you get the "shutdown imminent" warning,
  and about 4 seconds later the mech powers down unless you've dropped to 0.9.
  Hitting 1.5 skips the warning and shuts you down at once. Once shut down, the
  mech restarts when heat falls to 0.4.
- **Staying hot while shut down is a dice roll.** Every few seconds (3 seconds
  plus a random 0.1 to 3) the game rolls. 80% of the time you ride it out, but
  each of those rolls may also cook off an ammo bin, the one with the least
  ammo x damage left, with an explosion sound. The other 20%, the torso's damage
  routine runs and heat resets to zero (exactly what that does to the mech isn't
  traced yet). At 2.0 there is no roll: the torso is destroyed.
- **AI mechs play by the same heat rules as you.** There is a separate, simpler
  rule in the code ("non-player mechs explode at 2.0") behind a switch that is
  always on, so it never runs. AI mechs warn, shut down, cook off ammo and die
  exactly like the player; they just don't hear the beeps.
- **There is a hidden "Heat Death disabled" switch** (mech +0x630). With it set,
  hitting critical heat prints "Heat Death disabled" on the HUD instead of
  killing you. Likely a cheat or a developer toggle; how it is turned on is not
  traced yet.
- **You can't restart early from a heat shutdown.** The power on/off routine
  (0x41cc30) does nothing while a heat shutdown is in progress; the game powers
  you back up itself once heat is down to 0.4. (Likely what the power button
  calls; not traced from the key yet.)
- **One mission mode never kills from heat.** In "mission mode 3" (not
  identified yet) heat is always on, but reaching critical only resets it.

## Startup

- **The exe checks itself for stat hacks.** The very first thing the game does
  is a CRC-32 over two tables built into `Mech3.exe`: the mech roster (each
  mech's name, tonnage and class) and the BattleTech internal-structure table
  (structure points by tonnage for head, torsos, arms and legs). If the result
  isn't 0x00efd77d, someone has hex-edited the stats, and the game picks its
  "Exit State" as the first screen: it just closes, with no error. Likely aimed
  at multiplayer cheats (startup.md).
- **Only one copy runs at a time.** Started a second time, the game finds the
  first window and brings it to the front instead. Launched from a multiplayer
  lobby, it closes the old copy instead.
- **The default mech is the Bushwacker**, set in the app defaults at startup.

## Graphics

- **The software renderer draws with the stack pointer** (`blit-trick.md`). To
  fill a row of pixels fast, it points the CPU's stack at the screen and
  "pushes" colours onto it, two bytes at a time, backwards. It is quick on a
  Pentium, but anything else that uses the stack meanwhile writes straight into
  the picture. It is also why some modern wrappers (dgVoodoo) choke on the game.

## Timing

- **The game clock went bad on machines left on for days.** It reads Windows'
  "milliseconds since the PC booted" into a 32-bit float. After about 4.7 hours
  of uptime the clock can only step in whole milliseconds, and after 10 days in
  64 ms steps, so frames start to judder. A PC that sleeps instead of shutting
  down got there easily. (The rebuild counts from game start: `fixes.md`.)
- **Game speed does not depend on CPU speed.** Everything is scaled by the real
  time each frame took, capped at 1/8 second a frame. The exceptions found so
  far are per-frame, like the lava heat above.

## Data files

- **Every setting lives in `reader.zbd`**, 178 small files of nested lists
  (mechs, weapons, sounds, HUD, briefings) (`zreader.md`).
- **The data lookup can't tell names from values.** Asking for a key finds the
  first matching string anywhere in a file, even if that string is someone's
  value. And when the match is the last item of a list, the game reads whatever
  happens to be next in memory.
- **Loose text files override the archive.** The game first looks for a
  developer text version of each file in `data\common\zrdr` and only falls back
  to `reader.zbd` when there is none. None ship with the game, but the hook is
  still there.

## Game states

- **The menu/state stack has no real guard rails** (`zgameapp.cpp`). Too many
  "pops" leave it resuming the bottom screen, and too many "pushes" overwrite
  the top one. Pushing onto an empty stack would crash; the game always starts
  with a "replace", so it never happens.
