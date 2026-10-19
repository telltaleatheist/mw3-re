# The Direct3D draw table

Ported: `src/gamez/zvideo/zvid_ddd3d.{h,cpp}`. The draw functions the model
draw calls through `[0x72f3b0]..` (filled by 0x579f50), the render-state
cache, the transparent flush (0x57e110) and the overlay list (0x57e4c0), the
point-light colour (0x5798a0), FOGCOLOR (0x57bcf0), the frame calls (0x57a740,
0x57a7e0, 0x5777f0, 0x57a850) and texture records (0x57b130). Checked by
`tests/differential/test_drawtable.py` (probes in `tests/probe_drawtable.cpp`)
against the original in Unicorn at 24-bit FPU precision.

## How the port talks to the device (FIX (A))

The original's code is kept call for call: the same caches (`DeviceState`,
0x809388..0x8093a8) decide when a state is sent. A "sent" state now sets a
field of the `platform::DrawState` the device holds (`draw_state()`), and each
DrawPrimitive becomes `Render3D::draw(primitive, vertices, count, state)`:

| D3D call | DrawState |
|---|---|
| SetRenderState FILLMODE (8) | `wireframe` = value 2 |
| SHADEMODE (9) | `flat` = value 1 |
| ZWRITEENABLE (14) | `depth_write` |
| ZFUNC (23) | `depth_always` = value 8 (ALWAYS); 7 is GREATEREQUAL |
| ALPHABLENDENABLE (27) | `blend` |
| FOGENABLE (28) | `fog` (not sent by these functions) |
| FOGCOLOR (34) | `Render3D::set_fog_color` |
| DITHERENABLE (26) | dropped (not reproduced, render.md section 4) |
| SetTextureStageState ALPHAOP (4) | `alpha_modulate` = value 4 (MODULATE); 2 is SELECTARG1 |
| ADDRESSU / ADDRESSV (13 / 14) | `address_u` / `address_v` |
| SetTexture(0, t) | `texture` = record's handle, -1 for none |

