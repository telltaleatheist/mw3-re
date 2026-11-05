// The AMS's frame on PM, 0x0041a020 with its find 0x004b7c90 and fire 0x004b7d40 (test_pm_ams.py).
#include <cstdint>
#include <vector>

#include "gamez/zweapon/zwep_init.h"
#include "mech3/mech.h"
#include "mech3/weapon.h"

using namespace mech3;

extern "C" {

// f: range, next, period, x, y, z, height, clock. i: a player, the AMS fitted, on, rounds, team, the player's
// enemies, a stale target (a blip's index or -1). blips: n of x, z, health, hostile. out: result, next, rounds,
// the target's index or -1, then each blip's health.
int32_t probe_pm_ams(const float* f, const int32_t* i, int32_t n, const float* blips, float* out) {
  static Mech m, player;
  m = Mech{}, player = Mech{};
  std::vector<zwep::Projectile> shots(static_cast<size_t>(n));
  radar_clear();
  for (int32_t k = 0; k < n; ++k) {
    shots[k].position = {blips[4 * k], 0.0f, blips[4 * k + 1]};
    shots[k].health = blips[4 * k + 2];
    radar_add(&shots[k], blips[4 * k + 3] != 0.0f);
  }
  Ams ams(f[0], f[2], i[3]);
  ams.next = f[1], ams.on = i[2] != 0, ams.rounds = i[3];
  ams.target = i[6] >= 0 ? &shots[i[6]] : nullptr;
  float tr[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, f[3], f[4], f[5]};
  m.transform = tr, m.ams = &ams, m.height = f[6], m.team = static_cast<uint32_t>(i[4]);
  m.equipment[0] = i[1];
  player.enemies = static_cast<uint32_t>(i[5]);
  world().mission_player = i[0] ? &player : nullptr;
  weapon_world().clock = f[7];
  const int32_t r = update_ams(m);
  world().mission_player = nullptr;
  out[0] = ams.next, out[1] = static_cast<float>(ams.rounds), out[2] = -1.0f;
  for (int32_t k = 0; k < n; ++k) {
    if (ams.target == &shots[k]) out[2] = static_cast<float>(k);
    out[3 + k] = shots[k].health;
  }
  m.transform = nullptr, m.ams = nullptr;
  radar_clear();
  return r;
}

}  // extern "C"
