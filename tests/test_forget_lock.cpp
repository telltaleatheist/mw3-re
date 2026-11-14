// A mech deleted while shots are locked on it (FIX (A), zwep::forget_lock; Pirate's Moon's 0x005a35f0): the shots
// forget the lock, so none keeps pointers into the freed mech; the beams' lists are passed over (docs/fixes.md).
#include "check.h"
#include "gamez/zweapon/zweapon.h"
#include "gamez/zweapon/zwep_init.h"

TEST(fix_deleted_mech_leaves_no_lock_on_a_shot) {
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
  beams.active = &c;
  EXPECT(zwep::forget_lock(&mech) == 1);
  EXPECT(a.lock_entity == nullptr && a.target_at == nullptr && a.target_velocity == nullptr);
  EXPECT(b.lock_entity == &other && b.target_at == &at);
  EXPECT(c.lock_entity == &mech);
  shots.active = beams.active = nullptr;
  s.defs.resize(base);
}
