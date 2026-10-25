# Drawing a frame of the 3D world

Mech3.exe 1.2, 2026-10-19. How the original draws the mission world through
Direct3D 6, mapped for porting onto the SDL3 GPU front end. Addresses are the
exe's; "ECX/EDX" are MSVC fastcall registers (Ghidra drops them, so they come
from `analysis-output/mech3.asm`). **[unverified]** marks claims read from the
pseudocode or asm only, without an emulator run or a cross-check.

Emulator check of the conventions: `tests/differential/explore/render_matrix.py`
(euler, multiply, view matrix, projection; all match the formulas below).

## Ported so far

- Camera-space near/far clippers (0x5275e0 .. 0x529120): `zmodel/gmod_clip3d`,
  `render-clip3d.md`. Corrections: 0x527a30 is the untextured *lit* path and
  0x5286e0 the per-vertex *lit* textured one; the plane mask is read from
  [0x7402f8], EDX is the count pointer; the point light is copied, not
  interpolated, at a near cut.
- Screen-space side clippers (0x529ce0 .. 0x52fab0): `zmodel/gmod_clip2d`,
  `render-clip2d.md`. The clip struct is the address 0x7402f8 itself; each
  clipper recomputes its outcodes; order left, right, top, bottom, right and
  bottom exclusive; crossing points are perspective-correct for every
  attribute.
- Fog maths (0x555640, 0x555a70) and the transparent queue, sort and flush
  (0x57dec0, 0x57e110): `zmodel/gmod_fogcalc`, `zvideo/zvid_ddd3d`,
  `render-fogsort.md`. The model-level fog test is a conservative bound over
  the sphere; the flush always sends ALPHAOP MODULATE and forces Gouraud.
