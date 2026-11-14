// Probe for tests/differential/test_pm_weapon_select.py: Pirate's Moon's weapon selection (next / previous group
// 0x004b43a0 / 0x004b4480, select 0x004b4560, a group's next / previous ready weapon 0x004b5e10 / 0x004b5ea0; the
// port's WeaponManager::next_group / prev_group / select_group, WeaponGroup::next / prev) on scripted weapons.
#include <cstdint>

#include "mech3/weapon.h"

namespace mech3 {
bool weapon_uses_ammo(int32_t weapon);
}

namespace {
struct Scripted : mech3::Weapon {
  Scripted(mech3::WeaponDef* d, mech3::WeaponManager* m) : mech3::Weapon(d, nullptr, m, true) {}  // the beam form builds nothing more (is_beam is scripted)
  int id = 0;
  bool beam = false;
  int32_t left = 0;
  bool is_beam() const override { return beam; }
  int32_t ammo() const override { return left; }
  void release() override {}
};
}  // namespace

extern "C" {
int probe_pm_weapon_uses_ammo(int i) { return mech3::weapon_uses_ammo(i) ? 1 : 0; }

// nw weapons (at most 16), 7 words each: beam, ammo, table index, bin (0 none, 1 limited, 2 infinite), enabled,
// reload in ms, damage in thousandths. groups: 5 x 19 words (count, current, current weapon's id or -1, 16 ids).
// group the manager's, online its flag. op: 0 next group, 1 previous, 2 select arg, 3 group arg's next, 4 its
// previous. out: the result, the manager's group, then each group's current and current weapon's id. Returns words.
int probe_pm_weapon_select(int nw, const int* w, const int* groups, int group, int online, int op, int arg, int32_t* out) {
  static mech3::WeaponManager* manager = new mech3::WeaponManager(nullptr);
  static mech3::WeaponDef def;
  static mech3::AmmoBin bins[16];
  mech3::WeaponGroup* own[5];
  mech3::WeaponGroup gs[5];
  Scripted* ws[16] = {};
  for (int i = 0; i < nw; ++i) {
    const int* v = w + 7 * i;
    ws[i] = new Scripted(&def, manager);
    ws[i]->bin = nullptr;
    ws[i]->id = i, ws[i]->beam = v[0] != 0, ws[i]->left = v[1], ws[i]->index = v[2];
    bins[i] = mech3::AmmoBin{};
    bins[i].infinite = v[3] == 2;
    ws[i]->bin = v[3] ? &bins[i] : nullptr;
    ws[i]->enabled = v[4] != 0;
    ws[i]->reload = static_cast<float>(v[5]) / 1000.0f;
    ws[i]->damage = static_cast<float>(v[6]) / 1000.0f;
  }
  for (int g = 0; g < 5; ++g) {
    const int* v = groups + 19 * g;
    gs[g].manager = manager;
    gs[g].count = static_cast<uint32_t>(v[0]);
    gs[g].current = v[1];
    gs[g].current_weapon = v[2] >= 0 ? ws[v[2]] : nullptr;
    for (int k = 0; k < v[0]; ++k) gs[g].weapons[k] = ws[v[3 + k]];
    own[g] = manager->groups[g];
    manager->groups[g] = &gs[g];
  }
  manager->group = group;
  manager->online = online != 0;
  int r = 0;
  switch (op) {
    case 0: r = manager->next_group(); break;
    case 1: r = manager->prev_group(); break;
    case 2: r = manager->select_group(arg); break;
    case 3: gs[arg].next(); break;
    case 4: gs[arg].prev(); break;
    default: break;
  }
  int n = 0;
  out[n++] = r, out[n++] = manager->group;
  for (int g = 0; g < 5; ++g) {
    out[n++] = gs[g].current;
    out[n++] = gs[g].current_weapon ? static_cast<const Scripted*>(gs[g].current_weapon)->id : -1;
    manager->groups[g] = own[g];
  }
  for (int i = 0; i < nw; ++i) ws[i]->bin = nullptr, delete ws[i];  // the bins are the probe's
  return n;
}
}  // extern "C"
