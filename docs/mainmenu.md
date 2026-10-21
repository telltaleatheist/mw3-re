# The main menu screen (0x00486260): ported

Mech3.exe 1.2, 2026-10-20. The Main state's screen (game-loop.md, Main): a
zTwoD dialog (twod.md) built from dialog.zrd's MAINMENU, with a scrolling
background, seven rippling buttons, the scrolling MECH3 title and the version
line. Ported in `src/mech3/menu_widgets` (what the menu screens share) and
`src/mech3/mainmenu`; checked against the exe by
`tests/differential/test_mainmenu.py`. The screens the buttons open are not
ported: each press reaches its hook.

## 1. The classes

No RTTI; names are ours. Original files unknown (menu_widgets.h says where
each sits).

| Class | Constructor | Vtable | Size | Base |
|---|---|---|---|---|
| MenuScreen | 0x0040a110 (inlined in 0x00486260) | 0x005bc190 | 0x110b0 + 0x13c | Dialog |
| ScrollImage (BACKGROUND) | in the screen's | 0x005bc1b0 | 0x13c | Image |
| ShellButton | 0x0040a5d0 | 0x005bc230 | 0x158 | Control |
| SlideButton | 0x00438d70 | 0x005bd888 | 0x1ec | ShellButton |
| Ripple (zTwoD) | 0x005a53e0 | 0x005d04a8 | 0x4c | Object2D |
| Marquee (MECH3TEXT) | in 0x00486260 | 0x005bd988 | 0xc0 | Image |
| MainMenuButton | in 0x00486260 | 0x005c4f90, one per button 0x005c4ef0..0x005c4b30 | 0x2a8 | SlideButton |
| MainMenu | 0x00486260 | 0x005c4b10 | 0x12c1c | MenuScreen |

ScrollImage, ShellButton and SlideButton are used by many screens (their
vtables are set in 0x00428a1f.., 0x0042a8cb.., 0x0042daff.., 0x0042e90f..:
the options, the credits and others), so the next screens build on them.

### MenuScreen (0x005bc190)

A Dialog plus: +0x10fec the shell state that owns it, +0x10ff0 a "busy"
byte (+0x18 sets it and returns the old value, +0x1c reads it; a ShellButton
takes no mouse while it is set, 0x0040a720; ShellState::resume sets it
around its redraw), +0x10ff4 an Image for FastMenu's still screen, +0x110b0
the BACKGROUND. +0x14 (0x0040a510, three arguments, empty) is where a
ShellButton reports a press (0x0040a6e0: its id +0x150 and two values). The
slots +0x0c and +0x10 are empty.

### The background (0x005bc1b0)

Unless FastMenu (+0x138, read at construction), update (0x0040a210) moves a
view rectangle (+0xe8, starting at the display size 0x0040d760 / 0x0040d770)
by (+0xbc, +0xc0) = (1, 1) pixels a frame, wrapping at the image's size, and
cuts it into up to four pieces (+0xc4 flags, +0xc8 positions, +0xf8 source
rectangles): the view clipped at the image's right and bottom edges, then the
image's left strip, top strip and corner filling the rest of the screen.
draw (0x0040a490) draws each flagged piece at its place. A second test after
each edge clip (`right < right`) can never be true.

### The buttons

ShellButton (0x005bc230): a Control with +0x14c "reading" (inside
read_control: get_rect returns nothing then, and with FastMenu), +0x150 an id
(-1 for the main menu's), +0x154 "borrowed" (its destructor clears the
images first), +0x30 (0x0042a3d0) Control's press then +0x8c(1, 0), the
screen's +0x14.

SlideButton (0x005bd888) adds the ripple: on enter (0x00485570) it restarts
it (0x00485530: radius 20, phase 0, amplitude 7.0, on unless FastMenu); each
update (0x00485650), while on, the button is dirtied, drawn, and the ring
grows by +0x1bc = 120 pixels a second (0x00485510, with the wavelength
+0x1c4 = 12) up to MAXRADIUS (+0x1d0, default 200), the phase moves on by
pi x the growth / 12, and the amplitude falls by 0.3 a frame; it stops at
2.0 or less, drawing once more. The ring (the Ripple at +0x16c) runs from
radius - 3 wavelengths to radius at the button image's centre (after_bind
0x0042a250, also on every move 0x0042a400). RADIUS (+0x1d4, default 20) and
its square only set +0x15c..+0x168, a square nothing here reads. Leaving
(0x00485640) stops it. The button's bounds (0x00485590) are Control's,
computed with the stashed image when baked; its hit test (0x004855f0) takes
the edges and ignores hiding. +0x1dc is an HP STL vector of objects deleted
with the button, empty in every screen seen.

