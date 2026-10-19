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

`zvid::install_model_draw_table` now fills the five entries too
(`zmod::DrawTable::shadowed*`, `projected`, `projected_shadowed`). The
shadowed clippers (0x528bb0, 0x529120, 0x52e920, 0x52fab0) were already
ported and now get real UV sets.

Not ported (mapped below): the caster records' owner, 0x542bb0..0x543330
("gModShadow"), the image they draw, the per-mech upkeep 0x4814e0 (mech3
side), and the software polygon filler 0x55d010 the image uses.

## A frame

1. **Casters** (0x4814e0, once per frame from the Direct3D strategy; not
   ported). For each mech in the list at 0x73655c: fade 1 for the player's
   own mech, else by its squared distance to the player: 1 below 2,500 (50
   m), 0 from 10,000 (100 m), linear between. Fade 0 releases the mech's
   caster (mech +0xcc). Otherwise it makes one (0x542c20, 0x8c bytes, put at
   the head of the list at 0x64fbe0) with a 64x64 image for the player's mech
   and 32x32 for others (0x542d30), the mech's node (0x542dd0), enabled
   (0x542e60); sets its shade, from the cloud pattern at the mech and the
   sun's diffuse and ambient, as a grey (0x579370, 0x542e00), its position
   (0x542de0), then draws the image (0x542e70, below) along the sun's
   world direction (light +0x84) onto the ground height, and uploads it as
   the texture `gModShadow` (0x542e10: 0x57b130 with clamped U and V, no
   alpha; later frames reload it).
2. **The list for the frame** (0x4f7030, when shadows are on [0x74a264]):
   0x543720 takes the enabled casters from the list, newest first, into
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
| +0x5c / +0x60 | the image (zimg, 16-bit) and its texture record |
| +0x84 / +0x88 | previous / next in the list |

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

- **The shadow images are blank at the game's FPU precision.** 0x55d010 turns
  every edge into 16.16 fixed point with the 1.5 * 2^52 rounding trick, which
  needs double precision; at the 24-bit precision Direct3D 6 leaves the FPU
  in, every row comes out 0 and no polygon is filled. On 12 c1 objects the
  original's 0x542e70 gives 439 to 2,273 shaded pixels (10 shades,
  0x8430..0xffff) at 64 bits and none at 24: a white image
  (`explore/shadow_image_x87.py`). Multiplying by white changes nothing, so
  on hardware the mech shadows are drawn (every pass runs) but invisible.
  **[unverified in play]**
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
  instead of being clipped. **[read from the asm]**
- The 3x3 blur over the image works in place, so its left and upper
  neighbours are already blurred; the result indexes a 10-step grey ramp.
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

- Port the caster module (0x542bb0..0x543330, 0x4f7670 / 0x4f7750, 0x55d010)
  and 0x4814e0, then decide with Owen whether its 16.16 rounding (and the
  projected and point-light colours) get a FIX (bug) so shadows and weapon
  light pools show.
- Confirm both 24-bit findings in play (DDrawCompat on Owen's PC): look for
  mech shadows on the ground and for the ground near PPC or autocannon fire.
- 0x579b70(3), the device check 0x542bb0 makes before turning shadows on:
  which caps bits 0x8093c8 & 0x40, 0x809438 & 1, 0x80943c & 4, 0x809454 & 4
  are (likely the blend and texture-op caps the multiply needs). Without
  them shadows stay off and, per the fog quirk, hardware fog never comes on.
