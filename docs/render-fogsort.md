# Fog maths and the transparent queue

Ported: `src/gamez/zmodel/gmod_fogcalc.{h,cpp}` (0x555640, 0x555a70) and
`src/gamez/zvideo/zvid_ddd3d.{h,cpp}` (the queue's reserve block, the sort
0x57dec0, the flush 0x57e110). Checked by `tests/differential/test_fogsort.py`
against the original in Unicorn (probes in `tests/probe_fogsort.cpp`).

## Fog per model: 0x555640

`ECX` = the bounding sphere's camera-space centre, stack = its radius;
returns the fog in `st0`. Called by the model draw 0x53daa0 (0x53db83) only
when fog is on (`[0x74a22c]`) and the model has flag 2; the model is fogged
when the result is above 1/255 (0x5ce1f8).

It is the **most fog any point of the sphere can get**, a gate, not a value
used for drawing:

```
s      = approx_sqrt(x*x + z*z)       centre, camera-space x and z
far_pt = s + r                        farthest horizontal point
if far_pt <= near: 0                  whole sphere inside the clear zone
y      = world height of the centre;  bottom = y - r
if bottom >= alt_high: 0              whole sphere above the fog
if s - r < far:                       nearest point inside the range
    f = (min(far_pt, far) - near) / (far - near)
    if y + r > alt_low: f *= (alt_high - max(bottom, alt_low)) / (alt_high - alt_low)
else:                                 whole sphere beyond far
    if y + r <= alt_low: return 1     whole sphere under the band
    f = (alt_high - max(bottom, alt_low)) / (alt_high - alt_low)
clamp f to 0..1
```

## Fog per vertex: 0x555a70

`ECX` = vertex count; vertices from the pointer `[0x74639c]` (camera space,
stride 12), factors into `0x7463a4[]`, walked **last to first**. Formula as in
render.md section 5 (it is right). Returns 1 when any factor is above 1/255;
the caller (0x53f984) then keeps vertex fog for the model, else clears its
flag bit 1. On 1 it also pushes the fog colour: `[0x72f3f4..] = colour * 255`
and, when that differs from `[0x72f400..]`, copies it and calls
`[0x72f3f0]` (the device's FOGCOLOR). **This push skips the device-up test
`[0x72f348]`** that the setter path (`zrndr::device_fog_color`) has; it
happens only on frames where something is fogged.

## Corrections and additions to docs/render.md

- Neither function reads fog on, linear or density (0x74a22c, 0x74a230,
  0x74a260): there are no fog modes in this path. "Linear" vs anything else
  changes nothing here; fog off is only the caller's test of `[0x74a22c]`.
- The model-level test (0x555640) is not the vertex formula with the radius
  added: it takes the farthest distance and the lowest point, so it never
  under-estimates (see above).
- The FOGCOLOR push from 0x555a70 has no device-up test (above).
- Camera-to-world matrix: 4x3 at 0x73fc00, rows x, y, z, translation; only
  column 1 is read (0x73fc04, 0x73fc10, 0x73fc1c, 0x73fc28).
- Transparent record (0x80cd9c, 0x10 bytes): `{primitive, count, texture
  record or 0, vertex pointer into the 24000-vertex pool at 0x82449c}`;
  the sort key is `[record+0xc]+8`, the first TL vertex's sz.
- ALPHAOP in the flush: the value compared with the cache `[0x809390]` is 4
  when the first vertex's colour is above 0xff000000 (opaque and not pure
  black), otherwise the texture type; only 2 would send SELECTARG1, and no
  type is 2, so the flush always sends MODULATE (whenever that value changes).
  The opaque textured draw 0x57ce20 leaves the cache at 2, so after one the
  first textured polygon of the flush re-sends it.
