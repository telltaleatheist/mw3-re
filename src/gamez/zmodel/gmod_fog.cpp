#include "gamez/zmodel/gmod_fog.h"

#include "gamez/zrender/zrndr_fog.h"

namespace zmod {

namespace {
Fog g_fog;
}  // namespace

Fog& fog() { return g_fog; }

// Mech3.exe 1.2 @ 0x00526b80
void fog_set_on(int32_t on) { g_fog.on = on; }

// Mech3.exe 1.2 @ 0x00526ce0
void fog_set_linear(int32_t linear) { g_fog.linear = linear; }

// Mech3.exe 1.2 @ 0x00526cd0
void fog_set_density(float density) { g_fog.density = density; }

// Mech3.exe 1.2 @ 0x00526cf0
void fog_set_color(const float rgb[3]) {
  for (int i = 0; i < 3; ++i) g_fog.color[i] = rgb[i];
  zrndr::device_fog_color(rgb);
}

// Mech3.exe 1.2 @ 0x00526ba0. The inverse keeps its old value when the range is empty.
void fog_set_near(float range) {
  g_fog.near_range = range;
  g_fog.near_sq = range * range;
  if (g_fog.far_range - range != 0.0f) g_fog.inv_range = 1.0f / (g_fog.far_range - range);
}

// Mech3.exe 1.2 @ 0x00526c00
void fog_set_far(float range) {
  g_fog.far_range = range;
  g_fog.far_sq = range * range;
  if (range - g_fog.near_range != 0.0f) g_fog.inv_range = 1.0f / (range - g_fog.near_range);
}

// Mech3.exe 1.2 @ 0x00526c50
void fog_set_alt_high(float alt) {
  g_fog.alt_high = alt;
  if (alt - g_fog.alt_low != 0.0f) g_fog.inv_alt = 1.0f / (alt - g_fog.alt_low);
}

// Mech3.exe 1.2 @ 0x00526c90
void fog_set_alt_low(float alt) {
  g_fog.alt_low = alt;
  if (g_fog.alt_high - alt != 0.0f) g_fog.inv_alt = 1.0f / (g_fog.alt_high - alt);
}

// Mech3.exe 1.2 @ 0x00526de0
void fog_apply() { zrndr::set_fog_color(g_fog.color); }

}  // namespace zmod
