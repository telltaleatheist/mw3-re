# The scene walk of a frame (0x4f7030 down to the model draw)

Mech3.exe 1.2, 2026-10-19. Draft for `docs/render.md` section 1. Check:
`tests/differential/test_scenewalk.py` (probe `tests/probe_scenewalk.cpp`).

## What is ported, and where

| Exe | Port | File (func2file) |
|---|---|---|
| 0x4f7030 world render | `zcls::render_world` | after camera.c, before list.c: kept in `zclass/camera.cpp` |
| 0x4f6d40 horizon follow | `horizon_follow` | same |
| 0x4f6c50, 0x4f6c00, 0x4f6720 | `render_scene`, `render_world_children`, `render_cells` | camera.c |
| 0x4f61b0, 0x4f5c80 partition pickers, 0x4f5a90 hull, 0x4f5a00 angle | `pick_cells_virtual`, `pick_cells`, `view_hull` | camera.c |
| 0x4f5910 per node; 0x4f4580 camera, 0x4f4730 sound, 0x4f4900 light, 0x4f4ab0 object3d, 0x4f4ee0 animate, 0x4f5090 LoD, 0x4f56c0 sequence, 0x4f57e0 switch | `render_node` and `render_*` | camera.c |
| 0x4f4170, 0x4f41b0, 0x4f4370 camera matrices; 0x4f3610, 0x4f36b0, 0x4f3870, 0x4f3760, 0x4f3ff0, 0x4f4090, 0x4f4100 setters | `camera_update(_offset)`, `camera_matrices`, `camera_set_*` | camera.c |
| 0x4ed280 node to world, 0x4ebdb0 box corners (with 0x522ab0's corner sums inline) | `node_to_world`, `node_box_corners` | class.c, new `zclass/class.*` |
| 0x4f9c40 corners to sphere | `corners_sphere`, `approx_sqrt` | between cls_util.c and window.c: new `zclass/cls_util.*` |
| 0x4fa020, 0x4fa130 window size / origin | `window_size`, `window_origin` | window.c, new `zclass/window.*` |
| 0x4f1f10, 0x4f1fe0, 0x4f2060 cells | `world_wrap_cell`, `world_cell_wrapped`, `world_cell_clamped` | cls_world.c |
| 0x4fc020, 0x4fc0b0 translation | `object3d_get/set_translation` | object3d.c |
| 0x526a10..0x526a90, 0x526b90, 0x526bf0, 0x526e10 viewport, 0x5271a0 / 0x5272b0 planes | `zmod::set_tint` .. `set_camera`, `camera_local/world_planes` | gmod_init.c (the last three just after it): new `zmodel/gmod_init.*` |
| 0x541760 cull, 0x53cdc0 / 0x53cc40 zones, 0x54fe80 / 0x54ff40 view polygon, 0x5438d0 / 0x5438f0 caster lists | `zmod::cull_sphere`, `zone_visible`, `zone_reset`, `set_view_polygon`, `inside_view_polygon`, `use_*_casters` | unknown (gmod_const..gmod_cookie, gmod_puff..zreader): new `zmodel/gmod_cull.*` |

The model draw is called through `zmod::walk_state().draw` ([0x74a2e0],
default `zmod::draw_model`). The world's lights use the ported
`zmod::world_build_lights` / `world_update_lights`.

Hooks (not ported; null skips): `zcls::world_render_hooks()` BeginScene /
EndScene 0x579af0 / 0x579b10, the flushes [0x72f3e4] / [0x72f3e8], puffers
0x5496c0, 0x5a67c0, world sounds 0x4f3170, the zone probe 0x4eda10 (a ray
down through the world), dirty rects 0x576ce0, the sound listener 0x571e50 /
0x572360; `zcls::update_hooks()` the changed-node update 0x4f7e00 and the
frame update 0x4f7f70; `zmod::cull_hooks()` the hidden-surface test
0x53d750 (software span buffer). The shadow casters 0x543720 / 0x543760 /
0x543810 are the cull hooks' defaults now (render-shadows.md); the test
runs them for real on both sides and logs their lists. Software path 0x4f6dc0 and the cockpit pass 0x4f73f0 are not
ported (same pieces, other calls).

## How the walk works (corrections to render.md section 1)

- **EDX of 0x4f5910 is not a mode.** Every caller leaves its own count in
  EDX: a node's children get the parent's child count, cell nodes the cell's
  node count, fade entries the fade count; only the world's direct children
  get 2 (0x4f6c29). The class renderers cull only when the plane mask is
  non-zero **and EDX > 1**, so an only child is never culled on its own (it
  inherits its parent's mask and the model draw clips it). Most likely the
  source called the renderer without its second argument (K&R C).
- **A cell's mask is its cull result**, not 0x3f: the picker culls the
  cell's sphere (flags 0x100; else the 2D point with radius -tolerance) and
  stores the planes it straddles; wrapped cells keep 0x3f.
- **Object3d without a box** (flag 0x100 clear): when its sphere is due
  (`+0x2c` bit 0, a moved parent, or not tree-valid) and it is a cull
  candidate, 0x4f4ab0 returns before drawing: the node and its subtree are
  skipped. Camera, light and sound renderers do not check: they build the
  sphere from whatever the corner buffer held.
- **Matrix cache**: the object3d's stack slot is its data +0x60. Tree-valid
  and clean (data 0x20 clear): pushed as is; dirty: copy, times +0x30, clean;
  not tree-valid: copy, times +0x30 every frame. Data flag 8: no push.
  Rebuilding sets [0x5ea530] so the children redo their spheres.
- **Hidden-surface test** (0x53d750): object3d only, when [0x5ea518], ring
  > 0, EDX != 1, no override open and the model has no lights; cells: ring
  > 0. Ring 0 is never tested.
- **Partition picker 0x4f61b0**: the camera's cell (floor, then wrapped)
  gives the visibility mask and the ring centre; the footprint is the
  camera position plus the two far corners `(+-a, b, -far)` (b < 0, the
  bottom ones), or all four when |pitch| or |roll| > 10 degrees (then hulled),
  transformed by `Rz Rx Ry T` built from the camera's euler (+0x38..+0x40,
  pitch and roll left out when small). Rings by `|dz| + |dx|` of the
  unwrapped cells. 0x4f5c80 (world +0x50 == 0) clamps instead of wrapping
  (0.1 inside the grid, truncated) and is not reached by the shipped worlds
  (the test forces it).
- **Lights per cell**: a light gets 0x200 (skip) for the cell unless it has
  flags 0x10 and 0x2 and `(radius + range)^2 > d^2`; after the walk every
  light gets 0x200.
- **Fade entries** compare the **squared** distance (0x51dfa0 returns it
  squared) with their near/far.
- **Horizon follow**: object3d +0x54 is the translation row of its +0x30
  matrix, so setting it moves the sky at once; 0x4f7f70 (the frame update)
  then runs for the changed list.
- **Zones**: mode [0x5ea1d0] is 1 by default. A camera with +0x1e0 set and
  mode 1 uses its own list +0x1e4; otherwise 0x4eda10 probes the zone under
  the camera, and no hit resets the list to "count 0" (everything visible).
- **Camera matrices**: `+0x74 = -row 2`, the offset of a wrapped cell is
  added to the translation and kept at 0x73fbb0, the euler is taken from the
  stack top, the top is left the identity. Far corners and planes (0x5271a0:
  +x turned by half the h-fov about y and mirrored, -y by half the v-fov about
  x and mirrored, near (0,0,-1), far (0,0,1)) confirmed bit for bit.

## Quirks

- Mask stack 0x74a2e4: 16 entries, unchecked (the 17th overwrites the stack
  pointer itself); LoD stack 8, opacity 50, colour 32, the node chain in
  0x4ed280 13: all unchecked. The port's arrays are larger (comment at each).
- The view polygon lives in the model draw's vertex buffers ([0x74a290] =
  0x74039c, [0x74a294] = 0x74339c): valid only until the first model draws.