`Render3D::draw` has no HRESULT, so the error reports after DrawPrimitive
(0x580550, lines 0x5f0, 0x781, 0x949, 0xb6f, 0xc23) are gone, as are
DDERR_SURFACELOST restores. The texture record keeps `{handle, type,
address_u, address_v}` (the original's +0x08..+0x14); the two surfaces, the
QueryInterface and the Load become one RGBA8 `Render3D::create_texture`.
`zvid::TLVertex` is now `platform::TLVertex` (field `diffuse`, was `color`).

The draw functions read the TL buffer (0x72f41c) and the work records
(0x7481a8) through `draw_buffers()`, which defaults to `zmod::clip2d()`'s `tl`
and `work`; the model draw port can point it at its own buffers. The
Clip3dBuffers / Clip2D merge was not done: the model-draw port being written
at the same time uses those headers.

## The draw functions

ECX, EDX, then the stack, as the model draw passes them (render.md section 3).
`w[i]` is the work record of vertex i (+0 darkness, +4 point-light intensity,
+8 fog, +0xc colour), `round(x)` is `_ftol(x - -0.5)`, `fogA` is `_ftol(255.5 -
255 * w[i].fog) << 24` (0xff000000 without fog flag 1). Every function writes
`rhw = sz` and the specular into the TL buffer in place, then queues, adds to
the overlay list or draws.

| Function | Args | Colour per vertex | Transparent when | Overlay types (transparent / opaque) |
|---|---|---|---|---|
| 0x57bf80 flat | packed u16 colour, alpha, count, priority (unused), overlay | the packed colour unpacked with the display masks (0x80272c..) and shifts: `(c & rmask) >> rshift` etc., so 0xf8-style 8-bit values; alpha in the top byte; specular 0xff000000 | alpha < 255 | 0 / 1 |
| 0x57c700 untextured lit | packed colour (**unused**), &material rgb (floats 0..255), flags, alpha, count, priority (unused), overlay | `d = 1 - w[0].darkness` (**first vertex**); flag 4 and `w[i].point > 1/255`: `c = light * point + d * rgb`, scaled by `255 / c[k]` when `c[k] > 255` with k = the light's brightest channel [0x802704], **truncated**; else `round(d * rgb)` | never: alpha < 255 is **dropped** | - / 3 |
| 0x57ce20 textured | count, texture, opacity (float), overlay | `round(w[i].color)`, alpha `_ftol(opacity * 255)` when opacity < 1 | texture type 4 or opacity < 1 | 0 / 4 |
| 0x57d160 textured lit | flags, count, texture, priority (unused), opacity, overlay | `d = 1 - w[0].darkness` (**first vertex**); flag 4 and `w[i].point > 1/255`: `clamp(mround((point/255 * light + d) * color + 0.5))`; else `round(d * color)` | as 0x57ce20 | 0 / 5 |
| 0x57d690 per-vertex lit | flags, count, texture, opacity, overlay | as 0x57d160 with `d = 1 - w[i].darkness` | as 0x57ce20 | 0 / 6 |

`mround` is the magic-number round: `x - -1.5 * 2^52` stored as a double, its
low dword taken; `clamp` limits to 0..255 (signed). The `round` and truncating
paths do not clamp: a channel above 255 ORs into its neighbour (each byte is
`(r << 8 | g) << 8 | b`), and the alpha is ORed on top.

State, opaque draws: 0x57bf80 SetTexture(null), SHADEMODE 1, draws from its own
vertex buffer 0x80c59c (the TL buffer is left as it was; tu, tv in that buffer
are stale); 0x57c700 SetTexture(null), SHADEMODE 2; the textured three
SHADEMODE 2, ALPHAOP SELECTARG1, SetTexture, ADDRESSU/V (cached). No function
touches ALPHABLENDENABLE or ZWRITEENABLE except the flushes.

Queue and overlay: transparent polygons go to the queue (the reserve block,
render-fogsort.md), with overlay set to the overlay list (0x8dfc9c, 0x180
entries of 0x80c bytes `{type, count, texture, 64 TL vertices}`, count
[0x809318]). A full list reports `"Not enough ZVID_MAX_OVERWRITE_POLYS: need
%d"` (0x5d0 says "needs") with the list's **current count**, level 0x400;
lines 0x5f9 / 0x5d0 (flat), 0x760 (lit), 0x88c / 0x856, 0x953 / 0x925,
0xa23 / 0x9f2 (textured, transparent / opaque).

## The overlay flush 0x57e4c0

ZFUNC ALWAYS, then per entry, all with the **current primitive [0x72f418]**
(the entry does not keep its own):

- 0: Gouraud, blending on, Z writes off; SetTexture; with a texture, ALPHAOP
  from the cache compared with the type when the type is 4 or the first
  vertex is opaque (`diffuse & 0xff000000 == 0xff000000`), else MODULATE;
  address modes. After the draw, blending off and Z writes on again.
- 1, 2, 3: SetTexture(null), **flat**, draw.
- 4: **flat**, SetTexture, ALPHAOP from the type compare (always MODULATE),
  address modes, draw.
- 5, 6: Gouraud, SetTexture, ALPHAOP SELECTARG1, address modes, draw.

Then ZFUNC GREATEREQUAL and the count reset. Opaque entries keep Z writes on:
they are drawn on top but still write depth.

## Frame calls, fog colour, point light

- 0x57a740: BeginScene, then FILLMODE 3 (fill mode 0) or 2 (1) when [0x8026f8]
  bit 1 is set, DITHERENABLE [0x802700] when bit 2 is; each bit is cleared.
  The nesting counter at [0x80270c] lives in the callers 0x579af0 / 0x579b10.
- 0x57a7e0: EndScene; on SURFACELOST DirectDraw RestoreAllSurfaces.
- 0x5777f0 (ECX rect, EDX surface record): colour fill with [0x802794] when
  [0x8026f0]; depth fill 0 only when there is a Z buffer and the record is the
  back buffer (0x8027a4). The port clears through `Render3D::clear` for the
  back buffer only.
- 0x57a850 (ECX/EDX rects, stack wait, blit): 0x400 without a primary; with
  `blit` set, Blt from [0x8027e8] into the back buffer first; then Flip,
  retried on WASSTILLDRAWING, Restore on SURFACELOST. The blit is not ported
  (what [0x8027e8] holds is not mapped).
- 0x57bcf0: FOGCOLOR = `round` of the sent colour [0x72f400..] (0..255).
- 0x5798a0 (ECX rgb 0..1): light colour [0x72f40c..] = rgb * 255; **only when
  [0x8026e0] is set** it also picks the brightest channel [0x802704] (ties go
  to the lower index; NaN compares as "less").

## Texture records 0x57b130

ECX name, EDX zimg image, stack alpha, clamp u, clamp v. Checks, each failing
to the default record [0x80931c] with a zerr 0x200 report: size above the
device's max (0x257), not a power of two when the device asks (0x809448 & 2;
0x57e9e0 starts at 2, so a 1-texel side fails; a square power of two skips the
aspect test) (0x269), aspect above 8:1 (0x278), a palette (0x28e). Device
flag 0x20 (square only) resizes the image (0x519820); not ported, no modern
device asks. The port keeps the checks with a modern device's caps by default
(`Display`: 4096, no flags, 4444 supported).

