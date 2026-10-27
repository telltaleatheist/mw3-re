# Projected shadows and projected lights (Direct3D)

Mech3.exe 1.2, 2026-10-20. The two multi-pass effects of the Direct3D model
draw: mechs casting a blurred shadow texture onto the ground around them, and
lights with flag 0x40 (the light-state event's SUBDIVIDE) cutting the
polygons near them into small lit triangles. Complements `render.md`
sections 3 to 5 (corrections at the end). Check:
`tests/differential/test_shadows.py` (probe `tests/probe_shadows.cpp`) and
the caster lists inside `test_scenewalk.py`.

## What is ported, and where

| Exe | Port | File (func2file) |
|---|---|---|
| 0x543720 collect, 0x543760 slopes, 0x543810 per cell | `zmod::collect_casters`, `caster_directions`, `cell_casters` (the `CullHooks` defaults) | between gmod_cookie.cpp and gmod_light.c: `zmodel/gmod_cull` (beside 0x5438d0) |
| 0x540552..0x5408d8 and its copy 0x540ef6..0x541260 (inside 0x53f2e0) | `zmod::collect_shadows` (the `DrawHooks` default) | `zmodel/gmod_draw` |
| 0x547540, 0x547740, 0x547060, 0x547160 | `zmod::projected_reach`, `draw_projected`, `projected_light_vertex`, `projected_split` (the `DrawHooks` defaults) | between gmod_light.c and gmod_puff.cpp: `zmodel/gmod_light` |
| 0x57eeb0, 0x57f2a0, 0x57f860 ([0x72f3d0] / [0x72f3d4] / [0x72f3d8]) | `zvid::draw_shadowed`, `draw_shadowed_lit`, `draw_shadowed_vertex_lit` | `zvideo/zvid_ddd3d` |
| 0x57ea20, 0x57fe20 ([0x72f3cc] / [0x72f3dc]) | `zvid::draw_projected`, `draw_projected_shadowed` | `zvideo/zvid_ddd3d` |
| 0x542bb0 init, 0x542c20 new, 0x542ca0 delete, 0x542d30 size, 0x542dd0 / 0x542de0 / 0x542e00 / 0x542e60 setters, 0x542e10 upload, 0x542e40 release, 0x542e70 draw, 0x5432d0 by class, 0x543330 a model | `zmod::shadows_init`, `shadow_new` .. `shadow_draw`, `shadow_node`, `shadow_model` | `zmodel/gmod_cull` (the same region as the lists) |
| 0x4f7670 object3d, 0x4f7750 LoD (the image's node walk) | `zcls::shadow_object3d`, `shadow_lod` | `zclass/camera` (camera.c .. list.c) |
| 0x55d010 the flat polygon filler, 0x557f90 its target | `zrndr::fill_polygon`, `set_target`, `soft_target` | `zrender/zrndr_span` (zreader.cpp .. zrndr_draw.c) |
| 0x522ab0 box corners | `zmth::box_corners` (was inside class.cpp) | `zmath/zmth_matrix` |
| 0x57baa0 reload ([0x72f374]), 0x57bb00 release ([0x72f378]), 0x579b70 caps | `zvid::reload_texture`, `release_texture`, `has_caps` | `zvideo/zvid_ddd3d` |
| 0x4814e0 the per-entity upkeep | `mech3::update_shadows` (`Engine::update_shadows`, was the hook `hw_frame_effects`); on the program's entities `mech3::update_mech_shadows` (`GameEngine::update_shadows`: mechs, vehicles and field bases, turrets, the pocket marker; mission-plan item 4), the two sums shared (`app_shadows.h`) | `mech3/app_states`, `mech3/app_mech` |
| 0x4d58fc / 0x4d5907 in zEntity3d's base end 0x4d58a0: the caster freed | `AiTarget::release_shadow`, from `Mech::release` and `~CZAiTarget` | `mech3/zai_target` |

`zvid::install_model_draw_table` now fills the five entries too
(`zmod::DrawTable::shadowed*`, `projected`, `projected_shadowed`). The
shadowed clippers (0x528bb0, 0x529120, 0x52e920, 0x52fab0) were already
ported and now get real UV sets.

Hooks connected: `Engine::light_init` (0x542bb0, app start) is now the port,
`Engine::update_shadows` (0x4814e0, the Direct3D frame) too. 0x4814e0 reaches
the casters through `Engine::shadow_*` (handles the defaults map to the port's
records), the entity positions through `entity_position` (0x5a85e0, not
ported) and the mech's node through `node_of` (none until the entities are
ported). The textures go through zvid to `platform::Render3D`, which gains
`update_texture` (FIX (A): the reload's surface copy and Load). Not ported:
the mech destructor's release (0x4d58fc / 0x4d5907, entity code).

## A frame

1. **Casters** (0x4814e0, below, once per frame from the Direct3D strategy
   0x481790, before the scene). For each object in the list at 0x73655c a
   mech (its vt+0x2c): fade 1 for the player's own mech, else by its
   squared distance to the player: 1 up to 2,500 (50 m), 0 from 10,000 (100
   m), linear between. Fade 0 releases the mech's caster (mech +0xcc).
   Otherwise it makes one if it has none (0x542c20, 0x8c bytes, linked in
   *after* the list's head at 0x64fbe0) with a 64x64 image for the player's
   mech and 32x32 for others (0x542d30), the mech's node (0x542dd0), enabled
   (0x542e60); sets its shade (0x542e00), its position (0x542de0), then
   draws the image (0x542e70, below) along the sun's world direction (light
   +0x84) onto the mech's ground height, and uploads it as the texture
   `gModShadow` (0x542e10: 0x57b130 with clamped U and V, no alpha; later
   frames reload it through 0x57baa0).
2. **The list for the frame** (0x4f7030, when shadows are on [0x74a264]):
   0x543720 takes the enabled casters from the list (the first one made,
   then the others newest first), into
   0x73fc60 (200 at most, [0x7402c0]); 0x543760 turns each one's slope
   (x, 1, z) into camera space (+0x4c, the view's 3x3).
3. **Per cell** (0x4f6720): 0x543810 keeps those whose ground box overlaps the
   partition's box (min x, z; max x, z, shifted back by a wrapped cell's
   camera offset 0x73fbb0) into 0x73ffa0 (200 at most), and 0x5438f0 makes
   that the list the walk and the model draw read ([0x73fc4c], [0x7402c4]);
   the world's direct children get the whole list back (0x5438d0).
4. **Walk**: a node in the list draws with shadows off (0x526a70), so a mech
   never shadows itself.
5. **Model draw** (0x53f2e0): a model receives when shadows are on, model
   flag 1 (lit) is set and the pushed opacity is 1. Per textured polygon whose
   texture has no alpha, the receiver test (below) fills the shadow sets
   (0x7489a8: count, 0x7489ac: 7 textures, 0x7489c8: 7 x 64 UV pairs); a
   polygon with at least one is clipped by the shadowed clippers and drawn
   by the shadowed entries instead of 0x57ce20 / 0x57d160 / 0x57d690.

### The caster record (0x8c bytes)

| Off | Meaning |
|---|---|
| +0x00 | node |
| +0x04 | world position (0x542de0) |
| +0x10 | x, z where the light through the position meets the ground |
| +0x18 | display-packed shade (0x542e00); the image's darkness step is `(-1 - colour) & 0x1f` |
| +0x1c | ground height |
| +0x20 / +0x24 | slope: light x / light y, light z / light y (the ground shift per unit of height) |
| +0x28 | sqrt(slope x^2 + slope z^2) |
| +0x2c..+0x34 / +0x38..+0x40 | the shadow's box on the ground: the node's box corners slid down the light, x and z min and max; +0x3c the corners' top + 2; +0x30 never set |
| +0x44 / +0x48 | 1 / box width in x, z: ground position to texture UV |
| +0x4c | (slope x, 1, slope z) in camera space (0x543760) |
| +0x58 | enabled |
| +0x5c / +0x60 | the image (zimg, 16-bit, flags 1, no alpha) and its texture record |
| +0x64..+0x80 | never written (malloc's) |
| +0x84 / +0x88 | previous / next in the list |

0x542c20 sets only the node (0), the colour (the black 0x542bb0 packed,
[0x64fbd8]), image, texture, enabled and the links; the rest is malloc's
until the first draw. The new record goes after the head: `prev` = head,
`next` = head's next; it becomes the head only in an empty list.

### The receiver test (0x540552.., the same code twice)

`n` is the polygon's normal from the backface test (negated for a two-sided
back face), the vertices are the reversed camera-space ones (0x7476a4), and
casters are walked from the last to the first:

1. Each vertex to world space (the camera-to-world matrix 0x73fc00, the
   model draw's own transform order); `lowest` = the least world y.
2. Skip a caster when `lowest > top` (+0x3c): the polygon lies wholly above
   the caster.
3. Skip unless `n . slope_cam > 0`: the polygon faces the light.
4. A line through the caster: `limit = (x + off x) * slope x + (z + off z) *
   slope z + 2 * |slope|`. If any vertex has `x * slope x + z * slope z <=
   limit` (on the light's side of the line), every vertex slides down the
   light onto the ground height (`x += (ground - y) * slope x`, z alike);
   otherwise only vertices below the ground height do.
5. The slid footprint's x/z bounds must overlap the caster's box (shifted by
   the camera offset), strictly.
6. UVs `((x - box min x) * scale x, (z - box min z) * scale z)` per vertex,
   the caster's texture, and the next set. Seven sets at most.

The plain path's copy reads the texture's image without the null check the
lit path has (a textured material always has one).

### The shadowed draws

| Exe | Args (ECX, EDX, stack) | Colour | Specular |
|---|---|---|---|
| 0x57eeb0 | count, texture | the vertex colour, rounded | 0xff000000 |
| 0x57f2a0 | flags, count; texture | as 0x57d160: the first vertex's darkness, point light (flag 4) | fog (flag 1) |
| 0x57f860 | flags, count; texture | as 0x57d690: each vertex's darkness | fog (flag 1) |
| 0x57fe20 | light (3 floats a vertex, or null), flags; count, texture | `clamp(magic(colour * light + 0.5))` | fog (flag 1) |

Alpha is always 0xff: these draws never queue a transparent polygon and
never go to the overlay list (opacity and the overlay flag are not passed).
Each builds one interleaved buffer (0x8008e0: x, y, z, rhw = z, diffuse,
specular, the texture's UVs, then one pair per shadow), FVF
`0xc4 | (shadows + 1) << 8`, and:

1. SHADEMODE Gouraud, SetTexture, address modes (cached), ALPHAOP
   SELECTARG1 (cached), MAG/MINFILTER LINEAR; DrawPrimitive: the polygon as
   usual.
2. SRCBLEND ZERO, DESTBLEND SRCCOLOR, ALPHABLENDENABLE on (around the state
   cache), address modes CLAMP (cached), COLOROP SELECTARG1 (the texture's
   colour alone), FOGENABLE off.
3. Per shadow k: SetTexture(its texture), TEXCOORDINDEX k, DrawPrimitive:
   the frame multiplied by the shadow texture, same depth (GREATEREQUAL
   passes), depth written again.
4. SRCBLEND SRCALPHA, DESTBLEND INVSRCALPHA, blending off, TEXCOORDINDEX 0,
   COLOROP MODULATE, **FOGENABLE on**.

No other code in the game sends FOGENABLE (render.md section 6): hardware
fog is off until the first shadowed polygon has been drawn, and stays on
after.

FIX (A), in the port: the multi-texture DrawPrimitive becomes
`Render3D::draw_multi(primitive, vertices, uv sets, count, state)`;
`DrawState` gains `color_select` (COLOROP SELECTARG1) and `texcoord`
(TEXCOORDINDEX), and `multiply` now follows SRCBLEND / DESTBLEND (ZERO /
SRCCOLOR). `draw_multi`'s default draws the selected UV set as a TLVertex
with white in place of the diffuse when `color_select` is on, so backends
need nothing new (the SDL3 one already has the multiply pipeline).

## The caster's image (0x542e70)

ECX caster, EDX the light's world direction, stack the ground height.
Returns 1 without drawing when the caster is disabled, has no image or the
light does not point down (y >= 0; a NaN y goes on), 1 when the node has no
matrix (0x4ed280 fails), else 0.

1. Slopes `light x / y`, `light z / y` (+0x20, +0x24), their length (+0x28),
   the point under the position (+0x10).
2. The box: the node's own box (+0x74, 0x4ebd50) through its world matrix
   (a pushed stack slot, identity, 0x4ed280 mode 3; corners by 0x522ab0),
   each corner slid down the light onto the ground (`x += (ground - y) *
   slope x`, z alike). x and z min/max as the record shows; the top is the
   highest corner before the slide, + 2. +0x44 / +0x48 = 1 / width.
3. The image: the software target set on its pixels (0x557f90: rect 0, 0,
   w - 1, h - 1, 16 bits, pitch 2w), cleared to 0. The parent's world matrix
   (0x4ed280 on node +0x58[0], else identity) on a pushed slot; [0x73ff84] =
   the caster, [0x74a3c8] = 1 (the filler's winding, left set afterwards).
   The node is walked by class (the switch 0x5432d0 also has): object3d
   0x4f7670, LoD 0x4f7750; classes 1 and 7..11 cast nothing; others print
   "Unrecognized node rendering type: %s" with the node (its name).
4. **0x4f7670**: an active (flag 4) object3d clears flag 0x2000000, pushes
   its matrix as the walk's renderer does (a clean cache pushed as is, a
   dirty one under a tree-valid node multiplied in and cleaned, flag 8 none),
   sets flag 0x80000000, gives its model the walk's LoD morph ([0x60bc20]:
   model flag 8 and the factor [0x60bac8]), casts it (0x543330), then its
   children (0x5432d0 each). **0x4f7750**: an active LoD whose data +4 is not
   above 0 does the same without a matrix.
5. **0x543330**: the model's vertices into [0x74639c] by the stack top (or
   the LoD morph 0x5220e0 + 0x5227a0), then each slid onto the ground and
   mapped into the image: `x' = (slope x * (ground - y) + x) * sx + ox`,
   `sx = (w - 1.2) * scale x`, `ox = 0.6 - box min x * sx` (z alike into
   y'), so the box spans 0.6 .. size - 0.6. A vertex outside 0 .. size - 0.5
   (or NaN) drops the whole model. Then each polygon (a strip as triangles
   `(i, (i + 1) | 1, (i + 2) & ~1)`, last first) whose first three vertices
   turn one way, `(y1 - y0)(x2 - x1) > (x1 - x0)(y2 - y1)`, is filled with
   1 by 0x55d010, through [0x7476a4] (64 slots, unchecked).
6. **0x55d010** (ECX vertices every 12 bytes, EDX count, stack colour): the
   top and bottom vertex by their y bits as integers; two edge lists, up and
   down the vertex list (which is left is [0x74a3c8]); each edge starts at
   its first row centre `((y in 16.16 + 0x7fff) >> 16) + 0.5` and steps x by
   dx/dy, all in 16.16 by the 1.5 x 2^52 trick; rows from the top's row to
   `(bottom in 16.16 - 0x8041) >> 16`, spans `(xl + 0x7fff) >> 16 ..
   (xr - 0x8001) >> 16` through the solid span routine [0x800828]. No
   clipping. A side changes edge at most once a row.
7. A 3x3 count in place over the interior: each pixel becomes `p + 2 * n`, n
   the 1s among its 3x3 (the neighbours already done count by their low
   bit). Then every pixel takes ramp[p >> 1] (0..9): 10 greys from white,
   `255 - 8 * (i * step / 9)` with step = `~colour & 0x1f` (the shade's blue
   bits inverted), so 9 neighbours reach the shade.

### Precision

With the x87 at 24 bits every 16.16 value in step 6 is 0 (only bits 29..31
of the trick's low dword survive, all clear under 2^13): no row, nothing
filled, the image all white. **In play the shadows are visible** (Owen,
2026-10-20, Windows 11 + DDrawCompat, hardware mode): the reference runtime
leaves the FPU at 53 bits at least where the trick runs (the CRT's
`_PC_53`). Whether DirectX 6 on Windows 98 in 1999 truncated it and hid them
stays unconfirmed. So the port's default is `zmod::shadow_state().precision
= zrndr::X87::k53` (the filler in double, as the trick needs: the shadows as
seen in play); `k24` is kept as the switch for the truncated FPU. Only
0x55d010 changes with it; the rest of the port stays float (24-bit
arithmetic, a last-bit approximation of a 53-bit FPU: test_shadow_image.py's
`x53` mode counts the differences).

With k53, the 57 mech trees of mechlib.zbd in their rest pose under the six
worlds' suns (`explore/shadow_mechs.py`, the original's code): every model
cast, none dropped by the bounds test; 619 to 1,968 of the 64x64 pixels
shaded for the mechs and their `_1` (low detail) trees, 1,000 to 2,604 for
the `_hud` ones (c2 and t1, the lowest suns, the longest). At 24 bits: 0.

## The upkeep (0x4814e0)

Nothing without shadows (0x526a80), a sun object ([0x6098d4]) or its node
(+4); the light is the node's data (+0x38). Per object of the list
0x73655c..0x736560, its mech = vt+0x2c(), its position = 0x5a85e0 (the node
data's world translation, refreshed when dirty):

- player = [0x60861c] +0x14 (the pilot's entity); the player's mech is the
  one whose vt+0x50() is it. Fade as in step 1 above, the distance from the
  player's position (0x51dfa0, summed (x + y) + z).
- fade <= 0 (or NaN): release the texture and the caster (0x542e40,
  0x542ca0), +0xcc = 0. Otherwise a new caster if none (size by the same
  vt+0x50 test).
- shade: `c` = the cloud pattern at x, z (0x543ac0); `a = ambient * c`, `d =
  diffuse * c`; `t = a / (a - dir y * d)` (the share of the light that is
  ambient: 0 when a is 0, a when the divisor is 0); grey = `ftol(255.5 - (1
  - t) * fade * 255)` clamped to 0..255, packed (0x579370).
- ground: the height at vt+0x50()'s +0x41c +0x13c when vt+0x50() is not
  null, else the mech's own height (position y).
- position, draw (light +0x84, ground), upload.

Which entries: every one in the list. vt+0x2c is the object itself for the
vehicles and field bases (vtables 0x5c88e8, 0x5c23d8), the turrets
(0x5c8770) and the pocket commander's marker (0x5ccfe0, a plain AiTarget);
their vt+0x50 is 0x40bae0 (`xor eax, eax`), so they take 32x32 casters and
their own height as the ground. Only a mech's vt+0x50 answers (itself),
with its skis' centre ground. The program asks its registries the same
(`shadow_entity` in app_mech.cpp); the stealth object is not ported. Each
frees its caster at its end (0x4d58a0): the mechs', vehicles' and turrets'
destructors, the pocket marker's at the mission's end (0x49dc40).

So the shadow is as dark as the light the sun takes away: the share of
diffuse light; under a cloud it is lighter; it fades to white from 50 to
100 m (only the colour: the image is drawn at full size up to 100 m).

## Projected lights (light flags 0x10 and 0x40)

Which lights: an enabled light in the model's list (it reaches the model,
0x543d90) with flags 0x10 (positioned) and 0x40. Flag 0x40 is set by the
light setter 0x4fac70 from a light-state event's SUBDIVIDE (event flag
0x200, 0x503589). In the shipped missions 164 of the 1,221 light-state events
set it (`explore/light_state_survey.py`): the weapon lights (`yellow_ultra_light`,
`yellow_lbx_light`, `ppc_light`, `ppc_bluelight02`, `green_gaus_light`,
`laspuls_light`, `yellow_flame_light`, `ams_light`), `flick_light`, and t1's
`cave_plight01..06`.

The model draw sets its projected flag when node +0xbc < 2, the opacity is 1,
model flag 1 is set and such a light is in the list; a textured polygon
without texture alpha takes it (`spot`, which also makes 0x545720 skip the
0x40 lights for that polygon). After the clips, 0x547740 draws it if it
can; otherwise the polygon is drawn as usual. A strip asks 0x547540 first.

**0x547540 (ECX count)**: the polygon's TL vertices unprojected to camera
space (0x523590), their centre and radius (the approximate square root of
the largest squared distance); 1 when some projected light's
`(far + radius)^2` exceeds its squared distance to the centre.

**0x547740 (ECX count, EDX per-vertex; flags, material, priority)**:

1. The same sphere; lights that do not reach it leave the list (the last one
   takes the slot and is tested in turn). None left: return 0.
2. Per vertex a record (0x84 bytes at 0x65ddf8): camera position, screen
   position, UVs (texture and shadows), fog, light `1 - darkness` (vertex 0's
   unless per-vertex) plus `point / 255 * point colour` (flag 4), clamped at
   0, the vertex colour; then 0x547060 adds every projected light:
   `colour * (diffuse + ambient)`, times `((far - d) / (far - near))^2`
   between near and far (d by the approximate square root), nothing beyond
   far.
3. The centre gets the mean of every attribute and is lit the same way. A
   fan around it: triangle i = (i, i + 1, centre), with links to its two
   neighbours and none across the outline.
4. Six passes over the triangles there at the pass's start: an active one
   whose centroid lies more than sqrt(10) from a corner and that some light's
   falloff band crosses (`d < far + r` and `near - r < 0` or `d > near - r`)
   is split at the middle of its longest edge (0x547160): the new vertex
   takes the mean of the edge's two, is lit, and the triangle across the edge
   is split at the same point. Otherwise it goes inactive.
5. New vertices are projected (0x523540); every vertex's z (and so rhw)
   takes the priority bias; midpoints on the outline move one pixel away
   from the centre on screen (against cracks with the polygons around).
6. Each triangle goes to [0x72f3cc] 0x57ea20 (light multipliers, flags, 3,
   texture, priority, opacity, overlay) or, with shadows, [0x72f3dc]
   0x57fe20.

**0x57ea20** is 0x57d690's tail with the colour `clamp(magic(colour * light +
0.5))` per vertex and the alpha `ftol(0.5 - opacity * -255)` (rounded, where
the other draws truncate).

## Quirks

- **The shadow images are blank with a 24-bit FPU.** 0x55d010 turns
  every edge into 16.16 fixed point with the 1.5 * 2^52 rounding trick, which
  needs double precision; at 24-bit precision (a Direct3D 6 that cuts the
  FPU to single) every row comes out 0 and no polygon is filled. On 12 c1 objects the
  original's 0x542e70 gives 439 to 2,273 shaded pixels (10 shades,
  0x8430..0xffff) at 64 bits and none at 24: a white image
  (`explore/shadow_image_x87.py`). Multiplying by white changes nothing, so
  a 24-bit FPU would draw the mech shadows (every pass runs) invisibly. In
  play on Windows 11 + DDrawCompat they are visible (Owen, 2026-10-20), so
  the FPU is not at 24 bits there; the port defaults to k53 and keeps k24
  as a switch; test_shadow_image.py checks both against the original, and
  mechlib's mechs all cast with k53 (`explore/shadow_mechs.py`).
- **Projected-light triangles are black at the same precision.** 0x57ea20 and
  0x57fe20 round the lit colour with the same trick: diffuse 0 at 24 bits,
  correct at 64 (`explore/projected_color_x87.py`; the drawtable port has the
  same for point-lit textured vertices). So the ground around the weapon
  lights above is cut into small triangles and drawn black. **[unverified
  in play]** The port keeps both, as the original runs (fixes.md).
- Shadows only fall on lit (model flag 1), fully opaque, textured polygons
  whose texture has no alpha: never on untextured polygons or alpha-textured
  foliage.
- The caster's own subtree is drawn with shadows off: no self-shadowing.
- Seven shadows per polygon at most, taken from the last caster in the list
  backwards.
- A polygon on the light's side of the caster slides wholly onto the caster's
  ground, so shadows also reach up walls and slopes facing the light; the
  2 * |slope| margin moves that line past the caster.
- The image spans the caster node's own bounding box slid onto the ground,
  and 0x543330 drops a model with any vertex outside the image whole: a
  part of the mech that reaches past the box goes missing from the shadow
  instead of being clipped (emulator). The mech roots' boxes (mechlib) hold
  their rest pose: there no part drops; a part swung past the box in an
  animation would.
- The 3x3 count over the image works in place, so its left and upper
  neighbours count by their low bit (still the fill's 0 or 1); the border
  rows and columns are never counted and come out white. The result indexes
  a 10-step grey ramp down to the shade.
- The shade is the share of the sun's light that is diffuse, where the clouds
  put it, faded to white from 50 to 100 m (0x4814e0); at the game's 24-bit
  x87 the cloud pattern is a constant (render-lighting.md), so under the
  clouds the shade does not change either.
- A new caster is linked after the list's head, not at it: the first caster
  made stays first.
- The image size is fixed when the caster is made (64 for the player's mech,
  32 for the others) and the resize path of 0x542d30 crashes: it clears the
  image pointer, then frees through it (reads address 0x10). Never reached in
  1.2, as only a new caster gets a size; FIX (bug) in the port: the old
  image is freed.
- 0x55d010 lists a level edge whose row centre is not above it (y exactly a
  row centre, or within 2^-17 above one) with its first row but no x or
  slope: the stack's garbage, used for that row. FIX (bug) in the port: the
  edge's end x, no slope (the next edge's x there); the test makes the
  original do the same (counted). With no vertices its second walk loops
  forever; the port returns (FIX (bug); only a 0-vertex polygon whose stale
  first three face up gets there).
- 0x542e70 leaves [0x74a3c8] (the software fillers' winding) at 1 and the
  software target on the last image (harmless on Direct3D: 0x481790 sets the
  target right after).
- The shadowed draws ignore the node opacity and the overlay flag, and
  ALPHABLENDENABLE bypasses the state cache (harmless: the cache holds 0
  outside the flushes, as the device).
- 0x547740 has room for 2,048 triangles and 6,144 vertices and no check: a
  large polygon in a light's band can overrun them into the light count and
  the vertex records (the test hit it 16 times in 4,500 draws with random
  lights; whether a shipped light can is not checked). FIX (bug) in the
  port: a split that would not fit is not made (the test makes the original
  do the same at that point).
- 0x547160 looks for the shared vertex in the triangle across the edge
  without a bound; it is always found, the port stops at 3 (FIX (bug)).

## Check (test_shadows.py, 24-bit FPU)

1. The five draws with the Direct3D vtable stubbed as in test_drawtable.py
   (plus SRCBLEND / DESTBLEND, COLOROP, TEXCOORDINDEX, the filters and the
   multi-texture FVF): 7,500 calls in 250 sequences with random polygons,
   shadow sets 0..7, light multipliers, flags, opacities, overlays, caches,
   next to the queue and overlay limits; every DrawPrimitive (primitive,
   DrawState, the interleaved vertex bytes), the TL buffer, caches, state,
   queue and overlay entries identical.
2. The caster lists straight on 1,500 random lists (ties on the box edges,
   NaN and infinite boxes, disabled casters, up to 260): lists, slopes and
   cell lists bit for bit. test_scenewalk.py runs them inside the walk on all
   six worlds with records over drawn nodes (the 200 cap reached 13 times).
3. The model draw (test_modeldraw.py's harness: the world read by both, the
   original's 0x53f2e0 with the table stubbed by recorders that now also log
   the shadow sets and light multipliers) on c1, c4b and t1, 500 draws each:
   shadows on, 0..9 casters around the model, 0..5 projected lights near it
   (some saturated, some plain point lights), random camera offsets, uniform
   fog for the uniform shadowed draw. Every call identical within the model
   draw's tolerance (99.6% of floats bit-identical); 1,613 polygons through
   0x547740 (up to 1,769 triangles), 276 strips through 0x547540, shadow sets
   of 1 to 7.

Breaks, each caught (one-line changes): receiver line sign, top test on the
box bottom, UVs from the far edge, six sets at most, slide only below the
ground; fog left off after the passes, UV set k - 1, rhw from the TL rhw,
address cache not updated, 0x57ea20 opacity truncated, 0x57f2a0 per-vertex
darkness; linear falloff in 0x547060, split threshold 12, outline pushed
inward, a dropped light not re-tested, new neighbour always active, priority
on new vertices only, 0x547540 reach by near; cell-list ties kept, 199-caster
cap, transposed view in 0x543760.

## Check (test_shadow_image.py)

The caster module, its images and uploads, and the upkeep, against the
original in four precisions: `game` (the original at 24 bits, port k24),
`fix` (24 bits with 0x55d010 alone at 53, port k53), `x53` (the original at
53 bits throughout, the CRT's setting and apparently the reference
runtime's, port k53) and `full` (64 bits, port k53). The original's 0x55d010
gets a zeroed stack and the level-edge FIX at its four garbage paths;
DirectDraw is stubbed as test_texload.py does, so 0x57b130 / 0x57baa0 /
0x57bb00 run for real and each upload's RGBA is decoded from the surface.

1. 0x55d010 alone, 1,500 random polygons per precision (convex either way
   round, concave, crossing, level edges, vertices on and just off row
   centres, both windings, 64x64 / 32x32 / 17x9): every pixel identical in
   all four (0 of 1,500 filled anything at 24 bits, 811..832 at 53/64).
2. The casters on all 57 mechlib trees, 3 runs each per precision: real suns
   of the six worlds (after the original's light update) and random lights
   (some level, up or NaN), random poses, grounds, sizes 64/32/16, 1 to 3
   casters on the root or any node, flags and dirty caches toggled, the LoD
   morph, nodes turned LoD / no-cast / bad class, polygons turned strips
   (mechlib has neither), 1 to 3 frames, then release and delete in random
   order. After every call: the list, every record field, the image bytes,
   the texture records, the module's globals; after each draw every node's
   flags, class data and model header; every texture call and its RGBA; the
   zerr reports. `game` and `fix`: all bit-identical (game: 498 images drawn,
   none shaded; fix: 260 of 494 shaded). `x53` / `full`: records and node
   data within 1.6e-6 and 1.4e-5 relative (box scales by their widths), 48 / 24 images
   with 384 / 216 pixels apart out of ~520 drawn: the port's float code
   against the x87 at double or extended precision.
3. 0x4814e0 with every call out recorded (calltrace.py): 400 random worlds
   per precision (24, 53, 64 bits): no sun, no light, 0..8 objects, the
   player's own, controllers with grounds or none, distances around 50 and
   100 m, existing casters, clouds, zero ambient. ~11,000 calls each, every
   trace identical in all three, and the +0xcc writes.

Breaks, each caught (one-line changes, 26 of 26): filler row start
+0x8000, winding ignored, level-edge x 0, last row -0x8000, span right
-0x8000, precision switch ignored, target pitch + 2; the count's left
neighbour whole (a crash: past the ramp), the count without the pixel
itself, ramp step not inverted, facing test reversed, strip winding, image
offset 0.5, box top + 1, corners slid by the position's height, a new
caster at the head, the reload recreating the texture, a NaN light
returning, a LoD at 0 skipped, a clean cache pushed as a copy, 0x51fee0's
column-2 sum order, the reload with alpha; the upkeep's sizes swapped, full
shade up to 52 m, the shade by diffuse, the ground always the mech's own.

## Corrections to render.md

- Section 3 table: [0x72f3cc] 0x57ea20 and [0x72f3dc] 0x57fe20 are the
  projected-light triangle draws of 0x547740 (0x5481eb is inside it), not
  puffers. [0x72f3d4] / [0x72f3d8] take ECX flags, EDX count; [0x72f3d0] ECX
  count, EDX texture.
- Section 3 step 5: the projected-light test reads node +0xbc, not the
  camera's.
- Section 5, projected shadows: the casters are the mechs within 100 m of the
  player and the player's own (0x4814e0); the list head is 0x64fbe0, linked
  through +0x88; the receiver's box test also needs the polygon to face the
  light and not lie above the caster's top; up to 7 shadows.
- Section 6, fog quirk: 0x57fe20 sends FOGENABLE as part of the projected
  lights, 0x57eeb0..0x57f860 as part of the shadows.

## Open

- Shadows confirmed visible in play (Owen, 2026-10-20). The other 24-bit
  findings (projected-light triangles, point-lit vertices, clouds) are the
  lead's to revisit; whether Windows 98's DirectX 6 hid the shadows stays
  unconfirmed.
- 0x579b70(3), the device check 0x542bb0 makes before turning shadows on:
  read against the D3D6 layouts, 0x809438 & 1 and 0x80943c & 4 are the
  triangle caps' src blend ZERO and dest blend SRCCOLOR (the multiply),
  0x809454 & 4 the address caps' CLAMP, and 0x8093c8 & 0x40 the misc caps'
  0x40 (CULLCCW, if the base is right) **[unverified]**; the 1 it returns
  when 0x8026e0 is 0 is odd. Without them shadows stay off and, per the fog
  quirk, hardware fog never comes on. The port's `zvid::Display` has them set.
- The entity side (2026-10-21): the program runs the upkeep on its own
  mechs. `GameEngine::update_shadows` calls `update_mech_shadows`, which is
  0x4814e0 over zEntity's list with the program's types:
  - the mechs come from the registry (vehicles.md);
  - the sun is the mission's "sunlight" node ([0x6098d4] = the mission's
    +4), and its light is the node's `zcls::Light` (+0x84 the direction,
    +0x9c diffuse, +0xa0 ambient);
  - the position is 0x5a85e0 (`entity_position`);
  - the ground is the mech's skis' +0x13c (`ground[6]`), since vt+0x50 of
    a mech is the mech itself;
  - the caster is AiTarget +0xcc, now a `zmod::ShadowCaster*`.

  The fade and grey sums are shared with the handle version, which
  test_shadow_image still checks against the exe. The caster is freed where
  the exe frees it, in zEntity3d's base end (0x4d58a0: 0x542e40, then
  0x542ca0). `test_program`'s `mission_c1m1_mechs_cast_shadows_headless`
  runs c1m1's Direct3D frames and checks four things:
  - the player's 64x64 caster is drawn (more than 100 pixels shaded);
  - every other mech has one exactly while within 100 m;
  - those others are 32x32;
  - the list is empty after the cleanup.

  Breaks: without the upkeep, without the release, and with the sizes
  swapped, the test fails. Vehicles are skipped until they are ported;
  they cast in the exe.
