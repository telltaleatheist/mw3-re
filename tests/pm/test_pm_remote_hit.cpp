// PM's network mech hit (0x00470f60): no damage, but a weapon with a force jolts the remote mech's torso along
// the shot, flat and normalized (0x00537c70), with the weapon's force; without a force nothing.
#include <cmath>
#include <cstdint>

#include "check.h"
#include "gamez/zweapon/zweapon.h"
#include "gamez/zweapon/zwep_init.h"
#include "mech3/mech.h"
#include "mech3/mech_parts.h"
#include "mech3/weapon.h"

namespace {
struct Jolted : mech3::Torso {
  int calls = 0;
  mech3::Vec3 dir{};
  float force = 0;
  void apply_impact(mech3::Vec3* v, float f) override { ++calls, dir = *v, force = f; }
};
}  // namespace

TEST(pm_remote_mech_hit_jolts_its_torso_without_damage) {
  static mech3::Mech m;
  static Jolted torso;
  m.remote = true;
  m.torso = &torso;
  torso.armor = 10.0f;
  mech3::WeaponType type;
  zwep::WeaponDef def{};
  def.type = &type;
  zwep::HitState& hs = zwep::hit_state();
  const zwep::HitState saved = hs;
  hs.shot_from = {10.0f, 7.0f, 4.0f}, hs.shot_to = {7.0f, -50.0f, 0.0f};
  mech3::Collision coll{};
  type.impact_force = 0.0f;
  EXPECT(m.take_damage(&def, &coll, 30.0f) == 0);
  EXPECT(torso.calls == 0);
  type.impact_force = 2.5f;
  EXPECT(m.take_damage(&def, &coll, 30.0f) == 0);
  EXPECT(torso.calls == 1);
  EXPECT(std::fabs(torso.dir.x - 0.6f) < 1e-6f && torso.dir.y == 0.0f && std::fabs(torso.dir.z - 0.8f) < 1e-6f);
  EXPECT(torso.force == 2.5f);  // the force as it is, not times the direction
  EXPECT(torso.armor == 10.0f);
  hs.shot_from = hs.shot_to;  // a shot straight down its line: a zero direction, still passed
  EXPECT(m.take_damage(&def, &coll, 30.0f) == 0);
  EXPECT(torso.calls == 2 && torso.dir.x == 0.0f && torso.dir.z == 0.0f);
  hs = saved;
  m.torso = nullptr;
}
