# Model lighting (Direct3D model draw)

Mech3.exe 1.2, 2026-10-19. The lighting the Direct3D model draw 0x53f2e0
uses: the per-frame light list, the light direction update, the lights
reaching a model, the model's needs bits and the per-polygon shading into the
work records. Ported in `src/gamez/zmodel/gmod_light.{h,cpp}`; checked by
`tests/differential/test_lighting.py` (Unicorn at 24-bit x87, as in play).
Complements `docs/render.md` section 5, with corrections below.

## Where the code lives

| Address | Port (`zmod::`) | Original file |
|---|---|---|
| 0x543b60 | `build_light_list` | `zModel\gmod_light.c` (its assert path) |
| 0x543d90 | `lights_reaching` | after gmod_light.c, before gmod_puff.cpp (same file, likely) |
| 0x545720 | `shade_polygon` | same |
| 0x53daa0 | `model_needs` | between gmod_const.c and gmod_cookie.cpp (beside 0x53f2e0) |
| 0x543910..0x543ac0 | `clouds_*` | between gmod_cookie.cpp and gmod_light.c |
| 0x4fb0c0, 0x4fb310 | `light_world_pose`, `light_update` | `zClass\Light.c` |
| 0x4f2f30, 0x4f2f60 | `world_build_lights`, `world_update_lights` | `zClass\cls_world.c` |
| 0x4020d0, 0x51e110 | local `normalize`, `scale_inverse` | maths helpers (start..briefing.cpp; the 0x51b170 maths module) |

The Light.c and cls_world.c functions sit in gmod_light for now; move them
when those files grow. 0x544ed0 (the baked static-light pass, below) and the
software renderer's own lighting (0x544060, from 0x53eac2) are **not ported**.

## State

