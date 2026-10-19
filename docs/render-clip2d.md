# Screen-space side clipping (0x529ce0 and family)

Mech3.exe 1.2, 2026-10-19. Draft for `docs/render.md` section 3 item 6.
Port: `src/gamez/zmodel/gmod_clip2d.{h,cpp}`; check:
`tests/differential/test_clip2d.py` (all six functions, every output byte
identical to the original); edge cases: `tests/differential/explore/clip2d_edges.py`.
Original file unknown (between `gmod_init.c` and `gmod_matl.c`).

## The six functions

All are `int __fastcall clip(ScreenClip* ECX, int* EDX)`: ECX is the clip
struct, EDX points at the vertex count (the caller's `[ebp-4]`). They clip
the projected polygon in the TL buffer in place, rewrite the count, and
return 0 when fewer than 3 vertices are left (the caller then skips the
polygon), else 1.

| Address | Port | Caller | Carries, besides sx, sy, sz |
|---|---|---|---|
| 0x529ce0 | `clip2d_flat` | 0x5415dc (untextured, no fog/light), 0x541692 | nothing |
| 0x52aaf0 | `clip2d_lit` | 0x5414d0 (untextured, fogged or lit) | work +0..+8 (darkness, point light, fog) |
| 0x52bd10 | `clip2d_textured` | 0x540a0b, 0x541301 | tu, tv; work +0xc..+0x14 (vertex colour) |
| 0x52dc80 | `clip2d_textured_lit` | 0x5409e9 | tu, tv; work +0..+0x14 |
| 0x52e920 | `clip2d_textured_shadowed` | 0x540a04, 0x5412fa | as 0x52bd10, plus the shadow UV sets |
| 0x52fab0 | `clip2d_textured_lit_shadowed` | 0x5409e2 | as 0x52dc80, plus the shadow UV sets |

The shadowed pair is chosen when `[0x7489a8]` (shadow set count) is non-zero;
the `_lit` textured ones when the caller's `edi` at 0x5409cd is set
(**[unverified]**: per-vertex light/fog, going by what they carry). rhw,
diffuse and specular (+0xc..+0x14 of the TL vertex) are never read or
written: the draw functions fill them after the clip, so a clipped slot
keeps whatever stale values it had.

## Memory

| Address | What | Size |
|---|---|---|
| 0x72f41c | TL buffer, D3DTLVERTEX (sx, sy, sz, rhw, diffuse, specular, tu, tv) | 0x20 per vertex |
| 0x7481a8 | work records (render.md section 5) | 64 x 0x20, ends at 0x7489a8 |
| 0x7489a8 | shadow set count (int) | |
| 0x7489c8 | shadow UV sets: set k at + k*0x200, (u, v) per vertex | 7 x 64 x 8 |
| 0x63d890 | TL partner buffer (same layout; only sx, sy, sz, tu, tv used) | 0xd00 = 104 slots |
| 0x63e590 | shadow UV partner sets | 7 x 0x200 |
| 0x63f390 | work partner buffer | 0x20 per vertex |

The clip struct (ECX): the model draw passes the address 0x7402f8 (not a
pointer loaded from it: `mov ecx, 0x7402f8`), or 0x740338 for a second pass
(below). Read: +4 left, +8 top, +0x1c right, +0x20 bottom, all floats. +0 is
a side mask (0x740338's is set to 0xf by 0x527070) that these functions do
not read. 0x526e10 fills 0x7402f8 from the integer viewport: +4/+8 the
origin, +0x1c/+0x20 origin plus size **[unverified: read from the asm only]**.

## Algorithm

1. **Outcodes from the vertices.** For every vertex (last to first):
   x < left sets 1, else x >= right sets 2; y < top sets 4, else y >= bottom
   sets 8. The cull's plane mask (bits 0xf of `local_78`) only decides
   whether the clipper is called (`test bl, 0xf` before each call); the
   clipper recomputes its own. No bit set: return 1, nothing written.
2. **One Sutherland-Hodgman pass per set bit**, in the fixed order left,
   right, top, bottom. Inside is `x >= left`, `x < right`, `y >= top`,
   `y < bottom` (right and bottom are exclusive). Each pass walks the edges
   prev -> cur with prev starting at the last vertex; when inside(prev) !=
   inside(cur) it emits the crossing point, then cur if cur is inside.
   Passes ping-pong between the main buffers and the partner buffers.
3. After each pass the count is written back; below 3 the function returns
   0 at once (0x529ce0 skips that check after the bottom pass, but the final
   `count < 3` check gives the same result).
4. If an odd number of passes ran, the result is copied back from the
   partner buffers (last to first), only the fields the function carries.

**Crossing point** with `t = (edge - p) / (c - p)` on the clipped axis:
the clipped coordinate is set to the edge exactly, the other and sz are
`(c - p) * t + p`. Every other value is interpolated
**perspective-correctly**, using sz (= near / z, render.md section 0) as the
weight: `w = 1 / sz_new`, `a = t * w * c.sz`, `b = (w - t * w) * p.sz`,
`value = p.value * b + c.value * a`. That covers tu, tv, the work record
fields and the shadow UVs alike (so the light, fog and colour too, not just
the texture).

## Quirks

- **A vertex exactly on the right or bottom edge comes out twice.** It counts
  as outside (exclusive edge), so the edge into it emits a crossing at t = 1
  and the edge out of it one at t = 0, both equal to the vertex. A triangle
  (500,10) (640,50) (500,100) in a 0..640 rect leaves with 4 vertices, two of
  them (640,50). A vertex on the left or top edge is kept once. Harmless for
  drawing (a zero-length fan edge) but it costs a vertex slot.
- **Clipping can rotate the vertex order** even when nothing is cut: a
  full-screen quad (0,0) (640,0) (640,480) (0,480) comes out starting at
  (0,480). The fan centre moves, which matters only for flat shading with
  per-vertex colours (not used: render.md, Colour).
- **NaN coordinates** (x87 FCOMP sets C0 on unordered) count as outside the
  left/top sides and inside the right/bottom ones.
- **No bounds checks.** The work buffer has 64 slots and each pass can add
  one vertex; the shadowed pair keep the set pointers in two 7-entry stack
  arrays, so a shadow count above 7 would overrun the stack. Neither is
  reached by shipped data as far as known **[unverified]**. A count of 0
  would make the outcode loop run off the buffer (it is a do-while); callers
  never pass it. The port's loop does nothing then.
- **Precision decides vertex counts.** Direct3D 6 switches the FPU to 24-bit
  precision when the device is created unless the game asks
  DDSCL_FPUPRESERVE, and Mech3 does not (SetCooperativeLevel flags 0x13 at
  0x577c4b; 0x8 at 0x57a655 **[unverified: likely the same call]**). With
  the emulator at 24 bits, the port's plain float steps give identical bits
  on every case. At the x87's power-on
  64-bit precision 2-4% of the random cases (mostly vertices on or near
  an edge) end with a **different vertex count**, and values typically
  drift up to ~6e-3 relative (more in near-degenerate polygons). So the port relies on float arithmetic step by step, and
  `gmod_clip2d.cpp` turns off FP contraction (`#pragma STDC FP_CONTRACT
  OFF`): with clang's default fused multiply-adds on arm64, about 60% of the
  cases differ.

## Corrections to docs/render.md

- Section 3 item 6: the rect is the struct **at** 0x7402f8 (`mov ecx,
  0x7402f8`), not `[0x7402f8]`; the side bits are recomputed from the
  vertices by each clipper, the plane mask only gates the call; right and
  bottom are exclusive. The clip is perspective-correct for UVs, colours,
  light and fog.
- Two more shadowed variants exist besides the textured one: 0x52e920 goes
  with 0x52bd10, 0x52fab0 with 0x52dc80.
- Section 5, the work record: the buffer holds 64 records (0x7481a8 ..
  0x7489a8); the shadow set count and UVs follow it at 0x7489a8 / 0x7489c8.
- 0x529790, 0x52a250 and 0x52c8d0 (same region) are called from the
  software renderer's model draw (0x53ebb3..0x53f19e), not the Direct3D one.

## Neighbours and leads

- **Second clip rect.** At 0x541666 the untextured flat path, after drawing,
  checks `[0x74a32c]`; when set it runs 0x535d20 (ECX 0x740338, EDX count:
  a trivial reject, 0 when every vertex is outside one side its +0 mask
  selects) and then 0x529ce0 against 0x740338. 0x527070 fills that rect.
  What it is for (a second viewport, a HUD cut-out?) is open.
- **0x52b450** sits between 0x52aaf0 and 0x52bd10: another side clipper, on
  a 12-byte-per-vertex buffer (0x7479a0..). Nothing references it (no call,
  no pointer in the image): dead code; not ported.
- Ghidra's `FUN_00530043` ("created from a data pointer") is inside 0x52fab0,
  a false function start.

## Test coverage (test_clip2d.py)

4,000 random cases per function by default (seeded): rects normal, empty or
inverted, and huge (up to 1e30); polygons of 3..20 vertices inside, wholly
outside one side, straddling, convex around a random point or a corner,
snapped onto the edges, degenerate (one point, collinear, repeated) and
huge (up to 1e30); sz down to 1e-6, sometimes 0 or negative; shadow count
0..7 for every function (the unshadowed ones must leave the sets alone).
All other buffer bytes are random, and all six buffers are compared whole.

Branches never reached: in each function, the `count <= 0` skip at the start
of each pass loop (0x529e57, 0x529f7f, 0x52a0ac, 0x52a1e3; 0x52ad69,
0x52af76, 0x52b19e, 0x52b3be; 0x52c038, 0x52c2cd, 0x52c56a, 0x52c807;
0x52dfdc, 0x52e2a5, 0x52e576, 0x52e83b; 0x52ed6a, 0x52f17e, 0x52f596;
0x52ff5b, 0x5303b5, 0x530810, 0x530c59). They are unreachable: the first
pass needs a count of at least 1 to get an outcode, and every later pass
starts with at least 3.

Deliberate breaks, each caught (cases differing out of 300 per function):
an on-edge right vertex counted as inside (10-13), attributes interpolated
linearly instead of perspective-correctly (all but flat, ~2/3), the tu/tv
copy-back dropped (textured ones, 33-50), the first edge starting at vertex
0 (~1/2), top clipped before right (~1/2), no early drop below 3 vertices
(~1/3), crossing points never emitted (~2/3), the other coordinate
interpolated from the wrong end (~2/3), shadow sets not copied back (24-47),
shadow weights swapped or one set fewer (shadowed ones, ~1/2).
