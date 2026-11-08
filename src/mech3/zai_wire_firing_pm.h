#ifndef MECH3_ZAI_WIRE_FIRING_PM_H
#define MECH3_ZAI_WIRE_FIRING_PM_H

// The five firing hook classes on the program. Their calls into the plan, pick and shot pieces are real; a
// weapon manager's lists and the rest with no answer yet are counted (pm_hook_missing), so a pilot picks a
// target and doesn't fire until those land.
#include "mech3/zai_brain.h"
#include "mech3/zai_firing_pm.h"

namespace mech3::ai {

// One per vehicle that fires: the exe's firing object, split as the port's pieces take it.
struct FiringRecord {
  FiringPm fire;
  PickFiring pick;
  AimPm aim;
};
FiringRecord& pm_firing(Brain& b, zgame::Handle vehicle);  // made at the first ask
void pm_firing_forget(zgame::Handle vehicle);
void pm_firing_install();                                  // the hooks set; `PM_AI=1` only

}  // namespace mech3::ai

#endif
