# The modules' starts: zModel 0x00526510, zRender 0x005578f0 and 0x00557b30

Mission-plan item 17 (2026-10-28). The three calls set 157 globals once at
the start-up (0x00557b30 again at zRender's reset 0x00557b20). The port
holds the same state in its structs' initialisers, so the program answers
the calls itself (known in `GameEngine::known_unported`, program.md, The
census). Below, each global with the exe's value, the port's, and whether
code the port runs reads it (hardware path; the writes come from
the decomp, the reads from every direct reference in mech3.asm, a reader
counted as run when its address has a port).

Result: nothing to change. No global both differs and is read before
something else sets it:

- **The fog** (0x0074a22c..0x0074a25c): the exe starts it on, linear,
  colour (1, 0, 1), 500 to 700, altitude 200 to 300, density 2; the port
  starts it all 0 and also clears it at zModel's reset (models_reset; the
  exe's 0x00526800 keeps the last mission's). Neither is ever drawn with:
  the world read (cls_zbd, 0x004f1df0) sets all five groups and pushes
  them before the first draw, and the lab's view reads mission world 6
  first. Measured with the exe's values put in (temporary): the fog at
  every model draw in c1m1, t1, c3m1 and c4m2, the same from the first
  draw on with either start; in the lab (`--lab-mech 3`, item 16) off and
  all 0 at its first draw with either (the lab's own world, made empty,
  pushes all five groups as one of two worlds).
- **The model clip rect** (320 x 200 in the exe, 0 in the port): set from
  the window per camera (0x00526e10, `zmod::set_camera`) before a draw.
- **The GfxFlags masks' bases** (-1 in the exe, 0 in the port): the
  start-up's 0x00526750 / 0x00557ae0 point the masks at GfxFlags_HW.
- **The filler's winding** (1 / 0): each ported caller sets it (the HUD's
  polygon 0x004431fa, the shadows 1). **Bytes per pixel** (1 / 2):
  0x00557f90 sets it from the depth before a fill. **The screen width and
  row** (32 / 0): read only by the software span fillers.
- **The flare's depth rescale** 0x0074a3c0 / 0x0074a3c4 (0, 1): only the
  software draw 0x0053e050 writes them, so in hardware they stay an
  identity, which the port's flare (zrndr_flare.h) leaves out.
- The rest: equal, the port's own buffers or calls, read only by the
  software renderer (not ported, by the guiding rule), or never read
  directly (0x00800830..0x0080088c are the software span table).

### 0x00526510 zModel start

