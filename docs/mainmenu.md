# The main menu screen (0x00486260): mapped, not ported

Mech3.exe 1.2, 2026-10-20. The Main state's screen (game-loop.md, Main),
read from the constructor, the vtables and the button handlers; nothing here
is ported yet. `MainState` (`mech3/app_states`) still reaches it through
`construct(0x486260)` and `vcall`.

## The screen

`new(0x12c1c)`, constructor 0x00486260(state), vtable 0x005c4b10 (the
mech3 menu screens' base 0x005bc190 differs only in +0x08, the deleting
destructor 0x00485a60 -> 0x00485780). A zTwoD Dialog (0x0059ec40, twod.md)
with, by offset:

| Offset | What |
|---|---|
| +0x10fec | the Main state |
| +0x1116c, +0x11170 | 1, 1; then two Images (0x0059b970) and a rectangle set twice (0x0042a140, the second from the render size 0x0040d770 / 0x0040d760) |
| +0x110b0 | BACKGROUND: an Image subclass, vtable 0x005bc1b0 |
| +0x111e8 | byte: menus move (not FastMenu, 0x0040cec0; `-seasick` turns it off) |
| +0x111ec, +0x11494, +0x1173c, +0x119e4, +0x11c8c, +0x11f34, +0x121dc | the seven buttons (0x2a8 bytes each): INSTANTACTION, NEWCAMPAIGN, MULTIPLAYER, TRAINING, OPTIONS, CREDITS, QUIT |
| +0x12484 | MECH3TEXT: an Image subclass, vtable 0x005bd988, a float 2.0 at its +0xbc |
| +0x12600 | the dialog's +0x128 (the definition) when it loaded, else 0 |
| +0x12750 | a Text (0x005a15a0): the version line, "%s %s" of a message (0x004af290 -> 0x00576040) in a fonts.zrd font (0x00517680), centred by measure (0x005a1380); +0x12bc4 set: the text's +0xac |

The constructor, after the members: unless the no-voice flag (app +0x1c3)
a sound (0x0056f520), a CD cue (0x00572a30) and 0x004ac290 (likely the menu
music); the image path (0x00516f50); the render target from zVideo's
buffers (0x00576db0..0x00576d80, 0x00558290); `load("dialog.zrd",
"MAINMENU", !moving)` (0x0059f010); then BACKGROUND as a primitive, the seven
buttons by name, the version text, and either MECH3TEXT as a primitive
(menus moving) or, with FastMenu, 0x00486ed0: the screen drawn once at rest
and grabbed as the static background, every button set to its end place
(+0x90(1)). Then the page flipping mode (0x0059fcf0: 2, or 0 with
FastMenu), active (+0x04(1)), and [0x00609854] = the screen.

Shipped MAINMENU (dialog.zrd) labels: CAMPAIGN, TRAINING, INSTANT ACTION,
MULTIPLAYER, OPTIONS, CREDITS, QUIT (`mw3 --dialog dialog.zrd MAINMENU`
draws them, now with zInput's mouse).

## The buttons

Class constructor 0x00438d70(-1) (a Control, 0x0059bbe0, with a child Image
at +0x1ec), base vtable 0x005c4f90; the seven differ only in +0x00 (their
destructors 0x00486840..0x00486900) and +0x30, the left button's release:

| Button | +0x30 | Pushes |
|---|---|---|
| INSTANTACTION | 0x0048e980 | [0x0060985c] (0x0048a6c0's object) |
| NEWCAMPAIGN | 0x0048ea80 | [0x00609864] (0x0048aef0) |
| MULTIPLAYER | 0x0048e9c0 | [0x00609860] (0x0049c900) |
| TRAINING | 0x0048ea00 | [0x00609874] (0x0048b120) |
| OPTIONS | 0x0048ea40 | [0x00609868] (0x00438be0) |
| CREDITS | 0x0048eac0 | [0x00609878] (0x004295e0) |
| QUIT | 0x0048eb00 | (not decompiled; likely the quit question) |

Each: when the button is live (+0xc4), Control's press (0x0059c960: input
reset, the ACTIVATE image, sound and labels) and +0x8c(1, 0) (0x0040a6e0:
tells its owner, +0x08's +0x14, with its id +0x150); then
`App::push(menu object, 0)` (0x00590400). The menu objects are the states
of gametypes.md made by the Main state's enter.

The shared slots: +0x24 update 0x0048e810 (the child image's update, then
0x00485650, the slide), +0x2c 0x00485590 (the bounds, none while sliding:
+0x14c), +0x3c enter 0x00485570 (+0x98, then Control's), +0x40 leave
0x00485640, +0x54 accepts 0x0040a720, +0x5c hit 0x004855f0 (inside the
bounds), +0x84 read_control 0x0048e6b0 (Control's read 0x0059cdc0, then two
values from the entry, a float default 20.0 at +0x1d4 and an int default
200 at +0x1d0, their square at +0x1d8, the child image's BITMAP placed
by the entry's offsets and shown for good), +0x88 after_bind 0x0042a250
(the start and end places +0x15c..+0x168 from the position, the child
moved), +0x8c 0x0040a6e0.

## Left to port

The screen class with BACKGROUND (0x005bc1b0: 0x0041ebb0, 0x0040a490,
0x0040a210) and MECH3TEXT (0x005bd988) images, the button class (0x00438d70,
0x00485570..0x00485780, 0x0048e6b0, 0x0042a250, 0x0042a3d0, 0x0042a400) and
its slide, 0x00486ed0, the destructor; the targets are the nine menu states
(gametypes.md), none ported, so each press would be a logged hook until
they are; QUIT's 0x0048eb00 to be read from the disassembly. The test would
build on `test_twod.py`'s world (twod.md section 7) with the press's
`App::push` and the menu objects stubbed.
