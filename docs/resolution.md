# Screen size and aspect (any size, 16:9)

Mech3.exe 1.2, mapped 2026-10-21 (mac 2) for Owen's decision (fixes.md,
Candidates and Decided): any size up to 4K, 16:9 included, 640x480 4:3 the
default and identical to the original. 16:9 widens the view instead of
stretching it. The menus and HUD art are scaled, and the menus pillarboxed.
If larger sizes are trouble, the cap is 1920x1080. A FIX (B), behind
`zfix::options().any_resolution`.

## 1. Where the size comes from

The video mode (VMode [0x005f4d0c], and InGameVMode [0x005f4d10] for a
mission) is the only source. `set_vmode` 0x0040d810 (`mech3/options`)
turns it into three rectangles:

- the render rect (0x0040d6a0): what the 3D view draws;
- the window rect (0x0040d7b0): the zClass window node;
- the display rect (0x0040d750): the screen and the 2D layers.

The modes are:
- 3: 320x240 drawn, doubled to 640x480; software only.
- 5..10: 640x480, 800x600, 1024x768, 1152x864, 1280x1024, 1600x1200.
- 4 and any other number keep the sizes they had.

The input gets the display size (0x0051b590, `zin::mouse_set_screen`).
Nothing else stores a width or height of its own: everything below asks
these rectangles, or assumes 640x480.

## 2. Every place the size reaches

The 3D view:

| Exe | What | Port |
|---|---|---|
| 0x0040d590 | the camera's horizontal field of view = the vertical one x render w / h, linear in the angle | `mech3::set_camera`; the getter 0x004f3ee0 and setter 0x004f3d90 were unwired hooks |
| 0x004f9fa0 / 0x004fa0b0 (`WindowResolution`) | the window node's size and origin, from the render rect | `zcls` window (ported) |
| 0x00521c60 | the projection: centre = window origin + half size; `fx = cot(hfov / 2) * w / 2`, `fy = cot(vfov / 2) * h / 2` (render.md section 0) | `zmth` (ported) |
| 0x007402f8 | the clip rect, from the window node | `zmod::clip2d` (ported) |
| 0x004f3b90 | the zoom setter: angle = base / factor, **capped at 1.396 rad (80 degrees)**; only gamez.zbd's camera load (factors 1) and the unshipped anim events 20/21 call it | `zcls::camera_set_fov` |
| 0x00480df0 / 0x00481290 / 0x00481790 / 0x00481a10 / 0x00443320 | the frames: render target and back buffer by the three rects | `app_states` (Direct3D path); the software paths are not ported |
| the HUD's radar 0x0043f0c0 | the view cone from the camera's +0xe8 | follows the camera |

The 2D side:

