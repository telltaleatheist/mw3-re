# World parameters: world.zrd and mpinfo.zrd

Mech3.exe 1.2, 2026-10-19, from the asm of 0x491de0 (`mech3\mission.cpp`;
Ghidra's pseudocode drops the key strings and the fastcall ECX/EDX).
Ported to `src/mech3/mission_world.*`, emulator-checked
(`tests/differential/test_world_params.py`, all identical). Survey:
`tests/differential/explore/world_params_survey.py`.

## Where it is called

Mission load 0x490ab0 (0x490ba6) calls it on the mission object with
`("world.zrd", <mission name>)`; the name comes from 0x490600 (the game's
mission number, vtable +0x4c, through 0x490590): `m1`.., `ia1`.., `mp1`...
Each mission folder's `zbd/<world>/reader.zbd` has one `world.zrd` with a
block per mission of that world.

**Multiplayer maps 3 and up** (game type 3, 0x479480, and mission number > 2)
read `mpinfo.zrd`'s `world_params` instead. Only c3 has them, in
`readermp3.zbd` and `readermp4.zbd`; the world.zrd blocks `mp1`/`mp2` serve
the first two maps of every world.

## The object

The mission object's first 0x4c bytes (ctor 0x491d70 sets only +0..+9;
teardown 0x491d80):

| Off | Field | Default before the read |
|---|---|---|
| +0x00 | weather particle system (0x98 bytes) | kept |
| +0x04 | handle (new(8), vtable 0x5c6108) to the `sunlight` node | set if that node exists, before the file is opened; a second read leaks the first |
| +0x08 | cloud pattern read (byte) | |
| +0x09 | fog block read (byte) | |
| +0x0c | fog range near, far | kept |
| +0x14 | fog altitude (two) | kept |
| +0x1c | fog colour rgb (0..1) | kept |
| +0x28 | sky node (horizon child) | 0 when a fog block exists |
| +0x2c | fog `state` (int) | kept; no file has it and nothing here reads it |
| +0x30 | underwater range | (0, 250) |
| +0x38 | underwater altitude | (0, 100) |
| +0x40 | underwater colour | (50, 50, 80): 0..255-looking values, unlike every file's 0..1 |

Globals: 0x5e5d3c ambient temperature, 0x5e5d40 its inverse, 0x5e5d44 jump-jet
ceiling (startup 0x47f2b0/0x47f320: 20, 0.05, 150).

## The block, in read order

All lookups are zReader `find` (depth first) inside the block.

1. `fog` (+9 = 1, else 0): `range` vec2 -> +0xc; on hardware ([0x72f348])
   `hwrange` vec2 overwrites it; `color` vec3; `altitude` vec2; `state` int;
   `node` string -> list-6 find by name -> +0x28. A `fog` without `node` passes
   a null name to 0x4f7fe0, which faults in its string compare (the port
   inherits it; no file lacks it).
2. `underwater`: `range`, `hwrange` on hardware, `color`, `altitude`, as fog.
   Nothing here pushes it anywhere; the swimming code reads the fields
   **[unverified: who]**.
3. `jet_ceiling` float, default 150 every read -> 0x5e5d44. Then the puffer
   tick reset 0x549410, always.
4. `global_wind`: `max_speed` -> 0x54b650 (0x72be4c), `random_accel` ->
   0x54b660 (0x72be54), `random_angvel` degrees -> radians -> 0x54b670
   (0x72be50), `velocity` vec3 -> 0x54b630 (0x72be38). Each only if present.
   (The wind and the tick reset are ported in gmod_puff: puffers.md.)
