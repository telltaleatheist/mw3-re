#pragma once
#include <cstdint>

// The Direct3D 6 draw backend's transparent queue: polygons with alpha are
// copied out during the scene, sorted far to near and drawn blended at the end
// (docs/render.md section 3, docs/drafts/fogsort.md). Original: zvid_ddd3d.c.
namespace zvid {

struct TLVertex {                  // D3DTLVERTEX, FVF 0x1c4
  float sx, sy, sz, rhw;
  uint32_t color, specular;
  float tu, tv;
};
static_assert(sizeof(TLVertex) == 0x20, "D3DTLVERTEX is 0x20 bytes");

// The texture record 0x0057b130 makes (0x18 bytes in the original).
struct D3DTexture {
  void* system = nullptr;          // +0x00 system-memory surface
  void* video = nullptr;           // +0x04 video-memory surface
  void* d3d = nullptr;             // +0x08 IDirect3DTexture2, what SetTexture gets
  int32_t type = 1;                // +0x0c 1 opaque 565, 5 ARGB1555, 4 ARGB4444
  int32_t address_u = 1;           // +0x10 D3DTADDRESS (1 wrap, 3 clamp)
  int32_t address_v = 1;           // +0x14
};

struct TransparentPoly {           // 0x0080cd9c, 0x10 bytes each
  int32_t primitive;               // +0x00 D3DPRIMITIVETYPE, from [0x0072f418]
  int32_t count;                   // +0x04 vertices
  const D3DTexture* texture;       // +0x08 null when untextured
  TLVertex* verts;                 // +0x0c into TransparentQueue::verts
};

struct TransparentQueue {
  static constexpr uint32_t kMaxPolys = 6000;   // MAX_TRANSPARENT_POLYS
  static constexpr uint32_t kMaxVerts = 24000;  // MAX_TRANSPARENT_VERTICES
  uint32_t count = 0;                // 0x00803550
  uint32_t vertex_count = 0;         // 0x00803554
  int32_t order[kMaxPolys] = {};     // 0x00803558  draw order, indices into polys
  TransparentPoly polys[kMaxPolys] = {};  // 0x0080cd9c
  TLVertex verts[kMaxVerts] = {};    // 0x0082449c
};
TransparentQueue& transparent_queue();

// The device's cached render states (the D3D calls go through Device).
struct DeviceState {
  int32_t primitive = 6;           // 0x0072f418  current D3DPRIMITIVETYPE
  int32_t alpha_blend = 0;         // 0x00809388  ALPHABLENDENABLE (27)
  int32_t shade_mode = 0;          // 0x0080938c  SHADEMODE (9)
  int32_t alpha_op = 0;            // 0x00809390  ALPHAOP (4); holds a texture type, see 0x0057e110
  int32_t address_u = 0;           // 0x00809394  ADDRESSU (13)
  int32_t address_v = 0;           // 0x00809398  ADDRESSV (14)
  int32_t z_write = 0;             // 0x008093a8  ZWRITEENABLE (14)
};
DeviceState& device_state();

// FIX (A): the IDirect3DDevice3 methods the queue calls ([0x00809374] vtable,
// this = [0x00809370]), set by the front end; a recorder in the tests.
struct Device {
  void* ctx = nullptr;
  void (*set_render_state)(void* ctx, int32_t state, int32_t value) = nullptr;           // +0x58
  void (*set_texture_stage_state)(void* ctx, int32_t stage, int32_t type, int32_t value) = nullptr;  // +0xa0
  void (*set_texture)(void* ctx, int32_t stage, void* texture) = nullptr;                // +0x98
  int32_t (*draw_primitive)(void* ctx, int32_t primitive, uint32_t fvf, const TLVertex* verts,
                            int32_t count, uint32_t flags) = nullptr;                    // +0x70
  void (*report)(void* ctx, int32_t hresult, int32_t line) = nullptr;                    // 0x00580550
};
Device& device();

// The block every transparent draw inlines (0x0057bf80, 0x0057c2f0, 0x0057caa0,
// 0x0057ce20, 0x0057d160, 0x0057d690, 0x0057dba0): reserves a polygon and its
// vertices, or reports the overflow and returns null. The caller fills them.
TLVertex* queue_transparent(int32_t primitive, int32_t count, const D3DTexture* texture);

void sort_transparent();   // 0x0057dec0
void flush_transparent();  // 0x0057e110

}  // namespace zvid
