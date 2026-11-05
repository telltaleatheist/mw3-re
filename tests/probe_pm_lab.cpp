// The lab's PM pieces, for test_pm_lab_pick.py.
#include <cstdint>
#include <vector>

#include "crt/msvc_rand.h"
#include "mech3/app_hooks.h"
#include "mech3/lab_screen.h"

extern "C" int32_t probe_pm_lab_pick(const int32_t* parts, const float* radii, int32_t n, int32_t weapon, int32_t part,
                                     uint32_t seed) {
  std::vector<mech3::Engine::LabFirepoint> fps;
  for (int32_t i = 0; i < n; ++i) fps.push_back({parts[i], radii[i]});
  crt::msvc_srand(seed);
  return mech3::lab_pick_firepoint(fps, weapon, part);
}
