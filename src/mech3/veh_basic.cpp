#include "mech3/veh_basic.h"

#include "gamez/zmodel/gmod_matl.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3 {

// Mech3.exe 1.2 @ 0x004abd90. The material's soil is read directly, not
// through the soil table (docs/mission-start.md, The placement).
float ground_pick(const zcls::GroundHits& hits, float y, float step, int32_t* best, int32_t water_not_ground,
                  int32_t* in_water, float* surface) {
  *best = 0;
  *in_water = 0;
  *surface = -200.0f;
  float best_h = -200.0f;
  if (hits.count == 0) return 0.0f;  // 0x005c88c0
  float low = 0.0f;
  int32_t water = 0;
  for (int32_t i = 0; i < hits.count; ++i) {
    const zcls::GroundHit& g = hits.hit[i];
    const float h = g.height;
    if (i == 0 || !(h >= low)) low = h;
    if (g.material && g.material->soil == 1) {
      *surface = h;
      water = 1;
      if (water_not_ground) continue;
    }
    if (h > best_h && !(h - step > y)) {
      best_h = h;
      *best = i;
    }
  }
  if (step + *surface >= y) *in_water = water;
  return best_h > low ? best_h : low;
}

}  // namespace mech3
