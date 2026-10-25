// Probes for test_weather.py (docs/weather.md): the rain 0x005a60a0, its
// update 0x005a6110 with the draw it runs (0x005a5360, 0x005a57d0). rand,
// the camera's place and angles and the three device calls go to the trace;
// the texture is the emulator's address.
#include <cstdint>
#include <cstring>
#include <memory>

#include "gamez/zimage/zimg_library.h"
#include "gamez/zmodel/gmod_clip2d.h"
#include "gamez/zrender/zrndr_fog.h"
#include "gamez/ztwod/twod_weather.h"
#include "probe_app.h"

namespace {
uint32_t fb(float f) {
  uint32_t i;
  std::memcpy(&i, &f, 4);
  return i;
}
float bf(uint32_t i) {
  float f;
  std::memcpy(&f, &i, 4);
  return f;
}
uint32_t u(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
std::unique_ptr<ztwod::Rain> g_rain;
zimg::Texture g_image;
float g_place[3], g_angles[3];
ztwod::Rect g_rects[3];  // the object's own, the manager's two
}  // namespace

extern "C" {

// A rain of n particles made without a device, then given the device and an
// image as the test gives the emulator's.
void probe_weather_new(int32_t n, uint32_t texture) {
  if (g_rain) g_rain->image = nullptr, g_rain->texture = nullptr;  // the probe's, not to free
  zrndr::device_fog().active = 0;
  ztwod::WeatherHooks& h = ztwod::weather_hooks();
  h.rand = [] { return static_cast<int32_t>(probe_app::call("rand", {})); };
  h.camera_place = [](zcls::Node*, float out[3]) { std::memcpy(out, g_place, 12); };
  h.camera_angles = [](zcls::Node*, float out[3]) { std::memcpy(out, g_angles, 12); };
  h.draw = [](int32_t n, const zvid::D3DTexture* t, int32_t prio, float op, int32_t ov) {
    const zmod::TLVertex* v = zmod::clip2d().tl;
    probe_app::call("draw", {n, u(t), prio, fb(op), ov, fb(v[0].sx), fb(v[0].sy), fb(v[0].sz), fb(v[0].tu),
                             fb(v[0].tv), fb(v[1].sx), fb(v[1].sy), fb(v[1].sz), fb(v[1].tu), fb(v[1].tv),
                             fb(v[2].sx), fb(v[2].sy), fb(v[2].sz), fb(v[2].tu), fb(v[2].tv), fb(v[3].sx),
                             fb(v[3].sy), fb(v[3].sz), fb(v[3].tu), fb(v[3].tv)});
  };
  h.reload = [](zvid::D3DTexture* t, const zimg::Texture*) { probe_app::call("reload", {u(t)}); };
  h.flush = [] { probe_app::call("flush", {}); };
  ztwod::weather_state() = ztwod::WeatherState{};
  ztwod::WeatherLayer& l = ztwod::weather_layer();
  l.rects[0] = l.rects[1] = nullptr;
  g_rain = std::make_unique<ztwod::Rain>(n);
  zrndr::device_fog().active = 1;
  ztwod::Rain& r = *g_rain;
  r.ramp_pixels.assign(0x80, 0);
  r.ramp_alpha.assign(0x80, 0);
  g_image = zimg::Texture{};
  g_image.pixels = r.ramp_pixels.data();
  g_image.alpha = r.ramp_alpha.data();
  r.image = &g_image;
  r.texture = reinterpret_cast<zvid::D3DTexture*>(static_cast<uintptr_t>(texture));
  r.camera = reinterpret_cast<zcls::Node*>(static_cast<uintptr_t>(0x1000));
}

// One frame's inputs: the colour, alphas, wind, gravity, down (bits), the
// camera's place and angles, the hidden flag, the object's rect (or none),
// the manager (or none) with its two rects (each or none).
void probe_weather_set(const uint32_t* w, const int32_t* rects, int32_t has_rect, int32_t managed,
                       int32_t r0, int32_t r1, int32_t hidden) {
  ztwod::Rain& r = *g_rain;
  r.color = static_cast<uint16_t>(w[0]);
  r.alpha_tail = bf(w[1]), r.alpha_head = bf(w[2]);
  r.wind_dir = bf(w[3]), r.wind_vel = bf(w[4]), r.gravity = bf(w[5]);
  for (int k = 0; k < 3; ++k) r.down[k] = bf(w[6 + k]), g_place[k] = bf(w[9 + k]), g_angles[k] = bf(w[12 + k]);
  r.flags = hidden ? r.flags | ztwod::kHidden : r.flags & ~ztwod::kHidden;
  for (int i = 0; i < 3; ++i)
    g_rects[i] = {rects[4 * i], rects[4 * i + 1], rects[4 * i + 2], rects[4 * i + 3]};
  r.rect = has_rect ? &g_rects[0] : nullptr;
  ztwod::WeatherLayer& l = ztwod::weather_layer();
  l.rects[0] = r0 ? &g_rects[1] : nullptr;
  l.rects[1] = r1 ? &g_rects[2] : nullptr;
  r.owner = managed ? &l : nullptr;
}

void probe_weather_update(uint32_t dt) { g_rain->update(bf(dt)); }

// The state after: cur, the time, the camera's last place, which rect the
// object holds (0 none, 1 its own, 2 / 3 the manager's), then per particle
// its streak (8 words) and both buffers' places, then the ramp's 16 texels
// and 16 alphas.
int32_t probe_weather_state(uint32_t* out) {
  const ztwod::Rain& r = *g_rain;
  const ztwod::WeatherState& st = ztwod::weather_state();
  uint32_t* o = out;
  *o++ = static_cast<uint32_t>(r.cur);
  *o++ = fb(st.time);
  for (float c : st.last_camera) *o++ = fb(c);
  *o++ = !r.rect ? 0 : r.rect == &g_rects[0] ? 1 : r.rect == &g_rects[1] ? 2 : 3;
  for (int32_t i = 0; i < r.count; ++i) {
    const ztwod::WeatherStreak& s = r.streaks[static_cast<size_t>(i)];
    *o++ = static_cast<uint32_t>(s.x), *o++ = static_cast<uint32_t>(s.y);
    *o++ = static_cast<uint32_t>(s.w), *o++ = static_cast<uint32_t>(s.h);
    *o++ = s.color, *o++ = fb(s.u_tail), *o++ = fb(s.u_head), *o++ = static_cast<uint32_t>(s.shown);
    for (int b = 0; b < 2; ++b)
      for (int k = 0; k < 3; ++k) *o++ = fb(r.pos[b][static_cast<size_t>(3 * i + k)]);
  }
  for (int i = 0; i < 16; ++i) *o++ = r.ramp_pixels[static_cast<size_t>(i)];
  for (int i = 0; i < 16; ++i) *o++ = r.ramp_alpha[static_cast<size_t>(i)];
  return static_cast<int32_t>(o - out);
}

}  // extern "C"
