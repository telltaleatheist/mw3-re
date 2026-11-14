// Probe for tests/differential/test_pm_group_fire.py: Pirate's Moon's group trigger 0x004b5f80 (the port's
// WeaponGroup::trigger) on a group of scripted weapons: each says whether it is a beam and whether its trigger fires,
// and records, when triggered, the FIRE animation choice its kind holds and the beams' ground effect switch.
#include <cstdint>

#include "gamez/zweapon/zweapon.h"
#include "mech3/weapon.h"

namespace {
int32_t* g_out = nullptr;
int g_n = 0;

struct Scripted : mech3::Weapon {
  Scripted(mech3::WeaponDef* d, mech3::WeaponManager* m, int id_, bool beam_, bool fires_)
      : mech3::Weapon(d, nullptr, m, true), id(id_), beam(beam_), fires(fires_) {}
  int id;
  bool beam, fires;
  bool is_beam() const override { return beam; }
  bool trigger() override {
    g_out[g_n++] = id, g_out[g_n++] = def->fire.animation_choice, g_out[g_n++] = zwep::fire_state().beam_ground_fx;
    return fires;
  }
};
}  // namespace

extern "C" {
// n weapons (at most 16): beam, rating, kind (an index into 4 definitions), fires; mode the manager's. out: a triple
// (weapon, choice, ground effect) per trigger, then the result, the ground effect after and each kind's choice after.
// Returns the words written.
int probe_pm_group_fire(int n, const int* beam, const float* rating, const int* kind, const int* fires, int mode,
                        int32_t* out) {
  static mech3::WeaponManager* manager = new mech3::WeaponManager(nullptr);
  static zwep::WeaponDef defs[4];
  for (zwep::WeaponDef& d : defs) d.fire.animation_choice = 5;
  zwep::fire_state().beam_ground_fx = 1;
  manager->mode = mode;
  mech3::WeaponGroup g;
  g.manager = manager;
  g.count = static_cast<uint32_t>(n);
  Scripted* ws[16] = {};
  for (int i = 0; i < n; ++i) {
    ws[i] = new Scripted(&defs[kind[i]], manager, i, beam[i] != 0, fires[i] != 0);
    ws[i]->rating = rating[i];
    g.weapons[i] = ws[i];
  }
  g.current_weapon = n ? ws[0] : nullptr;
  g_out = out, g_n = 0;
  const bool r = g.trigger();
  out[g_n++] = r, out[g_n++] = zwep::fire_state().beam_ground_fx;
  for (zwep::WeaponDef& d : defs) out[g_n++] = d.fire.animation_choice;
  for (int i = 0; i < n; ++i) delete ws[i];
  return g_n;
}
}  // extern "C"
