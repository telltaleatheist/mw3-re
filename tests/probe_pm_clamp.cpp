// Probe for tests/differential/test_pm_clamp.py: Pirate's Moon's limit clamp 0x004a71f0 (the port's TurretLimits::clamp).
#include "mech3/veh_controls.h"

extern "C" int probe_pm_clamp(float lo, float hi, const float* d, float* out) {
  const mech3::TurretLimits l(lo, hi);
  return l.clamp(d, out) ? 1 : 0;
}