Baking (+0x90, 0x0042a160) is FastMenu's: the normal image moves to +0x158
and the button draws nothing (the still screen shows it); unbaking puts it
back.

The Ripple (zTwoD, vtable 0x005d04a8, in the render overlay layer's range)
draws through zRender's 0x00558dd0 (x, y, inner, outer, amplitude,
wavelength, phase, clip): every pixel of the ring is copied from the screen
shifted along the radius by a sine of the distance, the inside left alone.
Ported (`zrndr::ripple`, render-ripple.md); the SDL host draws it, zTwoD's
base `Host::ripple` stays a recorded call for the tests.

MainMenuButton (0x005c4f90) adds the line to its label (+0x1ec, AUXBITMAP
("name" dx dy), read raw: list items 1..3, placed at the button's position
plus the offsets and shown for good, 0x0048e6b0), updated before the button
(0x0048e810) and hidden while baked (0x0048e780). The seven differ in their
destructor and +0x30:

| Button | +0x30 | Pushes | Hotkey |
|---|---|---|---|
| INSTANTACTION | 0x0048e980 | [0x0060985c], Action State (0x0048a6c0) | I |
| NEWCAMPAIGN | 0x0048ea80 | [0x00609864], Campaign State (0x0048aef0) | C |
| MULTIPLAYER | 0x0048e9c0 | [0x00609860], Multiplayer State (0x0049c900) | M |
| TRAINING | 0x0048ea00 | [0x00609874], Training State (0x0048b120) | T |
| OPTIONS | 0x0048ea40 | [0x00609868] (0x00438be0) | O |
| CREDITS | 0x0048eac0 | [0x00609878] (0x004295e0) | R |
| QUIT | 0x0048eb00 | nothing: `Mech3App::confirm_quit` (0x00482a20) | Q |

Each but QUIT: when enabled, Control's press (the ACTIVATE image and sound)
and +0x8c(1, 0); then, enabled or not, `App::Push(menu object, 0)`
(0x00590400). QUIT goes straight to the question, without the press's
sound and image.

## 2. The screen (0x00486260)

The members, by offset: MenuScreen (above), the seven buttons at +0x111ec +
0x2a8 n (INSTANTACTION, NEWCAMPAIGN, MULTIPLAYER, TRAINING, OPTIONS,
CREDITS, QUIT; each constructed with its line hidden), MECH3TEXT at +0x12484
(a float 2.0 at +0xbc), an Image at +0x12544 and a Control at +0x12604 that
are never bound, +0x12600 the dialog's tree when it loaded, the version Text
at +0x12750.

Then, in order: unless the no-voice flag (app +0x1c3) the MENU sound set
(0x0056f520) and the menu music, CD track "menu" (0x00572a30) in mode 2
(0x004ac290); the image path "zbd" (0x00516f50); the render target 2 from
zVideo's buffers (0x00576db0, 0x00576da0, 0x00576d90, 0x00576d80 ->
0x00558290); `load("dialog.zrd", "MAINMENU", no_draw = menus moving)`; if it
loads: BACKGROUND bound as a primitive, the seven buttons by name, the
version line (fonts.zrd's debug_font, "%s %s" of message 7 and the version
number 0x004af290, rendered, placed at (640 - (width + 5), 480 - (height +
3)) and added), then either MECH3TEXT bound (menus moving) or, with FastMenu,
0x00486ed0 (the screen drawn once without the cursor and grabbed, the grab
made the static background and the still image, added to the list),
BACKGROUND taken out of the list and every button baked. Last: every
object's flags set to 0 when moving (drawn every frame) or 2 with FastMenu
(drawn only when dirty) (0x0059fcf0), active, [0x00609854] = the screen.

The destructor (0x00485780) releases the MENU set (0x0056f530), stops the
CD (0x00572c30) and clears [0x00609854].

Shipped MAINMENU: buttons at (20, 20) NEWCAMPAIGN, (92, 95) TRAINING, (20,
170) INSTANTACTION, (92, 245) MULTIPLAYER, (20, 320) OPTIONS, (360, 320)
CREDITS, (432, 395) QUIT, each `circlebtn_small` with a red label 47, 35 to
its right and the line `main_line1..7` at 59, 47; MECH3TEXT `main_mech3` at
(240, 240); BACKGROUND `common_background`. Each button but INSTANTACTION,
OPTIONS, CREDITS and QUIT also has DEMO_HELP ("Only Available In Full
Version"), a key the 1.2 exe never reads; dialog.zrd's DEMOEXITMENU (a
"Buy Now" screen) is never loaded either: both are the demo's.

## 3. Resolution (for the any-size work, fixes.md candidates)

The original assumes 640x480 in places and asks the display size in others:

