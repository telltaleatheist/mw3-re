# What the software renderer showed (audit, 2026-10-30)

CLAUDE.md's rendering rule: anything only the software path showed comes to
the GPU path. This audit looked for such things. **Finding: the software
renderer showed nothing the Direct3D path lacks.** Everywhere the two differ,
the software side is the reduced one: fewer effects, fewer lights, smaller
textures, no shadows. Its own code is the plumbing that does in 16-bit
software what the card does. One look difference (the fog's start) and the
software look as a whole are candidates for Owen; the rest is kept out.

## How

- The two frames: the Direct3D frame 0x00481790 against the software ones
  0x00481290 and 0x00481a10, by their direct calls; the scene walks
  0x004f7030 / 0x004f6dc0 and the model draws 0x0053f2e0 / 0x0053e050, by
  what each reaches (three calls deep).
- Every function that reads the hardware flag [0x0072f348] (83), each
  branch's software side: 74 are ported (the port takes the Direct3D side;
  their comments say what the other did), 9 are not.
- The data: the 654 animation sequences that test "hardware" (if/elseif
  flag 0x20, 0x00507010) and their ELSE branches; the software texture sets
  against the hardware ones (names and sizes).

## The list

| What | Where | What the player saw in software | Verdict |
|---|---|---|---|
| The rasteriser: span fillers, the span (depth) buffer, the 16-bit polygon fills, the window's mask polygons | 0x0055bbf0, 0x0055c2f0, 0x0055c900, 0x0055d660, 0x0055e310, 0x0055efa0, 0x005608f0, 0x00561910; 0x00564460 / 0x00564500 / 0x00564660 (the mask polygons, the Direct3D walk's depth fans after the scene); 0x00566c90..0x005670a0 | the same scene, drawn by the CPU | kept out (plumbing) |
| The software model draw, its projection and clips | 0x0053e050 (and 0x005273f0..0x0052c8d0, 0x00535c10 under it) | the same models | kept out |
| Light and fog by tables: 32 levels of every paletted texture, lit and fogged toward the light's and the fog's colours | 0x005195b0, 0x00519370 (tables), the software lighting 0x00544060 | banded (32 steps) lighting and fog, 8-bit colours | kept out (the GPU lights per vertex); the banding is part of "the software look" below |
| Fog distances | the worlds' world.zrd: `range` 500..750 (software), `hwrange` 32..750 (t1 64..750) (render.md section 7) | fog starting at 500 m; in Direct3D it starts at 32 m, so the hardware game is hazier close up | **candidate**: an option to start the fog where software did |
| Paletted, smaller textures | the worlds' `texture*.zbd` against `rtexture*.zbd` (the same names; c1: 42 of 581 at the same size, the rest smaller); `rmechtexs.zbd` 2286 mech textures against `rmechtex.zbd`'s 3360, about half smaller | blurrier, 8-bit textures | kept out (the hardware sets are the full ones) |
| `txgter_2`, `ggter_2`, `gter_2` | one texture in each of c1's, c2's and c3's software sets, absent from the hardware sets | nothing: no world, script or string names them | kept out (unused) |
| Animation "if hardware" ELSE branches | 134 of 654 sequences have an ELSE unlike their hardware branch: vehicles' and buildings' deaths `effectslow` instead of `effectshigh` (no shockwave, fewer fire and dust anims), fires without their smoke and flickering light, explosions and PPC impacts with one light instead of two, kicked dust switched on instead of scaled, c4b's fog changes without the lava room's light | fewer, plainer effects | kept out (the port takes the hardware branches) |
| Model lights | 0x00541df0 | no light point drawn (Direct3D draws one, faded with distance); flares the same (the same flare flag, light +0x3c; the software list also tested them against its span buffer, 0x005675e0) | kept out |
| Lens flares | 0x005675e0, 0x00567dc0 (software list and draw), against 0x00567d30 | the same four sprites per flaring light | kept out (the same flares) |
| Projected shadows | the Direct3D walk only (0x00543720 / 0x00543760) | no mech shadows | kept out (the GPU has them) |
| The sun's light | 0x00492660 | the sun node not lit in software | kept out |
| Rain | 0x0055a950 (software) against 0x005a57d0 | the same rain, drawn into the 16-bit buffer | kept out (the port draws the Direct3D one) |
| A darkened rect | 0x0055a740: software ANDs the pixels with a mask, Direct3D draws a 30% tint quad | the same darkening | kept out |
| The HUD's see-through panels | 0x0043a7c0 | opaque green instead of 60% see-through | kept out |
| TARGET's first mode | 0x00441d90 | the armour bars (3) instead of the 3D view (1) | kept out (a default; the player cycles modes either way) |
| The zoom window's 3D pass | 0x00443320 (software 0x00564460, 0x004f6dc0) | the same view | kept out |
| The static (noise) screens | 0x0043cd20, `zrndr::static_fill` | the same static, on both paths | ported already |
| The colour tint's finish | 0x00558610 (MMX rows; leaves the last four columns of a 640-wide screen untinted) | the fades and flashes | ported (render-tint.md) |
| The screen pick | 0x0053d560 / 0x00566070: software reads the span buffer's depth at a pixel, Direct3D casts a ray | nothing visible | kept out |
| Video set-up, input, flags | 0x00575d10, 0x00577df0, 0x00579210, 0x0058fac0, 0x00519cc0 (the screenshot's save dialog), 0x005a7288 | nothing visible | kept out |

No detail textures, water passes or sky passes exist apart from the shared
ones: the sky dome and the water are models drawn by both paths; "detail"
in the exe is only detail.zrd, the settings file.

## For Owen

1. **The fog's start** (the one difference with a look of its own): the
   software game kept the near 500 m clear, where Direct3D fogs from 32 m.
   An option could use `range` instead of `hwrange` on the GPU path.
2. **The software look as a whole** (already noted in CLAUDE.md as a possible
   shader pass): 8-bit paletted textures, banded 32-step lighting and fog,
   no filtering, no shadows, plainer effects. Only if wanted as an option;
   nothing in it is missing from the port's picture.
