# The menus the main menu opens: Options and Credits (ported)

Mech3.exe 1.2, 2026-10-20. The main menu's OPTIONS and CREDITS buttons
(mainmenu.md) push *menu objects*: shell states (game-loop.md) made on the
heap, each with a zTwoD screen (twod.md). Ported: the four menu objects of
the options and the credits, the credits screen, the game options and the
graphics and audio options, and the cycle button they share. The controls
screen (0x0042a590) is not: its state is ported and its screen stays a hook.
Code: `src/mech3/menu_states`, `credits`, `options_menu`, and the shared
pieces in `menu_widgets`; checked by `tests/differential/test_menus.py` and
`test_app_states.py`.

## 1. The menu objects

The Main state's enter (0x00486990) makes nine (game-loop.md); the pause
screen's Options button (0x00492d70, case 2) makes the three option ones
again and pushes the game options. Main leaves (0x00486c50) and Pause leaves
(0x004931e0) delete them. Each is a shell state (0x0048eb10: name, screen at
+0x0c, vtable over 0x005c6038); its destructor (0x0048eb80) deletes the
screen.

| Global | Name | Constructor | Vtable | Screen (size) | Made |
|---|---|---|---|---|---|
| [0x00609868] | Game Options State | 0x00438be0 | 0x005bf7a8 | 0x004379f0 (0x1407c) | with the state |
| [0x0060986c] | Gfx and Audio Options State | 0x0043a640 | 0x005c0040 | 0x00438f60 (0x13f9c) | with the state |
| [0x00609870] | Controls Configure State | 0x0042d9d0 | 0x005be3d8 | 0x0042a590 (0x16c94), not ported | with the state |
| [0x00609878] | Credits State | 0x004295e0 | 0x005bd938 | 0x004289a0 (0x11ae0) | on entering |
| [0x00609858] | Pilot State | 0x00488910 | 0x005c54e8 | 0x00486f40 (0x13184) | with the state; not a main menu button's (the campaign's and Instant Action's), not ported |

