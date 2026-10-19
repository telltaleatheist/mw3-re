# Startup and shutdown

Mech3.exe 1.2. `InitInstance` (0x480000) brings the subsystems up; 0x4808a0
takes them down. Arguments are from the disassembly (most go in ECX/EDX, which
Ghidra's pseudocode drops). Module names come from the source file around each
function (`func2file.json`); a name marked "likely" is inferred from position
and what the function touches.

## InitInstance (0x480000)

1. **Tamper check.** 0x4854c0 runs a CRC-32 (MSB first, polynomial
   0x04c11db7, start 0; 0x576c10, table built at 0x576bc0) over two tables
   built into the exe: the mech roster at 0x5e6078 (0x474 bytes: name, message
   id, tonnage, class for each mech) and the BattleTech internal-structure
   table at 0x5e6998 (0x17c bytes: per tonnage, the structure points of head,
   centre torso, side torso, arm, leg). The result must equal 0x00efd77d
   (0x5e6b14), which it does for the shipped exe. If not, the flag at 0x608f70
   is set and the state after the intro (app +0x1dc) becomes the Exit state
   (state 4) instead of Main: the splash movies play, then the game closes.
2. **Network lobby.** 0x5899b0 asks a DirectPlay lobby for a connection. Without one,
   0x480340 looks for a running copy (window class `Mech3Class`) and brings it
   to the front instead of starting a second one. With one, 0x4803a0 closes the
   other copy, 0x47fed0 picks the `DefaultPlayer` profile, 0x575e30 requires
   Winsock 2.0 (message 0xf60, then exit) and 0x589a50 connects.
3. **The window class** `Mech3Class`, with icon 0x94.
4. **Settings.** 0x598210("MicroProse", "MechWarrior 3", "1.0"): the registry
   root `HKLM\SOFTWARE\MicroProse\MechWarrior 3\1.0`, read by 0x597fc0. The
   title is message 7 of `Mech3Msg.dll`, fetched by 0x576040 (16 rotating
   0x200-byte buffers); the version string is at 0x5e489c.
5. **Input.** 0x51c630(0xa7, "DefaultCmdMap"): zInput with 0xa7 commands.
6. **A 0x200-entry pool** (0x555cd0, next to zReader; likely reader nodes or
   strings).
7. **Data.** `zrdr::init` (0x556170) with the loose-file path
   `..\data\common\zrdr`, then 0x40c420 loads `detail.zrd`. If that fails it
   opens `zbd\reader.zbd` as the current archive and tries again; failing twice
   shows message 0x14d, "Cannot read required data. Please ensure proper
   installation.", and exits.
8. **App defaults** (0x47fb80): the default mech is `bushwacker`.
9. **zEntity** (0x5a8b30), then small module switches: 0x5090d0(1) zEffect
   animation, 0x50d1e0(0) and 0x50d1f0(0) zEffect, 0x583740(0) zWeapon,
   0x501f90(0) zDecal client, 0x4f8f60(0) zClass.
10. **The main window** (MFC `Enable3dControls`, then app vtable +0xbc creates
   it), and the frame's +0xc0 points back at the app.
11. **Saved game state.** When the app flag +0x1c7 is set, 0x47fe10 restores
    from the registry (0x597d80) or falls back to defaults (0x40d4c0).
12. **zGameApp start** (0x58fc70): app vtable +0xac with the window; on failure
    +0xb0 and +0x70 clean up. Then `UpdateWindow` and 0x40cb30, which applies
    the options (about 15 setters at 0x40ce10-0x40d3b0).

## Game states

The app constructor (0x47f7a0) sets the first state, app +0xc4, to Intro, and
the state after it, app +0x1dc, to Main. The Intro state's update (0x482170)
plays the `SPLASH` movies from `fmv.zrd`, then moves on to 0x482970(12), which
returns +0x1dc. The states are static objects picked by 0x482970(n); their
constructors name them:

| n | Object | Name |
|---|---|---|
| 0 | 0x608e90 | Attract State |
| 1 | 0x608f58 | Intro State |
| 2 | 0x608f00 | Main State (after the intro) |
| 3 | 0x608f48 | Pause State |
| 4 | 0x608ef8 | Exit State (after the intro when the tamper check fails) |
| 5 | 0x608ec0 | Core State |
| 6 | 0x608f18 | Load State |
| 7 | 0x608f38 | Pre Lab State |
| 8 | 0x608ee8 | Salvage State |
| 9 | 0x608e70 | Win State |
| 10 | 0x608ea0 | (unnamed) |
| 12 | app +0x1dc | the state after the intro |

## Shutdown (0x4808a0)

The mission object (`[0x60861c]`, 0x4785f0) goes first, then the game-side
modules (0x414000 mechs, 0x4844b0, 0x4584e0, the object at `[0x608620]`), then
GameZ in roughly the reverse of startup: weapons and network (0x586c20), decal
client (0x502020), geometry (0x516f00), weapon module (0x583870), effects
(0x50d2b0), zClass (0x4f8f40), input (0x526780), sound (0x5715d0, 0x570510),
the zReader text side (0x557b10), archives (0x576c60), input devices
(0x51b170), zReader (0x556210, which closes every archive), video (0x579ef0)
and zAiTarget (0x4d72a0).

Some steps are skipped when `[0x608d5c]` is set; likely an "exiting after a
fatal error" flag, so that teardown does not touch half-built modules.
