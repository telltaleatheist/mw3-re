# zTwoD: the 2D front end

Mech3.exe 1.2, 2026-10-20. The engine's 2D objects (`C:\zipper\GameZMech3\
zTwoD\twod_*.cpp`): every menu, dialog, button, text, the software mouse
cursor and the message box. The game's screens (main menu, pilots, mech lab,
briefing, debriefing, options; `mech3\*`) are subclasses that bind their
widgets to a `.zrd` definition in `reader.zbd`. Ported core in
`src/gamez/ztwod/` and `src/gamez/zrender/zrndr_blit.*`; checked against the
original by `tests/differential/test_twod.py` and `test_twod_blit.py`.

Exploration scripts: `tests/differential/explore/twod_map.py` (functions of a
range with size and callers), `twod_vtables.py` (the classes' vtables, slots
that differ from the base marked), `asm.py` (one function's disassembly),
`strs_in.py`, `imports_in.py`.

## 1. Where it is

Only two source names survive in the exe's error reports: `twod_dialog.cpp`
(0x0059f810 BindButton, 0x0059f8d0 BindPrimitive) and `twod_wintext.cpp`
(0x005a0200..0x005a0710, the GDI-font text). The rest is placed by link order:

| Range | What |
|---|---|
| 0x0059b2f0..0x0059b7e0 | 3D line clipping against near/far for the HUD's line primitive (globals 0x005f4800..) |
| 0x0059b830..0x0059bb70 | Image |
| 0x0059bbe0..0x0059cdc0 | Control (push button) and its readers (labels 0x0059cb30, flash 0x0059ca90) |
| 0x0059d870..0x0059dc40 | CheckButton |
| 0x0059e0f0..0x0059e350 | Slider (FILLBITMAP) |
| 0x0059e430..0x0059ec20 | ListBox |
| 0x0059ec40..0x0059fab0 | Dialog (twod_dialog.cpp) |
| 0x0059fad0..0x005a0090 | Layer and the base Object2D |
| 0x005a00c0..0x005a0f00 | FontText (twod_wintext.cpp) |
| 0x005a1040..0x005a1790 | Text (with flash) |
| 0x005a18f0..0x005a2ae0 | TextBox (several lines of Text), Text copies |
| 0x005a2b50..0x005a2bb0 | Window (mouse routing) |
| 0x005a2da0..0x005a3080 | TextBase (bitmap-font text) |
| 0x005a3170..0x005a3bc0 | HUD primitives: polylines, rectangles, filled boxes |
| 0x005a3cd0..0x005a4880 | the text console layer (`Arial`), the interp debug console (0x005a40b0, interp.md) and its line editor |
| 0x005a4590..0x005a5440 | the render overlay layer (0x005a6790 / 0x005a67c0 from the draw paths) |
| 0x005a5470..0x005a6110 | weather as 2D: SnowFX base, snow 0x005a5ad0, rain 0x005a60a0 (world-params.md) |
| 0x005a67e0..0x005a6d10 | HUD lines (ENDP_REL / ENDP_ABS, the clamp of protection.md) and small HUD objects; the key-list screen's pieces (0x00482570) |
| 0x005a6e60..0x005a7730 | MessageBox, its modal loop 0x005a7288 and the MB_* buttons |
| 0x005a7750..0x005a7a50 | Cursor |
| 0x005a7a90..0x005a7d10 | Movie2D (a zFMV stream in a dialog) |
| 0x005a7e10.. | zEntity (not 2D; structs.md) |

The small virtuals at 0x00409a10..0x0040a8a0 (between briefing.cpp and
mech.cpp) and 0x00482870 / 0x0048c050 are zTwoD's too: the compiler emitted
them in the first file that used them and folded identical bodies, so one
address serves many classes (0x00409a80 is "return true" for any 2-argument
virtual; 0x0049bfd0 is every empty slot).

## 2. Classes

No RTTI; names are ours. Sizes from the `new` calls and the array
constructors (0x005ac2a0); vtables by `twod_vtables.py`.

| Class | Constructor | Vtable | Size | Base | Ported |
|---|---|---|---|---|---|
| Object2D | 0x0059fda0 | 0x005bc0e0 | 0x34 | | yes |
| Layer | 0x0059fba0 | 0x005cfd10 | 0x10 | | yes |
| Window | 0x005a2b50 | 0x005cfed0 | 0x58 | Layer | yes |
| Image | 0x0059b970 (0x0059b830, 0x0059b8e0) | 0x005cf8c8 | 0xbc | Object2D | yes |
| Control | 0x0059bbe0 | 0x005cf948 | 0x14c | Image | yes |
| CheckButton | 0x0059d870 | 0x005cfaa8 | 0x164 | Control | yes |
| Slider | 0x0059e0f0 | 0x005cfb38 | 0x188 | Control | yes |
| ListBox | 0x0059e430 | 0x005be308 | 0x650 | CheckButton | yes |
| ListItem | 0x0059e9a0 | 0x005cfbc8 | 0x4b8 | FontText | yes |
| TextBase | 0x005a2da0 | 0x005cfed8 | 0x248 | Object2D | yes (the zFont calls hooked) |
| FontText | 0x005a00c0 | 0x005cfd28 | 0x4b0 | TextBase | yes |
| Text | 0x005a15a0 | 0x005cf9d8 | 0x4cc | FontText | yes |
| TextBox | 0x005a18f0 | 0x005cfe00 | 0x4c8 | FontText | yes |
| Cursor | 0x005a7750 | 0x005d0da0 | 0xd0 | Image | yes |
| Movie2D | 0x005a7a90 | 0x005d0e28 | 0x144 | Object2D | constructor and destructor; open, colour key, start hooked |
| Dialog | 0x0059ec40 | 0x005cfc78 (+0x58: 0x005cfc88) | 0x10fec | Window, Cursor at +0x58 | yes |
| MessageBox | 0x005a6e60 | 0x005d08f0 | 0x12494 | Dialog | yes |
| MbButton (8 vtables) | 0x0059bbe0 | 0x005d0d10..0x005d0920 | 0x14c | Control | yes |
| PolyLine | 0x005a3830 | 0x005cfff0 | >= 0xec | Object2D | no |
| Shape (transformed polyline) | 0x005a3170 | 0x005cff58 | | PolyLine | no |
| Frame / FilledBox | 0x005a39f0, 0x005a3a60 / 0x005a3b60 | 0x005d0070 / 0x005d00f0 | | Object2D | no |
| Render overlay | 0x005a4590 | 0x005d0288 (+0x28: 0x005d0290) | | Layer | no |
| Ripple | 0x005a53e0 | 0x005d04a8 | 0x4c | Object2D | yes (its zRender draw 0x00558dd0 hooked; mainmenu.md) |
| SnowFX, Snow, Rain | 0x005a5470, 0x005a5ad0, 0x005a60a0 | 0x005d0528, 0x005d05e0, 0x005d0660 | | Object2D | no |

Dialog is a Window with a Cursor inside it (the second vtable is the cursor
part's, differing only in its deleting destructor). A Dialog is itself the
list its objects live in.

### The object (Object2D)

| Offset | Field |
|---|---|
| +0x04 | next in the owner's list |
| +0x08 | owner (the Layer) |
| +0x0c | flags: 0x01 timed (hides itself when the timer runs out), 0x02 managed (drawn only when dirty; without it, every frame), 0x04 / 0x08 dirty for this buffer / the other one when page flipping, 0x10 hidden |
| +0x10 | the timer, float seconds |
| +0x14, +0x18 | x, y |
| +0x1c | the background image erase() blits back |
| +0x20 | the part of it (a RECT) |
| +0x30 | mouse bits: 1 hover, 2 pressed |

The vtable, slot by slot (the release build's empty handlers are 0x0049bfd0):
+0x00 destructor, +0x04 draw, +0x08 erase, +0x0c set_pos, +0x10/+0x14
set_x/set_y, +0x18 set_background(image, rect), +0x1c set_bg_rect, +0x20
dirty, +0x24 update(dt), +0x28 frame, +0x2c get_rect, +0x30 left released
over it, +0x34 right released over it, +0x38 hover, +0x3c enter, +0x40 leave,
+0x44 press, +0x48 release, +0x4c drag(x, y), +0x50 click, +0x54
accepts(window, inside), +0x58 after_input(window, inside), +0x5c hit(x, y),
+0x60 show_for(seconds; negative: for good), +0x64 hide, +0x68/+0x6c
get_x/get_y, +0x70 set_fixed_rect, +0x74 rect_at, +0x78 read(node, dialog).
Image adds +0x7c; Control +0x80 refresh, +0x84 read_control(node, dialog),
+0x88 after_bind; CheckButton +0x8c set_checked; FontText +0x80..+0xac
(measure, the font calls, the text setters, render).

### Drawing by dirty rectangles

Nothing clears the screen between menu frames. Each object draws itself over
the last frame: update (0x0059ffb0) draws a managed object once when dirty
(and once more for the other buffer: dirty() sets 4, or 0xc when page
flipping, 0x005f49b8 by 0x0059fb80) and erases a hidden one the same way;
draw first erases, by blitting its part of the dialog's static background
back. That background is a screen grab taken right after the dialog drew its
static parts once at load (below), so every button only ever redraws its own
rectangle. Text is the exception: its update (0x005a1790) has no dirty flags
and redraws every frame while shown.

### The mouse (Window::update, 0x005a2bb0)

Each update copies zInput's mouse (0x0063d1d8 via 0x0051b7b0: x, y, ...,
+0x30 left, +0x34 right, +0x38 middle; button states 1 pressed this frame,
2 held, 4 released, 0x0051b6e0) and walks the list. For each object that
accepts (all do): a right-button release clears its pressed bit (release);
inside it: enter or hover, press when the left state matches the press mask
(1: pressed this frame), left-up, right-up, drag while held, click on a
left release while pressed; outside: release by the release mask, drag
while pressed, leave. Then the cursor erases, the layer updates (draws),
the cursor moves to the mouse and draws.

Push buttons (Control) act on +0x30, the left button's release over them
(0x0059c960): input reset (0x0051b1c0), the ACTIVATE image and sound, the
activate labels. Enter (0x0059c650) and leave (0x0059c8b0) swap in the
ROLLOVER image, sound and labels. The game's buttons override click/left-up
to do their work.

## 3. Dialogs

### The definition

A dialog is an entry of a `.zrd` (dialog.zrd: MAINMENU, DEMOEXITMENU,
PILOTDIALOG, ACTIONDIALOG, the three MULTIPLAYER dialogs; briefing.zrd:
BRIEFINGDIALOG, WAYEDITDIALOG; dlgPause, dlgWeapon, dlgMechLab,
dlgMechLabII, dlgSalvage, dlgWin, dlgMessage (MESSAGEBOX), dlgOptions (five),
dlgCredits). The keys the loader (0x0059f080) reads, in order:

| Key | Value | Where it goes |
|---|---|---|
| SHARED_IMAGE_PATH (in the file) | ("dir;dir") | zImage's search path (0x00516f50) |
| IMAGE_PATH | ("dir;dir") | the same |
| FONTS | ((id "face" height colour [shadow [weight [align]]]) ...) | the font table, 20 entries of 0x24 (+0x1d38); colour (r g b), or ((r g b) (r g b)) for text on an opaque background; align "LEFT", "RIGHT", "CENTER" (anything else keeps the previous); weight 500 unless given |
| BACKGROUND_IMAGES | (("name" x y) ...), up to 19 | the image array (+0x138) |
| BACKGROUND_VIDEOS | (("name" x y [(r g b)]) ...), up to 9 | Movie2D array (+0xfe8), with a colour key |
| BACKGROUND_TEXT | (("MSG_KEY" x y font) ...), up to 49 | Text array (+0x2008) |
| BACKGROUND_SOUNDS | (("name" [volume]) ...), up to 9 | a CD track when the name is one (0x00572a30), else a sound (0x0056f6f0) |
| CURSOR | (BITMAP ("name") CENTER (n) CAPTURE (n)) | the cursor at +0x58 |
| BUTTONS, PRIMITIVES | name, entry pairs | read when the screen binds a widget by name |

Shipped dialogs use only FONTS, BACKGROUND_IMAGES, CURSOR, BUTTONS and
PRIMITIVES: the background movies, texts and sounds are supported but unused.

Positions are relative to the dialog origin (+0x10fe0, +0x10fe4; 0 in every
screen seen so far).

### Loading (0x0059f010 -> 0x0059f050 -> 0x0059f080)

`load(file, name, no_draw)` reads the file (`zrdr::load`, kept at +0x128 and
freed by 0x0059fa10 when the name is missing), finds the entry and loads it:
under the video lock, unless no_draw it grabs the screen (+0x130, "behind"),
reads the keys above, then draws the static parts once: active, one update
with dt 0 and the sounds muted, a second grab (+0x134, the static background
every widget erases with), inactive, and the "behind" grab blitted back.
Then the cursor.

### Binding

The screen constructs its widgets and binds each by name:
`bind_button(control, name)` (0x0059f8b0 -> 0x0059f810, the BUTTONS entry)
calls the control's read_control (+0x84) and after_bind (+0x88);
`bind_primitive(object, name)` (0x0059f8d0, the PRIMITIVES entry) adds it,
calls its read (+0x78), applies MOUSETRAP (l t r b) by writing it over the
four dwords at +0x34 of whatever object it is (no shipped dialog has one),
and gives it the static background with an empty rectangle.

A BUTTONS entry (Control 0x0059cdc0): POSITION, BITMAP ("name" ["offset" (x
y)]), LABEL, FLASH, and ROLLOVER, DISABLE, ACTIVATE, each with BITMAP, SOUND
("name" [volume]), LABEL and FLASH. A LABEL (0x0059cb30) is ("MSG_KEY" [x y
| "offset" (x y)] ["font" ("name") | font-number]) or a list of them; each
becomes a Text at the button's base plus its offset, with a fonts.zrd font by
name or the dialog's font by number. FLASH is (RATE (seconds) COLOR (r g b)):
its labels swap to that colour every half period. CheckButton (0x0059dc40)
adds CHECKED, DISABLE_UNSEL and DISABLE_SEL with BITMAP and TEXT.

A PRIMITIVES entry: an Image reads BITMAP, CENTER, POSITION (0x0059ba40); a
Text reads TEXT (a message key), WORDWRAP (w h), the font by name ("font" or
"FONT" string) or number (FONT n), POSITION (0x005a1400).

Messages are looked up by key (0x00575fb0, Mech3Msg.dll's table) and used as
the text's printf format.

## 4. Text

Three layers:

- **TextBase** (0x005a2da0): 0x200 characters at +0x34 drawn with a zFont
  bitmap font (fonts.zrd; measure 0x0051a0d0, draw 0x005ae170), optionally
  centred between +0x23c and +0x240. The HUD's text.
- **FontText** (twod_wintext.cpp): a Windows font (`CreateFontA`, or a
  fonts.zrd font by name, 0x00517680, whose GDI font zImage makes) rendered
  by GDI into a 16-bit image (+0x248), blitted. render (0x005a0790): the
  text's box by `DrawTextA(DT_CALCRECT)` (or the fixed WORDWRAP box, drawn
  with DT_WORDBREAK), grown by the shadow offset and cut to +0x4a8; the image
  reused when big enough, else a new one; cleared; the shadow drawn in
  +0x254 (0x141414) offset by (+0x4a0, +0x4a4); the text in its colour, or
  white when top and bottom colours differ, after which every white pixel of
  each row is replaced by the blend of the two for that row's height within
  its line (`GetTextMetricsA`; 0x005a0d20, channels truncated). Background
  mode +0x46c: 1 transparent, 2 opaque in +0x470. Alignment +0x244 (0, 1
  centre, 2 right) shifts the blit left by half or all of the text width.
- **Text** (0x005a15a0): a FontText with a flash (+0x4b0..+0x4c8): mode 2
  swaps the colours with the flash colour every half period (set by FLASH,
  0x005a16e0); mode 1 would blink, but nothing in the exe sets it.
- **TextBox** (0x005a18f0): lines of Text (an HP STL vector by value at
  +0x4b8) under a FontText of its own whose text is "W", to measure the line
  height; the briefing's and the mech lab's text panels and the interp
  console. Each write (+0x7c, 0x005a1d90) goes to the next line; past the
  last, every line takes the text of the one below (0x005a1e80). Lines sit
  one height apart, downward, or upward from the current one when +0x4b4 is
  set. Copying a Text copies its Windows font (`GetObjectA` +
  `CreateFontIndirectA`, 0x005a0200), so the vector's growth (0x005a2700:
  size + max(size, n)) shows in the GDI calls and is kept as written; the
  assignment (0x005a03f0) drops the target's image and font without freeing
  them.

FIX (A): the GDI calls (fonts, DCs, `DrawTextA`, `GetTextExtentPoint32A`,
`GetTextMetricsA`, colours, modes) go through `ztwod::Host` one for one; a
front end implements them with a TrueType rasteriser over the shipped
arial/impact/lucon.ttf into the same 16-bit image ("Lucida Console" is
lucon.ttf). The render mutex (0x00731ab8, for the movie thread) is left out.

## 5. Drawing

All 2D drawing is one call, 0x0055b1f0(image, x, y, key, rect): with a
DirectDraw surface on the image and the locked buffer as the target, a
DirectDraw Blt (0x00576f90); otherwise the 16-bit software blit [0x00800824]
= 0x0055b3c0 (set at 0x00557a8b; 0x0055b250 is the 8-bit one) into the
locked buffer (0x008007c8 base, 0x008007cc width, 0x008007d0 height,
0x008007d4 pitch). The software blit (ported, `zrndr::blit16`) clips, then
copies 16-bit or palettised pixels: with an alpha plane each pixel blends by
its alpha (0..3, or 0..7 on a 555 display, skipped; 0xfc and up copied; in
between a fixed-point blend per channel), else with image flag 0x02 the key
colour (or index; the callers pass 0, black) is transparent, else a plain
copy (one dword copy when the image spans the whole width). The menus draw in
the software path in both renderers: the shell (0x0048eb10) locks the video,
updates its screen, unlocks and flips (game-loop.md). The Direct3D 2D quad
0x0057bd50 is the HUD's, not the menus'.

FIX (A): images carry no DirectDraw surface in the port, so 0x0055b1f0 always
takes the software path; the platform presents the 16-bit buffer.

Quirks of the blit, kept: an image whose total pixel count is odd loses its
last pixel in the whole-width copy; the green channel of the 565 blend rounds
a fall to an odd number of units (bit 5 is set from the sign). FIX (bug): a
bottom edge exactly one row past the buffer was not clipped (the clip test is
`<=`), writing a row beyond the buffer; and a zero-width source rectangle on
a palettised image made the palette loops count from 0 past 2^32 and fault.
The port clips the row and draws nothing for zero width (fixes.md).

### The cursor (0x005a7750)

An Image that saves the screen under it: on each move (0x005a7940) it grabs
the rectangle under its new place (0x00579660, clipped to the screen) into
its own buffer (+0xbc) and makes that its background, so erase restores it;
draw and erase clamp a negative position to 0. CAPTURE 0 turns the saving
off. The dialog's cursor is drawn last by Window::update.

### Sliders and list boxes

A Slider (0x0059e0f0) is a button with FILLBITMAP ("name" [x y]) over its
own image: its value 0..1 (+0x14c) cuts the fill at that share of its width
and the button's image shows the rest (0x0059e350; as tall as the fill
image). The FILLBITMAP's x y move the button. The zTwoD class stores a
value raw (+0x8c 0x0059fad0); the game's subclasses take it from a click
(0x0059e2f0: the mouse's distance from the left edge over the width) and
clamp it.

A ListBox (0x0059e430, the options' key lists) is a check button with the
chosen string shown at its POSITION (a ListItem at +0x164) and LISTSIZE (n
[above]) entries around it: `above` stacked upward by SPACING from LIST_OFFSET's
first pair, the rest downward from its second (0x0059e7d0). The game fills
its strings (a vector of CString pointers at +0x628, freed by the box);
select(i) (+0x90, 0x0059ea30) shows string i and the ones around it, hiding
entries past either end; clicking an entry selects it. SELECTED_FONT is a
dialog font number or a fonts.zrd name, LIST_FONT a name, LISTWIDTH the
entries' width cut, ACTIVE_RECT the box's bounds. SPACING is read as the
node's raw value: a bare number in the shipped files; a list there would
give a pointer as the spacing.

### The message box (0x005a6e60)

A Dialog (dlgMessage.zrd MESSAGEBOX) with a TITLE and MESSAGE FontText and
eight buttons MB_OK, MB_CANCEL, MB_YES, MB_NO, MB_ABORT, MB_RETRY, MB_IGNORE,
MB_USERBTN (Control subclasses 0x005d0920..0x005d0d10 whose left-up stores
their result and ends the loop). Its modal loop (0x005a7288): saves the
cursor and the render target, shows the buttons for the style (low 4 bits:
OK; OK/Cancel; Abort/Retry/Ignore; Yes/No/Cancel; Yes/No; Retry/Cancel),
then runs its own frames (input poll, clock, lock, update with the frame
time, unlock, flip) for up to 100,000 frames until a button answers.

The shipped dlgMessage.zrd defines only MB_OK, MB_CANCEL and MB_USERBTN; the
other five buttons bind nothing (no image, no label), so they cannot be
clicked. Every message box the game opens is OK or OK/Cancel (style 0, 0x30
with the exclamation icon, or 1), except the briefing's waypoint editor
(0x00404a46, "Teleport: Delete Location %1?", MB_YESNO): its box shows no
button that can be pressed, and after 100,000 frames it answers 0, not Yes.

The game opens a box through 0x004ac150 (style, caption id, format id, ...):
the text formatted by FormatMessage (0x00575ff0) into 0x0060a8e8, the box
built and run, FastMenu set around it (0x0040ce90), destroyed.

## 6. The port

`src/gamez/ztwod/`:

- `twod.h`, `twod_object.cpp`: Object2D (0x0059fda0, 0x0059fdf0, 0x0059fe50,
  0x00409af0..0x0040a8a0, 0x0059ff00..0x005a0090, 0x0059feb0), Layer
  (0x0059fba0..0x0059fd20, 0x0059fa60), Window (0x005a2b50..0x005a2bb0),
  Image (0x0059b830..0x0059bb70, 0x00409cb0, 0x00409cf0, 0x00409d30), Cursor
  (0x005a7750..0x005a7a50), Movie2D (0x005a7a90, 0x005a7ae0), page flip
  0x0059fb80, Ripple (0x005a53e0, 0x005a5440).
- `twod_control.cpp`: Control (0x0059bbe0..0x0059c960, read 0x0059cdc0),
  CheckButton (0x0059d870..0x0059dc40).
- `twod_wintext.cpp`: TextBase (0x005a2da0, 0x005a2f40..0x005a3080,
  0x005a30f0), FontText (0x005a00c0, 0x005a05a0..0x005a0790, 0x005a0d20,
  0x005a0f00, 0x005a1040..0x005a1400, 0x00409b30, 0x00409c80), Text
  (0x005a15a0, 0x005a1690..0x005a1790).
- `twod_dialog.cpp`: Dialog (0x0059ec40..0x0059fa40), name_and_offset
  0x0059cd90, read_flash 0x0059ca90, read_labels 0x0059cb30.
- `twod_list.cpp`: Slider (0x0059e0f0..0x0059e350, 0x0059fad0), ListBox
  (0x0059e430..0x0059ea30, its destructor 0x0042f480), ListItem
  (0x0059e9a0, 0x0059faf0, 0x0048e540, 0x0059ec20).
- `twod_textbox.cpp`: the text copies (0x005a0200, 0x005a03f0, 0x005a2a70,
  0x005a2ae0, 0x005a2e30, 0x005a2ed0), the vector (0x005a2700, 0x004097e0)
  and TextBox (0x005a18f0..0x005a2640, 0x00406020).
- `twod_msgbox.cpp`: MessageBox (0x005a6e60, 0x005a7150, the loop
  0x005a7280, the answers 0x005a7560..0x005a7640) and its buttons
  (0x005a7650..0x005a7730).
- `twod_host.h/.cpp`: `ztwod::Host`, every call outside: zImage's image cache
  (0x005179d0 / 0x00517b10, a refcounted list over `zimg::fetch_image`) and
  path, fonts.zrd fonts (0x00517680, 0x005172e0, 0x00517300), zFont
  (0x0051a0d0, 0x005ae170), zVideo's lock, unlock and grabs (0x00576de0,
  0x00576e70, 0x00579500, 0x00579660) and the image DC ([0x0072f394],
  [0x0072f398]), zSound (0x0056f6f0, 0x0056e3d0, 0x0056e9f0, 0x00572a30,
  0x00572af0, 0x00572c30), messages (0x00575fb0), zInput (0x0051b7b0,
  0x0051b1c0) and the GDI imports. The blit (0x0055b1f0) defaults to the port
  (`zrndr::blit16`), the texture free to `zimg::free_texture`; the rest
  report as unported until a front end implements them.

`src/gamez/zrender/zrndr_blit.*`: the 16-bit software blit 0x0055b3c0 and
its target. The message box's loop adds Host calls for zVideo and zRender
(the video mode, the double-size switch [0x0072f358], the cursor 0x00579840,
the screen rectangle 0x00579880, the render target 0x00558080 / 0x00557f90
and the buffers behind it, input poll 0x0051b360, the clock 0x00575a00, the
frame time, the flip 0x00576ea0). The main menu's buttons add zRender's
ripple 0x00558dd0 (mainmenu.md).

Objects keep the original fields and offsets (in comments) as native C++
members and the vtables as virtual methods in slot order, so the game's
screens can subclass them as the original did. The original's raw quirks
that C++ cannot express are noted where they are: MOUSETRAP's write into
+0x34 (a `set_trap_rect` the base ignores), the label array 0x007319b0
(bounded at 64 here), FontText's constructor's message-as-format (`%s`).

## 7. Testing

`tests/differential/twod_world.py` builds the emulated world: every call out
of zTwoD stubbed under the Host's keys with the same arguments; answers from
a seeded script (images sized by their name's hash, some missing; fonts.zrd
fonts as fake objects; messages "T:<key>", some missing; sounds; CD tracks
for names starting "cd"); a fake GDI on both sides (7 pixels a character,
13 + length mod 3 lines, text metrics 13/11/2/1, and DrawTextA painting the
text colour on two pixels of three into the image's DC, so the gradient's
tinting runs); the 565 format for 0x00579370; 24-bit x87.

`test_twod.py`: for each of the 22 shipped dialogs and 10 synthetic ones
(every loader key, shipped or not: two-colour fonts, background texts,
sounds and CD tracks, labels by number, by offset and nested, flashes, check
buttons with all their texts, wrapped text primitives, a capturing cursor),
three seeds: construct, load (one seed in three with no_draw), bind every
button (CheckButton when it has CHECKED) and primitive (Text when it has
TEXT), every button's bounds, then a random plan of activation, mouse frames
over the buttons (press, hold, release, right button, moving off), enable /
disable, hide / show for a time, check, text changes, flashes and measure.
Compared: every call out and a dump of the dialog (fonts, sounds, the mouse
it saw, its grabs) and of every object in its list (flags, timer, position,
background, images, sounds, bounds, labels by index, text, render state,
image size and a hash of its pixels, flash). Result: 96/96 runs identical
(66 shipped, 30 synthetic; 122,468 calls).

The plans bind LISTSIZE entries as list boxes, FILLBITMAP entries as
sliders and some TEXT primitives as text boxes of 1 to 6 lines (written past
their end, scrolled, grown, turned bottom-up, measured), (CTL_CFG_DIALOG's five key lists among them, synthetic ones with
every key) and drive them: values set and taken from clicks, strings added,
selections.

`test_twod_msgbox.py`: 90 message boxes, half the shipped one (the styles it
can answer) and half a synthetic one with all eight buttons (every style),
in both renderers, with and without the backdrop grab, the mouse wandering
then releasing over a shown button: construction, the loop and the
destructor identical in calls, answers (all seven), the page-flip state and
the frame count after (50,186 calls).

`test_twod_blit.py`: 3,000 random blits (direct and palettised; alpha in 565
and 555, keyed, opaque, whole-width; off screen, past every edge; whole or by
a rectangle; odd target widths, padded pitches): every target identical, the
two FIX (bug)s apart (the original wrote its extra row in 136 of them; 43
zero-width palettised blits, which run away in the original, draw nothing).

Breaking the port showed the tests catch it, 41 of 45: hover never entered,
centring rounded up, centred label bounds by the full width, a label's
number offset ignored, the shadow's sign, the gradient blend from the
middle, CENTER read as RIGHT, the check button's second rollover sound
dropped, the other buffer never cleared, the rollover bitmap not moving the
button, the CD in mode 6, measure without the shadow, the cursor's
background not relative, the disabled look keeping the normal image, flash
colour channels swapped; in the blit: the 565 green sign bit, 555's
thresholds, the whole-width copy keeping the odd pixel, the palette key
compared to the colour, the right clip, a rectangle's top ignored, 555's
blue; sliders cut by the height, the fill offset ignored, the empty part's
height; list boxes' entries below off by one, the offset pairs swapped, the
upward spacing, LISTWIDTH, an entry keeping its old index; the message box's
No answering Yes, no backdrop, the Direct3D target from the software
buffers, the page flipping restored on; the text box measuring an empty
text, its lines shifting the wrong way, clearing that keeps the current
line, bottom-up lines placed downward, copies sharing the source's font, the
vector growing by n only, scrolling down past a hidden line. Missed, and
unobservable: the text assignment keeping needs_render (zTwoD only ever
inserts at the end, so it never assigns), drawing
the blink mode's off half (nothing sets mode 1), the Yes/No/Cancel buttons
shown in another order (showing has no outside effect), and the box's loop
counting one frame less (it differs only at the 100,000-frame timeout).

## 8. Quirks kept

- A check button plays its rollover sound twice on enter (0x0059db00 plays
  it, then Control's enter plays it again).
- A ROLLOVER BITMAP moves the button at load: its position (where the
  images draw; the labels keep the base) becomes the base plus the rollover's
  offset, replacing the normal bitmap's. dlgMechLabII's ARMOR_REMOVE has no
  normal image (`BITMAP ()`) and relies on it: its rollover arrow lands 190
  pixels right of its labels; the options' key buttons give the same offset
  twice.
- DISABLE_UNSEL's TEXT replaces CHECKED's (the first stays in the dialog,
  leaked), and its LABEL search reads the button's own LABEL again, making
  a second, hidden copy of the normal labels as the disabled ones.
- Control's constructor leaves the DISABLE sound fields and the bounds
  uninitialised (zero in the port); the cursor's buffer index is set after
  its constructor may already grab (never, the dialog passes no name).
- Leaving a button (0x0059c8b0) forgets the rollover sound's handle without
  stopping it; releasing the left button over a pushed button stops it.
- A Text's timer hides it, then the same update still draws it once.
- BACKGROUND_TEXT's font number is read whether or not the entry has one.
- Messages are printf formats: a `%` in a message would be read as one.
- Shrinking a text box's line count (0x005a2320) leaves its current line one
  past the last (FIX (bug) in the port: such a line counts as hidden); the
  copies of a Text leave the fields their constructor never set (the
  background colour of a transparent text) as the stack held them.
- Dialog's constructor looks up "VMode" (0x00597d10) and drops the result.

## 9. What is left

Mapped, not ported:

- The LOGFONT and message-id setters 0x005961d0, 0x00409bd0,
  0x00409bf0, 0x00409c40 and the text box's 0x005a1cd0 / 0x005a1cf0 (the
  message table's FormatMessage, 0x00575ff0), the box's text thunk
  0x005a22c0, the plain DrawTextA measure 0x005a0e20.
- The game's message box call 0x004ac150 / 0x004ac050 (mech3).
- Movie2D's stream: open 0x005a7b50 (0x00575d40, zFMV 0x0059a4a0), colour key
  0x005a7c30, update 0x005a7c50, draw / erase 0x005a7ca0 / 0x005a7cd0, start
  0x005a7d10 (zFMV is not ported).
- The HUD side: primitives 0x005a3170..0x005a3bc0 (zRender's 2D lines
  0x00562940 / 0x00562cb0, boxes 0x005630c0 / 0x00563440), the line clip
  helpers 0x0059b2f0..0x0059b7e0, HUD lines 0x005a67e0..0x005a6b30, the
  render overlay 0x005a4590.. (the Ripple's +0x04 0x005a5360 draws in it)
  and zRender's ripple 0x00558dd0 (Host::ripple), the text console
  0x005a3cd0.., the interp debug console 0x005a40b0..0x005a4880, the
  key-list screen pieces 0x005a6bd0..0x005a6d10.
- Weather drawing 0x005a5470..0x005a6110 (world-params.md).
- Outside zTwoD, behind Host: zImage's image cache 0x005179d0 / 0x00517b10
  (a std::list of {strdup name, image, count}; release frees images it does
  not hold unless they are the default image, 0x005185f0) and named fonts
  (zimg_winfonts.cpp 0x00517280..0x00517680), zFont, zVideo's lock and grabs
  (0x00576de0, 0x00579500, 0x00579660), the DirectDraw path 0x00576f90 and
  the 8-bit blit 0x0055b250, the message lookup 0x00575fb0.
- The front end's `ztwod::Host` on SDL3 is done (`SdlTwodHost`,
  program.md: TTF text into the 16-bit image, the 16-bit target presented,
  mouse from SDL; `mw3 --dialog dialog.zrd MAINMENU` shows a dialog).
- The game's screens on the port's classes: the main menu is done
  (mainmenu.md: `mech3/menu_widgets` has the menu screens' base 0x0040a110,
  the scrolling background, the game's buttons 0x0040a5d0 and 0x00438d70
  that the other menus share; `mech3/mainmenu` the screen 0x00486260), and
  so are the credits and the game and graphics options (menus.md:
  `mech3/credits`, `mech3/options_menu`, and in `menu_widgets` the cycle
  button 0x0048b280 built of three Controls). Left: the controls screen,
  Instant Action, campaign, training and multiplayer, the pause, briefing,
  debriefing, salvage and mech lab screens, each still a `mech3::Engine`
  hook.
