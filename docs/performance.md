# Performance (release builds)

What a frame costs in a Release build (what Owen plays on Windows), where
the time goes, and what could be made cheaper. Measured on the Mac
(2026-10-30, Apple silicon, Metal); Windows numbers still to come.

## How it was measured

- `cmake -S . -B build-rel -G Ninja -DCMAKE_BUILD_TYPE=Release`.
- A write dir per mode with `settings.reg`'s InGameVMode set: 5 (640x480),
  13 (1920x1080), 15 (3840x2160); `--window` the same size.
- `--frame-ms 33` (each frame 33 ms of game time, so a run is in the
  mission after the same number of frames whatever the speed), timed with
  `--frame-times 100` over frames 100..1200 (all in the Core state).
- `--hidden`: no swapchain, so a frame's time is the CPU's (the GPU work is
  submitted, not waited on). A visible window waits for vsync.
- Scenes: `c4m2 --mech-ahead 100` (rain, the player near the enemy),
  `c3m6 --mech-ahead 100`, `ia1w8` (Instant Action map 1, wave 8, four
  allies). Without `--frame-ms` the first 1000 frames are only the loading
  fade, at about 1 ms each.

## Frame times (ms)

CPU (hidden), mean / p99:

| Scene | 640x480 | 1920x1080 | 3840x2160 |
|---|---|---|---|
| c4m2 near the enemy | 3.37 / 4.04 | 4.46 / 4.99 | 4.48 / 5.18 |
| c3m6 near the enemy | 2.66 / 3.17 | 3.40 / 3.92 | 3.43 / 4.00 |
| IA map 1, wave 8 | 2.59 / 3.08 | 3.34 / 3.85 | 3.33 / 3.78 |

1920x1080 and 3840x2160 cost the same: the 2D layer is the canvas (960x540
at both, the art scale), not the display; the 3D viewport is the full size.

Visible windows (vsync on a 60 Hz display): 16.7 ms median at every size
and scene, p99 17.4 to 19.7 ms; a few single frames of 33 to 66 ms in a run
of 800 (one to three missed vsyncs).

## Where the time goes

`sample` on the hidden runs at 1920x1080, the main thread's samples
inclusive (c4m2 / ia1w8 / c3m6, share of `CoreState::frame`):

| | c4m2 | ia1w8 | c3m6 |
|---|---|---|---|
| `SdlScreen2D::overlay_16` (the layer over the frame at each unlock) | 61% | 53% | 53% |
| of it, the 16-bit to RGBA conversion (`SdlScreen2D::upload`) | 32% | 26% | 27% |
| of it, the texture upload (`SdlRender3D::upload`, a new transfer buffer each time) | 27% | 28% | 26% |
| `SdlRender3D::end_scene` (vertices, a new transfer buffer a scene) | 14% | 15% | 13% |
| the scene walk (`zcls::render_world`) | 8% | 13% | 11% |
| the AI (sensors, brains, firing; `clear_line` sight lines 3%), roughly | 9% | 10% | 8% |
| mechs' update (collision and ground queries in it), roughly | 5% | 7% | 7% |

A frame overlays the layer three times (one of them under the weather's
draw, `Weather::draw_in_rect`; the others not traced). In c4m2 one of the three
held about 8,000 drawn pixels and the other two none at all, yet each
converts all 960x540 pixels to RGBA, uploads them through a transfer buffer
made for it, and draws the whole layer.

## What could be cheaper (not done; for the lead)

All in the SDL layer, none in the game's logic, so the differentials do not
change:

1. An overlay with nothing drawn (every pixel the key) skipped: one pass
   over the 16-bit pixels instead of the conversion, upload and draw.
2. Only the drawn pixels' bounding box converted, uploaded and drawn.
3. The transfer buffers kept and cycled (SDL_GPU's `cycle`) in
   `SdlRender3D::upload` and `end_scene`, not made and released each call.
4. The 565 to RGBA conversion through a 64K-entry table.

Seen meanwhile: at 1920x1080 the mission's opening fade darkens only the
top-left quarter of the screen (the fade's still at the 960x540 canvas
drawn into the 1920x1080 frame's corner).
