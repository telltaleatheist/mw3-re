// Probe for tests/differential/test_pm_object_lod.py: Pirate's Moon's object-detail setter 0x0040b120 (the port's
// mech3::set_object_lod), its two scale calls caught.
#include <cstdint>
#include <cstring>
#include <vector>

#include "mech3/options.h"

namespace {
std::vector<uint32_t> g_log;
uint32_t b(float x) { uint32_t v; std::memcpy(&v, &x, 4); return v; }
struct Fake : mech3::OptionsHooks {
  void camera_set_lod_scale(mech3::Handle cam, float s) override {
    g_log.insert(g_log.end(), {1u, cam, b(s)});
  }
  void mechs_set_lod_scale(float s) override { g_log.insert(g_log.end(), {2u, b(s)}); }
};
}  // namespace

extern "C" {
// in: the level, the hardware entry (0/1), a camera (0 none). out: the two slots after, then the calls.
int probe_pm_object_lod(int level, int hw, uint32_t cam, uint32_t* out) {
  static Fake fake;
  mech3::set_options_hooks(&fake);
  mech3::Options& o = mech3::options();
  o.hw = hw;
  o.object_lod[0] = o.object_lod[1] = 7;
  o.camera.camera = cam;
  g_log.clear();
  mech3::set_object_lod(level);
  int n = 0;
  out[n++] = static_cast<uint32_t>(o.object_lod[0]), out[n++] = static_cast<uint32_t>(o.object_lod[1]);
  for (uint32_t v : g_log) out[n++] = v;
  o.camera.camera = 0;
  mech3::set_options_hooks(nullptr);
  return n;
}
}  // extern "C"
