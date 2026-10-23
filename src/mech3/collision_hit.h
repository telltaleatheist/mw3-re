#pragma once
#include <cstdint>

#include "gamez/zclass/cls_di.h"
#include "mech3/vec3.h"

// One sorted crossing of the collision sweeps (malloc(0x48) in the exe), the
// mechs' (mech_collide) and the vehicles' (veh_move): the query's slot, the
// segment and, for a mech, the probe it came from.
namespace mech3 {

struct CollisionHit {
  zcls::SegmentHit hit;            // +0x00  normal, point, ..., material +0x20, node +0x24
  Vec3 from{}, to{};               // +0x28, +0x34
  int32_t probe = 0;               // +0x40
};

}  // namespace mech3
