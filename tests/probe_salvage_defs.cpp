// PM's salvage moves (tests/differential/test_salvage.py --moves-only on PM's exe): every weapon a definition of 100
// rounds a ton, so the lab rounds (PM's 0x004b8a60, the port's lab_rounds) are min(100, count) on both sides and a
// count of 0 comes to under one lab load. Apart from probe_salvage.cpp, whose Handle weapon.h's clashes with.
#include <vector>

#include "gamez/zweapon/zwep_init.h"
#include "mech3/mech_names.h"
#include "mech3/weapon.h"

namespace {
mech3::WeaponType g_type;
}

void* probe_salvage_pm_defs(bool on) {
  auto* kept = new std::vector<zwep::WeaponDef>;
  kept->swap(zwep::state().defs);
  if (on) {
    g_type.rounds_per_ton = 100;
    for (int32_t i = 0; i < mech3::weapon_count(); ++i) {
      zwep::WeaponDef d{};
      d.key = mech3::weapon_name(i);
      d.type = &g_type;
      zwep::state().defs.push_back(d);
    }
  }
  return kept;
}

void probe_salvage_defs_back(void* kept) {
  auto* k = static_cast<std::vector<zwep::WeaponDef>*>(kept);
  zwep::state().defs.swap(*k);
  delete k;
}
