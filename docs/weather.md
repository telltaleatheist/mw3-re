# The weather: rain and snow

Rain and snow are zTwoD objects (Object2D subclasses, 0x98 bytes) on the
render overlay layer [0x00736328], made by the world parameters' `weather`
block (world-params.md section 7) and drawn as screen-space streaks by the
Direct3D frame. Code: `src/gamez/ztwod/twod_weather` (the classes, the
layer's rect and tick), the program's wiring in `src/mech3/app_mission.cpp`
(`ProgramWeather`, `GameEngine::weather_rect` / `weather_tick`). The
original file is unknown: the functions sit between `zTwoD\twod_wintext.cpp`
and `zEntity\zent_init.cpp` (0x005a5310..0x005a6860).

Shipped: RAIN in c4 (m2, m3, ia, mp; 100 particles, colour 128 128 172,
gravity 3, wind 0 at 0.2, alpha_gradient 0.5 0), t1 m1..m4 (60, grey 165,
gravity 0.8, wind 0.1) and c3 mp2 only; SNOW in c4b m4 and m6.
Both are ported: the rain in item 19a, the snow in 19b.

## 1. The classes

| | Address | Vtable |
|---|---|---|
| Base (`ztwod::Weather`) | ctor 0x005a5470, dtor 0x005a5640 (deleting 0x005a5620) | 0x005d0528 |
| Rain (`ztwod::Rain`) | ctor 0x005a60a0 (dtor 0x005a60e0) | 0x005d0660 |
| Snow (`ztwod::Snow`) | ctor 0x005a5ad0 (dtor 0x005a5b10) | 0x005d05e0 |

Slots: +0x04 draw 0x005a5360 (shared), +0x24 update (rain 0x005a6110, snow
0x005a5b40; Ghidra's 0x005a5e10 is not a function but the snow update's
tail, made from a data pointer),
+0x60 / +0x64 zTwoD's show_for / hide (the world parameters' 0x492660(1)
shows it with -1, 0 hides it), +0x7c the particles' draw 0x005a57d0.

Fields (twod_weather.h): +0x34 the rect drawn into, +0x38 the streaks
(0x20 each: x, y, w, h, colour, u at the tail and the head, a shown flag),
+0x3c / +0x40 capacity and count, +0x44 colour (display-packed), +0x48 /
+0x4c alpha_gradient, +0x50 the player view's camera node, +0x58 / +0x5c two
place buffers (x y z per particle), +0x60 / +0x64 which is current and
next, +0x68 / +0x6c wind heading and speed, +0x70 gravity, +0x74 `down`
(0 1 0), +0x80..+0x88 the "SnowFX" ramp image and its texture, +0x8c..+0x94
(1, 20, 400 for both; unread by both). +0x54 is 0 (the constructor's,
never written): the snow's flake size reads it.

The constructor gives each particle a random place (0x005a56e0, CRT rand):
depth 0.5..1, x and y -1..0.5, either one below -depth folded up by 1.5
with the depth turned to 1.5 - depth. With a Direct3D device it makes a
16x8 ramp image (type 0xb, alpha plane, clamped) and its texture.

## 2. The frame

The Direct3D frame (0x00481870 and the other draw paths, `app_states.cpp`
draw_software / draw_hardware / draw_doubled): after the world's tick,
0x005a6790(0, rect) sets the layer's first rect (the render rect, or the
window's in the hardware path; 0x005a66b0, slots 0 and 1 only), the 3D lock
0x00576d00, then 0x005a67c0(dt), which ticks the layer (0x005a6600 ->
Layer::update 0x0059fd20, then clears its timed texts' count +0x1ec).

The camera draw 0x004f7030 would tick the layer too after its flush when
EDX is not 0 (`camera.h` after_scene), but its one caller (0x0044337c)
passes 0.

The layer [0x00736328] is the render overlay (0x005a4590, built at static
init by 0x005a6700): a Layer, active from the start, holding an object of
its own at +0x28 (vtable 0x005d0290: the screen tint, anim-run.md) and five
0x4c-byte texts at +0x70 (0x005a53e0), all hidden; its +0x18..+0x24 are the
2D text target. The rects, the tick and the tint are ported (`WeatherLayer`,
`ScreenTint`); the texts are not. The tint and the weather share the draw
0x005a5360 (`ManagerObject`, ours), and the tint is first in the list.

The rain's update 0x005a6110, unless hidden:

1. Adds dt to [0x0073653c] (summed, never read by the rain).
2. Without a camera, stops. The rect's size and corner come from +0x34;
   without one (the first frame, before a draw has set it) the corner is
   the screen's (0x00579880) but the width and height stay unset in the exe
   (FIX (bug): the screen's size).
3. The camera node's world place and angles (0x004ed530 / 0x004ed5d0). The
   motion this frame, each a tenth: the camera's move since the last frame
   ([0x00736528]), the fall (`down` x -0.1 gravity) and the wind (speed x
   0.1 at sin / cos of the heading in x and z). All three go into camera
   space through the camera's matrix (euler on a pushed matrix, set_view,
   load_view, rotate_points).
4. d = fall + wind - move, cut to length 1. The streak's vector s is d
   normalized and made 0.1 long, **but only when |d|^2 > 0.01**: a shorter
   move is its own streak, unscaled.
5. Per particle: the next place = place + d (overwritten in step 6), the
   tail = place + s; both ends projected as (k p + 0.5) x size + corner with
   k = 1.5 - depth (msvc_ftol); the streak's u at the tail is k_tail x
   alpha_tail, at the head k_head x alpha_head; shown = 1. Then a fresh
   random place for it (both buffers).
6. Object2D::update (0x0059ffb0), which draws it (it is not managed), then
   the buffers swap.

So the rain has no lasting motion: every frame each particle is a new
random point, and its streak is one frame's motion seen from the camera.

The snow's update 0x005a5b40 is the same up to the loop (its own time
[0x00736538] and camera place [0x00736518]; the port shares the code as
`Weather::motion` / `project`), then differs in the loop:

