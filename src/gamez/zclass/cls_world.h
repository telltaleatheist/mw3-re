#pragma once
#include <cstdint>

#include "gamez/zclass/cls_node.h"

// zClass\cls_world.c: the world's fog. Each setter stores the value and marks
// it changed in world +0; push_fog hands the changes to the renderer.
namespace zcls {

enum FogChanged : uint32_t { kFogType = 1, kFogColor = 2, kFogRange = 4, kFogDensity = 8,
                             kFogAltitude = 0x20 };

int world_set_fog_type(Node* world, uint32_t type);                    // 0x004f23e0
int world_set_fog_color(Node* world, float r, float g, float b);       // 0x004f23f0
int world_set_fog_altitude(Node* world, float at_2c, float at_28);     // 0x004f2420
int world_set_fog_range(Node* world, float near_range, float far_range);  // 0x004f2440
int world_set_fog_density(Node* world, float density);                 // 0x004f2460
void world_get_fog_color(Node* world, float* r, float* g, float* b);   // 0x004f24a0
void world_get_fog_range(Node* world, float* near_range, float* far_range);  // 0x004f24c0
void world_get_fog_altitude(Node* world, float* at_2c, float* at_28);  // 0x004f24e0
int world_push_fog(Node* world);                                       // 0x004f1df0

// The partition grid (docs/drafts/scenewalk.md). Cells past the grid on
// either side wrap into the virtual block (world +0x54..+0x60).
// ECX world node, EDX cell x, stack cell z: the cell actually stored and
// whether the given one is inside the grid.
int world_wrap_cell(Node* world, int32_t x, int32_t z, int32_t* out_x, int32_t* out_z,
                    int32_t* inside);                                 // 0x004f1f10
// ECX world node, EDX &cell x: the cell under a world (x, z), floor-divided,
// then wrapped.
int world_cell_wrapped(Node* world, int32_t* cell_x, float x, float z, int32_t* cell_z,
                       int32_t* out_x, int32_t* out_z, int32_t* inside);  // 0x004f1fe0
// ECX world node, EDX &cell x: the cell under (x, z) clamped to just inside
// the grid, truncated.
int world_cell_clamped(Node* world, int32_t* cell_x, float x, float z, int32_t* cell_z);  // 0x004f2060

// Adding and removing children, lights and sounds (ECX world, EDX node).
// A child goes in the one partition cell its box fits (its +0x4c / +0x50 the
// cell), else among the world's direct children (-1, -1); a cell that
// changes joins the world's pending list (+0x04..+0x0c).
int world_add_child(Node* world, Node* node);     // 0x004f2a40
int world_remove_child(Node* world, Node* node);  // 0x004f2c30
int world_add_light(Node* world, Node* light);    // 0x004f2d50
int world_remove_light(Node* world, Node* light);  // 0x004f2e00
int world_add_sound(Node* world, Node* sound);    // 0x004f2f90
int world_remove_sound(Node* world, Node* sound);  // 0x004f3040

struct World;
// 0x004f18a0, called for each pending area entry; not ported yet.
using AreaUpdate = void (*)(World* world, uint32_t* entry);
void set_area_update(AreaUpdate fn);

}  // namespace zcls