Formats and type:

| alpha | alpha plane | 4444 support ([0x809644]+0xb04) | surface | type |
|---|---|---|---|---|
| 0 | any | any | display format, copied | 1 |
| set | none | any | 1555, alpha bit = pixel != 0 | 5 |
| set | yes | no | 1555, alpha bit = plane bit 7 | **4** |
| set | yes | yes | 4444, alpha = plane top 4 bits | 4 |

The port decodes the same 16-bit surface to RGBA8 (bit replication) and
uploads that; the test decodes the original's surface with the pixel format
it gave CreateSurface and compares.

## Quirks

- **Translucent untextured polygons vanish when lit or fogged.** 0x57c700
  colours a polygon with alpha below 255 and then returns: nothing queues it
  (0x57bf80, the unlit path, does). So the translucent black fake shadows
  (`shdw`, alpha 51..153, render.md section 6) and any other translucent
  untextured polygon are not drawn at all on models that are fogged or lit by
  a point light. **[unverified in play]**
- **Point-lit textured vertices are black at 24 bits.** 0x57d160 and 0x57d690
  round point-lit colours with the `- -1.5 * 2^52` trick; at the 24-bit
  precision Direct3D leaves the FPU in, the sum rounds to 1.5 * 2^52 exactly
  and the low dword is 0, so every channel is 0 (alpha kept). At 64 bits the
  trick works (round half to even of `x + 0.5`). Emulator-confirmed at both
  precisions. Only vertices with point-light intensity above 1/255 and flag 4
  take this path (coloured point lights; none in the world files, the
  animation loader adds them, render.md section 5). The port does what the
  game does at 24 bits. **[unverified in play: a scene with a coloured point
  light on textured models; candidate for a FIX (bug) if it shows]**
- One darkness for the whole polygon: 0x57c700 and 0x57d160 use the first
  vertex's darkness for every vertex; per-vertex directed light only reaches
  textured polygons through 0x57d690.
- Untextured point light is added after the material (`light * point + d *
  rgb`), textured point light before (`(point/255 * light + d) * colour`); the
  untextured one is truncated and scaled down by one channel (the light's
  brightest, which is only picked when [0x8026e0] is set, so otherwise a stale
  index), the textured one clamped per channel.
- No clamp on the unlit colour paths: a vertex or material colour above 255
  spills into the next byte of the ARGB.
- The overlay list draws with whatever primitive the last polygon left, and
  flattens types 3 and 4 (the per-vertex colours of lit untextured overlay
  polygons collapse to one).
