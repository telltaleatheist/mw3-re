# Heat

Mech3.exe 1.2 @ 0x004192f0, called each frame from the mech update (0x419fc0,
vtable 0x5bcef8 +0x34) with the frame step. Ported to `src/mech3/mech.cpp`
(`mech3::update_heat`), checked against the original in Unicorn
(`tests/differential/test_heat.py`: 300 random scenarios x 400 frames, every
shutdown state and every outgoing call, floats within 1e-4).

Traced from the raw disassembly; Ghidra's pseudocode of this function drops the
ECX of most calls (the sound object, the sound index) and shows the negative
timer constants as `time - -2.0`.

## Fields

Mech offsets (`docs/structs.md` has the rest of the object):

| Offset | Field |
|---|---|
| +0x068 | height, from the model's bounding box (12 if none) |
| +0x0d4 | weapons and ammo bins |
| +0x0dc / +0x0dd / +0x0de | power-down pending / heat shutdown in progress / powered down |
| +0x37c | controls (`controls.md`); vtable +0x1c is GetThrottle, -1..1 |
| +0x38c, +0x390 | left and right leg; leg +0x78 is the foot |
| +0x554 | heat, 0 to 2 |
| +0x558 | heat added this frame (weapons add to it); cleared at the end |
| +0x55c | dissipation rate this frame |
| +0x574 | MASC on (time; non-zero while on) |
| +0x580 | jump jets firing |
| +0x5cc, +0x5d0, +0x608, +0x60c | sounds: warning beep, shutdown alarm, "shutdown imminent", "shutting down" |
| +0x630 | invincible: the debug menu's Invincible item or an option bit (`damage.md`); at critical heat it shows "Heat Death disabled" |
| +0x658 | heat sinks: +4 count (10 by default), +8 doubles |
| +0x684 | the pilot/HUD side: sounds and the screen effect go through it |
| +0x69c | state: bits 0-2 beep, bits 3-5 shutdown, bit 24 shutdown override, bit 25 powered down |
| +0x6a0, +0x6a4, +0x6a8 | shutdown, beep and voice timers (game time, seconds) |

Foot: +0xac in water, +0xdc water depth, +0xe8 soil under it. Soils (0x47f380):
1 water, 2 grass, 3 dirt, 4 mud, 5 concrete, 6 snow, 7 mech, 8 lava, 9 silt,
10 noslip.

## Each frame

```
rate = 1                      x3 while powered down
if left foot in water:        f = max(depth / height, 1); rate *= min((3f + 1)^2, 8)
dissipation = rate * [0x5e5d40]   (1 / the mission's ambient_temperature, 0.05 at the default 20; mission_world)
full model only:              dissipation *= (1.6 - |throttle|) when that is below 1
                              added += dt * 0.05 while jump jets fire, and again while MASC is on
added += 0.05                 while either foot is on lava (per frame)
dissipation *= sinks * 0.1 * (doubles ? 2.5 : 1.25)
if heat is on:                heat = clamp(heat - dt * dissipation, 0, 2), then clamp(heat + added, 0, 2)
else:                         heat = 0
```

"Heat is on" is bit 3 of the options word at `[0x5f4d04]` or game type 3,
multiplayer (the pilot `[0x60861c]`'s current game at +0x6c, vtable +0x2c;
`gametypes.md`). With ten single heat sinks the sink factor is
1.25, so a mech standing still sheds 0.0625 heat per second.

Quirk, kept: `f` is the larger of the ratio and 1, so `(3f + 1)^2` is at least 16
and the cap always applies: wading is a flat x8 whatever the depth. With `min`
the factor would have run from 1 (dry) up to the cap, which may have been the
intent.

## Who runs the full model

The byte at 0x5e1fd0 starts at 1 and nothing writes it, so every mech, AI
included, runs the full model with the two state machines. Being the player only
adds the beeps, voices and the heat haze. The other branch, "at 2.0, destroy the
torso", is never taken in the shipped game.

The vtable +0x5c check at the start ("if set, clear heat and return") is the
mech's 0x40bae0, which returns 0; the port leaves it out.

## The warning beep (bits 0-2)

0 idle: above 0.7 and past the beep timer, play sound 5 and go to 1 at once.
1: timer = now + 2, go to 2. 2: wait for the timer, go to 3. 3: below 0.7, stop
the beep, timer = now + 2, back to 0. Any of bits 25-30 force state 4+, which
cancels the beep.

## Shutdown (bits 3-5)