- LoD morph: [0x60bc20] is cleared at the end of any LoD, so a nested LoD
  ends its parent's morph for the siblings that follow.
- LoD distances and spheres use `bits/2 + 0x1fc00000` (up to ~6% off).
- 0x4f1f10 left of the grid counts back from the block's width without the
  block start (+0x5c / +0x60), right of it adds the start.
- 0x53cdc0 reads as many ids as the count byte says, past the 4 bytes.
- 0x4f61b0 also swaps the two wrapped-row outputs, which it never uses.
- Switch renderer: child i tests bit `i mod 32` (x86 shift).
- 0x526e10 nudges the right/bottom edge +0.001 on Direct3D (software:
  +0.5 for centres, -0.999 / +1.499 elsewhere).
- 0x4fc0b0 has no class check; NaN translations keep the identity flag.

Candidates for `docs/discoveries.md` (not added here): only-children are
never culled by themselves; ring 0 is never hidden-surface tested; nodes
without a bounding box vanish with their subtree when culling applies; past
the map edge only "terrain" nodes (flag 0x8000) are drawn, by moving the
camera; the sky follows the camera by having its matrix translation set.

## Check

`test_scenewalk.py`: the original reads each world (0x4ff520) in Unicorn at
24-bit x87, the port reads it with its loaders. Per world 40 cameras set
with the original's setters (0x4f36b0, 0x4f3870, horizon 0x4f4090, sometimes
0x4f3b90 / 0x4f3a70 / 0x4f3ff0) and the port's; then 0x4f7030 on both. The
model draw is a recorder (node, mask, stack top, opacity, overlay, shadows,
tint, morph); the hidden-surface test hides on a coarse grid of the centre
(same rule both sides); caster records over chosen nodes go through the
real caster lists (0x543720 / 0x543760 / 0x543810) on both sides, which log
what they leave (up to 300 casters, so the 200 cap is reached). Variations:
HSE on/off, dynamic LoD, LoD scale 0.6, shadows with
casters (and per-cell lists), zone lists, zoom and clip changes, the
non-virtual picker, cameras up to 400 units outside the map (wrapped cells),
pitch and roll beyond 10 degrees. Compared: every event in order, the
camera data (0x1e8 bytes), the view matrix and the ring tables. Every third
node loses flag 0x8000 so wrapped cells differ. Plus 400 runs of 0x4ebdb0 +
0x4f9c40 and 3000 of the hull 0x4f5a90 (grid points for ties), bit for bit.

Result: 240 frames, 10,446 model draws and 6,277 hidden-surface calls in
order; node, mask and state exact, matrices 99.5% bit-exact (the rest within
1e-7 relative: zmth sums), camera data and rings exact.

Breaks (each caught): children given EDX 2; far plane never tested;
straddle test widened; LoD ignoring camera +0xd4; ring distance from the
wrapped cell; wrapped cells drawing all nodes; hidden-surface test in ring
0; exact square root for the sphere; hull tie-break reversed; horizon y
not followed; far corner sign; the cached matrix ignored.

Not reached by the shipped worlds: camera, sound, animate, sequence and
switch nodes; LoD grow-in and morph (fields zero); override (0x800000)
nodes; fade entries; the zone probe's hit path; the sound listener
(the test keeps camera flag bit 0 off); dirty rects (one camera); the
software viewport branch.

## For fixes.md / integration

- The probe stubs the frame update; the node update runs for real on both
  sides. In the game `update_hooks()` is the port (`action-lists.md`).
- `zmth_matrix.cpp` has no `FP_CONTRACT OFF`: on arm64 its sums are fused,
  the cause of the 0.5% of matrices that are not bit-exact (and of any
  cull flip at a plane, none seen).