- ARGB1555 from 565: `(p & (r|g)) >> 1` moves green's lowest bit into blue's
  top bit, so a texture with alpha and no plane (texture flags 0x03, UI
  images) gets up to +16/31 blue wherever its green is odd.
- 0x579430's pixel-format globals (0x802750..) get green mask 0x3c0 for 1555
  (the surface itself gets 0x3e0); not ported, readers unknown.
- The palette upload code after CreateSurface is dead: palettes are refused
  before.
- 0x57ce20 and 0x57d690 do not check DrawPrimitive's result; the others do.
- The transparent flush and the overlay flush test "opaque first vertex"
  differently (`> 0xff000000` vs top byte `== 0xff`).

## Corrections to render.md

- Section 3 table: 0x57c700's ECX (the packed colour) is not used; EDX is the
  material's float rgb. Its transparent polygons are dropped, not queued.
- Section 3 "Colour": the untextured lit colour uses the first vertex's
  darkness only, and the point light is added, not multiplied; 0x57d160 also
  uses the first vertex's darkness. Unlit textured colours are rounded and not
  clamped.
- Section 4, 0x57e4c0: the types are 0 any transparent polygon (blended, Z
  writes off, textured or not), 1 untextured flat, 3 untextured lit (drawn
  flat), 4 textured plain (drawn flat), 5 / 6 textured lit (Gouraud); all with
  the current primitive; opaque entries write depth.
- Section 4, 0x57b130: type 4 whenever the image has an alpha plane, also when
  the surface had to be 1555.
- Section 3 step 3 calls flag bit 4 "tint"; the draw functions use bit 4 for
  the per-vertex coloured light in work +4 with colour [0x72f40c] (set by
  0x5798a0, called from the light code 0x54646e / 0x546534). The flags passed
  to the draws are the ones 0x545720 writes (0x53ff2a), not necessarily
  0x53daa0's bits **[unverified]**.

## Test (test_drawtable.py)

- Draws: 12,000 calls in 300 sequences of 40 (random work records with
  darkness, point light at and around 1/255, NaN, fog outside 0..1, colours at
  .5 and above 255; random TL vertices, materials, packed colours, alphas
  -1..300, opacities, flags, priorities, overlay on a quarter, 16 texture
  records of types 1/4/5 and both address modes, light colours to 400, all
  three brightest indices, 565 and 555 displays, random caches and device
  state), with the transparent and overlay flushes among them and the queue
  and list started empty, near and at both limits. Compared after every call:
  every Render3D draw (primitive, DrawState, vertex bytes), the TL buffer,
  caches, DrawState, light, the new queue record and vertices, the new overlay
  entry; at the end the whole queue, list and flat buffer, stale bytes
  included.
- 150 flushes of queues filled through 0x57bf80 / 0x57ce20 (up to 600
  polygons, type 2 among the textures).
- 400 frame calls: begin/end, clear (on and off, both displays), present
  (primary or not), FOGCOLOR, point-light colour with ties.
- 500 texture records: sizes 1..8192, pow2 caps on and off, max sizes,
  palettes, alpha with and without plane, 4444 support on and off, both
  displays; the record, the reports and the decoded surface identical.

Break tests (each caught): per-vertex darkness in 0x57c700, rounding the
untextured point light, scaling by red instead of the brightest channel,
drawing 0x57c700's transparent polygons, true rounding instead of the magic
trick, transparency by type only, fog 255 instead of 255.5, copying tu into
the flat buffer, Gouraud for overlay type 3, overlay report count + 1, first
vertex darkness in 0x57d690, brightest-channel ties, the 1555 green bit, type
5 for a plane on a 1555 surface, the flush sending ALPHAOP by type.

Not ported here: the other table entries (0x57c2f0, 0x57caa0, 0x57dba0, the
shadow passes 0x57eeb0.., puffers 0x57ea20 / 0x57fe20, the 2D quad 0x57bd50,
the tint quad 0x57e380), texture release, the square resize.