Checked first each pass: at 2.0 (critical) stop the sounds, play mech sound 26,
force state 4 and clear the shutdown timer. Otherwise, at 1.5 or more while not
powered down, stop the sounds and force state 2.

| State | What happens |
|---|---|
| 0 | above 0.95 and past the voice timer: "shutdown imminent" (sound 0x14), timers = now + 4, go to 1 |
| 1 | 0.9 or below: cancel, clear the override, back to 0; else once the timer passes, timer = now + 3, go to 2 and run again |
| 2 | "shutting down" (sound 0x15), power down (0x41cc30(1)), heat shutdown on, go to 3 (with the override and no 1.5 forcing, it moves to 3 without powering down) |
| 3 | 0.9 or below: cancel; else when the timer passes, timer = now + 0.1..3.0 at random, start the alarm (sound 6), go to 4 |
| 4 | when the timer passes: unless critical, an 80% roll rides it out: maybe an ammo bin cooks off (0x4b0de0, mech sound 15), timer = now + 3, back to 3. Otherwise, or at critical: the torso is destroyed (critical, not multiplayer) or hit (vtable +0x20 (0,1)), heat = 0, back to 3. When the mech is invincible: message 0x20c "Heat Death disabled" for 2 s instead, voice timer = now + 8, back to 0 |

States 3 and 4 as above only run while the mech is still powered up: with the
override on, or at critical (which clears bit 25). While powered down (bit 25)
states 1 and 4 just reset to 0, state 2 resets and
re-runs state 0, and state 3 waits for heat at or below 0.4 and the timer, then
powers back up (0x41cc30(0)).

The random draws are MSVCRT `rand()` (`crt::msvc_rand`), scaled by 1/32767.

## Frame rate

The lava heat is added per frame, not per second: 0.05 a frame is 1.5/s at 30
fps but 3/s at 60. Everything else scales with dt. The original presents with
`Flip(NULL, DDFLIP_WAIT)` (0x57a8b5) and no no-vsync flag, so in fullscreen it
was capped at the monitor's refresh, 60-85 Hz on the CRTs of the day, and often
ran slower. On a 144 Hz display the same code would cook a mech on lava 2-5
times faster than in 1999. FIX (B), decided with Owen 2026-10-20: the port
adds 0.05 per 1/30 s (`dt * 1.5`), so lava heats 1.5 a second at any frame
rate, as at 30 fps in the original. 1999 machines rarely held more than 30 in
battle. `zfix::options().per_second` switches back to per frame (the probes do,
so the differential test checks the exe's path; `tests/test_fixes.cpp` checks
the fix at 30 and 144 fps).

## Powering down and up

Ported (`Mech::power`, `Mech::finish_power_down`), checked by
`tests/differential/test_power.py` over every combination of the flags read.

`Mech::Power` (0x41cc30): ignored while a heat shutdown is in progress (+0xdd).
Power-down only sets "pending" (+0xdc) and stops the controls (+0x37c vtable
+0x4c), and not at all while +0x584 is set (likely airborne). Power-up, if
powered down, restarts the controls, restores the HUD (0x43bb50 with the byte
saved at +0x587), plays sound 7 when +0x688 bit 0 is set, and clears both flags.

0x41cac0 finishes a pending power-down once the mech's animation state (+0x578)
is 1, 12 or 13 and +0x584 is clear: powered down, the vector at +0x3c zeroed,
the HUD state saved (0x43bb20), the three looping sounds at +0x5b8 stopped and
sound 8 played. Animation state 10 cancels the request instead.

## The shutdown override (bit 24)

Command 0x40 in the mech's command handler (0x41be80) calls 0x419190(1): for
the player only, and only while the beep is active (state bits 0-1 non-zero), it
plays sound 0x16 and sets bit 24. Mech3Msg.dll calls it "Override Thermal
Shutdown" (0x87b): "Disables the current automatic shutdown until the 'Mech
cools then reaches a critical level of heat" (0x97b). In the state machine the
bit makes state 2 skip the power-down; 1.5 heat still forces it, and the bit
clears below 0.4. 0x419190(0) clears it.

## Losing heat sinks

`HeatSinks::Lose(n)` (0x45fbc0) takes n off the count (not below 0) and always
returns false. The loadout sets the starting count (0x414735 copies it from
the configuration); critical hit type 0 on a part removes one (0x460d50), and a
destroyed part removes its own (0x4617f0). Because Lose returns false, the
crit handler's "heat sink lost" voice (0x485390) never plays.

## Open

- Game types are mapped in `gametypes.md`: 0 Mech Lab/dev levels, 1 campaign,
  2 instant action, 3 multiplayer, 4 training.
