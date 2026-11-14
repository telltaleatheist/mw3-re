// Probe for tests/differential/test_pm_clamp.py: Pirate's Moon's limit clamp 0x004a71f0 (the port's TurretLimits::clamp).
#include "mech3/veh_controls.h"

extern "C" int probe_pm_clamp(float lo, float hi, const float* d, float* out) {
  const mech3::TurretLimits l(lo, hi);
  return l.clamp(d, out) ? 1 : 0;
}

#include "mech3/dir_limits_pm.h"

// Pirate's Moon's direction limits (tests/differential/test_pm_dir_limits.py): made by `how` (0 set 0x004a6ee0,
// 1 set_radians 0x004a7390, 2 set_turn 0x004a7420 with a[0], a[1], 3 unset 0x004a6ec0) from a[0..3]; the record
// out, within (0x004a70c0) and the clamp (0x004a71f0, its answer returned).
extern "C" int probe_pm_dir(int how, const float* a, const float* d, float* rec, int* within, float* out) {
  float c[mech3::pm_dir::kSize];
  mech3::pm_dir::unset(c);
  if (how == 0) mech3::pm_dir::set(c, a[0], a[1], a[2], a[3]);
  else if (how == 1) mech3::pm_dir::set_radians(c, a[0], a[1], a[2], a[3]);
  else if (how == 2) mech3::pm_dir::set_turn(c, a[0], a[1]);
  for (int i = 0; i < mech3::pm_dir::kSize; ++i) rec[i] = c[i];
  *within = mech3::pm_dir::within(c, d) ? 1 : 0;
  return mech3::pm_dir::clamp(c, d, out) ? 1 : 0;
}
