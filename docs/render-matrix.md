# The matrix stack and camera maths

Mech3.exe 1.2, 2026-10-19. The maths the render path and the scene graph
share: a matrix stack, the camera's view matrix, the projection constants,
euler angles in and out, and the point transforms of the model draw.
Ported in `src/gamez/zmath/zmth_matrix.{h,cpp}`; checked by
`tests/differential/test_matrix.py` (Unicorn against the port, 10,100 cases,
below). Original file name unknown: an unnamed module at 0x51b170..0x526920
between `zinput\zin_init.cpp` and `zmodel\gmod_init.c`, likely the engine's
maths library. Conventions as in `docs/render.md` section 0 (row vectors,
3x4, `world = local * parent`); this draft confirms them and adds the
details below.

## State

| Address | Port (`zmth::`) | Meaning |
|---|---|---|
| 0x5eda90 | `MatrixStack::top` | pointer to the current slot (data initialiser 0x63d760) |
| 0x5eda8c | `MatrixStack::top` | pointer to the current identity flag (initialiser 0x63d7e8) |
| 0x63d760 | `slot[32]` | matrix buffers, **owned by the callers** (push takes a pointer) |
| 0x63d7e8 | `identity[32]` | 1: that slot holds the identity |
| 0x73fbc0 | `View::view` | world -> camera (x right, y down, z forward) |
| 0x73fc00 | `View::camera` | camera -> world with rows y and z negated (W' below) |
| 0x63d338/33c | `origin_x/y` | window left, top |
| 0x63d748/74c | `half_w/h` | half the window size |
| 0x63d750/754 | `cot_h/v` | cot(fov/2), from camera +0x1d4/+0x1d8 |
| 0x63d740/744 | `inv_cot_h/v` | tan(fov/2) |
| 0x63d758/75c | `inv_fx/fy` | 1/fx, 1/fy |
| 0x63d7e0/7e4 | `fov_h/v` | effective fov (camera +0xe8/+0xec), set by 0x521c10 |
| 0x73fb80 | `near_z` | near clip (camera +0xb0) |
| 0x73fb88/b8c | `cx/cy` | screen centre = origin + half size |
| 0x73fb90 | `inv_near` | 1/near on Direct3D, 1 in software |
| 0x73fb94 | `far_z` | far clip (camera +0xb4) |
| 0x73fb9c/ba0 | `fx/fy` | cot(fov/2) * half size |
| 0x73fc30/b98 | `fx_near/fy_near` | fx * inv_near, fy * inv_near |

The stack is 32 deep with no bound check. Slot 0 starts as a null pointer
(.bss): the first operation must be a push. The only reset, 0x51e8b0, is
unreferenced; the pointers start at slot 0 from the .data initialisers.
Overflow would land on `fov_h/fov_v` (slot 32 = 0x63d7e0) and the flag
array's neighbour 0x63d868 **[unverified: never reached in play]**.

## Functions

All fastcall: ECX, EDX, then the stack (popped by the callee).

| Address | Port | Args | What |
|---|---|---|---|
| 0x51e9d0 | `push` | ECX buffer | new slot = buffer, contents untouched, flag 0 |
| 0x51e930 | `push_copy` | ECX buffer | new slot = buffer, copies the old top's 12 floats and its flag |
| 0x51ea00 | `pop` | | |
| 0x51fd40 | `load` | ECX m | top = m, flag 0 |
| 0x51fd70 | `load_rotation` | ECX m | top 3x3 = m 3x3, translation kept, flag 0 (one caller, 0x4acd28) |
| 0x51fde0 | `load_identity` | | flag 1 (the only place a flag becomes 1, besides push_copy copying it) |
| 0x51ea30 / 0x51ea60 | `load_view` / `load_camera` | | top = 0x73fbc0 / 0x73fc00, flag 0 |
| 0x51fb30 | `load_model_view` | | top = view, then top = slot[top-1] * view: the node's world matrix times the view |
| 0x51fee0 | `multiply` | ECX local, EDX mode | top = local * top; mode 2: 3x3 only |
| 0x520330 | `scale` | stack x, y, z | top = S * top (rows scaled) |
| 0x520480 | `translate` | stack x, y, z | top = T * top (translation += (x,y,z) * 3x3) |
| 0x520610 / 0x5207b0 / 0x520960 | `rotate_x/y/z` | stack angle | top = R * top, R as in render.md section 0 |
| 0x521670 | `euler` | ECX rot, EDX pos, stack scale | top = (S * Rz * Rx * Ry, pos) * top |
| 0x523bc0 | `matrix_to_euler` | ECX m, EDX out | render.md section 0 formula; see quirks |
| 0x523c50 | `invert`, `invert_single` | ECX m, EDX out, stack flag | cofactor inverse, row 3 = -t * inverse when flag; returns 0 if det == 0. `invert` keeps the x87's 53-bit intermediates (the reference-runtime path, test_matrix.py); `invert_single` rounds each operation to a float, for play code checked at 24 bits (the shot queries' model test 0x54cad0, test_shotq.py) |
| 0x520b00 | `set_view` | ECX camera world matrix | W' = W with rows 1, 2 negated -> 0x73fc00; V = rigid inverse of W' -> 0x73fbc0 |
| 0x521c10 | `set_fov` | stack h, v | |
| 0x521c60 | `set_projection` | stack x, y, w/2, h/2, cot_h, cot_v, near, far | all the Projection fields; reads [0x72f348] (Direct3D) |
| 0x51e8d0 | `focal` | | returns (fx, fy) in EAX:EDX |
| 0x521db0 / 0x521de0 | `scale_by_cot` / `scale_by_tan` | ECX points, EDX count | x *= cot_h, y *= cot_v (or by tan) |
| 0x5234f0 | `project` | ECX cam, EDX screen, stack count | sx = x*fx*(1/z) + cx, sy likewise, sz = 1/z |
| 0x523540 | `project_d3d` | same | sx = x*(fx/near)*(near/z) + cx (= the same pixel), sz = near/z |
| 0x5235f0 / 0x523590 | `unproject` / `unproject_d3d` | ECX screen, EDX cam, stack count | exact inverses of the two (z = 1/sz or near/sz) |
| 0x5220e0 | `morph` | ECX dst, EDX base, stack delta, count, f | dst = base + delta * f |
| 0x5225b0 | `rotate_points` | ECX pts, EDX count | in place, 3x3 only |
| 0x522640 | `rotate_points_transposed` | ECX pts, EDX count | in place, v' = R v (the inverse rotation) |
| 0x5226e0 | `rotate_points_to` | ECX src, EDX dst, stack count | the model draw's normals |
| 0x5227a0 | `transform_points` | ECX pts, EDX count | in place, full 3x4 (the morph path of 0x53f2e0) |
| 0x522910 | `inverse_transform_points_to` | ECX src, EDX dst, stack count | v' = (v - t) R^T |
| inline 0x53f3a9 / 0x53f8fb | `transform_points_to` | | the model draw's vertices into camera space |

