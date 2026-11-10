// New Weapon's definition for test_new_weapon.py: weapons_with_definition
// (upgrades/editor/new_weapon) on a weapons.zrd's bytes, the copy's numbers
// read and the given ones set.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "upgrades/editor/new_weapon.h"

// numbers: damage, heat, range, fire rate, velocity; ints: clan tons, slots,
// sphere tons, slots, rounds; fx: "slot=name" pairs split by '|' (the slot's
// number, EffectSlot; "(none)" left out), "" none. The bytes written to `out`
// (up to `cap`); their count, -1 when they do not fit or nothing was made.
extern "C" int probe_new_weapon(const uint8_t* zrd, int size, const char* copy, const char* name, const float* numbers,
                                const int32_t* ints, int tech, int ammo, const char* fx_text, uint8_t* out, int cap) {
  mech3::WeaponEffects fx;
  std::string t = fx_text ? fx_text : "";
  for (size_t at = 0; at < t.size();) {
    size_t end = t.find('|', at);
    if (end == std::string::npos) end = t.size();
    const std::string pair = t.substr(at, end - at);
    const size_t eq = pair.find('=');
    if (eq != std::string::npos) {
      const int slot = std::atoi(pair.substr(0, eq).c_str());
      if (slot >= 0 && slot < mech3::kEffectSlots) fx.value[slot] = pair.substr(eq + 1);
    }
    at = end + 1;
  }
  mech3::WeaponNumbers n;
  if (!mech3::weapon_numbers_of(zrd, size_t(size), copy, &n)) return -1;
  n.damage = numbers[0], n.heat = numbers[1], n.range = numbers[2], n.fire_rate = numbers[3], n.velocity = numbers[4];
  n.clan_tons = ints[0], n.clan_slots = ints[1], n.sphere_tons = ints[2], n.sphere_slots = ints[3], n.rounds = ints[4];
  const std::vector<uint8_t> b = mech3::weapons_with_definition(zrd, size_t(size), copy, name, n, tech, ammo != 0, &fx);
  if (b.empty() || static_cast<int>(b.size()) > cap) return -1;
  std::memcpy(out, b.data(), b.size());
  return static_cast<int>(b.size());
}
