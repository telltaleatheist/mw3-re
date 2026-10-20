# Startup and shutdown

Mech3.exe 1.2. `InitInstance` (0x480000) brings the subsystems up; 0x4808a0
takes them down. Ported in `mech3/app` (the main loop and the game states that
follow: game-loop.md; the options: options.md), checked against the original
by `tests/differential/test_app_states.py`. Arguments are from the disassembly (most go in ECX/EDX, which
Ghidra's pseudocode drops). Module names come from the source file around each
function (`func2file.json`); a name marked "likely" is inferred from position
and what the function touches.

## InitInstance (0x480000)

0. **The constructor** (0x47f7a0, before WinMain): zGameApp's fields, the
   first state (Intro) and the one after it (Main), RunDir (0x4828d0:
   `SetCurrentDirectory` from `HKLM\Software\Zipper\MechWarrior3\<ver>\RunDir`),
   Mech3Msg.dll (0x575f30; missing: the system's error text and exit), the
   DirectX version (0x57a3d0; below 6.0: message 0xf3f and exit), the input
   mode 5 (+0x190), the interp script object `[0x6040b0]` (0x410700,
   "DamageScale" from interp.zbd).
1. **Tamper check.** 0x4854c0 runs a CRC-32 (MSB first, polynomial
   0x04c11db7, start 0; 0x576c10, table built at 0x576bc0) over two tables
   built into the exe: the mech roster at 0x5e6078 (0x474 bytes: name, message
   id, tonnage, class for each mech) and the BattleTech internal-structure
   table at 0x5e6998 (0x17c bytes: per tonnage, the structure points of the
   centre torso, side torso, arm and leg; the head is a fixed 3, `damage.md`). The result must equal 0x00efd77d
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
6. **The list-link pool** (0x555cd0(0x200)): 512 free links (12 bytes,
   `{data, next, prev}`) for the engine's generic ring lists (list header
   `{count, .., .., .., head +0x10}`; 0x555d20 frees a link into the pool,
   0x555de0 takes one, growing by one when empty and counting the misses at
   `[0x72be8c]`, 0x555f30 appends, 0x555f70 removes, 0x556000 finds, 0x556030
   indexes, 0x556070 pops). zSound's groups (0x56e7d0, 0x573330..) and the
   archive list (0x5761ab) use them; 0x555d90 frees the pool at exit. Not
   ported: the rebuild's containers stand in.
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
    the options (options.md).

The main window (0x47ff90 -> 0x482d10) parses the command line first (below),
sets the input mode (0x51bae0), clamps its rectangle to the work area,
creates itself, runs the CD check (removed), attaches the debug menu
(gametypes.md), opens `mech3.err` (0x50da50) and titles itself "MechWarrior 3
<version>".

**Saved video setup** (0x47fe10, step 11, when full screen): with a 3D card
installed (InstallOptions & 0xff), the device saved (HWCardDev; 999 asks
0x4eaab0 to pick) and at least VMode 5; else software (HWCardDev -1).

**Engine start** (+0xac, 0x480570): removes the debug menu, then geometry
("zbd"), effects, classes, models, render, sound, utilities and weapons, each
printing `zXxxInit: PASSED/FAILED` to stdout (only "PASSED" when 0 comes
back, sound when 1), textures and sound modes, the video (0x480970: the 3D
card or InstallOptions 0xff00 for software, else message 0xf41; the screen
(0x579c60), the software spans (0x564370), the buffers), the graphics flags
into models and render, lights, fonts.zrd, sounds.zrd, input, the joystick
(none: the axis table made ready, UseJoystick off; with one that has force
feedback, 0x0051ddb0, an object 0x433090, likely the force effects: input.md), mechs, maps.zrd, the clock and CD, and a 3D card with under 4 MB
fails with message 0xfbf.

## Game states

The app constructor (0x47f7a0) sets the first state, app +0xc4, to Intro, and
the state after it, app +0x1dc, to Main. The Intro state's update (0x482170)
plays INTRO and the `SPLASH` movies from `fmv.zrd`, then moves on to
0x482970(12), which returns +0x1dc. The states are static objects picked by
0x482970(n); their constructors name them (what each does: game-loop.md):

| n | Object | Name |
|---|---|---|
| 0 | 0x608e90 | Attract State |
| 1 | 0x608f58 | Intro State |
| 2 | 0x608f00 | Main State (after the intro) |
| 3 | 0x608f48 | Pause State |
| 4 | 0x608ef8 | Exit State (Escape on the main menu; after the intro when the tamper check fails) |
| 5 | 0x608ec0 | Core State |
| 6 | 0x608f18 | Load State |
| 7 | 0x608f38 | Pre Lab State |
| 8 | 0x608ee8 | Salvage State |
| 9 | 0x608e70 | Win State |
| 10 | 0x608ea0 | (unnamed: the input commands and their keys) |
| 12 | app +0x1dc | the state after the intro |

## Shutdown (0x4808a0)

The pilots (when `[0x60861c]` is set it is cleared and 0x4785f0 runs, on the
app; `gametypes.md`) go first, then the game-side
modules (0x414000 mechs, 0x4844b0, 0x4584e0, the object at `[0x608620]`), then
GameZ in roughly the reverse of startup: weapons and network (0x586c20), decal
client (0x502020), geometry (0x516f00), weapon module (0x583870), effects
(0x50d2b0), zClass (0x4f8f40), input (0x526780), sound (0x5715d0, 0x570510),
the zReader text side (0x557b10), archives (0x576c60), input devices
(0x51b170), zReader (0x556210, which closes every archive), video (0x579ef0)
and zAiTarget (0x4d72a0).

Some steps are skipped when `[0x608d5c]` is set; likely an "exiting after a
fatal error" flag, so that teardown does not touch half-built modules.

The Exit state runs this on entry (0x482060) and then quits from its update.
ExitInstance (0x47fa10), once the window class was registered, unregisters
it, frees +0x1e4 and the script object, stops the CD and shuts down AI
targets, sound, network, input devices, the list pool and zEntity again.

## Command line (in 0x482d10)

`GetCommandLineA`, split with `strtok(" ")`, each token prefix-matched with
`strncmp` in this order (the program name included):

- `-nofmv` (6): movies off (app +0x1c1). The Intro still plays the SPLASH
  logos; only INTRO, ATTRACT and the Campaign State's movie when a campaign
  starts at mission 1 (0x48b056) check it.
- `-seasick` (8): FastMenu on (options.md).
- `-cd` (3): the mission music track, `atol` of the rest (0 when bare or not a
  number; `-cd12abc` is 12).

So `-nofmvx` counts as `-nofmv`, and `-c9` is nothing.