Every operation on the top clears its identity flag. Every point op
skips (or copies) when the top is flagged identity.

## The model draw's transform (render.md section 3, step 2)

0x53f2e0 pushes a local buffer (0x51e9d0) and calls **0x51fb30: top = world
* view**, the model-view matrix, so each vertex goes to camera space in one
step: `cam = v * (world * view)` into [0x74639c], inline (0x53f3a9 point
models, 0x53f8fb the others). A morphing model blends first (0x5220e0 into
[0x74639c], base +0x38, delta +0x44, morph count +0x1c, factor +0x24) and
transforms that buffer in place (0x5227a0, vertex count +0x14). Normals go
through 0x5226e0 (3x3 of the same top). Billboards (model type 1) build their
own top first (0x51ea90..0x51f750, not ported).

## Quirks

- **Mode 2 of 0x51fee0 copies stack garbage into the translation.** The
  3x3 product goes to a stack temp whose row 3 is never written, then all 12
  floats are copied to the top (0x51ff25..0x52003e, 0x5201c3). Modes 1 and 3
  are the usual ones; mode 2 reaches it only through 0x4ed280's mode, from
  0x5047f5 and 0x504979 (`zeff_anim_run` area) **[unverified: whether those
  read the translation]**. The identity shortcut copies all 12 floats of the
  local matrix in every mode. Port: `FIX (bug)`, translation 0. **For
  docs/fixes.md** (not edited here): "zmth::multiply mode 2 (0x51fee0): the
  original left the translation as leftover stack bytes; the port writes 0."
- **Identity shortcuts write only what changes.** scale writes the diagonal,
  translate row 3, rotate_* their four entries; they trust the buffer to
  really hold the identity, which holds because only load_identity sets the
  flag and push_copy copies flag and contents together (the other users of
  0x5eda8c, inlined point transforms at 0x4604fa, 0x4ebe18, 0x4ed7a3,
  0x4f5d41, 0x4fb121, 0x504868 and more, read it the same way as 0x53f2e0
  **[unverified: sampled, not all checked for writes]**).
- **The model draw's identity branches are dead.** 0x51fb30 always clears
  the flag (its own identity test reads the flag it just cleared), so the
  memcpy branches of 0x53f2e0's vertex loop never run.
- **euler skips translation parts equal to 0 and scales equal to 1**
  (0x5217d9.., 0x521819..). Same result, except that -0 becomes +0, a NaN
  translation becomes 0 and a NaN scale is ignored (the x87 compare treats
  unordered as equal). The port mirrors this (`differs`).