- The version line is placed with the constants 640 and 480 (0x005c4b04,
  0x005c4b00), whatever the mode.
- The background's view and its wrap, and MECH3TEXT's wrap to the right
  edge, use the display size (0x0040d760 / 0x0040d770): at a larger size the
  background tiles and the title travels the whole width.
- Every position in MAINMENU is a 640x480 pixel position; the cursor, the
  buttons and their labels do not scale.

The port keeps all of it (the test runs sizes from 320x240 to 1600x1200).

## 4. The port

`mech3::MainMenu` and its parts are native zTwoD objects; the game's calls
outside zTwoD go through `mech3::Engine` (new hooks: fast_menu 0x0040cec0,
display_width / display_height 0x0040d760 / 0x0040d770, push_state
0x00590400, confirm_quit 0x00482a20; the rest existed), zTwoD's through
`ztwod::Host` (new: ripple 0x00558dd0). The Main state is unchanged: it
still makes its screen with `construct(0x486260)` and talks to it through
`vcall`, `screen_refresh` etc.; the program's `GameEngine`
(`src/mech3/app_menus.cpp`, our addition) answers those for ported screens:
the handle is a block of the module memory the size of the object, offset 0
the screen's vtable (+0x00 update, +0x04 active, +0x08 delete, +0x18 /
+0x1c busy), +0x111ec + 0x2a8 n a button (+0x30 press); `shell_clear`
(0x0048ee00, now `ShellState::clear_screen`) deletes it. The options' and
the credits' menu objects are ported (menus.md) and pushed; the other five
are not, so their handles are 0 and a press is logged as `0x00590400 (menu
state 0)`. `twod_set_cursor` (0x0059fb80) is zTwoD's
page-flip switch, now wired.

FIX (B): the background's 1 pixel, MECH3TEXT's 2 pixels and the ripple's 0.3
are steps a frame, so the menu moved faster the faster the machine drew.
With `zfix::options().per_second` (on by default) each is taken once for
every 30 fps frame that has passed (fractions carried over); the probe turns
it off (fixes.md).

## 5. Testing

`test_mainmenu.py` (60 runs): the original constructor and screen run in
the emulator on `twod_world.py`'s world, plus stubs for the options
(FastMenu off, on, or random per call; display sizes 320x240..1600x1200),
zVideo's buffers, the render target, the ripple, the version and message,
App::Push, the quit question, the sound set and music, the no-voice flag;
even runs the shipped MAINMENU, odd runs a synthetic one (RADIUS and
MAXRADIUS on some buttons, buttons and primitives missing, moved buttons, no
line, MECH3TEXT near the left edge; one in eight with no MAINMENU at all, so
nothing binds); in two runs of three the background and title images are
small, so both wrap within the run. The plan:
construct, usually the unload the Main state does, then 30 to 90 frames of
mouse input around the buttons (rollovers start ripples, clicks press) mixed
with presses through +0x30, the busy flag, baking and unbaking, bounds,
enabling, activation, dirty-all and restore; then a dump and the deleting
destructor. Compared: every call (the ripples' arguments included) and a
dump of the dialog, every object in its list and the menu's own state
(background pieces, title, every button's ripple, line and stash). Result:
60/60 identical (192,251 calls; 822 ripples drawn, 265 menu pushes, 39
quit questions). `tests/test_mainmenu.cpp` checks the FIX (B) path (one
second at 30, 60 and 144 fps) and the exe's.

Breaking the port showed the test catches it, 19 of 20: the background
wrapping a pixel late, piece 2 placed by the width, the corner piece's
source, the title wrapping only past 0, the ring's inner edge, the fade
step, the phase's sign, the ripple ignoring FastMenu, the hit test's right
edge, the centre by the full width, a busy screen taking the mouse, baking
that keeps the image, AUXBITMAP's offsets swapped, QUIT pressed like the
others, a disabled button pushing nothing, the version line at the display
size, managed flags while moving, the music's mode, baking with the cursor
shown. Missed, and unobservable: the radius's clamp at 0 (the frame time is
never negative).

## 6. Left

- The menus the buttons open are ported (menus.md): the options, the
  credits, the controls screen, and the Action, Campaign, Training and
  Multiplayer states with the pilot screen they open first (menus.md
  section 10); what follows a pilot (the Action screen, the multiplayer
  screens) is still a hook. They share MenuScreen, ShellButton,
  SlideButton, ScrollImage, the cycle button, the file list and the edit bar.
- The ripple's drawing (zRender 0x00558dd0) and the render overlay layer
  (0x005a4590..) a Ripple can also live in (its +0x04, 0x005a5360).
- The Main state itself still goes through `construct` / `vcall` (its test,
  test_app_states.py, compares those calls); it can call the screen
  directly once the menu states are ported.