- The maths module (matrix stack, euler, view, projection, project and
  unproject, morphs, normals, the model draw's vertex transform):
  `zmath/zmth_matrix`, `render-matrix.md`. Section 3 step 2: the top of the
  stack is the model-view matrix built by 0x51fb30, so vertices go straight
  into camera space. Matches at 64 bits within 1e-6; at 24 bits 0.6% of cases
  drift beyond that (rounding only).
- The Direct3D draw table (0x57bf80, 0x57c700, 0x57ce20, 0x57d160, 0x57d690),
  the state cache, texture records (0x57b130), frame calls and the overlay
  flush, onto platform::Render3D: `zvideo/zvid_ddd3d`, `render-drawtable.md`
  (corrections to section 4's overlay types there).
- Lighting: the light list (0x543b60), lights reaching a model (0x543d90),
  the model needs (0x53daa0), polygon shading (0x545720), the cloud pattern
  setters and sampler, light directions (0x4fb310, 0x4fb0c0):
  `zmodel/gmod_light`, `render-lighting.md`. Correction to section 5: model
  flag 0x20 also drops the sun's diffuse term (the sun gives min(1, ambient)).
  Not ported: 0x544ed0, the baked static light (the sun reaches cached models
  only through it), and the software lighting 0x544060.
- The Direct3D model draw 0x53f2e0: default models, points, facades (modes
  0 and 1), morphs, per-polygon reversal, backface test, strips, material
  cycles, both clips, projection with the priority bias, the second pass,
  and the draw-table calls: `zmodel/gmod_draw`, `render-modeldraw.md`.
  Identical on all six worlds (18,000 draws). Still hooked: the light bake,
  lens flares (projected lights and shadows: render-shadows.md).
- The baked static light (0x544ed0) and the world texture loading (texture
  groups 0x519080, entries 0x518f70, image fetch, mip links) into the draw
  table's records: `zmodel/gmod_bake`, `zimage/zimg_texture`,
  `render-bake-textures.md`. The model draw's table is filled by
  `zvid::install_model_draw_table` (0x579f50's part).
- The scene walk: world render 0x4f7030, the horizon follow, the walk and
  per-node function 0x4f5910 with all eight class renderers, partition
  pickers and the hull, LoD, camera matrices and setters, cull and zones:
  `zclass/camera`, `class`, `cls_util`, `window`, `zmodel/gmod_cull`,
  `gmod_init`, `render-scenewalk.md`. Corrections: EDX of 0x4f5910 is the
  caller's child count, not a mode (only-children are never culled alone);
  a cell's plane mask is its cull result, not 0x3f.
- Projected shadows and projected lights on the Direct3D path: the caster
  lists (0x543720, 0x543760, 0x543810), the model draw's receiver test
  (0x540552..), the shadowed draws (0x57eeb0, 0x57f2a0, 0x57f860), the
  projected lights' subdivision (0x547540, 0x547740, 0x547060, 0x547160)
  and their draws (0x57ea20, 0x57fe20), as the `CullHooks` / `DrawHooks`
  defaults and table entries: `zmodel/gmod_cull`, `gmod_draw`, `gmod_light`,
  `zvideo/zvid_ddd3d`, `render-shadows.md`. Multi-texture draws go to
  `Render3D::draw_multi` (FIX (A)). Corrections there: [0x72f3cc] /
  [0x72f3dc] are the projected-light draws, not puffers; the casters are
  the mechs near the player (0x4814e0); at 24 bits the caster images are
  blank and projected-light triangles black (emulator).
- The casters and their images: records, the image builder 0x542e70 with
  its node walk (0x5432d0, 0x4f7670, 0x4f7750), 0x543330, the polygon filler
  0x55d010 and the blur; uploads through 0x57b130 / 0x57baa0 / 0x57bb00 into
  `Render3D` (`update_texture` new); the per-mech upkeep 0x4814e0 (mech3):
  `zmodel/gmod_cull`, `zclass/camera`, `zrender/zrndr_span`,
  `zvideo/zvid_ddd3d`, `mech3/app_states`, render-shadows.md. Identical to
  the original at 24 bits (blank images, the default) and with the filler at
  53 bits (`shadow_state().precision`; k53 the default: in play the shadows
  show, Owen 2026-10-20). Correction:
  0x51fee0 sums column 2 and the translation's z in another order
  (`zmth::multiply` now bit-exact).
- Lens flares, the Direct3D path (ported 2026-10-25, item 18:
  `zrender/zrndr_flare`, `zclass/camera` camera_draw_flares, gmod_draw's
  model light): a model light with +0x3c joins a list as the scene is walked
  (0x567420 from 0x541df0: its world point, the camera-space point through
  0x73fc00; its screen point; the colour's low word; the light's +0x30), at
  most 650, cleared at the frame's start (0x567570; 0x5674d0 its count).
  **The Direct3D frame draws them**: 0x481790 calls 0x4f7350 at 0x481954
  (correcting this page and fixes.md, which had the Direct3D frame only
  clearing the list): after the cockpit passes and the camera refresh
  0x4b51f0, with the cockpit node's INTERSECT on around it (0x4eb6d0), it
  begins a scene, picks the frame's flares (0x567650: the light's +0x3c on,
  among the first 64, a depth), casts a first-hit segment from the camera's
  position to each one's world point (0x4eed90, kind 0x40000 left out) and
  draws the ones nothing blocks (0x567d30), then flushes the transparent
  queue and ends the scene. 0x567d30: d = near (0x73fb80) / depth; full
  below the light's +0x44, fading to nothing at +0x48. 0x567b90: four
  squares through 0x567720 (the puffers' sprite, alpha 0.5, white, depth
  0.99), the base size width x scale / 16 (the width from the clip rect's
  left to the point rect's right): texture 0 on the light, 1 halfway to the
  screen's centre, 2 a tenth of the way (half size), 3 at (cx - dx, cy + dy)
  (1.5 times: mirrored through the centre in x only, as written). The
  textures are `LensFlareTexture` 0..3 (0x5676d0; every world's .gw sets
  lflare1..4); without all four nothing is drawn. The software frames
  (0x481290, 0x481a10, 0x443320) draw the list their own way (0x5675e0,
  0x567dc0; not ported). The worlds' flaring lights (+0x40, +0x44, +0x48):
  c1, c2, c4 and t1 one each (5, 30, 5000), c3 two (30, 20, 640), c4b none.
  Checked: `test_flares.py`, `mission_c4m2_sun_flare_drawn_and_blocked_headless`
  (c4 mission 2 starts facing the sun: drawn; an AI mech on the line hides
  it). The cockpit's INTERSECT is on around the draw as in the exe, but over
  a full turn in c4m2 the sight line never met the cockpit's models in the
  port (its node, `mech_madcat_hud.flt`, holds no model; the canopy sits in
  its children) **[open: whether the exe's ever does]**.
- The frame around the scene: the back buffer's lock for 2D on top
  (0x576d00 / 0x576d60 over 0x577590 / 0x577600), the scene nesting
  (0x579af0 / 0x579b10), the colour quad (0x57dd80, [0x72f3ec] = 0x57e380,
  now called by the scene walk after the overlay flush) and the software
  tint (0x558540, the finish 0x558610 and its rows): `zvideo/zvid_dd`,
  `zvid_ddd3d`, `zrender/zrndr_tint`, `render-frame.md`.
- The clippers, fog and sort match the original in Unicorn with the FPU at
  24-bit precision, as the game runs (CLAUDE.md).

## 0. Conventions in one place (for the viewer)

- **Units and axes.** World +Y is up (gravity -9.8 in anim.zbd, `anim.md`;
  terrain cells span X and Z). Right-handed, OpenGL-like: a camera with zero
  rotation looks along **-Z**, +X is screen right, +Y screen up.
- **Matrices** are 3x4, 12 floats, **row vectors**: rows 0..2 are the node's
  local x, y, z axes expressed in the parent, row 3 (floats 9..11) the
  translation. `p_parent = p_local * M` (`p' = x*row0 + y*row1 + z*row2 + row3`).
- **Composition**: `world = local * parent_world` (local applied first), built
  root to leaf on a matrix stack (0x51fee0: `top = L * top`).
- **Euler** (0x521670, object3d `rotate` +0x18 = (a, b, c)): with row-vector
  rotations `Rx(a) = [[1,0,0],[0,c,s],[0,-s,c]]`, `Ry(b) = [[c,0,-s],[0,1,0],[s,0,c]]`,
  `Rz(c) = [[c,s,0],[-s,c,0],[0,0,1]]`,
  **R = Rz(c) * Rx(a) * Ry(b)** (roll about Z first, then pitch about X, then
  yaw about Y); scale multiplies the rows: `row_i *= scale[i]` (= S*R). So
  `M = S * Rz * Rx * Ry` then translation in row 3. Written out
  (ca = cos a, ...):
  ```
  row0 = (cc*cb + sa*sb*sc,  ca*sc,  sa*cb*sc - cc*sb) * sx
  row1 = (sa*sb*cc - sc*cb,  ca*cc,  sb*sc + sa*cb*cc) * sy
  row2 = (ca*sb,             -sa,    ca*cb)            * sz
  row3 = translation
  ```
  The inverse (0x523bc0, matrix -> euler): a = asin(-m[7]), b = atan2(m[6], m[8]),
  c = atan2(m[1], m[4]); at |m[7]| >= 1: a = -copysign(pi/2, m[7]), b = atan2(-m[2], m[0]), c = 0.
- **View matrix** (0x520b00 into 0x73fbc0): from the camera's world matrix W,
  negate rows 1 and 2 (y and z axes), then invert the rigid transform:
  `V3x3 = transpose(W'3x3)`, `Vt = -t * V3x3`. Camera space is therefore
  **x right, y down, z forward (into the screen)**, still right-handed (two
  negated axes = a 180 degree turn about x).
- **Projection** (0x5234f0 software, 0x523540 Direct3D), per camera-space point:
  `sx = cx + x * fx / z`, `sy = cy + y * fy / z`, with
  `fx = cot(hfov/2) * (w/2)`, `fy = cot(vfov/2) * (h/2)`,
  `cx = win_x + w/2`, `cy = win_y + h/2` (0x521c60). Third value: software
  `1/z`; Direct3D `near/z` (= rhw scaled so the near plane is 1).
  hfov and vfov are independent (no aspect correction); the shipped camera
  is 80 x 60 degrees (camera data +0xe0/+0xe4 = 1.39626, 1.0472), near 0.1,
  far 800, so non-square scaling if the window is not 4:3.
  Depth value written to D3D: `sz = rhw = near/z` (times the priority
  bias, section 3), **reversed depth**: 1 at the near plane, toward 0 far
  away; the Z buffer is cleared to 0 and tested GREATEREQUAL (section 4).
- **Winding**: front faces are **counter-clockwise in file order** seen
  from the front, i.e. the right-hand normal `(v1-v0) x (v2-v0)` of the
  file's vertex order points at the viewer (OpenGL's default). Checked on
  the data: 97% of c1's terrain polygons get +Y that way, and all 72 sky
  dome polygons point at the dome's centre. The game culls itself
  (D3D CULLMODE is NONE); details in section 3.

## 1. Call chain, game loop to DrawPrimitive

```
zGameApp::Run 0x58fd70                      main loop, state stack (architecture.md)
 Core State (0x608ec0, vtable 0x5c4040) vt+0x10 = 0x481e70   one pass per frame
   render strategy  [state+0x1c]->vt[0](player_mech, draw_cockpit)   made by 0x480bf0:
       0x481790  Direct3D        ([0x72f348] != 0, vtable 0x5c4070)
       0x481290  software        (vtable 0x5c4068)
       0x481a10  software, 0x40d4b0() != 0 (vtable 0x5c406c)  [unverified: what 0x40d4b0 is]
   then [state+0x20]->vt[0](dt)   (the simulation step runs after the draw)
   0x576ea0(0,0)  present: [0x80932c] (flip), frame counter [0x72f338] += 1
```

0x481790 (Direct3D frame):

1. viewport/rect setup (0x40d6a0, 0x40d750, 0x40d7b0, 0x576cf0), 0x4814e0
   (the mechs' shadow images, render-shadows.md), 0x557f90 2D target.
2. `[0x5f4c8c]->vt+4` = 0x4b52c0: the player view object (ctor 0x40aca0 /
   0x4b5060: +4 camera node, +8 window wrapper, +0xc world wrapper,
   +0x14 = [0x72f348] hardware flag). Hardware: `0x4f7030(ECX camera, EDX 0)`;
   software: `0x4f6dc0(camera, 0, 1)`.
3. HUD/2D: 0x4a8cd0 (the sensors, not drawing: ai.md 15), 0x48f470, 0x4783a0, 0x40f8d0, 0x5a6790/0x5a67c0,
   0x579af0 BeginScene, `[0x72f3ec]` (0x57e380, one queued 4-vertex quad),
   0x579b10 EndScene.
4. If draw_cockpit (ported 2026-10-21 as `zcls::render_subtree`, the Direct3D
   path, checked in test_scenewalk.py: 173 subtree draws identical on the six
   worlds): three more `0x4f73f0(camera, subtree, hw)` passes, each a
   full BeginScene..flush..EndScene of its own: the pilot's cockpit node
   (`mech+0x684 +0x228 -> +4`) and `mech+0x394 / +0x398 -> +8`
   **[unverified: likely the arm/weapon models seen from the cockpit]**, with
   0x526a70 toggled around the first.
5. 0x4b51f0 (the view's camera refresh), 0x4f7350 (the lens flares, ported), HUD (0x40fac0, 0x40e4b0, 0x41ab90 or
   `[0x608f74]->vt+4`), 0x43cd20.

### 0x4f7030 world render (ECX camera node, EDX 0)

```
0x51e9d0 push matrix (local)            camera data = [0x60bb9c] = node+0x38
world = camera data +0, window = camera data +4
dynamic LOD (if [0x5ea51c], CameraSetDynamicLOD): factor [0x5ea524] -= 0.05
   when frame time [0x72c52c] > target [0x5ea520] else += 0.05, clamp [0.6, 1]
   -> 0x4f3ff0(camera, f): camera +0xd0 = f, +0xd4 = 1/f^2 (LOD distance scale)
0x4f2f30(world, ...)  -> 0x543b60(world light count +0xa0)          lights
0x4f4170(camera) -> 0x4f41b0 -> 0x4f4370   camera matrices (below)
0x4f6d40        horizon nodes (camera data +8 / +0xc: 0x4fc0b0, 0x4fc020)
0x526e10(camera data)   near/far, window rect, 0x521c60 projection, 0x521c10 fov
0x4f2f60(world)  -> 0x4fb310 per world light
0x4f3170(world)  -> 0x4fd680 per world sound
if [0x74a264]: 0x543720, 0x543760   (projected shadows: collect casters, their
                                     direction into camera space; section 5)
zones: [0x5ea1d0] zone mode, camera +0x1e0/+0x1e4 -> [0x74a328] (0x4eda10, 0x53cc40)
0x579af0  BeginScene (0x57a740)
0x4f6c50(ECX world, EDX camera node, camera data) = 0x4f6720 + 0x4f6c00   scene walk
0x5496c0  the puffers' particles (list [0x72be14]) as sprites, 0x5489a0 /
          0x567720 (gmod_puff, ported: puffers.md)
0x51ea00  pop
[0x72f3e4] 0x57e110  sorted transparent polygons
(EDX != 0: 0x5a67c0)
[0x72f3e4] again, [0x72f3e8] 0x57e4c0 overlay list (ZFUNC ALWAYS)
0x579b10  EndScene (0x57a7e0)
window +0x40 entries (stride 0x34, count +0xe0 & 0x7fffffff, bit 31 = on):
   bounding rect of each polygon -> 0x576ce0   [unverified: dirty rects]
```

### Camera matrices: 0x4f4370 (ECX camera node, EDX camera data, stack offset)

1. 0x51fde0 identity, `0x4ed280(camera, 1)` builds node-to-world for the
   camera (below). Camera node: data +0x10 bit 2 clear -> euler from
   rotation +0x20 and translation +0x14 (scale 1); bit 2 set -> its own 3x4 at
   +0x80 (0x4eca00 rebuilds +0x80 from +0x20/+0x14 when +0x10 bit 4 is set).
2. Optional offset (stack arg, used for virtual partitions): added to the
   translation; also kept at 0x73fbb0.
3. Camera data +0x2c = world position (row 3), +0x74 = **-row2** (view
   direction), +0x44..+0x73 = the world matrix, +0x38 = its euler (0x523bc0).
4. 0x520b00: view matrix into 0x73fbc0 (section 0).
5. Sound listener update when camera +0x10 bit 0 (0x571e50; velocity from
   +0x68..+0x70 over the frame time).

0x4f41b0 then: when +0xf8 (frustum dirty, set by the clip and fov setters):
far-plane corners in camera-local space at +0x108..+0x134:
`(t_h*far, -t_v*far, -far)`, `(-t_h*far, -t_v*far, -far)`,
`(t_h*far, t_v*far, -far)`, `(-t_h*far, t_v*far, -far)` with
`t = tan(fov_eff/2)` **[unverified: corner order]**; near point
`+0xb8 = pos + dir*near`, far point `+0xc4 = pos + dir*far`; 0x5271a0 /
0x5272b0 build the planes **[unverified: the plane maths; normals at
+0x188.. are inward, see 0x541760]**.

Camera data fields used here (0x1e8 bytes; c1 values in brackets):

| Off | Meaning |
|---|---|
| +0x00 / +0x04 / +0x08 / +0x0c | world node, window node, two horizon nodes (indices in the file) |
| +0x10 | flags: 1 sound listener, 2 matrix given (+0x80), 4 matrix dirty, 8 (passed to 0x4f2f30) |
| +0x14 / +0x20 | translation / euler rotation |
| +0x2c, +0x44..+0x73, +0x74 | world position, world matrix, view direction |
| +0xb0 / +0xb4 | near / far clip [0.1, 800] |
| +0xb8 / +0xc4 | near / far points on the view axis |
| +0xd0 / +0xd4 | LOD multiplier f / 1/f^2 [1, 1] |
| +0xd8 / +0xdc | zoom divisors given to CameraSetFOV 0x4f3b90 [1, 1] |
| +0xe0 / +0xe4 | base fov h / v, full angles in radians [1.39626, 1.0472] |
| +0xe8 / +0xec | effective fov = base / zoom, capped at 1.396 |
| +0xf0 / +0xf4 | half angles |
| +0xf8, +0x138, +0x184 | dirty flags |
| +0x108..+0x134 | far-plane corners (camera space) |
| +0x188..+0x1cc | plane normals: 4 sides (+0x188, +0x194, +0x1a0, +0x1ac), near +0x1b8, far +0x1c4 |
| +0x1d4 / +0x1d8 | cot(h/2), cot(v/2) [1.19175, 1.73205] |
| +0x1e0 / +0x1e4 | zone mode / zone list (count byte + ids; 0xff = all) |

Quirk: the file's 80 degree fov (1.39626) is above the setter's cap 1.396, so
any `CameraSetFOV` call lowers it to 79.98 degrees; the file value is used
as is until then.

### Building node-to-world matrices: 0x4ed280 (ECX node, EDX mode)

`gwNodeBuildNodeToAncestorMatrix`. Object3d with TREE_VALID (0x80000) and a
clean cache (data flag 0x20 clear): load its cached world matrix (data
+0x60) and stop. Otherwise collect the parents up the single-parent chain
(error "node has multiple parents" at a node with 2+), then apply from the
root down: camera (above), world/LoD: nothing, object3d: flag 8 (identity)
-> nothing, else multiply data +0x30 (and store the result in +0x60 and clear
0x20 when TREE_VALID), light 9: euler(data +0, data +0xc), sound 10:
translation +0x30 only, animate 8 (+4 bit 2): multiply data +8.

### The scene walk

**0x4f6720 (ECX world node, stack camera data)**: partitions, then
**0x4f6c00** (plane mask 0x3f, 0x5438d0 if lighting, then 0x4f5910 on every
direct child of the world: in c1 the `horizon` and nine top-level groups not
in any partition).

Partition selection (the "diamond tiler"): 0x4f5c80 when world +0x50 == 0,
**0x4f61b0** when it is 1 (all six shipped worlds have 1, "virtual
partitions"). Both:

1. Transform the camera position and the far-plane corners (3 points, or 5
   when the camera's pitch or roll exceeds 10 degrees) to world space and
   take their XZ bounding rectangle in partition cells (0x4f2060 /
   0x4f1fe0: world position -> cell).
2. Each cell in the rectangle whose visibility mask (+4) meets the camera
   cell's mask (always -1 in the shipped files) and passes 0x54ff40 (inside
   the projected view polygon, tolerance world +0x7c = -192) and the sphere
   cull 0x541760 (centre +0x28, radius = diagonal +0x34 since flags are
   always 0x100) goes into ring `|dz| + |dx|` from the camera's cell
   (0..49, at most 30 cells per ring; errors "Need more diamond tiler
   rings/cells").
3. 0x4f61b0 only: a cell outside the map grid wraps into the virtual block
   (0x4f1f10: world +0x54..+0x60) and is recorded with the offset
   `(dx * cell_w, dz * cell_h)` (world +0x64/+0x68), no sphere cull.

Drawing (0x4f6720) goes ring 0 to 49, nearest first. Per cell: a wrapped
cell moves the camera by the negated offset (0x4f41b0 with the offset,
section above) and draws only its nodes with node flag 0x8000 (in c1, 1025 of
the 1029 partition nodes have it: terrain-like); the next real cell restores
the camera (0x4f4170). So the ground outside the map is the virtual block
repeated, drawn by shifting the camera. World lights get flag 0x200 when the
cell is outside a point light's range (+0x60 position, +0xb8 range) to skip
them. The plane mask for the cell is set (0x3f), then 0x4f5910 on each
partition node (+0x3c, count i16 +0x3a), then the cell's fade entries (+0x40
count, +0x44 array of 0x44 bytes: node, 3x4 matrix copied into the node's
+0x30 and +0x60, sphere centre, near/far distance and fade factor: opacity
`(far - d) * k` pushed with 0x526a60 when below 1, zone byte; empty in the
files, filled at run time **[unverified: by the decal/feature client]**).
If fog is on (0x526b90), each cell switches fog on or off (0x526b80 sets
[0x74a22c]): on when `|cell centre - camera| + cell radius * k [0x5cdb70]
>= fog near` (0x4f6952..0x4f698f), so cells wholly nearer than the fog
start skip the fog work; fog is switched back on after the walk (0x4f6be8).

### Per node: 0x4f5910 (ECX node, EDX 2)

Zone test 0x53cdc0 (node byte +0x30 against [0x74a328]; 0xff passes), node
+0x2c bit 0x20/0x40 pre/post callbacks [0x5ea510]/[0x5ea514], then by class:
1 camera 0x4f4580, 5 object3d 0x4f4ab0, 6 LoD 0x4f5090, 7 sequence 0x4f56c0,
8 animate 0x4f4ee0, 9 light 0x4f4900, 10 sound 0x4f4730, 11 switch 0x4f57e0;
2 (world) and others: "Unrecognized node rendering type" on stderr.

**Object3d 0x4f4ab0** (others follow the same pattern):

1. Skip unless node flag 4 (ACTIVE). Clear 0x02000000.
2. Bounding sphere: when the node changed (+0x2c bit 0, or a parent's
   recompute flag [0x5ea530], or no TREE_VALID) and it has a box (flag
   0x100): 0x4ebdb0 transforms the 8 box corners to world space, 0x4f9c40
   takes their AABB: centre -> node +0x64, radius -> +0x70 with an
   **approximate square root** (`bits(x)>>1 + 0x1fc00000`, up to ~6% off).
3. Cull 0x541760 (ECX centre +0x64, EDX &mask, stack radius): for each plane
   bit still set in the parent's mask: `d = dot(centre - p, n)`; `d <= -r`:
   culled (returns the plane bit); `d < r`: keep the bit for the children.
   Bits: 1, 2, 4, 8 sides (through the camera position +0x2c), 0x10 near
   (point +0xb8), 0x20 far (point +0xc4). Node flag 0x80 ignores the far plane.
   Skipped when the mask is 0 (fully inside) or EDX < 2.
4. Matrix: data flag 8 clear: push data +0x60 as the new top (0x51e930
   copies the parent), multiply data +0x30 (0x51fee0 mode 3), clear 0x20; or,
   TREE_VALID and clean, push +0x60 as is. Flag 8: no push (identity).
5. Opacity (data flag 2, +4) pushed on 0x60bad0 / 0x526a60; colour (flag 4,
   +8..+0x14) on 0x60bc28 / 0x526a10; node flag 0x800000: 0x526a90 once per
   branch; node in the list at [0x73fc4c] (count 0x7402c4): 0x526a80/0x526a70.
   Correction (render map, section 5): [0x73fc4c] is the projected-shadow
   caster list and 0x526a70 writes [0x74a264] (shadows on), so this turns
   shadows off while a caster draws itself, not a lighting override
   **[unverified: read from 0x526a70's one store]**.
6. Hidden-surface test 0x53d750 (CameraSetObjectHSETest, [0x5ea518] = 1 by
   default) for ring >= 1 when the node has no model lights **[unverified:
   projects the sphere and tests a coverage buffer]**.
7. Visible: node flag 0x80000000; model (+0x3c) drawn with
   `[0x74a2e0](ECX node, EDX mask)` = **0x53f2e0** (Direct3D) / 0x53e050
   (software), with the LoD morph factor when inside a morphing LoD
   (model +8 |= 8, model +0x24 = [0x60bac8]); then children with the mask
   pushed on [0x74a324].
8. Pop everything.

**LoD 0x4f5090** (data 0x50; mech3ax `LodMwC`): +0 level (1 = measure,
0 = use the enclosing LoD's distance), +4 near^2, +8 far, +0xc far^2.
`d2 = |camera pos - node centre|^2 * camera +0xd4`; drawn when
`near^2 <= d2 < far^2`. Then (all zero in the files, so dead): grow-in
scaling by +0x1c..+0x30 (0x520330), fade-in/out opacity by +0x3c/+0x40
(pushed opacity), morph by +0x48/+0x4c (sets [0x60bc20]/[0x60bac8] for the
models below). Distances use the same approximate square root. Children
inherit the distance through a per-level stack (0x60bba0, level [0x5ea538]).

**Switch 0x4f57e0**: draws child i when bit i of `data[data[0] + 2]` is set.

## 2. Transform conventions: evidence

- Matrix multiply 0x51fee0 (`top = L * top`): asm 0x51ff3c.. reads L rows
  from ECX and the top from [[0x5eda90]]; result element (0,0) = L0*T0 +
  L1*T3 + L2*T6, translation = L9*T0 + L10*T3 + L11*T6 + T9. Mode 2
  (rotation only) never writes the translation of its stack result, so it
  copies stack garbage into the top's row 3 (0x51ff25..0x52003e then
  0x5201c3 copies all 12) **quirk; only used where the caller ignores it
  [unverified]**.
- Euler 0x521670: emulated on 200 random angle/scale sets, exact to 1e-4.
- Object3d update 0x4eca00 (frame update over list 7, 0x4f7db0): data flag 1
  (dirty) and not USE_MATRIX 0x10: `+0x30 = euler(rotate +0x18,
  translation +0x54, scale +0x24)` (0x4eca60..0x4eca95: push +0x30,
  identity, 0x521670(ECX +0x18, EDX copy of +0x54, scale +0x24)). USE_MATRIX
  keeps +0x30 as given. The setters (e.g. scale 0x4fbc10) clear USE_MATRIX,
  clear flag 8 unless the value is the identity, and set flag 1.
- View matrix 0x520b00: asm 0x520b1a..0x520c01 (negate rows at 0x73fc0c and
  0x73fc18, transpose by swaps, negate translation, multiply); emulated on
  100 random cameras: a point one unit along the camera's -Z lands at
  camera-space (0, 0, 1).
- Camera direction: 0x4f4370 stores `-row2` at camera +0x74..+0x7c.
- Projection 0x5234f0 / 0x523540 decompile cleanly; emulated (10, 5, 20) ->
  (480, 300, 0.05) with fx = fy = 320/240, centre (320, 240).

## 3. Polygons

**0x53f2e0** (`[0x74a2e0]`, Direct3D model draw; ECX node, EDX plane mask
from the cull) draws one model. Software has its own (0x53e050), not
mapped here. Sources: the pseudocode checked against the asm at every draw
call (0x54152a.., 0x541633.., 0x540ae9.., 0x540b16..); argument lists below
come from the asm, Ghidra drops the fastcall registers.

### Model level

1. Model type (model +0, mech3ax `ModelType`): 0 default, 1 facade
   (billboard; 4 modes at +4: 0x51ea90 cylindrical Y, 0x51efb0 spherical Y,
   0x51f390 cylindrical X, 0x51f750 cylindrical Z; model flag 0x40 rotates
   about the bounding-box centre +0x48 instead of the origin), 2 points
   (each vertex beyond the near plane and inside the screen rect is
   drawn as one point: `[0x72f3e0](0xff)` on Direct3D, 0x567420 on
   software). Type 2 returns right after.
2. All vertices are transformed once into camera space (`top-of-stack *
   v`, buffer `[0x74639c]`); a morphing model (flag 8 and morph factor +0x24
   and morph count +0x1c) blends first (0x5220e0, 0x5227a0).
3. 0x53daa0 decides what the model needs (its bounding-box centre +0x48,
   radius +0x54): bit 1 **fog** (fog on, model flag 2, model fog factor
   0x555640 > 1/255), bit 2 **lights** (light mask `[0x74a2cc] & 1`, any
   world light, model flag 1, 0x543d90 counts lights reaching it), bit 4
   **tint** (an object3d colour pushed, [0x74a270]). Fog then computes one
   factor per vertex (0x555a70 into 0x7463a4[]; none above 1/255 clears bit
   1). With SetVertexShading on ([0x749808], every shipped world turns it on
   for hardware in its `.gw` script) and normals in the model, the normals
   are rotated too (0x5226e0 into `[0x7463a0]`).
4. Model lights (+0x20 count, +0x40 array, 0x4c each) are animated here
   (0x541df0; type 0 blink, type 1 cycle) **[unverified: what they draw;
   all have a single vertex, likely lens flares/light points]**.
5. Shadow receivers ([0x74a264] on, model flag 1, opacity 1): `local_68`;
   spot/projected light textures (light flags 0x10 and 0x40, node
   `+0xbc < 2`): `local_98`. See section 5 and render-shadows.md.

### Per polygon (0x24 bytes, model +0x34, count +0x10)

1. **Vertex order is reversed**: the polygon's vertices are copied from the
   last index to the first into the work buffer (0x7476a4, camera space,
   12 bytes each), and so are its UVs and colours (0x53fbf6.. and 0x540d0b..).
   Everything below (D3D fan, strip) sees the reversed order.
2. **Backface test** (only when flag 0x400, mech3ax TRI_STRIP, is clear):
   `n = (b0 - b1) x (b2 - b1)` from the first three reversed vertices
   (= the right-hand normal of the file's last three vertices), `d = n . b0`
   (camera at the origin). `d < -0.005` ([0x5ef73c]): front, draw. Flag
   0x100 (two-sided, mech3ax SHOW_BACKFACE) and `d > 0.005`: draw with `n`
   negated (for lighting). Otherwise skipped; `|d| <= 0.005` is skipped even
   when two-sided, and since `n` is not normalised the threshold scales with
   the polygon's area and distance (a quirk: tiny or far, edge-on faces drop
   out). D3D culling is off (CULLMODE NONE, 0x57a900), so this test is the
   only culling.
3. **Primitive**: `[0x72f418] = 6 - (flag 0x400 != 0)`: D3DPT_TRIANGLEFAN (6)
   for normal polygons, D3DPT_TRIANGLESTRIP (5) for 0x400. One
   DrawPrimitive per polygon, `vertex count = flags & 0xff`. Strips are not
   culled as a whole; when the polygon is clipped, fogged/lit per vertex, not
   fully opaque or its texture has alpha, the strip is split into triangles
   (each one culled, alternating order, and drawn as a 3-vertex fan).
   **No shipped world polygon has 0x400** (c1: flags 0, 0x100, 0x200,
   0x300 only), so worlds are all fans.
4. **Normals** (flag 0x200, mech3ax NORMALS): per-vertex normal indices,
   used only with SetVertexShading on and model normals present; otherwise
   lighting uses the polygon normal `n` (flat).
5. **Texture**: material +1 bit 1 (the u16 at +0 tested with 0x100).
   Material flag 4 (cycled) advances the cycle first (0x5376c0). UVs (8
   bytes per vertex) go to the TL vertex `tu, tv` unchanged; the texture's
   own address mode (record +0x10/+0x14: 1 WRAP, 3 CLAMP) is set per draw.
   Scrolling (model flag 0x10, `Object3DSetScroll*`) is not the draw's:
   the UVs themselves are moved, by the scroll's step (Texture scroll,
   below).
6. **Clipping**, by the plane mask the cull left (EDX; `local_78`):
   bits 0x30 (near 0x10, far 0x20) clip in camera space before projection
   (untextured 0x5275e0, coloured 0x527a30, textured 0x527fd0, per-vertex
   colour 0x5286e0, shadowed 0x528bb0 / 0x529120). The far plane only
   rejects a polygon wholly beyond it, it does not cut one that straddles
   it (0x5275e0 lines 20-32); the near plane (camera +0xb0, [0x740304])
   really clips. Bits 0xf (the sides) clip in **screen space** after
   projection against the viewport rect ([0x7402f8] +4/+8 left/top,
   +0x1c/+0x20 right/bottom) (0x529ce0, 0x52aaf0, 0x52bd10, 0x52dc80,
   shadowed 0x52e920 / 0x52fab0). A fully clipped polygon is dropped.
   D3D may clip again (DrawPrimitive flags 0x18 = DONOTUPDATEEXTENTS |
   DONOTLIGHT, no DONOTCLIP).
7. **Projection** into the D3DTLVERTEX buffer at 0x72f41c (0x20 each:
   sx, sy, sz, rhw, diffuse, specular, tu, tv): `w = near / z` ([0x73fb80]),
   `sx = [0x73fc30] * x * w + [0x73fb88]`, `sy = [0x73fb98] * y * w +
   [0x73fb8c]`, `sz = w`; the draw functions copy sz into rhw (section 0).
8. **Priority** (polygon +4): `sz *= 1 + priority * [0x74a284]`; [0x74a284]
   is 0.02 or 0.01 (section 6). With reversed depth a larger sz is nearer,
   so priority is a **depth bias**, not a sort: +1-2% of 1/z per level. Shipped
   values 0-15 (c1: 24,253 at 0, 829 at 1, 109 at 4, ...), used by shadow
   decals (`shado`, `shadow`, `shade`), footprints (`print`, 4), car hoods.
   rhw gets the same factor, which leaves perspective-correct UVs unchanged.
9. Then one of the draw functions (section 4), chosen by textured / lit /
   per-vertex colour / shadows; argument lists:

| Pointer | Function | Used for | Args (ECX, EDX, stack...) |
|---|---|---|---|
| [0x72f3b0] | 0x57bf80 | untextured, no fog, no light | packed material colour (+2), alpha, count, priority, overlay |
| [0x72f3b8] | 0x57c700 | untextured, fogged or lit | packed colour, &material.r, flags (`local_c`), alpha, count, priority, overlay |
| [0x72f3c0] | 0x57ce20 | textured, plain | count, texture record, opacity, overlay |
| [0x72f3c4] | 0x57d160 | textured, lit/fogged, same for all vertices | flags, count, texture record, priority, opacity, overlay |
| [0x72f3c8] | 0x57d690 | textured, per-vertex light/fog | flags, count, texture record, opacity, overlay |
| [0x72f3d0] / [0x72f3d4] / [0x72f3d8] | 0x57eeb0 / 0x57f2a0 / 0x57f860 | textured with projected shadows (multi-pass) | count, texture / flags, count, texture |
| [0x72f3bc] | 0x57caa0 | textured, white vertices: puffers without colours (0x567720), weather (0x5a5a03); ported | count, texture record, priority (unused), opacity, overlay |
| [0x72f3cc] / [0x72f3dc] | 0x57ea20 / 0x57fe20 | one projected-light triangle from 0x547740 (correction: not puffers; render-shadows.md) | light, flags, count, texture, priority, opacity, overlay / light, flags, count, texture |

`alpha` = `ftol(material alpha * opacity [0x74a26c])`; `opacity` = the
pushed node opacity as a float; `overlay` = [0x74a268] (node flag
0x800000, mech3ax OVERRIDE: drawn last with no depth test, section 4).

### Colour

- **Untextured, unlit, unfogged** (0x57bf80): every vertex gets the
  **material colour** from the display-packed u16 at material +2 (so RGB565
  precision), alpha `material alpha * opacity`, **flat shading**
  (SHADEMODE 1). **Vertex colours are not used.**
- **Untextured, lit or fogged** (0x57c700): `colour = (1 - darkness) *
  material rgb` (floats +4..+0xc, 0..255), plus coloured point-light terms,
  Gouraud when they differ per vertex; fog in the specular alpha. Vertex
  colours are not used here either.
- **Textured** (0x57ce20, 0x57d160, 0x57d690): `diffuse = round(vertex
  colour * (1 - darkness))` per vertex, i.e. the polygon's vertex colours
  (0..255) are the base; alpha = opacity * 255 (255 when opaque); D3D
  stage 0 MODULATEs texture by diffuse. The **material colour and material
  alpha are ignored** for textured polygons (every textured polygon's
  material in the six worlds has alpha 255 anyway). Shipped vertex colours on
  textured polygons are 98.6% grey, mostly 255 (baked darkening).
- Unlit textured: darkness = 0, so `diffuse = vertex colour`.
- `round` is `ftol(x + 0.5)` (0x5ceb7c = -0.5 subtracted).

### Transparency

- Untextured: **material alpha** (byte +0) times the node opacity. Below
  255 the polygon goes to the transparent queue (0x57bf80: `alpha < 0xff`).
  Alpha 0 polygons are queued like any other and blend to nothing:
  **material alpha 0 = invisible** (see section 6 for what uses it).
- Textured: transparent when the texture record type +0xc is 4 (ARGB4444,
  made from a texture with a full alpha plane, texture flags 0x0b) or the
  node opacity is below 1. Type 5 (ARGB1555, alpha without a plane, flags
  0x03) and 1 (opaque) are drawn opaque; in the worlds every alpha texture
  is 0x0b (c1: 192 of 582, e.g. `drytree01`), 0x03 occurs only in UI
  images.
- Opaque draws: alpha blending off, ALPHAOP SELECTARG1 (texture alpha,
  unused); there is **no alpha test and no colour key anywhere** in the D3D
  code (no ALPHATESTENABLE / COLORKEYENABLE call), so 1555 textures drawn
  opaque would show their transparent texels.
- **Transparent queue** (max 6000 polygons, 24000 vertices; "Not enough
  MAX_TRANSPARENT_POLYS/VERTICES"): the TL vertices are copied out; at the
  end of the scene 0x57e110 sorts by the **first vertex's sz** (the float
  bits as int, ascending, so far to near; quicksort 0x57dec0 with an
  insertion-sort finish, not stable; the index list starts reversed) and
  draws each with ALPHABLENDENABLE on, SRCALPHA / INVSRCALPHA, **Z writes
  off** (Z test still on), ALPHAOP MODULATE, its own texture and address
  modes. The first vertex is the polygon's last file vertex (or the first
  after clipping), not a centroid.

## 4. Direct3D 6 interface points

All in `zvid_ddd3d.c` (0x57a740-0x580350) and `zvid_dd.c`. The draw
function table is filled by 0x579f50 (0x579fe9..0x57a165: [0x72f358]..
[0x72f3ec], [0x72f3f0]; the software renderer fills the same table with its
own). Device `[0x809370]`, its vtable `[0x809374]` (IDirect3DDevice3),
viewport `[0x80937c]` (IDirect3DViewport3), DirectDraw `[0x809358]`
(IDirectDraw4), primary `[0x80280c]`, back buffer `[0x8027c4]`, Z buffer
`[0x809378]`. The 38 table entries that were never Ghidra functions were
created and decompiled for this map (`tools/ghidra/CreateAndDecompile.java`;
now in `analysis-output/decomp/`). Vtable slots read as D3D6 / DirectX 6
SDK order **[unverified against the SDK headers, which are not in the
repo; the slot meanings fit every call's arguments]**.

### Calls

| Exe | D3D/DDraw call | Values | SDL3 GPU replacement |
|---|---|---|---|
| 0x57a900 (device init, once) | SetRenderState | CULLMODE(22)=NONE, ZENABLE(7)=1, ZFUNC(23)=GREATEREQUAL(7), SHADEMODE(9)=FLAT, TEXTUREPERSPECTIVE(4)=1, TEXTUREMAG/MIN(17/18)=LINEAR, SRCBLEND(19)=SRCALPHA, DESTBLEND(20)=INVSRCALPHA, SPECULARENABLE(29)=0 | pipeline state: cull none, depth test GREATER_OR_EQUAL, linear sampler, blend SRC_ALPHA / ONE_MINUS_SRC_ALPHA in the blended pipelines |
| 0x57a900 | SetTextureStageState stage 0 | COLOROP=MODULATE, COLORARG1=TEXTURE, COLORARG2=DIFFUSE, ALPHAARG1=TEXTURE, ALPHAARG2=DIFFUSE | fragment shader `tex * colour` |
| 0x57a900 | Viewport3 SetViewport2 (+0x44), SetBackground (+0x20), AddViewport/SetCurrentViewport (+0x14/+0x30) | | `SDL_SetGPUViewport` |
| 0x57a740 BeginScene (vt+0x24) | then FILLMODE(8) 3 solid or 2 wireframe when changed ([0x8026f8] bit 1), DITHERENABLE(26)=[0x802700] (`VideoSetDither`, on in common.gw) when changed | | begin the render pass; wireframe = a debug pipeline; dither: ignore |
| 0x57a7e0 EndScene (vt+0x28) | DDERR_SURFACELOST -> DirectDraw RestoreAllSurfaces (+0x64) | | end the render pass. FIX (A) |
| 0x5777f0 (frame clear, `[0x809344]` via 0x576cf0) | back buffer Blt COLORFILL\|WAIT (0x1000400) with [0x802794] when [0x8026f0]; Z buffer Blt DEPTHFILL (0x2000000) with **0** | | render pass clear colour (when on), **depth clear 0.0** (reversed depth) |
| every draw | SetTexture(0, handle) (vt+0x98), record +8 | | bind the texture + sampler |
| every textured draw | SetTextureStageState ADDRESSU/V (13/14) = record +0x10/+0x14 (1 WRAP or 3 CLAMP), ALPHAOP(4) = SELECTARG1(2) opaque or MODULATE(4) transparent; cached in [0x809394]/[0x809398]/[0x809390] | | sampler address mode per texture; the opaque pipeline ignores alpha |
| every draw | SHADEMODE 1 (flat: 0x57bf80, 0x57c2f0, 0x57bd50, 0x57dba0) or 2 (Gouraud: everything else); cached [0x80938c] | | flat: nothing to do, the flat callers write one colour to every vertex **[unverified which vertex D3D flat uses in a fan; it does not matter here]** |
| every draw | **DrawPrimitive** (vt+0x70) type [0x72f418] (6 fan / 5 strip / 1 point), FVF 0x1c4 (D3DTLVERTEX: XYZRHW, DIFFUSE, SPECULAR, TEX1), flags 0x18 | | append to a per-frame vertex buffer, fans turned into triangle lists (`0, i, i+1`); one draw per state change. Vertices are already in screen pixels: the vertex shader maps `sx, sy` to NDC (`x = sx / w * 2 - 1`, `y = 1 - sy / h * 2`), depth `sz`, `w = 1 / rhw` for perspective-correct UVs |
| 0x57e110 (transparent flush) | ALPHABLENDENABLE(27)=1, ZWRITEENABLE(14)=0, SHADEMODE 2, sorted DrawPrimitives, then back | | sort on the CPU as the original (section 3) and draw with a blended, no-depth-write pipeline |
| 0x57e4c0 (overlay list, [0x72f3e8], max 0x180 "ZVID_MAX_OVERWRITE_POLYS") | ZFUNC(23)=ALWAYS(8), blend on, Z writes off, per entry type 0 untextured flat / 1 untextured blended / 3 Gouraud / 4 textured; ZFUNC back to GREATEREQUAL | | a no-depth-test pipeline, drawn after the transparent queue |
| 0x57e380 ([0x72f3ec], one 4-vertex quad, 0x57dd80 sets rect and colour) | ZFUNC ALWAYS, blend on, no texture | | full-screen tint quad: set by 0x558540 (the fade state, 0x599b66, 0x5a53cf) and 0x55a653 / 0x55a7b3 (render-frame.md) |
| 0x57bcf0 ([0x72f3f0]) | SetRenderState FOGCOLOR(34) = fog rgb 0..255 ([0x72f3f4..]) | | a uniform for the fragment shader |
| 0x57bb70, 0x57bbc0, 0x57bc00, 0x57bc40 ([0x72f39c]..[0x72f3a8]) | FOGENABLE(28), SetLightState FOGMODE(4)=LINEAR(3), FOGSTART/FOGEND(5/6) | **never called** (no reference besides the table fill) | none |
| 0x57eeb0, 0x57f2a0 (shadow multipass) | per shadow pass: SRCBLEND ZERO(1), DESTBLEND SRCCOLOR(3) (multiply), FOGENABLE 0, TEXCOORDINDEX, ADDRESS CLAMP (3), MAG/MIN filter (16/17) LINEAR, COLOROP SELECTARG1; afterwards SRCALPHA/INVSRCALPHA and **FOGENABLE 1** | | a multiply-blend pipeline drawing the same triangles again with the shadow texture's UVs |
| 0x57bd50 ([0x72f3ac], 2D textured quad from 0x443123) | ALPHABLENDENABLE on around one DrawPrimitive, ADDRESS WRAP | | 2D/HUD path, not world |
| 0x57b130 (texture create) | DirectDraw CreateSurface (+0x18) system + video memory, QueryInterface IDirect3DTexture2, Load; pixel format 565 (no alpha) / 1555 (alpha, no plane) / 4444 (alpha plane); record `{sys, vid, d3dtex, type 1/5/4, addrU, addrV}` (0x18) | | `SDL_CreateGPUTexture` RGBA8 (convert on upload), keep the type for the transparent test |
| 0x57a850 (present, fullscreen [0x8026e0]==1) / 0x577130 (windowed) | primary Flip(NULL, WAIT) retrying on WASSTILLDRAWING / RestoreAllSurfaces on SURFACELOST; windowed: Blt back buffer -> primary (WAIT) | | `SDL_SubmitGPUCommandBuffer` + swapchain present with vsync (CLAUDE.md, heat.md) |

### What the GPU port must reproduce

- **Reversed depth**: `sz = near/z` with GREATEREQUAL and a 0 clear. A
  32-bit float depth target holds this well (better than the original's
  16-bit Z). Priority = the 2%-per-level scaling of sz (section 3); keep it
  multiplicative on sz (not a polygon offset).
- **Pre-transformed vertices**: the game does its own transform, cull,
  clip and projection (sections 0, 3). The simplest faithful port keeps
  that CPU path and feeds screen-space vertices; moving the transform to
  the GPU is a later, separate step (it would change clipping and the
  per-vertex lighting order).
- **Vertex fog** in the specular alpha: `final = mix(fog_colour, lit,
  spec.a / 255)` per pixel (D3D interpolates the fog factor per vertex),
  only while FOGENABLE is on (see section 6, fog quirk).
- Flat vs Gouraud: only matters where a polygon's vertices differ; the
  untextured flat path already writes one colour to every vertex.
- No alpha test, no colour key, linear filtering, perspective correction
  on. Dithering and the 16-bit back buffer are not reproduced (play-accurate,
  not bit-identical).

## 5. Lighting, fog, colour

### The per-vertex work record

0x53f2e0 and its helpers fill, per polygon vertex i (reversed order), a
0x20-byte record at `0x7481a8 + 0x20*i`: +0 **darkness** (1 - directed
light), +4 point-light intensity, +8 **fog factor** (also used for
saturated lights and the colour tint, below), +0xc..+0x14 the polygon's
vertex colour (0..255). The draw functions turn it into D3D colours:
`diffuse.rgb = base * (1 - darkness)` (+ coloured point light, see below),
`specular.a = round(255 - 255 * fog)`.

### Lights (0x545720, `gmod_light` region)

The active world lights are a list at 0x749824 (stride 0x14: +0 light
data, +0xc enabled), count [0x74981c], built per frame by 0x543b60 from the
world's light list (world +0xa0). **Each world file holds one light,
`sunlight`** (class 9; light_survey.py): directed (flags 0x38b, mech3ax
RECALC | DIRECTED_SOURCE | STATIC | COLOR | 0x2 | 0x200), colour 1,1,1,
per world `diffuse / ambient` 1.0/0.4 (c1), 1.0/0.3 (c2), 0.2/0.35 (c3),
1.0/0.4 (c4), 0.7/0.4 (c4b), 0.7/0.4 (t1). Other lights come from the
animation loader at mission load (anim.md).

Light direction (0x4fb310, per frame): the light's node-to-world matrix
(0x4fb0c0: points (0,0,0) and (0,0,-1)) rotated into camera space, then
negated into data +0x90: **+0x90 is the unit vector toward the light** in
camera space; in world space it is row 2 of `euler(orientation)` (section
0), e.g. c1's (-20, 45, 0) degrees gives `(0.66, 0.34, 0.66)`, a sun 20
degrees up.

Directed light (flag 8), per polygon or per vertex:
```
I = min(1, diffuse * max(0, N . L) + ambient)      N = unit normal (camera space)
I *= cloud(x, z)          only for models WITHOUT flag 0x20, when "fluctuate" is on
darkness += 1 - I
```
`N` is the polygon normal from the cull (normalised, 0x4020d0; flipped for
a two-sided back face), or, with SetVertexShading on and polygon flag
0x200, the vertex's own normal (`0x747ca8` buffer). So the sun is
Lambert plus ambient, clamped, **multiplying** the vertex colour
(textured) or the material colour (untextured). Nothing makes models
brighter than their base colour except saturated lights.

Point lights (flags without 8; none in the world files): distance `r`
with the approximate square root; `0` beyond far (+0xb8), `1` within near
(+0xb4), linear `(far - r) * +0xc4` between, times the directed term when
flag 4 is also set; added to +4 (coloured light, D3D adds `intensity *
light colour` [0x72f40c..] to the base before multiplying) or, with flag
0x20 (SATURATED), to +8, the fog channel, with the D3D fog colour set to
the light colour (0x546504): **saturated lights are drawn as "fog" toward
the light's colour**. **[unverified: point-light maths read from the
pseudocode only; no shipped world light exercises it]**

Cloud pattern (`fluctuate` in each mission's `world.zrd`: period, wavelength,
base, amplitude, heading; read by mission.cpp 0x492407.. into 0x543920..
0x5439f0, on/off [0x5ef860]): `cloud = base + amplitude * T(u) * T(v)`
with T a 128-entry table at 0x5ce260 (20.37 = 128/2pi: a sine), u and v the
vertex's world x and z scaled by 2pi/wavelength plus a moving offset
(0x5ef87c/0x5ef880) **[unverified: offset update and heading use]**. c1:
period 8, wavelength 600, base 0.7, amplitude 0.3, heading 30. Model flag
0x20 (mech3ax "CLOUDS") opts **out** (0x53ff02: flag 0x20 sets
bit 4, and 0x545720 skips the pattern when it is set).

Colour tint (object3d data flag 4, pushed by 0x526a10: rgb [0x74a274..],
weight [0x74a280]): also through the fog channel: +8 += weight and the fog
colour becomes the weighted mean of the fog, saturated-light and tint
colours (0x53daa0 tail, 0x5680c0) **[unverified]**.

Model lights (model +0x40, 0x4c each) are not lights in this sense; they
are drawn by 0x541df0 (section 3, model level).

### Texture scroll (0x526ae0, 0x541ad0; ported 2026-10-25)

0x526ae0 (gmod_init, ECX the model, EDX on, stack u, v) sets the model's
flag 0x10 (+8) from `on`'s bit 0 and its speeds, u at +0x28 and v at +0x2c,
in texture widths a second; no model reports and gives 1. The flag is
read only by the model copy's "may not share" test (0x53b3c0); the
scrolling is 0x541ad0 (gmod_const area), the step, which the callers run
once a frame: the interp's scroll actions (0x596380 on a node, 0x5963a0 on
the ScrollAlways holder; interp.md), the turrets' EFFECT (vehicles.md, the
turrets' frame step 5) and the vehicles' tracks (vehicles.md, the move 0x4a12c0: each tread's v at
(turn rate x 2.25 -+ speed) x 1.72, stepped at once). The step:

- nothing for no model (-1) or one already stepped this frame (+0x30 kept
  equal to the frame counter [0x72f338]); a speed of 0 (or NaN: the x87's
  equal test) leaves its axis;
- every textured polygon (its material's +1 bit 0) moves by dt [0x72c52c]
  times the speed less the whole part (_ftol, toward zero) of that plus the
  middle of the polygon's own range on the axis ((max + min) / 2), so a
  polygon's UVs stay within about one width of 0..1 however long it scrolls;
- three copies of the walk: u and v together (the maximum taken when above
  it) or one axis alone (taken when not above it, NaN included).

It is dt-scaled already, so no frame-rate fix. The ported draw reads the
moved UVs as any others. `zmod::model_set_scroll`, `zmod::model_scroll_tick`;
`tests/differential/test_model_fx.py`.

### The object3d colour blend (0x4fb8c0, 0x4fb840; ported 2026-10-25)

0x4fb8c0 (object3d.c, ECX the node, EDX an rgb or none, stack a weight)
writes the data's +0x14 weight and +8..+0x10 colour, each clamped to 0..1
(NaN to 0: the x87's compares); 0x4fb840 (EDX on) switches the data's
flag 4. Both check the class (5) and report as the other object3d setters
(lines 0x203.., 0x1db..). The draw reads them: the scene walk (camera.cpp,
0x526a10) pushes a flag-4 node's colour and weight for its subtree, and the
lighting folds them into the fog mix (render-lighting.md). Who sets it: the
TARGET view's tint of its target (vt+0x68: a turret's 0x4a75b0, a vehicle's
0x4ad390, red by damage; vehicles.md). `zcls::object3d_set_blend`,
`zcls::object3d_use_blend`. The mix's output stage (0x5680c0, 0x579950,
0x579980, 0x5798a0) is ported and wired (item 4b), and the models are lit
in the program since the GfxFlags are (item 4c): render-lighting.md.

### Fog

Settings: each world file has fog type 1 (linear) with **all ranges 0 and
colour black**; the real values come from the mission's `world.zrd`
`fog` block, pushed by mission.cpp 0x491de0 (0x492535..0x492592 into the
world setters, then the usual push 0x4f1df0): colour, altitude, range, and
on hardware **`hwrange` replaces `range`** (0x491f18). All six worlds use
`range (500, 750)`, `hwrange (32, 750)` (t1: 64, 750), `altitude (80,
100)` (t1: 70, 85), and a **fog colour equal to the sky band's material
colour** (c1 0.8157 = 208 grey; c2 118,127,118; c3 32,32,48; c4 187,143,99;
c4b 90,122,131; t1 181 grey). c4b's lava variant uses `horblack`, red fog
(0.9, 0.01, 0.01), altitude (-65, -3), hwrange (64, 350). The anim fog
events (c2: 3, c4b: 4, `render_fog_events.py`) change it during missions.
Defaults before any push (0x5265c8..): magenta (1, 0, 1), near 500, far 700,
altitude 200..300.

Per vertex (0x555a70; model-level test 0x555640 with the bounding radius):
```
d2 = x*x + z*z                    camera-space x and z (the camera's horizontal plane)
y  = world height of the vertex   (camera-to-world matrix at 0x73fc00)
if d2 <= near^2 or y >= alt_high: fog = 0
elif d2 < far^2: fog = (approx_sqrt(d2) - near) / (far - near)
                 if y > alt_low: fog *= (alt_high - y) / (alt_high - alt_low)
else:            fog = y > alt_low ? (alt_high - y) / (alt_high - alt_low) : 1
fog = clamp(fog, 0, 1)
```
So it is ground fog: full strength below `alt_low` (80), fading out by
`alt_high` (100), linear in horizontal distance from 32 (hardware) to 750,
and beyond the far range everything below `alt_low` is fully fogged.
Which of the pair is high is inferred from the sense of the formula
(**[unverified]**: the world setter order). The distance ignores the
camera-space y axis, so it is horizontal only for a level camera. Only
models with flag 2 (mech3ax FOG) are fogged; the sky models have flags
0x10 / 0, so **the sky is never fogged** and the fogged terrain fades into
a colour equal to the sky's band.

D3D: `specular.a = 255 * (1 - fog)` per vertex, colour via FOGCOLOR
(0x57bcf0, sent when it changes by any amount at this level; the 1%
threshold of discoveries.md is the software ramp's).

### Opacity and colour pushes

Object3d data flag 2: opacity (+4) pushed on 0x60bad0 and made current
([0x74a26c], 0x526a60); flag 4: colour (+8..+0x14) via 0x526a10 (tint
above). LoD fade-in/out also pushes opacity (all zero in the files). Node
flag 0x800000 (mech3ax OVERRIDE) sets [0x74a268] for the subtree: its
polygons go to the overlay list, drawn last with ZFUNC ALWAYS.

### Projected shadows

The mechs near the player (and the player's own) carry a caster record with
a small shadow image of themselves (0x4814e0, 0x542e70; list at 0x64fbe0).
Each frame 0x543720 / 0x543760 list the enabled ones and turn their light
slope into camera space, 0x543810 narrows the list per partition cell, and
the model draw tests each lit, opaque, textured polygon against them
(facing the light, not above the caster, its footprint slid down the light
overlapping the caster's ground box): up to 7 hits, each a UV set and the
caster's texture, drawn by the multipass functions (0x57eeb0..: the base
pass, then one multiply-blend pass per shadow). The caster's own node turns
shadows off while it is drawn (0x526a70 around 0x4f4ab0's model draw). At
a 24-bit FPU the images come out blank (white); in play (Windows 11 +
DDrawCompat) the shadows show, so the FPU is at 53 bits there (Owen,
2026-10-20). Details,
projected lights (flag 0x40) included: `render-shadows.md`. Separately,
many objects carry **fake shadow polygons**: untextured black with material
alpha 51..153 (`shdw`, `shad`, `dk_shad`, crate `shdw`), blended like any
transparent polygon, often with priority 1.

### The sky

- Every world has `world1/horizon/hormain/{horscroll, <sky band>,
  sunpoly|stars}`; c4b also `horizon/horblack`. `horscroll`: 72 textured
  polygons (one cloud texture, vertex colours 255), model flags 0x10
  (scroll; `Object3DSetScrollAlways on 0.015 0.0` in each world's `.gw`),
  an inward-facing dome 792..1753 units from its origin, y 100..423. The
  band (`g15513` in c1): 12 polygons in the fog colour plus 1 cap
  (c1: 165,183,208; the darker second colour in the other worlds), model
  flags 0 (unlit, unfogged), so drawn **flat in the material colour**.
- **It follows the camera**: the player view's constructor (0x40ad6a..
  0x40ad81) finds the node `horizon` and makes it the camera's horizon
  (camera data +8, 0x4f4090); every frame 0x4f6d40 sets the horizon node's
  **translation to the camera's world position** (all three axes,
  0x4fc0b0; `CameraSetHorizonXZ`, +0xc, would keep y; no script uses it).
  Rotation is not touched, so the sky turns against the view as a fixed
  dome. The world files' cameras have both fields -1; the game sets them.
- The mission picks the sky: of the horizon node's children, the one named
  in `world.zrd` `fog` `node` is made active and the others inactive
  (0x4925ba..0x4925db, `zcls_node_set_active`).
- The sky is farther than the far plane (800): `horizon` has node flag 0x80
  (mech3ax LANDMARK, the cull ignores the far plane) **[unverified that the
  far bit stays cleared for its children, so no polygon far test]**, and
  its depth `near/z` is tiny, so with GREATEREQUAL any terrain drawn in front
  of it wins. It is drawn as a direct child of the world after the
  partition cells (section 1).

## 6. Quirks and answers

### (a) The `inout` areas are deleted at mission load

The flat red / yellow / blue untextured models under `inout` (`inoutm1`..,
`inoutia`, `inoutmp`, t1 `inoutt`; all ACTIVE, node flags 0x0308051c /
0x0308031c, material alpha 255) are **not hidden by any render flag or
colour test, and no script touches them**. Mission load ("Loading InOut
data", 0x490ab0..0x490bd4) calls **0x4c3930** (`mech3\zainetmechinout.cpp`):

1. `inout` is found by name (0x4f7fe0, 0x4c3952).
2. The variant's name is built by 0x4c36c0 as `"inout" + suffix`: game
   mode 1 `m` + mission number (`inoutm1`..), mode 2 `ia`, mode 4 `t`
   (`%s%s`, 0x5e8acc / 0x5e8ac8 / 0x5e8ac4); the children are matched by
   0x4c3850 (`_strnicmp` plus `strtol` for the number). Multiplayer
   (`inoutmp`) has no branch, so it is never read. The matching child's
   polygons become the AI's in/out-of-bounds data (0x4c3b50 -> 0x4c3e50;
   "skipping inout polygon with morph_flag set", "limiting inout polygon
   vertex count", 0x53cb60).
3. Whatever matched, `inout` is unlinked from every parent (0x4c3a6d..
   0x4c3a9f: 0x4ebb40 on parent 0 while any remain) and **deleted with its
   whole subtree** (0x4f9050, the `DeleteTree` command's worker; it refuses
   nodes that still have parents, hence the unlinking).

So none of it is ever on screen. (Only when [0x60861c] is set is the data
parsed; the deletion happens whenever `inout` exists.) The viewer's
`under_marker` skip is right; its comment can say why.

### (b) The odd untextured colours are invisible: material alpha 0

Untextured polygons use the **material alpha** (section 3), and these
materials have alpha **0** (`render_poly_survey.py`, `alpha_survey`):

| Colour | Alpha | Where (c1 unless noted) |
|---|---|---|
| 217,69,255 | 0 | trees (`drytree06/healthy/l3/g3`, `drytree07/.../l1/g4`, c2/c3 `tree90..92/trunk`, c4 `tree80/81`): two crossed, two-sided (flag 0x104) vertical quads through the trunk, y 0..1.46, about 5.4 units wide |
| 254,15,159 | 0 | `drytree07`, `drytree03`, `drytree26` `.../l1/g4`: a horizontal diamond at y = 5, radius 3.39 (canopy height) |
| 254,254,254 | 0 | docks, piers, bridges, ramps (`fish_dock01/destroyed/dpier01/.../g5`, `lakbr/dbase`, `bramp/dbase`, `ctydock/.../flat1`, c2 `aptpt1/apt05/target`, `canbrg`, c3 `m2brg02/dbase`, c4 `brglake`, `track2`) |
| 255,255,255 | 0 | fishing boats, people's `torso` (`the_dude*/mover/l1/g29/torso`), c2 `sramp2` |
| 0,0,0 / others | 0 | `bvol1/2` (`span01`), `iagate`, `ter`, `trbbox`, `brgclan`, `bmtruck` |

They are queued as transparent and blend to nothing (0x57bf80: alpha <
255 -> queue; 0x57e110 draws with SRCALPHA/INVSRCALPHA and Z writes off),
so they cost a little and show nothing. They are not shadows and not
colour keys. The names (`target`, `centroid`, `bvol`, `dbase`) and the
placement (trunk, canopy, deck) suggest **helper geometry for collision,
targeting or the destroyed state** that the artists coloured loudly in the
editor; what reads them is not mapped **[unverified]**. The real fake
shadows are the untextured **black** polygons with alpha 51..153 (`shdw`,
`shad`, `dk_shad`, crate `shdw`, `whsa01/.../g38` alpha 89), drawn as
translucent black.

The viewer should treat untextured material alpha as the polygon's alpha:
skip 0, blend the rest.

### Other quirks

- **Vertex order reversed** for everything after the copy (section 3);
  only matters for a strip, for the transparent sort key (first vertex =
  the file's last), and for D3D's flat-shading vertex.
- **Backface threshold is unnormalised** (`|n . v| <= 0.005` with `n` the raw
  cross product): small or distant edge-on polygons vanish even when
  two-sided.
- **The far plane rejects, it does not cut**: a polygon straddling far
  (800) is drawn whole; only polygons wholly beyond it are dropped.
- **Untextured polygons ignore their vertex colours**, textured ones
  ignore the material colour and alpha. Flat untextured colour goes through
  the display-packed u16 (RGB565 on a 16-bit display), so it is quantised
  to 5-6-5 bits; lit or fogged untextured polygons use the float rgb instead
  **[unverified: the exact unpacked values]**.
- **Priority is a small depth bias, not a sort.** The model renderer's init
  (0x526510, run from 0x58fad3 and 0x48060e) sets the factor to 0.02 with a
  Direct3D device ([0x72f348]) and 0.01 without; `SetInverseZTolerance .01`
  in common.gw (0x526df0) sets 0.01 again (and [0x72f350] on hardware).
  Which runs last decides 1% or 2% per level **[unverified: the order]**.
- **Directed light only darkens**: `I <= 1` multiplies the base colour;
  brightening exists only through saturated lights (as fog toward the
  light colour).
- **Hardware fog is switched on by the shadow code**: no function sets
  D3D FOGENABLE except the shadow/multipass draws (0x57eeb0, 0x57f2a0,
  0x57f860 for shadows, 0x57fe20 for shadowed projected-light triangles),
  which turn it off for their passes and **on** afterwards (ported as is,
  render-shadows.md). Until the first such draw the device default (off, in D3D6)
  applies. The table's own fog setters (0x57bb70.., [0x72f39c]..) are
  never called. **[unverified: on the real device; a DDrawCompat run with
  no shadows on screen would show it]**.
- **World-file fog is a dummy**: linear, black, all ranges 0. Had a
  mission no `world.zrd` fog block, everything fogged below altitude 0
  beyond its own radius would be fully black (the formula's `d >= far`
  branch). All shipped missions have the block.
- **Fog uses camera-space x and z**: tilting the view changes which
  distance counts.
- **Software and hardware fog differ**: `range` 500..750 in software,
  `hwrange` 32..750 (t1 64..750) in hardware: hardware fog starts almost at
  the cockpit.
- **mech3ax's CLOUDS model flag (0x20) is an opt-out** of the moving cloud
  pattern in the lighting code.
- **The sky dome is beyond the far plane** (792..1775 units vs 800) and is
  kept by the LANDMARK flag; it follows the camera's position, not its
  rotation, and is never lit or fogged.
- 0x57e110 caches the texture record's type (1/4/5) as if it were the
  ALPHAOP value, so the transparent queue re-sends ALPHAOP MODULATE more
  often than needed. Harmless.
- Transparent polygons are sorted by one vertex, not a centroid; large
  translucent polygons can sort wrongly against small ones (as the original).

Candidates for `docs/discoveries.md` (not added here): the inout areas
(mission boundaries the AI reads, deleted before the first frame); the
invisible magenta tree and white dock helpers; hardware fog starting at
32 m versus 500 in software; fog colour = sky band colour per world; the
sun only darkens; fog relying on the shadow code to be switched on.

## 7. Porting order

Shaped by the code above; each step is checkable on its own.

1. **CPU vertex path of 0x53f2e0, default models only**: camera-space
   transform, reversed copy, backface test, near clip and screen clip
   (0x5275e0, 0x529ce0 first; the textured and coloured variants share the
   pattern), projection with priority bias. These are self-contained:
   differential tests in Unicorn on random polygons (inputs: the 0x7476a4
   buffer and the 0x7402f8 clip struct; outputs: the TL buffer at 0x72f41c
   and the count). Port into `src/gamez/zmodel/` (file name unknown,
   between `gmod_const.c` and `gmod_cookie.cpp`).
2. **The draw table as an interface** (`[0x72f3b0]`..`[0x72f3ec]`): a C++
   class the CPU path calls with the same arguments, a recording backend for
   tests, then the SDL3 GPU backend (section 4): per-frame vertex buffer,
   fans to triangle lists, pipelines {opaque textured, opaque untextured,
   transparent (blend, no Z write), overlay (no depth test), shadow
   multiply}, reversed float depth cleared to 0, screen-space vertex shader,
   `tex * diffuse` and vertex fog in the fragment shader. FIX (A) for
   Direct3D 6, marked at each draw function.
3. **Transparent queue and its sort** (0x57bf80/0x57ce20 queueing, 0x57dec0
   sort, 0x57e110 flush): port the sort exactly (order changes blending);
   differential test of 0x57dec0 on random keys.
4. **Textures**: the 0x57b130 record (type 1/4/5, address modes) on top of
   the ported `zimg` library; RGBA8 upload (FIX (A)).
5. **Lighting**: light list 0x543b60, light direction 0x4fb310/0x4fb0c0,
   0x53daa0, then 0x545720's directed branch (the only one the worlds use
   at load), then point/saturated lights and the tint. Differential tests
   on the work record (0x7481a8..).
6. **Fog**: 0x555640 / 0x555a70 (self-contained, differential), the world
   push already ported, and mission.cpp 0x491de0's world.zrd `fog` block
   (with `hwrange`) and sky choice.
7. **Horizon follow** 0x4f6d40 / 0x4fc0b0 and the player view's horizon
   hookup (0x40ad6a..), **inout removal** 0x4c3930 (with the AI boundary
   reader 0x4c3b50 later, when the AI comes).
8. Then: projected shadows (0x543720/0x543760 + multipass; done but the
   caster images, render-shadows.md), the cloud
   pattern (`fluctuate`), facades and points, morphs, model lights
   (0x541df0), the overlay list and the full-screen quad. Puffers (0x5496c0) are ported
   (puffers.md).
9. The software renderer (0x481290 strategy, 0x53e050, span code) is not
   ported (Owen, 2026-10-25): hardware textures only; a software look, if
   ever wanted, is a shader pass on the GPU path (CLAUDE.md, Rendering).

For the viewer now (`tools/viewer`), in order of visible effect: material
alpha for untextured polygons (0 = skip, else blend), texture alpha
(blend + sort, no Z write, instead of opaque with a colour-key guess), the
sun (Lambert + ambient, multiplying), backface culling (CCW front, 0x100
two-sided), priority bias (decals and shadows z-fight without it), sky at
the camera's position and drawn past the far plane, fog from world.zrd
(`hwrange`, altitude band, sky colour), 80 x 60 degree fov.
