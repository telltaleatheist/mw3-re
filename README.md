# mw3-re

A learning project: studying how MechWarrior 3 (Zipper Interactive, 1999) works,
using REA (https://github.com/morluto/rea) with Ghidra, and rebuilding its
mechanics as new code.

No original game files are in this repository. Everything that reads the game
reads the player's own copy (patch 1.2).

## Build and test

Needs CMake 3.20+ and a C++17 compiler (macOS: Xcode command line tools).

```bash
cmake -S . -B build -G Ninja && cmake --build build && ./build/mw3tests
```

The differential tests run functions from the original `Mech3.exe` in the
Unicorn x86 emulator and compare them with the rebuilt C++. They need the game
files in `game/mw3-1.2/` (see CLAUDE.md) and Unicorn:

```bash
python3 -m venv .venv && .venv/bin/pip install unicorn
tests/differential/run_all.sh   # all of the below
.venv/bin/python tests/differential/test_spans.py
.venv/bin/python tests/differential/test_ztimer.py
.venv/bin/python tests/differential/test_zreader.py   # needs game/mw3-1.2/zbd/reader.zbd
.venv/bin/python tests/differential/test_heat.py
.venv/bin/python tests/differential/test_power.py
.venv/bin/python tests/differential/test_motion.py
.venv/bin/python tests/differential/test_mechlib.py
.venv/bin/python tests/differential/test_gamez.py
.venv/bin/python tests/differential/test_textures.py  # needs the zbd texture files; ~4 min
```

## Layout

- `src/gamez/<module>/`, `src/mech3/`: the rebuild, laid out like the original
  source tree recovered from the exe (`docs/module-map.md`). Each ported
  function carries its original address.
- `src/platform/`: what the rebuild needs from the machine, in place of Win32,
  DirectX and MCI.
- `src/crt/`: the original C runtime's behaviour where it matters (`rand()`).
- `docs/`: findings: `architecture.md`, `module-map.md`, `blit-trick.md`,
  `fixes.md`, `symbols.csv`.
- `tools/`: the analysis scripts and the Ghidra script.

## Run

With SDL3 and SDL3_ttf (`brew install sdl3 sdl3_ttf sdl3_sound`) the build
makes `build/mw3`, the game on SDL3: `build/mw3 path/to/mw3-1.2` runs the
original start-up; `build/mw3 --mission c1` starts straight into a mission
world with a free camera (`docs/program.md`).

Music: put the CD's audio tracks in `music/` as `track02.ogg`, `track03.ogg`
and so on (ogg, flac, wav or mp3), numbered as on the disc, in the write dir
or the game folder (`docs/program.md`).
