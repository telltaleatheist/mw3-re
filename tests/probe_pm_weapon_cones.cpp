// Probe for tests/differential/test_pm_weapon_cones.py: Pirate's Moon's per-weapon enable 0x004b6e80 (the
// player's firing cone, Weapon::set_enabled) and aim 0x004b7ac0 (Weapon::aim), with the weapon's line 0x004b7800
// stood in (WeaponCalls::pm_line) by a script of answers, as the test's stub answers the exe's.
#include <cstdint>
#include <cstring>
#include <vector>

#include "gamez/zweapon/zwep_init.h"
#include "mech3/mech_reader.h"
#include "mech3/weapon.h"

namespace {
namespace m3 = mech3;

// in (words): ret, cos, kept (floats as bits) of the stood-in line
std::vector<uint32_t> g_log;
uint32_t g_ret = 0;
float g_cos = 0, g_kept = 0;

bool line(m3::Weapon& w, const m3::Vec3f* at, m3::Vec3f* mz, m3::Vec3f* dir, float* cos, float* kept) {
  (void)w, (void)mz, (void)dir;
  g_log.push_back(1);
  g_log.push_back(at ? 1u : 0u);
  if (at) {
    uint32_t b[3];
    std::memcpy(b, &at->x, 12);
    g_log.insert(g_log.end(), b, b + 3);
  }
  *cos = g_cos, *kept = g_kept;
  return g_ret != 0;
}

bool in_water(m3::Handle part) { return reinterpret_cast<uintptr_t>(part) & 1; }

float from(uint32_t b) {
  float f;
  std::memcpy(&f, &b, 4);
  return f;
}
}  // namespace

extern "C" {
// in: 0 the line's answer, 1 its cosine, 2 its kept cosine (bits), 3 the player's own (1) or another's, 4 beam,
// 5 in water, 6 the firepoint's part, 7 side, 8 force, 9 enabled before, 10 the aim floor (bits), 11..13 the aim
// point (bits). mode 0: set_enabled, out: changed, enabled; mode 1: aim, out: its answer. Then the line's log.
int probe_pm_weapon_cone(int mode, const uint32_t* in, uint32_t* out) {
  g_log.clear();
  g_ret = in[0], g_cos = from(in[1]), g_kept = from(in[2]);
  m3::WeaponCalls& k = m3::weapon_calls();
  const m3::WeaponCalls kept_calls = k;
  k.pm_line = line;
  k.part_in_water = in_water;
  m3::WeaponWorld& world = m3::weapon_world();
  const m3::WeaponWorld kept_world = world;
  static char me, other;
  world.mission_player = &me;

  zwep::WeaponDef def{};
  def.flags = in[4] ? zwep::kBeam : 0;
  m3::WeaponManager mgr(nullptr);
  mgr.owner = in[3] ? static_cast<m3::Handle>(&me) : static_cast<m3::Handle>(&other);
  mgr.aim = {from(in[11]), from(in[12]), from(in[13])};
  m3::Weapon w(&def, nullptr, &mgr, true);  // true: the constructor stops before the rating (no type here)
  m3::Firepoint fp{};
  fp.part = static_cast<int32_t>(in[6]);
  w.firepoint = &fp;
  w.part = reinterpret_cast<m3::Handle>(static_cast<uintptr_t>(in[5] ? 0x1001 : 0x1000));
  w.enabled = in[9] != 0;
  w.aim_floor = from(in[10]);
  int n = 0;
  if (mode == 0) {
    const bool changed = w.set_enabled(static_cast<int32_t>(in[7]), in[8] != 0);
    out[n++] = changed, out[n++] = w.enabled;
  } else {
    m3::Vec3f at = mgr.aim, mz{}, dir{};
    out[n++] = w.aim(&at, &mz, &dir, nullptr);
  }
  for (uint32_t v : g_log) out[n++] = v;
  w.firepoint = nullptr, w.part = nullptr;
  k = kept_calls;
  world = kept_world;
  return n;
}
}  // extern "C"
