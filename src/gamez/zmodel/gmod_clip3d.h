#pragma once
#include <cstdint>

// The camera-space near/far clippers the Direct3D model draw (0x0053f2e0)
// runs before projection (docs/drafts/clip3d.md, docs/render.md section 3).
// Original file unknown: between gmod_init.c and gmod_matl.c.
namespace zmod {

// The clip struct at 0x007402f8 (ECX of every clipper). The side planes
// (+0x4..+0x20) belong to the screen-space clippers.
struct ClipPlanes {
  uint32_t mask = 0;               // +0x00 0x007402f8  planes to clip: 0x10 near, 0x20 far, 0xf sides
  uint32_t side_04 = 0;            // +0x04 left (screen clippers)
  uint32_t side_08 = 0;            // +0x08 top
  float near_z = 0;                // +0x0c 0x00740304
  uint32_t unknown_10 = 0;         // +0x10
  uint32_t unknown_14 = 0;         // +0x14
  float far_z = 0;                 // +0x18 0x00740310
  uint32_t side_1c = 0;            // +0x1c right
  uint32_t side_20 = 0;            // +0x20 bottom
};
static_assert(sizeof(ClipPlanes) == 0x24, "ClipPlanes covers +0x00..+0x20");

// The globals the clippers read and rewrite, 64 vertices each (the buffers
// around them are laid out that way). The first three are the model draw's
// per-polygon buffers and move to its port later; the *_out ones are the
// clippers' scratch. Nothing checks the 64: a polygon that clips to more
// overruns into the next buffer in the original.
struct Clip3dBuffers {
  static constexpr int kVerts = 64;
  float cam[kVerts][3] = {};           // 0x007476a4  camera-space vertices
  float rec[kVerts][8] = {};           // 0x007481a8  per-vertex work record (render.md section 5)
  float tl[kVerts][8] = {};            // 0x0072f41c  D3DTLVERTEX; only tu, tv (+0x18) here
  float tl_out[kVerts][8] = {};        // 0x0063d890  only tu, tv (+0x18)
  float cam_out[kVerts][3] = {};       // 0x0063e290
  float rec_out[kVerts][8] = {};       // 0x0063f390
  // Projected shadows (render.md section 5): up to 7 lights, one UV set of
  // 64 vertices each, set s at [64 * s] (flat, as the original indexes it).
  static constexpr int kShadows = 7;
  int32_t shadow_count = 0;                     // 0x007489a8
  float shadow_uv[kShadows * kVerts][2] = {};  // 0x007489c8
  float shadow_out[kShadows * kVerts][2] = {}; // 0x0063e590
};
Clip3dBuffers& clip3d_buffers();

// ECX = the clip struct, EDX = &count. Each returns false to drop the polygon
// and rewrites the buffers and count when the near plane cuts it. They differ
// only in what they carry along: the draw paths of 0x0053f2e0 they serve.
bool clip3d_flat(const ClipPlanes& clip, int32_t& count);         // 0x005275e0 untextured
bool clip3d_lit(const ClipPlanes& clip, int32_t& count);          // 0x00527a30 untextured, lit/fogged
bool clip3d_textured(const ClipPlanes& clip, int32_t& count);     // 0x00527fd0 textured
bool clip3d_textured_lit(const ClipPlanes& clip, int32_t& count); // 0x005286e0 textured, per-vertex light/fog
// The same two with the shadow UV sets carried along.
bool clip3d_textured_shadow(const ClipPlanes& clip, int32_t& count);     // 0x00528bb0
bool clip3d_textured_lit_shadow(const ClipPlanes& clip, int32_t& count); // 0x00529120

}  // namespace zmod
