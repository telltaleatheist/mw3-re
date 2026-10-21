# The main menu's ripple (zRender 0x00558dd0)

Mech3.exe 1.2, ported 2026-10-21 (mac 2). The water ring around a main menu
button the mouse enters (mainmenu.md, SlideButton and Ripple). It has no
colour of its own: it moves the pixels already on the screen, so on the
main menu it looks like the green background rippling.

## What it does

`0x00558dd0(ECX x, EDX y; inner, outer, amplitude, wavelength, phase, clip)`,
on two buffers:

- the screen: the finish target 0x00558290 sets ([0x0072c1e4], pitch
  [0x0072c1f4]), the locked back buffer;
- a scratch, [0x0072c1e0]: 0x180000 bytes malloc'd once by zRender's start
  0x005581c0, whose rows are as wide as the screen ([0x0072c1e8]).

1. Radii: an outer below 1 is 0; inner is clamped into 0..outer; equal radii
   draw nothing. The clip is the argument (left, top, right, bottom) or the
   screen (0, 0, width - 1, height - 1). If the ring with its amplitude
   misses the clip, nothing happens.
2. Two tables on the stack, one entry per radius from max(inner - 1, 1) up
   to outer: `amplitude x sin(r / wavelength + phase)` and `1 / r`, as
   floats.
3. For each (i, j) with -outer <= j <= i <= inner, and its seven mirror
   images, the squared distance is taken as a float. Between inner^2 and
   outer^2 (exclusive), the radius r comes from the float's bits (the
   exponent halved, the mantissa linear: never below the true root, so the
   table is never read below inner - 1), capped at outer. The pixel takes
   the screen pixel `ftol(i x k)`, `ftol(j x k)` further out, with
   `k = sin-table[r] / r`. Elsewhere it takes its own screen pixel. Both
   are written into the scratch.
4. The square around the centre is copied from the scratch back onto the
   screen.

There are two copies of steps 3 and 4:
- When the ring plus amplitude lies inside the clip (left <= x and
  x < right, the same for y), it runs without checks.
- Otherwise every pixel and its source are checked to lie inside the clip.
  The right and bottom edges are exclusive, so the clip's last column and
  row are never written.

The x87's precision changes the result: the table's sum and the products k
and i x k round to floats at 24 bits and to doubles at 53. The port takes
both (`zrndr::X87`); the program uses zfix's fpu53, as other ports do.

## Quirks

- **The copy-back tests screen coordinates as if they were the ring's.**
  Step 4 skips rows and columns within ±inner of 0, not of the centre, and
  only moves its pointers on a copy. Away from the screen's top-left
  corner, where inner is never as large as the coordinates, it copies the
  whole square. A ripple near the top-left corner would shift rows sideways.
  Kept: no shipped button sits there.
- **Stale scratch at the clip's edge.** A clipped point whose source lies
  outside the clip is never written to the scratch, yet the copy-back
  carries it onto the screen. What shows there is whatever an earlier
  ripple left in the scratch. Kept: the port's scratch persists the same
  way, and starts zeroed where the exe's held malloc's leftovers.
- **The scratch only holds 1024x768.** 0x180000 bytes is 786,432 pixels,
  and its rows are the screen's width. A ripple reaching below row
  786,432 / width writes past it: row 614 at 1280x1024, row 491 at
  1600x1200. The original never got there, because the menus switch the
  screen to 640x480 before their first frame (menus.md section 7). The
  port's menus are to run at any size (resolution.md), so its scratch grows
  to the screen: FIX (B).

## The port

`src/gamez/zrender/zrndr_ripple.{h,cpp}`: `zrndr::ripple` and
`ripple_state()` (the scratch, the centre and the clip). The SDL front
end's `SdlTwodHost::ripple` calls it on the finish target. `video_lock` points
that at the program's 16-bit buffer, the one the main menu draws into and
`video_flip` presents, so the ring moves what the menu drew before it in
the same frame, as the exe moved the locked back buffer. zTwoD's base
`Host::ripple` stays a recorded call: test_mainmenu compares the calls
with the exe's.

## Checks

- `tests/differential/test_ripple.py`: 600 ripples, at 24 and at 53 bits.
  The cases are random 16-bit screens and scratches, pitches wider than the
  width, centres inside, near and past the edges, clip rects, and
  main-menu-sized rings (640x480, outer 20..200, inner outer - 36,
  wavelength 12). The screen and the scratch must match pixel for pixel.
  Breaks, 150 cases each: measuring the copy-back from the centre (57
  differ), a cleared scratch (207), an exact square root (133) and an
  inclusive clip (93) are all caught. Swapping the precisions is not: the
  24- and 53-bit results agree on every case tried (400 more in the port
  alone). The rounding differences would only show where a shift lands
  within a rounding step of a whole pixel. The switch follows the exe's
  arithmetic, but nothing here shows it mattering.
- `tests/test_sdl_frontend.cpp`, `sdl_host_ripple_moves_the_ring_on_the_screen`:
  a menu ring through the SDL host moves pixels in the ring only, and a
  clip holds.

## What it looks like (for checking on screen)

Moving the mouse onto a main menu button starts a ring at radius 20 around
the button image's centre. It grows 120 pixels a second. It is three waves
of 12 pixels wide, and its inside goes still once the ring has left the
centre. The background inside it is pushed in and out along the radius by
up to 7 pixels. The push falls by 0.3 a frame (per 1/30 s with zfix's
per_second) and the ring stops at 2. So a hover gives about half a second
of a widening, weakening water ring, out to about 85 pixels, over the green
background and anything drawn under it. Leaving the button stops it, and
FastMenu turns it off.
