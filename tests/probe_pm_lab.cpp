// The lab's PM pieces, for test_pm_lab_pick.py.
#include <cstdint>
#include <vector>

#include "crt/msvc_rand.h"
#include "mech3/app_hooks.h"
#include "mech3/lab_screen.h"
#include "mech3/mech_config.h"

extern "C" int32_t probe_pm_lab_pick(const int32_t* parts, const float* radii, int32_t n, int32_t weapon, int32_t part,
                                     uint32_t seed) {
  std::vector<mech3::Engine::LabFirepoint> fps;
  for (int32_t i = 0; i < n; ++i) fps.push_back({parts[i], radii[i]});
  crt::msvc_srand(seed);
  return mech3::lab_pick_firepoint(fps, weapon, part);
}

namespace {
struct PanelEngine : mech3::Engine {
  mech3::Config* c = nullptr;
  float lab_config_weight_of(mech3::Handle, int32_t what) override { return static_cast<float>(c->weight(what)); }
  int32_t lab_config_engine(mech3::Handle) override { return c->engine; }
};
}  // namespace

// The two data panels' numbers for a loadout: run, walk, the total, the most; then the
// chassis panel's rows 0..7, the total and the most (test_pm_lab_data.py).
extern "C" void probe_pm_lab_numbers(void* config, int32_t engine, int32_t xl, float tons, int32_t endo, float* out) {
  auto& c = *static_cast<mech3::Config*>(config);
  const int32_t keep_engine = c.engine;
  const uint8_t keep_xl = c.xl, keep_endo = c.endo_steel;
  const float keep_tons = c.armor_tons;
  c.engine = engine, c.xl = static_cast<uint8_t>(xl), c.armor_tons = tons, c.endo_steel = static_cast<uint8_t>(endo);
  PanelEngine e;
  e.c = &c;
  const mech3::LabSpeeds s = mech3::lab_mech_speeds(e, 1);
  out[0] = s.run, out[1] = s.walk, out[2] = static_cast<float>(c.weight(-1)), out[3] = s.most;
  for (int32_t r = 0; r < 8; ++r) out[4 + r] = mech3::lab_chassis_tons(e, 1, r);
  out[12] = static_cast<float>(c.weight(-1)), out[13] = s.most;
  c.engine = keep_engine, c.xl = keep_xl, c.armor_tons = keep_tons, c.endo_steel = keep_endo;
}
