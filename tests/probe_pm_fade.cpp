// Probe for tests/differential/test_pm_fade.py: Pirate's Moon's mission fade step 0x00483470 (the port's
// ztwod::weather_fade_step on the weather layer), its tint's draw caught at Direct3D's quad.
#include <cstdint>
#include <cstring>
#include <vector>

#include "gamez/zrender/zrndr_fog.h"
#include "gamez/zrender/zrndr_tint.h"
#include "gamez/ztwod/twod_weather.h"

namespace {
std::vector<uint32_t> g_log;
uint32_t lo(double d) { uint64_t b; std::memcpy(&b, &d, 8); return static_cast<uint32_t>(b); }
uint32_t hi(double d) { uint64_t b; std::memcpy(&b, &d, 8); return static_cast<uint32_t>(b >> 32); }
double dbl(uint32_t l, uint32_t h) { uint64_t b = (uint64_t{h} << 32) | l; double d; std::memcpy(&d, &b, 8); return d; }
}  // namespace

extern "C" {
// in: done, present, rate (two words), value (two words), dt (bits). out: done, value, the tint's alpha, then per
// draw [1, alpha], then the shown path: [2] the scene (hardware) or [3] the software's.
int probe_pm_fade_step(const uint32_t* in, uint32_t* out) {
  ztwod::WeatherLayer& w = ztwod::weather_layer();
  w.fade_done = in[0] != 0, w.fade_present = in[1] != 0;
  w.fade_rate = dbl(in[2], in[3]), w.fade_value = dbl(in[4], in[5]);
  w.fade_tint.alpha = 0;
  float dt;
  std::memcpy(&dt, &in[6], 4);
  g_log.clear();
  zrndr::TintHooks& h = zrndr::tint_hooks();
  const zrndr::TintHooks kept = h;
  h.quad = [](uint16_t, const int32_t*, double a) { g_log.push_back(1), g_log.push_back(lo(a)), g_log.push_back(hi(a)); };
  auto& fog = zrndr::device_fog();
  const auto was = fog.active;
  fog.active = 1;
  if (ztwod::weather_fade_step(dt)) g_log.push_back(w.fade_present ? 2u : 3u);
  fog.active = was;
  h = kept;
  int n = 0;
  out[n++] = w.fade_done, out[n++] = lo(w.fade_value), out[n++] = hi(w.fade_value);
  out[n++] = lo(w.fade_tint.alpha), out[n++] = hi(w.fade_tint.alpha);
  for (uint32_t v : g_log) out[n++] = v;
  return n;
}
}  // extern "C"
