# The main loop and the game states

Mech3.exe 1.2. How the app runs once `InitInstance` (startup.md) returns: the
engine's `zGameApp::Run` (0x0058fd70) and the stack of game states it drives.
Ported in `gamez/zgame/zgameapp`, `mech3/app` and `mech3/app_states`; the
options the states apply are in options.md.

## The app object

One static object at 0x00608c88 (constructor 0x0047f7a0), an MFC `CWinApp`
with the engine's `zGameApp` layer (0x0058fcd0, vtable 0x005cf378) and the
game's overrides (vtable 0x005c3f80):

| Slot | zGameApp | MW3 | What |
|---|---|---|---|
| +0x58 | 0x0058fa40 | 0x00480000 | InitInstance (startup.md) |
| +0x5c | 0x0058fd70 | same | Run |
| +0x60 | MFC | 0x004803f0 | PreTranslateMessage: swallows WM_SYSKEYDOWN/UP while +0x1d8 is set |
| +0x64 | MFC | same | PumpMessage |
| +0x70 | MFC | 0x0047fa10 | ExitInstance |
| +0xa4 / +0xa8 | 0x0058faa0 / 0x0058fa80 | same | activate / deactivate (+0xd0, returns the old value) |
| +0xac | 0x0058fac0 | 0x00480570 | start the engine (below) |
| +0xb0 | 0x0058fc30 | 0x004808a0 | stop it (startup.md, Shutdown) |
| +0xb4 | 0x00590740 | same | MM_MCINOTIFY: the CD's notify (0x00572bf0), then the state's +0x24 |
| +0xb8 | nothing | nothing | idle, called with "first idle pass" |
| +0xbc | MFC | 0x0047ff90 | create the main window (0x00482d10) |

zGameApp's fields: +0xc4 the first state, +0xc8 the stack depth (-1 empty),
+0xd0 active, +0xd8 sixteen state pointers, +0x118 the idle wait (100 ms),
+0x120..+0x148 an MSVC `std::deque` of queued commands (16-byte blocks
`{0, type, state, arg}`).

**Much of the game reads the app's fields by absolute address**: 0x00608e14..
0x00608e64 are the app's +0x18c..+0x1dc. Earlier notes called some of them
globals. The ones known:

| Field | Address | Meaning |
|---|---|---|
| +0x18c | 0x00608e14 | a mission has started (set by the mission start) |
| +0x18d | 0x00608e15 | 1 (set by the constructor and the lobby path) |
| +0x18e | 0x00608e16 | the lobby flow (0x0047fed0's argument): the Intro state plays no movies |
| +0x190 | 0x00608e18 | the input mode, 5; bits 2-3 == 2 (8) make the mouse follow the Windows cursor |
| +0x198 | 0x00608e20 | the default mech, "bushwacker" (a CString) |
| +0x19c | 0x00608e24 | started from a DirectPlay lobby |
| +0x1b2 | 0x00608e3a | 1; 0 after the debug menu's Mech Lab (0x0047ff60) |
| +0x1b4 | 0x00608e3c | the `-cdN` track |
| +0x1bc / +0x1bf | 0x00608e44 / 47 | the main menu presses Instant Action on entry (0x0047fc20: set by the debug menu's Instant Action and on the way back from an Instant Action game, 0x0047cfe8), then clears it, keeping the argument |
| +0x1bd | 0x00608e45 | a network game is on (0x0047fc40): cheats leave the mech alone, system keys are locked |
| +0x1be | 0x00608e46 | the main menu presses Training on entry (0x0047fe00: set on the way back from a training mission, 0x00491ba5, 0x00492e89) |
| +0x1c1 | 0x00608e49 | movies on (`-nofmv` clears it) |
| +0x1c3 | 0x00608e4b | no briefing and debriefing voices |
| +0x1c4 / +0x1c5, +0x1cc..+0x1d4 | 0x00608e4c.., 0x00608e54.. | GameIntensity's flags and values (options.md) |
| +0x1c6 | 0x00608e4e | the main menu presses Credits once on entry: set when the "Win Movie State" (constructor 0x0047a960, vtable 0x005c3958: fmv.zrd's c4win, the campaign's ending) finishes its movie (0x0047aa50) and replaces itself with Main |
| +0x1c7 | 0x00608e4f | full screen: the video start restores the saved card and mode; no cursor polling |
| +0x1d8 | 0x00608e60 | lock the system keys (Alt-Tab etc.) in missions |
| +0x1dc | 0x00608e64 | the state after the intro |

## Run (0x0058fd70)

As written, ported as `zgame::App::run`:

```
raise the thread to THREAD_PRIORITY_HIGHEST
loop:
  first_idle = 1
  loop:
    loop:
      while a message waits: PumpMessage; on WM_QUIT return ExitInstance()
      ztimer::update (0x00575a00); network poll 0x00588930(-1, 0)
      cur = the top state
      if active: break
      if no message waits: idle(first_idle); first_idle = 0;
                           MsgWaitForMultipleObjects(.., 100 ms, QS_ALLINPUT)
    first_idle = 1
    if a command is queued: break
    if cur and cur.update(): deactivate; PostQuitMessage(0)
  run the first queued command; drop it
```

So each pass runs either one queued state change or the top state's update,
never both, and nothing runs while the window is inactive. A state quits the
game by returning true from update. The window's `OnActivate` (0x00590c00)
tells the top state (+0x04), then deactivates (+0xa8, input released
0x0051b050) or activates (+0xa4, input taken 0x0051b080, the window restored
from an icon 0x00579f20).

**Queueing** (`replace` 0x00590250, `push` 0x00590400, `pop` 0x005905a0) appends
the command, then tells the states: replace and pop call the current state's
+0x14 ("leaving"), replace and push the new state's +0x08 ("queued"). Push does
not warn the state below. All of MW3's states leave both empty.

**Commands** (in Run): Pop calls the top's leave (+0x18), drops it and resumes
the one below (+0x20, with the argument); Push suspends the top (+0x1c) and
enters the new one (+0x0c), which may refuse; Replace leaves the top and enters
the new one, re-entering the old one when it refuses. Quirks kept: a refused
Push leaves the state below suspended; Pop below the bottom reports "Stack
Depth Underflow" (an empty function in the release build, below) and resumes
slot 0; Push past 16 overwrites the top.

## The states

The base vtable 0x005c36a8: +0x00 destructor, +0x04 window (de)activated,
+0x08 queued, +0x0c enter (true), +0x10 update (false), +0x14 leaving,
+0x18 leave, +0x1c suspend(arg), +0x20 resume(arg), +0x24 message (1). Picked
by number with 0x00482970:

| n | Object | Vtable | State | What it does |
|---|---|---|---|---|
| 0 | 0x00608e90 | 0x005c4078 | Attract | pauses the CD music, plays fmv.zrd's ATTRACT; pops (1) and resumes the music in mode 6 when done |
| 1 | 0x00608f58 | 0x005c40c8 | Intro | INTRO (when movies are on and not the lobby flow), then the SPLASH logos (not the lobby flow), then Replace with state 12 |
| 2 | 0x00608f00 | 0x005c5030 | Main | the main menu (below) |
| 3 | 0x00608f48 | 0x005c40f0 | Pause | a short busy wait (0x00599f70(4, 1)), the pause screen; Escape pops with 6 |
| 4 | 0x00608ef8 | 0x005c4178 | Exit | stops the game (0x004808a0) on enter; its update returns true: quit |
| 5 | 0x00608ec0 | 0x005c4040 | Core | the mission (below) |
| 6 | 0x00608f18 | 0x005c4148 | Load | the briefing (briefing.cpp): its movie, then the briefing screen |
| 7 | 0x00608f38 | 0x005c4120 | Pre Lab | the debug menu's Mech Lab: pilot "Default Player", game type 0, world 6; then Replace with Load |
| 8 | 0x00608ee8 | 0x005c3ee8 | Salvage | a screen only |
| 9 | 0x00608e70 | 0x005c3980 | Win | the debriefing (below) |
| 10 | 0x00608ea0 | 0x005c40a0 | (no name) | lists the 0xa7 input commands with their keys; Escape pops |
| 12 | app +0x1dc | | | the state after the intro: Main, Load for a "DefaultPlayer" lobby launch |

Main, Pause, Load, Win and Salvage share a base (0x0048eb10, vtable
0x005c6038): a 2D screen at +0x0c drawn every update between video lock and
unlock, then flipped; command 1 (Escape) calls the state's +0x28; enter and
leave save and restore the cursor (0x00579840) and take and release input.

**Main** (0x00486990..): makes the screen (0x00486260) and nine menu objects
(0x00609858..0x00609878, among them the Action, Campaign, Training and
Multiplayer states of gametypes.md). Sixty idle seconds (+0x14) push the
Attract state. Hotkeys press the screen's buttons: raw keys read with 0x0051a9b0, not
commands (input.md): O 0x18 Options, C 0x2e Campaign, M 0x32 Multiplayer, I
0x17 Instant Action, T 0x14 Training, Q 0x10 Quit, R 0x13 cRedits, the first
pressed this frame wins. On entry the screen's Multiplayer button is pressed
in a network game or a lobby, and the flags above reopen the menu a game
came from (Instant Action, Training) or the credits after the campaign's
end. The screen is ported (mainmenu.md). Escape asks "quit?" (0x00482a20, messages 0xdc8/0xde1) and
on yes replaces with Exit. Back from a game (resume 4 or 5) 0x00477f50
("mechlab") runs.

**Win** (0x0047ac20): in a campaign the mission counts only when nothing
marked it failed (`[0x00609926]`) and no cheat was used (`[0x00609994]`,
protection.md); then 0x0047ae10(0, 1) keeps the results (0 for operation 4
mission 4), the pilot's campaign record takes the screen's values, and the
voice `c<op>m<mission>win` plays unless +0x1c3; operation 4 mission 6 sets
+0x10, the campaign won. Otherwise 0x0047ae10(1, 1).

**Load** (0x00409440): the briefing movie first (0x00409320 starts it, then it
is polled), then once: the mission's sounds and `zbd;zbd\<dir>` geometry,
the briefing name `brief_<dir><n>` (`brief_ia..`, `brief_mp..`; "default"
otherwise), its voice unless +0x1c3, the screen (0x00405230), then the mission load 0x00490880 (mission-load.md).

### The mission: Core

Enter (0x00480c70): in a network game the system keys are locked
(`SystemParametersInfo(SPI_SETSCREENSAVERRUNNING)`, Windows 9x); all options
applied (0x0040cb30); without a pilot's mech, Replace with Main is queued; the
in-game screen mode set; the drawing path picked; the message box
(dlgMessage.zrd MESSAGEBOX) made hidden; the mission started (below); then
a fade (software) or resume(7); "mech_powerup" plays unless the pilot's +0x59;
the mouse acceleration options applied.

The drawing path (0x00480bf0), re-picked whenever VMode changes: Direct3D
0x00481790, software 0x00481290, or software doubled 0x00481a10 (VMode 3,
320x240 drawn at 640x480).

**One mission frame** (update 0x00481f70 -> 0x00481e70):

1. +0x14 += dt.
2. Unless paused (+0x18): input poll; the cursor follows the mouse when not
   full screen and the input mode says so.
3. zClass's action lists (0x004f7d80: lists 0..5, each followed by the frame
   update 0x004f7f70; `action-lists.md`): the scripts' scrolling textures
   (1), the player's tick and the radar (2), weapon_tick (3), the
   animations (4), the puffers (5). Then sound (0x0056dfb0(0, 1)).
4. The vehicle: the view's target (+0x64) vt+0x50; the cockpit flag when the
   view's camera (+0x58) is kind 1.
5. The drawing path (software, below). It returns true when command 1
   (Escape) is down: outside multiplayer the frame stops and update pushes
   Pause; in multiplayer the message box asks instead (0x0048e2d0) and the
   mission runs on.
6. The message box updates (vt+0x00(dt)); the frame is flipped
   (0x00576ea0(0, 0, 0, 0)).

The software path (0x00481290): the window cleared, the render target set;
the cockpit hidden; the scene begun and the view's camera run (vt+0x04),
drawn; the mech's AI ticks (0x00430aa0 +0x144c vt+0x28); Escape is checked
here, before the world moves; then effects (0x004a8cd0), the missions
(0x0048f470, mission-frame.md), the pilot (0x004783a0) and cameras (0x0040f8d0) update; effects
and particles draw under the video lock; the cockpit parts (+0x394, +0x398, the
torso) are drawn over it; the HUD of the view; the scene ends and presents;
the target's vt+0xe4(dt); under the lock the HUD (0x0040fac0, 0x0040e4b0),
the cockpit (0x0041ab90, or the debug overlay `[0x00608f74]`), the radar.
The Direct3D path does the same around BeginScene/EndScene, with the mech's
global light off for its torso and a shadow pass. The doubled path also sets
the view's window to the window size around the present and back.

**The mission start** (0x00480df0): the clock ticks twice around a 30 ms
`Sleep`; "Dummy Frame" and " L e t s  P l a y ! " markers (0x0040fef0);
cursor on, input reset, the view reset and run once, the AI list reset; the
25 HUD text lines cleared; then the CD track: `-cdN` when 0 < N < the
track count; else on a disc of 2..5 tracks `mission % (tracks - 1) + 2`;
on a bigger one `rand() % tracks + 1`, which can be track 1, the data track
(silent; discoveries.md); played in mode 6 (replay). In Direct3D the fog
colour is set to (25.5, 51, 76.5) when it differs.

Leave (0x00481fc0): mouse acceleration back to 1.3, screen mode 5, the CD
stopped, the mission archives closed, animations, sounds and voices stopped,
the system keys unlocked.

## The release build's error reports are empty

0x0049bfd0, the `FUN_0049bfd0(level, file, line, format)` every assertion
calls (zerr.h), is a lone `ret` in 1.2: the same address fills every empty
virtual slot. "Stack Depth Underflow" and the like are never shown or logged.
The rebuild keeps its report sink for debugging; nothing installs one.

## The port

The program that runs all this on SDL3, and what each hook is wired to
there, is program.md. `zgame::App::run` takes a `platform::MessagePump` (PeekMessage, PumpMessage,
MsgWaitForMultipleObjects, PostQuitMessage, the thread priority). The game's
calls into modules not ported go through `mech3::Engine` (app_hooks.h), one
virtual per original function with its address as the key; the defaults call
the port where it exists (options, ztimer, zsnd_cd, mech3_music, the CRT
`rand`). Fields of objects not ported are read and written through
`peek`/`poke` with the original's addresses, and their virtual calls go
through `vcall(object, slot)`, so the code stays shaped like the original
until those modules are ported. States are named to hooks by their original
address.

FIX (A): window class, window creation and title, the debug menu, UpdateWindow,
MessageBox/Beep, ExitProcess, RunDir, the Mech3Msg.dll load and the DirectX 6
check, the system-key lock, GetCursorPos and Sleep are front-end calls.
FIX (trap): the exe self-check (0x004854c0) and the CD loop (0x00482f90) are
left out (protection.md).

## Testing

`tests/differential/calltrace.py` compares orchestration code by its calls.
The original runs in the emulator with every callee stubbed (an exe function
by address, an import by IAT slot, a fake object's vtable slot); each stub
records its key and arguments and answers from a script that depends only on
the key and its occurrence. Then the rebuild runs with those answers replayed
(`Replay`), its peeks answered from the original's memory before the run
plus what stubs wrote, its pokes kept apart. Compared: the call traces, the
state changes queued, the result, the ported fields at their original
addresses, and every byte either side wrote outside the ported fields.

- `test_app_loop.py`: start + Run with scripted messages, activations and
  states that queue changes; 60 runs, the traces and the final stack.
- `test_app_options.py`: options.md.
- `test_app_states.py`: 56 functions of the app and the states, 60 worlds
  each (pilot, view, vehicle, menus, options as fake objects).
- `tests/test_zgameapp.cpp`, `tests/test_app.cpp`: the loop's rules and a few
  behaviours (command line, the attract timer, the music pick) in C++.

Breaking the port showed each test catches what it covers: Push warning the
state below, a refused Push resuming, the idle flag not reset, camera mode 2
read from the wrong bit, VMode 4 treated as 5, heat off not a cheat, `-cd`
matched on two letters, the music never picking track 1, cheats not stopping
a campaign result, no second try with reader.zbd, the hotkeys reordered,
Escape always pausing, the shell redrawing on any resume: 13 of 13 caught.

## Open

- The key list's screen (0x00482570) and the screens' constructors are the
  2D front end's; KeyListState::enter is one hook. The main menu's is ported
  (mainmenu.md) and reached through `construct(0x486260)` in the program;
  0x0048ee00, which deletes a shell's screen, is `ShellState::clear_screen`.
- 0x004814e0, the Direct3D path's first call, is the mechs' projected
  shadows (ported: `mech3::update_shadows`, render-shadows.md); its entity
  calls (0x005a85e0, the node handles) wait for the entities' port.
- What 0x00477f50("mechlab") does (likely reloads or saves the pilots).
- 0x004810d0 toggles the debug overlay at 0x00608f74 (called from 0x00480b20,
  itself not called); not ported.