- A flake keeps its place: the next buffer holds place + d, and the buffers
  swap, so the flakes drift with the fall, the wind and against the camera.
- `shown`, the streak's second-edge offset in pixels, is ftol((+0x54 + 1)
  x k_head x 3.5) (0x005d05d8): 1 to 3 pixels, thicker the nearer.
- A flake starts again (0x005a56e0) only when its next place leaves the
  view: |y| > |z|, |x| > |z|, z > 1, or not z >= 0.5 (so a NaN depth too).

## 3. The draw

0x005a5360 erases (no background: nothing), points the software finish at
the layer's text target if it has pixels (0x00558290), then draws the
particles once per set layer rect (+0x10, +0x14) with +0x34 set to it, and
leaves +0x34 at the first.

0x005a57d0, Direct3D only (the software path 0x0055a950 is not ported):
lets go of the 2D lock if held (0x00576d70 / 0x00576d60); when the colour
changed, re-tints the ramp's first row (16 texels of the colour, alpha
k / 16 for k = 0, 255, .. 3825) and reloads the texture ([0x0072f374]);
scene begin; per streak a quad in the TL buffer 0x0072f41c: the tail
(x, y), the tail a pixel along x (when wider than tall) or y, the head that
pixel along, the head (x + w, y + h); u_tail on the tail's two, u_head on the
head's, v 0, depth the particle's new depth; drawn white-textured
([0x0072f3bc] = 0x0057caa0, priority 1, opacity 1) only when all four
vertices are inside the rect, edges included (0x005a5a60); the transparent
list flushed ([0x0072f3e4]), scene end, the 2D lock taken back.

The ramp image is malloc'd in the exe and only its first row is ever
written; the texture's other seven rows hold whatever was in memory. FIX
(A): the port zeroes them (only v = 0 is drawn).

## 4. Frame rate

The fall and the wind are per-frame steps of a 30 fps frame; the camera's
move is a real frame's. For the rain a streak's length is fixed (0.1 after
the normalize), so the frame rate changes only its slant: at 60 fps the
camera term halves and the rain looks steeper while you walk. The snow's
flakes keep moving by d, so they fall and blow twice as fast at 60 fps.
FIX (B), behind `zfix` per_second: the fall and the wind are scaled by the
frame's 30 fps steps, so both look as at 30 fps at any frame rate.

## 5. Tests

- `tests/differential/test_weather.py` (`tests/probe_weather.cpp`): the
  constructor and up to 12 frames (snow 40) of the update (with its draw)
  per case, 400 cases, odd seeds snow: every streak, both buffers, the
  summed time, the camera's last place, the held rect, the ramp's row and
  the calls (rand, reload, each quad's vertices, flush). The x87 at 24
  bits. Exact but for the x87's fsin / fcos against libm's (the wind, the
  camera's matrix), which a short d's normalize and a cancelling place
  magnify: 8 ulps, 1e-7 of a place, or a pixel, 69 frames in 5,227. The
  snow's places are the emulator's again after each frame (the ulps would
  add up). Frames starting without a rect are compared on the places only.
  20 break tests caught (7 of them the snow's).
- `mission_c4m2_rain_made_and_drawn_headless` and
  `mission_c4b_m4_snow_made_and_drawn_headless` (test_program.cpp): the
  mission makes its weather with its block's values, the frames set its
  rect and draw quads inside the window (snow: thicker flakes too), and the
  cleanup takes it off the layer.
- Screenshots (frame 90): c4m2 rain and c4b m4 snow, each on and off.

## 6. Open

- 0x004b5290 adds or removes another object, 0x0060b270, on the same layer
  (a camera-side object, not traced).
- The software draw 0x0055a950.
