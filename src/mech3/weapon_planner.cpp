#include "mech3/weapon_planner.h"

#include <vector>

#include "crt/msvc_math.h"

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

namespace {
// 0x004b1370's record (0x14 bytes): a weapon, its group, the damage a burst
// deals and the heat it makes; the volley's five group records are the same
// with no weapon (0x004b1350: zeros, index -1, then 0..4).
struct Shot {
  Weapon* weapon = nullptr;     // +0x00
  WeaponGroup* group = nullptr; // +0x04
  int32_t index = -1;           // +0x08  the group's
  float damage = 0;             // +0x0c
  float heat = 0;               // +0x10
};

// The volley's context (0x90 bytes on its stack).
struct Volley {
  float hold;          // +0x04
  float budget;        // +0x08  the heat it may make
  float sum_heat = 0;  // +0x0c
  float sum_damage = 0;  // +0x10
  float dist2;         // +0x14
  Shot groups[5];      // +0x1c
  std::vector<Shot> shots;  // +0x80
};

bool in_range(const Weapon& w, float dist2) {
  return w.type_ready() && w.can_fire() && !(w.min_range_sq() > dist2) && w.max_range_sq() >= dist2;
}

// Mech3.exe 1.2 @ 0x004b1370: a burst of `hold` seconds' heat (type +0x08;
// times the hold for a timed-damage beam) and its damage (+0x18 a second
// times the hold) when the weapon is ready and the target within its range.
Shot shot_of(Weapon* w, float dist2, float hold) {
  Shot r;
  r.weapon = w;
  r.group = w->group_ptr();  // 0x004b3a90
  r.heat = w->def->type->heat;
  if ((w->def->flags & zwep::kBeam) && (w->def->flags & zwep::kTimedDamage)) r.heat = hold * r.heat;
  if (in_range(*w, dist2)) r.damage = w->rating * hold;
  if (r.group) r.index = r.group->index;
  return r;
}

// Mech3.exe 1.2 @ 0x004b1470: a weapon that deals damage, flies, is ready,
// in range and within the heat budget is a candidate; its group's sums grow
// either way.
void add_weapon(Volley& v, Weapon* w) {
  if (!w) return;
  const Shot r = shot_of(w, v.dist2, v.hold);
  const uint32_t f = w->def->flags;
  const float speed = (f & zwep::kInstant) || (f & zwep::kBeam) ? 3.4028235e+38f : w->def->velocity;
  if (r.damage > 0 && speed > 0 && in_range(*w, v.dist2) && !(r.heat > v.budget)) v.shots.push_back(r);
  if (r.index != -1) {
    Shot& g = v.groups[r.index];
    g.index = r.index;
    g.heat = r.heat + g.heat;
    g.damage = r.damage + g.damage;
  }
  // The exe doubles its sums instead of adding the weapon's: they stay 0,
  // so the alpha strike below is never picked (docs/fixes.md, a candidate).
  v.sum_heat = v.sum_heat + v.sum_heat;
  v.sum_damage = v.sum_damage + v.sum_damage;
}

// 0x004b1440: a group over the heat budget or dealing nothing.
bool group_out(const Volley& v, const Shot& g) { return g.heat > v.budget || !(g.damage > 0); }

int32_t pick_index(int32_t n, int32_t (*rand)()) {
  if (n < 2) return 0;
  const int32_t x = rand() * n;
  return (x + ((x >> 31) & 0x7fff)) >> 15;  // x / 32768, toward 0
}
}  // namespace

// Mech3.exe 1.2 @ 0x004b1860. The roll: below +0x90 x 32767 an alpha strike
// (never: the sums above), below that plus (1 - +0x90) x 26213 x 32767 (as
// the exe has it: nearly always) a random group within the budget, else a
// random candidate weapon.
int32_t volley_pick(WeaponManager& m, const VolleyPilot* pilot, const float* dist2, const float* owner_heat,
                    int32_t (*rand)()) {
  // FIX (bug): the exe reads a vehicle without controls or a brain through null.
  if (!pilot) return 0;
  float h = 0;
  if (owner_heat) h = *owner_heat > 1.5f ? 1.0f : *owner_heat * 0.6666667f;  // 0x005c9054, 0x005c9058
  Volley v;
  v.hold = pilot->hold;
  v.budget = 0.9f - h;  // 0x005e8688
  v.dist2 = dist2 ? *dist2 : 3.4028235e+38f;
  for (int32_t i = 0; i < 5; ++i) v.groups[i].index = i;
  v.shots.reserve(16);
  for (uint32_t i = 0; i < m.count; ++i) add_weapon(v, m.weapons[i]);
  const int32_t r = rand();
  const int32_t alpha = crt::msvc_ftol(pilot->chance * 32767.0f);  // 0x005c905c
  float k = 1.0f - pilot->chance;
  k = k * 26213.0f;   // [0x005e868c], an int
  k = k * -32767.0f;  // 0x005c9060
  const int32_t group = crt::msvc_ftol(crt::x87_add_f24(static_cast<double>(alpha), -static_cast<double>(k)));
  if (r < alpha && v.sum_heat < v.budget && v.sum_damage > 0) {
    m.mode = 1;
    return 3;
  }
  if (r < group) {
    // 0x004b17d0: the groups left in order.
    std::vector<Shot> kept;
    for (const Shot& g : v.groups)
      if (!group_out(v, g)) kept.push_back(g);
    const int32_t n = static_cast<int32_t>(kept.size());
    if (n > 0) {
      const int32_t i = pick_index(n, rand);
      m.mode = 2;
      return m.select_group(kept[static_cast<size_t>(i)].index) ? 2 : 0;
    }
  }
  const int32_t n = static_cast<int32_t>(v.shots.size());
  if (n <= 0) return 0;
  const int32_t i = pick_index(n, rand);
  m.mode = 1;
  const Shot& s = v.shots[static_cast<size_t>(i)];
  // FIX (bug): a weapon in no group is read through null in the exe.
  const bool ok = s.weapon && s.group && m.select_group(s.group->index) && s.group->select(s.weapon);
  return ok ? 1 : 0;
}

}  // namespace mech3
