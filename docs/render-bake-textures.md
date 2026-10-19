# Baked static light and world texture loading

Mech3.exe 1.2, 2026-10-19. Two pieces the Direct3D model draw needs: the
baked static light 0x544ed0 (polygon +0x18) and the loading that gives a
world's texture table its images and Direct3D texture records. Ported in
`src/gamez/zmodel/gmod_bake.*` and `src/gamez/zimage/zimg_texture.*` (with
the library lists in `zimg_library.*`); checked by
`tests/differential/test_bake.py` and `test_texload.py`. **[unverified]**
marks what was read from the asm only.

## Where the code lives

| Address | Port | Original file (func2file.json) |
|---|---|---|
| 0x544ed0 | `zmod::bake_static_light` | between gmod_light.c and gmod_puff.cpp (gmod_light.c, likely) |
| 0x524020, 0x51e110 | local `poly_normal`, `scale_inverse` | maths helpers between zin_init.cpp and gmod_init.c |
| 0x519080, 0x5190d0 | `zimg::load_group`, `load_group_0` | between zimg_texture.cpp and zimg_fonts.cpp |
| 0x518f70 | `load_texture` | same |
| 0x518ee0, 0x518e50 | `fetch_image`, `clear_transparent` | same |
| 0x519270, 0x519210, 0x519150 | `link_mips`, `set_entry_name` (+ cut extension) | same |
| 0x518220 | `find_texture` | same |
| 0x5183b0, 0x5184c0, 0x518670, 0x518630, 0x518600, 0x518d60 | `new_image_library`, `register_image_library`, `open_all`, `close_all`, `close`, `find_in_list` (zimg_library) | same |
| 0x517d00 | `release_buffers` (zimg_library) | between zimg_library.cpp and zimg_texture.cpp |
| 0x557ed0 | `software_prepare` (kept in zimg_texture) | between zreader.cpp and zrndr_draw.c |

## 1. The baked static light, 0x544ed0

### Who calls it and when

Only the Direct3D model draw, at 0x53fbc7, once per draw call of a model
whose cache is due (0x53fb7d..0x53fbdf):

```
if [0x5ef740] (static lights on) and model type (+0) == 0 and model +0x0c < 2
   and needs bit 2 (lights reach it, 0x53daa0):
    if node +0xc0 >= [0x5e9e2c]:       baked = on            (cache valid)
    elif node +0xc4 <= frame [0x72f338]:
        0x544ed0(ECX model); 0x4ed980(ECX node): node +0xc0 = [0x5e9e2c]
        baked = on
    else:                              baked = off
else baked = off
```

- `[0x5e9e2c]` is the light-change stamp: 0x4ed960 sets it to frame + 1
  (unless [0x60b94c] freezes it). Its callers are the Light.c setters
  (0x4fa8e7..0x4fad24) and 0x4eb1e7. zClass startup 0x4f8f20 sets it to 1;
  node +0xc0 starts at 0 (0x4ed990), so every model bakes on its first lit
  draw. A frame later the cache stays valid until a light changes.
- Node +0xc4 is a "not before frame" stamp (0x4ed990 copies [0x73f3a8]);
  until then the draw runs without baked light, so the static lights are
  then applied live by 0x545720 (it skips flag 0x100 lights only when given
  a baked array).
- `[0x5ef740]` is 1 after the model renderer's init (0x526700 region, the
  port's `DrawState::static_lights`) and is toggled by a pair of callbacks,
  0x4e6720 (off, and [0x60b94c] = 1: stamps frozen) and 0x4e6770 (on,
  unfrozen), installed through 0x4f7630 / 0x4f7650 by 0x4e6840
  **[unverified: what event they mark]**.
- With the bake on, the draw passes polygon +0x18 to 0x545720 for polygons
  without flag 0x100 (two-sided ones are never baked: the bake skips them).

For gmod_draw: `DrawHooks::refresh_static` is the place to call
`zmod::bake_static_light(*model, draw_buffers().cam, draw_buffers().normals)`
and then stamp the node (0x4ed980).

### What it reads and writes