| Address | Port | Meaning |
|---|---|---|
| 0x74980c / 0x749810 | `LightList::nodes/data` | the world's light node and data arrays |
| 0x749814 | `any` | count > 0 |
| 0x749818 / 0x74981c | `in_count / count` | lights offered / active ones listed |
| 0x749820 | `directed` | index of the last directed light, -1 (0x543d90 also writes it in software) |
| 0x749824 | `list[128]` | 0x14 each: +0 data, +4 node, +8 full, +0xc reach, +0x10 unused |
| 0x7497c8..0x749807 | `sw_*` | software renderer colours, set only without Direct3D |
| 0x74a2cc -> 0x74a2c8 | `Lighting::mask` | bit 0: lighting on (pushed per subtree) |
| 0x74a2d4 | `single` | 0 on Direct3D, 1 in software (0x526800; 0x5269e0 sets it in software only) |
| 0x74a270 / 0x74a274 / 0x74a280 | `tint_on / tint / tint_weight` | object3d colour push 0x526a10 (weight > 1/255 turns it on) |
| 0x73fbb0 | `camera_offset` | written by the camera matrices 0x4f4370 (the camera's last move; **[unverified]** meaning) |
| 0x7476a4 / 0x747ca4 / 0x747ca8 / 0x7481a8 | `cam / vertex_normals / normals / rec` | the draw's per-polygon buffers |
| 0x5ef860..0x5ef880 | `Clouds` | on, 2pi/period, 2pi/wavelength, base, amplitude, sin/cos heading, phases u, v |

## Light data (class 9, 0xd0 bytes) as the renderer reads it

| Offset | Meaning |
|---|---|
| +0x00 / +0x0c | orientation (radians) / translation, the node matrix euler(+0, +0xc) |
| +0x18 | pitch, yaw, 0 toward the light's -z (directed and spot lights) |
| +0x24 | world position (node matrix applied to the origin) |
| +0x60 | copy of +0x24 (flag 0x10) |
| +0x78 | camera-space position (flag 0x10) |
| +0x84 | world direction the light shines: the node matrix's -z row |
| +0x90 | camera-space unit vector toward the light: -(+0x84) rotated by the view |
| +0x9c / +0xa0 | diffuse / ambient |
| +0xa4 | colour rgb 0..1 |
| +0xb0 | flags (below) |
| +0xb4 / +0xb8 / +0xbc / +0xc0 / +0xc4 | near, far, near^2, far^2, 1/(far - near) |

Flags +0xb0, by what the code does with them (mech3ax reuses the
light-state *event* names for this word, which do not match):

| Bit | Use |
|---|---|
| 0x1 | recalc: cleared by 0x4fb310 |
| 0x2 | ranged: 0x543d90 tests the bounding sphere against the range |
| 0x4 | spot: N.L factor for point lights; angles and direction computed |
| 0x8 | directed (the sun) |
| 0x10 | positioned: camera-space position kept |
| 0x20 | saturated: goes to the fog channel (+8) instead of the point term (+4) |
| 0x40 | skipped on polygons drawn with projected shadows |
| 0x100 | static: skipped when the polygon has baked light (0x544ed0's cache) |
| 0x200 | needed with 0x2 for the light to reach anything |

The sun (0x38b) is recalc, ranged, directed, static, 0x80 (unused here) and 0x200.

## Per frame

`world_update_lights` (0x4f2f60, from the three world renders 0x4f6e88..)
runs `light_update` (0x4fb310) on every world light: inactive nodes (flag 4
clear) are left alone; `light_world_pose` (0x4fb0c0) builds the node-to-world
matrix (0x4ed280 through `LightHooks::node_to_world`; the default handles a
light under a world, which is every shipped light) and sets +0x24, +0x18,
+0x84; then +0x90 and +0x78 against the view. `world_build_lights`
(0x4f2f30, EDX 1 for no lights) lists the active nodes (node flag 4) in
order, at most 128 (a report at the 129th; the rest are dropped).

## Per model: 0x53daa0 and 0x543d90

`model_needs(flags, centre, radius)`: centre into camera space by the stack
top; bit 1 fog (fog on, flag 2, `fog_sphere` > 1/255); bit 2 lights (mask
bit 0, any light, flag 1, `lights_reaching` > 0); bit 4 tint (counts as one
more source). With two or more sources it mixes a colour, sum of colour x
weight over lights plus the tint's, over the summed weights, and hands it to
`mix_fog` (0x5680c0, the fog colour setter that clamps and sends it).

`lights_reaching(centre, radius)` per listed light:
- Direct3D and directed, or not ranged (0x2 clear): full and reach.
- Else 0x200 clear: neither. Else distance d (the centre's z for a directed
  light in software, else |position - centre| by the approximate square
  root): reach when `d - r < far` (directed always), full when `d + r < near`.
- Then each reaching light's intensity, 1 when full, else the falloff of
  `d - r`, capped by diffuse + ambient and clamped 0..1, goes to `rec[i][0]`
  (directed) or `rec[i][1]` **indexed by the light**, not a vertex: the work
  records double as per-light scratch until the polygon pass overwrites them.

## Per polygon: 0x545720 (`shade_polygon`)

Vertices are walked last to first; `rec[i]` +0 darkness, +4 point light, +8
fog/saturation. +0 and +4 are cleared, +8 only when the polygon is not fogged.

1. **Baked light** (ECX non-null: polygon +0x18, filled by 0x544ed0): in
   file order against the reversed vertices, `darkness = 1 - cloud * baked`
   (`1 - baked` for models with flag 0x20).
2. **Directed light** (flag 8): `I = min(1, diffuse * max(0, N.L) +
   ambient)` (no clamp on the ambient-only branch), N the polygon normal
   (normalised in place once per call) or, with vertex normals, each
   vertex's. Then `darkness += 1 - cloud * I`. Models with flag 0x20 take
   `I = min(1, ambient)`: **no diffuse term and no clouds** (correction:
   render.md has 0x20 opting out of the clouds only).
3. **Point lights**: spot factor (flag 4) from N.L, falloff by range, added
   to the per-vertex point term or (saturated) the fog channel. Saturated
   lights are skipped on fogged polygons.
4. **Tint**: `+8 += weight` on every vertex.
5. Clamp darkness, point and saturation to 0..1; the point term goes into
   +4; flags: vary bit 1 when any darkness > 1/255 or the point term differs
   across the polygon; bit 2 when the saturation differs. Saturated or
   tinted: draw flags `|= 9` and the device fog colour becomes the last
   contributing point light's colour (else the tint's), or, when fogged, that
   colour goes to the point-light slot (0x5798a0) and the mixed fog colour is
   sent (0x579980). A contributing point light: draw flags `|= 4` and its
   colour to the point-light slot.

## Entry points for the model draw (gmod_draw)

```cpp
// gamez/zmodel/gmod_light.h, namespace zmod
uint32_t model_needs(uint32_t model_flags, const float centre[3], float radius);
    // 0x53daa0: pass model +0x08, +0x48, +0x54; returns local_68's bits
int32_t shade_polygon(const float* baked, float normal[3], int32_t count, uint32_t* draw_flags,
                      uint32_t* vary_flags, int32_t shadowed, uint32_t needs);
    // 0x545720
int32_t lights_reaching(const float centre[3], float radius);  // 0x543d90 (model_needs calls it)
Lighting& lighting();      // set cam, vertex_normals, normals, rec (the draw's buffers),
                           // mask, tint_on/tint/tint_weight, single, camera_offset
LightHooks& light_hooks(); // point_color (0x5798a0), send_mix_fog (0x579980),
                           // mix_fog (0x5680c0), node_to_world (0x4ed280)
Clouds& clouds();
int32_t world_update_lights(zcls::Node* world);             // 0x4f2f60
int32_t world_build_lights(zcls::Node* world, int32_t none);  // 0x4f2f30
```

**Gap in the program (2026-10-25, found in item 4; item 4b ports it):** the
program never sets `light_hooks()` (only the probes do), so the lighting's
output to the device is dropped: `mix_fog` (0x5680c0: the mixed colour
clamped, kept when it moves 0.01, copied for Direct3D by 0x579950 into
[0x802798] x 255, the software ramp 0x56c170), `send_mix_fog` (0x579980:
[0x802798] against the sent colour [0x72f400], then the device's
[0x72f3f0]) and `point_color` (0x5798a0). Whatever the lighting mixes into
the fog channel for a model (the saturated point lights, the object3d
colour blend) never reaches the device; zrndr_fog's world fog colour does.

How 0x53f2e0 calls `shade_polygon` (0x53ff35 textured, 0x5413f6 untextured;
only when needs & 6):
- `baked` = polygon +0x18 when the draw's baked switch `[ebp-0x80]` is on
  and polygon flag 0x100 is clear, else null. The switch is set at
  0x53fb83..: model type 0, model +0x0c < 2 **[unverified meaning]**, needs
  bit 2, and the node's light cache valid (+0xc0 >= [0x5e9e2c]) or rebuilt
  now by 0x544ed0 (when +0xc4 <= frame [0x72f338]) and stamped (0x4ed980).
- `normal` = the cull's normal (`[ebp-0x24]`, negated for a back face).
- `draw_flags` = `&local_8` (bit 1 per-vertex fog already), `vary_flags` =
  `&local_c` (bit 2 fog varies; the draw ORs 4 for model flag 0x20 before
  and clears it after).
- `shadowed` = `[ebp-0x54]` (projected shadows on this polygon) at 0x53ff35,
  0 at 0x5413f6; `needs` = `local_68`.
- A non-zero result makes the draw OR 2 into local_8.

## Quirks

- **Clouds are flat in the Direct3D renderer** (player-facing). The pattern
  takes its table index with the 1.5 * 2^52 rounding trick, which needs
  double precision; at 24-bit (Direct3D 6 sets it) the sum keeps no low bits,
  so the index is always 0, the table gives 0 and `cloud = base` everywhere:
  the sun is a constant 70% (c1 base 0.7) and the shadows never move.
  `explore/lighting_clouds_x87.py`: at 64 or 53 bits 0x543ac0 gives ~885
  distinct values (0.40..1.0) over 2,000 points, at 24 bits one (0.7). The
  port keeps the 24-bit behaviour (float arithmetic). 0x4814e0 (mech3,
  between mech_waynet.cpp and mission.cpp) samples the same pattern and
  scales two fields at +0x9c/+0xa0 of some object by it **[unverified: likely
  a light's diffuse and ambient]**, so it is flat there too. The software
  renderer (0x544060) probably runs at the default 53 bits and would show
  them **[unverified]**.
- **Point lights go dark just inside their near range** (player-facing,
  when a model is only partly within it). Below near^2 the squared distance
  is kept and then compared with the near *distance*: from sqrt(near) to
  near the falloff `(far - d^2) / (far - near)` goes negative, and the
  negative value is summed with the other lights before the clamp. With a new
  light's defaults (near 32, far 64) a vertex at 6 gets 0.875, at 10..31 gets
  0, at 32 gets 1 (`explore/lighting_near_quirk.py`). Which shipped lights
  reach this depends on the anim light ranges **[unverified]**.
- The spot factor at polygon level (flag 4) dots the light direction with a
  vector that is not set yet: the stack slot still holds the previous
  light's last vertex vector, or leftovers from earlier calls. The port keeps
  it in `Lighting::stale`, carried across calls.
- For a model wholly inside a spot light's near range the per-vertex factor
  uses the unnormalised light-to-vertex vector.
- Vary bit 1 is set whenever any vertex has darkness above 1/255, whether
  or not it varies across the polygon; only the point-light term gets a real
  "differs" test. Under the flat 24-bit clouds every sunlit vertex has
  darkness of at least 1 - base, so in practice every sunlit polygon sets it
  (what the draw does with the bit: gmod_draw).
- 0x53daa0's colour mix reads +4 of each light's record, also for directed
  lights (whose intensity went to +0) and for point lights that did not
  reach (stale values).
- In Direct3D the sun is always full and reaching: its range is never
  tested, and the software-only `single` switch is off.
- No bound checks: more than 64 vertices overrun 0x545720's stack array;
  the work records' per-light use assumes at most 64 lights before the
  shadow sets.
- The 1/255 tests compare against a double constant (0x5ce4a0), not a float.

## Checked (test_lighting.py, 24-bit)

72 frames of world lights (six worlds, original reader 0x4ff520, random
views, Direct3D and software, "no lights"), 400 light sets (inactive nodes,
140 lights for the 128 cap), 1,500 models through 0x543d90 and 0x53daa0
(exact range boundaries included), 4,500 random polygons through 0x545720
(3,000+ lit; every hook, flag and path hit) and 1,200 polygons under each
world's real sun with the shipped diffuse/ambient and fluctuate values, plus
400 cloud setter/sampler calls: identical within 1e-6. Breaks the test
catches: the near^2 quirk "fixed", baked light in vertex order, `<=` for the
full test, the mix reading the directed slot, +0x84 sign, the cloud index
rounded properly, the stale vector cleared, the point-light vary test
skipped. Not emulator-checked: `clouds_advance` (0x543a50 calls the CRT's
`_CIfmod`).

## Open

- 0x544ed0 (bake static lights into polygon +0x18) is needed by the draw for
  the sun on cached models; the software lighting 0x544060.
- What model +0x0c < 2 means for the bake switch; 0x73fbb0's meaning.
- Shipped point-light ranges (anim light-state events), for how often the
  near-range dark ring shows.

## For docs/discoveries.md

- Under Direct3D the "fluctuate" cloud shadows never appear: the maths that
  picks the cloud pattern needs more precision than Direct3D leaves the CPU,
  so every point gets the same value and the sun is just dimmed (to 70% on
  the first mission).
- Coloured point lights (explosions, flares) light a model fully when very
  close, then not at all for a stretch, then fade normally: a squared
  distance compared as a plain distance.
