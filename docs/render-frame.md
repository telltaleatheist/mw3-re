# The Direct3D frame around the scene: locks, scene nesting, the colour tint

What a Direct3D frame does besides the scene walk (render.md): the back
buffer locked for 2D drawing on top of the 3D picture, the nested
BeginScene / EndScene, and the full-screen colour (flashes and fades) as a
Direct3D quad or, in software, a pass over the 16-bit buffer. All ported and
emulator-checked; the DirectX calls under them are the program's (FIX (A)).

## 1. Surfaces and the 3D lock

zVideo keeps three surface records of 0x24 bytes: the back buffer
`0x008027a4`, the software renderer's surface `0x008027c8`, the primary
`0x008027ec`. A record is `+0x00` width, `+0x04` height, `+0x08` pitch,
`+0x0c` valid, `+0x10` pixels, `+0x14` locked, `+0x20` the DirectDraw surface.

- Lock 0x00577590 ([0x00809340]): the primary only when [0x008026e4]; a
  locked record is left; else DirectDraw Lock (0x00577630, DDSURFACEDESC2:
  height +8, width +0xc, pitch +0x10, lpSurface +0x24) fills the record and
  sets valid and locked. Unlock 0x00577600 ([0x0080933c]) clears locked and
  unlocks (0x00577690).
- 0x00576d00, the Engine's `video_begin_3d`: locks the back buffer, then
  points every 2D target at it: the fillers' and blit's 0x00557f90 (pixels,
  no rect, pitch), the finish target 0x00558290 (pixels, width, height,
  pitch: [0x0072c1e4..0x0072c1f4]), and the windows-text object's
  0x005a67a0 ([0x00736328] +0x18..+0x24). 0x00576d60 unlocks.
- The Core state's Direct3D frame locks twice: around the particles, and
  around the HUD and radar; the fade's step locks to draw its still.

## 2. Scene nesting

0x00579af0 calls BeginScene (0x0057a740) only at depth 0 and counts up;
0x00579b10 calls EndScene (0x0057a7e0) only at depth 1 and counts down
([0x0080270c]). The scene walk (0x004f7030) and the Core frame's tint pass
both use them.

## 3. The colour tint

0x00558540 (CX colour, EDX rect or null, the alpha a double):

- No rect: the finish target's whole screen with right = width - 1 (an
  inclusive right) and bottom = height.
- Direct3D ([0x0072f348]): 0x0057dd80 with the rect's right plus one. The
  quad ([0x008095c0], four D3DTLVERTEX, pending [0x008095bc]) gets the rect's
  corners and one ARGB colour: alpha `ftol(alpha * 255)`, the 16-bit colour
  widened by the display's masks and shifts. Without a rect it would take
  the software surface's [0x008027cc] as its right and [0x008027c8] as its
  bottom (height and width swapped, read unsigned); no caller passes none.
  [0x0072f3ec] = 0x0057e380 draws it once in the next scene: Gouraud, blend
  on, Z writes off, no texture, ZFUNC ALWAYS, the current primitive type,
  then ZFUNC GREATEREQUAL, blend off, Z writes on. The scene walk calls it
  after the overlay flush; the Core frame and the fade between their own
  0x00579af0 / 0x00579b10.
- Software: kept at [0x007fb2dc..]: pending (never cleared by the finish),
  left, top, right, bottom, colour, alpha. The finish 0x00558610 (the
  Engine's `render_finish`) tints rows top..bottom-1 (clamped to the
  target), from left (at least 0) for `min(right, width) - left` pixels:
  each field `field * keep / 32 + colour_field * in / 32` with
  `in = ftol(alpha * 32)` and `keep = ftol((1 - alpha) * 32)` (24-bit), the
  colour terms taken through the display masks. The row routine by the CPU
  ([[0x008008d8]] bit 2, MMX) and the 2D format ([0x0074a738] 5: RGB555):
  0x005582c0 / 0x00558320 (scalar, a pixel pair a dword, count / 2 + 1
  pairs) or 0x00558380 / 0x00558460 (MMX, count / 4 quads of words).

Quirks:

- The MMX rows tint count rounded down to four: with the inclusive right
  edge the screen's count is 639, so the last four columns of a 640-wide
  screen stay untinted. The scalar rows tint one pair more than count / 2:
  the whole width there, and one or two pixels past a rect's right edge.
- FIX (bug): on the last row of the buffer that extra pair lies past the
  buffer's end; the port writes only the pixels inside it.
- A right edge left of the left one makes the original's row loops run
  wild (a crash); no caller does it.
- The tint stays pending, so every later software finish tints again with
  it until 0x00558540 sets another. 0x005587b0 clears it (no caller found in
  the code; perhaps through a table); the software renderer's 0x005578f0 and
  0x005674e0 also write [0x007fb2dc].

Callers of 0x00558540: the fade state's step (0x0048f0b0), 0x00599b66 and
0x005a53cf (2D front end, not ported). 0x0057dd80 is also called from
0x0055a653 and 0x0055a7b3 (not ported).

## 4. The port

