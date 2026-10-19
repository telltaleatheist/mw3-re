# Camera-space near/far clipping

Mech3.exe 1.2, 2026-10-19. The six clippers the Direct3D model draw
(0x53f2e0) runs on a polygon before projection, when the cull left plane
bits 0x30 (docs/render.md section 3, item 6). Ported in
`src/gamez/zmodel/gmod_clip3d.{h,cpp}`; checked by
`tests/differential/test_clip3d.py` (Unicorn against the port, below).
Original file name unknown (between `gmod_init.c` and `gmod_matl.c`).

## Who calls what

All six are fastcall with **ECX = the clip struct 0x7402f8** and **EDX =
&vertex count** (a local of 0x53f2e0); no stack arguments. Each returns 0
(drop the polygon) or 1 (draw it), and rewrites the count and the buffers
in place when the near plane cuts. The caller only calls them when its own
copy of the plane mask has a bit of 0x30; the clipper then reads the mask
again from the struct's +0.

| Clipper | Draw path (0x53f2e0) | Carries along |
|---|---|---|
| 0x5275e0 `clip3d_flat` | untextured, unlit (0x541570; then 0x529ce0, [0x72f3b0]) | position |
| 0x527a30 `clip3d_lit` | untextured, lit or fogged (0x541464; then 0x52aaf0, [0x72f3b8]) | position, work record +0..+8 |
| 0x527fd0 `clip3d_textured` | textured, uniform light/fog (0x540928, 0x541285) | position, record +0xc..+0x14, UV |
| 0x5286e0 `clip3d_textured_lit` | textured, per-vertex light/fog (0x540906; [0x72f3c8]) | position, record +0..+0x14, UV |
| 0x528bb0 `clip3d_textured_shadow` | 0x527fd0 with projected shadows ([0x7489a8] > 0) | as 0x527fd0 + shadow UV sets |
| 0x529120 `clip3d_textured_lit_shadow` | 0x5286e0 with projected shadows | as 0x5286e0 + shadow UV sets |

The per-vertex-light choice is the 0x53f2e0 local `[ebp-0xc]`. After
0x527fd0 (uniform light) the caller copies vertex 0's record +4 and +8 into
the slots from the old count up to the new one (0x540946..0x54096e), so the
vertices the clip added get the polygon's uniform values.

Corrections to docs/render.md section 3 item 6: 0x527a30 is the untextured
*lit/fogged* path (not "coloured"), 0x5286e0 the textured *per-vertex lit*
path (not "per-vertex colour"); the plane mask comes from `[0x7402f8]`, EDX is
the count pointer. There are three more siblings, used only by the software
model draw 0x53e050: 0x5277d0 (carries record +4 only, never interpolated:
the cut takes the previous vertex's), 0x527d20 (position + the UV buffer at
0x747fa8, 8 bytes a vertex), 0x5283c0 (called at 0x53eb24) **[not ported,
not checked]**.

## Globals

| Address | What | Size |
|---|---|---|
| 0x7402f8 | clip struct: +0 plane mask (0x10 near, 0x20 far, 0xf sides), +0xc near z (0x740304), +0x18 far z (0x740310); +4/+8/+0x1c/+0x20 the screen rect (clip2d) | 0x24 |
| 0x7476a4 | camera-space vertices, 12 bytes | 64 |
| 0x7481a8 | per-vertex work record, 0x20 bytes (+0 darkness, +4 point light, +8 fog, +0xc..+0x14 colour) | 64, ends at 0x7489a8 |
| 0x72f41c | D3DTLVERTEX buffer; only tu, tv (+0x18, +0x1c) are touched here | 64 |
| 0x7489a8 | shadow set count (0..7: the caller stops adding at 7, 0x5408bc) | |
| 0x7489c8 | shadow UVs: set s, vertex i at `+0x200*s + 8*i` | 7 x 64 |
| 0x63e290 | scratch: positions | 64 |
| 0x63f390 | scratch: work records | 64 |
| 0x63d890 | scratch: TL vertices (only +0x18/+0x1c written) | 64 (the screen clippers use it too) |
| 0x63e590 | scratch: shadow UVs, same layout as 0x7489c8 | 7 x 64 |

The 64s come from the layout: 0x7476a4 + 64*12 is the next buffer, the
records end exactly at 0x7489a8, the shadow sets are 0x200 apart. Nothing
checks them; a polygon that clips to more than 64 vertices overruns the
scratch (into the next scratch buffer) in the original. The port keeps the
same capacities and does not guard either.

## Semantics (all six)

