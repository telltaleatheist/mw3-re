// Pirate's Moon's mech destructor (0x00415940): its first step (0x005a35f0) makes every live shot locked on the mech
// forget the lock, so a missile homing on a mech that is deleted (a network player leaving) holds no pointer into it.
// MW3 left the lock (docs/pm-tests.md).
#include "check.h"
#include "gamez/zweapon/zweapon.h"
#include "gamez/zweapon/zwep_init.h"

TEST(pm_deleted_mech_leaves_no_lock_on_a_shot) {
  zwep::State& s = zwep::state();
  const size_t base = s.defs.size();
  s.defs.emplace_back();
  s.defs.emplace_back();
  zwep::WeaponDef& shots = s.defs[base];
  zwep::WeaponDef& beams = s.defs[base + 1];
  beams.flags |= zwep::kBeam;
  int mech = 0, other = 0;
  const zwep::Vec3f3 at{}, vel{};
  zwep::Projectile a, b, c;
  a.lock_entity = &mech, a.target_at = &at, a.target_velocity = &vel;
  b.lock_entity = &other, b.target_at = &at, b.target_velocity = &vel;
  c.lock_entity = &mech, c.target_at = &at;
  a.next = &b, shots.active = &a;
  beams.active = &c;  // a beam's list: not shots, passed over
  const int32_t n = zwep::forget_lock(&mech);
  EXPECT(n == 1);
  EXPECT(a.lock_entity == nullptr && a.target_at == nullptr && a.target_velocity == nullptr);
  EXPECT(b.lock_entity == &other && b.target_at == &at);
  EXPECT(c.lock_entity == &mech);
  shots.active = beams.active = nullptr;
  s.defs.resize(base);
}
