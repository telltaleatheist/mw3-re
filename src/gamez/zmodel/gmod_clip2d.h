#pragma once
#include <cstdint>

// Screen-space side clipping of one projected polygon against the viewport
// rect, after the near clip and projection (docs/render.md section 3,
// docs/drafts/clip2d.md). Original file unknown: between gmod_init.c and
// gmod_matl.c.
namespace zmod {

// D3DTLVERTEX, 0x20 bytes.
struct TLVertex {
  float sx, sy, sz, rhw;
  uint32_t diffuse, specular;
  float tu, tv;
};

// The per-vertex work record (docs/render.md section 5), 0x20 bytes.
struct VertexWork {
  float light[3];   // +0x00 darkness, point-light intensity, fog factor
  float color[3];   // +0x0c vertex colour (0..255)
  float unused[2];  // +0x18
};

// The clip struct the model draw passes in ECX (0x7402f8, or 0x740338 for
// the second pass at 0x54168d); filled by 0x526e10.
struct ScreenClip {
  uint32_t head;        // +0x00
  float left, top;      // +0x04, +0x08
  float unknown[4];     // +0x0c
  float right, bottom;  // +0x1c, +0x20, exclusive
};

// Slots per buffer: the work records end at the shadow sets (0x7481a8 +
// 0x800 = 0x7489a8); 0x63d890 has room for 104. The original does not
// bound-check: a polygon may gain one vertex per clipped side.
constexpr int kClipVerts = 64;

struct ShadowUV {
  float u, v;
};
constexpr int kShadowSets = 7;    // the shadowed clippers keep 7 set pointers on the stack
constexpr int kShadowVerts = 64;  // 0x200 bytes per set

// Projected-shadow texture coordinates, one set per shadow (docs/render.md
// section 5). Owned by the shadow code; here until that is ported.
struct ShadowSets {
  int32_t count;                              // 0x007489a8
  uint32_t unknown[7];                        // 0x007489ac
  ShadowUV uv[kShadowSets][kShadowVerts];     // 0x007489c8
};

struct Clip2D {
  TLVertex tl[kClipVerts];        // 0x0072f41c, the projected polygon
  VertexWork work[kClipVerts];    // 0x007481a8
  TLVertex tl_tmp[kClipVerts];    // 0x0063d890, ping-pong partner of tl
  ShadowUV shadow_tmp[kShadowSets][kShadowVerts];  // 0x0063e590, partner of shadow.uv
  VertexWork work_tmp[kClipVerts];  // 0x0063f390, ping-pong partner of work
  ShadowSets shadow;
};
Clip2D& clip2d();

// Each clips clip2d().tl[0 .. *count) in place, rewrites *count and returns
// 0 when fewer than 3 vertices are left (the polygon is dropped), else 1.
int32_t clip2d_flat(const ScreenClip& r, int32_t* count);         // 0x00529ce0
int32_t clip2d_lit(const ScreenClip& r, int32_t* count);          // 0x0052aaf0
int32_t clip2d_textured(const ScreenClip& r, int32_t* count);     // 0x0052bd10
int32_t clip2d_textured_lit(const ScreenClip& r, int32_t* count); // 0x0052dc80
// The same two with shadow.count projected-shadow UV sets carried along.
int32_t clip2d_textured_shadowed(const ScreenClip& r, int32_t* count);      // 0x0052e920
int32_t clip2d_textured_lit_shadowed(const ScreenClip& r, int32_t* count);  // 0x0052fab0

}  // namespace zmod
