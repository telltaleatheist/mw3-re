// Probe for tests/differential/test_pm_forced_fire.py: Pirate's Moon's projectile and beam starts 0x004b7600 and
// 0x004b8030 (the port's Weapon::aim_and_fire and BeamWeapon::aim_and_fire), forced or not, with the weapon's line
// 0x004b7800 stood in (WeaponCalls::pm_line) by a script of answers, as the test's stubs answer the exe's.
#include <cstdint>
#include <cstring>
#include <vector>

#include "gamez/zweapon/zwep_init.h"
#include "mech3/weapon.h"
#include "mech3/zai_wire_firing_pm.h"

namespace {
namespace m3 = mech3;
std::vector<uint32_t> g_log;
uint32_t g_ret = 0, g_fire = 0;
float g_cos = 0, g_kept = 0;
m3::Vec3f g_vel{};

uint32_t bits(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
float from(uint32_t b) { float f; std::memcpy(&f, &b, 4); return f; }

bool line(m3::Weapon&, const m3::Vec3f* at, m3::Vec3f* mz, m3::Vec3f* dir, float* cos, float* kept) {
  g_log.push_back(1);
  g_log.push_back(m3::ai::pm_line_quiet() ? 1u : 0u);
  g_log.insert(g_log.end(), {bits(at->x), bits(at->y), bits(at->z)});
  *mz = {1, 2, 3}, *dir = {4, 5, 6};
  *cos = g_cos, *kept = g_kept;
  return g_ret != 0;
}
const m3::Vec3f* velocity(m3::Handle) { return &g_vel; }
m3::Handle vnode(m3::Handle) { return reinterpret_cast<m3::Handle>(0x5150); }
m3::Handle beam_create(m3::WeaponDef*, m3::Handle owner, m3::Handle node, const m3::Vec3f* mz, const m3::Vec3f* dir) {
  g_log.insert(g_log.end(), {3u, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(owner)),
                             static_cast<uint32_t>(reinterpret_cast<uintptr_t>(node)), bits(mz->x), bits(dir->z)});
  return reinterpret_cast<m3::Handle>(0x77);
}

struct Shooter : m3::Weapon {
  using m3::Weapon::Weapon;
  int32_t ammo() const override { return -1; }
  bool fire(const m3::Vec3f* vel) override {
    g_log.push_back(2);
    g_log.push_back(vel == &g_vel ? 1u : 0u);
    return g_fire != 0;
  }
};
}  // namespace

extern "C" {
// in: 0 the line's answer, 1 its cosine, 2 its kept cosine (bits), 3 ready (the readiness test), 4 forced, 5 the aim
// floor (bits), 6..8 the aim point (bits), 9 fire's answer, 10 a beam already made. mode 0 the projectile, 1 the beam.
// out: the answer, then (projectile) +0x70 and the target, or (beam) the beam and whether the aim point is kept;
// then the calls.
int probe_pm_forced_fire(int mode, const uint32_t* in, uint32_t* out) {
  g_log.clear();
  g_ret = in[0], g_cos = from(in[1]), g_kept = from(in[2]), g_fire = in[9];
  m3::WeaponCalls& k = m3::weapon_calls();
  const m3::WeaponCalls kept_calls = k;
  k.pm_line = line, k.vehicle_velocity = velocity, k.vehicle_node = vnode, k.beam_create = beam_create;
  m3::WeaponType type{};
  type.fire_time = 1.0f;
  zwep::WeaponDef def{};
  def.type = &type;
  def.period = 1.0f;
  m3::WeaponManager mgr(nullptr);
  const m3::Vec3f at{from(in[6]), from(in[7]), from(in[8])};
  int n = 0;
  if (mode == 0) {
    Shooter w(&def, reinterpret_cast<m3::Handle>(0x2424), &mgr, true);
    w.reload = in[3] ? 0.0f : 1.0f;
    w.aim_floor = from(in[5]);
    w.x6c = 0x1234;
    const bool r = w.aim_and_fire(&at, in[4] != 0);
    out[n++] = r, out[n++] = static_cast<uint32_t>(w.x6c);
    out[n++] = bits(w.target.x), out[n++] = bits(w.target.y), out[n++] = bits(w.target.z);
  } else {
    def.flags = zwep::kBeam;
    m3::BeamWeapon w(&def, reinterpret_cast<m3::Handle>(0x2424), &mgr);
    w.reload = in[3] ? 0.0f : 1.0f;
    w.aim_floor = from(in[5]);
    if (in[10]) w.beam = reinterpret_cast<m3::Handle>(0x66);
    const bool r = w.aim_and_fire(&at, in[4] != 0);
    out[n++] = r, out[n++] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(w.beam)), out[n++] = w.aim_at == &at;
    w.beam = nullptr;
  }
  for (uint32_t v : g_log) out[n++] = v;
  k = kept_calls;
  return n;
}
}  // extern "C"
