#include "mech3/weapon_planner.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3 {

namespace {
constexpr int32_t kGroupFire = 2;  // manager +0x1c

bool has_ammo(const Weapon& w) {
  const int32_t n = w.ammo();
  return n > 0 || n == -1;
}
}  // namespace

// Mech3.exe 1.2 @ 0x004b1bd0
float group_max_range(const WeaponManager& m) {
  const WeaponGroup* g = m.group_ptr();
  if (!g) return 0;
  float best = 0;
  if (m.mode == kGroupFire) {
    for (uint32_t i = 0; i < g->count; ++i) {
      const float r = g->at(i)->max_range();
      if (!(best > r)) best = r;  // fcompp: a NaN range is taken
    }
  } else if (g->current_weapon) {
    best = g->current_weapon->max_range();
  }
  return best;
}

// Mech3.exe 1.2 @ 0x004b1c70
bool group_locks_on(const WeaponManager& m) {
  const WeaponGroup* g = m.group_ptr();
  if (!g) return false;
  if (m.mode == kGroupFire) {
    for (uint32_t i = 0; i < g->count; ++i)
      if (g->weapons[i]->def->flags & zwep::kLockOn) return true;
    return false;
  }
  return g->current_weapon && (g->current_weapon->def->flags & zwep::kLockOn);
}

// Mech3.exe 1.2 @ 0x004b1cd0
bool group_has_ammo(const WeaponManager& m) {
  const WeaponGroup* g = m.group_ptr();
  if (!g) return false;
  // FIX (bug): an empty group in group fire, or no current weapon otherwise,
  // answers no; the exe returns a byte of its saved ECX (docs/fixes.md).
  if (m.mode == kGroupFire) {
    for (uint32_t i = 0; i < g->count; ++i)
      if (has_ammo(*g->at(i))) return true;
    return false;
  }
  return g->current_weapon && has_ammo(*g->current_weapon);
}

}  // namespace mech3