| Exe | What | Port |
|---|---|---|
| zTwoD's dialogs, every .zrd position | 640x480 pixel positions | `ztwod`, the menus (menus.md section 7, mainmenu.md section 3) |
| 0x0040a110 / 0x0040a210 / 0x0042fb30 | the menu backgrounds' views and wraps, MECH3TEXT's slide: the display size | `menu_widgets` |
| 0x004289a0, 0x0042a590, 0x0042da80, 0x0042e890, 0x00486260, 0x00488bc0, 0x00493260, 0x00495f90, 0x00438cc0 | each screen's background view at the display size when it is made | credits, controls, main menu, pilot screens (ported); 0x00438cc0 the graphics screen's |
| the version line 0x005c4b04 / 0x005c4b00 | the constants 640 and 480 | `mainmenu` |
| zVideo buffer 2 grabs (controls' axis popup, load/save) | the display size | ported |
| the still background 0x0055b3c0 | a 1280x960 image: the source rect past it above ~1000 lines was the community's crash; the port clips (fix done) | `zrndr::blit16` |

The HUD:

| Exe | What | Port |
|---|---|---|
| 0x0043bb80 | hud.zrd's block by VMode: 3/5 V640x480, 6 V800x600, 7 V1024x768, 8..10 the three hidden; any other mode keeps the 640x480 defaults (hud.md section 3) | `hud`, `app_view` |
| constructors (hud.md section 5) | 640x480 default positions, radar area 320x240, message lines from y 0x5f, fixed fonts | `hud` |
| 0x0043d6d0, 0x0043fa40, 0x00440510, 0x00440b00, 0x004438f0, 0x004445f0 | the zoom window, on-screen clamps, target lines, the arrow, the centre at the display's half size | `hud`, `app_hud` |
| 0x0040f330 | the HUD's text area re-placed after a mode change (display size) | not ported |
| 0x00479dd0 / 0x00479ef0 | the mission timer's 2D box | partly (`hud.h`) |
| 0x00468900, 0x004690d0 | the mech's screen-space aim (window rect; unproject) and a display-width text | not ported |

The modes and settings:

| Exe | What | Port |
|---|---|---|
| 0x0040c420 | detail.zrd's VMode (not saved, 5 on any modern PC), InGameVMode read and saved as any number (settings.md) | `options` |
| 0x0047fc10 / 0x0047fe10 / 0x00480970 | the start-up: an earlier run's mode re-applied, VMode below 5 raised to 5, the screen opened at it | `app` |
| 0x004ab860 | the device's range: software 3..5; a card 5..7, cut to 6 / 5 by a video-memory estimate | `Engine::video_mode_range` |
| 0x0043a470 | RESOLUTION: the modes in the range, labels 0x41..0x44 and 0xfda..0xfdc (8..10 named but never in range) | `ResolutionCycle::rebuild` |
| 0x004ab6b0 | the switch: zVideo's surfaces re-made at the new size, the render target, the HUD (0x0040f330), every mech's cockpit (0x0041cdc0) | not ported (`set_screen_mode` hook) |
| 0x004ab910 | Shift+V (`VMODE_TOGGLE`): the next mode in the range, saved as InGameVMode | not ported |
| 0x00481f70 | Core state: the drawing path re-picked when VMode changes | `app_states` |
| 0x00482070, 0x004834b0 | the pixel doubling (mode 3) and the stride for the software finish | `app_states` |

The platform (FIX (A) layer):

- `GameEngine::video_open` makes the 16-bit 2D buffer at the display size
  and sets `Render3D`'s viewport to it.
- `SdlRender3D` renders into a back buffer the window's pixel size and
  stretches the game's viewport over it: a 16:9 mode in a 4:3 window is
  squashed, so the window must take the mode's shape, or be pillarboxed
  or letterboxed.
- `mw3_main.cpp` scales the mouse by `640 / window width` (`mickey_scale`),
  a 640x480 assumption.
- `--window WxH` (default 1280x960) is independent of the mode.

## 3. The original's view is not square

At 640x480 the camera is 80 degrees across and 60 high (the world's camera
data, and 0x0040d590's rule 60 x 640 / 480). Projected onto 640x480,
tan 40 / tan 30 = 1.453 against 640 / 480 = 1.333: the horizontal scale is
`cot 40 x 320 = 381.4` against the vertical `cot 30 x 240 = 415.7`, so
everything is drawn about 8% narrower than tall (discoveries.md).

0x0040d590's rule is linear in the angle. At other shapes it gives:
- 1920x1080: 106.7 degrees across, which squeezes the picture to 77%.
- 1280x1024 (5:4): 75 degrees.

The 16:9 fix keeps the original's 4:3 look: the same vertical angle and
the same squeeze. The rule is `tan(h / 2) = tan(v / 2) x (w / h) x k`, with
`k = tan(2v / 3) / (tan(v / 2) x 4 / 3)`; k is 1.090 for 60 degrees, so the
squeeze is 1 / k = 0.9175. At 16:9 that gives 96.4 degrees across, where
square pixels would give 91.5 and the linear rule 106.7. A 4:3 size takes
the original's formula unchanged, so 640x480 stays bit for bit.

## 4. Porting order

1. **Settings and the mode list (done 2026-10-21, section 5)**: the sizes,
   the graphics screen's list and the range check, the camera's angle for
   any shape. Headless.
2. The window's shape: `--window` and the SDL window follow the mode's
   aspect (or pillarbox the back buffer); the mouse scale from the display
   size, not 640. Platform only; Owen checks it on screen.
3. The mode switch 0x004ab6b0 and Shift+V 0x004ab910 on the port's zVideo:
   the 2D buffer, `Render3D`'s viewport, the HUD's text area 0x0040f330, the
   cockpits 0x0041cdc0.
4. The HUD by anchors: the 640x480 block's pieces placed from the left,
   right and bottom edges of any size (hud.md section 5), with a scale for
   the art. The hud.zrd blocks stay for their own modes when the fix is
   off.
5. The menus: one 640x480 frame scaled to the height and centred
   (pillarboxed), the cursor and clicks mapped through it; the backgrounds
   tiled past 2560x1920 (menus.md section 7).
6. The rest of section 2's unported readers (0x00468900's aim, 0x00479dd0's
   timer) as their modules land.

## 5. The port (step 1)

`zfix::options().any_resolution` (FIX (B), on; the probes turn it off).
With it:

- **Modes.** 3 and 5..10 are the original's. 11..15 are 1280x720, 1600x900,
  1920x1080, 2560x1440 and 3840x2160. 16 is the player's own size,
  `ScreenWidth` x `ScreenHeight` in the settings (saved, 1920x1080 by
  default), clamped to 640x480..3840x2160. Without the fix, 11..16 keep
  their sizes, as any unknown mode does in the exe. `mech3::vmode_size`
  answers a mode's size, and `kMaxScreen` (3840x2160) is the cap: one
  constant to lower to 1920x1080 if larger sizes are trouble.
- **The range.** A 3D card offers 5..16, since a modern card has the memory.
  Software stays 3..5.
- **The list.** RESOLUTION shows 8..10 under their own names, then the new
  sizes, labelled "1280x720" and so on, the custom one by its size.
- **The camera.** `set_camera` keeps 0x0040d590's linear rule when
  w x 3 == h x 4. For any other shape it uses section 3's rule. The
  options' `camera_get_fov` / `camera_set_fov` are wired to zClass
  (0x004f3ee0, ported now; 0x004f3d90).

Checked by `tests/test_resolution.cpp`:
- every original mode's sizes;
- the new modes and the custom size with its clamps;
- the range and the list, with the fix and without;
- the camera at 640x480 (bit for bit the original's rule);
- at 16:9, 5:4 and 21:9 (the vertical kept, the squeeze kept);
- through the wired hooks onto a real camera node.

The original's paths are those of test_app_options.py and test_menus
(fix off). Breaks: with square pixels (k = 1) the 16:9 test fails, and so
does the modes test with 11..16 not gated by the fix. Removing the 4:3 guard
changes nothing: on 20,000 vertical angles the new rule at 4:3 rounds to
the original's float every time, since tan and atan cancel. The guard only
keeps the original's own expression.