ECX = model. Reads: model +0x08 (flag 0x20: no sun term), +0x10 polygon
count, +0x34 polygons; the model's camera-space vertices `[0x74639c]` and
rotated normals `[0x7463a0]` (filled by the draw just before); the light list
(0x749824, `[0x74981c]` entries) with each entry's full/reach as 0x543d90
left them for this model; per light +0x78 camera position, +0x90 camera
direction toward the light, +0x9c diffuse, +0xa0 ambient, +0xb0 flags,
+0xb4..+0xc4 ranges. Writes: polygon +0x18 (`count` floats, **file order**),
and as scratch the draw's polygon buffers 0x7476a4 (vertices), 0x747ca8
(normals) and the flag 0x747ca4 (all refilled by the draw per polygon; the
port uses clip3d_buffers().cam and its own normal buffer).

Per polygon without flag 0x100 (n vertices):

1. Copy the vertices (file order, not reversed); copy vertex normals when
   polygon flag 0x200 and the model has no flag 0x20 (no check of
   SetVertexShading: `[0x7463a0]` is read whenever the polygon has normals).
2. `out[i] = 1` for every vertex.
3. For each listed light with flag 0x100 (static):
   - **Directed** (flag 8), no reach test: `t = min(1, ambient)` with model
     flag 0x20; else `t = N.L > 0 ? min(1, diffuse * N.L + ambient) :
     ambient` with N the polygon's unit normal (0x524020 on vertices 1, 2, 0)
     or each vertex's normal. `out[i] += t`. Marks "directed seen".
   - **Point/spot** (reach set): the polygon factor is 1 (point) or, for a
     spot (flag 4), `N.L` (0 unless above 1/255) and then, for vertex 0, the
     dot of the light direction with the **previous** light-to-vertex vector
     (the stack slot [ebp-0x1c], set by the last light or polygon).
     Skipped when neither that factor nor ambient is positive. Intensity
     `t = min(1, diffuse * f + ambient)` (ambient when f <= 0). Then vertex
     0, and the other vertices last to first: when the entry is not full,
     the vector light - vertex and its squared distance; beyond far^2 the
     vertex gets nothing; inside it the distance is the approximate square
     root (spot: always, and the vector is normalised; point: only at or
     beyond near^2, else the **squared** distance stays, the near^2 quirk of
     render-lighting.md). Range factor 1 within near, `(far - d) / (far -
     near)` up to far, 0 beyond; full entries take `t` straight. For vertices
     after the first a spot recomputes its factor per vertex from that
     (normalised, or for full entries unnormalised) vector.
4. If any directed static light was seen, `out[i] -= 1`.

So with a sun the array holds `sun + point lights` per vertex; without any
directed static light it holds `1 + point lights`. 0x545720 then uses
`darkness = 1 - cloud * baked`.

### Quirks

- **Instances share one bake** (player-facing, **[unverified in play]**).
  The cache flag is per node (+0xc0) but the array is in the model's
  polygons, and the value depends on the node's orientation (camera-space
  normal against the camera-space sun). When several nodes use one model
  (c1: 678 of 2,737 object3d models are shared, one by 58 nodes), each
  node's first draw re-bakes and the others keep their "valid" flag: every
  copy is lit as the last-baked copy faces until a light changes. Copies
  placed at the same rotation look right; rotated copies (rocks, trees,
  buildings turned on the map) get another copy's sunlight.
- The spot factor of each polygon's first vertex uses a stale vector (the
  previous light's or polygon's last vertex, or whatever the stack held
  before the call). The port carries it in `BakeState::stale`.
- Static point lights use the squared distance inside near^2, as the live
  shading does (render-lighting.md).
- Directed static lights are never range-tested and need no reach entry.
- A polygon with 0 vertices would loop 2^32 times (do-while copies);
  more than 64 vertices overrun the polygon buffers. Neither occurs in the
  shipped worlds.
- 0x524020 stores a zero-length normal as (y, z, x) (the x87 stack order);
  the components are zero anyway.

## 2. World textures into the draw table

### Call chain at mission load

