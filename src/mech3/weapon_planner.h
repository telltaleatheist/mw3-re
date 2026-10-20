#pragma once
#include "mech3/weapon.h"

// The AI's weapon planner (docs/ai.md section 17.7): weapon.cpp's tail,
// 0x004b1170..0x004b1d7f. Only the three the HUD reads are here, the
// selected group's longest range, lock-on and ammo; the volley pick 0x004b1860
// is not ported.
namespace mech3 {

// Mech3.exe 1.2 @ 0x004b1bd0: in group fire the longest of the group's
// weapons, else the current weapon's; 0 without a group.
float group_max_range(const WeaponManager& m);
// Mech3.exe 1.2 @ 0x004b1c70: whether a weapon of the group (group fire) or
// the current one locks on (def LOCK_ON, flags bit 15).
bool group_locks_on(const WeaponManager& m);
// Mech3.exe 1.2 @ 0x004b1cd0: whether a weapon of the group (group fire) or
// the current one has ammo (vt+0x14 above 0, or -1 for unlimited).
bool group_has_ammo(const WeaponManager& m);

}  // namespace mech3