| Global | Exe | Port | Read on a path the port runs? |
|---|---|---|---|
| 0x005ef740 | 1 | 1 (`draw_state().static_lights`) | same |
| 0x0072f350 | 0.02 | — | no direct read |
| 0x007402f0 | 2 | — | no direct read |
| 0x007402f4 | 0 | null (`camera_data`, `init().camera`) | set per camera by 0x526e10 |
| 0x007402fc | 0 | `clip` (0) | set per camera by 0x526e10 |
| 0x00740300 | 0 | `clip` (0) | set per camera by 0x526e10 |
| 0x00740308 | 320 | `clip` (0) | set per camera by 0x526e10 |
| 0x0074030c | 200 | `clip` (0) | set per camera by 0x526e10 |
| 0x00740314 | 319 | `clip.side_1c` (0) | set per camera by 0x526e10 |
| 0x00740318 | 199 | `clip` (0) | set per camera by 0x526e10 |
| 0x0074032c | 4.0 | — | read only by unported (software) code: 0x535ed0, 0x535ef0, 0x53e050 |
| 0x00740330 | 40 | — | read only by unported (software) code: 0x535ed0 |
| 0x00740334 | 0 | — | no direct read |
| 0x0074639c | &0x74039c | `draw_buffers().cam` | own buffer |
| 0x007463a0 | &0x74339c | `draw_buffers()` normals | own buffer |
| 0x00747ca4 | 0 | 0 (`poly_normals`, `vertex_normals`) | same |
| 0x00749808 | 0 | 0 (`vertex_shading`) | same |
| 0x0074980c | 0 | null (`lights.nodes`) | same |
| 0x00749810 | 0 | null (`lights.data`) | same |
| 0x00749814 | 0 | 0 (`lights.any`) | same |
| 0x00749818 | 0 | 0 (`lights.in_count`) | same |
| 0x0074981c | 0 | 0 (`lights.count`) | same |
| 0x0074a224 | 0 | — | no direct read |
| 0x0074a228 | 1 | — | no direct read |
| 0x0074a22c | 1 | 0 (`fog().on`) | differs; the world read sets it before any draw |
| 0x0074a230 | 1 | — | no direct read |
| 0x0074a234 | 1 | 0 (`fog().color[0]`) | differs; the world read sets it |
| 0x0074a238 | 0 | 0 (`color[1]`) | same; the world read sets it |
| 0x0074a23c | 1 | 0 (`color[2]`) | differs; the world read sets it |
| 0x0074a240 | 500 | 0 (`near_range`) | differs; the world read sets it |
| 0x0074a244 | 250000 | 0 (`near_sq`) | differs; the world read sets it |
| 0x0074a248 | 700 | 0 (`far_range`) | differs; the world read sets it |
| 0x0074a24c | 490000 | 0 (`far_sq`) | differs; the world read sets it |
| 0x0074a250 | 0.005 | 0 (`inv_range`) | differs; the world read sets it |
| 0x0074a254 | 300 | 0 (`alt_high`) | differs; the world read sets it |
| 0x0074a258 | 200 | 0 (`alt_low`) | differs; the world read sets it |
| 0x0074a25c | 0.01 | 0 (`inv_alt`) | differs; the world read sets it |
| 0x0074a260 | 2 | — | no direct read |
| 0x0074a264 | 0 | 0 (`shadows`) | same |
| 0x0074a268 | 0 | 0 (`overlay`) | same |
| 0x0074a26c | 1 | 1.0 (`opacity`) | same |
| 0x0074a270 | 0 | 0 (`tint_on`) | same |
| 0x0074a284 | 0.01 | 0.02 (`priority_scale`) | same (Direct3D) |
| 0x0074a288 | &0x74039c | a local scratch (gmod_const) | own buffer |
| 0x0074a28c | &0x74339c | a local scratch (gmod_hit) | own buffer |
| 0x0074a290 | &0x74039c | `cull.points` | own buffer |
| 0x0074a294 | &0x74339c | `cull.normals` | own buffer |
| 0x0074a298 | 0 | 0 (`cull.count`) | same |
| 0x0074a2a4 | 0.2 | — | no direct read |
| 0x0074a2a8 | 0.2 | — | no direct read |
| 0x0074a2ac | 0 | 0 (`marks.on`) | same |
| 0x0074a2b0 | 0 | 0 (`marks.type`) | same |
| 0x0074a2b4 | 0 | null (`marks.maps`) | same |
| 0x0074a2b8 | 0 | — | no direct read |
| 0x0074a2bc | 0 | — | no direct read |
| 0x0074a2c0 | 0 | 0 (`marks.uv`) | same |
| 0x0074a2c4 | 0 | — | read only by unported (software) code: 0x542060, 0x542730 |
| 0x0074a2c8 | 0xffffffff | 0 (`mask_base`) | differs, unread: the start-up's 0x526750 points the mask at GfxFlags_HW |
| 0x0074a2cc | 0x597d10() | `&mask_base` | set by 0x526750 (GfxFlags_HW) |
| 0x0074a2e0 | 0x53e050 | 0x53f2e0 (`walk_state().draw`) | same after the reset 0x526800 (Direct3D) |
| 0x0074a324 | &0x74a2e4 | 0 (`init().top`) | same |
| 0x0074a32c | 0 | 0 (`second_pass`) | same |

### 0x005578f0 zRender start

