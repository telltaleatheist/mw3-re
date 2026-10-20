#include "gamez/zrender/zrndr_fog.h"

#include <cmath>

#include "crt/msvc_math.h"
#include "gamez/zvideo/zvid_ddd3d.h"

namespace zrndr {

namespace {
DeviceFog g_device;
SoftwareFormat g_format;
FogColor g_fog;
}  // namespace

DeviceFog& device_fog() { return g_device; }
SoftwareFormat& software_format() { return g_format; }
FogColor& fog_color() { return g_fog; }

void device_fog_color(const float rgb[3]) {
  DeviceFog& d = g_device;
  if (!d.active) return;
  for (int i = 0; i < 3; ++i) d.color[i] = rgb[i] * 255.0f;
  if (d.sent[0] != d.color[0] || d.sent[1] != d.color[1] || d.sent[2] != d.color[2]) {
    for (int i = 0; i < 3; ++i) d.sent[i] = d.color[i];
    if (d.send) d.send();
  }
}

// Mech3.exe 1.2 @ 0x00567eb0. A change under 0.01 in every channel is ignored.
void set_fog_color(float rgb[3]) {
  for (int i = 0; i < 3; ++i) {
    if (rgb[i] > 1.0f) rgb[i] = 1.0f;
    else if (rgb[i] < 0.0f) rgb[i] = 0.0f;
  }
  constexpr double kStep = 0.01f;  // 0x005ce668
  const auto same = [&](int i) { return std::fabs(double{g_fog.rgb[i]} - rgb[i]) < kStep; };
  if (same(0) && same(1) && same(2)) return;
  for (int i = 0; i < 3; ++i) g_fog.rgb[i] = rgb[i];
  device_fog_color(rgb);
  const SoftwareFormat& f = g_format;
  const auto level = [&](int i) { return static_cast<uint32_t>(crt::msvc_ftol(0.5 - rgb[i] * -255.0)); };
  const uint32_t b = level(2) >> f.b_shift_right;
  const uint32_t g = (level(1) << f.g_shift) & f.g_mask;
  const uint32_t r = (level(0) << f.r_shift) & f.r_mask;
  set_fog_ramp(g_fog, r, g, b);
}

FogColor& light_color() {
  static FogColor g_light;
  return g_light;
}

// Mech3.exe 1.2 @ 0x00568370. As set_fog_color, for a light's colour: the
// Direct3D device gets it as its point-light colour (0x005798a0).
void set_light_color(float rgb[3]) {
  for (int i = 0; i < 3; ++i) {
    if (rgb[i] > 1.0f) rgb[i] = 1.0f;
    else if (rgb[i] < 0.0f) rgb[i] = 0.0f;
  }
  FogColor& c = light_color();
  constexpr double kStep = 0.01f;  // 0x005ce688
  const auto same = [&](int i) { return std::fabs(double{c.rgb[i]} - rgb[i]) < kStep; };
  if (same(0) && same(1) && same(2)) return;
  for (int i = 0; i < 3; ++i) c.rgb[i] = rgb[i];
  if (g_device.active) zvid::set_point_light_color(rgb);
  const SoftwareFormat& f = g_format;
  const auto level = [&](int i) { return static_cast<uint32_t>(crt::msvc_ftol(0.5 - rgb[i] * -255.0)); };
  const uint32_t b = level(2) >> f.b_shift_right;
  const uint32_t g = (level(1) << f.g_shift) & f.g_mask;
  const uint32_t r = (level(0) << f.r_shift) & f.r_mask;
  set_fog_ramp(c, r, g, b);
}

// Mech3.exe 1.2 @ 0x0056c170
void set_fog_ramp(FogColor& f, uint32_t r, uint32_t g, uint32_t b) {
  f.b = b, f.g = g, f.r = r;
  const uint32_t all = b | g | r;
  f.packed = static_cast<uint16_t>(all);
  f.doubled = all << 16 | all;
  const uint32_t step = (b | r) << 11 | static_cast<uint32_t>(static_cast<int32_t>(g) >> 5);
  uint32_t v = 0;
  for (int i = 31; i >= 0; --i, v += step) f.ramp[i] = static_cast<int32_t>(v);
}

}  // namespace zrndr
