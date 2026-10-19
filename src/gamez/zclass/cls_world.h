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
int world_push_fog(Node* world);                                       // 0x004f1df0

struct World;
// 0x004f18a0, called for each pending area entry; not ported yet.
using AreaUpdate = void (*)(World* world, uint32_t* entry);
void set_area_update(AreaUpdate fn);

}  // namespace zcls
