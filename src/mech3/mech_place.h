#pragma once
#include <cstdint>

#include "gamez/zclass/cls_di.h"
#include "mech3/mech_parts.h"

// Putting a mech on the ground (mech_place.cpp): the calls it makes outside
// the mech code. Unset, they run the ports on world_hooks().world. vtable
// +0x18 is collide_calls().set_position when set.
namespace mech3 {

struct PlaceCalls {
  int (*ground)(float x, float y, float z, zcls::GroundHits* out) = nullptr;  // 0x004edab0 on [0x00608c84]+4
  void (*attach)(Handle node) = nullptr;  // [0x00608c84] vtable +0 (0x0040bab0): 0x004eb9b0 under the world
};
PlaceCalls& place_calls();

}  // namespace mech3