5. `sun`, only when the sunlight handle exists: the light's current Euler
   rotation (0x4fb060) and its diffuse (+0x9c) / ambient (+0xa0) are the
   defaults; `orientation` degrees (all three times 0.01745329251994, the
   exe's pi cut to 3.1415926535892), `diffuse`, `ambient`; then set rotation
   0x4faff0, ambient 0x4fa940, diffuse 0x4fa9b0 (the `LightSet*` commands).
6. `ambient_temperature` float, default 20; zero, negative or NaN become 20.
   0x5e5d3c = t, **0x5e5d40 = 1/t**.
7. `weather`: `particles` int (default 100), `type` string compared exactly:
   `SNOW` -> new(0x98) + 0x5a5ad0(particles), `RAIN` -> 0x5a60a0; anything
   else (or no type) creates nothing and **keeps the weather of an earlier
   read**. With a weather object: `color` int vec3 -> packed by 0x579370 (the
   display's 5-6-5, bytes truncated) into +0x44 (u16); `wind` block: `dir`,
   `vel` (each default 0) -> +0x68, +0x6c; `gravity` (default 0) -> +0x70;
   `alpha_gradient` vec2 -> +0x48, +0x4c; then added to the particle
   manager 0x736328 (0x59fbe0). `gravity` is only ever the weather's, the
   rain's fall speed; world gravity is the `WorldSetGravity` command
   (interp.md). The rain itself (the classes, its update and draw) is in
   `weather.md`; the program makes both since item 19.
8. `fluctuate` (+8 = 1): `period` (5) -> 0x543920 (stores 2pi / period),
   `wavelength` (500) -> 0x543940, `base` (0.7) and `amplitude` (0.3) ->
   0x543960 (clamps base to 0..1 and |amplitude| to base and 1 - base),
   `heading` degrees (0) -> radians -> 0x5439f0 (sin, cos). Defaults are the
   values written when a key is missing; the file values are in the table.

After the block (also when the file or key is missing):

- 0x492660(1): weather vtable +0x60(-1.0); the sunlight node active **only on
  the Direct3D renderer** ([0x72f348]); cloud pattern on (0x543910) if read.
  0x492660(0) stops the weather (+0x64), switches the sun and the pattern off.
- If the `HWCardFlag` option (0x40d4d0, [[0x5f4ccc]]) is 0 the sun is
  switched off again **[unverified: what the option means]**.
- The weather gets the player view's camera node ([[0x5f4c8c]+4], read
  without a check) at +0x50.
- With fog and a world object ([0x608c84]): world fog type 1, range, colour,
  altitude (0x4f23e0..0x4f2440; pushed later by 0x4f1df0, render.md). Then,
  with a sky node and a player view, every child of the camera's horizon
  (camera data +8) is set active if it is the sky node, inactive otherwise.
- Without fog but with a world: the world's fog is copied back into
  +0xc..+0x24 (0x4f24c0, 0x4f24a0, 0x4f24e0).
- The tree is freed; in software, 0x566250 runs on the fog colour and then
  the underwater colour. Both write [0x5efba4] (0x5195b0 of the colour), so
  the underwater colour's result is the one left **[unverified: what reads
  0x5efba4; likely the software fog's palette index]**.

## Shipped values

46 blocks. Common to all: `global_wind` velocity (0 0 0), max_speed 13,
random_accel 2, random_angvel 10; fog altitude (80 100) and range (500 750)
except where noted; underwater range (0 250), altitude (80 100). No block
has `state`, `hwrange` under `underwater`, or `gravity` outside `weather`.

| World | Missions | Fog colour | hwrange | Sun (orient. / diffuse / ambient) | Temp | Weather | Clouds (period wave base amp heading) |
|---|---|---|---|---|---|---|---|
| c1 | all | 0.8157 grey | 32 750 | m2 (-20 22 0) 1/0.4; m3 (-20 -22 0); m4 (-20 -45 0) | 40 | - | 8 600 0.7 0.3 30 |
| c2 | m1, m3, ia, mp | 0.464 0.496 0.464 | 32 750 | m3 (-30 -50 0) 1/0.3 | 20 | - | 8 600 0.8 0.2 30 |
| c2 | m2 | `horblack`, 0.02 grey, alt (-125 -90), range = hwrange (375 500) | | (-60 -20 0) 0/0.35 | 15 | - | none; **jet_ceiling -90** |
| c2 | m4 | `horblack`, 0.01 grey, alt (-47 -15), range = hwrange (375 500) | | (-60 -50 0) 0/0.35 | 15 | - | none |
| c3 | all (m2, m5 17; m3, m4 16; mpinfo mp3 17) | 0.125 0.125 0.1875 | 32 750 | - | 18 | mp2 only: RAIN | none |
| c4 | m1 | 0.7343 0.5625 0.3867 | 32 750 | - | 31 | - | 8 600 0.6 0.3 30 |
| c4 | m2, m3, ia, mp | same | 32 750 | m2 (-18 -100 0) 1/0.4; m3 (-15 -110 0) 0.9/0.4 | m2 30, m3 29, others 31 | RAIN | 8 600 0.6 0.3 30 |
| c4b | m4 | 0.3515 0.4804 0.5156 | 32 750 | - | 2 | SNOW | none |
| c4b | m5 | `horblack`, red 0.9 0.01 0.01, alt (-65 -3), range (200 500) | 64 350 | (-60 -20 0) 0/0.35 | 40 | - | none; **jet_ceiling -3** |
| c4b | m6 | as m4 | 32 750 | (-30 145 0) 0.7/0.4 | 10 | SNOW | none |
| t1 | m1..m4 | 0.7109 grey, alt (70 85) | 64 750 | - | 31 (m3 32) | RAIN | none |

Weather blocks: RAIN in c3/c4: 100 particles, colour (128 128 172), wind
dir 0 vel 0.2, gravity 3, alpha_gradient (0.5 0); t1 RAIN: 60 particles,
(165 165 165), vel 0.1, gravity 0.8, (0.5 0); c4b SNOW: 100, (245 245 245),
vel 0.1, gravity 0.2, (1 0.25). Underwater colours per world: c1 (0.2695
0.352 0.414), c2 (0.3242 0.4453 0.4179), c3 (0.109 0.117 0.129), c4 (0.273
0.289 0.32), c4b (0.1758 0.4727 0.7227), t1 (0.211 0.258 0.223).

Missions without a `sun` block keep the world file's light (render.md
section 5: per world diffuse/ambient).

