# The mission at run time: the frame and the cleanup

Mech3.exe 1.2, `C:\zipper\Mech3\mission.cpp`, ported as `mech3/mission`
(`Missions::begin`, `update`, `complete`, `lose`, `end_screen`,
`on_command`, `cleanup`) with the world's part in `mech3/mission_world`.
Checked against the exe by `tests/differential/test_mission_frame.py`
(section 6). The load before it: mission-load.md.

## 1. The missions object at run time (0x006098d0)

| Off | What |
|---|---|
| +0x00..+0x4b | world.zrd's parameters (`MissionWorld`, world-params.md): +0x00 the weather, +0x04 the sun, +0x08 the cloud pattern on |
| +0x4c | **the game time**, seconds: the global [0x0060991c] that every timer in the game reads (the valves' delays, weapons' readiness, the AI's fire timers, hits) is this field |
| +0x50 / +0x54 | the mission clock: counts up, or down while +0x54 |
| +0x55 | the mission has ended (won or lost): the win's checks stop |
| +0x56 / +0x58 | lost: the end screen comes when +0x58 (8 s) runs out |
| +0x5c | the mission is up (the setup's last step; nothing clears it but the next setup) |
| +0x64 | the game's mission number (0x00490720, the Load state) |
| +0x68 / +0x6c | the loader thread's event and thread |
| +0x78..+0x88 | objectives.zrd's (0x0045c2d0): +0x7c the primary objectives, +0x80 the secondary, +0x84 the win's condition, +0x88 the loss's |
| +0x8c..+0x98 | com_success, com_failed, obj_primary, obj_secondary |
| +0x9c | the line an objective checked now announces itself with |
| +0xa0 / +0xa4 | the line asked for this frame and its priority |
| +0xa8 / +0xac | the line playing and its priority (-1 none) |
| +0xb0 / +0xb4 | multiplayer's stations and their owner |
| +0xc4 | **a cheat was on as the mission started** ([0x00609994], protection.md) |

## 2. The start, 0x0048f400

Called first by the setup 0x00490ab0: +0x5c, the game time, the abort flag
and the line playing to 0; the command handler 0x0048f440 bound to
MISSION_COMPLETE (0x71, Exit Mission, Ctrl+Q) and 0x72 (0x0051d2f0); then
0x00491d20: +0xc4 = invincibility (0x0040cf20) or unlimited ammo
(0x0040cf80) or heat off (0x0040cfc0), 0 in a network game ([0x00608e45]).

The game time is the one global [0x0060991c] (the missions' +0x4c), so in
the exe every mission's load, launch and first updates run from 0. The port
keeps copies in the mech and zWeapon modules; each write of +0x4c (here,
the frame's and the cleanup's) sets them at once (`game_time_set`),
2026-10-25 (before, they caught up only at the first frame, and a second
mission's load read the first's time).

The handler: Exit Mission works only once the mission has ended, won (+0x55)
or lost (+0x56), and then goes straight to the end screen (0x00491bf0,
through its thunk 0x00491c70 when lost); 0x72 is bound but does nothing.

## 3. One frame, 0x0048f470 (ECX missions, dt)

From the Core state's world tick (game-loop.md), after the effects' update:

```
time += dt; clock += dt (or -= dt)
0x004926d0  the world: with the cloud pattern on, the clouds drift (0x00543a50, the frame's dt
            [0x0072c52c]) the way the camera looks: its rotation (0x00522170, pitch and yaw to a
            direction), flattened and normalised (0x004020d0), as (z, -x)
0x004d3d30  the valves' delayed updates (ai.md section 13), at the game time
0x0045a270  the map object 0x00605ae0 (the multiplayer MFB deployment check)
stations    vt+0x6c(dt, the player's vehicle), multiplayer
+0xa0 = 0   no line asked yet
lost (+0x56): +0x58 -= dt; at 0 or below and not [0x00608e4a]: the end screen, then the line, done
else:
  +0x80 the secondary objectives: +0x9c = obj_secondary, checked (0x0045e540(1))
  not ended:
    +0x7c the primary ones: +0x9c = obj_primary, the done byte (+0x08) read, checked
    all done now: when it just changed and the cockpit's screen is up and 0x0045c180 says
      so, the screen's +0x17964 vt+0x60(-1); +0x9c = 0, the win's condition +0x84 vt+0x04;
      met (or no condition): the win 0x0048f740
  +0x88 the loss's condition: +0x9c = 0, vt+0x04; met: the loss 0x0048f9e0
0x0048f630  the line
```

An objective's check announces its own line through 0x0048f6c0 with no
sound, which takes +0x9c at priority 0 (likely: the objective list's
done and failed lines). **The line** (0x0048f630): the one playing is
dropped when zSound says it ended (0x0056e8e0); the line asked plays
(0x0056e3d0, volume 1) unless the one playing has a higher priority, which
it then stops (0x0056e9f0). 0x0048f6c0 keeps the higher of two asks in a
frame (the later of equals).

[0x00608e4a] (the app's +0x1c2) is never written: the end screen always
follows.

## 4. The win and the loss

**The win** 0x0048f740 (once: +0x55): the cockpit's screen (0x00430aa0 on
the mech's data, when the player's view is on the player's mech) and
0x0045c180's answer kept, then by game type:

- **Instant Action** (2) in its endless (the game's +0x44 1) or wave (2)
  mode: the wave count +0x6c (and +0x70) goes up, the best wave +0x68 is
  kept in wave mode; unless wave mode has passed wave 20, the objectives
  are freed (0x0045c1d0) and `objectives.zrd`'s `ia<n>` loaded again
  (0x0045c2d0) and started (0x0045c4a0), after 0x005392c0 and
  0x0049dc80(0): the next wave; the mission goes on (+0x55 and +0x56
  back to 0) and the screen resets (0x00446ad0). The win's line
  is not asked in these modes.
- **Training** (4): 0x004794a0 with the objectives, then 0x004373b0: when
  it gives a next lesson the mission goes on, without the line.
- Otherwise the mission has ended.

Then com_success at priority 2 (but in Instant Action's two modes), and
the screen's vt+0x60(-1) when 0x0045c180 said so.

A win does not reach the end screen: in mission.cpp only the loss's
countdown (0x0048f50c) and Exit Mission's handler (0x0048f453,
0x0048f466) do, so after a win the mission runs until the player leaves.

**The loss** 0x0048f9e0 (also the player's mech destroyed, 0x0041b359 and
0x0041d121, and the valves' `missionabort`): in multiplayer (0x00479480)
the player's mech is put down through its action 0x0041d510(1) vt+0x3c
unless its +0x578 is already 10, and the game goes on. Otherwise, once:
ended and lost, +0x58 = 8 s, com_failed plays at once (not through the
line's priorities), the screen's vt+0x60(-1).

**The end screen** 0x00491bf0: by game type, campaign (0, 1)
0x0040f0b0(0) then the fade sequence 0x0048ef00 on 0x006099a0 (1, 2,
2.0 s), Instant Action the same without 0x0040f0b0, training the sequence
0x006098c8; then 0x0040f0b0(0). Multiplayer gets no fade here.

## 5. The cleanup, 0x00491780(unload)

Nothing when no mission is up (+0x5c). In order: 0x004eb040 kept (and
0x004eb030(0) when unloading), the loader's event and thread closed (the
game thread back to normal priority), 0x004e7bc0, +0xbc through
0x004582c0, the animations 0x0050ace0(1), 0x00570410(1), zSound's
snapshot stopped and freed (0x0056eb10, 0x0056f0a0, 0x0056f190), zSound's
reset 0x005716a0, the view (0x0040ae70, 0x0040adf0), the player's vehicle
taken from the pilot (0x004782c0(0)) and deleted, the stealth entity
taken out of zEntity's list (0x005a89c0) while 0x004e6910, 0x004b4ae0,
the landmines 0x00413770, 0x0049dc40, 0x004a7970, every target (0x004d8270, ai.md section 14) and
0x00584fe0 clean up, then put back (0x005a8820), 0x0047a720 on
[0x0060b544], the stations' owner deleted, 0x004d54f0, 0x00479850,
0x004ea750, "Deleting AI Nets", znet's registry 0x0058b290, 0x004c5fa0,
0x004c3240, **the valves end** (0x004d3fa0, ai.md section 13), the
targets' HUD images (0x004d3030, ai.md section 14), 0x004d71f0,
0x00427300, zClass's purge 0x004f7bd0, the objectives freed, the world's
parameters (0x00491d80: the weather out of the particle manager and
deleted, the sun, the cloud pattern off), 0x00414000, the mission's voice
set unloaded (`<dir><block>`, "c1m1", 0x0056f530), 0x00431800,
0x0050b2f0; unloading: 0x00490480 and "Done:Unloading db", else
0x005394c0(1, 1); 0x005573d0(0), 0x00576270(0), 0x004eb030 given back
what was kept, the purge again, and the game time 0. The two calls to
0x005ac5c0 are an empty function.

**In the program** (audited 2026-10-22): the calls with ports are wired
(`WiredMissionHooks`): 0x004e7bc0 (the listeners' slot +0x10), the
animations, zSound's, the player's vehicle, the targets, the AI switch
off, znet, the valves, the targets' images, zClass's purge, the
objectives, 0x00414000, the voice set, 0x00431800, and since this audit
0x004eb040 / 0x004eb030 (zClass's delete mode), the view's 0x0040ae70(0)
and 0x0040adf0(0, 0) (it lets go of the player's mech before it is
deleted), the radar objects 0x004b4ae0, the regions 0x004c5fa0 (freed,
else a later mission's region lookups by name found the earlier
mission's: a port-only gap; `mission_after_another_gets_its_own_regions_headless`),
the player controls' pilot record 0x00427300, the landmines 0x00413770
(`landmines.md`) and zReader's
0x005573d0(0) (the mission's archives closed). Not ported, logged:
CloseHandle / SetThreadPriority (no loader thread in the port), the map
objects 0x004582c0 (+0xbc's list 0x00605acc), the stealth entity's
0x005a89c0 / 0x005a8820, 0x004a7970 ([0x00609a38]
..[0x00609a44]), 0x0047a720 and 0x004d54f0 ([0x0060b544]), the waynets
0x00479850, 0x004d71f0 (0x004add10), zEffect's 0x0050b2f0 (the
mission-made animation instances, +0x94 bit 17), 0x005394c0(1, 1) and
0x00576270; 0x004c3240 is empty.

Callers (not ported): the briefing (0x0040501f, 0x004050b6), the debrief
(0x0047cfda..0x0047d286), 0x00470717, 0x00491b99, 0x00492e6a, and the
load's failure (0x0048fe17, mission-load.md).

## 6. The port and its test

- `Missions` holds the fields; every call out of mission.cpp is a
  `MissionHooks` method named by address; the world's (weather, clouds,
  camera) are `WorldHooks`.
- **In the program** (`app_mission.cpp`): the Core state's world tick calls
  `GameEngine::missions_update`, the port's frame. The valves' tick copies
  the game time to the ports' copies of [0x0060991c] (`World::time`,
  `WeaponWorld::time`) and ticks the timers (`ai::valves_tick`); zSound
  plays, checks and stops the lines; the clouds drift by the camera
  `camera1`; the valves' missionabort is the port's loss. The Load state's
  0x00490720 keeps only the mission number. The cleanup is the port's
  (the load's failure calls it too) with the valves' end and zSound's
  reset and voice sets; the rest is logged. Nothing in the program calls
  the cleanup yet (the briefing and debrief are not ported).
- `mw3tests` `mission_c1m1_frames_fire_bramps_timers_headless`: c1 mission
  1 through the Load and Core states; bramp set on the first Core frame
  wakes the Owens, its two delayed updates set `truck_go` after 18 s and
  `c1m1dialogue4_play` after 22 s, each on the first frame its time is
  past; each frame's game time is the last plus the frame's step; the
  cleanup ends the valves and puts the time back to 0.
- `test_mission_frame.py` (probe `tests/probe_mission.cpp`): 400 cases,
  each a random sequence of the start, frames, the win, the loss, Exit
  Mission and other commands, asks for a line and the cleanup, on a random
  missions object (every game type, Instant Action's modes and waves round
  20, training's lessons, objectives present or not and answering either
  way, the loss's delay running out, lines of every priority, multiplayer
  with the mech down or not, the cockpit's screen up or not, the clouds on
  and off, the cleanup with and without the unload, the mission up or not,
  every handle set or not). The x87 at 24 bits. Compared: the call traces
  (the clouds' direction within one unit in the last place) with every
  field of the missions object after each step, and Instant Action's
  counters: 12,245 calls and states identical. Breaking the port showed the
  test catches it, 22 of 22: the clock counting up while it counts down,
  the loss's delay ending one moment late, the secondary objectives with
  the primary line, the done byte read after the check, the win without its
  condition, the loss checked once won, a line of equal priority not
  replacing, a lower ask replacing a higher, Instant Action stopping at
  wave 19, the best wave kept in every mode, training's line asked when it
  goes on, a 5 s loss delay, multiplayer putting a mech already down, Exit
  Mission before the end, the campaign's end screen without 0x0040f0b0,
  the cleanup of a mission not up, the stealth entity not put back, the
  valves ended after the targets, the voice set by the game's number, the
  game time kept by the cleanup, the cheats ignoring the heat, the clouds
  drifting the other way. `mw3tests`' case catches the frame without the
  valves' tick, the game time not advanced and not copied (3 of 3).
  `test_mission_load.py` now runs the ported start inside the setup.

## Open

- What the objective lists' checks (0x0045e540) and 0x0045c180 do
  (objectives.zrd is not traced), the map object 0x0045a270 beyond its
  multiplayer deployment check, the stations, the fade sequence 0x0048ef00.
- Nothing ported calls the cleanup in play yet; the next mission's load
  would.
