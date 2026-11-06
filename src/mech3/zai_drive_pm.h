#ifndef MECH3_ZAI_DRIVE_PM_H
#define MECH3_ZAI_DRIVE_PM_H

// Pirate's Moon's way of driving a mech at the steering's carrot, what MW3's `walk_drive` and the turns
// beside it do (zai_node); docs/pm-map.md, step 3. Nothing runs it until the orders' switch.
#include "mech3/zai_walk_pm.h"

namespace mech3::ai {

// The turn's constants: the dead zone's cosine and sine, the gain, the floor, and facing away below `behind`.
struct TurnPmParams {
  float cos_dead, sin_dead, gain, least, behind;
};
const TurnPmParams& turn_walk_params();  // [0x0062fb08], 0x004d2d40
float turn_pm(const float fwd[3], const float to[3], float* dot, float* cross, float keep, const TurnPmParams& p);  // 0x004d7060
// 0x004d71e0: past 135 degrees it's measured from the vehicle's back, and `back` says so.
float turn_or_back_pm(const float fwd[3], const float to[3], float* dot, float* cross, float keep, bool* back,
                      const TurnPmParams& p);

void away_pm(const SteerPm& s, float away[2], float* sum);  // 0x004dccf0
// 0x004dcd20: this frame's turn and throttle. `near`: closer than this it faces along the way.
void drive_pm(WalkPmHooks& h, SteerPm& s, zgame::Handle v, const float away[2], float dist, float near, float* turn,
              float* throttle, float inv_slow, float slack_along, float slack_side);

}  // namespace mech3::ai

#endif
