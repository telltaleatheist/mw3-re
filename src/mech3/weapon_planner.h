#pragma once
#include "mech3/weapon.h"

// The AI's weapon planner (docs/ai.md section 17.7): weapon.cpp's tail,
// 0x004b1170..0x004b1d7f: the AI's volley pick, and the three the HUD reads
// (the selected group's longest range, lock-on and ammo). The static
// 0x004b1170 builds at 0x0060b240 (two reserved vectors) is read by nothing
// and is not kept.
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

// What the volley pick reads of the pilot (the vehicle's vt+0x2c, vt+0x40,
// its controls vt+0x140 and their brain vt+0x54): the brain's +0x34, the
// seconds a burst holds the trigger, and +0x90, the share of alpha strikes.
struct VolleyPilot {
  float hold = 0;    // +0x34
  float chance = 0;  // +0x90
};

// Mech3.exe 1.2 @ 0x004b1860 (the AI's firing, 0x004b5a20): what to fire at
// a target `dist2` (its contact's +0x0c; none: FLT_MAX) away with the owner
// mech's heat (+0x554; none: 0): 3 an alpha strike, 2 a group (its fire
// mode set to group fire), 1 one weapon (single fire), 0 nothing; the
// choice selected on the manager (docs/ai.md 17.7). `pilot` null: nothing.
int32_t volley_pick(WeaponManager& m, const VolleyPilot* pilot, const float* dist2, const float* owner_heat,
                    int32_t (*rand)());

}  // namespace mech3
