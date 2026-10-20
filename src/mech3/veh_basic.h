#pragma once
#include <cstdint>

#include "gamez/zclass/cls_di.h"

// Original file unknown: the code sits between mech3\turret.cpp and
// mech3\veh_basic.cpp (named veh_basic here, after the file it precedes).
namespace mech3 {

// EDX the record of a point query; the point's y and how far above it the
// ground may be. The highest polygon no more than `step` above y (its index
// in *best); water (a material of soil 1) gives the surface (-200 without)
// and is ground only when water_not_ground is 0; *in_water when the surface
// is within step below y. With none in reach, the lowest polygon; an empty
// record gives 0. (The skis' pick 0x00477820 is the same but for its
// defaults and the soil table.)
float ground_pick(const zcls::GroundHits& hits, float y, float step, int32_t* best, int32_t water_not_ground,
                  int32_t* in_water, float* surface);  // 0x004abd90

}  // namespace mech3
