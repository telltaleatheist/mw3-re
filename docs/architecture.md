# How Mech3.exe is put together

Mech3.exe 1.2, as recovered with REA and Ghidra (2026-10-18). Addresses are
virtual addresses in the exe. The original file of each function is in
`docs/module-map.md`.

## Two programs in one

- **GameZ** (`C:\zipper\GameZMech3\`, 0x4eaea0-0x5aa3b0): Zipper Interactive's
  engine, as about 20 libraries: zClass (scene graph: worlds, cameras, lights,
  objects), zRender (software rasterizer), zVideo (DirectDraw/Direct3D), zSound
  (DirectSound, CD audio), zInput, zNetwork (DirectPlay), zModel, zGeometry,
  zImage, zEffect, zFMV, zTwoD, zInterp, zReader (the `.zbd` data reader, see `zreader.md`),
  zEntity, zSIScript, zWeapon, zDEClient, zUtil, zError, zGame.
- **Mech3** (`C:\zipper\Mech3\`, 0x401000-0x4eada0): the game: mechs, weapons,
  missions, the player, AI nets, MechLab, salvage, briefings.
- After 0x5aa450: the C runtime and other libraries.

MFC (MFC42.DLL) is used for the application object and for dialogs
(`CDialog`, data exchange, tree and tab controls), which look like built-in
editors such as `ZAiNetEdit.cpp`, not the game's own UI.

## Start, run, stop

```
entry 0x5ac3a0  ->  AfxWinMain (MFC)
  app object: Mech3's app (constructor 0x47f7a0, vtable 0x5c3f80)
              derived from GameZ zGameApp (constructor 0x58fcd0, vtable 0x5cf378)
              derived from MFC CWinApp
  constructor 0x47f7a0   loads Mech3Msg.dll strings; refuses DirectX < 6
  InitInstance 0x480000  network lobby check, player profile, window class
                         "Mech3Class", then each subsystem's init (calls.py 0x480000)
  Run          0x58fd70  the main loop (below)
  shutdown     0x4808a0  each subsystem's shutdown, in order
```

`InitInstance` is reached only through the vtable, so Ghidra's auto-analysis
missed it, like about 1,000 other functions; `tools/ghidra/CreateAndDecompile.java`
creates them.

## The main loop and the state stack

`zGameApp::Run` (0x58fd70) pumps Windows messages, then each pass:

1. advances the frame clock (0x575a00),
2. polls DirectPlay (0x588930),
3. then **either** runs one queued state command **or** updates the current
   state, never both in one pass. An update that returns true quits.

Game states (front end, MechLab, mission, ...) sit on a stack of 16. Commands
Pop (1), Push (2) and Replace (3) are queued and run one per pass; a state's
vtable has enter (+0x0c), update (+0x10), leave (+0x18), suspend (+0x1c),
resume (+0x20) and a message hook (+0x24, which also receives MCI
notifications). Ported in `src/gamez/zgame/zgameapp.cpp`.

## The frame clock

0x575a00: seconds from `GetTickCount()` as a float; the step is clamped to
0.125 s (8 frames a second) unless the clamp is turned off, and a one-frame
scale can slow or speed a step. Ported in `src/gamez/zutil/ztimer.cpp`.

## Music

CD audio through MCI, `zSound\zsnd_cd.cpp` (0x572450-0x5731f0). The game asks
for named cues; the `CD_TRACKS` table in `zbd/reader.zbd` maps them to tracks:
`menu` -> 2, `c1`..`c4` -> 3, `dev1`, `dev2` -> 2. Menus play their track once
(mode 2); missions loop theirs (mode 6), replayed on MCI's "finished"
notification. Ported in `src/gamez/zsound/zsnd_cd.cpp` and `src/mech3/music.cpp`.

## Cutscenes

`zFMV`: Indeo 5 AVIs, opened through MCI's `MPEGVideo` device (0x59b080).

## The software renderer

zRender's span routines draw with the stack-pointer trick: docs/blit-trick.md.

## Where things stand (2026-10-19)

| Area | Doc | Code | Checked against the exe |
|---|---|---|---|
| Main loop, state stack | this file | `gamez/zgame/zgameapp` | unit tests |
| Frame clock | this file, `fixes.md` | `gamez/zutil/ztimer` | `test_ztimer.py` |
| CD music | this file | `gamez/zsound/zsnd_cd`, `mech3/music` | unit tests |
| Span fill (blit trick) | `blit-trick.md` | `gamez/zrender/zrndr_span` | `test_spans.py` |
| Archives, reader data | `zreader.md` | `gamez/zutil/zutl_zar`, `gamez/zreader` | `test_zreader.py` |
| Textures | `textures.md` | `gamez/zimage/zimg_library` | `test_textures.py` |
| Animations | `motion.md` | `gamez/zsiscript/si_script` | `test_motion.py` |
| Heat, power | `heat.md` | `mech3/mech` | `test_heat.py`, `test_power.py` |
| Structures | `structs.md`, `types.json` | Ghidra (`ApplyTypes.java`) | |
| Startup, shutdown, game states | `startup.md` | | |
| Game types | `gametypes.md` | | |
| Mech controls | `controls.md` | | |
| Network | `network.md`, `network-messages.md` | | |
| Asset files | `assets.md` | | |
| Player-facing findings | `discoveries.md` | | |