```
0x48fcfe  0x5182a0("texture", variant, r prefix)     list 0
0x48fd1e  0x5182a0("mechtex" | "mechtexS", .., r)    list 0
          (0x516eeb: 0x5184c0(1) -> 0x5183b0("image", 0, 1), list 1)
          ... gamez.zbd read: 0x518160 fills the table, group = [0x6384d4]
0x48fea9  0x5190d0 -> 0x519080(0)                    load group 0
mechlib:  0x4847ca / 0x484825 0x519080(3)            (docs/mechlib.md)
others:   0x5190d0 from 0x49151c, 0x501f47, 0x50d546
```

The texture system's init 0x5184e0 clears the table and, on Direct3D, makes
the default record [0x5ecbfc] from the default image 0x5ecbc0 under the name
`DEFAULT_TEXTURE`.

### 0x519080(ECX group)

Opens list 0's library files (0x518670 -> 0x5187c0 each), walks the table
(count re-read every step) and calls 0x518f70(entry, EDX 0, stack 1) on each
entry whose group equals ECX (-1: all), then closes list 0's files (0x518630;
the entries stay loaded). List 1 (the image library) opens on first use and
stays open.

### 0x518f70(ECX entry, stack free_pixels)

Only entries in state 2 (every shipped world entry) or 3:

1. `+0x00 image = 0x518ee0(name)`; `+0x24 mip = 0` (the file's mip index is
   dropped and rebuilt by name). No image: the hook [0x638520] (never set in
   1.2), else the **default image** 0x5ecbc0: 8x8, 565, columns of two
   alternating red (0xf800) and half-green (0x03e0) per row.
2. 0x519270: the mip chain (below).
3. Direct3D ([0x72f348]):
   - one of the three entries at 0x74a2b4 (0x542710, the projected-light
     textures **[unverified]**): 0x557ed0 only, state 1;
   - state 3: [0x72f374] = 0x57baa0 reloads the image into the existing
     record (not ported; a hook), state 1;
   - no record yet: `+0x04 = [0x72f364](ECX name, EDX image, alpha = flags
     & 2, clamp u = stretch & 1, clamp v = stretch >> 1 & 1)` =
     0x57b130 (`zvid::create_texture`); then, when free_pixels and
     [0x72f348] != 2, 0x517d00 frees the image's owned pixels, alpha and
     palette (the record keeps the only copy); [0x638524] += 2 * texels
     (even when the record is the default one). State 1.
   - a record already: state 1.
4. Software: 0x557ed0 (span constants), state 1.

The record pointer lives at entry +0x04 (`surface`), which the model draw
reads through material +0x10 (`texture`) -> +0x04.

### Fetch by name: 0x518ee0 and 0x518d60

0x518ee0 searches the current list ([0x63851c], starts 0) with 0x518d60
(each library in registration order, 0x518db0, `_stricmp`; first hit), and on
a miss switches to the other list and searches that. The switch stays even
when both miss, so the next search starts in the other list. A found image
with alpha (flags 2) and an alpha plane gets its transparent texels zeroed
(0x518e50; palettised images are left alone).

### Mips: 0x519270

Only a name ending in `_1` (the first `_1` in the name must be at its end):
its last digit is incremented (`_2`, `_3`, ...) while a library has the name.
Each mip: looked up in the table by exact name (0x518220, plain strcmp, in-use
entries only); a missing one takes the next table slot (no bound check),
named by 0x519210 (base name, extension cut), keeping that slot's old group
and record; state 1, its image prepared (0x557ed0). Then `prev +0x24 = mip`,
`mip +0x24 = 0`, and the mip image's +0x1c = (float)(base width / mip width)
(integer division). A mip already loaded (state not 2) is linked without a
fetch, and its scale is written to the **previous** fetched image, or, on the
first step, through an unset stack slot (the port uses that mip's own image;
not reached with the shipped files, where only bases are in the tables).

In the shipped data only the software libraries (`texture*.zbd`) have mips
(`txgter_2`, `ggter_2`, `gter_2`); the hardware ones (`rtexture*.zbd`) have
only the `_1` base, so on Direct3D the chain stops at the first lookup (and
flips the list, above).

