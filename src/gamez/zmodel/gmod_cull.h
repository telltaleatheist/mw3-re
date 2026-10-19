#pragma once
#include <cstdint>

// What the scene walk asks the model renderer before drawing a node: the
// sphere-against-frustum cull (0x00541760), the zone test (0x0053cdc0), the
// hidden-surface test (0x0053d750, not ported), the view polygon the
// partition picker tests cells against (0x0054fe80, 0x0054ff40) and the
// projected-shadow caster list (0x005438d0, 0x005438f0). Original files
// unknown: the cull, zone and hidden-surface tests sit between gmod_const.c
// and gmod_cookie.cpp, the caster list between gmod_cookie.cpp and
// gmod_light.c, the view polygon between gmod_puff.cpp and zreader.cpp
// (docs/drafts/scenewalk.md).
namespace zcls { struct Node; }

namespace zmod {

// --- Zones ---
struct Zones {
  int32_t mode = 1;                // 0x005ea1d0  0: every node passes
  uint8_t list[4] = {};            // 0x0074a328  count, then ids (0xff = all)
};
Zones& zones();
int32_t zone_visible(uint8_t zone);  // 0x0053cdc0  ECX the node's zone byte (+0x30)
void zone_reset(uint8_t list[4]);    // 0x0053cc40  count 0, ids 0xff

// --- Cull ---
// ECX centre (world), EDX &mask, stack radius. Tests the planes whose bit is
// set in *mask against the camera set by set_camera (0x00526e10); *mask
// becomes the planes the sphere crosses. Returns the plane bit that rejects
// it, or 0.
uint32_t cull_sphere(const float centre[3], uint32_t* mask, float radius);  // 0x00541760

// --- The view polygon (XZ) of the partition picker ---
struct ViewPolygon {
  float points[64][3] = {};        // [0x0074a290] = 0x0074039c (the model draw's camera buffer)
  float normals[64][3] = {};       // [0x0074a294] = 0x0074339c
  int32_t count = 0;               // 0x0074a298
};
ViewPolygon& view_polygon();
int32_t set_view_polygon(const float (*points)[3], int32_t count);  // 0x0054fe80, 0 over 64
int32_t inside_view_polygon(const float point[3], float tolerance); // 0x0054ff40

// --- Projected-shadow casters (docs/render-shadows.md) ---
// A caster record, 0x8c bytes (made by 0x00542c20 for each mech near the
// player, filled by 0x00542e70): the node casting, the box its shadow covers
// on the ground and the texture drawn over the receivers inside it.
struct ShadowCaster {
  zcls::Node* node = nullptr;      // +0x00
  float position[3] = {};          // +0x04  world (0x00542de0)
  float ground_at[2] = {};         // +0x10  x, z where the light through position meets ground_y
  uint32_t color = 0;              // +0x18  display-packed shadow colour (0x00542e00)
  float ground_y = 0;              // +0x1c  the height the shadow is cast onto
  float slope[2] = {};             // +0x20  light x / y, z / y: the ground shift per unit of height
  float slope_len = 0;             // +0x28  |slope|
  float box_min[3] = {};           // +0x2c  the shadow's box on the ground (y: the node's lowest)
  float box_max[3] = {};           // +0x38  (+0x3c the node's top + 2)
  float scale[2] = {};             // +0x44  1 / box width in x, z: ground to texture UV
  float slope_cam[3] = {};         // +0x4c  (slope x, 1, slope z) in camera space (0x00543760)
  int32_t enabled = 0;             // +0x58
  void* image = nullptr;           // +0x5c  zimg::Texture, the shadow's pixels
  void* texture = nullptr;         // +0x60  zvid::D3DTexture made from it
  uint8_t unknown_64[0x20] = {};   // +0x64
  ShadowCaster* prev = nullptr;    // +0x84
  ShadowCaster* next = nullptr;    // +0x88
};
ShadowCaster*& caster_list();      // 0x0064fbe0  the head, newest first

struct ShadowCasters {
  ShadowCaster* all[200] = {};     // 0x0073fc60, filled by 0x00543720
  int32_t all_count = 0;           // 0x007402c0
  ShadowCaster* cell[200] = {};    // 0x0073ffa0, filled per partition by 0x00543810
  int32_t cell_count = 0;          // 0x0073ff80
  ShadowCaster** list = nullptr;   // 0x0073fc4c  the set the walk checks
  int32_t count = 0;               // 0x007402c4
};
ShadowCasters& shadow_casters();
void use_all_casters();            // 0x005438d0
void use_cell_casters();           // 0x005438f0
void collect_casters();            // 0x00543720  the enabled casters into all[] (200 at most)
void caster_directions();          // 0x00543760  slope_cam of each, by the view
void cell_casters(const float* box);  // 0x00543810  ECX the partition's box (+0x10): those over it

// The model renderer's calls the walk makes. Null hooks do nothing; the
// hidden-surface test then passes everything. The caster hooks default to
// the ports above (tests swap them).
struct CullHooks {
  void* ctx = nullptr;
  // 0x0053d750(ECX centre, stack radius): 0 when the sphere is hidden behind
  // what is already drawn (the software span buffer), else 1.
  int32_t (*hidden_surface)(void* ctx, const float centre[3], float radius) = nullptr;
  void (*collect_casters)(void* ctx) = [](void*) { zmod::collect_casters(); };      // 0x00543720
  void (*caster_directions)(void* ctx) = [](void*) { zmod::caster_directions(); };  // 0x00543760
  // 0x00543810(ECX partition +0x10): the casters over this cell into cell[].
  void (*cell_casters)(void* ctx, const float* rect) = [](void*, const float* r) { zmod::cell_casters(r); };
};
CullHooks& cull_hooks();
int32_t hidden_surface(const float centre[3], float radius);  // 0x0053d750 through the hook

}  // namespace zmod
