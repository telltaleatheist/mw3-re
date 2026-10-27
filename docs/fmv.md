# zFMV: the movie scripts

`C:\zipper\GameZMech3\zFMV\fmv_script.cpp` (0x00598cc0..0x0059a530) runs the
named scripts of `fmv.zrd` (in `reader.zbd`): a list of actions played one
after another, the images, waits, fades, blurs and sounds around the AVI
movies. `fmv_stream.cpp` (0x0059a650..) is the AVI player (Video for Windows
and Indeo 5), `fmv_main.cpp` (0x0059b040..) the MCI movie. Ported:
`src/gamez/zfmv/fmv_script.{h,cpp}`, the blur passes in
`src/gamez/zrender/zrndr_blur.{h,cpp}`, and the program's side in
`src/mech3/app_fmv.cpp`. PLAYAVI is a stub until the stream is ported
(frontend-plan.md item 11): it ends at once, as the original's does when the
file is not there.

## 1. Who plays what

| Caller | Block | When |
|---|---|---|
| Intro state 0x00482070 / 0x00482170 | `INTRO` | at start, with movies on (not `-nofmv`, not the lobby flow) |
| Intro state 0x00482170 | `SPLASH` | after it, always (not the lobby flow) |
| Attract state 0x00482330 | `ATTRACT` | 60 idle seconds in the main menu (0x00486d40) |
| Pilot menu 0x0048b0b6 | `campaign` | a new campaign's first pilot |
| Load state 0x004099da (0x00409320) | `c<op>m<n>` | before a campaign mission's briefing, when the briefing has one (0x0040a820) |
| Win movie state 0x0047a9b0 | `c4win` | the campaign's end (`c1win`..`c3win` are never played) |

Each makes the script (new(0x20), 0x00598cc0(0, 0, 0)), gives it the main
window (+4), loads the block (0x00598d80 with "fmv.zrd") and, when that is
not -1, plays it (0x005996d0); then polls 0x00599710 each frame until it is
false, rewinds it (0x00599750(0): back to the first action, the current one
not ended) and deletes it (0x00598d00).

`UNLOAD` (the mech3unload image), `DEMO_INTRO` (three logos faded in, blurred
and faded out to a sound, then the splash), `DEMO_EXIT` (the splash and the
`demoexit` image at 110, 260) and `DEMO_SPLASH` are left from the demo: no
code in 1.2 names them. Nor does any block use BLURH, BLURV or PLAYMCI.

## 2. The script and its load

The object: +0 FMV_PATH (strdup'd), +4 the window (0: [0x00608f80], the main
one), +8 the start time (a double), +0x10 the first action, +0x14 the last,
+0x18 the current one.

0x00598d80 (ECX script; file, block): zReader's tree of the file (0x00556740;
none: -1, after the engine's empty report "Failed to find FMV definitions
(fmv.zrd)"), FMV_PATH strdup'd to +0, IMAGE_PATH given to zImage's
directories (0x00516f50), then the block found (0x005569e0; none: 0, the
tree leaked). Each entry must be a list (else the read stops and the load
answers 0: "Error in parsing fmv actions: file=%s, tag=%s", also empty); its
first item is the verb, compared in this order, case-sensitive:

| Verb | Items | Action |
|---|---|---|
| SHOWIMAGE | name [x y] | image, shown at once; at x, y with both, else stretched over the screen |
| BLITIMAGE | name x y | image, shown at once, at x, y (always reads both) |
| LOADIMAGE | name [x y] | image, drawn but not shown |
| WAIT | seconds | wait |
| FADEIN, FADEOUT | (r g b) seconds alpha | fade |
| PLAYAVI | name [flag] | movie `<FMV_PATH>\<name>`; the flag (default 1) goes to the stream |
| PLAYMCI | name | MCI movie (the window, the path) |
| BLUR, BLURH, BLURV | passes | one blur step of `passes` passes, both ways, along the rows, down the columns |
| PLAYSOUND | name | sound (the name strncpy'd into 0x32 bytes) |

An unknown verb is skipped. The answer is the number of entries. Values are
read as the words they are (an x given as 1.5 is its float's bits). FIX
(bug): a short entry (BLITIMAGE without x and y, a FADE without its values)
read past its list in the original; the port reads 0 there.

## 3. The run

0x005995b0 (play, at time t): with no current action, false. Else the
finish target goes on the 2D target (0x00558290 with 0x00576d80, d90, da0,
db0), the `FMV` sound set is loaded (0x0056f520), the keys are cleared
(0x0051a300), t is kept at +8 and the first action started at 0.

0x00599620 (poll, at time t): with no current action, false. If Space,
Escape, Enter or the keypad's Enter (DIK 0x39, 0x01, 0x1c, 0x9c, asked in that
order and only until one answers) is in any state (pressed, held, let go),
the current action is ended, every sound playing is stopped (0x0056eb10's
record to 0x0056f0a0, the record not freed) and the script ends: false.
Else the action steps at t less the start (a float: the x87 is at 24 bits
in play); when it is done it is ended and the next started at the same time.
True, also on the poll the last action ends (the next poll is false).

The time is timeGetTime() x 0.001f (0x005996d0, 0x00599710), at 24 bits:
seconds since Windows started, as a float. After about 4.6 hours of uptime
it steps in milliseconds, after 24 days in quarter seconds, so waits and
fades would stutter on a machine left on for weeks. The port's clock starts
with the game (the frame clock's FIX (B), fixes.md).

An action's slots: +0 its vtable, +4 the next. The base's (0x005c6130):
step false (0x00493110), start nothing (0x0040baf0), end nothing
(0x0049bfd0), and a run-to-end (0x00493120: its own clock 0x00575d30) that
nothing calls.

### Images (vtable 0x005cf850)

+8 the name, +0xc the image, +0x10 shown at once, +0x11 placed, +0x14 a
rect. Placed (0x00599760): x, y and two words of a static never set (0).
Not placed (0x00599820): 0, 0 and the render target's size (0x00558080) at
load time. Start (0x00599930): the image by name (0x005179d0). Step
(0x00599950): once in software, twice in Direct3D; each time, a placed image
(or any in mode 2) through the locked 2D buffer (0x00576de0, 0x0055b1f0 at
x, y, 0x00576e70), else the whole image stretched over the rect
([0x0072f360] = 0x005773e0, a DirectDraw Blt onto the 2D surface
[0x008027e8]); shown (0x00576ea0(0, 0, 1, 1)) when shown at once. Done at
once. End (0x005999d0): released (0x00517b10).

LOADIMAGE does not show its image: the next flip that copies the 2D surface
does (0x0057a850's blit). In SPLASH that is the WAIT's end, so the logos
appear as the 4 seconds end, while the main menu loads, and the wait shows
what the screen held before (the intro movie's last frame, or black with
`-nofmv`). Not yet checked on the original (Owen's PC).

### The wait (vtable 0x005cf820)

+8 seconds, +0xc the start (a float). Start (0x0059a460): t as a float. Step
(0x0059a470): t below start + seconds (a float sum). End (0x0059a490): the
2D surface shown (0x00576ea0(0, 0, 1, 1)).

### Fades (vtable 0x005cf868)

0x005999f0: +8 out (0 FADEIN, 1 FADEOUT), +0xc the colour (0x00579370 of
the r, g, b bytes), +0x10 seconds, +0x24 alpha. Start (0x00599a40): the
screen grabbed (0x00579500(1)) at +0x20, t at +0x18. Step (0x00599ab0):
without a grab, done. f = (t - start) / seconds; a fade in uses 1 - f and
ends at 0 or below (NaN too), a fade out ends past 1. The grab drawn whole
(Direct3D: in the 3D lock 0x00576d00 / 0x00576d60; software: the 2D lock),
the colour over it at alpha x f (0x00558540), then in Direct3D the quad
(0x00579af0, [0x0072f3ec], 0x00579b10) and a flip without the copy, in
software the finish (0x00558610), the unlock and the flip with it. End
(0x00599be0): the grab released through zImage (0x00517b10). The fade state
(game-loop.md) is the same drawing on the frame's time.

### Blurs (vtables 0x005cf898, 0x005cf808, 0x005cf7f0)

0x00599f70(steps, passes): +8 steps, +0xc passes; the loader gives 1 step.
Start (0x00599f90): +0x10 a rect of the screen's size (0x00576cb0,
0x00576cc0), +0x20 one of the 2D target's (0x00576d90, 0x00576da0); in
Direct3D the finish target on the 2D target at the screen's size and the
picture copied from the back buffer to the 2D surface ([0x0072f358]), in
software on the renderer's buffer (0x00576ca0, 0x00576cd0) and copied back
([0x0072f35c]). Step (0x0059a070 both, 0x0059a100 along, 0x0059a190 down):
a step less; `passes` passes (0x0055a540 with EDX 3, 1, 2) in the 2D lock
(Direct3D) or the 3D lock and then the copy (software); shown with the copy;
done when no steps are left. End (0x0059a040): the finish target back on
the 2D target. 0x00599a70 runs one to its end; the pause screen does that
with a stack blur of 4 steps of 1 pass (0x00493040), so the battlefield
behind the pause menu softens over four frames before the menu grabs it.

FIX (A): the program has one 2D buffer, so the copies are nothing to do and
the blur works on the buffer in place (app_fmv.cpp).

### Sounds (vtable 0x005cf7d8)

+8 the sound, +0xc the voice, +0x10 the name. Start (0x00599d10): found by
name (0x0056f6f0), the last voice stopped (0x0056e9f0), played at volume 1
(0x0056e3d0). Done at once, the sound left playing.

### Movies (vtable 0x005cf880)

0x00599c00: +8 the path `<FMV_PATH>\<name>` (calloc'd), +0xc the flag. When
`_stat` does not find it the exe looks for it on a CD-ROM drive (0x00575d40
with ECX 5) and copies the found path over its own; without one the path is
freed and the movie ends at once (step 0x00599de0 is false without a path).
FIX (A): the drives are not searched (protection.md, the disc check): the
movies come from the imported folder. Start (0x00599ea0): the stream
(new(0x1e4), 0x0059a4a0) and the frame counter at -1. Step: the frame for
the time (0x0059aaf0), drawn and shown. End (0x00599f40): the stream freed.
Until item 11 the port finds no file (GameFmvHost::file_stat), so PLAYAVI
ends at once.

PLAYMCI (vtable 0x005cf8b0, 0x0059a220): an MCI movie in the window, played
at start (0x0059b080), its end grabs and clears the screen (0x0059a3d0). No
block uses it; the port's ends at once.

## 4. The blur passes

0x0055a540 (ECX rect or null, EDX kind): 1 along the rows (0x0055a3b0), 2
down the columns (0x0055a1b0), else both (0x00559ec0), on the finish target
([0x0072c1e4], width [0x0072c1e8], height [0x0072c1ec], pitch in pixels
[0x0072c1f4]) through the ripple's scratch ([0x0072c1e0], rows of the
target's width). Each pixel becomes (a + b + 2c) / 4 of its two neighbours
and itself per channel: green under its mask, red and blue together under
theirs (0x00579280's masks; the sums cannot carry into the next channel).

- Along: rows from the rect's top (at least 0) to its bottom, pixels from
  its left (at least 1) to its right less one. Each row is blurred into the
  scratch and copied back at once, so the next row's left neighbour is
  already blurred.
- Down: rows from the top (at least 1) to the bottom less one, every column
  of the rect, into the scratch, then copied back. The rows above and below
  are a scratch width away, not a pitch.
- Both: down into the scratch (the row above copied first, the bottom row
  last), then along back onto the target, each row's two end pixels copied.

Quirk: the passes step from row to row by the width less the pitch after
each row's pixels, not by the pitch. On the whole of a target as wide as its
pitch that lands on the next row; with a rect narrower than the target (the
along pass always: it does two pixels fewer) each "row" starts where the last
ended, so the pass runs over the buffer as one line and mixes the ends of the
rows. Nothing in the game blurs a rect.

## 5. Tests

`tests/differential/test_fmv.py` (probe `tests/probe_fmv.cpp`): every shipped
block named above and random blocks with every verb (short and bad entries,
unknown verbs) loaded from the same binary tree on both sides, played and
polled at random times in Direct3D, software and mode 2, skipped by a key,
rewound, replayed, left mid-way; every call out compared in order with its
arguments, and each poll's answer and current action. The pause screen's
blur (0x00599f70 run by 0x00599a70). The three passes bit for bit on random
screens (with the rows around the target the walk reads), scratch buffers,
pitches, rects and both 16-bit formats. Shots: `build/mw3 --nosound --hidden
--shot-any --shot-frame 3` is the SPLASH logos (frame 1 the start-up's clear,
2 the INTRO wait's end).
