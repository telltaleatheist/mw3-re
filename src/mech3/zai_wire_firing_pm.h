#ifndef MECH3_ZAI_WIRE_FIRING_PM_H
#define MECH3_ZAI_WIRE_FIRING_PM_H

// The five firing hook classes on the program. Their calls into the plan, pick and shot pieces are real; a
// weapon manager's lists and the rest with no answer yet are counted (pm_hook_missing), so a pilot picks a
// target and doesn't fire until those land.
#include "mech3/zai_brain.h"
#include "mech3/zai_firing_pm.h"

namespace mech3 {
struct Weapon;
}

namespace mech3::ai {

// One per vehicle that fires: the exe's firing object, split as the port's pieces take it.
struct FiringRecord {
  FiringPm fire;
  PickFiring pick;
  AimPm aim;
};
FiringRecord& pm_firing(Brain& b, zgame::Handle vehicle);  // made at the first ask
// [0x0062f86c]: the weapon line's facing test skipped (0x004b7800), set by a forced start (0x004b7600, 0x004b8030)
// around its line and cleared after.
bool pm_line_quiet();
void pm_set_line_quiet(bool on);
void pm_set_aim_floor(mech3::Weapon& w, float rate);  // 0x004b7a90: the volley's tolerance on a weapon's +0x98
void pm_firing_forget(zgame::Handle vehicle);
void pm_hit_an_ally(FiringRecord& r, zgame::Handle victim);  // 0x004bb6a0: the ally kept, then 0x004ba8c0
void pm_stand_down(FiringRecord& r);                          // 0x004ba8c0
void pm_forget_target(zgame::Handle vehicle, zgame::Handle x);  // 0x004fbe10 -> 0x004bd280: `x` gone
void pm_firing_install();                                  // the hooks set; `PM_AI=1` only

}  // namespace mech3::ai

#endif
