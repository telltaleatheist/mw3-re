# Sanitizer runs (crash hunting)

AddressSanitizer and UndefinedBehaviorSanitizer over the port's long runs,
to find crashes before a player does (as W1, TARGET's view of a turret, was
found by Owen). Ours; the method and what each run found.

## The build

    F="-fsanitize=address,undefined -fsanitize-recover=address -fno-omit-frame-pointer"
    cmake -S . -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug \
      "-DCMAKE_C_FLAGS=$F" "-DCMAKE_CXX_FLAGS=$F" \
      "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined"
    cmake --build build-asan --target mw3 mw3tests mw3sdltests
    export ASAN_OPTIONS=halt_on_error=0:detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1

`-fsanitize-recover=address` with `halt_on_error=0` reports every error and
goes on, so one run lists them all; UBSan reports each place once. The probe
library (the differential tests) is not built this way.

## 2026-10-30 (main 873bf47)

| Run | Time | Reports |
|---|---|---|
| mw3tests, all 232 | 49 min | 3 UBSan, one test's (below) |
| mw3sdltests, all 21 (and the programs they start) | 3 min | 1 ASan (below) |
| the kills chain, all 20 missions to the credits (`--campaign-chain kills:20 --frame-ms 33 --seed 1999`) | 22 min | the same ASan, in mission 2 |
| c1m1 near the Owens, near a turret; c1m2 with a vehicle ahead; c4m2 near the Champion; Instant Action map 1 wave 3: fire held, the MFD, TARGET and the target (E, Q, W) stepped every 60 frames, 5000 frames each | 19 min with the two below | none |
| the lab (`--lab-mech 9`): the three tabs, four skin swaps | | none |
| the briefing, the lab (tabs, skins), accept, launch (`--briefing c1m2`) | | none |
| `mission_c1m3_hud_views_follow_targets_as_they_die_headless` (new) | 1 min | none |

The held-fire runs did not kill what they faced (the fire is not aimed: the
turret stood behind a ridge, the Owens below the view), so the deaths of
targeted objects are the new test's: c1m3's hostiles in turn, each kind
(3 mechs, 6 vehicles, 6 turrets, 5 targets.zrd buildings) shot dead through
zWeapon's impact while TARGET shows it, TARGET and the MFD stepping through
their modes every 10 frames, 20 frames on each wreck.

## What they found

- **The waypoint arrow kept a released image** (ours to fix, the exe's
  behaviour): the arrow is one zTwoD image for the whole run; the last
  waynet's end (0x00479850) releases the arrow images; in the next mission
  the first arrow at the right or bottom edge reads the arrow's image size
  before it sets its own (0x00479ef0), a freed image. The exe makes the same
  read (zImage's memory); in the port it is a heap use-after-free, a crash
  waiting on Windows. FIX (bug): the free clears the arrow's image
  (fixes.md; `fix_waynet_arrow_lets_go_of_released_images`).
- **A test's Mech on the stack** (`fix_pocket_frame_without_a_hud`): bools
  of 120 and 6. `Mech`'s plain fields had no initialisers; the program's
  `new Mech()` zeroes them, a `Mech m;` does not (28 probes and tests make
  one so). They are value-initialised in the struct now.
