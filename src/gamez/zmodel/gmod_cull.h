#pragma once
#include <cstdint>

#include "gamez/zrender/zrndr_span.h"

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
int32_t zones_overlap(const uint8_t a[4], const uint8_t b[4]);  // 0x0053cc90  ECX a, EDX b: share a zone

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

// --- The casters and their images (0x00542bb0..0x00543330, docs/render-shadows.md) ---
struct ShadowState {
  uint32_t black = 0;              // 0x0064fbd8  pack(0, 0, 0): a new caster's colour
  uint32_t white = 0;              // 0x0064fbdc  pack(255, 255, 255), never read
  ShadowCaster* current = nullptr; // 0x0073ff84  the caster whose image is being drawn
  // Not in the original: the x87 precision the image's polygon filler
  // (0x0055d010) runs at. In play on Windows 11 + DDrawCompat the shadows
  // show (Owen, 2026-10-20), so the FPU is at 53 bits there: k53, the
  // default. k24 is what a Direct3D 6 that truncates the FPU gives: every
  // image blank (render-shadows.md).
  zrndr::X87 precision = zrndr::X87::k53;
};
ShadowState& shadow_state();

void shadows_init();                                       // 0x00542bb0  shadows on with the caps
ShadowCaster* shadow_new();                                // 0x00542c20  at the list's head
void shadow_delete(ShadowCaster* c);                       // 0x00542ca0  ECX
void shadow_set_size(ShadowCaster* c, int32_t w, int32_t h);  // 0x00542d30  ECX, EDX w, stack h
void shadow_set_node(ShadowCaster* c, zcls::Node* node);   // 0x00542dd0
void shadow_set_position(ShadowCaster* c, const float p[3]);  // 0x00542de0
void shadow_set_color(ShadowCaster* c, uint32_t color);    // 0x00542e00
void shadow_upload(ShadowCaster* c);                       // 0x00542e10  the texture "gModShadow"
void shadow_release_texture(ShadowCaster* c);              // 0x00542e40
void shadow_enable(ShadowCaster* c, int32_t on);           // 0x00542e60
// ECX caster, EDX the light's world direction, stack the ground height: the
// node's silhouette slid down the light onto the ground, blurred into the
// image. 1 (nothing drawn) unless the light points down; 0 when drawn.
int32_t shadow_draw(ShadowCaster* c, const float light[3], float ground);  // 0x00542e70
void shadow_node(zcls::Node* node);   // 0x005432d0  by class (inlined in 0x00542e70)
void shadow_model(zcls::Node* node);  // 0x00543330  the node's model into the current image

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