The option states' enter (0x00438c90 for the game options, 0x0042da50 for
the other two) is the shell's (0x0048ebf0) then the screen's +0x10 with 0:
the screen reads the options. Only the game options first switch to the
menus' screen mode 5 (0x004ab6b0(5), 640x480): the sub-screens open from it.
Resume is the shell's (0x0042da70). Escape (+0x28, 0x00438c80) pops with 1.
The credits state makes its screen on entering (0x00429600, the clock
[+0x11adc] zeroed) and deletes it on leaving (0x00429670: 0x0048ee00, then
the shell's leave). Their +0x24 (0x0042a130) answers 1, as the base.

## 2. The shared pieces (menu_widgets)

**The screen** (MenuScreen, mainmenu.md): +0x10 is the option screens'
read/apply (empty in the base, 0x004a5bc0). FastMenu's still screen
(0x00486ed0) is called on every one of these screens, not only the main
menu's. The sub-screens borrow the main menu's BACKGROUND (inline in each
constructor): with a main menu ([0x00609854]) its image (+0x110ec) and
whether it moves (+0x111e8), else (the option screens only) the dialog's own
BACKGROUND; a still background is placed at the origin with its view
rectangle as the source. The option screens also make it still while a
mission runs (0x0047fb70, the Core state's +0x19): the pause screen's
Options. The credits read [0x00609854] unchecked.

**BACK** (0x00428c60, on the credits and both option screens) only calls
the state's +0x28: no ACTIVATE sound or image, unlike every other button.

**The cycle button** (0x0048b280, vtable 0x005c5ba0, "cycle button" by the
text its destructor leaves in its label), 0x424 bytes: a ShellButton (its
own id -1; the cycle's id at +0x420 goes to the screen's +0x14) with a list
of choices (+0x15c, 8-byte items: a `_strdup`'d text and a value), the
current index (+0x16c, -1 for none), and TOGGLE_LEFT and TOGGLE_RIGHT read
as Controls of their own (+0x188, +0x2d4) and taken out of the dialog, at
their POSITION from the button. The choice shows in the first LABEL
(+0xa0, 0x0048b850; "" for none). A press (0x0048b6b0) steps back on the
left arrow, forward on the right one, or forward anywhere when +0x168 is set
(+0x94); the screen hears of it either way. Stepping (+0xa4, 0x0048b790)
wraps when +0x170 (always 1 here), else stops at the ends. The first choice
added is selected at once (0x0048bac0). Rolling over (0x0048b550) lights the
right arrow before the left. +0x90 (0x0048b490) enables the button and its
arrows. After binding (+0x88) each kind adds its choices:

| Kind | +0x88 | Choices (message, value) |
|---|---|---|
| ON / OFF | 0x00438ed0 | OFF 0, ON 1 |
| ON / OFF, one in red (0x00438090) | 0x00437fe0, +0xa0 0x00438040 | OFF 0, ON 1; ia_small_green, the alert one (+0x42c) in ia_small_red |
| LOW / MEDIUM / HIGH | 0x00438f10 | LOW 0, MEDIUM 1, HIGH 2 |
| CAMERA | 0x00437f90 | 1st Person 1, 3rd Person (high) 0, 3rd Person (low) 2 |
| RESOLUTION | 0x004394b0 | the device's modes (section 5) |
| VIDEO_DEVICE | 0x004394d0 | Software Render -1 (InstallOptions 0xff00), then each card by name (0xff) |
| TEXTURES | 0x00439550 | 2 MB 6, 4 MB 5, 8 MB 3, MAX 0 (section 5) |
| SOUND_QUALITY | 0x00439570 | HIGH 0 (InstallOptions 0x40000), LOW 2 (0x10000) |

The helpers 0x00438e10 (item or null), 0x00438e50 (value or 0), 0x0048b8f0
(select the first with a value) and 0x0043a710 (select every one with it:
the last stays) sit with the option screens.

## 3. The credits (0x004289a0, vtable 0x005bd738)

dlgCredits.zrd's CREDITSDIALOG: CANCEL (+0x111ec, BACK above), CREDITS_SCREEN
(+0x113d8, vtable 0x005bd758, a Control), CREDIT_TEXT and CREDIT_LINES. A
float 4.0 at +0x11ad8 is read by nothing; +0x11adc is the clock.

CREDITS_SCREEN's read (0x00429200): RECT ((left top) (right bottom)) from
the control's place; SCROLL_RATE; SCROLLING_TEXT, a list of sections, each a
list of lines (("key" ...) "offset" (dx dy) "font" ("name")). A line's text
is its keys' messages run together; the line is a FontText plus its offset,
copied into the section (0x00429700) and the section into the list
(0x00429a70) as HP STL vectors, so every copy and every growth of the list
copies the lines' GDI fonts (ported as written, `HpVector`). A section is as
tall as its tallest line (text height + dy); the sections stack.

Each frame (0x00429030) the clock grows by the frame time and t = clock /
(sections x SCROLL_RATE); past 1 it starts again. 0x00429120 places every line
at dx + left, dy + y with y = (top - total) t + (1 - t) bottom (truncated):
the names rise from the rectangle's bottom (t = 0) until the last has left
its top (t = 1), and a line shows only wholly inside it. The shipped file
has 246 one-line sections at SCROLL_RATE 1.0: four minutes a pass, at the
same speed at any frame rate (it goes by time).

A line whose "offset" is not two numbers keeps the one read before: the
original reads each into the same stack slot. The shipped "Other Voices"
heading has `(20)`, so it takes the name above's (30 0): indented like a
name with no gap (fixes.md, Candidates). The port keeps the offset across
lines the same way.

## 4. The game options (0x004379f0, vtable 0x005bee60)

GAME_OPTION_DIALOG. Read on entering (+0x10, 0x00438100 with 0), written on
ACCEPT:

| Button | Kind | Option (getter / setter, options.md) |
|---|---|---|
| DIFFICULTY | LOW / MEDIUM / HIGH | GameIntensity (0x0040cdf0 / 0x0040cc20) |
| CAMERA | CAMERA | CameraMode (0x0040ce70 / 0x0040ce10) |
| INVINCIBLE | ON red | Invincible (0x0040cf20 / 0x0040ced0) |
| INFINITE_AMMO | ON red | InfiniteAmmo (0x0040cf80 / 0x0040cf30) |
| HEAT_MGMT | OFF red | HeatMgmt (0x0040cfc0 / 0x0040cf90) |
| TORSO_AUTO_RTN | ON / OFF | 0x0040d010 / 0x0040cfd0 |
| THROTTLE_CONST | ON / OFF | 0x0040d060 / 0x0040d020 |
| MW2JJETS | ON / OFF | 0x0040d1e0 / 0x0040d1c0 |
| CMD_INVERT_MOUSE_Y | ON / OFF | MouseAccelY < 0; written -1.3 or 1.3 straight into the entry |

The red choices are the cheats (protection.md: turning them on sets the
cheated flag). ACCEPT (0x00437f60) applies (+0x10 with 1), writes the
settings to the registry (0x00597a60) and calls the state's +0x28; BACK
leaves without applying. GFX_AUDIO_OPTIONS and CONTROLS_CFG (0x004380c0,
0x004380e0) push [0x0060986c] and [0x00609870], also without a press sound.

## 5. The graphics and audio options (0x00438f60, vtable 0x005bf7d8)

GFX_AUDIO_OPTIONDIALOG. MUSICSLIDER and SFXSLIDER (vtables 0x005bffb0,
0x005bff20) are Sliders whose press sets CDVolume (0x0040dab0) or
SoundVolume (0x0040da80) at once; the cycles: SHADOWS (Shadow),
SELF_SHADOWS (ShadowParts), LIGHTING (GfxFlags bit 0), RESOLUTION
(InGameVMode, the mode a mission runs in), VIDEO_DEVICE (HWCardDev),
TEXTURES (TextureMemory), SOUND_QUALITY (SoundLOD), OBJECT_DETAIL
(ObjectLOD) and SPECIAL_EFFECTS (EffectsLevel), the last two shown
inverted (level 0, the most, is HIGH).

**The device** (0x00439a90, and inline when VIDEO_DEVICE is pressed,
0x0043a0d0): RESOLUTION and TEXTURES are rebuilt for it; LIGHTING and
SELF_SHADOWS are enabled for a 3D card, SHADOWS also needs its capability 3
(0x00579b70); the device is selected for a moment to show its own lighting,
effects and shadow settings (the _SW or _HW entries, options.md), then put
back; zModel's and zRender's GfxFlags are pointed at its entry (0x00526750,
0x00557ae0).

**Resolutions** (0x0043a470): the modes from 0x004ab860's range: software
3..5 (320x240 doubled, 640x480), a 3D card 5..7, 7 (1024x768) cut to 6 below
4.5 MB (three frames of 1.5 MB) of free video memory and to 5 below 2.75 MB (three frames of 960,000 bytes). The
message table names 1152x864, 1280x1024 and 1600x1200 (0xfda..0xfdc) and the
list would show them, but no range reaches past 7: they stay registry-only
(community-issues.md). The old choice is kept, pulled into the range.

**Textures** (0x0043a310): software 2 MB, 4 MB and MAX by InstallOptions
0x100, 0x200, 0x400; a card 2, 4 and 8 MB by InstallOptions 1, 2, 4 and its
memory ([0x0072f38c], in MB; 2 if unknown). TextureMemory is selected if
set, else the last one added.

**ACCEPT** (0x00439440) queues the pop, then applies through the state's
screen (+0x10 with 1, 0x004395c0): a device other than the one read on
entering asks "You have changed a setting that requires the game to be
restarted" (0x004ac150, messages 0xf2e / 0xf2f); yes sets HWCardDev,
replaces the state stack with the Exit state (0x00590250(state(4))) and
applies the game options' screen too; no puts the old device back. The rest
is written either way; lighting and both shadows only when HWCardFlag and
InstallOptions 0xff allow a 3D card, else written off.

## 6. Resolution (for the any-size work, fixes.md Candidates)

The dialogs are 640x480 pixel positions, as the main menu's. Each screen's
background view is the display size when it is made: 640x480 from the main
menu, but the mission's mode when the pause screen makes the option screens
(before the game options switch to mode 5). With the background still (a
mission running, or FastMenu) the view is the source rectangle of the whole
1280x960 image, so on a screen taller than 960 lines or wider than 1280 the
draw reads past the image: exactly the modes above ~1000 lines (1280x1024,
1600x1200) players see crash in the pause screen's Options
(community-issues.md). In the original the switch to 640x480 comes before
the first frame, and the screen-side clip of both blits keeps that draw
inside the image, so the crash itself is likely in a draw before the switch
(FastMenu's still screen is drawn at construction) or in the switch's own
grab of the old screen (0x004ab6b0, zVideo); the port, which has no mode
switch and will draw menus at any size, cuts every source rectangle at its
image (FIX (B), `zrndr::blit16`, fixes.md; `tests/test_menus.cpp`). The
scrolling background's four pieces cover at most twice the image: beyond
2560x1920 it needs tiling further.

## 7. The port

The states are `mech3::OptionsState` and `CreditsState` (menu_states.h);
the program's `GameEngine` (app_menus.cpp) makes them for the main menu's
`construct` calls, a 0x10-byte block of the module memory standing for each
(its handle is its address for the hooks), pushes them for `push_state`,
deletes them on their +0x00, and answers `state_screen` (+0x0c) for them;
the screens are handles as the main menu's (mainmenu.md, The port), with the
screen's real size for the offsets into it. The options are reached through
`mech3::Engine` (the getters and setters, `set_mouse_accel_y`,
`set_in_game_vmode`, `settings_write`), zVideo's devices and memory through
new hooks (0x00579a10, 0x00579a40, 0x00579aa0, 0x00579a70, 0x00579b70), the
app's queue through `pop_state` and `replace_with_exit`. 0x004ab860 is
ported as `Engine::video_mode_range`. Controls Configure's state is made and
pushed; its screen's `construct` (0x0042a590) is logged and returns 0, so
it shows nothing and Escape returns.

## 8. Testing

`test_menus.py` (30 runs a screen: 90/90 identical, 436,719 calls; 40 a
screen also identical): the original constructor and screen run in the emulator on
`twod_world.py`'s world, plus stubs for every option getter and setter, the
app's push, pop and replace, the restart question, the registry write,
zVideo's device list, names, capabilities and memory, the card memory
callback [0x0072f38c], the GfxFlags binds, the CRT's `_strdup` and
`sprintf`; the state (its +0x28) and the screens reached through states
(+0x10) are fakes whose calls are recorded. Even runs the shipped dialog,
odd runs a synthetic one (buttons missing or moved, arrows and labels
missing, RADIUS / MAXRADIUS; for the credits random SCROLLING_TEXT, RECT and
SCROLL_RATE, 0 included); one in eight with no dialog. FastMenu, the display
size (640x480 to 1600x1200), the main menu's background (there, moving or
still), a running mission, InstallOptions, the devices and every option
vary. The plan: construct, read the options, 30 to 90 frames of mouse input
over the buttons and arrows mixed with presses of every bound button, cycle
and slider, the option screens' +0x10 both ways, the cycles' +0x90, +0x94,
+0xa0, +0xa4, the device change, busy, bake, dirty-all and restore; then a
dump and the deleting destructor. Compared: every call and a dump of the
dialog, its objects and the screen's own state (each cycle's choices, index,
arrows; the credits' sections and lines; the slider values; the entries
written). `test_app_states.py` has seven new cases (the three option enters,
resume, escape, the credits' enter and leave), 40 seeds each: identical.

Breaking the port showed the test catches it, 23 of 24 (12 runs each): the
credits scrolling from the top, a line shown on the rectangle's edge, the
tallest line without its offset, no restart past t = 1, the offset not
carried across lines, RECT not from the control's place; a cycle that stops
at its end, the left arrow winning the rollover, click_steps ignored, no
first selection, the arrows left in the dialog, the alert font on the wrong
choice; inverted Y by the wrong sign, ACCEPT without the registry write, the
camera choices reordered, the background moving in a mission; detail not
inverted, the resolution clamped to the wrong end, 8 MB offered from 4,
shadows without capability 3, the old device not put back when the restart
is refused, lighting written without a card, the slider's reading flag left
set. Missed, and unobservable: after-bind clearing kHidden too (nothing is
hidden at that point). `tests/test_menus.cpp` checks the FIX (B) cut (and
fails with it removed).

## 9. Left

- The controls screen (0x0042a590 and 0x0042be10.., its load and save
  0x0042d0b0, the defaults, the axis popup ASSIGN_AXIS_POPUP and
  LOAD_SAVE_POPUP of dlgOptions.zrd): its state is ported, its screen a hook.
- The Pilot State (0x00488910) and the Action, Campaign, Training and
  Multiplayer states (gametypes.md).
- The pause screen (0x00492750) and its Options press (0x00492d70), which
  make and push the option objects from a mission.
- The menu objects' constructors and destructors run in the program but are
  not in a differential case (their calls are the screen's construct and
  delete, compared through the screens' tests).