- `zvideo/zvid_dd` (surfaces, the lock and unlock, scene nesting, the 3D
  lock, the text target), `zvideo/zvid_ddd3d` (`set_tint_quad`,
  `draw_tint_quad`), `zrender/zrndr_tint` (the finish target, the tint, the
  finish and its four rows).
- The program (`GameEngine`, program.md): `video_begin_3d` / `end_3d` /
  `begin_scene` / `draw_3d` / `end_scene` / `render_finish` /
  `screen_tint` call these. FIX (A): the back buffer's Lock hands out the
  16-bit layer, filled with a key (0xf81f), and its Unlock lays every pixel
  the 2D code changed over the 3D frame (`Screen2D::overlay_16`). Render3D
  keeps one back-buffer texture per frame; scenes, layers and the quad draw
  into it in order, the frame clear clears it once, and the flip shows it.
- Not reproduced: a lock that reads the 3D picture back (the layer starts
  as the key, so 2D code that reads the screen sees the key; nothing ported
  reads it: a readback each lock would stall the GPU every frame).
- The alpha blends into it (blit16's alpha images, 0x0055b3c0: the HUD's
  compass, reticle and gauges) would mix with the key instead of the
  picture. FIX (A): while the layer is locked, `zrndr::layer_cover()` holds
  a per-pixel alpha and colour; a blend over an untouched pixel writes the
  source and records its alpha, over a partly covered one combines the two,
  over a drawn one blends as written and marks it opaque. The overlay draws
  a pixel still holding its recorded colour with the recorded alpha. In
  Direct3D the original's HUD images take this same software blit onto the
  locked back buffer (their DirectDraw surface path, 0x00576f90 keyed on
  black, only serves images made by [0x0072f360] = 0x005773e0).
  mw3tests `fix_layer_blend_keeps_its_alpha`.
- The back buffer's picture between frames. The exe's back buffer kept the
  last frame shown (the flip chain's other page), so a 2D lock (0x00576de0)
  after a 3D frame, or a 3D lock that draws nothing, found the screen there:
  the message box over a mission's end, or over the main menu (its
  0x004ac150 locks and unlocks the back buffer first), draws over it and
  grabs it. FIX (A): the first 2D lock after a frame through Render3D reads
  that frame into the 16-bit buffer, and a 3D lock over a 2D picture that
  draws nothing gives the picture back (`GameEngine::video_lock`,
  `lock_back` / `unlock_back`). `tools/analysis/boxcheck.py`.
- Fog is on from the device's setup (0x0057bb70(1): FOGENABLE, and the
  light state FOGMODE linear, which Render3D's vertex fog stands for):
  `zvid::set_fog(1)` in the program's stand-in for the setup. Before, only
  the shadow passes turned it on, so a frame drawn before a mission's first
  (the lab's view from the briefing) had no fog and no object3d tint.
- The tint quad's vertices (the fades' colour, 0x0057dd80) are set up as
  unfogged (specular 0xff000000) by the screen mode's setup (0x00579c60) and
  given depth and 1/w 0.99 by the device's (0x0057dd40 from 0x0057a900);
  `zvid::reset_tint_quad` / `set_tint_depth`, called where the program
  stands in for them. Without them the quad was fully fogged once a
  mission had turned fog on: the end of a mission faded to its fog colour
  (c1's light grey) instead of black.
- The screen grab 0x00579500 (the fade's still, game-loop.md) does read it
  back, FIX (A): `Render3D::read_back` downloads the back-buffer texture
  (SDL GPU, waited on a fence), and `GameEngine::video_grab` scales it to the
  game's 16-bit size. The Core state queues its fade before the mission's
  first frame, so the still is the last 3D picture drawn; the fade brings it
  in from black over two seconds. Checked with `tools/analysis/fadecheck.py`
  (`mw3 --shot-any`, frames 60, 200 and 400 of c1m1: the picture, brighter
  each time; flat black before).

## 5. Testing

- `test_d3d_frame.py` (`tests/probe_frame.cpp`): the tint in both modes
  (3,000 runs: the software state or the quad, 0x0057dd80 run natively),
  the finish on random buffers with every row routine and rects reaching
  outside (600 runs: pixels and constants identical inside the buffer; the
  original's writes past it counted), the 3D lock and unlock with DirectDraw
  stubbed (2,000 runs: record, targets, Lock and Unlock calls).
- `test_drawtable.py`, `test_tint_quad`: 0x0057dd80, 0x0057e380 and the
  scene nesting mixed with the draw table's calls (1,500 calls, 160 quads
  drawn: device calls, quad memory, caches and depth identical).
- Breaking the port: 18 of 19 caught (the inclusive right, the quad's
  right + 1, the tint not kept, the scalar rows' extra pair, the 565 merge,
  the MMX count, the row choice, the alpha scale, the clamps, the pair
  rotation, a relock, the valid flag, the finish target's size, EndScene at
  every depth, the quad's alpha, ZFUNC left ALWAYS, a quad corner); the
  miss, the MMX blue product kept in 16 bits, cannot change a result
  (0x1f x 32 fits).
- mw3tests `frame_lock_lays_the_layer_and_the_tint_goes_through_the_ports`.
- Headless: `mw3 --mission c1m1 --shot` and `mw3 --fly c1 --shot` draw the
  frame.