## Findings

- **Heat sinks work by the weather.** The heat code's dissipation factor
  0x5e5d40 (heat.md: `dissipation = rate * 0.05`) is `1 / ambient_temperature`,
  set here per mission; 0.05 is only the default for 20 degrees. So mechs
  cool **half as fast** on c1 (40) and the c4b lava base (m5, 40), about 1.5x
  slower in c4 (31) and the training ground (31), slightly faster in c3
  (16..18), and **ten times faster in the c4b snow missions m4 (2 degrees)**
  and twice as fast in m6 (10). `src/mech3/mech.cpp` still uses 0.05; it
  should read `mech3::environment().inv_temperature` (0x4193ea `fmul
  [0x5e5d40]`).
- **Jump-jet ceilings indoors.** 150 by default; c2 m2 sets -90 and c4b m5
  -3: the underground missions. The jump code (0x4774a9) caps a jump value at
  `(ceiling - height) / 4` **[unverified: which value, thrust or climb]**, so
  jets fade out within 4 m of the ceiling.
- **No sun in software.** The `sunlight` node is switched on only on the
  Direct3D renderer, and also off when the HWCardFlag option is 0.
- **The sky is swapped per mission**: `fog node` picks `hormain` or
  `horblack` among the horizon's children and hides the others. c2 and c4b
  have both; c2 m2, c2 m4 and c4b m5 (the underground and lava missions)
  show `horblack`, so the normal sky, with its cloud dome `horscroll` and
  sun polygon under `hormain`, is hidden there. Worlds without `horblack`
  have only `hormain`, which stays on.
- Rain and snow: per mission (table), with fall speed (`gravity`), sideways
  drift (`wind vel`), and a fade from top to bottom (`alpha_gradient`).
  Unknown `type` strings are ignored, and the comparison is case-sensitive.
- `underwater` default colour (50, 50, 80) is on a 0..255 scale while every
  file uses 0..1; unused since every block has one.
- A missing `world.zrd` or block leaves the world file's dummy fog (black,
  ranges 0, render.md) copied into the mission, so the fog stays black.

Candidates for `docs/discoveries.md` (not added): per-mission temperature
changes cooling (snow x10 faster, c1 and the lava base x2 slower); jump-jet
ceilings in the two underground missions; the per-mission rain/snow; no sun
lighting in the software renderer; hardware fog starting at 32 m (render.md).

## Port and check

`world_init` 0x491d70, `world_clear` 0x491d80, `world_set_enabled` 0x492660,
`world_read_params` 0x491de0. Unported callees go through `WorldHooks`; the
ported list find, node SetActive, world fog setters, colour packing and
zReader are called directly. The world fog getters (0x4f24a0..0x4f24e0,
cls_world.c) are file-local in mission_world.cpp until cls_world.cpp takes
them.

The test runs every block on hardware (sun option on) and in software (off),
the multiplayer blocks also as multiplayer games, both mpinfo files, and 20
crafted blocks (missing keys, empty blocks, temperature 0/-5/NaN/string,
unknown and lower-case weather types, partial lists) in seven scenes (no sun
node, no world, no horizon, out of memory for the weather, option off) and
read twice; 269 runs, traces identical bit for bit. Breaking the port
(hwrange in software, NaN temperature kept, alpha gradient swapped, altitude
getter swapped, other skies left on, sun lit in software, any type makes
rain) fails the test each time; using the exact pi/180 instead of the exe's
constant does not change any shipped or crafted value.

Open: the multiplayer game's +0x94 (vtable +0x4c, "mission number" > 2);
who reads the underwater fields; what HWCardFlag means; 0x5efba4.
