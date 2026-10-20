# zInput: keyboard, mouse, joystick and the command map

Mech3.exe 1.2, 2026-10-20. The engine's input module
(`C:\zipper\GameZMech3\zInput\`: `zin_kbd.cpp` 0x0051a170..0x0051ab90,
`zin_init.cpp` 0x0051b0a0 and its neighbours to 0x0051b360; the mouse
0x0051b3c0..0x0051baf0, the command map 0x0051bb30..0x0051d490 and the
joystick 0x0051d520..0x0051ddc0 follow with no file name of their own;
0x0051dfa0.. is zMath). Ported in `src/gamez/zinput/` on the program's
DirectInput-shaped `platform::Input` (program.md), checked against the exe
by `tests/differential/test_zinput.py`. Key-name table and strings:
`tests/differential/explore/zin_keynames.py`.

## 0. Start, devices, polls (zin_init.cpp)

`start(window, instance)` 0x0051b0a0 (from the engine start, 0x00480570):
DirectInputCreateA(0x500); the keyboard (0x0051a170), the mouse (0x0051b5f0)
and the joystick (0x0051d520), each a flag byte (0x00638584 keyboard, 85
joystick, 86 mouse: bit 0 present, bit 1 suspended) and a use count
(0x00638588, 8a, 8c); one use each. `poll(callbacks)` 0x0051b360 reads each
device whose flags are exactly 1 and that has users: the mouse, the
joystick, then the keyboard; `callbacks` lets them run the command map's
callbacks. A use count's first user clears that device's state (0x0051b290
keyboard, 0x0051b2c0 joystick, 0x0051b310 mouse: the menus' shell takes the
mouse on entry and lets it go on leave, 0x0051b340). `reset` 0x0051b1c0 clears
all three (the menus call it on every button press). `suspend` 0x0051b050 and
`resume` 0x0051b080 are zGameApp's deactivate and activate: suspend only sets
the flags' bit 1 (DirectInput drops a foreground device by itself); resume
clears each suspended device's state. `stop` 0x0051b170.

Errors go to 0x0051ddc0, which names the DIERR_ code and calls the report
function, empty in the release build (game-loop.md).

## 1. The keyboard (zin_kbd.cpp)

Buffered: GetDeviceData of up to 0x80 events (DIPROP_BUFFERSIZE 0x80,
non-exclusive, background) into 0x00638598. A **key code** is the DIK code
with the modifiers held: 0x100 Alt, 0x200 Ctrl, 0x400 Shift (0x006385a8;
either Ctrl, Shift or Alt key). Codes below 0x7de have a state and a
callback at 0x006385ac (8 bytes each). States: a down event gives 1 (3 when
it was already 1), an up event adds 4. Before each read (0x0051a550) the keys
of the previous read age: released (bit 4) to 0, exactly 1 to 2 (held).
Keys not in the last read keep their state. So `& 3` is "down", `== 1`
"pressed this frame", and a key pressed and let go within one frame reads 5.

A read (0x0051a5d0), per event: a modifier key updates the modifiers; any
other key, with modifiers held, first marks its unmodified code released,
gets the modifier bits written into the event (0x40 Alt, 0x20 Ctrl, 0x10
Shift, so the next ageing finds the same code) and becomes the modified
code; then the state changes, and on a key down the text callback
(0x0063c49c, set by 0x0051a9a0 for the line editors) gets the key's
character (0x0051ab90: ToAscii; the keypad without NumLock gives the
editors' cursor codes 1 home, 2 left, 4 end, 6 right, 0x7f delete). Then the
codes of the read are listed once each (0x006385a0, grown one at a time);
Alt-F4 (0x13e) runs its callback at once whatever else happens, if it has
one and is down; then, when `callbacks` is set and no text callback is,
every listed key that is down runs its callback. Any result but 0 or
DIERR_INPUTLOST (re-acquired) drops the read, DI_BUFFEROVERFLOW too.

`read_key(wait)` 0x0051aa00 takes events one at a time and returns the last
plain or modified key that went down. Quirk: the count it asks for is the
last read's, so after an empty read it asks for none; with `wait` (unused,
the controls screen passes 0) it would never return.

**The menus use raw keys, not commands**: 0x0051a9b0(DIK) is what Escape
(1) and the main menu's hotkeys read: O 0x18, C 0x2e, M 0x32, I 0x17,
T 0x14, Q 0x10, R 0x13 (game-loop.md, Main).

**Key names** (0x0051c6b0): by DIK code, each message `MSG_KEY_<name>` from
Mech3Msg.dll or the name itself (`COMMA`, `NUMPAD7` ...); Left Alt is looked
up as `MSG_KEY_HOME` (falling back to `LMENU`), so it shows as Home. 0x0051c480
prefixes Ctrl-, Alt-, Shift- (`MSG_KEY_CTRL` ..). Joystick buttons
`MSG_JBTN_n` / "Button n" (1..10), mouse buttons Left, Right, Middle.
0x0051aee0 finds a key by its name.

## 2. The mouse

Exclusive and foreground (the app's input mode 5 as the cooperative level,
0x0051bae0; the controls' mode bit 8 makes it background), read with
GetDeviceState into a DIMOUSESTATE. zInput keeps an integrated position on a
screen it is told the size of (0x0051b590, from the options' screen mode,
options.md): 15 dwords at 0x0063d1d8 (0x0051b7b0, `zin::MouseInfo`): x, y,
wheel (500 centred), the same from the centre in -1..1, the last motion
and its share, and the left, right and middle button states (1 pressed, 2
held, 4 released: this read against the last, 0x0051b6e0).

Each read (0x0051b7d0): the counts times the acceleration (0x005ece48/4c,
MouseAccelX/Y, 1.3; cut toward zero) moved onto the position, clamped to the
screen; the wheel unscaled and unclamped; with `callbacks`, each button
pressed this read runs its command. Reset (0x0051ba80) forgets the buttons
and zeroes the motion, but not the wheel's: quirk kept, a reset adds the last
wheel count to the wheel position again. 0x0051b440 puts the mouse at a
point from the centre, 0x0051b4f0 at Windows' cursor (windowed missions with
mode 8), 0x0051b560 at the centre; each moves Windows' cursor along
(ClientToScreen, SetCursorPos).

Without a mouse the original crashes at the start: 0x0051b5f0 ignores
CreateDevice's failure and calls through the pointer it did not get. The
port carries on with no mouse (FIX (bug), fixes.md).

## 3. The command map

The game's 0xa7 commands (InitInstance: 0x0051c630(0xa7, "DefaultCmdMap"),
startup.md; the CMD_ names and default keys are set by mech3's controls
setup, 0x00411150, not ported). A map (0x3f5c bytes, `zin::CmdMap`) holds per
command one dword of bindings (key1 bits 0-10, key2 11-21, joystick button
22-25, mouse button 26-27; 0x0051beb0), a callback and a name (0x50 bytes),
and reverse tables: key1 and key2 to command (0x7de each), joystick button
(16) and mouse button (4) to command. The bindings are the settings store's
entry named for the map (0x00597d90, type 7, per user), so they are saved
with the other settings (settings.md; `zin::Host::settings_block`, which the
SDL host answers from the store; null keeps them in the map).

- `bind(cmd, name, key1, key2, joy, mouse)` 0x0051c190 (0x0051d2c0): each
  input taken from the command that had it, the command's old one freed.
- `set_callback(cmd, fn)` 0x0051c2b0 (99 callers): only a command with a
  key keeps a callback; its keys get the shared key callback 0x0051c340
  (which runs whatever command the key is bound to now), the first given
  winning (0x0051a9c0).
- `command(cmd)` 0x0051c370 (0x0051d300): the or of its keys', joystick
  button's and mouse button's states (0 while the map is disabled, +0x3f58).
  PlayerControls reads the mech's commands this way (controls.md).
- `push_map` 0x0051d3a0 / `pop_map` 0x0051d490: a stack of maps; push with
  none copies the current one (the interp console 0x005a40b0 and the
  debug key setup 0x0040e430 push theirs). Each change rebuilds the tables
  and the keys' callbacks (0x0051bca0).

## 4. The joystick

The first attached joystick (EnumDevices), as an IDirectInputDevice2 with
c_dfDIJoystick2; exclusive, foreground with force feedback (DIDC_FORCEFEEDBACK,
0x0051ddb0: the engine start then makes the force-feedback object 0x00433090,
likely), background without. Its setup (0x0051d700): for each of X, Y, Z, RX,
RY, RZ the stick has (GetObjectInfo by offset), the range -1000..1000 (read
back when refused) and a dead zone, 10% on X and Y, none on Z (the
throttle), 25% on the rotations; each axis's centre and scale kept at
0x0063c6fc (0x0051da90). Reads (0x0051dc20): Poll and GetDeviceState
into a DIJOYSTATE2, rotations the stick lacks zeroed, the last one kept as
the previous; with `callbacks`, buttons 1..10 pressed this read run their
commands. Button states as the mouse's (0x0051dcf0). Reset (0x0051dd60)
centres the hats (0xffff) and clears the button bytes 1..10. Quirk kept:
those are buttons 2..11, so button 1, the trigger, keeps its state through a
reset.

## 5. The port

`src/gamez/zinput/`: `zinput.h` (the globals as `zin::State` with their
addresses), `zin_init.cpp`, `zin_kbd.cpp`, `zin_mouse.cpp`, `zin_cmdmap.cpp`,
`zin_joy.cpp`. FIX (A): DirectInput is `platform::Input` (open is
CreateDevice + SetDataFormat + the buffer size; set_cooperative_level;
the joystick's GetCapabilities, GetObjectInfo, DIPROP_RANGE and
DIPROP_DEADZONE), the Win32 calls (ClientToScreen, ScreenToClient,
SetCursorPos, GetClientRect, MapVirtualKeyA, GetKeyboardState, ToAscii)
`zin::Host`. On SDL (`SdlInput`, `SdlZinHost`): an exclusive foreground
mouse is SDL's relative mouse mode while acquired (the cursor hidden and held,
as DirectInput's), lost with the focus (reads give DIERR_INPUTLOST until
zInput takes it back); motion is scaled to the game's 640 pixels across the
window; the joystick's ranges and dead zones are applied to SDL's axes; the
characters come from SDL's keymap with the shift and lock states; the
client area is the 640x480 frame.

`mech3::GameEngine` wires the app's hooks: input_init 0x0051c630,
input_start 0x0051b0a0, input_devices_shutdown 0x0051b170, the activate and
deactivate 0x0051b080 / 0x0051b050, input_poll 0x0051b360, key_state
0x0051a9b0, input_clear 0x0051a300, input_reset 0x0051b1c0, input_capture
/ input_release 0x0051b310 / 0x0051b340, the mouse's centre, position and
acceleration, the options' screen size 0x0051b590, joystick_present
0x0051dc10, force_feedback_present 0x0051ddb0. zTwoD's mouse (0x0051b7b0)
and reset are zInput's.

FIX (bug): a key code past the table (a newer key such as DIK_POWER 0xde ..
DIK_MEDIASELECT 0xed, pressed with Ctrl, Alt and Shift held) wrote past the
key states into the text callback's pointer and the joystick's globals; such
codes are skipped. Key names past the table (0x0051c480 with a code of
0xde..0xff read the joystick's names and the mouse's numbers as names) give
none.

## 6. Testing

`test_zinput.py`: 200 random plans against the exe, the x87 at 24 bits.
Fake DirectInput COM objects in the emulator (IDirectInput's CreateDevice
and EnumDevices; each device's QueryInterface, Release, GetCapabilities,
Get/SetProperty, Acquire, Unacquire, GetDeviceState, GetDeviceData,
SetCooperativeLevel, GetObjectInfo, Poll), the Win32 and CRT imports and
the message lookups stubbed; a fake `platform::Input` and `zin::Host` in
the probe (`tests/probe_zinput.cpp`); both ask one scripted World: devices
present or not, acquires that fail, queued key events (modifiers, presses
and releases in one read, Alt-F4 in pieces), mouse counts and buttons,
joystick states, read errors (DIERR_INPUTLOST, DI_BUFFEROVERFLOW,
DIERR_NOTACQUIRED), axes missing or refusing their range, message keys
with and without text. Steps: start, polls with and without callbacks, key
states, binds, callbacks, command states, names, map pushes and pops, mouse
moves, screens, accelerations, resets and waits, joystick buttons and
resets, suspend and resume, use counts, the text callback, read_key, key
characters and names, shutdown and stop. Compared after every step: the
result, every call out with its arguments (callbacks fired included), and
a dump of the globals (9,000 dwords: flags, every key's state and callback,
the event buffer, the joystick's and the mouse's states, the axis setup, the
map's bindings, callbacks and tables) and the names. Result: 200 plans,
51,830 calls out, identical. Scenarios the original cannot survive are not
played: no mouse, GetKeyboardState failing (it then reads its stack buffer
as it was), names past the tables.

Breaking the port showed the test catches it, 12 of 12: a repeated press
not giving 3, presses not ageing to held, Shift read as Ctrl, the mouse
clamped one pixel wide, a second key not freed when rebound, the right
button's command reading the left, the joystick centre halved, Alt-F4 run
while up, the wheel's reset quirk fixed, an overflowing read kept, the
joystick reset's off-by-one fixed, the first key's callback not set.

## Open

- mech3's controls setup: the default bindings and CMD_ names 0x00411150
  (with 0x004110d0, the groups 0x00410b70, the name map 0x004127d0), the
  controls file reader 0x004120a0 (the axis table 0x006040d0) and the
  controls screen (0x0042be10 ..), which use read_key and the wait functions.
- The settings store entry the bindings live in (0x00597d90).
- The static object at 0x0051af60..0x0051b020 (lists at +0x4848) between
  the two files: not traced.