### 0x557ed0 (software span constants)

+0x0a = log2 width, +0x0b = log2 height (halvings while > 1), +0x1c = 1.0,
+0x24 = 20 - log2 width, +0x28 = width - 1, +0x2c = (height << 20) -
0x100000 (powers of two assumed).

### Quirks

- **Missing textures are a red/green checkerboard** (player-facing, partly
  **[unverified in play]**). Every world names four textures no hardware
  library has at load time (`sizzle`, `laspulstrc`, `gaustrac`,
  `gaustrac2`; c4 also `rokt06`, c4b the `lflare1..4` lens flares); each gets
  the 8x8 default image and its own Direct3D record from it. Whether
  something replaces them later (weapon effects, flares) is not traced.
  With `mechtexS` (low-detail mechs) c2 has 12 such entries, c4b 16; the
  software sets miss about 33.
- The table's own lookup (0x518220) is case-sensitive while the libraries'
  is not (`GTER_1` in rtexture3 is found for the world's `gter_1`).
- The default image is shared and never freed; software loading writes its
  span constants into it in place.
- The texture-memory count includes textures that fell back to the default
  record.

## Tests

- `test_bake.py`: the six worlds read with the original 0x4ff520 and the
  port's reader; lights updated by the original 0x4f2f60 under a random view
  (the real sun, flags 0x38b); per world 120 random models (of 841..3,344)
  placed at random in front of the camera, baked with the real sun (80%)
  plus 0-5 random lights (static or not, directed, point, spot, saturated;
  random full/reach) and, in 30% of cases, a point light whose near range is
  exactly one vertex's squared distance; model flag 0x20 forced on or off in
  half of the runs; the stale stack slot seeded. 720 models, ~23,000 vertex
  values (2,500 polygons with vertex normals), identical within 1e-6 at
  24-bit, and the stale vector after.
- `test_texload.py`: eight runs (c1..t1; texture variants 0..3; mechtex and
  mechtexS; Direct3D with a 256 power-of-two card or a 64x128 card without
  4444, and software with the unprefixed libraries): the original 0x518160,
  0x5182a0, 0x5184c0 and 0x519080 with 0x57b130 behind DirectDraw stubs,
  against the port with a recording Render3D. The whole table (names,
  states, groups, mip links, image identity and fields, record type and
  address modes), the current list, the memory count, the reports (39 and 85
  refusals on the small card) and every upload (size, RGBA) are identical:
  2,849 uploads.

Breaks caught: bake without the final `- 1`, the fresh vector for the first
vertex's spot factor, the near^2 quirk fixed, a reach test on directed
lights, the 0x20 ambient unclamped, normalising the full spot vector (the
stale vector after); texture loading without freeing the pixels after the
upload, the memory count skipping default records, transparent texels kept,
the alpha argument from the wrong flag, clamp u/v swapped, the mip scale
inverted. Not observable with the shipped files: `>=` instead of `>` at the
bake's near boundary (the falloff is 1 there), undoing the list switch after
a double miss (only the search order changes: no name is in both lists), a
case-insensitive table lookup (no case collisions).

## Open

- What 0x4e6720 / 0x4e6770 mark (bake off and stamps frozen, then on).
- Model +0x0c < 2 (the bake gate); node +0xc4's source [0x73f3a8].
- Who fills the three projected-light entries at 0x74a2b4; who sets state 3.
- Whether `sizzle`, `laspulstrc`, `gaustrac*`, `lflare*` get real images
  later (0x518d10 loads by name on demand, from 0x473df4).
- 0x57baa0 (reload) and the software shading copies (0x519740).

## For docs/discoveries.md

- Copies of the same object share one set of baked sunlight: when the game
  first draws each copy it re-bakes the shared model for that copy's
  orientation, so all copies end up lit as the last one faces until a light
  changes. **[unverified in play]**
- Textures the game cannot find show as a small red-and-green checkerboard;
  every mission's texture list names a few weapon-effect textures
  (`sizzle`, `laspulstrc`, `gaustrac`) that the hardware texture files do not
  contain.
