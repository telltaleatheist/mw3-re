# mw3-re

A rebuild of **MechWarrior 3** (Zipper Interactive, 1999) in modern C++,
running natively on macOS and Windows on SDL3, with no Wine and no
DirectX wrappers.

It is not a remake. The original `Mech3.exe` (patch 1.2) is taken apart with
Ghidra and [REA](https://github.com/morluto/rea), and its code is ported back
function by function. Every ported function keeps its original address in a
comment (`// Mech3.exe 1.2 @ 0x0041a2b0`), and nearly every one is checked
against the original in an x86 emulator. The game should play the way it
always did, minus the bugs and the problems it has on modern machines.

This is a personal fan project. No original game files are in this
repository: the rebuild reads them from your own copy of the game.

## Where it stands

**Not playable yet.** About 74,000 lines are ported so far.

| Area | State |
|---|---|
| Every data format (worlds, models, textures, mechs, animations, scripts, sounds) | Loads, identical to the original |
| The 3D renderer | Ported end to end; draws the mission worlds |
| Start-up, main loop, game states, settings, controls setup | Ported |
| Main menu, options, controls screen, credits, pilot screen | Ported |
| Mech building, movement, physics, jump jets, falls | Ported |
| Heat, power, damage, critical hits | Ported |
| Weapons, missiles, beams, anti-missile, TAG | Ported |
| Mission scripts, triggers, timers, animations, smoke and fire | Ported |
| AI: path networks, controllers, sensors, targeting | Partly ported; the AI's decisions are next |
| Cockpit view, mech placement | In progress |
| HUD, briefings, campaign screens, mech lab, multiplayer | Not yet |

Today `mw3` starts, runs the intro, and reaches the real main menu.
`mw3 --mission c1m1` loads a mission: it builds your mech and the enemy mechs
and runs the mission's frame, but there is no HUD or cockpit view yet.

## What makes it different

- **Checked against the original.** Differential tests run the original x86
  code in the [Unicorn](https://www.unicorn-engine.org/) emulator and the
  rebuilt C++ side by side, on the same inputs, and compare everything that
  comes out. There are 85 of these tests, and break tests (deliberate
  mistakes put into the port) show that they catch errors.
- **Bugs fixed, quirks kept.** Where the original has a clear bug (crashes,
  typos in data, physics that change with your frame rate), the rebuild fixes
  it. Where something is a quirk that players know and plan around, it stays.
  Every change from the original is listed in plain words in
  [`CHANGES.md`](CHANGES.md).
- **Runs right on fast machines.** The original ties falls, slopes, jump jets,
  smoke and more to the frame rate, which is why it goes wrong on modern PCs.
  The rebuild runs those at the original's 30 fps pace whatever your frame
  rate.

## Discoveries

Taking the game apart turned up hundreds of hidden mechanics, quirks and
bugs, collected for players in
[`docs/discoveries.md`](docs/discoveries.md). A few:

- Lasers never cause critical hits, and criticals only happen once a part's
  armor is down to 15%.
- Ammo criticals don't explode: they just empty a bin. CASE in a side torso
  does nothing at all, and the stock Mad Cat carries both of its CASE there.
- Ultra autocannons never jam; they simply fire twice as fast.
- TAG works, but the missiles home on the spot you marked, not the target,
  and take 7 seconds to arrive.
- Wading in water nearly as deep as the mech is tall slows it almost to a
  standstill, while deeper water lets it speed back up. That is a bug, and
  it's fixed here.
- Pilot files track hidden Instant Action and multiplayer stats that the game
  never shows.

## Build

Needs CMake 3.20+, a C++17 compiler and, for the program itself, SDL3 and
SDL3_ttf (macOS: `brew install cmake ninja sdl3 sdl3_ttf sdl3_sound`).

```bash
cmake -S . -B build -G Ninja && cmake --build build
./build/mw3tests          # the unit tests
```

## Run

Point it at a MechWarrior 3 install patched to 1.2:

```bash
./build/mw3 path/to/mw3-1.2              # the original start-up and main menu
./build/mw3 --mission c1m1               # straight into a mission
./build/mw3 --fly c1                     # a free camera over a mission world
```

Music: the original plays CD audio. Rip the disc's tracks to
`music/track02.ogg`, `track03.ogg` and so on (ogg, flac, wav or mp3) in the
game folder, numbered as on the disc. More in [`docs/program.md`](docs/program.md).

## The differential tests

They need your game files in `game/mw3-1.2/` and Python with Unicorn:

```bash
python3 -m venv .venv && .venv/bin/pip install unicorn
tests/differential/run_all.sh
```

## Layout

- `src/gamez/<module>/`, `src/mech3/`: the rebuild, laid out like the
  original source tree recovered from the executable's debug strings
  ([`docs/module-map.md`](docs/module-map.md)).
- `src/platform/`, `src/frontend/`: SDL3 in place of Win32, DirectX and MCI.
- `docs/`: how each system works, the evidence, and the open questions.
  [`docs/fixes.md`](docs/fixes.md) is the technical record of every change.
- `tests/`: unit tests, and the differential tests against the original.
- `tools/`: the analysis scripts.

MechWarrior is a trademark of its owners. This project is not affiliated with
them; you need your own copy of the game.