- **matrix_to_euler**: the gimbal branch is `|m[7]| >= 1` (NaN takes the
  asin branch); pitch there is `-_copysign(1.570796326795, m[7])`, the exe's
  pi/2 a few ulps over (0x3ff921fb54442eea), roll 0, yaw `atan2(-m[2], m[0])`.
  asin is MSVCRT `_CIasin` (0x5bb5f4), atan2 is `fpatan`. No sin/cos tables
  in this module: fsin/fcos/fsincos throughout, with a separate branch for
  |angle| > 9.22e18 (the x87's 2^63 range limit).
- **project and project_d3d are do-whiles:** count 0 runs 2^32 times; all
  callers pass 1 or more. The unproject pair checks `count > 0` first. The
  inline vertex loop on the point-model path (0x53f37d) does not check for
  0 vertices either **[unverified: whether any point model has none]**.
- **Software and Direct3D project to the same pixel**; only the third value
  differs (1/z or near/z). set_projection folds 1/near into fx/fy for
  Direct3D, project_d3d multiplies it back.
- **invert's singular test is exact**: det == 0 (or NaN) only. The x87
  keeps c2, det, 1/det unrounded; the port does them in double so the same
  matrices come out singular (float rounding made exactly singular input
  look invertible: the first port failed 80 cases there).

## Corrections and additions to docs/render.md

- Section 0 is right on every formula checked (euler, matrix -> euler,
  multiply order, view matrix, projection). Additions: 0x73fc00 keeps W'
  (camera -> world with y, z negated) after 0x520b00; 0x73fb94 is the far
  plane; on Direct3D 0x73fc30 / 0x73fb98 hold fx/near, fy/near.
- 0x51fee0's sums (2026-10-20, render-shadows.md): columns 0 and 1 and the
  translation's x, y left to right, but column 2 as `a0 t2 + (a1 t5 + a2
  t8)` and the translation's z as `(l9 t2 + t11) + (l10 t5 + l11 t8)`;
  `zmth::multiply` now follows them and matches bit for bit at 24 bits
  (test_shadow_image.py compares the cached world matrices exactly).
- Section 2, mode 2: confirmed, and the caller list (0x4ed280 mode 2 from
  0x5047f5 / 0x504979) is above.
- Section 3, step 2 ("top-of-stack * v"): the top is the model-view matrix
  built by 0x51fb30 (world * view) on a buffer 0x53f2e0 pushes, so vertices
  land in camera space directly; the identity copy there is dead.

## Check

`tests/differential/test_matrix.py` runs each original in Unicorn (MSVCRT
`_CIasin` and `_copysign` stood in by x86 snippets; the inlined vertex loop
run from 0x53f37d to 0x53f426 on a fake model) against the port through
`tests/probe_matrix.cpp`: the 15 stack operations (random top/below matrices
and flags, all four multiply modes, euler with 0/-0/NaN translations and
1/NaN scales), 500 cameras, 1,500 matrix -> euler (euler-built, the
`|m[7]|` edges 1, 1 +- 1 ulp, 2, NaN, atan2(0, 0), pitch +-pi/2 built by
0x521670), 800 inversions (a tenth singular), 400 projection setups (common
and random rects, fovs, near/far, both renderers) with 2,400 project /
unproject / scale runs, 1,500 point-op runs (counts 0..30). Tolerance: 1e-6
of the sum of |terms| per value (x87 against SSE). All 10,100 match.

Break tests (each a one-line change, all caught): multiply without the top's
translation; multiply ignoring the identity flag; euler m[2] sign; euler
copying a NaN translation (`!=` compare); matrix_to_euler gimbal yaw sign;
gimbal test at `> 1`; set_view without the transpose; projection centre
without the origin; project_d3d with software factors; unproject x with
1/fy; invert's determinant in float; model-view in the wrong order; inverse
transform without the translation; morph without the factor; rotate_y's
identity shortcut sign.

Not reached: the |angle| > 9.22e18 trig branches; count 0 in project,
project_d3d and the inline loop (they would run 2^32 times); stack overflow.
Mode 2's translation is not compared (garbage in the original).

## Not ported (same module)

Billboard tops 0x51ea90 / 0x51efb0 / 0x51f390 / 0x51f750; 0x51e740 (2D
normal with the approximate square root); 0x51e930's neighbours 0x51e970
(unreferenced copy); unreferenced 0x51fe60 (top = diag(1,-1,-1)), 0x520200
(scale by a vector), 0x520c60 / 0x521080 / 0x5214f0 (camera from euler and
position, building 0x73fbc0 / 0x73fc00 directly), 0x521a90 (euler to a given
matrix), 0x521e10 (projection as a matrix); 0x522170 / 0x522210 / 0x522460
(direction vectors from angles), 0x522280 / 0x5223c0 (2D side tests),
0x5224b0 (plane from 3 points), 0x522ab0 / 0x522d60 / 0x523040 / 0x5231b0
(box corners and extents through a matrix), 0x523650 / 0x523930 (screen
points back to world), 0x523ac0 / 0x523b40 (angles toward a point),
0x523df0.. (single-axis vector rotations), 0x522840 (an in-place transform
Ghidra did not make a function), and the rest of 0x524000..0x526920.

## Integration notes

- `gmod_fogcalc.h` keeps its own `camera_to_world` at 0x73fc00; it should
  read `zmth::view().camera` once both land.
- The Direct3D flag 0x72f348 is read through `zrndr::device_fog().active`,
  the existing port of that global.