| Global | Exe | Port | Read on a path the port runs? |
|---|---|---|---|
| 0x0074a3c0 | 0 | not ported (the flare's depth rescale) | identity: only the software draw 0x53e050 writes it |
| 0x0074a3c4 | 1 | not ported (the same) | identity: only 0x53e050 writes it |
| 0x0074a3c8 | 1 | 0 (`soft_target().winding`) | differs, unread: each ported filler call sets it (0x4431fa, the shadows) |
| 0x0074a3cc | 1 | — | no direct read |
| 0x0074a3e0 | 0 | — | read only by unported (software) code: 0x53d750, 0x55bbf0, 0x55c2f0, 0x55c900 ... |
| 0x0074a3e4 | 0 | — | read only by unported (software) code: 0x55a740, 0x564370, 0x564460, 0x5646d0 ... |
| 0x0074a3e8 | 0 | — | read only by unported (software) code: 0x564370, 0x564460, 0x5646d0 |
| 0x0074a3ec | 0 | — | read only by unported (software) code: 0x564710 |
| 0x0074a3f0 | 0 | — | read only by unported (software) code: 0x564710 |
| 0x0074a3f4 | 0 | — | read only by unported (software) code: 0x564710 |
| 0x0074a3f8 | 0 | — | no direct read |
| 0x0074a3fc | 0 | — | read only by unported (software) code: 0x558010, 0x564370, 0x564460 |
| 0x0074a404 | 0.0001 | — | read only by unported (software) code: 0x564710 |
| 0x0074a408 | 1.0001 | — | read only by unported (software) code: 0x558170, 0x564710, 0x565d30 |
| 0x0074a40c | 0.9999 | — | read only by unported (software) code: 0x558170, 0x564710 |
| 0x0074a758 | 0x20 | — | read only by unported (software) code: 0x558080 |
| 0x0074a75c | 0x20 | 0 (`soft_target().screen_w`) | differs, unread: only the software span fillers read screen_row |
| 0x0074a760 | 5 | — | no direct read |
| 0x0074a764 | 32 | — | read only by unported (software) code: 0x5580d0, 0x55f000, 0x5608f0, 0x561910 |
| 0x0074a768 | 0.00333 | — | read only by unported (software) code: 0x558140, 0x566070 |
| 0x0074a76c | &0x74a75c * &0x8007d8 | 0 (`screen_row`) | differs, unread (the same) |
| 0x0074a770 | 0 | — | read only by unported (software) code: 0x55f000, 0x5608f0, 0x561910 |
| 0x0074a77c | 0 | — | read only by unported (software) code: 0x5662d0, 0x566620, 0x566890, 0x566c90 ... |
| 0x00797b70 | 0 | — | read only by unported (software) code: 0x5662d0, 0x566620, 0x566890, 0x5670a0 |
| 0x007fb2dc | 0 | 0 (`tint.pending`) | same |
| 0x007fb300 | 0 | 0 (`flares.count`) | same |
| 0x00800444 | 0 | 0 (`flares.picked_count`) | same |
| 0x008007c8 | 0 | null (`soft_target().base`) | set by 0x557f90 each frame |
| 0x008007cc | 0 | 0 (`width`) | set by 0x557f90 |
| 0x008007d0 | 0 | 0 (`height`) | set by 0x557f90 |
| 0x008007d4 | 0 | 0 (`pitch`) | set by 0x557f90 |
| 0x008007d8 | 1 | 2 (`bytes`) | differs, set by 0x557f90 from the depth (16 bits) before a fill |
| 0x008007dc | 1 | — | no direct read |
| 0x008007e0 | 1 | — | no direct read |
| 0x008007e4 | 0 | — | no direct read |
| 0x008007e8 | 0 | — | no direct read |
| 0x008007ec | 0 | — | no direct read |
| 0x008007f0 | 0 | — | no direct read |
| 0x008007f4 | 0 | — | no direct read |
| 0x008007f8 | 0 | — | no direct read |
| 0x008007fc | 0 | — | no direct read |
| 0x00800800 | 0 | — | no direct read |
| 0x00800804 | 0 | — | no direct read |
| 0x00800808 | 0 | — | read only by unported (software) code: 0x55f000, 0x5608f0 |
| 0x0080080c | 0 | — | no direct read |
| 0x00800810 | 1 | — | no direct read |
| 0x00800814 | 1 | — | read only by unported (software) code: 0x557f70, 0x563940 |
| 0x00800818 | 0 | — | no direct read |
| 0x00800824 | 0x55b3c0 | the 16-bit blit (`ztwod::Host`) | own (the 2D blit) |
| 0x008008d4 | 0xffffffff | the render mask base | unread: 0x557ae0 points it at GfxFlags_HW |
| 0x008008d8 | 0x597d10() | the render mask | set by 0x557ae0 |

### 0x00557b30 zRender reset

| Global | Exe | Port | Read on a path the port runs? |
|---|---|---|---|
| 0x0074a774 | 0x200 | — | read only by unported (software) code: 0x55f000, 0x5608f0, 0x561910 |
| 0x0074a778 | 0.1 | — | read only by unported (software) code: 0x55f000, 0x5608f0, 0x561910 |
| 0x00800828 | 0x564080 | `span_fill16` (0x564080) | same |
| 0x0080082c | 0x5640c0 | — | read only by unported (software) code: 0x55c900 |
| 0x00800830 | 0x56cb40 | — | no direct read (the software span table, read by index) |
| 0x00800834 | 0x56cbc0 | — | no direct read (the software span table, read by index) |
| 0x00800838 | 0x56cc40 | — | no direct read (the software span table, read by index) |
| 0x0080083c | 0x56ccc0 | — | no direct read (the software span table, read by index) |
| 0x00800840 | 0x56cd40 | — | no direct read (the software span table, read by index) |
| 0x00800844 | 0x56cdc0 | — | no direct read (the software span table, read by index) |
| 0x00800848 | 0x56ce40 | — | read only by unported (software) code: 0x55f000 |
| 0x0080084c | 0x56cec0 | — | no direct read (the software span table, read by index) |
| 0x00800850 | 0x56d2c0 | — | no direct read (the software span table, read by index) |
| 0x00800854 | 0x56d340 | — | no direct read (the software span table, read by index) |
| 0x00800858 | 0x56d3c0 | — | no direct read (the software span table, read by index) |
| 0x0080085c | 0x56d440 | — | no direct read (the software span table, read by index) |
| 0x00800860 | 0x56d4c0 | — | no direct read (the software span table, read by index) |
| 0x00800864 | 0x56d540 | — | no direct read (the software span table, read by index) |
| 0x00800868 | 0x56d5c0 | — | no direct read (the software span table, read by index) |
| 0x0080086c | 0x56d640 | — | no direct read (the software span table, read by index) |
| 0x00800870 | 0x56dab0 | — | no direct read (the software span table, read by index) |
| 0x00800874 | 0x56db50 | — | no direct read (the software span table, read by index) |
| 0x00800878 | 0x56dbf0 | — | no direct read (the software span table, read by index) |
| 0x0080087c | 0x56dc90 | — | no direct read (the software span table, read by index) |
| 0x00800880 | 0x56dd30 | — | no direct read (the software span table, read by index) |
| 0x00800884 | 0x56ddd0 | — | no direct read (the software span table, read by index) |
| 0x00800888 | 0x56de70 | — | no direct read (the software span table, read by index) |
| 0x0080088c | 0x56df10 | — | no direct read (the software span table, read by index) |
| 0x00800890 | 0x5685c0 | — | read only by unported (software) code: 0x55e310 |
| 0x00800898 | 0x5685c0 | — | read only by unported (software) code: 0x55d660 |
| 0x0080089c | 0x5685c0 | — | read only by unported (software) code: 0x55d660 |
| 0x008008a0 | 0x5689d0 | — | read only by unported (software) code: 0x55d660 |
| 0x008008a4 | 0x5689d0 | — | read only by unported (software) code: 0x55d660 |
| 0x008008a8 | 0x569a80 | — | read only by unported (software) code: 0x561910 |
| 0x008008ac | 0x56abd0 | — | read only by unported (software) code: 0x561910 |
| 0x008008b0 | 0x5685c0 | — | read only by unported (software) code: 0x55e310 |
| 0x008008b4 | 0x5689d0 | — | read only by unported (software) code: 0x55e310 |
| 0x008008b8 | 0x5685c0 | — | read only by unported (software) code: 0x55e310 |
| 0x008008bc | 0x5689d0 | — | read only by unported (software) code: 0x55e310 |
| 0x008008c0 | 0x563b80 | direct calls (zrndr_prim2d) | own (0x563b80 the same) |
| 0x008008c4 | 0x563c50 | — | read only by unported (software) code: 0x562c80 |
| 0x008008c8 | 0x563db0 | direct call to 0x563db0 | own |
| 0x008008cc | 0x56c830 | — | read only by unported (software) code: 0x55f000, 0x560010 |
| 0x008008d0 | 0x56cf40 | — | no direct read |