1. **Far** (mask 0x20): if *no* vertex has `z < far`, return 0. The far plane
   never cuts; a polygon straddling it is drawn whole, its far part past the
   far plane (its TL depth `near / z` stays inside 0..1 however far, so
   Direct3D should not cut it either **[unverified]**).
2. **Near** (mask 0x10): if no vertex has `z < near`, return `count >= 3`
   (untouched). Without bit 0x10 the function returns 1 whatever the count,
   even 0.
3. Otherwise one Sutherland-Hodgman pass, starting with the edge from the
   last vertex to vertex 0. A vertex is inside when `z >= near`. For each
   vertex i (prev = the one before, wrapping):
   - inside(i) != inside(prev): emit the cut point, `t = (near - z_prev) /
     (z_i - z_prev)`, each carried value `prev + (cur - prev) * t` (x87
     order), and z stored as the near value itself (a bit copy), so the
     projected `rhw = near / z` is exactly 1 there.
   - inside(i): emit vertex i unchanged.
4. Store the new count; fewer than 3: return 0 (the buffers are not written
   back). Otherwise copy the scratch back over the first `count` entries of
   each buffer it carries, return 1. Fields a variant does not carry keep
   their old per-slot values.

x87 comparisons are `fcomp; fnstsw; test ah, 1`: the branch is taken on
"below" or unordered. So a NaN z is **in front of** the far plane (it blocks
the far reject) but **behind** the near plane.

## Quirks

- **The point light is not interpolated** (0x527a30, 0x5286e0, 0x529120,
  and the software 0x5277d0): at a cut, record +4 is copied from the edge's
  previous vertex instead of interpolated, while darkness (+0) and fog (+8)
  are. A polygon crossing the near plane under a point light gets the light
  of whichever neighbour comes first in its (reversed) vertex order on the
  new edge. With near = 0.1 it is rarely visible.
- **On-plane vertices make duplicates**: a vertex with `z == near` is inside,
  and an edge from it to a vertex behind cuts at t = 0, emitting a second
  copy of it. Harmless zero-length edges in the fan.
- **Reject-only far plane** (above), and degenerate polygons (count < 3) are
  only dropped here when the near bit is set.

## Precision

The emulator check runs the original twice. **At 24-bit x87 precision**,
which is what the game runs at (Direct3D 6 sets the FPU to single precision
on device creation unless the cooperative level has DDSCL_FPUPRESERVE, and
0x577c51 passes 0x13 = EXCLUSIVE | ALLOWREBOOT | FULLSCREEN; the C runtime
sets 53-bit before that, `_controlfp(_PC_53, _MCW_PC)` at 0x5ac5a0), the port in plain `float` is
**bit-identical** except:

- NaNs differ in sign only: x87's default NaN is 0xffc00000, the host's
  0x7fc00000. Accepted as equal.
- Inputs so large or small (|v| > 1e18 or < 1e-18, inf, NaN) that an
  intermediate leaves the float range: the x87 keeps its 15-bit exponent in
  registers, `float` does not. Accepted (and counted) only in those cases.

At the power-on 64-bit precision, interpolated values differ in the low bits
(and by more after cancellation); that run gates only the return value and
count. The port avoids FMA contraction by computing `(cur - prev) * t` and
`+ prev` in two statements.

## The check

`tests/differential/test_clip3d.py [cases]` (default 3000 per clipper,
18,000 polygons, ~20 s): 0..20 vertices (mostly 3..20), all in front, all
behind, beyond far, straddling near / far / both, vertices exactly on
either plane, degenerate (all one point), huge values, inf and NaN; near
0.1 / 1 / 0 / -1 / random, far 800 / random / equal to near; random plane
masks with side bits and junk high bits; 0..7 shadow sets; every buffer and
scratch filled with the same random bytes on both sides. Compared: EAX, the
count, every byte of all eight buffers. Every instruction of the six
functions is executed (the test lists any it misses).

Deliberate breaks, each caught in the first few cases: interpolating the
point light; `>` instead of `>=` on the plane; NaN taken as inside
(`!(z < plane)`); far rejecting on any vertex beyond; keeping 1-2 vertex
polygons; lerp as `cur*t + prev*(1-t)` (last-bit differences); copying back
one vertex short; textured UVs lerped from the wrong end; the last shadow
set not copied back; shadow UVs not interpolated.

## For docs/discoveries.md

- When a polygon is cut by the camera's near plane, the new corner takes
  the point-light brightness of its neighbour instead of blending it (darkness
  and fog are blended). A one-line slip in the original: the copy of the
  previous vertex's value sits where the blend should be.
- The far clip plane never cuts polygons: anything with one corner inside the
  800 m draw distance is drawn whole, however far its other corners reach.