- SetTexture is sent for every queued polygon (null when untextured), not
  cached. SHADEMODE is forced to Gouraud for the whole queue, also for
  polygons queued by the flat routines (0x57bf80, 0x57c2f0, 0x57dba0); all
  their vertices have one colour, so it looks the same. After the queue:
  blending off, Z writes on, SHADEMODE left at 2, both counts reset.
- A failing DrawPrimitive is reported (0x580550, line 0xb6f) and the flush
  carries on.

## The queue (the reserve block)

Inlined in every transparent draw (0x57bf80, 0x57c2f0, 0x57caa0, 0x57ce20,
0x57d160, 0x57d690, 0x57dba0; same source lines 0x591/0x599, so an inline
function in the original). Polygon check first (`count >= 6000`: "Not enough
MAX_TRANSPARENT_POLYS: need count+1"), then vertices (`used + n > 24000`:
"... VERTICES: need used+n"), both zerr level 0x400; the polygon is then
dropped. The caller fills the vertices. Seen in 0x57bf80/0x57c2f0 (flat,
untextured): **rhw is a copy of sz** and specular is 0xff000000 (no fog).

## The sort: 0x57dec0

Index list `0x803558[count]` (the flush fills it reversed, `count-1 .. 0`).
Key = the first vertex's sz bits as a **signed int**, ascending (far to near
for positive sz; negative floats would order backwards among themselves and
before all positives; sz is never negative in play).

- Quicksort with an explicit stack (in 0x4b4 bytes of locals) while a run has
  more than 9 elements (`hi > lo + 8`); median of three: compare-swap
  (lo, mid), (lo, hi), (mid, hi), `mid = (lo + hi) >> 1`; pivot = key of mid;
  Hoare scan from lo+1 / hi-1 with strict `<` / `>`, swap and step on equal
  keys. The larger side is pushed, the smaller continued.
- Then one insertion sort over the whole list (strict `>`; equal keys stay).
- Not stable: equal keys come out in an order set by the partitions; the test
  compares the full permutation with duplicates, all-equal and few-value
  arrays.

## Test (test_fogsort.py)

- Fog: 1,500 random settings (shipped hwrange/altitude pairs, lava, defaults,
  the world-file dummy, random ones incl. far < near, inverted band, stale
  inverses), random camera matrices, 12,000+ spheres and 270,000 vertices over
  every branch, exact boundaries (d = near, d = far, y = high/low), NaN, and
  heights that hit the band edges only after rounding. Factors within 1e-6,
  return values, device colour and sends exact.
- Sort: sizes 0..6000, random/sorted/reversed/few values/all equal/negative
  floats/organ pipe/int extremes, reversed and shuffled starting order:
  identical permutations (888 runs).
- Queue: 0x57dba0 and 0x57bf80 from empty and next to both limits.
- Flush: 400 random queues with textures, vertex colours, cached states and
  failing draws; the device call log, final order and states identical.

Precision: the x87 keeps `x*x + z*z`, the world height and the fog products
unrounded until stored, and compares the unrounded height with `alt_high` and
derives `bottom = y - r` from it. The port does these in double (one sphere
case was 1e-6 off with float). No FIX needed.

Break tests (each caught): insertion sort moving equal keys, quicksort cutoff
8 -> 7, flush order not reversed, vertex limit off by one, ALPHAOP sending the
type, sphere altitude from the centre instead of the bottom, sphere bottom
from the rounded height, `<` instead of `<=` on near, visible threshold 0
instead of 1/255 (caught by the faint-fog case).

Not observable, so not caught: which side is pushed on a tie (each run is
sorted independently, the result is the same), and comparing the rounded
instead of the unrounded height with `alt_high` (both give factor 0 there).

Not run: the textured queue sites (0x57ce20 etc., colour conversions with
`_ftol`) — their reserve block is the same; the overlay list (0x8dfc9c,
`ZVID_MAX_OVERWRITE_POLYS`) the same routines fill when their last argument is
set; 0x5557f0 (a third fog routine over the 0x7476a4 buffer, scaling
`0x7481ac`), next to these.
